#!/usr/bin/env python3
"""Build the Matron's body: an aged woman from MakeHuman's CC0 base mesh and
macro targets, dressed in a site first-aid attendant's uniform generated from
the body itself, skinned to a skeleton whose joints are matron_view.gd's pivots.

Run with:  python3 -I build_matron_body.py <download dir> <out dir> [preset ...]

Inputs (all CC0 1.0, MakeHuman assets, commit a8bc2d54ff0ac92e78ff71431b1023eda42bf482
of github.com/makehumancommunity/makehuman, see sources.json): 3dobjs/base.obj
(hm08), targets/macrodetails/..., rigs/default.mhskel, rigs/default_weights.mhw.
No MakeHuman program code is used or copied (that code is AGPL): the file
formats are read here by our own parsers and the macro blend is our own
piecewise-linear weighting.

Outputs per preset: <name>.glb (skinned, one node per garment), <name>_rig.json
(pivot layout and bone modes for the Godot adapter), <name>_stats.json.
"""
import hashlib
import json
import math
import os
import struct
import sys

import numpy as np

DL = sys.argv[1]
OUT = sys.argv[2]
PRESETS_WANTED = sys.argv[3:] or ["matron"]
SOLE = 0.012   # the shoes' sole under the bare foot, metres
FACE_T = os.environ.get("FACE_TARGETS", "")   # optional dir of CC0 face-age targets

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


# A frail old woman, but a woman and not a stick: see the stats lineup.
FACE_AGE = [("head-age-incr", 1.0), ("l-cheek-volume-decr", 0.8), ("r-cheek-volume-decr", 0.8),
            ("l-eye-bag-incr", 0.7), ("r-eye-bag-incr", 0.7), ("l-eye-push1-in", 0.5), ("r-eye-push1-in", 0.5),
            ("mouth-angles-down", 0.5), ("mouth-laugh-lines-out", 0.7), ("mouth-upperlip-volume-decr", 0.5),
            ("mouth-lowerlip-volume-decr", 0.4), ("nose-hump-incr", 0.4), ("neck-platysma", 0.25)]
BODY_AGE = [("targets/stomach/stomach-pregnant-incr.target", 0.22),   # the soft low belly of old age
            ("targets/torso/torso-vshape-decr.target", 0.5),          # narrow, sloping shoulders
            ("targets/buttocks/buttocks-volume-decr.target", 0.55),   # flat old buttocks
            ("targets/measure/measure-shoulder-dist-decr.target", 0.25),
            ("targets/neck/neck-trans-forward.target", 0.45),         # the head carried forward
            ("targets/measure/measure-upperarm-circ-decr.target", 0.15),
            ("targets/measure/measure-calf-circ-decr.target", 0.7)]

PRESETS = {
    # The pick: 77 years, little muscle, a little under average weight.
    "matron": {"age": 0.9, "muscle": 0.15, "weight": 0.50, "height": 0.64,
               "breast_firmness": 0.08, "breast_cup": 0.42, "body_age": True, "face_age": True,
               "stand_m": 1.80},
    # What MakeHuman makes of the thinnest old woman: the stick figure.
    "stick": {"age": 0.9, "muscle": 0.0, "weight": 0.0, "height": 0.64, "breast_firmness": 0.08,
              "body_age": True, "face_age": True, "stand_m": 1.80},
    # An ordinary old woman of average build.
    "thin": {"age": 0.9, "muscle": 0.2, "weight": 0.3, "height": 0.64, "breast_firmness": 0.08,
             "breast_cup": 0.42, "body_age": True, "face_age": True, "stand_m": 1.80},
    "average": {"age": 0.9, "muscle": 0.5, "weight": 0.5, "height": 0.64, "breast_firmness": 0.2,
                "body_age": False, "face_age": True, "stand_m": 1.80},
    # The 1.75 m reference man.
    "reference": {"age": 0.5, "muscle": 0.5, "weight": 0.5, "height": 0.5, "sex": "male",
                  "body_age": False, "face_age": False, "stand_m": 1.75, "dress": False},
}

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


def build_rig(V, J, skel):
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
    add("skirt", "body", hipc + np.array([0.0, 0.12, 0.0]), I, "skirt", "rags")
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


def write_glb(path, nodes_bones, parts, materials):
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
    gl_nodes = [{"name": "Matron", "children": []}]
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
    gltf = {"asset": {"version": "2.0", "generator": "build_matron_body.py"}, "scene": 0, "scenes": [{"nodes": [0]}],
            "nodes": gl_nodes, "meshes": meshes, "skins": [skin], "materials": mats, "accessors": accessors,
            "bufferViews": views, "buffers": [{"byteLength": len(blob)}]}
    js = json.dumps(gltf).encode()
    js += b" " * ((-len(js)) % 4)
    out = struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(blob))
    out += struct.pack("<II", len(js), 0x4E4F534A) + js + struct.pack("<II", len(blob), 0x004E4942) + blob
    open(path, "wb").write(out)


# ------------------------------------------------------------- build --------

