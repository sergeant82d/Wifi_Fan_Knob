#include "ui.h"
#include "config.h"
#include <lvgl.h>
#include <WiFi.h>
#include <time.h>

// Main screen (240x240 round):
//   clock (top) · RPM number + "RPM target" (centre) · status box (bottom)
//   270° arc around the edge showing target RPM

static lv_obj_t *rpm_arc = nullptr;
static lv_obj_t *rpm_label = nullptr;
static lv_obj_t *clock_label = nullptr;
static lv_obj_t *status_box = nullptr;
static lv_obj_t *status_label = nullptr;
static lv_timer_t *status_flash_timer = nullptr;
static bool status_flash_red = false;

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
  }
}

// ============================================================================
// PUBLIC
// ============================================================================

void ui_init() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  // RPM arc: 270° sweep with the gap at the bottom; display only (no knob, not touchable)
  rpm_arc = lv_arc_create(scr);
  lv_obj_set_size(rpm_arc, 228, 228);
  lv_obj_center(rpm_arc);
  lv_arc_set_rotation(rpm_arc, 135);
  lv_arc_set_bg_angles(rpm_arc, 0, 270);
  lv_arc_set_range(rpm_arc, config.fan.minRpm, config.fan.maxRpm);
  lv_arc_set_value(rpm_arc, config.fan.minRpm);
  lv_obj_remove_style(rpm_arc, nullptr, LV_PART_KNOB);
  lv_obj_clear_flag(rpm_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(rpm_arc, 12, LV_PART_MAIN);
  lv_obj_set_style_arc_width(rpm_arc, 12, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(rpm_arc, lv_color_hex(0x303030), LV_PART_MAIN);
  lv_obj_set_style_arc_color(rpm_arc, lv_palette_main(LV_PALETTE_CYAN), LV_PART_INDICATOR);

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
  ui_set_target_rpm(config.fan.minRpm);
}

void ui_update() {
  update_clock();
  update_status_box();
}

void ui_set_target_rpm(uint16_t rpm) {
  lv_arc_set_value(rpm_arc, rpm);
  lv_label_set_text_fmt(rpm_label, "%u", rpm);
}
