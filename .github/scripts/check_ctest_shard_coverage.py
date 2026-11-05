#!/usr/bin/env python3
"""Check that every CTest test runs in exactly one CI shard.

The native-tests job splits scraperx_sim tests across four shards:
athletic-traversal (an -R list), matron-slices-1 (an -R list),
matron-slices-2 (an -R list), and matron-slices-3 (an -E exclusion list:
every test not excluded runs there). A test selected by zero shards never
runs in CI; a test selected by two shards runs twice, doubling its CI
time and its failure surface. Either case is a proof gap: CI green would
no longer mean every registered test passed exactly once.

This script reads the shard patterns out of the workflow file itself,
so it checks the workflow as written, not a copy of its patterns, and
compares them against the test list `ctest -N` reports for a build dir.
It exits 0 only when every test is selected by exactly one shard.

When no usable build dir exists, the test list falls back to parsing
the root CMakeLists.txt: plain add_test(NAME ...) lines plus add_test
lines inside foreach blocks, expanded over the block's item list. The
fallback prints a SOURCE FALLBACK banner so its output never passes
itself off as a `ctest -N` run, and the build-dir path stays primary:
whenever `ctest -N` succeeds, its list is used and the fallback never
runs. A coverage hole still fails on either path.

Usage:
    python3 .github/scripts/check_ctest_shard_coverage.py \
        --workflow .github/workflows/wo000-delivery-spine.yml \
        --test-dir build/host
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

# Step names in wo000-delivery-spine.yml, in shard order. The athletic
# step and the slice-1/2 steps select with -R; the slice-3 step selects
# with -E (everything not excluded).
SHARDS = [
    ("athletic-traversal", "Run athletic_traversal", "-R"),
    ("matron-slices-1", "Run CTest Matron slice group 1", "-R"),
    ("matron-slices-2", "Run CTest Matron slice group 2", "-R"),
    ("matron-slices-3", "Run CTest Matron slice group 3", "-E"),
]

TEST_LINE = re.compile(r"^\s*Test\s+#\d+:\s+(\S+)\s*$")


def shard_patterns(workflow_text):
    """Return {shard: compiled pattern} read from the workflow steps."""
    patterns = {}
    for shard, step_name, flag in SHARDS:
        start = workflow_text.find(step_name)
        if start < 0:
            raise SystemExit(f"check_ctest_shard_coverage: no step named {step_name!r}")
        block = workflow_text[start:]
        next_step = block.find("\n      - name:", 1)
        if next_step >= 0:
            block = block[:next_step]
        match = re.search(rf"{re.escape(flag)}\s+\"([^\"]+)\"", block)
        if match is None:
            raise SystemExit(
                f"check_ctest_shard_coverage: no {flag} pattern in step {step_name!r}"
            )
        patterns[shard] = re.compile(match.group(1))
    return patterns


ADD_TEST = re.compile(r"add_test\s*\(\s*NAME\s+(\S+)")
FOREACH = re.compile(r"foreach\s*\(\s*(\w+)((?:\s+\S+)+)\s*\)")
ENDFOREACH = re.compile(r"^\s*endforeach\s*\(")


def list_tests_from_cmakelists(cmakelists):
    """Return test names parsed from the root CMakeLists.txt.

    Expands add_test lines inside foreach blocks over the block's item
    list, so a slice added to the loop is picked up without editing
    this script. A name containing an unexpanded variable is a parse
    failure, never a silently skipped test.
    """
    tests = []
    loop_var = None
    loop_items = []
    for line in Path(cmakelists).read_text().splitlines():
        if line.strip().startswith("#"):
            continue
        foreach = FOREACH.search(line)
        if foreach is not None:
            loop_var, loop_items = foreach.group(1), foreach.group(2).split()
            continue
        if ENDFOREACH.search(line):
            loop_var, loop_items = None, []
            continue
        match = ADD_TEST.search(line)
        if match is None:
            continue
        name = match.group(1)
        if "${" in name:
            var = re.search(r"\$\{(\w+)\}", name).group(1)
            if var != loop_var or not loop_items:
                raise SystemExit(
                    "check_ctest_shard_coverage: cannot expand "
                    f"test name {name!r} from {cmakelists}"
                )
            tests.extend(name.replace("${" + var + "}", item) for item in loop_items)
        else:
            tests.append(name)
    if not tests:
        raise SystemExit(f"check_ctest_shard_coverage: no tests parsed from {cmakelists}")
    return tests


def list_tests(test_dir, cmakelists):
    """Return test names, preferring `ctest -N` for a build dir.

    Returns (tests, source) where source is "ctest" or "cmake". The
    cmake source is used only when `ctest -N` fails, so a fresh
    checkout without a build still gets a real coverage verdict
    instead of a build error dressed up as a coverage failure.
    """
    result = subprocess.run(
        ["ctest", "-N", "--test-dir", str(test_dir)],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode == 0:
        tests = [m.group(1) for line in result.stdout.splitlines() for m in [TEST_LINE.match(line)] if m]
        if tests:
            return tests, "ctest"
        sys.stderr.write("check_ctest_shard_coverage: ctest -N listed no tests\n")
    else:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
    if not Path(cmakelists).is_file():
        raise SystemExit(f"check_ctest_shard_coverage: ctest -N failed in {test_dir}")
    tests = list_tests_from_cmakelists(cmakelists)
    print(f"SOURCE FALLBACK: ctest -N unusable in {test_dir}; test list parsed from {cmakelists}")
    return tests, "cmake"


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workflow", default=root / ".github/workflows/wo000-delivery-spine.yml")
    parser.add_argument("--test-dir", default=root / "build/host")
    args = parser.parse_args()

    patterns = shard_patterns(Path(args.workflow).read_text())
    tests, source = list_tests(args.test_dir, root / "CMakeLists.txt")

    selected = {}
    for test in tests:
        shards = []
        for shard, _step, flag in SHARDS:
            matched = patterns[shard].match(test) is not None
            if (flag == "-R" and matched) or (flag == "-E" and not matched):
                shards.append(shard)
        selected[test] = shards

    for shard, _step, _flag in SHARDS:
        names = [t for t, s in selected.items() if shard in s]
        print(f"{shard}: {len(names)} tests")

    bad = {t: s for t, s in selected.items() if len(s) != 1}
    if bad:
        print("FAIL: tests not selected by exactly one shard:")
        for test, shards in sorted(bad.items()):
            print(f"  {test}: {len(shards)} shards ({', '.join(shards) or 'none'})")
        return 1
    print(f"PASS: {len(tests)} tests ({source} list), each selected by exactly one shard")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
