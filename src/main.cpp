/**
 * @file main.cpp
 * @brief Главный файл iDryer Link (ESP32-C3)
 *
 * iDryer Link - сетевой мост между RP2040 контроллером и облаком.
 * Обеспечивает:
 * - WiFi подключение (через Improv Wi-Fi)
 * - MQTT коммуникацию с backend
 * - UART протокол с RP2040
 * - Claiming (привязка устройства через WebSerial)
 */

#include <Arduino.h>
#include <idryer_protocol.h>
#include <platform/arduino/idryer_arduino.h>
#include <ArduinoJson.h>
#include <ImprovWiFiLibrary.h>
#include <Preferences.h>

// Menu библиотека для кэширования конфига
#include <menu_commands.h>
#include <menu_cache.h>
#include <menu_meta.h>

#include "secrets.h"
#include "version.h"

// Используем namespace'ы библиотеки
using namespace DryerUart;
using namespace idryer;
using namespace idryer::hal;

// =============================================================================
// DEBUG МАКРОСЫ (runtime-условные, зависят от logsEnabled)
// =============================================================================

#define ANSI_RESET "\033[0m"
#define ANSI_BLUE "\033[34m"
#define ANSI_GREEN "\033[32m"
#define ANSI_YELLOW "\033[33m"

// Логи работают только когда Serial свободен от Improv протокола
#define DEBUG_LOG(...) do { if (logsEnabled) Serial.printf(__VA_ARGS__); } while(0)
#define DEBUG_JSON(doc) do { if (logsEnabled) { serializeJson(doc, Serial); Serial.println(); } } while(0)

namespace
{
    // ESP32-C3 UART пины для связи с RP2040
    constexpr int UART_RX_PIN = 6;
    constexpr int UART_TX_PIN = 7;

    // Improv Wi-Fi (настройка WiFi через браузер)
    Preferences preferences;
    ImprovWiFi improvSerial(&Serial);
    bool wifiConfigured = false;
    bool logsEnabled = false; // Логи включаются после настройки WiFi (Serial освобождается от Improv)

    // =========================================================================
    // WiFi credentials (Improv + NVS)
    // =========================================================================

    void saveWiFiCredentials(const char *ssid, const char *password)
    {
        preferences.begin("wifi", false);
        preferences.putString("ssid", ssid);
        preferences.putString("password", password);
        preferences.putBool("configured", true);
        preferences.end();
        DEBUG_LOG("[IMPROV] WiFi credentials saved\n");
    }

    bool loadWiFiCredentials(String &ssid, String &password)
    {
        preferences.begin("wifi", true);
        bool configured = preferences.getBool("configured", false);
        if (configured)
        {
            ssid = preferences.getString("ssid", "");
            password = preferences.getString("password", "");
        }
        preferences.end();
        return configured && ssid.length() > 0;
    }

    void clearWiFiCredentials()
    {
        preferences.begin("wifi", false);
        preferences.clear();
        preferences.end();
        DEBUG_LOG("[IMPROV] WiFi credentials cleared\n");
    }

    constexpr uint32_t FIRMWARE_VERSION = VERSION_NUMBER;

    // =============================================================================
    // ГЛОБАЛЬНЫЕ ОБЪЕКТЫ
    // =============================================================================

    // HAL Serial для UART (ESP32-C3: Serial1 = UART_NUM_1)
    ArduinoSerial uartSerial(Serial1, 1);

    // Протокольные компоненты
    UartBridge uartBridge;

    // Платформенные реализации
    ArduinoWifiManager wifiManager;
    ArduinoHttpClient httpClient;
    ArduinoCredentialStore credStore;

    // Главный фасад устройства
    IdryerDevice device(&wifiManager, &httpClient, &credStore, &uartBridge, IDRYER_API_BASE);

    void onImprovWiFiConnectCallback(const char *ssid, const char *password)
    {
        DEBUG_LOG("[IMPROV] Received credentials - SSID: %s\n", ssid);
        saveWiFiCredentials(ssid, password);
        wifiManager.begin(ssid, password);
        wifiConfigured = true;
    }

    void onImprovWiFiErrorCallback(ImprovTypes::Error err)
    {
        DEBUG_LOG("[IMPROV] Error: %d\n", err);
    }

