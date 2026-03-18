/**
 * @file WsServer.cpp
 * @brief Реализация WebSocket сервера для локального доступа
 *
 * JSON формат полностью аналогичен MQTT (telemetry_publisher.cpp).
 * mDNS сервис: _idryer._tcp на порту 81.
 */

#if defined(ESP32) || defined(ESP_PLATFORM)

#include "WsServer.h"
#include <WebSocketsServer.h>
#include <ESPmDNS.h>
#include <hal/hal_types.h>

// Подкласс для доступа к protected-методам фрагментированной отправки
class WsServerImpl : public WebSocketsServer {
public:
    explicit WsServerImpl(uint16_t port) : WebSocketsServer(port) {}

    void sendFragmented(uint8_t num, const char* prefix, size_t prefixLen,
                        const char* json, size_t jsonLen) {
        WSclient_t* client = &_clients[num];
        char suffix = '}';
        sendFrame(client, WSop_text,
            reinterpret_cast<uint8_t*>(const_cast<char*>(prefix)), prefixLen, false);
        sendFrame(client, WSop_continuation,
            reinterpret_cast<uint8_t*>(const_cast<char*>(json)), jsonLen, false);
        sendFrame(client, WSop_continuation,
            reinterpret_cast<uint8_t*>(&suffix), 1, true);
    }
};

// =============================================================================
// КОНСТРУКТОР / ДЕСТРУКТОР
// =============================================================================

WsServer::WsServer(DryerUart::UartBridge* uart)
    : uart_(uart)
{
    memset(deviceName_, 0, sizeof(deviceName_));
    memset(deviceToken_, 0, sizeof(deviceToken_));
}

WsServer::~WsServer()
{
    stop();
}

// =============================================================================
// LIFECYCLE
// =============================================================================

void WsServer::begin(const char* deviceName, const char* deviceToken)
{
    if (enabled_) return;

    strncpy(deviceName_, deviceName, sizeof(deviceName_) - 1);
    deviceName_[sizeof(deviceName_) - 1] = '\0';

    strncpy(deviceToken_, deviceToken ? deviceToken : "", sizeof(deviceToken_) - 1);
    deviceToken_[sizeof(deviceToken_) - 1] = '\0';

    // Запускаем WebSocket сервер на порту 81
    ws_ = new WsServerImpl(81);
    ws_->onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
        onWsEvent(num, static_cast<uint8_t>(type), payload, length);
    });
    ws_->begin();

    // mDNS — имя = serial number устройства
    MDNS.begin(deviceName_);
    MDNS.addService("_idryer", "_tcp", 81);

    enabled_ = true;

    HAL_LOG_INFO("WS", "Server started: %s.local:81", deviceName_);
}

void WsServer::stop()
{
    if (!enabled_) return;

    if (ws_) {
        ws_->close();
        delete ws_;
        ws_ = nullptr;
    }

    MDNS.end();

    enabled_ = false;
    connectedClient_ = -1;
    clientAuthorized_ = false;

    HAL_LOG_INFO("WS", "Server stopped");
}

void WsServer::loop()
{
    if (!enabled_ || !ws_) return;
    ws_->loop();
}

// =============================================================================
// WS СОБЫТИЯ
// =============================================================================

void WsServer::onWsEvent(uint8_t num, uint8_t type, uint8_t* payload, size_t length)
{
    WStype_t wsType = static_cast<WStype_t>(type);

    switch (wsType) {
    case WStype_CONNECTED:
        HAL_LOG_INFO("WS", "Client #%d connected", num);
        // Допускаем только одного клиента
        if (connectedClient_ >= 0 && connectedClient_ != num) {
            HAL_LOG_WARN("WS", "Rejecting client #%d, already have #%d", num, connectedClient_);
            ws_->disconnect(num);
            return;
        }
        connectedClient_ = num;
        clientAuthorized_ = false;
        break;

    case WStype_DISCONNECTED:
        HAL_LOG_INFO("WS", "Client #%d disconnected", num);
        if (connectedClient_ == num) {
            connectedClient_ = -1;
            clientAuthorized_ = false;
        }
        break;

    case WStype_TEXT:
        if (num == connectedClient_) {
            handleWsMessage(num, reinterpret_cast<const char*>(payload), length);
        }
        break;

    default:
        break;
    }
}

