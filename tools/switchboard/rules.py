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
# "Supersedes #43", "supersedes #43 and #44", "Supersedes #43, #44": the PRs a PR body says it replaces.
SUPERSEDES = re.compile(r"\bsupersedes\s+(?P<list>#\d+(?:\s*(?:,|and|&)\s*#\d+)*)", re.IGNORECASE)
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
    pool: Sequence[str] = ()               # review.pool: who shares the reviews, one per PR (API_REVIEWER for the API)
    api_reviews_claude: bool = False       # the automatic review also takes Claude's PRs, and its LAND counts there

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
    pool = list(review.get("pool", []))
    api_claude = review.get("api_reviews_claude", False)
    if not isinstance(api_claude, bool):
        raise ValueError("agents.yml: review.api_reviews_claude must be true or false")
    for p in pool:
        if p != API_REVIEWER and p not in agents:
            raise ValueError(f"agents.yml: review.pool names unknown agent {p!r}")
        if p != API_REVIEWER and agent_vendor[p] == "anthropic":
            raise ValueError(f"agents.yml: review.pool names {p!r}, a Claude agent: the Claude API reviews as {API_REVIEWER!r}")
    if len(set(pool)) != len(pool):
        raise ValueError("agents.yml: review.pool names an agent twice")
    fallback = list(review.get("fallback_order", []))
    for f in fallback:
        if f not in agents:
            raise ValueError(f"agents.yml: review.fallback_order names unknown agent {f!r}")
        if agent_vendor[f] == "anthropic":
            raise ValueError(f"agents.yml: review.fallback_order names {f!r}, a Claude agent: Claude's work is never reviewed by Claude")
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
                tuple(pool), api_claude)


def _has_repro(body: str) -> bool:
    found = {m.group(1).lower() for m in REPRO_LINE.finditer(body)}
    return all(f in found for f in REPRO_FIELDS)


CODEX_LOGIN = "chatgpt-codex-connector[bot]"   # the Codex GitHub app: reviews every PR on open, and on "@codex review"
_CODEX_STATE = re.compile(r"codex-review-state:v1\s+(\{.*?\})\s*-->", re.DOTALL)


def parse_codex_state(body: str, created_at: str, author_login: str) -> Verdict | None:
    """The Codex app's summary comment -> its verdict on the commit it reviewed (Cory, 11-03: "If Codex
    is the one that's available, great"). Only that app's own login counts. A finished review with no
    unresolved findings is a LAND; one with unresolved findings is a FIX (it holds the merge until they
    are resolved or a later Codex pass is clean); a running or unknown state is no verdict."""
    review = codex_review_state(body, author_login)
    if review is None:
        return None
    sha = str(review.get("commit_sha") or "")
    if review.get("status") != "completed" or not re.fullmatch(r"[0-9a-f]{7,40}", sha):
        return None
    unresolved = review.get("has_unresolved_findings")
    if unresolved is None:
        return None
    return Verdict("openai", "Codex", "review", "FIX" if unresolved else "LAND", sha, created_at, author_login, False)


def codex_review_state(body: str, author_login: str) -> dict | None:
    """The `code_review` part of the Codex app's state comment, or None when the comment is not one."""
    import html
    import json
    if author_login.strip().lower() != CODEX_LOGIN:
        return None
    m = _CODEX_STATE.search(html.unescape(body))   # the app writes its braces as &#123; &#125;
    if not m:
        return None
    try:
        review = json.loads(m.group(1)).get("code_review") or {}
    except (ValueError, AttributeError):
        return None
    return review if isinstance(review, dict) else None


CODEX_WAIT_MINUTES = 30   # how long after the proof went green a review Codex is still running is waited for


