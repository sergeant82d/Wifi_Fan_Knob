# Handoff: features from ETH_Touch_PWM worth adapting here

Written 2026-09-28 by the Claude session working on the sister project ETH_Touch_PWM
(`D:\GitHub\VSCodeProjects\ETH_Touch_PWM`, a two-fan Ethernet controller with a 320x240
LCD). The user asked which of that day's features fit this project. This file is the
answer, for a session working here. Nothing in this repo was changed except adding this file.

**Work items only when the user asks** (this repo's `CLAUDE.md` rules apply: TODO.md,
flash-space report, test on the board, commit when the user confirms). Suggested order:
1, 3, 2, 4.

## Differences to keep in mind when porting

| | ETH_Touch_PWM (source) | Wifi_Fan_Knob (here) |
|---|---|---|
| Core | pioarduino 55.03.311 = Arduino core 3.3.11 | pioarduino 51.x = core 3.0.4 |
| Web server | own `NetworkServer` loop, runs in `loop()` | ESPAsyncWebServer: handlers run on another task, so anything touching LVGL goes through a flag applied in `loop()` (as the mode requests already do) |
| JSON | ArduinoJson 7 (`JsonDocument`) | ArduinoJson 6 (`StaticJsonDocument`) |
| Files | LittleFS | SPIFFS (`/config.json`, `/fans.json`) |
| LCD | Adafruit GFX, drawn directly, no frame buffer | LVGL 8.4 + LovyanGFX; the eye is drawn outside LVGL |
| Port | 80 | 8080 (port 80 redirects) |

Flash: 85.5 % of the 6.25 MB app slot on 2026-09-27. Items 1-4 together should add tens of
KB (one photo eye is ~225 KB). Report the `Flash:` figure after each.

## 1. USB serial stall fix (one line, do first)

`src/main.cpp` calls `Serial.begin(115200)` but not `Serial.setTxTimeoutMs(0)`. On the
ESP32-S3's native USB, when a PC has the port open but isn't reading it (a closed serial
monitor, the UPS software that grabs COM ports), every `Serial.print` retries for up to
~2 s. In ETH_Touch_PWM a dozen prints dropped the MQTT connection. Here `loop()` prints
touch, encoder and `[FAN]` lines, so the knob and LCD could freeze.

Fix: `Serial.setTxTimeoutMs(0);` right after `Serial.begin()`. Output is simply dropped when
nobody reads. Check it exists on core 3.0.4 (HWCDC); it does on 3.3.11.

## 2. LCD view on the web page (real pixels via LVGL snapshot)

ETH_Touch_PWM redraws its LCD layout in a web canvas from `/api/status`, because an Adafruit
GFX screen can't be read back. Here LVGL can snapshot the screen, so the page can show the
real pixels, and it stays right when the LCD layout changes.

