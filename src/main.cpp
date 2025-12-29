#include <Arduino.h>
#include <idryer_protocol.h>
#include <platform/arduino/idryer_arduino.h>
#include <ArduinoJson.h>

#include "secrets_config.h"

// Раскомментируйте для включения Improv Wi-Fi (настройка через браузер)
// #define ENABLE_IMPROV_WIFI

#ifdef ENABLE_IMPROV_WIFI
#include <ImprovWiFiLibrary.h>
#include <Preferences.h>
#endif

using namespace DryerUart;

#ifdef DEBUG_SERIAL
// ANSI цветовые коды для терминала
// Формат: \033[XXm где XX - код цвета
#define ANSI_RESET "\033[0m" // Сброс форматирования

// Обычные цвета (30-37)
#define ANSI_BLACK "\033[30m"
#define ANSI_RED "\033[31m"
#define ANSI_GREEN "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_BLUE "\033[34m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_CYAN "\033[36m"
#define ANSI_WHITE "\033[37m"

// Яркие цвета (90-97)
#define ANSI_BRIGHT_BLACK "\033[90m"   // Серый
#define ANSI_BRIGHT_RED "\033[91m"
#define ANSI_BRIGHT_GREEN "\033[92m"
#define ANSI_BRIGHT_YELLOW "\033[93m"
#define ANSI_BRIGHT_BLUE "\033[94m"
#define ANSI_BRIGHT_MAGENTA "\033[95m"
#define ANSI_BRIGHT_CYAN "\033[96m"
#define ANSI_BRIGHT_WHITE "\033[97m"

// Стили текста
#define ANSI_BOLD "\033[1m"      // Жирный
#define ANSI_DIM "\033[2m"       // Тусклый
#define ANSI_UNDERLINE "\033[4m" // Подчеркнутый
#define ANSI_BLINK "\033[5m"     // Мигающий
#define ANSI_REVERSE "\033[7m"   // Инверсия цветов

#define DEBUG_LOG(...) DEBUG_SERIAL.printf(__VA_ARGS__)
#define DEBUG_JSON(doc)             \
  serializeJson(doc, DEBUG_SERIAL); \
  DEBUG_SERIAL.println();
#else
#define DEBUG_LOG(...)
#define DEBUG_JSON(doc)
#endif

namespace
{

  // ESP32-C3 UART pins for RP2040 communication
  constexpr int UART_RX_PIN = 6; // GPIO6 (пробуем другой пин)
  constexpr int UART_TX_PIN = 7; // GPIO7 (пробуем другой пин)

#ifdef ENABLE_IMPROV_WIFI
  Preferences preferences;
  ImprovWiFi improvSerial(&Serial);
  bool wifiConfigured = false;

