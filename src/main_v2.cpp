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
#include "esp_heap_caps.h"

#include <iDryer.h>
#include <idryer_uart.h>
#include <idryer_integrations.h>
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
    .model             = "iDryer",
};

static iDryer::Link                s_link(CFG);
static hal::ArduinoSerial          s_uartSerial(Serial1, 1);
static UartBridge                  s_uart;
static ConfigReceiver              s_configRx;

// Кэша конфига нет: на сушилке значения меняются и через энкодер на железе,
// ESP про это узнаёт не сразу — кэш отдавал бы устаревший snapshot. Всегда
// перезапрашиваем у RP2040 (get_config / online-transition → requestConfig()).

// Состояние для onlne-transition в every().
static bool s_prevOnline = false;

// HA controls state — температура и время для команды drying из HA.
static int  s_haDryTemp          = 60;
static int  s_haDryTime          = 240;
static bool s_haControlsReady    = false;

// Буфер для собранного меню выделяется на heap по требованию (publishConfig).
// В .bss держать ~38 КБ нельзя — фрагментирует heap, ломает TLS-handshake mbedtls.

// ── Публикация delta (один-несколько изменённых пунктов) ─────────────────────
// json — сырой delta от RP2040: {"rev":N,"vals":{"7":[50]}}
// Используется когда ConfigReceiver::isDelta() (старший бит transferId).
// Канон в mqtt_contract.yaml (config_delta): {"rev":N,"d":{"7":[50]}} —
// поле 'd' вместо 'vals'. Перепаковываем перед publish.
static void publishConfigDelta(const char* json, uint16_t len) {
    if (!json || len == 0) return;

    // Обновляем g_menu_cache (для local-WS клиентов и других потребителей).
    if (!menu_parseDelta(json)) {
        HAL_LOG_WARN("MENU", "parseDelta FAILED, dropping (%u bytes)", len);
        return;
    }

    // Парсим RP2040-формат и переименовываем "vals" → "d". 512 байт capacity
    // хватает на 1–3 изменённых per-unit пункта; на стеке.
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, json, len)) {
        HAL_LOG_WARN("MENU", "delta deserialize FAILED (%u bytes)", len);
        return;
    }
    if (!doc.containsKey("vals")) {
        HAL_LOG_WARN("MENU", "delta missing 'vals' key, dropping");
        return;
    }
    doc["d"] = doc["vals"];
    doc.remove("vals");

    char buf[256];
    size_t out = serializeJson(doc, buf, sizeof(buf));
    if (out == 0) {
        HAL_LOG_WARN("MENU", "delta reserialize FAILED");
        return;
    }

    s_link.devicePublisher()->publishConfigDelta(buf, out);
    HAL_LOG_INFO("MENU", "TX delta → MQTT: %u bytes", (unsigned)out);
}

