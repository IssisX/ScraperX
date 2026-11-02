#!/usr/bin/env python3
"""Read-only drawn-hair ribbon width diagnostic for a Matron probe recording.

    python3 tools/matron_probe/ribbon_width.py <recording> --records 100,8163,10468
    python3 tools/matron_probe/ribbon_width.py <recording> --stride 60 --top 5

Uses score.py's exact recorder, ribbon pairing, and CPU skinning. Reports
geometry, not body penetration or an owner-approved pass/fail threshold.
"""
import argparse

import numpy as np

from score import Hair, Run, STATE_NAMES, dominant_bone, skin


def ribbon_widths(vertices, left, right):
    """Return Euclidean width for each paired ribbon edge, in meters."""
    return np.linalg.norm(vertices[left] - vertices[right], axis=1)


def record_indices(run, requested, stride):
    if requested:
        try:
            indices = [int(token.strip()) for token in requested.split(",")]
        except ValueError as exc:
            raise ValueError("--records requires comma-separated integer record indices") from exc
    else:
        indices = list(range(0, run.nrec, stride))
    if any(i < 0 or i >= run.nrec for i in indices):
        raise ValueError("record index outside 0..%d" % (run.nrec - 1))
    return list(dict.fromkeys(indices))


def analyze(run, records, top, report_over_mm):
    hair = Hair(run)
    part = hair.part
    left, right = hair.a, hair.b
    count = len(left)
    rest = ribbon_widths(part.v, left, right)
    dominant = dominant_bone(part)
    mismatch = dominant[left] != dominant[right]
    fully_weighted = (part.w[left].max(axis=1) >= 0.999) & (part.w[right].max(axis=1) >= 0.999)
    print("ribbons=%d cross_bone=%d fully_weighted_cross_bone=%d threshold=%.1f_mm (reporting only)" % (
        count, np.count_nonzero(mismatch), np.count_nonzero(mismatch & fully_weighted), report_over_mm))
    selected, inverse = np.unique(np.r_[left, right], return_inverse=True)
    for rec in records:
        posed = skin(part, run.world_bones(rec), sel=selected)
        drawn = np.linalg.norm(posed[inverse[:count]] - posed[inverse[count:]], axis=1)
        if not np.all(np.isfinite(drawn)):
            raise ValueError("nonfinite drawn hair width at record %d" % rec)
        row = int(run.row_of_rec[rec])
        frame = int(run.rows["f"][row]) if row >= 0 else -1
        state = STATE_NAMES.get(int(run.rows["state"][row]), "unknown") if row >= 0 else "unknown"
        print("record=%d frame=%d state=%s max_mm=%.3f over_%.1f_mm=%d cross_bone_over=%d" % (
            rec, frame, state, 1000 * drawn.max(), report_over_mm,
            np.count_nonzero(drawn * 1000 > report_over_mm),
            np.count_nonzero((drawn * 1000 > report_over_mm) & mismatch)))
        for j in np.argsort(drawn)[-top:][::-1]:
            a, b = int(left[j]), int(right[j])
            ratio = drawn[j] / rest[j] if rest[j] > 1e-9 else float("nan")
            print("  pair=%d vertices=%d/%d chain=%d seg=%d bones=%s/%s rest_mm=%.3f drawn_mm=%.3f ratio=%.2f cross_bone=%s" % (
                j, a, b, int(hair.chain[j]), int(hair.seg[j]),
                run.bone_names[int(dominant[a])], run.bone_names[int(dominant[b])],
                1000 * rest[j], 1000 * drawn[j], ratio, bool(mismatch[j])))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("recording", help="directory containing meta.json, mesh.bin, bones.bin and frames.csv")
    ap.add_argument("--records", default="", help="comma-separated drawn-record indices; overrides --stride")
    ap.add_argument("--stride", type=int, default=60, help="sample every Nth drawn record (default 60)")
    ap.add_argument("--top", type=int, default=3, help="show N widest ribbon pairs per record")
    ap.add_argument("--report-over-mm", type=float, default=100.0, help="descriptive width count, not a pass/fail gate")
    args = ap.parse_args(argv)
    if args.stride < 1 or args.top < 1 or not np.isfinite(args.report_over_mm) or args.report_over_mm <= 0:
        ap.error("--stride, --top and --report-over-mm must be positive finite values")
    try:
        run = Run(args.recording)
        indices = record_indices(run, args.records, args.stride)
        analyze(run, indices, args.top, args.report_over_mm)
    except (OSError, ValueError, KeyError, IndexError) as exc:
        ap.error("ribbon-width recording analysis failed for %s: %s" % (args.recording, exc))


if __name__ == "__main__":
    main()
