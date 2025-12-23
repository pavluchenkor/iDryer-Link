/**
 * @file mqtt_client.h
 * @brief MQTT Client for iDryer Device (C++ wrapper)
 *
 * C++ класс для работы с MQTT брокером, использующий константы из idryer_topics.h
 *
 * Implements MQTT protocol according to:
 * @see docs/mqtt-api-kit/02-api-reference/device-to-backend.md
 * @see docs/mqtt-api-kit/04-esp32/connection.md
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "idryer_topics.h"

// Константы MQTT подключения
#define MQTT_BROKER             "mqtt.idryer.org"
#define MQTT_PORT               8883  // MQTTS (TLS)
#define IDRYER_MQTT_KEEPALIVE   60    // Секунд (переименовано чтобы не конфликтовать с PubSubClient)

// Максимальные размеры
#define MQTT_BUFFER_SIZE    4096  // Для больших JSON payload
#define TOPIC_BUFFER_SIZE   128   // Для топиков

/**
 * @brief Класс MQTT клиента для iDryer устройства
 *
 * Управляет подключением к MQTT брокеру, публикацией топиков
 * и обработкой команд от Backend.
 *
 * Использует константы топиков, QoS и Retained флагов из idryer_topics.h
 */
class MqttClient {
public:
    /**
     * @brief Callback для обработки команд
     * @param command Имя команды ("start", "stop", "get_config")
     * @param data JSON данные команды
     */
    using CommandCallback = std::function<void(const char* command, JsonObjectConst data)>;

    /**
     * @brief Инициализация MQTT клиента
     * @param serialNumber Serial number устройства (username для MQTT)
     * @param token JWT token (password для MQTT)
     */
    void begin(const char* serialNumber, const char* token);

    /**
     * @brief Установить callback для обработки команд
     * @param callback Функция-обработчик
     */
    void setCommandCallback(CommandCallback callback);

    /**
     * @brief Подключиться к MQTT брокеру
     * @return true если подключено успешно
     */
    bool connect();

    /**
     * @brief Проверка состояния подключения
     * @return true если подключено
     */
    bool isConnected();

    /**
     * @brief Основной loop для обработки MQTT сообщений
     */
    void loop();

    // ========================================================================
    // Публикация топиков согласно API
    // @see docs/mqtt-api-kit/02-api-reference/device-to-backend.md
    // Все методы используют константы из idryer_topics.h:
    //   - IDRYER_TOPIC_*      - имена топиков
    //   - IDRYER_QOS_*        - QoS уровни
    //   - IDRYER_RETAINED_*   - Retained флаги
    // ========================================================================

    /**
     * @brief Публикация info топика (версии устройства)
     *
     * Использует:
     *   - Топик: IDRYER_TOPIC_INFO ("info")
     *   - QoS: IDRYER_QOS_INFO (1)
     *   - Retained: IDRYER_RETAINED_INFO (true)
     *
     * JSON формат - см. idryer_topics.h, константа IDRYER_TOPIC_INFO
     */
    bool publishInfo(const char* hwVersion, const char* fwVersion, uint32_t workTimeCounter);

    /**
     * @brief Публикация telemetry топика (каждые 5 сек)
     *
     * Использует:
     *   - Топик: IDRYER_TOPIC_TELEMETRY ("telemetry")
     *   - QoS: IDRYER_QOS_TELEMETRY (0)
     *   - Retained: IDRYER_RETAINED_TELEMETRY (false)
     *
     * JSON формат - см. idryer_topics.h, константа IDRYER_TOPIC_TELEMETRY
     */
    bool publishTelemetry(JsonDocument& json);

    /**
     * @brief Публикация status топика (retained, при изменении)
     *
     * Использует:
     *   - Топик: IDRYER_TOPIC_STATUS ("status")
     *   - QoS: IDRYER_QOS_STATUS (1)
     *   - Retained: IDRYER_RETAINED_STATUS (true)
     *
     * JSON формат - см. idryer_topics.h, константа IDRYER_TOPIC_STATUS
     */
    bool publishStatus(JsonDocument& json);

