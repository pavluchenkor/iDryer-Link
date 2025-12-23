# iDryer Protocol Library

Библиотека протокола для устройств iDryer — сушилок филамента.

**Версия**: 2.5.0
**API**: [MQTT API v2.5](../docs/mqtt-api-kit/README.md)

---

## Возможности

- **Типы данных** — enums (Mode, Severity), packed структуры (Telemetry, Status, Events)
- **MQTT топики** — константы, QoS, retained флаги
- **UART кадры** — формат для связи ESP32 ↔ RP2040
- **Совместимость** — Arduino, ESP-IDF, Pico SDK

## Установка

### PlatformIO

```ini
; platformio.ini
lib_deps =
    ; Из локальной папки (уже подключена)
    idryer-protocol

    ; Или из GitHub (когда будет опубликована)
    ; https://github.com/idryer/idryer-protocol.git#v2.5.0
```

### Arduino IDE

Скопируйте папку `lib/idryer-protocol` в `~/Arduino/libraries/`

---

## Быстрый старт

```cpp
#include <idryer_protocol.h>

void setup() {
    Serial.begin(115200);

    // Проверка версии
    Serial.printf("Protocol: %s (API %s)\n",
                  IDRYER_PROTOCOL_VERSION,
                  IDRYER_API_VERSION);
}

void sendTelemetry() {
    // Создаём payload
    idryer_telemetry_unit_t telemetry = {
        .unit_id = "U1",
        .temperature = 501,  // 50.1°C (x10)
        .humidity = 12,
        .heater_power = 85,
        .fan_status = 1
    };

    // Формируем MQTT топик
    char topic[64];
    idryer_make_topic(topic, sizeof(topic), serialNumber, IDRYER_TOPIC_TELEMETRY);
    // Результат: "idryer/DEVICE_xxx/telemetry"

    // Публикуем (ваш MQTT клиент)
    // mqtt.publish(topic, json, IDRYER_QOS_TELEMETRY, IDRYER_RETAINED_TELEMETRY);
}

void sendViaUart() {
    idryer_telemetry_unit_t telemetry = { /* ... */ };

    // Формируем UART кадр
    uint8_t frame[128];
    size_t len = idryer_uart_build_frame(
        frame,
        IDRYER_UART_MSG_TELEMETRY,
        IDRYER_UART_FLAG_NONE,
        &telemetry,
        sizeof(telemetry)
    );

    // Отправляем по UART
    Serial1.write(frame, len);
}
```

---

## Структура библиотеки

```
src/
├── idryer_protocol.h      # Главный include
├── version.h              # Версия библиотеки
│
├── types/
│   ├── idryer_types.h     # Enums: Mode, Severity, RfidEvent
│   ├── idryer_payloads.h  # Структуры: Telemetry, Status, Event...
│   └── idryer_commands.h  # Команды: Start, Stop, GetConfig...
│
├── mqtt/
│   └── idryer_topics.h    # Топики, QoS, retained, интервалы
│
└── uart/
    ├── idryer_uart_frame.h  # Формат UART кадра
    └── idryer_uart_frame.c  # CRC16, сборка/парсинг кадров
```

---

## API Reference

### Enums

```cpp
// Режим работы юнита
idryer_mode_t mode = IDRYER_MODE_DRYING;
const char* str = idryer_mode_to_string(mode);  // "DRYING"
mode = idryer_mode_from_string("STORAGE");      // IDRYER_MODE_STORAGE

// Уровень важности события
idryer_severity_t sev = IDRYER_SEVERITY_WARNING;
const char* str = idryer_severity_to_string(sev);  // "warning"
```

### Payloads

```cpp
// Телеметрия (RP2040 → ESP32 → MQTT)
idryer_telemetry_unit_t telemetry;
telemetry.temperature = IDRYER_TEMP_FROM_FLOAT(50.1f);  // 501

// Статус
idryer_status_unit_t status;
status.mode = IDRYER_MODE_DRYING;
status.duration = IDRYER_DURATION_INFINITE;  // Для STORAGE

// Событие
idryer_event_payload_t event;
event.severity = IDRYER_SEVERITY_CRITICAL;
strcpy(event.source, "THERMISTOR");
strcpy(event.event, "SENSOR_SHORT");
```

### MQTT Topics

```cpp
char topic[64];

// Формирование топика
idryer_make_topic(topic, sizeof(topic), "DEVICE_abc", IDRYER_TOPIC_TELEMETRY);
// "idryer/DEVICE_abc/telemetry"

// Подписка на команды
idryer_make_cmd_subscribe_topic(topic, sizeof(topic), "DEVICE_abc");
// "idryer/DEVICE_abc/commands/#"

// Метаданные топика
const idryer_topic_info_t* info = idryer_get_topic_info(IDRYER_TOPIC_STATUS);
// info->qos = 1, info->retained = 1
```

### UART Frames

```cpp
// Сборка кадра
uint8_t frame[128];
idryer_telemetry_unit_t data = { /* ... */ };
size_t len = idryer_uart_build_frame(
    frame,
    IDRYER_UART_MSG_TELEMETRY,
    IDRYER_UART_FLAG_ACK_REQ,
    &data,
    sizeof(data)
);

// Парсинг кадра
idryer_uart_header_t header;
const uint8_t* payload;
size_t payload_len;
if (idryer_uart_parse_frame(buf, buf_len, &header, &payload, &payload_len)) {
    switch (header.msg_type) {
        case IDRYER_UART_MSG_TELEMETRY:
            // ...
            break;
    }
}
```

---

## Константы

### Временные интервалы

| Константа | Значение | Описание |
|-----------|----------|----------|
| `IDRYER_INTERVAL_TELEMETRY_MS` | 5000 | Интервал телеметрии (5 сек) |
| `IDRYER_INTERVAL_WEIGHTS_MS` | 10000 | Интервал весов (10 сек) |
| `IDRYER_UART_HEARTBEAT_MS` | 1000 | Heartbeat UART (1 сек) |
| `IDRYER_UART_CMD_TIMEOUT_MS` | 500 | Таймаут команды |

### QoS и Retained

| Топик | QoS | Retained |
|-------|-----|----------|
| telemetry | 0 | No |
| status | 1 | Yes |
| events | 1 | No |
| info | 1 | Yes |

---

## Синхронизация с API

При изменении MQTT API:

1. Обновить `docs/mqtt-api-kit/` (источник истины)
2. Обновить `lib/idryer-protocol/src/version.h`
3. Обновить соответствующие `.h` файлы
4. Запустить тесты

```bash
# Проверка версии
grep "IDRYER_API_VERSION" lib/idryer-protocol/src/version.h
# Должна совпадать с версией в docs/mqtt-api-kit/README.md
```

---

## Лицензия

MIT License. См. [LICENSE](LICENSE).
