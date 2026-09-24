#ifndef POWER_H
#define POWER_H

// Standby: dimmed backlight + standby screen; fan target set to 0.
// Requests are safe from any task (web server, LVGL callbacks); loop() applies them.
void power_request_standby(bool standby);
bool power_is_standby();
bool power_screensaver_on();              // Dragon eye showing while Active (idle)

// Peripheral power switch (GPIO 4): on while awake, off in standby.
// Level (active HIGH/LOW) comes from config.power.activeHigh.
bool power_peripherals_on();
void applyPowerSettings();              // Re-drive the pin after the level setting changes

#endif