    /**
     * @brief Публикация weights топика (каждые 10 сек)
     *
     * Использует:
     *   - Топик: IDRYER_TOPIC_WEIGHTS ("weights")
     *   - QoS: IDRYER_QOS_WEIGHTS (1)
     *   - Retained: IDRYER_RETAINED_WEIGHTS (false)
     *
     * JSON формат - см. idryer_topics.h, константа IDRYER_TOPIC_WEIGHTS
     */
    bool publishWeights(JsonDocument& json);

    /**
     * @brief Публикация rfid топика (при событии)
     *
     * Использует:
     *   - Топик: IDRYER_TOPIC_RFID ("rfid")
     *   - QoS: IDRYER_QOS_RFID (1)
     *   - Retained: IDRYER_RETAINED_RFID (true)
     *
     * JSON формат - см. idryer_topics.h, константа IDRYER_TOPIC_RFID
     */
    bool publishRfid(JsonDocument& json);

    /**
     * @brief Публикация events топика (при событии)
     *
     * Использует:
     *   - Топик: IDRYER_TOPIC_EVENTS ("events")
     *   - QoS: IDRYER_QOS_EVENTS (1)
     *   - Retained: IDRYER_RETAINED_EVENTS (false)
     *
     * JSON формат - см. idryer_topics.h, константа IDRYER_TOPIC_EVENTS
     */
    bool publishEvent(JsonDocument& json);

    /**
     * @brief Публикация config топика (по запросу)
     *
     * Использует:
     *   - Топик: IDRYER_TOPIC_CONFIG ("config")
     *   - QoS: IDRYER_QOS_CONFIG (1)
     *   - Retained: IDRYER_RETAINED_CONFIG (false)
     *
     * JSON формат - см. idryer_topics.h, константа IDRYER_TOPIC_CONFIG
     */
    bool publishConfig(JsonDocument& json);

    // ========================================================================
    // Вспомогательные методы
    // ========================================================================

    /**
     * @brief Получить ISO 8601 timestamp (UTC)
     * @param buffer Буфер для записи (минимум 25 символов)
     * @return Указатель на buffer
     *
     * Формат: "2025-12-22T10:00:00Z"
     */
    static char* getIsoTimestamp(char* buffer);

    /**
     * @brief Генерация UUID v4 для sessionId
     * @param buffer Буфер для UUID (минимум 37 символов)
     * @return Указатель на buffer
     *
     * Формат: "a7b3c9d1-e4f5-6789-0abc-def123456789"
     */
    static char* generateUuid(char* buffer);

private:
    WiFiClientSecure wifiClient_;
    PubSubClient mqttClient_;
    CommandCallback commandCallback_;

    char serialNumber_[32];
    char token_[512];
    char clientId_[32];

    // Буферы для топиков
    char topicBuffer_[TOPIC_BUFFER_SIZE];

    /**
     * @brief Callback для входящих MQTT сообщений
     */
    static void mqttCallback(char* topic, byte* payload, unsigned int length);

    /**
     * @brief Обработка входящего сообщения
     */
    void handleMessage(const char* topic, const char* payload, size_t length);

    /**
     * @brief Построить полный топик используя константы из idryer_topics.h
     * @param suffix Суффикс топика (используйте IDRYER_TOPIC_* константы)
     * @return Указатель на topicBuffer_
     */
    const char* makeTopic(const char* suffix);

    /**
     * @brief Публикация JSON документа в топик
     * @param suffix Суффикс топика (используйте IDRYER_TOPIC_* константы)
     * @param json JSON документ
     * @param retained Флаг retained (используйте IDRYER_RETAINED_* константы)
     * @param qos QoS уровень (используйте IDRYER_QOS_* константы)
     * @return true если опубликовано
     */
    bool publishJson(const char* suffix, JsonDocument& json, bool retained = false, int qos = 0);

    // Singleton для callback
    static MqttClient* instance_;
};

#endif // MQTT_CLIENT_H
