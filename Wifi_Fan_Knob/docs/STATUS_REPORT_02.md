# WiFi Fan Knob - Project Status Report 02
**Date**: September 23, 2026
**Session**: First hardware session (board arrived; EMC2101 still in transit)
**Status**: Everything except fan control is built and verified on the board. 27 commits, all pushed.

For the technical details of each feature, see `CLAUDE.md`. This report covers what changed today and what's left.

---

## What works now (verified on hardware)

| Area | Result |
|---|---|
| **Board** | Display, touch, knob and button all working. 16 MB flash and 8 MB PSRAM detected. Serial works over the USB-C port |
| **WiFi** | Joins the saved network; falls back to its own hotspot if it can't, and turns the hotspot off once connected. Hotspot password can be changed |
| **LCD** | Three pages you swipe between: **Main** (drag the RPM arc to set speed, clock, IP address box), **Presets** (Low/Med/High/Max/OFF) and **Settings** (brightness, network info) |
| **Knob** | Turn to change RPM in steps of 100. Hold the button 1 s for standby |
| **Standby** | Animated **dragon eye** at about 62 fps. Fan off, external power off. Touch, knob or button wakes it |
| **Web page** (port 8080) | All four tabs work: Home (speed, presets, big FAN OFF, standby), WiFi (scan, connect, forget, hotspot password), Config (settings, **all world time zones**, web login, factory reset), OTA (firmware upload) |
| **Security** | Every change needs the web login. Factory reset also asks for the password again |
| **OTA updates** | Firmware upload over WiFi from the web page or command line. Bad or incomplete files are rejected, and the running firmware stays untouched |
| **Home Assistant** | Appears automatically as a "WiFi Fan Knob" device with Fan Speed, Standby, Fan Running, WiFi Signal and Uptime. Shows as unavailable when the board is offline |
| **Power switch** | GPIO 4 switches power to the fan, lights, sensors and EMC2101. Choose ON = HIGH or ON = LOW in Config |
| **Time** | Local time from NTP, following the time zone chosen on the web page |

## Problems found and fixed today

- **No serial output:** the board has no separate USB-serial chip, so serial output had to be sent over the ESP32's own USB (enabled with a build flag).
- **Wrong board settings:** the flash and PSRAM settings were for a different board variant. Fixed to 16 MB flash and OPI PSRAM.
- **Blank display:** GPIO 1 powers the display and had never been switched on. The display settings were also being guessed by an auto-detect library; replaced with Elecrow's exact settings.
- **Web page updates wiped your settings:** the page is now built into the firmware instead of stored alongside the settings.
- **Web routes caught each other:** "Forget WiFi" and "Reset to Defaults" had never worked, because other routes were catching them. Now fixed.
- **Hotspot stuck on:** once the board fell back to its hotspot, it never retried your WiFi. It now retries every 20 s.
- **Knob button woke standby immediately:** pressing the knob also touches the glass, so the eye woke straight back up. Touch now only wakes after the finger lifts.
- **Swipe pages stopped working:** once the RPM arc became draggable, the whole arc area caught every swipe. LVGL's ring-only touch detection is off by default; now turned on.
- **GPIO 2:** it isn't the whole-board power latch the original notes described. Setting it LOW on USB power had no visible effect.

## Decisions made

- The fan starts at 0 after a power loss.
- Standby turns the fan off and cuts the external power (GPIO 4). Fan Off only sets the speed to 0.
- Standby uses light sleep with an animation, not deep sleep. Light sleep itself is still to do.
- OTA, all changes and factory reset are behind the web login.
- The time zone is stored as its name plus a POSIX rule, so any zone in the world works.

## Parked / flagged for later

- **Startup crash:** about 4 in 40 boots crash while the knob's interrupts are being set up, then restart cleanly. The cause is in the precompiled SDK. It's written up in `CLAUDE.md` (Known issue: ipc1 boot panic) with how to reproduce it, how to decode the crash, and four possible fixes.
- **Dragon eye upgrades**, after the project is complete:
  - full 240×240 graphics instead of pixel doubling
  - "sleeping" behaviour (closed, twitching)
  - its own standby brightness (currently 10%)
  - slower frame rate or light sleep to save power
  - other eye styles
  - wake gestures

## Next steps

1. **EMC2101 arrives:** in `fan_control.cpp`, add PWM output and tachometer reading, then an actual-RPM reading on the screen, web page and Home Assistant.
2. Fan calibration (min/max PWM), audio, and testing on the bench with real fumes.
3. Menu on a short press of the knob (currently only logged).
4. Test GPIO 2 on non-USB power, and fit the hardware pull resistor on the GPIO 4 power switch.
