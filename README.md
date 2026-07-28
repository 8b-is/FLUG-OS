# Flipper WiFi Rev — 802.11 Packet Analyzer for Flipper Zero WiFi Module v1

Defensive WiFi packet analyzer firmware for the **ESP8285**-based Flipper Zero WiFi module v1.
Captures 802.11 frames in promiscuous mode, parses headers, outputs structured packet data
over UART for analysis on the Flipper Zero or any serial terminal.

**For educational and defensive use on your own networks only.**

## Hardware

- Flipper Zero WiFi Module v1 (ESP8285, 1MB flash)
- UART: TX/RX to Flipper Zero at 115200 baud
- Power: 3.3V from Flipper Zero

## Protocol

UART output is line-delimited JSON. Each line represents one sniffed frame:

```json
{"ts":123456,"len":68,"rssi":-45,"type":"mgmt","subtype":"beacon","src":"aa:bb:cc:dd:ee:ff","dst":"ff:ff:ff:ff:ff:ff","bssid":"aa:bb:cc:dd:ee:ff","ssid":"MyNetwork","chan":6}
```

## Build

Using the Arduino framework with ESP8266 core 3.x:

```bash
# PlatformIO
pio run -e esp8285
pio run -e esp8285 -t upload

# Or Arduino IDE: open flipper-wifi-rev.ino, select "ESP8285" board
```

## Controls (UART commands)

| Command | Description |
|---------|-------------|
| `ch <n>`  | Set channel 1-13 |
| `hop`     | Enable channel hopping (1s per channel) |
| `filter <type>` | Filter by frame type: mgmt, data, ctrl, all |
| `stats`   | Show capture statistics |
| `reset`   | Reset counters |

## Defensive use cases

- Monitor your own WiFi network for rogue access points
- Analyze beacon intervals and signal strength
- Detect deauthentication floods on your network
- Understand 802.11 frame structure and protocol
- Educational reverse engineering of WiFi communication

## Frame types parsed

- **Management**: Beacon, Probe Request/Response, Auth, Deauth, Association
- **Control**: RTS, CTS, ACK
- **Data**: QoS Data, Null Data

## License

MIT — for educational/defensive security research only.
