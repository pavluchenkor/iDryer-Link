/**
 * @file menu_commands.cpp
 * @brief Реализация парсинга JSON и формирования команд
 */

#include "menu_commands.h"
#include "menu_cache.h"
#include "menu_meta.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

// Callback для уведомления об изменениях
static MenuChangeCallback s_changeCallback = nullptr;

// ============================================================================
// Вспомогательные функции
// ============================================================================

static void notifyChange(uint16_t id, uint8_t unit) {
    if (s_changeCallback) {
        s_changeCallback(id, unit);
    }
}

// Конвертация поля lang (строка "ru"/"en" или число) в индекс языка
static uint8_t parseLangVariant(JsonVariant langVal) {
    if (langVal.is<const char*>()) {
        const char* s = langVal.as<const char*>();
        if (!s) return 0;
        if (strcmp(s, "en") == 0 || strcmp(s, "EN") == 0) return 1;
        if (strcmp(s, "1") == 0) return 1;
        return 0; // по умолчанию ru
    }
    uint8_t langIdx = langVal.as<uint8_t>();
    return langIdx ? 1 : 0; // нормализуем к 0/1
}

static void applyLangToCache(uint8_t lang) {
    uint8_t norm = lang ? 1 : 0;
    g_menu_cache.lang = norm;
    // Синхронизируем с пунктом меню LANGUAGE (последний элемент)
    const uint16_t langId = MENU_META_COUNT - 1;
    if (langId < MENU_META_COUNT) {
        g_menu_cache.setFloat(langId, norm, 0);
    }
}

static void applyUnitsToCache(uint8_t units) {
    uint8_t norm = (units >= 1 && units <= MENU_MAX_UNITS) ? units : g_menu_cache.units_count;
    g_menu_cache.units_count = norm;
    // Синхронизируем с пунктом UNITS_COUNT (предпоследний элемент)
    const uint16_t unitsId = MENU_META_COUNT - 2;
    if (unitsId < MENU_META_COUNT) {
        g_menu_cache.setFloat(unitsId, norm, 0);
    }
}

// ============================================================================
// Парсинг JSON от MCU
// ============================================================================

// Статический буфер для парсинга (не на стеке!)
static StaticJsonDocument<8192> s_configDoc;

/**
 * @brief Парсинг vals объекта (общая логика для full и delta)
 * @param vals JsonObject с ключами = ID, значениями = val или [val, val, val]
 */
static void parseValsObject(JsonObject vals) {
    for (JsonPair kv : vals) {
        // Ключ - это ID (строка "3", "143", etc)
        uint16_t id = (uint16_t)atoi(kv.key().c_str());
        if (id >= MENU_META_COUNT) continue;

        const MenuMeta* meta = menu_meta_get(id);
        if (!meta) continue;

        JsonVariant val = kv.value();

        if (meta->scope == META_SCOPE_GLOBAL) {
            // Global: одно значение
            g_menu_cache.setFloat(id, val.as<float>(), 0);
            notifyChange(id, 255);
        } else {
            // Per-unit: массив значений
            if (val.is<JsonArray>()) {
                JsonArray arr = val.as<JsonArray>();
                uint8_t u = 0;
                for (JsonVariant v : arr) {
                    if (u >= MENU_MAX_UNITS) break;
                    g_menu_cache.setFloat(id, v.as<float>(), u);
                    notifyChange(id, u);
                    u++;
                }
            } else {
                // Fallback: если пришло одно значение для per-unit
                g_menu_cache.setFloat(id, val.as<float>(), 0);
                notifyChange(id, 0);
            }
        }

        // Поддержка мгновенного обновления языка/юнитов при их приходе в vals
        const uint16_t langId = MENU_META_COUNT - 1;
        const uint16_t unitsId = MENU_META_COUNT - 2;
        if (id == langId) {
            applyLangToCache((uint8_t)val.as<uint8_t>());
        } else if (id == unitsId) {
            applyUnitsToCache((uint8_t)val.as<uint8_t>());
        }
    }
}

