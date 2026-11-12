"""Muse's queue against GitHub (J-097): which jobs in work/tasks/muse.md's "Unstarted" block are really unstarted.

One set of checks for both callers: tools/board/queue_check.py (the CLI the integrator runs before a queue mail)
and the switchboard's sweep (switch.py queue_check: under LOW_UNDER really-unstarted jobs, a QUEUE-LOW inbox item on
#282, Cory 11-11: "you should be making sure muse DOES NOT RUN LOW ON QUEUE at any point"). Each caller hands in its
own GitHub client (ghapi.GitHub, or a test's fake): only `repo`, `get(path)` and `paged(path)` are used, every one a
read. No `gh` or `git` subprocess runs here.

A queue line names a job as `J-NNN #N`. Its verdict, the first that holds:
  DONE      the issue is closed, or a merged PR's body starts `Job: J-NNN`
  OPEN-PR   an open PR's body starts `Job: J-NNN` (work exists; not unstarted)
  BRANCH    a branch names the job (`j0NN`) and its head commit is under RECENT_H old (work exists)
  STARTED   the owner said it started (a bridged Muse post on the issue with "started" in its title line), work
            that is not pushed yet: never take such a job from its owner
  OK        none of the above: the job is really unstarted

Reads per check, each listing once (Reads): the job's issue, the PRs newest first down to the oldest queued job's
issue number (a PR for a job comes after its issue: issues and PRs share one number line), the branch listing, the
head commit of each branch that names a queued job (immutable, so the ETag cache answers it free after the first),
and the comments of an issue still OK after those. Every listing is capped (MAX_PR_PAGES, MAX_BRANCH_PAGES,
MAX_COMMIT_READS); a cap reached is a note, and an unread branch counts as moved (the safe side: a job is counted as
unstarted only when nothing says otherwise).
"""

from __future__ import annotations

import dataclasses
import datetime as dt
import re
import urllib.parse

QUEUE_FILE = "work/tasks/muse.md"
LOW_UNDER = 8          # fewer really-unstarted jobs than this: QUEUE-LOW (Cory 11-03, 11-11)
REFILL_TO = 15
RECENT_H = 72
MAX_PR_PAGES = 5       # PRs read newest first, 100 a page
MAX_BRANCH_PAGES = 10  # 596 branches on 11-11: six pages
MAX_COMMIT_READS = 60  # head commits of branches that name a queued job
JOB_LINE = re.compile(r"\bJ-(\d{3})[a-z]?\s+#(\d+)")
WAITS = re.compile(r"(?:after|on)\s+J-(\d{3})", re.I)
ANY_JOB = re.compile(r"\bJ-(\d{3})[a-z]?(?:\s+#(\d+))?")


@dataclasses.dataclass(frozen=True)
class QueueLine:
    num: str        # "097"
    issue: int
    rest: str       # the line after `J-NNN #N` (where a WAITS clause is read)

    @property
    def job(self) -> str:
        return f"J-{self.num}"


@dataclasses.dataclass(frozen=True)
class Verdict:
    line: QueueLine
    verdict: str                  # DONE, OPEN-PR, BRANCH, STARTED or OK
    branches: tuple[str, ...] = ()

    @property
    def text(self) -> str:
        """The verdict as the CLI prints it: BRANCH names its branches."""
        return "BRANCH " + ",".join(self.branches) if self.verdict == "BRANCH" else self.verdict


@dataclasses.dataclass
class Report:
    verdicts: list[Verdict]
    notes: list[str]

    @property
    def unstarted(self) -> list[Verdict]:
        return [v for v in self.verdicts if v.verdict == "OK"]


def unstarted_block(text: str) -> str:
    """The "Unstarted" block: from the first "Unstarted" to the next blank line ("" when there is none)."""
    start = text.find("Unstarted")
    if start < 0:
        return ""
    end = text.find("\n\n", start)
    return text[start:end if end >= 0 else None]


def queue_lines(text: str) -> list[QueueLine]:
    out = []
    for line in unstarted_block(text).splitlines():
        m = JOB_LINE.search(line)
        if m:
            out.append(QueueLine(m.group(1), int(m.group(2)), line[m.end():]))
    return out


def parse_time(s: str) -> dt.datetime | None:
    try:
        return dt.datetime.fromisoformat((s or "").replace("Z", "+00:00"))
    except ValueError:
        return None


