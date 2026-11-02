#!/usr/bin/env python3
"""The integrator's stall sweep: what is waiting, and on whom.

Lists every open PR and every agent branch pushed in the last day, and says
the one next action each needs so nothing sits waiting:

  MERGE     proof green and a second agent's LAND on the PR's head
  REVIEW    proof green, no LAND or FIX yet
  FIX-CI    the proof failed on the head (its owner fixes it)
  FIX       a reviewer's FIX is the latest verdict
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
NOT_COUNTED = {"publish-checkpoint", "playtest-apk"}
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
    if run["status"] != "completed":
        return "running", run["html_url"]
    jobs = gh(f"repos/{repo}/actions/runs/{run['id']}/jobs?per_page=50")["jobs"]
    failed = [
        j["name"]
        for j in jobs
        if j["name"] not in NOT_COUNTED and j["conclusion"] not in ("success", "skipped")
    ]
    return ("red: " + ", ".join(failed) if failed else "green"), run["html_url"]


def latest_verdict(repo: str, number: int, head_time: dt.datetime) -> str:
    """The newest LAND/FIX (a BLOCK counts as FIX) posted after the head commit, else ''."""
    verdict, when = "", None
    for page in range(1, MAX_PAGES + 1):
        comments = gh(f"repos/{repo}/issues/{number}/comments?per_page=100&page={page}")
        for c in comments:
            m = VERDICT.search(c["body"][:400])
            t = parse_time(c["created_at"])
            if m and t >= head_time and (when is None or t > when):
                verdict, when = ("FIX" if m.group(1) == "BLOCK" else m.group(1)), t
        if len(comments) < 100:
            break
    return verdict


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
        verdict = latest_verdict(args.repo, pr["number"], head_time)
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
