# API микроконтроллера iDryer

Документ описывает, как «железный» контроллер сушилки общается с backend-порталом iDryer через REST и WebSocket. Все примеры даны для production (`https://portal.idryer.org`) и локальной разработки (`http://localhost:3000`). Формат обмена — JSON в кодировке UTF-8.

## 1. Архитектура обмена
- **REST (HTTP/HTTPS)** — используется только для первичной регистрации устройства и проверки привязки. Базовый URL: `https://portal.idryer.org/api` (в деве `http://localhost:3000`).
- **WebSocket (Socket.IO v4)** — основной канал телеметрии/команд. Точка подключения: `wss://portal.idryer.org/socket.io` (или `ws://localhost:3000/socket.io`). Используется transport `websocket`, авторизация токеном устройства.
- **Идентификаторы**: каждое устройство хранит постоянный `token` (генерируется на производстве) и получает `deviceId` (UUID из БД) после привязки.

## 2. Процесс регистрации и привязки устройства
1. **Запрос PIN на устройстве**  
   `POST /devices/register` (публично, без JWT). Тело (`RegisterDeviceDto`, `backend/src/devices/dto/register-device.dto.ts`):
   ```json
   {
     "token": "unique-hardware-token",
     "serialNumber": "IDRYER-PRO-0001"
   }
   ```
   Ответ (`devices.service.registerUnclaimedDevice`, `backend/src/devices/devices.service.ts:229`):
   ```json
   {
     "pin": "12345678",
     "expiresAt": "2025-01-18T12:34:56.000Z",
     "remainingSeconds": 599
   }
   ```
   PIN состоит ровно из 8 цифр и действует 10 минут. Повторный запрос до истечения продлевает `remainingSeconds`.

2. **Пользователь вводит PIN**  
   Через портал вызывается `POST /devices/claim` c PIN и читаемым именем (`ClaimDeviceDto`). Устройство этот этап не вызывает, но должно отображать PIN и отслеживать статус.

3. **Периодический опрос статуса**  
   `GET /devices/check-claim/:token` (публично). Пока устройство не привязано — `404` c `{"claimed": false}` (`devices.controller.ts:99`). После привязки ответ:
   ```json
   {
     "claimed": true,
     "device": {
       "id": "2a1d...c3",
       "name": "iDryer Pro в мастерской",
       "serialNumber": "IDRYER-PRO-0001",
       "token": "unique-hardware-token",
       "createdAt": "2025-01-18T12:35:42.000Z"
     }
   }
   ```
   Полученные `device.id` и `token` нужно сохранить во flash/EEPROM; именно они используются при WebSocket-подключении.

## 3. WebSocket API
Все сообщения идут через Socket.IO. Клиент инициирует соединение и сразу отправляет событие `device:connect`.

### 3.1 Подключение (`device:connect`)
- **Направление:** устройство → backend.  
- **Payload** (`DeviceConnectDto`, `backend/src/gateway/dto/device-connect.dto.ts`):
  ```json
  {
    "deviceId": "2a1d...c3",
    "token": "unique-hardware-token"
  }
  ```
- **Ответ:** `{ "status": "connected", "deviceId": "..." }`.  
- При неверном токене backend шлёт `error` с сообщением `Invalid device credentials` (`events.gateway.ts:95-159`).  
- Если устройство уже онлайн, старое соединение получает `device:duplicate` и разрывается (`events.gateway.ts:120-138`).

### 3.2 Телеметрия (`telemetry:data`)
- **Направление:** устройство → backend.  
- **Payload:** `{ "deviceId": "...", "data": TelemetryData }`, где `TelemetryData` строго соответствует `backend/src/gateway/dto/telemetry-data.dto.ts`. Обязательные поля — `temperature` и `humidity`.

