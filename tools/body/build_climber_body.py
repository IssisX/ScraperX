#!/usr/bin/env python3
"""The climber's own body below the arms, as the player sees it looking down.

Run with:  python3 -I tools/body/build_climber_body.py [makehuman dir] [out dir]
           (defaults: build/makehuman, godot/presentation/life/climber)

A young man of average build from MakeHuman's CC0 scanned body (human.py),
scaled so his eyes are where the camera is (main_feel.gd: 1.52 m over the
boots' soles standing). Nothing of him is bare: the hands are the viewmodel's
(first_person_arms.gd) and there is no head (the eye is in it), so what is
written is his clothes, skinned to the rig's bones:

- work trousers: an offset shell of the legs and seat, hemmed over the boots;
- the work jacket: an offset shell of the whole trunk, the shoulders and up
  to the collar, so looking down the eye meets a chest and shoulders (not a
  lid), the shoulders ending rounded and the collar closed over, a hi-vis
  band round it at the waist, a zip up its front;
- ankle boots: lofted round each foot, on a rubber sole;
- and, for his shadow only, his head, neck, arms and hands.

Writes climber_body.glb (one node per part, one skin), climber_body_rig.gd
(the numbers first_person_body.gd poses it by: the eye, the neck's base and
the pelvis at rest, the legs' lengths, each boot's heel and toe in its foot
bone's frame) and climber_body_rig.json (the same and the bones, for tools).
"""
import json
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import human as h  # noqa: E402

DL = sys.argv[1] if len(sys.argv) > 1 else os.path.join("build", "makehuman")
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join("godot", "presentation", "life", "climber")

# The camera's height over the soles, standing (main_feel.gd EYE_OFFSET over
# the native capsule's centre): his eyes.
EYE_Y = 1.52
# The boots' rubber under the bare foot.
SOLE_M = 0.028
# His shadow-only parts are simplified to this grid.
SHADOW_CELL_M = 0.015
PRESET = {"age": 0.5, "muscle": 0.5, "weight": 0.5, "height": 0.5, "sex": "male"}

