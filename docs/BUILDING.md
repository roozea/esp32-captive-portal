# 🔧 Building the Firmware

This guide explains how to compile the firmware and generate the `.bin` file for web flashing.

## Option 1: Arduino IDE (Recommended for beginners)

### Step 1: Install Arduino IDE
Download from https://www.arduino.cc/en/software

### Step 2: Install ESP32 Board Support
1. Open Arduino IDE
2. Go to **File > Preferences**
3. Add to "Additional Boards Manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Go to **Tools > Board > Boards Manager**
5. Search "esp32"
6. Install **"esp32 by Espressif Systems"** version **2.0.17** (NOT 3.x!)

### Step 3: Install Libraries
1. Go to **Sketch > Include Library > Manage Libraries**
2. Install **ArduinoJson** by Benoit Blanchon (v7.x)

3. Install AsyncTCP and ESPAsyncWebServer manually:
   ```bash
   cd ~/Documents/Arduino/libraries
   git clone https://github.com/mathieucarbou/AsyncTCP
   git clone https://github.com/mathieucarbou/ESPAsyncWebServer
   ```

### Step 4: Configure Board
In Arduino IDE:
- **Tools > Board**: `ESP32S3 Dev Module`
- **Tools > USB CDC On Boot**: `Enabled`
- **Tools > Partition Scheme**: `Default 4MB with spiffs`
- **Tools > Flash Size**: `4MB`

### Step 5: Export Binary
1. Open `src/main.cpp` (rename to `EvilPortal_v1.1.ino` if needed)
2. Go to **Sketch > Export Compiled Binary**
3. The `.bin` file will be in the sketch folder under `build/`

### Step 6: Find Your Binary
The exported files will be in:
```
YourSketchFolder/build/esp32.esp32.esp32s3/
├── EvilPortal_v1.1.ino.bin           # Main firmware
├── EvilPortal_v1.1.ino.bootloader.bin
└── EvilPortal_v1.1.ino.partitions.bin
```

Copy `EvilPortal_v1.1.ino.bin` to the `bin/` folder.

---

## Option 2: PlatformIO (Recommended for developers)

### Step 1: Install PlatformIO
```bash
pip install platformio
```

Or install the VSCode extension: https://platformio.org/install/ide?install=vscode

### Step 2: Build
```bash
cd esp32-captive-portal
pio run -e esp32s3
```

### Step 3: Find Your Binary
The compiled firmware will be at:
```
.pio/build/esp32s3/firmware.bin
```

Copy to `bin/` folder:
```bash
cp .pio/build/esp32s3/firmware.bin bin/EvilPortal_v1.1_esp32s3.bin
```

---

## Option 3: Command Line with arduino-cli

### Step 1: Install arduino-cli
```bash
# macOS
brew install arduino-cli

# Linux
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh

# Windows: Download from https://arduino.github.io/arduino-cli/
```

### Step 2: Configure
```bash
arduino-cli config init
arduino-cli core update-index
arduino-cli core install esp32:esp32@2.0.17

# Install libraries
arduino-cli lib install "ArduinoJson"
```

### Step 3: Compile
```bash
cd esp32-captive-portal/src

arduino-cli compile \
  --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,PartitionScheme=default_4mb \
  --output-dir ../bin \
  main.cpp
```

---

## Web Flasher Setup

Once you have the `.bin` file, you can set up a web flasher:

### Using ESP Web Tools

1. Create an `index.html` for your web flasher:

```html
<!DOCTYPE html>
<html>
<head>
    <title>Evil Portal Web Flasher</title>
    <script type="module" src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"></script>
</head>
<body>
    <h1>Evil Portal v1.1 Web Flasher</h1>
    <esp-web-install-button manifest="manifest.json">
        <button slot="activate">Install Evil Portal</button>
    </esp-web-install-button>
</body>
</html>
```

2. Create `manifest.json`:

```json
{
  "name": "Evil Portal v1.1",
  "version": "1.1.0",
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        { "path": "EvilPortal_v1.1_esp32s3.bin", "offset": 0 }
      ]
    }
  ]
}
```

3. Host both files along with your `.bin` on GitHub Pages or any web server.

### Full Manifest with Bootloader

For a complete flash including bootloader:

```json
{
  "name": "Evil Portal v1.1",
  "version": "1.1.0", 
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        { "path": "bootloader.bin", "offset": 0 },
        { "path": "partitions.bin", "offset": 32768 },
        { "path": "firmware.bin", "offset": 65536 }
      ]
    }
  ]
}
```

---

## Troubleshooting

### "espcomm_sync failed"
- Hold BOOT button while clicking Upload
- Try a different USB cable
- Check USB port permissions on Linux

### Compilation errors with ESP32 Core 3.x
- Downgrade to 2.0.17: **Tools > Board > Boards Manager > esp32 > Select version 2.0.17**

### "SPIFFS mount failed"
- Make sure partition scheme includes SPIFFS
- Try: **Tools > ESP32 Sketch Data Upload**

### Binary too large
- Use a larger partition scheme
- Remove debug output
- Optimize HTML templates

---

## Binary Naming Convention

For the `bin/` folder, use this naming:
```
EvilPortal_v{VERSION}_{BOARD}.bin

Examples:
- EvilPortal_v1.1.0_esp32s3.bin
- EvilPortal_v1.1.0_esp32.bin
- EvilPortal_v1.1.0_electroniccats.bin
```
