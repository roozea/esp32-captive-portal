# 🌐 Evil Portal Studio - Web Flasher

Flash Evil Portal firmware directly from your browser - no software installation needed!

## 🚀 Live Web Flasher

**👉 [https://roozea.github.io/esp32-captive-portal/](https://roozea.github.io/esp32-captive-portal/)**

## ✨ Features

- **Two Editions**: Choose between Standalone or Flipper Edition
- **No Installation**: Works directly in Chrome/Edge browser
- **One-Click Flash**: Select your board and click Install

## 📋 Requirements

- ✅ Chrome or Edge browser (desktop)
- ✅ USB data cable (not charge-only)
- ✅ ESP32-S3 board

## 🔷 Editions

| Edition | Description |
|---------|-------------|
| **Standalone** | Independent operation, no external devices needed |
| **Flipper** | Optimized for Flipper Zero integration |

## 📁 Files

| File | Description |
|------|-------------|
| `index.html` | Web flasher interface |
| `manifest-standalone.json` | ESP Web Tools manifest for Standalone edition |
| `manifest-flipper.json` | ESP Web Tools manifest for Flipper edition |

## ⚠️ Notes

- Web Serial API only works on HTTPS (GitHub Pages provides this)
- iOS/Safari and mobile browsers are not supported
- If web flashing doesn't work, download the `.bin` from [Releases](https://github.com/roozea/esp32-captive-portal/releases) and use [esptool](https://esptool.spacehuhn.com/)

## 🔗 Links

- [Main Repository](https://github.com/roozea/esp32-captive-portal)
- [Releases](https://github.com/roozea/esp32-captive-portal/releases)
- [ESP Web Tools Documentation](https://esphome.github.io/esp-web-tools/)
