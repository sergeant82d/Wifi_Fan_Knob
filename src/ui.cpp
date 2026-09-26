#include "ui.h"
#include "config.h"
#include "fan_control.h"
#include "mqtt.h"
#include <lvgl.h>
#include <WiFi.h>
#include <time.h>

// Main screen (240x240 round) is a horizontal tileview built from PAGES (below); swipe left from Main:
//   Main:     RPM arc · band of Off + preset segments inside it (top) · target RPM
//             (gold, centre) · actual RPM (light, below) · clock (bottom gap)
//   Settings: brightness slider (live; saved on release), IP box, network/MQTT info
// Auto Configure has its own gold screen (reversed theme), shown only while it runs.
// Page dots sit in the arc's bottom gap. Knob turns and wake return to Main.
// Knob short press opens a menu of the pages: turn to choose, press or tap to go.
// Double-tap on Main stops the fan (or pops up "Fan is not running").
// On every other page, a tap on empty space slides back to Main.

// Theme: the web page's dark blue and gold. Change a colour here and everything using it follows.
static const uint32_t THEME_BG_TOP = 0x1A1A2E;     // Screen background: top-to-bottom gradient,
static const uint32_t THEME_BG_BOTTOM = 0x16213E;  //   as on the web page
static const uint32_t THEME_GOLD = 0xFFD700;       // Accent: target RPM, RPM arc, lit segment, selection
static const uint32_t THEME_ON_GOLD = 0x1A1A2E;    // Text on gold
static const uint32_t THEME_TEXT = 0xE0E0E0;       // Main text: clock, segment labels, IP box
static const uint32_t THEME_TEXT_DIM = 0xA0A0A0;   // Secondary text: "RPM (now N)" caption
static const uint32_t THEME_PANEL = 0x2A3150;      // Unlit segments, IP box, menu items, popups
static const uint32_t THEME_TRACK = 0x252B45;      // RPM arc's unfilled track
static const uint32_t THEME_OFF = 0x8B1E1E;        // Off segment when not lit
static const uint32_t THEME_ALERT = 0xD32F2F;      // "Fan stopped" popup, WiFi-lost flash
static const uint32_t THEME_GOLD_DARK = 0xB39700;  // Auto Configure screen (gold): progress ring's track

static lv_obj_t *rpm_arc = nullptr;
static lv_obj_t *rpm_label = nullptr;
static lv_obj_t *clock_label = nullptr;
static lv_obj_t *unit_label = nullptr;    // "RPM" under the target, or Auto Configure progress
static lv_obj_t *actual_label = nullptr;  // Measured RPM ("now 1234"), hidden when stopped
static lv_obj_t *main_screen = nullptr;
static lv_obj_t *standby_screen = nullptr;   // Dimmed: large clock only (dragon eye later)
static lv_obj_t *standby_clock = nullptr;
static lv_obj_t *config_screen = nullptr;    // Auto Configure progress (gold), shown only while it runs
static lv_obj_t *cfg_ring = nullptr;
static lv_obj_t *cfg_name = nullptr;
static lv_obj_t *cfg_big = nullptr;          // "40%", then "Done" / "Cancelled" / "Failed"
static lv_obj_t *cfg_rpm = nullptr;
static lv_obj_t *cfg_detail = nullptr;       // "Step 12 of 30", then the result message
static lv_obj_t *cfg_hint = nullptr;
static unsigned long cfg_result_until = 0;   // Result shown until (millis); 0 = not showing
static lv_obj_t *status_box = nullptr;
static lv_obj_t *status_label = nullptr;
static lv_timer_t *status_flash_timer = nullptr;
static bool status_flash_red = false;

static lv_obj_t *tileview = nullptr;
static lv_obj_t *brightness_slider = nullptr;
static lv_obj_t *brightness_label = nullptr;
static lv_obj_t *info_label = nullptr;

static void create_standby_screen();
static void create_config_screen();
static void create_main_page(lv_obj_t *tile);
static void create_settings_page(lv_obj_t *tile);
static void create_page_dots(lv_obj_t *parent);
static void tap_back_cb(lv_event_t *);
static void create_menu(lv_obj_t *parent);

// Swipe pages, left to right. To add or reorder a page, write a create_*_page(tile)
// builder and edit this table: tiles, page dots and the knob menu all follow it.
// Main must stay first (knob turns and wake return to page 0).
struct Page {
  const char *name;                 // Shown in the knob menu
  void (*create)(lv_obj_t *tile);   // Builds the page's widgets on its tile
};
static const Page PAGES[] = {
  {"Main", create_main_page},
  {"Settings", create_settings_page},
};
static const int PAGE_COUNT = sizeof(PAGES) / sizeof(PAGES[0]);
static lv_obj_t *page_dots[PAGE_COUNT];

