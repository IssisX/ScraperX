#!/usr/bin/env python3
"""The reconciler (J-072, Design B stage 1, #350): one record per job, every handoff an obligation with an ack, a
due time, one re-send and a dead-letter, kept on its own branch. SHADOW ONLY: it writes `switchboard/state` and
nothing else, posts nothing, and each pass writes a daily diff of what it would send against what the ledger sent.

Level-triggered: each pass reads GitHub fresh through the ledger's reader (ledger.read_feed + ledger.classify), so
a dropped or repeated pass changes nothing. The reconciler is the only writer of the state branch; a commit lands
by a fast-forward-only ref update (force=false: GitHub's compare-and-swap), and a rejected one re-reads and redoes
the pass, at most MAX_CAS_TRIES times.

On `switchboard/state`:
  state/jobs/J-NNN.json        one record per job: job, state, owner, attempt, head, pr, ci, review, deadline,
                               outcome, obligations[] (and the facts that derive them)
  state/index.json             every job record in one file, written in the same commit (a read is one blob)
  state/threads.json           obligations on threads that name no job (asks on the board, ...)
  state/unmatched.json         everything that could not be placed, with when it was first seen
  state/cory.json              the hourly "Needs you" batches for Cory (each item reported once), the last error
  state/events/YYYY-MM.ndjson  append-only, one event per line (UTC month)
  state/shadow/YYYY-MM-DD.md   the day's shadow diff and the section 6 SLO numbers (UTC day, times in Central)

States: accepted, assigned, working, proof, review, landable, then one terminal outcome: merged, superseded,
cancelled or failed; `blocked` (reason, holder) from any state. A job whose work has landed while its issue is
still open reads `merged` with no outcome yet; the outcome is recorded once, when nothing of it is open.

Obligations: id O-<8 hex>, kind (ask|review|ruling|fix|start-agent|ci), to, via
(api|session|relay|email|issue|cory), created, attempts, last_sent, due, dead, ack, ack_rule, dead_lettered, missing,
unseen. Due by wakeability (work/agents.yml `wake`): the Claude API (reviews) 30 min; the integrator (`session`: every
other Claude obligation) the first of its hourly runs (work/tasks/integrator-hourly.md: 8:17 AM-11:17 PM Central on
weekdays, 9:17 AM-9:17 PM Sat-Sun) at or after its next run plus 1 h, so nothing falls due overnight; Codex
(`relay`: its app ignores the bot's mentions, so the integrator posts the mention) the integrator's due plus 30 min;
email (Muse) 4 h; issue with a known schedule (Grok: 10 AM, 4 PM Central) its next run + 1 h; anything else is
manual and goes straight to Cory. Ack: `/ack O-<id>` from a team login, a proven action (a new head, a verdict on
the head, a merge), or a reply by the addressee after the obligation that names it (its id or the ask's comment id),
or is on the obligation's own thread where it owes nothing else (never on the shared board). An obligation closes
only on positive evidence (merged, closed, answered, the head moved): one missing from a pass is untouched, and
missing UNSEEN_AFTER passes in a row it reads `unseen` (dormant, not closed); its want coming back reopens it. No
ack: re-sent once at the due time; at the dead time (twice the due span; for a scheduled agent, its run after the
due plus 1 h) dead-lettered to the "Needs you" list, which goes to Cory at most once an hour in one batch (stage 1
writes the message it would send, in state/cory.json and the day's shadow file). A dead-letter is never done: it
stays open until acked, and its job never takes a terminal outcome while it does.

Commands (one line each, from a team login, on the job's issue or PR; keyed by comment id, so a repeat does
nothing): `/ack O-xxxxxxxx`, `/blocked <reason>` (or a line `BLOCKED: <what>, needs <whom>`), `/unblock`,
`/cancel J-NNN`.

Usage (read-only against GitHub; the state goes to a local directory, never pushed):
  python3 tools/switchboard/reconciler.py --state-dir /tmp/reconciler-state [--repo IssisX/ScraperX]
"""

from __future__ import annotations

import base64
import dataclasses
import datetime as dt
import hashlib
import json
import pathlib
import re
import sys
import time
from typing import Callable, Iterable
from zoneinfo import ZoneInfo

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import ledger as L  # noqa: E402  the reader and classifier (ledger.read_feed, ledger.classify)
import asks  # noqa: E402  (ledger puts tools/board on the path)
import rules  # noqa: E402

STATE_BRANCH = "switchboard/state"
CONFIG = "work/switchboard.yml"      # `reconciler: shadow` turns it on; absent or anything else is off
JOBS_DIR, EVENTS_DIR, SHADOW_DIR = "state/jobs", "state/events", "state/shadow"
THREADS, UNMATCHED, INDEX = "state/threads.json", "state/unmatched.json", "state/index.json"
CORY_FILE = "state/cory.json"
CENTRAL = ZoneInfo("America/Chicago")

WAKEABLE = dt.timedelta(minutes=30)  # the Claude API (reviews); Codex once the integrator has posted the mention
EMAIL = dt.timedelta(hours=4)        # Muse, by the mail bridge
POLL_GRACE = dt.timedelta(hours=1)   # a polled agent: its next run plus this
CORY = dt.timedelta(minutes=15)      # manual: straight to Cory
# Central (hour, minute) of each polled agent's runs: Grok 10 AM and 4 PM; ChatGPT's review task hourly at :27 (11-09).
POLL_RUNS = {"grok": ((10, 0), (16, 0)), "chatgpt": tuple((h, 27) for h in range(24))}
# The integrator's hourly runs (work/tasks/integrator-hourly.md, Cory 11-06 and 11-08): weekdays 8:17 AM-11:17 PM
# Central, Sat-Sun 9:17 AM-9:17 PM, none overnight. Keyed by "is a weekend": (first hour, last hour, minute).
INTEGRATOR_RUNS = {False: (8, 23, 17), True: (9, 21, 17)}
UNSEEN_AFTER = 3                     # passes in a row an obligation's want is missing before it reads `unseen`
CORY_EVERY = dt.timedelta(hours=1)   # the "Needs you" batch goes to Cory at most this often, never per item
PASS_GAP = dt.timedelta(minutes=25)  # no pass this long (the clock dispatches every 10 min): a PIPE line for Cory
TOKEN_WARN = dt.timedelta(days=7)    # a token expiring this soon: a line for Cory
MAX_BATCHES = 48                     # Cory batches kept in state/cory.json
COMPUTING = "GitHub is still computing mergeability"
MUSE_LABEL = "wake:muse"             # the bridge mails an issue's CLAUDE_TO_MUSE comments while this label is on
CI_LATE = dt.timedelta(minutes=90)   # a proof still running this long after its head was pushed is late
RECHECK = dt.timedelta(hours=6)      # a quiet branch's existence and landing are re-read this often
MAX_CAS_TRIES = 3
MAX_SENDS = 10                       # deliveries per pass; the rest come next pass
MAX_CMDS = 200                       # command ids kept per record
TERMINAL = ("merged", "superseded", "cancelled", "failed")
STATES = ("accepted", "assigned", "working", "proof", "review", "landable", "blocked") + TERMINAL
CONFLICT = "merge conflict with the base"

CMD = re.compile(r"^[ \t]*(?:/(?P<cmd>ack|blocked|unblock|cancel)\b[ \t]*(?P<arg>[^\n]*)"
                 r"|\**BLOCKED:\**[ \t]*(?P<blk>[^\n]+))", re.MULTILINE | re.IGNORECASE)
OB_ID = re.compile(r"\bO-[0-9a-f]{8}\b")
JOB_ID = re.compile(r"\bJ-\d{3,}[a-z]?\b")
SENT_MARK = "<!-- reconciler:sent id={id} send={send} attempt={attempt} -->"
SENT_RE = re.compile(r"<!-- reconciler:sent id=(O-[0-9a-f]{8}) send=(\d+) attempt=(\d+) -->")
CORY_MARK = "<!-- reconciler:cory batch={batch} keys={keys} -->"
CORY_RE = re.compile(r"<!-- reconciler:cory batch=(\S+) keys=(\S*) -->")


def iso(t: dt.datetime | None) -> str | None:
    return L.iso(t) if t else None


def at(stamp: str | None) -> dt.datetime | None:
    return L.parse(stamp)


def central(t: dt.datetime) -> str:
    return t.astimezone(CENTRAL).strftime("%m-%d %I:%M %p").replace(" 0", " ")


def ob_id(job: str, kind: str, to: str, version: str) -> str:
    return "O-" + hashlib.sha256(f"{job}|{kind}|{to}|{version}".encode("utf-8")).hexdigest()[:8]


# ---------- errors ----------
class Conflict(RuntimeError):
    """The state branch moved under this pass (the compare-and-swap refused the update)."""


class RateLimited(RuntimeError):
    def __init__(self, retry_after: float, detail: str = ""):
        super().__init__(f"rate limited, retry after {retry_after:.0f}s {detail}".strip())
        self.retry_after = retry_after


def rate_limit_of(err: Exception) -> RateLimited | None:
    """A 429, or a 403 that says it is a rate limit, is a backoff; any other 403 is a refusal."""
    text = str(getattr(err, "detail", "") or err)
    low = text.lower()
    if "http 429" in low or ("http 403" in low and "rate limit" in low):
        m = re.search(r"retry[- ]after:?\s*(\d+)", low)
        return RateLimited(float(m.group(1)) if m else 60.0, text[:120])
    return None


# ---------- due times by wakeability ----------
def is_claude(roster: L.Roster, agent: str) -> bool:
    return agent == "claude" or (agent not in roster.agents and agent.startswith("claude"))


def channel(roster: L.Roster, agent: str, kind: str = "") -> str:
    if is_claude(roster, agent):
        return "api" if kind == "review" else "session"   # the API reviews; the rest waits on the integrator's runs
    if agent not in roster.agents:
        return "cory"
    wake = roster.wake(agent)
    if wake == "mention":
        return "relay"   # the bot's own mentions do not wake Codex (#350 hole 5): the integrator posts the mention
    if wake in ("api", "email"):
        return wake
    if wake == "issue" and agent in POLL_RUNS:
        return "issue"
    return "cory"   # manual, or a mode with no known schedule: nothing waits on an agent that will not run


def next_run(t: dt.datetime, runs, inclusive: bool = False) -> dt.datetime:
    """The first run strictly after `t` (at or after it when `inclusive`). `runs` is a list of Central (hour,
    minute), or a function of the date that gives that list (the integrator's weekday and weekend hours differ)."""
    local = t.astimezone(CENTRAL)
    for day in range(0, 8):
        d = (local + dt.timedelta(days=day)).date()
        for h, m in sorted(runs(d) if callable(runs) else runs):
            cand = dt.datetime(d.year, d.month, d.day, h, m, tzinfo=CENTRAL)
            if cand > local or (inclusive and cand == local):
                return cand.astimezone(dt.timezone.utc)
    return t + dt.timedelta(days=1)


