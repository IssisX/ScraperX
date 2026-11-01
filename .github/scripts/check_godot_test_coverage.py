#!/usr/bin/env python3
"""Check that every godot test script runs in CI or is a known gap.

The godot-audits-and-mix job runs headless SceneTree scripts from
godot/tests/ by hand-written workflow steps. A script in neither the
workflow nor a known-gap entry never runs in CI, and CI green would
say nothing about it -- the same silent-drop hole the CTest and touch
shard guards closed for their jobs. This script is their counterpart
for godot tests: it reads the invoked scripts out of the workflow
file itself, so it checks the workflow as written, not a copy of its
steps, and compares them against the *.gd files in godot/tests/.

Scope is godot/tests/*.gd only. Scripts elsewhere (the solids export,
the floating audit, the touch driver) are invoked by their own steps
and are not double-counted here. Running a script twice would be
wasteful, not a coverage hole, so duplicates are not failed on.

KNOWN_GAPS names godot test scripts that are in no workflow step for
a tracked reason. Each prints as a WARNING and does not fail the
guard, so one known gap cannot hide a new one: any other script with
zero invocations still fails, and a KNOWN_GAPS entry that becomes
invoked fails as stale -- remove it only together with the workflow
step that now runs it, never on its own, or the next run fails the
script as uncovered.

Usage:
    python3 .github/scripts/check_godot_test_coverage.py \
        --workflow .github/workflows/wo000-delivery-spine.yml \
        --test-dir godot/tests
"""

import argparse
import re
import shlex
import sys
from pathlib import Path

# Godot test scripts in no workflow step for a tracked reason. Warning,
# not failure; stale the moment the script appears in the workflow.
KNOWN_GAPS = {
    "gore_shots_test.gd": (
        "studio deck, renders PNGs with a display; renders-only, not "
        "a CI gate"
    ),
    "hair_pose_probe.gd": (
        "studio measurement probe, writes a probe directory for the "
        "scorer; no pass/fail verdict, not a CI gate"
    ),
    "script_load_test.gd": (
        "invocation owed, not yet proven: runs every presentation "
        "script plus the native bridge, so its local proof needs a "
        "built native library, which was not complete at package "
        "time; add its audits step together with removing this entry"
    ),
}

TEST_PATH_RE = re.compile(r"res://tests/([A-Za-z0-9_]+\.gd)\Z")
RUN_BLOCK_RE = re.compile(r"^(\s*)run:\s*\|[-+]?\s*(?:#.*)?$")



def _quote_after_line(line, quote=None):
    """Track shell quote state across physical lines, ignoring comments/escapes."""
    escaped = False
    word_start = True
    for char in line:
        if escaped:
            escaped = False
            word_start = False
            continue
        if char == "\\" and quote != "'":
            escaped = True
            continue
        if quote:
            if char == quote:
                quote = None
            continue
        if char == "#" and word_start:
            break
        if char in ("'", '"'):
            quote = char
            word_start = False
        elif char.isspace() or char in "|&;()<>":
            word_start = True
        else:
            word_start = False
    return quote


def _safe_command_prefix(command):
    """Return a shell command before an unquoted pipe, or reject unsafe forms."""
    quote = None
    escaped = False
    word_start = True
    first_pipe = None
    for index, char in enumerate(command):
        if escaped:
            escaped = False
            word_start = False
            continue
        if char == "\\" and quote != "'":
            escaped = True
            continue
        if quote:
            if char == quote:
                quote = None
            continue
        if char == "#" and word_start:
            break
        if char in ("'", '"'):
            quote = char
            word_start = False
            continue
        if char == ";":
            return None
        if char == "|":
            if command[index:index + 2] == "||":
                return None
            if first_pipe is None:
                first_pipe = index
            word_start = True
            continue
        if char == "&":
            # >& and &> redirect streams; a bare & backgrounds the test.
            if index > 0 and command[index - 1] == ">" or command[index:index + 2] == "&>":
                word_start = True
                continue
            return None
        word_start = char.isspace() or char in "()<>"
    return None if quote else command[:first_pipe] if first_pipe is not None else command


