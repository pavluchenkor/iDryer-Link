#!/usr/bin/env python3
import argparse
import struct
import sys
import time

import serial

SOF = 0xAA
VERSION = 1
KIND_HELLO = 0x01
KIND_TELEMETRY = 0x10
FLAG_ACK_REQUIRED = 0x01


def crc16(data: bytes) -> int:
  crc = 0xFFFF
  for byte in data:
    crc ^= byte << 8
    for _ in range(8):
      if crc & 0x8000:
        crc = ((crc << 1) & 0xFFFF) ^ 0x1021
      else:
        crc = (crc << 1) & 0xFFFF
  return crc


def send_frame(ser: serial.Serial, kind: int, payload: bytes, seq: int,
               flags: int = 0) -> int:
  header = bytearray([SOF, VERSION, flags, kind, seq & 0xFF, len(payload)])
  body = bytes(header) + payload
  crc = crc16(body)
  frame = body + bytes([crc & 0xFF, (crc >> 8) & 0xFF])
  ser.write(frame)
  return (seq + 1) & 0xFF


def main():
  parser = argparse.ArgumentParser(
      description="Эмулятор RP2040 для UART-протокола idryer-link")
  parser.add_argument("--port", default="/dev/cu.usbserial-130")
  parser.add_argument("--baud", type=int, default=115200)
  parser.add_argument("--session", type=int, default=60,
                      help="длительность сессии чтения лога, секунд")
  args = parser.parse_args()

  ser = serial.Serial()
  ser.port = args.port
  ser.baudrate = args.baud
  ser.timeout = 0.05
  ser.dtr = False
  ser.rts = False
  ser.open()
  print(f"[HOST] UART открыт {args.port} @ {args.baud}")

  seq = 0
  start = time.time()
  hello_sent = False
  telemetry_sent = False
  while time.time() - start < args.session:
    data = ser.read(256)
    if data:
      sys.stdout.write(data.decode("utf-8", errors="replace"))
      sys.stdout.flush()
    elapsed = time.time() - start
    if not hello_sent and elapsed > 10:
      payload = struct.pack("<BII", 0x01, 0x000700, 0x00000007)
      seq = send_frame(ser, KIND_HELLO, payload, seq)
      hello_sent = True
      print(f"[HOST] -> HELLO seq={seq}")
    if hello_sent and not telemetry_sent and elapsed > 25:
      telemetry = struct.pack(
          "<hBBBHB BHI I",
          int(340),
          45,
          70,
          1,
          1100,
          2,
          0,
          40,
          505050,
          123456)
      seq = send_frame(ser,
                       KIND_TELEMETRY,
                       telemetry,
                       seq,
                       FLAG_ACK_REQUIRED)
      telemetry_sent = True
      print(f"[HOST] -> TELEMETRY seq={seq}")

  ser.close()
  print("[HOST] UART закрыт")


if __name__ == "__main__":
  main()
