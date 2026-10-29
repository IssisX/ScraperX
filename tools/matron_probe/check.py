#!/usr/bin/env python3
"""Pass/fail checks on the Matron as she is drawn, from recordings made by probe.gd.

    python3 tools/matron_probe/check.py <run dir> [<run dir> ...] [--stride N] [--report FILE]

The sim can be right while her drawn body is wrong (her arm was drawn 2.7 times its length in a hold for
weeks while every sim test passed), so this reads what the renderer draws: the posed skeleton of every
recorded frame (score.py skins it the way the renderer does). Exit status 1 when any check fails that is
not an expected failure, or when an expected failure no longer fails (remove it from EXPECTED then).

Checks, each over every recorded frame unless noted, with its tolerance:

  stretch_arm, stretch_leg
           a bone drawn longer than its rest length by more than STRETCH_PCT percent (the distance between
           a bone and its parent, against the rest pose; arms are the shoulder and elbow bones, legs the
           hip, knee and ankle). Hair and eyes are not bones she poses and are skipped.
  grip_hold, grip_climb
           while the sim says a hand of hers holds something, the drawn hand (the line from a wrist to its
           knuckles, the nearer hand) more than GRIP_M from the sim's grip point: the hold (his ankle, from
           GRIP_FROM_S into it) and the climb or pipe grip (contact_hand and contact_hand2, when the native
           says they are held).
  plant    a drawn foot that is down (its lowest point within PLANT_M of the surface under it) while she
           stands still on a floor (not climbing, held, knocked down or turning; under STILL_MPS), more than
           PLANT_DRIFT_M across the ground from where it came down. A jump of the root over TELEPORT_M in a
           frame (a restore) starts the plants again.
  hair, skirt
           hair points (or skirt points) inside her drawn body past the probe's HIT_M (3 mm), her bind
           pose's layering already taken away (score.py); sampled every STRIDE frames (the hair solver is
           the slow part).

Not measured: limb vertices inside her torso (only hair and skirt are scored against her body).

EXPECTED names a known defect and the exit-board item that fixes it. The tolerance stays the law; a
known defect is marked, never tolerated.
"""
import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import score  # noqa: E402

STRETCH_PCT = 5.0
GRIP_M = 0.05
GRIP_FROM_S = 0.3
STILL_MPS = 0.15
TURN_RAD_S = 0.4           # a body turning faster than this steps its feet round
TELEPORT_M = 1.0
PLANT_M = score.PLANT_M
PLANT_DRIFT_M = 0.05
HIT_M = score.HIT_M
MIN_REST_M = 0.01          # a bone shorter at rest than this has no length to measure
ARM_BONES = ("sh_", "el_")
CHECKS = ("stretch_arm", "stretch_leg", "grip_hold", "grip_climb", "plant", "hair", "skirt")
# check -> the exit-board item that fixes the known defect (strict: when it stops failing, remove it).
# Found on trunk 1bd28c1 (touch_matron_act, 2026-10-29); the tolerance stays the law.
EXPECTED = {
    "stretch_arm": "exit item 4, holds at a real arm's length (PR #37)",
    "grip_hold": "exit item 4, holds at a real arm's length (PR #37)",
    "grip_climb": "no exit item yet: her drawn hands are off the climb's grip (reported 2026-10-29)",
    "plant": "no exit item yet: a foot slides up to 8.5 cm while she is nearly still (reported 2026-10-29)",
    "hair": "exit item 10, hair",
    "skirt": "exit item 10, hair and cloth (the skirt's baseline is her bind pose, an A-pose)",
}


