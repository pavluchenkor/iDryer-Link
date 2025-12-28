#!/usr/bin/env python3
import serial
import sys
import time

port = '/dev/cu.usbserial-2110'
baud = 115200

try:
    ser = serial.Serial(port, baud, timeout=0.1)
    print(f"[INFO] Connected to {port} at {baud} baud", file=sys.stderr)

    # Сбрасываем ESP32 через DTR/RTS
    print("[INFO] Resetting ESP32...", file=sys.stderr)
    ser.setDTR(False)
    ser.setRTS(True)
    time.sleep(0.1)
    ser.setRTS(False)
    time.sleep(0.5)

    print("[INFO] Reading serial output...", file=sys.stderr)

    # Читаем первые 60 секунд
    start_time = time.time()
    while time.time() - start_time < 60:
        if ser.in_waiting:
            try:
                line = ser.readline().decode('utf-8', errors='replace').rstrip()
                if line:
                    print(line)
                    sys.stdout.flush()
            except UnicodeDecodeError:
                pass
        time.sleep(0.01)

    ser.close()
    print("[INFO] Done reading", file=sys.stderr)
except Exception as e:
    print(f"[ERROR] {e}", file=sys.stderr)
    sys.exit(1)
