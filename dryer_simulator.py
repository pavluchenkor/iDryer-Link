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
import argparse
import json
import os
from datetime import datetime
from dataclasses import dataclass
from typing import Optional

# JSON для загрузки конфигов (встроенный модуль)

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
FLAG_IS_ACK = 0x02
FLAG_ERROR = 0x04
FLAG_FRAGMENTED = 0x08
FLAG_LAST_FRAGMENT = 0x10

# Размер данных в одном чанке
CONFIG_CHUNK_HEADER_SIZE = 6
CONFIG_CHUNK_DATA_SIZE = 200 - CONFIG_CHUNK_HEADER_SIZE  # 194 байта

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
    sequence = sequence % 256  # Ограничиваем 0-255
    header = struct.pack('BBBBBB', SOF, PROTOCOL_VERSION, flags, kind, sequence, payload_len)
    crc_data = header + payload
    crc = crc16_ccitt(crc_data)
    frame = header + payload + struct.pack('<H', crc)
    ser.write(frame)
    return (sequence + 1) % 256  # Ограничиваем возврат

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
    weight_loss_rate: float = 1.0 / 60   # г/сек (1 г/мин)

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
            if self.weight > self.initial_weight * 0.80:
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
        self.weight = max(self.initial_weight * 0.80, self.weight)

