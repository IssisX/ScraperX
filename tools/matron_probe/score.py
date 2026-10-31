#!/usr/bin/env python3
"""Score the Matron as she is drawn, from a recording made by probe.gd.

    python3 tools/matron_probe/score.py <run dir> [--stride N] [--no-replay]
    python3 tools/matron_probe/score.py compare <run dir> <run dir> ...

Her drawn body is computed here the way the renderer computes it: every part
of her (the skinned meshes of matron.glb as the game loaded them) is skinned
on the CPU from the frame's posed skeleton, with each part's own skin (its bind
poses) and each vertex's four bone weights, linear blend skinning as Godot's
does it (vertex = sum of weight * skeleton world * bone pose * bind pose *
rest vertex). Nothing is read from her collision proxies or control points
except where a scorer says so (the hair causes below).

What it scores, each written to <run>/score/ (CSV) and summed up in
<run>/score/summary.txt:

  penetration  every drawn hair point against her drawn body (skin, uniform,
               eyes; not hair, brows, lashes, mouth or teeth), and every point
               of her skirt (the only clothing that moves off her body; the
               "skirt" and "rags" pivots drive it, the "cape" pivot has no
               mesh) against the rest of her: depth, body part, her state, how
               often and how long. A hair point is a strand's middle (its two
               ribbon edges, 1-6 mm apart, averaged). A hit is a point more
               than 3 mm inside her surface that was not already inside it in
               her bind pose.
  causes       each hair hit classed: "outside the proxies" (a hair solver
               point is itself inside her body where neither of the solver's
               two spheres, skull and upper back, reaches), "segment crossing"
               (the solver's points on either side are legal, outside the
               spheres and her body, and the segment or the skinned strand
               between them crosses a sphere or her body), "lag" (gone when
               her pose is held still and her hair let settle under the same
               rules; and, across runs, rates at 60, 30 and 20 fps) and
               "rest direction" (gone when the solver's stiffness pulls toward
               world down instead of her head's down). The replays run a copy
               of matron_rig.gd step_hair over the recorded skeleton; the copy
               is checked against the recorded hair every frame.
  plants       planted palms and feet: skate (moving in the world while
               planted, threshold max(2.5 v dt, 5 cm) a frame), float (her
               lowest limb above the surface under it while she is on a
               floor), and limbs through the steel.
  grips        her hand to what it holds: the ankle in a hold (<= 0.10 m from
               0.3 s into it), a rung or bar while she climbs.
  joints       joint rate (<= 12 rad/s outside flagged bursts) and root step
               (<= max(1.5 v dt, 5 cm) a frame).
"""
import argparse
import csv
import json
import math
import os
import sys
import time

import numpy as np

STATE_NAMES = {0: "Dormant", 1: "Straining", 2: "Emerging", 3: "Hunting", 4: "Searching", 5: "Shuffling", 6: "Dead",
               7: "Guarding", 8: "Peering", 9: "Unrigging", 10: "Staggering", 11: "Down", 12: "Grab", 13: "Carry",
               14: "Dangle", 15: "ArmPlay", 16: "Release", 17: "Spare", 18: "Claimed"}
HOLD_STATES = (12, 13, 14, 15, 16, 17, 18)
HIT_M = 0.003            # deeper than this inside her surface is a hit
VARIANT_NAMES = {"head": "rig's rules", "world": "rest toward world down", "unclamped": "no 1/30 s cap"}
GRID_M = 0.04            # the nearest-surface search's cell
HAIR_REACH_M = 0.75      # body farther than this from her head is never reached by her hair
NOT_FOR_HAIR = {"hair", "brows", "lashes", "teeth", "mouth", "cornea", "skirt_cloth"}
NOT_FOR_SKIRT = {"hair", "brows", "lashes", "teeth", "mouth", "cornea", "skirt_cloth", "eyes"}
LEG_BONES = ("hp_", "kn_", "an_")
PLANT_M = 0.03           # a limb this close to the surface under it is on it
FLOAT_M = 0.03
SINK_M = 0.03
JOINT_RAD_S = 12.0
GRIP_M = 0.10


# --- reading a recording ---------------------------------------------------------------------------------------

def xf(a):
    """12 floats (basis x, y, z columns, then the origin) -> (..., 3, 4)."""
    a = np.asarray(a, dtype=np.float64)
    return np.swapaxes(a.reshape(a.shape[:-1] + (4, 3)), -1, -2)


def compose(a, b):
    """Affine a then b: (..., 3, 4) x (..., 3, 4)."""
    r = a[..., :3, :3] @ b[..., :3, :3]
    t = (a[..., :3, :3] @ b[..., :3, 3:4])[..., 0] + a[..., :3, 3]
    return np.concatenate([r, t[..., None]], axis=-1)


def apply(m, p):
    return p @ m[:3, :3].T + m[:3, 3]


def inverse(m):
    r = np.linalg.inv(m[..., :3, :3])
    t = -(r @ m[..., :3, 3:4])[..., 0]
    return np.concatenate([r, t[..., None]], axis=-1)


class Part:
    pass


