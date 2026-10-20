#!/usr/bin/env python3
"""The game's people from MakeHuman's CC0 scanned body: the shared machinery.

Loading MakeHuman's files (our own parsers: the format is read here, none of
MakeHuman's AGPL program code is used), the macro blend (age, sex, muscle,
weight, height), the rig (bones at MakeHuman's joints, named for the views'
pivots), linear-blend skin weights from MakeHuman's CC0 default weights,
clothing generated from the body itself (offset shells, lofts, bands), and a
skinned glTF writer. Each character's builder (build_climber_body.py, and the
Matron's) chooses the body and dresses it.

Sources: CC0 1.0 MakeHuman assets, pinned by commit and SHA-256 in
sources.json, fetched by fetch_makehuman.py.
"""
import hashlib
import json
import math
import os
import struct

import numpy as np

# ---------------------------------------------------------------- loading ---


def sha(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()


def load_obj(path):
    V, VT, faces = [], [], []
    g = None
    for line in open(path):
        if line.startswith("v "):
            V.append([float(x) for x in line.split()[1:4]])
        elif line.startswith("vt "):
            VT.append([float(x) for x in line.split()[1:3]])
        elif line.startswith("g "):
            g = line.split()[1]
        elif line.startswith("f "):
            parts = line.split()[1:]
            faces.append(([int(p.split("/")[0]) - 1 for p in parts],
                          [int(p.split("/")[1]) - 1 for p in parts], g))
    return np.array(V, float), np.array(VT, float), faces


_tcache = {}
DL = ""   # the MakeHuman download dir (set_download_dir)


def set_download_dir(path):
    global DL
    DL = path


def target(rel):
    if rel in _tcache:
        return _tcache[rel]
    path = rel if os.path.isabs(rel) else os.path.join(DL, rel)
    idx, d = [], []
    for line in open(path):
        if line[:1].isdigit():
            p = line.split()
            idx.append(int(p[0]))
            d.append([float(p[1]), float(p[2]), float(p[3])])
    _tcache[rel] = (np.array(idx, int), np.array(d, float))
    return _tcache[rel]


# ------------------------------------------------------- the macro blend ---
# Our own weighting: each macro slider is split piecewise-linearly between the
# two neighbouring categories MakeHuman ships targets for, and a combined
# target's weight is the product of its categories' weights.


def split(v, lo, mid, hi):
    v = min(max(v, 0.0), 1.0)
    if v < 0.5:
        return {lo: 1.0 - 2.0 * v, mid: 2.0 * v}
    return {mid: 2.0 - 2.0 * v, hi: 2.0 * v - 1.0}


def age_split(v):
    # 0.5 = 25 years, 1.0 = 90 years (MakeHuman's age slider convention).
    v = min(max(v, 0.5), 1.0)
    return {"young": 2.0 - 2.0 * v, "old": 2.0 * v - 1.0}


def years(v):
    return 25.0 + (v - 0.5) / 0.5 * 65.0


def macro_terms(p):
    terms = []
    A = age_split(p["age"])
    M = split(p["muscle"], "minmuscle", "averagemuscle", "maxmuscle")
    W = split(p["weight"], "minweight", "averageweight", "maxweight")
    sex = p.get("sex", "female")
    for a, wa in A.items():
        if wa > 1e-6:   # sex, age and race: the shape itself (all caucasian here)
            terms.append(("targets/macrodetails/caucasian-%s-%s.target" % (sex, a), wa))
        for m, wm in M.items():
            for w, ww in W.items():
                k = wa * wm * ww
                if k <= 1e-6:
                    continue
                terms.append(("targets/macrodetails/universal-%s-%s-%s-%s.target" % (sex, a, m, w), k))
                h = p.get("height", 0.5)
                if h > 0.5:
                    terms.append(("targets/macrodetails/height/%s-%s-%s-%s-maxheight.target" % (sex, a, m, w), k * (h - 0.5) * 2.0))
                elif h < 0.5:
                    terms.append(("targets/macrodetails/height/%s-%s-%s-%s-minheight.target" % (sex, a, m, w), k * (0.5 - h) * 2.0))
                if a == "old" and sex == "female":
                    f = p.get("breast_firmness", 0.5)
                    c = p.get("breast_cup", 0.5)
                    if f < 0.5:
                        kf = (0.5 - f) * 2.0
                        cup = "mincup" if c < 0.5 else "averagecup"
                        kc = (0.5 - c) * 2.0 if c < 0.5 else 1.0
                        terms.append(("targets/breast/%s-old-%s-%s-averagecup-minfirmness.target" % (sex, m, w), k * kf * (1.0 - kc if cup == "mincup" else 1.0)))
                        if cup == "mincup":
                            terms.append(("targets/breast/%s-old-%s-%s-mincup-minfirmness.target" % (sex, m, w), k * kf * kc))
    for rel, k in p.get("extra", []):
        terms.append((rel, k))
    return terms


def morph(V0, p):
    V = V0.copy()
    for rel, k in macro_terms(p):
        if k == 0.0:
            continue
        idx, d = target(rel)
        if len(idx):
            V[idx] += k * d
    return V


# ------------------------------------------------------------ geometry ------


def tri_faces(faces):
    out = []
    for vi, ti, g in faces:
        for k in range(1, len(vi) - 1):
            out.append(([vi[0], vi[k], vi[k + 1]], [ti[0], ti[k], ti[k + 1]], g))
    return out


def vertex_normals(V, T):
    T = np.asarray(T)
    n = np.cross(V[T[:, 1]] - V[T[:, 0]], V[T[:, 2]] - V[T[:, 0]])
    N = np.zeros_like(V)
    for k in range(3):
        np.add.at(N, T[:, k], n)
    l = np.linalg.norm(N, axis=1, keepdims=True)
    return N / np.maximum(l, 1e-12)


def neighbours(nv, T):
    T = np.asarray(T)
    e = np.concatenate([T[:, [0, 1]], T[:, [1, 2]], T[:, [2, 0]]])
    e = np.concatenate([e, e[:, ::-1]])
    e = np.unique(e, axis=0)
    return e


def laplacian(V, E, mask=None):
    S = np.zeros_like(V)
    C = np.zeros(len(V))
    np.add.at(S, E[:, 0], V[E[:, 1]])
    np.add.at(C, E[:, 0], 1.0)
    avg = S / np.maximum(C, 1)[:, None]
    L = avg - V
    L[C == 0] = 0
    if mask is not None:
        L[~mask] = 0
    return L


def hull2(P):
    P = np.unique(np.round(P, 6), axis=0)
    if len(P) < 3:
        return P
    P = P[np.lexsort((P[:, 1], P[:, 0]))]

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for p in P:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(tuple(p))
    for p in P[::-1]:
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(tuple(p))
    return np.array(lower[:-1] + upper[:-1])


def perimeter(H):
    if len(H) < 3:
        return 0.0
    return float(np.linalg.norm(H - np.roll(H, 1, axis=0), axis=1).sum())


def girth(V, sel, origin, normal, band=0.004):
    """Tape-measure girth: convex hull of the vertices within `band` of the plane."""
    n = normal / np.linalg.norm(normal)
    P = V[sel]
    d = (P - origin) @ n
    P = P[np.abs(d) < band]
    if len(P) < 6:
        return 0.0
    a = np.cross(n, [1.0, 0.0, 0.0])
    if np.linalg.norm(a) < 0.1:
        a = np.cross(n, [0.0, 0.0, 1.0])
    a /= np.linalg.norm(a)
    b = np.cross(n, a)
    Q = np.stack([(P - origin) @ a, (P - origin) @ b], 1)
    return perimeter(hull2(Q))


def volume(V, T):
    T = np.asarray(T)
    return float(np.einsum("ij,ij->i", V[T[:, 0]], np.cross(V[T[:, 1]], V[T[:, 2]])).sum() / 6.0)


# ------------------------------------------------------------ rotations -----


def frame(y_axis, z_hint):
    y = y_axis / np.linalg.norm(y_axis)
    z = z_hint - y * (z_hint @ y)
    z /= np.linalg.norm(z)
    x = np.cross(y, z)
    return np.stack([x, y, z], 1)   # columns are the local axes in world


def quat_from_m(m):
    t = np.trace(m)
    if t > 0:
        s = math.sqrt(t + 1.0) * 2
        w, x, y, z = 0.25 * s, (m[2, 1] - m[1, 2]) / s, (m[0, 2] - m[2, 0]) / s, (m[1, 0] - m[0, 1]) / s
    elif m[0, 0] > m[1, 1] and m[0, 0] > m[2, 2]:
        s = math.sqrt(1.0 + m[0, 0] - m[1, 1] - m[2, 2]) * 2
        w, x, y, z = (m[2, 1] - m[1, 2]) / s, 0.25 * s, (m[0, 1] + m[1, 0]) / s, (m[0, 2] + m[2, 0]) / s
    elif m[1, 1] > m[2, 2]:
        s = math.sqrt(1.0 + m[1, 1] - m[0, 0] - m[2, 2]) * 2
        w, x, y, z = (m[0, 2] - m[2, 0]) / s, (m[0, 1] + m[1, 0]) / s, 0.25 * s, (m[1, 2] + m[2, 1]) / s
    else:
        s = math.sqrt(1.0 + m[2, 2] - m[0, 0] - m[1, 1]) * 2
        w, x, y, z = (m[1, 0] - m[0, 1]) / s, (m[0, 2] + m[2, 0]) / s, (m[1, 2] + m[2, 1]) / s, 0.25 * s
    q = np.array([x, y, z, w])
    return q / np.linalg.norm(q)


def euler_yxz(m):
    # Godot's default order: basis = Ry * Rx * Rz.  Returns (x, y, z).
    sx = -m[1, 2]
    sx = min(max(sx, -1.0), 1.0)
    x = math.asin(sx)
    if abs(sx) < 0.9999:
        y = math.atan2(m[0, 2], m[2, 2])
        z = math.atan2(m[1, 0], m[1, 1])
    else:
        y = math.atan2(-m[2, 0], m[0, 0])
        z = 0.0
    return [x, y, z]


def rx(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[1, 0, 0], [0, c, -s], [0, s, c]], float)


