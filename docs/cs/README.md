# iDryer Link — krátký návod

Link je komunikační modul pro iDryer. Připojuje se do portu Ethernet ovladače a prostřednictvím něj komunikuje se řídící deskou (port se používá jako konektor s napájením a UART, nikoli jako síťový). Link přináší sušičku na internet a propojuje ji s [portal.idryer.org](https://portal.idryer.org/).

![link1](../img/link2.png)
![link1](../img/link1.png)

## Jak připojit k ovladači
**Vypněte napájení ovladače**

**Sestavte kabel RJ45**, zkontrolujte podle
![RJ45](../img/RJ45.png)
aby se páry nespletly. Důležité: RJ45 zde je pouze konektor napájení/UART, nepřipojujte jej do síťového přepínače.

**Propojte vodiče** s ESP32-C3 Super mini podle schématu
![esp32superMini](../img/esp32superMini.png)

**V závislosti na typu a výrobci desky se umístění kontaktů 6 a 7 může lišit.**
Zkontrolujte pinout vaší desky poskytnutý výrobcem.

```
UART_RX_PIN 6 (bílo-modrý)
UART_TX_PIN 7 (bílo-zelený)
```

**Pinout ESP32-C3 super mini**

![Pinout ESP32-C3 super mini](../img/ESP32-C3-Super-Mini-pinout-low.jpg)

**Pinout ESP32-C3 Zero (Waveshare)**

![Pinout ESP32-C3 super mini](../img/ESP32-C3-ZERO-Waveshare-pinout-low.jpg)

Podobně lze připojit jakoukoli vývojovou desku, pokud se budete řídit pinoutem.


**Schéma zapojení kabelu**

 ![wiring](../img/wiring.png)
<!-- 5) Připojte Link do portu Ethernet ovladače. Po zapnutí napájení bude ovladač pracovat s Link jako s externím modemem. -->

## Jak flashovat přes webový flasher
Webový flasher je na https://install.idryer.org/

- Připojte Link k USB portu počítače.
- Otevřete [stránku](https://install.idryer.org/) a vyberte zařízení **iDryer Link**.
- Vyberte desku:
   - `ESP32-C3 super-mini` — hlavní varianta pro sériové moduly.
   - `ESP32-C3 DevKit` — pokud máte vývojovou desku.
- Klikněte na **Connect**, vyberte sériový port (obvykle `USB JTAG/serial` nebo `CH340`). Pokud se firmware nespustí, podržte `BOOT` na desce a krátce stiskněte `RST`.
- Klikněte na **Install**. Flasher automaticky flashuje vše potřebné.
- Po 100% se otevře průvodce Improv: zadejte SSID a heslo Wi-Fi, počkejte na stav "Connected".
- Pokud se průvodce Improv neotevřel, odpojte USB a znovu jej připojte, přičemž zvolte **Connect** bez opakovaného flashování.

## Připojení k portálu

- Klikněte na tlačítko **Start Claim**. Zobrazí se řetězec `PIN:12345678` — toto je PIN, platí 10 minut.
- Přejděte na https://portal.idryer.org → „Přidat zařízení" → zadejte PIN. Po úspěšné vazbě se zařízení objeví v seznamu.
- Odpojte USB a připojte Link k ovladači prostřednictvím RJ45.
- Zapněte napájení iDryer
- o úspěšném připojení bude informovat LED indikace modrým „dechem"

## Co byste měli získat
- Ovladač vidí Link ihned po připojení napájení.
- V webovém portálu se zařízení objeví online během 1–2 minut po připojení k Wi-Fi.
- Pokud se stav nezobrazí: znovu zkontrolujte rozpinování podle `../../img/RJ45.png`, kvalitu krimpování a správnost výběru desky v kroku flashování a konfiguraci portů z nabídky iDryer.

## CAD

[Stáhnout pouzdro](../../CAD/link-case.stp)