void WsServer::handleWsMessage(uint8_t num, const char* json, size_t length)
{
    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, json, length);
    if (err) {
        HAL_LOG_WARN("WS", "JSON parse error: %s", err.c_str());
        return;
    }

    const char* type = doc["type"] | "";

    // Auth — должен быть первым сообщением
    if (strcmp(type, "auth") == 0) {
        const char* token = doc["token"] | "";

        if (deviceToken_[0] != '\0' && strcmp(token, deviceToken_) == 0) {
            clientAuthorized_ = true;

            StaticJsonDocument<128> resp;
            resp["type"] = "auth_ok";
            resp["deviceName"] = deviceName_;
            sendJson(nullptr, resp);

            HAL_LOG_INFO("WS", "Client authorized");
        } else {
            StaticJsonDocument<64> resp;
            resp["type"] = "auth_fail";
            resp["reason"] = "invalid_token";
            sendJson(nullptr, resp);

            HAL_LOG_WARN("WS", "Client auth failed: invalid token");
        }
        return;
    }

    // Все остальные сообщения требуют авторизации
    if (!clientAuthorized_) {
        StaticJsonDocument<64> resp;
        resp["type"] = "auth_fail";
        resp["reason"] = "not_authorized";
        sendJson(nullptr, resp);
        return;
    }

    // Команды
    if (strcmp(type, "command") == 0) {
        const char* command = doc["command"] | "";
        JsonObjectConst data = doc["data"].as<JsonObjectConst>();

        if (strlen(command) > 0 && cmdCallback_) {
            HAL_LOG_INFO("WS", "Command: %s", command);
            cmdCallback_(command, data);
        }
        return;
    }

    HAL_LOG_WARN("WS", "Unknown message type: %s", type);
}

// =============================================================================
// ОТПРАВКА JSON
// =============================================================================

void WsServer::sendJson(const char* type, JsonDocument& doc)
{
    if (!ws_ || connectedClient_ < 0) return;

    // Если type задан — это обёртка {"type": ..., "data": ...}
    // Если type == nullptr — отправляем doc как есть (для auth ответов)
    if (type) {
        StaticJsonDocument<2048> wrapper;
        wrapper["type"] = type;
        wrapper["data"] = doc.as<JsonObject>();

        char buf[2048];
        size_t len = serializeJson(wrapper, buf, sizeof(buf));
        ws_->sendTXT(connectedClient_, buf, len);
    } else {
        char buf[256];
        size_t len = serializeJson(doc, buf, sizeof(buf));
        ws_->sendTXT(connectedClient_, buf, len);
    }
}

void WsServer::sendWrappedJsonRaw(const char* type, const char* json, size_t length)
{
    if (!ws_ || connectedClient_ < 0 || !type || !json || length == 0) return;

    char prefix[48];
    int prefixLen = snprintf(prefix, sizeof(prefix), "{\"type\":\"%s\",\"data\":", type);
    if (prefixLen <= 0 || static_cast<size_t>(prefixLen) >= sizeof(prefix)) {
        HAL_LOG_WARN("WS", "Raw wrapper prefix overflow for type=%s", type);
        return;
    }

    // Отправляем тремя фрагментами без malloc большого буфера:
    // 1) {"type":"...","data":   (fin=false)
    // 2) <json>                  (fin=false)
    // 3) }                       (fin=true)
    ws_->sendFragmented(connectedClient_, prefix, static_cast<size_t>(prefixLen), json, length);
}

// =============================================================================
// ПУБЛИКАЦИЯ ДАННЫХ
// =============================================================================

void WsServer::sendTelemetry(const DryerUart::TelemetryPayload& data)
{
    if (!isClientConnected()) return;

    DynamicJsonDocument doc(1024);
    JsonArray units = doc.createNestedArray("units");

    for (uint8_t i = 0; i < data.count && i < 4; ++i) {
        const auto& entry = data.units[i];
        JsonObject unit = units.createNestedObject();

        char unitIdStr[4];
        formatUnitId(unitIdStr, sizeof(unitIdStr), entry.unitId);
        unit["unitId"] = unitIdStr;
        unit["temperature"] = entry.temperatureC10 / 10.0f;
        unit["humidity"] = entry.humidityPct10 / 10.0f;
        unit["heaterPower"] = entry.heaterPowerPct;
        unit["fanStatus"] = (entry.fanOn == 1);
    }

    sendJson("telemetry", doc);
}

void WsServer::sendStatus(const DryerUart::StatusPayload& data)
{
    if (!isClientConnected()) return;

    DynamicJsonDocument doc(2048);
    JsonArray units = doc.createNestedArray("units");

    for (uint8_t i = 0; i < data.count && i < 4; ++i) {
        const auto& entry = data.units[i];
        JsonObject unit = units.createNestedObject();

        char unitIdStr[4];
        formatUnitId(unitIdStr, sizeof(unitIdStr), entry.unitId);
        unit["unitId"] = unitIdStr;
        unit["mode"] = modeToString(entry.mode);

        if (entry.mode != DryerUart::DryerMode::Idle &&
            entry.mode != DryerUart::DryerMode::Fault) {

            unit["sessionNum"] = entry.sessionNum;

            JsonObject target = unit.createNestedObject("target");
            target["temperature"] = entry.targetTempC10 / 10.0f;
            target["duration"] = entry.durationMinutes;
            if (entry.targetHumidityPct > 0) {
                target["humidity"] = entry.targetHumidityPct;
            }

            unit["totalElapsed"] = entry.elapsedSeconds;
            unit["totalRemaining"] = entry.totalRemainingSeconds;

            if (entry.mode == DryerUart::DryerMode::Profile) {
                unit["currentStage"] = entry.currentStage;
                unit["totalStages"] = entry.totalStages;
                unit["stageElapsed"] = entry.stageElapsedSeconds;
                unit["stageRemaining"] = entry.stageRemainingSeconds;
                unit["stagePhase"] = (entry.stagePhase == DryerUart::StagePhase::Ramp) ? "RAMP" : "HOLD";
            }
        }
    }

    doc["uptime"] = data.uptime;
    sendJson("status", doc);
}