class Run:
    def __init__(self, path):
        self.path = path
        with open(os.path.join(path, "meta.json")) as fh:
            self.meta = json.load(fh)
        m = self.meta
        self.bone_names = [b["name"] for b in m["bones"]]
        self.bone_index = {n: i for i, n in enumerate(self.bone_names)}
        self.parent = np.array([b["parent"] for b in m["bones"]])
        self.pose_position = np.array([b["pose_position"] for b in m["bones"]], dtype=np.float64)
        self.parts = {}
        raw = open(os.path.join(path, "mesh.bin"), "rb").read()
        for pd in m["parts"]:
            p = Part()
            p.name = pd["name"]
            n, t, k = pd["vertices"], pd["triangles"], pd["bones_per_vertex"]
            at = pd["offset"]

            def take(count, dtype, at_):
                arr = np.frombuffer(raw, dtype=dtype, count=count, offset=at_)
                return arr, at_ + arr.nbytes
            v, at = take(n * 3, "<f4", at)
            nn, at = take(n * 3, "<f4", at)
            uv, at = take(n * 2, "<f4", at)
            bi, at = take(n * k, "<i4", at)
            bw, at = take(n * k, "<f4", at)
            ix, at = take(t * 3, "<i4", at)
            p.v = v.reshape(n, 3).astype(np.float64)
            p.n = nn.reshape(n, 3).astype(np.float64)
            p.uv = uv.reshape(n, 2).astype(np.float64)
            p.bind_index = bi.reshape(n, k)
            p.w = bw.reshape(n, k).astype(np.float64)
            p.tris = ix.reshape(t, 3)
            p.binds = np.array(pd["binds"])
            p.bind_poses = xf(np.array(pd["bind_poses"]))
            p.bone = p.binds[p.bind_index]          # each influence's skeleton bone
            assert np.allclose(np.array(pd["to_skeleton"]), [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0], atol=1e-5), \
                "part %s is not at its skeleton" % p.name
            self.parts[p.name] = p
        self.rows = read_frames(os.path.join(path, "frames.csv"))
        nb = len(self.bone_names)
        h = m["hair"]
        self.nc = len(h["chains"])
        self.np_ = int(h["chain_points"])
        F = int(m["record_floats"])
        rec = np.fromfile(os.path.join(path, "bones.bin"), dtype="<f4")
        rec = rec[: (len(rec) // F) * F].reshape(-1, F)
        self.nrec = len(rec)
        self.skel = xf(rec[:, :12])
        self.bones_raw = rec[:, 12:12 + 12 * nb]
        k = 12 + 12 * nb
        cp = self.nc * self.np_ * 3
        self.pts = rec[:, k:k + cp].reshape(-1, self.nc, self.np_, 3).astype(np.float64)
        self.prev = rec[:, k + cp:k + 2 * cp].reshape(-1, self.nc, self.np_, 3).astype(np.float64)
        self.wind_clock = rec[:, k + 2 * cp].astype(np.float64)
        self.fps = int(m.get("fixed_fps") or 0) or int(round(1.0 / np.median(self.rows["dt"][1:])))
        self.chains = h["chains"]
        self.hc = h["consts"]
        self.skull_local = np.array(h["skull_local"])
        self.skull_r = float(h["skull_radius"])
        self.back_local = np.array(h["back_local"])
        self.back_r = float(self.hc.get("BACK_RADIUS", 0.17))
        self.contacts = m["contacts"]
        # record index -> frame row
        self.row_of_rec = np.full(self.nrec, -1)
        r = self.rows["rec"].astype(int)
        ok = (r >= 0) & (r < self.nrec)
        self.row_of_rec[r[ok]] = np.nonzero(ok)[0]

    def limit(self, lo, hi):
        keep = (np.arange(self.nrec) >= lo) & (np.arange(self.nrec) < hi)
        self.row_of_rec = np.where(keep, self.row_of_rec, -1)

    def bones(self, i):
        """Bone poses of record i in skeleton space, (B, 3, 4)."""
        return xf(self.bones_raw[i].reshape(-1, 12))

    def world_bones(self, i):
        return compose(self.skel[i][None], self.bones(i))


def read_frames(path):
    with open(path) as fh:
        rd = csv.reader(fh)
        head = next(rd)
        cols = {h: [] for h in head}
        for row in rd:
            if len(row) != len(head):
                continue
            for h, v in zip(head, row):
                cols[h].append(v)
    out = {}
    for h, vals in cols.items():
        if h in ("climb_move",):
            out[h] = np.array(vals)
            continue
        out[h] = np.array([float(v) if v not in ("", "nan") else np.nan for v in vals])
    return out


# --- her drawn body ---------------------------------------------------------------------------------------------

def skin(part, world_bones, sel=None, normals=False):
    """The part's vertices (or the selected ones) as drawn: linear blend skinning."""
    mats = compose(world_bones[part.binds], part.bind_poses)         # (binds, 3, 4)
    bi = part.bind_index if sel is None else part.bind_index[sel]
    w = part.w if sel is None else part.w[sel]
    v = part.v if sel is None else part.v[sel]
    m = np.einsum("nk,nkij->nij", w, mats[bi])
    pos = np.einsum("nij,nj->ni", m[:, :, :3], v) + m[:, :, 3]
    if not normals:
        return pos
    n0 = part.n if sel is None else part.n[sel]
    nrm = np.einsum("nij,nj->ni", m[:, :, :3], n0)
    nrm /= np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-9)
    return pos, nrm


def dominant_bone(part):
    return part.bone[np.arange(len(part.v)), np.argmax(part.w, axis=1)]


def region_names(run, part, dom):
    """A plain body-part name for each vertex, from the bone that moves it most."""
    names = []
    bind_at = {}
    for b in range(len(run.bone_names)):
        # the bone's bind-pose origin, from any part's bind pose for it
        j = np.nonzero(part.binds == b)[0]
        if len(j):
            bind_at[b] = inverse(part.bind_poses[j[0]])[:, 3]
    for i, b in enumerate(dom):
        n = run.bone_names[b]
        side = " L" if n.endswith("_l") or n.endswith("_l_x") else " R" if n.endswith("_r") or n.endswith("_r_x") else ""
        front = part.v[i, 2] > bind_at.get(b, np.zeros(3))[2] + 0.02
        if n in ("head", "jaw") or n.startswith("eye"):
            r = "head"
        elif n.startswith("neck"):
            r = "neck"
        elif n in ("chest", "hump", "spine_mid"):
            r = "chest" if front else "upper back"
        elif n in ("spine_lo", "body", "box"):
            r = "belly" if front else "lower back"
        elif n.startswith("sh_"):
            r = ("shoulder" if np.linalg.norm(part.v[i] - bind_at.get(b, part.v[i])) < 0.10 else "upper arm") + side
        elif n.startswith("el_"):
            r = "forearm" + side
        elif n.startswith("wr_") or n[:1] in "ft" and n[1:2].isdigit():
            r = "hand" + side
        elif n.startswith("hp_"):
            r = "thigh" + side
        elif n.startswith("kn_"):
            r = "shin" + side
        elif n.startswith("an_"):
            r = "foot" + side
        elif n == "skirt":
            r = "skirt"
        elif n.startswith("hair"):
            r = "hair"
        else:
            r = n
        names.append(r)
    return np.array(names)


class Body:
    """A set of her parts as one surface: vertices, outward normals, the body part of each."""

    def __init__(self, run, names, keep=None):
        self.parts = [run.parts[n] for n in names]
        self.sel, self.region, self.part = [], [], []
        for p in self.parts:
            dom = dominant_bone(p)
            sel = np.ones(len(p.v), bool) if keep is None else keep(run, p, dom)
            self.sel.append(np.nonzero(sel)[0])
            self.region.append(region_names(run, p, dom[sel]))
            self.part.append(np.array([p.name] * int(sel.sum())))
        self.region = np.concatenate(self.region)
        self.part = np.concatenate(self.part)
        self.rest = np.concatenate([p.v[s] for p, s in zip(self.parts, self.sel)])
        self.rest_n = np.concatenate([p.n[s] for p, s in zip(self.parts, self.sel)])

    def drawn(self, world_bones):
        ps, ns = [], []
        for p, s in zip(self.parts, self.sel):
            a, b = skin(p, world_bones, s, normals=True)
            ps.append(a)
            ns.append(b)
        return np.concatenate(ps), np.concatenate(ns)


# --- the nearest surface: a hash grid over the drawn vertices ---------------------------------------------------

def _keys(c):
    c = c + (1 << 20)
    return ((c[..., 0] << 42) | (c[..., 1] << 21) | c[..., 2]).astype(np.uint64)


_KERNEL = None


def _kernel():
    """nearest.c, compiled once into the temporary directory (or $MATRON_PROBE_CACHE); None without a compiler."""
    global _KERNEL
    if _KERNEL is not None:
        return _KERNEL or None
    import ctypes
    import hashlib
    import subprocess
    import tempfile
    src = os.path.join(os.path.dirname(os.path.abspath(__file__)), "nearest.c")
    try:
        tag = hashlib.sha1(open(src, "rb").read()).hexdigest()[:12]
        cache = os.environ.get("MATRON_PROBE_CACHE") or tempfile.gettempdir()
        so = os.path.join(cache, "matron_probe_nearest_%s.so" % tag)
        if not os.path.exists(so):
            tmp = "%s.%d" % (so, os.getpid())
            subprocess.run([os.environ.get("CC", "cc"), "-O2", "-shared", "-fPIC", "-o", tmp, src, "-lm"],
                           check=True, capture_output=True)
            os.replace(tmp, so)
        f = ctypes.CDLL(so).nearest
        d = np.ctypeslib.ndpointer(np.float64, flags="C_CONTIGUOUS")
        u = np.ctypeslib.ndpointer(np.uint64, flags="C_CONTIGUOUS")
        i = np.ctypeslib.ndpointer(np.int64, flags="C_CONTIGUOUS")
        f.argtypes = [d, d, u, i, i, ctypes.c_int64, i, ctypes.c_double, ctypes.c_int64, d, ctypes.c_int64, i, d, d]
        f.restype = None
        _KERNEL = f
    except Exception as e:      # no compiler, or it failed: the numpy search below
        print("matron probe: nearest.c not built (%s); the numpy search is slower" % e)
        _KERNEL = False
    return _KERNEL or None


class Grid:
    """Her drawn vertices (and their outward normals) in a uniform grid of cell h."""

    def __init__(self, pts, nrm, h=0.04):
        self.h = h
        self.pts = np.ascontiguousarray(pts, dtype=np.float64)
        self.nrm = np.ascontiguousarray(nrm, dtype=np.float64)
        key = _keys(np.floor(self.pts / h).astype(np.int64))
        self.order = np.argsort(key, kind="stable").astype(np.int64)
        u, st, ct = np.unique(key[self.order], return_index=True, return_counts=True)
        self.uniq = np.ascontiguousarray(u, dtype=np.uint64)
        self.start = st.astype(np.int64)
        self.count = ct.astype(np.int64)

    def signed(self, q, ring=1):
        """For each query: the signed distance to her surface (negative inside: the depth along the nearest
        vertex's normal, never more than the distance to it), the nearest vertex, and whether that vertex
        is surely the nearest (within ring * h; farther ones are not searched for)."""
        q = np.ascontiguousarray(q, dtype=np.float64)
        n = len(q)
        vi = np.full(n, -1, dtype=np.int64)
        d = np.full(n, np.inf)
        s = np.full(n, np.inf)
        if n == 0 or len(self.uniq) == 0:
            return s, vi, np.zeros(n, bool)
        k = _kernel()
        if k is not None:
            k(self.pts, self.nrm, self.uniq, self.start, self.count, len(self.uniq), self.order, self.h, ring,
              q, n, vi, d, s)
        else:
            vi, d = self._nearest_numpy(q, ring)
            f = vi >= 0
            rel = q[f] - self.pts[vi[f]]
            along = np.einsum("ij,ij->i", rel, self.nrm[vi[f]])
            s[f] = np.where(along < 0.0, np.maximum(along, -d[f]), d[f])
        return s, vi, d <= ring * self.h

    def _nearest_numpy(self, q, ring):
        r = np.arange(-ring, ring + 1)
        offs = np.array([(x, y, z) for x in r for y in r for z in r], dtype=np.int64)
        best = np.full(len(q), np.inf)
        best_i = np.full(len(q), -1, dtype=np.int64)
        cq = np.floor(q / self.h).astype(np.int64)
        nu = len(self.uniq)
        for off in offs:
            k = _keys(cq + off)
            pos = np.minimum(np.searchsorted(self.uniq, k), nu - 1)
            qi = np.nonzero(self.uniq[pos] == k)[0]
            if len(qi) == 0:
                continue
            st = self.start[pos[qi]]
            n = self.count[pos[qi]]
            tot = int(n.sum())
            bstart = np.cumsum(n) - n
            vi = self.order[np.repeat(st - bstart, n) + np.arange(tot)]
            rq = np.repeat(qi, n)
            e = q[rq] - self.pts[vi]
            d2 = np.einsum("ij,ij->i", e, e)
            mind = np.minimum.reduceat(d2, bstart)
            blk = np.repeat(np.arange(len(qi)), n)
            first = np.minimum.reduceat(np.where(d2 == mind[blk], np.arange(tot), tot), bstart)
            better = mind < best[qi]
            best[qi[better]] = mind[better]
            best_i[qi[better]] = vi[first[better]]
        return best_i, np.sqrt(best)


# --- her hair: strands, chains, the solver's copy ----------------------------------------------------------------

class Hair:
    """Her hair mesh as strands: each ribbon's two edges paired along it into middle points, ordered from
    the root; and for each middle point the solver chain and segment that move it most."""

    def __init__(self, run):
        p = run.parts["hair"]
        self.part = p
        n = len(p.v)
        # strands: the ribbons' connected pieces
        lab = np.arange(n)
        t = p.tris
        while True:
            m = np.minimum(np.minimum(lab[t[:, 0]], lab[t[:, 1]]), lab[t[:, 2]])
            new = lab.copy()
            for c in range(3):
                np.minimum.at(new, t[:, c], m)
            new = new[new]
            if np.array_equal(new, lab):
                break
            lab = new
        u = p.uv[:, 0] > 0.5
        order = np.lexsort((p.uv[:, 1], u, lab))
        lab_o, u_o = lab[order], u[order]
        pairs_a, pairs_b, strand = [], [], []
        i = 0
        sid = 0
        while i < n:
            j = i
            while j < n and lab_o[j] == lab_o[i]:
                j += 1
            seg = order[i:j]
            left = seg[~u_o[i:j]]
            right = seg[u_o[i:j]]
            m_ = min(len(left), len(right))
            if m_ >= 2:
                pairs_a.append(left[:m_])
                pairs_b.append(right[:m_])
                strand.append(np.full(m_, sid))
                sid += 1
            i = j
        self.a = np.concatenate(pairs_a)
        self.b = np.concatenate(pairs_b)
        self.strand = np.concatenate(strand)
        self.nstrands = sid
        self.npts = len(self.a)
        # position of each middle point along its strand (0 = the root)
        first = np.r_[0, np.nonzero(np.diff(self.strand))[0] + 1]
        self.along = np.arange(self.npts) - np.repeat(first, np.diff(np.r_[first, self.npts]))
        self.first = first
        # the solver bone each point hangs from: chain, segment (-1 for points the head alone moves)
        hair_bone = {}
        for c, ch in enumerate(run.chains):
            for s, b in enumerate(ch["idx"]):
                hair_bone[int(b)] = (c, s)
        w = p.w[self.a]
        bones = p.bone[self.a]
        is_hair = np.vectorize(lambda b: int(b) in hair_bone)(bones)
        wh = np.where(is_hair, w, 0.0)
        top = np.argmax(wh, axis=1)
        self.rigid = wh.max(axis=1) < 0.05
        best = bones[np.arange(self.npts), top]
        self.chain = np.array([hair_bone.get(int(b), (-1, -1))[0] for b in best])
        self.seg = np.array([hair_bone.get(int(b), (-1, -1))[1] for b in best])

    def middles(self, pos):
        return 0.5 * (pos[self.a] + pos[self.b])

    def fill_along(self, status):
        """Status (+1 out, -1 in, 0 unknown) of unknown points from the known point nearest the root
        before them on their strand, else after: a strand cannot pass through her surface between two
        points both farther than the search from every vertex of it."""
        st = status.copy()
        idx = np.arange(self.npts)
        known = st != 0
        # forward: last known index at or before i on the same strand
        last = np.where(known, idx, -1)
        last = np.maximum.accumulate(last)
        ok = (last >= 0) & (self.strand[np.maximum(last, 0)] == self.strand)
        st = np.where(~known & ok, st[np.maximum(last, 0)], st)
        known2 = st != 0
        nxt = np.where(known2, idx, self.npts)
        nxt = np.minimum.accumulate(nxt[::-1])[::-1]
        ok2 = (nxt < self.npts) & (self.strand[np.minimum(nxt, self.npts - 1)] == self.strand)
        st = np.where(~known2 & ok2, st[np.minimum(nxt, self.npts - 1)], st)
        st[st == 0] = 1
        return st


def quat_arc_matrix(a, b):
    """Rotation matrices taking unit a to unit b (Godot's Quaternion(arc_from, arc_to)), (n, 3, 3)."""
    c = np.cross(a, b)
    d = np.einsum("ij,ij->i", a, b)
    s = np.sqrt(np.maximum((1.0 + d) * 2.0, 1e-30))
    q = np.concatenate([c / s[:, None], (s * 0.5)[:, None]], axis=1)   # x y z w
    q /= np.linalg.norm(q, axis=1, keepdims=True)
    x, y, z, w = q.T
    return np.stack([
        np.stack([1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)], -1),
        np.stack([2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)], -1),
        np.stack([2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)], -1)], 1)


