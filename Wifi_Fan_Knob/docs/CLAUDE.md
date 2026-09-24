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
| `include/webserver.h` / `src/webserver.cpp` | 🟡 Partial | `/` → embedded index.html; `GET /api/status` (network + target RPM/range, fan controller, power mode); `POST /api/fan` (target RPM); `POST /api/ota` (firmware upload); `/api/wifi` save/forget/scan (scan async: 202→200); `GET/POST /api/config`, `POST /api/config/reset` |
| `web/index.html` | 🟡 Partial | 4-tab web UI; All 4 tabs wired (Home, WiFi, Config, OTA); Standby button says not implemented |
| `include/mqtt.h` / `src/mqtt.cpp` | ✅ Working | MQTT (PubSubClient) + Home Assistant discovery in own task |
| `include/fan_control.h` / `src/fan_control.cpp` | 🟡 Partial | Target RPM (knob + web, clamped to config) and EMC2101 probe; PWM/tach TODO |
| `lib/Adafruit_EMC2101/` | Vendored | Adafruit EMC2101 driver (local copy, not from registry) |
| `include/ui.h` / `src/ui.cpp` | ✅ Working | LVGL tileview pages (Main / Presets / Settings) + standby screen |

### Documentation (`docs/`)

| File | Purpose |
|------|---------|
| `STATUS_REPORT_01.md` | End-of-day report from the pre-hardware sessions |
| `STATUS_REPORT_02.md` | End-of-day report, 2026-09-23 (first hardware session) |
| `MQTT_SCHEMA.md` | Original HA discovery design (superseded; see MQTT below) |
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
| **Peripheral power switch** | **4** | Transistor for external devices; level in config |
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
- OTA (verified over home WiFi): OTA tab uploads `.pio/build/esp32-s3-devkitc-1/firmware.bin`
  to the spare slot (app0/app1), validated by `Update.end(true)` before switching. Rejects
  non-images (first byte != 0xE9) and truncated images; running firmware untouched on error.
  OTA tab shows Build ID (ELF SHA-256 prefix, changes every build) and running slot.
  Requires the web login (see below).
  CLI: `curl -u user:pass -F "firmware=@.pio/build/esp32-s3-devkitc-1/firmware.bin" http://<ip>:8080/api/ota`.
  USB upload still works after OTA (it rewrites otadata, booting app0 again).
- WiFi: hotspot (`WiFi-Fan-Knob-xxxxxx`) turns off once the saved network is joined; comes back if
  that network is lost for 60 s. Hotspot password (`config.wifi.apPassword`, default 12345678,
  8-63 chars) set on WiFi tab; applies next time hotspot starts. `/api/status` reports `hotspot_on`.
- Time zones (worldwide): page embeds posix_tz_db (MIT, 461 IANA zones -> POSIX rules) with a
  search box and "Use this browser's time zone". Config stores `display.timezone` (IANA name,
  for the UI) and `display.posixTz` (applied via setenv/tzset and `configTzTime`). Old US codes
  ("CST" etc.) are migrated on load (`migrateLegacyTimezone()` in config.cpp). Server checks
  both fields' characters/lengths only; the page supplies the rule from its table.
- LCD pages (verified): horizontal LVGL tileview, swipe left from Main. Main: RPM arc (drag
  along the ring to set speed, snaps to rpmStep; ring-only hit test (needs LV_OBJ_FLAG_ADV_HITTEST, off by default) + 15 px ext area so
  mid-screen swipes still page), clock, status box. Presets: config presets + red OFF (tap sets
  target, slides back to Main). Settings: brightness slider (live; saveConfig on release) +
  IP/SSID/MQTT info. Page dots in the arc's bottom gap. Knob turns and wake return to Main.
- Standby (`power.h`, verified): knob button held 1 s (fires while held) or web Standby/Wake
  button (`POST /api/standby`, login). Dims backlight to 10% and loads a standby screen (large
  grey clock) and sets fan target to 0. Any touch, knob turn or button press wakes (fan stays 0);
  the waking input is discarded. A web fan speed > 0 while in standby also wakes.
  Requests from web/touch are flags applied in `loop()` (LVGL not thread-safe). No light sleep yet.
