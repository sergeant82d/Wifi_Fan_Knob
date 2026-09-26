# WiFi Fan Knob - Project Status Report 04
**Date**: September 25, 2026
**Session**: Third hardware session (EMC2101 fan controller arrived)
**Status**: Final for the day. Everything below is built, flashed and pushed. Items not yet checked on the board are marked.

For the technical details of each feature, see `../CLAUDE.md`. For changing the screen's look, see `../DISPLAY_GUIDE.md`.

---

## What changed today

| Area | Result |
|---|---|
| **Fan control** | The EMC2101 now drives the fan: 12 kHz PWM with 30 speed steps. The knob, LCD buttons, web page and Home Assistant set the real fan speed. No audible whine |
| **Measured RPM** | The fan's actual speed (from its tach wire) shows on the LCD, on the web Home tab ("Measured") and in Home Assistant (new "Fan RPM" sensor with history) |
| **Fan profiles** | Up to 5 saved fans, each with its own measured speed table, max RPM and Low / Med / High / Max presets. Pick the active fan on the web Config tab; rename or delete. Profiles survive firmware updates |
| **Auto Configure** | One button on the Config tab measures the connected fan (about 2 minutes: full speed, then step by step down until it stops), saves a profile and sets its presets to 25 / 50 / 75 / 100 % of its top speed. Speed targets are then accurate. Knob press or web Cancel stops it |
| **LCD look** | New dark blue and gold theme, borrowed from the web page. Main: gold target RPM, light "now 1234" actual RPM underneath, clock at the bottom (flashes red if WiFi is lost). The IP address moved to the Settings page |
| **Auto Configure screen** | A separate gold screen appears by itself during Auto Configure: progress ring, fan name, big percentage, "Press knob to cancel", live RPM, step count. The result stays up for 30 seconds |
| **Wake gestures** | In standby, the first tap stirs the eye (it opens and follows your finger); 4 more taps wake the board. Turning the knob stirs it and it glances that way; pressing the knob wakes at once. Two tap-counting bugs found in the log and fixed |
| **Repo** | Project moved up one folder (no more nested `Wifi_Fan_Knob/Wifi_Fan_Knob`). Status reports in `docs/Status_Reports/`, doc pictures in `docs/images/`, eye artwork in `assets/eye_art/`. README cleaned up |
| **Docs** | `EYE_IMAGE_PROMPT.md`: how to cut a picture from an eye sheet and show it on the LCD, plus a spec for your artist's eyelid images. `DISPLAY_GUIDE.md`: theme colours, new layout, wake gesture settings, Auto Configure screen |

## Measured today (Auto Configure)

| Fan | Top speed | Slowest | Presets |
|---|---|---|---|
| NF-P12 PWM | 1626 RPM | 149 RPM (stops below that) | 400 / 800 / 1200 / 1600 |
| NF-A20 Chromax | 1011 RPM | 421 RPM, **never stops** (still turning at 0 %) | 500 / 500 / 800 / 1000 (before the fix below) |
| Noctua industrial 3000 RPM | Failed: no RPM at full speed | | Power supply can't run it yet |

## Problems found and fixed today

- **Fan ran flat out and ignored every control:** the firmware only detected the chip. Adafruit's start-up code also sets the fan to 100 % with a very slow (~23 Hz) PWM clock. The firmware now sets 12 kHz, starts at 0 and drives the target speed.
- **Tach read impossible speeds (up to 360,000 RPM), then nothing:** the Adafruit EMC2101 board's TACH pin has no pull-up switched on, so it floated and counted interference from the PWM wire. Fixed in hardware: **10 kΩ from TACH to 3.3 V**. A loose wire was also found and fixed.
- **Fan only crawled during the test sweeps:** not enough power from the bench supply. Other fans worked properly.
- **USB flashing often failed ("port busy" / "access denied"):** UPS software was grabbing the serial port. CyberPower PowerPanel is uninstalled. Some failures still happen; Tripp Lite PowerAlert is the next suspect. Retrying works.
- **Board started an old firmware after a stuck USB connection** (no preset buttons on the LCD): the ESP32 fell back to its other firmware slot. A power cycle and a normal flash fixed it.
- **Wake taps:** after a knob stir, one tap could wake the board (left-over count), and a single tap was sometimes counted twice. Both fixed and checked.
- **Auto Configure on the big NF-A20:** it was still speeding up when full speed was measured, and the Low and Med presets came out equal. Now each step waits until the speed is steady, and when 25 % is below the fan's slowest speed the presets are spread from that slowest speed up to the top.

## Not yet checked on the board

- **NF-A20 re-run** (Save as → Replace): should show the steady-speed wait and presets of about 500 / 700 / 800 / 1000.
- **Auto Configure screen's final layout** (cancel hint moved to the middle) and the **30-second result**.

## Decisions made

- **PWM frequency:** 12 kHz (PWM_F 15, 30 steps). Quiet on the fans tried.
- **GPIO 2:** no further test. The finished build runs from a fixed 12 V supply with no battery.
- **Fan profiles:** presets belong to each fan; 5 profiles is plenty.
- **Finer PWM** (the ESP32 driving the fan at 25 kHz): not wanted; removed from the list.
- **Start point:** not measured separately. The EMC2101's built-in spin-up starts a stopped fan.

## Waiting on you

- **NF-A20 re-run** with Auto Configure (see above).
- **Power supply for the industrial fans**, then Auto Configure them.
- **GPIO 4 pull-down resistor** (about 10 kΩ to ground), so external power stays off during boot.
- **Eyelid images** from your artist.
- **Decision for later:** fans like the NF-A20 can't be stopped with PWM. Should Off also cut the external power (which also switches off anything else on that rail)?

## Next steps

1. Industrial fans: power, Auto Configure, field testing.
2. Later list (`CLAUDE.md`): eyelid images, Adafruit M4 Eyes, standalone eye project.
