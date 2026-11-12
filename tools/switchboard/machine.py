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
  landed        the job's PR merged and no unlanded head is left; the issue is still open
  closed        the issue is closed
and beside them:
  held          the plan holds it (the reason is on its line); an open PR still lands on its own proof and review
  blocked       a job it comes `after` is not closed
  conflict      the evidence disagrees with itself or with the plan; one rule, and the next step is the integrator's
"""
from __future__ import annotations

from dataclasses import dataclass, field
import datetime as dt
import hashlib
import re

import plan as P
import rules

STATES = ("planned", "told", "acknowledged", "started", "pushed", "proven", "reviewed", "landed", "closed",
          "held", "blocked", "conflict")
IN_FLIGHT = frozenset({"started", "pushed", "proven", "reviewed"})   # what an owner's limit counts
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
    ("*", "unpushed"): Rule(OWNER, NOW, "push {why} to GitHub: a mail names it and GitHub does not have it, so it "
                                        "proves nothing and nobody may act on it"),
    ("*", "codex"): Rule(INTEGRATOR, hours(1), "post `@codex review` on #{pr} (only a person's login wakes Codex)"),
    ("*", "asked"): Rule(REVIEWER, hours(3), "review #{pr} @ {sha}: Codex was asked, and the merge waits for its clean review"),
    ("*", "fix"): Rule(OWNER, hours(6), "push a head to {ref} that answers the open FIX on #{pr} ({why})"),
    ("*", "stacked"): Rule(OWNER, hours(6), "push the fix onto {ref}: it sits on {why}, which waits for #{pr} to land"),
    ("*", "base"): Rule(OWNER, hours(3), "merge trunk into {ref} and push: #{pr} conflicts with the base"),
    ("*", "draft"): Rule(OWNER, hours(24), "mark #{pr} ready for review"),
    ("*", "off-trunk"): Rule(OWNER, hours(24), "point #{pr} at trunk: {why}"),
    ("landed", ""): Rule(INTEGRATOR, hours(24), "close #{issue}, or write on it what is left"),
    ("closed", ""): Rule(INTEGRATOR, None, "take its line out of the plan (tools/board/plan.py tidy)"),
    ("held", ""): Rule(NOBODY, None, "none while it is held: {why}"),
    ("blocked", ""): Rule(AHEAD, None, "land {why} first"),
    ("conflict", ""): Rule(INTEGRATOR, NOW, "rule it: {why}"),
}

# Who is told its order, and how. A mail owner gets the versioned notice through the bridge and answers with a
# receipt. The integrator writes the plan, so Claude's own lines need no notice. Any other owner has no channel
# the machine can prove: its jobs stay `planned` and the integrator tells it by hand.
MAIL_OWNERS = frozenset({"muse"})
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
    planned_at: dt.datetime | None = None      # when work/plan.md last changed on trunk
    notice_at: dt.datetime | None = None       # the notice of that version was sent
    receipt_at: dt.datetime | None = None      # the owner's `plan <version>` came back
    claimed_at: dt.datetime | None = None      # the owner mailed "started"
    unpushed: str = ""                         # a commit the owner's newest mail names that GitHub does not have
    unpushed_at: dt.datetime | None = None     # when that mail was filed
    heads: tuple[Head, ...] = ()               # every live head that names the job, whoever pushed it
    work: Work | None = None                   # the owner's proof-bearing head
    merged_at: dt.datetime | None = None       # a `Job:` PR of it merged
    clashes: tuple[str, ...] = ()              # evidence that disagrees, as the readers found it


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
         ahead: str = "") -> Step:
    ln, w = ev.line, ev.work
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
    if ln is None:
        return out + [f"open issue #{', #'.join(map(str, ev.issues))} is titled {ev.job} and the plan has no line for it"]
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
    """One job's state and next step, from its evidence alone. `lane`: why the owner may not open a lane for it
    now ("" when it may): the table decides that from every other job's state (table()). `ahead`: who owns the
    job it waits on."""
    row = _step(ev, lane, ahead)
    if ev.unpushed and row.state not in ("conflict", "closed", "landed", "held"):
        # the state stays what GitHub shows; the step is the push, and it is late at once (J-106: a job was
        # opened on a local "baseline" within minutes)
        return _row(ev, row.state, "unpushed", ev.unpushed[:12], ev.unpushed_at)
    return row


def _step(ev: Evidence, lane: str, ahead: str) -> Step:
    found = clashes(ev)
    if found:
        return _row(ev, "conflict", why="; ".join(found))
    ln, w = ev.line, ev.work
    if ev.closed:
        return _row(ev, "closed")
    if w and w.pr:   # an open PR lands on its own proof and reviews, held or not
        return _row(ev, w.state, w.gate, w.why, w.at)
    if ev.merged_at and not ev.heads:
        return _row(ev, "landed", since=ev.merged_at)
    if ln.hold:
        return _row(ev, "held", why=ln.hold)
    if w:
        return _row(ev, w.state, w.gate, w.why, w.at)
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
        return _row(ev, state, "queued" if lane else "", lane, ev.receipt_at or ev.planned_at)
    if ev.notice_at:
        return _row(ev, state, since=ev.notice_at)
    return _row(ev, state, "" if ln.owner in MAIL_OWNERS else "by-hand", since=ev.planned_at)


def table(model: P.Plan, evidence: dict[str, Evidence]) -> list[Step]:
    """Every job's row, in plan order, then the jobs the plan does not hold. An owner's limit counts its jobs in
    started..reviewed; only as many acknowledged jobs as it has free lanes, in plan order, are owed a start."""
    owners = model.by_job()
    rows = {job: step(ev, ahead=", ".join(sorted({owners[j].owner for j in ev.waits if j in owners})))
            for job, ev in evidence.items()}
    flying = {o: sum(1 for ln in model.owner_lines(o) if rows[ln.job].state in IN_FLIGHT) for o in model.limits}
    free = {o: model.limits[o] - flying[o] for o in model.limits}
    for ln in model.lines:
        row = rows[ln.job]
        if row.state != "acknowledged" or row.gate:
            continue
        if free.get(ln.owner, 0) > 0:
            free[ln.owner] -= 1
        else:
            n, limit = flying.get(ln.owner, 0), model.limits.get(ln.owner, 0)
            why = (f"{n} of {limit} lanes are in flight" if n >= limit else
                   f"the {limit - n} free lane(s) go to the jobs above it in the plan")
            rows[ln.job] = step(evidence[ln.job], lane=why)
    return [rows[ln.job] for ln in model.lines] + [rows[j] for j in sorted(rows) if j not in owners]


def wip(model: P.Plan, rows: list[Step]) -> dict[str, tuple[int, int]]:
    """owner -> (jobs in flight, its limit)."""
    return {o: (sum(1 for r in rows if r.owner == o and r.state in IN_FLIGHT), limit)
            for o, limit in sorted(model.limits.items())}


def alarms(rows: list[Step], now: dt.datetime) -> list[Step]:
    return [r for r in rows if r.late(now)]


def alarm_lines(rows: list[Step], now: dt.datetime) -> list[str]:
    return [line for _, line in alarm_groups(rows, now)]


def alarm_groups(rows: list[Step], now: dt.datetime) -> list[tuple[Step, str]]:
    """The late rows as (the row, its inbox line); jobs late on the very same notice share one line."""
    groups: dict[tuple, list[Step]] = {}
    for r in alarms(rows, now):
        same = (r.state, r.owes, r.next) if r.state in ("told", "planned") else (r.job,)
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
def order_text(model: P.Plan, owner: str, bodies: dict[str, str] | None = None) -> str:
    """What the integrator authored for `owner`: its limit, its lines in work order with their dependencies and
    holds, and each job's issue text. Never a state and never a title: a push, a PR or a merge is not a changed
    order, so it must not mint a new version (and a new notice)."""
    bodies = bodies or {}
    return "\n".join([f"limit {model.limits.get(owner, 0)}"] + [
        f"{ln.job} #{ln.issue} after={','.join(ln.after)} hold={ln.hold} "
        f"body={hashlib.sha256((bodies.get(ln.job) or '').encode()).hexdigest()[:12]}"
        for ln in model.owner_lines(owner)])


def order_version(model: P.Plan, owner: str, bodies: dict[str, str] | None = None) -> str:
    return hashlib.sha256(order_text(model, owner, bodies).encode()).hexdigest()[:12]


def job_issues(issues) -> dict[str, list[dict]]:
    """job -> the open issues titled with it (a PR is not one)."""
    titled: dict[str, list[dict]] = {}
    for it in issues or []:
        job = rules.job_of_title(it.get("title") or "")
        if job and it.get("state", "open") == "open" and it.get("pull_request") is None:
            titled.setdefault(job, []).append(it)
    return titled


def versions(model: P.Plan, issues) -> dict[str, str]:
    """owner -> the version of its order, as the open issues' text stands."""
    bodies = {job: (its[0].get("body") or "") for job, its in job_issues(issues).items()}
    return {o: order_version(model, o, bodies) for o in model.limits}


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
NOTICE_SENT = re.compile(r"<!-- switchboard:plan owner=([a-z][a-z0-9-]*) ver=([0-9a-f]{12})(?: jobs=([A-Za-z0-9,-]*))? -->")
NOTICE_AGAIN = "<!-- switchboard:plan-again owner={owner} ver={ver} -->"   # the one re-send of an unanswered version
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


