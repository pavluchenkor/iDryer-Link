pio run -e esp32-c3-prod -t erase --upload-port /dev/cu.usbmodem114201
esptool.py --chip esp32c3 --port /dev/cu.usbmodem114201 --baud 460800 erase_flash