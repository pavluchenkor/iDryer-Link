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

## 4. UART протокол (RP2040 ↔ ESP32)

ESP32 выступает сетевым мостом и связывается с RP2040 по UART (115200 бод, 8N1). Канал используется для передачи телеметрии, команд и конфигурации.

**📚 Полная спецификация UART протокола:**

Детальное описание протокола (формат кадров, структуры данных, фрагментация, CRC, примеры) вынесено в отдельную библиотеку:

👉 **[idryer-protocol/docs/UART_PROTOCOL.md](https://github.com/pavluchenkor/idryer-protocol/blob/master/docs/UART_PROTOCOL.md)**

**Что включает спецификация:**
- Физический уровень и пины
- Побайтовый разбор кадра
- Типы сообщений (MessageKind)
- Все структуры payload (Telemetry, Command, Log, Heartbeat)
- Фрагментация больших данных (JSON конфига)
- Таймауты и ретраи
- Обработка ошибок
- CRC16 расчёт
- Hex примеры всех типов кадров

**Используемая библиотека:**
```ini
[env:esp32]
lib_deps =
    https://github.com/pavluchenkor/idryer-protocol.git
```

Эта спецификация охватывает полный цикл взаимодействия микроконтроллера с iDryer Portal через WebSocket API. При расширении набора датчиков добавляйте поля в `TelemetryDataDto` и согласуйте их с backend командой (DTO настроены в «whitelist»-режиме, поэтому незадекларированные поля игнорируются).

---

## 📚 Связанные документы

### iDryer Protocol (общая библиотека)

**GitHub:** https://github.com/pavluchenkor/idryer-protocol

| Документ | Описание |
|----------|----------|
| [UART_PROTOCOL.md](https://github.com/pavluchenkor/idryer-protocol/blob/master/docs/UART_PROTOCOL.md) | Детальная спецификация UART (RP2040 ↔ ESP32) |
| [SYSTEM_MAP.md](https://github.com/pavluchenkor/idryer-protocol/blob/master/docs/SYSTEM_MAP.md) | Архитектура системы, компоненты, источники правды |
| [mqtt-api-kit/](https://github.com/pavluchenkor/idryer-protocol/tree/master/docs/mqtt-api-kit) | MQTT API документация |
| [error_defs.h](https://github.com/pavluchenkor/idryer-protocol/blob/master/docs/examples/error_defs.h) | Пример системы ошибок (для справки) |

### Backend (iDryerPortal)

| Документ | Описание |
|----------|----------|
| `backend/src/gateway/dto/` | DTO для WebSocket сообщений |
| `backend/src/devices/` | API регистрации устройств |
| `backend/prisma/schema.prisma` | Схема БД (Device, Session, Filament) |

### RP2040 (контроллер)

| Документ | Описание |
|----------|----------|
| `src/menu/menu_v2.yaml` | Структура меню устройства |
| `src/error/error_defs.h` | Система ошибок (X-macros) |

---

**Последнее обновление:** 2025-12-23