def codex_in_flight(comments: Iterable[Mapping], reactions: Iterable[Mapping], head_sha: str) -> bool:
    """Is Codex reviewing this head right now: its state comment names the head with a status other than
    completed, or it has reacted 👀 on the PR (it does so when it starts, and takes it off when done) while no
    completed state names the head."""
    done = False
    for c in comments:
        r = codex_review_state(c.get("body") or "", (c.get("user") or {}).get("login", ""))
        sha = str((r or {}).get("commit_sha") or "")
        if r is None or not sha or not head_sha.startswith(sha):
            continue
        if r.get("status") == "completed":
            done = True
        elif r.get("status"):
            return True
    eyes = any(x.get("content") == "eyes" and (x.get("user") or {}).get("login", "").lower() == CODEX_LOGIN
               for x in reactions)
    return eyes and not done


def codex_wait_over(proof_done_at: str, now_iso: str, minutes: float = CODEX_WAIT_MINUTES) -> bool:
    """The wait for a running Codex review is over: `minutes` since the proof went green, or no time to count from."""
    return not proof_done_at or ask_is_overdue(proof_done_at, now_iso, minutes / 60)


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
    never_edit: bool = False        # the rule check BLOCKs a never-edited file (protected-file): nothing lifts it


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


def is_api_review(v: Verdict) -> bool:
    """The automatic Claude review through the API: a review posted by the workflow bot as Claude. A Claude
    verdict any agent posts by hand (every agent shares one login) is not it."""
    return v.kind == "review" and v.author_login == BOT_LOGIN and v.vendor == "anthropic"


def api_review_on(verdicts: Iterable[Verdict], head_sha: str) -> Verdict | None:
    """The newest automatic Claude review naming this head, or None when it has none."""
    on_head = [v for v in verdicts if is_api_review(v) and names_head(v, head_sha)]
    return max(on_head, key=lambda v: v.created_at) if on_head else None


def land_counts(v: Verdict, author_vendor: str, team: Team | None = None) -> bool:
    """May this verdict be the LAND a PR needs: a known vendor other than the author's, or the automatic
    Claude review (posted by the workflow bot) when review.api_reviews_claude lets it review Claude's work
    (Cory, 11-03: "give it real work, the Claude on GitHub actions"; the Codex app still reviews every PR
    on open and its findings still block)."""
    if v.vendor not in ("unknown", author_vendor):
        return True
    return bool(team and team.api_reviews_claude and is_api_review(v))


