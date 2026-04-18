"""
Post-upload script for STAGE environments: auto-claim device on staging backend.

Flow:
1. Opens serial port after upload
2. Waits for CLAIM_PIN:<pin>:<expires> from firmware
3. Logs into staging backend (POST /auth/login)
4. Claims device with PIN (POST /devices/claim)
5. Prints result and exits (pio monitor takes over)

Usage:
  pio run -e esp32c3-super-mini-stage -t upload
  # Script runs automatically after upload, claims device, then exits.
  # Run `pio run -e ... -t monitor` separately to see telemetry.

Config (environment variables, all optional):
  STAGING_EMAIL    - staging account email    (default: pavluchenko.r@gmail.com)
  STAGING_PASSWORD - staging account password (default: staging123)
  STAGING_API_URL  - staging API base URL     (default: https://staging.idryer.org/api)
"""

import os
import sys
import time
import json
import serial
import urllib.request
import urllib.error
import ssl

Import("env")

# ANSI
GREEN = '\033[92m'
YELLOW = '\033[93m'
CYAN = '\033[96m'
RED = '\033[91m'
DIM = '\033[2m'
RESET = '\033[0m'

STAGING_EMAIL = os.getenv("STAGING_EMAIL", "pavluchenko.r@gmail.com")
STAGING_PASSWORD = os.getenv("STAGING_PASSWORD", "staging123")
STAGING_API_URL = os.getenv("STAGING_API_URL", "https://staging.idryer.org/api")

SERIAL_TIMEOUT = 90
CLAIM_DEVICE_NAME = "Stage Test Device"

_env_name = env.subst("$PIOENV")
_monitor_port = env.subst("$MONITOR_PORT") or env.subst("$UPLOAD_PORT")
_monitor_speed = int(env.subst("$MONITOR_SPEED") or "115200")

# Skip for non-stage environments
if "stage" not in _env_name:
    pass
