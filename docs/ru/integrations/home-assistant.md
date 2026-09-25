# Home Assistant

Сушилка с модулем Link публикует себя в Home Assistant через **MQTT Discovery**: HA сам создаёт сущности — показания камеры, поля запуска сушки и хранения, кнопки. Портал для этого не нужен, всё идёт через ваш MQTT-брокер.

Ниже — включение интеграции, проверка и готовая раскладка карточки, чтобы прибор выглядел как на картинке, а не списком сущностей.

![Карточки приборов в Home Assistant](../../img/ha-card.png)
*Обе камеры сушилки на панели Home Assistant: показания, запуск сушки и хранения, обслуживание.*

!!! note
    Устройство **не появится** в `Settings → Devices & services → Discovered`: это MQTT Discovery, а не UPnP/zeroconf. Интеграция **MQTT** в Home Assistant должна быть добавлена заранее.

## Что нужно

1. MQTT-брокер: дополнение **Mosquitto broker** в Home Assistant или любой брокер в вашей сети.
2. В Home Assistant добавлена интеграция **MQTT**, указывающая на этот брокер.
3. Сушилка с модулем Link в сети и `Online` на портале.

## Шаг 1. Включить интеграцию на приборе

Откройте устройство на [portal.idryer.org](https://portal.idryer.org/) и найдите блок **Интеграции** → **Home Assistant**.

| Поле | Что вписать |
|---|---|
| Host | адрес брокера в вашей сети, например `192.168.1.27` |
| Port | порт брокера, обычно `1883` |
| Username / Password | учётные данные брокера, если он их требует |
| Discovery prefix | `homeassistant`, если не меняли его в настройках HA |
| Включено | галочка — иначе прибор к брокеру не подключится |

Настройки уходят прямо на прибор по локальной сети — портал их не хранит.

![Окно Home Assistant в блоке «Интеграции» на портале](../../img/ha-portal-integration.png)
*Адрес брокера, порт и признак «Включено» — всё, что нужно прибору.*

## Шаг 2. Найти устройство в Home Assistant

`Settings` → `Devices & services` → карточка **MQTT** → в разделе **Services** разверните узел брокера. Приборы iDryer видны под серийными номерами вида `DEVICE_*`.

![Приборы iDryer на странице интеграции MQTT](../../img/ha-mqtt-devices.png)
*Устройства под узлом брокера; у сушилки видно число сущностей.*

Откройте устройство: HA уже показывает показания и элементы управления. Проверьте, что значения живые — они обновляются вместе с телеметрией прибора.

## Шаг 3. Собрать карточку

HA раскладывает сущности сам, и получается длинный список. Готовая раскладка группирует их так же, как в приложении: показания сверху, затем **Drying**, **Storage** и **Service**.

1. `Settings` → `Dashboards` → **Add dashboard** → пустой дашборд, откройте его.
2. Правый верхний угол → карандаш (**Edit**) → меню «⋮» → **Raw configuration editor**.
3. Вставьте содержимое ниже и сохраните.

Раскладка рассчитана на дашборд типа `sections`.

```yaml
title: iDryer
views:
- title: Devices
  path: devices
  type: sections
  max_columns: 4
  sections:
  - type: grid
    background: true
    cards:
    - type: heading
      heading: iDryer · Chamber 1
      heading_style: title
      icon: mdi:printer-3d-nozzle-heat
      badges:
      - type: entity
        entity: sensor.idryer_u1_mode
        show_icon: false
        show_state: true
        color: primary
    - type: tile
      entity: sensor.idryer_u1_temperature
      name: Temperature
    - type: tile
      entity: sensor.idryer_u1_humidity
      name: Humidity
    - type: tile
      entity: sensor.idryer_u1_heater_temperature
      name: Heater
    - type: tile
      entity: sensor.idryer_u1_heater_power
      name: Heater power
    - type: tile
      entity: binary_sensor.idryer_u1_fan
      name: Fan
    - type: tile
      entity: binary_sensor.idryer_u1_damper
      name: Damper
    - type: tile
      entity: sensor.idryer_u1_weight
      name: Spool weight
      visibility:
      - condition: state
        entity: sensor.idryer_u1_weight
        state_not:
        - unknown
        - unavailable
    - type: heading
      heading: Drying
      heading_style: subtitle
    - type: tile
      entity: number.idryer_u1_drying_temperature
      name: Temperature
      features:
      - type: numeric-input
        style: buttons
      features_position: bottom
    - type: tile
      entity: number.idryer_u1_drying_duration
      name: Duration
      features:
      - type: numeric-input
        style: buttons
      features_position: bottom
    - type: tile
      entity: button.idryer_u1_drying
      name: Start drying
      icon: mdi:play
      hide_state: true
      tap_action: &id001
        action: perform-action
        perform_action: button.press
        target:
          entity_id: button.idryer_u1_drying
      icon_tap_action: *id001
    - type: heading
      heading: Storage
      heading_style: subtitle
    - type: tile
      entity: number.idryer_u1_storage_temperature
      name: Temperature
      features:
      - type: numeric-input
        style: buttons
      features_position: bottom
    - type: tile
      entity: number.idryer_u1_storage_humidity
      name: Humidity
      features:
      - type: numeric-input
        style: buttons
      features_position: bottom
    - type: tile
      entity: button.idryer_u1_storage
      name: Start storage
      icon: mdi:play
      hide_state: true
      tap_action: &id002
        action: perform-action
        perform_action: button.press
        target:
          entity_id: button.idryer_u1_storage
      icon_tap_action: *id002
    - type: heading
      heading: Service
      heading_style: subtitle
    - type: tile
      entity: button.idryer_u1_stop
      name: Stop
      icon: mdi:stop
      hide_state: true
      tap_action: &id003
        action: perform-action
        perform_action: button.press
        target:
          entity_id: button.idryer_u1_stop
      icon_tap_action: *id003
    - type: tile
      entity: button.idryer_u1_identify
      name: Identify
      icon: mdi:bell-ring-outline
      hide_state: true
      tap_action: &id004
        action: perform-action
        perform_action: button.press
        target:
          entity_id: button.idryer_u1_identify
      icon_tap_action: *id004
    - type: tile
      entity: button.idryer_u1_clear_errors
      name: Clear errors
      icon: mdi:alert-remove-outline
      hide_state: true
      tap_action: &id005
        action: perform-action
        perform_action: button.press
        target:
          entity_id: button.idryer_u1_clear_errors
      icon_tap_action: *id005
```

![Raw configuration editor со вставленной раскладкой](../../img/ha-raw-editor.png)
*Та же раскладка в редакторе конфигурации дашборда.*

### Вторая камера

У двухкамерной сушилки сущности второй камеры называются так же, но с `u2`: `sensor.idryer_u2_temperature`, `button.idryer_u2_drying` и так далее. Скопируйте блок `- type: grid` целиком, вставьте следом за первым и замените в нём `u1` на `u2`, а заголовок — на `iDryer · Chamber 2`. Получится два столбца рядом.

## Если имена сущностей не совпали

Раскладка рассчитана на стандартные идентификаторы вида `sensor.idryer_u1_temperature`. Если карточка показывает «Entity not found», посмотрите свои: `Settings` → `Devices & services` → **MQTT** → ваше устройство → список сущностей, — и замените префикс в раскладке на свой.

## Диагностика

| Симптом | Что проверить |
|---|---|
| Устройство не появилось в HA | В портале у интеграции Home Assistant стоит галочка «Включено», адрес и порт брокера верны. Прибор должен быть `Online`. |
| Появилось, но значения `Unknown` | Подождите цикл телеметрии. Если пусто дальше — брокер не хранит retained-сообщения либо прибор к нему не подключился. |
| Кнопки не срабатывают | Проверьте, что у брокера разрешена публикация в топики `idryer/#`, а у прибора в логе нет ошибок авторизации. |
| Сущности-призраки со значением `Unknown` | Остались retained-сообщения от прежней прошивки. Очистите: `mosquitto_pub -h <брокер> -t 'homeassistant/<...>/config' -n -r`. |
