#!/usr/bin/env python3
"""Check that every runnable touch scenario runs in a CI touch shard.

The godot-touch job splits UITEST scenarios across three shards by
hand-written `for scenario in ...` lists (touch-1, touch-2, touch-3). A
scenario in no list never runs in CI, and CI green would say nothing about
it; a scenario listed twice runs twice. Until now that partition was
verified by hand each time a list changed. This script is the touch
counterpart of check_ctest_shard_coverage.py: it reads the shard lists
out of the workflow file itself, so it checks the workflow as written,
not a copy of its lists, and compares them against the scenario
registry in godot/presentation/ui/ui_test_driver.gd.

Scope is the godot-touch job only. Scenarios that run in other jobs
are not double-runs here: touch_summit_ending also runs rendered in
godot-audits-and-mix, and audio_mix/audio_fall run in that job's audio
loop, so neither is flagged for appearing outside godot-touch.

A listed name ending in _buttons is the same scenario played on the
BUTTONS scheme (the driver trims the suffix), so it is a distinct run
of a registered scenario, not a duplicate. The guard therefore fails
on an exact name listed twice, and on a registered scenario with zero
runs -- not on a scenario run once per scheme.

Scenarios skipped by design, with the in-repo evidence for each:
  look_*        renders only, not in CI (driver comments; the local
                runner is tools/look/look.sh).
  pad_*, keyboard_*  retired devices: the workflow's own touch-1
                comment records the owner rule (no gamepad, keyboard
                or mouse, now or ever).
  audio_mix, audio_fall  run in godot-audits-and-mix ("Measure the
                mix"), not in godot-touch.
  perf_top, perf_grade   measured locally, not in CI.
  perf_locker, perf_view_bottom, perf_view_sling, perf_view_top,
  perf_attrib, perf_attrib_top, perf_attrib_locker, perf_walk
                rendered only (J-045, #248): measured locally under
                xvfb; headless draws nothing.

KNOWN_GAPS names registered touch scenarios that are in no shard for
a tracked reason. Each prints as a WARNING and does not fail the
guard, so one known gap cannot hide a new one: any other registered
scenario with zero runs still fails, and a KNOWN_GAPS entry that
becomes covered fails as stale -- remove it when the scenario lands
in a shard.

The shard-selection rule itself had no regression checks: a logic
regression in it passes the real workflow while a scenario goes
unselected or double-selected, and no other guard reads the touch
shard lists. Proven 2026-11-05: on a fixture workflow whose driver
registers a scenario no shard lists, the real guard fails (exit 1,
"registered but in no shard: touch_orphan"), while a copy with the
missing-scenario detection removed passes it (exit 0). The parser
regression checks in _self_test() run on every invocation, so that
class of regression trips the guard itself before CI trusts it.

Usage:
    python3 .github/scripts/check_touch_shard_coverage.py \\
        --workflow .github/workflows/wo000-delivery-spine.yml \\
        --driver godot/presentation/ui/ui_test_driver.gd
"""

import argparse
import re
import sys
from collections import Counter
from pathlib import Path

# Step names in wo000-delivery-spine.yml, in shard order.
SHARDS = [
    ("touch-1", "Drive control paths through the real input pipeline (shard 1)"),
    ("touch-2", "Drive control paths through the real input pipeline (shard 2)"),
    ("touch-3", "Drive control paths through the real input pipeline (shard 3)"),
]

SKIP_PREFIXES = ("look_", "pad_", "keyboard_")
# Run in another CI job, or measured locally only -- not godot-touch.
SKIP_EXACT = {"audio_mix", "audio_fall", "perf_top", "perf_grade",
              "perf_locker", "perf_view_bottom", "perf_view_sling", "perf_view_top",
              "perf_attrib", "perf_attrib_top", "perf_attrib_locker", "perf_walk"}

# Registered touch scenarios in no shard for a tracked reason. Warning,
# not failure; stale the moment the scenario appears in a shard.
KNOWN_GAPS = {
    "touch_matron_pipe": (
        "fails on trunk (rechecked 2026-11-04, issue #134): at the take-back "
        "the drum lies 17.8 m south of where it was set down and the "
        "climber is on 143, so PICK UP is never offered; PR #29 proposes "
        "a staging fix. Wire it into a shard together with a fix"
    ),
}


