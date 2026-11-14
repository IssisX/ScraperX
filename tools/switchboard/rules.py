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
# A job is J-NNN, or J-NNN plus one letter for a sub-job (J-044b): "J-\d{3,}\b" alone never matched J-044b,
# so a lettered job's green branch got no auto-PR and its issue was missing from jobs() (11-09).
JOB_ID = r"J-\d{3,}[a-z]?"
JOB_IN_PR = re.compile(r"^\s*Job:\s*(?P<job>" + JOB_ID + r")\b", re.MULTILINE)
# "Supersedes #43", "supersedes #43 and #44", "Supersedes #43, #44": the PRs a PR body says it replaces.
SUPERSEDES = re.compile(r"\bsupersedes\s+(?P<list>#\d+(?:\s*(?:,|and|&)\s*#\d+)*)", re.IGNORECASE)
JOB_TITLE = re.compile(r"^(?P<job>" + JOB_ID + r")\b")
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
    signed: bool = False  # the switch found this automatic review's MAC under SWITCHBOARD_REVIEW_KEY (J-056)
    receipt: bool = False  # the Codex app's clean receipt (parse_codex_receipt): a LAND only, never clears a FIX


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
    manual: frozenset[str] = frozenset()   # agents with `wake: manual` (they run only when Cory starts them): never asked
    agent_of_name: Mapping[str, str] = dataclasses.field(default_factory=dict)   # lowercased name -> agent key

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
    keys: dict[str, str] = {}
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
            keys[str(n).lower()] = key
        for p in a.get("branch_prefixes", []):
            prefixes.append((str(p), vendor))
        for o in a.get("owner_labels", []):
            owners[str(o)] = vendor
    review = cfg.get("review", {})
    # J-072: an agent with `wake: manual` runs only when Cory starts it, so no review is ever routed to it: it is
    # dropped from every ask list, the pool and the fallback order, and a review waits on Codex, the API or Grok.
    manual = frozenset(k for k, a in agents.items() if isinstance(a, Mapping) and a.get("wake") == "manual")
    humans = list(review.get("claude_authored_asks", []))
    for h in humans + [review.get("matron_reviewer", "")]:
        if h and h not in agents:
            raise ValueError(f"agents.yml: review names unknown agent {h!r}")
    humans = [h for h in humans if h not in manual]
    if not humans:
        raise ValueError("agents.yml: review.claude_authored_asks must name at least one agent that is not `wake: manual`")
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
    others = [o for o in others if o not in manual]
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
    pool = [p for p in pool if p not in manual]
    fallback = list(review.get("fallback_order", []))
    for f in fallback:
        if f not in agents:
            raise ValueError(f"agents.yml: review.fallback_order names unknown agent {f!r}")
        if agent_vendor[f] == "anthropic":
            raise ValueError(f"agents.yml: review.fallback_order names {f!r}, a Claude agent: Claude's work is never reviewed by Claude")
    fallback = [f for f in fallback if f not in manual]
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
                tuple(pool), api_claude, manual, keys)


def _has_repro(body: str) -> bool:
    found = {m.group(1).lower() for m in REPRO_LINE.finditer(body)}
    return all(f in found for f in REPRO_FIELDS)


CONFLICT = "merge conflict with the base"   # the reason merge_decision gives; the switch and the reconciler match it
CODEX_LOGIN = "chatgpt-codex-connector[bot]"   # the Codex GitHub app: reviews a PR a person opens, and on a person's "@codex review"
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


_CODEX_RECEIPT = "<!-- codex-review-receipt:v1 -->"
_CODEX_CLEAN = re.compile(r"^\s*Codex Review:\s*Didn['’]t find any major issues\b", re.MULTILINE)
_CODEX_REVIEWED = re.compile(r"\*\*Reviewed commit:\*\*\s*`(?P<sha>[0-9a-f]{7,40})`")


