#!/usr/bin/env python3
"""Which test groups can this push reach? Decides which of the proof's build and test jobs run.

Cory, 11-04: running full game builds and tests "when there's only been small changes ... is so extremely
wasteful". Each changed file is mapped to the groups of the proof that read it; a group no changed file
reaches is skipped, and a skipped job counts as passing for the switchboard and sweep.py. Any doubt (an
unknown path, a dispatch, a diff that cannot be listed) runs the whole proof.

The groups (checked 11-04 against CMakeLists.txt, the test sources and the proof workflow):
  native   native-tests, the four CTest shards: scraperx_sim_tests, shared_survival and parkour_flow. They
           compile src/sim, src/bridge/fabricated_delivery.* (scraperx_delivery), tests/, the Jolt patch and
           tools/fab/dev_public_key.hex, and read godot/tests/fabricated/ at run time (SCRAPERX_SOURCE_DIR).
  godot    godot-touch (both shards) and godot-audits-and-mix: the Godot project godot/ with the
           GDExtension, which compiles src/sim and all of src/bridge.
  android  android-build and android-apk: the same native graph cross-compiled, and godot/ exported.
host-build runs when any group does (it builds what all three need and checks the shard lists).

What reaches what (first match wins; see groups_of):
  nothing          the switchboard, the board tools, the agents' notes and rules (NO_GAME); the design and
                   execution docs (01_*/, 02_*/, 03_EXECUTION/) and any .md outside godot/; tools/ other than
                   tools/fab/ and tools/matron_probe/ (tools/rule_check and the switchboard's unit tests run in
                   the scope job on every push anyway); src/voice/ (the offline voice generators: what they
                   make is committed under godot/); the root's MANIFEST.txt, SHA256SUMS.txt, .gitignore, .mcp.json
  godot            tools/matron_probe/ (the godot-touch drawn-body gate records with probe.gd and gates on
                   check.py, and host-build's drawn-body guard reads check.py's EXPECTED keys; the probe
                   files are host-side tools, not packaged in the APK, so android does not run)
  every group      .github/ and ci/ (the proof itself), CMakeLists.txt, third_party/, tools/fab/, src/ other
                   than below, godot/tests/fabricated/, and any path not named here
  native           tests/ (built only into the native test binaries)
  godot, android   godot/ (scripts, scenes, audio, art: the native tests never read it), and the bridge's
                   Godot-only files src/bridge/register_types.* and src/bridge/scraperx_simulation.*

What is compared:
  push to ScraperX-Claude    the pushed range, BEFORE..HEAD (every commit of the push)
  push to an integrate branch the branch against trunk, merge-base(trunk, HEAD)..HEAD, so a docs-only push
                             on a branch that also changes game code still proves the game
  anything else (dispatch)   the whole proof

Writes `game=`, `native=`, `godot=` and `android=` (true|false) to $GITHUB_OUTPUT when it is set; `game` is
true when any group runs. Prints the decision and why.
"""
from __future__ import annotations

import os
import subprocess
import sys

TRUNK = "ScraperX-Claude"
ZERO_SHA = "0" * 40

GROUPS = ("native", "godot", "android")
EVERY = frozenset(GROUPS)
NONE: frozenset[str] = frozenset()
GODOT = frozenset({"godot", "android"})
NATIVE = frozenset({"native"})
PROBE = frozenset({"godot"})

# Read by no game build, test or audit of the proof (checked 11-04: no code under ci/, godot/, src/, tests/ or the
# game jobs reads them; the mentions there are comments). The switchboard's and the board tools' unit tests run in
# the proof's scope job on every push (J-072: switchboard.yml's selftest never starts on a PR its bot opens), not in
# a game job, so they need no group here.
NO_GAME_PREFIXES = (
    "tools/switchboard/",
    "tools/board/",
    "work/",
    ".claude/",
    "03_EXECUTION/handoffs/",
)
NO_GAME_FILES = frozenset({
    ".github/workflows/switchboard.yml",
    "CLAUDE.md",
    "AGENTS.md",
    "00_START_HERE.md",
    "00_START_HERE_ARCHIVE.md",
    "REPO_MAP.md",
    "ci/test_proof_scope.py",   # run by the scope job itself, on every push
})

# Docs and offline tools: nothing in CMakeLists.txt, src/, tests/, godot/ or the proof workflow reads them
# (checked 11-04 by grep for every path; godot reads only res://, which is godot/).
DOC_PREFIXES = (
    "01_PRODUCT_AUTHORITY/",
    "02_ENGINEERING_AUTHORITY/",
    "03_EXECUTION/",
    "src/voice/",
)
ROOT_NO_GAME_FILES = frozenset({"MANIFEST.txt", "SHA256SUMS.txt", ".gitignore", ".mcp.json"})

