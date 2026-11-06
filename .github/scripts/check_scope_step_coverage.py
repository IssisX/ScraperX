#!/usr/bin/env python3
"""Check that every scope proof step runs in the scope job.

The scope job is the proof's front door: it runs the rule check, the
proof-scope unit tests, and the proof-scope decision itself, and its
outputs gate every later job. Each of those three steps is a silent
drop if deleted:

  - without the rule check, a protected change (a removed or loosened
    test check, a budget number changed) sails through a green proof;
  - without the scope unit tests, a mapping bug in proof_scope.py
    ships without any test ever running on it in CI;
  - without the proof-scope decision, the scope outputs are unset,
    every downstream job's scope gate reads false, every proof job is
    skipped, and skipped counts as passing for the switchboard --
    a green run with zero proof.

No other guard reads the scope job: the shard guards read their own
jobs, the audits guard reads the audits job, and the guard-of-guards
reads the host-build steps. So a single step dropped from the scope
job in a bad merge goes green everywhere else. This script is the
guard for that job. It reads the workflow as written, not a copy of
it, and requires, in the scope job:

  - the job exists and has no job-level `if:` (a gated scope job
    skips the whole proof the same way a missing decision does);
  - exactly one `run:` step with no step-level `if:` invoking
    tools/rule_check/rule_check.py;
  - exactly one `run:` step with no step-level `if:` invoking the
    proof-scope unit tests (test_proof_scope);
  - exactly one `run:` step with no step-level `if:` invoking
    ci/proof_scope.py.

The guard is wired in the scope job itself, so it runs on every
push, including pushes where the proof-scope decision would skip
the later jobs: deleting the decision step cannot also skip its
own guard. It is wired in host-build too, exactly once, which
backstops the scope-job wiring on game pushes (a dropped scope
wiring step is still caught there) and keeps the guard-of-guards
passing, since that guard requires every check script to be invoked
exactly once in host-build. A script deleted together with both of
its steps passes by design -- that is a deliberate retirement,
visible as a file deletion in the diff, not a silent drop.

Parser regressions live in `_self_test()` below and run on every
invocation, `--self-test` or not, the same pattern as the other
coverage guards: a regression in the step parsing must fail the
self-test, not pass a broken workflow.

Usage:
    python3 .github/scripts/check_scope_step_coverage.py \
        --workflow .github/workflows/wo000-delivery-spine.yml
    python3 .github/scripts/check_scope_step_coverage.py --self-test
"""

import argparse
import re
import sys
from pathlib import Path

JOB_HEADER = re.compile(r"^  ([A-Za-z0-9_-]+):\s*$")
STEP_HEADER = re.compile(r"^      - name: ")
FIELD_RUN = re.compile(r"^        run:", re.MULTILINE)
FIELD_IF = re.compile(r"^        if:", re.MULTILINE)
JOB_IF = re.compile(r"^    if:")
STEPS_HEADER = re.compile(r"^    steps:\s*$")

# Marker -> what it is, for failure messages. Each marker must appear
# in exactly one scope-job step.
MARKERS = [
    ("tools/rule_check/rule_check.py", "rule check"),
    ("test_proof_scope", "proof-scope unit tests"),
    ("ci/proof_scope.py", "proof-scope decision"),
]


def scope_job_block(workflow_text):
    """Return the scope job's lines, or None if the job is missing."""
    lines = workflow_text.splitlines()
    start = None
    for i, line in enumerate(lines):
        if line == "  scope:":
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


def check_scope_steps(workflow_text):
    """Check workflow text for scope-step problems.

    Returns the list of failure strings (empty when the workflow
    is clean). The caller prints them; this function does no I/O,
    so the self-test can run it on fixture text.
    """
    job_block = scope_job_block(workflow_text)
    if job_block is None:
        return ["no scope job in workflow"]

    problems = []

    # A job-level if: on scope gates the whole proof: with the job
    # skipped, every downstream scope gate reads empty and skips too.
    for line in job_block:
        if STEPS_HEADER.match(line):
            break
        if JOB_IF.match(line):
            problems.append("job-level if: scope job is conditionally skipped")

    steps = step_blocks(job_block)
    if not steps:
        problems.append("no steps in scope job")
    for marker, what in MARKERS:
        invoking = [s for s in steps if marker in "\n".join(s[1])]
        if not invoking:
            problems.append(f"missing: no scope-job step invokes {what} ({marker})")
        elif len(invoking) > 1:
            problems.append(
                f"duplicated: {what} ({marker}) invoked in {len(invoking)} scope-job steps")
        else:
            name, lines = invoking[0]
            text = "\n".join(lines)
            if not FIELD_RUN.search(text):
                problems.append(
                    f"not-a-run-step: {what} ({marker}) is in step {name!r}, which has no run: block")
            if FIELD_IF.search(text):
                problems.append(
                    f"conditional: {what} ({marker}) is in step {name!r}, which has a step-level if:")

    return problems


