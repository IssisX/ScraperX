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
           hip, knee and ankle). Hair and eyes are not bones she poses and are skipped. Each frame is
           counted under the one cause it is drawn in, so a known cause covers only its own frames:
             stretch_arm_hold   a hold (score.HOLD_STATES), or its reach fading after it (hold_weight)
             stretch_arm_reach  her stock reach (the probe's stock_reach column: carrying or seeking a handle)
             stretch_arm_turn, stretch_leg_turn
                                the Turn (turn > 0), whose limb_scale 1.3 (matron_view.gd) draws her arms 1.3x
                                and her thighs and shins 1.06x by design
             stretch_arm, stretch_leg
                                every other frame
  grip_hold, grip_climb
           while the sim says a hand of hers holds something, the drawn hand (the line from a wrist to its
           knuckles, the nearer hand) more than GRIP_M from the sim's grip point: the hold (the held ankle,
           where her fingers hold him by the native's grip, the point her drawn hand closes on, not her
           hand's commanded target, which it lags; from GRIP_FROM_S into the hold; a held frame without a
           held ankle is a failure) and the climb or pipe grip (contact_hand and contact_hand2, when the
           native says they are held).
  finger_wrap
           while the sim says a hand of hers holds something, a drawn finger of that hand more than
           FINGER_M (2 cm) off the surface it holds: the joints past the base joint (f01, f02 and the tip
           of each finger; t1 and the tip of the thumb) against the held body's surface (leaf_surface: the
           box, cylinder or capsule the grip closes round, its size, centre and frame, as the probe
           records it). The base joint (f00, t0) is the palm's edge, covered by the palm-gap check. A held
           grip whose surface the recording cannot resolve is a failure, not a quiet pass. Measured from
           GRIP_FROM_S into the grip, so the wrap's spring has settled. The right hand on the climb or
           pipe grip (contact_hand), the left on the second hold (contact_hand2) or on the pipe's same
           surface; in a hold the right hand on the held ankle and the left, in the rip (ArmPlay), on his
           arm — the same capsule.
  plant    a drawn foot that is down (its lowest point within PLANT_M of the surface under it) while she
           stands still on a floor (not climbing, held, knocked down or turning; under STILL_MPS), more than
           PLANT_DRIFT_M across the ground from where it came down. A jump of the root over TELEPORT_M in a
           frame (a restore) starts the plants again.
  hair, skirt
           hair points (or skirt points) inside her drawn body past the probe's HIT_M (3 mm), her bind
           pose's layering already taken away (score.py); sampled every STRIDE frames (the hair solver is
           the slow part).

Not measured: limb vertices inside her torso (only hair and skirt are scored against her body).

Counting: each check counts failing frames (a plant, slides; hair and skirt, points inside) and keeps the
worst amount, per scenario.

EXPECTED names a known defect, keyed by (check, scenario), with the exit-board item that fixes it and
today's failing count and worst amount. The tolerance stays the law; a known defect is marked, never
tolerated, and is strict both ways:
  - more failures than COUNT_MARGIN over its count, or a worst amount more than PEAK_MARGIN over its
    worst, is a regression and FAILS;
  - no failure at all is an XPASS and FAILS, so the entry is removed;
  - a failure in a (check, scenario) with no entry FAILS.
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
FINGER_M = 0.02          # a drawn finger more than this off the surface it holds fails
FINGER_SEG_M = 0.04      # a finger segment's length (matron_view.gd)
THUMB_SEG_M = 0.035      # a thumb segment's length
STILL_MPS = 0.15
TURN_RAD_S = 0.4           # a body turning faster than this steps its feet round
TELEPORT_M = 1.0
PLANT_M = score.PLANT_M
PLANT_DRIFT_M = 0.05
HIT_M = score.HIT_M
MIN_REST_M = 0.01          # a bone shorter at rest than this has no length to measure
ARM_BONES = ("sh_", "el_")
CHECKS = ("stretch_arm", "stretch_arm_hold", "stretch_arm_reach", "stretch_arm_turn", "stretch_leg",
          "stretch_leg_turn", "grip_hold", "grip_climb", "finger_wrap", "plant", "hair", "skirt")
