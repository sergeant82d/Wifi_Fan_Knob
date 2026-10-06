#include "air.h"
#include <Wire.h>
#include <VOCGasIndexAlgorithm.h>
#include <NOxGasIndexAlgorithm.h>

// All I2C here runs on the loop() task, like the EMC2101's. Wire's lock covers one call, not a
// request plus the reads after it, so two tasks on the bus could get each other's replies.
// Sensirion's driver libraries delay() inside each call (50 ms per SGP41 read), which would
// stall loop(); so the raw commands are sent here and the waits are steps of a schedule.
// Commands and formulas: SHT4x and SGP41 datasheets. Sensirion's Gas Index Algorithm turns
// the SGP41's raw signals into the VOC and NOx Indexes.

static const uint8_t SHT41_ADDR = 0x44;
static const uint8_t SGP41_ADDR = 0x59;
static const uint32_t PERIOD_MS = 1000;      // The Gas Index Algorithm expects one sample a second
static const uint32_t SHT_WAIT_MS = 10;      // High-precision measurement: 8.3 ms max
static const uint32_t SGP_WAIT_MS = 55;      // Measure or conditioning: 50 ms max
static const uint32_t CONDITION_MS = 10000;  // SGP41 conditioning after power-up (10 s max)

static VOCGasIndexAlgorithm voc_algo;
static NOxGasIndexAlgorithm nox_algo;
static portMUX_TYPE readings_mux = portMUX_INITIALIZER_UNLOCKED;
static AirReadings readings = {false, 0, 0, 0, 0, "no sensor"};

// Sensirion CRC-8 (poly 0x31, init 0xFF) over each 2-byte word
static uint8_t crc8(const uint8_t *data) {
  uint8_t crc = 0xFF;
  for (int i = 0; i < 2; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : crc << 1;
  }
  return crc;
}

static bool send(uint8_t addr, const uint8_t *data, size_t len) {
  Wire.beginTransmission(addr);
  Wire.write(data, len);
  return Wire.endTransmission() == 0;
}

// Reads count words (2 bytes + CRC each); false on a short read or a bad CRC
static bool read_words(uint8_t addr, uint16_t *words, int count) {
  uint8_t buf[6];
  size_t len = count * 3;
  if (Wire.requestFrom(addr, len) != len) return false;
  for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
  for (int i = 0; i < count; i++) {
    if (crc8(buf + 3 * i) != buf[3 * i + 2]) return false;
    words[i] = (buf[3 * i] << 8) | buf[3 * i + 1];
  }
  return true;
}

static void put_word(uint8_t *p, uint16_t w) {
  p[0] = w >> 8;
  p[1] = w & 0xFF;
  p[2] = crc8(p);
}

static void publish(const AirReadings &r) {
  portENTER_CRITICAL(&readings_mux);
  readings = r;
  portEXIT_CRITICAL(&readings_mux);
}

AirReadings air_get() {
  portENTER_CRITICAL(&readings_mux);
  AirReadings r = readings;
  portEXIT_CRITICAL(&readings_mux);
  return r;
}

void air_update() {
  enum Step { START, READ_SHT, READ_SGP };
  static Step step = START;
  static unsigned long cycle_at = 0, step_at = 0;
  static bool conditioning = false, sgp_ok = false;
  static AirReadings r = {false, 0, 0, 0, 0, "no sensor"};
  static uint16_t rh_ticks = 0x8000, t_ticks = 0x6666;  // SGP41 compensation (50 %RH, 25 C until read)
  unsigned long now = millis();

  switch (step) {
    case START: {
      if ((long)(now - cycle_at) < 0) return;
      cycle_at = (now - cycle_at > PERIOD_MS) ? now + PERIOD_MS : cycle_at + PERIOD_MS;  // Resync if late
      static const uint8_t SHT_MEASURE = 0xFD;  // Measure T + RH, high precision
      r.temp_ok = send(SHT41_ADDR, &SHT_MEASURE, 1);
      step_at = now;
      step = READ_SHT;
      return;
    }

    case READ_SHT: {
      if (now - step_at < SHT_WAIT_MS) return;
      uint16_t w[2];
      if (r.temp_ok && read_words(SHT41_ADDR, w, 2)) {
        r.temp_c = -45 + 175.0f * w[0] / 65535;
        r.humidity = constrain(-6 + 125.0f * w[1] / 65535, 0.0f, 100.0f);
        t_ticks = w[0];                                // Same scale as the SGP41's
        rh_ticks = lroundf(r.humidity * 65535 / 100);  // SGP41: %RH * 65535 / 100
      } else {
        r.temp_ok = false;
      }
      // SGP41: conditioning for the first 10 s after power-up, then measuring (both: 0x26 xx,
      // RH word, T word; conditioning takes the defaults)
      conditioning = now < CONDITION_MS;  // Powered with the board, so time since boot
      uint8_t cmd[8] = {0x26, conditioning ? (uint8_t)0x12 : (uint8_t)0x19};
      put_word(cmd + 2, conditioning ? 0x8000 : rh_ticks);
      put_word(cmd + 5, conditioning ? 0x6666 : t_ticks);
      sgp_ok = send(SGP41_ADDR, cmd, sizeof(cmd));
      step_at = now;
      step = READ_SGP;
      return;
    }

    case READ_SGP: {
      if (now - step_at < SGP_WAIT_MS) return;
      uint16_t w[2];
      if (sgp_ok && conditioning) {
        sgp_ok = read_words(SGP41_ADDR, w, 1);  // SRAW_VOC only; not fed to the algorithm
      } else if (sgp_ok && read_words(SGP41_ADDR, w, 2)) {
        r.voc = voc_algo.process(w[0]);  // 0 for the algorithm's first ~45 s after boot
        r.nox = nox_algo.process(w[1]);
      } else {
        sgp_ok = false;
      }
      if (!sgp_ok) r.voc = r.nox = 0;
      r.state = !sgp_ok && !r.temp_ok ? "no sensor"
                : sgp_ok && (conditioning || r.voc == 0) ? "warming up"
                                                         : "ok";
      publish(r);
      step = START;
      return;
    }
  }
}
