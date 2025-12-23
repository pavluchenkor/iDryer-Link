/**
 * @file mqtt_client.cpp
 * @brief MQTT Client Implementation
 */

#include "mqtt_client.h"
#include "root_ca.h"
#include <esp_system.h>
#include <time.h>

// Singleton instance для callback
MqttClient* MqttClient::instance_ = nullptr;

void MqttClient::begin(const char* serialNumber, const char* token) {
    // Сохраняем учетные данные
    strncpy(serialNumber_, serialNumber, sizeof(serialNumber_) - 1);
    strncpy(token_, token, sizeof(token_) - 1);
    strncpy(clientId_, serialNumber, sizeof(clientId_) - 1);

    // Настраиваем TLS
    wifiClient_.setCACert(ROOT_CA_LETSENCRYPT);
    wifiClient_.setTimeout(10);  // 10 секунд

    // Настраиваем MQTT клиент
    mqttClient_.setClient(wifiClient_);
    mqttClient_.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient_.setBufferSize(MQTT_BUFFER_SIZE);
    mqttClient_.setKeepAlive(IDRYER_MQTT_KEEPALIVE);
    mqttClient_.setCallback(MqttClient::mqttCallback);

    // Singleton для callback
    instance_ = this;

#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.printf("[MQTT] Init: broker=%s:%d serial=%s\n",
                        MQTT_BROKER, MQTT_PORT, serialNumber_);
#endif
}

void MqttClient::setCommandCallback(CommandCallback callback) {
    commandCallback_ = callback;
}

bool MqttClient::connect() {
    if (mqttClient_.connected()) {
        return true;
    }

#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.printf("[MQTT] Connecting as %s...\n", clientId_);
#endif

    // Подключаемся: clientId, username, password
    bool connected = mqttClient_.connect(
        clientId_,          // Client ID
        serialNumber_,      // Username
        token_              // Password (JWT)
    );

    if (!connected) {
#ifdef DEBUG_SERIAL
        int state = mqttClient_.state();
        DEBUG_SERIAL.printf("[MQTT] Connection failed: %d\n", state);
#endif
        return false;
    }

#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.println("[MQTT] Connected!");
#endif

    // Подписываемся на команды используя константу IDRYER_TOPIC_CMD_WILDCARD
    const char* cmdTopic = makeTopic(IDRYER_TOPIC_CMD_WILDCARD);
    mqttClient_.subscribe(cmdTopic, IDRYER_QOS_COMMANDS);

#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.printf("[MQTT] Subscribed: %s (QoS %d)\n", cmdTopic, IDRYER_QOS_COMMANDS);
#endif

    return true;
}

bool MqttClient::isConnected() {
    return mqttClient_.connected();
}

void MqttClient::loop() {
    if (!mqttClient_.connected()) {
        // Auto-reconnect
        connect();
    }
    mqttClient_.loop();
}

// ============================================================================
// Публикация топиков (используя константы из idryer_topics.h)
// ============================================================================

bool MqttClient::publishInfo(const char* hwVersion, const char* fwVersion, uint32_t workTimeCounter) {
    StaticJsonDocument<256> doc;

    // Формируем JSON согласно API (см. idryer_topics.h)
    doc["hardwareVersion"] = hwVersion;
    doc["firmwareVersion"] = fwVersion;
    doc["workTimeCounter"] = workTimeCounter;

    char timestamp[32];
    doc["timestamp"] = getIsoTimestamp(timestamp);

    // Используем константы: IDRYER_TOPIC_INFO, IDRYER_RETAINED_INFO, IDRYER_QOS_INFO
    return publishJson(IDRYER_TOPIC_INFO, doc, IDRYER_RETAINED_INFO, IDRYER_QOS_INFO);
}

bool MqttClient::publishTelemetry(JsonDocument& json) {
    // Используем константы: IDRYER_TOPIC_TELEMETRY, IDRYER_RETAINED_TELEMETRY, IDRYER_QOS_TELEMETRY
    return publishJson(IDRYER_TOPIC_TELEMETRY, json, IDRYER_RETAINED_TELEMETRY, IDRYER_QOS_TELEMETRY);
}

bool MqttClient::publishStatus(JsonDocument& json) {
    // Используем константы: IDRYER_TOPIC_STATUS, IDRYER_RETAINED_STATUS, IDRYER_QOS_STATUS
    return publishJson(IDRYER_TOPIC_STATUS, json, IDRYER_RETAINED_STATUS, IDRYER_QOS_STATUS);
}

bool MqttClient::publishWeights(JsonDocument& json) {
    // Используем константы: IDRYER_TOPIC_WEIGHTS, IDRYER_RETAINED_WEIGHTS, IDRYER_QOS_WEIGHTS
    return publishJson(IDRYER_TOPIC_WEIGHTS, json, IDRYER_RETAINED_WEIGHTS, IDRYER_QOS_WEIGHTS);
}

bool MqttClient::publishRfid(JsonDocument& json) {
    // Используем константы: IDRYER_TOPIC_RFID, IDRYER_RETAINED_RFID, IDRYER_QOS_RFID
    return publishJson(IDRYER_TOPIC_RFID, json, IDRYER_RETAINED_RFID, IDRYER_QOS_RFID);
}

