"""Extra inputs for the elite-technique probes (own code, numpy + Pillow only).

Run: python3 -I build_extra.py DL_DIR DATA_DIR
Reads the head arrays build_head.py wrote, and CC0 MakeHuman data in DL_DIR.
Writes, per LOD: tan.f32 (xyzw tangents for normal maps) and curv.f32 (mean
curvature in 1/cm from an umbrella operator on the position-welded mesh).
Writes hair/ and brows/ card meshes fitted to her morphed head through the
.mhclo bindings (format: three base-mesh vertex ids, three weights, and an
offset scaled by the x/y/z_scale reference distances), eyes.json (eye centres
and radii from the helper-l/r-eye groups), and textures: skin_lut.png
(pre-integrated diffusion, Penner 2011, d'Eon sum-of-Gaussians profile),
pores_n.png (tiling micro-normal), head_albedo.png (the head's UV region of
the CC0 old_caucasian_female skin, re-graded toward her sallow, bloodless tone).
"""
import json
import os
import sys

import numpy as np
from PIL import Image

DL, DATA = sys.argv[1], sys.argv[2]
SCALE = 0.1
rng = np.random.default_rng(1234)


# ------------------------------------------------------------------ per LOD
def load(lod, name, dt, w):
    return np.fromfile(os.path.join(DATA, lod, name), dt).reshape(-1, w)


def tangents(P, N, UV, T):
    t1 = np.zeros_like(P)
    t2 = np.zeros_like(P)
    a, b, c = T[:, 0], T[:, 1], T[:, 2]
    e1, e2 = P[b] - P[a], P[c] - P[a]
    d1, d2 = UV[b] - UV[a], UV[c] - UV[a]
    r = d1[:, 0] * d2[:, 1] - d2[:, 0] * d1[:, 1]
    r = np.where(np.abs(r) < 1e-12, 1e-12, r)
    sd = (e1 * d2[:, 1:2] - e2 * d1[:, 1:2]) / r[:, None]
    td = (e2 * d1[:, 0:1] - e1 * d2[:, 0:1]) / r[:, None]
    for k in range(3):
        np.add.at(t1, T[:, k], sd)
        np.add.at(t2, T[:, k], td)
    t = t1 - N * (N * t1).sum(1, keepdims=True)
    t /= np.maximum(np.linalg.norm(t, axis=1, keepdims=True), 1e-12)
    w = np.where((np.cross(N, t) * t2).sum(1) < 0.0, -1.0, 1.0)
    return np.concatenate([t, w[:, None]], 1)


def curvature(P, N, T):
    key = np.round(P / 1e-5).astype(np.int64)
    _, uid, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    inv = inv.reshape(-1)
    Pu = P[uid]
    Nu = np.zeros_like(Pu)
    np.add.at(Nu, inv, N)
    Nu /= np.maximum(np.linalg.norm(Nu, axis=1, keepdims=True), 1e-12)
    Tu = inv[T]
    e = np.concatenate([Tu[:, [0, 1]], Tu[:, [1, 2]], Tu[:, [2, 0]]])
    e = np.unique(np.sort(e, 1), axis=0)
    s = np.zeros_like(Pu)
    l2 = np.zeros(len(Pu))
    cnt = np.zeros(len(Pu))
    for i, j in ((0, 1), (1, 0)):
        d = Pu[e[:, j]] - Pu[e[:, i]]
        np.add.at(s, e[:, i], d)
        np.add.at(l2, e[:, i], (d * d).sum(1))
        np.add.at(cnt, e[:, i], 1)
    lap = s / np.maximum(cnt, 1)[:, None]
    k = -2.0 * (lap * Nu).sum(1) / np.maximum(l2 / np.maximum(cnt, 1), 1e-12)  # 1/m, convex +
    # one smoothing pass so the LUT lookup does not speckle
    acc = k.copy()
    c2 = np.ones(len(Pu))
    for i, j in ((0, 1), (1, 0)):
        np.add.at(acc, e[:, i], k[e[:, j]])
        np.add.at(c2, e[:, i], 1)
    k = acc / c2
    return (k / 100.0)[inv]  # 1/cm


