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

#endif
