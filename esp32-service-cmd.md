pio run -e esp32-c3-prod -t erase --upload-port /dev/cu.usbmodem114201
esptool.py --chip esp32c3 --port /dev/cu.usbmodem114201 --baud 460800 erase_flash
python3 ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32c3 --port /dev/cu.usbmodem114201 --baud 460800 erase_flash
pio run -e esp32c3-super-mini-prod -t erase 

scp -r /Users/ruslanpavlucenko/Projects/iDryerPortal/flasher-portal/* \root@82.146.63.133:/var/www/flasher-portal/