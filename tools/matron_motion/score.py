#!/usr/bin/env python3
"""Rule 37 from a recording made by record.gd: her movement is continuous,
joints at most 20 rad/s, hands reaching each hold.

    python3 tools/matron_motion/score.py <dir> [<dir> ...]

For each recording it writes <dir>/score/summary.txt (also printed),
joints.csv (one row a drawn frame: the fastest joint and its rate),
bones.csv (each bone's fastest frame) and holds.csv (one row a hold).

Joints. A joint's rate is the angle between its rotation in its parent's frame
on two drawn frames in a row, over the frame's time: the pose the renderer
skins, read after every view pass. Pairs across a frame she was not drawn are
not motion on screen and are skipped, as are pairs across a reload of the
save (the climber's death restores her whole: rule 34). Her body's joints are every bone but
her hair (driven by the hair solver, not a joint) and her eyes (scored on
their own line: saccades). Her fingers are joints and count. The skeleton's
own turn in the world (her body turning, not a joint) has its own line. Frames
where her drawn place moves more than JUMP_M in one frame are listed: a snap
there is not a joint's rate but is not continuous either.

Holds. A hold is a run of frames in which the simulation says a hand holds a
point (contact_hand, contact_hand2: her climbing contacts; climb_hand,
climb_hand2: the hold her climb is on; the ankle she holds the climber by);
a new one starts where the point jumps over HOLD_JUMP_M in a frame. Reached:
her nearer hand (the knuckles as the rig places them) comes within GRIP_M of
the point. Kept: from REACH_S into the hold, the share of drawn frames her
nearer hand is within GRIP_M. Out of reach: frames where the point is farther
from both shoulders than that arm's reach (shoulder to knuckles, its bones'
own lengths) plus GRIP_M, so no drawn arm could be on it: the simulation's
contact, not the drawing.
"""
import csv
import json
import math
import os
import sys

import numpy as np

JOINT_RAD_S = 20.0       # rule 37
GRIP_M = 0.10            # as tools/matron_probe/score.py GRIP_M
REACH_S = 0.3
HOLD_JUMP_M = 0.3
JUMP_M = 0.5
HOLD_STATES = (12, 13, 14, 15, 16, 17, 18)   # matron_probe/score.py: Grab .. Claimed
STATE_NAMES = {0: "Dormant", 1: "Straining", 2: "Emerging", 3: "Hunting", 4: "Searching", 5: "Shuffling", 6: "Dead",
               7: "Guarding", 8: "Peering", 9: "Unrigging", 10: "Staggering", 11: "Down", 12: "Grab", 13: "Carry",
               14: "Dangle", 15: "ArmPlay", 16: "Release", 17: "Spare", 18: "Claimed"}
HOLD_SOURCES = (
    ("contact_hand", "hand_held", "hand_"),
    ("contact_hand2", "hand2_held", "hand2_"),
    ("climb_hand", "ch_known", "ch"),
    ("climb_hand2", "ch2_known", "ch2"),
)


def read(path):
    with open(os.path.join(path, "meta.json")) as f:
        meta = json.load(f)
    with open(os.path.join(path, "frames.csv")) as f:
        rows = list(csv.DictReader(f))
    n = int(meta["record_floats"])
    rec = np.fromfile(os.path.join(path, "rotations.bin"), dtype=np.float32)
    if rec.size % n:
        raise ValueError("%s: rotations.bin holds %d floats, not a whole number of %d-float records" % (
            path, rec.size, n))
    rec = rec.reshape(-1, n)
    return meta, rows, rec


def fnum(row, key):
    v = row.get(key, "")
    return float(v) if v not in ("", None) else math.nan


def point(row, prefix):
    """The point in columns <prefix>x, <prefix>y, <prefix>z (NaN where blank)."""
    return np.array([fnum(row, prefix + axis) for axis in "xyz"])


