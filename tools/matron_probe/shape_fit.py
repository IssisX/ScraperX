#!/usr/bin/env python3
"""How well her collision body fits her drawn body, frame by frame.

Reads a probe recording (tools/matron_probe/probe.gd: meta.json, mesh.bin,
frames.csv, bones.bin), skins her drawn body on the CPU as score.py does, and
measures it against her collision capsule in her own frame (her simulated
middle, turned by her simulated yaw):

  out  how far her drawn body reaches outside the capsule (the body she shows
       that nothing can hit), per frame the 98th percentile of her skinned
       vertices (fingertips and stray skirt hem aside) and the worst one;
  in   how far the capsule reaches past her drawn body (the body that hits
       with nothing shown), per frame the worst of the six directions of her
       frame (up, down, forward, back, left, right).

The capsule is the one the recording's frames carry (shape_* columns, the
native's export) or, for a recording made before the native exported one,
her old upright capsule (2.10 m, r 0.30, about her middle).

    python3 tools/matron_probe/shape_fit.py <probe_dir> [--stride N] [--fixture OUT.inc] [--per-family K]

--fixture writes her drawn limb segments (body frame) for K frames of each
stance family as a C++ include for the native shape check
(tests/data/matron_drawn_limbs.inc).
"""
import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import score  # noqa: E402

FAMILIES = ["upright", "low", "climb", "pipe", "down", "hold", "lift"]   # Matron::Stance

# Her limb volumes: a segment between two joints of her rig (matron_rig.gd),
# the radius of her drawn flesh about it measured each frame (the 90th
# percentile of the distances of the vertices those bones move most), and
# its region: 0 trunk and head, 1 forearm and hand (her reach), 2 upper arm,
# 3 leg. "skull" is her head's centre (the hair's skull, meta.json).
SEGMENTS = [
    ("body", "spine_mid", 0, ("body", "spine_lo", "skirt")),
    ("spine_mid", "neck_lo", 0, ("spine_mid", "chest", "hump")),
    ("neck_lo", "head", 0, ("neck_lo", "neck_hi")),
    ("head", "skull", 0, ("head", "jaw", "eye_l", "eye_r")),
    ("sh_l", "el_l", 2, ("sh_l", "sh_l_x")), ("sh_r", "el_r", 2, ("sh_r", "sh_r_x")),
    ("el_l", "wr_l", 1, ("el_l", "el_l_x", "wr_l")), ("el_r", "wr_r", 1, ("el_r", "el_r_x", "wr_r")),
    ("hp_l", "kn_l", 3, ("hp_l", "hp_l_x")), ("hp_r", "kn_r", 3, ("hp_r", "hp_r_x")),
    ("kn_l", "an_l", 3, ("kn_l", "kn_l_x", "an_l")), ("kn_r", "an_r", 3, ("kn_r", "kn_r_x", "an_r")),
]


def family_of(rows, i):
    g = lambda k: float(rows[k][i]) if k in rows else 0.0  # noqa: E731
    if "stance" in rows and not np.isnan(rows["stance"][i]):
        return FAMILIES[int(rows["stance"][i])]
    state = int(g("state"))
    if state == 11:
        return "down"
    if state in (13, 14, 15):   # Carry, Dangle, ArmPlay (Matron::wanted_stance)
        return "lift"
    if 12 <= state <= 18:
        return "hold"
    if g("on_pipe") > 0.5:
        return "pipe"
    if g("climbing") > 0.5 or g("clinging") > 0.5 or g("on_line") > 0.5 or g("mantling") > 0.5:
        return "climb"
    if state == 3 and (int(g("stage")) > 0 or g("speed") <= 1.0):   # Matron::wanted_stance
        return "low"
    return "upright"


def capsule_of(rows, i, centre):
    """(a, b, r) in world: the recorded native shape, or her old upright capsule."""
    if "shape_ax" in rows and not np.isnan(rows["shape_ax"][i]):
        a = np.array([rows["shape_ax"][i], rows["shape_ay"][i], rows["shape_az"][i]])
        b = np.array([rows["shape_bx"][i], rows["shape_by"][i], rows["shape_bz"][i]])
        return a, b, float(rows["shape_r"][i])
    return centre + np.array([0, -0.75, 0]), centre + np.array([0, 0.75, 0]), 0.30


def seg_dist(p, a, b):
    ab = b - a
    t = np.clip(((p - a) @ ab) / max(ab @ ab, 1e-12), 0.0, 1.0)
    return np.linalg.norm(p - (a + t[:, None] * ab), axis=1)


