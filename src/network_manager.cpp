#include "network_manager.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_system.h>

using DryerUart::AckPayload;
using DryerUart::CommandCode;
using DryerUart::CommandPayload;
using DryerUart::ConfigPayload;
using DryerUart::ErrorCode;
using DryerUart::ErrorPayload;
using DryerUart::FrameHeader;
using DryerUart::HelloPayload;
using DryerUart::MessageKind;
using DryerUart::TelemetryPayload;
using DryerUart::DryerState;

namespace {

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
constexpr uint32_t TELEMETRY_PUBLISH_INTERVAL_MS = 5000;  // Каждые 5 сек

// API endpoints
const char* API_BASE = "https://portal.idryer.org/api";

/**
 * @brief Генерация серийного номера на основе MAC адреса
 */
String generateSerialNumber() {
    uint8_t mac[6];
    WiFi.macAddress(mac);

    char serialNumber[32];
    snprintf(serialNumber, sizeof(serialNumber), "DEVICE_%02x%02x%02x_%07d",
             mac[3], mac[4], mac[5],
             esp_random() % 10000000);

    return String(serialNumber);
}

} // namespace

void NetworkManager::begin(DryerUart::UartBridge *bridge) {
  uart_ = bridge;
  credentialStore_.begin();

  // Генерируем или загружаем serialNumber
  if (identity_.serialNumber.isEmpty()) {
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
  mqttClient_.setCommandCallback([this](const char* cmd, JsonObjectConst data) {
      handleMqttCommand(cmd, data);
  });

  NET_LOG("[NET] Init: serial=%s deviceId=%s\n",
          identity_.serialNumber.c_str(),
          identity_.deviceId.c_str());
}

void NetworkManager::loop() {
  ensureWifi();

  if (WiFi.status() == WL_CONNECTED) {
    ensureProvisioning();
    ensureRegistration();
    pollClaimStatus();
    ensureMqtt();

    // Публикация топиков
    mqttClient_.loop();
    publishTelemetry();
  }
}

// ============================================================================
// Состояние подключения
// ============================================================================

void NetworkManager::ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (state_ == CloudState::WifiConnecting) {
      state_ = CloudState::Provisioning;
      NET_LOG("[NET] Wi-Fi connected, IP: %s\n", WiFi.localIP().toString().c_str());
    }
    return;
  }

  const uint32_t now = millis();
  if (now - lastWifiAttempt_ < WIFI_RETRY_INTERVAL_MS) {
    return;
  }
  lastWifiAttempt_ = now;

  if (!wifiScanLogged_) {
    NET_LOG("[NET] Scanning Wi-Fi networks...\n");
    const int networkCount = WiFi.scanNetworks(/*async=*/false, true);
    if (networkCount > 0) {
      for (int i = 0; i < networkCount; ++i) {
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

void NetworkManager::ensureProvisioning() {
  // Если токен уже есть - пропускаем
  if (!identity_.token.isEmpty()) {
    if (state_ == CloudState::Provisioning) {
      state_ = CloudState::Registering;
    }
    return;
  }

  const uint32_t now = millis();
  if (now - lastProvisionAttempt_ < PROVISION_RETRY_MS) {
    return;
  }
  lastProvisionAttempt_ = now;

  NET_LOG("[NET] Provisioning device...\n");
  if (provisionDevice()) {
    credentialStore_.save(identity_);
    state_ = CloudState::Registering;
  }
}

void NetworkManager::ensureRegistration() {
  // Если устройство уже привязано - пропускаем
  if (!identity_.deviceId.isEmpty()) {
    if (state_ == CloudState::Registering || state_ == CloudState::AwaitingClaim) {
      state_ = CloudState::Ready;
    }
    return;
  }

  // Если еще не получили token - пропускаем
  if (identity_.token.isEmpty()) {
    return;
  }

  const uint32_t now = millis();
  if (!awaitingClaim_) {
    if (now - lastRegistrationAttempt_ >= REGISTRATION_RETRY_MS) {
      NET_LOG("[NET] Registering device for claim...\n");
      if (registerDevice()) {
        awaitingClaim_ = true;
        scheduleNextClaimPoll();
      }
      lastRegistrationAttempt_ = now;
    }
  }
}

void NetworkManager::pollClaimStatus() {
  if (!awaitingClaim_ || !identity_.deviceId.isEmpty()) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastClaimPollAt_ < CLAIM_POLL_INTERVAL_MS) {
    return;
  }
  lastClaimPollAt_ = now;

  NET_LOG("[NET] Checking claim status...\n");
  if (checkClaim()) {
    awaitingClaim_ = false;
    credentialStore_.save(identity_);
    state_ = CloudState::Ready;
    NET_LOG("[NET] Device claimed! deviceId=%s\n", identity_.deviceId.c_str());
  }
}

void NetworkManager::ensureMqtt() {
  // Нужны token и deviceId для подключения к MQTT
  if (identity_.token.isEmpty() || identity_.deviceId.isEmpty()) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (mqttClient_.isConnected()) {
    if (state_ != CloudState::Online) {
      state_ = CloudState::Online;
      NET_LOG("[NET] MQTT Online!\n");
      publishInfoOnce();
    }
    return;
  }

  const uint32_t now = millis();
  if (now - lastMqttConnectAttempt_ < MQTT_RETRY_INTERVAL_MS) {
    return;
  }
  lastMqttConnectAttempt_ = now;

  state_ = CloudState::MqttConnecting;
  NET_LOG("[NET] Connecting to MQTT broker...\n");

  // Инициализируем MQTT клиент (только один раз)
  static bool mqttInitialized = false;
  if (!mqttInitialized) {
      mqttClient_.begin(identity_.serialNumber.c_str(), identity_.token.c_str());
      mqttInitialized = true;
  }

  mqttClient_.connect();
}

// ============================================================================
// Регистрация устройства (HTTPS API)
// ============================================================================

bool NetworkManager::provisionDevice() {
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
  if (!httpPostJson(buildProvisionUrl(), payload, response)) {
    NET_LOG("[NET] Provision failed\n");
    return false;
  }

  identity_.token = response["deviceToken"].as<String>();
  bool isNew = response["isNew"].as<bool>();
  bool isClaimed = response["isClaimed"].as<bool>();

  NET_LOG("[NET] Provision success: token=%s isNew=%d isClaimed=%d\n",
          identity_.token.c_str(), isNew, isClaimed);

  // Если устройство уже привязано - пропускаем регистрацию PIN
  if (isClaimed) {
    // Получаем deviceId из базы (должен быть в ответе для claimed устройств)
    if (response.containsKey("deviceId")) {
      identity_.deviceId = response["deviceId"].as<String>();
    }
  }

  return true;
}

bool NetworkManager::registerDevice() {
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
  if (!httpPostJson(buildRegisterUrl(), payload, response)) {
    NET_LOG("[NET] Register failed\n");
    return false;
  }

  pendingPin_ = response["pin"].as<String>();
  uint32_t remainingSeconds = response["remainingSeconds"].as<uint32_t>();

  NET_LOG("[NET] Registration PIN: %s (expires in %u sec)\n",
          pendingPin_.c_str(), remainingSeconds);

  return true;
}

bool NetworkManager::checkClaim() {
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
  if (!httpGetJson(buildCheckClaimUrl(identity_.token), response)) {
    return false;
  }

  bool claimed = response["claimed"].as<bool>();
  if (!claimed) {
    return false;
  }

  identity_.deviceId = response["deviceId"].as<String>();
  NET_LOG("[NET] Claim confirmed! deviceId=%s\n", identity_.deviceId.c_str());
  return true;
}

void NetworkManager::scheduleNextClaimPoll() {
  lastClaimPollAt_ = millis() - CLAIM_POLL_INTERVAL_MS;
}

// ============================================================================
// Публикация MQTT топиков
// ============================================================================

void NetworkManager::publishInfoOnce() {
  if (infoPublished_) {
    return;
  }

  // Версии прошивки
  const char* hwVersion = "v1.0";
  const char* fwVersion = "1.0.0";
  uint32_t workTimeCounter = millis() / 1000;  // Для теста - uptime

  mqttClient_.publishInfo(hwVersion, fwVersion, workTimeCounter);
  infoPublished_ = true;

  NET_LOG("[NET] Published info topic\n");
}

void NetworkManager::publishTelemetry() {
  if (!mqttClient_.isConnected() || !telemetryDirty_) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastTelemetryPublishAt_ < TELEMETRY_PUBLISH_INTERVAL_MS) {
    return;
  }
  lastTelemetryPublishAt_ = now;

  // Формируем JSON согласно API:
  // Topic: idryer/{serialNumber}/telemetry
  // QoS: 0, Retained: false, Frequency: каждые 5 сек
  //
  // {
  //   "units": [
  //     {
  //       "unitId": "U1",
  //       "temperature": 49.8,
  //       "humidity": 12.3,
  //       "heaterPower": 85,
  //       "fanStatus": true
  //     }
  //   ],
  //   "timestamp": "2025-12-22T10:00:00Z"
  // }
  DynamicJsonDocument doc(1024);
  JsonArray units = doc.createNestedArray("units");

  JsonObject unit = units.createNestedObject();
  unit["unitId"] = "U1";  // TODO: взять из payload
  unit["temperature"] = latestTelemetry_.temperatureC10 / 10.0f;
  unit["humidity"] = latestTelemetry_.humidityPct;
  unit["heaterPower"] = latestTelemetry_.heaterPowerPct;
  unit["fanStatus"] = (latestTelemetry_.fanOn == 1);

  // timestamp добавляется автоматически в mqttClient_.publishTelemetry()

  if (mqttClient_.publishTelemetry(doc)) {
    telemetryDirty_ = false;
    NET_LOG("[NET] Published telemetry\n");
  }
}

void NetworkManager::publishStatus() {
  if (!mqttClient_.isConnected()) {
    return;
  }

  // Формируем JSON согласно API:
  // Topic: idryer/{serialNumber}/status
  // QoS: 1, Retained: true
  // Frequency: При изменении режима/статуса
  //
  // JSON формат (DRYING):
  // {
  //   "units": [
  //     {
  //       "unitId": "U1",
  //       "mode": "DRYING",
  //       "sessionId": "a7b3c9d1-e4f5-6789-0abc-def123456789",
  //       "target": {
  //         "temperature": 50,
  //         "duration": 240,
  //         "humidity": 10
  //       },
  //       "elapsedTime": 180
  //     }
  //   ],
  //   "uptime": 3600,
  //   "timestamp": "2025-12-22T10:00:00Z"
  // }
  //
  // JSON формат (IDLE):
  // {
  //   "units": [
  //     {
  //       "unitId": "U1",
  //       "mode": "IDLE"
  //     }
  //   ],
  //   "uptime": 7200,
  //   "timestamp": "2025-12-22T16:00:00Z"
  // }

  DynamicJsonDocument doc(1024);
  JsonArray units = doc.createNestedArray("units");

  JsonObject unit = units.createNestedObject();
  unit["unitId"] = "U1";  // TODO: взять из payload

  // Определяем режим работы
  DryerState state = static_cast<DryerState>(latestTelemetry_.state);
  const char* modeStr = "IDLE";

  switch (state) {
    case DryerState::Idle:
      modeStr = "IDLE";
      // При IDLE очищаем sessionId (устройство остановлено)
      currentSessionId_ = "";
      break;

    case DryerState::Drying:
      modeStr = "DRYING";
      // При старте DRYING генерируем новый sessionId если его нет
      if (currentSessionId_.isEmpty() || lastMode_ != static_cast<uint8_t>(state)) {
        char uuidBuf[37];
        currentSessionId_ = MqttClient::generateUuid(uuidBuf);
        NET_LOG("[NET] Generated sessionId: %s\n", currentSessionId_.c_str());
      }
      break;

    case DryerState::Fault:
      modeStr = "FAULT";
      // При FAULT очищаем sessionId (аварийная остановка)
      currentSessionId_ = "";
      break;

    default:
      break;
  }

  unit["mode"] = modeStr;

  // Добавляем sessionId только для активных режимов
  if (!currentSessionId_.isEmpty()) {
    unit["sessionId"] = currentSessionId_;

    // Добавляем target параметры
    JsonObject target = unit.createNestedObject("target");
    target["temperature"] = latestTelemetry_.temperatureC10 / 10.0f;  // TODO: целевая, не фактическая
    target["duration"] = latestTelemetry_.remainingMinutes;  // TODO: общая длительность
    target["humidity"] = 10;  // TODO: целевая влажность

    // Добавляем elapsedTime
    unit["elapsedTime"] = latestTelemetry_.uptimeSeconds;  // TODO: время с начала сессии
  }

  // Добавляем uptime устройства
  doc["uptime"] = millis() / 1000;

  // Публикуем с retained=true
  if (mqttClient_.publishStatus(doc)) {
    lastMode_ = static_cast<uint8_t>(state);
    NET_LOG("[NET] Published status: mode=%s sessionId=%s\n",
            modeStr, currentSessionId_.c_str());
  }
}

void NetworkManager::flushPendingTelemetry() {
  if (telemetryDirty_) {
    publishTelemetry();
  }
}

// ============================================================================
// Обработка данных от RP2040
// ============================================================================

void NetworkManager::handleRpHello(const HelloPayload &payload,
                                   const FrameHeader &header) {
  (void)payload;
  NET_LOG("[NET] RP2040 hello seq=%u\n", header.sequence);
}

void NetworkManager::handleTelemetry(const TelemetryPayload &payload,
                                     const FrameHeader &header) {
  (void)header;

  // Проверяем изменение режима работы
  bool modeChanged = (latestTelemetry_.state != payload.state);

  latestTelemetry_ = payload;
  telemetryDirty_ = true;

  // Если режим изменился - публикуем status
  if (modeChanged) {
    publishStatus();
  }
}

void NetworkManager::handleCommandAck(const AckPayload &payload,
                                      const FrameHeader &header) {
  (void)payload;
  (void)header;
  // TODO: публиковать ACK в events топик
}

void NetworkManager::handleConfigAck(const AckPayload &payload,
                                     const FrameHeader &header) {
  (void)payload;
  (void)header;
  // TODO: публиковать ACK в events топик
}

void NetworkManager::handleUartError(const ErrorPayload &payload,
                                     bool remote) {
  (void)payload;
  (void)remote;
  // TODO: публиковать ошибку в events топик
}

// ============================================================================
// Обработка команд от MQTT
// ============================================================================

void NetworkManager::handleMqttCommand(const char* command, JsonObjectConst data) {
  if (!uart_) {
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

  if (strcmp(command, "start") == 0) {
    cmdCode = CommandCode::StartDry;
  } else if (strcmp(command, "stop") == 0) {
    cmdCode = CommandCode::Stop;
  } else if (strcmp(command, "get_config") == 0) {
    cmdCode = CommandCode::PushConfig;
  } else if (strcmp(command, "set_config") == 0) {
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
                       : 0;
  cmdPayload.arg1 = data["durationMinutes"].is<int>()
                       ? static_cast<uint32_t>(data["durationMinutes"].as<int>())
                       : 0;

  uart_->sendCommand(cmdPayload, true);
}

// ============================================================================
// HTTP запросы к Backend API
// ============================================================================

bool NetworkManager::httpPostJson(const String &url, const String &body,
                                  DynamicJsonDocument &response) {
  WiFiClientSecure client;
  client.setInsecure();  // TODO: добавить сертификат

  HTTPClient https;
  https.setTimeout(10000);

  if (!https.begin(client, url)) {
    return false;
  }

  https.addHeader("Content-Type", "application/json");
  int httpCode = https.POST(body);

  if (httpCode < 200 || httpCode >= 300) {
    NET_LOG("[NET] HTTP POST %s failed: %d\n", url.c_str(), httpCode);
    https.end();
    return false;
  }

  String payload = https.getString();
  https.end();

  DeserializationError err = deserializeJson(response, payload);
  if (err) {
    NET_LOG("[NET] JSON parse error: %s\n", err.c_str());
    return false;
  }

  return true;
}

bool NetworkManager::httpGetJson(const String &url,
                                 DynamicJsonDocument &response) {
  WiFiClientSecure client;
  client.setInsecure();  // TODO: добавить сертификат

  HTTPClient https;
  https.setTimeout(10000);

  if (!https.begin(client, url)) {
    return false;
  }

  int httpCode = https.GET();

  if (httpCode < 200 || httpCode >= 300) {
    NET_LOG("[NET] HTTP GET %s failed: %d\n", url.c_str(), httpCode);
    https.end();
    return false;
  }

  String payload = https.getString();
  https.end();

  DeserializationError err = deserializeJson(response, payload);
  return !err;
}

String NetworkManager::buildProvisionUrl() const {
  return String(API_BASE) + "/devices/provision";
}

String NetworkManager::buildRegisterUrl() const {
  return String(API_BASE) + "/devices/register";
}

String NetworkManager::buildCheckClaimUrl(const String &token) const {
  return String(API_BASE) + "/devices/check-claim/" + token;
}
