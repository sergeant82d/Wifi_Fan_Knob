# Project history

The long guide to the WiFi Fan Knob: what each part does and why, hardware notes, gotchas,
decisions and plans. Read it before starting work. The short root `CLAUDE.md` loads in every
session and points here; open items are in `docs/TODO.md`. (This file was `docs/CLAUDE.md`
until 2026-10-07; older notes, commits and status reports use that name.)

**Repository**: https://github.com/sergeant82d/Wifi_Fan_Knob.git
**Repo root / PlatformIO project**: `D:\GitHub\VSCodeProjects\Wifi_Bench_Fan\Wifi_Fan_Knob`

## Current status (2026-10-07)

- **Board:** Elecrow CrowPanel 1.28" rotary display (ESP32-S3), COM13, `http://192.168.10.102:8080`
  (static IP). Platform pinned to pioarduino 51.03.04 = Arduino core 3.0.4. Flash 86.6 % of the
  6.25 MB app slot (room for about 4 more photo eyes).
- **Working and checked on the board:** fan control through the EMC2101 with fan profiles and
  Auto Configure (NF-P12 fitted; the 140 mm fan died 2026-10-06), LCD pages System / QR code /
  Main / Settings with the knob, eyes (10 Uncanny + photo Dragons 2-11), web page (left sidebar,
  `D:\GitHub\WEB_STYLE.md`), Home Assistant over MQTT, RGB LEDs, SHT41 + SGP41 air readings,
  APDS-9999 light level with Auto brightness, restart (LCD / web / HA), notes, web LCD view, OTA.
- **Waiting:** AMG8833 thermal camera (presence); Auto mode from the VOC Index (thresholds from
  a few days of HA history); relook at LCD swiping; FPC breakout (fan PWM to the UART0
  connector, GPIO 4 pull-down). Details and the rest: `docs/TODO.md`.

---

## Quick Setup

```bash
git clone https://github.com/sergeant82d/Wifi_Fan_Knob.git
cd Wifi_Fan_Knob
```

- Open `Wifi_Fan_Knob.code-workspace` (repo root) in VS Code with PlatformIO.
- Board environment: `esp32-s3-devkitc-1`, overridden in `platformio.ini` for this board:
  16 MB QIO flash, OPI PSRAM (`qio_opi`), `default_16MB.csv` partitions (2 OTA slots).
- Serial is native USB (COM port VID 303A:1001); needs `-DARDUINO_USB_CDC_ON_BOOT=1`,
  otherwise `Serial` goes to unconnected UART0 and only IDF logs reach USB.
- Platform is pioarduino 51.03.04 → Arduino core 3.0.4 / ESP-IDF 5.1, pinned by URL in
  `platformio.ini` (2026-09-28). Plain `espressif32` means "whichever pioarduino release was
  installed last on this PC"; ETH_Touch_PWM's 55.03.311 (core 3.3.11) had replaced it. Both
  projects share one core folder, so the first build after switching projects re-downloads
  the core (~3 min).
- Build: PlatformIO **Build**. Flash: **Upload** only — the web UI (`web/index.html`) is
  compiled into the firmware via `board_build.embed_txtfiles`. SPIFFS holds `/config.json`,
  `/fans.json` (fan profiles) and `/notes.json` (Home tab notes). Do not run `uploadfs`: it
  rewrites the whole SPIFFS partition, wiping them (config defaults are recreated on next
  boot; profiles and notes are lost).
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
| `web/index.html` | ✅ Working | Web UI, left sidebar (`D:\GitHub\WEB_STYLE.md`): Dashboard, Fan Control, Home Assistant, Network, System |
| `include/mqtt.h` / `src/mqtt.cpp` | ✅ Working | MQTT (PubSubClient) + Home Assistant discovery in own task |
| `include/fan_control.h` / `src/fan_control.cpp` | ✅ Working | Target RPM → EMC2101 PWM (12 kHz, 30 steps), tach RPM, Auto Configure |
| `include/fan_profiles.h` / `src/fan_profiles.cpp` | ✅ Working | Up to 5 fan profiles (`/fans.json`): measured table, max RPM, presets |
| `lib/Adafruit_EMC2101/` | Vendored | Adafruit EMC2101 driver (local copy, not from registry) |
| `include/ui.h` / `src/ui.cpp` | ✅ Working | LVGL tileview pages (System / QR code / Main / Settings since 2026-10-07; knob left wraps from System to Settings), segments, knob menu |
| `include/dragon_eye.h` / `src/dragon_eye.cpp` | ✅ Working | Animated eye (standby + screensaver), native 240x240 |
| `include/eye_styles.h` / `src/eye_styles.cpp` | ✅ Working | The eye styles: 10 Uncanny Eyes (`include/eyes/*Eye.h` = Adafruit tables) + photo eyes |
| `include/lcd_view.h` / `src/lcd_view.cpp` | ✅ Working | Web LCD view: copies of the screen for `GET /api/lcd` (LVGL snapshot; eye rows copied as drawn) |
| `include/air.h` / `src/air.cpp` | ✅ Working | SHT41 + SGP41 read once a second from `loop()` (raw commands, no driver library); VOC/NOx Index via Sensirion's Gas Index Algorithm |
| `include/presence.h` / `src/presence.cpp` | ✅ Working | APDS-9999 proximity + light (lux) read from `loop()` (Adafruit APDS9999 library) |
| `include/leds.h` / `src/leds.cpp` | ✅ Working | RGB LEDs (5x WS2812, GPIO 48): Solid / Flash / Breathe / Rainbow, brightness capped at 100 of 255 |
| `include/photo_eye.h` / `src/photo_eye.cpp` | ✅ Working | Photo eye renderer (artist open/shut pictures: moving iris, reactive slit pupil, lids); data `include/eyes/*Photo.h` from `tools/photo_eye.py` |

### Documentation (`docs/`)

| File | Purpose |
|------|---------|
| `PROJECT_HISTORY.md` | This file: the long guide (was `docs/CLAUDE.md` until 2026-10-07) |
| `TODO.md` | The user's list: Your notes / Open / Done, tags (all-projects format) |
| `DISPLAY_GUIDE.md` | How to change the LCD: colours, fonts, segments, pages (human-readable) |
| `PHOTO_EYES.md` | Photo eyes: how they animate, adding a new one (`tools/photo_eye.py`, `eye.json`), artist spec |
| `EYE_IMAGE_PROMPT.md` | Prompt: cut a picture from a `*_dragon-eyes-8.5x11.jpg` sheet and show it on the LCD (test build); eyelid image spec for the artist |
| `Status_Reports/STATUS_REPORT_01.md` | End-of-day report from the pre-hardware sessions |
| `Status_Reports/STATUS_REPORT_02.md` | End-of-day report, 2026-09-23 (first hardware session) |
| `Status_Reports/STATUS_REPORT_03.md` | End-of-day report, 2026-09-24 (second hardware session) |
| `Status_Reports/STATUS_REPORT_04.md` | End-of-day report, 2026-09-25 (third hardware session: fan control, profiles, Auto Configure, LCD theme) |
| `Status_Reports/STATUS_REPORT_05.md` | End-of-day report, 2026-09-26 (web page and LCD to-do items, display modes, standby prompt, photo eyes Dragon 2-10) |
| `Status_Reports/STATUS_REPORT_06.md` | End-of-day report, 2026-09-28 (handoff items: USB serial fix, QR code page, web LCD view, Notes box) |
| `Status_Reports/STATUS_REPORT_07.md` | End-of-day report, 2026-10-06 (RGB LEDs, SHT41 + SGP41 + APDS-9999, dead 140 mm fan and TACH pull-up, Home tab reorder) |
| `Status_Reports/STATUS_REPORT_08.md` | End-of-day report, 2026-10-07 (Auto brightness, all-projects standards, left-sidebar web page, Restart, docs split, smooth LCD swiping) |
| `MQTT_SCHEMA.md` | Original HA discovery design (superseded; see MQTT below) |
| `SPIFFS_CONFIG_SCHEMA.md` | JSON config structure |
| `images/` | Pictures used by the docs: board photo, display layout diagram, arc-button design reference |
| `Uncanny Eyes ... Instructables.pdf` | Reference for the dragon-eye animation |

