"""Build the Matron's head for the elite-technique probes, from CC0 MakeHuman data.

Own code (numpy only, no MakeHuman code, which is AGPL). Inputs are CC0 data
files (each file's header says so). Run: python3 -I build_head.py DL_DIR OUT_DIR

Outputs (raw little-endian arrays + meta.json) for a code-built ArrayMesh:
  LOD0: one Catmull-Clark level over the hm08 head and neck (hero close-up)
  LOD1: the hm08 head and neck as is
  LOD2: a coarse clustered decimation of LOD1 (far)
Per vertex: position, normal, uv, color (R curvature 1/cm, G baked AO,
B thickness (transmission), A zone id / 255), 3-bone neck weights, and
blend-shape deltas (blink, wide eyes, jaw open, lips back, smile, brow).
"""
import json
import os
import sys

import numpy as np

DL, OUT = sys.argv[1], sys.argv[2]
os.makedirs(OUT, exist_ok=True)
SCALE = 0.1  # MakeHuman decimeters -> meters

# ---------------------------------------------------------------- OBJ
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
V = np.array(V, dtype=np.float64)
VT = np.array(VT, dtype=np.float64)


def load_target(name):
    d = np.zeros_like(V)
    for line in open(os.path.join(DL, name)):
        if line[:1].isdigit():
            p = line.split()
            d[int(p[0])] += [float(p[1]), float(p[2]), float(p[3])]
    return d


# Her identity: an old, thin woman, hollow cheeks, a hooked nose, a down-turned
# mouth, sunken eyes. Weights are a starting point for the art pass.
IDENTITY = [
    ("cfo.target", 1.0),
    ("targets/universal-female-old-minmuscle-minweight.target", 1.0),
    ("targets/head-age-incr.target", 1.0),
    ("targets/l-cheek-volume-decr.target", 0.9),
    ("targets/r-cheek-volume-decr.target", 1.0),
    ("targets/l-cheek-bones-incr.target", 0.5),
    ("targets/r-cheek-bones-incr.target", 0.45),
    ("targets/l-eye-bag-incr.target", 0.8),
    ("targets/r-eye-bag-incr.target", 0.7),
    ("targets/l-eye-push1-in.target", 0.6),
    ("targets/r-eye-push1-in.target", 0.5),
    ("targets/mouth-angles-down.target", 0.6),
    ("targets/nose-hump-incr.target", 0.7),
    ("targets/mouth-laugh-lines-out.target", 0.8),
    ("targets/mouth-upperlip-volume-decr.target", 0.6),
    ("targets/mouth-lowerlip-volume-decr.target", 0.5),
    ("targets/neck-platysma.target", 0.5),
]
for name, w in IDENTITY:
    V += w * load_target(name)

SHAPES = {
    "blink_l": "targets/eye-left-closure.target",
    "blink_r": "targets/eye-right-closure.target",
    "wide_l": "targets/eye-left-opened-up.target",
    "wide_r": "targets/eye-right-opened-up.target",
    "jaw_open": "targets/mouth-open.target",
    "lips_back": "targets/mouth-retraction.target",
    "smile": "targets/mouth-corner-puller.target",
    "brow_l": "targets/eyebrows-left-inner-up.target",
    "brow_r": "targets/eyebrows-right-inner-up.target",
}
DELTA = {k: load_target(p) for k, p in SHAPES.items()}


def joint(name):
    idx = sorted({i for f, g in zip(F, G) if g == name for i in f})
    return V[idx].mean(axis=0)


J = {n: joint("joint-" + n) for n in ["head", "head-2", "neck", "jaw", "l-eye", "r-eye"]}

# ---------------------------------------------------------------- cut
body = [i for i, g in enumerate(G) if g == "body"]
ys = V[np.unique(np.concatenate([F[i] for i in body])), 1]
ycut = J["neck"][1] - 0.35  # a little below the neck joint (collar line)
keep = [i for i in body if all(V[v, 1] > ycut for v in F[i])]
mouth = [i for i, g in enumerate(G) if g in ("helper-upper-teeth", "helper-lower-teeth", "helper-tongue")]
print("head faces", len(keep), "mouth faces", len(mouth), "ycut", ycut)


def submesh(face_ids):
    used = sorted({v for i in face_ids for v in F[i]})
    remap = {v: k for k, v in enumerate(used)}
    faces = [[remap[v] for v in F[i]] for i in face_ids]
    fuv = [[VT[t] for t in FT[i]] for i in face_ids]
    return np.array(used), faces, fuv