def integrator_runs(d: dt.date) -> list[tuple[int, int]]:
    first, last, minute = INTEGRATOR_RUNS[d.weekday() >= 5]
    return [(h, minute) for h in range(first, last + 1)]


def session_due(t: dt.datetime) -> dt.datetime:
    """The integrator's deadline from `t`: its next run plus 1 h, moved on to the first run at or after that, so a
    deadline never falls overnight (an obligation at 10:50 PM is due at the 8:17 AM run, not at 12:17 AM)."""
    return next_run(next_run(t, integrator_runs) + POLL_GRACE, integrator_runs, inclusive=True)


def due_at(roster: L.Roster, agent: str, created: dt.datetime, kind: str = "") -> tuple[str, dt.datetime]:
    via = channel(roster, agent, kind)
    if via == "api":
        return via, created + WAKEABLE
    if via == "session":
        return via, session_due(created)
    if via == "relay":
        return via, session_due(created) + WAKEABLE
    if via == "email":
        return via, created + EMAIL
    if via == "issue":
        return via, next_run(created, POLL_RUNS[agent]) + POLL_GRACE
    return "cory", created + CORY


def dead_time(agent: str, via: str, created: dt.datetime, due: dt.datetime) -> dt.datetime:
    """When an unacked obligation is dead-lettered. A scheduled agent gets its next run after the due time too,
    plus the same grace, so a dead-letter never lands overnight; anyone else gets twice the due span."""
    if via == "session":
        return session_due(due)
    if via == "relay":
        return session_due(due - WAKEABLE) + WAKEABLE
    if via == "issue":
        return next_run(due, POLL_RUNS[agent]) + POLL_GRACE
    return created + 2 * (due - created)


def dead_at(ob: dict) -> dt.datetime:
    if ob.get("dead"):
        return at(ob["dead"])
    created, due = at(ob["created"]), at(ob["due"])
    return created + 2 * (due - created)


def is_live(ob: dict) -> bool:
    """Not acked and not closed: open, or unseen (dormant until its want comes back)."""
    return ob.get("ack") is None and ob.get("closed") is None


def is_open(ob: dict) -> bool:
    """Live and seen: only these are delivered, re-sent and dead-lettered."""
    return is_live(ob) and not ob.get("unseen")


def opens_prs(roster: L.Roster, agent: str) -> bool:
    """work/agents.yml `opens_prs` (default true). Muse pushes its branches but cannot open a PR (CLAUDE.md 11-05:
    the switchboard opens them), so "open the PR" goes to the integrator, who can."""
    return bool((roster.agents.get(agent) or {}).get("opens_prs", True))


# ---------- what the pass sees ----------
@dataclasses.dataclass
class Want:
    kind: str
    to: str          # agent key
    version: str
    ref: str         # the ledger item it matches (`PR|#343`, `BRANCH|name`, `JOB|#350`, `ASK|ask on #340`)
    thread: int
    what: str
    ack_rule: str
    asked: dt.datetime | None = None


@dataclasses.dataclass
class Comment:
    id: str
    thread: int
    t: dt.datetime
    login: str
    body: str

    @property
    def author(self) -> str:
        return asks.author(self.body)


@dataclasses.dataclass
class Pass:
    """Everything one pass read: the feed and its classification, the comment stream, the ledger's sent keys."""
    feed: L.Feed
    led: L.Ledger
    comments: list[Comment]
    ledger_sent: set[str]
    trusted: frozenset[str] = frozenset({"IssisX"})
    now: dt.datetime | None = None
    tokens: dict = dataclasses.field(default_factory=dict)   # token name -> its expiry (GitHub's expiration header)


class Lookups:
    """Point reads the feed does not carry. Each answer for one head is kept on the record, so it is read once."""

    def landed(self, branch: str, sha: str) -> str:   # "in trunk", "merged #N", "closed #N" or ""
        return ""

    def branch_sha(self, name: str) -> str | None:     # None when the branch is gone
        return None

    def pr(self, n: int) -> dict:                       # {"merged": bool, "state": "open|closed"}
        return {}

    def issue(self, n: int) -> dict:                    # {"state": ..., "state_reason": ...}
        return {}


class GitHubLookups(Lookups):
    def __init__(self, gh, trunk: str = rules.TRUNK):
        self.gh, self.trunk = gh, trunk

    def landed(self, branch: str, sha: str) -> str:
        cmp = self.gh.get(f"repos/{self.gh.repo}/compare/{self.trunk}...{sha}?per_page=1") or {}
        if cmp.get("status") in ("behind", "identical"):
            return "in trunk"
        for p in self.gh.get(f"repos/{self.gh.repo}/commits/{sha}/pulls") or []:
            if (p.get("head") or {}).get("sha") == sha:
                if p.get("merged_at"):
                    return f"merged #{p['number']}"
                if p.get("state") == "closed":
                    return f"closed #{p['number']}"
        return ""

    def branch_sha(self, name: str) -> str | None:
        from ghapi import GitHubError
        try:
            return ((self.gh.get(f"repos/{self.gh.repo}/git/ref/heads/{name}") or {}).get("object") or {}).get("sha")
        except GitHubError as err:
            if "HTTP 404" in err.detail:
                return None
            raise

    def pr(self, n: int) -> dict:
        p = self.gh.get(f"repos/{self.gh.repo}/pulls/{n}") or {}
        return {"merged": bool(p.get("merged_at")), "state": p.get("state", "")}

    def issue(self, n: int) -> dict:
        i = self.gh.get(f"repos/{self.gh.repo}/issues/{n}") or {}
        return {"state": i.get("state", ""), "state_reason": i.get("state_reason") or ""}


# ---------- the state ----------
def new_record(job: str, now: dt.datetime) -> dict:
    return {"job": job, "state": "accepted", "owner": "", "attempt": 0, "head": "", "pr": 0, "ci": "",
            "review": "", "deadline": None, "outcome": None, "outcome_reason": "", "obligations": [],
            "issue": 0, "blocked": None, "cancelled": None, "prs": [], "branches": {}, "landed": {},
            "commands": [], "pr_done": "", "created": iso(now), "updated": iso(now)}


@dataclasses.dataclass
class State:
    jobs: dict[str, dict]
    threads: dict
    unmatched: dict[str, dict]
    events_text: str          # this month's event log as read
    files: dict[str, str]     # every file as read, to write only what changed

    loaded: dict[str, str] = dataclasses.field(default_factory=dict)   # each job's JSON as read
    cory: dict = dataclasses.field(default_factory=dict)              # state/cory.json

    @classmethod
    def load(cls, files: dict[str, str], month: str) -> "State":
        jobs = dict((json.loads(files[INDEX]).get("jobs") or {})) if INDEX in files else {}
        threads = json.loads(files[THREADS]) if THREADS in files else {"job": "", "obligations": [], "commands": []}
        unmatched = json.loads(files[UNMATCHED]) if UNMATCHED in files else {}
        cory = json.loads(files[CORY_FILE]) if CORY_FILE in files else {}
        for k, v in (("reported", {}), ("last_batch", None), ("batches", []), ("last_error", "")):
            cory.setdefault(k, v)
        return cls(jobs, threads, unmatched, files.get(f"{EVENTS_DIR}/{month}.ndjson", ""), dict(files),
                   {j: dump_json(r) for j, r in jobs.items()}, cory)

    def records(self) -> list[dict]:
        return list(self.jobs.values()) + [self.threads]

    def find(self, oid: str) -> tuple[dict, dict] | None:
        for rec in self.records():
            for ob in rec["obligations"]:
                if ob["id"] == oid:
                    return rec, ob
        return None

    def changes(self) -> dict[str, str]:
        """The files this pass changed: each job file whose record changed, and the index with them."""
        out = {f"{JOBS_DIR}/{j}.json": t for j, r in self.jobs.items() if (t := dump_json(r)) != self.loaded.get(j)}
        if out or INDEX not in self.files:
            out[INDEX] = dump_json({"jobs": self.jobs})
        for path, obj in ((THREADS, self.threads), (UNMATCHED, self.unmatched), (CORY_FILE, self.cory)):
            if self.files.get(path) != (t := dump_json(obj)):
                out[path] = t
        return out


def dump_json(obj) -> str:
    return json.dumps(obj, indent=1, sort_keys=True) + "\n"


class Events:
    def __init__(self, now: dt.datetime):
        self.now, self.items = now, []

    def __call__(self, ev: str, job: str = "", **kw) -> None:
        self.items.append({"t": iso(self.now), "ev": ev, "job": job, **kw})

    def text(self) -> str:
        return "".join(json.dumps(e, sort_keys=True) + "\n" for e in self.items)


# ---------- observing ----------
@dataclasses.dataclass
class JobObs:
    job: str
    issue: L.IssueIn | None = None
    prs: list = dataclasses.field(default_factory=list)          # PrIn
    branches: list = dataclasses.field(default_factory=list)     # (BranchIn, run state)
    asks: list = dataclasses.field(default_factory=list)         # AskIn
    threads: set = dataclasses.field(default_factory=set)


def observe(p: Pass, roster: L.Roster) -> tuple[dict[str, JobObs], list, dict[int, str]]:
    """Per job, what is open now. Returns (jobs, asks on threads that name no job, thread -> job)."""
    feed = p.feed
    jobs: dict[str, JobObs] = {}
    thread_job: dict[int, str] = {}

    def get(job: str) -> JobObs:
        return jobs.setdefault(job, JobObs(job))

    for i in feed.issues:
        job = rules.job_of_title(i.title)
        if job and get(job).issue is None:
            get(job).issue = i
            get(job).threads.add(i.number)
            thread_job[i.number] = job
    pr_refs = set()
    for pr in feed.prs:
        if pr.number in feed.system_prs:
            continue
        pr_refs.add(pr.head_ref)
        job = rules.job_of_pr(pr.job_line) or L.job_of_branch(pr.head_ref)
        if job:
            get(job).prs.append(pr)
            get(job).threads.add(pr.number)
            thread_job[pr.number] = job
    superseded = {it.ref for it in p.led.items if it.kind == "SUPERSEDED"}
    newest: dict[str, L.RunIn] = {}
    for r in sorted(feed.runs, key=lambda r: r.created_at):
        newest[r.branch] = r
    for b in feed.branches:
        if (b.name == feed.trunk or b.name in feed.system_branches or b.name.startswith(L.SYSTEM_PREFIXES)
                or b.name in pr_refs or b.name in feed.open_heads or b.name in superseded):
            continue
        job = L.job_of_branch(b.name) or rules.job_of_pr(b.job_line)
        if not job:
            continue   # the ledger lists it UNMATCHED
        r = newest.get(b.name)
        run = "none" if r is None or r.sha != b.sha else (
            r.state or ("green" if r.conclusion == "success" else "running" if not r.conclusion else "red"))
        get(job).branches.append((b, run))
    loose = []
    own = own_notices(p)
    for a in feed.asks:
        if a.cid in own:
            continue   # the reconciler's own "To <Name>:" notice is not a new ask (it would owe itself forever)
        job = thread_job.get(a.thread)
        (get(job).asks if job else loose).append(a)
    return jobs, loose, thread_job


