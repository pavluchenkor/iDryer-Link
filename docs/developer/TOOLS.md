# Инструменты разработчика

Все инструменты находятся в папке `tools/` и синхронизированы с протоколом lib/idryer-protocol версии 1.

## Быстрый старт

### Тестирование ESP32 с эмулятором MCU
```bash
# Автоматический режим
python3 tools/emulate_controller.py --port /dev/ttyUSB0 --units 2

# Интерактивный режим  
python3 tools/test_uart_rp2040_emulator.py /dev/ttyUSB0
```

### Тестирование cloud flow
```bash
# Mock backend
python3 tools/mock_portal.py

# Мониторинг логов
python3 tools/read_serial.py
```

## Инструменты

| Инструмент | Назначение | Статус |
|------------|------------|--------|
| **test_uart_rp2040_emulator.py** | Интерактивный эмулятор MCU | ✅ v2.0 |
| **emulate_controller.py** | Автоматический эмулятор MCU | ✅ Актуален |
| **mock_portal.py** | Mock Backend для cloud flow | ✅ Актуален |
| **read_serial.py** | Serial монитор ESP32 | ✅ Актуален |

Подробная документация: [tools/README.md](../../tools/README.md)

## Критические обновления v2.0

- **Исправлены структуры данных** — HelloPayload (86 байт), StatusEntry (32 байта), HeartbeatPayload (9 байт)
- **Добавлена двусторонняя коммуникация** — обработка Command, ConfigPush от ESP32
- **Поддержка всех MessageKind** — включая WebSocket, RFID Data, Error сообщения
- **Interactive режим** — полноценное тестирование протокола с автоответами

Все инструменты синхронизированы с исходным кодом lib/idryer-protocol/src/uart/uart_protocol.h