    uint32_t lastHeartbeatAt = 0;
    uint32_t heartbeatErrors = 0;

    // =============================================================================
    // UART ОБРАБОТЧИКИ
    // =============================================================================

    void handleHello(const HelloPayload &payload, const FrameHeader &header)
    {
        device.handleRpHello(payload, header);
    }

    void handleTelemetry(const TelemetryPayload &payload, const FrameHeader &header)
    {
        DEBUG_LOG("\n" ANSI_BLUE "← Telemetry (seq=%d, count=%d)" ANSI_RESET "\n",
                  header.sequence, payload.count);
        if (logsEnabled)
        {
            StaticJsonDocument<512> doc;
            JsonArray units = doc.createNestedArray("units");
            for (uint8_t i = 0; i < payload.count && i < 4; i++)
            {
                JsonObject unit = units.createNestedObject();
                unit["unitId"] = payload.units[i].unitId;
                unit["tempC"] = payload.units[i].temperatureC10 / 10.0;
                unit["humidity"] = payload.units[i].humidityPct10 / 10.0f;
            }
            DEBUG_JSON(doc);
        }
        device.handleTelemetry(payload, header);
    }

    void handleCommandAck(const AckPayload &payload, const FrameHeader &header)
    {
        device.handleCommandAck(payload, header);
        if (payload.status != ErrorCode::None)
        {
            heartbeatErrors++;
        }
    }

    void handleConfigAck(const AckPayload &payload, const FrameHeader &header)
    {
        device.handleConfigAck(payload, header);
        if (payload.status != ErrorCode::None)
        {
            heartbeatErrors++;
        }
    }

    void handleConfigPushChunk(const ConfigChunkPayload &payload, uint8_t dataLen, const FrameHeader &header)
    {
        DEBUG_LOG("\n" ANSI_BLUE "← ConfigPush chunk (transferId=%d, chunk=%d, dataLen=%d)" ANSI_RESET "\n",
                  payload.header.transferId, payload.header.chunkIndex, dataLen);
        device.handleConfigPushChunk(payload, dataLen, header);
    }

    void handleHeartbeat(const HeartbeatPayload &payload, const FrameHeader &header)
    {
        DEBUG_LOG("\n" ANSI_BLUE "← Heartbeat (seq=%d)" ANSI_RESET "\n", header.sequence);
        if (logsEnabled)
        {
            StaticJsonDocument<128> doc;
            doc["uptime"] = payload.uptimeSeconds;
            doc["rssi"] = payload.wifiRssiDbm;
            doc["errors"] = payload.errorsSinceBoot;
            DEBUG_JSON(doc);
        }
    }

    void handleError(const ErrorPayload &payload, bool remote)
    {
        device.handleUartError(payload, remote);
        heartbeatErrors++;
    }

    void handleLog(const uint8_t *payload, uint8_t length)
    {
        if (length < sizeof(LogPayload))
        {
            DEBUG_LOG("Log message too short: %d bytes\n", length);
            return;
        }

        const LogPayload *log = reinterpret_cast<const LogPayload *>(payload);
        device.handleLog(log);
    }

    void handleStatus(const StatusPayload &payload, const FrameHeader &header)
    {
        DEBUG_LOG("\n" ANSI_BLUE "← Status (seq=%d, count=%d)" ANSI_RESET "\n",
                  header.sequence, payload.count);
        if (logsEnabled)
        {
            StaticJsonDocument<1024> doc;
            doc["uptime"] = payload.uptime;
            JsonArray units = doc.createNestedArray("units");
            for (uint8_t i = 0; i < payload.count && i < 4; i++)
            {
                JsonObject unit = units.createNestedObject();
                unit["unitId"] = payload.units[i].unitId;
                unit["mode"] = static_cast<int>(payload.units[i].mode);
                unit["sessionNum"] = payload.units[i].sessionNum;
                unit["elapsed"] = payload.units[i].elapsedSeconds;
                unit["remaining"] = payload.units[i].totalRemainingSeconds;
                unit["stage"] = payload.units[i].currentStage;
            }
            DEBUG_JSON(doc);
        }
        device.handleStatus(payload, header);
    }

