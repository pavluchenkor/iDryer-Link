# Вспомогательные скрипты iDryer LINK

---

## emulate_controller.py — эмулятор RP2040

Эмулирует MCU (RP2040) на стороне хоста. Позволяет проверить работу LINK (ESP32)
без подключённой платы контроллера.

**Зависимости:**
```bash
pip install pyserial
```

**Запуск:**
```bash
python emulate_controller.py --port /dev/cu.usbserial-130 --baud 115200
```

**Параметры:**

| Параметр     | По умолчанию            | Описание |
|--------------|-------------------------|----------|
| `--port`     | `/dev/cu.usbserial-130` | UART порт (macOS/Linux) |
| `--baud`     | `115200`                | Скорость |
| `--session`  | `120`                   | Длительность сессии, секунд |
| `--fw-major` | `2`                     | MAJOR версия прошивки (для проверки совместимости с LINK) |
| `--units`    | `2`                     | Количество юнитов (1–4) |
| `--rfid`     | выкл                    | Отправить RFID tag_detected через 15 с |

**Что делает:**
- Через 2 с отправляет `Hello` с параметрами устройства
- Каждые 5 с — `Telemetry` и `Heartbeat`
- Каждые 10 с — `Status` (режим DRYING) и `Weights`
- Реагирует на `HelloRequest` от LINK — отправляет `Hello`
- Реагирует на `Command` — отправляет `CommandAck`
- Реагирует на `ConfigPush` — отправляет `ConfigAck`, выводит JSON в лог

**Примеры:**

```bash
# Тест с несовпадением версий (LINK major=1, эмулятор major=3)
python emulate_controller.py --fw-major 3

# Один юнит + RFID событие
python emulate_controller.py --units 1 --rfid

# Windows
python emulate_controller.py --port COM5
```

**Порт на macOS:** найти через `ls /dev/cu.*` или `ls /dev/tty.*`
**Порт на Linux:** обычно `/dev/ttyUSB0` или `/dev/ttyACM0`

---

## mock_portal.py — mock Backend для provisioning

Эмулирует HTTP API Backend для тестирования cloud flow без реального сервера.
Полезен при отладке WiFi provisioning и claiming.

**Зависимости:**
```bash
pip install flask python-socketio eventlet
```

**Запуск:**
```bash
python mock_portal.py
```

Сервер запускается на `0.0.0.0:5050`.

**Настройка LINK для работы с mock:**

Собрать прошивку с флагами:
```ini
build_flags =
  -DIDRYER_API_BASE="http://192.168.1.100:5050/api"
  -DMQTT_USE_TLS=0
```
где `192.168.1.100` — IP компьютера с mock в той же сети.

**Реализованные эндпоинты:**

| Метод | Путь | Описание |
|-------|------|----------|
| `POST` | `/api/devices/provision` | Получить токен по serialNumber |
| `POST` | `/api/devices/register`  | Получить PIN для claiming |
| `GET`  | `/api/devices/check-claim/{token}` | Проверить статус claiming |

**Логика mock:**
- `provision` — выдаёт токен `mock-token-{последние 8 символов serial}`
- `register` — выдаёт PIN `12345678`, `remainingSeconds=300`
- `check-claim` — автоматически подтверждает claiming при первом вызове

> **Примечание:** Socket.IO обработчики (`device:command`, `telemetry:data` и др.)
> оставлены в коде для совместимости со старыми тестами, но в текущей архитектуре
> LINK общается с Backend через **MQTT**, а не Socket.IO.

**MQTT для полного теста:** запустить локальный MQTT брокер (например Mosquitto):
```bash
# macOS
brew install mosquitto
mosquitto -p 1883

# Docker
docker run -p 1883:1883 eclipse-mosquitto
```

Прошивка со staging настройками и IP брокера вместо `mqtt.idryer.org`.
