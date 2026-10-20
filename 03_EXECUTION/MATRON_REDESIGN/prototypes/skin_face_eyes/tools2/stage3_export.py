# Stage 3: export meshes as raw little-endian arrays for the Godot probe.
#   head: pos f32x3 (m), nrm f32x3, tan f32x4, uv f32x2, col f32x4 (ao, curvature, thinness, 1), idx u32
#   eyes.json: centres/radii; tear strip, eye-occlusion strip, brows, lashes, cap, tunic as small meshes
import sys, os, json, numpy as np
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
from geom import ORIGIN, SCALE
WORK = os.path.join(HERE, '..', 'work2'); OUT = os.path.join(HERE, '..', 'probe2', 'mesh'); os.makedirs(OUT, exist_ok=True)
DL = os.path.join(HERE, '..', 'dl', 'sys')
m = np.load(os.path.join(WORK, 'mesh.npz'))
P_all = m['P']
def to_m(p): return ((p - ORIGIN) * SCALE).astype(np.float32)

def write_mesh(name, pos, nrm, idx, uv=None, tan=None, col=None):
    d = dict(pos=to_m(pos) if pos.dtype != np.float32 or True else pos)
    with open(os.path.join(OUT, name + '.bin'), 'wb') as f:
        hdr = np.array([len(pos), len(idx) * 3, uv is not None, tan is not None, col is not None], np.uint32)
        f.write(hdr.tobytes())
        f.write(np.asarray(pos, np.float32).tobytes()); f.write(np.asarray(nrm, np.float32).tobytes())
        if uv is not None: f.write(np.asarray(uv, np.float32).tobytes())
        if tan is not None: f.write(np.asarray(tan, np.float32).tobytes())
        if col is not None: f.write(np.asarray(col, np.float32).tobytes())
        f.write(np.asarray(idx, np.uint32).reshape(-1).tobytes())

# ---------------- head ----------------
pos = m['pos_dm']; N = m['N']; T = m['T']; w = m['w']; idx = m['idx']; rv = m['rv']
# smooth the per-vertex curvature over the welded graph
uniq, inv = np.unique(rv, return_inverse=True)
wid = inv[idx]
curv_w = np.zeros(len(uniq)); np.add.at(curv_w, inv, m['curv']); cnt = np.bincount(inv); curv_w /= cnt
nb_sum = np.zeros(len(uniq)); nb_cnt = np.zeros(len(uniq))
for _ in range(3):
    nb_sum[:] = 0; nb_cnt[:] = 0
    for a, b in ((0, 1), (1, 2), (2, 0), (1, 0), (2, 1), (0, 2)):
        np.add.at(nb_sum, wid[:, a], curv_w[wid[:, b]]); np.add.at(nb_cnt, wid[:, a], 1)
    curv_w = 0.5 * curv_w + 0.5 * nb_sum / np.maximum(nb_cnt, 1)
k_cm = curv_w[inv] / 10.0                       # 1/dm -> 1/cm
curv01 = np.clip(k_cm / 4.0, 0, 1)              # LUT row: 0 = flat, 1 = r<=2.5 mm
thick_mm = np.minimum(m['thick'], 2.0) * 100.0
thin01 = np.clip(1.0 - (thick_mm - 1.0) / 14.0, 0, 1)
ao = np.clip(m['ao'], 0, 1)
col = np.stack([ao, curv01, thin01, np.ones_like(ao)], 1)
tan = np.concatenate([T, w[:, None]], 1)
write_mesh('head', to_m(pos), N, idx, m['uv'], tan, col)
print('head', len(pos), len(idx), 'curv01 pct', np.percentile(curv01, [10, 50, 90, 99]).round(2), 'thin01 pct', np.percentile(thin01, [50, 90, 99]).round(2))

# ---------------- eyes and lid loops ----------------
eyes = m['eyes']   # rows: cx, cy, cz, r (dm)
# boundary edges on the welded head mesh
from collections import Counter, defaultdict
ec = Counter()
for t in wid:
    for a, b in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])):
        ec[(min(a, b), max(a, b))] += 1
