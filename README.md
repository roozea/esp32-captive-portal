# 🎭 Evil Portal Studio

**The Ultimate Evil Portal Firmware for ESP32-S3** — Two powerful editions for every use case.

![Version](https://img.shields.io/badge/version-1.3.0-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Platform](https://img.shields.io/badge/platform-ESP32--S3-orange)
![Samsung](https://img.shields.io/badge/Samsung-auto--popup-success)

---

## 🚀 Two Editions, One Goal

| Edition | Description | Best For |
|---------|-------------|----------|
| **🔷 Standalone** | Complete standalone captive portal with Admin Panel, REST API, and persistent storage | Independent operation, no external devices needed |
| **🟠 Flipper Edition** | Optimized for Flipper Zero integration via serial communication | Flipper Zero users who want portal control from their device |

---

## ✨ Features

### Common Features (Both Editions)

- 📱 **Universal Compatibility** - Works with iPhone, Android, Samsung, Windows
- 💾 **Persistent Storage** - Credentials survive reboots (SPIFFS)
- 🖥️ **Admin Panel** - Beautiful dark-themed responsive dashboard
- 🔌 **REST API** - Full API for integration with external tools
- 📊 **Data Export** - Download logs in JSON or CSV format
- ⚙️ **Dynamic Config** - Change SSID and admin credentials without reflashing
- 🔐 **Authentication** - Protected admin area with HTTP Basic Auth
- 🆘 **Factory Reset** - Emergency reset without reflashing
- 📱 **Responsive Design** - Admin panel works great on mobile

### 🔷 Standalone Edition Exclusive

- 🌐 **100% Independent** - No external devices required
- 💡 **LED Status Indicators** - Visual feedback for portal activity
- 🔄 **Auto-start Portal** - Boots directly into captive portal mode

### 🟠 Flipper Edition Exclusive

- 📟 **Flipper Zero Control** - Start/stop portal from Flipper menu
- 📡 **Serial Communication** - Real-time credential streaming to Flipper
- 🎨 **Custom Templates** - Load HTML templates from Flipper SD card
- 📋 **Live Logging** - See captures in real-time on Flipper screen

---

## ⚡ Quick Install

### 🌐 Web Flasher (Recommended)

Flash directly from your browser - no software installation needed!

**👉 [Open Web Flasher](https://roozea.github.io/esp32-captive-portal/)**

**Requirements:**

- ✅ Chrome or Edge browser (desktop)
- ✅ USB data cable (not charge-only)
- ✅ ESP32-S3 board

### 📦 Pre-compiled Binaries

Download from [GitHub Releases](https://github.com/roozea/esp32-captive-portal/releases/latest):

| File | Description |
|------|-------------|
| `evilportal-standalone-*.bin` | Standalone Edition firmware |
| `evilportal-flipper-*.bin` | Flipper Zero Edition firmware |

---

## 📱 Device Compatibility

| Device | Auto-popup? | Notes |
|--------|-------------|-------|
| iPhone | ✅ Yes | Instant popup |
| Android (Pixel, etc.) | ✅ Yes | Instant popup |
| Samsung | ✅ Yes | Thanks to IP 4.3.2.1 |
| Windows | ✅ Yes | Shows notification |
| MacOS | ✅ Yes | Instant popup |

---

## 🔷 Standalone Edition Guide

### Getting Started

1. **Flash the firmware** using the [Web Flasher](https://roozea.github.io/esp32-captive-portal/)
2. **Connect to the WiFi** network "Free_WiFi" (or your custom SSID)
3. **Access the Admin Panel** at `http://4.3.2.1/admin`

### Default Credentials

```
Username: admin
Password: admin
```

> ⚠️ **Change these immediately** in the Config panel!

### Admin Panel Pages

| Page | URL | Description |
|------|-----|-------------|
| Dashboard | `/admin` | Overview with stats and recent captures |
| Logs | `/admin/logs` | Full table of captured credentials |
| Config | `/admin/config` | Change SSID and admin credentials |
| Templates | `/admin/templates` | Visual HTML template editor |
| Export | `/admin/export` | Download data as JSON/CSV |
| Logout | `/admin/logout` | End session |

### Factory Reset

If you get locked out:

```
http://4.3.2.1/factory-reset
```

This resets credentials to `admin/admin` without reflashing.

### REST API

Base URL: `http://4.3.2.1/api/v1`

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/ping` | Health check (no auth) |
| GET | `/status` | System status and stats |
| GET | `/logs` | List all credentials |
| GET | `/logs?limit=5` | List last N credentials |
| DELETE | `/logs` | Clear all credentials |
| DELETE | `/logs/{id}` | Delete specific credential |
| GET | `/config` | Get current configuration |
| POST | `/config` | Update configuration |
| POST | `/ssid` | Change SSID without restart (v1.3+) |
| GET | `/portal-html` | Get current portal HTML |
| POST | `/portal-html` | Update portal HTML (v1.3+) |
| DELETE | `/portal-html` | Reset portal to default HTML |
| GET | `/templates` | List all saved templates |
| POST | `/templates` | Save a new template |
| GET | `/export/json` | Download logs as JSON |
| GET | `/export/csv` | Download logs as CSV |
| POST | `/reboot` | Restart device |

#### API Examples

```bash
# Health check
curl http://4.3.2.1/ping

# Get status
curl -u admin:admin http://4.3.2.1/api/v1/status

# Get all logs
curl -u admin:admin http://4.3.2.1/api/v1/logs

# Export as JSON
curl -u admin:admin http://4.3.2.1/api/v1/export/json -o credentials.json

# Change SSID (legacy - updates config)
curl -u admin:admin -X POST -H "Content-Type: application/json" \
  -d '{"ssid":"Coffee Shop WiFi"}' \
  http://4.3.2.1/api/v1/config

# Change SSID instantly without restart (v1.3+)
curl -u admin:admin -X POST -H "Content-Type: application/json" \
  -d '{"ssid":"New Network Name"}' \
  http://4.3.2.1/api/v1/ssid

# Get current portal HTML
curl -u admin:admin http://4.3.2.1/api/v1/portal-html

# Update portal HTML (v1.3+)
curl -u admin:admin -X POST -H "Content-Type: application/json" \
  -d '{"html":"<html><body><h1>Custom Portal</h1></body></html>"}' \
  http://4.3.2.1/api/v1/portal-html

# Reset portal to default HTML
curl -u admin:admin -X DELETE http://4.3.2.1/api/v1/portal-html

# List saved templates
curl -u admin:admin http://4.3.2.1/api/v1/templates

# Save a new template
curl -u admin:admin -X POST -H "Content-Type: application/json" \
  -d '{"name":"my_template","html":"<html>...</html>"}' \
  http://4.3.2.1/api/v1/templates
```

---

## 🟠 Flipper Edition Guide

### Hardware Setup

Connect your ESP32-S3 to the Flipper Zero:

| ESP32-S3 | Flipper Zero |
|----------|--------------|
| TX | RX (pin 14) |
| RX | TX (pin 13) |
| GND | GND |
| 3.3V | 3.3V |

> **Note:** For the Electronic Cats WiFi Dev Board, use the dedicated Flipper connector.

### Flipper Zero App

1. Install the **Evil Portal** app on your Flipper Zero
2. Copy your HTML templates to `/apps_data/evil_portal/` on the SD card
3. Connect the ESP32-S3 board
4. Open the Evil Portal app and select "Start Portal"

### Serial Commands

The Flipper Edition responds to these serial commands:

| Command | Description |
|---------|-------------|
| `START` | Start the captive portal |
| `STOP` | Stop the captive portal |
| `STATUS` | Get current status |
| `SSID:NetworkName` | Change the SSID |
| `HTML:...` | Load custom HTML template |

### LED Indicators

| Color | Meaning |
|-------|---------|
| 🔵 Blue | Booting / Initializing |
| 🟢 Green | Portal active, waiting for victims |
| 🔴 Red (blink) | Credentials captured! |

---

## 🛠️ Building from Source

### Requirements

- ESP32-S3 board (tested on Electronic Cats WiFi Dev Board)
- [PlatformIO](https://platformio.org/) or Arduino IDE 2.x
- ESP32 Arduino Core **2.0.x** (⚠️ NOT 3.x)

### Using PlatformIO (Recommended)

```bash
# Clone the repository
git clone https://github.com/roozea/esp32-captive-portal.git
cd esp32-captive-portal

# Build Standalone Edition
pio run -e standalone

# Build Flipper Edition
pio run -e flipper

# Upload Standalone Edition
pio run -e standalone -t upload

# Upload Flipper Edition
pio run -e flipper -t upload
```

### Using Arduino IDE

1. Install required libraries:

```bash
cd ~/Documents/Arduino/libraries
git clone https://github.com/mathieucarbou/AsyncTCP
git clone https://github.com/mathieucarbou/ESPAsyncWebServer
```

2. Install ArduinoJson from Library Manager

3. Open `src/main.cpp` (Standalone) or `src/main_flipper.cpp` (Flipper)

4. Configure Arduino IDE:
   - Board: `ESP32S3 Dev Module`
   - USB CDC On Boot: `Enabled`
   - Partition Scheme: `Default 4MB with spiffs`

5. Upload!

---

## 🎨 Customization

### Change Default SSID

**In code:**

```cpp
#define DEFAULT_SSID "Your_Network_Name"
```

**Or dynamically** via Admin Panel → Config

### Custom Portal Templates

Several templates included in `/templates/`:

| Template | Description |
|----------|-------------|
| `free_wifi.html` | Generic free WiFi |
| `starbucks.html` | Coffee shop themed |
| `hotel.html` | Hotel guest WiFi |
| `airport.html` | Airport WiFi |
| `google_signin.html` | Google-style login |

### LED Pin Configuration

For non-Electronic Cats boards:

```cpp
#define LED_ACCENT_PIN 4   // Blue
#define LED_STATUS_PIN 5   // Green
#define LED_ALERT_PIN  6   // Red
```

Set to `-1` to disable.

---

## 🐛 Troubleshooting

### "Serial stops after boot"

**Expected behavior** with IP 4.3.2.1 (Samsung compatibility). The portal works fine - this is a known ESP32-S3 USB-CDC bug. See [KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md).

### "Compilation error with Core 3.x"

Downgrade to ESP32 Arduino Core **2.0.x**. Version 3.x has breaking changes with AsyncTCP.

### "SPIFFS mount failed"

1. Tools → Partition Scheme → `Default 4MB with spiffs`
2. Tools → Erase All Flash Before Sketch Upload → `Enabled`
3. Upload again

### "Admin panel not loading on mobile"

Use domain names instead of IP:

```
http://portal.local/admin
http://setup.wifi/admin
```

### "Double login prompt"

Fixed in v1.2.0+. Make sure you're running the latest version.

---

## 📁 Project Structure

```
esp32-captive-portal/
├── src/
│   ├── main.cpp              # Standalone Edition
│   └── main_flipper.cpp      # Flipper Edition
├── templates/                 # HTML portal templates
├── webflasher/               # Web flasher page
├── docs/
│   ├── BUILDING.md           # Build instructions
│   └── KNOWN_ISSUES.md       # Known issues
├── .github/
│   └── workflows/
│       └── build.yml         # CI/CD workflow
├── platformio.ini            # PlatformIO config
├── partitions_evilportal.csv # Partition table
├── CHANGELOG.md              # Version history
├── LICENSE                   # MIT License
└── README.md                 # This file
```

---

## 🔒 Security Notes

- **Change default credentials immediately** after first boot
- **Physical access required** - Admin only accessible on portal WiFi
- **No encryption** - Data stored in plain text on SPIFFS
- **Rate limiting** - Basic brute force protection on auth endpoints

---

## ⚠️ Legal Disclaimer

**For authorized security testing and education only.**

This tool is intended for:

- ✅ Penetration testing with written authorization
- ✅ Security awareness training
- ✅ Educational demonstrations
- ✅ Research purposes

**Using this tool without permission is illegal.** Authors are not responsible for misuse.

---

## 🤝 Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

---

## 📜 Changelog

See [CHANGELOG.md](CHANGELOG.md) for version history.

### Latest: v1.3.0

- 🆕 **Dynamic SSID Change** - Change network name without restart via `/api/v1/ssid`
- 🆕 **Portal HTML Editor** - Edit captive portal HTML directly from web admin
- 🆕 **Template Management** - Save and load HTML templates in `/admin/templates`
- 🆕 **Bidirectional Flipper Sync** - Web changes sync to Flipper Zero in real-time
- ✅ New REST API endpoints for SSID, portal-html, and templates
- ✅ Visual template editor with live preview

---

## 🙏 Credits

- [Electronic Cats](https://electroniccats.com/) — WiFi Dev Board hardware
- [mathieucarbou](https://github.com/mathieucarbou) — Async libraries
- [CDFER](https://github.com/CDFER/Captive-Portal-ESP32) — Samsung IP research
- [bigbrodude6119](https://github.com/bigbrodude6119) — Flipper Evil Portal inspiration
- [justcallmekoko](https://github.com/justcallmekoko) — ESP32 Marauder inspiration

---

## 📄 License

MIT — Use it, modify it, share it. Just don't be evil.

---

<p align="center">
  <b>Made with ❤️ for the security research community</b>
</p>
