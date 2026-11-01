#!/usr/bin/env python3
"""Check that every godot test script runs in CI or is a known gap.

The godot-audits-and-mix job runs headless SceneTree scripts from
godot/tests/ by hand-written workflow steps. A script in neither the
workflow nor a known-gap entry never runs in CI, and CI green would
say nothing about it -- the same silent-drop hole the CTest and touch
shard guards closed for their jobs. This script is their counterpart
for godot tests: it reads the invoked scripts out of the workflow
file itself, so it checks the workflow as written, not a copy of its
steps, and compares them against the *.gd files in godot/tests/.

Scope is godot/tests/*.gd only. Scripts elsewhere (the solids export,
the floating audit, the touch driver) are invoked by their own steps
and are not double-counted here. Running a script twice would be
wasteful, not a coverage hole, so duplicates are not failed on.

KNOWN_GAPS names godot test scripts that are in no workflow step for
a tracked reason. Each prints as a WARNING and does not fail the
guard, so one known gap cannot hide a new one: any other script with
zero invocations still fails, and a KNOWN_GAPS entry that becomes
invoked fails as stale -- remove it only together with the workflow
step that now runs it, never on its own, or the next run fails the
script as uncovered.

Usage:
    python3 .github/scripts/check_godot_test_coverage.py \
        --workflow .github/workflows/wo000-delivery-spine.yml \
        --test-dir godot/tests
"""

import argparse
import re
import sys
from pathlib import Path

# Godot test scripts in no workflow step for a tracked reason. Warning,
# not failure; stale the moment the script appears in the workflow.
KNOWN_GAPS = {
    "gore_shots_test.gd": (
        "studio deck, renders PNGs with a display; renders-only, not "
        "a CI gate"
    ),
    "hair_pose_probe.gd": (
        "studio measurement probe, writes a probe directory for the "
        "scorer; no pass/fail verdict, not a CI gate"
    ),
    "matron_presentation_test.gd": (
        "pending the presentation landing: FAILs on trunk in the "
        "mantle section, so gating it now would turn the audits job "
        "red; add its audits step together with removing this entry"
    ),
}

INVOCATION_RE = re.compile(r"res://tests/([A-Za-z0-9_]+\.gd)")


def invoked_scripts(workflow_text):
    """Return the res://tests/*.gd names invoked anywhere in the workflow."""
    return set(INVOCATION_RE.findall(workflow_text))


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workflow", default=root / ".github/workflows/wo000-delivery-spine.yml")
    parser.add_argument("--test-dir", default=root / "godot/tests")
    args = parser.parse_args()

    workflow_text = Path(args.workflow).read_text()
    test_dir = Path(args.test_dir)
    if not test_dir.is_dir():
        raise SystemExit(f"check_godot_test_coverage: no test dir {test_dir}")
    files = sorted(p.name for p in test_dir.glob("*.gd"))
    if not files:
        raise SystemExit("check_godot_test_coverage: no .gd files in test dir")
    invoked = invoked_scripts(workflow_text)

    print(f"godot tests on disk: {len(files)}")
    print(f"godot tests invoked in workflow: {len(invoked)}")

    failures = []
    for name in sorted(invoked - set(files)):
        failures.append(f"invoked but no such test file: {name}")

    for name in files:
        if name in invoked:
            continue
        if name in KNOWN_GAPS:
            print(f"WARNING known gap, in no workflow step: {name} ({KNOWN_GAPS[name]})")
        else:
            failures.append(f"test file in no workflow step: {name}")

    stale = [n for n in sorted(KNOWN_GAPS) if n in invoked]
    for name in stale:
        failures.append(f"stale known gap, now invoked -- remove from KNOWN_GAPS: {name}")

    if failures:
        print("FAIL: godot test coverage gaps:")
        for failure in failures:
            print(f"  {failure}")
        return 1
    print(
        f"PASS: {len(invoked)} godot tests invoked in CI, "
        f"{len(KNOWN_GAPS) - len(stale)} known gap(s) warned"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
