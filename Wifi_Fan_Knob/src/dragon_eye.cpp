// Dragon eye, ported from "Uncanny Eyes" by Phil Burgess / Paint Your Dragon for
// Adafruit Industries (MIT license), via Bodmer's TFT_eSPI Animated_Eyes example.
// Changes: single eye; LovyanGFX output; 128x128 eye drawn at 2x and cropped to
// 240x240; one frame per call (the original's blocking iris loop is replaced by a
// non-blocking iris ramp).

#include "dragon_eye.h"
#include <Arduino.h>

#define SYMMETRICAL_EYELID  // Single centred eye: left/right symmetrical lids
#include "eyes/dragonEye.h"  // SCLERA_*, IRIS_*, SCREEN_* (128), tables

static lgfx::LGFX_Device *tft = nullptr;

// 128 px eye at 2x = 256; crop 4 source px each edge → 240
static const int SCALE = 2;
static const int CROP = (SCREEN_WIDTH * SCALE - 240) / (2 * SCALE);
static const int OUT = (SCREEN_WIDTH - 2 * CROP) * SCALE;  // 240
static uint16_t line_buf[OUT];

// Ease in/out curve for eye movements (3t^2 - 2t^3), from the original
static const uint8_t ease[] = {
  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  2,  2,  2,  3,
  3,  3,  4,  4,  4,  5,  5,  6,  6,  7,  7,  8,  9,  9, 10, 10,
  11, 12, 12, 13, 14, 15, 15, 16, 17, 18, 18, 19, 20, 21, 22, 23,
  24, 25, 26, 27, 27, 28, 29, 30, 31, 33, 34, 35, 36, 37, 38, 39,
  40, 41, 42, 44, 45, 46, 47, 48, 50, 51, 52, 53, 54, 56, 57, 58,
  60, 61, 62, 63, 65, 66, 67, 69, 70, 72, 73, 74, 76, 77, 78, 80,
  81, 83, 84, 85, 87, 88, 90, 91, 93, 94, 96, 97, 98, 100, 101, 103,
  104, 106, 107, 109, 110, 112, 113, 115, 116, 118, 119, 121, 122, 124, 125, 127,
  128, 130, 131, 133, 134, 136, 137, 139, 140, 142, 143, 145, 146, 148, 149, 151,
  152, 154, 155, 157, 158, 159, 161, 162, 164, 165, 167, 168, 170, 171, 172, 174,
  175, 177, 178, 179, 181, 182, 183, 185, 186, 188, 189, 190, 192, 193, 194, 195,
  197, 198, 199, 201, 202, 203, 204, 205, 207, 208, 209, 210, 211, 213, 214, 215,
  216, 217, 218, 219, 220, 221, 222, 224, 225, 226, 227, 228, 228, 229, 230, 231,
  232, 233, 234, 235, 236, 237, 237, 238, 239, 240, 240, 241, 242, 243, 243, 244,
  245, 245, 246, 246, 247, 248, 248, 249, 249, 250, 250, 251, 251, 251, 252, 252,
  252, 253, 253, 253, 254, 254, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255
};

// Blink state machine
enum { NOBLINK, ENBLINK, DEBLINK };
static struct {
  uint8_t state = NOBLINK;
  uint32_t duration = 0;
  uint32_t startTime = 0;
} blink;

void eye_begin(lgfx::LGFX_Device *display) {
  tft = display;
}

// Renders the eye. Inputs are sclera offsets of the 128x128 view window plus
// eyelid thresholds; same pixel logic as the original drawEye().
static void draw_eye(uint32_t iScale, uint32_t scleraX, uint32_t scleraY, uint32_t uT, uint32_t lT) {
  tft->startWrite();
  tft->setAddrWindow(0, 0, OUT, OUT);

  uint32_t scleraXsave = scleraX + CROP;
  scleraY += CROP;
  int32_t irisY = scleraY - (SCLERA_HEIGHT - IRIS_HEIGHT) / 2;

  for (uint32_t screenY = CROP; screenY < SCREEN_HEIGHT - CROP; screenY++, scleraY++, irisY++) {
    uint32_t sx = scleraXsave;
    int32_t irisX = scleraXsave - (SCLERA_WIDTH - IRIS_WIDTH) / 2;
    uint16_t *out = line_buf;
    for (uint32_t screenX = CROP; screenX < SCREEN_WIDTH - CROP; screenX++, sx++, irisX++) {
      uint32_t p;
      if ((lower[screenY * SCREEN_WIDTH + screenX] <= lT) ||
          (upper[screenY * SCREEN_WIDTH + screenX] <= uT)) {  // Covered by eyelid
        p = 0;
      } else if ((irisY < 0) || (irisY >= IRIS_HEIGHT) ||
                 (irisX < 0) || (irisX >= IRIS_WIDTH)) {      // In sclera
        p = sclera[scleraY * SCLERA_WIDTH + sx];
      } else {                                                // Maybe iris
        p = polar[irisY * IRIS_WIDTH + irisX];                // Polar angle/dist
        uint32_t d = (iScale * (p & 0x7F)) / 128;             // Distance (Y)
        if (d < IRIS_MAP_HEIGHT) {
          uint32_t a = (IRIS_MAP_WIDTH * (p >> 7)) / 512;     // Angle (X)
          p = iris[d * IRIS_MAP_WIDTH + a];
        } else {
          p = sclera[scleraY * SCLERA_WIDTH + sx];
        }
      }
      *out++ = p;  // 2x horizontally
      *out++ = p;
    }
    tft->writePixels((lgfx::rgb565_t *)line_buf, OUT);  // 2x vertically
    tft->writePixels((lgfx::rgb565_t *)line_buf, OUT);
  }
  tft->endWrite();
}

