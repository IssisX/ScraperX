"""Live job control: issue labels are orders; mail, queue text and board notes are not.

Pure validation for switchboard and the read-only board CLI. It never changes labels,
ignores no job for a missing label, and fails closed when an order is ambiguous.
"""
from __future__ import annotations

from dataclasses import dataclass
import re

JOB = re.compile(r"^(J-\d{3,}[a-z]?)(?![\w])", re.I)
BRANCH_JOB = re.compile(r"(?:^|/)integrate-(?:protected-)?(j-?\d{3,}[a-z]?)(?![\w])", re.I)
DEPENDENCY = re.compile(r"^Blocked-by:\s*(.+)$", re.I | re.M)
JOB_REF = re.compile(r"J-\d{3,}[a-z]?(?![\w])", re.I)
STATES = frozenset({"ready", "active", "parked"})
MUSE_WIP_LIMIT = 4


def job_id(title: str) -> str:
    found = JOB.match(title or "")
    return found.group(1).upper() if found else ""


def branch_job(ref: str) -> str:
    match = BRANCH_JOB.search(ref or "")
    return ("J-" + match.group(1)[1:].lstrip("-").zfill(3)).upper() if match else ""


def label_names(issue: dict) -> frozenset[str]:
    return frozenset((x if isinstance(x, str) else x.get("name") or "")
                     for x in issue.get("labels") or [])


@dataclass(frozen=True)
class Job:
    id: str
    number: int
    owner: str
    state: str
    labels: frozenset[str]
    blocked_by: tuple[str, ...]
    errors: tuple[str, ...]


@dataclass(frozen=True)
class Inventory:
    jobs: dict[str, Job]
    violations: tuple[str, ...]
    ready_muse: tuple[Job, ...]
    active_muse: int

    def start_problem(self, job: str, agent: str) -> str:
        item = self.jobs.get(job)
        if not item:
            return f"{job}: no open job issue"
        if item.errors:
            return f"{job}: invalid issue ({'; '.join(item.errors)})"
        if item.owner != agent:
            return f"{job}: issue owner is {item.owner}, not {agent}"
        if item.state != "ready":
            return f"{job}: state is {item.state}, not ready"
        if item.blocked_by:
            blockers = [j for j in item.blocked_by if j in self.jobs]
            if blockers:
                return f"{job}: blocked by {', '.join(blockers)}"
        if agent == "muse" and self.active_muse >= MUSE_WIP_LIMIT:
            return f"{job}: Muse WIP cap {MUSE_WIP_LIMIT} reached"
        return ""


def validate(issue: dict) -> Job:
    name = job_id(issue.get("title") or "")
    labels = label_names(issue)
    owners = sorted(x[6:] for x in labels if x.startswith("owner:"))
    states = sorted(x[6:] for x in labels if x.startswith("state:"))
    errors = []
    if "job" not in labels:
        errors.append("missing job label")
    if len(owners) != 1 or not owners[0]:
        errors.append("requires exactly one owner label")
    if len(states) != 1 or states[0] not in STATES:
        errors.append("requires one state:ready, state:active or state:parked")
    matches = DEPENDENCY.findall(issue.get("body") or "")
    deps = tuple(dict.fromkeys(x.upper() for row in matches for x in JOB_REF.findall(row)))
    if name in deps:
        errors.append("depends on itself")
    return Job(name, int(issue["number"]), owners[0] if len(owners) == 1 else "",
               states[0] if len(states) == 1 else "", labels, deps, tuple(errors))


def inventory(issues: list[dict], branches: list[str] | tuple[str, ...] = (),
              open_prs: list[dict] | tuple[dict, ...] = ()) -> Inventory:
    jobs: dict[str, Job] = {}
    violations: list[str] = []
    for issue in issues:
        if issue.get("pull_request") is not None or issue.get("state", "open") != "open":
            continue
        if not job_id(issue.get("title") or ""):
            continue
        item = validate(issue)
        if item.id in jobs:
            violations.append(f"{item.id}: duplicate issues #{jobs[item.id].number} and #{item.number}")
            continue
        jobs[item.id] = item
        violations.extend(f"{item.id} #{item.number}: {err}" for err in item.errors)

    remote: dict[str, set[str]] = {}
    for branch in branches:
        name = branch if isinstance(branch, str) else branch.get("name") or ""
        job = branch_job(name)
        if job:
            remote.setdefault(job, set()).add(name)
    for pr in open_prs:
        branch = (pr.get("head") or {}).get("ref") or ""
        job = job_id((pr.get("body") or "").removeprefix("Job: ")) or branch_job(branch)
        if job and branch:
            remote.setdefault(job, set()).add(branch)

    active_muse = sum(j.owner == "muse" and j.state == "active" for j in jobs.values())
    if active_muse > MUSE_WIP_LIMIT:
        violations.append(f"Muse WIP: {active_muse} active jobs exceeds cap {MUSE_WIP_LIMIT}; do not start more")
    for item in jobs.values():
        refs = remote.get(item.id, set())
        if item.state == "active" and not refs:
            violations.append(f"{item.id} #{item.number}: STARTED but no remote branch or open PR")
        if item.state == "ready" and refs:
            violations.append(f"{item.id} #{item.number}: ready despite pushed work: {', '.join(sorted(refs))}")
        if any(ref.startswith("fb-bot/") for ref in refs) and any(ref.startswith("claude/") for ref in refs):
            violations.append(f"{item.id} #{item.number}: two vendors have branches: {', '.join(sorted(refs))}")
        for dep in item.blocked_by:
            if dep in jobs and item.state == "active":
                violations.append(f"{item.id} #{item.number}: active before {dep} is complete")
    visited: set[str] = set()
    stack: set[str] = set()

    def walk(name: str, path: tuple[str, ...]) -> None:
        if name in stack:
            violations.append("dependency cycle: " + " -> ".join(path + (name,)))
            return
        if name in visited:
            return
        stack.add(name)
        for dep in jobs[name].blocked_by:
            if dep in jobs:
                walk(dep, path + (name,))
        stack.remove(name)
        visited.add(name)

    for name in sorted(jobs):
        walk(name, ())
    ready = tuple(sorted((j for j in jobs.values() if j.owner == "muse" and j.state == "ready"
                          and not j.errors and not any(dep in jobs for dep in j.blocked_by)
                          and not remote.get(j.id)), key=lambda j: j.number))
    return Inventory(jobs, tuple(violations), ready, active_muse)


def merge_problem(job: str, branch: str, issue: dict | None) -> str:
    """Do not merge one owner's work under another owner's or a parked job's order."""
    if not issue:
        return f"{job}: no open job issue"
    item = validate(issue)
    if item.id != job or item.errors:
        return f"{job}: invalid issue or missing labels: {'; '.join(item.errors)}"
    if item.state == "parked":
        return f"{job}: parked job cannot merge"
    if branch.startswith("fb-bot/") and item.owner != "muse":
        return f"{job}: fb-bot branch, but issue owner is {item.owner}"
    if branch.startswith("claude/") and item.owner != "claude":
        return f"{job}: Claude branch, but issue owner is {item.owner}"
    return ""
