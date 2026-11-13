"""The job machine: one state per job, computed only from evidence (collaboration rebuild, Cory 11-12).

work/plan.md is the one authored record: who owns a job, in what order, held or not. Everything else is observed:
the open issues, the remote heads, the proof and the merge rule's answer on a head, merged PRs, and the mail the
bridge filed. collect() turns those reads into one Evidence per job; step() turns one job's evidence into its
state and its one next step: who owes it and how long it may take (TABLE). The only alarm is a job past that
limit (Step.alarm). Labels, the board and a worker's queue are views of the table (labels, board, queue,
notice); nobody sets a state by hand, and nothing here writes to GitHub.

The states, in the order a job moves through them:
  planned       a plan line and its open issue; the owner's present order has not been sent to it
  told          the notice of the owner's order version was sent; no receipt yet
  acknowledged  the owner answered `plan <version>` (or wrote the order itself); no remote work yet
  started       the owner's wip-<job> branch is on the remote
  pushed        the owner's integrate-<job> head is on the remote; its proof is not green
  proven        the proof is green on that head
  reviewed      a LAND that counts names that head and no FIX is open
  landed        a PR of the job merged, no unlanded head is left, and its issue is still open: a part is left. The
                last part's PR carries `Closes #N`, so GitHub closes the issue at that merge; until then the owner
                owes the next part (or the close), in a lane like any start
  closed        the issue is closed
and beside them:
  held          the plan holds it (the reason is on its line); an open PR still lands on its own proof and review
  blocked       a job it comes `after` is not closed, or the plan moved the job from a mail owner that has not yet
                answered the notice that dropped it (the handover: one clock, the old owner's)
  conflict      the evidence disagrees with itself or with the plan; one rule, and the next step is the integrator's

The job set is every job the evidence names, not only the plan's: a plan line, an open job issue, a head, a PR or a
mailed start. A lettered id (J-044c) is its base job (J-044) when only the base has a line, so a split's heads are
the base job's work (or another owner's: a conflict), never unseen.

Each job's clock runs from its own line. The switch's marks carry, per job, the hash of its line and when that line
was first told and acknowledged (mark_jobs), so a commit that changes another line moves nothing. The order version
(the notice and its receipt) is work/plan.md alone, never an issue's text.

Lanes are granted in plan order before any other gate: pushed work always holds its lane; then each wip head and
each mailed start, then each acknowledged job (and each landed part owed a next one), while lanes are free. A start
or a wip head with no lane granted is told to park, on no clock; nothing is cut.
"""
from __future__ import annotations

from dataclasses import dataclass, field
import datetime as dt
import hashlib
import re
from typing import NamedTuple

import plan as P
import rules

STATES = ("planned", "told", "acknowledged", "started", "pushed", "proven", "reviewed", "landed", "closed",
          "held", "blocked", "conflict")
IN_FLIGHT = frozenset({"started", "pushed", "proven", "reviewed"})   # what an owner's limit counts (and a granted claim)
COMMITTED = frozenset({"pushed", "proven", "reviewed"})   # pushed work: its lane is never refused
RANK = {"pushed": 0, "proven": 1, "reviewed": 2}           # how far a Work is: a job is as far as its least advanced
OWNER, SWITCH, INTEGRATOR, CI, REVIEWER, NOBODY, AHEAD = "owner", "switchboard", "integrator", "ci", "reviewer", "", "ahead"
NOW = dt.timedelta(0)


def hours(n: float) -> dt.timedelta:
    return dt.timedelta(hours=n)


@dataclass(frozen=True)
class Rule:
    owes: str                    # OWNER, SWITCH, INTEGRATOR, CI, REVIEWER, AHEAD (whoever owns the job in the way) or NOBODY
    limit: dt.timedelta | None   # how long the step may take; None: nothing is owed on a clock
    next: str                    # the step, in plain words ({job}, {owner}, {ver}, {wip}, {integrate}, {ref}, {sha}, {pr}, {issue}, {why})


# The state table. A row is a state and, where one state has more than one possible next step, the gate that is
# shut ("" is the plain case). "*" rows hold in whatever state the gate is met. Limits are the clocks the
# switchboard already kept apart: 24 h to start (the reconciler's START_GRACE), 3 days on a branch with no proof
# (CLAUDE.md), 3 h red (INBOX_RED_STUCK), 6 h on a FIX (INBOX_FIX_STUCK), 3 h for a review (review.fallback_after_hours).
TABLE: dict[tuple[str, str], Rule] = {
    ("planned", ""): Rule(SWITCH, hours(1), "send {owner} the notice of plan {ver}"),
    ("planned", "by-hand"): Rule(INTEGRATOR, None, "tell {owner} its order by hand: no notice channel reaches it"),
    ("told", ""): Rule(OWNER, hours(2), 'answer the notice: put "plan {ver}" in a mail subject'),
    # Grok's runs are at 10 AM and 4 PM Central, so a notice sent after the second is read the next morning
    ("told", "reply"): Rule(OWNER, hours(20), 'answer the notice: reply "plan {ver}" on the thread it is on'),
    ("acknowledged", ""): Rule(OWNER, hours(24), "start it: push {wip}"),
    ("acknowledged", "queued"): Rule(OWNER, None, "wait for a lane: {why}"),
    ("started", ""): Rule(OWNER, hours(72), "push {integrate} so its proof runs"),
    ("pushed", "unread"): Rule(SWITCH, hours(72), "read the proof on {ref} @ {sha}: it has no PR and no commit in 3 days"),
    ("pushed", "none"): Rule(CI, hours(1), "start the proof on {ref} @ {sha}"),
    ("pushed", "running"): Rule(CI, hours(2), "finish the proof on {ref} @ {sha}"),
    ("pushed", "red"): Rule(OWNER, hours(3), "push a head that fixes the red proof ({why})"),
    ("proven", "auto-pr"): Rule(SWITCH, hours(1), "open the PR for {ref}"),
    ("proven", "no-pr"): Rule(OWNER, hours(1), "open the PR for {ref}"),
    ("proven", ""): Rule(REVIEWER, hours(3), "review #{pr} @ {sha}"),
    ("proven", "note"): Rule(INTEGRATOR, hours(1), "read the automatic review's NOTE on #{pr} and post LAND or FIX"),
    ("reviewed", ""): Rule(SWITCH, hours(1), "merge #{pr}"),
    ("reviewed", "hand"): Rule(INTEGRATOR, hours(1), "merge #{pr} by hand: {why}"),
    ("reviewed", "security"): Rule(SWITCH, hours(1), "run the security pass on #{pr} @ {sha}"),
    ("reviewed", "eye"): Rule(SWITCH, hours(1), "run the eye on #{pr} @ {sha}"),
    ("reviewed", "wait"): Rule(SWITCH, hours(1), "merge #{pr} once the merge rule clears: {why}"),
    ("*", "claimed"): Rule(OWNER, hours(2), "push {wip}: a start was mailed and no head is on the remote"),
    ("*", "no-lane"): Rule(OWNER, None, "park it: no lane is granted ({why}); push any work it holds and open no "
                                        "more on it"),
    ("*", "mail-unread"): Rule(SWITCH, hours(24), "read this job's mail thread: it was past the mail reader's bound "
                                                  "of {why} threads, so no receipt or start on it is proven"),
    ("*", "unpushed"): Rule(OWNER, NOW, "push {why} to GitHub: a mail names it and GitHub does not have it, so it "
                                        "proves nothing and nobody may act on it"),
    ("*", "codex"): Rule(INTEGRATOR, hours(1), "post `@codex review` on #{pr} (only a person's login wakes Codex)"),
    ("*", "asked"): Rule(REVIEWER, hours(3), "review #{pr} @ {sha}: Codex was asked, and the merge waits for its clean review"),
    ("*", "fix"): Rule(OWNER, hours(6), "push a head to {ref} that answers the open FIX on #{pr} ({why})"),
    ("*", "stacked"): Rule(OWNER, hours(6), "push the fix onto {ref}: it sits on {why}, which waits for #{pr} to land"),
    ("*", "base"): Rule(OWNER, hours(3), "merge trunk into {ref} and push: #{pr} conflicts with the base"),
    ("*", "draft"): Rule(OWNER, hours(24), "mark #{pr} ready for review"),
    ("*", "off-trunk"): Rule(OWNER, hours(24), "point #{pr} at trunk: {why}"),
    ("landed", ""): Rule(OWNER, hours(24), "start its next part: a part merged and #{issue} is still open (the last "
                                           "part's PR carries `Closes #{issue}`); if no part is left, close #{issue}"),
    ("landed", "queued"): Rule(OWNER, None, "wait for a lane for its next part: {why}"),
    ("closed", ""): Rule(INTEGRATOR, None, "take its line out of the plan (tools/board/plan.py tidy)"),
    ("held", ""): Rule(NOBODY, None, "none while it is held: {why}"),
    ("blocked", ""): Rule(AHEAD, None, "land {why} first"),
    ("blocked", "handover"): Rule(AHEAD, hours(2), "hand it over to {owner}: {why}"),
    ("conflict", ""): Rule(INTEGRATOR, NOW, "rule it: {why}"),
}

