#ifndef UI_H
#define UI_H

#include <Arduino.h>

void ui_init();                          // Build main screen (after LVGL display is registered)
void ui_update();                        // Refresh clock + network status (call ~1 s)
void ui_set_target_rpm(uint16_t rpm);    // Update arc + RPM number
void ui_set_attention(bool attention);   // Flash status box red/white while true

#endif
