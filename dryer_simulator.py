#!/usr/bin/env python3
"""
iDryer RP2040 (Сушилка) Симулятор
Эмулирует реальную сушилку с физической моделью и интерактивным управлением
Работает по UART с ESP32-C3
"""

import serial
import struct
import time
import sys
import threading
import select
import termios
import tty
from datetime import datetime
from dataclasses import dataclass
from typing import Optional

# ============================================================
# КОНСТАНТЫ ПРОТОКОЛА
# ============================================================
SOF = 0xAA
PROTOCOL_VERSION = 0x01

# MessageKind
MSG_HELLO = 0x01
MSG_HELLO_ACK = 0x02
MSG_COMMAND = 0x20
MSG_COMMAND_ACK = 0x21
MSG_CONFIG = 0x30
MSG_CONFIG_ACK = 0x31
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

# ============================================================
# CRC16-CCITT
# ============================================================
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

# ============================================================
# ОТПРАВКА ФРЕЙМОВ
# ============================================================
def send_frame(ser, kind, payload, flags=0, sequence=0):
    """Отправка фрейма по UART"""
    payload_len = len(payload)
    header = struct.pack('BBBBBB', SOF, PROTOCOL_VERSION, flags, kind, sequence, payload_len)
    crc_data = header + payload
    crc = crc16_ccitt(crc_data)
    frame = header + payload + struct.pack('<H', crc)
    ser.write(frame)
    return sequence + 1

# ============================================================
# ФИЗИЧЕСКАЯ МОДЕЛЬ СУШИЛКИ
# ============================================================
@dataclass
class DryerPhysics:
    """Физическая модель сушилки"""
    # Текущие значения
    temperature: float = 25.0      # °C
    humidity: float = 60.0          # %
    weight: float = 1000.0          # грамм

    # Целевые значения
    target_temp: int = 55           # °C
    target_humidity: int = 15       # %
    target_duration: int = 240      # минуты

    # Состояние
    mode: int = 0                   # 0=IDLE, 1=DRYING
    heater_power: int = 0           # 0-100%
    fan_on: bool = False

    # Сессия
    session_num: int = 0
    session_start_time: Optional[float] = None
    elapsed_seconds: int = 0

    # Физические константы
    heating_rate: float = 0.15      # °C/сек при 100% мощности
    cooling_rate: float = 0.02      # °C/сек
    humidity_decrease_rate: float = 0.15  # %/сек
    humidity_increase_rate: float = 0.02  # %/сек
    weight_loss_rate: float = 0.02 / 60   # г/сек (0.02 г/мин)

    initial_weight: float = 1000.0

    def update(self, dt: float = 1.0):
        """Обновление физики (вызывается каждую секунду)"""
        if self.mode == 1:  # DRYING
            # Обновляем время
            if self.session_start_time:
                self.elapsed_seconds = int(time.time() - self.session_start_time)

            # Температурный контроль
            if self.temperature < self.target_temp - 1:
                # Нагрев
                self.heater_power = 100
                self.temperature += self.heating_rate * dt
            elif self.temperature > self.target_temp + 1:
                # Охлаждение
                self.heater_power = 0
                self.temperature -= self.cooling_rate * dt
            else:
                # Поддержание температуры
                self.heater_power = 50
                # Небольшая осцилляция ±0.5°C
                import math
                oscillation = 0.5 * math.sin(time.time() / 30)
                self.temperature = self.target_temp + oscillation

            # Контроль влажности
            if self.humidity > self.target_humidity:
                self.humidity -= self.humidity_decrease_rate * dt
                # Случайные скачки (имитация открытия двери)
                if int(time.time()) % 120 == 0:  # раз в 2 минуты
                    self.humidity += 2.0

            # Потеря веса
            if self.weight > self.initial_weight * 0.95:
                self.weight -= self.weight_loss_rate * dt

            self.fan_on = True
        else:
            # IDLE - остывание
            if self.temperature > 25:
                self.temperature -= self.cooling_rate * dt
            if self.humidity < 60:
                self.humidity += self.humidity_increase_rate * dt
            self.heater_power = 0
            self.fan_on = False

        # Ограничения
        self.temperature = max(20, min(100, self.temperature))
        self.humidity = max(5, min(95, self.humidity))
        self.weight = max(self.initial_weight * 0.9, self.weight)