# Who is told its order, and how. A mail owner gets the versioned notice through the bridge and answers with a
# receipt. A comment owner gets it as a board comment that starts `To <Name>:`, the one thing its runs read
# (work/tasks/grok-answers.md, grok-engineer.md: Grok and Grok Bot read comments that start `To Grok:` or
# `To Builder:`), and answers with a reply naming `plan <version>` (comment_receipts). The integrator writes the
# plan, so Claude's own lines need no notice. Any other owner has no channel the machine can prove: its jobs stay
# `planned` and the integrator tells it by hand.
MAIL_OWNERS = frozenset({"muse"})
COMMENT_OWNERS = {"grok": "Grok", "builder": "Builder"}   # owner -> the name its notice opens with
COMMENT_AUTHORS = frozenset({"Grok", "Builder", "PE"})    # who may answer one (asks.author: Grok Bot signs as any)
NOTICE_OWNERS = MAIL_OWNERS | frozenset(COMMENT_OWNERS)
SELF_TOLD = frozenset({"claude"})
PREFIXES = {"muse": ("fb-bot/", "muse/"), "builder": ("builder/", "pe/")}


def prefixes(owner: str) -> tuple[str, ...]:
    return PREFIXES.get(owner, (owner + "/",))


def owns(owner: str, ref: str) -> bool:
    return ref.startswith(prefixes(owner))


def agent_of(ref: str, owners) -> str:
    """The plan owner whose branch this is, else the branch's first path part."""
    return next((o for o in owners if owns(o, ref)), ref.split("/", 1)[0])


@dataclass(frozen=True)
class Head:
    """A remote head that carries unlanded work for a job."""
    ref: str
    sha: str = ""
    at: dt.datetime | None = None   # its head commit's time; None when it was not read

    @property
    def wip(self) -> bool:
        return bool(re.search(r"(?:^|/)wip-", self.ref))


@dataclass(frozen=True)
class Work:
    """The job's proof-bearing head and what the proof and the merge rule say of it (work_of, branch_work)."""
    ref: str
    sha: str
    proof: str = "unread"            # switch.proof's words: green, running, none, "red: <jobs>"; unread: not read this pass
    land: bool = False               # a LAND that counts names this head, and no FIX is open on it
    gate: str = ""                   # the TABLE gate that is shut, "" for the plain next step
    why: str = ""
    at: dt.datetime | None = None    # when the head got where it is: pushed, proven or reviewed
    pr: int = 0                      # its open PR, 0 for none

    @property
    def state(self) -> str:
        return "reviewed" if self.land and self.proof == "green" else "proven" if self.proof == "green" else "pushed"


@dataclass(frozen=True)
class Evidence:
    """Everything observed about one job. Every field is a read; none is typed by hand."""
    job: str
    line: P.Line | None = None                 # its plan line; None: an open job issue the plan does not hold
    issues: tuple[int, ...] = ()               # the open issues titled with the job
    closed: bool = False                       # the plan's issue was read closed
    waits: tuple[str, ...] = ()                # the jobs it comes `after` that are not closed
    version: str = ""                          # the owner's order version (order_version)
    planned_at: dt.datetime | None = None      # when work/plan.md last changed on trunk (only a fallback clock)
    notice_at: dt.datetime | None = None       # this job's line, as it stands, was first told to its owner
    receipt_at: dt.datetime | None = None      # the owner first acknowledged that line (`plan <version>` came back)
    claimed_at: dt.datetime | None = None      # the owner mailed "started" after the job's newest notice or ruling
    unpushed: str = ""                         # a commit the owner's newest mail names that GitHub does not have
    unpushed_at: dt.datetime | None = None     # when that mail was filed
    heads: tuple[Head, ...] = ()               # every live head that names the job, whoever pushed it
    works: tuple[Work, ...] = ()               # one per open PR of the owner's and per its integrate head with no PR
    merged_at: dt.datetime | None = None       # a `Job:` PR of it merged
    clashes: tuple[str, ...] = ()              # evidence that disagrees, as the readers found it
    handover: str = ""                         # the old owner that has not answered the notice that dropped the job
    handover_ver: str = ""                     # that owner's present version, the one it answers
    handover_at: dt.datetime | None = None     # when the notice that dropped the job went to it
    mail_unread: int = 0                       # >0: the job's mail thread was past the reader's bound (that bound)

    @property
    def work(self) -> Work | None:
        return least(self.works)


def least(works) -> Work | None:
    """The least advanced of a job's Works: the job is no further on than its least advanced live head (two open
    PRs for one job, #463 and #464: the lower one's red proof or FIX is the job's, on its own clock)."""
    oldest = dt.datetime.min.replace(tzinfo=dt.timezone.utc)
    return min(works, key=lambda w: (RANK[w.state], w.at or oldest, w.pr), default=None)


def flying(r: "Step") -> bool:
    """In flight, as an owner's limit counts it: a pushed head of the job, or a mailed start a lane was granted to."""
    return r.state in IN_FLIGHT or r.gate == "claimed"


@dataclass(frozen=True)
class Step:
    """One job's row of the table: its state and the one next step."""
    job: str
    owner: str
    state: str
    gate: str = ""
    owes: str = ""
    next: str = ""
    why: str = ""
    since: dt.datetime | None = None
    limit: dt.timedelta | None = None
    issue: int = 0
    title: str = ""

    def due(self) -> dt.datetime | None:
        return None if self.limit is None or self.since is None else self.since + self.limit

    def late(self, now: dt.datetime) -> bool:
        """Past its limit. A conflict has no clock to wait out: it is late the moment it is seen."""
        if self.limit is None:
            return False
        return self.limit == NOW or (self.since is not None and now - self.since > self.limit)

    def alarm(self, now: dt.datetime) -> str:
        """The one alarm: job J in state S past its limit; the next step belongs to X."""
        if self.limit == NOW:
            return f"{self.job} in {self.state}: next step ({self.next}) belongs to {self.owes}"
        over = now - self.since - self.limit
        return (f"{self.job} in {self.state} {span(over)} past its limit ({span(self.limit)}): next step "
                f"({self.next}) belongs to {self.owes}")

    def key(self) -> str:
        """What makes an alarm a new one: the job, its state, the step and who owes it."""
        return hashlib.sha256(f"{self.job}|{self.state}|{self.gate}|{self.owes}|{self.next}".encode()).hexdigest()[:16]


def span(t: dt.timedelta) -> str:
    h = t.total_seconds() / 3600
    return f"{h / 24:.0f} d" if h >= 48 else f"{h:.0f} h" if h >= 1 else f"{h * 60:.0f} min"


def _row(ev: Evidence, state: str, gate: str = "", why: str = "", since: dt.datetime | None = None,
         ahead: str = "", work: Work | None = None) -> Step:
    ln, w = ev.line, work or ev.work
    rule = TABLE.get((state, gate)) or TABLE[("*", gate)]
    owner = ln.owner if ln else ""
    stem = f"{prefixes(owner)[0]}{{}}-{ev.job.lower().replace('-', '')}" if owner else "{}"
    text = rule.next.format(job=ev.job, owner=owner, ver=ev.version, why=why, wip=stem.format("wip"),
                            integrate=stem.format("integrate"), ref=w.ref if w else "", sha=(w.sha if w else "")[:8],
                            pr=w.pr if w else 0, issue=ln.issue if ln else (ev.issues[0] if ev.issues else 0))
    owes = {OWNER: owner, AHEAD: ahead or INTEGRATOR}.get(rule.owes, rule.owes)
    return Step(ev.job, owner, state, gate, owes, text, why, since, rule.limit, ln.issue if ln else 0,
                ln.title if ln else "")


