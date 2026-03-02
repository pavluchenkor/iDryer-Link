/**
 * @file WsServer.h
 * @brief WebSocket сервер для локального доступа к iDryer без облака
 *
 * Активируется из меню RP2040 через UART (WsEnable 0x73).
 * Полный аналог MQTT API: те же JSON форматы, те же команды.
 * mDNS имя совпадает с serial number устройства.
 */

#pragma once

#include <functional>
#include <ArduinoJson.h>
#include <uart/uart_bridge.h>

class WebSocketsServer;

class WsServer {
public:
    explicit WsServer(DryerUart::UartBridge* uart);
    ~WsServer();

    /**
     * @brief Запуск WS сервера + mDNS
     * @param deviceName Имя устройства (serial number, например "DEVICE_4AF988_3847291")
     * @param pin PIN-код 4 цифры (1000-9999), генерируется на MCU
     */
    void begin(const char* deviceName, uint16_t pin);

    /** @brief Остановка WS сервера + mDNS */
    void stop();

    /** @brief Обработка WS событий, вызывать в loop() */
    void loop();

    bool isEnabled() const { return enabled_; }
    bool isClientConnected() const { return connectedClient_ >= 0 && clientAuthorized_; }

    // Публикация данных (вызывается из IdryerDevice)
    void sendTelemetry(const DryerUart::TelemetryPayload& data);
    void sendStatus(const DryerUart::StatusPayload& data);
    void sendWeights(const DryerUart::WeightsPayload& data);
    void sendRfid(const DryerUart::RfidPayload& data);
    void sendConfig(const char* json, uint16_t length, bool isDelta);
    void sendInfo(uint8_t unitsCount, const DryerUart::UnitConfig* units,
                  const char* hwVersion, const char* fwVersion, uint32_t workTime,
                  const char* mcuSerial = nullptr);

    /** @brief Получить текущий статус для UART (WsStatusPayload) */
    DryerUart::WsStatusPayload getStatus() const;

    /** @brief Сброс привязанных клиентов */
    void resetClients();

    /** @brief Callback для входящих команд от WS клиента */
    using CommandCallback = std::function<void(const char* command, JsonObjectConst data)>;
    void setCommandCallback(CommandCallback cb) { cmdCallback_ = cb; }

private:
    char deviceName_[40];  // "DEVICE_XXXXXX_XXXXXXX"

    // WebSocket
    WebSocketsServer* ws_ = nullptr;
    bool enabled_ = false;
    int8_t connectedClient_ = -1;
    bool clientAuthorized_ = false;

    // PIN и клиенты
    uint16_t pin_ = 0;   // PIN 4 цифры (от MCU)
    char pinStr_[5];      // "1234\0"
    static constexpr uint8_t MAX_PAIRED = 5;
    char pairedClients_[MAX_PAIRED][40];
    uint8_t pairedCount_ = 0;

    // NVS (только клиенты, PIN от MCU)
    void loadClientsFromNvs();
    void saveClientsToNvs();

    // Auth
    bool authenticateClient(const char* pin, const char* clientId);
    bool isClientPaired(const char* clientId) const;

    // WS события
    void onWsEvent(uint8_t num, uint8_t type, uint8_t* payload, size_t length);
    void handleWsMessage(uint8_t num, const char* json, size_t length);

    // JSON helpers
    void sendJson(const char* type, JsonDocument& doc);

    // Вспомогательные форматеры (аналогично TelemetryPublisher)
    static void formatUnitId(char* buf, size_t size, uint8_t unitId);
    static void formatSensorId(char* buf, size_t size, uint8_t sensorId);
    static const char* modeToString(DryerUart::DryerMode mode);
    static const char* rfidEventToString(DryerUart::RfidEvent event);

    CommandCallback cmdCallback_;
    DryerUart::UartBridge* uart_;
};
