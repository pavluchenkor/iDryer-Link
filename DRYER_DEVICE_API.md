# iDryer Link Device API (текущая реализация)

Документ описывает **фактический** сетевой и протокольный поток для прошивки Link (ESP32-C3) в этом репозитории.

Источник правды: текущий код `src/` и `lib/idryer-protocol/src/`.

## 1. Роли компонентов

- **ESP32 Link**: сетевой мост между MCU (RP2040) и iDryer Portal.
- **RP2040 MCU**: контроллер сушилки, UI и локальная логика режимов.
- **Portal Backend**: REST для provision/claim, MQTT брокер для realtime команд и телеметрии.
- **Portal Frontend**: UI для пользователя (ввод PIN, управление устройством).

## 2. Протоколы в текущей прошивке

- **UART (RP2040 ↔ ESP32)**: телеметрия, команды, claim-кадры `0x70..0x72`.
- **REST (ESP32 → Portal API)**: `/devices/provision`, `/devices/register`, `/devices/check-claim/{token}`.
- **MQTT (ESP32 ↔ Broker)**: основной рабочий канал после claim.
- **USB Serial (Web Installer ↔ ESP32)**: команда `START_CLAIM`, события `CLAIM_*`.

Важно: в этой прошивке рабочий realtime-канал устройства с backend — **MQTT**, не Socket.IO/WebSocket.

## 3. Конфигурация окружений

### Production

- API base: `https://portal.idryer.org/api`
- MQTT broker: `mqtt.idryer.org:8883`
- MQTT TLS: включен

### Staging

- API base: `https://staging.idryer.org/api`
- MQTT broker: `82.146.63.133:1884`
- MQTT TLS: выключен

## 4. Идентификаторы устройства

- `serialNumber`: генерируется на ESP32 из MAC как `DEVICE_<MAC12HEX>`.
  - Пример: `DEVICE_A1B2C3D4E5F6`
- `token`: выдается backend в `POST /devices/provision` (`deviceToken`).
- `deviceId`: появляется только после успешного claim (или recovery-ответа backend).

Хранение на ESP32 (NVS):

- key `serial`
- key `token`
- key `deviceId`

## 5. Cloud state machine

Состояния:

1. `WifiConnecting`
2. `Provisioning`
3. `AwaitingClaim` (только когда claim запущен)
4. `Ready`
5. `MqttConnecting`
6. `Online`

Ключевая логика:

- Если есть `token`, но нет `deviceId`, устройство **не** идет в MQTT автоматически.
- Для старта claim нужен явный триггер (`START_CLAIM` по USB или `ClaimStart` по UART).
- В MQTT устройство переходит только при наличии **и `token`, и `deviceId`**.

## 6. Claiming: пошаговый поток

### 6.1 Точка входа

Claim может быть запущен двумя путями:

- USB Serial: строка `START_CLAIM`
- UART от RP2040: кадр `ClaimStart` (`MessageKind=0x70`)

### 6.2 Provision (если нет token)

ESP32 вызывает:

`POST /devices/provision`

Request:

```json
{
  "serialNumber": "DEVICE_A1B2C3D4E5F6"
}
```

Ожидаемые поля ответа, которые использует прошивка:

```json
{
  "deviceToken": "...",
  "isNew": true,
  "isClaimed": false,
  "deviceId": "..."
}
```

- `deviceToken` сохраняется как `token`.
- Если `isClaimed=true` и есть `deviceId`, claim считается восстановленным (recovery) без PIN.

### 6.3 Register (получение PIN)

ESP32 вызывает:

`POST /devices/register`

Request:

```json
{
  "token": "<deviceToken>",
  "serialNumber": "DEVICE_A1B2C3D4E5F6"
}
```

Вариант A, обычный ответ:

```json
{
  "pin": "12345678",
  "remainingSeconds": 599
}
```

Вариант B, recovery (уже привязано):

```json
{
  "alreadyClaimed": true,
  "deviceId": "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
}
```

### 6.4 Доставка PIN

После получения PIN ESP32 отправляет его:

- в RP2040 по UART: `ClaimStatus{ status=WaitingClaim, pin, remainingSeconds }`
- в USB Serial: `CLAIM_PIN:<pin>:<remainingSeconds>`

Дополнительно при старте через USB:

- `CLAIM_STARTED:OK` при успешном запуске процесса
- `CLAIM_STARTED:ERROR` при ошибке старта

### 6.5 Polling статуса claim

ESP32 опрашивает backend:

`GET /devices/check-claim/{token}`

Интервал polling: `IDRYER_CLAIM_POLL_INTERVAL_MS = 5000` (5 секунд).

Поведение:

- `404` для `check-claim` трактуется как валидный ответ "еще не привязано".
- При `claimed=true` считывается `deviceId`, сохраняется в NVS.

### 6.6 Завершение claim

После получения `deviceId`:

- ESP32 отправляет в RP2040: `ClaimComplete{ success=1, deviceId }`
- Cloud state: `AwaitingClaim -> Ready -> MqttConnecting -> Online`

## 7. MQTT после claim

MQTT подключение выполняется только когда есть:

- `serialNumber`
- `token`
- `deviceId`

Параметры CONNECT:

- `clientId = serialNumber`
- `username = serialNumber`
- `password = token`

Подписка:

- `idryer/{serialNumber}/commands/#`

Публикации устройства:

- `info`
- `telemetry`
- `status`
- `weights`
- `rfid`
- `events`
- `config`
- `config/delta`

## 8. UART claim-кадры

- `ClaimStart (0x70)`: RP2040 → ESP32, пустой payload
- `ClaimStatus (0x71)`: ESP32 → RP2040
  - `status`: `Idle | Provisioning | WaitingClaim | Claimed | Error`
  - `pin[9]`
  - `expiresAt`
  - `remainingSeconds`
- `ClaimComplete (0x72)`: ESP32 → RP2040
  - `success`
  - `deviceId[37]`

## 9. Ошибки и recovery

- Нет Wi-Fi: `requestClaim()` возвращает ошибку, claim не стартует.
- Ошибка `provision/register`: claim не стартует или остается в ожидании, в зависимости от шага.
- `register` вернул `alreadyClaimed=true`: сразу сохраняется `deviceId`, переход к `Ready`.
- `provision` вернул `isClaimed=true` + `deviceId`: тоже recovery без PIN.

## 10. Ограничения текущей реализации

- `ClaimStatusPayload.expiresAt` пока отправляется как `0` (TODO в коде).
- Ветвление `CLAIM_ALREADY` в USB-потоке требует доработки: при уже claimed `requestClaim()` сейчас возвращает `false`, и наружу уходит `CLAIM_STARTED:ERROR`.

## 11. Краткий чек-лист интеграции

1. Настроить Wi-Fi.
2. Запустить claim (`START_CLAIM` или UART `ClaimStart`).
3. Получить PIN и показать пользователю.
4. Пользователь вводит PIN в портале.
5. Дождаться `ClaimComplete` и сохранения `deviceId`.
6. Проверить переход в `Online` и подписку на `commands/#`.

---

Последнее обновление: **2026-03-04**.
