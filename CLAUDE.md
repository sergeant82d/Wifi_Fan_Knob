# WiFi Fan Knob

ESP32-S3 fan controller for a solder fume extractor, on the Elecrow CrowPanel 1.28" round
rotary display: knob and touch LCD with an animated eye screensaver, fan speed through an
EMC2101 (fan profiles, Auto Configure), air sensors (SHT41 + SGP41), light sensor
(APDS-9999), RGB LED ring, a web page (login, OTA) and Home Assistant over MQTT (discovery).

**Read `docs/PROJECT_HISTORY.md` first:** current status, what each part does and why, pins,
gotchas, decisions and plans. Open items and the user's notes: `docs/TODO.md`. Changing the
LCD: `docs/DISPLAY_GUIDE.md` (written for the user; keep it current with LCD changes). Photo
eyes: `docs/PHOTO_EYES.md`. General rules (working principles, testing, git, TODO format,
status reports, web standard, Windows/PlatformIO build) are in the user's global
`~/.claude/CLAUDE.md`.

## Hardware

- Elecrow CrowPanel 1.28" rotary display (ESP32-S3, 16 MB flash, 8 MB PSRAM, GC9A01 240x240,
  CST816D touch, knob, 5 WS2812 LEDs) on COM13, native USB. Pins: `docs/PROJECT_HISTORY.md`
  (Pins) and the top of `src/main.cpp`.
- Main I2C (GPIO 38/39): SHT41 0x44, EMC2101 0x4C, APDS-9999 0x52, SGP41 0x59 (and the
  AMG8833 at 0x69 when it arrives), all on always-on 3.3 V. GPIO 4 switches only the 12 V
  rail (the fan): on while awake, off in standby. The boot log lists the bus (`[I2C] Found:`).
- Fan: Noctua NF-P12 via the EMC2101 (12 kHz PWM, 30 steps); its TACH needs the 10 kOhm
  pull-up to 3.3 V (replaced 2026-10-06).

## Web page

- `web/index.html` (HTML + CSS + JS in one file) is compiled into the firmware;
  `src/webserver.cpp` serves it and the JSON API on port 8080 (port 80 redirects).
- It follows the all-projects standard `D:\GitHub\WEB_STYLE.md` (left sidebar; tabs Dashboard,
  Fan Control, Home Assistant, Network, System). Navy & gold only, no theme picker.
- After editing the script, extract the `<script>` block and run `node --check`. Keep CRLF.
- Board: `http://192.168.10.102:8080`. `/api/status`, `/api/config`, `/api/fans` read without
  a login; every change needs the user's web login. Every web control is also in Home Assistant.

## Build

- `~/.platformio/penv/Scripts/pio.exe run -t upload --upload-port COM13` from PowerShell.
  Uploads sometimes fail with "port busy / access denied" (UPS software grabs the port): retry.
  Close your own serial reader first; opening COM13 resets the board unless DTR and RTS are set
  low before opening.
- Platform pinned to pioarduino 51.03.04 = Arduino core 3.0.4 (`platformio.ini`); the core
  folder is shared with ETH_Touch_PWM (3.3.11), so switching projects re-downloads it.
- Settings are JSON in SPIFFS (`/config.json`, `/fans.json`, `/notes.json`); new fields load
  with defaults, so older files keep working. Never run `uploadfs` (it wipes them).
- **Flash space:** after a major change, tell the user the build's `Flash:` figure (86.6 % on
  2026-10-07; warn past ~90 %). Ways to free space: `docs/PROJECT_HISTORY.md`, "Possible future mods".

## Rules

- The WiFi, MQTT and web passwords are entered on the web page and never sent back to it.
  Full flash backups contain them: keep backups outside the repo
  (`D:\GitHub\VSCodeProjects\Wifi_Bench_Fan\Wifi_Fan_Knob_backups\`).
- Status reports: `docs/Status_Reports/STATUS_REPORT_NN.md`, only when the user asks (usually
  for the previous day, before a new day's work).

## Success criteria

1. Builds with no new warnings; flash below ~90 % of the app slot.
2. On the board, after a change that touches it: fan (presets, slider, knob, measured RPM), LCD
   pages, knob and touch, eye in screensaver and standby, web page (every tab, login, OTA),
   Home Assistant entities both ways, sensors, LEDs.
3. Settings survive a firmware update (older settings files load with defaults for new fields).
