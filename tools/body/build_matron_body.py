#!/usr/bin/env python3
"""Build the Matron: a real old woman from MakeHuman's CC0 scanned body, in
the site first-aid attendant's uniform gone foul, skinned to a skeleton whose
joints are matron_view.gd's pivots (the owner, 2026-10-20: redesigned from
scratch as a real ragged rough foul old woman, a body that isn't too thin but
old-woman fat, pale wrinkly aged skin, detailed hands with working fingers, a
dirty cloth uniform, working hair, eyes that track you).

Run with:  python3 -I tools/body/fetch_makehuman.py
           python3 -I tools/body/build_matron_body.py [download dir] [out dir]
           (defaults build/makehuman and godot/presentation/life/matron)

Inputs, all CC0 1.0 and pinned by SHA-256 in sources.json: MakeHuman's hm08
base mesh, macro and measure targets, its default skeleton and weights, the
face-age targets, and from its system asset pack the old woman's skin, the
long01 hair, eyebrow010, eyelashes02, the high-poly eyes and the base teeth.
No MakeHuman program code (AGPL) is used: the formats are read here.

Outputs (the game loads them at runtime, kept as they are by their .import
files): matron.glb (skinned, one mesh per part), matron_rig.json (bones,
contacts, hair chains, eyes, for matron_rig.gd), matron_stats.json, and the
textures: skin_a.jpg and skin_h.png (albedo and wrinkle height in MakeHuman's
UV layout), hair_a.png, eye_a.jpg, teeth_a.jpg, brow_a.png, lash_a.png,
cloth_n.png, knit_n.png.
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import human as h  # noqa: E402

DL = sys.argv[1] if len(sys.argv) > 1 else os.path.join("build", "makehuman")
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join("godot", "presentation", "life", "matron")
ASSETS = os.path.join(DL, "assets")
SOLE = 0.012   # her shoes' sole under the bare foot, metres

# Seventy-seven, heavy in the old way: a soft low belly, a heavy low bust,
# thick waist and upper arms, but not fat. Little muscle; 1.80 m standing.
PRESET = {"age": 0.9, "muscle": 0.3, "weight": 0.82, "height": 0.6,
          "breast_firmness": 0.05, "breast_cup": 0.62, "stand_m": 1.80}
FACE_AGE = [("face/head-age-incr.target", 1.0), ("face/l-cheek-volume-decr.target", 0.6),
            ("face/r-cheek-volume-decr.target", 0.6), ("face/l-eye-bag-incr.target", 0.8),
            ("face/r-eye-bag-incr.target", 0.8), ("face/l-eye-push1-in.target", 0.5),
            ("face/r-eye-push1-in.target", 0.5), ("face/mouth-angles-down.target", 0.7),
            ("face/mouth-laugh-lines-out.target", 0.8), ("face/mouth-upperlip-volume-decr.target", 0.6),
            ("face/mouth-lowerlip-volume-decr.target", 0.4), ("face/nose-hump-incr.target", 0.5),
            ("face/neck-platysma.target", 0.35)]
BODY_AGE = [("targets/stomach/stomach-pregnant-incr.target", 0.55),   # the soft low belly
            ("targets/torso/torso-vshape-decr.target", 0.45),          # narrow sloping shoulders
            ("targets/buttocks/buttocks-volume-decr.target", 0.3),
            ("targets/measure/measure-shoulder-dist-decr.target", 0.15),
            ("targets/neck/neck-trans-forward.target", 0.45),          # the head carried forward
            ("targets/measure/measure-upperarm-circ-incr.target", 0.35),
            ("targets/measure/measure-calf-circ-decr.target", 0.3),
            # big hands: what reaches for you is the first thing you see
            ("targets/armslegs/l-hand-scale-incr.target", 0.55),
            ("targets/armslegs/r-hand-scale-incr.target", 0.55)]

# Her hair: long01's sheet, thinned to lank strands, hung on chains of bones
# that matron_rig.gd swings (gravity, her motion, her head and back in the way).
HAIR_CHAINS = 10
HAIR_SEGMENTS = 4
HAIR_SEGMENT_M = 0.11


def linear(hex_srgb):
    """An sRGB hex color as glTF's linear base color."""
    c = [int(hex_srgb[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return [((x + 0.055) / 1.055) ** 2.4 if x > 0.04045 else x / 12.92 for x in c] + [1.0]


def srgb_to_linear(c):
    c = np.asarray(c, float)
    return np.where(c > 0.04045, ((c + 0.055) / 1.055) ** 2.4, c / 12.92)


# ------------------------------------------------------------- noise ----------
# Deterministic value noise over 3-D points (bind pose, metres): where the
# stains, the wear and the tears fall. Same input, same output.

def _hash3(ix, iy, iz, seed):
    v = (ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791) ^ (seed * 2654435761)
    v = (v ^ (v >> 13)) * 1274126177
    return ((v ^ (v >> 16)) & 0xFFFF).astype(float) / 65535.0


def _turn(seed):
    """A fixed rotation per seed, so the noise's lattice never lines up with
    her body's axes (axis-aligned lattices make square stains)."""
    q, r = np.linalg.qr(np.random.default_rng(1000 + seed).normal(size=(3, 3)))
    return q * np.sign(np.diag(r))


_GRADS = np.array([[1, 1, 0], [-1, 1, 0], [1, -1, 0], [-1, -1, 0], [1, 0, 1], [-1, 0, 1], [1, 0, -1], [-1, 0, -1],
                   [0, 1, 1], [0, -1, 1], [0, 1, -1], [0, -1, -1]], float)


def value_noise(P, freq, seed):
    """Gradient noise (Perlin's), 0..1: its iso-lines are round, where value
    noise's follow its lattice and make square stains."""
    Q = (np.asarray(P, float) @ _turn(seed).T) * freq
    i = np.floor(Q).astype(np.int64)
    f = Q - i
    u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0)
    acc = np.zeros(len(Q))
    for dx in (0, 1):
        wx = u[:, 0] if dx else 1.0 - u[:, 0]
        for dy in (0, 1):
            wy = u[:, 1] if dy else 1.0 - u[:, 1]
            for dz in (0, 1):
                wz = u[:, 2] if dz else 1.0 - u[:, 2]
                g = _GRADS[(_hash3(i[:, 0] + dx, i[:, 1] + dy, i[:, 2] + dz, seed) * 11.999).astype(int)]
                acc += wx * wy * wz * np.einsum("ij,ij->i", g, f - np.array([dx, dy, dz], float))
    return np.clip(acc * 0.75 + 0.5, 0.0, 1.0)


def fbm(P, freq, seed, octaves=4):
    total, amp, norm = np.zeros(len(P)), 0.5, 0.0
    for o in range(octaves):
        total += amp * value_noise(P, freq * (2.0 ** o), seed + 17 * o)
        norm += amp
        amp *= 0.5
    return total / norm


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


# ------------------------------------------------------------- proxies --------

def fit_proxy(V, stem):
    """A MakeHuman proxy (stem.obj + stem.mhclo) fitted to the morphed body V:
    each proxy vertex is three base-mesh vertices blended, plus an offset
    scaled by the reference distances (x/y/z_scale), so it follows any body.
    Returns positions, the obj's uvs, its faces (v, vt), and the binding."""
    path = os.path.join(ASSETS, stem)
    scale, ids, ws, offs = {}, [], [], []
    on = False
    for line in open(path + ".mhclo"):
        p = line.split()
        if not p or p[0].startswith("#"):
            continue
        if p[0] in ("x_scale", "y_scale", "z_scale"):
            scale[p[0][0]] = (int(p[1]), int(p[2]), float(p[3]))
        elif p[0] == "verts":
            on = True
        elif on and p[0].isdigit():
            if len(p) == 1:
                ids.append([int(p[0])] * 3)
                ws.append([1.0, 0.0, 0.0])
                offs.append([0.0, 0.0, 0.0])
            else:
                ids.append([int(x) for x in p[0:3]])
                ws.append([float(x) for x in p[3:6]])
                offs.append([float(x) for x in p[6:9]])
        elif on and ids:
            on = False   # the section after the vertices (a "material" line may come before them)
    ids, ws, offs = np.array(ids), np.array(ws), np.array(offs)
    s = np.array([abs(V[scale[a][0], k] - V[scale[a][1], k]) / scale[a][2] for k, a in enumerate("xyz")])
    P = (V[ids] * ws[:, :, None]).sum(1) + offs * s
    VT, faces = [], []
    for line in open(path + ".obj"):
        if line.startswith("vt "):
            VT.append([float(x) for x in line.split()[1:3]])
        elif line.startswith("f "):
            toks = line.split()[1:]
            faces.append(([int(t.split("/")[0]) - 1 for t in toks], [int(t.split("/")[1]) - 1 for t in toks], None))
    if len(P) == 0:
        raise SystemExit("fit_proxy: %s: no vertices bound" % stem)
    return P, np.array(VT), faces, (ids, ws)


def corners(P, VT, faces, keep=None):
    """Proxy faces -> (unique (v, vt) corners, their positions, uvs (v down,
    as glTF has it), triangles), wound as the body is."""
    tris = h.tri_faces(faces)
    if keep is not None:
        tris = [t for t, k in zip(tris, keep) if k]
    vi, ti, T = h.split_corners(P, VT, tris, np.ones(len(tris), bool))
    UV = VT[ti].copy()
    UV[:, 1] = 1.0 - UV[:, 1]
    return vi, P[vi], UV, T


def bound_weights(binding, Wfull):
    ids, ws = binding
    W = (Wfull[ids] * ws[:, :, None]).sum(1)
    s = W.sum(1, keepdims=True)
    return W / np.maximum(s, 1e-9)


# ------------------------------------------------------------- textures -------

def tiling_noise(n, octaves, base, aniso=(1.0, 1.0), seed=0):
    r = np.random.default_rng(seed)
    acc = np.zeros((n, n))
    amp = 1.0
    k = np.fft.fftfreq(n) * n
    kx, ky = np.meshgrid(k, k)
    for o in range(octaves):
        f = base * 2 ** o
        rad = np.sqrt((kx / aniso[0]) ** 2 + (ky / aniso[1]) ** 2)
        band_ = np.exp(-((rad - f) ** 2) / (2 * (0.35 * f + 0.5) ** 2))
        F = band_ * (r.normal(size=(n, n)) + 1j * r.normal(size=(n, n)))
        layer = np.real(np.fft.ifft2(F))
        layer /= layer.std() + 1e-9
        acc += amp * layer
        amp *= 0.5
    return acc / acc.std()


def height_to_normal(hgt, strength):
    gx = (np.roll(hgt, -1, 1) - np.roll(hgt, 1, 1)) * 0.5 * strength
    gy = (np.roll(hgt, -1, 0) - np.roll(hgt, 1, 0)) * 0.5 * strength
    n = np.stack([-gx, gy, np.ones_like(hgt)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return Image.fromarray(((n * 0.5 + 0.5) * 255).astype(np.uint8))


def uv_mask(size, UV, T, sel=None):
    """The texels a set of triangles covers, in an image of `size` (uv v down)."""
    img = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(img)
    tris = T if sel is None else T[sel]
    for t in tris:
        pts = [(float(UV[i, 0]) * size, float(UV[i, 1]) * size) for i in t]
        draw.polygon(pts, fill=255)
    return np.asarray(img, float) / 255.0


def blur(a, radius):
    img = Image.fromarray((np.clip(a, 0.0, 1.0) * 255).astype(np.uint8))
    return np.asarray(img.filter(ImageFilter.GaussianBlur(radius)), float) / 255.0


def cloth_maps(out):
    n = 512
    crease = tiling_noise(n, 3, 3, aniso=(1.0, 0.25), seed=3)
    soft = tiling_noise(n, 2, 2, seed=4)
    y, x = np.mgrid[0:n, 0:n]
    weave = (np.sin(x * 2 * np.pi / 4) * np.sin(y * 2 * np.pi / 4)) * 0.5 + \
        0.5 * np.sign(np.sin(x * 2 * np.pi / 8)) * np.sin(y * 2 * np.pi / 8)
    # worn through in places: the weave pills and frays
    pill = np.clip(tiling_noise(n, 2, 40, seed=6), 0, None) ** 2
    hgt = 2.2 * np.tanh(crease * 0.8) + 1.2 * soft + 0.25 * weave + 0.35 * pill
    height_to_normal(hgt, 1.2).save(os.path.join(out, "cloth_n.png"))
    m = 256
    yy, xx = np.mgrid[0:m, 0:m]
    knit = np.abs(np.sin(xx * 2 * np.pi / 4)) + 0.2 * tiling_noise(m, 2, 8, seed=5)
    height_to_normal(knit, 1.5).save(os.path.join(out, "knit_n.png"))


def skin_maps(out, UV, T, regions):
    """MakeHuman's CC0 old woman's skin, regraded pale, sallow and bloodless;
    the scalp under her thin hair repainted as skin; age spots, veins, grime on
    the hands and knees, yellowed nails with dirt under them; and a wrinkle
    height map from the skin's own creases (the game makes the normal map)."""
    src = Image.open(os.path.join(ASSETS, "skins/old_caucasian_female/old_lightskinned_female_diffuse.png")).convert("RGB")
    N = src.size[0]
    a = np.asarray(src, float) / 255.0
    lum = a @ np.array([0.299, 0.587, 0.114])
    # pale and sallow: most of the pink gone, a cold yellow-grey in its place
    tint = np.array([1.0, 0.965, 0.90])
    pale = (0.5 * a + 0.5 * lum[..., None] * tint) * 0.94
    rng = np.random.default_rng(77)

    def region(name, grow=0.0):
        m = uv_mask(N, UV, T, regions[name])
        return blur(m, grow) if grow > 0 else m
    # the scalp: pale skin with a few grey hairs' roots, no painted dark hair
    scalp = region("scalp", 6.0)
    forehead = region("forehead") > 0.5
    if not forehead.any():
        raise SystemExit("skin_maps: the forehead region covers no texels")
    skin_tone = pale[forehead].mean(0)
    mott = tiling_noise(N, 3, 12, seed=21)
    bald = skin_tone[None, None, :] * (1.0 + 0.03 * mott[..., None])
    bald = bald * 0.97 + 0.03 * np.array([0.6, 0.59, 0.57])   # a grey cast from the hair over it
    col = pale * (1.0 - scalp[..., None]) + bald * scalp[..., None]
    # age spots on the backs of the hands, the face and the forearms
    spotted = np.clip(region("hands", 3.0) + region("face", 3.0) + region("forearms", 3.0), 0, 1)
    spots = np.zeros((N, N))
    for _ in range(2600):
        cx, cy = rng.integers(0, N, 2)
        if spotted[cy, cx] < 0.5:
            continue
        r = rng.uniform(1.5, 6.0)
        s = int(r * 3)
        ys, xs = np.mgrid[-s:s + 1, -s:s + 1]
        blob = np.exp(-(xs ** 2 + ys ** 2) / (2 * r * r)) * rng.uniform(0.3, 1.0)
        yi, xi = np.clip(cy + ys, 0, N - 1), np.clip(cx + xs, 0, N - 1)
        spots[yi, xi] = np.maximum(spots[yi, xi], blob)
    col = col * (1 - 0.3 * spots[..., None]) + 0.3 * spots[..., None] * np.array([0.52, 0.38, 0.27])
    # veins under thin skin: the backs of the hands, the temples, the chest
    veined = np.clip(region("hands", 2.0) + region("temples", 4.0) + region("chest", 4.0), 0, 1)
    veins = np.zeros((N, N))
    for _ in range(420):
        x, y = rng.uniform(0, N, 2)
        if veined[int(y), int(x)] < 0.5:
            continue
        ang = rng.uniform(0, 2 * np.pi)
        for _ in range(int(rng.uniform(20, 70))):
            ang += rng.normal(0, 0.25)
            x, y = x + 2.0 * math.cos(ang), y + 2.0 * math.sin(ang)
            if not (0 <= x < N and 0 <= y < N):
                break
            veins[int(y), int(x)] = 1.0
    veins = blur(veins, 1.6) * veined
    col = col * (1.0 - 0.45 * veins[..., None] * np.array([0.35, 0.22, 0.05]))
    # grime: in the hands' creases and on the fingers, the knees, a smear at the chin
    dirty = np.clip(region("hands", 4.0) * 0.9 + region("knees", 8.0) * 0.6 + region("chin", 6.0) * 0.35, 0, 1)
    grime_n = np.clip(tiling_noise(N, 3, 30, seed=33) * 0.5 + 0.5, 0, 1)
    crease = np.clip((blur(lum, 2.0) - lum) * 8.0, 0, 1)   # the darker lines of the skin's own creases
    grime = dirty * np.clip(0.25 * grime_n + 0.6 * crease, 0, 1)
    col = col * (1.0 - grime[..., None] * 0.4) + grime[..., None] * 0.4 * np.array([0.32, 0.27, 0.21])
    # nails: yellowed horn, a black rim of dirt at the tips
    nails = region("nails", 1.0)
    tips = region("nail_tips", 1.0)
    col = col * (1.0 - nails[..., None]) + nails[..., None] * (col * np.array([0.92, 0.84, 0.62]))
    col = col * (1.0 - tips[..., None] * 0.8) + tips[..., None] * 0.8 * np.array([0.12, 0.1, 0.08])
    Image.fromarray((np.clip(col, 0, 1) * 255).astype(np.uint8)).save(os.path.join(out, "skin_a.jpg"), quality=92)
    # wrinkles: the skin texture's own creases as height (dark = deep), with
    # the scalp and the painted hair smoothed away, and a crepe over it all
    detail = blur(lum, 1.2) - blur(lum, 9.0)
    detail = detail * (1.0 - scalp)
    crepe = tiling_noise(N, 2, 160, aniso=(1.0, 0.6), seed=41) * 0.015
    hgt = np.clip(0.5 + 1.6 * detail + crepe, 0, 1)
    Image.fromarray((hgt * 255).astype(np.uint8), "L").save(os.path.join(out, "skin_h.png"))


HAIR_CLUMPS = 300
STRANDS_PER_CLUMP = (4, 7)
HAIR_LENGTH_M = (0.26, 0.55)
HAIR_STEP_M = 0.04


def drape(root, SV, SN, BV, BN, length, rng, nape):
    """A clump's path: from its root over the skull, combed back toward her
    nape a hair's breadth off the scalp (a few side locks just fall), until
    the scalp turns under or ends; then hanging, off her neck, shoulders and
    back."""
    pts = [root.copy()]
    pos = root.copy()
    hanging = False
    lay = rng.uniform(0.002, 0.006)    # hair over hair: how far off the scalp this clump lies
    i0 = int(np.argmin(np.sum((SV - root) ** 2, 1)))
    side_lock = abs(SN[i0][0]) > 0.6 and rng.random() < 0.4   # from over an ear: a lock that falls by her face
    drift = rng.normal(0.0, 0.15, 3)
    drift[1] = 0.0
    travelled = 0.0
    while travelled < length:
        if not hanging:
            i = int(np.argmin(np.sum((SV - pos) ** 2, 1)))
            n = SN[i]
            comb = np.array([0.0, -1.0, -0.2]) if side_lock else nape - pos
            comb = comb / np.linalg.norm(comb)
            d = comb - n * (comb @ n)
            if np.linalg.norm(d) < 1e-3:
                d = np.array([0.0, 0.0, -1.0]) - n * (-n[2])
            if n[1] < 0.05 and np.linalg.norm(d) < 0.5:
                hanging = True   # where the scalp turns under, it falls
            else:
                pos = pos + d / np.linalg.norm(d) * HAIR_STEP_M
                i = int(np.argmin(np.sum((SV - pos) ** 2, 1)))
                if np.linalg.norm(SV[i] - pos) > 0.03:
                    hanging = True
                else:
                    rel = pos - SV[i]
                    pos = SV[i] + rel - SN[i] * (rel @ SN[i]) + SN[i] * lay
        if hanging:
            step = np.array([0.0, -1.0, 0.0]) + 0.08 * drift
            pos = pos + step / np.linalg.norm(step) * HAIR_STEP_M
            d2 = np.sum((BV - pos) ** 2, 1)
            j = int(np.argmin(d2))
            gap = (pos - BV[j]) @ BN[j]
            if gap < 0.008 and d2[j] < 0.05 ** 2:
                pos = pos + BN[j] * (0.008 - gap)
        pts.append(pos.copy())
        travelled += HAIR_STEP_M
    return np.array(pts)


def grow_hair(V, N, T_body, in_hair, is_body, Wfull, col, head_c, byname):
    """Her hair as ribbons of a few hairs each, in clumps that cling together
    toward their ends (greasy), thin over the crown where it has fallen out,
    grey to yellowed white, darker and greasier at the roots."""
    rng = np.random.default_rng(1951)
    St = T_body[in_hair[T_body].all(1)]
    A, B, C = V[St[:, 0]], V[St[:, 1]], V[St[:, 2]]
    area = 0.5 * np.linalg.norm(np.cross(B - A, C - A), axis=1)
    prob = area / area.sum()
    sv = np.unique(St.ravel())
    SV, SN = V[sv], N[sv]
    crown_y = float(SV[:, 1].max())
    near = sum(Wfull[:, col[b]] for b in ("neck_lo", "neck_hi", "chest", "spine_mid", "hump", "sh_l", "sh_r"))
    shoulders_y = float(byname["sh_l"]["pos"][1])
    under = np.where(is_body & (near > 0.3) & (V[:, 1] > shoulders_y - 0.35) & ~in_hair)[0]
    BV, BN = V[under], N[under]
    nape = head_c + np.array([0.0, -0.14, -0.13])
    strands = []
    for _ in range(HAIR_CLUMPS):
        t = int(rng.choice(len(St), p=prob))
        r1, r2 = rng.random(2)
        if r1 + r2 > 1.0:
            r1, r2 = 1.0 - r1, 1.0 - r2
        root = A[t] + (B[t] - A[t]) * r1 + (C[t] - A[t]) * r2
        if root[1] > crown_y - 0.035 and rng.random() < 0.3:
            continue
        guide = drape(root, SV, SN, BV, BN, rng.uniform(*HAIR_LENGTH_M), rng, nape)
        tone = rng.uniform(0.0, 1.0)
        for _k in range(int(rng.integers(STRANDS_PER_CLUMP[0], STRANDS_PER_CLUMP[1] + 1))):
            strands.append((guide, rng.normal(0.0, 0.0045, 3), rng.uniform(0.75, 1.05), tone + rng.normal(0.0, 0.12),
                            rng.uniform(0.0, 0.006), rng.uniform(0.0, 2.0 * math.pi), rng.random() < 0.08))
    P, T, UV, Cl = [], [], [], []
    base = 0
    for guide, off, frac, tone, wave, phase, stray in strands:
        m = max(3, min(len(guide), int(round(len(guide) * frac))))
        v = np.linspace(0.0, 1.0, m)
        # clumped, the tips drawn together; a slow wave in each; now and then a
        # stray hair standing off the rest
        spread = np.linspace(1.0, 0.3, m) if not stray else np.linspace(1.0, 2.0, m)
        g = guide[:m].copy()
        out = ribbon_normals(g, None, head_c)
        # the clump's spread and the wave lie along her head, never off it
        lateral = off[None, :] * spread[:, None]
        lateral[:, 0] += wave * np.sin(v * 9.0 + phase) * v
        lateral[:, 2] += wave * np.cos(v * 7.0 + phase) * v
        g = g + lateral - out * np.sum(lateral * out, 1, keepdims=True)
        tang = np.gradient(g, axis=0)
        tang /= np.maximum(np.linalg.norm(tang, axis=1, keepdims=True), 1e-9)
        side = np.cross(tang, out)
        side /= np.maximum(np.linalg.norm(side, axis=1, keepdims=True), 1e-9)
        w = (0.0012 + 0.004 * np.clip(1.0 - v * 5.0, 0.0, 1.0) + 0.0018 * (1.0 - v))[:, None]
        P += [g - side * w / 2.0, g + side * w / 2.0]
        UV += [np.stack([np.zeros(m), v], 1), np.stack([np.ones(m), v], 1)]
        tt = float(np.clip(tone, 0.0, 1.0))
        c = np.array([0.5, 0.49, 0.47]) + (np.array([0.86, 0.83, 0.76]) - np.array([0.5, 0.49, 0.47])) * tt
        along = (0.8 + 0.2 * v)[:, None]
        cc = srgb_to_linear(np.clip(c[None, :] * along, 0.0, 1.0))
        Cl += [np.concatenate([cc, np.ones((m, 1))], 1)] * 2
        for k in range(m - 1):
            a_, b_ = base + k, base + m + k
            T += [[a_, b_, a_ + 1], [b_, b_ + 1, a_ + 1]]
        base += 2 * m
    return np.concatenate(P), np.array(T), np.concatenate(UV), np.concatenate(Cl), len(strands)


def ribbon_normals(P, T, head_c):
    """Hair lit as a volume round her head, not as flat ribbons: each point's
    normal away from her head's axis (from its center, over the skull)."""
    out = P - np.stack([np.full(len(P), head_c[0]), P[:, 1], np.full(len(P), head_c[2])], 1)
    top = P[:, 1] > head_c[1]
    out[top] = (P - head_c)[top]
    return out / np.maximum(np.linalg.norm(out, axis=1, keepdims=True), 1e-9)


def hair_map(out):
    """A lock's cross-section: opaque in the middle, soft at its edges (the
    color is each strand's own, in its vertex colors)."""
    n = 64
    u = (np.arange(n) + 0.5) / n
    alpha = 1.0 - smoothstep(0.22, 0.5, np.abs(u - 0.5))
    a = np.tile(alpha[None, :], (n, 1))
    Image.fromarray((np.concatenate([np.ones((n, n, 3)), a[..., None]], -1) * 255).astype(np.uint8), "RGBA").save(
        os.path.join(out, "hair_a.png"))


def _unused_long_hair_map(out):
    """long01's strands turned grey and yellowed, and thinned: clumps of lank
    strands with the scalp between them, cut off at uneven lengths."""
    src = Image.open(os.path.join(ASSETS, "hair/long01/long01_diffuse.png")).convert("RGBA").resize((1024, 1024), Image.LANCZOS)
    a = np.asarray(src, float) / 255.0
    lum = a[..., :3] @ np.array([0.299, 0.587, 0.114])
    l = np.clip((lum - lum[a[..., 3] > 0.5].min()) / (np.percentile(lum[a[..., 3] > 0.5], 99) + 1e-6), 0, 1)
    dark = np.array([0.2, 0.19, 0.18])
    light = np.array([0.66, 0.64, 0.59])   # grey gone yellow at the ends
    rgb = dark + (light - dark) * (l[..., None] ** 0.8)
    n = 1024
    rng = np.random.default_rng(9)
    # clumps along u: a lank strand bundle every few texels, gaps between
    u = np.arange(n)
    clump = np.zeros(n)
    tip = np.ones(n)
    x = 0
    while x < n:
        w = int(rng.integers(3, 9))
        gap = int(rng.integers(3, 15))
        clump[x:x + w] = rng.uniform(0.75, 1.0)
        tip[x:x + w] = rng.uniform(0.45, 1.0)   # how far down this clump reaches (1 = full length)
        x += w + gap
    clump = np.asarray(Image.fromarray((clump[None, :].repeat(4, 0) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.2)), float)[0] / 255.0
    v = np.arange(n)[:, None] / n   # 0 at the top of the image (roots) .. 1 (tips)
    reach = (v < tip[None, :]).astype(float)
    alpha = a[..., 3] * clump[None, :] * reach
    alpha *= 0.85 + 0.15 * (np.asarray(tiling_noise(n, 2, 60, aniso=(1.0, 0.1), seed=12)) > 0)
    out_a = np.concatenate([rgb, alpha[..., None]], -1)
    Image.fromarray((np.clip(out_a, 0, 1) * 255).astype(np.uint8), "RGBA").save(os.path.join(out, "hair_a.png"))


def eye_map(out):
    """The CC0 grey eye, paled to a washed-out ice grey, the white gone
    yellow and threaded with red."""
    a = np.asarray(Image.open(os.path.join(ASSETS, "eyes/materials/grey_eye.png")).convert("RGB").resize((512, 512)), float) / 255.0
    lum = a @ np.array([0.299, 0.587, 0.114])
    sat = a - lum[..., None]
    a = lum[..., None] + 0.45 * sat
    sclera = (lum > 0.62).astype(float)
    sclera = blur(sclera, 2.0)
    yellow = np.array([0.93, 0.86, 0.68])
    a = a * (1.0 - 0.35 * sclera[..., None]) + 0.35 * sclera[..., None] * yellow * lum[..., None] * 1.1
    rng = np.random.default_rng(5)
    red = np.zeros((512, 512))
    for _ in range(90):
        x, y = rng.uniform(0, 512, 2)
        if sclera[int(y), int(x)] < 0.6:
            continue
        ang = rng.uniform(0, 2 * np.pi)
        for _ in range(int(rng.uniform(8, 30))):
            ang += rng.normal(0, 0.4)
            x, y = x + 1.4 * math.cos(ang), y + 1.4 * math.sin(ang)
            if not (0 <= x < 512 and 0 <= y < 512):
                break
            red[int(y), int(x)] = 1.0
    red = blur(red, 0.7) * sclera
    a = a * (1.0 - 0.6 * red[..., None]) + 0.6 * red[..., None] * np.array([0.62, 0.12, 0.10])
    Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8)).save(os.path.join(out, "eye_a.jpg"), quality=92)


def teeth_map(out):
    """The CC0 teeth gone yellow-brown, the gums grey-pink, some teeth rotten."""
    src = Image.open(os.path.join(ASSETS, "teeth/teeth_base/teeth.png")).convert("RGBA").resize((512, 512), Image.LANCZOS)
    a = np.asarray(src, float) / 255.0
    rgb = a[..., :3]
    lum = rgb @ np.array([0.299, 0.587, 0.114])
    gum = (rgb[..., 0] > rgb[..., 1] * 1.4).astype(float)
    tooth = rgb * np.array([0.92, 0.82, 0.58]) * 0.95
    gums = lum[..., None] * np.array([0.85, 0.62, 0.62])
    out_rgb = tooth * (1 - gum[..., None]) + gums * gum[..., None]
    rot = (np.asarray(tiling_noise(512, 2, 6, seed=8)) > 1.0).astype(float)
    rot = blur(rot, 3.0) * (1 - gum)
    out_rgb = out_rgb * (1.0 - 0.75 * rot[..., None]) + 0.75 * rot[..., None] * np.array([0.22, 0.16, 0.09])
    Image.fromarray((np.clip(out_rgb, 0, 1) * 255).astype(np.uint8)).save(os.path.join(out, "teeth_a.jpg"), quality=90)


def strands_map(src_rel, out_name, tone):
    """Brows and lashes: their CC0 alpha, recolored and thinned."""
    a = np.asarray(Image.open(os.path.join(ASSETS, src_rel)).convert("RGBA").resize((256, 256), Image.LANCZOS), float) / 255.0
    rng = np.random.default_rng(len(out_name))
    keep = (rng.random(256) > 0.35).astype(float)
    alpha = a[..., 3] * blur(keep[None, :].repeat(256, 0), 0.8)
    rgb = np.ones((256, 256, 3)) * np.array(tone)
    Image.fromarray((np.clip(np.concatenate([rgb, alpha[..., None]], -1), 0, 1) * 255).astype(np.uint8), "RGBA").save(
        os.path.join(OUT, out_name))


# ------------------------------------------------------------- build ----------

def build():
    os.makedirs(OUT, exist_ok=True)
    h.set_download_dir(DL)
    V0, VT, faces = h.load_obj(os.path.join(DL, "3dobjs/base.obj"))
    skel = json.load(open(os.path.join(DL, "rigs/default.mhskel")))
    wfile = json.load(open(os.path.join(DL, "rigs/default_weights.mhw")))["weights"]
    V = h.morph(V0, PRESET)
    for rel, k in FACE_AGE + BODY_AGE:
        idx, d = h.target(rel)
        V[idx] += k * d
    tris = h.tri_faces(faces)
    body_tris = [t for t in tris if t[2] == "body"]
    body_v = np.unique(np.array([t[0] for t in body_tris]).ravel())
    is_body = np.zeros(len(V), bool)
    is_body[body_v] = True
    # decimetres -> metres, soles on y = 0, scaled to her standing height, on her shoes' soles
    V = V * 0.1
    V[:, 1] -= V[body_v, 1].min()
    natural = float(V[body_v, 1].max())
    k = PRESET["stand_m"] / natural
    V *= k
    V[:, 1] += SOLE
    J = {jn: V[vs].mean(0) for jn, vs in skel["joints"].items()}
    T_body = np.array([t[0] for t in body_tris])
    # Arthritic knuckles: the joints of every finger swollen a little.
    N0 = h.vertex_normals(V, T_body)
    for mh in (".L", ".R"):
        for f in ("finger1", "finger2", "finger3", "finger4", "finger5"):
            for seg in ((2, 3) if f == "finger1" else (1, 2, 3)):
                c = J["%s-%d%s____head" % (f, seg, mh)]
                ax = J["%s-%d%s____tail" % (f, seg, mh)] - c
                ax /= np.linalg.norm(ax)
                d = V[body_v] - c
                along = d @ ax
                radial = d - along[:, None] * ax
                rl = np.linalg.norm(radial, axis=1)
                amp = 0.0016 if seg == 2 else 0.0011
                w = amp * np.exp(-(along ** 2) / (2 * 0.004 ** 2)) * (rl < 0.013)
                V[body_v] += radial / np.maximum(rl, 1e-6)[:, None] * w[:, None]
    N = h.vertex_normals(V, T_body)
    E = h.neighbours(len(V), T_body)
    bones, byname = h.build_rig(V, J, skel)

    # Her eyes on their own bones, and her hair's chains, all under the head.
    def add_bone(name, parent, pos, mode, **kw):
        P = byname[parent]
        b = dict(name=name, parent=parent, pos=np.array(pos, float), F=np.eye(3), mode=mode, pivot=None, **kw)
        b["t"] = P["F"].T @ (b["pos"] - P["pos"])
        b["L"] = P["F"].T @ b["F"]
        bones.append(b)
        byname[name] = b
    eye_fit, eye_VT, eye_faces, eye_bind = fit_proxy(V, "eyes/high-poly/high-poly")
    eye_centre = {}
    for s in "lr":
        side = eye_fit[:, 0] < 0.0 if s == "l" else eye_fit[:, 0] > 0.0
        eye_centre[s] = eye_fit[side].mean(0)
        add_bone("eye_" + s, "head", eye_centre[s], "eye")
    head_c = byname["head"]["pos"] + np.array([0.0, 0.085, 0.01])
    root_y = float(byname["head"]["pos"][1]) + 0.025
    chain_az = np.linspace(0.42 * math.pi, 1.58 * math.pi, HAIR_CHAINS)   # 0 = her front, pi = her back
    chains = []
    for c, az in enumerate(chain_az):
        root = np.array([head_c[0] + 0.088 * math.sin(az), root_y, head_c[2] + 0.088 * math.cos(az)])
        names_c = []
        parent = "head"
        for seg in range(HAIR_SEGMENTS):
            name = "hair%d_%d" % (c, seg)
            add_bone(name, parent, root - np.array([0.0, HAIR_SEGMENT_M * seg, 0.0]), "hair")
            names_c.append(name)
            parent = name
        chains.append({"bones": names_c, "azimuth": float(az), "root": [float(x) for x in root],
                       "segment_m": HAIR_SEGMENT_M})

    Wfull, names = h.skin_weights(V, len(V), bones, byname, skel, wfile)
    col = {n: i for i, n in enumerate(names)}

    def wsum(*bs):
        return sum(Wfull[:, col[b]] for b in bs)

    # ---- measurements (bind pose, before clothes) ----
    stats = {"params": PRESET, "years": h.years(PRESET["age"]), "natural_height_m": natural, "scale_to_stand": k}
    vol = h.volume(V, T_body)
    stats["mass_kg"] = vol * 1010.0
    stats["bmi"] = stats["mass_kg"] / PRESET["stand_m"] ** 2

    def tris_of(sel):
        m = np.zeros(len(V), bool)
        m[sel] = True
        return T_body[m[T_body].all(1)]

    def cut(Tsel, origin, normal, radius=0.25):
        n = normal / np.linalg.norm(normal)
        d = (V - origin) @ n
        e = np.concatenate([Tsel[:, [0, 1]], Tsel[:, [1, 2]], Tsel[:, [2, 0]]])
        e = e[d[e[:, 0]] * d[e[:, 1]] < 0]
        if len(e) < 6:
            return 0.0
        a, b = e[:, 0], e[:, 1]
        P = V[a] + (V[b] - V[a]) * (d[a] / (d[a] - d[b]))[:, None]
        P = P[np.linalg.norm(P - origin, axis=1) < radius]
        if len(P) < 6:
            return 0.0
        u = np.cross(n, [1.0, 0.0, 0.0])
        if np.linalg.norm(u) < 0.1:
            u = np.cross(n, [0.0, 0.0, 1.0])
        u /= np.linalg.norm(u)
        w = np.cross(n, u)
        return h.perimeter(h.hull2(np.stack([(P - origin) @ u, (P - origin) @ w], 1)))
    up = np.array([0.0, 1.0, 0.0])
    trunk_sel = np.where(wsum("body", "spine_lo", "spine_mid", "chest", "hump", "hp_l", "hp_r") > 0.5)[0]
    trunk_sel = trunk_sel[is_body[trunk_sel]]
    Ttr = tris_of(trunk_sel)
    cz = float(J["spine03____head"][2])
    wy = np.linspace(J["spine03____head"][1], J["spine01____head"][1], 30)
    g = [cut(Ttr, np.array([0, y, cz]), up, 0.3) for y in wy]
    gi = int(np.argmin([x if x > 0 else 9 for x in g]))
    stats["waist_cm"] = 100 * g[gi]
    stats["waist_y"] = float(wy[gi])
    g = [cut(Ttr, np.array([0, y, cz]), up, 0.3) for y in np.linspace(J["spine02____head"][1], J["spine01____head"][1] + 0.06, 30)]
    stats["bust_cm"] = 100 * max(g)
    yh = float(J["upperleg01.L____head"][1])
    g = [cut(Ttr, np.array([0, y, cz]), up, 0.35) for y in np.linspace(yh - 0.12, yh + 0.04, 30)]
    stats["hip_cm"] = 100 * max(g)
    a, b = byname["sh_r"]["pos"], byname["el_r"]["pos"]
    sel = np.where(wsum("sh_r", "sh_r_x", "el_r") > 0.5)[0]
    stats["upper_arm_cm"] = 100 * cut(tris_of(sel[is_body[sel]]), 0.5 * (a + b), b - a, 0.1)
    a, b = byname["kn_r"]["pos"], byname["an_r"]["pos"]
    sel = np.where(wsum("kn_r", "kn_r_x", "hp_r_x", "an_r") > 0.5)[0]
    stats["calf_cm"] = 100 * max(cut(tris_of(sel[is_body[sel]]), a + (b - a) * t, b - a, 0.1) for t in np.linspace(0.12, 0.5, 16))
    stats["hand_len_m"] = float(np.linalg.norm(J["finger3-3.R____tail"] - J["wrist.R____head"]))
    stats["eye_y"] = float(J["eye.L____head"][1])
    parts = []
    materials = {}

    def mat(nm, color, rough, metal=0.0, double=False):
        materials[nm] = {"color": color, "rough": rough, "metal": metal, "double": double}

    def flipv(uv):
        uv = np.array(uv, float)
        uv[:, 1] = 1.0 - uv[:, 1]
        return uv

    # The cloth's dirt, by where a point is on her (bind pose): gone dingy all
    # over, soot and mud up from the hem, old blood and grease in blotches down
    # the front, sweat at the armpits, her hands' smears down her thighs.
    y_waist = stats["waist_y"]
    knee_y = float(byname["kn_l"]["pos"][1])
    hem_y = knee_y - 0.075
    pits = [J["upperarm01.L____head"] + np.array([0, -0.06, 0]), J["upperarm01.R____head"] + np.array([0, -0.06, 0])]
    front_z = float(J["spine02____head"][2])

    def stain(c, mask, color, edge_dark=0.75):
        """A dried stain: its color, and a darker tide line where it dried."""
        tide = np.clip(mask * (1.0 - mask) * 4.0, 0.0, 1.0)
        c = c * (1 - mask[:, None]) + mask[:, None] * srgb_to_linear(color)
        return c * (1.0 - (1.0 - edge_dark) * tide[:, None])

    def cloth_color(P, base_hex, hem=True):
        P = np.asarray(P, float)
        base = srgb_to_linear(np.array([int(base_hex[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]))
        c = np.tile(base, (len(P), 1))
        # gone dingy unevenly: greyer in the folds of wear, yellower where handled
        c *= (0.86 + 0.16 * fbm(P, 4.0, 1))[:, None]
        if hem:
            soot = smoothstep(hem_y + 0.3, hem_y - 0.02, P[:, 1]) * (0.5 + 0.5 * fbm(P, 20.0, 2))
            c = c * (1 - soot[:, None]) + soot[:, None] * srgb_to_linear([0.24, 0.22, 0.2])
        front = smoothstep(front_z - 0.02, front_z + 0.06, P[:, 2])
        c = stain(c, smoothstep(0.6, 0.72, fbm(P, 3.5, 4)) * 0.45, [0.66, 0.6, 0.47], 0.9)                     # old tea-colored fades
        c = stain(c, smoothstep(0.68, 0.74, fbm(P, 9.0, 6)) * 0.5, [0.46, 0.43, 0.39], 0.85)                   # grime
        c = stain(c, smoothstep(0.73, 0.77, fbm(P, 22.0, 3)) * (0.4 + 0.6 * front), [0.3, 0.13, 0.09], 0.6)   # old blood, spattered
        sweat = np.zeros(len(P))
        for q in pits:
            sweat = np.maximum(sweat, np.exp(-np.sum((P - q) ** 2, 1) / (2 * 0.05 ** 2)))
        c = stain(c, 0.55 * sweat, [0.62, 0.56, 0.36], 0.85)
        smear_zone = smoothstep(hem_y + 0.1, hem_y + 0.25, P[:, 1]) * smoothstep(y_waist - 0.02, y_waist - 0.18, P[:, 1])
        streak = smoothstep(0.64, 0.74, fbm(P * np.array([1.0, 0.16, 1.0]), 24.0, 5)) * smear_zone
        c = stain(c, 0.75 * streak, [0.26, 0.22, 0.2], 0.8)                                                       # her hands wiped down her
        return np.concatenate([c, np.ones((len(P), 1))], 1)

    keep = np.ones(len(body_tris), bool)
    # ---- the tunic: an offset shell of the trunk and upper arms ----
    arm_up = wsum("sh_l", "sh_l_x", "sh_r", "sh_r_x")
    arm_low = wsum("el_l", "el_l_x", "el_r", "el_r_x", "wr_l", "wr_r") + \
        sum(Wfull[:, col[n]] for n in names if n[0] in "ft" and n[1].isdigit())
    neckw = wsum("neck_lo", "neck_hi", "head", "jaw")
    u_arm = np.zeros(len(V))
    for s in "lr":
        a, b = byname["sh_" + s]["pos"], byname["el_" + s]["pos"]
        d = b - a
        u = ((V - a) @ d) / (d @ d)
        sel = (wsum("sh_" + s, "sh_%s_x" % s) > 0.3) & (np.sign(V[:, 0]) == (-1 if s == "l" else 1))
        u_arm[sel] = u[sel]
    # the sleeves end ragged, and her right one is torn half up the arm
    sleeve_u = 0.42 + 0.09 * (fbm(V, 26.0, 11) - 0.5)
    r_tear = (V[:, 0] > 0) & (np.abs(V[:, 2] - (J["upperarm01.L____head"][2] + 0.02)) < 0.022)
    sleeve_u = np.where(r_tear, 0.2, sleeve_u)
    y_jug = float(J["neck01____head"][1])
    in_top = (V[:, 1] > y_waist - 0.035) & (neckw < 0.22) & (arm_low < 0.3) & (u_arm < sleeve_u)
    front = V[:, 2] > J["neck01____head"][2] + 0.02
    in_top &= ~(front & (V[:, 1] > y_jug - 0.035) & (np.abs(V[:, 0]) < 0.07))
    in_top &= is_body
    top_tris = np.array([all(in_top[t[0]]) for t in body_tris])
    Tt = T_body[top_tris]
    top_v = np.unique(Tt.ravel())
    S = h.shell(V, N, E, Tt, top_v, 0.010, smooth_iters=24, outward_iters=60, min_gap=0.004)
    flare = np.clip((u_arm - 0.12) / (0.42 - 0.12), 0.0, 1.0)
    S += N * (0.009 * flare)[:, None]
    bust_y = float(J["breast.L____head"][1])
    midz = float(J["spine02____head"][2])
    tv = top_v[(S[top_v, 1] < bust_y) & (S[top_v, 1] > y_waist + 0.01)]
    ring = top_v[np.abs(S[top_v, 1] - bust_y) < 0.02]
    for v in tv:
        col_ = ring[np.abs(S[ring, 0] - S[v, 0]) < 0.02]
        if len(col_) == 0:
            continue
        gather = np.clip((S[v, 1] - y_waist) / 0.09, 0.0, 1.0)
        gather = gather * gather * (3 - 2 * gather)
        if S[v, 2] > midz:
            zt = S[col_, 2].max() - (bust_y - S[v, 1]) * 0.22
            if zt > S[v, 2]:
                S[v, 2] += (zt - S[v, 2]) * gather
    sc_y = float(J["spine01____head"][1])
    tv = top_v[(S[top_v, 1] < sc_y) & (S[top_v, 1] > y_waist + 0.01)]
    ring = top_v[np.abs(S[top_v, 1] - sc_y) < 0.02]
    for v in tv:
        col_ = ring[np.abs(S[ring, 0] - S[v, 0]) < 0.02]
        if len(col_) == 0 or S[v, 2] > midz:
            continue
        gather = np.clip((S[v, 1] - y_waist) / 0.08, 0.0, 1.0)
        zt = S[col_, 2].min() + (sc_y - S[v, 1]) * 0.18
        if zt < S[v, 2]:
            S[v, 2] += (zt - S[v, 2]) * gather
    for _ in range(6):
        S += 0.5 * h.laplacian(S, E, np.isin(np.arange(len(V)), top_v))
    nip = [J["breast.L____tail"], J["breast.R____tail"]]
    near = np.zeros(len(V), bool)
    for q in nip:
        near |= np.linalg.norm(V - q, axis=1) < 0.09
    near &= np.isin(np.arange(len(V)), top_v)
    for _ in range(18):
        S += 0.5 * h.laplacian(S, E, near)
        L = h.laplacian(S, E, near)
        S += 0.8 * N * np.maximum((L * N).sum(1), 0.0)[:, None]
    vi, ti, Tc = h.split_corners(V, VT, body_tris, top_tris)
    tunic = h.Part("tunic", "cloth")
    bl = h.loops(h.boundary_edges(Tt))
    # the neckline and waist edges smoothed; the sleeves' left ragged
    for lp in bl:
        lp = np.array(lp)
        on_arm = (u_arm[lp] > 0.15).mean() > 0.5
        for _ in range(3 if on_arm else 10):
            S[lp] = 0.5 * S[lp] + 0.25 * (np.roll(S[lp], 1, 0) + np.roll(S[lp], -1, 0))
    Ns = h.vertex_normals(S, Tt)
    tunic.add(S[vi], Ns[vi], flipv(VT[ti] * 8.0), Wfull[vi], Tc, cloth_color(V[vi], "d6d3cb", hem=False))
    for lp in bl:
        lp = np.array(lp)
        band_part = h.Part("_band", "cloth")
        h.band(band_part, S, Ns, Wfull, lp, -Ns, 0.006)
        P_, N_, UV_, W_, T_, _ = band_part.arrays()
        tunic.add(P_, N_, UV_, W_, T_, cloth_color(P_, "c9c5bb", hem=False))
    parts.append(tunic)
    mat("cloth", linear("d6d3cb"), 0.9, double=True)
    # the collar
    neck_loop = max(bl, key=lambda lp: S[lp, 1].mean())
    NL = S[neck_loop]
    dmin = np.min(np.linalg.norm(S[top_v][:, None, :] - NL[None, :, :], axis=2), axis=1)
    dist = np.full(len(V), 9.0)
    dist[top_v] = dmin
    czn = J["neck01____head"][2]
    in_col = (dist < 0.062) & ~((np.abs(V[:, 0]) < 0.012) & (V[:, 2] > czn))
    col_tris = np.array([all(in_col[t]) for t in Tt])
    Tcl = Tt[col_tris]
    C2 = S + Ns * 0.004
    cv = np.unique(Tcl.ravel())
    for _ in range(4):
        C2 += 0.5 * h.laplacian(C2, h.neighbours(len(V), Tcl), np.isin(np.arange(len(V)), cv))
    collar = h.Part("collar", "cloth")
    vi2, ti2, Tc2 = h.split_corners(V, VT, [t for t, k2 in zip(body_tris, top_tris) if k2], col_tris)
    for lp in h.loops(h.boundary_edges(Tcl)):
        lp = np.array(lp)
        for _ in range(10):
            C2[lp] = 0.5 * C2[lp] + 0.25 * (np.roll(C2[lp], 1, 0) + np.roll(C2[lp], -1, 0))
    Nc = h.vertex_normals(C2, Tcl)
    collar.add(C2[vi2], Nc[vi2], flipv(VT[ti2] * 8.0), Wfull[vi2], Tc2, cloth_color(V[vi2], "dedbd3", hem=False))
    for lp in h.loops(h.boundary_edges(Tcl)):
        bp_ = h.Part("_b", "cloth")
        h.band(bp_, C2, Nc, Wfull, lp, -Nc, 0.005)
        P_, N_, UV_, W_, T_, _ = bp_.arrays()
        collar.add(P_, N_, UV_, W_, T_, cloth_color(P_, "dedbd3", hem=False))
    parts.append(collar)
    # the breast pocket on her left, sagging with the penlight in it, a brown
    # stain bled out from under it
    bp = J["breast.L____tail"]
    pc = np.array([bp[0] - 0.004, bp[1] + 0.06])
    front_v = top_v[S[top_v, 2] > float(J["spine02____head"][2]) + 0.03]
    pw, ph_ = 0.105, 0.12
    nx_, ny_ = 9, 10
    gx, gy = np.meshgrid(np.linspace(-pw / 2, pw / 2, nx_), np.linspace(ph_ / 2, -ph_ / 2, ny_))
    G2 = np.stack([gx.ravel() + pc[0], gy.ravel() + pc[1]], 1)
    Pg = np.zeros((len(G2), 3))
    Ng = np.zeros((len(G2), 3))
    for i, (x_, y_) in enumerate(G2):
        d = np.hypot(S[front_v, 0] - x_, S[front_v, 1] - y_)
        nn = np.argsort(d)[:4]
        w_ = 1.0 / (d[nn] + 1e-4)
        Pg[i] = [x_, y_, float((S[front_v[nn], 2] * w_).sum() / w_.sum())]
        Ng[i] = (Ns[front_v[nn]] * w_[:, None]).sum(0)
    Ng /= np.linalg.norm(Ng, axis=1, keepdims=True)
    sag = np.exp(-((gx.ravel()) ** 2 / 0.0005 + (gy.ravel() + 0.03) ** 2 / 0.0009))
    Pg += Ng * (0.0035 + 0.004 * sag)[:, None]
    Tg = []
    for j in range(ny_ - 1):
        for i in range(nx_ - 1):
            a_, b_, c_, d_ = j * nx_ + i, j * nx_ + i + 1, (j + 1) * nx_ + i, (j + 1) * nx_ + i + 1
            Tg += [[a_, c_, b_], [b_, c_, d_]]
    Wg = weights_of_nearest(Pg, S, Wfull, front_v)
    pocket = h.Part("pocket", "cloth")
    pcol = cloth_color(Pg, "d6d3cb", hem=False)
    bled = np.clip(1.0 - np.hypot(gx.ravel() + 0.01, gy.ravel() + 0.045) / 0.06, 0, 1) ** 0.7
    pcol[:, :3] = pcol[:, :3] * (1 - bled[:, None]) + bled[:, None] * srgb_to_linear([0.3, 0.16, 0.11])
    pocket.add(Pg, Ng, (G2 - pc) * 8.0, Wg, Tg, pcol)
    rim = [j * nx_ for j in range(ny_)] + [(ny_ - 1) * nx_ + i for i in range(1, nx_)] + \
          [j * nx_ + nx_ - 1 for j in range(ny_ - 2, -1, -1)] + [i for i in range(nx_ - 2, 0, -1)]
    h.band(pocket, Pg, Ng, Wg, rim, -Ng, 0.004)
    pocket.C[-1] = cloth_color(pocket.P[-1], "d6d3cb", hem=False)
    parts.append(pocket)
    top_c = Pg[2] + np.array([0.0, 0.0, -0.002])
    pen = h.Part("penlight", "steel")
    h.cylinder_mesh(pen, top_c + np.array([0.0, -0.04, -0.001]), [0, 1, 0], 0.0055, 0.13, Wg[2], 10)
    h.box_mesh(pen, top_c + np.array([0.0, 0.0, 0.006]), [0.004, 0.04, 0.0015], np.eye(3), Wg[2])
    parts.append(pen)
    mat("steel", linear("8a8a8c"), 0.45, 0.6)
    # the placket, its buttons, two of them gone
    in_pl = (np.abs(V[:, 0]) < 0.016) & (V[:, 2] > 0.0)
    pl_tris = np.array([all(in_pl[t]) for t in Tt])
    Tpl = Tt[pl_tris]
    PL = S + Ns * 0.002
    plk = h.Part("placket", "cloth")
    vi4, ti4, Tc4 = h.split_corners(V, VT, [t for t, k2 in zip(body_tris, top_tris) if k2], pl_tris)
    plk.add(PL[vi4], h.vertex_normals(PL, Tpl)[vi4], flipv(VT[ti4] * 8.0), Wfull[vi4], Tc4, cloth_color(V[vi4], "cfcbc1", hem=False))
    parts.append(plk)
    btn = h.Part("buttons", "button")
    plv = np.unique(Tpl.ravel())
    for n_b, yb in enumerate(np.linspace(y_waist + 0.05, y_jug - 0.05, 5)):
        if n_b in (1, 3):
            continue   # torn off: a loose thread where each was
        i = plv[np.argmin(np.abs(PL[plv, 1] - yb) + 3 * np.abs(PL[plv, 0]))]
        h.cylinder_mesh(btn, PL[i] + Ns[i] * 0.0025, Ns[i], 0.0065, 0.003, Wfull[i], 10)
    parts.append(btn)
    mat("button", linear("b9b2a2"), 0.35)
    # the skin under the tunic goes (2 cm kept under every edge)
    edge_v = np.unique(np.concatenate([np.array(lp) for lp in bl]))
    de = np.min(np.linalg.norm(V[top_v][:, None, :] - V[edge_v][None, ::3, :], axis=2), axis=1)
    covered = np.zeros(len(V), bool)
    covered[top_v[de > 0.025]] = True
    covered[top_v[(u_arm[top_v] > 0.0) & (u_arm[top_v] < sleeve_u[top_v] - 0.06)]] = True
    keep &= ~np.array([all(covered[t[0]]) for t in body_tris])

    # ---- the skirt: lofted from the waist, hanging from the hips, its hem torn ----
    legs = (wsum("body", "spine_lo", "spine_mid", "hp_l", "hp_r", "hp_l_x", "hp_r_x", "kn_l", "kn_r", "kn_l_x", "kn_r_x") > 0.7)
    legs &= is_body
    legs &= arm_up + arm_low < 0.05
    top_y = y_waist - 0.005
    nring, nseg = 48, 160
    ts = np.linspace(0.0, 1.0, nring)
    th = np.linspace(0, 2 * math.pi, nseg, endpoint=False)
    U = np.stack([np.sin(th), np.zeros(nseg), np.cos(th)], 1)
    cx = 0.0
    czs = float(V[legs & (np.abs(V[:, 1] - J["upperleg01.L____head"][1]) < 0.03), 2].mean())
    Srest = h.rest_mats(bones, byname)
    Vr, _ = h.skin_points(V, Wfull, Srest)
    LP = Vr[legs]
    # the hem: ragged all round, five rips torn up into it
    ring_pts = np.stack([np.sin(th), np.zeros(nseg), np.cos(th)], 1)
    raise_m = 0.045 * fbm(ring_pts, 3.0, 13) + 0.02 * fbm(ring_pts, 11.0, 14)
    rng = np.random.default_rng(31)
    for _ in range(5):
        c0 = rng.uniform(0, 2 * math.pi)
        width = rng.uniform(0.06, 0.13)
        depth = rng.uniform(0.09, 0.2)
        dth = np.abs((th - c0 + math.pi) % (2 * math.pi) - math.pi)
        raise_m = np.maximum(raise_m, depth * np.clip(1.0 - dth / width, 0.0, 1.0) ** 1.4)
    rad = np.zeros((nring, nseg))
    ys_mean = top_y + (hem_y - top_y) * ts
    for i, y in enumerate(ys_mean):
        sl = LP[np.abs(LP[:, 1] - y) < 0.02]
        if len(sl) == 0:
            rad[i] = rad[i - 1]
            continue
        rel = sl - np.array([cx, 0, czs])
        rad[i] = (rel[:, [0, 2]] @ U[:, [0, 2]].T).max(0)
    ease = 0.016 + 0.018 * np.clip(ts * 3, 0, 1) + 0.055 * ts ** 1.5   # roomy over her hips and belly
    rad = rad + ease[:, None]
    for i in range(1, nring):
        rad[i] = np.maximum(rad[i], rad[i - 1] - 0.002)
    for _ in range(3):
        rad = 0.25 * np.roll(rad, 1, 1) + 0.5 * rad + 0.25 * np.roll(rad, -1, 1)
    folds = 1.0 + 0.035 * ts[:, None] ** 1.2 * np.sin(th[None, :] * 11 + 0.6 * np.sin(th[None, :] * 3))
    rings = []
    for i, t in enumerate(ts):
        y = top_y + (hem_y + raise_m - top_y) * t    # each column ends at its own torn hem
        rings.append(np.stack([cx + U[:, 0] * rad[i] * folds[i], y, czs + U[:, 2] * rad[i] * folds[i]], 1))

    def skirt_w(P):
        Wk = np.zeros((len(P), len(names)))
        tt = np.clip((top_y - P[:, 1]) / (top_y - hem_y), 0, 1)
        thigh = 0.85 * tt ** 1.2
        sx = np.clip(0.5 - P[:, 0] / 0.12, 0, 1)
        Wk[:, col["hp_l"]] = thigh * sx
        Wk[:, col["hp_r"]] = thigh * (1 - sx)
        Wk[:, col["skirt"]] = (1 - thigh) * 0.5 * tt
        Wk[:, col["body"]] = (1 - thigh) * (1 - 0.5 * tt)
        return Wk

    def to_bind(rs):
        return [h.unskin_points(r, skirt_w(r), Srest) for r in rs]
    skirt = h.Part("skirt_cloth", "cloth")
    hem_ring = rings[-1]
    inner = [hem_ring, hem_ring * np.array([0.985, 1.0, 0.985]) + np.array([cx * 0.015, 0.02, czs * 0.015])]
    SP, ST = h.loft(skirt, to_bind(rings), skirt_w, uv_scale=8.0)
    h.loft(skirt, to_bind(inner), skirt_w, uv_scale=8.0)
    # threads hanging from the torn hem
    thr_rng = np.random.default_rng(57)
    for _ in range(34):
        j = int(thr_rng.integers(0, nseg))
        root = hem_ring[j]
        length = thr_rng.uniform(0.04, 0.15)
        wdt = thr_rng.uniform(0.0015, 0.003)
        side_ = np.cross(U[j], [0.0, 1.0, 0.0])
        pts = []
        for q in range(5):
            f = q / 4.0
            off = np.array([0.0, -length * f, 0.0]) + U[j] * 0.01 * f * f + side_ * thr_rng.normal(0, 0.004) * f
            pts += [root + off - side_ * wdt, root + off + side_ * wdt]
        pts = np.array(pts)
        tri_ = []
        for q in range(4):
            a_ = 2 * q
            tri_ += [[a_, a_ + 2, a_ + 1], [a_ + 1, a_ + 2, a_ + 3]]
        bind_pts = h.unskin_points(pts, skirt_w(pts), Srest)
        nrm = np.tile(U[j], (len(pts), 1))
        skirt.add(bind_pts, nrm, np.zeros((len(pts), 2)), skirt_w(pts), tri_)
        skirt.add(bind_pts, -nrm, np.zeros((len(pts), 2)), skirt_w(pts), np.array(tri_)[:, ::-1])
    P_all = np.concatenate(skirt.P)
    Vr_all, _ = h.skin_points(P_all, np.concatenate(skirt.W), Srest)
    skirt.C = [cloth_color(Vr_all, "d0ccc2")]
    skirt.C = [skirt.C[0][sum(len(p) for p in skirt.P[:i]):sum(len(p) for p in skirt.P[:i + 1])] for i in range(len(skirt.P))]
    parts.append(skirt)
    # the belt over the join, and its buckle
    belt = h.Part("belt", "cloth")
    bys = np.linspace(top_y + 0.022, top_y - 0.018, 3)
    brings = [np.stack([cx + U[:, 0] * (rad[0] + 0.004), np.full(nseg, y), czs + U[:, 2] * (rad[0] + 0.004)], 1) for y in bys]
    h.loft(belt, [h.unskin_points(r, weights_of_nearest(r, Vr, Wfull, trunk_sel), Srest) for r in brings],
           lambda P: weights_of_nearest(P, V, Wfull, trunk_sel), uv_scale=8.0)
    belt.C = [cloth_color(np.concatenate(belt.P), "6b6256", hem=False)]
    parts.append(belt)
    front_i = int(np.argmin(np.abs(th)))
    bk = h.unskin_points((rings[0][front_i] + np.array([0, 0.002, 0.008]))[None], weights_of_nearest(rings[0][front_i][None], Vr, Wfull, trunk_sel), Srest)[0]
    buckle = h.Part("buckle", "steel")
    h.box_mesh(buckle, bk, [0.06, 0.045, 0.006], np.eye(3), weights_of_nearest(bk[None], V, Wfull, trunk_sel)[0])
    parts.append(buckle)

    # ---- shoes: worn flats lofted round each foot, scuffed at the toe ----
    shoe = h.Part("shoes", "shoe")
    hidden = np.zeros(len(V), bool)
    for s_ in "lr":
        ank = byname["an_" + s_]["pos"]
        fsel = (wsum("an_" + s_) > 0.5) | ((wsum("kn_%s_x" % s_) > 0.3) & (V[:, 1] < ank[1] + 0.02))
        fsel &= (V[:, 1] < ank[1] + 0.045) & is_body
        FP = V[fsel]
        low = FP[FP[:, 1] < SOLE + 0.02]
        heel, toe = low[np.argmin(low[:, 2])], low[np.argmax(low[:, 2])]
        ax = toe - heel
        ax[1] = 0.0
        flen = float(np.linalg.norm(ax))
        ax /= flen
        lat = np.cross([0.0, 1.0, 0.0], ax)
        nsl, nrg = 18, 22
        phis = np.linspace(0, 2 * math.pi, nrg, endpoint=False)
        R = []
        for i in range(nsl):
            tt = (i + 0.5) / nsl
            c0 = heel + ax * (tt * flen)
            d = (FP - c0) @ ax
            sl = FP[np.abs(d) < 0.012]
            if len(sl) < 4:
                sl = FP[np.argsort(np.abs(d))[:12]]
            cl = (sl - c0) @ lat
            cy = sl[:, 1]
            ctr = np.array([cl.mean(), 0.5 * (cy.min() + cy.max())])
            Q = np.stack([cl, cy], 1) - ctr
            rr = []
            for ph in phis:
                u2 = np.array([math.cos(ph), math.sin(ph)])
                r = float((Q @ u2).max()) + 0.005
                pt2 = ctr + u2 * r
                y = pt2[1]
                if y < SOLE + 0.006:
                    y = 0.0
                rr.append(c0 + lat * pt2[0] + np.array([0.0, y - c0[1], 0.0]))
            R.append(np.array(rr))
        R = np.array(R)
        tip = R[-1].mean(0) + ax * 0.012
        back = R[0].mean(0) - ax * 0.006
        R = np.concatenate([0.35 * R[:1] + 0.65 * back, R, 0.4 * R[-1:] + 0.6 * tip])
        pool = np.where(fsel)[0]
        base_n = sum(len(x) for x in shoe.P)
        SP_, ST_ = h.loft(shoe, list(R), lambda P, pool=pool: weights_of_nearest(P, V, Wfull, pool), uv_scale=10.0)
        open_face = []
        for tri in ST_:
            pts = SP_[tri]
            along = ((pts - heel) @ ax) / flen
            open_face.append(bool((pts[:, 1] > SOLE + 0.055).all() and (along < 0.62).all()))
        shoe.T[-1] = shoe.T[-1][~np.array(open_face)]
        along_all = ((SP_ - heel) @ ax) / flen
        scuff = smoothstep(0.75, 1.0, along_all) * (0.5 + 0.5 * fbm(SP_, 30.0, 15))
        c_ = np.tile(srgb_to_linear([0.07, 0.065, 0.06]), (len(SP_), 1))
        c_ = c_ * (1 - scuff[:, None]) + scuff[:, None] * srgb_to_linear([0.24, 0.2, 0.17])
        shoe.C[-1] = np.concatenate([c_, np.ones((len(SP_), 1))], 1)
        hidden |= fsel & (V[:, 1] < SOLE + 0.05)
    parts.append(shoe)
    mat("shoe", linear("121110"), 0.5)
    keep &= ~np.array([all(hidden[t[0]]) for t in body_tris])

    # ---- the scalp under her hair: where it grows, and where the skin shows ----
    headw = Wfull[:, col["head"]]
    hj = byname["head"]["pos"]
    eye = 0.5 * (J["eye.L____head"] + J["eye.R____head"])
    brow_y = eye[1] + 0.03
    p_front = np.array([0.0, brow_y + 0.058, eye[2] + 0.005])
    p_nape = np.array([0.0, hj[1] - 0.005, hj[2] - 0.075])
    p_side = np.array([0.07, eye[1] + 0.035, hj[2] - 0.0])
    nrm_h = np.cross(p_nape - p_front, p_side - p_front)
    if nrm_h[1] < 0:
        nrm_h = -nrm_h
    in_hair = ((V - p_front) @ nrm_h > 0) & (headw > 0.6) & is_body
    # ---- the music box on its cord at her hip ----
    Wb = np.zeros(len(names))
    Wb[col["box"]] = 1.0
    bxp = byname["box"]["pos"]
    mb = h.Part("music_box", "wood")
    h.cylinder_mesh(mb, bxp + np.array([0, -0.15, 0]), [0, 1, 0], 0.0025, 0.3, Wb, 6)
    h.box_mesh(mb, bxp + np.array([0, -0.33, 0]), [0.075, 0.055, 0.06], np.eye(3), Wb)
    parts.append(mb)
    mat("wood", linear("4f3622"), 0.6)

    # ---- the skin: the body less what clothes hide; legs below the hem in torn stockings ----
    vi, ti, Tc = h.split_corners(V, VT, body_tris, keep)
    skin = h.Part("skin", "skin")
    stock = h.Part("stockings", "stocking")
    legv = ((wsum("hp_l", "hp_r", "hp_l_x", "hp_r_x", "kn_l", "kn_r", "kn_l_x", "kn_r_x", "an_l", "an_r") > 0.5)
            & (V[:, 1] < float(J["upperleg01.L____head"][1]) - 0.06))
    tri_leg = legv[vi][Tc].all(1)
    for part, sel in ((skin, ~tri_leg), (stock, tri_leg)):
        Tsel = Tc[sel]
        used = np.unique(Tsel.ravel())
        remap = -np.ones(len(vi), int)
        remap[used] = np.arange(len(used))
        P_ = V[vi[used]]
        C_ = None
        if part is stock:
            # worn through at the knees, a ladder run down from each hole,
            # and moth holes: her pale skin shows through them
            dark = srgb_to_linear([0.34, 0.31, 0.28])
            pale = srgb_to_linear([0.78, 0.72, 0.65])
            hole = np.zeros(len(P_))
            for s_ in "lr":
                kn = byname["kn_" + s_]["pos"] + np.array([0.0, -0.16 if s_ == "l" else -0.23, 0.045])
                hole = np.maximum(hole, smoothstep(0.045, 0.025, np.linalg.norm((P_ - kn) * np.array([1.0, 0.7, 1.0]), axis=1)))
                run_x = kn[0] + (0.012 if s_ == "l" else -0.009)
                ladder = (np.abs(P_[:, 0] - run_x) < 0.0065) & (P_[:, 2] > kn[2] - 0.03) & (P_[:, 1] < kn[1]) & (P_[:, 1] > kn[1] - 0.22)
                hole = np.maximum(hole, 0.55 * ladder)
            hole = np.maximum(hole, smoothstep(0.82, 0.86, fbm(P_, 35.0, 19)))
            C_ = np.concatenate([dark * (1 - hole[:, None]) + pale * hole[:, None], np.ones((len(P_), 1))], 1)
        part.add(P_, N[vi[used]], flipv(VT[ti[used]]), Wfull[vi[used]], remap[Tsel], C_)
    parts.append(skin)
    parts.append(stock)
    mat("skin", linear("cbbcae"), 0.55)
    mat("stocking", linear("26221e"), 0.6)

    # ---- the face's parts: eyes on their bones, teeth on the head and jaw, brows, lashes ----
    # (MakeHuman's eye is the ball and a clear cornea over it, the cornea
    # mapped to the eye texture's transparent corner: its own glossy part.)
    evi, eP, eUV, eT = corners(eye_fit, eye_VT, eye_faces)
    We = np.zeros((len(eP), len(names)))
    left = eP[:, 0] < 0.0
    We[left, col["eye_l"]] = 1.0
    We[~left, col["eye_r"]] = 1.0
    eye_rgba = np.asarray(Image.open(os.path.join(ASSETS, "eyes/materials/grey_eye.png")).convert("RGBA"))
    cuv = eUV[eT].mean(1)
    px = np.clip((cuv[:, 0] * eye_rgba.shape[1]).astype(int), 0, eye_rgba.shape[1] - 1)
    py = np.clip((cuv[:, 1] * eye_rgba.shape[0]).astype(int), 0, eye_rgba.shape[0] - 1)
    clear = eye_rgba[py, px, 3] < 128
    for part_name, mat_name, sel in (("eyes", "eye", ~clear), ("cornea", "cornea", clear)):
        part = h.Part(part_name, mat_name)
        part.add(eP, h.vertex_normals(eP, eT[sel]), eUV, We, eT[sel])
        parts.append(part)
    stats["cornea_triangles"] = int(clear.sum())
    mat("eye", linear("ffffff"), 0.08)
    mat("cornea", linear("ffffff"), 0.02)
    t_fit, t_VT, t_faces, t_bind = fit_proxy(V, "teeth/teeth_base/teeth_base")
    tvi, tP, tUV, tT = corners(t_fit, t_VT, t_faces)
    # Upper teeth on the head, lower on the jaw: as MakeHuman's own weights
    # have the teeth they are fitted to (a cut by height splits the crowns).
    Wb_ = bound_weights(t_bind, Wfull)[tvi]
    lower = Wb_[:, col["jaw"]] > Wb_[:, col["head"]]
    Wt = np.zeros((len(tP), len(names)))
    Wt[~lower, col["head"]] = 1.0
    Wt[lower, col["jaw"]] = 1.0
    stats["teeth_on_jaw"] = int(lower.sum())
    teeth = h.Part("teeth", "teeth")
    teeth.add(tP, h.vertex_normals(tP, tT), tUV, Wt, tT)
    parts.append(teeth)
    # the dark of her mouth behind them, opening with her jaw
    mouth = h.Part("mouth", "mouth")
    mc = tP.mean(0) + np.array([0.0, 0.0, -0.012])
    h.ellipsoid_mesh(mouth, mc, [0.026, 0.022, 0.024], np.eye(3), np.zeros(len(names)), 8, 14)
    MP = mouth.P[-1]
    Wm = np.zeros((len(MP), len(names)))
    Wm[MP[:, 1] >= mc[1], col["head"]] = 1.0
    Wm[MP[:, 1] < mc[1], col["jaw"]] = 1.0
    mouth.W[-1] = Wm
    parts.append(mouth)
    mat("mouth", linear("140606"), 1.0)
    mat("teeth", linear("b8a67c"), 0.45)
    for stem, part_name, mat_name in (("eyebrows/eyebrow010/eyebrow010", "brows", "brow"),
                                      ("eyelashes/eyelashes02/eyelashes02", "lashes", "lash")):
        p_fit, p_VT, p_faces, p_bind = fit_proxy(V, stem)
        pvi, pP, pUV, pT = corners(p_fit, p_VT, p_faces)
        Wp = bound_weights(p_bind, Wfull)[pvi]
        part = h.Part(part_name, mat_name)
        part.add(pP, h.vertex_normals(pP, pT), pUV, Wp, pT)
        parts.append(part)
        mat(mat_name, linear("8c8780"), 0.7, double=True)

    # ---- her hair: lank grey strands grown from her scalp, in greasy clumps,
    # lying over her skull and hanging from it, hung on the chains ----
    hP, hT, hUV, hC, n_strands = grow_hair(V, N, T_body, in_hair, is_body, Wfull, col, head_c, byname)
    Whr = np.zeros((len(hP), len(names)))
    az = np.arctan2(hP[:, 0] - head_c[0], hP[:, 2] - head_c[2]) % (2 * math.pi)
    depth = np.clip(root_y - hP[:, 1], 0.0, None)
    above = smoothstep(0.0, 0.03, depth)
    fpos = np.interp(az, chain_az, np.arange(HAIR_CHAINS))   # outside the span: the end chains
    c0 = np.floor(fpos).astype(int)
    c1 = np.minimum(c0 + 1, HAIR_CHAINS - 1)
    cf = fpos - c0
    sf = depth / HAIR_SEGMENT_M
    s0 = np.minimum(np.floor(sf).astype(int), HAIR_SEGMENTS - 1)
    s1 = np.minimum(s0 + 1, HAIR_SEGMENTS - 1)
    sfr = np.where(s0 == HAIR_SEGMENTS - 1, 0.0, sf - np.floor(sf))
    hcol = {(c, s_): col["hair%d_%d" % (c, s_)] for c in range(HAIR_CHAINS) for s_ in range(HAIR_SEGMENTS)}
    Whr[:, col["head"]] = 1.0 - above
    for i in range(len(hP)):
        for cc, wc in ((c0[i], 1.0 - cf[i]), (c1[i], cf[i])):
            Whr[i, hcol[(cc, s0[i])]] += above[i] * wc * (1.0 - sfr[i])
            Whr[i, hcol[(cc, s1[i])]] += above[i] * wc * sfr[i]
    hair = h.Part("hair", "hair")
    hair.add(hP, np.zeros_like(hP), hUV, Whr, hT, hC)
    hair.N[-1] = ribbon_normals(hP, hT, head_c)
    parts.append(hair)
    stats["hair_strands"] = int(n_strands)
    mat("hair", linear("9c9a94"), 0.55, double=True)

    # ---- write ----
    out_parts = []
    tri_counts = {}
    for part in parts:
        P, Nn, UV, Wp, Tp, Cp = part.arrays()
        idx, w = h.top4(Wp)
        out_parts.append(h.named_part(part.name, part.material, (P, Nn, UV, (idx, w), Tp, Cp)))
        tri_counts[part.name] = int(len(Tp))
    stats["triangles"] = tri_counts
    stats["triangles_total"] = int(sum(tri_counts.values()))
    stats["bones"] = len(bones)
    h.write_glb(os.path.join(OUT, "matron.glb"), bones, out_parts, materials, root="Matron", generator="tools/body/build_matron_body.py")
    rig = []
    for b in bones:
        r = {"name": b["name"], "parent": b["parent"], "t": [float(x) for x in b["t"]],
             "q": [float(x) for x in h.quat_from_m(b["L"])], "euler": h.euler_yxz(b["L"]), "mode": b["mode"],
             "pivot": b["pivot"], "pos": [float(x) for x in b["pos"]]}
        if b["name"] in ("sh_l", "sh_r"):
            r["abduct"] = -h.ARM_ABDUCT if b["name"] == "sh_l" else h.ARM_ABDUCT
        for key in ("share", "length"):
            if key in b:
                r[key] = b[key]
        rig.append(r)

    def local(bn, pt):
        bb = byname[bn]
        return [float(x) for x in bb["F"].T @ (np.asarray(pt) - bb["pos"])]
    contacts = {}
    hv = np.where(Wfull[:, col["head"]] > 0.5)[0]
    hv = hv[is_body[hv]]
    contacts["head"] = {"top": local("head", V[hv[np.argmax(V[hv, 1])]])}
    for s_ in "lr":
        mh = h.SIDES[s_]
        fv_ = np.where(wsum("an_" + s_) > 0.5)[0]
        fv_ = fv_[is_body[fv_]]
        low = fv_[V[fv_, 1] < V[fv_, 1].min() + 0.01]
        heel = V[low[np.argmin(V[low, 2])]].copy()
        toe = V[low[np.argmax(V[low, 2])]].copy()
        heel[1] = toe[1] = 0.0
        contacts["an_" + s_] = {"heel": local("an_" + s_, heel), "toe": local("an_" + s_, toe)}
        contacts["f12_" + s_] = {"tip": local("f12_" + s_, J["finger3-3%s____tail" % mh])}
        contacts["wr_" + s_] = {"knuckles": local("wr_" + s_, J["finger3-1%s____head" % mh])}
    eyes_out = {s: {"bone": "eye_" + s, "centre": [float(x) for x in eye_centre[s]]} for s in "lr"}
    json.dump({"bones": rig, "stats": stats, "contacts": contacts, "hair": {"chains": chains, "skull": [float(x) for x in head_c],
               "skull_radius": 0.1}, "eyes": eyes_out}, open(os.path.join(OUT, "matron_rig.json"), "w"), indent=1)
    json.dump(stats, open(os.path.join(OUT, "matron_stats.json"), "w"), indent=1)
    return stats, (V, VT, body_tris, Wfull, col, J, byname, in_hair, is_body)


def weights_of_nearest(P, V, W, pool):
    """Each point takes the skin weights of the nearest body vertex in `pool`."""
    P = np.atleast_2d(P)
    d = np.linalg.norm(V[pool][None, :, :] - P[:, None, :], axis=2)
    return W[pool[np.argmin(d, axis=1)]]


def texture_regions(V, VT, body_tris, Wfull, col, J, byname, in_hair, is_body):
    """Body triangles by region, for painting the skin in its UV space."""
    T = np.array([t[0] for t in body_tris])
    TT = np.array([t[1] for t in body_tris])
    UV = VT.copy()
    UV[:, 1] = 1.0 - UV[:, 1]

    def w(*bs):
        return sum(Wfull[:, col[b]] for b in bs)

    def tri_sel(vmask):
        return vmask[T].all(1)
    fingers = sum(Wfull[:, col[n]] for n in col if n[0] in "ft" and n[1].isdigit())
    hands = w("wr_l", "wr_r") + fingers > 0.5
    head = w("head", "jaw") > 0.5
    eye = 0.5 * (J["eye.L____head"] + J["eye.R____head"])
    face = head & (V[:, 2] > eye[2] - 0.06)
    forehead = face & (V[:, 1] > eye[1] + 0.035) & (V[:, 1] < eye[1] + 0.06) & (np.abs(V[:, 0]) < 0.04)
    temples = head & (np.abs(V[:, 0]) > 0.055) & (np.abs(V[:, 1] - (eye[1] + 0.02)) < 0.03) & (V[:, 2] > eye[2] - 0.05)
    chin = face & (V[:, 1] < eye[1] - 0.08)
    forearms = w("el_l", "el_l_x", "el_r", "el_r_x") > 0.5
    chest = (w("chest", "spine_mid") > 0.5) & (V[:, 2] > J["spine01____head"][2]) & (V[:, 1] > J["breast.L____head"][1] + 0.03)
    knees = np.zeros(len(V), bool)
    for s in "lr":
        kn = byname["kn_" + s]["pos"]
        knees |= (np.linalg.norm(V - kn, axis=1) < 0.075) & (V[:, 2] > kn[2] - 0.01)
    # nails: the fingertips' backs; their very tips for the dirt
    tip_v = np.zeros(len(V), bool)
    rim_v = np.zeros(len(V), bool)
    for mh in (".L", ".R"):
        for f in ("finger1", "finger2", "finger3", "finger4", "finger5"):
            a = J["%s-3%s____head" % (f, mh)]
            b = J["%s-3%s____tail" % (f, mh)]
            ax = b - a
            ln = np.linalg.norm(ax)
            ax /= ln
            u = (V - a) @ ax / ln
            rad = np.linalg.norm((V - a) - np.outer((V - a) @ ax, ax), axis=1)
            near = (u > 0.35) & (u < 1.2) & (rad < 0.012)
            # the back of the finger: away from the palm (the hand's own normal)
            wr = J["wrist%s____head" % mh]
            mid = J["finger3-1%s____head" % mh]
            pn = np.cross(J["finger2-1%s____head" % mh] - J["finger5-1%s____head" % mh], mid - wr)
            if (J["finger3-3%s____tail" % mh] - mid) @ pn < 0:
                pn = -pn
            back = ((V - a) @ pn) < 0
            tip_v |= near & back & is_body
            rim_v |= near & back & (u > 0.92) & is_body
    scalp = in_hair
    sel = {"scalp": scalp, "forehead": forehead, "face": face, "temples": temples, "chin": chin, "hands": hands,
           "forearms": forearms, "chest": chest, "knees": knees, "nails": tip_v, "nail_tips": rim_v}
    regions = {k: tri_sel(v) for k, v in sel.items()}
    return UV, TT, regions


def main():
    stats, ctx = build()
    V, VT, body_tris, Wfull, col, J, byname, in_hair, is_body = ctx
    UV, TT, regions = texture_regions(V, VT, body_tris, Wfull, col, J, byname, in_hair, is_body)
    skin_maps(OUT, UV, TT, regions)
    hair_map(OUT)
    eye_map(OUT)
    teeth_map(OUT)
    strands_map("eyebrows/eyebrow010/eyebrow010.png", "brow_a.png", [0.55, 0.53, 0.5])
    strands_map("eyelashes/eyelashes02/eyelashes02.png", "lash_a.png", [0.35, 0.33, 0.31])
    cloth_maps(OUT)
    s = stats
    print("matron h=%.2f m  bmi=%.1f mass=%.0f kg  waist=%.0f hip=%.0f bust=%.0f arm=%.1f calf=%.1f cm  hand=%.3f m  tris=%d bones=%d" % (
        PRESET["stand_m"], s["bmi"], s["mass_kg"], s["waist_cm"], s["hip_cm"], s["bust_cm"], s["upper_arm_cm"],
        s["calf_cm"], s["hand_len_m"], s["triangles_total"], s["bones"]))


if __name__ == "__main__":
    main()
