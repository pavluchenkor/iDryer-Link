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
#include "IdryerDevice.h"
#include "WsServer.h"
#include <ArduinoJson.h>
#include <ImprovWiFiLibrary.h>
#include <Preferences.h>

// Menu библиотека для кэширования конфига
#include <menu_commands.h>
#include <menu_cache.h>
#include <menu_meta.h>

#include "secrets.h"
#include "version.h"

using namespace DryerUart;
using namespace idryer;
using namespace idryer::hal;

// =============================================================================
// DEBUG МАКРОСЫ (runtime-условные, зависят от logsEnabled)
// =============================================================================

#define ANSI_RESET "\033[0m"
#define ANSI_GREEN "\033[32m"

#define DEBUG_LOG(...)                  \
    do                                  \
    {                                   \
        if (logsEnabled)                \
            Serial.printf(__VA_ARGS__); \
    } while (0)

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

    // =============================================================================
    // ГЛОБАЛЬНЫЕ ОБЪЕКТЫ
    // =============================================================================

    // HAL Serial для UART (ESP32-C3: Serial1 = UART_NUM_1)
    ArduinoSerial uartSerial(Serial1, 1);
    UartBridge uartBridge;

    // Платформенные реализации
    ArduinoWifiManager wifiManager;
    ArduinoHttpClient httpClient;
    ArduinoCredentialStore credStore;

    // Главный фасад устройства
    IdryerDevice device(&wifiManager, &httpClient, &credStore, &uartBridge, IDRYER_API_BASE);

    // WebSocket сервер для локального доступа
    WsServer wsServer(&uartBridge);

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

    // =============================================================================
    // WEBSERIAL CLAIMING (для веб-морды install.idryer.org)
    // =============================================================================

    char currentClaimPin[10] = "";
    uint32_t claimPinExpiresIn = 0;

    /**
     * @brief Callback когда получен PIN от backend
     *
     * PIN выводится в Serial для веб-морды в формате: CLAIM_PIN:<pin>:<expires>
     * Отправка PIN на RP2040 происходит автоматически внутри библиотеки.
     */
    void onWebClaimPin(const char *pin, uint32_t expiresInSeconds)
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

    void onConfigReceived(const char *json, uint16_t length, bool isDelta)
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

void setup()
{
    Serial.begin(115200);

    initArduinoHal(nullptr);
    // Отключаем JTAG для использования GPIO6/7
    gpio_reset_pin((gpio_num_t)UART_RX_PIN);
    gpio_reset_pin((gpio_num_t)UART_TX_PIN);

    // Инициализируем UART1 для связи с RP2040
    Serial1.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

    // Инициализируем UART Bridge
    uartBridge.begin(&uartSerial, 115200);

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
#if defined(IDRYER_WIFI_SSID) && defined(IDRYER_WIFI_PASSWORD)
    else
    {
        wifiManager.begin(IDRYER_WIFI_SSID, IDRYER_WIFI_PASSWORD);
        saveWiFiCredentials(IDRYER_WIFI_SSID, IDRYER_WIFI_PASSWORD);
        wifiConfigured = true;
    }
#endif

    // device.begin() регистрирует все UART обработчики и запускает облачную логику
    device.begin();

    // Подключаем WS сервер к фасаду (WS активируется позже по UART команде WsEnable)
    device.setWsServer(&wsServer);

    // WS команды идут через тот же CommandHandler что и MQTT
    wsServer.setCommandCallback([](const char *command, JsonObjectConst data)
                                { device.handleExternalCommand(command, data); });

    // Callback для получения конфига от MCU
    device.setConfigReceivedCallback(onConfigReceived);

    // Callback для получения PIN (WebSerial claiming)
    device.setClaimPinCallback(onWebClaimPin);

    // Авто-refresh deviceToken при WS invalid_token:
    // ESP32 делает re-provision на портал и получает актуальный токен.
    // Приложение параллельно делает retry через ~2-3 сек — к тому времени токен обновлён.
    wsServer.setTokenRefreshCallback([&]()
                                     {
        auto* csm = device.getCloudStateMachine();
        if (!csm) return;
        HAL_LOG_INFO("DEVICE", "WS auth fail → auto-refreshing token from portal...");
        if (csm->refreshToken()) {
            wsServer.updateToken(csm->getIdentity().token);
            HAL_LOG_INFO("DEVICE", "WS token auto-refreshed OK");
        } else {
            HAL_LOG_WARN("DEVICE", "WS token refresh failed (no WiFi, no serial, or cooldown)");
        } });
}

void loop()
{
    device.loop(); // UART + heartbeat + cloud + публикация данных

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
            Serial.printf("[BOOT] FW=%s  UART_PROTO=%d\n", VERSION_STR, DryerUart::PROTOCOL_VERSION);
            Serial.println("[BOOT] Logs enabled after WiFi config");
            Serial.println("========================================");
            HAL_LOG_INFO("CLOUD", "WiFi connected, logs enabled");
            HAL_LOG_INFO("CLOUD", "WiFi connected, IP: %s, RSSI: %d dBm",
                         WiFi.localIP().toString().c_str(), WiFi.RSSI());
        }
    }
    else
    {
        processWebSerialCommands();
    }
}