Plan:
- `include/lv_conf.h`: `#define LV_USE_SNAPSHOT 1`.
- A 240x240 RGB565 buffer in PSRAM (115,200 bytes, `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`).
  Use `lv_snapshot_take_to_buf()` with that buffer, not `lv_snapshot_take()` (that one
  allocates from LVGL's own small heap).
- LVGL isn't thread-safe: the web handler sets a request flag; `loop()` takes the snapshot
  (`lv_scr_act()`) between `lv_timer_handler()` calls; the handler serves the last one.
  Simplest: `GET /api/lcd` returns the raw RGB565 bytes (little-endian, check
  `LV_COLOR_16_SWAP`) plus a sequence number; the page draws them into a 240x240 canvas with
  `ImageData` and a round clip (the screen is round).
- Refresh every 2 s while the Home tab is visible (115 KB per frame over WiFi is fine at that
  rate; don't poll when the tab is hidden).
- The eye (screensaver/standby) is drawn line by line straight to the panel by
  `dragon_eye.cpp` / `photo_eye.cpp`, outside LVGL, and LVGL's draw buffer is only 10 lines,
  so it can't be captured. While it shows, the card shows text instead, e.g.
  "Screensaver: Dragon 11" / "Standby" (from `power_mode` and the eye style).
- Covers Main, Settings, the Auto Configure screen and the standby prompt.

The ETH version for comparison: `drawLcdView()` in ETH_Touch_PWM's `web/index.html` (canvas
scaled 3x, `.lcd-view` CSS, redraw every 200 ms).

## 3. QR code page on the LCD

ETH_Touch_PWM: tap the gear, a page shows a QR code of `http://<IP>/` (IP, not name.local:
many Android phones can't open .local names); while the hotspot is on, a second QR joins it
(`WIFI:T:WPA;S:<ssid>;P:<password>;;`, with `\ ; , : "` escaped by a backslash). Any tap or
60 s closes it. Source: `drawInfoPage()`, `wifiEscape()` in ETH's `src/display.cpp`.

Here:
- LVGL 8.4 has the widget (`src/extra/libs/qrcode/lv_qrcode.h`): `#define LV_USE_QRCODE 1`
  in `lv_conf.h`, then `lv_qrcode_create(parent, size, dark, light)` + `lv_qrcode_update()`.
  It sizes the code itself (ETH needed its own capacity table for the ricmoo library).
- New page = one builder + one row in the `PAGES` table in `ui.cpp` (keep `MAIN_PAGE` /
  `SETTINGS_PAGE` right). Or put it on Settings; ask the user.
- Round 240 px screen: the inscribed square is ~170 px, so a QR of ~150 px plus a white quiet
  zone fits. Two QRs (hotspot mode) won't fit side by side: show one at a time (hotspot join
  first, tap or knob for the page URL).
- URL: `http://<IP>/` works because port 80 redirects to 8080.
- Update the text on the page when the IP or hotspot state changes (rebuild on show is enough).
- Keep `docs/DISPLAY_GUIDE.md` current.

## 4. Notes box (web Home tab)

ETH_Touch_PWM Dashboard card: a scrollable textarea (170-420 px tall, resizable), a row of 12
emoji buttons that insert at the cursor, a byte counter (4000 max, UTF-8 bytes), "last saved
<time> by <user>", a Save button (login required), and a `beforeunload` warning while there
are unsaved changes. Stored on the board, shared by every browser.

Source in ETH_Touch_PWM:
- `web/index.html`: section `// ---------- Notes (Dashboard) ----------` (`NOTE_EMOJIS`,
  `loadNotes()`, the `#notes-save` handler), CSS `.notes` and `.emoji-bar`, and the Notes
  panel markup on the Dashboard.
- `src/web_server.cpp`: `GET/POST /api/notes`, file `/notes.json` =
  `{"text", "saved", "by"}`, `NOTES_MAX` 4000, body up to ~12 KB (escaped emojis).

Here: SPIFFS `/notes.json` (separate from `/config.json`, so settings changes never touch it),
`require_login()` on the POST, and an ESPAsyncWebServer body handler (the body can arrive in
chunks). The page's CSS uses fixed colours, not ETH's CSS variables: adapt the styles.

## Later, or only if the user wants them

5. **Themes.** ETH's "Navy & gold" preset came from this page. Porting the theme picker (and
   ETH's left-tab layout, from esp32-nut) means restructuring this page's CSS into colour
   variables. Medium job, looks only.
6. **History tab.** ETH logs to an SD card; this board has no SD slot and nothing to log but
   target and measured RPM. Worth it once the SGP41/SHT41 arrive (log to SPIFFS, 3.4 MB).
   ETH's chart code (`drawChart()`, `drawRangeChart()`, canvas, no library) would carry over.
7. **MQTT status dot.** ETH added one to its LCD title bar. The Settings page here already
   shows MQTT as text and the round Main screen has little room; skip unless asked.

## Doesn't apply here

SD health and SD dot, CSV column names, 30-day/records views (no log yet), OTA board check
(only one Knob build), Ethernet/DHCP. The header already shows the WiFi name (2026-09-26).
