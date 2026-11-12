"""Versioned collaboration plan: one authored order, derived status.

Labels, board text and mail are projections. This module never edits GitHub (observe() only reads, through
the client it is handed),
does not infer local work from mail, and rejects ambiguous or cyclic orders.
"""
from __future__ import annotations

from dataclasses import dataclass
import datetime as dt
import hashlib
import re

JOB = re.compile(r"^J-\d{3,}[a-z]?$")
ORDER = re.compile(r"^(J-\d{3,}[a-z]?)\s+([a-z][a-z0-9-]*)\s+#(\d+)(.*?)\s*\|\s*(\S.*)$")
LIMIT = re.compile(r"^limit\s+([a-z][a-z0-9-]*)\s+(\d+)$")
AFTER = re.compile(r"after\s+((?:J-\d{3,}[a-z]?\s*,?\s*)+)")
HOLD = re.compile(r"\bhold:\s*(.+)$")
PR_JOB = re.compile(r"^\s*Job:\s*(J-\d{3,}[a-z]?)\b")
BRANCH_JOB = re.compile(r"(?:^|/)integrate-(?:protected-)?(j\d{3,}[a-z]?)(?![\w])|(?:^|/)wip-(j\d{3,}[a-z]?)(?![\w])", re.I)


@dataclass(frozen=True)
class Line:
    job: str
    owner: str
    issue: int
    after: tuple[str, ...]
    hold: str
    title: str


@dataclass(frozen=True)
class Plan:
    lines: tuple[Line, ...]
    limits: dict[str, int]
    errors: tuple[str, ...]

    def by_job(self) -> dict[str, Line]:
        return {x.job: x for x in self.lines}

    def owner_lines(self, owner: str) -> tuple[Line, ...]:
        return tuple(x for x in self.lines if x.owner == owner)

    def fingerprint(self) -> str:
        return hashlib.sha256(render(self).encode()).hexdigest()[:12]


def parse(text: str) -> Plan:
    lines: list[Line] = []
    limits: dict[str, int] = {}
    errors: list[str] = []
    seen: set[str] = set()
    for number, raw in enumerate(text.splitlines(), 1):
        s = raw.strip()
        if not s or s.startswith("#"):
            continue
        limit = LIMIT.fullmatch(s)
        if limit:
            agent, count = limit.groups()
            if agent in limits or int(count) < 1:
                errors.append(f"line {number}: invalid or duplicate {agent} limit")
            else:
                limits[agent] = int(count)
            continue
        m = ORDER.fullmatch(s)
        if not m:
            errors.append(f"line {number}: malformed order")
            continue
        job, owner, issue, options, title = m.groups()
        if job in seen:
            errors.append(f"line {number}: duplicate {job}")
        seen.add(job)
        a = AFTER.search(options)
        deps = tuple(dict.fromkeys(re.findall(r"J-\d{3,}[a-z]?", a.group(1)))) if a else ()
        h = HOLD.search(options)
        hold = h.group(1).strip() if h else ""
        extras = options.replace(a.group(0), "") if a else options
        extras = extras.replace(h.group(0), "") if h else extras
        if extras.strip() or job in deps:
            errors.append(f"line {number}: invalid options or self-dependency for {job}")
        lines.append(Line(job, owner, int(issue), deps, hold, title))
    for ln in lines:
        if ln.owner not in limits:
            errors.append(f"{ln.job}: owner {ln.owner} has no limit")
    graph = {l.job: l.after for l in lines}
    done: set[str] = set()
    visiting: set[str] = set()

    def visit(job: str, path: tuple[str, ...]) -> None:
        if job in visiting:
            errors.append("dependency cycle: " + " -> ".join(path + (job,)))
            return
        if job in done:
            return
        visiting.add(job)
        for dep in graph[job]:
            if dep in graph:
                visit(dep, path + (job,))
        visiting.remove(job)
        done.add(job)

    for job in graph:
        visit(job, ())
    return Plan(tuple(lines), limits, tuple(errors))


def render(plan: Plan) -> str:
    result = [f"limit {owner} {limit}" for owner, limit in sorted(plan.limits.items())]
    for line in plan.lines:
        suffix = (" after " + ", ".join(line.after) if line.after else "")
        suffix += (" hold: " + line.hold if line.hold else "")
        result.append(f"{line.job} {line.owner} #{line.issue}{suffix} | {line.title}")
    return "\n".join(result) + "\n"


def job_of_branch(ref: str) -> str:
    m = BRANCH_JOB.search(ref or "")
    if not m:
        return ""
    key = (m.group(1) or m.group(2))[1:].lower()
    return "J-" + key


def branch_name(ref) -> str:
    return ref if isinstance(ref, str) else ref.get("name") or ""


