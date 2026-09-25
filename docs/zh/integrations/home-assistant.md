# Home Assistant

带 Link 模块的干燥机通过 **MQTT Discovery** 把自己发布到 Home Assistant：HA 自行创建实体 —— 腔室读数、启动烘干和存储的参数字段、按钮。这不需要门户，全部通过您的 MQTT 代理完成。

下面是启用集成、检查以及现成的卡片布局，让设备显示成图中的样子，而不是一串实体列表。

![Home Assistant 中的设备卡片](../../img/ha-card.png)
*Home Assistant 面板上干燥机的两个腔室：读数、启动烘干和存储、维护。*

!!! note
    设备**不会出现**在 `Settings → Devices & services → Discovered` 中：这是 MQTT Discovery，不是 UPnP/zeroconf。Home Assistant 中必须事先添加 **MQTT** 集成。

## 需要什么

1. MQTT 代理：Home Assistant 中的 **Mosquitto broker** 加载项，或您网络中的任意代理。
2. Home Assistant 中已添加指向该代理的 **MQTT** 集成。
3. 带 Link 模块的干燥机已接入网络，并在门户上显示 `Online`。

## 步骤 1. 在设备上启用集成

在 [portal.idryer.org](https://portal.idryer.org/) 上打开设备，找到 **集成** → **Home Assistant** 区块。

| 字段 | 填写内容 |
|---|---|
| Host | 您网络中代理的地址，例如 `192.168.1.27` |
| Port | 代理端口，通常是 `1883` |
| Username / Password | 代理的凭据，如果代理要求的话 |
| Discovery prefix | `homeassistant`，如果没有在 HA 设置中改过 |
| 已启用 | 勾选 —— 否则设备不会连接到代理 |

设置通过局域网直接发送到设备 —— 门户不保存它们。

![门户「集成」区块中的 Home Assistant 窗口](../../img/ha-portal-integration.png)
*代理地址、端口和「已启用」标记 —— 设备需要的全部内容。*

## 步骤 2. 在 Home Assistant 中找到设备

`Settings` → `Devices & services` → **MQTT** 卡片 → 在 **Services** 部分展开代理节点。iDryer 设备以 `DEVICE_*` 形式的序列号显示。

![MQTT 集成页面上的 iDryer 设备](../../img/ha-mqtt-devices.png)
*代理节点下的设备；干燥机会显示实体数量。*

打开设备：HA 已经显示读数和控制元素。检查数值是否在实时变化 —— 它们随设备遥测一起更新。

## 步骤 3. 组装卡片

HA 自行排列实体，结果是一长串列表。现成的布局按应用中的方式分组：读数在上方，然后是 **Drying**、**Storage** 和 **Service**。

1. `Settings` → `Dashboards` → **Add dashboard** → 空仪表板，打开它。
2. 右上角 → 铅笔（**Edit**）→「⋮」菜单 → **Raw configuration editor**。
3. 粘贴下面的内容并保存。

该布局适用于 `sections` 类型的仪表板。

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

![粘贴了布局的 Raw configuration editor](../../img/ha-raw-editor.png)
*仪表板配置编辑器中的同一布局。*

### 第二个腔室

双腔干燥机第二个腔室的实体名称相同，只是带 `u2`：`sensor.idryer_u2_temperature`、`button.idryer_u2_drying` 等等。完整复制 `- type: grid` 区块，粘贴到第一个之后，把其中的 `u1` 换成 `u2`，标题换成 `iDryer · Chamber 2`。这样会得到并排的两列。

## 如果实体名称对不上

该布局针对 `sensor.idryer_u1_temperature` 这类标准标识符。如果卡片显示「Entity not found」，请查看自己的名称：`Settings` → `Devices & services` → **MQTT** → 您的设备 → 实体列表，然后把布局中的前缀换成您自己的。

## 诊断

| 症状 | 检查内容 |
|---|---|
| 设备没有出现在 HA 中 | 门户上 Home Assistant 集成勾选了「已启用」，代理地址和端口正确。设备必须是 `Online`。 |
| 出现了，但数值是 `Unknown` | 等待一个遥测周期。如果之后仍然为空 —— 代理不保存 retained 消息，或者设备没有连上它。 |
| 按钮没有反应 | 检查代理是否允许发布到 `idryer/#` 主题，以及设备日志中有没有授权错误。 |
| 数值为 `Unknown` 的幽灵实体 | 旧固件残留的 retained 消息。清除：`mosquitto_pub -h <代理> -t 'homeassistant/<...>/config' -n -r`。 |
