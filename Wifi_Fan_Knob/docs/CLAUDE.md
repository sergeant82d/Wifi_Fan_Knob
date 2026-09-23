# WiFi Fan Knob - Claude Code Transfer Guide

**Project**: ESP32-S3 PWM Fan Controller with Rotary Display + Home Assistant  
**Repository**: https://github.com/sergeant82d/Wifi_Fan_Knob.git  
**Local Path**: `D:\GitHub\VSCodeProjects\Wifi_Bench_Fan\Wifi_Fan_Knob`  
**Status**: ✅ Architecture complete, clean compile, awaiting hardware testing  
**Transfer Date**: 2026-09-23


1. Don’t assume. Don’t hide confusion. Surface tradeoffs.

2. Minimum code that solves the problem. Nothing speculative.

3. Touch only what you must. Clean up only your own mess.

4. Define success criteria. Loop until verified.


---

## Quick Setup in Claude Code

```bash
# 1. Clone repository
git clone https://github.com/sergeant82d/Wifi_Fan_Knob.git
cd Wifi_Fan_Knob

# 2. Verify structure
ls -la
# Expected: src/, include/, data/, docs/, platformio.ini, .gitignore

# 3. Open in VS Code with PlatformIO
code .

# 4. PlatformIO will auto-detect ESP32-S3 board
# Environment: esp32-s3-devkitc-1 (Elecrow board detected as this variant)

# 5. Build (Ctrl+Alt+B)
# Expected: Clean compile, no errors
```

---

## Project File Inventory

### Core Source Files (Ready to Integrate)

| File | Location | Status | Purpose |
|------|----------|--------|---------|
| `main.cpp` | `src/main.cpp` | ✅ Complete | Main firmware skeleton (boot, display init, encoder, WiFi, NTP) |
| `config.h` | `include/config.h` | ✅ Complete | Config struct + function declarations |
| `config.cpp` | `src/config.cpp` | ✅ Complete | SPIFFS JSON load/save, validation, setters |
| `lv_conf.h` | `include/lv_conf.h` | ✅ Minimal | LVGL configuration (240×240, 16-bit) |
| `index.html` | `data/index.html` | ✅ Complete | Webserver UI (4-tab responsive) |
| `webserver.h` | `include/webserver.h` | ⏳ Stub | Webserver routes/handlers (TODO) |
| `webserver.cpp` | `src/webserver.cpp` | ⏳ Stub | AsyncWebServer implementation (TODO) |
| `mqtt.h` | `include/mqtt.h` | ⏳ Stub | MQTT client wrapper (TODO) |
| `mqtt.cpp` | `src/mqtt.cpp` | ⏳ Stub | MQTT pub/sub + HA discovery (TODO) |
| `fan_control.h` | `include/fan_control.h` | ⏳ Stub | EMC2101 commands (TODO) |
| `fan_control.cpp` | `src/fan_control.cpp` | ⏳ Stub | Fan PWM, RPM feedback (TODO) |

### Documentation Files (in `/docs/`)

| File | Purpose |
|------|---------|
| `STATUS_REPORT.md` | Full end-of-day report (8 completed items, timeline, risks) |
| `PIN_MAPPING.md` | Complete ESP32-S3 pin reference (all 48 pins) |
| `MQTT_SCHEMA.md` | Home Assistant auto-discovery schema (7 entities) |
| `SPIFFS_CONFIG_SCHEMA.md` | JSON config structure + pseudocode |
| `CONFIG_INTEGRATION.md` | How to integrate config.cpp into main.cpp |
| `INTEGRATION_GUIDE.md` | Initial PlatformIO setup guide |

### Configuration & Build

| File | Purpose |
|------|---------|
| `platformio.ini` | PlatformIO project config (ESP32-S3, all lib deps) |
| `.gitignore` | Git ignore rules (PlatformIO standard) |
| `README.md` | Project overview at root |

---

## Project Structure