# ============================================================
# СИМУЛЯТОР СУШИЛКИ
# ============================================================
class DryerSimulator:
    def __init__(self, port: str, units_count: int = 1, readers_count: int = 0, unit_capabilities: list = None):
        self.port = port
        self.units_count = units_count
        # Если readers_count не задан (0), используем количество юнитов
        self.readers_count = readers_count if readers_count > 0 else units_count
        # Capabilities для каждого юнита
        if unit_capabilities is None:
            # По умолчанию: normal dryer для всех
            self.unit_capabilities = [0x003B] * units_count
        else:
            self.unit_capabilities = unit_capabilities[:units_count]
            # Дополняем если не хватает
            while len(self.unit_capabilities) < units_count:
                self.unit_capabilities.append(0x003B)

        self.ser = None
        self.sequence = 1
        self.physics = DryerPhysics()
        self.running = True
        self.boot_time = time.time()

        # Link state machine
        self.link_state = "CONNECTING"  # CONNECTING, CONNECTED, DISCONNECTED
        self.hello_attempts = 0
        self.last_hello_sent = 0
        self.last_esp_message = 0
        self.BOOT_HELLO_INTERVAL = 1.0      # 1 сек при boot
        self.RUNTIME_HELLO_INTERVAL = 30.0  # 30 сек в runtime (hotplug)
        self.MAX_BOOT_ATTEMPTS = 10         # 10 попыток при boot

        # 4 RFID метки для тестирования (по одной на каждый ридер)
        self.rfid_tags = {
            1: "DEADBEEF12345678",  # Метка для ридера 1
            2: "ABS001RED",  # Метка для ридера 2
            # 2: "CAFEBABE87654321",  # Метка для ридера 2
            3: "FACADE0123456789",  # Метка для ридера 3
            4: "ABCD123456789EF0",  # Метка для ридера 4
        }

        # Состояние RFID ридеров: True = метка установлена, False = пустой
        # По умолчанию все ридеры пустые (при boot должны отправить TagRemoved)
        self.rfid_loaded = {i: False for i in range(1, 5)}

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

        # Remote Config
        self.config_transfer_id = 0
        self.config_rev = 1  # Версия конфига (инкрементируется при изменениях)

        # Загружаем конфиги из JSON файлов
        self.config_files = {
            1: "config_1unit.json",
            3: "config_3units.json",
        }
        self.configs = {}
        self._load_json_configs()
        self.demo_config = self._create_demo_config()

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
        """Hello от RP2040 с запросом подтверждения"""
        # Формируем units[4] массив (UnitConfig = 12 байт каждый)
        units_data = b''
        for i in range(4):
            if i < self.units_count:
                # Активный юнит: используем кастомные capabilities
                caps = self.unit_capabilities[i]
                unit_config = struct.pack('<BxH4B4B',
                    i,              # unitId
                    # padding 1 байт (x)
                    caps,           # capabilities (uint16_t) - кастомные для каждого юнита
                    i, 0xFF, 0xFF, 0xFF,  # scales[4]: только один датчик весов
                    i, 0xFF, 0xFF, 0xFF   # rfid[4]: только один RFID
                )
            else:
                # Пустой слот
                unit_config = struct.pack('<BxH4B4B',
                    0xFF, 0,
                    0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF
                )
            units_data += unit_config

        payload = struct.pack('<B3xII8sB',
            0x01,  # Role::RpController
            # 3x = padding (3 байта)
            0x010000,  # version 1.0.0
            int(time.time() - self.boot_time),  # uptime
            b'v1.0\x00\x00\x00\x00',  # hardwareVersion
            self.units_count  # unitsCount
        ) + units_data + struct.pack('3s', b'\x00\x00\x00')  # reserved[3]

        # FLAG_ACK_REQUIRED = 0x01
        self.sequence = send_frame(self.ser, MSG_HELLO, payload, flags=FLAG_ACK_REQUIRED, sequence=self.sequence)
        self.last_hello_sent = time.time()

        state_info = ""
        if self.link_state == "CONNECTING":
            state_info = f" (attempt {self.hello_attempts + 1}/{self.MAX_BOOT_ATTEMPTS})"

        # Формируем строку с capabilities для каждого юнита
        if self.units_count == 1:
            caps_str = f"caps=0x{self.unit_capabilities[0]:04x}"
        else:
            caps_list = ','.join(f"0x{c:04x}" for c in self.unit_capabilities[:self.units_count])
            caps_str = f"caps=[{caps_list}]"

        payload_size = len(payload)
        self.log(f"→ Hello sent{state_info} ({self.units_count} units, {caps_str}, {payload_size} bytes), waiting for HelloAck...", color='yellow')

    def send_telemetry(self):
        """Telemetry от RP2040"""
        entries = []

        # TelemetryEntry: unitId(1) + temperatureC10(2) + humidityPct10(2) + heaterPowerPct(1) + fanOn(1)
        for unit_id in range(self.units_count):
            # Уникальные базовые смещения для каждого юнита (чтобы графики не сливались)
            temp_offset = unit_id * 5.0  # 0°C, 5°C, 10°C, 15°C для юнитов 1-4
            humidity_offset = unit_id * 10.0  # 0%, 10%, 20%, 30% для юнитов 1-4

            entries.append(struct.pack('<BhHBB',
                unit_id,  # unitId
                int((self.physics.temperature + temp_offset) * 10),  # temperatureC10
                int(min(1000, (self.physics.humidity + humidity_offset) * 10)),  # humidityPct10 (cap at 100%)
                self.physics.heater_power,  # heaterPowerPct
                1 if self.physics.fan_on else 0  # fanOn
            ))

        # Дополняем до 4 записей
        while len(entries) < 4:
            entries.append(struct.pack('<BhHBB', 0, 0, 0, 0, 0))

        payload = struct.pack('<B', self.units_count) + b''.join(entries)
        self.sequence = send_frame(self.ser, MSG_TELEMETRY, payload, flags=FLAG_ACK_REQUIRED, sequence=self.sequence)

        # Показываем диапазон значений для всех юнитов
        if self.units_count > 1:
            temp_range = f"{self.physics.temperature:.1f}-{self.physics.temperature + (self.units_count - 1) * 5:.1f}°C"
            hum_range = f"{self.physics.humidity:.1f}-{min(100, self.physics.humidity + (self.units_count - 1) * 10):.1f}%"
            self.log(f"→ Telemetry: {self.units_count} unit(s), T={temp_range}, H={hum_range}, heater={self.physics.heater_power}%", color='green')
        else:
            self.log(f"→ Telemetry: {self.units_count} unit(s), {self.physics.temperature:.1f}°C, {self.physics.humidity:.1f}%, heater={self.physics.heater_power}%", color='green')

    def send_weights(self):
        """Weights от RP2040"""
        entries = []

        # WeightEntry: sensorId(1) + unitId(1) + weightGrams(2)
        for unit_id in range(self.units_count):
            # Уникальное смещение веса для каждого юнита (чтобы графики не сливались)
            weight_offset = unit_id * 50.0  # 0г, 50г, 100г, 150г для юнитов 1-4

            entries.append(struct.pack('<BBH',
                unit_id,  # sensorId
                unit_id,  # unitId
                int(self.physics.weight + weight_offset)  # weightGrams
            ))

        while len(entries) < 4:
            entries.append(struct.pack('<BBH', 0, 0, 0))

        payload = struct.pack('<B', self.units_count) + b''.join(entries)
        self.sequence = send_frame(self.ser, MSG_WEIGHTS, payload, flags=0, sequence=self.sequence)

        # Показываем диапазон весов для всех юнитов
        if self.units_count > 1:
            weight_range = f"{self.physics.weight:.1f}-{self.physics.weight + (self.units_count - 1) * 50:.1f}g"
            self.log(f"→ Weight: {self.units_count} unit(s), {weight_range}", color='green')
        else:
            self.log(f"→ Weight: {self.units_count} unit(s), {self.physics.weight:.1f}g", color='green')

    def send_status(self):
        """Status от RP2040"""
        entries = []

        remaining_sec = max(0, self.physics.target_duration * 60 - self.physics.elapsed_seconds)

        # StatusEntry: 30 байт
        for unit_id in range(self.units_count):
            # Генерируем уникальный sessionNum для каждого юнита
            # Формат: базовый номер (YMMDDHHmm) + номер юнита (1-based)
            # Пример: 5122914321 для юнита 1, 5122914322 для юнита 2
            unique_session_num = self.physics.session_num * 10 + (unit_id + 1) if self.physics.session_num > 0 else 0

            entry = struct.pack('<BBIHHH IIII BB',
                unit_id,  # unitId
                self.physics.mode,  # mode (0=IDLE, 1=DRYING)
                unique_session_num,  # sessionNum (уникальный для каждого юнита)
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
        payload = struct.pack('<B', self.units_count) + b''.join(entries) + struct.pack('<I', uptime)
        self.sequence = send_frame(self.ser, MSG_STATUS, payload, flags=0, sequence=self.sequence)

        mode_name = "DRYING" if self.physics.mode == 1 else "IDLE"
        if self.physics.session_num > 0:
            session_range = f"{self.physics.session_num * 10 + 1}-{self.physics.session_num * 10 + self.units_count}"
            self.log(f"→ Status: {self.units_count} unit(s), {mode_name} (sessions #{session_range}, elapsed={self.physics.elapsed_seconds}s)", color='green')
        else:
            self.log(f"→ Status: {self.units_count} unit(s), {mode_name} (elapsed={self.physics.elapsed_seconds}s)", color='green')

    def send_rfid(self):
        """RFID Event от RP2040 - отправляем текущее состояние всех ридеров"""
        for reader_id in range(self.readers_count):
            reader_num = reader_id + 1  # 1-based для отображения
            unit_id = reader_id % self.units_count

            # Проверяем состояние ридера
            if self.rfid_loaded.get(reader_num, False):
                # Метка установлена - отправляем TagDetected
                tag = self.rfid_tags.get(reader_num, "UNKNOWN")
                event = 1  # TagDetected
                event_name = "tag_detected"
            else:
                # Метка извлечена - отправляем TagRemoved
                tag = ""
                event = 2  # TagRemoved
                event_name = "tag_removed"

            payload = struct.pack('<BB32sB2s',
                event,  # event (1=TagDetected, 2=TagRemoved)
                reader_id,  # readerId (0-based)
                tag.ljust(32, '\x00').encode('utf-8'),  # tag (пустой для TagRemoved)
                unit_id,  # unitId (0-based: 0=U1, 1=U2, ...)
                b'\x00\x00'  # padding
            )
            self.sequence = send_frame(self.ser, MSG_RFID, payload, flags=0, sequence=self.sequence)

            if tag:
                self.log(f"→ RFID: R{reader_num} → U{unit_id + 1} {event_name} tag={tag}", color='green')
            else:
                self.log(f"→ RFID: R{reader_num} → U{unit_id + 1} {event_name}", color='yellow')

    def send_heartbeat(self):
        """Heartbeat от RP2040"""
        payload = struct.pack('<IhH',
            int(time.time() - self.boot_time),  # uptimeSeconds
            int(self.physics.temperature * 10),  # wifiRssiDbm (используем как temp MCU)
            0  # errorsSinceBoot
        )
        self.sequence = send_frame(self.ser, MSG_HEARTBEAT, payload, flags=0, sequence=self.sequence)

    # --------------------------------------------------------
    # REMOTE CONFIG
    # --------------------------------------------------------
    def _load_json_configs(self):
        """Загрузка конфигов из JSON файлов"""
        # Определяем директорию скрипта
        script_dir = os.path.dirname(os.path.abspath(__file__))

        for units, filename in self.config_files.items():
            filepath = os.path.join(script_dir, filename)
            if os.path.exists(filepath):
                try:
                    with open(filepath, 'r', encoding='utf-8') as f:
                        config = json.load(f)
                    self.configs[units] = config
                    menu_count = len(config.get('menu', []))
                    self.log(f"Loaded config: {filename} ({units} units, {menu_count} menu items)", color='green')
                except Exception as e:
                    self.log(f"Failed to load {filename}: {e}", color='red')
            else:
                self.log(f"Config file not found: {filepath}", color='yellow')

    def _create_demo_config(self):
        """Создание демо-конфигурации (fallback если JSON не загружен)"""
        # Минимальный конфиг в формате протокола
        return {
            "rev": self.config_rev,
            "v": 8,
            "units": self.units_count,
            "active": 0,
            "lang": "en",
            "menu": [
                {"id": 0, "title": "MENU", "t": "sub", "p": -1, "ch": [1, 2]},
                {"id": 1, "title": "CONTROLLER", "t": "val", "p": 0, "vt": "u8", "min": 0, "max": 2, "step": 1, "scope": "g", "val": 0},
                {"id": 2, "title": "DRYING", "t": "sub", "p": 0, "ch": [3, 4, 5]},
                {"id": 3, "title": "TEMPERATURE", "unit": "°C", "t": "val", "p": 2, "vt": "f32", "min": 30, "max": 110, "step": 1, "scope": "u", "val": [50.0] * self.units_count},
                {"id": 4, "title": "TIME", "unit": "min", "t": "val", "p": 2, "vt": "u16", "min": 0, "max": 600, "step": 1, "scope": "u", "val": [240] * self.units_count},
                {"id": 5, "title": "START", "t": "act", "p": 2},
            ]
        }

    def get_vals_for_units(self, units_count: int):
        """Получить vals формат для указанного количества юнитов

        UART формат: {"rev":N,"vals":{"3":[55,55,60],"143":3,"144":1}}
        Только значения, без структуры меню — она есть в menu_meta на LINK
        """
        # Базовые значения
        vals = {
            # id=143: units_count (global)
            "143": units_count,
            # id=144: language (global), 0=ru, 1=en
            "144": 1,
            # id=3: temperature (per-unit)
            "3": [50.0 + i*5 for i in range(units_count)],
            # id=4: time (per-unit)
            "4": [240] * units_count,
            # id=1: controller (global)
            "1": 0,
        }

        self.log(f"[DEBUG] get_vals_for_units: units_count={units_count}", color='cyan')
        self.log(f"[DEBUG] self.configs keys: {list(self.configs.keys())}", color='cyan')

        # Если есть загруженный JSON конфиг — извлекаем значения из него
        if units_count in self.configs:
            config = self.configs[units_count]
            if "menu" in config:
                menu_items_count = len(config["menu"])
                self.log(f"[DEBUG] Loading {menu_items_count} items from config", color='cyan')
                for item in config["menu"]:
                    if "val" in item and "id" in item:
                        vals[str(item["id"])] = item["val"]
                self.log(f"[DEBUG] After loading from config: {len(vals)} vals", color='cyan')
            else:
                self.log(f"[DEBUG] No 'menu' in config for units={units_count}", color='yellow')
        else:
            self.log(f"[DEBUG] No config loaded for units={units_count}", color='yellow')

        return {"rev": self.config_rev, "full": True, "vals": vals}

    def send_config(self, is_delta=False, delta_changes=None, units_count: int = None):
        """Отправка конфига (полного или delta) с фрагментацией

        UART формат: {"rev":N,"vals":{"3":[55,55,60],"143":3,"144":1}}
        Только значения, без структуры меню

        Args:
            is_delta: True для отправки delta-обновления
            delta_changes: Словарь изменений для delta (формат: {"id": value})
            units_count: Количество юнитов (1 или 3), None = текущее количество
        """
        if is_delta and delta_changes:
            # Delta: только изменённые значения
            self.config_rev += 1
            config_data = {"rev": self.config_rev, "vals": delta_changes}
        else:
            # Полный конфиг: все значения в vals формате
            if units_count is not None:
                config_data = self.get_vals_for_units(units_count)
            else:
                config_data = self.get_vals_for_units(self.units_count)

        json_str = json.dumps(config_data, ensure_ascii=False, separators=(',', ':'))
        json_bytes = json_str.encode('utf-8')
        total_size = len(json_bytes)

        self.config_transfer_id += 1
        transfer_id = self.config_transfer_id

        vals_count = len(config_data.get('vals', {}))
        rev = config_data.get('rev', 0)
        # Желтый цвет для delta (ответ на SET), зеленый для full
        log_color = 'yellow' if is_delta else 'green'
        self.log(f"→ ConfigPush: {'delta' if is_delta else 'full'} ({total_size} bytes, {vals_count} vals, rev={rev}, transferId={transfer_id})", color=log_color)

        # Фрагментация
        offset = 0
        chunk_index = 0
        while offset < total_size:
            # Размер данных этого чанка
            remaining = total_size - offset
            data_len = min(CONFIG_CHUNK_DATA_SIZE, remaining)
            is_last = (offset + data_len >= total_size)

            # Формируем header: transferId(2) + totalSize(2) + chunkIndex(2)
            # totalSize только в первом чанке
            header_total_size = total_size if chunk_index == 0 else 0
            chunk_header = struct.pack('<HHH', transfer_id, header_total_size, chunk_index)

            # Данные
            chunk_data = json_bytes[offset:offset + data_len]

            # Полный payload
            payload = chunk_header + chunk_data
            payload_len = len(payload)

            # Флаги
            flags = FLAG_FRAGMENTED
            if is_last:
                flags |= FLAG_LAST_FRAGMENT

            self.sequence = send_frame(self.ser, MSG_CONFIG, payload, flags=flags, sequence=self.sequence)
            self.log(f"  → Chunk {chunk_index}: {data_len} bytes, flags=0x{flags:02X}", color='cyan')

            offset += data_len
            chunk_index += 1

            # Задержка между фрагментами (ESP32 нужно время на обработку)
            time.sleep(0.05)  # 50ms между чанками

        self.log(f"→ ConfigPush complete: {chunk_index} chunks sent", color='green')

    def send_config_delta(self, menu_id: int, value):
        """Отправка delta-обновления (одно изменение)

        Args:
            menu_id: ID элемента меню
            value: Новое значение (число для global, массив для per-unit)
        """
        # Delta формат: {"d": {"3": [55,60,55], "7": 42}}
        changes = {str(menu_id): value}
        self.send_config(is_delta=True, delta_changes=changes)

    def handle_json_command(self, payload: bytes, sequence: int):
        """Обработка JSON команды от LINK

        Форматы команд:
        - {"cmd":"set","id":3,"unit":0,"val":55.0}
        - {"cmd":"invoke","id":89}
        - {"cmd":"get_config"}
        - {"cmd":"set_active","unit":1}
        """
        try:
            # Пропускаем заголовок фрагментации если есть (6 байт: transferId + totalSize + chunkIndex)
            # Для простоты ищем начало JSON
            json_start = payload.find(b'{')
            if json_start == -1:
                self.log("← Config: no JSON found in payload", color='red')
                return

            json_bytes = payload[json_start:]
            json_str = json_bytes.decode('utf-8', errors='ignore').rstrip('\x00')
            cmd_data = json.loads(json_str)

            cmd = cmd_data.get('cmd')

            if cmd == 'set':
                menu_id = cmd_data.get('id')
                unit = cmd_data.get('unit', 0)
                val = cmd_data.get('val')
                self.log(f"← JSON: SET id={menu_id} unit={unit} val={val}", color='yellow')
                self.log(f"→ Sending DELTA response...", color='yellow')
                # Отправляем delta обратно как подтверждение
                if isinstance(val, list):
                    self.send_config_delta(menu_id, val)
                else:
                    # Для per-unit настроек создаём массив с новым значением
                    # Симулируем что у нас 3 юнита и меняем только указанный
                    if self.units_count > 1:
                        # Per-unit: создаём массив [val, val, val]
                        delta_val = [val] * self.units_count
                        self.send_config_delta(menu_id, delta_val)
                    else:
                        # Global значение
                        self.send_config_delta(menu_id, val)

            elif cmd == 'invoke':
                menu_id = cmd_data.get('id')
                self.log(f"← JSON: INVOKE id={menu_id}", color='yellow')
                # Обработка action по ID
                if menu_id == 5:  # START drying
                    self.log(f"→ Executing START drying action", color='yellow')
                    self.start_drying()
                elif menu_id == 10:  # START storage
                    self.log(f"→ Executing START storage action", color='yellow')
                    self.start_drying(temp=45, duration=9999)
                else:
                    self.log(f"→ Action {menu_id} executed (simulated)", color='yellow')

            elif cmd == 'get_config':
                self.log(f"← JSON: GET_CONFIG", color='blue')
                # Отправляем полный конфиг
                time.sleep(0.05)
                self.send_config(is_delta=False)

            elif cmd == 'set_active':
                unit = cmd_data.get('unit', 0)
                self.log(f"← JSON: SET_ACTIVE unit={unit}", color='blue')
                # TODO: обновить active_unit в конфиге

            else:
                self.log(f"← JSON: Unknown cmd={cmd}", color='yellow')

        except json.JSONDecodeError as e:
            self.log(f"← Config: JSON parse error: {e}", color='red')
        except Exception as e:
            self.log(f"← Config: Error handling command: {e}", color='red')

        # Отправляем ACK
        ack_payload = struct.pack('<BB', sequence, 0)
        self.sequence = send_frame(self.ser, MSG_CONFIG_ACK, ack_payload, sequence=self.sequence)

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

        # Генерируем уникальный номер сессии на основе текущего времени
        # Формат: месяц + день + час + минута (базовый, без unit_id)
        # unit_id добавляется при отправке status для каждого юнита
        # Пример: 12291459 для 29 декабря 14:59
        now = time.localtime()
        self.physics.session_num = int(f"{now.tm_mon:02d}{now.tm_mday:02d}{now.tm_hour:02d}{now.tm_min:02d}")

        self.physics.mode = 1  # DRYING
        self.physics.target_temp = temp
        self.physics.target_duration = duration
        if humidity:
            self.physics.target_humidity = humidity
        self.physics.session_start_time = time.time()
        self.physics.elapsed_seconds = 0
        self.physics.initial_weight = self.physics.weight

        self.log(f"🔥 Started {program_name}: {temp}°C, {duration}min (session base #{self.physics.session_num})", color='red')
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

    def change_units(self, new_count: int):
        """Изменение количества юнитов (имитация изменения конфигурации оборудования)"""
        if new_count < 1 or new_count > 4:
            self.log(f"Invalid units count: {new_count} (must be 1-4)", color='red')
            return

        old_count = self.units_count
        self.units_count = new_count

        # Обновляем readers_count если он привязан к units_count
        if self.readers_count == old_count:
            self.readers_count = new_count

        # Обновляем unit_capabilities: дополняем недостающие элементы
        while len(self.unit_capabilities) < new_count:
            self.unit_capabilities.append(0x003B)  # Normal dryer по умолчанию

        self.log(f"🔧 Configuration changed: {old_count} → {new_count} units", color='magenta')

        # Принудительно отправляем Hello с новой конфигурацией
        self.send_hello()

    def toggle_rfid_tag(self, reader_num: int):
        """Установка/извлечение RFID метки (имитация установки/извлечения катушки)"""
        if reader_num < 1 or reader_num > self.readers_count:
            self.log(f"Invalid reader number: {reader_num} (available: 1-{self.readers_count})", color='red')
            return

        # Переключаем состояние
        current_state = self.rfid_loaded.get(reader_num, False)
        new_state = not current_state
        self.rfid_loaded[reader_num] = new_state

        # Определяем unit_id для этого ридера
        reader_id = reader_num - 1  # 0-based
        unit_id = reader_id % self.units_count

        # Отправляем событие
        if new_state:
            # Метка установлена
            tag = self.rfid_tags.get(reader_num, "UNKNOWN")
            event = 1  # TagDetected
            payload = struct.pack('<BB32sB2s',
                event,
                reader_id,
                tag.ljust(32, '\x00').encode('utf-8'),
                unit_id,
                b'\x00\x00'
            )
            self.sequence = send_frame(self.ser, MSG_RFID, payload, flags=0, sequence=self.sequence)
            self.log(f"🏷️  R{reader_num}: tag_detected → U{unit_id + 1} tag={tag}", color='green')
        else:
            # Метка извлечена
            event = 2  # TagRemoved
            payload = struct.pack('<BB32sB2s',
                event,
                reader_id,
                ''.ljust(32, '\x00').encode('utf-8'),
                unit_id,
                b'\x00\x00'
            )
            self.sequence = send_frame(self.ser, MSG_RFID, payload, flags=0, sequence=self.sequence)
            self.log(f"🏷️  R{reader_num}: tag_removed → U{unit_id + 1}", color='yellow')

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

        flags = data[2]
        msg_kind = data[3]
        sequence = data[4]
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
        if msg_kind == MSG_HELLO:
            # Парсим HelloPayload от ESP32
            if payload_len >= 1:
                role = payload[0]
                role_names = {0: "UNKNOWN", 1: "RpController", 2: "EspBridge", 0xFF: "HelloRequest"}
                self.log(f"← Hello from ESP32 (role={role_names.get(role, f'0x{role:02X}')})", color='blue')

                # role=0xFF (HelloRequest) — ESP просит MCU прислать свои данные
                # Отправляем свой Hello с полными данными (не HelloAck!)
                self.log(f"→ Responding with Hello (bidirectional handshake)", color='cyan')
                self.send_hello()

        elif msg_kind == MSG_HELLO_ACK:
            # Получили HelloAck от ESP32!
            if payload_len >= 36:
                ip_addr = struct.unpack('<I', payload[0:4])[0]
                ssid = payload[4:36].decode('utf-8', errors='ignore').rstrip('\x00')

                ip_str = f"{ip_addr & 0xFF}.{(ip_addr >> 8) & 0xFF}.{(ip_addr >> 16) & 0xFF}.{(ip_addr >> 24) & 0xFF}" if ip_addr else "not connected"

                self.log(f"← HelloAck from ESP32: IP={ip_str}, SSID=\"{ssid}\"", color='green')

                # Обновляем состояние
                self.last_esp_message = time.time()

                if self.link_state != "CONNECTED":
                    old_state = self.link_state
                    self.link_state = "CONNECTED"
                    self.hello_attempts = 0
                    self.log(f"✅ ESP32 Link: {old_state} → CONNECTED", color='green')

        elif msg_kind == MSG_COMMAND:
            # Обновляем watchdog
            self.last_esp_message = time.time()

            # Парсим CommandPayload (13 bytes):
            # command(1) + targetState(1) + unitId(1) + reserved[2](2) + arg0(4) + arg1(4)
            if payload_len >= 13:
                cmd_code, target_state, unit_id = struct.unpack('<BBB', payload[0:3])
                # reserved[2] пропускаем (payload[3:5])
                arg0, arg1 = struct.unpack('<II', payload[5:13])

                # CommandCode: Start=1, Stop=2, Find=3, GetConfig=5, SetConfig=6, ReadRfid=7, WriteRfid=8
                cmd_names = {1: 'START', 2: 'STOP', 3: 'FIND', 5: 'GET_CONFIG', 6: 'SET_CONFIG', 7: 'READ_RFID', 8: 'WRITE_RFID'}
                cmd_name = cmd_names.get(cmd_code, f'UNKNOWN({cmd_code})')

                # DryerMode: Idle=0, Drying=1, Storage=2, Profile=3, Fault=4
                mode_names = {0: 'IDLE', 1: 'DRYING', 2: 'STORAGE', 3: 'PROFILE', 4: 'FAULT'}
                mode_name = mode_names.get(target_state, f'UNKNOWN({target_state})')

                unit_str = f"U{unit_id + 1}" if unit_id < 4 else "ALL"

                # Обработка в зависимости от команды
                if cmd_code == 1:  # START (может быть DRYING или STORAGE)
                    temp_c = arg0 / 10.0 if arg0 > 0 else 55
                    if target_state == 1:  # DRYING
                        duration_min = arg1 if arg1 > 0 else 240
                        self.log(f"← Command: START (DRYING) unitId={unit_str} temp={temp_c}°C duration={duration_min}min", color='blue')
                        self.start_drying(temp=int(temp_c), duration=duration_min)
                    elif target_state == 2:  # STORAGE
                        humidity = arg1 if arg1 > 0 else 15
                        self.log(f"← Command: START (STORAGE) unitId={unit_str} temp={temp_c}°C humidity={humidity}%", color='blue')
                        # Для режима хранения используем start_drying с большой длительностью
                        self.start_drying(temp=int(temp_c), duration=9999)
                elif cmd_code == 2:  # STOP
                    self.log(f"← Command: STOP unitId={unit_str}", color='blue')
                    self.stop_drying()
                elif cmd_code == 3:  # FIND
                    self.log(f"← Command: FIND unitId={unit_str} 🔍 (визуальная индикация)", color='magenta')
                elif cmd_code == 5:  # GET_CONFIG
                    self.log(f"← Command: GET_CONFIG unitId={unit_str} — отправляем полный конфиг", color='blue')
                    # Отправляем ACK сначала
                    ack_payload = struct.pack('<BB', sequence, 0)
                    self.sequence = send_frame(self.ser, MSG_COMMAND_ACK, ack_payload, sequence=self.sequence)
                    # Затем отправляем конфиг
                    time.sleep(0.05)  # Небольшая пауза
                    self.send_config(is_delta=False)
                    return  # ACK уже отправлен
                elif cmd_code == 6:  # SET_CONFIG
                    self.log(f"← Command: SET_CONFIG unitId={unit_str} — применяем настройки", color='blue')
                    # TODO: парсить и применять JSON из arg0/arg1 или отдельного payload
                else:
                    # Прочие команды
                    self.log(f"← Command: {cmd_name} unitId={unit_str} targetState={mode_name} arg0={arg0} arg1={arg1}", color='blue')

            # Отправляем ACK с правильным ackSequence
            ack_payload = struct.pack('<BB', sequence, 0)  # ackSequence=sequence из команды, status=None
            self.sequence = send_frame(self.ser, MSG_COMMAND_ACK, ack_payload, sequence=self.sequence)

        elif msg_kind == MSG_CONFIG:
            self.last_esp_message = time.time()
            # Парсим JSON команду от LINK
            self.handle_json_command(payload, sequence)

        elif msg_kind == MSG_CLAIM_STATUS:
            self.last_esp_message = time.time()
            if payload_len >= 45:
                status = payload[0]
                pin = payload[1:10].decode('utf-8', errors='ignore').rstrip('\x00')
                remaining = struct.unpack('<I', payload[14:18])[0]
                status_names = {0: "IDLE", 1: "PROVISIONING", 2: "WAITING_CLAIM", 3: "CLAIMED", 4: "ERROR"}
                self.log(f"← ClaimStatus: {status_names.get(status, status)} PIN=\"{pin}\" expires_in={remaining}s", color='blue')

        elif msg_kind == MSG_HEARTBEAT:
            # Heartbeat от ESP32 (каждые 5 секунд)
            self.last_esp_message = time.time()
            if payload_len >= 8:
                uptime, rssi, errors = struct.unpack('<IhH', payload[:8])
                # Не логируем каждый heartbeat (слишком много шума)
                # self.log(f"← Heartbeat: uptime={uptime}s rssi={rssi}dBm", color='gray')

        elif msg_kind == MSG_CLAIM_COMPLETE:
            self.last_esp_message = time.time()
            if payload_len >= 38:
                success = payload[0]
                device_id = payload[1:37].decode('utf-8', errors='ignore').rstrip('\x00')
                self.log(f"← ClaimComplete: success={success} deviceId=\"{device_id}\"", color='blue')

    def handle_incoming(self):
        """Обработка входящих сообщений от ESP32 с буферизацией"""
        # Читаем всё что есть в буфере
        if self.ser.in_waiting > 0:
            new_data = self.ser.read(self.ser.in_waiting)
            # DEBUG: показываем что пришло
            hex_str = ' '.join(f'{b:02X}' for b in new_data)
            # self.log(f"[RX] {len(new_data)} bytes: {hex_str}", color='yellow')
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
        """Главный цикл симулятора с Link State Machine"""
        # Начинаем с отправки Hello
        time.sleep(0.5)
        self.send_hello()
        self.hello_attempts = 1

        last_telemetry = time.time()
        last_heartbeat = time.time()
        last_physics = time.time()
        last_weights = time.time()
        last_status = time.time()

        self.log("Simulator starting. Link State Machine: CONNECTING", color='cyan')
        self.log("Keys: 1-5=program, S=stop, H=Hello, R=RFID, U=units, C(1|3)=config, D=delta, Q=quit", color='cyan')

        while self.running:
            now = time.time()

            # === LINK STATE MACHINE ===
            if self.link_state == "CONNECTING":
                # Boot phase: пытаемся подключиться к ESP32
                if now - self.last_hello_sent >= self.BOOT_HELLO_INTERVAL:
                    if self.hello_attempts >= self.MAX_BOOT_ATTEMPTS:
                        # Переходим в standalone режим
                        self.link_state = "DISCONNECTED"
                        self.log(f"⚠️  ESP32 Link: CONNECTING → DISCONNECTED (timeout after {self.MAX_BOOT_ATTEMPTS} attempts)", color='yellow')
                        self.log("📱 Running in STANDALONE mode (local UI only)", color='yellow')
                        self.hello_attempts = 0
                    else:
                        # Повторяем Hello
                        self.send_hello()
                        self.hello_attempts += 1

            elif self.link_state == "DISCONNECTED":
                # Standalone mode: периодически проверяем hotplug
                if now - self.last_hello_sent >= self.RUNTIME_HELLO_INTERVAL:
                    self.log("🔍 Checking for ESP32 hotplug...", color='cyan')
                    self.send_hello()

            elif self.link_state == "CONNECTED":
                # Connected mode: watchdog на потерю связи
                if now - self.last_esp_message > 60.0:
                    self.log("⚠️  ESP32 Link: CONNECTED → DISCONNECTED (watchdog timeout)", color='yellow')
                    self.link_state = "DISCONNECTED"

            # === ФИЗИКА (всегда работает) ===
            if now - last_physics >= 1.0:
                self.physics.update(dt=1.0)
                last_physics = now

            # === ОТПРАВКА ДАННЫХ (только если CONNECTED) ===
            if self.link_state == "CONNECTED":
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

                # Отправка Status (каждые 20 секунд)
                if now - last_status >= 20.0:
                    self.send_status()
                    last_status = now

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
                elif key == 'r':
                    # Интерактивное управление RFID метками
                    # Показываем текущее состояние всех ридеров
                    simulator.log("Current RFID readers state:", color='cyan')
                    for r in range(1, simulator.readers_count + 1):
                        reader_id = r - 1
                        unit_id = reader_id % simulator.units_count
                        loaded = simulator.rfid_loaded.get(r, False)
                        if loaded:
                            tag = simulator.rfid_tags.get(r, "UNKNOWN")
                            simulator.log(f"  R{r} → U{unit_id + 1}: LOADED (tag={tag})", color='green')
                        else:
                            simulator.log(f"  R{r} → U{unit_id + 1}: EMPTY", color='yellow')

                    simulator.log(f"Select reader to toggle (1-{simulator.readers_count}):", color='yellow')
                    if select.select([sys.stdin], [], [], 2.0)[0]:
                        reader_key = sys.stdin.read(1)
                        if reader_key.isdigit():
                            reader_num = int(reader_key)
                            if 1 <= reader_num <= simulator.readers_count:
                                simulator.toggle_rfid_tag(reader_num)
                            else:
                                simulator.log(f"Invalid reader: {reader_num} (available: 1-{simulator.readers_count})", color='red')
                        else:
                            simulator.log(f"Invalid input: {reader_key}", color='red')
                    else:
                        simulator.log("Timeout", color='red')
                elif key == 'h':
                    simulator.send_hello()
                elif key == 'c':
                    # Отправка полного конфига (C1 = 1 unit, C3 = 3 units)
                    simulator.log("Select config: 1=1unit, 3=3units, Enter=current:", color='yellow')
                    if select.select([sys.stdin], [], [], 2.0)[0]:
                        config_key = sys.stdin.read(1)
                        if config_key == '1':
                            simulator.send_config(is_delta=False, units_count=1)
                        elif config_key == '3':
                            simulator.send_config(is_delta=False, units_count=3)
                        elif config_key == '\n' or config_key == '\r':
                            simulator.send_config(is_delta=False)
                        else:
                            simulator.log(f"Invalid config: {repr(config_key)}, use 1, 3 or Enter", color='red')
                    else:
                        # Таймаут - отправляем текущий конфиг
                        simulator.send_config(is_delta=False)
                elif key == 'd':
                    # Отправка delta (пример: изменение температуры сушки для 3 юнитов)
                    # ID 3 = dry_temp (per-unit)
                    simulator.send_config_delta(3, [55.0, 70.0, 90.0])
                elif key == 'u':
                    # Ждем следующую цифру 1-4
                    simulator.log("Select units count (1-4):", color='yellow')
                    if select.select([sys.stdin], [], [], 2.0)[0]:  # 2 секунды таймаут
                        units_key = sys.stdin.read(1)
                        if units_key in '1234':
                            new_count = int(units_key)
                            simulator.change_units(new_count)
                        else:
                            simulator.log(f"Invalid units count: {units_key}", color='red')
                    else:
                        simulator.log("Timeout, cancelled", color='red')
                elif key in '12345':
                    simulator.start_drying(key)
    finally:
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)

