/**
 * @file IdryerDevice.h
 * @brief Фасад iDryer Link (application layer, не часть idryer-protocol).
 */

#pragma once

#include <functional>
#include <core/types.h>
#include <cloud/cloud_state_machine.h>
#include <cloud/telemetry_publisher.h>
#include <cloud/command_handler.h>
#include <cloud/http_api.h>
#include <config/config_manager.h>
#include <mqtt/mqtt_client.h>
#include <mqtt/ha_mqtt_client.h>
#include <cloud/ha_publisher.h>
#include <uart/uart_bridge.h>
#include <device/interfaces/IWifiManager.h>
#include <device/interfaces/IHttpClient.h>
#include <device/interfaces/ICredentialStore.h>

class WsServer;

namespace idryer {

/// (json, length, isDelta)
using ConfigReceivedCallback = std::function<void(const char* json, uint16_t length, bool isDelta)>;

/// Вызывается после таймаута hello/retry.
using McuTimeoutCallback = std::function<void()>;

/// (pin, expiresInSeconds)
using ClaimPinCallback = std::function<void(const char* pin, uint32_t expiresInSeconds)>;

/// Координирует UART, cloud state machine, MQTT и WS.
class IdryerDevice {
public:
    IdryerDevice(IWifiManager* wifi,
                 IHttpClient* http,
                 ICredentialStore* store,
                 DryerUart::UartBridge* uart,
                 const char* apiBaseUrl = nullptr);

    /// Инициализация зависимостей и регистрация callbacks.
    void begin();

    /// Главный цикл фасада. Отдельно `uart.loop()` вызывать не нужно.
    void loop();

    /// Возвращает `false`, если claiming невозможен в текущем состоянии.
    bool requestClaimProcess();

    // UART callbacks

    void handleRpHello(const DryerUart::HelloPayload& payload,
                       const DryerUart::FrameHeader& header);

    void handleTelemetry(const DryerUart::TelemetryPayload& payload,
                         const DryerUart::FrameHeader& header);

    void handleStatus(const DryerUart::StatusPayload& payload,
                      const DryerUart::FrameHeader& header);

    void handleWeights(const DryerUart::WeightsPayload& payload,
                       const DryerUart::FrameHeader& header);

    void handleRfidEvent(const DryerUart::RfidPayload& payload,
                         const DryerUart::FrameHeader& header);

    void handleCommandAck(const DryerUart::AckPayload& payload,
                          const DryerUart::FrameHeader& header);

    void handleConfigAck(const DryerUart::AckPayload& payload,
                         const DryerUart::FrameHeader& header);

    void handleUartError(const DryerUart::ErrorPayload& payload, bool remote);

    void handleLog(const DryerUart::LogPayload* log);

    /// `dataLen` — длина JSON-данных в текущем фрагменте, без chunk header.
    void handleConfigPushChunk(const DryerUart::ConfigChunkPayload& payload,
                               uint8_t dataLen,
                               const DryerUart::FrameHeader& header);

    /// `isDelta=true` -> `config/delta`, иначе `config`.
    bool publishConfig(const char* json, uint16_t length, bool isDelta = false);

    void setConfigReceivedCallback(ConfigReceivedCallback cb) {
        configCallback_ = cb;
    }

    void setMcuTimeoutCallback(McuTimeoutCallback cb) {
        mcuTimeoutCallback_ = cb;
    }

    /// Дополняет внутренний callback, который отправляет PIN на RP2040.
    void setClaimPinCallback(ClaimPinCallback cb) {
        userClaimPinCallback_ = cb;
    }

    void setWsServer(WsServer* ws) { wsServer_ = ws; }

    /// Внешняя команда идёт в тот же `CommandHandler`, что и MQTT.
    void handleExternalCommand(const char* command, JsonObjectConst data);

    bool isMcuConnected() const { return helloReceived_; }

    // Чтение состояния (без модификации)
    bool isOnline() const { return cloud_.isOnline(); }

    cloud::CloudState getCloudState() const { return cloud_.getState(); }

    const DeviceIdentity& getIdentity() const { return cloud_.getIdentity(); }

    /// Для регистрации внешних callbacks и диагностики состояния.
    cloud::CloudStateMachine* getCloudStateMachine() { return &cloud_; }

private:
    IWifiManager* wifi_;
    IHttpClient* http_;
    ICredentialStore* store_;
    DryerUart::UartBridge* uart_;

    // Компоненты облака
    cloud::HttpApi api_;
    MqttClient mqtt_;
    cloud::CloudStateMachine cloud_;
    cloud::TelemetryPublisher publisher_;
    cloud::CommandHandler cmdHandler_;

    // Home Assistant (опциональный)
    ha::HaMqttClient haMqtt_;
    ha::HaPublisher haPublisher_;
    bool haEnabled_ = false;

    bool helloReceived_ = false;   // Публикации разрешаются после первого Hello.
    uint8_t unitsCount_ = 1;       // Актуализируется из Hello payload.

    // Кэш последнего Hello от MCU — публикуется повторно при переподключении MQTT.
    DryerUart::HelloPayload lastHello_{};
    bool lastHelloValid_ = false;

    // Офлайн-кэш: последнее RFID событие на ридер.
    static constexpr uint8_t MAX_RFID_READERS = 4;
    DryerUart::RfidPayload latestRfid_[MAX_RFID_READERS]{};
    bool rfidDirty_[MAX_RFID_READERS] = {false};

    // Сборка фрагментов ConfigPush от MCU.
    DryerUart::ConfigReceiver configReceiver_;

    ConfigReceivedCallback configCallback_;
    McuTimeoutCallback mcuTimeoutCallback_;
    ClaimPinCallback userClaimPinCallback_;

    // Состояние retry для Hello Request.
    uint32_t lastHelloRequestMs_ = 0;
    uint8_t helloRequestAttempts_ = 0;
    bool mcuTimeoutNotified_ = false;

    // Локальный WebSocket мост.
    WsServer* wsServer_ = nullptr;

    // Состояние heartbeat.
    uint32_t lastHeartbeatAt_ = 0;
    uint32_t heartbeatErrors_ = 0;

    void registerUartHandlers();
    void processHeartbeat();
    void sendHelloRequest();
    void publishCachedData();
    void publishDeviceInfo(const DryerUart::HelloPayload& payload);
    void syncTimeFromBackend(const char* timestamp);

    // Home Assistant
    void initHomeAssistant();
    void publishHADiscovery();

    void handleMqttCommand(const char* command, JsonObjectConst data);

    static void onCloudStateChange(cloud::CloudState oldState,
                                   cloud::CloudState newState,
                                   void* ctx);

    static void onClaimPin(const char* pin, uint32_t expiresInSeconds, void* ctx);

    static void onClaimComplete(const char* deviceId, void* ctx);

    static void onUnclaimed(void* ctx);
};

} // namespace idryer
