# WiFi Fan Knob - Project Status Report 06
**Date**: September 28, 2026
**Session**: Fifth session day: the handoff list from the ETH_Touch_PWM project (`../HANDOFF_FROM_ETH_Touch_PWM.md`)
**Status**: Final for the day. Everything below is flashed, checked on the board by you, committed and pushed. The board runs `85d6784` (the later commits are docs only).

For the technical details of each feature, see `../CLAUDE.md`. For changing the screen's look, see `../DISPLAY_GUIDE.md`.

---

## What changed today

| Area | Result |
|---|---|
| **USB serial stall** | A PC holding the USB port open without reading it (closed serial monitor, UPS software) can no longer freeze the knob and LCD: a blocked print now gives up after ~1 ms (`c12d170`) |
| **Build platform pinned** | `platformio.ini` now names the exact platform (core 3.0.4) instead of whichever version was installed last; ETH_Touch_PWM had installed a newer one on this PC (`c12d170`) |
| **QR code page (LCD)** | New page, left of Settings. On your WiFi: a QR code that opens the web page (`http://<IP>/`). In hotspot mode: a QR code that joins the hotspot; tap it for the page link. Off Main, the knob now steps one page at a time (`2420c49`) |
| **LCD on the web page** | New "LCD" card on the Home tab: a copy of the real screen, updated every 2 s, including the eye in screensaver and standby. On phones it's the first card on the Home tab (`610fad2`, `e93a0ab`) |
| **Notes box** | New "Notes" card on the Home tab: kept on the board and shared by every browser, 12 emoji buttons, byte counter (4000 max), "last saved <time> by <user>", warning before leaving with unsaved changes. Saving needs the login (`85d6784`) |
| **Docs** | Handoff file committed and kept up to date; `CLAUDE.md` verified list, file list and "Possible future mods"; `DISPLAY_GUIDE.md` (QR page, page order, knob paging); your global `CLAUDE.md` serial note corrected |

## Checked today

- On the board, by you: serial fix (knob, fan and LCD used with the port held open, no freezes), QR code page and knob paging, LCD card on PC and phone, phone layout, Notes box.
- By me on the board: the LCD copy matches the screen (Main, screensaver eye open and mid-blink, shut standby eye); the standby eye stays at 14 fps while copied; notes can be read without the login and saving without it is refused.
- Moved to verified in `CLAUDE.md`: photo eyes Dragon 2-11 (your check on 2026-09-27) and the NTP clock (right local time on the LCD and in the notes).

## Problems found and fixed today

- **The serial fix as first written froze the board:** `setTxTimeoutMs(0)`, the advice from ETH_Touch_PWM, hangs every print on this project's older core (a counter wraps around). `setTxTimeoutMs(1)` works on every core. Your global `CLAUDE.md` now says so, with the reason.
- **Wrong core after working on ETH_Touch_PWM:** the build had silently picked up the newer platform. Fixed by pinning (above).
- **Reading the serial port reset the board:** my test script now opens the port without toggling the reset lines.
- **Knob didn't reach the QR page from Settings:** off Main the knob now moves one page per turn.

## Not yet checked on the board

- The QR page's **hotspot view** (join-hotspot code and the tap to switch). It needs the board in hotspot mode, so it can wait until that happens.
- Frame rate of each photo eye (`[EYE] n fps`); they look fine, the number just hasn't been recorded.

## Decisions made

- The QR page sits left of Settings (your choice), not right of Main.
- The eye is copied from what's actually drawn, rather than a slideshow of the art images: it's the real screen, costs no extra flash, and covers the Uncanny styles too.
- Notes are stored in their own file (`/notes.json`), so settings changes never touch them.
- Handoff items dropped: Themes and the History tab. The MQTT dot on the LCD is parked in `CLAUDE.md` "Possible future mods" (Settings already shows MQTT).

## Numbers

- Flash: **86.0 %** of the 6.25 MB app slot (85.5 % at the start of the day: +0.1 serial fix, +0.1 QR page, +0.1 LCD view, +0.2 Notes). About 900 KB free, room for about 4 more photo eyes.
- RAM: 35.0 %. The LCD copy uses two 115 KB buffers in PSRAM.
- Full flash backup from this morning, taken before the first flash: `Wifi_Fan_Knob_backups\full_flash_2026-09-28_before_serial_fix.bin` (outside the repo; contains the WiFi and MQTT passwords).

## Waiting on you

- **Still open:** GPIO 4 pull-down resistor (waits on the FPC breakout).
- Hardware on your list: the 5 V switching supply, and the SGP41 + SHT41 sensors.

## Next steps

1. **SGP41 + SHT41** air sensors and Auto mode when they arrive (plan in `CLAUDE.md`), together with the two LCD TODO items (lower RPM arc; speed buttons above the horizontal, Off and Auto below).
2. Move PWM and tach to the UART0 connector after the FPC breakout arrives.
3. Later: presence sensor (APDS9999), Adafruit "M4 Eyes".
