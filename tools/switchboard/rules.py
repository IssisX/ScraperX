"""The switchboard's decisions, as pure functions: no network, no clock, no files.

Everything the switch decides (verdict parsing, the merge rule, the reviewer pick, the owner-label
rule) lives here so the unit tests can pin it. The callers in switch.py fetch the facts and act.
"""

from __future__ import annotations

import dataclasses
import re
from typing import Iterable, Mapping, Sequence

TRUNK = "ScraperX-Claude"
BOT_LOGIN = "github-actions[bot]"

# The first line of a verdict comment or review, as the team writes it today:
#   **ChatGPT (review of #88 @ f9575008): LAND**
#   **Claude eye (look of #100 @ 1a2b3c4d): FIX**
VERDICT_HEAD = re.compile(
    r"^\s*\*\*\s*(?P<who>[A-Za-z][A-Za-z0-9_-]*)(?P<mid>[^\n]*?):\s*\**\s*(?P<verdict>LAND|FIX|NOTE|BLOCK)\b"
)
SHA_IN_HEAD = re.compile(r"@\s*`?(?P<sha>[0-9a-f]{7,40})\b")
REPRO_FIELDS = ("command", "input", "expected", "observed")
REPRO_LINE = re.compile(r"^[\s>*_`-]*(command|input|expected|observed)[*_`]*\s*:", re.IGNORECASE | re.MULTILINE)
JOB_IN_PR = re.compile(r"^\s*Job:\s*(?P<job>J-\d{3,})\b", re.MULTILINE)
JOB_TITLE = re.compile(r"^(?P<job>J-\d{3,})\b")
BUDGET_LINE = re.compile(r"^\s*budget\s+(?P<usd>\d{1,4}(?:\.\d{1,2})?)\s*$", re.IGNORECASE | re.MULTILINE)


@dataclasses.dataclass(frozen=True)
class Verdict:
    vendor: str          # "anthropic", "openai", ... or "unknown"
    who: str             # the name as written
    kind: str            # "review" or "eye"
    verdict: str         # "LAND", "FIX" or "NOTE" (BLOCK reads as FIX; FIX without a repro reads as NOTE)
    sha: str             # the head named in the verdict, "" when none
    created_at: str      # ISO 8601 UTC, as GitHub reports it
    author_login: str    # the GitHub login that posted it
    downgraded: bool     # a FIX or BLOCK without a deterministic repro


@dataclasses.dataclass(frozen=True)
class Team:
    """work/agents.yml, parsed. vendor_of_name and vendor_of_prefix map to vendors."""

    vendor_of_name: Mapping[str, str]
    vendor_of_prefix: Sequence[tuple[str, str]]
    owner_label_vendor: Mapping[str, str]
    human_reviewers: Sequence[str]   # agent keys asked to review Claude-authored work, in order
    matron_reviewer: str             # agent key preferred when the job needs matron
    agent_vendor: Mapping[str, str]  # agent key -> vendor
    verdict_logins: frozenset[str]   # lowercased GitHub logins whose comments may carry a verdict
    agent_name: Mapping[str, str] = dataclasses.field(default_factory=dict)   # agent key -> the name it signs verdicts with
    fallback_order: Sequence[str] = ()     # agent keys asked, in turn, when an ask goes unanswered
    fallback_after_hours: float = 3.0      # an ask with no verdict this long is passed to the next agent
    other_authored_asks: Sequence[str] = ()  # agent keys asked, in order, to review work not by Claude
    api_alongside: bool = False            # the automatic Claude review runs on every PR, beside the ask

    def vendor_of_branch(self, ref: str) -> str:
        for prefix, vendor in self.vendor_of_prefix:
            if ref.startswith(prefix):
                return vendor
        return "unknown"

    def vendor_of_who(self, who: str) -> str:
        return self.vendor_of_name.get(who.lower(), "unknown")