def own_notices(p: Pass) -> set[str]:
    return {c.id for c in p.comments if c.login == rules.BOT_LOGIN and (SENT_RE.search(c.body) or CORY_RE.search(c.body))}


def is_landed(how: str) -> bool:
    """"in trunk" or "merged #N" is landed; "closed #N" (closed without merging) is not (#350 hole 8)."""
    return bool(how) and not how.startswith("closed")


def landed_of(b: L.BranchIn, lookups: Lookups) -> str:
    """"in trunk", "merged #N", "closed #N" or "": the ledger's own reading (ledger.branch_fate, from the trunk
    compare and closed PRs the feed carries) when it read them, else one lookup."""
    gone = L.branch_fate(b)[0]
    if gone == "landed":
        return "in trunk"
    m = re.search(r"#(\d+) was (merged|closed)", gone)
    if m:
        return f"{m.group(2)} #{m.group(1)}"
    return "" if (b.vs_trunk or b.closed) else lookups.landed(b.name, b.sha)


def owner_of(o: JobObs, rec: dict, roster: L.Roster) -> str:
    if o.issue is not None:
        who = roster.of_labels(o.issue.labels)
        if who:
            return who.agent
    refs = [pr.head_ref for pr in sorted(o.prs, key=lambda x: x.touched)] + \
           [b.name for b, _ in sorted(o.branches, key=lambda x: x[0].pushed_at)]
    for ref in reversed(refs):
        who = roster.of_branch(ref)
        if who:
            return who.agent
    return rec.get("owner") or ""


def reviewer_of(pr: L.PrIn, roster: L.Roster) -> str:
    if not pr.reviewer or pr.reviewer == "codex":
        return "codex"
    return pr.reviewer if pr.reviewer in roster.agents else "claude"


def derive(o: JobObs, rec: dict, owner: str, now: dt.datetime, roster: L.Roster, landed: set[str],
           closed: dict[str, str] | None = None) -> tuple[str, list[Want]]:
    """The job's state and the obligations it owes now, from what is open. Pure. `closed` maps a branch whose head
    is a PR closed without merging to that "closed #N": not landed, it owes the integrator one ruling."""
    closed = closed or {}
    wants: list[Want] = []
    state = "accepted"
    states: list[str] = []
    issue_thread = o.issue.number if o.issue else 0
    for pr in sorted(o.prs, key=lambda x: x.number):
        author = (roster.of_branch(pr.head_ref) or L.Person("", "")).agent
        fixer = owner or author or "claude"
        ref, n, h = f"PR|#{pr.number}", pr.number, pr.head_sha
        if CONFLICT in pr.reasons:
            wants.append(Want("fix", fixer, f"conflict:{h}", ref, n, f"#{n} has a {CONFLICT}", "new-head|reply|ack"))
            states.append("working")
        elif pr.action == "FIX-CI":
            wants.append(Want("fix", fixer, f"red:{h}", ref, n, f"#{n} proof {pr.proof}", "new-head|reply|ack"))
            states.append("working")
        elif pr.action == "FIX":
            wants.append(Want("fix", fixer, f"fix:{h}", ref, n, f"#{n}: {L.clean('; '.join(pr.reasons), 100)}",
                              "new-head|reply|ack"))
            states.append("working")
        elif pr.action == "MERGE":
            states.append("landable")   # the switchboard merges it: no one else owes anything
        elif pr.action == "HUMAN-MERGE":
            wants.append(Want("ruling", "claude", f"merge:{h}", ref, n, f"merge #{n} by hand", "merged|reply|ack"))
            states.append("landable")
        elif pr.proof in ("running", "none"):
            pushed = at(pr.head_committed_at)
            if pr.proof == "running" and pushed and now - pushed >= CI_LATE:
                wants.append(Want("ci", "claude", f"ci:{h}", ref, n, f"#{n} proof still running "
                                  f"{L.age_text(now - pushed)} after its head", "ci-done|ack"))
            states.append("proof")
        elif pr.needs_review:
            who = reviewer_of(pr, roster)
            wants.append(Want("review", who, h, ref, n, f"review #{n} @ {h[:8]}", "verdict|ack"))
            states.append("review")
        else:
            states.append("review")
    for b, run in o.branches:
        if b.name in landed:
            continue
        fixer = owner or (roster.of_branch(b.name) or L.Person("", "")).agent or "claude"
        ref, n = f"BRANCH|{b.name}", issue_thread
        if b.name in closed:   # its PR was closed at this very head: not landed, and nothing reopens it by itself
            m = re.search(r"#(\d+)", closed[b.name])
            where = int(m.group(1)) if m else issue_thread
            wants.append(Want("ruling", "claude", f"closed:{b.sha}", ref, where,
                              f"`{b.name}` @ {b.sha[:8]}: {closed[b.name]} without merging at this head: reopen it, "
                              "or delete the branch", "reply|ack"))
            states.append("working")
            continue
        if run.startswith("red"):
            wants.append(Want("fix", fixer, f"red:{b.sha}", ref, n, f"`{b.name}` proof {run}, no PR",
                              "new-head|reply|ack"))
            states.append("working")
        elif run == "green":
            if not (b.name.startswith("fb-bot/integrate-") and rules.job_of_pr(b.job_line)):   # as the ledger
                opener, why = fixer, ""
                if not opens_prs(roster, fixer):   # Muse cannot open a PR: the integrator does (#350 hole 8)
                    opener, why = "claude", f" ({roster.address(fixer)} cannot open PRs)"
                wants.append(Want("fix", opener, f"open-pr:{b.sha}", ref, n, f"`{b.name}` is green with no PR: "
                                  f"open one{why}", "new-head|reply|ack"))
            states.append("proof")
        else:
            states.append("proof" if run == "running" else "working")
    if o.issue is not None and not states:
        if not owner:
            wants.append(Want("ruling", "claude", f"assign:{o.issue.number}", f"JOB|#{o.issue.number}",
                              o.issue.number, f"{o.job} has no owner: label", "owner|reply|ack"))
            states.append("accepted")
        elif rec.get("pr_done") == "merged" or any(is_landed(v[1]) for v in rec["landed"].values()):
            states.append("merged")   # its work landed; the issue is still open
        elif not rec["head"] and not rec["prs"] and now - o.issue.touched < L.STALE:
            states.append("assigned")   # just assigned: the owner has a day before a start is owed
        elif not rec["head"] and not rec["prs"]:   # quiet a day with no work seen (the feed reads 3 days of pushes)
            wants.append(Want("start-agent", owner, f"start:{o.issue.number}", f"JOB|#{o.issue.number}",
                              o.issue.number, f"start {o.job}", "work|reply|ack"))
            states.append("assigned")
        else:
            states.append("working")
    for a in o.asks:
        who = roster.of_name(a.target)
        wants.append(Want("ask", who.agent if who else a.target.lower(), a.cid, f"ASK|ask on #{a.thread}",
                          a.thread, f"ask from {L.clean(a.author, 20)} on #{a.thread}", "reply|ack", a.created_at))
    order = ("landable", "review", "proof", "working", "merged", "assigned", "accepted")
    for s in order:   # the most advanced open piece of work is the job's state
        if s in states:
            state = s
            break
    return state, wants


# ---------- the pass ----------
def apply_commands(state: State, p: Pass, thread_job: dict[int, str], events: Events, unmatched: dict,
                   roster: L.Roster) -> None:
    for c in sorted(p.comments, key=lambda c: (c.t, c.id)):
        if c.login not in p.trusted:
            continue
        for m in CMD.finditer(c.body):
            cmd = (m.group("cmd") or "blocked").lower()
            arg = (m.group("arg") if m.group("cmd") else m.group("blk")) or ""
            named = JOB_ID.search(arg)
            job = named.group(0) if (named and cmd in ("cancel", "blocked")) else thread_job.get(c.thread, "")
            key = f"{c.id}:{m.start()}"
            if cmd == "ack":
                ids = OB_ID.findall(arg)
                for oid in ids:
                    hit = state.find(oid)
                    if hit is None:
                        unmatched[f"CMD|{c.id}|{oid}"] = f"/ack for an unknown obligation {oid} on #{c.thread}"
                        continue
                    rec, ob = hit
                    if key in rec["commands"] or not is_open(ob):
                        continue   # a repeat, or already settled: no second transition
                    ob["ack"] = {"how": "/ack", "at": iso(c.t), "by": c.author, "comment": c.id}
                    rec["commands"] = (rec["commands"] + [key])[-MAX_CMDS:]
                    events("acked", rec["job"], id=oid, how="/ack", by=c.author)
                if not ids:
                    unmatched[f"CMD|{c.id}"] = f"/ack with no O-xxxxxxxx id on #{c.thread}"
                continue
            if not job:
                unmatched[f"CMD|{c.id}"] = f"/{cmd} on #{c.thread}, which names no job"
                continue
            rec = state.jobs.get(job)
            if rec is None:
                unmatched[f"CMD|{c.id}"] = f"/{cmd} for {job}, which has no record"
                continue
            if key in rec["commands"]:
                continue
            rec["commands"] = (rec["commands"] + [key])[-MAX_CMDS:]
            if cmd == "blocked":
                reason = L.clean(arg, 160)
                needs = re.search(r"needs\s+([A-Za-z]+)", arg)
                who = roster.of_name(needs.group(1)) if needs else None
                holder = who.agent if who else "cory"
                new = {"reason": reason, "holder": holder, "since": iso(c.t), "head": rec.get("head", "")}
                if rec.get("blocked") and rec["blocked"]["reason"] == reason and rec["blocked"]["holder"] == holder:
                    continue
                rec["blocked"] = new
                events("blocked", job, reason=reason, holder=holder, by=c.author)
            elif cmd == "unblock" and rec.get("blocked"):
                rec["blocked"] = None
                events("unblocked", job, by=c.author)
            elif cmd == "cancel" and not rec.get("cancelled"):
                rec["cancelled"] = {"at": iso(c.t), "by": c.author}
                events("cancel", job, by=c.author)


def thread_replies(p: Pass, roster: L.Roster) -> dict[int, list[tuple[dt.datetime, str, str]]]:
    """Per thread, each team post as (time, agent, body), oldest first."""
    out: dict[int, list] = {}
    for c in sorted(p.comments, key=lambda c: (c.t, c.id)):
        if c.login == rules.BOT_LOGIN:
            continue
        who = roster.of_name(c.author)
        if who:
            out.setdefault(c.thread, []).append((c.t, who.agent, c.body))
    return out