UNIT = {c: "%" for c in CHECKS if c.startswith("stretch")}
# A known defect's bounds: its failing count may grow by this fraction (at least COUNT_MIN frames) and its
# worst amount by this fraction, before it reads as a regression (frame timing moves a few frames run to run).
COUNT_MARGIN = 0.10
COUNT_MIN = 3
PEAK_MARGIN = 0.05
# Her drawn limbs come back from the Turn's limb_scale over the frames after it ends; those frames are still
# the Turn's. Since #170 (rule 33) the Turn's long arm is her shoulder wrenched out of its socket (sh_ drawn
# 1.86x, matron_rig.gd), and it eases back in for up to 0.58 s (35 frames at 60 fps, touch_matron_act,
# measured 2026-11-04); her legs, within 10 frames.
TURN_TAIL_S = 0.7
# (check, scenario) -> (the exit-board item that fixes it, today's failing count, today's worst amount).
# Measured on CI (godot-touch touch-2's drawn-body step); see README.md for the run. touch_matron_act's rows
# re-measured 2026-11-04 on trunk after J-032 (#170, #172, #175: her pipe tuck vault, the toolbox in her real
# arm, the Turn's arm as her shoulder out of its socket); the look scenarios' rows are from 2026-11-02.
HOLD_ITEM = "exit item 4, holds at a real arm's length (hold worker, PR #37)"
REACH_ITEM = "her stock reach with real arms (Builder job 3; PROP_STRETCH_MAX 2.7, matron_rig.gd two-handed reach)"
TURN_ITEM = "the Turn's limb_scale 1.3 (matron_view.gd; her arm's as her shoulder out of its socket), by design; the owner's call (rule 36)"
CLIMB_ITEM = "no exit item yet: her drawn hands are off the climb's grip (reported 2026-10-29)"
PLANT_ITEM = "no exit item yet: a foot slides up to 7.5 cm while she is nearly still (reported 2026-10-29)"
HAIR_ITEM = "exit item 10, hair"
SKIRT_ITEM = "exit item 10, hair and cloth (the skirt's baseline is her bind pose, an A-pose)"
EXPECTED = {
    ("stretch_arm_hold", "touch_matron_act"): (HOLD_ITEM, 2045, 204.684),
    ("stretch_arm_reach", "touch_matron_act"): (REACH_ITEM, 15, 7.529),
    ("stretch_arm_turn", "touch_matron_act"): (TURN_ITEM, 605, 85.653),
    ("stretch_leg_turn", "touch_matron_act"): (TURN_ITEM, 520, 6.000),
    ("grip_climb", "touch_matron_act"): (CLIMB_ITEM, 2056, 1.034),
    ("plant", "touch_matron_act"): (PLANT_ITEM, 50, 0.075),
    ("hair", "touch_matron_act"): (HAIR_ITEM, 14751, 0.193),
    ("skirt", "touch_matron_act"): (SKIRT_ITEM, 17495, 0.156),
    ("grip_climb", "look_matron_moves"): (CLIMB_ITEM, 1494, 1.556),
    ("hair", "look_matron_moves"): (HAIR_ITEM, 1617, 0.192),
    ("skirt", "look_matron_moves"): (SKIRT_ITEM, 1415, 0.114),
    ("stretch_arm_hold", "look_matron_hold"): (HOLD_ITEM, 634, 206.990),
    ("grip_hold", "look_matron_hold"): (HOLD_ITEM, 19, 0.183),
    ("hair", "look_matron_hold"): (HAIR_ITEM, 495, 0.040),
    ("skirt", "look_matron_hold"): (SKIRT_ITEM, 506, 0.086),
    ("stretch_arm_hold", "look_matron_fatal"): (HOLD_ITEM, 373, 198.334),
    ("hair", "look_matron_fatal"): (HAIR_ITEM, 279, 0.040),
    ("skirt", "look_matron_fatal"): (SKIRT_ITEM, 291, 0.086),
}


