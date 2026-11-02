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


def list_tests(test_dir):
    """Return the test names `ctest -N` reports for a build dir."""
    result = subprocess.run(
        ["ctest", "-N", "--test-dir", str(test_dir)],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit(f"check_ctest_shard_coverage: ctest -N failed in {test_dir}")
    tests = [m.group(1) for line in result.stdout.splitlines() for m in [TEST_LINE.match(line)] if m]
    if not tests:
        raise SystemExit(f"check_ctest_shard_coverage: no tests listed in {test_dir}")
    return tests


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workflow", default=root / ".github/workflows/wo000-delivery-spine.yml")
    parser.add_argument("--test-dir", default=root / "build/host")
    args = parser.parse_args()

    patterns = shard_patterns(Path(args.workflow).read_text())
    tests = list_tests(args.test_dir)

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
    print(f"PASS: {len(tests)} tests, each selected by exactly one shard")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
