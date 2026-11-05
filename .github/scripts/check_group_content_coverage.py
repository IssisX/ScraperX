#!/usr/bin/env python3
"""Check that every native test group's content runs in CI.

The CTest shard guard proves every registered test runs in exactly
one shard, but it reads only the registered tests: a SCRAPERX_ONLY
group with no slice and no caller runs nowhere in CI, and CI green
says nothing about it -- the same silent-drop hole the matron-climb
registration closed. A group can be covered without its own slice:
its content may run inside another slice's group (board and scan
run inside matron-climb) or inside the fallthrough (the up-throw
content runs inside run_up_controls). So this guard checks content,
not registration: for every group, every test function its dispatch
reaches that asserts something itself -- its own body holds a
require( -- must also be reachable from a CI entry point, the
fallthrough plus every slice's dispatch. A dispatch entry that only
delegates asserts nothing itself, so it is routing, not content.
require( is the only assertion marker in tests/.

Layering: this guard trusts the CTest guard to prove the slices and
the fallthrough actually run in CI. Together they prove every
group's content runs; neither alone does.

Rules, all read from the repo as written:
  - every group in the dispatch calls at least one test function,
    and every function it calls is defined;
  - every CTest slice has a SCRAPERX_ONLY dispatch: a slice with
    no dispatch would run the fallthrough, not its group;
  - for every group, every asserting function reachable from its
    dispatch is reachable from the fallthrough or a slice's dispatch,
    or the group is a known gap (warning, not failure); a known gap
    whose content becomes covered fails as stale.

KNOWN_GAPS names groups whose content is in no CI entry point for a
tracked reason. Each warns, never fails, so one known gap cannot hide
a new one. A stale entry fails -- remove it only together with the
change that now covers the group.

Usage:
    python3 .github/scripts/check_group_content_coverage.py \
        --dispatch tests/simulation_tests.cpp \
        --test-dir tests --cmake CMakeLists.txt
"""

import argparse
import re
import sys
from pathlib import Path

# Groups whose content is in no CI entry point for a tracked reason (warning, not failure; stale when covered).
KNOWN_GAPS = {}

DISPATCH_RE = re.compile(r'std::string\(only\) == "([a-z0-9-]+)"\)\s*\{([^}]*)\}')
TEST_FN_RE = re.compile(r"\b((?:run|test|up)_[A-Za-z0-9_]+)\s*\(")
DEF_RE = re.compile(
    r"\b(?:void|bool|int|float|double|std::[A-Za-z0-9_:<>]+)"
    r"\s+((?:run|test|up)_[A-Za-z0-9_]+)\s*\([^;{]*\)\s*\{")
SLICES_RE = re.compile(r"foreach\(slice ([^)]+)\)")


def _strip_code(text):
    """Remove comments and string literals so braces and calls parse clean."""
    out, i, quote = [], 0, None
    while i < len(text):
        c = text[i]
        if quote:
            if c == "\\":
                i += 2
                continue
            quote = None if c == quote else quote
            i += 1
            continue
        if c in ("'", '"'):
            quote, i = c, i + 1
            continue
        if text.startswith("//", i):
            i = text.find("\n", i)
            i = len(text) if i < 0 else i
            continue
        if text.startswith("/*", i):
            i = text.find("*/", i + 2)
            i = len(text) if i < 0 else i + 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


def parse_dispatch(text):
    """Return {group: [test functions called]} plus end of the last block."""
    dispatch, end = {}, 0
    for match in DISPATCH_RE.finditer(text):
        dispatch[match.group(1)] = TEST_FN_RE.findall(match.group(2))
        end = match.end()
    return dispatch, end


def parse_fallthrough(text, dispatch_end):
    """Return the test functions called in main() after the dispatch chain."""
    tail = text[dispatch_end:]
    stop = tail.find("return EXIT_SUCCESS")
    if stop < 0:
        raise ValueError("no fallthrough return found after the dispatch chain")
    return TEST_FN_RE.findall(tail[:stop])


def parse_call_graph(test_dir):
    """Return ({fn: {fns it calls}}, {fn: asserts}). A function asserts
    when its own body holds a require( -- the only assertion marker
    in tests/. One with no require( only delegates: routing, not content."""
    graph, asserts = {}, {}
    for path in sorted(Path(test_dir).glob("*.cpp")):
        text = _strip_code(path.read_text(errors="ignore"))
        for match in DEF_RE.finditer(text):
            name, start = match.group(1), match.end()
            depth, i = 1, start
            while i < len(text) and depth:
                if text[i] == "{":
                    depth += 1
                elif text[i] == "}":
                    depth -= 1
                i += 1
            body = text[start:i - 1]
            graph.setdefault(name, set()).update(
                set(TEST_FN_RE.findall(body)) - {name}
            )
            asserts[name] = bool(re.search(r"\brequire\s*\(", body))
    return graph, asserts


def parse_slices(cmake_text):
    """Return the CTest slice names from the CMakeLists foreach."""
    match = SLICES_RE.search(cmake_text)
    if not match:
        raise ValueError("no foreach(slice ...) list found in CMakeLists.txt")
    return match.group(1).split()


def reachable(roots, graph):
    """Return every test function reachable from roots through the graph."""
    out, stack = set(), list(roots)
    while stack:
        fn = stack.pop()
        if fn in out:
            continue
        out.add(fn)
        stack.extend(graph.get(fn, ()))
    return out


