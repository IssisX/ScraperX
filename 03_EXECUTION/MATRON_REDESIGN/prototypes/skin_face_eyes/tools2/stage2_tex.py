# Stage 2: texture bake in the repacked head UV space.
#   albedo.png   sRGB  - the CC0 photo skin, re-graded pale/sallow, + age spots, veins, under-eye, capillaries
#   normal.png   RGB   - tangent-space detail: photo wrinkles (high-pass), pores, crepe, wrinkle families
#   maskA.png    RGBA  - R dose from the left breast pocket (log), G veins, B oiliness, A cavity
#   maskB.png    RGBA  - R flake noise, G petechiae, B lid/lip wet, A hair (scalp) coverage
import sys, os, numpy as np
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
from imgops import gauss, dilate, fbm, vnoise, worley, smoothstep, rgb_to_lab, lab_to_rgb
WORK = os.path.join(HERE, '..', 'work2'); OUT = os.path.join(HERE, '..', 'probe2', 'tex'); os.makedirs(OUT, exist_ok=True)
SRC = os.path.join(HERE, '..', 'dl', 'sys', 'skins', 'old_caucasian_female', 'old_lightskinned_female_diffuse.png')
RES = int(sys.argv[1]) if len(sys.argv) > 1 else 2048
VARIANT = sys.argv[2] if len(sys.argv) > 2 else 'real'      # 'real' or 'wrong'
r = np.load(os.path.join(WORK, f'raster_{RES}.npz'))
mask = r['mask'] > 0; ys, xs = np.nonzero(mask)
P = r['P'][mask].astype(np.float64); N = r['N'][mask].astype(np.float64); isl = np.rint(r['isl'][mask, 0]).astype(int)
ouv = r['ouv'][mask]; ao = r['ao'][mask, 0]
n = len(P)
def img(vals, ch=1, fill=0.0):
    a = np.full((RES, RES, ch), fill, np.float32); a[ys, xs] = vals.reshape(n, ch); return a

# ---------------- landmarks (dm, aged hm08) ----------------
MM = np.load(os.path.join(WORK, 'mesh.npz')); _e = MM['eyes']
EYE = {+1: _e[0, :3].copy(), -1: _e[1, :3].copy()}
MOUTH_C = np.array([0.0, 6.66, 1.52]); NOSE = np.array([0.0, 6.938, 1.753])
_PA = MM['P']; _band = (_PA[:, 1] > 4.3) & (_PA[:, 1] < 4.6) & (_PA[:, 0] > 0.6) & (_PA[:, 0] < 0.95)
POCKET = _PA[_band][np.argmax(_PA[_band][:, 2])] + np.array([0.0, 0.0, 0.08])   # her left breast pocket (the lost source)
print('pocket', POCKET.round(3))
def d3(c): return np.linalg.norm(P - c, axis=1)
x, y, z = P[:, 0], P[:, 1], P[:, 2]
head_isl = isl == 0; mouth_isl = isl == 1; socket_isl = (isl == 3) | (isl == 4)
front = smoothstep(0.3, 0.9, z)

# ---------------- source photo ----------------
src = np.asarray(Image.open(SRC).convert('RGB')).astype(np.float32) / 255.0
S = src.shape[0]
u = np.clip(ouv[:, 0] * S - 0.5, 0, S - 1.001); v = np.clip(ouv[:, 1] * S - 0.5, 0, S - 1.001)
u0 = np.floor(u).astype(int); v0 = np.floor(v).astype(int); fu = (u - u0)[:, None]; fv = (v - v0)[:, None]
photo = (src[v0, u0] * (1 - fu) * (1 - fv) + src[v0, u0 + 1] * fu * (1 - fv) + src[v0 + 1, u0] * (1 - fu) * fv + src[v0 + 1, u0 + 1] * fu * fv)
lab = rgb_to_lab(photo)
L0 = lab[:, 0].copy()

