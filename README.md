
                                   ___
                             ~~~~~|~~~ FLUG-OS
        ___--~~~~~---__    ~~~~~ | ~~~
  ___--~~             ~~--__  ~~~|~~~ {-1, 0, +1}
 |                        | ~~~~|~~~~
 |   FLUG-OS              |     |
 |   Fluid Operating Sys  |     |  ~~~~~~
 |   802.11 Packet-Wave   |     | ~~
 |   Sampler              |     |~
 |________________________|    ~~|~~
      |                |    ~~~~ |
      |  ESP8285       |   ~~~~~~|
      |  Flipper Zero  | ~~~~
      |  WiFi v1       |~~
      |________________|

     ~ 0.750 0.320 2 1 0.9231     ← packet RSSI density type domain kernel
     ~ 0.4231 1 0.9231             ← synthesized wave domain kernel
     🌊 {"ts":...,"rssi":-45,...} ← raw 802.11 frame JSON


**FLUG-OS{-1,0,+1}** — Fluid Operating System for the **Flipper Zero WiFi Module v1** (ESP8285).
Captures 802.11 frames in promiscuous mode, streams packet features as numerical wave data
through the universal sine kernel K(u)=sin(πu)/(πu). Every WiFi packet becomes a musical note,
a ternary matrix row, or a spectral analysis point.

Aligned with **axiomquant.org** — five threshold domains:
Erdős β₁ · BitNet ternary γ · Markowitz λ · Black-Scholes σ · Rényi dyadic β.

Signed with **ML-DSA-65 + SLH-DSA** post-quantum signatures (NIST FIPS 203/204/205).

---

## Quick start — 3 minutes

### 1. What you need

```
[ ] Flipper Zero
[ ] WiFi Module v1 (ESP8285, black PCB)
[ ] USB-C cable
[ ] Computer with Python 3
```

### 2. Flash

```bash
# Install PlatformIO (one time)
pip3 install platformio

# Clone & build
git clone https://github.com/8b-is/FLUG-OS.git
cd FLUG-OS
pio run -e esp8285 -t upload
```

> **No Flipper?** FLUG-OS runs on any ESP8266. Edit `platformio.ini`:
> change `board = esp8285` to `board = nodemcuv2` for NodeMCU, Wemos D1, etc.

### 3. Connect

```bash
# Find port
ls /dev/tty.*
# macOS: /dev/tty.usbserial-XXXX
# Linux: /dev/ttyUSB0

# Listen
screen /dev/tty.usbserial-XXXX 115200
```

### 4. Start sniffing

```
mode wave          # hear the spectrum as sine waves
ch 6              # listen on channel 6
hop               # sweep all channels
filter mgmt       # only management frames
stats             # packet counts
```

### 5. Pipe to music

```bash
pip3 install pyserial
python3 bridge/wave_bridge.py /dev/tty.usbserial-XXXX --mode wave --dump
```

---

## Commands

| Command | What it does |
|---------|-------------|
| `ch 6` | Set channel (1-13) |
| `hop` | Toggle channel hopping (1s/ch) |
| `mode raw` | RSSI density type domain kernel — 5 floats |
| `mode wave` | Synthesized sine wave sample — 3 floats |
| `mode json` | Structured JSON with musical parameters |
| `filter mgmt|ctrl|data|all` | Filter by frame type |
| `stats` | Show capture statistics |
| `reset` | Reset counters |

---

## Wave output formats

### raw mode — 5 floats per packet

```
~ RSSI DENSITY FRAME_TYPE DOMAIN KERNEL
~ 0.750 0.320 2 1 0.9231
```

| Field | Range | Meaning |
|-------|-------|---------|
| RSSI | 0.0 — 1.0 | Normalized signal strength |
| DENSITY | 0.0 — 1.0 | Packets per 100ms window |
| FRAME_TYPE | 0, 1, 2 | mgmt, ctrl, data |
| DOMAIN | -1, 0, +1 | Below/at/above critical threshold |
| KERNEL | 0.0 — 1.0 | Sine kernel K(u)=sin(πu)/(πu) |

### wave mode — 3 floats per packet

```
~ WAVE DOMAIN KERNEL
~ 0.4231 1 0.9231
```

### json mode — structured musical data

```
~ {"rssi":0.750,"density":0.320,"freq":220.00,"type":0,"subtype":8,"domain":1,"kernel":0.9231,"ts":123456}
```

### default (no wave mode) — 802.11 frame JSON

```
🌊 {"ts":123456,"len":68,"rssi":-45,"type":"mgmt","subtype":"beacon","src":"aa:bb:cc:dd:ee:ff","dst":"...","bssid":"...","ssid":"MyNetwork","chan":6,"bi":100}
```

---

## Ecosystem

| Node | What | Layer |
|------|------|-------|
| [**HF-MAC{-1,0,+1}**](https://github.com/8b-is/hf-mac) | Native macOS Hugging Face client | App |
| [**entheai**](https://github.com/entropy-om/entheai) | Agent with MEM8 wave memory | Agent |
| [**ayeOS**](https://github.com/8b-is/ayeos) | Ternary matrix inference daemon | Inference |
| [**MLX-QUANT**](https://github.com/8b-is/MLX-QUANT) | Metal GPU ternary kernels | GPU |
| [**FLUG-OS**](https://github.com/8b-is/FLUG-OS) | 802.11 packet-wave sampler | Hardware |
| [**axiomquant.org**](https://axiomquant.org) | Universal threshold kernel research | Math |

All ternary. All aligned. {-1, 0, +1}

---

## Build

```bash
pio run -e esp8285                    # Flipper Zero WiFi v1
pio run -e generic                    # Generic ESP8266
pio run -e debug                      # Debug build with serial output
```

### Files

```
FLUG-OS/
├── src/
│   ├── flugos.cpp           # Main firmware (500 lines)
│   ├── wave_output.h        # Sine kernel wave sonification
│   ├── matrix_decoder.h     # 1:1 packet→ternary matrix decoder
│   └── version.h            # Semantic versioning + CI commit
├── bridge/
│   └── wave_bridge.py       # Serial → HTTP/MIDI wave bridge
├── .github/workflows/
│   └── ci.yml               # Blacksmith CI + PQC signing
├── platformio.ini           # Build config
└── README.md                # This file
```

## Security

Firmware releases are signed with **post-quantum signatures**:

| Algorithm | Standard | Purpose |
|-----------|----------|---------|
| ML-DSA-65 | FIPS 204 (CRYSTALS-Dilithium) | Primary firmware signature |
| SLH-DSA-SHAKE-128s | FIPS 205 (SPHINCS+) | Backup signature |

Public keys published with each release. Verify:

```bash
openssl dgst -verify flugos-signing-mldsa65.pub \
  -signature firmware.bin.mldsa65.sig firmware.bin
```

Reference: [NIST FIPS 203/204/205 — August 13, 2024](https://security.googleblog.com/2024/08/post-quantum-cryptography-standards.html)

## License

MIT — educational/defensive security research only.

---

*Built by the 8b-is constellation. 🜂 {-1, 0, +1} — ennyi elég.*