def clashes(ev: Evidence) -> list[str]:
    """The one conflict rule: every way the evidence can disagree with itself or with the plan."""
    ln, out = ev.line, ([] if ev.closed else list(ev.clashes))   # a closed job is not reopened by what a mail says
    if ln is None:   # a job only the evidence names: an issue, a head or a PR, or a mailed start (in ev.clashes)
        if ev.issues:
            out.append(f"open issue #{', #'.join(map(str, ev.issues))} is titled {ev.job} and the plan has no line for it")
        elif ev.heads or not out:
            out.append(f"unlanded work on {', '.join(sorted(h.ref for h in ev.heads)) or 'GitHub'} names {ev.job} "
                       "and the plan has no line for it")
        return out
    if len(ev.issues) > 1:
        out.append(f"two open issues are titled {ev.job}: #{' and #'.join(map(str, ev.issues))}")
    if not ev.closed and ln.issue not in ev.issues:
        out.append(f"the plan says #{ln.issue}; no open issue with that number is titled {ev.job}")
    foreign = sorted(h.ref for h in ev.heads if not owns(ln.owner, h.ref))
    if foreign:
        out.append(f"the plan gives it to {ln.owner}; unlanded work is also on {', '.join(foreign)}")
    if ev.closed and ev.heads:
        out.append(f"#{ln.issue} is closed with unlanded work on {', '.join(sorted(h.ref for h in ev.heads))}")
    return out


def step(ev: Evidence, lane: str = "", ahead: str = "") -> Step:
    """One job's state and next step, from its evidence alone. `lane`: why the owner holds no lane for it ("" when
    it does or needs none): the table grants lanes from every other job's state (table()). `ahead`: who owns the
    job it waits on."""
    row = _step(ev, ahead)
    if lane and (row.state == "started" or row.gate == "claimed"):
        # a wip head or a mailed start with no lane granted: refused a clock, never cut
        row = _row(ev, row.state, "no-lane", lane)
    elif lane and row.state in ("acknowledged", "landed") and not row.gate:
        row = _row(ev, row.state, "queued", lane, row.since)
    if ev.mail_unread and row.state in ("planned", "told", "acknowledged"):
        # the state is what the mail that was read shows; the thread that could prove a receipt or a start was not
        row = _row(ev, row.state, "mail-unread", str(ev.mail_unread), row.since)
    if ev.unpushed and row.state not in ("conflict", "closed", "landed", "held"):
        # the state stays what GitHub shows; the step is the push, and it is late at once (J-106: a job was
        # opened on a local "baseline" within minutes)
        return _row(ev, row.state, "unpushed", ev.unpushed[:12], ev.unpushed_at)
    return row


def _step(ev: Evidence, ahead: str) -> Step:
    found = clashes(ev)
    if found:
        return _row(ev, "conflict", why="; ".join(found))
    ln = ev.line
    if ev.closed:
        return _row(ev, "closed")
    prs = [w for w in ev.works if w.pr]
    if prs:   # an open PR lands on its own proof and reviews, held or not
        w = least(prs if ln.hold else ev.works)
        return _row(ev, w.state, w.gate, w.why, w.at, work=w)
    if ev.merged_at and not ev.heads:
        return _row(ev, "landed", since=ev.merged_at)
    if ln.hold:
        return _row(ev, "held", why=ln.hold)
    if ev.handover:   # one clock, the old owner's: the new owner starts nothing until it has handed the job over
        return _row(ev, "blocked", "handover", why=f'{ev.handover} pushes any lane it holds on {ev.job}, then puts '
                    f'"plan {ev.handover_ver}" in a mail subject', since=ev.handover_at or ev.planned_at,
                    ahead=ev.handover)
    if ev.works:
        w = ev.work
        return _row(ev, w.state, w.gate, w.why, w.at, work=w)
    wips = [h for h in ev.heads if h.wip]
    if wips:
        times = [h.at for h in wips if h.at]
        return _row(ev, "started", since=max(times) if times else None)
    if ev.waits:
        return _row(ev, "blocked", why=", ".join(ev.waits), ahead=ahead)
    told = ln.owner in SELF_TOLD or ev.receipt_at is not None
    state = "acknowledged" if told else "told" if ev.notice_at else "planned"
    if ev.claimed_at:   # a start that is only a mail: the state stays what the evidence shows, and a push is owed
        return _row(ev, state, "claimed", since=ev.claimed_at)
    if told:
        return _row(ev, state, since=ev.receipt_at or ev.planned_at)
    if ev.notice_at:
        return _row(ev, state, "reply" if ln.owner in COMMENT_OWNERS else "", since=ev.notice_at)
    return _row(ev, state, "" if ln.owner in NOTICE_OWNERS else "by-hand", since=ev.planned_at)


def table(model: P.Plan, evidence: dict[str, Evidence]) -> list[Step]:
    """Every job's row, in plan order, then the jobs the plan does not hold. Lanes are granted per owner, in plan
    order, before any other gate: its pushed work (an integrate head or a PR) always holds one; then each wip head
    and each mailed start while a lane is free (the rest: park, on no clock); then each acknowledged job and each
    landed part owed a next one (the rest: wait for a lane)."""
    owners = model.by_job()

    def row(job: str, lane: str = "") -> Step:
        ev = evidence[job]
        return step(ev, lane, ", ".join(sorted({owners[j].owner for j in ev.waits if j in owners})))

    rows = {job: row(job) for job in evidence}
    for o, limit in model.limits.items():
        lines = model.owner_lines(o)
        taken = sum(1 for ln in lines if rows[ln.job].state in COMMITTED)
        for ln in lines:
            r = rows[ln.job]
            if r.state == "started" or r.gate == "claimed":
                if taken < limit:
                    taken += 1
                else:
                    n = sum(1 for x in lines if flying(rows[x.job]))
                    rows[ln.job] = row(ln.job, f"{n} of {limit} lanes are in flight")
        n = sum(1 for x in lines if flying(rows[x.job]))
        for ln in lines:
            r = rows[ln.job]
            if r.state not in ("acknowledged", "landed") or r.gate:
                continue
            if taken < limit:
                taken += 1
            else:
                rows[ln.job] = row(ln.job, f"{n} of {limit} lanes are in flight" if n >= limit else
                                   f"the {limit - n} free lane(s) go to the jobs above it in the plan")
    return [rows[ln.job] for ln in model.lines] + [rows[j] for j in sorted(rows) if j not in owners]


def wip(model: P.Plan, rows: list[Step]) -> dict[str, tuple[int, int]]:
    """owner -> (jobs in flight, its limit). Real work over the limit is counted, never hidden."""
    return {o: (sum(1 for r in rows if r.owner == o and flying(r)), limit)
            for o, limit in sorted(model.limits.items())}


def alarms(rows: list[Step], now: dt.datetime) -> list[Step]:
    return [r for r in rows if r.late(now)]


def alarm_lines(rows: list[Step], now: dt.datetime) -> list[str]:
    return [line for _, line in alarm_groups(rows, now)]


def alarm_groups(rows: list[Step], now: dt.datetime) -> list[tuple[Step, str]]:
    """The late rows as (the row, its inbox line); jobs late on the very same notice, or on the same unread mail
    bound, share one line."""
    groups: dict[tuple, list[Step]] = {}
    for r in alarms(rows, now):
        same = ((r.gate, r.owes, r.next) if r.gate == "mail-unread" else
                (r.state, r.owes, r.next) if r.state in ("told", "planned") else (r.job,))
        groups.setdefault(same, []).append(r)
    out = []
    for group in groups.values():
        first = group[0]
        line = first.alarm(now)
        if len(group) > 1:
            line = line.replace(first.job, f"{first.job} and {len(group) - 1} more "
                                           f"({', '.join(r.job for r in group[1:6])}{', ...' if len(group) > 6 else ''})", 1)
        out.append((first, line))
    return out


# ---------- the readers: raw GitHub answers in, evidence out. Pure; the switch does the reading ----------
def order_text(model: P.Plan, owner: str) -> str:
    """What the integrator authored for `owner`: its limit and its lines in work order with their dependencies and
    holds, read from work/plan.md alone. Never a state, a title or an issue's text: a push, a PR, a merge or an
    edit to a job issue (a ticked checkbox) is not a changed order, so it must not mint a new version (and a new
    notice). A change of scope that matters goes on the plan line, the one place an order is written."""
    return "\n".join([f"limit {model.limits.get(owner, 0)}"] + [
        f"{ln.job} #{ln.issue} after={','.join(ln.after)} hold={ln.hold}" for ln in model.owner_lines(owner)])


