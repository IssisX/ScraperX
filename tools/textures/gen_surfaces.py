#!/usr/bin/env python3
"""Makes the structure's surface maps (environment plan, job 6: real surfaces).

Every map here is made in code from seeded noise: no photograph, no outside
source, so nothing to licence (CC0 by authorship; ui/licences.gd says so).
The same seeds give the same maps, byte for byte.

For each surface kind two maps are written to godot/presentation/textures/:

  <kind>_albedo.png  RGB: a linear multiplier on the part's own colour, which
                     the batcher carries in its vertices (so one map serves
                     every colour of that kind). Its linear mean is held at
                     MEAN_LINEAR, the mean of the grime map it replaces
                     (main_geometry.gd _make_weathering_texture: 0.723), so
                     job 2's measured value ladder still holds.
  <kind>_rough.png   L: a roughness multiplier (linear), on its own: one
                     texture is never sampled both as colour and as data.
  <kind>_normal.png  RGB: a tangent-space normal map, +Y up the image
                     (Godot's convention: binormal = -dP/dv).

Each map tiles in both directions and covers TILE metres (u, v), so a
texel is a known size: 512 px to the metre on the steel, so it holds up at
arm's length. Broad variation over tens of metres is not here: the batcher
lays it in the vertex colours (main_geometry.gd Batcher.broad_tint), so the
tile never shows its repeat at 50 m.

Run: python3 tools/textures/gen_surfaces.py   (numpy and Pillow only)
"""

import os
import sys

import numpy as np
from PIL import Image

OUT = os.path.join(os.path.dirname(__file__), "..", "..", "godot", "presentation", "textures")
MEAN_LINEAR = 0.723


# ---------------------------------------------------------------- helpers

def spectral(shape, beta, seed, fmin=1.0, fmax=None, aniso=(1.0, 1.0)):
    """Periodic noise with a power-law spectrum (amplitude ~ f^(-beta/2)).
    fmin/fmax are in cycles per tile; aniso weighs the frequency axes (v, u):
    aniso=(1, 25) damps change along u, so features run long along u. Zero
    mean, unit std."""
    h, w = shape
    rng = np.random.default_rng(seed)
    white = rng.standard_normal(shape)
    fy = np.fft.fftfreq(h)[:, None] * h * aniso[0]
    fx = np.fft.fftfreq(w)[None, :] * w * aniso[1]
    f = np.sqrt(fx * fx + fy * fy)
    f[0, 0] = 1.0
    amp = f ** (-beta / 2.0)
    amp[f < fmin] = 0.0
    if fmax is not None:
        amp[f > fmax] = 0.0
    amp[0, 0] = 0.0
    out = np.real(np.fft.ifft2(np.fft.fft2(white) * amp))
    out -= out.mean()
    return out / (out.std() + 1e-12)


def worley(shape, cells, seed):
    """Periodic cellular noise on a jittered grid: (F1, F2, cell id), the
    distances in pixels. `cells` = (rows, cols) of the grid."""
    h, w = shape
    gy, gx = cells
    rng = np.random.default_rng(seed)
    jit = rng.random((gy, gx, 2))
    ids = rng.permutation(gy * gx).reshape(gy, gx)
    csy, csx = h / gy, w / gx
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float64)
    cy = np.floor(yy / csy).astype(int)
    cx = np.floor(xx / csx).astype(int)
    f1 = np.full(shape, 1e9)
    f2 = np.full(shape, 1e9)
    cid = np.zeros(shape, dtype=int)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            ny, nx = cy + dy, cx + dx
            wy, wx = ny % gy, nx % gx
            py = (ny + jit[wy, wx, 0]) * csy
            px = (nx + jit[wy, wx, 1]) * csx
            d = np.hypot(yy - py, xx - px)
            closer = d < f1
            f2 = np.where(closer, f1, np.minimum(f2, d))
            cid = np.where(closer, ids[wy, wx], cid)
            f1 = np.where(closer, d, f1)
    return f1, f2, cid


def stamp(field, centres_px, radii_px, depth, profile="crater"):
    """Adds round marks into `field` (periodic). profile: 'crater' (a bowl),
    'dome' (a bump)."""
    h, w = field.shape
    for (cy, cx), r, d in zip(centres_px, radii_px, depth):
        rr = int(np.ceil(r)) + 1
        ys = np.arange(int(cy) - rr, int(cy) + rr + 1)
        xs = np.arange(int(cx) - rr, int(cx) + rr + 1)
        dy = ys[:, None] - cy
        dx = xs[None, :] - cx
        t = np.clip(1.0 - (dx * dx + dy * dy) / (r * r), 0.0, 1.0)
        bump = d * (t * t if profile == "crater" else np.sqrt(t))
        field[np.ix_(ys % h, xs % w)] += bump


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def blur(field, passes=1):
    for _ in range(passes):
        field = (field * 4 + np.roll(field, 1, 0) + np.roll(field, -1, 0)
                 + np.roll(field, 1, 1) + np.roll(field, -1, 1)) / 8.0
    return field