def team_from_config(cfg: Mapping) -> Team:
    """Parse work/agents.yml; raises ValueError naming the key that is wrong."""
    agents = cfg.get("agents")
    if not isinstance(agents, Mapping) or not agents:
        raise ValueError("agents.yml: `agents` must be a non-empty mapping")
    names: dict[str, str] = {}
    prefixes: list[tuple[str, str]] = []
    owners: dict[str, str] = {}
    agent_vendor: dict[str, str] = {}
    for key, a in agents.items():
        vendor = a.get("vendor") if isinstance(a, Mapping) else None
        if not isinstance(vendor, str) or not vendor:
            raise ValueError(f"agents.yml: agent {key!r} needs a `vendor`")
        agent_vendor[key] = vendor
        for n in a.get("names", []):
            names[str(n).lower()] = vendor
        for p in a.get("branch_prefixes", []):
            prefixes.append((str(p), vendor))
        for o in a.get("owner_labels", []):
            owners[str(o)] = vendor
    review = cfg.get("review", {})
    humans = list(review.get("claude_authored_asks", []))
    for h in humans + [review.get("matron_reviewer", "")]:
        if h and h not in agents:
            raise ValueError(f"agents.yml: review names unknown agent {h!r}")
    if not humans:
        raise ValueError("agents.yml: review.claude_authored_asks must name at least one agent")
    logins = cfg.get("verdict_logins")
    if not isinstance(logins, Sequence) or isinstance(logins, str) or not logins:
        raise ValueError("agents.yml: `verdict_logins` must list the GitHub logins whose verdicts count")
    others = list(review.get("other_authored_asks", []))
    for o in others:
        if o not in agents:
            raise ValueError(f"agents.yml: review.other_authored_asks names unknown agent {o!r}")
        if agent_vendor[o] == "anthropic":
            raise ValueError(f"agents.yml: review.other_authored_asks names {o!r}, a Claude agent: the paid "
                             "Claude review is the fallback when no one listed here can take it")
    fallback = list(review.get("fallback_order", []))
    for f in fallback:
        if f not in agents:
            raise ValueError(f"agents.yml: review.fallback_order names unknown agent {f!r}")
        if agent_vendor[f] == "anthropic":
            raise ValueError(f"agents.yml: review.fallback_order names {f!r}, a Claude agent: Claude's work is never reviewed by Claude")
    alongside = review.get("claude_api_alongside", False)
    if not isinstance(alongside, bool):
        raise ValueError("agents.yml: review.claude_api_alongside must be true or false")
    try:
        hours = float(review.get("fallback_after_hours", 3))
    except (TypeError, ValueError):
        hours = 0.0
    if not 0 < hours < 24 * 14:
        raise ValueError("agents.yml: review.fallback_after_hours must be a number of hours above 0 and under 336")
    signs = {key: str((a.get("names") or [key])[0]) for key, a in agents.items()}
    prefixes.sort(key=lambda pv: -len(pv[0]))  # longest prefix wins
    return Team(names, prefixes, owners, humans, review.get("matron_reviewer", ""), agent_vendor,
                frozenset(str(l).strip().lower() for l in logins), signs, tuple(fallback), hours, tuple(others),
                alongside)


def _has_repro(body: str) -> bool:
    found = {m.group(1).lower() for m in REPRO_LINE.finditer(body)}
    return all(f in found for f in REPRO_FIELDS)


def parse_verdict(body: str, created_at: str, author_login: str, team: Team) -> Verdict | None:
    """One comment or review body -> its verdict, or None when its first line is not a verdict.

    Only the first line is read, so a quoted verdict further down never counts. The repo is public,
    so a comment from a login outside verdict_logins is never a verdict: anyone can comment.
    """
    if author_login.strip().lower() not in team.verdict_logins:
        return None
    first = body.lstrip().split("\n", 1)[0]
    m = VERDICT_HEAD.match(first)
    if not m:
        return None
    who, mid, raw = m.group("who"), m.group("mid"), m.group("verdict")
    kind = "eye" if re.match(r"^\s*eye\b", mid, re.IGNORECASE) else "review"
    sha_m = SHA_IN_HEAD.search(mid)
    verdict = "FIX" if raw == "BLOCK" else raw
    downgraded = False
    if verdict == "FIX" and not _has_repro(body):
        verdict, downgraded = "NOTE", True
    return Verdict(team.vendor_of_who(who), who, kind, verdict, sha_m.group("sha") if sha_m else "",
                   created_at, author_login, downgraded)


def names_head(v: Verdict, head_sha: str) -> bool:
    return bool(v.sha) and head_sha.startswith(v.sha)


@dataclasses.dataclass(frozen=True)
class PrFacts:
    number: int
    head_sha: str
    head_ref: str
    base_ref: str
    draft: bool
    mergeable: bool | None          # GitHub's answer; None while it is computing
    head_committed_at: str          # ISO 8601 UTC
    proof: str                      # "green", "running", "none" or "red: <jobs>"
    verdicts: Sequence[Verdict]
    needs: frozenset[str]           # needs:* names from the PR and its job issue
    rule_check_block: bool | None   # None when the rule check could not run
    touches_workflows: bool


@dataclasses.dataclass(frozen=True)
class MergeDecision:
    action: str      # MERGE, HUMAN-MERGE, WAIT, FIX, FIX-CI, SKIP
    reasons: tuple[str, ...]
    land_by: str = ""


