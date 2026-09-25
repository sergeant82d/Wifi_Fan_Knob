# Prompt: Show a Picture on the LCD

Use this document as the prompt for a new session. It assumes no earlier context.
It explains how to cut one round picture out of a printed sheet and show it on the
knob's round LCD as a test.

---

## Task

> Cut the picture I name out of the sheet I name in `assets/eye_art/`, fit it to the 240 × 240
> round LCD, and show it on the board in place of the animated eye, as a **temporary
> test build**. Show me the cropped picture before flashing. Don't commit the test code.
> When I say I'm done, take the test code out and flash the normal firmware again.

The user names the sheet and the picture, for example "1.5_inch sheet, bottom left".

## Project facts you need

- **Board:** Elecrow CrowPanel 1.28" (ESP32-S3). GC9A01 LCD, 240 × 240, round: only the
  circle is visible. Drawn with LovyanGFX in RGB565.
- **PlatformIO:** the project is at the repo root. Always use
  `~/.platformio/penv/Scripts/pio.exe`. Build and flash over USB with
  `pio.exe run -t upload`; the board is usually COM13. If the port is busy, ask the user
  to close any serial monitor rather than hunting for the process.
- **Board web page:** `http://192.168.10.102:8080` (static IP). `/api/status` answers
  without a login. Actions like the Screensaver button need the user's login, so ask
  the user to press them.
- **Where the eye is drawn:** `eye_frame()` in `src/dragon_eye.cpp`. It runs while the
  screensaver or standby is on. `OUT` is 240 and `tft` is the display.
- **Tools:** use Python with Pillow and numpy. Write multi-line Python with the Write
  tool rather than bash heredocs, because heredocs mangle `\n`. Use the scratchpad
  directory for intermediate files.

## The sheets

- The files are `assets/eye_art/*_dragon-eyes-8.5x11.jpg`: US Letter at 300 dpi, 2550 × 3300 px.
  Each holds round pictures in a grid on a white background, 4 across by 6 down on the
  1.5 inch sheet.
- The file name gives the circle size. At 300 dpi, 1.5 inch is about 450 px across.
- These are purchased images. Use them only as the user directs, and remember the
  repo is public.

## Steps

### 1. Find the circle's centre and diameter

Make a small preview of the sheet (fit it inside 900 px) and look at it with the Read
tool to find the picture. Then measure it at full resolution:

- Estimate the centre from the preview, multiplied by 2550 ÷ preview width.
- Along the row and column through that centre, walk outwards until a pixel's grey
  value is 235 or more (the white paper). Repeat until the centre stops moving.
- Check it: take the midpoint of the dark span on 8 rows and 8 columns across the
  circle. They should agree within 1 px.

### 2. Crop, scale and check

- A square crop of half-size `h` around the centre, scaled to 240 × 240 with
  `Image.LANCZOS`.
- Keep `h` a few pixels inside the radius so no white edge shows. For a 451 px circle,
  `h = 221` gives a centred picture.
- **To move the picture on the LCD:** first shrink `h` to make room (`h = 214` allows
  about 3 px of movement), then shift the crop window the **opposite** way.
  - One screen pixel is `2·h / 240` source pixels.
  - For the picture to move right and down by `n` px, use centre `(cx − s, cy − s)`,
    where `s = round(n · 2h / 240)`.
- Save the result as a PNG and **show it to the user with Read before flashing.**
  Check the edge all the way round for white.

### 3. Convert to firmware data

Write `include/eye_image_test.h` with one `static const uint16_t
EYE_IMAGE_TEST[240 * 240]` array. Each value is plain RGB565:
`((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)`, with rows top to bottom and 16
values per line. It takes about 113 KB of flash; the app slot has room.

### 4. Temporary test hook

In `src/dragon_eye.cpp`, directly above `void eye_frame()`, add:

```cpp
#include "eye_image_test.h"  // TEMP TEST
```

Inside `eye_frame()`, right after the first line (`if (!tft || !eye.style) return;`), add:

```cpp
  {  // TEMP TEST: show the still image instead of the animated eye
    tft->startWrite();
    tft->setAddrWindow(0, 0, OUT, OUT);
    tft->writePixels((lgfx::rgb565_t *)EYE_IMAGE_TEST, OUT * OUT);
    tft->endWrite();
    return;
  }
```

### 5. Flash and view

- Run `pio.exe run -t upload`, then check that `/api/status` answers.
- Ask the user to press **Screensaver** on the web Home tab, or to wait out the idle
  time (default 30 s). Standby shows the picture too.
- A touch or a knob turn returns to the normal screen.
- Ask about colours, brightness and centring, and adjust with step 2.

### 6. Clean up when the user is done

Delete `include/eye_image_test.h`, remove both TEMP TEST additions
(`git checkout src/dragon_eye.cpp` if nothing else changed there), and flash again.
Check that `git status` is clean.

## Reference: the first picture done (2026-09-25)

- **Sheet:** `assets/eye_art/1.5_inch_dragon-eyes-8.5x11.jpg`, bottom-left picture (purple and
  orange fiery eye).
- **Measured circle:** centre (502, 2898), diameter 451 px.
- **Final, approved by the user:** `h = 214`, centre (497, 2893). That's 3 px right and
  3 px down on the LCD, zoomed 3 %.
- This centring suits this artwork: the eye sits slightly up and left inside its
  circle. Moving it further would cut off the edge of the eye socket.
- The user said the colours and brightness look right with the conversion above, with
  no correction needed.

---

## Spec for the artist: eyelid images

These images will be shown when the eye is shut. The plan, from the Later list in
`CLAUDE.md`, is to use them as the eyelid texture: while blinking, twitching or
peeking, the eye opens through the image.

- **Size:** 240 × 240 px, or any larger square (it will be scaled down). A PNG is best;
  JPG is fine.
- **Visible area:** only the centred circle, 240 px across. Anything in the corners is
  never seen. Keep important detail slightly inside the edge.
- **Content:** a closed eye (eyelids, scales, skin) centred in the circle. It should
  line up with the eye it covers, so the lids open around the same spot.
- **Matching an open eye:** if a lid is meant for a particular open-eye picture or eye
  style, make it from the same artwork at the same size and position.
- **Rights:** the images go into a public GitHub repository, so the user must have the
  right to publish them.
