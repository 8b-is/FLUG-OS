#!/usr/bin/env bash
# flash-s3-amoled.sh — Flash FLUG-OS [env:esp32s3_amoled] (v1.8.0+omni-upz-alexai-aurora)
# Uses 128 KiB (0x20000) --no-stub chunks @ 115200 baud with automatic per-chunk retry.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

PIO="${PIO:-/Library/Frameworks/Python.framework/Versions/3.12/bin/pio}"
"$PIO" run -e esp32s3_amoled

python3 - <<'PY'
import os, subprocess, sys, glob, time

build_dir = ".pio/build/esp32s3_amoled"
fw_path = os.path.join(build_dir, "firmware.bin")
data = open(fw_path, "rb").read()
print(f"firmware.bin size: {len(data)} bytes")
chunk = 0x20000 # 131072 bytes (128 KiB, 32 * 4096B sectors -> ~1.4s per chunk)
parts = []
for idx, offset in enumerate(range(0, len(data), chunk)):
    p_name = os.path.join(build_dir, f"fw_chunk{idx}.bin")
    open(p_name, "wb").write(data[offset:offset+chunk])
    parts.append((0x10000 + offset, p_name))

esptool = os.environ.get("ESPTOOL", "/Library/Frameworks/Python.framework/Versions/3.12/bin/esptool")
boot_app0 = os.path.expanduser("~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin")

def get_port():
    for _ in range(40):
        ports = sorted(glob.glob("/dev/cu.usbmodem*"))
        if ports:
            return ports[0]
        time.sleep(0.25)
    raise RuntimeError("No /dev/cu.usbmodem* found")

def flash_items(items, reset_after=False):
    for attempt in range(1, 6):
        port = get_port()
        cmd = [
            esptool, "--chip", "esp32s3", "--port", port, "--baud", "115200", "--no-stub",
            "--before", "default-reset", "--after", "watchdog-reset" if reset_after else "no-reset",
            "write-flash", "--flash-mode", "keep", "--flash-freq", "keep", "--flash-size", "keep"
        ]
        for addr, path in items:
            cmd += [hex(addr), path]
        print(f"[attempt {attempt}] flashing {[hex(a) for a, _ in items]} on {port}...")
        res = subprocess.run(cmd)
        if res.returncode == 0:
            return
        time.sleep(0.5)
    raise RuntimeError(f"Failed to flash {[hex(a) for a, _ in items]}")

flash_items([
    (0x0000, os.path.join(build_dir, "bootloader.bin")),
    (0x8000, os.path.join(build_dir, "partitions.bin")),
    (0xe000, boot_app0),
], reset_after=False)

for i, (addr, p_name) in enumerate(parts):
    is_last = (i == len(parts) - 1)
    flash_items([(addr, p_name)], reset_after=is_last)

print("ALL CHUNKS FLASHED AND VERIFIED SUCCESSFULLY!")
PY
