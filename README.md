# FLUG-OS {-1, 0, +1}

**F**luid **O**perating **S**ystem for the Flipper Zero WiFi Module v1.

Turns your Flipper Zero into a raw 802.11 packet-wave sampler. Captures WiFi frames in promiscuous mode, streams packet features as numerical wave data — RSSI becomes amplitude, frame types become frequency, packet density becomes rhythm. Pipe it into generative music, signal analysis, or just watch the invisible spectrum become visible.

```
FLUG-OS on ESP8285    →    UART    →    Python bridge    →    music.vaked.dev
                                      your ears
                                      MIDI synth
                                      any HTTP endpoint
```

Part of the 8b-is stack. Ternary by design. {-1, 0, +1}

---

## Quick Start

### 1. What you need

- **Flipper Zero** with a **WiFi Module v1** (ESP8285-based, black PCB)
- **USB-C cable** to connect Flipper to your computer
- **PlatformIO** (or Arduino IDE with ESP8266 core)

### 2. Flash the firmware

```bash
# Clone
git clone https://github.com/8b-is/FLUG-OS.git
cd FLUG-OS

# Build & upload (Flipper must be in QFlipper mode or connected via USB)
pio run -e esp8285 -t upload

# Or build the binary and flash manually
pio run -e esp8285
```

> **No Flipper?** You can flash any ESP8285/ESP8266 board (NodeMCU, Wemos D1, etc.)
> with the same firmware. Change `board = esp8285` to `board = nodemcuv2` in platformio.ini.

### 3. Connect

```bash
# Find your serial port
ls /dev/tty.*
# Typical: /dev/tty.usbserial-XXXX (macOS) or /dev/ttyUSB0 (Linux)

# Open serial monitor
pio device monitor --port /dev/tty.usbserial-XXXX --baud 115200

# Or use screen/tmux
screen /dev/tty.usbserial-XXXX 115200
```

You should see JSON lines streaming in — each line is a captured WiFi frame.

### 4. Wave bridge (optional — for music generation)

```bash
# Install deps
pip3 install pyserial

# Stream raw wave data to terminal
python3 bridge/wave_bridge.py /dev/tty.usbserial-XXXX --mode wave --dump

# Pipe to music.vaked.dev (or any HTTP endpoint)
python3 bridge/wave_bridge.py /dev/tty.usbserial-XXXX --mode json --http https://music.vaked.dev/wave
```

---

## Commands

Send commands over serial. Every command returns JSON confirmation.

| Command | What it does | Example |
|---------|-------------|---------|
| `ch <n>` | Set fixed channel 1–13 | `ch 6` — listen on WiFi channel 6 |
| `hop` | Toggle channel hopping (1s per channel) | `hop` — sweeps all channels |
| `mode raw` | Wave output: RSSI + density + type | `mode raw` — 3 floats per packet |
| `mode wave` | Wave output: synthesized sinusoid | `mode wave` — audio waveform samples |
| `mode json` | Wave output: structured musical JSON | `mode json` — for API integration |
| `filter mgmt` | Show only management frames | `filter mgmt` — beacons, probes, deauths |
| `filter data` | Show only data frames | `filter data` — actual network traffic |
| `filter ctrl` | Show only control frames | `filter ctrl` — RTS, CTS, ACK |
| `filter all` | Show everything | `filter all` — default |
| `stats` | Show capture statistics | `stats` — packet counts per type |
| `reset` | Reset all counters | `reset` — fresh stats |

---

## Wave theory

FLUG-OS is aligned with the **axiomquant.org** universal threshold kernel.
Every packet is evaluated through the sine kernel K(u) = sin(πu)/(πu).

| Domain | Below critical | At critical | Above critical |
|--------|---------------|-------------|----------------|
| Erdős β₁ (graph) | Fragmented | Giant component | Connected |
| **BitNet ternary γ** | Continuous | **Round to ±1** | **{-1,0,+1}** |
| Markowitz λ | Zero weight | Enters | Dominates |
| Black-Scholes σ | Deep OTM | ATM | Deep ITM |
| Rényi dyadic β | Preserved | 1 bit/iter | Chaotic |

FLUG-OS operates in the ternary regime. RSSI → {-1, 0, +1}. Domain tag (-1/0/+1) on every line.

https://axiomquant.org/

---

## Wave output explained

When you set `mode wave`, ambient WiFi traffic becomes a live audio data stream:

```
Packet type → frequency     Beacon (220 Hz)  Deauth (415 Hz)  Data (523 Hz)
RSSI        → amplitude     -30 dBm → 1.0    -80 dBm → 0.2    -50 dBm → 0.6
Density     → rhythm        100 packets/s → dense   1 packet/s → sparse
```

The Python bridge converts these to MIDI notes, OSC messages, or HTTP payloads.
Each packet is a note. The WiFi spectrum is the instrument. Your environment is the composer.

---

## Output format

**Default (JSON mode) — one line per frame:**
```json
{"ts":123456,"len":68,"rssi":-45,"type":"mgmt","subtype":"beacon","src":"aa:bb:cc:dd:ee:ff","dst":"ff:ff:ff:ff:ff:ff","bssid":"aa:bb:cc:dd:ee:ff","ssid":"MyNetwork","chan":6,"bi":100}
```

**Wave mode — one float per packet:**
```
0.4231
-0.1872
0.8910
-0.5432
```

**Raw mode — three floats per packet:**
```
0.750 0.320 0
0.210 0.010 2
0.890 0.750 0
```
Columns: `rssi(0-1) density(0-1) frame_type(0=mgmt, 1=ctrl, 2=data)`

---

## Build from source

```bash
# Install PlatformIO
pip3 install platformio

# Build for ESP8285 (Flipper Zero WiFi v1)
pio run -e esp8285

# Build for generic ESP8266 (NodeMCU, etc.)
# Edit platformio.ini: change board = nodemcuv2

# Upload
pio run -e esp8285 -t upload
```

### Files

```
FLUG-OS/
├── src/
│   ├── flugos.cpp         # Main firmware (~450 lines)
│   └── wave_output.h      # Wave sonification engine
├── bridge/
│   └── wave_bridge.py      # Python serial → HTTP bridge
├── platformio.ini          # Build config
└── README.md               # This file
```

---

## Ecosystem

FLUG-OS is one node in the **8b-is constellation**:

| Node | What | Layer |
|------|------|-------|
| **HF-MAC{-1,0,+1}** | Native macOS Hugging Face client | App |
| **entheai** | Agent with MEM8 wave memory | Agent |
| **ayeOS** | Ternary matrix inference daemon | Inference |
| **MLX-QUANT** | Metal GPU ternary kernels | GPU |
| **FLUG-OS** | Raw 802.11 packet-wave sampler | Hardware |
| **music.vaked.dev** | Generative music from wave data | Audio |

All ternary. All aligned. {-1, 0, +1}

---

## License

MIT — educational/defensive security research only.

---

*Built by the 8b-is constellation. 🜂 {-1, 0, +1} — ennyi elég.*
