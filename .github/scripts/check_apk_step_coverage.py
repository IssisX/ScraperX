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

Usage:
    python3 .github/scripts/check_apk_step_coverage.py \
        --workflow .github/workflows/wo000-delivery-spine.yml
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workflow",
                        default=".github/workflows/wo000-delivery-spine.yml")
    args = parser.parse_args()

    workflow_text = Path(args.workflow).read_text()
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