| Поле | Тип | Обяз. | Диапазон/формат | Назначение |
| --- | --- | --- | --- | --- |
| `temperature` | number | ✓ | -50…150 °C | Текущая температура внутри сушилки |
| `humidity` | number | ✓ | 0…100 % | Влажность |
| `heaterPower` | number | | 0…100 % | Мощность нагревателя (для UI) |
| `fanStatus` | boolean | | `true` если вентилятор включен |
| `filamentWeight` | number | | 0…10000 г | Текущий вес катушки; сохраняется как масса спула (`telemetry.service.ts:55-90`) |
| `rfidTag` | string | | Любая строка | UID метки; backend нормализует и может автоматически подобрать катушку (`telemetry.service.ts:91-163`) |
| `deviceStatus` | string | | `IDLE` / `DRYING` / `STORAGE` | Состояние контроллера; управляет автосессиями |
| `targetTemperature` | number | | 30…100 °C | Цель, которую выбрал пользователь на устройстве |
| `targetDuration` | number | | 1…1440 мин | Целевая длительность |
| `elapsedTime` | number | | 0…86400 сек | Сколько уже сушим (для UI таймера) |

**Важно про `deviceStatus`:** перечисление `DeviceStatus` определено в `backend/prisma/schema.prisma:587`. Backend автоматически создаёт/закрывает сессии:
- Переход в `DRYING` или `STORAGE` при отсутствии активной сессии → `sessionsService.create(...)` с температурой/длительностью из телеметрии (`events.gateway.ts:210-239`).  
- Переход в `IDLE` при наличии активной сессии завершает её, фиксируя `endTime` (`events.gateway.ts:241-255`).  
Поэтому устройство должно отправлять актуальный статус при каждом значимом изменении.

**Ответ сервера:** `{ "status": "saved" }` или `error` (например, если устройство не найдено). После сохранения backend рассылает обновления всем фронтам (`telemetry:update`, `telemetry:new`).

### 3.3 Команды от портала (`command:execute`)
- **Направление:** backend → устройство (проксируется из `device:command`, `events.gateway.ts:299-333`).
- **Payload:** объект `CommandDto` (`backend/src/gateway/dto/command.dto.ts`):

| `type` | Описание | Payload (если есть) |
| --- | --- | --- |
| `START` | Запуск сушки с параметрами текущей сессии | `{ "temperature": number, "duration": number }` |
| `STOP` | Мгновенная остановка, перевод в `IDLE` | – |
| `PAUSE` / `RESUME` | Управление паузой | – |
| `STORAGE` | Перевод в режим хранения | `{ "temperature": number }` (опционально) |
| `SET_TEMPERATURE` | Изменение целевой температуры | `{ "temperature": number }` |
| `SET_DURATION` | Изменение длительности | `{ "duration": number }` |
| `SET_FAN_SPEED` | Регулировка вентилятора | `{ "percent": number }` |

Устройство должно:
1. Выполнить команду.
2. Подтвердить результат сменой `deviceStatus`/`target*` в следующем сообщении `telemetry:data`.
3. (Опционально) послать собственное событие-ACK, если требуется (протокол не заставляет, но допустимо отправить `telemetry:data` с флагом успеха).

Если устройство оффлайн, фронтенд получит `error: "Device not connected"`; девайсу ничего отправлено не будет.

## 4. Поток событий «из коробки»
1. **Boot**: устройство загружает `token`. Если `deviceId` не известен, идёт в секцию регистрации.  
2. **Ожидание привязки**: цикл `POST /devices/register` → отобразить PIN → каждые 3–5 сек `GET /devices/check-claim/:token` до ответа `claimed:true`.  
3. **Рабочий режим**:
   - Подключиться к Socket.IO и отправить `device:connect`.
   - Раз в 1–5 секунд публиковать `telemetry:data` (минимум при каждом изменении статуса или RFID).
   - Реагировать на входящие `command:execute`.
   - При завершении сушки обязательно установить `deviceStatus = "IDLE"` (иначе сессия не закроется).

## 5. Обработка ошибок и отладка
- **HTTP**:  
  - `400 Bad Request` — токен уже привязан или PIN просрочен (`devices.service.ts:237-335`).  
  - `404 Not Found` — PIN не найден или устройство ещё не привязано (`devices.controller.ts:99-114`).  
  - `409 Conflict` — не используется, но коллизии PIN предотвращаются генератором (`utils/pin.utils.ts`).
