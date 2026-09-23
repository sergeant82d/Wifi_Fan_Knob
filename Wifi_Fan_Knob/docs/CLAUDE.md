# WiFi Fan Knob - Claude Code Guide

**Project**: ESP32-S3 PWM Fan Controller with Rotary Display + Home Assistant  
**Repository**: https://github.com/sergeant82d/Wifi_Fan_Knob.git  
**Repo root**: `D:\GitHub\VSCodeProjects\Wifi_Bench_Fan\Wifi_Fan_Knob`  
**PlatformIO project**: `<repo root>\Wifi_Fan_Knob` (nested one level down)  
**Status**: Hardware bring-up in progress — display, WiFi AP, SPIFFS and webserver verified on the board  
**Last updated**: 2026-09-23


1. Don’t assume. Don’t hide confusion. Surface tradeoffs.

2. Minimum code that solves the problem. Nothing speculative.

3. Touch only what you must. Clean up only your own mess.

4. Define success criteria. Loop until verified.


---

## Quick Setup

```bash
git clone https://github.com/sergeant82d/Wifi_Fan_Knob.git
cd Wifi_Fan_Knob/Wifi_Fan_Knob     # PlatformIO project is nested
```

- Open `Wifi_Fan_Knob.code-workspace` (repo root) in VS Code with PlatformIO.
- Board environment: `esp32-s3-devkitc-1`, overridden in `platformio.ini` for this board:
  16 MB QIO flash, OPI PSRAM (`qio_opi`), `default_16MB.csv` partitions (2 OTA slots).
- Serial is native USB (COM port VID 303A:1001); needs `-DARDUINO_USB_CDC_ON_BOOT=1`,
  otherwise `Serial` goes to unconnected UART0 and only IDF logs reach USB.
- Platform is pioarduino `espressif32` 51.x → Arduino core 3.0.4 / ESP-IDF 5.1.
- Build: PlatformIO **Build**. Flash: **Upload** only — the web UI (`web/index.html`) is
  compiled into the firmware via `board_build.embed_txtfiles`. SPIFFS holds only
  `/config.json`. Do not run `uploadfs`: it rewrites the whole SPIFFS partition, wiping
  `/config.json` (defaults are recreated on next boot).
- CLI builds: use `~/.platformio/penv/Scripts/pio.exe`. An older PlatformIO in
  `C:\Python312\Scripts` (6.1.19) fails with `SCons.Tool.FortranCommon` errors.

---

## Project File Inventory

### Source

| File | Status | Purpose |
|------|--------|---------|
| `src/main.cpp` | ✅ Working | Boot, display (LGFX + LVGL), encoder, WiFi, NTP, state machine skeleton |
| `include/config.h` / `src/config.cpp` | ✅ Working | SPIFFS JSON config load/save/validate/defaults |
| `include/lv_conf.h` | ✅ Minimal | LVGL 8 config (240×240, 16-bit) |
| `include/webserver.h` / `src/webserver.cpp` | 🟡 Partial | `/` → embedded index.html; `GET /api/status` (live IP/SSID/RSSI/MAC); `/api/wifi` save/forget/scan (scan async: 202→200); `GET/POST /api/config`, `POST /api/config/reset` |
| `web/index.html` | 🟡 Partial | 4-tab web UI; WiFi + Config tabs wired; Home + OTA still mock `alert()`s |
| `include/mqtt.h` / `src/mqtt.cpp` | ⬜ Empty | MQTT + HA discovery (TODO; needs an MQTT library in `lib_deps`) |
| `include/fan_control.h` / `src/fan_control.cpp` | ⬜ Empty | EMC2101 PWM + tach (TODO) |
| `lib/Adafruit_EMC2101/` | Vendored | Adafruit EMC2101 driver (local copy, not from registry) |
| `include/ui.h` / `src/ui.cpp` | ✅ Working | LVGL main screen: RPM arc, target RPM, clock, status box |

### Documentation (`docs/`)

| File | Purpose |
|------|---------|
| `STATUS_REPORT_01.md` | End-of-day report from the pre-hardware sessions |
| `MQTT_SCHEMA.md` | Home Assistant auto-discovery schema (7 entities) |
| `SPIFFS_CONFIG_SCHEMA.md` | JSON config structure |
| `CrowPanel1.28inchRotary-11.jpg` | Board photo |
| `Uncanny Eyes ... Instructables.pdf` | Reference for the dragon-eye animation |

Pin reference lives in this file and at the top of `src/main.cpp`; there is no separate pin-mapping doc.

---

## Hardware Reference

### Board: Elecrow CrowPanel 1.28" Rotary Display (ESP32-S3)

