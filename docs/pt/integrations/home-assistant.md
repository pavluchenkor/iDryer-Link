# Home Assistant

O secador com módulo Link publica-se no Home Assistant através de **MQTT Discovery**: o HA cria as entidades por si — leituras da câmara, campos de arranque da secagem e do armazenamento, botões. O portal não é necessário para isto, tudo passa pelo seu broker MQTT.

A seguir: como ativar a integração, como verificar e uma disposição de cartão pronta a usar, para que o dispositivo fique com o aspeto da imagem e não como uma lista de entidades.

![Cartões dos dispositivos no Home Assistant](../../img/link-ha-card.png)
*Ambas as câmaras do secador no painel do Home Assistant: leituras, arranque da secagem e do armazenamento, manutenção.*

!!! note
    O dispositivo **não aparece** em `Settings → Devices & services → Discovered`: isto é MQTT Discovery, não UPnP/zeroconf. A integração **MQTT** no Home Assistant tem de estar adicionada previamente.

## O que é necessário

1. Um broker MQTT: o add-on **Mosquitto broker** no Home Assistant ou qualquer broker na sua rede.
2. No Home Assistant, a integração **MQTT** adicionada e a apontar para esse broker.
3. O secador com módulo Link na rede e `Online` no portal.

## Passo 1. Ativar a integração no dispositivo

Abra o dispositivo em [portal.idryer.org](https://portal.idryer.org/) e encontre o bloco **Integrações** → **Home Assistant**.

| Campo | O que introduzir |
|---|---|
| Host | o endereço do broker na sua rede, por exemplo `192.168.1.27` |
| Port | a porta do broker, normalmente `1883` |
| Username / Password | as credenciais do broker, se este as exigir |
| Discovery prefix | `homeassistant`, se não o tiver alterado nas definições do HA |
| Ativado | a caixa de verificação — caso contrário o dispositivo não se liga ao broker |

As definições vão diretamente para o dispositivo através da rede local — o portal não as guarda.

![Janela do Home Assistant no bloco «Integrações» do portal](../../img/link-ha-portal-integration.png)
*O endereço do broker, a porta e a marca «Ativado» — tudo o que o dispositivo precisa.*

## Passo 2. Encontrar o dispositivo no Home Assistant

`Settings` → `Devices & services` → cartão **MQTT** → na secção **Services**, expanda o nó do broker. Os dispositivos iDryer aparecem com números de série no formato `DEVICE_*`.

![Dispositivos iDryer na página da integração MQTT](../../img/link-ha-mqtt-devices.png)
*Dispositivos sob o nó do broker; no secador vê-se o número de entidades.*

Abra o dispositivo: o HA já mostra as leituras e os elementos de controlo. Verifique que os valores estão vivos — são atualizados juntamente com a telemetria do dispositivo.

## Passo 3. Montar o cartão

O HA dispõe as entidades por si, e o resultado é uma lista longa. A disposição pronta agrupa-as da mesma forma que na aplicação: as leituras em cima, depois **Drying**, **Storage** e **Service**.

1. `Settings` → `Dashboards` → **Add dashboard** → um painel vazio, abra-o.
2. Canto superior direito → o lápis (**Edit**) → menu «⋮» → **Raw configuration editor**.
3. Cole o conteúdo abaixo e guarde.

A disposição destina-se a um painel do tipo `sections`.

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

![Raw configuration editor com a disposição colada](../../img/link-ha-raw-editor.png)
*A mesma disposição no editor de configuração do painel.*

### Segunda câmara

Num secador de duas câmaras, as entidades da segunda câmara têm os mesmos nomes, mas com `u2`: `sensor.idryer_u2_temperature`, `button.idryer_u2_drying` e assim por diante. Copie o bloco `- type: grid` na íntegra, cole-o a seguir ao primeiro e substitua nele `u1` por `u2` e o título por `iDryer · Chamber 2`. Ficam duas colunas lado a lado.

## Se os nomes das entidades não coincidirem

A disposição destina-se aos identificadores padrão do tipo `sensor.idryer_u1_temperature`. Se o cartão mostrar «Entity not found», consulte os seus: `Settings` → `Devices & services` → **MQTT** → o seu dispositivo → lista de entidades — e substitua o prefixo na disposição pelo seu.

## Diagnóstico

| Sintoma | O que verificar |
|---|---|
| O dispositivo não apareceu no HA | No portal, a integração Home Assistant tem a caixa «Ativado» marcada e o endereço e a porta do broker estão corretos. O dispositivo tem de estar `Online`. |
| Apareceu, mas os valores são `Unknown` | Aguarde um ciclo de telemetria. Se continuar vazio — o broker não guarda mensagens retained ou o dispositivo não se ligou a ele. |
| Os botões não funcionam | Verifique que o broker permite a publicação nos tópicos `idryer/#` e que o registo do dispositivo não tem erros de autorização. |
| Entidades fantasma com o valor `Unknown` | Ficaram mensagens retained de um firmware anterior. Limpe-as: `mosquitto_pub -h <broker> -t 'homeassistant/<...>/config' -n -r`. |