def stretch_cause(rows, i, turning):
    """The one cause a frame is drawn in, for the stretch checks: hold, reach, turn, or "" (none)."""
    if int(rows["hold_state"][i]) in score.HOLD_STATES or ("hold_weight" in rows and rows["hold_weight"][i] > 0.001):
        return "hold"
    if "stock_reach" in rows and rows["stock_reach"][i] > 0:
        return "reach"
    if turning:
        return "turn"
    return ""


def stretch_failures(run):
    """check -> (pct over, what, rec, t) per failing frame: its worst stretched bone that frame."""
    nb = len(run.bone_names)
    origin = score.xf(run.bones_raw.reshape(run.nrec, nb, 12))[..., :, 3]
    parent = run.parent
    rest = np.array([np.linalg.norm(b["rest"][9:12]) for b in run.meta["bones"]])
    skip = np.array([n.startswith("hair") or n.startswith("eye") for n in run.bone_names])
    use = (parent >= 0) & (rest > MIN_REST_M) & ~skip
    idx = np.nonzero(use)[0]
    arm = np.array([run.bone_names[b].startswith(ARM_BONES) for b in idx])
    length = np.linalg.norm(origin[:, idx] - origin[:, parent[idx]], axis=2)
    ratio = length / rest[idx]
    out = {}
    limit = 1.0 + STRETCH_PCT / 100.0
    rows = run.rows
    last_turn = -1.0e9
    for rec in np.nonzero(run.row_of_rec >= 0)[0]:
        i = run.row_of_rec[rec]
        t = float(rows["t"][i])
        if rows["turn"][i] > 0:
            last_turn = t
        turning = t - last_turn <= TURN_TAIL_S
        cause = stretch_cause(rows, i, turning)
        for limb, mask in (("arm", arm), ("leg", ~arm)):
            r = np.where(mask, ratio[rec], 0.0)
            k = int(np.argmax(r))
            if r[k] > limit:
                # Her legs have one known cause, the Turn (a hold or a reach draws only her arms).
                c = cause if limb == "arm" else ("turn" if turning else "")
                check = "stretch_" + limb + ("_" + c if c else "")
                bone = run.bone_names[idx[k]]
                what = "%s drawn %.2fx its rest length (state %d, turn %d%s)" % (
                    bone, r[k], int(rows["state"][i]), int(rows["turn"][i]), ", " + c if c else "")
                out.setdefault(check, []).append(((float(r[k]) - 1.0) * 100.0, what, int(rec), float(rows["t"][i])))
    return out


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


def held_point(rows, i, have_ankle):
    """Where her fingers hold him in a hold: the held ankle (the native's held_ankle, from his body and her
    grip, the point her drawn hand closes on in matron_view.gd _hold_gate), not "hand" (hx, hy, hz: her
    hand's commanded target, which the ankle lags). Her hand's target only where the recording has no held
    ankle (a probe or native from before it, or zero: her fingers are not on him), as the renderer falls
    back to it. Returns (point, True when it is the held ankle)."""
    if have_ankle:
        ankle = np.array([rows["held_anklex"][i], rows["held_ankley"][i], rows["held_anklez"][i]])
        if np.all(np.isfinite(ankle)) and float(np.dot(ankle, ankle)) > 1.0e-6:
            return ankle, True
    return np.array([rows["hx"][i], rows["hy"][i], rows["hz"][i]]), False


def grip_failures(run):
    """(gap, kind, rec, t) for each grip the drawn hand is off, and notes on what was measured against what."""
    rows = run.rows
    out = []
    notes = []
    since = None
    have_ankle = "held_anklex" in rows
    climbs = [(kind, col) for kind, col in (("climb", "contact_hand"), ("climb2", "contact_hand2"))
              if col + "_held" in rows]
    for col in ("contact_hand", "contact_hand2"):
        if col + "_held" not in rows:
            notes.append("grip_climb not measured for %s: the recording has no %s columns" % (col, col))
    on_target = 0
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
            point, ankle = held_point(rows, i, have_ankle)
            if not ankle:
                # The native says she holds him and gives no point her fingers hold: a failure, not a
                # quiet measure against her hand's target.
                on_target += 1
                out.append((float("inf"), "hold, no held ankle", rec, t))
                continue
            want.append(("hold", point))
        for kind, col in climbs:
            if rows[col + "_held"][i] > 0:
                want.append((kind, np.array([rows[col + "x"][i], rows[col + "y"][i], rows[col + "z"][i]])))
        if not want:
            continue
        wb = run.world_bones(rec)
        for kind, point in want:
            gap = palm_gap(run, wb, point)
            if gap > GRIP_M:
                out.append((gap, kind, rec, t))
    if on_target:
        notes.append("grip_hold: %d held frames had no held ankle%s: each is a failure"
                     % (on_target, "" if have_ankle else " column"))
    return out, notes