# ============================================================
# СИМУЛЯТОР СУШИЛКИ
# ============================================================
class DryerSimulator:
    def __init__(self, port: str):
        self.port = port
        self.ser = None
        self.sequence = 1
        self.physics = DryerPhysics()
        self.running = True
        self.boot_time = time.time()
        self.rfid_tag = "DEADBEEF12345678"

        # Буфер для приёма UART
        self.rx_buffer = bytearray()

        # Программы сушки
        self.programs = {
            '1': {'name': 'PETG', 'temp': 60, 'duration': 240},
            '2': {'name': 'TPU', 'temp': 70, 'duration': 300},
            '3': {'name': 'ABS', 'temp': 85, 'duration': 180},
            '4': {'name': 'PA', 'temp': 90, 'duration': 240},
            '5': {'name': 'PC', 'temp': 100, 'duration': 300},
        }

    def connect(self):
        """Подключение к UART"""
        try:
            self.ser = serial.Serial(self.port, 115200, timeout=0.1)
            self.log("Connected to ESP32", color='green')
            return True
        except Exception as e:
            self.log(f"Failed to connect: {e}", color='red')
            return False

    def log(self, msg: str, color: str = 'white'):
        """Цветной вывод"""
        colors = {
            'white': '\033[0m',
            'green': '\033[92m',
            'red': '\033[91m',
            'yellow': '\033[93m',
            'blue': '\033[94m',
            'cyan': '\033[96m',
        }
        timestamp = datetime.now().strftime('%H:%M:%S')
        print(f"{colors.get(color, '')}[{timestamp}] {msg}\033[0m")

    # --------------------------------------------------------
    # ОТПРАВКА СООБЩЕНИЙ
    # --------------------------------------------------------
    def send_hello(self):
        """Hello от RP2040"""
        payload = struct.pack('<BII8s',
            0x01,  # Role::RpController
            0x010000,  # version 1.0.0
            int(time.time() - self.boot_time),  # uptime
            b'v1.0\x00\x00\x00\x00'
        )
        self.sequence = send_frame(self.ser, MSG_HELLO, payload, flags=0, sequence=self.sequence)
        self.log("→ Hello sent", color='cyan')

    def send_telemetry(self):
        """Telemetry от RP2040"""
        count = 1  # 1 unit
        entries = []

        # TelemetryEntry: unitId(1) + temperatureC10(2) + humidityPct(1) + heaterPowerPct(1) + fanOn(1)
        entries.append(struct.pack('<BhBBB',
            0,  # unitId
            int(self.physics.temperature * 10),  # temperatureC10
            int(self.physics.humidity),  # humidityPct
            self.physics.heater_power,  # heaterPowerPct
            1 if self.physics.fan_on else 0  # fanOn
        ))

        # Дополняем до 4 записей
        while len(entries) < 4:
            entries.append(struct.pack('<BhBBB', 0, 0, 0, 0, 0))

        payload = struct.pack('<B', count) + b''.join(entries)
        self.sequence = send_frame(self.ser, MSG_TELEMETRY, payload, flags=FLAG_ACK_REQUIRED, sequence=self.sequence)

        self.log(f"→ Telemetry: {self.physics.temperature:.1f}°C, {self.physics.humidity:.1f}%, heater={self.physics.heater_power}%", color='green')

    def send_weights(self):
        """Weights от RP2040"""
        count = 1
        entries = []

        # WeightEntry: sensorId(1) + unitId(1) + weightGrams(2)
        entries.append(struct.pack('<BBH',
            0,  # sensorId
            0,  # unitId
            int(self.physics.weight)  # weightGrams
        ))

        while len(entries) < 4:
            entries.append(struct.pack('<BBH', 0, 0, 0))

        payload = struct.pack('<B', count) + b''.join(entries)
        self.sequence = send_frame(self.ser, MSG_WEIGHTS, payload, flags=0, sequence=self.sequence)

        self.log(f"→ Weight: {self.physics.weight:.1f}g", color='yellow')

    def send_status(self):
        """Status от RP2040"""
        count = 1
        entries = []

        remaining_sec = max(0, self.physics.target_duration * 60 - self.physics.elapsed_seconds)

        # StatusEntry: 30 байт
        entry = struct.pack('<BBIHHH IIII BB',
            0,  # unitId
            self.physics.mode,  # mode (0=IDLE, 1=DRYING)
            self.physics.session_num,  # sessionNum
            int(self.physics.target_temp * 10),  # targetTempC10
            self.physics.target_humidity,  # targetHumidityPct
            self.physics.target_duration,  # durationMinutes
            self.physics.elapsed_seconds,  # elapsedSeconds
            0,  # stageElapsedSeconds
            0,  # stageRemainingSeconds
            remaining_sec,  # totalRemainingSeconds
            0,  # currentStage
            0   # totalStages
        )
        entries.append(entry)

        while len(entries) < 4:
            entries.append(b'\x00' * 30)

        uptime = int(time.time() - self.boot_time)
        payload = struct.pack('<B', count) + b''.join(entries) + struct.pack('<I', uptime)
        self.sequence = send_frame(self.ser, MSG_STATUS, payload, flags=0, sequence=self.sequence)

        mode_name = "DRYING" if self.physics.mode == 1 else "IDLE"
        self.log(f"→ Status: {mode_name} (session #{self.physics.session_num}, elapsed={self.physics.elapsed_seconds}s)", color='blue')

    def send_rfid(self):
        """RFID Event от RP2040"""
        payload = struct.pack('<BB32sB2s',
            1,  # event (TagDetected)
            0,  # readerId
            self.rfid_tag.ljust(32, '\x00').encode('utf-8'),  # tag
            0,  # unitId
            b'\x00\x00'  # padding
        )
        self.sequence = send_frame(self.ser, MSG_RFID, payload, flags=0, sequence=self.sequence)
        self.log(f"→ RFID: {self.rfid_tag}", color='cyan')

    def send_heartbeat(self):
        """Heartbeat от RP2040"""
        payload = struct.pack('<IhH',
            int(time.time() - self.boot_time),  # uptimeSeconds
            int(self.physics.temperature * 10),  # wifiRssiDbm (используем как temp MCU)
            0  # errorsSinceBoot
        )
        self.sequence = send_frame(self.ser, MSG_HEARTBEAT, payload, flags=0, sequence=self.sequence)

    # --------------------------------------------------------
    # УПРАВЛЕНИЕ СУШКОЙ
    # --------------------------------------------------------
    def start_drying(self, program_key: str = None, temp: int = None, duration: int = None, humidity: int = None):
        """Запуск программы сушки"""
        # Если передан ключ программы - используем предустановку
        if program_key and program_key in self.programs:
            prog = self.programs[program_key]
            temp = prog['temp']
            duration = prog['duration']
            program_name = prog['name']
        else:
            # Используем параметры из команды
            temp = temp or 55
            duration = duration or 240
            humidity = humidity or 15
            program_name = f"Custom {temp}°C"

        self.physics.session_num += 1
        self.physics.mode = 1  # DRYING
        self.physics.target_temp = temp
        self.physics.target_duration = duration
        if humidity:
            self.physics.target_humidity = humidity
        self.physics.session_start_time = time.time()
        self.physics.elapsed_seconds = 0
        self.physics.initial_weight = self.physics.weight

        self.log(f"🔥 Started {program_name}: {temp}°C, {duration}min (session #{self.physics.session_num})", color='red')
        self.send_status()

    def stop_drying(self):
        """Остановка сушки"""
        if self.physics.mode == 0:
            self.log("Already IDLE", color='yellow')
            return

        session = self.physics.session_num
        self.physics.mode = 0  # IDLE
        self.physics.session_num = 0
        self.physics.session_start_time = None
        self.physics.elapsed_seconds = 0

        self.log(f"⏹️  Stopped (session #{session})", color='yellow')
        self.send_status()

    # --------------------------------------------------------
    # ПРИЕМ СООБЩЕНИЙ ОТ ESP32
    # --------------------------------------------------------
    def parse_frame(self, data: bytes):
        """Парсинг одного фрейма"""
        if len(data) < 8:  # минимум: header(6) + crc(2)
            return

        # Проверяем SOF и версию
        if data[0] != SOF or data[1] != PROTOCOL_VERSION:
            return

        msg_kind = data[3]
        payload_len = data[5]

        # Проверяем что весь фрейм получен
        expected_len = 6 + payload_len + 2  # header + payload + crc
        if len(data) < expected_len:
            return

        # Проверяем CRC
        crc_received = struct.unpack('<H', data[6 + payload_len:6 + payload_len + 2])[0]
        crc_calculated = crc16_ccitt(data[0:6 + payload_len])
        if crc_received != crc_calculated:
            self.log(f"CRC error: expected {crc_calculated:04X}, got {crc_received:04X}", color='red')
            return

        # Извлекаем payload
        payload = data[6:6 + payload_len]

        # Обработка по типу сообщения
        if msg_kind == MSG_COMMAND:
            # Парсим CommandPayload (10 bytes):
            # command(1) + targetState(1) + arg0(4) + arg1(4)
            if payload_len >= 10:
                cmd_code, target_state = struct.unpack('<BB', payload[0:2])
                arg0, arg1 = struct.unpack('<II', payload[2:10])

                cmd_names = {0: 'NONE', 1: 'START', 2: 'STOP', 3: 'PAUSE', 4: 'RESUME', 5: 'SET_CONFIG'}
                cmd_name = cmd_names.get(cmd_code, f'UNKNOWN({cmd_code})')

                # arg0 = temperatureC10, arg1 = durationMinutes
                temp_c = arg0 / 10.0 if arg0 > 0 else 55
                duration_min = arg1 if arg1 > 0 else 240

                self.log(f"← Command: {cmd_name} (targetState={target_state}, temp={temp_c}°C, duration={duration_min}min)", color='cyan')

                # Обработка команды
                if cmd_code == 1:  # START
                    self.start_drying(temp=int(temp_c), duration=duration_min)
                elif cmd_code == 2:  # STOP
                    self.stop_drying()

            # Отправляем ACK
            ack_payload = struct.pack('<BB', 0, 0)  # ackSequence=0, status=None
            self.sequence = send_frame(self.ser, MSG_COMMAND_ACK, ack_payload, sequence=self.sequence)

        elif msg_kind == MSG_CONFIG:
            self.log("← Config received from ESP", color='cyan')
            # Отправляем ACK
            ack_payload = struct.pack('<BB', 0, 0)
            self.sequence = send_frame(self.ser, MSG_CONFIG_ACK, ack_payload, sequence=self.sequence)

        elif msg_kind == MSG_CLAIM_STATUS:
            if payload_len >= 45:
                status = payload[0]
                pin = payload[1:10].decode('utf-8', errors='ignore').rstrip('\x00')
                remaining = struct.unpack('<I', payload[14:18])[0]
                status_names = {0: "IDLE", 1: "PROVISIONING", 2: "WAITING_CLAIM", 3: "CLAIMED", 4: "ERROR"}
                self.log(f"← ClaimStatus: {status_names.get(status, status)} PIN=\"{pin}\" expires_in={remaining}s", color='green')

        elif msg_kind == MSG_CLAIM_COMPLETE:
            if payload_len >= 38:
                success = payload[0]
                device_id = payload[1:37].decode('utf-8', errors='ignore').rstrip('\x00')
                self.log(f"← ClaimComplete: success={success} deviceId=\"{device_id}\"", color='green')

    def handle_incoming(self):
        """Обработка входящих сообщений от ESP32 с буферизацией"""
        # Читаем всё что есть в буфере
        if self.ser.in_waiting > 0:
            new_data = self.ser.read(self.ser.in_waiting)
            # DEBUG: показываем что пришло
            hex_str = ' '.join(f'{b:02X}' for b in new_data)
            self.log(f"[RX] {len(new_data)} bytes: {hex_str}", color='yellow')
            self.rx_buffer.extend(new_data)

        # Ищем и обрабатываем фреймы
        while len(self.rx_buffer) >= 8:  # минимальный фрейм
            # Ищем SOF
            sof_index = self.rx_buffer.find(SOF)
            if sof_index == -1:
                # SOF не найден - очищаем буфер
                self.rx_buffer.clear()
                break

            # Удаляем мусор перед SOF
            if sof_index > 0:
                self.rx_buffer = self.rx_buffer[sof_index:]

            # Проверяем что у нас есть хотя бы заголовок
            if len(self.rx_buffer) < 6:
                break

            # Читаем payload_len
            payload_len = self.rx_buffer[5]
            frame_len = 6 + payload_len + 2  # header + payload + crc

            # Ждём полный фрейм
            if len(self.rx_buffer) < frame_len:
                break

            # Извлекаем фрейм
            frame = bytes(self.rx_buffer[:frame_len])
            self.rx_buffer = self.rx_buffer[frame_len:]

            # Парсим фрейм
            self.parse_frame(frame)

    # --------------------------------------------------------
    # ГЛАВНЫЙ ЦИКЛ
    # --------------------------------------------------------
    def run(self):
        """Главный цикл симулятора"""
        # Отправляем Hello при старте
        time.sleep(1)
        self.send_hello()

        # Начальный статус
        self.send_status()
        self.send_weights()
        self.send_rfid()

        last_telemetry = time.time()
        last_heartbeat = time.time()
        last_physics = time.time()
        last_weights = time.time()

        self.log("Simulator running. Press 1-5 to start program, S to stop, Q to quit", color='cyan')

        while self.running:
            now = time.time()

            # Обновление физики (каждую секунду)
            if now - last_physics >= 1.0:
                self.physics.update(dt=1.0)
                last_physics = now

            # Отправка телеметрии (каждые 5 секунд)
            if now - last_telemetry >= 5.0:
                self.send_telemetry()
                last_telemetry = now

            # Отправка Heartbeat (каждые 10 секунд)
            if now - last_heartbeat >= 10.0:
                self.send_heartbeat()
                last_heartbeat = now

            # Отправка Weights (каждые 15 секунд)
            if now - last_weights >= 15.0:
                self.send_weights()
                last_weights = now

            # Обработка входящих сообщений от ESP
            self.handle_incoming()

            time.sleep(0.1)

    def close(self):
        """Закрытие подключения"""
        if self.ser:
            self.ser.close()
        self.log("Disconnected", color='yellow')

