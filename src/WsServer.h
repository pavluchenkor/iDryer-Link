/**
 * @file WsServer.h
 * @brief WebSocket сервер для локального доступа к iDryer без облака
 *
 * Активируется из меню RP2040 через UART (WsEnable 0x73).
 * Полный аналог MQTT API: те же JSON форматы, те же команды.
 * mDNS имя совпадает с serial number устройства.
 *
 * Аутентификация: клиент предъявляет device_token (выдан порталом).
 * Никакого отдельного PIN и локальной привязки клиентов нет.
 */

#pragma once

#include <functional>
#include <ArduinoJson.h>
#include <uart/uart_bridge.h>
#include <core/config.h>

class WebSocketsServer;

class WsServer {
public:
    explicit WsServer(DryerUart::UartBridge* uart);
    ~WsServer();

    /**
     * @brief Запуск WS сервера + mDNS
     * @param deviceName Имя устройства (serial number, например "DEVICE_4AF988_3847291")
     * @param deviceToken Device token из портала (хранится в NVS, известен приложению)
     */
    void begin(const char* deviceName, const char* deviceToken);

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

    /** @brief Callback для входящих команд от WS клиента */
    using CommandCallback = std::function<void(const char* command, JsonObjectConst data)>;
    void setCommandCallback(CommandCallback cb) { cmdCallback_ = cb; }

private:
    char deviceName_[40];                    // "DEVICE_XXXXXX_XXXXXXX"
    char deviceToken_[IDRYER_MAX_TOKEN_LEN]; // Device token для проверки auth

    // WebSocket
    WebSocketsServer* ws_ = nullptr;
    bool enabled_ = false;
    int8_t connectedClient_ = -1;
    bool clientAuthorized_ = false;

    // WS события
    void onWsEvent(uint8_t num, uint8_t type, uint8_t* payload, size_t length);
    void handleWsMessage(uint8_t num, const char* json, size_t length);

    // JSON helpers
    void sendJson(const char* type, JsonDocument& doc);
    void sendWrappedJsonRaw(const char* type, const char* json, size_t length);

    // Вспомогательные форматеры (аналогично TelemetryPublisher)
    static void formatUnitId(char* buf, size_t size, uint8_t unitId);
    static void formatSensorId(char* buf, size_t size, uint8_t sensorId);
    static const char* modeToString(DryerUart::DryerMode mode);
    static const char* rfidEventToString(DryerUart::RfidEvent event);

    CommandCallback cmdCallback_;
    DryerUart::UartBridge* uart_;
};
