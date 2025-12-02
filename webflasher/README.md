# 🌐 Web Flasher

Flash Evil Portal directly from your browser - no Arduino IDE needed!

## 🚀 Live Web Flasher

**Coming soon!** Once binaries are generated, the web flasher will be available at:

```
https://YOUR_USERNAME.github.io/esp32-captive-portal/webflasher/
```

## 📦 How It Works

This web flasher uses [ESP Web Tools](https://esphome.github.io/esp-web-tools/) to flash firmware directly from the browser using Web Serial API.

**Requirements:**
- Chrome or Edge browser (desktop)
- USB data cable
- ESP32-S3 board

## 🔧 Generating Binary Files

To update the firmware binaries, follow these steps:

### Step 1: Compile in Arduino IDE

1. Open `src/main.cpp` in Arduino IDE
2. Configure settings:
   - Board: `ESP32S3 Dev Module`
   - Partition Scheme: `Default 4MB with spiffs`
   - USB CDC On Boot: `Enabled`
3. Click **Sketch → Export Compiled Binary**

### Step 2: Find the Binary Files

After export, Arduino creates files in a `build` folder. You need these files:

| File | Description |
|------|-------------|
| `*.ino.bootloader.bin` | Bootloader |
| `*.ino.partitions.bin` | Partition table |
| `boot_app0.bin` | Boot application |
| `*.ino.bin` | Main firmware |

The `boot_app0.bin` is located at:
```
~/.arduino15/packages/esp32/hardware/esp32/2.0.x/tools/partitions/boot_app0.bin
```

### Step 3: Copy Files to webflasher/

Rename and copy the files:

```bash
cp build/esp32.esp32.esp32s3/*.bootloader.bin webflasher/bootloader.bin
cp build/esp32.esp32.esp32s3/*.partitions.bin webflasher/partitions.bin
cp ~/.arduino15/packages/esp32/hardware/esp32/2.0.*/tools/partitions/boot_app0.bin webflasher/
cp build/esp32.esp32.esp32s3/*.ino.bin webflasher/firmware.bin
```

### Step 4: Update manifest.json

Verify the `manifest.json` has correct paths:

```json
{
  "name": "Evil Portal Standalone",
  "version": "1.2.0",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "improv": false,
      "parts": [
        { "path": "bootloader.bin", "offset": 0 },
        { "path": "partitions.bin", "offset": 32768 },
        { "path": "boot_app0.bin", "offset": 57344 },
        { "path": "firmware.bin", "offset": 65536 }
      ]
    }
  ]
}
```

## 🌍 Enabling GitHub Pages

To host the web flasher on GitHub Pages:

1. Go to your repo → **Settings** → **Pages**
2. Source: **Deploy from a branch**
3. Branch: `main` (or `master`)
4. Folder: `/ (root)`
5. Click **Save**

After a few minutes, your flasher will be live at:
```
https://YOUR_USERNAME.github.io/REPO_NAME/webflasher/
```

## 📁 File Structure

```
webflasher/
├── index.html      # Web flasher page
├── manifest.json   # ESP Web Tools manifest
├── bootloader.bin  # ESP32-S3 bootloader
├── partitions.bin  # Partition table
├── boot_app0.bin   # Boot application
├── firmware.bin    # Main firmware
└── README.md       # This file
```

## ⚠️ Notes

- Web Serial only works on **HTTPS** (GitHub Pages provides this)
- Only **Chrome** and **Edge** support Web Serial
- iOS/Safari not supported
- Mobile browsers not supported

## 🔗 Alternative: Manual Flashing

If web flashing doesn't work, you can use:

1. **Spacehuhn ESP Tool**: https://esptool.spacehuhn.com/
2. **ESPHome Web**: https://web.esphome.io/
3. **esptool.py** command line:
   ```bash
   esptool.py --chip esp32s3 --port /dev/ttyUSB0 write_flash \
     0x0 bootloader.bin \
     0x8000 partitions.bin \
     0xe000 boot_app0.bin \
     0x10000 firmware.bin
   ```

## 📚 Resources

- [ESP Web Tools Documentation](https://esphome.github.io/esp-web-tools/)
- [Web Serial API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API)
- [ESP32-S3 Flash Offsets](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/bootloader.html)