# ---------------- scalp hair coverage (the photo's dark stubble cap) ----------------
hairline = y - (7.85 + (z - 1.05) * 0.55)          # >0 above a plane tilted forehead->nape
temple_band = smoothstep(0.52, 0.68, np.abs(x)) * smoothstep(7.3, 7.55, y) * smoothstep(1.05, 0.55, z)
Ls = gauss(img(L0), 6.0 * RES / 2048)[ys, xs, 0]
scalp_zone = np.clip(smoothstep(-0.35, -0.1, hairline) + temple_band, 0, 1) * head_isl
hair = scalp_zone * smoothstep(60, 50, Ls)
hair = np.maximum(hair, smoothstep(-0.17, -0.07, hairline + 0.04 * (fbm(P * 30.0, 2, seed=77) - 0.5)) * head_isl)

# ---------------- base grade: kill the peach/red, keep a sallow undertone ----------------
# De-light the photo: its low and mid frequencies carry baked shading (dark sockets, shaded
# nasolabial folds). Keep the fine band (pores, creases, fine pigment) and rebuild the broad tone.
def bands(ch):
    im = img(ch)
    lo = gauss(im, 40.0 * RES / 2048)[ys, xs, 0]; md = gauss(im, 10.0 * RES / 2048)[ys, xs, 0]
    return lo, md - lo, ch - md
Llo, Lmd, Lhi = bands(L0); Alo, Amd, Ahi = bands(lab[:, 1]); Blo, Bmd, Bhi = bands(lab[:, 2])
A = lab[:, 1]; B = lab[:, 2]
face_m = head_isl.astype(float)
L2 = 68.0 + 0.2 * (Llo - 66.0) + 0.3 * Lmd + 1.0 * Lhi
A2 = 1.5 + 0.30 * Alo + 0.30 * Amd + 0.8 * Ahi
B2 = 5.0 + 0.45 * Blo + 0.30 * Bmd + 0.8 * Bhi
# low-frequency mottling (uneven old skin): a little blotchy pink/olive variation
mott = fbm(P * 6.0, 4, seed=3) - 0.5; mott2 = fbm(P * 14.0, 3, seed=9) - 0.5
A2 += mott * 5.0 + mott2 * 2.0; B2 += -mott * 2.0; L2 += mott2 * 2.5
# sallow forehead/chin/neck, slightly greyer temples
B2 += 2.5 * smoothstep(7.5, 7.9, y) * front + 2.0 * smoothstep(6.4, 5.8, y)
# keep skin hue warm: b* never far below a* (stops the lilac cast around the mouth and jaw)
B2 = np.maximum(B2, 0.85 * A2 + 3.5)
# under-eye: thin violet-brown skin; upper lid: faint pink-violet
for s, c in EYE.items():
    q = P - c
    under = np.exp(-((q[:, 0] * 0.9) ** 2 / 0.025 + (q[:, 1] + 0.17) ** 2 / 0.006)) * smoothstep(0.9, 1.15, z)
    lid = np.exp(-(q[:, 0] ** 2 / 0.03 + (q[:, 1] - 0.10) ** 2 / 0.004)) * smoothstep(0.95, 1.2, z)
    L2 -= under * 9.0; A2 += under * 2.5; B2 -= under * 6.0
    L2 -= lid * 3.0; A2 += lid * 3.0; B2 -= lid * 3.0
# capillary blush: nose tip/alae and cheekbones, as fine broken threads (telangiectasia), not a flush
nose_w = np.exp(-(d3(NOSE) ** 2) / 0.03)
cheek_w = sum(np.exp(-(((x - s * 0.55) ** 2) / 0.04 + ((y - 7.0) ** 2) / 0.03)) * smoothstep(0.8, 1.2, z) for s in (-1, 1))
thread = smoothstep(0.86, 0.97, 1 - np.abs(2 * fbm(P * np.array([40, 25, 40]), 3, seed=21) - 1))
A2 += nose_w * (5.0 + 6.0 * thread) + cheek_w * (2.0 + 7.0 * thread)
L2 -= (nose_w + cheek_w) * thread * 2.0