@dataclass(frozen=True)
class Mail:
    """The filed mail as evidence. `posts`: the bridge's, oldest first. `notices`: the plan notices the switch
    sent, (owner, version, when). `missing`: job -> a commit its owner's newest mail names that GitHub does not
    have. `worker`: whose mail the bridge files."""
    posts: tuple[Post, ...] = ()
    notices: tuple[tuple[str, str, dt.datetime], ...] = ()
    missing: dict[str, str] = field(default_factory=dict)
    worker: str = "muse"
    receipts: dict[tuple[str, str], dt.datetime] = field(default_factory=dict)   # (owner, version) -> recorded receipt


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


def sent_notices(comments: list[dict], bot: str = rules.BOT_LOGIN) -> list[tuple[str, str, dt.datetime]]:
    """(owner, version, when) for every plan notice among `comments`: only the switch's own login can have sent one."""
    out = []
    for c in comments:
        at = when(c.get("created_at") or "")
        if ((c.get("user") or {}).get("login") or "") == bot and at:
            out += [(m.group(1), m.group(2), at) for m in NOTICE_SENT.finditer(c.get("body") or "")]
    return sorted(out, key=lambda n: n[2])


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
                    found = [j for j in m.group(3).split(",") if j]
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


def receipt_post(mail: Mail, ver: str, sent: dt.datetime) -> Post | None:
    """The worker's first mail after the notice that carries `plan <ver>`: the proof that version was read. Read
    from the subject and the body; a version is 12 hex after the word plan, never a commit."""
    return next((p for p in mail.posts if not p.hand and p.at >= sent
                 and ver in {v.lower() for v in PLAN_VER.findall(p.subject + "\n" + p.body)}), None)


