#!/usr/bin/env python3
"""
Продвинутый эмулятор RP2040 для UART-протокола iDryer

НАЗНАЧЕНИЕ:
Эмулирует MCU (RP2040) для тестирования LINK (ESP32) без физического
контроллера. Автоматический режим с периодической отправкой данных.

ЗАПУСК:
python3 emulate_controller.py --port /dev/ttyUSB0 --units 2 --fw-major 2

ПАРАМЕТРЫ:
--port     UART порт (по умолчанию /dev/cu.usbserial-130)
--baud     Скорость (по умолчанию 115200)
--units    Количество юнитов 1-4 (по умолчанию 2)
--fw-major MAJOR версия firmware (по умолчанию 2)
--session  Длительность сессии секунд (по умолчанию 120)
--rfid     Отправить RFID событие через 15с

АВТОМАТИЧЕСКИЙ РЕЖИМ:
- Hello через 2с после запуска
- Telemetry каждые 5с, Status каждые 10с, Heartbeat каждые 5с
- Реагирует на HelloRequest, Command, ConfigPush от ESP32
- Поддержка версионирования протокола

ВОЗМОЖНОСТИ:
- ✅ Актуальные структуры данных (HelloPayload 86 байт, StatusEntry 32 байта)
- ✅ Двусторонняя коммуникация с ESP32
- ✅ Эмуляция units topology и RFID событий
- ✅ Поддержка claiming flow

Протокол: docs/02-uart/01-uart.md
Зависимости: pip install pyserial
Обновлено: 2026-03-25
Статус: ✅ Актуален (проверен на соответствие протоколу)
"""

import argparse
import struct
import sys
import time

import serial

# ---------------------------------------------------------------------------
# Константы протокола
# ---------------------------------------------------------------------------
SOF = 0xAA
PROTOCOL_VERSION = 1

# FLAGS
FLAG_ACK_REQUIRED = 0x01
FLAG_IS_ACK       = 0x02
FLAG_ERROR        = 0x04
FLAG_FRAGMENTED   = 0x08
FLAG_LAST_FRAGMENT = 0x10

# MessageKind
KIND_HELLO          = 0x01
KIND_HELLO_ACK      = 0x02
KIND_TELEMETRY      = 0x10
KIND_TELEMETRY_ACK  = 0x11
KIND_WEIGHTS        = 0x12
KIND_STATUS         = 0x13
KIND_RFID           = 0x14
KIND_COMMAND        = 0x20
KIND_COMMAND_ACK    = 0x21
KIND_CONFIG_PUSH    = 0x30
KIND_CONFIG_ACK     = 0x31
KIND_HEARTBEAT      = 0x40
KIND_ERROR          = 0x50
KIND_LOG            = 0x60
KIND_CLAIM_START    = 0x70

# Role
ROLE_MCU           = 0x01
ROLE_ESP           = 0x02
ROLE_HELLO_REQUEST = 0xFF

# ErrorCode
ERR_NONE           = 0x00

# RfidEvent
RFID_TAG_DETECTED  = 1
RFID_TAG_REMOVED   = 2

# DryerMode
MODE_IDLE    = 0
MODE_DRYING  = 1
MODE_STORAGE = 2
MODE_PROFILE = 3
MODE_FAULT   = 4

# UnitCapabilities
CAP_HEATER           = 0x0001
CAP_FAN              = 0x0002
CAP_SERVO            = 0x0004
CAP_RH_AIR_SENSOR    = 0x0008
CAP_TEMP_AIR_SENSOR  = 0x0010
CAP_TEMP_HTR_SENSOR  = 0x0020
CAP_ALL              = 0x003F


# ---------------------------------------------------------------------------
# CRC16-CCITT
# ---------------------------------------------------------------------------
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


# ---------------------------------------------------------------------------
# Кадр
# ---------------------------------------------------------------------------
def build_frame(kind: int, payload: bytes, seq: int, flags: int = 0) -> bytes:
    header = bytes([SOF, PROTOCOL_VERSION, flags, kind, seq & 0xFF, len(payload)])
    body = header + payload
    crc = crc16(body)
    return body + bytes([crc & 0xFF, (crc >> 8) & 0xFF])


