# Задача 00 — Библиотека протокола iDryer (idryer-protocol)

> **Платформа**: ESP32-C3/S3, RP2040, любой ARM Cortex-M
> **Совместимость**: Arduino IDE, ESP-IDF, Pico SDK, чистый C/C++
> **Репозиторий**: `github.com/idryer/idryer-protocol`

> **Источник истины (API)**:
> - [MQTT API README](../docs/mqtt-api-kit/README.md)
> - [Types](../docs/mqtt-api-kit/02-api-reference/types.md)
> - [Topics](../docs/mqtt-api-kit/02-api-reference/topics.md)
> - [Commands](../docs/mqtt-api-kit/02-api-reference/commands.md)
> - [Device → Backend](../docs/mqtt-api-kit/02-api-reference/device-to-backend.md)

---

## Цель

Создать публичную библиотеку протокола iDryer:
- **Единый источник типов** для ESP32 и RP2040
- **Формализованные структуры** для UART и MQTT
- **Переиспользование** в разных проектах
- **Версионирование** синхронно с MQTT API

---

## Структура библиотеки

```
idryer-protocol/
├── README.md
├── LICENSE                     # MIT
├── library.json                # PlatformIO
├── library.properties          # Arduino IDE
├── CHANGELOG.md
│
├── src/
│   ├── idryer_protocol.h       # Главный include
│   │
│   ├── types/
│   │   ├── idryer_types.h      # Enums: Mode, Severity, RfidEvent
│   │   ├── idryer_payloads.h   # Структуры payload (packed)
│   │   └── idryer_commands.h   # Структуры команд
│   │
│   ├── mqtt/
│   │   ├── idryer_topics.h     # Топики, QoS, retained
│   │   └── idryer_json.h       # Хелперы сериализации JSON (опционально)
│   │
│   ├── uart/
│   │   ├── idryer_uart_frame.h # Формат кадра, CRC16
│   │   ├── idryer_uart_frame.c
│   │   └── idryer_uart_types.h # MessageKind enum
│   │
│   └── version.h               # IDRYER_PROTOCOL_VERSION
│
├── examples/
│   ├── esp32_mqtt_publish/
│   ├── esp32_uart_receive/
│   └── rp2040_uart_send/
│
└── test/
    ├── test_payloads.cpp
    ├── test_uart_frame.cpp
    └── test_json_serialize.cpp
```

---

## Ключевые файлы

### version.h — Версия протокола

```c
#ifndef IDRYER_VERSION_H
#define IDRYER_VERSION_H

// Версия библиотеки (semver)
#define IDRYER_PROTOCOL_VERSION_MAJOR  2
#define IDRYER_PROTOCOL_VERSION_MINOR  5
#define IDRYER_PROTOCOL_VERSION_PATCH  0
#define IDRYER_PROTOCOL_VERSION        "2.5.0"

// Соответствует MQTT API версии
#define IDRYER_API_VERSION             "2.5"

// Дата последней синхронизации с API
#define IDRYER_API_SYNC_DATE           "2025-12-13"

#endif
```

### idryer_types.h — Enums

```c
#ifndef IDRYER_TYPES_H
#define IDRYER_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Режим работы юнита
 * @see docs/mqtt-api-kit/02-api-reference/types.md#enum-mode
 */
typedef enum {
    IDRYER_MODE_IDLE    = 0,  // Простаивает
    IDRYER_MODE_DRYING  = 1,  // Сушка (один этап)
    IDRYER_MODE_STORAGE = 2,  // Хранение (бесконечно)
    IDRYER_MODE_PROFILE = 3,  // Профильная сушка (несколько этапов)
    IDRYER_MODE_FAULT   = 4   // Аварийная остановка
} idryer_mode_t;

/**
 * Уровень важности события
 * @see docs/mqtt-api-kit/02-api-reference/types.md#enum-severity
 */
typedef enum {
    IDRYER_SEVERITY_INFO     = 0,  // Информация
    IDRYER_SEVERITY_WARNING  = 1,  // Предупреждение
    IDRYER_SEVERITY_ERROR    = 2,  // Ошибка
    IDRYER_SEVERITY_CRITICAL = 3   // Критическая (автостоп)
} idryer_severity_t;

/**
 * Тип RFID события
 * @see docs/mqtt-api-kit/02-api-reference/types.md#enum-rfideventtype
 */
typedef enum {
    IDRYER_RFID_TAG_DETECTED = 0,
    IDRYER_RFID_TAG_REMOVED  = 1
} idryer_rfid_event_t;

// Хелперы для конвертации в строки (для JSON)
const char* idryer_mode_to_string(idryer_mode_t mode);
idryer_mode_t idryer_mode_from_string(const char* str);

const char* idryer_severity_to_string(idryer_severity_t sev);
idryer_severity_t idryer_severity_from_string(const char* str);

#ifdef __cplusplus
}
#endif

#endif
```

