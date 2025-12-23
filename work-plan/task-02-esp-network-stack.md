# Задача 02 — Сетевой стек ESP32-C3/S3 и MQTT клиент

> **Платформа**: ESP32-C3, ESP32-S3
>
> **Зависимости**:
> - [task-00-protocol-library.md](./task-00-protocol-library.md) — типы и топики из `idryer-protocol`
> - [task-01-uart-architecture.md](./task-01-uart-architecture.md) — UART протокол
>
> **Ссылки на API:**
> - [First Touch (Provisioning)](../docs/mqtt-api-kit/01-getting-started/first-touch.md)
> - [ESP32 Connection](../docs/mqtt-api-kit/04-esp32/connection.md)
> - [Topics](../docs/mqtt-api-kit/02-api-reference/topics.md)
> - [TLS Security](../docs/mqtt-api-kit/06-security/tls.md)

## Цель
Реализовать прошивку ESP32-C3/S3 как MQTT-мост между облаком и основным МК (RP2040). ESP32 поднимает Wi-Fi, проходит процедуру provisioning (получение токена), подключается к MQTT брокеру (EMQX) и передаёт данные по UART. ESP32 не управляет железом напрямую — только сетевое взаимодействие.

## Основные шаги

### 1. Модули и конфигурация
- Подготовить модули Wi-Fi/HTTPS/MQTT в `src/` или `lib/`
- Вынести конфигурацию (SSID, пароль) в `src/secrets.h` или Captive Portal
- Хранить `serialNumber`, `token`, `deviceId` в NVS (зашифрованный раздел)

### 2. Provisioning Flow ([First Touch](../docs/mqtt-api-kit/01-getting-started/first-touch.md))
Реализовать 3-этапную регистрацию:

**Этап 1: Provision** — получение токена
```
POST https://portal.idryer.org/api/devices/provision
Body: { "serialNumber": "DEVICE_aabbcc_1234567" }
Response: { "deviceToken": "eyJ...", "isNew": true, "isClaimed": false }
```
- Сохранить `deviceToken` в NVS
- Если `isClaimed: true` → сразу к MQTT

**Этап 2: Register** — получение PIN для привязки
```
POST https://portal.idryer.org/api/devices/register
Body: { "token": "eyJ..." }
Response: { "pin": "12345678", "expiresAt": "...", "remainingSeconds": 600 }
```
- Показать PIN на экране / отправить в UART для отображения на RP2040
- TTL: 10 минут

**Этап 3: Check Claim** — polling статуса привязки
```
GET https://portal.idryer.org/api/devices/check-claim/{token}
Response (ожидание): { "claimed": false }
Response (успех): { "claimed": true, "deviceId": "uuid" }
```
- Polling каждые 3-5 секунд
- При `claimed: true` → сохранить `deviceId` → подключиться к MQTT

### 3. MQTT подключение ([Connection](../docs/mqtt-api-kit/04-esp32/connection.md))
```cpp
// TLS на порту 8883
esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = "mqtts://mqtt.idryer.org:8883",
    .credentials.username = serialNumber,      // "DEVICE_aabbcc_1234567"
    .credentials.authentication.password = deviceToken,  // JWT
    .session.keepalive = 60,                   // ОБЯЗАТЕЛЬНО!
};
```

**Подписка на команды:**
```
idryer/{serialNumber}/commands/#
```

**Публикация данных** (топики из [topics.md](../docs/mqtt-api-kit/02-api-reference/topics.md)):
| Топик | QoS | Retained | Частота |
|-------|-----|----------|---------|
| `info` | 1 | Yes | При включении |
| `telemetry` | 0 | No | 5 сек |
| `weights` | 1 | No | 10 сек |
| `status` | 1 | Yes | При изменении |
| `events` | 1 | No | При событии |
| `rfid` | 1 | Yes | При изменении |
| `config` | 1 | No | По запросу |

### 4. Обработка команд
При получении MQTT сообщения из `commands/*`:
- Парсинг JSON payload
- Конвертация в UART команду для RP2040
- Ожидание ACK от RP2040
- Публикация события `COMMAND_ACK` в `events`

### 5. TLS Security ([TLS](../docs/mqtt-api-kit/06-security/tls.md))
- Root CA Let's Encrypt вшит в прошивку
- HTTPS для provisioning API
- MQTTS (порт 8883) для MQTT
- NVS с Flash Encryption для хранения токена

### 6. Автопереподключение
- При потере Wi-Fi → reconnect
- При потере MQTT → reconnect с exponential backoff
- Уведомление RP2040 о статусе сети через UART (`WifiStatus`)

### 7. Логирование
- Отладочные макросы (`DEBUG_SERIAL`)
- Логирование состояния: Wi-Fi, provisioning, MQTT, UART

## Результат
- Стабильная связь ESP32 ↔ MQTT брокер с восстановлением после обрывов
- Полная реализация [First Touch](../docs/mqtt-api-kit/01-getting-started/first-touch.md) протокола
- Документированные точки интеграции с UART (задача 03)
- Код компилируется `pio run`, соответствует [platformio.ini](../docs/mqtt-api-kit/04-esp32/connection.md#platformioini)

## Критерии приёмки
- Успешный provisioning: provision → register → claim → MQTT connect
- MQTT соединение держится >30 мин, корректно пересоединяется
- Все топики из [topics.md](../docs/mqtt-api-kit/02-api-reference/topics.md) публикуются с правильными QoS/retained
- Команды из [commands.md](../docs/mqtt-api-kit/02-api-reference/commands.md) обрабатываются
- RP2040 получает статусы сети через UART согласно задаче 01
- TLS работает корректно (проверка сертификата)