def mode(row):
    s = STATE_NAMES.get(int(row["state"]), row["state"])
    if int(row["climbing"]):
        s += " climbing"
    if row.get("climb_move") and int(row.get("climb_move_active") or 0):
        s += " " + row["climb_move"]
    if row.get("burst") == "1":
        s += " burst"
    return s


def angles(qa, qb):
    """Angle between unit quaternions (rows), radians."""
    d = np.abs(np.sum(qa * qb, axis=-1))
    na = np.linalg.norm(qa, axis=-1)
    nb = np.linalg.norm(qb, axis=-1)
    return 2.0 * np.arccos(np.clip(d / np.maximum(na * nb, 1e-12), 0.0, 1.0))


def pct(xs, p):
    return float(np.quantile(np.asarray(xs), p)) if len(xs) else math.nan


def score(path):
    meta, rows, rec = read(path)
    out_dir = os.path.join(path, "score")
    os.makedirs(out_dir, exist_ok=True)
    lines = []

    def say(s=""):
        lines.append(s)
        print(s)

    names = [b["name"] for b in meta["bones"]]
    nb = len(names)
    hair = np.array([n.startswith("hair") for n in names])
    eyes = np.array([n.startswith("eye") for n in names])
    body = ~(hair | eyes)
    say("%s: %s" % (os.path.basename(os.path.normpath(path)), " ".join(meta.get("args", []))))
    dts = [float(r["dt"]) for r in rows if float(r["dt"]) > 0.0]
    say("engine %s, frame time median %.4f s (%.0f fps), %d frames, %d drawn, game time %.1f s" % (
        meta.get("engine"), pct(dts, .5), 1.0 / pct(dts, .5) if dts else 0.0, int(meta.get("frames", len(rows))),
        rec.shape[0], float(meta.get("t_game", 0.0))))
    say("joints counted: %d (all but %d hair and %d eye bones)" % (int(body.sum()), int(hair.sum()), int(eyes.sum())))

    # ---- joints ----------------------------------------------------------------------------------------------
    drawn = [r for r in rows if int(r["rec"]) >= 0]
    pairs = []
    restores = []
    for a, b in zip(drawn, drawn[1:]):
        if int(b["f"]) == int(a["f"]) + 1 and int(b["rec"]) == int(a["rec"]) + 1:
            if b.get("deaths", "0") != a.get("deaths", "0"):
                restores.append(b)
                continue
            pairs.append((a, b))
    skipped = len(drawn) - 1 - len(pairs) - len(restores) if drawn else 0
    body_rates, eye_rates, turn_rates = [], [], []
    over = []
    jumps = []
    per_bone = np.zeros(nb)
    per_bone_at = [None] * nb
    with open(os.path.join(out_dir, "joints.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["f", "t", "dt", "rate_rad_s", "bone", "eye_rate_rad_s", "turn_rad_s", "moved_m", "mode"])
        for a, b in pairs:
            dt = float(b["dt"])
            ia, ib = int(a["rec"]), int(b["rec"])
            qa = rec[ia, 4:].reshape(nb, 4)
            qb = rec[ib, 4:].reshape(nb, 4)
            ang = angles(qa, qb)
            turn = float(angles(rec[ia, :4][None], rec[ib, :4][None])[0])
            moved = float(np.linalg.norm(point(b, "r") - point(a, "r")))
            if dt <= 0.0:
                # A frame with no time (paused): any change is a snap.
                if float(ang[body].max()) > 1e-4:
                    jumps.append((moved, b, "pose changed in a frame of no time"))
                continue
            rates = ang / dt
            k = int(np.argmax(np.where(body, rates, -1.0)))
            rate = float(rates[k])
            eye_rate = float(rates[eyes].max()) if eyes.any() else 0.0
            body_rates.append(rate)
            eye_rates.append(eye_rate)
            turn_rates.append(turn / dt)
            better = rates > per_bone
            per_bone = np.where(better, rates, per_bone)
            for j in np.nonzero(better)[0]:
                per_bone_at[j] = b
            if rate > JOINT_RAD_S:
                over.append((rate, names[k], b))
            if moved > JUMP_M:
                jumps.append((moved, b, "drawn place moved"))
            w.writerow([b["f"], b["t"], b["dt"], "%.3f" % rate, names[k], "%.3f" % eye_rate, "%.3f" % (turn / dt),
                        "%.4f" % moved, mode(b)])
    say("")
    say("JOINTS (rule 37: at most %.0f rad/s), %d frame pairs drawn in a row (%d across a gap and %d across a "
        "reload of the save skipped%s):" % (JOINT_RAD_S, len(pairs), skipped, len(restores), "" if not restores else
                                            ", at t=" + ", ".join(r["t"] for r in restores)))
    if body_rates:
        say("  body joints: p50 %.2f p95 %.2f p99 %.2f max %.2f rad/s; over %.0f: %d frames" % (
            pct(body_rates, .5), pct(body_rates, .95), pct(body_rates, .99), max(body_rates), JOINT_RAD_S, len(over)))
        counts = {}
        for o in over:
            counts[o[1]] = counts.get(o[1], 0) + 1
        if over:
            say("    by bone: " + ", ".join("%s %d" % kv for kv in sorted(counts.items(), key=lambda kv: -kv[1])[:8]))
            for o in sorted(over, key=lambda o: -o[0])[:10]:
                say("    %.1f rad/s %s at t=%s f=%s (%s)" % (o[0], o[1], o[2]["t"], o[2]["f"], mode(o[2])))
        say("  eyes (not counted): max %.2f rad/s; turn of her whole body in the world (not a joint): max %.2f rad/s" % (
            max(eye_rates) if eye_rates else 0.0, max(turn_rates) if turn_rates else 0.0))
    else:
        say("  she was never drawn on two frames in a row")
    say("  drawn place moved over %.2f m in a frame, or posed in a frame of no time: %d%s" % (
        JUMP_M, len(jumps), "" if not jumps else "; " + "; ".join(
            "%.2f m at t=%s (%s, %s)" % (j[0], j[1]["t"], j[2], mode(j[1])) for j in jumps[:5])))
    with open(os.path.join(out_dir, "bones.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["bone", "counted", "max_rad_s", "t", "f", "mode"])
        for j in np.argsort(-per_bone):
            at = per_bone_at[j]
            w.writerow([names[j], int(body[j]), "%.3f" % per_bone[j], at["t"] if at else "", at["f"] if at else "",
                        mode(at) if at else ""])

    # ---- holds -----------------------------------------------------------------------------------------------
    def hands(row):
        out = []
        for s in "lr":
            p = point(row, "knuckles_%s_" % s)
            if np.all(np.isfinite(p)):
                out.append(p)
        return out

    reach = meta.get("arm_reach", {})

    def out_of_reach(row, p):
        """How far the point lies beyond the nearer arm's reach (<= 0: within it); NaN if unknown."""
        best = math.inf
        for s in "lr":
            sh = point(row, "sh_%s_" % s)
            if s in reach and np.all(np.isfinite(sh)):
                best = min(best, float(np.linalg.norm(p - sh)) - float(reach[s]))
        return best if best < math.inf else math.nan

    episodes = []
    sources = list(HOLD_SOURCES) + [("ankle", None, "ankle_")]
    for name, flag, prefix in sources:
        cur = None
        for r in rows:
            if flag is None:
                on = int(r["held"]) > 0 and int(r["hold_state"]) in HOLD_STATES
            else:
                on = int(r[flag]) > 0
            p = point(r, prefix)
            if on and cur is not None and np.linalg.norm(p - cur["last"]) <= HOLD_JUMP_M:
                cur["rows"].append(r)
                cur["points"].append(p)
                cur["last"] = p
                continue
            if cur is not None:
                episodes.append(cur)
                cur = None
            if on:
                cur = {"source": name, "at": p, "last": p, "rows": [r], "points": [p]}
        if cur is not None:
            episodes.append(cur)
    say("")
    say("HOLDS (her nearer knuckles within %.2f m of the held point; kept: from %.1f s in):" % (GRIP_M, REACH_S))
    results = []
    with open(os.path.join(out_dir, "holds.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["source", "t_start", "t_end", "seconds", "drawn_frames", "x", "y", "z", "nearest_m",
                    "reached", "kept_share", "kept_frames", "settled_median_m", "out_of_reach_frames",
                    "most_beyond_reach_m", "mode"])
        for e in episodes:
            t0 = float(e["rows"][0]["t"])
            t1 = float(e["rows"][-1]["t"])
            dists, kept, beyond = [], [], []
            for r, p in zip(e["rows"], e["points"]):
                hs = hands(r)
                if not hs:
                    continue
                d = min(float(np.linalg.norm(h - p)) for h in hs)
                dists.append(d)
                beyond.append(out_of_reach(r, p))
                if float(r["t"]) - t0 >= REACH_S:
                    kept.append(d)
            e["beyond"] = beyond
            nearest = min(dists) if dists else math.nan
            reached = bool(dists) and nearest <= GRIP_M
            share = (sum(1 for d in kept if d <= GRIP_M) / len(kept)) if kept else math.nan
            results.append((e, t0, t1, dists, kept, nearest, reached, share))
            w.writerow([e["source"], "%.3f" % t0, "%.3f" % t1, "%.3f" % (t1 - t0), len(dists),
                        "%.4f" % e["at"][0], "%.4f" % e["at"][1], "%.4f" % e["at"][2],
                        "" if not dists else "%.4f" % nearest, int(reached),
                        "" if not kept else "%.3f" % share, len(kept),
                        "" if not kept else "%.4f" % float(np.median(kept)),
                        sum(1 for b in beyond if b > GRIP_M), "" if not beyond else "%.4f" % np.nanmax(beyond),
                        mode(e["rows"][0])])
    for name, _, _ in sources:
        mine = [x for x in results if x[0]["source"] == name]
        seen = [x for x in mine if x[3]]
        if not mine:
            say("  %-13s none in this run" % name)
            continue
        missed = [x for x in seen if not x[6]]
        kept = [x for x in seen if x[4]]
        say("  %-13s %3d holds (%d while she was drawn): reached %d of %d%s; kept from %.1f s in: median share %.2f, "
            "lowest %.2f" % (name, len(mine), len(seen), len(seen) - len(missed), len(seen),
                             "" if not missed else " (MISSED %d)" % len(missed), REACH_S,
                             pct([x[7] for x in kept], .5) if kept else math.nan,
                             min(x[7] for x in kept) if kept else math.nan))
        for x in sorted(missed, key=lambda x: -x[5])[:6]:
            say("    missed: nearest %.3f m, %.2f s held from t=%.2f at (%.2f, %.2f, %.2f) (%s)" % (
                x[5], x[2] - x[1], x[1], x[0]["at"][0], x[0]["at"][1], x[0]["at"][2], mode(x[0]["rows"][0])))
        poor = [x for x in kept if x[7] < 0.9]
        for x in sorted(poor, key=lambda x: x[7])[:6]:
            far = [b for b in x[0]["beyond"] if b > GRIP_M]
            say("    kept %.2f of %d frames, median %.3f m, %.2f s from t=%.2f at (%.2f, %.2f, %.2f) (%s); out of her "
                "arms' reach on %d frames, up to %.2f m beyond" % (
                    x[7], len(x[4]), float(np.median(x[4])), x[2] - x[1], x[1], x[0]["at"][0], x[0]["at"][1],
                    x[0]["at"][2], mode(x[0]["rows"][0]), len(far), max(far) if far else 0.0))

    with open(os.path.join(out_dir, "summary.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    for path in sys.argv[1:]:
        score(path)
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