- **MCU**: ESP32-S3 (dual-core, 240 MHz), 16 MB flash, 8 MB OPI PSRAM (verified)
- **Display**: 240×240 round IPS, GC9A01 over SPI
- **Touch**: CST816D at 0x15 on Wire1 (6/7); raw coords map directly to screen (verified)
- **Encoder**: rotary knob with push button
- **RGB LED**: 5× WS2812
- **Audio / BLE**: present, unused

Elecrow's example code is the source of truth for board specifics:
https://github.com/Elecrow-RD/CrowPanel-1.28inch-HMI-ESP32-Rotary-Display-240-240-IPS-Round-Touch-Knob-Screen
(`example/Arduino/RotaryScreen_1_28/RotaryScreen_1_28.ino`)

### Pins

| Function | GPIO | Notes |
|----------|------|-------|
| Display SCLK / MOSI | 10 / 11 | SPI2, 3-wire, 80 MHz |
| Display DC / CS / RST | 3 / 9 / 14 | |
| Display Backlight | 46 | HIGH = on; PWM-capable |
| **Display rail** | **1** | Must be HIGH or the display is dark |
| **"KEEP_ALIVE"** | **2** | Role unknown — see note below |
| Touch SDA / SCL | 6 / 7 | |
| Touch INT / RST | 5 / 13 | |
| Main I2C SDA / SCL | 38 / 39 | EMC2101 (0x4C) + optional OLED |
| Encoder A / B / SW | 45 / 42 / 41 | Interrupts on A and B; SW active-low |
| Power Light | 40 | Elecrow drives it LOW |
| RGB LED Data | 48 | |

**GPIO 1 & 2.** Earlier design notes called GPIO 2 a soft power latch (P-MOSFET) for the
whole board. Elecrow's example sets GPIO 1 and 2 HIGH with the comment "These two rails must
remain enabled while the display is operating." Firmware drives both HIGH first in `setup()`.

Test 2026-09-23 (USB-C powered): GPIO 2 LOW for 3 s → MCU kept running (serial heartbeat
continued) and no visible display change. So on USB power GPIO 2 neither cuts MCU power nor
blanks the display. Still possible it latches a battery/switch path that USB bypasses —
untested. `shutdown_system()` must not be relied on to power off the board.

**Round screen**: corners of the 240×240 buffer are not visible — keep content inside the
circle (e.g. `gfx.println` at (10,10) is off-screen).

---

## Build & Dependencies

See `platformio.ini`. Libraries:

| Library | Use |
|---------|-----|
| `lvgl/LVGL@^8.4.0` | UI (LVGL 8 API; Elecrow examples use LVGL 9 — API differs) |
| `lovyan03/LovyanGFX@^1.1.0` | Display driver (explicit `LGFX` class in `main.cpp`) |
| `adafruit/Adafruit BusIO` | Required by vendored EMC2101 driver |
| `bblanchon/ArduinoJson@^6.21.0` | Config JSON |
| `esp32async/ESPAsyncWebServer@^3.7.0` | Webserver (pulls AsyncTCP) |
| `arduino-libraries/NTPClient` | Listed but unused — NTP uses `configTime()` |

---

## Current Status