# ----------------------------------------------------------- the rig --------
# Bones are matron_view.gd's pivots, plus helpers the adapter drives from them:
# three spine bones share the torso pivot's rotation, two neck bones share the
# neck pivot's, and each long bone has a "_x" stretch bone at its far joint so
# the Turn lengthens a limb by sliding (no scale, so the skin is not sheared).

SIDES = {"l": ".R", "r": ".L"}   # pivot "_l" sits at -x, which is MakeHuman's .R


def build_rig(V, J, skel, extras=("hump", "skirt", "box")):
    def jp(name):
        return J[name]
    hipc = 0.5 * (jp("upperleg01.L____head") + jp("upperleg01.R____head"))
    bones = []   # (name, parent, position, frame, mode, pivot)

    def add(name, parent, pos, F, mode, pivot=None, **kw):
        bones.append(dict(name=name, parent=parent, pos=np.array(pos, float), F=F, mode=mode, pivot=pivot, **kw))
    I = np.eye(3)
    body_p = hipc + np.array([0.0, 0.03, 0.0])
    add("body", None, body_p, I, "body", "body")
    add("spine_lo", "body", jp("spine04____head"), I, "spine", "torso", share=0.25)
    add("spine_mid", "spine_lo", jp("spine02____head"), I, "spine", "torso", share=0.15)
    add("chest", "spine_mid", jp("spine01____head"), I, "spine", "torso", share=0.10)
    # pelvis takes the rest (0.5) of the torso's pitch: the lumbo-pelvic rhythm.
    if "hump" in extras:
        back = jp("spine01____head") + np.array([0.0, 0.0, -0.05])
        add("hump", "chest", back, I, "scale", "hump")
    add("neck_lo", "chest", jp("neck01____head"), I, "neck", "neck", share=0.5)
    add("neck_hi", "neck_lo", jp("neck02____head"), I, "neck", "neck", share=0.5)
    add("head", "neck_hi", jp("head____head"), I, "copy", "head")
    add("jaw", "head", jp("jaw____head"), I, "copy", "jaw")
    fwd = np.array([0.0, 0.0, 1.0])
    for s, mh in SIDES.items():
        sh, el, wr = jp("upperarm01%s____head" % mh), jp("lowerarm01%s____head" % mh), jp("wrist%s____head" % mh)
        Fsh = frame(sh - el, fwd)
        Fel = frame(el - wr, fwd)
        add("sh_" + s, "chest", sh, Fsh, "limb", "sh_" + s, length=float(np.linalg.norm(el - sh)))
        add("sh_%s_x" % s, "sh_" + s, el, Fsh, "stretch", "sh_" + s, length=float(np.linalg.norm(el - sh)))
        add("el_" + s, "sh_%s_x" % s, el, Fel, "limb", "el_" + s, length=float(np.linalg.norm(wr - el)))
        add("el_%s_x" % s, "el_" + s, wr, Fel, "stretch", "sh_" + s, length=float(np.linalg.norm(wr - el)))
        mid = jp("finger3-1%s____head" % mh)
        # the palm's normal: out of the palm, the side the fingers curl to
        idx_k, pnk_k = jp("finger2-1%s____head" % mh), jp("finger5-1%s____head" % mh)
        across = idx_k - pnk_k
        along = mid - wr
        n = np.cross(across, along)
        tip = jp("finger3-3%s____tail" % mh)
        if (tip - mid) @ n < 0:      # curled fingertips lean to the palm side
            n = -n
        Fwr = frame(wr - mid, n)
        add("wr_" + s, "el_%s_x" % s, wr, Fwr, "build", "wr_" + s)
        for f, mf in enumerate(["finger2", "finger3", "finger4", "finger5"]):
            parent = "wr_" + s
            for seg in range(3):
                a = jp("%s-%d%s____head" % (mf, seg + 1, mh))
                b = jp("%s-%d%s____tail" % (mf, seg + 1, mh))
                name = "f%d%d_%s" % (f, seg, s)
                add(name, parent, a, frame(a - b, n), "finger", "fingers_%s/%d/%d" % (s, f, seg))
                parent = name
        parent = "wr_" + s
        for seg, mseg in enumerate([2, 3]):
            a = jp("finger1-%d%s____head" % (mseg, mh))
            b = jp("finger1-%d%s____tail" % (mseg, mh))
            name = "t%d_%s" % (seg, s)
            add(name, parent, a, frame(a - b, n), "finger", "fingers_%s/4/%d" % (s, seg))
            parent = name
        hp, kn, an = jp("upperleg01%s____head" % mh), jp("lowerleg01%s____head" % mh), jp("foot%s____head" % mh)
        Fhp = frame(hp - kn, fwd)
        Fkn = frame(kn - an, fwd)
        add("hp_" + s, "body", hp, Fhp, "limb", "hp_" + s, length=float(np.linalg.norm(kn - hp)))
        add("hp_%s_x" % s, "hp_" + s, kn, Fhp, "stretch", "hp_" + s, length=float(np.linalg.norm(kn - hp)))
        add("kn_" + s, "hp_%s_x" % s, kn, Fkn, "limb", "kn_" + s, length=float(np.linalg.norm(an - kn)))
        add("kn_%s_x" % s, "kn_" + s, an, Fkn, "stretch", "hp_" + s, length=float(np.linalg.norm(an - kn)))
        # The ankle pivot is posed at rotation.x = -0.2 always: make that neutral.
        add("an_" + s, "kn_%s_x" % s, an, Fkn @ rx(-0.2), "finger", "an_" + s)
    if "skirt" in extras:
        add("skirt", "body", hipc + np.array([0.0, 0.12, 0.0]), I, "skirt", "rags")
    if "box" in extras:
        add("box", "body", hipc + np.array([0.0, 0.1, 0.0]) + np.array([-0.2, 0.0, 0.0]), I, "copy", "box_cord")
    byname = {b["name"]: b for b in bones}
    for b in bones:
        if b["parent"] is None:
            b["t"] = b["pos"]
            b["L"] = b["F"]
        else:
            P = byname[b["parent"]]
            b["t"] = P["F"].T @ (b["pos"] - P["pos"])
            b["L"] = P["F"].T @ b["F"]
    return bones, byname


