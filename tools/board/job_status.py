#!/usr/bin/env python3
"""Read-only job board: issues are orders, remote branches are work receipts."""
from __future__ import annotations

import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "switchboard"))

import job_control  # noqa: E402
from ghapi import GitHub  # noqa: E402

REPO = "IssisX/ScraperX"


def report(gh, out=print) -> int:
    issues = gh.paged(f"repos/{gh.repo}/issues?state=open")
    branches = gh.paged(f"repos/{gh.repo}/branches")
    prs = gh.paged(f"repos/{gh.repo}/pulls?state=open")
    inv = job_control.inventory(issues, branches, prs)
    for job in sorted(inv.jobs.values(), key=lambda x: x.number):
        out(f"{job.id} #{job.number}: owner={job.owner or '?'} state={job.state or '?'}"
            + (f" blocked-by={','.join(job.blocked_by)}" if job.blocked_by else ""))
    for error in inv.violations:
        out(f"VIOLATION {error}")
    out(f"MUSE READY {len(inv.ready_muse)}: " + ", ".join(j.id for j in inv.ready_muse))
    out(f"MUSE WIP {inv.active_muse}/{job_control.MUSE_WIP_LIMIT}")
    out(f"JOB CONTROL {'BLOCKED' if inv.violations else 'CLEAN'}")
    return 1 if inv.violations else 0


def main() -> int:
    gh = GitHub(REPO, dry_run=True, log=lambda x: print(x, file=sys.stderr))
    return report(gh)


if __name__ == "__main__":
    sys.exit(main())
