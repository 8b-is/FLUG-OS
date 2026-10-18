+----------------------------------------------------------------------+
|  _____ _     _   _  ____      ___  ____   FLUID OPERATING SYSTEM     |
| |  ___| |   | | | |/ ___|    / _ \/ ___|  ----------------------     |
| | |_  | |   | | | | |  _    | | | \___ \  802.11 PACKET-WAVE SAMPLER |
| |  _| | |___| |_| | |_| |   | |_| |___) | ESP8285 SAMPLER BOARD      |
| |_|   |_____|\___/ \____|    \___/|____/  FLIPPER ZERO WIFI v1       |
+----------------------------------------------------------------------+
|  [ 2.4GHz SPECTRUM ANALYZER WAVE ]         [ FLIPPER CYBER-DOLPHIN ] |
|  dBm                                              ( ( ( RF ) ) )     |
|  -30|        /\                 /\                    /\             |
|  -40|       /  \    /\         /  \                  /  \   __       |
|  -50|   /\ /    \  /  \  /\   /    \/\              /  /   |[o ]\__  |
|  -60|  /  #      \/    \/  # /      \ \____________/  /====|====/  \ |
|  -70| /  / \     /\    /\ / /        \              /      '--'    \ |
|  -80|_/_/   \___/  \__/  /_/          \____________/        __\/\_/  |
|  2412MHz--------------------------------------------------> 2484MHz  |
|  CH: 01 02 03 04 05 06 07 08 09 10 11 12 13 14              \__/     |
|  WAV: ~~~^~~~v~~~^~~~^~~~v~~~^~~~v~~~^~~~v~~~^~~~v~~~~~~~~~~~~~~~~~  |
+----------------------------------------------------------------------+
|  PACKET TYPES : [BEACON] [PROBE-REQ] [DATA-ACK] [EAPOL] [DEAUTH]     |
|  SAMPLING     : Promiscuous IQ Waveform Captures @ 802.11b/g/n       |
+----------------------------------------------------------------------+
|  TARGET BOARDS: Waveshare ESP32-S3-Touch-AMOLED-1.8  (368x448 QSPI)  |
|                 Waveshare ESP32-S3-Touch-AMOLED-2.06 (410x502 QSPI)  |
|                 Flipper Zero WiFi Devboard v1 (ESP8285)              |
|  SYSTEM CORE  : FLUG-OS Real-Time Kernel + Wave Engine v0.5.0        |
|  SIGNING      : ML-DSA-65 + SLH-DSA post-quantum (NIST FIPS 204/205) |
+----------------------------------------------------------------------+


**FLUG-OS{-1,0,+1} (`v0.5.0+ultra-omni-zen`)** — Fluid Operating System for the **Waveshare ESP32-S3-Touch-AMOLED-1.8** (`368 × 448` `SH8601` QSPI AMOLED, `TCA9554` I/O expander, `ES8311` audio codec, `FT3168` capacitive touch, `QMI8658` 6-axis IMU, `AXP2101` PMIC — persona **`adaye`**), the **Waveshare ESP32-S3-Touch-AMOLED-2.06** (`410 × 502` `SH8601/CO5300` QSPI AMOLED — persona **`vaked`**), and the **Flipper Zero WiFi Module v1** (`ESP8285`).
Captures 802.11 frames in promiscuous mode, streams packet features as numerical wave data
through the universal sine kernel K(u)=sin(πu)/(πu), synchronizes wave splashes and track selection across sibling boards via **ESP-NOW** (`S3OmniPeerPacket`), and renders a live 5-tab Sovereign Constellation Cockpit (`ZEN`, `RADIO`, `802.11`, `NAND`, `QWAVE`) on both the 1.8" (`368×448`) and 2.06" (`410×502`) AMOLED displays.

Aligned with **axiomquant.org** & **vaked.dev × 8b-is** — five threshold domains:
Erdős β₁ · BitNet ternary γ · Markowitz λ · Black-Scholes σ · Rényi dyadic β.

Signed with **ML-DSA-65 + SLH-DSA** post-quantum signatures (NIST FIPS 203/204/205).

**Roadmap:** https://github.com/8b-is/FLUG-OS/blob/main/ROADMAP.md — target v1.4.2 parity with MLX-QUANT.

## ESP32-S3-Touch-AMOLED-1.8 (`adaye`) & 2.06 (`vaked`) ULTRA-OMNI-ZEN Cockpit (`v0.5.0`)

Evolving `vaked.dev` & `8b-is` (`baye-rust` Jelly Sphere Mind-Wall + 2D Wave Pool, `zoneInn` / `radio.vaked.dev`, `ultrawhale-oscillator`, `qwave` `sphered` / `quanttern`) onto both attached Waveshare ESP32-S3 AMOLED boards with runtime auto-detection:

