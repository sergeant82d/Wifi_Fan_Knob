#ifndef POWER_H
#define POWER_H

// Standby: dimmed backlight + standby screen; fan target untouched.
// Requests are safe from any task (web server, LVGL callbacks); loop() applies them.
void power_request_standby(bool standby);
bool power_is_standby();

#endif