**Status reports** always go in `docs/Status_Reports/`, named `STATUS_REPORT_NN.md` with
the next number (one per session day), and get a row in the table above.

**Eye artwork** (source art that ends up in the firmware) lives in `assets/eye_art/`, not
`docs/`: the artist's open/shut pairs used by the photo eyes (`assets/eye_photos/dragonN/eye.json` names its pair):
Dragon 2 = `5-1`/`6`, 3 = `01A`/`01B`, 4 = `02A`/`02B`, 5 = `7`/`8`, 6 = `9`/`9B`,
7 = `15`/`16`, 8 = `17`/`18`, 9 = `19`/`20`, 10 = `21`/`22`, 11 = `23-4`/`24-4`. Reference and archive material
(not used by the build) is in `assets/eye_art/image_sheets/`: the purchased
`*_dragon-eyes-8.5x11.jpg` sheets (300 dpi). Moved there by the user 2026-09-27.

Pin reference lives in this file and at the top of `src/main.cpp`; there is no separate pin-mapping doc.

---

## Hardware Reference

### Board: Elecrow CrowPanel 1.28" Rotary Display (ESP32-S3)

- **MCU**: ESP32-S3 (dual-core, 240 MHz), 16 MB flash, 8 MB OPI PSRAM (verified)
- **Display**: 240×240 round IPS, GC9A01 over SPI
- **Touch**: CST816D at 0x15 on Wire1 (6/7); raw coords map directly to screen (verified)
- **Encoder**: rotary knob with push button
- **RGB LED**: 5× WS2812
- **Bluetooth LE**: present, unused

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
| **Peripheral power switch** | **4** | Transistor switching the 12 V rail (fan) only; level in config |
| Touch SDA / SCL | 6 / 7 | |
| Touch INT / RST | 5 / 13 | |
| Main I2C SDA / SCL | 38 / 39 | SHT41 (0x44), EMC2101 (0x4C), APDS-9999 (0x52), SGP41 (0x59); all on always-on 3.3 V. Boot/wake log lists them (`[I2C] Found:`) |
| EMC2101 TACH (on the Adafruit 4808 board) | — | **Needs a 10 kΩ pull-up to 3.3 V** (added by user 2026-09-25; found open and discoloured on the TACH end 2026-10-06 and replaced: symptom was 0 RPM measured at every speed; tach line then tested open to GND, 3.3 V and 5 V; the board's own TACH pull-up is off unless its solder jumper is bridged). Without it the tach floats and counts PWM noise |
| Encoder A / B / SW | 45 / 42 / 41 | A/B decoded by PCNT hardware (no interrupts); SW polled, active-low |
| Power Light | 40 | Elecrow drives it LOW |
| RGB LED Data | 48 | 5x WS2812, GRB (Elecrow example); `leds.cpp` |

**GPIO 1 & 2.** Earlier design notes called GPIO 2 a soft power latch (P-MOSFET) for the
whole board. Elecrow's example sets GPIO 1 and 2 HIGH with the comment "These two rails must
remain enabled while the display is operating." Firmware drives both HIGH first in `setup()`.

Test 2026-09-23 (USB-C powered): GPIO 2 LOW for 3 s → MCU kept running (serial heartbeat
continued) and no visible display change. So on USB power GPIO 2 neither cuts MCU power nor
blanks the display. Still possible it latches a battery/switch path that USB bypasses —
untested. `shutdown_system()` must not be relied on to power off the board.

Decided 2026-09-25: no further GPIO 2 test. The finished build runs from a fixed 12 V / 5 A
supply (12 V for the fans, regulated 5 V for the level shifters' high side, 3.3 V for the
rest), with no battery, so there is nothing for a latch to hold. Keep driving it HIGH as
Elecrow does; the board cannot switch itself off in software.

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
- Static IP / DHCP (WiFi tab "Network Configuration", `POST /api/wifi/config`, login): stored in
  `config.wifi.useStaticIp/staticIp/staticGateway/staticSubnet/staticDns`; server checks each is
  a valid address and that IP and gateway share the subnet before changing anything. Applied by
  `apply_ip_config()` before every `WiFi.begin()`; saving reboots the board to apply it. DNS
  defaults to the gateway. Verified on hardware (DHCP and static).
  A wrong static IP can leave the board unreachable while WiFi still reports connected (so
  the fallback hotspot never starts): recover over USB with the SPIFFS erase below.
- Time zones (worldwide, verified on hardware): page embeds posix_tz_db (MIT, 461 IANA zones -> POSIX rules) with a
  search box and "Use this browser's time zone". Config stores `display.timezone` (IANA name,
  for the UI) and `display.posixTz` (applied via setenv/tzset and `configTzTime`). Old US codes
  ("CST" etc.) are migrated on load (`migrateLegacyTimezone()` in config.cpp). Server checks
  both fields' characters/lengths only; the page supplies the rule from its table.
- LCD pages (verified): horizontal LVGL tileview; Settings is left of Main (swipe right from Main). Main: RPM arc (drag
  along the ring to set speed, snaps to rpmStep; ring-only hit test (needs LV_OBJ_FLAG_ADV_HITTEST, off by default) + 8 px ext area (stops at the segments' outer edge, SEG_R_OUT) so
  mid-screen swipes still page), target RPM, actual RPM, clock. Settings: brightness slider (live; saveConfig on release; turns Auto brightness off; shows "Auto NN%" while Auto runs) +
  IP box + SSID/MQTT info. Page dots in the arc's bottom gap. Knob turns and wake return to Main.
  Pages come from the `PAGES` table in `ui.cpp` (name + builder); tiles, dots and the knob
  menu follow it, so adding a page = one builder + one table row (keep `MAIN_PAGE` /
  `SETTINGS_PAGE` pointing at the right rows).
  On every page after Main, a tap on empty space slides back to Main (verified).
- Main quick segments (verified): Off + Low/Med/High/Max drawn as 5 ring slices inside the
  RPM arc (non-clickable `lv_arc`s, `SEG_*` constants in `ui.cpp`); taps on the Main tile are
  mapped to a segment by angle/radius (`seg_at`). A segment is gold while pressed and while
  its speed is the current target. RPM arc ends are derived from the band (152° to 28°) to
  clear the bottom gap. RPM number uses Montserrat 40. How-to: `docs/DISPLAY_GUIDE.md`.
- Theme + layout (verified 2026-09-25): the web page's dark blue and gold, as named
  `THEME_*` colours at the top of `ui.cpp`. Main: navy gradient; gold target RPM (y -6) and
  arc; dim "RPM" caption (y 22, "Setup NN%" during Auto Configure); actual RPM "now N" in light
  text (y 48, LVGL recolour for the dim "now", hidden when stopped); dim clock in the bottom
  gap (y 82). Settings (same dark theme) now holds the IP box (`create_status_box(tile, 22)`,
  panel + gold text + gold border); its info shows SSID + MQTT only. Auto Configure has its
  own gold screen (reversed theme, `create_config_screen()` / `update_config_screen()`):
  progress ring, fan name, big %, live RPM, step, cancel hint; loaded automatically while it
  runs, then the result for 30 s (`CFG_RESULT_MS`; knob turn/press skips), then Main unless something else
  (standby) took the screen. WiFi lost flashes both the
  IP box and Main's clock red (`status_flash_cb`). Page dots have a navy outline so the gold
  current dot shows on the gold page.
- Knob menu (verified): short press shows the page names (current one
  highlighted); turn to choose, press again or tap a name to go; tap outside closes. While
  open, knob turns don't change RPM. Standby closes it.
- Double-tap on Main (verified): two taps within 400 ms (millis) stop the
  fan (target 0, red "Fan stopped" popup) or show "Fan is not running"; popup 1.5 s.
  Status box bubbles its taps to the page; the arc keeps its own touches.
- LCD pages are now Main + Settings (the Presets page was dropped: Main's segments replace it).
- Fan max RPM editable on the web (Config -> Fan Presets, `fan_max_rpm`, rpmStep..20000;
  `config.fan.maxRpm`, default 2500): limits knob, LCD arc (range synced in ui_update), web
  slider, presets and HA (discovery republished on the post-save MQTT reconnect). A running
  target above the new max is re-clamped.
- Preset speeds editable on the web (Config -> Fan Presets): `preset_low/medium/high/max` in
  `POST /api/config`, each within minRpm..maxRpm and in order Low <= Med <= High <= Max;
  `/api/config` returns `fan.presets`. Home tab preset buttons are filled from them. The LCD
  segments read them at tap time; `ui_update()` re-checks the lit segment each second.
- LCD brightness on the web Home tab (below Manual Speed Control): `POST /api/brightness`
  (login, 10-100), applied and saved on slider release; no longer part of `/api/config`.
  The LCD Settings slider follows web changes (ui_update, unless being dragged).
- Screensaver (`main.cpp`): dragon eye after `config.display.screensaverSec` idle seconds
  (default 30, 0 = off, 0-3600, Config -> Display & Interface). Activity = knob, button, touch
  (`note_activity()`) or any fan target change. Fan/peripherals keep running, brightness
  unchanged. Touch, knob turn or short press only dismiss it; long press still -> standby;
  a web/MQTT speed change dismisses it. Standby clears it and shows the eye itself.
  Verified on hardware: starts after the delay; touch/knob/short press dismiss; long press ->
  standby; web speed change dismisses.
  `/api/status` power_mode is "Active (screensaver)" while it shows (`power_screensaver_on()`).
  Web Home tab "Screensaver" / "Wake display" button: `POST /api/screensaver` on=1/0 (login),
  a flag applied in loop() (`power_request_screensaver()`), ignored in standby (button
  disabled). Also a HA switch (Screensaver).
- Standby (`power.h`, verified): knob button held 1 s (fires while held) or web Standby/Wake
  button (`POST /api/standby`, login). Dims backlight to 10% and loads a standby screen (large
  grey clock) and sets fan target to 0. Any touch, knob turn or button press wakes (fan stays 0);
  the waking input is discarded. A web fan speed > 0 while in standby also wakes.
  Requests from web/touch are flags applied in `loop()` (LVGL not thread-safe). Power saving in
  standby = external power off (GPIO 4) + eye at 15 fps; no light sleep (decided 2026-09-24).
- Eye (standby + screensaver, verified): `dragon_eye.cpp` ports Adafruit "Uncanny Eyes" (MIT,
  Phil Burgess; via Bodmer's TFT_eSPI example) to LovyanGFX. 10 styles (`eye_styles.cpp`):
  Dragon, Human, Cat, Goat, Owl, Doe, Newt, Nauga, No sclera, Terminator. Data:
  `include/eyes/*.h` = Adafruit's `uncannyEyes/graphics` tables (unmodified, 128 px, each
  included in its own namespace; `eye_limits.h` / `eye_undef.h` handle the macros). Chosen on
  the web (Config -> Display & Interface, `eye_style`, `config.display.eyeStyle`, default
  "dragon"); loop() applies a change within 1 s. Native 240x240: when a style is selected its
  tables are upscaled once into PSRAM (bilinear sclera/lids, iris polar angles recomputed,
  distance interpolated; 0.3-0.5 s, ~0.4-0.7 MB). The source art is 128 px, so it's smooth, not
  more detailed. Pixels outside the round screen are skipped. Iris formula and per-style pupil
  limits are the original's (Bodmer's differed). Measured 37-52 fps (all 10 styles cycled twice,
  no leaks). Standby draws at most `STANDBY_EYE_FPS` (15; measured 14), screensaver flat out.
  Firmware 3.3 MB of the 6.5 MB OTA slot (before the photo eyes). In standby/screensaver `loop()` renders eye frames
  instead of running LVGL and polls touch directly.
  Photo eyes (Dragon 2-11): see the photo eyes entry below.
  Sleeping eye (standby only, `eye_set_sleeping()` from `set_standby()`): lids close over 2 s,
  stay shut 3.5-10 s, then a twitch (50%: opens 20-45% for 0.4-0.7 s) or a peek (opens
  50-85%, holds 1.5-4 s, closes slowly; tuned 2026-09-24 after hardware review). While fully shut nothing is redrawn: measured 12-69 draws per
  10 s in standby vs ~150 awake at 15 fps. Standby backlight = `config.display.standbyBrightness`
  (web Config -> Display & Interface, 0-100 %, default 10, 0 = off; replaced STANDBY_BRIGHTNESS). Touch wake only after the screen has read "no touch" once (`standby_touch_armed`):
  pressing the knob also touches the glass. Each wake logs its cause (`Wake: button/knob/touch/
  fan target`).
  Wake gestures (standby only, verified 2026-09-25): the first new tap stirs the eye
  (`eye_stir()`: opens over 400 ms, blinks, `eye_look()` follows the finger), then `WAKE_TAPS`
  (4) more taps wake the board; each tap restarts `STIR_MS` (5 s); when it closes again the
  count restarts. A knob turn stirs it and glances that way (`KNOB_GLANCE_MS`); a knob press,
  web or HA wakes at once. A tap only counts after the finger has been off for `TAP_LIFT_MS`
  (60 ms): single dropped touch readings were counting as extra taps. A stirred eye draws at
  full frame rate. The screensaver is unchanged (any input dismisses it). Logs `Stir: touch/
  knob`, `Wake tap n/4`, `Wake: 4 taps`. Constants listed in `DISPLAY_GUIDE.md` section 5.
- Web page changes from `docs/TODO.md` (verified 2026-09-26): Home tab presets styled like the
  Display Mode buttons, the one at the current target solid gold (`showPresets()`); speed
  slider sends live (throttled to 250 ms, final send on release; errors shown only then),
  Apply Speed removed. New **Fan Settings** tab (between WiFi and Config): Fan Profiles, Min/Max
  PWM, Fan max RPM, presets, saved by `POST /api/fan/settings` (`apply_fan_form()`; also syncs
  the active profile and reconnects MQTT); `/api/config` no longer takes fan fields. Config
  tab order: Display & Interface, time zone/format, MQTT, Peripheral Power Switch, Save, Web
  Login (retitled "protects all changes"; kept on Config, user decision), Factory Reset. Header
  shows `WiFi connected to "<ssid>"` / `Hotspot "<ssid>"`.
- TODO.md items, second batch (verified 2026-09-26): Settings moved left of Main (`PAGES` order,
  `MAIN_PAGE` / `SETTINGS_PAGE`; start-up, wake and tap-back use `MAIN_PAGE`); knob: fan target
  0 + left turn → Settings (`ui_show_settings()`), on Settings a right turn → Main, speed
  unchanged. Settings "Screensaver" switch = temporary pause (`power_set_saver_paused()`, not
  saved): no automatic screensaver (so no standby prompt); ends when the fan target goes from
  >0 to 0, at standby, after `config.display.saverPauseMaxMin` (default 120, 0 = no limit), or
  on restart; closed-eye icon left of the Main clock while paused; `/api/status` saver_paused,
  shown in the web Display Mode note. Config tab: time zone/format in a "Clock" card. Port 80:
  second AsyncWebServer that redirects to `:<port>` (so `http://fanknob.local` works; tested).
- Device name / mDNS (verified 2026-09-26): `config.wifi.hostname` (default "fanknob"; 1-31
  lowercase letters, digits, hyphens), set in WiFi tab → Network Configuration (saved with the
  static IP settings; restarts). `WiFi.setHostname()` in `apply_ip_config()` (router list),
  `MDNS.begin()` + `_http._tcp` service after `init_wifi()`: `http://fanknob.local:8080`
  (tested from the PC). Some Android versions don't resolve `.local`.
- QR code page (verified 2026-09-28, from `HANDOFF_FROM_ETH_Touch_PWM.md` item 3): left-most
  LCD page (QR code | Settings | Main), LVGL's QR widget (`LV_USE_QRCODE`). On home WiFi it
  opens `http://<IP>/` (port 80 redirects; IP because Android often can't open .local).
  Hotspot: join code first, tap the code for the page link (**hotspot view untested**). Off
  Main, the knob now steps one page per click the way it turns (`ui_step_page()`).
  `docs/DISPLAY_GUIDE.md` section 4.
- Web LCD view (verified 2026-09-28 by the user on PC and phone, handoff item 2): Home tab "LCD" card, a round 240 px canvas refreshed every 2 s while the Home tab
  shows. `GET /api/lcd` (no login) = raw 240x240 RGB565 little-endian (115,200 B, header
  `X-Frame`); 503 = no copy yet or the last one still sending (page retries in 1.2 s). The
  screen can't be read back (no MISO), so `lcd_view.cpp` copies frames on the way to it:
  `lv_snapshot_take_to_buf(lv_scr_act())` in `loop()` for LVGL screens (`LV_USE_SNAPSHOT`),
  and the eye renderers copy each row (`lcd_view_eye_row()`) during one frame a second;
  `eye_redraw()` makes a shut sleeping eye draw once for it. Captures run only for 5 s after
  a request, at most 1/s. Two 115 KB PSRAM buffers; publish skips while a frame is sending.
  Checked: Main, screensaver eye (open and mid-blink), shut standby eye; standby eye stays
  14 fps while captured. Not on the capture: LVGL's top layer (nothing uses it now).
  On phones (max-width 768 px) the LCD card is first on the Home tab (`#home` becomes a flex
  column, `#lcd-card { order: -1 }`); desktop order unchanged. Verified by the user.
- Notes box (verified 2026-09-28 by the user, handoff item 4): Home tab "Notes" card under
  the LCD card: textarea, 12 emoji buttons (insert at cursor), byte counter, "last saved
  <time> by <user>", `beforeunload` warning while unsaved. `/notes.json` =
  `{"text","saved","by"}`, separate from `config.json`. `GET /api/notes` (no login) serves
  the file (`{"text":""}` if none); `POST /api/notes` (login) is a plain form post, field
  `text` (the library parses long form bodies itself, no body handler), `NOTES_MAX` 4000
  UTF-8 bytes; the reply is `{"saved","by"}`. `saved` is blank until NTP has synced.
- RGB LEDs (verified 2026-10-06 by the user: every effect, colour, brightness, speed,
  standby, Home Assistant; TODO 2026-10-03): 5x WS2812 on GPIO 48 behind the knob, Adafruit
  NeoPixel library, drawn by `leds_update()` in `loop()` (max 50/s, sent only on change).
  Effects Solid, Flash (2 s period at speed 1, 0.2 s at 10), Breathe (8 s .. 0.8 s, squared
  fade), Rainbow (hues spread over the 5 LEDs, a turn in 10 s .. 1 s). Off is the "on" flag, so
  the effect is kept. Stays on in standby (user 2026-10-07; was off). **Brightness cap `LED_BRIGHTNESS_MAX` = 100 of 255 (user
  2026-10-06)**: applied when drawing, also the top of the web slider and HA's 100 %.
  `config.leds` = on, effect, color (0xRRGGBB), brightness 1-100, speed 1-10; defaults off,
  gold 0xFFD700, 50, 5. Web: Home tab "LEDs" card (under Display Mode), `POST /api/leds`
  (login; any of on, effect, color #RRGGBB, brightness, speed; all checked before applying),
  values in `GET /api/config` `leds` (the card fills on page load, so an HA change shows after
  a reload). HA: light "LEDs" (JSON schema, `brightness_scale` 100, rgb, effect list) on
  `<base>/leds` + `/leds/set`, number "LED Speed" on `<base>/led_speed`. MQTT buffer 1536.
- Air sensors (verified 2026-10-06 on the board: readings at boot, through standby and after
  wake; HA entities, temperature accuracy and eye smoothness confirmed by the user): Adafruit
  SHT41 (0x44) + SGP41 (0x59) on the main bus, always-on 3.3 V. `air.cpp` reads them from
  `loop()` once a second as a non-blocking sequence: SHT41 0xFD, wait 10 ms, read; SGP41
  0x2619 with the SHT41's RH/T as compensation (0x2612 conditioning with defaults for the
  first 10 s after boot), wait 55 ms, read; Sensirion CRC-8 checked. Not a separate task: Wire's
  lock covers one call, not a request plus its reads, so two tasks could mix replies; and
  Sensirion's driver libraries delay() inside calls. Only `sensirion/Sensirion Gas Index
  Algorithm` is used (VOC Index: ~100 = the room's normal, learned over hours, 0 for the first
  ~45 s; NOx Index: 1 = normal). `GET /api/status` `air` = state ("ok", "warming up", "no
  sensor") + temp_c, humidity, voc, nox when valid. Web: Home tab "Air" card (°F and °C). HA:
  sensors Temperature (°C, HA converts), Humidity, VOC Index, NOx Index, every 10 s, valid
  values only. First readings: 26.5 °C, 42 %RH, NOx 1.
- APDS-9999 readings (verified 2026-10-06 on the board; step 1 of the presence plan):
  Adafruit APDS9999 library (no delays inside; the chip measures by itself, `loop()` reads
  the result registers): proximity 11-bit (0-2047, higher = closer) every 200 ms, lux from
  the green channel every 1 s. `GET /api/status` `presence` = ok, prox, lux. Web: Home tab
  "Presence & Light" card. HA: sensors Proximity and Illuminance, every 10 s.
  **Bench test, default LED settings, sensor tilted ~30 deg back, facing the user:** empty
  bench 2-7, sitting 1-9, leaning in 6-11, standing 1-8, hand at ~20 cm peaks 13-16. So at
  working distance it can't see a person (it's a phone-style near sensor). The lux reading
  dropped with the user's shadow (36 -> 16 lx leaning in), but that depends on the lamps.
  **Full power** (25 mA, 255 pulses; user seated 80-110 cm away): empty bench ~100 (75-120),
  sitting the same, leaning in ~135 (115-155). Still no seated detection; leaning in is too
  near the noise. **Conclusion (2026-10-06): the APDS-9999 can't do presence at this bench;
  use it for light level (auto LCD brightness) and maybe a close hand wave. For presence:
  the AMG8833 thermal camera (on the user's hardware list) or an mmWave radar.**
  The chip is always powered and keeps its registers over a board restart, so
  `presence_begin()` writes the datasheet reset values (PS_VCSEL 0x36, 8 pulses) every boot.
- Auto brightness (verified 2026-10-07 by the user: response speed, LEDs following, standby,
  slider turning Auto off): `config.display.autoBrightness` / `autoMin` (Dark, default 20 %) /
  `autoMax` (Bright, default 100 %). `auto_brightness_update()` in `main.cpp`: APDS-9999 lux
  sampled once a second, log10 averaged over `AUTO_AVG_S` = 5 s, mapped on a log scale from
  5 lx (Dark) to 500 lx (Bright); the backlight fades 1 % per `AUTO_STEP_MS` = 50 ms (both
  halved from 10 s / 100 ms at the user's request, 2026-10-07). The LED ring follows the same
  fraction, LED brightness 10..90 (`AUTO_LED_MIN/MAX`; user 2026-10-07), via
  `autoLedBrightness()` (-1 when off); LEDs keep following in standby, the screen keeps
  `standbyBrightness`. Any manual brightness change turns Auto off: LCD slider, web screen
  slider, HA "LCD Brightness", and an LED brightness change (web or HA) too. Web: LCD card
  checkbox + Dark/Bright sliders, `POST /api/brightness/auto` (auto, min, max);
  `/api/status` `auto_brightness`, `backlight`. HA: switch "LCD Auto Brightness", numbers
  "LCD Auto Min" / "LCD Auto Max".
  **What shows Auto's live level, and what doesn't (told to the user 2026-10-07):**
  - Home Assistant never shows Auto's current levels. While Auto is on, the "LCD Brightness"
    number shows the manual screen level (`config.display.brightness`) and the "LEDs" light
    shows the set LED brightness (`config.leds.brightness`). Those are the levels used again
    when Auto is turned off. Reason: publishing the live levels would send an MQTT message on
    every fade step (up to 20 a second). The "LCD Auto Brightness" switch shows Auto is on.
  - Web page: the LCD card's brightness slider does follow Auto's live level (from
    `/api/status` `backlight`, every 2 s); the LEDs card's brightness slider shows the set
    LED brightness, like HA.
  - LCD Settings page: the slider follows Auto's live level, labelled "Auto NN%".
- Web page, left sidebar (verified 2026-10-07 by the user on PC and phone): follows the
  all-projects standard `D:\GitHub\WEB_STYLE.md` (reference ETH_Touch_PWM). Tabs (old name):
  **Dashboard** (Home: RPM, presets, manual speed, FAN OFF, Display Mode, Air, Presence & Light,
  LCD + brightness, LEDs, Notes), **Fan Control** (Fan Settings), **Home Assistant** (MQTT, was
  on Config), **Network** (WiFi), **System** (Status, Firmware Information, Display & Interface
  + Save, Firmware update (OTA), Clock + Peripheral Power + Save, Web Login, Factory Reset;
  from Config and OTA). Old docs and commits use the old names. The System Status card became
  the sidebar lights (WiFi / MQTT / Fan controller; its rows are System → Status). The settings
  form is three forms (Home Assistant, System x2) that all call `saveConfig()`, which posts every
  field of all three (`POST /api/config` needs them all). Top bar: page title, the board's
  clock (`/api/status` `clock`, as the LCD shows it; absent until NTP), login state and a
  Log in / Log out button (`<dialog>`). URL `#hash` keeps the tab. Phones (<= 760 px): sidebar
  becomes a top strip; on the Dashboard the LCD card's two parts (`#lcd-viewer`,
  `#lcd-brightness`) become separate cards (`display: contents`): picture first, Brightness
  between Presence & Light and LEDs (user). Colours: the standard variables, Navy & gold only.
- LCD swiping (verified 2026-10-07 by the user: "That did it"). Measured with a temporary log:
  still, ~185 `loop()` passes/s and `lv_task_handler()` 1-2 ms; while swiping, 34-160 passes/s,
  LVGL's clock at 20-80 % of real time (it got `lv_tick_inc(5)` per pass) and single redraws of
  105-240 ms. Fixes: (1) `LV_TICK_CUSTOM` with `millis()` in `lv_conf.h` (no `lv_tick_inc`);
  (2) the RPM arc is display only (not clickable, drag handle hidden), so swipes that start on
  the ring change page; segment taps count out to the screen edge (`SEG_TOUCH_R_OUT` 120);
  (3) two 40-line draw buffers (`DRAW_LINES`, internal DMA RAM, ~19 KB each; fallback: one
  10-line buffer) and `pushImageDMA` in `display_flush()` with the write left open, so LVGL
  draws the next part while the last one is sent. After (3) idle redraws stay 1-2 ms. Not
  done: a faster slide animation (only if wanted).
- Restart (verified 2026-10-07 by the user: LCD, web, HA): `power_request_restart(from)`
  (`power.h`, `main.cpp`) restarts ~0.5 s later from `loop()`; nothing saved or lost, the fan
  starts stopped. LCD: System page (left-most, `create_system_page()`: firmware version,
  uptime, Restart held `RESTART_HOLD_MS` 2 s, fills red while held). Web: System tab "Restart"
  card above Factory Reset, `POST /api/restart` (login; restarts after the reply). HA: button
  "Restart" (`device_class` restart, `<base>/restart/set`, payload PRESS), ignored in the first
  30 s after boot so a retained message can't make it restart over and over.
- Photo eyes, Dragon 2-11 (verified on the board 2026-09-27 by the user: Dragons 3-10 all
  fine; Dragon 11 added from `23-4.png` / `24-4.png`, "works great"; `docs/PHOTO_EYES.md`):
  the artist's open/shut pairs (`assets/eye_art/`) animated by `photo_eye.cpp`: iris moves
  over the socket, slit pupil reacts "to light" (wide when shut, narrows on peeks, snaps narrow
  on stir/screensaver start, drifts and flinches awake), cornea highlights fixed, lids
  blink/sleep through the shut picture; same motion/blink/sleep/stir/glance as the Uncanny
  styles (`dragon_eye.cpp` dispatches on `EyeStyle::photo`). Data from `tools/photo_eye.py`
  (Pillow + numpy) and a hand-measured `eye.json` per eye; ~200-240 KB flash each plus a 77 KB
  PSRAM iris table built by `photo_eye_prepare()` when one is selected. Firmware 85.5 % of the
  app slot with Dragon 11 (room for about 4 more; removing a style's block in `eye_styles.cpp`
  frees its flash). `/api/config` lists 19 styles (~1.9 KB of its 3 KB JSON document).
  Not recorded: fps per photo eye (`[EYE] n fps`).
- NTP (verified 2026-09-28: the LCD clock and the Notes "last saved" time show the correct
  local time): background SNTP started when WiFi STA connects, re-syncs every 60 min, local
  time per configured zone (`configTzTime`; plain `configTime` would reset TZ to UTC).
- Display modes as radio buttons (2026-09-26): one `mode_request` (`power_request_mode()`,
  `PowerMode` in `power.h`) replaces the separate standby/screensaver requests; knob, touch,
  web (`POST /api/mode` active|screensaver|standby) and HA all go through it, so exactly one
  of Active / Screensaver / Standby is on and any can be picked from any (Standby →
  Screensaver works; HA Screensaver ON now leaves standby). Web Home tab "Display Mode" card:
  three buttons, the current one gold. Not yet confirmed by the user on the web/HA side.
- Standby prompt (verified 2026-09-26): after `config.display.standbyAfterMin` minutes of
  screensaver (default 10, 0-1440, 0 = never; counted from `saver_since`), the eye closes and
  a full-screen LVGL overlay asks "Keep the fan running?" (`ui_prompt_*` in `ui.cpp`). Keep
  running / Standby buttons: tap, or knob turn to choose + press. Countdown
  `standbyPromptSec` (default 30, 5-300); no answer → standby. Keep running → back to the
  screensaver, countdown restarts (changed after user review; first version went to Main).
  Fan target already 0 → straight to standby, no prompt. A mode chosen on the web/HA answers
  it. `/api/status` power_mode "Active (standby prompt)" while it shows.
- Standby stops the fan first (fixed 2026-09-26): `set_standby()` calls `fan_stop_now()`,
  which writes Fan Setting 0 to the EMC2101 before the external power is cut. Before, the
  target became 0 but the write only happened on the next `fan_update()`, after the chip was
  already marked unpowered, so the fan kept running. Note: if a future wiring cuts the
  EMC2101's power but not the fan's 12 V, the PWM line floats high and a 4-wire fan runs at
  100 %: switch the fan's 12 V too (or keep the EMC2101 powered).
- `include/ui.h` standby LVGL screen (grey clock) still exists but is no longer shown.
- Peripheral power switch (verified): GPIO 4 drives a transistor that switches **only the 12 V
  rail** (the fan). The EMC2101 and the air sensors are on the MCU's always-on 3.3 V (user
  2026-10-06; earlier notes here said they were switched). ON while awake, OFF in standby; Fan Off only sets
  target/PWM 0. ON level configurable (`config.power.activeHigh`, Config tab, default HIGH;
  applied on save). Pin undriven until config loads at boot, so hardware needs a pull holding
  the switch OFF (pull-down if active-HIGH, pull-up if active-LOW). The EMC2101 is probed
  50 ms after the rail comes on, at boot and on every wake, and marked absent in standby (the
  firmware's choice; it stays powered).
- MQTT / Home Assistant (verified with HA + Mosquitto add-on at 192.168.10.85, login required):
  `mqtt.cpp` runs PubSubClient in its own task (core 0) so blocking connects never stall loop();
  retries every 15 s; reconnects after Config save. Topics `<topicPrefix>/<chipId>/...`
  (`wifi_fan_knob/24C55D/`): `speed`, `speed/set`, `standby`, `standby/set`, `brightness`,
  `brightness/set`, `screensaver`, `screensaver/set`, `running`, `rssi`, `uptime`, `ip`, `status` (LWT
  online/offline, retained). Discovery (retained, `homeassistant/<comp>/
  wifi_fan_knob_<chipId>/<obj>/config`): number Fan Speed, switch Standby, number LCD
  Brightness (10-100 %, applied + saved like the web slider), switch Screensaver (start /
  dismiss, ignored in standby; was a binary_sensor briefly, whose config is cleared on each
  connect), binary_sensor Fan Running, sensors WiFi Signal + Uptime + IP Address (diagnostic). State retained, on
  change; rssi/uptime/ip every 60 s. Brightness/Screensaver added 2026-09-24, verified in HA.
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
  valid), one-line status box (`[AP ]IP:port` / `WiFi lost`; now on the Settings page). It
  flashes red via `ui_set_attention()` when a saved network is configured but not connected
  (the Main clock flashes with it). Colours and layout since changed: see Theme + layout.
- SPIFFS mount + config defaults written
- WiFi AP mode (`WiFi-Fan-Knob-XXXXXX` / `12345678`)
- Webserver serves `index.html` at `http://192.168.4.1:8080`
- Config tab: loads current settings, validates, saves (MQTT password never sent to browser;
  blank = keep). Brightness (PWM backlight) and time zone apply
  at boot and immediately on save via `applyDisplaySettings()` (verified on hardware).
- Fan drive (verified 2026-09-25, EMC2101 at 0x4C on I2C 38/39, Noctua 3000 RPM): `fan_init()`
  overrides Adafruit's `begin()` (which picks the 1.4 kHz base clock, ~23 Hz PWM; its 100 % start
  was changed to 0 % in our copy of the library on 2026-09-26, because it spun the fan briefly at
  every boot and wake from standby):
  360 kHz base, PWM_F 15 → 12.0 kHz, then Fan Setting 0. `fan_update()` (every loop) maps the
  target RPM linearly between Config Min/Max PWM (0-255 duty, kept in duty units so they
  survive a PWM_F change) to a Fan Setting 0-30, written straight to register 0x4C only when
  it changes; 0 RPM = setting 0 (fan stops). Tach read once a second → `fan_get_rpm()`,
  `fan_rpm` in `/api/status`, and `[FAN] n RPM measured` in the log every 5 s. Starting from
  stopped is left to the chip's spin-up (Fan Spin Up register default: 100 % for up to 3.2 s,
  ended early by the tach).
