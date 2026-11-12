"""Evidence reader: each job head's proof and review, for the job machine (machine.py). Reads only.

read(sw, model, reads, steered) -> dict[str, machine.Work]   (head sha -> its Work)
  steered  the sweep's list of switch.Steered (every open PR's facts, ctx and merge decision; a full pass has all)
Fills: one Work per open PR in `steered` (machine.work_of: proof_at from ctx["proof_done_at"]; codex_asked when
the switch's own ask-codex mark for this head is among ctx["comments"] and rules.needs_review still holds;
codex_woken, the time a person's login asked Codex for this head;
stacked: the name of another live branch of the same job and owner whose head is ahead of the PR's head while the
PR has an open FIX, by GET compare/<pr head>...<branch head>, at most STACK_READS a pass); and one Work per
planned job's own integrate head with no open PR (machine.branch_work with sw.proof(sha), newest heads first, at
most PROOF_READS a pass; a head not read stays "unread", never "none").

Every read that is skipped (a bound reached, a read that failed, a rate limit) leaves its head out of the answer
and says so on sw.out: machine.collect then shows that head as unread, never as having no proof. A failed compare
leaves the PR's gate at its plain FIX. Nothing here writes to GitHub.
"""
from __future__ import annotations

import datetime as dt
import re

import ghapi
import machine
import plan as P
import rules

PROOF_READS = 30
STACK_READS = 10
AUTO_PR_PREFIX = "fb-bot/integrate-"   # switch.AUTO_PR_PREFIX: the heads the switch opens a PR for by itself
MARK = "<!-- switchboard:{kind} pr={pr} head={head} -->"   # switch.MARK
CODEX_MENTION = "@codex review"                             # switch.CODEX_MENTION
INTEGRATE = re.compile(r"(?:^|/)integrate-")
_OLDEST = dt.datetime.min.replace(tzinfo=dt.timezone.utc)


def read(sw, model, reads, steered) -> dict[str, machine.Work]:
    reads = reads or {}
    works: dict[str, machine.Work] = {}
    budget = _Budget()
    for s in steered or []:
        f = s.facts
        works[f.head_sha] = machine.work_of(f, s.decision, sw.team, proof_at=(s.ctx or {}).get("proof_done_at") or "",
                                            codex_asked=codex_asked(sw, f, s.ctx or {}),
                                            codex_woken=codex_woken(sw, f, s.ctx or {}),
                                            stacked=stacked(sw, model, reads, f, s.ctx or {}, budget))
    if budget.skipped:
        sw.out(f"NOTE        work: {budget.skipped} stacked-fix compare(s) not read this pass (at most {STACK_READS}); "
               "those PRs show their plain FIX")
    works.update(branch_works(sw, model, reads, steered or [], set(works)))
    return works


class _Budget:
    def __init__(self) -> None:
        self.left, self.skipped, self.stopped = STACK_READS, 0, False


def codex_asked(sw, f: rules.PrFacts, ctx: dict) -> bool:
    """The switch's own ask-codex mark for this head, posted by the workflow bot (whose mention never wakes Codex:
    the integrator must post `@codex review`), while the head still needs a review. A mark in anyone else's
    comment is not the switch's: the repo is public."""
    tag = MARK.format(kind="ask-codex", pr=f.number, head=f.head_sha)
    marked = any(tag in (c.get("body") or "")
                 and ((c.get("user") or {}).get("login") or "").strip().lower() == rules.BOT_LOGIN.lower()
                 for c in ctx.get("comments") or [])
    return marked and rules.needs_review(f, sw.team.vendor_of_branch(f.head_ref), sw.team)


def codex_woken(sw, f: rules.PrFacts, ctx: dict) -> str:
    """When a person's login asked Codex for this head, "" if none did: the switch's ask mark posted with the team
    token, or a team login's own `@codex review` after the head commit (switch.Switch.codex_asked's second half).
    Codex was woken, so the review is Codex's step and nobody is told to ask again."""
    tag = MARK.format(kind="ask-codex", pr=f.number, head=f.head_sha)
    times = []
    for c in ctx.get("comments") or []:
        login = ((c.get("user") or {}).get("login") or "").strip().lower()
        body = c.get("body") or ""
        if login == rules.BOT_LOGIN.lower() or login not in sw.team.verdict_logins:
            continue   # the bot's mention wakes nobody; the repo is public, so anyone else's comment is not an ask
        if tag in body or (body.lstrip().lower().startswith(CODEX_MENTION)
                           and (c.get("created_at") or "") >= f.head_committed_at):
            times.append(c.get("created_at") or "")
    return max(times, default="")


