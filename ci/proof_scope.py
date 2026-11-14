#!/usr/bin/env python3
"""Which test groups can this push reach? Decides which of the proof's build and test jobs run.

Cory, 11-04: running full game builds and tests "when there's only been small changes ... is so extremely
wasteful". Each changed file is mapped to the groups of the proof that read it; a group no changed file
reaches is skipped, and a skipped job counts as passing for the switchboard and sweep.py. Any doubt (an
unknown path, a dispatch on trunk or any ref not named below, a diff that cannot be listed) runs the whole proof.

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
  (or a dispatch on one)     on a branch that also changes game code still proves the game
  dispatch on a switchboard  the switchboard's merge of a green PR head with trunk (switch.py merge_candidate):
  merge candidate            HEAD is that merge commit, and the range is merge-base(its two parents)..HEAD:
                             the PR's own change since its green proof's trunk, and trunk's since then, and
                             anything else in the merge's tree (11-13: every candidate ran all 13 jobs, even
                             when one group was reached)
  anything else (dispatch)   the whole proof (a dispatch on trunk too)

Writes `game=`, `native=`, `godot=` and `android=` (true|false) to $GITHUB_OUTPUT when it is set; `game` is
true when any group runs. Prints the decision and why.

One dispatch on trunk is scoped (J-072, 11-14: run 43596588412, the dispatched trunk proof of #551's merge, six
switchboard files, ran 15 jobs for 24 minutes; its push would have run no game group). After the switchboard merges
a PR with the workflow token no push run starts, so it dispatches the trunk proof (switch.py merge). That run is
read as the push of that merge, HEAD^1..HEAD, only when all of this holds: it was dispatched, and not re-run, by
github-actions[bot]; HEAD is a merge of two commits; and HEAD's author and committer are the ones GitHub writes on
a workflow-token merge. Nothing a PR or a dispatch can set is read (not the commit message: it carries the PR's
branch name). A person's dispatch on trunk, any other HEAD (a plain commit, a hand merge, a merge made with the
merge token, whose push has its own run) or a first parent that cannot be fetched runs the whole proof, which
playtest-apk and hand checks need.
"""
from __future__ import annotations

import os
import re
import subprocess
import sys

TRUNK = "ScraperX-Claude"
CANDIDATE_REF = "refs/heads/switchboard/candidate-"   # tools/switchboard/switch.py CANDIDATE_PREFIX
INTEGRATE_REF = re.compile(r"^refs/heads/[A-Za-z0-9._-]+/integrate-[A-Za-z0-9._/-]+$")
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


def scoped(event: str, ref: str) -> bool:
    """Is this run's scope read from its changed files? A push; a dispatch only on a merge candidate or an
    integrate branch (a dispatch on trunk, or on any other ref, runs the whole proof)."""
    if event == "push":
        return True
    return event == "workflow_dispatch" and (ref.startswith(CANDIDATE_REF) or bool(INTEGRATE_REF.match(ref)))


def decide(event: str, ref: str, changed: list[str] | None) -> tuple[frozenset[str], str]:
    """(the groups to run, why). `changed` is None when the changed files could not be listed."""
    if not scoped(event, ref):
        return EVERY, f"event {event!r} on {ref!r}: the whole proof"
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
        if ref.startswith(CANDIDATE_REF):
            parents = merge_parents()
            _git("fetch", "--quiet", "--depth=100", "origin", *parents)
            base = _git("merge-base", *parents).strip()
        elif ref == f"refs/heads/{TRUNK}":
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


def merge_parents() -> list[str]:
    """HEAD's two parents, read from the commit object itself (a shallow checkout shows HEAD with none). Anything
    but a two-parent merge raises: the whole proof runs."""
    head = _git("cat-file", "-p", "HEAD").split("\n\n", 1)[0]
    parents = [line.split()[1] for line in head.splitlines() if line.startswith("parent ")]
    if len(parents) != 2 or not all(re.fullmatch(r"[0-9a-f]{40}", p) for p in parents):
        raise RuntimeError(f"HEAD is not a merge of two commits ({len(parents)} parent(s))")
    return parents


def outputs(run: frozenset[str]) -> dict[str, str]:
    """The $GITHUB_OUTPUT values the proof's jobs read."""
    flag = {True: "true", False: "false"}
    out = {"game": flag[bool(run)]}
    out.update({g: flag[g in run] for g in GROUPS})
    return out


# The trunk proof the switchboard dispatches after a workflow-token merge (tools/switchboard/switch.py merge): the
# actor of that dispatch, and the author and committer GitHub writes on that merge commit (read on 55ae0ca1, the
# merge of #551, and 12eef769, of #545). A merge made with the merge token is authored by that token's login.
WORKFLOW_TOKEN_ACTOR = "github-actions[bot]"
WORKFLOW_TOKEN_AUTHOR = "github-actions[bot] <41898282+github-actions[bot]@users.noreply.github.com>"
API_MERGE_COMMITTER = "GitHub <noreply@github.com>"


def switchboard_merge_base(event: str, ref: str, actor: str, triggering_actor: str) -> str:
    """HEAD's first parent, the trunk the merge landed on, when this run is the trunk proof the switchboard dispatches
    after it merges a PR with the workflow token; "" for every other run, and on any doubt. The actors are the
    runner's own GITHUB_ACTOR and GITHUB_TRIGGERING_ACTOR (a person's re-run of the bot's run changes the second).
    The parents, author and committer are read from the commit object itself (a shallow checkout shows HEAD with no
    parents); its message is never read."""
    if event != "workflow_dispatch" or ref != f"refs/heads/{TRUNK}":
        return ""
    if actor != WORKFLOW_TOKEN_ACTOR or triggering_actor != WORKFLOW_TOKEN_ACTOR:
        return ""
    try:
        header = _git("cat-file", "commit", "HEAD").split("\n\n", 1)[0].splitlines()
    except (RuntimeError, ValueError) as err:   # ValueError: a commit that is not UTF-8
        print(f"proof_scope: {err}", file=sys.stderr)
        return ""

    def field(name: str) -> list[str]:
        return [line[len(name) + 1:] for line in header if line.startswith(name + " ")]

    parents, author, committer = field("parent"), field("author"), field("committer")
    if len(parents) != 2 or not all(re.fullmatch(r"[0-9a-f]{40}", p) for p in parents):
        return ""
    if len(author) != 1 or not author[0].startswith(WORKFLOW_TOKEN_AUTHOR + " "):
        return ""
    if len(committer) != 1 or not committer[0].startswith(API_MERGE_COMMITTER + " "):
        return ""
    return parents[0]


def main() -> int:
    event = os.environ.get("EVENT", "")
    ref = os.environ.get("REF", "")
    before = os.environ.get("BEFORE", "")
    actor, triggering_actor = os.environ.get("GITHUB_ACTOR", ""), os.environ.get("GITHUB_TRIGGERING_ACTOR", "")
    print(f"proof_scope: {event} on {ref}, actor {actor!r}, run started by {triggering_actor!r}")
    merged_onto = switchboard_merge_base(event, ref, actor, triggering_actor)
    if merged_onto:
        print(f"proof_scope: a switchboard merge, dispatched on trunk by the workflow token: scoped as its push, "
              f"{merged_onto}..HEAD")
        event, before = "push", merged_onto
    changed = changed_files(ref, before) if scoped(event, ref) else None
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