def _quat_matrix(qx, qy, qz, qw):
    """A unit quaternion's 3x3 rotation matrix."""
    n = float(np.sqrt(qx * qx + qy * qy + qz * qz + qw * qw))
    if n <= 1.0e-9:
        return np.eye(3)
    x, y, z, w = qx / n, qy / n, qz / n, qw / n
    return np.array([
        [1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y - z * w), 2.0 * (x * z + y * w)],
        [2.0 * (x * y + z * w), 1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z - x * w)],
        [2.0 * (x * z - y * w), 2.0 * (y * z + x * w), 1.0 - 2.0 * (x * x + y * y)]])


def _surface(rows, i, prefix):
    """(kind, size, centre, rot) for a grip's surface (leaf_surface), or None
    when the recording has no surface columns for it or the surface is not a
    box, cylinder or capsule with a size."""
    if (prefix + "_shape") not in rows:
        return None
    kind = int(rows[prefix + "_shape"][i])
    if kind not in (0, 1, 2):
        return None
    size = np.array([rows[prefix + "_sizex"][i], rows[prefix + "_sizey"][i], rows[prefix + "_sizez"][i]])
    centre = np.array([rows[prefix + "_centrex"][i], rows[prefix + "_centrey"][i], rows[prefix + "_centrez"][i]])
    rot = np.array([rows[prefix + "_rotx"][i], rows[prefix + "_roty"][i], rows[prefix + "_rotz"][i],
                    rows[prefix + "_rotw"][i]])
    if not (np.all(np.isfinite(size)) and np.all(np.isfinite(centre)) and np.all(np.isfinite(rot))):
        return None
    if float(np.dot(size, size)) <= 1.0e-12:
        return None
    return kind, size, centre, rot


def _surface_distance(surface, point):
    """A point's signed distance to a box (0), cylinder (1) or capsule (2)
    surface: negative inside, positive outside. The shape's local frame is
    its centre and rot; size is the box's half extents or (radius, half
    height, 0)."""
    kind, size, centre, rot = surface
    local = _quat_matrix(*rot).T @ (point - centre)
    if kind == 0:  # a box: the rung or rail she grips
        q = np.abs(local) - size
        return float(np.linalg.norm(np.maximum(q, 0.0)) + min(max(q[0], q[1], q[2]), 0.0))
    if kind == 1:  # a cylinder along local y: the pipe she grips
        q = np.array([float(np.linalg.norm(local[[0, 2]])) - size[0], abs(local[1]) - size[1]])
        return float(np.linalg.norm(np.maximum(q, 0.0)) + min(max(q[0], q[1]), 0.0))
    # A capsule along local y (his body): the distance to its segment, minus the radius.
    along = max(-size[1], min(size[1], local[1]))
    return float(np.linalg.norm(local - np.array([0.0, along, 0.0])) - size[0])


def _palm_gap_hand(run, wb, hand, point):
    """One hand's drawn palm (the line from its wrist to its knuckles) to `point`."""
    wr = wb[run.bone_index["wr_" + hand]]
    a = wr[:3, 3]
    b = score.apply(wr, np.array(run.contacts["wr_" + hand]["knuckles"]))
    ab = b - a
    t = float(np.clip(np.dot(point - a, ab) / max(float(np.dot(ab, ab)), 1.0e-9), 0.0, 1.0))
    return float(np.linalg.norm(point - (a + t * ab)))


