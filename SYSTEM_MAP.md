# 🗺️ Карта системы iDryer

> **Единый источник правды** о том, что где находится и как компоненты общаются

---

## 🎯 Компоненты системы

### 1️⃣ RP2040 (Контроллер устройства)

**Местоположение:** `/Users/ruslanpavlucenko/Projects/iDryerRP2040`

**Отвечает за:**
- Управление железом (нагреватель, вентилятор, серво, датчики)
- Меню и настройки устройства
- Пресеты сушки (PLA, ABS, PETG и т.д.)
- Чтение/запись RFID меток (OpenPrintTag)
- Локальный UI (экран + кнопки)
- Система ошибок

**Источники правды:**
- `src/menu/menu_v2.yaml` - структура меню, все настройки
- `src/error/error_defs.h` - коды ошибок (severity, source, code)

**Что знает:**
- Все параметры устройства
- Текущий режим (DRYING/STORAGE/IDLE)
- Телеметрия (температура, влажность, вес)
- Ошибки и статусы
- RFID метки (читает UID, данные OpenPrintTag)
- UART протокол (для общения с ESP32)

**Чего НЕ знает:**
- Wi-Fi, интернет
- Пользователей, сессии в облаке
- MQTT протокол

---

### 2️⃣ ESP32 (Сетевой мост + экран)

**Местоположение:** `/Users/ruslanpavlucenko/Documents/PlatformIO/Projects/idryer-link`

**Отвечает за:**
- Wi-Fi подключение
- MQTT клиент (связь с Backend)
- Преобразование UART ↔ MQTT
- Аутентификация устройства (JWT токен)
- **Опционально:** управление экраном (в некоторых конфигурациях)

**Источники правды:**
- `lib/idryer-protocol` - UART/MQTT протоколы
- `include/uart_protocol.h` - структуры UART кадров

**Что знает:**
- UART протокол
- MQTT топики
- Как пересылать данные туда-сюда
- **С экраном:** парсит JSON конфиг, отображает меню, знает текущие значения

**Чего НЕ знает:**
- Структуру меню до получения JSON от RP2040
- Логику работы устройства (контроль температуры, PID)
- Как генерировать меню из YAML

**Роль:**
- **Без экрана:** "Dumb pipe" - умная труба для передачи данных
- **С экраном:** "Smart display" - парсит JSON, рендерит UI, отправляет команды

---

### 3️⃣ Backend (Портал iDryer)

**Местоположение:** `/Users/ruslanpavlucenko/Projects/iDryerPortal`

**Отвечает за:**
- Управление пользователями
- Сессии сушки (старт/стоп/история)
- База филаментов
- Статистика и аналитика
- Web интерфейс
- RFID OpenPrintTag генерация

**Источники правды:**
- `mqtt-api-kit/02-api-reference/commands.md` - команды Backend → Device
- `mqtt-api-kit/02-api-reference/device-to-backend.md` - топики Device → Backend
- База данных (Prisma)

**Что знает:**
- Пользователей и их устройства
- Историю сушки
- MQTT API для управления устройствами

**Чего НЕ знает:**
- Внутреннюю структуру меню RP2040
- Железо устройства
- UART протокол

---

## 🔄 Обмен данными

### RP2040 ← UART → ESP32

**Физика:** 115200 бод, 8N1

**Протокол:** Бинарные кадры (6 байт header + payload + CRC16)

| Направление | Что передаётся | Тип сообщения | Формат |
|-------------|----------------|---------------|--------|
| RP2040 → ESP32 | Телеметрия (каждые 1-15 сек) | `Telemetry` | `TelemetryPayload` (бинарный) |
| RP2040 → ESP32 | Конфиг меню (при старте/изменении языка) | `ConfigPush` | JSON (фрагментированный) |
| RP2040 → ESP32 | **События (ошибки, RFID, info)** | `Log` | `LogPayload` (текстовый) |
| RP2040 → ESP32 | Heartbeat (каждые 5 сек) | `Heartbeat` | `HeartbeatPayload` |
| ESP32 → RP2040 | Команды START/STOP | `Command` | `CommandPayload` (бинарный) |
| ESP32 → RP2040 | Запрос конфига | `Command` (GetConfig) | — |
| ESP32 → RP2040 | **Подтверждение ошибок** | `Command` (ClearErrors) | — |
| ESP32 → RP2040 | Статус WiFi | `HelloAck` | Содержит RSSI |

