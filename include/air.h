#ifndef AIR_H
#define AIR_H

#include <Arduino.h>

// SHT41 (temperature, humidity) + SGP41 (VOC, NOx) on the main I2C bus, read once a second
// from loop(). The sensors are on always-on 3.3 V (user, 2026-10-06), so they read in standby too.

struct AirReadings {
  bool temp_ok;       // temp_c and humidity valid
  float temp_c;
  float humidity;     // %RH
  int32_t voc;        // VOC Index 1-500 (100 = this room's normal); 0 = not ready
  int32_t nox;        // NOx Index 1-500 (1 = normal); 0 = not ready
  const char *state;  // "ok", "warming up", "no sensor"
};

void air_update();       // loop(): one non-blocking step of the read sequence
AirReadings air_get();   // Latest readings; safe from any task

#endif