| Subsystem | 1.8" (`adaye` · `/dev/cu.usbmodem1101`) | 2.06" (`vaked` · `/dev/cu.usbmodem2101`) | Function in FLUG-OS v0.5.0 |
|-----------|------------------------------------------|-------------------------------------------|----------------------------|
| **AMOLED Display** | `SH8601` (`368×448` QSPI, `x_gap=0`) | `SH8601 / CO5300` (`410×502` QSPI, `x_gap=0x16`) | 28-row DMA band rasterizer (`S3Canvas`) at 40 MHz QSPI in Constellation Ink |
| **I/O Expander** | `TCA9554` @ `0x20` (`SDA=15, SCL=14`) | Direct GPIO reset (`LCD_RST=8, TP_RST=9`) | Runtime board auto-detection (`0x20` probe) + display/touch/audio rail reset |
| **Audio Codec** | `ES8311` @ `0x18` + `PA_EN=GPIO46` | `ES8311` @ `0x18` + `PA_EN=GPIO46` | Full ADF codec register init + 85% DAC volume |
| **Power / PMIC** | `AXP2101` @ `0x34` | `AXP2101` @ `0x34` | Enables `DCDC1` & `ALDO1..4` + `BLDO1..2` 3.3V rails + battery mV/% telemetry |
| **Capacitive Touch** | `FT3168` @ `0x38` (`INT=21`) | `FT3168` @ `0x38` (`RST=9, INT=38`) | 5-tab navigation, live 2D Wave Pool ripple injection, crate & channel control |
| **6-Axis IMU** | `QMI8658` @ `0x6B` | `QMI8658` @ `0x6B` | 3D Accel/Gyro → `qwave` `QuantTern` 6-bit VAD affect (`qt:xx`), sickness & Lissajous tilt |
| **ESP-NOW Mesh** | MAC `80:B5:4E:DB:3B:C4` | MAC `80:B5:4E:F2:02:B0` | Over-the-air `S3OmniPeerPacket` wave splash & `zoneInn` track synchronization |

### Five Interactive AMOLED Views

1. **`[0] ZEN` (`ULTRA-OMNI-ZEN · JELLY SPHERE MIND-WALL`)**: Circular **Jelly Sphere Mind-Wall** (`baye-rust` / `8b-is/oxi-phoenix`) backed by a `64×64` integer Laplacian **2D Wave Pool** (`u_next = 2u - u_prev + c²∇²u`, damping `251/256`), orbiting 7-stage ternary NAND nodes on the amber info ring, **Eye of Aye** IMU-tilted reticle, touch-to-splash ripple injection, and live `adaye ↔ vaked` ESP-NOW peer capsule sync.
2. **`[1] RADIO` (`ZONEINN · RADIO.VAKED.DEV`)**: Interactive **14 Favourite Musics Crate** (`Innerbloom`, `Mahakala`, `Take Five`, `So What`, `Time`, `Clair de Lune`, `Gymnopédie No. 1`, `Golden Ratio 432×φ`, `Beethoven 5th`, `Ode to Joy`, `Für Elise`, `Pachelbel's Canon`, `Midnight Sovereign`, `Ultrawhale NAND Groove`) + live 8-band Ternary Harmonic Equalizer (`{-1, 0, +1}`).
3. **`[2] 802.11` (`FLUG-OS · PACKET-WAVE SAMPLER`)**: Live 13-channel promiscuous RSSI spectrum analyzer, AxiomQuant `K(u) = sin(πu)/(πu)` oscilloscope trace, and `64×3` `TernarySeam` radiation matrix.
4. **`[3] NAND` (`ULTRAWHALE · NAND × IMU`)**: Live 7-stage cross-coupled ternary NAND ring oscillator (`peetpedro/ultrawhale-oscillator`) + QMI8658 IMU-tilted 2D Lissajous harmonic phase portrait.
5. **`[4] QWAVE` (`SPHERED · SOVEREIGN CORE`)**: `<(? admit ~ transform ! verify @ attribute > commit)>` `#Commit = #ValidWitness` 5-stage witness badge + Hardware Identity (`1.8" adaye` / `2.06" vaked`) + Flash & Backup Switcher.

### Build, Flash & Multi-Board Bridge (ESP32-S3 AMOLED 1.8" & 2.06")

```bash
# Build unified ESP32-S3 AMOLED firmware (.pio/build/esp32s3_amoled_18/firmware.bin)
pio run -e esp32s3_amoled -e esp32s3_amoled_18

# Flash Waveshare ESP32-S3-Touch-AMOLED-1.8 (/dev/cu.usbmodem1101, 16MB flash)
esptool.py --chip esp32s3 --port /dev/cu.usbmodem1101 --baud 921600 \
  write_flash -z --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x00010000 .pio/build/esp32s3_amoled_18/firmware.bin

# Flash Waveshare ESP32-S3-Touch-AMOLED-2.06 (/dev/cu.usbmodem2101, ota_0 @ 0x00a00000)
esptool.py --chip esp32s3 --port /dev/cu.usbmodem2101 --baud 921600 \
  write_flash -z --flash_mode dio --flash_freq 80m --flash_size 32MB \
  0x00a00000 .pio/build/esp32s3_amoled/firmware.bin

# Query live telemetry / inject 2D wave splashes / sync tracks across BOTH boards simultaneously
uv run --with pyserial python bridge/s3_amoled_bridge.py --all --status
uv run --with pyserial python bridge/s3_amoled_bridge.py --all --splash
uv run --with pyserial python bridge/s3_amoled_bridge.py --all --track 8
uv run --with pyserial python bridge/s3_amoled_bridge.py --listen --bt "Soundcore Select 4"
```