ARM_ABDUCT = 0.26   # rad: the shoulders' rest frame, so hanging hands clear her hips


def rz(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]], float)


def rest_mats(bones, byname):
    """Skinning matrices bind -> rest (all pivots at rotation 0: upright, arms
    hanging just clear of the hips, legs plumb), for dressing in the rest pose."""
    G = {}
    for b in bones:
        L = np.eye(3)
        if b["mode"] in ("build", "finger"):
            L = b["L"]
        if b["name"] in ("sh_l", "sh_r"):
            L = rz(-ARM_ABDUCT if b["name"] == "sh_l" else ARM_ABDUCT)
        M = np.eye(4)
        M[:3, :3] = L
        M[:3, 3] = b["t"]
        G[b["name"]] = M if b["parent"] is None else G[b["parent"]] @ M
    S = []
    for b in bones:
        Bm = np.eye(4)
        Bm[:3, :3] = b["F"]
        Bm[:3, 3] = b["pos"]
        S.append(G[b["name"]] @ np.linalg.inv(Bm))
    return np.array(S)


def skin_points(P, W, S):
    """Linear blend skinning of points P (n,3) with dense weights W (n,bones)."""
    M = np.einsum("nb,bij->nij", W, S)
    Ph = np.concatenate([P, np.ones((len(P), 1))], 1)
    return np.einsum("nij,nj->ni", M, Ph)[:, :3], M


