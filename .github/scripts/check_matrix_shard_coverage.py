#!/usr/bin/env python3
"""Check that every CI matrix shard actually runs its proof step.

The native-tests and godot-touch jobs split their proof across a
strategy.matrix.shard list, and each proof step runs only when its
step-level `if: matrix.shard == '<shard>'` matches the leg. The
matrix is a silent drop in three directions, and no other guard
reads it: a shard dropped from the matrix never gets a leg, so its
step never runs; a step whose `if:` names a shard not in the matrix
never runs on any leg; a shard left in the matrix with no step
gated on it runs a leg that proves nothing. In all three cases the
ctest/touch shard guards still read the step text (the -R/-E
pattern, the `for scenario in` list) and pass, while the proof
that shard carried never runs.

Proven 2026-11-05: on workflow copies with matron-slices-3
dropped from the native-tests matrix, with touch-2 dropped from
the godot-touch matrix, and with the matron-slices-3 step retargeted
to a phantom 'matron-slices-9', all six trunk guards and the three
pending workflow-reading guards (audits, scope-steps, guard-steps)
pass.

Rules, all read from the workflow as written:
  - each matrix job must exist and declare strategy.matrix.shard
    as exactly the expected list -- no missing, extra, or
    duplicated shard (a shard renamed here must be renamed in
    EXPECTED in the same change, never on its own);
  - every step `if:` naming matrix.shard must name a shard in
    that job's matrix, with exactly the form
    `matrix.shard == '<shard>'`;
  - every matrix shard must gate at least one `run:` step -- a
    leg with no gated proof step proves nothing.

The job-gate guard pins job-level gates; this guard pins the
step-level matrix gates below them. It is wired in the scope job
itself, so it runs on every push, and in host-build too, exactly
once, which backstops the scope-job wiring and keeps the
guard-of-guards passing. A script deleted together with both of
its steps passes by design -- a deliberate retirement, visible
as a file deletion, not a drop.

Usage:
    python3 .github/scripts/check_matrix_shard_coverage.py \
        --workflow .github/workflows/wo000-delivery-spine.yml
"""

import argparse
import re
import sys
from pathlib import Path

JOB_HEADER = re.compile(r"^  ([A-Za-z0-9_-]+):\s*$")
STRATEGY_LINE = re.compile(r"^    strategy:\s*$")
MATRIX_LINE = re.compile(r"^      matrix:\s*$")
SHARD_LINE = re.compile(r"^        shard:\s*$")
MATRIX_ITEM = re.compile(r"^          - (.+?)\s*$")
STEP_HEADER = re.compile(r"^      - name: (.+?)\s*$")
STEP_IF = re.compile(r"^        if:\s*(.+?)\s*$")
STEP_RUN = re.compile(r"^        run:\s*")
MATRIX_GATE = re.compile(r"matrix\.shard\s*==\s*['\"]([^'\"]+)['\"]")

# job -> exact strategy.matrix.shard list, in workflow order. These are
# the shards the CTest guard (native-tests) and the touch-shard guard
# (godot-touch) already read the proof of; this guard pins the legs
# that decide whether that proof runs.
EXPECTED = {
    "native-tests": ["athletic-traversal-1", "athletic-traversal-2",
                     "matron-slices-1", "matron-slices-2", "matron-slices-3"],
    "godot-touch": ["touch-1", "touch-2"],
}


def job_blocks(workflow_text):
    """Split the workflow into {job name: lines after the header}."""
    jobs = {}
    current = None
    for line in workflow_text.splitlines():
        match = JOB_HEADER.match(line)
        if match:
            current = match.group(1)
            jobs[current] = []
        elif current is not None:
            jobs[current].append(line)
    return jobs

def parse_matrix(header_lines):
    """The strategy.matrix.shard list of a job block, or None."""
    in_strategy = False
    in_matrix = False
    in_shard = False
    items = []
    for line in header_lines:
        if STRATEGY_LINE.match(line):
            in_strategy = True
            continue
        if not in_strategy:
            continue
        if MATRIX_LINE.match(line):
            in_matrix = True
            continue
        if not in_matrix:
            continue
        if SHARD_LINE.match(line):
            in_shard = True
            continue
        if in_shard:
            match = MATRIX_ITEM.match(line)
            if match:
                items.append(match.group(1).strip())
                continue
            if line.strip():
                break
    return items if in_shard else None

def parse_steps(header_lines):
    """The job's steps as [{name, if, has_run}], in workflow order."""
    steps = []
    current = None
    for line in header_lines:
        match = STEP_HEADER.match(line)
        if match:
            current = {"name": match.group(1), "if": None, "has_run": False}
            steps.append(current)
            continue
        if current is None:
            continue
        match = STEP_IF.match(line)
        if match:
            current["if"] = match.group(1).strip()
        elif STEP_RUN.match(line):
            current["has_run"] = True
    return steps

