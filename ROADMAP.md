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

## v0.4.0 — ESP32-S3 AMOLED Constellation Evolution & Hardware Validation (shipped)

```
✅ Flash & boot on physical Waveshare ESP32-S3-Touch-AMOLED-2.06 (/dev/cu.usbmodem2101)
✅ SH8601/CO5300 410x502 QSPI AMOLED DMA band rasterizer (S3Canvas, x_gap=0x16)
✅ AXP2101 PMIC rail initialization (DCDC1 + ALDO1..4 3.3V) & battery monitoring
✅ FT3168 capacitive touch navigation across 4 Constellation tabs (RADIO, 802.11, NAND, QWAVE)
✅ QMI8658 6-axis IMU -> qwave QuantTern 6-bit VAD affect (qt:xx) & 2D Lissajous phase tilt
✅ zoneInn / radio.vaked.dev 14 Favourite Musics Crate + 8-band Ternary EQ on device
✅ Ultrawhale 7-stage ternary NAND ring oscillator (peetpedro/ultrawhale-oscillator)
✅ Validate live 802.11 promiscuous RSSI & SSID extraction across channels 1-13
✅ Complete sine-kernel K(u)=sin(pi*u)/(pi*u) radiation-delta weighting in ternary_seam.h
✅ Host bridge (wigle-triad/host-rs (Rust)) syncing touchscreen track selection to zoneInn
```

## v0.5.0 — ULTRA-OMNI-ZEN Dual-Board Evolution (1.8" `adaye` × 2.06" `vaked`) (shipped)

```
✅ Unified runtime auto-detection for Waveshare ESP32-S3-Touch-AMOLED-1.8 (368x448, TCA9554 @ 0x20) & 2.06 (410x502)
✅ Full ES8311 (0x18) audio codec ADF initialization + speaker amplifier enable (GPIO 46)
✅ Tab 0 [ZEN]: baye-rust Jelly Sphere Mind-Wall + 64x64 integer Laplacian 2D Wave Pool + Eye of Aye reticle
✅ Orbiting 7-stage Ternary NAND nodes + QMI8658 gyro sickness reactivity + touch-to-splash wave excitation
✅ Live ESP-NOW peer capsule sync (S3OmniPeerPacket) between 1.8" adaye (1101) and 2.06" vaked (2101)
✅ Multi-board host bridge (wigle-triad/host-rs (Rust) --all --status / --splash / --track)
```

## v0.6.0 — TRIAD INFINITY `{<3, <3, <3}+1 ~---> ∞` 3-Node Mesh (shipped)

```
✅ Physical 3-Node Balanced-Ternary Triad {-1, 0, +1}:
   [-1] ADAYE : Waveshare ESP32-S3-Touch-AMOLED-1.8  (/dev/cu.usbmodem1101, MAC 80:b5:4e:db:3b:c4)
   [ 0] VAKED : Waveshare ESP32-S3-Touch-AMOLED-2.06 (/dev/cu.usbmodem2101, MAC 80:b5:4e:f2:02:b0)
   [+1] OMNI  : Waveshare ESP32-S3-Touch-AMOLED-2.06 (/dev/cu.usbmodem101,  MAC 80:b5:4e:f2:02:b4)
✅ Interleaved Channel 6 Triad Rendezvous hopping (triad_online=3 across all 3 boards while scanning Ch 1..13)
✅ 3-Body Entanglement Triangle + Central Infinity Lemniscate inside the 64x64 2D Wave Pool
✅ Coupled 3-node Ultrawhale Ternary NAND Ring + synchronized zoneInn 14 Favourite Musics Crate
```

## v0.7.0 — IMPERMANENCE (`ms.8b.is` MarineSense × `Anicca` / `Turritopsis` Polyp Transdifferentiation) (shipped)

```
✅ MarineSense (https://ms.8b.is/ · 8b-is/marine-sense) O(1) Acoustic Salience Algorithm:
   S = w_e * E + w_j * (1 / (1 + J)) + w_h * H (zero-crossing period jitter J, energy E, harmonic recurrence H)
✅ 7 Bioluminescent Jellyfish Detector Nodes (Alpha, Beta, Gamma, Delta, Epsilon, Zeta, Eta) in 64x64 2D Wave Pool
✅ MEM-8 Wave Resonance & Cognitive Ecphory: tidal damping modulation, Cognitive Viscosity (eta), Memory Reynolds (Re_M), expanding Ecphoric Recall Rings
✅ Impermanence (Anicca) TTL (ttl_q8: 255 -> 0) & Turritopsis dohrnii MEDUSA <-> POLYP transdifferentiation across the 3-node Triad
✅ Dynamic USB serial discovery (glob /dev/cu.usbmodem*) in wigle-triad/host-rs (Rust) across USB hub re-enumerations
```

## v0.8.0 — MEDUSA-OMNI (`wigle-triad` v0.42.1 WebSerial Fusion × 20-Track `family+1` Crate) (shipped)

