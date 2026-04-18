# Инструменты разработчика iDryer Link

Набор инструментов для тестирования и отладки протоколов iDryer Link.

## 🔧 Инструменты тестирования

### test_uart_rp2040_emulator.py
**Назначение:** Эмулятор MCU (RP2040) для тестирования ESP32 без физического контроллера  
**Протокол:** UART версии 1 (синхронизирован с lib/idryer-protocol)

**Запуск:**
```bash
python3 test_uart_rp2040_emulator.py /dev/ttyUSB0
```

**Возможности:**
- ✅ Актуальные структуры данных (HelloPayload 86 байт, StatusEntry 32 байта)
- ✅ Двусторонняя коммуникация (обработка Command, ConfigPush)
- ✅ Interactive режим с автоответами
- ✅ Поддержка всех MessageKind и флагов протокола
- ✅ Эмуляция units topology, RFID событий, claiming

**Обновления v2.0:**
- Исправлены критические несоответствия размеров структур
- Добавлена обработка входящих сообщений от ESP32
- Поддержка ConfigPush/ConfigAck для remote config
- Interactive режим для полноценного тестирования

---

### emulate_controller.py
**Назначение:** Продвинутый эмулятор RP2040 с аргументами командной строки  
**Статус:** ✅ Актуален (использует правильные структуры данных)

**Запуск:**
```bash
python3 emulate_controller.py --port /dev/ttyUSB0 --units 2 --fw-major 2
```

**Параметры:**
- `--port` — UART порт (по умолчанию /dev/cu.usbserial-130)
- `--baud` — скорость (по умолчанию 115200)
- `--units` — количество юнитов 1-4 (по умолчанию 2)
- `--fw-major` — MAJOR версия firmware (по умолчанию 2)
- `--session` — длительность сессии в секундах (по умолчанию 120)
- `--rfid` — отправить RFID событие через 15с

**Автоматический режим:**
- Отправляет Hello через 2с после запуска
- Периодическая телеметрия (5с), статус (10с), heartbeat (5с)
- Реагирует на HelloRequest, Command, ConfigPush
- Поддержка версионирования протокола

---

### mock_portal.py
**Назначение:** Mock Backend для тестирования cloud flow без реального сервера  
**Статус:** ✅ Актуален

**Запуск:**
```bash
pip install flask python-socketio eventlet
python3 mock_portal.py
```

**Сервер:** http://0.0.0.0:5050

**Настройка ESP32:**
```ini
build_flags =
  -DIDRYER_API_BASE="http://192.168.1.100:5050/api"
  -DMQTT_USE_TLS=0
```

**API endpoints:**
- `POST /api/devices/provision` — получить токен
- `POST /api/devices/register` — получить PIN для claiming  
- `GET /api/devices/check-claim/{token}` — проверить claiming

**Логика:**
- provision выдает токен `mock-token-{serial}`
- register выдает PIN `12345678`
- check-claim автоматически подтверждает при первом вызове

---

### read_serial.py
**Назначение:** Простой монитор UART для чтения логов ESP32  
**Статус:** ✅ Актуален

**Запуск:**
```bash
python3 read_serial.py
```

**Возможности:**
- Автоматический reset ESP32 через DTR/RTS
- Чтение 60 секунд с выводом в stdout
- Обработка Unicode ошибок

**Настройка:** Отредактируйте переменные `port` и `baud` в файле

---

## 🛠 Build инструменты

### extra_scripts/copy_firmware.py
**Назначение:** Post-build скрипт для копирования firmware в две папки  
**Статус:** ✅ Исправлен и работает

**Функции:**
- Копирует firmware.bin, bootloader.bin, partitions.bin, boot_app0.bin
- Локальная папка: `firmware/<board>/`
- Flasher Portal: `/Users/ruslanpavlucenko/Projects/iDryerPortal/flasher-portal/firmware/link/<slot>/<board>/`
- Правильный маппинг имен плат для flasher-portal
- Определяет slot (prod/stage) по имени environment

**Исправления v2.0:**
- Добавлено копирование в локальную папку firmware/
- Исправлен маппинг имен плат (esp32c3-super-mini остается с дефисами)
- Улучшенные логи с индикацией успеха

### extra_scripts/copy_menu.py  
**Назначение:** Pre-build скрипт для синхронизации menu файлов с RP2040  
**Статус:** ✅ Актуален

**Функции:**
- Копирует menu_meta.h, menu_ids.h, menu_cache.h/cpp из RP2040 проекта
- Копирует version.h для синхронизации версий
- Создает library.json для lib/idryer-menu
- Поддержка симлинков и кэширования

---

## 📋 Использование

### Базовое тестирование ESP32
```bash
# 1. Запустить эмулятор MCU
python3 tools/emulate_controller.py --port /dev/ttyUSB0 --units 2

# 2. В другом терминале - мониторинг логов
python3 tools/read_serial.py
```

### Тестирование cloud flow
```bash
# 1. Запустить mock backend
python3 tools/mock_portal.py

# 2. Собрать ESP32 с mock настройками
# 3. Запустить эмулятор с claiming
python3 tools/emulate_controller.py --port /dev/ttyUSB0 --rfid
```

### Интерактивное тестирование протокола
```bash
# Запустить интерактивный эмулятор
python3 tools/test_uart_rp2040_emulator.py /dev/ttyUSB0

# Выбрать опцию 'I' для interactive режима
# Эмулятор будет отвечать на команды от ESP32
```

## ✅ Статус актуальности

| Инструмент | Статус | Протокол | Обновлен |
|------------|--------|----------|----------|
| test_uart_rp2040_emulator.py | ✅ v2.0 | UART v1 | 2026-03-25 |
| emulate_controller.py | ✅ Актуален | UART v1 | Проверен |
| mock_portal.py | ✅ Актуален | HTTP API | Проверен |
| read_serial.py | ✅ Актуален | Serial | Проверен |
| copy_firmware.py | ✅ Актуален | Build | Проверен |
| copy_menu.py | ✅ Актуален | Build | Проверен |

Все инструменты синхронизированы с текущим протоколом lib/idryer-protocol версии 1.