#ifndef POWER_H
#define POWER_H

// Display modes work like radio buttons: exactly one of Active, Screensaver or Standby.
// Standby: dimmed eye, fan target 0, external power off. Screensaver: eye, fan keeps running.
// Requests are safe from any task (web server, MQTT, LVGL callbacks); loop() applies them.
enum PowerMode { POWER_ACTIVE, POWER_SCREENSAVER, POWER_STANDBY };
void power_request_mode(PowerMode mode);
void power_request_standby(bool standby);  // true = Standby, false = Active (wake)
void power_request_screensaver(bool on);   // true = Screensaver (also leaves standby), false = Active
bool power_is_standby();
bool power_screensaver_on();               // Eye showing while Active (idle)
bool power_standby_prompt_on();            // LCD asking "Keep the fan running?" before standby

// Peripheral power switch (GPIO 4): on while awake, off in standby.
// Level (active HIGH/LOW) comes from config.power.activeHigh.
bool power_peripherals_on();
void applyPowerSettings();              // Re-drive the pin after the level setting changes

#endif
