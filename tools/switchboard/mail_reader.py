"""Evidence reader: the filed mail, for the job machine (machine.py). Reads only.

read(sw, model, reads, also=()) -> machine.Mail
  sw      the Switch (sw.gh reads, sw.team.verdict_logins, sw.now, sw.out, sw.board_comments(), sw.comments_since())
  model   plan.Plan (work/plan.md)
  reads   plan.observe's answer (issues, branches, prs, ...)
Fills: posts (machine.posts over the comments read: bridge posts from team logins only), notices
(machine.sent_notices: the switch's own plan notices), missing (job -> a commit its owner's newest mail names
that GitHub does not have: machine.named_shas, each sha checked first against the branch and PR heads in `reads`,
then with GET repos/<repo>/commits/<sha>, where 404 or 422 means missing; at most SHA_READS such GETs a pass).
Every listing is bounded and the bound is printed when it is reached; an unread mail is never read as "no mail".

What is read, and why there. The bridge files a mail on the open issue its subject names (#N), else on the job
issue J-NNN names, else on the board (muse_bridge.gs IN and REC); a plan notice goes where the switch posts it. A
receipt or a "started" can be days old and still be the job's state, and the repo files about 600 comments a day,
so no time window holds a job's mail whole. So each read is a whole thread:
  - the board (sw.board_comments(), read on past the client's 500 cap when it is full: plan.whole);
  - every open plan job's issue thread with comments (plan.whole each, ETag-cached; the most recently updated
    first, at most MAIL_THREADS a pass: the jobs of the threads past it are Mail.unread, and the machine shows
    each as unread, never as having no mail; nothing raises, so one busy day never silences the whole pass);
  - the last RECENT of repo comments (sw.comments_since: the inbox's own listing, so no extra read), for a mail
    filed on some other issue its subject named;
  - `also`: the switch's notice comments (closed boards too while a version is unanswered), so a receipt is read
    from the same comment set as the notice it answers.
A read that fails raises: the caller then has no Mail, never an empty one.

Which named hex is a commit claim: machine.NAMED_SHA, less what the mail itself says is not one: a tree
(`tree 8d2ce0d4`), a head it calls local (`local merge commit 9c5a44ee1`, `remote object for local 3696ab5f`:
it names the pushed head beside it) and a plan version in any case (`Plan 0123456789ab`). An unlabeled local head
(`baseline f5ee22d6b`, J-106; `@ ad9fc702`, J-059) is still a claim, and GitHub's 422 shows it is missing.
"""
from __future__ import annotations

import dataclasses
import datetime as dt
import re

import machine
import plan as P
import rules
from ghapi import GitHubError, RateLimited

SHA_READS = 10        # GET commits/<sha> a pass; a sha past it is left unchecked (never counted missing) and said
MAIL_THREADS = 120    # job issue threads read a pass (work/plan.md: 80 lines on 11-12); past it: Mail.unread
RECENT = dt.timedelta(hours=24)   # switch.INBOX_RUNS_WINDOW: the same comments_since listing the inbox reads

NOT_A_CLAIM = re.compile(r"(?:\btree|\blocal(?:\s+[a-z]+){0,2}|\bplan)\s*:?\s*$", re.IGNORECASE)
NOTE = "NOTE        mail: "


def read(sw, model, reads, also=()) -> machine.Mail:
    """`also`: comments the caller read already (the switch's notice comments, closed boards included while a
    version is unanswered): posts and notices come from one comment set, so a receipt filed on a board that has
    since closed is read with the notice it answers."""
    comments, unread = _comments(sw, model, reads, also)
    mail = machine.Mail(posts=tuple(machine.posts(comments, sw.team.verdict_logins)),
                        notices=tuple(machine.sent_notices(comments)), unread=unread,
                        bound=MAIL_THREADS if unread else 0)
    return dataclasses.replace(mail, missing=_missing(sw, model, reads, mail))


