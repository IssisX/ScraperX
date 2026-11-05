#!/usr/bin/env python3
"""Check that every proof job is gated on its own scope output.

The scope job decides which test groups a push reaches and publishes
the decision as job outputs (game, native, godot, android). Each
proof job gates on one of those outputs with a job-level `if:`.
That gate is a silent drop in two directions:

  - a wrong output name (a typo, or drift after proof_scope.py
    renames one) reads empty in GitHub Actions, so the comparison
    is false and the job is skipped on every push -- CI stays
    green with zero proof of that group;
  - a wrong group (native-tests gated on `godot`) skips the job
    exactly on the pushes that change its group -- a native-only
    push goes green without ever running the native tests.

A dropped `needs:` entry is the same hole one layer down: a
needed job that is skipped skips its dependents, so android-apk
(which has no `if:` of its own) is gated only by needing
android-build, and publish-checkpoint only by needing all six
proof jobs.

No other guard reads these gates: the shard guards read their own
jobs' steps, the audits guard reads the audits job's steps, the
guard-of-guards reads the host-build steps, and the scope-step
guard reads the scope job's steps. Proven 2026-11-05: on workflow
copies with native-tests gated on `godot`, on a typo'd `nativ`
output, with the `native` scope output deleted, and with the
`native` scope output wired to the godot step output, all seven
existing and pending guards pass.

Rules, all read from the workflow as written:
  - the scope job declares exactly game, native, godot, android,
    each wired to steps.scope.outputs.<same name>, and every
    needs.scope.outputs.<name> reference names one of the four;
  - host-build gates on game, native-tests on native, godot-touch
    and godot-audits-and-mix on godot, android-build on android,
    each with exactly the needs: set its gate requires;
  - android-apk has no job-level if: and needs exactly host-build
    and android-build; publish-checkpoint and playtest-apk need
    all six proof jobs, the first naming the trunk ref and the
    android-apk result in its if:, the second dispatch-only.

The writer side is pinned elsewhere: test_proof_scope.py's
test_outputs asserts proof_scope.py writes exactly those four,
and the scope job runs those unit tests on every push. This guard
pins the reader side, so the two cannot drift apart silently.

The guard is wired in the scope job itself, so it runs on every
push: a broken gate cannot also skip its own guard. It is wired
in host-build too, exactly once, which backstops the scope-job
wiring on game pushes and keeps the guard-of-guards passing. A
script deleted together with both of its steps passes by design --
a deliberate retirement, visible as a file deletion, not a drop.

Usage:
    python3 .github/scripts/check_job_gate_coverage.py \
        --workflow .github/workflows/wo000-delivery-spine.yml
"""

import argparse
import re
import sys
from pathlib import Path

JOB_HEADER = re.compile(r"^  ([A-Za-z0-9_-]+):\s*$")
STEPS_HEADER = re.compile(r"^    steps:\s*$")
NEEDS_LINE = re.compile(r"^    needs:\s*(.+?)\s*$")
IF_LINE = re.compile(r"^    if:\s*(.+?)\s*$")
OUTPUTS_HEADER = re.compile(r"^    outputs:\s*$")
OUTPUT_LINE = re.compile(r"^      ([A-Za-z0-9_-]+):\s*(.+?)\s*$")
SCOPE_OUTPUT_REF = re.compile(r"needs\.scope\.outputs\.([A-Za-z0-9_-]+)")
IF_CONTINUATION = re.compile(r"^      \S")

OUTPUTS = ("game", "native", "godot", "android")

# job -> (scope output its if: must gate on, exact needs: set)
GATED = {
    "host-build": ("game", {"scope"}),
    "native-tests": ("native", {"scope", "host-build"}),
    "godot-touch": ("godot", {"scope", "host-build"}),
    "godot-audits-and-mix": ("godot", {"scope", "host-build"}),
    "android-build": ("android", {"scope"}),
}

# job -> exact needs: set (no if: of its own; the needs are the gate)
TRANSITIVE = {
    "android-apk": {"host-build", "android-build"},
    "publish-checkpoint": {"host-build", "native-tests", "godot-touch",
                           "godot-audits-and-mix", "android-build", "android-apk"},
    "playtest-apk": {"host-build", "native-tests", "godot-touch",
                     "godot-audits-and-mix", "android-build", "android-apk"},
}


def job_blocks(workflow_text):
    """Split the workflow into {job name: header lines before steps:}."""
    lines = workflow_text.splitlines()
    jobs = {}
    current = None
    for line in lines:
        match = JOB_HEADER.match(line)
        if match:
            current = match.group(1)
            jobs[current] = []
        elif current is not None:
            if STEPS_HEADER.match(line):
                current = None
            else:
                jobs[current].append(line)
    return jobs


def parse_needs(header_lines):
    """The needs: set of a job header, or None if the job has none."""
    for line in header_lines:
        match = NEEDS_LINE.match(line)
        if match:
            value = match.group(1).strip()
            if value.startswith("[") and value.endswith("]"):
                value = value[1:-1]
            return {n.strip() for n in value.split(",") if n.strip()}
    return None