- Measured RPM shown (verified 2026-09-25): LCD caption "RPM (now N)" under the big target
  number while the fan runs (`ui_update()`), web Home tab "Measured: N RPM", Home Assistant
  sensor "Fan RPM" (`<base>/rpm`, state class measurement; published on a 30 RPM change, to/from
  0, or every 10 s while it moves).
- Fan profiles + Auto Configure (verified 2026-09-25 on an NF-P12 and an NF-A20): Config tab
  "Fan Profiles" card. Up to 5 profiles in `/fans.json` (separate from `config.json`; survive
  firmware updates). Each: name, `rpm[31]` measured at every Fan Setting, stall setting, max
  RPM, presets. Activating one copies its max RPM + presets into `config.fan` (which the knob,
  LCD, web and HA already use) and reconnects MQTT so HA's Fan Speed max follows; saving the
  Config tab copies them back into the active profile (`fan_profiles_sync_from_config()`), so
  presets are per fan. With a profile active, `setting_for()` picks the running setting whose
  measured RPM is closest to the target; with none, the old linear Min/Max PWM estimate.
  Auto Configure (`fan_autoconfig_start()`, web button, ~2 min): Fan Setting 30, then down one
  step at a time to 0 or until the fan stops; runs a step per `fan_update()` call (loop keeps
  going). New profile's max = top speed rounded down to the RPM step; presets 25/50/75/100 %
  of it, or, when 25 % is below the slowest held speed (e.g. a fan that never stops), spread
  evenly from that slowest speed to the top. LCD caption "Setup NN% (RPM)"; screensaver
  blocked, knob turns ignored, knob press or web Cancel cancels; standby cancels. Fails with a
  message if there is no RPM at full speed (nothing saved; fan returns to its target).
  Measured: NF-P12 1626 → 149 RPM, stops at setting 1, presets 400/800/1200/1600. NF-A20
  1011 → 421 RPM and **never stops** (still 421 RPM at 0 %), so Off can't stop it; only
  standby (external power off) does. Tuned after that run (re-run and tested by the user
  2026-09-27, then the NF-A20 profile was deleted: not for this project): each step
  now waits until two tach reads 500 ms apart agree within 1 % (min 2 s, max 10 s; full speed
  min 3 s, max 20 s) because the NF-A20 was still speeding up at the fixed 6 s full-speed wait;
  and the even-spread preset rule (it gave 500/500/800/1000 before).

