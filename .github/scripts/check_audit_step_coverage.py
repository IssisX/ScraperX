#!/usr/bin/env python3
"""Check that every non-script proof step of godot-audits-and-mix runs in CI.

The godot-test guard covers only scripts invoked with
`--script res://tests/...`. Five proof steps in godot-audits-and-mix
run no such script: the Fold-aspect runtime proof, the rendered summit
ending, the audio mix, the world-solids table check, and the floating
audit drive the game or an exporter by other flags instead. Drop one
of those steps and CI stays green while the proof it carried is gone
-- the same silent-drop hole the CTest, touch, godot and drawn-body
guards closed for their jobs. This script is their counterpart for
those five steps: it reads the step names out of the workflow file
itself, so it checks the workflow as written, not a copy of its steps.

Rules, all read from the repo as written:
  - the godot-audits-and-mix job must exist;
  - each required step must appear exactly once, with no `if:`
    condition (job-level scope gating is the only gate; a step-level
    condition could skip the proof while the job runs);
  - each required step must be a `run:` step whose block holds at
    least one proof assertion outside a comment: a grep, a diff, or
    a test. A step gutted to a bare run fails, not passes.

A step renamed here must be renamed in REQUIRED in the same change,
never on its own, or the next run fails the step as missing.

Usage:
    python3 .github/scripts/check_audit_step_coverage.py \
        --workflow .github/workflows/wo000-delivery-spine.yml
"""

import argparse
import re
import sys
from pathlib import Path

JOB = "godot-audits-and-mix"

# Proof steps in godot-audits-and-mix that invoke no res://tests/
# script, so the godot-test guard cannot see them. Every other proof
# step in the job is a --script step and is covered there.
REQUIRED = [
    "Run Godot at Fold aspect through native machine authority and capture proof",
    "Render the complete summit ending through touch",
    "Measure the mix a player hears",
    "World solids table matches the Godot builders",
    "Nothing floats",
]

JOB_RE = re.compile(r"^  ([A-Za-z0-9_-]+):\s*$")
STEP_RE = re.compile(r"^      - name: (.+?)\s*$")
IF_RE = re.compile(r"^\s*if:\s*")
RUN_RE = re.compile(r"^\s*run:\s*\|")


def job_steps(workflow_text):
    """Return (counts, {step name: block lines}) for the audits job, or None.

    A duplicate step name keeps one block; the count check in main
    fails it rather than silently using the first block.
    """
    lines = workflow_text.splitlines()
    counts = {}
    ordered = {}
    current = None
    in_job = False
    for line in lines:
        job = JOB_RE.match(line)
        if job is not None:
            if in_job:
                break
            in_job = job.group(1) == JOB
            current = None
            continue
        if not in_job:
            continue
        step = STEP_RE.match(line)
        if step is not None:
            current = step.group(1)
            counts[current] = counts.get(current, 0) + 1
            ordered.setdefault(current, [])
            continue
        if current is not None:
            ordered[current].append(line)
    if not counts:
        return None
    return counts, ordered


def _is_proof_line(line):
    """True for a non-comment grep/diff/test assertion line."""
    stripped = line.strip()
    if not stripped or stripped.startswith("#"):
        return False
    if stripped.startswith(("grep ", "grep\t", "diff ", "diff\t",
                            "test ", "test\t")):
        return True
    if stripped.startswith(("! grep ", "if grep ", "if ! grep ")):
        return True
    return False


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workflow", default=root / ".github/workflows/wo000-delivery-spine.yml")
    args = parser.parse_args()

    found = job_steps(Path(args.workflow).read_text())
    if found is None:
        print(f"FAIL: no job {JOB} in workflow")
        return 1
    counts, steps = found

    failures = []
    for name in REQUIRED:
        if counts.get(name, 0) == 0:
            failures.append(f"missing audit proof step: {name}")
            continue
        if counts[name] > 1:
            failures.append(f"duplicate audit proof step: {name}")
            continue
        block = steps[name]
        if any(IF_RE.match(line) for line in block):
            failures.append(f"conditional audit proof step, must run unconditionally: {name}")
            continue
        if not any(RUN_RE.match(line) for line in block):
            failures.append(f"audit proof step is not a run step: {name}")
            continue
        if not any(_is_proof_line(line) for line in block):
            failures.append(f"audit proof step has no grep/diff/test assertion: {name}")

    if failures:
        print("FAIL: audit proof step coverage gaps:")
        for failure in failures:
            print(f"  {failure}")
        return 1
    print(
        f"PASS: {len(REQUIRED)} audit proof steps present in {JOB}, "
        "each with a proof assertion"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