**Размер payload:** до 200 байт (фрагментация для больших JSON)

---

### ESP32 ← MQTT → Backend

**Протокол:** MQTTS (TLS, порт 8883), брокер `mqtt.idryer.org`

**Аутентификация:**
- Username: `serialNumber` (DEVICE_xxx)
- Password: JWT токен

#### Device → Backend (публикация)

| Топик | QoS | Retained | Частота | Содержимое |
|-------|-----|----------|---------|------------|
| `idryer/{serial}/info` | 1 | ✅ | При старте | Версии FW/HW |
| `idryer/{serial}/telemetry` | 0 | ❌ | Каждые 5 сек | Температура, влажность, состояние |
| `idryer/{serial}/status` | 1 | ✅ | При изменении | Режим работы (DRYING/STORAGE/IDLE) |
| `idryer/{serial}/weights` | 1 | ❌ | Каждые 10 сек | Вес филамента |
| `idryer/{serial}/rfid` | 1 | ✅ | При событии | Данные RFID метки |
| `idryer/{serial}/events` | 1 | ❌ | При событии | Ошибки, предупреждения |
| `idryer/{serial}/config` | 1 | ❌ | По запросу | JSON конфиг меню |

#### Backend → Device (подписка)

| Топик | QoS | Что делает |
|-------|-----|------------|
| `idryer/{serial}/commands/start` | 1 | Запуск сушки (mode: DRYING/STORAGE/PROFILE) |
| `idryer/{serial}/commands/stop` | 1 | Остановка юнита |
| `idryer/{serial}/commands/ping` | 0 | Синхронизация времени |
| `idryer/{serial}/commands/get_config` | 1 | Запрос конфига меню |
| `idryer/{serial}/commands/set_config` | 1 | Обновление настроек |
| `idryer/{serial}/commands/write_rfid` | 1 | Запись OpenPrintTag на RFID метку |

---

## 📋 Команды и действия

### Локальные команды (только на устройстве)

**Источник:** кнопки/экран RP2040

**Примеры** (полный список ~30+ команд в menu_v2.yaml):

| Действие | Где определено | Как вызывается |
|----------|----------------|----------------|
| `start_drying` | menu_v2.yaml | Кнопка "СТАРТ" в меню СУШКА |
| `start_storage` | menu_v2.yaml | Кнопка "СТАРТ" в меню ХРАНЕНИЕ |
| `start_drying_pla` | menu_v2.yaml | Пресет PLA (и другие: PETG, ABS, PA, PC...) |
| `calib_zero1` | menu_v2.yaml | Калибровка весов (для всех 4 контроллеров) |
| `pid_autotune_heater` | menu_v2.yaml | Автонастройка PID нагревателя/камеры |
| ... | menu_v2.yaml | Другие action-команды из меню |

**Эти команды НЕ доступны через MQTT** - только локально.

---

### Удалённые команды (через MQTT)

**Источник:** Backend → MQTT → ESP32 → UART → RP2040

| MQTT команда | UART команд код | Что делает |
|--------------|-----------------|------------|
| `commands/start` | `Start (0x01)` | Запуск режима (DRYING/STORAGE/PROFILE) |
| `commands/stop` | `Stop (0x02)` | Остановка юнита |
| `commands/get_config` | `GetConfig (0x05)` | Запрос JSON конфига |
| `commands/set_config` | `SetConfig (0x06)` | Применить настройки из JSON |
| `commands/write_rfid` | `WriteRfid (0x08)` | Записать OpenPrintTag на метку |

**⚠️ НЕТ в системе:**
- `Pause` / `Resume` - таких команд нет нигде
- Устройство само решает когда переходить между режимами

---

### Служебные команды (только UART)

| Команд код | От кого | Куда | Назначение |
|------------|---------|------|------------|
| `ResetFault (0x10)` | ESP32 | RP2040 | Сброс ошибки |
| `WifiStatus (0x11)` | RP2040 | ESP32 | Запрос IP адреса для отображения на экране |
| `ClearErrors (0x12)` | ESP32 | RP2040 | Пользователь подтвердил ошибки (очистка EEPROM лога) |

---

## ⚠️ Обработка ошибок при загрузке

### Сценарий: RP2040 загружается с ошибками в EEPROM