### 🔧 Implemented, not yet verified
- (none)

### ⬜ Not started
- (Done) Noctua industrial 3000 RPM fan: first Auto Configure failed ("no RPM at full speed") on
  the bench supply; user rewired its power 2026-09-27. Profiles on the board now: "Noctua 140mm -
  1" (the industrial fan: top 2981 RPM, slowest 361, max 2900; active) and "Noctua 120mm - 1"
  (the NF-P12: top 1626, slowest 149).
  **2026-10-06: the 140 mm industrial fan died** (12 V at its connector, no spin at any setting;
  same day the TACH pull-up was found open). Don't reconnect it to the board. The NF-P12 is
  fitted and its profile active; it measured ~1415 RPM at the 1600 setting (1603 on 09-25)
  with 12.43 V at the fan at full speed, so the user will re-run Auto Configure.
- Manual calibration mode (knob steps one Fan Setting, press to accept): superseded by Auto
  Configure unless the user asks for it.

### Known quirks
- **USB serial stall (fixed 2026-09-28, verified)**: with COM13 held open but not read (closed
  serial monitor, UPS software), each print retried up to ~2 s and froze `loop()`.
  `Serial.setTxTimeoutMs(1)` after `Serial.begin()` drops output instead. **Not 0 on core
  3.0.4**: `HWCDC::write()` counts retries down from the timeout, 0 wraps to ~4 billion, and
  the first full buffer (MQTT discovery burst at boot) hung the board, off the network too.
  Opening the port from a script resets the board unless DTR and RTS are set False first.
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
- **Boot panic in `attachInterrupt()` (FIXED 2026-09-24)** — the firmware no longer uses GPIO
  interrupts; see "Resolved: ipc1 boot panic" below. Don't reintroduce `attachInterrupt()`
  (or anything that installs the GPIO ISR service) without reading it.
