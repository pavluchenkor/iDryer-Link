import eventlet
eventlet.monkey_patch()
import socketio
from flask import Flask, jsonify, request

eventlet.hubs.use_hub("poll")

sio = socketio.Server(async_mode="eventlet", cors_allowed_origins="*", logger=True, engineio_logger=True)
flask_app = Flask(__name__)


@flask_app.post("/api/devices/register")
def register_device():
  payload = request.get_json(force=True) or {}
  token = payload.get("token", "UNKNOWN")
  serial = payload.get("serialNumber", "IDRYER-PRO-MOCK")
  print(f"[MOCK] register token={token} serial={serial}", flush=True)
  return jsonify({"pin": "123456"})


@flask_app.get("/api/devices/check-claim/<token>")
def check_claim(token):
  print(f"[MOCK] claim check token={token}", flush=True)
  return jsonify({
      "claimed": True,
      "device": {
          "id": f"DEV-{token[-4:] or '0000'}",
          "serialNumber": "IDRYER-PRO-MOCK"
      }
  })


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