**1. RP2040 в setup():**
```cpp
void setup() {
  uart_init();  // Инициализация UART ПЕРЕД проверкой ошибок

  if (errlog_count() > 0) {
    // Отправляем ВСЕ ошибки из EEPROM на ESP32
    for (uint16_t i = 0; i < errlog_count(); i++) {
      ErrLogRec r;
      errlog_read_ith_oldest(i, &r);
      uart_send_log_event(&r);  // MessageKind::Log
    }

    // Блокируем устройство до подтверждения
    while (g_wait_error_ack) {
      watchdog_update();
      uart_process();  // Обрабатываем UART (ждем ClearErrors)
      displayLocalErrorOverlay();  // Показываем на своем экране
      delay(10);
    }
  }

  // ГАРАНТИЯ: ошибки подтверждены, можно работать
}
```

**2. ESP32 получает ошибки:**
```cpp
void onUartLog(LogPayload& log) {
  // Публикуем в MQTT events
  mqtt_publish_event(log);

  // С экраном: показываем список ошибок
  if (hasDisplay) {
    displayError(log);
  }
}

// Пользователь нажал "Clear" на экране ESP32
void onUserClearErrors() {
  uart_send_command(ClearErrors);  // → RP2040
}
```

**3. RP2040 получает ClearErrors:**
```cpp
void onCommandReceived(CommandCode cmd) {
  if (cmd == ClearErrors) {
    errlog_clear();           // Очистка EEPROM
    g_wait_error_ack = false;  // Разблокировка
  }
}
```

### Важные особенности:

**❌ Backend команды ИГНОРИРУЮТСЯ:**
- Пока `g_wait_error_ack == true`, RP2040 НЕ принимает команды START/STOP/SET_CONFIG
- ESP32 может отклонять команды Backend с ответом "Device in error state"

**✅ MQTT события публикуются:**
- Все ошибки отправляются в `idryer/{serial}/events`
- Backend видит что устройство в ошибке
- Опционально: Backend может показать статус "Awaiting error acknowledgment"

**🔄 UART всегда работает:**
- setup() не блокирует UART
- Обработка команд идёт в цикле while
- Watchdog обновляется

---

## 🗂️ Общие библиотеки (idryer-protocol)

**Планируется:** отдельный репозиторий `idryer-protocol`

**Будет содержать:**
```
idryer-protocol/
├── src/
│   ├── uart/              # UART протокол (FrameHeader, MessageKind)
│   ├── mqtt/              # MQTT клиент (топики, QoS)
│   ├── errors/            # error_defs.h (ERRSEV, ERRSRC, ERRC)
│   └── types/             # Общие типы (TelemetryPayload, CommandPayload)
├── docs/
│   ├── json-protocol.md   # Формат JSON конфига меню
│   └── mqtt-api-kit/      # MQTT API документация
└── examples/
    ├── basic_uart/        # Пример для других контроллеров
    └── custom_display/    # Пример своего UI
```

**Подключение:**
- RP2040: `lib/idryer-protocol` (git submodule)
- ESP32: `lib/idryer-protocol` (git submodule)
- Сторонние разработчики: PlatformIO `lib_deps`

---

## 🔧 Система ошибок

**Источник правды:** `iDryerRP2040/src/error/error_defs.h`

**Формат:** 3 компонента (не enum!)

```c
ERRSEV_* (severity)  - INFO, WARN, ERROR, CRIT
ERRSRC_* (source)    - HEATER, AIR, THERM, SHT, SERVO, MODE...
ERRC_*   (code)      - OK, SENSOR_INVALID, OUT_OF_RANGE, TIMEOUT...
```

**В UART передаются (MessageKind::Log):**
```cpp
struct LogPayload {
  char severity[10];   // "critical", "error", "warning", "info"
  char source[20];     // "THERMISTOR", "HEATER", "SHT"...
  char event[32];      // "SENSOR_SHORT", "OVER_MAX"...
  char message[100];   // "Thermistor short circuit"
  uint8_t unitId;      // 0-3 (controller ID)
};  // 164 байта (влезает в MAX_PAYLOAD_SIZE=200)
```

**RP2040 формирует сообщение:**
```cpp
void sendError(uint8_t ctrl_id, ErrSource src, ErrSeverity sev, ErrCode code, const char* msg) {
  LogPayload log;
  strcpy(log.severity, errsev_name(sev));    // "ERROR" → "error"
  strcpy(log.source, errsrc_name(src));       // "HEATER"
  strcpy(log.event, errcode_name(code));      // "OVER_MAX"
  strcpy(log.message, msg);                   // "Value over maximum"
  log.unitId = ctrl_id;

  uart_send_log(&log);  // MessageKind::Log
}
```

