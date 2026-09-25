# Home Assistant

A secadora com o módulo Link se publica no Home Assistant via **MQTT Discovery**: o próprio HA cria as entidades — leituras da câmara, campos de início da secagem e do armazenamento, botões. O portal não é necessário para isso, tudo passa pelo seu broker MQTT.

Abaixo: ativação da integração, verificação e um layout de cartão pronto, para que o aparelho fique como na imagem e não como uma lista de entidades.

![Cartões dos aparelhos no Home Assistant](../../img/ha-card.png)
*As duas câmaras da secadora no painel do Home Assistant: leituras, início da secagem e do armazenamento, manutenção.*

!!! note
    O aparelho **não vai aparecer** em `Settings → Devices & services → Discovered`: isto é MQTT Discovery, não UPnP/zeroconf. A integração **MQTT** no Home Assistant deve estar adicionada com antecedência.

## O que é necessário

1. Um broker MQTT: o add-on **Mosquitto broker** no Home Assistant ou qualquer broker na sua rede.
2. A integração **MQTT** adicionada no Home Assistant, apontando para esse broker.
3. A secadora com o módulo Link na rede e `Online` no portal.

## Passo 1. Ativar a integração no aparelho

Abra o dispositivo em [portal.idryer.org](https://portal.idryer.org/) e encontre o bloco **Integrações** → **Home Assistant**.

| Campo | O que preencher |
|---|---|
| Host | o endereço do broker na sua rede, por exemplo `192.168.1.27` |
| Port | a porta do broker, normalmente `1883` |
| Username / Password | as credenciais do broker, se ele as exigir |
| Discovery prefix | `homeassistant`, se você não o alterou nas configurações do HA |
| Ativado | a caixa de seleção — sem ela o aparelho não se conecta ao broker |

As configurações vão direto para o aparelho pela rede local — o portal não as armazena.

![A janela do Home Assistant no bloco «Integrações» do portal](../../img/ha-portal-integration.png)
*O endereço do broker, a porta e a marca «Ativado» — tudo o que o aparelho precisa.*

## Passo 2. Encontrar o dispositivo no Home Assistant

`Settings` → `Devices & services` → o cartão **MQTT** → na seção **Services** expanda o nó do broker. Os aparelhos iDryer aparecem sob números de série no formato `DEVICE_*`.

![Aparelhos iDryer na página da integração MQTT](../../img/ha-mqtt-devices.png)
*Dispositivos sob o nó do broker; na secadora aparece a quantidade de entidades.*

Abra o dispositivo: o HA já mostra as leituras e os controles. Verifique se os valores estão vivos — eles se atualizam junto com a telemetria do aparelho.

## Passo 3. Montar o cartão

O HA distribui as entidades por conta própria, e o resultado é uma lista longa. O layout pronto as agrupa da mesma forma que no aplicativo: leituras em cima, depois **Drying**, **Storage** e **Service**.

1. `Settings` → `Dashboards` → **Add dashboard** → um painel vazio, abra-o.
2. Canto superior direito → o lápis (**Edit**) → o menu «⋮» → **Raw configuration editor**.
3. Cole o conteúdo abaixo e salve.

O layout é feito para um painel do tipo `sections`.

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

![Raw configuration editor com o layout colado](../../img/ha-raw-editor.png)
*O mesmo layout no editor de configuração do painel.*

### Segunda câmara

Na secadora de duas câmaras as entidades da segunda câmara têm os mesmos nomes, mas com `u2`: `sensor.idryer_u2_temperature`, `button.idryer_u2_drying` e assim por diante. Copie o bloco `- type: grid` inteiro, cole-o logo após o primeiro e substitua nele `u1` por `u2`, e o título por `iDryer · Chamber 2`. O resultado são duas colunas lado a lado.

## Se os nomes das entidades não coincidirem

O layout é feito para os identificadores padrão do tipo `sensor.idryer_u1_temperature`. Se o cartão mostrar «Entity not found», veja os seus: `Settings` → `Devices & services` → **MQTT** → o seu dispositivo → a lista de entidades, — e substitua o prefixo no layout pelo seu.

## Diagnóstico

| Sintoma | O que verificar |
|---|---|
| O dispositivo não apareceu no HA | No portal, a integração Home Assistant está com a marca «Ativado», o endereço e a porta do broker estão corretos. O aparelho deve estar `Online`. |
| Apareceu, mas os valores estão `Unknown` | Aguarde um ciclo de telemetria. Se continuar vazio — o broker não guarda mensagens retained ou o aparelho não se conectou a ele. |
| Os botões não funcionam | Verifique se o broker permite a publicação nos tópicos `idryer/#` e se não há erros de autorização no log do aparelho. |
| Entidades-fantasma com o valor `Unknown` | Restaram mensagens retained do firmware anterior. Limpe-as: `mosquitto_pub -h <broker> -t 'homeassistant/<...>/config' -n -r`. |
