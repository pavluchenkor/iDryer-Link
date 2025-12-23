# Задача 03 — Реализация UART↔MQTT бриджа на ESP32-C3/S3

> **Платформа**: ESP32-C3, ESP32-S3
>
> **Зависимости**:
> - [task-00-protocol-library.md](./task-00-protocol-library.md) — типы из `idryer-protocol`
> - [task-01-uart-architecture.md](./task-01-uart-architecture.md) — UART протокол
> - [task-02-esp-network-stack.md](./task-02-esp-network-stack.md) — MQTT клиент
>
> **Ссылки на API:**
> - [Types (Payload структуры)](../docs/mqtt-api-kit/02-api-reference/types.md)
> - [Device → Backend](../docs/mqtt-api-kit/02-api-reference/device-to-backend.md)
> - [Commands](../docs/mqtt-api-kit/02-api-reference/commands.md)
> - [ESP32 Publishing](../docs/mqtt-api-kit/04-esp32/publishing.md)
> - [ESP32 Commands Handler](../docs/mqtt-api-kit/04-esp32/commands-handler.md)

## Цель
Создать модуль, который:
1. Принимает данные от RP2040 по UART → конвертирует в JSON → публикует в MQTT топики
2. Получает MQTT команды → конвертирует в UART пакеты → отправляет на RP2040

UART — единственный интерфейс между ESP32 и RP2040.

## Основные шаги

### 1. UART драйвер
- Драйвер UART (HardwareSerial) с буферизацией, таймаутами и коллбеками
- Парсер бинарных кадров: валидация заголовков, длины, CRC16
- Преобразование в структуры C++ (из задачи 01)

### 2. UART → MQTT (публикация данных)

**Конвертация UART пакетов в MQTT payload** ([types.md](../docs/mqtt-api-kit/02-api-reference/types.md)):

| UART MessageKind | MQTT Topic | JSON Payload |
|------------------|------------|--------------|
| `Telemetry` | `telemetry` | `TelemetryPayload` |
| `Weights` | `weights` | `WeightsPayload` |
| `Status` | `status` | Массив `UnitStatus` |
| `Event` | `events` | `EventPayload` |
| `Rfid` | `rfid` | `RfidEventPayload` |
| `Info` | `info` | `InfoPayload` |
| `Config` | `config` | `ConfigPayload` |

**Пример конвертации Telemetry:**
```cpp
// UART → JSON
void publishTelemetry(const UartTelemetryUnit& uart) {
    StaticJsonDocument<256> doc;
    JsonArray units = doc.createNestedArray("units");
    JsonObject unit = units.createNestedObject();

    unit["unitId"] = uart.unitId;
    unit["temperature"] = uart.temperature / 10.0;  // int16 → float
    unit["humidity"] = uart.humidity;
    unit["heaterPower"] = uart.heaterPower;
    unit["fanStatus"] = uart.fanStatus;

    doc["timestamp"] = getIso8601Timestamp();

    String json;
    serializeJson(doc, json);
    mqtt.publish("idryer/" + serialNumber + "/telemetry", json, QoS0);
}
```

**Пример конвертации Status:**
```cpp
void publishStatus(const UartStatusUnit& uart) {
    StaticJsonDocument<512> doc;
    JsonArray units = doc.createNestedArray("units");
    JsonObject unit = units.createNestedObject();

    unit["unitId"] = uart.unitId;
    unit["mode"] = modeToString(uart.mode);  // IDLE, DRYING, STORAGE, PROFILE, FAULT

    if (uart.mode != MODE_IDLE && uart.mode != MODE_FAULT) {
        JsonObject target = unit.createNestedObject("target");
        target["temperature"] = uart.targetTemp / 10.0;
        target["duration"] = uart.duration == 0xFFFF ? nullptr : uart.duration;
        target["humidity"] = uart.targetHumidity;
        unit["elapsedTime"] = uart.elapsedTime;

        // Для PROFILE режима
        if (uart.mode == MODE_PROFILE) {
            unit["currentStage"] = uart.currentStage;
            unit["totalStages"] = uart.totalStages;
        }
    }

    doc["timestamp"] = getIso8601Timestamp();
    mqtt.publish("idryer/" + serialNumber + "/status", json, QoS1, RETAINED);
}
```

