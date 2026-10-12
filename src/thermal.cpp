#include "thermal.h"
#include <Wire.h>

// Raw register reads (Grid-EYE datasheet), no library. Same task as the other I2C users (see
// air.cpp for why). The 128 pixel bytes are read in four pieces, one per loop() pass, so a
// picture never holds loop() for more than a few ms (the bus runs at 100 kHz).
// The sensor makes 10 pictures a second by itself; they are read at that rate only while
// someone looks (LCD Thermal page or web Dashboard), otherwise once a second.

static const uint8_t REG_PCTL = 0x00;   // Power control: 0x00 = normal
static const uint8_t REG_RST = 0x01;    // 0x3F = initial reset
static const uint8_t REG_FPSC = 0x02;   // Frame rate: 0x00 = 10 per second
static const uint8_t REG_TTHL = 0x0E;   // Thermistor, 2 bytes
static const uint8_t REG_PIXEL = 0x80;  // Pixels, 64 x 2 bytes, row by row
static const uint32_t WATCHED_MS = 100;
static const uint32_t IDLE_MS = 1000;
static const uint32_t WATCH_HOLD_MS = 3000;
static const uint32_t START_MS = 100;   // After the reset, before the first picture
static const int CHUNK = 32;            // Bytes per read
// Picture orientation, applied here so the LCD and the web page agree. As mounted, left and
// right are correct and up and down were reversed (user, hand wave, 2026-10-12).
static const bool FLIP_X = false;
static const bool FLIP_Y = true;

static uint8_t addr = 0;                // 0 = not found
static volatile uint32_t watch_at = 0;  // millis() of the last thermal_watch()
static portMUX_TYPE frame_mux = portMUX_INITIALIZER_UNLOCKED;
static ThermalFrame frame = {};

static bool write_reg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

static bool read_regs(uint8_t reg, uint8_t *buf, size_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, len) != len) return false;
  for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

static void publish(const ThermalFrame &f) {
  portENTER_CRITICAL(&frame_mux);
  frame = f;
  portEXIT_CRITICAL(&frame_mux);
}

void thermal_get(ThermalFrame *f) {
  portENTER_CRITICAL(&frame_mux);
  *f = frame;
  portEXIT_CRITICAL(&frame_mux);
}

void thermal_watch() {
  watch_at = millis();
}

void thermal_begin() {
  for (uint8_t a : {0x69, 0x68}) {  // Adafruit's default, then the other address
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      addr = a;
      break;
    }
  }
  if (addr && !(write_reg(REG_PCTL, 0x00) && write_reg(REG_RST, 0x3F) && write_reg(REG_FPSC, 0x00))) addr = 0;
  if (addr) {
    Serial.printf("[THERMAL] AMG8833 ready at 0x%02X\n", addr);
  } else {
    Serial.println("[THERMAL] AMG8833 not found");
  }
}

void thermal_update() {
  static int chunk = -1;  // Piece being read; -1 = waiting for the next picture
  static unsigned long next_at = 0;
  static bool started = false;
  static uint8_t raw[THERMAL_N * THERMAL_N * 2];
  static ThermalFrame f = {};
  if (!addr) return;
  unsigned long now = millis();
  if (!started) {
    started = true;
    next_at = now + START_MS;
  }
  if (chunk < 0) {
    if ((long)(now - next_at) < 0) return;
    next_at = now + (now - watch_at < WATCH_HOLD_MS ? WATCHED_MS : IDLE_MS);
    chunk = 0;
  }
  bool ok = read_regs(REG_PIXEL + chunk * CHUNK, raw + chunk * CHUNK, CHUNK);
  if (ok && ++chunk < (int)sizeof(raw) / CHUNK) return;  // More pieces on the next passes
  chunk = -1;

  uint8_t t[2];
  ok = ok && read_regs(REG_TTHL, t, 2);
  f.ok = ok;
  if (ok) {
    int16_t th = t[0] | ((t[1] & 0x07) << 8);  // 12-bit sign and magnitude, 0.0625 C
    f.thermistor = (t[1] & 0x08 ? -th : th) * 0.0625f;
    for (int y = 0; y < THERMAL_N; y++) {
      for (int x = 0; x < THERMAL_N; x++) {
        int i = y * THERMAL_N + x;
        int16_t v = raw[2 * i] | ((raw[2 * i + 1] & 0x0F) << 8);  // 12-bit two's complement, 0.25 C
        if (v & 0x800) v -= 0x1000;
        int sx = FLIP_X ? THERMAL_N - 1 - x : x;
        int sy = FLIP_Y ? THERMAL_N - 1 - y : y;
        f.px[sy * THERMAL_N + sx] = v * 0.25f;
      }
    }
    f.seq++;
  }
  publish(f);
}