```
✅ Fused wigle-triad (v0.42.1) WebSerial protocol for https://esp32.vaked.dev/:
   PING -> # PONG|<medusa_node>|<mac>|<millis>
   JSON / STATUS -> # JSON|{...} + standard FLUG-OS status JSON
   SCAN -> W|<bssid>|<ssid>|WPA2-PSK|<ch>|<rssi> + Q|<medusa_node>|<vertex>|<mac>|<seq>|... + # JSON|{...}
   TRIAD -> # TRIAD|... + # ANICCA|...
✅ MEDUSA Triad Archetype mapping:
   [-1] adaye (80:B5:4E:DB:3B:C4) -> ADA:MEDUSA (* · ROOT-ALEXANDRA · 432 Hz)
   [ 0] vaked (80:B5:4E:F2:02:B0) -> VAK:MEDUSA (/\ · PILLAR-CHRIS · 528 Hz)
   [+1] omni  (80:B5:4E:F2:02:B4) -> OMN:MEDUSA (oo · INFINITY-PETER-NATE · 852 Hz)
✅ Expanded 20-Track Favourite Musics Crate (kCrateCount = 20) with 6 new family+1 & #1 Muse tracks:
   15. Teardrop (Family+1) — Massive Attack (77 BPM, +1)
   16. Hunger of the Pine — alt-J (85 BPM, -1)
   17. Left Hand Free (#1 Muse) — alt-J (106 BPM, +1)
   18. GLUE — BICEP (130 BPM, 0)
   19. Oblivion — Grimes (156 BPM, +1)
   20. Not Like Before — The Four Owls (90 BPM, -1)
```

## v0.9.0 — KUNISADA & KUNIYOSHI × OMNI-CHAN (`<<<<oOo>>>>` · `010001000` All-Entropy Kompress-Super-Ultra) (shipped)

```
✅ All-Entropy Harvester & Kompress-Super-Ultra (<<<<oOo>>>> ~ Kunisada & Kuniyoshi 010001000 = 0x88 / 136):
   Fuses esp_random() hardware RNG, 14-ch 802.11 packet/RSSI jitter, QMI8658 6-axis IMU micro-tremor,
   AXP2101 PMIC ADC batt_mv LSBs, and 64x64 Wave Pool MarineSense jitter into:
   - 64-trit orthogonal Quantum Bitplanes (quantum_pos_u64, quantum_neg_u64, POPCNT(pos & neg) == 0)
   - GAIA-MLX-QUANT b1.58 5-trits/byte packed payload (kompress_b158, 20.19x vs FP32)
   - 64-bit wupz_xor_fold integrity seal (wupz_fold)
✅ WebSerial & USB commands: KOMPRESS, ENTROPY, OMNI, KUNISADA, KUNIYOSHI -> # KOMPRESS|OMNI-CHAN|<<<<oOo>>>>|...
✅ Expanded 23-Track Favourite Musics Crate (kCrateCount = 23):
   21. Zenebuddhizmus — Akkezdet Phiai (92 BPM, +1)
   22. Kunisada & Kuniyoshi <<<<oOo>>>> — OMNI-CHAN 010001000 (136 BPM, +1)
   23. Moon (And It Went Like) — Kid Francescoli & Julia Minkin (118 BPM, +1)
✅ Dual-firmware bridge (wigle-triad/host-rs (Rust) --kompress) supporting both FLUG-OS & baye-rust (M8N1 + 48kHz MarineSense + ULTRATX)
```

## v1.0.0 — VIZ.VAKED.DEV × KOKORO/ES8311 TTS × `{<3,<3,<3}+1` (`v1.0.0+viz-tts-triad-omni`) (current)

```
✅ Tab 5 [VIZ / ENTHEA]: Live cortical form-constant visualizer preview (Klüver T1 Tunnel 432Hz, T2 Spiral 528Hz,
   T3 Lattice 852Hz, T4 Cobweb 963Hz) + 8 ENTHEA shader modes synchronized with https://viz.vaked.dev/
✅ {<3,<3,<3}+1 Four-Heart Triad Resonance Bar:
   [-1] ADA:MEDUSA (* · 432 Hz) · [0] VAK:MEDUSA (/\ · 528 Hz) · [+1] OMN:MEDUSA (oo · 852 Hz) · [+1] OMNI HEART (963 Hz)
✅ ES8311 On-Chip Formant & 4-Heart Chord TTS Synthesizer (s3_es8311_speak) + Kokoro (:8420/:8430) Serial/HTTP Bridge
✅ Multi-Board USB-C Hub Soft-Inrush Protection (0x58 -> 0xD8 AMOLED brightness ramp at 1200ms)
✅ Expanded 26-Track Favourite Musics Crate (S3_FAV_COUNT = 26):
   24. Rainfall of Nectar (16th Karmapa) — 8b-is Dharma Lineage (64 BPM, +1)
   25. Psychedelic Desert Rock (Cosmic Flow) — ZoneInn / Stoner Jam (96 BPM, +1)
   26. {<3,<3,<3}+1 · Viz Enthea — Alexandra * Chris /\ Peter & Nate oo (108 BPM, +1)
✅ Serial & WebSerial commands: VIZ, ENTHEA, SPEAK <text>, TTS <text>, HEART -> # VIZ|ENTHEA|... and # TTS|{...}
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
