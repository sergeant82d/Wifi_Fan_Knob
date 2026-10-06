# WiFi Fan Knob - Project Status Report 07
**Date**: October 6, 2026
**Session**: Sixth session day: RGB LEDs, the three new I2C sensors (SHT41, SGP41, APDS-9999), a fan fault, Home tab reorder
**Status**: Final for the day. Everything below is flashed, checked on the board, committed and pushed. The board runs `55b962c`.

For the technical details of each feature, see `../CLAUDE.md`.

---

## What changed today

| Area | Result |
|---|---|
| **RGB LEDs** | The five LEDs on the knob now work: Off, Solid, Flash, Breathe and Rainbow, with colour, brightness and speed. Brightness is capped in firmware at 100 of 255, which is also the most you can set. Off in standby. Web: "LEDs" card on the Home tab. Home Assistant: an "LEDs" light (on/off, colour, brightness, effects) and an "LED Speed" slider (`584d624`) |
| **Air sensors** | SHT41 (temperature, humidity) and SGP41 (VOC Index, NOx Index) read once a second, also through standby (they're on always-on 3.3 V). Web: "Air" card (°F and °C). Home Assistant: Temperature, Humidity, VOC Index, NOx Index (`7598214`) |
| **I2C device list** | The serial log lists every device on the main I2C bus at boot and on each wake. Today: 0x44 SHT41, 0x4C EMC2101, 0x52 APDS-9999, 0x59 SGP41 (`7598214`) |
| **APDS-9999** | Proximity and light level (lux) on the web ("Presence & Light" card) and in Home Assistant (Proximity, Illuminance) (`342f512`). Its settings are reset at every boot, since the chip keeps them while powered (`561efcf`) |
| **Home tab order** | RPM, Quick Start Presets, Manual Speed Control, FAN OFF, Display Mode, Air, Presence & Light, LCD (brightness slider now under the picture), LEDs, System Status, Notes. Phones: LCD card first (`55b962c`) |
| **Docs** | Power notes corrected: GPIO 4 switches only the 12 V rail; the EMC2101 and sensors are on always-on 3.3 V. Auto mode restart plan, presence decision and left-side tabs plan recorded. TODO LED item ticked (`0f778d3`) |

## Checked today

- On the board, by you: every LED effect, colour, brightness, speed, standby and Home Assistant; air readings through standby and after wake, the four HA air sensors, temperature accuracy, eye still smooth; the Home tab order on PC and phone.
- By me on the board: the I2C scan; air readings from boot (temperature and humidity at once, VOC and NOx after about a minute); APDS-9999 readings; the proximity tests below.

## Problems found and fixed today

- **Fan stopped spinning.** Two separate faults:
  - The **TACH pull-up resistor** on the fan controller board was open and discoloured on the TACH end. That's why the RPM read 0. You replaced it, and RPM reads again.
  - The **140 mm industrial fan is dead**: 12 V at its connector, no spin at any setting. Don't plug it back into the board. The NF-P12 is fitted, and you re-ran Auto Configure for it (presets 400 / 700 / 1100 / 1400). The supply measured 12.43 V at full speed, so it's fine.
- **A Git Bash `sed` command stripped the page file's CRLF line endings.** Restored; I no longer use `sed` on CRLF files.
- **Sharing the I2C bus safely.** The core's `Wire` lock covers one call, not a request plus the reads after it, so the sensors are read from `loop()` like the fan, not from their own task. Sensirion's driver libraries wait inside every call, so I send the SGP41 and SHT41 commands myself.

## Proximity tests (APDS-9999)

| | Default power | Full power (25 mA, 255 pulses) |
|---|---|---|
| Empty bench | 2-7 | 75-120 |
| Sitting (80-110 cm) | 1-9 | same as empty |
| Leaning in | 6-11 | 115-155 |
| Hand at ~20 cm | peaks 13-16 | — |

It can't see a seated person at your bench. It's a short-range sensor (centimetres). It stays in use for light level, and maybe a close hand wave later.

## Decisions made

- LED brightness cap: 100 of 255, also the user maximum.
- Every web control also goes into Home Assistant, until you say otherwise.
- Auto mode will ignore the VOC Index for about the first hour after a restart (option A). The learned state isn't saved, so a restart is the "recalibrate" after a room move.
- Presence: **AMG8833 thermal camera** ordered. The STHS34PF80 was ruled out because a hot iron in its view would look like a person.
- Next web page change: left-side tabs, layout from ETH_Touch_PWM.

## Numbers

- Flash: **86.5 %** of the 6.25 MB app slot (86.0 % at the start of the day: LEDs +0.2, air sensors +0.2, APDS-9999 +0.1). About 880 KB free.
- RAM: 35.0 %.
- Full flash backup before today's first flash: `Wifi_Fan_Knob_backups\full_flash_2026-10-06_before_leds.bin` (outside the repo; contains the WiFi and MQTT passwords).

## Waiting on you

- The **AMG8833** to arrive. Wire it like the others (always-on 3.3 V, SDA/SCL on GPIO 38/39); the boot log should then also list 0x69.
- A day or two of VOC Index history in Home Assistant, to pick the Auto mode thresholds.
- **Still open:** GPIO 4 pull-down resistor (waits on the FPC breakout).

## Next steps

1. AMG8833: test first (away, sitting, leaning in, hot iron with nobody there, a long still sit), then the "presence for X seconds wakes the screen" rule.
2. Auto mode from the VOC Index, with the two LCD TODO items (lower RPM arc; speed buttons above the horizontal, Off and Auto below).
3. Web page: left-side tabs.
4. Later: automatic LCD brightness from the light level; PWM and tach on the UART0 connector after the FPC breakout arrives.