## Quick start — Flipper Zero WiFi v1 (3 minutes)

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
# Find your serial port
ls /dev/tty.*
# macOS: /dev/tty.usbserial-XXXX  or  /dev/tty.wchusbserial*
# Linux: /dev/ttyUSB0  or  /dev/ttyACM0
# Windows: COM3 or similar

# Listen (macOS/Linux)
screen /dev/tty.usbserial-XXXX 115200
```

> **WiFi Module v1**: the ESP8285 module connects via UART through the
> Flipper Zero's USB-C. FLUG-OS talks directly to the ESP8285's serial
> console at 115200 baud. No Flipper Zero application needed — the
> firmware runs standalone on the ESP8285 itself.

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

## WiFi Module v1 setup guide

The Flipper Zero WiFi Module v1 uses an **ESP8285** (ESP8266 family) with 1MB flash.
It connects to your computer through the Flipper Zero's USB-C port and communicates
over UART at 115200 baud.

### Identifying your module

| Revision | Chip | Flash | Board ID |
|----------|------|-------|----------|
| **v1** (black PCB) | ESP8285 | 1MB | `esp8285` |
| **v2** (white PCB) | ESP32-S2 | 4MB | `esp32-s2` |

FLUG-OS targets the **v1 module (ESP8285)**. For v2 with ESP32-S2 a separate
build target is planned.

### Physical connection

The WiFi Module v1 does **not** have its own USB port. It is powered and
communicated with through the Flipper Zero's USB-C connector. When you
plug the Flipper Zero into your computer via USB, the ESP8285 appears as
a serial device:

```
  Flipper Zero (USB-C)  --- cable --->  WiFi Module v1 (ESP8285)
  /dev/tty.usbserial-XXXX                UART @ 115200 baud
```

### Flashing via Flipper Zero (recommended)

1. Connect the WiFi Module to the Flipper Zero (16-pin GPIO header)
2. Connect the Flipper Zero to your computer via USB-C
3. Put the Flipper in DFU mode: hold **LEFT + BACK** on boot
4. Flash:
   ```bash
   pio run -e esp8285 -t upload
   ```
   PlatformIO auto-detects the ESP8285 through the Flipper's serial bridge.

### Flashing directly (FTDI adapter)

If you have a USB-UART (FTDI) adapter:

```
FTDI -> ESP8285
GND  -> GND (pin 9)
TX   -> RX  (pin 8)
RX   -> TX  (pin 7)
3.3V -> 3.3V(pin 1)
IO0  -> GND (for flash mode, remove after)
```

```bash
pio run -e esp8285 -t upload --upload-port /dev/tty.usbserial-XXXX
```

After flashing, remove the IO0->GND jumper and reset the module.

### Serial console

Connect to see the FLUG-OS boot screen:

```bash
screen /dev/tty.usbserial-XXXX 115200
```

Expected output:

```
  =============================================
        FLUG-OS  {-1, 0, +1}
    Fluid Operating System v0.3.0+dev
  =============================================
  ...
```

### No Flipper Zero?

FLUG-OS runs on **any ESP8266/ESP8285 board**. Edit `platformio.ini`:

```ini
board = nodemcuv2        # NodeMCU v2, Wemos D1 Mini, etc.
```

Build and upload:

```bash
pio run -e generic -t upload
```

UART output will be on the board's built-in USB serial (`/dev/ttyUSB0`).

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
pio run -e esp32s3_amoled             # Waveshare ESP32-S3-Touch-AMOLED-2.06 (410x502 QSPI)
pio run -e esp8285                    # Flipper Zero WiFi v1
pio run -e generic                    # Generic ESP8266
pio run -e debug                      # Debug build with serial output
```

### Files

```
FLUG-OS/
├── src/
│   ├── flugos.cpp           # Main firmware entrypoint (ESP8285 + ESP32-S3 AMOLED)
│   ├── s3_amoled_hal.h      # SH8601 QSPI AMOLED, AXP2101 PMIC, FT3168 Touch, QMI8658 IMU, S3Canvas
│   ├── s3_amoled_app.h      # 4-Tab Sovereign Cockpit (RADIO / 802.11 / NAND / QWAVE)
│   ├── ternary_seam.h       # Radiation-transfer ternary matrix seam + sine kernel weighting
│   ├── wave_output.h        # Sine kernel wave sonification
│   ├── matrix_decoder.h     # 1:1 packet→ternary matrix decoder
│   └── version.h            # Semantic versioning + CI commit
├── bridge/
│   ├── s3_amoled_bridge.py  # ESP32-S3 AMOLED <-> zoneInn & Mac host serial bridge
│   └── wave_bridge.py       # Serial → HTTP/MIDI wave bridge
├── .github/workflows/
│   └── ci.yml               # Blacksmith CI + PQC signing
├── platformio.ini           # Build config (esp8285, generic, debug, esp32s3_amoled)
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
