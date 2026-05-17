// iDryer Link v2 — UART bridge RP2040↔Cloud на базе idryer-core SDK.
//
// Архитектура: RP2040 (контроллер) <—UART→ ESP32 (этот файл) <—WiFi/MQTT→ Портал
//
// iDryer::Link обеспечивает: WiFi/Improv, MQTT, claiming, LocalAccess, HA.
// idryer::UartBridge парсит фреймы RP2040 и диспетчирует их в хэндлеры ниже.
// Телеметрия/статус из UART записываются в s_link.telemetry / s_link.status
// и периодически публикуются библиотекой.
// Команды портала (drying/stop/storage) транслируются в UartCmdPayload → RP2040.
// Конфиг (меню) приходит от RP2040 чанками → переиздаётся на MQTT retained.

#include <Arduino.h>
#include <WiFi.h>
#include <driver/gpio.h>

#include <iDryer.h>
#include <idryer_uart.h>
#include <config/config_manager.h>
#include <hal/hal_arduino.h>
#include <local_access/device_publisher.h>

#include "version.h"

#include <menu_commands.h>
#include <menu_cache.h>

using namespace idryer;

// ── Пины UART (ESP32-C3 Super Mini, JTAG-shared → требуют gpio_reset_pin) ──
constexpr int UART_RX_PIN = 6;
constexpr int UART_TX_PIN = 7;

// ── SDK объекты ──────────────────────────────────────────────────────────────
static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Dryer,
    .unitsCount        = 1,  // реальное число физических юнитов на этом железе; уточняется из Hello RP2040
    .hasHeaterPower    = true,
    .hasFanStatus      = true,
    .hasLed            = false,
    .hasScales         = true,
    .hasRfid           = true,
    .hasAirTemp        = true,
    .hasAirHumidity    = true,
    .hasHeaterTemp     = false,
    .allowHa           = true,
    .allowBambu        = false,
    .allowMoonraker    = false,
    .telemetryPeriodMs = 5000,
    .statusPeriodMs    = 10000,
    .hardwareVersion   = "DRYER-v3",
    .firmwareVersion   = VERSION_STR,
    .model             = "iDryer Dryer",
};

static iDryer::Link                s_link(CFG);
static hal::ArduinoSerial          s_uartSerial(Serial1, 1);
static UartBridge                  s_uart;
static ConfigReceiver              s_configRx;

// Кэш последнего config JSON от RP2040 — переиздаётся при get_config и при онлайн.
static char     s_lastConfig[CONFIG_BUFFER_SIZE];
static uint16_t s_lastConfigLen = 0;

// Состояние для onlne-transition в every().
static bool s_prevOnline = false;

// Статический буфер для полного меню JSON ({v, menu:[...]}).
static char s_menuJson[MENU_FULL_JSON_BUF_SIZE];

// ── Вспомогательная функция публикации конфига ────────────────────────────────
// json — сырой JSON от RP2040: {v, full:true, vals:{...}}
// Парсим его в g_menu_cache, затем собираем {v, menu:[...]} для портала.
static void publishConfig(const char* json, uint16_t len) {
    if (!json || len == 0) return;

    // Парсим vals из RP2040 → обновляем g_menu_cache
    if (!menu_parseFullConfig(json)) {
        HAL_LOG_WARN("MENU", "parseFullConfig failed, publishing raw");
        s_link.devicePublisher()->publishConfigRaw(json, len);
        return;
    }

    // Собираем {v, menu:[...]} для портала
    size_t menuLen = menu_buildFullJson(s_menuJson, sizeof(s_menuJson));
    if (menuLen == 0) {
        HAL_LOG_WARN("MENU", "buildFullJson failed, publishing raw");
        s_link.devicePublisher()->publishConfigRaw(json, len);
        return;
    }

    // Кэшируем собранный JSON (для get_config и online-transition)
    if (menuLen < CONFIG_BUFFER_SIZE) {
        memcpy(s_lastConfig, s_menuJson, menuLen);
        s_lastConfig[menuLen] = '\0';
        s_lastConfigLen = (uint16_t)menuLen;
    }

    s_link.devicePublisher()->publishConfigRaw(s_menuJson, menuLen);
}

