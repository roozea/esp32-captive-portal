# 🎭 ESP32 Captive Portal Standalone

**A standalone Evil Portal for ESP32-S3** — with persistent storage, admin panel, and REST API.

Perfect for the Electronic Cats WiFi Dev Board, but works on any ESP32-S3.

![License](https://img.shields.io/badge/license-MIT-green)
![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue)
![Version](https://img.shields.io/badge/version-1.2.0-brightgreen)
![Samsung](https://img.shields.io/badge/Samsung-auto--popup-success)

---

## ✨ Features

- **📱 Universal Compatibility** - Works with iPhone, Android, Samsung, Windows
- **💾 Persistent Storage** - Credentials survive reboots (SPIFFS)
- **🖥️ Admin Panel** - Beautiful dark-themed responsive dashboard
- **🔌 REST API** - Full API for integration with external tools
- **📊 Data Export** - Download logs in JSON or CSV format
- **⚙️ Dynamic Config** - Change SSID and admin credentials without reflashing
- **🔐 Authentication** - Protected admin area with HTTP Basic Auth
- **🆘 Factory Reset** - Emergency reset without reflashing

---

## 🆕 What's New in v1.2

- **🌐 Web Flasher** - Flash from browser, no Arduino IDE needed!
- **🆘 Factory Reset** - Access `/factory-reset` if you get locked out
- **📱 Responsive Design** - Admin panel works great on mobile
- **🔧 Improved Auth Flow** - No more double login prompts
- **🚪 Better Logout** - Clear instructions when changing passwords
- **🐛 Bug Fixes** - Fixed credential save/load issues

---

## 📥 Downloads

| Method | Link |
|--------|------|
| 🌐 **Web Flasher** | [Launch Web Flasher](https://roozea.github.io/esp32-captive-portal/) |
| 📦 **Pre-built Binary** | [GitHub Releases](https://github.com/roozea/esp32-captive-portal/releases) |
| 💻 **Source Code** | Clone this repo |

---

## ✅ Device Compatibility

| Device | Auto-popup? | Notes |
|--------|-------------|-------|
| iPhone | ✅ Yes | Instant popup |
| Android (Pixel, etc.) | ✅ Yes | Instant popup |
| Samsung | ✅ Yes | Thanks to IP 4.3.2.1 |
| Windows | ✅ Yes | Shows notification |

---

## ⚡ Quick Start

### Option 1: Web Flasher (Easiest!)

Flash directly from your browser - no software needed:

**🌐 [Launch Web Flasher](https://roozea.github.io/esp32-captive-portal/)**

Requirements:
- Chrome or Edge browser (desktop)
- USB data cable
- ESP32-S3 board

### Option 2: Arduino IDE

#### Requirements

- ESP32-S3 board (tested on Electronic Cats WiFi Dev Board)
- Arduino IDE 2.x
- ESP32 Arduino Core **2.0.x** (⚠️ NOT 3.x)

#### Installation

```bash
# 1. Install required libraries
cd ~/Documents/Arduino/libraries
git clone https://github.com/mathieucarbou/AsyncTCP
git clone https://github.com/mathieucarbou/ESPAsyncWebServer

# 2. Install ArduinoJson from Library Manager
#    Sketch > Include Library > Manage Libraries > Search "ArduinoJson"
```

3. Open `src/main.cpp` in Arduino IDE (or copy to `.ino` file)

4. Configure Arduino IDE:
   - Board: `ESP32S3 Dev Module`
   - USB CDC On Boot: `Enabled`
   - Partition Scheme: `Default 4MB with spiffs`

5. Upload and you're done!

---

## 🖥️ Admin Panel

Access the admin panel by connecting to the portal WiFi and navigating to:

```
http://4.3.2.1/admin
```

> 💡 **Tip:** If on mobile with cellular data, use `http://portal.local/admin` instead to avoid routing issues.

**Default credentials:**
- Username: `admin`
- Password: `admin`

⚠️ **Change these immediately in the Config section!**

### Pages

| Page | URL | Description |
|------|-----|-------------|
| Dashboard | `/admin` | Overview with stats and recent captures |
| Logs | `/admin/logs` | Full table of captured credentials |
| Config | `/admin/config` | Change SSID and admin credentials |
| Export | `/admin/export` | Download data as JSON/CSV |
| Logout | `/admin/logout` | End session |

### Factory Reset

If you ever get locked out:

```
http://4.3.2.1/factory-reset
```

This resets credentials to `admin/admin` without reflashing.

---

## 🔌 REST API

Base URL: `http://4.3.2.1/api/v1`

All API endpoints require HTTP Basic Authentication.

### Endpoints

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
| GET | `/export/json` | Download logs as JSON |
| GET | `/export/csv` | Download logs as CSV |
| POST | `/reboot` | Restart device |
| GET | `/dashboard` | Combined status + logs (optimized) |

### Example Usage

```bash
# Health check
curl http://4.3.2.1/ping

# Get status
curl -u admin:admin http://4.3.2.1/api/v1/status

# Get all logs
curl -u admin:admin http://4.3.2.1/api/v1/logs

# Export as JSON
curl -u admin:admin http://4.3.2.1/api/v1/export/json -o credentials.json

# Change SSID
curl -u admin:admin -X POST -H "Content-Type: application/json" \
  -d '{"ssid":"Coffee Shop WiFi"}' \
  http://4.3.2.1/api/v1/config

# Clear all logs
curl -u admin:admin -X DELETE http://4.3.2.1/api/v1/logs
```

---

## 💾 Storage Details

### SPIFFS Structure

```
/
├── config.json    # Portal configuration
└── logs.json      # Captured credentials
```

### Limits

- Maximum 100 credentials stored (FIFO - oldest deleted when full)
- Config changes persist across reboots
- Total SPIFFS size: ~1.5MB typical

---

## 🎨 Customization

### Change Default Network Name

Edit in the code:
```cpp
#define DEFAULT_SSID "Free_WiFi"
```

Or change dynamically via Admin Panel → Config.

### Portal Page Templates

Several templates are included in `/templates/`:

| Template | Description |
|----------|-------------|
| `free_wifi.html` | Generic free WiFi |
| `starbucks.html` | Coffee shop themed |
| `hotel.html` | Hotel guest WiFi |
| `airport.html` | Airport WiFi |
| `google_signin.html` | Google-style login |

### LED Pins

For non-Electronic Cats boards:
```cpp
#define LED_ACCENT_PIN 4   // Blue
#define LED_STATUS_PIN 5   // Green  
#define LED_ALERT_PIN  6   // Red
```

Set to `-1` to disable.

---

## 🔧 Troubleshooting

### Can't Login After Changing Password

1. Close **ALL** browser tabs
2. Open a new tab
3. Go to `/admin` and enter new credentials
4. If still stuck, go to `/factory-reset`

### SPIFFS Mount Failed

1. Tools → Partition Scheme → `Default 4MB with spiffs`
2. Tools → Erase All Flash Before Sketch Upload → `Enabled`
3. Upload again

### Serial Output Stops

Expected with IP 4.3.2.1 (Samsung compatibility). The portal works fine.

### ESP32 Core 3.x Errors

Downgrade to ESP32 Arduino Core 2.0.x.

### Mobile Data Conflicts

Use domain names instead of IP:
- `http://portal.local/admin`
- `http://setup.wifi/admin`

---

## 📦 Project Structure

```
esp32-captive-portal/
├── src/
│   └── main.cpp          # Main firmware
├── templates/            # HTML portal templates
├── docs/
│   ├── BUILDING.md       # Build instructions
│   └── KNOWN_ISSUES.md   # Known issues
├── bin/                  # Pre-compiled binaries
├── platformio.ini        # PlatformIO config
├── partitions_evilportal.csv
├── LICENSE
└── README.md
```

---

## 🔒 Security Notes

1. **Change default credentials immediately**
2. **Physical access required** - Admin only accessible on portal WiFi
3. **No encryption** - Data stored in plain text
4. **Rate limiting** - Basic brute force protection

---

## ⚠️ Legal Disclaimer

**For authorized security testing and education only.**

This tool is intended for:
- Penetration testing with written authorization
- Security awareness training
- Educational demonstrations
- Research purposes

Using this tool without permission is illegal. Authors are not responsible for misuse.

---

## 🙏 Credits

- [Electronic Cats](https://electroniccats.com/) — WiFi Dev Board
- [mathieucarbou](https://github.com/mathieucarbou) — Async libraries
- [CDFER](https://github.com/CDFER/Captive-Portal-ESP32) — Samsung research

---

## 📄 License

MIT — Use it, modify it, share it. Just don't be evil.
