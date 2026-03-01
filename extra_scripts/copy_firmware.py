"""
Post-build script: копирует firmware.bin в папку firmware/<env_name>/
"""

import shutil
from pathlib import Path

Import("env")

def copy_firmware(source, target, env):
    env_name = env["PIOENV"]
    project_dir = Path(env["PROJECT_DIR"])
    dest_dir = project_dir / "firmware" / env_name

    dest_dir.mkdir(parents=True, exist_ok=True)

    src_bin = Path(str(target[0]))
    dst_bin = dest_dir / "firmware.bin"

    shutil.copy2(src_bin, dst_bin)
    print(f"  [FIRMWARE] Copied to {dst_bin}")

env.AddPostAction("$BUILD_DIR/firmware.bin", copy_firmware)
