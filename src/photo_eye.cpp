// Photo eye renderer (see include/photo_eye.h). The same steps, in numpy, make the
// preview in tools/photo_eye.py: keep the two in step.

#include "photo_eye.h"
#include "lcd_view.h"
#include <esp_heap_caps.h>
#include <math.h>

static const int OUT = 240;
static const int FX = 16;      // Column positions are in 1/16 px
static const int Q = 2;        // Iris distances are in 1/2 px (tools/photo_eye.py Q)
static const float LID_K = 0.15f;  // Share of the closure spent reaching the eye's edge

static uint16_t line_buf[OUT];
static int16_t upos[OUT], lpos[OUT];  // This frame's lid edges per column (1/16 px)
static int16_t lid_top[OUT], lid_bot[OUT];  // Lid region per column: up_start / lo_start
static uint16_t pb[PHOTO_ANGLES];     // Pupil edge per angle (1/2 px)
static uint32_t inv[PHOTO_ANGLES];    // PHOTO_TEX_R / (rim - pupil edge), 16.16
static float cos_t[PHOTO_ANGLES], sin_t[PHOTO_ANGLES];
static uint16_t *pix_ang = nullptr;   // Per-pixel iris table around the pupil centre: angle
static uint8_t *pix_rad = nullptr;    //   and distance (1/2 px, 255 = outside)
static int pix_n = 0;

static inline uint16_t darken(uint16_t p) {  // Half brightness
  return (p >> 1) & 0x7BEF;
}

static inline uint16_t blend(uint16_t bg, uint16_t fg, uint32_t a) {  // a 0-255 = fg share
  int r = (bg >> 11) + (((int)(fg >> 11) - (int)(bg >> 11)) * (int)a >> 8);
  int g = ((bg >> 5) & 63) + (((int)((fg >> 5) & 63) - (int)((bg >> 5) & 63)) * (int)a >> 8);
  int b = (bg & 31) + (((int)(fg & 31) - (int)(bg & 31)) * (int)a >> 8);
  return (r << 11) | (g << 5) | b;
}

// Lid edge for a closure: start -> edge in the first LID_K, then edge -> seam
static int16_t lid_pos(int start, int edge, int seam, float c) {
  if (c <= LID_K) return start + (int)((edge - start) * c / LID_K);
  return edge + (int)((seam - edge) * (c - LID_K) / (1 - LID_K));
}

