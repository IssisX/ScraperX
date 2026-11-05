#!/usr/bin/env python3
"""Check that every proof guard actually runs in the host-build job.

The host-build job wires one step per proof guard: CTest shard
coverage, touch shard coverage, godot test coverage, and drawn-body
coverage (plus any guard added later, e.g. the audits-step guard).
Each guard reads the workflow as written and checks its own job, but
no guard reads the host-build steps themselves: delete a guard's
step from host-build and every remaining guard still passes, while
the deleted gate never runs at all. That is the same silent-drop
hole the guards closed for their own jobs, one layer up -- proven
2026-11-04 by deleting the touch-guard step from a workflow copy
and watching all four trunk guards pass on it.

This script is the guard of the guards. It reads the guard list out
of the repo as written, not a copy of it: every
`.github/scripts/check_*.py` file must be invoked exactly once in
the host-build job, by a `run:` step with no step-level `if:`.
Job-level gating (the host-build job's own `if:` on the scope
outputs) is the only gate allowed, exactly as for the other guards.

Rules, all read from the repo as written:
  - the host-build job must exist;
  - every `.github/scripts/check_*.py` script must be invoked
    exactly once inside that job (FAIL if missing or duplicated);
  - the invoking step must be a `run:` step (FAIL if it is a
    `uses:` step or has no run block);
  - the invoking step must have no step-level `if:` line (a
    conditionally skipped guard is a silently dropped guard).

The required set is derived from the scripts directory, so a new
guard lands together with its step and keeps passing, and deleting
a guard's step without deleting its script fails, whichever order
the guards land in. A script deleted together with its step passes
by design -- that is a deliberate retirement, visible as a file
deletion in the diff, not a silent drop -- and the scope job maps
`.github/` to every group, so the change still runs the full proof.

Usage:
    python3 .github/scripts/check_guard_step_coverage.py \
        --workflow .github/workflows/wo000-delivery-spine.yml \
        --scripts-dir .github/scripts
"""

import argparse
import re
import sys
from pathlib import Path

JOB_HEADER = re.compile(r"^  ([A-Za-z0-9_-]+):\s*$")
STEP_HEADER = re.compile(r"^      - name: ")
FIELD_RUN = re.compile(r"^        run:", re.MULTILINE)
FIELD_IF = re.compile(r"^        if:", re.MULTILINE)


def host_build_block(workflow_text):
    """Return the host-build job's lines, or None if the job is missing."""
    lines = workflow_text.splitlines()
    start = None
    for i, line in enumerate(lines):
        if line == "  host-build:":
            start = i
            break
    if start is None:
        return None
    block = []
    for line in lines[start + 1:]:
        if JOB_HEADER.match(line):
            break
        block.append(line)
    return block


def step_blocks(job_block):
    """Split a job block into (step name, lines) pairs."""
    steps = []
    current = None
    for line in job_block:
        match = STEP_HEADER.match(line)
        if match:
            current = (line[len("      - name: "):].strip(), [])
            steps.append(current)
        elif current is not None:
            current[1].append(line)
    return steps


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--workflow", default=".github/workflows/wo000-delivery-spine.yml")
    parser.add_argument("--scripts-dir", default=".github/scripts")
    args = parser.parse_args()

    workflow_text = Path(args.workflow).read_text()
    job_block = host_build_block(workflow_text)
    if job_block is None:
        print("FAIL: no host-build job in workflow")
        return 1
    job_text = "\n".join(job_block)
    steps = step_blocks(job_block)

    scripts = sorted(p.name for p in Path(args.scripts_dir).glob("check_*.py"))
    if not scripts:
        print("FAIL: no check_*.py scripts in scripts dir")
        return 1

    problems = []
    for script in scripts:
        invocation = f".github/scripts/{script}"
        count = job_text.count(invocation)
        script_problems = []
        if count == 0:
            script_problems.append(f"missing: {script} is never invoked in host-build")
        elif count != 1:
            script_problems.append(f"duplicated: {script} invoked {count} times in host-build")
        else:
            invoking = [s for s in steps if invocation in "\n".join(s[1])]
            if not invoking:
                script_problems.append(f"not-a-step: {script} mentioned in host-build but in no step")
            else:
                name, block_lines = invoking[0]
                block_text = "\n".join(block_lines)
                if not FIELD_RUN.search(block_text):
                    script_problems.append(f"not-a-run-step: {script} invoked by step {name!r} with no run: block")
                if FIELD_IF.search(block_text):
                    script_problems.append(f"conditional: {script} invoked by step {name!r} with a step-level if:")
        if script_problems:
            problems.extend(script_problems)
        else:
            print(f"OK: {script} invoked exactly once in host-build")

    if problems:
        for problem in problems:
            print(f"FAIL: {problem}")
        return 1
    print(f"PASS: {len(scripts)} guard scripts each invoked exactly once in host-build")
    return 0


if __name__ == "__main__":
    sys.exit(main())
