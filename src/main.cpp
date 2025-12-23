#include <Arduino.h>

#include "network_manager.h"
#include "uart_bridge.h"

using namespace DryerUart;

namespace {

// ESP32-C3 UART pins for RP2040 communication
constexpr int UART_RX_PIN = 16; // GPIO16
constexpr int UART_TX_PIN = 17; // GPIO17

constexpr uint32_t FIRMWARE_VERSION = (1u << 16); // 1.0.0
constexpr uint32_t CAPABILITIES_MASK = 0x00000001;

UartBridge uartBridge;
NetworkManager networkManager;
uint32_t lastHeartbeatAt = 0;
uint32_t heartbeatErrors = 0;

void handleHello(const HelloPayload &payload, const FrameHeader &header) {
  if (header.kind == MessageKind::Hello) {
    HelloPayload response{};
    response.role = Role::EspBridge;
    response.firmwareVersion = FIRMWARE_VERSION;
    response.capabilitiesMask = CAPABILITIES_MASK;
    uartBridge.sendHelloAck(response);
  }
  networkManager.handleRpHello(payload, header);
}

void handleTelemetry(const TelemetryPayload &payload,
                     const FrameHeader &header) {
  networkManager.handleTelemetry(payload, header);
}

void handleCommandAck(const AckPayload &payload, const FrameHeader &header) {
  networkManager.handleCommandAck(payload, header);
  if (payload.status != ErrorCode::None) {
    heartbeatErrors++;
  }
}

void handleConfigAck(const AckPayload &payload, const FrameHeader &header) {
  networkManager.handleConfigAck(payload, header);
  if (payload.status != ErrorCode::None) {
    heartbeatErrors++;
  }
}

void handleHeartbeat(const HeartbeatPayload &, const FrameHeader &) {
  // RP2040 сообщает о своём аптайме, ESP обновляет сторожей
}

void handleError(const ErrorPayload &payload, bool remote) {
  networkManager.handleUartError(payload, remote);
  heartbeatErrors++;
}

void processHeartbeat() {
  const uint32_t now = millis();
  if (now - lastHeartbeatAt < HEARTBEAT_INTERVAL_MS) {
    return;
  }

  HeartbeatPayload payload{};
  payload.uptimeSeconds = now / 1000;
  payload.wifiRssiDbm = 0;
  payload.errorsSinceBoot = heartbeatErrors;
  uartBridge.sendHeartbeat(payload);

  lastHeartbeatAt = now;
}

void sendInitialHello() {
  HelloPayload payload{};
  payload.role = Role::EspBridge;
  payload.firmwareVersion = FIRMWARE_VERSION;
  payload.capabilitiesMask = CAPABILITIES_MASK;
  uartBridge.sendHello(payload, false);
}

} // namespace

void setup() {
#ifdef DEBUG_SERIAL
  DEBUG_SERIAL.begin(115200);
#endif

  // Initialize UART1 for RP2040 communication on GPIO16/17
  Serial1.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  uartBridge.begin(Serial1, 115200);

  uartBridge.setHelloHandler(handleHello);
  uartBridge.setTelemetryHandler(handleTelemetry);
  uartBridge.setCommandAckHandler(handleCommandAck);
  uartBridge.setConfigAckHandler(handleConfigAck);
  uartBridge.setHeartbeatHandler(handleHeartbeat);
  uartBridge.setErrorHandler(handleError);

  networkManager.begin(&uartBridge);
  sendInitialHello();
}

void loop() {
  uartBridge.loop();
  processHeartbeat();
  networkManager.loop();
  delay(1);
}
