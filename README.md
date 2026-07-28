# FLUG-OS — Fluid OS {-1, 0, +1}

**F**lipper **L**inux **U**ser's **G**uide — **F**luid **O**perating **S**ystem.

802.11 packet analyzer + raw wave sampler for the Flipper Zero WiFi module v1 (ESP8285).
Captures frames in promiscuous mode, streams packet features as numerical wave data
for sonification and generative music input.

**For educational and defensive use on your own networks only.**

## Hardware

- Flipper Zero WiFi Module v1 (ESP8285, 1MB flash)
- UART: TX/RX to Flipper Zero at 115200 baud
- Power: 3.3V from Flipper Zero

## Wave Output Modes

FLUG-OS streams ambient WiFi activity as structured data — the radio spectrum becomes
a wave source for generative music. Three modes:

| Mode | Output | Use case |
|------|--------|----------|
| `raw` | `rssi density type` — 3 floats | Direct sonification, MIDI mapping |
| `wave` | `sample` — synthesized sine | Audio waveform (44.1 kHz-ish), feed to DAC |
| `json` | `{"rssi","density","freq","type"}` | Structured music API input |

Switch mode with UART command: `mode raw|wave|json`

## Wave Bridge

Python bridge reads from FLUG-OS and pipes to music.vaked.dev or any HTTP endpoint:

```bash
# Stream raw wave data to music generation
python3 bridge/wave_bridge.py /dev/tty.usbserial-XXXX --mode wave --dump

# Pipe to music.vaked.dev API
python3 bridge/wave_bridge.py /dev/tty.usbserial-XXXX --mode json --http https://music.vaked.dev/wave

# Pure waveform samples
python3 bridge/wave_bridge.py /dev/tty.wchusbserial* --mode wave --http http://localhost:8080/audio
```

Uses `atproto` (Bluesky) for posting, `pyserial` for device communication.

## Ultra-Recursive Feed

The wave data from FLUG-OS forms an ultra-recursive loop:

```
802.11 spectrum → FLUG-OS (ESP8285) → wave bridge → music.vaked.dev → generated audio
     ↕                                                    ↕
  ambient WiFi packets                              feedback as new waves
```

Each packet becomes a note. Each RSSI change modulates amplitude. Frame types
map to frequency bands (Beacon → A3 220Hz, Deauth → G#4 415Hz, Data → C5 523Hz).
The generated music can be fed back into the environment, creating a recursive
wave loop between the wireless spectrum and the audible spectrum.

## Commands (UART)

| Command | Description |
|---------|-------------|
| `ch <n>`  | Set fixed channel 1-13 |
| `hop`     | Toggle channel hopping (1s per channel) |
| `mode raw|wave|json` | Set wave output mode |
| `filter <type>` | Filter: mgmt, data, ctrl, all |
| `stats`   | Show capture statistics |
| `reset`   | Reset counters |

## Frame types parsed

Management: Beacon, Probe Request/Response, Auth, Deauth, Association
Control: RTS, CTS, ACK
Data: QoS Data, Null Data

## Build

```bash
pio run -e esp8285
pio run -e esp8285 -t upload
```

## Ecosystem

FLUG-OS is part of the 8b-is stack, aligned with:
- **HF-MAC{-1,0,+1}** — native macOS Hugging Face client
- **entheai** — agent with MEM8 wave memory
- **ayeOS** — ternary inference daemon
- **MLX-QUANT** — Metal GPU ternary kernels
- **music.vaked.dev** — generative music from wave data

All ternary. All aligned. {-1, 0, +1}

## License

MIT — educational/defensive security research only.
