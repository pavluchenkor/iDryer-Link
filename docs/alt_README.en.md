# iDryer Link

Link is a communication module for iDryer. It connects to the RJ45 port of the controller and provides internet access to the dryer through the [portal.idryer.org](https://portal.idryer.org/) portal.

!!! note ""
    RJ45 is used here as a power and UART connector — not as a network interface. Do not connect Link to a network switch.

![Link](img/link2.png)
![Link](img/link1.png)

---

## Before connecting

!!! warning ""
    **Power off the controller** before connecting Link.

---

## Assembling the RJ45 cable

1. Assemble the RJ45 cable according to the diagram:

    ![RJ45](./img/RJ45.png)

2. Connect the wires to the ESP32-C3 according to the diagram:

    ![ESP32-C3 Super Mini](./img/esp32superMini.png)

    ```
    UART_RX_PIN 6 (white-blue)
    UART_TX_PIN 7 (white-green)
    ```

    !!! note ""
        The location of pins 6 and 7 may vary depending on the board manufacturer. Refer to the pinout of your specific module.

**ESP32-C3 Super Mini pinout:**

![ESP32-C3 Super Mini pinout](img/ESP32-C3-Super-Mini-pinout-low.jpg)

**ESP32-C3 Zero pinout (Waveshare):**

![ESP32-C3 Zero pinout](img/ESP32-C3-ZERO-Waveshare-pinout-low.jpg)

**Wiring diagram:**

![Wiring](./img/wiring.png)

---

## Flashing via web flasher

1. Connect Link to a USB port on your computer.
2. Open [install.idryer.org](https://install.idryer.org/) and select **iDryer Link**.
3. Select the board:
   - `ESP32-C3 super-mini` — for production modules.
   - `ESP32-C3 DevKit` — for development boards.
4. Click **Connect**, select the serial port (`USB JTAG/serial` or `CH340`).
5. If flashing does not start — hold `BOOT` and briefly press `RST`.
6. Click **Install**. The flasher will handle everything automatically.
7. After 100%, the Improv wizard will open: enter your Wi-Fi SSID and password, wait for the "Connected" status.

---

## Claiming on the portal

1. Click **Start Claim**. The screen will show `PIN:123456` — valid for ~5 minutes.
2. Go to [portal.idryer.org](https://portal.idryer.org/) → "Add device" → enter the PIN.
3. After successful claiming:
   - Disconnect USB.
   - Connect Link to the controller via RJ45.
   - Power on the iDryer.
4. Successful connection — blue "breathing" LED on Link.

---

## Expected result

- The controller detects Link immediately after power-on.
- The device appears online on the portal within 1–2 minutes.

If the device does not appear — check:
- Cable pinout against `img/RJ45.png`.
- Crimp quality of the connector.
- Port configuration in the iDryer menu (`GLOBAL → PORT CONFIG` → the relevant port must be set to `LNK`).

---

## CAD

[Download Link enclosure](cad/link-case.stp)
