#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include <Arduino.h>

bool fan_init();                        // Probe EMC2101 on Wire (after Wire.begin + peripheral power on)
bool fan_controller_present();          // EMC2101 answered at last power-up
void fan_power_lost();                  // Peripheral power cut: EMC2101 unavailable until fan_init()

// Target RPM, clamped to config.fan.minRpm..maxRpm. Safe to call from any task;
// the UI picks up changes in loop(). A non-zero target wakes from standby.
void fan_set_target(int32_t rpm);
uint16_t fan_get_target();

void fan_update();                      // loop() only: apply the target to the EMC2101, read the tach
uint16_t fan_get_rpm();                 // Measured RPM (tach, updated once a second; 0 = stopped/absent)

// Auto configure: measure the fan at every Fan Setting and save the result as a fan profile
// (slot -1 = first free). Takes ~2 minutes; the fan's normal target is ignored meanwhile.
// Safe to call from any task; loop() does the work.
bool fan_autoconfig_start(int slot, const char *name);  // false if running, no controller or standby
void fan_autoconfig_cancel();
int fan_autoconfig_progress();          // -1 not running, else 0-99 %
int fan_autoconfig_step();              // Fan Setting being measured (FAN_STEPS down to 0)
int fan_autoconfig_steps();             // FAN_STEPS
const char *fan_autoconfig_name();      // Name of the profile being made
const char *fan_autoconfig_result();    // Last outcome ("" if none yet)

#endif
