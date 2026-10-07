# WiFi Fan Knob - Project Status Report 08
**Date**: October 7, 2026
**Session**: Seventh session day: Auto brightness, the all-projects standards (TODO format, web page), the left-sidebar web page, Restart, the docs split, and smooth LCD swiping
**Status**: Final for the day. Everything below is flashed, checked on the board by you, committed and pushed. The board runs `3d7311c`.

For the details, see `../PROJECT_HISTORY.md` (until today `docs/CLAUDE.md`). Your list: `../TODO.md`.

---

## What changed today

| Area | Result |
|---|---|
| **Auto brightness** | The LCD follows the room light from the APDS-9999, between a Dark level (default 20 %) and a Bright level (default 100 %), fading so a passing shadow doesn't flicker it. Sped up at your request (5 s average, 50 ms steps). The LED ring follows the same light, LED brightness 10 to 90. Any manual brightness change turns Auto off. Web: LCD card; HA: "LCD Auto Brightness", "LCD Auto Min", "LCD Auto Max" (`429c112`) |
| **LEDs in standby** | The LEDs now stay on in standby (your request) (`429c112`) |
| **All-projects standards** | `D:\GitHub\WEB_STYLE.md`: the web page standard (left sidebar, tab names and order, colour variables, components, rules). Your global `CLAUDE.md` gained the TODO format, a "Web pages" section and the display/DMA default. Both projects' TODO lists use the same format and the same alphabetical tag list (`74fcb05`, `1dbfccf`; ETH_Touch_PWM `b696eba`, `600f90e`) |
| **Web page** | Left sidebar like ETH_Touch_PWM: tabs Dashboard, Fan Control, Home Assistant, Network, System; lights WiFi / MQTT / Fan controller; top bar with the board's clock and a login pop-up; Navy & gold only. Your layout: OTA below Display & Interface; on phones only the LCD picture first, Brightness as its own card between Presence & Light and LEDs (`942d0a8`) |
| **Restart** | LCD: new System page (left-most; firmware version, uptime, Restart held 2 s). Web: System tab card above Factory Reset. HA: "Restart" button (ignored in the first 30 s after boot) (`388a3e2`) |
| **Docs split** | Short root `CLAUDE.md`; the long guide is now `docs/PROJECT_HISTORY.md` with a current-status section (`c465253`) |
| **LCD swiping** | From "barely usable" to smooth: LVGL reads the real clock; the RPM arc is display only (swipes from the edge change page; segment taps reach the screen edge); two 40-line draw buffers sent by DMA (`3d7311c`) |

## Checked today

- On the board, by you: Auto brightness speed, LEDs following and staying on in standby, the slider turning Auto off; every web tab on PC and phone, login, saves; all three restarts; swiping ("That did it").
- By me on the board: every build and flash; screenshots of every web tab at desktop and phone width; the web LCD copy of Main after the arc change; timing measurements before and after the swipe fixes.

## Measured: why swiping was bad

| | Loop passes per second | LVGL's clock | Longest redraw |
|---|---|---|---|
| Screen still | ~185 | ~95 % of real time | 1-2 ms |
| Swiping, before | 34-160 | 20-80 % of real time | 105-240 ms |

The clock now comes from `millis()`, so it can't fall behind; the DMA buffers let drawing and sending overlap.

## Problems found and fixed today

- **My TODO notice about HA brightness was incomplete:** HA's "LCD Brightness" also shows the manual level while Auto runs, not only the LEDs. The docs now list what shows the live level (web screen slider, LCD) and what shows the set level (HA, web LED slider).
- **The second Save on the System tab** (your 2026-09-27 request) was lost in the first conversion; put back before your testing.

## Decisions made

- One web standard for all projects (`D:\GitHub\WEB_STYLE.md`); standard tab names; no theme picker here for now (may be revisited).
- One TODO format for all projects (Your notes / Open / Done, tags, Done archived after ~2 weeks).
- Displays on DMA-capable chips default to DMA drawing (global `CLAUDE.md`). ETH_Touch_PWM has a `[Decide]` item for it (it uses Adafruit GFX and shares its SPI bus with the SD card, so it's not a straight copy).
- The RPM arc on the LCD only shows the speed; the knob, segments and web page set it.

## Numbers

- Flash: **86.6 %** of the 6.25 MB app slot (86.5 % at the start of the day). About 875 KB free.
- RAM: 35.0 % static; the two draw buffers (~38 KB) come from free internal RAM at boot.
- No new full flash backup today (settings changes were additive; yesterday's backup `full_flash_2026-10-06_before_leds.bin` is outside the repo).

## Waiting on you

- **VOC history:** leave the board running untouched for a day or two so the VOC Index can learn the room (today's flashes restarted it); then pick the Auto mode thresholds from HA.
- **AMG8833** to arrive (presence).
- **FPC breakout** (GPIO 4 pull-down; fan PWM to the UART0 connector).

## Next steps

1. Auto mode from the VOC Index, with the two LCD layout items (lower RPM arc; speed buttons above the horizontal, Off and Auto below).
2. AMG8833: test, then the presence wake rule.
3. Optional: faster LCD slide animation; LED colour from fan speed or air quality; QR page hotspot view check.
