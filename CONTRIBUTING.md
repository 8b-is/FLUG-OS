# Contributing to FLUG-OS {-1, 0, +1}

First: thank you. Seriously. You're here. That's already the whole point.

## Principles

- **Ternary by design.** {-1, 0, +1} — no maybe. If a change isn't clearly additive (+1), clearly reductive (-1), or clearly neutral (0), sit with it until it is.
- **Keep it beautiful.** ASCII boot screen, clean JSON output, wave output that makes you *feel* the spectrum. This is firmware as poetry.
- **Post-quantum from day one.** Every release is dual-signed with ML-DSA + SLH-DSA. No shortcuts.
- **Defensive only.** This is for learning your own networks. Deauth detection for awareness, not destruction.

## What needs help

- **PlatformIO builds** on other ESP8266 boards (NodeMCU, Wemos D1, etc.)
- **Hardware testing** — particularly RSSI extraction on real ESP8285 hardware
- **Wave bridge** improvements — MIDI output, OSC support, WebSocket streaming
- **Documentation** — your first experience matters. If something confused you, fix it.
- **MATRIX decoder** — feed FLUG-OS packets into ayeOSd for real ternary inference

## Code style

- ESP8285 Arduino (C++17-ish)
- 4-space indentation, no tabs
- `static` everything that doesn't need to be extern
- `snprintf` with explicit buffer sizes — no sprintf without bounds
- Comments explain *why*, not *what* (the code says what)

## PR process

1. Open an issue describing what you're doing (keeps everyone in sync)
2. Fork, branch, code, test
3. PR with a clear description of what changed and why
4. CI must pass (Blacksmith build + PQC signing verification)

## License

MIT — educational/defensive security research only. By contributing, you agree that your contributions are under the same license.

---

*Built by the 8b-is constellation. 🜂 {-1, 0, +1}*
