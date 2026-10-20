# Quick flat-shaded shape comparison of macro/age presets (painter's algorithm, numpy + PIL).
import sys, numpy as np
from PIL import Image, ImageDraw
sys.path.insert(0, '.')
from mhload import load_obj, target
TD = '../dl/mh_targets'
V, VT, F, G = load_obj()
def T(n): return target(len(V), n, TD)
AGE = {'head-age-incr': 1.0, 'l-cheek-volume-decr': 1.0, 'r-cheek-volume-decr': 0.9, 'l-eye-bag-incr': 0.8, 'r-eye-bag-incr': 1.0,
       'l-eye-push1-in': 0.8, 'r-eye-push1-in': 0.7, 'mouth-angles-down': 0.6, 'nose-hump-incr': 0.7, 'mouth-laugh-lines-out': 0.8,
       'l-cheek-bones-incr': 0.6, 'r-cheek-bones-incr': 0.6, 'mouth-upperlip-volume-decr': 0.8, 'mouth-lowerlip-volume-decr': 0.6}
cache = {}
def TT(n):
    if n not in cache: cache[n] = T(n)
    return cache[n]
def make(macro, age_scale, extra={}):
    P = V.copy()
    for k, w in macro.items(): P = P + TT(k) * w
    for k, w in AGE.items(): P = P + TT(k) * w * age_scale.get(k, age_scale.get('*', 1.0))
    for k, w in extra.items(): P = P + TT(k) * w
    eyev = [i for f, g in zip(F, G) if g in ('helper-l-eye', 'helper-r-eye') for i, _ in f]
    mid = P[eyev].mean(0); P = P + (np.array([0, 7.2841, 1.2454]) - mid)
    return P
tris = []
for f, g in zip(F, G):
    if g not in ('body', 'helper-l-eye', 'helper-r-eye'): continue
    for k in range(1, len(f) - 1): tris.append((f[0][0], f[k][0], f[k + 1][0]))
Tr = np.array(tris)
def render(P, yaw, W=360):
    c, s = np.cos(yaw), np.sin(yaw); R = np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])
    Q = (P - [0, 7.0, 0.6]) @ R.T
    a, b, cc = Q[Tr[:, 0]], Q[Tr[:, 1]], Q[Tr[:, 2]]
    keep = (a[:, 1] > -1.6) & (b[:, 1] > -1.6)
    n = np.cross(b - a, cc - a); n /= np.linalg.norm(n, axis=1, keepdims=True) + 1e-9
    L = np.array([-0.4, 0.5, 0.75]); L /= np.linalg.norm(L)
    sh = np.clip(n @ L, 0, 1) * 0.8 + 0.15
    depth = (a[:, 2] + b[:, 2] + cc[:, 2]) / 3
    img = Image.new('RGB', (W, W), (25, 25, 28)); d = ImageDraw.Draw(img); sc = W / 2.6
    for i in np.argsort(depth):
        if not keep[i] or n[i, 2] <= 0: continue
        pts = [(W / 2 + p[0] * sc, W / 2 - p[1] * sc) for p in (a[i], b[i], cc[i])]
        v = sh[i]; d.polygon(pts, fill=(int(220 * v), int(200 * v), int(185 * v)))
    return img
presets = {
 'A_prev(neutral+minw)': ({'universal-female-old-minmuscle-minweight': 1.0}, {}),
 'B_female-old+minw': ({'caucasian-female-old': 1.0, 'universal-female-old-minmuscle-minweight': 1.0}, {}),
 'C_female-old+avgw': ({'caucasian-female-old': 1.0, 'universal-female-old-minmuscle-averageweight': 1.0}, {'l-cheek-volume-decr': 0.35, 'r-cheek-volume-decr': 0.35}),
 'D_female-old+half': ({'caucasian-female-old': 1.0, 'universal-female-old-minmuscle-minweight': 0.5}, {'l-cheek-volume-decr': 0.5, 'r-cheek-volume-decr': 0.5}),
}
row = []
for name, (mac, ag) in presets.items():
    P = make(mac, ag, {'l-cheek-trans-down': 0.5, 'r-cheek-trans-down': 0.5} if name[0] in 'CD' else {})
    im = Image.new('RGB', (720, 380), (0, 0, 0)); im.paste(render(P, 0.0), (0, 0)); im.paste(render(P, 0.75), (360, 0))
    ImageDraw.Draw(im).text((8, 362), name, fill=(255, 255, 0)); row.append(im)
S = Image.new('RGB', (1440, 760))
for i, im in enumerate(row): S.paste(im, ((i % 2) * 720, (i // 2) * 380))
S.save('../renders/shape_presets.png')