### idryer_payloads.h — Структуры данных

```c
#ifndef IDRYER_PAYLOADS_H
#define IDRYER_PAYLOADS_H

#include "idryer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Максимальные размеры
#define IDRYER_UNIT_ID_LEN      4
#define IDRYER_SOURCE_LEN       16
#define IDRYER_EVENT_LEN        24
#define IDRYER_MESSAGE_LEN      64
#define IDRYER_MAX_UNITS        4
#define IDRYER_MAX_WEIGHTS      4
#define IDRYER_MAX_PROFILE_STAGES 10

// Специальные значения
#define IDRYER_DURATION_INFINITE  0xFFFF

/**
 * Телеметрия одного юнита
 * @see docs/mqtt-api-kit/02-api-reference/types.md#type-telemetrypayload
 */
typedef struct __attribute__((packed)) {
    char     unit_id[IDRYER_UNIT_ID_LEN];  // "U1", "U2"
    int16_t  temperature;    // °C * 10 (например 501 = 50.1°C)
    uint8_t  humidity;       // % (0-100)
    uint8_t  heater_power;   // % (0-100)
    uint8_t  fan_status;     // bool
} idryer_telemetry_unit_t;

/**
 * Статус одного юнита
 * @see docs/mqtt-api-kit/02-api-reference/types.md#type-unitstatus
 */
typedef struct __attribute__((packed)) {
    char     unit_id[IDRYER_UNIT_ID_LEN];
    uint8_t  mode;              // idryer_mode_t
    int16_t  target_temp;       // °C * 10
    uint16_t duration;          // минуты (IDRYER_DURATION_INFINITE = бесконечно)
    uint8_t  target_humidity;   // %
    uint32_t elapsed_time;      // секунды с начала
    // Для PROFILE режима:
    uint8_t  current_stage;     // 1-based
    uint8_t  total_stages;
    uint32_t stage_elapsed;     // секунды на текущем этапе
    uint32_t stage_remaining;   // секунды до конца этапа
    uint32_t total_remaining;   // секунды до конца программы
} idryer_status_unit_t;

/**
 * Данные веса
 * @see docs/mqtt-api-kit/02-api-reference/types.md#type-weightspayload
 */
typedef struct __attribute__((packed)) {
    char     sensor_id[IDRYER_UNIT_ID_LEN];  // "W1", "W2"
    float    value;             // граммы
    char     unit_id[IDRYER_UNIT_ID_LEN];
} idryer_weight_t;

/**
 * RFID событие
 * @see docs/mqtt-api-kit/02-api-reference/types.md#type-rfideventpayload
 */
typedef struct __attribute__((packed)) {
    uint8_t  event;             // idryer_rfid_event_t
    char     reader_id[IDRYER_UNIT_ID_LEN];  // "R1", "R2"
    char     tag[16];           // hex string или пусто
    char     unit_id[IDRYER_UNIT_ID_LEN];
} idryer_rfid_payload_t;

/**
 * Событие/ошибка
 * @see docs/mqtt-api-kit/02-api-reference/types.md#type-eventpayload
 */
typedef struct __attribute__((packed)) {
    uint8_t  severity;          // idryer_severity_t
    char     source[IDRYER_SOURCE_LEN];    // "HEATER", "THERMISTOR", "SHT"
    char     event[IDRYER_EVENT_LEN];      // "SENSOR_SHORT", "NO_RESPONSE"
    char     unit_id[IDRYER_UNIT_ID_LEN];
    char     message[IDRYER_MESSAGE_LEN];
} idryer_event_payload_t;

/**
 * Информация об устройстве
 * @see docs/mqtt-api-kit/02-api-reference/types.md#type-infopayload
 */
typedef struct __attribute__((packed)) {
    char     hw_version[8];     // "v1.0"
    char     fw_version[16];    // "1.2.3"
    uint32_t work_time_counter; // секунды наработки
} idryer_info_payload_t;

#ifdef __cplusplus
}
#endif

#endif
```

### idryer_commands.h — Команды

