/**
 * @file menu_commands.h
 * @brief Парсинг JSON конфигов и формирование команд для MCU
 *
 * Этот модуль не знает про транспорт (UART/MQTT).
 * Только работает с JSON строками и g_menu_cache.
 *
 * @example
 * // Парсинг полного конфига от MCU:
 * menu_parseFullConfig(jsonFromMcu);
 * float temp = g_menu_cache.getFloat(3); // dry_temp
 *
 * // Формирование команды для MCU:
 * char buf[64];
 * menu_buildSetCommand(buf, sizeof(buf), 3, 0, 55.0f);
 * // buf = {"cmd":"set","id":3,"unit":0,"val":55}
 * // Дальше отправляешь через ConfigSender
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ArduinoJson.h>
#include "menu_meta.h"
#include "menu_cache.h"

// Расчёт вместимости JSON для полного меню
constexpr size_t MENU_JSON_ITEM_CAP =
    JSON_OBJECT_SIZE(8) + JSON_ARRAY_SIZE(MENU_MAX_UNITS); // id,t,n,p,u,min,max,step,val[]
constexpr size_t MENU_JSON_DOC_CAP =
    JSON_OBJECT_SIZE(2) +                             // v + menu
    JSON_ARRAY_SIZE(MENU_META_COUNT) +                // оболочка массива
    MENU_META_COUNT * MENU_JSON_ITEM_CAP;             // сами элементы
constexpr size_t MENU_FULL_JSON_BUF_SIZE = MENU_JSON_DOC_CAP + 256; // небольшой запас под сериализацию

// ============================================================================
// Парсинг JSON от MCU → обновление g_menu_cache
// ============================================================================

/**
 * @brief Парсинг полного конфига от MCU
 *
 * JSON формат:
 * {
 *   "v": 8,           // версия конфига
 *   "units": 3,       // количество юнитов
 *   "active": 0,      // активный юнит
 *   "lang": "ru",     // язык
 *   "menu": [
 *     {"id":3, "t":"val", "val":[50,55,60], ...},
 *     ...
 *   ]
 * }
 *
 * @param json NULL-terminated JSON строка
 * @return true если парсинг успешен
 */
bool menu_parseFullConfig(const char* json);

/**
 * @brief Парсинг delta-обновления от MCU
 *
 * JSON формат:
 * {
 *   "d": {
 *     "3": [55, 60, 55],   // per-unit значение
 *     "7": 42              // global значение
 *   }
 * }
 *
 * @param json NULL-terminated JSON строка
 * @return true если парсинг успешен
 */
bool menu_parseDelta(const char* json);

// ============================================================================
// Формирование JSON команд → MCU
// ============================================================================

/**
 * @brief Сформировать команду set (изменение значения)
 *
 * Результат: {"cmd":"set","id":3,"unit":0,"val":55.5}
 * Для global элементов unit игнорируется.
 *
 * @param buf Буфер для JSON
 * @param bufSize Размер буфера
 * @param id ID элемента меню
 * @param unit Номер юнита (0-2), или 255 для active_unit
 * @param val Новое значение
 * @return Длина JSON (без \0), или 0 при ошибке
 */
size_t menu_buildSetCommand(char* buf, size_t bufSize,
                            uint16_t id, uint8_t unit, float val);

/**
 * @brief Сформировать команду invoke (вызов action)
 *
 * Результат: {"cmd":"invoke","id":89}
 *
 * @param buf Буфер для JSON
 * @param bufSize Размер буфера
 * @param id ID элемента меню (type=action)
 * @return Длина JSON (без \0), или 0 при ошибке
 */
size_t menu_buildInvokeCommand(char* buf, size_t bufSize, uint16_t id);

/**
 * @brief Сформировать команду get_config (запрос полного конфига)
 *
 * Результат: {"cmd":"get_config"}
 *
 * @param buf Буфер для JSON
 * @param bufSize Размер буфера
 * @return Длина JSON (без \0), или 0 при ошибке
 */
size_t menu_buildGetConfigCommand(char* buf, size_t bufSize);

/**
 * @brief Сформировать команду set_active (смена активного юнита)
 *
 * Результат: {"cmd":"set_active","unit":1}
 *
 * @param buf Буфер для JSON
 * @param bufSize Размер буфера
 * @param unit Номер юнита (0-2)
 * @return Длина JSON (без \0), или 0 при ошибке
 */
size_t menu_buildSetActiveCommand(char* buf, size_t bufSize, uint8_t unit);

// ============================================================================
// Callback для уведомления об изменениях
// ============================================================================

/**
 * @brief Тип callback функции при изменении значения в кэше
 *
 * Вызывается после успешного парсинга delta или full config.
 *
 * @param id ID изменённого элемента
 * @param unit Номер юнита (255 для global)
 */
typedef void (*MenuChangeCallback)(uint16_t id, uint8_t unit);

/**
 * @brief Установить callback на изменение значений
 *
 * @param cb Callback функция (nullptr для отключения)
 */
void menu_setChangeCallback(MenuChangeCallback cb);

// ============================================================================
// Формирование полного JSON для бэкенда
// ============================================================================

/**
 * @brief Сформировать полный JSON меню с названиями для бэкенда
 *
 * Объединяет g_menu_meta (названия, структура) + g_menu_cache (значения)
 * в один JSON для отправки в MQTT.
 *
 * Формат:
 * {
 *   "v": 8,
 *   "units": 3,
 *   "active": 0,
 *   "lang": "en",
 *   "menu": [
 *     {"id":0,"t":"sub","n":["МЕНЮ","MENU"],"p":-1},
 *     {"id":3,"t":"val","n":["ТЕМПЕРАТУРА","TEMPERATURE"],"u":["°C","°C"],
 *      "p":2,"min":30,"max":110,"step":1,"val":[50,65,85]},
 *     ...
 *   ]
 * }
 *
 * @param buf Буфер для JSON
 * @param bufSize Размер буфера (рекомендуется 16KB+)
 * @return Длина JSON (без \0), или 0 при ошибке
 */
size_t menu_buildFullJson(char* buf, size_t bufSize);
