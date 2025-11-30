# 🎭 ESP32 Captive Portal Standalone

**A standalone Evil Portal for ESP32-S3** — no Flipper Zero, no Marauder, no dependencies. Just flash and go.

Perfect for the Electronic Cats WiFi Dev Board, but works on any ESP32-S3.

![License](https://img.shields.io/badge/license-MIT-green)
![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue)
![Samsung](https://img.shields.io/badge/Samsung-auto--popup-success)
![Flipper](https://img.shields.io/badge/Flipper_Zero-not_required-orange)

---

## 🤔 Why does this exist?

If you've tried running Evil Portal with the Electronic Cats WiFi Dev Board (or any ESP32-S3), you probably hit these walls:

1. **Samsung phones don't auto-popup** the captive portal with the default IP
2. **Changing the IP to fix Samsung breaks serial communication** — a bug in ESP32-S3's USB-CDC

This firmware says "screw it" and runs **completely standalone**. No serial needed in the field anyway.

### The USB-CDC Bug (for the curious)

On ESP32-S3 with native USB, calling `WiFi.softAPConfig()` with a custom IP kills USB serial permanently. We've [reported this to Espressif](#known-issues).

**Our solution:** Don't fight it. Run standalone, capture credentials, blink LEDs. Done.

---

## ✅ What works

| Device | Auto-popup? | Notes |
|--------|-------------|-------|
| iPhone | ✅ Yes | Instant popup |
| Android (Pixel, etc.) | ✅ Yes | Instant popup |
| Samsung | ✅ Yes | Thanks to IP 4.3.2.1 |
| Windows | ✅ Yes | Shows notification |
| Flipper Zero | ❌ Not needed | This is standalone! |

---

## ⚡ Quick Start

### Requirements

- ESP32-S3 board (tested on Electronic Cats WiFi Dev Board)
- Arduino IDE 2.x
- ESP32 Arduino Core **2.0.x** (⚠️ NOT 3.x — causes compilation errors)

### Installation

```bash
# 1. Install required libraries
cd ~/Documents/Arduino/libraries
git clone https://github.com/mathieucarbou/AsyncTCP
git clone https://github.com/mathieucarbou/ESPAsyncWebServer

# 2. Download this repo
git clone https://github.com/YOUR_USERNAME/esp32-captive-portal
```

3. Open `EvilPortal_Standalone.ino` in Arduino IDE

4. Configure Arduino IDE:
   - Board: `ESP32S3 Dev Module`
   - USB CDC On Boot: `Enabled`

5. Upload and you're done!

---

## 🎨 Customization

**Change network name** (line ~15):
```cpp
const char* WIFI_SSID = "Starbucks Wifi";
```

**Change login page:**
Edit the HTML inside `index_html`. We include templates in `/templates/`.

**LED pins** (for non-Electronic Cats boards):
```cpp
#define LED_ACCENT_PIN 4   // Blue - change to your pins
#define LED_STATUS_PIN 5   // Green
#define LED_ALERT_PIN  6   // Red
```

---

## 📦 Templates included

| Template | Description |
|----------|-------------|
| `EvilPortal_Standalone.ino` | Clean, generic "Free WiFi" |
| `templates/EvilPortal_Starbucks.ino` | Coffee shop themed |

Want to contribute a template? PRs welcome!

---

## 🔧 Known Issues

### ESP32 Core 3.x doesn't work

Use **2.0.17** or similar. Version 3.x has breaking changes.

### Serial stops working after boot

This is expected! The IP change that enables Samsung support breaks USB-CDC. Credentials are still captured — you just can't see them in Serial Monitor.

We've reported this bug to Espressif. See `docs/KNOWN_ISSUES.md` for the full technical deep-dive.

---

## 🤝 Related projects

| Project | What it does | Samsung? | Standalone? |
|---------|--------------|----------|-------------|
| [ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder) | Full WiFi/BT toolkit | ❌ | ❌ |
| [Flipper Evil Portal](https://github.com/bigbrodude6119/flipper-zero-evil-portal) | Portal via Flipper | ❌ | ❌ |
| **This project** | Just captive portal | ✅ | ✅ |

Use Marauder if you need the full toolkit. Use this if you just want a working portal that actually triggers on Samsung.

---

## ⚠️ Legal

**For authorized security testing only.** Get permission before testing networks you don't own.

---

## 🙏 Credits

- [justcallmekoko](https://github.com/justcallmekoko) — ESP32 Marauder inspiration
- [bigbrodude6119](https://github.com/bigbrodude6119) — Evil Portal HTML ideas
- [Electronic Cats](https://electroniccats.com/) — WiFi Dev Board hardware
- [CDFER](https://github.com/CDFER/Captive-Portal-ESP32) — Samsung IP research

---

## 📄 License

MIT — Use it, modify it, share it. Just don't sue me.