REACH = ("el_", "wr_", "f0", "f1", "f2", "f3", "t0", "t1")   # forearm and hand bones


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dirs", nargs="+")
    ap.add_argument("--stride", type=int, default=3)
    ap.add_argument("--fixture", default="")
    ap.add_argument("--per-family", type=int, default=24)
    args = ap.parse_args()
    per = {f: {"body": [], "all": [], "in": []} for f in FAMILIES}
    limbs = {f: [] for f in FAMILIES}
    for path in args.dirs:
        measure(path, args, per, limbs)
    print("family   frames   her body out of it, p95 of frames (all but forearms and hands / all)   it past her body p95")
    for f in FAMILIES:
        if not per[f]["body"]:
            continue
        q = lambda x: float(np.percentile(x, 95))  # noqa: E731
        print("%-8s %6d   %5.2f / %5.2f m   %5.2f m" % (f, len(per[f]["body"]), q(per[f]["body"]), q(per[f]["all"]),
                                                     q(per[f]["in"])))
    if args.fixture:
        write_fixture(args.fixture, limbs, args.per_family, ", ".join(os.path.basename(d.rstrip("/")) for d in args.dirs))


def measure(path, args, per, limbs):
    run = score.Run(path)
    rows = run.rows
    parts = [p for n, p in run.parts.items() if n != "hair"]   # her hair is soft, not her body
    names = run.bone_index
    dom_all = np.concatenate([score.dominant_bone(p) for p in parts])
    reach = np.isin(dom_all, [i for n, i in names.items() if n.startswith(REACH)])
    for rec in range(0, run.nrec, args.stride):
        i = run.row_of_rec[rec]
        if i < 0 or rows["alive"][i] < 0.5 or rows["vis"][i] < 0.5 or rows["released"][i] < 0.5:
            continue
        centre = np.array([rows["px"][i], rows["py"][i], rows["pz"][i]])
        yaw = float(rows["yaw"][i])
        fam = family_of(rows, i)
        # Drawn between ticks (render position and yaw): carried into the
        # tick's frame, where her collision body is.
        drawn_at = np.array([rows["rx"][i], rows["ry"][i], rows["rz"][i]])
        turn = float(rows["ryaw"][i]) - yaw
        c, s_ = np.cos(-turn), np.sin(-turn)
        to_tick = np.array([[c, 0.0, s_], [0.0, 1.0, 0.0], [-s_, 0.0, c]])
        wb = run.world_bones(rec).copy()
        wb[:, :3, :3] = to_tick @ wb[:, :3, :3]
        wb[:, :3, 3] = (wb[:, :3, 3] - drawn_at) @ to_tick.T + centre
        v = np.concatenate([score.skin(p, wb) for p in parts])
        a, b, r = capsule_of(rows, i, centre)
        d = seg_dist(v, a, b) - r
        per[fam]["body"].append(float(np.percentile(d[~reach], 98)))
        per[fam]["all"].append(float(np.percentile(d, 98)))
        # her frame: x right, y up, z forward
        fwd = np.array([np.sin(yaw), 0.0, np.cos(yaw)])
        right = np.array([np.cos(yaw), 0.0, -np.sin(yaw)])
        up = np.array([0.0, 1.0, 0.0])
        worst = 0.0
        for axis in (up, -up, fwd, -fwd, right, -right):
            worst = max(worst, max(a @ axis, b @ axis) + r - float(np.percentile(v @ axis, 99.5)))
        per[fam]["in"].append(worst)
        if args.fixture:
            frame = np.stack([right, up, fwd])
            skull = score.apply(wb[names["head"]], run.skull_local)
            joint = lambda n: skull if n == "skull" else wb[names[n]][:, 3]  # noqa: E731
            segs = []
            for pa, ch, region, owners in SEGMENTS:
                ja, jb = joint(pa), joint(ch)
                mine = np.isin(dom_all, [names[o] for o in owners])
                rad = float(np.percentile(seg_dist(v[mine], ja, jb), 90)) if mine.any() else 0.05
                segs.append((frame @ (ja - centre), frame @ (jb - centre), rad, region))
            limbs[fam].append(segs)


def write_fixture(path, limbs, k, src):
    with open(path, "w") as fh:
        fh.write("// Generated by tools/matron_probe/shape_fit.py --fixture from probe recordings\n")
        fh.write("// (%s): her drawn limb volumes in her body frame (x right, y up, z\n" % src)
        fh.write("// forward, about her simulated middle): stance, frame, region, a, b, radius.\n")
        fh.write("// Do not edit by hand.\n")
        for f in FAMILIES:
            frames = limbs[f]
            if not frames:
                continue
            pick = np.linspace(0, len(frames) - 1, min(k, len(frames))).astype(int)
            for n, j in enumerate(pick):
                for a, b, r, region in frames[j]:
                    fh.write("{%d, %d, %d, {%.3fF, %.3fF, %.3fF}, {%.3fF, %.3fF, %.3fF}, %.3fF},\n" % (
                        FAMILIES.index(f), n, region, a[0], a[1], a[2], b[0], b[1], b[2], r))


if __name__ == "__main__":
    main()
