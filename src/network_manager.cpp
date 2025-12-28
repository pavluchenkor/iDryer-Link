#include "network_manager.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_system.h>

using DryerUart::AckPayload;
using DryerUart::CommandCode;
using DryerUart::CommandPayload;
using DryerUart::ConfigPayload;
using DryerUart::DryerState;
using DryerUart::DryerMode;
using DryerUart::ErrorCode;
using DryerUart::ErrorPayload;
using DryerUart::FrameHeader;
using DryerUart::HelloPayload;
using DryerUart::MessageKind;
using DryerUart::TelemetryPayload;
using DryerUart::StatusPayload;
using DryerUart::WeightsPayload;
using DryerUart::RfidPayload;

namespace
{

#ifdef DEBUG_SERIAL
#define NET_LOG(...) DEBUG_SERIAL.printf(__VA_ARGS__)
#else
#define NET_LOG(...)
#endif

  constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 5000;
  constexpr uint32_t PROVISION_RETRY_MS = 10000;
  constexpr uint32_t REGISTRATION_RETRY_MS = 60000;
  constexpr uint32_t CLAIM_POLL_INTERVAL_MS = 5000;
  constexpr uint32_t MQTT_RETRY_INTERVAL_MS = 5000;
  constexpr uint32_t TELEMETRY_PUBLISH_INTERVAL_MS = 5000; // Каждые 5 сек

  // API endpoints
  const char *API_BASE = "https://portal.idryer.org/api";

  /**
   * @brief Генерация серийного номера на основе MAC адреса
   */
  String generateSerialNumber()
  {
    uint8_t mac[6];
    WiFi.macAddress(mac);

    char serialNumber[32];
    snprintf(serialNumber, sizeof(serialNumber), "DEVICE_%02x%02x%02x_%07d",
             mac[3], mac[4], mac[5],
             esp_random() % 10000000);

    return String(serialNumber);
  }

} // namespace

void NetworkManager::begin(DryerUart::UartBridge *bridge)
{
  uart_ = bridge;
  credentialStore_.begin();

  // Генерируем или загружаем serialNumber
  if (identity_.serialNumber.isEmpty())
  {
    identity_.serialNumber = generateSerialNumber();
  }

  // Загружаем сохраненные credentials (token, deviceId)
  credentialStore_.load(identity_);

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  state_ = CloudState::WifiConnecting;
  lastWifiAttempt_ = millis() - WIFI_RETRY_INTERVAL_MS;

  // Настраиваем MQTT callback для команд
  mqttClient_.setCommandCallback([this](const char *cmd, JsonObjectConst data)
                                 { handleMqttCommand(cmd, data); });

  NET_LOG("[NET] Init: serial=%s deviceId=%s\n",
          identity_.serialNumber.c_str(),
          identity_.deviceId.c_str());
}

void NetworkManager::loop()
{
  ensureWifi();

  if (WiFi.status() == WL_CONNECTED)
  {
    ensureProvisioning();
    // ensureRegistration() - убрано! Теперь только по запросу через requestClaimProcess()
    pollClaimStatus();  // Работает только если awaitingClaim_ == true
    ensureMqtt();

    // Публикация топиков
    mqttClient_.loop();
    publishTelemetry();
  }
}

// ============================================================================
// Состояние подключения
// ============================================================================

void NetworkManager::ensureWifi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    if (state_ == CloudState::WifiConnecting)
    {
      state_ = CloudState::Provisioning;
      NET_LOG("[NET] Wi-Fi connected, IP: %s\n", WiFi.localIP().toString().c_str());
    }
    return;
  }

  const uint32_t now = millis();
  if (now - lastWifiAttempt_ < WIFI_RETRY_INTERVAL_MS)
  {
    return;
  }
  lastWifiAttempt_ = now;

  if (!wifiScanLogged_)
  {
    NET_LOG("[NET] Scanning Wi-Fi networks...\n");
    const int networkCount = WiFi.scanNetworks(/*async=*/false, true);
    if (networkCount > 0)
    {
      for (int i = 0; i < networkCount; ++i)
      {
        NET_LOG("[NET] AP %d: %s (RSSI=%d dBm)\n", i + 1,
                WiFi.SSID(i).c_str(), WiFi.RSSI(i));
      }
    }
    WiFi.scanDelete();
    wifiScanLogged_ = true;
  }

  NET_LOG("[NET] Connecting Wi-Fi SSID=%s\n", IDRYER_WIFI_SSID);
  WiFi.begin(IDRYER_WIFI_SSID, IDRYER_WIFI_PASSWORD);
  state_ = CloudState::WifiConnecting;
}