def normal_map(height_m, px_m):
    """Height in metres over pixels px_m = (v, u) metres -> RGB normal map,
    +Y up the image: n = (-dh/du, +dh/dv, 1) normalised."""
    dhdu = (np.roll(height_m, -1, 1) - np.roll(height_m, 1, 1)) / (2.0 * px_m[1])
    dhdv = (np.roll(height_m, -1, 0) - np.roll(height_m, 1, 0)) / (2.0 * px_m[0])
    n = np.stack([-dhdu, dhdv, np.ones_like(dhdu)], axis=-1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return np.clip(np.round((n * 0.5 + 0.5) * 255.0), 0, 255).astype(np.uint8)


def linear_to_srgb(x):
    x = np.clip(x, 0.0, 1.0)
    return np.where(x <= 0.0031308, 12.92 * x, 1.055 * np.power(x, 1.0 / 2.4) - 0.055)


def albedo_map(lum, tint, rough):
    """lum: linear luminance multiplier field; tint: (h, w, 3) per-channel
    factors around 1. Scales lum so the linear mean of the result is
    MEAN_LINEAR, clipped at 1. Returns (RGB uint8, roughness L uint8, mean)."""
    rgb = lum[..., None] * tint
    for _ in range(6):
        mean = np.clip(rgb, 0.0, 1.0).mean()
        rgb *= MEAN_LINEAR / mean
    rgb = np.clip(rgb, 0.0, 1.0)
    srgb = np.clip(np.round(linear_to_srgb(rgb) * 255.0), 0, 255).astype(np.uint8)
    rough8 = np.clip(np.round(np.clip(rough, 0.0, 1.0) * 255.0), 0, 255).astype(np.uint8)
    return (srgb, rough8), float(rgb.mean())


def tint_field(t, warm, cool=(1.0, 1.0, 1.0)):
    """t in about [-1, 1] -> per-channel factors: +t toward the `warm`
    (r, g, b) factors, -t toward the `cool` ones (neutral unless named)."""
    warm = np.asarray(warm)[None, None, :]
    cool = np.asarray(cool)[None, None, :]
    tp = np.clip(t, 0.0, 1.0)[..., None]
    tn = np.clip(-t, 0.0, 1.0)[..., None]
    return 1.0 + tp * (warm - 1.0) + tn * (cool - 1.0)


def write(kind, albedo, normal):
    os.makedirs(OUT, exist_ok=True)
    Image.fromarray(albedo[0], "RGB").save(os.path.join(OUT, f"{kind}_albedo.png"), optimize=True)
    Image.fromarray(albedo[1], "L").save(os.path.join(OUT, f"{kind}_rough.png"), optimize=True)
    Image.fromarray(normal, "RGB").save(os.path.join(OUT, f"{kind}_normal.png"), optimize=True)


# ---------------------------------------------------------------- kinds

def rust(seed=101, n=512, tile=1.0):
    """Heavy oxide on old steel: fine grain, pits a few millimetres across,
    and lifted scale -- flakes with sharp edges, a shade brighter and more
    orange where fresh oxide shows, darker in the pits."""
    px = tile / n
    rng = np.random.default_rng(seed)
    grain = spectral((n, n), 1.6, seed + 1, fmin=40)
    mid = spectral((n, n), 2.6, seed + 2, fmin=6, fmax=160)
    broad = spectral((n, n), 3.5, seed + 3, fmin=1, fmax=6)
    flake = smoothstep(0.45, 0.6, mid + 0.3 * grain)
    pits = np.zeros((n, n))
    count = 2200
    stamp(pits, rng.random((count, 2)) * n, rng.uniform(0.8, 4.0, count) * 1e-3 / px * rng.uniform(0.5, 1.0, count),
          rng.uniform(0.6, 1.0, count))
    pits = np.clip(pits, 0.0, 1.0)
    # Magnetite: the black oxide under the red, in specks.
    specks = smoothstep(1.6, 2.4, spectral((n, n), 1.2, seed + 4, fmin=60))
    height = 0.0003 * grain + 0.0006 * flake - 0.0012 * pits + 0.0002 * mid
    lum = 0.80 + 0.12 * grain + 0.12 * flake - 0.40 * pits - 0.35 * specks + 0.03 * broad
    t = 0.7 * flake - 0.6 * pits - 0.6 * specks + 0.2 * grain
    tint = tint_field(np.clip(t, -1, 1), (1.10, 0.95, 0.72), (0.92, 0.86, 0.84))
    rough = 0.94 + 0.04 * grain * 0.5 + 0.06 * pits - 0.05 * flake
    alb, mean = albedo_map(lum, tint, rough)
    return alb, normal_map(height, (px, px)), mean


def concrete(seed=202, n=512, tile=1.5):
    """Cast concrete: sand grain, aggregate showing through the skin in
    flecks a few millimetres across, air holes (bugholes), and the cement's
    own slow mottling."""
    px = tile / n
    rng = np.random.default_rng(seed)
    sand = spectral((n, n), 1.6, seed + 1, fmin=60)
    mottle = spectral((n, n), 3.2, seed + 2, fmin=1, fmax=20)
    agg = np.zeros((n, n))
    count = 2600
    stamp(agg, rng.random((count, 2)) * n, rng.uniform(1.0, 4.5, count) * 1e-3 / px,
          rng.choice([-1.0, 1.0], count) * rng.uniform(0.4, 1.0, count), profile="dome")
    holes = np.zeros((n, n))
    count = 520
    stamp(holes, rng.random((count, 2)) * n, rng.uniform(0.8, 3.5, count) * 1e-3 / px, rng.uniform(0.7, 1.0, count))
    holes = np.clip(holes, 0.0, 1.0)
    height = 0.00012 * sand + 0.00025 * np.abs(agg) - 0.0012 * holes
    lum = 0.80 + 0.06 * sand + 0.12 * agg + 0.07 * mottle - 0.45 * holes
    tint = tint_field(np.clip(0.4 * mottle + 0.3 * agg, -1, 1), (1.04, 1.0, 0.95))
    rough = 0.96 + 0.03 * sand + 0.04 * holes - 0.03 * np.abs(agg)
    alb, mean = albedo_map(lum, tint, rough)
    return alb, normal_map(height, (px, px)), mean


def paint(seed=303, n=512, tile=1.0):
    """Industrial enamel on steel, years out: a fine orange peel, dirt
    settled in a slow mottle, and chips -- small, clustered, sharp-edged --
    down to dark rusted steel, rough where the gloss is gone."""
    px = tile / n
    peel = spectral((n, n), 2.4, seed + 1, fmin=60, fmax=260)
    chipfield = spectral((n, n), 2.2, seed + 2, fmin=10, fmax=300)
    cluster = spectral((n, n), 3.5, seed + 3, fmin=1, fmax=6)
    dirt = spectral((n, n), 3.0, seed + 4, fmin=2, fmax=40)
    fine = spectral((n, n), 1.5, seed + 5, fmin=80)
    chip = smoothstep(1.9, 2.05, chipfield + 0.8 * cluster)
    lip = np.clip(blur(chip, 2) - chip, 0.0, 1.0)
    height = 0.000025 * peel - 0.00025 * chip + 0.0002 * lip
    specks = smoothstep(2.0, 2.6, spectral((n, n), 1.2, seed + 6, fmin=80))
    lum = 0.92 - 0.12 * smoothstep(-0.5, 1.5, dirt) + 0.03 * fine - 0.25 * specks
    tint = np.ones((n, n, 3))
    under = np.asarray([0.50, 0.38, 0.30])[None, None, :]
    tint = tint * (1.0 - chip[..., None]) + under * chip[..., None]
    rough = 0.82 + 0.06 * smoothstep(-0.5, 1.5, dirt) + 0.18 * chip + 0.02 * fine
    alb, mean = albedo_map(lum, tint, rough)
    return alb, normal_map(height, (px, px)), mean


def galvanised(seed=404, n=512, tile=1.0):
    """Hot-dip zinc: spangle crystals 8 to 25 mm across, each tilted a
    little and a little brighter or duller than its neighbours, so the sky
    breaks up across it; grey-white blotches of zinc carbonate."""
    px = tile / n
    rng = np.random.default_rng(seed)
    f1, f2, cid = worley((n, n), (64, 64), seed + 1)
    per = rng.random((64 * 64, 3))
    shade = per[cid, 0] - 0.5
    tilt_u = per[cid, 1] - 0.5
    tilt_v = per[cid, 2] - 0.5
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float64)
    edge = 1.0 - smoothstep(0.0, 1.6, f2 - f1)
    patina = smoothstep(0.4, 1.4, spectral((n, n), 3.2, seed + 2, fmin=2, fmax=30))
    fine = spectral((n, n), 1.6, seed + 3, fmin=90)
    # A tilt as a slope, not a height: written straight into the normal.
    slope = 0.035
    lum = 0.86 + 0.10 * shade - 0.08 * edge + 0.10 * patina + 0.02 * fine
    tint = tint_field(patina, (1.0, 1.01, 1.03))
    rough = 0.80 + 0.22 * (per[cid, 0] - 0.5) + 0.2 * patina + 0.05 * edge
    alb, mean = albedo_map(lum, tint, rough)
    height = 0.00002 * fine - 0.00006 * edge
    dhdu = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) / (2.0 * px) + slope * tilt_u * (1.0 - patina)
    dhdv = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) / (2.0 * px) + slope * tilt_v * (1.0 - patina)
    nrm = np.stack([-dhdu, dhdv, np.ones_like(dhdu)], axis=-1)
    nrm /= np.linalg.norm(nrm, axis=-1, keepdims=True)
    nrm = np.clip(np.round((nrm * 0.5 + 0.5) * 255.0), 0, 255).astype(np.uint8)
    return alb, nrm, mean


