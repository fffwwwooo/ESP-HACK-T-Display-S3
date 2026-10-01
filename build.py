import os
import re
import subprocess
import sys
from pathlib import Path

Import("env")


def read_firmware_version(project_dir):
    config_path = project_dir / "src" / "CONFIG.h"
    try:
        config_text = config_path.read_text(encoding="utf-8")
    except OSError as e:
        print(f"ERROR: Unable to read firmware version from {config_path}: {e}")
        sys.exit(1)

    match = re.search(r'static\s+const\s+char\s*\*\s*FIRMWARE\s*=\s*"([^"]+)"', config_text)
    if not match:
        print(f"ERROR: FIRMWARE value was not found in {config_path}")
        sys.exit(1)

    return match.group(1)


def merge_binaries(source, target, env):
    project_dir = Path(env["PROJECT_DIR"])
    firmware_version = read_firmware_version(project_dir)
    merged_bin = project_dir / f"esp-hack_{firmware_version}-tds3.bin"

    build_dir = Path(env.subst("$BUILD_DIR"))
    firmware_bin = build_dir / "firmware.bin"
    bootloader_bin = build_dir / "bootloader.bin"
    partitions_bin = build_dir / "partitions.bin"

    # ESP32-S3 flash map: bootloader @ 0x0, partitions @ 0x8000, app @ 0x10000
    cmd = [
        sys.executable, "-m", "esptool",
        "--chip", "esp32s3",
        "merge_bin",
        "-o", str(merged_bin),
        "--flash_size", "16MB",
        "--flash_mode", "qio",
        "0x0", str(bootloader_bin),
        "0x8000", str(partitions_bin),
        "0x10000", str(firmware_bin),
    ]

    print("Merging binary files:")
    print(f" - Bootloader:  {bootloader_bin} @ 0x0")
    print(f" - Partitions:  {partitions_bin} @ 0x8000")
    print(f" - Firmware:    {firmware_bin} @ 0x10000")
    print(f" - Output:      {merged_bin}")

    try:
        subprocess.run(cmd, check=True, shell=True)
        if merged_bin.exists():
            size = os.path.getsize(merged_bin) / 1024
            print(f"\nSuccessfully created merged file: {merged_bin}")
            print(f"File size: {size:.2f} KB")
            print(f'To flash, use: python -m esptool --chip esp32s3 write_flash 0x0 "{merged_bin}"')
        else:
            print("ERROR: Merged file was not created")
            sys.exit(1)
    except subprocess.CalledProcessError as e:
        print(f"ERROR: esptool failed with code {e.returncode}")
        sys.exit(1)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_binaries)