def branch_sha(ref) -> str:
    return "" if isinstance(ref, str) else (ref.get("commit") or {}).get("sha") or ref.get("sha") or ""


def owns(owner: str, ref: str) -> bool:
    return ref.startswith("fb-bot/") if owner == "muse" else ref.startswith(owner + "/")


def live_refs(job: str, branches, inactive: set[str] | frozenset[str] = frozenset()) -> list[str]:
    """Branches that carry unlanded work for `job`. A head that is a merged or closed PR's head, or is already
    in trunk, is history, not work in flight (it would otherwise count forever). A bare name with no sha
    cannot be proven landed, so it still counts."""
    return [branch_name(b) for b in _live(job, branches, inactive)]


def _live(job: str, branches, inactive) -> list:
    return [b for b in branches
            if job_of_branch(branch_name(b)) == job and not (branch_sha(b) and branch_sha(b) in inactive)]


def _open_pr(job: str, prs) -> bool:
    return any((m := PR_JOB.match(p.get("body") or "")) and m.group(1) == job
               for p in prs if p.get("state") != "closed")


def status(line: Line, *, closed: set[str] | frozenset[str] = frozenset(),
           prs: tuple[dict, ...] | list[dict] = (), branches: tuple[str, ...] | list[str] = (),
           started: set[str] | frozenset[str] = frozenset(),
           inactive: set[str] | frozenset[str] = frozenset(),
           merged: set[str] | frozenset[str] = frozenset(),
           dormant: set[str] | frozenset[str] = frozenset()) -> str:
    """done, review, held, pushed, started, dormant, contested, landed, waiting or queued. Only started,
    pushed and review are in flight. A hold outranks remote work (the work stays on its branch, nobody is
    moving it); it never outranks an open PR, which lands on its proof and reviews like any other. Only the
    owner's own branches are the owner's lane: another vendor's live head is `contested` (NOT-OWNER), and an
    owner's head with no PR and no commit for DORMANT_DAYS (`dormant` shas) is kept and flagged, not counted."""
    if line.job in closed:
        return "done"
    if _open_pr(line.job, prs):
        return "review"
    if line.hold:
        return "held"
    live = _live(line.job, branches, inactive)
    own = [b for b in live if owns(line.owner, branch_name(b))]
    if any(not (branch_sha(b) and branch_sha(b) in dormant) for b in own):
        return "pushed"
    if line.job in started:
        return "started"
    if own:
        return "dormant"
    if live:
        return "contested"
    if line.job in merged:
        return "landed"  # its PRs merged, the issue is still open: close it or say what is left
    if any(dep not in closed for dep in line.after):
        return "waiting"
    return "queued"


IN_FLIGHT = frozenset({"started", "pushed", "review"})
DORMANT_DAYS = 3   # CLAUDE.md: a job held 3 days goes to a stronger agent


def findings(plan: Plan, issues: list[dict], branches: list[str], prs: list[dict] = (),
             started: set[str] | frozenset[str] = frozenset(),
             inactive: set[str] | frozenset[str] = frozenset(),
             merged: set[str] | frozenset[str] = frozenset(),
             dormant: set[str] | frozenset[str] = frozenset()) -> list[str]:
    """Fail closed on inconsistent observations; do not silently rewrite the order."""
    errors = list(plan.errors)
    by_job = plan.by_job()
    open_issues = {}
    for issue in issues:
        if issue.get("state", "open") != "open" or issue.get("pull_request") is not None:
            continue
        m = re.match(r"^(J-\d{3,}[a-z]?)(?!\w)", issue.get("title") or "")
        if m:
            if m.group(1) in open_issues:
                errors.append(f"DUP-ISSUE {m.group(1)}: #{open_issues[m.group(1)]['number']} and #{issue['number']}")
            open_issues[m.group(1)] = issue
            if m.group(1) not in by_job:
                errors.append(f"NO-LINE {m.group(1)} #{issue['number']}")
    for line in plan.lines:
        issue = open_issues.get(line.job)
        if issue is None or issue.get("number") != line.issue:
            errors.append(f"NO-ISSUE {line.job} #{line.issue}")
        refs = set(live_refs(line.job, branches, inactive))
        foreign = [b for b in refs if not owns(line.owner, b)]
        if foreign:
            errors.append(f"NOT-OWNER {line.job}: {', '.join(sorted(foreign))}")
        if line.job in started and not refs and not _open_pr(line.job, prs):
            errors.append(f"UNPUSHED {line.job}: started without a remote head")
        if status(line, closed=set(), prs=prs, branches=branches, started=started, inactive=inactive,
                  dormant=dormant) == "dormant":
            errors.append(f"DORMANT {line.job}: {', '.join(sorted(b for b in refs if owns(line.owner, b)))} "
                          f"has no PR and no commit in {DORMANT_DAYS} days: land it, hold it or hand it off")
    closed = {ln.job for ln in plan.lines if ln.job not in open_issues}
    for owner, limit in plan.limits.items():
        in_flight = [ln.job for ln in plan.owner_lines(owner) if status(
                     ln, closed=closed, prs=prs, branches=branches, started=started, inactive=inactive,
                     merged=merged, dormant=dormant) in IN_FLIGHT]
        if len(in_flight) > limit:
            errors.append(f"OVER-LIMIT {owner}: {len(in_flight)}/{limit} jobs in flight")
    return errors