- `config.system.deepSleepEnabled` name predates the light-sleep decision; not renamed
  (would change the config JSON format).

---

## Resolved: ipc1 boot panic (fixed 2026-09-24)

**Fix**: the knob no longer uses GPIO interrupts. A/B are decoded by the PCNT hardware counter
(`init_encoder()` / `encoder_read_detents()` in `main.cpp`: x4 quadrature, 4 counts per detent,
1 us glitch filter, no event callbacks so no interrupt is allocated); the button is polled with
a 20 ms debounce (`poll_button()`). Nothing installs the GPIO ISR service now, so the ipc1 call
that overflowed never runs. Verified: 60 of 60 boots clean; knob direction, one step per click,
short/long press and fast spinning checked by hand. Caveat: the "before" run on the same day was
also 0 of 60 (the 4 of 40 were during flash-and-reset testing), so the evidence for the fix is
that the only crash path is gone, not a measured rate. The original analysis follows.

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

**Candidate fixes considered** (the PCNT change above replaced all of them):
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
| UI Layout | 5 tabs (Home/WiFi/Fan Settings/Config/OTA) | Mobile-responsive |
| HA Integration | MQTT Auto-Discovery | Zero-config |
| Fan Speed | 100 RPM steps, 0-2500 RPM | Noctua fan range |
| Standby | External power off + eye at 15 fps | Deep sleep can't animate; light sleep not needed for now |
| Time Sync | SNTP background, 60 min interval | No blocking in loop |
| OTA | Web form upload | Local, no cloud |