def markers(p: Pass) -> dict[str, tuple[int, int, dt.datetime]]:
    out: dict[str, tuple[int, int, dt.datetime]] = {}
    for c in p.comments:
        if c.login != rules.BOT_LOGIN:
            continue
        for m in SENT_RE.finditer(c.body):
            got = (int(m.group(2)), int(m.group(3)), c.t)
            if m.group(1) not in out or got[:2] > out[m.group(1)][:2]:
                out[m.group(1)] = got
    return out


def sync(rec: dict, wants: list[Want], now: dt.datetime, roster: L.Roster, events: Events, evidence_of: Callable) \
        -> None:
    """Create the obligations wanted now (a want that comes back reopens its unseen or closed obligation); settle
    the open ones no longer wanted only on positive evidence (`evidence_of`). With none, the obligation is left
    as it is and counted missing; UNSEEN_AFTER passes in a row make it `unseen`, never closed (#350 hole 3). A
    dead-letter ends only by an ack."""
    by_id = {ob["id"]: ob for ob in rec["obligations"]}
    wanted = {}
    for w in wants:
        oid = ob_id(rec["job"], w.kind, w.to, w.version)
        wanted[oid] = w
        ob = by_id.get(oid)
        if ob is not None:
            ob["missing"] = 0
            if ob.get("ack") is None and (ob.get("unseen") or ob.get("closed")):
                was = "unseen" if ob.get("unseen") else ob["closed"]["why"]
                ob["unseen"], ob["closed"], ob["reopened"] = None, None, iso(now)
                if not ob.get("dead_lettered"):   # a fresh clock: its dormant time is not the addressee's delay
                    via, due = due_at(roster, ob["to"], now, ob["kind"])
                    ob["due"], ob["dead"] = iso(due), iso(dead_time(ob["to"], via, now, due))
                events("reopened", rec["job"], id=oid, was=was)
            continue
        via, due = due_at(roster, w.to, now, w.kind)
        ob = {"id": oid, "kind": w.kind, "to": w.to, "via": via, "ref": w.ref, "thread": w.thread,
              "version": w.version, "what": w.what, "created": iso(now), "attempts": 0, "sends": 0,
              "last_sent": None, "last_error": "", "due": iso(due), "dead": iso(dead_time(w.to, via, now, due)),
              "ack": None, "ack_rule": w.ack_rule, "dead_lettered": None, "closed": None, "asked": iso(w.asked),
              "missing": 0, "unseen": None}
        rec["obligations"].append(ob)
        by_id[oid] = ob
        events("obligation", rec["job"], id=oid, kind=w.kind, to=w.to, via=via, ref=w.ref, due=iso(due))
    for ob in rec["obligations"]:
        if not is_open(ob) or ob["id"] in wanted:
            continue
        moved = next((w for w in wanted.values() if w.kind == ob["kind"] and w.version == ob["version"]
                      and w.to != ob["to"]), None)
        if moved:   # the owner changed: seen, so positive
            ob["closed"] = {"why": f"reassigned to {moved.to}", "at": iso(now)}
            events("closed", rec["job"], id=ob["id"], why=ob["closed"]["why"])
            continue
        how = evidence_of(ob)
        if how.startswith("ack:"):
            ob["ack"] = {"how": how[4:], "at": iso(now), "by": ob["to"]}
            ob["missing"] = 0
            events("acked", rec["job"], id=ob["id"], how=how[4:])
        elif how == "owed":
            ob["missing"] = 0
        elif ob.get("dead_lettered"):
            continue   # a dead-letter stands until acked
        elif how.startswith("close:"):
            ob["closed"] = {"why": how[6:], "at": iso(now)}
            events("closed", rec["job"], id=ob["id"], why=how[6:])
        else:   # no evidence either way: an input missing from this pass is not an answer
            ob["missing"] = int(ob.get("missing") or 0) + 1
            if ob["missing"] >= UNSEEN_AFTER:
                ob["unseen"] = iso(now)
                events("unseen", rec["job"], id=ob["id"], passes=ob["missing"])


class Seen:
    """What this pass read, for positive evidence, with each point read made once and a failed read no evidence."""

    def __init__(self, p: Pass, lookups: Lookups, roster: L.Roster):
        f = p.feed
        self.lookups, self.roster = lookups, roster
        self.prs = {pr.number: pr for pr in f.prs}
        self.open_prs = set(f.open_prs)
        self.branches = {b.name: b for b in f.branches}
        self.issues = {i.number: i for i in f.issues}
        self.heads = dict(f.open_heads) | {pr.head_ref: pr.number for pr in f.prs}
        self.runs: dict[str, L.RunIn] = {}
        for r in sorted(f.runs, key=lambda r: r.created_at):
            self.runs[r.branch] = r
        self.cache: dict = {}

    def _once(self, key, fn, unknown):
        if key not in self.cache:
            try:
                self.cache[key] = fn()
            except Exception:   # noqa: BLE001  a read that failed tells nothing
                self.cache[key] = unknown
        return self.cache[key]

    def pr(self, n: int) -> dict:
        return self._once(("pr", n), lambda: self.lookups.pr(n), {}) or {}

    def issue(self, n: int) -> dict:
        return self._once(("issue", n), lambda: self.lookups.issue(n), {}) or {}

    def branch_sha(self, name: str) -> str | None:   # None: gone; "?": unknown
        return self._once(("branch", name), lambda: self.lookups.branch_sha(name), "?")

    def run_state(self, b: L.BranchIn) -> str:
        r = self.runs.get(b.name)
        if r is None or r.sha != b.sha:
            return "none"
        return r.state or ("green" if r.conclusion == "success" else "running" if not r.conclusion else "red")


def evidence(ob: dict, o: JobObs | None, seen: Seen, rec: dict, now: dt.datetime) -> str:
    """Why an open obligation that is not wanted this pass is over, from what was read: `ack:<proven action>`,
    `close:<why>`, `owed` (still owed though its source no longer lists it), or "" (no evidence: left as is)."""
    kind, version = ob["kind"], ob["version"]
    tag, _, sha = version.rpartition(":")
    refkind, ref = ob["ref"].split("|", 1)
    if kind == "ask":   # asks.py drops an ask on any later post by its addressee (#350 hole 7): not an answer
        asked = at(ob.get("asked")) or at(ob["created"])
        return "owed" if now - asked < L.LIVE else ""
    if refkind == "PR":
        n = ob["thread"]
        pr = seen.prs.get(n)
        if pr is None:
            if n in seen.open_prs:
                return ""   # still open, just not read this pass
            got = seen.pr(n)
            if got.get("merged"):
                return "ack:merged" if kind == "ruling" else "close:PR merged"
            if got.get("state") == "closed":
                return "close:PR closed without merging"
            return ""
        if pr.head_sha != sha:
            return "ack:new head" if kind == "fix" else f"close:new head {pr.head_sha[:8]}"
        if kind == "review":
            verdict = not pr.needs_review and pr.action in ("MERGE", "FIX", "HUMAN-MERGE")
            return "ack:verdict on the head" if verdict else ""
        if kind == "ci":
            return "ack:proof finished" if pr.proof not in ("running", "none") else ""
        if kind == "fix" and tag == "conflict":
            return "close:conflict cleared at the head" if CONFLICT not in pr.reasons and \
                COMPUTING not in pr.reasons else ""
        if kind == "fix" and tag == "red":
            return "close:proof green at the head" if pr.proof == "green" else ""
        if pr.action == "MERGE":   # a FIX, or a hand merge, the switchboard's own merge now settles
            return "close:a LAND at the head: the switchboard merges it"
        return ""
    if refkind == "BRANCH":
        if ref in seen.heads:
            n = seen.heads[ref]
            if tag == "open-pr":
                return f"ack:PR #{n} opened"
            if tag == "closed":
                return f"ack:a PR is open again (#{n})"
            return f"close:PR #{n} took it over"
        b = seen.branches.get(ref)
        now_sha = b.sha if b is not None else seen.branch_sha(ref)
        if now_sha is None:
            return "ack:branch deleted" if tag == "closed" else "close:branch deleted"
        if now_sha == "?":
            return ""
        if now_sha != sha:
            return "ack:new head"
        landed = (rec.get("landed") or {}).get(ref)
        if landed and landed[0] == sha and is_landed(landed[1]):
            return f"close:landed ({landed[1]})"
        if b is not None and tag == "red" and seen.run_state(b) == "green":
            return "close:proof green at the head"
        return ""
    if refkind == "JOB":
        n = ob["thread"]
        issue = seen.issues.get(n)
        if tag == "assign" and issue is not None and seen.roster.of_labels(issue.labels):
            return "ack:owner labelled"
        if tag == "start" and o is not None and (o.prs or o.branches):
            return "ack:work pushed"
        if issue is None and seen.issue(n).get("state") == "closed":
            return "close:issue closed"
        return ""
    return ""


def owed_replies(state: State) -> dict[tuple[int, str], int]:
    """(thread, agent) -> how many live reply-ackable obligations that agent owes on that thread."""
    out: dict[tuple[int, str], int] = {}
    for rec in state.records():
        for ob in rec["obligations"]:
            if is_live(ob) and "reply" in ob["ack_rule"]:
                out[(ob["thread"], ob["to"])] = out.get((ob["thread"], ob["to"]), 0) + 1
    return out


def acks_by_reply(rec: dict, replies: dict, events: Events, board: int, owes: dict) -> None:
    """A reply acks only from the addressee, after the obligation (an ask: after the ask), and only when it names
    it (its O-id, or the ask's comment id) or is on the obligation's own thread where the addressee owes nothing
    else. Never an unrelated post on the shared board (#350 hole 7)."""
    for ob in rec["obligations"]:
        if not is_live(ob) or "reply" not in ob["ack_rule"]:
            continue
        since = at(ob.get("asked")) if ob["kind"] == "ask" and ob.get("asked") else at(ob["created"])
        names = [ob["id"]] + ([ob["version"]] if ob["kind"] == "ask" else [])
        alone = ob["thread"] not in (0, board) and owes.get((ob["thread"], ob["to"]), 0) == 1
        for t, agent, body in replies.get(ob["thread"], []):
            if agent != ob["to"] or t <= since:
                continue
            named = any(re.search(rf"(?<![\w-]){re.escape(x)}\b|issuecomment-{re.escape(x)}\b", body) for x in names)
            if named or alone:
                how = "reply naming it" if named else "reply on the thread"
                ob["ack"] = {"how": how, "at": iso(t), "by": agent}
                ob["unseen"] = None
                events("acked", rec["job"], id=ob["id"], how=how)
                break


class ShadowOutbox:
    """Stage 1: nothing is posted. A send is recorded as what would have gone, and always 'succeeds'."""
    live = False

    def send(self, rec: dict, ob: dict, send: int, attempt: int) -> None:
        return None


