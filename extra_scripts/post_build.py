# pyright: reportMissingImports=false
"""
Post-build script: copies firmware to local folder and flasher-portal.

How flasher-portal copy works:
- IDRYER_FLASHER_PORTAL_PATH must point to the flasher-portal directory itself.
  Example: /Users/ruslanpavlucenko/Projects/iDryerPortal/flasher-portal
- Local firmware/<board>/ is updated on every build.
- flasher-portal is updated only from main/master when the current git HEAD is
  exactly on tag vX.X.X and the tag matches VERSION_STR in src/version.h.

flasher-portal layout:
  firmware/storage-link/prod/<board>/v<version>/{firmware,bootloader,partitions,boot_app0}.bin
  manifests/storage-link/<board>/v<version>.json
  firmware/versions.json
"""

import json
import os
import re
import shutil
import subprocess
from pathlib import Path

Import("env")

GREEN = "\033[92m"
BLUE = "\033[94m"
YELLOW = "\033[93m"
RESET = "\033[0m"

PRODUCT_NAME = "iheater-link"
PRODUCT_LABEL = "iHeater Link"

BOOT_APP0_SRC = Path.home() / ".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"

_env_name = env.subst("$PIOENV")
_build_dir = Path(env.subst("$BUILD_DIR"))
_proj_dir = Path(env.subst("$PROJECT_DIR"))
_board_mcu = env.subst("$BOARD_MCU").lower()


def _chip_family() -> str:
    if "esp32s3" in _board_mcu:
        return "ESP32-S3"
    if "esp32s2" in _board_mcu:
        return "ESP32-S2"
    if "esp32c3" in _board_mcu:
        return "ESP32-C3"
    return "ESP32"


def _board_name() -> str:
    name = _env_name
    if name.endswith("-prod"):
        name = name[:-5]
    elif name.endswith("-stage"):
        name = name[:-6]
    return name


def _board_label(board: str) -> str:
    labels = {
        "esp32c3-devkitm": "DevKit M",
        "esp32c3-super-mini": "Super Mini",
        "xiao-esp32s3": "XIAO ESP32-S3",
        "waveshare-esp32s3-zero": "Waveshare ESP32-S3 Zero",
    }
    suffix = labels.get(board, board)
    return f"{PRODUCT_LABEL} ({suffix})"


def _idryer_flasher_portal_root() -> str | None:
    raw = os.environ.get("IDRYER_FLASHER_PORTAL_PATH")
    if not raw:
        env_dict = env.get("ENV")
        if isinstance(env_dict, dict):
            raw = env_dict.get("IDRYER_FLASHER_PORTAL_PATH")
    if not raw:
        return None
    raw = str(raw).strip().strip('"').strip("'")
    return raw or None


def _portal_path_hint(portal_root: str) -> str | None:
    if "…" in portal_root or "⋯" in portal_root:
        return "path contains Unicode ellipsis; use the real full path"
    if "/.../" in portal_root or portal_root.endswith("/...") or portal_root.startswith(".../"):
        return "path contains literal ...; use the real full path"
    if os.path.basename(portal_root.rstrip(os.sep)) != "flasher-portal":
        return "IDRYER_FLASHER_PORTAL_PATH must point to flasher-portal, not its parent"
    return None


def _run_git(args) -> str | None:
    try:
        return subprocess.check_output(
            ["git", *args], cwd=str(_proj_dir), text=True, stderr=subprocess.DEVNULL
        ).strip()
    except Exception:
        return None


def _current_branch() -> str:
    return _run_git(["rev-parse", "--abbrev-ref", "HEAD"]) or "unknown"


def _release_tag_for_version(version: str) -> str | None:
    tags_raw = _run_git(["tag", "--points-at", "HEAD"]) or ""
    tags = [t.strip() for t in tags_raw.splitlines() if t.strip()]
    expected = f"v{version}"
    if expected in tags and re.fullmatch(r"v\d+\.\d+\.\d+", expected):
        return expected
    return None


def _extract_version() -> str:
    ver_hdr = _proj_dir / "src" / "version.h"
    try:
        txt = ver_hdr.read_text(encoding="utf-8")
    except Exception:
        return "0.0.0"
    m = re.search(r'#\s*define\s+VERSION_STR\s+"([^"]+)"', txt)
    if m:
        return m.group(1).strip()
    major = re.search(r"#\s*define\s+VERSION_MAJOR\s+(\d+)", txt)
    minor = re.search(r"#\s*define\s+VERSION_MINOR\s+(\d+)", txt)
    patch = re.search(r"#\s*define\s+VERSION_PATCH\s+(\d+)", txt)
    if major and minor and patch:
        return f"{major.group(1)}.{minor.group(1)}.{patch.group(1)}"
    return "0.0.0"


def _write_json(path: str, data: dict):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(data, fh, ensure_ascii=False, indent=2)
        fh.write("\n")