void WsServer::sendWeights(const DryerUart::WeightsPayload& data)
{
    if (!isClientConnected()) return;

    DynamicJsonDocument doc(512);
    JsonArray weights = doc.createNestedArray("weights");

    for (uint8_t i = 0; i < data.count && i < 4; ++i) {
        const auto& entry = data.weights[i];
        JsonObject w = weights.createNestedObject();

        char sensorIdStr[4];
        formatSensorId(sensorIdStr, sizeof(sensorIdStr), entry.sensorId);
        w["sensorId"] = sensorIdStr;
        w["value"] = static_cast<float>(entry.weightGramsC10) / 10.0f;

        char unitIdStr[4];
        formatUnitId(unitIdStr, sizeof(unitIdStr), entry.unitId);
        w["unitId"] = unitIdStr;
    }

    sendJson("weights", doc);
}

void WsServer::sendRfid(const DryerUart::RfidPayload& data)
{
    if (!isClientConnected()) return;

    DynamicJsonDocument doc(512);

    char unitIdStr[4];
    formatUnitId(unitIdStr, sizeof(unitIdStr), data.unitId);
    doc["unitId"] = unitIdStr;
    doc["event"] = rfidEventToString(data.event);

    char tag[33];
    strncpy(tag, data.tag, sizeof(tag) - 1);
    tag[sizeof(tag) - 1] = '\0';
    doc["tag"] = tag;
    doc["readerId"] = data.readerId;

    sendJson("rfid", doc);
}

void WsServer::sendConfig(const char* json, uint16_t length, bool isDelta)
{
    if (!isClientConnected()) return;

    // Для config raw JSON уже подготовлен в IdryerDevice для MQTT публикации.
    // Повторный deserialize/serialize здесь не нужен и ломает full config на ~2 KB лимите.
    sendWrappedJsonRaw(isDelta ? "config_delta" : "config", json, length);
}

void WsServer::sendInfo(uint8_t unitsCount, const DryerUart::UnitConfig* units,
                         const char* hwVersion, const char* fwVersion, uint32_t workTime,
                         const char* mcuSerial)
{
    if (!isClientConnected()) return;

    DynamicJsonDocument doc(512);
    doc["hwVersion"] = hwVersion;
    doc["fwVersion"] = fwVersion;
    doc["workTime"] = workTime;
    doc["unitsCount"] = unitsCount;
    if (mcuSerial && mcuSerial[0] != '\0') {
        doc["mcuSerial"] = mcuSerial;
    }

    JsonArray unitsArr = doc.createNestedArray("units");
    for (uint8_t i = 0; i < unitsCount && i < 4; ++i) {
        JsonObject u = unitsArr.createNestedObject();
        char unitIdStr[4];
        formatUnitId(unitIdStr, sizeof(unitIdStr), units[i].unitId);
        u["unitId"] = unitIdStr;
        u["capabilities"] = units[i].capabilities;
    }

    sendJson("info", doc);
}

// =============================================================================
// СТАТУС
// =============================================================================

DryerUart::WsStatusPayload WsServer::getStatus() const
{
    DryerUart::WsStatusPayload status{};

    if (!enabled_) {
        status.state = DryerUart::WsState::Disabled;
    } else if (connectedClient_ >= 0 && clientAuthorized_) {
        status.state = DryerUart::WsState::Connected;
    } else {
        status.state = DryerUart::WsState::Listening;
    }

    status.pin = 0;
    status.pairedCount = clientAuthorized_ ? 1 : 0;
    status.maxClients = 1;

    return status;
}

// =============================================================================
// ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ
// =============================================================================

void WsServer::formatUnitId(char* buf, size_t size, uint8_t unitId)
{
    snprintf(buf, size, "U%d", unitId + 1);
}

void WsServer::formatSensorId(char* buf, size_t size, uint8_t sensorId)
{
    snprintf(buf, size, "W%d", sensorId + 1);
}

const char* WsServer::modeToString(DryerUart::DryerMode mode)
{
    switch (mode) {
    case DryerUart::DryerMode::Idle:    return "IDLE";
    case DryerUart::DryerMode::Drying:  return "DRYING";
    case DryerUart::DryerMode::Storage: return "STORAGE";
    case DryerUart::DryerMode::Profile: return "PROFILE";
    case DryerUart::DryerMode::Fault:   return "FAULT";
    default:                             return "UNKNOWN";
    }
}

const char* WsServer::rfidEventToString(DryerUart::RfidEvent event)
{
    switch (event) {
    case DryerUart::RfidEvent::TagDetected: return "tag_detected";
    case DryerUart::RfidEvent::TagRemoved:  return "tag_removed";
    default:                                 return "unknown";
    }
}

#endif // ESP32 || ESP_PLATFORM