for lod in ("lod0", "lod1"):
    P = load(lod, "pos.f32", "<f4", 3).astype(np.float64)
    N = load(lod, "nrm.f32", "<f4", 3).astype(np.float64)
    UV = load(lod, "uv.f32", "<f4", 2).astype(np.float64)
    T = load(lod, "idx.i32", "<i4", 3)
    tan = tangents(P, N, UV, T[:, [0, 2, 1]])
    tan.astype("<f4").tofile(os.path.join(DATA, lod, "tan.f32"))
    k = curvature(P, N, T)
    k.astype("<f4").tofile(os.path.join(DATA, lod, "curv.f32"))
    print(lod, "curv 1/cm p5/p50/p95", np.percentile(k, [5, 50, 95]).round(3))

# ------------------------------------------------------------------ morphed base mesh (same identity as build_head.py)
V, VT, F, FT, G = [], [], [], [], []
grp = None
for line in open(os.path.join(DL, "base.obj")):
    if line.startswith("v "):
        V.append([float(x) for x in line.split()[1:4]])
    elif line.startswith("vt "):
        VT.append([float(x) for x in line.split()[1:3]])
    elif line.startswith("g "):
        grp = line.split()[1]
    elif line.startswith("f "):
        toks = line.split()[1:]
        F.append([int(t.split("/")[0]) - 1 for t in toks])
        FT.append([int(t.split("/")[1]) - 1 for t in toks])
        G.append(grp)
V = np.array(V)
VT = np.array(VT)


def load_target(name):
    d = np.zeros_like(V)
    for line in open(os.path.join(DL, name)):
        if line[:1].isdigit():
            p = line.split()
            d[int(p[0])] += [float(p[1]), float(p[2]), float(p[3])]
    return d


IDENTITY = [
    ("cfo.target", 1.0), ("targets/universal-female-old-minmuscle-minweight.target", 1.0),
    ("targets/head-age-incr.target", 1.0), ("targets/l-cheek-volume-decr.target", 0.9),
    ("targets/r-cheek-volume-decr.target", 1.0), ("targets/l-cheek-bones-incr.target", 0.5),
    ("targets/r-cheek-bones-incr.target", 0.45), ("targets/l-eye-bag-incr.target", 0.8),
    ("targets/r-eye-bag-incr.target", 0.7), ("targets/l-eye-push1-in.target", 0.6),
    ("targets/r-eye-push1-in.target", 0.5), ("targets/mouth-angles-down.target", 0.6),
    ("targets/nose-hump-incr.target", 0.7), ("targets/mouth-laugh-lines-out.target", 0.8),
    ("targets/mouth-upperlip-volume-decr.target", 0.6), ("targets/mouth-lowerlip-volume-decr.target", 0.5),
    ("targets/neck-platysma.target", 0.5),
]
for name, w in IDENTITY:
    V += w * load_target(name)

eyes = {}
for side in ("l", "r"):
    idx = sorted({i for f, g in zip(F, G) if g == "helper-%s-eye" % side for i in f})
    c = V[idx].mean(0)
    rad = np.linalg.norm(V[idx] - c, axis=1).mean()
    eyes[side] = dict(centre=list(c * SCALE), radius=float(rad * SCALE))
json.dump(eyes, open(os.path.join(DATA, "eyes.json"), "w"), indent=1)
print("eyes", eyes)