void NetworkManager::ensureProvisioning()
{
  // Если токен уже есть - пропускаем
  if (!identity_.token.isEmpty())
  {
    if (state_ == CloudState::Provisioning)
    {
      state_ = CloudState::Registering;
    }
    return;
  }

  const uint32_t now = millis();
  if (now - lastProvisionAttempt_ < PROVISION_RETRY_MS)
  {
    return;
  }
  lastProvisionAttempt_ = now;

  NET_LOG("[NET] Provisioning device...\n");
  if (provisionDevice())
  {
    credentialStore_.save(identity_);
    state_ = CloudState::Registering;
  }
}

bool NetworkManager::requestClaimProcess()
{
  using DryerUart::ClaimStatusPayload;
  using DryerUart::ClaimingStatus;

  // Проверяем что устройство еще не привязано
  if (!identity_.deviceId.isEmpty())
  {
    NET_LOG("[NET] Device already claimed, deviceId=%s\n", identity_.deviceId.c_str());
    return false;
  }

  // Проверяем что WiFi подключен
  if (WiFi.status() != WL_CONNECTED)
  {
    NET_LOG("[NET] WiFi not connected\n");
    ClaimStatusPayload payload{};
    payload.status = ClaimingStatus::Error;
    payload.pin[0] = '\0';
    payload.expiresAt = 0;
    payload.remainingSeconds = 0;
    uart_->sendClaimStatus(payload);
    return false;
  }

  // Проверяем что получили token (если нет - делаем provision)
  if (identity_.token.isEmpty())
  {
    NET_LOG("[NET] No token, running provision first...\n");
    ClaimStatusPayload payload{};
    payload.status = ClaimingStatus::Provisioning;
    payload.pin[0] = '\0';
    payload.expiresAt = 0;
    payload.remainingSeconds = 0;
    uart_->sendClaimStatus(payload);

    if (!provisionDevice())
    {
      NET_LOG("[NET] Provision failed\n");
      payload.status = ClaimingStatus::Error;
      uart_->sendClaimStatus(payload);
      return false;
    }
    credentialStore_.save(identity_);
  }

  // Проверяем что уже не в процессе claiming
  if (awaitingClaim_)
  {
    NET_LOG("[NET] Claim process already in progress, PIN=%s\n", pendingPin_.c_str());
    return true;  // Уже запущен
  }

  NET_LOG("[NET] Starting claim process by user request...\n");
  if (registerDevice())
  {
    awaitingClaim_ = true;
    scheduleNextClaimPoll();
    state_ = CloudState::AwaitingClaim;

    // Отправляем PIN на RP2040
    ClaimStatusPayload payload{};
    payload.status = ClaimingStatus::WaitingClaim;
    strncpy(payload.pin, pendingPin_.c_str(), sizeof(payload.pin) - 1);
    payload.pin[8] = '\0';
    payload.expiresAt = 0; // TODO: получить из API
    payload.remainingSeconds = 600; // 10 минут
    uart_->sendClaimStatus(payload);

    NET_LOG("[CLAIM] PIN sent to RP2040: %s\n", pendingPin_.c_str());
    return true;
  }

  // Ошибка регистрации
  ClaimStatusPayload payload{};
  payload.status = ClaimingStatus::Error;
  payload.pin[0] = '\0';
  payload.expiresAt = 0;
  payload.remainingSeconds = 0;
  uart_->sendClaimStatus(payload);
  return false;
}

