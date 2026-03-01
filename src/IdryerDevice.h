/**
 * @file IdryerDevice.h
 * @brief Фасад устройства iDryer Link (ESP32-C3 + RP2040)
 *
 * Это APPLICATION CODE, специфичный для iDryer Link.
 * Не является частью библиотеки idryer-protocol.
 *
 * IdryerDevice объединяет компоненты библиотеки:
 * - CloudStateMachine - управление WiFi/Provision/MQTT подключением
 * - TelemetryPublisher - публикация телеметрии в MQTT
 * - CommandHandler - обработка MQTT команд
 * - HttpApi - HTTP API к backend
 * - UartBridge - UART протокол с RP2040
 *
 * Пример использования:
 * @code
 * #include <idryer_protocol.h>
 * #include <platform/arduino/idryer_arduino.h>
 * #include "IdryerDevice.h"
 *
 * using namespace idryer;
 *
 * ArduinoWifiManager wifi;
 * ArduinoHttpClient http;
 * ArduinoCredentialStore store;
 * hal::ArduinoSerial uartSerial(&Serial2);
 * DryerUart::UartBridge uart;
 *
 * IdryerDevice device(&wifi, &http, &store, &uart);
 *
 * void setup() {
 *     Serial.begin(115200);
 *     hal::initArduinoHal(&Serial);
 *
 *     wifi.begin(WIFI_SSID, WIFI_PASSWORD);
 *     uart.begin(&uartSerial, 115200);
 *     device.begin();
 * }
 *
 * void loop() {
 *     device.loop();  // UART + cloud + heartbeat
 * }
 * @endcode
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
#include <uart/uart_bridge.h>
#include <device/interfaces/IWifiManager.h>
#include <device/interfaces/IHttpClient.h>
#include <device/interfaces/ICredentialStore.h>

namespace idryer {

/// Callback при получении конфига от MCU (json, length, isDelta)
using ConfigReceivedCallback = std::function<void(const char* json, uint16_t length, bool isDelta)>;

/// Callback при таймауте связи с MCU (MCU не отвечает на Hello Request)
using McuTimeoutCallback = std::function<void()>;

/// Callback при получении PIN для привязки устройства (pin, expiresInSeconds)
using ClaimPinCallback = std::function<void(const char* pin, uint32_t expiresInSeconds)>;

/**
 * @brief Главный класс устройства iDryer Link
 *
 * Объединяет все компоненты в единую систему:
 * 1. WiFi подключение (через IWifiManager)
 * 2. Регистрация устройства (provision → register → claim)
 * 3. MQTT коммуникация
 * 4. UART протокол с RP2040
 */
class IdryerDevice {
public:
    /**
     * @brief Конструктор
     * @param wifi WiFi менеджер
     * @param http HTTP клиент
     * @param store хранилище credentials
     * @param uart UART мост к RP2040
     * @param apiBaseUrl базовый URL API (по умолчанию из config)
     */
    IdryerDevice(IWifiManager* wifi,
                 IHttpClient* http,
                 ICredentialStore* store,
                 DryerUart::UartBridge* uart,
                 const char* apiBaseUrl = nullptr);

    /**
     * @brief Инициализация устройства
     *
     * Загружает credentials, настраивает callbacks.
     * Вызвать один раз в setup().
     */
    void begin();

    /**
     * @brief Главный цикл обработки
     *
     * ОБЯЗАТЕЛЬНО вызывать в loop()!
     * Обрабатывает UART, WiFi, MQTT, heartbeat, публикацию телеметрии.
     * Отдельный вызов uart.loop() НЕ нужен.
     */
    void loop();

    /**
     * @brief Запустить процесс привязки устройства (claiming)
     * @return true если процесс запущен
     *
     * Вызывается по запросу пользователя (кнопка на экране RP2040).
     * Генерирует PIN и начинает polling статуса.
     */
    bool requestClaimProcess();

    // =========================================================================
    // UART CALLBACKS
    // Вызываются из UartBridge при получении данных от RP2040
    // =========================================================================

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

    /**
     * @brief Обработка фрагментированного конфига от RP2040
     * @param payload Фрагмент конфига
     * @param dataLen Длина данных (без заголовка)
     * @param header Заголовок фрейма (flags для определения LAST_FRAGMENT)
     */
    void handleConfigPushChunk(const DryerUart::ConfigChunkPayload& payload,
                               uint8_t dataLen,
                               const DryerUart::FrameHeader& header);

    /**
     * @brief Публикация готового JSON конфига в MQTT
     * @param json JSON строка
     * @param length Длина JSON
     * @param isDelta true для delta топика, false для полного конфига
     * @return true если опубликовано успешно
     */
    bool publishConfig(const char* json, uint16_t length, bool isDelta = false);

