# Home Assistant

Сушилка с модулем Link публикует себя в Home Assistant через **MQTT Discovery**: HA сам создаёт сущности — показания камеры, поля запуска сушки и хранения, кнопки. Портал для этого не нужен, всё идёт через ваш MQTT-брокер.

Ниже — включение интеграции, проверка и готовая раскладка карточки, чтобы прибор выглядел как на картинке, а не списком сущностей.

<!-- СКРИНШОТ: итоговая карточка сушилки в Home Assistant (две камеры рядом) -->

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
| Host | адрес брокера в вашей сети, например `192.168.1.60` |
| Port | порт брокера, обычно `1883` |
| Username / Password | учётные данные брокера, если он их требует |
| Discovery prefix | `homeassistant`, если не меняли его в настройках HA |
| Включено | галочка — иначе прибор к брокеру не подключится |

Настройки уходят прямо на прибор по локальной сети — портал их не хранит.

<!-- СКРИНШОТ: блок «Интеграции» на портале и окно Home Assistant с полями -->

## Шаг 2. Найти устройство в Home Assistant

`Settings` → `Devices & services` → карточка **MQTT** → в разделе **Services** разверните узел брокера. Приборы iDryer видны под серийными номерами вида `DEVICE_*`.

<!-- СКРИНШОТ: устройство сушилки на странице интеграции MQTT -->

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
      heading: iDryer · Камера 1
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
      name: Температура
    - type: tile
      entity: sensor.idryer_u1_humidity
      name: Влажность
    - type: tile
      entity: sensor.idryer_u1_heater_temperature
      name: Нагреватель
    - type: tile
      entity: sensor.idryer_u1_heater_power
      name: Мощность нагрева
    - type: tile
      entity: binary_sensor.idryer_u1_fan
      name: Вентилятор
    - type: tile
      entity: binary_sensor.idryer_u1_damper
      name: Заслонка
    - type: tile
      entity: sensor.idryer_u1_weight
      name: Вес катушки
      visibility:
      - condition: state
        entity: sensor.idryer_u1_weight
        state_not:
        - unknown
        - unavailable
    - type: heading
      heading: Сушка
      heading_style: subtitle
    - type: tile
      entity: number.idryer_u1_drying_temperature
      name: Температура
      features:
      - type: numeric-input
        style: buttons
      features_position: bottom
    - type: tile
      entity: number.idryer_u1_drying_duration
      name: Длительность
      features:
      - type: numeric-input
        style: buttons
      features_position: bottom
    - type: tile
      entity: button.idryer_u1_drying
      name: Запустить сушку
      icon: mdi:play
      hide_state: true
      tap_action: &id001
        action: perform-action
        perform_action: button.press
        target:
          entity_id: button.idryer_u1_drying
      icon_tap_action: *id001
    - type: heading
      heading: Хранение
      heading_style: subtitle
    - type: tile
      entity: number.idryer_u1_storage_temperature
      name: Температура
      features:
      - type: numeric-input
        style: buttons
      features_position: bottom
    - type: tile
      entity: number.idryer_u1_storage_humidity
      name: Влажность
      features:
      - type: numeric-input
        style: buttons
      features_position: bottom
    - type: tile
      entity: button.idryer_u1_storage
      name: Запустить хранение
      icon: mdi:play
      hide_state: true
      tap_action: &id002
        action: perform-action
        perform_action: button.press
        target:
          entity_id: button.idryer_u1_storage
      icon_tap_action: *id002
    - type: heading
      heading: Обслуживание
      heading_style: subtitle
    - type: tile
      entity: button.idryer_u1_stop
      name: Стоп
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
      name: Найти прибор
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
      name: Сбросить ошибки
      icon: mdi:alert-remove-outline
      hide_state: true
      tap_action: &id005
        action: perform-action
        perform_action: button.press
        target:
          entity_id: button.idryer_u1_clear_errors
      icon_tap_action: *id005
```

<!-- СКРИНШОТ: Raw configuration editor со вставленной раскладкой -->

### Вторая камера

У двухкамерной сушилки сущности второй камеры называются так же, но с `u2`: `sensor.idryer_u2_temperature`, `button.idryer_u2_drying` и так далее. Скопируйте блок `- type: grid` целиком, вставьте следом за первым и замените в нём `u1` на `u2`, а заголовок — на «Камера 2». Получится два столбца рядом.

## Если имена сущностей не совпали

Раскладка рассчитана на стандартные идентификаторы вида `sensor.idryer_u1_temperature`. Если карточка показывает «Entity not found», посмотрите свои: `Settings` → `Devices & services` → **MQTT** → ваше устройство → список сущностей, — и замените префикс в раскладке на свой.

## Диагностика

| Симптом | Что проверить |
|---|---|
| Устройство не появилось в HA | В портале у интеграции Home Assistant стоит галочка «Включено», адрес и порт брокера верны. Прибор должен быть `Online`. |
| Появилось, но значения `Unknown` | Подождите цикл телеметрии. Если пусто дальше — брокер не хранит retained-сообщения либо прибор к нему не подключился. |
| Кнопки не срабатывают | Проверьте, что у брокера разрешена публикация в топики `idryer/#`, а у прибора в логе нет ошибок авторизации. |
| Сущности-призраки со значением `Unknown` | Остались retained-сообщения от прежней прошивки. Очистите: `mosquitto_pub -h <брокер> -t 'homeassistant/<...>/config' -n -r`. |