bedges = [e for e, c in ec.items() if c == 1]
adj = defaultdict(list)
for a, b in bedges: adj[a].append(b); adj[b].append(a)
wpos = P_all[uniq]
loops = []
seen = set()
for s in adj:
    if s in seen: continue
    loop = [s]; seen.add(s); prev = None; cur = s
    while True:
        nxt = [q for q in adj[cur] if q != prev and q not in seen]
        if not nxt: break
        prev, cur = cur, nxt[0]; loop.append(cur); seen.add(cur)
    loops.append(loop)
eye_info = []
isl = m['isl']; s0 = set(rv[isl == 0])
seams = [sorted(s0 & set(rv[isl == k])) for k in (3, 4)]
EYE_R = 0.122      # dm: a real eyeball (12.2 mm). MakeHuman's helper eye (15.9 mm) pokes through her aged, sunken lids.
for e in eyes:
    c = e[:3].copy()
    best = min(seams, key=lambda S: np.linalg.norm(P_all[S].mean(0) - c))
    L = P_all[best]
    # slide the eye back along z until every lid-margin point sits >= 0.35 mm outside the eyeball
    rr = EYE_R + 0.0035
    rho2 = (L[:, 0] - c[0]) ** 2 + (L[:, 1] - c[1]) ** 2
    ok = rho2 < rr * rr
    dz = np.min(L[ok, 2] - c[2] - np.sqrt(rr * rr - rho2[ok]))
    c[2] += dz; r = EYE_R
    print('eye moved back by %.2f mm' % (-dz * 100))
    # order the loop by angle around the eye axis (z), starting at the inner corner
    ang = np.arctan2(L[:, 1] - c[1], L[:, 0] - c[0])
    o = np.argsort(ang); L = L[o]
    eye_info.append(dict(centre=to_m(c[None])[0].tolist(), radius=float(r * SCALE), loop=to_m(L).tolist()))
    print('eye', c.round(3), 'r', round(r, 4), 'loop verts', len(best))

def strips(info, inner_mm_up, inner_mm_lo, alpha_up, alpha_lo, lift_mm, name, kind):
    """A strip from the lid margin onto the eyeball surface. kind 'occ' (dark gradient) or 'tear' (lower lid only)."""
    allp = []; alln = []; allc = []; alli = []; base = 0
    for ei in info:
        c = np.array(ei['centre']); R = ei['radius']
        L = np.array(ei['loop'])
        L = np.concatenate([L, L[:1]])
        fwd = np.array([0, 0, 1.0])
        pts = []; cols = []
        for p in L:
            d = p - c; dn = d / np.linalg.norm(d)
            on = c + dn * (R + lift_mm / 1000.0)
            up = p[1] >= c[1]
            if kind == 'tear' and up:
                pass
            # move toward the cornea centre along the sphere
            target = c + fwd * (R + lift_mm / 1000.0)
            inner = inner_mm_up if up else inner_mm_lo
            t = min(1.0, inner / 1000.0 / max(np.linalg.norm(on - target), 1e-6))
            q = on + (target - on) * t
            q = c + (q - c) / np.linalg.norm(q - c) * (R + lift_mm / 1000.0)
            a = alpha_up if up else alpha_lo
            if kind == 'tear':
                # sit at the margin itself, a hair above the eyeball
                pts.append((on + (p - on) * 0.6, q)); cols.append((a, a * 0.6))
            else:
                pts.append((p + (on - p) * 0.3, q)); cols.append((a, 0.0))
        n0 = len(pts)
        for k, ((pa, pb), (ca, cb)) in enumerate(zip(pts, cols)):
            allp += [pa, pb]; dn = (pa - c) / np.linalg.norm(pa - c); alln += [dn, dn]
            allc += [[0, 0, 0, ca], [0, 0, 0, cb]]
        for k in range(n0 - 1):
            if kind == 'tear' and (L[k][1] > c[1] - 0.0015 or L[k + 1][1] > c[1] - 0.0015): continue
            i0 = base + 2 * k
            alli += [[i0, i0 + 1, i0 + 2], [i0 + 1, i0 + 3, i0 + 2]]
        base += 2 * n0
    alli = np.array(alli)
    write_mesh(name, np.array(allp), np.array(alln), alli, col=np.array(allc))
    return len(alli)
print('occ tris', strips(eye_info, 3.2, 1.6, 0.85, 0.6, 0.15, 'eye_occlusion', 'occ'))
print('tear tris', strips(eye_info, 0.9, 0.9, 0.9, 0.9, 0.25, 'tear', 'tear'))
json.dump(dict(eyes=eye_info), open(os.path.join(OUT, 'eyes.json'), 'w'))