// ── Вспомогательная функция публикации конфига ────────────────────────────────
// json — сырой JSON от RP2040: {v, full:true, vals:{...}}
// Парсим его в g_menu_cache, затем собираем {v, menu:[...]} для портала.
static void publishConfig(const char* json, uint16_t len) {
    if (!json || len == 0) {
        HAL_LOG_WARN("MENU", "publishConfig skipped: json=%p len=%u", json, len);
        return;
    }

    // Парсим vals из RP2040 → обновляем g_menu_cache
    if (!menu_parseFullConfig(json)) {
        HAL_LOG_WARN("MENU", "parseFullConfig FAILED → publishing raw (%u bytes)", len);
        s_link.devicePublisher()->publishConfigRaw(json, len);
        HAL_LOG_INFO("MENU", "TX raw → MQTT: %u bytes", len);
        return;
    }
    HAL_LOG_INFO("MENU", "parseFullConfig OK (values cached)");

    // Heap-alloc буфера на время сборки и публикации (после TLS уже подняли).
    HAL_LOG_INFO("MENU", "malloc(%u): free=%u largest=%u",
                 (unsigned)MENU_FULL_JSON_BUF_SIZE,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    char* menuJson = (char*)malloc(MENU_FULL_JSON_BUF_SIZE);
    if (!menuJson) {
        HAL_LOG_ERROR("MENU", "malloc(%u) FAILED → publishing raw (%u bytes)",
                      (unsigned)MENU_FULL_JSON_BUF_SIZE, len);
        s_link.devicePublisher()->publishConfigRaw(json, len);
        HAL_LOG_INFO("MENU", "TX raw → MQTT: %u bytes", len);
        return;
    }
    HAL_LOG_INFO("MENU", "malloc OK at %p", menuJson);

    // Собираем {v, menu:[...]} для портала
    size_t menuLen = menu_buildFullJson(menuJson, MENU_FULL_JSON_BUF_SIZE);
    HAL_LOG_INFO("MENU", "buildFullJson returned %u bytes", (unsigned)menuLen);
    if (menuLen == 0) {
        HAL_LOG_WARN("MENU", "buildFullJson FAILED → publishing raw (%u bytes)", len);
        free(menuJson);
        s_link.devicePublisher()->publishConfigRaw(json, len);
        HAL_LOG_INFO("MENU", "TX raw → MQTT: %u bytes", len);
        return;
    }
    HAL_LOG_INFO("MENU", "TX preview: %.200s%s", menuJson,
                 (menuLen > 200) ? "..." : "");

    s_link.devicePublisher()->publishConfigRaw(menuJson, menuLen);
    HAL_LOG_INFO("MENU", "TX assembled → MQTT: %u bytes", (unsigned)menuLen);
    free(menuJson);
    HAL_LOG_INFO("MENU", "free done: heap free=%u largest=%u",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));

    // При первом получении конфига регистрируем HA controls с реальными min/max из меню.
    if (!s_haControlsReady) {
        s_haControlsReady = true;
        int tempMin = (int)g_menu_meta[3].min_val;
        int tempMax = (int)g_menu_meta[3].max_val;
        int timeMin = (int)g_menu_meta[4].min_val;
        int timeMax = (int)g_menu_meta[4].max_val;
        s_haDryTemp = (int)g_menu_cache.getFloat(3);
        s_haDryTime = (int)g_menu_cache.getFloat(4);

        auto& ha = s_link.ha();
        ha.number("dry_temp", "Drying temperature", tempMin, tempMax,
                  [](int v) { s_haDryTemp = v; }, "°C", "mdi:thermometer-plus");
        ha.number("dry_time", "Drying duration", timeMin, timeMax,
                  [](int v) { s_haDryTime = v; }, "min", "mdi:timer-outline");
        ha.button("start_drying", "Start drying", []() {
            UartCmdPayload cmd{};
            cmd.command     = UartCmdCode::Start;
            cmd.targetState = (uint8_t)UartDryerMode::Drying;
            cmd.unitId      = 0;
            cmd.arg0        = (uint32_t)(s_haDryTemp * 10);
            cmd.arg1        = (uint32_t)s_haDryTime;
            s_uart.sendCommand(cmd);
        }, "mdi:play-circle");
        ha.button("start_storage", "Start storage", []() {
            UartCmdPayload cmd{};
            cmd.command     = UartCmdCode::Start;
            cmd.targetState = (uint8_t)UartDryerMode::Storage;
            cmd.unitId      = 0;
            cmd.arg0        = (uint32_t)((int)g_menu_meta[7].min_val * 10);
            cmd.arg1        = (uint32_t)g_menu_meta[8].min_val;
            s_uart.sendCommand(cmd);
        }, "mdi:archive");
        ha.button("stop", "Stop", []() {
            UartCmdPayload cmd{};
            cmd.command = UartCmdCode::Stop;
            cmd.unitId  = 0;
            s_uart.sendCommand(cmd);
        }, "mdi:stop-circle");

        s_link.ha().republishAll();
    }
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

    // Always ack Hello to give RP2040 connection info (IP/SSID).
    UartHelloAckPayload ack{};
    ack.ipAddress = (uint32_t)WiFi.localIP();
    strncpy(ack.ssid, WiFi.SSID().c_str(), sizeof(ack.ssid) - 1);
    s_uart.sendHelloAck(ack);
    s_mcuConnected = true;

    // Pass mcuSerial to cloud layer first — must happen before setUnitsCount
    // and publishInfoNow so that buildInfoJson() picks up the correct mcuSerial.
    auto result = s_link.setMcuSerial(p.mcuSerial);
    s_link.setMcuFirmwareVersion(p.firmwareVersion);

    if (result == iDryer::McuSerialResult::Mismatch) {
        // Different RP2040 connected — signal error to controller via UART.
        // Cloud layer does not touch UART; product code handles the signal here.
        UartClaimStatusPayload sp{};
        sp.status = UartClaimStatus::Error;
        s_uart.sendClaimStatus(sp);
        return;
    }

    if (result == iDryer::McuSerialResult::Ignored) {
        HAL_LOG_WARN("UART", "Hello mcuSerial empty, waiting for valid Hello");
        return;
    }

    // mcuSerial accepted (AcceptedFirstBind or AcceptedBound) — proceed.
    if (p.unitsCount >= 1 && p.unitsCount <= iDryer::MAX_UNITS) {
        s_link.setUnitsCount(p.unitsCount);
        s_link.publishInfoNow(); // info now contains correct mcuSerial
    }

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
    // TODO(diag): убрать после стабилизации меню (chunk-by-chunk трассировка).
    HAL_LOG_INFO("MENU", "chunk: dataLen=%u flags=0x%02X result=%d total=%u",
                 dataLen, hdr.flags, (int)result, s_configRx.getLength());
    if (result == ConfigFragResult::Complete) {
        const uint16_t len   = s_configRx.getLength();
        const char*    json  = s_configRx.getJson();
        const bool     delta = s_configRx.isDelta();
        // TODO(diag): убрать после стабилизации меню.
        HAL_LOG_INFO("MENU", "RX from RP2040: %u bytes %s (capacity %u)",
                     len, delta ? "DELTA" : "FULL", (unsigned)CONFIG_BUFFER_SIZE);
        HAL_LOG_INFO("MENU", "RX preview: %.200s%s", json ? json : "(null)",
                     (len > 200) ? "..." : "");
        if (delta) {
            publishConfigDelta(json, len);
        } else {
            HAL_LOG_INFO("MENU", "heap before publishConfig: free=%u largest=%u",
                         (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
                         (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
            publishConfig(json, len);
        }
        s_configRx.reset();
    }
}

static void onLog(const uint8_t* payload, uint8_t length) {
    if (length < sizeof(idryer::UartLogPayload)) return;
    const auto* log = reinterpret_cast<const idryer::UartLogPayload*>(payload);

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
        // Без кэша: всегда тянем актуальные значения с RP2040 (энкодер может
        // крутить пользователь на железе, ESP про это узнаёт только из ответа).
        requestConfig();
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
        cmd.unitId  = parseUnitId(data);
        s_uart.sendCommand(cmd);
    });

    s_link.onCommand("find", [](JsonObjectConst data) {
        UartCmdPayload cmd{};
        cmd.command = UartCmdCode::Find;
        cmd.unitId  = parseUnitId(data);
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
            p.stages[i].temp = (uint16_t)(s["temperature"].as<int>() * 10);
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
        p.transferId = ++s_configTid;
        p.totalSize  = (uint16_t)len;
        p.chunkIndex = 0;
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
        p.transferId = ++s_configTid;
        p.totalSize  = (uint16_t)len;
        p.chunkIndex = 0;
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
        hb.cloudState      = static_cast<idryer::UartLinkCloudState>(s_link.isOnline() ? 7 : 1);
        s_uart.sendHeartbeat(hb);
    });

    // При выходе в онлайн запрашиваем конфиг у RP2040 (кэша нет — см. publishConfig).
    // RP2040 шлёт Hello только при своём старте — если ESP32 перезапустился позже,
    // Hello не придёт, запрашиваем GetConfig сами при первом online.
    s_link.every(2000, []() {
        const bool online = s_link.isOnline();
        if (online && !s_prevOnline) {
            requestConfig();
        }
        s_prevOnline = online;
    });
}

// ─────────────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    WiFi.persistent(false);

    s_link.onDiagnostic([](const char* message) {
        Serial.println(message);
    });

    s_link.onClaimPin([](const char* pin, uint32_t exp) {
        Serial.printf("CLAIM_PIN:%s:%lu\n", pin, exp);
        Serial.flush();
        UartClaimStatusPayload sp{};
        sp.status = UartClaimStatus::WaitingClaim;
        strncpy(sp.pin, pin, sizeof(sp.pin) - 1);
        sp.remainingSeconds = exp;
        s_uart.sendClaimStatus(sp);
    });

    s_link.setWaitForMcuSerial(true);
    s_link.begin();
    s_link.integrationsManager()->setActive(idryer::cloud::ActiveIntegration::Ha);
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

    // Periodic HelloRequest to RP2040 until it responds (max 12 attempts, every 5s).
    // Needed when RP2040 was already running before ESP32 booted and its initial
    // Hello was missed.
    if (!s_mcuConnected) {
        static uint32_t s_lastHelloReqMs = 0;
        static uint8_t  s_helloReqCount  = 0;
        const uint32_t  now = millis();
        if (s_helloReqCount < 12 && now - s_lastHelloReqMs >= 5000) {
            UartHelloPayload req{};
            req.role = UartRole::HelloRequest;
            req.firmwareVersion = VERSION_NUMBER;
            s_uart.sendHello(req, false);
            s_lastHelloReqMs = now;
            s_helloReqCount++;
            HAL_LOG_INFO("UART", "HelloRequest -> RP2040 (attempt %u/12)", s_helloReqCount);
        }
    }
}
