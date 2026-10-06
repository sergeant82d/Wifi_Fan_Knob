#include "leds.h"
#include "config.h"
#include "power.h"
#include <Adafruit_NeoPixel.h>

// Pins and colour order from Elecrow's example (RotaryScreen_1_28.ino)
static const uint8_t LED_PIN = 48;
static const uint8_t LED_COUNT = 5;
static Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

const char *const LED_EFFECT_NAMES[LED_EFFECT_COUNT] = {"Solid", "Flash", "Breathe", "Rainbow"};

int led_effect_from_name(const char *name) {
  for (int i = 0; i < LED_EFFECT_COUNT; i++) {
    if (strcasecmp(name, LED_EFFECT_NAMES[i]) == 0) return i;
  }
  return -1;
}

// 0xRRGGBB scaled by level (0-255)
static uint32_t dim(uint32_t c, uint32_t level) {
  return ((((c >> 16) & 0xFF) * level / 255) << 16) | ((((c >> 8) & 0xFF) * level / 255) << 8) |
         ((c & 0xFF) * level / 255);
}

void leds_begin() {
  strip.begin();
  strip.show();  // All off
}

void leds_update() {
  static unsigned long last = 0;
  static uint32_t shown[LED_COUNT];
  static bool first = true;
  unsigned long now = millis();
  if (now - last < 20) return;
  last = now;

  uint32_t px[LED_COUNT] = {};
  const auto &l = config.leds;
  if (l.on && !power_is_standby()) {
    uint32_t b = min<uint32_t>(l.brightness, LED_BRIGHTNESS_MAX);  // Never above the cap
    uint32_t s = constrain(l.speed, 1, LED_SPEED_MAX);
    switch (l.effect) {
      case LED_SOLID:
        for (auto &p : px) p = dim(l.color, b);
        break;
      case LED_FLASH: {  // On half the time: 2 s period at speed 1, 0.2 s at 10
        uint32_t period = 2000 / s;
        if (now % period < period / 2) for (auto &p : px) p = dim(l.color, b);
        break;
      }
      case LED_BREATHE: {  // Smooth fade up and down: 8 s at speed 1, 0.8 s at 10
        uint32_t period = 8000 / s;
        float v = (1 - cosf(2 * PI * (now % period) / period)) / 2;
        for (auto &p : px) p = dim(l.color, lroundf(b * v * v));  // Squared: looks even to the eye
        break;
      }
      case LED_RAINBOW: {  // Hues spread around the LEDs, turning: 10 s a turn at speed 1, 1 s at 10
        uint32_t period = 10000 / s;
        uint32_t hue = (uint64_t)(now % period) * 65536 / period;
        for (int i = 0; i < LED_COUNT; i++) {
          px[i] = dim(strip.gamma32(strip.ColorHSV(hue + i * 65536 / LED_COUNT)), b);
        }
        break;
      }
    }
  }

  if (first || memcmp(px, shown, sizeof(px)) != 0) {  // Send only changes
    for (int i = 0; i < LED_COUNT; i++) strip.setPixelColor(i, px[i]);
    strip.show();
    memcpy(shown, px, sizeof(px));
    first = false;
  }
}
