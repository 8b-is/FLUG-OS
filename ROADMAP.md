# FLUG-OS Roadmap — to v1.4.2 and beyond

Version alignment: **MLX-QUANT v1.4.2** = full fused ternary path on Metal.
**FLUG-OS v1.4.2** = full fused hardware-software path: 802.11 capture →
ternary matrix → ayeOS inference → HF-MAC visualization.

---

## Legend

```
✅ shipped    🛠 in progress    ⏳ planned    🧪 experimental
```

---

## v0.3.x — Spectrum Foundation (current)

```
✅ Pure ASCII hero banner (agy-generated)
✅ Interactive menu: m d w t s q
✅ Real-time dashboard (14-channel RSSI + peak + activity)
✅ Waterfall display (14×40 activity grid over time)
✅ Top SSIDs by packet count + RSSI bar
✅ Channel scanner with best-channel recommendation
✅ Wave output: raw/wave/json modes via sine kernel
✅ 802.11 frame parsing: Beacon, Probe, Auth, Deauth, Data
✅ SSID extraction from beacon/probe response
✅ Channel hopping (1s per channel)
✅ Frame type filtering (mgmt/data/ctrl/all)
✅ Capture statistics (packet counts per type, RSSI min/max)
✅ Serial command interface
✅ PlatformIO build for ESP8285 + generic ESP8266
✅ CI with Blacksmith runner (build + test)
```

## v0.4.x — Hardware Validation

```
⏳ Flash to real ESP8285 hardware (waiting on USB-C cable)
⏳ Validate RSSI extraction from RxControl struct
⏳ Test channel hopping on real hardware
⏳ Verify SSID parsing with live beacons
⏳ Measure packet loss rate at 115200 baud
⏳ Calibrate RSSI thresholds for ternary encoding
⏳ Benchmark: max packets/sec before buffer overflow
⏳ Test with Flipper Zero DFU flash (LEFT+BACK)
⏳ Test with FTDI direct flash
```

## v1.0.0 — Stable Release

```
🛠 ML-DSA-65 + SLH-DSA firmware signing (CI done, needs release)
🛠 GitHub Pages landing site (deployed, pending content polish)
🧪 Binary release with signed firmware (.bin + .mldsa65.sig + .slhdsa.sig)
🧪 Homebrew tap for easy flashing: brew install flug-os
🧪 Generic ESP8266 board support (NodeMCU, Wemos D1 tested)
🧪 Flash documentation: DFU mode, FTDI, each OS (macOS/Linux/Windows)
🧪 Release checklist: tag → CI build → sign → publish → brew bump
```

## v1.1.x — MATRIX Integration

```
🧪 MATRIX decoder (src/matrix_decoder.h) — packet → ternary row
🧪 Real-time ternary matrix accumulation on ESP8285
🧪 MEMNET capsule export over UART (ayeOS-compatible JSON)
🧪 Feed packets directly into ayeOSd for inference
🧪 HF-MAC Ecosystem tab shows FLUG-OS capsule status
⏳ Verify packet→ternary accuracy against ayeOS matvec
⏳ Test with live network: 60s capture → ternary matrix → ayeOS inference
```

## v1.2.x — Wave Sonification

```
🧪 Audio output via PWM (headphone jack on ESP8285 GPIO)
🧪 Real-time sine wave synthesis from packet RSSI
🧪 MIDI note output over UART (serial MIDI)
🧪 OSC output over UDP (if WiFi is enabled for output)
🧪 Bridge to music.vaked.dev via HTTP (bridge/wave_bridge.py done)
🧪 Record wave sessions to .wav via bridge
```

## v1.3.x — Multi-Mode Capture

```
🧪 Beacon table: track all visible APs with RSSI history
🧪 Probe request logging: see devices scanning near you
🧪 Deauth detection: count + reason code analysis
🧪 Channel utilization % (airtime per channel)
🧪 Packet capture buffer: last N packets in RAM for replay
🧪 Export to pcap format over UART (Wireshark-compatible)
🧪 Promiscuous + monitor mode toggle
```

## v1.4.0 — Protocol Maturity

```
🧪 ESP32-S2 support (WiFi module v2)
🧪 5 GHz channel support (ESP32-S2)
🧪 OTA firmware updates via MEMNET
🧪 Web-based dashboard (serve config page over WiFi AP)
🧪 End-to-end test suite: FLUG-OS → MATRIX → ayeOS → HF-MAC
🧪 Formal security audit (post-quantum signing, memory safety)
```

## v1.4.2 — Parity with MLX-QUANT

```
🧪 Full fused path: packet → ternary → inference → visualization
🧪 Performance: 10,000+ packets/sec sustained
🧪 Memory: < 50KB RAM usage at full capture rate
🧪 All 14 channels: simultaneous RSSI sampling
🧪 Zero packet loss at 115200 baud
🧪 Deterministic MATRIX output: same packets → same ternary matrix
🧪 Complete ecosystem integration: FLUG-OS ↔ ayeOS ↔ HF-MAC
🧪 Hardware-software co-design: the ESP8285 as a ternary sensor
```

## Beyond v1.4.2

```
🧪 Multi-device mesh (multiple FLUG-OS nodes → collaborative spectrum map)
🧪 ML-based channel prediction (Quant1bitLLM on ESP8285)
🧪 Integrated honest-irc encryption (self-encrypted packet memory)
🧪 Flipper Zero application (graphical spectrum analyzer on Flipper screen)
🧪 ESP32-C5 support (first Espressif 5 GHz + 2.4 GHz combo chip)
```

---

**Current:** v0.3.0 (9 milestones shipped, waiting on hardware validation)
**Next milestone:** v0.4.0 (hardware validation — need USB-C cable)
**Target:** v1.4.2 (full MLX-QUANT parity — packet → ternary → inference → viz)

*Built by the 8b-is constellation. 🜂 {-1, 0, +1}*