// ── Маппинг UartDryerMode → iDryer::UnitMode ─────────────────────────────────
static iDryer::UnitMode modeFromUart(UartDryerMode m) {
    switch (m) {
        case UartDryerMode::Drying:  return iDryer::UnitMode::Drying;
        case UartDryerMode::Storage: return iDryer::UnitMode::Storage;
        case UartDryerMode::Profile: return iDryer::UnitMode::Profile;
        case UartDryerMode::Fault:   return iDryer::UnitMode::Fault;
        default:                     return iDryer::UnitMode::Idle;
    }
}

// ── UART handlers (RP2040 → ESP32) ────────────────────────────────────────────

// Зеркало DryerUart::LogPayload (idryer-protocol) — RP2040 шлёт бинарный блок.
// Структура задокументирована в idryer-protocol/uart_protocol.h строки 301-308.
struct UartLogPayload {
    char    severity[10];  // "CRIT" | "ERROR" | "WARN" | "INFO"
    char    source[20];
    char    event[32];
    char    message[100];
    uint8_t unitId;
    uint8_t count;  // occurrence counter (was _pad)
};

static bool s_mcuConnected = false;

static void requestConfig() {
    UartCmdPayload cmd{};
    cmd.command = UartCmdCode::GetConfig;
    cmd.unitId  = 0;  // 0xFF rejected by RP2040 (unitId >= NUM_UNITS check)
    s_uart.sendCommand(cmd, false);
}

static void onHello(const UartHelloPayload& p, const UartFrameHeader&) {
    HAL_LOG_INFO("UART", "Hello: type=%u fw=%u units=%u serial=%s",
                 p.deviceType, p.firmwareVersion, p.unitsCount, p.mcuSerial);
    UartHelloAckPayload ack{};
    ack.ipAddress = (uint32_t)WiFi.localIP();
    strncpy(ack.ssid, WiFi.SSID().c_str(), sizeof(ack.ssid) - 1);
    s_uart.sendHelloAck(ack);
    s_mcuConnected = true;

    // Синхронизируем число юнитов из EEPROM RP2040 → SDK → бэкенд
    if (p.unitsCount >= 1 && p.unitsCount <= iDryer::MAX_UNITS) {
        s_link.setUnitsCount(p.unitsCount);
        s_link.publishInfoNow(); // переиздаём info с правильным unitsCount
    }

    // Запрашиваем конфиг сразу после Hello
    requestConfig();
}

static void onTelemetry(const UartTelemetryPayload& p, const UartFrameHeader& hdr) {
    for (uint8_t i = 0; i < p.count && i < iDryer::MAX_UNITS; i++) {
        const auto& e = p.units[i];
        if (e.unitId >= iDryer::MAX_UNITS) continue;
        s_link.telemetry.airTempC[e.unitId]      = e.temperatureC10  / 10.0f;
        s_link.telemetry.airHumidityPct[e.unitId]= e.humidityPct10   / 10.0f;
        s_link.telemetry.heaterPower01[e.unitId] = e.heaterPowerPct  / 100.0f;
        s_link.telemetry.fanOn[e.unitId]         = (e.fanOn != 0);
    }
    s_uart.sendTelemetryAck(hdr.sequence);
}

static void onStatus(const UartStatusPayload& p, const UartFrameHeader&) {
    for (uint8_t i = 0; i < p.count && i < iDryer::MAX_UNITS; i++) {
        const auto& e = p.units[i];
        if (e.unitId >= iDryer::MAX_UNITS) continue;
        s_link.status.mode[e.unitId]       = modeFromUart((UartDryerMode)e.mode);
        s_link.status.targetTempC[e.unitId]= e.targetTempC10 / 10.0f;
        s_link.status.durationS[e.unitId]  = (uint32_t)e.durationMinutes * 60u;
        s_link.status.elapsedS[e.unitId]   = e.elapsedSeconds;
    }
    s_link.publishStatusNow();
}

