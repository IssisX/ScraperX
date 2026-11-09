#!/usr/bin/env python3
"""Check that every CTest test runs in exactly one CI shard.

The native-tests job splits scraperx_sim tests across five shards:
athletic-traversal-1 and athletic-traversal-2 (each an -R list; J-071),
matron-slices-1 (an -R list), matron-slices-2 (an -R list), and
matron-slices-3 (an -E exclusion list: every test not excluded runs there). A test selected by zero shards never
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

The shard-selection rule itself had no regression checks: a logic
regression in it passes the real workflow while a test goes
unselected or double-selected, and no other guard reads the -R/-E
selection. Proven 2026-11-05: on a fixture workflow whose slice-3
-E list excludes a test no -R list selects, the real guard fails
(exit 1, "0 shards"), while a copy with the zero-shard detection
removed passes it (exit 0, "PASS: 5 tests"). The parser regression
checks in _self_test() run on every invocation, so that class of
regression trips the guard itself before CI trusts it.

Usage:
    python3 .github/scripts/check_ctest_shard_coverage.py \
        --workflow .github/workflows/wo000-delivery-spine.yml \
        --test-dir build/host
"""

import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

# Step names in wo000-delivery-spine.yml, in shard order. The athletic
# step and the slice-1/2 steps select with -R; the slice-3 step selects
# with -E (everything not excluded).
SHARDS = [
    ("athletic-traversal-1", "Run athletic-traversal-1", "-R"),
    ("athletic-traversal-2", "Run athletic-traversal-2", "-R"),
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
# The CTest slices CMakeLists.txt registers from the test files (J-074): the
# list it loops over, and the same match it reads each name with.
REGISTRY_SLICES = ["IN", "LISTS", "SCRAPERX_SIM_SLICES"]
SLICE_RE = re.compile(r'SCRAPERX_SLICE\("([^"]+)"')


def registry_slices(test_dir):
    """Return the SCRAPERX_SLICE names in test_dir's sim_tests_*.cpp, as CMake reads them."""
    names = []
    for path in sorted(Path(test_dir).glob("sim_tests_*.cpp")):
        names.extend(SLICE_RE.findall(path.read_text(errors="ignore")))
    return sorted(names)


def list_tests_from_cmakelists(cmakelists):
    """Return test names parsed from the root CMakeLists.txt.

    Expands add_test lines inside foreach blocks over the block's item
    list, so a slice added to the loop is picked up without editing
    this script; the loop over SCRAPERX_SIM_SLICES expands over the
    SCRAPERX_SLICE names in tests/ beside it. A name containing an
    unexpanded variable is a parse failure, never a silently skipped test.
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
            if loop_items == REGISTRY_SLICES:
                loop_items = registry_slices(Path(cmakelists).parent / "tests")
                if not loop_items:
                    raise SystemExit(f"check_ctest_shard_coverage: no SCRAPERX_SLICE found beside {cmakelists}")
            elif loop_items[:2] == ["IN", "LISTS"]:
                loop_items = []  # any other list: an add_test in it cannot be expanded
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


def select_shards(patterns, tests):
    """Return {test: [shards selecting it]} for the given patterns."""
    selected = {}
    for test in tests:
        shards = []
        for shard, _step, flag in SHARDS:
            matched = patterns[shard].match(test) is not None
            if (flag == "-R" and matched) or (flag == "-E" and not matched):
                shards.append(shard)
        selected[test] = shards
    return selected


def check_tests(patterns, tests):
    """Return {test: [shards]} for tests not selected by exactly one shard."""
    selected = select_shards(patterns, tests)
    return {t: s for t, s in selected.items() if len(s) != 1}


def _self_test():
    """Regression checks for the shard parser and selector, run on every invocation."""
    athletic1 = r"^scraperx_sim\.(shared_survival|athletic-traversal-1|parkour_flow)$"
    athletic2 = r"^scraperx_sim\.athletic-traversal-2$"
    group1 = r"^scraperx_sim\.(matron-ladder|matron-hearing|matron-climb-support|matron-crash|matron-tower)$"
    group2 = r"^scraperx_sim\.(matron-chain|matron-towers|matron-board-walls|matron-ladders|matron-stand)$"
    exclude = (
        r"^scraperx_sim\.(shared_survival|athletic-traversal-1|athletic-traversal-2|parkour_flow|"
        r"matron-ladder|matron-hearing|matron-climb-support|matron-crash|matron-tower|"
        r"matron-chain|matron-towers|matron-board-walls|matron-ladders|matron-stand)$"
    )
    good = (
        "name: test\njobs:\n  native-tests:\n    steps:\n"
        "      - name: Run athletic-traversal-1, shared_survival, parkour_flow\n"
        "        run: |\n"
        f"          ctest -R \"{athletic1}\" -V\n"
        "      - name: Run athletic-traversal-2\n"
        "        run: |\n"
        f"          ctest -R \"{athletic2}\" -V\n"
        "      - name: Run CTest Matron slice group 1 (ladder, hearing)\n"
        "        run: |\n"
        f"          ctest -R \"{group1}\" -j 2\n"
        "      - name: Run CTest Matron slice group 2 (chain, towers)\n"
        "        run: |\n"
        f"          ctest -R \"{group2}\" -j 2\n"
        "      - name: Run CTest Matron slice group 3 (remaining, chase, grab)\n"
        "        run: |\n"
        f"          ctest -E \"{exclude}\" -j 2\n"
    )
    patterns = shard_patterns(good)
    tests = [
        "scraperx_sim.athletic-traversal-1",
        "scraperx_sim.athletic-traversal-2",
        "scraperx_sim.matron-ladder",
        "scraperx_sim.matron-chain",
        "scraperx_sim.matron-remaining",
        "scraperx_sim.matron-budget",
    ]
    cases = []

    def case(name, fn):
        fn()
        cases.append(name)

    def valid():
        if check_tests(patterns, tests):
            raise AssertionError(f"valid: expected clean, got {check_tests(patterns, tests)}")
        selected = select_shards(patterns, tests)
        if selected["scraperx_sim.matron-budget"] != ["matron-slices-3"]:
            raise AssertionError("valid: brand-new test must land in slice 3 only, "
                                 f"got {selected['scraperx_sim.matron-budget']}")

    def zero_shard():
        # Slice 3's -E list excludes matron-foo while no -R list
        # selects it: zero shards, the exact drift the hole proof
        # caught.
        text = good.replace(exclude, exclude[:-2] + "|matron-foo)$", 1)
        bad = check_tests(shard_patterns(text), ["scraperx_sim.matron-foo"])
        if bad != {"scraperx_sim.matron-foo": []}:
            raise AssertionError(f"zero-shard: expected matron-foo with no shards, got {bad}")

    def double_selected():
        # Group 2's -R retargeted at group 1's list: matron-ladder is
        # selected by both slice groups.
        text = good.replace(f'ctest -R "{group2}"', f'ctest -R "{group1}"', 1)
        bad = check_tests(shard_patterns(text), ["scraperx_sim.matron-ladder"])
        if bad.get("scraperx_sim.matron-ladder") != ["matron-slices-1", "matron-slices-2"]:
            raise AssertionError(f"double-selected: expected both slice groups, got {bad}")

    def excluded_not_double():
        # A test in a -R list AND in the -E list is selected by its -R
        # shard only, not also by slice 3.
        selected = select_shards(patterns, ["scraperx_sim.matron-ladder"])
        if selected["scraperx_sim.matron-ladder"] != ["matron-slices-1"]:
            raise AssertionError("excluded-not-double: expected slice 1 only, "
                                 f"got {selected['scraperx_sim.matron-ladder']}")

    def missing_step():
        text = good.replace(
            "      - name: Run CTest Matron slice group 2 (chain, towers)\n", "", 1)
        try:
            shard_patterns(text)
        except SystemExit as exc:
            if "Run CTest Matron slice group 2" not in str(exc):
                raise AssertionError(f"missing-step: wrong message {exc}")
            return
        raise AssertionError("missing-step: expected SystemExit")

    def missing_pattern():
        text = good.replace(
            f'          ctest -R "{group2}" -j 2\n',
            "          ctest -j 2\n", 1)
        try:
            shard_patterns(text)
        except SystemExit as exc:
            if "-R pattern" not in str(exc):
                raise AssertionError(f"missing-pattern: wrong message {exc}")
            return
        raise AssertionError("missing-pattern: expected SystemExit")

    def cmake_foreach():
        text = (
            "add_test(NAME scraperx_sim.shared_survival COMMAND scraperx_sim_tests)\n"
            "foreach(slice matron-ladder matron-chain matron-remaining)\n"
            "    add_test(NAME scraperx_sim.${slice} COMMAND scraperx_sim_tests)\n"
            "endforeach()\n"
        )
        with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as tmp:
            tmp.write(text)
            tmp_path = tmp.name
        got = list_tests_from_cmakelists(tmp_path)
        if got != ["scraperx_sim.shared_survival", "scraperx_sim.matron-ladder",
                   "scraperx_sim.matron-chain", "scraperx_sim.matron-remaining"]:
            raise AssertionError(f"cmake-foreach: got {got}")

    def cmake_registry_foreach():
        # The slices registered in the test files: the loop over
        # SCRAPERX_SIM_SLICES expands over their SCRAPERX_SLICE names.
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / "tests").mkdir()
            (Path(tmp) / "tests" / "sim_tests_a.cpp").write_text(
                'SCRAPERX_SLICE("matron-b", run_b);\nSCRAPERX_SELECT("not-a-slice", run_c);\n'
                'SCRAPERX_SLICE("matron-a", run_a);\n')
            (Path(tmp) / "CMakeLists.txt").write_text(
                "foreach(slice IN LISTS SCRAPERX_SIM_SLICES)\n"
                "    add_test(NAME scraperx_sim.${slice} COMMAND scraperx_sim_tests)\n"
                "endforeach()\n")
            got = list_tests_from_cmakelists(Path(tmp) / "CMakeLists.txt")
        if got != ["scraperx_sim.matron-a", "scraperx_sim.matron-b"]:
            raise AssertionError(f"cmake-registry-foreach: got {got}")

    def cmake_unexpanded_var():
        text = "add_test(NAME scraperx_sim.${other} COMMAND scraperx_sim_tests)\n"
        with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as tmp:
            tmp.write(text)
            tmp_path = tmp.name
        try:
            list_tests_from_cmakelists(tmp_path)
        except SystemExit as exc:
            if "cannot expand" not in str(exc):
                raise AssertionError(f"cmake-unexpanded-var: wrong message {exc}")
            return
        raise AssertionError("cmake-unexpanded-var: expected SystemExit")

    def cmake_comment_skipped():
        text = (
            "# add_test(NAME scraperx_sim.matron-ghost COMMAND scraperx_sim_tests)\n"
            "add_test(NAME scraperx_sim.matron-ladder COMMAND scraperx_sim_tests)\n"
        )
        with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as tmp:
            tmp.write(text)
            tmp_path = tmp.name
        if list_tests_from_cmakelists(tmp_path) != ["scraperx_sim.matron-ladder"]:
            raise AssertionError("cmake-comment-skipped: commented test was listed")

    def cmake_empty():
        with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as tmp:
            tmp.write("# no tests here\n")
            tmp_path = tmp.name
        try:
            list_tests_from_cmakelists(tmp_path)
        except SystemExit as exc:
            if "no tests parsed" not in str(exc):
                raise AssertionError(f"cmake-empty: wrong message {exc}")
            return
        raise AssertionError("cmake-empty: expected SystemExit")

    case("valid", valid)
    case("zero-shard", zero_shard)
    case("double-selected", double_selected)
    case("excluded-not-double", excluded_not_double)
    case("missing-step", missing_step)
    case("missing-pattern", missing_pattern)
    case("cmake-foreach", cmake_foreach)
    case("cmake-registry-foreach", cmake_registry_foreach)
    case("cmake-unexpanded-var", cmake_unexpanded_var)
    case("cmake-comment-skipped", cmake_comment_skipped)
    case("cmake-empty", cmake_empty)
    return len(cases)


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workflow", default=root / ".github/workflows/wo000-delivery-spine.yml")
    parser.add_argument("--test-dir", default=root / "build/host")
    parser.add_argument("--self-test", action="store_true",
                        help="run parser regressions only")
    args = parser.parse_args()

    fixture_count = _self_test()
    if args.self_test:
        print(f"PASS: {fixture_count} ctest shard coverage parser regression checks")
        return 0

    patterns = shard_patterns(Path(args.workflow).read_text())
    tests, source = list_tests(args.test_dir, root / "CMakeLists.txt")

    selected = select_shards(patterns, tests)

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
