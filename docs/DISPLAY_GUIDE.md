# Display Guide: changing how the knob's screen looks

This guide explains how to change the round LCD: colours, text, buttons, sizes and positions,
and how to add or remove swipe pages. It assumes you can open the project in VS Code with
PlatformIO and press **Upload**. You don't need to know LVGL, the graphics library underneath.

![Main page layout and the angle system](images/display_guide_layout.png)

---

## 1. Before you start

### Where things live

| What you want to change | File |
|---|---|
| Almost everything on the screen | `src/ui.cpp` |
| Which font sizes are available | `include/lv_conf.h` |
| Standby brightness, how the screensaver starts and stops | `src/main.cpp` |
| The eye itself, and the list of eye styles | `src/dragon_eye.cpp`, `src/eye_styles.cpp`, `include/eyes/` |

Some things need no code at all. They're on the web page:

| Setting | Web page tab |
|---|---|
| LCD brightness | Home |
| Fan max RPM (top of the arc, knob and all speed controls) | Config → Fan Presets |
| Preset speeds (Low / Med / High / Max) | Config → Fan Presets |
| 12 / 24-hour clock, time zone | Config |
| Screensaver delay (0 = off) | Config → Display & Interface |
| Eye style (Dragon, Cat, Owl, …) | Config → Display & Interface |

### Getting a change onto the screen

1. Edit and save the file.
2. Press **Upload** in PlatformIO with the knob plugged in by USB. It builds and flashes.
   If the upload says the port is busy, close the Serial Monitor first.
3. Or, without USB: press **Build**, then send
   `.pio/build/esp32-s3-devkitc-1/firmware.bin` from the web page's **OTA Update** tab.

If a change breaks something, undo it and upload again. USB upload always works, even if
the screen is blank.

**Tip:** in `ui.cpp`, use **Ctrl+F** to find the names used in this guide (`SEG_R_OUT`,
`create_main_page`, and so on). Line numbers change as the code grows; names don't.

---

## 2. Four basics

### Positions: x and y from the centre

The screen is 240 × 240 pixels, but round: the corners can't be seen. Most things are
placed relative to the **centre** of the screen:

```cpp
lv_obj_align(clock_label, LV_ALIGN_CENTER, 0, 82);
//                                          x   y
```

* **x**: positive moves **right**, negative moves left.
* **y**: positive moves **down**, negative moves up.
* The visible circle has a radius of 120, so keep everything within about 110 of the centre.

Example: to move the clock up 10 pixels, change `82` to `72`.

### Colours

**The theme.** The screen uses the web page's dark blue and gold. The colours are named once,
at the top of `src/ui.cpp`, and used everywhere by name. Change one there and every use follows:

| Name | Colour | Used for |
|---|---|---|
| `THEME_BG_TOP`, `THEME_BG_BOTTOM` | `0x1A1A2E` → `0x16213E` dark blue | Main's background (a top-to-bottom gradient); dark text on gold |
| `THEME_GOLD` | `0xFFD700` gold | Target RPM, RPM arc, lit segment, current page dot, selected menu item, IP box text and border, Settings slider, Auto Configure screen background |
| `THEME_ON_GOLD` | `0x1A1A2E` | Text on a gold segment or menu item |
| `THEME_TEXT` | `0xE0E0E0` light grey | Actual RPM, segment labels, menu items, Settings brightness label |
| `THEME_TEXT_DIM` | `0xA0A0A0` grey | "RPM" caption, "now", clock, Settings title and info |
| `THEME_PANEL` | `0x2A3150` slate blue | Unlit segments, other page dots, menu items, IP box, Settings slider track, "not running" popup |
| `THEME_TRACK` | `0x252B45` | RPM arc's unfilled track |
| `THEME_OFF` | `0x8B1E1E` dark red | Off segment when not lit |
| `THEME_ALERT` | `0xD32F2F` red | "Fan stopped" popup, WiFi-lost flash |
| `THEME_GOLD_DARK` | `0xB39700` | Auto Configure screen: progress ring's unfilled track |

For a one-off colour, write it as a hex code, like on a web page:

```cpp
lv_color_hex(0x8B1E1E)          // dark red
lv_color_white()                // white
lv_color_black()                // black
lv_palette_main(LV_PALETTE_CYAN)
```

