#!/usr/bin/env python3
"""Photo eye converter: turns an artist's open/closed eye pictures into firmware data.

    python tools/photo_eye.py assets/eye_photos/dragon2            # writes the header
    python tools/photo_eye.py assets/eye_photos/dragon2 --preview  # also preview PNG + GIF

The folder holds open.png, closed.png (same artwork, eye open and shut, round picture on
a transparent or black square) and eye.json (geometry, in 240 px screen coordinates;
see docs/PHOTO_EYES.md). Output: include/eyes/<id>Photo.h, used by src/eye_styles.cpp,
drawn by src/photo_eye.cpp. Needs Pillow and numpy.
"""

import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

OUT = 240        # Screen size
HI = 3           # Analysis resolution = OUT * HI
ANGLES = 512     # Iris texture angles (must match photo_eye.cpp)
TEX_R = 64       # Iris texture radial samples, pupil edge -> ring outer edge
TABLE = 120      # Per-pixel iris table size (pixels around the pupil centre)
Q = 4            # Radius units per pixel in the per-pixel table (quarter pixels)
FX = 16          # Column positions in 1/16 px

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# ----------------------------------------------------------------------------
# Loading
# ----------------------------------------------------------------------------

def load_crop(path, inset, size):
    """Crop the round picture (found from its alpha or non-black pixels) and scale."""
    im = Image.open(path).convert('RGBA')
    a = np.array(im)
    vis = (a[..., 3] > 128) & (a[..., :3].astype(int).sum(2) > 30)
    ys, xs = np.nonzero(vis)
    cx, cy = (xs.min() + xs.max() + 1) / 2, (ys.min() + ys.max() + 1) / 2
    h = (xs.max() - xs.min() + 1 + ys.max() - ys.min() + 1) / 4 - inset
    box = (cx - h, cy - h, cx + h, cy + h)
    return im.convert('RGB').resize((size, size), Image.LANCZOS, box=box)


def polyline(points):
    """Per-column y (pixel-centre x = i + 0.5) from [[x, y], ...]; NaN outside."""
    pts = sorted(points)
    px = np.array([p[0] for p in pts], float)
    py = np.array([p[1] for p in pts], float)
    xc = np.arange(OUT) + 0.5
    y = np.interp(xc, px, py)
    y[(xc < px[0]) | (xc > px[-1])] = np.nan
    return y


def rgb565(rgb):
    rgb = np.asarray(rgb).astype(np.uint16)
    return ((rgb[..., 0] >> 3) << 11) | ((rgb[..., 1] >> 2) << 5) | (rgb[..., 2] >> 3)


def bilinear(img, u, v):
    """Sample float image (H, W, C) at continuous coords (u, v) in its own pixel units."""
    hgt, wid = img.shape[:2]
    x = np.clip(u - 0.5, 0, wid - 1.001)
    y = np.clip(v - 0.5, 0, hgt - 1.001)
    x0 = x.astype(int); y0 = y.astype(int)
    fx = (x - x0)[..., None]; fy = (y - y0)[..., None]
    a = img[y0, x0]; b = img[y0, x0 + 1]; c = img[y0 + 1, x0]; d = img[y0 + 1, x0 + 1]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


# ----------------------------------------------------------------------------
# Geometry (the lens and ellipse formulas must match photo_eye.cpp)
# ----------------------------------------------------------------------------

def lens_radius(w, h, c, s):
    """Distance from the pupil centre to the edge of a slit pupil (|x| <= w(1-(y/h)^2))
    along direction (c, s)."""
    a = w * s * s / (h * h)
    return 2 * w / (np.abs(c) + np.sqrt(c * c + 4 * a * w))


def ellipse_radius(px, py, ex, ey, rx, ry, c, s):
    """Distance from (px, py) along (c, s) to the ellipse centred (ex, ey)."""
    dx, dy = (px - ex) / rx, (py - ey) / ry
    cx, cy = c / rx, s / ry
    a = cx * cx + cy * cy
    b = 2 * (dx * cx + dy * cy)
    k = dx * dx + dy * dy - 1
    return (-b + np.sqrt(b * b - 4 * a * k)) / (2 * a)


# ----------------------------------------------------------------------------
# Build
# ----------------------------------------------------------------------------

