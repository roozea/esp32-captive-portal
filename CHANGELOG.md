# Changelog

All notable changes to Evil Portal Studio will be documented in this file.

## [1.3.0] - 2024-12-02

### 🎉 Major New Features

#### Dynamic SSID Change (No Restart!)
- **NEW** `POST /api/v1/ssid` endpoint for instant SSID changes
- WiFi network name updates immediately without device restart
- Changes persist to SPIFFS automatically

#### HTML Template Editor
- **NEW** `/admin/templates` page with full HTML editor
- Live preview of templates before applying
- Syntax highlighting and responsive design

#### Template Management System
- **NEW** `POST /api/v1/templates` - Save templates to SPIFFS
- **NEW** `GET /api/v1/templates` - List all saved templates
- **NEW** `GET /api/v1/templates/{name}` - Load specific template
- **NEW** `DELETE /api/v1/portal-html` - Reset to default template

#### Bidirectional Flipper Sync
- Web changes notify Flipper via UART: `ssid_changed:`, `html_changed`
- Unified state management between Web and Flipper sources
- Source tracking: know if change came from Web, Flipper, or default

### 🔧 Improvements

- **Config Page**: SSID field now shows "LIVE" badge, instant apply button
- **Dashboard**: Shows HTML source (Default/Web Admin/Flipper Zero)
- **Dashboard**: Flipper connection status with timeout detection
- **API**: `/api/v1/status` includes `html_source` and `flipper_connected`
- **API**: `/api/v1/config` includes `active_ssid` separate from stored `ssid`

### 🏗️ Architecture Changes

- Introduced `activeSSID` and `activeHtml` as unified runtime state
- Added `ChangeSource` enum to track origin of changes
- Flipper timeout detection (30 seconds without activity)
- Templates stored in `/templates/` directory on SPIFFS
- Active HTML stored in `/active_html.html` for persistence

### 📡 New Flipper Notifications

When changes are made from the web admin:
- `ssid_changed: NewSSID` - Sent when SSID changes from web
- `html_changed` - Sent when HTML template changes from web

### 🛠️ Technical Details

- Maximum HTML size: 20KB (unchanged)
- Maximum templates: 10 (soft limit based on SPIFFS space)
- Flipper timeout: 30 seconds
- All changes persist across reboots

---

## [1.2.3] - Previous Release

- Web flasher support
- ESP32 Core 3.x compatibility
- Bug fixes for SPIFFS mounting

## [1.2.0] - Previous Release

- Initial Flipper Zero Edition
- SPIFFS credential storage
- Web admin panel
- REST API with 11 endpoints
- Samsung auto-popup support (IP 4.3.2.1)

---

## Migration Guide: v1.2.x → v1.3.0

### Breaking Changes
None! v1.3.0 is fully backward compatible.

### New Config File Format
The `/config.json` format is unchanged, but a new file `/active_html.html` may be created when custom HTML is applied from web.

### Flipper Compatibility
Existing Flipper commands (`setap=`, `sethtml=`, `start`, `stop`) continue to work exactly as before. New notification messages are informational only.

### Recommended Actions
1. Update firmware via Web Flasher or PlatformIO
2. Factory reset is NOT required
3. Existing credentials and config are preserved