bool photo_eye_prepare(const PhotoEye *e) {
  const int n = e->table;
  if (n != pix_n) {
    heap_caps_free(pix_ang);
    heap_caps_free(pix_rad);
    pix_ang = (uint16_t *)heap_caps_malloc(n * n * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    pix_rad = (uint8_t *)heap_caps_malloc(n * n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    pix_n = pix_ang && pix_rad ? n : 0;
    if (!pix_n) return false;
  }
  // Same as the converter: pixel centres relative to the pupil centre
  for (int j = 0; j < n; j++) {
    float dy = j + e->table_y + 0.5f - e->pupil_y;
    for (int i = 0; i < n; i++) {
      float dx = i + e->table_x + 0.5f - e->pupil_x;
      float t = atan2f(dy, dx);
      if (t < 0) t += 2 * (float)M_PI;
      int a = (int)(t * PHOTO_ANGLES / (2 * (float)M_PI));
      pix_ang[j * n + i] = a > PHOTO_ANGLES - 1 ? PHOTO_ANGLES - 1 : a;
      int r = (int)lroundf(sqrtf(dx * dx + dy * dy) * Q);
      pix_rad[j * n + i] = r > 254 ? 255 : r;
    }
  }
  return true;
}

void photo_eye_draw(lgfx::LGFX_Device *tft, const PhotoEye *e, int gx, int gy, float cu, float cl,
                    float droop, float pupil, const uint8_t *row_x0, const uint8_t *row_x1) {
  if (cos_t[0] == 0) {
    for (int a = 0; a < PHOTO_ANGLES; a++) {
      float t = (a + 0.5f) * 2 * (float)M_PI / PHOTO_ANGLES;
      cos_t[a] = cosf(t);
      sin_t[a] = sinf(t);
    }
  }

  // Pupil edge per angle: slit |x| <= w(1 - (y/h)^2), same formula as the converter
  float w = e->w_min + (e->w_max - e->w_min) * pupil;
  float h = e->h_min + (e->h_max - e->h_min) * pupil;
  const float ct = cosf(e->tilt), st = sinf(e->tilt);
  for (int a = 0; a < PHOTO_ANGLES; a++) {
    float c = cos_t[a] * ct - sin_t[a] * st, s = cos_t[a] * st + sin_t[a] * ct;  // Slit's axes
    float k = w * s * s / (h * h);
    int p = (int)(2 * w / (fabsf(c) + sqrtf(c * c + 4 * k * w)) * Q + 0.5f);
    int rim = e->iris_rim[a];
    if (p > rim - 1) p = rim - 1;
    pb[a] = p;
    inv[a] = ((uint32_t)PHOTO_TEX_R << 16) / (rim - p);
  }

  // Lid edges per column
  for (int x = 0; x < OUT; x++) {
    if (e->seam[x] < 0) {
      lid_top[x] = lid_bot[x] = upos[x] = lpos[x] = -1;
      continue;
    }
    int edge = e->up_edge[x], seam = e->seam[x];
    int u = lid_pos(e->up_start[x], edge, seam, cu);
    int d = edge + (int)((seam - edge) * droop);
    upos[x] = u > d ? u : d;
    lpos[x] = lid_pos(e->lo_start[x], e->lo_edge[x], seam, cl);
    lid_top[x] = e->up_start[x];
    lid_bot[x] = e->lo_start[x];
  }
  const bool shade = cu > LID_K;  // Upper lid is over the eye: it casts a shadow
  const int n = e->table;
  const int tx0 = e->table_x + gx, ty0 = e->table_y + gy;

  tft->startWrite();
  tft->setAddrWindow(0, 0, OUT, OUT);
  for (int y = 0; y < OUT; y++) {
    const int y16 = y * FX + FX / 2;
    const uint16_t *open_row = e->open + y * OUT;
    const uint16_t *closed_row = e->closed_px + e->closed_off[y] - e->closed_x0[y];  // Index by x
    const int iy = y - ty0;
    const bool in_table_y = iy >= 0 && iy < n;
    const bool in_hl_y = y >= e->hl_y && y < e->hl_y + e->hl_h;
    const int x0 = row_x0[y], x1 = row_x1[y];
    for (int x = 0; x < x0; x++) line_buf[x] = 0;
    for (int x = x1; x < OUT; x++) line_buf[x] = 0;
    for (int x = x0; x < x1; x++) {
      if (lid_top[x] >= 0 && ((y16 >= lid_top[x] && y16 < upos[x]) || (y16 > lpos[x] && y16 <= lid_bot[x]))) {
        line_buf[x] = closed_row[x];                                  // Lid
        continue;
      }
      uint16_t p = open_row[x];
      if (y16 <= e->eye_top[x] || y16 >= e->eye_bot[x]) {            // Face
        line_buf[x] = p;
        continue;
      }
      const int ix = x - tx0;                                         // Eyeball
      if (in_table_y && ix >= 0 && ix < n) {
        const int i = iy * n + ix;
        const int r = pix_rad[i];
        const int a = pix_ang[i];
        if (r < e->iris_rim[a]) {
          if (r < pb[a]) {
            p = 0;                                                    // Pupil
          } else {
            p = e->iris_tex[a * PHOTO_TEX_R + (((r - pb[a]) * inv[a]) >> 16)];  // Iris
            if (r < pb[a] + Q) p = darken(p);                         // Soft pupil edge
          }
        }
      }
      if (in_hl_y && x >= e->hl_x && x < e->hl_x + e->hl_w) {         // Cornea highlight
        const int j = (y - e->hl_y) * e->hl_w + (x - e->hl_x);
        if (e->hl_alpha[j]) p = blend(p, e->hl_rgb[j], e->hl_alpha[j]);
      }
      if (shade && y16 < upos[x] + 3 * FX) p = darken(p);             // Shadow under the lid
      line_buf[x] = p;
    }
    tft->writePixels((lgfx::rgb565_t *)line_buf, OUT);
    lcd_view_eye_row(y, line_buf);  // Web LCD view (copies only while capturing)
  }
  tft->endWrite();
}