class Reads:
    """One check's reads: each listing once, each issue once."""

    def __init__(self, gh, now: dt.datetime, oldest_issue: int):
        self.gh, self.now, self.oldest_issue = gh, now, oldest_issue
        self.notes: list[str] = []
        self._prs: list[dict] | None = None
        self._branches: list[dict] | None = None
        self._issues: dict[int, dict] = {}
        self._commit_reads = 0

    def issue(self, number: int) -> dict:
        if number not in self._issues:
            self._issues[number] = self.gh.get(f"repos/{self.gh.repo}/issues/{number}") or {}
        return self._issues[number]

    def prs(self) -> list[dict]:
        """PRs newest first, down to the oldest queued job's issue number or MAX_PR_PAGES pages."""
        if self._prs is None:
            got: list[dict] = []
            for page in range(1, MAX_PR_PAGES + 1):
                batch = self.gh.get(f"repos/{self.gh.repo}/pulls?state=all&sort=created&direction=desc"
                                    f"&per_page=100&page={page}") or []
                got.extend(batch)
                if len(batch) < 100 or min(int(p.get("number") or 0) for p in batch) < self.oldest_issue:
                    break
            else:
                self.notes.append(f"the PR listing stopped at its cap ({MAX_PR_PAGES * 100} PRs)")
            self._prs = got
        return self._prs

    def job_prs(self, job: str) -> tuple[bool, bool]:
        """(merged, open) for PRs whose body starts `Job: J-NNN`."""
        merged = opened = False
        starts = re.compile(rf"Job:\s*{re.escape(job)}\b")
        for pr in self.prs():
            if not starts.match((pr.get("body") or "").lstrip()):
                continue
            if pr.get("state") == "open":
                opened = True
            elif pr.get("merged_at"):
                merged = True
        return merged, opened

    def branches(self) -> list[dict]:
        if self._branches is None:
            got: list[dict] = []
            for page in range(1, MAX_BRANCH_PAGES + 1):
                batch = self.gh.get(f"repos/{self.gh.repo}/branches?per_page=100&page={page}") or []
                got.extend(batch)
                if len(batch) < 100:
                    break
            else:
                self.notes.append(f"the branch listing stopped at its cap ({MAX_BRANCH_PAGES * 100} branches)")
            self._branches = got
        return self._branches

    def recent_branches(self, num: str) -> list[str]:
        """Branches naming `j<num>` whose head commit is under RECENT_H old."""
        live = []
        for b in self.branches():
            name = b.get("name") or ""
            if f"j{num}" not in name.lower():
                continue
            sha = (b.get("commit") or {}).get("sha") or ""
            if self._commit_reads >= MAX_COMMIT_READS or not sha:
                self.notes.append(f"{name}: head commit not read (cap {MAX_COMMIT_READS}); counted as moved")
                live.append(name)
                continue
            self._commit_reads += 1
            commit = self.gh.get(f"repos/{self.gh.repo}/commits/{sha}") or {}
            when = parse_time(((commit.get("commit") or {}).get("committer") or {}).get("date") or "")
            if when is None or (self.now - when).total_seconds() < RECENT_H * 3600:
                live.append(name)
        return live

    def comments(self, number: int) -> list[dict]:
        return self.gh.paged(f"repos/{self.gh.repo}/issues/{number}/comments")

    def started(self, number: int) -> bool:
        """A bridged Muse post on the issue whose title line says it started."""
        if not int(self.issue(number).get("comments") or 0):
            return False
        for c in self.comments(number):
            first = (c.get("body") or "").strip().split("\n", 1)[0].lower()
            if first.startswith("**muse (via email bridge)") and "started" in first and "never started" not in first:
                return True
        return False


def verdict_of(r: Reads, q: QueueLine) -> Verdict:
    issue = r.issue(q.issue)
    merged, opened = r.job_prs(q.job)
    if issue.get("state") == "closed" or merged:
        return Verdict(q, "DONE")
    if opened:
        return Verdict(q, "OPEN-PR")
    branches = r.recent_branches(q.num)
    if branches:
        return Verdict(q, "BRANCH", tuple(branches))
    if r.started(q.issue):
        return Verdict(q, "STARTED")
    return Verdict(q, "OK")