# ---------------- age spots (solar lentigines) + a few seborrheic keratoses ----------------
warp = np.stack([fbm(P * 9.0, 2, seed=s_) for s_ in (201, 202, 203)], 1) - 0.5
F1, F2, ID = worley(P * 13.0 + warp * 1.1, seed=5)    # ~8 mm cells, warped so spots are not round
sun = np.clip(smoothstep(7.3, 7.8, y) * front + 0.8 * smoothstep(0.45, 0.65, np.abs(x)) * smoothstep(6.8, 7.4, y)
              + 0.7 * cheek_w + 0.4 * smoothstep(6.0, 5.6, y), 0, 1) * head_isl.astype(float)
sun = np.maximum(sun, 0.5 * smoothstep(6.2, 5.6, y))   # neck/chest get them too
edge = (fbm(P * 70.0, 3, seed=8) - 0.5) * 0.45
size = 0.22 + 0.42 * ((ID * 13.7) % 1.0)
spot = smoothstep(size, size * 0.55, F1 + edge) * (ID < 0.11) * sun * (1 - hair)
spot *= 0.55 + 0.45 * fbm(P * 160.0, 2, seed=17)     # mottled inside, not flat discs
seb = smoothstep(0.30, 0.20, F1 + edge * 0.5) * (ID > 0.99) * sun * (1 - hair)
tone = 0.5 + 0.7 * ((ID * 7.3) % 1.0)
L2 -= spot * 7.0 * tone + seb * 15.0; A2 += spot * 1.2 + seb * 2.0; B2 += spot * 5.0 * tone + seb * 4.0
# fine freckling/pigment dust
dust = smoothstep(0.75, 0.9, vnoise(P * 220.0, seed=13)) * sun * 0.6
L2 -= dust * 4.0; B2 += dust * 3.0

# ---------------- veins (blue-grey, under thin skin) ----------------
def vein_field(scale, seed, warp=0.35):
    q = P * scale
    wv = np.stack([fbm(q * 0.6, 3, seed=seed + k) for k in range(3)], 1) - 0.5
    f = fbm(q + wv * warp * 6.0, 4, seed=seed + 10)
    return smoothstep(0.86, 0.985, 1 - np.abs(2 * f - 1))
temple = sum(np.exp(-(((x - s * 0.64) ** 2) / 0.02 + ((y - 7.55) ** 2) / 0.05 + ((z - 0.8) ** 2) / 0.12)) for s in (-1, 1))
eyelid = sum(np.exp(-(((x - s * 0.29) ** 2) / 0.02 + ((y - 7.3) ** 2) / 0.012)) * smoothstep(1.0, 1.25, z) for s in (-1, 1))
neckside = smoothstep(6.4, 6.0, y) * smoothstep(0.25, 0.5, np.abs(x)) * smoothstep(5.4, 5.8, y)
chest = smoothstep(5.75, 5.4, y) * front
vis = np.clip(temple * 1.1 + eyelid * 0.8 + neckside * 0.35 + chest * 0.12 + 0.04 * front, 0, 1) * (1 - hair)
veins = np.maximum(vein_field(9.0, 31) * 1.0, vein_field(22.0, 41) * 0.5) * vis
veins = gauss(img(veins), 1.6 * RES / 2048)[ys, xs, 0]          # under the skin: soft-edged, never a drawn line
L2 -= veins * 5.0; A2 -= veins * 2.5; B2 -= veins * 6.5

# ---------------- lips: desaturated, mauve, thin ----------------
lipm = smoothstep(4.0, 14.0, A) * np.exp(-(d3(MOUTH_C) ** 2) / 0.12) * head_isl
_mz = np.exp(-(d3(MOUTH_C) ** 2) / 0.15) * head_isl
_Lb = gauss(img(L0), 4.0 * RES / 2048)[ys, xs, 0]
_seam = smoothstep(4.0, 10.0, L0 - _Lb) * _mz
L2 = L2 - (L0 - _Lb) * _seam
L0 = L0 * (1 - _seam) + _Lb * _seam
A2 = A2 * (1 - lipm) + lipm * (A * 0.55 + 1.0)
B2 = B2 * (1 - lipm) + lipm * (B * 0.25 - 1.0)
L2 = L2 * (1 - lipm) + lipm * (L0 + 2.0)