# The bridge files compiled only into the GDExtension (scraperx_native); fabricated_delivery.* is in
# scraperx_delivery, which the native tests link, so it stays in every group.
BRIDGE_GODOT_ONLY = frozenset({
    "src/bridge/register_types.cpp",
    "src/bridge/register_types.hpp",
    "src/bridge/scraperx_simulation.cpp",
    "src/bridge/scraperx_simulation.hpp",
})


def groups_of(path: str) -> frozenset[str]:
    """The proof's test groups that read this file; every group when in doubt."""
    if path in NO_GAME_FILES or path.startswith(NO_GAME_PREFIXES):
        return NONE
    if path.startswith((".github/", "ci/")):
        return EVERY
    if path.startswith(DOC_PREFIXES) or path in ROOT_NO_GAME_FILES:
        return NONE
    if path.endswith(".md") and not path.startswith("godot/"):
        return NONE
    # The drawn-body gate is a godot-touch step, not a local-only tool: a probe-only push
    # that skipped godot would leave check.py unexercised while CI stayed green.
    if path.startswith("tools/matron_probe/"):
        return PROBE
    if path.startswith("tools/"):
        return EVERY if path.startswith("tools/fab/") else NONE
    if path in BRIDGE_GODOT_ONLY:
        return GODOT
    if path.startswith("tests/"):
        return NATIVE
    if path.startswith("godot/tests/fabricated/"):
        return EVERY
    if path.startswith("godot/"):
        return GODOT
    return EVERY


def reaches_game(path: str) -> bool:
    """True unless the file is one the proof never reads."""
    return bool(groups_of(path))


def _shown(paths: list[str]) -> str:
    return ", ".join(paths[:6]) + (f" and {len(paths) - 6} more" if len(paths) > 6 else "")


def decide(event: str, ref: str, changed: list[str] | None) -> tuple[frozenset[str], str]:
    """(the groups to run, why). `changed` is None when the changed files could not be listed."""
    if event != "push":
        return EVERY, f"event {event!r}: the whole proof"
    if changed is None:
        return EVERY, "the changed files could not be listed: the whole proof"
    if not changed:
        return EVERY, "no changed files listed: the whole proof"
    run: set[str] = set()
    why = []
    for group in GROUPS:
        hits = [p for p in changed if group in groups_of(p)]
        if hits:
            run.add(group)
            why.append(f"{group} runs: {len(hits)} of {len(changed)} changed files reach it ({_shown(hits)})")
        else:
            why.append(f"{group} skipped: none of the {len(changed)} changed files reach it")
    if not run:
        return NONE, f"all {len(changed)} changed files are outside the game (ref {ref}): build and game tests skipped"
    return frozenset(run), "; ".join(why)


def _git(*args: str) -> str:
    out = subprocess.run(["git", *args], capture_output=True, text=True, encoding="utf-8")
    if out.returncode != 0:
        raise RuntimeError(f"git {' '.join(args)} failed ({out.returncode}): {out.stderr.strip()}")
    return out.stdout


def changed_files(ref: str, before: str) -> list[str] | None:
    """The files this push changes, as `decide` compares them; None when they cannot be listed."""
    # The checkout is shallow (depth 1), so each base is fetched; a failed fetch or merge-base means the whole proof.
    try:
        if ref == f"refs/heads/{TRUNK}":
            if not before or before == ZERO_SHA:
                return None
            _git("fetch", "--quiet", "--depth=1", "origin", before)
            base = before
        else:
            _git("fetch", "--quiet", "--depth=1", "origin", TRUNK)
            trunk = _git("rev-parse", "FETCH_HEAD").strip()
            try:
                _git("fetch", "--quiet", "--deepen=100", "origin")
            except RuntimeError as err:   # as the rule check: the merge-base below still decides
                print(f"proof_scope: {err}", file=sys.stderr)
            base = _git("merge-base", trunk, "HEAD").strip()
        names = _git("diff", "--no-renames", "--name-only", "-z", base, "HEAD")
    except RuntimeError as err:
        print(f"proof_scope: {err}", file=sys.stderr)
        return None
    return [n for n in names.split("\0") if n]


def outputs(run: frozenset[str]) -> dict[str, str]:
    """The $GITHUB_OUTPUT values the proof's jobs read."""
    flag = {True: "true", False: "false"}
    out = {"game": flag[bool(run)]}
    out.update({g: flag[g in run] for g in GROUPS})
    return out


def main() -> int:
    event = os.environ.get("EVENT", "")
    ref = os.environ.get("REF", "")
    changed = changed_files(ref, os.environ.get("BEFORE", "")) if event == "push" else None
    run, why = decide(event, ref, changed)
    values = outputs(run)
    print("proof_scope: " + " ".join(f"{k}={v}" for k, v in values.items()))
    print(f"proof_scope: {why}")
    out = os.environ.get("GITHUB_OUTPUT")
    if out:
        with open(out, "a", encoding="utf-8") as fh:
            fh.writelines(f"{k}={v}\n" for k, v in values.items())
    return 0


if __name__ == "__main__":
    sys.exit(main())
