#!/usr/bin/env python3
"""Check live issue-owned jobs (no arguments), or audit a legacy queue file (explicit path).

The checks live in tools/switchboard/muse_queue.py (J-097), which the switchboard also runs every sweep (a
QUEUE-LOW inbox item under 8 really-unstarted jobs). A queue line names a job as `J-NNN #N`. For each one it reports:
  DONE      the issue is closed, or a merged PR's body starts `Job: J-NNN`
  OPEN-PR   an open PR's body starts `Job: J-NNN` (work exists; not unstarted)
  BRANCH    a remote branch names the job (`j0NN`) and moved in the last 72 h (work exists)
  STARTED   the owner said it started (a bridged Muse post on the issue with "started" in its title line), work
            that is not pushed yet: never take such a job from its owner
  OK        none of the above: the job is really unstarted
A line that says it waits ("after J-NNN lands", "on J-NNN") also gets WAITS-ON J-NNN (open) or READY (that job's
issue is closed, so the integrator tells the owner to start it). Every job on the "Parked:" line is listed with the
newest non-bot comment on its issue, so an answer that unparks it is seen. Duplicates stay a judgment: no string
match finds two jobs doing the same thing.

Two holes are listed too (11-12): OWNER, a job the queue tells Muse to work (its "Finish (in flight):" line or the
Unstarted block) whose open issue is not labelled owner:muse (a job taken by a helper must leave the queue and carry
owner:claude); NO-LABEL, an open issue titled J-NNN with no `job` label, which the switchboard cannot see.

Usage: python3 tools/board/queue_check.py\nLegacy audit: python3 tools/board/queue_check.py work/tasks/muse.md
Exit 1 when any line in the "Unstarted" block is not OK or any hole is listed, so a stale queue cannot be mailed unseen.
Reads only (REST `gh api`, through tools/switchboard/ghapi.py); writes nothing.
"""
from __future__ import annotations

import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "switchboard"))

import muse_queue  # noqa: E402
from ghapi import GitHub  # noqa: E402

REPO = "IssisX/ScraperX"


def main() -> int:
    # The normal check reads the live issue orders. An explicit queue-file path is
    # retained only for historical analysis and existing parser regression tests.
    if len(sys.argv) == 1:
        import job_status
        return job_status.main()
    path = sys.argv[1]
    text = pathlib.Path(path).read_text(encoding="utf-8")
    gh = GitHub(REPO, dry_run=True, log=lambda line: print(line, file=sys.stderr))   # reads only
    report = muse_queue.check(gh, text)
    bad = 0
    for v in report.verdicts:
        verdict = v.text
        bad += v.verdict != "OK"
        wait = muse_queue.WAITS.search(v.line.rest)
        if wait:
            verdict += "  " + muse_queue.blocker_state(gh, wait.group(1))
        print(f"{verdict:8} {v.line.job} #{v.line.issue}")
    for num, issue in muse_queue.parked(text):
        print(f"PARKED   J-{num} #{issue}  newest: {muse_queue.newest_comment(gh, issue)}")
    holes = muse_queue.owner_mismatches(gh, text)
    holes = [f"OWNER    {h}" for h in holes] + [f"NO-LABEL {h}" for h in
                                                  muse_queue.unlabeled(gh.paged(f"repos/{REPO}/issues?state=open"))]
    for line in holes:
        print(line)
    for note in report.notes:
        print(f"NOTE     {note}", file=sys.stderr)
    print(f"QUEUE {'STALE' if bad or holes else 'CLEAN'}: {bad} not unstarted"
          + (f", {len(holes)} owner or label holes" if holes else ""))
    return 1 if bad or holes else 0


if __name__ == "__main__":
    sys.exit(main())