def unskin_points(Pr, W, S):
    M = np.einsum("nb,bij->nij", W, S)
    Ph = np.concatenate([Pr, np.ones((len(Pr), 1))], 1)
    return np.einsum("nij,nj->ni", np.linalg.inv(M), Ph)[:, :3]


# MakeHuman bone -> our bone, by name, else by the nearest mapped ancestor.
def map_mh_bone(mb, skel):
    def side_of(name):
        return "r" if name.endswith(".L") else "l"
    b = mb
    while b is not None:
        base = b.split(".")[0]
        s = side_of(b)
        if b in ("root",) or base == "pelvis":
            return "body"
        if base in ("spine05", "spine04"):
            return "spine_lo"
        if base in ("spine03", "spine02", "breast"):
            return "spine_mid"
        if base in ("spine01", "clavicle"):
            return "chest"
        if base == "neck01":
            return "neck_lo"
        if base in ("neck02", "neck03"):
            return "neck_hi"
        if base in ("jaw",) or base.startswith("tongue"):
            return "jaw"
        if base == "head":
            return "head"
        if base in ("shoulder01", "upperarm01", "upperarm02"):
            return "sh_" + s
        if base in ("lowerarm01", "lowerarm02"):
            return "el_" + s
        if base in ("wrist",) or base.startswith("metacarpal") or base == "finger1-1":
            return "wr_" + s
        if base.startswith("finger"):
            f, seg = base[6:].split("-")
            f, seg = int(f), int(seg)
            if f == 1:
                return "t%d_%s" % (0 if seg == 2 else 1, s)
            return "f%d%d_%s" % (f - 2, seg - 1, s)
        if base in ("upperleg01", "upperleg02"):
            return "hp_" + s
        if base in ("lowerleg01", "lowerleg02"):
            return "kn_" + s
        if base == "foot" or base.startswith("toe"):
            return "an_" + s
        b = skel["bones"][b]["parent"]
    return "head"


