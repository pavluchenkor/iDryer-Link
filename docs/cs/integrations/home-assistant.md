# Home Assistant

Sušička s modulem Link se do Home Assistant publikuje přes **MQTT Discovery**: HA sám vytvoří entity — hodnoty z komory, pole pro spuštění sušení a skladování, tlačítka. Portál k tomu není potřeba, vše jde přes váš MQTT broker.

Níže je zapnutí integrace, kontrola a hotové rozložení karty, aby zařízení vypadalo jako na obrázku, a ne jako seznam entit.

![Karty zařízení v Home Assistant](../../img/link-ha-card.png)
*Obě komory sušičky na panelu Home Assistant: hodnoty, spuštění sušení a skladování, servis.*

!!! note
    Zařízení se **neobjeví** v `Settings → Devices & services → Discovered`: jde o MQTT Discovery, ne o UPnP/zeroconf. Integrace **MQTT** musí být v Home Assistant přidána předem.

## Co je potřeba

1. MQTT broker: doplněk **Mosquitto broker** v Home Assistant nebo libovolný broker ve vaší síti.
2. V Home Assistant je přidaná integrace **MQTT**, která ukazuje na tento broker.
3. Sušička s modulem Link je v síti a na portálu `Online`.

## Krok 1. Zapnout integraci na zařízení

Otevřete zařízení na [portal.idryer.org](https://portal.idryer.org/) a najděte blok **Integrace** → **Home Assistant**.

| Pole | Co vyplnit |
|---|---|
| Host | adresa brokeru ve vaší síti, například `192.168.1.27` |
| Port | port brokeru, obvykle `1883` |
| Username / Password | přihlašovací údaje brokeru, pokud je vyžaduje |
| Discovery prefix | `homeassistant`, pokud jste jej v nastavení HA neměnili |
| Zapnuto | zaškrtnutí — jinak se zařízení k brokeru nepřipojí |

Nastavení jde přímo do zařízení po místní síti — portál je neukládá.

![Okno Home Assistant v bloku „Integrace“ na portálu](../../img/link-ha-portal-integration.png)
*Adresa brokeru, port a příznak „Zapnuto“ — vše, co zařízení potřebuje.*

## Krok 2. Najít zařízení v Home Assistant

`Settings` → `Devices & services` → karta **MQTT** → v sekci **Services** rozbalte uzel brokeru. Zařízení iDryer jsou vidět pod sériovými čísly ve tvaru `DEVICE_*`.

![Zařízení iDryer na stránce integrace MQTT](../../img/link-ha-mqtt-devices.png)
*Zařízení pod uzlem brokeru; u sušičky je vidět počet entit.*

Otevřete zařízení: HA už ukazuje hodnoty a ovládací prvky. Zkontrolujte, že jsou hodnoty živé — aktualizují se spolu s telemetrií zařízení.

## Krok 3. Sestavit kartu

HA si entity rozloží sám a vznikne dlouhý seznam. Hotové rozložení je seskupí stejně jako v aplikaci: hodnoty nahoře, pak **Drying**, **Storage** a **Service**.

1. `Settings` → `Dashboards` → **Add dashboard** → prázdný dashboard, otevřete jej.
2. Pravý horní roh → tužka (**Edit**) → nabídka „⋮“ → **Raw configuration editor**.
3. Vložte obsah níže a uložte.

Rozložení počítá s dashboardem typu `sections`.

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

![Raw configuration editor s vloženým rozložením](../../img/link-ha-raw-editor.png)
*Totéž rozložení v editoru konfigurace dashboardu.*

### Druhá komora

U dvoukomorové sušičky se entity druhé komory jmenují stejně, jen s `u2`: `sensor.idryer_u2_temperature`, `button.idryer_u2_drying` a tak dále. Zkopírujte celý blok `- type: grid`, vložte jej za první a nahraďte v něm `u1` za `u2` a nadpis za `iDryer · Chamber 2`. Vzniknou dva sloupce vedle sebe.

## Pokud se názvy entit neshodují

Rozložení počítá se standardními identifikátory ve tvaru `sensor.idryer_u1_temperature`. Pokud karta ukazuje „Entity not found“, podívejte se na své: `Settings` → `Devices & services` → **MQTT** → vaše zařízení → seznam entit — a nahraďte prefix v rozložení svým.

## Diagnostika

| Příznak | Co zkontrolovat |
|---|---|
| Zařízení se v HA neobjevilo | Na portálu má integrace Home Assistant zaškrtnuté „Zapnuto“, adresa a port brokeru jsou správné. Zařízení musí být `Online`. |
| Objevilo se, ale hodnoty jsou `Unknown` | Počkejte na cyklus telemetrie. Pokud je prázdno i dál — broker neuchovává retained zprávy nebo se k němu zařízení nepřipojilo. |
| Tlačítka nereagují | Zkontrolujte, že broker povoluje publikaci do témat `idryer/#` a že zařízení nemá v logu chyby autorizace. |
| Duchové entit s hodnotou `Unknown` | Zůstaly retained zprávy z předchozího firmwaru. Vyčistěte je: `mosquitto_pub -h <broker> -t 'homeassistant/<...>/config' -n -r`. |
