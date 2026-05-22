"""
Post-build script: copies ESP32 firmware artifacts.

How flasher-portal copy works:
- IDRYER_FLASHER_PORTAL_PATH must point to the flasher-portal directory itself.
  Example: /Users/ruslanpavlucenko/Projects/iDryerPortal/flasher-portal
- Local firmware/ is updated on every build.
- flasher-portal is updated only from main/master when the current git HEAD is
  exactly on tag vX.X.X and the tag matches firmware VERSION_STR.

flasher-portal layout:
  firmware/link/<slot>/<board>/vX.X.X/{bootloader.bin,partitions.bin,boot_app0.bin,firmware.bin}
  manifests/link/<board>/vX.X.X.json
  firmware/versions.json
"""

import json
import os
import re
import shutil
import subprocess
from pathlib import Path
from typing import Optional

Import("env")

GREEN = "\033[92m"
BLUE = "\033[94m"
YELLOW = "\033[93m"
RESET = "\033[0m"

PRODUCT_KEY = "link"
PRODUCT_LABEL = "iDryer Link"
FUNDING_URL = "https://portal.idryer.org"

PROJECT_DIR = Path(env["PROJECT_DIR"])
LOCAL_FIRMWARE_DIR = PROJECT_DIR / "firmware"
BOOT_APP0_SRC = Path.home() / ".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"

_env_name = env.subst("$PIOENV")
_build_dir = Path(env.subst("$BUILD_DIR"))


def _idryer_flasher_portal_root(scons_env) -> Optional[Path]:
    raw = os.environ.get("IDRYER_FLASHER_PORTAL_PATH")
    if not raw:
        env_dict = scons_env.get("ENV")
        if isinstance(env_dict, dict):
            raw = env_dict.get("IDRYER_FLASHER_PORTAL_PATH")
    if not raw:
        return None
    return Path(str(raw).strip().strip('"').strip("'"))


def _portal_path_hint(portal_root: Path) -> Optional[str]:
    raw = str(portal_root)
    if "\u2026" in raw or "\u22ef" in raw:
        return "path contains Unicode ellipsis; use the real full path"
    if "/.../" in raw or raw.endswith("/...") or raw.startswith(".../"):
        return "path contains literal ...; use the real full path"
    if portal_root.name != "flasher-portal":
        return "IDRYER_FLASHER_PORTAL_PATH must point to flasher-portal, not iDryerPortal"
    return None


def _run_git(args) -> Optional[str]:
    try:
        return subprocess.check_output(["git", *args], cwd=PROJECT_DIR, text=True, stderr=subprocess.DEVNULL).strip()
    except Exception:
        return None


def _current_branch() -> str:
    return _run_git(["rev-parse", "--abbrev-ref", "HEAD"]) or "unknown"


def _release_tag_for_version(version: str) -> Optional[str]:
    tags_raw = _run_git(["tag", "--points-at", "HEAD"]) or ""
    tags = [line.strip() for line in tags_raw.splitlines() if line.strip()]
    expected = f"v{version}"
    if expected in tags and re.fullmatch(r"v\d+\.\d+\.\d+", expected):
        return expected
    return None


def _extract_defines(path: Path) -> dict:
    try:
        text = path.read_text(encoding="utf-8")
    except Exception:
        return {}
    out = {}
    for key in ("VERSION_MAJOR", "VERSION_MINOR", "VERSION_PATCH"):
        match = re.search(rf"#\s*define\s+{key}\s+(\d+)", text)
        if match:
            out[key] = int(match.group(1))
    match = re.search(r'#\s*define\s+VERSION_STR\s+"([^"]+)"', text)
    if match:
        out["VERSION_STR"] = match.group(1).strip()
    return out


def _firmware_version() -> str:
    own = _extract_defines(PROJECT_DIR / "src" / "version.h")
    menu = _extract_defines(PROJECT_DIR / "lib" / "idryer-menu" / "src" / "version.h")
    major = menu.get("VERSION_MAJOR", own.get("VERSION_MAJOR", 0))
    minor = own.get("VERSION_MINOR", 0)
    patch = own.get("VERSION_PATCH", 0)
    return f"{major}.{minor}.{patch}"


def _chip_family(board_name: str) -> str:
    if "esp32s3" in board_name:
        return "ESP32-S3"
    return "ESP32-C3"


def _board_display_name(board_name: str) -> str:
    return {
        "esp32c3": "ESP32-C3 DevKit",
        "esp32c3-super-mini": "Super Mini",
        "xiao-esp32s3": "Seeed XIAO ESP32-S3",
        "waveshare-esp32s3-zero": "Waveshare ESP32-S3-Zero",
    }.get(board_name, board_name)


def _load_versions(path: Path) -> dict:
    if not path.exists():
        return {"schema": 1, "links": {}, "controller": {}}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        data = {}
    data.setdefault("schema", 1)
    data.setdefault("links", {})
    data.setdefault("controller", {})
    return data


def _write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def _update_versions_json(portal_root: Path, board_name: str, version: str, manifest_rel: str) -> None:
    versions_path = portal_root / "firmware" / "versions.json"
    data = _load_versions(versions_path)
    product = data["links"].setdefault(PRODUCT_KEY, {"label": PRODUCT_LABEL, "boards": {}})
    product.setdefault("label", PRODUCT_LABEL)
    boards = product.setdefault("boards", {})
    entries = [item for item in boards.get(board_name, []) if item.get("version") != version]
    entries.insert(0, {
        "version": version,
        "protocolMajor": int(version.split(".")[0]),
        "manifest": manifest_rel,
        "recommended": True,
    })
    for item in entries[1:]:
        item["recommended"] = False
    boards[board_name] = entries
    _write_json(versions_path, data)