bool MqttClient::publishEvent(JsonDocument& json) {
    // Используем константы: IDRYER_TOPIC_EVENTS, IDRYER_RETAINED_EVENTS, IDRYER_QOS_EVENTS
    return publishJson(IDRYER_TOPIC_EVENTS, json, IDRYER_RETAINED_EVENTS, IDRYER_QOS_EVENTS);
}

bool MqttClient::publishConfig(JsonDocument& json) {
    // Используем константы: IDRYER_TOPIC_CONFIG, IDRYER_RETAINED_CONFIG, IDRYER_QOS_CONFIG
    return publishJson(IDRYER_TOPIC_CONFIG, json, IDRYER_RETAINED_CONFIG, IDRYER_QOS_CONFIG);
}

// ============================================================================
// Обработка команд
// ============================================================================

void MqttClient::mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (instance_) {
        char* payloadStr = (char*)malloc(length + 1);
        if (payloadStr) {
            memcpy(payloadStr, payload, length);
            payloadStr[length] = '\0';
            instance_->handleMessage(topic, payloadStr, length);
            free(payloadStr);
        }
    }
}

void MqttClient::handleMessage(const char* topic, const char* payload, size_t length) {
#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.printf("[MQTT] Message: %s (len=%u)\n", topic, length);
#endif

    // Парсим топик: idryer/{serial}/commands/{command}
    // Используем константы для проверки префикса команд
    const char* cmdPrefix = "/commands/";
    const char* cmdStart = strstr(topic, cmdPrefix);
    if (!cmdStart) {
        return;
    }
    cmdStart += strlen(cmdPrefix);  // Пропускаем "/commands/"

#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.printf("[MQTT] Command: %s\n", cmdStart);
#endif

    // Парсим JSON payload команды
    // Формат команд см. в idryer_topics.h (IDRYER_TOPIC_CMD_*)
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
#ifdef DEBUG_SERIAL
        DEBUG_SERIAL.printf("[MQTT] JSON parse error: %s\n", err.c_str());
#endif
        return;
    }

    // Вызываем callback для обработки команды
    if (commandCallback_) {
        commandCallback_(cmdStart, doc.as<JsonObjectConst>());
    }
}

// ============================================================================
// Вспомогательные методы
// ============================================================================

const char* MqttClient::makeTopic(const char* suffix) {
    // Используем функцию из idryer_topics.h
    idryer_make_topic(topicBuffer_, sizeof(topicBuffer_), serialNumber_, suffix);
    return topicBuffer_;
}

bool MqttClient::publishJson(const char* suffix, JsonDocument& json, bool retained, int qos) {
    if (!mqttClient_.connected()) {
        return false;
    }

    // Добавляем timestamp если его нет
    // Все топики ДОЛЖНЫ содержать timestamp в формате ISO 8601 (UTC)
    if (!json.containsKey("timestamp")) {
        char timestamp[32];
        json["timestamp"] = getIsoTimestamp(timestamp);
    }

    // Сериализуем JSON в строку
    size_t jsonSize = measureJson(json);
    char* jsonBuffer = (char*)malloc(jsonSize + 1);
    if (!jsonBuffer) {
#ifdef DEBUG_SERIAL
        DEBUG_SERIAL.println("[MQTT] Out of memory!");
#endif
        return false;
    }

    size_t written = serializeJson(json, jsonBuffer, jsonSize + 1);

    // Формируем полный топик используя константы из idryer_topics.h
    const char* topic = makeTopic(suffix);

#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.printf("[MQTT] Publish: %s (len=%u qos=%d retained=%d)\n",
                        topic, written, qos, retained);
    DEBUG_SERIAL.printf("[MQTT] Payload: %s\n", jsonBuffer);
#endif

    // Публикуем в MQTT брокер
    // retained = true означает что MQTT брокер сохранит последнее сообщение
    // и отправит его новым подписчикам
    bool success = mqttClient_.publish(topic, jsonBuffer, retained);
    free(jsonBuffer);

    if (!success) {
#ifdef DEBUG_SERIAL
        DEBUG_SERIAL.println("[MQTT] Publish failed!");
#endif
    }

    return success;
}

// ============================================================================
// Утилиты: Timestamp и UUID
// ============================================================================

char* MqttClient::getIsoTimestamp(char* buffer) {
    // Получаем текущее время
    time_t now = time(nullptr);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);

    // Форматируем в ISO 8601 (UTC)
    strftime(buffer, 32, "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return buffer;
}

char* MqttClient::generateUuid(char* buffer) {
    // UUID v4 формат: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    // где x = random hex, y = [8,9,a,b]
    //
    // Используется для sessionId согласно API (см. idryer_topics.h, IDRYER_TOPIC_STATUS)
    // - Генерируется устройством при старте DRYING/STORAGE/PROFILE
    // - Передается в status топике
    // - Backend использует для создания/обновления dryingSessionNew
    //
    // Пример: "a7b3c9d1-e4f5-6789-0abc-def123456789"

    uint32_t r1 = esp_random();
    uint32_t r2 = esp_random();
    uint32_t r3 = esp_random();
    uint32_t r4 = esp_random();

    // Формируем UUID v4 (RFC 4122)
    snprintf(buffer, 37,
             "%08x-%04x-4%03x-%04x-%012llx",
             r1,
             (r2 >> 16) & 0xFFFF,
             r2 & 0x0FFF,
             ((r3 >> 16) & 0x3FFF) | 0x8000,  // y = [8,9,a,b]
             ((uint64_t)(r3 & 0xFFFF) << 32) | r4);

    return buffer;
}
