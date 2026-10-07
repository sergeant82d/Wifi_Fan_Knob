#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_HOR_RES_MAX 240
#define LV_VER_RES_MAX 240
#define LV_COLOR_DEPTH 16
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0
#define LV_FONT_MONTSERRAT_20 1     // Clock
#define LV_FONT_MONTSERRAT_40 1     // RPM number
#define LV_FONT_MONTSERRAT_48 1     // Standby clock (unused screen)
#define LV_USE_QRCODE 1             // QR code page
#define LV_USE_SNAPSHOT 1           // Web LCD view (lcd_view.cpp)

// LVGL reads the real clock. It used to get lv_tick_inc(5) per loop() pass, which ran at 20-80 %
// of real time while a swipe redrew the screen (a pass then took up to ~240 ms; measured
// 2026-10-07), so swipes and their touch sampling slowed down with it.
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#endif