# ---------------- MakeHuman proxies (eyebrows, eyelashes) fitted with their .mhclo ----------------
def fit_proxy(objpath, clopath):
    V = []; VT = []; F = []
    for line in open(objpath):
        if line.startswith('v '): V.append([float(x) for x in line.split()[1:4]])
        elif line.startswith('vt '): VT.append([float(x) for x in line.split()[1:3]])
        elif line.startswith('f '): F.append([tuple(int(a) - 1 for a in t.split('/')[:2]) for t in line.split()[1:]])
    sc = {}; ref = []; mode = None; offs = None
    for line in open(clopath):
        p = line.split()
        if not p or p[0].startswith('#'): continue
        if p[0] in ('x_scale', 'y_scale', 'z_scale'):
            a, b, s = int(p[1]), int(p[2]), float(p[3]); ax = 'xyz'.index(p[0][0])
            sc[ax] = abs(P_all[a, ax] - P_all[b, ax]) / s
        elif p[0] == 'verts': mode = 'v'
        elif p[0] in ('delete_verts', 'material', 'obj_file', 'name', 'uuid', 'tag', 'z_depth', 'basemesh', 'max_pole', 'special_pose', 'license', 'author', 'homepage', 'vertexboneweights_file'):
            mode = None if p[0] == 'delete_verts' else mode
        elif mode == 'v' and len(p) == 9:
            ref.append([float(x) for x in p])
        elif mode == 'v' and len(p) == 1:
            ref.append([int(p[0]), int(p[0]), int(p[0]), 1.0, 0.0, 0.0, 0.0, 0.0, 0.0])
    ref = np.array(ref)
    vi = ref[:, :3].astype(int); wts = ref[:, 3:6]; off = ref[:, 6:9]
    pos = (P_all[vi] * wts[:, :, None]).sum(1) + off * np.array([sc[0], sc[1], sc[2]])
    tris = []; tuv = []
    for f in F:
        for j in range(1, len(f) - 1): tris.append((f[0], f[j], f[j + 1]))
    key = {}; rp = []; ru = []; ii = []
    for t in tris:
        row = []
        for v, u in t:
            if (v, u) not in key: key[(v, u)] = len(rp); rp.append(pos[v]); ru.append(VT[u])
            row.append(key[(v, u)])
        ii.append([row[0], row[2], row[1]])
    rp = np.array(rp); ru = np.array(ru); ru[:, 1] = 1 - ru[:, 1]; ii = np.array(ii)
    a, b, c = rp[ii[:, 0]], rp[ii[:, 1]], rp[ii[:, 2]]
    fn = np.cross(c - a, b - a); Nn = np.zeros_like(rp)
    for k in range(3): np.add.at(Nn, ii[:, k], fn)
    Nn /= np.linalg.norm(Nn, axis=1, keepdims=True) + 1e-12
    return rp, Nn, ii, ru
for nm, o, c in (('brows', 'eyebrows/eyebrow010/eyebrow010.obj', 'eyebrows/eyebrow010/eyebrow010.mhclo'),
                 ('lashes', 'eyelashes/eyelashes02/eyelashes02.obj', 'eyelashes/eyelashes02/eyelashes02.mhclo')):
    rp, Nn, ii, ru = fit_proxy(os.path.join(DL, o), os.path.join(DL, c))
    write_mesh(nm, to_m(rp), Nn, ii, ru)
    print(nm, len(rp), len(ii))

# ---------------- cap and tunic shells ----------------
Pw = wpos; Nw_ = np.zeros_like(Pw)
a, b, c = Pw[wid[:, 0]], Pw[wid[:, 1]], Pw[wid[:, 2]]
fn = np.cross(c - a, b - a)
for k in range(3): np.add.at(Nw_, wid[:, k], fn)
Nw_ /= np.linalg.norm(Nw_, axis=1, keepdims=True) + 1e-12
def shell(sel_fn, offset_fn, name):
    keep = np.all(sel_fn(Pw)[wid], axis=1)
    tri = wid[keep]; used = np.unique(tri); remap = -np.ones(len(Pw), int); remap[used] = np.arange(len(used))
    q = Pw[used] + Nw_[used] * offset_fn(Pw[used])[:, None]
    tt = remap[tri]
    write_mesh(name, to_m(q), Nw_[used], tt, uv=(Pw[used][:, [0, 1]] * 0.1).astype(np.float32))
    return len(tt)