class HairSolver:
    """A copy of matron_rig.gd step_hair. Segments stay outside the skull and the
    upper-back sphere, walk sideways-and-down out of the collar, belt and neck
    cylinders, then take a horizontal step back from the chest-front slope.
    rest="head" pulls toward the head's down; rest="world" toward
    world down. clamp=False drops the 1/30 s cap."""

    def __init__(self, run, rest="head", clamp=True):
        self.run = run
        self.rest = rest
        self.clamp = clamp
        hc = run.hc
        self.damp_base = float(hc.get("HAIR_DAMP", 0.84))
        self.g = np.array(hc.get("HAIR_GRAVITY", [0.0, -9.81, 0.0]))
        self.stiff = np.array(hc.get("HAIR_STIFFNESS", [0.22, 0.12, 0.07, 0.04, 0.02]))
        self.iters = int(hc.get("HAIR_ITERATIONS", 3))
        self.jump = float(hc.get("HAIR_JUMP_M", 1.0))
        self.wind_ms2 = float(hc.get("WIND_MS2", 0.7))
        self.wind_rate = float(hc.get("WIND_RATE", 0.23))
        self.wind_dir = np.array(hc.get("WIND_DIR", [0.93, 0.0, 0.37]))
        ch = run.chains
        self.root_local = np.array([c["root_local"] for c in ch])
        self.seg = np.array([float(c["seg"]) for c in ch])
        self.damp = np.array([float(c["damp"]) for c in ch])
        self.give = np.array([float(c["give"]) for c in ch])
        self.phase = np.array([float(c["phase"]) for c in ch])
        self.idx = np.array([c["idx"] for c in ch])
        self.head = run.bone_index["head"]
        self.chest = run.bone_index["chest"]
        self.belt = run.bone_index["spine_mid"]
        self.neck = run.bone_index["neck_lo"]
        self.n = self.idx.shape[1]

    def spheres(self, wb):
        head, chest = wb[self.head], wb[self.chest]
        return apply(head, self.run.skull_local), apply(chest, self.run.back_local)

    def step(self, pts, prev, wb, delta, wind_clock, wind=True):
        """One frame: pts, prev (C, n+1, 3) updated in place; returns the new wind clock."""
        dt = min(max(delta, 1.0 / 240.0), 1.0 / 30.0) if self.clamp else max(delta, 1.0 / 240.0)
        wind_clock += dt
        head = wb[self.head]
        skull, back = self.spheres(wb)
        hb = head[:3, :3]
        down = hb @ np.array([0.0, -1.0, 0.0])
        rest_down = down if self.rest == "head" else np.array([0.0, -1.0, 0.0])
        root = self.root_local @ hb.T + head[:3, 3]
        n = self.n
        jumped = np.linalg.norm(pts[:, 0] - root, axis=1) > self.jump
        jumped |= ~np.isfinite(pts).all(axis=(1, 2))
        if jumped.any():
            k = np.arange(n + 1)
            hang = root[jumped][:, None, :] + down[None, None, :] * (self.seg[jumped][:, None, None] * k[None, :, None])
            pts[jumped] = hang
            prev[jumped] = hang
        pts[:, 0] = root
        prev[:, 0] = root
        if wind:
            gust = max(0.0, math.sin(math.tau * self.wind_rate * wind_clock)) * \
                (0.6 + 0.4 * np.sin(math.tau * self.wind_rate * 3.1 * wind_clock + self.phase))
        else:
            gust = np.zeros(len(self.phase))
        wvec = self.wind_dir[None, :] * self.wind_ms2 * gust[:, None]
        for k in range(1, n + 1):
            cur = pts[:, k].copy()
            vel = (cur - prev[:, k]) * self.damp[:, None]
            prev[:, k] = cur
            pts[:, k] = cur + vel + (self.g[None, :] + wvec * (k / n)) * dt * dt
        for _ in range(self.iters):
            for k in range(1, n + 1):
                above = pts[:, k - 1]
                target = above + rest_down[None, :] * self.seg[:, None]
                wgt = (self.stiff[k - 1] * self.give / self.iters)[:, None]
                q = pts[:, k] + (target - pts[:, k]) * wgt
                d = q - above
                dl = np.linalg.norm(d, axis=1, keepdims=True)
                unit = np.where(dl > 1e-6, d / np.maximum(dl, 1e-12), rest_down[None, :])
                q = above + unit * self.seg[:, None]
                q = _seg_out_batch(above, q, self.seg, skull, self.run.skull_r)
                q = _seg_out_batch(above, q, self.seg, back, self.run.back_r)
                q = _ring_batch(above, q, self.seg, wb[self.chest], 0.091, 0.154, 0.138)
                q = _ring_batch(above, q, self.seg, wb[self.belt], -0.001, 0.042, 0.192)
                q = _ring_batch(above, q, self.seg, wb[self.neck], -0.01, 0.105, 0.095)
                q = _front_batch(above, q, self.seg, wb[self.chest])
                pts[:, k] = q
        return wind_clock

    def bones_from(self, pts, wb):
        """The hair bones' world transforms from the points, as step_hair turns them."""
        out = wb.copy()
        head = wb[self.head]
        basis = np.repeat(head[None, :3, :3], len(pts), axis=0)
        origin = np.repeat(head[None, :3, 3], len(pts), axis=0)
        down = np.array([0.0, -1.0, 0.0])
        for k in range(self.n):
            bi = self.idx[:, k]
            o = origin + np.einsum("cij,cj->ci", basis, self.run.pose_position[bi])
            dirv = pts[:, k + 1] - pts[:, k]
            dirv /= np.maximum(np.linalg.norm(dirv, axis=1, keepdims=True), 1e-12)
            local = np.einsum("cji,cj->ci", basis, dirv)     # basis^T dir (the bases are rotations)
            local /= np.maximum(np.linalg.norm(local, axis=1, keepdims=True), 1e-12)
            r = quat_arc_matrix(np.repeat(down[None], len(pts), 0), local)
            flip = local @ down <= -0.999
            if flip.any():
                r[flip] = np.diag([1.0, -1.0, -1.0])
            basis = basis @ r
            out[bi] = np.concatenate([basis, o[:, :, None]], axis=2)
            origin = o
        return out