```
Wifi_Fan_Knob/
├── src/
│   ├── main.cpp               ✅ Complete skeleton
│   ├── config.cpp             ✅ SPIFFS JSON system
│   ├── webserver.cpp          ⏳ Stub (next priority)
│   ├── mqtt.cpp               ⏳ Stub
│   └── fan_control.cpp        ⏳ Stub
├── include/
│   ├── lv_conf.h              ✅ LVGL config
│   ├── config.h               ✅ Config struct + declarations
│   ├── webserver.h            ⏳ Stub
│   ├── mqtt.h                 ⏳ Stub
│   └── fan_control.h          ⏳ Stub
├── data/
│   └── index.html             ✅ Webserver UI (4-tab responsive)
├── docs/
│   ├── STATUS_REPORT.md       ✅ Comprehensive status
│   ├── PIN_MAPPING.md         ✅ Hardware reference
│   ├── MQTT_SCHEMA.md         ✅ HA auto-discovery spec
│   ├── SPIFFS_CONFIG_SCHEMA.md ✅ JSON config reference
│   ├── CONFIG_INTEGRATION.md  ✅ Config integration guide
│   └── INTEGRATION_GUIDE.md   ✅ Initial setup guide
├── platformio.ini             ✅ Build config
├── .gitignore                 ✅ Git rules
├── .git/                       (GitHub versioning)
└── README.md                  ✅ Project overview
```

---

## Hardware Reference

### Board: Elecrow 1.28" Rotary Display (ESP32-S3)

**Detected as**: `esp32-s3-devkitc-1` in PlatformIO

**Key Hardware**:
- **MCU**: ESP32-S3 (dual-core, 240 MHz, PSRAM)
- **Display**: 240×240 IPS round LCD (GC9A01 SPI driver)
- **Touch**: Capacitive touch (CST816D, separate I2C bus)
- **Encoder**: Rotary knob with push button
- **Audio**: Integrated speaker (unused in main.cpp, TODO)
- **RGB LED**: 5× WS2812 NeoPixel chain
- **Connectivity**: WiFi 802.11b/g/n, Bluetooth LE (unused in main.cpp, TODO)

### Critical Pins

| Function | GPIO | Notes |
|----------|------|-------|
| Display SPI SCLK | 10 | GC9A01 clock |
| Display SPI MOSI | 11 | GC9A01 data |
| Display DC | 3 | Data/Command select |
| Display CS | 9 | Chip select |
| Display RST | 14 | Reset |
| Display Backlight | 46 | PWM brightness |
| Touchscreen SDA | 6 | Separate I2C bus |
| Touchscreen SCL | 7 | Separate I2C bus |
| Touchscreen INT | 5 | Touch interrupt |
| Touchscreen RST | 13 | Touch reset |
| **Main I2C SDA** | **38** | **EMC2101 + optional OLED** |
| **Main I2C SCL** | **39** | **EMC2101 + optional OLED** |
| Encoder A | 45 | Gray code input |
| Encoder B | 42 | Gray code input |
| Encoder SW | 41 | Push button |
| **KEEP_ALIVE** | **2** | **Soft power latch (critical)** |
| Power Light | 40 | Status LED |
| RGB LED Data | 48 | WS2812 chain |

**See PIN_MAPPING.md for full 48-pin reference.**

---

## Build & Dependencies

### platformio.ini

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200

lib_deps =
    lvgl/LVGL@^8.4.0
    lovyan03/LovyanGFX@^1.1.0
    adafruit/Adafruit BusIO@^1.14.5
    arduino-libraries/NTPClient@^3.2.1
    bblanchic/ArduinoJson@^6.21.0

build_flags =
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
    -I include/

