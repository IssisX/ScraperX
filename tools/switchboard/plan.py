"""Versioned collaboration plan: one authored order, derived status, no network.

Labels, board text and mail are projections. This module never edits GitHub,
does not infer local work from mail, and rejects ambiguous or cyclic orders.
"""
from __future__ import annotations

from dataclasses import dataclass
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


def live_refs(job: str, branches, inactive: set[str] | frozenset[str] = frozenset()) -> list[str]:
    """Branches that carry unlanded work for `job`. A head that is a merged or closed PR's head, or is already
    in trunk, is history, not work in flight (it would otherwise count forever). A bare name with no sha
    cannot be proven landed, so it still counts."""
    return [branch_name(b) for b in branches
            if job_of_branch(branch_name(b)) == job and not (branch_sha(b) and branch_sha(b) in inactive)]


def _open_pr(job: str, prs) -> bool:
    return any((m := PR_JOB.match(p.get("body") or "")) and m.group(1) == job
               for p in prs if p.get("state") != "closed")


def status(line: Line, *, closed: set[str] | frozenset[str] = frozenset(),
           prs: tuple[dict, ...] | list[dict] = (), branches: tuple[str, ...] | list[str] = (),
           started: set[str] | frozenset[str] = frozenset(),
           inactive: set[str] | frozenset[str] = frozenset(),
           merged: set[str] | frozenset[str] = frozenset()) -> str:
    """done, review, held, pushed, started, landed, waiting or queued. Only started, pushed and review are in
    flight. A hold outranks remote work (the work stays on its branch, nobody is moving it); it never
    outranks an open PR, which lands on its proof and reviews like any other."""
    if line.job in closed:
        return "done"
    if _open_pr(line.job, prs):
        return "review"
    if line.hold:
        return "held"
    if live_refs(line.job, branches, inactive):
        return "pushed"
    if line.job in started:
        return "started"
    if line.job in merged:
        return "landed"  # its PRs merged, the issue is still open: close it or say what is left
    if any(dep not in closed for dep in line.after):
        return "waiting"
    return "queued"


IN_FLIGHT = frozenset({"started", "pushed", "review"})


def findings(plan: Plan, issues: list[dict], branches: list[str], prs: list[dict] = (),
             started: set[str] | frozenset[str] = frozenset(),
             inactive: set[str] | frozenset[str] = frozenset(),
             merged: set[str] | frozenset[str] = frozenset()) -> list[str]:
    """Fail closed on inconsistent observations; do not silently rewrite the order."""
    errors = list(plan.errors)
    by_job = plan.by_job()
    open_issues = {}
    for issue in issues:
        if issue.get("state", "open") != "open" or issue.get("pull_request") is not None:
            continue
        m = re.match(r"^(J-\d{3,}[a-z]?)(?!\w)", issue.get("title") or "")
        if m:
            open_issues[m.group(1)] = issue
            if m.group(1) not in by_job:
                errors.append(f"NO-LINE {m.group(1)} #{issue['number']}")
    for line in plan.lines:
        issue = open_issues.get(line.job)
        if issue is None or issue.get("number") != line.issue:
            errors.append(f"NO-ISSUE {line.job} #{line.issue}")
        refs = set(live_refs(line.job, branches, inactive))
        foreign = [b for b in refs if not (b.startswith("fb-bot/") if line.owner == "muse" else
                                           b.startswith(line.owner + "/"))]
        if foreign:
            errors.append(f"NOT-OWNER {line.job}: {', '.join(sorted(foreign))}")
        if line.job in started and not refs and not _open_pr(line.job, prs):
            errors.append(f"UNPUSHED {line.job}: started without a remote head")
    closed = {ln.job for ln in plan.lines if ln.job not in open_issues}
    for owner, limit in plan.limits.items():
        in_flight = [ln.job for ln in plan.owner_lines(owner) if status(
                     ln, closed=closed, prs=prs, branches=branches, started=started, inactive=inactive,
                     merged=merged) in IN_FLIGHT]
        if len(in_flight) > limit:
            errors.append(f"OVER-LIMIT {owner}: {len(in_flight)}/{limit} jobs in flight")
    return errors


def view(plan: Plan, owner: str, statuses: dict[str, str], issue_bodies: dict[str, str] | None = None) -> tuple[str, str]:
    parts = [f"{ln.job} #{ln.issue} {statuses.get(ln.job, 'unknown')} {ln.title}"
             for ln in plan.owner_lines(owner)]
    bodies = issue_bodies or {}
    parts += [f"{ln.job} body={hashlib.sha256(bodies.get(ln.job, '').encode()).hexdigest()[:12]}"
              for ln in plan.owner_lines(owner)]
    text = "\n".join(parts)
    return text, hashlib.sha256(text.encode()).hexdigest()[:12]