def _outside(q, centre, radius):
    d = q - centre[None, :]
    ln = np.linalg.norm(d, axis=1)
    inside = (ln < radius) & (ln >= 1e-6)
    q = q.copy()
    q[inside] = centre[None, :] + d[inside] / ln[inside, None] * radius
    return q


def _seg_hits(a, d_unit, length, centre, radius):
    oc = a - centre
    bb = 2.0 * np.dot(oc, d_unit)
    cc = np.dot(oc, oc) - radius * radius
    disc = bb * bb - 4.0 * cc
    if disc < 0.0:
        return False
    s = math.sqrt(disc)
    t1 = (-bb - s) * 0.5
    t2 = (-bb + s) * 0.5
    return t1 < length and t2 > 0.0


def _seg_out_one(a, b, length, centre, radius):
    """Same rule as matron_rig.gd _segment_outside."""
    if np.linalg.norm(a - centre) < radius:
        d = b - centre
        ln = np.linalg.norm(d)
        pushed = b if ln >= radius or ln < 1e-6 else centre + d / ln * radius
        pd = pushed - a
        if np.linalg.norm(pd) < 1e-6:
            pd = np.array([0.0, -1.0, 0.0])
        return a + pd / np.linalg.norm(pd) * length
    d = b - a
    if np.linalg.norm(d) < 1e-6:
        d = np.array([0.0, -1.0, 0.0])
    d = d / np.linalg.norm(d)
    if not _seg_hits(a, d, length, centre, radius):
        return a + d * length
    toward = centre - a
    axis = np.cross(toward, d)
    if np.linalg.norm(axis) < 1e-6:
        axis = np.cross(toward, np.array([0.0, 1.0, 0.0]))
        if np.linalg.norm(axis) < 1e-6:
            axis = np.cross(toward, np.array([1.0, 0.0, 0.0]))
    axis = axis / np.linalg.norm(axis)
    lo, hi = 0.0, math.pi
    for _ in range(16):
        mid = (lo + hi) * 0.5
        trial = _rodrigues(axis, mid, d)
        trial = trial / np.linalg.norm(trial)
        if _seg_hits(a, trial, length, centre, radius):
            lo = mid
        else:
            hi = mid
    out = _rodrigues(axis, hi, d)
    return a + out / np.linalg.norm(out) * length


def _rodrigues(axis, ang, v):
    c, s = math.cos(ang), math.sin(ang)
    return v * c + np.cross(axis, v) * s + axis * np.dot(axis, v) * (1.0 - c)


def _seg_out_batch(a, b, length, centre, radius):
    out = np.empty_like(b)
    for i in range(len(b)):
        out[i] = _seg_out_one(a[i], b[i], float(length[i]), centre, float(radius))
    return out


def _ring_depth(a, b, origin, axis, y0, y1, radius):
    worst = 0.0
    for s in range(8):
        p = a + (b - a) * (s / 7.0)
        rel = p - origin
        along = float(np.dot(rel, axis))
        if along < y0 - 0.01 or along > y1 + 0.01:
            continue
        radial = rel - axis * along
        worst = max(worst, radius - float(np.linalg.norm(radial)))
    return worst


def _ring_one(a, b, length, bone, y0, y1, radius):
    origin = bone[:3, 3]
    axis = bone[:3, 1]
    al = np.linalg.norm(axis)
    if al < 1e-8:
        return b
    axis = axis / al
    for _ in range(6):
        if _ring_depth(a, b, origin, axis, y0, y1, radius) <= 0.001:
            return b
        best = 1.0e9
        out = np.zeros(3)
        for s in range(8):
            p = a + (b - a) * (s / 7.0)
            rel = p - origin
            along = float(np.dot(rel, axis))
            if along < y0 - 0.01 or along > y1 + 0.01:
                continue
            radial = rel - axis * along
            rl = float(np.linalg.norm(radial))
            if rl < best:
                best = rl
                out = radial
        if np.linalg.norm(out) < 1e-4:
            out = np.array([a[0] - origin[0], 0.0, a[2] - origin[2]])
        horiz = np.array([out[0], 0.0, out[2]])
        if np.linalg.norm(horiz) < 1e-4:
            horiz = np.array([0.0, 0.0, -1.0])
        horiz = horiz / np.linalg.norm(horiz)
        d = b - a
        if np.linalg.norm(d) < 1e-6:
            d = np.array([0.0, -1.0, 0.0])
        d = d / np.linalg.norm(d)
        trial = d + horiz * 0.45
        trial[1] = min(trial[1], -0.15)
        if np.linalg.norm(trial) < 1e-6:
            trial = np.array([0.0, -1.0, 0.0])
        b = a + trial / np.linalg.norm(trial) * length
    return b


def _ring_batch(a, b, length, bone, y0, y1, radius):
    out = np.empty_like(b)
    for i in range(len(b)):
        out[i] = _ring_one(a[i], b[i], float(length[i]), bone, y0, y1, radius)
    return out


def _chest_front_z(y):
    """Same knots as matron_rig.gd _chest_front_z."""
    if y <= -0.25:
        return 0.249
    if y <= -0.15:
        return 0.249 + (0.228 - 0.249) * (y + 0.25) / 0.10
    if y <= -0.05:
        return 0.228 + (0.179 - 0.228) * (y + 0.15) / 0.10
    if y <= 0.05:
        return 0.179 + (0.173 - 0.179) * (y + 0.05) / 0.10
    if y <= 0.12:
        return 0.173 + (0.116 - 0.173) * (y - 0.05) / 0.07
    if y <= 0.18:
        return 0.116 + (0.098 - 0.116) * (y - 0.12) / 0.06
    return 0.098


def _front_one(a, b, length, bone):
    """Same rule as matron_rig.gd _clear_front. Horizontal only, never up."""
    origin = bone[:3, 3]
    axis = bone[:3, 1]
    al = np.linalg.norm(axis)
    if al < 1e-8:
        return b
    axis = axis / al
    fwd = bone[:3, 2]
    fwd = fwd / max(np.linalg.norm(fwd), 1e-8)
    side = bone[:3, 0]
    side = side / max(np.linalg.norm(side), 1e-8)
    for _ in range(8):
        worst = 0.0
        sx = 0.0
        for s in range(8):
            p = a + (b - a) * (s / 7.0)
            rel = p - origin
            x = float(np.dot(rel, side))
            y = float(np.dot(rel, axis))
            z = float(np.dot(rel, fwd))
            if abs(x) > 0.16 or y < -0.25 or y > 0.18:
                continue
            ex = z - _chest_front_z(y)
            if ex > worst:
                worst = ex
                sx = x
        if worst <= 0.001:
            return b
        back = -fwd
        back[1] = min(back[1], 0.0)
        if np.linalg.norm(back) < 0.35:
            back = side * (1.0 if sx >= 0.0 else -1.0)
            back[1] = 0.0
        if np.linalg.norm(back) < 1e-6:
            back = np.array([0.0, 0.0, -1.0])
        back = back / np.linalg.norm(back)
        d = b - a
        if np.linalg.norm(d) < 1e-6:
            d = np.array([0.0, -1.0, 0.0])
        else:
            d = d / np.linalg.norm(d)
        gain = min(0.70, worst / length)
        trial = d + back * gain
        if trial[1] > -0.15:
            trial[1] = -0.15
        if np.linalg.norm(trial) < 1e-6:
            trial = np.array([0.0, -1.0, 0.0])
        b = a + trial / np.linalg.norm(trial) * length
    return b


