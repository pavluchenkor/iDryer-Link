"""
Post-build script: копирует firmware + bootloader + partitions + boot_app0
в /Users/ruslanpavlucenko/Projects/iDryerPortal/flasher-portal/firmware/link/<slot>/<board>/
"""

import shutil
from pathlib import Path

Import("env")

FLASHER_FIRMWARE_DIR = Path("/Users/ruslanpavlucenko/Projects/iDryerPortal/flasher-portal/firmware/link")
BOOT_APP0_SRC = Path.home() / ".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"

# Захватываем переменные из внешнего скоупа пока env доступен
_env_name = env.subst("$PIOENV")
_build_dir = Path(env.subst("$BUILD_DIR"))


def copy_firmware(source, target, env):
    if _env_name.endswith("-prod"):
        slot = "prod"
        board_name = _env_name[:-5]
    elif _env_name.endswith("-stage"):
        slot = "stage"
        board_name = _env_name[:-6]
    else:
        slot = "prod"
        board_name = _env_name

    dest_dir = FLASHER_FIRMWARE_DIR / slot / board_name
    dest_dir.mkdir(parents=True, exist_ok=True)

    files = {
        "firmware.bin":    _build_dir / "firmware.bin",
        "bootloader.bin":  _build_dir / "bootloader.bin",
        "partitions.bin":  _build_dir / "partitions.bin",
        "boot_app0.bin":   BOOT_APP0_SRC,
    }

    for filename, src in files.items():
        if src.exists():
            shutil.copy2(src, dest_dir / filename)
            print(f"  [FIRMWARE] {filename} → {dest_dir / filename}")
        else:
            print(f"  [FIRMWARE] WARNING: {src} not found, skipping")


env.AddPostAction("$BUILD_DIR/firmware.bin", copy_firmware)
