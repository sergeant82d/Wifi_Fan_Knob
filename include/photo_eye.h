#ifndef PHOTO_EYE_H
#define PHOTO_EYE_H

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// "Photo eye": an artist's open and closed eye pictures, animated. The iris moves over
// the open picture, the slit pupil widens and narrows, highlights stay put on the
// cornea, and the lids close through the closed picture. Data made by
// tools/photo_eye.py (include/eyes/*Photo.h); drawn by src/photo_eye.cpp.

#define PHOTO_ANGLES 512  // Iris texture angles (tools/photo_eye.py ANGLES)
#define PHOTO_TEX_R 64    // Iris texture radial samples, pupil edge -> ring edge (TEX_R)

struct PhotoEye {
  const uint16_t *open;       // 240x240 RGB565, iris taken out (the socket it moves over)
  const uint16_t *closed;     // 240x240 RGB565, eye shut
  // Per column, 1/16 px, -1 = none. Upper lid: up_start (where it starts to change) ->
  // up_edge (edge of the open eye) -> seam (where the lids meet); lower lid likewise.
  const int16_t *up_start, *up_edge, *seam, *lo_edge, *lo_start;
  const int16_t *eye_top, *eye_bot;  // Where the eyeball shows (the iris is drawn only there)
  const uint16_t *iris_tex;   // PHOTO_ANGLES x PHOTO_TEX_R RGB565
  const uint16_t *iris_rim;   // PHOTO_ANGLES: ring outer edge from the pupil centre, 1/4 px
  const uint16_t *pix_ang;    // table x table around the pupil centre: angle (0-511)
  const uint8_t *pix_rad;     //   and distance (1/4 px, 255 = outside)
  uint16_t table;
  int16_t table_x, table_y;   // Screen position of the table's top-left pixel (eye centred)
  const uint16_t *hl_rgb;     // Highlight patch (cornea reflections): colour
  const uint8_t *hl_alpha;    //   and opacity, hl_w x hl_h at hl_x, hl_y
  uint8_t hl_x, hl_y, hl_w, hl_h;
  float pupil_x, pupil_y, pupil_w, pupil_h;  // Pupil as drawn: centre, half width, half height
  float w_min, w_max, h_min, h_max;          // Pupil half width / half height range
  float range_x, range_y;     // How far the iris moves (px)
  float droop;                // Upper lid lowers this much (of edge -> seam) looking fully down
};

// Draw one frame. gx, gy: iris offset (px, + = right/down); cu, cl: upper/lower lid
// closure 0 (open) - 1 (shut); droop 0-1 (upper lid follows the iris down); pupil 0
// (narrow slit) - 1 (wide). row_x0/x1: visible span of each row on the round screen.
void photo_eye_draw(lgfx::LGFX_Device *tft, const PhotoEye *e, int gx, int gy, float cu, float cl,
                    float droop, float pupil, const uint8_t *row_x0, const uint8_t *row_x1);

#endif
