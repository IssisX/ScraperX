#!/usr/bin/env python3
"""Check that every native test group's content runs in CI.

The CTest shard guard proves every registered test runs in exactly
one shard, but it reads only the registered tests: a SCRAPERX_ONLY
group with no slice and no caller runs nowhere in CI, and CI green
says nothing about it -- the same silent-drop hole the matron-climb
registration closed. A group can be covered without its own slice:
its content may run inside another slice's group (board and scan
run inside matron-climb) or inside another slice's dispatch (the
up-throw content runs inside run_up_controls, called by the
athletic-traversal-1 slice). So this guard checks content,
not registration: for every group, every test function its dispatch
reaches that asserts something itself -- its own body holds a
require( -- must also be reachable from a CI entry point, every
slice's dispatch. A dispatch entry that only
delegates asserts nothing itself, so it is routing, not content.
require( is the only assertion marker in tests/.

Layering: this guard trusts the CTest guard to prove the slices
actually run in CI (each in exactly one shard). Together they prove
every group's content runs; neither alone does.

The groups register themselves (tests/sim_registry.hpp, J-074): a
SCRAPERX_SELECT or SCRAPERX_SLICE line is a group's dispatch (a
SCRAPERX_SLICE is also its CTest slice), and the SCRAPERX_GROUP lines,
in (key, name) order, are the default sequence the bare binary runs
locally. The GROUP lines are NOT a CI entry point: since J-071 step 2
the default sequence runs in CI only as the two athletic-traversal
slices, whose dispatches call the first and second half of the
sequence, and no CI step runs the bare binary. So this guard's CI
entry points are exactly the slices' dispatches; a group in the
registry but in no slice's reachable set runs nowhere in CI, however
many GROUP lines name it. (Before this fix the GROUP lines were
counted as entry points themselves, so a group dropped from both
athletic halves still passed -- the same silent-drop hole, reopened
by the split.)
All are read from every tests/*.cpp.

Rules, all read from the repo as written:
  - every group in the dispatch calls at least one test function,
    and every function it calls is defined; no name is registered
    twice (a key may repeat: groups run in (key, name) order), and the runner and CMakeLists.txt hold no
    hand-written group or slice list;
  - every CTest slice has a SCRAPERX_ONLY dispatch: a slice with
    no dispatch would run the fallthrough, not its group;
  - for every group, every asserting function reachable from its
    dispatch is reachable from a slice's dispatch,
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
import tempfile
from pathlib import Path

# Groups whose content is in no CI entry point for a tracked reason (warning, not failure; stale when covered).
KNOWN_GAPS = {}

SELECT_RE = re.compile(r'\bSCRAPERX_(SELECT|SLICE)\(\s*"([^"]+)"\s*,([^;]*)\)\s*;')
GROUP_RE = re.compile(r'\bSCRAPERX_GROUP\(\s*"([^"]+)"\s*,\s*(-?\d+)\s*,\s*(\w+)\s*\)\s*;')
REG_FN_RE = re.compile(r"\b((?:run|test|up)_[A-Za-z0-9_]+)\b")
TEST_FN_RE = re.compile(r"\b((?:run|test|up)_[A-Za-z0-9_]+)\s*\(")
DEF_RE = re.compile(
    r"\b(?:void|bool|int|float|double|std::[A-Za-z0-9_:<>]+)"
    r"\s+((?:run|test|up)_[A-Za-z0-9_]+)\s*\([^;{]*\)\s*\{")
# The hand lists the registry replaced: neither may come back.
HAND_DISPATCH_RE = re.compile(r'std::string\(only\)\s*==\s*"')
HAND_SLICES_RE = re.compile(r"foreach\s*\(\s*slice\s+(?!IN\s)")


def _strip_code(text, keep_strings=False):
    """Remove comments (and, unless keep_strings, string literals) so braces and calls parse clean."""
    out, i, quote = [], 0, None
    while i < len(text):
        c = text[i]
        if quote:
            if c == "\\":
                if keep_strings:
                    out.append(text[i:i + 2])
                i += 2
                continue
            quote = None if c == quote else quote
            if keep_strings:
                out.append(c)
            i += 1
            continue
        if c in ("'", '"'):
            quote, i = c, i + 1
            if keep_strings:
                out.append(c)
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


def parse_registry(sources):
    """Return (dispatch, fallthrough, slices, errors) from the registrations.

    sources is [(where, text)]. dispatch is {group: [test functions it
    runs]} from SCRAPERX_SELECT and SCRAPERX_SLICE; slices the
    SCRAPERX_SLICE names; fallthrough the SCRAPERX_GROUP functions in
    (key, name) order, as the runner runs them. A name registered
    twice is an error, as it is in the runner; a key may repeat.
    """
    dispatch, slices, groups, errors = {}, [], [], []
    for where, raw in sources:
        text = _strip_code(raw, keep_strings=True)
        for match in SELECT_RE.finditer(text):
            kind, name, fns = match.groups()
            if name in dispatch:
                errors.append(f"SCRAPERX_ONLY name {name} registered twice ({where})")
            dispatch[name] = REG_FN_RE.findall(fns)
            if kind == "SLICE":
                slices.append(name)
        for match in GROUP_RE.finditer(text):
            groups.append((int(match.group(2)), match.group(1), match.group(3), where))
    names = set()
    for _key, name, _fn, where in groups:
        if name in names:
            errors.append(f"group {name} registered twice ({where})")
        names.add(name)
    fallthrough = [fn for _key, _name, fn, _where in sorted(groups, key=lambda g: (g[0], g[1]))]
    return dispatch, fallthrough, slices, errors


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


def read_sources(test_dir):
    """Return [(file name, text)] for every tests/*.cpp."""
    return [(path.name, path.read_text(errors="ignore")) for path in sorted(Path(test_dir).glob("*.cpp"))]


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


def check(runner_text, test_dir, cmake_text):
    """Return (failures, warnings, counts) for the repo as written."""
    dispatch, fallthrough, slices, failures = parse_registry(read_sources(test_dir))
    graph, asserts = parse_call_graph(test_dir)
    warnings = []
    if HAND_DISPATCH_RE.search(_strip_code(runner_text, keep_strings=True)):
        failures.append("the runner holds a hand-written SCRAPERX_ONLY dispatch: register the group instead")
    if any(HAND_SLICES_RE.search(line) for line in cmake_text.splitlines()
           if not line.strip().startswith("#")):
        failures.append("CMakeLists.txt holds a hand-written foreach(slice ...) list: use SCRAPERX_SLICE")
    if not dispatch:
        failures.append("no SCRAPERX_ONLY groups found in the dispatch")
    if not fallthrough:
        failures.append("no SCRAPERX_GROUP default sequence found")
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
    roots = set()
    # CI entry points are exactly the slices' dispatches: every CTest
    # test runs one SCRAPERX_ONLY dispatch, and no CI step runs the
    # bare binary's default sequence. The GROUP lines must NOT be
    # roots: they name the local default sequence, and counting them
    # as covered hid a group dropped from both athletic halves (the
    # J-071 step 2 split runs the sequence only through the two
    # athletic-traversal slice dispatches).
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
    """Parser regressions on synthetic registry/call-graph text."""
    sources = [
        ("a.cpp", 'void run_alpha() {}\nSCRAPERX_SELECT("alpha", run_alpha);\n'
                  'SCRAPERX_GROUP("late", 30, run_late);\n'
                  '// SCRAPERX_SLICE("ghost", run_ghost);\n'),
        ("b.cpp", 'SCRAPERX_SLICE("beta", test_beta,\n                test_beta2);\n'
                  'SCRAPERX_GROUP("fall", 10, run_fall);\n'),
    ]
    dispatch, fallthrough, slices, errors = parse_registry(sources)
    if dispatch != {"alpha": ["run_alpha"], "beta": ["test_beta", "test_beta2"]} or errors:
        raise AssertionError(f"registry parse failed: {dispatch} {errors}")
    if slices != ["beta"]:
        raise AssertionError(f"slice parse failed (a commented slice counts?): {slices}")
    # The fallthrough runs in key order, whatever order the files come in.
    if fallthrough != ["run_fall", "run_late"]:
        raise AssertionError(f"fallthrough order failed: {fallthrough}")
    dup = sources + [("c.cpp", 'SCRAPERX_SELECT("alpha", run_x);\nSCRAPERX_GROUP("fall2", 30, run_y);\n'
                               'SCRAPERX_GROUP("fall", 40, run_z);\n')]
    errors = parse_registry(dup)[3]
    if errors != ["SCRAPERX_ONLY name alpha registered twice (c.cpp)", "group fall registered twice (c.cpp)"]:
        raise AssertionError(f"duplicate name detection failed: {errors}")
    # Two groups on one key run in name order, whatever order the files
    # come in, and a shared key is no error (#482/#483, #524/J-043).
    shared = [("d.cpp", 'SCRAPERX_GROUP("zeta", 20, run_zeta);\n'),
              ("e.cpp", 'SCRAPERX_GROUP("eta", 20, run_eta);\n')] + sources
    _dispatch, fallthrough, _slices, errors = parse_registry(shared)
    if errors or fallthrough != ["run_fall", "run_eta", "run_zeta", "run_late"]:
        raise AssertionError(f"shared key order failed: {fallthrough} {errors}")
    graph = {
        "run_alpha": {"test_a"}, "test_beta": {"test_b"}, "run_fall": {"test_a"},
        "test_a": set(), "test_b": set(),
        "run_wrap": {"run_mid"}, "run_mid": {"test_a"},
        "run_half": {"run_fall"},
    }
    asserts = {
        "run_alpha": False, "test_beta": True, "run_fall": False,
        "test_a": True, "test_b": True, "run_wrap": False, "run_mid": False,
        "run_half": False,
    }
    # alpha is not a slice, but its content runs inside the half slice's
    # dispatch. The CI entry points are the slices only, never the
    # GROUP lines themselves.
    covered = reachable(["run_half", "test_beta"], graph)
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
    # A group in the registry but in no slice's reachable set runs
    # nowhere in CI: the old GROUP-line roots hid exactly this (a
    # group dropped from both athletic halves still passed).
    with tempfile.TemporaryDirectory() as tmp:
        half = Path(tmp) / "sim_tests_half.cpp"
        dropped = (
            "void run_half() { run_a(); }\n"
            "void run_a() { require(1); }\n"
            "void run_b() { require(1); }\n"
            'SCRAPERX_SLICE("half", run_half);\n'
            'SCRAPERX_GROUP("a", 10, run_a);\n'
            'SCRAPERX_GROUP("b", 20, run_b);\n'
            'SCRAPERX_SELECT("a", run_a);\n'
            'SCRAPERX_SELECT("b", run_b);\n'
        )
        half.write_text(dropped)
        failures, _warnings, _counts = check("", tmp, "")
        if not any("group b content runs nowhere in CI" in f for f in failures):
            raise AssertionError(f"dropped group not flagged: {failures}")
        # The same registry with the group called by the half passes.
        half.write_text(dropped.replace(
            "void run_half() { run_a(); }",
            "void run_half() { run_a(); run_b(); }",
        ))
        failures, _warnings, _counts = check("", tmp, "")
        if failures:
            raise AssertionError(f"covered group flagged: {failures}")
    return 11


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
