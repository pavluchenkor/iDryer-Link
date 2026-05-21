# fake_moonraker.py

Локальный заглушечный WebSocket-сервер, имитирующий Moonraker для проверки
интеграции iHeater Link без реального Klipper/принтера.

## Назначение

Проверка цепочки:

```
fake_moonraker → ESP32 (LinkIntegrationsManager → onVirtualChamberUpdate)
              → auto_heat (filament-type lookup в menu)
              → RmtOutputAdapter (RMT pulse) → STM32 iHeater
              → mirrorRmtToStatus → MQTT status → портал
```

## Что делает

- Слушает `ws://0.0.0.0:7125/websocket`.
- Отвечает на JSON-RPC: `server.connection.identify`, `server.info`,
  `printer.objects.list`, `printer.objects.subscribe`.
- После `printer.objects.subscribe` шлёт `notify_status_update` с объектом
  `VIRTUAL_CHAMBER.target` всем подписчикам.
- Каждые 10 секунд переключает `target` между `0.0` и `70.0` °C
  (heartbeat — провоцирует ESP на ON/OFF цикл).

## Запуск

```bash
pip3 install --user websockets
python3 tools/fake_moonraker.py
```

В логе появится:

```
listening on 0.0.0.0:7125 (path=/websocket)
+ client 192.168.0.x
← server.connection.identify id=1
← printer.objects.subscribe id=2
→ notify VIRTUAL_CHAMBER.target=0.0
→ notify VIRTUAL_CHAMBER.target=70.0
```

## Настройка интеграции в портале

iHeater Link → Settings → Moonraker:

| Поле     | Значение            |
| -------- | ------------------- |
| host     | `<IP вашего мака>`  |
| port     | `7125`              |
| path     | `/websocket`        |
| enabled  | `true`              |

IP мака:

```bash
ipconfig getifaddr en0
```

ESP32 и мак должны быть в одной WiFi-сети.

Включение Moonraker в портале автоматически выключает Bambu/HA в меню
устройства (exclusivity в `MenuBridge`).

## Ожидаемый Serial-вывод на ESP

```
[INFO ] MQTT: ← .../commands/link_integration ... {"type":"moonraker", "enabled":true, ...}
[CMD  ] link_integration: switched active to moonraker
[HEATER] VIRTUAL_CHAMBER: target=70.0 → output=ON
[RMT→status] mode=Drying target=70.0
[DEBUG] MQTT: → .../status (...): {"unitsInfo":[{"mode":"Drying","target":{"temperature":70 ...}}]}
```

## Диагностика

- ESP не подключается → проверьте firewall на маке (System Settings →
  Network → Firewall → разрешить входящие подключения для `python3`).
- Подключение установилось, но `VIRTUAL_CHAMBER` не приходит → ESP не шлёт
  `printer.objects.subscribe`. Проверьте что в портале `enabled:true` и host
  указан корректно.
- Pylance ругается «websockets не разрешается» — ложноположительное:
  библиотека стоит в `~/Library/Python/3.9/`, runtime её видит.
