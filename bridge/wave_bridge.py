#!/usr/bin/env python3
"""
FLUG-OS Wave Bridge — pipe raw 802.11 packet waves to music.vaked.dev.

Reads wave output from FLUG-OS over serial, formats for generative music,
and streams to music.vaked.dev (or any WebSocket/MIDI endpoint).

Usage:
  python3 bridge/wave_bridge.py /dev/tty.usbserial-XXXX  # read from device
  python3 bridge/wave_bridge.py --mode wave /dev/tty.wchusbserial*  # synth wave mode
  python3 bridge/wave_bridge.py --http http://localhost:8080/wave /dev/tty.*
"""

import argparse
import json
import math
import serial
import sys
import time
import urllib.request

# ============================================================
# Wave packet → musical parameters
# ============================================================
NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

def freq_to_midi(freq):
    """Convert frequency to MIDI note number."""
    if freq <= 0:
        return 0
    return 69 + 12 * math.log2(freq / 440.0)

def freq_to_note(freq):
    """Convert frequency to nearest note name."""
    midi = freq_to_midi(freq)
    if midi <= 0:
        return "---"
    note_idx = int(round(midi)) % 12
    octave = int(round(midi)) // 12 - 1
    return f"{NOTE_NAMES[note_idx]}{octave}"

# ============================================================
# Parse FLUG-OS raw wave line
# ============================================================
def parse_raw_line(line):
    """Parse raw mode: 'rssi density type'"""
    parts = line.strip().split()
    if len(parts) >= 3:
        return {
            "rssi": float(parts[0]),
            "density": float(parts[1]),
            "frame_type": int(parts[2]),
        }
    return None

def parse_wave_line(line):
    """Parse wave mode: single float"""
    try:
        return {"wave": float(line.strip())}
    except ValueError:
        return None

def parse_json_line(line):
    """Parse JSON mode"""
    try:
        return json.loads(line.strip())
    except json.JSONDecodeError:
        return None

# ============================================================
# Music.vaked.dev format
# ============================================================
def to_music_vaked(data):
    """Convert FLUG-OS wave data to music.vaked.dev API format."""
    if "freq" in data:
        midi = freq_to_midi(data["freq"])
        note = freq_to_note(data["freq"])
        return {
            "note": note,
            "midi": round(midi),
            "velocity": round(data.get("rssi", 0.5) * 127),
            "duration": data.get("density", 0.5),
            "source": "flug-os",
            "timestamp": int(time.time() * 1000),
        }
    elif "wave" in data:
        # Pure waveform sample
        return {
            "sample": data["wave"],
            "source": "flug-os",
            "timestamp": int(time.time() * 1000),
        }
    else:
        rssi = data.get("rssi", 0.5)
        return {
            "note": freq_to_note(220 + rssi * 300),
            "velocity": round(rssi * 127),
            "source": "flug-os",
            "timestamp": int(time.time() * 1000),
        }

# ============================================================
# Streaming
# ============================================================
def stream_to_http(data, url):
    """Send wave data to HTTP endpoint."""
    try:
        payload = json.dumps(data).encode()
        req = urllib.request.Request(url, data=payload,
            headers={"Content-Type": "application/json"})
        urllib.request.urlopen(req, timeout=1)
    except Exception:
        pass  # fire-and-forget

# ============================================================
# Main loop
# ============================================================
def main():
    parser = argparse.ArgumentParser(
        description="FLUG-OS Wave Bridge — pipe WiFi packet waves to music.vaked.dev")
    parser.add_argument("port", help="Serial port (e.g., /dev/tty.usbserial-*)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    parser.add_argument("--mode", choices=["raw", "wave", "json"], default="json",
                       help="FLUG-OS wave output mode")
    parser.add_argument("--http", help="HTTP endpoint URL for wave data")
    parser.add_argument("--dump", action="store_true", help="Dump parsed data to stdout")
    args = parser.parse_args()

    # Open serial
    try:
        ser = serial.Serial(args.port, args.baud, timeout=1)
    except serial.SerialException as e:
        print(f"Error opening {args.port}: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"FLUG-OS Wave Bridge — reading from {args.port} @ {args.baud}")
    print(f"Output mode: {args.mode}")
    if args.http:
        print(f"HTTP endpoint: {args.http}")
    print()

    # Wait for device boot
    time.sleep(2)
    ser.flushInput()

    # Set wave mode
    ser.write(f"mode {args.mode}\n".encode())
    time.sleep(0.1)

    parsers = {
        "raw": parse_raw_line,
        "wave": parse_wave_line,
        "json": parse_json_line,
    }
    parser_fn = parsers[args.mode]
    buf = b""

    try:
        while True:
            data = ser.read(1024)
            if not data:
                continue
            buf += data
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                try:
                    text = line.decode("utf-8", errors="replace").strip()
                    if not text:
                        continue
                    parsed = parser_fn(text)
                    if parsed:
                        music_data = to_music_vaked(parsed)
                        if args.dump:
                            print(json.dumps(music_data))
                        if args.http:
                            stream_to_http(music_data, args.http)
                except Exception as e:
                    print(f"Parse error: {e}", file=sys.stderr)
    except KeyboardInterrupt:
        print("\nBridge stopped.")
    finally:
        ser.close()

if __name__ == "__main__":
    main()
