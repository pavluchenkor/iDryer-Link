"""
Pre-build script: копирует menu файлы из RP2040 проекта в lib/idryer-menu

Логика:
1. Симлинк есть, файлы есть → копируем в lib/idryer-menu/
2. Симлинка нет, но в lib есть → используем что есть (warning)
3. Нигде нет → ошибка сборки
"""

import os
import shutil
from pathlib import Path

Import("env")

# Пути
PROJECT_DIR = Path(env["PROJECT_DIR"])
SOURCE_DIR = PROJECT_DIR / "config-exmple" / "menu"
DEST_DIR = PROJECT_DIR / "lib" / "idryer-menu" / "src"
VERSION_DEST = DEST_DIR / "version.h"

# Файлы для копирования (только нужные для LINK)
MENU_FILES = [
    "menu_meta.h",    # метаданные меню (структура, тексты, min/max)
    "menu_ids.h",     # enum с ID пунктов меню
    "menu_cache.h",   # кэш значений (заголовок)
    "menu_cache.cpp", # кэш значений (определение g_menu_cache)
]

def copy_menu_files():
    """Копирует menu файлы перед сборкой"""

    source_exists = SOURCE_DIR.exists() and SOURCE_DIR.is_dir()
    dest_exists = DEST_DIR.exists() and any(DEST_DIR.glob("*.cpp"))

    # Проверяем что симлинк рабочий (указывает на существующую папку)
    if SOURCE_DIR.is_symlink():
        target = SOURCE_DIR.resolve()
        if not target.exists():
            source_exists = False
            print(f"  [WARNING] Симлинк {SOURCE_DIR} указывает на несуществующую папку: {target}")

    if source_exists:
        # Копируем из симлинка
        DEST_DIR.mkdir(parents=True, exist_ok=True)

        copied = 0
        for filename in MENU_FILES:
            src = SOURCE_DIR / filename
            dst = DEST_DIR / filename

            if src.exists():
                # Копируем только если файл изменился
                if not dst.exists() or src.stat().st_mtime > dst.stat().st_mtime:
                    shutil.copy2(src, dst)
                    print(f"  [MENU] Copied: {filename}")
                    copied += 1
            else:
                print(f"  [WARNING] File not found in source: {filename}")

        if copied == 0:
            print("  [MENU] Files up to date")

        # Копируем version.h из MCU в lib/idryer-menu/src/
        # Если SOURCE_DIR это симлинк, берем реальный путь
        real_source = SOURCE_DIR.resolve() if SOURCE_DIR.is_symlink() else SOURCE_DIR
        version_src = real_source.parent / "version.h"
        if version_src.exists():
            if not VERSION_DEST.exists() or version_src.stat().st_mtime > VERSION_DEST.stat().st_mtime:
                shutil.copy2(version_src, VERSION_DEST)
                print(f"  [VERSION] Copied MCU version.h to lib/idryer-menu/src/")
            else:
                print("  [VERSION] MCU version.h up to date")

        # Создаём library.json если нет
        lib_json = DEST_DIR.parent / "library.json"
        if not lib_json.exists():
            lib_json.write_text('''{
    "name": "idryer-menu",
    "version": "1.0.0",
    "description": "Menu data structure (auto-copied from RP2040 project)",
    "frameworks": "*",
    "platforms": "*"
}
''')
            print("  [MENU] Created library.json")

    elif dest_exists:
        # Используем кэшированные файлы
        print("  [MENU] Using cached files (source symlink not available)")

    else:
        # Нигде нет — ошибка
        print("\n" + "="*60)
        print("ERROR: Menu files not found!")
        print("="*60)
        print(f"Source (symlink): {SOURCE_DIR}")
        print(f"Destination:      {DEST_DIR}")
        print("")
        print("Solutions:")
        print("1. Create symlink to RP2040 project:")
        print(f"   ln -s /path/to/iDryerRP2040/src/menu {SOURCE_DIR}")
        print("")
        print("2. Or copy menu files manually to:")
        print(f"   {DEST_DIR}")
        print("="*60 + "\n")
        env.Exit(1)

# Запускаем при загрузке скрипта (pre-build)
print("[PRE-BUILD] Checking menu files...")
copy_menu_files()