Palette names you can use: `RED`, `PINK`, `PURPLE`, `DEEP_PURPLE`, `INDIGO`, `BLUE`,
`LIGHT_BLUE`, `CYAN`, `TEAL`, `GREEN`, `LIGHT_GREEN`, `LIME`, `YELLOW`, `AMBER`, `ORANGE`,
`DEEP_ORANGE`, `BROWN`, `BLUE_GREY`, `GREY`. Write them as `LV_PALETTE_ORANGE`, etc.

Any colour picker (search "colour picker" in a browser) gives you a hex code like
`#FF8800`. Write it as `0xFF8800`.

### Text size (fonts)

The screen uses the Montserrat font in fixed sizes. These are switched on now:

| Font | Used for |
|---|---|
| `lv_font_montserrat_14` | small text: segment labels, "RPM", IP box, info |
| `lv_font_montserrat_20` | clock, page titles, menu, popups |
| `lv_font_montserrat_40` | the big RPM number |
| `lv_font_montserrat_48` | not shown at the moment (old standby clock) |

To use another size (any even number from 8 to 48), switch it on in `include/lv_conf.h`:

```c
#define LV_FONT_MONTSERRAT_32 1
```

and then use `&lv_font_montserrat_32` in `ui.cpp`. Each size you switch on takes some
firmware space: about 10 KB for small sizes and up to about 60 KB for the largest. There's
plenty of room, but don't switch on sizes you don't use.

### Icons

The built-in fonts include simple icons. Use them inside text:

```cpp
LV_SYMBOL_POWER "\nOff"      // power icon, then "Off" on a second line
```

Other icons include `LV_SYMBOL_SETTINGS`, `LV_SYMBOL_WIFI`, `LV_SYMBOL_HOME`,
`LV_SYMBOL_BELL`, `LV_SYMBOL_OK`, `LV_SYMBOL_CLOSE`, `LV_SYMBOL_PLUS`, `LV_SYMBOL_MINUS`,
`LV_SYMBOL_UP`, `LV_SYMBOL_DOWN`, `LV_SYMBOL_EYE_OPEN`, `LV_SYMBOL_WARNING` and
`LV_SYMBOL_REFRESH`. They only work in the Montserrat fonts above.

---

## 3. The Main page

The Main page is built in `create_main_page()` in `ui.cpp`. The picture at the top shows
each part.

### The five segment buttons (Off, Low, Med, High, Max)

These are the settings near the top of the **QUICK SEGMENTS** section of `ui.cpp`:

```cpp
static const int SEG_COUNT = 5;
static const int SEG_START = 150;   // Left end, just below 9 o'clock
static const int SEG_SPAN = 48;     // Per segment incl. gap
static const int SEG_GAP = 4;
static const int SEG_R_OUT = 94;    // Outer edge of the band
static const int SEG_R_IN = 60;     // Inner edge of the band
```

**Angles** are in degrees, measured **clockwise from 3 o'clock**: 0° is 3 o'clock, 90° is
6 o'clock, 180° is 9 o'clock and 270° is 12 o'clock. The right-hand side of the picture
shows this.

| To... | Change |
|---|---|
| Make the band thicker or thinner | `SEG_R_IN` (smaller = thicker, towards the centre) |
| Move the band in or out | `SEG_R_OUT` and `SEG_R_IN` together. Keep `SEG_R_OUT` at 94 or less, or the segments touch the RPM arc |
| Make the gaps between segments wider | `SEG_GAP` |
| Rotate the whole band | `SEG_START` |
| Make each segment longer or shorter | `SEG_SPAN` (the band covers `SEG_COUNT × SEG_SPAN` degrees; keep it at 240 or less so the bottom stays open for the actual RPM and clock) |

**The RPM arc follows the band automatically.** Its ends are worked out from these numbers,
so it always ends level with the outer segments.

**Labels:** the text on each segment is in this line:

```cpp
static const char *const seg_text[SEG_COUNT] = {LV_SYMBOL_POWER "\nOff", "Low", "Med", "High", "Max"};
```

`"\n"` starts a new line. The labels use `lv_font_montserrat_14`, set in `create_segments()`.

**Colours** are in `seg_highlight()`. A lit segment (pressed, or the current speed) is gold
with dark text; otherwise Off is dark red and the others slate blue, with light text:

