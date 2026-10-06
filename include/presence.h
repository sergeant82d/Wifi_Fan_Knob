#ifndef PRESENCE_H
#define PRESENCE_H

#include <Arduino.h>

// APDS-9999 (0x52, always-on 3.3 V): infrared proximity and ambient light, read from loop().
// Proximity is reflected IR from whatever is in front of it: it sees how close something is,
// not motion across a room.

struct PresenceReadings {
  bool ok;         // Sensor answered at boot and the last read worked
  uint16_t prox;   // 0-2047 (11-bit), higher = closer
  float lux;       // Ambient light
};

void presence_begin();             // setup(), after Wire.begin()
void presence_update();            // loop()
PresenceReadings presence_get();   // Latest readings; safe from any task

#endif