def _write_manifest(portal_root: Path, slot: str, board_name: str, version: str) -> str:
    manifest_rel = f"manifests/{PRODUCT_KEY}/{board_name}/v{version}.json"
    firmware_base = f"firmware/{PRODUCT_KEY}/{slot}/{board_name}/v{version}"
    manifest = {
        "name": f"{PRODUCT_LABEL} ({_board_display_name(board_name)})",
        "version": version,
        "funding_url": FUNDING_URL,
        "builds": [{
            "chipFamily": _chip_family(board_name),
            "improv": True,
            "parts": [
                {"path": f"{firmware_base}/bootloader.bin", "offset": 0},
                {"path": f"{firmware_base}/partitions.bin", "offset": 32768},
                {"path": f"{firmware_base}/boot_app0.bin", "offset": 57344},
                {"path": f"{firmware_base}/firmware.bin", "offset": 65536},
            ],
        }],
    }
    _write_json(portal_root / manifest_rel, manifest)
    return manifest_rel


def _env_slot_and_board() -> tuple[str, str]:
    if _env_name.endswith("-prod"):
        return "prod", _env_name[:-5]
    if _env_name.endswith("-stage"):
        return "stage", _env_name[:-6]
    return "prod", _env_name


def copy_firmware(source, target, env):
    version = _firmware_version()
    release_tag = _release_tag_for_version(version)
    branch = _current_branch()
    slot, board_name = _env_slot_and_board()

    files = {
        "firmware.bin": _build_dir / "firmware.bin",
        "bootloader.bin": _build_dir / "bootloader.bin",
        "partitions.bin": _build_dir / "partitions.bin",
        "boot_app0.bin": BOOT_APP0_SRC,
    }

    local_dest = LOCAL_FIRMWARE_DIR / board_name
    local_dest.mkdir(parents=True, exist_ok=True)

    print(f"  {BLUE}[FIRMWARE] Copying {_env_name} firmware v{version}...{RESET}")
    print(f"  [FIRMWARE] git branch: {branch}")
    print(f"  [FIRMWARE] release tag: {release_tag or 'none'}")

    for filename, src in files.items():
        if not src.exists():
            print(f"  {YELLOW}[FIRMWARE] WARNING: {src} not found, skipping {filename}{RESET}")
            continue
        shutil.copy2(src, local_dest / filename)
        print(f"  [FIRMWARE] {filename} → {local_dest / filename}")

    portal_root = _idryer_flasher_portal_root(env)
    if not portal_root:
        print(f"  {YELLOW}[FIRMWARE] IDRYER_FLASHER_PORTAL_PATH is not set → flasher-portal skipped{RESET}")
        print(f"  {GREEN}[FIRMWARE] ✅ local firmware updated{RESET}")
        return
    if not portal_root.is_dir():
        print(f"  {YELLOW}[FIRMWARE] WARNING: IDRYER_FLASHER_PORTAL_PATH is not a directory: {portal_root}{RESET}")
        hint = _portal_path_hint(portal_root)
        if hint:
            print(f"  {YELLOW}[FIRMWARE] Hint: {hint}{RESET}")
        print(f"  {GREEN}[FIRMWARE] ✅ local firmware updated{RESET}")
        return
    hint = _portal_path_hint(portal_root)
    if hint:
        print(f"  {YELLOW}[FIRMWARE] Hint: {hint}{RESET}")
        print(f"  {GREEN}[FIRMWARE] ✅ local firmware updated{RESET}")
        return
    if branch not in ("main", "master"):
        print(
            f"  {YELLOW}[FIRMWARE] skip flasher-portal: release copy requires main/master, "
            f"current branch is {branch}{RESET}"
        )
        print(f"  {GREEN}[FIRMWARE] ✅ local firmware updated{RESET}")
        return
    if not release_tag:
        print(
            f"  {YELLOW}[FIRMWARE] skip flasher-portal: release copy requires exact tag v{version} "
            f"on current HEAD{RESET}"
        )
        print(f"  {GREEN}[FIRMWARE] ✅ local firmware updated{RESET}")
        return

    flasher_dest = portal_root / "firmware" / PRODUCT_KEY / slot / board_name / f"v{version}"
    flasher_dest.mkdir(parents=True, exist_ok=True)

    for filename, src in files.items():
        if not src.exists():
            continue
        shutil.copy2(src, flasher_dest / filename)
        print(f"  [FIRMWARE] {filename} → {flasher_dest / filename}")

    manifest_rel = _write_manifest(portal_root, slot, board_name, version)
    _update_versions_json(portal_root, board_name, version, manifest_rel)
    print(f"  [FIRMWARE] manifest → {portal_root / manifest_rel}")
    print(f"  [FIRMWARE] versions → {portal_root / 'firmware' / 'versions.json'}")
    print(f"  {GREEN}[FIRMWARE] ✅ local + flasher-portal firmware updated{RESET}")


env.AddPostAction("$BUILD_DIR/firmware.bin", copy_firmware)
