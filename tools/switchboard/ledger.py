#!/usr/bin/env python3
"""The ledger: every open item in the collaboration, who holds it, its age and its deadline, written into the
board's body (the open issue labeled `board`) by the switchboard each full sweep, so nothing sits unseen until
someone sweeps by hand.

Items, from GitHub alone:
  PR          every open PR, from the sweep's own reading (switch.py's Steered list)
  BRANCH      every branch pushed in the last LIVE_DAYS days with no open PR (archive/* and the switch's own
              branches are counted as system, never listed). One whose head is in trunk (landed) or is the head of
              a closed PR, merged or not, is resolved and counted; only commits after that PR's head list it again,
              and its state says how many
  JOB         every open issue labeled `job`
  ASK         every unanswered `To <Name>:` ask (tools/board/asks.py decides what is answered)
  TRUNK       trunk's newest proof run, when it is red
  SUPERSEDED  an older live branch of a job that has a newer one (protected and plain branches are separate
              lanes): its owner closes it or says `keep <branch>` in a comment
Every red proof run in the window lands on one of these or counts as resolved (a newer run or head, or a branch
that is gone). Whatever cannot be placed (no job id, an unknown branch prefix, a PR with no `Job:` line, a job
issue with no owner, a listing that was cut short) is an UNMATCHED item: never skipped.

Each item has an owner (the issue's `owner:` label, else the branch prefix in work/agents.yml), a holder (who
acts next), an age and a deadline: 2 h for a red branch, an unanswered ask and a green PR with no review;
4 h untouched for everything else (stale; Cory 11-12: nothing sits a day unnoticed). Past twice its deadline an item is listed under ESCALATED.

The ledger is the board's list, not a notifier (J-072: one owner of notices). The reconciler
(tools/switchboard/reconciler.py) tells each holder, with an ack, one re-send and a dead-letter; with it live and
`notices: v2` in work/switchboard.yml, run() is called with notify=False and posts nothing. Without that switch
(the reconciler off or in shadow, or no `notices: v2`) the ledger's old notices are what tells the holder: past
its deadline once through its own channel (Muse: a `CLAUDE_TO_MUSE:` comment plus `wake:muse`; a Codex-held item
goes to the integrator; the others: a `To <Name>:` comment on the item's thread), past twice the deadline once
more. Such a notice is sent once per item, version (head, ask, last touch) and stage: the keys sent live in a
hidden marker in the ledger section and in each notice comment's own marker.

The section between `<!-- ledger:start -->` and `<!-- ledger:end -->` is the only text the ledger writes; the
markers go under the body's first heading when missing. Nothing is ever closed or deleted.

Usage (a dry run, read-only: prints the section, and every notice it would send when the ledger still notifies):
  python3 tools/switchboard/ledger.py [--repo IssisX/ScraperX]
"""

from __future__ import annotations

import dataclasses
import datetime as dt
import hashlib
import json
import pathlib
import re
import subprocess
import sys
import urllib.parse
from typing import Callable, Iterable

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "board"))

import asks  # noqa: E402  tools/board/asks.py: what counts as an ask and as its answer
import muse_git  # noqa: E402  Muse's git channel: its two branches are system branches, never lanes
import rules  # noqa: E402

LIVE = dt.timedelta(days=3)            # a branch pushed this recently is live; asks are read this far back
TOUCH_WINDOW = dt.timedelta(days=2)    # how far back touches are read: past 2 x STALE an item is escalated anyway
ACT = dt.timedelta(hours=2)            # red branch or PR, unanswered ask, green PR with no review
STALE = dt.timedelta(hours=4)          # everything else: untouched this long (Cory 11-12, was 24 h)
MAX_NOTICES = 10                       # notice comments per sweep; the rest come next sweep
MAX_MAIL_ITEMS = 15                    # items in Muse's one mail per sweep
MAX_SECTION = 6000                     # chars of the visible section before it collapses to per-holder counts
MAX_BODY = 65000                       # GitHub's issue body limit is 65536
MAX_PAGES = 10                         # pages read from any cursor or page listing (1000 items)
MAX_COMMIT_READS = 25                  # head commits read per sweep for a `Job:` line
CLOSED_WINDOW = dt.timedelta(days=14)  # closed PRs read this far back (by last update), to match branch heads
MAX_KEYS = 800

START, END = "<!-- ledger:start -->", "<!-- ledger:end -->"
SENT = re.compile(r"<!-- ledger:sent v1 keys=([0-9a-f,]*) keep=([0-9a-f,]*) -->")
NOTICE_MARK = asks.LEDGER_MARK + "keys={keys} -->"
NOTICE_KEYS = re.compile(re.escape(asks.LEDGER_MARK) + r"keys=([0-9a-f,]*) -->")
KEEP = re.compile(r"^\s*\**\s*keep:?\**\s+`?(?P<branch>[A-Za-z0-9._/-]+)`?\s*$", re.IGNORECASE | re.MULTILINE)
BRANCH_JOB = re.compile(r"(?<![a-z0-9])j-?(\d{3,})([a-z])?(?![a-z0-9])", re.IGNORECASE)
ASK_MARK = re.compile(r"<!-- switchboard:ask-(?P<agent>[a-z0-9-]+) pr=(?P<pr>\d+) head=(?P<head>[0-9a-f]{40}) -->")
SYSTEM_PREFIXES = ("archive/",)
TOUCH_EVENTS = {"labeled", "unlabeled", "closed", "reopened", "renamed", "assigned", "unassigned",
                "milestoned", "demilestoned", "ready_for_review", "convert_to_draft", "head_ref_force_pushed"}
ORDER = ("ESCALATED", "UNMATCHED")


def parse(stamp: str | None) -> dt.datetime | None:
    if not stamp:
        return None
    try:
        return dt.datetime.fromisoformat(stamp.replace("Z", "+00:00"))
    except ValueError:
        return None


