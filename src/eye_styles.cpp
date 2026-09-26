// Eye styles for the dragon eye screen. Each include/eyes/*.h (Adafruit "Uncanny Eyes",
// MIT) defines the same array names and macros, so each is included inside its own
// namespace and its macros are cleared before the next one.
//
// To add a style: copy another block below, change the namespace, header, id and name,
// and add it to EYE_STYLES. The id is what config.json stores; the name is shown on
// the web page. Photo eyes (artist pictures, tools/photo_eye.py) use PHOTO_STYLE_DEF.

#include "eye_styles.h"

#define SYMMETRICAL_EYELID  // Single centred eye: use the left/right symmetrical lid maps

// Default pupil range (Uncanny Eyes config.h) for headers that don't set their own
#define DEFAULT_IRIS_MIN 120
#define DEFAULT_IRIS_MAX 720

#define EYE_STYLE_DEF(ID, NAME)                                                      \
  const EyeStyle style = {                                                          \
    ID, NAME, &sclera[0][0], SCLERA_WIDTH, SCLERA_HEIGHT,                           \
    &iris[0][0], IRIS_MAP_WIDTH, IRIS_MAP_HEIGHT, &upper[0][0], &lower[0][0],        \
    &polar[0][0], IRIS_WIDTH, EYE_IRIS_MIN, EYE_IRIS_MAX, nullptr};

#define PHOTO_STYLE_DEF(ID, NAME) \
  const EyeStyle style = {ID, NAME, nullptr, 0, 0, nullptr, 0, 0, nullptr, nullptr, nullptr, 0, 0, 0, &photo};

namespace eye_dragon {
#include "eyes/dragonEye.h"
#include "eyes/eye_limits.h"
EYE_STYLE_DEF("dragon", "Dragon")
}
#include "eyes/eye_undef.h"

namespace eye_dragon2 {
#include "eyes/dragon2Photo.h"
PHOTO_STYLE_DEF("dragon2", "Dragon 2")
}

namespace eye_default {
#include "eyes/defaultEye.h"
#include "eyes/eye_limits.h"
EYE_STYLE_DEF("human", "Human")
}
#include "eyes/eye_undef.h"

namespace eye_cat {
#include "eyes/catEye.h"
#include "eyes/eye_limits.h"
EYE_STYLE_DEF("cat", "Cat")
}
#include "eyes/eye_undef.h"

namespace eye_goat {
#include "eyes/goatEye.h"
#include "eyes/eye_limits.h"
EYE_STYLE_DEF("goat", "Goat")
}
#include "eyes/eye_undef.h"

namespace eye_owl {
#include "eyes/owlEye.h"
#include "eyes/eye_limits.h"
EYE_STYLE_DEF("owl", "Owl")
}
#include "eyes/eye_undef.h"

namespace eye_doe {
#include "eyes/doeEye.h"
#include "eyes/eye_limits.h"
EYE_STYLE_DEF("doe", "Doe")
}
#include "eyes/eye_undef.h"

namespace eye_newt {
#include "eyes/newtEye.h"
#include "eyes/eye_limits.h"
EYE_STYLE_DEF("newt", "Newt")
}
#include "eyes/eye_undef.h"

namespace eye_nauga {
#include "eyes/naugaEye.h"
#include "eyes/eye_limits.h"
EYE_STYLE_DEF("nauga", "Nauga")
}
#include "eyes/eye_undef.h"

namespace eye_nosclera {
#include "eyes/noScleraEye.h"
#include "eyes/eye_limits.h"
EYE_STYLE_DEF("nosclera", "No sclera")
}
#include "eyes/eye_undef.h"

namespace eye_terminator {
#include "eyes/terminatorEye.h"
#include "eyes/eye_limits.h"
EYE_STYLE_DEF("terminator", "Terminator")
}
#include "eyes/eye_undef.h"

// Order shown on the web page; the first is the default
const EyeStyle *const EYE_STYLES[] = {
  &eye_dragon::style, &eye_dragon2::style, &eye_default::style, &eye_cat::style, &eye_goat::style,
  &eye_owl::style, &eye_doe::style, &eye_newt::style, &eye_nauga::style,
  &eye_nosclera::style, &eye_terminator::style,
};
const int EYE_STYLE_COUNT = sizeof(EYE_STYLES) / sizeof(EYE_STYLES[0]);

const EyeStyle *eye_style_find(const char *id) {
  for (int i = 0; i < EYE_STYLE_COUNT; i++) {
    if (strcmp(EYE_STYLES[i]->id, id) == 0) return EYE_STYLES[i];
  }
  return nullptr;
}
