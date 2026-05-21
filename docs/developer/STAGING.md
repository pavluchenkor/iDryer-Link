# Тестирование на Staging

Staging -- нестабильная среда для тестирования новых фич портала и прошивки.
Backend, frontend, MQTT-брокер и база данных на staging **отдельные** от production.

**Staging может быть недоступен, перезапущен или содержать ошибки в любой момент.**
Данные на staging могут быть сброшены без предупреждения.

## Получение доступа

Staging -- закрытая среда. Для доступа нужны:
- **basic auth** (пароль от staging фронтенда)
- **staging-аккаунт** (логин/пароль для API и портала)

Напиши в Telegram [@pavluchenkor](https://t.me/pavluchenkor) -- получишь credentials.
Регистрация через staging UI не работает (отправка email не настроена).

## Отличия от production

| | Production | Staging |
|---|-----------|---------|
| Портал | portal.idryer.org | staging.idryer.org (basic auth: admin) |
| API | portal.idryer.org/api | staging.idryer.org/api (без basic auth) |
| MQTT | mqtt.idryer.org:8883 (TLS) | staging.idryer.org:1884 (без TLS) |
| MQTT WS | wss://mqtt.idryer.org:8084/mqtt | wss://staging.idryer.org:8084/mqtt |
| База | dryer_production | idryer_staging |
| Debug | CORE_DEBUG_LEVEL=0 | CORE_DEBUG_LEVEL=3 |

## Быстрый старт

### 1. Прошить stage-окружение

```bash
pio run -e esp32c3-super-mini-stage -t upload
```

Доступные stage-окружения:
- `esp32c3-stage`
- `esp32c3-super-mini-stage`
- `xiao-esp32s3-stage`
- `waveshare-esp32s3-zero-stage`

### 2. Auto-claim (автоматически)

После upload скрипт `stage_auto_claim.py` запустится сам:

```
==================================================
  STAGE AUTO-CLAIM: esp32c3-super-mini-stage
  Port: /dev/cu.usbmodem114301 @ 115200
  Waiting for CLAIM_PIN from firmware...
==================================================

  [SERIAL] [BOOT] Logs enabled after WiFi config
  [SERIAL] [CLOUD] Provisioning device...
  [SERIAL] [CLOUD] Device NOT claimed.

  [AUTO-CLAIM] Sending START_CLAIM...
  [SERIAL] CLAIM_PIN:12345678:600

  [AUTO-CLAIM] Logging into staging backend...
  [AUTO-CLAIM] Login OK
  [AUTO-CLAIM] Claiming device with PIN 12345678...

  Device claimed successfully!
  deviceId: abc123
```

Скрипт:
1. Ждёт WiFi + provision от прошивки
2. Отправляет `START_CLAIM` по serial
3. Ловит PIN
4. Логинится на staging backend и клеймит устройство

Если auto-claim пропустил (Ctrl+C) или не сработал -- см. ручной клейм ниже.

### 3. Мониторинг

```bash
pio run -e esp32c3-super-mini-stage -t monitor
```

Ожидаемый вывод после клейма:
```
[CLOUD] Device claimed! deviceId=xxx
[CLOUD] State: Ready -> MqttConnecting
[MQTT] Connecting as DEVICE_AABBCCDD...
[MQTT] Connected!
[MQTT] Subscribed: idryer/DEVICE_AABBCCDD/commands/# (QoS 1)
```

## Ручной клейм (без auto-claim)

Если auto-claim не подходит:

1. Прошить: `pio run -e esp32c3-super-mini-stage -t upload`
2. Открыть monitor: `pio run -e esp32c3-super-mini-stage -t monitor`
3. В monitor набрать: `START_CLAIM` + Enter
4. Увидеть в логе: `CLAIM_PIN:12345678:600`
5. Открыть https://staging.idryer.org (ввести basic auth credentials)
6. Войти staging-аккаунтом (credentials у @pavluchenkor)
7. Devices -> Claim -> ввести PIN `12345678`

## Настройка WiFi

WiFi нужен для работы прошивки (provision, MQTT). Credentials сохраняются в NVS.

### secrets.h (рекомендуется для staging)

Создай `include/secrets.h` (файл в `.gitignore`, не попадёт в репо):

```bash
cat > include/secrets.h << 'EOF'
#pragma once

#define IDRYER_WIFI_SSID "ваш-wifi-ssid"
#define IDRYER_WIFI_PASSWORD "ваш-wifi-пароль"
EOF
```

При первом запуске (NVS пуст) прошивка подхватит WiFi из `secrets.h`,
сохранит в NVS и подключится -- **без Improv, без браузера**.
Логи на Serial доступны сразу, auto-claim работает.

### Improv Wi-Fi (альтернатива)

Если `secrets.h` не создан и NVS пуст, прошивка запустит Improv на Serial.
Пока Improv активен, логи **не работают** -- Serial занят протоколом.

Настроить WiFi через Improv можно на https://www.improv-wifi.com/
или через https://install.idryer.org (Web Serial).

### Сброс WiFi

```bash
pio run -e esp32c3-super-mini-stage -t erase
pio run -e esp32c3-super-mini-stage -t upload
```

## Конфигурация auto-claim

Переменные окружения (все опциональные):

```bash
export STAGING_EMAIL="your-staging-email@example.com"
export STAGING_PASSWORD="your-staging-password"
export STAGING_API_URL="https://staging.idryer.org/api"
```

Credentials для staging-аккаунта запросите у [@pavluchenkor](https://t.me/pavluchenkor).

## Устранение проблем

### WiFi не подключается
- Сотри NVS: `pio run -e ... -t erase` и настрой WiFi заново через Improv
- Проверь что SSID и пароль правильные (Improv не показывает ошибку пароля)

### Provision failed (HTTP -1 или 401)
- Staging backend может быть перезапущен -- подожди пару минут
- Проверь что staging запущен: `curl https://staging.idryer.org/api/health`

### MQTT не подключается
- Убедись что EMQX staging запущен (спроси у @pavluchenkor)
- В логе `[MQTT] Connection failed: -2` = брокер недоступен
- В логе `[MQTT] Connection failed: 5` = auth rejected, попробуй стереть NVS и заново

### Auto-claim: "No PIN received"
- RP2040 не подключен -- serial number приходит от MCU по UART
- Используй эмулятор: `python3 tools/emulate_controller.py --port /dev/ttyUSB0`

### Staging недоступен
Staging пересобирается автоматически при каждом push в master.
Во время сборки (~30 мин) сервисы могут быть недоступны.
Статус: спроси в Telegram @pavluchenkor.