bool menu_parseFullConfig(const char* json) {
    if (!json) return false;

    s_configDoc.clear();
    DeserializationError err = deserializeJson(s_configDoc, json);
    if (err) return false;

    JsonDocument& doc = s_configDoc;

    // Revision (rev или v)
    if (doc.containsKey("rev")) {
        g_menu_cache.revision = doc["rev"].as<uint16_t>();
    } else if (doc.containsKey("v")) {
        g_menu_cache.revision = doc["v"].as<uint16_t>();
    }

    // Поддержка старого формата: units/active/lang в корне JSON
    if (doc.containsKey("units")) {
        applyUnitsToCache(doc["units"].as<uint8_t>());
    }

    if (doc.containsKey("active")) {
        uint8_t active = doc["active"].as<uint8_t>();
        if (active < MENU_MAX_UNITS) {
            g_menu_cache.active_unit = active;
        }
    }

    if (doc.containsKey("lang")) {
        applyLangToCache(parseLangVariant(doc["lang"]));
    }

    // Парсинг vals объекта: {"rev":8,"vals":{"3":[55,55,60],"143":3,"144":1}}
    JsonObject vals = doc["vals"].as<JsonObject>();
    if (!vals) return false;

    parseValsObject(vals);

    return true;
}

bool menu_parseDelta(const char* json) {
    if (!json) return false;

    // Используем тот же статический буфер
    s_configDoc.clear();
    DeserializationError err = deserializeJson(s_configDoc, json);
    if (err) return false;

    // Delta формат UART от MCU: {"rev":124,"vals":{"3":[55,55,60]}}
    // Обновляем revision
    if (s_configDoc.containsKey("rev")) {
        g_menu_cache.revision = s_configDoc["rev"].as<uint16_t>();
    }

    // Возможный top-level lang в delta
    if (s_configDoc.containsKey("lang")) {
        applyLangToCache(parseLangVariant(s_configDoc["lang"]));
    }

    // Парсим vals
    JsonObject vals = s_configDoc["vals"].as<JsonObject>();
    if (!vals) return false;

    parseValsObject(vals);

    return true;
}

// ============================================================================
// Формирование JSON команд
// ============================================================================

size_t menu_buildSetCommand(char* buf, size_t bufSize,
                            uint16_t id, uint8_t unit, float val) {
    if (!buf || bufSize < 32) return 0;

    const MenuMeta* meta = menu_meta_get(id);
    if (!meta) return 0;

    // Для global элементов unit не нужен
    if (meta->scope == META_SCOPE_GLOBAL) {
        return snprintf(buf, bufSize,
            "{\"cmd\":\"set\",\"id\":%u,\"val\":%.2f}",
            id, val);
    } else {
        // Если unit=255, используем active_unit
        if (unit == 255) {
            unit = g_menu_cache.active_unit;
        }
        return snprintf(buf, bufSize,
            "{\"cmd\":\"set\",\"id\":%u,\"unit\":%u,\"val\":%.2f}",
            id, unit, val);
    }
}

size_t menu_buildInvokeCommand(char* buf, size_t bufSize, uint16_t id) {
    if (!buf || bufSize < 24) return 0;

    return snprintf(buf, bufSize,
        "{\"cmd\":\"invoke\",\"id\":%u}",
        id);
}

size_t menu_buildGetConfigCommand(char* buf, size_t bufSize) {
    if (!buf || bufSize < 20) return 0;

    return snprintf(buf, bufSize, "{\"cmd\":\"get_config\"}");
}

size_t menu_buildSetActiveCommand(char* buf, size_t bufSize, uint8_t unit) {
    if (!buf || bufSize < 28) return 0;

    return snprintf(buf, bufSize,
        "{\"cmd\":\"set_active\",\"unit\":%u}",
        unit);
}

// ============================================================================
// Callback
// ============================================================================