class CommentOutbox:
    """Stage 2's delivery, built and tested now, never wired in stage 1 (mode() reads `live` as off until
    STAGE2_READY).
    The marker on the delivered comment is the receipt: a crash before the state commit never re-sends."""
    live = True

    def __init__(self, gh, roster: L.Roster, board: int):
        self.gh, self.roster, self.board = gh, roster, board

    def send(self, rec: dict, ob: dict, send: int, attempt: int) -> None:
        name = self.roster.address(ob["to"]) if ob["to"] in self.roster.agents else ob["to"].capitalize()
        again = " (again: no ack yet)" if send > 1 else ""
        due = central(at(ob["due"]))
        mark = SENT_MARK.format(id=ob["id"], send=send, attempt=attempt)
        what = ob["what"]
        if ob["via"] == "email":
            # Muse (#350 hole 6): on the job's issue or the board, never a PR (the bridge's token is Issues-only);
            # plain text with no links, code marks or orders (work/tasks/mail-to-muse.md); labelled wake:muse so the
            # bridge mails it. The receipt marker goes in only after the label lands (as switch.py's muse()), so a
            # failed label leaves no receipt and the next pass sends again.
            where = int(rec.get("issue") or 0) or self.board
            body = strip_mentions(f"CLAUDE_TO_MUSE: {rec['job'] or 'thread'} {what.replace('`', '')}{again}\n\n"
                                  f"From the switchboard's reconciler (Claude). Due {due} Central. When it is "
                                  f"done, reply on this thread or post /ack {ob['id']}.")
            resp = self.gh.comment(where, body, f"reconciler: {ob['id']} to muse on #{where}")
            self.gh.write("POST", f"repos/{self.gh.repo}/issues/{where}/labels", {"labels": [MUSE_LABEL]},
                          f"label #{where} {MUSE_LABEL} (the bridge mails Muse, then removes it)")
            cid = (resp if isinstance(resp, dict) else {}).get("id")
            if cid is not None:
                self.gh.write("PATCH", f"repos/{self.gh.repo}/issues/comments/{cid}", {"body": f"{body}\n\n{mark}"},
                              f"reconciler: mark {ob['id']} delivered")
            return
        n = ob["thread"] or self.board
        if ob["via"] == "relay":
            # Codex (#350 hole 5): its app does not answer the bot's mentions, so the ask goes to the integrator,
            # who posts the mention as the team login. The mention is in a code span: the bot's post pings no one.
            body = (strip_mentions(f"To Claude:{again} relay to {name} on #{n}: {what}.") +
                    f" Post `@codex review` on #{n} yourself: the Codex app does not answer the bot's mentions."
                    f"\n\nDue {due} Central. Ack with `/ack {ob['id']}` once it is posted.")
        else:
            body = strip_mentions(f"To {name}:{again} {what}\n\nDue {due} Central. Ack with `/ack {ob['id']}`, "
                                  "or act.")
        self.gh.comment(n, f"{body}\n\n{mark}", f"reconciler: {ob['id']} to {ob['to']}")


def strip_mentions(text: str) -> str:
    return re.sub(r"@(?=[\w-])", "", text)


class ShadowCory:
    """Stage 1: the "Needs you" batch is written to the state (state/cory.json, the day's shadow file), not sent."""
    live = False

    def post(self, text: str, mark: str) -> str:
        return ""


class CoryNotifier:
    """Stage 2's "Needs you" delivery, built and tested now: the hourly batch as one comment on the board starting
    "To Cory:" (he is subscribed to it, so GitHub notifies him by push and mail with no mention), and the same
    text through `mailer(subject, text)` when one is wired (the second path, outside the board). The marker on the
    comment is the receipt. A failed mail after the comment landed is reported, never a reason to post again."""
    live = True

    def __init__(self, gh, board: int, mailer: Callable[[str, str], None] | None = None):
        self.gh, self.board, self.mailer = gh, board, mailer

    def post(self, text: str, mark: str) -> str:
        self.gh.comment(self.board, f"{text}\n\n{mark}", "reconciler: needs you (Cory)")
        if self.mailer is None:
            return ""
        try:
            self.mailer(text.split("\n", 1)[0], text)
        except Exception as err:   # noqa: BLE001
            return f"the mail failed ({type(err).__name__}: {str(err)[:120]}); the board comment went"
        return ""


def deliver(state: State, p: Pass, outbox, now: dt.datetime, events: Events, memo: dict, out) -> str:
    """Send what is owed: the first send, one re-send at the due time, a dead-letter at twice the due span.
    Returns "" or "backoff" when GitHub rate-limited a send (the rest wait for the next pass)."""
    found = markers(p)
    budget = MAX_SENDS
    status = ""
    for rec in state.records():
        for ob in rec["obligations"]:
            if not is_open(ob):
                continue
            src = max([found.get(ob["id"], (0, 0, now)), memo.get(ob["id"], (0, 0, now))], key=lambda x: x[:2])
            if src[0] > ob["sends"]:   # delivered before a crash or a lost commit: record it, never re-send
                ob["sends"], ob["attempts"], ob["last_sent"] = src[0], max(ob["attempts"], src[1]), iso(src[2])
                if ob["id"] in memo and memo[ob["id"]][:2] == src[:2]:   # this pass sent it; the commit was retried
                    events("sent" if outbox.live else "would_send", rec["job"], id=ob["id"], to=ob["to"],
                           via=ob["via"], ref=ob["ref"], send=src[0], attempt=src[1])
                else:
                    events("delivery_found", rec["job"], id=ob["id"], send=src[0])
            if ob["via"] == "cory":
                if not ob["sends"]:
                    ob["sends"], ob["attempts"], ob["last_sent"] = 1, 1, iso(now)
                    events("would_send" if not outbox.live else "sent", rec["job"], id=ob["id"], to=ob["to"],
                           via="cory", ref=ob["ref"], send=1, attempt=1)
                continue
            if not ob.get("dead_lettered") and now >= dead_at(ob):
                ob["dead_lettered"] = iso(now)
                events("dead_letter", rec["job"], id=ob["id"], to=ob["to"], ref=ob["ref"])
                continue
            if ob.get("dead_lettered"):
                continue
            need = ob["sends"] == 0 or (ob["sends"] == 1 and now >= at(ob["due"]))
            if not need or status or budget <= 0:
                continue
            budget -= 1
            send, attempt = ob["sends"] + 1, ob["attempts"] + 1
            try:
                outbox.send(rec, ob, send, attempt)
            except Exception as err:   # noqa: BLE001  one failed send never stops the pass; it is retried next pass
                rl = rate_limit_of(err)
                if rl:
                    ob["last_error"] = str(rl)
                    events("backoff", rec["job"], id=ob["id"], retry_after=rl.retry_after)
                    out(f"WARN        reconciler: {rl}; sends wait for the next pass")
                    status = "backoff"
                    continue
                ob["attempts"], ob["last_error"] = attempt, f"{type(err).__name__}: {str(err)[:160]}"
                events("send_failed", rec["job"], id=ob["id"], attempt=attempt, error=ob["last_error"])
                continue
            ob["sends"], ob["attempts"], ob["last_sent"], ob["last_error"] = send, attempt, iso(now), ""
            memo[ob["id"]] = (send, attempt, now)
            events("sent" if outbox.live else "would_send", rec["job"], id=ob["id"], to=ob["to"], via=ob["via"],
                   ref=ob["ref"], send=send, attempt=attempt)
    return status


def finalize(rec: dict, o: JobObs | None, lookups: Lookups, now: dt.datetime, events: Events,
             open_prs: Iterable[int] = ()) -> None:
    """Nothing of the job is open: record its one terminal outcome, unless a dead-letter still stands. Only on
    positive evidence: a PR or issue that is open, or whose read failed, keeps the job live (#350 hole 3)."""
    if rec["outcome"]:
        return
    standing = [ob for ob in rec["obligations"] if is_open(ob) and ob.get("dead_lettered")]
    if standing:
        rec["state"] = "blocked"
        rec["blocked"] = {"reason": f"dead-letter {standing[0]['id']} stands", "holder": "cory",
                          "since": standing[0]["dead_lettered"], "head": rec.get("head", "")}
        return
    for name, sha in sorted(rec["branches"].items()):   # quiet, not gone? it is still live
        seen = rec["landed"].get(name)
        if seen and is_landed(seen[1]):
            continue   # a head closed without merging is not landed: re-read it like any other (#350 hole 8)
        last = at(seen[2]) if seen and len(seen) > 2 else None
        if last and now - last < RECHECK:
            return
        now_sha = lookups.branch_sha(name)
        if now_sha is None:
            rec["landed"][name] = [sha, seen[1] if seen else "", iso(now)]
            continue
        how = lookups.landed(name, now_sha)
        rec["landed"][name] = [now_sha, how, iso(now)]
        if not is_landed(how):
            rec["state"] = "working"
            return
    outcome, why = "", ""
    if rec.get("cancelled"):
        outcome, why = "cancelled", f"/cancel by {rec['cancelled']['by']}"
    elif any(is_landed(v[1]) for v in rec["landed"].values()):
        outcome, why = "merged", "; ".join(sorted({f"{k}: {v[1]}" for k, v in rec["landed"].items()
                                                   if is_landed(v[1])}))
    if not outcome and rec["pr"]:
        if rec.get("pr_done") == "merged":
            got = {"merged": True, "state": "closed"}
        elif rec["pr"] in set(open_prs):
            return   # still open (not read this pass): the job is not over
        else:
            got = lookups.pr(rec["pr"])
        if got.get("state") != "closed" and not got.get("merged"):
            return   # open, or the read told nothing
        if got.get("merged"):
            outcome, why = "merged", f"#{rec['pr']} merged"
        elif len(rec["prs"]) > 1:
            outcome, why = "superseded", f"#{rec['pr']} closed after {len(rec['prs'])} PRs"
    if not outcome and rec["issue"]:
        got = lookups.issue(rec["issue"])
        reason = got.get("state_reason") or ""
        if got.get("state") != "closed":
            return   # open, or the read told nothing
        outcome, why = {"not_planned": ("cancelled", "issue closed as not planned"),
                        "duplicate": ("superseded", "issue closed as a duplicate"),
                        }.get(reason, ("merged" if reason == "completed" else "failed",
                                       f"issue closed ({reason or 'no reason'})"))
    if not outcome:
        outcome, why = "failed", "closed or gone with nothing of it merged"
    rec["outcome"], rec["outcome_reason"], rec["state"] = outcome, why, outcome
    for ob in rec["obligations"]:
        if is_live(ob):
            ob["closed"] = {"why": f"job {outcome}", "at": iso(now)}
            events("closed", rec["job"], id=ob["id"], why=ob["closed"]["why"])
    events("outcome", rec["job"], outcome=outcome, why=why)