def order_version(model: P.Plan, owner: str) -> str:
    return hashlib.sha256(order_text(model, owner).encode()).hexdigest()[:12]


def versions(model: P.Plan) -> dict[str, str]:
    """owner -> the version of its order."""
    return {o: order_version(model, o) for o in model.limits}


def line_hash(ln: P.Line) -> str:
    """One job's own order: its owner, issue, dependencies and hold. A job's clock runs from the first time its
    line, as it stands, was told; a change to any other line leaves it alone."""
    return hashlib.sha256(f"{ln.job} {ln.owner} #{ln.issue} after={','.join(ln.after)} hold={ln.hold}"
                          .encode()).hexdigest()[:8]


def job_issues(issues) -> dict[str, list[dict]]:
    """job -> the team's open issues titled with it (a PR is not one, nor is a stranger's issue: rules.team_issue)."""
    titled: dict[str, list[dict]] = {}
    for it in issues or []:
        job = rules.job_of_title(it.get("title") or "")
        if job and it.get("state", "open") == "open" and it.get("pull_request") is None and rules.team_issue(it):
            titled.setdefault(job, []).append(it)
    return titled


def when(stamp) -> dt.datetime | None:
    if isinstance(stamp, dt.datetime):
        return stamp
    try:
        return dt.datetime.fromisoformat((stamp or "").replace("Z", "+00:00")) if stamp else None
    except ValueError:
        return None


def work_of(f: "rules.PrFacts", d: "rules.MergeDecision", team: "rules.Team", *, proof_at="", codex_asked: bool = False,
            codex_woken: str = "", stacked: str = "") -> Work:
    """An open PR as the steer sweep read it (its facts and the merge rule's decision), as the machine's Work.
    Nothing is decided here that rules.merge_decision did not decide: this names the gate that is shut.
    codex_asked: the workflow bot's ask mark is on this head (its mention wakes nobody: the integrator posts).
    codex_woken: when a person's login asked Codex for this head ("" if none did): the review is then Codex's."""
    author = team.vendor_of_branch(f.head_ref)
    lands = sorted((v for v in f.verdicts if v.kind == "review" and v.verdict == "LAND"
                    and rules.names_head(v, f.head_sha) and rules.land_counts(v, author, team)),
                   key=lambda v: v.created_at)
    fixes = rules.open_fixes(f.verdicts, f.head_sha, f.head_committed_at, "review")
    if "eyes" in f.needs:
        fixes += rules.open_fixes(f.verdicts, f.head_sha, f.head_committed_at, "eye")
    pushed, proven = when(f.head_committed_at), when(proof_at) or when(f.head_committed_at)
    base = dict(ref=f.head_ref, sha=f.head_sha, proof=f.proof, pr=f.number)
    reasons = "; ".join(d.reasons)
    if f.base_ref != rules.TRUNK:
        return Work(**base, gate="off-trunk", why=f"it is stacked on {f.base_ref}", at=pushed)
    if f.mergeable is False:   # nothing else can move until trunk is merged in: the proof and the reviews are of an old base
        return Work(**base, land=bool(lands) and not fixes, gate="base", at=pushed)
    if fixes:
        newest = max(fixes, key=lambda v: v.created_at)
        return Work(**base, gate="stacked" if stacked else "fix",
                    why=stacked or ", ".join(sorted({f"{v.who} ({v.kind})" for v in fixes})),
                    at=when(newest.created_at) or proven)
    if f.proof != "green":
        gate = "red" if f.proof.startswith("red") else f.proof if f.proof in ("running", "none") else "unread"
        return Work(**base, gate=gate, why=f.proof[5:] if gate == "red" else "", at=when(proof_at) or pushed)
    if f.draft:
        return Work(**base, gate="draft", at=proven)
    if not lands:
        note = any(v.kind == "review" and v.verdict == "NOTE" and v.author_login == rules.BOT_LOGIN
                   and rules.names_head(v, f.head_sha) for v in f.verdicts)
        return Work(**base, gate="note" if note else "" if codex_woken else "codex" if codex_asked else "", at=proven)
    at = max(t for t in (proven, when(lands[0].created_at)) if t) if proven or when(lands[0].created_at) else None
    if d.action == "MERGE":
        return Work(**base, land=True, at=at)
    if "the Codex app's clean review" in reasons and codex_woken:
        return Work(**base, land=True, gate="asked", why=reasons, at=max(t for t in (at, when(codex_woken)) if t))
    gate = ("codex" if "the Codex app's clean review" in reasons else
            "security" if "no automatic Claude review on this head" in reasons else
            "eye" if "no eye LAND" in reasons else
            "hand" if d.action == "HUMAN-MERGE" or "rule_check BLOCK" in reasons else "wait")
    return Work(**base, land=True, gate=gate, why=reasons, at=at)


def branch_work(ref: str, sha: str, proof: str = "unread", at=None, auto_pr: bool = False) -> Work:
    """An integrate head with no open PR, by its proof alone."""
    gate = (("auto-pr" if auto_pr else "no-pr") if proof == "green" else "red" if proof.startswith("red") else
            proof if proof in ("running", "none") else "unread")
    return Work(ref, sha, proof if gate != "unread" else "unread", gate=gate, why=proof[5:] if gate == "red" else "",
                at=when(at))


# ---------- the mail side: what the bridge filed, read as evidence, never as an order ----------
BRIDGE_POST = "**Muse (via email bridge)"   # switch.BRIDGE_POST: the first line of a mail the bridge posted for Muse
HAND_MAIL = "CLAUDE_TO_MUSE:"               # the first line of a hand mail to Muse as the bridge records it
NOTICE_HEAD = "CLAUDE_TO_MUSE: plan "
NOTICE_MARK = "<!-- switchboard:plan owner={owner} ver={ver} jobs={jobs} -->"
NOTICE_SENT = re.compile(r"<!-- switchboard:plan owner=([a-z][a-z0-9-]*) ver=([0-9a-f]{12})(?: jobs=([A-Za-z0-9,:@!-]*))? -->")
# A mark's jobs: per job, `J-NNN:<line hash>@<first told>!<first acknowledged>` (mark_jobs). An old mark lists bare
# job ids; it proves only its whole version.
ENTRY = re.compile(r"(J-\d{3,}[a-z]?)(?::([0-9a-f]{8}))?(?:@(\d{8}T\d{4}Z))?(?:!(\d{8}T\d{4}Z))?")
NOTICE_AGAIN = "<!-- switchboard:plan-again owner={owner} ver={ver} -->"   # the one re-send of an unanswered version
# The switch's record of a self-told owner's version (Claude writes the plan, so nothing is mailed): the same mark
# under its own words, so each of its jobs has a first-seen time of its own.
SELF_HEAD = "Plan {ver} recorded for {owner}: no mail, the integrator writes the plan, so its own lines are known."
# The switch writes this into its own notice once it has seen the receipt, so the receipt outlives the window
# the mail is read in. It cites the comment that carried it; only the bot's login can have written it.
RECEIPT_MARK = "<!-- switchboard:plan-receipt owner={owner} ver={ver} at={at} comment={comment} -->"
RECEIPT_SEEN = re.compile(r"<!-- switchboard:plan-receipt owner=([a-z][a-z0-9-]*) ver=([0-9a-f]{12}) at=(\S+) comment=(\d+) -->")
PLAN_START, PLAN_END = "<!-- plan:start -->", "<!-- plan:end -->"   # the board body's job section
NOTICE_MAX = 1200    # characters. 11-12: Muse's safety check refused a long rulings mail; the file is the order
NOTICE_JOBS = 6      # job ids named per list in a notice; the rest are a count
JOB_ID = re.compile(r"\bJ-\d{3,}[a-z]?(?![\w])")
PLAN_VER = re.compile(r"\bplan\s+([0-9a-f]{12})\b", re.IGNORECASE)
STARTED = re.compile(r"\bstarted\b", re.IGNORECASE)
NOT_STARTED = re.compile(r"\b(?:not|never)\s+started\b", re.IGNORECASE)
# A commit as a mail names one: 7 to 12 hex or the full 40, with a digit and a letter in it. Never an obligation
# id (O-2c7fcb6a), a plan version (`plan <12 hex>`), or a mail or comment id (16 hex, or the word before says so).
NAMED_SHA = re.compile(r"(?<![\w-])(?<![Pp][Ll][Aa][Nn] )(?<![Mm]ail )(?<![Ii][Dd] )(?=[0-9a-f]*[0-9])(?=[0-9a-f]*[a-f])"
                       r"(?:[0-9a-f]{40}|[0-9a-f]{7,12})(?![\w-])")
