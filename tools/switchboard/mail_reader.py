"""Evidence reader: the filed mail, for the job machine (machine.py). Reads only.

read(sw, model, reads) -> machine.Mail
  sw      the Switch (sw.gh reads, sw.team.verdict_logins, sw.now, sw.out, sw.board_comments(), sw.comments_since())
  model   plan.Plan (work/plan.md)
  reads   plan.observe's answer (issues, branches, prs, ...)
Fills: posts (machine.posts over the comments read: bridge posts from team logins only), notices
(machine.sent_notices: the switch's own plan notices), missing (job -> a commit its owner's newest mail names
that GitHub does not have: machine.named_shas, each sha checked first against the branch and PR heads in `reads`,
then with GET repos/<repo>/commits/<sha>, where 404 or 422 means missing; at most SHA_READS such GETs a pass).
Every listing is bounded and the bound is printed when it is reached; an unread mail is never read as "no mail".
"""
from __future__ import annotations

import machine

SHA_READS = 10


def read(sw, model, reads) -> machine.Mail:
    return machine.Mail()   # the stub: no mail read yet (every mail owner's job stays `planned`)
