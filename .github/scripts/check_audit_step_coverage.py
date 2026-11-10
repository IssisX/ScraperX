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

This script's own logic has the same silent-drop shape one layer
further up: if its step parser or proof-line check regresses, no other
guard reads the godot-audits-and-mix non-script steps, so a broken
audits guard passes the real workflow while an audit proof step goes
unverified -- proven 2026-11-05 by neutering its step-level `if:`
detector and watching it PASS a workflow with a conditional proof
step. So its parser regressions live in `_self_test()` below and run
on every invocation, `--self-test` or not, the same pattern as the
matrix, guard-step and apk-step guards: a logic regression fails its
own run before it can pass a real workflow.

Usage:
    python3 .github/scripts/check_audit_step_coverage.py \
        --workflow .github/workflows/wo000-delivery-spine.yml

    python3 .github/scripts/check_audit_step_coverage.py --self-test
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


def check_text(workflow_text):
    """Check workflow text against REQUIRED.

    Returns (problems, checked): problems is the list of failure
    strings (empty when the workflow is clean), checked is the count
    of proof steps found exactly once. The caller prints them; this
    function does no I/O, so the self-test can run it on fixture text.
    """
    found = job_steps(workflow_text)
    if found is None:
        return [f"no job {JOB} in workflow"], 0
    counts, steps = found

    failures = []
    checked = 0
    for name in REQUIRED:
        if counts.get(name, 0) == 0:
            failures.append(f"missing audit proof step: {name}")
            continue
        if counts[name] > 1:
            failures.append(f"duplicate audit proof step: {name}")
            continue
        checked += 1
        block = steps[name]
        if any(IF_RE.match(line) for line in block):
            failures.append(f"conditional audit proof step, must run unconditionally: {name}")
            continue
        if not any(RUN_RE.match(line) for line in block):
            failures.append(f"audit proof step is not a run step: {name}")
            continue
        if not any(_is_proof_line(line) for line in block):
            failures.append(f"audit proof step has no grep/diff/test assertion: {name}")

    return failures, checked


def _self_test():
    """Regression checks for the audit-step guard parser, run on every invocation."""
    def step(name, body):
        return (f"      - name: {name}\n"
                "        run: |\n"
                f"          {body}\n")

    fold = step("Run Godot at Fold aspect through native machine authority and capture proof",
                'grep -q "proof" fold.txt')
    summit = step("Render the complete summit ending through touch",
                  'grep -q "summit" summit.txt')
    mix = step("Measure the mix a player hears", "test -f mix.wav")
    solids = step("World solids table matches the Godot builders",
                  "diff solids.txt expected.txt")
    floats = step("Nothing floats", 'grep -q "float" audit.txt')

    valid = ("name: test\njobs:\n"
             "  godot-audits-and-mix:\n    steps:\n"
             + fold + summit + mix + solids + floats)
    cases = []

    def case(name, text, want):
        problems, _checked = check_text(text)
        joined = "\n".join(problems)
        if want is None:
            if problems:
                raise AssertionError(f"{name}: expected clean, got {problems}")
        elif want not in joined:
            raise AssertionError(f"{name}: expected {want!r} in {problems}")
        cases.append(name)

    case("valid", valid, None)
    case("dropped-step",
         valid.replace(floats, "", 1),
         "missing audit proof step: Nothing floats")
    case("duplicated-step",
         valid.replace(floats, floats + floats, 1),
         "duplicate audit proof step: Nothing floats")
    case("conditional-step",
         valid.replace("      - name: Nothing floats\n",
                       "      - name: Nothing floats\n"
                       "        if: github.event_name == 'push'\n", 1),
         "conditional audit proof step, must run unconditionally: Nothing floats")
    case("no-run-block",
         valid.replace('        run: |\n          grep -q "float" audit.txt\n',
                       "        uses: actions/checkout@v4\n", 1),
         "audit proof step is not a run step: Nothing floats")
    case("gutted-step",
         valid.replace('grep -q "float" audit.txt', "echo hi", 1),
         "audit proof step has no grep/diff/test assertion: Nothing floats")
    case("comment-only-assertion",
         valid.replace('grep -q "float" audit.txt', '# grep -q "float" audit.txt', 1),
         "audit proof step has no grep/diff/test assertion: Nothing floats")
    case("bang-grep-counts",
         valid.replace('grep -q "float" audit.txt', '! grep -q "float" audit.txt', 1),
         None)
    case("missing-job",
         valid.replace("  godot-audits-and-mix:\n    steps:\n", "  other-job:\n    steps:\n", 1),
         "no job godot-audits-and-mix in workflow")
    return len(cases)


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workflow", default=root / ".github/workflows/wo000-delivery-spine.yml")
    parser.add_argument("--self-test", action="store_true",
                        help="run parser regressions only")
    args = parser.parse_args()

    fixture_count = _self_test()
    if args.self_test:
        print(f"PASS: {fixture_count} audit-step coverage parser regression checks")
        return 0

    problems, checked = check_text(Path(args.workflow).read_text())
    if problems:
        print("FAIL: audit proof step coverage gaps:")
        for problem in problems:
            print(f"  {problem}")
        return 1
    print(
        f"PASS: {checked} audit proof steps present in {JOB}, "
        "each with a proof assertion"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