---

## Next Steps

1. ✅ Done 2026-09-25: `fan_control.cpp` EMC2101 PWM + tach (see Working).
   **PWM frequency (decided 2026-09-25): PWM_F = 15 (0x0F), divider 1 → 12.0 kHz, 30 speed
   steps of 3.3 % (~100 RPM each on the 3000 RPM fan).** Keep it one setting so it can be
   changed after bench testing. Datasheet (rev 2.54, App. A): steps = 2 × PWM_F, frequency =
   360 kHz / (2 × PWM_F × PWM_D); a Fan Setting above 2 × PWM_F gives 100 %. So usable Fan
   Settings are 0-30, and the Adafruit `setDutyCycle()` (maps 0-100 % to 0-63) must not be
   used; write the Fan Setting register directly. Trade-off: 12 kHz is below Noctua's 21-28
   kHz spec (22.5 kHz would give only 16 steps, ~190 RPM each). User to check for whine and
   smooth response; fallbacks are PWM_F 16 (11.25 kHz, 32 steps), PWM_F 8 (22.5 kHz, 16
   steps) or PWM from an ESP32 pin (25 kHz, fine steps; EMC2101 reads the tach only).
   Fan: Noctua 140 mm Chromax 3000 RPM, 4-wire; starts at about 6 % duty at 25 kHz.