def parse_codex_receipt(body: str, created_at: str, author_login: str) -> Verdict | None:
    """The Codex app's receipt comment ("Codex Review: Didn't find any major issues", "Reviewed commit: `<sha>`")
    -> a LAND on the commit it names (11-12, #343 and #453: a clean pass can come only as a receipt, with no fresh
    state comment for the head). Only that app's own login, only the clean wording, only one named commit; the
    LAND counts only where that prefix starts the PR's head (names_head), so a receipt for an older head counts
    for nothing. It names a 10-character prefix, so it never stands in for a workflow-file PR's whole-sha review
    (codex_review_on). As a `receipt` it never clears a FIX (open_fixes): the state comment's unresolved findings
    on the same head still hold the merge (#343 @ b1177151: a clean receipt beside an open P1 thread)."""
    if author_login.strip().lower() != CODEX_LOGIN or _CODEX_RECEIPT not in body or not _CODEX_CLEAN.search(body):
        return None
    shas = {m.group("sha") for m in _CODEX_REVIEWED.finditer(body)}
    if len(shas) != 1:
        return None
    return Verdict("openai", "Codex", "review", "LAND", shas.pop(), created_at, author_login, False, receipt=True)


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
        if r.get("status") in ("completed", "failed"):
            done = True   # a failed review (its usage limit, 11-10 #343) is over, not running
        elif r.get("status"):
            return True
    eyes = any(x.get("content") == "eyes" and (x.get("user") or {}).get("login", "").lower() == CODEX_LOGIN
               for x in reactions)
    return eyes and not done


def codex_failed(comments: Iterable[Mapping], head_sha: str) -> bool:
    """Codex's newest state for this head says its review failed ("We couldn't complete this review"; 11-10 #343:
    its usage limit). Its turn on the head is over: the review goes on as if Codex were not there."""
    last = None
    for c in comments:
        r = codex_review_state(c.get("body") or "", (c.get("user") or {}).get("login", ""))
        sha = str((r or {}).get("commit_sha") or "")
        if r is not None and sha and head_sha.startswith(sha) and r.get("status"):
            last = r.get("status")
    return last == "failed"


def codex_wait_over(proof_done_at: str, now_iso: str, minutes: float = CODEX_WAIT_MINUTES) -> bool:
    """The wait for a running Codex review is over: `minutes` since the proof went green, or no time to count from."""
    return not proof_done_at or ask_is_overdue(proof_done_at, now_iso, minutes / 60)


CODEX_START_MINUTES = 10   # how long Codex has to start on a head (its 👀) once the head is green and it is asked


def codex_turn(since: Iterable[str], in_flight: bool, expected: bool, now_iso: str) -> bool:
    """Is it still Codex's turn on a green head, so that no one else is asked and the paid review waits (one
    review per head; Cory, 11-04: Codex first, the API only for a head Codex has not reviewed)?

    `since`: when the head's proof went green, when its PR opened and when Codex was asked on it (ISO 8601 UTC,
    "" for none); the clock runs from the latest. While Codex is `expected` (its review can count here, so the
    switch asks it once per head), it has CODEX_START_MINUTES to start. A review that is running (`in_flight`)
    is waited for until CODEX_WAIT_MINUTES. Past that the review goes on as if Codex were not there."""
    start = max((t for t in since if t), default="")
    if not start:
        return False
    if expected and not ask_is_overdue(start, now_iso, CODEX_START_MINUTES / 60):
        return True
    return in_flight and not ask_is_overdue(start, now_iso, CODEX_WAIT_MINUTES / 60)


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


def names_whole_head(v: Verdict, head_sha: str) -> bool:
    """The verdict names all 40 characters of this head (J-056). A prefix can be ground: a new commit whose sha
    starts with an already-reviewed head's 8 characters takes a few billion tries, so the gates of a PR that
    changes .github/workflows never read one."""
    return len(v.sha) == 40 and v.sha == head_sha


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
    codex_failed: bool = False      # the Codex app's newest state for this head is "failed" (its usage limit)


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
        if v.kind != kind or v.receipt:   # a clean receipt adds a LAND; only the state comment clears its FIX
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
    """The newest signed automatic Claude review naming this whole head, or None when it has none (a
    workflow-file PR's security pass, J-056: an old 8-character mark is not one). Signed: any branch's push
    workflow posts as the workflow bot too, so only the MAC under SWITCHBOARD_REVIEW_KEY tells the real one."""
    on_head = [v for v in verdicts if is_api_review(v) and v.signed and names_whole_head(v, head_sha)]
    return max(on_head, key=lambda v: v.created_at) if on_head else None