# Cap and collar are shrink-wrapped grids (rays cast at the head), so their edges are smooth curves.
from raster import ray_hits
def grid_mesh(Pg, closed_u=True, inside=None):
    R_, C_ = Pg.shape[:2]
    pos = Pg.reshape(-1, 3); tris = []
    cols = C_ if closed_u else C_ - 1
    for r in range(R_ - 1):
        for c in range(cols):
            c2 = (c + 1) % C_
            a, b, cc, d = r * C_ + c, r * C_ + c2, (r + 1) * C_ + c, (r + 1) * C_ + c2
            tris += [[a, cc, b], [b, cc, d]]
    tris = np.array(tris)
    A, B, Cc = pos[tris[:, 0]], pos[tris[:, 1]], pos[tris[:, 2]]
    fn = np.cross(Cc - A, B - A)
    if inside is not None:   # face every triangle away from the inside point/axis
        cen = (A + B + Cc) / 3; out = cen - inside(cen)
        flip = np.sum(fn * out, 1) < 0
        tris[flip] = tris[flip][:, [0, 2, 1]]
        A, B, Cc = pos[tris[:, 0]], pos[tris[:, 1]], pos[tris[:, 2]]
        fn = np.cross(Cc - A, B - A)
    Nn = np.zeros_like(pos)
    for k in range(3): np.add.at(Nn, tris[:, k], fn)
    Nn /= np.linalg.norm(Nn, axis=1, keepdims=True) + 1e-12
    return pos, Nn, tris
def capline(p): return p[..., 1] - (7.85 + (p[..., 2] - 1.05) * 0.55)
C0 = np.array([0.0, 7.6, 0.55])
NU = 64
phis = np.linspace(0, 2 * np.pi, NU, endpoint=False)
# Cap = offset shells of the scalp; the edge is cut per pixel in the shader from the analytic cap line
# (cap_line_* uniforms), so it is a smooth curve whatever the mesh resolution.
def shell2(sel, off, name):
    keep = np.all(sel[wid], axis=1)
    tri = wid[keep]; used = np.unique(tri); remap = -np.ones(len(Pw), int); remap[used] = np.arange(len(used))
    q = Pw[used] + Nw_[used] * off[used][:, None]
    write_mesh(name, to_m(q), Nw_[used], remap[tri], uv=np.zeros((len(used), 2), np.float32))
    return keep.sum()
cl = capline(Pw)
cap_off = 0.045 + 0.09 * np.clip(cl / 0.5, 0, 1) ** 0.7       # cloth stands off the crown
print('cap tris', shell2(cl > -0.2, cap_off, 'cap'))
band_off = cap_off + 0.035
print('band tris', shell2((cl > -0.2) & (cl < 0.45), band_off, 'cap_band'))
# stand-up collar of the tunic: rays out from the neck axis
collar_top = lambda ph: 5.78 - 0.18 * np.exp(-((np.mod(ph + np.pi, 2 * np.pi) - np.pi) / 0.55) ** 2)   # dips at the front (phi=0 -> +z)
NY = 16
rows = []
t_last = None
for j in range(NY + 1):
    f = j / NY
    yy = collar_top(phis) * (1 - f) + 5.05 * f
    o = np.stack([np.zeros(NU), yy, np.full(NU, 0.35)], -1)
    d = np.stack([np.sin(phis), np.zeros(NU), np.cos(phis)], -1)
    t = ray_hits(o, d, Pw, wid, far=False)
    if t_last is not None: t = np.where(np.isfinite(t), t, t_last)
    t = np.where(np.isfinite(t), t, 0.8); t_last = t
    rows.append(o + d * (t[:, None] + 0.06 + 0.05 * f))
col = np.array(rows)
qp, qn, qt = grid_mesh(col, inside=lambda q: np.stack([np.zeros(len(q)), q[:, 1], np.full(len(q), 0.35)], 1))
write_mesh('tunic', to_m(qp), qn, qt, uv=np.zeros((len(qp), 2), np.float32))
print('collar tris', len(qt))