def finger_failures(run):
    """(gap, kind, rec, t) for each drawn finger off the surface it holds, and notes."""
    rows = run.rows
    out = []
    notes = []
    have_surface = {}
    for prefix in ("contact_hand", "contact_hand2", "held_ankle"):
        have_surface[prefix] = (prefix + "_shape") in rows
        if not have_surface[prefix]:
            notes.append("finger_wrap not measured for %s: the recording has no %s columns"
                         % (prefix, prefix))
    # Four fingers of three joints and a thumb of two, each joint a pivot
    # (matron_view.gd). The base joint (f00, t0) is the palm's edge, covered
    # by the palm-gap check; the joints past it and the tip must lie on the
    # surface the hand holds.
    chains = {}
    for s in "lr":
        chains[s] = [["f%d%d_%s" % (f, seg, s) for seg in range(3)] for f in range(4)]
        chains[s].append(["t%d_%s" % (seg, s) for seg in range(2)])
    missing = [b for s in "lr" for chain in chains[s] for b in chain if b not in run.bone_index]
    if missing:
        notes.append("finger_wrap not measured: the recording has no finger bones (%s)"
                     % ", ".join(missing[:6]))
        return out, notes
    have_hand = "contact_hand_held" in rows
    have_hand2 = "contact_hand2_held" in rows
    since = {}
    for rec in range(run.nrec):
        i = run.row_of_rec[rec]
        if i < 0:
            continue
        t = rows["t"][i]
        st = int(rows["hold_state"][i])
        held = st in score.HOLD_STATES and rows["held"][i] > 0
        on_pipe = bool(rows["on_pipe"][i])
        # Each hand's surface this frame: the right hand on the climb or pipe
        # grip, or on the held ankle in a hold; the left on the second hold
        # or on the pipe's same surface. The left hand in the rip (ArmPlay)
        # is on his arm, and the sim exports no arm surface, so it is not
        # measured here (measuring it against the ankle reads ~0.7 m, the
        # arm-to-ankle distance, not a finger off its surface).
        want = []
        if have_hand and rows["contact_hand_held"][i] > 0 and have_surface["contact_hand"]:
            want.append(("r", "climb" if not on_pipe else "pipe", "contact_hand"))
        elif held and have_surface["held_ankle"]:
            want.append(("r", "hold", "held_ankle"))
        if have_hand2 and rows["contact_hand2_held"][i] > 0 and have_surface["contact_hand2"]:
            want.append(("l", "climb2", "contact_hand2"))
        elif on_pipe and have_hand and rows["contact_hand_held"][i] > 0 and have_surface["contact_hand"]:
            want.append(("l", "pipe", "contact_hand"))
        if not want:
            since.clear()
            continue
        keys = set()
        wb = run.world_bones(rec)
        for hand, kind, prefix in want:
            key = (hand, kind)
            keys.add(key)
            if key not in since:
                since[key] = t
            if t - since[key] < GRIP_FROM_S:
                continue
            # The fingers are only on a surface the hand itself holds: the
            # palm must be on the grip point (the palm-gap check's bar). A
            # hand the rig has not brought to its grip — a climb's known
            # hand-off-the-rung, already the grip check's expected defect —
            # is not the fingers' failure to measure. The pipe's second hand
            # sits 0.08 m along the pipe from the grip point, so it gets
            # that offset on top of the bar.
            if prefix == "held_ankle" and "held_anklex" in rows:
                point = np.array([rows["held_anklex"][i], rows["held_ankley"][i], rows["held_anklez"][i]])
            else:
                point = np.array([rows[prefix + "x"][i], rows[prefix + "y"][i], rows[prefix + "z"][i]])
            gate = GRIP_M + (0.08 if kind == "pipe" and hand == "l" else 0.0)
            if _palm_gap_hand(run, wb, hand, point) > gate:
                continue
            surface = _surface(rows, i, prefix)
            if surface is None:
                out.append((float("inf"), kind + ", no surface", rec, t))
                continue
            for chain in chains[hand]:
                seg = FINGER_SEG_M if len(chain) == 3 else THUMB_SEG_M
                for bone in chain[1:]:
                    m = wb[run.bone_index[bone]]
                    gap = abs(_surface_distance(surface, m[:3, 3]))
                    if gap > FINGER_M:
                        out.append((gap, kind, rec, t))
                tip_m = wb[run.bone_index[chain[-1]]]
                tip = tip_m[:3, 3] - seg * tip_m[:3, 1]
                gap = abs(_surface_distance(surface, tip))
                if gap > FINGER_M:
                    out.append((gap, kind, rec, t))
        for key in list(since):
            if key not in keys:
                del since[key]
    return out, notes


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


