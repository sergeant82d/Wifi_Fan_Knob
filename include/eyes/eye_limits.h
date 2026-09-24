// Pupil range for the eye header just included: its own IRIS_MIN/MAX, else the default.
// Used by src/eye_styles.cpp (no include guard: included once per style).
#ifdef IRIS_MIN
#define EYE_IRIS_MIN IRIS_MIN
#else
#define EYE_IRIS_MIN DEFAULT_IRIS_MIN
#endif
#ifdef IRIS_MAX
#define EYE_IRIS_MAX IRIS_MAX
#else
#define EYE_IRIS_MAX DEFAULT_IRIS_MAX
#endif