monitor_filters = esp32_exception_decoder
```

### Compile Status

✅ **CLEAN BUILD** — All dependencies resolve, no errors.

---

## Current Development Status

### ✅ Completed (Session 1-2)

- PlatformIO + GitHub setup
- All 48 ESP32 pins mapped + documented
- main.cpp skeleton (display, encoder, I2C, WiFi, NTP, state machine)
- LVGL display integration (240×240 16-bit)
- Webserver UI design (4-tab HTML/CSS)
- MQTT Home Assistant auto-discovery schema (7 entities)
- SPIFFS JSON configuration system (70+ settings)
- Full config.cpp implementation (load/save/validate/defaults)
- Documentation complete (6 guides + status report)

### ⏳ Stubbed (Ready for Implementation)

- `webserver.cpp` — AsyncWebServer routes, form handlers
- `mqtt.cpp` — MQTT broker connection, auto-discovery payloads, pub/sub
- `fan_control.cpp` — EMC2101 PWM commands, tachometer reading
- `ui.cpp` — LVGL screens (Dragon eye standby, main display, menus)

### ⏸ Awaiting Hardware

- Boot sequence verification
- Display rendering (LVGL basic test)
- I2C EMC2101 detection
- Encoder rotation input
- WiFi AP mode startup
- NTP time sync

---

## Architecture Overview

### System State Machine

```
SHUTDOWN (0)
  └─ User presses physical button
     └─ ACTIVE

ACTIVE (2)
  ├─ Knob rotation → Fan speed adjustment
  ├─ Long press → Standby menu
  └─ Screen shows: RPM, clock, arc gauge, quick presets

STANDBY (1)
  ├─ Dragon eye animation (closed, twitching)
  ├─ MCU in deep sleep
  ├─ Display minimal power
  └─ Touch screen → Wake to ACTIVE
```

### Boot Sequence

1. GPIO init (encoder, backlight, power light)
2. Power latch engaged (KEEP_ALIVE HIGH)
3. Display init (LGFX + LVGL)
4. I2C init (dual buses: touch @ 6/7, EMC2101 @ 38/39)
5. Config load from SPIFFS (or defaults)
6. WiFi connect (saved SSID or AP mode)
7. NTP sync (set system time)
8. MQTT connect (if enabled)
9. Encoder ISR attached
10. Main loop starts

---

## Key Design Decisions

| Decision | Choice | Why |
|----------|--------|-----|
| WiFi | Native ESP32 (no WiFiManager) | Direct UI control in AP mode |
| Config Storage | SPIFFS + JSON | Flexible, debuggable, scalable |
| Webserver | AsyncWebServer port 8080 | Non-blocking, safe port |
| UI Layout | 4-tab tabbed (Home/WiFi/Config/OTA) | Organized, mobile-responsive |
| HA Integration | MQTT Auto-Discovery | Zero-config user experience |
| Fan Speed | 100 RPM steps, 0-2500 RPM range | Matches Noctua fan capabilities |
| Power Latch | Soft GPIO 2 (P-MOSFET) | Safe shutdown, no power drain |
| Time Sync | NTP every 60 min | Accurate, minimal overhead |
| OTA | Web form upload | Simple, local, no cloud dependency |

---

## Next Steps (In Priority Order)

### Immediate (Hardware Arrival)
1. Flash main.cpp to board
2. Verify serial boot output (115200 baud)
3. Test EMC2101 I2C detection
4. Verify encoder input (rotation + button)
5. Display rendering (simple test pattern)
6. WiFi AP mode startup

### Week 1 (Core Functionality)
1. Integrate config.cpp into main.cpp
2. Implement webserver.cpp (AsyncWebServer)
3. Test webserver UI in browser (http://192.168.10.x:8080)
4. Implement mqtt.cpp (broker connection, auto-discovery)
5. Verify Home Assistant integration

### Week 2 (Fan Control & UI)
1. Implement fan_control.cpp (EMC2101 PWM)
2. Test encoder → RPM adjustment
3. Implement Dragon eye standby animation (LVGL)
4. Main display screen (RPM gauge, time, presets)
5. Menu navigation (touch swipe between screens)

### Week 3 (Polish & Testing)
1. Calibration UI (min/max PWM for fan)
2. Audio system (warning sounds, wake chime)
3. Field testing (actual fume extraction)
4. OTA update verification
5. Home Assistant advanced automations

---

## Known Issues & Workarounds

| Issue | Status | Workaround |
|-------|--------|-----------|
| WiFiManager incompatible with ESP32-S3 | ✅ Resolved | Using native WiFi stack |
| LVGL config required | ✅ Resolved | Minimal lv_conf.h in include/ |
| LovyanGFX auto-detect needed | ✅ Resolved | Using LGFX_AUTODETECT.hpp |
| ArduinoJson dependency | ✅ Resolved | Added to platformio.ini |
| EMC2101 library dependency | ✅ Resolved | Added Adafruit BusIO dependency |

---

## GitHub Workflow

```bash
# Clone & setup
git clone https://github.com/sergeant82d/Wifi_Fan_Knob.git
cd Wifi_Fan_Knob

