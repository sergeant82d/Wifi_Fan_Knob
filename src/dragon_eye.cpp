// Animated eye ("dragon eye" and other styles), ported from "Uncanny Eyes" by Phil Burgess /
// Paint Your Dragon for Adafruit Industries (MIT license), via Bodmer's TFT_eSPI
// Animated_Eyes example.
// Changes: single eye; LovyanGFX output; native 240x240: the style's 128 px tables are
// upscaled once into PSRAM when the style is chosen (bilinear; iris angles recomputed),
// so there are no 2x2 pixel blocks; pixels outside the round screen are skipped; one
// frame per call (the original's blocking iris loop is replaced by a non-blocking ramp).
// Iris/pupil formula and per-style pupil limits are the original's.
// Added: sleeping mode for standby (lids close, then an occasional twitch or peek).
// Photo eyes (artist pictures, photo_eye.cpp) share the motion, blinks and sleep here.

#include "dragon_eye.h"
#include "eye_styles.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <math.h>

static lgfx::LGFX_Device *tft = nullptr;

static const int SRC = 128;  // Screen size the styles' tables were drawn for
static const int OUT = 240;  // This screen
static int up(int v) { return (v * OUT + SRC / 2) / SRC; }  // Source px -> screen px (x1.875)

// Selected style, upscaled to screen resolution (PSRAM)
struct Tables {
  const EyeStyle *style = nullptr;
  uint16_t *sclera = nullptr;          // sclera_w x sclera_h
  int sclera_w = 0, sclera_h = 0;
  uint8_t *upper = nullptr;            // OUT x OUT eyelid thresholds
  uint8_t *lower = nullptr;
  uint16_t *polar = nullptr;           // iris_size x iris_size
  int iris_size = 0;
};
static Tables eye;

static uint16_t line_buf[OUT];
static uint8_t row_x0[OUT], row_x1[OUT];  // Visible span of each row on the round screen

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

// ============================================================================
// UPSCALING (runs once per style change)
// ============================================================================

// Source sample position for destination index i (pixel centres aligned): i0, i1 and
// the weight of i1 in 1/256ths
static void src_pos(int i, int src_n, int dst_n, int &i0, int &i1, int &f) {
  float s = (i + 0.5f) * src_n / dst_n - 0.5f;
  if (s < 0) s = 0;
  if (s > src_n - 1) s = src_n - 1;
  i0 = (int)s;
  i1 = i0 + 1 < src_n ? i0 + 1 : i0;
  f = (int)((s - i0) * 256);
}

static int lerp2(int a, int b, int c, int d, int fx, int fy) {
  int top = a * 256 + (b - a) * fx;
  int bot = c * 256 + (d - c) * fx;
  return (top * 256 + (bot - top) * fy + 32768) >> 16;
}

