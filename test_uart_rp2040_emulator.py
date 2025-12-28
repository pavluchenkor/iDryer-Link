#!/usr/bin/env python3
"""
RP2040 UART Emulator для тестирования ESP32 iDryer Link
Отправляет различные типы сообщений по UART
"""

import serial
import struct
import time
import sys

# Константы протокола
SOF = 0xAA
PROTOCOL_VERSION = 0x01

# MessageKind
MSG_HELLO = 0x01
MSG_TELEMETRY = 0x10
MSG_WEIGHTS = 0x12
MSG_STATUS = 0x13
MSG_RFID = 0x14
MSG_HEARTBEAT = 0x40
MSG_CLAIM_START = 0x70
MSG_CLAIM_STATUS = 0x71
MSG_CLAIM_COMPLETE = 0x72

# Флаги
FLAG_ACK_REQUIRED = 0x01

# CRC16-CCITT
def crc16_ccitt(data):
    """Вычисление CRC16-CCITT (0xFFFF начальное)"""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc = crc << 1
            crc &= 0xFFFF
    return crc

def send_frame(ser, kind, payload, flags=0, sequence=0):
    """Отправка фрейма по UART"""
    payload_len = len(payload)

    # Заголовок: SOF + version + flags + kind + sequence + length
    header = struct.pack('BBBBBB', SOF, PROTOCOL_VERSION, flags, kind, sequence, payload_len)

    # Вычисляем CRC от header + payload
    crc_data = header + payload
    crc = crc16_ccitt(crc_data)

    # Собираем фрейм
    frame = header + payload + struct.pack('<H', crc)

    # Отправляем
    ser.write(frame)
    print(f"[SEND] Kind=0x{kind:02X}, Seq={sequence}, Len={payload_len}, CRC=0x{crc:04X}")

    # Ждем ответ от ESP
    time.sleep(0.1)
    if ser.in_waiting:
        print(f"[RECV] {ser.in_waiting} bytes from ESP")
        data = ser.read(ser.in_waiting)
        print("  " + " ".join(f"{b:02X}" for b in data))

        # Декодируем ClaimStatus (0x71) и ClaimComplete (0x72)
        if len(data) >= 6:
            msg_kind = data[3]
            if msg_kind == MSG_CLAIM_STATUS and len(data) >= 51:  # 6 header + 45 payload + 2 CRC
                status = data[6]
                pin = data[7:16].decode('utf-8', errors='ignore').rstrip('\x00')
                expires_at = struct.unpack('<I', data[16:20])[0]
                remaining = struct.unpack('<I', data[20:24])[0]
                status_names = {0: "IDLE", 1: "PROVISIONING", 2: "WAITING_CLAIM", 3: "CLAIMED", 4: "ERROR"}
                print(f"  → ClaimStatus: status={status_names.get(status, status)} PIN=\"{pin}\" expires_in={remaining}s")
            elif msg_kind == MSG_CLAIM_COMPLETE and len(data) >= 44:  # 6 header + 38 payload + 2 CRC
                success = data[6]
                device_id = data[7:43].decode('utf-8', errors='ignore').rstrip('\x00')
                print(f"  → ClaimComplete: success={success} deviceId=\"{device_id}\"")

    return sequence + 1

def send_hello(ser, sequence):
    """Hello от RP2040"""
    # HelloPayload: role(1) + firmwareVersion(4) + workTimeCounter(4) + hardwareVersion[8]
    payload = struct.pack('<BII8s',
        0x01,  # Role::RpController
        0x010203,  # version 1.2.3
        1234,  # uptime
        b'v1.0\x00\x00\x00\x00'
    )
    return send_frame(ser, MSG_HELLO, payload, flags=0, sequence=sequence)

def send_telemetry(ser, sequence, count=2):
    """Telemetry от RP2040"""
    # TelemetryPayload: count(1) + array[4] TelemetryEntry
    # TelemetryEntry: unitId(1) + temperatureC10(2) + humidityPct(1) + heaterPowerPct(1) + fanOn(1)

    entries = []
    for i in range(count):
        # unitId, tempC10, humidity, heaterPower, fanOn
        entries.append(struct.pack('<BhBBB',
            i,              # unitId
            250 + i*10,     # temperatureC10 (25.0°C)
            45 + i*5,       # humidityPct (45%)
            85 + i*5,       # heaterPowerPct (85%)
            1               # fanOn (true)
        ))

    # Дополняем до 4 записей нулями
    while len(entries) < 4:
        entries.append(struct.pack('<BhBBB', 0, 0, 0, 0, 0))

    payload = struct.pack('<B', count) + b''.join(entries)
    return send_frame(ser, MSG_TELEMETRY, payload, flags=FLAG_ACK_REQUIRED, sequence=sequence)

def send_weights(ser, sequence, count=2):
    """Weights от RP2040"""
    # WeightsPayload: count(1) + array[4] WeightEntry
    # WeightEntry: sensorId(1) + unitId(1) + weightGrams(2)

    entries = []
    for i in range(count):
        entries.append(struct.pack('<BBH',
            i,              # sensorId
            i,              # unitId
            1000 + i*100    # weightGrams (1000g, 1100g...)
        ))

    while len(entries) < 4:
        entries.append(struct.pack('<BBH', 0, 0, 0))

    payload = struct.pack('<B', count) + b''.join(entries)
    return send_frame(ser, MSG_WEIGHTS, payload, flags=0, sequence=sequence)