def shard_lists(workflow_text):
    """Return {shard: [scenario names]} read from the workflow steps."""
    job_start = workflow_text.find("\n  godot-touch:")
    if job_start < 0:
        raise SystemExit("check_touch_shard_coverage: no godot-touch job")
    job_end = workflow_text.find("\n  godot-audits-and-mix:", job_start)
    job = workflow_text[job_start:job_end if job_end >= 0 else None]
    lists = {}
    for shard, step_name in SHARDS:
        start = job.find(step_name)
        if start < 0:
            raise SystemExit(f"check_touch_shard_coverage: no step named {step_name!r}")
        block = job[start:]
        next_step = block.find("\n      - name:", 1)
        if next_step >= 0:
            block = block[:next_step]
        match = re.search(r"for scenario in (.*?); do", block, re.S)
        if match is None:
            raise SystemExit(f"check_touch_shard_coverage: no scenario loop in step {step_name!r}")
        names = match.group(1).replace("\\", " ").split()
        if not names:
            raise SystemExit(f"check_touch_shard_coverage: empty scenario loop in step {step_name!r}")
        lists[shard] = names
    return lists


def registered_scenarios(driver_text):
    """Return the SCENARIOS dict keys from ui_test_driver.gd."""
    match = re.search(r"const SCENARIOS := \{(.*?)\n\}", driver_text, re.S)
    if match is None:
        raise SystemExit("check_touch_shard_coverage: no SCENARIOS dict in driver")
    keys = []
    for line in match.group(1).splitlines():
        code = line.split("#", 1)[0]
        keys += re.findall(r'"([^"]+)":', code)
    if not keys:
        raise SystemExit("check_touch_shard_coverage: empty SCENARIOS dict in driver")
    return keys


def played_name(listed):
    """A listed run's scenario: the driver trims a _buttons suffix."""
    return listed[:-8] if listed.endswith("_buttons") else listed


def skipped_reason(name):
    if name in SKIP_EXACT:
        return "other CI job or local-only measurement"
    for prefix in SKIP_PREFIXES:
        if name.startswith(prefix):
            return "renders-only or retired device"
    return None


def check_coverage(lists, registered):
    """Return the coverage verdict for shard lists vs registered scenarios.

    Pure: no I/O, no prints. Returns a dict with keys covered (set of
    registered scenarios with at least one run), skipped (dict
    reason -> [names] skipped by design), known_warned ([names] in
    KNOWN_GAPS still uncovered), and failures ([str] in the order the
    old inline code appended them: duplicates, listed-not-registered,
    registered-in-no-shard, stale known gaps).
    """
    registered_set = set(registered)
    failures = []

    all_listed = [name for names in lists.values() for name in names]
    duplicates = [n for n, c in Counter(all_listed).items() if c > 1]
    for name in sorted(duplicates):
        failures.append(f"listed twice: {name}")

    for name in sorted(set(all_listed)):
        played = played_name(name)
        if played not in registered_set:
            failures.append(f"listed but not registered: {name} (played {played})")

    covered = {played_name(n) for n in all_listed}
    skipped = {}
    missing = []
    known_warned = []
    for name in sorted(registered_set):
        if name in covered:
            continue
        reason = skipped_reason(name)
        if reason is not None:
            skipped.setdefault(reason, []).append(name)
        elif name in KNOWN_GAPS:
            known_warned.append(name)
        else:
            missing.append(name)
    for name in sorted(missing):
        failures.append(f"registered but in no shard: {name}")

    stale = [n for n in sorted(KNOWN_GAPS) if n in covered]
    for name in stale:
        failures.append(f"stale known gap, now covered -- remove from KNOWN_GAPS: {name}")

    return {
        "covered": covered,
        "skipped": skipped,
        "known_warned": known_warned,
        "failures": failures,
    }