# Make changes in VS Code
# ... edit src/main.cpp, etc. ...

# Commit & push
git add .
git commit -m "Implement webserver module"
git push origin main

# Pull latest
git pull origin main
```

**All work is versioned on GitHub** — safe to continue from any machine.

---

## Resources & References

### Documentation
- **PIN_MAPPING.md** — All hardware pins (detailed)
- **MQTT_SCHEMA.md** — Home Assistant integration (7 sensors)
- **SPIFFS_CONFIG_SCHEMA.md** — JSON config structure (70+ fields)
- **CONFIG_INTEGRATION.md** — How to use config system
- **STATUS_REPORT.md** — Full project status report

### External Links
- **Elecrow GitHub**: https://github.com/Elecrow-RD/CrowPanel-1.28inch-HMI-ESP32-Rotary-Display-240-240-IPS-Round-Touch-Knob-Screen.git
- **Adafruit EMC2101**: https://www.adafruit.com/product/4808
- **Noctua Fan**: https://www.amazon.com/clp/B0G3TVV5H7

---

## Questions for Next Session

**Still to finalize after hardware testing:**
1. Fan speed units in Home Assistant: RPM (0-2500) or % (0-100)?
2. Audio format for warnings: WAV, MP3, or other?
3. Dragon eye animation: Pre-rendered frames or procedural LVGL drawing?
4. Expected Noctua fan min/max PWM: Initial guess 50-200 (adjust after calibration)
5. ROM/RAM budget: Display gets priority, how aggressive with fan control?

---

## Quick Reference

**Default Ports & Credentials**:
- Webserver: `http://192.168.10.x:8080` (no auth by default)
- MQTT Broker: `192.168.10.50:1883` (configurable in webserver)
- WiFi AP (fallback): `WiFi-Fan-Knob-XXXXXX` / `12345678`
- Serial Monitor: `115200 baud`

**Configuration File**:
- Path: `/config.json` (SPIFFS)
- Size: ~2-3 KB
- Format: JSON
- Auto-created on boot if missing

**Important GPIO**:
- **KEEP_ALIVE (GPIO 2)**: Must go HIGH on boot or system power-cycles immediately
- **Encoder (45/42/41)**: ISR active; any rotation triggers `encoder_count++` / `encoder_count--`
- **Display Backlight (46)**: PWM control (0-255) adjusts brightness

---

## Handoff Summary

This project is **architecture-complete** and **ready for hardware testing**. All core systems are designed, documented, and compiling cleanly. The codebase is modular and Git-versioned for easy collaboration.

**What's needed**: Hardware board arrival + integration testing.

**What's provided**: Complete documentation, design decisions, code skeleton, build config, and roadmap.

**Next developer**: Clone repo, build, flash, and start with hardware verification (see "Next Steps" above).

---

**Generated**: 2026-09-23  
**Status**: 🟢 Ready for Claude Code Transfer  
**Questions?**: See docs/ folder for detailed information.


