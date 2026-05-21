# iDryer Link

**iDryer Link** is a connectivity module for iDryer. It connects to the controller through the RJ45 port and brings the device online: after Wi-Fi setup, the dryer can work with the iDryer portal and mobile app.

The RJ45 port is used as a power and UART connector here. **It is not a network port**: do not connect Link to a switch or router.

![iDryer Link](docs/img/link2.png)

## What Link Does

- Connects iDryer to the internet over Wi-Fi.
- Links the device to [portal.idryer.org](https://portal.idryer.org).
- Works with the iDryer mobile app.
- Supports firmware installation through the web flasher.
- Provides open wiring diagrams and a CAD case file.

## App And Portal

- [iDryer on the App Store](https://apps.apple.com/app/idryer/id6760609044)
- [iDryer on Google Play](https://play.google.com/store/apps/details?id=org.idryer.mobile)
- [iDryer Portal](https://portal.idryer.org)
- [Web flasher](https://install.idryer.org)

![iDryer Portal dashboard](docs/img/portal1.png)

![iDryer Portal spool storage](docs/img/portal2.png)

## Quick Start

1. Assemble the RJ45 cable using the [wiring diagram in the guide](docs/README.en.md#how-to-connect-to-the-controller).
2. Connect the wires to the ESP32-C3 board.
3. Flash Link through [install.idryer.org](https://install.idryer.org), following the instructions on the site.

Full guide: [docs/README.en.md](docs/README.en.md)

## Diagrams And Files

![Link connection](docs/img/link1.png)

- [Russian guide](docs/README.ru.md)
- [English guide](docs/README.en.md)
- [RJ45 diagram](docs/img/RJ45.png)
- [Wiring diagram](docs/img/wiring.png)
- [ESP32-C3 Super Mini board](docs/img/esp32superMini.png)
- [ESP32-C3 Super Mini pinout](docs/img/ESP32-C3-Super-Mini-pinout-low.jpg)
- [ESP32-C3 Zero Waveshare pinout](docs/img/ESP32-C3-ZERO-Waveshare-pinout-low.jpg)
- [Case CAD file](docs/cad/link-case.stp)

## For Developers

Technical materials are kept in a separate section:

- [Repository notes](docs/developer/repository-workflow.md)
- [Post-build scripts](docs/developer/POST_BUILD_SCRIPTS.md)
- [Staging](docs/developer/STAGING.md)
- [Developer tools](docs/developer/TOOLS.md)
- [Documentation map](docs/guide/README.md)