def step(job: str, rec: dict, o: JobObs | None, seen: Seen, roster: L.Roster, lookups: Lookups, now: dt.datetime,
         events: Events, unmatched: dict) -> None:
    """One job's record from this pass's reading."""
    if rec["outcome"]:
        if o and (o.prs or [b for b, _ in o.branches if b.name not in rec["landed"]]):
            unmatched[f"JOB|{job}"] = f"new work on {job}, whose outcome is {rec['outcome']}"
        return
    if o is None:
        sync(rec, [], now, roster, events, lambda ob: evidence(ob, None, seen, rec, now))
        finalize(rec, None, lookups, now, events, seen.open_prs)
        return
    if o.issue is not None:
        rec["issue"] = o.issue.number
    for pr in o.prs:
        if pr.number not in rec["prs"]:
            rec["prs"].append(pr.number)
            rec["attempt"] = len(rec["prs"])
    # Landed branches (head in trunk, or a merged PR at the head) owe nothing; a head whose PR was closed without
    # merging is not landed and owes one ruling (#350 hole 8). Read once per head.
    landed: set[str] = set()
    closed: dict[str, str] = {}
    for b, _ in o.branches:
        was = rec["landed"].get(b.name)
        if not was or was[0] != b.sha:
            how = landed_of(b, lookups)
            rec["landed"][b.name] = [b.sha, how, iso(now)]
            if is_landed(how):
                events("landed", job, branch=b.name, sha=b.sha, how=how)
            elif how:
                events("closed_unmerged", job, branch=b.name, sha=b.sha, how=how)
        how = rec["landed"][b.name][1]
        if is_landed(how):
            landed.add(b.name)
        elif how:
            closed[b.name] = how
        rec["branches"][b.name] = b.sha
    owner = owner_of(o, rec, roster)
    if owner != rec["owner"]:
        if rec["owner"]:
            events("reassigned", job, frm=rec["owner"], to=owner)
        rec["owner"] = owner
    if not o.prs and rec["pr"] and not rec.get("pr_done") and rec["pr"] not in seen.open_prs:
        got = lookups.pr(rec["pr"])   # its PR closed: read once how
        if got.get("state") == "closed":
            rec["pr_done"] = "merged" if got.get("merged") else "closed"
            events("pr_closed", job, pr=rec["pr"], how=rec["pr_done"])
    cur = max(o.prs, key=lambda x: x.touched, default=None)
    if cur is not None:
        if cur.head_sha != rec["head"] and rec.get("blocked") and rec["blocked"].get("head") \
                and rec["blocked"]["head"] != cur.head_sha:
            rec["blocked"] = None
            events("unblocked", job, how="new head")
        rec["pr"], rec["head"], rec["ci"] = cur.number, cur.head_sha, cur.proof
        rec["review"] = ("LAND" if cur.action == "MERGE" else "FIX" if cur.action == "FIX" else
                         "waiting" if cur.needs_review else "")
    else:
        live = [b for b, _ in o.branches if b.name not in landed]
        if live:
            rec["head"] = max(live, key=lambda b: b.pushed_at).sha
    live_work = o.issue is not None or o.prs or any(b.name not in landed for b, _ in o.branches)
    state_now, wants = derive(o, rec, owner, now, roster, landed, closed)
    if rec.get("cancelled"):
        wants = []
    sync(rec, wants, now, roster, events, lambda ob: evidence(ob, o, seen, rec, now))
    if rec.get("cancelled") or not live_work:
        finalize(rec, o, lookups, now, events, seen.open_prs)
    else:
        rec["state"] = "blocked" if rec.get("blocked") else state_now


def reconcile(state: State, p: Pass, roster: L.Roster, lookups: Lookups, now: dt.datetime, events: Events) \
        -> list[str]:
    """One level-triggered pass over the state: pure but for the lookups. A job that raises is left as it was,
    listed UNMATCHED, and the pass goes on (#350 hole 9: a partial failure is not an outage). Returns those jobs."""
    p.now = now
    jobs, loose, thread_job = observe(p, roster)
    seen = Seen(p, lookups, roster)
    unmatched: dict[str, str] = {}
    for job in jobs:
        if job not in state.jobs:
            state.jobs[job] = new_record(job, now)
            events("job", job)
    apply_commands(state, p, thread_job, events, unmatched, roster)
    replies = thread_replies(p, roster)

    befores: dict[str, tuple[str, str]] = {}
    degraded: list[str] = []
    for job, rec in sorted(state.jobs.items()):
        befores[job] = (rec["state"], json.dumps(rec, sort_keys=True))
        try:
            step(job, rec, jobs.get(job), seen, roster, lookups, now, events, unmatched)
        except Exception as err:   # noqa: BLE001
            state.jobs[job] = json.loads(befores[job][1])
            degraded.append(job)
            why = f"{type(err).__name__}: {L.clean(str(err), 120)}"
            unmatched[f"ERROR|{job}"] = f"this pass could not reconcile {job} ({why}); its record is as it was"
            events("degraded", job, error=why)

    # asks on threads that name no job
    wants = []
    for a in loose:
        who = roster.of_name(a.target)
        wants.append(Want("ask", who.agent if who else a.target.lower(), a.cid, f"ASK|ask on #{a.thread}",
                          a.thread, f"ask from {L.clean(a.author, 20)} on #{a.thread}", "reply|ack", a.created_at))
    sync(state.threads, wants, now, roster, events, lambda ob: evidence(ob, None, seen, state.threads, now))

    owes = owed_replies(state)
    for rec in state.records():
        acks_by_reply(rec, replies, events, p.feed.board, owes)
    for job, rec in sorted(state.jobs.items()):
        before = befores[job]
        due = [at(ob["due"]) for ob in rec["obligations"] if is_open(ob)]
        rec["deadline"] = iso(min(due)) if due else None
        if rec["state"] != before[0]:
            events("state", job, frm=before[0], to=rec["state"])
        if json.dumps(rec, sort_keys=True) != before[1]:
            rec["updated"] = iso(now)

    # UNMATCHED: the ledger's, and the reconciler's own; each keeps its first-seen time for its age
    for it in p.led.items:
        if it.unmatched:
            unmatched[it.ident] = f"{it.unmatched} ({L.clean(it.state, 80)})"
    for missing in p.led.dropped(p.feed):
        unmatched[f"DROPPED|{missing}"] = "the ledger's classifier dropped this input"
    for ident, why in unmatched.items():
        if ident not in state.unmatched:
            state.unmatched[ident] = {"why": why, "first_seen": iso(now)}
            events("unmatched", "", ident=ident, why=why)
        state.unmatched[ident]["why"], state.unmatched[ident]["last_seen"] = why, iso(now)
    for ident in [k for k in state.unmatched if k not in unmatched]:
        events("matched", "", ident=ident)
        del state.unmatched[ident]
    return degraded


# ---------- "Needs you": what reaches Cory ----------
def last_pass(events_text: str) -> dt.datetime | None:
    for line in reversed(events_text.splitlines()):
        if '"pass"' in line:
            try:
                e = json.loads(line)
            except ValueError:
                continue
            if e.get("ev") == "pass":
                return at(e["t"])
    return None


def pipe_alarms(state: State, p: Pass, now: dt.datetime) -> list[tuple[str, str]]:
    """Lines for Cory about the pipe itself: a gap in the passes (the clock not dispatching, #350 hole 1) and a
    token close to expiry (hole 14). Each is keyed so it is reported once."""
    out = []
    last = last_pass(state.events_text)
    if last and now - last >= PASS_GAP:
        mins = round((now - last).total_seconds() / 60)
        out.append((f"gap:{iso(last)}", f"- Needs you: PIPE: no switchboard pass from {central(last)} to "
                    f"{central(now)} Central ({mins} min; the clock dispatches every 10 min). The 10-minute trigger "
                    "(muse_bridge.gs) or its health check is not running; since " + central(last) + " Central."))
    for name, exp in sorted((p.tokens or {}).items()):
        if exp and exp - now <= TOKEN_WARN:
            gone = " (it has expired)" if exp <= now else ""
            out.append((f"token:{name}:{exp:%Y-%m-%d}", f"- Needs you: the {name} token expires {central(exp)} "
                        f"Central{gone}. Renew it and update its secret before then."))
    return out


def needs_you(state: State, roster: L.Roster) -> list[tuple[str, str]]:
    """One line per item Cory must handle: what, who failed, since when."""
    out = []
    for rec in state.records():
        for ob in rec["obligations"]:
            if not is_open(ob) or not (ob.get("dead_lettered") or ob["via"] == "cory"):
                continue
            who = roster.address(ob["to"]) if ob["to"] in roster.agents else ob["to"].capitalize()
            if ob.get("dead_lettered"):
                failed, since = (f"{who} did not act ({ob['sends']} send(s) by {ob['via']}, no ack)",
                                 ob["dead_lettered"])
            else:
                failed, since = f"{who} runs only when you start it", ob["created"]
            where = f" on #{ob['thread']}" if ob.get("thread") else ""
            out.append((ob["id"], f"- Needs you: {rec['job'] or 'thread'} {ob['kind']}{where}: "
                                  f"{L.clean(ob['what'], 120)}. {failed}; since {central(at(since))} Central. "
                                  f"Done: /ack {ob['id']}"))
    return out


def notify_cory(state: State, p: Pass, cory, roster: L.Roster, now: dt.datetime, events: Events, memo: dict,
                out) -> None:
    """The "Needs you" batch (#350 hole 2): every item not yet reported, in one message, at most once an hour.
    Stage 1 (ShadowCory) records the message it would send; stage 2 (CoryNotifier) posts it. A failed post is
    retried next pass; a batch posted before a crash or a lost commit is found by its marker, never sent twice."""
    cs = state.cory
    for c in p.comments:
        if c.login != rules.BOT_LOGIN:
            continue
        for m in CORY_RE.finditer(c.body):
            for k in filter(None, m.group(2).split(",")):
                cs["reported"].setdefault(k, m.group(1))
            if not cs["last_batch"] or m.group(1) > cs["last_batch"]:
                cs["last_batch"] = m.group(1)
    for k, line in pipe_alarms(state, p, now):
        if k not in cs["reported"]:
            cs.setdefault("pending", {})[k] = line
    items = needs_you(state, roster) + sorted(cs.get("pending", {}).items())
    standing = {k for k, _ in items}
    for k in [k for k, v in cs["reported"].items() if k not in standing and now - at(v) > dt.timedelta(days=7)]:
        del cs["reported"][k]
    if "cory" in memo:   # this pass posted it already, and the commit was retried on a fresh read
        batch, keys, text, warn = memo["cory"]
    else:
        new = [(k, line) for k, line in items if k not in cs["reported"]]
        last = at(cs["last_batch"])
        if not new or (last and now - last < CORY_EVERY):
            return
        batch, keys = iso(now), [k for k, _ in new]
        more = len(standing) - len(new)
        lines = [f"To Cory: Needs you ({len(new)}), from the switchboard at {central(now)} Central.", ""]
        lines += [line for _, line in new]
        if more > 0:
            lines += ["", f"Still standing from earlier batches: {more} (the day's shadow file lists them)."]
        text = strip_mentions("\n".join(lines))
        try:
            warn = cory.post(text, CORY_MARK.format(batch=batch, keys=",".join(keys)))
        except Exception as err:   # noqa: BLE001
            cs["last_error"] = f"{iso(now)} {type(err).__name__}: {str(err)[:160]}"
            events("cory_failed", "", error=cs["last_error"], keys=keys)
            out(f"WARN        reconciler: the Needs-you batch for Cory did not go ({cs['last_error']}); "
                "retried next pass")
            return
        memo["cory"] = (batch, keys, text, warn)
    cs["last_batch"] = batch
    for k in keys:
        cs["reported"][k] = batch
        cs.get("pending", {}).pop(k, None)
    cs["batches"] = (cs["batches"] + [{"at": batch, "keys": keys, "text": text, "live": cory.live}])[-MAX_BATCHES:]
    cs["last_error"] = warn or ""
    events("needs_you" if cory.live else "would_notify_cory", "", keys=keys, n=len(keys))
    if warn:
        out(f"WARN        reconciler: {warn}")


