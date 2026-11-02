#!/usr/bin/env python3
"""Drawn hair and skirt edge stretch over a Matron probe recording.

    python3 tools/matron_probe/edge_stretch.py <recording> [--stride N] [--gate 1.10]

Every triangle edge of the hair and skirt_cloth parts is skinned with score.py's
CPU skinning in every drawn record (or every Nth) and compared with its length
as the game loaded it. Prints the largest drawn/rest ratio per part, where it
happens, and the ratio seen in hold and fall frames. Exit code 1 if any edge
is longer than --gate times its rest length.
"""
import argparse
import sys

import numpy as np

from score import HOLD_STATES, Run, STATE_NAMES, dominant_bone, skin

PARTS = ("hair", "skirt_cloth")


def edges(part):
    t = part.tris
    e = np.concatenate([t[:, [0, 1]], t[:, [1, 2]], t[:, [2, 0]]])
    e.sort(axis=1)
    e = np.unique(e, axis=0)
    rest = np.linalg.norm(part.v[e[:, 0]] - part.v[e[:, 1]], axis=1)
    keep = rest > 1e-5
    return e[keep], rest[keep]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("recording")
    ap.add_argument("--stride", type=int, default=1)
    ap.add_argument("--gate", type=float, default=1.10)
    a = ap.parse_args(argv)
    run = Run(a.recording)
    states = run.rows["state"]
    worst_all = 0.0
    for name in PARTS:
        part = run.parts.get(name)
        if part is None:
            print("%s: missing" % name)
            continue
        e, rest = edges(part)
        dom = dominant_bone(part)
        cross = dom[e[:, 0]] != dom[e[:, 1]]
        best = (0.0, -1, -1)
        hold_max = fall_max = 0.0
        over = 0
        for rec in range(0, run.nrec, a.stride):
            pos = skin(part, run.world_bones(rec))
            r = np.linalg.norm(pos[e[:, 0]] - pos[e[:, 1]], axis=1) / rest
            j = int(np.argmax(r))
            row = int(run.row_of_rec[rec])
            st = int(states[row]) if row >= 0 and np.isfinite(states[row]) else -1
            if st in HOLD_STATES:
                hold_max = max(hold_max, r[j])
            if STATE_NAMES.get(st, "") in ("Falling", "Fallen", "Knocked", "Topple", "Downed"):
                fall_max = max(fall_max, r[j])
            over += int(r[j] > a.gate)
            if r[j] > best[0]:
                best = (float(r[j]), rec, j)
        ratio, rec, j = best
        row = int(run.row_of_rec[rec]) if rec >= 0 else -1
        st = int(states[row]) if row >= 0 and np.isfinite(states[row]) else -1
        print("%s: edges=%d cross_bone=%d max_ratio=%.3f record=%d state=%s rest_mm=%.2f bones=%s/%s "
              "hold_max=%.3f fall_max=%.3f records_over_%.2f=%d" % (
                  name, len(e), int(cross.sum()), ratio, rec, STATE_NAMES.get(st, st), 1000 * rest[j],
                  run.bone_names[int(dom[e[j, 0]])], run.bone_names[int(dom[e[j, 1]])],
                  hold_max, fall_max, a.gate, over))
        worst_all = max(worst_all, ratio)
    print("states seen: %s" % sorted({STATE_NAMES.get(int(s), int(s)) for s in states[np.isfinite(states)]}))
    print("GATE %s max_ratio=%.3f gate=%.2f" % ("PASS" if worst_all <= a.gate else "FAIL", worst_all, a.gate))
    return 0 if worst_all <= a.gate else 1


if __name__ == "__main__":
    sys.exit(main())
