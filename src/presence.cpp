#include "presence.h"
#include <Wire.h>
#include <Adafruit_APDS9999.h>

// The sensor measures by itself (proximity every 100 ms, light every 100 ms by default); loop()
// only reads its result registers. Adafruit's library has no delays, so this doesn't stall loop().
// Same task as the other I2C users (see air.cpp for why).

static const uint32_t PROX_READ_MS = 200;
static const uint32_t LUX_READ_MS = 1000;

static Adafruit_APDS9999 apds;
static bool found = false;
static portMUX_TYPE readings_mux = portMUX_INITIALIZER_UNLOCKED;
static PresenceReadings readings = {false, 0, 0};

void presence_begin() {
  // The chip is always powered, so it keeps settings across board restarts: set the proximity
  // LED to the datasheet's reset values (PS_VCSEL 0x36: 60 kHz, current bits 110; 8 pulses).
  // Full power (25 mA, 255 pulses) was tested 2026-10-06 and still couldn't see a seated user.
  found = apds.begin(0x52, &Wire) && apds.setProxResolution(APDS9999_PROX_RES_11BIT) &&
          apds.setLEDFrequency(APDS9999_LED_FREQ_60KHZ) && apds.setLEDCurrent((apds9999_led_current_t)0x06) &&
          apds.setLEDPulses(8) && apds.enableProximitySensor(true) && apds.enableLightSensor(true);
  Serial.println(found ? "[PRESENCE] APDS-9999 ready" : "[PRESENCE] APDS-9999 not found");
}

PresenceReadings presence_get() {
  portENTER_CRITICAL(&readings_mux);
  PresenceReadings r = readings;
  portEXIT_CRITICAL(&readings_mux);
  return r;
}

void presence_update() {
  static unsigned long prox_at = 0, lux_at = 0;
  static PresenceReadings r = {false, 0, 0};
  if (!found) return;
  unsigned long now = millis();
  if (now - prox_at < PROX_READ_MS) return;
  prox_at = now;

  uint16_t prox;
  r.ok = apds.readProximity(&prox);
  if (r.ok) r.prox = prox;
  if (r.ok && now - lux_at >= LUX_READ_MS) {
    lux_at = now;
    uint32_t red, green, blue, ir;
    r.ok = apds.getRGBIRData(&red, &green, &blue, &ir);
    if (r.ok) r.lux = apds.calculateLux(green);  // Green channel = the ALS (eye-like) response
  }
  portENTER_CRITICAL(&readings_mux);
  readings = r;
  portEXIT_CRITICAL(&readings_mux);
}