# ---------------- v2: scalp under thin hair (the hair itself is drawn by shells) ----------------
# The photo's scalp carries a dark stubble cap; replace it with pale scalp skin (a little pinker and
# less yellow than the face) and keep only a faint trace of its fine band. A few painted strands
# near the hairline bridge the shells into the skin.
sweep = np.stack([P[:, 0] * 260.0, P[:, 1] * 260.0 + fbm(P * 8.0, 2, seed=52) * 6.0, P[:, 2] * 30.0], 1)
strand = fbm(sweep, 3, seed=51)
scalpL = 71.0 + 0.25 * Lhi + 2.0 * (fbm(P * 9.0, 2, seed=55) - 0.5)
scalpA = 7.5 + 2.0 * (fbm(P * 5.0, 2, seed=56) - 0.5); scalpB = 9.0
# density of the hair (maskB.A for the shells): thin everywhere, thinner at the crown and the part,
# a soft frontal edge with wisps; zero on the face.
dens_edge = smoothstep(-0.10, 0.05, hairline + 0.05 * (fbm(P * 25.0, 2, seed=57) - 0.5))
front_w = smoothstep(0.55, 0.9, z) * smoothstep(0.62, 0.40, np.abs(x))      # the forehead hairline only
dens_edge = dens_edge * front_w + (1 - front_w) * smoothstep(0.15, 0.6, hair)
dens = hair * dens_edge * (0.62 + 0.30 * (fbm(P * 7.0, 3, seed=58) - 0.5))
crown = np.exp(-((x / 0.35) ** 2) - ((y - 8.45) / 0.3) ** 2 - ((z - 0.45) / 0.45) ** 2)
dens *= 1.0 - 0.35 * crown
hair_dens = np.clip(dens, 0, 1)
sc_w = np.clip(hair * smoothstep(0.02, 0.25, hair_dens) + hair * (1 - front_w) * 0.0, 0, 1)
L2 = L2 * (1 - sc_w) + sc_w * scalpL; A2 = A2 * (1 - sc_w) + sc_w * scalpA; B2 = B2 * (1 - sc_w) + sc_w * scalpB
# where the photo's stubble cap was but no hair grows now (forehead edge): its fine band halved
L2 -= hair * (1 - sc_w) * 0.5 * np.clip(Lhi, -30, 0)

paint = smoothstep(0.55, 0.8, strand) * smoothstep(0.05, 0.3, hair_dens)
L2 += paint * 8.0; A2 -= paint * 3.0; B2 -= paint * 2.0

# ---------------- v2: perioral and chin, de-bearded ----------------
# v1 read as an old MAN: the photo's perioral band was ~10 L* darker than the cheek and lilac
# (a* ~ b*), i.e. a five-o'clock shadow. Pull its low band to the cheek's level and hue.
pz = np.exp(-((x / 0.42) ** 2) - ((y - 6.5) / 0.36) ** 2) * smoothstep(1.0, 1.35, z) * head_isl * (1 - lipm)
cheek_sel = (np.abs(np.abs(x) - 0.45) < 0.12) & (np.abs(y - 6.95) < 0.12) & (z > 1.05) & head_isl
Lc, Ac, Bc = np.median(L2[cheek_sel]), np.median(A2[cheek_sel]), np.median(B2[cheek_sel])
Lf = gauss(img(L2), 14.0 * RES / 2048)[ys, xs, 0]; Af = gauss(img(A2), 14.0 * RES / 2048)[ys, xs, 0]; Bf = gauss(img(B2), 14.0 * RES / 2048)[ys, xs, 0]
L2 += pz * np.clip(Lc - 2.0 - Lf, 0, 14); A2 -= pz * np.clip(Af - Ac, 0, 6); B2 += pz * np.clip(Bc - Bf, 0, 8)
L2 -= pz * 0.45 * np.clip(Lhi, -30, 0)          # stubble dots: halve the dark fine band
print('cheek Lab', round(Lc, 1), round(Ac, 1), round(Bc, 1))
# upper lids and brows: the photo's lids carry baked shadow ("eyeshadow") and its brows are dark;
# pull the lid band toward the cheek (a little darker, pinker), and grey/thin the painted brows
for sd, c in EYE.items():
    q = P - c
    upl = np.exp(-((q[:, 0] / 0.2) ** 2) - ((q[:, 1] - 0.12) / 0.075) ** 2) * smoothstep(0.95, 1.2, z) * head_isl * (1 - socket_isl)
    L2 += upl * np.clip(Lc - 5.0 - Lf, 0, 16); A2 += upl * 1.5
    brz = np.exp(-((q[:, 0] / 0.24) ** 2) - ((q[:, 1] - 0.26) / 0.06) ** 2) * head_isl
    L2 -= brz * 0.6 * np.clip(Lhi, -40, 0); L2 += brz * np.clip(Lc - 3.0 - Lf, 0, 10) * 0.7