def _front_batch(a, b, length, bone):
    out = np.empty_like(b)
    for i in range(len(b)):
        out[i] = _front_one(a[i], b[i], float(length[i]), bone)
    return out


# --- the scorer -------------------------------------------------------------------------------------------------

def mode_of(rows, i):
    flags = []
    if rows["climbing"][i] > 0:
        mv = str(rows["climb_move"][i]) if rows["climb_move_active"][i] > 0 else ""
        flags.append("climbing" + ((" " + mv) if mv else ""))
    for key, name in (("on_pipe", "pipe"), ("on_line", "line"), ("clinging", "clinging"), ("head_down", "head down"),
                      ("kneeling", "kneeling"), ("climb_airborne", "airborne"), ("bracing", "bracing")):
        if rows[key][i] > 0:
            flags.append(name)
    st = int(rows["state"][i])
    if st == 3 and rows["clinging"][i] <= 0 and ((rows["speed"][i] > 1.0 and rows["stage"][i] > 0) or rows["turn"][i] == 2):
        flags.append("all fours")
    if rows["turn"][i] == 1:
        flags.append("turn windup")
    elif rows["turn"][i] == 2:
        flags.append("turn lunge")
    if rows["fall_v"][i] < -6.0:
        flags.append("falling")
    return STATE_NAMES.get(st, str(st)) + ((" (" + ", ".join(flags) + ")") if flags else "")


def keep_upper(run, p, dom):
    names = np.array(run.bone_names)[dom]
    return ~np.char.startswith(names.astype(str), LEG_BONES[0]) & ~np.char.startswith(names.astype(str), LEG_BONES[1]) \
        & ~np.char.startswith(names.astype(str), LEG_BONES[2])


def keep_lower(run, p, dom):
    return p.v[:, 1] < 1.35


