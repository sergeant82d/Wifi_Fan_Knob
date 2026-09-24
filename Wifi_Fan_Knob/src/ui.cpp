#include "ui.h"
#include "config.h"
#include "fan_control.h"
#include "mqtt.h"
#include <lvgl.h>
#include <WiFi.h>
#include <time.h>

// Main screen (240x240 round) is a horizontal tileview built from PAGES (below); swipe left from Main:
//   Main:     RPM arc · band of Off + preset segments inside it (top) · RPM number
//             (centre) · clock · status box (bottom gap)
//   Presets:  config presets + OFF (tap sets target, slides back to Main)
//   Settings: brightness slider (live; saved on release) + network/MQTT info
// Page dots sit in the arc's bottom gap. Knob turns and wake return to Main.
// Knob short press opens a menu of the pages: turn to choose, press or tap to go.
// Double-tap on Main stops the fan (or pops up "Fan is not running").
// On every other page, a tap on empty space slides back to Main.

static lv_obj_t *rpm_arc = nullptr;
static lv_obj_t *rpm_label = nullptr;
static lv_obj_t *clock_label = nullptr;
static lv_obj_t *main_screen = nullptr;
static lv_obj_t *standby_screen = nullptr;   // Dimmed: large clock only (dragon eye later)
static lv_obj_t *standby_clock = nullptr;
static lv_obj_t *status_box = nullptr;
static lv_obj_t *status_label = nullptr;
static lv_timer_t *status_flash_timer = nullptr;
static bool status_flash_red = false;

static lv_obj_t *tileview = nullptr;
static lv_obj_t *brightness_slider = nullptr;
static lv_obj_t *brightness_label = nullptr;
static lv_obj_t *info_label = nullptr;

static void create_standby_screen();
static void create_main_page(lv_obj_t *tile);
static void create_presets_page(lv_obj_t *tile);
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
  {"Presets", create_presets_page},
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

static void status_flash_cb(lv_timer_t *) {
  status_flash_red = !status_flash_red;
  if (status_flash_red) {
    status_set_colors(lv_palette_main(LV_PALETTE_RED), lv_color_white());
  } else {
    status_set_colors(lv_color_white(), lv_color_black());
  }
}

