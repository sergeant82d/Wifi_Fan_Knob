# SPIFFS Configuration Schema

Device stores configuration in `/spiffs/config.json` on the ESP32.

---

## Complete config.json Example

```json
{
  "device": {
    "name": "WiFi Fan Knob",
    "chipId": "A1B2C3D4E5F6",
    "firmwareVersion": "v1.0.0",
    "buildDate": "2026-09-22"
  },
  
  "network": {
    "wifi": {
      "ssid": "MyHomeWiFi",
      "password": "secure_password_here",
      "saveCredentials": true
    },
    "webserver": {
      "port": 8080,
      "username": "",
      "password": ""
    },
    "mqtt": {
      "enabled": true,
      "broker": "192.168.1.50",
      "port": 1883,
      "username": "mqtt_user",
      "password": "mqtt_pass",
      "discoveryEnabled": true,
      "topicPrefix": "wifi_fan_knob"
    },
    "ntp": {
      "server": "pool.ntp.org",
      "syncInterval": 3600000
    }
  },
  
  "display": {
    "timezone": "CST",
    "timeFormat": "12h",
    "brightness": 80,
    "screenTimeout": 0
  },
  
  "fan": {
    "minRpm": 0,
    "maxRpm": 2500,
    "rpmStep": 100,
    "calibration": {
      "minPwm": 50,
      "maxPwm": 200,
      "enableFeedback": true
    },
    "presets": {
      "low": 800,
      "medium": 1200,
      "high": 1600,
      "max": 2000
    }
  },
  
  "system": {
    "deepSleepEnabled": true,
    "standbyTimeout": 0,
    "audioEnabled": true,
    "autoUpdate": false
  },
  
  "advanced": {
    "debugMode": false,
    "logLevel": "info"
  }
}
```

---

## Detailed Field Descriptions

### `device` (Read-Only, Auto-Generated)
Information about the device hardware. Updated on boot.

```json
"device": {
  "name": "WiFi Fan Knob",          // Device name
  "chipId": "A1B2C3D4E5F6",          // ESP32 chip ID (hex)
  "firmwareVersion": "v1.0.0",        // Current firmware version
  "buildDate": "2026-09-22"           // Build timestamp
}
```

---

### `network.wifi`
WiFi credentials for home network.

```json
"wifi": {
  "ssid": "MyHomeWiFi",               // Network SSID
  "password": "secure_password_here", // WiFi password (plain text in storage)
  "saveCredentials": true             // Save and auto-reconnect on boot
}
```

**Notes:**
- Passwords stored plain text in SPIFFS (ESP32 has encryption, but not absolute)
- If empty, device starts in AP mode (Access Point) for configuration
- SSID max 32 chars, password max 63 chars

---

### `network.webserver`
Web control panel settings.

```json
"webserver": {
  "port": 8080,                       // HTTP port (1-65535)
  "username": "",                     // Optional HTTP basic auth username
  "password": ""                      // Optional HTTP basic auth password
}
```

**Notes:**
- Port 80 requires elevated privileges; 8080 is safer default
- Leave username/password empty for no auth (open webserver)
- If auth enabled, user must login to access all pages

---

### `network.mqtt`
Home Assistant MQTT integration.

```json
"mqtt": {
  "enabled": true,                    // Enable MQTT publish/subscribe
  "broker": "192.168.1.50",           // MQTT broker IP or hostname
  "port": 1883,                       // MQTT port (default 1883)
  "username": "mqtt_user",            // MQTT broker username (empty if none)
  "password": "mqtt_pass",            // MQTT broker password (empty if none)
  "discoveryEnabled": true,           // Publish Home Assistant discovery payloads
  "topicPrefix": "wifi_fan_knob"      // MQTT topic prefix (no slashes)
}
```

**Notes:**
- If broker unreachable, device continues normally (no blocking)
- Discovery payloads sent only on boot and on config change
- Topic prefix becomes: `homeassistant/sensor/{topicPrefix}_rpm/config`, etc.

---

### `network.ntp`
NTP time synchronization settings.

```json
"ntp": {
  "server": "pool.ntp.org",           // NTP server address
  "syncInterval": 3600000             // Sync every 60 minutes (milliseconds)
}
```

**Notes:**
- syncInterval: 3600000 = 60 minutes (1000 = 1 second)
- Device syncs on boot + periodic interval while running
- If NTP fails, device continues with last-known time

---

### `display`
LCD display settings.

```json
"display": {
  "timezone": "CST",                  // Time zone (EST, CST, MST, PST, UTC)
  "timeFormat": "12h",                // "12h" or "24h"
  "brightness": 80,                   // LCD backlight brightness (0-100%)
  "screenTimeout": 0                  // Screen off timeout (0 = never, minutes)
}
```

**Notes:**
- brightness: 0 = off, 100 = full brightness (PWM controlled)
- screenTimeout: 0 = always on; 5 = 5 minutes; etc.
- Used by display.py in LVGL UI

---

### `fan`
Fan speed and control settings.

```json
"fan": {
  "minRpm": 0,                        // Minimum fan speed (0-2500)
  "maxRpm": 2500,                     // Maximum fan speed (0-2500)
  "rpmStep": 100,                     // RPM increment per knob click
  "calibration": {
    "minPwm": 50,                     // Minimum PWM value for fan to start (0-255)
    "maxPwm": 200,                    // Maximum PWM value (0-255)
    "enableFeedback": true            // Use tachometer feedback for PID loop
  },
  "presets": {
    "low": 800,                       // Quick-start preset buttons (Home screen)
    "medium": 1200,
    "high": 1600,
    "max": 2000
  }
}
```