    void handleWeights(const WeightsPayload &payload, const FrameHeader &header)
    {
        DEBUG_LOG("\n" ANSI_BLUE "← Weights (seq=%d, count=%d)" ANSI_RESET "\n",
                  header.sequence, payload.count);
        if (logsEnabled)
        {
            StaticJsonDocument<256> doc;
            JsonArray weights = doc.createNestedArray("weights");
            for (uint8_t i = 0; i < payload.count && i < 4; i++)
            {
                JsonObject w = weights.createNestedObject();
                w["unitId"] = payload.weights[i].unitId;
                w["weight"] = payload.weights[i].weightGramsC10;
            }
            DEBUG_JSON(doc);
        }
        device.handleWeights(payload, header);
    }

    void handleRfid(const RfidPayload &payload, const FrameHeader &header)
    {
        DEBUG_LOG("\n" ANSI_BLUE "← RFID (seq=%d)" ANSI_RESET "\n", header.sequence);
        if (logsEnabled)
        {
            StaticJsonDocument<256> doc;
            doc["event"] = static_cast<int>(payload.event);
            doc["readerId"] = payload.readerId;
            doc["unitId"] = payload.unitId;
            doc["tag"] = String(payload.tag);
            DEBUG_JSON(doc);
        }
        device.handleRfidEvent(payload, header);
    }

    void processHeartbeat()
    {
        const uint32_t now = millis();
        if (now - lastHeartbeatAt < HEARTBEAT_INTERVAL_MS)
        {
            return;
        }

        HeartbeatPayload payload{};
        payload.uptimeSeconds = now / 1000;
        payload.wifiRssiDbm = 0;
        payload.errorsSinceBoot = heartbeatErrors;
        uartBridge.sendHeartbeat(payload);

        lastHeartbeatAt = now;
    }

    void sendInitialHello()
    {
        DEBUG_LOG(ANSI_GREEN "→ Sending Hello from ESP32..." ANSI_RESET "\n");
        HelloPayload payload{};
        payload.role = Role::EspBridge;
        payload.firmwareVersion = FIRMWARE_VERSION;
        payload.workTimeCounter = millis() / 1000;
        strncpy(payload.hardwareVersion, "v1.0", sizeof(payload.hardwareVersion) - 1);
        uartBridge.sendHello(payload, false);
    }

    void handleClaimStart(const FrameHeader &header)
    {
        DEBUG_LOG("\n" ANSI_BLUE "← ClaimStart from RP2040 (seq=%d)" ANSI_RESET "\n",
                  header.sequence);
        bool result = device.requestClaimProcess();
        DEBUG_LOG("[CLAIM] requestClaimProcess() returned: %s\n",
                  result ? "true" : "false");
    }

    // =============================================================================
    // WEBSERIAL CLAIMING (для веб-морды install.idryer.org)
    // =============================================================================

    char currentClaimPin[10] = "";  // Текущий PIN для отображения
    uint32_t claimPinExpiresIn = 0; // Время жизни PIN в секундах

    /**
     * @brief Callback когда получен PIN от backend
     *
     * PIN выводится в Serial для веб-морды в формате: CLAIM_PIN:<pin>:<expires>
     * И также отправляется на RP2040 через UART.
     */
    void onWebClaimPin(const char *pin, uint32_t expiresInSeconds, void *ctx)
    {
        strncpy(currentClaimPin, pin, sizeof(currentClaimPin) - 1);
        currentClaimPin[sizeof(currentClaimPin) - 1] = '\0';
        claimPinExpiresIn = expiresInSeconds;

        // Выводим PIN в Serial для веб-морды
        Serial.print("CLAIM_PIN:");
        Serial.print(pin);
        Serial.print(":");
        Serial.println(expiresInSeconds);
        Serial.flush();

        DEBUG_LOG("[WEB_CLAIM] PIN sent to Serial: %s (expires in %ds)\n", pin, expiresInSeconds);

        // Также отправляем PIN на RP2040 через UART
        DryerUart::ClaimStatusPayload payload{};
        payload.status = DryerUart::ClaimingStatus::WaitingClaim;
        strncpy(payload.pin, pin, sizeof(payload.pin) - 1);
        payload.pin[sizeof(payload.pin) - 1] = '\0';
        payload.expiresAt = 0; // TODO: вычислить timestamp
        payload.remainingSeconds = expiresInSeconds;

        uartBridge.sendClaimStatus(payload);
        DEBUG_LOG("[WEB_CLAIM] PIN sent to RP2040\n");
    }

