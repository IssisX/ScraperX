#!/usr/bin/env python3
"""Read-only view of the job machine's table: every job's state, who owes its next step, and what is late.

Usage: python3 tools/board/job_status.py [--queue <owner> | --board]
The same plan, reads and machine the switchboard uses (tools/switchboard/plan.py, machine.py). The mail and each
head's proof are read only by the switchboard's own pass, so a job can read `planned` or `pushed` here where the
sweep, which also reads the mail and the proofs, says told or proven. Exit 1 when a job is late, 2 on a bad plan.
"""
from __future__ import annotations

import datetime as dt
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/switchboard"))

import machine  # noqa: E402
import plan  # noqa: E402
from ghapi import GitHub  # noqa: E402

REPO = "IssisX/ScraperX"


def report(gh, out=print, content: str | None = None, now: dt.datetime | None = None, view: str = "") -> int:
    now = now or dt.datetime.now(dt.timezone.utc)
    source = content if content is not None else (ROOT / "work/plan.md").read_text(encoding="utf-8")
    model = plan.parse(source)
    if model.errors:
        for error in model.errors:
            out("PLAN-BAD " + error)
        return 2
    reads = plan.observe(gh, model, now, lambda x: print(x, file=sys.stderr))
    rows = machine.table(model, machine.collect(model, reads, planned_at=reads.get("planned_at")))
    if view == "board":
        out(machine.board(model, rows, now))
    elif view:
        out(machine.queue(model, rows, view))
    else:
        for r in rows:
            who = f" -> {r.owes}: {r.next}" if r.owes else f" ({r.next})"
            out(f"{r.job} #{r.issue} {r.owner or '-'}: {r.state}{who}")
    late = machine.alarm_lines(rows, now)
    for line in late:
        out("LATE " + line)
    for owner, (n, limit) in machine.wip(model, rows).items():
        out(f"WIP {owner}: {n}/{limit}")
    out(f"PLAN {'LATE ' + str(len(late)) if late else 'CLEAN'} {model.fingerprint()}")
    return 1 if late else 0


def main(argv: list[str] | None = None) -> int:
    argv = sys.argv[1:] if argv is None else argv
    view = "board" if "--board" in argv else argv[argv.index("--queue") + 1] if "--queue" in argv[:-1] else ""
    gh = GitHub(REPO, dry_run=True, log=lambda x: print(x, file=sys.stderr))
    try:
        return report(gh, view=view)
    except (OSError, ValueError, RuntimeError) as err:
        print(f"job status: {err}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