# ---------------------------------------------------------------- Catmull-Clark as a matrix
def cc_matrix(nv, faces):
    """Return (rows, faces2) where rows[k] = {old_vertex: weight} for each new
    vertex, and faces2 = new quads (as new-vertex ids) with the parent corner
    layout so UVs can follow. Handles open boundaries (the neck cut)."""
    edges = {}
    vfaces = [[] for _ in range(nv)]
    for fi, f in enumerate(faces):
        n = len(f)
        for k in range(n):
            a, b = f[k], f[(k + 1) % n]
            edges.setdefault((min(a, b), max(a, b)), []).append(fi)
            vfaces[a].append(fi)
    vedges = [[] for _ in range(nv)]
    for e in edges:
        vedges[e[0]].append(e)
        vedges[e[1]].append(e)
    rows = []

    def add(d, v, w):
        d[v] = d.get(v, 0.0) + w

    # face points
    fp = []
    for f in faces:
        d = {}
        for v in f:
            add(d, v, 1.0 / len(f))
        fp.append(len(rows))
        rows.append(d)
    # edge points
    ep = {}
    for e, fs in edges.items():
        d = {}
        if len(fs) == 2:
            add(d, e[0], 0.25)
            add(d, e[1], 0.25)
            for fi in fs:
                for v, w in rows[fp[fi]].items():
                    add(d, v, 0.25 * w)
        else:
            add(d, e[0], 0.5)
            add(d, e[1], 0.5)
        ep[e] = len(rows)
        rows.append(d)
    # vertex points
    vp = []
    for v in range(nv):
        d = {}
        bnd = [e for e in vedges[v] if len(edges[e]) == 1]
        if bnd:
            add(d, v, 0.75)
            for e in bnd[:2]:
                o = e[0] if e[1] == v else e[1]
                add(d, o, 0.125)
        else:
            n = len(vfaces[v])
            add(d, v, (n - 3.0) / n)
            for fi in vfaces[v]:
                for u, w in rows[fp[fi]].items():
                    add(d, u, w / (n * n))
            for e in vedges[v]:
                add(d, e[0], 1.0 / (n * n))
                add(d, e[1], 1.0 / (n * n))
        vp.append(len(rows))
        rows.append(d)
    faces2, corners = [], []
    for fi, f in enumerate(faces):
        n = len(f)
        for k in range(n):
            a, b, c = f[(k - 1) % n], f[k], f[(k + 1) % n]
            q = [vp[b], ep[(min(b, c), max(b, c))], fp[fi], ep[(min(a, b), max(a, b))]]
            faces2.append(q)
            corners.append((fi, k))
    return rows, faces2, corners


class Sparse:
    def __init__(self, rows):
        r, c, w = [], [], []
        for i, d in enumerate(rows):
            for v, x in d.items():
                r.append(i); c.append(v); w.append(x)
        self.r, self.c, self.w, self.n = np.array(r), np.array(c), np.array(w), len(rows)

    def __matmul__(self, X):
        out = np.zeros((self.n,) + X.shape[1:])
        np.add.at(out, self.r, X[self.c] * (self.w if X.ndim == 1 else self.w[:, None]))
        return out


# ---------------------------------------------------------------- geometry helpers
def triangulate(faces):
    t = []
    for f in faces:
        for k in range(1, len(f) - 1):
            t.append([f[0], f[k], f[k + 1]])
    return np.array(t, dtype=np.int64)


def vnormals(P, T):
    n = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])
    N = np.zeros_like(P)
    for k in range(3):
        np.add.at(N, T[:, k], n)
    return N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-12)


def edge_curvature(P, N, T):
    """Mean normal curvature along edges, 1/m, convex positive."""
    e = np.concatenate([T[:, [0, 1]], T[:, [1, 2]], T[:, [2, 0]]])
    dp = P[e[:, 1]] - P[e[:, 0]]
    dn = N[e[:, 1]] - N[e[:, 0]]
    k = (dn * dp).sum(1) / np.maximum((dp * dp).sum(1), 1e-12)
    acc = np.zeros(len(P))
    cnt = np.zeros(len(P))
    for c in (0, 1):
        np.add.at(acc, e[:, c], k)
        np.add.at(cnt, e[:, c], 1)
    return acc / np.maximum(cnt, 1)


def raycast_min(orig, dirs, P, T, max_t):
    """For each (origin, dir) pair return nearest hit distance (or max_t)."""
    v0, v1, v2 = P[T[:, 0]].astype(np.float32), P[T[:, 1]].astype(np.float32), P[T[:, 2]].astype(np.float32)
    e1, e2 = v1 - v0, v2 - v0
    out = np.full(len(orig), max_t, dtype=np.float32)
    CH = 192
    for s in range(0, len(orig), CH):
        o = orig[s:s + CH, None, :].astype(np.float32)
        d = dirs[s:s + CH, None, :].astype(np.float32)
        pv = np.cross(d, e2[None])
        det = (e1[None] * pv).sum(-1)
        inv = 1.0 / np.where(np.abs(det) < 1e-12, 1e-12, det)
        tv = o - v0[None]
        u = (tv * pv).sum(-1) * inv
        qv = np.cross(tv, e1[None])
        v = (d * qv).sum(-1) * inv
        t = (e2[None] * qv).sum(-1) * inv
        ok = (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 2e-4) & (t < max_t)
        t = np.where(ok, t, max_t)
        out[s:s + CH] = t.min(1)
    return out