# What a hand mail orders, by the words in its subject. Mail carries no orders: one that says other than the
# plan says is conflicting evidence, whichever of the two is right.
# Only an order's own words count: the game has a hold, a drop, a start and a build of its own.
TAKES = re.compile(r"\b(?:taking|taken by|not yours|reassign\w*)\b", re.IGNORECASE)
STOPS = re.compile(r"\b(?:stop (?:that|those|your|the) lanes?|parked|park it)\b", re.IGNORECASE)
GIVES = re.compile(r"\b(?:is yours|are yours|yours again|back to you|unparked)\b", re.IGNORECASE)


@dataclass(frozen=True)
class Post:
    id: int
    thread: int
    at: dt.datetime
    subject: str     # the first line, without the bridge's prefix or the markup
    body: str
    hand: bool       # True: a hand mail to the worker the bridge recorded; False: the worker's own mail


class Notice(NamedTuple):
    """One of the switch's plan marks: a notice it sent (a mail owner) or a version it recorded (a self-told one)."""
    owner: str
    ver: str
    at: dt.datetime
    jobs: tuple = ()   # (job, line hash, first told, first acknowledged) per job it lists; () when it lists none


@dataclass(frozen=True)
class Mail:
    """The filed mail as evidence. `posts`: the bridge's, oldest first. `notices`: the switch's plan marks (Notice,
    or a bare (owner, version, when)). `missing`: job -> a commit its owner's newest mail names that GitHub does
    not have. `worker`: whose mail the bridge files. `unread`: the jobs whose mail thread was past the reader's
    `bound` this pass: nothing on them is proven either way."""
    posts: tuple[Post, ...] = ()
    notices: tuple = ()
    missing: dict[str, str] = field(default_factory=dict)
    worker: str = "muse"
    receipts: dict[tuple[str, str], dt.datetime] = field(default_factory=dict)   # (owner, version) -> recorded receipt
    unread: frozenset[str] = frozenset()
    bound: int = 0


def posts(comments: list[dict], logins, bot: str = rules.BOT_LOGIN) -> list[Post]:
    """The bridge's posts among `comments`, oldest first: the worker's mails and the hand mails to it. Only from a
    team login (the repo is public: anyone can type the bridge's first line), and never the switch's own
    CLAUDE_TO_MUSE notices (the bot's login), which are not hand mail."""
    out = []
    for c in comments:
        login = ((c.get("user") or {}).get("login") or "").lower()
        body = (c.get("body") or "").lstrip()
        at = when(c.get("created_at") or "")
        hand = body.startswith(HAND_MAIL)
        if login not in logins or login == bot.lower() or at is None or not (hand or body.startswith(BRIDGE_POST)):
            continue
        first = body.split("\n", 1)[0]
        subject = first[len(HAND_MAIL):] if hand else first[len(BRIDGE_POST):].lstrip("):")
        try:
            thread = int((c.get("issue_url") or "").rsplit("/", 1)[1])
        except (ValueError, IndexError):
            thread = 0
        out.append(Post(int(c.get("id") or 0), thread, at, subject.strip(" *"), body, hand))
    return sorted(out, key=lambda p: (p.at, p.id))


def stamp(t: dt.datetime) -> str:
    return t.astimezone(dt.timezone.utc).strftime("%Y%m%dT%H%MZ")


def unstamp(s: str | None) -> dt.datetime | None:
    try:
        return dt.datetime.strptime(s, "%Y%m%dT%H%MZ").replace(tzinfo=dt.timezone.utc) if s else None
    except ValueError:
        return None


def entries(text: str | None) -> tuple:
    out = []
    for part in filter(None, (text or "").split(",")):
        m = ENTRY.fullmatch(part)
        if m:
            out.append((m.group(1), m.group(2) or "", unstamp(m.group(3)), unstamp(m.group(4))))
    return tuple(out)


def sent_notices(comments: list[dict], bot: str = rules.BOT_LOGIN) -> list[Notice]:
    """Every plan mark among `comments`, oldest first: only the switch's own login can have written one."""
    out = []
    for c in comments:
        at = when(c.get("created_at") or "")
        if ((c.get("user") or {}).get("login") or "") == bot and at:
            out += [Notice(m.group(1), m.group(2), at, entries(m.group(3)))
                    for m in NOTICE_SENT.finditer(c.get("body") or "")]
    return sorted(out, key=lambda n: n.at)


def notice_comment(comments: list[dict], owner: str, ver: str, bot: str = rules.BOT_LOGIN) -> dict | None:
    """The switch's own comment that carried the notice of this version to this owner."""
    return next((c for c in comments if ((c.get("user") or {}).get("login") or "") == bot and any(
        m.group(1) == owner and m.group(2) == ver for m in NOTICE_SENT.finditer(c.get("body") or ""))), None)


def notice_jobs(comments: list[dict], owner: str, bot: str = rules.BOT_LOGIN) -> list[str] | None:
    """The jobs the newest notice to `owner` listed as its own; None when no notice says."""
    found = None
    for c in sorted(comments, key=lambda c: c.get("created_at") or ""):
        if ((c.get("user") or {}).get("login") or "") == bot:
            for m in NOTICE_SENT.finditer(c.get("body") or ""):
                if m.group(1) == owner and m.group(3) is not None:
                    found = [e[0] for e in entries(m.group(3))]
    return found


def recorded_receipts(comments: list[dict], bot: str = rules.BOT_LOGIN) -> dict[tuple[str, str], dt.datetime]:
    """(owner, version) -> when its receipt came, as the switch recorded it on its own notice."""
    out = {}
    for c in comments:
        if ((c.get("user") or {}).get("login") or "") == bot:
            for m in RECEIPT_SEEN.finditer(c.get("body") or ""):
                if when(m.group(3)):
                    out[(m.group(1), m.group(2))] = when(m.group(3))
    return out


def notice_head(owner: str, ver: str) -> str:
    """A notice's first line: `To <Name>: plan <ver>` for a comment owner (its runs read only comments that open
    with its address), the bridge's `CLAUDE_TO_MUSE: plan <ver>` for a mail owner."""
    return f"To {COMMENT_OWNERS[owner]}: plan {ver}" if owner in COMMENT_OWNERS else f"{NOTICE_HEAD}{ver}"


def comment_receipts(comments: list[dict], mail: Mail, vers: dict[str, str], logins, author,
                     bot: str = rules.BOT_LOGIN) -> dict[tuple[str, str], tuple[dt.datetime, int]]:
    """(comment owner, its present version) -> (when, comment id) of its receipt: the first comment after that
    version's notice, from a team login (never the switch's own), written by one of COMMENT_AUTHORS (`author`:
    asks.author, the signed first line) and naming `plan <version>` near its top. Pure: the caller reads."""
    team = {str(x).lower() for x in logins} - {bot.lower()}
    out: dict[tuple[str, str], tuple[dt.datetime, int]] = {}
    for owner in COMMENT_OWNERS:
        ver = vers.get(owner)
        sent = notice_at(mail, owner, ver) if ver else None
        if sent is None:
            continue
        for c in sorted(comments, key=lambda c: (c.get("created_at") or "", c.get("id") or 0)):
            at = when(c.get("created_at") or "")
            body = c.get("body") or ""
            if (at is None or at < sent or ((c.get("user") or {}).get("login") or "").lower() not in team
                    or author(body) not in COMMENT_AUTHORS):
                continue
            if ver in {v.lower() for v in PLAN_VER.findall(body[:600])}:
                out[(owner, ver)] = (at, int(c.get("id") or 0))
                break
    return out