class Scorer:
    def __init__(self, run, stride=1, replay=True, log=print):
        self.run = run
        self.stride = stride
        self.replay = replay
        self.log = log
        self.hair = Hair(run)
        names = [n for n in run.parts if n not in NOT_FOR_HAIR]
        self.hair_body = Body(run, names, keep_upper)
        self.skirt_part = run.parts.get("skirt_cloth")
        self.skirt_body = Body(run, [n for n in run.parts if n not in NOT_FOR_SKIRT], keep_lower)
        self.solver = HairSolver(run, "head", True)
        self.rows = run.rows
        # bind pose: what is already inside by design (layered cloth, the scalp under the roots) never counts
        hb = self.hair_body
        g = Grid(hb.rest, hb.rest_n, GRID_M)
        hm = self.hair.middles(run.parts["hair"].v)
        s, _, _ = g.signed(hm)
        self.hair_bind_in = s < -HIT_M
        self.skirt_bind_in = np.zeros(0, bool)
        if self.skirt_part is not None:
            sb = self.skirt_body
            g2 = Grid(sb.rest, sb.rest_n, GRID_M)
            s2, _, _ = g2.signed(self.skirt_part.v)
            self.skirt_bind_in = s2 < -HIT_M
        self.foot_sel, self.palm_sel = self._limb_vertices()

    def _limb_vertices(self):
        run = self.run
        feet, palms = {}, {}
        shoes = run.parts.get("shoes")
        skinp = run.parts["skin"]
        for s in "lr":
            if shoes is not None:
                dom = np.array(run.bone_names)[dominant_bone(shoes)]
                feet[s] = ("shoes", np.nonzero(np.char.endswith(dom.astype(str), "_" + s))[0])
            dom = np.array(run.bone_names)[dominant_bone(skinp)].astype(str)
            hand = np.char.startswith(dom, "wr_" + s) | ((np.char.startswith(dom, "f") | np.char.startswith(dom, "t"))
                                                         & np.char.endswith(dom, "_" + s) & ~np.char.startswith(dom, "hp"))
            palms[s] = ("skin", np.nonzero(hand)[0])
        return feet, palms

    def hair_frame(self, wb, hair_pos=None, body_drawn=None):
        """Her hair's middle points against her body this frame: the points, their signed distance
        (deep points: the depth found by a wider search), the nearest body vertex, which are inside,
        which are deep (no vertex within the search, inside by their strand), and the body as drawn."""
        hair = self.hair
        if hair_pos is None:
            hair_pos = skin(hair.part, wb)
        q = hair.middles(hair_pos)
        head_o = wb[self.run.bone_index["head"]][:3, 3]
        body = self.hair_body
        bp, bn = body.drawn(wb) if body_drawn is None else body_drawn
        idx = np.nonzero(np.linalg.norm(bp - head_o, axis=1) < HAIR_REACH_M)[0]
        grid = Grid(bp[idx], bn[idx], GRID_M)
        s, vi_l, sure = grid.signed(q)
        status = np.where(sure, np.where(s < -HIT_M, -1, 1), 0)
        st = hair.fill_along(status)
        deep = (~sure) & (st < 0)
        if deep.any():
            d_i = np.nonzero(deep)[0]
            s3, v3, _ = grid.signed(q[d_i], ring=3)
            s[d_i] = np.where(np.isfinite(s3) & (s3 < 0), s3, -GRID_M)
            vi_l[d_i] = v3
        vi = np.where(vi_l >= 0, idx[np.maximum(vi_l, 0)], -1)
        inside = (st < 0) & ~self.hair_bind_in
        return q, s, vi, inside, deep, bp, bn, grid, idx

    def run_all(self):
        run, rows, hair = self.run, self.rows, self.hair
        t0 = time.time()
        recs = [r for r in range(run.nrec) if run.row_of_rec[r] >= 0]
        recs = recs[::self.stride]
        nfr = len(recs)
        self.frames = []
        self.hits = []          # (rec, row, set, region, part, depth, class, point)
        self.validate = []
        # the replays: a copy of the solver over the recorded skeleton, from the first scored frame on, with
        # the rig's own rules ("head", the control), its stiffness toward world down ("world"), and without
        # its 1/30 s cap on the step ("unclamped"); each checked a step at a time against the recording
        solvers = {"head": self.solver, "world": HairSolver(run, "world", True)}
        if np.nanmax(rows["dt"]) > 1.0 / 30.0 + 1e-6:
            solvers["unclamped"] = HairSolver(run, "head", False)
        self.variants = list(solvers) if self.replay else []
        rep = {}
        if self.replay and recs:
            r0 = recs[0]
            for name in solvers:
                rep[name] = [run.pts[r0].copy(), run.prev[r0].copy(), run.wind_clock[r0]]
        last_rec = None
        stepping = list(range(recs[0], recs[-1] + 1)) if self.replay and recs else recs
        rec_set = set(recs)
        for r in stepping:
            row = run.row_of_rec[r]
            if row < 0:
                continue
            wb = run.world_bones(r)
            if self.replay and last_rec is not None and rows["paused"][row] <= 0:
                for name, sv in solvers.items():
                    rep[name][2] = sv.step(rep[name][0], rep[name][1], wb, rows["dt"][row], rep[name][2])
                one = [run.pts[last_rec].copy(), run.prev[last_rec].copy()]
                self.solver.step(one[0], one[1], wb, rows["dt"][row], run.wind_clock[last_rec])
                self.validate.append((r, float(np.nanmax(np.linalg.norm(one[0] - run.pts[r], axis=2)))))
            last_rec = r
            if r not in rec_set:
                continue
            fr = self._score_frame(r, row, wb, rep if self.replay else None)
            self.frames.append(fr)
            if len(self.frames) % 500 == 0:
                self.log("  %d/%d frames, %.0f s" % (len(self.frames), nfr, time.time() - t0))
        self.elapsed = time.time() - t0

    def _score_frame(self, r, row, wb, rep):
        run, rows, hair = self.run, self.rows, self.hair
        fr = {"rec": r, "row": row, "t": rows["t"][row], "t_sim": rows["t_sim"][row], "dt": rows["dt"][row],
              "state": int(rows["state"][row]), "mode": mode_of(rows, row)}
        q, s, vi, inside, deep, bp, bn, grid, idx = self.hair_frame(wb)
        body = self.hair_body
        skull, back = self.solver.spheres(wb)
        fr["hair_n"] = int(inside.sum())
        fr["hair_regions"] = {}
        if inside.any():
            ii = np.nonzero(inside)[0]
            depth = -s[ii]
            reg = np.where(vi[ii] >= 0, body.region[np.maximum(vi[ii], 0)], "deep")
            prt = np.where(vi[ii] >= 0, body.part[np.maximum(vi[ii], 0)], "?")
            cls = self._classes(r, wb, q, ii, skull, back, grid, bp, bn, idx)
            for k, i in enumerate(ii):
                key = (reg[k], prt[k])
                cur = fr["hair_regions"].get(key)
                if cur is None or depth[k] > cur[0]:
                    fr["hair_regions"][key] = (float(depth[k]), cls[k], int(i), int(hair.chain[i]), int(hair.seg[i]))
            for key, (d, c, i, ch, sg) in fr["hair_regions"].items():
                n_in = int(np.sum((reg == key[0]) & (prt == key[1])))
                self.hits.append({"rec": r, "row": row, "t": fr["t"], "set": "hair", "region": key[0], "part": key[1],
                                  "depth": d, "class": c, "points": n_in, "chain": ch, "seg": sg})
        # the replays: the same frame's body, the hair from the copy of the solver
        if rep is not None:
            for name in rep:
                hb = self.solver.bones_from(rep[name][0], wb)
                _, _, vi2, ins2, _, _, _, _, _ = self.hair_frame(wb, skin(hair.part, hb), (bp, bn))
                fr["hair_n_" + name] = int(ins2.sum())
                fr["hair_regions_" + name] = {body.region[v] if v >= 0 else "deep" for v in vi2[ins2]}
        # the skirt against her legs and the rest of her
        fr["skirt_n"] = 0
        fr["skirt_regions"] = {}
        if self.skirt_part is not None:
            sp = skin(self.skirt_part, wb)
            sb = self.skirt_body
            bp2, bn2 = sb.drawn(wb)
            g2 = Grid(bp2, bn2, GRID_M)
            s2, vi2, found2 = g2.signed(sp, ring=2)
            ins = (vi2 >= 0) & (s2 < -HIT_M) & ~self.skirt_bind_in
            fr["skirt_n"] = int(ins.sum())
            if ins.any():
                ii = np.nonzero(ins)[0]
                reg = sb.region[vi2[ii]]
                prt = sb.part[vi2[ii]]
                for k, i in enumerate(ii):
                    key = (reg[k], prt[k])
                    d = float(-s2[i])
                    if key not in fr["skirt_regions"] or d > fr["skirt_regions"][key][0]:
                        fr["skirt_regions"][key] = (d, "outside the proxies", int(i))
                for key, (d, c, i) in fr["skirt_regions"].items():
                    self.hits.append({"rec": r, "row": row, "t": fr["t"], "set": "skirt", "region": key[0],
                                      "part": key[1], "depth": d, "class": "no collision pass",
                                      "points": int(np.sum((reg == key[0]) & (prt == key[1]))), "chain": -1, "seg": -1})
        fr.update(self._limbs(r, row, wb))
        return fr

    def _classes(self, r, wb, q, ii, skull, back, grid, bp, bn, idx):
        """Each hit hair point's geometric class from the solver's own points this frame."""
        run, hair = self.run, self.hair
        pts = run.pts[r]
        # the solver's points against her body (a wider search: they are 9 cm apart)
        flat = pts[:, 1:].reshape(-1, 3)
        sp, _, fp = grid.signed(flat, ring=3)
        in_body = (sp < -HIT_M).reshape(len(pts), -1)
        pad = np.zeros((len(pts), 1), bool)
        in_body = np.concatenate([pad, in_body], axis=1)       # the root is pinned: legal
        out = []
        for i in ii:
            p = q[i]
            in_proxy = np.linalg.norm(p - skull) < run.skull_r - HIT_M or np.linalg.norm(p - back) < run.back_r - HIT_M
            c, sg = hair.chain[i], hair.seg[i]
            if c < 0:
                out.append("rigid on the head")
                continue
            ends_in_body = in_body[c, sg] or in_body[c, sg + 1]
            if in_proxy or not ends_in_body:
                out.append("segment crossing")
            else:
                out.append("outside the proxies")
        return out

    def _limbs(self, r, row, wb):
        run, rows = self.run, self.rows
        out = {}
        for s in "lr":
            for kind, sel, col in (("foot", self.foot_sel.get(s), "surf_foot_" + s), ("palm", self.palm_sel.get(s), "surf_palm_" + s)):
                if sel is None:
                    continue
                pos = skin(run.parts[sel[0]], wb, sel[1])
                lo = pos[np.argmin(pos[:, 1])]
                out[kind + "_" + s] = lo
                out["gap_" + kind + "_" + s] = lo[1] - rows[col][row] if np.isfinite(rows[col][row]) else np.nan
        # hands: the wrists and knuckles as the rig places them
        for s in "lr":
            wr = wb[run.bone_index["wr_" + s]]
            out["wrist_" + s] = wr[:3, 3].copy()
            out["knuckles_" + s] = apply(wr, np.array(run.contacts["wr_" + s]["knuckles"]))
        out["root"] = run.skel[r][:3, 3].copy()
        out["rot"] = self._local_rotations(r)
        return out

    def _local_rotations(self, r):
        g = self.run.bones(r)
        par = self.run.parent
        rot = g[:, :3, :3].copy()
        has = par >= 0
        rot[has] = np.swapaxes(g[par[has], :3, :3], -1, -2) @ g[has, :3, :3]
        return rot

    # -- the held-still test: her pose at the frame kept, her hair let settle by the rig's own rules
    def held_still(self, r, regions):
        run = self.run
        wb = run.world_bones(r)
        pts, prev = run.pts[r].copy(), run.pts[r].copy()
        wc = run.wind_clock[r]
        for _ in range(90):
            wc = self.solver.step(pts, prev, wb, 1.0 / 30.0, wc, wind=False)
        hb = self.solver.bones_from(pts, wb)
        _, _, vi, ins, _, _, _, _, _ = self.hair_frame(wb, skin(self.hair.part, hb))
        left = set()
        for j in np.nonzero(ins)[0]:
            left.add(self.hair_body.region[vi[j]] if vi[j] >= 0 else "deep")
        return {reg: reg in left for reg in regions}


# --- reports ----------------------------------------------------------------------------------------------------

def episodes(hits, frames, set_name, gap=2):
    """Runs of frames with hits in one body part (gaps of up to `gap` frames joined)."""
    pos = {f["rec"]: k for k, f in enumerate(frames)}
    by = {}
    for h in hits:
        if h["set"] != set_name:
            continue
        by.setdefault(h["region"], []).append(h)
    out = []
    for reg, hs in by.items():
        hs.sort(key=lambda h: pos[h["rec"]])
        cur = []
        for h in hs:
            if cur and pos[h["rec"]] - pos[cur[-1]["rec"]] > gap + 1:
                out.append((reg, cur))
                cur = []
            cur.append(h)
        if cur:
            out.append((reg, cur))
    out.sort(key=lambda e: e[1][0]["t"])
    return out


def q(xs, p):
    xs = sorted(xs)
    return xs[int(p * (len(xs) - 1))] if xs else float("nan")


