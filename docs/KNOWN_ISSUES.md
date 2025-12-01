# Known Issues & Workarounds

## Version 1.1 Specific Issues

### SPIFFS Mount Failure on First Boot

**Problem:** First boot after flashing may show `[!] SPIFFS mount failed`.

**Solution:** This is normal on first flash. The firmware automatically formats SPIFFS on first boot (`SPIFFS.begin(true)`). If the problem persists:

1. In Arduino IDE, use: Tools > ESP32 Sketch Data Upload
2. Or manually format via serial: The firmware handles this automatically

### Admin Panel Shows "No credentials captured" After Reboot

**Problem:** Previously captured credentials don't appear after device restart.

**Possible causes:**
1. SPIFFS wasn't properly initialized
2. Flash corruption
3. Wrong partition scheme selected

**Solution:**
1. Verify partition scheme has SPIFFS: Tools > Partition Scheme > "Default 4MB with spiffs"
2. Check serial output for SPIFFS errors on boot
3. Try reflashing with "Erase All Flash Before Sketch Upload" enabled

### API Returns 401 Unauthorized

**Problem:** API calls return authentication errors even with correct credentials.

**Solution:**
1. Use HTTP Basic Auth header, not form data
2. Example: `curl -u admin:admin http://4.3.2.1/api/v1/status`
3. In browser, you'll get a login popup - enter credentials there
4. Check if you changed the admin credentials and forgot them

**Recovery if locked out:** Reflash the firmware to reset to defaults.

### JSON Export Contains Escaped Characters

**Problem:** Exported JSON shows `\n` or `\"` in fields.

**Explanation:** This is correct behavior. Special characters are escaped per JSON specification. Any JSON parser will handle this correctly.

### Memory Issues with Many Credentials

**Problem:** Device becomes slow or crashes with many stored credentials.

**Solution:** The firmware limits storage to 100 credentials (FIFO). If you're experiencing issues:
1. Export and clear logs regularly
2. Check free memory in dashboard
3. Reboot device periodically during long engagements

---

## General Issues (All Versions)

### 1. ESP32 Arduino Core Version Compatibility

**Problem:** ESP32 Arduino Core version 3.x introduced breaking changes.

**Error Example:**
```
error: 'struct ip_event_ap_staipassigned_t' has no member named 'ip'
```

**Solution:** Use ESP32 Arduino Core version **2.0.x** (specifically 2.0.17 is well-tested).

In Arduino IDE:
1. Tools > Board > Boards Manager
2. Find "esp32 by Espressif Systems"
3. Select version 2.0.17
4. Click Install

---

### 2. USB-CDC Breaks After WiFi.softAPConfig()

**Problem:** On ESP32-S3 with native USB-CDC, calling `WiFi.softAPConfig()` with any custom IP causes serial to die.

**Symptoms:**
- Serial output stops after WiFi configuration
- WiFi AP works correctly
- LED indicators work correctly
- Device functions normally except for serial

**Affected Hardware:**
- ESP32-S3 boards with native USB
- Electronic Cats WiFi Dev Board
- ESP32-S3-DevKitC boards

**Why We Accept This:** The IP 4.3.2.1 is necessary for Samsung auto-popup. Since credentials are now stored in SPIFFS and viewable via admin panel, serial output isn't critical for operation.

**Workarounds:**
1. **Recommended:** Use admin panel at `http://4.3.2.1/admin` to view credentials
2. **Alternative:** Use external UART adapter on GPIO pins
3. **Alternative:** Use default IP 192.168.4.1 (breaks Samsung auto-popup)

---

### 3. Samsung Captive Portal Detection

**Problem:** Samsung phones require "public-looking" IP addresses for automatic popup.

**Solution:** Use IP `4.3.2.1` (already configured in v1.1).

This IP:
- Triggers Samsung's captive portal detection
- Works with all other devices too
- Is not routable on the internet (safe to use locally)

---

### 4. Multiple ESP32 Packages Installed

**Problem:** Having both "Arduino ESP32 Boards" and "esp32 by Espressif Systems" causes conflicts.

**Solution:** Only keep "esp32 by Espressif Systems" installed.

---

### 5. AsyncTCP Library Conflicts

**Problem:** Multiple versions of AsyncTCP exist, some incompatible.

**Solution:** Use the mathieucarbou fork:

```bash
cd ~/Documents/Arduino/libraries
rm -rf AsyncTCP ESPAsyncWebServer
git clone https://github.com/mathieucarbou/AsyncTCP
git clone https://github.com/mathieucarbou/ESPAsyncWebServer
```

---

### 6. ArduinoJson Version Issues (v1.1+)

**Problem:** Very old versions of ArduinoJson use different API.

**Solution:** Use ArduinoJson v7.x (latest). Install via Library Manager.

The v1.1 firmware uses `JsonDocument` which requires ArduinoJson 7.x. If you have 6.x, the code will still work but you may see deprecation warnings.

---

## Reporting New Issues

Found something not listed here? Please open an issue with:

1. Firmware version (check serial output or `/api/v1/status`)
2. Your board model
3. ESP32 Core version
4. ArduinoJson version
5. Arduino IDE version
6. The error message or unexpected behavior
7. Steps to reproduce

The more details, the faster we can help!

---

## FAQ

**Q: Can I use this without the Electronic Cats board?**
A: Yes! Any ESP32-S3 works. Just adjust the LED pin definitions or set them to -1.

**Q: Why not use ESP32 (non-S3)?**
A: You can! The USB-CDC issue doesn't affect original ESP32 since it uses external UART. However, the S3 has more RAM which helps with the web server.

**Q: Can I increase the 100 credential limit?**
A: Yes, change `MAX_CREDENTIALS` in the code. Be mindful of SPIFFS space - each credential uses ~200-300 bytes.

**Q: How do I completely reset the device?**
A: Flash with "Erase All Flash Before Sketch Upload" enabled, or use esptool: `esptool.py erase_flash`

**Q: The timestamps show 1970 dates. Why?**
A: Without internet access, the ESP32 has no way to know the real time. Timestamps are relative to boot time. Consider the sequence/ID for ordering.
