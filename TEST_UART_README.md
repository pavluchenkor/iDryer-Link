# Тестирование UART протокола

## 🎯 Что проверяем:
- `FLAG_ACK_REQUIRED` → автоматически включает retry
- Обработчики вызываются
- JSON формируется правильно
- Отладочная печать через DEBUG_SERIAL

---

## 📋 Подготовка:

### 1. Прошей ESP32:
```bash
pio run -e esp32-c3 -t upload
```

### 2. Открой Serial Monitor:
```bash
pio device monitor -e esp32-c3
```

### 3. Установи Python зависимости:
```bash
pip3 install pyserial
```

---

## 🚀 Запуск теста:

### Вариант 1: Полноценный симулятор сушилки (рекомендуется)

**Что делает:**
- Физическая модель (нагрев, охлаждение, влажность, вес)
- Автоматическая отправка телеметрии (каждые 5 сек)
- Интерактивное управление (клавиши 1-5, S, Q)
- Симуляция процесса сушки с программами

**Подключение:**
- ESP32 TX (GPIO7) → USB-UART RX
- ESP32 RX (GPIO6) → USB-UART TX
- GND → GND

**Запуск:**
```bash
python3 dryer_simulator.py /dev/ttyUSB0
```

**Управление:**
```
1-5 - Запуск программы сушки (PETG/TPU/ABS/PA/PC)
S   - Стоп сушки (IDLE)
Q   - Выход
```

**Что увидишь:**
```
[12:30:15] → Telemetry: 25.0°C, 60.0%, heater=0%
[12:30:20] → Telemetry: 26.5°C, 58.5%, heater=100%
[12:30:25] → Weight: 998.5g
[12:30:30] → Status: DRYING (session #1, elapsed=15s)
```

---

### Вариант 2: Ручной эмулятор (для тестирования протокола)

**Подключение:** как в Варианте 1

**Запуск:**
```bash
python3 test_uart_rp2040_emulator.py /dev/ttyUSB0
```

**В меню:**
```
1. Send Hello           - Hello от "RP2040"
2. Send Telemetry      - Телеметрия с FLAG_ACK_REQUIRED
3. Send Weights        - Вес филамента
4. Send Status         - Статус работы
5. Send RFID Event     - RFID событие
6. Send Heartbeat      - Heartbeat
7. Send All            - Отправка всех сообщений
8. Send ClaimStart     - Запрос процесса привязки устройства (Device Claiming)
```

---

### Вариант 3: Loopback (без эмулятора)

**Подключение:**
- Соедини проводом GPIO6 ↔ GPIO7

**Что увидишь:**
- ESP отправляет Heartbeat сам себе каждые 5 сек
- Видно retry если отключить loopback

---

## 📊 Что увидишь в Serial Monitor:

### При успешном приёме Telemetry:
```
[RECV] Telemetry (seq=1, count=2)
{"units":[{"unitId":0,"tempC":25.0,"humidity":45},{"unitId":1,"tempC":26.0,"humidity":50}]}
```

### При успешном приёме Heartbeat:
```
[RECV] Heartbeat (seq=5)
{"uptime":1234,"rssi":-50,"errors":0}
```

### При успешном приёме Weights:
```
[RECV] Weights (seq=2, count=2)
{"weights":[{"unitId":0,"weight":1000},{"unitId":1,"weight":1100}]}
```

### При успешном приёме Status:
```
[RECV] Status (seq=3, count=2)
{"uptime":5000,"units":[{"unitId":0,"mode":1,"sessionNum":100,"elapsed":300,"remaining":600,"stage":0},...]}
```

### При успешном приёме RFID:
```
[RECV] RFID (seq=4)
{"event":1,"readerId":0,"unitId":0,"tag":"DEADBEEF12345678"}
```

---

## 🔐 Тестирование Device Claiming Flow:

### Описание процесса:

Device Claiming - это процедура привязки устройства к аккаунту пользователя через PIN-код.

**Участники:**
- **RP2040** (эмулятор) → инициирует claiming, показывает PIN на экране
- **ESP32** → получает PIN от Backend, отправляет через UART
- **Backend** → генерирует PIN, проверяет привязку

### Шаги теста:

1. **Запусти эмулятор:**
```bash
python3 test_uart_rp2040_emulator.py /dev/cu.usbserial-XXXXXX
```

2. **Выбери опцию 8** (Send ClaimStart)

3. **ESP32 выполнит:**
   - Проверит WiFi подключение
   - Сделает provisioning (если нет token)
   - Зарегистрирует устройство → получит PIN от Backend
   - Отправит ClaimStatus с PIN обратно на RP2040

