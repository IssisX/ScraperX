#!/usr/bin/env python3
"""Check that every APK proof step runs in the android-apk and playtest-apk jobs.

The android-apk job exports the checkpoint APK and proves it is this
branch's build: the export step asserts the fresh native library is
inside it (grep for lib/arm64-v8a/libscraperx_native.so), the identity
step reads the package, label, authorities and permissions back out of
the APK (check_apk_identity.sh), and the story-radio step verifies the
recordings packaged in the APK match the manifest
(ci/verify_story_radio_package.py). The playtest-apk job stages the
same APK and re-proves it: the stage step asserts the native library
is inside, and the identity step reads the identity back again.

No other guard reads these steps: the shard guards read their own
jobs, the audits guard reads godot-audits-and-mix, the guard-of-guards
reads host-build, the scope-step guard reads the scope job, and the
job-gate guard reads job-level if:/needs: only -- a step deleted from
either APK job leaves every one of those guards green. Proven
2026-11-05: deleting the identity step from android-apk (or from
playtest-apk), the story-radio step, or either .so grep line passes
all ten existing and pending guards while the APK proof it carried
silently leaves CI.

Rules, all read from the workflow as written:
  - the android-apk job holds exactly one run: step named "Export
    checkpoint Android arm64 APK" containing the .so grep, one named
    "Verify checkpoint Android APK identity" invoking
    check_apk_identity.sh, and one named "Verify exported story-radio
    package" invoking ci/verify_story_radio_package.py;
  - the playtest-apk job holds exactly one run: step named "Stage
    playtest APK" containing the .so grep, and one named "Verify
    playtest Android APK identity" invoking check_apk_identity.sh;
  - each proof step has no step-level if: (a conditionally skipped
    proof is a silently dropped proof).
Job-level gating is not this guard's job: playtest-apk runs only on
dispatch to trunk by design, and the job-gate guard pins that gate.
The stable-apk signing step is delivery, not proof, and is not
pinned here; workflow signing changes are SECURITY.md review.

The guard is wired in the scope job itself, so it runs on every push,
and in host-build too, exactly once, which keeps the guard-of-guards
passing. A script deleted together with both of its steps passes by
design -- a deliberate retirement, visible as a file deletion, not a
silent drop.

This script's own logic has the same silent-drop shape one layer
further up: if its step parser or marker check regresses, no other
guard reads the APK jobs, so a broken apk-step guard passes the real
workflow while an APK proof step goes unverified -- proven 2026-11-05
by neutering its step-level `if:` detector and watching it PASS a
workflow with a conditional identity step. So its parser regressions
live in `_self_test()` below and run on every invocation,
`--self-test` or not, the same pattern as the matrix and guard-step
guards: a logic regression fails its own run before it can pass a
real workflow.

Usage:
    python3 .github/scripts/check_apk_step_coverage.py \
        --workflow .github/workflows/wo000-delivery-spine.yml

    python3 .github/scripts/check_apk_step_coverage.py --self-test
"""

import argparse
import re
import sys
from pathlib import Path

JOB_HEADER = re.compile(r"^  ([A-Za-z0-9_-]+):\s*$")
STEP_HEADER = re.compile(r"^      - name: (.*)$")
FIELD_RUN = re.compile(r"^        run:", re.MULTILINE)
FIELD_IF = re.compile(r"^        if:", re.MULTILINE)

# (job, step name, marker that must appear in the step's run block, what it is)
EXPECTED = [
    ("android-apk", "Export checkpoint Android arm64 APK",
     'grep -F "lib/arm64-v8a/libscraperx_native.so"',
     "fresh native library inside the exported APK"),
    ("android-apk", "Verify checkpoint Android APK identity",
     ".github/scripts/check_apk_identity.sh",
     "APK identity read back from the checkpoint APK"),
    ("android-apk", "Verify exported story-radio package",
     "ci/verify_story_radio_package.py",
     "packaged story-radio recordings checked against the manifest"),
    ("playtest-apk", "Stage playtest APK",
     'grep -F "lib/arm64-v8a/libscraperx_native.so"',
     "fresh native library inside the staged APK"),
    ("playtest-apk", "Verify playtest Android APK identity",
     ".github/scripts/check_apk_identity.sh",
     "APK identity read back from the playtest APK"),
]


def job_block(workflow_text, job):
    """Return the job's lines, or None if the job is missing."""
    lines = workflow_text.splitlines()
    start = None
    for i, line in enumerate(lines):
        if line == f"  {job}:":
            start = i
            break
    if start is None:
        return None
    block = []
    for line in lines[start + 1:]:
        if JOB_HEADER.match(line):
            break
        block.append(line)
    return block


def step_blocks(block):
    """Split a job block into (step name, text) pairs."""
    steps = []
    current = None
    for line in block:
        match = STEP_HEADER.match(line)
        if match:
            current = (match.group(1).strip(), [])
            steps.append(current)
        elif current is not None:
            current[1].append(line)
    return [(name, "\n".join(text)) for name, text in steps]


