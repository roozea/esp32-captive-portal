# Known Issues & Workarounds

## 1. ESP32 Arduino Core Version Compatibility

### Problem

ESP32 Arduino Core version 3.x introduced breaking changes that cause compilation errors with AsyncTCP and ESPAsyncWebServer libraries.

### Error Example

```
error: 'struct ip_event_ap_staipassigned_t' has no member named 'ip'
```

### Solution

Use ESP32 Arduino Core version **2.0.x** (specifically 2.0.17 is well-tested).

In Arduino IDE:
1. Tools > Board > Boards Manager
2. Find "esp32 by Espressif Systems"
3. Select version 2.0.17
4. Click Install

### Why This Happens

Espressif changed some internal structures in version 3.x. The async libraries haven't been fully updated to match.

---

## 2. USB-CDC Breaks After WiFi.softAPConfig()

### Problem

On ESP32-S3 boards using native USB-CDC for serial communication (like the Electronic Cats WiFi Dev Board), calling `WiFi.softAPConfig()` with any IP address other than the default `192.168.4.1` causes the serial connection to die.

### Symptoms

- Serial output stops immediately after `WiFi.softAPConfig()`
- The WiFi AP starts and works correctly
- Serial never recovers, even after WiFi is working
- Unplugging and re-plugging USB doesn't help (need to reflash or reset)

### Affected Hardware

- ESP32-S3 boards with native USB (no external UART chip)
- Electronic Cats WiFi Dev Board
- Most ESP32-S3-DevKitC boards
- Any ESP32-S3 using "USB CDC On Boot: Enabled"

### Not Affected

- ESP32 (original, non-S3) - uses external UART chip
- ESP32-S2 - different USB implementation
- ESP32-S3 with external USB-UART adapter (CP2102, CH340, etc.)

### Technical Explanation

The ESP32-S3's native USB-CDC shares some internal resources with the TCP/IP stack. When `WiFi.softAPConfig()` reconfigures the network stack for a custom IP, it appears to disrupt the USB-CDC driver.

This is likely due to:
- Shared DMA buffers between USB and WiFi subsystems
- Internal resource conflicts during TCP/IP stack reconfiguration
- A bug in the ESP-IDF USB-CDC implementation for S3

We've reported this issue to Espressif: [Link to issue when created]

### Workarounds

**Option 1: Don't need serial during operation (Recommended for standalone use)**

The firmware works perfectly - you just can't see serial output after boot. Credentials are still captured, LEDs still work, everything functions except serial monitoring.

For pentesting scenarios, this is usually fine since you're not watching serial output in the field anyway.

**Option 2: Use default IP (Sacrifices Samsung auto-popup)**

```cpp
// Don't call WiFi.softAPConfig() at all
WiFi.softAP("FakeNetwork", "");  // Uses 192.168.4.1 automatically
```

This keeps serial working but Samsung phones won't auto-popup the captive portal (users have to manually tap "Sign in to network").

**Option 3: External UART adapter**

Use a USB-UART adapter (CP2102, CH340, FTDI) connected to GPIO pins instead of native USB:

```cpp
// Use Serial1 on custom pins
HardwareSerial FlipperSerial(1);

void setup() {
    FlipperSerial.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
    // ... rest of setup
}
```

Connect the UART adapter to available GPIO pins. This completely bypasses the USB-CDC issue.

**Option 4: Two-stage boot (Advanced)**

1. Boot with default IP, establish serial communication
2. Send a command to switch to custom IP
3. Accept that serial will die after the switch

This is useful if you need to configure the portal before deployment.

### Tested IP Addresses

| IP Address | Serial Works? | Samsung Auto-Popup? |
|------------|---------------|---------------------|
| 192.168.4.1 (default) | ✅ Yes | ❌ No |
| 192.168.1.1 | ❌ No | ❌ No |
| 10.0.0.1 | ❌ No | ❌ No |
| 4.3.2.1 | ❌ No | ✅ Yes |
| 8.8.8.8 | ❌ No | ✅ Yes |
| 1.1.1.1 | ❌ No | ✅ Yes |

Any custom IP breaks serial, regardless of whether it's public or private.

---

## 3. Samsung Captive Portal Detection

### Problem

Samsung phones have stricter captive portal detection than other devices. They specifically check if the gateway IP is a "public" IP address. Private IPs (192.168.x.x, 10.x.x.x, 172.16.x.x) don't trigger the automatic popup.

### Solution

Use IP address `4.3.2.1` - it's a Class A address that's not actually assigned to anyone on the internet, so it's safe to use locally while still triggering Samsung's detection.

```cpp
IPAddress apIP(4, 3, 2, 1);
WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
```

### Conflict with USB-CDC

This solution conflicts with Issue #2 above. See that section for workarounds.

### Alternative: Manual Connection

If you use the default IP, Samsung users can still access the portal - they just need to:
1. Connect to the WiFi
2. Wait for "Sign in to network" notification
3. Tap the notification

It's not automatic, but it works.

---

## 4. Multiple ESP32 Packages Installed

### Problem

If you have both "Arduino ESP32 Boards by Arduino" and "esp32 by Espressif Systems" installed, you'll get weird compilation errors and unpredictable behavior.

### Solution

Only keep **one** ESP32 package installed. We recommend "esp32 by Espressif Systems" as it's the official one from Espressif.

To check:
1. Tools > Board > Boards Manager
2. Search "esp32"
3. If you see two packages installed, uninstall "Arduino ESP32 Boards"

---

## 5. AsyncTCP Library Conflicts

### Problem

There are multiple versions of AsyncTCP floating around. Some don't work with newer ESP32 cores.

### Solution

Use the mathieucarbou fork, which is actively maintained:

```bash
cd ~/Documents/Arduino/libraries
rm -rf AsyncTCP  # Remove old version if present
git clone https://github.com/mathieucarbou/AsyncTCP
git clone https://github.com/mathieucarbou/ESPAsyncWebServer
```

---

## Reporting New Issues

Found something not listed here? Please open an issue with:

1. Your board model
2. ESP32 Core version
3. Arduino IDE version
4. The error message or unexpected behavior
5. Minimal code to reproduce the problem

The more details, the faster we can help!