def iso(t: dt.datetime) -> str:
    return t.astimezone(dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def digest(*parts: str) -> str:
    return hashlib.sha256("|".join(parts).encode("utf-8")).hexdigest()[:16]


def clean(text: str, limit: int = 120) -> str:
    """Outside text on one line, with nothing that could mention anyone, close a marker or start markup."""
    text = " ".join(str(text).split())[:limit]
    return text.replace("@", "").replace("<", "(").replace(">", ")").replace("`", "'")


def ob_id(job: str, kind: str, to: str, version: str) -> str:
    """The reconciler's id for one obligation (reconciler.py): O-<8 hex> of its job ("" on a thread that names
    none), kind, holder (the agent key) and version (an ask: its comment id). It is here, not there, so the ledger
    knows an ask by the id the reconciler's notices and the team's `/ack` lines carry, with no read of the state."""
    return "O-" + hashlib.sha256(f"{job}|{kind}|{to}|{version}".encode("utf-8")).hexdigest()[:8]


def job_of_branch(name: str) -> str:
    """J-NNN or J-NNNx from a branch name: `fb-bot/integrate-protected-j044b-grant` is J-044b."""
    m = BRANCH_JOB.search(name.split("/", 1)[-1])
    return f"J-{m.group(1)}{(m.group(2) or '').lower()}" if m else ""


def age_text(span: dt.timedelta) -> str:
    h = max(span.total_seconds(), 0) / 3600
    return f"{h:.1f}h" if h < 48 else f"{h / 24:.1f}d"


# ---------- who is who (work/agents.yml) ----------
@dataclasses.dataclass(frozen=True)
class Person:
    name: str    # as shown and addressed: Muse, Claude, Builder, ...
    agent: str   # the agents.yml key (muse, claude, grok, ...), "" for the switchboard itself


SWITCHBOARD = Person("Switchboard", "")


class Roster:
    def __init__(self, cfg: dict):
        self.agents = cfg.get("agents") or {}
        self.prefixes: list[tuple[str, str]] = []
        self.labels: dict[str, str] = {}
        self.names: dict[str, str] = {}
        for key, a in self.agents.items():
            for p in a.get("branch_prefixes", []):
                self.prefixes.append((str(p), key))
            for o in a.get("owner_labels", []):
                self.labels[str(o)] = key
            for n in a.get("names", []):
                self.names[str(n).lower()] = key
        self.prefixes.sort(key=lambda pk: -len(pk[0]))

    def person(self, key: str, hint: str = "") -> Person:
        """The agent `key`, named by `hint` when that is one of its names (owner:builder is Builder, a Grok name)."""
        names = [str(n) for n in (self.agents.get(key) or {}).get("names", [])] or [key.capitalize()]
        for n in names:
            if hint and n.lower() == hint.lower():
                return Person(n, key)
        return Person(names[0], key)

    def of_branch(self, ref: str) -> Person | None:
        for prefix, key in self.prefixes:
            if ref.startswith(prefix):
                return self.person(key, prefix.rstrip("/"))
        return None

    def of_labels(self, labels: Iterable[str]) -> Person | None:
        for label in labels:
            if label in self.labels:
                return self.person(self.labels[label], label.split(":", 1)[1].split("-")[0])
        return None

    def of_name(self, name: str) -> Person | None:
        key = self.names.get(name.lower())
        return self.person(key, name) if key else None

    def wake(self, agent: str) -> str:
        return str((self.agents.get(agent) or {}).get("wake", "issue"))

    def address(self, agent: str) -> str:
        """The name a `To <Name>:` line uses: the agent's first name (Grok's runs read only `To Grok:`)."""
        return self.person(agent).name


# ---------- inputs: what the reader got from GitHub ----------
@dataclasses.dataclass
class PrIn:
    number: int
    url: str
    head_ref: str
    head_sha: str
    job_line: str            # the `Job:` line of the body, "" when none
    action: str              # rules.MergeDecision.action
    reasons: tuple[str, ...]
    proof: str
    proof_done_at: str
    head_committed_at: str
    needs_review: bool
    reviewer: str            # the agent key asked to review this head, "" when the switch asked no one
    touched: dt.datetime
    keeps: tuple[str, ...] = ()   # `keep <branch>` lines in the PR's comments


@dataclasses.dataclass(frozen=True)
class ClosedPr:
    number: int
    head_sha: str            # the PR's head when it was closed
    merged: bool


@dataclasses.dataclass
class BranchIn:
    name: str
    sha: str
    pushed_at: dt.datetime
    job_line: str = ""       # the head commit's `Job:` line, read only when the name has no job id
    # read only for a branch with no open PR (see branch_fate):
    closed: tuple[ClosedPr, ...] = ()   # closed PRs from this branch, the newest closed first
    vs_trunk: str = ""       # compare trunk...head status: ahead, behind, identical, diverged; "" when not read
    ahead_by: int = 0        # commits on the head not in trunk
    ahead: tuple[str, ...] = ()   # their shas, oldest first (the compare's list)


@dataclasses.dataclass
class IssueIn:
    number: int
    url: str
    title: str
    labels: tuple[str, ...]
    touched: dt.datetime


@dataclasses.dataclass
class AskIn:
    cid: str
    thread: int
    url: str
    target: str              # as asks.py names it: Muse, ChatGPT, Grok, Builder, PE, Claude
    author: str
    created_at: dt.datetime


@dataclasses.dataclass
class RunIn:
    id: int
    url: str
    branch: str
    sha: str
    created_at: dt.datetime
    done_at: dt.datetime | None
    conclusion: str          # GitHub's: success, failure, cancelled, ... ("" while running)
    state: str = ""          # judged by its jobs (switch.run_state): green, red: <jobs>, running; "" when not judged


@dataclasses.dataclass
class Feed:
    prs: list[PrIn]
    open_prs: list[int]              # every open PR GitHub lists, read or not
    branches: list[BranchIn]         # live (pushed in the window), still there
    issues: list[IssueIn]
    asks: list[AskIn]
    runs: list[RunIn]                # every proof run in the window
    trunk: str = rules.TRUNK
    system_branches: frozenset[str] = frozenset()
    system_prs: frozenset[int] = frozenset()
    keeps: frozenset[str] = frozenset()   # branches someone said `keep` for
    open_heads: dict[str, int] = dataclasses.field(default_factory=dict)   # open PRs' head branches, read or not
    problems: tuple[str, ...] = ()   # listings cut short or not read: each is an UNMATCHED item
    board: int = 340
    repo: str = "IssisX/ScraperX"
    # (older head, newer head) of two live branches in one job's lane -> why the newer one holds the older one's
    # work ("" when it does not; absent when not read). Only a held one is SUPERSEDED (J-072: #315 was marked
    # superseded by #336 for sharing J-007 alone, while it was different, live work).
    overlap: dict[tuple[str, str], str] = dataclasses.field(default_factory=dict)
    # PR number -> (when it merged or closed, its job id or ""), from the issue events read: a status ask to the
    # integrator that names it or its job is closed by it (tools/board/asks.py read, `closed`)
    closed: dict[int, tuple[dt.datetime, str]] = dataclasses.field(default_factory=dict)


# ---------- items ----------
@dataclasses.dataclass
class Item:
    kind: str                # PR, BRANCH, JOB, ASK, TRUNK, SUPERSEDED, READ
    ref: str                 # "#343", a branch name, "ask 12345"
    link: str
    job: str
    owner: Person | None
    holder: Person
    state: str
    clock: dt.datetime       # when the deadline clock started
    deadline: dt.timedelta
    version: str             # a new head, ask or touch is a new version: its notices are sent again
    thread: int = 0          # where a notice goes; 0 is the board
    touched: dt.datetime | None = None
    unmatched: str = ""      # why the switchboard could not place it; "" when it could

    def age(self, now: dt.datetime) -> dt.timedelta:
        return now - self.clock

    def stage(self, now: dt.datetime) -> int:
        """0 within the deadline, 1 past it, 2 past twice it (ESCALATED)."""
        a = self.age(now)
        return 0 if a < self.deadline else 1 if a < 2 * self.deadline else 2

    def stale(self, now: dt.datetime) -> bool:
        return self.touched is not None and now - self.touched >= STALE

    def key(self, stage: int) -> str:
        return digest("ledger", self.kind, self.ref, self.version, str(stage))

    @property
    def ident(self) -> str:
        return f"{self.kind}|{self.ref}"


def ref_md(it: Item) -> str:
    """The item as a link: `#N` alone for an issue or PR (GitHub links it), so the section stays short."""
    if it.kind in ("BRANCH", "SUPERSEDED"):
        return f"[`{clean(it.ref, 80)}`]({it.link})"
    if it.kind in ("PR", "JOB") and re.fullmatch(r"#\d+", it.ref):
        return it.ref
    return f"[{clean(it.ref, 40)}]({it.link})" if it.link else clean(it.ref, 40)


def ref_plain(it: Item) -> str:
    """The item with no link and no markup, for mail: Muse's injection check refuses a mail that carries links,
    code marks or hidden text (work/tasks/mail-to-muse.md), and a refused ledger mail would be the silent drop
    this ledger exists to end (J-072: the 11-09 17:04Z ledger mail was refused)."""
    return clean(it.ref, 80)


@dataclasses.dataclass
class Ledger:
    items: list[Item]
    accounted: dict[str, str]        # every input -> the item that holds it, or why it is resolved
    counts: dict[str, int]           # resolved and system inputs, by reason
    pending: list[dict] = dataclasses.field(default_factory=list)   # run(): the notices a clock will make due

    def dropped(self, feed: Feed) -> list[str]:
        """Inputs no item and no reason accounts for: always empty, or the classifier lost something."""
        return [i for i in inputs_of(feed) if i not in self.accounted]


def inputs_of(feed: Feed) -> list[str]:
    out = [f"pr:{p.number}" for p in feed.prs]
    out += [f"open-pr:{n}" for n in feed.open_prs]
    out += [f"branch:{b.name}" for b in feed.branches]
    out += [f"issue:{i.number}" for i in feed.issues]
    out += [f"ask:{a.cid}:{a.target}" for a in feed.asks]
    out += [f"run:{r.id}" for r in feed.runs if r.conclusion in ("failure", "timed_out")]
    out += [f"problem:{n}" for n in range(len(feed.problems))]
    return out


def branch_fate(b: BranchIn) -> tuple[str, str, str]:
    """A branch with no open PR: (why it is resolved, its count reason, "") or, when it is live, ("", "", what its
    state says about its newest closed PR). Its head at a closed PR's head is that PR's (merged or not); its head in
    trunk has landed; only commits after the newest closed PR's head make it live again."""
    for c in b.closed:
        if c.head_sha == b.sha:
            how = "merged" if c.merged else "closed"
            return f"its PR #{c.number} was {how} at this head", f"branch at its {how} PR's head", ""
    if b.vs_trunk in ("behind", "identical"):
        return "landed", "branch landed", ""
    if not b.closed:
        return "", "", ""
    c = b.closed[0]
    if not b.vs_trunk:
        return "", "", f"commits after closed PR #{c.number} (not counted: the trunk compare was not read)"
    n = len(b.ahead) - b.ahead.index(c.head_sha) - 1 if c.head_sha in b.ahead else b.ahead_by
    return "", "", f"{n} commit{'' if n == 1 else 's'} after closed PR #{c.number}"


def classify(feed: Feed, now: dt.datetime, roster: Roster) -> Ledger:
    """Every input in the feed becomes an item or is accounted resolved/system, with a reason. Pure."""
    claude = roster.person("claude")
    items: list[Item] = []
    acc: dict[str, str] = {}
    counts: dict[str, int] = {}
    gh = f"https://github.com/{feed.repo}"
    issue_of = {}
    for i in feed.issues:
        job = rules.job_of_title(i.title)
        if job and job not in issue_of:
            issue_of[job] = i.number

    def add(it: Item, *inputs: str) -> Item:
        items.append(it)
        for i in inputs:
            acc[i] = it.ident
        return it

    def resolved(inp: str, why: str, count_as: str = "") -> None:
        acc[inp] = why
        counts[count_as or why] = counts.get(count_as or why, 0) + 1

    # newest run per branch, and per (branch, head)
    newest: dict[str, RunIn] = {}
    for r in sorted(feed.runs, key=lambda r: r.created_at):
        newest[r.branch] = r
    live = {b.name: b for b in feed.branches}
    pr_refs = {p.head_ref: p for p in feed.prs}

    # ---- PRs ----
    read = {p.number for p in feed.prs}
    for n in feed.open_prs:
        if n in feed.system_prs:
            resolved(f"open-pr:{n}", "system PR")
        elif n not in read:
            add(Item("PR", f"#{n}", f"{gh}/pull/{n}", "", None, claude, "the sweep could not read this PR", now, ACT,
                     "unread", n, None, "not read this sweep (see the switchboard log)"), f"open-pr:{n}")
    for p in feed.prs:
        if p.number in feed.system_prs:   # counted once, by the open-PR listing
            acc[f"pr:{p.number}"] = acc[f"open-pr:{p.number}"] = "system PR"
            continue
        it = pr_item(p, now, roster, claude, gh, issue_of)
        add(it, f"pr:{p.number}", f"open-pr:{p.number}")

    # ---- branches ----
    groups: dict[tuple[str, bool], list[BranchIn]] = {}
    settled: set[str] = set()   # landed, or at a closed PR's head: neither listed nor in a superseded group
    for b in feed.branches:
        if b.name == feed.trunk or b.name in feed.system_branches or b.name.startswith(SYSTEM_PREFIXES):
            resolved(f"branch:{b.name}", "system branch")
            continue
        gone, count_as, after = ("", "", "") if b.name in pr_refs or b.name in feed.open_heads else branch_fate(b)
        if gone:
            resolved(f"branch:{b.name}", gone, count_as)
            settled.add(b.name)
            continue
        job = job_of_branch(b.name) or rules.job_of_pr(b.job_line)
        if job:
            groups.setdefault((job, "/integrate-protected-" in b.name), []).append(b)
        if b.name in pr_refs:
            acc[f"branch:{b.name}"] = f"PR|#{pr_refs[b.name].number}"
            continue
        if b.name in feed.open_heads:   # its PR was not read this sweep: that PR's own item says so
            acc[f"branch:{b.name}"] = f"PR|#{feed.open_heads[b.name]}"
            continue
        owner = roster.of_branch(b.name)
        run = newest.get(b.name)
        at_head = run if run and run.sha == b.sha else None
        holder, state, clock, deadline, version = owner or claude, "", b.pushed_at, STALE, b.sha + iso(b.pushed_at)
        if at_head is None:
            state = "no proof run on its head" + ("" if "/integrate-" in b.name else " (not an integrate branch)")
        elif at_head.state.startswith("red") or (not at_head.state and at_head.conclusion in ("failure", "timed_out")):
            state = f"proof {at_head.state or 'red'}, no PR"
            clock, deadline, version = at_head.done_at or at_head.created_at, ACT, b.sha
        elif at_head.state == "running" or not at_head.conclusion:
            state = "proof running"
        elif at_head.conclusion == "success" or at_head.state == "green":
            auto = b.name.startswith("fb-bot/integrate-") and rules.job_of_pr(b.job_line)
            state = "green, no PR" + (": the switchboard opens it" if auto else ": open one")
            holder = SWITCHBOARD if auto else (owner or claude)
        else:
            state = f"proof {at_head.conclusion}"
        if after:
            state = f"{after}; {state}"
        unmatched = ("unknown branch prefix" if owner is None else
                     "" if job else "no job id (branch name or head commit `Job:` line)")
        add(Item("BRANCH", b.name, f"{gh}/tree/{urllib.parse.quote(b.name, safe='/')}", job, owner, holder, state,
                 clock, deadline, version, issue_of.get(job, 0), b.pushed_at, unmatched), f"branch:{b.name}")

    # ---- superseded: two or more live branches for one job and lane; the newest push is current ----
    for (job, _), group in sorted(groups.items()):
        if len(group) < 2:
            continue
        group.sort(key=lambda b: (b.pushed_at, b.name), reverse=True)
        current = group[0]
        for old in group[1:]:
            held = feed.overlap.get((old.sha, current.sha))
            if old.name in feed.keeps or not held:   # a shared job number alone is not superseded work (J-072)
                continue
            owner = roster.of_branch(old.name)
            pr = pr_refs.get(old.name)
            where = f" (#{pr.number})" if pr else ""
            items[:] = [x for x in items if not (x.kind == "BRANCH" and x.ref == old.name)]
            add(Item("SUPERSEDED", old.name, f"{gh}/tree/{urllib.parse.quote(old.name, safe='/')}", job, owner,
                     owner or claude, f"SUPERSEDED by {clean(current.name, 80)}{where} ({clean(held, 60)}): close it "
                     "or say keep",
                     current.pushed_at, STALE, old.sha + "|" + current.name,
                     pr.number if pr else issue_of.get(job, 0), old.pushed_at), f"branch:{old.name}")

    # ---- red proof runs: each is current on an item, or resolved ----
    trunk_run = newest.get(feed.trunk)
    for r in feed.runs:
        if r.conclusion not in ("failure", "timed_out"):
            continue
        inp = f"run:{r.id}"
        if r.branch == feed.trunk:
            if trunk_run is r and (r.state.startswith("red") or not r.state):
                add(Item("TRUNK", feed.trunk, r.url, "", claude, claude, f"trunk proof {r.state or 'red'}",
                         r.done_at or r.created_at, ACT, r.sha, 0, None), inp)
            else:
                resolved(inp, "red run resolved (newer trunk run, or its jobs pass)")
        elif r.branch in settled:
            resolved(inp, "red run on a landed or closed branch")
        elif r.branch in pr_refs and pr_refs[r.branch].head_sha == r.sha and newest.get(r.branch) is r:
            acc[inp] = f"PR|#{pr_refs[r.branch].number}"
        elif r.branch in live and live[r.branch].sha == r.sha and newest.get(r.branch) is r:
            acc[inp] = acc.get(f"branch:{r.branch}", "branch")
        elif r.branch in live or r.branch in pr_refs:
            resolved(inp, "red run resolved (a newer run or head)")
        else:
            resolved(inp, "red run on a branch gone or not live")

    # ---- job issues ----
    for i in feed.issues:
        job = rules.job_of_title(i.title)
        owner = roster.of_labels(i.labels)
        state = next((l.split(":", 1)[1] for l in i.labels if l.startswith("state:")), "open")
        prs = [p for p in feed.prs if job and rules.job_of_pr(p.job_line) == job]
        branches = [b for b in feed.branches if job and (job_of_branch(b.name) or rules.job_of_pr(b.job_line)) == job]
        touched = max([i.touched] + [p.touched for p in prs] + [b.pushed_at for b in branches])
        branches = [b for b in branches if b.name not in settled]
        bits = [state] + ([f"{len(prs)} PR"] if prs else []) + ([f"{len(branches)} live branch(es)"] if branches else [])
        unmatched = ("job issue title has no J-NNN" if not job else
                     "no owner: label" if owner is None else "")
        add(Item("JOB", f"#{i.number}", i.url, job, owner, owner or claude, ", ".join(bits), touched, STALE,
                 iso(touched), i.number, touched, unmatched), f"issue:{i.number}")

    # ---- asks ----
    for a in feed.asks:
        target = roster.of_name(a.target) or Person(a.target, "")
        add(Item("ASK", f"ask on #{a.thread}", a.url, "", roster.of_name(a.author), target,
                 f"to {target.name} from {clean(a.author, 20)}, no answer", a.created_at, ACT, a.cid, a.thread,
                 None), f"ask:{a.cid}:{a.target}")

    # ---- what could not be read ----
    for n, why in enumerate(feed.problems):
        add(Item("READ", f"read {n + 1}", "", "", None, claude, clean(why, 200), now, ACT, digest(why), 0, None,
                 "a listing was cut short or failed: items may be missing"), f"problem:{n}")

    for it in items:   # the switchboard is never told: what it should have done and did not goes to the integrator
        if it.holder == SWITCHBOARD and it.stage(now):
            it.holder, it.state = claude, f"{it.state} (the switchboard has not done it)"
    led = Ledger(items, acc, counts)
    for missing in led.dropped(feed):   # fail loud: the classifier never loses an input silently
        add(Item("READ", missing, "", "", None, claude, "the ledger dropped this input", now, ACT, missing, 0, None,
                 "dropped by the classifier (a bug in tools/switchboard/ledger.py)"), missing)
    return led


def pr_item(p: PrIn, now: dt.datetime, roster: Roster, claude: Person, gh: str, issue_of: dict) -> Item:
    owner = roster.of_branch(p.head_ref)
    job = rules.job_of_pr(p.job_line)
    holder, clock, deadline, version = owner or claude, p.touched, STALE, p.head_sha + iso(p.touched)
    why = "; ".join(p.reasons)
    act = p.action
    if act == "FIX-CI":
        state = f"proof {p.proof}"
        clock, deadline, version = parse(p.proof_done_at) or parse(p.head_committed_at) or p.touched, ACT, p.head_sha
    elif act == "FIX":
        state = clean(why, 120)
    elif act == "MERGE":
        state, holder = "ready: the switchboard merges it", SWITCHBOARD
    elif act == "HUMAN-MERGE":
        state, holder = f"merge by hand: {clean(why, 100)}", claude
    elif act == "SKIP":
        state = clean(why, 80)
    elif p.needs_review:
        if not p.reviewer or p.reviewer == "codex":
            holder, by = roster.of_name("Codex") or Person("Codex", "codex"), "Codex"
        elif p.reviewer in roster.agents:
            holder = by = roster.person(p.reviewer)
            by = holder.name
        else:   # the switch's own automatic review (claude-api): when it is late, the integrator looks
            holder, by = claude, f"the {clean(p.reviewer, 20)} review"
        state = f"green, waiting on {by}"
        clock, deadline, version = parse(p.proof_done_at) or p.touched, ACT, p.head_sha
    elif p.proof == "running":
        state = "proof running"
    elif p.proof == "none":
        state = "no proof run on its head"
    else:
        state = clean(why, 120) or "waiting"
        if "eyes" in why:
            holder = claude
        elif "rule_check did not run" in why or "computing mergeability" in why:
            holder = SWITCHBOARD
    unmatched = ("unknown branch prefix" if owner is None else "" if job else "no `Job:` line in the PR body")
    return Item("PR", f"#{p.number}", p.url, job, owner, holder, state, clock, deadline, version, p.number,
                p.touched, unmatched)


# ---------- the board section ----------
def line(it: Item, now: dt.datetime, holder: bool) -> str:
    who = f"{it.holder.name}: " if holder else ""
    flags = []
    st = it.stage(now)
    if st == 1:   # past twice it is said by the ESCALATED heading
        flags.append("stale" if it.deadline == STALE else f"past {age_text(it.deadline)}")
    elif st == 0 and it.stale(now):
        flags.append("stale")
    if it.unmatched:
        flags.insert(0, f"UNMATCHED: {it.unmatched}")
    job = f" {it.job}" if it.job and it.job not in it.ref else ""
    tail = f" · {' · '.join(flags)}" if flags else ""
    return f"- {who}{ref_md(it)}{job} · {clean(it.state, 90)} · {age_text(it.age(now))}{tail}"


def section(led: Ledger, now: dt.datetime, when: str, sent: list[str], kept: list[str], notify: bool = True) -> str:
    items = led.items
    esc = [it for it in items if it.stage(now) == 2]
    unm = [it for it in items if it.unmatched and it.stage(now) < 2]
    rest = [it for it in items if it not in esc and it not in unm]
    by_kind: dict[str, int] = {}
    for it in items:
        by_kind[it.kind] = by_kind.get(it.kind, 0) + 1
    kinds = ", ".join(f"{n} {k.lower()}" for k, n in sorted(by_kind.items()))
    past = sum(1 for it in items if it.stage(now) == 1)
    stale = sum(1 for it in items if it.stale(now))
    unmatched = sum(1 for it in items if it.unmatched)
    folded = ", ".join(f"{n} {why}" for why, n in sorted(led.counts.items()))
    head = [
        "## Ledger",
        f"Written by the switchboard at {when}, every sweep (`tools/switchboard/ledger.py`); text between the ledger "
        f"markers is replaced each sweep. **{len(items)} items** ({kinds}): **{len(esc)} escalated**, {past} past "
        f"deadline, {unmatched} unmatched, {stale} stale." + (f" Not listed: {folded}." if folded else ""),
        "Deadlines: red, an unanswered ask, a green PR with no review 2 h; anything else 4 h untouched. " + (
            "Past it the holder is told once; past twice it, again, and it is listed here as ESCALATED."
            if notify else
            "Past twice it an item is listed here as ESCALATED. The ledger sends no notices: the reconciler "
            "(`tools/switchboard/reconciler.py`) tells each holder.") +
        " A superseded branch: close it, or comment `keep <branch>`.",
    ]

    def order(xs: list[Item]) -> list[Item]:
        return sorted(xs, key=lambda it: (-it.stage(now), -it.age(now).total_seconds(), it.ref))

    top = [f"### ESCALATED ({len(esc)})"] + ([line(it, now, True) for it in order(esc)] or ["- none"])
    top += [f"### UNMATCHED ({len(unm)})"] + ([line(it, now, True) for it in order(unm)] or ["- none"])
    holders: dict[str, list[Item]] = {}
    for it in rest:
        holders.setdefault(it.holder.name, []).append(it)
    full: list[str] = []
    short: list[str] = []
    for name in sorted(holders, key=lambda n: (-len(holders[n]), n)):
        xs = order(holders[name])
        full += [f"### {name} ({len(xs)})"] + [line(it, now, False) for it in xs]
        p = sum(1 for it in xs if it.stage(now) == 1)
        s = sum(1 for it in xs if it.stale(now))
        short.append(f"- **{name}**: {len(xs)} ({p} past deadline, {s} stale; oldest {age_text(xs[0].age(now))})")
    marker = f"<!-- ledger:sent v1 keys={','.join(sent[-MAX_KEYS:])} keep={','.join(kept[-MAX_KEYS:])} -->"
    text = "\n".join(head + top + full)
    if len(text) > MAX_SECTION:
        text = "\n".join(head + top + [f"### Everything else, by holder ({len(rest)}; collapsed to keep this "
                                       f"under {MAX_SECTION} characters)"] + short)
    return f"{text}\n{marker}"


def rewrite(body: str, text: str) -> str | None:
    """The body with only the text between the ledger markers replaced (the markers inserted under the first
    heading when the body has none). None when the markers are broken (one without the other, repeated, or out
    of order): then nothing is written, and the caller says so."""
    starts, ends = body.count(START), body.count(END)
    if starts == 1 and ends == 1:
        i, j = body.index(START), body.index(END)
        if i > j:
            return None
        return body[:i + len(START)] + "\n" + text + "\n" + body[j:]
    if starts or ends:
        return None
    block = f"{START}\n{text}\n{END}"
    m = re.search(r"^#{1,6} [^\r\n]*", body, re.MULTILINE)
    if not m:
        return f"{block}\n\n{body}"
    nl = "\r\n" if "\r\n" in body else "\n"
    return body[:m.end()] + nl + nl + block + body[m.end():]   # outside text unchanged: the block goes in between


def section_state(body: str) -> tuple[list[str], list[str]]:
    """The keys the last ledger section recorded: (notices sent, branches kept)."""
    i, j = body.find(START), body.find(END)
    m = SENT.search(body[i:j] if 0 <= i < j else "")
    if not m:
        return [], []
    return [k for k in m.group(1).split(",") if k], [k for k in m.group(2).split(",") if k]


# ---------- notices ----------
@dataclasses.dataclass
class Notice:
    thread: int
    agent: str
    items: list[tuple[Item, int]]
    name: str = ""   # the name its `To <Name>:` line uses: the holder's own (Builder, not Grok); "" is the agent's first

    @property
    def keys(self) -> list[str]:
        return [it.key(st) for it, st in self.items]


def send(gh, notices: list[Notice], roster: Roster, now: dt.datetime, board: int, out=print,
         muse_label: str = "wake:muse") -> list[str]:
    """Post the notices (at most MAX_NOTICES); returns the keys posted. A failed one is retried next sweep."""
    posted: list[str] = []
    for n in notices[:MAX_NOTICES]:
        wake, text = notice_body(n, roster, now, board)
        marked = f"{text}\n\n{NOTICE_MARK.format(keys=','.join(n.keys))}"
        out(marked)
        try:
            if wake == "email":   # as switch.py's muse(): the marker goes in after the label lands
                resp = gh.comment(n.thread, text, f"ledger: {len(n.items)} item(s) to Muse on #{n.thread}")
                gh.write("POST", f"repos/{gh.repo}/issues/{n.thread}/labels", {"labels": [muse_label]},
                         f"label #{n.thread} {muse_label} (the bridge emails Muse, then removes it)")
                cid = (resp if isinstance(resp, dict) else {}).get("id")
                if cid is not None:
                    gh.write("PATCH", f"repos/{gh.repo}/issues/comments/{cid}", {"body": marked},
                             f"ledger: mark #{n.thread} delivered")
            else:
                gh.comment(n.thread, marked, f"ledger: {len(n.items)} item(s) to {n.agent} on #{n.thread}")
        except Exception as err:   # noqa: BLE001  one failed notice never stops the others; it is retried next sweep
            out(f"ERROR       ledger notice on #{n.thread}: {type(err).__name__}: {err}")
            continue
        posted += n.keys
    return posted


# ---------- the clocks between full passes (J-072) ----------
# A fast sweep reads no ledger input, but a deadline ripens on the clock, not on a read. The full pass keeps each
# notice a clock will make due (the item, its stage, when); a fast sweep sends the ripe ones from that, with no
# read, unless something about the item changed since the full pass: then the ledger runs in full.
def pending(items: list[Item], now: dt.datetime, sent: set[str]) -> list[dict]:
    """Per item, the next notice owed: one due now and not sent (past MAX_NOTICES or a failed post: the next sweep
    sends it), else the next stage's, at its time."""
    out = []
    for it in items:
        if not it.holder.agent:
            continue
        now_stage = it.stage(now)
        if now_stage and it.key(now_stage) not in sent:
            at = now
        elif now_stage < 2 and it.key(now_stage + 1) not in sent:
            at = it.clock + (now_stage + 1) * it.deadline
        else:
            continue
        out.append({"at": at.strftime("%Y-%m-%dT%H:%M:%SZ"), "item": item_json(it)})
    return out


def advance(timers: list[dict], posted: Iterable[str], now: dt.datetime) -> list[dict]:
    """The timers after a fire: one whose notice went out waits for its item's next stage, if any is left."""
    done = set(posted)
    keep = []
    for t in timers:
        it = item_of(t["item"])
        if it.key(1) not in done and it.key(2) not in done:
            keep.append(t)
        elif it.key(2) not in done:
            keep.append(dict(t, at=(it.clock + 2 * it.deadline).strftime("%Y-%m-%dT%H:%M:%SZ")))
    return keep


def item_json(it: Item) -> dict:
    d = dataclasses.asdict(it)
    for k in ("clock", "touched"):
        d[k] = d[k].strftime("%Y-%m-%dT%H:%M:%SZ") if d[k] else None
    d["deadline"] = it.deadline.total_seconds()
    return d


def item_of(d: dict) -> Item:
    person = lambda p: Person(p["name"], p["agent"]) if p else None   # noqa: E731
    return Item(d["kind"], d["ref"], d["link"], d["job"], person(d["owner"]), person(d["holder"]), d["state"],
                parse(d["clock"]), dt.timedelta(seconds=float(d["deadline"])), d["version"], int(d["thread"]),
                parse(d["touched"]) if d.get("touched") else None, d.get("unmatched") or "")


def ripe(timers: list[dict], now: dt.datetime) -> list[dict]:
    return [t for t in timers if (parse(t.get("at")) or now) <= now]


def touched_by(t: dict, dirty: dict) -> bool:
    """Did anything about this notice's item change since the full pass (a PR read, a comment on its thread, a
    proof on its branch)? Then its cached state may be wrong, and the ledger runs in full instead."""
    d = t["item"]
    refs = {int(r) for r in re.findall(r"#(\d+)", d.get("ref") or "")} | {int(d.get("thread") or 0)}
    return bool(refs & set(dirty.get("threads") or [])) or (d.get("ref") or "") in set(dirty.get("branches") or [])


def fire(gh, timers: list[dict], roster: Roster, now: dt.datetime, board: int, out=print,
         muse_label: str = "wake:muse") -> list[str]:
    """Send the ripe cached notices, with no read. Returns the keys posted."""
    items = [item_of(t["item"]) for t in ripe(timers, now)]
    return send(gh, due_notices(items, now, set(), roster, board), roster, now, board, out, muse_label)


def fires_on_board(timers: list[dict], roster: Roster) -> bool:
    """Does one of these cached notices go on the board itself (due_notices: mail, or an item with no thread of its
    own)? A fast sweep reads the board's number from GitHub only then; a notice on a job thread costs no read."""
    for t in timers:
        d = t["item"]
        if not int(d.get("thread") or 0) or roster.wake((d.get("holder") or {}).get("agent") or "") == "email":
            return True
    return False


def due_notices(items: list[Item], now: dt.datetime, sent: set[str], roster: Roster, board: int) -> list[Notice]:
    """The notices owed this sweep, the most urgent first: each item past its deadline whose key for its stage was
    never sent, one comment per (thread, holder). Muse reads mail, not threads: hers go in one mail per sweep, on
    the board issue (at most MAX_MAIL_ITEMS; the rest next sweep). The switchboard itself is never
    notified; an item past twice its deadline that was never told at all gets the one (stage 2) notice.
    A holder is addressed by its own name from work/agents.yml (J-072: Builder's notice went out as `To Grok: for
    Builder`, and Grok's runs read only `To Grok:`). A Codex-held item goes to the integrator (`wake: mention`):
    the switchboard's own `@codex review` never woke Codex (2 of 2, #191 and #150), so Claude posts it."""
    groups: dict[tuple[int, str, str], Notice] = {}
    due = [(it, it.stage(now)) for it in items]
    due = sorted(((it, st) for it, st in due if st and it.holder.agent and it.key(st) not in sent),
                 key=lambda x: (-x[1], x[0].clock))
    mail: Notice | None = None
    for it, st in due:
        wake = roster.wake(it.holder.agent)
        if wake == "email":
            if mail is None:   # on the board, never the item's thread: that may be a PR, which the bridge
                # cannot mail from (its token is Issues-only; #337's notice of 11-10 2:59 AM sat unmailed)
                mail = groups.setdefault((board, it.holder.agent, ""), Notice(board, it.holder.agent, []))
            if len(mail.items) < MAX_MAIL_ITEMS:
                mail.items.append((it, st))
            continue
        where = it.thread or board
        agent, name = ("claude", roster.address("claude")) if wake == "mention" else (it.holder.agent, it.holder.name)
        groups.setdefault((where, agent, name), Notice(where, agent, [], name)).items.append((it, st))
    return sorted(groups.values(), key=lambda n: (-max(st for _, st in n.items),
                                                 min(it.clock for it, _ in n.items), n.thread, n.agent, n.name))


def notice_body(n: Notice, roster: Roster, now: dt.datetime, board: int) -> tuple[str, str]:
    """(the first line's channel, the comment without its marker)."""
    wake = roster.wake(n.agent)
    ref = ref_plain if wake == "email" else ref_md
    address = n.name or roster.address(n.agent)
    lines = []
    for it, st in n.items:
        what = ("twice past" if wake == "email" else "ESCALATED, past twice") if st == 2 else "past"
        codex = roster.wake(it.holder.agent) == "mention" and wake != "mention"
        step = (f". Codex holds it: post `codex review` with an at-sign in front on #{it.thread or board} as IssisX "
                f"(Codex does not answer the switchboard's own mention).") if codex else ""
        lines.append(f"- {ref(it)}{(' ' + it.job) if it.job and it.job not in it.ref else ''}: {clean(it.state, 160)}; "
                     f"{age_text(it.age(now))} old, {what} its {age_text(it.deadline)} deadline{step}")
    if wake == "email":   # plain wording for Muse's check: no order, no pressure (work/tasks/mail-to-muse.md 6-7)
        tail = (f"From the switchboard's ledger (the section in #{board}'s body). The next step for each is a PR, a "
                f"fix, or one line on its issue saying why it waits. Each is sent once per stage.")
    else:
        tail = (f"From the switchboard's ledger (the section in #{board}'s body). You hold these: act, or say on the "
                f"thread why they wait. Each is sent once per stage.")
    persona = sorted({it.holder.name for it, _ in n.items} - {address})
    who = f" for {', '.join(persona)}," if persona else ""
    if wake == "email":
        first = f"CLAUDE_TO_MUSE: ledger,{who} {len(n.items)} item(s) of yours past deadline"
    else:   # never `@codex review` from this login: it does not wake Codex, so a Codex item is Claude's (due_notices)
        first = f"To {address}:{who} {len(n.items)} item(s) past deadline"
    body = "\n".join([first, ""] + lines + ["", tail]).replace("@", "")
    return wake, body


# ---------- the run ----------
def run(gh, feed: Feed, roster: Roster, now: dt.datetime, when: str, out=print, muse_label: str = "wake:muse",
        sent_in_comments: Iterable[str] = (), board_body: Callable[[], str] | None = None, notify: bool = True) -> Ledger:
    """Classify, send the notices owed, rewrite the board's ledger section. Writes go through gh.write and
    gh.comment, so a dry run only prints them. With notify False (the reconciler owns the notices: `notices: v2`
    in work/switchboard.yml) nothing is posted and no notice clock is kept: only the section is written."""
    led = classify(feed, now, roster)
    body = board_body() if board_body else (gh.get(f"repos/{gh.repo}/issues/{feed.board}") or {}).get("body") or ""
    old_sent, old_kept = section_state(body)
    sent = set(old_sent) | set(sent_in_comments)
    posted: list[str] = []
    if notify:
        posted = send(gh, due_notices(led.items, now, sent, roster, feed.board), roster, now, feed.board, out, muse_label)
        led.pending = pending(led.items, now, sent | set(posted))
    live_keys = {it.key(s) for it in led.items for s in (1, 2)}
    keep_keys = sorted({digest("keep", b) for b in feed.keeps} |
                       ({k for k in old_kept} & {digest("keep", b.name) for b in feed.branches}))
    keys = [k for k in dict.fromkeys(old_sent + sorted(set(sent_in_comments)) + posted) if k in live_keys]
    text = section(led, now, when, keys, keep_keys, notify)
    new = rewrite(body, text)
    if new is None:
        out(f"ERROR       ledger: #{feed.board}'s body has broken ledger markers; not written (fix them by hand)")
        return led
    if len(new) > MAX_BODY:
        out(f"ERROR       ledger: #{feed.board}'s body would be {len(new)} chars; not written")
        return led
    out(text)
    if new != body:
        gh.write("PATCH", f"repos/{gh.repo}/issues/{feed.board}", {"body": new},
                 f"ledger: rewrite #{feed.board}'s ledger section ({len(led.items)} items)")
    return led


# ---------- reading GitHub ----------
def cursor_pages(repo: str, path: str, stop: Callable[[dict], bool] | None = None,
                 limit: int = MAX_PAGES) -> tuple[list, bool]:
    """A cursor-paged listing (the repo activity API), read with `gh api -i` for its Link header. The next link
    names the repo by id, which some proxies refuse, so only its `after` cursor is kept. (items, cut short)."""
    items: list = []
    url = path
    for _ in range(limit):
        got = subprocess.run(["gh", "api", "-i", url], capture_output=True, timeout=60)
        if got.returncode != 0:
            raise RuntimeError(f"gh api {url} failed: {got.stderr.decode('utf-8', 'replace')[:200]}")
        text = got.stdout.decode("utf-8", "replace").replace("\r\n", "\n")
        head, _, data = text.partition("\n\n")
        batch = json.loads(data) if data.strip() else []
        items += batch
        if stop and batch and stop(batch[-1]):   # newest first: the rest is older than the window
            return items, False
        m = re.search(r'<[^>]*[?&]after=([^&>]+)[^>]*>;\s*rel="next"', head)
        if not m:
            return items, False
        sep = "&" if "?" in path else "?"
        url = f"{path}{sep}after={m.group(1)}"
    return items, True


EVENT_KEYS = ("id", "event", "created_at", "actor", "issue")
ACTIVITY_KEYS = ("id", "ref", "timestamp", "activity_type", "after", "before")


def newest_first(sw, name: str, path: str, since: dt.datetime, when: Callable[[dict], dt.datetime | None],
                 keys: tuple[str, ...], cursor: bool = False) -> tuple[list, bool]:
    """A newest-first listing (issue events, repo activity) read only back to what the last full pass kept (J-072:
    its first page changes with every new event, so it was 4-5 counted reads each pass). The items kept, trimmed to
    `keys` and to the window, ride in the sweep state; a read stops at the first item already kept. (items, cut)."""
    state = sw.gh.state if isinstance(getattr(sw.gh, "state", None), dict) else {}
    kept = [k for k in (state.get("ledger_" + name) or []) if isinstance(k, dict)]
    seen = {k.get("id") for k in kept}
    newest = max((when(k) for k in kept if when(k)), default=None)

    def stop(item: dict) -> bool:
        t = when(item)
        return item.get("id") in seen or (t is not None and t < since) or (newest is not None and t is not None and t < newest)
    got, cut = (cursor_pages(sw.gh.repo, path, stop=stop) if cursor else page_list(sw.gh, path, stop=stop))
    fresh = [{k: it.get(k) for k in keys} for it in got if it.get("id") not in seen]
    if name == "events":   # an event's issue: only its number is read
        for it in fresh:
            it["issue"] = {"number": (it.get("issue") or {}).get("number")}
            it["actor"] = {"login": (it.get("actor") or {}).get("login")}
    items = [it for it in fresh + kept if (when(it) or since) >= since]
    items.sort(key=lambda it: when(it) or since, reverse=True)
    if isinstance(getattr(sw.gh, "state", None), dict):
        sw.gh.state["ledger_" + name] = items
    return items, cut and not kept


def page_list(gh, path: str, stop: Callable[[dict], bool] | None = None) -> tuple[list, bool]:
    """A page-numbered listing, past ghapi's 5 pages, stopping early once `stop` holds for an item."""
    sep = "&" if "?" in path else "?"
    items: list = []
    for page in range(1, MAX_PAGES + 1):
        batch = gh.get(f"{path}{sep}per_page=100&page={page}") or []
        items += batch
        if len(batch) < 100 or (stop and batch and stop(batch[-1])):
            return items, False
    return items, True


def comment_window(gh, repo: str, since: dt.datetime, now: dt.datetime) -> tuple[list, bool]:
    """Every comment updated since `since`, oldest first, and whether some were not read. The listing goes from a
    6-hour floor (J-072: the URL then repeats, and its pages answer 304) and is filtered to `since`. GitHub lists it
    oldest first and page_list stops at MAX_PAGES, so a full listing has lost its NEWEST comments, the ones a pass
    acts on (J-104: about 1,470 in 3 days, and every /ack, reply and ask past the 1,000th went unread for a day, so
    acked items were re-sent and dead-lettered). A full listing is read again newest first and merged by id, as the
    switch's own inbox does (switch.Switch.comments_since). Cut only when both reads were full and never met."""
    floor = since.astimezone(dt.timezone.utc)
    floor = floor.replace(hour=floor.hour - floor.hour % 6, minute=0, second=0, microsecond=0)
    got, cut = page_list(gh, f"repos/{repo}/issues/comments?since={iso(floor)}")
    if cut:
        newest, cut = page_list(gh, f"repos/{repo}/issues/comments?since={iso(since)}&sort=created&direction=desc",
                                lambda c: (parse(c.get("created_at")) or now) < since)
        ids = {c.get("id") for c in got}
        cut = cut and not any(c.get("id") in ids for c in newest)
        got = got + [c for c in reversed(newest) if c.get("id") not in ids]
    return [c for c in got if (parse(c.get("updated_at") or c.get("created_at")) or now) >= since], cut


def touch_times(comments: list[dict], events: list[dict], bot: str) -> dict[int, dt.datetime]:
    """Per thread, the newest touch by anyone but the switchboard's own login (its notices must not reset a clock)."""
    out: dict[int, dt.datetime] = {}
    for c in comments:
        if (c.get("user") or {}).get("login") == bot:
            continue
        try:
            n = int((c.get("issue_url") or "").rsplit("/", 1)[1])
        except (ValueError, IndexError):
            continue
        t = parse(c.get("created_at"))
        if t and (n not in out or t > out[n]):
            out[n] = t
    for e in events:
        if e.get("event") not in TOUCH_EVENTS or (e.get("actor") or {}).get("login") == bot:
            continue
        n = (e.get("issue") or {}).get("number")
        t = parse(e.get("created_at"))
        if n and t and (n not in out or t > out[n]):
            out[n] = t
    return out


def closed_prs(events: list[dict]) -> dict[int, tuple[dt.datetime, str]]:
    """PR number -> (when it first merged or closed, its job: the body's `Job:` line, else a job id in its title), from
    the issue events read (asks.py read's `closed`)."""
    out: dict[int, tuple[dt.datetime, str]] = {}
    for e in events:
        it = e.get("issue") or {}
        t = parse(e.get("created_at"))
        if e.get("event") not in ("closed", "merged") or "pull_request" not in it or not it.get("number") or not t:
            continue
        job = rules.job_of_pr(it.get("body")) or (asks.JOB_ID.findall(it.get("title") or "") or [""])[0]
        n = int(it["number"])
        if n not in out or t < out[n][0]:
            out[n] = (t, job)
    return out


def touched_at(created: str, updated: str, touches: dict[int, dt.datetime], n: int, window: dt.datetime,
               *more: dt.datetime | None) -> dt.datetime:
    """When a thread was last touched by someone other than the switchboard. GitHub's updated_at counts the
    switchboard's own comments, so inside the read window only the comments and events read count; a thread
    updated inside it with no such touch was last touched before the window: its start is used."""
    times = [t for t in (parse(created), touches.get(n), *more) if t]
    up = parse(updated)
    if up and up < window:
        times.append(up)
    best = max(times) if times else window
    if up and up >= window and best < window:
        return window
    return best


def read_feed(sw, steered: list, now: dt.datetime, board: int, proof_workflow: str, system_branches: Iterable[str],
              system_prs: Iterable[int], roster: Roster | None = None) -> tuple[Feed, list[str]]:
    """Everything the ledger needs, read from GitHub through the switch's own client. Returns the feed and the
    notice keys found in the switchboard's own recent comments. With the roster, the asks are judged as the
    reconciler judges them (ask_judge): by job thread, by agent, and by the reconciler's own id for each ask."""
    gh, repo = sw.gh, sw.gh.repo
    system_branches = frozenset(system_branches) | muse_git.BRANCHES
    problems: list[str] = []
    since_live, since_touch = now - LIVE, now - TOUCH_WINDOW

    comments, cut = comment_window(gh, repo, since_live, now)
    if cut:
        problems.append(f"comments since {iso(since_live)}: more than {2 * MAX_PAGES * 100}, the middle not read")
    events, cut = newest_first(sw, "events", f"repos/{repo}/issues/events", since_touch,
                               lambda e: parse(e.get("created_at")), EVENT_KEYS)
    if cut:
        problems.append(f"issue events: more than {MAX_PAGES * 100} since {iso(since_touch)}, the rest not read")
    touches = touch_times(comments, events, rules.BOT_LOGIN)

    sent: list[str] = []
    keeps: set[str] = set()
    asked: dict[tuple[int, str], tuple[str, str]] = {}   # (pr, head) -> (when, agent): the newest review ask
    for c in comments:
        body = c.get("body") or ""
        if (c.get("user") or {}).get("login") == rules.BOT_LOGIN:
            for m in NOTICE_KEYS.finditer(body):
                sent += [k for k in m.group(1).split(",") if k]
            for m in ASK_MARK.finditer(body):
                k = (int(m.group("pr")), m.group("head"))
                asked[k] = max(asked.get(k, ("", "")), (c.get("created_at") or "", m.group("agent")))
        elif asks.LEDGER_MARK not in body:
            keeps |= {m.group("branch") for m in KEEP.finditer(body)}
    for c in gh.paged(f"repos/{repo}/issues/{board}/comments"):
        if (c.get("user") or {}).get("login") != rules.BOT_LOGIN:
            keeps |= {m.group("branch") for m in KEEP.finditer(c.get("body") or "")}

    # PRs: the sweep's own reading
    open_list = sw.listing(f"repos/{repo}/pulls?state=open") if hasattr(sw, "listing") else \
        gh.paged(f"repos/{repo}/pulls?state=open")
    open_prs = [p["number"] for p in open_list]
    open_heads = {(p.get("head") or {}).get("ref") or "": p["number"] for p in open_list
                  if (((p.get("head") or {}).get("repo") or {}).get("full_name") or "") == repo}
    prs: list[PrIn] = []
    for s in steered:
        f, ctx, d = s.facts, s.ctx, s.decision
        author = sw.team.vendor_of_branch(f.head_ref)
        times = [parse(f.head_committed_at)] + [parse(v.created_at) for v in f.verdicts]
        cs = ctx.get("comments") or []
        times += [parse(c.get("created_at")) for c in cs if (c.get("user") or {}).get("login") != rules.BOT_LOGIN]
        best = asked.get((f.number, f.head_sha), ("", ""))   # asks on the job issue are in the stream
        for c in cs:
            if (c.get("user") or {}).get("login") != rules.BOT_LOGIN:
                keeps |= {m.group("branch") for m in KEEP.finditer(c.get("body") or "")}
                continue
            for m in ASK_MARK.finditer(c.get("body") or ""):
                if int(m.group("pr")) == f.number and m.group("head") == f.head_sha:
                    best = max(best, (c.get("created_at") or "", m.group("agent")))
        reviewer = best[1]
        touched = touched_at("", "", touches, f.number, since_touch, *times)
        prs.append(PrIn(f.number, f"https://github.com/{repo}/pull/{f.number}", f.head_ref, f.head_sha,
                        ctx.get("body") or "", d.action, tuple(d.reasons), f.proof, ctx.get("proof_done_at") or "",
                        f.head_committed_at, rules.needs_review(f, author, sw.team), reviewer, touched))

    # live branches: the repo's push activity, newest first (a deletion after the last push means gone)
    acts, cut = newest_first(sw, "activity", f"repos/{repo}/activity?time_period=week&per_page=100", since_live,
                             lambda a: parse(a.get("timestamp")), ACTIVITY_KEYS, cursor=True)
    acts = [a for a in acts if (parse(a.get("timestamp")) or now) >= since_live]
    if cut:
        problems.append(f"repo activity: more than {MAX_PAGES * 100} entries this week, the rest not read")
    last: dict[str, dict] = {}
    for a in sorted(acts, key=lambda a: a.get("timestamp") or ""):
        ref = a.get("ref") or ""
        if ref.startswith("refs/heads/"):
            last[ref[len("refs/heads/"):]] = a
    branches: list[BranchIn] = []
    reads = 0
    for name, a in sorted(last.items()):
        if a.get("activity_type") == "branch_deletion":
            continue
        b = BranchIn(name, a.get("after") or "", parse(a["timestamp"]) or now)
        if not job_of_branch(name) and name not in system_branches and not name.startswith(SYSTEM_PREFIXES) \
                and name != rules.TRUNK:
            if reads < MAX_COMMIT_READS:
                reads += 1
                try:
                    b.job_line = (gh.get(f"repos/{repo}/commits/{b.sha}") or {}).get("commit", {}).get("message", "")
                except Exception as err:   # noqa: BLE001  the branch is still listed, as unmatched
                    problems.append(f"head commit of {clean(name, 60)}: {type(err).__name__}")
            else:
                problems.append(f"head commit of {clean(name, 60)} not read ({MAX_COMMIT_READS} per sweep)")
        branches.append(b)

    # a branch with no open PR: at a closed PR's head (from the one closed listing, no per-branch call), else one
    # trunk compare per head, cached for the sweep: its head in trunk has landed (classify's branch_fate)
    closed_raw, cut = page_list(gh, f"repos/{repo}/pulls?state=closed&sort=updated&direction=desc",
                                stop=lambda p: (parse(p.get("updated_at")) or now) < now - CLOSED_WINDOW)
    if cut:
        problems.append(f"closed PRs: more than {MAX_PAGES * 100} since {iso(now - CLOSED_WINDOW)}, the rest not read")
    closed: dict[str, list[tuple[str, ClosedPr]]] = {}
    for p in closed_raw:
        h = p.get("head") or {}
        if ((h.get("repo") or {}).get("full_name") or "") != repo:
            continue
        closed.setdefault(h.get("ref") or "", []).append(
            (p.get("closed_at") or "", ClosedPr(p["number"], h.get("sha") or "", bool(p.get("merged_at")))))
    compared: dict[str, dict | None] = {}
    for b in branches:
        if b.name == rules.TRUNK or b.name in system_branches or b.name.startswith(SYSTEM_PREFIXES) \
                or b.name in open_heads:
            continue
        b.closed = tuple(c for _, c in sorted(closed.get(b.name, []), key=lambda x: x[0], reverse=True))
        if not b.sha or any(c.head_sha == b.sha for c in b.closed):
            continue
        if b.sha not in compared:
            try:
                compared[b.sha] = gh.get(f"repos/{repo}/compare/{rules.TRUNK}...{b.sha}") or {}
            except Exception as err:   # noqa: BLE001  the branch is still listed, as if not landed
                compared[b.sha] = None
                problems.append(f"trunk compare of {clean(b.name, 60)}: {type(err).__name__}")
        got = compared[b.sha] or {}
        b.vs_trunk, b.ahead_by = got.get("status") or "", int(got.get("ahead_by") or 0)
        b.ahead = tuple(c.get("sha") or "" for c in got.get("commits") or [])

    # superseded (J-072): two live branches in one job's lane are compared by their work, not their job number:
    # the older is held by the newer when its head is an ancestor of the newer's, or the newer changes every file
    # it changes. One compare per pair, and the trunk compares above (or one more per head) for the files.
    def trunk_compare(sha: str) -> dict:
        if sha not in compared:
            compared[sha] = gh.get(f"repos/{repo}/compare/{rules.TRUNK}...{sha}") or {}
        return compared[sha] or {}

    overlap: dict[tuple[str, str], str] = {}
    refs = set(open_heads) | {p.head_ref for p in prs}
    for group in lane_groups(branches, refs, system_branches).values():
        current = group[0]
        for old in group[1:]:
            try:
                overlap[(old.sha, current.sha)] = held_by(gh.get(f"repos/{repo}/compare/{old.sha}...{current.sha}") or {},
                                                          trunk_compare(old.sha), trunk_compare(current.sha),
                                                          current.name)
            except Exception as err:   # noqa: BLE001  not read: not marked superseded, and the read is listed
                problems.append(f"superseded compare of {clean(old.name, 50)} and {clean(current.name, 50)}: "
                                f"{type(err).__name__}")

    # proof runs in the window; the newest on a live head (or trunk) is judged by its jobs when it is not green
    raw = gh.paged(f"repos/{repo}/actions/workflows/{proof_workflow}/runs?created=%3E%3D{iso(since_live)}", "workflow_runs")
    if len(raw) >= 500:
        problems.append("proof runs: 500 or more in the window, the rest not read")
    runs = [RunIn(r["id"], r.get("html_url") or "", r.get("head_branch") or "", r.get("head_sha") or "",
                  parse(r.get("created_at")) or now, parse(r.get("updated_at")),
                  r.get("conclusion") or "" if r.get("status") == "completed" else "") for r in raw]
    newest: dict[str, RunIn] = {}
    for r in sorted(runs, key=lambda r: r.created_at):
        newest[r.branch] = r
    heads = {b.name: b.sha for b in branches} | {p.head_ref: p.head_sha for p in prs} | {rules.TRUNK: ""}
    by_id = {r["id"]: r for r in raw}
    for name, r in newest.items():
        if name in heads and (not heads[name] or heads[name] == r.sha) and r.conclusion not in ("success", "cancelled", "skipped"):
            try:
                r.state = sw.run_state(by_id[r.id])[0]
            except Exception as err:   # noqa: BLE001
                problems.append(f"proof run {r.id} jobs: {type(err).__name__}")

    # With the plan live (work/switchboard.yml `plan: live`) every open issue of the team's titled J-NNN is a job,
    # with the `job` label or not: a missing label is itself a collaboration defect and must never make the job
    # disappear (a stranger's issue with a job's title is not one: the repo is public). Until then the jobs are
    # the issues that carry the `job` label, as before.
    plan_live = getattr(sw, "plan_mode", lambda: "off")() == "live"
    with_label = [] if plan_live else gh.paged(f"repos/{repo}/issues?labels=job&state=open")
    all_open_issues = gh.paged(f"repos/{repo}/issues?state=open")
    issues: list[IssueIn] = []
    for it in all_open_issues if plan_live else with_label:
        if "pull_request" in it or (plan_live and not (rules.job_of_title(it.get("title") or "")
                                                       and rules.team_issue(it))):
            continue
        labels = tuple(l["name"] for l in it.get("labels") or [])
        issues.append(IssueIn(it["number"], it.get("html_url") or "", it.get("title") or "", labels,
                              touched_at(it.get("created_at"), it.get("updated_at"), touches, it["number"], since_touch)))

    open_threads = {int(i["number"]) for i in all_open_issues}
    stream = []
    for c in comments:
        try:
            n = int((c.get("issue_url") or "").rsplit("/", 1)[1])
        except (ValueError, IndexError):
            continue
        stream.append({"n": n, "t": parse(c["created_at"]), "url": c.get("html_url") or "", "body": c.get("body") or "",
                       "id": str(c.get("id")), "login": (c.get("user") or {}).get("login")})
    judge = ask_judge(roster, thread_jobs(issues, prs, frozenset(system_prs))) if roster is not None else {}
    closed = closed_prs(events)
    found = [AskIn(a["id"], a["n"], a["url"], a["target"], a["a"], a["t"])
             for a in asks.unanswered(stream, open_threads, since_live, closed=closed, **judge)
             if a["login"] != rules.BOT_LOGIN]

    feed = Feed(prs, open_prs, branches, issues, found, runs, rules.TRUNK, frozenset(system_branches),
                frozenset(system_prs), frozenset(keeps), open_heads, tuple(problems), board, repo, overlap, closed)
    return feed, sent


def thread_jobs(issues: list, prs: list, system_prs: Iterable[int]) -> dict[int, str]:
    """Each job's own threads -> its job id: the job's issue (the first open one whose title names it) and its open
    PRs (the `Job:` line, else the branch name). The board, the inbox and any thread that names no job are not in
    it. The reconciler files its records by this same map (reconciler.observe)."""
    out: dict[int, str] = {}
    for i in issues:
        job = rules.job_of_title(i.title)
        if job and job not in out.values():
            out[i.number] = job
    for pr in prs:
        if pr.number in system_prs:
            continue
        job = rules.job_of_pr(pr.job_line) or job_of_branch(pr.head_ref)
        if job:
            out[pr.number] = job
    return out


def ask_judge(roster: Roster, thread_job: dict[int, str]) -> dict:
    """The keywords tools/board/asks.py's read() and answer_rule() take, from the roster and the job threads, so the
    ledger and the reconciler put the same questions to the one judge: which threads are a job's own
    (`job_threads`), which names are one agent (`who`), and the reconciler's id for an ask (`oid`)."""
    def who(name: str) -> str:
        p = roster.of_name(name)
        return p.agent if p else name.lower()

    def oid(comment: dict, target: str) -> str:
        return ob_id(thread_job.get(comment["n"], ""), "ask", who(target), str(comment.get("id")))

    return {"job_threads": frozenset(thread_job), "who": who, "oid": oid}


def lane_groups(branches: list[BranchIn], refs: set[str], system_branches: Iterable[str]) -> dict:
    """The live branches classify groups for SUPERSEDED, by (job, protected lane), each newest push first: not a
    system branch, and not settled (landed, or at a closed PR's head) unless an open PR is on it."""
    system = set(system_branches)
    groups: dict[tuple[str, bool], list[BranchIn]] = {}
    for b in branches:
        if b.name == rules.TRUNK or b.name in system or b.name.startswith(SYSTEM_PREFIXES):
            continue
        if b.name not in refs and branch_fate(b)[0]:
            continue
        job = job_of_branch(b.name) or rules.job_of_pr(b.job_line)
        if job:
            groups.setdefault((job, "/integrate-protected-" in b.name), []).append(b)
    return {k: sorted(g, key=lambda b: (b.pushed_at, b.name), reverse=True) for k, g in groups.items() if len(g) > 1}


def held_by(between: dict, old_vs_trunk: dict, new_vs_trunk: dict, newer: str) -> str:
    """Why the newer branch holds the older one's work ("" when it does not): `between` is compare old...new."""
    if (between.get("status") or "") in ("ahead", "identical"):
        return f"its head is in {newer}"
    old_files = {f.get("filename") for f in old_vs_trunk.get("files") or []} - {None}
    new_files = {f.get("filename") for f in new_vs_trunk.get("files") or []} - {None}
    if old_files and old_files <= new_files:
        return f"{newer} changes every file it changes"
    return ""


def sweep(sw, steered: list, board: int, muse_label: str, proof_workflow: str, system_branches: Iterable[str],
          system_prs: Iterable[int], when: str, notify: bool = True) -> Ledger:
    """The switch's one call (switch.py main, after a full steer sweep). `notify` False: the section only (run)."""
    import yaml
    roster = Roster(yaml.safe_load((sw.root / "work/agents.yml").read_text(encoding="utf-8")) or {})
    feed, sent = read_feed(sw, steered, sw.now, board, proof_workflow, system_branches, system_prs, roster)
    return run(sw.gh, feed, roster, sw.now, when, sw.out, muse_label, sent, notify=notify)


def main() -> int:
    """A dry run against the live repo: the PRs read as the sweep reads them (without the rule check)."""
    import argparse
    import reconciler
    import switch
    from ghapi import GitHub
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--repo", default=switch.REPO)
    args = ap.parse_args()
    now = dt.datetime.now(dt.timezone.utc)
    sw = switch.Switch(GitHub(args.repo, True), HERE.parents[1], now)
    steered = []
    for pr in sw.target_prs(0, ""):
        facts, ctx = sw.pr_facts(pr)
        d = rules.merge_decision(facts, sw.team.vendor_of_branch(facts.head_ref), sw.team)
        steered.append(switch.Steered(facts, ctx, d))
    sweep(sw, steered, sw.board_issue(), switch.MUSE_LABEL, switch.PROOF_WORKFLOW,
          {switch.LEDGER_BRANCH, switch.INBOX_BRANCH}, {switch.INBOX_PR}, switch.central(now),
          reconciler.ledger_notifies(sw.root))   # as the switch calls it (work/switchboard.yml `notices`)
    return 0


if __name__ == "__main__":
    sys.exit(main())