def verdict(check, scenario, items):
    """(word, failed, text) for one (check, scenario)."""
    known = EXPECTED.get((check, scenario))
    count = len(items)
    peak = max((a for a, _, _, _ in items), default=0.0)
    unit = UNIT.get(check, " m")
    if not known:
        if count:
            return "FAIL", True, "%d failures, worst %.3f%s" % (count, peak, unit)
        return "PASS", False, ""
    why, was_count, was_peak = known
    if not count:
        return "XPASS", True, "no failure now; it was expected until %s. Remove it from EXPECTED." % why
    count_bound = max(int(np.ceil(was_count * (1.0 + COUNT_MARGIN))), was_count + COUNT_MIN)
    peak_bound = was_peak * (1.0 + PEAK_MARGIN)
    text = "%d failures (known %d, bound %d), worst %.3f%s (known %.3f, bound %.3f); %s" % (
        count, was_count, count_bound, peak, unit, was_peak, peak_bound, why)
    if count > count_bound or peak > peak_bound:
        return "FAIL", True, "REGRESSION past its known defect: " + text
    return "XFAIL", False, text


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("runs", nargs="+")
    ap.add_argument("--stride", type=int, default=10, help="score hair and skirt every Nth frame")
    ap.add_argument("--report", default="")
    args = ap.parse_args()
    lines = []
    failed = []

    def say(text):
        lines.append(text)
        print(text, flush=True)

    ran = set()
    for path in args.runs:
        run = score.Run(path)
        name = os.path.basename(os.path.normpath(path))
        ran.add(name)
        frames = int((run.row_of_rec >= 0).sum())
        say("== %s: %d drawn frames at %d fps" % (name, frames, run.fps))
        if "stock_reach" not in run.rows:
            say("  note: the recording has no stock_reach column: reach frames count as stretch_arm")
        found = {c: [] for c in CHECKS}
        for check, items in stretch_failures(run).items():
            found[check].extend(items)
        grips, notes = grip_failures(run)
        for gap, kind, rec, t in grips:
            found["grip_hold" if kind.startswith("hold") else "grip_climb"].append((gap, kind, rec, t))
        for note in notes:
            say("  note: " + note)
        fingers, notes = finger_failures(run)
        for gap, kind, rec, t in fingers:
            found["finger_wrap"].append((gap, kind, rec, t))
        for note in notes:
            say("  note: " + note)
        for drift, foot, rec, t in plant_failures(run):
            found["plant"].append((drift, foot, rec, t))
        pen, secs = penetration_failures(run, args.stride)
        found["hair"], found["skirt"] = pen["hair"], pen["skirt"]
        for check in CHECKS:
            items = sorted(found[check], reverse=True)
            word, bad, text = verdict(check, name, items)
            say("  %-5s %-17s %s" % (word, check, text or "%d failures" % len(items)))
            if items and word != "PASS":
                for amount, what, rec, t in items[:6]:
                    say("        frame %s:%d t=%.2f s: %s %.3f%s" % (name, rec, t, what, amount, UNIT.get(check, " m")))
            if bad:
                failed.append("%s/%s" % (check, name))
        say("  (hair and skirt scored every %d frames: %.0f s)" % (args.stride, secs))
    for (check, scenario) in sorted(EXPECTED):
        if scenario not in ran:
            say("note: %s/%s expected but its scenario was not recorded" % (check, scenario))
    if args.report:
        with open(args.report, "w") as fh:
            fh.write("\n".join(lines) + "\n")
    print("MATRON_DRAWN %s" % ("FAIL: " + ", ".join(failed) if failed else "PASS"))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