def stretch_failures(run):
    """(ratio, bone, rec) for the worst frame of each stretched bone, and the count of frames."""
    nb = len(run.bone_names)
    origin = score.xf(run.bones_raw.reshape(run.nrec, nb, 12))[..., :, 3]
    parent = run.parent
    rest = np.array([np.linalg.norm(b["rest"][9:12]) for b in run.meta["bones"]])
    skip = np.array([n.startswith("hair") or n.startswith("eye") for n in run.bone_names])
    use = (parent >= 0) & (rest > MIN_REST_M) & ~skip
    idx = np.nonzero(use)[0]
    length = np.linalg.norm(origin[:, idx] - origin[:, parent[idx]], axis=2)
    ratio = length / rest[idx]
    recs = np.nonzero(run.row_of_rec >= 0)[0]
    out = []
    for k, bone in enumerate(idx):
        r = ratio[recs, k]
        over = np.nonzero(r > 1.0 + STRETCH_PCT / 100.0)[0]
        if over.size:
            worst = over[np.argmax(r[over])]
            out.append((float(r[worst]), run.bone_names[bone], int(recs[worst]), int(over.size)))
    return sorted(out, reverse=True)


def palm_gap(run, wb, point):
    """The nearer hand's drawn palm (the line from its wrist to its knuckles) to `point`."""
    best = 1.0e9
    for s in "lr":
        wr = wb[run.bone_index["wr_" + s]]
        a = wr[:3, 3]
        b = score.apply(wr, np.array(run.contacts["wr_" + s]["knuckles"]))
        ab = b - a
        t = float(np.clip(np.dot(point - a, ab) / max(float(np.dot(ab, ab)), 1.0e-9), 0.0, 1.0))
        best = min(best, float(np.linalg.norm(point - (a + t * ab))))
    return best


def grip_failures(run):
    rows = run.rows
    out = []
    since = None
    have_contact = "chand_held" in rows
    for rec in range(run.nrec):
        i = run.row_of_rec[rec]
        if i < 0:
            continue
        t = rows["t"][i]
        st = int(rows["hold_state"][i])
        held = st in score.HOLD_STATES and rows["held"][i] > 0
        since = (t if since is None else since) if held else None
        want = []
        if held and t - since >= GRIP_FROM_S:
            want.append(("hold", np.array([rows["hx"][i], rows["hy"][i], rows["hz"][i]])))
        if have_contact and rows["chand_held"][i] > 0:
            want.append(("climb", np.array([rows["chandx"][i], rows["chandy"][i], rows["chandz"][i]])))
        if have_contact and rows["chand2_held"][i] > 0:
            want.append(("climb2", np.array([rows["chand2x"][i], rows["chand2y"][i], rows["chand2z"][i]])))
        if not want:
            continue
        wb = run.world_bones(rec)
        for kind, point in want:
            gap = palm_gap(run, wb, point)
            if gap > GRIP_M:
                out.append((gap, kind, rec, t))
    return out


def plant_failures(run):
    rows = run.rows
    out = []
    # per foot: the heel and toe points, as the probe measures the surface under it
    pts = {s: (np.array(run.contacts["an_" + s]["heel"]), np.array(run.contacts["an_" + s]["toe"])) for s in "lr"}
    anchor = {"l": None, "r": None}
    last_root = None
    last_yaw = None
    for rec in range(run.nrec):
        i = run.row_of_rec[rec]
        if i < 0:
            continue
        root = run.skel[rec][:3, 3]
        if last_root is not None and float(np.linalg.norm(root - last_root)) > TELEPORT_M:
            anchor = {"l": None, "r": None}
        last_root = root
        grounded = not (rows["climbing"][i] or rows["on_pipe"][i] or rows["on_line"][i] or rows["clinging"][i]
                        or rows["head_down"][i] or rows["climb_airborne"][i] or rows["fall_v"][i] < -1.0
                        or (rows["state"][i] == 11 and not rows["kneeling"][i]) or rows["held"][i] > 0
                        or rows["speed"][i] > STILL_MPS or rows["turn"][i] != 0)
        yaw = float(rows["ryaw"][i])
        if last_yaw is not None and abs((yaw - last_yaw + np.pi) % (2 * np.pi) - np.pi) / max(float(rows["dt"][i]), 1e-3) > TURN_RAD_S:
            grounded = False
        last_yaw = yaw
        wb = run.world_bones(rec)
        for s in "lr":
            surf = rows["surf_foot_" + s][i]
            an = wb[run.bone_index["an_" + s]]
            heel, toe = score.apply(an, pts[s][0]), score.apply(an, pts[s][1])
            low = heel if heel[1] < toe[1] else toe
            planted = grounded and np.isfinite(surf) and abs(min(heel[1], toe[1]) - surf) < PLANT_M
            if not planted:
                anchor[s] = None
                continue
            here = (heel + toe) * 0.5
            if anchor[s] is None:
                anchor[s] = (here.copy(), rec)
                continue
            drift = float(np.linalg.norm((here - anchor[s][0])[[0, 2]]))
            if drift > PLANT_DRIFT_M:
                out.append((drift, "foot_" + s, rec, rows["t"][i]))
                anchor[s] = (here.copy(), rec)       # one failure per slide, not one per frame after it
    return out