def _self_test():
    """Regression checks for the shard parser and selector, run on every invocation."""
    good_workflow = (
        "name: test\njobs:\n  godot-touch:\n    steps:\n"
        "      - name: Drive control paths through the real input pipeline (shard 1)\n"
        "        run: |\n"
        "          for scenario in touch_alpha touch_beta touch_hold_buttons; do\n"
        "            echo \"$scenario\"\n"
        "          done\n"
        "      - name: Drive control paths through the real input pipeline (shard 2)\n"
        "        run: |\n"
        "          for scenario in touch_gamma \\\n"
        "            touch_delta touch_hold; do\n"
        "            echo \"$scenario\"\n"
        "          done\n"
        "      - name: Drive control paths through the real input pipeline (shard 3)\n"
        "        run: |\n"
        "          for scenario in touch_epsilon; do\n"
        "            echo \"$scenario\"\n"
        "          done\n"
        "  godot-audits-and-mix:\n    steps:\n"
        "      - name: Measure the mix\n"
        "        run: echo mix\n"
    )
    good_driver = (
        'const SCENARIOS := {\n'
        '"touch_alpha": {},\n'
        '"touch_beta": {},\n'
        '"touch_gamma": {},\n'
        '"touch_delta": {},\n'
        '"touch_hold": {},\n'
        '"touch_epsilon": {},\n'
        '"audio_mix": {},\n'
        '"look_still": {},\n'
        '"perf_attrib": {},\n'
        '"touch_matron_pipe": {},\n'
        '}\n'
    )
    cases = []

    def case(name, fn):
        fn()
        cases.append(name)

    def valid():
        verdict = check_coverage(shard_lists(good_workflow),
                                 registered_scenarios(good_driver))
        if verdict["failures"]:
            raise AssertionError(f"valid: expected clean, got {verdict['failures']}")
        if verdict["covered"] != {"touch_alpha", "touch_beta", "touch_gamma",
                                  "touch_delta", "touch_hold", "touch_epsilon"}:
            raise AssertionError(f"valid: wrong covered set {verdict['covered']}")
        skipped = verdict["skipped"]
        if skipped.get("other CI job or local-only measurement") != ["audio_mix", "perf_attrib"]:
            raise AssertionError(f"valid: wrong other-job skips {skipped}")
        if skipped.get("renders-only or retired device") != ["look_still"]:
            raise AssertionError(f"valid: wrong renders-only skips {skipped}")
        if verdict["known_warned"] != ["touch_matron_pipe"]:
            raise AssertionError(f"valid: wrong known gaps {verdict['known_warned']}")

    def missing_scenario():
        # A registered scenario no shard lists and no skip rule covers:
        # the exact drift the hole proof caught.
        driver = good_driver.replace('"touch_matron_pipe": {},',
                                     '"touch_matron_pipe": {},\n"touch_orphan": {},', 1)
        verdict = check_coverage(shard_lists(good_workflow),
                                 registered_scenarios(driver))
        if verdict["failures"] != ["registered but in no shard: touch_orphan"]:
            raise AssertionError(f"missing-scenario: got {verdict['failures']}")

    def duplicate():
        # touch_alpha listed in both shards: an exact name twice.
        text = good_workflow.replace("touch_gamma \\", "touch_alpha touch_gamma \\", 1)
        verdict = check_coverage(shard_lists(text),
                                 registered_scenarios(good_driver))
        if verdict["failures"] != ["listed twice: touch_alpha"]:
            raise AssertionError(f"duplicate: got {verdict['failures']}")

    def buttons_not_duplicate():
        # touch_hold runs once per scheme (plain + _buttons): distinct
        # runs of one scenario, not a duplicate, and it is covered.
        verdict = check_coverage(shard_lists(good_workflow),
                                 registered_scenarios(good_driver))
        if any(f.startswith("listed twice") for f in verdict["failures"]):
            raise AssertionError(f"buttons-not-duplicate: got {verdict['failures']}")
        if "touch_hold" not in verdict["covered"]:
            raise AssertionError("buttons-not-duplicate: touch_hold not covered")

    def buttons_unregistered():
        # A _buttons run whose base scenario is not registered.
        text = good_workflow.replace("touch_delta touch_hold",
                                     "touch_delta touch_hold touch_ghost_buttons", 1)
        verdict = check_coverage(shard_lists(text),
                                 registered_scenarios(good_driver))
        expected = "listed but not registered: touch_ghost_buttons (played touch_ghost)"
        if verdict["failures"] != [expected]:
            raise AssertionError(f"buttons-unregistered: got {verdict['failures']}")

    def stale_known_gap():
        # touch_matron_pipe lands in a shard: its KNOWN_GAPS entry is
        # stale until removed.
        text = good_workflow.replace("touch_alpha touch_beta",
                                     "touch_alpha touch_beta touch_matron_pipe", 1)
        verdict = check_coverage(shard_lists(text),
                                 registered_scenarios(good_driver))
        expected = ("stale known gap, now covered -- remove from KNOWN_GAPS: "
                    "touch_matron_pipe")
        if verdict["failures"] != [expected]:
            raise AssertionError(f"stale-known-gap: got {verdict['failures']}")

    def missing_step():
        text = good_workflow.replace(
            "      - name: Drive control paths through the real input pipeline (shard 2)\n",
            "", 1)
        try:
            shard_lists(text)
        except SystemExit as exc:
            if "Drive control paths through the real input pipeline (shard 2)" not in str(exc):
                raise AssertionError(f"missing-step: wrong message {exc}")
            return
        raise AssertionError("missing-step: expected SystemExit")

    def missing_step_3():
        # touch-3's step gone: its scenarios would silently stop running.
        text = good_workflow.replace(
            "      - name: Drive control paths through the real input pipeline (shard 3)\n",
            "", 1)
        try:
            shard_lists(text)
        except SystemExit as exc:
            if "Drive control paths through the real input pipeline (shard 3)" not in str(exc):
                raise AssertionError(f"missing-step-3: wrong message {exc}")
            return
        raise AssertionError("missing-step-3: expected SystemExit")

    def duplicate_across_2_3():
        # touch_gamma listed in shard 2 and again in shard 3.
        text = good_workflow.replace("for scenario in touch_epsilon;",
                                     "for scenario in touch_epsilon touch_gamma;", 1)
        verdict = check_coverage(shard_lists(text),
                                 registered_scenarios(good_driver))
        if verdict["failures"] != ["listed twice: touch_gamma"]:
            raise AssertionError(f"duplicate-across-2-3: got {verdict['failures']}")

    def missing_loop():
        text = good_workflow.replace(
            "          for scenario in touch_gamma \\\n"
            "            touch_delta touch_hold; do\n",
            "          echo no loop here\n", 1)
        try:
            shard_lists(text)
        except SystemExit as exc:
            if "no scenario loop" not in str(exc):
                raise AssertionError(f"missing-loop: wrong message {exc}")
            return
        raise AssertionError("missing-loop: expected SystemExit")

    def empty_loop():
        text = good_workflow.replace(
            "for scenario in touch_alpha touch_beta touch_hold_buttons; do",
            "for scenario in ; do", 1)
        try:
            shard_lists(text)
        except SystemExit as exc:
            if "empty scenario loop" not in str(exc):
                raise AssertionError(f"empty-loop: wrong message {exc}")
            return
        raise AssertionError("empty-loop: expected SystemExit")

    def missing_job():
        try:
            shard_lists("name: test\njobs:\n  other:\n    steps: []\n")
        except SystemExit as exc:
            if "no godot-touch job" not in str(exc):
                raise AssertionError(f"missing-job: wrong message {exc}")
            return
        raise AssertionError("missing-job: expected SystemExit")

    def no_scenarios_dict():
        try:
            registered_scenarios("const OTHER := {\n\"touch_alpha\": {},\n}\n")
        except SystemExit as exc:
            if "no SCENARIOS dict" not in str(exc):
                raise AssertionError(f"no-scenarios-dict: wrong message {exc}")
            return
        raise AssertionError("no-scenarios-dict: expected SystemExit")

    def driver_comment_skipped():
        # A commented-out SCENARIOS entry is not registered.
        driver = good_driver.replace('"touch_delta": {},',
                                     '# "touch_delta": {},', 1)
        if "touch_delta" in registered_scenarios(driver):
            raise AssertionError("driver-comment-skipped: commented scenario was listed")

    def continuation_split():
        # touch_delta reaches shard 2 only through a backslash
        # continuation: the parser must join it, not drop it.
        lists = shard_lists(good_workflow)
        if lists["touch-2"] != ["touch_gamma", "touch_delta", "touch_hold"]:
            raise AssertionError(f"continuation-split: got {lists['touch-2']}")
        # Shard 2's block ends at shard 3's step: no bleed either way.
        if lists["touch-3"] != ["touch_epsilon"]:
            raise AssertionError(f"continuation-split: got touch-3 {lists['touch-3']}")

    case("valid", valid)
    case("missing-scenario", missing_scenario)
    case("duplicate", duplicate)
    case("buttons-not-duplicate", buttons_not_duplicate)
    case("buttons-unregistered", buttons_unregistered)
    case("stale-known-gap", stale_known_gap)
    case("missing-step", missing_step)
    case("missing-step-3", missing_step_3)
    case("duplicate-across-2-3", duplicate_across_2_3)
    case("missing-loop", missing_loop)
    case("empty-loop", empty_loop)
    case("missing-job", missing_job)
    case("no-scenarios-dict", no_scenarios_dict)
    case("driver-comment-skipped", driver_comment_skipped)
    case("continuation-split", continuation_split)
    return len(cases)


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workflow", default=root / ".github/workflows/wo000-delivery-spine.yml")
    parser.add_argument("--driver", default=root / "godot/presentation/ui/ui_test_driver.gd")
    parser.add_argument("--self-test", action="store_true",
                        help="run parser regressions only")
    args = parser.parse_args()

    fixture_count = _self_test()
    if args.self_test:
        print(f"PASS: {fixture_count} touch shard coverage parser regression checks")
        return 0

    lists = shard_lists(Path(args.workflow).read_text())
    registered = registered_scenarios(Path(args.driver).read_text())
    verdict = check_coverage(lists, registered)

    for shard, names in lists.items():
        print(f"{shard}: {len(names)} runs")

    for name in verdict["known_warned"]:
        print(f"WARNING known gap, in no shard: {name} ({KNOWN_GAPS[name]})")

    for reason, names in sorted(verdict["skipped"].items()):
        print(f"skipped by design ({reason}): {len(names)}")

    failures = verdict["failures"]
    if failures:
        print("FAIL: touch shard coverage gaps:")
        for failure in failures:
            print(f"  {failure}")
        return 1
    print(
        f"PASS: {len(verdict['covered'])} registered scenarios covered, "
        f"{sum(len(v) for v in verdict['skipped'].values())} skipped by design, "
        f"{len(KNOWN_GAPS) - len([n for n in verdict['covered'] if n in KNOWN_GAPS])} known gap(s) warned"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