def open_fixes(verdicts: Iterable[Verdict], head_sha: str, head_time: str, kind: str) -> list[Verdict]:
    """FIX verdicts still open on this head: per vendor, the newest verdict on this head is a FIX.

    A FIX counts on this head when it names it, or names no head and came after the head commit;
    FIX is read generously because it only ever holds a merge back.
    """
    newest: dict[str, Verdict] = {}
    for v in sorted(verdicts, key=lambda x: x.created_at):
        if v.kind != kind:
            continue
        on_head = names_head(v, head_sha) or (not v.sha and v.created_at >= head_time)
        if on_head and v.verdict in ("LAND", "FIX"):
            newest[v.vendor + "/" + v.who.lower()] = v
    return [v for v in newest.values() if v.verdict == "FIX"]


def merge_decision(pr: PrFacts, author_vendor: str) -> MergeDecision:
    """Design v3 section 2.3 step 6 with the owner's corrections. All must hold to MERGE:

    proof green on the head; one LAND naming the head from a known vendor other than the author's;
    no open FIX (a FIX without a repro is a NOTE); no rule_check BLOCK; the eye's LAND on the head
    when the job needs eyes. A PR that changes .github/workflows is HUMAN-MERGE: the workflow token
    cannot merge workflow files.
    """
    if pr.base_ref != TRUNK:
        return MergeDecision("SKIP", (f"stacked on {pr.base_ref}, not {TRUNK}",))
    if pr.draft:
        return MergeDecision("SKIP", ("draft",))
    if pr.proof.startswith("red"):
        return MergeDecision("FIX-CI", (f"proof {pr.proof}",))
    fixes = open_fixes(pr.verdicts, pr.head_sha, pr.head_committed_at, "review")
    eye_fixes = open_fixes(pr.verdicts, pr.head_sha, pr.head_committed_at, "eye") if "eyes" in pr.needs else []
    if fixes or eye_fixes:
        who = ", ".join(sorted({f"{v.who} ({v.kind})" for v in fixes + eye_fixes}))
        return MergeDecision("FIX", (f"open FIX from {who}",))
    waits: list[str] = []
    if pr.proof != "green":
        waits.append(f"proof {pr.proof}")
    lands = [v for v in pr.verdicts
             if v.kind == "review" and v.verdict == "LAND" and names_head(v, pr.head_sha)
             and v.vendor not in ("unknown", author_vendor)]
    if not lands:
        waits.append(f"no LAND on @{pr.head_sha[:8]} from a vendor other than {author_vendor}")
    if "eyes" in pr.needs:
        eye_lands = [v for v in pr.verdicts if v.kind == "eye" and v.verdict == "LAND"
                     and names_head(v, pr.head_sha) and v.author_login == BOT_LOGIN]
        if not eye_lands:
            waits.append("needs:eyes and no eye LAND on this head")
    if pr.rule_check_block is None:
        waits.append("rule_check did not run")
    elif pr.rule_check_block:
        waits.append("rule_check BLOCK: a protected change, the integrator merges it")
    if pr.mergeable is False:
        waits.append("merge conflict with the base")
    elif pr.mergeable is None:
        waits.append("GitHub is still computing mergeability")
    if waits:
        return MergeDecision("WAIT", tuple(waits))
    land_by = lands[-1].who
    if pr.touches_workflows:
        return MergeDecision("HUMAN-MERGE", ("ready, but it changes .github/workflows; the workflow token cannot merge it",), land_by)
    return MergeDecision("MERGE", (f"proof green, LAND by {land_by} on @{pr.head_sha[:8]}",), land_by)


@dataclasses.dataclass(frozen=True)
class ReviewPick:
    route: str     # "claude-api" (automatic, budgeted) or "ask" (a human-driven app)
    agent: str     # agent key asked, "" for claude-api
    api: bool = False  # the automatic Claude review also runs, beside the ask (review.claude_api_alongside)

    def wants_api(self) -> bool:
        return self.route == "claude-api" or self.api


def pick_reviewer(author_vendor: str, needs: frozenset[str], team: Team) -> ReviewPick:
    """Who reviews a PR: the ask below, plus the automatic Claude review beside it when
    review.claude_api_alongside is on (Cory, 11-03: the API credit is free to us, so every PR gets
    Muse and the Claude API at once). On Claude's own work the automatic review's FIX holds the merge
    back but its LAND does not count: a LAND still has to come from another vendor (merge_decision)."""
    pick = _pick_asked(author_vendor, needs, team)
    if pick.route == "ask" and team.api_alongside:
        return dataclasses.replace(pick, api=True)
    return pick