def merge_decision(pr: PrFacts, author_vendor: str, team: Team | None = None,
                   merge_token: bool = False) -> MergeDecision:
    """Design v3 section 2.3 step 6 with the owner's corrections. All must hold to MERGE:

    proof green on the head; one LAND naming the head from a known vendor other than the author's;
    no open FIX (a FIX without a repro is a NOTE); no rule_check BLOCK, unless the branch is a declared
    protected change (`<agent>/integrate-protected-<job>`), which merges on the same green proof and LAND
    (Cory, 11-04: merging should not need him or the integrator); the eye's LAND on the head when the job
    needs eyes.

    A PR that changes .github/workflows (J-056): the workflow token cannot merge it, so it is HUMAN-MERGE
    unless `merge_token` (the caller holds SWITCHBOARD_MERGE_TOKEN) and, on top of every gate above, the
    security pass is clean: the automatic Claude review on this exact head exists and is not FIX or BLOCK
    (a FIX or BLOCK without a repro counts here too). Its LAND must also come from a vendor other than the
    author's in fact: the API's LAND on Claude's own work (api_reviews_claude) does not count for these.
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
             and land_counts(v, author_vendor, team)]
    if not lands:
        waits.append(f"no LAND on @{pr.head_sha[:8]} from a vendor other than {author_vendor}")
    if "eyes" in pr.needs:
        eye_lands = [v for v in pr.verdicts if v.kind == "eye" and v.verdict == "LAND"
                     and names_head(v, pr.head_sha) and v.author_login == BOT_LOGIN]
        if not eye_lands:
            waits.append("needs:eyes and no eye LAND on this head")
    if pr.rule_check_block is None:
        waits.append("rule_check did not run")
    elif pr.never_edit:
        waits.append("rule_check BLOCK protected-file: a file that is never edited is changed")
    elif pr.rule_check_block and "/integrate-protected-" not in pr.head_ref:
        waits.append("rule_check BLOCK on a branch not named <agent>/integrate-protected-<job>")
    if pr.mergeable is False:
        waits.append("merge conflict with the base")
    elif pr.mergeable is None:
        waits.append("GitHub is still computing mergeability")
    if waits:
        return MergeDecision("WAIT", tuple(waits))
    land_by = lands[-1].who
    if pr.touches_workflows:
        return workflow_merge(pr, author_vendor, lands, merge_token)
    return MergeDecision("MERGE", (f"proof green, LAND by {land_by} on @{pr.head_sha[:8]}",), land_by)


def workflow_merge(pr: PrFacts, author_vendor: str, lands: Sequence[Verdict], merge_token: bool) -> MergeDecision:
    """The end of merge_decision for a ready PR that changes .github/workflows (J-056): MERGE with the merge
    token only after a clean security pass on the head, else HUMAN-MERGE as before the token, saying why."""
    sha, land_by = pr.head_sha[:8], lands[-1].who
    if not merge_token:
        return MergeDecision("HUMAN-MERGE", ("ready, but it changes .github/workflows; the workflow token cannot merge it",), land_by)
    api = api_review_on(pr.verdicts, pr.head_sha)
    cross = [v for v in lands if v.vendor not in ("unknown", author_vendor)]
    why = ("no automatic Claude review on this head (the security pass)" if api is None else
           "the automatic Claude review on this head is FIX or BLOCK (the security pass)" if api.verdict == "FIX" or api.downgraded else
           f"no LAND from a vendor other than {author_vendor} (the API's own LAND does not count here)" if not cross else "")
    if why:
        return MergeDecision("HUMAN-MERGE", (f"ready, but it changes .github/workflows: {why}",), land_by)
    return MergeDecision("MERGE", (f"proof green, LAND by {cross[-1].who} and a clean automatic Claude review ({api.verdict}) "
                                   f"on @{sha}; it changes .github/workflows, so the merge token merges it",), cross[-1].who)


@dataclasses.dataclass(frozen=True)
class ReviewPick:
    route: str     # "claude-api" (automatic, budgeted) or "ask" (a human-driven app)
    agent: str     # agent key asked, "" for claude-api


def pick_reviewer(author_vendor: str, needs: frozenset[str], team: Team) -> ReviewPick:
    """Who reviews a PR, by its author's vendor.

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


API_REVIEWER = "claude-api"   # the automatic Claude review through the API, as a member of review.pool


def review_rotation(number: int, author_vendor: str, needs: frozenset[str], team: Team,
                    api_ok: bool = True) -> list[str]:
    """Who may review PR `number`, one of them per PR (Cory, 11-03: "get the ones that are available and
    ready to do the reviews ... if we spread the work out, the merrier ... I'm not asking for two agents
    to review the same shit"); choose_reviewer picks the free one.

    The pool members whose vendor is not the author's, the Claude API only for work not by Claude
    (reviews stay cross-model) and only while the budget allows a call (`api_ok`). The start rotates
    with the PR number, so consecutive PRs go to different reviewers; a job that needs matron starts
    at review.matron_reviewer, its specialty. An unknown author gets the API alone, as before."""
    if author_vendor == "unknown":
        return [API_REVIEWER] if api_ok else []
    fits = [p for p in team.pool
            if (p == API_REVIEWER and api_ok and (author_vendor != "anthropic" or team.api_reviews_claude))
            or (p != API_REVIEWER and team.agent_vendor.get(p) not in (None, "anthropic", author_vendor))]
    # The last resort (Cory, 11-04): when the API is in the pool but cannot review this head (a cap is hit, or
    # it failed or gave no verdict here), the agents of review.fallback_order follow, by their own scheduled runs.
    last = [f for f in team.fallback_order
            if not api_ok and API_REVIEWER in team.pool and f not in fits
            and team.agent_vendor.get(f) not in (None, "anthropic", author_vendor)]
    if not fits:
        return last
    start = number % len(fits)
    if "matron" in needs and team.matron_reviewer in fits:
        start = fits.index(team.matron_reviewer)
    return fits[start:] + fits[:start] + last