static void *psram_alloc(size_t bytes) {
  return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static uint8_t *scale_u8(const uint8_t *src, int sn, int dn) {
  uint8_t *dst = (uint8_t *)psram_alloc(dn * dn);
  if (!dst) return nullptr;
  for (int y = 0; y < dn; y++) {
    int y0, y1, fy;
    src_pos(y, sn, dn, y0, y1, fy);
    for (int x = 0; x < dn; x++) {
      int x0, x1, fx;
      src_pos(x, sn, dn, x0, x1, fx);
      dst[y * dn + x] = lerp2(src[y0 * sn + x0], src[y0 * sn + x1],
                              src[y1 * sn + x0], src[y1 * sn + x1], fx, fy);
    }
  }
  return dst;
}

static uint16_t *scale_rgb565(const uint16_t *src, int sw, int sh, int dw, int dh) {
  uint16_t *dst = (uint16_t *)psram_alloc(dw * dh * 2);
  if (!dst) return nullptr;
  for (int y = 0; y < dh; y++) {
    int y0, y1, fy;
    src_pos(y, sh, dh, y0, y1, fy);
    for (int x = 0; x < dw; x++) {
      int x0, x1, fx;
      src_pos(x, sw, dw, x0, x1, fx);
      uint16_t a = src[y0 * sw + x0], b = src[y0 * sw + x1];
      uint16_t c = src[y1 * sw + x0], d = src[y1 * sw + x1];
      int r = lerp2(a >> 11, b >> 11, c >> 11, d >> 11, fx, fy);
      int g = lerp2((a >> 5) & 63, (b >> 5) & 63, (c >> 5) & 63, (d >> 5) & 63, fx, fy);
      int bl = lerp2(a & 31, b & 31, c & 31, d & 31, fx, fy);
      dst[y * dw + x] = (r << 11) | (g << 5) | bl;
    }
  }
  return dst;
}

// Iris polar table at screen resolution: angle recomputed exactly, distance (which
// carries the pupil shape for dragon/cat/goat) interpolated from the style's table.
// Source entries outside its circle (127) count as the rim (0) so the edge stays clean.
static uint16_t *build_polar(const EyeStyle *s, int n) {
  uint16_t *dst = (uint16_t *)psram_alloc(n * n * 2);
  if (!dst) return nullptr;
  const int sn = s->iris_size;
  const float r = n / 2.0f;
  auto dist = [&](int sy, int sx) {
    uint16_t p = s->polar[sy * sn + sx];
    return p == 127 ? 0 : (int)(p & 0x7F);
  };
  for (int y = 0; y < n; y++) {
    int y0, y1, fy;
    src_pos(y, sn, n, y0, y1, fy);
    float dy = y - r + 0.5f;
    for (int x = 0; x < n; x++) {
      float dx = x - r + 0.5f;
      if (dx * dx + dy * dy >= r * r) {  // Outside the iris circle
        dst[y * n + x] = 127;
        continue;
      }
      int x0, x1, fx;
      src_pos(x, sn, n, x0, x1, fx);
      int a = (int)((atan2f(dy, dx) + (float)M_PI) / (2.0f * (float)M_PI) * 512.0f);
      if (a > 511) a = 511;
      int d = lerp2(dist(y0, x0), dist(y0, x1), dist(y1, x0), dist(y1, x1), fx, fy);
      dst[y * n + x] = (a << 7) | (d > 127 ? 127 : d);
    }
  }
  return dst;
}

static void free_tables(Tables &t) {
  heap_caps_free(t.sclera);
  heap_caps_free(t.upper);
  heap_caps_free(t.lower);
  heap_caps_free(t.polar);
  t = Tables();
}

bool eye_set_style(const char *id) {
  const EyeStyle *s = eye_style_find(id);
  if (!s) {
    Serial.printf("[EYE] Unknown style '%s'\n", id);
    return false;
  }
  if (s == eye.style) return true;
  if (s->photo) {  // Photo eye: drawn straight from flash, no tables to build
    free_tables(eye);
    eye.style = s;
    Serial.printf("[EYE] Style %s ready\n", s->name);
    return true;
  }

  uint32_t start = millis();
  Tables t;
  t.style = s;
  t.sclera_w = up(s->sclera_w);
  t.sclera_h = up(s->sclera_h);
  t.iris_size = up(s->iris_size);
  t.sclera = scale_rgb565(s->sclera, s->sclera_w, s->sclera_h, t.sclera_w, t.sclera_h);
  t.upper = scale_u8(s->upper, SRC, OUT);
  t.lower = scale_u8(s->lower, SRC, OUT);
  t.polar = build_polar(s, t.iris_size);
  if (!t.sclera || !t.upper || !t.lower || !t.polar) {
    free_tables(t);
    Serial.printf("[EYE] Not enough PSRAM for style %s\n", s->name);
    return false;
  }
  free_tables(eye);
  eye = t;
  Serial.printf("[EYE] Style %s ready in %lu ms\n", s->name, millis() - start);
  return true;
}

const char *eye_style_id() {
  return eye.style ? eye.style->id : "";
}

void eye_begin(lgfx::LGFX_Device *display) {
  tft = display;
  for (int y = 0; y < OUT; y++) {  // Round screen: only these pixels are visible
    float dy = y + 0.5f - OUT / 2.0f;
    float half = sqrtf(OUT * OUT / 4.0f - dy * dy);
    int x0 = (int)(OUT / 2.0f - half);
    row_x0[y] = x0 < 0 ? 0 : x0;
    row_x1[y] = OUT - row_x0[y];
  }
}

// ============================================================================
// SLEEPING (standby): lids close slowly, stay shut 3.5-10 s, then twitch (brief flicker)
// or peek (open part way, look around, close). Openness 0 = shut, 1 = normal.
// A stir (touch or knob) opens it fully and keeps it awake, blinking, then it closes again.
// ============================================================================

enum SleepPhase { SLEEP_CLOSING, SLEEP_CLOSED, SLEEP_TWITCH, SLEEP_PEEK_OPEN, SLEEP_PEEK_HOLD, SLEEP_PEEK_CLOSE,
                  SLEEP_STIR_OPEN, SLEEP_STIR };
static bool sleeping = false;
static bool drawn_shut = false;    // Last frame drawn was fully shut: nothing to redraw
static SleepPhase sleep_phase = SLEEP_CLOSED;
static uint32_t phase_start = 0, phase_ms = 1;
static float peek_open = 0;        // How far this twitch/peek opens (0-1)
static float openness_now = 0;     // Last sleeping openness drawn (a stir opens from here)
static float stir_from = 0;        // Openness when the stir began
static uint32_t stir_ms = 0;       // How long to stay awake once open
const uint32_t STIR_OPEN_MS = 400;

// Gaze held by eye_look() (0-1023, like the autonomous motion)
static int16_t gaze_x = 512, gaze_y = 512;
static uint32_t gaze_until = 0;

static void sleep_phase_set(SleepPhase p, uint32_t ms) {
  sleep_phase = p;
  phase_start = millis();
  phase_ms = ms;
}

void eye_set_sleeping(bool on) {
  sleeping = on;
  drawn_shut = false;  // The screen was LVGL's (or awake eye) until now
  if (on) sleep_phase_set(SLEEP_CLOSING, 2000);
}

void eye_stir(uint32_t ms) {
  if (!sleeping) return;
  stir_ms = ms;
  if (sleep_phase == SLEEP_STIR) {
    sleep_phase_set(SLEEP_STIR, ms);  // Already awake: restart the awake time
  } else if (sleep_phase != SLEEP_STIR_OPEN) {
    stir_from = openness_now;
    sleep_phase_set(SLEEP_STIR_OPEN, STIR_OPEN_MS);
  }
}

bool eye_stirred() {
  return sleeping && (sleep_phase == SLEEP_STIR_OPEN || sleep_phase == SLEEP_STIR);
}

void eye_look(int x, int y, uint32_t ms) {
  // The view window slides over the sclera, so a larger offset shows the iris further
  // left/up: invert so the eye turns toward the point.
  int32_t gx = 1023 - constrain(x, 0, OUT - 1) * 1023 / (OUT - 1);
  int32_t gy = 1023 - constrain(y, 0, OUT - 1) * 1023 / (OUT - 1);
  int32_t dx = gx * 2 - 1023, dy = gy * 2 - 1023;  // Keep inside the circle
  float r = sqrtf((float)(dx * dx + dy * dy));
  if (r > 1023) {
    gx = 512 + (int32_t)(dx * 1023 / r) / 2;
    gy = 512 + (int32_t)(dy * 1023 / r) / 2;
  }
  gaze_x = gx;
  gaze_y = gy;
  gaze_until = millis() + ms;
}

static float smooth(float x) {  // Ease in/out, 0-1
  return x * x * (3 - 2 * x);
}

static float sleep_openness() {
  float x = (float)(millis() - phase_start) / phase_ms;
  if (x >= 1) {  // Phase over: pick the next one
    switch (sleep_phase) {
      case SLEEP_CLOSING:
      case SLEEP_TWITCH:
      case SLEEP_PEEK_CLOSE:
        sleep_phase_set(SLEEP_CLOSED, random(3500, 10000));
        break;
      case SLEEP_CLOSED:
        if (random(100) < 50) {
          peek_open = random(20, 45) / 100.0f;
          sleep_phase_set(SLEEP_TWITCH, random(400, 700));
        } else {
          peek_open = random(50, 85) / 100.0f;
          sleep_phase_set(SLEEP_PEEK_OPEN, random(900, 1500));
        }
        break;
      case SLEEP_PEEK_OPEN:
        sleep_phase_set(SLEEP_PEEK_HOLD, random(1500, 4000));
        break;
      case SLEEP_PEEK_HOLD:
        sleep_phase_set(SLEEP_PEEK_CLOSE, random(1200, 2000));
        break;
      case SLEEP_STIR_OPEN:
        sleep_phase_set(SLEEP_STIR, stir_ms);
        break;
      case SLEEP_STIR:  // Awake time over: close slowly, like the end of a peek
        peek_open = 1;
        sleep_phase_set(SLEEP_PEEK_CLOSE, 1500);
        break;
    }
    x = 0;
  }
  switch (sleep_phase) {
    case SLEEP_CLOSING:    return 1 - smooth(x);
    case SLEEP_TWITCH:     return peek_open * sinf((float)M_PI * x);
    case SLEEP_PEEK_OPEN:  return peek_open * smooth(x);
    case SLEEP_PEEK_HOLD:  return peek_open;
    case SLEEP_PEEK_CLOSE: return peek_open * (1 - smooth(x));
    case SLEEP_STIR_OPEN:  return stir_from + (1 - stir_from) * smooth(x);
    case SLEEP_STIR:       return 1;
    default:               return 0;  // SLEEP_CLOSED
  }
}

// ============================================================================
// DRAWING
// ============================================================================

// Renders one frame. scleraX/Y: view window offset in the sclera; uT/lT: eyelid
// thresholds; iScale: pupil size (0-1023). Same pixel logic as the original drawEye().
static void draw_eye(uint32_t iScale, int scleraX, int scleraY, uint32_t uT, uint32_t lT) {
  const EyeStyle *s = eye.style;
  uint32_t irisThreshold = (128 * (1023 - iScale) + 512) / 1024;
  if (irisThreshold == 0) irisThreshold = 1;  // iScale near 1023 (nauga): avoid /0
  uint32_t irisScale = s->iris_map_h * 65536 / irisThreshold;
  const int irisOffX = (eye.sclera_w - eye.iris_size) / 2;
  const int irisOffY = (eye.sclera_h - eye.iris_size) / 2;

  tft->startWrite();
  tft->setAddrWindow(0, 0, OUT, OUT);
  for (int y = 0; y < OUT; y++) {
    const int sy = scleraY + y;
    const int iy = sy - irisOffY;
    const uint16_t *sclera_row = eye.sclera + sy * eye.sclera_w;
    const uint8_t *upper_row = eye.upper + y * OUT;
    const uint8_t *lower_row = eye.lower + y * OUT;
    const int x0 = row_x0[y], x1 = row_x1[y];
    for (int x = 0; x < x0; x++) line_buf[x] = 0;
    for (int x = x1; x < OUT; x++) line_buf[x] = 0;
    for (int x = x0; x < x1; x++) {
      const int sx = scleraX + x;
      const int ix = sx - irisOffX;
      uint32_t p;
      if (lower_row[x] <= lT || upper_row[x] <= uT) {                     // Eyelid
        p = 0;
      } else if (iy < 0 || iy >= eye.iris_size || ix < 0 || ix >= eye.iris_size) {
        p = sclera_row[sx];                                                // Sclera
      } else {                                                             // Maybe iris
        p = eye.polar[iy * eye.iris_size + ix];
        uint32_t d = p & 0x7F;                                             // Distance
        if (d < irisThreshold) {
          d = d * irisScale / 65536;
          uint32_t a = (s->iris_map_w * (p >> 7)) / 512;                  // Angle
          p = s->iris[d * s->iris_map_w + a];
        } else {
          p = sclera_row[sx];
        }
      }
      line_buf[x] = p;
    }
    tft->writePixels((lgfx::rgb565_t *)line_buf, OUT);
  }
  tft->endWrite();
}

// Pupil size: non-blocking ramp between random targets in the style's range (the
// original used a blocking recursive "split" that ran for ~10 s)
static uint16_t next_iris(uint32_t t) {
  const EyeStyle *s = eye.style;
  static uint16_t from = 0, to = 0;
  static uint32_t start = 0, duration = 1;
  static const EyeStyle *for_style = nullptr;
  if (for_style != s) {  // New style: start mid-range
    for_style = s;
    from = to = (s->iris_min + s->iris_max) / 2;
  }
  uint32_t dt = t - start;
  if (dt >= duration) {
    from = to;
    to = random(s->iris_min, s->iris_max);
    start = t;
    duration = random(500000, 2500000);
    dt = 0;
  }
  return from + (int32_t)(to - from) * (int32_t)dt / (int32_t)duration;
}

// ============================================================================
// PHOTO EYES
// ============================================================================

// Pupil 0 (slit) - 1 (wide), reacting as if to light: wide while the lids are shut,
// narrowing as they open; snaps narrow when stirred or when the eye appears (screensaver),
// then relaxes; awake, it drifts and now and then flinches narrow.
static float photo_pupil(float openness) {
  static float p = 1, target = 0.4f;
  static uint32_t last = 0, next_drift = 0, next_flinch = 0, flash_until = 0;
  static bool was_stirred = false;
  uint32_t now = millis();
  float dt = (now - last) / 1000.0f;
  if (now - last > 1000) {  // Not drawn for a while: the eye just appeared out of the dark
    p = 1;
    if (!sleeping) flash_until = now + 700;
    next_flinch = now + random(12000, 30000);
    dt = 0;
  }
  last = now;
  bool stirred = eye_stirred();
  if (stirred && !was_stirred) flash_until = now + 700;  // Stirred: bright, snap narrow
  was_stirred = stirred;

  float tau;  // Seconds to get most of the way to the target
  if ((int32_t)(flash_until - now) > 0) {
    target = 0.02f;
    tau = 0.12f;
  } else if (sleeping && !stirred) {
    target = 1 - 0.6f * openness;  // Dark behind the lids; a peek lets light in
    tau = 1.0f;
  } else {
    if ((int32_t)(now - next_flinch) >= 0) {
      next_flinch = now + random(12000, 30000);
      flash_until = now + random(400, 800);
    }
    if ((int32_t)(now - next_drift) >= 0) {
      next_drift = now + random(1500, 4000);
      target = random(20, 65) / 100.0f;
    }
    tau = 1.2f;
  }
  p += (target - p) * (1 - expf(-dt / tau));
  return p;
}

static void photo_frame(uint32_t t, int eyeX, int eyeY) {
  const PhotoEye *e = eye.style->photo;
  // Autonomous motion and eye_look() use 0-1023 as a view-window offset (larger = iris
  // further left/up): invert to an iris offset in px
  int gx = (int)lroundf((512 - eyeX) * e->range_x / 512);
  int gy = (int)lroundf((512 - eyeY) * e->range_y / 512);
  float droop = gy > 0 && e->range_y > 0 ? e->droop * gy / e->range_y : 0;

  float f = sleeping ? sleep_openness() : 1;
  float closure;
  if (sleeping && !(f >= 1 && sleep_phase == SLEEP_STIR)) {  // Lids follow the sleep openness
    openness_now = f;
    if (f <= 0.01f) {
      if (drawn_shut) {  // Already shut on screen: nothing to redraw
        photo_pupil(0);
        return;
      }
      drawn_shut = true;
      f = 0;
    } else {
      drawn_shut = false;
    }
    closure = 1 - f;
  } else {  // Awake (or stirred awake): blinks
    if (sleeping) {
      openness_now = 1;
      drawn_shut = false;
    }
    f = 1;
    closure = 0;
    if (blink.state) {
      uint32_t s = t - blink.startTime;
      s = (s >= blink.duration) ? 255 : 255 * s / blink.duration;
      s = (blink.state == DEBLINK) ? 1 + s : 256 - s;  // Openness, 1-256
      closure = 1 - s / 256.0f;
    }
  }
  photo_eye_draw(tft, e, gx, gy, closure, closure, droop, photo_pupil(f), row_x0, row_x1);
}

void eye_frame() {
  if (!tft || !eye.style) return;
  uint32_t t = micros();
  int16_t eyeX, eyeY;

  // Autonomous eye motion: move to a random point, hold, repeat (0-1023 range)
  static bool inMotion = false;
  static int16_t oldX = 512, oldY = 512, newX = 512, newY = 512;
  static uint32_t moveStart = 0;
  static int32_t moveDuration = 0;
  int32_t dt = t - moveStart;
  if ((int32_t)(gaze_until - millis()) > 0) {  // eye_look(): ease toward the point, then hold
    oldX += (gaze_x - oldX) / 3;
    oldY += (gaze_y - oldY) / 3;
    eyeX = oldX;
    eyeY = oldY;
    inMotion = false;
    moveStart = t;
    moveDuration = random(1000000, 3000000);  // Roam again 1-3 s after the look ends
  } else if (inMotion) {
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

  if (eye.style->photo) {
    photo_frame(t, eyeX, eyeY);
    return;
  }

  // Scale motion to sclera offsets of the 240x240 view window
  eyeX = map(eyeX, 0, 1023, 0, eye.sclera_w - OUT);
  eyeY = map(eyeY, 0, 1023, 0, eye.sclera_h - OUT);

  // Upper lid tracks the pupil (sample the lid map just above it)
  static uint8_t uThreshold = 128;
  uint8_t lThreshold, n;
  int sampleX = constrain(eye.sclera_w / 2 - eyeX / 2, 0, OUT - 1);
  int sampleY = eye.sclera_h / 2 - (eyeY + eye.iris_size / 4);
  if (sampleY < 0) {
    n = 0;
  } else {
    sampleY = min(sampleY, OUT - 1);
    n = (eye.upper[sampleY * OUT + sampleX] + eye.upper[sampleY * OUT + (OUT - 1 - sampleX)]) / 2;
  }
  uThreshold = (uThreshold * 3 + n) / 4;
  lThreshold = 254 - uThreshold;

  // Sleeping: lids follow the sleep openness instead of blinking. While fully shut the
  // screen is already black, so skip drawing (standby does almost nothing then).
  // Stirred awake: draw like the awake eye (with blinks) until the stir ends
  if (sleeping && sleep_openness() >= 1 && sleep_phase == SLEEP_STIR) {
    openness_now = 1;
    drawn_shut = false;
  } else if (sleeping) {
    float f = sleep_openness();
    openness_now = f;
    if (f <= 0.01f) {
      if (drawn_shut) return;
      drawn_shut = true;
      f = 0;
    } else {
      drawn_shut = false;
    }
    n = 254 - (uint8_t)((254 - uThreshold) * f);
    lThreshold = 254 - (uint8_t)((254 - lThreshold) * f);
    draw_eye(next_iris(t), eyeX, eyeY, n, lThreshold);
    return;
  }

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