# a touch more sallow overall, a touch less pink on the jaw (old pale skin, not flushed)
B2 += 1.5 * head_isl; A2 -= 1.2 * smoothstep(6.7, 6.2, y) * smoothstep(5.9, 6.2, y) * head_isl
A2 += 3.0 * smoothstep(6.1, 5.7, y)

# ---------------- inner eyelid sockets / mouth interior ----------------
# the lid rims and the caruncle: wet pink conjunctiva (old lids sag a little and show it)
L2 = np.where(socket_isl, L0 * 0.45 + 30, L2); A2 = np.where(socket_isl, 17.0 + A * 0.2, A2); B2 = np.where(socket_isl, 9.0 + B * 0.15, B2)
L2 = np.where(mouth_isl, L0 * 0.45, L2); A2 = np.where(mouth_isl, A * 0.5, A2); B2 = np.where(mouth_isl, B * 0.4, B2)

if VARIANT == 'wrong':
    # subtly wrong: bluish-grey cyanotic lips and nail-bed tone, a greenish sallow cast under the eyes,
    # skin a hair too translucent-white over the cheekbones (waxy)
    A2 = A2 - lipm * 4.0; B2 = B2 - lipm * 7.0
    for s, c in EYE.items():
        q = P - c
        under = np.exp(-((q[:, 0] * 0.9) ** 2 / 0.03 + (q[:, 1] + 0.2) ** 2 / 0.01)) * smoothstep(0.9, 1.15, z)
        A2 -= under * 3.0; B2 += under * 2.0
    L2 += cheek_w * 3.0 * (1 - thread)
    # livedo reticularis: a faint violet lace under the jaw and down the neck (cold, failing
    # circulation; also what a body does). Cell borders of a warped Worley field.
    lw = np.stack([fbm(P * 4.0, 2, seed=s_) for s_ in (301, 302, 303)], 1) - 0.5
    lF1, lF2, _ = worley(P * 7.0 + lw * 0.8, seed=305)
    lace = smoothstep(0.10, 0.0, lF2 - lF1) * (0.5 + 0.5 * fbm(P * 3.0, 2, seed=306))
    lzone = smoothstep(6.55, 6.1, y) * smoothstep(5.2, 5.6, y) + 0.5 * smoothstep(6.9, 6.4, y) * smoothstep(0.8, 0.45, z) * head_isl
    lace *= np.clip(lzone, 0, 1)
    L2 -= lace * 5.0; A2 += lace * 3.0; B2 -= lace * 7.0

albedo = lab_to_rgb(np.stack([L2, A2, B2], 1))
alb = dilate(img(albedo, 3), r['mask'], 24)
Image.fromarray((np.clip(alb, 0, 1) * 255 + 0.5).astype(np.uint8)).save(os.path.join(OUT, f'albedo_{VARIANT}.png'))