static void onWeights(const UartWeightsPayload& p, const UartFrameHeader&) {
    for (uint8_t i = 0; i < p.count && i < iDryer::MAX_UNITS; i++) {
        const auto& w = p.weights[i];
        if (w.unitId < iDryer::MAX_UNITS)
            s_link.telemetry.weightG[w.unitId] = w.weightGramsC10 / 10u;
    }
}

// RP2040 шлёт JSON меню фрагментами. ConfigReceiver склеивает, потом публикуем.
static void onConfigChunk(const UartConfigChunkPayload& p, uint8_t dataLen,
                          const UartFrameHeader& hdr) {
    auto result = s_configRx.processFragment(p, dataLen, hdr.flags);
    s_uart.sendConfigAck(hdr.sequence);
    if (result == ConfigFragResult::Complete) {
        HAL_LOG_INFO("UART", "Config received: %u bytes", s_configRx.getLength());
        publishConfig(s_configRx.getJson(), s_configRx.getLength());
        s_configRx.reset();
    }
}

static void onLog(const uint8_t* payload, uint8_t length) {
    if (length < sizeof(UartLogPayload)) return;
    const auto* log = reinterpret_cast<const UartLogPayload*>(payload);

    HAL_LOG_INFO("UART", "Log[%s] %s/%s: %s (U%u)",
                 log->severity, log->source, log->event, log->message, log->unitId + 1);

    // Строим JSON вручную, чтобы сохранить все поля как в idryer-protocol:
    // severity (CRIT/ERROR/WARN/INFO), source (SHT31/HEATER/...), event, message, unitId.
    // raiseEvent() не использем — оно теряет source и деградирует CRIT→ERROR.
    StaticJsonDocument<256> doc;
    doc["severity"] = log->severity;   // "CRIT" | "ERROR" | "WARN" | "INFO"
    doc["source"]   = log->source;     // "SHT31" | "THERMISTOR" | "HEATER" | ...
    doc["event"]    = log->event;      // "NO_RESPONSE" | "OVER_MAX" | ...
    doc["message"]  = log->message;    // human-readable
    doc["count"]    = log->count;      // occurrence counter (dedup)

    char uid[4];
    if (log->unitId < iDryer::MAX_UNITS) {
        snprintf(uid, sizeof(uid), "U%u", log->unitId + 1);
        doc["unitId"] = uid;
    } else {
        doc["unitId"] = "DEVICE";
    }

    s_link.devicePublisher()->publishEvent(doc);
}

static void onClaimStart(const UartFrameHeader&) {
    HAL_LOG_INFO("UART", "ClaimStart from MCU");
    s_link.requestClaim();
}

static void onUartError(const UartErrorPayload& p, bool remote) {
    HAL_LOG_WARN("UART", "error code=%u remote=%d", (uint8_t)p.code, remote);
}

// ── Портальные команды (портал → ESP32 → RP2040) ─────────────────────────────

// Парсит unitId вида "U1".."U4" → индекс 0..3. Возвращает 0xFF если не распознан.
static uint8_t parseUnitId(JsonObjectConst data) {
    JsonVariantConst v = data["unitId"];
    if (v.is<const char*>()) {
        const char* s = v.as<const char*>();
        if (s && s[0] == 'U' && s[1] >= '1' && s[1] <= '4') return (uint8_t)(s[1] - '1');
    }
    return 0xFF;
}