COMPARES = 60   # trunk-containment reads per pass for job branches no PR accounts for
ALL_PAGES = 30  # a whole listing, past the client's 500-item paged() cap (647 branches on 11-12)


def whole(gh, path: str) -> list:
    """Every item of a listing. gh.paged stops at its page cap without saying so, which silently dropped 147
    branches (fb-bot/integrate-j109 among them) from every branch check. Reads on past the cap; a listing
    longer than ALL_PAGES raises rather than be counted short."""
    items = list(gh.paged(path))
    if len(items) % 100 or not items:
        return items
    sep = "&" if "?" in path else "?"
    for page in range(len(items) // 100 + 1, ALL_PAGES + 1):
        batch = gh.get(f"{path}{sep}per_page=100&page={page}") or []
        items.extend(batch)
        if len(batch) < 100:
            return items
    raise RuntimeError(f"{path}: more than {ALL_PAGES * 100} items, refusing a short count")


def observe(gh, model: Plan, now: dt.datetime, out=print, *, listing=None, trunk: str = "ScraperX-Claude",
            errors: tuple = (Exception,)) -> dict:
    """What the plan is checked against, read through `gh` (switch and tools/board/job_status.py share it).
    `inactive`: heads that are history, not work in flight: every closed PR's head (merged or abandoned), and a
    job branch whose head is already in trunk (compare says behind or identical; a sha-pinned compare never
    changes, so its ETag keeps it cheap). `merged`: jobs with a merged `Job: J-NNN` PR. `dormant`: an
    owner-side head with no PR whose last commit is older than DORMANT_DAYS. A compare that fails or is not
    reached leaves the head live: unproven work is never hidden."""
    repo = gh.repo
    issues = whole(gh, f"repos/{repo}/issues?state=open")
    branches = whole(gh, f"repos/{repo}/branches")
    prs = (listing or gh.paged)(f"repos/{repo}/pulls?state=open")
    closed = whole(gh, f"repos/{repo}/pulls?state=closed&base={trunk}")
    inactive = {p["head"]["sha"] for p in closed if (p.get("head") or {}).get("sha")}
    merged = {m.group(1) for p in closed if p.get("merged_at") if (m := PR_JOB.match(p.get("body") or ""))}
    jobs = set(model.by_job())
    dormant: set[str] = set()
    open_heads = {(p.get("head") or {}).get("sha") for p in prs}
    todo = [b for b in branches if job_of_branch(branch_name(b)) in jobs
            and (sha := branch_sha(b)) and sha not in inactive and sha not in open_heads]
    for b in todo[:COMPARES]:
        sha = branch_sha(b)
        try:
            cmp = gh.get(f"repos/{repo}/compare/{trunk}...{sha}") or {}
        except errors as err:
            out(f"NOTE        plan: compare {branch_name(b)} failed ({err}); counted as live")
            continue
        if cmp.get("status") in ("behind", "identical"):
            inactive.add(sha)
            continue
        head = (cmp.get("commits") or [{}])[-1]
        stamp = ((head.get("commit") or {}).get("committer") or {}).get("date") or ""
        when = dt.datetime.fromisoformat(stamp.replace("Z", "+00:00")) if stamp else None
        if head.get("sha") == sha and when and now - when > dt.timedelta(days=DORMANT_DAYS):
            dormant.add(sha)
    if len(todo) > COMPARES:
        out(f"NOTE        plan: {len(todo) - COMPARES} job branch(es) not compared this pass; counted as live")
    return {"issues": issues, "branches": branches, "prs": prs, "inactive": frozenset(inactive),
            "merged": frozenset(merged), "dormant": frozenset(dormant)}


def view(plan: Plan, owner: str, statuses: dict[str, str], issue_bodies: dict[str, str] | None = None) -> tuple[str, str]:
    parts = [f"{ln.job} #{ln.issue} {statuses.get(ln.job, 'unknown')} {ln.title}"
             for ln in plan.owner_lines(owner)]
    bodies = issue_bodies or {}
    parts += [f"{ln.job} body={hashlib.sha256(bodies.get(ln.job, '').encode()).hexdigest()[:12]}"
              for ln in plan.owner_lines(owner)]
    text = "\n".join(parts)
    return text, hashlib.sha256(text.encode()).hexdigest()[:12]