# ---------------- blend shapes (MakeHuman CC0 expression units), deltas in metres ----------------
from mhload import target as _tgt
nV = len(P_all)
pos_r = m['pos_dm']
def side(sgn, w=0.06):
    return 1.0 / (1.0 + np.exp(-sgn * pos_r[:, 0] / w * 4.0))
SHAPES = [('blink_l', 'eye-left-closure', None), ('blink_r', 'eye-right-closure', None),
          ('wide_l', 'eye-left-opened-up', None), ('wide_r', 'eye-right-opened-up', None),
          ('slit_l', 'eye-left-slit', None), ('slit_r', 'eye-right-slit', None),
          ('brow_in_l', 'eyebrows-left-inner-up', None), ('brow_in_r', 'eyebrows-right-inner-up', None),
          ('smile_l', 'mouth-corner-puller', +1), ('smile_r', 'mouth-corner-puller', -1),
          ('jaw_open', 'mouth-open', None), ('lips_part', 'mouth-parling', None), ('neck_cords', 'neck-platysma', None)]
with open(os.path.join(OUT, 'head_shapes.bin'), 'wb') as f:
    f.write(np.array([len(SHAPES), len(rv)], np.uint32).tobytes())
    for nm, tg, sd in SHAPES:
        d = _tgt(nV, tg)[rv] * SCALE
        if sd is not None: d = d * side(sd)[:, None]
        f.write(nm.encode().ljust(32, b'\0')); f.write(d.astype(np.float32).tobytes())
        print('shape', nm, 'moved verts', int((np.abs(d).sum(1) > 0).sum()))

# ---------------- stray grey hairs escaping the cap (ribbons; a few clumps at temples and nape) ----------------
rng = np.random.default_rng(11)
clv = capline(Pw)
cand = np.where((clv > -0.07) & (clv < -0.01) & ((np.abs(Pw[:, 0]) > 0.45) | (Pw[:, 2] < 0.4)))[0]
roots = []
for _ in range(6):                                   # clumps
    c0 = cand[rng.integers(len(cand))]
    for _ in range(rng.integers(3, 7)):
        roots.append(Pw[c0] + rng.normal(size=3) * 0.02)
from numpy.linalg import norm
def nearest(p):
    i = np.argmin(norm(Pw - p, axis=1)); return Pw[i], Nw_[i]
hp = []; hu = []; hi = []
for p0 in roots:
    v, n = nearest(p0); p = v + n * 0.012
    g = np.array([0, -1.0, 0]); d = g - n * (g @ n); d /= norm(d) + 1e-9
    d = d + rng.normal(size=3) * 0.3; d /= norm(d)
    L = rng.uniform(0.35, 0.9); segs = 12; step = L / segs; width = rng.uniform(0.004, 0.007)
    pts = [p]
    for k in range(segs):
        v, n = nearest(p)
        gg = g - n * (g @ n)
        d = d * 0.75 + gg * 0.25 + rng.normal(size=3) * 0.12; d /= norm(d)
        p = p + d * step
        v, n = nearest(p)
        h = (p - v) @ n
        if h < 0.008: p = p + n * (0.008 - h)
        pts.append(p)
    base = len(hp)
    for k, q in enumerate(pts):
        v, n = nearest(q)
        tdir = pts[min(k + 1, segs)] - pts[max(k - 1, 0)]; tdir /= norm(tdir) + 1e-9
        side = np.cross(tdir, n); side /= norm(side) + 1e-9
        w = width * (1.0 - 0.6 * k / segs)
        hp += [q - side * w, q + side * w]; hu += [[0, k / segs], [1, k / segs]]
    for k in range(segs):
        a = base + 2 * k
        hi += [[a, a + 2, a + 1], [a + 1, a + 2, a + 3]]
hp = np.array(hp); hi = np.array(hi)
A_, B_, C_ = hp[hi[:, 0]], hp[hi[:, 1]], hp[hi[:, 2]]
fn = np.cross(C_ - A_, B_ - A_); hn = np.zeros_like(hp)
for k in range(3): np.add.at(hn, hi[:, k], fn)
hn /= norm(hn, axis=1, keepdims=True) + 1e-12
write_mesh('strays', to_m(hp), hn, hi, uv=np.array(hu, np.float32))
print('stray hairs', len(roots), 'tris', len(hi))