static void registerCommands() {
    s_link.onCommand("get_config", [](JsonObjectConst) {
        if (s_lastConfigLen > 0)
            s_link.devicePublisher()->publishConfigRaw(s_lastConfig, s_lastConfigLen);
    });

    s_link.onCommand("drying", [](JsonObjectConst data) {
        UartCmdPayload cmd{};
        cmd.command     = UartCmdCode::Start;
        cmd.targetState = (uint8_t)UartDryerMode::Drying;
        cmd.unitId      = parseUnitId(data);
        JsonObjectConst params = data["params"];
        cmd.arg0        = (uint32_t)(params["temperature"].as<int>() * 10);
        cmd.arg1        = (uint32_t)params["duration"].as<int>();
        s_uart.sendCommand(cmd);
    });

    s_link.onCommand("stop", [](JsonObjectConst data) {
        UartCmdPayload cmd{};
        cmd.command = UartCmdCode::Stop;
        cmd.unitId  = data["unitId"] | (uint8_t)0xFF;
        s_uart.sendCommand(cmd);
    });

    s_link.onCommand("clear_errors", [](JsonObjectConst data) {
        // Бэкенд может слать unitId как строку "U1" — ArduinoJson не конвертирует в uint8_t,
        // возвращает 0xFF. RP2040 отклоняет unitId >= NUM_UNITS, поэтому при 0xFF чистим все юниты.
        uint8_t uid = data["unitId"] | (uint8_t)0xFF;
        if (uid < iDryer::MAX_UNITS) {
            UartCmdPayload cmd{};
            cmd.command = UartCmdCode::ClearErrors;
            cmd.unitId  = uid;
            s_uart.sendCommand(cmd);
        } else {
            for (uint8_t i = 0; i < iDryer::MAX_UNITS; i++) {
                UartCmdPayload cmd{};
                cmd.command = UartCmdCode::ClearErrors;
                cmd.unitId  = i;
                s_uart.sendCommand(cmd);
            }
        }
    });

    s_link.onCommand("storage", [](JsonObjectConst data) {
        UartCmdPayload cmd{};
        cmd.command     = UartCmdCode::Start;
        cmd.targetState = (uint8_t)UartDryerMode::Storage;
        cmd.unitId      = parseUnitId(data);
        JsonObjectConst params = data["params"];
        cmd.arg0        = (uint32_t)(params["temperature"].as<int>() * 10);
        cmd.arg1        = (uint32_t)params["humidity"].as<int>();
        s_uart.sendCommand(cmd);
    });

    s_link.onCommand("profile", [](JsonObjectConst data) {
        UartProfilePayload p{};
        p.unitId      = parseUnitId(data);
        JsonObjectConst params = data["params"];
        p.startStage  = params["startStage"].as<uint8_t>();
        JsonArrayConst stages = params["stages"];
        p.totalStages = 0;
        for (JsonObjectConst s : stages) {
            if (p.totalStages >= 10) break;
            uint8_t i = p.totalStages++;
            p.stages[i].temp = (uint16_t)s["temperature"].as<int>();
            p.stages[i].ramp = (uint16_t)s["ramp"].as<int>();
            p.stages[i].hold = (uint16_t)s["hold"].as<int>();
        }
        s_uart.sendProfileCommand(p);
    });

    // set/invoke — пересылают JSON в RP2040 через ConfigPush (фрагмент с LAST_FRAGMENT).
    static uint16_t s_configTid = 0;

    s_link.onCommand("set", [](JsonObjectConst data) {
        if (!data["id"].is<int>()) return;
        char json[128];
        StaticJsonDocument<128> doc;
        doc["cmd"]  = "set";
        doc["id"]   = data["id"].as<int>();
        doc["unit"] = data["unit"] | 0;
        if (data.containsKey("val")) doc["val"] = data["val"];
        size_t len = serializeJson(doc, json, sizeof(json));

        UartConfigChunkPayload p{};
        p.header.transferId = ++s_configTid;
        p.header.totalSize  = (uint16_t)len;
        p.header.chunkIndex = 0;
        memcpy(p.data, json, len);
        s_uart.sendConfigPushChunk(p,
            UART_CONFIG_CHUNK_HEADER_SIZE + (uint8_t)len,
            UART_FLAG_ACK_REQ | UART_FLAG_LAST_FRAGMENT);
    });

    s_link.onCommand("invoke", [](JsonObjectConst data) {
        if (!data["id"].is<int>()) return;
        char json[64];
        StaticJsonDocument<64> doc;
        doc["cmd"] = "invoke";
        doc["id"]  = data["id"].as<int>();
        size_t len = serializeJson(doc, json, sizeof(json));

        UartConfigChunkPayload p{};
        p.header.transferId = ++s_configTid;
        p.header.totalSize  = (uint16_t)len;
        p.header.chunkIndex = 0;
        memcpy(p.data, json, len);
        s_uart.sendConfigPushChunk(p,
            UART_CONFIG_CHUNK_HEADER_SIZE + (uint8_t)len,
            UART_FLAG_ACK_REQ | UART_FLAG_LAST_FRAGMENT);
    });

    // Heartbeat → RP2040: без него RP2040 не устанавливает uartLinkReady=true
    // и никогда не шлёт накопленные ошибки (errlog) через UART.
    s_link.every(5000, []() {
        UartHeartbeatPayload hb{};
        hb.uptimeSeconds   = millis() / 1000;
        hb.wifiRssiDbm     = (int16_t)WiFi.RSSI();
        hb.errorsSinceBoot = 0;
        hb.cloudState      = s_link.isOnline() ? 7 : 1; // Online=7, WifiConnecting=1
        s_uart.sendHeartbeat(hb);
    });

    // При выходе в онлайн: переиздаём кэш конфига или запрашиваем если нет.
    // RP2040 шлёт Hello только при своём старте — если ESP32 перезапустился позже,
    // Hello не придёт, запрашиваем GetConfig сами при первом online.
    s_link.every(2000, []() {
        const bool online = s_link.isOnline();
        if (online && !s_prevOnline) {
            if (s_lastConfigLen > 0) {
                s_link.devicePublisher()->publishConfigRaw(s_lastConfig, s_lastConfigLen);
            } else {
                requestConfig();
            }
        }
        s_prevOnline = online;
    });
}