else:

    def _api_request(path, data=None, token=None):
        """Simple HTTP request using only stdlib (no requests dependency)."""
        url = f"{STAGING_API_URL}{path}"
        headers = {"Content-Type": "application/json"}
        if token:
            headers["Authorization"] = f"Bearer {token}"

        ctx = ssl.create_default_context()
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE

        body = json.dumps(data).encode() if data else None
        req = urllib.request.Request(url, data=body, headers=headers)

        try:
            with urllib.request.urlopen(req, context=ctx, timeout=10) as resp:
                return json.loads(resp.read().decode())
        except urllib.error.HTTPError as e:
            error_body = e.read().decode() if e.fp else ""
            print(f"  {RED}HTTP {e.code}: {error_body}{RESET}")
            return None
        except Exception as e:
            print(f"  {RED}Request failed: {e}{RESET}")
            return None

    def _login_staging():
        """Login to staging backend, return JWT access token."""
        print(f"  {DIM}POST /auth/login ({STAGING_EMAIL}){RESET}")
        result = _api_request("/auth/login", {
            "email": STAGING_EMAIL,
            "password": STAGING_PASSWORD,
        })
        if result and "accessToken" in result:
            return result["accessToken"]
        return None

    def _claim_device(pin, token):
        """Claim device by PIN using JWT token."""
        print(f"  {DIM}POST /devices/claim (PIN={pin}){RESET}")
        return _api_request("/devices/claim", {
            "pin": pin,
            "name": CLAIM_DEVICE_NAME,
        }, token=token)

    def stage_auto_claim(source, target, env):
        """Post-upload action: monitor serial, catch PIN, auto-claim."""
        if "stage" not in _env_name:
            return

        if not _monitor_port:
            print(f"  {YELLOW}[AUTO-CLAIM] No serial port configured, skipping{RESET}")
            return

        print()
        print(f"  {CYAN}{'=' * 50}{RESET}")
        print(f"  {CYAN}  STAGE AUTO-CLAIM: {_env_name}{RESET}")
        print(f"  {CYAN}  Port: {_monitor_port} @ {_monitor_speed}{RESET}")
        print(f"  {CYAN}  Waiting for CLAIM_PIN from firmware...{RESET}")
        print(f"  {CYAN}  (timeout: {SERIAL_TIMEOUT}s, Ctrl+C to skip){RESET}")
        print(f"  {CYAN}{'=' * 50}{RESET}")
        print()

        pin = None
        already_claimed = False

        try:
            time.sleep(2)

            ser = serial.Serial(_monitor_port, _monitor_speed, timeout=1)
            start = time.time()
            claim_sent = False
            wifi_ready = False

            while time.time() - start < SERIAL_TIMEOUT:
                if ser.in_waiting > 0:
                    try:
                        line = ser.readline().decode("utf-8", errors="replace").strip()
                    except Exception:
                        continue

                    if not line:
                        continue

                    print(f"  {DIM}[SERIAL] {line}{RESET}")

                    if "Logs enabled after WiFi config" in line:
                        wifi_ready = True

                    if wifi_ready and not claim_sent and "NOT claimed" in line:
                        print(f"\n  {CYAN}[AUTO-CLAIM] Sending START_CLAIM...{RESET}")
                        ser.write(b"START_CLAIM\n")
                        ser.flush()
                        claim_sent = True

                    if line.startswith("CLAIM_PIN:"):
                        parts = line.split(":")
                        if len(parts) >= 2:
                            pin = parts[1]
                            expires = parts[2] if len(parts) >= 3 else "?"
                            print(f"\n  {GREEN}[AUTO-CLAIM] Got PIN: {pin} (expires: {expires}s){RESET}")
                            break

                    if line.startswith("CLAIM_ALREADY:"):
                        serial_num = line.split(":")[1] if ":" in line else "?"
                        print(f"\n  {GREEN}[AUTO-CLAIM] Device already claimed (serial={serial_num}){RESET}")
                        already_claimed = True
                        break

                    if "Device claimed!" in line:
                        print(f"\n  {GREEN}[AUTO-CLAIM] Device already claimed by another session{RESET}")
                        already_claimed = True
                        break

            ser.close()

        except KeyboardInterrupt:
            print(f"\n  {YELLOW}[AUTO-CLAIM] Skipped by user{RESET}")
            return
        except serial.SerialException as e:
            print(f"  {YELLOW}[AUTO-CLAIM] Serial error: {e}{RESET}")
            print(f"  {YELLOW}[AUTO-CLAIM] Claim manually or re-run{RESET}")
            return

        if already_claimed:
            print(f"  {GREEN}[AUTO-CLAIM] Nothing to do, device is already claimed.{RESET}")
            return

        if not pin:
            print(f"  {YELLOW}[AUTO-CLAIM] No PIN received within {SERIAL_TIMEOUT}s{RESET}")
            print(f"  {YELLOW}[AUTO-CLAIM] Possible causes:{RESET}")
            print(f"  {YELLOW}  - WiFi not configured (run Improv first){RESET}")
            print(f"  {YELLOW}  - RP2040 not connected (no serial number){RESET}")
            print(f"  {YELLOW}  - staging.idryer.org API unreachable{RESET}")
            return

        # Login to staging
        print(f"\n  {CYAN}[AUTO-CLAIM] Logging into staging backend...{RESET}")
        jwt = _login_staging()
        if not jwt:
            print(f"  {RED}[AUTO-CLAIM] Login failed! Claim manually at staging.idryer.org{RESET}")
            print(f"  {RED}[AUTO-CLAIM] PIN: {pin}{RESET}")
            return

        print(f"  {GREEN}[AUTO-CLAIM] Login OK{RESET}")

        # Claim
        print(f"  {CYAN}[AUTO-CLAIM] Claiming device with PIN {pin}...{RESET}")
        result = _claim_device(pin, jwt)
        if result and result.get("claimed"):
            device_id = result.get("deviceId", "?")
            print(f"\n  {GREEN}{'=' * 50}{RESET}")
            print(f"  {GREEN}  Device claimed successfully!{RESET}")
            print(f"  {GREEN}  deviceId: {device_id}{RESET}")
            print(f"  {GREEN}  MQTT should connect shortly.{RESET}")
            print(f"  {GREEN}  Run: pio run -e {_env_name} -t monitor{RESET}")
            print(f"  {GREEN}{'=' * 50}{RESET}")
        else:
            print(f"  {RED}[AUTO-CLAIM] Claim failed!{RESET}")
            print(f"  {RED}[AUTO-CLAIM] Try manually at staging.idryer.org, PIN: {pin}{RESET}")

    env.AddPostAction("upload", stage_auto_claim)