```cpp
lv_color_t c = lit    ? lv_color_hex(THEME_GOLD)
             : i == 0 ? lv_color_hex(THEME_OFF)
                      : lv_color_hex(THEME_PANEL);
```

**Speeds:** each segment's speed comes from the preset speeds on the web page (Config →
Fan Presets). Off is always 0. The list is in `seg_rpm()`.

**Adding or removing a segment** means changing three things so they agree:

1. `SEG_COUNT`: the number of segments.
2. `seg_text`: one label per segment.
3. `seg_rpm()`: one speed per segment.

Then adjust `SEG_SPAN` so they still fit (for 6 segments, try `SEG_SPAN = 40`). If you
list more labels or speeds than `SEG_COUNT`, the build fails. If you list fewer, the extra
segments have no label and set 0 RPM (the same as Off).

### The RPM arc (outer ring)

In `create_main_page()`:

| To change | Look for |
|---|---|
| Ring thickness | `lv_obj_set_style_arc_width(rpm_arc, 12, ...)`. There are two lines: track and fill |
| Track colour (unfilled part) | `THEME_TRACK` on the `LV_PART_MAIN` line |
| Fill colour | `THEME_GOLD` on the `LV_PART_INDICATOR` line |
| Drag knob colour | `THEME_TEXT` on the `LV_PART_KNOB` line |
| Overall size | `lv_obj_set_size(rpm_arc, 228, 228)`. If you make it smaller, reduce `SEG_R_OUT` to match |

The arc can be dragged to set the speed. It snaps to the RPM step size set in the config.

### Target RPM, actual RPM and clock

All four are in `create_main_page()`, one block each. Gold means "what you asked for", light
text means "what the fan is doing":

| Item | Font | Colour | Position (x, y) |
|---|---|---|---|
| Target RPM (`rpm_label`): set by knob, arc and segments | `montserrat_40` | `THEME_GOLD` | 0, -6 |
| "RPM" caption (`unit_label`) | `montserrat_14` | `THEME_TEXT_DIM` | 0, 22 |
| Actual RPM (`actual_label`): "now 1234" from the tach | `montserrat_20` | `THEME_TEXT`, "now" dim | 0, 48 |
| Clock (`clock_label`), in the arc's bottom gap | `montserrat_20` | `THEME_TEXT_DIM` | 0, 82 |

The actual line changes by itself, once a second (`ui_update()` in `src/ui.cpp`). It is
hidden while the fan is stopped (or with no fan controller). The grey "now" uses LVGL's recolour
code: `#A0A0A0 now#` in the text. The font has no special characters such as "·".

When the saved WiFi is lost, the clock flashes red (with the IP box on Settings), in
`status_flash_cb()`.

**Watch the RPM number's size.** A 4-digit speed at a larger font runs into the Off and Max
segments; that's why it's 40 and not 48.

### Page dots

In `create_page_dots()`: 8-pixel dots, 16 pixels apart, 10 pixels up from the bottom.
The current page is gold and the others slate blue, all with a thin dark outline so the gold
dot always stands out. There's one dot per page, added automatically.

### Double-tap popup

Double-tapping empty space on Main stops the fan. The popup text and colours are in
`main_tap_cb()`: `"Fan stopped"` on `THEME_ALERT` red, `"Fan is not running"` on `THEME_PANEL`. The popup's font,
padding and 1.5-second display time are in `show_popup()`. The double-tap window
(400 ms) is in `main_tap_cb()`: `now - last_tap < 400`.

---

## 4. Pages

### How pages work

You swipe left and right between pages. The pages are listed in **one table** near the top
of `ui.cpp`:

```cpp
static const Page PAGES[] = {
  {"Main", create_main_page},
  {"Settings", create_settings_page},
};
```

Each row is a **name** (shown in the knob menu) and a **function that builds the page**.
Everything else follows this table automatically:

* the swipe order (top row is the left-most page)
* the page dots
* the knob menu (short press on the knob)
* tap-empty-space-to-return-to-Main, on every page except Main

**Main must stay first.** Turning the knob and waking up both return to the first page.

### Reordering, renaming or removing a page

* **Reorder:** move the rows.
* **Rename in the menu:** change the name in quotes.
* **Remove:** delete the row. Also delete the page's `create_..._page` function and its
  declaration near the top of the file, or the build warns that it's unused.

### Adding a page: a worked example

This adds an "About" page after Settings.

