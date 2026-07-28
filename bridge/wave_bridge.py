#!/usr/bin/env python3
"""
FLUG-OS Wave Bridge — pipe raw 802.11 packet waves to music.vaked.dev.

Reads wave output from FLUG-OS over serial, formats for generative music,
and streams to music.vaked.dev (or any HTTP endpoint).

Usage:
  python3 bridge/wave_bridge.py /dev/tty.usbserial-XXXX         # read + dump
  python3 bridge/wave_bridge.py --mode wave /dev/tty.wchusbserial*  # synth waves
  python3 bridge/wave_bridge.py --http https://music.vaked.dev/wave /dev/tty.*
"""

import argparse
import json
import math
import sys
import time
import urllib.request
from typing import Optional


NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def freq_to_midi(freq: float) -> float:
    """Convert frequency to MIDI note number."""
    if freq <= 0:
        return 0
    return 69 + 12 * math.log2(freq / 440.0)


def freq_to_note(freq: float) -> str:
    """Convert frequency to nearest note name (e.g. 'A4')."""
    midi = freq_to_midi(freq)
    if midi <= 0:
        return "---"
    note_idx = int(round(midi)) % 12
    octave = int(round(midi)) // 12 - 1
    return f"{NOTE_NAMES[note_idx]}{octave}"


# ============================================================
# Line parsers for each FLUG-OS wave mode
# ============================================================

def parse_raw_line(line: str) -> Optional[dict]:
    """Parse raw mode: 'RSSI DENSITY TYPE' → dict."""
    parts = line.strip().split()
    if len(parts) < 3:
        return None
    try:
        return {
            "rssi": float(parts[0]),
            "density": float(parts[1]),
            "frame_type": int(parts[2]),
        }
    except (ValueError, IndexError):
        return None


def parse_wave_line(line: str) -> Optional[dict]:
    """Parse wave mode: single float sample."""
    try:
        return {"wave": float(line.strip())}
    except ValueError:
        return None


def parse_json_line(line: str) -> Optional[dict]:
    """Parse JSON mode: structured musical data."""
    try:
        return json.loads(line.strip())
    except json.JSONDecodeError:
        return None


PARSERS = {
    "raw": parse_raw_line,
    "wave": parse_wave_line,
    "json": parse_json_line,
}


# ============================================================
# FLUG-OS data → music.vaked.dev format
# ============================================================

def to_music_vaked(data: dict) -> dict:
    """Convert FLUG-OS wave data to music.vaked.dev compatible format."""
    ts = int(time.time() * 1000)

    if "freq" in data:
        midi = freq_to_midi(data["freq"])
        return {
            "note": freq_to_note(data["freq"]),
            "midi": round(midi),
            "velocity": round(data.get("rssi", 0.5) * 127),
            "duration_sec": data.get("density", 0.5),
            "source": "flug-os",
            "timestamp_ms": ts,
        }

    if "wave" in data:
        return {
            "sample": data["wave"],
            "source": "flug-os",
            "timestamp_ms": ts,
        }

    rssi = data.get("rssi", 0.5)
    return {
        "note": freq_to_note(220 + rssi * 300),
        "velocity": round(rssi * 127),
        "source": "flug-os",
        "timestamp_ms": ts,
    }


# ============================================================
# HTTP streaming
# ============================================================

_http_ok = True  # only warn once

def stream_to_http(data: dict, url: str):
    """Send wave data to HTTP endpoint. Fire-and-forget with one-time warning."""
    global _http_ok
    try:
        payload = json.dumps(data).encode()
        req = urllib.request.Request(
            url,
            data=payload,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        urllib.request.urlopen(req, timeout=2)
    except Exception as e:
        if _http_ok:
            print(f"[bridge] HTTP error: {e}", file=sys.stderr)
            _http_ok = False


# ============================================================
# Serial reader with auto-reconnect
# ============================================================

def open_serial(port: str, baud: int, max_retries: int = 3):
    """Open serial port with retries."""
    import serial

    for attempt in range(max_retries):
        try:
            ser = serial.Serial(port, baud, timeout=1)
            return ser
        except serial.SerialException as e:
            if attempt < max_retries - 1:
                print(f"[bridge] retrying serial ({attempt+1}/{max_retries}): {e}", file=sys.stderr)
                time.sleep(2)
            else:
                print(f"[bridge] failed to open {port}: {e}", file=sys.stderr)
                sys.exit(1)
    return None  # unreachable


# ============================================================
# Main
# ============================================================

def main():
    parser = argparse.ArgumentParser(
        description="FLUG-OS Wave Bridge — pipe WiFi packet waves to music.vaked.dev",
    )
    parser.add_argument("port", help="Serial port (e.g., /dev/tty.usbserial-*)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    parser.add_argument(
        "--mode",
        choices=["raw", "wave", "json"],
        default="json",
        help="FLUG-OS wave output mode (default: json)",
    )
    parser.add_argument("--http", help="HTTP endpoint URL for wave data")
    parser.add_argument("--dump", action="store_true", help="Dump parsed data to stdout")
    args = parser.parse_args()

    # Open serial (with retries)
    ser = open_serial(args.port, args.baud)

    print(f"[bridge] FLUG-OS on {args.port} @ {args.baud}")
    print(f"[bridge] mode={args.mode}" + (f" http={args.http}" if args.http else ""))

    # Wait for device boot and flush boot messages
    time.sleep(2.5)
    ser.reset_input_buffer()

    # Send wave mode command
    ser.write(f"mode {args.mode}\n".encode())
    time.sleep(0.2)

    parser_fn = PARSERS[args.mode]
    buf = b""
    line_count = 0

    try:
        while True:
            try:
                data = ser.read(1024)
            except serial.SerialException:
                print("[bridge] serial disconnected, reconnecting...", file=sys.stderr)
                ser.close()
                time.sleep(2)
                ser = open_serial(args.port, args.baud)
                ser.write(f"mode {args.mode}\n".encode())
                continue

            if not data:
                continue

            buf += data

            while b"\n" in buf:
                line_bytes, buf = buf.split(b"\n", 1)
                text = line_bytes.decode("utf-8", errors="replace").strip()
                if not text:
                    continue

                # Strip ASCII/emoji prefix (~ or 🌊)
                if text.startswith("~ "):
                    text = text[2:]
                elif text.startswith("🌊 "):
                    text = text[2:]
                elif text.startswith("{"):
                    pass  # raw JSON, no prefix
                else:
                    continue  # boot message or other noise

                parsed = parser_fn(text)
                if not parsed:
                    continue

                music_data = to_music_vaked(parsed)
                line_count += 1

                if args.dump:
                    print(json.dumps(music_data, ensure_ascii=False))

                if args.http:
                    stream_to_http(music_data, args.http)

                if line_count % 1000 == 0:
                    print(f"[bridge] {line_count} waves streamed", file=sys.stderr)

    except KeyboardInterrupt:
        print(f"\n[bridge] stopped. {line_count} waves streamed.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