def receipt_post(mail: Mail, ver: str, sent: dt.datetime) -> Post | None:
    """The worker's first mail after the notice that carries `plan <ver>`: the proof that version was read. Read
    from the subject and the body; a version is 12 hex after the word plan, never a commit."""
    return next((p for p in mail.posts if not p.hand and p.at >= sent
                 and ver in {v.lower() for v in PLAN_VER.findall(p.subject + "\n" + p.body)}), None)


def marks(mail: Mail, owner: str | None = None) -> list[Notice]:
    """The switch's plan marks, oldest first (a bare (owner, version, when) is a mark that lists no job)."""
    out = [n if isinstance(n, Notice) else Notice(*n) for n in mail.notices]
    return sorted((n for n in out if owner is None or n.owner == owner), key=lambda n: n.at)


def notice_at(mail: Mail, owner: str, ver: str) -> dt.datetime | None:
    return next((n.at for n in marks(mail, owner) if n.ver == ver), None)


def receipt_at(mail: Mail, owner: str, ver: str, sent: dt.datetime) -> dt.datetime | None:
    """When the owner's receipt for this version came: the one the switch recorded, else the mail itself."""
    post = receipt_post(mail, ver, sent) if owner == mail.worker else None
    return mail.receipts.get((owner, ver)) or (post.at if post else None)


def _entry(n: Notice, job: str) -> tuple | None:
    return next((e for e in n.jobs if e[0] == job), None)


def covers(n: Notice, job: str, h: str, ver: str) -> bool:
    """The mark told this job's line as it stands: its entry carries the line's hash. A mark with no hashes (an
    old one) proves only its whole version."""
    if n.jobs and all(e[1] for e in n.jobs):
        return (_entry(n, job) or ("", ""))[1] == h
    return n.ver == ver


def told(mail: Mail, owner: str, job: str, h: str, ver: str) -> tuple[dt.datetime | None, dt.datetime | None]:
    """(first told, first acknowledged) of this job's line as it stands: over the newest unbroken run of the
    owner's marks that cover it. A change of this line breaks the run (back to an old text is a new run too); a
    change of any other line does not. Each mark carries the run's times forward, so a mark on a closed board is
    never needed. A self-told owner's mark is its own receipt."""
    run = []
    for n in reversed(marks(mail, owner)):
        if not covers(n, job, h, ver):
            break
        run.append(n)
    if not run:
        return None, None
    first = min((_entry(n, job) or (0, 0, None))[2] or n.at for n in run)
    acks = [e[3] for n in run if (e := _entry(n, job)) and e[3]]
    acks += [r for n in run if (r := receipt_at(mail, owner, n.ver, n.at))]
    if owner in SELF_TOLD:
        acks.append(first)
    return first, min(acks, default=None)


def mark_jobs(model: P.Plan, mail: Mail, owner: str, now: dt.datetime, planned_at: dt.datetime | None = None) -> str:
    """The `jobs=` of a new mark for `owner`: each of its lines with its hash and when it was first told and
    acknowledged, carried forward from the marks before it. A line told for the first time is told now (a mail
    owner's notice) or when the plan last changed (a self-told owner's record, written on the next pass)."""
    ver, out = order_version(model, owner), []
    for ln in model.owner_lines(owner):
        h = line_hash(ln)
        first, ack = told(mail, owner, ln.job, h, ver)
        first = first or (now if owner not in SELF_TOLD else min(t for t in (planned_at, now) if t))
        out.append(f"{ln.job}:{h}@{stamp(first)}" + (f"!{stamp(ack)}" if ack else ""))
    return ",".join(out)


def handover(mail: Mail, ln: P.Line, vers: dict[str, str]) -> tuple[str, str, dt.datetime | None] | None:
    """(old owner, its present version, when the notice that dropped the job went) while a mail owner the job was
    moved from has not answered a notice that no longer lists it (J-109, J-038 from the other side: both built it).
    None once it has, or when no mail owner's notice ever listed the job as its own."""
    for o in sorted(MAIL_OWNERS - {ln.owner}):
        mine = marks(mail, o)
        listed = [n for n in mine if _entry(n, ln.job)]
        if not listed:
            continue
        later = [n for n in mine if n.at > listed[-1].at]
        if not any(receipt_at(mail, o, n.ver, n.at) for n in later):
            return o, vers.get(o, ""), (later[0].at if later else None)
    return None


def _latest(*times) -> dt.datetime | None:
    return max((t for t in times if t), default=None)


def started_posts(mail: Mail, job_of=lambda j: j) -> dict[str, list[dt.datetime]]:
    """job -> when the worker said it started, oldest first (a mail whose subject names the job and says
    "started"). `job_of`: the job an id is (a lettered split is its base line's job)."""
    out: dict[str, list[dt.datetime]] = {}
    for p in mail.posts:
        if not p.hand and STARTED.search(p.subject) and not NOT_STARTED.search(p.subject):
            for job in dict.fromkeys(job_of(j) for j in JOB_ID.findall(p.subject)):
                out.setdefault(job, []).append(p.at)
    return out


def answered(mail: Mail, job_of=lambda j: j) -> dict[str, dt.datetime]:
    """job -> the newest hand mail that names it: what the worker was last told of the job by hand."""
    out: dict[str, dt.datetime] = {}
    for p in mail.posts:
        if p.hand:
            for job in JOB_ID.findall(p.subject):
                out[job_of(job)] = p.at
    return out


def claim_after(times: list[dt.datetime], cut: dt.datetime | None) -> dt.datetime | None:
    """The first start mailed after `cut` (the job's newest notice or ruling): an older one is history."""
    return next((t for t in times if cut is None or t > cut), None)


def named_shas(mail: Mail, by_thread: dict[int, str]) -> dict[str, tuple[Post, tuple[str, ...]]]:
    """job -> (its owner's newest mail that names a commit, the commits it names). The job is the one the
    subject names, else the job whose issue the bridge filed the mail on."""
    out: dict[str, tuple[Post, tuple[str, ...]]] = {}
    for p in mail.posts:
        shas = tuple(dict.fromkeys(NAMED_SHA.findall(p.body)))
        if p.hand or not shas:
            continue
        jobs = list(dict.fromkeys(JOB_ID.findall(p.subject))) or [by_thread.get(p.thread, "")]
        for job in filter(None, jobs[:1]):
            out[job] = (p, shas)
    return out


def mail_clashes(model: P.Plan, mail: Mail, planned_at: dt.datetime | None = None) -> dict[str, list[str]]:
    """job -> what the mail says that the plan does not hold. The worker mailed a start on a job the plan gives
    someone else, holds, or has no line for (J-109, J-038: taken by mail, built twice). The newest hand mail on a
    job orders other than the plan says (an order that lives only in a mail). A start the worker has since been
    answered on (a later hand mail on the job, or a later plan notice) is history, and so is a hand mail older
    than the plan's last commit (`planned_at`): the plan was written after it (J-001, J-040, J-043, J-081 at the
    11-12 sync)."""
    out: dict[str, list[str]] = {}
    by_job = model.by_job()

    def job_of(j: str) -> str:
        return P.plan_job(j, by_job)

    last = _latest(*(n.at for n in marks(mail, mail.worker)))
    hands = answered(mail, job_of)
    for job, times in started_posts(mail, job_of).items():
        if claim_after(times, _latest(hands.get(job), last)) is None:
            continue
        ln = by_job.get(job)
        if ln is None:
            out.setdefault(job, []).append(f"{mail.worker} mailed that it started {job}; the plan has no line for it")
        elif ln.owner != mail.worker:
            out.setdefault(job, []).append(f"{mail.worker} mailed that it started; the plan gives it to {ln.owner}")
        elif ln.hold:
            out.setdefault(job, []).append(f"{mail.worker} mailed that it started; the plan holds it ({ln.hold[:60]})")
    newest: dict[str, Post] = {}   # the newest hand mail that orders something on the job: a later mail that
    for p in mail.posts:           # only mentions it changes nothing (J-109: the taking, then a rulings mail)
        if planned_at and p.at < planned_at:
            continue
        if p.hand and (TAKES.search(p.subject) or STOPS.search(p.subject) or GIVES.search(p.subject)):
            for job in dict.fromkeys(job_of(j) for j in JOB_ID.findall(p.subject)):
                newest[job] = p
    for job, p in newest.items():
        ln = by_job.get(job)
        if ln is None:
            continue
        said = f'a hand mail says "{p.subject[:70]}" (comment {p.id})'
        mine = ln.owner == mail.worker
        if TAKES.search(p.subject) and mine:
            out.setdefault(job, []).append(f"{said}; the plan still gives it to {ln.owner}")
        elif STOPS.search(p.subject) and mine and not ln.hold:
            out.setdefault(job, []).append(f"{said}; the plan neither holds it nor moves it")
        elif GIVES.search(p.subject) and not TAKES.search(p.subject) and not STOPS.search(p.subject) \
                and (not mine or ln.hold):
            out.setdefault(job, []).append(f"{said}; the plan {'holds it' if mine else 'gives it to ' + ln.owner}")
    return out


