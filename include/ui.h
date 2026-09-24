#ifndef UI_H
#define UI_H

#include <Arduino.h>

void ui_init();                          // Build main screen (after LVGL display is registered)
void ui_update();                        // Refresh clock + network status (call ~1 s)
void ui_set_target_rpm(uint16_t rpm);    // Update arc + RPM number
void ui_set_attention(bool attention);   // Flash status box red/white while true
void ui_set_standby(bool standby);       // Switch between main and standby screens
void ui_show_main();                     // Slide back to the Main page
bool ui_menu_open();                     // Knob page menu showing?
void ui_menu_button();                   // Short press: open menu, or go to the chosen page
void ui_menu_turn(int delta);            // Knob turn while the menu is open

#endif
