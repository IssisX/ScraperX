# Shape presets for a frail old WOMAN (the v1 head read as an old man): flat-shaded painter renders.
import sys, os, numpy as np
from PIL import Image, ImageDraw
T1 = '/tmp/claude-0/-home-user-ScraperX/290f45ef-c41d-5b08-903d-c4b10970a44f/scratchpad/overhaul/skin_face_eyes/tools'
sys.path.insert(0, T1)
from mhload import load_obj, target, AGE, AGE_SCALE, SAG
D1 = '/tmp/claude-0/-home-user-ScraperX/290f45ef-c41d-5b08-903d-c4b10970a44f/scratchpad/overhaul/skin_face_eyes/dl/mh_targets'
D2 = D1 + '2'
V, VT, F, G = load_obj()
cache = {}
def TT(n):
    if n not in cache:
        d = D2 if os.path.exists(f'{D2}/{n}.target') else D1
        cache[n] = target(len(V), n, d)
    return cache[n]
BASE_MACRO = {'caucasian-female-old': 1.0, 'universal-female-old-minmuscle-averageweight': 1.0}
def make(macro, extra):
    P = V.copy()
    for k, w in macro.items(): P = P + TT(k) * w
    for k, w in AGE.items(): P = P + TT(k) * w * AGE_SCALE.get(k, 1.0)
    for k, w in SAG.items(): P = P + TT(k) * w
    for k, w in extra.items(): P = P + TT(k) * w
    eyev = [i for f, g in zip(F, G) if g in ('helper-l-eye', 'helper-r-eye') for i, _ in f]
    P = P + (np.array([0, 7.2841, 1.2454]) - P[eyev].mean(0))
    return P
tris = []
for f, g in zip(F, G):
    if g not in ('body', 'helper-l-eye', 'helper-r-eye'): continue
    for k in range(1, len(f) - 1): tris.append((f[0][0], f[k][0], f[k + 1][0]))
Tr = np.array(tris)
def render(P, yaw, W=400):
    c, s = np.cos(yaw), np.sin(yaw); R = np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])
    Q = (P - [0, 7.0, 0.6]) @ R.T
    a, b, cc = Q[Tr[:, 0]], Q[Tr[:, 1]], Q[Tr[:, 2]]
    keep = (a[:, 1] > -1.6) & (b[:, 1] > -1.6)
    n = np.cross(b - a, cc - a); n /= np.linalg.norm(n, axis=1, keepdims=True) + 1e-9
    L = np.array([-0.3, 0.7, 0.65]); L /= np.linalg.norm(L)
    sh = np.clip(n @ L, 0, 1) * 0.75 + 0.18
    depth = (a[:, 2] + b[:, 2] + cc[:, 2]) / 3
    img = Image.new('RGB', (W, W), (25, 25, 28)); d = ImageDraw.Draw(img); sc = W / 2.6
    for i in np.argsort(depth):
        if not keep[i] or n[i, 2] <= 0: continue
        pts = [(W / 2 + p[0] * sc, W / 2 - p[1] * sc) for p in (a[i], b[i], cc[i])]
        v = sh[i]; d.polygon(pts, fill=(int(220 * v), int(205 * v), int(190 * v)))
    return img
FEM = {'head-oval': 0.5, 'chin-width-decr': 0.5, 'nose-scale-horiz-decr': 0.45, 'nose-point-width-decr': 0.5,
       'nose-flaring-decr': 0.4, 'forehead-nubian-decr': 0.6, 'mouth-scale-horiz-decr': 0.25}
FRAIL = {'forehead-temple-decr': 0.7, 'head-fat-decr': 0.4, 'neck-scale-horiz-decr': 0.4, 'l-cheek-inner-decr': 0.4, 'r-cheek-inner-decr': 0.4}
LIP = {'mouth-trans-down': 0.25, 'mouth-upperlip-height-decr': 0.3, 'mouth-cupidsbow-decr': 0.4}
presets = {
  'v1 (current)': (BASE_MACRO, {}),
  'v2 feminine': (BASE_MACRO, FEM),
  'v3 fem+frail': (BASE_MACRO, {**FEM, **FRAIL}),
  'v4 fem+frail+lips': (BASE_MACRO, {**FEM, **FRAIL, **LIP}),
  'v5 v4, minweight 0.5': ({'caucasian-female-old': 1.0, 'universal-female-old-minmuscle-averageweight': 0.5, 'universal-female-old-minmuscle-minweight': 0.5}, {**FEM, **FRAIL, **LIP}),
  'v6 v4, oval 1.0 chin 0.8': (BASE_MACRO, {**FEM, **FRAIL, **LIP, 'head-oval': 1.0, 'chin-width-decr': 0.8, 'nose-volume-decr': 0.3}),
}
sel = sys.argv[2:] or list(presets)
rows = []
for name in sel:
    P = make(*presets[name])
    a = render(P, 0.0); b = render(P, 0.6)
    row = Image.new('RGB', (800, 420), (0, 0, 0)); row.paste(a, (0, 0)); row.paste(b, (400, 0))
    ImageDraw.Draw(row).text((6, 404), name, fill=(255, 230, 80)); rows.append(row)
cols = 3
W = Image.new('RGB', (800 * cols, 420 * ((len(rows) + cols - 1) // cols)))
for i, r in enumerate(rows): W.paste(r, ((i % cols) * 800, (i // cols) * 420))
W.save(sys.argv[1])
