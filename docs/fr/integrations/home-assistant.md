# Home Assistant

Le séchoir équipé du module Link se publie dans Home Assistant via **MQTT Discovery** : HA crée lui-même les entités — relevés de la chambre, champs de lancement du séchage et du stockage, boutons. Le portail n'est pas nécessaire, tout passe par votre broker MQTT.

Ci-dessous : l'activation de l'intégration, la vérification et une disposition de carte prête à l'emploi, pour que l'appareil ressemble à l'illustration et non à une liste d'entités.

![Cartes des appareils dans Home Assistant](../../img/ha-card.png)
*Les deux chambres du séchoir sur le tableau de bord Home Assistant : relevés, lancement du séchage et du stockage, maintenance.*

!!! note
    L'appareil **n'apparaîtra pas** dans `Settings → Devices & services → Discovered` : il s'agit de MQTT Discovery, pas d'UPnP/zeroconf. L'intégration **MQTT** doit être ajoutée au préalable dans Home Assistant.

## Prérequis

1. Un broker MQTT : le module complémentaire **Mosquitto broker** dans Home Assistant ou tout autre broker de votre réseau.
2. L'intégration **MQTT** ajoutée dans Home Assistant et pointant vers ce broker.
3. Le séchoir avec le module Link présent sur le réseau et `Online` sur le portail.

## Étape 1. Activer l'intégration sur l'appareil

Ouvrez l'appareil sur [portal.idryer.org](https://portal.idryer.org/) et trouvez le bloc **Intégrations** → **Home Assistant**.

| Champ | Que saisir |
|---|---|
| Host | l'adresse du broker dans votre réseau, par exemple `192.168.1.27` |
| Port | le port du broker, généralement `1883` |
| Username / Password | les identifiants du broker, s'il les exige |
| Discovery prefix | `homeassistant`, si vous ne l'avez pas modifié dans les paramètres de HA |
| Activé | la case à cocher — sinon l'appareil ne se connectera pas au broker |

Les paramètres sont transmis directement à l'appareil via le réseau local — le portail ne les conserve pas.

![Fenêtre Home Assistant dans le bloc « Intégrations » du portail](../../img/ha-portal-integration.png)
*L'adresse du broker, le port et la case « Activé » — c'est tout ce dont l'appareil a besoin.*

## Étape 2. Trouver l'appareil dans Home Assistant

`Settings` → `Devices & services` → carte **MQTT** → dans la section **Services**, développez le nœud du broker. Les appareils iDryer y figurent sous des numéros de série de la forme `DEVICE_*`.

![Appareils iDryer sur la page de l'intégration MQTT](../../img/ha-mqtt-devices.png)
*Les appareils sous le nœud du broker ; pour le séchoir, le nombre d'entités est indiqué.*

Ouvrez l'appareil : HA affiche déjà les relevés et les éléments de contrôle. Vérifiez que les valeurs sont actualisées — elles évoluent avec la télémétrie de l'appareil.

## Étape 3. Composer la carte

HA dispose les entités lui-même, ce qui donne une longue liste. La disposition prête à l'emploi les regroupe comme dans l'application : les relevés en haut, puis **Drying**, **Storage** et **Service**.

1. `Settings` → `Dashboards` → **Add dashboard** → un tableau de bord vide, ouvrez-le.
2. Coin supérieur droit → crayon (**Edit**) → menu « ⋮ » → **Raw configuration editor**.
3. Collez le contenu ci-dessous et enregistrez.

La disposition est prévue pour un tableau de bord de type `sections`.

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

![Raw configuration editor avec la disposition collée](../../img/ha-raw-editor.png)
*La même disposition dans l'éditeur de configuration du tableau de bord.*

### Deuxième chambre

Sur un séchoir à deux chambres, les entités de la deuxième chambre portent les mêmes noms, mais avec `u2` : `sensor.idryer_u2_temperature`, `button.idryer_u2_drying`, et ainsi de suite. Copiez le bloc `- type: grid` en entier, collez-le à la suite du premier et remplacez-y `u1` par `u2`, ainsi que le titre par `iDryer · Chamber 2`. Vous obtiendrez deux colonnes côte à côte.

## Si les noms des entités ne correspondent pas

La disposition est prévue pour des identifiants standard de la forme `sensor.idryer_u1_temperature`. Si la carte affiche « Entity not found », consultez les vôtres : `Settings` → `Devices & services` → **MQTT** → votre appareil → liste des entités, — et remplacez le préfixe dans la disposition par le vôtre.

## Diagnostic

| Symptôme | À vérifier |
|---|---|
| L'appareil n'apparaît pas dans HA | Sur le portail, la case « Activé » de l'intégration Home Assistant est cochée, l'adresse et le port du broker sont corrects. L'appareil doit être `Online`. |
| Il apparaît, mais les valeurs sont `Unknown` | Attendez un cycle de télémétrie. Si rien n'arrive ensuite — le broker ne conserve pas les messages retained, ou l'appareil ne s'y est pas connecté. |
| Les boutons ne réagissent pas | Vérifiez que le broker autorise la publication dans les topics `idryer/#` et que le journal de l'appareil ne contient pas d'erreurs d'autorisation. |
| Entités fantômes avec la valeur `Unknown` | Des messages retained d'un ancien micrologiciel subsistent. Nettoyez-les : `mosquitto_pub -h <broker> -t 'homeassistant/<...>/config' -n -r`. |