def notice_at(mail: Mail, owner: str, ver: str) -> dt.datetime | None:
    return next((at for o, v, at in mail.notices if o == owner and v == ver), None)


def receipt_at(mail: Mail, owner: str, ver: str, sent: dt.datetime) -> dt.datetime | None:
    """When the owner's receipt for this version came: the one the switch recorded, else the mail itself."""
    post = receipt_post(mail, ver, sent) if owner == mail.worker else None
    return mail.receipts.get((owner, ver)) or (post.at if post else None)


def started_posts(mail: Mail) -> dict[str, dt.datetime]:
    """job -> when the worker first said it started (a mail whose subject names the job and says "started")."""
    out: dict[str, dt.datetime] = {}
    for p in mail.posts:
        if not p.hand and STARTED.search(p.subject) and not NOT_STARTED.search(p.subject):
            for job in JOB_ID.findall(p.subject):
                out.setdefault(job, p.at)
    return out


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


def mail_clashes(model: P.Plan, mail: Mail) -> dict[str, list[str]]:
    """job -> what the mail says that the plan does not hold. The worker mailed a start on a job the plan gives
    someone else or holds (J-109, J-038: taken by mail, built twice). The newest hand mail on a job orders other
    than the plan says (an order that lives only in a mail)."""
    out: dict[str, list[str]] = {}
    by_job = model.by_job()
    for job, at in started_posts(mail).items():
        ln = by_job.get(job)
        # a start the worker has since been answered on is history: a later hand mail on the job, or a later plan
        # notice, has told it where the job stands (what it does next is seen on the remote)
        if any(p.hand and p.at > at and job in JOB_ID.findall(p.subject) for p in mail.posts) or \
                any(o == mail.worker and sent > at for o, _, sent in mail.notices):
            continue
        if ln and ln.owner != mail.worker:
            out.setdefault(job, []).append(f"{mail.worker} mailed that it started; the plan gives it to {ln.owner}")
        elif ln and ln.hold:
            out.setdefault(job, []).append(f"{mail.worker} mailed that it started; the plan holds it ({ln.hold[:60]})")
    newest: dict[str, Post] = {}   # the newest hand mail that orders something on the job: a later mail that
    for p in mail.posts:           # only mentions it changes nothing (J-109: the taking, then a rulings mail)
        if p.hand and (TAKES.search(p.subject) or STOPS.search(p.subject) or GIVES.search(p.subject)):
            for job in dict.fromkeys(JOB_ID.findall(p.subject)):
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
    """One Evidence per planned job and per open job issue the plan does not hold. `reads` is plan.observe's
    answer: the open issues, every branch, the open PRs, `inactive` (landed heads), `merged_at` (job -> when a
    `Job:` PR of it merged), `head_at` (sha -> its commit time) and `closed` (the jobs whose plan issue was read
    closed). `works`: head sha -> its Work, for the heads whose proof the switch read this pass."""
    works, mail = works or {}, mail or Mail()
    by_job = model.by_job()
    titled = job_issues(reads.get("issues"))
    inactive, head_at = reads.get("inactive") or frozenset(), reads.get("head_at") or {}
    heads: dict[str, list[Head]] = {}
    for b in reads.get("branches") or []:
        name, sha = P.branch_name(b), P.branch_sha(b)
        job = P.job_of_branch(name)
        ruled = by_job[job].kept if job in by_job else ()   # a head the plan keeps as a record is not work in flight
        if job and not (sha and sha in inactive) and not any(
                name == ref and sha and sha.startswith(pin) for ref, pin in ruled):
            heads.setdefault(job, []).append(Head(name, sha, head_at.get(sha)))
    prs: dict[str, list[dict]] = {}
    for p in reads.get("prs") or []:
        ref, sha = (p.get("head") or {}).get("ref") or "", (p.get("head") or {}).get("sha") or ""
        m = P.PR_JOB.match(p.get("body") or "")
        job = m.group(1) if m else P.job_of_branch(ref)
        if job and p.get("state", "open") != "closed":
            prs.setdefault(job, []).append(p)
            if not any(h.ref == ref for h in heads.get(job, [])):
                heads.setdefault(job, []).append(Head(ref, sha, head_at.get(sha)))
    closed = reads.get("closed") or frozenset()
    vers = versions(model, reads.get("issues"))
    said, wrong = started_posts(mail), mail_clashes(model, mail)
    named = named_shas(mail, {ln.issue: ln.job for ln in model.lines})
    out: dict[str, Evidence] = {}
    for ln in model.lines:
        mine = [h for h in heads.get(ln.job, []) if owns(ln.owner, h.ref)]
        work = None
        pr = next((p for p in sorted(prs.get(ln.job, []), key=lambda p: -int(p.get("number") or 0))
                   if owns(ln.owner, (p.get("head") or {}).get("ref") or "")), None)
        if pr:
            sha = (pr.get("head") or {}).get("sha") or ""
            work = works.get(sha) or Work((pr.get("head") or {}).get("ref") or "", sha, gate="unread",
                                          pr=int(pr.get("number") or 0))
        else:
            proofs = sorted((h for h in mine if not h.wip), key=lambda h: (h.sha in works, h.at or dt.datetime.min.replace(
                tzinfo=dt.timezone.utc)), reverse=True)
            if proofs:
                work = works.get(proofs[0].sha) or branch_work(proofs[0].ref, proofs[0].sha, at=proofs[0].at)
        ver = vers.get(ln.owner, "")
        sent = notice_at(mail, ln.owner, ver)
        out[ln.job] = Evidence(
            job=ln.job, line=ln, issues=tuple(sorted(int(i["number"]) for i in titled.get(ln.job, []))),
            closed=ln.job in closed,
            waits=tuple(j for j in ln.after if j not in closed and (j in by_job or j in titled)),
            version=ver, planned_at=planned_at, notice_at=sent,
            receipt_at=receipt_at(mail, ln.owner, ver, sent) if sent else None,
            claimed_at=said.get(ln.job) if ln.owner == mail.worker else None,
            **_unpushed(ln, mail, named.get(ln.job), mine),
            heads=tuple(heads.get(ln.job, [])), work=work, merged_at=when((reads.get("merged_at") or {}).get(ln.job)),
            clashes=tuple(wrong.get(ln.job, [])))
    for job, its in titled.items():
        if job not in by_job:
            out[job] = Evidence(job=job, issues=tuple(sorted(int(i["number"]) for i in its)),
                                heads=tuple(heads.get(job, [])))
    return out


