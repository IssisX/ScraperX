"""The collaboration plan: work/plan.md, the one authored record of who owns a job, in what order, held or not.

This module parses and renders that file and reads what it is checked against (observe). A job's state is never
written here: machine.py computes it from the plan and the evidence. Nothing here edits GitHub (observe() only
reads, through the client it is handed), and an ambiguous or cyclic order is rejected.
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
# The integrator's ruling on another agent's unlanded head for this job: it is kept as a record, not a lane.
# The sha pins the ruling: once that branch moves, it is unlanded work again.
KEPT = re.compile(r"\bkept\s+([\w./-]+)@([0-9a-f]{7,40})\b")
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
    kept: tuple[tuple[str, str], ...] = ()   # (branch, sha) heads ruled a record


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
        extras = options.replace(h.group(0), "") if h else options
        kept = tuple(dict.fromkeys(KEPT.findall(extras)))
        extras = KEPT.sub("", extras.replace(a.group(0), "") if a else extras)
        if extras.strip() or job in deps:
            errors.append(f"line {number}: invalid options or self-dependency for {job}")
        lines.append(Line(job, owner, int(issue), deps, hold, title, kept))
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
        suffix += "".join(f" kept {ref}@{sha}" for ref, sha in line.kept)
        suffix += (" hold: " + line.hold if line.hold else "")
        result.append(f"{line.job} {line.owner} #{line.issue}{suffix} | {line.title}")
    return "\n".join(result) + "\n"


def job_of_branch(ref: str) -> str:
    m = BRANCH_JOB.search(ref or "")
    if not m:
        return ""
    key = (m.group(1) or m.group(2))[1:].lower()
    return "J-" + key


LETTERED = re.compile(r"(J-\d{3,})[a-z]")


def plan_job(job: str, jobs) -> str:
    """The job an id is, against the plan's jobs: itself when the plan has its line, else, for a lettered split
    (J-044c) whose base (J-044) has a line, the base; else itself (a job the plan does not hold)."""
    m = LETTERED.fullmatch(job or "")
    return m.group(1) if m and job not in jobs and m.group(1) in jobs else job


def branch_name(ref) -> str:
    return ref if isinstance(ref, str) else ref.get("name") or ""


def branch_sha(ref) -> str:
    return "" if isinstance(ref, str) else (ref.get("commit") or {}).get("sha") or ref.get("sha") or ""


def live_refs(job: str, branches, inactive: set[str] | frozenset[str] = frozenset()) -> list[str]:
    """Branches that carry unlanded work for `job`. A head that is a merged or closed PR's head, or is already
    in trunk, is history, not work in flight (it would otherwise count forever). A bare name with no sha
    cannot be proven landed, so it still counts. (The editor's guard: is there remote work to dispose of?)"""
    return [branch_name(b) for b in _live(job, branches, inactive)]


def _live(job: str, branches, inactive) -> list:
    return [b for b in branches
            if job_of_branch(branch_name(b)) == job and not (branch_sha(b) and branch_sha(b) in inactive)]


PLAN_FILE = "work/plan.md"
COMPARES = 60   # trunk-containment reads per pass for job branches no PR accounts for
CLOSED_READS = 30   # plan lines with no open issue whose issue is read each pass (tidy keeps them few)
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


CLOSED_OVERLAP = dt.timedelta(hours=1)   # a warm read of the merged PRs goes back this far before the last one


def merged_prs(gh, trunk: str) -> list:
    """Every PR merged into trunk, trimmed to what observe() reads (number, merged_at, head sha, the body's first
    line). It only grows, and the whole closed listing was read every full pass (310 PRs on 11-13, 4 pages, most of
    them 200s since a close shifts every page; past ALL_PAGES it would raise and stop the plan pass). So with the
    sweep state (`gh.state`, saved with the signed cache) the list is kept there, and a warm pass reads the closed
    PRs newest-updated first only back to the last pass's newest, less CLOSED_OVERLAP: a merge always updates its PR,
    so none is missed. Without the state (tools/board/job_status.py) it is the whole listing, as before."""
    path = f"repos/{gh.repo}/pulls?state=closed&base={trunk}"

    def slim(p: dict) -> dict:
        return {"number": p.get("number"), "merged_at": p.get("merged_at"), "updated_at": p.get("updated_at") or "",
                "head": {"sha": (p.get("head") or {}).get("sha") or ""}, "body": ((p.get("body") or "")[:200])}

    state = gh.state if isinstance(getattr(gh, "state", None), dict) else None
    kept = (state or {}).get("plan_merged")
    if state is None or not isinstance(kept, dict) or not isinstance(kept.get("prs"), list) or not kept.get("at"):
        got = [slim(p) for p in whole(gh, path) if p.get("merged_at")]
    else:
        floor = (dt.datetime.fromisoformat(str(kept["at"]).replace("Z", "+00:00")) - CLOSED_OVERLAP)
        floor_s = floor.strftime("%Y-%m-%dT%H:%M:%SZ")
        fresh: list = []
        for page in range(1, ALL_PAGES + 1):
            batch = gh.get(f"{path}&sort=updated&direction=desc&per_page=100&page={page}") or []
            fresh += batch
            if len(batch) < 100 or (batch[-1].get("updated_at") or "") < floor_s:
                break
        else:
            raise RuntimeError(f"{path}: more than {ALL_PAGES * 100} PRs updated since {floor_s}, refusing a short count")
        by_number = {p.get("number"): p for p in kept["prs"] if isinstance(p, dict)}
        by_number.update({p.get("number"): slim(p) for p in fresh if p.get("merged_at")})
        got = list(by_number.values())
    if state is not None:
        newest = max((p.get("updated_at") or "" for p in got), default="") or str((kept or {}).get("at") or "")
        if newest:
            state["plan_merged"] = {"at": newest, "prs": got}
    return got


def observe(gh, model: Plan, now: dt.datetime, out=print, *, listing=None, trunk: str = "ScraperX-Claude",
            errors: tuple = (Exception,)) -> dict:
    """What the plan is checked against, read through `gh` (switch and tools/board/job_status.py share it).
    `inactive`: proven landed heads, not work in flight: merged PR heads and heads already in trunk.
    A PR closed without merging is NOT evidence of landed work; its branch stays visible until explicitly
    reconciled or an ancestry compare proves its head is already in trunk. `merged_at`: job -> when its newest
    `Job: J-NNN` PR merged. `head_at`: sha -> its commit time, for the heads compared. `closed`: the jobs whose
    plan issue is not in the open listing and was read closed under the job's title (a missing listing entry
    alone proves nothing). A compare that fails or is not reached leaves the head live: unproven work is never
    hidden."""
    repo = gh.repo
    issues = whole(gh, f"repos/{repo}/issues?state=open")
    branches = whole(gh, f"repos/{repo}/branches")
    prs = (listing or gh.paged)(f"repos/{repo}/pulls?state=open")
    closed = merged_prs(gh, trunk)   # only merged PRs are read below (inactive, merged_at)
    inactive = {p["head"]["sha"] for p in closed if p.get("merged_at") and (p.get("head") or {}).get("sha")}
    merged_at: dict[str, str] = {}
    for p in closed:
        m = PR_JOB.match(p.get("body") or "") if p.get("merged_at") else None
        if m and p["merged_at"] > merged_at.get(m.group(1), ""):
            merged_at[m.group(1)] = p["merged_at"]
    jobs = set(model.by_job())
    head_at: dict[str, dt.datetime] = {}
    open_heads = {(p.get("head") or {}).get("sha") for p in prs}
    # the plan's jobs first (a lettered split is its base's), then the heads of jobs the plan does not hold: such
    # a head is a row only once this compare proves it unlanded (machine.collect)
    todo = sorted((b for b in branches if job_of_branch(branch_name(b))
                   and (sha := branch_sha(b)) and sha not in inactive and sha not in open_heads),
                  key=lambda b: plan_job(job_of_branch(branch_name(b)), jobs) not in jobs)
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
        if head.get("sha") == sha and when:
            head_at[sha] = when
    left = todo[COMPARES:]
    planned = sum(1 for b in left if plan_job(job_of_branch(branch_name(b)), jobs) in jobs)
    if planned:
        out(f"NOTE        plan: {planned} job branch(es) not compared this pass; counted as live")
    if len(left) > planned:
        out(f"NOTE        plan: {len(left) - planned} branch(es) of jobs the plan does not hold not compared this "
            "pass; not shown until a compare proves one unlanded")
    open_numbers = {i.get("number") for i in issues if i.get("pull_request") is None}
    gone = [ln for ln in model.lines if ln.issue not in open_numbers]
    done: set[str] = set()
    for ln in gone[:CLOSED_READS]:
        try:
            it = gh.get(f"repos/{repo}/issues/{ln.issue}") or {}
        except errors as err:
            out(f"NOTE        plan: issue #{ln.issue} ({ln.job}) not read ({err}); not counted as closed")
            continue
        if it.get("state") == "closed" and it.get("pull_request") is None and \
                re.match(rf"^{re.escape(ln.job)}(?!\w)", it.get("title") or ""):
            done.add(ln.job)
    if len(gone) > CLOSED_READS:
        out(f"NOTE        plan: {len(gone) - CLOSED_READS} line(s) with no open issue not read this pass; tidy the plan")
    planned_at = None
    try:   # when the order last changed on trunk: the clock of a job nobody has been told about or has started
        last = gh.get(f"repos/{repo}/commits?path={PLAN_FILE}&sha={trunk}&per_page=1") or [{}]
        stamp = (((last[0].get("commit") or {}).get("committer") or {}).get("date") or "") if last else ""
        planned_at = dt.datetime.fromisoformat(stamp.replace("Z", "+00:00")) if stamp else None
    except errors as err:
        out(f"NOTE        plan: the plan file's last commit was not read ({err})")
    return {"issues": issues, "branches": branches, "prs": prs, "inactive": frozenset(inactive),
            "merged_at": merged_at, "head_at": head_at, "closed": frozenset(done), "planned_at": planned_at}
