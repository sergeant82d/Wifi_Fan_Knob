#ifndef DRAGON_EYE_H
#define DRAGON_EYE_H

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// Animated dragon eye for the standby screen, drawn directly with LovyanGFX
// (LVGL must not be flushing while it runs). Ported from Adafruit "Uncanny Eyes".
void eye_begin(lgfx::LGFX_Device *display);
void eye_frame();   // Render one full-screen frame (non-blocking; call repeatedly)

#endif
