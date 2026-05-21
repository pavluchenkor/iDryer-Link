#!/usr/bin/env python3
"""
Fake Bambu publisher — шлёт report-payloads в device/{SERIAL}/report.

Эмулирует реальный сценарий Bambu без датчика камеры (P1S/A1):
chamber_target всегда 0 — целевая температура камеры берётся из типа
филамента активного трея AMS, через menu.mat_<type> на iHeater-link.

Лестница по 15 сек:
  PREPARE (PLA) → RUNNING ams (PLA) → RUNNING vt_tray (PETG)
                → RUNNING ams_ht (PA-CF) → FINISH (пусто) → ...

Состояния gcode_state:
  PREPARE — принтер готовится (homing/прогрев), tray уже выбран
  RUNNING — печать идёт
  FINISH  — печать завершена, трей выгружен (tray_now="255")

ESP-side auto_heat включает нагрев при PREPARE и RUNNING, выключает
при FINISH (см. iHeater-link/src/heater/auto_heat.cpp::bambuShouldHeat).

Зависимости: pip3 install --user paho-mqtt
"""
import json, ssl, sys, time, logging, os

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("fake-bambu")

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("pip3 install --user paho-mqtt", file=sys.stderr)
    sys.exit(1)

BROKER         = os.environ.get("FAKE_BAMBU_HOST", "127.0.0.1")
PORT           = int(os.environ.get("FAKE_BAMBU_PORT", "8883"))
SERIAL         = os.environ.get("FAKE_BAMBU_SERIAL", "FAKE_BAMBU_001")
LAN_CODE       = os.environ.get("FAKE_BAMBU_LAN", "12345678")
CERT_PATH      = os.environ.get("FAKE_BAMBU_CERT",
                                os.path.join(os.path.dirname(__file__), "cert.pem"))
# Лестница: каждый источник филамента → свой тип, по 15 сек.
#
#   ("ams",      "PLA")    — обычный AMS slot 0 (tray_now="0")
#   ("vt_tray",  "PETG")   — внешний держатель катушки (tray_now="254")
#   ("ams_ht",   "PA-CF")  — AMS HT (tray_now="128"), полиамиды
#   ("finish",   None)     — печать завершена, трей выгружен (tray_now="255")
PATTERN = [
    ("prepare", "PLA"),
    ("ams",     "PLA"),
    ("vt_tray", "PETG"),
    ("ams_ht",  "PA-CF"),
    ("finish",  None),
]
STEP_SECONDS = 15

REPORT_TOPIC  = f"device/{SERIAL}/report"
REQUEST_TOPIC = f"device/{SERIAL}/request"

# Текущее состояние лестницы — нужно для ответа на pushall.
current_index = 0


def make_payload(source: str, tray_type):
    """source: 'prepare' | 'ams' | 'vt_tray' | 'ams_ht' | 'finish'."""
    if source == "prepare":
        # Принтер готовится к печати: tray уже выбран, прогрев/homing.
        # ESP должен включить нагрев камеры (auto_heat: PREPARE → heat).
        return {
            "print": {
                "command":     "push_status",
                "gcode_state": "PREPARE",
                "mc_percent":  0,
                "ams": {
                    "tray_now": "0",
                    "ams": [{
                        "id": "0",
                        "tray": [
                            {"id": "0", "tray_type": tray_type, "tray_info_idx": "GFA00"},
                            {"id": "1"},
                            {"id": "2"},
                            {"id": "3"},
                        ],
                    }],
                },
            }
        }

    if source == "ams":
        # tray_now="0" → ams_index = 0>>2 = 0, tray_index = 0&3 = 0
        return {
            "print": {
                "command":     "push_status",
                "gcode_state": "RUNNING",
                "mc_percent":  42,
                "ams": {
                    "tray_now": "0",
                    "ams": [{
                        "id": "0",
                        "tray": [
                            {"id": "0", "tray_type": tray_type, "tray_info_idx": "GFA00"},
                            {"id": "1"},
                            {"id": "2"},
                            {"id": "3"},
                        ],
                    }],
                },
            }
        }

    if source == "vt_tray":
        # tray_now="254" → внешняя катушка, tray_type из vt_tray
        return {
            "print": {
                "command":     "push_status",
                "gcode_state": "RUNNING",
                "mc_percent":  42,
                "ams": {
                    "tray_now": "254",
                    "vt_tray":  {"tray_type": tray_type, "tray_info_idx": "GFB99"},
                    "ams":      [],
                },
            }
        }

    if source == "ams_ht":
        # tray_now>=80 → AMS HT, ams_index = tray_now (128), tray_index = 0
        return {
            "print": {
                "command":     "push_status",
                "gcode_state": "RUNNING",
                "mc_percent":  42,
                "ams": {
                    "tray_now": "128",
                    "ams": [{
                        "id": "128",
                        "tray": [
                            {"id": "0", "tray_type": tray_type, "tray_info_idx": "GFN05"},
                        ],
                    }],
                },
            }
        }

    # finish
    return {
        "print": {
            "command":     "push_status",
            "gcode_state": "FINISH",
            "ams": {"tray_now": "255"},
        }
    }