# ---------------- height (mm) ----------------
lum = img(L0)[..., 0]
hp = (lum - gauss(lum, 5.0 * RES / 2048)) * mask
hp_s = gauss(hp, 0.7 * RES / 2048)
h = np.zeros(n)
h += np.clip(hp_s[ys, xs], -12, 12) * 0.010 * (1 - socket_isl) * (1 - mouth_isl)     # photo creases
# pores: denser and deeper on nose and cheeks, faint on lids and lips
pF1, _, pID = worley(P * 95.0, seed=61)         # ~1 mm
pore_depth = 0.035 + 0.05 * nose_w + 0.03 * cheek_w + 0.01 * front
pore_depth *= (1 - lipm) * (1 - 0.7 * eyelid) * (1 - hair)
h -= smoothstep(0.38, 0.05, pF1) * pore_depth * (0.6 + 0.8 * pID)
# crepe: fine crossing lines of old thin skin
def lines(dirv, f, seed):
    dirv = np.array(dirv) / np.linalg.norm(dirv)
    t = P @ dirv * f + 3.0 * fbm(P * 18.0, 2, seed=seed)
    return 1 - np.abs(np.sin(t))
crepe = (lines([1, 1, 0.3], 260, 71) ** 6 + lines([1, -1, 0.3], 260, 72) ** 6) * 0.5
crepe_w = 0.02 + 0.03 * eyelid + 0.02 * smoothstep(6.4, 5.8, y) + 0.015 * cheek_w
h -= crepe * crepe_w * (1 - lipm) * (1 - hair)
# wrinkle families
def crease(t, width):  # t: signed coordinate across the crease family; returns 0..1 valley
    return np.exp(-(np.sin(t) ** 2) / width)
forehead = smoothstep(7.5, 7.65, y) * smoothstep(8.0, 7.8, y) * smoothstep(0.7, 1.05, z)
brk = lambda sd: smoothstep(0.35, 0.7, fbm(P * np.array([9.0, 2.0, 9.0]), 3, seed=sd))   # wrinkles come and go along their length
h -= crease((y + 0.06 * fbm(P * 8, 2, seed=81)) * 52.0, 0.035) * forehead * 0.085 * (0.25 + 0.75 * brk(82))
glab = np.exp(-((np.abs(x) - 0.06) ** 2) / 0.0012 - (y - 7.5) ** 2 / 0.006) * smoothstep(1.1, 1.3, z)
h -= glab * 0.15
for s, c in EYE.items():
    oc = c + np.array([s * 0.17, -0.02, -0.08])            # outer canthus
    q = P - oc; ang = np.arctan2(q[:, 1], q[:, 0] * s); rad = np.linalg.norm(q, axis=1)
    fan = smoothstep(0.04, 0.1, rad) * smoothstep(0.42, 0.18, rad) * smoothstep(0.2, 0.55, np.cos(ang)) * smoothstep(0.6, 1.0, z)
    h -= crease(ang * 9.0 + 0.3 * fbm(P * 10, 2, seed=91 + s), 0.05) * fan * 0.07 * (0.3 + 0.7 * brk(92 + s))
    # lower-lid folds (festoons): arcs under the eye
    q2 = P - (c + np.array([0, -0.05, 0])); rr = np.linalg.norm(q2[:, :2] * [1.0, 1.6], axis=1)
    h -= crease(rr * 70.0, 0.05) * smoothstep(0.12, 0.2, rr) * smoothstep(0.38, 0.26, rr) * (q2[:, 1] < 0) * smoothstep(0.9, 1.2, z) * 0.08
# perioral vertical lip lines ("smoker's lines")
lipzone = np.exp(-((x / 0.32) ** 2) - ((y - 6.76) / 0.1) ** 2) * smoothstep(1.3, 1.5, z) * (1 - lipm * 0.3)
lipzone += np.exp(-((x / 0.3) ** 2) - ((y - 6.5) / 0.07) ** 2) * smoothstep(1.3, 1.5, z) * 0.6
h -= crease(x * 95.0 + 0.4 * fbm(P * 12, 2, seed=101), 0.05) * lipzone * 0.09 * (0.3 + 0.7 * brk(102))
# lips: vertical fissures
h -= crease(x * 160.0 + 0.6 * fbm(P * 20, 2, seed=111), 0.08) * lipm * 0.06
# neck rings
neck = smoothstep(6.35, 6.15, y) * smoothstep(5.6, 5.8, y)
h -= crease((y + 0.05 * fbm(P * 6, 2, seed=121)) * 30.0, 0.07) * neck * 0.08 * (0.3 + 0.7 * brk(122))
# seborrheic keratoses are raised
h += seb * 0.10 + spot * 0.005
# veins at the temples stand up slightly
h += veins * temple * 0.05