def _comments(sw, model, reads, also=()) -> tuple[list[dict], frozenset[str]]:
    """Every comment of the board, of each open plan job's thread (the most recently updated first, at most
    MAIL_THREADS), of the last RECENT and of `also`, once each; and the jobs whose thread was past the bound."""
    repo = sw.gh.repo
    board = sw.board_issue()
    got = list(sw.board_live_comments())
    if got and not len(got) % 100:   # the client's paged() may have stopped at its cap: read on, whole
        got = P.whole(sw.gh, f"repos/{repo}/issues/{board}/comments")
    got += sw.board_carry()   # the board it continues, just before the roll (Switch.board_carry)
    open_issues = {int(it["number"]): it for it in reads.get("issues") or []
                   if it.get("state", "open") == "open" and it.get("pull_request") is None and it.get("number")}
    threads = sorted(dict.fromkeys(
        ln.issue for ln in model.lines
        if ln.issue != board and ln.issue in open_issues and int(open_issues[ln.issue].get("comments", 1) or 0)),
        key=lambda n: open_issues[n].get("updated_at") or "", reverse=True)
    past = set(threads[MAIL_THREADS:])
    if past:
        sw.out(f"{NOTE}{len(past)} job thread(s) past the {MAIL_THREADS}-thread bound not read this pass; their jobs "
               "show their mail unread (no receipt or start proven)")
    for number in threads[:MAIL_THREADS]:
        got += P.whole(sw.gh, f"repos/{repo}/issues/{number}/comments")
    got += sw.comments_since(sw.now - RECENT)
    got += list(also)
    seen: dict = {}
    for c in got:
        seen.setdefault(c.get("id"), c)
    return list(seen.values()), frozenset(ln.job for ln in model.lines if ln.issue in past)


def _claims(body: str) -> str:
    """The body with every named hex the mail itself says is not a pushed commit blanked out."""
    def keep(m: re.Match) -> str:
        return "_" * len(m.group(0)) if NOT_A_CLAIM.search(body[max(0, m.start() - 32):m.start()]) else m.group(0)
    return machine.NAMED_SHA.sub(keep, body)


def _on_github(sw, sha: str) -> bool | None:
    """True: GitHub has the commit. False: 404 or 422, it does not. None: the read failed otherwise (unknown)."""
    try:
        sw.gh.get(f"repos/{sw.gh.repo}/commits/{sha}")
        return True
    except RateLimited:
        raise
    except GitHubError as err:
        if err.status in (404, 422) or re.search(r"\bHTTP (?:404|422)\b", err.detail or ""):
            return False
        sw.out(f"{NOTE}GET commits/{sha} failed ({err.detail[:120]}); not counted missing")
        return None


def _missing(sw, model, reads, mail: machine.Mail) -> dict[str, str]:
    by_job = model.by_job()
    by_thread = {ln.issue: ln.job for ln in model.lines}
    for it in reads.get("issues") or []:
        job = rules.job_of_title(it.get("title") or "")
        if job and it.get("number"):
            by_thread.setdefault(int(it["number"]), job)
    claims = machine.Mail(posts=tuple(dataclasses.replace(p, body=_claims(p.body)) for p in mail.posts),
                          worker=mail.worker)
    named = {P.plan_job(j, by_job): v for j, v in machine.named_shas(claims, by_thread).items()}
    known = {P.branch_sha(b) for b in reads.get("branches") or []}
    known |= {(p.get("head") or {}).get("sha") or "" for p in reads.get("prs") or []}
    known |= set(reads.get("inactive") or ()) | set(reads.get("head_at") or {})
    known.discard("")
    checked = sw.__dict__.setdefault("_mail_commits", {})   # sha -> True / False / None, once a run
    left, unchecked, missing = SHA_READS, [], {}
    for job in sorted((j for j in named if j in by_job), key=lambda j: (named[j][0].at, named[j][0].id), reverse=True):
        for sha in named[job][1]:
            if any(k.startswith(sha) for k in known):
                continue
            if sha not in checked:
                if left <= 0:
                    unchecked.append(f"{job} {sha}")
                    continue
                left -= 1
                checked[sha] = _on_github(sw, sha)
            if checked[sha] is False:
                missing[job] = sha
                break
    if unchecked:
        sw.out(f"{NOTE}{len(unchecked)} named commit(s) past the {SHA_READS}-read bound not checked this pass, none "
               f"counted missing: {', '.join(unchecked[:6])}")
    return missing
