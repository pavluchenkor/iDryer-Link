# Задача 04 — Тестирование, диагностика и эксплуатационные сценарии

> **Платформа**: ESP32-C3, ESP32-S3
>
> **Зависимости**: Все предыдущие задачи (00-03)
>
> **Ссылки на API:**
> - [MQTT API README](../docs/mqtt-api-kit/README.md)
> - [Scenarios](../docs/mqtt-api-kit/02-api-reference/scenarios.md)
> - [Troubleshooting](../docs/mqtt-api-kit/01-getting-started/first-touch.md#troubleshooting)

## Цель
Подготовить полный цикл проверки: UART ↔ ESP32 ↔ MQTT ↔ Backend, убедиться в стабильной работе и получить инструменты для отладки при внедрении.

## Основные шаги

### 1. Автоматизированные тесты

**Unit-тесты ESP32:**
```
test/
├── uart/
│   ├── test_uart_parser.cpp       # Парсинг UART кадров
│   ├── test_uart_serializer.cpp   # Сериализация команд
│   └── test_uart_bridge.cpp       # Интеграция UartBridge
├── mqtt/
│   ├── test_mqtt_payloads.cpp     # JSON payload соответствие типам
│   └── test_mqtt_commands.cpp     # Обработка команд
└── provisioning/
    └── test_first_touch.cpp       # Provisioning flow
```

**Тестовые сценарии** (из [scenarios.md](../docs/mqtt-api-kit/02-api-reference/scenarios.md)):

| Сценарий | Описание | Проверки |
|----------|----------|----------|
| First Boot | Первое включение устройства | provision → register → claim → MQTT connect |
| Normal Operation | Штатная работа | telemetry каждые 5с, weights каждые 10с, status retained |
| Start Drying | Запуск сушки | `commands/start` → UART → ACK → `events` COMMAND_ACK |
| Stop | Остановка | `commands/stop` → UART → ACK → status IDLE |
| Profile Drying | Профильная сушка | Несколько этапов, события STAGE_CHANGED, PROFILE_COMPLETED |
| Error Handling | Обработка ошибок | critical event → status FAULT |
| Network Loss | Потеря сети | Reconnect, буферизация, восстановление |
| UART Fault | Ошибка UART | 3 таймаута → UART_FAULT event |

### 2. Инструменты тестирования

**Python UART Simulator** (`tools/uart_simulator.py`):
```python
# Генерация тестовых UART пакетов
class UartSimulator:
    def send_telemetry(self, unit_id, temp, humidity, heater_power):
        """Эмуляция телеметрии от RP2040"""

    def send_status(self, unit_id, mode, target_temp, elapsed):
        """Эмуляция статуса от RP2040"""

    def send_event(self, severity, source, event, message):
        """Эмуляция события от RP2040"""

    def expect_command(self, timeout=5) -> UartCommand:
        """Ожидание команды от ESP32"""

    def send_ack(self, command_type, success=True):
        """Отправка ACK на команду"""
```

**MQTT Test Client** (`tools/mqtt_test_client.py`):
```python
# Подключение к брокеру как Backend
class MqttTestClient:
    def subscribe_device(self, serial_number):
        """Подписка на все топики устройства"""

    def send_command(self, serial_number, command, payload):
        """Отправка команды на устройство"""

    def wait_for_telemetry(self, timeout=10) -> TelemetryPayload:
        """Ожидание телеметрии"""

    def wait_for_event(self, event_type, timeout=30) -> EventPayload:
        """Ожидание конкретного события"""
```

### 3. Мониторинг и логирование

**Уровни логов:**
```cpp
#define LOG_LEVEL_ERROR   0
#define LOG_LEVEL_WARN    1
#define LOG_LEVEL_INFO    2
#define LOG_LEVEL_DEBUG   3
#define LOG_LEVEL_VERBOSE 4
```

**Цветной вывод в `pio device monitor`:**
```ini
[env:esp32]
monitor_filters = esp32_exception_decoder, colorize
monitor_speed = 115200
build_flags =
    -DCORE_DEBUG_LEVEL=3
    -DLOG_LOCAL_LEVEL=ESP_LOG_DEBUG
```

**Логируемые события:**
- `[WIFI]` — состояние Wi-Fi (connect, disconnect, IP)
- `[PROV]` — provisioning (provision, register, claim)
- `[MQTT]` — MQTT (connect, disconnect, publish, subscribe)
- `[UART]` — UART (rx, tx, error, timeout)
- `[CMD]` — команды (received, ack, timeout)

### 4. Стендовые проверки

**Чек-лист первичной проверки:**
```
□ ESP32 включается, подключается к Wi-Fi
□ Provisioning: POST /provision → получен token
□ Register: POST /register → получен PIN (отображается)
□ Claim: PIN введён в UI → claimed: true
□ MQTT: подключение к mqtt.idryer.org:8883
□ Info: топик info публикуется (retained)
□ Telemetry: данные каждые 5 секунд
□ Status: топик status публикуется (retained)
□ Commands: start → ACK → status DRYING
□ Commands: stop → ACK → status IDLE
□ Events: COMMAND_ACK публикуется
□ Reconnect: выключить Wi-Fi → восстановление
```

**Требования к стенду:**
- ESP32-C3 DevKit / ESP32-S3 DevKit
- USB для питания и отладки
- UART подключение к RP2040 (или UART Simulator)
- Доступ к Wi-Fi сети
- MQTT брокер (mqtt.idryer.org или локальный EMQX)

### 5. Проверка соответствия API

**Валидация JSON payload:**
```python
# Проверка что ESP32 отправляет валидный JSON
def validate_telemetry(payload: dict):
    assert "units" in payload
    assert "timestamp" in payload
    for unit in payload["units"]:
        assert "unitId" in unit
        assert "temperature" in unit
        assert "humidity" in unit
```

**Типы из [types.md](../docs/mqtt-api-kit/02-api-reference/types.md):**
- `TelemetryPayload` — температура, влажность, мощность
- `StatusPayload` — mode, target, elapsedTime
- `EventPayload` — severity, source, event, message
- `InfoPayload` — hardwareVersion, firmwareVersion, workTimeCounter

### 6. Стресс-тестирование

**Длительный тест (>1 час):**
- Непрерывная отправка телеметрии (5с интервал)
- Периодическая отправка команд (каждые 5 минут)
- Симуляция потери сети (каждые 15 минут)
- Проверка отсутствия memory leak
- Проверка отсутствия потери сообщений

**Метрики:**
- Uptime ESP32
- Количество reconnect MQTT
- Количество UART ошибок
- Latency команд (send → ACK)

### 7. OTA обновление

**Документировать процедуру:**
- Сборка прошивки: `pio run -e esp32-production`
- Загрузка firmware.bin на сервер
- Команда OTA update через MQTT (если реализовано)
- Fallback при неудачном обновлении

## Результат
- Репродуцируемый тестовый набор (`pio test`)
- Python инструменты для диагностики (uart_simulator, mqtt_test_client)
- Чек-листы для стендовых проверок
- Документация по troubleshooting

## Критерии приёмки
- Все unit-тесты проходят локально и в CI
- JSON payload соответствует типам из [types.md](../docs/mqtt-api-kit/02-api-reference/types.md)
- Все сценарии из [scenarios.md](../docs/mqtt-api-kit/02-api-reference/scenarios.md) протестированы
- Стресс-тест >1 час без ошибок
- Есть логирование: UART timeout, MQTT reconnect, provisioning errors
- Документация достаточна для эксплуатации