def build(name, p, V0, VT, faces, skel, wfile):
    V = morph(V0, p)
    if p.get("face_age") and FACE_T:
        for t, k in FACE_AGE:
            idx, d = target(os.path.join(FACE_T, t + ".target"))
            V[idx] += k * d
    if p.get("body_age"):
        for rel, k in BODY_AGE:
            idx, d = target(rel)
            V[idx] += k * d
    tris = tri_faces(faces)
    body_tris = [t for t in tris if t[2] == "body"]
    body_v = np.unique(np.array([t[0] for t in body_tris]).ravel())
    # decimetres -> metres, soles on y = 0, scaled to the standing height asked for
    V = V * 0.1
    sole = V[body_v, 1].min()
    V[:, 1] -= sole
    natural = float(V[body_v, 1].max())
    k = p["stand_m"] / natural
    V *= k
    if p.get("dress", True):
        V[:, 1] += SOLE   # she stands on her shoes' soles
    J = {jn: V[vs].mean(0) for jn, vs in skel["joints"].items()}
    T_body = np.array([t[0] for t in body_tris])
    N = vertex_normals(V, T_body)
    # helpers (eyes) get their own normals
    eye_tris = [t for t in tris if t[2] in ("helper-l-eye", "helper-r-eye")]
    if eye_tris:
        Ne = vertex_normals(V, np.array([t[0] for t in eye_tris]))
        ev = np.unique(np.array([t[0] for t in eye_tris]).ravel())
        N[ev] = Ne[ev]
    E = neighbours(len(V), T_body)
    bones, byname = build_rig(V, J, skel)
    Wfull, names = skin_weights(V, len(V), bones, byname, skel, wfile)
    col = {n: i for i, n in enumerate(names)}

    def wsum(*bs):
        return sum(Wfull[:, col[b]] for b in bs)
    stats = {"preset": name, "params": {k2: v for k2, v in p.items() if k2 != "extra"}, "years": years(p["age"]),
             "natural_height_m": natural, "scale_to_stand": k, "stand_m": p["stand_m"]}
    # measurements (bind pose, before clothes): tape-measure girths, the
    # convex hull of where a plane cuts the skin's triangles
    vol = volume(V, T_body)
    stats["volume_l"] = vol * 1000
    stats["mass_kg"] = vol * 1010.0
    stats["bmi"] = stats["mass_kg"] / p["stand_m"] ** 2

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
        return perimeter(hull2(np.stack([(P - origin) @ u, (P - origin) @ w], 1)))
    up = np.array([0.0, 1.0, 0.0])
    trunk_sel = np.where(wsum("body", "spine_lo", "spine_mid", "chest", "hump", "hp_l", "hp_r") > 0.5)[0]
    trunk_sel = trunk_sel[np.isin(trunk_sel, body_v)]
    Ttr = tris_of(trunk_sel)
    cz = float(J["spine03____head"][2])
    wy = np.linspace(J["spine03____head"][1], J["spine01____head"][1], 30)
    g = [cut(Ttr, np.array([0, y, cz]), up, 0.22) for y in wy]
    gi = int(np.argmin([x if x > 0 else 9 for x in g]))
    stats["waist_cm"] = 100 * g[gi]
    stats["waist_y"] = float(wy[gi])
    g = [cut(Ttr, np.array([0, y, cz]), up, 0.24) for y in np.linspace(J["spine02____head"][1], J["spine01____head"][1] + 0.06, 30)]
    stats["bust_cm"] = 100 * max(g)
    yh = float(J["upperleg01.L____head"][1])
    g = [cut(Ttr, np.array([0, y, cz]), up, 0.3) for y in np.linspace(yh - 0.12, yh + 0.04, 30)]
    stats["hip_cm"] = 100 * max(g)
    s_ = "r"
    a, b = byname["sh_" + s_]["pos"], byname["el_" + s_]["pos"]
    Ta = tris_of(np.where(wsum("sh_" + s_, "sh_%s_x" % s_, "el_" + s_) > 0.5)[0][np.isin(np.where(wsum("sh_" + s_, "sh_%s_x" % s_, "el_" + s_) > 0.5)[0], body_v)])
    stats["upper_arm_cm"] = 100 * cut(Ta, 0.5 * (a + b), b - a, 0.08)
    a, b = byname["kn_" + s_]["pos"], byname["an_" + s_]["pos"]
    sel = np.where(wsum("kn_" + s_, "kn_%s_x" % s_, "hp_%s_x" % s_, "an_" + s_) > 0.5)[0]
    Tl = tris_of(sel[np.isin(sel, body_v)])
    stats["calf_cm"] = 100 * max(cut(Tl, a + (b - a) * t, b - a, 0.09) for t in np.linspace(0.12, 0.5, 16))
    a, b = byname["hp_" + s_]["pos"], byname["kn_" + s_]["pos"]
    sel = np.where(wsum("hp_" + s_, "hp_%s_x" % s_, "kn_" + s_) > 0.5)[0]
    Tl = tris_of(sel[np.isin(sel, body_v)])
    stats["thigh_cm"] = 100 * cut(Tl, a + (b - a) * 0.4, b - a, 0.12)
    stats["shoulder_width_cm"] = 100 * float(np.linalg.norm(byname["sh_l"]["pos"] - byname["sh_r"]["pos"]))
    stats["arm_len_m"] = byname["sh_r"]["length"] + byname["el_r"]["length"]
    stats["thigh_len_m"] = byname["hp_r"]["length"]
    stats["shin_len_m"] = byname["kn_r"]["length"]
    stats["hip_joint_y"] = float(byname["hp_r"]["pos"][1])
    stats["ankle_y"] = float(byname["an_r"]["pos"][1])
    stats["head_joint_y"] = float(byname["head"]["pos"][1])
    stats["eye_y"] = float(J["eye.L____head"][1])
    stats["torso_len_m"] = float(byname["neck_lo"]["pos"][1] - byname["spine_lo"]["pos"][1])
    parts = []
    materials = {}

    def mat(nm, color, rough, metal=0.0, double=False):
        materials[nm] = {"color": color, "rough": rough, "metal": metal, "double": double}

    dress = p.get("dress", True)
    keep = np.ones(len(body_tris), bool)
    # ---- the costume ----
    if dress:
        # regions on the body (bind pose)
        sleeve_u = 0.42
        sh_l, el_l = byname["sh_l"]["pos"], byname["el_l"]["pos"]
        y_waist = stats["waist_y"]   # the natural waist: the narrowest girth
        arm_up = wsum("sh_l", "sh_l_x", "sh_r", "sh_r_x")
        arm_low = wsum("el_l", "el_l_x", "el_r", "el_r_x", "wr_l", "wr_r") + sum(Wfull[:, col[n]] for n in names if n[0] in "ft" and n[1].isdigit())
        neckw = wsum("neck_lo", "neck_hi", "head", "jaw")
        u_arm = np.zeros(len(V))
        for s in "lr":
            a, b = byname["sh_" + s]["pos"], byname["el_" + s]["pos"]
            d = b - a
            u = ((V - a) @ d) / (d @ d)
            sel = (wsum("sh_" + s, "sh_%s_x" % s) > 0.3) & (np.sign(V[:, 0]) == (-1 if s == "l" else 1))
            u_arm[sel] = u[sel]
        y_jug = float(J["neck01____head"][1])
        in_top = (V[:, 1] > y_waist - 0.035) & (neckw < 0.22) & (arm_low < 0.3) & (u_arm < sleeve_u)
        # a round neckline a little lower in front
        front = V[:, 2] > J["neck01____head"][2] + 0.02
        in_top &= ~(front & (V[:, 1] > y_jug - 0.035) & (np.abs(V[:, 0]) < 0.07))
        in_top[~np.isin(np.arange(len(V)), body_v)] = False
        top_tris = np.array([all(in_top[t[0]]) for t in body_tris])
        Tt = T_body[top_tris]
        top_v = np.unique(Tt.ravel())
        S = shell(V, N, E, Tt, top_v, 0.010, smooth_iters=24, outward_iters=60, min_gap=0.004)
        flare = np.clip((u_arm - 0.12) / (sleeve_u - 0.12), 0.0, 1.0)
        S += N * (0.009 * flare)[:, None]
        # cloth falls from the bust to the belt and from the shoulder blades to the
        # small of the back, instead of following the body under them
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
            S += 0.5 * laplacian(S, E, np.isin(np.arange(len(V)), top_v))
        # the bust: loose cotton over it, no nipples, no cleavage
        nip = [J["breast.L____tail"], J["breast.R____tail"]]
        near = np.zeros(len(V), bool)
        for q in nip:
            near |= np.linalg.norm(V - q, axis=1) < 0.085
        near &= np.isin(np.arange(len(V)), top_v)
        for _ in range(18):
            S += 0.5 * laplacian(S, E, near)
            L = laplacian(S, E, near)
            S += 0.8 * N * np.maximum((L * N).sum(1), 0.0)[:, None]
        Wtop = Wfull
        # UVs: the skin's own atlas (stains and the burn share it) -- corners split
        vi, ti, Tc = split_corners(V, VT, body_tris, top_tris)
        tunic = Part("tunic", "tunic")
        tunic.add(S[vi], vertex_normals(S, Tt)[vi], VT[ti] * 8.0, Wfull[vi], Tc)
        # the turned edges: neckline, sleeves, waist (under the belt)
        bl = loops(boundary_edges(Tt))
        Ns = vertex_normals(S, Tt)
        for lp in bl:
            lp = np.array(lp)
            for _ in range(10):
                S[lp] = 0.5 * S[lp] + 0.25 * (np.roll(S[lp], 1, 0) + np.roll(S[lp], -1, 0))
        Ns = vertex_normals(S, Tt)
        for lp in bl:
            band(tunic, S, Ns, Wfull, lp, -Ns, 0.006)
        parts.append(tunic)
        mat("tunic", [0.86, 0.84, 0.79, 1.0], 0.86)
        # Peter Pan collar: the top shell near the neckline, raised and split at the front
        neck_loop = max(bl, key=lambda lp: S[lp, 1].mean())
        NL = S[neck_loop]
        dmin = np.min(np.linalg.norm(S[top_v][:, None, :] - NL[None, :, :], axis=2), axis=1)
        dist = np.full(len(V), 9.0)
        dist[top_v] = dmin
        cz = J["neck01____head"][2]
        in_col = (dist < 0.062) & ~((np.abs(V[:, 0]) < 0.012) & (V[:, 2] > cz))
        col_tris = np.array([all(in_col[t]) for t in Tt])
        Tcl = Tt[col_tris]
        C2 = S + Ns * 0.004
        cv = np.unique(Tcl.ravel())
        for _ in range(4):
            C2 += 0.5 * laplacian(C2, neighbours(len(V), Tcl), np.isin(np.arange(len(V)), cv))
        collar = Part("collar", "collar")
        vi2, ti2, Tc2 = split_corners(V, VT, [t for t, k2 in zip(body_tris, top_tris) if k2], col_tris)
        for lp in loops(boundary_edges(Tcl)):
            lp = np.array(lp)
            for _ in range(10):
                C2[lp] = 0.5 * C2[lp] + 0.25 * (np.roll(C2[lp], 1, 0) + np.roll(C2[lp], -1, 0))
        Nc = vertex_normals(C2, Tcl)
        collar.add(C2[vi2], Nc[vi2], VT[ti2] * 8.0, Wfull[vi2], Tc2)
        for lp in loops(boundary_edges(Tcl)):
            band(collar, C2, Nc, Wfull, lp, -Nc, 0.005)
        parts.append(collar)
        mat("collar", [0.93, 0.92, 0.88, 1.0], 0.62)
        # the breast pocket on her left (+x), where the pellet is: a clean
        # rectangle laid on the tunic's front, sagging a little with what is in it
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
        Wg = _w_of(Pg, S, Wfull, front_v)
        pocket = Part("pocket", "collar")
        pocket.add(Pg, Ng, (G2 - pc) * 8.0, Wg, Tg)
        rim = [j * nx_ for j in range(ny_)] + [(ny_ - 1) * nx_ + i for i in range(1, nx_)] + \
              [j * nx_ + nx_ - 1 for j in range(ny_ - 2, -1, -1)] + [i for i in range(nx_ - 2, 0, -1)]
        band(pocket, Pg, Ng, Wg, rim, -Ng, 0.004)
        parts.append(pocket)
        # a penlight clipped in it (the death-cam's penlight), at its inner top corner
        top_c = Pg[2] + np.array([0.0, 0.0, -0.002])
        W1 = Wg[2]
        pen = Part("penlight", "steel")
        cylinder_mesh(pen, top_c + np.array([0.0, -0.04, -0.001]), [0, 1, 0], 0.0055, 0.13, W1, 10)
        box_mesh(pen, top_c + np.array([0.0, 0.0, 0.006]), [0.004, 0.04, 0.0015], np.eye(3), W1)
        parts.append(pen)
        mat("steel", [0.7, 0.7, 0.72, 1.0], 0.35, 0.5)
        # the front placket and buttons down it
        in_pl = (np.abs(V[:, 0]) < 0.016) & (V[:, 2] > 0.0)
        pl_tris = np.array([all(in_pl[t]) for t in Tt])
        Tpl = Tt[pl_tris]
        PL = S + Ns * 0.002
        plk = Part("placket", "tunic")
        vi4, ti4, Tc4 = split_corners(V, VT, [t for t, k2 in zip(body_tris, top_tris) if k2], pl_tris)
        plk.add(PL[vi4], vertex_normals(PL, Tpl)[vi4], VT[ti4] * 8.0, Wfull[vi4], Tc4)
        parts.append(plk)
        btn = Part("buttons", "button")
        plv = np.unique(Tpl.ravel())
        ylo, yhi = y_waist + 0.05, y_jug - 0.05
        for yb in np.linspace(ylo, yhi, 5):
            i = plv[np.argmin(np.abs(PL[plv, 1] - yb) + 3 * np.abs(PL[plv, 0]))]
            cylinder_mesh(btn, PL[i] + Ns[i] * 0.0025, Ns[i], 0.0065, 0.003, Wfull[i], 10)
        parts.append(btn)
        mat("button", [0.88, 0.86, 0.8, 1.0], 0.3)
        # hide the skin the tunic covers (keep 2 cm under every edge)
        edge_v = np.unique(np.concatenate([np.array(lp) for lp in bl]))
        de = np.min(np.linalg.norm(V[top_v][:, None, :] - V[edge_v][None, ::3, :], axis=2), axis=1)
        covered = np.zeros(len(V), bool)
        covered[top_v[de > 0.025]] = True
        covered[top_v[(u_arm[top_v] > 0.0) & (u_arm[top_v] < sleeve_u - 0.06)]] = True
        keep &= ~np.array([all(covered[t[0]]) for t in body_tris])
        # ---- the skirt: a loft from the waist that hangs from the hips ----
        legs = (wsum("body", "spine_lo", "spine_mid", "hp_l", "hp_r", "hp_l_x", "hp_r_x", "kn_l", "kn_r", "kn_l_x", "kn_r_x") > 0.7)
        legs &= np.isin(np.arange(len(V)), body_v)
        legs &= arm_up + arm_low < 0.05
        knee_y = float(byname["kn_l"]["pos"][1])
        hem_y = knee_y - 0.075
        top_y = y_waist - 0.005
        nring, nseg = 22, 56
        ys = np.linspace(top_y, hem_y, nring)
        cx = 0.0
        cz = float(V[legs & (np.abs(V[:, 1] - J["upperleg01.L____head"][1]) < 0.03), 2].mean())
        th = np.linspace(0, 2 * math.pi, nseg, endpoint=False)
        U = np.stack([np.sin(th), np.zeros(nseg), np.cos(th)], 1)
        rad = np.zeros((nring, nseg))
        Srest = rest_mats(bones, byname)
        Vr, _ = skin_points(V, Wfull, Srest)
        LP = Vr[legs]
        # where the hanging hands are, in the rest pose (they must stay outside)
        hand_pts = Vr[(wsum("wr_l", "wr_r") > 0.5) & np.isin(np.arange(len(V)), body_v)]
        stats["rest_hand_inner_x"] = float(np.abs(hand_pts[:, 0]).min())
        stats["rest_hand_y_range"] = [float(hand_pts[:, 1].min()), float(hand_pts[:, 1].max())]
        for i, y in enumerate(ys):
            sl = LP[np.abs(LP[:, 1] - y) < 0.02]
            if len(sl) == 0:
                rad[i] = rad[i - 1]
                continue
            rel = sl - np.array([cx, 0, cz])
            rad[i] = (rel[:, [0, 2]] @ U[:, [0, 2]].T).max(0)
        stats["skirt_ring_halfwidth_x"] = [round(float(max(r_[np.argmin(np.abs(th - np.pi / 2))], r_[np.argmin(np.abs(th - 3 * np.pi / 2))])), 3) for r_ in rad[:8]]
        stats["skirt_ring_y"] = [round(float(y), 3) for y in ys[:8]]
        # ease: snug at the waist, room over the hips, an A-line flare to the hem
        t = (ys - top_y) / (hem_y - top_y)
        ease = 0.012 + 0.016 * np.clip(t * 3, 0, 1) + 0.05 * t ** 1.5
        rad = rad + ease[:, None]
        for i in range(1, nring):   # cloth falls: never narrower than the ring above
            rad[i] = np.maximum(rad[i], rad[i - 1] - 0.002)
        for _ in range(3):
            rad = 0.25 * np.roll(rad, 1, 1) + 0.5 * rad + 0.25 * np.roll(rad, -1, 1)
        folds = 1.0 + 0.035 * t[:, None] ** 1.2 * np.sin(th[None, :] * 11 + 0.6 * np.sin(th[None, :] * 3))
        rings = [np.array([cx, y, cz]) + (U * (rad[i] * folds[i])[:, None]) for i, y in enumerate(ys)]
        hp_l, hp_r = byname["hp_l"]["pos"], byname["hp_r"]["pos"]
        # the skirt's half-width where the hands hang, against the hands
        yh0, yh1 = stats["rest_hand_y_range"]
        band_i = [i for i, y in enumerate(ys) if yh0 <= y <= yh1]
        if band_i:
            side_r = max(float(np.max(np.abs((rings[i] - np.array([cx, 0, cz]))[:, 0]))) for i in band_i)
            stats["rest_skirt_halfwidth_at_hands"] = side_r
            stats["rest_hand_clearance_m"] = stats["rest_hand_inner_x"] - side_r

        def skirt_w(P):
            Wk = np.zeros((len(P), len(names)))
            tt = np.clip((top_y - P[:, 1]) / (top_y - hem_y), 0, 1)
            thigh = 0.85 * tt ** 1.2
            sx = np.clip(0.5 - P[:, 0] / 0.12, 0, 1)     # 1 on -x (the "_l" leg)
            Wk[:, col["hp_l"]] = thigh * sx
            Wk[:, col["hp_r"]] = thigh * (1 - sx)
            Wk[:, col["skirt"]] = (1 - thigh) * 0.5 * tt
            Wk[:, col["body"]] = (1 - thigh) * (1 - 0.5 * tt)
            return Wk
        def to_bind(rs):
            out = []
            for r in rs:
                out.append(unskin_points(r, skirt_w(r), Srest))
            return out
        skirt = Part("skirt", "tunic")
        hem_ring = rings[-1]
        inner = [hem_ring, np.array([cx, hem_y + 0.02, cz]) + (hem_ring - np.array([cx, hem_y, cz])) * 0.985]
        SP, ST = loft(skirt, to_bind(rings), skirt_w, uv_scale=8.0)
        loft(skirt, to_bind(inner), skirt_w, uv_scale=8.0)
        parts.append(skirt)
        # the belt over the join, and its buckle
        belt = Part("belt", "tunic")
        bys = np.linspace(top_y + 0.022, top_y - 0.018, 3)
        brings = [np.array([cx, y, cz]) + U * (rad[0] + 0.004)[:, None] for y in bys]
        loft(belt, [unskin_points(r, _w_of(r, Vr, Wfull, trunk_sel), Srest) for r in brings], lambda P: _w_of(P, V, Wfull, trunk_sel), uv_scale=8.0)
        parts.append(belt)
        front_i = int(np.argmin(np.abs(th)))
        bk = unskin_points((rings[0][front_i] + np.array([0, 0.002, 0.008]))[None], _w_of(rings[0][front_i][None], Vr, Wfull, trunk_sel), Srest)[0]
        buckle = Part("buckle", "steel")
        box_mesh(buckle, bk, [0.06, 0.045, 0.006], np.eye(3), _w_of(bk[None], V, Wfull, trunk_sel)[0])
        parts.append(buckle)
        # skin under the skirt: keep the thighs (the skirt rides up when she crawls)
        # ---- shoes: lace-up flats lofted round each foot, a 12 mm sole ----
        shoe = Part("shoes", "shoe")
        hidden = np.zeros(len(V), bool)
        for s_ in "lr":
            ank = byname["an_" + s_]["pos"]
            fsel = (wsum("an_" + s_) > 0.5) | ((wsum("kn_%s_x" % s_) > 0.3) & (V[:, 1] < ank[1] + 0.02))
            fsel &= (V[:, 1] < ank[1] + 0.045) & np.isin(np.arange(len(V)), body_v)
            FP = V[fsel]
            low = FP[FP[:, 1] < SOLE + 0.02]
            heel, toe = low[np.argmin(low[:, 2])], low[np.argmax(low[:, 2])]
            ax = toe - heel
            ax[1] = 0.0
            flen = float(np.linalg.norm(ax))
            ax /= flen
            lat = np.cross([0.0, 1.0, 0.0], ax)
            nsl, nring = 18, 22
            phis = np.linspace(0, 2 * math.pi, nring, endpoint=False)
            rings = []
            collar = []
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
                ring = []
                for ph in phis:
                    u = np.array([math.cos(ph), math.sin(ph)])
                    r = float((Q @ u).max()) + 0.005
                    pt2 = ctr + u * r
                    y = pt2[1]
                    if y < SOLE + 0.006:
                        y = 0.0            # the sole, flat on the floor
                    ring.append(c0 + lat * pt2[0] + np.array([0.0, y - c0[1], 0.0]))
                rings.append(np.array(ring))
            R = np.array(rings)
            # toe and heel caps: shrink the end rings to a point
            tip = R[-1].mean(0) + ax * 0.012
            back = R[0].mean(0) - ax * 0.006
            R = np.concatenate([back[None, None, :].repeat(nring, 1) * 0.0 + (0.35 * R[:1] + 0.65 * back), R, 0.4 * R[-1:] + 0.6 * tip])
            pool = np.where(fsel)[0]
            Wfn = lambda P, pool=pool: _w_of(P, V, Wfull, pool)
            base_n = sum(len(x) for x in shoe.P)
            SP_, ST_ = loft(shoe, list(R), Wfn, uv_scale=10.0)
            # the opening: drop the top of the rings over the heel half, where the leg goes in
            k = nring + 1
            Pk = SP_.reshape(len(R), k, 3)
            open_face = []
            for tri in ST_:
                pts = SP_[tri]
                along = ((pts - heel) @ ax) / flen
                if (pts[:, 1] > SOLE + 0.055 + 0.03 * np.clip(along.mean() - 0.1, 0, 1) * 0).all() and (along < 0.62).all():
                    open_face.append(True)
                else:
                    open_face.append(False)
            open_face = np.array(open_face)
            shoe.T[-1] = shoe.T[-1][~open_face]
            # skin inside the shoe goes
            inside = fsel & (V[:, 1] < SOLE + 0.05)
            hidden |= inside
        parts.append(shoe)
        mat("shoe", [0.07, 0.065, 0.06, 1.0], 0.42)
        keep &= ~np.array([all(hidden[t[0]]) for t in body_tris])
        # ---- hair: a thin grey cap over the scalp, a bun; the white cap over it ----
        headw = Wfull[:, col["head"]]
        hj = byname["head"]["pos"]
        eye = 0.5 * (J["eye.L____head"] + J["eye.R____head"])
        brow_y = eye[1] + 0.03
        rel = V - hj
        # hairline: a plane through the forehead (6 cm over the brows), the ear tops and the nape
        p_front = np.array([0.0, brow_y + 0.058, eye[2] + 0.005])
        p_nape = np.array([0.0, hj[1] - 0.005, hj[2] - 0.075])
        p_side = np.array([0.07, eye[1] + 0.035, hj[2] - 0.0])
        nrm = np.cross(p_nape - p_front, p_side - p_front)
        if nrm[1] < 0:
            nrm = -nrm
        in_hair = ((V - p_front) @ nrm > 0) & (headw > 0.6) & np.isin(np.arange(len(V)), body_v)
        h_tris = np.array([all(in_hair[t[0]]) for t in body_tris])
        Th = T_body[h_tris]
        hv = np.unique(Th.ravel())
        HS = shell(V, N, E, Th, hv, 0.004, smooth_iters=8, outward_iters=30)
        # a clean hairline: smooth the staircase of whole faces along the edge
        for lp in loops(boundary_edges(Th)):
            lp = np.array(lp)
            for _ in range(12):
                HS[lp] = 0.5 * HS[lp] + 0.25 * (np.roll(HS[lp], 1, 0) + np.roll(HS[lp], -1, 0))
        hair = Part("hair", "hair")
        Nh = vertex_normals(HS, Th)
        Wh = np.zeros(len(names))
        Wh[col["head"]] = 1.0
        back_pt = HS[hv][np.argmin(HS[hv, 2] + 0.5 * np.abs(HS[hv, 1] - (hj[1] + 0.07)))]
        # strands combed back to the bun: UVs on meridians that meet at the bun,
        # so a strand texture's columns run from the hairline to the bun
        hc = hj + np.array([0.0, 0.085, 0.015])
        bun = back_pt + np.array([0, 0.0, -0.022])
        ax_ = (bun - hc) / np.linalg.norm(bun - hc)
        e1 = np.cross(ax_, [1.0, 0.0, 0.0])
        e1 /= np.linalg.norm(e1)
        e2 = np.cross(ax_, e1)
        if e1[1] > 0:      # the seam (phi = pi) under the bun, at the nape
            e1 = -e1
            e2 = -e2
        Pc = HS[Th.ravel()]
        dd = Pc - hc
        dd /= np.linalg.norm(dd, axis=1, keepdims=True)
        theta = np.arccos(np.clip(dd @ ax_, -1, 1))
        phi = np.arctan2(dd @ e2, dd @ e1)
        U_ = (phi / (2 * np.pi) + 0.5) * 7.0
        V_ = theta / np.pi * 1.5
        U_ = U_.reshape(-1, 3)
        span = U_.max(1, keepdims=True) - U_.min(1, keepdims=True)
        U_ = np.where((span > 3.5) & (U_ < 3.5), U_ + 7.0, U_).ravel()
        # the hairline thins out: alpha by height over the (smooth) hairline plane
        dpl = ((Pc - p_front) @ (nrm / np.linalg.norm(nrm)))
        alpha = np.clip(0.15 + dpl / 0.03, 0.0, 1.0)
        Cc = np.stack([np.ones_like(alpha), np.ones_like(alpha), np.ones_like(alpha), alpha], 1)
        nT = len(Th)
        hair.add(Pc, Nh[Th.ravel()], np.stack([U_, V_], 1), Wfull[Th.ravel()], np.arange(nT * 3).reshape(-1, 3), Cc)
        parts.append(hair)
        bunp = Part("bun", "hair")
        ellipsoid_mesh(bunp, bun + np.array([0.0, -0.01, -0.004]), [0.036, 0.03, 0.026], np.eye(3), Wh, 10, 16, swirl=0.04)
        parts.append(bunp)
        mat("hair", [0.62, 0.61, 0.58, 1.0], 0.7)
        # the cap: a crown shell over the hair and a stiff upturned band at its front
        c_front = p_front + np.array([0, 0.035, -0.035])
        c_back = np.array([0.0, hj[1] + 0.085, hj[2] - 0.1])
        c_side = np.array([0.08, eye[1] + 0.085, hj[2] - 0.03])
        cn = np.cross(c_back - c_front, c_side - c_front)
        if cn[1] < 0:
            cn = -cn
        in_cap = in_hair & ((V - c_front) @ cn > 0)
        c_tris = np.array([all(in_cap[t[0]]) for t in body_tris])
        Tcp = T_body[c_tris]
        cpv = np.unique(Tcp.ravel())
        CP = shell(V, N, E, Tcp, cpv, 0.016, smooth_iters=16, outward_iters=40)
        cap = Part("cap", "cap")
        vi7, ti7, Tc7 = split_corners(V, VT, body_tris, c_tris)
        Ncp = vertex_normals(CP, Tcp)
        cap.add(CP[vi7], Ncp[vi7], VT[ti7] * 8.0, Wfull[vi7], Tc7)
        lp = max(loops(boundary_edges(Tcp)), key=len)
        # the band: up and a little back, along the cap's front half
        lpa = np.array(lp)
        frontish = CP[lpa, 2] > hj[2] - 0.02
        # rotate the loop to start at a back point so the front run is contiguous
        start = int(np.argmin(np.where(frontish, 9, CP[lpa, 2])))
        lpa = np.roll(lpa, -start)
        run = [v for v in lpa if CP[v, 2] > hj[2] - 0.035]
        up = np.zeros_like(CP)
        up[:, 1] = 1.0
        up[:, 2] = -0.75
        up /= np.linalg.norm(up, axis=1, keepdims=True)
        if len(run) > 3:
            loopr = run
            A = CP[loopr]
            Bp = A + up[loopr] * 0.042
            n = len(loopr)
            pts = np.concatenate([A, Bp])
            nn = np.concatenate([Ncp[loopr], Ncp[loopr]])
            seg = np.r_[0, np.cumsum(np.linalg.norm(np.diff(A, axis=0), axis=1))]
            uv = np.concatenate([np.stack([seg * 8, np.zeros(n)], 1), np.stack([seg * 8, np.full(n, 0.4)], 1)])
            Tb = []
            for i in range(n - 1):
                Tb += [[i, n + i, i + 1], [i + 1, n + i, n + i + 1]]
            cap.add(pts, nn, uv, np.repeat(Wh[None], len(pts), 0), Tb)
        parts.append(cap)
        mat("cap", [0.95, 0.94, 0.91, 1.0], 0.55, double=True)
        # skin under the cap
        covered = np.zeros(len(V), bool)
        covered[cpv] = True
        # ---- the music box on its cord at her hip ----
        Wb = np.zeros(len(names))
        Wb[col["box"]] = 1.0
        bxp = byname["box"]["pos"]
        mb = Part("music_box", "wood")
        cylinder_mesh(mb, bxp + np.array([0, -0.15, 0]), [0, 1, 0], 0.0025, 0.3, Wb, 6)
        box_mesh(mb, bxp + np.array([0, -0.33, 0]), [0.075, 0.055, 0.06], np.eye(3), Wb)
        parts.append(mb)
        mat("wood", [0.32, 0.2, 0.11, 1.0], 0.55)
    # ---- the skin: body minus what clothes hide; legs below the hem in stockings ----
    vi, ti, Tc = split_corners(V, VT, body_tris, keep)
    skin = Part("skin", "skin")
    stock = Part("stockings", "stocking")
    if dress:
        legv = ((wsum("hp_l", "hp_r", "hp_l_x", "hp_r_x", "kn_l", "kn_r", "kn_l_x", "kn_r_x", "an_l", "an_r") > 0.5)
                & (V[:, 1] < float(J["upperleg01.L____head"][1]) - 0.06))
        legc = legv[vi]
        tri_leg = legc[Tc].all(1)
        for part, sel in ((skin, ~tri_leg), (stock, tri_leg)):
            Tsel = Tc[sel]
            used = np.unique(Tsel.ravel())
            remap = -np.ones(len(vi), int)
            remap[used] = np.arange(len(used))
            part.add(V[vi[used]], N[vi[used]], VT[ti[used]], Wfull[vi[used]], remap[Tsel])
        parts.append(skin)
        parts.append(stock)
        mat("skin", [0.80, 0.72, 0.66, 1.0], 0.62)
        mat("stocking", [0.13, 0.11, 0.1, 1.0], 0.5)
    else:
        skin.add(V[vi], N[vi], VT[ti], Wfull[vi], Tc)
        parts.append(skin)
        mat("skin", [0.55, 0.55, 0.56, 1.0], 0.7)
    # ---- eyes: MakeHuman's helper eyes with a painted iris ----
    if dress:
        eyes = Part("eyes", "eye")
        for grp, jn in (("helper-l-eye", "eye.L"), ("helper-r-eye", "eye.R")):
            et = [t for t in tris if t[2] == grp]
            Te = np.array([t[0] for t in et])
            ev = np.unique(Te.ravel())
            ctr = J[jn + "____head"]
            ax = J[jn + "____tail"] - ctr
            ax /= np.linalg.norm(ax)
            d = V[ev] - ctr
            ang = np.arccos(np.clip((d / np.linalg.norm(d, axis=1, keepdims=True)) @ ax, -1, 1))
            C = np.tile(np.array([0.86, 0.83, 0.74, 1.0]), (len(ev), 1))
            C[ang < 0.22] = [0.03, 0.03, 0.03, 1]
            C[(ang < 0.5) & (ang >= 0.22)] = [0.36, 0.39, 0.4, 1]
            remap = -np.ones(len(V), int)
            remap[ev] = np.arange(len(ev))
            Wv = np.zeros((len(ev), len(names)))
            Wv[:, col["head"]] = 1.0
            eyes.add(V[ev], N[ev], np.zeros((len(ev), 2)), Wv, remap[Te], C)
        parts.append(eyes)
        mat("eye", [1, 1, 1, 1], 0.15)
    # ---- write ----
    out_parts = []
    tri_counts = {}
    for part in parts:
        P, Nn, UV, Wp, Tp, Cp = part.arrays()
        idx, w = top4(Wp)
        o = (P, Nn, UV, (idx, w), Tp, Cp)
        out_parts.append(_Named(part.name, part.material, o))
        tri_counts[part.name] = int(len(Tp))
    stats["triangles"] = tri_counts
    stats["triangles_total"] = int(sum(tri_counts.values()))
    stats["bones"] = len(bones)
    write_glb(os.path.join(OUT, name + ".glb"), bones, out_parts, materials)
    rig = []
    for b in bones:
        r = {"name": b["name"], "parent": b["parent"], "t": [float(x) for x in b["t"]],
             "q": [float(x) for x in quat_from_m(b["L"])], "euler": euler_yxz(b["L"]), "mode": b["mode"],
             "pivot": b["pivot"], "pos": [float(x) for x in b["pos"]]}
        if b["name"] in ("sh_l", "sh_r"):
            r["abduct"] = -ARM_ABDUCT if b["name"] == "sh_l" else ARM_ABDUCT
        for key in ("share", "length"):
            if key in b:
                r[key] = b[key]
        rig.append(r)
    # points to measure ground contact and height by, in each bone's bind frame
    def local(bn, pt):
        bb = byname[bn]
        return [float(x) for x in bb["F"].T @ (np.asarray(pt) - bb["pos"])]
    contacts = {}
    hv = np.where((Wfull[:, col["head"]] > 0.5))[0]
    hv = hv[np.isin(hv, body_v)]
    contacts["head"] = {"top": local("head", V[hv[np.argmax(V[hv, 1])]])}
    for s_ in "lr":
        mh = SIDES[s_]
        fv_ = np.where(wsum("an_" + s_) > 0.5)[0]
        fv_ = fv_[np.isin(fv_, body_v)]
        low = fv_[V[fv_, 1] < V[fv_, 1].min() + 0.01]
        heel = V[low[np.argmin(V[low, 2])]].copy()
        toe = V[low[np.argmax(V[low, 2])]].copy()
        heel[1] = toe[1] = 0.0
        contacts["an_" + s_] = {"heel": local("an_" + s_, heel), "toe": local("an_" + s_, toe)}
        contacts["f12_" + s_] = {"tip": local("f12_" + s_, J["finger3-3%s____tail" % mh])}
        contacts["wr_" + s_] = {"knuckles": local("wr_" + s_, J["finger3-1%s____head" % mh])}
    json.dump({"bones": rig, "stats": stats, "contacts": contacts}, open(os.path.join(OUT, name + "_rig.json"), "w"), indent=1)
    json.dump(stats, open(os.path.join(OUT, name + "_stats.json"), "w"), indent=1)
    return stats


