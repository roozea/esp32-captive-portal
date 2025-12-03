# Changelog

All notable changes to Evil Portal Studio will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.3.0] - 2024-12-03

### Added
- 🔄 **Dynamic SSID Change** - Change network name instantly without restart via `/api/v1/ssid`
- 🎨 **Portal HTML Editor** - Edit captive portal HTML directly from web admin panel
- 📝 **Template Management** - Save, load, and manage HTML templates in `/admin/templates`
- 🔗 **Bidirectional Flipper Sync** - Web changes sync to Flipper Zero in real-time
- 🆕 New REST API endpoints:
  - `POST /api/v1/ssid` - Change SSID without restart
  - `GET /api/v1/portal-html` - Get current portal HTML
  - `POST /api/v1/portal-html` - Update portal HTML
  - `DELETE /api/v1/portal-html` - Reset to default HTML
  - `GET /api/v1/templates` - List saved templates
  - `POST /api/v1/templates` - Save new template

### Changed
- Admin panel now includes Templates page with visual editor
- Improved SPIFFS storage for custom HTML templates
- Better integration between Web Admin and Flipper Zero control

### Technical
- Templates stored in `/templates/` directory on SPIFFS
- Custom portal HTML stored as `/portal.html`
- Real-time SSID updates without WiFi AP restart

## [1.2.3] - 2024-12-02

### Fixed
- GitHub Actions workflow now uses correct PlatformIO environments (`standalone`, `flipper`)
- Added forward declarations for SPIFFS functions in Flipper edition
- Updated library versions for PlatformIO compatibility:
  - AsyncTCP: ^3.2.14
  - ESPAsyncWebServer: ^3.3.23
  - ArduinoJson: ^7.2.1
- Removed missing `rename_firmware.py` script reference

### Changed
- Both Standalone and Flipper editions now build correctly in CI/CD
- Web Flasher updated with edition selector (Standalone vs Flipper)
- README completely rewritten to document both editions

## [1.2.2] - 2024-12-01

### Fixed
- Build issues with partition file paths

## [1.2.1] - 2024-12-01

### Fixed
- Release workflow improvements

## [1.2.0] - 2024-12-01

### Added
- 🆘 **Factory Reset** - Access `/factory-reset` if you get locked out
- 📱 **Responsive Design** - Admin panel works great on mobile
- 🌐 **Web Flasher** - Flash from browser using ESP Web Tools, no Arduino IDE needed!
- 🟠 **Flipper Edition** - New firmware variant optimized for Flipper Zero integration

### Fixed
- 🔧 **Improved Auth Flow** - No more double login prompts
- 🚪 **Better Logout** - Clear instructions when changing passwords
- 🐛 **Credential Save/Load Issues** - Fixed SPIFFS persistence bugs

### Changed
- Admin panel redesigned with dark theme
- Improved mobile responsiveness
- Better error handling throughout

## [1.1.0] - 2024-11-30

### Added
- 💾 **Persistent Storage** - Credentials survive reboots (SPIFFS)
- 🖥️ **Admin Panel** - Beautiful dark-themed responsive dashboard
- 🔌 **REST API** - Full API with 11 endpoints for integration
- 📊 **Data Export** - Download logs in JSON or CSV format
- ⚙️ **Dynamic Config** - Change SSID and admin credentials without reflashing
- 🔐 **Authentication** - Protected admin area with HTTP Basic Auth

### Changed
- Migrated from simple serial output to full web interface
- Improved credential capture reliability
- Better captive portal detection across devices

## [1.0.0] - 2024-11-28

### Added
- Initial release
- 📱 **Universal Compatibility** - Works with iPhone, Android, Samsung, Windows
- Samsung auto-popup support using IP 4.3.2.1
- LED status indicators (Electronic Cats WiFi Dev Board)
- Serial output for credential capture
- Basic captive portal functionality

### Known Issues
- USB-CDC breaks after `WiFi.softAPConfig()` on ESP32-S3 (documented in KNOWN_ISSUES.md)
- Requires ESP32 Arduino Core 2.0.x (3.x not compatible)

---

## Version Naming

- **Standalone Edition**: Independent operation, no external devices
- **Flipper Edition**: Optimized for Flipper Zero integration

## Links

- [GitHub Repository](https://github.com/roozea/esp32-captive-portal)
- [Web Flasher](https://roozea.github.io/esp32-captive-portal/)
- [Releases](https://github.com/roozea/esp32-captive-portal/releases)
- [Issues](https://github.com/roozea/esp32-captive-portal/issues)
