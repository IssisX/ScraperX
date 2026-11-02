#!/usr/bin/env python3
"""Check that every runnable touch scenario runs in a CI touch shard.

The godot-touch job splits UITEST scenarios across two shards by
hand-written `for scenario in ...` lists (touch-1, touch-2). A scenario
in neither list never runs in CI, and CI green would say nothing about
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

KNOWN_GAPS names registered touch scenarios that are in no shard for
a tracked reason. Each prints as a WARNING and does not fail the
guard, so one known gap cannot hide a new one: any other registered
scenario with zero runs still fails, and a KNOWN_GAPS entry that
becomes covered fails as stale -- remove it when the scenario lands
in a shard.

Usage:
    python3 .github/scripts/check_touch_shard_coverage.py \
        --workflow .github/workflows/wo000-delivery-spine.yml \
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
]

SKIP_PREFIXES = ("look_", "pad_", "keyboard_")
# Run in another CI job, or measured locally only -- not godot-touch.
SKIP_EXACT = {"audio_mix", "audio_fall", "perf_top", "perf_grade"}

# Registered touch scenarios in no shard for a tracked reason. Warning,
# not failure; stale the moment the scenario appears in a shard.
KNOWN_GAPS = {
    "touch_matron_pipe": (
        "encounter-owned known trunk failure (drum staging decision "
        "pending); runs locally, in no CI touch shard yet"
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


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workflow", default=root / ".github/workflows/wo000-delivery-spine.yml")
    parser.add_argument("--driver", default=root / "godot/presentation/ui/ui_test_driver.gd")
    args = parser.parse_args()

    lists = shard_lists(Path(args.workflow).read_text())
    registered = registered_scenarios(Path(args.driver).read_text())
    registered_set = set(registered)

    for shard, names in lists.items():
        print(f"{shard}: {len(names)} runs")

    all_listed = [name for names in lists.values() for name in names]
    failures = []

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
    for name in sorted(registered_set):
        if name in covered:
            continue
        reason = skipped_reason(name)
        if reason is not None:
            skipped.setdefault(reason, []).append(name)
        elif name in KNOWN_GAPS:
            print(f"WARNING known gap, in no shard: {name} ({KNOWN_GAPS[name]})")
        else:
            missing.append(name)
    for name in sorted(missing):
        failures.append(f"registered but in no shard: {name}")

    stale = [n for n in sorted(KNOWN_GAPS) if n in covered]
    for name in stale:
        failures.append(f"stale known gap, now covered -- remove from KNOWN_GAPS: {name}")

    for reason, names in sorted(skipped.items()):
        print(f"skipped by design ({reason}): {len(names)}")

    if failures:
        print("FAIL: touch shard coverage gaps:")
        for failure in failures:
            print(f"  {failure}")
        return 1
    print(
        f"PASS: {len(covered)} registered scenarios covered, "
        f"{sum(len(v) for v in skipped.values())} skipped by design, "
        f"{len(KNOWN_GAPS) - len(stale)} known gap(s) warned"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