```c
#ifndef IDRYER_COMMANDS_H
#define IDRYER_COMMANDS_H

#include "idryer_types.h"
#include "idryer_payloads.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Этап профиля сушки
 */
typedef struct __attribute__((packed)) {
    int16_t  temperature;   // °C * 10
    uint16_t duration;      // минуты
    uint8_t  humidity;      // %
} idryer_profile_stage_t;

/**
 * Команда START
 * @see docs/mqtt-api-kit/02-api-reference/commands.md#команда-start
 */
typedef struct __attribute__((packed)) {
    char     unit_id[IDRYER_UNIT_ID_LEN];
    uint8_t  mode;              // DRYING, STORAGE, PROFILE
    // Для DRYING/STORAGE:
    int16_t  temperature;       // °C * 10
    uint16_t duration;          // минуты
    uint8_t  humidity;          // %
    // Для PROFILE:
    uint8_t  stage_count;       // 0 если не PROFILE
    idryer_profile_stage_t stages[IDRYER_MAX_PROFILE_STAGES];
} idryer_cmd_start_t;

/**
 * Команда STOP
 * @see docs/mqtt-api-kit/02-api-reference/commands.md#команда-stop
 */
typedef struct __attribute__((packed)) {
    char unit_id[IDRYER_UNIT_ID_LEN];
} idryer_cmd_stop_t;

/**
 * Команда GET_CONFIG
 * @see docs/mqtt-api-kit/02-api-reference/commands.md#команда-get_config
 */
typedef struct __attribute__((packed)) {
    uint8_t reserved;  // пустая команда
} idryer_cmd_get_config_t;

#ifdef __cplusplus
}
#endif

#endif
```

### idryer_topics.h — MQTT топики

```c
#ifndef IDRYER_TOPICS_H
#define IDRYER_TOPICS_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * MQTT топики
 * @see docs/mqtt-api-kit/02-api-reference/topics.md
 */

// Базовый префикс
#define IDRYER_TOPIC_PREFIX         "idryer"

// Device → Backend (publish)
#define IDRYER_TOPIC_INFO           "info"
#define IDRYER_TOPIC_TELEMETRY      "telemetry"
#define IDRYER_TOPIC_WEIGHTS        "weights"
#define IDRYER_TOPIC_RFID           "rfid"
#define IDRYER_TOPIC_STATUS         "status"
#define IDRYER_TOPIC_EVENTS         "events"
#define IDRYER_TOPIC_CONFIG         "config"

// Backend → Device (subscribe)
#define IDRYER_TOPIC_CMD_START      "commands/start"
#define IDRYER_TOPIC_CMD_STOP       "commands/stop"
#define IDRYER_TOPIC_CMD_GET_CONFIG "commands/get_config"
#define IDRYER_TOPIC_CMD_SET_CONFIG "commands/set_config"
#define IDRYER_TOPIC_CMD_WILDCARD   "commands/#"

// QoS уровни
#define IDRYER_QOS_INFO             1
#define IDRYER_QOS_TELEMETRY        0  // Потеря некритична
#define IDRYER_QOS_WEIGHTS          1
#define IDRYER_QOS_RFID             1
#define IDRYER_QOS_STATUS           1
#define IDRYER_QOS_EVENTS           1
#define IDRYER_QOS_CONFIG           1
#define IDRYER_QOS_COMMANDS         1

// Retained флаги
#define IDRYER_RETAINED_INFO        1
#define IDRYER_RETAINED_TELEMETRY   0
#define IDRYER_RETAINED_WEIGHTS     0
#define IDRYER_RETAINED_RFID        1
#define IDRYER_RETAINED_STATUS      1
#define IDRYER_RETAINED_EVENTS      0
#define IDRYER_RETAINED_CONFIG      0

// Интервалы публикации (мс)
#define IDRYER_INTERVAL_TELEMETRY   5000   // 5 сек
#define IDRYER_INTERVAL_WEIGHTS     10000  // 10 сек

/**
 * Формирование полного топика
 * @param buf      буфер для результата
 * @param buf_size размер буфера
 * @param serial   серийный номер устройства
 * @param suffix   суффикс топика (IDRYER_TOPIC_*)
 * @return указатель на buf
 */
static inline char* idryer_make_topic(char* buf, size_t buf_size,
                                       const char* serial, const char* suffix) {
    snprintf(buf, buf_size, "%s/%s/%s", IDRYER_TOPIC_PREFIX, serial, suffix);
    return buf;
}

/**
 * Структура с метаданными топика
 */
typedef struct {
    const char* suffix;
    uint8_t qos;
    uint8_t retained;
    uint32_t interval_ms;  // 0 = по событию
} idryer_topic_info_t;

// Таблица топиков для итерации
extern const idryer_topic_info_t IDRYER_TOPICS[];
extern const size_t IDRYER_TOPICS_COUNT;

#ifdef __cplusplus
}
#endif

#endif
```

---

## Процесс синхронизации с API

### Правило для разработчиков

```
┌─────────────────────────────────────────────────────────────────┐
│  ПРАВИЛО: API-First                                             │
│                                                                 │
│  1. Изменения ВСЕГДА начинаются с docs/mqtt-api-kit/            │
│  2. После изменения API → обновить idryer-protocol              │
│  3. Версия библиотеки = версия API                              │
│  4. Коммит: "sync: update protocol to API v2.X"                 │
└─────────────────────────────────────────────────────────────────┘
```

### Чек-лист при изменении API

