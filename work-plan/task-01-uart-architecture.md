# Задача 01 — Спецификация UART между ESP32-C3/S3 и RP2040

> **Платформа**: ESP32-C3, ESP32-S3
>
> **Зависимости**: [task-00-protocol-library.md](./task-00-protocol-library.md) — использует типы из `idryer-protocol`
>
> **Ссылки на API:**
> - [MQTT API README](../docs/mqtt-api-kit/README.md)
> - [Types (Mode, Severity, Payloads)](../docs/mqtt-api-kit/02-api-reference/types.md)
> - [Device → Backend](../docs/mqtt-api-kit/02-api-reference/device-to-backend.md)
> - [Commands](../docs/mqtt-api-kit/02-api-reference/commands.md)

## Цель
Зафиксировать двунаправленный UART-протокол, по которому ESP32-C3/S3 (сетевой MQTT-шлюз) обменивается командами и телеметрией с RP2040. ESP32 ограничена ролью сетевого моста: принимает данные от RP2040 и публикует в MQTT топики, принимает MQTT команды и передаёт их RP2040.

## Основные шаги

### 1. Анализ требований
Собрать требования из MQTT API, выделить сущности для UART:
- **Телеметрия** (`TelemetryPayload`): температура, влажность, мощность нагревателя, статус вентилятора
- **Вес** (`WeightsPayload`): данные с датчиков веса
- **Статус** (`UnitStatus`): режим работы (`Mode`), целевые параметры, elapsed time
- **События** (`EventPayload`): severity, source, event, message
- **RFID** (`RfidEventPayload`): tag_detected, tag_removed
- **Info** (`InfoPayload`): версии HW/FW, счётчик наработки
- **Config** (`ConfigPayload`): конфигурация меню устройства

### 2. Формат UART кадров
Определить бинарный протокол:
- Стартовый байт `0xAA`, версия протокола, флаги ACK/ошибок
- Тип сообщения (`MessageKind`), длина, payload, CRC16
- Тайминги: `HEARTBEAT_INTERVAL_MS`, `COMMAND_REPLY_TIMEOUT_MS`, `LINK_LOSS_TIMEOUT_MS`

### 3. Словарь команд (маппинг на MQTT API)
**RP2040 → ESP32 (для публикации в MQTT):**
| UART MessageKind | MQTT Topic | Частота |
|------------------|------------|---------|
| `Telemetry` | `idryer/{sn}/telemetry` | 5 сек |
| `Weights` | `idryer/{sn}/weights` | 10 сек |
| `Status` | `idryer/{sn}/status` | При изменении |
| `Event` | `idryer/{sn}/events` | При событии |
| `Rfid` | `idryer/{sn}/rfid` | При изменении |
| `Info` | `idryer/{sn}/info` | При включении |
| `Config` | `idryer/{sn}/config` | По запросу |

**ESP32 → RP2040 (из MQTT команд):**
| MQTT Command | UART MessageKind | Действие |
|--------------|------------------|----------|
| `commands/start` | `CmdStart` | Запуск DRYING/STORAGE/PROFILE |
| `commands/stop` | `CmdStop` | Остановка юнита |
| `commands/get_config` | `CmdGetConfig` | Запрос конфигурации |
| `commands/set_config` | `CmdSetConfig` | Обновление конфигурации |

Дополнительно:
- `Heartbeat` — проверка связи ESP↔RP2040
- `WifiStatus` — ESP сообщает RP2040 о состоянии сети
- `Ack` — подтверждения команд

### 4. Структуры payload

> **ВАЖНО**: Структуры определены в библиотеке `idryer-protocol`.
> См. [task-00-protocol-library.md](./task-00-protocol-library.md)

Использовать готовые типы из библиотеки:

```c
#include <idryer_protocol.h>

// Типы уже определены:
// - idryer_telemetry_unit_t  (RP2040 → ESP32)
// - idryer_status_unit_t     (RP2040 → ESP32)
// - idryer_event_payload_t   (RP2040 → ESP32)
// - idryer_cmd_start_t       (ESP32 → RP2040)
// - idryer_cmd_stop_t        (ESP32 → RP2040)
```

Дополнительно определить UART-специфичные обёртки в `idryer_uart_types.h`:

```c
// Тип сообщения UART
typedef enum {
    UART_MSG_TELEMETRY    = 0x01,
    UART_MSG_WEIGHTS      = 0x02,
    UART_MSG_STATUS       = 0x03,
    UART_MSG_EVENT        = 0x04,
    UART_MSG_RFID         = 0x05,
    UART_MSG_INFO         = 0x06,
    UART_MSG_CONFIG       = 0x07,
    UART_MSG_CMD_START    = 0x10,
    UART_MSG_CMD_STOP     = 0x11,
    UART_MSG_CMD_GET_CFG  = 0x12,
    UART_MSG_CMD_SET_CFG  = 0x13,
    UART_MSG_ACK          = 0x20,
    UART_MSG_NACK         = 0x21,
    UART_MSG_HEARTBEAT    = 0x30,
    UART_MSG_WIFI_STATUS  = 0x31,
} idryer_uart_msg_t;
```

### 5. Обработка ошибок
- `MAX_RETRIES = 3` для команд
- При 3 сбоях подряд: RP2040 генерирует `FAULT` + событие `UART_TIMEOUT`
- Поведение при `SequenceMismatch`, `Timeout`, `CrcMismatch`

### 6. Физический уровень
- 115200 бод, 8N1, без аппаратного flow-control
- Выделенные GPIO для UART (TX/RX)
- Документировать в `include/uart_protocol.h`

## Результат
- Утверждённый файл `include/uart_protocol.h` с перечислениями, структурами и константами
- Документация маппинга UART ↔ MQTT (соответствие [MQTT API](../docs/mqtt-api-kit/02-api-reference/types.md))
- Таблица GPIO ↔ роль для UART
- Диаграммы последовательностей

## Критерии приёмки
- Структуры UART payload полностью соответствуют MQTT типам из [types.md](../docs/mqtt-api-kit/02-api-reference/types.md)
- Все команды из [commands.md](../docs/mqtt-api-kit/02-api-reference/commands.md) имеют UART эквиваленты
- Есть диаграммы: boot → info → status → telemetry, command flow, error handling
- ESP32 и RP2040 могут реализовывать драйверы на единой структуре кадров