def uncovered_content(roots, covered, graph, asserts):
    """Return the asserting functions reachable from roots that CI misses.

    Only functions that assert something themselves are content: a
    pure delegator asserts nothing, so it is routing, not content,
    and its absence from CI is not a gap -- what matters is whether
    the asserting functions it calls are covered.
    """
    return {
        fn for fn in reachable(roots, graph)
        if asserts.get(fn) and fn not in covered
    }


def check(dispatch_text, test_dir, cmake_text):
    """Return (failures, warnings, counts) for the repo as written."""
    dispatch, dispatch_end = parse_dispatch(dispatch_text)
    fallthrough = parse_fallthrough(dispatch_text, dispatch_end)
    graph, asserts = parse_call_graph(test_dir)
    slices = parse_slices(cmake_text)
    failures, warnings = [], []
    if not dispatch:
        failures.append("no SCRAPERX_ONLY groups found in the dispatch")
    for group, fns in sorted(dispatch.items()):
        if not fns:
            failures.append(f"group {group} calls no test function")
        for fn in fns:
            if fn not in graph:
                failures.append(f"group {group} calls undefined {fn}")
    for slice_name in slices:
        if slice_name not in dispatch:
            failures.append(
                f"slice {slice_name} has no SCRAPERX_ONLY dispatch "
                "(it would run the fallthrough, not its group)"
            )
    roots = set(fallthrough)
    for slice_name in slices:
        roots.update(dispatch.get(slice_name, []))
    covered = reachable(roots, graph)
    for group in sorted(dispatch):
        missing = sorted(uncovered_content(dispatch[group], covered, graph, asserts))
        if group in KNOWN_GAPS:
            if not missing:
                failures.append(
                    f"stale known gap, group {group} is now covered "
                    "-- remove it from KNOWN_GAPS"
                )
            else:
                warnings.append(
                    f"known gap, group {group} content in no CI entry "
                    f"point: {', '.join(missing)} ({KNOWN_GAPS[group]})"
                )
        elif missing:
            failures.append(
                f"group {group} content runs nowhere in CI: "
                f"{', '.join(missing)}"
            )
    counts = (len(dispatch), len(slices), sum(1 for fn in covered if asserts.get(fn)))
    return failures, warnings, counts


def _self_test():
    """Parser regressions on synthetic dispatch/call-graph text."""
    dispatch_text = (
        'if (std::string(only) == "alpha") { run_alpha(); return EXIT_SUCCESS; }\n'
        'if (std::string(only) == "beta") {\n'
        "    test_beta();\n    return 0;\n}\n"
        "run_fall();\nreturn EXIT_SUCCESS;\n"
    )
    dispatch, end = parse_dispatch(dispatch_text)
    if dispatch != {"alpha": ["run_alpha"], "beta": ["test_beta"]}:
        raise AssertionError(f"dispatch parse failed: {dispatch}")
    if parse_fallthrough(dispatch_text, end) != ["run_fall"]:
        raise AssertionError("fallthrough parse failed")
    graph = {
        "run_alpha": {"test_a"}, "test_beta": {"test_b"}, "run_fall": {"test_a"},
        "test_a": set(), "test_b": set(),
        "run_wrap": {"run_mid"}, "run_mid": {"test_a"},
    }
    asserts = {
        "run_alpha": False, "test_beta": True, "run_fall": False,
        "test_a": True, "test_b": True, "run_wrap": False, "run_mid": False,
    }
    # alpha is not a slice, but its content runs in the fallthrough.
    covered = reachable(["run_fall", "test_beta"], graph)
    if uncovered_content(["run_alpha"], covered, graph, asserts):
        raise AssertionError("shared content reported as uncovered")
    # A wrapper of a wrapper is routing, not content, when its content is covered.
    if uncovered_content(["run_wrap"], covered, graph, asserts):
        raise AssertionError("wrapper of a wrapper was not excused")
    # A group whose content only its own dispatch reaches must fail,
    # naming the asserting function, not its delegating wrapper.
    graph["run_solo"] = {"test_solo"}
    graph["test_solo"] = set()
    asserts["run_solo"] = False
    asserts["test_solo"] = True
    if uncovered_content(["run_solo"], covered, graph, asserts) != {"test_solo"}:
        raise AssertionError("solo-content detection failed")
    # A leaf test function called directly by a non-slice group must fail.
    graph["test_leaf"] = set()
    asserts["test_leaf"] = True
    if uncovered_content(["test_leaf"], covered, graph, asserts) != {"test_leaf"}:
        raise AssertionError("direct leaf-test detection failed")
    return 4


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dispatch", default=root / "tests/simulation_tests.cpp")
    parser.add_argument("--test-dir", default=root / "tests")
    parser.add_argument("--cmake", default=root / "CMakeLists.txt")
    parser.add_argument("--self-test", action="store_true", help="run parser regressions only")
    args = parser.parse_args()

    checks = _self_test()
    if args.self_test:
        print(f"PASS: {checks} group-content parser regression checks")
        return 0

    failures, warnings, counts = check(
        Path(args.dispatch).read_text(), args.test_dir, Path(args.cmake).read_text()
    )
    groups, slices, covered = counts
    print(f"SCRAPERX_ONLY groups: {groups}")
    print(f"CTest slices: {slices}")
    print(f"asserting test functions reachable from CI entry points: {covered}")
    for warning in warnings:
        print(f"WARNING {warning}")
    if failures:
        print("FAIL: group content coverage gaps:")
        for failure in failures:
            print(f"  {failure}")
        return 1
    print(
        f"PASS: {groups} groups, {slices} slices, every group's content "
        f"runs in CI, {len(KNOWN_GAPS)} known gap(s) warned"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
