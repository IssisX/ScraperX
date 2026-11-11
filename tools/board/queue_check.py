#!/usr/bin/env python3
"""Check every job in an agent's queue file against GitHub before the queue is mailed (Cory 11-11).

A queue line names a job as `J-NNN #N`. For each one it reports:
  DONE      the issue is closed, or a merged PR's body starts `Job: J-NNN`
  OPEN-PR   an open PR's body starts `Job: J-NNN` (work exists; not unstarted)
  BRANCH    a remote branch names the job (`j0NN`) and moved in the last 72 h (work exists)
  OK        none of the above: the job is really unstarted
and, for every pair of OK jobs, nothing; duplicates are a judgment, not a string match.

Usage: python3 tools/board/queue_check.py [work/tasks/muse.md]
Exit 1 when any line in the "Unstarted" block is not OK, so a stale queue cannot be mailed unseen.
Reads only (REST `gh api`); writes nothing.
"""
from __future__ import annotations

import datetime as dt
import json
import re
import subprocess
import sys

REPO = "IssisX/ScraperX"
JOB_LINE = re.compile(r"\bJ-(\d{3})[a-z]?\s+#(\d+)")
RECENT_H = 72


def gh(path: str) -> object:
    out = subprocess.run(["gh", "api", path], check=True, capture_output=True, text=True, timeout=60).stdout
    return json.loads(out)


def job_prs(job: str) -> tuple[bool, bool]:
    """(merged, open) for PRs whose body starts `Job: J-NNN`."""
    q = f'repo:{REPO} is:pr "Job: {job}"'
    res = subprocess.run(["gh", "api", "-X", "GET", "search/issues", "-f", f"q={q}", "-f", "per_page=50"],
                         check=True, capture_output=True, text=True, timeout=60).stdout
    merged = opened = False
    for item in json.loads(res).get("items", []):
        body = (item.get("body") or "").lstrip()
        if not re.match(rf"Job:\s*{re.escape(job)}\b", body):
            continue
        if item.get("state") == "open":
            opened = True
        elif (item.get("pull_request") or {}).get("merged_at"):
            merged = True
    return merged, opened


def recent_branches(num: str) -> list[str]:
    refs = subprocess.run(["git", "ls-remote", "--heads", "origin"], check=True, capture_output=True,
                          text=True, timeout=60).stdout.split("\n")
    names = [r.split("refs/heads/", 1)[1] for r in refs if f"j{num}" in r.lower() and "refs/heads/" in r]
    now = dt.datetime.now(dt.timezone.utc)
    live = []
    for name in names:
        commit = gh(f"repos/{REPO}/commits/{name}")
        when = dt.datetime.fromisoformat(commit["commit"]["committer"]["date"].replace("Z", "+00:00"))
        if (now - when).total_seconds() < RECENT_H * 3600:
            live.append(name)
    return live


def main() -> int:
    path = sys.argv[1] if len(sys.argv) > 1 else "work/tasks/muse.md"
    text = open(path, encoding="utf-8").read()
    start = text.find("Unstarted")
    end = text.find("\n\n", start) if start >= 0 else -1
    block = text[start:end if end >= 0 else None] if start >= 0 else ""
    bad = 0
    for line in block.splitlines():
        m = JOB_LINE.search(line)
        if not m:
            continue
        job, issue = f"J-{m.group(1)}", m.group(2)
        state = gh(f"repos/{REPO}/issues/{issue}")["state"]
        merged, opened = job_prs(job)
        branches = recent_branches(m.group(1))
        if state == "closed" or merged:
            verdict = "DONE"
        elif opened:
            verdict = "OPEN-PR"
        elif branches:
            verdict = "BRANCH " + ",".join(branches)
        else:
            verdict = "OK"
        bad += verdict != "OK"
        print(f"{verdict:8} {job} #{issue}")
    print(f"QUEUE {'STALE' if bad else 'CLEAN'}: {bad} not unstarted")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
