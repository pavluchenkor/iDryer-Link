# Home Assistant

Ein Trockner mit Link-Modul meldet sich in Home Assistant über **MQTT Discovery** an: HA erstellt die Entitäten selbst — Kammermesswerte, Felder für den Start von Trocknung und Lagerung, Schaltflächen. Das Portal wird dafür nicht benötigt, alles läuft über Ihren MQTT-Broker.

Nachfolgend: die Integration einschalten, prüfen und ein fertiges Karten-Layout, damit das Gerät wie auf dem Bild aussieht und nicht wie eine Liste von Entitäten.

![Gerätekarten in Home Assistant](../../img/link-ha-card.png)
*Beide Kammern des Trockners auf dem Home-Assistant-Panel: Messwerte, Start von Trocknung und Lagerung, Wartung.*

!!! note
    Das Gerät **erscheint nicht** unter `Settings → Devices & services → Discovered`: Das ist MQTT Discovery, nicht UPnP/zeroconf. Die Integration **MQTT** muss in Home Assistant bereits hinzugefügt sein.

## Was benötigt wird

1. Ein MQTT-Broker: das Add-on **Mosquitto broker** in Home Assistant oder ein beliebiger Broker in Ihrem Netzwerk.
2. Die in Home Assistant hinzugefügte Integration **MQTT**, die auf diesen Broker zeigt.
3. Ein Trockner mit Link-Modul im Netzwerk und `Online` auf dem Portal.

## Schritt 1. Die Integration am Gerät einschalten

Öffnen Sie das Gerät auf [portal.idryer.org](https://portal.idryer.org/) und suchen Sie den Block **Integrationen** → **Home Assistant**.

| Feld | Was einzutragen ist |
|---|---|
| Host | die Adresse des Brokers in Ihrem Netzwerk, zum Beispiel `192.168.1.27` |
| Port | der Port des Brokers, üblicherweise `1883` |
| Username / Password | die Zugangsdaten des Brokers, falls er sie verlangt |
| Discovery prefix | `homeassistant`, sofern Sie ihn in den HA-Einstellungen nicht geändert haben |
| Aktiviert | das Häkchen — sonst verbindet sich das Gerät nicht mit dem Broker |

Die Einstellungen gehen über das lokale Netzwerk direkt an das Gerät — das Portal speichert sie nicht.

![Das Fenster Home Assistant im Block „Integrationen“ auf dem Portal](../../img/link-ha-portal-integration.png)
*Die Adresse des Brokers, der Port und das Kennzeichen „Aktiviert“ — mehr braucht das Gerät nicht.*

## Schritt 2. Das Gerät in Home Assistant finden

`Settings` → `Devices & services` → die Karte **MQTT** → klappen Sie im Abschnitt **Services** den Knoten des Brokers auf. Die iDryer-Geräte erscheinen unter Seriennummern der Form `DEVICE_*`.

![iDryer-Geräte auf der Seite der MQTT-Integration](../../img/link-ha-mqtt-devices.png)
*Die Geräte unter dem Knoten des Brokers; beim Trockner ist die Anzahl der Entitäten zu sehen.*

Öffnen Sie das Gerät: HA zeigt bereits die Messwerte und die Bedienelemente. Prüfen Sie, dass die Werte aktuell sind — sie werden zusammen mit der Telemetrie des Geräts aktualisiert.

## Schritt 3. Die Karte zusammenstellen

HA ordnet die Entitäten selbst an, und es entsteht eine lange Liste. Das fertige Layout gruppiert sie genauso wie in der App: die Messwerte oben, danach **Drying**, **Storage** und **Service**.

1. `Settings` → `Dashboards` → **Add dashboard** → ein leeres Dashboard, öffnen Sie es.
2. Rechte obere Ecke → der Stift (**Edit**) → das Menü „⋮“ → **Raw configuration editor**.
3. Fügen Sie den nachfolgenden Inhalt ein und speichern Sie.

Das Layout ist für ein Dashboard des Typs `sections` ausgelegt.

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

![Raw configuration editor mit dem eingefügten Layout](../../img/link-ha-raw-editor.png)
*Dasselbe Layout im Konfigurationseditor des Dashboards.*

### Die zweite Kammer

Bei einem Trockner mit zwei Kammern heißen die Entitäten der zweiten Kammer genauso, nur mit `u2`: `sensor.idryer_u2_temperature`, `button.idryer_u2_drying` und so weiter. Kopieren Sie den Block `- type: grid` vollständig, fügen Sie ihn hinter dem ersten ein und ersetzen Sie darin `u1` durch `u2` sowie die Überschrift durch `iDryer · Chamber 2`. So entstehen zwei Spalten nebeneinander.

## Wenn die Namen der Entitäten nicht übereinstimmen

Das Layout ist für Standardbezeichner der Form `sensor.idryer_u1_temperature` ausgelegt. Zeigt die Karte „Entity not found“, sehen Sie Ihre eigenen nach: `Settings` → `Devices & services` → **MQTT** → Ihr Gerät → die Liste der Entitäten — und ersetzen Sie das Präfix im Layout durch Ihres.

## Diagnose

| Symptom | Was zu prüfen ist |
|---|---|
| Das Gerät ist nicht in HA erschienen | Auf dem Portal ist bei der Integration Home Assistant das Häkchen „Aktiviert“ gesetzt, Adresse und Port des Brokers stimmen. Das Gerät muss `Online` sein. |
| Es ist erschienen, aber die Werte sind `Unknown` | Warten Sie einen Telemetriezyklus ab. Bleibt es weiterhin leer — der Broker speichert keine Retained-Nachrichten oder das Gerät hat sich nicht mit ihm verbunden. |
| Die Schaltflächen reagieren nicht | Prüfen Sie, dass der Broker das Veröffentlichen in die Topics `idryer/#` erlaubt und dass im Log des Geräts keine Autorisierungsfehler stehen. |
| Phantom-Entitäten mit dem Wert `Unknown` | Es sind Retained-Nachrichten der früheren Firmware übrig geblieben. Löschen Sie sie: `mosquitto_pub -h <Broker> -t 'homeassistant/<...>/config' -n -r`. |