def _self_test():
    """Regression checks for the scope-step parser, run on every invocation."""
    def scope(job_if=None, steps_text=None):
        if steps_text is None:
            steps_text = (
                "      - name: Check rule\n"
                "        run: |\n"
                "          python3 tools/rule_check/rule_check.py \"$TRUNK\" HEAD\n"
                "      - name: Decide\n"
                "        run: |\n"
                "          (cd ci && python3 -m unittest -q test_proof_scope)\n"
                "          python3 ci/proof_scope.py\n")
        text = "  scope:\n    runs-on: ubuntu-24.04\n"
        if job_if is not None:
            text += f"    if: {job_if}\n"
        return text + "    steps:\n" + steps_text

    valid = "name: test\njobs:\n" + scope()
    cases = []

    def case(name, text, want):
        problems = check_scope_steps(text)
        joined = "\n".join(problems)
        if want is None:
            if problems:
                raise AssertionError(f"{name}: expected clean, got {problems}")
        elif want not in joined:
            raise AssertionError(f"{name}: expected {want!r} in {problems}")
        cases.append(name)

    case("valid", valid, None)
    case("no-scope-job", "name: test\njobs:\n",
         "no scope job in workflow")
    case("job-level-if",
         "name: test\njobs:\n" + scope(job_if="github.ref == 'refs/heads/ScraperX-Claude'"),
         "job-level if: scope job is conditionally skipped")
    case("missing-rule-check",
         "name: test\njobs:\n" + scope(steps_text=(
             "      - name: Decide\n"
             "        run: |\n"
             "          (cd ci && python3 -m unittest -q test_proof_scope)\n"
             "          python3 ci/proof_scope.py\n")),
         "missing: no scope-job step invokes rule check (tools/rule_check/rule_check.py)")
    case("missing-unit-tests",
         "name: test\njobs:\n" + scope(steps_text=(
             "      - name: Check rule\n"
             "        run: |\n"
             "          python3 tools/rule_check/rule_check.py \"$TRUNK\" HEAD\n"
             "      - name: Decide\n"
             "        run: |\n"
             "          python3 ci/proof_scope.py\n")),
         "missing: no scope-job step invokes proof-scope unit tests (test_proof_scope)")
    case("missing-decision",
         "name: test\njobs:\n" + scope(steps_text=(
             "      - name: Check rule\n"
             "        run: |\n"
             "          python3 tools/rule_check/rule_check.py \"$TRUNK\" HEAD\n"
             "      - name: Decide\n"
             "        run: |\n"
             "          (cd ci && python3 -m unittest -q test_proof_scope)\n")),
         "missing: no scope-job step invokes proof-scope decision (ci/proof_scope.py)")
    case("duplicated-decision",
         "name: test\njobs:\n" + scope(steps_text=(
             "      - name: Decide\n"
             "        run: |\n"
             "          (cd ci && python3 -m unittest -q test_proof_scope)\n"
             "          python3 ci/proof_scope.py\n"
             "      - name: Decide again\n"
             "        run: |\n"
             "          python3 ci/proof_scope.py\n")),
         "duplicated: proof-scope decision (ci/proof_scope.py) invoked in 2 scope-job steps")
    case("conditional-decision",
         "name: test\njobs:\n" + scope(steps_text=(
             "      - name: Check rule\n"
             "        run: |\n"
             "          python3 tools/rule_check/rule_check.py \"$TRUNK\" HEAD\n"
             "      - name: Decide\n"
             "        if: github.event_name == 'push'\n"
             "        run: |\n"
             "          (cd ci && python3 -m unittest -q test_proof_scope)\n"
             "          python3 ci/proof_scope.py\n")),
         "conditional: proof-scope decision (ci/proof_scope.py) is in step 'Decide', which has a step-level if:")
    case("not-a-run-step",
         "name: test\njobs:\n" + scope(steps_text=(
             "      - name: Check\n"
             "        uses: some/action\n"
             "        with:\n"
             "          script: tools/rule_check/rule_check.py\n"
             "      - name: Decide\n"
             "        run: |\n"
             "          (cd ci && python3 -m unittest -q test_proof_scope)\n"
             "          python3 ci/proof_scope.py\n")),
         "not-a-run-step: rule check (tools/rule_check/rule_check.py) is in step 'Check', which has no run: block")
    case("no-steps",
         "name: test\njobs:\n  scope:\n    runs-on: ubuntu-24.04\n    steps:\n",
         "no steps in scope job")
    case("marker-elsewhere",
         "name: test\njobs:\n" + scope(steps_text=(
             "      - name: Check rule\n"
             "        run: |\n"
             "          python3 tools/rule_check/rule_check.py \"$TRUNK\" HEAD\n"
             "      - name: Decide\n"
             "        run: |\n"
             "          (cd ci && python3 -m unittest -q test_proof_scope)\n"))
         + "  host-build:\n"
           "    runs-on: ubuntu-24.04\n"
           "    steps:\n"
           "      - name: Decide elsewhere\n"
           "        run: |\n"
           "          python3 ci/proof_scope.py\n",
         "missing: no scope-job step invokes proof-scope decision (ci/proof_scope.py)")
    return len(cases)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--workflow",
                        default=".github/workflows/wo000-delivery-spine.yml")
    parser.add_argument("--self-test", action="store_true",
                        help="run parser regressions only")
    args = parser.parse_args()

    fixture_count = _self_test()
    if args.self_test:
        print(f"PASS: {fixture_count} scope-step coverage parser regression checks")
        return 0

    workflow_text = Path(args.workflow).read_text()
    problems = check_scope_steps(workflow_text)
    if problems:
        for problem in problems:
            print(f"FAIL: {problem}")
        return 1
    print("PASS: scope job runs the rule check, the proof-scope unit tests, "
          "and the proof-scope decision, each exactly once, unconditionally")
    return 0


if __name__ == "__main__":
    sys.exit(main())
