# WiFi Fan Knob - Project Status Report 05
**Date**: September 26, 2026
**Session**: Fourth session day: web page and LCD to-do items (on the board), photo eyes (cloud session, from the phone)
**Status**: Final for the day. The web/LCD items are flashed and checked on the board. The photo eyes (Dragon 2-10) are pushed but **not yet built or run on the board**.

For the technical details of each feature, see `../CLAUDE.md`. For changing the screen's look, see `../DISPLAY_GUIDE.md`. For the photo eyes, see `../PHOTO_EYES.md`.

---

## What changed today

| Area | Result |
|---|---|
| **Display modes** | Active / Screensaver / Standby work like radio buttons from the knob, touch, web (new Display Mode card on the Home tab) and Home Assistant; any mode can be picked from any other |
| **Standby prompt** | After the screensaver has run for "Standby after screensaver" minutes (default 10), the LCD asks "Keep the fan running?". No answer in 30 s means standby. Standby now also stops the fan before cutting external power |
| **No fan burst at boot** | The fan no longer spins up briefly at every boot or wake (our copy of the Adafruit EMC2101 library starts at 0 %) |
| **Web page** | Preset buttons act as radio buttons; the speed slider works live (Apply Speed removed); new **Fan Settings** tab between WiFi and Config; header shows the WiFi network name; time zone and format back in a "Clock" card |
| **Device name** | `http://fanknob.local` works (mDNS name set on the WiFi tab, default "fanknob"); port 80 now forwards to 8080, so no port to type |
| **LCD** | Settings page moved left of Main (knob left from a stopped Main opens it). New Settings switch pauses the screensaver until the fan stops, standby, restart or the pause limit (web Config, default 120 min); a closed-eye icon by the clock shows while paused |
| **Photo eyes** | Your artist's pictures are now 9 new eye styles, **Dragon 2 to Dragon 10**, on the web list after Dragon. The iris moves, the slit pupil widens and narrows as if reacting to light, reflections stay put, and the lids blink and sleep through the shut picture. Same sleep, twitch, peek, stir and glance behaviour as the other styles |
| **Eye pictures matched** | Pairs in `assets/eye_art`: Dragon 2 = 5-1/6, 3 = 01A/01B, 4 = 02A/02B, 5 = 7/8, 6 = 9/9B, 7 = 15/16, 8 = 17/18, 9 = 19/20, 10 = 21/22 |
| **Eye tools** | `tools/photo_eye.py` turns a pair plus a small settings file (`eye.json`) into firmware data and a preview picture, so new eyes can be added the same way |
| **Docs** | New `PHOTO_EYES.md` (how the photo eyes work, adding and removing one, flash budget, artist spec). `CLAUDE.md`, `DISPLAY_GUIDE.md`, root `CLAUDE.md` + `docs/TODO.md` (your to-do list), BME688 / Auto mode plan |

## Checked today

- On the board: web page changes, LCD page order and screensaver pause, Clock card, port 80 redirect, `fanknob.local` from the PC, standby prompt.
- Photo eyes, on the PC only: the code compiles, and the renderer's output matches the preview pictures (Dragon 2 exactly the same before and after the space-saving rework).

## Problems found and fixed today

- **Photo eyes too big for the firmware:** nine eyes at ~375 KB each would have overflowed the 6.25 MB app slot. Now ~200-240 KB each (about 2 MB for all nine) with no loss of picture quality.
- **Large irises cut off square** (Dragon 7): the drawing area was too small; enlarged for all eyes.
- **Dark patch when an eye looked sideways** (pale-sclera eyes 6, 7, 10): the area behind the iris now continues the sclera.
- **Slanted pupils** (Dragons 7, 9, 10): the pupil can now be tilted to match the art.

## Not yet checked on the board

- **Photo eyes, Dragon 2-10:** build size (estimate ~5.5 MB of 6.25 MB), frame rate for each (`[EYE] n fps` in the serial log), alignment with the pictures, pupil and blink look.
- **Display Mode card** from the web and Home Assistant side.

## Known rough edges (photo eyes)

- Dragon 8's cyan reflection and Dragon 3's grey one are only partly held still; the rest moves with the iris.
- Dragon 5's reflection shows faint "ai" lettering; it is in the artist's picture.
- On a few eyes the lid corner shows a slight straight edge while the lid is mostly closed.

## Decisions made

- Photo eye art stays in `assets/eye_art/` only; each eye's `eye.json` points at its pair.
- Flash budget: room for about three more photo eyes. You're fine with deleting some if needed; removing a style's block in `src/eye_styles.cpp` frees its space.

## Waiting on you

*Updated 2026-09-27 from your notes:*

- ~~Build and flash, then cycle through Dragons 2-10~~ **Done:** flashed last night; all eight new eyes (Dragons 3-10) look fine on the board.
- ~~One more eye picture pair from the artist~~ **Received** (`23-4.png` / `24-4.png`); it became **Dragon 11** on 2026-09-27.
- ~~Display Mode card check~~ **Done:** in place and looks good.
- ~~Power supply for the industrial fans~~ **Done:** the industrial fan is rewired and works great.
- ~~NF-A20 Auto Configure re-run~~ **Done:** re-run and tested, then deleted as not for this project.
- **Still open:** GPIO 4 pull-down resistor (waits on the FPC breakout for GPIO 4 access).

## Next steps

1. ~~Photo eyes on the board; add the last pair (Dragon 11)~~ Done 2026-09-27.
2. ~~BME688~~ **SGP41 + SHT41** air sensors and Auto mode when they arrive (plan in `CLAUDE.md`; changed 2026-09-27).
3. Move PWM and tach to the UART0 connector after the FPC breakout arrives.
4. Industrial fans: power, Auto Configure, field testing.
