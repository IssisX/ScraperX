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
the guards land in. A script deleted together with its step would
otherwise pass by the same logic -- the set would just shrink --
so the guards that must exist are also pinned in EXPECTED_GUARDS
below: a pinned guard missing from the directory fails, and a
guard retires only by removing its name from that set in the same
diff, which is a deliberate, visible edit, not a silent drop.
A new guard is NOT required to be in the pin to pass (it is still
covered by the exactly-once rule), so a guard landing on another
branch keeps passing while the pin catches up; add its name to
the pin when it lands.

This script's own logic has the same silent-drop shape one layer
further up: if its step parser or exactly-once counting regresses,
no other guard reads host-build, so a broken guard-of-guards
passes the real workflow while a guard step goes unverified --
proven 2026-11-05 by neutering its step-level `if:` detector and
watching it PASS a workflow with a conditional guard step. So its
parser regressions live in `_self_test()` below and run on every
invocation, `--self-test` or not, the same pattern as the matrix
guard: a logic regression fails its own run before it can pass a
real workflow.

Usage:
    python3 .github/scripts/check_guard_step_coverage.py \
        --workflow .github/workflows/wo000-delivery-spine.yml \
        --scripts-dir .github/scripts

    python3 .github/scripts/check_guard_step_coverage.py --self-test
"""

import argparse
import re
import sys
from pathlib import Path

JOB_HEADER = re.compile(r"^  ([A-Za-z0-9_-]+):\s*$")
STEP_HEADER = re.compile(r"^      - name: ")
FIELD_RUN = re.compile(r"^        run:", re.MULTILINE)
FIELD_IF = re.compile(r"^        if:", re.MULTILINE)

# Guards that must exist. A pinned guard missing from the scripts
# directory fails this check: retiring a guard means removing its
# name here in the same diff, a deliberate edit, not a silent drop.
# A guard not yet in this set still passes (it is covered by the
# exactly-once rule below), so a guard landing on another branch
# keeps passing while the pin catches up; add its name here when
# it lands.
EXPECTED_GUARDS = frozenset({
    "check_apk_step_coverage.py",
    "check_audit_step_coverage.py",
    "check_ctest_shard_coverage.py",
    "check_drawn_body_coverage.py",
    "check_godot_test_coverage.py",
    "check_group_content_coverage.py",
    "check_guard_step_coverage.py",
    "check_job_gate_coverage.py",
    "check_matrix_shard_coverage.py",
    "check_scope_step_coverage.py",
    "check_touch_shard_coverage.py",
})


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


def check_text(workflow_text, scripts, expected=EXPECTED_GUARDS):
    """Check workflow text against a scripts list.

    Returns (problems, oks): problems is the list of failure strings
    (empty when the workflow is clean), oks is one "OK: ..." line per
    script that passed. The caller prints them; this function does
    no I/O, so the self-test can run it on fixture text.
    """
    job_block = host_build_block(workflow_text)
    if job_block is None:
        return ["no host-build job in workflow"], []
    if not scripts:
        return ["no check_*.py scripts in scripts dir"], []
    job_text = "\n".join(job_block)
    steps = step_blocks(job_block)

    problems = []
    oks = []
    for missing in sorted(expected - set(scripts)):
        problems.append(
            f"retired: {missing} is pinned in EXPECTED_GUARDS but missing "
            "from the scripts dir -- retire it by removing its name from "
            "EXPECTED_GUARDS in the same diff"
        )
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
            oks.append(f"OK: {script} invoked exactly once in host-build")
    return problems, oks


def _self_test():
    """Regression checks for the guard-of-guards parser, run on every invocation."""
    alpha = "check_alpha_coverage.py"
    beta = "check_beta_coverage.py"
    new = "check_new_coverage.py"
    pinned = frozenset({alpha, beta})

    def step(name, script):
        return (f"      - name: {name}\n"
                "        shell: bash\n"
                "        run: |\n"
                f"          python3 .github/scripts/{script}\n")

    valid = ("name: test\njobs:\n  host-build:\n    steps:\n"
             + step("Verify alpha", alpha)
             + step("Verify beta", beta))
    with_new = valid + step("Verify new guard", new)
    cases = []

    def case(name, text, scripts, want, expected=pinned):
        problems, _oks = check_text(text, scripts, expected)
        joined = "\n".join(problems)
        if want is None:
            if problems:
                raise AssertionError(f"{name}: expected clean, got {problems}")
        elif want not in joined:
            raise AssertionError(f"{name}: expected {want!r} in {problems}")
        cases.append(name)

    case("valid", valid, [alpha, beta], None)
    case("unpinned-new-guard", with_new, [alpha, beta, new], None)
    case("dropped-step",
         valid.replace(step("Verify beta", beta), "", 1),
         [alpha, beta],
         f"missing: {beta} is never invoked in host-build")
    case("duplicated-step",
         valid + step("Verify alpha again", alpha),
         [alpha, beta],
         f"duplicated: {alpha} invoked 2 times in host-build")
    case("conditional-step",
         valid.replace("      - name: Verify alpha\n",
                       "      - name: Verify alpha\n        if: github.event_name == 'push'\n", 1),
         [alpha, beta],
         f"conditional: {alpha} invoked by step 'Verify alpha' with a step-level if:")
    case("uses-step",
         valid.replace("        run: |\n          python3 .github/scripts/" + alpha + "\n",
                       "        env:\n          GUARD: .github/scripts/" + alpha + "\n"
                       "        uses: actions/checkout@v4\n", 1),
         [alpha, beta],
         f"not-a-run-step: {alpha} invoked by step 'Verify alpha' with no run: block")
    case("mentioned-not-a-step",
         valid.replace(step("Verify alpha", alpha),
                       "      - name: Verify .github/scripts/" + alpha + "\n        shell: bash\n", 1),
         [alpha, beta],
         f"not-a-step: {alpha} mentioned in host-build but in no step")
    case("retired-pinned-guard",
         valid, [alpha],
         f"retired: {beta} is pinned in EXPECTED_GUARDS but missing from the scripts dir")
    case("legitimate-retirement",
         valid.replace(step("Verify beta", beta), "", 1),
         [alpha], None, frozenset({alpha}))
    case("missing-job",
         "name: test\njobs:\n  other-job:\n    steps:\n",
         [alpha, beta],
         "no host-build job in workflow")
    case("empty-scripts-dir", valid, [], "no check_*.py scripts in scripts dir")
    return len(cases)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--workflow", default=".github/workflows/wo000-delivery-spine.yml")
    parser.add_argument("--scripts-dir", default=".github/scripts")
    parser.add_argument("--self-test", action="store_true",
                        help="run parser regressions only")
    args = parser.parse_args()

    fixture_count = _self_test()
    if args.self_test:
        print(f"PASS: {fixture_count} guard-step coverage parser regression checks")
        return 0

    workflow_text = Path(args.workflow).read_text()
    scripts = sorted(p.name for p in Path(args.scripts_dir).glob("check_*.py"))
    problems, oks = check_text(workflow_text, scripts)
    for ok in oks:
        print(ok)
    if problems:
        for problem in problems:
            print(f"FAIL: {problem}")
        return 1
    print(f"PASS: {len(scripts)} guard scripts each invoked exactly once in host-build")
    return 0


if __name__ == "__main__":
    sys.exit(main())