def penetration_failures(run, stride):
    sc = score.Scorer(run, stride=stride, replay=False, log=lambda *a: None)
    sc.run_all()
    out = {"hair": [], "skirt": []}
    for h in sc.hits:
        if h["depth"] > HIT_M and h["set"] in out:
            out[h["set"]].append((h["depth"], h["part"] + "/" + h["region"], h["rec"], h["t"]))
    return out, sc.elapsed


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("runs", nargs="+")
    ap.add_argument("--stride", type=int, default=10, help="score hair and skirt every Nth frame")
    ap.add_argument("--report", default="")
    args = ap.parse_args()
    results = {}      # check -> list of (amount, what, rec, t)
    lines = []

    def say(text):
        lines.append(text)
        print(text, flush=True)

    for path in args.runs:
        run = score.Run(path)
        name = os.path.basename(os.path.normpath(path))
        frames = int((run.row_of_rec >= 0).sum())
        say("== %s: %d drawn frames at %d fps" % (name, frames, run.fps))
        found = {name_: [] for name_ in CHECKS}
        for ratio, bone, rec, count in stretch_failures(run):
            key = "stretch_arm" if bone.startswith(ARM_BONES) else "stretch_leg"
            found[key].append(((ratio - 1.0) * 100.0, "%s drawn %.2fx its rest length, %d frames" % (bone, ratio, count), rec,
                               run.rows["t"][run.row_of_rec[rec]]))
        for gap, kind, rec, t in grip_failures(run):
            found["grip_hold" if kind == "hold" else "grip_climb"].append((gap, kind, rec, t))
        for drift, foot, rec, t in plant_failures(run):
            found["plant"].append((drift, foot, rec, t))
        pen, secs = penetration_failures(run, args.stride)
        found["hair"], found["skirt"] = pen["hair"], pen["skirt"]
        for check, items in found.items():
            results.setdefault(check, []).extend((a, w, "%s:%d" % (name, r), t) for a, w, r, t in items)
            say("  %-8s %d failures" % (check, len(items)))
        say("  (hair and skirt scored every %d frames: %.0f s)" % (args.stride, secs))
    failed = []
    say("")
    for check in CHECKS:
        items = sorted(results.get(check, []), reverse=True)
        known = EXPECTED.get(check)
        unit = "%" if check.startswith("stretch") else " m"
        if items:
            say("%s %s: %d failures%s" % ("XFAIL" if known else "FAIL", check, len(items),
                                          " (expected, fixed by %s)" % known if known else ""))
            for amount, what, rec, t in items[:12]:
                say("    %s frame %s t=%.2f s: %s %.3f%s" % ("  ", rec, t, what, amount, unit))
            if not known:
                failed.append(check)
        elif known:
            say("XPASS %s: no failure now; it was expected until %s. Remove it from EXPECTED." % (check, known))
            failed.append(check)
        else:
            say("PASS %s" % check)
    if args.report:
        with open(args.report, "w") as fh:
            fh.write("\n".join(lines) + "\n")
    print("MATRON_DRAWN %s" % ("FAIL: " + ", ".join(failed) if failed else "PASS"))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