// Iris size: non-blocking ramp between random targets (original used a blocking
// recursive "split" that ran for ~10 s)
static uint16_t next_iris(uint32_t t) {
  static uint16_t from = (IRIS_MIN + IRIS_MAX) / 2, to = from;
  static uint32_t start = 0, duration = 1;
  uint32_t dt = t - start;
  if (dt >= duration) {
    from = to;
    to = random(IRIS_MIN, IRIS_MAX);
    start = t;
    duration = random(500000, 2500000);
    dt = 0;
  }
  return from + (int32_t)(to - from) * (int32_t)dt / (int32_t)duration;
}

void eye_frame() {
  if (!tft) return;
  uint32_t t = micros();
  int16_t eyeX, eyeY;

  // Autonomous eye motion: move to a random point, hold, repeat
  static bool inMotion = false;
  static int16_t oldX = 512, oldY = 512, newX = 512, newY = 512;
  static uint32_t moveStart = 0;
  static int32_t moveDuration = 0;
  int32_t dt = t - moveStart;
  if (inMotion) {
    if (dt >= moveDuration) {
      inMotion = false;
      moveDuration = random(3000000);  // 0-3 s hold
      moveStart = t;
      eyeX = oldX = newX;
      eyeY = oldY = newY;
    } else {
      int16_t e = ease[255 * dt / moveDuration] + 1;
      eyeX = oldX + (((newX - oldX) * e) / 256);
      eyeY = oldY + (((newY - oldY) * e) / 256);
    }
  } else {
    eyeX = oldX;
    eyeY = oldY;
    if (dt > moveDuration) {
      int16_t dx, dy;
      do {  // New destination inside the circle
        newX = random(1024);
        newY = random(1024);
        dx = (newX * 2) - 1023;
        dy = (newY * 2) - 1023;
      } while ((uint32_t)(dx * dx + dy * dy) > (1023u * 1023u));
      moveDuration = random(72000, 144000);  // ~1/14 - 1/7 s
      moveStart = t;
      inMotion = true;
    }
  }

  // Autonomous blinking
  static uint32_t lastBlink = 0, nextBlink = 0;
  if ((t - lastBlink) >= nextBlink) {
    lastBlink = t;
    uint32_t blinkDuration = random(36000, 72000);
    if (blink.state == NOBLINK) {
      blink.state = ENBLINK;
      blink.startTime = t;
      blink.duration = blinkDuration;
    }
    nextBlink = blinkDuration * 3 + random(4000000);
  }
  if (blink.state && (t - blink.startTime) >= blink.duration) {
    if (++blink.state > DEBLINK) {
      blink.state = NOBLINK;
    } else {  // ENBLINK -> DEBLINK: reopen at half speed
      blink.duration *= 2;
      blink.startTime = t;
    }
  }

  // Scale motion to sclera pixel offsets of the 128x128 view window
  eyeX = map(eyeX, 0, 1023, 0, SCLERA_WIDTH - SCREEN_WIDTH);
  eyeY = map(eyeY, 0, 1023, 0, SCLERA_HEIGHT - SCREEN_HEIGHT);

  // Upper lid tracks the pupil (sample the lid map just above it)
  static uint8_t uThreshold = 128;
  uint8_t lThreshold, n;
  int16_t sampleX = SCLERA_WIDTH / 2 - (eyeX / 2);
  int16_t sampleY = SCLERA_HEIGHT / 2 - (eyeY + IRIS_HEIGHT / 4);
  if (sampleY < 0) {
    n = 0;
  } else {
    n = (upper[sampleY * SCREEN_WIDTH + sampleX] +
         upper[sampleY * SCREEN_WIDTH + (SCREEN_WIDTH - 1 - sampleX)]) / 2;
  }
  uThreshold = (uThreshold * 3 + n) / 4;
  lThreshold = 254 - uThreshold;

  // Blink scales both lids toward closed
  if (blink.state) {
    uint32_t s = t - blink.startTime;
    s = (s >= blink.duration) ? 255 : 255 * s / blink.duration;
    s = (blink.state == DEBLINK) ? 1 + s : 256 - s;
    n = (uThreshold * s + 254 * (257 - s)) / 256;
    lThreshold = (lThreshold * s + 254 * (257 - s)) / 256;
  } else {
    n = uThreshold;
  }

  draw_eye(next_iris(t), eyeX, eyeY, n, lThreshold);
}