def report(sc, out_dir, held_still=True):
    run, rows, frames = sc.run, sc.rows, sc.frames
    os.makedirs(out_dir, exist_ok=True)
    lines = []
    say = lines.append
    fps = run.fps
    dt = 1.0 / fps
    vis_s = len(frames) * dt * sc.stride
    scen = next((a.split("=", 1)[1] for a in run.meta.get("args", []) if a.startswith("--uitest=")), "?")
    say("MATRON_PROBE %s at %d fps: %d frames drawn (%.1f s of her on screen), scored every %d" % (
        scen, fps, len(frames), vis_s, sc.stride))
    say("  her drawn body: %d parts skinned on the CPU from each frame's skeleton (linear blend, the parts' own bind poses); "
        "hair %d strands, %d points; scored in %.0f s" % (len(run.parts), sc.hair.nstrands, sc.hair.npts, sc.elapsed))
    if sc.validate:
        errs = np.array([e for _, e in sc.validate])
        say("  the hair solver's copy, one step from each recorded frame against the next: median %.1e m, p99 %.1e m, "
            "max %.1e m over %d steps (the game's own float32 rounding at its world coordinates)" % (
                np.nanmedian(errs), np.nanpercentile(errs, 99), np.nanmax(errs), len(errs)))
    # ---- penetration
    for set_name, label in (("hair", "hair through her body"), ("skirt", "skirt through her body")):
        eps = episodes(sc.hits, frames, set_name)
        hit_frames = {h["rec"] for h in sc.hits if h["set"] == set_name}
        say("")
        say("%s: %d of %d frames (%.1f%%), %.2f hit-seconds per second on screen, %d episodes" % (
            label, len(hit_frames), len(frames), 100.0 * len(hit_frames) / max(len(frames), 1),
            len(hit_frames) / max(len(frames), 1), len(eps)))
        if not eps:
            continue
        table = {}
        ep_rows = []
        for reg, hs in eps:
            worst = max(hs, key=lambda h: h["depth"])
            states = sorted({mode_of(rows, h["row"]) for h in hs})
            classes = {}
            for h in hs:
                classes[h["class"]] = classes.get(h["class"], 0) + 1
            cls = max(classes, key=classes.get)
            dur = (len({h["rec"] for h in hs})) * dt * sc.stride
            ep = {"set": set_name, "region": reg, "part": worst["part"], "t0": hs[0]["t"], "t1": hs[-1]["t"],
                  "frames": len({h["rec"] for h in hs}), "seconds": dur, "max_depth_m": worst["depth"],
                  "worst_t": worst["t"], "worst_rec": worst["rec"], "max_points": max(h["points"] for h in hs),
                  "state": "; ".join(states)[:200], "geometry": cls, "lag": "", "rest_direction": "", "class": cls}
            if set_name == "hair" and sc.replay:
                recs = {h["rec"] for h in hs}
                fw = [f for f in frames if f["rec"] in recs]
                left = {name: sum(1 for f in fw if reg in f.get("hair_regions_" + name, set()))
                        for name in sc.variants}
                for name in sc.variants:
                    ep["replay_%s_frames" % name] = left[name]
                # a variant judges only what the control (the copy with the rig's own rules) shows too
                ctl = left.get("head", 0)
                if ctl == 0:
                    ep["rest_direction"] = "not shown by the replay"
                else:
                    ep["rest_direction"] = "gone with world-down rest" if left["world"] <= 0.1 * ctl else \
                        "stays with world-down rest (%d of %d frames)" % (left["world"], ctl)
                    if "unclamped" in left:
                        ep["dt_clamp"] = "gone without the 1/30 s cap" if left["unclamped"] <= 0.1 * ctl else \
                            "stays without the cap (%d of %d frames)" % (left["unclamped"], ctl)
            if set_name == "hair" and held_still:
                still = sc.held_still(worst["rec"], [reg])
                ep["lag"] = "stays when held still" if still[reg] else "gone when held still"
            # the primary class, in the order a repair would try them: the rest direction, the lag, the geometry
            if set_name == "hair":
                if ep.get("rest_direction", "").startswith("gone"):
                    ep["class"] = "rest direction"
                elif ep["lag"].startswith("gone") or ep.get("dt_clamp", "").startswith("gone"):
                    ep["class"] = "lag"
                else:
                    ep["class"] = cls
            else:
                ep["class"] = "no collision pass"
            ep_rows.append(ep)
            t = table.setdefault(reg, {"episodes": 0, "frames": 0, "seconds": 0.0, "depth": 0.0, "classes": {}})
            t["episodes"] += 1
            t["frames"] += ep["frames"]
            t["seconds"] += ep["seconds"]
            t["depth"] = max(t["depth"], ep["max_depth_m"])
            t["classes"][ep["class"]] = t["classes"].get(ep["class"], 0) + 1
        with open(os.path.join(out_dir, "%s_episodes.csv" % set_name), "w", newline="") as fh:
            keys = list(ep_rows[0].keys())
            for e in ep_rows:
                for k in e:
                    if k not in keys:
                        keys.append(k)
            w = csv.DictWriter(fh, fieldnames=keys)
            w.writeheader()
            for e in ep_rows:
                w.writerow({k: (round(v, 4) if isinstance(v, float) else v) for k, v in e.items()})
        say("  %-18s %8s %7s %8s %9s  %s" % ("body part", "episodes", "frames", "seconds", "max depth", "class (episodes)"))
        for reg, t in sorted(table.items(), key=lambda kv: -kv[1]["seconds"]):
            say("  %-18s %8d %7d %8.2f %8.3f m  %s" % (reg, t["episodes"], t["frames"], t["seconds"], t["depth"],
                                                    ", ".join("%s %d" % kv for kv in sorted(t["classes"].items()))))
        tot, totf = {}, {}
        for e in ep_rows:
            tot[e["class"]] = tot.get(e["class"], 0) + 1
            totf[e["class"]] = totf.get(e["class"], 0) + e["frames"]
        say("  by class (episodes, frames): " + ", ".join("%s %d, %d" % (k, v, totf[k]) for k, v in
                                                       sorted(tot.items(), key=lambda kv: -totf[kv[0]])))
        if set_name == "hair":
            geo = {}
            for e in ep_rows:
                geo[e["geometry"]] = geo.get(e["geometry"], 0) + 1
            say("  by geometry alone: " + ", ".join("%s %d" % kv for kv in sorted(geo.items(), key=lambda kv: -kv[1])))
            if sc.replay:
                say("  frames with hair inside her, the copy of the solver over the same skeleton: " + ", ".join(
                    "%s %d" % (VARIANT_NAMES[n], sum(f.get("hair_n_" + n, 0) > 0 for f in frames)) for n in sc.variants)
                    + " (as drawn %d)" % len(hit_frames))
        bys = {}
        for e in ep_rows:
            for stt in e["state"].split("; "):
                bys[stt] = bys.get(stt, 0) + e["seconds"]
        say("  seconds by her state: " + "; ".join("%s %.2f" % kv for kv in sorted(bys.items(), key=lambda kv: -kv[1])[:8]))
        worst = sorted(ep_rows, key=lambda e: -e["max_depth_m"])[:5]
        for e in worst:
            say("  worst: %.3f m into her %s (%s) at t=%.2f s (record %d), %d frames, %s; %s" % (
                e["max_depth_m"], e["region"], e["part"], e["worst_t"], e["worst_rec"], e["frames"], e["state"][:90],
                e["class"]))
    # per-frame table
    with open(os.path.join(out_dir, "frames.csv"), "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["rec", "t", "t_sim", "state", "hair_points_in", "hair_worst_region", "hair_worst_depth_m",
                    "hair_points_in_world_rest", "hair_points_in_unclamped", "skirt_points_in", "skirt_worst_region",
                    "skirt_worst_depth_m", "gap_foot_l", "gap_foot_r", "gap_palm_l", "gap_palm_r"])
        for f in frames:
            hw = max(f["hair_regions"].items(), key=lambda kv: kv[1][0]) if f["hair_regions"] else None
            sw = max(f["skirt_regions"].items(), key=lambda kv: kv[1][0]) if f["skirt_regions"] else None
            w.writerow([f["rec"], "%.4f" % f["t"], "%.4f" % f["t_sim"], f["mode"], f["hair_n"],
                        hw[0][0] if hw else "", "%.4f" % hw[1][0] if hw else "",
                        f.get("hair_n_world", ""), f.get("hair_n_unclamped", f.get("hair_n_head", "")), f["skirt_n"],
                        sw[0][0] if sw else "", "%.4f" % sw[1][0] if sw else "",
                        *["%.4f" % f.get(k, np.nan) for k in ("gap_foot_l", "gap_foot_r", "gap_palm_l", "gap_palm_r")]])
    limbs_report(sc, say, out_dir)
    with open(os.path.join(out_dir, "summary.txt"), "w") as fh:
        fh.write("\n".join(lines) + "\n")
    print("\n".join(lines))


