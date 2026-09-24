#ifndef DRAGON_EYE_H
#define DRAGON_EYE_H

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// Animated eye for standby and the screensaver, drawn directly with LovyanGFX
// (LVGL must not be flushing while it runs). Ported from Adafruit "Uncanny Eyes".
void eye_begin(lgfx::LGFX_Device *display);
bool eye_set_style(const char *id);  // Style from eye_styles.h; false if unknown or no memory
const char *eye_style_id();          // Current style id ("" before the first eye_set_style)
void eye_frame();                    // Render one full-screen frame (non-blocking; call repeatedly)
void eye_set_sleeping(bool on);      // Standby: lids close, then twitch/peek now and then

#endif