- **WebSocket**:  
  - `error` событие с `message` (например, `Connection failed`, `Failed to save telemetry`).  
  - `device:duplicate` — означает, что новое соединение вытеснило старое; устройство может переподключиться.
- **Диагностика RFID/веса**: backend логирует автозамену катушек и ошибки создания «unclaimed» филаментов (`telemetry.service.ts:91-163`). Это помогает понять, почему меняется `currentFilament`.

## 6. Рекомендации для прошивки
1. **Повторное соединение**: по любому сетевому сбою перезапускайте Socket.IO и заново шлите `device:connect`. Backend автоматически пометит устройство offline/online (`events.gateway.ts:71-158`).
2. **Защита PIN**: не храните PIN в постоянной памяти — он одноразовый.
3. **Частота телеметрии**: чтобы не перегружать канал, придерживайтесь 1–2 сообщений в секунду во время активной сушки; при IDLE можно снизить до 1 сообщения в 15–30 секунд, но обязательно отправляйте обновление при каждом изменении RFID, веса или статуса.
4. **Валидация данных**: backend режет любые поля, которых нет в DTO (глобальный `ValidationPipe` в `backend/src/main.ts:39-64`). Следите, чтобы типы и диапазоны совпадали со схемой.
5. **Сохранение параметров**: храните `deviceId`, `token`, последнюю известную `targetTemperature/Duration`, чтобы после перезагрузки вернуться в корректный режим и продолжить отчёт (`elapsedTime` можно восстановить из RTC).

## 4. UART RP2040 ↔ ESP8266
ESP8266 выступает сетевым мостом и связывается с RP2040 только по UART. Канал используется для всех аппаратных команд, поэтому протокол строго регламентирован.

### 4.1 Физический уровень и пины
- **Скорость:** 115200 бод, 8N1, без аппаратного контроля потока.
- **Буферизация:** обе стороны содержат очередь исходящих кадров и ожидают подтверждение с тем же `sequence`.
- **GPIO-сопоставление:**

| Сигнал | RP2040 | ESP8266 | Комментарий |
| --- | --- | --- | --- |
| UART TX → RX | GPIO0 (UART0 TX) | GPIO3 (RX0) | RP2040 → ESP (телеметрия, Ack) |
| UART RX ← TX | GPIO1 (UART0 RX) | GPIO1 (TX0) | ESP → RP2040 (команды, конфигурация) |
| GND | Общий | Общий | Требуется общее заземление |
| RESET ESP | GPIO2 | RST | Опционально для удалённого перезапуска |

- Питание UART согласуется уровнем 3V3; дополнительные делители/буферы не нужны.

### 4.2 Формат кадра
Фрейм описан в `include/uart_protocol.h`:
```
0xAA | версия | флаги | MessageKind | sequence | длина | payload | CRC16
```
- CRC16-CCITT (0x1021, init 0xFFFF), порядок байт: младший, затем старший.
- `flags`: бит0 — требуется ACK, бит1 — это ACK, бит2 — ошибка. Остальные биты зарезервированы.
- Максимальная длина полезной нагрузки — 48 байт. Повторная отправка выполняется максимум 3 раза.

