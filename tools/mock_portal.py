"""
Mock Backend для тестирования cloud flow iDryer LINK

НАЗНАЧЕНИЕ:
Эмулирует HTTP API Backend для тестирования WiFi provisioning и claiming
без реального сервера. Полезен при отладке cloud flow.

ЗАПУСК:
  pip install flask python-socketio eventlet
  python3 mock_portal.py

НАСТРОЙКА ESP32:
Собрать прошивку с флагами:
  -DIDRYER_API_BASE="http://192.168.1.100:5050/api"
  -DMQTT_USE_TLS=0

API ENDPOINTS:
- POST /api/devices/provision — получить токен по serialNumber
- POST /api/devices/register — получить PIN для claiming
- GET /api/devices/check-claim/{token} — проверить статус claiming

ЛОГИКА MOCK:
- provision выдает токен mock-token-{serial}
- register выдает PIN 12345678, remainingSeconds=300
- check-claim автоматически подтверждает при первом вызове

Примечание: Socket.IO обработчики оставлены для совместимости,
но в текущей архитектуре LINK использует MQTT, а не Socket.IO.

Обновлено: 2026-03-25
Статус: ✅ Актуален
"""

import eventlet
eventlet.monkey_patch()
import socketio
from flask import Flask, jsonify, request

eventlet.hubs.use_hub("poll")

sio = socketio.Server(async_mode="eventlet", cors_allowed_origins="*", logger=True, engineio_logger=True)
flask_app = Flask(__name__)

# Простое хранилище состояния в памяти
_devices = {}  # token -> {serialNumber, pin, claimed, deviceId}


@flask_app.post("/api/devices/provision")
def provision_device():
    """
    POST /api/devices/provision
    Body: {"serialNumber": "..."}
    Response: {"deviceToken": "...", "isNew": true, "isClaimed": false}
    """
    payload = request.get_json(force=True) or {}
    serial_number = payload.get("serialNumber", "UNKNOWN")
    token = f"mock-token-{serial_number[-8:]}"
    is_claimed = serial_number in [d.get("serialNumber") for d in _devices.values() if d.get("claimed")]
    _devices[token] = {"serialNumber": serial_number, "claimed": is_claimed, "deviceId": None}
    print(f"[MOCK] provision serial={serial_number} -> token={token} isClaimed={is_claimed}", flush=True)
    return jsonify({
        "deviceToken": token,
        "isNew": True,
        "isClaimed": is_claimed,
    })


@flask_app.post("/api/devices/register")
def register_device():
    """
    POST /api/devices/register
    Body: {"token": "...", "serialNumber": "..."}
    Response: {"pin": "12345678", "remainingSeconds": 300}
           or {"alreadyClaimed": true, "deviceId": "..."}
    """
    payload = request.get_json(force=True) or {}
    token = payload.get("token", "UNKNOWN")
    serial = payload.get("serialNumber", "")
    device = _devices.get(token)
    if device and device.get("claimed"):
        print(f"[MOCK] register token={token} -> alreadyClaimed", flush=True)
        return jsonify({"alreadyClaimed": True, "deviceId": device["deviceId"]})
    pin = "12345678"
    if device:
        device["pin"] = pin
    print(f"[MOCK] register token={token} serial={serial} -> PIN={pin}", flush=True)
    return jsonify({"pin": pin, "remainingSeconds": 300})


@flask_app.get("/api/devices/check-claim/<token>")
def check_claim(token):
    """
    GET /api/devices/check-claim/{token}
    Response: {"claimed": true, "deviceId": "..."}
           or {"claimed": false}
    """
    device = _devices.get(token)
    if device and not device.get("claimed"):
        # Автоматически подтверждаем claiming через 5 секунд после register
        device["claimed"] = True
        device["deviceId"] = f"mock-device-{token[-8:]}"
    if device and device.get("claimed"):
        print(f"[MOCK] check-claim token={token} -> CLAIMED deviceId={device['deviceId']}", flush=True)
        return jsonify({"claimed": True, "deviceId": device["deviceId"]})
    print(f"[MOCK] check-claim token={token} -> not claimed", flush=True)
    return jsonify({"claimed": False})


@flask_app.post("/api/emit-command")
def emit_command():
  body = request.get_json(force=True) or {}
  print(f"[MOCK] manual command -> {body}", flush=True)
  sio.emit("device:command", body)
  return jsonify({"status": "sent"})


@sio.event
def connect(sid, environ):
  print(f"[MOCK] socket connected {sid}", flush=True)
  sio.start_background_task(target=push_command, sid=sid)


@sio.event
def disconnect(sid):
  print(f"[MOCK] socket disconnected {sid}", flush=True)


@sio.on("device:connect")
def device_connect(sid, data):
  print(f"[MOCK] device connect payload: {data}", flush=True)


@sio.on("telemetry:data")
def telemetry_data(sid, data):
  print(f"[MOCK] telemetry received: {data}", flush=True)


@sio.on("device:commandAck")
def command_ack(sid, data):
  print(f"[MOCK] command ack: {data}", flush=True)


def push_command(sid):
  eventlet.sleep(3)
  body = {
      "command": "start",
      "targetTemperature": 40.0,
      "durationMinutes": 20,
      "jobId": 9001
  }
  print(f"[MOCK] emitting device:command -> {body}", flush=True)
  sio.emit("device:command", body, to=sid)


def broadcast_command_cycle():
  commands = [
      {"command": "start", "targetTemperature": 45.0, "durationMinutes": 15, "jobId": 1001},
      {"command": "pause", "durationMinutes": 0, "jobId": 1001},
      {"command": "resume", "durationMinutes": 15, "jobId": 1001},
      {"command": "stop", "jobId": 1001},
  ]
  while True:
    for body in commands:
      eventlet.sleep(5)
      print(f"[MOCK] broadcast command -> {body}", flush=True)
      sio.emit("device:command", body)


if __name__ == "__main__":
  eventlet.spawn_n(broadcast_command_cycle)
  PORT = 5050
  print(f"[MOCK] starting portal on 0.0.0.0:{PORT}", flush=True)
  wsgi_app = socketio.WSGIApp(sio, flask_app)
  eventlet.wsgi.server(eventlet.listen(("0.0.0.0", PORT)), wsgi_app)