**Notes:**
- Calibration minPwm/maxPwm: User adjusts per fan model (Noctua may differ from others)
- rpmStep: 100 RPM steps = knob click → 100 RPM change
- Presets: User-configurable quick buttons (webserver Config tab)
- Feedback PID: Tachometer adjusts PWM to maintain target RPM (lightweight loop)

---

### `system`
General system settings.

```json
"system": {
  "deepSleepEnabled": true,           // Allow deep sleep in standby mode
  "standbyTimeout": 0,                // Auto-standby timeout (0 = disabled, minutes)
  "audioEnabled": true,               // Speaker audio (warning sounds, wake chime)
  "autoUpdate": false                 // Check for OTA updates automatically
}
```

**Notes:**
- deepSleepEnabled: Reduces power consumption during standby
- standbyTimeout: 0 = manual only; 30 = auto standby after 30 min idle
- audioEnabled: Disable to silence Dragon eye wake sound + warnings
- autoUpdate: Future feature (manual OTA for now)

---

### `advanced`
Developer/debug settings.

```json
"advanced": {
  "debugMode": false,                 // Serial debug output + extra MQTT messages
  "logLevel": "info"                  // "error", "warn", "info", "debug"
}
```

**Notes:**
- debugMode: Verbosity on serial monitor (115200 baud)
- logLevel: Controls console output detail
- Leave as defaults for normal operation

---

## File Operations (C++ Pseudocode)

### Load Configuration

```cpp
#include <SPIFFS.h>
#include <ArduinoJson.h>

void loadConfig() {
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
    return;
  }
  
  File file = SPIFFS.open("/config.json", "r");
  if (!file) {
    Serial.println("Config not found, using defaults");
    setDefaultConfig();
    return;
  }
  
  StaticJsonDocument<1024> doc;
  deserializeJson(doc, file);
  
  // Copy JSON values to globals
  strlcpy(config.wifi.ssid, doc["network"]["wifi"]["ssid"], 32);
  strlcpy(config.wifi.password, doc["network"]["wifi"]["password"], 64);
  config.webserver.port = doc["network"]["webserver"]["port"] | 8080;
  config.fan.minRpm = doc["fan"]["calibration"]["minPwm"] | 50;
  // ... etc
  
  file.close();
  Serial.println("Config loaded successfully");
}
```

### Save Configuration

```cpp
void saveConfig() {
  StaticJsonDocument<1024> doc;
  
  // Populate JSON from globals
  doc["network"]["wifi"]["ssid"] = config.wifi.ssid;
  doc["network"]["wifi"]["password"] = config.wifi.password;
  doc["network"]["webserver"]["port"] = config.webserver.port;
  doc["fan"]["calibration"]["minPwm"] = config.fan.minPwm;
  // ... etc
  
  File file = SPIFFS.open("/config.json", "w");
  if (!file) {
    Serial.println("Failed to open config for writing");
    return;
  }
  
  serializeJson(doc, file);
  file.close();
  Serial.println("Config saved");
}
```

### Reset to Defaults

```cpp
void setDefaultConfig() {
  StaticJsonDocument<1024> doc;
  
  doc["network"]["wifi"]["ssid"] = "";
  doc["network"]["wifi"]["password"] = "";
  doc["network"]["webserver"]["port"] = 8080;
  doc["network"]["mqtt"]["broker"] = "192.168.1.50";
  doc["network"]["mqtt"]["port"] = 1883;
  doc["network"]["mqtt"]["discoveryEnabled"] = true;
  
  doc["display"]["timezone"] = "CST";
  doc["display"]["timeFormat"] = "12h";
  doc["display"]["brightness"] = 80;
  
  doc["fan"]["minRpm"] = 0;
  doc["fan"]["maxRpm"] = 2500;
  doc["fan"]["rpmStep"] = 100;
  doc["fan"]["calibration"]["minPwm"] = 50;
  doc["fan"]["calibration"]["maxPwm"] = 200;
  doc["fan"]["presets"]["low"] = 800;
  doc["fan"]["presets"]["medium"] = 1200;
  doc["fan"]["presets"]["high"] = 1600;
  doc["fan"]["presets"]["max"] = 2000;
  
  doc["system"]["deepSleepEnabled"] = true;
  doc["system"]["audioEnabled"] = true;
  
  // Save to SPIFFS
  File file = SPIFFS.open("/config.json", "w");
  serializeJson(doc, file);
  file.close();
}
```

---

## Size & Memory Notes

- **File size**: ~2-3 KB (well within SPIFFS limits)
- **JSON library**: Use `ArduinoJson` (added to platformio.ini dependencies)
- **Load time**: <100ms on boot
- **Frequent updates**: Don't save on every change (wear leveling); batch saves or add dirty flag

---

## Configuration Flow (WebServer Integration)

1. User opens webserver (8080 port)
2. All form fields pre-populate with current config.json values
3. User edits values in browser
4. User clicks "Save" → POST request to `/api/config`
5. C++ handler parses JSON POST body
6. Validates values (ranges, required fields)
7. Saves to SPIFFS via `saveConfig()`
8. Returns success/error response to browser
9. Browser shows confirmation, page refreshes with new values

---

## Backup Strategy

For future versions, consider:
- Periodic SPIFFS snapshot to cloud storage
- Config export as JSON file (user download via webserver)
- Config import from JSON file (restore backup)