def limbs_report(sc, say, out_dir):
    run, rows, frames = sc.run, sc.rows, sc.frames
    dt = sc.stride / run.fps
    pairs = [(a, b) for a, b in zip(frames, frames[1:]) if b["rec"] - a["rec"] == sc.stride]

    def grounded(f):
        i = f["row"]
        return not (rows["climbing"][i] or rows["on_pipe"][i] or rows["on_line"][i] or rows["clinging"][i]
                    or rows["head_down"][i] or rows["climb_airborne"][i] or rows["fall_v"][i] < -1.0
                    or (rows["state"][i] == 11 and not rows["kneeling"][i]))
    say("")
    # ---- plants: skate, float, through the steel
    skate = []
    slides = []
    sink = []
    floats = []
    cur = {}
    for a, b in pairs:
        if not (grounded(a) and grounded(b)):
            cur = {}
            continue
        v = max(rows["speed"][a["row"]], rows["speed"][b["row"]])
        allow = max(2.5 * v * dt, 0.05)
        for limb in ("foot_l", "foot_r", "palm_l", "palm_r"):
            ga, gb = a.get("gap_" + limb, np.nan), b.get("gap_" + limb, np.nan)
            if not (np.isfinite(ga) and np.isfinite(gb)):
                continue
            if gb < -SINK_M:
                sink.append((gb, limb, b))
            on = abs(ga) < PLANT_M and abs(gb) < PLANT_M
            if on:
                d = float(np.linalg.norm((b[limb] - a[limb])[[0, 2]]))
                if d > allow:
                    skate.append((d, allow, limb, b))
                cur.setdefault(limb, [0.0, b])
                cur[limb][0] += d
            elif limb in cur:
                slides.append((cur[limb][0], limb, cur[limb][1]))
                del cur[limb]
        gaps = [b.get("gap_" + k, np.nan) for k in ("foot_l", "foot_r", "palm_l", "palm_r")]
        gaps = [g for g in gaps if np.isfinite(g)]
        if gaps and min(gaps) > FLOAT_M:
            floats.append((min(gaps), b))
    for limb, (dsum, f0) in cur.items():
        slides.append((dsum, limb, f0))
    gr = sum(1 for f in frames if grounded(f))
    say("plants (on a floor, %d frames): skate over max(2.5 v dt, 5 cm) a frame: %d limb-frames%s" % (
        gr, len(skate), "" if not skate else "; worst %.3f m (allowed %.3f) %s at t=%.2f %s" % (
            max(skate, key=lambda x: x[0] / x[1])[0], max(skate, key=lambda x: x[0] / x[1])[1],
            max(skate, key=lambda x: x[0] / x[1])[2], max(skate, key=lambda x: x[0] / x[1])[3]["t"],
            max(skate, key=lambda x: x[0] / x[1])[3]["mode"])))
    big = [s for s in slides if s[0] > 0.05]
    say("  planted limbs sliding over 5 cm while down: %d plants%s" % (len(big), "" if not big else
        "; longest %.2f m (%s from t=%.2f, %s)" % (max(big)[0], max(big)[1], max(big)[2]["t"], max(big)[2]["mode"])))
    say("  floating (her lowest hand or foot over %.0f cm above the surface under it): %d frames%s" % (
        FLOAT_M * 100, len(floats), "" if not floats else "; worst %.3f m at t=%.2f (%s)" % (
            max(floats, key=lambda x: x[0])[0], max(floats, key=lambda x: x[0])[1]["t"],
            max(floats, key=lambda x: x[0])[1]["mode"])))
    say("  through the steel (a hand or foot over %.0f cm below the surface): %d limb-frames%s" % (
        SINK_M * 100, len(sink), "" if not sink else "; worst %.3f m %s at t=%.2f (%s)" % (
            -min(sink, key=lambda x: x[0])[0], min(sink, key=lambda x: x[0])[1], min(sink, key=lambda x: x[0])[2]["t"],
            min(sink, key=lambda x: x[0])[2]["mode"])))
    # ---- grips
    since, gaps = None, {}
    climb = []
    for f in frames:
        i = f["row"]
        st = int(rows["state"][i])
        if st in HOLD_STATES and rows["held"][i] > 0:
            since = f["t"] if since is None else since
            if f["t"] - since >= 0.3:
                h = np.array([rows["hx"][i], rows["hy"][i], rows["hz"][i]])
                g = min(np.linalg.norm(f["knuckles_l"] - h), np.linalg.norm(f["knuckles_r"] - h))
                gaps.setdefault(STATE_NAMES[st], []).append((g, f))
        else:
            since = None
        if rows["ch_known"][i] > 0:
            hold = np.array([rows["chx"][i], rows["chy"][i], rows["chz"][i]])
            climb.append((float(np.linalg.norm(f["wrist_r"] - hold)), f))
    say("grips: her hand (the nearer knuckles) to the held ankle, from 0.3 s into a hold, <= %.2f m:" % GRIP_M)
    if not gaps:
        say("  no hold in this run")
    for k, v in gaps.items():
        gs = [x[0] for x in v]
        over = [x for x in v if x[0] > GRIP_M]
        say("  %-8s %5d frames: median %.3f p95 %.3f max %.3f m; over %d%s" % (
            k, len(v), q(gs, .5), q(gs, .95), max(gs), len(over),
            "" if not over else " (worst at t=%.2f)" % max(over, key=lambda x: x[0])[1]["t"]))
    if climb:
        gs = [x[0] for x in climb]
        over = [x for x in climb if x[0] > GRIP_M]
        say("  climbing: right wrist to the native's hold %d frames: median %.3f p95 %.3f max %.3f m; over %.2f m: %d" % (
            len(climb), q(gs, .5), q(gs, .95), max(gs), GRIP_M, len(over)))
    # ---- joints and root
    rates, over = [], []
    skip = [i for i, n in enumerate(run.bone_names) if n.startswith("hair") or n.startswith("eye")
            or (n[:1] in "ft" and n[1:2].isdigit())]
    keep = np.ones(len(run.bone_names), bool)
    keep[skip] = False
    names = np.array(run.bone_names)[keep]
    steps = []
    for a, b in pairs:
        ra, rb = a["rot"][keep], b["rot"][keep]
        rel = np.swapaxes(ra, -1, -2) @ rb
        ang = np.arccos(np.clip((np.trace(rel, axis1=1, axis2=2) - 1.0) * 0.5, -1.0, 1.0))
        k = int(np.argmax(ang))
        rate = float(ang[k] / dt)
        burst = rows["burst"][b["row"]] if "burst" in rows else np.nan
        rates.append(rate)
        if rate > JOINT_RAD_S and not (np.isfinite(burst) and burst > 0):
            over.append((rate, names[k], b))
        i = b["row"]
        if grounded(a) and grounded(b):
            v = max(rows["speed"][a["row"]], rows["speed"][i])
            step = float(np.linalg.norm(b["root"] - a["root"]))
            allow = max(1.5 * v * dt, 0.05)
            if step > allow:
                steps.append((step, allow, b))
    say("joints: rate p95 %.1f p99 %.1f max %.1f rad/s; over %.0f rad/s outside flagged bursts: %d frames%s" % (
        q(rates, .95), q(rates, .99), max(rates) if rates else 0.0, JOINT_RAD_S, len(over),
        "" if not over else " (%s)" % ", ".join("%s %d" % kv for kv in sorted(
            {n: sum(1 for o in over if o[1] == n) for n in {o[1] for o in over}}.items(), key=lambda kv: -kv[1])[:5])))
    for o in sorted(over, key=lambda o: -o[0])[:3]:
        say("    %.1f rad/s %s at t=%.2f (%s)" % (o[0], o[1], o[2]["t"], o[2]["mode"]))
    say("root: step over max(1.5 v dt, 5 cm) a frame on a floor: %d frames%s" % (
        len(steps), "" if not steps else "; worst %.3f m (allowed %.3f) at t=%.2f (%s)" % (
            max(steps, key=lambda s: s[0] / s[1])[0], max(steps, key=lambda s: s[0] / s[1])[1],
            max(steps, key=lambda s: s[0] / s[1])[2]["t"], max(steps, key=lambda s: s[0] / s[1])[2]["mode"])))


def compare(dirs):
    """Hair through her body across runs of one scenario at different frame rates, per second on screen."""
    print("%-34s %4s %8s %10s %12s %10s %9s" % ("run", "fps", "on screen", "hit frames", "hit s per s", "episodes",
                                              "per minute"))
    for d in dirs:
        summ = os.path.join(d, "score", "frames.csv")
        if not os.path.exists(summ):
            print("%s: not scored yet" % d)
            continue
        with open(summ) as fh:
            rows = list(csv.DictReader(fh))
        meta = json.load(open(os.path.join(d, "meta.json")))
        fps = int(meta.get("fixed_fps") or 60)
        n = len(rows)
        hit = sum(1 for r in rows if int(r["hair_points_in"]) > 0)
        eps = 0
        epf = os.path.join(d, "score", "hair_episodes.csv")
        classes = {}
        if os.path.exists(epf):
            with open(epf) as fh:
                er = list(csv.DictReader(fh))
            eps = len(er)
            for e in er:
                classes[e["class"]] = classes.get(e["class"], 0) + 1
        on_s = n / fps
        print("%-34s %4d %8.1f s %10d %12.3f %10d %9.1f  %s" % (
            os.path.basename(os.path.normpath(d))[:34], fps, on_s, hit, hit / max(n, 1), eps, eps / max(on_s / 60.0, 1e-9),
            ", ".join("%s %d" % kv for kv in sorted(classes.items()))))


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "compare":
        compare(sys.argv[2:])
        return
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("run")
    ap.add_argument("--stride", type=int, default=1, help="score every Nth drawn frame (the replays still step every frame)")
    ap.add_argument("--no-replay", action="store_true", help="skip the hair solver's replays (rest direction, the cap)")
    ap.add_argument("--no-still", action="store_true", help="skip the held-still test")
    ap.add_argument("--records", default="", help="only records a:b (for a quick look)")
    a = ap.parse_args()
    run = Run(a.run)
    if a.records:
        lo, hi = (int(x) if x else None for x in a.records.split(":"))
        run.limit(lo or 0, hi if hi is not None else run.nrec)
    print("read %s: %d drawn frames of %d" % (a.run, run.nrec, len(run.rows["f"])))
    sc = Scorer(run, a.stride, not a.no_replay)
    sc.run_all()
    report(sc, os.path.join(a.run, "score"), held_still=not a.no_still)


if __name__ == "__main__":
    main()