2. ✅ Done 2026-09-25: measured RPM on the LCD, web Home tab and Home Assistant.
3. ✅ Done 2026-09-25 as **Auto Configure + fan profiles** (see Working). Original design,
   kept for reference: a **Calibrate** button in the Config tab's fan
   section. The LCD shows a calibration screen with the raw Fan Setting and the measured RPM
   (also live on the web page). Standby, screensaver and double-tap are blocked meanwhile.
   The knob moves one raw step per click. Min: start from stopped, raise until the fan
   reliably starts, press to accept. Max: raise until the RPM stops climbing, press to
   accept; the RPM there can fill in Fan max RPM. Long press or web Cancel keeps the old
   values; leaving always restores the previous fan setting. Then a second button,
   **Auto Sweep**: step through every setting, let it settle, record the RPM, and store the
   table so a target RPM maps to the right setting. Build manual first, then the sweep.
4. Field testing, including the industrial fans once they have power.

### Later (user notes)
- **Theme picker (may be revisited, user 2026-10-07):** the page has Navy & gold only. The
  colours are already the standard variables, so adding ETH_Touch_PWM's picker means its
  System "Appearance" panel, the preset blocks and a saved choice on the board.
- **Air quality sensors + Auto mode** (sensors fitted and read 2026-10-06, see Verified; Auto mode not started): **SGP41** (VOC + NOx) with an **SHT41**
  (temperature + humidity) on the main I2C bus (user 2026-09-27; replaces the BME688 plan of
  2026-09-26). Addresses 0x59 (SGP41) and 0x44 (SHT41): no clash with EMC2101 0x4C or APDS9999
  0x52. Libraries: Sensirion I2C SGP41 + Sensirion Gas Index Algorithm (open source) + an SHT4x
  library. The SHT41's readings are passed to every SGP41 measurement (humidity/temperature
  compensation). The Gas Index Algorithm gives a **VOC Index** (1-500; 100 = this room's normal,
  learned over time; fumes push it up) and a NOx Index. Needs a 1 s sample rate; after start the
  first readings take about a minute and the baseline keeps learning for hours. Works the same
  whether the fan stays on the EMC2101 or moves to direct PWM (Auto only sets the target RPM).
  - LCD: a 6th Main segment, **green "Auto"**, right of Max (6 x 40 deg instead of 5 x 48;
    see DISPLAY_GUIDE "Adding or removing a segment"); lit green while Auto is on. Knob or any
    other segment = manual override (Auto off). Web + HA: Auto switch; VOC Index, NOx Index,
    temperature and humidity as HA sensors (history graphs help pick thresholds).
  - Step 1, binary: VOC Index above an on-threshold -> fan High; below a lower off-threshold ->
    off (e.g. on > 150, off < 120); plus a minimum run time (e.g. 2 min after clearing) so it
    doesn't flap. Ignore the index until the SGP41 has settled.
  - **Restarts and room moves (decided 2026-10-06, option A):** the Gas Index Algorithm's
    learned state is in RAM only, so every restart (power cycle, reset, firmware flash) starts
    it learning from scratch; after a room move it re-learns by itself (learning window ~12 h),
    and a restart is the quick "recalibrate". So Auto mode ignores the VOC Index for about the
    first hour after a restart (exact time to set from real readings). Not done: saving the
    learned state (Sensirion's get/set states) so restarts keep it; that would need a
    "Recalibrate" button for room moves. Revisit only if restarts upset Auto mode in practice.
  - Step 2, later: fan speed scaled from the VOC Index (Low..Max), smoothed.
  - Placement matters: in the fan's airflow the readings depend on the fan running; near the
    work it reads the air the user breathes.
  - With the Auto segment the RPM arc can reach lower again (towards a 270 deg sweep): the IP
    box moved to Settings (TODO.md item, do it together with the Auto button).
- **Presence: AMG8833 thermal camera ordered (user decision 2026-10-06).** Chosen over ST's
  STHS34PF80 (single IR pixel with on-chip presence flags: less code, but it can't tell a hot
  iron or hot-air gun from a person, and the iron can't be kept out of its view). The AMG8833's
  8x8 picture lets the firmware ignore a small very hot spot and look for a person-sized warm
  area. I2C 0x69 (Adafruit default; 0x68 alt), no clash. Before the wake rule: test away /
  sitting / leaning in / iron hot with nobody there / a long still sit.
- **Presence sensor (APDS-9999 fitted 2026-10-06; its proximity doesn't reach a seated user even at full power, see Verified)**: APDS9999 (proximity, lux,
  colour), I2C 0x52. Its interrupt pin is optional (polling a few times a second is enough), so
  no GPIO needs reserving now. **User's rule: presence for longer than X seconds wakes the
  screen (leaves the screensaver/standby as a wake) AND starts the SGP41 settle time**, so the
  air sensor is ready when work starts (X settable). Other ideas: the eye looks up at the user;
  no presence for a while -> screensaver sooner; lux -> automatic LCD brightness.
- **Short term, after the FPC breakout (GPIO 4 / 12) arrives: replace the EMC2101 with
  "real" 25 kHz PWM + tach on the UART0 connector** (user, 2026-09-26). Serial is on native
  USB (`ARDUINO_USB_CDC_ON_BOOT=1`), so both UART connectors are free GPIOs. UART0 is almost
  certainly GPIO 43 (TX) / 44 (RX); the second UART connector's pins are not documented by
  Elecrow (confirm with the schematic or a pin-toggle test build). Plan: LEDC PWM at 25 kHz
  (fine resolution) and tach via MCPWM capture or a spare PCNT unit (4 units; the knob uses
  one). GPIO 43 prints the ROM boot log at power-up, so use it for the tach **input** and
  GPIO 44 for the PWM **output** (else the fan blips at every boot). 3.3 V logic: tach needs a
  pull-up to 3.3 V; PWM via the 5 V level shifter if a fan needs it. Auto Configure / profiles
  stay the same idea with finer steps.
- **Eye upgrades** — done: native 240x240, 10 styles selectable on the web, 15 fps in
  standby, sleeping eye in standby, standby brightness setting, wake gestures. Declined:
  light sleep between frames (small saving with WiFi on; revisit only for battery power). Left:
  - **Eyelid images** — done differently (2026-09-26): the artist delivered open/shut pairs,
    which became the photo eyes (Dragon 2-10). The Uncanny Eyes styles still draw black lids.
  - **Standalone eye project** (later, after M4 Eyes) — break out all dragon eye code, display config, and setup steps
  into a semi-universal project that works with any LovyanGFX-compatible display. Document the
  GC9A01 example and how to adapt it to other boards.
- **Adafruit "M4 Eyes" (user wants this, after the fan hardware is done)** — eyes with art
  drawn at 240 px (Adafruit_Learning_System/M4_Eyes), so more detail than the upscaled 128 px
  Uncanny Eyes. Different engine (runtime eyeball/lid rendering, per-eye config + images), so a
  real port, not a data swap. All 10 current styles verified and liked (2026-09-24).

---

## Open Questions

1. Noctua fan min/max PWM: initial guess 50-200. Decided 2026-09-24: leave until the fan is
   connected, then calibrate (see Next Steps 3). Kept in 0-255 duty units; the firmware
   converts to Fan Settings. Set to Min 16 (≈6 %, where the fan starts) / Max 255 for now.
2. ✅ 12 kHz PWM: no audible whine (user, 2026-09-25).

Decided: Home Assistant fan speed stays in RPM (2026-09-24). The eye is drawn procedurally
with LovyanGFX (Uncanny Eyes), not pre-rendered frames or LVGL.

## Possible future mods

Not issues now; kept so they can be looked at again later (user, 2026-09-27).

1. **Off for fans that never stop.** Some fans keep turning at 0 % PWM (the NF-A20 ran at
   421 RPM), so Off can't stop them. Both fans in use now stop at 0 % (profiles show
   `stops: true`). Option if needed: Off also cuts the GPIO 4 external power (which also
   switches off anything else on that rail).
2. **Flash space.** The app slot is 6.25 MB (6,553,600 bytes). Firmware was 5.6 MB = 85.5 %
   on 2026-09-27 (ten photo eyes; each adds ~225 KB, so about 4 more fit). **After any major
   change, check the `Flash:` line PlatformIO prints and tell the user the new figure,
   especially past ~90 %.** Ways to make room: remove unused eye styles in
   `src/eye_styles.cpp` (photo or Uncanny), or a custom partition table with a bigger app
   slot (flashed over USB; moves SPIFFS, so config and fan profiles are reset).
3. **MQTT status dot on the LCD** (from `HANDOFF_FROM_ETH_Touch_PWM.md`; parked by the user
   2026-09-28). ETH_Touch_PWM shows one in its LCD title bar. Here the Settings page already
   shows MQTT as text, and the round Main screen has little room.

---

## Quick Reference

- Webserver: `http://192.168.4.1:8080` in AP mode (viewing needs no login; changes need the web login)
- MQTT broker default: `192.168.10.50:1883` (configurable)
- WiFi AP: `WiFi-Fan-Knob-XXXXXX` / `12345678`
- Serial: 115200 baud
- Config: `/config.json` on SPIFFS, auto-created on boot if missing

---

## Log

One report per session day in `docs/Status_Reports/` (table above); every change is in the git
log, and finished to-do items with dates and commits are under Done in `docs/TODO.md`.