**Step 1.** Add a declaration next to the others near the top of `ui.cpp`:

```cpp
static void create_settings_page(lv_obj_t *tile);
static void create_about_page(lv_obj_t *tile);      // ← add
```

**Step 2.** Add a row to the table:

```cpp
static const Page PAGES[] = {
  {"Main", create_main_page},
  {"Settings", create_settings_page},
  {"About", create_about_page},                     // ← add
};
```

**Step 3.** Write the page. Put it just after `create_settings_page()`, so it can use the
`page_title()` helper:

```cpp
static void create_about_page(lv_obj_t *tile) {
  page_title(tile, "About");                        // grey title near the top

  lv_obj_t *text = lv_label_create(tile);
  lv_obj_set_style_text_font(text, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(text, lv_color_white(), 0);
  lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(text, "WiFi Fan Knob\nsergeant82d/Wifi_Fan_Knob");
  lv_obj_align(text, LV_ALIGN_CENTER, 0, 0);
}
```

Upload. There's now a third dot, "About" is in the knob menu, and a tap on the page returns
to Main.

**A button on a new page** looks like this (copy it inside your page function):

```cpp
  lv_obj_t *btn = lv_btn_create(tile);
  lv_obj_set_size(btn, 100, 44);                                  // width, height
  lv_obj_align(btn, LV_ALIGN_CENTER, 0, 50);                      // x, y
  lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_GREEN), 0);
  lv_obj_add_event_cb(btn, my_button_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, "Press me");
  lv_obj_center(label);
```

with what it does written above the page function:

```cpp
static void my_button_cb(lv_event_t *) {
  fan_set_target(1500);   // example: set the fan to 1500 RPM
}
```

### The Settings page

In `create_settings_page()`, in the same dark blue and gold as Main:
* title at `y = -80`
* brightness label and slider (slider 150 wide, range 10–100 %; gold fill and knob)
* IP address box at `y = 22`: slate blue with gold text and a thin gold border, made by
  `create_status_box(tile, 22)`.
  When the saved WiFi is lost it flashes red and white every half second (colours in
  `status_flash_cb()`, speed in `lv_timer_create(status_flash_cb, 500, ...)`).
* network name and MQTT status at `y = 60`

The IP box and the information text are updated once a second (`update_status_box()`,
`update_info()`).

### The Auto Configure screen

A separate screen (not a swipe page) that appears by itself while Auto Configure runs (web
page → Config → Fan Profiles). It uses the theme **reversed**, a gold background with dark
blue text, so it can't be mistaken for anything else. Built in `create_config_screen()`,
updated once a second by `update_config_screen()`:

| Item | Font | Position (y) |
|---|---|---|
| Progress ring around the edge (fills clockwise from the top) | 10 px wide, 232 across | — |
| "AUTO CONFIGURE" | `montserrat_14` | -70 |
| Fan name (cut with "..." if too long) | `montserrat_14` | -48 |
| Percentage, then "Done" / "Cancelled" / "Failed" | `montserrat_48` | -16 |
| "Press knob to cancel" | `montserrat_14` | 20 |
| Live RPM | `montserrat_20` | 46 |
| "Step 12 of 31" (`CFG_DETAIL_Y`) | `montserrat_14` | 70 |
| Result message, wraps onto 2-3 lines (`CFG_RESULT_Y`) | `montserrat_14` | 34 |

Keep text within about ±80 of the centre: lower down, the progress ring cuts across it.

When it finishes, the result stays up for 30 seconds (`CFG_RESULT_MS` in `src/ui.cpp`), then Main returns. A knob turn or press skips the wait.

### The knob menu

In `create_menu()` and `menu_highlight()`:
* The background is dark blue (`THEME_BG_BOTTOM`) at 90 % opacity (`LV_OPA_90`).
* Each item is 150 × 40 with `montserrat_20` text.
* The selected item is gold with dark text; the others are slate blue with light text.

---

## 5. Screensaver and standby

