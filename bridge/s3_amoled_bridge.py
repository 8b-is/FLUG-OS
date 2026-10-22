#!/usr/bin/env python3
"""
FLUG-OS × vaked.dev × 8b-is — ESP32-S3 AMOLED MEDUSA-OMNI Bridge (v0.8.0+medusa-omni)

Connects the Waveshare ESP32-S3-Touch-AMOLED-1.8 ("adaye" / ADA:MEDUSA, 368x448)
and Waveshare ESP32-S3-Touch-AMOLED-2.06 ("vaked" / VAK:MEDUSA & "omni" / OMN:MEDUSA, 410x502)
to the Mac host stack & https://esp32.vaked.dev/ WebSerial protocol:
  - Queries live hardware status (board auto-detection, persona, medusa_node, vertex,
    archetype, freq_hz, 802.11 promiscuous packet count, RSSI, SSID, QMI8658 IMU
    QuantTern affect byte `qt:xx` + sickness, 64x64 2D Wave Pool splashes,
    ESP-NOW peer rx count, ES8311 audio status, AXP2101 battery)
  - Syncs touchscreen track selections from the 20 Favourite Musics Crate (family+1)
    directly to `zoneInn` (`cargo run -- play <slug>` or Bluetooth A2DP sink)
  - Switches AMOLED UI tabs (`0=ZEN`, `1=RADIO`, `2=802.11`, `3=NAND`, `4=QWAVE`),
    injects 2D Wave Pool splashes (`--splash`), locks channels (`--ch 1..13`),
    or switches partitions (`boot factory`)

Usage:
  uv run --with pyserial python bridge/s3_amoled_bridge.py --all --status
  uv run --with pyserial python bridge/s3_amoled_bridge.py --port /dev/cu.usbmodem1101 --status
  uv run --with pyserial python bridge/s3_amoled_bridge.py --all --splash
  uv run --with pyserial python bridge/s3_amoled_bridge.py --all --tab 0
  uv run --with pyserial python bridge/s3_amoled_bridge.py --all --track 15
  uv run --with pyserial python bridge/s3_amoled_bridge.py --listen --bt "Soundcore Select 4"
"""

import argparse
import glob
import json
import subprocess
import sys
import time
from pathlib import Path
from typing import Optional


DEFAULT_PORT = "/dev/cu.usbmodem1101"
KNOWN_PORTS = ["/dev/cu.usbmodem1101", "/dev/cu.usbmodem2101", "/dev/cu.usbmodem101"]
DEFAULT_BAUD = 115200


def discover_ports() -> list[str]:
    return sorted(glob.glob("/dev/cu.usbmodem*"))


def open_port(port: str, baud: int):
    import serial
    return serial.Serial(port, baud, timeout=0.4, dsrdtr=False, rtscts=False)


def send_command(ser, cmd: str, wait_sec: float = 1.6) -> list[str]:
    ser.reset_input_buffer()
    clean = cmd.strip()
    wire = "STATUS\nstatus\n" if clean.lower() == "status" else f"{clean}\nSTATUS\n"
    ser.write(wire.encode("utf-8"))
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
            if text.startswith("{") and (
                '"board"' in text or '"cmd"' in text or '"event"' in text
            ):
                break
            if text.startswith("[USB] STATUS") and len(lines) >= 2:
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


def run_on_port(port: str, args) -> None:
    ser = open_port(port, args.baud)
    time.sleep(0.15)
    try:
        if args.tab is not None:
            for line in send_command(ser, f"tab {args.tab}"):
                print(f"[{port}] {line}")

        if args.splash:
            for line in send_command(ser, "splash"):
                print(f"[{port}] {line}")

        if args.track is not None:
            for line in send_command(ser, f"track {args.track}"):
                print(f"[{port}] {line}")

        if args.ch is not None:
            for line in send_command(ser, f"ch {args.ch}"):
                print(f"[{port}] {line}")

        if args.hop:
            for line in send_command(ser, "hop"):
                print(f"[{port}] {line}")

        if args.kompress:
            for line in send_command(ser, "KOMPRESS\nULTRATX", wait_sec=2.2):
                print(f"[{port}] {line}")

        if args.status or (
            args.tab is None
            and not args.splash
            and args.track is None
            and args.ch is None
            and not args.hop
            and not args.kompress
            and not args.listen
        ):
            for line in send_command(ser, "status"):
                print(f"[{port}] {line}")

        if args.listen:
            zoneinn_dir = Path(__file__).resolve().parents[2] / "zoneInn"
            print(f"[s3-bridge] listening on {port} @ {args.baud} (Ctrl-C to stop)...")
            while True:
                raw = ser.readline()
                if not raw:
                    continue
                text = raw.decode("utf-8", errors="replace").strip()
                if not text:
                    continue
                print(f"[{port}] {text}")
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
        print("\n[s3-bridge] stopped.")
    finally:
        ser.close()


def main():
    parser = argparse.ArgumentParser(
        description="FLUG-OS ESP32-S3 AMOLED ULTRA-OMNI-ZEN Bridge (vaked.dev x 8b-is)"
    )
    parser.add_argument("--port", default=DEFAULT_PORT, help=f"Serial port (default: {DEFAULT_PORT})")
    parser.add_argument("--all", action="store_true", help="Run command across all attached ESP32-S3 AMOLED boards (1.8\" & 2.06\")")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="Baud rate (default: 115200)")
    parser.add_argument("--status", action="store_true", help="Query live JSON status from ESP32-S3 AMOLED")
    parser.add_argument("--tab", type=int, choices=[0, 1, 2, 3, 4], help="Switch active AMOLED tab (0=ZEN, 1=RADIO, 2=802.11, 3=NAND, 4=QWAVE)")
    parser.add_argument("--splash", action="store_true", help="Inject central 2D Wave Pool splash + ESP-NOW peer broadcast")
    parser.add_argument("--track", type=int, help="Select Favourite Music track (1..23)")
    parser.add_argument("--ch", type=int, help="Lock 802.11 channel (1..13)")
    parser.add_argument("--hop", action="store_true", help="Toggle 802.11 channel hopping")
    parser.add_argument("--kompress", action="store_true", help="Harvest & Kompress-Super-Ultra all entropy (<<<<oOo>>>> ~ Kunisada & Kuniyoshi 010001000)")
    parser.add_argument("--listen", action="store_true", help="Listen for touchscreen events & wave telemetry")
    parser.add_argument("--bt", help="Optional Bluetooth A2DP sink name for zoneInn playback")
    args = parser.parse_args()

    ports = discover_ports() if args.all else [args.port]
    for port in ports:
        run_on_port(port, args)


if __name__ == "__main__":
    main()
