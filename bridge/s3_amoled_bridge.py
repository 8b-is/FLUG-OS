#!/usr/bin/env python3
"""
FLUG-OS × vaked.dev × 8b-is — ESP32-S3 AMOLED Constellation Bridge

Connects the Waveshare ESP32-S3-Touch-AMOLED-2.06 (/dev/cu.usbmodem2101)
to the Mac host stack:
  - Queries live hardware status (802.11 promiscuous packet count, RSSI,
    SSID, QMI8658 IMU QuantTern affect byte `qt:xx`, AXP2101 battery mV/%)
  - Syncs touchscreen track selections from the 14 Favourite Musics Crate
    directly to `zoneInn` (`cargo run -- play <slug>` or Bluetooth A2DP sink)
  - Switches AMOLED UI tabs (`0=RADIO`, `1=802.11`, `2=NAND`, `3=QWAVE`),
    channels (`ch 1..13`), or dual-boot partitions (`boot factory`)

Usage:
  uv run --with pyserial python bridge/s3_amoled_bridge.py --status
  uv run --with pyserial python bridge/s3_amoled_bridge.py --tab 1
  uv run --with pyserial python bridge/s3_amoled_bridge.py --track 8
  uv run --with pyserial python bridge/s3_amoled_bridge.py --listen --bt "Soundcore Select 4"
"""

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path
from typing import Optional


DEFAULT_PORT = "/dev/cu.usbmodem2101"
DEFAULT_BAUD = 115200


def open_port(port: str, baud: int):
    import serial
    return serial.Serial(port, baud, timeout=0.4)


def send_command(ser, cmd: str, wait_sec: float = 1.2) -> list[str]:
    ser.reset_input_buffer()
    ser.write(f"{cmd.strip()}\n".encode("utf-8"))
    ser.flush()
    lines: list[str] = []
    deadline = time.time() + wait_sec
    while time.time() < deadline:
        raw = ser.readline()
        if not raw:
            continue
        text = raw.decode("utf-8", errors="replace").strip()
        if text:
            lines.append(text)
            if text.startswith("{") and ("\"board\"" in text or "\"cmd\"" in text or "\"event\"" in text):
                break
    return lines


def trigger_zoneinn(slug: str, bt_device: Optional[str], zoneinn_dir: Path):
    cmd = ["cargo", "run", "--quiet", "--", "play", slug]
    if bt_device:
        cmd.extend(["--bt", bt_device])
    print(f"[s3-bridge] -> launching zoneInn: {' '.join(cmd)}")
    try:
        subprocess.Popen(cmd, cwd=str(zoneinn_dir))
    except Exception as exc:
        print(f"[s3-bridge] failed to launch zoneInn: {exc}", file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(
        description="FLUG-OS ESP32-S3 AMOLED Constellation Bridge (vaked.dev x 8b-is)"
    )
    parser.add_argument("--port", default=DEFAULT_PORT, help=f"Serial port (default: {DEFAULT_PORT})")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="Baud rate (default: 115200)")
    parser.add_argument("--status", action="store_true", help="Query live JSON status from ESP32-S3 AMOLED")
    parser.add_argument("--tab", type=int, choices=[0, 1, 2, 3], help="Switch active AMOLED tab (0..3)")
    parser.add_argument("--track", type=int, help="Select Favourite Music track (1..14)")
    parser.add_argument("--ch", type=int, help="Lock 802.11 channel (1..13)")
    parser.add_argument("--hop", action="store_true", help="Toggle 802.11 channel hopping")
    parser.add_argument("--listen", action="store_true", help="Listen for touchscreen events & wave telemetry")
    parser.add_argument("--bt", help="Optional Bluetooth A2DP sink name for zoneInn playback")
    args = parser.parse_args()

    ser = open_port(args.port, args.baud)
    time.sleep(0.15)

    try:
        if args.tab is not None:
            for line in send_command(ser, f"tab {args.tab}"):
                print(line)

        if args.track is not None:
            for line in send_command(ser, f"track {args.track}"):
                print(line)

        if args.ch is not None:
            for line in send_command(ser, f"ch {args.ch}"):
                print(line)

        if args.hop:
            for line in send_command(ser, "hop"):
                print(line)

        if args.status or (args.tab is None and args.track is None and args.ch is None and not args.hop and not args.listen):
            for line in send_command(ser, "status"):
                print(line)

        if args.listen:
            zoneinn_dir = Path(__file__).resolve().parents[2] / "zoneInn"
            print(f"[s3-bridge] listening on {args.port} @ {args.baud} (Ctrl-C to stop)...")
            while True:
                raw = ser.readline()
                if not raw:
                    continue
                text = raw.decode("utf-8", errors="replace").strip()
                if not text:
                    continue
                print(text)
                if text.startswith("{"):
                    try:
                        evt = json.loads(text)
                        if evt.get("event") == "zoneinn_track" and evt.get("playing") == 1:
                            slug = evt.get("slug")
                            if slug and zoneinn_dir.exists():
                                trigger_zoneinn(slug, args.bt, zoneinn_dir)
                    except json.JSONDecodeError:
                        pass
    except KeyboardInterrupt:
        print("\n[s3-bridge] disconnected.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