def invoked_scripts(workflow_text):
    """Find Godot test commands in shell run blocks, not mentions or comments.

    Only a literal Godot executable in a run block counts. A workflow
    description, commented command, or echo of a test path does not execute
    that test. Backslash continuations are joined before shell tokenization.
    Unrecognized, conditional, masked, or data-only commands fail closed.
    """
    invoked = set()
    in_run = False
    block_indent = 0
    pending = ""
    heredoc_end = ""
    control_stack = []
    multiline_quote = None
    for line in workflow_text.splitlines():
        stripped = line.strip()
        indent = len(line) - len(line.lstrip(" "))
        if in_run and stripped and indent <= block_indent:
            in_run = False
            pending = ""
            heredoc_end = ""
            control_stack = []
            multiline_quote = None
        if not in_run:
            match = RUN_BLOCK_RE.match(line)
            if match:
                in_run = True
                block_indent = len(match.group(1))
            continue
        if heredoc_end:
            if stripped == heredoc_end:
                heredoc_end = ""
            continue
        if not stripped or (stripped.startswith("#") and not multiline_quote):
            continue
        # Quoted data can span physical lines; none of its lines is a command.
        was_quoted = multiline_quote is not None
        multiline_quote = _quote_after_line(stripped, multiline_quote)
        if was_quoted or multiline_quote:
            pending = ""
            continue
        # A conditional or loop body may never run. Do not count commands
        # inside shell control structures as unconditional test coverage.
        opener = re.match(r"(?:if|for|while|until|case)\b", stripped)
        closer = re.match(r"(?:fi|done|esac)\b", stripped)
        if opener:
            end = "fi" if stripped.startswith("if ") else "esac" if stripped.startswith("case ") else "done"
            if not re.search(r"\b" + end + r"\b", stripped):
                control_stack.append(end)
            continue
        if control_stack:
            if closer and stripped.split()[0] == control_stack[-1]:
                control_stack.pop()
            continue
        # Here-document contents are data even if they resemble commands.
        heredoc = re.search(r"<<-?\s*['\"]?([A-Za-z_][A-Za-z0-9_]*)['\"]?", stripped)
        if heredoc:
            heredoc_end = heredoc.group(1)
            continue
        if stripped.endswith("\\"):
            pending += stripped[:-1] + " "
            continue
        command = pending + stripped
        pending = ""
        # A fallback can turn a failing test into a successful CI step.
        # Conservatively reject compound commands rather than count them.
        command = _safe_command_prefix(command)
        if command is None or "--script" not in command or "res://tests/" not in command:
            continue
        try:
            tokens = shlex.split(command, comments=True, posix=True)
        except ValueError as exc:
            raise ValueError(f"cannot parse Godot test command {command!r}") from exc
        # Count only a Godot executable, not a path passed to echo or
        # another command. The shell scanner already isolated the pipeline head.
        i = 0
        while i < len(tokens):
            if tokens[i] == "env":
                i += 1
                while i < len(tokens) and re.fullmatch(
                    r"[A-Za-z_][A-Za-z0-9_]*=.*", tokens[i]
                ):
                    i += 1
            elif tokens[i] == "timeout":
                if i + 1 >= len(tokens) or not re.fullmatch(
                    r"\d+(?:\.\d+)?[smhd]?", tokens[i + 1]
                ):
                    break
                i += 2
            elif tokens[i] == "xvfb-run":
                i += 1
                while i < len(tokens) and tokens[i] in ("-a", "-s"):
                    i += 2 if tokens[i] == "-s" else 1
            else:
                break
        if i >= len(tokens) or tokens[i] != "tools/godot/godot":
            continue
        args = tokens[i + 1:]
        # Godot stops parsing engine flags after --; later tokens are game args.
        if "--" in args:
            args = args[:args.index("--")]
        for i, token in enumerate(args):
            if token == "--script" and i + 1 < len(args):
                match = TEST_PATH_RE.fullmatch(args[i + 1])
            elif token.startswith("--script="):
                match = TEST_PATH_RE.fullmatch(token[len("--script="):])
            else:
                continue
            if match:
                invoked.add(match.group(1))
    return invoked


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workflow", default=root / ".github/workflows/wo000-delivery-spine.yml")
    parser.add_argument("--test-dir", default=root / "godot/tests")
    args = parser.parse_args()

    workflow_text = Path(args.workflow).read_text()
    test_dir = Path(args.test_dir)
    if not test_dir.is_dir():
        raise SystemExit(f"check_godot_test_coverage: no test dir {test_dir}")
    files = sorted(p.name for p in test_dir.glob("*.gd"))
    if not files:
        raise SystemExit("check_godot_test_coverage: no .gd files in test dir")
    invoked = invoked_scripts(workflow_text)

    print(f"godot tests on disk: {len(files)}")
    print(f"godot tests invoked in workflow: {len(invoked)}")

    failures = []
    for name in sorted(invoked - set(files)):
        failures.append(f"invoked but no such test file: {name}")

    for name in files:
        if name in invoked:
            continue
        if name in KNOWN_GAPS:
            print(f"WARNING known gap, in no workflow step: {name} ({KNOWN_GAPS[name]})")
        else:
            failures.append(f"test file in no workflow step: {name}")

    stale = [n for n in sorted(KNOWN_GAPS) if n in invoked]
    for name in stale:
        failures.append(f"stale known gap, now invoked -- remove from KNOWN_GAPS: {name}")

    if failures:
        print("FAIL: godot test coverage gaps:")
        for failure in failures:
            print(f"  {failure}")
        return 1
    print(
        f"PASS: {len(invoked)} godot tests invoked in CI, "
        f"{len(KNOWN_GAPS) - len(stale)} known gap(s) warned"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