    /**
     * @brief Обработчик команды START_CLAIM от веб-морды
     */
    void handleWebSerialCommand(const String &line)
    {
        if (line.equalsIgnoreCase("START_CLAIM"))
        {
            DEBUG_LOG("[WEB_CLAIM] Received START_CLAIM command from web\n");

            bool result = device.requestClaimProcess();

            if (result)
            {
                auto *csm = device.getCloudStateMachine();
                if (csm && csm->getState() == cloud::CloudState::Ready)
                {
                    const char *serial = csm->getIdentity().serialNumber;
                    Serial.printf("CLAIM_ALREADY:%s\n", serial);
                    DEBUG_LOG("[WEB_CLAIM] Device already claimed, serial=%s\n", serial);
                }
                else
                {
                    Serial.println("CLAIM_STARTED:OK");
                    DEBUG_LOG("[WEB_CLAIM] Claim process started successfully\n");
                }
            }
            else
            {
                Serial.println("CLAIM_STARTED:ERROR");
                DEBUG_LOG("[WEB_CLAIM] Failed to start claim process\n");
            }
            Serial.flush();
        }
    }

    /**
     * @brief Чтение и обработка команд из Serial (для веб-морды)
     * Работает только когда logsEnabled = true (Serial свободен от Improv)
     */
    void processWebSerialCommands()
    {
        if (!logsEnabled)
            return;

        if (Serial.available() > 0)
        {
            String line = Serial.readStringUntil('\n');
            line.trim();

            if (line.length() > 0)
            {
                handleWebSerialCommand(line);
            }
        }
    }

    // =============================================================================
    // MENU CONFIG CALLBACK
    // =============================================================================

    void printMenuItem(uint16_t id, int depth)
    {
        const MenuMeta *meta = menu_meta_get(id);
        if (!meta)
            return;

        for (int i = 0; i < depth; i++)
            DEBUG_LOG("  ");

        uint8_t lang = g_menu_cache.getLang();
        const char *name = meta->title[lang] ? meta->title[lang] : "?";
        const char *unit = meta->unit[lang] ? meta->unit[lang] : "";

        switch (meta->type)
        {
        case META_SUBMENU:
            DEBUG_LOG("[%s]\n", name);
            for (uint16_t childId = 0; childId < MENU_META_COUNT; childId++)
            {
                const MenuMeta *child = menu_meta_get(childId);
                if (child && child->parent == (int16_t)id)
                {
                    printMenuItem(childId, depth + 1);
                }
            }
            break;

        case META_ACTION:
            DEBUG_LOG("%s (action)\n", name);
            break;

        case META_VALUE:
        case META_TOGGLE:
            if (meta->scope == META_SCOPE_GLOBAL)
            {
                if (meta->type == META_TOGGLE)
                {
                    DEBUG_LOG("%s = %s\n", name,
                              g_menu_cache.getBool(id, 0) ? "ON" : "OFF");
                }
                else
                {
                    DEBUG_LOG("%s = %.1f %s\n", name,
                              g_menu_cache.getFloat(id, 0), unit);
                }
            }
            else
            {
                DEBUG_LOG("%s = [", name);
                for (uint8_t u = 0; u < g_menu_cache.getUnitsCount(); u++)
                {
                    if (u > 0)
                        DEBUG_LOG(", ");
                    if (meta->type == META_TOGGLE)
                    {
                        DEBUG_LOG("%s", g_menu_cache.getBool(id, u) ? "ON" : "OFF");
                    }
                    else
                    {
                        DEBUG_LOG("%.1f", g_menu_cache.getFloat(id, u));
                    }
                }
                DEBUG_LOG("] %s\n", unit);
            }
            break;
        }
    }

    void printMenuCache()
    {
        DEBUG_LOG("\n--- MENU CACHE ---\n");
        DEBUG_LOG("Version: %d, Units: %d, Active: %d, Lang: %s\n\n",
                  g_menu_cache.revision,
                  g_menu_cache.getUnitsCount(),
                  g_menu_cache.active_unit,
                  g_menu_cache.getLang() == 0 ? "RU" : "EN");

        printMenuItem(0, 0);

        DEBUG_LOG("------------------\n");
    }

