#!/usr/bin/env python3
"""Tiling cloth and skin maps for the probe, made from code (no files in).
python3 -I make_textures.py <out dir>
  cloth_n.png    512^2 normal map: hanging-cloth creases + a plain weave
  knit_n.png     256^2 normal map: stocking knit (fine vertical ribs)
  skin_a.png     1024^2 albedo in MakeHuman's UV space: pale old skin, mottling, age spots
  skin_n.png     512^2 tiling normal: fine creases and pores
"""
import sys

import numpy as np
from PIL import Image

OUT = sys.argv[1]
rng = np.random.default_rng(1950)


def tiling_noise(n, octaves, base, aniso=(1.0, 1.0), seed=0):
    r = np.random.default_rng(seed)
    acc = np.zeros((n, n))
    amp = 1.0
    for o in range(octaves):
        f = base * 2 ** o
        F = np.zeros((n, n), complex)
        k = np.fft.fftfreq(n) * n
        kx, ky = np.meshgrid(k, k)
        rad = np.sqrt((kx / aniso[0]) ** 2 + (ky / aniso[1]) ** 2)
        band = np.exp(-((rad - f) ** 2) / (2 * (0.35 * f + 0.5) ** 2))
        F = band * (r.normal(size=(n, n)) + 1j * r.normal(size=(n, n)))
        layer = np.real(np.fft.ifft2(F))
        layer /= layer.std() + 1e-9
        acc += amp * layer
        amp *= 0.5
    return acc / acc.std()


def to_normal(h, strength):
    gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5 * strength
    gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5 * strength
    n = np.stack([-gx, gy, np.ones_like(h)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return Image.fromarray(((n * 0.5 + 0.5) * 255).astype(np.uint8))


n = 512
# hanging cloth: long vertical creases (stretched noise) and a few soft folds
crease = tiling_noise(n, 3, 3, aniso=(1.0, 0.25), seed=3)
soft = tiling_noise(n, 2, 2, seed=4)
y, x = np.mgrid[0:n, 0:n]
weave = (np.sin(x * 2 * np.pi / 4) * np.sin(y * 2 * np.pi / 4)) * 0.5 + 0.5 * np.sign(np.sin(x * 2 * np.pi / 8)) * np.sin(y * 2 * np.pi / 8)
h = 2.2 * np.tanh(crease * 0.8) + 1.2 * soft + 0.25 * weave
to_normal(h, 1.2).save(OUT + "/cloth_n.png")
# knit: fine vertical ribs with a little irregularity
m = 256
yy, xx = np.mgrid[0:m, 0:m]
knit = np.abs(np.sin(xx * 2 * np.pi / 4)) + 0.2 * tiling_noise(m, 2, 8, seed=5)
to_normal(knit, 1.5).save(OUT + "/knit_n.png")
# skin albedo in UV space: pale, a little sallow, uneven, with age spots
N = 1024
base = np.array([0.80, 0.715, 0.655])
low = tiling_noise(N, 3, 4, seed=7)
mid = tiling_noise(N, 3, 24, seed=8)
alb = np.ones((N, N, 3)) * base
alb *= (1.0 + 0.035 * low[..., None] + 0.018 * mid[..., None])
# blotchy redness and a faint grey-blue under it (veins, thin skin)
blush = np.clip(tiling_noise(N, 2, 10, seed=9), 0, None)
alb[..., 0] += 0.025 * blush
alb[..., 2] += 0.012 * np.clip(tiling_noise(N, 3, 16, seed=10), 0, None)
# age spots (lentigines): small soft brown discs
spots = np.zeros((N, N))
for _ in range(900):
    cx, cy = rng.integers(0, N, 2)
    r = rng.uniform(1.2, 4.5)
    s = int(r * 3)
    ys, xs = np.mgrid[-s:s + 1, -s:s + 1]
    blob = np.exp(-(xs ** 2 + ys ** 2) / (2 * r * r)) * rng.uniform(0.3, 1.0)
    yi = (cy + ys) % N
    xi = (cx + xs) % N
    spots[yi, xi] = np.maximum(spots[yi, xi], blob)
alb = alb * (1 - 0.22 * spots[..., None]) + 0.22 * spots[..., None] * np.array([0.55, 0.40, 0.30])
Image.fromarray((np.clip(alb, 0, 1) ** (1 / 1.0) * 255).astype(np.uint8)).save(OUT + "/skin_a.png")
# skin detail normal: fine crêpe creases and pores
cre = tiling_noise(n, 3, 40, aniso=(1.0, 0.5), seed=11)
pore = tiling_noise(n, 1, 120, seed=12)
to_normal(0.6 * cre - 0.4 * np.clip(pore, 0, None), 0.6).save(OUT + "/skin_n.png")
print("ok")
# thin grey hair: combed strands with the scalp between them (alpha), 256 x 512
W_, H_ = 256, 512
hair = np.zeros((H_, W_, 4))
r2 = np.random.default_rng(77)
for _ in range(320):
    x0 = r2.uniform(0, W_)
    w = r2.uniform(0.7, 1.8)
    tone = r2.uniform(0.55, 0.95)          # grey to white
    a = r2.uniform(0.55, 1.0)
    wob = r2.uniform(0.5, 2.5)
    ph = r2.uniform(0, 6.28)
    ys = np.arange(H_)
    xc = (x0 + wob * np.sin(ys / H_ * 2 * np.pi * 2 + ph)) % W_
    xs = np.arange(W_)
    dx = np.abs(((xs[None, :] - xc[:, None]) + W_ / 2) % W_ - W_ / 2)
    m = np.clip(1.0 - dx / w, 0, 1)
    upd = m > hair[..., 3] * 0.0
    hair[..., :3] = np.where(m[..., None] > 0.01, np.maximum(hair[..., :3], tone * m[..., None]), hair[..., :3])
    hair[..., 3] = np.maximum(hair[..., 3], a * m)
Image.fromarray((np.clip(hair, 0, 1) * 255).astype(np.uint8), "RGBA").save(OUT + "/hair_a.png")
print("hair ok")
