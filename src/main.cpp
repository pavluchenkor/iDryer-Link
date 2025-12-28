#include <Arduino.h>
#include <idryer_protocol.h>
#include <ArduinoJson.h>

#include "network_manager.h"

using namespace DryerUart;

#ifdef DEBUG_SERIAL
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

  constexpr uint32_t FIRMWARE_VERSION = (1u << 16); // 1.0.0

  UartBridge uartBridge;
  NetworkManager networkManager;
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
    networkManager.handleRpHello(payload, header);
  }

  void handleTelemetry(const TelemetryPayload &payload, const FrameHeader &header)
  {
#ifdef DEBUG_SERIAL
    DEBUG_LOG("\n[RECV] Telemetry (seq=%d, count=%d)\n", header.sequence, payload.count);
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
    networkManager.handleTelemetry(payload, header);
  }

  void handleCommandAck(const AckPayload &payload, const FrameHeader &header)
  {
    networkManager.handleCommandAck(payload, header);
    if (payload.status != ErrorCode::None)
    {
      heartbeatErrors++;
    }
  }

  void handleConfigAck(const AckPayload &payload, const FrameHeader &header)
  {
    networkManager.handleConfigAck(payload, header);
    if (payload.status != ErrorCode::None)
    {
      heartbeatErrors++;
    }
  }

  void handleHeartbeat(const HeartbeatPayload &payload, const FrameHeader &header)
  {
#ifdef DEBUG_SERIAL
    DEBUG_LOG("\n[RECV] Heartbeat (seq=%d)\n", header.sequence);
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
    networkManager.handleUartError(payload, remote);
    heartbeatErrors++;
  }

  void handleStatus(const StatusPayload &payload, const FrameHeader &header)
  {
#ifdef DEBUG_SERIAL
    DEBUG_LOG("\n[RECV] Status (seq=%d, count=%d)\n", header.sequence, payload.count);
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
    networkManager.handleStatus(payload, header);
  }

  void handleWeights(const WeightsPayload &payload, const FrameHeader &header)
  {
#ifdef DEBUG_SERIAL
    DEBUG_LOG("\n[RECV] Weights (seq=%d, count=%d)\n", header.sequence, payload.count);
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
    networkManager.handleWeights(payload, header);
  }

  void handleRfid(const RfidPayload &payload, const FrameHeader &header)
  {
#ifdef DEBUG_SERIAL
    DEBUG_LOG("\n[RECV] RFID (seq=%d)\n", header.sequence);
    StaticJsonDocument<256> doc;
    doc["event"] = static_cast<int>(payload.event);
    doc["readerId"] = payload.readerId;
    doc["unitId"] = payload.unitId;
    doc["tag"] = String(payload.tag);
    DEBUG_JSON(doc);
#endif
    networkManager.handleRfidEvent(payload, header);
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
    HelloPayload payload{};
    payload.role = Role::EspBridge;
    payload.firmwareVersion = FIRMWARE_VERSION;
    payload.workTimeCounter = millis() / 1000;
    strncpy(payload.hardwareVersion, "v1.0", sizeof(payload.hardwareVersion) - 1);
    uartBridge.sendHello(payload, false);
  }

  void handleClaimStart(const FrameHeader &header)
  {
    DEBUG_LOG("\n[CLAIM] ClaimStart received from RP2040 (seq=%d)\n", header.sequence);
    // Вызываем процесс claiming в NetworkManager
    networkManager.requestClaimProcess();
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

  // Initialize UART1 for RP2040 communication on GPIO6/7
  DEBUG_LOG("[INIT] Initializing UART (GPIO6/7, 115200)...\n");
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

  DEBUG_LOG("\n[INIT] Starting NetworkManager...\n");
  networkManager.begin(&uartBridge);
  DEBUG_LOG("[INIT] NetworkManager started. Ready to receive UART messages...\n");

  sendInitialHello();
}

void loop()
{
  uartBridge.loop();
  processHeartbeat();
  networkManager.loop();
}