### 3. MQTT → UART (обработка команд)

**Команды из [commands.md](../docs/mqtt-api-kit/02-api-reference/commands.md):**

```cpp
void handleMqttCommand(const String& topic, const String& payload) {
    StaticJsonDocument<512> doc;
    deserializeJson(doc, payload);

    if (topic.endsWith("/commands/start")) {
        UartCmdStartPayload cmd;
        strncpy(cmd.unitId, doc["unitId"], 4);
        cmd.mode = modeFromString(doc["mode"]);  // DRYING, STORAGE, PROFILE

        if (doc.containsKey("params")) {
            cmd.temperature = doc["params"]["temperature"].as<float>() * 10;
            cmd.duration = doc["params"]["duration"] | 0xFFFF;
            cmd.humidity = doc["params"]["humidity"] | 0;
        }

        sendUartCommand(CMD_START, &cmd, sizeof(cmd));
    }
    else if (topic.endsWith("/commands/stop")) {
        UartCmdStopPayload cmd;
        strncpy(cmd.unitId, doc["unitId"], 4);
        sendUartCommand(CMD_STOP, &cmd, sizeof(cmd));
    }
    else if (topic.endsWith("/commands/get_config")) {
        sendUartCommand(CMD_GET_CONFIG, nullptr, 0);
    }
    else if (topic.endsWith("/commands/set_config")) {
        // Передать весь JSON как есть или сериализовать в бинарный формат
        sendUartCommand(CMD_SET_CONFIG, payload.c_str(), payload.length());
    }
}
```

### 4. FSM и очереди
- FSM обработки UART: `IDLE` → `RECEIVING` → `PARSING` → `PUBLISHING` → `IDLE`
- Очередь исходящих команд (MQTT → UART) с приоритетами
- Очередь входящих данных (UART → MQTT) для буферизации при переподключении MQTT

### 5. ACK и обработка ошибок
- При получении команды от MQTT → отправить в UART → ждать ACK от RP2040
- При получении ACK → опубликовать событие `COMMAND_ACK`:
```json
{
  "severity": "info",
  "event": "COMMAND_ACK",
  "message": "Start command received",
  "unitId": "U1",
  "data": { "command": "start", "status": "success" },
  "timestamp": "2025-12-08T12:00:01Z"
}
```
- При таймауте/ошибке → событие с `status: "failure"`
- 3 неудачных кадра подряд → событие `UART_FAULT`

### 6. API класса UartBridge
```cpp
class UartBridge {
public:
    void begin(HardwareSerial& serial, MqttClient& mqtt);
    void loop();  // Вызывать из main loop

    // Callbacks от MQTT
    void onMqttCommand(const String& topic, const String& payload);

    // Callbacks от UART
    using TelemetryCallback = std::function<void(const UartTelemetryUnit&)>;
    void onTelemetry(TelemetryCallback cb);

    // Статистика
    uint32_t getUartRxCount();
    uint32_t getUartTxCount();
    uint32_t getUartErrorCount();
};
```

### 7. Тесты
- Юнит-тесты парсинга UART кадров
- Тесты конвертации UART ↔ JSON
- Интеграционные тесты с моком MQTT
- Папка: `test/uart/test_uart_bridge.cpp`

## Результат
- Класс `UartBridge`, инкапсулирующий весь обмен UART ↔ MQTT
- Полное соответствие MQTT payload из [types.md](../docs/mqtt-api-kit/02-api-reference/types.md)
- Документация с примерами кадров и JSON payload
- Тесты парсинга и конвертации

## Критерии приёмки
- Нет потери данных при длительной работе (стресс-тест >1 час)
- Все MQTT топики из [device-to-backend.md](../docs/mqtt-api-kit/02-api-reference/device-to-backend.md) корректно публикуются
- Все команды из [commands.md](../docs/mqtt-api-kit/02-api-reference/commands.md) корректно обрабатываются
- RP2040 получает подтверждения о каждой принятой команде
- JSON payload соответствует TypeScript типам из API
- Тесты проходят локально
