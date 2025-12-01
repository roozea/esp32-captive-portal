# 🎭 ESP32 Captive Portal Standalone v1.1

**A standalone Evil Portal for ESP32-S3** — now with persistent storage, admin panel, and REST API.

Perfect for the Electronic Cats WiFi Dev Board, but works on any ESP32-S3.

![License](https://img.shields.io/badge/license-MIT-green)
![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue)
![Version](https://img.shields.io/badge/version-1.1.0-brightgreen)
![Samsung](https://img.shields.io/badge/Samsung-auto--popup-success)

---

## 🆕 What's New in v1.1

- **💾 Persistent Storage** - Credentials survive reboots (stored in SPIFFS)
- **🖥️ Admin Panel** - Web-based dashboard to view/manage captured data
- **🔌 REST API** - Full API for integration with external tools
- **📊 Data Export** - Download logs in JSON or CSV format
- **⚙️ Dynamic Config** - Change SSID and admin credentials without reflashing
- **🔐 Authentication** - Protected admin area with HTTP Basic Auth

---

## ✅ What works

| Device | Auto-popup? | Notes |
|--------|-------------|-------|
| iPhone | ✅ Yes | Instant popup |
| Android (Pixel, etc.) | ✅ Yes | Instant popup |
| Samsung | ✅ Yes | Thanks to IP 4.3.2.1 |
| Windows | ✅ Yes | Shows notification |

---

## ⚡ Quick Start

### Requirements

- ESP32-S3 board (tested on Electronic Cats WiFi Dev Board)
- Arduino IDE 2.x
- ESP32 Arduino Core **2.0.x** (⚠️ NOT 3.x)

### Installation

```bash
# 1. Install required libraries
cd ~/Documents/Arduino/libraries
git clone https://github.com/mathieucarbou/AsyncTCP
git clone https://github.com/mathieucarbou/ESPAsyncWebServer

# 2. Install ArduinoJson from Library Manager
#    Sketch > Include Library > Manage Libraries > Search "ArduinoJson"
```

3. Open `EvilPortal_v1.1.ino` in Arduino IDE

4. Configure Arduino IDE:
   - Board: `ESP32S3 Dev Module`
   - USB CDC On Boot: `Enabled`
   - Partition Scheme: `Default 4MB with spiffs` (or any scheme with SPIFFS)

5. Upload and you're done!

---

## 🖥️ Admin Panel

Access the admin panel by connecting to the portal WiFi and navigating to:

```
http://4.3.2.1/admin
```

**Default credentials:**
- Username: `admin`
- Password: `admin`

⚠️ **Change these immediately in the Config section!**

### Dashboard Features

| Page | URL | Description |
|------|-----|-------------|
| Dashboard | `/admin` | Overview with stats and recent captures |
| Logs | `/admin/logs` | Full table of captured credentials |
| Config | `/admin/config` | Change SSID and admin credentials |
| Export | `/admin/export` | Download data as JSON/CSV |

### Screenshots

The admin panel features a dark theme optimized for field use:
- Real-time credential count
- Uptime and memory monitoring
- One-click data export
- Auto-refreshing log view

---

## 🔌 REST API

Base URL: `http://4.3.2.1/api/v1`

All API endpoints require HTTP Basic Authentication with admin credentials.

### Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
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

### Example Usage

```bash
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

### Response Examples

**GET /api/v1/status**
```json
{
  "version": "1.1.0",
  "uptime": 3600,
  "ssid": "Free_WiFi",
  "ip": "4.3.2.1",
  "credentials_count": 5,
  "memory_free": 180000,
  "spiffs_used": 12000,
  "spiffs_total": 1500000,
  "default_creds": false
}
```

**GET /api/v1/logs**
```json
{
  "count": 2,
  "logs": [
    {
      "id": 1,
      "timestamp": 1234567890,
      "email": "victim@email.com",
      "password": "secret123",
      "ssid": "Free_WiFi",
      "client_ip": "4.3.2.100"
    },
    {
      "id": 2,
      "timestamp": 1234567900,
      "email": "another@email.com",
      "password": "hunter2",
      "ssid": "Free_WiFi",
      "client_ip": "4.3.2.101"
    }
  ]
}
```

---

## 💾 Storage Details

### SPIFFS File Structure

```
/
├── config.json    # Portal configuration
└── logs.json      # Captured credentials
```

### Limits

- Maximum 100 credentials stored (FIFO - oldest deleted when full)
- Config changes persist across reboots
- Total SPIFFS size depends on partition scheme (~1.5MB typical)

---

## 🎨 Customization

### Change Default Network Name

Edit in the code before uploading:
```cpp
#define DEFAULT_SSID "Free_WiFi"
```

Or change dynamically via Admin Panel → Config after deployment.

### Change Portal Page Design

Edit the `portal_html` constant in the firmware. The HTML includes:
- Responsive design
- Modern styling
- Form that captures email + password
- Success message after submission

### LED Pins

For non-Electronic Cats boards, adjust these pins:
```cpp
#define LED_ACCENT_PIN 4   // Blue
#define LED_STATUS_PIN 5   // Green  
#define LED_ALERT_PIN  6   // Red
```

Set to `-1` to disable if your board doesn't have RGB LED.

---

## 🔧 Troubleshooting

### SPIFFS Mount Failed

If you see `[!] SPIFFS mount failed`:
1. Make sure you selected a partition scheme with SPIFFS
2. Try: Tools → ESP32 Sketch Data Upload (to format SPIFFS)
3. Or enable SPIFFS formatting on first boot (already enabled in code)

### Serial Output Stops After Boot

This is expected when using IP 4.3.2.1 (Samsung compatibility). See `docs/KNOWN_ISSUES.md` for details. The portal works fine - you just can't see serial output.

### Admin Panel Won't Load

1. Make sure you're connected to the portal WiFi
2. Use `http://` not `https://`
3. Try `http://4.3.2.1/admin` directly

### ESP32 Core 3.x Errors

Downgrade to ESP32 Arduino Core 2.0.x. Version 3.x has breaking changes.

---

## 📦 Templates

| Template | File | Description |
|----------|------|-------------|
| Generic | `EvilPortal_v1.1.ino` | Clean "Free WiFi" design |
| Starbucks | `templates/EvilPortal_Starbucks.ino` | Coffee shop themed (v1.0) |

Want to contribute a template? PRs welcome!

---

## 🔒 Security Notes

1. **Change default credentials immediately** - The admin panel warns you if using defaults
2. **Physical access required** - Admin panel only accessible when connected to portal
3. **No encryption** - Data stored in plain text on SPIFFS (physical access = data access)
4. **Rate limiting** - Basic protection against brute force (10 requests/second)

---

## 🚀 Roadmap (v1.2+)

- [ ] BLE communication with Flipper Zero / mobile app
- [ ] Multiple HTML templates (switchable from admin)
- [ ] Statistics dashboard with charts
- [ ] Webhook notifications on capture
- [ ] Stealth mode (hide admin panel)

---

## ⚠️ Legal Disclaimer

**For authorized security testing and education only.**

This tool is intended for:
- Penetration testing with written authorization
- Security awareness training
- Educational demonstrations
- Research purposes

Using this tool against networks without explicit permission is illegal. The authors are not responsible for misuse.

---

## 🙏 Credits

- [justcallmekoko](https://github.com/justcallmekoko) — ESP32 Marauder inspiration
- [bigbrodude6119](https://github.com/bigbrodude6119) — Evil Portal concepts
- [Electronic Cats](https://electroniccats.com/) — WiFi Dev Board hardware
- [CDFER](https://github.com/CDFER/Captive-Portal-ESP32) — Samsung IP research
- [mathieucarbou](https://github.com/mathieucarbou) — Async libraries maintenance

---

## 📄 License

MIT — Use it, modify it, share it. Just don't use it for evil (unauthorized testing).