def fit_clothes(stem, out):
    mh = os.path.join(DL, "mhassets", stem)
    lines = open(mh + ".mhclo").read().splitlines()
    sc = {}
    binds = []
    on = False
    for ln in lines:
        p = ln.split()
        if not p or p[0].startswith("#"):
            continue
        if p[0] in ("x_scale", "y_scale", "z_scale"):
            sc[p[0][0]] = (int(p[1]), int(p[2]), float(p[3]))
        elif p[0] == "verts":
            on = True
        elif on and len(p) == 9 and p[0].isdigit():
            binds.append([float(x) for x in p])
        elif on and p[0].isalpha():
            if binds:
                on = False
    B = np.array(binds)
    vid = B[:, 0:3].astype(int)
    w = B[:, 3:6]
    off = B[:, 6:9]
    s = np.array([abs(V[sc[a][0], k] - V[sc[a][1], k]) / sc[a][2] for k, a in enumerate("xyz")])
    Pc = (V[vid] * w[:, :, None]).sum(1) + off * s
    # the card mesh's own faces and uvs
    ov, ovt, of, oft = [], [], [], []
    for ln in open(mh + ".obj"):
        if ln.startswith("v "):
            ov.append(1)
        elif ln.startswith("vt "):
            ovt.append([float(x) for x in ln.split()[1:3]])
        elif ln.startswith("f "):
            t = ln.split()[1:]
            of.append([int(x.split("/")[0]) - 1 for x in t])
            oft.append([int(x.split("/")[1]) - 1 for x in t])
    assert len(ov) == len(Pc), (len(ov), len(Pc))
    ovt = np.array(ovt)
    key = {}
    src, uvs, tris = [], [], []
    for f, ft in zip(of, oft):
        ids = []
        for v, t in zip(f, ft):
            kk = (v, t)
            if kk not in key:
                key[kk] = len(src)
                src.append(v)
                uvs.append(ovt[t])
            ids.append(key[kk])
        for k in range(1, len(ids) - 1):
            tris.append([ids[0], ids[k + 1], ids[k]])
    src = np.array(src)
    UV = np.array(uvs)
    T = np.array(tris)
    P = Pc[src] * SCALE
    n = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])
    N = np.zeros_like(P)
    for k in range(3):
        np.add.at(N, T[:, k], n)
    N /= np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-12)
    N = -N  # Godot winding was flipped above
    # strand direction: the 3D direction of increasing texture v (root->tip)
    uvf = np.stack([UV[:, 0], 1.0 - UV[:, 1]], 1)
    tan = tangents(P, N, uvf, T[:, [0, 2, 1]])
    # tangent along u; the strand runs along v in MakeHuman hair textures: use bitangent
    bit = np.cross(N, tan[:, :3]) * tan[:, 3:4]
    d = os.path.join(DATA, out)
    os.makedirs(d, exist_ok=True)
    P.astype("<f4").tofile(os.path.join(d, "pos.f32"))
    N.astype("<f4").tofile(os.path.join(d, "nrm.f32"))
    uvf.astype("<f4").tofile(os.path.join(d, "uv.f32"))
    T.astype("<i4").tofile(os.path.join(d, "idx.i32"))
    np.concatenate([bit, np.ones((len(P), 1))], 1).astype("<f4").tofile(os.path.join(d, "tan.f32"))
    json.dump(dict(verts=len(P), tris=len(T)), open(os.path.join(d, "meta.json"), "w"))
    print(out, len(P), "verts", len(T), "tris", P.min(0).round(3), P.max(0).round(3))


fit_clothes("ponytail01", "hair")
fit_clothes("eyebrow010", "brows")

# ------------------------------------------------------------------ textures
TEX = os.path.join(DATA, "tex")
os.makedirs(TEX, exist_ok=True)

# Pre-integrated skin LUT: x = N.L*0.5+0.5, y = curvature (0..1 maps to 0..2 /cm).
# Profile: d'Eon & Luebke sum of Gaussians (variances mm^2, rgb weights), with the
# red tail cut back: old, thin, poorly perfused skin scatters less blood-red.
GV = [0.0064, 0.0484, 0.187, 0.567, 1.99, 7.41]
GW = np.array([[0.233, 0.455, 0.649], [0.100, 0.336, 0.344], [0.118, 0.198, 0.0],
               [0.113, 0.007, 0.007], [0.358, 0.004, 0.0], [0.078, 0.0, 0.0]])