def is_codex_app(v: Verdict) -> bool:
    """A verdict the Codex app posted under its own login (its review-state comment, rules.parse_codex_state).
    No agent can post as that login, so a workflow-file PR takes its cross-vendor LAND from it alone (J-056)."""
    return v.kind == "review" and v.author_login.strip().lower() == CODEX_LOGIN and v.who == "Codex" \
        and v.vendor == "openai"


def codex_review_on(verdicts: Iterable[Verdict], head_sha: str) -> Verdict | None:
    """The Codex app's newest verdict naming this whole head, or None when it has none."""
    on_head = [v for v in verdicts if is_codex_app(v) and not v.receipt and names_whole_head(v, head_sha)]
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
                   merge_token: bool = False, review_key: bool = False) -> MergeDecision:
    """Design v3 section 2.3 step 6 with the owner's corrections. All must hold to MERGE:

    proof green on the head; one LAND naming the head from a known vendor other than the author's;
    no open FIX (a FIX without a repro is a NOTE); no rule_check BLOCK, unless the branch is a declared
    protected change (`<agent>/integrate-protected-<job>`), which merges on the same green proof and LAND
    (Cory, 11-04: merging should not need him or the integrator); the eye's LAND on the head when the job
    needs eyes.

    A PR that changes .github/workflows lands like any other PR (SYSTEM_PLAN 4.1, 11-13): the same gates above,
    then MERGE with the merge token, since GitHub refuses the workflow token there; HUMAN-MERGE only when no
    merge token is set (`merge_token`). `review_key` is no longer read here (its callers go in the D1 cleanup).
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
        # Any team login's eye LAND naming the head counts, a hand look included, not only the workflow bot's
        # (SYSTEM_PLAN item 8); parse_verdict already drops a login outside verdict_logins.
        eye_lands = [v for v in pr.verdicts if v.kind == "eye" and v.verdict == "LAND"
                     and names_head(v, pr.head_sha)]
        if not eye_lands:
            waits.append("needs:eyes and no eye LAND on this head")
    if pr.rule_check_block is None:
        waits.append("rule_check did not run")
    elif pr.never_edit:
        waits.append("rule_check BLOCK protected-file: a file that is never edited is changed")
    elif pr.rule_check_block and "/integrate-protected-" not in pr.head_ref:
        waits.append("rule_check BLOCK on a branch not named <agent>/integrate-protected-<job>")
    if pr.mergeable is False:
        waits.append(CONFLICT)
    elif pr.mergeable is None:
        waits.append("GitHub is still computing mergeability")
    if waits:
        return MergeDecision("WAIT", tuple(waits))
    land_by = lands[-1].who
    if pr.touches_workflows:
        return workflow_merge(pr, lands, merge_token)
    return MergeDecision("MERGE", (f"proof green, LAND by {land_by} on @{pr.head_sha[:8]}",), land_by)


def workflow_merge(pr: PrFacts, lands: Sequence[Verdict], merge_token: bool) -> MergeDecision:
    """The end of merge_decision for a ready PR that changes .github/workflows: MERGE with the merge token (the
    workflow token cannot merge it), else HUMAN-MERGE, saying why. Nothing more than any other PR needs
    (SYSTEM_PLAN 4.1 and section A, 11-13)."""
    sha, land_by = pr.head_sha[:8], lands[-1].who
    if not merge_token:
        return MergeDecision("HUMAN-MERGE", ("ready, but it changes .github/workflows; the workflow token cannot merge it",), land_by)
    return MergeDecision("MERGE", (f"proof green, LAND by {land_by} on @{sha}; it changes .github/workflows, so it "
                                   f"merges with the merge token",), land_by)


@dataclasses.dataclass(frozen=True)
class ReviewPick:
    route: str     # "claude-api" (automatic, budgeted) or "ask" (a human-driven app)
    agent: str     # agent key asked, "" for claude-api


def pick_reviewer(author_vendor: str, needs: frozenset[str], team: Team) -> ReviewPick:
    """Who reviews a PR, by its author's vendor.

    A PR by another vendor is asked of the first agent in review.other_authored_asks whose vendor is
    not the author's (Cory, 11-03: Muse is always on and free, so it goes first). Only when none
    fits, or the author is unknown, does it get the paid automatic Claude review through the API. A
    Claude-authored PR is asked on the job issue: the matron reviewer first when the job needs matron
    (its specialty), else the first agent in review.claude_authored_asks. A `wake: manual` agent is never
    in those lists (team_from_config drops it).
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