def check_text(workflow_text):
    """Return the list of problems found in this workflow text."""
    jobs = job_blocks(workflow_text)
    problems = []
    for job, expected in EXPECTED.items():
        if job not in jobs:
            problems.append(f"missing: no {job} job in workflow")
            continue
        header = jobs[job]
        matrix = parse_matrix(header)
        if matrix is None:
            problems.append(f"missing-matrix: {job} declares no strategy.matrix.shard list")
            continue
        for shard in sorted(set(matrix)):
            if matrix.count(shard) > 1:
                problems.append(
                    f"duplicate-matrix: {job} lists matrix shard {shard!r} "
                    f"{matrix.count(shard)} times")
        for shard in sorted(set(expected) - set(matrix)):
            problems.append(
                f"missing-matrix-shard: {job} matrix has no {shard!r}; "
                f"its proof step never runs")
        for shard in sorted(set(matrix) - set(expected)):
            problems.append(
                f"unexpected-matrix-shard: {job} matrix lists {shard!r}, "
                f"which no shard guard expects")
        steps = parse_steps(header)
        gated = {}
        for step in steps:
            gate = step["if"]
            if gate is None or "matrix.shard" not in gate:
                continue
            match = MATRIX_GATE.search(gate)
            if match is None or MATRIX_GATE.search(gate).group(0) != gate.strip():
                problems.append(
                    f"unrecognized-matrix-gate: {job} step {step['name']!r} "
                    f"gates on {gate!r}, expected matrix.shard == '<shard>'")
                continue
            shard = match.group(1)
            if shard not in matrix:
                problems.append(
                    f"phantom-gate: {job} step {step['name']!r} gates on "
                    f"matrix.shard == {shard!r}, which is not in the matrix; "
                    f"it never runs")
                continue
            if not step["has_run"]:
                problems.append(
                    f"not-a-run-step: {job} step {step['name']!r} gates on "
                    f"{shard!r} but has no run: block")
                continue
            gated.setdefault(shard, []).append(step["name"])
        for shard in expected:
            if shard in matrix and shard not in gated:
                problems.append(
                    f"shard-runs-nothing: {job} matrix shard {shard!r} gates "
                    f"no proof step; its leg proves nothing")
    return problems

def _self_test():
    """Regression checks for the matrix parser, run on every invocation."""
    def job(name, shards, steps):
        text = f"  {name}:\n    strategy:\n      fail-fast: false\n      matrix:\n        shard:\n"
        for shard in shards:
            text += f"          - {shard}\n"
        text += "    steps:\n"
        for step_name, shard in steps:
            text += (f"      - name: {step_name}\n"
                     f"        if: matrix.shard == '{shard}'\n"
                     "        run: |\n          echo ok\n")
        return text

    good = ("name: test\njobs:\n"
            + job("native-tests",
                  ["athletic-traversal-1", "athletic-traversal-2",
                   "matron-slices-1", "matron-slices-2", "matron-slices-3"],
                  [("Run athletic-traversal-1", "athletic-traversal-1"),
                   ("Run athletic-traversal-2", "athletic-traversal-2"),
                   ("Run CTest Matron slice group 1", "matron-slices-1"),
                   ("Run CTest Matron slice group 2", "matron-slices-2"),
                   ("Run CTest Matron slice group 3", "matron-slices-3")])
            + job("godot-touch", ["touch-1", "touch-2"],
                  [("Drive control paths (shard 1)", "touch-1"),
                   ("Drive control paths (shard 2)", "touch-2")]))
    cases = []

    def case(name, text, want_substring):
        problems = check_text(text)
        joined = "\n".join(problems)
        if want_substring is None:
            if problems:
                raise AssertionError(f"{name}: expected clean, got {problems}")
        elif want_substring not in joined:
            raise AssertionError(f"{name}: expected {want_substring!r} in {problems}")
        cases.append(name)

    case("valid", good, None)
    case("dropped-shard",
         good.replace("          - matron-slices-3\n", "", 1),
         "missing-matrix-shard")
    case("extra-shard",
         good.replace("          - touch-2\n",
                      "          - touch-2\n          - touch-3\n", 1),
         "unexpected-matrix-shard")
    case("duplicate-shard",
         good.replace("          - touch-2\n",
                      "          - touch-2\n          - touch-2\n", 1),
         "duplicate-matrix")
    case("phantom-gate",
         good.replace("matrix.shard == 'matron-slices-3'",
                      "matrix.shard == 'matron-slices-9'", 1),
         "phantom-gate")
    case("weird-gate",
         good.replace("matrix.shard == 'matron-slices-3'",
                      "matrix.shard != 'matron-slices-3'", 1),
         "unrecognized-matrix-gate")
    case("shard-runs-nothing",
         good.replace("        if: matrix.shard == 'touch-1'\n", "", 1),
         "shard-runs-nothing")
    case("not-a-run-step",
         good.replace("        if: matrix.shard == 'touch-1'\n        run: |\n          echo ok\n",
                      "        if: matrix.shard == 'touch-1'\n        uses: actions/upload-artifact@v4\n", 1),
         "not-a-run-step")
    case("missing-job", good.replace("  native-tests:\n", "  native-tests-x:\n", 1),
         "missing: no native-tests job")
    return len(cases)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workflow",
                        default=".github/workflows/wo000-delivery-spine.yml")
    parser.add_argument("--self-test", action="store_true",
                        help="run parser regressions only")
    args = parser.parse_args()

    fixture_count = _self_test()
    if args.self_test:
        print(f"PASS: {fixture_count} matrix coverage parser regression checks")
        return 0

    workflow_text = Path(args.workflow).read_text()
    problems = check_text(workflow_text)
    if problems:
        for problem in problems:
            print(f"FAIL: {problem}")
        return 1
    total = sum(len(v) for v in EXPECTED.values())
    print(f"PASS: {total} matrix shards across {len(EXPECTED)} jobs, "
          f"each gating at least one run: proof step")
    return 0


if __name__ == "__main__":
    sys.exit(main())