def build(folder):
    cfg = json.load(open(os.path.join(folder, 'eye.json')))
    inset = cfg.get('crop_inset', 5)
    op240 = np.array(load_crop(os.path.join(folder, 'open.png'), inset, OUT)).astype(float)
    cl240 = np.array(load_crop(os.path.join(folder, 'closed.png'), inset, OUT)).astype(float)
    opHI = np.array(load_crop(os.path.join(folder, 'open.png'), inset, OUT * HI)).astype(float)

    up_edge = polyline(cfg['upper_edge'])
    lo_edge = polyline(cfg['lower_edge'])
    seam = polyline(cfg['seam'])
    eye_cols = ~np.isnan(up_edge) & ~np.isnan(lo_edge) & (lo_edge > up_edge)
    seam = np.where(np.isnan(seam) & eye_cols, (up_edge + lo_edge) / 2, seam)
    seam = np.where(eye_cols, np.clip(seam, up_edge, lo_edge), seam)
    # Where the upper lid starts to change (the brow moves too); upper_sweep, if given,
    # overrides it and can reach past the eye corners
    up_start = up_edge - cfg.get('sweep_up', 10)
    if 'upper_sweep' in cfg:
        us = polyline(cfg['upper_sweep'])
        up_start = np.where(np.isnan(us), up_start, us)
    lo_start = lo_edge + cfg.get('sweep_lo', 4)
    lid_cols = ~np.isnan(seam) & ~np.isnan(up_start)
    up_start = np.minimum(up_start, np.where(eye_cols, up_edge, seam))
    # Past the corners there is no eye: the lid sweeps evenly from its start to the seam
    k = 0.15
    lid_up = np.where(eye_cols, up_edge, up_start + k * (seam - up_start))
    lid_lo = np.where(eye_cols, lo_edge, seam)
    lo_start = np.where(eye_cols, lo_start, seam)

    iris = cfg['iris']        # Ring outer ellipse: cx, cy, rx, ry
    pupil = cfg['pupil']      # Slit pupil as drawn: cx, cy, w (half width), h (half height)
    pcx, pcy = pupil['cx'], pupil['cy']

    def in_eye(u, v, margin=0.0):
        col = np.clip(u.astype(int), 0, OUT - 1)
        ok = eye_cols[col]
        return ok & (v > np.nan_to_num(up_edge[col]) + margin) & (v < np.nan_to_num(lo_edge[col]) - margin)

    # Highlights (cornea reflections): bright, unsaturated pixels on the iris. They stay
    # put while the iris moves under them.
    hsv = np.array(Image.fromarray(opHI.astype(np.uint8)).convert('HSV')).astype(float)
    sat, val = hsv[..., 1], hsv[..., 2]
    hl = np.clip((val - 120) / 80, 0, 1) * np.clip((110 - sat) / 60, 0, 1)
    uu, vv = np.meshgrid((np.arange(OUT * HI) + 0.5) / HI, (np.arange(OUT * HI) + 0.5) / HI)
    ell = ((uu - iris['cx']) / iris['rx']) ** 2 + ((vv - iris['cy']) / iris['ry']) ** 2 < 1
    hl *= ell & in_eye(uu, vv, 1.5)
    hlHI = hl
    hl240 = np.clip(hl.reshape(OUT, HI, OUT, HI).mean((1, 3)) * 2, 0, 1)  # Art pixel already holds it
    hl240[hl240 < 0.1] = 0

    # Iris texture: unwrap from the pupil edge (as drawn) out to the ring's outer edge
    ang = (np.arange(ANGLES) + 0.5) * 2 * math.pi / ANGLES
    c, s = np.cos(ang), np.sin(ang)
    pb0 = lens_radius(pupil['w'], pupil['h'], c, s)
    rim = ellipse_radius(pcx, pcy, iris['cx'], iris['cy'], iris['rx'], iris['ry'], c, s)
    frac = (np.arange(TEX_R) + 0.5) / TEX_R
    r = pb0[:, None] + frac[None, :] * (rim - pb0)[:, None]
    su = pcx + r * c[:, None]
    sv = pcy + r * s[:, None]
    soft = np.array(Image.fromarray(opHI.astype(np.uint8)).filter(ImageFilter.GaussianBlur(HI * 0.5))).astype(float)
    tex = bilinear(soft, su * HI, sv * HI)  # Blurred to screen detail: no speckle aliasing
    hls = bilinear(hlHI[..., None], su * HI, sv * HI)[..., 0]
    valid = in_eye(su, sv, 0.8) & (hls < 0.1)
    # Hidden or highlighted samples: take the mirror image (top <-> bottom), then the
    # nearest valid sample along the same angle, then the nearest filled angle
    mirror = (ANGLES - 1 - np.arange(ANGLES))
    fill = ~valid & valid[mirror]
    tex[fill] = tex[mirror][fill]
    valid = valid | fill
    for a in range(ANGLES):
        vi = np.nonzero(valid[a])[0]
        if len(vi) == 0:
            continue
        for d in np.nonzero(~valid[a])[0]:
            tex[a, d] = tex[a, vi[np.abs(vi - d).argmin()]]
        valid[a] = True
    good = np.nonzero(valid.all(1))[0]
    for a in np.nonzero(~valid.all(1))[0]:
        dist = np.minimum(np.abs(good - a), ANGLES - np.abs(good - a))
        tex[a] = tex[good[dist.argmin()]]

    # Per-pixel table around the pupil centre: angle and radius (quarter px)
    tx0 = int(round(pcx)) - TABLE // 2
    ty0 = int(round(pcy)) - TABLE // 2
    gx, gy = np.meshgrid(np.arange(TABLE) + tx0 + 0.5 - pcx, np.arange(TABLE) + ty0 + 0.5 - pcy)
    pa = (np.arctan2(gy, gx) % (2 * math.pi)) * ANGLES / (2 * math.pi)
    pix_ang = np.minimum(pa.astype(int), ANGLES - 1).astype(np.uint16)
    pr = np.round(np.hypot(gx, gy) * Q)
    pix_rad = np.where(pr > 254, 255, pr).astype(np.uint8)

    # Open picture with the iris taken out (the socket the iris moves over)
    base = op240.copy()
    u240, v240 = np.meshgrid(np.arange(OUT) + 0.5, np.arange(OUT) + 0.5)
    e = ((u240 - iris['cx']) / iris['rx']) ** 2 + ((v240 - iris['cy']) / iris['ry']) ** 2
    inside = in_eye(u240, v240)
    band = inside & (e > 0.85) & (e < 1.0)
    fill_rgb = np.median(op240[band], axis=0) if band.any() else np.array([12, 18, 22])
    base[inside & (e < 1.0)] = fill_rgb

    # Highlight patch (bounding box)
    ys, xs = np.nonzero(hl240)
    hx0, hy0 = xs.min(), ys.min()
    hw, hh = xs.max() - hx0 + 1, ys.max() - hy0 + 1
    hl_rgb = rgb565(op240[hy0:hy0 + hh, hx0:hx0 + hw].round())
    hl_a = np.round(hl240[hy0:hy0 + hh, hx0:hx0 + hw] * 255).astype(np.uint8)

    def col(v, cols):  # 1/16 px, -1 where the column has no lid (or no eye)
        return np.where(cols, np.round(np.nan_to_num(v) * FX), -1).astype(np.int16)

    pr_cfg = cfg.get('pupil_range', {})
    return dict(
        cfg=cfg,
        open=rgb565(base.round()), closed=rgb565(cl240.round()),
        up_start=col(up_start, lid_cols), up_edge=col(lid_up, lid_cols), seam=col(seam, lid_cols),
        lo_edge=col(lid_lo, lid_cols), lo_start=col(lo_start, lid_cols),
        eye_top=col(up_edge, eye_cols), eye_bot=col(lo_edge, eye_cols),
        tex=rgb565(np.clip(tex, 0, 255).round()),
        rim=np.round(rim * Q).astype(np.uint16),
        pix_ang=pix_ang, pix_rad=pix_rad, tx0=tx0, ty0=ty0,
        hl=(hx0, hy0, hw, hh, hl_rgb, hl_a),
        params=dict(
            pupil_x=pcx, pupil_y=pcy, pupil_w=pupil['w'], pupil_h=pupil['h'],
            w_min=pr_cfg.get('w_min', 1.2), w_max=pr_cfg.get('w_max', 16.0),
            h_min=pr_cfg.get('h_min', pupil['h'] * 0.95), h_max=pr_cfg.get('h_max', pupil['h'] * 1.15),
            range_x=cfg.get('gaze_x', 6), range_y=cfg.get('gaze_y', 4), droop=cfg.get('droop', 0.12)),
        preview_src=(op240, cl240),
    )