def hemisphere_dirs(n):
    # fixed Fibonacci directions on the +z hemisphere, cosine weighted
    i = np.arange(n) + 0.5
    r = np.sqrt(i / n)
    phi = i * 2.399963
    return np.stack([r * np.cos(phi), r * np.sin(phi), np.sqrt(1 - r * r)], 1)


def frame(nrm):
    a = np.where(np.abs(nrm[:, 1:2]) < 0.9, np.array([[0, 1, 0]]), np.array([[1, 0, 0]]))
    t = np.cross(a, nrm)
    t /= np.linalg.norm(t, axis=1, keepdims=True)
    b = np.cross(nrm, t)
    return t, b


def bake_ao_thickness(P, N, T):
    t, b = frame(N)
    H = hemisphere_dirs(20)
    occ = np.zeros(len(P))
    for h in H:
        d = t * h[0] + b * h[1] + N * h[2]
        hit = raycast_min(P + N * 3e-4, d, P, T, 0.03)
        occ += np.clip(1.0 - hit / 0.03, 0, 1)  # near hits occlude more
    ao = 1.0 - occ / len(H)
    th = np.zeros(len(P))
    C = hemisphere_dirs(6) * np.array([0.35, 0.35, 1.0])
    for h in C:
        d = -(t * h[0] + b * h[1] + N * h[2])
        d /= np.linalg.norm(d, axis=1, keepdims=True)
        th += raycast_min(P - N * 3e-4, d, P, T, 0.04)
    th /= len(C)
    return ao, th


# ---------------------------------------------------------------- build LOD1 (base) and LOD0 (CC)
used, faces1, fuv1 = submesh(keep)
P1 = V[used] * SCALE
D1 = {k: d[used] * SCALE for k, d in DELTA.items()}
# zones in object space (meters): 1 lips, 2 nose/cheek redness, 3 under-eye blue, 4 scalp, 5 ears
le, re = J["l-eye"] * SCALE, J["r-eye"] * SCALE
print("eyes", le, re, "head joint", J["head"] * SCALE, "neck", J["neck"] * SCALE, "jaw", J["jaw"] * SCALE)

rows, faces0, corners = cc_matrix(len(P1), faces1)
S = Sparse(rows)
P0 = S @ P1
D0 = {k: S @ d for k, d in D1.items()}
fuv0 = []
for (fi, k), q in zip(corners, faces0):
    uv = fuv1[fi]
    n = len(uv)
    a, b, c = uv[(k - 1) % n], uv[k], uv[(k + 1) % n]
    ctr = np.mean(uv, axis=0)
    fuv0.append([b, (b + c) / 2, ctr, (a + b) / 2])


def weld(P, faces, fuv):
    """Split vertices on UV seams. Returns new arrays and the vertex map."""
    key = {}
    src, uvs, tris = [], [], []
    for f, uvf in zip(faces, fuv):
        ids = []
        for v, uv in zip(f, uvf):
            kk = (v, round(float(uv[0]), 5), round(float(uv[1]), 5))
            if kk not in key:
                key[kk] = len(src)
                src.append(v)
                uvs.append(uv)
            ids.append(key[kk])
        for k in range(1, len(ids) - 1):
            tris.append([ids[0], ids[k + 1], ids[k]])  # Godot: clockwise front faces
    return np.array(src), np.array(uvs), np.array(tris, dtype=np.int64)


def neck_weights(P):
    y = P[:, 1]
    y_lo = ycut * SCALE
    y_hi = J["neck"][1] * SCALE
    y_hd = J["head"][1] * SCALE
    def ss(a, b, x):
        t = np.clip((x - a) / (b - a), 0, 1)
        return t * t * (3 - 2 * t)
    w_head = ss(y_hi + 0.25 * (y_hd - y_hi), y_hd, y)
    w_lo = 1.0 - ss(y_lo, y_hi, y)
    w_hi = np.clip(1.0 - w_head - w_lo, 0, 1)
    W = np.stack([w_lo, w_hi, w_head, np.zeros_like(y)], 1)
    return W / W.sum(1, keepdims=True), dict(neck_lo=y_lo, neck_hi=y_hi, head=y_hd)