    /**
     * @brief Установить callback для получения конфига от MCU
     *
     * Вызывается когда конфиг полностью получен (после сборки фрагментов).
     * Callback получает raw JSON для парсинга через menu_parseFullConfig/menu_parseDelta.
     */
    void setConfigReceivedCallback(ConfigReceivedCallback cb) {
        configCallback_ = cb;
    }

    /**
     * @brief Установить callback для таймаута связи с MCU
     *
     * Вызывается после HELLO_REQUEST_MAX_ATTEMPTS неудачных попыток получить Hello от MCU.
     */
    void setMcuTimeoutCallback(McuTimeoutCallback cb) {
        mcuTimeoutCallback_ = cb;
    }

    /**
     * @brief Установить callback для получения PIN при привязке устройства
     *
     * Вызывается когда backend выдаёт PIN для claiming.
     * Этот callback ДОПОЛНЯЕТ внутренний (который отправляет PIN на RP2040).
     */
    void setClaimPinCallback(ClaimPinCallback cb) {
        userClaimPinCallback_ = cb;
    }

    /**
     * @brief Проверка получен ли Hello от MCU
     * @return true если MCU на связи
     */
    bool isMcuConnected() const { return helloReceived_; }

    // =========================================================================
    // ДОСТУП К СОСТОЯНИЮ
    // =========================================================================

    /**
     * @brief Проверка онлайн статуса
     * @return true если MQTT подключен
     */
    bool isOnline() const { return cloud_.isOnline(); }

    /**
     * @brief Текущее состояние облака
     */
    cloud::CloudState getCloudState() const { return cloud_.getState(); }

    /**
     * @brief Доступ к identity (read-only)
     */
    const DeviceIdentity& getIdentity() const { return cloud_.getIdentity(); }

    /**
     * @brief Доступ к CloudStateMachine для регистрации дополнительных callbacks
     * @return Указатель на CloudStateMachine
     */
    cloud::CloudStateMachine* getCloudStateMachine() { return &cloud_; }

private:
    // =========================================================================
    // КОМПОНЕНТЫ
    // =========================================================================

    IWifiManager* wifi_;
    IHttpClient* http_;
    ICredentialStore* store_;
    DryerUart::UartBridge* uart_;

    // Cloud компоненты
    cloud::HttpApi api_;
    MqttClient mqtt_;
    cloud::CloudStateMachine cloud_;
    cloud::TelemetryPublisher publisher_;
    cloud::CommandHandler cmdHandler_;

    // =========================================================================
    // СОСТОЯНИЕ
    // =========================================================================

    bool helloReceived_ = false;  // Блокирует публикации до Hello от RP2040
    uint8_t unitsCount_ = 1;       // Количество камер (из Hello)

    // Кэш RFID событий (по одному на ридер, до 4 ридеров)
    // Нужен чтобы сервер узнал о метках если MQTT был оффлайн
    static constexpr uint8_t MAX_RFID_READERS = 4;
    DryerUart::RfidPayload latestRfid_[MAX_RFID_READERS]{};
    bool rfidDirty_[MAX_RFID_READERS] = {false};

    // Config receiver для сборки фрагментов от RP2040
    DryerUart::ConfigReceiver configReceiver_;

    // Callbacks
    ConfigReceivedCallback configCallback_;
    McuTimeoutCallback mcuTimeoutCallback_;
    ClaimPinCallback userClaimPinCallback_;

    // Hello Request retry state
    uint32_t lastHelloRequestMs_ = 0;   // Время последней отправки Hello Request
    uint8_t helloRequestAttempts_ = 0;  // Счётчик попыток
    bool mcuTimeoutNotified_ = false;   // Callback уже вызван

    // Heartbeat state
    uint32_t lastHeartbeatAt_ = 0;
    uint32_t heartbeatErrors_ = 0;

    // =========================================================================
    // ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
    // =========================================================================

    void registerUartHandlers();
    void processHeartbeat();
    void sendHelloRequest();
    void publishCachedData();
    void publishDeviceInfo(const DryerUart::HelloPayload& payload);
    void syncTimeFromBackend(const char* timestamp);

    // Callback для MQTT команд
    void handleMqttCommand(const char* command, JsonObjectConst data);

    // Callback для смены состояния облака
    static void onCloudStateChange(cloud::CloudState oldState,
                                   cloud::CloudState newState,
                                   void* ctx);

    // Callback для PIN (claiming)
    static void onClaimPin(const char* pin, uint32_t expiresInSeconds, void* ctx);

    // Callback для завершения claiming
    static void onClaimComplete(const char* deviceId, void* ctx);

    // Callback когда устройство не привязано (после provision)
    static void onUnclaimed(void* ctx);
};

} // namespace idryer