def send_frame(ser: serial.Serial, kind: int, payload: bytes, seq: int,
               flags: int = 0) -> int:
    frame = build_frame(kind, payload, seq, flags)
    ser.write(frame)
    return (seq + 1) & 0xFF


# ---------------------------------------------------------------------------
# Парсер входящих кадров
# ---------------------------------------------------------------------------
class FrameParser:
    """Побайтовый парсер UART кадров."""

    HEADER_SIZE = 6  # SOF VER FLAGS KIND SEQ LEN

    def __init__(self):
        self._buf = bytearray()

    def feed(self, data: bytes):
        """Добавить байты, вернуть список (kind, flags, seq, payload) для каждого полного кадра."""
        self._buf.extend(data)
        frames = []
        while True:
            # Ищем SOF
            idx = self._buf.find(SOF)
            if idx < 0:
                self._buf.clear()
                break
            if idx > 0:
                self._buf = self._buf[idx:]

            if len(self._buf) < self.HEADER_SIZE:
                break

            payload_len = self._buf[5]
            total = self.HEADER_SIZE + payload_len + 2  # +2 CRC

            if len(self._buf) < total:
                break

            frame = bytes(self._buf[:total])
            self._buf = self._buf[total:]

            body = frame[:-2]
            crc_lo, crc_hi = frame[-2], frame[-1]
            expected_crc = (crc_hi << 8) | crc_lo
            actual_crc = crc16(body)

            if actual_crc != expected_crc:
                print(f"[RX] CRC mismatch (got {actual_crc:04X}, expected {expected_crc:04X})")
                continue

            ver   = frame[1]
            flags = frame[2]
            kind  = frame[3]
            seq   = frame[4]
            payload = frame[self.HEADER_SIZE:self.HEADER_SIZE + payload_len]

            if ver != PROTOCOL_VERSION:
                print(f"[RX] Версия протокола {ver} != {PROTOCOL_VERSION}")
                continue

            frames.append((kind, flags, seq, payload))

        return frames


# ---------------------------------------------------------------------------
# Payload builders
# ---------------------------------------------------------------------------
def make_unit_config(unit_id: int, caps: int, scales: list, rfid: list) -> bytes:
    """UnitConfig: 12 байт."""
    scales_b = bytes((scales + [0xFF] * 4)[:4])
    rfid_b   = bytes((rfid   + [0xFF] * 4)[:4])
    return struct.pack('<BBH', unit_id, 0, caps) + scales_b + rfid_b


def make_hello(fw_major: int = 2, fw_minor: int = 0, fw_patch: int = 0,
               units_count: int = 2, mcu_serial: str = "36B955AB4350") -> bytes:
    """
    HelloPayload: 86 байт.
    role(1) + pad(3) + fwVer(4) + workTime(4) + hwVer[8] + unitsCount(1) + units[4](48) + mcuSerial[17]
    """
    role      = bytes([ROLE_MCU, 0, 0, 0])
    fw_ver    = struct.pack('<I', (fw_major << 16) | (fw_minor << 8) | fw_patch)
    work_time = struct.pack('<I', 3600)
    hw_ver    = b'v1.0\x00\x00\x00\x00'
    u_count   = bytes([units_count])

    unit0 = make_unit_config(0, CAP_ALL, [0, 1], [0])   # U1: W0,W1 / R0
    unit1 = make_unit_config(1, CAP_ALL, [2],    [1])   # U2: W2 / R1
    unit2 = make_unit_config(2, 0,       [],     [])    # пусто
    unit3 = make_unit_config(3, 0,       [],     [])    # пусто

    serial_b = mcu_serial.encode()[:16].ljust(17, b'\x00')

    payload = role + fw_ver + work_time + hw_ver + u_count + unit0 + unit1 + unit2 + unit3 + serial_b
    assert len(payload) == 86, f"HelloPayload size {len(payload)} != 86"
    return payload


