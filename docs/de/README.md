# iDryer Link — Kurzanleitung

Link ist ein Kommunikationsmodul für iDryer. Es wird in den Ethernet-Anschluss des Controllers eingesteckt und kommuniziert über diesen mit der Steuerplatine (der Anschluss wird als Stromversorgung und UART verwendet, nicht als Netzwerk). Link verbindet den Trockner mit dem Internet und bindet ihn an [portal.idryer.org](https://portal.idryer.org/) an.

![link1](../img/link2.png)
![link1](../img/link1.png)

## Anschluss an den Controller
**Schalten Sie die Stromversorgung des Controllers aus**

**Fertigen Sie das RJ45-Kabel an**, orientieren Sie sich an
![RJ45](../img/RJ45.png)
um die Paare nicht zu verwechseln. Wichtig: RJ45 ist hier nur ein Stromanschluss/UART-Stecker, verbinden Sie ihn nicht mit einem Netzwerk-Switch.

**Verbinden Sie die Drähte** mit dem ESP32-C3 Super Mini nach dem Schaltplan
![esp32superMini](../img/esp32superMini.png)

**Je nach Typ und Hersteller der Platine können die Positionen der Kontakte 6 und 7 unterschiedlich sein.**
Überprüfen Sie das Pinout Ihrer Platine, das vom Hersteller bereitgestellt wurde.

```
UART_RX_PIN 6 (weiß-blau)
UART_TX_PIN 7 (weiß-grün)
```

**Pinout ESP32-C3 Super Mini**

![Pinout ESP32-C3 Super Mini](../img/ESP32-C3-Super-Mini-pinout-low.jpg)

**Pinout ESP32-C3 Zero (Waveshare)**

![Pinout ESP32-C3 Super Mini](../img/ESP32-C3-ZERO-Waveshare-pinout-low.jpg)

Auf die gleiche Weise können Sie jede beliebige Entwicklerplatine anschließen, indem Sie sich am Pinout orientieren.


**Verdrahtung des Kabels**

 ![wiring](../img/wiring.png)
<!-- 5) Verbinden Sie Link mit dem Ethernet-Anschluss des Controllers. Nach dem Einschalten der Stromversorgung wird der Controller mit Link wie mit einem externen Modem arbeiten. -->

## Programmierung über Web-Flasher
Der Web-Flasher befindet sich unter https://install.idryer.org/

- Verbinden Sie Link mit dem USB-Anschluss Ihres Computers.
- Öffnen Sie die [Seite](https://install.idryer.org/) und wählen Sie das Gerät **iDryer Link** aus.
- Wählen Sie die Platine aus:
   - `ESP32-C3 super-mini` — die Hauptvariante für Serienmodule.
   - `ESP32-C3 DevKit` — wenn Sie eine Entwicklerplatine haben.
- Klicken Sie auf **Connect**, wählen Sie den seriellen Anschluss aus (normalerweise `USB JTAG/serial` oder `CH340`). Wenn das Flashing nicht startet, halten Sie `BOOT` auf der Platine gedrückt und drücken Sie kurz `RST`.
- Klicken Sie auf **Install**. Der Flasher programmiert alles Notwendige automatisch.
- Nach 100% wird der Improv-Assistent geöffnet: Geben Sie SSID und Wi-Fi-Passwort ein, warten Sie auf den Status „Connected".
- Wenn der Improv-Assistent nicht geöffnet wurde, trennen Sie USB ab und wiederholen Sie die Verbindung, indem Sie **Connect** auswählen, ohne erneut zu programmieren.

## Verbindung mit dem Portal

- Klicken Sie auf die Schaltfläche **Start Claim**. Es erscheint eine Zeichenkette `PIN:123456` — dies ist die PIN mit einer Gültigkeitsdauer von etwa 5 Minuten.
- Rufen Sie https://portal.idryer.org → „Gerät hinzufügen" → auf und geben Sie die PIN ein. Nach erfolgreicher Bindung wird das Gerät in der Liste angezeigt.
- Trennen Sie USB ab und verbinden Sie Link über RJ45 mit dem Controller.
- Schalten Sie die Stromversorgung des iDryer ein
- Erfolgreiche Verbindung wird durch eine blaue LED-Anzeige „atmen" signalisiert

## Was Sie erhalten sollten
- Der Controller erkennt Link sofort nach dem Einschalten.
- Im Web-Portal wird das Gerät innerhalb von 1–2 Minuten nach der Verbindung mit Wi-Fi online angezeigt.
- Wenn der Status nicht angezeigt wird: überprüfen Sie noch einmal das Pinout nach `../../img/RJ45.png`, die Qualität der Konfektionierung und die richtige Auswahl der Platine im Programmierungsschritt sowie die Konfiguration der Anschlüsse mit dem iDryer-Menü.

## CAD

[Gehäuse herunterladen](../../CAD/link-case.stp)
