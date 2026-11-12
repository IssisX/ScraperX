#!/usr/bin/env python3
"""Read-only view of the versioned job plan and observed remote work."""
from __future__ import annotations

import datetime as dt
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/switchboard"))

import plan  # noqa: E402
from ghapi import GitHub  # noqa: E402

REPO = "IssisX/ScraperX"


def report(gh, out=print, content: str | None = None, now: dt.datetime | None = None) -> int:
    source = content if content is not None else (ROOT / "work/plan.md").read_text(encoding="utf-8")
    model = plan.parse(source)
    r = plan.observe(gh, model, now or dt.datetime.now(dt.timezone.utc), lambda x: print(x, file=sys.stderr))
    issues, prs = r["issues"], r["prs"]
    seen = dict(branches=r["branches"], inactive=r["inactive"], merged=r["merged"], dormant=r["dormant"])
    problems = plan.findings(model, issues, prs=prs, **seen)
    open_jobs = {ln.job for ln in model.lines if any(i.get("number") == ln.issue and
                                                       i.get("state", "open") == "open" for i in issues)}
    closed = {ln.job for ln in model.lines if ln.job not in open_jobs}
    statuses = {ln.job: plan.status(ln, closed=closed, prs=prs, **seen) for ln in model.lines}
    for ln in model.lines:
        out(f"{ln.job} #{ln.issue} {ln.owner}: {statuses[ln.job]}")
    for error in problems:
        out("PLAN-BLOCK " + error)
    for owner, limit in model.limits.items():
        active = sum(statuses[ln.job] in plan.IN_FLIGHT for ln in model.owner_lines(owner))
        out(f"WIP {owner}: {active}/{limit}")
    out(f"PLAN {'BLOCKED' if problems else 'CLEAN'} {model.fingerprint()}")
    return 1 if problems else 0


def main() -> int:
    gh = GitHub(REPO, dry_run=True, log=lambda x: print(x, file=sys.stderr))
    try:
        return report(gh)
    except (OSError, ValueError) as err:
        print(f"job status: {err}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
