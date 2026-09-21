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
#include <mbedtls/base64.h>
#include "esp_heap_caps.h"

#include <iDryer.h>
#include <idryer_uart.h>
#include <idryer_integrations.h>
#include <config/config_manager.h>
#include <hal/hal_arduino.h>
#include <local_access/device_publisher.h>
#include <ota_receiver.h>  // OtaReceiver: target=esp + UART-proxy для target=rp2040
#include <platform/arduino/EspTouchProvisioner.h>

#include "version.h"

// HW-идентификатор Link-платы. Задаётся при сборке через build_flags
// (-DHW_LINK=\"esp32c3-super-mini\"). Fallback — для сборок без флага.
#ifndef HW_LINK
#define HW_LINK "esp32c3-super-mini"
#endif

#include <menu_commands.h>
#include <menu_cache.h>
#include <menu_publisher.h>  // idryer::MenuPublisher — pre-allocated публикатор меню

using namespace idryer;

// ── Пины UART (ESP32-C3 Super Mini, JTAG-shared → требуют gpio_reset_pin) ──
constexpr int UART_RX_PIN = 6;
constexpr int UART_TX_PIN = 7;

// ── SDK объекты ──────────────────────────────────────────────────────────────
static const iDryer::Config CFG = {
    .deviceType        = iDryer::DeviceType::Dryer,
    .unitsCount        = 1,  // реальное число физических юнитов на этом железе; уточняется из Hello RP2040
    .hasHeater         = true,
    .hasFan            = true,
    .hasLed            = false,
    .hasWeight         = true,
    .hasRfid           = true,
    .hasAirTemp        = true,
    .hasAirHumidity    = true,
    .hasHeaterTemp     = true,
    .hasServo          = true,  // заслонка сушилки (привод на стороне контроллера)
    .allowHa           = true,
    .allowBambu        = false,
    .allowMoonraker    = false,
    .telemetryPeriodMs     = 30000,
    .telemetryPeriodIdleMs = 60000,
    .statusPeriodMs        = 60000,  // сверка; изменения mode/target уходят сразу (SDK)
    .statusPeriodIdleMs    = 300000,
    // Сушилка: прерывание портит сушку и оставляет филамент под нагревом —
    // обновление ждёт простоя.
    .otaInterrupt = iDryer::OTA_INTERRUPT_DRYER_V3,
    .hardwareVersion   = HW_LINK,
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

// Алерт в портал при ошибках сборки/публикации конфига. Сырой JSON НЕ
// публикуем: портал его не понимает, а retained-публикация затирает последний
// хороший config на брокере — тихая ошибка хуже громкой.
static void publishMenuError(const char* event, const char* message) {
    HAL_LOG_ERROR("MENU", "%s: %s", event, message);
    StaticJsonDocument<192> ev;
    ev["severity"] = "ERROR";
    ev["source"]   = "MENU";
    ev["event"]    = event;
    ev["message"]  = message;
    ev["unitId"]   = "DEVICE";
    s_link.devicePublisher()->publishEvent(ev);
}

// ── Публикация delta (один-несколько изменённых пунктов) ─────────────────────
// json — сырой delta от RP2040: {"rev":N,"vals":{"7":[50]}}
// Используется когда ConfigReceiver::isDelta() (старший бит transferId).
// Канон в mqtt_contract.yaml (config_delta): {"rev":N,"d":{"7":[50]}} —
// поле 'd' вместо 'vals'. Перепаковываем перед publish.
static void publishConfigDelta(const char* json, uint16_t len) {
    if (!json || len == 0) return;

    // Обновляем g_menu_cache (для local-WS клиентов и других потребителей).
    if (!menu_parseDelta(json)) {
        publishMenuError("DELTA_PARSE_FAILED", "menu_parseDelta returned false");
        return;
    }

    // Парсим RP2040-формат и переименовываем "vals" → "d". 512 байт capacity
    // хватает на 1–3 изменённых per-unit пункта; на стеке.
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, json, len)) {
        publishMenuError("DELTA_DESERIALIZE_FAILED", "ArduinoJson deserialize error");
        return;
    }
    if (!doc.containsKey("vals")) {
        publishMenuError("DELTA_MISSING_VALS", "delta JSON has no 'vals' key");
        return;
    }
    doc["d"] = doc["vals"];
    doc.remove("vals");

    char buf[256];
    size_t out = serializeJson(doc, buf, sizeof(buf));
    if (out == 0) {
        publishMenuError("DELTA_RESERIALIZE_FAILED", "serializeJson returned 0");
        return;
    }

    s_link.devicePublisher()->publishConfigDelta(buf, out);
    HAL_LOG_INFO("MENU", "TX delta → MQTT: %u bytes", (unsigned)out);
}

