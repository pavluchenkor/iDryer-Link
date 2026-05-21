# fake_bambu — заглушка Bambu Lab MQTT

Локальный TLS-MQTT broker + publisher, эмулирующий принтер Bambu Lab
для проверки iHeater Link Bambu integration без реального принтера.

## Что это

Bambu Lab принтер изнутри запускает MQTT-broker на TLS-порту 8883
с user=`bblp` / pass=`<LAN access code>`. iHeater Link подключается
к нему как клиент, подписывается на `device/{serial}/report` и читает
`chamber_target` / `ams.tray[].tray_type`.

Эта связка `mosquitto + publisher.py` слушает ESP-коннекту и шлёт
в `device/{serial}/report` лестницу `chamber_target`:
`70 → 65 → 60 → 55 → 50 → 55 → 60 → 65 → ...` по 30 сек.

## Зависимости

```bash
brew install mosquitto
pip3 install --user paho-mqtt
```

## Запуск

В трёх терминалах:

```bash
# 1. Cert (один раз).
cd tools/fake_bambu
./gen_cert.sh

# 2. MQTT-broker (foreground).
mosquitto -c mosquitto.conf

# 3. Publisher (foreground).
python3 publisher.py
```

Параметры publisher.py настраиваются env-переменными:

| Переменная           | Дефолт           | Описание                                    |
| -------------------- | ---------------- | ------------------------------------------- |
| `FAKE_BAMBU_HOST`    | `127.0.0.1`      | broker host для самого publisher            |
| `FAKE_BAMBU_PORT`    | `8883`           | broker port                                 |
| `FAKE_BAMBU_SERIAL`  | `FAKE_BAMBU_001` | serial — должен совпасть с настройкой ESP   |
| `FAKE_BAMBU_LAN`     | `12345678`       | LAN access code                             |
| `FAKE_BAMBU_TRAY`    | `PETG`           | тип филамента в vt_tray (PLA / PETG / ABS)  |
| `FAKE_BAMBU_CERT`    | `./cert.pem`     | путь к CA cert                              |

## Настройка интеграции в портале

iHeater Link → Settings → Bambu:

| Поле              | Значение            |
| ----------------- | ------------------- |
| ip                | `<IP вашего мака>`  |
| serial            | `FAKE_BAMBU_001`    |
| lan access code   | `12345678`          |
| enabled           | on                  |

IP мака:

```bash
ipconfig getifaddr en0
```

## Ожидаемый Serial-вывод на ESP

```
[INFO ] BAMBU: configure: ip=192.168.0.171 serial=FAKE_BAMBU_001
[INFO ] BAMBU: connect attempt: 192.168.0.171:8883 (user=bblp)
[INFO ] BAMBU: connected
[INFO ] BAMBU: subscribed to device/FAKE_BAMBU_001/report
[HEATER] Bambu: chamber_target=70 → output=ON
[RMT→status] mode=Drying target=70.0
```

## Диагностика

- Подключение не происходит → mosquitto падает в логе (cert не нашёл, порт занят).
- Publisher теряет соединение → проверьте, что `tls_insecure_set(True)` в скрипте.
- ESP-side TLS handshake fails → ESP делает `setInsecure()`, но
  PubSubClient бывает капризен на mac-cert. В этом случае генерируйте
  cert с `addext "subjectAltName=IP:<mac IP>"` (gen_cert.sh уже включает
  `192.168.0.171` — поправьте под ваш IP).
- Хотите проверить tray_type lookup в menu (mat_petg/pla/abs) — задайте
  `FAKE_BAMBU_TRAY=PLA python3 publisher.py` и убедитесь что в меню
  устройства `mat_pla` стоит ненулевое значение.