// ─────────────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    WiFi.persistent(false);

    s_link.onClaimPin([](const char* pin, uint32_t exp) {
        Serial.printf("CLAIM_PIN:%s:%lu\n", pin, exp);
        Serial.flush();
        UartClaimStatusPayload sp{};
        sp.status = UartClaimStatus::WaitingClaim;
        strncpy(sp.pin, pin, sizeof(sp.pin) - 1);
        sp.remainingSeconds = exp;
        s_uart.sendClaimStatus(sp);
    });

    s_link.begin();
    registerCommands();

    // ESP32-C3: GPIO6/7 по умолчанию JTAG — сбрасываем перед Serial1.
    gpio_reset_pin((gpio_num_t)UART_RX_PIN);
    gpio_reset_pin((gpio_num_t)UART_TX_PIN);
    Serial1.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    s_uart.begin(&s_uartSerial, 115200);

    s_uart.setHelloHandler(onHello);
    s_uart.setTelemetryHandler(onTelemetry);
    s_uart.setStatusHandler(onStatus);
    s_uart.setWeightsHandler(onWeights);
    s_uart.setConfigChunkHandler(onConfigChunk);
    s_uart.setClaimStartHandler(onClaimStart);
    s_uart.setErrorHandler(onUartError);
    s_uart.setLogHandler(onLog);
    s_uart.setRfidHandler([](const UartRfidPayload& p, const UartFrameHeader&) {
        StaticJsonDocument<128> doc;
        char uid[4];
        snprintf(uid, sizeof(uid), "U%u", p.unitId + 1);
        doc["unitId"]   = uid;
        doc["event"]    = (p.event == 1) ? "tag_detected" : "tag_removed";
        doc["readerId"] = p.readerId;
        if (p.event == 1) doc["tag"] = p.tag;
        s_link.devicePublisher()->publishRfid(doc);
    });

    HAL_LOG_INFO("MAIN", "iDryer Link v2 ready, fw=%s", VERSION_STR);
}

void loop() {
    s_link.loop();
    s_uart.loop();
}
