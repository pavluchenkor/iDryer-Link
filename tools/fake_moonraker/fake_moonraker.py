#!/usr/bin/env python3
"""
Fake Moonraker WS-server для проверки iHeater Link Moonraker integration.
Слушает ws://0.0.0.0:7125/websocket, отвечает на JSON-RPC и шлёт
notify_status_update с VIRTUAL_CHAMBER target.

Сценарий:
  1) Запустить:  python3 /tmp/fake_moonraker.py
  2) В портале → Moonraker integration: host=<mac IP>, port=7125, enabled:true
  3) ESP подключается → printer.objects.subscribe → SDK.onVirtualChamberUpdate
     → auto_heat → s_output (RMT pulse).
  4) Логи в Serial:
       [HEATER] VIRTUAL_CHAMBER: ... target=70.0 → output=ON
       [RMT→status] mode=Drying target=70.0
"""
import asyncio, json, logging, sys

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("fake-moonraker")

try:
    import websockets
except ImportError:
    print("pip3 install websockets", file=sys.stderr)
    sys.exit(1)

# Текущее значение VIRTUAL_CHAMBER target (°C).
chamber_target = 0.0
clients = set()


async def broadcast_status(target_temp: float):
    """Шлём notify_status_update всем подписчикам."""
    msg = {
        "jsonrpc": "2.0",
        "method": "notify_status_update",
        "params": [
            {
                "gcode_macro VIRTUAL_CHAMBER": {
                    "target":      target_temp,
                    "temperature": -1,
                    "has_sensor":  0,
                },
            },
            # eventtime
            123456.0,
        ],
    }
    payload = json.dumps(msg)
    if not clients:
        return
    log.info(f"→ notify VIRTUAL_CHAMBER.target={target_temp}")
    await asyncio.gather(
        *[c.send(payload) for c in clients], return_exceptions=True
    )


async def heartbeat():
    """Лестница температур: 70→65→60→55→50→55→60→65→70 ... по 30 сек на ступень."""
    global chamber_target
    pattern = [70.0, 65.0, 60.0, 55.0, 50.0, 55.0, 60.0, 65.0]
    idx = 0
    while True:
        chamber_target = pattern[idx % len(pattern)]
        await broadcast_status(chamber_target)
        idx += 1
        await asyncio.sleep(30)


async def handle(ws):
    clients.add(ws)
    peer = ws.remote_address
    log.info(f"+ client {peer}")
    try:
        async for raw in ws:
            try:
                msg = json.loads(raw)
            except Exception:
                log.warning(f"bad JSON: {raw[:120]}")
                continue

            method = msg.get("method", "?")
            mid = msg.get("id")
            log.info(f"← {method} id={mid}")

            # JSON-RPC reply (только если есть id).
            result = {}
            if method == "server.connection.identify":
                result = {"connection_id": 42}
            elif method == "server.info":
                result = {
                    "klippy_connected": True,
                    "klippy_state": "ready",
                    "moonraker_version": "fake-1.0.0",
                }
            elif method == "printer.objects.list":
                result = {"objects": ["gcode_macro VIRTUAL_CHAMBER"]}
            elif method == "printer.objects.subscribe":
                # Сразу отдаём текущее состояние + статус.
                result = {
                    "eventtime": 123456.0,
                    "status": {
                        "gcode_macro VIRTUAL_CHAMBER": {
                            "target":      chamber_target,
                            "temperature": -1,
                            "has_sensor":  0,
                        }
                    },
                }
            else:
                result = {"ok": True}

            if mid is not None:
                resp = {"jsonrpc": "2.0", "id": mid, "result": result}
                await ws.send(json.dumps(resp))

            # После subscribe — сразу пушим notify, чтобы ESP отработал.
            if method == "printer.objects.subscribe":
                await broadcast_status(chamber_target)
    except websockets.ConnectionClosed:
        pass
    finally:
        clients.discard(ws)
        log.info(f"- client {peer}")


async def main():
    log.info("listening on 0.0.0.0:7125 (path=/websocket)")
    log.info("press 't' + Enter in another shell? — see heartbeat (auto 10s flip 0↔70)")
    async with websockets.serve(handle, "0.0.0.0", 7125):
        await heartbeat()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