def choose_reviewer(rotation: Sequence[str], asked_at: Mapping[str, str], now_iso: str, hours: float,
                    outstanding: Mapping[str, int], recent: Mapping[str, int]) -> str:
    """The one reviewer this head gets (Cory, 11-03: "it should be available to take. Using any agent
    available"). Someone already asked on this head, less than `hours` ago, keeps it. Otherwise the
    free one: fewest reviews still unanswered on other PRs (`outstanding`; the API never has any), then
    fewest asked in the last day (`recent`, so the work spreads), then the rotation's order. Anyone asked
    on this head who let `hours` pass is skipped. "" when nobody is left."""
    for who in rotation:
        if who in asked_at and who != API_REVIEWER and not ask_is_overdue(asked_at[who], now_iso, hours):
            return who
    free = [w for w in rotation if w == API_REVIEWER or w not in asked_at]
    if not free:
        return ""
    return min(free, key=lambda w: (outstanding.get(w, 0), recent.get(w, 0), rotation.index(w)))


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


def needs_review(pr: PrFacts, author_vendor: str = "", team: Team | None = None) -> bool:
    """Proof green and no LAND, FIX or NOTE yet on this head from a review that can count: a verdict by the
    author's own vendor (say the Codex app's unasked review of a ChatGPT PR) never answers the ask, since
    merge_decision would not count its LAND. Its FIX still holds the merge back there."""
    if pr.proof != "green" or pr.draft or pr.base_ref != TRUNK:
        return False
    return not any(v.kind == "review" and names_head(v, pr.head_sha)
                   and not (author_vendor and v.vendor == author_vendor and not land_counts(v, author_vendor, team))
                   for v in pr.verdicts)


def needs_eye(pr: PrFacts) -> bool:
    if "eyes" not in pr.needs or pr.proof != "green" or pr.draft or pr.base_ref != TRUNK:
        return False
    return not any(v.kind == "eye" and names_head(v, pr.head_sha) and v.author_login == BOT_LOGIN
                   for v in pr.verdicts)


def needs_security_pass(pr: PrFacts) -> bool:
    """A green PR that changes .github/workflows, with no open FIX and no automatic Claude review on its head
    (J-056): the switch merges it only after that review, so the ask part runs it even when Codex already
    reviewed the head and no other review is needed."""
    if not pr.touches_workflows or pr.proof != "green" or pr.draft or pr.base_ref != TRUNK:
        return False
    if open_fixes(pr.verdicts, pr.head_sha, pr.head_committed_at, "review"):
        return False
    return api_review_on(pr.verdicts, pr.head_sha) is None


def extra_owner_labels(labels_in_order: Sequence[str]) -> list[str]:
    """Labels to remove so an issue keeps exactly its first owner:* label (design 2.3 step 2)."""
    owners = [l for l in labels_in_order if l.startswith("owner:")]
    return owners[1:]


def job_of_pr(body: str | None) -> str:
    m = JOB_IN_PR.search(body or "")
    return m.group("job") if m else ""


def superseded_prs(body: str | None, own: int) -> list[int]:
    """The PR numbers a PR's body says it supersedes, in order, without repeats or itself."""
    out: list[int] = []
    for m in SUPERSEDES.finditer(body or ""):
        for n in re.findall(r"#(\d+)", m.group("list")):
            if int(n) != own and int(n) not in out:
                out.append(int(n))
    return out


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
