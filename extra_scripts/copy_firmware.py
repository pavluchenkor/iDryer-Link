"""
Post-build script: копирует firmware + bootloader + partitions + boot_app0

НАЗНАЧЕНИЕ:
Автоматически копирует собранную прошивку в две папки после сборки.

ПУТИ НАЗНАЧЕНИЯ:
1. Локальная папка: firmware/<board>/
2. Flasher Portal: /Users/ruslanpavlucenko/Projects/iDryerPortal/flasher-portal/firmware/link/<slot>/<board>/

МАППИНГ ПЛАТ:
- esp32c3-prod → esp32c3 (flasher-portal)
- esp32c3-super-mini-prod → esp32c3-super-mini (flasher-portal)
- xiao-esp32s3-prod → xiao-esp32s3 (flasher-portal)
- waveshare-esp32s3-zero-prod → waveshare-esp32s3-zero (flasher-portal)

ФАЙЛЫ:
- firmware.bin, bootloader.bin, partitions.bin, boot_app0.bin

Копирование в flasher-portal: только OS env IDRYER_FLASHER_PORTAL_PATH (путь к корню репозитория
flasher-portal). Читается при каждом post-action; дополнительно смотрим env['ENV'] у SCons — так же,
как у подпроцессов PlatformIO. Запускайте pio из терминала, где export уже выполнен; кнопка Build
в IDE часто стартует без вашего shell-профиля.

Обновлено: 2026-03-25
"""

import os
import shutil
from pathlib import Path
from typing import Optional

Import("env")

# ANSI цвета для терминала
GREEN = '\033[92m'
BLUE = '\033[94m'
YELLOW = '\033[93m'
RESET = '\033[0m'

# Пути назначения
PROJECT_DIR = Path(env["PROJECT_DIR"])
LOCAL_FIRMWARE_DIR = PROJECT_DIR / "firmware"

BOOT_APP0_SRC = Path.home() / ".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"


def _idryer_flasher_portal_root(scons_env):
    """Только переменная окружения: os.environ, затем SCons ENV (как у дочерних процессов PIO)."""
    raw = os.environ.get("IDRYER_FLASHER_PORTAL_PATH")
    if not raw:
        env_dict = scons_env.get("ENV")
        if isinstance(env_dict, dict):
            raw = env_dict.get("IDRYER_FLASHER_PORTAL_PATH")
    if not raw:
        return None
    raw = str(raw).strip().strip('"').strip("'")
    return raw or None


def _portal_path_bad_placeholder(portal_root: str) -> Optional[str]:
    """Если путь похож на шаблон с многоточием — вернуть подсказку для лога."""
    if "\u2026" in portal_root or "\u22ef" in portal_root:
        return "path contains Unicode ellipsis (…); use the real full path, not a shortened placeholder"
    if "/.../" in portal_root or portal_root.endswith("/...") or portal_root.startswith(".../"):
        return "path contains literal ...; use the real full path"
    return None


# Захватываем переменные из внешнего скоупа пока env доступен
_env_name = env.subst("$PIOENV")
_build_dir = Path(env.subst("$BUILD_DIR"))


def copy_firmware(source, target, env):
    portal_root = _idryer_flasher_portal_root(env)
    flasher_firmware_base = None
    if portal_root:
        pr = Path(portal_root)
        if pr.is_dir():
            flasher_firmware_base = pr / "firmware" / "link"
        else:
            print(
                f"  {YELLOW}[FIRMWARE] WARNING: IDRYER_FLASHER_PORTAL_PATH is not a directory: "
                f"{portal_root!r}{RESET}"
            )
            hint = _portal_path_bad_placeholder(portal_root)
            if hint:
                print(f"  {YELLOW}[FIRMWARE] Hint: {hint}{RESET}")

    # Определяем slot и board_name
    if _env_name.endswith("-prod"):
        slot = "prod"
        board_name = _env_name[:-5]  # убираем -prod
    elif _env_name.endswith("-stage"):
        slot = "stage"
        board_name = _env_name[:-6]  # убираем -stage
    else:
        slot = "prod"
        board_name = _env_name

    # Маппинг имен плат для flasher-portal (соответствует существующим папкам)
    flasher_board_mapping = {
        "esp32c3": "esp32c3",
        "esp32c3-super-mini": "esp32c3-super-mini",
        "xiao-esp32s3": "xiao-esp32s3", 
        "waveshare-esp32s3-zero": "waveshare-esp32s3-zero"
    }
    
    flasher_board_name = flasher_board_mapping.get(board_name, board_name)
    
    # Пути назначения
    local_dest = LOCAL_FIRMWARE_DIR / board_name
    local_dest.mkdir(parents=True, exist_ok=True)
    
    if flasher_firmware_base:
        flasher_dest = flasher_firmware_base / slot / flasher_board_name
        flasher_dest.mkdir(parents=True, exist_ok=True)
    else:
        flasher_dest = None
        if not portal_root:
            print(
                f"  {YELLOW}[FIRMWARE] IDRYER_FLASHER_PORTAL_PATH unset in this build process → "
                f"flasher-portal skipped (local only). Use a shell where `export` is set, then `pio run`.{RESET}"
            )

    # Файлы для копирования
    files = {
        "firmware.bin":    _build_dir / "firmware.bin",
        "bootloader.bin":  _build_dir / "bootloader.bin",
        "partitions.bin":  _build_dir / "partitions.bin",
        "boot_app0.bin":   BOOT_APP0_SRC,
    }

    print(f"  {BLUE}[FIRMWARE] Copying {_env_name} firmware...{RESET}")
    
    for filename, src in files.items():
        if src.exists():
            # Копируем в локальную папку
            shutil.copy2(src, local_dest / filename)
            print(f"  [FIRMWARE] {filename} → {local_dest / filename}")
            
            # Копируем в flasher-portal (если настроен)
            if flasher_dest:
                shutil.copy2(src, flasher_dest / filename)
                print(f"  [FIRMWARE] {filename} → {flasher_dest / filename}")
        else:
            print(f"  [FIRMWARE] WARNING: {src} not found, skipping {filename}")
    
    destinations = "local"
    if flasher_dest:
        destinations += " + flasher-portal"
    
    print(f"  {GREEN}[FIRMWARE] ✅ {_env_name} copied to {destinations}!{RESET}")


env.AddPostAction("$BUILD_DIR/firmware.bin", copy_firmware)