H = dilate(img(h), r['mask'], 24)[..., 0]
# normal from height: slopes in mm per mm using world texel size
Pimg = r['P']; Ti = r['T']; Bi = r['B']
dPdu = np.linalg.norm(np.roll(Pimg, -1, 1) - Pimg, axis=2) * 100.0   # dm -> mm
dPdv = np.linalg.norm(np.roll(Pimg, -1, 0) - Pimg, axis=2) * 100.0
med_u = np.median(dPdu[mask & (dPdu < 5)]); med_v = np.median(dPdv[mask & (dPdv < 5)])
du_mm = np.where(mask & (dPdu < 5 * med_u) & (dPdu > 0), dPdu, med_u)
dv_mm = np.where(mask & (dPdv < 5 * med_v) & (dPdv > 0), dPdv, med_v)
du_mm = gauss(du_mm, 3); dv_mm = gauss(dv_mm, 3)
print('texel size mm (median)', med_u, med_v)
Hs = gauss(H, 0.6)
gx = (np.roll(Hs, -1, 1) - np.roll(Hs, 1, 1)) / (2 * du_mm)
gy = (np.roll(Hs, -1, 0) - np.roll(Hs, 1, 0)) / (2 * dv_mm)
nrm = np.stack([-gx, -gy, np.ones_like(gx)], -1)
nrm /= np.linalg.norm(nrm, axis=2, keepdims=True)
Image.fromarray(((nrm * 0.5 + 0.5) * 255 + 0.5).astype(np.uint8)).save(os.path.join(OUT, 'normal.png'))

# ---------------- masks ----------------
# dose from the source in the pocket: inverse square, self-shielding by the body (surfaces facing away get less)
to_src = POCKET - P; dist = np.linalg.norm(to_src, axis=1); facing = np.clip((N * to_src).sum(1) / dist, -1, 1)
dose = (1.0 / dist ** 2) * (0.25 + 0.75 * smoothstep(-0.4, 0.6, facing))
# irregular front: dose modulated by low-frequency noise (sweat, collar contact, skin folds)
dose *= np.exp((fbm(P * 5.0, 4, seed=131) - 0.5) * 1.4)
logd = np.log(dose)
# normalise so the collar base ~0.95 and the cheek ~0.25 at most
ref_hi = np.log(1.0 / 0.9 ** 2); ref_lo = np.log(1.0 / 3.0 ** 2)
doseN = np.clip((logd - ref_lo) / (ref_hi - ref_lo), 0, 1)
cav = np.clip(0.5 + (H - gauss(H, 3.0)) [ys, xs] * 3.0, 0, 1)
oil = np.clip(0.25 + 0.55 * nose_w + 0.35 * np.exp(-(x ** 2) / 0.06 - (y - 7.7) ** 2 / 0.05) * front + 0.3 * lipm, 0, 1)
maskA = np.stack([doseN, np.clip(veins, 0, 1), oil, cav], 1)
Image.fromarray((np.clip(dilate(img(maskA, 4), r['mask'], 24), 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(OUT, 'maskA.png'))
fF1, _, fID = worley(P * 40.0, seed=141)
flakes = np.clip((fID * 0.7 + 0.3 * fbm(P * 60, 2, seed=151)), 0, 1)
pF, _, pI = worley(P * 55.0, seed=161)
pet = smoothstep(0.22, 0.08, pF) * (pI < 0.2)
lidwet = np.clip(socket_isl * 1.0 + lipm * 0.6, 0, 1)
maskB = np.stack([flakes, pet, lidwet, hair_dens], 1)
Image.fromarray((np.clip(dilate(img(maskB, 4), r['mask'], 24), 0, 1) * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(OUT, 'maskB.png'))
print('done', VARIANT, 'dose collar/cheek', np.percentile(doseN[y < 5.6], 90), np.percentile(doseN[(y > 6.8) & (z > 1)], 90))