def mill_scale(seed=505, n=512, tile=1.0):
    """Mill scale: the blue-black forged skin, crazed with fine cracks, lifted
    off in patches where rust has got under it -- rougher and warmer there."""
    px = tile / n
    f1, f2, _ = worley((n, n), (40, 40), seed + 1)
    craze = 1.0 - smoothstep(0.0, 1.3, f2 - f1)
    lift = smoothstep(0.55, 0.85, spectral((n, n), 2.8, seed + 2, fmin=3, fmax=90))
    skin = spectral((n, n), 2.2, seed + 3, fmin=30)
    broad = spectral((n, n), 3.5, seed + 4, fmin=1, fmax=8)
    height = 0.00004 * skin - 0.00012 * craze * (1.0 - lift) - 0.00025 * lift \
        + 0.0001 * spectral((n, n), 2.0, seed + 5, fmin=60) * lift
    lum = 0.86 + 0.04 * skin - 0.14 * craze * (1.0 - lift) + 0.08 * lift + 0.05 * broad
    tint = tint_field(lift, (1.25, 0.95, 0.70))
    rough = 0.82 + 0.05 * skin + 0.25 * lift + 0.05 * craze
    alb, mean = albedo_map(lum, tint, rough)
    return alb, normal_map(height, (px, px)), mean


