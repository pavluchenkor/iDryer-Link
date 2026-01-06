# idryer-menu

Библиотека для работы с меню iDryer на стороне LINK (ESP32).

## Назначение

- Хранение кэша значений меню (RAM ~2KB)
- Парсинг JSON конфигов от MCU
- Формирование JSON команд для MCU
- Доступ к метаданным меню (FLASH)

## Архитектура

```
MCU (RP2040)                    LINK (ESP32)
┌─────────────┐                ┌─────────────────────┐
│ EEPROM      │                │ menu_cache          │
│ (значения)  │ ──JSON────────>│ (копия значений)    │
│             │                │                     │
│ callbacks   │ <──команды──── │ menu_commands       │
│ (действия)  │                │ (формирует JSON)    │
└─────────────┘                └─────────────────────┘
```

MCU — источник истины. LINK кэширует значения для быстрого доступа UI.

## Файлы

| Файл | Описание | Источник |
|------|----------|----------|
| `menu_meta.h` | Метаданные меню (структура, тексты, min/max) | Генератор |
| `menu_ids.h` | enum MenuId с ID пунктов | Генератор |
| `menu_cache.h` | Кэш значений (заголовок) | Генератор |
| `menu_cache.cpp` | Кэш значений (g_menu_cache) | Генератор |
| `menu_commands.h` | Парсинг/формирование JSON | Вручную |
| `menu_commands.cpp` | Реализация | Вручную |

## Использование

### 1. Получение конфига от MCU

```cpp
#include "menu_commands.h"
#include "menu_cache.h"

// При получении JSON от MCU:
void onMcuMessage(const char* json) {
    // Полный конфиг
    if (strstr(json, "\"menu\"")) {
        menu_parseFullConfig(json);
    }
    // Delta-обновление
    else if (strstr(json, "\"d\"")) {
        menu_parseDelta(json);
    }
}

// Доступ к значениям:
float temp = g_menu_cache.getFloat(MENU_ID_DRY_TEMP);
bool enabled = g_menu_cache.getBool(MENU_ID_DRY_ENABLE);
```

### 2. Отправка команд на MCU

```cpp
char buf[64];

// Изменение значения
menu_buildSetCommand(buf, sizeof(buf), MENU_ID_DRY_TEMP, 0, 55.0f);
// Результат: {"cmd":"set","id":3,"unit":0,"val":55.00}
sendToMcu(buf);

// Вызов action
menu_buildInvokeCommand(buf, sizeof(buf), MENU_ID_FACTORY_RESET);
// Результат: {"cmd":"invoke","id":89}
sendToMcu(buf);

// Запрос полного конфига
menu_buildGetConfigCommand(buf, sizeof(buf));
// Результат: {"cmd":"get_config"}
sendToMcu(buf);

// Смена активного юнита
menu_buildSetActiveCommand(buf, sizeof(buf), 1);
// Результат: {"cmd":"set_active","unit":1}
sendToMcu(buf);
```

### 3. Подписка на изменения

```cpp
void onMenuChange(uint16_t id, uint8_t unit) {
    // unit=255 для global элементов
    Serial.printf("Changed: id=%u, unit=%u\n", id, unit);

    // Обновить UI
    ui_refresh(id, unit);
}

void setup() {
    menu_setChangeCallback(onMenuChange);
}
```

### 4. Доступ к метаданным

```cpp
#include "menu_meta.h"

const MenuMeta* meta = menu_meta_get(MENU_ID_DRY_TEMP);
if (meta) {
    Serial.printf("Name: %s\n", meta->name_en);
    Serial.printf("Min: %.1f, Max: %.1f\n", meta->min, meta->max);
    Serial.printf("Scope: %s\n",
        meta->scope == META_SCOPE_GLOBAL ? "global" : "per-unit");
}
```

## JSON протокол

### От MCU (полный конфиг)

```json
{
  "v": 8,
  "units": 3,
  "active": 0,
  "lang": "ru",
  "menu": [
    {"id": 3, "t": "val", "val": [50, 55, 60]},
    {"id": 7, "t": "tog", "val": [1, 1, 0]}
  ]
}
```

### От MCU (delta)

```json
{
  "d": {
    "3": [55, 60, 55],
    "7": 42
  }
}
```

### На MCU (команды)

```json
{"cmd": "set", "id": 3, "unit": 0, "val": 55.5}
{"cmd": "invoke", "id": 89}
{"cmd": "get_config"}
{"cmd": "set_active", "unit": 1}
```

## Память

- Метаданные (FLASH): ~100 байт × N элементов
- Кэш значений (RAM): ~4 байт × N элементов × 3 юнита ≈ 2KB
- JSON буфер: рекомендуется 8KB для полного конфига

## Зависимости

- ArduinoJson (для парсинга/сериализации)

## Генерация файлов

Файлы menu_meta.h, menu_ids.h, menu_cache.h, menu_cache.cpp генерируются из YAML:

```bash
cd config-exmple/menu/generator
python gen_menu_v2.py
```

При сборке `copy_menu.py` автоматически копирует файлы из `config-exmple/menu/` в `lib/idryer-menu/src/`.