def send_status(ser, sequence, count=2):
    """Status от RP2040"""
    # StatusPayload: count(1) + array[4] StatusEntry + uptime(4)
    # StatusEntry: unitId(1) + mode(1) + sessionNum(4) + targetTempC10(2) + targetHumidityPct(2) + durationMinutes(2) +
    #              elapsedSeconds(4) + stageElapsedSeconds(4) + stageRemainingSeconds(4) + totalRemainingSeconds(4) + currentStage(1) + totalStages(1)

    entries = []
    for i in range(count):
        entry = struct.pack('<BBIHHH IIII BB',
            i,      # unitId
            1,      # mode (DRYING)
            100+i,  # sessionNum
            250,    # targetTempC10 (25.0°C)
            45,     # targetHumidityPct
            120,    # durationMinutes
            300,    # elapsedSeconds
            100,    # stageElapsedSeconds
            200,    # stageRemainingSeconds
            600,    # totalRemainingSeconds
            0,      # currentStage
            0       # totalStages
        )
        entries.append(entry)

    while len(entries) < 4:
        entries.append(b'\x00' * 30)  # 30 байт на запись (StatusEntry)

    payload = struct.pack('<B', count) + b''.join(entries) + struct.pack('<I', 5000)  # uptime
    return send_frame(ser, MSG_STATUS, payload, flags=0, sequence=sequence)

def send_rfid(ser, sequence):
    """RFID Event от RP2040"""
    # RfidPayload: event(1) + readerId(1) + tag[32] + unitId(1) + pad[2]
    payload = struct.pack('<BB32sB2s',
        1,      # event (TagDetected)
        0,      # readerId
        b'DEADBEEF12345678\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00',  # tag
        0,      # unitId
        b'\x00\x00'  # padding
    )
    return send_frame(ser, MSG_RFID, payload, flags=0, sequence=sequence)

def send_heartbeat(ser, sequence):
    """Heartbeat от RP2040"""
    # HeartbeatPayload: uptimeSeconds(4) + wifiRssiDbm(2) + errorsSinceBoot(2)
    payload = struct.pack('<IhH',
        1234,   # uptimeSeconds
        -50,    # rssi (температура MCU для RP2040)
        0       # errors
    )
    return send_frame(ser, MSG_HEARTBEAT, payload, flags=0, sequence=sequence)

def send_claim_start(ser, sequence):
    """ClaimStart от RP2040 - запрос начала claiming процесса"""
    # ClaimStart имеет пустой payload
    return send_frame(ser, MSG_CLAIM_START, b'', flags=FLAG_ACK_REQUIRED, sequence=sequence)

def main_menu():
    """Главное меню"""
    print("\n" + "="*50)
    print("RP2040 UART Emulator - Test Menu")
    print("="*50)
    print("1. Send Hello")
    print("2. Send Telemetry (with ACK)")
    print("3. Send Weights")
    print("4. Send Status")
    print("5. Send RFID Event")
    print("6. Send Heartbeat")
    print("7. Send All (sequence)")
    print("8. Send ClaimStart (request claiming)")
    print("0. Exit")
    print("="*50)

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 test_uart_rp2040_emulator.py <SERIAL_PORT>")
        print("Example: python3 test_uart_rp2040_emulator.py /dev/ttyUSB0")
        sys.exit(1)

    port = sys.argv[1]

    print(f"[INFO] Connecting to {port} at 115200 baud...")
    try:
        ser = serial.Serial(port, 115200, timeout=1)
        print("[INFO] Connected!")
    except Exception as e:
        print(f"[ERROR] Failed to open serial port: {e}")
        sys.exit(1)

    sequence = 1

    try:
        while True:
            main_menu()
            choice = input("Select option: ").strip()

            if choice == '0':
                break
            elif choice == '1':
                sequence = send_hello(ser, sequence)
            elif choice == '2':
                sequence = send_telemetry(ser, sequence, count=2)
            elif choice == '3':
                sequence = send_weights(ser, sequence, count=2)
            elif choice == '4':
                sequence = send_status(ser, sequence, count=2)
            elif choice == '5':
                sequence = send_rfid(ser, sequence)
            elif choice == '6':
                sequence = send_heartbeat(ser, sequence)
            elif choice == '7':
                print("[INFO] Sending all messages...")
                sequence = send_hello(ser, sequence)
                time.sleep(0.1)
                sequence = send_telemetry(ser, sequence, count=2)
                time.sleep(0.1)
                sequence = send_weights(ser, sequence, count=2)
                time.sleep(0.1)
                sequence = send_status(ser, sequence, count=2)
                time.sleep(0.1)
                sequence = send_rfid(ser, sequence)
                time.sleep(0.1)
                sequence = send_heartbeat(ser, sequence)
            elif choice == '8':
                sequence = send_claim_start(ser, sequence)
                print("[INFO] ClaimStart sent. Watch ESP32 for ClaimStatus with PIN...")
            else:
                print("[WARN] Invalid option")

            time.sleep(0.2)

    except KeyboardInterrupt:
        print("\n[INFO] Interrupted by user")
    finally:
        ser.close()
        print("[INFO] Serial port closed")

if __name__ == '__main__':
    main()