### ✅ Verified on hardware
- Serial boot output over native USB CDC
- 16 MB flash + 8 MB PSRAM detected
- Touch: own minimal CST816D driver in `main.cpp` (init sequence from Elecrow; single-attempt
  reads — Elecrow's retries forever). Feeds LVGL pointer; logs `Touch: x,y` on press.
- Encoder: table-driven quadrature decoder (from Elecrow), 1 count per detent; debounced
  button. Serial prints `Encoder: n` / `Button pressed`.
- Main screen (`ui.cpp`): 270° cyan arc + large target RPM (knob, `rpmStep` per detent,
  clamped to min/max; not persisted, starts at 0), clock (12h/24h + TZ, `--:--` until
  valid), one-line status box at bottom (`[AP ]IP:port` / `WiFi lost`). Status box flashes
  red/white via `ui_set_attention()` when a saved network is configured but not connected.
- SPIFFS mount + config defaults written
- WiFi AP mode (`WiFi-Fan-Knob-XXXXXX` / `12345678`)
- Webserver serves `index.html` at `http://192.168.4.1:8080`
- Config tab: loads current settings, validates, saves (MQTT password never sent to browser;
  blank = keep). Brightness (PWM backlight) and time zone (POSIX TZ, US zones with DST) apply
  at boot and immediately on save via `applyDisplaySettings()` (verified on hardware).

### 🔧 Implemented, not yet verified
- NTP: background SNTP started when WiFi STA connects, re-syncs every 60 min, local time
  per configured zone (`configTzTime`; plain `configTime` would reset TZ to UTC).
- EMC2101 detection at 0x4C on I2C 38/39 — module not yet delivered; `EMC2101 not found!` expected.

### ⬜ Not started
- Web UI handlers: Home (needs fan_control), OTA
- `fan_control.cpp`, `mqtt.cpp`
- LVGL menus, dragon-eye standby
- Light-sleep standby

### Known quirks
- **First boot `task_wdt: esp_task_wdt_reset(763): task not found` spam**: expected once.
  `SPIFFS.begin(true)` formats an empty partition, and `SPIFFS::format()` removes the
  core-0 idle task from the WDT during the format. Stops when format completes.
- **Rare boot panic (~1 in 9 boots), accepted for now**: `Guru Meditation ... Unhandled debug
  exception` during `attachInterrupt()`. Coredump showed the `ipc1` task (1024-byte stack, fixed
  in precompiled SDK) overflowing when an interrupt frame lands while it installs the GPIO ISR
  service; the end-of-stack watchpoint fires and the board reboots cleanly. Not caused by our
  code. Revisit if frequency rises (newer core may have larger IPC stack).
  Coredump is saved to the `coredump` partition (0xFF0000); decode with `esp-coredump
  info_corefile` against `.pio/build/esp32-s3-devkitc-1/firmware.elf`.
- `config.system.deepSleepEnabled` name predates the light-sleep decision; not renamed
  (would change the config JSON format).

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
  ├─ MCU in light sleep, waking periodically for a slow animation frame
  │  (deep sleep ruled out: CPU halts, can't animate)
  ├─ Display minimal power
  └─ Touch screen → Wake to ACTIVE
```

### Boot Sequence (as implemented in `setup()`)

1. Display rails GPIO 1 & 2 HIGH (before any delay)
2. Serial, GPIO (encoder, power light, backlight)
3. Display init (LGFX + LVGL)
4. Main I2C (38/39) + EMC2101 probe
5. Encoder / button interrupts attached
6. Config load from SPIFFS (or defaults)
7. WiFi connect (saved SSID, else AP mode) — blocks up to 10 s
8. NTP started if STA connected (non-blocking)
9. Webserver started on `config.webserver.port` (default 8080)
10. Main loop (MQTT not yet implemented)

---

## Key Design Decisions

| Decision | Choice | Why |
|----------|--------|-----|
| WiFi | Native ESP32 (no WiFiManager) | Direct UI control in AP mode |
| Config Storage | SPIFFS + JSON | Flexible, debuggable |
| Webserver | ESPAsyncWebServer, port 8080 | Non-blocking |
| Webserver exposure | Only explicit routes, no `serveStatic` | `/config.json` in SPIFFS holds WiFi/MQTT passwords |
| UI Layout | 4-tab (Home/WiFi/Config/OTA) | Mobile-responsive |
| HA Integration | MQTT Auto-Discovery | Zero-config |
| Fan Speed | 100 RPM steps, 0-2500 RPM | Noctua fan range |
| Standby | Light sleep + slow animation | Deep sleep can't animate |
| Time Sync | SNTP background, 60 min interval | No blocking in loop |
| OTA | Web form upload | Local, no cloud |

---

## Next Steps

1. Flash + verify encoder (rotation direction, 1 count per detent, button)
2. Determine GPIO 2 role (see Hardware Reference)
3. Web UI handlers: WiFi credentials, settings save, then OTA
4. `fan_control.cpp` (EMC2101 PWM, tach) → encoder drives RPM
5. LVGL main screen (RPM arc, clock); touch driver
6. `mqtt.cpp` + Home Assistant discovery
7. Dragon-eye standby in light sleep
8. Calibration UI, audio, field testing

---

## Open Questions

1. Fan speed units in Home Assistant: RPM (0-2500) or % (0-100)?
2. Audio format for warnings: WAV, MP3, or other?
3. Dragon eye animation: pre-rendered frames or procedural LVGL drawing?
4. Noctua fan min/max PWM: initial guess 50-200 (calibrate)
5. GPIO 2: no effect on USB power — does it matter on battery/other supply?

---

## Quick Reference

- Webserver: `http://192.168.4.1:8080` in AP mode (no auth)
- MQTT broker default: `192.168.10.50:1883` (configurable)
- WiFi AP: `WiFi-Fan-Knob-XXXXXX` / `12345678`
- Serial: 115200 baud
- Config: `/config.json` on SPIFFS, auto-created on boot if missing