// ── Вспомогательная функция публикации конфига ────────────────────────────────
// json — сырой JSON от RP2040: {v, full:true, vals:{...}}
// Парсим его в g_menu_cache, затем собираем {v, menu:[...]} для портала.
// При любой ошибке шлём алерт в портал и НЕ публикуем сырой JSON — он перетёр
// бы последний хороший retained config на брокере (портал raw-формат не парсит).
// Pre-allocated публикатор меню — один malloc на старте после s_link.begin(),
// переиспользуется на каждый publishConfig. Заменяет старую логику с malloc()
// MENU_FULL_JSON_BUF_SIZE на каждый вызов (фрагментировала heap при
// MQTT-reconnect'ах). См. menu_publisher.h.
static idryer::MenuPublisher s_menuPub;

static void publishConfig(const char* json, uint16_t len) {
    if (!json || len == 0) {
        publishMenuError("PUBLISH_EMPTY_INPUT", "publishConfig called with empty json");
        return;
    }

    // Парсим vals из RP2040 → обновляем g_menu_cache
    if (!menu_parseFullConfig(json)) {
        char msg[64];
        snprintf(msg, sizeof(msg), "menu_parseFullConfig failed (input %u bytes)", len);
        publishMenuError("PARSE_FAILED", msg);
        return;
    }

    // Pre-allocated buffer + DynamicJsonDocument (выделены в begin() после TLS).
    // Никаких malloc/free в горячем пути.
    size_t menuLen = s_menuPub.publishFull(s_link.devicePublisher());
    if (menuLen == 0) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "menuPub.publishFull returned 0 (init failed / overflow), heap free=%u largest=%u",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
        publishMenuError("PUBLISH_FAILED", msg);
        return;
    }
    HAL_LOG_INFO("MENU", "TX assembled → MQTT: %u bytes", (unsigned)menuLen);

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

