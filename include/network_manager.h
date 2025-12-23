#pragma once

#include <ArduinoJson.h>

#include "credential_store.h"
#include "secrets_config.h"
#include "mqtt/mqtt_client.h"  // Из библиотеки idryer-protocol
#include "uart_bridge.h"

/**
 * @brief Менеджер сетевых подключений и MQTT коммуникации
 *
 * Управляет:
 * - Подключением к Wi-Fi
 * - Регистрацией устройства (provision + register + polling)
 * - MQTT подключением и публикацией топиков
 * - Обработкой команд от Backend
 */
class NetworkManager {
 public:
  void begin(DryerUart::UartBridge *bridge);
  void loop();

  // Обработка данных от RP2040 через UART
  void handleRpHello(const DryerUart::HelloPayload &payload,
                     const DryerUart::FrameHeader &header);
  void handleTelemetry(const DryerUart::TelemetryPayload &payload,
                       const DryerUart::FrameHeader &header);
  void handleCommandAck(const DryerUart::AckPayload &payload,
                        const DryerUart::FrameHeader &header);
  void handleConfigAck(const DryerUart::AckPayload &payload,
                       const DryerUart::FrameHeader &header);
  void handleUartError(const DryerUart::ErrorPayload &payload, bool remote);

 private:
  enum class CloudState : uint8_t {
    Idle,
    WifiConnecting,
    Provisioning,      // POST /devices/provision (получение token)
    Registering,       // POST /devices/register (получение PIN)
    AwaitingClaim,     // Polling /check-claim
    Ready,             // Готов к подключению MQTT
    MqttConnecting,    // Подключение к MQTT брокеру
    Online             // Подключен к MQTT
  };

  // Состояние подключения
  void ensureWifi();
  void ensureProvisioning();
  void ensureRegistration();
  void pollClaimStatus();
  void ensureMqtt();

  // Регистрация устройства (HTTPS API)
  bool provisionDevice();
  bool registerDevice();
  bool checkClaim();
  void scheduleNextClaimPoll();

  // Публикация MQTT топиков
  void publishInfoOnce();
  void publishTelemetry();
  void publishStatus();
  void flushPendingTelemetry();

  // Обработка команд от MQTT
  void handleMqttCommand(const char* command, JsonObjectConst data);

  // HTTP запросы к Backend API
  bool httpPostJson(const String &url, const String &body,
                    DynamicJsonDocument &response);
  bool httpGetJson(const String &url, DynamicJsonDocument &response);
  String buildProvisionUrl() const;
  String buildRegisterUrl() const;
  String buildCheckClaimUrl(const String &token) const;

  DryerUart::UartBridge *uart_ = nullptr;
  MqttClient mqttClient_;
  CredentialStore credentialStore_;
  DeviceIdentity identity_;
  CloudState state_ = CloudState::Idle;

  bool wifiScanLogged_ = false;
  uint32_t lastWifiAttempt_ = 0;
  uint32_t lastProvisionAttempt_ = 0;
  uint32_t lastRegistrationAttempt_ = 0;
  uint32_t lastClaimPollAt_ = 0;
  uint32_t lastMqttConnectAttempt_ = 0;
  uint32_t lastTelemetryPublishAt_ = 0;
  bool awaitingClaim_ = false;
  bool infoPublished_ = false;
  String pendingPin_;

  DryerUart::TelemetryPayload latestTelemetry_{};
  bool telemetryDirty_ = false;

  // Session management
  String currentSessionId_;  // UUID сессии для DRYING/STORAGE/PROFILE
  uint8_t lastMode_ = 0;     // Последний режим (для отслеживания изменений)
};