def _fix_open(f: rules.PrFacts) -> bool:
    """machine.work_of's FIX gate: an open review FIX on the head, or an eye FIX where the job needs eyes."""
    if rules.open_fixes(f.verdicts, f.head_sha, f.head_committed_at, "review"):
        return True
    return "eyes" in f.needs and bool(rules.open_fixes(f.verdicts, f.head_sha, f.head_committed_at, "eye"))


def _head_at(reads: dict, sha: str) -> dt.datetime:
    return machine.when((reads.get("head_at") or {}).get(sha)) or _OLDEST


def stacked(sw, model, reads: dict, f: rules.PrFacts, ctx: dict, budget: _Budget) -> str:
    """The live branch of the PR's job and owner that holds the answer to its open FIX: its head is ahead of the
    PR's head (the fix was stacked on the PR it unblocks, #343). "" when no FIX is open, the PR is not on trunk
    (work_of names that gate first), no such branch is ahead, or the compare was not read."""
    if f.base_ref != rules.TRUNK or not _fix_open(f):
        return ""
    job = ctx.get("job") or P.job_of_branch(f.head_ref)
    if not job:
        return ""
    owner = machine.agent_of(f.head_ref, model.limits)
    inactive = reads.get("inactive") or frozenset()
    seen: set[str] = set()
    todo = []
    for b in reads.get("branches") or []:
        name, sha = P.branch_name(b), P.branch_sha(b)
        if (name == f.head_ref or not sha or sha == f.head_sha or sha in inactive or sha in seen
                or P.job_of_branch(name) != job or not machine.owns(owner, name)):
            continue
        seen.add(sha)
        todo.append((name, sha))
    todo.sort(key=lambda x: _head_at(reads, x[1]), reverse=True)
    for i, (name, sha) in enumerate(todo):
        if budget.stopped or budget.left <= 0:
            budget.skipped += len(todo) - i
            return ""
        budget.left -= 1
        try:
            cmp = sw.gh.get(f"repos/{sw.gh.repo}/compare/{f.head_sha}...{sha}") or {}
        except ghapi.RateLimited as err:
            budget.stopped = True
            budget.skipped += len(todo) - i
            sw.out(f"NOTE        work: rate limited comparing {name} with #{f.number} ({err}); no more compares this pass")
            return ""
        except (RuntimeError, KeyError, ValueError) as err:
            sw.out(f"NOTE        work: compare of {name} with #{f.number} failed ({type(err).__name__}: {err}); "
                   "its plain FIX stands")
            continue
        if cmp.get("status") == "ahead":
            return name
    return ""


def branch_works(sw, model, reads: dict, steered, known: set[str]) -> dict[str, machine.Work]:
    """One Work per planned job's own integrate head with no open PR, by sw.proof, newest heads first."""
    by_job = model.by_job()
    inactive = reads.get("inactive") or frozenset()
    open_refs, open_shas = set(), set(known)
    for p in reads.get("prs") or []:
        head = p.get("head") or {}
        if p.get("state", "open") != "closed":
            open_refs.add(head.get("ref") or "")
            open_shas.add(head.get("sha") or "")
    for s in steered:
        open_refs.add(s.facts.head_ref)
        open_shas.add(s.facts.head_sha)
    todo, seen = [], set()
    for b in reads.get("branches") or []:
        name, sha = P.branch_name(b), P.branch_sha(b)
        line = by_job.get(P.job_of_branch(name))
        if (line is None or not sha or not INTEGRATE.search(name) or not machine.owns(line.owner, name)
                or sha in inactive or sha in open_shas or name in open_refs or sha in seen):
            continue
        seen.add(sha)
        todo.append((name, sha))
    todo.sort(key=lambda x: _head_at(reads, x[1]), reverse=True)
    out: dict[str, machine.Work] = {}
    for i, (name, sha) in enumerate(todo[:PROOF_READS]):
        try:
            proof, _url, _run, done_at = sw.proof(sha)
        except ghapi.RateLimited as err:
            sw.out(f"NOTE        work: rate limited reading the proof of {name} ({err}); "
                   f"{len(todo) - i} head(s) left unread this pass")
            return out
        except (RuntimeError, KeyError, ValueError, TypeError) as err:
            sw.out(f"NOTE        work: the proof of {name} @{sha[:8]} was not read ({type(err).__name__}: {err}); "
                   "it stays unread")
            continue
        finished = proof == "green" or proof.startswith("red")
        at = machine.when(done_at) if finished and done_at else (reads.get("head_at") or {}).get(sha)
        out[sha] = machine.branch_work(name, sha, proof, at=at, auto_pr=name.startswith(AUTO_PR_PREFIX))
    if len(todo) > PROOF_READS:
        sw.out(f"NOTE        work: {len(todo) - PROOF_READS} integrate head(s) with no PR not read this pass "
               f"(at most {PROOF_READS}); they stay unread")
    return out