def timber(seed=606, w=1024, h=256, tile=(2.0, 0.5)):
    """Weathered deck timber, the grain running along u (along the plank):
    growth rings as long dark latewood lines, the soft earlywood worn down
    between them so the grain stands up, and drying checks along it."""
    px_u, px_v = tile[0] / w, tile[1] / h
    rng = np.random.default_rng(seed)
    shape = (h, w)
    # Ring lines across v, wandering slowly along u.
    wander = spectral(shape, 3.6, seed + 1, fmin=1, fmax=12, aniso=(1.0, 4.0))
    vv = np.mgrid[0:h, 0:w][0].astype(np.float64) * px_v
    rings_per_m = 110.0
    phase = (vv + 0.012 * wander) * rings_per_m
    latewood = smoothstep(0.62, 0.92, 0.5 + 0.5 * np.sin(2.0 * np.pi * phase))
    fibre = spectral(shape, 1.8, seed + 2, fmin=8, aniso=(1.0, 25.0))
    streak = spectral(shape, 2.8, seed + 3, fmin=1, fmax=40, aniso=(1.0, 12.0))
    checks = np.zeros(shape)
    for _ in range(30):
        v0 = rng.integers(0, h)
        u0 = rng.integers(0, w)
        length = rng.integers(w // 10, w // 2)
        us = (u0 + np.arange(length)) % w
        t = np.arange(length) / length
        depth = np.sin(np.pi * t) * rng.uniform(0.6, 1.0)
        drift = (rng.uniform(-3, 3) * t).astype(int)
        vs = (v0 + drift) % h
        checks[vs, us] = np.maximum(checks[vs, us], depth)
        checks[(vs + 1) % h, us] = np.maximum(checks[(vs + 1) % h, us], 0.5 * depth)
    # Weathering: the sun greys the wood in long streaks along the grain.
    grey = 0.35 + 0.65 * smoothstep(-0.6, 1.2, streak)
    height = 0.00055 * latewood + 0.0001 * fibre - 0.0015 * checks
    lum = 0.86 - 0.30 * latewood + 0.08 * fibre + 0.06 * streak - 0.5 * checks
    tint = tint_field(np.clip(0.5 * latewood - 1.0 * grey, -1, 1), (1.04, 0.94, 0.84), (0.80, 0.93, 1.15))
    rough = 0.92 + 0.05 * fibre * 0.5 + 0.08 * checks - 0.04 * latewood
    alb, mean = albedo_map(lum, tint, rough)
    return alb, normal_map(height, (px_v, px_u)), mean


KINDS = {
    "rust": rust,
    "concrete": concrete,
    "paint": paint,
    "galvanised": galvanised,
    "mill_scale": mill_scale,
    "timber": timber,
}


def main():
    names = sys.argv[1:] or list(KINDS)
    for kind in names:
        albedo, normal, mean = KINDS[kind]()
        write(kind, albedo, normal)
        print(f"{kind}: {normal.shape[1]}x{normal.shape[0]} linear mean {mean:.3f}")


if __name__ == "__main__":
    main()