static void create_status_box(lv_obj_t *parent) {
  status_box = lv_obj_create(parent);
  lv_obj_set_size(status_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_align(status_box, LV_ALIGN_CENTER, 0, 82);  // In the arc's bottom gap, above the page dots
  lv_obj_clear_flag(status_box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_width(status_box, 0, 0);
  lv_obj_set_style_radius(status_box, 6, 0);
  lv_obj_set_style_pad_hor(status_box, 8, 0);
  lv_obj_set_style_pad_ver(status_box, 4, 0);
  lv_obj_add_flag(status_box, LV_OBJ_FLAG_EVENT_BUBBLE);  // Taps reach the page (double-tap)

  status_label = lv_label_create(status_box);
  lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
  lv_label_set_text(status_label, "Starting...");

  status_set_colors(lv_color_white(), lv_color_black());
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
    status_set_colors(lv_color_white(), lv_color_black());
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
  lv_obj_set_style_bg_color(main_screen, lv_color_black(), 0);
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
static int seg_pressed = -1;
static uint16_t seg_target = 0;   // Current target RPM; the matching segment stays lit

static uint16_t seg_rpm(int i) {  // Read at tap time, so Config changes apply
  const uint16_t rpm[SEG_COUNT] = {0, config.fan.presets.low, config.fan.presets.medium,
                                   config.fan.presets.high, config.fan.presets.max};
  return rpm[i];
}

// Cyan while pressed, and while its speed is the current target (Off when stopped)
static void seg_highlight() {
  for (int i = 0; i < SEG_COUNT; i++) {
    bool lit = i == seg_pressed || seg_rpm(i) == seg_target;
    lv_color_t c = lit         ? lv_palette_main(LV_PALETTE_CYAN)
                 : i == 0      ? lv_color_hex(0x8B1E1E)   // Off: dark red
                               : lv_color_hex(0x4A5058);  // Presets: steel grey
    lv_obj_set_style_arc_color(segs[i], c, LV_PART_MAIN);
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
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);  // Off: icon above word
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
      show_popup("Fan stopped", lv_palette_main(LV_PALETTE_RED));
    } else {
      show_popup("Fan is not running", lv_color_hex(0x404040));
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
  // in which case the whole 228 px square grabs every swipe). Ext area widens the ring.
  lv_obj_add_flag(rpm_arc, LV_OBJ_FLAG_ADV_HITTEST);
  lv_obj_set_ext_click_area(rpm_arc, 15);  // 12 px ring is a small finger target
  lv_obj_set_style_arc_width(rpm_arc, 12, LV_PART_MAIN);
  lv_obj_set_style_arc_width(rpm_arc, 12, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(rpm_arc, lv_color_hex(0x303030), LV_PART_MAIN);
  lv_obj_set_style_arc_color(rpm_arc, lv_palette_main(LV_PALETTE_CYAN), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(rpm_arc, lv_color_white(), LV_PART_KNOB);
  lv_obj_set_style_pad_all(rpm_arc, 2, LV_PART_KNOB);
  lv_obj_add_event_cb(rpm_arc, arc_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);

  clock_label = lv_label_create(scr);
  lv_obj_set_style_text_font(clock_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(clock_label, lv_color_hex(0xB0B0B0), 0);
  lv_label_set_text(clock_label, "--:--");
  lv_obj_align(clock_label, LV_ALIGN_CENTER, 0, 50);

  rpm_label = lv_label_create(scr);
  lv_obj_set_style_text_font(rpm_label, &lv_font_montserrat_40, 0);  // 48 crowded the Off/Max segments
  lv_obj_set_style_text_color(rpm_label, lv_color_white(), 0);
  lv_obj_align(rpm_label, LV_ALIGN_CENTER, 0, -4);

  lv_obj_t *unit_label = lv_label_create(scr);
  lv_obj_set_style_text_font(unit_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(unit_label, lv_color_hex(0x808080), 0);
  lv_label_set_text(unit_label, "RPM");
  lv_obj_align(unit_label, LV_ALIGN_CENTER, 0, 28);

  create_status_box(scr);

  create_segments(scr);
}

// ============================================================================
// PAGES
// ============================================================================

static void tap_back_cb(lv_event_t *) {
  ui_show_main();
}

void ui_show_main() {
  lv_obj_set_tile_id(tileview, 0, 0, LV_ANIM_ON);
}

static int current_page() {
  lv_obj_t *tile = lv_tileview_get_tile_act(tileview);  // NULL until the first scroll
  return tile ? lv_obj_get_x(tile) / lv_obj_get_width(tileview) : 0;
}

static void page_changed_cb(lv_event_t *) {
  int page = current_page();
  for (int i = 0; i < PAGE_COUNT; i++) {
    lv_obj_set_style_bg_color(page_dots[i], i == page ? lv_color_white() : lv_color_hex(0x404040), 0);
  }
}

static void create_page_dots(lv_obj_t *parent) {
  for (int i = 0; i < PAGE_COUNT; i++) {
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
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
    lv_obj_set_style_bg_color(menu_items[i], sel ? lv_palette_main(LV_PALETTE_CYAN) : lv_color_hex(0x303030), 0);
    lv_obj_set_style_text_color(menu_items[i], sel ? lv_color_black() : lv_color_white(), 0);
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
  lv_obj_set_style_bg_color(menu, lv_color_black(), 0);
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
  lv_obj_set_style_text_color(title, lv_color_hex(0xB0B0B0), 0);
  lv_label_set_text(title, text);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -80);
  return title;
}

static void preset_cb(lv_event_t *e) {
  fan_set_target((int32_t)(intptr_t)lv_event_get_user_data(e));
  ui_show_main();
}

static void preset_button(lv_obj_t *tile, const char *name, uint16_t rpm, int x, int y, int w, lv_color_t color) {
  lv_obj_t *btn = lv_btn_create(tile);
  lv_obj_set_size(btn, w, 44);
  lv_obj_align(btn, LV_ALIGN_CENTER, x, y);
  lv_obj_set_style_bg_color(btn, color, 0);
  lv_obj_add_event_cb(btn, preset_cb, LV_EVENT_CLICKED, (void *)(intptr_t)rpm);
  lv_obj_t *label = lv_label_create(btn);
  if (rpm > 0) {
    lv_label_set_text_fmt(label, "%s\n%u", name, rpm);
  } else {
    lv_label_set_text(label, name);
  }
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(label);
}

static void create_presets_page(lv_obj_t *tile) {
  page_title(tile, "Presets");
  lv_color_t c = lv_color_hex(0x1E4E5A);
  preset_button(tile, "Low", config.fan.presets.low, -45, -32, 84, c);
  preset_button(tile, "Med", config.fan.presets.medium, 45, -32, 84, c);
  preset_button(tile, "High", config.fan.presets.high, -45, 20, 84, c);
  preset_button(tile, "Max", config.fan.presets.max, 45, 20, 84, c);
  preset_button(tile, "OFF", 0, 0, 72, 110, lv_palette_main(LV_PALETTE_RED));
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
  lv_obj_set_style_text_color(brightness_label, lv_color_white(), 0);
  lv_label_set_text_fmt(brightness_label, "Brightness %u%%", config.display.brightness);
  lv_obj_align(brightness_label, LV_ALIGN_CENTER, 0, -42);

  brightness_slider = lv_slider_create(tile);
  lv_obj_set_width(brightness_slider, 150);
  lv_obj_align(brightness_slider, LV_ALIGN_CENTER, 0, -14);
  lv_slider_set_range(brightness_slider, 10, 100);
  lv_slider_set_value(brightness_slider, config.display.brightness, LV_ANIM_OFF);
  lv_obj_add_event_cb(brightness_slider, brightness_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(brightness_slider, brightness_cb, LV_EVENT_RELEASED, nullptr);

  info_label = lv_label_create(tile);
  lv_obj_set_style_text_font(info_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(info_label, lv_color_hex(0x909090), 0);
  lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(info_label, "");
  lv_obj_align(info_label, LV_ALIGN_CENTER, 0, 40);
}

static void update_info() {
  String text;
  if (WiFi.status() == WL_CONNECTED) {
    text = WiFi.localIP().toString() + "\n" + WiFi.SSID();
  } else if (WiFi.getMode() & WIFI_AP) {
    text = WiFi.softAPIP().toString() + "\nHotspot";
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

void ui_set_standby(bool standby) {
  if (standby) menu_close();
  if (!standby) lv_obj_set_tile_id(tileview, 0, 0, LV_ANIM_OFF);  // Wake on Main page
  lv_scr_load(standby ? standby_screen : main_screen);
}

void ui_update() {
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
