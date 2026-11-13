#!/usr/bin/env python3
"""The integrator's stall sweep: what is waiting, and on whom.

Lists every open PR and every agent branch pushed in the last day, and says
the one next action each needs so nothing sits waiting:

  MERGE     proof green and a second agent's review LAND on the PR's head (an eye LAND alone is not one)
  REVIEW    proof green, no review LAND and no FIX yet
  FIX-CI    the proof failed on the head (its owner fixes it)
  FIX       any reviewer's (or the eye's) newest verdict on the head is FIX or BLOCK, or a Codex P0/P1 finding has
            no reply
  PROVING   the proof is running
  DISPATCH  pushed, but no proof ever ran on the head
  QUIET     nothing changed for --quiet-hours

Green means every proof job green. Off trunk, `publish-checkpoint` does not
count (it is skipped there, or on older heads stops at the signing step).

Reads GitHub through `gh api` and the branches through `git`; changes nothing.
Usage: python3 tools/board/sweep.py [--repo IssisX/ScraperX] [--quiet-hours 3]
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import re
import subprocess
import sys

WORKFLOW = "wo000-delivery-spine.yml"
TRUNK = "ScraperX-Claude"
NOT_COUNTED = {"publish-checkpoint", "playtest-apk", "wake-switchboard"}
WAKE_JOB = "wake-switchboard"  # the proof's last job, which dispatches the switchboard


def only_wake_left(jobs: list) -> bool:
    """True when the proof's one unfinished job is the one waking the switchboard (as switch.py)."""
    return any(j["name"] == WAKE_JOB for j in jobs) and all(
        j["status"] == "completed" for j in jobs if j["name"] != WAKE_JOB)
VERDICT = re.compile(r"^\*\*[^\n]*?:\s*\**\s*(LAND|FIX|BLOCK)\b", re.MULTILINE)
AGENT_PREFIX = re.compile(r"^(claude|chatgpt|gemini|grok|fb-bot|copilot|codex|pe|work)/")
MAX_PAGES = 5  # bounded pagination: at most 500 comments read per PR


def gh(path: str) -> object:
    """GET one GitHub API path; raises with the path on failure."""
    out = subprocess.run(["gh", "api", path], capture_output=True, text=True, timeout=60)
    if out.returncode != 0:
        raise RuntimeError(f"gh api {path} failed ({out.returncode}): {out.stderr.strip()[:300]}")
    return json.loads(out.stdout)


def parse_time(stamp: str) -> dt.datetime:
    return dt.datetime.fromisoformat(stamp.replace("Z", "+00:00"))


def proof_state(repo: str, sha: str) -> tuple[str, str]:
    """The newest proof run on this commit: (state, run url)."""
    runs = gh(f"repos/{repo}/actions/workflows/{WORKFLOW}/runs?head_sha={sha}&per_page=5")["workflow_runs"]
    if not runs:
        return "none", ""
    run = max(runs, key=lambda r: r["created_at"])
    jobs = gh(f"repos/{repo}/actions/runs/{run['id']}/jobs?per_page=50")["jobs"]
    if run["status"] != "completed" and not only_wake_left(jobs):
        return "running", run["html_url"]
    failed = [
        j["name"]
        for j in jobs
        if j["name"] not in NOT_COUNTED and j["conclusion"] not in ("success", "skipped")
    ]
    return ("red: " + ", ".join(failed) if failed else "green"), run["html_url"]


HEADER = re.compile(r"^\*\*(?P<who>[^(\n*]+?)\s*(?:\((?P<what>[^)\n]*)\))?\s*:\s*\**\s*(?:LAND|FIX|BLOCK)\b")
NAMED_HEAD = re.compile(r"@\s*([0-9a-f]{7,40})\b")


def verdict_of(comments: list[dict], head_sha: str, head_time: dt.datetime) -> str:
    """The PR's verdict on its head: FIX while any reviewer's newest LAND/FIX/BLOCK on this head is a FIX or BLOCK
    (each reviewer, and the eye apart from the review, counts on its own: 11-13, #494 read MERGE on an eye LAND that
    came after the API review's FIX 6840871637, still unfixed); else LAND when a review (not the eye) LANDed; else
    ''. A verdict counts on this head when it names it (`@ <sha>`), or names none and came after the head commit."""
    newest: dict[tuple[str, bool], tuple[dt.datetime, str]] = {}
    for c in comments:
        top = (c.get("body") or "")[:400]
        m = VERDICT.search(top)
        if not m:
            continue
        line = top[m.start():].split("\n", 1)[0]
        h = HEADER.match(line)
        who, what = (h.group("who").strip().lower(), (h.group("what") or "").lower()) if h else ("?", "")
        named = NAMED_HEAD.search(what)
        t = parse_time(c["created_at"])
        if (named and not head_sha.startswith(named.group(1))) or (not named and t < head_time):
            continue
        key = (who, "eye" in who or "eye" in what)
        if key not in newest or t >= newest[key][0]:
            newest[key] = (t, "FIX" if m.group(1) == "BLOCK" else m.group(1))
    if any(v == "FIX" for _, v in newest.values()):
        return "FIX"
    return "LAND" if any(v == "LAND" and not eye for (_, eye), (_, v) in newest.items()) else ""