def ledger_notices(state: State, p: Pass, events: Events) -> None:
    """Record each ledger notice the first time this pass sees its key: what the ledger actually sent."""
    recorded = set()
    for line in state.events_text.splitlines():
        if '"ledger_sent"' in line:
            try:
                recorded.add(json.loads(line).get("key"))
            except ValueError:
                continue
    for it in p.led.items:
        for st in (1, 2):
            k = it.key(st)
            if k in p.ledger_sent and k not in recorded:
                recorded.add(k)
                events("ledger_sent", it.job, ref=it.ident, to=it.holder.agent, stage=st, key=k)


# ---------- the shadow diff and the SLO numbers ----------
def read_events(text: str) -> list[dict]:
    out = []
    for line in text.splitlines():
        try:
            out.append(json.loads(line))
        except ValueError:
            continue
    return out


def slo(state: State, evs: list[dict], now: dt.datetime) -> dict:
    passes = sorted(at(e["t"]) for e in evs if e.get("ev") == "pass" and now - at(e["t"]) <= dt.timedelta(days=1))
    gaps = [b - a for a, b in zip(passes, passes[1:])]
    late = [ob["id"] for r in state.records() for ob in r["obligations"]
            if is_open(ob) and not ob.get("dead_lettered") and ob["via"] != "cory" and now >= dead_at(ob)]
    old = [k for k, v in state.unmatched.items() if now - at(v["first_seen"]) >= dt.timedelta(hours=24)]
    week = [r for r in state.jobs.values() if r["outcome"] and now - at(r["updated"]) <= dt.timedelta(days=7)]
    return {"passes_24h": len(passes), "largest_gap_min": round(max(gaps).total_seconds() / 60, 1) if gaps else None,
            "past_2x_due_not_dead": len(late), "unmatched_over_24h": len(old), "unmatched": len(state.unmatched),
            "jobs": len(state.jobs), "jobs_terminal": sum(1 for r in state.jobs.values() if r["outcome"]),
            "jobs_terminal_7d": len(week),
            "open_obligations": sum(1 for r in state.records() for ob in r["obligations"] if is_open(ob)),
            "dead_letters": sum(1 for r in state.records() for ob in r["obligations"]
                                if is_open(ob) and ob.get("dead_lettered")),
            "unseen": sum(1 for r in state.records() for ob in r["obligations"] if is_live(ob) and ob.get("unseen")),
            "cory_unreported": sum(1 for r in state.records() for ob in r["obligations"]
                                   if is_open(ob) and (ob.get("dead_lettered") or ob["via"] == "cory")
                                   and ob["id"] not in state.cory["reported"]) + len(state.cory.get("pending", {})),
            "cory_last_error": state.cory.get("last_error") or "",
            "degraded": sum(1 for k in state.unmatched if k.startswith("ERROR|"))}


def shadow_md(state: State, evs: list[dict], now: dt.datetime) -> tuple[str, dict]:
    day = now.strftime("%Y-%m-%d")
    today = [e for e in evs if (e.get("t") or "").startswith(day)]
    would = {}
    for e in today:
        if e["ev"] == "would_send":
            would.setdefault((e["ref"], e["to"]), []).append(e)
    sent = {}
    for e in today:
        if e["ev"] == "ledger_sent":
            sent.setdefault((e["ref"], e["to"]), []).append(e)
    both, union = set(would) & set(sent), set(would) | set(sent)
    agree = round(100.0 * len(both) / len(union), 1) if union else 100.0
    s = slo(state, evs, now)
    s.update({"agree_pct": agree, "would_send": len(would), "ledger_sent": len(sent), "matched": len(both)})
    lines = [f"# Reconciler shadow, {day} (UTC day)", "",
             f"Shadow mode (J-072 stage 1): nothing here was posted. Updated {central(now)} Central.", "",
             "## Would send vs. what the ledger sent", "",
             f"Agreement {agree}%: {len(both)} matched of {len(union)} (item, holder) pairs; "
             f"{len(would)} the reconciler would send, {len(sent)} the ledger sent.", "",
             f"### Would send, the ledger did not ({len(set(would) - set(sent))})", ""]
    for k in sorted(set(would) - set(sent)):
        e = would[k][0]
        lines.append(f"- `{e['id']}` {k[0]} to {k[1]} by {e['via']}, send {e['send']}, {central(at(e['t']))}")
    lines += ["", f"### The ledger sent, the reconciler would not ({len(set(sent) - set(would))})", ""]
    for k in sorted(set(sent) - set(would)):
        e = sent[k][0]
        lines.append(f"- {k[0]} to {k[1] or 'nobody'}, stage {e['stage']}, first seen {central(at(e['t']))}")
    lines += ["", "Each divergence gets a written reason on #350 before the stage 2 cutover.", "",
              "## Needs you (dead-letters and manual asks)", ""]
    for r in state.records():
        for ob in r["obligations"]:
            if is_open(ob) and (ob.get("dead_lettered") or ob["via"] == "cory"):
                why = "dead-letter" if ob.get("dead_lettered") else f"start {ob['to']}"
                lines.append(f"- `{ob['id']}` {r['job'] or 'thread'} {ob['kind']} to {ob['to']}: "
                             f"{L.clean(ob['what'], 100)} ({why})")
    batches = [b for b in state.cory["batches"] if (b.get("at") or "").startswith(day)]
    sent = "went" if any(b.get("live") for b in batches) else "would go"
    lines += ["", f"## What {sent} to Cory today ({len(batches)} batch(es), at most one an hour)", ""]
    if state.cory.get("last_error"):
        lines += [f"Last error: {L.clean(state.cory['last_error'], 200)}", ""]
    for b in batches:
        lines += [f"At {central(at(b['at']))} Central:", "", "```", b["text"], "```", ""]
    unseen = [(r, ob) for r in state.records() for ob in r["obligations"] if is_live(ob) and ob.get("unseen")]
    lines += ["", f"## Unseen: missing {UNSEEN_AFTER} passes, not closed ({len(unseen)})", ""]
    for r, ob in unseen:
        lines.append(f"- `{ob['id']}` {r['job'] or 'thread'} {ob['kind']} to {ob['to']}: "
                     f"{L.clean(ob['what'], 100)} (since {central(at(ob['unseen']))})")
    lines += ["", f"## UNMATCHED ({len(state.unmatched)})", ""]
    for k, v in sorted(state.unmatched.items(), key=lambda kv: kv[1]["first_seen"]):
        lines.append(f"- {L.clean(k, 80)}: {L.clean(v['why'], 120)} · {L.age_text(now - at(v['first_seen']))}")
    lines += ["", "## SLO (section 6)", "", "| SLO | now |", "|---|---|",
              f"| Largest gap between non-dry passes, last 24 h | "
              f"{s['largest_gap_min'] if s['largest_gap_min'] is not None else 'n/a'} min ({s['passes_24h']} passes) |",
              f"| Obligations past 2x due and not dead-lettered | {s['past_2x_due_not_dead']} |",
              f"| UNMATCHED older than 24 h | {s['unmatched_over_24h']} (of {s['unmatched']}) |",
              f"| Jobs with a terminal outcome | {s['jobs_terminal']} of {s['jobs']} "
              f"({s['jobs_terminal_7d']} in the last 7 d) |",
              f"| Open obligations / dead-letters | {s['open_obligations']} / {s['dead_letters']} |", ""]
    return "\n".join(lines), s


# ---------- the stores ----------
@dataclasses.dataclass
class Snapshot:
    head: str | None       # the branch's commit, None when the branch does not exist
    tree: str | None
    files: dict[str, str]


class GitStore:
    """`switchboard/state` through the REST Git Data API. A read is the ref, its tree and at most four blobs; a
    commit lands by a ref update with force=false, which GitHub refuses unless it fast-forwards: the
    compare-and-swap. Creating the branch is the same: a second creator gets 422 and re-reads."""

    def __init__(self, gh, branch: str = STATE_BRANCH):
        self.gh, self.branch = gh, branch

    def read(self, month: str) -> Snapshot:
        from ghapi import GitHubError
        repo = self.gh.repo
        try:
            head = self.gh.get(f"repos/{repo}/git/ref/heads/{self.branch}")["object"]["sha"]
        except GitHubError as err:
            if "HTTP 404" in err.detail:
                return Snapshot(None, None, {})
            raise
        tree = self.gh.get(f"repos/{repo}/git/trees/{head}?recursive=1")
        if tree.get("truncated"):
            raise RuntimeError(f"{self.branch}'s tree listing was cut short")
        blobs = {e["path"]: e["sha"] for e in tree.get("tree") or [] if e.get("type") == "blob"}
        files = {}
        for path in (INDEX, THREADS, UNMATCHED, CORY_FILE, f"{EVENTS_DIR}/{month}.ndjson"):
            if path in blobs:
                got = self.gh.get(f"repos/{repo}/git/blobs/{blobs[path]}")
                files[path] = base64.b64decode(got["content"]).decode("utf-8")
        return Snapshot(head, tree["sha"], files)

    def commit(self, base: Snapshot, changes: dict[str, str], message: str) -> str:
        from ghapi import GitHubError
        repo = self.gh.repo
        entries = [{"path": p, "mode": "100644", "type": "blob", "content": t} for p, t in sorted(changes.items())]
        tree_body = {"tree": entries} | ({"base_tree": base.tree} if base.tree else {})
        tree = self.gh.write("POST", f"repos/{repo}/git/trees", tree_body, f"state tree ({len(entries)} files)")
        commit = self.gh.write("POST", f"repos/{repo}/git/commits",
                               {"message": message, "tree": tree["sha"], "parents": [base.head] if base.head else []},
                               "state commit")
        try:
            if base.head:
                self.gh.write("PATCH", f"repos/{repo}/git/refs/heads/{self.branch}",
                              {"sha": commit["sha"], "force": False}, f"fast-forward {self.branch}")
            else:
                self.gh.write("POST", f"repos/{repo}/git/refs",
                              {"ref": f"refs/heads/{self.branch}", "sha": commit["sha"]}, f"create {self.branch}")
        except GitHubError as err:
            if "HTTP 422" in err.detail or "HTTP 409" in err.detail:
                raise Conflict(err.detail[:160]) from err
            raise
        return commit["sha"]