def skin_weights(V, nv, bones, byname, skel, wfile):
    names = [b["name"] for b in bones]
    col = {n: i for i, n in enumerate(names)}
    W = np.zeros((nv, len(names)))
    for mb, lst in wfile.items():
        ours = col[map_mh_bone(mb, skel)]
        for v, w in lst:
            W[v, ours] += w
    # the hump: what swells under the cloth between her shoulder blades in the Turn
    if "hump" in byname:
        hp_ = byname["hump"]["pos"]
        d2 = ((V[:nv] - (hp_ + np.array([0, 0, -0.04]))) ** 2).sum(1)
        f = 0.85 * np.exp(-d2 / (2 * 0.065 ** 2)) * (V[:nv, 2] < hp_[2])
        moved = W[:, col["chest"]] * f
        W[:, col["hump"]] += moved
        W[:, col["chest"]] -= moved
    # stretch split along each long bone
    for s in "lr":
        for bone, helper, nxt in (("sh_" + s, "sh_%s_x" % s, "el_" + s), ("el_" + s, "el_%s_x" % s, "wr_" + s),
                                  ("hp_" + s, "hp_%s_x" % s, "kn_" + s), ("kn_" + s, "kn_%s_x" % s, "an_" + s)):
            a, b = byname[bone]["pos"], byname[nxt]["pos"]
            d = b - a
            u = np.clip(((V[:nv] - a) @ d) / (d @ d), 0.0, 1.0)
            u = u * u * (3 - 2 * u)
            moved = W[:, col[bone]] * u
            W[:, col[helper]] += moved
            W[:, col[bone]] -= moved
    return W, names


def top4(W):
    idx = np.argsort(-W, axis=1)[:, :4]
    w = np.take_along_axis(W, idx, 1)
    s = w.sum(1, keepdims=True)
    w = np.where(s > 0, w / np.maximum(s, 1e-9), np.array([1.0, 0, 0, 0]))
    return idx.astype(np.uint16), w.astype(np.float32)


# ----------------------------------------------------------- the costume -----