GW[3:, 0] *= 0.55
GW /= GW.sum(0, keepdims=True)
W, H = 128, 64
lut = np.zeros((H, W, 3))
th = np.linspace(-np.pi, np.pi, 721)
for j in range(H):
    curv = max((j + 0.5) / H * 2.0, 0.02)  # 1/cm
    r_mm = 10.0 / curv
    dist = 2.0 * r_mm * np.abs(np.sin(th / 2.0))
    for i in range(W):
        ndl = (i + 0.5) / W * 2.0 - 1.0
        base = np.arccos(np.clip(ndl, -1, 1))
        lit = np.clip(np.cos(base + th), 0, None)
        for ch in range(3):
            prof = sum(GW[k, ch] * np.exp(-dist ** 2 / (2 * GV[k])) / (2 * np.pi * GV[k]) for k in range(6))
            lut[j, i, ch] = (lit * prof).sum() / prof.sum()
lut = np.clip(lut, 0, 1) ** (1 / 2.2)
Image.fromarray((lut * 255 + 0.5).astype(np.uint8)).save(os.path.join(TEX, "skin_lut.png"))

# Tiling pore + fine-line micro normal (512, own procedural): cellular pits.
S = 512
yy, xx = np.mgrid[0:S, 0:S].astype(np.float64)
h = np.zeros((S, S))
pts = rng.random((2600, 2)) * S
r = rng.uniform(1.0, 2.2, len(pts))
for (px, py), rr in zip(pts, r):
    x0, y0 = int(px), int(py)
    for dx in range(-4, 5):
        for dy in range(-4, 5):
            X, Y = (x0 + dx) % S, (y0 + dy) % S
            d2 = (x0 + dx - px) ** 2 + (y0 + dy - py) ** 2
            h[Y, X] -= np.exp(-d2 / (rr * rr))
# fine crossing lines (old skin's cross-hatching)
for ang in (0.6, -0.9, 2.1):
    u = np.cos(ang) * xx + np.sin(ang) * yy
    h += 0.18 * np.sin(u * 2 * np.pi / 9.0 + rng.random() * 6) ** 16
f = np.fft.fft2(h)
fx = np.fft.fftfreq(S)[None, :]
fy = np.fft.fftfreq(S)[:, None]
h = np.real(np.fft.ifft2(f * np.exp(-(fx ** 2 + fy ** 2) * 900.0)))
gx = (np.roll(h, -1, 1) - np.roll(h, 1, 1)) * 0.5
gy = (np.roll(h, -1, 0) - np.roll(h, 1, 0)) * 0.5
k = 2.2
n = np.stack([-gx * k, -gy * k, np.ones_like(h)], -1)
n /= np.linalg.norm(n, axis=-1, keepdims=True)
Image.fromarray(((n * 0.5 + 0.5) * 255 + 0.5).astype(np.uint8)).save(os.path.join(TEX, "pores_n.png"))

# Albedo: the CC0 old skin, regraded (less red, greyer, sallow), full atlas at 2048.
im = np.asarray(Image.open(os.path.join(DL, "mhassets", "old_lightskinned_female_diffuse.png")).convert("RGB")).astype(np.float64) / 255.0
lin = im ** 2.2
lum = (lin * [0.2126, 0.7152, 0.0722]).sum(-1, keepdims=True)
target = np.array([0.80, 0.74, 0.62])  # sallow, bloodless
grade = lum * target / (target * [0.2126, 0.7152, 0.0722]).sum()
out = lin * 0.35 + grade * 0.65
out = np.clip(out * 0.92, 0, 1) ** (1 / 2.2)
Image.fromarray((out * 255 + 0.5).astype(np.uint8)).save(os.path.join(TEX, "head_albedo.png"))
Image.fromarray((im * 255).astype(np.uint8)).save(os.path.join(TEX, "head_albedo_raw.png"))
for nm in ("ponytail01_diffuse.png", "eyebrow010.png"):
    Image.open(os.path.join(DL, "mhassets", nm)).save(os.path.join(TEX, nm))
# head UV region share of the atlas (for the memory budget)
uv1 = load("lod1", "uv.f32", "<f4", 2)
print("head uv bbox", uv1.min(0).round(3), uv1.max(0).round(3))
print("done")
