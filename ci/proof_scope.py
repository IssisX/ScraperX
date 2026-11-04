#!/usr/bin/env python3
"""Does this push reach the game? Decides whether the proof's build and test jobs run.

Cory, 11-04: running full game builds and tests "when there's only been small changes ... is so extremely
wasteful". A push whose changed files all sit in NO_GAME (the switchboard, the board tools, the agents' notes
and rules) builds nothing and runs no game test: those jobs are skipped, and a skipped job counts as passing
for the switchboard and sweep.py. Anything else, or any doubt, runs the whole proof.

What is compared:
  push to ScraperX-Claude    the pushed range, BEFORE..HEAD (every commit of the push)
  push to an integrate branch the branch against trunk, merge-base(trunk, HEAD)..HEAD, so a docs-only push
                             on a branch that also changes game code still proves the game
  anything else (dispatch)   the whole proof

Writes `game=true|false` to $GITHUB_OUTPUT when it is set; prints the decision and why.
"""
from __future__ import annotations

import os
import subprocess
import sys

TRUNK = "ScraperX-Claude"
ZERO_SHA = "0" * 40

# Read by no build, test or audit of the proof (checked 11-04: no code under ci/, godot/, src/, tests/ or the
# proof workflow reads them; the mentions there are comments). The switchboard's own code is tested by
# switchboard.yml's selftest job, not by the game proof.
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
    "ci/test_proof_scope.py",
})


def reaches_game(path: str) -> bool:
    """True unless the file is one the proof never reads."""
    return not (path in NO_GAME_FILES or path.startswith(NO_GAME_PREFIXES))


def decide(event: str, ref: str, changed: list[str] | None) -> tuple[bool, str]:
    """(run the game proof?, why). `changed` is None when the changed files could not be listed."""
    if event != "push":
        return True, f"event {event!r}: the whole proof"
    if changed is None:
        return True, "the changed files could not be listed: the whole proof"
    if not changed:
        return True, "no changed files listed: the whole proof"
    game = [p for p in changed if reaches_game(p)]
    if game:
        shown = ", ".join(game[:10]) + (f" and {len(game) - 10} more" if len(game) > 10 else "")
        return True, f"{len(game)} of {len(changed)} changed files reach the game ({shown})"
    return False, f"all {len(changed)} changed files are outside the game (ref {ref}): build and game tests skipped"


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


def main() -> int:
    event = os.environ.get("EVENT", "")
    ref = os.environ.get("REF", "")
    changed = changed_files(ref, os.environ.get("BEFORE", "")) if event == "push" else None
    game, why = decide(event, ref, changed)
    print(f"proof_scope: game={'true' if game else 'false'}: {why}")
    out = os.environ.get("GITHUB_OUTPUT")
    if out:
        with open(out, "a", encoding="utf-8") as fh:
            fh.write(f"game={'true' if game else 'false'}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