def check(gh, text: str, now: dt.datetime | None = None) -> Report:
    """Every job line in the queue text's Unstarted block, with its verdict."""
    now = now or dt.datetime.now(dt.timezone.utc)
    lines = queue_lines(text)
    r = Reads(gh, now, min((q.issue for q in lines), default=0))
    verdicts = []
    for q in lines:
        try:
            verdicts.append(verdict_of(r, q))
        except RuntimeError as err:   # ghapi.GitHubError: a 404 is this line's own (a wrong issue number); else raise
            if getattr(err, "status", 0) != 404 and "HTTP 404" not in str(getattr(err, "detail", err)):
                raise
            r.notes.append(f"{q.job} #{q.issue}: not found on GitHub; not counted as unstarted")
            verdicts.append(Verdict(q, "UNREAD"))
    return Report(verdicts, list(dict.fromkeys(r.notes)))


# ---------- the CLI's extras (tools/board/queue_check.py) ----------
def job_issue(gh, num: str) -> dict | None:
    q = f'repo:{gh.repo} is:issue label:job "J-{num}" in:title'
    got = gh.get(f"search/issues?q={urllib.parse.quote(q, safe='')}") or {}
    for item in got.get("items", []):
        if re.match(rf"J-{num}\b", item.get("title", "")):
            return item
    return None


def blocker_state(gh, num: str) -> str:
    it = job_issue(gh, num)
    if it is None:
        return f"WAITS-ON J-{num} (no issue found)"
    return "READY (J-%s #%s closed)" % (num, it["number"]) if it["state"] == "closed" else \
        f"WAITS-ON J-{num} #{it['number']} (open)"


def newest_comment(gh, issue: str) -> str:
    got = gh.paged(f"repos/{gh.repo}/issues/{issue}/comments")
    people = [c for c in got if not c["user"]["login"].endswith("[bot]")]
    if not people:
        return "none"
    c = people[-1]
    first = (c.get("body") or "").strip().split("\n", 1)[0][:70]
    return f"{c['created_at']} {first}"


def parked(text: str) -> list[tuple[str, str]]:
    """(NNN, issue) for every job with an issue on a "Parked:" line."""
    out = []
    for line in text.splitlines():
        if "Parked:" in line:
            out += [(pj.group(1), pj.group(2)) for pj in ANY_JOB.finditer(line.split("Parked:", 1)[1]) if pj.group(2)]
    return out


# ---------- owners (11-12): the queue, the issue's owner: label and the switchboard must agree ----------
MUSE_LABEL = "owner:muse"
IN_FLIGHT = "Finish (in flight):"
TITLE_JOB = re.compile(r"J-\d{3}\b")


def told_lines(text: str) -> list[QueueLine]:
    """Every `J-NNN #N` Muse is told to work: its "Finish (in flight):" line, then the Unstarted block."""
    out = []
    for line in text.splitlines():
        if line.startswith(IN_FLIGHT):
            out += [QueueLine(m.group(1), int(m.group(2)), "") for m in JOB_LINE.finditer(line)]
    return out + queue_lines(text)


def owner_mismatches(gh, text: str) -> list[str]:
    """Jobs Muse's queue tells it to work whose open issue is not labelled owner:muse. 11-12: J-109 and J-038 were
    taken by Claude's helpers by mail while the queue and the labels still said Muse's, and Muse built J-109 twice."""
    out = []
    for q in told_lines(text):
        issue = gh.get(f"repos/{gh.repo}/issues/{q.issue}") or {}
        if "pull_request" in issue:   # the queue named the job's PR: its issue is found by the job's title
            issue = job_issue(gh, q.num) or {}
        if not issue or issue.get("state") == "closed":
            continue
        labels = [lab.get("name", "") for lab in issue.get("labels") or []]
        if MUSE_LABEL not in labels:
            owner = next((lab for lab in labels if lab.startswith("owner:")), "no owner: label")
            out.append(f"{q.job} #{q.issue}: Muse's queue lists it; its issue says {owner}")
    return out


def unlabeled(issues: list[dict]) -> list[str]:
    """Open issues titled `J-NNN` with no `job` label: the switchboard reads only labelled jobs, so no stuck-work
    alarm can see them (11-12: J-105 to J-128 were created bare and invisible)."""
    return [f"#{i['number']} {(i.get('title') or '')[:60]}" for i in issues
            if "pull_request" not in i and TITLE_JOB.match(i.get("title") or "")
            and "job" not in {lab.get("name") for lab in i.get("labels") or []}]