```markdown
## При изменении MQTT API:

- [ ] Обновить docs/mqtt-api-kit/README.md (версия, changelog)
- [ ] Обновить docs/mqtt-api-kit/02-api-reference/types.md
- [ ] Обновить docs/mqtt-api-kit/02-api-reference/topics.md
- [ ] Обновить docs/mqtt-api-kit/02-api-reference/commands.md

## Синхронизация библиотеки:

- [ ] Обновить src/version.h (версия, дата синхронизации)
- [ ] Обновить src/types/idryer_types.h (если изменились enums)
- [ ] Обновить src/types/idryer_payloads.h (если изменились структуры)
- [ ] Обновить src/types/idryer_commands.h (если изменились команды)
- [ ] Обновить src/mqtt/idryer_topics.h (если изменились топики)
- [ ] Обновить CHANGELOG.md
- [ ] Запустить тесты: `pio test`
- [ ] Создать git tag: `git tag v2.X.0`
- [ ] Push: `git push origin main --tags`

## Оповещение:

- [ ] Обновить README с breaking changes (если есть)
- [ ] Создать GitHub Release с описанием изменений
```

### Автоматизация (CI/CD)

```yaml
# .github/workflows/sync-check.yml
name: API Sync Check

on:
  push:
    paths:
      - 'docs/mqtt-api-kit/**'

jobs:
  check-sync:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Check version sync
        run: |
          API_VERSION=$(grep -oP 'Версия.*: \K[\d.]+' docs/mqtt-api-kit/README.md)
          LIB_VERSION=$(grep -oP 'IDRYER_API_VERSION\s+"\K[^"]+' src/version.h)

          if [ "$API_VERSION" != "$LIB_VERSION" ]; then
            echo "::error::API version ($API_VERSION) != Library version ($LIB_VERSION)"
            echo "Run: update src/version.h to match API"
            exit 1
          fi

      - name: Notify if out of sync
        if: failure()
        run: |
          echo "⚠️ Library needs sync with API!"
          # Можно добавить Slack/Telegram notification
```

---

## Публикация

### PlatformIO Registry

```json
// library.json
{
  "name": "idryer-protocol",
  "version": "2.5.0",
  "description": "iDryer device protocol - types, payloads, MQTT topics",
  "keywords": ["idryer", "mqtt", "uart", "protocol", "esp32", "rp2040"],
  "repository": {
    "type": "git",
    "url": "https://github.com/idryer/idryer-protocol.git"
  },
  "authors": [
    {
      "name": "iDryer Team",
      "url": "https://idryer.org"
    }
  ],
  "license": "MIT",
  "homepage": "https://github.com/idryer/idryer-protocol",
  "frameworks": ["arduino", "espidf"],
  "platforms": ["espressif32", "raspberrypi"]
}
```

### Arduino Library Manager

```properties
# library.properties
name=iDryer Protocol
version=2.5.0
author=iDryer Team
maintainer=iDryer Team <dev@idryer.org>
sentence=iDryer device protocol library
paragraph=Types, payloads, and MQTT topics for iDryer devices
category=Communication
url=https://github.com/idryer/idryer-protocol
architectures=esp32,rp2040
includes=idryer_protocol.h
```

### Использование

```cpp
// В проекте ESP32 или RP2040
#include <idryer_protocol.h>

void setup() {
    // Проверка версии
    Serial.printf("Protocol version: %s\n", IDRYER_PROTOCOL_VERSION);

    // Использование типов
    idryer_telemetry_unit_t telemetry = {
        .unit_id = "U1",
        .temperature = 501,  // 50.1°C
        .humidity = 12,
        .heater_power = 85,
        .fan_status = 1
    };

    // Формирование топика
    char topic[64];
    idryer_make_topic(topic, sizeof(topic), serial_number, IDRYER_TOPIC_TELEMETRY);
    // topic = "idryer/DEVICE_aabbcc_1234567/telemetry"
}
```

---

## Результат

- Публичная библиотека на GitHub: `github.com/idryer/idryer-protocol`
- Доступна через PlatformIO: `lib_deps = idryer/idryer-protocol@^2.5.0`
- Доступна через Arduino Library Manager
- Автоматическая проверка синхронизации с API
- Документированный процесс обновления

## Критерии приёмки

- [ ] Библиотека компилируется для ESP32-C3, ESP32-S3, RP2040
- [ ] Все типы соответствуют [types.md](../docs/mqtt-api-kit/02-api-reference/types.md)
- [ ] Все топики соответствуют [topics.md](../docs/mqtt-api-kit/02-api-reference/topics.md)
- [ ] Тесты проходят на хосте (native)
- [ ] README содержит примеры использования
- [ ] CI проверяет синхронизацию версий
- [ ] Опубликовано в PlatformIO Registry