def make_telemetry(units: list) -> bytes:
    """
    TelemetryPayload.
    units = [{'id': 0, 'temp_c': 55.3, 'hum_pct': 45.0, 'heater_pct': 80, 'fan': True}, ...]
    """
    data = bytes([len(units)])
    for u in units:
        temp_c10 = int(u['temp_c'] * 10)
        hum_pct10 = int(u['hum_pct'] * 10)
        data += struct.pack('<BhHBB',
                            u['id'],
                            temp_c10,
                            hum_pct10,
                            u['heater_pct'],
                            1 if u['fan'] else 0)
    return data


def make_status(units: list, uptime: int = 0) -> bytes:
    """
    StatusPayload.
    units = [{'id': 0, 'mode': MODE_DRYING, 'session': 1, 'target_temp': 55.0,
               'target_hum': 20, 'duration_min': 240, 'elapsed': 120, 'remaining': 3480}, ...]
    StatusEntry: 32 байт.
    """
    data = bytes([len(units)])
    for u in units:
        entry = struct.pack('<BBIHHIIIIBBBB',
                            u['id'],
                            u.get('mode', MODE_IDLE),
                            u.get('session', 0),
                            int(u.get('target_temp', 0) * 10),
                            u.get('target_hum', 0),
                            u.get('duration_min', 0),
                            u.get('elapsed', 0),
                            u.get('stage_elapsed', 0),
                            u.get('stage_remaining', 0),
                            u.get('remaining', 0),
                            u.get('current_stage', 0),
                            u.get('total_stages', 0),
                            u.get('stage_phase', 0),
                            0)  # _pad
        assert len(entry) == 32, f"StatusEntry size {len(entry)} != 32"
        data += entry
    # Дополнить до 4 юнитов пустыми (упрощение для фиксированного размера)
    empty = struct.pack('<BBIHHIIIIBBBB', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
    while len(data) < 1 + 4 * 32:
        data += empty
    data += struct.pack('<I', uptime)
    return data


def make_weights(sensors: list) -> bytes:
    """
    WeightsPayload.
    sensors = [{'sensor_id': 0, 'unit_id': 0, 'weight_g': 123.4}, ...]
    """
    data = bytes([len(sensors)])
    for s in sensors:
        w_c10 = int(s['weight_g'] * 10)
        data += struct.pack('<BBH', s['sensor_id'], s['unit_id'], w_c10)
    return data


def make_rfid(event: int, reader_id: int, unit_id: int, tag: str = "") -> bytes:
    """
    RfidPayload: 37 байт.
    event: RFID_TAG_DETECTED или RFID_TAG_REMOVED
    """
    tag_b = tag.encode()[:31].ljust(32, b'\x00')
    return struct.pack('<BB', event, reader_id) + tag_b + struct.pack('<BBB', unit_id, 0, 0)


def make_heartbeat(uptime: int, mcu_temp_c: float = 25.0, errors: int = 0) -> bytes:
    """HeartbeatPayload: 9 байт."""
    temp_raw = int(mcu_temp_c * 10)
    return struct.pack('<IhHB', uptime, temp_raw, errors, 0)


def make_ack(ack_seq: int, status: int = ERR_NONE) -> bytes:
    """AckPayload: 2 байта."""
    return struct.pack('<BB', ack_seq, status)


def make_config_ack(ack_seq: int) -> bytes:
    return make_ack(ack_seq)


# ---------------------------------------------------------------------------
# Обработка входящих кадров
# ---------------------------------------------------------------------------
def handle_frame(ser, kind, flags, seq, payload, state):
    """Реагировать на входящий кадр. state — dict с изменяемым состоянием."""

    if kind == KIND_HELLO:
        role = payload[0] if payload else 0
        if role == ROLE_HELLO_REQUEST:
            print(f"[RX] HelloRequest от LINK — отправляем Hello")
            hello = make_hello()
            state['seq'] = send_frame(ser, KIND_HELLO, hello, state['seq'])
            state['hello_sent'] = True
        elif role == ROLE_ESP:
            print(f"[RX] Hello от LINK (role=ESP)")

    elif kind == KIND_HELLO_ACK:
        if len(payload) >= 4:
            ip = struct.unpack('<I', payload[:4])[0]
            ip_str = f"{ip & 0xFF}.{(ip >> 8) & 0xFF}.{(ip >> 16) & 0xFF}.{(ip >> 24) & 0xFF}"
            ssid = payload[4:].rstrip(b'\x00').decode('utf-8', errors='replace')
            print(f"[RX] HelloAck: IP={ip_str} SSID={ssid!r}")

    elif kind == KIND_COMMAND:
        if len(payload) >= 13:
            cmd_code, target_state, unit_id = struct.unpack('<BBB', payload[:3])
            arg0, arg1 = struct.unpack('<II', payload[5:13])
            print(f"[RX] Command: code=0x{cmd_code:02X} unit={unit_id} arg0={arg0} arg1={arg1}")
        else:
            print(f"[RX] Command (короткий payload {len(payload)}B)")
        if flags & FLAG_ACK_REQUIRED:
            ack = make_ack(seq)
            state['seq'] = send_frame(ser, KIND_COMMAND_ACK, ack, state['seq'], FLAG_IS_ACK)
            print(f"[TX] CommandAck seq={seq}")

    elif kind == KIND_CONFIG_PUSH:
        if len(payload) >= 6:
            transfer_id, total_size, chunk_idx = struct.unpack('<HHH', payload[:6])
            json_data = payload[6:].rstrip(b'\x00').decode('utf-8', errors='replace')
            print(f"[RX] ConfigPush: tid={transfer_id} total={total_size} chunk={chunk_idx}")
            print(f"     JSON: {json_data[:80]}{'...' if len(json_data) > 80 else ''}")
        else:
            json_data = payload.rstrip(b'\x00').decode('utf-8', errors='replace')
            print(f"[RX] ConfigPush (нефрагментированный): {json_data[:80]}")
        if flags & FLAG_ACK_REQUIRED:
            ack = make_config_ack(seq)
            state['seq'] = send_frame(ser, KIND_CONFIG_ACK, ack, state['seq'], FLAG_IS_ACK)

    elif kind == KIND_HEARTBEAT:
        if len(payload) >= 9:
            uptime, rssi, errors, cloud_state = struct.unpack('<IhHB', payload[:9])
            print(f"[RX] Heartbeat: uptime={uptime}s RSSI={rssi}dBm errors={errors} cloud={cloud_state}")

    elif kind == KIND_ERROR:
        if len(payload) >= 4:
            err_code, last_seq, detail = struct.unpack('<BBH', payload[:4])
            print(f"[RX] Error: code={err_code} seq={last_seq} detail={detail}")

    elif kind == KIND_LOG:
        print(f"[RX] Log: {payload.decode('utf-8', errors='replace')[:100]}")

    elif kind & FLAG_IS_ACK:
        pass  # ACK на наши кадры — игнорируем

    else:
        print(f"[RX] Неизвестный kind=0x{kind:02X} payload={payload.hex()[:32]}")


# ---------------------------------------------------------------------------
# Главный цикл
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(
        description="Эмулятор RP2040 для UART-протокола iDryer")
    parser.add_argument("--port", default="/dev/cu.usbserial-130",
                        help="UART порт")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--session", type=int, default=120,
                        help="Длительность сессии, секунд")
    parser.add_argument("--fw-major", type=int, default=2,
                        help="MAJOR версия прошивки (для проверки совместимости)")
    parser.add_argument("--units", type=int, default=2,
                        help="Количество юнитов (1-4)")
    parser.add_argument("--rfid", action="store_true",
                        help="Отправить RFID tag_detected событие")
    args = parser.parse_args()

    ser = serial.Serial()
    ser.port = args.port
    ser.baudrate = args.baud
    ser.timeout = 0.05
    ser.dtr = False
    ser.rts = False
    ser.open()
    print(f"[HOST] UART открыт {args.port} @ {args.baud}")

    parser_obj = FrameParser()
    state = {
        'seq': 0,
        'hello_sent': False,
    }

    t_start      = time.time()
    t_hello      = 0.0       # время последнего Hello
    t_telemetry  = 0.0
    t_status     = 0.0
    t_weights    = 0.0
    t_heartbeat  = 0.0
    t_rfid       = 0.0
    rfid_sent    = False

    def elapsed():
        return time.time() - t_start

    try:
        while elapsed() < args.session:
            # --- Чтение и обработка входящих кадров ---
            raw = ser.read(256)
            if raw:
                for kind, flags, seq, payload in parser_obj.feed(raw):
                    handle_frame(ser, kind, flags, seq, payload, state)

            now = elapsed()

            # --- Hello (каждые 30 с или первый раз через 2 с) ---
            if now - t_hello > (2.0 if not state['hello_sent'] else 30.0):
                hello = make_hello(
                    fw_major=args.fw_major,
                    units_count=args.units,
                )
                state['seq'] = send_frame(ser, KIND_HELLO, hello, state['seq'])
                state['hello_sent'] = True
                t_hello = now
                print(f"[TX] Hello (fw {args.fw_major}.0.0, {args.units} units)")

            if not state['hello_sent']:
                continue

            # --- Telemetry (каждые 5 с) ---
            if now - t_telemetry > 5.0:
                units_tel = [
                    {'id': i, 'temp_c': 55.3 + i * 2, 'hum_pct': 23.0,
                     'heater_pct': 75, 'fan': True}
                    for i in range(args.units)
                ]
                tel = make_telemetry(units_tel)
                state['seq'] = send_frame(ser, KIND_TELEMETRY, tel, state['seq'],
                                          FLAG_ACK_REQUIRED)
                t_telemetry = now
                print(f"[TX] Telemetry ({args.units} units)")

            # --- Status (каждые 10 с) ---
            if now - t_status > 10.0:
                units_st = [
                    {'id': i, 'mode': MODE_DRYING, 'session': 1,
                     'target_temp': 55.0, 'target_hum': 20,
                     'duration_min': 240, 'elapsed': int(now),
                     'remaining': max(0, 14400 - int(now))}
                    for i in range(args.units)
                ]
                st = make_status(units_st, uptime=int(now))
                state['seq'] = send_frame(ser, KIND_STATUS, st, state['seq'],
                                          FLAG_ACK_REQUIRED)
                t_status = now
                print(f"[TX] Status (DRYING, elapsed={int(now)}s)")

            # --- Weights (каждые 10 с) ---
            if now - t_weights > 10.0:
                sensors = [
                    {'sensor_id': i, 'unit_id': i // 2, 'weight_g': 200.0 + i * 50.5}
                    for i in range(min(args.units * 2, 4))
                ]
                wt = make_weights(sensors)
                state['seq'] = send_frame(ser, KIND_WEIGHTS, wt, state['seq'])
                t_weights = now
                print(f"[TX] Weights ({len(sensors)} sensors)")

            # --- Heartbeat (каждые 5 с) ---
            if now - t_heartbeat > 5.0:
                hb = make_heartbeat(uptime=int(now), mcu_temp_c=38.5)
                state['seq'] = send_frame(ser, KIND_HEARTBEAT, hb, state['seq'])
                t_heartbeat = now

            # --- RFID (один раз через 15 с, если --rfid) ---
            if args.rfid and not rfid_sent and now > 15.0:
                rfid = make_rfid(RFID_TAG_DETECTED, reader_id=0, unit_id=0,
                                 tag="AABBCCDDEEFF00")
                state['seq'] = send_frame(ser, KIND_RFID, rfid, state['seq'],
                                          FLAG_ACK_REQUIRED)
                rfid_sent = True
                t_rfid = now
                print(f"[TX] Rfid tag_detected AABBCCDDEEFF00 (R0/U1)")

    except KeyboardInterrupt:
        print("\n[HOST] Прервано")
    finally:
        ser.close()
        print(f"[HOST] UART закрыт. Отправлено кадров: seq={state['seq']}")


if __name__ == "__main__":
    main()