| To change | Where |
|---|---|
| Screensaver delay, or turn it off | Web page → Config → Display & Interface (seconds; 0 = off) |
| Eye style | Web page → Config → Display & Interface |
| Standby brightness (0 = screen off) | Web page → Config → Display & Interface |
| How long the screensaver runs before asking "Keep the fan running?" (0 = never) | Web page → Config → Display & Interface → Standby after screensaver (minutes) |
| How long that question waits for an answer before standby | Web page → Config → Display & Interface → Standby prompt timeout (seconds) |
| The question's look: text, buttons, countdown | `create_prompt()` in `src/ui.cpp` |
| How the eye sleeps in standby (how long it stays shut, twitches, peeks) | `sleep_openness()` in `src/dragon_eye.cpp` |
| Eye speed in standby (15 frames per second) | `STANDBY_EYE_FPS` in `src/main.cpp` |
| Taps needed to wake from standby, after the first tap that stirs the eye (4) | `WAKE_TAPS` in `src/main.cpp` |
| How long a stirred eye stays awake after each tap or knob turn (5000 ms) | `STIR_MS` in `src/main.cpp` |
| How long the eye glances left or right after a knob turn (1000 ms) | `KNOB_GLANCE_MS` in `src/main.cpp` |
| How long the finger must be off the glass before the next tap counts (60 ms) | `TAP_LIFT_MS` in `src/main.cpp` |
| How fast a stirred eye opens (400 ms) and closes again (1500 ms) | `STIR_OPEN_MS` and the `SLEEP_STIR` case in `sleep_openness()`, `src/dragon_eye.cpp` |
| What counts as "activity" | `note_activity()` calls in `src/main.cpp` (knob, button, touch, speed changes) |
| The styles on offer, and their order | `EYE_STYLES` in `src/eye_styles.cpp` |

The eye styles come from Adafruit's "Uncanny Eyes". Each one is a data file in
`include/eyes/`, drawn for a 128-pixel screen. When you pick a style, the knob scales it up
to this screen's 240 pixels, so it's smooth but not more detailed than the original art.
To remove a style from the web list, delete its line in `EYE_STYLES`. To add one, copy a
block in `src/eye_styles.cpp` (the file explains how).

The screensaver shows the eye while the fan keeps running. A touch, knob turn or
short press only dismisses it. In standby the eye goes to sleep: it closes, stays shut,
and now and then twitches or peeks. Standby also stops the fan and switches external
power off.

**Screensaver, then standby.** After the screensaver has run for the set time, the eye
closes and the LCD asks **"Keep the fan running?"** with **Keep running** and **Standby**
buttons and a countdown. Tap a button, or turn the knob to choose and press to pick. Keep
running goes back to the screensaver (and the time starts again); no answer means standby. If
the fan is already off, there's nothing to ask: it goes straight to standby.

**Modes are like radio buttons.** Active, Screensaver and Standby: exactly one is on. The web
Home tab's Display Mode buttons and Home Assistant's switches can pick any of them directly.

**Waking from standby (wake gestures).** A bump shouldn't wake the board, so the eye
reacts first:

- **First tap:** the eye stirs. It opens, looks toward your finger and stays awake for
  `STIR_MS`.
- **While it's awake:** it follows your finger. `WAKE_TAPS` more taps wake the board, and
  each tap keeps the eye awake a little longer. If it falls asleep first, the count starts
  over.
- **Turning the knob:** stirs the eye, which glances the way the knob turned, without
  waking the board.
- **Pressing the knob:** always wakes the board straight away.
- **The web page and Home Assistant:** wake it as before.

None of this applies to the screensaver: any touch, knob turn or press dismisses it at
once, and the eye doesn't follow your finger there.

---

## 6. When something goes wrong

| Symptom | Likely cause and fix |
|---|---|
| Build error: `'lv_font_montserrat_32' was not declared` | That font size isn't switched on. Add `#define LV_FONT_MONTSERRAT_32 1` to `include/lv_conf.h` |
| Something doesn't appear | It's outside the visible circle (keep within ~110 of the centre), or something created later is drawn on top of it |
| Taps on a new item do nothing | Something clickable is on top of it. Labels don't take taps; buttons and boxes do |
| Swiping between pages stops working | A large clickable item on the page is catching the swipe. Make it smaller, or make it not clickable: `lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE)` |
| Text runs into the segments | Use a smaller font, or move it down (bigger y) |
| Screen blank or rebooting after an upload | Open the Serial Monitor to see the error. Undo the change and upload again by USB |

For anything not covered here, the LVGL 8 documentation is at
<https://docs.lvgl.io/8.4/>. This project uses LVGL **8.4**; examples written for LVGL 9
(including Elecrow's) use different function names.