// Публикует результат write_rfid в MQTT топик rfid/write_result.
// commandId эхо из запроса портала — портал по нему сопоставит ответ.
// status: "ok" | "failed". error: текст ошибки только для failed.
static void publishWriteResult(const char* commandId, const char* status, const char* error) {
    StaticJsonDocument<192> doc;
    doc["commandId"] = commandId ? commandId : "";
    doc["status"]    = status;
    if (error && *error) doc["error"] = error;
    s_link.devicePublisher()->publishRfidWriteResult(doc);
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
// Paired OTA Этап 6: bootValid должен дёрнуться ТОЛЬКО после первого Hello
// с правильным major (= RP подтвердила что мы paired-совместимы). Без этого
// при reboot ESP откатится bootloader'ом. Идемпотентный single-shot флаг.
static bool s_bootValidConfirmed = false;

static void requestConfig() {
    UartCmdPayload cmd{};
    cmd.command = UartCmdCode::GetConfig;
    cmd.unitId  = 0;  // 0xFF rejected by RP2040 (unitId >= NUM_UNITS check)
    s_uart.sendCommand(cmd, false);
}

// Форвард action-команды портала/локального WS на RP2040 с проброшенным origin.
// Локальные (WS в LAN) метятся UART_FLAG_LOCAL — гейт RP2040 ignore_external_cmd
// их пропускает; облачные идут без флага и гейтятся. Origin берём из SDK
// (валиден только внутри onCommand-хендлера). НЕ использовать для HA-кнопок и
// прочих не-onCommand источников — там currentCommandFromLocal() неактуален.
static uint8_t actionOriginFlag() {
    return s_link.currentCommandFromLocal() ? UART_FLAG_LOCAL : 0;
}
static void sendActionCommand(const UartCmdPayload& cmd) {
    s_uart.sendCommand(cmd, true, actionOriginFlag());
}

static void onHello(const UartHelloPayload& p, const UartFrameHeader&) {
    HAL_LOG_INFO("UART", "Hello: type=%u fw=%u units=%u serial=%s",
                 p.deviceType, p.firmwareVersion, p.unitsCount, p.mcuSerial);

    // Paired OTA Этап 6: подтверждаем boot ТОЛЬКО когда RP прислала Hello с
    // major == VERSION_MAJOR ESP. До этого момента bootloader-rollback
    // активен — если новая прошивка ESP несовместима с RP, следующий
    // power-cycle откатит ESP на старую. После подтверждения rollback
    // отключается (esp_ota_mark_app_valid_cancel_rollback идемпотентен).
    // Без таймаута: ждём сколько нужно (миграция EEPROM на RP может занять минуты).
    if (!s_bootValidConfirmed) {
        uint8_t rpMajor = (uint8_t)((p.firmwareVersion >> 16) & 0xFF);
        if (rpMajor == VERSION_MAJOR) {
            idryer::OtaReceiver::markCurrentBootValid();
            s_bootValidConfirmed = true;
            HAL_LOG_INFO("OTA", "Boot confirmed: RP major=%u matches ESP", rpMajor);
        } else {
            HAL_LOG_WARN("OTA", "Boot NOT confirmed: RP major=%u != ESP %u (rollback armed)",
                         rpMajor, VERSION_MAJOR);
        }
    }

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
    s_link.setMcuHardwareVersion(p.hardwareVersion);  // железо контроллера → info.mcuHardwareVersion
    s_link.setMcuWorkTimeCounter(p.workTimeCounter);  // наработка сушилки (RP2040) → info.workTimeCounter

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
    // [TEMP-DEBUG] Печатаем что реально пришло в payload — для отладки нулей в MQTT.
    Serial.printf("[RX-TELE] count=%u u0_id=%u u0_t10=%d u0_h10=%u u0_pwr=%u u0_fan=%u\n",
                  p.count, p.units[0].unitId, p.units[0].temperatureC10,
                  p.units[0].humidityPct10, p.units[0].heaterPowerPct, p.units[0].fanOn);
    for (uint8_t i = 0; i < p.count && i < iDryer::MAX_UNITS; i++) {
        const auto& e = p.units[i];
        if (e.unitId >= iDryer::MAX_UNITS) continue;
        s_link.telemetry.airTempC[e.unitId]      = e.getTemperature(); // sentinel → NaN (нет данных)
        s_link.telemetry.airHumidityPct[e.unitId]= e.getHumidity();
        s_link.telemetry.heaterTempC[e.unitId]   = e.getHeaterTemp(); // термистор нагревателя, sentinel → NaN
        s_link.telemetry.heaterPower01[e.unitId] = e.heaterPowerPct  / 100.0f;
        s_link.telemetry.fanOn[e.unitId]         = (e.fanOn != 0);
        s_link.telemetry.servoOpen[e.unitId]     = (e.servoOpen != 0);
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
        // Номер сессии считает RP2040 (пер-юнитовый счётчик в EEPROM). Link его
        // только транслирует: свой счётчик в RAM обнулялся бы при каждом ребуте
        // ESP32 и рвал живую сессию в БД (см. publishStatusNow в SDK).
        s_link.status.sessionNum[e.unitId] = e.sessionNum;
    }
    // Зеркалим device-wide флаг из RP2040 в SDK. Setter триггерит немедленный
    // publishStatusNow при изменении.
    s_link.setIgnoreExternalCmd(p.ignoreExternalCmd != 0);
    // Публикацию в облако решает SDK: сразу при изменении mode/target/duration,
    // иначе периодика-сверка (statusPeriodMs/statusPeriodIdleMs). Зеркалить
    // каждый UART-кадр в MQTT (как было) — 720 пустых status/час.
}

static void onWeights(const UartWeightsPayload& p, const UartFrameHeader&) {
    StaticJsonDocument<512> doc;
    JsonArray arr = doc.createNestedArray("weights");
    for (uint8_t i = 0; i < p.count && i < iDryer::MAX_UNITS; i++) {
        const auto& w = p.weights[i];
        if (w.unitId >= iDryer::MAX_UNITS) continue;
        char sid[4]; snprintf(sid, sizeof(sid), "W%u", (unsigned)(w.sensorId + 1));
        char uid[4]; snprintf(uid, sizeof(uid), "U%u", (unsigned)(w.unitId + 1));
        JsonObject o = arr.createNestedObject();
        o["sensorId"] = sid;
        o["value"]    = w.weightGramsC10 / 10.0f;
        o["unitId"]   = uid;
    }
    if (!arr.isNull() && arr.size() > 0)
        s_link.devicePublisher()->publishWeights(doc);
}

// RP2040 шлёт JSON меню фрагментами. ConfigReceiver склеивает, потом публикуем.
static void onConfigChunk(const UartConfigChunkPayload& p, uint8_t dataLen,
                          const UartFrameHeader& hdr) {
    auto result = s_configRx.processFragment(p, dataLen, hdr.flags);
    s_uart.sendConfigAck(hdr.sequence);
    // Diag (chunk-by-chunk): раскомментировать при отладке config-flow от RP2040.
    // HAL_LOG_INFO("MENU", "chunk: dataLen=%u flags=0x%02X result=%d total=%u",
    //              dataLen, hdr.flags, (int)result, s_configRx.getLength());
    if (result == ConfigFragResult::Complete) {
        const uint16_t len   = s_configRx.getLength();
        const char*    json  = s_configRx.getJson();
        const bool     delta = s_configRx.isDelta();
        // Diag (RX summary/preview/heap): раскомментировать при разборе проблем меню.
        // HAL_LOG_INFO("MENU", "RX from RP2040: %u bytes %s (capacity %u)",
        //              len, delta ? "DELTA" : "FULL", (unsigned)CONFIG_BUFFER_SIZE);
        // HAL_LOG_INFO("MENU", "RX preview: %.200s%s", json ? json : "(null)",
        //              (len > 200) ? "..." : "");
        if (delta) {
            publishConfigDelta(json, len);
        } else {
            // HAL_LOG_INFO("MENU", "heap before publishConfig: free=%u largest=%u",
            //              (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
            //              (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
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

    // Строим JSON вручную, чтобы сохранить все поля:
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
    // binding-v3: привязку начинает владелец из приложения, не устройство.
    // Кнопка в меню RP2040 пока есть — отвечаем состоянием, чтобы контроллер
    // не ждал PIN, которого больше не будет.
    HAL_LOG_INFO("UART", "ClaimStart from MCU — ignored (binding-v3: app owns pairing)");
    UartClaimStatusPayload sp{};
    sp.status = s_link.isBound() ? UartClaimStatus::Claimed : UartClaimStatus::Error;
    s_uart.sendClaimStatus(sp);
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
        sendActionCommand(cmd);
    });

    s_link.onCommand("stop", [](JsonObjectConst data) {
        UartCmdPayload cmd{};
        cmd.command = UartCmdCode::Stop;
        cmd.unitId  = parseUnitId(data);
        sendActionCommand(cmd);
    });

    s_link.onCommand("find", [](JsonObjectConst data) {
        UartCmdPayload cmd{};
        cmd.command = UartCmdCode::Find;
        cmd.unitId  = parseUnitId(data);
        sendActionCommand(cmd);
    });

    // binding-v3: портал отвязал устройство (retained REVOKE на commands/revoke)
    // → стереть секрет из NVS и вернуться к ожиданию токена привязки (SETUP).
    s_link.onCommand("revoke", [](JsonObjectConst) {
        s_link.handleRevoke();
    });

    s_link.onCommand("clear_errors", [](JsonObjectConst data) {
        // Бэкенд может слать unitId как строку "U1" — ArduinoJson не конвертирует в uint8_t,
        // возвращает 0xFF. RP2040 отклоняет unitId >= NUM_UNITS, поэтому при 0xFF чистим все юниты.
        uint8_t uid = data["unitId"] | (uint8_t)0xFF;
        if (uid < iDryer::MAX_UNITS) {
            UartCmdPayload cmd{};
            cmd.command = UartCmdCode::ClearErrors;
            cmd.unitId  = uid;
            sendActionCommand(cmd);
        } else {
            for (uint8_t i = 0; i < iDryer::MAX_UNITS; i++) {
                UartCmdPayload cmd{};
                cmd.command = UartCmdCode::ClearErrors;
                cmd.unitId  = i;
                sendActionCommand(cmd);
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
        sendActionCommand(cmd);
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
        s_uart.sendProfileCommand(p, true, actionOriginFlag());
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

    // write_rfid: Variant B контракта (see write_rfid_payload_mismatch).
    // 1) парсим JSON от портала, 2) base64 → bytes, 3) UART WriteRfid + ACK,
    // 4) фрагменты по 163 байта stop-and-wait, 5) публикуем rfid/write_result.
    s_link.onCommand("write_rfid", [](JsonObjectConst data) {
        // Этап 1. Парсинг JSON.
        const char* commandId = data["commandId"] | "";
        if (!*commandId) {
            HAL_LOG_WARN("RFID", "write_rfid: missing commandId — cannot respond");
            return;
        }
        uint8_t unitId = parseUnitId(data);
        if (unitId >= iDryer::MAX_UNITS) {
            // Fallback: payload с unitId как int 0..3.
            JsonVariantConst v = data["unitId"];
            if (v.is<int>()) {
                int u = v.as<int>();
                if (u >= 0 && u < iDryer::MAX_UNITS) unitId = (uint8_t)u;
            }
        }
        if (unitId >= iDryer::MAX_UNITS) {
            publishWriteResult(commandId, "failed", "invalid unitId");
            return;
        }
        const char* b64 = data["data"] | (const char*)nullptr;
        if (!b64 || !*b64) {
            publishWriteResult(commandId, "failed", "missing data");
            return;
        }

        // Variant B новые поля — пока только логируем (см. долг #4 в отчёте).
        const char* expectedUid = data["expectedUid"] | (const char*)nullptr;
        uint32_t    ttlMs       = data["ttlMs"] | 15000u;
        HAL_LOG_INFO("RFID", "write_rfid: cmd=%s unit=U%u expectedUid=%s ttl=%ums",
                     commandId, unitId + 1,
                     expectedUid ? expectedUid : "(none)", (unsigned)ttlMs);
        // TODO(expectedUid): MCU должен валидировать UID метки перед записью
        //                    (требует доработки iDryerControllerV2 — добавить поле в UART).
        // TODO(ttlMs):       сейчас используем фиксированный 200мс на ACK; полная
        //                    операция укладывается в ~1.5с. Если портал станет
        //                    слать ttlMs < 2000 — нужно учитывать.

        // Этап 2. Base64 decode → raw bytes (макс 888 байт для RFID).
        constexpr size_t kMaxRfidBytes = 888;
        uint8_t raw[kMaxRfidBytes] = {};
        size_t rawLen = 0;
        int rc = mbedtls_base64_decode(raw, sizeof(raw), &rawLen,
                                       reinterpret_cast<const unsigned char*>(b64),
                                       strlen(b64));
        if (rc != 0 || rawLen == 0 || rawLen > kMaxRfidBytes) {
            char err[64];
            snprintf(err, sizeof(err), "base64 decode failed rc=%d len=%u",
                     rc, (unsigned)rawLen);
            publishWriteResult(commandId, "failed", err);
            return;
        }

        // Этап 3. UART команда WriteRfid + ACK от MCU.
        constexpr uint32_t kAckTimeoutMs   = 200;
        constexpr uint32_t kVerifyHeader32 = 1;  // дефолт legacy
        UartCmdPayload cmd{};
        cmd.command = UartCmdCode::WriteRfid;
        cmd.unitId  = unitId;
        cmd.arg0    = (uint32_t)rawLen;
        cmd.arg1    = kVerifyHeader32;
        s_uart.sendCommand(cmd, true);  // ackRequired = true
        if (!s_uart.waitForAck(kAckTimeoutMs)) {
            publishWriteResult(commandId, "failed", "command ack timeout");
            return;
        }

        // Этап 4. Фрагменты данных (stop-and-wait, кусок 163 байта).
        constexpr size_t kFragSize = 163;
        const size_t fragCount = (rawLen + kFragSize - 1) / kFragSize;
        for (size_t f = 0; f < fragCount; f++) {
            UartRfidDataPayload frag{};
            frag.readerId = 0xFF;  // bridge не знает mapping slot→reader
            frag.unitId   = unitId;
            const size_t srcOff = f * kFragSize;
            size_t copyLen = rawLen - srcOff;
            if (copyLen > kFragSize) copyLen = kFragSize;
            memcpy(frag.fragment, raw + srcOff, copyLen);

            const bool isLast = (f == fragCount - 1);
            uint8_t flags = UART_FLAG_ACK_REQ;
            flags |= isLast ? UART_FLAG_LAST_FRAGMENT : UART_FLAG_FRAGMENT;
            s_uart.sendRfidWriteData(frag, flags);

            if (!s_uart.waitForAck(kAckTimeoutMs)) {
                char err[64];
                snprintf(err, sizeof(err), "frag[%u/%u] ack timeout",
                         (unsigned)f, (unsigned)fragCount);
                publishWriteResult(commandId, "failed", err);
                return;
            }
        }

        // Этап 5. Успех.
        HAL_LOG_INFO("RFID", "write_rfid: cmd=%s OK (%u bytes, %u frags)",
                     commandId, (unsigned)rawLen, (unsigned)fragCount);
        publishWriteResult(commandId, "ok", nullptr);
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

    s_link.setWaitForMcuSerial(true);
    s_link.begin();

    // Не шлём device-timestamp в publish: портал хранит своё серверное время
    // приёма, device-timestamp избыточен (экономия трафика). См. MqttClient.
    s_link.mqttClient()->setAddTimestamp(false);

    // Pre-allocate MenuPublisher СРАЗУ после s_link.begin() — TLS-handshake уже
    // прошёл и contiguous heap максимально свободен. ~37КБ на одну аллокацию
    // (MENU_SERIALIZED_MAX_SIZE + DynamicJsonDocument capacity). Без этого
    // publishConfig() сразу упадёт с PUBLISH_FAILED.
    if (!s_menuPub.begin()) {
        HAL_LOG_ERROR("MENU",
                      "MenuPublisher.begin() failed, heap free=%u largest=%u",
                      (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
                      (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
        // Не выходим — продукт продолжает работу, но publishConfig вернёт false
        // до перезагрузки. Лог будет в админке через publishMenuError.
    }

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

    // Phase 6 OTA: target=esp — self-flash через Update lib; target=rp2040 —
    // UART-proxy в RP (DRYER paired OTA, Этап 2). UartBridge передаём для
    // активации rp2040-ветки. markCurrentBootValid НЕ зовём в этой точке —
    // для DRYER это будет условно после Hello-handshake (Этап 6).
    idryer::OtaReceiver::instance().begin(&s_link, "idryer_link", &s_uart, VERSION_MAJOR);

    HAL_LOG_INFO("MAIN", "iDryer Link v2 ready, fw=%s", VERSION_STR);
}

// Режим настройки Wi-Fi → контроллеру: его LCD показывает QR. Обычный heartbeat
// идёт через s_link.every(), а планировщик до подъёма сети не крутится, поэтому
// здесь отдельная отправка — только пока ESP слушает эфир: сразу при входе в
// режим и дальше с периодом heartbeat.
static void sendWifiSetupHeartbeat() {
    static bool     s_wasSetup = false;
    static uint32_t s_lastMs   = 0;
    const bool     setup = idryer::EspTouchProvisioner::instance().isActive();
    const uint32_t now   = millis();
    if (setup && (!s_wasSetup || now - s_lastMs >= idryer::UART_HEARTBEAT_MS)) {
        UartHeartbeatPayload hb{};
        hb.uptimeSeconds = now / 1000;
        hb.cloudState    = idryer::UartLinkCloudState::WifiSetup;
        s_uart.sendHeartbeat(hb);
        s_lastMs = now;
    }
    s_wasSetup = setup;
}

void loop() {
    s_link.loop();
    s_uart.loop();
    // Paired OTA Этап 5: периодически шлём RP актуальный espReady-статус.
    idryer::OtaReceiver::instance().tick(millis());
    sendWifiSetupHeartbeat();

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