class Part:
    def __init__(self, name, material):
        self.name, self.material = name, material
        self.P, self.N, self.UV, self.W, self.T, self.C = [], [], [], [], [], []

    def add(self, P, N, UV, W, T, C=None):
        base = sum(len(p) for p in self.P)
        self.P.append(np.asarray(P, float))
        self.N.append(np.asarray(N, float))
        self.UV.append(np.asarray(UV, float))
        self.W.append(np.asarray(W, float))
        self.T.append(np.asarray(T, int) + base)
        if C is None:
            C = np.ones((len(P), 4))
        self.C.append(np.asarray(C, float))

    def arrays(self):
        return (np.concatenate(self.P), np.concatenate(self.N), np.concatenate(self.UV), np.concatenate(self.W),
                np.concatenate(self.T), np.concatenate(self.C))

    def tris(self):
        return sum(len(t) for t in self.T)


def split_corners(Vsrc, VTsrc, tris, keep):
    """Body triangles -> (unique (v, vt) corners, triangles over them)."""
    T = np.array([t[0] for t, k in zip(tris, keep) if k], int)
    TT = np.array([t[1] for t, k in zip(tris, keep) if k], int)
    key = T.astype(np.int64) * 100000 + TT
    uniq, inv = np.unique(key.ravel(), return_inverse=True)
    vi = (uniq // 100000).astype(int)
    ti = (uniq % 100000).astype(int)
    return vi, ti, inv.reshape(-1, 3)


def shell(V, N, E, tris_idx, region_v, offset, smooth_iters=12, outward_iters=25, min_gap=None):
    """An offset shell over the body faces in tris_idx, smoothed so cloth bridges
    hollows (under the bust, the small of the back) instead of following skin."""
    S = V + N * offset
    mask = np.zeros(len(V), bool)
    mask[region_v] = True
    for _ in range(smooth_iters):
        S += 0.5 * laplacian(S, E, mask)
    for _ in range(outward_iters):
        L = laplacian(S, E, mask)
        out = np.maximum((L * N).sum(1), 0.0)
        S += 0.8 * N * out[:, None]
    gap = offset if min_gap is None else min_gap
    d = ((S - V) * N).sum(1)
    S += N * np.maximum(gap - d, 0.0)[:, None]
    return S


def boundary_edges(T):
    e = np.concatenate([T[:, [0, 1]], T[:, [1, 2]], T[:, [2, 0]]])
    k = np.sort(e, 1)
    u, inv, cnt = np.unique(k, axis=0, return_inverse=True, return_counts=True)
    return e[cnt[inv.ravel()] == 1]


def loops(edges):
    nxt = {}
    for a, b in edges:
        nxt[a] = b
    seen, out = set(), []
    for a in list(nxt):
        if a in seen:
            continue
        loop = [a]
        seen.add(a)
        b = nxt.get(a)
        while b is not None and b not in seen:
            loop.append(b)
            seen.add(b)
            b = nxt.get(b)
        out.append(loop)
    return out


def band(part, P, N, W, loop, depth_dir, depth, uv_scale=10.0, flip=False):
    """A strip off a boundary loop: a hem's turned edge, a cuff, a raised band."""
    loop = list(loop) + [loop[0]]
    A = P[loop]
    B = A + depth_dir[loop] * depth
    n = len(loop)
    pts = np.concatenate([A, B])
    nrm = np.concatenate([N[loop], N[loop]])
    ww = np.concatenate([W[loop], W[loop]])
    seg = np.r_[0, np.cumsum(np.linalg.norm(np.diff(A, axis=0), axis=1))]
    uv = np.concatenate([np.stack([seg * uv_scale, np.zeros(n)], 1), np.stack([seg * uv_scale, np.full(n, depth * uv_scale)], 1)])
    T = []
    for i in range(n - 1):
        a, b, c, d = i, i + 1, n + i, n + i + 1
        T += [[a, c, b], [b, c, d]] if not flip else [[a, b, c], [b, d, c]]
    part.add(pts, nrm, uv, ww, T)


def loft(part, rings, Wfn, closed=True, uv_scale=6.0, cap_end=False):
    """rings: list of (k, 3) arrays; a tube through them, its own UVs."""
    R = np.array(rings)
    nr, k = R.shape[0], R.shape[1]
    if closed:
        R = np.concatenate([R, R[:, :1]], 1)
        k += 1
    P = R.reshape(-1, 3)
    circ = np.r_[0, np.cumsum(np.linalg.norm(np.diff(R, axis=1), axis=2).mean(0))]
    down = np.r_[0, np.cumsum(np.linalg.norm(np.diff(R, axis=0), axis=2).mean(1))]
    UV = np.stack(np.meshgrid(circ, down), -1).reshape(-1, 2) * uv_scale
    T = []
    for i in range(nr - 1):
        for j in range(k - 1):
            a, b, c, d = i * k + j, i * k + j + 1, (i + 1) * k + j, (i + 1) * k + j + 1
            T += [[a, b, c], [b, d, c]]
    T = np.array(T)
    Nn = vertex_normals(P, T)
    if closed:   # weld the seam's normals
        for i in range(nr):
            a, b = i * k, i * k + k - 1
            m = Nn[a] + Nn[b]
            Nn[a] = Nn[b] = m / max(np.linalg.norm(m), 1e-9)
    part.add(P, Nn, UV, Wfn(P), T)
    return P, T


def box_mesh(part, centre, size, R, W1, uv_scale=10.0):
    c = np.asarray(centre, float)
    h = np.asarray(size, float) / 2
    P, N, UV, T = [], [], [], []
    for axis in range(3):
        for sgn in (-1, 1):
            n = np.zeros(3)
            n[axis] = sgn
            u = np.zeros(3)
            u[(axis + 1) % 3] = 1
            v = np.cross(n, u)
            base = len(P)
            for a, b in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                p = n * h[axis] + u * a * h[(axis + 1) % 3] + v * b * h[(axis + 2) % 3]
                P.append(c + R @ p)
                N.append(R @ n)
                UV.append([a * 0.5 + 0.5, b * 0.5 + 0.5])
            T += [[base, base + 2, base + 1], [base, base + 3, base + 2]] if False else [[base, base + 1, base + 2], [base, base + 2, base + 3]]
    P = np.array(P)
    part.add(P, np.array(N), np.array(UV) * uv_scale * np.max(size), np.repeat(W1[None], len(P), 0), T)


def cylinder_mesh(part, centre, axis, radius, height, W1, sides=12):
    axis = np.asarray(axis, float) / np.linalg.norm(axis)
    a = np.cross(axis, [0, 0, 1.0]) if abs(axis[2]) < 0.9 else np.cross(axis, [1.0, 0, 0])
    a /= np.linalg.norm(a)
    b = np.cross(axis, a)
    P, N, UV, T = [], [], [], []
    c = np.asarray(centre, float)
    for end in (-0.5, 0.5):
        for i in range(sides + 1):
            t = 2 * math.pi * i / sides
            d = a * math.cos(t) + b * math.sin(t)
            P.append(c + axis * end * height + d * radius)
            N.append(d)
            UV.append([i / sides, end + 0.5])
    for i in range(sides):
        p, q, r, s = i, i + 1, sides + 1 + i, sides + 2 + i
        T += [[p, r, q], [q, r, s]]
    for end, sgn in ((-0.5, -1), (0.5, 1)):
        base = len(P)
        P.append(c + axis * end * height)
        N.append(axis * sgn)
        UV.append([0.5, 0.5])
        for i in range(sides):
            t = 2 * math.pi * i / sides
            P.append(c + axis * end * height + (a * math.cos(t) + b * math.sin(t)) * radius)
            N.append(axis * sgn)
            UV.append([0.5 + 0.5 * math.cos(t), 0.5 + 0.5 * math.sin(t)])
        for i in range(sides):
            j = (i + 1) % sides
            T += [[base, base + 1 + i, base + 1 + j]] if sgn > 0 else [[base, base + 1 + j, base + 1 + i]]
    P = np.array(P)
    part.add(P, np.array(N), np.array(UV), np.repeat(W1[None], len(P), 0), T)


def ellipsoid_mesh(part, centre, radii, R, W1, lat=8, lon=12, swirl=0.0):
    P, N, UV, T = [], [], [], []
    c = np.asarray(centre, float)
    r = np.asarray(radii, float)
    for i in range(lat + 1):
        ph = math.pi * i / lat
        for j in range(lon + 1):
            th = 2 * math.pi * j / lon
            d = np.array([math.sin(ph) * math.cos(th), math.cos(ph), math.sin(ph) * math.sin(th)])
            bump = 1.0 + swirl * math.sin(5 * th + 3 * ph)
            P.append(c + R @ (d * r * bump))
            nn = R @ (d / r)
            N.append(nn / np.linalg.norm(nn))
            UV.append([j / lon * 2, i / lat])
    for i in range(lat):
        for j in range(lon):
            a, b, cc, d = i * (lon + 1) + j, i * (lon + 1) + j + 1, (i + 1) * (lon + 1) + j, (i + 1) * (lon + 1) + j + 1
            T += [[a, cc, b], [b, cc, d]]
    P = np.array(P)
    part.add(P, np.array(N), np.array(UV), np.repeat(W1[None], len(P), 0), T)


# ------------------------------------------------------------- glTF ---------


class NamedArrays(tuple):
    """A part's arrays for write_glb, carrying its name and material."""

    def __new__(cls, name, material, arrays):
        t = super().__new__(cls, arrays)
        t.name, t.material = name, material
        return t


def named_part(name, material, arrays):
    return NamedArrays(name, material, arrays)


def write_glb(path, nodes_bones, parts, materials, root="Body", generator="tools/body"):
    bin_chunks = []
    accessors, views = [], []

    def push(arr, target=None, comp=5126, typ="VEC3", minmax=False):
        arr = np.ascontiguousarray(arr)
        raw = arr.tobytes()
        off = sum(len(c) for c in bin_chunks)
        pad = (-len(raw)) % 4
        bin_chunks.append(raw + b"\0" * pad)
        view = {"buffer": 0, "byteOffset": off, "byteLength": len(raw)}
        if target:
            view["target"] = target
        views.append(view)
        acc = {"bufferView": len(views) - 1, "componentType": comp, "count": int(arr.shape[0]), "type": typ}
        if minmax:
            acc["min"] = arr.min(0).tolist()
            acc["max"] = arr.max(0).tolist()
        accessors.append(acc)
        return len(accessors) - 1
    gl_nodes = [{"name": root, "children": []}]
    joint_idx = {}
    for b in nodes_bones:
        joint_idx[b["name"]] = len(gl_nodes)
        q = quat_from_m(b["L"])
        gl_nodes.append({"name": b["name"], "translation": [float(x) for x in b["t"]], "rotation": [float(x) for x in q]})
    for b in nodes_bones:
        if b["parent"] is None:
            gl_nodes[0]["children"].append(joint_idx[b["name"]])
        else:
            gl_nodes[joint_idx[b["parent"]]].setdefault("children", []).append(joint_idx[b["name"]])
    ibm = []
    for b in nodes_bones:
        M = np.eye(4)
        M[:3, :3] = b["F"]
        M[:3, 3] = b["pos"]
        ibm.append(np.linalg.inv(M).T.ravel())   # column-major
    ibm_acc = push(np.array(ibm, np.float32), None, 5126, "MAT4")
    skin = {"joints": [joint_idx[b["name"]] for b in nodes_bones], "inverseBindMatrices": ibm_acc,
            "skeleton": joint_idx[nodes_bones[0]["name"]]}
    meshes = []
    mat_list = list(materials.keys())
    for part in parts:
        P, N, UV, Wi, T, C = part
        idx, w = Wi
        prim = {"attributes": {
            "POSITION": push(P.astype(np.float32), 34962, minmax=True),
            "NORMAL": push(N.astype(np.float32), 34962),
            "TEXCOORD_0": push(UV.astype(np.float32), 34962, typ="VEC2"),
            "JOINTS_0": push(idx.astype(np.uint16), 34962, 5123, "VEC4"),
            "WEIGHTS_0": push(w.astype(np.float32), 34962, typ="VEC4"),
            "COLOR_0": push(C.astype(np.float32), 34962, typ="VEC4")},
            "indices": push(T.astype(np.uint32).ravel(), 34963, 5125, "SCALAR"),
            "material": mat_list.index(part.material)}
        meshes.append({"name": part.name, "primitives": [prim]})
        gl_nodes.append({"name": part.name, "mesh": len(meshes) - 1, "skin": 0})
        gl_nodes[0]["children"].append(len(gl_nodes) - 1)
    mats = []
    for name, m in materials.items():
        mats.append({"name": name, "pbrMetallicRoughness": {"baseColorFactor": m["color"], "metallicFactor": m.get("metal", 0.0),
                                                             "roughnessFactor": m["rough"]}, "doubleSided": m.get("double", False)})
    blob = b"".join(bin_chunks)
    gltf = {"asset": {"version": "2.0", "generator": generator}, "scene": 0, "scenes": [{"nodes": [0]}],
            "nodes": gl_nodes, "meshes": meshes, "skins": [skin], "materials": mats, "accessors": accessors,
            "bufferViews": views, "buffers": [{"byteLength": len(blob)}]}
    js = json.dumps(gltf).encode()
    js += b" " * ((-len(js)) % 4)
    out = struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(blob))
    out += struct.pack("<II", len(js), 0x4E4F534A) + js + struct.pack("<II", len(blob), 0x004E4942) + blob
    open(path, "wb").write(out)