### 4.3 Словарь сообщений и структуры
Ключевые `MessageKind`:
- `Hello` / `HelloAck` — обмен ролями (`Role::Rp2040Controller`, `Role::EspBridge`), версиями прошивок и состоянием сети.
- `Telemetry` → `TelemetryAck` — RP2040 передаёт `TelemetryPayload`: температура (×10 °C), влажность, мощность нагревателя, состояние (`DryerState`), `FaultCode`, вес катушки, `remainingMinutes`, `jobId`, `uptimeSeconds`.
- `Command` → `CommandAck` — ESP посылает `CommandPayload` (`CommandCode::StartDry`, `Stop`, `Pause`, `Resume`, `PushConfig`, `Identify`, `ResetFault`, `WifiStatus`, `RequestTelemetry`). При ошибке RP2040 возвращает `AckPayload` с `ErrorCode`.
- `ConfigPush`/`ConfigAck` — новые целевые параметры (`ConfigPayload`: цель°C×10, допустимая влажность, длительность, ШИМ вентилятора).
- `Heartbeat` — обе стороны каждые 5 секунд передают `HeartbeatPayload` (uptime, RSSI для ESP, температура MCU для RP2040).
- `Error` — несёт `ErrorPayload` (`ErrorCode::CrcMismatch`, `InvalidPayload`, `Timeout`, `SequenceMismatch`, `Busy`). Используется при исчерпании ретраев или некорректном кадре.
- `Log` — текстовые диагностические сообщения (до 32 байт ASCII); флаг ACK не используется.

### 4.4 Последовательности и тайминги
**Старт устройства**
1. RP2040 ждёт стабильного питания ESP и шлёт `Hello` с версией прошивки и маской возможностей.
2. ESP отвечает `HelloAck` с состоянием сети (`Wi-Fi connecting/connected`, RSSI, сохранённые `deviceId`/`token`).
3. После обмена RP2040 переходит в штатный режим и начинает отправлять `Telemetry` с периодом:
   - 1 с при активной сушке.
   - 15 с в режиме IDLE (или любое событие: изменение RFID, веса, состояния).
4. ESP подтверждает каждый `Telemetry` (`FLAG_ACK_REQUIRED` обязательно). При отсутствии Ack за 700 мс RP2040 повторяет кадр, максимум три попытки; после трёх неудач выставляет `FaultCode::UartTimeout` и отправляет `Error`.

**Получение команды с сервера**
1. Backend шлёт команду через WebSocket → ESP формирует `Command` с новым `sequence`, устанавливает флаг ACK.
2. RP2040 проверяет параметры (`CommandPayload.arg0/arg1`, `targetState`). Если команда допустима — выполняет и отвечает `CommandAck` (`ErrorCode::None`). При отклонении указывает причину (`CommandRejected`, `Busy` и т. п.).
3. Если ESP не получил Ack за 700 мс, повторяет кадр (до 3 раз) и при провале шлёт на сервер событие `DEVICE_UART_FAULT`, переводя устройство в offline.

**Heartbeat и восстановление**
- Каждая сторона отправляет `Heartbeat` каждые 5 с. Если нет кадров >20 с (`LINK_LOSS_TIMEOUT_MS`), ESP инициирует перезапуск UART-драйвера и уведомляет сервер. RP2040 при отсутствии любых сообщений более 20 с также генерирует `FaultCode::UartTimeout`.
- После перезапуска ESP снова шлёт `Hello`, RP2040 сбрасывает счётчики и отвечает `HelloAck`.

**Логи и диагностика**
- RP2040 может отправлять `Log` при смене состояния, аварии датчика, запуске программы. ESP передаёт текст в backend (WebSocket событие `device:log`).
- ESP отправляет `Error` при внутренних проблемах (например, не удалось сериализовать WebSocket-команду). Поле `detail` кодирует дополнительные данные (ожидаемая длина, статус HTTP и т. д.).

### 4.5 Требования к реализациям
- Любой кадр с неизвестным `MessageKind` игнорируется, но сторона отправляет `Error` (`UnknownMessage`).
- Номера последовательностей инкрементируются по модулю 256 независимо в каждую сторону.
- RP2040 сбрасывает глобальные состояния (PID нагревателя, вентилятор) только после подтверждённой команды `Stop` или `ResetFault`.
- ESP хранит зеркалирование состояния (`DryerState`) для отображения в портале и переподключения WebSocket.
- Таймеры должны сбрасываться на любой входящий кадр, включая `Log`. Если `Heartbeat` не получен вовремя, сторожа переводят интерфейс в Fault.

### 4.6 Справочник payload’ов
**TelemetryPayload**

