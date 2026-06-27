# iDryer Link — guide rapide

Link est un module de communication pour iDryer. Il se connecte au port Ethernet du contrôleur et communique avec la carte de commande (le port est utilisé comme connecteur d'alimentation et UART, pas comme port réseau). Link connecte le séchoir à Internet et le relie à [portal.idryer.org](https://portal.idryer.org/).

![link1](../img/link2.png)
![link1](../img/link1.png)

## Comment connecter au contrôleur
**Coupez l'alimentation du contrôleur**

**Assemblez le câble RJ45**, consultez
![RJ45](../img/RJ45.png)
pour ne pas confondre les paires. Important : RJ45 ici est simplement un connecteur d'alimentation/UART, ne le branchez pas sur un commutateur réseau.

**Connectez les fils** à l'ESP32-C3 Super mini selon le schéma
![esp32superMini](../img/esp32superMini.png)

**Selon le type et le fabricant de la carte, la disposition des broches 6 et 7 peut différer.**
Consultez le brochage de votre carte fourni par le fabricant.

```
UART_RX_PIN 6 (blanc-bleu)
UART_TX_PIN 7 (blanc-vert)
```

**Brochage ESP32-C3 super mini**

![Brochage ESP32-C3 super mini](../img/ESP32-C3-Super-Mini-pinout-low.jpg)

**Brochage ESP32-C3 Zero (Waveshare)**

![Brochage ESP32-C3 super mini](../img/ESP32-C3-ZERO-Waveshare-pinout-low.jpg)

De même, en consultant le brochage, vous pouvez connecter n'importe quelle carte de développement.


**Schéma de câblage**

 ![wiring](../img/wiring.png)
<!-- 5) Connectez Link au port Ethernet du contrôleur. Après activation de l'alimentation, le contrôleur fonctionnera avec Link comme modem externe. -->

## Comment programmer via le flasheur web
Le flasheur web se trouve sur https://install.idryer.org/

- Connectez Link au port USB de votre ordinateur.
- Ouvrez la [page](https://install.idryer.org/) et sélectionnez le périphérique **iDryer Link**.
- Choisissez la carte :
   - `ESP32-C3 super-mini` — option principale pour les modules en série.
   - `ESP32-C3 DevKit` — si vous avez une carte de débogage.
- Cliquez sur **Connect**, sélectionnez le port série (généralement `USB JTAG/serial` ou `CH340`). Si la programmation ne démarre pas, maintenez `BOOT` sur la carte et appuyez brièvement sur `RST`.
- Cliquez sur **Install**. Le flasheur programmera tout ce qui est nécessaire.
- Après 100%, l'assistant Improv s'ouvrira : entrez le SSID et le mot de passe Wi-Fi, attendez le statut « Connected ».
- Si l'assistant Improv ne s'est pas ouvert, débranchez l'USB et recommencez la connexion, en cliquant sur **Connect** sans reprogrammer.

## Connexion au portail

- Cliquez sur le bouton **Start Claim**. Une chaîne `PIN:123456` apparaîtra — c'est le PIN, valide environ 5 minutes.
- Accédez à https://portal.idryer.org → « Ajouter un appareil » → entrez le PIN. Après un appairage réussi, l'appareil apparaîtra dans la liste.
- Débranchez l'USB et connectez Link au contrôleur via RJ45.
- Allumez iDryer
- Une indication LED bleue « respirante » confirmera la connexion réussie

## Ce que vous devriez obtenir
- Le contrôleur détecte Link immédiatement après la mise sous tension.
- Dans le portail web, l'appareil apparaît en ligne dans 1–2 minutes après la connexion à Wi-Fi.
- Si le statut n'apparaît pas : revérifiez le brochage selon `../../img/RJ45.png`, la qualité de la connexion et le choix correct de la carte à l'étape de programmation et la configuration des ports dans le menu iDryer.

## CAO

[Télécharger le boîtier](../../CAD/link-case.stp)
