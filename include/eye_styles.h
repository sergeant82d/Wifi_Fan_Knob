#ifndef EYE_STYLES_H
#define EYE_STYLES_H

#include <Arduino.h>
#include "photo_eye.h"

// One "Uncanny Eyes" style: 128 px tables in flash (see include/eyes/*.h).
// dragon_eye.cpp upscales them to 240 px when the style is selected.
// Or a photo eye (photo set, tables unused): drawn by photo_eye.cpp.
struct EyeStyle {
  const char *id;            // Stored in config, e.g. "dragon"
  const char *name;          // Shown on the web page, e.g. "Dragon"
  const uint16_t *sclera;    // RGB565, sclera_w x sclera_h
  uint16_t sclera_w, sclera_h;
  const uint16_t *iris;      // RGB565 iris texture, iris_map_w (angle) x iris_map_h (distance)
  uint16_t iris_map_w, iris_map_h;
  const uint8_t *upper;      // Eyelid threshold maps, 128 x 128
  const uint8_t *lower;
  const uint16_t *polar;     // iris_size x iris_size: angle (high 9 bits) | distance (low 7)
  uint16_t iris_size;
  uint16_t iris_min, iris_max;  // Pupil size range (0-1023)
  const PhotoEye *photo;     // Photo eye data, or nullptr for an Uncanny Eyes style
};

extern const EyeStyle *const EYE_STYLES[];
extern const int EYE_STYLE_COUNT;

const EyeStyle *eye_style_find(const char *id);  // nullptr if unknown

#endif
