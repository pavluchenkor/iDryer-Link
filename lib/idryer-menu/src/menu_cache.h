// Auto-generated for ESP32 LINK. Do not edit.
// Contains menu value cache (current values from MCU).
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "menu_meta.h"

#define MENU_MAX_UNITS 3

// Total menu items: 145, with values: 76

// Значение одного элемента меню
union MenuValue {
    float    f32;
    uint32_t u32;
    int32_t  i32;
    uint16_t u16;
    uint8_t  u8;
    bool     b;
};

// ID системных настроек в menu_meta
#define MENU_ID_UNITS_COUNT 143  // Количество юнитов (1-3)
#define MENU_ID_LANGUAGE    144  // Язык (0=ru, 1=en)

// Кэш значений меню для LINK
class MenuCache {
public:
    uint16_t revision = 0;       // Ревизия конфига от MCU
    uint8_t  active_unit = 0;    // Активный юнит (runtime, не из меню)

    // Значения: [menu_id][unit_index]
    // Для global элементов используется только [id][0]
    MenuValue values[MENU_META_COUNT][MENU_MAX_UNITS] = {};

    // Количество юнитов (из menu id=143)
    uint8_t getUnitsCount() const {
        return (uint8_t)values[MENU_ID_UNITS_COUNT][0].u8;
    }

    // Текущий язык (из menu id=144): 0=ru, 1=en
    uint8_t getLang() const {
        return (uint8_t)values[MENU_ID_LANGUAGE][0].u8;
    }

    // Получить значение как float
    float getFloat(uint16_t id, uint8_t unit = 255) const {
        if (id >= MENU_META_COUNT) return 0.0f;
        const MenuMeta* m = &g_menu_meta[id];
        uint8_t u = (unit == 255) ? active_unit : unit;
        if (m->scope == META_SCOPE_GLOBAL) u = 0;
        if (u >= MENU_MAX_UNITS) u = 0;
        const MenuValue& v = values[id][u];
        switch (m->vtype) {
            case META_VT_F32:  return v.f32;
            case META_VT_U32:  return (float)v.u32;
            case META_VT_I32:  return (float)v.i32;
            case META_VT_U16:  return (float)v.u16;
            case META_VT_U8:   return (float)v.u8;
            case META_VT_BOOL: return v.b ? 1.0f : 0.0f;
            default: return 0.0f;
        }
    }

    // Установить значение из float
    void setFloat(uint16_t id, float val, uint8_t unit = 255) {
        if (id >= MENU_META_COUNT) return;
        const MenuMeta* m = &g_menu_meta[id];
        uint8_t u = (unit == 255) ? active_unit : unit;
        if (m->scope == META_SCOPE_GLOBAL) u = 0;
        if (u >= MENU_MAX_UNITS) u = 0;
        MenuValue& v = values[id][u];
        switch (m->vtype) {
            case META_VT_F32:  v.f32 = val; break;
            case META_VT_U32:  v.u32 = (uint32_t)val; break;
            case META_VT_I32:  v.i32 = (int32_t)val; break;
            case META_VT_U16:  v.u16 = (uint16_t)val; break;
            case META_VT_U8:   v.u8  = (uint8_t)val; break;
            case META_VT_BOOL: v.b   = (val != 0.0f); break;
            default: break;
        }
    }

    // Получить bool значение
    bool getBool(uint16_t id, uint8_t unit = 255) const {
        return getFloat(id, unit) != 0.0f;
    }

    // Получить int значение
    int32_t getInt(uint16_t id, uint8_t unit = 255) const {
        return (int32_t)getFloat(id, unit);
    }
};

// Глобальный экземпляр кэша
extern MenuCache g_menu_cache;
