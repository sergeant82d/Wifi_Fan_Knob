# WiFi Fan Knob - Project Status Report 03
**Date**: September 24, 2026
**Session**: Second hardware session (EMC2101 still in transit)
**Status**: **In progress — not final for the day.** Everything below is built, flashed and pushed unless marked otherwise.

For the technical details of each feature, see `CLAUDE.md`. For changing the screen's look, see `DISPLAY_GUIDE.md`.

---

## What changed today (verified on hardware)

| Area | Result |
|---|---|
| **LCD Main page** | Off / Low / Med / High / Max are curved segments inside the RPM arc. The segment for the current speed stays lit (cyan). The arc ends level with the outer segments, clear of the IP box. Smaller RPM number |
| **LCD pages** | The Presets page is gone (the segments replace it); Main and Settings remain. A short knob press opens a menu of the pages. On Settings, tapping empty space returns to Main |
| **Double-tap** | Double-tap on Main stops the fan, or pops up "Fan is not running" |
| **Screensaver** | The eye appears after a set idle time (web Config, default 30 s, 0 = off). The fan keeps running. Touch, knob or a short press dismisses it; a long press still goes to standby. The web Home tab shows "Active (screensaver)" |
| **Eye** | Drawn at the screen's full 240 × 240 (no more 2 × 2 pixel blocks). 10 styles to pick from on the web page: Dragon, Human, Cat, Goat, Owl, Doe, Newt, Nauga, No sclera, Terminator. 37–52 frames per second |
| **Standby** | External power off, eye slowed to 15 frames per second to save power |
| **Web: presets and fan max** | Preset speeds and the fan's max RPM are editable on the Config tab. Presets set their exact value everywhere (web and LCD) |
| **Web: brightness** | The LCD brightness slider is on the Home tab |
| **Web: screensaver button** | Home tab button starts the screensaver, or wakes the display from it |
| **Web: network** | Static IP or DHCP on the WiFi tab. Saving restarts the board, with a notice beside the button |
| **Web: other** | GitHub link on Home, power icon on the FAN OFF button now shows on Android, the page no longer goes stale after a firmware update |
| **Home Assistant** | New IP Address sensor, LCD Brightness slider and Screensaver sensor |
| **Docs** | `DISPLAY_GUIDE.md`: how to change colours, fonts, segments, pages and eye styles |

## Problems found and fixed today

- **Web preset buttons rounded to 100 RPM:** they went through the speed slider, which moves in 100-RPM steps. They now send their exact value.
- **LCD taps on a segment's outer edge set a stepped speed:** the RPM arc's touch zone reached into the segments. It now stops at their edge.
- **Browser kept running an old copy of the page after a firmware update:** the board now tells browsers not to keep a stored copy.
- **One-off WiFi driver crash** on the first boot after a flash (inside Espressif's precompiled WiFi code). Not seen again in 6 restarts or during testing; watching for it.
- **Startup crash (fixed):** about 1 boot in 10 yesterday crashed while the knob's interrupts were set up. The knob now uses the chip's hardware encoder counter instead of pin interrupts, which removes the code path every recorded crash came from. 60 of 60 restarts clean, and the knob checked by hand (direction, one step per click, short and long press, fast spinning). The "before" test today was also 0 of 60, so this shows reliability rather than a measured improvement.

## Decisions made

- Home Assistant fan speed stays in RPM.
- Fan drive limits (min/max PWM) wait until the fan is connected.
- The eye styles stay Adafruit's 128-pixel art, scaled up; Adafruit's "M4 Eyes" (drawn at 240 px) come after the fan hardware.
- Presets are not needed in Home Assistant; an LCD brightness control is on the later list.

## Housekeeping

- Removed the unused NTPClient library.

## Next steps

1. **EMC2101 arrives:** fan drive and tachometer, then measured RPM on the LCD, web page and Home Assistant.
2. Fan calibration and bench testing.
3. Later list (`CLAUDE.md`): eye upgrades, M4 Eyes.