def zones(P):
    x, y, z = P[:, 0], P[:, 1], P[:, 2]
    zid = np.zeros(len(P))
    mouth_c = (J["jaw"] * SCALE) + np.array([0, 0.012, 0.075])
    eyes_y = le[1]
    zid[(np.abs(x) < 0.025) & (np.abs(y - (eyes_y - 0.065)) < 0.012) & (z > 0.09)] = 1  # lips (refined in shader)
    return zid


def finish(P, faces, fuv, deltas, name, baked):
    src, uv, T = weld(np.arange(len(P)), faces, fuv)
    Pw = P[src]
    Nq = vnormals(P, triangulate(faces))
    curv = edge_curvature(P, Nq, triangulate(faces)) / 100.0  # 1/cm
    ao, th = baked
    Nw = Nq[src]
    W, bone_y = neck_weights(Pw)
    col = np.stack([np.clip(curv[src], 0, 2), ao[src], np.clip(th[src] / 0.04, 0, 1), zones(Pw) / 255.0], 1)
    write(name, Pw, Nw, uv, col, T, W, {k: d[src] for k, d in deltas.items()}, P, faces)
    return bone_y


def write(name, P, N, UV, COL, T, W, deltas, Pshared, faces):
    d = os.path.join(OUT, name)
    os.makedirs(d, exist_ok=True)
    P.astype("<f4").tofile(os.path.join(d, "pos.f32"))
    N.astype("<f4").tofile(os.path.join(d, "nrm.f32"))
    np.stack([UV[:, 0], 1.0 - UV[:, 1]], 1).astype("<f4").tofile(os.path.join(d, "uv.f32"))
    COL.astype("<f4").tofile(os.path.join(d, "col.f32"))
    T.astype("<i4").tofile(os.path.join(d, "idx.i32"))
    np.tile(np.array([0, 1, 2, 0], dtype="<i4"), len(P)).tofile(os.path.join(d, "bones.i32"))
    W.astype("<f4").tofile(os.path.join(d, "weights.f32"))
    names = []
    for k, dl in deltas.items():
        dl.astype("<f4").tofile(os.path.join(d, "bs_%s.f32" % k))
        names.append(k)
    meta = dict(verts=len(P), tris=len(T), shapes=names)
    json.dump(meta, open(os.path.join(d, "meta.json"), "w"), indent=1)
    print(name, meta)


N1 = vnormals(P1, triangulate(faces1))
ao1, th1 = bake_ao_thickness(P1, N1, triangulate(faces1))
print("baked ao range", ao1.min(), ao1.max(), "thickness range", th1.min(), th1.max())
bone_y = finish(P0, faces0, fuv0, D0, "lod0", (S @ ao1, S @ th1))
finish(P1, faces1, fuv1, D1, "lod1", (ao1, th1))

# LOD2: vertex clustering on a 1.4 cm grid (ugly but honest; the far shape)
g = 0.014
cell = np.floor(P1 / g).astype(np.int64)
keys, inv = np.unique(cell, axis=0, return_inverse=True)
inv = inv.reshape(-1)
Pc = np.zeros((len(keys), 3))
np.add.at(Pc, inv, P1)
Pc /= np.bincount(inv)[:, None]
T1 = triangulate([list(reversed(f)) for f in faces1])
Tc = inv[T1]
Tc = Tc[(Tc[:, 0] != Tc[:, 1]) & (Tc[:, 1] != Tc[:, 2]) & (Tc[:, 0] != Tc[:, 2])]
Tc = np.unique(np.sort(Tc, 1) * 0 + Tc, axis=0)
Nc = vnormals(Pc, Tc[:, [0, 2, 1]])
UVc = np.zeros((len(Pc), 2))
cnt = np.bincount(inv)
uvsum = np.zeros((len(Pc), 2))
# rough uv per cluster: average of member vertex first-seen uv
first_uv = np.zeros((len(P1), 2))
for f, uvf in zip(faces1, fuv1):
    for v, uv in zip(f, uvf):
        first_uv[v] = uv
np.add.at(uvsum, inv, first_uv)
UVc = uvsum / cnt[:, None]
Wc, _ = neck_weights(Pc)
COLc = np.stack([np.zeros(len(Pc)), np.ones(len(Pc)), np.full(len(Pc), 1.0), np.zeros(len(Pc))], 1)
write("lod2", Pc, Nc, UVc, COLc, Tc, Wc, {}, Pc, [])

meta = dict(
    eye_l=list(le), eye_r=list(re), jaw=list(J["jaw"] * SCALE), head=list(J["head"] * SCALE),
    neck=list(J["neck"] * SCALE), bones=bone_y,
    license="MakeHuman hm08 base mesh and targets, CC0 (explicitly released Sept 2020; header of each file)",
    source="https://github.com/makehumancommunity/makehuman/tree/master/makehuman/data",
)
json.dump(meta, open(os.path.join(OUT, "head_meta.json"), "w"), indent=1)
print("done")