# ============================================================
# ИНТЕРАКТИВНОЕ УПРАВЛЕНИЕ (КЛАВИАТУРА)
# ============================================================
def keyboard_listener(simulator: DryerSimulator):
    """Слушатель клавиатуры (отдельный поток)"""
    old_settings = termios.tcgetattr(sys.stdin)
    try:
        tty.setcbreak(sys.stdin.fileno())
        while simulator.running:
            if select.select([sys.stdin], [], [], 0.1)[0]:
                key = sys.stdin.read(1).lower()

                if key == 'q':
                    simulator.running = False
                    simulator.log("Quitting...", color='red')
                elif key == 's':
                    simulator.stop_drying()
                elif key in '12345':
                    simulator.start_drying(key)
    finally:
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)

# ============================================================
# MAIN
# ============================================================
def main():
    if len(sys.argv) < 2:
        print("Usage: python3 dryer_simulator.py <SERIAL_PORT>")
        print("Example: python3 dryer_simulator.py /dev/ttyUSB0")
        sys.exit(1)

    port = sys.argv[1]

    print("""
╔═══════════════════════════════════════════════════════╗
║      iDryer RP2040 (Сушилка) Simulator v1.0          ║
║      Физическая модель + интерактивное управление     ║
╚═══════════════════════════════════════════════════════╝

📋 Программы сушки:
  1. PETG  → 60°C,  240 мин (4 ч)
  2. TPU   → 70°C,  300 мин (5 ч)
  3. ABS   → 85°C,  180 мин (3 ч)
  4. PA    → 90°C,  240 мин (4 ч)
  5. PC    → 100°C, 300 мин (5 ч)

⌨️  Управление:
  1-5 - Запуск программы
  S   - Стоп (IDLE)
  Q   - Выход

🔄 Автоматическая отправка:
  - Telemetry (каждые 5 сек)
  - Heartbeat (каждые 10 сек)
  - Weights (каждые 15 сек)
""")

    simulator = DryerSimulator(port)

    if not simulator.connect():
        sys.exit(1)

    # Запускаем слушатель клавиатуры в отдельном потоке
    keyboard_thread = threading.Thread(target=keyboard_listener, args=(simulator,), daemon=True)
    keyboard_thread.start()

    try:
        simulator.run()
    except KeyboardInterrupt:
        print("\n")
        simulator.log("Interrupted by user", color='yellow')
    finally:
        simulator.close()
        keyboard_thread.join(timeout=1)

if __name__ == '__main__':
    main()
