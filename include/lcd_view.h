#ifndef LCD_VIEW_H
#define LCD_VIEW_H

#include <Arduino.h>

// Web LCD view: a copy of the real screen for GET /api/lcd (240x240 RGB565, little-endian,
// 115,200 bytes). The screen can't be read back (no MISO), so frames are copied on their
// way to it: LVGL screens by lv_snapshot, the eye line by line from its renderers.
// Captures run only while someone is watching (a request in the last few seconds).
static const int LCD_VIEW_SIZE = 240;
static const size_t LCD_VIEW_BYTES = LCD_VIEW_SIZE * LCD_VIEW_SIZE * 2;

bool lcd_view_init();                   // setup(): buffers in PSRAM; false if no memory

// loop() side
bool lcd_view_capture_due();            // A viewer is watching and the last capture is old enough
void lcd_view_capture_lvgl();           // Snapshot of the active LVGL screen
void lcd_view_eye_begin();              // The next eye frame is captured...
void lcd_view_eye_row(int y, const uint16_t *row);  // ...row by row (renderers call this for every row)
void lcd_view_eye_end();                // ...and published if it was drawn in full

// Web side (AsyncTCP task)
const uint8_t *lcd_view_lock(uint32_t *seq);  // Latest frame, held until unlock; nullptr if none or busy
void lcd_view_unlock();

#endif