def _w_of(P, V, Wfull, pool):
    P = np.atleast_2d(P)
    d = np.linalg.norm(V[pool][None, :, :] - P[:, None, :], axis=2)
    return Wfull[pool[np.argmin(d, axis=1)]]


class _Named(tuple):
    def __new__(cls, name, material, o):
        t = super().__new__(cls, o)
        t.name, t.material = name, material
        return t


def main():
    os.makedirs(OUT, exist_ok=True)
    manifest = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sources.json")
    if os.path.exists(manifest):
        for rel, h in json.load(open(manifest))["sha256"].items():
            path = rel if os.path.isabs(rel) else os.path.join(DL, rel)
            if os.path.exists(path) and sha(path) != h:
                raise SystemExit("hash mismatch: " + rel)
    V0, VT, faces = load_obj(os.path.join(DL, "3dobjs/base.obj"))
    skel = json.load(open(os.path.join(DL, "rigs/default.mhskel")))
    wfile = json.load(open(os.path.join(DL, "rigs/default_weights.mhw")))["weights"]
    allstats = {}
    for name in PRESETS_WANTED:
        if name.endswith("_bare"):   # the same body undressed, for the lineup
            pr = dict(PRESETS[name[:-5]])
            pr["dress"] = False
        else:
            pr = PRESETS[name]
        allstats[name] = build(name, pr, V0, VT, faces, skel, wfile)
        s = allstats[name]
        print("%-10s h=%.2f m  bmi=%.1f mass=%.0f kg  waist=%.0f hip=%.0f bust=%.0f arm=%.1f calf=%.1f thigh=%.1f cm  tris=%d" % (
            name, s["stand_m"], s["bmi"], s["mass_kg"], s["waist_cm"], s["hip_cm"], s["bust_cm"], s["upper_arm_cm"],
            s["calf_cm"], s["thigh_cm"], s["triangles_total"]))
    json.dump(allstats, open(os.path.join(OUT, "lineup_stats.json"), "w"), indent=1)


if __name__ == "__main__":
    main()