def check_text(workflow_text):
    """Check workflow text against EXPECTED.

    Returns (problems, checked): problems is the list of failure
    strings (empty when the workflow is clean), checked is the count
    of proof steps found exactly once. The caller prints them; this
    function does no I/O, so the self-test can run it on fixture text.
    """
    problems = []
    checked = 0
    for job, name, marker, what in EXPECTED:
        block = job_block(workflow_text, job)
        if block is None:
            problems.append(f"no job {job!r} in workflow")
            continue
        found = [(text) for (step, text) in step_blocks(block) if step == name]
        if not found:
            problems.append(f"{job}: no step named {name!r} ({what})")
            continue
        if len(found) > 1:
            problems.append(f"{job}: step {name!r} appears {len(found)} times")
            continue
        checked += 1
        text = found[0]
        if not FIELD_RUN.search(text):
            problems.append(f"{job}: step {name!r} has no run: block")
        if FIELD_IF.search(text):
            problems.append(f"{job}: step {name!r} has a step-level if: ({what})")
        if marker not in text:
            problems.append(f"{job}: step {name!r} lacks {marker!r} ({what})")
    return problems, checked


def _self_test():
    """Regression checks for the apk-step guard parser, run on every invocation."""
    so_grep = 'grep -F "lib/arm64-v8a/libscraperx_native.so"'
    identity = ".github/scripts/check_apk_identity.sh"
    radio = "ci/verify_story_radio_package.py"

    def step(name, body):
        return (f"      - name: {name}\n"
                "        shell: bash\n"
                f"{body}")

    def run_block(*lines):
        return "        run: |\n" + "".join(f"          {line}\n" for line in lines)

    export = step("Export checkpoint Android arm64 APK", run_block(so_grep))
    apk_identity = step("Verify checkpoint Android APK identity", run_block(identity))
    story_radio = step("Verify exported story-radio package", run_block(radio))
    stage = step("Stage playtest APK", run_block(so_grep))
    playtest_identity = step("Verify playtest Android APK identity", run_block(identity))

    valid = ("name: test\njobs:\n"
             "  android-apk:\n    steps:\n"
             + export + apk_identity + story_radio
             + "  playtest-apk:\n    steps:\n"
             + stage + playtest_identity)
    cases = []

    def case(name, text, want):
        problems, _checked = check_text(text)
        joined = "\n".join(problems)
        if want is None:
            if problems:
                raise AssertionError(f"{name}: expected clean, got {problems}")
        elif want not in joined:
            raise AssertionError(f"{name}: expected {want!r} in {problems}")
        cases.append(name)

    case("valid", valid, None)
    case("dropped-step",
         valid.replace(apk_identity, "", 1),
         "android-apk: no step named 'Verify checkpoint Android APK identity'")
    case("duplicated-step",
         valid.replace(export, export + export, 1),
         "android-apk: step 'Export checkpoint Android arm64 APK' appears 2 times")
    case("conditional-step",
         valid.replace("      - name: Verify checkpoint Android APK identity\n",
                       "      - name: Verify checkpoint Android APK identity\n"
                       "        if: github.event_name == 'push'\n", 1),
         "android-apk: step 'Verify checkpoint Android APK identity' has a step-level if:")
    case("missing-marker",
         valid.replace(so_grep, "echo apk", 1),
         f"android-apk: step 'Export checkpoint Android arm64 APK' lacks {so_grep!r}")
    case("missing-marker-playtest",
         valid.replace(stage, step("Stage playtest APK", run_block("echo apk")), 1),
         f"playtest-apk: step 'Stage playtest APK' lacks {so_grep!r}")
    case("no-run-block",
         valid.replace(run_block(identity), "        env:\n          APK: app.apk\n", 1),
         "android-apk: step 'Verify checkpoint Android APK identity' has no run: block")
    case("missing-job",
         valid.replace("  playtest-apk:\n    steps:\n" + stage + playtest_identity, "", 1),
         "no job 'playtest-apk' in workflow")
    return len(cases)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workflow",
                        default=".github/workflows/wo000-delivery-spine.yml")
    parser.add_argument("--self-test", action="store_true",
                        help="run parser regressions only")
    args = parser.parse_args()

    fixture_count = _self_test()
    if args.self_test:
        print(f"PASS: {fixture_count} apk-step coverage parser regression checks")
        return 0

    workflow_text = Path(args.workflow).read_text()
    problems, checked = check_text(workflow_text)
    if problems:
        print("FAIL: APK proof step gaps:")
        for problem in problems:
            print(f"  {problem}")
        return 1
    print(f"PASS: {checked} APK proof steps present in android-apk and playtest-apk, "
          "each with its proof assertion")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
