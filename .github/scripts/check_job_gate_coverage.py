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

Parser regressions live in `_self_test()` below and run on every
invocation, `--self-test` or not, the same pattern as the other
coverage guards: a regression in the gate parsing must fail the
self-test, not pass a broken workflow.

Usage:
    python3 .github/scripts/check_job_gate_coverage.py \
        --workflow .github/workflows/wo000-delivery-spine.yml
    python3 .github/scripts/check_job_gate_coverage.py --self-test
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


def check_job_gates(workflow_text):
    """Check workflow text for job-gate problems.

    Returns the list of failure strings (empty when the workflow
    is clean). The caller prints them; this function does no I/O,
    so the self-test can run it on fixture text.
    """
    jobs = job_blocks(workflow_text)
    problems = []

    if "scope" not in jobs:
        return ["no scope job in workflow"]
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

    return problems


def _self_test():
    """Regression checks for the job-gate parser, run on every invocation."""
    def scope(outputs=None):
        if outputs is None:
            outputs = OUTPUTS
        lines = ("  scope:\n"
                 "    runs-on: ubuntu-24.04\n"
                 "    outputs:\n")
        for name in outputs:
            lines += f"      {name}: ${{{{ steps.scope.outputs.{name} }}}}\n"
        return lines + ("    steps:\n"
                        "      - name: Decide\n"
                        "        run: echo ok\n")

    def gated(name, group, needs="[scope, host-build]"):
        return (f"  {name}:\n"
                f"    needs: {needs}\n"
                f"    if: needs.scope.outputs.{group} == 'true'\n"
                "    runs-on: ubuntu-24.04\n"
                "    steps:\n"
                "      - name: Run\n"
                "        run: echo ok\n")

    def transitive(name, gate=None, needs="[host-build, native-tests, godot-touch, godot-audits-and-mix, android-build, android-apk]"):
        text = (f"  {name}:\n"
                f"    needs: {needs}\n")
        if gate is not None:
            text += f"    if: {gate}\n"
        return text + ("    runs-on: ubuntu-24.04\n"
                       "    steps:\n"
                       "      - name: Run\n"
                       "        run: echo ok\n")

    valid = ("name: test\njobs:\n"
             + scope()
             + gated("host-build", "game", "scope")
             + gated("native-tests", "native")
             + gated("godot-touch", "godot")
             + gated("godot-audits-and-mix", "godot")
             + gated("android-build", "android", "scope")
             + transitive("android-apk", None, "[host-build, android-build]")
             + transitive("publish-checkpoint",
                          "github.ref == 'refs/heads/ScraperX-Claude' && needs.android-apk.result == 'success'")
             + transitive("playtest-apk",
                          "github.event_name == 'workflow_dispatch' && github.ref == 'refs/heads/ScraperX-Claude'"))
    cases = []

    def case(name, text, want):
        problems = check_job_gates(text)
        joined = "\n".join(problems)
        if want is None:
            if problems:
                raise AssertionError(f"{name}: expected clean, got {problems}")
        elif want not in joined:
            raise AssertionError(f"{name}: expected {want!r} in {problems}")
        cases.append(name)

    case("valid", valid, None)
    case("folded-gate",
         valid.replace("    if: needs.scope.outputs.game == 'true'\n",
                       "    if: >-\n      needs.scope.outputs.game == 'true'\n", 1),
         None)
    case("no-scope-job", valid.replace(scope(), "", 1),
         "no scope job in workflow")
    case("missing-outputs",
         valid.replace("    outputs:\n"
                       "      game: ${{ steps.scope.outputs.game }}\n"
                       "      native: ${{ steps.scope.outputs.native }}\n"
                       "      godot: ${{ steps.scope.outputs.godot }}\n"
                       "      android: ${{ steps.scope.outputs.android }}\n", "", 1),
         "missing: scope job declares no outputs:")
    case("wrong-outputs",
         valid.replace("      android: ${{ steps.scope.outputs.android }}\n", "", 1),
         "wrong-outputs: scope job declares ['game', 'godot', 'native'], "
         "expected ['android', 'game', 'godot', 'native']")
    case("duplicated-output",
         valid.replace("      game: ${{ steps.scope.outputs.game }}\n",
                       "      game: ${{ steps.scope.outputs.game }}\n"
                       "      game: ${{ steps.scope.outputs.game }}\n", 1),
         "duplicated: scope output game declared twice")
    case("wrong-source",
         valid.replace("      game: ${{ steps.scope.outputs.game }}\n",
                       "      game: ${{ steps.other.outputs.game }}\n", 1),
         "wrong-source: scope output game is wired to '${{ steps.other.outputs.game }}', "
         "expected '${{ steps.scope.outputs.game }}'")
    case("wrong-group-gate",
         valid.replace("    if: needs.scope.outputs.native == 'true'\n",
                       "    if: needs.scope.outputs.godot == 'true'\n", 1),
         "wrong-gate: native-tests gates on \"needs.scope.outputs.godot == 'true'\", "
         "expected \"needs.scope.outputs.native == 'true'\"")
    case("missing-gate",
         valid.replace("    if: needs.scope.outputs.game == 'true'\n", "", 1),
         "missing-gate: host-build has no job-level if:")
    case("wrong-needs",
         valid.replace("  native-tests:\n    needs: [scope, host-build]\n",
                       "  native-tests:\n    needs: [scope]\n", 1),
         "wrong-needs: native-tests needs ['scope'], expected ['host-build', 'scope']")
    case("unknown-output-ref",
         valid.replace("needs.android-apk.result == 'success'",
                       "needs.android-apk.result == 'success' && needs.scope.outputs.nativ == 'true'", 1),
         "unknown-output: publish-checkpoint gate references needs.scope.outputs.nativ, "
         "which the scope job does not declare")
    case("duplicated-needs",
         valid.replace("  host-build:\n    needs: scope\n",
                       "  host-build:\n    needs: scope\n    needs: scope\n", 1),
         "duplicated: host-build declares needs: twice")
    case("transitive-wrong-needs",
         valid.replace("  android-apk:\n    needs: [host-build, android-build]\n",
                       "  android-apk:\n    needs: [android-build]\n", 1),
         "wrong-needs: android-apk needs ['android-build'], expected ['android-build', 'host-build']")
    case("apk-has-gate",
         valid.replace("  android-apk:\n    needs: [host-build, android-build]\n",
                       "  android-apk:\n    needs: [host-build, android-build]\n"
                       "    if: needs.scope.outputs.android == 'true'\n", 1),
         "unexpected-gate: android-apk has a job-level if:; it is gated only by needing android-build")
    case("publish-missing-marker",
         valid.replace("github.ref == 'refs/heads/ScraperX-Claude' && needs.android-apk.result == 'success'",
                       "github.ref == 'refs/heads/ScraperX-Claude'", 1),
         "wrong-gate: publish-checkpoint gate is missing 'needs.android-apk.result'")
    case("playtest-missing-marker",
         valid.replace("github.event_name == 'workflow_dispatch' && github.ref == 'refs/heads/ScraperX-Claude'",
                       "github.ref == 'refs/heads/ScraperX-Claude'", 1),
         "wrong-gate: playtest-apk gate is missing 'workflow_dispatch'")
    case("missing-job", valid.replace(gated("godot-touch", "godot"), "", 1),
         "missing: no godot-touch job in workflow")
    return len(cases)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--workflow",
                        default=".github/workflows/wo000-delivery-spine.yml")
    parser.add_argument("--self-test", action="store_true",
                        help="run parser regressions only")
    args = parser.parse_args()

    fixture_count = _self_test()
    if args.self_test:
        print(f"PASS: {fixture_count} job-gate coverage parser regression checks")
        return 0

    workflow_text = Path(args.workflow).read_text()
    problems = check_job_gates(workflow_text)
    if problems:
        for problem in problems:
            print(f"FAIL: {problem}")
        return 1
    print("PASS: scope declares game/native/godot/android and every proof job "
          "gates on its own scope output (transitive needs gate the rest)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
