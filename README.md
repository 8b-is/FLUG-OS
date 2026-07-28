# FLUG-OS — Fluid OS {-1, 0, +1}

**F**lipper **L**inux **U**ser's **G**uide — **F**luid **O**perating **S**ystem.

802.11 packet analyzer firmware for the ESP8285-based Flipper Zero WiFi module v1.
Captures frames in promiscuous mode, parses headers, outputs structured data over UART.
Part of the 8b-is stack. Ternary by design. {-1, 0, +1}

**For educational and defensive use on your own networks only.**

## Hardware

- Flipper Zero WiFi Module v1 (ESP8285, 1MB flash)
- UART: TX/RX to Flipper Zero at 115200 baud
- Power: 3.3V from Flipper Zero

## Output Protocol

UART output is line-delimited JSON:

```json
{"ts":123456,"len":68,"rssi":-45,"type":"mgmt","subtype":"beacon","src":"aa:bb:cc:dd:ee:ff","dst":"ff:ff:ff:ff:ff:ff","bssid":"aa:bb:cc:dd:ee:ff","ssid":"MyNetwork","chan":6,"bi":100}
```

## Build

```bash
pio run -e esp8285
pio run -e esp8285 -t upload
```

## Commands (UART)

| Command | Description |
|---------|-------------|
| `ch <n>`  | Set fixed channel 1-13 |
| `hop`     | Toggle channel hopping (1s per channel) |
| `filter <type>` | Filter: mgmt, data, ctrl, all |
| `stats`   | Show capture statistics |
| `reset`   | Reset counters |

## Frame types parsed

Management: Beacon, Probe Request/Response, Auth, Deauth, Association
Control: RTS, CTS, ACK
Data: QoS Data, Null Data

## Ecosystem

FLUG-OS is part of the 8b-is stack, aligned with:
- **HF-MAC{-1,0,+1}** — native macOS Hugging Face client
- **entheai** — agent with MEM8 wave memory
- **ayeOS** — ternary inference daemon
- **MLX-QUANT** — Metal GPU ternary kernels

All ternary. All aligned. {-1, 0, +1}

## License

MIT — educational/defensive security research only.