void NetworkManager::ensureRegistration()
{
  // DEPRECATED: Этот метод больше не вызывается автоматически
  // Регистрация теперь только через requestClaimProcess()
  // Оставлен для обратной совместимости
}

void NetworkManager::pollClaimStatus()
{
  using DryerUart::ClaimCompletePayload;

  if (!awaitingClaim_ || !identity_.deviceId.isEmpty())
  {
    return;
  }

  const uint32_t now = millis();
  if (now - lastClaimPollAt_ < CLAIM_POLL_INTERVAL_MS)
  {
    return;
  }
  lastClaimPollAt_ = now;

  NET_LOG("[NET] Checking claim status...\n");
  if (checkClaim())
  {
    awaitingClaim_ = false;
    credentialStore_.save(identity_);
    state_ = CloudState::Ready;
    NET_LOG("[NET] Device claimed! deviceId=%s\n", identity_.deviceId.c_str());

    // Отправляем ClaimComplete на RP2040
    ClaimCompletePayload payload{};
    payload.success = 1;
    strncpy(payload.deviceId, identity_.deviceId.c_str(), sizeof(payload.deviceId) - 1);
    payload.deviceId[36] = '\0';
    uart_->sendClaimComplete(payload);
    NET_LOG("[CLAIM] ClaimComplete sent to RP2040\n");
  }
}

