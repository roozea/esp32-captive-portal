# Known Issues & Workarounds

## 1. ESP32 Arduino Core Version

### ✅ Now Compatible with Core 3.x!

As of version 1.2.0, the firmware is fully compatible with ESP32 Arduino Core 3.x (tested with 3.3.4).

**Recommended Setup:**
- ESP32 Core: 3.3.4 or newer
- AsyncTCP: mathieucarbou fork (required for Core 3.x)
- ESPAsyncWebServer: mathieucarbou fork

### Library Installation

```bash
cd ~/Documents/Arduino/libraries
rm -rf AsyncTCP ESPAsyncWebServer  # Remove old versions
git clone https://github.com/mathieucarbou/AsyncTCP
git clone https://github.com/mathieucarbou/ESPAsyncWebServer
```

### Still using Core 2.x?

It still works! Both Core 2.x and 3.x are supported.

---

## 2. USB-CDC Breaks After WiFi.softAPConfig() (ESP32-S3)

### Problem

On ESP32-S3 boards using native USB-CDC, calling `WiFi.softAPConfig()` with a custom IP address (like 4.3.2.1) can break serial communication.

### Symptoms

- Serial output stops after WiFi initialization
- The WiFi AP works correctly
- Need to reflash to recover

### Affected Hardware

- ESP32-S3 boards with native USB (no external UART chip)
- Electronic Cats WiFi Dev Board
- ESP32-S3-DevKitC boards
- Any ESP32-S3 using "USB CDC On Boot: Enabled"

### Status

We reported this to Espressif. It appears to be **fixed in Core 3.3.4**.

If you still experience issues:
1. Update to ESP32 Core 3.3.4 or newer
2. Use an external UART adapter for debugging

### Workaround for Flipper Edition

The Flipper Edition uses GPIO 43/44 for UART communication with Flipper, which is unaffected by this bug. Debug output goes to USB, Flipper communication goes to hardware UART.

---

## 3. Samsung Captive Portal Detection

### Background

Samsung phones have stricter captive portal detection than other devices. They specifically check if the gateway IP appears to be a "public" IP address.

### Solution

We use IP address `4.3.2.1` which:
- Triggers Samsung's captive portal detection
- Is unassigned/reserved (safe for local use)
- Works with all other devices too

### Verification

After flashing, check Serial output:
```
[+] IP:   4.3.2.1
```

If it shows `192.168.4.1`, the configuration failed.

---

## 4. Flipper Zero Communication Issues

### Problem: Flipper sends "reset" command

The Flipper expects specific responses in exact format:
- `ap set` (not "AP Set" or "ap_set")
- `html set` (not "HTML Set")
- `all set` (not "All Set")

If responses don't match exactly, Flipper sends "reset" after ~3 seconds.

### Solution

The Flipper Edition firmware uses:
1. Exact response strings
2. `flush()` after each response
3. Small delays for timing

### Verification

Watch Serial Monitor for:
```
[→FLIP] ap set
[→FLIP] html set
[→FLIP] all set
```

If you see these but Flipper still resets, increase the delay in `sendToFlipper()`:
```cpp
void sendToFlipper(const char* message) {
    FlipperSerial.println(message);
    FlipperSerial.flush();
    delay(20);  // Try 20ms instead of 10ms
}
```

---

## 5. SPIFFS Mounting Failures

### Symptoms

- "SPIFFS not available" in Serial output
- Credentials don't persist after reboot
- Config resets to defaults

### Solution

1. **Check Partition Scheme:**
   - Tools > Partition Scheme > "Default 4MB with spiffs"

2. **Format SPIFFS:**
   The firmware auto-formats on first boot, but you can force it:
   - Tools > ESP32 Sketch Data Upload (if available)
   - Or access `/factory-reset` endpoint

3. **Check Flash Size:**
   - Ensure your board actually has 4MB flash
   - Some cheap boards have only 2MB

---

## 6. Admin Panel Login Issues

### Can't Login After Password Change

Browser caches Basic Auth credentials. Solutions:

1. **Clear browser data:**
   - Close ALL tabs to the portal
   - Clear site data for 4.3.2.1
   - Open new incognito/private window

2. **Use Factory Reset:**
   ```
   http://4.3.2.1/factory-reset
   ```
   Resets to admin/admin without needing login.

### Double Login Prompt

Fixed in v1.2.0. If you experience this:
1. Clear browser cache
2. Use incognito mode
3. Try a different browser

---

## 7. Multiple ESP32 Board Packages

### Problem

Having both "Arduino ESP32 Boards" and "esp32 by Espressif Systems" causes compilation errors.

### Solution

Keep only ONE package installed:

1. Tools > Board > Boards Manager
2. Search "esp32"
3. Uninstall "Arduino ESP32 Boards" if present
4. Keep only "esp32 by Espressif Systems"

---

## 8. LED Not Working

### Electronic Cats Board

LEDs are active LOW on this board. The firmware handles this:
```cpp
digitalWrite(LED_PIN, LOW);  // LED ON
digitalWrite(LED_PIN, HIGH); // LED OFF
```

### Custom Board

Update the pin definitions at the top of the sketch:
```cpp
#define LED_ACCENT_PIN 4   // Your blue LED pin (-1 to disable)
#define LED_STATUS_PIN 5   // Your green LED pin
#define LED_ALERT_PIN  6   // Your red LED pin
```

If your LEDs are active HIGH, modify `setLED()`:
```cpp
void setLED(bool accent, bool status, bool alert) {
    if (LED_ACCENT_PIN >= 0) digitalWrite(LED_ACCENT_PIN, accent ? HIGH : LOW);
    // ... etc
}
```

---

## 9. Captive Portal Not Appearing

### Checklist

1. **Correct SSID?** Check Serial output for actual network name
2. **Device connected?** Make sure device is connected to the portal WiFi
3. **IP correct?** Should be 4.3.2.1
4. **DNS working?** Try accessing any URL like http://test.com
5. **Browser cache?** Try different browser or incognito mode

### Manual Access

If auto-popup doesn't work, manually browse to:
- http://4.3.2.1
- http://captive.apple.com (iOS)
- http://connectivitycheck.gstatic.com (Android)

---

## 10. Credentials Not Saving

### Check

1. SPIFFS available? (See issue #5)
2. Disk space? Check Serial output for SPIFFS usage
3. Max credentials reached? (100 limit, FIFO after that)

### Force Save

Credentials are auto-saved. If issues persist:
1. Access `/factory-reset`
2. Reconfigure settings
3. Test capture again

---

## Reporting New Issues

Found something not listed here? Open an issue with:

1. **Board model** (exact name/link)
2. **ESP32 Core version** (from Boards Manager)
3. **Arduino IDE version**
4. **Firmware version** (Standalone or Flipper, version number)
5. **Serial output** (copy the relevant lines)
6. **Steps to reproduce**

The more details, the faster we can help!

---

## Quick Reference

| Problem | Quick Fix |
|---------|-----------|
| Can't login | `http://4.3.2.1/factory-reset` |
| SPIFFS error | Check partition scheme |
| Samsung no popup | Verify IP is 4.3.2.1 |
| Flipper reset | Check response timing |
| Serial stops | Update to Core 3.3.4 |
| LED not working | Check pin definitions |
