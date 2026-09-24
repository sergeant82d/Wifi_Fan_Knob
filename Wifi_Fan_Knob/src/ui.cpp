#include "ui.h"
#include "config.h"
#include "fan_control.h"
#include "mqtt.h"
#include <lvgl.h>
#include <WiFi.h>
#include <time.h>

// Main screen (240x240 round) is a horizontal tileview; swipe left from Main:
//   0 Main:     clock (top) · RPM number (centre) · status box · 270° RPM arc
//   1 Presets:  config presets + OFF (tap sets target, slides back to Main)
//   2 Settings: brightness slider (live; saved on release) + network/MQTT info
// Page dots sit in the arc's bottom gap. Knob turns and wake return to Main.

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

static const int PAGE_COUNT = 3;
static lv_obj_t *tileview = nullptr;
static lv_obj_t *page_dots[PAGE_COUNT];
static lv_obj_t *brightness_slider = nullptr;
static lv_obj_t *brightness_label = nullptr;
static lv_obj_t *info_label = nullptr;

static void create_standby_screen();
static void create_presets_page(lv_obj_t *tile);
static void create_settings_page(lv_obj_t *tile);
static void create_page_dots(lv_obj_t *parent);

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
  lv_obj_align(status_box, LV_ALIGN_CENTER, 0, 72);
  lv_obj_clear_flag(status_box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_width(status_box, 0, 0);
  lv_obj_set_style_radius(status_box, 6, 0);
  lv_obj_set_style_pad_hor(status_box, 8, 0);
  lv_obj_set_style_pad_ver(status_box, 4, 0);

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
  lv_obj_t *scr = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_RIGHT);  // Main page
  create_presets_page(lv_tileview_add_tile(tileview, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT));
  create_settings_page(lv_tileview_add_tile(tileview, 2, 0, LV_DIR_LEFT));
  create_page_dots(main_screen);

  // RPM arc: 270° sweep with the gap at the bottom. Drag along the ring to set speed
  // (arc only hit-tests on the ring, so swipes in the middle still change pages).
  rpm_arc = lv_arc_create(scr);
  lv_obj_set_size(rpm_arc, 228, 228);
  lv_obj_center(rpm_arc);
  lv_arc_set_rotation(rpm_arc, 135);
  lv_arc_set_bg_angles(rpm_arc, 0, 270);
  lv_arc_set_range(rpm_arc, config.fan.minRpm, config.fan.maxRpm);
  lv_arc_set_value(rpm_arc, config.fan.minRpm);
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
  lv_obj_align(clock_label, LV_ALIGN_CENTER, 0, -58);

  rpm_label = lv_label_create(scr);
  lv_obj_set_style_text_font(rpm_label, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(rpm_label, lv_color_white(), 0);
  lv_obj_align(rpm_label, LV_ALIGN_CENTER, 0, -4);

  lv_obj_t *unit_label = lv_label_create(scr);
  lv_obj_set_style_text_font(unit_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(unit_label, lv_color_hex(0x808080), 0);
  lv_label_set_text(unit_label, "RPM target");
  lv_obj_align(unit_label, LV_ALIGN_CENTER, 0, 30);

  create_status_box(scr);
  create_standby_screen();
  ui_set_target_rpm(config.fan.minRpm);
}

// ============================================================================
// PAGES
// ============================================================================

void ui_show_main() {
  lv_obj_set_tile_id(tileview, 0, 0, LV_ANIM_ON);
}

static void page_changed_cb(lv_event_t *) {
  lv_obj_t *tile = lv_tileview_get_tile_act(tileview);  // NULL until the first scroll
  int page = tile ? lv_obj_get_x(tile) / lv_obj_get_width(tileview) : 0;
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
    lv_obj_align(dot, LV_ALIGN_BOTTOM_MID, (i - 1) * 16, -10);
    page_dots[i] = dot;
  }
  lv_obj_add_event_cb(tileview, page_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  page_changed_cb(nullptr);
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
  if (!standby) lv_obj_set_tile_id(tileview, 0, 0, LV_ANIM_OFF);  // Wake on Main page
  lv_scr_load(standby ? standby_screen : main_screen);
}

void ui_update() {
  update_clock();
  update_status_box();
  update_info();
}

void ui_set_target_rpm(uint16_t rpm) {
  lv_arc_set_value(rpm_arc, rpm);
  lv_label_set_text_fmt(rpm_label, "%u", rpm);
}