# ---------------- v2: hair shells (thin white hair combed back to a bun) ----------------
# K offset copies of the scalp with the head's own UVs: the shader reads the density from maskB.A,
# so the hairline, the thin crown and the part are painted in one place (stage2).
from PIL import Image as _Image
mB = np.asarray(_Image.open(os.path.join(HERE, '..', 'probe2', 'tex', 'maskB.png')).convert('RGBA')).astype(np.float32) / 255.0
Rz = mB.shape[0]
uvh = m['uv']
px = np.clip((uvh[:, 0] * Rz).astype(int), 0, Rz - 1); py = np.clip((uvh[:, 1] * Rz).astype(int), 0, Rz - 1)
vd = mB[py, px, 3]
keep = vd[idx].max(1) > 0.02
tri = idx[keep]; used = np.unique(tri); remap = -np.ones(len(pos), int); remap[used] = np.arange(len(used))
tri = remap[tri]
Pv = pos[used]; Nv = N[used]; Uv = uvh[used]
K = 10
yv, zv = Pv[:, 1], Pv[:, 2]
thick = 0.018 + 0.010 * np.clip((yv - 7.9) / 0.5, 0, 1) + 0.008 * np.clip((0.6 - zv) / 0.8, 0, 1)   # dm: 1.8 mm at the hairline, ~3 mm on the crown (combed flat)
sp = []; sn = []; su = []; sc = []; si = []
for k in range(K):
    f = (k + 0.5) / K
    sp.append(Pv + Nv * (thick * f)[:, None] + Nv * 0.002); sn.append(Nv); su.append(Uv)
    sc.append(np.stack([np.full(len(Pv), f), thick / 0.05, np.zeros(len(Pv)), np.ones(len(Pv))], 1))
    si.append(tri + k * len(Pv))
write_mesh('hair_shells', to_m(np.concatenate(sp)), np.concatenate(sn), np.concatenate(si), uv=np.concatenate(su), col=np.concatenate(sc))
print('hair shells', K, 'layers', len(tri), 'tris each')
# bun: the back of the head, a little above the occiput
back = (np.abs(Pw[:, 0]) < 0.05) & (np.abs(Pw[:, 1] - 7.85) < 0.08)
bi = np.argmin(np.where(back, Pw[:, 2], 9.0))
bc = Pw[bi] + Nw_[bi] * 0.12
bax = Nw_[bi] / np.linalg.norm(Nw_[bi])
R_ = np.array([0.26, 0.21, 0.17])     # dm: a small, tight bun
st_r, st_s = 14, 24
bp = []; bn = []; bcol = []; bidx = []
upv = np.array([0, 1.0, 0]); e1 = np.cross(upv, bax); e1 /= np.linalg.norm(e1); e2 = np.cross(bax, e1)
for k in range(4):
    f = (k + 0.5) / 4; base = len(bp)
    for i in range(st_r + 1):
        th = np.pi * i / st_r
        for j in range(st_s + 1):
            ph = 2 * np.pi * j / st_s
            d = np.array([np.sin(th) * np.cos(ph), np.sin(th) * np.sin(ph), np.cos(th)])
            loc = d * (R_ * (0.86 + 0.14 * f))
            q = bc + e1 * loc[0] + e2 * loc[1] + bax * loc[2] * (0.6 if loc[2] < 0 else 1.0)
            bp.append(q); bn.append(e1 * d[0] + e2 * d[1] + bax * d[2]); bcol.append([f, 0.6, 1.0, 1.0])
    for i in range(st_r):
        for j in range(st_s):
            a = base + i * (st_s + 1) + j; b2 = a + st_s + 1
            bidx += [[a, a + 1, b2], [a + 1, b2 + 1, b2]]
bp = np.array(bp); bn = np.array(bn)
write_mesh('bun', to_m(bp), bn, np.array(bidx), uv=np.zeros((len(bp), 2), np.float32), col=np.array(bcol, np.float32))
json.dump(dict(bun_centre=to_m(bc[None])[0].tolist(), bun_axis=bax.tolist(), head_centre=to_m(np.array([[0.0, 7.75, 0.45]]))[0].tolist()),
          open(os.path.join(OUT, 'hair.json'), 'w'))
print('bun at', bc.round(3))