def _make_manifest(board: str, version: str, slot: str) -> dict:
    base = f"/firmware/{PRODUCT_NAME}/{slot}/{board}/v{version}"
    return {
        "name": _board_label(board),
        "version": version,
        "funding_url": "https://portal.idryer.org",
        "builds": [
            {
                "chipFamily": _chip_family(),
                "improv": True,
                "parts": [
                    {"path": f"{base}/bootloader.bin", "offset": 0},
                    {"path": f"{base}/partitions.bin", "offset": 32768},
                    {"path": f"{base}/boot_app0.bin", "offset": 57344},
                    {"path": f"{base}/firmware.bin", "offset": 65536},
                ],
            }
        ],
    }


def _update_versions_json(portal_root: str, board: str, version: str, manifest_rel: str):
    versions_path = os.path.join(portal_root, "firmware", "versions.json")
    if os.path.exists(versions_path):
        try:
            with open(versions_path, "r", encoding="utf-8") as fh:
                data = json.load(fh)
        except Exception:
            data = {}
    else:
        data = {}
    data.setdefault("schema", 1)
    data.setdefault("links", {})
    data.setdefault("controller", {})

    product = data["links"].setdefault(PRODUCT_NAME, {"label": PRODUCT_LABEL, "boards": {}})
    boards = product.setdefault("boards", {})
    entries = [e for e in boards.get(board, []) if e.get("version") != version]
    entries.insert(0, {
        "version": version,
        "protocolMajor": int(version.split(".")[0]),
        "manifest": manifest_rel,
        "recommended": True,
    })
    for e in entries[1:]:
        e["recommended"] = False
    boards[board] = entries
    _write_json(versions_path, data)


def copy_firmware(source, target, env):
    version = _extract_version()
    board = _board_name()
    branch = _current_branch()
    release_tag = _release_tag_for_version(version)

    slot = "stage" if _env_name.endswith("-stage") else "prod"

    files = {
        "firmware.bin":    _build_dir / "firmware.bin",
        "bootloader.bin":  _build_dir / "bootloader.bin",
        "partitions.bin":  _build_dir / "partitions.bin",
        "boot_app0.bin":   BOOT_APP0_SRC,
    }

    print(f"  {BLUE}[FIRMWARE] {_env_name} — version {version}, branch {branch}, tag {release_tag or 'none'}{RESET}")

    # Локальная копия
    local_dest = _proj_dir / "firmware" / board
    local_dest.mkdir(parents=True, exist_ok=True)
    for filename, src in files.items():
        if src.exists():
            shutil.copy2(src, local_dest / filename)
        else:
            print(f"  {YELLOW}[FIRMWARE] WARNING: {src} not found, skipping{RESET}")
    print(f"  [FIRMWARE] → {local_dest}")

    # Flasher-portal
    flasher_portal_allowed = branch in ("main", "master")
    if not flasher_portal_allowed:
        print(f"  {YELLOW}[FIRMWARE] branch '{branch}' is not release → flasher-portal skipped{RESET}")
        print(f"  {GREEN}[FIRMWARE] ✅ {_env_name} local done{RESET}")
        return

    if not release_tag:
        print(f"  {YELLOW}[FIRMWARE] no tag v{version} on HEAD → flasher-portal skipped{RESET}")
        print(f"  {GREEN}[FIRMWARE] ✅ {_env_name} local done{RESET}")
        return

    portal_root = _idryer_flasher_portal_root()
    if not portal_root:
        print(f"  {YELLOW}[FIRMWARE] IDRYER_FLASHER_PORTAL_PATH not set → flasher-portal skipped{RESET}")
        print(f"  {GREEN}[FIRMWARE] ✅ {_env_name} local done{RESET}")
        return
    if not os.path.isdir(portal_root):
        print(f"  {YELLOW}[FIRMWARE] IDRYER_FLASHER_PORTAL_PATH is not a directory: {portal_root!r}{RESET}")
        print(f"  {GREEN}[FIRMWARE] ✅ {_env_name} local done{RESET}")
        return
    hint = _portal_path_hint(portal_root)
    if hint:
        print(f"  {YELLOW}[FIRMWARE] Hint: {hint}{RESET}")
        print(f"  {GREEN}[FIRMWARE] ✅ {_env_name} local done{RESET}")
        return

    # Копируем в версионную папку
    portal_fw_dest = Path(portal_root) / "firmware" / PRODUCT_NAME / slot / board / f"v{version}"
    portal_fw_dest.mkdir(parents=True, exist_ok=True)
    for filename, src in files.items():
        if src.exists():
            shutil.copy2(src, portal_fw_dest / filename)
    print(f"  [FIRMWARE] → {portal_fw_dest}")

    # Манифест
    manifest_rel = f"manifests/{PRODUCT_NAME}/{board}/v{version}.json"
    manifest_path = os.path.join(portal_root, manifest_rel)
    _write_json(manifest_path, _make_manifest(board, version, slot))
    print(f"  [FIRMWARE] manifest → {manifest_path}")

    # versions.json
    _update_versions_json(portal_root, board, version, manifest_rel)
    print(f"  [FIRMWARE] versions.json updated")

    print(f"  {GREEN}[FIRMWARE] ✅ {_env_name} local + flasher-portal done{RESET}")


env.AddPostAction("$BUILD_DIR/firmware.bin", copy_firmware)
