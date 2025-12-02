# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2.0] - 2024-12-02

### Added
- **Web Flasher** - Flash firmware directly from browser using ESP Web Tools
- **Factory Reset endpoint** (`/factory-reset`) - Reset credentials without reflashing
- **Responsive design** - Admin panel works great on mobile devices
- **Debug logging** - Detailed serial output for troubleshooting
- **Health check endpoint** (`/ping`) - Quick connectivity test
- **GitHub Actions** - Automated builds and GitHub Pages deployment
- **Merged binary** - Single file for easy flashing

### Changed
- Improved logout flow with clear instructions
- Better error handling for API endpoints (JSON errors instead of auth popups)
- Optimized dashboard endpoint for faster loading
- Enhanced password change flow with user guidance

### Fixed
- Double login prompt after changing password
- Logout causing infinite authentication loop
- SPIFFS initialization failures causing boot loops
- API endpoints not properly returning JSON on auth failure

## [1.1.0] - 2024-12-01

### Added
- **Persistent Storage** - Credentials stored in SPIFFS, survive reboots
- **Admin Panel** - Web-based dashboard at `/admin`
  - Dashboard with stats and recent captures
  - Logs page with full capture history
  - Config page for SSID and credential changes
  - Export page for JSON/CSV download
- **REST API** - Full API at `/api/v1/`
  - Status, logs, config, export endpoints
  - Authentication required for all endpoints
- **HTTP Basic Auth** - Protected admin area
- **Rate Limiting** - Basic brute force protection
- **FIFO Log Management** - Auto-delete oldest when at 100 max

### Changed
- Migrated from standalone sketch to structured project
- Improved Samsung compatibility documentation

## [1.0.0] - 2024-11-30

### Added
- Initial release
- Standalone captive portal for ESP32-S3
- Samsung auto-popup support via IP 4.3.2.1
- RGB LED status indicators
- Credential capture with serial output
- Support for multiple captive portal detection endpoints:
  - Android: `/generate_204`, `/gen_204`
  - iOS: `/hotspot-detect.html`, `/library/test/success.html`
  - Windows: `/connecttest.txt`, `/ncsi.txt`
  - Firefox: `/canonical.html`

### Known Issues
- USB-CDC serial breaks after WiFi.softAPConfig() on ESP32-S3
- Requires ESP32 Arduino Core 2.0.x (3.x not compatible)