    void onConfigReceived(const char *json, uint16_t length, bool isDelta, void *ctx)
    {
        DEBUG_LOG("\n" ANSI_GREEN "← Config received: %d bytes, isDelta=%d" ANSI_RESET "\n",
                  length, isDelta);

        bool ok = false;
        if (isDelta)
        {
            ok = menu_parseDelta(json);
            DEBUG_LOG("[MENU] Delta parsed: %s\n", ok ? "OK" : "FAIL");
        }
        else
        {
            ok = menu_parseFullConfig(json);
            DEBUG_LOG("[MENU] Full config parsed: %s, units=%d, active=%d\n",
                      ok ? "OK" : "FAIL",
                      g_menu_cache.getUnitsCount(),
                      g_menu_cache.active_unit);
        }

        if (ok)
        {
            printMenuCache();
        }
    }

} // namespace

// =============================================================================
// SETUP
// =============================================================================

void setup()
{
    Serial.begin(115200);
    while (!Serial)
    {
        delay(10);
    }
    delay(500);

    // При старте Improv использует Serial — HAL логи отключены
    initArduinoHal(nullptr);

    // Отключаем JTAG для использования GPIO6/7
    gpio_reset_pin((gpio_num_t)UART_RX_PIN);
    gpio_reset_pin((gpio_num_t)UART_TX_PIN);

    // Инициализируем UART1 для связи с RP2040
    Serial1.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

    // Инициализируем UART Bridge
    uartBridge.begin(&uartSerial, 115200);

    // Регистрируем обработчики UART сообщений
    uartBridge.setHelloHandler(handleHello);
    uartBridge.setTelemetryHandler(handleTelemetry);
    uartBridge.setCommandAckHandler(handleCommandAck);
    uartBridge.setConfigAckHandler(handleConfigAck);
    uartBridge.setConfigPushChunkHandler(handleConfigPushChunk);
    uartBridge.setHeartbeatHandler(handleHeartbeat);
    uartBridge.setErrorHandler(handleError);
    uartBridge.setLogHandler(handleLog);
    uartBridge.setStatusHandler(handleStatus);
    uartBridge.setWeightsHandler(handleWeights);
    uartBridge.setRfidHandler(handleRfid);
    uartBridge.setClaimStartHandler(handleClaimStart);

    // Инициализация Improv Wi-Fi
    improvSerial.setDeviceInfo(
        ImprovTypes::ChipFamily::CF_ESP32_C3,
        "iDryer Link",
        VERSION_STRING,
        "iDryer",
        "");

    improvSerial.onImprovConnected(onImprovWiFiConnectCallback);
    improvSerial.onImprovError(onImprovWiFiErrorCallback);

    // Проверяем, есть ли сохранённые WiFi credentials
    String savedSSID, savedPassword;
    if (loadWiFiCredentials(savedSSID, savedPassword))
    {
        wifiManager.begin(savedSSID.c_str(), savedPassword.c_str());
        wifiConfigured = true;
    }

    device.begin();

    // Callback для получения конфига от MCU
    device.setConfigReceivedCallback(onConfigReceived);

    // Callback для получения PIN (WebSerial claiming)
    device.getCloudStateMachine()->setClaimPinCallback(onWebClaimPin, nullptr);

    sendInitialHello();
}

// =============================================================================
// LOOP
// =============================================================================

void loop()
{
    uartBridge.loop();
    processHeartbeat();
    device.loop();

    // Improv работает пока WiFi не настроен
    if (!logsEnabled)
    {
        improvSerial.handleSerial();

        // После подключения к WiFi — Serial свободен для логов и WebSerial команд
        if (wifiConfigured && WiFi.status() == WL_CONNECTED)
        {
            logsEnabled = true;
            initArduinoHal(&Serial);
            Serial.println("\n========================================");
            Serial.println("[BOOT] Logs enabled after WiFi config");
            Serial.println("========================================");
            HAL_LOG_INFO("CLOUD", "WiFi connected, logs enabled");
        }
    }
    else
    {
        // Serial свободен — обрабатываем WebSerial команды
        processWebSerialCommands();
    }
}
