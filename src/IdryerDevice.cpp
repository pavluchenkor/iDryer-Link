/**
 * @file IdryerDevice.cpp
 * @brief Реализация фасада устройства iDryer Link
 *
 * Это APPLICATION CODE, специфичный для iDryer Link.
 * IdryerDevice использует cloud компоненты и делегирует им всю работу.
 * Сам класс только связывает компоненты и обрабатывает UART события.
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "IdryerDevice.h"
#include "WsServer.h"
#include <hal/hal_types.h>
#include <time.h>
#include <sys/time.h>
#include <ArduinoJson.h>
#include <menu_commands.h>
#include "version.h"

// API URL по умолчанию
#ifndef IDRYER_API_BASE
#define IDRYER_API_BASE "https://api.idryer.io"
#endif

namespace idryer
{

    // =============================================================================
    // ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
    // =============================================================================

    /**
     * @brief Преобразует "A.B.C.D" в 32-битное поле для `HelloAckPayload::ipAddress`.
     * @note Возвращаемый формат — A | (B<<8) | (C<<16) | (D<<24).
     * Он должен совпадать с распаковкой на стороне RP2040.
     */
    static uint32_t parseIpAddress(const char *ipStr)
    {
        if (!ipStr || !ipStr[0])
        {
            return 0;
        }

        // Парсим "A.B.C.D" вручную
        uint8_t octets[4] = {0};
        int octetIndex = 0;
        int value = 0;

        for (const char *p = ipStr; *p && octetIndex < 4; ++p)
        {
            if (*p >= '0' && *p <= '9')
            {
                value = value * 10 + (*p - '0');
                if (value > 255)
                    return 0; // Невалидный октет
            }
            else if (*p == '.')
            {
                octets[octetIndex++] = (uint8_t)value;
                value = 0;
            }
            else
            {
                return 0; // Невалидный символ
            }
        }

        // Последний октет
        if (octetIndex == 3 && value <= 255)
        {
            octets[octetIndex] = (uint8_t)value;
        }
        else
        {
            return 0;
        }

        // Порядок байт должен оставаться совместимым с текущим UART контрактом.
        return (uint32_t)octets[0] |
               ((uint32_t)octets[1] << 8) |
               ((uint32_t)octets[2] << 16) |
               ((uint32_t)octets[3] << 24);
    }

    // =============================================================================
    // КОНСТРУКТОР
    // =============================================================================

    IdryerDevice::IdryerDevice(IWifiManager *wifi,
                               IHttpClient *http,
                               ICredentialStore *store,
                               DryerUart::UartBridge *uart,
                               const char *apiBaseUrl)
        : wifi_(wifi), http_(http), store_(store), uart_(uart), api_(http, apiBaseUrl ? apiBaseUrl : IDRYER_API_BASE), cloud_(wifi, store, &api_, &mqtt_), publisher_(&mqtt_), cmdHandler_(uart)
    {
    }

    // =============================================================================
    // ИНИЦИАЛИЗАЦИЯ
    // =============================================================================

    void IdryerDevice::begin()
    {
        HAL_LOG_INFO("DEVICE", "Initializing...");

        // Регистрируем UART обработчики
        registerUartHandlers();

        // Настраиваем callbacks для CloudStateMachine
        cloud_.setStateChangeCallback(onCloudStateChange, this);
        cloud_.setClaimPinCallback(onClaimPin, this);
        cloud_.setClaimCompleteCallback(onClaimComplete, this);
        cloud_.setUnclaimedCallback(onUnclaimed, this);

        // Настраиваем callback для MQTT команд
        mqtt_.setCommandCallback([this](const char *cmd, JsonObjectConst data)
                                 { handleMqttCommand(cmd, data); });

        // Настраиваем callback для синхронизации времени
        cmdHandler_.setTimeSyncCallback([](const char *ts, void *ctx)
                                        { static_cast<IdryerDevice *>(ctx)->syncTimeFromBackend(ts); }, this);

        // Инициализируем облачную машину состояний
        cloud_.begin();

        HAL_LOG_INFO("DEVICE", "Initialized, serial=%s",
                     cloud_.getIdentity().serialNumber);

        // Отправляем первый Hello Request к MCU
        sendHelloRequest();
    }

    // =============================================================================
    // РЕГИСТРАЦИЯ UART ОБРАБОТЧИКОВ
    // =============================================================================

    void IdryerDevice::registerUartHandlers()
    {
        uart_->setHelloHandler([this](const DryerUart::HelloPayload &p, const DryerUart::FrameHeader &h)
                               { handleRpHello(p, h); });

        uart_->setTelemetryHandler([this](const DryerUart::TelemetryPayload &p, const DryerUart::FrameHeader &h)
                                   { handleTelemetry(p, h); });

        uart_->setCommandAckHandler([this](const DryerUart::AckPayload &p, const DryerUart::FrameHeader &h)
                                    {
            handleCommandAck(p, h);
            if (p.status != DryerUart::ErrorCode::None) heartbeatErrors_++; });

        uart_->setConfigAckHandler([this](const DryerUart::AckPayload &p, const DryerUart::FrameHeader &h)
                                   {
            handleConfigAck(p, h);
            if (p.status != DryerUart::ErrorCode::None) heartbeatErrors_++; });

        uart_->setConfigPushChunkHandler([this](const DryerUart::ConfigChunkPayload &p, uint8_t dataLen, const DryerUart::FrameHeader &h)
                                         { handleConfigPushChunk(p, dataLen, h); });

        uart_->setHeartbeatHandler([](const DryerUart::HeartbeatPayload &p, const DryerUart::FrameHeader &h)
                                   { HAL_LOG_DEBUG("UART", "Heartbeat: uptime=%d rssi=%d errors=%d",
                                                   p.uptimeSeconds, p.wifiRssiDbm, p.errorsSinceBoot); });

        uart_->setErrorHandler([this](const DryerUart::ErrorPayload &p, bool remote)
                               {
            handleUartError(p, remote);
            heartbeatErrors_++; });

        uart_->setLogHandler([this](const uint8_t *payload, uint8_t length)
                             {
            if (length < sizeof(DryerUart::LogPayload)) {
                HAL_LOG_WARN("UART", "Log message too short: %d bytes", length);
                return;
            }
            handleLog(reinterpret_cast<const DryerUart::LogPayload *>(payload)); });

        uart_->setStatusHandler([this](const DryerUart::StatusPayload &p, const DryerUart::FrameHeader &h)
                                { handleStatus(p, h); });

        uart_->setWeightsHandler([this](const DryerUart::WeightsPayload &p, const DryerUart::FrameHeader &h)
                                 { handleWeights(p, h); });

        uart_->setRfidHandler([this](const DryerUart::RfidPayload &p, const DryerUart::FrameHeader &h)
                              { handleRfidEvent(p, h); });

        uart_->setClaimStartHandler([this](const DryerUart::FrameHeader &h)
                                    {
            HAL_LOG_INFO("UART", "ClaimStart from RP2040 (seq=%d)", h.sequence);
            requestClaimProcess(); });

        // WebSocket Local Access обработчики
        uart_->setWsEnableHandler([this](const DryerUart::WsEnablePayload &p, const DryerUart::FrameHeader &h)
                                  {
            HAL_LOG_INFO("UART", "WsEnable: enable=%d", p.enable);
            if (wsServer_) {
                if (p.enable) {
                    wsServer_->begin(cloud_.getIdentity().serialNumber,
                                     cloud_.getIdentity().token);
                } else {
                    wsServer_->stop();
                }
                uart_->sendWsStatus(wsServer_->getStatus());
            } });

        uart_->setWsStatusRequestHandler([this](const DryerUart::FrameHeader &h)
                                         {
            HAL_LOG_INFO("UART", "WsStatusRequest");
            if (wsServer_) {
                uart_->sendWsStatus(wsServer_->getStatus());
            } else {
                // WS не инициализирован — отправляем Disabled
                DryerUart::WsStatusPayload status{};
                status.state = DryerUart::WsState::Disabled;
                status.pairedCount = 0;
                status.maxClients = 1;
                uart_->sendWsStatus(status);
            } });

        HAL_LOG_INFO("DEVICE", "UART handlers registered");
    }

    // =============================================================================
    // ГЛАВНЫЙ ЦИКЛ
    // =============================================================================

    void IdryerDevice::loop()
    {
        // Обрабатываем UART (входящие данные, retry)
        uart_->loop();

        // Обрабатываем облачную машину состояний
        cloud_.loop();

        // Обрабатываем WS сервер (если включён)
        if (wsServer_)
            wsServer_->loop();

        // Heartbeat каждые 5 секунд
        processHeartbeat();

        // Hello Request retry логика
        if (!helloReceived_)
        {
            uint32_t now = HAL_MILLIS();
            if (now - lastHelloRequestMs_ >= DryerUart::HELLO_REQUEST_INTERVAL_MS)
            {
                if (helloRequestAttempts_ < DryerUart::HELLO_REQUEST_MAX_ATTEMPTS)
                {
                    sendHelloRequest();
                }
                else if (!mcuTimeoutNotified_)
                {
                    // Все попытки исчерпаны — вызываем callback
                    mcuTimeoutNotified_ = true;
                    HAL_LOG_WARN("DEVICE", "MCU timeout after %d attempts", helloRequestAttempts_);
                    if (mcuTimeoutCallback_)
                    {
                        mcuTimeoutCallback_();
                    }
                }
            }
        }

        // Публикуем закэшированные данные если онлайн
        if (cloud_.isOnline() && helloReceived_)
        {
            publishCachedData();
        }
    }

    // =============================================================================
    // CLAIMING
    // =============================================================================

    bool IdryerDevice::requestClaimProcess()
    {
        return cloud_.requestClaim();
    }

    // =============================================================================
    // UART ОБРАБОТЧИКИ
    // =============================================================================

    void IdryerDevice::handleRpHello(const DryerUart::HelloPayload &payload,
                                     const DryerUart::FrameHeader &header)
    {
        HAL_LOG_INFO("DEVICE", "Hello from RP2040: units=%d", payload.unitsCount);

        if (wifi_->isConnected() && payload.mcuSerial[0] != '\0')
        {
            Serial.printf("RP2040_SERIAL:%s\n", payload.mcuSerial);
            Serial.flush();
        }

        // Кэшируем для повторной публикации при переподключении MQTT
        lastHello_ = payload;
        lastHelloValid_ = true;

        // Обновляем состояние
        unitsCount_ = payload.unitsCount;

        // Сбрасываем retry state
        helloRequestAttempts_ = 0;
        mcuTimeoutNotified_ = false;

        // Первый Hello (или reconnect) - разблокируем публикации
        if (!helloReceived_)
        {
            helloReceived_ = true;
            HAL_LOG_INFO("DEVICE", "MCU connected, publications unlocked");
        }

        // Отправляем HelloAck с текущим IP и SSID
        {
            DryerUart::HelloAckPayload ack{};

            char ipStr[16];
            wifi_->getLocalIP(ipStr, sizeof(ipStr));
            ack.ipAddress = parseIpAddress(ipStr);

            wifi_->getSSID(ack.ssid, sizeof(ack.ssid));

            uart_->sendHelloAck(ack);
            HAL_LOG_INFO("DEVICE", "HelloAck sent (IP=%s, SSID=%s)", ipStr, ack.ssid);
        }

        // Переключаем MQTT топик на mcuSerial (RP2040 identity) если устройство уже привязано
        // и серийник ещё не обновлён (первый boot после claming)
        if (payload.mcuSerial[0] != '\0' && cloud_.getIdentity().hasDeviceId())
        {
            if (strcmp(cloud_.getIdentity().serialNumber, payload.mcuSerial) != 0)
            {
                HAL_LOG_INFO("DEVICE", "Switching MQTT topic: %s -> %s (mcuSerial)",
                             cloud_.getIdentity().serialNumber, payload.mcuSerial);
                cloud_.switchSerialNumber(payload.mcuSerial);
            }
        }

        // Всегда публикуем info при получении Hello (если онлайн)
        if (cloud_.isOnline())
        {
            // Сбрасываем флаг чтобы info публиковался при каждом Hello
            publisher_.resetInfoPublished();
            publishDeviceInfo(payload);
        }
    }

    void IdryerDevice::publishDeviceInfo(const DryerUart::HelloPayload &payload)
    {
        // Извлекаем версии из payload
        char hwVersion[16];
        strncpy(hwVersion, payload.hardwareVersion, sizeof(hwVersion) - 1);
        hwVersion[sizeof(hwVersion) - 1] = '\0';

        char fwVersion[16];
        snprintf(fwVersion, sizeof(fwVersion), "%d.%d.%d",
                 (payload.firmwareVersion >> 16) & 0xFF,
                 (payload.firmwareVersion >> 8) & 0xFF,
                 payload.firmwareVersion & 0xFF);

        publisher_.publishInfo(unitsCount_, payload.units, hwVersion, fwVersion,
                               payload.workTimeCounter, payload.mcuSerial);

        // WS параллельная публикация info
        if (wsServer_)
            wsServer_->sendInfo(unitsCount_, payload.units, hwVersion, fwVersion,
                                payload.workTimeCounter, payload.mcuSerial);

        HAL_LOG_INFO("DEVICE", "Published info: units=%d hw=%s fw=%s",
                     unitsCount_, hwVersion, fwVersion);
    }

    void IdryerDevice::handleTelemetry(const DryerUart::TelemetryPayload &payload,
                                       const DryerUart::FrameHeader &header)
    {
        HAL_LOG_DEBUG("DEVICE", "Telemetry: count=%d", payload.count);

        // Публикуем сразу, без кэширования (данные придут снова через 5 сек)
        if (cloud_.isOnline() && helloReceived_)
        {
            publisher_.publishTelemetry(payload);
        }

        // WS параллельная публикация
        if (wsServer_)
            wsServer_->sendTelemetry(payload);
    }

    void IdryerDevice::handleStatus(const DryerUart::StatusPayload &payload,
                                    const DryerUart::FrameHeader &header)
    {
        HAL_LOG_DEBUG("DEVICE", "Status: uptime=%d count=%d",
                      payload.uptime, payload.count);

        // Публикуем сразу (retained топик)
        if (cloud_.isOnline() && helloReceived_)
        {
            publisher_.publishStatus(payload);
        }

        // WS параллельная публикация
        if (wsServer_)
            wsServer_->sendStatus(payload);
    }

    void IdryerDevice::handleWeights(const DryerUart::WeightsPayload &payload,
                                     const DryerUart::FrameHeader &header)
    {
        HAL_LOG_DEBUG("DEVICE", "Weights: count=%d", payload.count);

        // Публикуем сразу, без кэширования
        if (cloud_.isOnline() && helloReceived_)
        {
            publisher_.publishWeights(payload);
        }

        // WS параллельная публикация
        if (wsServer_)
            wsServer_->sendWeights(payload);
    }

    void IdryerDevice::handleRfidEvent(const DryerUart::RfidPayload &payload,
                                       const DryerUart::FrameHeader &header)
    {
        HAL_LOG_INFO("DEVICE", "RFID: event=%d reader=%d unit=%d tag=%s",
                     static_cast<int>(payload.event), payload.readerId,
                     payload.unitId, payload.tag);

        // Кэшируем RFID событие (важно для сервера знать какая метка загружена)
        uint8_t readerId = payload.readerId;
        if (readerId < MAX_RFID_READERS)
        {
            latestRfid_[readerId] = payload;
            rfidDirty_[readerId] = true;
        }

        // Пытаемся опубликовать сразу
        if (cloud_.isOnline() && helloReceived_)
        {
            publisher_.publishRfid(payload);
            if (readerId < MAX_RFID_READERS)
            {
                rfidDirty_[readerId] = false;
            }
        }

        // WS параллельная публикация
        if (wsServer_)
            wsServer_->sendRfid(payload);
    }

    void IdryerDevice::handleCommandAck(const DryerUart::AckPayload &payload,
                                        const DryerUart::FrameHeader &header)
    {
        HAL_LOG_DEBUG("DEVICE", "CommandAck: seq=%d status=%d",
                      payload.ackSequence, static_cast<int>(payload.status));
        // TODO: публиковать в events топик
    }

    void IdryerDevice::handleConfigAck(const DryerUart::AckPayload &payload,
                                       const DryerUart::FrameHeader &header)
    {
        HAL_LOG_DEBUG("DEVICE", "ConfigAck: seq=%d status=%d",
                      payload.ackSequence, static_cast<int>(payload.status));
        // TODO: публиковать в events топик
    }

    void IdryerDevice::handleUartError(const DryerUart::ErrorPayload &payload,
                                       bool remote)
    {
        HAL_LOG_ERROR("DEVICE", "UART Error: code=%d seq=%d remote=%d",
                      static_cast<int>(payload.code), payload.lastSequence, remote);

        // Публикуем ошибку протокола в events топик
        if (cloud_.isOnline())
        {
            StaticJsonDocument<256> doc;
            doc["severity"] = "error";
            doc["source"] = "UART";
            doc["event"] = remote ? "PROTOCOL_ERROR_REMOTE" : "PROTOCOL_ERROR_LOCAL";

            char msg[64];
            snprintf(msg, sizeof(msg), "UART protocol error: code=%d seq=%d",
                     static_cast<int>(payload.code), payload.lastSequence);
            doc["message"] = msg;

            char timestamp[32];
            doc["timestamp"] = MqttClient::getIsoTimestamp(timestamp);

            if (mqtt_.publishEvent(doc))
            {
                HAL_LOG_INFO("DEVICE", "Published UART error event");
            }
        }
    }

    void IdryerDevice::handleLog(const DryerUart::LogPayload *log)
    {
        if (!log)
        {
            return;
        }

        // Логируем локально
        HAL_LOG_INFO("MCU_LOG", "[%s] %s: %s - %s (U%d)",
                     log->severity, log->source, log->event, log->message, log->unitId + 1);

        // Публикуем в events топик если онлайн
        if (cloud_.isOnline())
        {
            StaticJsonDocument<512> doc;

            // Копируем строки с null-termination
            char severity[11] = {0};
            char source[21] = {0};
            char event[33] = {0};
            char message[101] = {0};

            strncpy(severity, log->severity, sizeof(log->severity));
            strncpy(source, log->source, sizeof(log->source));
            strncpy(event, log->event, sizeof(log->event));
            strncpy(message, log->message, sizeof(log->message));

            doc["severity"] = severity;
            doc["source"] = source;
            doc["event"] = event;
            doc["message"] = message;

            // unitId в формате "U1", "U2", ...
            char unitStr[4];
            snprintf(unitStr, sizeof(unitStr), "U%d", log->unitId + 1);
            doc["unitId"] = unitStr;

            char timestamp[32];
            doc["timestamp"] = MqttClient::getIsoTimestamp(timestamp);

            if (mqtt_.publishEvent(doc))
            {
                HAL_LOG_DEBUG("DEVICE", "Published log event from MCU");
            }
            else
            {
                HAL_LOG_WARN("DEVICE", "Failed to publish log event");
            }
        }
    }

    void IdryerDevice::handleConfigPushChunk(const DryerUart::ConfigChunkPayload &payload,
                                             uint8_t dataLen,
                                             const DryerUart::FrameHeader &header)
    {
        HAL_LOG_DEBUG("DEVICE", "ConfigPushChunk: transferId=%d chunk=%d dataLen=%d flags=0x%02X",
                      payload.header.transferId, payload.header.chunkIndex, dataLen, header.flags);

        auto result = configReceiver_.processFragment(payload, dataLen, header.flags);

        switch (result)
        {
        case DryerUart::ConfigFragmentResult::Ok:
            HAL_LOG_DEBUG("DEVICE", "Config chunk %d received, waiting for more...",
                          payload.header.chunkIndex);
            break;

        case DryerUart::ConfigFragmentResult::Complete:
        {
            const char *json = configReceiver_.getJson();
            uint16_t jsonLen = configReceiver_.getJsonLength();

            // Определяем тип конфига:
            // Full: {"rev":N,"full":true,"vals":{...}}
            // Delta: {"rev":N,"vals":{...}} (без full)
            bool isDelta = (strstr(json, "\"full\"") == nullptr);

            HAL_LOG_INFO("DEVICE", "========== CONFIG FROM MCU ==========");
            HAL_LOG_INFO("DEVICE", "Type: %s, Size: %d bytes", isDelta ? "DELTA" : "FULL", jsonLen);
            HAL_LOG_INFO("DEVICE", "Received JSON:");
            HAL_LOG_INFO("DEVICE", "%.*s", jsonLen, json);

            // Вызываем callback для парсинга в menu_cache
            if (configCallback_)
            {
                configCallback_(json, jsonLen, isDelta);
            }

            // Для delta: преобразуем "vals" → "d" для MQTT формата
            const char *publishJson = json;
            uint16_t publishLen = jsonLen;
            static char deltaJsonBuf[1024];

            if (isDelta)
            {
                // Парсим UART формат {"rev":N,"vals":{...}}
                // Преобразуем в MQTT формат {"rev":N,"d":{...}}
                StaticJsonDocument<1024> doc;
                if (deserializeJson(doc, json, jsonLen) == DeserializationError::Ok)
                {
                    // Переименовываем "vals" в "d"
                    doc["d"] = doc["vals"];
                    doc.remove("vals");
                    publishLen = serializeJson(doc, deltaJsonBuf, sizeof(deltaJsonBuf));
                    publishJson = deltaJsonBuf;
                    HAL_LOG_INFO("DEVICE", "Converted vals->d: %d bytes", publishLen);
                    HAL_LOG_INFO("DEVICE", "Publishing to MQTT (delta):");
                    HAL_LOG_INFO("DEVICE", "%.*s", publishLen, publishJson);
                }
            }
            else
            {
                HAL_LOG_INFO("DEVICE", "Publishing to MQTT (full):");
                HAL_LOG_INFO("DEVICE", "%.*s", publishLen, publishJson);
            }

            // WS параллельная публикация конфига
            if (wsServer_)
                wsServer_->sendConfig(publishJson, publishLen, isDelta);

            // Публикуем в MQTT
            bool published = publishConfig(publishJson, publishLen, isDelta);
            if (isDelta)
            {
                HAL_LOG_INFO("DEVICE", "\033[33mPublished to config/delta: %s\033[0m",
                             published ? "OK" : "FAILED");
            }
            else
            {
                HAL_LOG_INFO("DEVICE", "Published to config: %s",
                             published ? "OK" : "FAILED");
            }

            configReceiver_.reset();
            break;
        }

        case DryerUart::ConfigFragmentResult::ErrorSequence:
            HAL_LOG_ERROR("DEVICE", "Config chunk sequence error: expected %d",
                          payload.header.chunkIndex);
            configReceiver_.reset();
            break;

        case DryerUart::ConfigFragmentResult::ErrorOverflow:
            HAL_LOG_ERROR("DEVICE", "Config buffer overflow");
            configReceiver_.reset();
            break;

        case DryerUart::ConfigFragmentResult::ErrorTransferId:
            HAL_LOG_ERROR("DEVICE", "Config transferId mismatch");
            configReceiver_.reset();
            break;
        }
    }

    bool IdryerDevice::publishConfig(const char *json, uint16_t length, bool isDelta)
    {
        if (!cloud_.isOnline())
        {
            HAL_LOG_WARN("DEVICE", "Cannot publish config: offline");
            return false;
        }

        const char *topicName = isDelta ? "config/delta" : "config";

        if (isDelta)
        {
            // Delta публикуется как есть в config/delta
            bool success = mqtt_.publishConfigDelta(json, length);
            if (success)
            {
                HAL_LOG_INFO("DEVICE", "Published %s (%d bytes)", topicName, length);
            }
            else
            {
                HAL_LOG_ERROR("DEVICE", "Failed to publish %s", topicName);
            }
            return success;
        }

        // Полный конфиг: формируем JSON с названиями из menu_meta
        static char fullJsonBuf[MENU_FULL_JSON_BUF_SIZE];
        const char *publishJson = json;
        uint16_t publishLen = length;

        size_t fullLen = menu_buildFullJson(fullJsonBuf, sizeof(fullJsonBuf));
        if (fullLen > 0)
        {
            publishJson = fullJsonBuf;
            publishLen = (uint16_t)fullLen;
            HAL_LOG_INFO("DEVICE", "Built full config with names: %d bytes", publishLen);
            HAL_LOG_INFO("DEVICE", "Full config JSON (first 1000 chars):");
            HAL_LOG_INFO("DEVICE", "%.*s", publishLen > 1000 ? 1000 : publishLen, publishJson);
            if (publishLen > 1000)
            {
                HAL_LOG_INFO("DEVICE", "... (middle part skipped)");
                HAL_LOG_INFO("DEVICE", "Full config JSON (last 500 chars):");
                int startPos = publishLen > 500 ? publishLen - 500 : 0;
                HAL_LOG_INFO("DEVICE", "%.*s", 500, publishJson + startPos);
            }
        }
        else
        {
            HAL_LOG_WARN("DEVICE", "Failed to build full JSON, using raw");
        }

        // Публикуем с автоматической фрагментацией
        uint16_t chunks = mqtt_.publishConfigRaw(publishJson, publishLen);

        if (chunks > 0)
        {
            HAL_LOG_INFO("DEVICE", "Published %s (%d bytes, %d chunks)",
                         topicName, publishLen, chunks);
            return true;
        }

        HAL_LOG_ERROR("DEVICE", "Failed to publish %s", topicName);
        return false;
    }

    // =============================================================================
    // MQTT КОМАНДЫ
    // =============================================================================

    void IdryerDevice::handleMqttCommand(const char *command, JsonObjectConst data)
    {
        // Делегируем в CommandHandler
        cmdHandler_.handleMqttCommand(command, data);
    }

    void IdryerDevice::handleExternalCommand(const char *command, JsonObjectConst data)
    {
        // Тот же обработчик что и для MQTT
        cmdHandler_.handleMqttCommand(command, data);
    }

    // =============================================================================
    // ПУБЛИКАЦИЯ ДАННЫХ
    // =============================================================================

    void IdryerDevice::publishCachedData()
    {
        // Публикуем пропущенные RFID события (если MQTT был оффлайн)
        for (uint8_t i = 0; i < MAX_RFID_READERS; i++)
        {
            if (rfidDirty_[i])
            {
                if (publisher_.publishRfid(latestRfid_[i]))
                {
                    rfidDirty_[i] = false;
                    HAL_LOG_INFO("DEVICE", "Published cached RFID: reader=%d", i);
                }
            }
        }
    }

    void IdryerDevice::processHeartbeat()
    {
        uint32_t now = HAL_MILLIS();
        if (now - lastHeartbeatAt_ < DryerUart::HEARTBEAT_INTERVAL_MS)
        {
            return;
        }

        DryerUart::HeartbeatPayload payload{};
        payload.uptimeSeconds = now / 1000;
        payload.wifiRssiDbm = wifi_->getRSSI();
        payload.errorsSinceBoot = heartbeatErrors_;
        payload.cloudState = static_cast<uint8_t>(cloud_.getState());
        uart_->sendHeartbeat(payload);

        lastHeartbeatAt_ = now;
    }

    void IdryerDevice::sendHelloRequest()
    {
        // Формируем Hello с role=HelloRequest (0xFF)
        DryerUart::HelloPayload payload{};
        payload.role = DryerUart::Role::HelloRequest;
        payload.firmwareVersion = VERSION_NUMBER;
        // Остальные поля нулевые — MCU их игнорирует

        uart_->sendHello(payload, false); // ACK не требуется

        lastHelloRequestMs_ = HAL_MILLIS();
        helloRequestAttempts_++;

        HAL_LOG_INFO("DEVICE", "Hello Request sent (attempt %d/%d)",
                     helloRequestAttempts_, DryerUart::HELLO_REQUEST_MAX_ATTEMPTS);
    }

    // =============================================================================
    // CALLBACKS
    // =============================================================================

    void IdryerDevice::onCloudStateChange(cloud::CloudState oldState,
                                          cloud::CloudState newState,
                                          void *ctx)
    {
        auto *self = static_cast<IdryerDevice *>(ctx);

        HAL_LOG_INFO("DEVICE", "Cloud: %s -> %s",
                     cloud::cloudStateToString(oldState),
                     cloud::cloudStateToString(newState));

        // При готовности WiFi (Ready) отправляем HelloAck с IP и SSID
        if (newState == cloud::CloudState::Ready && self->helloReceived_)
        {
            DryerUart::HelloAckPayload ack{};

            // Получаем IP и конвертируем в uint32_t
            char ipStr[16];
            self->wifi_->getLocalIP(ipStr, sizeof(ipStr));
            ack.ipAddress = parseIpAddress(ipStr);

            // Получаем SSID
            self->wifi_->getSSID(ack.ssid, sizeof(ack.ssid));

            self->uart_->sendHelloAck(ack);
            HAL_LOG_INFO("DEVICE", "Sent HelloAck with IP=%s", ipStr);
        }

        // При подключении к MQTT - публикуем info если Hello уже был получен
        if (newState == cloud::CloudState::Online && self->lastHelloValid_)
        {
            self->publisher_.resetInfoPublished();
            self->publishDeviceInfo(self->lastHello_);
            HAL_LOG_INFO("DEVICE", "Re-published info after MQTT reconnect (mcuSerial=%s)",
                         self->lastHello_.mcuSerial);
        }
    }

    void IdryerDevice::onClaimPin(const char *pin, uint32_t expiresInSeconds, void *ctx)
    {
        auto *self = static_cast<IdryerDevice *>(ctx);

        HAL_LOG_INFO("DEVICE", "Claim PIN: %s (expires in %ds)", pin, expiresInSeconds);

        // Отправляем PIN на RP2040
        if (self->uart_)
        {
            DryerUart::ClaimStatusPayload payload{};
            payload.status = DryerUart::ClaimingStatus::WaitingClaim;
            strncpy(payload.pin, pin, sizeof(payload.pin) - 1);
            payload.pin[sizeof(payload.pin) - 1] = '\0';
            payload.expiresAt = 0; // TODO: вычислить timestamp
            payload.remainingSeconds = expiresInSeconds;

            self->uart_->sendClaimStatus(payload);
        }

        // Вызываем пользовательский callback (например, для WebSerial)
        if (self->userClaimPinCallback_)
        {
            self->userClaimPinCallback_(pin, expiresInSeconds);
        }
    }

    void IdryerDevice::onClaimComplete(const char *deviceId, void *ctx)
    {
        auto *self = static_cast<IdryerDevice *>(ctx);

        HAL_LOG_INFO("DEVICE", "Claim complete: deviceId=%s", deviceId);

        // Отправляем deviceId на RP2040
        if (self->uart_)
        {
            DryerUart::ClaimCompletePayload payload{};
            payload.success = 1;
            strncpy(payload.deviceId, deviceId, sizeof(payload.deviceId) - 1);
            payload.deviceId[sizeof(payload.deviceId) - 1] = '\0';

            self->uart_->sendClaimComplete(payload);
        }

        // Если mcuSerial уже известен (Hello от RP2040 пришёл до claiming) —
        // переключаем MQTT топик на mcuSerial прямо сейчас, чтобы первое
        // подключение к MQTT было уже на правильном топике
        if (self->lastHelloValid_ && self->lastHello_.mcuSerial[0] != '\0')
        {
            const char *currentSerial = self->cloud_.getIdentity().serialNumber;
            if (strcmp(currentSerial, self->lastHello_.mcuSerial) != 0)
            {
                HAL_LOG_INFO("DEVICE", "Switching MQTT topic after claim: %s -> %s",
                             currentSerial, self->lastHello_.mcuSerial);
                self->cloud_.switchSerialNumber(self->lastHello_.mcuSerial);
            }
        }
    }

    void IdryerDevice::onUnclaimed(void *ctx)
    {
        auto *self = static_cast<IdryerDevice *>(ctx);

        HAL_LOG_WARN("DEVICE", "Device not claimed - notifying MCU");

        // Отправляем статус Idle на RP2040 чтобы показать что нужна привязка
        if (self->uart_)
        {
            DryerUart::ClaimStatusPayload payload{};
            payload.status = DryerUart::ClaimingStatus::Idle;
            payload.pin[0] = '\0';
            payload.expiresAt = 0;
            payload.remainingSeconds = 0;

            self->uart_->sendClaimStatus(payload);
        }
    }

    // =============================================================================
    // СИНХРОНИЗАЦИЯ ВРЕМЕНИ
    // =============================================================================

    void IdryerDevice::syncTimeFromBackend(const char *timestamp)
    {
        if (!timestamp)
        {
            return;
        }

        struct tm tm = {0};

        // Убираем миллисекунды из ISO 8601
        char cleanTimestamp[32];
        strncpy(cleanTimestamp, timestamp, sizeof(cleanTimestamp) - 1);
        cleanTimestamp[sizeof(cleanTimestamp) - 1] = '\0';

        char *dot = strchr(cleanTimestamp, '.');
        if (dot)
        {
            char *z = strchr(dot, 'Z');
            if (z)
            {
                *dot = 'Z';
                *(dot + 1) = '\0';
            }
        }

        if (strptime(cleanTimestamp, "%Y-%m-%dT%H:%M:%SZ", &tm) != nullptr)
        {
            // Важно: mktime интерпретирует `tm` как локальное время.
            // Здесь ожидается UTC-вход (`...Z`), поэтому TZ окружения влияет на результат.
            time_t t = mktime(&tm);

            struct timeval tv = {0};
            tv.tv_sec = t;
            tv.tv_usec = 0;
            settimeofday(&tv, nullptr);

            HAL_LOG_INFO("DEVICE", "Time synced: %s", timestamp);
        }
        else
        {
            HAL_LOG_WARN("DEVICE", "Failed to parse timestamp: %s", timestamp);
        }
    }

} // namespace idryer

#endif // ESP32 || ESP_PLATFORM