def main():
    log.info(f"broker={BROKER}:{PORT} serial={SERIAL} lan={LAN_CODE}")
    log.info(f"topic={REPORT_TOPIC}")
    log.info(f"pattern={PATTERN} step={STEP_SECONDS}s")

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                         client_id=f"fake-bambu-publisher-{os.getpid()}")
    client.username_pw_set("bblp", LAN_CODE)

    if os.path.exists(CERT_PATH):
        client.tls_set(ca_certs=CERT_PATH,
                       tls_version=ssl.PROTOCOL_TLS_CLIENT,
                       cert_reqs=ssl.CERT_NONE)
    else:
        log.warning(f"cert {CERT_PATH} not found — using TLS without CA")
        client.tls_set(tls_version=ssl.PROTOCOL_TLS_CLIENT,
                       cert_reqs=ssl.CERT_NONE)
    client.tls_insecure_set(True)

    def on_connect(c, u, f, rc, p):
        log.info(f"connected rc={rc}")
        # Слушаем команды от ESP — Bambu принтер делает то же.
        c.subscribe(REQUEST_TOPIC, qos=0)
        log.info(f"subscribed to {REQUEST_TOPIC}")

    def on_message(c, u, msg):
        try:
            payload = json.loads(msg.payload.decode("utf-8", "replace"))
        except Exception as e:
            log.warning(f"req parse err: {e}")
            return
        log.info(f"← request: {payload}")
        # Реальный Bambu по pushall шлёт полный initial snapshot.
        # У нас это — текущий шаг лестницы.
        pushing = payload.get("pushing", {})
        if pushing.get("command") == "pushall":
            source, tray_type = PATTERN[current_index % len(PATTERN)]
            snap = json.dumps(make_payload(source, tray_type))
            c.publish(REPORT_TOPIC, snap, qos=0, retain=True)
            log.info(f"→ pushall reply: source={source} tray_type={tray_type or '<empty>'}")

    client.on_connect    = on_connect
    client.on_disconnect = lambda c, u, f, rc, p: log.warning(f"disconnected rc={rc}")
    client.on_message    = on_message

    client.connect(BROKER, PORT, keepalive=60)
    client.loop_start()

    try:
        global current_index
        while True:
            source, tray_type = PATTERN[current_index % len(PATTERN)]
            payload = json.dumps(make_payload(source, tray_type))
            client.publish(REPORT_TOPIC, payload, qos=0, retain=True)
            log.info(f"→ source={source:8s} tray_type={tray_type or '<empty>'}")
            current_index += 1
            time.sleep(STEP_SECONDS)
    except KeyboardInterrupt:
        pass
    finally:
        client.loop_stop()
        client.disconnect()


if __name__ == "__main__":
    main()