def latest_verdict(repo: str, number: int, head_sha: str, head_time: dt.datetime) -> str:
    """verdict_of the PR's comments (at most MAX_PAGES pages)."""
    comments: list[dict] = []
    for page in range(1, MAX_PAGES + 1):
        batch = gh(f"repos/{repo}/issues/{number}/comments?per_page=100&page={page}")
        comments += batch
        if len(batch) < 100:
            break
    return verdict_of(comments, head_sha, head_time)


CODEX_SERIOUS = re.compile(r"!\[P[01] Badge\]")


def open_codex_findings(repo: str, number: int) -> int:
    """Codex P0/P1 inline findings with no reply yet (11-05: #258 sat an hour as MERGE on one)."""
    comments = []
    for page in range(1, MAX_PAGES + 1):
        batch = gh(f"repos/{repo}/pulls/{number}/comments?per_page=100&page={page}")
        comments += batch
        if len(batch) < 100:
            break
    answered = {c["in_reply_to_id"] for c in comments if c.get("in_reply_to_id")}
    return sum(
        1
        for c in comments
        if "codex" in c["user"]["login"].lower()
        and not c.get("in_reply_to_id")
        and CODEX_SERIOUS.search(c["body"][:300])
        and c["id"] not in answered
    )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--repo", default="IssisX/ScraperX")
    ap.add_argument("--quiet-hours", type=float, default=3.0)
    args = ap.parse_args()
    now = dt.datetime.now(dt.timezone.utc)
    rows: list[tuple[str, str]] = []

    prs = gh(f"repos/{args.repo}/pulls?state=open&per_page=100")  # stacked PRs included
    pr_heads = set()
    for pr in prs:
        sha = pr["head"]["sha"]
        pr_heads.add(pr["head"]["ref"])
        commit = gh(f"repos/{args.repo}/commits/{sha}")
        head_time = parse_time(commit["commit"]["committer"]["date"])
        state, url = proof_state(args.repo, sha)
        verdict = latest_verdict(args.repo, pr["number"], sha, head_time)
        if open_codex_findings(args.repo, pr["number"]):
            verdict = "FIX"
        quiet = (now - parse_time(pr["updated_at"])).total_seconds() / 3600
        if state.startswith("red"):
            action = "FIX-CI"
        elif verdict == "FIX":
            action = "FIX"
        elif state == "running":
            action = "PROVING"
        elif state == "none":
            action = "DISPATCH"
        elif verdict == "LAND":
            action = "MERGE"
        else:
            action = "REVIEW"
        if action in ("FIX", "FIX-CI", "DISPATCH") and quiet >= args.quiet_hours:
            action += "+QUIET"
        draft = " draft" if pr["draft"] else ""
        rows.append((action, f"#{pr['number']}{draft} {pr['head']['ref']} @{sha[:8]} proof={state} "
                             f"verdict={verdict or '-'} idle={quiet:.1f}h {url}"))

    subprocess.run(["git", "fetch", "-q", "origin"], check=False, timeout=120)
    refs = subprocess.run(
        ["git", "for-each-ref", "--sort=-committerdate", "--format=%(refname:strip=3) %(objectname) %(committerdate:iso-strict)",
         "refs/remotes/origin"], capture_output=True, text=True, check=True).stdout.splitlines()
    for line in refs:
        name, sha, stamp = line.split(" ", 2)
        age = (now - dt.datetime.fromisoformat(stamp)).total_seconds() / 3600
        if age > 24:
            break
        if not AGENT_PREFIX.match(name) or name in pr_heads:
            continue
        state, url = proof_state(args.repo, sha)
        if state == "none" and "/integrate-" not in name:
            rows.append(("DISPATCH?", f"{name} @{sha[:8]} pushed {age:.1f}h ago, no proof on its head"))
        elif state.startswith("red"):
            rows.append(("FIX-CI", f"{name} @{sha[:8]} proof={state} {url}"))

    order = ["MERGE", "REVIEW", "FIX-CI", "FIX", "DISPATCH", "DISPATCH?", "PROVING"]
    rows.sort(key=lambda r: next((i for i, o in enumerate(order) if r[0].startswith(o)), len(order)))
    for action, text in rows:
        print(f"{action:<15} {text}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, subprocess.SubprocessError, KeyError, ValueError) as err:
        print(f"sweep: {err}", file=sys.stderr)
        sys.exit(2)