def _unpushed(ln: P.Line, mail: Mail, named: tuple[Post, tuple[str, ...]] | None, mine: list[Head]) -> dict:
    """The commit the worker's newest mail on its own job names and GitHub does not have, until a head of the
    job is pushed after that mail (what it does next is then seen on the remote)."""
    sha = mail.missing.get(ln.job, "")
    if not sha or ln.owner != mail.worker:
        return {}
    said = named[0].at if named else None
    if said and any(h.at and h.at > said for h in mine):
        return {}
    return {"unpushed": sha, "unpushed_at": said}


def collect(model: P.Plan, reads: dict, *, works: dict[str, Work] | None = None, mail: Mail | None = None,
            planned_at: dt.datetime | None = None) -> dict[str, Evidence]:
    """One Evidence per job the evidence names: every plan line, and every job with no line that an open issue,
    a head proven unlanded, an open PR or a mailed start names (a lettered id is its base line's job when only
    the base has one). `reads` is plan.observe's answer: the open issues, every branch, the open PRs, `inactive`
    (landed heads), `merged_at` (job -> when a `Job:` PR of it merged), `head_at` (sha -> its commit time, for the
    heads a compare proved unlanded) and `closed` (the jobs whose plan issue was read closed). `works`: head sha ->
    its Work, for the heads whose proof the switch read this pass. A head of a job with no line, no base line, no
    open issue and no PR (a job tidied out of the plan) is a row only once a compare proved it unlanded."""
    works, mail = works or {}, mail or Mail()
    by_job = model.by_job()

    def job_of(j: str) -> str:
        return P.plan_job(j, by_job)

    titled = job_issues(reads.get("issues"))
    inactive, head_at = reads.get("inactive") or frozenset(), reads.get("head_at") or {}
    merged: dict[str, dt.datetime] = {}
    for j, stamp_ in (reads.get("merged_at") or {}).items():
        t = when(stamp_)
        if t and (job_of(j) not in merged or t > merged[job_of(j)]):
            merged[job_of(j)] = t
    heads: dict[str, list[Head]] = {}
    for b in reads.get("branches") or []:
        name, sha = P.branch_name(b), P.branch_sha(b)
        job = job_of(P.job_of_branch(name))
        ruled = by_job[job].kept if job in by_job else ()   # a head the plan keeps as a record is not work in flight
        if not job or (sha and sha in inactive) or any(name == ref and sha and sha.startswith(pin) for ref, pin in ruled):
            continue
        head = Head(name, sha, head_at.get(sha))
        if head.wip and head.at and job in merged and head.at < merged[job]:
            continue   # a wip head older than the job's merge is history: its commits were squashed into the PR
        if job not in by_job and job not in titled and head.at is None:
            continue   # a head of a job out of the plan that no compare proved unlanded (observe says how many)
        heads.setdefault(job, []).append(head)
    prs: dict[str, list[dict]] = {}
    for p in reads.get("prs") or []:
        ref, sha = (p.get("head") or {}).get("ref") or "", (p.get("head") or {}).get("sha") or ""
        m = P.PR_JOB.match(p.get("body") or "")
        job = job_of(m.group(1) if m else P.job_of_branch(ref))
        if job and p.get("state", "open") != "closed":
            prs.setdefault(job, []).append(p)
            if not any(h.ref == ref for h in heads.get(job, [])):
                heads.setdefault(job, []).append(Head(ref, sha, head_at.get(sha)))
    closed = reads.get("closed") or frozenset()
    vers = versions(model)
    wrong = mail_clashes(model, mail, planned_at)
    starts, hands = started_posts(mail, job_of), answered(mail, job_of)
    named = {job_of(j): v for j, v in named_shas(mail, {ln.issue: ln.job for ln in model.lines}).items()}
    out: dict[str, Evidence] = {}
    for ln in model.lines:
        mine = [h for h in heads.get(ln.job, []) if owns(ln.owner, h.ref)]
        ws, refs = [], set()
        for pr in sorted(prs.get(ln.job, []), key=lambda p: int(p.get("number") or 0)):   # one Work per open PR
            ref, sha = (pr.get("head") or {}).get("ref") or "", (pr.get("head") or {}).get("sha") or ""
            refs.add(ref)
            if owns(ln.owner, ref):
                ws.append(works.get(sha) or Work(ref, sha, gate="unread", pr=int(pr.get("number") or 0)))
        stacked = {w.why for w in ws if w.gate == "stacked"}   # the branch a FIX's answer sits on is that PR's step
        for h in mine:   # and one per integrate head with no PR
            if not h.wip and h.ref not in refs and h.ref not in stacked:
                ws.append(works.get(h.sha) or branch_work(h.ref, h.sha, at=h.at))
        ver, h = vers.get(ln.owner, ""), line_hash(ln)
        first, ack = told(mail, ln.owner, ln.job, h, ver) if ln.owner in NOTICE_OWNERS | SELF_TOLD else (None, None)
        if ln.owner in SELF_TOLD:
            first, ack = None, ack or planned_at
        moved = handover(mail, ln, vers)
        claim = (claim_after(starts.get(ln.job, []), _latest(first or planned_at, hands.get(ln.job)))
                 if ln.owner == mail.worker else None)
        out[ln.job] = Evidence(
            job=ln.job, line=ln, issues=tuple(sorted(int(i["number"]) for i in titled.get(ln.job, []))),
            closed=ln.job in closed,
            waits=tuple(j for j in ln.after if j not in closed and (j in by_job or j in titled)),
            version=ver, planned_at=planned_at, notice_at=first, receipt_at=ack, claimed_at=claim,
            **_unpushed(ln, mail, named.get(ln.job), mine),
            heads=tuple(heads.get(ln.job, [])), works=tuple(ws), merged_at=merged.get(ln.job),
            clashes=tuple(wrong.get(ln.job, [])),
            **(dict(handover=moved[0], handover_ver=moved[1], handover_at=moved[2]) if moved else {}),
            mail_unread=mail.bound if ln.owner == mail.worker and ln.job in mail.unread else 0)
    for job in sorted((set(titled) | set(heads) | set(wrong)) - set(by_job)):
        out[job] = Evidence(job=job, issues=tuple(sorted(int(i["number"]) for i in titled.get(job, []))),
                            heads=tuple(heads.get(job, [])), clashes=tuple(wrong.get(job, [])))
    return out


# ---------- the views: labels, the board, a worker's queue and its notice. Text and diffs only; the switch
# writes them, and only when work/switchboard.yml says `plan: live` ----------
PARKED = "state:parked"   # reconciler.PARKED_LABEL: the one state label kept, made from a hold
WAITING = "state:waiting"   # the owner asked someone and the job waits on the answer (waits)


class Wait(NamedTuple):
    """A job waiting on an answer: its owner's oldest open ask that blocks it (asks.blocking)."""
    to: str              # the addressee, as the ask names it: Claude, Muse, Grok, ...
    url: str
    asked: dt.datetime
    thread: int


def waits(model: P.Plan, open_asks: list[dict], thread_job: dict[int, str], who) -> dict[str, Wait]:
    """job -> the oldest open ask its owner posted that it waits on. `open_asks`: asks.unanswered's dicts (`a` the
    asker, `target`, `n`, `t`, `url`, `body`), each one already judged open and blocking by the caller.
    `thread_job`: a job's own issue and PR numbers -> its job. `who`: a name -> its agent key (Builder and PE are
    one), so an ask counts only when its asker is the job's owner and it is addressed to someone else. An ask on
    the job's own thread is its job's; elsewhere (the board) it is the job its opening names, when it names one."""
    by_job = model.by_job()
    out: dict[str, Wait] = {}
    for a in sorted(open_asks, key=lambda a: a["t"]):
        named = sorted({P.plan_job(j, by_job) for j in JOB_ID.findall((a.get("body") or "")[:300])} & set(by_job))
        job = thread_job.get(a["n"]) or (named[0] if len(named) == 1 else "")
        ln = by_job.get(job)
        if ln is None or job in out or who(a["a"]) != who(ln.owner) or who(a["target"]) == who(ln.owner):
            continue
        out[job] = Wait(a["target"], a.get("url") or "", a["t"], a["n"])
    return out