def reviewers_done(verdicts: Iterable[Verdict], head_sha: str, team: Team) -> frozenset[str]:
    """The reviewers whose turn on this head is over: each has a review verdict naming it. A NOTE decides
    nothing (needs_review), so the next reviewer in line is asked: never the one who gave it, a second time."""
    done: set[str] = set()
    for v in verdicts:
        if v.kind != "review" or not names_head(v, head_sha):
            continue
        if is_api_review(v):
            done.add(API_REVIEWER)
        elif is_codex_app(v):
            done.add("codex")
        elif v.who.lower() in team.agent_of_name:
            done.add(team.agent_of_name[v.who.lower()])
    return frozenset(done)


def review_rotation(number: int, author_vendor: str, needs: frozenset[str], team: Team,
                    api_ok: bool = True, done: frozenset[str] = frozenset()) -> list[str]:
    """Who may review PR `number`, one of them per PR (Cory, 11-03: "get the ones that are available and
    ready to do the reviews ... if we spread the work out, the merrier ... I'm not asking for two agents
    to review the same shit"); choose_reviewer picks the free one.

    The pool members whose vendor is not the author's, the Claude API only for work not by Claude
    (reviews stay cross-model) and only while the budget allows a call (`api_ok`). The start rotates
    with the PR number, so consecutive PRs go to different reviewers; a job that needs matron starts
    at review.matron_reviewer, its specialty. An unknown author gets the API alone, as before.

    `done` (reviewers_done): whoever already gave a verdict on this head is out of its rotation. The API being
    in it (its NOTE landed nothing) reads like the API being unable to review: the fallback order follows."""
    api_ok = api_ok and API_REVIEWER not in done
    if author_vendor == "unknown":
        return [API_REVIEWER] if api_ok else []
    fits = [p for p in team.pool
            if p not in done and (
                (p == API_REVIEWER and api_ok and (author_vendor != "anthropic" or team.api_reviews_claude))
                or (p != API_REVIEWER and team.agent_vendor.get(p) not in (None, "anthropic", author_vendor)))]
    # The last resort (Cory, 11-04): when the API is in the pool but cannot review this head (a cap is hit, it
    # failed, or its verdict here was a NOTE), the agents of review.fallback_order follow, by their own scheduled runs.
    last = [f for f in team.fallback_order
            if not api_ok and API_REVIEWER in team.pool and f not in fits and f not in done
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
    """Proof green and no LAND or FIX yet on this head from a review that can count. A NOTE decides nothing:
    merge_decision merges only on a LAND, so a head whose only verdict is a NOTE still needs a review (#376
    sat on the API's NOTE with no one asked; reviewers_done keeps the one who gave it from being asked again).
    A verdict by the author's own vendor (say the Codex app's unasked review of a ChatGPT PR) never answers the
    ask either, since merge_decision would not count its LAND. Its FIX still holds the merge back there."""
    if pr.proof != "green" or pr.draft or pr.base_ref != TRUNK:
        return False
    return not any(v.kind == "review" and v.verdict in ("LAND", "FIX") and names_head(v, pr.head_sha)
                   and not (author_vendor and v.vendor == author_vendor and not land_counts(v, author_vendor, team))
                   for v in pr.verdicts)


def known_defect_jobs(spec: Mapping) -> tuple[str, ...]:
    """The open jobs a look set names as owning its known defect (`known_defect: J-063`, or a list)."""
    kd = spec.get("known_defect") or ()
    return (kd,) if isinstance(kd, str) else tuple(kd)


def eye_overall(judged: Sequence[Mapping], spec_sets: Mapping[str, Mapping], pr_job: str,
                open_jobs: Iterable[str]) -> tuple[str, dict[str, tuple[str, ...]]]:
    """The eye's overall verdict, and the sets whose FIX does not count on this PR (J-068).

    A set whose `known_defect` names a job still open (its issue labeled `job`, open) is waived on every PR
    but that job's own: a FIX there is shown, marked as that job's defect, and does not make the verdict FIX.
    On the owner job's PR, and once every named job's issue is closed, the set counts like any other.
    A set without the field always counts. Every set must still be judged."""
    open_now = set(open_jobs)
    waived: dict[str, tuple[str, ...]] = {}
    for name, spec in spec_sets.items():
        owners = known_defect_jobs(spec)
        still_open = tuple(j for j in owners if j in open_now)
        if still_open and pr_job not in owners:
            waived[name] = still_open
    ok = {s["set"] for s in judged} == set(spec_sets) and \
        all(s["verdict"] == "LAND" or s["set"] in waived for s in judged)
    return ("LAND" if ok else "FIX"), waived


EYE_SET_LINE = re.compile(r"^- (?P<set>[A-Za-z0-9_-]+): (?P<verdict>LAND|FIX)\b", re.MULTILINE)


def eye_sets(body: str) -> dict[str, str] | None:
    """The per-set verdicts the eye's comment lists (switch.eye: "- <set>: LAND|FIX..."), the worst per set, so a
    line its free text adds can never turn a FIX into a LAND. None when it lists a set as not judged, or none."""
    if re.search(r"^- sets not judged:", body, re.MULTILINE):
        return None
    out: dict[str, str] = {}
    for m in EYE_SET_LINE.finditer(body):
        name, verdict = m.group("set"), m.group("verdict")
        out[name] = "FIX" if "FIX" in (verdict, out.get(name)) else "LAND"
    return out or None


def eye_relook(v: Verdict, body: str, spec_sets: Mapping[str, Mapping], pr_job: str,
               open_jobs: Iterable[str]) -> Verdict:
    """The eye's FIX on a head, read again under today's look sets (11-12, #453): when every set it failed is now
    a known defect of an open job other than this PR's (work/looks/sets.yml changed after the eye looked), it is
    the eye's LAND, as a fresh look would post (eye_overall), with no paid call. Only the workflow bot's eye FIX
    with its repro, and every set of today's sets.yml must be in it; anything else is returned as it is."""
    if v.kind != "eye" or v.verdict != "FIX" or v.author_login != BOT_LOGIN or v.downgraded:
        return v
    sets = eye_sets(body)
    if not sets:
        return v
    overall, _ = eye_overall([{"set": k, "verdict": x} for k, x in sets.items()], spec_sets, pr_job, open_jobs)
    return dataclasses.replace(v, verdict="LAND") if overall == "LAND" else v


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


def superseded_prs(body: str | None, own: int) -> list[int]:
    """The PR numbers a PR's body says it supersedes, in order, without repeats or itself."""
    out: list[int] = []
    for m in SUPERSEDES.finditer(body or ""):
        for n in re.findall(r"#(\d+)", m.group("list")):
            if int(n) != own and int(n) not in out:
                out.append(int(n))
    return out


OUTSIDE = frozenset({"NONE", "CONTRIBUTOR", "FIRST_TIMER", "FIRST_TIME_CONTRIBUTOR", "MANNEQUIN"})


def team_issue(issue: dict) -> bool:
    """Is this issue the team's? The repo is public: anyone can open an issue titled J-NNN, and a stranger's is
    not a job (it would be a second issue for a real job, or a job the plan never held). The team's is one a
    login with write access opened (GitHub's author_association, on every issue it returns) or labeled `job`
    (a label takes write access to set)."""
    labels = {(lab.get("name") if isinstance(lab, dict) else lab) for lab in issue.get("labels") or []}
    return "job" in labels or (issue.get("author_association") or "OWNER") not in OUTSIDE


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
