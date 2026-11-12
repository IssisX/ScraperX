"""Evidence reader: each job head's proof and review, for the job machine (machine.py). Reads only.

read(sw, model, reads, steered) -> dict[str, machine.Work]   (head sha -> its Work)
  steered  the sweep's list of switch.Steered (every open PR's facts, ctx and merge decision; a full pass has all)
Fills: one Work per open PR in `steered` (machine.work_of: proof_at from ctx["proof_done_at"]; codex_asked when
the switch's own ask-codex mark for this head is among ctx["comments"] and rules.needs_review still holds;
stacked: the name of another live branch of the same job and owner whose head is ahead of the PR's head while the
PR has an open FIX, by GET compare/<pr head>...<branch head>, at most STACK_READS a pass); and one Work per
planned job's own integrate head with no open PR (machine.branch_work with sw.proof(sha), newest heads first, at
most PROOF_READS a pass; a head not read stays "unread", never "none").
"""
from __future__ import annotations

import machine

PROOF_READS = 30
STACK_READS = 10


def read(sw, model, reads, steered) -> dict[str, machine.Work]:
    return {}   # the stub: every head reads as `pushed`, proof unread