void menu_setChangeCallback(MenuChangeCallback cb) {
    s_changeCallback = cb;
}

// ============================================================================
// Формирование полного JSON для бэкенда
// ============================================================================

size_t menu_buildFullJson(char* buf, size_t bufSize) {
    if (!buf || bufSize < 256) return 0;

    // Статический буфер, рассчитанный от количества пунктов меню
    static StaticJsonDocument<MENU_JSON_DOC_CAP> doc;
    doc.clear();

    // Метаданные — только revision
    // units, lang теперь обычные пункты меню (id=143, 144)
    doc["v"] = g_menu_cache.revision;

    // Соглашение о позиции элементов в меню:
    // - Язык (LANGUAGE) всегда последний элемент
    // - Количество юнитов (UNITS_COUNT) всегда предпоследний элемент

    if (MENU_META_COUNT >= 2) {
        // Обновляем units_count из предпоследнего элемента меню
        uint16_t unitsCountId = MENU_META_COUNT - 2;
        uint8_t unitsCountValue = (uint8_t)g_menu_cache.getInt(unitsCountId, 0);
        if (unitsCountValue > 0 && unitsCountValue <= MENU_MAX_UNITS) {
            g_menu_cache.units_count = unitsCountValue;
        }

        // Обновляем язык из последнего элемента меню
        uint16_t langId = MENU_META_COUNT - 1;
        uint8_t langValue = (uint8_t)g_menu_cache.getInt(langId, 0);
        g_menu_cache.lang = langValue;  // 0=ru, 1=en
    }

    // Массив меню
    JsonArray menu = doc.createNestedArray("menu");
    uint8_t lang = g_menu_cache.getLang();
    uint8_t unitsCount = g_menu_cache.getUnitsCount();
    if (unitsCount == 0) unitsCount = 1;  // Защита от нуля

    for (uint16_t id = 0; id < MENU_META_COUNT; id++) {
        const MenuMeta* meta = &g_menu_meta[id];

        JsonObject item = menu.createNestedObject();
        item["id"] = id;

        // Тип
        const char* typeStr = "sub";
        switch (meta->type) {
            case META_SUBMENU: typeStr = "sub"; break;
            case META_ACTION:  typeStr = "act"; break;
            case META_VALUE:   typeStr = "val"; break;
            case META_TOGGLE:  typeStr = "tog"; break;
        }
        item["t"] = typeStr;

        // Название (только текущий язык)
        item["n"] = meta->title[lang] ? meta->title[lang] : "";

        // Parent
        item["p"] = meta->parent;

        // Единицы измерения (только текущий язык, если есть)
        if (meta->unit[lang]) {
            item["u"] = meta->unit[lang];
        }

        // Для val/tog добавляем значения и диапазоны
        if (meta->type == META_VALUE || meta->type == META_TOGGLE) {
            // min/max/step для value
            if (meta->type == META_VALUE) {
                item["min"] = meta->min_val;
                item["max"] = meta->max_val;
                item["step"] = meta->step;
            }

            // Значения
            if (meta->scope == META_SCOPE_GLOBAL) {
                // Global: одно значение
                if (meta->type == META_TOGGLE) {
                    item["val"] = g_menu_cache.getBool(id, 0);
                } else {
                    item["val"] = g_menu_cache.getFloat(id, 0);
                }
            } else {
                // Per-unit: массив значений
                JsonArray vals = item.createNestedArray("val");
                for (uint8_t u = 0; u < unitsCount; u++) {
                    if (meta->type == META_TOGGLE) {
                        vals.add(g_menu_cache.getBool(id, u));
                    } else {
                        vals.add(g_menu_cache.getFloat(id, u));
                    }
                }
            }
        }
    }

    // Сериализация
    if (doc.overflowed()) {
        // Если не хватило памяти, логируем и не рискуем публиковать усечённый JSON
        return 0;
    }

    size_t len = serializeJson(doc, buf, bufSize);
    return len;
}