  // Сохранение Wi-Fi credentials в NVS
  void saveWiFiCredentials(const char *ssid, const char *password)
  {
    preferences.begin("wifi", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.putBool("configured", true);
    preferences.end();
    DEBUG_LOG("[IMPROV] WiFi credentials saved\n");
  }

  // Загрузка Wi-Fi credentials из NVS
  bool loadWiFiCredentials(String &ssid, String &password)
  {
    preferences.begin("wifi", true);
    bool configured = preferences.getBool("configured", false);
    if (configured)
    {
      ssid = preferences.getString("ssid", "");
      password = preferences.getString("password", "");
    }
    preferences.end();
    return configured && ssid.length() > 0;
  }

  // Очистка сохранённых credentials (для отладки)
  void clearWiFiCredentials()
  {
    preferences.begin("wifi", false);
    preferences.clear();
    preferences.end();
    DEBUG_LOG("[IMPROV] WiFi credentials cleared\n");
  }
#endif

  constexpr uint32_t FIRMWARE_VERSION = (1u << 16); // 1.0.0

  UartBridge uartBridge;
  ArduinoWifiManager wifiManager;
  ArduinoHttpClient httpClient;
  ArduinoCredentialStore credStore;
  IdryerDevice device(&wifiManager, &httpClient, &credStore, &uartBridge);

#ifdef ENABLE_IMPROV_WIFI
  // Callback при получении credentials от Improv
  void onImprovWiFiConnectCallback(const char *ssid, const char *password)
  {
    DEBUG_LOG("[IMPROV] Received credentials - SSID: %s\n", ssid);
    saveWiFiCredentials(ssid, password);
    wifiManager.begin(ssid, password);
    wifiConfigured = true;
  }

  // Callback при ошибке Improv
  void onImprovWiFiErrorCallback(ImprovTypes::Error err)
  {
    DEBUG_LOG("[IMPROV] Error: %d\n", err);
  }
#endif

  uint32_t lastHeartbeatAt = 0;
  uint32_t heartbeatErrors = 0;

  void handleHello(const HelloPayload &payload, const FrameHeader &header)
  {
    if (header.kind == MessageKind::Hello)
    {
      HelloAckPayload response{};
      response.ipAddress = 0; // будет обновлено в NetworkManager
      response.ssid[0] = '\0';
      uartBridge.sendHelloAck(response);
    }
    device.handleRpHello(payload, header);
  }

  void handleTelemetry(const TelemetryPayload &payload, const FrameHeader &header)
  {
#ifdef DEBUG_SERIAL
    DEBUG_LOG("\n" ANSI_BLUE "← Telemetry (seq=%d, count=%d)" ANSI_RESET "\n", header.sequence, payload.count);
    StaticJsonDocument<512> doc;
    JsonArray units = doc.createNestedArray("units");
    for (uint8_t i = 0; i < payload.count && i < 4; i++)
    {
      JsonObject unit = units.createNestedObject();
      unit["unitId"] = payload.units[i].unitId;
      unit["tempC"] = payload.units[i].temperatureC10 / 10.0;
      unit["humidity"] = payload.units[i].humidityPct;
    }
    DEBUG_JSON(doc);
#endif
    device.handleTelemetry(payload, header);
  }

  void handleCommandAck(const AckPayload &payload, const FrameHeader &header)
  {
    device.handleCommandAck(payload, header);
    if (payload.status != ErrorCode::None)
    {
      heartbeatErrors++;
    }
  }

  void handleConfigAck(const AckPayload &payload, const FrameHeader &header)
  {
    device.handleConfigAck(payload, header);
    if (payload.status != ErrorCode::None)
    {
      heartbeatErrors++;
    }
  }

  void handleHeartbeat(const HeartbeatPayload &payload, const FrameHeader &header)
  {
#ifdef DEBUG_SERIAL
    DEBUG_LOG("\n" ANSI_BLUE "← Heartbeat (seq=%d)" ANSI_RESET "\n", header.sequence);
    StaticJsonDocument<128> doc;
    doc["uptime"] = payload.uptimeSeconds;
    doc["rssi"] = payload.wifiRssiDbm;
    doc["errors"] = payload.errorsSinceBoot;
    DEBUG_JSON(doc);
#endif
    // RP2040 сообщает о своём аптайме, ESP обновляет сторожей
  }

  void handleError(const ErrorPayload &payload, bool remote)
  {
    device.handleUartError(payload, remote);
    heartbeatErrors++;
  }

  void handleStatus(const StatusPayload &payload, const FrameHeader &header)
  {
#ifdef DEBUG_SERIAL
    DEBUG_LOG("\n" ANSI_BLUE "← Status (seq=%d, count=%d)" ANSI_RESET "\n", header.sequence, payload.count);
    StaticJsonDocument<1024> doc;
    doc["uptime"] = payload.uptime;
    JsonArray units = doc.createNestedArray("units");
    for (uint8_t i = 0; i < payload.count && i < 4; i++)
    {
      JsonObject unit = units.createNestedObject();
      unit["unitId"] = payload.units[i].unitId;
      unit["mode"] = static_cast<int>(payload.units[i].mode);
      unit["sessionNum"] = payload.units[i].sessionNum;
      unit["elapsed"] = payload.units[i].elapsedSeconds;
      unit["remaining"] = payload.units[i].totalRemainingSeconds;
      unit["stage"] = payload.units[i].currentStage;
    }
    DEBUG_JSON(doc);
#endif
    device.handleStatus(payload, header);
  }

  void handleWeights(const WeightsPayload &payload, const FrameHeader &header)
  {
#ifdef DEBUG_SERIAL
    DEBUG_LOG("\n" ANSI_BLUE "← Weights (seq=%d, count=%d)" ANSI_RESET "\n", header.sequence, payload.count);
    StaticJsonDocument<256> doc;
    JsonArray weights = doc.createNestedArray("weights");
    for (uint8_t i = 0; i < payload.count && i < 4; i++)
    {
      JsonObject w = weights.createNestedObject();
      w["unitId"] = payload.weights[i].unitId;
      w["weight"] = payload.weights[i].weightGrams;
    }
    DEBUG_JSON(doc);
#endif
    device.handleWeights(payload, header);
  }

  void handleRfid(const RfidPayload &payload, const FrameHeader &header)
  {
#ifdef DEBUG_SERIAL
    DEBUG_LOG("\n" ANSI_BLUE "← RFID (seq=%d)" ANSI_RESET "\n", header.sequence);
    StaticJsonDocument<256> doc;
    doc["event"] = static_cast<int>(payload.event);
    doc["readerId"] = payload.readerId;
    doc["unitId"] = payload.unitId;
    doc["tag"] = String(payload.tag);
    DEBUG_JSON(doc);
#endif
    device.handleRfidEvent(payload, header);
  }

  void processHeartbeat()
  {
    const uint32_t now = millis();
    if (now - lastHeartbeatAt < HEARTBEAT_INTERVAL_MS)
    {
      return;
    }

    HeartbeatPayload payload{};
    payload.uptimeSeconds = now / 1000;
    payload.wifiRssiDbm = 0;
    payload.errorsSinceBoot = heartbeatErrors;
    uartBridge.sendHeartbeat(payload);

    lastHeartbeatAt = now;
  }

  void sendInitialHello()
  {
    DEBUG_LOG(ANSI_GREEN "→ Sending Hello from ESP32..." ANSI_RESET "\n");
    HelloPayload payload{};
    payload.role = Role::EspBridge;
    payload.firmwareVersion = FIRMWARE_VERSION;
    payload.workTimeCounter = millis() / 1000;
    strncpy(payload.hardwareVersion, "v1.0", sizeof(payload.hardwareVersion) - 1);
    uartBridge.sendHello(payload, false);
  }

  void handleClaimStart(const FrameHeader &header)
  {
    DEBUG_LOG("\n" ANSI_BLUE "← ClaimStart from RP2040 (seq=%d)" ANSI_RESET "\n", header.sequence);
    // Вызываем процесс claiming в IdryerDevice
    bool result = device.requestClaimProcess();
    DEBUG_LOG("[CLAIM] requestClaimProcess() returned: %s\n", result ? "true" : "false");
  }

} // namespace

void setup()
{
#ifdef DEBUG_SERIAL
  DEBUG_SERIAL.begin(115200);
  delay(100);
  DEBUG_LOG("\n\n========================================\n");
  DEBUG_LOG("[BOOT] iDryer Link ESP32-C3 starting...\n");
  DEBUG_LOG("========================================\n");
#endif

  // Отключаем JTAG для использования GPIO6/7
  gpio_reset_pin((gpio_num_t)UART_RX_PIN);
  gpio_reset_pin((gpio_num_t)UART_TX_PIN);

  // Initialize UART1 for RP2040 communication on GPIO6/7
  DEBUG_LOG("[INIT] Initializing UART (RX=GPIO%d, TX=GPIO%d, 115200)...\n", UART_RX_PIN, UART_TX_PIN);
  Serial1.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  DEBUG_LOG("[INIT] Serial1 initialized\n");
  uartBridge.begin(Serial1, 115200);
  DEBUG_LOG("[INIT] UartBridge.begin() called\n");

  uartBridge.setHelloHandler(handleHello);
  uartBridge.setTelemetryHandler(handleTelemetry);
  uartBridge.setCommandAckHandler(handleCommandAck);
  uartBridge.setConfigAckHandler(handleConfigAck);
  uartBridge.setHeartbeatHandler(handleHeartbeat);
  uartBridge.setErrorHandler(handleError);
  uartBridge.setStatusHandler(handleStatus);
  uartBridge.setWeightsHandler(handleWeights);
  uartBridge.setRfidHandler(handleRfid);
  uartBridge.setClaimStartHandler(handleClaimStart);

  DEBUG_LOG("\n[INIT] Starting WiFi and Device...\n");

#ifdef ENABLE_IMPROV_WIFI
  // Инициализация Improv Wi-Fi
  improvSerial.setDeviceInfo(
      ImprovTypes::ChipFamily::CF_ESP32_C3,
      "iDryer Link",
      "1.0.0",
      "iDryer",
      "http://{LOCAL_IPV4}/?name={DEVICE_NAME}");

  improvSerial.onImprovConnected(onImprovWiFiConnectCallback);
  improvSerial.onImprovError(onImprovWiFiErrorCallback);

  // Проверяем, есть ли сохранённые credentials
  String savedSSID, savedPassword;
  if (loadWiFiCredentials(savedSSID, savedPassword))
  {
    DEBUG_LOG("[IMPROV] Using saved credentials: %s\n", savedSSID.c_str());
    wifiManager.begin(savedSSID.c_str(), savedPassword.c_str());
    wifiConfigured = true;
  }
  else
  {
    DEBUG_LOG("[IMPROV] No saved credentials. Waiting for Improv Wi-Fi configuration...\n");
    // Wi-Fi будет настроен через Improv callback
  }
#else
  // Используем credentials из secrets_config.h
  wifiManager.begin(IDRYER_WIFI_SSID, IDRYER_WIFI_PASSWORD);
#endif

  device.begin();
  DEBUG_LOG("[INIT] Device started. Ready to receive UART messages...\n");

  // credStore.clear();
  // clearWiFiCredentials(); // Раскомментируйте для сброса Wi-Fi настроек
  sendInitialHello();
}

void loop()
{
  static uint32_t lastLoopLog = 0;
  static uint32_t loopCounter = 0;
  const uint32_t now = millis();

  loopCounter++;

  // Периодический лог (для отладки)
  // if (now - lastLoopLog > 10000)
  // {
  //   DEBUG_LOG("[MAIN] Loop running... (iterations: %u, Serial1.available=%d)\n",
  //             loopCounter, Serial1.available());
  //   loopCounter = 0;
  //   lastLoopLog = now;
  // }

  uartBridge.loop();
  processHeartbeat();
  device.loop();

#ifdef ENABLE_IMPROV_WIFI
  // Обработка Improv Wi-Fi команд через Serial
  improvSerial.handleSerial();
#endif
}
