#ifndef LEDS_H
#define LEDS_H

#include <Arduino.h>

// RGB LEDs: 5 x WS2812 on GPIO 48 (Elecrow board). Settings in config.leds, changed from the
// web Home tab and Home Assistant; loop() draws the effect. Stays on in standby (user 2026-10-07).

#define LED_BRIGHTNESS_MAX 100  // Firmware cap, of 255; also the most the user can set (user, 2026-10-06)
#define LED_SPEED_MAX 10        // Effect speed 1-10

enum LedEffect { LED_SOLID, LED_FLASH, LED_BREATHE, LED_RAINBOW, LED_EFFECT_COUNT };
extern const char *const LED_EFFECT_NAMES[LED_EFFECT_COUNT];  // "Solid", ... (web + HA)
int led_effect_from_name(const char *name);                    // -1 if unknown

void leds_begin();   // setup(): all LEDs off
void leds_update();  // loop(): draw the current effect (at most 50 times a second)

#endif