- Dragon eye standby screen (verified, ~62 fps): `dragon_eye.cpp` ports Adafruit "Uncanny Eyes"
  (MIT, Phil Burgess; via Bodmer's TFT_eSPI example) to LovyanGFX. Data: `include/eyes/
  dragonEye.h` (unmodified tables, ~260 KB flash). Single eye, symmetrical lids, 128x128 drawn
  at 2x and cropped to 240x240; one frame per `eye_frame()` call (original's blocking iris loop
  replaced). In standby `loop()` renders eye frames instead of running LVGL and polls touch
  directly. Touch wake only after the screen has read "no touch" once (`standby_touch_armed`):
  pressing the knob also touches the glass. Each wake logs its cause (`Wake: button/knob/touch/
  fan target`).
- `include/ui.h` standby LVGL screen (grey clock) still exists but is no longer shown.
- Peripheral power switch (verified): GPIO 4 drives a transistor that powers everything except
  MCU/LCD (fan, lights, sensors, EMC2101). ON while awake, OFF in standby; Fan Off only sets
  target/PWM 0. ON level configurable (`config.power.activeHigh`, Config tab, default HIGH;
  applied on save). Pin undriven until config loads at boot, so hardware needs a pull holding
  the switch OFF (pull-down if active-HIGH, pull-up if active-LOW). EMC2101 is on the switched
  rail: probed 50 ms after power-on at boot and on every wake; marked absent in standby.
  I2C caution: an unpowered EMC2101 must not back-power from or drag down SDA/SCL.
- MQTT / Home Assistant (verified with HA + Mosquitto add-on at 192.168.10.85, login required):
  `mqtt.cpp` runs PubSubClient in its own task (core 0) so blocking connects never stall loop();
  retries every 15 s; reconnects after Config save. Topics `<topicPrefix>/<chipId>/...`
  (`wifi_fan_knob/24C55D/`): `speed`, `speed/set`, `standby`, `standby/set`, `running`, `rssi`,
  `uptime`, `status` (LWT online/offline, retained). Discovery (retained, `homeassistant/<comp>/
  wifi_fan_knob_<chipId>/<obj>/config`): number Fan Speed, switch Standby, binary_sensor Fan
  Running, sensors WiFi Signal + Uptime (diagnostic). State retained, on change; rssi/uptime 60 s.
  Commands go through `fan_set_target()` / `power_request_standby()`. Deviations from
  MQTT_SCHEMA.md: chip ID in ids/topics, LWT instead of "MQTT Connected" sensor, Standby switch
  instead of Power Mode sensor, no RPM sensor until EMC2101.
- Decisions: fan target starts at 0 after power loss (not resumed). Standby turns the fan
  off (target 0).
- Home tab has a large FAN OFF button (target 0, no confirmation).
- Web login (HTTP Basic; `config.webserver.username/password`): required by every POST (fan, config,
  WiFi, hotspot password, OTA, factory reset) via `require_login()`; GETs stay open. No login set =
  all changes refused (403) until one is set on the Config tab (first set needs no auth; changes
  need current login). Page keeps the login in sessionStorage and sends it on every POST.
  Factory reset additionally requires re-typing the password; it also clears the login.
- Home tab: target RPM shared with knob (page polls /api/status every 2 s; LCD redrawn only
  from loop() since LVGL isn't thread-safe). Presets hard-coded in page (match config defaults).
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
  blank = keep). Brightness (PWM backlight) and time zone apply
  at boot and immediately on save via `applyDisplaySettings()` (verified on hardware).

### 🔧 Implemented, not yet verified
- NTP: background SNTP started when WiFi STA connects, re-syncs every 60 min, local time
  per configured zone (`configTzTime`; plain `configTime` would reset TZ to UTC).
- EMC2101 detection at 0x4C on I2C 38/39 — module not yet delivered; `EMC2101 not found!` expected.

### ⬜ Not started
- `fan_control.cpp`, `mqtt.cpp`
- LVGL menus, dragon-eye standby
- Light-sleep standby

### Known quirks
- **Forgotten web login**: every change needs it, so recovery is over USB — erase the SPIFFS
  partition (config lives only there; defaults are recreated on next boot):
  `~/.platformio/penv/Scripts/python.exe ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32s3 --port COM13 erase_region 0xC90000 0x360000`
- **Boot WiFi connect often exceeds 10 s** (weak signal, ~-74 dBm): board falls back to hotspot,
  then `maintain_wifi()` retries every 20 s and turns the hotspot off once joined.
- **Routes must use `AsyncURIMatcher::exact()`**: a plain `server->on("/api/wifi", ...)` also
  matches `/api/wifi/*`, and the first-registered handler wins (this broke Forget and Reset).
- **First boot `task_wdt: esp_task_wdt_reset(763): task not found` spam**: expected once.
  `SPIFFS.begin(true)` formats an empty partition, and `SPIFFS::format()` removes the
  core-0 idle task from the WDT during the format. Stops when format completes.
- **Boot panic in `attachInterrupt()` (KNOWN ISSUE, parked 2026-09-23)** — see
  "Known issue: ipc1 boot panic" section below for evidence, repro and candidate fixes.
- `config.system.deepSleepEnabled` name predates the light-sleep decision; not renamed
  (would change the config JSON format).

---

## Known issue: ipc1 boot panic (parked)

**Symptom**: `Guru Meditation Error: Core 1 panic'ed (Unhandled debug exception)` early in boot,
then an automatic reboot that succeeds. Never seen after boot completes.

**Frequency**: ~4 in ~40 boots observed (2026-09-23), all during USB/OTA flash-and-reset testing.

**Evidence** (3 coredumps, identical): crashed task `ipc1`; backtrace
`gpio_isr_register_on_core_static → esp_intr_alloc → heap_caps_malloc → multi_heap_malloc`,
faulting at `_xt_lowint1+15` with the stack pointer ~8 bytes from the end of ipc1's stack.
Cause: `attachInterrupt()` installs the GPIO ISR service via an IPC call; ipc1's stack is
1024 bytes (`CONFIG_ESP_IPC_TASK_STACK_SIZE`, fixed in the precompiled Arduino SDK). If a
level-1 interrupt arrives while ipc1 is deep in `malloc` (heap poisoning on), the interrupt
frame overflows the stack and the FreeRTOS end-of-stack watchpoint fires. Not our code.

**Reproduce / measure** (board on COM13, serial monitor closed):
```bash
PY=~/.platformio/penv/Scripts/python.exe; ET=~/.platformio/packages/tool-esptoolpy/esptool.py
for i in $(seq 1 50); do $PY $ET --chip esp32s3 --port COM13 --after hard_reset chip_id >/dev/null 2>&1
  $PY readser.py | grep -c Guru; done   # readser.py: open COM13 (retry until present), read ~15 s
```

**Decode a crash** (dump persists in the `coredump` partition until the next crash):
```bash
$PY $ET --chip esp32s3 --port COM13 read_flash 0xFF0000 0x10000 coredump.bin
pip install esp-coredump   # in a scratch venv
python -m esp_coredump --chip esp32s3 info_corefile --core coredump.bin --core-format raw   --gdb ~/.platformio/packages/tool-xtensa-esp-elf-gdb/bin/xtensa-esp32s3-elf-gdb.exe   .pio/build/esp32-s3-devkitc-1/firmware.elf
```
The ELF must be the exact build that crashed.

**Candidate fixes** (cheapest first; measure each with the repro loop, e.g. 0 in 100 boots):
1. Attach encoder/button interrupts first in `setup()`, before USB CDC output, display SPI DMA,
   WiFi and I2C are active (fewer interrupt sources during the IPC call).
2. Call `gpio_install_isr_service()` ourselves at the very top of `setup()` so the IPC
   allocation happens while the system is quiet; later `attachInterrupt()` calls reuse it.
3. Update the pioarduino platform / Arduino core and check whether its sdkconfig raises
   `CONFIG_ESP_IPC_TASK_STACK_SIZE`.
4. Build the Arduino SDK libs ourselves (pioarduino `custom_sdkconfig`) with a larger IPC
   stack — most effective, slowest builds.

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

1. `fan_control.cpp`: EMC2101 PWM + tach once the module arrives (target RPM already wired)
2. Light sleep / throttling in standby (see dragon eye upgrades)
3. Knob short press → menu (currently only logged)
4. MQTT: add actual RPM sensor once the EMC2101 reads tach
5. Calibration UI, audio, field testing
6. GPIO 2 role on non-USB power (see Hardware Reference)

### Later (user notes)
- **Dragon eye upgrades (re-look after project is complete)** — current version works as
  agreed. Candidates: full 240x240 graphics (regenerate tables with Adafruit's tablegen from the
  source images) instead of 2x pixel doubling; "sleeping" behaviour (mostly closed / twitching /
  peeking, opening on approach); own standby brightness (currently 10%, dim for the eye);
  throttled frame rate or light sleep between frames (renders flat out at ~62 fps now);
  other eye styles (Uncanny Eyes has several); revisit wake gestures (knob press = glass touch);
  extract as standalone project — break out all dragon eye code, display config, and setup steps
  into a semi-universal project that works with any LovyanGFX-compatible display. Document the
  GC9A01 example and how to adapt it to other boards.
- **GitHub link on the web page** — add a link to the project's GitHub repository at the
  bottom of the Home tab (`web/index.html`).
- **FAN OFF button icon on Android** — the power button icon (⏻) does not render on Android
  phones. Check browser compatibility for the Unicode character and consider a fallback text
  label or SVG icon.

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
