# iDryer Link — quick guide

Link is a connectivity module for iDryer. It plugs into the controller’s Ethernet port and talks to the main board over power + UART (the port is used as a connector, not as a network jack). Link brings the dryer online and connects it to portal.idryer.org.

## How to connect to the controller
1) Power off the controller.  
2) Crimp the RJ45 cable, follow ![RJ45](./img/RJ45.png) to keep the pairs in the right order. Important: this RJ45 is only power/UART, do not plug it into a network switch.  
3) Wire the leads to the esp32-C3 Super Mini per ![esp32superMini](./img/esp32superMini.png).  
4) Cable pinout reference: ![wiring](./img/wiring.png)  
<!-- 5) Plug Link into the controller’s Ethernet port. After power-on the controller treats Link as an external modem. -->

## How to flash via the web flasher
The web flasher lives at https://install.idryer.org/

- Connect Link to the computer via USB.
- Open the [page](https://install.idryer.org/) and choose **iDryer Link**.
- In the board list pick:
   - `ESP32-C3 super-mini` — main option for production modules.
   - `ESP32-C3 DevKit` — if you have a dev board.
- Click **Connect**, choose the serial port (usually `USB JTAG/serial` or `CH340`). If flashing does not start, hold `BOOT` and briefly tap `RST`.
- Click **Install**. The flasher will write three files (bootloader.bin, partitions.bin, firmware.bin) at the offsets from the manifest.
- After 100% the Improv wizard opens: enter Wi‑Fi SSID/password, wait for “Connected”.
- If the Improv wizard didn’t appear, unplug USB and reconnect, choose **Connect** without re-flashing.
- Click **Start Claim**. You will see a line `PIN:123456` — that’s the PIN, valid for ~5 minutes.
- Go to https://portal.idryer.org → “Add device” → enter the PIN. After successful claim the device appears in the list.
- Unplug USB and connect Link to the controller via RJ45.
- Power up the iDryer.
- Successful cloud connection is indicated by a blue “breathing” LED pattern.

## Expected result
- The controller sees Link right after power-on.
- In the web portal the device comes online within 1–2 minutes after Wi‑Fi connection.
- If it does not show up: recheck the pinout in `img/RJ45.png`, crimp quality, the selected board during flashing, and the UART port configuration in the iDryer menu.