4. **Эмулятор покажет:**
```
[RECV] XX bytes from ESP
  AA 01 00 71 05 2D [payload] XX XX
```
Это ClaimStatus (0x71) с PIN-кодом внутри payload

5. **В Serial Monitor ESP32 увидишь:**
```
[CLAIM] ClaimStart received from RP2040 (seq=X)
[NET] Starting claim process by user request...
[NET] Registering device for claim...
[NET] Registration PIN: 12345678 (expires in 600 sec)
[CLAIM] PIN sent to RP2040: 12345678
```

6. **Введи PIN в Web UI:** https://portal.idryer.org/devices/claim
   - Логин в аккаунт
   - "Add Device" → введи PIN
   - Нажми "Claim"

7. **ESP32 через 5 секунд получит успешный claim:**
```
[NET] Checking claim status...
[NET] Device claimed! deviceId=550e8400-e29b-41d4-...
[CLAIM] ClaimComplete sent to RP2040
```

8. **Эмулятор получит ClaimComplete (0x72):**
```
[RECV] XX bytes from ESP
  AA 01 00 72 XX 26 [payload with deviceId] XX XX
```

### Что тестируем:

- ✅ RP2040 → ESP: ClaimStart (0x70, пустой payload)
- ✅ ESP → Backend: POST /devices/provision → получение token
- ✅ ESP → Backend: POST /devices/register → получение PIN
- ✅ ESP → RP2040: ClaimStatus (0x71) с PIN
- ✅ ESP → Backend: GET /check-claim/:token (polling каждые 5 сек)
- ✅ ESP → RP2040: ClaimComplete (0x72) с deviceId
- ✅ HTTP 404 обрабатывается как валидный ответ (claimed: false)

### Отладка:

**Если PIN не пришёл:**
- Проверь WiFi подключение: `[NET] Wi-Fi connected, IP: ...`
- Проверь что Backend доступен (не 404/500)

**Если claiming не завершается:**
- Убедись что PIN правильно введён в Web UI
- Проверь что устройство не было привязано ранее
- Очисти NVS: добавь в `setup()`:
```cpp
Preferences prefs;
prefs.begin("idryer", false);
prefs.clear();
prefs.end();
```

---

## 🧪 Проверка retry механизма:

1. Запусти эмулятор
2. Отправь Telemetry (опция 2) - с FLAG_ACK_REQUIRED
3. **НЕ отправляй ничего в ответ** (эмулятор не шлёт ACK)
4. Увидишь в логах ESP32:
```
[UART] Resend seq=X attempt=1 result=XX
[UART] Resend seq=X attempt=2 result=XX
[UART] Resend seq=X attempt=3 result=XX
[ERROR] Timeout...
```

Это докажет что `FLAG_ACK_REQUIRED` → `trackPending=true` работает!

---

## ❌ Отключение DEBUG:

Закомментируй в `platformio.ini`:
```ini
build_flags =
  # -DDEBUG_SERIAL=Serial  ← закомментировать
```

Пересобери:
```bash
pio run -e esp32-c3 -t upload
```

Никаких логов не будет!

---

## 🔧 Поиск серийного порта:

**Linux/macOS:**
```bash
ls /dev/tty*
# Ищи /dev/ttyUSB0 или /dev/ttyACM0
```

**Windows:**
```bash
# COM3, COM4 и т.д. в Device Manager
```

---

## ✅ Что проверено:

### Базовый протокол:
- [x] Парсинг UART фреймов
- [x] CRC проверка
- [x] Вызов обработчиков
- [x] JSON формирование
- [x] DEBUG печать (включается/выключается)
- [x] FLAG_ACK_REQUIRED → retry
- [x] Разные типы сообщений (Hello, Telemetry, Weights, Status, RFID, Heartbeat)

### Device Claiming (новый протокол 0x70-0x72):
- [x] ClaimStart (RP2040 → ESP) - пустой payload
- [x] ClaimStatus (ESP → RP2040) - статус + PIN
- [x] ClaimComplete (ESP → RP2040) - success + deviceId
- [x] HTTP API integration (provision, register, check-claim)
- [x] HTTP 404 обработка как валидный ответ
- [x] WiFi проверка перед claiming
- [x] NVS сохранение (token, deviceId)
- [x] Polling механизм (каждые 5 сек)

### Документация:
- [x] docs/uart-claiming-flow.md - полное описание протокола
- [x] docs/all-in-one-protocol.csv - обновлён с ClaimStart/Status/Complete
- [x] Примеры UART кадров в документации

Готово! Протокол полностью реализован и протестирован.
