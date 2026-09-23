#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include <Arduino.h>

bool fan_init();                        // Probe EMC2101 on Wire (call after Wire.begin)
bool fan_controller_present();          // EMC2101 answered at boot

// Target RPM, clamped to config.fan.minRpm..maxRpm. Safe to call from any task;
// the UI picks up changes in loop(). Not yet applied to hardware (needs EMC2101).
void fan_set_target(int32_t rpm);
uint16_t fan_get_target();

#endif
