#!/usr/bin/env python3
"""Check that the drawn-body CI gate cannot silently lose a scenario.

The godot-touch touch-2 job's "Her drawn body" step records probe
frames for a hand-written `for scenario in ...` list and then runs
tools/matron_probe/check.py over those recordings. check.py is strict
both ways for the scenarios it is given, but it never sees a scenario
that is not recorded: drop a scenario from the workflow loop and its
EXPECTED entries go unexercised while CI stays green -- the same
silent-drop hole the CTest, touch-shard and godot guards closed for
their jobs. This script is their counterpart for the drawn-body gate:
it reads the scenario list out of the workflow file itself, so it
checks the workflow as written, not a copy of its list, and compares
it against the EXPECTED keys in tools/matron_probe/check.py.

Rules, all read from the repo as written:
  - the "Her drawn body" step must exist, must run check.py, and must
    grep for the MATRON_DRAWN PASS line, or the step is not a gate
    at all and the guard fails;
  - the step's scenario list must not contain a duplicate;
  - every scenario named in check.py's EXPECTED must be in the step's
    list, or its expected failures are never measured (FAIL);
  - a scenario in the list with no EXPECTED entry is fine: a clean
    scenario needs no entry, and check.py fails it on any unexpected
    failure of its own.

A scenario removed from the list together with its EXPECTED entries
passes this guard by design -- that is a deliberate retirement, not a
silent drop -- and check.py's own XPASS rule covers the other way
(an EXPECTED entry whose defect is fixed fails until removed).

Usage:
    python3 .github/scripts/check_drawn_body_coverage.py \
        --workflow .github/workflows/wo000-delivery-spine.yml \
        --check tools/matron_probe/check.py
"""

import argparse
import ast
import re
import sys
from pathlib import Path

STEP_MARKER = "Her drawn body"
CHECK_SCRIPT = "tools/matron_probe/check.py"
PASS_MARKER = "MATRON_DRAWN PASS"
LOOP_RE = re.compile(r"for scenario in ([^;]+);")
STEP_RE = re.compile(r"^(\s*)- name: ")


def drawn_body_block(workflow_text):
    """Return the run-block lines of the drawn-body step, or None."""
    lines = workflow_text.splitlines()
    start = None
    for i, line in enumerate(lines):
        if STEP_MARKER in line and "- name:" in line:
            start = i
            break
    if start is None:
        return None
    block = []
    for line in lines[start + 1:]:
        if STEP_RE.match(line):
            break
        block.append(line)
    return block


def loop_scenarios(block):
    """Return the scenario list from the step's `for scenario in  loop."""
    for line in block:
        match = LOOP_RE.search(line)
        if match:
            return match.group(1).split()
    return None


def expected_scenarios(check_path):
    """Return the scenario set in check.py's EXPECTED keys.

    Only the dict keys are read (each key is a (check, scenario)
    literal pair); the values name exit-board items and are not
    evaluated, so this works without importing check.py or numpy.
    """
    tree = ast.parse(Path(check_path).read_text())
    for node in ast.walk(tree):
        if not isinstance(node, ast.Assign):
            continue
        if not any(
            isinstance(t, ast.Name) and t.id == "EXPECTED" for t in node.targets
        ):
            continue
        if not isinstance(node.value, ast.Dict):
            raise SystemExit("check_drawn_body_coverage: EXPECTED is not a dict")
        scenarios = set()
        for key in node.value.keys:
            if not isinstance(key, ast.Tuple) or len(key.elts) != 2:
                raise SystemExit(
                    "check_drawn_body_coverage: EXPECTED key is not a pair"
                )
            scenario = key.elts[1]
            if not isinstance(scenario, ast.Constant):
                raise SystemExit(
                    "check_drawn_body_coverage: EXPECTED scenario is not a literal"
                )
            scenarios.add(scenario.value)
        return scenarios
    raise SystemExit("check_drawn_body_coverage: no EXPECTED in check.py")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--workflow", default=".github/workflows/wo000-delivery-spine.yml")
    parser.add_argument("--check", default="tools/matron_probe/check.py")
    args = parser.parse_args()

    failures = []
    block = drawn_body_block(Path(args.workflow).read_text())
    if block is None:
        print("FAIL: drawn-body coverage gaps:")
        print("  no 'Her drawn body' step in the workflow")
        return 1
    block_text = "\n".join(block)
    if CHECK_SCRIPT not in block_text:
        failures.append("drawn-body step does not run tools/matron_probe/check.py")
    if PASS_MARKER not in block_text:
        failures.append("drawn-body step does not grep for the MATRON_DRAWN PASS line")
    scenarios = loop_scenarios(block)
    if not scenarios:
        failures.append("drawn-body step has no `for scenario in` loop")
        scenarios = []
    duplicates = sorted({s for s in scenarios if scenarios.count(s) > 1})
    for name in duplicates:
        failures.append(f"scenario listed twice in the drawn-body step: {name}")
    expected = expected_scenarios(args.check)
    for name in sorted(expected - set(scenarios)):
        failures.append(
            f"EXPECTED scenario never recorded: {name} "
            "(in check.py EXPECTED, not in the drawn-body step's list)"
        )

    if failures:
        print("FAIL: drawn-body coverage gaps:")
        for failure in failures:
            print(f"  {failure}")
        return 1
    print(
        f"PASS: {len(scenarios)} drawn-body scenarios recorded, "
        f"{len(expected)} EXPECTED scenario(s) all exercised"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