def parse_if(header_lines):
    """The job-level if: expression, folded continuations joined, or None."""
    for i, line in enumerate(header_lines):
        match = IF_LINE.match(line)
        if match:
            value = match.group(1).strip()
            if value in (">-", ">", "|", "|-"):
                parts = []
                for cont in header_lines[i + 1:]:
                    if IF_CONTINUATION.match(cont):
                        parts.append(cont.strip())
                    else:
                        break
                value = " ".join(parts)
            return " ".join(value.split())
    return None


def parse_scope_outputs(header_lines):
    """The scope job's {output name: value} and any duplicated output names."""
    for i, line in enumerate(header_lines):
        if OUTPUTS_HEADER.match(line):
            outputs = {}
            duplicates = []
            for rest in header_lines[i + 1:]:
                if not rest.strip():
                    continue
                match = OUTPUT_LINE.match(rest)
                if match:
                    if match.group(1) in outputs:
                        duplicates.append(match.group(1))
                    outputs[match.group(1)] = match.group(2)
                else:
                    break
            return outputs, duplicates
    return None, []


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--workflow",
                        default=".github/workflows/wo000-delivery-spine.yml")
    args = parser.parse_args()

    workflow_text = Path(args.workflow).read_text()
    jobs = job_blocks(workflow_text)
    problems = []

    if "scope" not in jobs:
        print("FAIL: no scope job in workflow")
        return 1
    declared, duplicates = parse_scope_outputs(jobs["scope"])
    if declared is None:
        problems.append("missing: scope job declares no outputs:")
    else:
        for name in duplicates:
            problems.append(f"duplicated: scope output {name} declared twice")
        if set(declared) != set(OUTPUTS):
            problems.append(
                f"wrong-outputs: scope job declares {sorted(declared)}, expected {sorted(OUTPUTS)}")
        for name in OUTPUTS:
            if name in declared:
                want = "${{ steps.scope.outputs." + name + " }}"
                if declared[name] != want:
                    problems.append(
                        f"wrong-source: scope output {name} is wired to {declared[name]!r}, expected {want!r}")
    known_outputs = set(declared) if declared is not None else set()

    # A duplicated needs: or if: line is last-wins in YAML, so a bad
    # merge can silently replace the gate this guard just read.
    for job in ("scope", *GATED, *TRANSITIVE):
        if job not in jobs:
            continue
        for pattern, field in ((NEEDS_LINE, "needs:"), (IF_LINE, "if:")):
            if sum(1 for line in jobs[job] if pattern.match(line)) > 1:
                problems.append(f"duplicated: {job} declares {field} twice")

    def check_refs(where, expression):
        for name in SCOPE_OUTPUT_REF.findall(expression or ""):
            if name not in known_outputs:
                problems.append(
                    f"unknown-output: {where} references needs.scope.outputs.{name}, "
                    f"which the scope job does not declare")

    if declared is not None:
        for name, value in declared.items():
            check_refs(f"scope output {name}", value)

    for job, (group, needs) in GATED.items():
        if job not in jobs:
            problems.append(f"missing: no {job} job in workflow")
            continue
        header = jobs[job]
        actual_needs = parse_needs(header)
        if actual_needs is None:
            problems.append(f"missing-needs: {job} has no needs:")
        elif actual_needs != needs:
            problems.append(
                f"wrong-needs: {job} needs {sorted(actual_needs)}, expected {sorted(needs)}")
        actual_if = parse_if(header)
        want_if = f"needs.scope.outputs.{group} == 'true'"
        if actual_if is None:
            problems.append(f"missing-gate: {job} has no job-level if:")
        elif actual_if != want_if:
            problems.append(
                f"wrong-gate: {job} gates on {actual_if!r}, expected {want_if!r}")
        else:
            check_refs(f"{job} gate", actual_if)

    for job, needs in TRANSITIVE.items():
        if job not in jobs:
            problems.append(f"missing: no {job} job in workflow")
            continue
        header = jobs[job]
        actual_needs = parse_needs(header)
        if actual_needs is None:
            problems.append(f"missing-needs: {job} has no needs:")
        elif actual_needs != needs:
            problems.append(
                f"wrong-needs: {job} needs {sorted(actual_needs)}, expected {sorted(needs)}")

    if "android-apk" in jobs:
        if parse_if(jobs["android-apk"]) is not None:
            problems.append(
                "unexpected-gate: android-apk has a job-level if:; it is gated only by needing android-build")

    if "publish-checkpoint" in jobs:
        gate = parse_if(jobs["publish-checkpoint"])
        if gate is None:
            problems.append("missing-gate: publish-checkpoint has no job-level if:")
        else:
            for marker in ("github.ref == 'refs/heads/ScraperX-Claude'",
                           "needs.android-apk.result"):
                if marker not in gate:
                    problems.append(
                        f"wrong-gate: publish-checkpoint gate is missing {marker!r}")
            check_refs("publish-checkpoint gate", gate)

    if "playtest-apk" in jobs:
        gate = parse_if(jobs["playtest-apk"])
        if gate is None:
            problems.append("missing-gate: playtest-apk has no job-level if:")
        else:
            for marker in ("workflow_dispatch", "refs/heads/ScraperX-Claude"):
                if marker not in gate:
                    problems.append(
                        f"wrong-gate: playtest-apk gate is missing {marker!r}")

    if problems:
        for problem in problems:
            print(f"FAIL: {problem}")
        return 1
    print("PASS: scope declares game/native/godot/android and every proof job "
          "gates on its own scope output (transitive needs gate the rest)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
