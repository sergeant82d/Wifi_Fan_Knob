# Photo Eyes (artist pictures, animated)

A photo eye turns two pictures from the artist, eye **open** and eye **shut**, into an
animated eye style (**Dragon 2** to **Dragon 11**). It is chosen on the web page like the
Uncanny Eyes styles and uses the same motion, blinks, sleep, stir and glance behaviour.

What moves:

- **Iris** slides over the socket (a few px each way), clipped by the lids.
- **Pupil** (slit) widens and narrows as if reacting to light: wide while the lids are
  shut, narrowing as a peek opens; snaps narrow when stirred or when the screensaver
  starts, then relaxes; awake it drifts and now and then flinches narrow.
- **Highlights** (cornea reflections) stay put while the iris moves under them.
- **Lids** close through the shut picture: the upper lid sweeps down, the lower up, to
  the seam; the upper lid droops a little when the eye looks down.

## Files

| File | Purpose |
|------|---------|
| `assets/eye_art/*.png` | The artist's pictures, in open/shut pairs (same artwork, same size and position) |
| `assets/eye_photos/<id>/eye.json` | Geometry, in 240 px screen coordinates |
| `assets/eye_photos/<id>/preview.png` | Made by the converter: rest, looks, pupil, blink stages, shut, original |
| `tools/photo_eye.py` | Converter (Python 3 + Pillow + numpy) |
| `include/eyes/<id>Photo.h` | Generated firmware data (~200-240 KB flash per eye; the shut picture is stored only where a lid can cover) |
| `src/photo_eye.cpp`, `include/photo_eye.h` | Renderer |
| `src/eye_styles.cpp` | Style list (`PHOTO_STYLE_DEF`) |

## Flash space

Each photo eye adds about 200-240 KB to the firmware (the open picture, the shut picture
where a lid can cover, the iris texture). With ten photo eyes (Dragons 2-11) the firmware is
5.6 MB, 85.5 % of the 6.25 MB app slot (measured 2026-09-27; it was 3.3 MB before the photo
eyes), so there is room for about four more (check the size PlatformIO prints after a build). To make room, remove a style: delete its
`namespace eye_dragonN { ... }` block and its `EYE_STYLES` entry in `src/eye_styles.cpp`
(its header is then not compiled in). A board set to a removed style falls back to the first
style. A bigger app slot would need a custom partition table (flashed over USB; it moves
SPIFFS, so the config and fan profiles are reset).

## Adding a new eye

1. Put the pair in `assets/eye_art/`. Any square size; the round picture is found from its
   transparent/black surround and scaled to 240.
2. Make `assets/eye_photos/dragon11/`, copy an `eye.json` into it and set `id` (config value,
   max 15 chars), `name` (shown on the web page), and `open` / `closed` (paths relative to
   the folder, e.g. `../../eye_art/23.png`).
3. Measure the geometry (below) and run:
   ```
   python tools/photo_eye.py assets/eye_photos/dragon11 --preview
   ```
   Check `preview.png` (and `preview.gif`, not committed: look around, pupil, blink).
   Adjust `eye.json` and repeat.
4. In `src/eye_styles.cpp` add
   ```cpp
   namespace eye_dragon11 {
   #include "eyes/dragon11Photo.h"
   PHOTO_STYLE_DEF("dragon11", "Dragon 11")
   }
   ```
   and `&eye_dragon11::style` to `EYE_STYLES`.
5. Build, flash, pick it on the web page (Config -> Display & Interface).

## eye.json

All values are 240 px screen coordinates (x right, y down). An easy way to measure:
scale the open picture to 720 px with a 10 px grid (labels in 240 units) and read the
points off it.

| Key | Meaning |
|-----|---------|
| `upper_edge`, `lower_edge` | Polylines `[[x, y], ...]` of the open eye's lid edges (where lid meets eyeball), corner to corner; both start and end at the corners |
| `seam` | Where the lids meet in the shut picture. May reach past the corners, where the lids still move |
| `upper_sweep` | Optional: where the upper lid region starts changing (brow moves too). Default: `sweep_up` px above `upper_edge` |
| `sweep_up`, `sweep_lo` | Default sweep margins above/below the lid edges (px) |
| `iris` | Ellipse `cx, cy, rx, ry` around the iris **including** its dark outer ring |
| `open`, `closed` | Paths of the pictures, relative to the folder (default `open.png`, `closed.png` in it) |
| `pupil` | Pupil as drawn: centre `cx, cy`, half width `w`, half height `h`; optional `tilt` in degrees (+ = bottom leans right) |
| `pupil_range` | Half width `w_min`..`w_max` and half height `h_min`..`h_max` over slit..wide |
| `gaze_x`, `gaze_y` | How far the iris moves (px) |
| `droop` | Upper lid lowering when looking fully down (share of edge -> seam) |
| `crop_inset` | Source px trimmed inside the picture's edge |

Hidden parts of the iris (under the lid or a highlight) are filled from the mirror
image (top <-> bottom), so keep `gaze_y` small when a lot of the iris is hidden.

## Artist spec

- Two pictures of the same artwork: eye open, eye shut, identical framing (only the
  eye area should differ).
- Round picture on a transparent or black square, PNG preferred, 1080 px is plenty.
- Open eye: a slit (or round) pupil drawn fairly narrow, iris with a dark outer ring;
  reflections are fine (they stay fixed on screen).
- The more of the iris that is visible (not under the lid), the further it can move.
- Rights: the repository is public; only commit pictures the user may publish.