# ---------- the views: labels, the board, a worker's queue and its notice. Text and diffs only; the switch
# writes them, and only when work/switchboard.yml says `plan: live` ----------
PARKED = "state:parked"   # reconciler.PARKED_LABEL: the one state label kept, made from a hold


def labels(model: P.Plan, issues: list[dict],
           aliases: dict[str, frozenset[str]] | None = None) -> list[tuple[int, tuple[str, ...], tuple[str, ...]]]:
    """(issue, labels to add, labels to remove) for every line whose own open issue carries labels the plan does
    not say: `job`, one owner label, `state:parked` exactly when held; every other `owner:` and `state:` label
    goes. `aliases`: per owner, the labels work/agents.yml counts as that owner's (owner:claude-lane-a is
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
        want = {"job"} | ({PARKED} if ln.hold else set())
        if own and not have & own:
            want.add(f"owner:{ln.owner}")
        add = tuple(sorted(want - have))
        remove = tuple(sorted(lab for lab in have - want - own if lab.startswith(("owner:", "state:"))))
        if add or remove:
            out.append((ln.issue, add, remove))
    return out


def board(model: P.Plan, rows: list[Step], now: dt.datetime) -> str:
    """The board's job section: per owner its lanes, then every job that is not closed with its state and step."""
    lanes = wip(model, rows)
    out = ["### Jobs (made from work/plan.md and what GitHub shows; nobody edits this by hand)", ""]
    for owner in sorted({r.owner for r in rows if r.owner}):
        mine = [r for r in rows if r.owner == owner and r.state != "closed"]
        n, limit = lanes.get(owner, (0, 0))
        out.append(f"**{owner}**: {n} of {limit} lanes in flight")
        for r in mine:
            late = " **LATE**" if r.late(now) else ""
            who = f"; next: {r.owes}, {r.next}" if r.owes else f"; {r.next}"
            out.append(f"- {r.job} #{r.issue} {r.state}{late}{who}")
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
        f"In flight ({n} of {limit}): " + some(lambda r: r.state in IN_FLIGHT),
        "Start next, in this order: " + some(lambda r: r.state == "acknowledged" and r.gate == ""),
        "Waiting for a lane or the notice: " + some(lambda r: r.state in ("planned", "told") or r.gate in ("queued", "claimed")),
        "Blocked on another job: " + some(lambda r: r.state == "blocked"),
        "Held, not to be worked: " + some(lambda r: r.state == "held"),
        "Landed, waiting to be closed: " + some(lambda r: r.state == "landed"),
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
    head = f"{NOTICE_HEAD}{ver}"
    receipt = f'Receipt: put "plan {ver}" in the subject of your next MUSE_TO_CLAUDE mail.'
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