def linear(hex_srgb):
    """An sRGB hex colour as glTF's linear base colour."""
    c = [int(hex_srgb[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return [((x + 0.055) / 1.055) ** 2.4 if x > 0.04045 else x / 12.92 for x in c] + [1.0]


# The game dresses these in main_feel.gd's own materials (the arms' sleeves'
# jacket and band among them); these are the same colours, for anything that
# draws the file as it is.
MATERIALS = {
    "trousers": {"color": linear("23262b"), "rough": 0.9},
    "jacket": {"color": linear("1f231c"), "rough": 0.93},
    "jacket_inner": {"color": linear("0d0d0c"), "rough": 0.95},
    "hivis": {"color": linear("b8611d"), "rough": 0.55},
    "zip": {"color": linear("0d0d0c"), "rough": 0.5},
    "boot": {"color": linear("1c130b"), "rough": 0.82},
    "sole": {"color": linear("0d0d0c"), "rough": 0.95},
    "shadow": {"color": linear("808080"), "rough": 1.0},
}


def weights_of_nearest(P, V, W, pool):
    """Each point takes the skin weights of the nearest body vertex in `pool`."""
    P = np.atleast_2d(P)
    d = np.linalg.norm(V[pool][None, :, :] - P[:, None, :], axis=2)
    return W[pool[np.argmin(d, axis=1)]]


def cap_dome(part, ring, outward, wfn, rise=0.45):
    """Close the opening `ring` (k, 3) with a low dome bulging along
    `outward`: rings drawn in toward its centre and out, then an apex."""
    ctr = ring.mean(0)
    radius = float(np.linalg.norm(ring - ctr, axis=1).mean())
    rings = [ring]
    for t in (0.3, 0.55, 0.78, 0.93):
        a = t * math.pi / 2.0
        rings.append(ctr + (ring - ctr) * math.cos(a) + outward * radius * rise * math.sin(a))
    apex = ctr + outward * radius * rise
    k = len(ring)
    P = np.concatenate([np.concatenate(rings), apex[None]])
    T = []
    for i in range(len(rings) - 1):
        for j in range(k):
            a, b = i * k + j, i * k + (j + 1) % k
            c, d = (i + 1) * k + j, (i + 1) * k + (j + 1) % k
            T += [[a, b, c], [b, d, c]]
    top = (len(rings) - 1) * k
    ai = len(P) - 1
    T += [[top + j, top + (j + 1) % k, ai] for j in range(k)]
    T = np.array(T)
    # wound to face out of the opening
    n = np.cross(P[T[:, 1]] - P[T[:, 0]], P[T[:, 2]] - P[T[:, 0]])
    if (n @ outward).sum() < 0:
        T = T[:, ::-1]
    part.add(P, h.vertex_normals(P, T), np.stack([P[:, 0] + P[:, 2], P[:, 1]], 1) * 6.0, wfn(P), T)


def cross_section_hull(points, centre, angles):
    """The farthest reach of `points` (x, z about `centre`) along each angle:
    the convex outline a loose garment hangs on."""
    rel = points[:, [0, 2]] - centre
    dirs = np.stack([np.sin(angles), np.cos(angles)], 1)
    return (rel @ dirs.T).max(0)


def main():
    h.set_download_dir(DL)
    V0, VT, faces = h.load_obj(os.path.join(DL, "3dobjs/base.obj"))
    skel = json.load(open(os.path.join(DL, "rigs/default.mhskel")))
    wfile = json.load(open(os.path.join(DL, "rigs/default_weights.mhw")))["weights"]
    V = h.morph(V0, PRESET)
    tris = h.tri_faces(faces)
    body_tris = [t for t in tris if t[2] == "body"]
    T_body = np.array([t[0] for t in body_tris])
    body_v = np.unique(T_body.ravel())
    is_body = np.zeros(len(V), bool)
    is_body[body_v] = True
    # decimetres -> metres, the bare soles on y = 0
    V = V * 0.1
    V[:, 1] -= V[body_v, 1].min()
    J0 = {jn: V[vs].mean(0) for jn, vs in skel["joints"].items()}
    natural_eye = float(0.5 * (J0["eye.L____head"][1] + J0["eye.R____head"][1]))
    k = (EYE_Y - SOLE_M) / natural_eye
    V *= k
    V[:, 1] += SOLE_M          # he stands on his boots' soles
    J = {jn: V[vs].mean(0) for jn, vs in skel["joints"].items()}
    N = h.vertex_normals(V, T_body)
    E = h.neighbours(len(V), T_body)
    bones, byname = h.build_rig(V, J, skel, extras=())
    W, names = h.skin_weights(V, len(V), bones, byname, skel, wfile)
    col = {n: i for i, n in enumerate(names)}

    def wsum(*bs):
        return sum(W[:, col[b]] for b in bs)

    fingers = [n for n in names if n[0] in "ft" and n[1].isdigit()]
    arm = wsum("sh_l", "sh_l_x", "el_l", "el_l_x", "wr_l", "sh_r", "sh_r_x", "el_r", "el_r_x", "wr_r",
               *fingers)
    head = wsum("neck_lo", "neck_hi", "head", "jaw")
    legs = wsum("hp_l", "hp_l_x", "kn_l", "kn_l_x", "an_l", "hp_r", "hp_r_x", "kn_r", "kn_r_x", "an_r")
    trunk = wsum("body", "spine_lo", "spine_mid", "chest")
    hip_y = float(byname["hp_l"]["pos"][1])
    knee_y = float(byname["kn_l"]["pos"][1])
    ankle_y = float(byname["an_l"]["pos"][1])
    waist_y = float(0.5 * (J["spine04____head"][1] + J["spine03____head"][1]))
    eye = 0.5 * (J["eye.L____head"] + J["eye.R____head"])
    neck_base = J["neck01____head"]
    stats = {"preset": PRESET, "scale": k, "hip_y": hip_y, "knee_y": knee_y, "ankle_y": ankle_y,
             "waist_y": waist_y, "sole_m": SOLE_M, "thigh_m": byname["hp_l"]["length"],
             "shin_m": byname["kn_l"]["length"], "eye": [float(x) for x in eye],
             "neck_base": [float(x) for x in neck_base],
             "pelvis": [float(x) for x in byname["body"]["pos"]]}
    parts = []

    def tris_of(mask):
        return T_body[mask[T_body].all(1)]

    # ---- trousers: the legs and seat, waist to over the boots ----
    boot_top_y = ankle_y + 0.075
    in_trousers = is_body & (arm < 0.2) & (head < 0.2) & (V[:, 1] < waist_y + 0.04) & (V[:, 1] > boot_top_y - 0.02) \
        & ((legs + trunk) > 0.6)
    Tt = tris_of(in_trousers)
    tv = np.unique(Tt.ravel())
    S = h.shell(V, N, E, Tt, tv, 0.014, smooth_iters=18, outward_iters=40, min_gap=0.008)
    # a little looser down the shins, as work trousers hang
    flare = np.clip((knee_y - S[:, 1]) / max(knee_y - boot_top_y, 1e-3), 0.0, 1.0)
    S += N * (0.010 * flare)[:, None]
    for _ in range(4):
        S += 0.5 * h.laplacian(S, E, np.isin(np.arange(len(V)), tv))
    Ns = h.vertex_normals(S, Tt)
    sel = np.isin(np.arange(len(T_body)), np.where(in_trousers[T_body].all(1))[0])
    vi, ti, Tc = h.split_corners(V, VT, body_tris, in_trousers[T_body].all(1))
    trousers = h.Part("trousers", "trousers")
    trousers.add(S[vi], Ns[vi], VT[ti] * 6.0, W[vi], Tc)
    # the hems over the boots: the two lowest boundary loops, turned in
    hems = sorted(h.loops(h.boundary_edges(Tt)), key=lambda lp: S[lp, 1].mean())[:2]
    for lp in hems:
        lp = np.array(lp)
        for _ in range(8):
            S[lp] = 0.5 * S[lp] + 0.25 * (np.roll(S[lp], 1, 0) + np.roll(S[lp], -1, 0))
        h.band(trousers, S, Ns, W, lp, -Ns, 0.01)
    parts.append(trousers)
    del sel

    # ---- the jacket: a shell of the whole trunk, hip to collar ----
    hem_y = hip_y + 0.05
    neck_y = float(J["neck01____head"][1])
    in_jacket = is_body & (head < 0.3) & (arm < 0.45) & (V[:, 1] > hem_y) & ((trunk + arm + head) > 0.5)
    Tj = tris_of(in_jacket)
    jv = np.unique(Tj.ravel())
    JS = h.shell(V, N, E, Tj, jv, 0.018, smooth_iters=22, outward_iters=50, min_gap=0.01)
    # canvas hangs: the hem stands a little off the seat and belly
    hang = np.clip((waist_y - JS[:, 1]) / max(waist_y - hem_y, 1e-3), 0.0, 1.0)
    JS += N * (0.012 * hang)[:, None]
    for _ in range(4):
        JS += 0.5 * h.laplacian(JS, E, np.isin(np.arange(len(V)), jv))
    # the openings: the hem (lowest), the collar (highest), the armholes. The
    # region's edge follows whole faces of the body (a staircase): the edges
    # are smoothed, and two rings of faces inside them with them.
    openings = [np.array(lp) for lp in h.loops(h.boundary_edges(Tj)) if len(lp) >= 8]
    openings.sort(key=lambda lp: JS[lp, 1].mean())
    in_j = np.isin(np.arange(len(V)), jv)
    Ej = h.neighbours(len(V), Tj)
    on_edge = np.zeros(len(V), bool)
    for lp in openings:
        on_edge[lp] = True
    near_edge = on_edge.copy()
    for _ in range(2):
        grow = near_edge.copy()
        grow[Ej[near_edge[Ej[:, 0]], 1]] = True
        near_edge = grow & in_j
    inside = near_edge & ~on_edge
    for _ in range(14):
        for lp in openings:
            JS[lp] = 0.5 * JS[lp] + 0.25 * (np.roll(JS[lp], 1, 0) + np.roll(JS[lp], -1, 0))
        JS += 0.5 * h.laplacian(JS, Ej, inside)
    NJ = h.vertex_normals(JS, Tj)
    keep_j = in_jacket[T_body].all(1)
    vi, ti, Tc = h.split_corners(V, VT, body_tris, keep_j)
    jacket = h.Part("jacket", "jacket")
    jacket.add(JS[vi], NJ[vi], VT[ti] * 6.0, W[vi], Tc)
    trunk_mid = JS[jv].mean(0)
    for lp in openings[1:]:
        # the shoulders end rounded and the collar closes over: low domes of
        # the jacket's own cloth, never a hole for the eye to look into
        ring = JS[lp]
        out = ring.mean(0) - trunk_mid
        if lp is openings[-1]:
            out = np.array([0.0, 1.0, 0.0])     # the collar: up
        cap_dome(jacket, ring, out / np.linalg.norm(out), lambda P: weights_of_nearest(P, V, W, jv))
    parts.append(jacket)
    lining = h.Part("jacket_inner", "jacket_inner")
    h.band(lining, JS, NJ, W, openings[0], -NJ, 0.012)   # the hem, turned in
    parts.append(lining)
    # the hi-vis band round the trunk at the waist: rings of the shell's own
    # outline there, stood just proud of it (one across the chest would sit
    # right under the eye, a bright arc across the bottom of the view)
    nseg = 48
    th = np.linspace(0.0, 2.0 * math.pi, nseg, endpoint=False)
    U = np.stack([np.sin(th), np.zeros(nseg), np.cos(th)], 1)
    centre = np.array([0.0, float(JS[jv][np.abs(JS[jv, 1] - waist_y) < 0.03, 2].mean())])
    wfn = lambda P: weights_of_nearest(P, V, W, jv)   # noqa: E731
    hivis = h.Part("hivis", "hivis")
    for y_mid in (waist_y - 0.03,):
        rings = []
        for y in (y_mid - 0.025, y_mid, y_mid + 0.025):
            sl = JS[jv][np.abs(JS[jv, 1] - y) < 0.012]
            r = cross_section_hull(sl, centre, th) + 0.004
            rings.append(np.array([centre[0], y, centre[1]]) + U * r[:, None])
        h.loft(hivis, rings, wfn, uv_scale=10.0)
    parts.append(hivis)
    # the zip up the front: a narrow raised strip from the hem to the collar
    front_v = jv[(np.abs(JS[jv, 0]) < 0.012)]
    front_v = front_v[np.argsort(JS[front_v, 1])]
    zys = np.linspace(hem_y + 0.01, neck_y - 0.02, 24)
    zip_pts = []
    for y in zys:
        near = front_v[np.argsort(np.abs(JS[front_v, 1] - y))[:3]]
        zip_pts.append(JS[near].mean(0))
    zip_pts = np.array(zip_pts)
    zipper = h.Part("zip", "zip")
    zw = 0.007
    zP = np.concatenate([zip_pts + np.array([-zw, 0.0, 0.003]), zip_pts + np.array([zw, 0.0, 0.003])])
    zT = []
    n_ = len(zip_pts)
    for i_ in range(n_ - 1):
        zT += [[i_, i_ + 1, n_ + i_], [i_ + 1, n_ + i_ + 1, n_ + i_]]
    zipper.add(zP, np.tile([0.0, 0.0, 1.0], (len(zP), 1)), np.zeros((len(zP), 2)), wfn(zP), zT)
    parts.append(zipper)

    # ---- boots: lofted round each foot to over the ankle, on a rubber sole ----
    boot = h.Part("boots", "boot")
    sole = h.Part("soles", "sole")
    contacts = {}
    for s_ in "lr":
        ank = byname["an_" + s_]["pos"]
        fsel = is_body & ((wsum("an_" + s_) > 0.4) | ((wsum("kn_" + s_, "kn_%s_x" % s_) > 0.3) & (V[:, 1] < boot_top_y)))
        fsel &= V[:, 1] < boot_top_y
        FP = V[fsel]
        low = FP[FP[:, 1] < SOLE_M + 0.03]
        heel_p, toe_p = low[np.argmin(low[:, 2])], low[np.argmax(low[:, 2])]
        ax = toe_p - heel_p
        ax[1] = 0.0
        flen = float(np.linalg.norm(ax))
        ax /= flen
        lat = np.cross([0.0, 1.0, 0.0], ax)
        nring = 24
        phis = np.linspace(0, 2 * math.pi, nring, endpoint=False)
        # along the foot from heel to toe: a ring round the foot's slice, with
        # room in it (leather and lining), the toe boxed up
        R = []
        nsl = 20
        for i in range(nsl):
            tt = (i + 0.5) / nsl
            c0 = heel_p + ax * (tt * flen)
            d = (FP - c0) @ ax
            slab = FP[np.abs(d) < 0.014]
            if len(slab) < 4:
                slab = FP[np.argsort(np.abs(d))[:12]]
            cl = (slab - c0) @ lat
            cy = slab[:, 1]
            ctr = np.array([cl.mean(), 0.5 * (cy.min() + cy.max())])
            Q = np.stack([cl, cy], 1) - ctr
            ease = 0.009 + 0.006 * max(0.0, tt - 0.6) / 0.4
            ring = []
            for ph in phis:
                u = np.array([math.cos(ph), math.sin(ph)])
                r = float((Q @ u).max()) + ease
                p2 = ctr + u * r
                y = max(p2[1], SOLE_M)
                ring.append(c0 + lat * p2[0] + np.array([0.0, y - c0[1], 0.0]))
            R.append(np.array(ring))
        R = np.array(R)
        tip = R[-1].mean(0) + ax * 0.018
        back = R[0].mean(0) - ax * 0.012
        R = np.concatenate([(0.35 * R[:1] + 0.65 * back), R, (0.45 * R[-1:] + 0.55 * tip)])
        pool = np.where(fsel)[0]
        bfn = lambda P, pool=pool: weights_of_nearest(P, V, W, pool)   # noqa: E731
        h.loft(boot, list(R), bfn, uv_scale=10.0)
        # the shaft up round the ankle: rings of the leg's slice, open at the top
        lsel = is_body & (legs > 0.5) & (V[:, 1] > ankle_y - 0.04) & (V[:, 1] < boot_top_y + 0.01) \
            & (np.sign(V[:, 0]) == (-1 if s_ == "l" else 1))
        LP = V[lsel]
        lc = LP[:, [0, 2]].mean(0)
        shaft_rings = []
        for y in np.linspace(ankle_y - 0.02, boot_top_y, 6):
            sl = LP[np.abs(LP[:, 1] - y) < 0.015]
            if len(sl) < 6:
                sl = LP
            r = cross_section_hull(sl, lc, th) + 0.012
            shaft_rings.append(np.array([lc[0], y, lc[1]]) + U * r[:, None])
        lpool = np.where(lsel)[0]
        lfn = lambda P, pool=lpool: weights_of_nearest(P, V, W, pool)   # noqa: E731
        h.loft(boot, shaft_rings, lfn, uv_scale=10.0)
        top_ring = shaft_rings[-1]
        h.loft(boot, [top_ring, np.array([lc[0], boot_top_y, lc[1]]) + (top_ring - np.array([lc[0], boot_top_y, lc[1]])) * 0.9
                      + np.array([0.0, -0.012, 0.0])], lfn, uv_scale=10.0)
        # the sole: a slab under the foot's outline
        outline = R[1:-1, :, :]
        lo = outline.reshape(-1, 3)
        rel = lo - heel_p
        along = rel @ ax
        across = rel @ lat
        nx_ = 14
        ts = np.linspace(-0.01, flen + 0.016, nx_)
        half = []
        for tv_ in ts:
            m_ = np.abs(along - tv_) < 0.02
            half.append(float(np.abs(across[m_]).max()) + 0.004 if m_.any() else 0.03)
        half = np.array(half)
        top_pts, bot_pts = [], []
        for tv_, hw in zip(ts, half):
            for sgn in (-1.0, 1.0):
                base = heel_p + ax * tv_ + lat * (sgn * hw)
                top_pts.append(np.array([base[0], SOLE_M, base[2]]))
                bot_pts.append(np.array([base[0], 0.0, base[2]]))
        top_pts = np.array(top_pts)
        bot_pts = np.array(bot_pts)
        nP = len(top_pts)
        P_ = np.concatenate([top_pts, bot_pts])
        T_ = []
        for i in range(nx_ - 1):
            a0, a1, b0, b1 = 2 * i, 2 * i + 1, 2 * i + 2, 2 * i + 3
            T_ += [[a0, b0, a1], [a1, b0, b1]]                                   # top
            T_ += [[nP + a0, nP + a1, nP + b0], [nP + a1, nP + b1, nP + b0]]     # bottom
            T_ += [[a0, nP + a0, b0], [b0, nP + a0, nP + b0]]                    # one side
            T_ += [[a1, b1, nP + a1], [b1, nP + b1, nP + a1]]                    # the other
        T_ += [[0, 1, nP], [1, nP + 1, nP]]
        e0, e1 = 2 * (nx_ - 1), 2 * (nx_ - 1) + 1
        T_ += [[e0, nP + e0, e1], [e1, nP + e0, nP + e1]]
        T_ = np.array(T_)
        sole.add(P_, h.vertex_normals(P_, T_), np.stack([P_[:, 0], P_[:, 2]], 1) * 8.0, bfn(P_), T_)
        # heel and toe on the ground, in the foot bone's frame
        bb = byname["an_" + s_]

        def local(pt, bb=bb):
            return [float(x) for x in bb["F"].T @ (np.asarray(pt) - bb["pos"])]
        heel_g = heel_p - ax * 0.012
        toe_g = toe_p + ax * 0.018
        heel_g[1] = toe_g[1] = 0.0
        contacts["an_" + s_] = {"heel": local(heel_g), "toe": local(toe_g)}
    parts.append(boot)
    parts.append(sole)

    # ---- what only his shadow shows: his head and neck, his arms and hands.
    # The view draws its own arms (first_person_arms.gd, which cast no
    # shadow) and no head (the eye is in it), so without these his shadow
    # was legs and a trunk. The game sets this part to cast shadows only; it
    # is a little fuller than the skin, for the sleeves and his hair.
    in_shadow = is_body & ((head > 0.3) | (arm > 0.3))
    Tsh = tris_of(in_shadow)
    used = np.unique(Tsh.ravel())
    remap = -np.ones(len(V), int)
    remap[used] = np.arange(len(used))
    SP_ = V[used] + N[used] * 0.012
    ST_ = remap[Tsh]
    # A shadow needs no face or fingernails: the vertices clustered on a
    # SHADOW_CELL_M grid, each cluster at its mean with its mean weights,
    # and the triangles that collapse dropped.
    cell = np.floor(SP_ / SHADOW_CELL_M).astype(np.int64)
    _, cluster, counts = np.unique(cell, axis=0, return_inverse=True, return_counts=True)
    cluster = cluster.reshape(-1)
    n_c = len(counts)
    CP_ = np.zeros((n_c, 3))
    CW_ = np.zeros((n_c, W.shape[1]))
    np.add.at(CP_, cluster, SP_)
    np.add.at(CW_, cluster, W[used])
    CP_ /= counts[:, None]
    CW_ /= counts[:, None]
    CT_ = cluster[ST_]
    CT_ = CT_[(CT_[:, 0] != CT_[:, 1]) & (CT_[:, 1] != CT_[:, 2]) & (CT_[:, 0] != CT_[:, 2])]
    shadow = h.Part("shadow", "shadow")
    shadow.add(CP_, h.vertex_normals(CP_, CT_), np.zeros((n_c, 2)), CW_, CT_)
    parts.append(shadow)

    out_parts = []
    tri_counts = {}
    for part in parts:
        P, Nn, UV, Wp, Tp, Cp = part.arrays()
        idx, w = h.top4(Wp)
        out_parts.append(h.named_part(part.name, part.material, (P, Nn, UV, (idx, w), Tp, Cp)))
        tri_counts[part.name] = int(len(Tp))
    stats["triangles"] = tri_counts
    stats["triangles_total"] = int(sum(tri_counts.values()))
    os.makedirs(OUT, exist_ok=True)
    h.write_glb(os.path.join(OUT, "climber_body.glb"), bones, out_parts, MATERIALS, root="ClimberBody",
                generator="tools/body/build_climber_body.py")
    rig = []
    for b in bones:
        r = {"name": b["name"], "parent": b["parent"], "t": [float(x) for x in b["t"]],
             "q": [float(x) for x in h.quat_from_m(b["L"])], "pos": [float(x) for x in b["pos"]]}
        if "length" in b:
            r["length"] = b["length"]
        rig.append(r)
    json.dump({"bones": rig, "contacts": contacts, "stats": stats}, open(os.path.join(OUT, "climber_body_rig.json"), "w"),
              indent=1)

    def v3(p):
        return "Vector3(%.5f, %.5f, %.5f)" % tuple(float(x) for x in p)
    lines = [
        "# GENERATED by tools/body/build_climber_body.py: do not edit. Change the",
        "# builder and run it again (the .glb beside this is its other output).",
        "# Rest-pose numbers in the body's model space (soles on y = 0, facing +z).",
        "",
        "const EYE := %s" % v3(stats["eye"]),
        "const NECK_BASE := %s" % v3(stats["neck_base"]),
        "const PELVIS := %s" % v3(stats["pelvis"]),
        "const HIP_L := %s" % v3(byname["hp_l"]["pos"]),
        "const HIP_R := %s" % v3(byname["hp_r"]["pos"]),
        "const THIGH := %.5f" % stats["thigh_m"],
        "const SHIN := %.5f" % stats["shin_m"],
        "const ANKLE_L := %s" % v3(byname["an_l"]["pos"]),
        "const ANKLE_R := %s" % v3(byname["an_r"]["pos"]),
        "# Each boot's heel and toe on the ground, in its foot bone's frame.",
        "const HEEL_L := %s" % v3(contacts["an_l"]["heel"]),
        "const TOE_L := %s" % v3(contacts["an_l"]["toe"]),
        "const HEEL_R := %s" % v3(contacts["an_r"]["heel"]),
        "const TOE_R := %s" % v3(contacts["an_r"]["toe"]),
        "",
    ]
    open(os.path.join(OUT, "climber_body_rig.gd"), "w").write("\n".join(lines))
    print("climber_body: hip %.3f knee %.3f ankle %.3f waist %.3f m, %d triangles (%s)" % (
        hip_y, knee_y, ankle_y, waist_y, stats["triangles_total"],
        ", ".join("%s %d" % kv for kv in tri_counts.items())))


if __name__ == "__main__":
    main()
