#ifndef THERMAL_H
#define THERMAL_H

#include <Arduino.h>

// AMG8833 8x8 thermal camera (Panasonic Grid-EYE, I2C 0x69 or 0x68, always-on 3.3 V), read from
// loop(). Shown on the LCD's Thermal page and the web Dashboard. Later: presence detection.

static const int THERMAL_N = 8;  // 8 x 8 pixels

struct ThermalFrame {
  bool ok;                          // Sensor answered at boot and the last read worked
  uint32_t seq;                     // Counts up with every new picture
  float thermistor;                 // The sensor's own temperature, C
  float px[THERMAL_N * THERMAL_N];  // C, row by row from the top left
};

void thermal_begin();               // setup(), after Wire.begin()
void thermal_update();              // loop()
void thermal_watch();               // Someone is looking: full rate (10/s) for a few seconds; any task
void thermal_get(ThermalFrame *f);  // Latest picture; safe from any task

#endif