| Поле | Тип | Масштаб | Диапазон | Комментарий |
| --- | --- | --- | --- | --- |
| `temperatureC10` | int16 | градус ×10 | -500…1500 | Температура камеры |
| `humidityPct` | uint8 | % | 0…100 | Влажность воздуха |
| `heaterPowerPct` | uint8 | % | 0…100 | Текущий ШИМ нагревателя |
| `fanOn` | uint8 | bool | 0/1 | Состояние вентилятора |
| `filamentWeightGrams` | uint16 | г | 0…10000 | Вес катушки |
| `state` | DryerState | — | см. enum | IDLE, PREHEAT, … |
| `fault` | FaultCode | — | см. enum | Последняя авария |
| `remainingMinutes` | uint16 | мин | 0…65535 | Остаток программы |
| `jobId` | uint32 | — | — | Идентификатор задания из backend |
| `uptimeSeconds` | uint32 | сек | 0…4,2e9 | Аптайм RP2040 |

**CommandPayload**

| Код | Назначение | Аргументы |
| --- | --- | --- |
| `StartDry` | Запустить сушку | `arg0` = цель°C×10, `arg1` = длительность в мин |
| `Stop` | Остановить цикл | — |
| `Pause`/`Resume` | Пауза и возобновление | — |
| `PushConfig` | Обновить базовые настройки | payload `ConfigPayload` |
| `Identify` | Подсветка/звуковой сигнал | `arg0` = длительность мс |
| `ResetFault` | Снять аварийное состояние | — |
| `RequestTelemetry` | Требует немедленного кадра телеметрии | — |
| `WifiStatus` | Вернуть RSSI, SSID, IP | — |

RP2040 обязан отвечать `CommandAck` c `AckPayload` (`ackSequence`, `ErrorCode`). Допустимые ошибки: `Busy`, `InvalidPayload`, `CommandRejected`.

**HeartbeatPayload**

| Поле | Отправитель | Комментарий |
| --- | --- | --- |
| `uptimeSeconds` | обе стороны | Аптайм в секундах |
| `wifiRssiDbm` | ESP | RSSI Wi-Fi в дБм; RP2040 использует поле для температуры MCU |
| `errorsSinceBoot` | обе стороны | Счётчик критических ошибок |

**ErrorPayload**

| Код | Описание | Detail |
| --- | --- | --- |
| `CrcMismatch` | Контрольная сумма не совпала | ожидаемая/фактическая длина |
| `UnknownMessage` | `MessageKind` не поддерживается | значение поля |
| `InvalidPayload` | Размер/данные не соответствуют структуре | смещение/код проверки |
| `Busy` | Система не готова выполнить команду | текущий `DryerState` |
| `Timeout` | Не получили ACK/ответ | sequence |
| `SequenceMismatch` | ACK с другим номером | ожидаемый/полученный sequence |

ESP также транслирует эти коды в backend (event `device:uartError`).

### 4.7 Соответствие состояний RP2040 и портала
- `DryerState::Idle` ↔ Web UI статус “Готов”.
- `DryerState::Preheat` ↔ “Нагрев”.
- `DryerState::Drying` ↔ “Сушка активна”.
- `DryerState::Cooling` ↔ “Охлаждение”.
- `DryerState::Fault` ↔ “Ошибка” с отображением `FaultCode` (`SensorFailure`, `HeaterOverrun`, `UartTimeout`, `CommandRejected`).
- `DryerState::Service` ↔ “Сервисный режим” — backend не посылает команды, кроме `ResetFault`.

Backend хранит `state` зеркально и при подключении WebSocket передаёт его в UI. ESP обязан инициировать `Command`/`ConfigPush` только когда RP2040 подтвердил `HelloAck`.

Эта спецификация охватывает полный цикл взаимодействия микроконтроллера с iDryer Portal и UART-каналом к RP2040. При расширении набора датчиков добавляйте поля в `TelemetryDataDto` и согласуйте их с backend командой (DTO настроены в «whitelist»-режиме, поэтому незадекларированные поля игнорируются).
