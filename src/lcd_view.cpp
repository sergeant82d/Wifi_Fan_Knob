#include "lcd_view.h"
#include <lvgl.h>
#include <esp_heap_caps.h>
#include <freertos/semphr.h>

static const uint32_t CAPTURE_MS = 1000;   // At most one capture a second
static const uint32_t WATCH_MS = 5000;     // Keep capturing this long after the last request
static const uint32_t SEND_MAX_MS = 15000; // A send that never reported its end frees the frame anyway

static uint16_t *back = nullptr;    // Being captured (loop)
static uint16_t *ready = nullptr;   // Last complete frame (sent to the web)
static SemaphoreHandle_t lock = nullptr;
static volatile uint32_t watched_at = 0;  // millis() of the last request; 0 = never
static uint32_t captured_at = 0;
static bool have_frame = false;
static uint32_t frame_seq = 0;
static bool sending = false;
static uint32_t send_start = 0;
static bool eye_capturing = false;
static int eye_rows = 0;

bool lcd_view_init() {
  back = (uint16_t *)heap_caps_malloc(LCD_VIEW_BYTES, MALLOC_CAP_SPIRAM);
  ready = (uint16_t *)heap_caps_malloc(LCD_VIEW_BYTES, MALLOC_CAP_SPIRAM);
  lock = xSemaphoreCreateMutex();
  return back && ready && lock;
}

bool lcd_view_capture_due() {
  return back && watched_at && millis() - watched_at < WATCH_MS && millis() - captured_at >= CAPTURE_MS;
}

// back -> ready, unless ready is being sent (then this frame is dropped; the next one comes)
static void publish() {
  captured_at = millis();
  xSemaphoreTake(lock, portMAX_DELAY);
  if (sending && millis() - send_start > SEND_MAX_MS) sending = false;
  if (!sending) {
    memcpy(ready, back, LCD_VIEW_BYTES);
    have_frame = true;
    frame_seq++;
  }
  xSemaphoreGive(lock);
}

void lcd_view_capture_lvgl() {
  lv_img_dsc_t dsc;
  if (lv_snapshot_take_to_buf(lv_scr_act(), LV_IMG_CF_TRUE_COLOR, &dsc, back, LCD_VIEW_BYTES) == LV_RES_OK) {
    publish();
  } else {
    captured_at = millis();  // Try again next time
  }
}

void lcd_view_eye_begin() {
  eye_capturing = back != nullptr;
  eye_rows = 0;
}

void lcd_view_eye_row(int y, const uint16_t *row) {
  if (!eye_capturing || y < 0 || y >= LCD_VIEW_SIZE) return;
  memcpy(back + y * LCD_VIEW_SIZE, row, LCD_VIEW_SIZE * 2);
  eye_rows++;
}

void lcd_view_eye_end() {
  if (eye_capturing && eye_rows == LCD_VIEW_SIZE) publish();
  eye_capturing = false;
}

const uint8_t *lcd_view_lock(uint32_t *seq) {
  watched_at = millis();
  if (!lock) return nullptr;
  xSemaphoreTake(lock, portMAX_DELAY);
  if (sending && millis() - send_start > SEND_MAX_MS) sending = false;
  bool ok = have_frame && !sending;
  if (ok) {
    sending = true;
    send_start = millis();
    *seq = frame_seq;
  }
  xSemaphoreGive(lock);
  return ok ? (const uint8_t *)ready : nullptr;
}

void lcd_view_unlock() {
  xSemaphoreTake(lock, portMAX_DELAY);
  sending = false;
  xSemaphoreGive(lock);
}
