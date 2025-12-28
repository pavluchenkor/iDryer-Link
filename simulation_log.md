# Локальная симуляция регистрации и обмена с порталом

Этот документ описывает, как в текущем репозитории воспроизвести полный сценарий «первичного клайминга» устройства и проверить приём команд. Набор шагов полезен разработчикам для регрессионных проверок, когда backend недоступен.

## 1. Подготовка окружения

1. Установить зависимости mock-портала (однократно):
   ```bash
   python3 -m pip install --user flask "python-socketio<6" eventlet
   ```
2. Очистить MCU, чтобы стереть сохранённый `deviceId`:
   ```bash
   PLATFORMIO_HOME_DIR=/tmp/piohome pio run -t erase
   ```
3. Пересобрать и прошить основную прошивку:
   ```bash
   PLATFORMIO_HOME_DIR=/tmp/piohome pio run -t upload
   ```

## 2. Запуск mock-портала

Mock сервер обслуживает REST (`/api/devices/register`, `/api/devices/check-claim/<token>`) и Socket.IO (включая циклическую отправку `start/pause/resume/stop`). Логи пишутся в `/tmp/mock_portal.log`.

```bash
python3 scripts/mock_portal.py > /tmp/mock_portal.log 2>&1
```

В логах должны появиться строки вида:

```
[MOCK] register token=HARDWARE_TOKEN serial=IDRYER-PRO-0000
192.168.1.7 - - "POST /api/devices/register HTTP/1.1" 200 ...
[MOCK] claim check token=HARDWARE_TOKEN
192.168.1.7 - - "GET /api/devices/check-claim/HARDWARE_TOKEN HTTP/1.1" 200 ...
[MOCK] broadcast command -> {'command': 'start', ...}
```

## 3. Снятие UART-логов

Для удобства используем скрипт, эмулирующий RP2040 и записывающий последовательный порт в файл:

```bash
python3 scripts/emulate_controller.py --session 200 > /tmp/emulator.log 2>&1
```

Ключевые ожидания в `/tmp/emulator.log`:

* После загрузки — `[NET] init with token ... deviceId IDRYER-PRO-0000`
* Успешное подключение Wi‑Fi/Socket.IO:
  ```
  [NET] Wi-Fi connected, IP: 192.168.1.7
  [NET] Connecting Socket.IO ...
  [NET] Socket event type=48 ...
  [NET] Socket connected: /socket.io/?EIO=4
  ```
* Отправка телеметрии: `[NET] Sending telemetry len=220 heap=...`
* Приём команд (если включен `DEBUG_SOCKET_COMMAND` или активен mock): `[NET] Event device:command`.

## 4. Включение отладочной инъекции команд (опционально)

Для офлайн-проверки команд можно временно пересобрать прошивку с `DEBUG_SOCKET_COMMAND`. В этом режиме `NetworkManager` однажды прокрутит команды `start → pause → resume → stop`, которые проходят через обычный парсер:

```bash
PLATFORMIO_BUILD_FLAGS="-DDEBUG_SOCKET_COMMAND" PLATFORMIO_HOME_DIR=/tmp/piohome pio run -t upload
```

В UART-логе появятся строки:

```
[NET] Raw socket payload len=...
[NET] Event device:command
```

Не забывайте возвращаться к штатной сборке (`pio run -t upload` без дополнительных флагов) после завершения теста.

## 5. Памятка по логам

* `/tmp/mock_portal.log` — HTTP/Socket сервер, выводит REST обращения, телеметрию и команды.
* `/tmp/emulator.log` — UART ESP8266↔RP2040: Wi‑Fi статус, телеметрия, события Socket.IO.
* При необходимости можно увеличить длительность сессии (`--session`), чтобы получить несколько циклов команд.

Эти шаги покрывают весь протокол клайминга: регистрация → polling `check-claim` → Socket.IO connect → `device:command` + `telemetry:data`. Отклонения (например, отсутствие `check-claim` в логах) служат индикатором регрессий.