// ============================================================================
// STATUS BOX (IP address; flashes when attention needed)
// ============================================================================

static void status_set_colors(lv_color_t bg, lv_color_t fg) {
  lv_obj_set_style_bg_color(status_box, bg, 0);
  lv_obj_set_style_text_color(status_label, fg, 0);
}

// WiFi lost: the IP box (Settings) and the clock (Main) flash red, so it shows on either page
static void status_flash_cb(lv_timer_t *) {
  status_flash_red = !status_flash_red;
  if (status_flash_red) {
    status_set_colors(lv_color_hex(THEME_ALERT), lv_color_white());
  } else {
    status_set_colors(lv_color_hex(THEME_PANEL), lv_color_hex(THEME_GOLD));
  }
  lv_obj_set_style_text_color(clock_label, lv_color_hex(status_flash_red ? THEME_ALERT : THEME_TEXT_DIM), 0);
}

static void create_status_box(lv_obj_t *parent, int y) {
  status_box = lv_obj_create(parent);
  lv_obj_set_size(status_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_align(status_box, LV_ALIGN_CENTER, 0, y);
  lv_obj_clear_flag(status_box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_width(status_box, 1, 0);
  lv_obj_set_style_border_color(status_box, lv_color_hex(THEME_GOLD), 0);
  lv_obj_set_style_radius(status_box, 6, 0);
  lv_obj_set_style_pad_hor(status_box, 8, 0);
  lv_obj_set_style_pad_ver(status_box, 4, 0);
  lv_obj_add_flag(status_box, LV_OBJ_FLAG_EVENT_BUBBLE);  // Taps reach the page (double-tap)

  status_label = lv_label_create(status_box);
  lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
  lv_label_set_text(status_label, "Starting...");

  status_set_colors(lv_color_hex(THEME_PANEL), lv_color_hex(THEME_GOLD));
  status_flash_timer = lv_timer_create(status_flash_cb, 500, nullptr);
  lv_timer_pause(status_flash_timer);
}

void ui_set_attention(bool attention) {
  bool flashing = !status_flash_timer->paused;
  if (attention == flashing) return;
  if (attention) {
    lv_timer_resume(status_flash_timer);
  } else {
    lv_timer_pause(status_flash_timer);
    status_flash_red = false;
    status_set_colors(lv_color_hex(THEME_PANEL), lv_color_hex(THEME_GOLD));
    lv_obj_set_style_text_color(clock_label, lv_color_hex(THEME_TEXT_DIM), 0);
  }
}

static void update_status_box() {
  String text;
  if (WiFi.status() == WL_CONNECTED) {
    text = WiFi.localIP().toString() + ":" + String(config.webserver.port);
  } else if (WiFi.getMode() & WIFI_AP) {
    text = "AP " + WiFi.softAPIP().toString() + ":" + String(config.webserver.port);
  } else {
    text = "WiFi lost";
  }
  if (text != lv_label_get_text(status_label)) {
    lv_label_set_text(status_label, text.c_str());
  }

  // Attention: saved network configured but not connected
  ui_set_attention(config.wifi.ssid[0] != '\0' && WiFi.status() != WL_CONNECTED);
}

// ============================================================================
// CLOCK
// ============================================================================

static void update_clock() {
  char text[12] = "--:--";
  time_t now = time(nullptr);
  if (now > 24 * 3600) {  // Clock valid (NTP synced or retained across reset)
    struct tm t;
    localtime_r(&now, &t);
    if (strcmp(config.display.timeFormat, "24h") == 0) {
      strftime(text, sizeof(text), "%H:%M", &t);
    } else {
      strftime(text, sizeof(text), "%I:%M %p", &t);
      if (text[0] == '0') memmove(text, text + 1, strlen(text));  // "05:43 PM" → "5:43 PM"
    }
  }
  if (strcmp(text, lv_label_get_text(clock_label)) != 0) {
    lv_label_set_text(clock_label, text);
    lv_label_set_text(standby_clock, text);
  }
}

// ============================================================================
// PUBLIC
// ============================================================================

// Arc dragged by touch: snap to the RPM step; loop() redraws from the new target
static void arc_changed_cb(lv_event_t *) {
  int32_t step = config.fan.rpmStep > 0 ? config.fan.rpmStep : 1;
  int32_t rpm = ((lv_arc_get_value(rpm_arc) + step / 2) / step) * step;
  fan_set_target(rpm);
}

void ui_init() {
  main_screen = lv_scr_act();
  lv_obj_set_style_bg_color(main_screen, lv_color_hex(THEME_BG_TOP), 0);
  lv_obj_set_style_bg_grad_color(main_screen, lv_color_hex(THEME_BG_BOTTOM), 0);
  lv_obj_set_style_bg_grad_dir(main_screen, LV_GRAD_DIR_VER, 0);
  lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_SCROLLABLE);

  tileview = lv_tileview_create(main_screen);
  lv_obj_set_style_bg_opa(tileview, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);
  for (int i = 0; i < PAGE_COUNT; i++) {
    int dir = (i > 0 ? LV_DIR_LEFT : 0) | (i < PAGE_COUNT - 1 ? LV_DIR_RIGHT : 0);
    lv_obj_t *tile = lv_tileview_add_tile(tileview, i, 0, (lv_dir_t)dir);
    PAGES[i].create(tile);
    // Tap on empty space returns to Main (controls handle their own taps; swipes send no CLICKED)
    if (i > 0) lv_obj_add_event_cb(tile, tap_back_cb, LV_EVENT_CLICKED, nullptr);
  }
  create_page_dots(main_screen);
  create_menu(main_screen);  // After the dots so it covers them
  create_standby_screen();
  create_config_screen();
  ui_set_target_rpm(config.fan.minRpm);
}

// ============================================================================
// QUICK SEGMENTS ON MAIN: Off + presets as ring slices inside the RPM arc
// ============================================================================
// Drawn with non-clickable lv_arc slices; taps on the Main tile are matched to a segment
// by angle and radius (seg_at). Angles are LVGL's: 0 = 3 o'clock, clockwise.

static const int SEG_COUNT = 5;
static const int SEG_START = 150;   // Left end, just below 9 o'clock
static const int SEG_SPAN = 48;     // Per segment incl. gap: 5 x 48 = 240°, open at the bottom
static const int SEG_GAP = 4;
static const int SEG_R_OUT = 94;    // Inside the RPM ring (inner edge ~102) and its knob
static const int SEG_R_IN = 60;
static const char *const seg_text[SEG_COUNT] = {LV_SYMBOL_POWER "\nOff", "Low", "Med", "High", "Max"};
static lv_obj_t *segs[SEG_COUNT];
static lv_obj_t *seg_labels[SEG_COUNT];
static int seg_pressed = -1;
static uint16_t seg_target = 0;   // Current target RPM; the matching segment stays lit

static uint16_t seg_rpm(int i) {  // Read at tap time, so Config changes apply
  const uint16_t rpm[SEG_COUNT] = {0, config.fan.presets.low, config.fan.presets.medium,
                                   config.fan.presets.high, config.fan.presets.max};
  return rpm[i];
}

// Gold while pressed, and while its speed is the current target (Off when stopped)
static void seg_highlight() {
  for (int i = 0; i < SEG_COUNT; i++) {
    bool lit = i == seg_pressed || seg_rpm(i) == seg_target;
    lv_color_t c = lit    ? lv_color_hex(THEME_GOLD)
                 : i == 0 ? lv_color_hex(THEME_OFF)
                          : lv_color_hex(THEME_PANEL);
    if (lv_obj_get_style_arc_color(segs[i], LV_PART_MAIN).full != c.full) {  // Only on change (avoids redraws)
      lv_obj_set_style_arc_color(segs[i], c, LV_PART_MAIN);
      lv_obj_set_style_text_color(seg_labels[i], lv_color_hex(lit ? THEME_ON_GOLD : THEME_TEXT), 0);
    }
  }
}

// Segment under the current touch point, or -1 (6 px slack at the band edges)
static int seg_at() {
  lv_point_t p;
  lv_indev_get_point(lv_indev_get_act(), &p);
  int dx = p.x - lv_disp_get_hor_res(nullptr) / 2;
  int dy = p.y - lv_disp_get_ver_res(nullptr) / 2;
  float r = sqrtf(dx * dx + dy * dy);
  if (r < SEG_R_IN - 6 || r > SEG_R_OUT + 6) return -1;
  int a = ((int)lroundf(atan2f(dy, dx) * RAD_TO_DEG) + 360) % 360;
  int rel = (a - SEG_START + 360) % 360;
  return rel < SEG_COUNT * SEG_SPAN ? rel / SEG_SPAN : -1;
}

// Pressed segment lights up; a swipe (PRESS_LOST) or release clears it
static void seg_press_cb(lv_event_t *e) {
  seg_pressed = lv_event_get_code(e) == LV_EVENT_PRESSED ? seg_at() : -1;
  seg_highlight();
}

static void create_segments(lv_obj_t *tile) {
  const int label_r = (SEG_R_IN + SEG_R_OUT) / 2;
  for (int i = 0; i < SEG_COUNT; i++) {
    int a0 = SEG_START + i * SEG_SPAN + SEG_GAP / 2;
    int a1 = a0 + SEG_SPAN - SEG_GAP;
    lv_obj_t *arc = lv_arc_create(tile);
    lv_obj_set_size(arc, SEG_R_OUT * 2, SEG_R_OUT * 2);
    lv_obj_center(arc);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);  // Taps go to the tile (seg_at)
    lv_obj_set_style_pad_all(arc, 0, 0);
    lv_arc_set_rotation(arc, 0);
    lv_arc_set_bg_angles(arc, a0 % 360, a1 % 360);   // Wraps past 360 for the last slice
    lv_obj_set_style_arc_width(arc, SEG_R_OUT - SEG_R_IN, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, false, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    segs[i] = arc;

    float mid = (a0 + a1) / 2.0f * DEG_TO_RAD;
    lv_obj_t *label = lv_label_create(tile);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(THEME_TEXT), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);  // Off: icon above word
    seg_labels[i] = label;
    lv_label_set_text(label, seg_text[i]);
    lv_obj_align(label, LV_ALIGN_CENTER, lroundf(label_r * cosf(mid)), lroundf(label_r * sinf(mid)));
  }
  seg_highlight();
  lv_obj_add_event_cb(tile, seg_press_cb, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(tile, seg_press_cb, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(tile, seg_press_cb, LV_EVENT_PRESS_LOST, nullptr);
}

// ============================================================================
// DOUBLE-TAP ON MAIN: stop the fan, or say it isn't running
// ============================================================================

static lv_obj_t *popup = nullptr;
static lv_timer_t *popup_timer = nullptr;

static void popup_hide_cb(lv_timer_t *) {
  lv_obj_add_flag(popup, LV_OBJ_FLAG_HIDDEN);
  lv_timer_pause(popup_timer);
}

// Short message in the centre of the screen for 1.5 s
static void show_popup(const char *text, lv_color_t bg) {
  if (!popup) {
    popup = lv_label_create(main_screen);
    lv_obj_set_style_text_font(popup, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(popup, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(popup, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(popup, 10, 0);
    lv_obj_set_style_pad_all(popup, 14, 0);
    popup_timer = lv_timer_create(popup_hide_cb, 1500, nullptr);
  }
  lv_obj_set_style_bg_color(popup, bg, 0);
  lv_label_set_text(popup, text);
  lv_obj_center(popup);
  lv_obj_clear_flag(popup, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(popup);
  lv_timer_reset(popup_timer);
  lv_timer_resume(popup_timer);
}

// Two taps within 400 ms (millis: lv_tick only approximates real time here).
// Swipes and arc drags don't count: LVGL sends no CLICKED after a scroll, and
// the arc handles its own touches.
static void main_tap_cb(lv_event_t *) {
  static unsigned long last_tap = 0;
  int seg = seg_at();
  if (seg >= 0) {  // Quick segment: set its speed (not part of a double-tap)
    last_tap = 0;
    fan_set_target(seg_rpm(seg));
    return;
  }
  unsigned long now = millis();
  if (last_tap != 0 && now - last_tap < 400) {
    last_tap = 0;
    if (fan_get_target() > 0) {
      fan_set_target(0);
      Serial.println("Double-tap: fan stopped");
      show_popup("Fan stopped", lv_color_hex(THEME_ALERT));
    } else {
      show_popup("Fan is not running", lv_color_hex(THEME_PANEL));
    }
  } else {
    last_tap = now;
  }
}

static void create_main_page(lv_obj_t *scr) {
  lv_obj_add_event_cb(scr, main_tap_cb, LV_EVENT_CLICKED, nullptr);

  // RPM arc: ends level with the bottom edges of the outer segments (Off, Max), leaving
  // the gap at the bottom clear of the status box. Drag along the ring to set speed
  // (arc only hit-tests on the ring, so swipes in the middle still change pages).
  rpm_arc = lv_arc_create(scr);
  lv_obj_set_size(rpm_arc, 228, 228);
  lv_obj_center(rpm_arc);
  lv_arc_set_rotation(rpm_arc, SEG_START + SEG_GAP / 2);                  // 152°
  lv_arc_set_bg_angles(rpm_arc, 0, SEG_COUNT * SEG_SPAN - SEG_GAP);        // 236° sweep
  lv_arc_set_range(rpm_arc, config.fan.minRpm, config.fan.maxRpm);
  lv_arc_set_value(rpm_arc, config.fan.minRpm);
  // Ring-only touch: ADV_HITTEST makes LVGL use the arc's ring hit test (off by default,
  // in which case the whole 228 px square grabs every swipe). Ext area widens the ring,
  // but only down to the segments' outer edge: 114 - 12 - 8 = 94 = SEG_R_OUT. Wider, and
  // taps on a segment's outer part set the arc (a stepped speed) instead of the preset.
  lv_obj_add_flag(rpm_arc, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_set_ext_click_area(rpm_arc, 114 - 12 - SEG_R_OUT);
  lv_obj_set_style_arc_width(rpm_arc, 12, LV_PART_MAIN);
  lv_obj_set_style_arc_width(rpm_arc, 12, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(rpm_arc, lv_color_hex(THEME_TRACK), LV_PART_MAIN);
  lv_obj_set_style_arc_color(rpm_arc, lv_color_hex(THEME_GOLD), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(rpm_arc, lv_color_hex(THEME_TEXT), LV_PART_KNOB);
  lv_obj_set_style_pad_all(rpm_arc, 2, LV_PART_KNOB);
  lv_obj_add_event_cb(rpm_arc, arc_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);

  // Clock in the arc's bottom gap, above the page dots
  clock_label = lv_label_create(scr);
  lv_obj_set_style_text_font(clock_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(clock_label, lv_color_hex(THEME_TEXT_DIM), 0);
  lv_label_set_text(clock_label, "--:--");
  lv_obj_align(clock_label, LV_ALIGN_CENTER, 0, 82);

  // Target RPM: what the knob, arc and segments set (gold, like the arc)
  rpm_label = lv_label_create(scr);
  lv_obj_set_style_text_font(rpm_label, &lv_font_montserrat_40, 0);  // 48 crowded the Off/Max segments
  lv_obj_set_style_text_color(rpm_label, lv_color_hex(THEME_GOLD), 0);
  lv_obj_align(rpm_label, LV_ALIGN_CENTER, 0, -6);

  unit_label = lv_label_create(scr);
  lv_obj_set_style_text_font(unit_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(unit_label, lv_color_hex(THEME_TEXT_DIM), 0);
  lv_label_set_text(unit_label, "RPM");
  lv_obj_align(unit_label, LV_ALIGN_CENTER, 0, 22);

  // Actual RPM from the tach, light text so it can't be mistaken for the gold target
  actual_label = lv_label_create(scr);
  lv_obj_set_style_text_font(actual_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(actual_label, lv_color_hex(THEME_TEXT), 0);
  lv_label_set_recolor(actual_label, true);  // "now" in the dim colour
  lv_label_set_text(actual_label, "");
  lv_obj_align(actual_label, LV_ALIGN_CENTER, 0, 48);

  create_segments(scr);
}

// ============================================================================
// PAGES
// ============================================================================

static void tap_back_cb(lv_event_t *) {
  ui_show_main();
}

void ui_show_main() {
  if (lv_scr_act() == config_screen && fan_autoconfig_progress() < 0) {  // Result showing: skip it
    cfg_result_until = 0;
    lv_scr_load(main_screen);
  }
  lv_obj_set_tile_id(tileview, 0, 0, LV_ANIM_ON);
}

static int current_page() {
  lv_obj_t *tile = lv_tileview_get_tile_act(tileview);  // NULL until the first scroll
  return tile ? lv_obj_get_x(tile) / lv_obj_get_width(tileview) : 0;
}

static void page_changed_cb(lv_event_t *) {
  int page = current_page();
  for (int i = 0; i < PAGE_COUNT; i++) {
    lv_obj_set_style_bg_color(page_dots[i], lv_color_hex(i == page ? THEME_GOLD : THEME_PANEL), 0);
  }
}

static void create_page_dots(lv_obj_t *parent) {
  for (int i = 0; i < PAGE_COUNT; i++) {
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, 1, 0);  // Outline: the gold dot still shows on the gold Settings page
    lv_obj_set_style_border_color(dot, lv_color_hex(THEME_BG_TOP), 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(dot, LV_ALIGN_BOTTOM_MID, (2 * i - (PAGE_COUNT - 1)) * 8, -10);  // 16 px apart, centred
    page_dots[i] = dot;
  }
  lv_obj_add_event_cb(tileview, page_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  page_changed_cb(nullptr);
}

// ============================================================================
// KNOB MENU (short press): list of PAGES; turn to choose, press or tap to go
// ============================================================================

static lv_obj_t *menu = nullptr;
static lv_obj_t *menu_items[PAGE_COUNT];
static int menu_sel = 0;

static void menu_highlight() {
  for (int i = 0; i < PAGE_COUNT; i++) {
    bool sel = i == menu_sel;
    lv_obj_set_style_bg_color(menu_items[i], lv_color_hex(sel ? THEME_GOLD : THEME_PANEL), 0);
    lv_obj_set_style_text_color(menu_items[i], lv_color_hex(sel ? THEME_ON_GOLD : THEME_TEXT), 0);
  }
  lv_obj_scroll_to_view(menu_items[menu_sel], LV_ANIM_ON);  // Long lists scroll
}

static void menu_close() {
  lv_obj_add_flag(menu, LV_OBJ_FLAG_HIDDEN);
}

static void menu_go(int page) {
  menu_close();
  lv_obj_set_tile_id(tileview, page, 0, LV_ANIM_ON);
}

static void menu_item_cb(lv_event_t *e) {
  menu_go((int)(intptr_t)lv_event_get_user_data(e));
}

static void menu_bg_cb(lv_event_t *) {
  menu_close();  // Tap outside the list
}

static void create_menu(lv_obj_t *parent) {
  menu = lv_obj_create(parent);
  lv_obj_set_size(menu, 240, 240);
  lv_obj_center(menu);
  lv_obj_set_style_radius(menu, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(menu, lv_color_hex(THEME_BG_BOTTOM), 0);
  lv_obj_set_style_bg_opa(menu, LV_OPA_90, 0);
  lv_obj_set_style_border_width(menu, 0, 0);
  lv_obj_clear_flag(menu, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(menu, menu_bg_cb, LV_EVENT_CLICKED, nullptr);

  // Not clickable, so taps between items fall through to the background (close)
  lv_obj_t *list = lv_obj_create(menu);
  lv_obj_set_size(list, 170, 180);
  lv_obj_center(list);
  lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_all(list, 0, 0);
  lv_obj_set_style_pad_row(list, 8, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(list, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(list, LV_OBJ_FLAG_CLICKABLE);

  for (int i = 0; i < PAGE_COUNT; i++) {
    lv_obj_t *btn = lv_btn_create(list);
    lv_obj_set_size(btn, 150, 40);
    lv_obj_add_event_cb(btn, menu_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_label_set_text(label, PAGES[i].name);
    lv_obj_center(label);
    menu_items[i] = btn;
  }
  menu_close();
}

bool ui_menu_open() {
  return !lv_obj_has_flag(menu, LV_OBJ_FLAG_HIDDEN);
}

void ui_menu_button() {
  if (lv_scr_act() == config_screen) {  // Auto Configure result showing: a press just dismisses it
    ui_show_main();
    return;
  }
  if (ui_menu_open()) {
    menu_go(menu_sel);
    return;
  }
  menu_sel = current_page();
  lv_obj_clear_flag(menu, LV_OBJ_FLAG_HIDDEN);
  menu_highlight();
}

void ui_menu_turn(int delta) {
  menu_sel = ((menu_sel + delta) % PAGE_COUNT + PAGE_COUNT) % PAGE_COUNT;  // Wraps
  menu_highlight();
}

static lv_obj_t *page_title(lv_obj_t *tile, const char *text) {
  lv_obj_t *title = lv_label_create(tile);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(THEME_TEXT_DIM), 0);
  lv_label_set_text(title, text);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -80);
  return title;
}

static void brightness_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
    config.display.brightness = lv_slider_get_value(brightness_slider);
    lv_label_set_text_fmt(brightness_label, "Brightness %u%%", config.display.brightness);
    applyDisplaySettings();
  } else {  // LV_EVENT_RELEASED: persist once, not on every step
    saveConfig();
  }
}

static void create_settings_page(lv_obj_t *tile) {
  page_title(tile, "Settings");

  brightness_label = lv_label_create(tile);
  lv_obj_set_style_text_color(brightness_label, lv_color_hex(THEME_TEXT), 0);
  lv_label_set_text_fmt(brightness_label, "Brightness %u%%", config.display.brightness);
  lv_obj_align(brightness_label, LV_ALIGN_CENTER, 0, -42);

  brightness_slider = lv_slider_create(tile);
  lv_obj_set_width(brightness_slider, 150);
  lv_obj_align(brightness_slider, LV_ALIGN_CENTER, 0, -14);
  lv_slider_set_range(brightness_slider, 10, 100);
  lv_slider_set_value(brightness_slider, config.display.brightness, LV_ANIM_OFF);
  lv_obj_add_event_cb(brightness_slider, brightness_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(brightness_slider, brightness_cb, LV_EVENT_RELEASED, nullptr);
  lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(THEME_PANEL), LV_PART_MAIN);
  lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(THEME_GOLD), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(THEME_GOLD), LV_PART_KNOB);

  create_status_box(tile, 22);  // IP address (web page link)

  info_label = lv_label_create(tile);
  lv_obj_set_style_text_font(info_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(info_label, lv_color_hex(THEME_TEXT_DIM), 0);
  lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(info_label, "");
  lv_obj_align(info_label, LV_ALIGN_CENTER, 0, 60);
}

static void update_info() {
  String text;
  if (WiFi.status() == WL_CONNECTED) {
    text = WiFi.SSID();
  } else if (WiFi.getMode() & WIFI_AP) {
    text = "Hotspot";
  } else {
    text = "WiFi lost";
  }
  text += String("\nMQTT ") + (mqtt_connected() ? "connected" : "not connected");
  if (text != lv_label_get_text(info_label)) {
    lv_label_set_text(info_label, text.c_str());
  }
}

static void create_standby_screen() {
  standby_screen = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(standby_screen, lv_color_black(), 0);
  lv_obj_clear_flag(standby_screen, LV_OBJ_FLAG_SCROLLABLE);

  standby_clock = lv_label_create(standby_screen);
  lv_obj_set_style_text_font(standby_clock, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(standby_clock, lv_color_hex(0x606060), 0);
  lv_label_set_text(standby_clock, "--:--");
  lv_obj_center(standby_clock);
}

// Auto Configure: gold (reversed theme) so it can't be mistaken for any other screen.
// Ring = progress; big % in the middle; cancel hint under it; live RPM and step below.
// Keep text inside y = +/-80 or so: lower down, the ring cuts across the line.
static const int CFG_DETAIL_Y = 70;         // "Step N of 31" while running
static const int CFG_RESULT_Y = 34;         // Result message: higher, it wraps onto 2-3 lines
static const uint32_t CFG_RESULT_MS = 30000; // Result stays up this long (a knob turn/press leaves sooner)

static lv_obj_t *config_label(const lv_font_t *font, int y) {
  lv_obj_t *label = lv_label_create(config_screen);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(THEME_BG_TOP), 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(label, "");
  lv_obj_align(label, LV_ALIGN_CENTER, 0, y);
  return label;
}

static void create_config_screen() {
  config_screen = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(config_screen, lv_color_hex(THEME_GOLD), 0);
  lv_obj_clear_flag(config_screen, LV_OBJ_FLAG_SCROLLABLE);

  cfg_ring = lv_arc_create(config_screen);
  lv_obj_set_size(cfg_ring, 232, 232);
  lv_obj_center(cfg_ring);
  lv_obj_clear_flag(cfg_ring, LV_OBJ_FLAG_CLICKABLE);
  lv_arc_set_rotation(cfg_ring, 270);  // Fills clockwise from 12 o'clock
  lv_arc_set_bg_angles(cfg_ring, 0, 360);
  lv_arc_set_range(cfg_ring, 0, 100);
  lv_obj_set_style_arc_width(cfg_ring, 10, LV_PART_MAIN);
  lv_obj_set_style_arc_width(cfg_ring, 10, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(cfg_ring, lv_color_hex(THEME_GOLD_DARK), LV_PART_MAIN);
  lv_obj_set_style_arc_color(cfg_ring, lv_color_hex(THEME_BG_TOP), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(cfg_ring, LV_OPA_TRANSP, LV_PART_KNOB);

  lv_label_set_text(config_label(&lv_font_montserrat_14, -70), "AUTO CONFIGURE");
  cfg_name = config_label(&lv_font_montserrat_14, -48);
  lv_label_set_long_mode(cfg_name, LV_LABEL_LONG_DOT);
  lv_obj_set_width(cfg_name, 170);
  cfg_big = config_label(&lv_font_montserrat_48, -16);
  cfg_hint = config_label(&lv_font_montserrat_14, 20);
  lv_label_set_text(cfg_hint, "Press knob to cancel");
  cfg_rpm = config_label(&lv_font_montserrat_20, 46);
  cfg_detail = config_label(&lv_font_montserrat_14, CFG_DETAIL_Y);
  lv_label_set_long_mode(cfg_detail, LV_LABEL_LONG_WRAP);  // Result messages are long
  lv_obj_set_width(cfg_detail, 170);
}

// Once a second from ui_update(): show the screen while Auto Configure runs, then its
// result for CFG_RESULT_MS, then back to Main (only if nothing else, e.g. standby, took the screen)
static void update_config_screen() {
  static bool was_running = false;
  int progress = fan_autoconfig_progress();
  if (progress >= 0) {
    if (!was_running) {
      was_running = true;
      menu_close();
      lv_label_set_text(cfg_name, fan_autoconfig_name());
      lv_obj_clear_flag(cfg_hint, LV_OBJ_FLAG_HIDDEN);
      lv_obj_align(cfg_detail, LV_ALIGN_CENTER, 0, CFG_DETAIL_Y);
    }
    if (lv_scr_act() != config_screen) lv_scr_load(config_screen);
    lv_arc_set_value(cfg_ring, progress);
    lv_label_set_text_fmt(cfg_big, "%d%%", progress);
    lv_label_set_text_fmt(cfg_rpm, "%u RPM", fan_get_rpm());
    lv_label_set_text_fmt(cfg_detail, "Step %d of %d",
                          fan_autoconfig_steps() - fan_autoconfig_step() + 1, fan_autoconfig_steps() + 1);
    return;
  }
  if (was_running) {  // Just finished: show the outcome
    was_running = false;
    const char *result = fan_autoconfig_result();
    bool saved = strncmp(result, "Saved", 5) == 0;
    lv_label_set_text(cfg_big, saved ? "Done" : strncmp(result, "Cancelled", 9) == 0 ? "Cancelled" : "Failed");
    if (saved) lv_arc_set_value(cfg_ring, 100);
    lv_label_set_text(cfg_rpm, "");
    lv_label_set_text(cfg_detail, result);
    lv_obj_align(cfg_detail, LV_ALIGN_CENTER, 0, CFG_RESULT_Y);
    lv_obj_add_flag(cfg_hint, LV_OBJ_FLAG_HIDDEN);
    cfg_result_until = millis() + CFG_RESULT_MS;
  } else if (cfg_result_until && (long)(millis() - cfg_result_until) >= 0) {
    cfg_result_until = 0;
    if (lv_scr_act() == config_screen) lv_scr_load(main_screen);
  }
}

void ui_set_standby(bool standby) {
  if (standby) menu_close();
  if (!standby) lv_obj_set_tile_id(tileview, 0, 0, LV_ANIM_OFF);  // Wake on Main page
  lv_scr_load(standby ? standby_screen : main_screen);
}

void ui_update() {
  // Actual RPM under the target while the fan turns (hidden when stopped or without a fan
  // controller). Auto Configure has its own screen.
  char actual[40] = "";
  if (fan_controller_present() && (fan_get_target() > 0 || fan_get_rpm() > 0)) {
    snprintf(actual, sizeof(actual), "#%06lX now# %u", (unsigned long)THEME_TEXT_DIM, fan_get_rpm());
  }
  if (strcmp(actual, lv_label_get_text(actual_label)) != 0) lv_label_set_text(actual_label, actual);
  update_config_screen();
  seg_highlight();  // Presets may have been changed on the web page
  if (lv_arc_get_max_value(rpm_arc) != config.fan.maxRpm) {  // Fan max RPM changed on the web page
    lv_arc_set_range(rpm_arc, config.fan.minRpm, config.fan.maxRpm);
  }
  // Brightness may have been changed on the web page (leave it alone while dragged)
  if (lv_slider_get_value(brightness_slider) != config.display.brightness && !lv_slider_is_dragged(brightness_slider)) {
    lv_slider_set_value(brightness_slider, config.display.brightness, LV_ANIM_OFF);
    lv_label_set_text_fmt(brightness_label, "Brightness %u%%", config.display.brightness);
  }
  update_clock();
  update_status_box();
  update_info();
}

void ui_set_target_rpm(uint16_t rpm) {
  seg_target = rpm;
  seg_highlight();
  lv_arc_set_value(rpm_arc, rpm);
  lv_label_set_text_fmt(rpm_label, "%u", rpm);
}