void NetworkManager::ensureMqtt()
{
  // Нужны token и deviceId для подключения к MQTT
  if (identity_.token.isEmpty() || identity_.deviceId.isEmpty())
  {
    return;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  if (mqttClient_.isConnected())
  {
    if (state_ != CloudState::Online)
    {
      state_ = CloudState::Online;
      NET_LOG("[NET] MQTT Online!\n");
      publishInfoOnce();
    }
    return;
  }

  const uint32_t now = millis();
  if (now - lastMqttConnectAttempt_ < MQTT_RETRY_INTERVAL_MS)
  {
    return;
  }
  lastMqttConnectAttempt_ = now;

  state_ = CloudState::MqttConnecting;
  NET_LOG("[NET] Connecting to MQTT broker...\n");

  // Инициализируем MQTT клиент (только один раз)
  static bool mqttInitialized = false;
  if (!mqttInitialized)
  {
    NET_LOG("[NET] MQTT init: serial='%s' token='%s'\n",
            identity_.serialNumber.c_str(),
            identity_.token.isEmpty() ? "(empty)" : "***");
    mqttClient_.begin(identity_.serialNumber.c_str(), identity_.token.c_str());
    mqttInitialized = true;
  }

  mqttClient_.connect();
}

// ============================================================================
// Регистрация устройства (HTTPS API)
// ============================================================================

bool NetworkManager::provisionDevice()
{
  // POST /devices/provision
  // Request:
  // {
  //   "serialNumber": "DEVICE_aabbcc_1234567"
  // }
  //
  // Response (новое устройство):
  // {
  //   "deviceToken": "eyJhbGciOiJIUzI1Ni...",
  //   "serialNumber": "DEVICE_aabbcc_1234567",
  //   "isNew": true,
  //   "isClaimed": false
  // }
  //
  // Response (существующее устройство):
  // {
  //   "deviceToken": "eyJhbGciOiJIUzI1Ni...",
  //   "serialNumber": "DEVICE_aabbcc_1234567",
  //   "isNew": false,
  //   "isClaimed": true,
  //   "deviceId": "550e8400-e29b-41d4-a716-446655440000"
  // }

  DynamicJsonDocument body(256);
  body["serialNumber"] = identity_.serialNumber;

  String payload;
  payload.reserve(256);
  serializeJson(body, payload);

  DynamicJsonDocument response(512);
  if (!httpPostJson(buildProvisionUrl(), payload, response))
  {
    NET_LOG("[NET] Provision failed\n");
    return false;
  }

  identity_.token = response["deviceToken"].as<String>();
  bool isNew = response["isNew"].as<bool>();
  bool isClaimed = response["isClaimed"].as<bool>();

  NET_LOG("[NET] Provision success: token=%s isNew=%d isClaimed=%d\n",
          identity_.token.c_str(), isNew, isClaimed);

  // Если устройство уже привязано - пропускаем регистрацию PIN
  if (isClaimed)
  {
    // Получаем deviceId из базы (должен быть в ответе для claimed устройств)
    if (response.containsKey("deviceId"))
    {
      identity_.deviceId = response["deviceId"].as<String>();
    }
  }

  return true;
}

bool NetworkManager::registerDevice()
{
  // POST /devices/register
  // Request:
  // {
  //   "token": "eyJhbGciOiJIUzI1Ni..."
  // }
  //
  // Response:
  // {
  //   "pin": "12345678",
  //   "expiresAt": "2024-12-12T12:10:00Z",
  //   "remainingSeconds": 600
  // }
  //
  // PIN отображается пользователю на устройстве для привязки через Web UI

  DynamicJsonDocument body(256);
  body["token"] = identity_.token;

  String payload;
  payload.reserve(256);
  serializeJson(body, payload);

  DynamicJsonDocument response(512);
  if (!httpPostJson(buildRegisterUrl(), payload, response))
  {
    NET_LOG("[NET] Register failed\n");
    return false;
  }

  pendingPin_ = response["pin"].as<String>();
  uint32_t remainingSeconds = response["remainingSeconds"].as<uint32_t>();

  NET_LOG("[NET] Registration PIN: %s (expires in %u sec)\n",
          pendingPin_.c_str(), remainingSeconds);

  return true;
}

bool NetworkManager::checkClaim()
{
  // GET /devices/check-claim/{token}
  //
  // Response (ещё не привязано):
  // {
  //   "claimed": false
  // }
  //
  // Response (успешно привязано):
  // {
  //   "claimed": true,
  //   "deviceId": "550e8400-e29b-41d4-a716-446655440000"
  // }
  //
  // Polling каждые 5 секунд до получения claimed=true

  DynamicJsonDocument response(512);
  if (!httpGetJson(buildCheckClaimUrl(identity_.token), response))
  {
    return false;
  }

  bool claimed = response["claimed"].as<bool>();
  if (!claimed)
  {
    return false;
  }

  identity_.deviceId = response["deviceId"].as<String>();
  NET_LOG("[NET] Claim confirmed! deviceId=%s\n", identity_.deviceId.c_str());
  return true;
}

void NetworkManager::scheduleNextClaimPoll()
{
  lastClaimPollAt_ = millis() - CLAIM_POLL_INTERVAL_MS;
}

// ============================================================================
// Публикация MQTT топиков
// ============================================================================

void NetworkManager::publishInfoOnce()
{
  if (infoPublished_)
  {
    return;
  }

  // Версии прошивки
  const char *hwVersion = "v1.0";
  const char *fwVersion = "1.0.0";
  uint32_t workTimeCounter = millis() / 1000; // Для теста - uptime

  mqttClient_.publishInfo(hwVersion, fwVersion, workTimeCounter);
  infoPublished_ = true;

  NET_LOG("[NET] Published info topic\n");
}

void NetworkManager::publishTelemetry()
{
  if (!mqttClient_.isConnected() || !telemetryDirty_)
  {
    return;
  }

  const uint32_t now = millis();
  if (now - lastTelemetryPublishAt_ < TELEMETRY_PUBLISH_INTERVAL_MS)
  {
    return;
  }
  lastTelemetryPublishAt_ = now;

  // Формируем JSON согласно API:
  // Topic: idryer/{serialNumber}/telemetry
  // QoS: 0, Retained: false, Frequency: каждые 5 сек
  DynamicJsonDocument doc(1024);
  JsonArray units = doc.createNestedArray("units");

  // Итерируем по массиву TelemetryEntry в payload
  for (uint8_t i = 0; i < latestTelemetry_.count && i < 4; ++i)
  {
    const auto &entry = latestTelemetry_.units[i];
    JsonObject unit = units.createNestedObject();

    // Конвертируем unitId: 0-3 → U1-U4
    char unitIdStr[3];
    snprintf(unitIdStr, sizeof(unitIdStr), "U%d", entry.unitId + 1);
    unit["unitId"] = unitIdStr;

    unit["temperature"] = entry.temperatureC10 / 10.0f;
    unit["humidity"] = entry.humidityPct;
    unit["heaterPower"] = entry.heaterPowerPct;
    unit["fanStatus"] = (entry.fanOn == 1);
  }

  // timestamp добавляется автоматически в mqttClient_.publishTelemetry()

  if (mqttClient_.publishTelemetry(doc))
  {
    telemetryDirty_ = false;
    NET_LOG("[NET] Published telemetry (%d units)\n", latestTelemetry_.count);
  }
}

void NetworkManager::publishStatus()
{
  if (!mqttClient_.isConnected() || !statusDirty_)
  {
    return;
  }

  DynamicJsonDocument doc(2048);  // Больший размер для нескольких юнитов
  JsonArray units = doc.createNestedArray("units");

  // Итерируем по массиву StatusEntry в payload
  for (uint8_t i = 0; i < latestStatus_.count && i < 4; ++i)
  {
    const auto &entry = latestStatus_.units[i];
    JsonObject unit = units.createNestedObject();

    // Конвертируем unitId: 0-3 → U1-U4
    char unitIdStr[3];
    snprintf(unitIdStr, sizeof(unitIdStr), "U%d", entry.unitId + 1);
    unit["unitId"] = unitIdStr;

    // Конвертируем DryerMode в строку
    const char *modeStr = "IDLE";
    switch (entry.mode)
    {
    case DryerMode::Idle:
      modeStr = "IDLE";
      break;
    case DryerMode::Drying:
      modeStr = "DRYING";
      break;
    case DryerMode::Storage:
      modeStr = "STORAGE";
      break;
    case DryerMode::Profile:
      modeStr = "PROFILE";
      break;
    case DryerMode::Fault:
      modeStr = "FAULT";
      break;
    default:
      modeStr = "UNKNOWN";
      break;
    }
    unit["mode"] = modeStr;

    // Добавляем данные только для активных режимов
    if (entry.mode != DryerMode::Idle && entry.mode != DryerMode::Fault)
    {
      unit["sessionNum"] = entry.sessionNum;

      // Target параметры
      JsonObject target = unit.createNestedObject("target");
      target["temperature"] = entry.targetTempC10 / 10.0f;
      target["duration"] = entry.durationMinutes;
      target["humidity"] = entry.targetHumidityPct;

      // Временные поля
      unit["totalElapsed"] = entry.elapsedSeconds;
      unit["totalRemaining"] = entry.totalRemainingSeconds;

      // Для PROFILE режима добавляем этапы
      if (entry.mode == DryerMode::Profile)
      {
        unit["currentStage"] = entry.currentStage;
        unit["totalStages"] = entry.totalStages;
        unit["stageElapsed"] = entry.stageElapsedSeconds;
        unit["stageRemaining"] = entry.stageRemainingSeconds;
      }
    }
  }

  // Добавляем uptime устройства
  doc["uptime"] = latestStatus_.uptime;

  // timestamp добавляется автоматически в mqttClient_.publishStatus()

  if (mqttClient_.publishStatus(doc))
  {
    statusDirty_ = false;
    NET_LOG("[NET] Published status (%d units)\n", latestStatus_.count);
  }
}

void NetworkManager::flushPendingTelemetry()
{
  if (telemetryDirty_)
  {
    publishTelemetry();
  }
}

// ============================================================================
// Обработка данных от RP2040
// ============================================================================

void NetworkManager::handleRpHello(const HelloPayload &payload,
                                   const FrameHeader &header)
{
  (void)payload;
  NET_LOG("[NET] RP2040 hello seq=%u\n", header.sequence);
}

void NetworkManager::handleTelemetry(const TelemetryPayload &payload,
                                     const FrameHeader &header)
{
  NET_LOG("\n[RECV] Telemetry (seq=%d, count=%d)\n", header.sequence, payload.count);

  // Вывод данных для отладки
  NET_LOG("Units: ");
  for (uint8_t i = 0; i < payload.count && i < 4; i++)
  {
    NET_LOG("U%d(T:%.1f°C H:%d%%) ",
            payload.units[i].unitId,
            payload.units[i].temperatureC10 / 10.0f,
            payload.units[i].humidityPct);
  }
  NET_LOG("\n");

  latestTelemetry_ = payload;
  telemetryDirty_ = true;
}

void NetworkManager::handleStatus(const StatusPayload &payload,
                                  const FrameHeader &header)
{
  NET_LOG("\n[RECV] Status (seq=%d, uptime=%ds, count=%d)\n",
          header.sequence, payload.uptime, payload.count);

  // Вывод данных для отладки
  for (uint8_t i = 0; i < payload.count && i < 4; i++)
  {
    NET_LOG("  U%d: mode=%d session=%d elapsed=%ds remaining=%ds\n",
            payload.units[i].unitId,
            payload.units[i].mode,
            payload.units[i].sessionNum,
            payload.units[i].elapsedSeconds,
            payload.units[i].totalRemainingSeconds);
  }

  latestStatus_ = payload;
  statusDirty_ = true;
  publishStatus();
}

void NetworkManager::handleWeights(const WeightsPayload &payload,
                                   const FrameHeader &header)
{
  NET_LOG("\n[RECV] Weights (seq=%d, count=%d)\n", header.sequence, payload.count);

  // Вывод данных для отладки
  NET_LOG("Sensors: ");
  for (uint8_t i = 0; i < payload.count && i < 4; i++)
  {
    NET_LOG("S%d→U%d(%dg) ",
            payload.weights[i].sensorId,
            payload.weights[i].unitId,
            payload.weights[i].weightGrams);
  }
  NET_LOG("\n");

  latestWeights_ = payload;
  weightsDirty_ = true;
}

void NetworkManager::handleRfidEvent(const RfidPayload &payload,
                                     const FrameHeader &header)
{
  (void)payload;
  (void)header;
  // TODO: обработка RFID событий от RP2040
  // Публиковать в MQTT RFID события
}

void NetworkManager::handleCommandAck(const AckPayload &payload,
                                      const FrameHeader &header)
{
  (void)payload;
  (void)header;
  // TODO: публиковать ACK в events топик
}

void NetworkManager::handleConfigAck(const AckPayload &payload,
                                     const FrameHeader &header)
{
  (void)payload;
  (void)header;
  // TODO: публиковать ACK в events топик
}

void NetworkManager::handleUartError(const ErrorPayload &payload,
                                     bool remote)
{
  (void)payload;
  (void)remote;
  // TODO: публиковать ошибку в events топик
}

// ============================================================================
// Обработка команд от MQTT
// ============================================================================

void NetworkManager::handleMqttCommand(const char *command, JsonObjectConst data)
{
  if (!uart_)
  {
    return;
  }

  NET_LOG("[NET] MQTT command: %s\n", command);

  // Обрабатываем команды из топика idryer/{serialNumber}/commands/{command}
  //
  // Пример JSON для start:
  // {
  //   "targetTemperature": 50,
  //   "durationMinutes": 240,
  //   "targetHumidity": 10
  // }
  //
  // Пример JSON для set_config:
  // {
  //   "targetTemperature": 50,
  //   "targetHumidity": 10,
  //   "durationMinutes": 240,
  //   "fanDutyPct": 85
  // }

  // Маппинг MQTT команд на UART протокол
  CommandCode cmdCode = CommandCode::Stop;

  if (strcmp(command, "start") == 0)
  {
    cmdCode = CommandCode::Start;
  }
  else if (strcmp(command, "stop") == 0)
  {
    cmdCode = CommandCode::Stop;
  }
  else if (strcmp(command, "get_config") == 0)
  {
    cmdCode = CommandCode::SetConfig;
  }
  else if (strcmp(command, "set_config") == 0)
  {
    // Обрабатываем конфигурацию - передаем через ConfigPayload на RP2040
    ConfigPayload config{};
    config.targetTemperatureC10 =
        static_cast<int16_t>(data["targetTemperature"].as<float>() * 10);
    config.targetHumidityPct = data["targetHumidity"].as<uint16_t>();
    config.durationMinutes = data["durationMinutes"].as<uint16_t>();
    config.fanDutyPct = data["fanDutyPct"].as<uint16_t>();

    uart_->sendConfigPush(config, true);
    return;
  }

  // Отправляем команду на RP2040 через UART
  CommandPayload cmdPayload{};
  cmdPayload.command = cmdCode;
  cmdPayload.targetState = static_cast<uint8_t>(DryerState::Drying);
  cmdPayload.arg0 = data["targetTemperature"].is<float>()
                        ? static_cast<int32_t>(data["targetTemperature"].as<float>() * 10)
                        : 550;  // по умолчанию 55°C
  cmdPayload.arg1 = data["durationMinutes"].is<int>()
                        ? static_cast<uint32_t>(data["durationMinutes"].as<int>())
                        : 240;  // по умолчанию 240 мин

  NET_LOG("[NET] Sending Command: code=%d, temp=%d.%d°C, duration=%dmin\n",
          static_cast<int>(cmdCode),
          cmdPayload.arg0 / 10,
          cmdPayload.arg0 % 10,
          cmdPayload.arg1);

  uart_->sendCommand(cmdPayload, true);
}

// ============================================================================
// HTTP запросы к Backend API
// ============================================================================

bool NetworkManager::httpPostJson(const String &url, const String &body,
                                  DynamicJsonDocument &response)
{
  WiFiClientSecure client;
  client.setInsecure(); // TODO: добавить сертификат

  HTTPClient https;
  https.setTimeout(10000);

  if (!https.begin(client, url))
  {
    return false;
  }

  https.addHeader("Content-Type", "application/json");
  int httpCode = https.POST(body);

  if (httpCode < 200 || httpCode >= 300)
  {
    NET_LOG("[NET] HTTP POST %s failed: %d\n", url.c_str(), httpCode);
    https.end();
    return false;
  }

  String payload = https.getString();
  https.end();

  DeserializationError err = deserializeJson(response, payload);
  if (err)
  {
    NET_LOG("[NET] JSON parse error: %s\n", err.c_str());
    return false;
  }

  return true;
}

bool NetworkManager::httpGetJson(const String &url,
                                 DynamicJsonDocument &response)
{
  WiFiClientSecure client;
  client.setInsecure(); // TODO: добавить сертификат

  HTTPClient https;
  https.setTimeout(10000);

  if (!https.begin(client, url))
  {
    return false;
  }

  int httpCode = https.GET();

  // По документации: для /check-claim 404 - валидный ответ с JSON {"claimed": false}
  // Поэтому принимаем 200-299 и 404
  if (httpCode < 0 || (httpCode >= 300 && httpCode != 404))
  {
    NET_LOG("[NET] HTTP GET %s failed: %d\n", url.c_str(), httpCode);
    https.end();
    return false;
  }

  String payload = https.getString();
  https.end();

  DeserializationError err = deserializeJson(response, payload);
  if (err)
  {
    NET_LOG("[NET] JSON parse error: %s\n", err.c_str());
    return false;
  }

  return true;
}

String NetworkManager::buildProvisionUrl() const
{
  return String(API_BASE) + "/devices/provision";
}

String NetworkManager::buildRegisterUrl() const
{
  return String(API_BASE) + "/devices/register";
}

String NetworkManager::buildCheckClaimUrl(const String &token) const
{
  return String(API_BASE) + "/devices/check-claim/" + token;
}
