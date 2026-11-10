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
    python3 .github/scripts/check_drawn_body_coverage.py --self-test
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


def check_drawn_body(workflow_text, expected):
    """Return (failures, scenarios) for a workflow text and EXPECTED set.

    Pure function: no file reads, so the self-test can drive it with
    fixture workflow text and a fixture EXPECTED set.
    """
    block = drawn_body_block(workflow_text)
    if block is None:
        return ["no 'Her drawn body' step in the workflow"], []
    failures = []
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
    for name in sorted(expected - set(scenarios)):
        failures.append(
            f"EXPECTED scenario never recorded: {name} "
            "(in check.py EXPECTED, not in the drawn-body step's list)"
        )
    return failures, scenarios


def _self_test():
    """Regression checks for the drawn-body parser, run on every invocation."""
    def fixture(scenarios="alpha beta", extra_step=""):
        return (
            "jobs:\n"
            "  godot-touch:\n"
            "    steps:\n"
            "      - name: Her drawn body (bones, grips, plants, hair and skirt)\n"
            "        shell: bash\n"
            "        run: |\n"
            "          set -euo pipefail\n"
            f"          for scenario in {scenarios}; do\n"
            "            echo \"recording ${scenario}\"\n"
            "          done\n"
            "          python3 tools/matron_probe/check.py \"${runs[@]}\" --stride 10\n"
            "          grep -E \"^MATRON_DRAWN PASS\" artifacts/godot-uitest-matron-drawn.log\n"
            "\n"
            "      - name: Upload shard touch logs\n"
            "        uses: actions/upload-artifact@v4\n"
            f"{extra_step}"
        )

    good = fixture()
    expected = {"alpha", "beta"}
    cases = []

    def case(name, text, expected_set, want_substring):
        failures, _scenarios = check_drawn_body(text, expected_set)
        joined = "\n".join(failures)
        if want_substring is None:
            if failures:
                raise AssertionError(f"{name}: expected clean, got {failures}")
        elif want_substring not in joined:
            raise AssertionError(f"{name}: expected {want_substring!r} in {failures}")
        cases.append(name)

    case("valid", good, expected, None)
    case("missing-step",
         good.replace("Her drawn body", "Her drawn arm", 1),
         expected, "no 'Her drawn body' step in the workflow")
    case("no-check-script",
         good.replace(
             "          python3 tools/matron_probe/check.py \"${runs[@]}\" --stride 10\n",
             "", 1),
         expected, "does not run tools/matron_probe/check.py")
    case("no-pass-marker",
         good.replace(
             "          grep -E \"^MATRON_DRAWN PASS\" artifacts/godot-uitest-matron-drawn.log\n",
             "", 1),
         expected, "does not grep for the MATRON_DRAWN PASS line")
    case("no-loop",
         good.replace("          for scenario in alpha beta; do\n", "", 1),
         expected, "has no `for scenario in` loop")
    case("duplicate-scenario",
         fixture("alpha alpha beta"), expected,
         "scenario listed twice in the drawn-body step: alpha")
    case("missing-expected",
         fixture("alpha"), expected,
         "EXPECTED scenario never recorded: beta")
    case("extra-scenario-ok",
         fixture("alpha beta gamma"), expected, None)
    case("later-step-loop-ignored",
         fixture(extra_step=(
             "      - name: Some other step\n"
             "        run: |\n"
             "          for scenario in gamma; do\n"
             "            echo hi\n"
             "          done\n"
         )),
         {"alpha", "gamma"},
         "EXPECTED scenario never recorded: gamma")
    case("empty-expected", good, set(), None)
    return len(cases)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workflow", default=".github/workflows/wo000-delivery-spine.yml")
    parser.add_argument("--check", default="tools/matron_probe/check.py")
    parser.add_argument("--self-test", action="store_true",
                        help="run parser regressions only")
    args = parser.parse_args()

    fixture_count = _self_test()
    if args.self_test:
        print(f"PASS: {fixture_count} drawn-body coverage parser regression checks")
        return 0

    workflow_text = Path(args.workflow).read_text()
    expected = expected_scenarios(args.check)
    failures, scenarios = check_drawn_body(workflow_text, expected)
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