# ============================================================
# MAIN
# ============================================================
def main():
    parser = argparse.ArgumentParser(description='iDryer RP2040 Simulator - Физическая модель сушилки')
    parser.add_argument('port', help='Serial port (e.g., /dev/ttyUSB0)')
    parser.add_argument('-u', '--units', type=int, default=1, choices=[1, 2, 3, 4],
                        help='Количество камер сушилки (1-4, по умолчанию: 1)')
    parser.add_argument('-r', '--readers', type=int, default=0, choices=[0, 1, 2, 3, 4],
                        help='Количество RFID ридеров (1-4, по умолчанию: равно units)')
    parser.add_argument('-c', '--caps', type=str, default='normal',
                        help='Capabilities юнитов: "normal" (0x003B), "monitor" (0x0038), '
                             'или hex/bin/dec: "0x003B" / "0b00111011" / "59" для всех, '
                             'или "0x003B,0x0038,..." для каждого юнита отдельно')

    args = parser.parse_args()

    # Парсим capabilities
    def parse_capabilities(caps_str, units_count):
        """Парсит строку capabilities в список для каждого юнита"""
        # Пресеты
        if caps_str == 'normal':
            # Normal dryer: heater + fan + sensors
            return [0x003B] * units_count
        elif caps_str == 'monitor':
            # Monitor mode: только sensors
            return [0x0038] * units_count

        # Hex/Binary значения
        if ',' in caps_str:
            # Список значений для каждого юнита
            values_str = caps_str.split(',')
            result = []
            for i, val in enumerate(values_str):
                if i >= units_count:
                    break
                try:
                    # Автоопределение формата: 0x - hex, 0b - binary, иначе decimal
                    result.append(int(val.strip(), 0))
                except ValueError:
                    print(f"Invalid value: {val}, using 0x003B")
                    result.append(0x003B)
            # Дополняем до units_count
            while len(result) < units_count:
                result.append(result[0] if result else 0x003B)
            return result
        else:
            # Одно значение для всех юнитов
            try:
                # Автоопределение формата: 0x - hex, 0b - binary, иначе decimal
                value = int(caps_str, 0)
                return [value] * units_count
            except ValueError:
                print(f"Invalid value: {caps_str}, using 0x003B (normal)")
                return [0x003B] * units_count

    unit_capabilities = parse_capabilities(args.caps, args.units)

    # Количество ридеров по умолчанию = количество юнитов
    readers_count = args.readers if args.readers > 0 else args.units

    # RFID метки (автоматически привязываются к ридерам)
    rfid_tags = {
        1: "DEADBEEF12345678",
        2: "CAFEBABE87654321",
        3: "FACADE0123456789",
        4: "ABCD123456789EF0",
    }

    # Формируем строку с RFID ридерами
    rfid_info = ""
    for i in range(1, readers_count + 1):
        unit_id = ((i - 1) % args.units) + 1
        rfid_info += f"\n║      R{i} → U{unit_id}: {rfid_tags[i]:<34} ║"

    # Формируем строку с capabilities
    def caps_to_str(caps):
        """Преобразует capabilities в читаемую строку"""
        features = []
        if caps & (1 << 0): features.append("heater")
        if caps & (1 << 1): features.append("fan")
        if caps & (1 << 2): features.append("servo")
        if caps & (1 << 3): features.append("RhAir")
        if caps & (1 << 4): features.append("TempAir")
        if caps & (1 << 5): features.append("TempHeater")
        return '+'.join(features) if features else "none"

    caps_info = ""
    for i in range(args.units):
        caps = unit_capabilities[i]
        caps_info += f"\n║      U{i+1}: 0x{caps:04x} ({caps_to_str(caps):<38}) ║"

    print(f"""
╔═══════════════════════════════════════════════════════╗
║      iDryer RP2040 (Сушилка) Simulator v1.0          ║
║      Физическая модель + интерактивное управление     ║
║      Камер: {args.units}, RFID ридеров: {readers_count:<30} ║{rfid_info}
║                                                       ║
║ 🔧 Hardware Capabilities:                             ║{caps_info}
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
  H   - Отправить Hello
  R   - Установить/извлечь RFID метку (toggle)
  U   - Изменить количество юнитов (1-4)
  C   - Отправить Config (C1=1unit, C3=3units, Enter=текущий)
  D   - Отправить Delta (изменение настройки)
  Q   - Выход

🔄 Автоматическая отправка:
  - Telemetry (каждые 5 сек)
  - Heartbeat (каждые 10 сек)
  - Weights (каждые 15 сек)
  - Status (каждые 20 сек)

💡 RFID метки управляются вручную (R), по умолчанию все пустые

⚙️  Параметры запуска:
  -u N              Количество юнитов (1-4)
  -r N              Количество RFID ридеров (по умолчанию = units)
  -c MODE           Hardware capabilities (битовая маска):

    Биты (uint16_t):
    ┌─────┬──────────────────┬────────────────────────────────────────┐
    │ Bit │ Название         │ Описание                               │
    ├─────┼──────────────────┼────────────────────────────────────────┤
    │  0  │ heater           │ Есть нагреватель                       │
    │  1  │ fan              │ Есть вентилятор                        │
    │  2  │ servo            │ Есть сервопривод заслонки              │
    │  3  │ RhAirSensor      │ Есть датчик влажности воздуха          │
    │  4  │ TempAirSensor    │ Есть датчик температуры воздуха        │
    │  5  │ TempHeaterSensor │ Есть датчик температуры нагревателя    │
    │ 6-15│ (резерв)         │ Зарезервировано для будущих флагов     │
    └─────┴──────────────────┴────────────────────────────────────────┘

    Пресеты:
      "normal"  - 0x003B (0b00111011) heater+fan+sensors
      "monitor" - 0x0038 (0b00111000) только sensors

    Форматы значений (поддерживаются hex/binary/decimal):
      "0x003B"              - hex для всех юнитов
      "0b00111011"          - binary для всех юнитов
      "59"                  - decimal для всех юнитов
      "0x003B,0x0038,..."   - разные значения для каждого юнита

  Примеры:
    python3 dryer_simulator.py /dev/ttyUSB0 -u2 -c normal
    python3 dryer_simulator.py /dev/ttyUSB0 -u2 -c 0b00111011
    python3 dryer_simulator.py /dev/ttyUSB0 -u4 -c 0x003B,0x0038,0x003B,0x0038
""")

    simulator = DryerSimulator(args.port, args.units, readers_count, unit_capabilities)

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