def _pick_asked(author_vendor: str, needs: frozenset[str], team: Team) -> ReviewPick:
    """Who is asked to review a PR, by its author's vendor.

    A PR by another vendor is asked of the first agent in review.other_authored_asks whose vendor is
    not the author's (Cory, 11-03: Muse is always on and free, so it goes first). Only when none
    fits, or the author is unknown, does it get the paid automatic Claude review through the API. A
    Claude-authored PR is asked of ChatGPT or Muse on the job issue: Muse first when the job needs
    matron (its specialty), else the first agent in review.claude_authored_asks.
    """
    if author_vendor != "anthropic":
        if author_vendor != "unknown":
            for agent in team.other_authored_asks:
                if team.agent_vendor.get(agent) not in (None, "anthropic", author_vendor):
                    return ReviewPick("ask", agent)
        return ReviewPick("claude-api", "")
    if "matron" in needs and team.matron_reviewer:
        return ReviewPick("ask", team.matron_reviewer)
    for agent in team.human_reviewers:
        if team.agent_vendor.get(agent) != "anthropic":
            return ReviewPick("ask", agent)
    raise ValueError("agents.yml: no non-Claude agent to ask for a review of Claude's work")


def next_fallback(asked: Sequence[str], author_vendor: str, team: Team) -> str:
    """The next agent to ask for a review of a head: the first of review.fallback_order not yet asked on it
    whose vendor is neither the author's (its LAND would not count) nor Anthropic's. "" when none is left."""
    for agent in team.fallback_order:
        vendor = team.agent_vendor.get(agent)
        if agent not in asked and vendor not in (None, "anthropic", author_vendor):
            return agent
    return ""


def ask_is_overdue(asked_at: str, now_iso: str, hours: float) -> bool:
    """An ask posted at `asked_at` that has had no verdict for `hours` hours (ISO 8601 UTC both)."""
    import datetime as _dt
    then = _dt.datetime.fromisoformat(asked_at.replace("Z", "+00:00"))
    now = _dt.datetime.fromisoformat(now_iso.replace("Z", "+00:00"))
    return (now - then).total_seconds() >= hours * 3600


def needs_review(pr: PrFacts) -> bool:
    """Proof green and no LAND, FIX or NOTE yet on this head from a review by an agent (the automatic
    Claude review, posted by the bot, does not answer the ask: both run at once)."""
    if pr.proof != "green" or pr.draft or pr.base_ref != TRUNK:
        return False
    return not any(v.kind == "review" and names_head(v, pr.head_sha) and v.author_login != BOT_LOGIN
                   for v in pr.verdicts)


def needs_api_review(pr: PrFacts) -> bool:
    """Proof green and no automatic Claude review yet on this head."""
    if pr.proof != "green" or pr.draft or pr.base_ref != TRUNK:
        return False
    return not any(v.kind == "review" and names_head(v, pr.head_sha) and v.author_login == BOT_LOGIN
                   for v in pr.verdicts)


def needs_eye(pr: PrFacts) -> bool:
    if "eyes" not in pr.needs or pr.proof != "green" or pr.draft or pr.base_ref != TRUNK:
        return False
    return not any(v.kind == "eye" and names_head(v, pr.head_sha) and v.author_login == BOT_LOGIN
                   for v in pr.verdicts)


def extra_owner_labels(labels_in_order: Sequence[str]) -> list[str]:
    """Labels to remove so an issue keeps exactly its first owner:* label (design 2.3 step 2)."""
    owners = [l for l in labels_in_order if l.startswith("owner:")]
    return owners[1:]


def job_of_pr(body: str | None) -> str:
    m = JOB_IN_PR.search(body or "")
    return m.group("job") if m else ""


def job_of_title(title: str) -> str:
    m = JOB_TITLE.match(title)
    return m.group("job") if m else ""


def jobs_by_pr_mention(titles: Mapping[str, str]) -> dict[int, str]:
    """PR number -> job, for job issues whose title names `#N` (e.g. "J-012 ... (#100, supersedes #43)").

    The fallback for PRs whose body has no `Job:` line. A PR named by two jobs maps to neither.
    """
    seen: dict[int, set[str]] = {}
    for job, title in titles.items():
        for n in re.findall(r"#(\d{1,6})\b", title):
            seen.setdefault(int(n), set()).add(job)
    return {n: next(iter(jobs)) for n, jobs in seen.items() if len(jobs) == 1}


def is_stale(head_committed_at: str, now_iso: str, days: int) -> bool:
    """A head older than `days` gets no new review or ask (the PR is listed, and can still merge)."""
    import datetime as _dt
    head = _dt.datetime.fromisoformat(head_committed_at.replace("Z", "+00:00"))
    now = _dt.datetime.fromisoformat(now_iso.replace("Z", "+00:00"))
    return (now - head).total_seconds() > days * 86400


def budget_override(comments: Iterable[tuple[str, str]], team: Team) -> str:
    """The newest `budget N` line in the given (body, login) comments (oldest first), as a string, or "".

    Only the team's own logins can raise a job's cap: the repo is public, so anyone can comment, and
    the workflow bot never sets budgets for itself.
    """
    found = ""
    for b, login in comments:
        login = login.strip().lower()
        if login not in team.verdict_logins or login == BOT_LOGIN:
            continue
        for m in BUDGET_LINE.finditer(b):
            found = m.group("usd")
    return found