**ESP32 пересылает в MQTT events:**
```json
{
  "severity": "error",
  "source": "HEATER",
  "event": "OVER_MAX",
  "message": "Value over maximum",
  "unitId": "U1",
  "timestamp": "2025-12-23T10:00:00Z"
}
```

**Преимущества:**
- RP2040 сам формирует текст (знает язык, контекст)
- ESP32 = dumb pipe (просто пересылает)
- Сторонние разработчики отправляют свои сообщения
- Мультиязычность (RP2040 выбирает язык)

---

## 📝 JSON конфиг меню

**Генерация:** RP2040 → JSON (из menu_v2.yaml)

**Передача:**
1. RP2040 сериализует menu в JSON (~4-5 KB)
2. Разбивает на фрагменты по 200 байт
3. Отправляет по UART с флагами `FLAG_FRAGMENTED`
4. ESP32 собирает фрагменты в буфер
5. Публикует готовый JSON в MQTT `config` топик

**Формат JSON:** см. `config-exmple/menu/json-link.md`

**Языки:** Передаётся только активный язык (не все переводы)

---

## 🎯 Принципы архитектуры

### 1. Источник правды - устройство (RP2040)
- Устройство знает всё о себе
- Backend только отправляет команды и получает статусы
- Backend НЕ управляет переходами между режимами

### 2. ESP32 - "dumb pipe"
- Не знает структуру меню
- Не парсит JSON (только пересылает)
- Не принимает решений

### 3. Один источник правды для каждого типа данных
- Меню → `menu_v2.yaml` (RP2040)
- Ошибки → `error_defs.h` (RP2040)
- MQTT команды → `commands.md` (Backend)
- UART протокол → `uart_protocol.h` (idryer-protocol)

### 4. Расширяемость
- Сторонние разработчики могут:
  - Делать свои контроллеры (используя idryer-protocol)
  - Делать свои экраны/UI (парсят JSON конфиг)
  - Подключаться к Backend (через MQTT API)

---

## 🚀 Следующие шаги

### Инфраструктура:
- [ ] Создать репозиторий `idryer-protocol`
- [ ] Перенести `error_defs.h` в `idryer-protocol/src/errors/`
- [ ] Перенести `uart_protocol.h` в `idryer-protocol/src/uart/`
- [ ] Подключить через git submodule в оба проекта (RP2040 + ESP32)

### Протокол UART:
- [x] Удалить устаревшие команды (Pause, Resume, RequestTelemetry, Identify)
- [x] Добавить команду `ClearErrors` (ESP32 → RP2040)
- [x] Добавить структуру `LogPayload` (для MessageKind::Log)
- [x] Удалить ошибки из `TelemetryPayload` (ошибки = события, не состояние)
- [x] Добавить флаги фрагментации (FLAG_FRAGMENTED, FLAG_LAST_FRAGMENT)
- [x] Увеличить MAX_PAYLOAD_SIZE до 200 байт
- [ ] Реализовать фрагментацию JSON в UART (для ConfigPush)
- [ ] Реализовать обработку Log сообщений в ESP32

### RP2040 (Контроллер):
- [ ] Реализовать `uart_send_log_event()` - отправка LogPayload
- [ ] Реализовать отправку всех ошибок из EEPROM при загрузке
- [ ] Добавить обработку команды `ClearErrors` → очистка EEPROM лога
- [ ] Блокировка команд Backend при `g_wait_error_ack == true`
- [ ] Реализовать генератор menu → JSON (сериализация menu_v2.yaml)

### ESP32 (Сетевой мост):
- [ ] Реализовать обработку LogPayload → публикация в MQTT events
- [ ] Реализовать отклонение Backend команд при ошибках (опционально)
- [ ] С экраном: UI для просмотра/подтверждения ошибок
- [ ] С экраном: отправка команды ClearErrors при нажатии кнопки

### Backend:
- [ ] Обработка статуса "Device awaiting error acknowledgment" (опционально)
- [ ] Хранение истории ошибок в БД
- [ ] UI для просмотра ошибок устройства

---

**Последнее обновление:** 2025-12-23
**Автор:** Ruslan Pavlucenko