class LocalStore:
    """The same contract on a directory, for the dry run: HEAD is a counter, a commit checks it first."""

    def __init__(self, root: pathlib.Path):
        self.root = pathlib.Path(root)

    def read(self, month: str) -> Snapshot:
        head = self.root / "HEAD"
        if not head.exists():
            return Snapshot(None, None, {})
        files = {}
        for path in [self.root / INDEX, self.root / THREADS, self.root / UNMATCHED, self.root / CORY_FILE,
                     self.root / EVENTS_DIR / f"{month}.ndjson"]:
            if path.exists():
                files[str(path.relative_to(self.root))] = path.read_text(encoding="utf-8")
        h = head.read_text().strip()
        return Snapshot(h, h, files)

    def commit(self, base: Snapshot, changes: dict[str, str], message: str) -> str:
        head = self.root / "HEAD"
        cur = head.read_text().strip() if head.exists() else None
        if cur != base.head:
            raise Conflict(f"local HEAD {cur} is not {base.head}")
        for p, text in changes.items():
            f = self.root / p
            f.parent.mkdir(parents=True, exist_ok=True)
            f.write_text(text, encoding="utf-8")
        new = str(int(cur or 0) + 1)
        head.write_text(new + "\n")
        return new


# ---------- one pass ----------
@dataclasses.dataclass
class Result:
    status: str              # "committed", "unchanged", "dry", "backoff", "gave-up"
    head: str | None
    slo: dict
    tries: int
    events: int
    degraded: tuple[str, ...] = ()


def run_pass(store, p: Pass, roster: L.Roster, lookups: Lookups, now: dt.datetime, outbox=None, out=print,
             dry: bool = False, ping: Callable[[], None] | None = None, cory=None) -> Result:
    """Read the state, reconcile, deliver (shadow: record only), batch the "Needs you" lines for Cory (shadow:
    record only), write the daily diff, commit by CAS; a rejected commit re-reads and redoes the pass. A dry pass
    commits nothing, records no pass, sends nothing to Cory and never pings."""
    outbox = outbox or ShadowOutbox()
    cory = cory or ShadowCory()
    month = now.strftime("%Y-%m")
    memo: dict = {}
    s: dict = {}
    p.now = now
    for tries in range(1, MAX_CAS_TRIES + 1):
        snap = store.read(month)
        state = State.load(snap.files, month)
        events = Events(now)
        degraded = reconcile(state, p, roster, lookups, now, events)
        for job in degraded:
            out(f"DEGRADED    reconciler: {job} not reconciled this pass ({state.unmatched[f'ERROR|{job}']['why']})")
        ledger_notices(state, p, events)
        status = deliver(state, p, outbox, now, events, memo, out)
        if not dry:
            notify_cory(state, p, cory, roster, now, events, memo, out)
        if not dry:
            events("pass", "", tries=tries, jobs=len(state.jobs), status=status or "ok")
        evs = read_events(state.events_text) + events.items
        md, s = shadow_md(state, evs, now)
        if dry:
            return Result("dry", snap.head, s, tries, len(events.items), tuple(degraded))
        changes = state.changes()
        changes[f"{EVENTS_DIR}/{month}.ndjson"] = state.events_text + events.text()
        changes[f"{SHADOW_DIR}/{now.strftime('%Y-%m-%d')}.md"] = md
        try:
            head = store.commit(snap, changes, f"[skip ci] reconciler pass {iso(now)}: {len(events.items)} events")
        except Conflict as err:
            out(f"WARN        reconciler: the state branch moved ({err}); re-reading (try {tries})")
            continue
        except Exception as err:   # noqa: BLE001
            rl = rate_limit_of(err)
            if rl is None:
                raise
            out(f"WARN        reconciler: commit {rl}; the next pass redoes it")
            return Result("backoff", snap.head, s, tries, len(events.items))
        if ping:
            ping()
        return Result(status or "committed", head, s, tries, len(events.items), tuple(degraded))
    out(f"ERROR       reconciler: the state branch moved {MAX_CAS_TRIES} times; this pass is dropped (next one redoes it)")
    return Result("gave-up", None, s, MAX_CAS_TRIES, 0)


# ---------- reading GitHub, and the switch's one call ----------
class CountingGH:
    """The switch's client with this pass's GETs remembered (the comment listing is read once for the reader and
    the commands) and counted."""

    def __init__(self, gh):
        self.inner, self.repo, self.dry_run = gh, gh.repo, gh.dry_run
        self.cache: dict[str, object] = {}
        self.calls = 0

    def get(self, path: str, accept: str = ""):
        if (path, accept) not in self.cache:
            self.calls += 1
            self.cache[(path, accept)] = self.inner.get(path, accept)
        return self.cache[(path, accept)]

    def paged(self, path: str, key: str = "") -> list:
        sep = "&" if "?" in path else "?"
        items: list = []
        for page in range(1, 6):
            got = self.get(f"{path}{sep}per_page=100&page={page}")
            batch = got[key] if key else got
            items.extend(batch)
            if len(batch) < 100:
                break
        return items

    def write(self, *a, **kw):
        self.calls += 1
        return self.inner.write(*a, **kw)

    def comment(self, *a, **kw):
        raise RuntimeError("the shadow reconciler posts nothing")

    def __getattr__(self, name):   # the rest of the client (get_text, get_bytes) as is, counted
        self.calls += 1
        return getattr(self.inner, name)


def read_pass(sw, gh: CountingGH, steered: list, board: int, proof_workflow: str, system_branches: Iterable[str],
              system_prs: Iterable[int], roster: L.Roster, cfg: dict) -> Pass:
    import types
    shim = types.SimpleNamespace(gh=gh, team=sw.team, run_state=sw.run_state)
    feed, sent = L.read_feed(shim, steered, sw.now, board, proof_workflow, system_branches, system_prs)
    led = L.classify(feed, sw.now, roster)
    raw, _ = L.page_list(gh, f"repos/{gh.repo}/issues/comments?since={L.iso(sw.now - L.LIVE)}")   # cached: read once
    comments = []
    for c in raw:
        try:
            n = int((c.get("issue_url") or "").rsplit("/", 1)[1])
        except (ValueError, IndexError):
            continue
        comments.append(Comment(str(c.get("id")), n, L.parse(c.get("created_at")) or sw.now,
                                (c.get("user") or {}).get("login") or "", c.get("body") or ""))
    body = (gh.get(f"repos/{gh.repo}/issues/{board}") or {}).get("body") or ""
    ledger_sent = set(sent) | set(L.section_state(body)[0])
    trusted = frozenset(cfg.get("verdict_logins") or ["IssisX"]) - {rules.BOT_LOGIN}
    # Each token's expiry, when the client reads GitHub's `github-authentication-token-expiration` header (#350
    # hole 14): {name: datetime}. ghapi.py does not read it yet, so this is empty until it does.
    tokens = dict(getattr(gh.inner, "token_expiry", None) or {})
    return Pass(feed, led, comments, ledger_sent, trusted, tokens=tokens)


# Stage 2's switch (#350): set True, and `reconciler: live` in work/switchboard.yml, and the same pass posts its
# notices (CommentOutbox) and Cory's hourly batch (CoryNotifier) instead of recording them. Until then `live`
# reads as off, and stage 1 stays shadow.
STAGE2_READY = False


def mode(root: pathlib.Path) -> str:
    import yaml
    path = pathlib.Path(root) / CONFIG
    if not path.exists():
        return "off"
    got = (yaml.safe_load(path.read_text(encoding="utf-8")) or {}).get("reconciler")
    if got == "live" and STAGE2_READY:
        return "live"
    return "shadow" if got == "shadow" else "off"


def outboxes(live: bool, gh, roster: L.Roster, board: int, mailer: Callable | None = None):
    """(the notice outbox, Cory's notifier) for a mode: shadow records, live posts."""
    if not live:
        return ShadowOutbox(), ShadowCory()
    return CommentOutbox(gh, roster, board), CoryNotifier(gh, board, mailer)


def sweep(sw, steered: list, board: int, proof_workflow: str, system_branches: Iterable[str],
          system_prs: Iterable[int]) -> Result | None:
    """The switch's one call, at the end of a non-dry full sweep: shadow, or live once STAGE2_READY is set."""
    import yaml
    which = mode(sw.root)
    if which == "off" or sw.gh.dry_run:
        return None
    cfg = yaml.safe_load((sw.root / "work/agents.yml").read_text(encoding="utf-8")) or {}
    roster = L.Roster(cfg)
    gh = CountingGH(sw.gh)
    p = read_pass(sw, gh, steered, board, proof_workflow, set(system_branches) | {STATE_BRANCH}, system_prs,
                  roster, cfg)
    outbox, cory = outboxes(which == "live", sw.gh, roster, board)   # posts go through the switch's own client
    res = run_pass(GitStore(gh), p, roster, GitHubLookups(gh), sw.now, outbox, sw.out, cory=cory)
    sw.out(f"reconciler  {which} pass {res.status}: {res.events} events, {gh.calls} GitHub calls, "
           f"tries {res.tries}; SLO {json.dumps(res.slo, sort_keys=True)}")
    return res


def main() -> int:
    """A read-only pass against the live repo, the state in a local directory (never pushed)."""
    import argparse
    import yaml
    import switch
    from ghapi import GitHub
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--repo", default=switch.REPO)
    ap.add_argument("--state-dir", required=True)
    args = ap.parse_args()
    now = dt.datetime.now(dt.timezone.utc)
    real = GitHub(args.repo, True)
    sw = switch.Switch(real, HERE.parents[1], now)
    gh = CountingGH(real)
    sw.gh = gh
    steered = []
    for pr in sw.target_prs(0, ""):
        facts, ctx = sw.pr_facts(pr)
        d = rules.merge_decision(facts, sw.team.vendor_of_branch(facts.head_ref), sw.team)
        steered.append(switch.Steered(facts, ctx, d))
    before = gh.calls
    cfg = yaml.safe_load((sw.root / "work/agents.yml").read_text(encoding="utf-8")) or {}
    roster = L.Roster(cfg)
    p = read_pass(sw, gh, steered, switch.BOARD_ISSUE, switch.PROOF_WORKFLOW,
                  {switch.LEDGER_BRANCH, switch.INBOX_BRANCH, STATE_BRANCH}, {switch.INBOX_PR}, roster, cfg)
    res = run_pass(LocalStore(pathlib.Path(args.state_dir)), p, roster, GitHubLookups(gh), now, ShadowOutbox())
    print(f"reconciler dry run at {central(now)} Central: {res.status}, {res.events} events, "
          f"{gh.calls - before} GitHub calls after the steer read ({before} for it)")
    print(json.dumps(res.slo, indent=1, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