def wait_text(w: Wait, now: dt.datetime) -> str:
    return f"waiting on {w.to}'s answer ([ask]({w.url}), {span(now - w.asked)})" if w.url else \
        f"waiting on {w.to}'s answer (#{w.thread}, {span(now - w.asked)})"


def labels(model: P.Plan, issues: list[dict],
           aliases: dict[str, frozenset[str]] | None = None,
           waiting=frozenset()) -> list[tuple[int, tuple[str, ...], tuple[str, ...]]]:
    """(issue, labels to add, labels to remove) for every line whose own open issue carries labels the plan does
    not say: `job`, one owner label, `state:parked` exactly when held, `state:waiting` exactly while the job is in
    `waiting` (it waits on an answer, waits()); every other `owner:` and `state:` label goes. `aliases`: per owner, the labels work/agents.yml counts as that owner's (owner:claude-lane-a is
    Claude's): one already there is left alone. A line whose issue number is wrong gets nothing (it is a
    conflict): a label is never written to an issue the plan cannot prove is the job's."""
    out = []
    by_number = {i.get("number"): i for i in issues
                 if i.get("state", "open") == "open" and i.get("pull_request") is None}
    for ln in model.lines:
        issue = by_number.get(ln.issue)
        if issue is None or rules.job_of_title(issue.get("title") or "") != ln.job:
            continue
        have = {(lab.get("name") if isinstance(lab, dict) else lab) or "" for lab in issue.get("labels") or []}
        own = (set((aliases or {}).get(ln.owner) or ()) | {f"owner:{ln.owner}"}) if ln.owner != "unassigned" else set()
        want = {"job"} | ({PARKED} if ln.hold else set()) | ({WAITING} if ln.job in waiting else set())
        if own and not have & own:
            want.add(f"owner:{ln.owner}")
        add = tuple(sorted(want - have))
        remove = tuple(sorted(lab for lab in have - want - own if lab.startswith(("owner:", "state:"))))
        if add or remove:
            out.append((ln.issue, add, remove))
    return out


def board(model: P.Plan, rows: list[Step], now: dt.datetime, waiting: dict[str, Wait] | None = None) -> str:
    """The board's job section: per owner its lanes, then every job that is not closed with its state and step, and
    what it waits on when its owner's ask is open (waits())."""
    lanes = wip(model, rows)
    out = ["### Jobs (made from work/plan.md and what GitHub shows; nobody edits this by hand)", ""]
    for owner in sorted({r.owner for r in rows if r.owner}):
        mine = [r for r in rows if r.owner == owner and r.state != "closed"]
        n, limit = lanes.get(owner, (0, 0))
        out.append(f"**{owner}**: {n} of {limit} lanes in flight")
        for r in mine:
            late = " **LATE**" if r.late(now) else ""
            who = f"; next: {r.owes}, {r.next}" if r.owes else f"; {r.next}"
            wait = f"; {wait_text((waiting or {})[r.job], now)}" if r.job in (waiting or {}) else ""
            out.append(f"- {r.job} #{r.issue} {r.state}{late}{wait}{who}")
        out.append("")
    strays = [r for r in rows if not r.owner]
    if strays:
        out.append("**not in the plan**")
        out += [f"- {r.job} {r.state}: {r.why}" for r in strays]
    return "\n".join(out).rstrip() + "\n"


def splice(body: str, text: str) -> str | None:
    """The board body with its job section replaced (added at the end when it has none); None when the markers
    are broken, so nothing a person wrote is overwritten."""
    section = f"{PLAN_START}\n{text.rstrip()}\n{PLAN_END}"
    starts, ends = body.count(PLAN_START), body.count(PLAN_END)
    if (starts, ends) == (0, 0):
        return f"{body.rstrip()}\n\n{section}\n" if body.strip() else section + "\n"
    if (starts, ends) != (1, 1) or body.index(PLAN_START) > body.index(PLAN_END):
        return None
    return body[:body.index(PLAN_START)] + section + body[body.index(PLAN_END) + len(PLAN_END):]


def queue(model: P.Plan, rows: list[Step], owner: str) -> str:
    """A worker's queue, the view that replaces the hand-kept list in work/tasks/<worker>.md: what it has in
    flight, what it may start now, what waits, what is held and what is not its to touch."""
    mine = [r for r in rows if r.owner == owner]
    n, limit = wip(model, rows).get(owner, (0, 0))

    def some(state_ok) -> str:
        return ", ".join(f"{r.job} #{r.issue}" for r in mine if state_ok(r)) or "none"

    return "\n".join([
        f"In flight ({n} of {limit}): " + some(flying),
        "Park, no lane is granted: " + some(lambda r: r.gate == "no-lane"),
        "Start next, in this order: " + some(lambda r: r.state == "acknowledged" and r.gate == ""),
        "Waiting for a lane or the notice: " + some(lambda r: r.state in ("planned", "told") or r.gate == "queued"),
        "Blocked on another job: " + some(lambda r: r.state == "blocked"),
        "Held, not to be worked: " + some(lambda r: r.state == "held"),
        "Landed a part; the next part (or the close) is owed: " + some(lambda r: r.state == "landed"),
        "With the integrator (conflicting evidence): " + some(lambda r: r.state == "conflict"),
    ]) + "\n"


def _some(jobs: list[str]) -> str:
    return ", ".join(jobs[:NOTICE_JOBS]) + (f" and {len(jobs) - NOTICE_JOBS} more" if len(jobs) > NOTICE_JOBS else "")


def notice(model: P.Plan, rows: list[Step], owner: str, ver: str, before: list[str] | None = None,
           again: bool = False) -> str:
    """The one mail a plan version sends: short, plain, and not an order. It says the file changed and where the
    order is; what is newly or no longer the owner's since the last notice (`before`: that notice's jobs, None
    when unknown); what is held; and the next jobs only while a lane is free: a held, blocked or over-limit job
    is never named as work to start. `again`: the re-send of an unanswered version, two lines."""
    head = notice_head(owner, ver)
    receipt = (f'Receipt: reply on this thread, first line `To Claude (from {COMMENT_OWNERS[owner]}): plan {ver}`.'
               if owner in COMMENT_OWNERS else f'Receipt: put "plan {ver}" in the subject of your next MUSE_TO_CLAUDE mail.')
    if again:
        return "\n".join([head, "", f"Sent again: plan {ver} is on ScraperX-Claude in work/plan.md and has no "
                          "receipt from you yet. Pull trunk and read your lines there.", receipt])
    mine = [r for r in rows if r.owner == owner]
    jobs = [r.job for r in mine]
    lines = [head, "", "work/plan.md on ScraperX-Claude changed. Pull trunk and read your lines there, top to "
             "bottom: that file is the order, this mail is not."]
    if before is not None:
        new, gone = [j for j in jobs if j not in before], [j for j in before if j not in jobs]
        if new:
            lines.append(f"Newly yours: {_some(new)}.")
        if gone:
            lines.append(f"No longer yours: {_some(gone)}. If a lane holds work on one, push it, mail the branch "
                         "and sha, and stop that lane.")
    held = [r.job for r in mine if r.state == "held"]
    if held:
        lines.append(f"Held, not to be worked: {_some(held)} (the reason is on each line).")
    n, limit = wip(model, rows).get(owner, (0, 0))
    free = limit - n
    ready = [r.job for r in mine if r.state in ("planned", "told", "acknowledged") and r.gate in ("", "queued")][:max(free, 0)]
    if ready:
        lines.append(f"In flight: {n} of {limit}. Next, in order: {_some(ready)}. A lane starts with a pushed "
                     f"{prefixes(owner)[0]}wip-<job> branch.")
    else:
        lines.append(f"In flight: {n} of {limit}. " + ("At or over the limit: land, review or unblock what is "
                     "open; open no new lane." if free <= 0 else "Nothing is ready for a new lane."))
    lines.append(receipt)
    text = "\n".join(lines)
    return text if len(text) <= NOTICE_MAX else "\n".join(lines[:3] + lines[-2:])