# ----------------------------------------------------------------------------
# Header output
# ----------------------------------------------------------------------------

def c_array(ctype, name, data, per_line, fmt):
    flat = np.asarray(data).ravel()
    lines = []
    for i in range(0, len(flat), per_line):
        lines.append('  ' + ', '.join(fmt(int(v)) for v in flat[i:i + per_line]) + ',')
    return 'static const %s %s[%d] = {\n%s\n};\n' % (ctype, name, len(flat), '\n'.join(lines))


def write_header(folder, d):
    cfg = d['cfg']
    sid = cfg['id']
    path = os.path.join(REPO, 'include', 'eyes', '%sPhoto.h' % sid)
    h16 = lambda v: '0x%04X' % v
    dec = str
    hx0, hy0, hw, hh, hl_rgb, hl_a = d['hl']
    p = d['params']
    rel = os.path.relpath(folder, REPO).replace('\\', '/')
    out = []
    out.append('// Photo eye "%s", generated by tools/photo_eye.py from %s/ - do not edit.\n'
               '// Artwork: %s. Included inside a namespace by src/eye_styles.cpp.\n\n'
               % (cfg['name'], rel, cfg.get('credit', 'commissioned by the project owner')))
    out.append(c_array('uint16_t', 'open_img', d['open'], 16, h16))
    out.append(c_array('uint16_t', 'closed_img', d['closed'], 16, h16))
    for n in ('up_start', 'up_edge', 'seam', 'lo_edge', 'lo_start', 'eye_top', 'eye_bot'):
        out.append(c_array('int16_t', n, d[n], 16, dec))
    out.append(c_array('uint16_t', 'iris_tex', d['tex'], 16, h16))
    out.append(c_array('uint16_t', 'iris_rim', d['rim'], 16, dec))
    out.append(c_array('uint16_t', 'pix_ang', d['pix_ang'], 20, dec))
    out.append(c_array('uint8_t', 'pix_rad', d['pix_rad'], 24, dec))
    out.append(c_array('uint16_t', 'hl_rgb', hl_rgb, 16, h16))
    out.append(c_array('uint8_t', 'hl_alpha', hl_a, 24, dec))
    out.append('\nstatic const PhotoEye photo = {\n'
               '  open_img, closed_img, up_start, up_edge, seam, lo_edge, lo_start, eye_top, eye_bot,\n'
               '  iris_tex, iris_rim, pix_ang, pix_rad, %d, %d, %d,\n'
               '  hl_rgb, hl_alpha, %d, %d, %d, %d,\n'
               '  %.2ff, %.2ff, %.2ff, %.2ff,  // Pupil centre x, y; half width, half height as drawn\n'
               '  %.2ff, %.2ff, %.2ff, %.2ff,  // Pupil half width min/max, half height min/max\n'
               '  %.1ff, %.1ff, %.2ff,           // Gaze range x, y (px); upper lid droop looking down\n'
               '};\n'
               % (TABLE, d['tx0'], d['ty0'], hx0, hy0, hw, hh,
                  p['pupil_x'], p['pupil_y'], p['pupil_w'], p['pupil_h'],
                  p['w_min'], p['w_max'], p['h_min'], p['h_max'],
                  p['range_x'], p['range_y'], p['droop']))
    with open(path, 'w', newline='\n') as f:
        f.write(''.join(out))
    print('Wrote', os.path.relpath(path, REPO), '(%d KB of data)' % (
        (d['open'].size * 4 + d['tex'].size * 2 + d['pix_ang'].size * 3 + 5 * OUT * 2) // 1024))


# ----------------------------------------------------------------------------
# Preview: same drawing steps as photo_eye.cpp, in numpy
# ----------------------------------------------------------------------------

def unpack(img565):
    r = (img565 >> 11) & 31; g = (img565 >> 5) & 63; b = img565 & 31
    return np.stack([(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)], -1).astype(np.uint8)


def lid_pos(start, edge, seam, closure, droop):
    k = 0.15
    if closure <= k:
        pos = start + (edge - start) * closure / k
    else:
        pos = edge + (seam - edge) * (closure - k) / (1 - k)
    return np.maximum(pos, edge + (seam - edge) * droop) if droop else pos


def render(d, gx, gy, cu, cl, pupil):
    p = d['params']
    w = p['w_min'] + (p['w_max'] - p['w_min']) * pupil
    h = p['h_min'] + (p['h_max'] - p['h_min']) * pupil
    ang = (np.arange(ANGLES) + 0.5) * 2 * math.pi / ANGLES
    pb = np.round(lens_radius(w, h, np.cos(ang), np.sin(ang)) * Q).astype(int)
    rim = d['rim'].astype(int)
    droop = p['droop'] * max(0.0, gy / p['range_y']) if p['range_y'] else 0.0
    out = d['open'].copy()
    ys, xs = np.mgrid[0:OUT, 0:OUT]
    y16 = ys * FX + FX // 2
    cols = d['seam'] >= 0
    upos = np.where(cols, lid_pos(d['up_start'], d['up_edge'], d['seam'], cu, droop), -1)
    lpos = np.where(cols, lid_pos(d['lo_start'], d['lo_edge'], d['seam'], cl, 0), 99999)
    covered = cols[None, :] & (((y16 >= d['up_start'][None, :]) & (y16 < upos[None, :])) |
                               ((y16 > lpos[None, :]) & (y16 <= d['lo_start'][None, :])))
    eye = (y16 > d['eye_top'][None, :]) & (y16 < d['eye_bot'][None, :]) & ~covered
    ix = xs - gx - d['tx0']; iy = ys - gy - d['ty0']
    inside = eye & (ix >= 0) & (ix < TABLE) & (iy >= 0) & (iy < TABLE)
    ixc = np.clip(ix, 0, TABLE - 1); iyc = np.clip(iy, 0, TABLE - 1)
    a = d['pix_ang'].reshape(TABLE, TABLE)[iyc, ixc].astype(int)
    r = d['pix_rad'].reshape(TABLE, TABLE)[iyc, ixc].astype(int)
    iris_px = inside & (r < rim[a])
    pup = iris_px & (r < pb[a])
    t = iris_px & ~pup
    dd = np.clip((r - pb[a]) * TEX_R // np.maximum(rim[a] - pb[a], 1), 0, TEX_R - 1)
    img = unpack(out).astype(float)
    img[t] = unpack(d['tex'].reshape(ANGLES, TEX_R)[a[t], dd[t]])
    edge = t & (r < pb[a] + Q)  # Soften the pupil edge
    img[edge] *= 0.5
    img[pup] = 0
    hx0, hy0, hw, hh, hl_rgb, hl_a = d['hl']
    al = np.zeros((OUT, OUT)); hc = np.zeros((OUT, OUT, 3))
    al[hy0:hy0 + hh, hx0:hx0 + hw] = hl_a / 255.0
    hc[hy0:hy0 + hh, hx0:hx0 + hw] = unpack(hl_rgb)
    al *= eye
    img = img * (1 - al[..., None]) + hc * al[..., None]
    shade = eye & (cu > 0.15) & (y16 < upos[None, :] + 3 * FX)
    img[shade] *= 0.5
    img[covered] = unpack(d['closed'])[covered]
    circ = (xs + 0.5 - OUT / 2) ** 2 + (ys + 0.5 - OUT / 2) ** 2 > (OUT / 2) ** 2
    img[circ] = 0
    return Image.fromarray(img.clip(0, 255).astype(np.uint8))


def preview(folder, d):
    p = d['params']
    rx, ry = int(p['range_x']), int(p['range_y'])
    states = [
        ('rest', 0, 0, 0, 0, 0.45), ('look left', -rx, 0, 0, 0, 0.45),
        ('look right', rx, 0, 0, 0, 0.45), ('look up', 0, -ry, 0, 0, 0.45),
        ('look down', 0, ry, 0, 0, 0.45), ('slit', 0, 0, 0, 0, 0.0),
        ('wide', 0, 0, 0, 0, 1.0), ('blink 30%', 0, 0, 0.3, 0.3, 0.45),
        ('blink 60%', 0, 0, 0.6, 0.6, 0.45), ('blink 85%', 0, 0, 0.85, 0.85, 0.45),
        ('shut', 0, 0, 1, 1, 0.45), ('original', None, 0, 0, 0, 0),
    ]
    tiles = []
    for name, gx, gy, cu, cl, pu in states:
        if gx is None:
            tiles.append(Image.fromarray(d['preview_src'][0].astype(np.uint8)))
        else:
            tiles.append(render(d, gx, gy, cu, cl, pu))
    sheet = Image.new('RGB', (OUT * 4, OUT * 3))
    for i, t in enumerate(tiles):
        sheet.paste(t, ((i % 4) * OUT, (i // 4) * OUT))
    sp = os.path.join(folder, 'preview.png')
    sheet.save(sp)
    frames = []
    n = 60
    for i in range(n):  # Look around, pupil breathes, one blink
        t = i / n
        gx = int(round(rx * math.sin(2 * math.pi * t)))
        gy = int(round(ry * math.sin(4 * math.pi * t)))
        pu = 0.5 + 0.5 * math.sin(2 * math.pi * t + 1)
        b = max(0.0, 1 - abs(i - 45) / 4)
        frames.append(render(d, gx, gy, b, b, pu))
    gp = os.path.join(folder, 'preview.gif')
    frames[0].save(gp, save_all=True, append_images=frames[1:], duration=66, loop=0)
    print('Wrote', os.path.relpath(sp, REPO), 'and', os.path.relpath(gp, REPO))


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    folder = os.path.abspath(sys.argv[1])
    data = build(folder)
    write_header(folder, data)
    if '--preview' in sys.argv:
        preview(folder, data)
