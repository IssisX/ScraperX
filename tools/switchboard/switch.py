#!/usr/bin/env python3
"""The switchboard (design v3 step B, job J-031): GitHub's wake switch and merger. No model decides here.

Grown from tools/board/sweep.py. Each run reads the state from GitHub and acts on it, so a run that
is dropped or repeated changes nothing: every write is either guarded by a marker comment for the
head it acts on, or idempotent at GitHub (a merge names the head sha it merges).

Parts (a workflow job runs one; `all` runs both, for a dry run):
  ask    the paid calls, the only part that holds ANTHROPIC_API_KEY: the automatic Claude review of
         a non-Claude PR, the eye on a `needs:eyes` PR's screenshots, the orchestrator (off in v0)
  steer  everything free: the merge rule, review asks to ChatGPT or Muse, one owner:* label per
         issue, opening PRs for finished fb-bot/integrate-* branches, the 7:00 AM Central digest, and
         after a full sweep one comment on the integrator's inbox PR (#282) for what needs it (J-051),
         and the Muse mail pipe's health check (a PIPE ALARM on the board, once per incident)

Usage:
  python3 tools/switchboard/switch.py --dry-run                     # every action printed, none taken
  python3 tools/switchboard/switch.py --part steer [--pr N | --sha S]
  python3 tools/switchboard/switch.py --part ask [--pr N | --sha S]
  python3 tools/switchboard/switch.py --part steer --issue N --label L   # an issues.labeled event
  python3 tools/switchboard/switch.py --part steer --digest [--force-digest]
Exit status: 0 done, 2 the switch could not read what it needs (nothing was half-done).
"""

from __future__ import annotations

import argparse
import base64
import dataclasses
import datetime as dt
import hashlib
import hmac
import io
import os
import pathlib
import re
import subprocess
import sys
import time
import urllib.parse
import uuid
import zipfile
from decimal import Decimal
from zoneinfo import ZoneInfo

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import budget as money  # noqa: E402
import rules  # noqa: E402
from ghapi import GitHub, GitHubError  # noqa: E402

REPO = "IssisX/ScraperX"
PROOF_WORKFLOW = "wo000-delivery-spine.yml"
NOT_COUNTED = {"publish-checkpoint", "playtest-apk", "wake-switchboard"}   # as sweep.py: skipped or signing-only off trunk
WAKE_JOB = "wake-switchboard"   # the proof's last job, which dispatches this sweep (wo000-delivery-spine.yml)


def only_wake_left(jobs: list[dict]) -> bool:
    """True when the proof's one unfinished job is the one waking this sweep: every proof job is done."""
    return any(j["name"] == WAKE_JOB for j in jobs) and all(
        j["status"] == "completed" for j in jobs if j["name"] != WAKE_JOB)
LEDGER_BRANCH = "switchboard/ledger"
LEDGER_PATH = "work/archive/runs.log"
DIGEST_TITLE = "Digest"
DIGEST_LABEL = "digest"
MUSE_LABEL = "wake:muse"
BOARD_ISSUE = 340  # the live board; #107 closed at 100 comments (11-09)
CENTRAL = ZoneInfo("America/Chicago")
DIGEST_HOUR = 7
MAX_DIFF_CHARS = 400_000
MAX_ARTIFACT_BYTES = 60_000_000
MAX_IMAGE_BYTES = 3_750_000            # base64 of this stays under 5 MB
MAX_PRS = 60
MAX_AUTO_PRS = 3                   # PRs the auto-open opens in one sweep (J-047)
AUTO_PR_PREFIX = "fb-bot/integrate-"   # fb-bot/integrate-protected-* matches too; claude/* never does
# GitHub's refusal reads "not permitted to create or approve pull requests" (it was "create pull requests").
AUTO_PR_REFUSED = "not permitted to create"
# The integrator's inbox (J-051): the integrator is subscribed to this draft PR, and each comment on it wakes it.
INBOX_PR = 282
INBOX_BRANCH = "claude/integrator-inbox"
INBOX_HEADER = "Switchboard (inbox):"
INBOX_MARK = "<!-- switchboard:inbox keys={keys} -->"
INBOX_KEYS = re.compile(r"^<!-- switchboard:inbox keys=([0-9a-f,]*) -->$", re.MULTILINE)
INBOX_BRANCH_PREFIXES = ("fb-bot/integrate-", "claude/integrate-")
INBOX_READY_AFTER = dt.timedelta(minutes=15)   # one sweep apart: green with a LAND this long was so at the sweep before
INBOX_RED_STUCK = dt.timedelta(hours=3)        # FIX-CI with no new head this long
INBOX_BRANCH_RED = dt.timedelta(hours=1)       # a red integrate branch with no PR this long
INBOX_RUNS_WINDOW = dt.timedelta(hours=24)     # proof runs read per sweep, as tools/board/sweep.py's branch window
QUIET_HOURS = 3.0                              # tools/board/sweep.py --quiet-hours
MAX_INBOX_LINES = 30                           # items in one inbox comment; the rest come next sweep
MAX_INBOX_KEYS = 600                           # keys carried forward in the newest inbox comment's marker
MAX_INBOX_BRANCHES = 40                        # integrate branches whose proof jobs are read per sweep
INBOX_ORDER = ("MERGE-BY-HAND", "TO-CLAUDE", "NOT-MERGED", "AUTO-PR-FAILED", "FIX", "RED", "RED-TWICE", "RED-STUCK", "RED-BRANCH", "QUIET")
MUSE_MARK = "<!-- switchboard:muse keys={keys} -->"
MUSE_KEYS = re.compile(r"^<!-- switchboard:muse keys=([0-9a-f,]*) -->$", re.MULTILINE)
MAX_MUSE_COMMENTS = 10          # Muse comments posted in one sweep; the rest come next sweep
MAX_MUSE_FINDING = 600          # chars of a FIX finding quoted in a Muse comment
OLD_BOARDS = (107, 87, 3)       # closed boards: the muse markers on them still count, so a board move never re-sends
MAX_OLD_BOARD_PAGES = 3         # comment pages read from each closed board (a board closes at 100 comments)
# The Muse mail pipe's health (bridge-health): the bridge (muse_bridge.gs) is an Apps Script whose errors only Cory
# can see, so the switch watches what it leaves on GitHub and posts one alarm per incident on the board.
PIPE_HEADER = "Claude (integrator): PIPE ALARM:"
PIPE_MARK = "<!-- switchboard:pipe-alarm kinds={kinds} -->"
PIPE_KINDS = re.compile(r"^<!-- switchboard:pipe-alarm kinds=([a-z,]*) -->$", re.MULTILINE)
PIPE_WAKE_STUCK = dt.timedelta(minutes=20)    # a wake:muse label this old: the bridge's outbound is stuck (it runs every 5 min)
PIPE_INBOUND_QUIET = dt.timedelta(hours=24)   # no bridge post this long while asks went to Muse: its inbound is stuck
PIPE_LOOKBACK = dt.timedelta(days=7)          # how far back an open inbound incident is read; older, it alarms again
BRIDGE_POST = "**Muse (via email bridge)"     # the first line of every inbound bridge post
MAX_WAKE_ISSUES = 30                          # wake:muse issues read per sweep (one page)
# The switchboard workflow wakes on `: LAND`, `: FIX`, `: NOTE` or `: BLOCK` anywhere in a comment, in any case.
VERDICT_TRIGGER = re.compile(r":(?= ?(?:land|fix|note|block))", re.IGNORECASE)
# J-056: the merge of a PR that changes .github/workflows uses this secret (a fine-grained PAT), held only by the
# steer job's one step; main() takes it out of the environment so no child process inherits it.
MERGE_TOKEN_ENV = "SWITCHBOARD_MERGE_TOKEN"
MERGE_TOKEN_REFUSED = {401, 403, 404, 405}   # GitHub's answers to a token that cannot merge here
MERGE_TOKEN_NEEDS = "Contents, Pull requests and Workflows: read/write"
# J-056: the automatic review signs its verdict with this secret (HMAC-SHA256). Only the ask and steer jobs' switch
# steps hold it, through environment switchboard-merge; main() takes it out of the environment like the merge token.
# Any branch's push workflow posts as the workflow bot, so on a workflow-file PR only a signed review is the pass.
REVIEW_KEY_ENV = "SWITCHBOARD_REVIEW_KEY"
REVIEW_SIG = "<!-- switchboard:review-sig v1 pr={pr} head={head} verdict={verdict} mac={mac} -->"
REVIEW_SIG_LINE = re.compile(r"<!-- switchboard:review-sig v1 pr=(?P<pr>\d{1,9}) head=(?P<head>[0-9a-f]{40}) "
                             r"verdict=(?P<verdict>LAND|FIX|NOTE|NONE) mac=(?P<mac>[0-9a-f]{64}) -->")
MAX_FACT_LINES = 25                   # rule_check lines quoted in a review ask
ACT_WITHIN_DAYS = 3                   # older heads get no new review or ask: J-030 claims or closes them
MARK = "<!-- switchboard:{kind} pr={pr} head={head} -->"
RUN_MARK = "<!-- switchboard:orchestrator-run id={run} -->"   # in the start comment: the ledger run its session pays
RUN_ID = re.compile(r"<!-- switchboard:orchestrator-run id=([0-9a-f]{12}) -->")
UNKNOWN_MARK = "<!-- switchboard:orchestrator-unknown run={run} at={at} -->"   # a start whose answer was lost
UNKNOWN_RUN = re.compile(r"<!-- switchboard:orchestrator-unknown run=([0-9a-f]{12}) at=(\S+) -->")
GIVE_UP_AFTER = dt.timedelta(minutes=30)   # an unknown start with no session found by then is written off
MAX_WATCHED = 30                       # open orchestrator issues the watcher reads (one page)
CONSOLE_SESSION = "https://platform.claude.com/workspaces/default/sessions/{session}"   # `default` workspace
OUTCOMES = {"end_turn": "finished its turn", "budget_reached": "stopped at its session budget",
            "retries_exhausted": "stopped after repeated errors", "refusal": "stopped on a refusal",
            "terminated": "ended (terminated)"}


def central(t: dt.datetime) -> str:
    return t.astimezone(CENTRAL).strftime("%-I:%M %p Central")


def load_yaml(path: pathlib.Path):
    import yaml
    with path.open(encoding="utf-8") as f:
        return yaml.safe_load(f)


def parse_time(stamp: str) -> dt.datetime | None:
    return dt.datetime.fromisoformat(stamp.replace("Z", "+00:00")) if stamp else None


def is_inbox(pr: dict) -> bool:
    """The integrator's inbox PR, which the switch never routes, reviews, merges or closes (J-051)."""
    return pr.get("number") == INBOX_PR or (pr.get("head") or {}).get("ref") == INBOX_BRANCH


def untrusted(text: str, limit: int = 80) -> str:
    """PR or branch text in one code span: no backtick, @, angle bracket or control character, bounded."""
    clean = "".join(ch for ch in text.replace("`", "'") if ch.isprintable() and ch not in "@<>")[:limit]
    return f"`{clean or '-'}`"


def plain(text: str) -> str:
    """The switch's own words in an inbox line: no @, and <...> shown rather than read as HTML."""
    return text.replace("@", "").replace("<", "&lt;").replace(">", "&gt;")


def no_verdict(text: str) -> str:
    """Outside text in an inbox line that cannot wake the switchboard: a colon before a verdict word becomes ' -'."""
    return VERDICT_TRIGGER.sub(" -", text)


def hours(span: dt.timedelta) -> str:
    return f"{span.total_seconds() / 3600:.1f} h"


def red_jobs(proof: str) -> list[str]:
    return proof[len("red: "):].split(", ") if proof.startswith("red: ") else []


def http_status(detail: str) -> int:
    """The HTTP status in gh's error text ("gh: Not Found (HTTP 404)"), 0 when it names none."""
    m = re.search(r"\bHTTP (\d{3})\b", detail)
    return int(m.group(1)) if m else 0


class MergeTokenRefused(RuntimeError):
    """GitHub refused the merge token on a workflow-file PR (J-056): the PR becomes merge-by-hand, with why."""


def review_mac(key: str, repo: str, pr: int, head: str, verdict: str) -> str:
    """HMAC-SHA256 under the review key of one automatic review's canonical message (J-056): the version, the repo,
    the PR, the whole head sha and the verdict ("NONE" for a security pass that gave none), one per line."""
    msg = "\n".join(("switchboard-review-sig", "v1", repo, str(pr), head, verdict))
    return hmac.new(key.encode("utf-8"), msg.encode("utf-8"), hashlib.sha256).hexdigest()


def review_sig(key: str, repo: str, pr: int, head: str, verdict: str) -> str:
    """The last line of an automatic review's comment: its MAC, never the key."""
    return REVIEW_SIG.format(pr=pr, head=head, verdict=verdict, mac=review_mac(key, repo, pr, head, verdict))


def signed_for(body: str, key: str, repo: str, pr: int, head: str, verdict: str = "") -> bool:
    """The comment's last line is a review-sig line for this PR, this whole head and (when given) this verdict, and
    its MAC checks under `key`. Never without a key."""
    m = REVIEW_SIG_LINE.fullmatch(body.rstrip().rsplit("\n", 1)[-1].strip()) if key else None
    if not m or int(m["pr"]) != pr or m["head"] != head or (verdict and m["verdict"] != verdict):
        return False
    return hmac.compare_digest(m["mac"], review_mac(key, repo, pr, head, m["verdict"]))


def signed_verdict(v: rules.Verdict) -> str:
    """The verdict review() signed for this parsed one: as posted, before a FIX without a repro reads as NOTE."""
    return "FIX" if v.downgraded else v.verdict


def inbox_key(kind: str, target: str, head: str) -> str:
    """One item's key: its kind, its PR (#N) or branch, and the head sha. A hash, so branch text never reaches the marker."""
    import hashlib
    return hashlib.sha256(f"{kind}|{target}|{head}".encode("utf-8")).hexdigest()[:16]


@dataclasses.dataclass
class Steered:
    """One PR as the steer sweep left it, read by the inbox (J-051)."""
    facts: rules.PrFacts
    ctx: dict
    decision: rules.MergeDecision
    merge_note: str = ""   # why a MERGE did not happen; "" once merged, or in a dry run


def owner_of(ref: str) -> str:
    """Whose work a branch is: Muse's (fb-bot/), Claude's (claude/), or "" when neither routes it (J-052)."""
    return "muse" if ref.startswith("fb-bot/") else "claude" if ref.startswith("claude/") else ""


@dataclasses.dataclass(frozen=True)
class InboxItem:
    kind: str     # one of INBOX_ORDER
    target: str   # "#277", or the branch name
    head: str     # the head sha
    line: str     # the inbox line, without the leading "- "
    dest: str = "claude"   # who it goes to: "muse", "claude", or "both" (J-052)
    finding: str = ""      # for a FIX: the finding's first 600 chars and its link, for Muse's comment

    @property
    def key(self) -> str:
        return inbox_key(self.kind, self.target, self.head)

    @property
    def muse_key(self) -> str:
        return inbox_key("muse-" + self.kind, self.target, self.head)


def inbox_body(items: list[InboxItem], held: int, keys: list[str], when: str) -> str:
    """The inbox comment: the header line first, one line per item, the keys in a hidden marker last."""
    lines = [INBOX_HEADER, "", f"{len(items)} new for the integrator at {when} (each is listed once per head):", ""]
    lines += [f"- {it.line}" for it in items]
    if held:
        lines.append(f"- ({held} more next sweep)")
    body = "\n".join(lines).replace("@", "")   # nothing in an inbox comment can mention anyone
    return f"{body}\n\n{INBOX_MARK.format(keys=','.join(keys))}"


class Switch:
    auto_pr_failed: tuple[InboxItem, ...] = ()   # this sweep's AUTO-PR-FAILED items, for the inbox (J-051)
    merge_token: str = ""                        # SWITCHBOARD_MERGE_TOKEN, set by main() in the steer job only (J-056)
    review_key: str = ""                         # SWITCHBOARD_REVIEW_KEY, set by main() in the ask and steer jobs (J-056)

    def __init__(self, gh: GitHub, root: pathlib.Path, now: dt.datetime, out=print):
        self.gh, self.root, self.now, self.out = gh, root, now, out
        self.team = rules.team_from_config(load_yaml(root / "work/agents.yml"))
        self.budget = money.budget_from_config(load_yaml(root / "work/budget.yml"))
        self.looks = load_yaml(root / "work/looks/sets.yml")
        self._jobs: dict[str, dict] | None = None
        self._ledger: tuple[str, str | None] | None = None
        self._load: tuple[dict[str, int], dict[str, int]] | None = None   # review_load, once per run
        self._claude = None

    # ---------- facts ----------
    def jobs(self) -> dict[str, dict]:
        """Open issues labeled `job`, by J-NNN."""
        if self._jobs is None:
            self._jobs = {}
            for it in self.gh.paged(f"repos/{self.gh.repo}/issues?labels=job&state=open"):
                if "pull_request" not in it and rules.job_of_title(it["title"]):
                    self._jobs[rules.job_of_title(it["title"])] = it
        return self._jobs

    def proof(self, sha: str) -> tuple[str, str, int, str]:
        """The newest proof run on this commit: (state, url, run id)."""
        runs = self.gh.get(f"repos/{self.gh.repo}/actions/workflows/{PROOF_WORKFLOW}/runs?head_sha={sha}&per_page=5")["workflow_runs"]
        if not runs:
            return "none", "", 0, ""
        return self.run_state(max(runs, key=lambda r: r["created_at"]))

    def run_state(self, run: dict) -> tuple[str, str, int, str]:
        """One proof run, judged by its jobs: (state, url, run id, finished at)."""
        jobs = self.gh.get(f"repos/{self.gh.repo}/actions/runs/{run['id']}/jobs?per_page=50")["jobs"]
        if run["status"] != "completed" and not only_wake_left(jobs):
            return "running", run["html_url"], run["id"], ""
        failed = [j["name"] for j in jobs if j["name"] not in NOT_COUNTED and j["conclusion"] not in ("success", "skipped")]
        return ("red: " + ", ".join(failed) if failed else "green"), run["html_url"], run["id"], run.get("updated_at") or ""

    def verdicts(self, number: int) -> tuple[list[rules.Verdict], list[dict]]:
        comments = self.gh.paged(f"repos/{self.gh.repo}/issues/{number}/comments")
        reviews = self.gh.paged(f"repos/{self.gh.repo}/pulls/{number}/reviews")
        out = []
        for c in comments:
            body = c.get("body") or ""
            v = rules.parse_verdict(body, c["created_at"], c["user"]["login"], self.team) or \
                rules.parse_codex_state(body, c.get("updated_at") or c["created_at"], c["user"]["login"])
            # J-056: an automatic review whose MAC checks for this PR, the whole head it names and its verdict.
            if v and rules.is_api_review(v) and signed_for(body, self.review_key, self.gh.repo, number, v.sha,
                                                           signed_verdict(v)):
                v = dataclasses.replace(v, signed=True)
            if v:
                out.append(v)
        for r in reviews:
            v = rules.parse_verdict(r.get("body") or "", r.get("submitted_at") or "", r["user"]["login"], self.team)
            if v:
                out.append(v)
        return out, comments

    def run_rule_check(self, number: int, head: str) -> tuple[int, str] | None:
        """Run trunk's rule check over base...head (it reads the diff only): (exit status, its output). None if it cannot run."""
        try:
            subprocess.run(["git", "fetch", "-q", "--no-tags", "origin", rules.TRUNK,
                            f"+refs/pull/{number}/head:refs/remotes/switchboard/pr-{number}"],
                           cwd=self.root, check=True, capture_output=True, timeout=180)
            r = subprocess.run([sys.executable, "tools/rule_check/rule_check.py", f"origin/{rules.TRUNK}", head],
                               cwd=self.root, capture_output=True, text=True, timeout=180)
        except (subprocess.SubprocessError, OSError) as err:
            self.out(f"NOTE  #{number} rule_check could not run: {err}")
            return None
        if r.returncode not in (0, 1, 2):
            self.out(f"NOTE  #{number} rule_check exited {r.returncode}: {r.stderr.strip()[:200]}")
            return None
        return r.returncode, r.stdout

    def rule_check_block(self, number: int, head: str) -> tuple[bool | None, bool]:
        """(does trunk's rule check BLOCK base...head, None if it cannot run; does a BLOCK hit a never-edited file)."""
        got = self.run_rule_check(number, head)
        if got is None:
            return None, False
        never = any(l.startswith("BLOCK") and " protected-file:" in l for l in got[1].splitlines())
        return got[0] == 2, never

    def review_facts(self, facts: rules.PrFacts, ctx: dict) -> str:
        """What a reviewer with no checkout keeps asking for, read here at the head: the proof's jobs and
        trunk's rule check over base...head. Every line is bounded, and fenced so PR text cannot add markup."""
        sha = facts.head_sha[:8]
        lines = [f"Facts read by the switchboard at @{sha} (a reviewer needs no checkout for these):"]
        if ctx.get("run_id"):
            jobs = self.gh.get(f"repos/{self.gh.repo}/actions/runs/{ctx['run_id']}/jobs?per_page=50")["jobs"]
            tally: dict[str, list[str]] = {}
            for j in jobs:
                tally.setdefault(j["conclusion"] or j["status"], []).append(j["name"])
            counts = ", ".join(f"{len(v)} {k}" for k, v in sorted(tally.items()))
            bad = [n for k, v in sorted(tally.items()) if k not in ("success", "skipped") for n in v]
            lines.append(f"- Proof jobs ({ctx['proof_url']}): {counts or 'none'}" + (f"; not passing: {', '.join(bad)}" if bad else ""))
        else:
            lines.append(f"- Proof: {facts.proof}")
        got = self.run_rule_check(facts.number, facts.head_sha)
        if got is None:
            lines.append("- rule_check: could not run here")
        else:
            out = [l.rstrip() for l in got[1].splitlines() if l.strip()]
            summary = out[-1] if out else "rule_check: no output"
            findings = [l for l in out[:-1] if l.startswith(("BLOCK", "REVIEW"))]
            lines.append(f"- {summary} (origin/{rules.TRUNK}...@{sha})")
            if findings:
                shown = [l[:200].replace("`" * 3, "'" * 3) for l in findings[:MAX_FACT_LINES]]
                lines += ["`" * 3] + shown + ["`" * 3]
                if len(findings) > len(shown):
                    lines.append(f"({len(findings) - len(shown)} more lines not shown)")
        return "\n".join(lines)

    def pr_facts(self, pr: dict) -> tuple[rules.PrFacts, dict]:
        number, sha = pr["number"], pr["head"]["sha"]
        full = self.gh.get(f"repos/{self.gh.repo}/pulls/{number}")
        commit = self.gh.get(f"repos/{self.gh.repo}/commits/{sha}")
        proof, url, run_id, proof_done_at = self.proof(sha)
        verdicts, comments = self.verdicts(number)
        job = rules.job_of_pr(pr.get("body")) or \
            rules.jobs_by_pr_mention({j: it["title"] for j, it in self.jobs().items()}).get(number, "")
        issue = self.jobs().get(job)
        labels = [l["name"] for l in pr.get("labels", [])] + ([l["name"] for l in issue["labels"]] if issue else [])
        needs = frozenset(l.split(":", 1)[1] for l in labels if l.startswith("needs:"))
        files = self.gh.paged(f"repos/{self.gh.repo}/pulls/{number}/files")
        facts = rules.PrFacts(
            number=number, head_sha=sha, head_ref=pr["head"]["ref"], base_ref=pr["base"]["ref"], draft=bool(pr["draft"]),
            mergeable=full.get("mergeable"), head_committed_at=commit["commit"]["committer"]["date"], proof=proof,
            verdicts=verdicts, needs=needs, rule_check_block=False,
            touches_workflows=any(f["filename"].startswith(".github/workflows/") for f in files))
        ctx = {"job": job, "issue": issue, "proof_url": url, "run_id": run_id, "proof_done_at": proof_done_at, "comments": comments,
               "title": pr["title"], "body": pr.get("body") or "", "files": files, "updated_at": pr.get("updated_at") or ""}
        return facts, ctx

    def target_prs(self, only_pr: int, only_sha: str) -> list[dict]:
        """The open PRs the switch acts on: never the integrator's inbox PR (J-051)."""
        prs = [p for p in self.gh.paged(f"repos/{self.gh.repo}/pulls?state=open") if not is_inbox(p)][:MAX_PRS]
        if only_pr:
            prs = [p for p in prs if p["number"] == only_pr]
        if only_sha:
            prs = [p for p in prs if p["head"]["sha"] == only_sha]
        return prs

    def stale(self, facts: rules.PrFacts) -> bool:
        return rules.is_stale(facts.head_committed_at, self.now.strftime("%Y-%m-%dT%H:%M:%SZ"), ACT_WITHIN_DAYS)

    def marked(self, comments: list[dict], kind: str, pr: int, head: str, signed: bool = False) -> bool:
        """A bot comment carries this mark. `signed` (the security pass, with the review key; J-056): only on a comment
        the switch signed for this head, so a forged mark cannot stop the real review."""
        tag = MARK.format(kind=kind, pr=pr, head=head)
        return any(tag in (c.get("body") or "") and c["user"]["login"] == rules.BOT_LOGIN and
                   (not signed or self.signed_mark(c.get("body") or "", pr, head)) for c in comments)

    def signed_mark(self, body: str, pr: int, head: str) -> bool:
        """Its MAC covers this head and the comment's own verdict: a review, or the security pass's note ("NONE")."""
        v = rules.parse_verdict(body, "", rules.BOT_LOGIN, self.team)
        return signed_for(body, self.review_key, self.gh.repo, pr, head, signed_verdict(v) if v else "NONE")

    # ---------- the ledger (append-only, on its own branch so trunk is never written per call) ----------
    def ledger(self) -> tuple[str, str | None]:
        if self._ledger is None:
            try:
                got = self.gh.get(f"repos/{self.gh.repo}/contents/{LEDGER_PATH}?ref={LEDGER_BRANCH}")
            except GitHubError as err:
                if "HTTP 404" not in err.detail:
                    raise
                self._ledger = ("", None)
            else:
                if got.get("size", 0) > self.budget.max_log_bytes or got.get("encoding") != "base64":
                    raise money.BudgetConfigError(f"{LEDGER_PATH} is {got.get('size')} bytes; rotate it before any paid call")
                self._ledger = (base64.b64decode(got["content"]).decode("utf-8"), got["sha"])
        return self._ledger

    def entries(self) -> list[money.Entry]:
        return money.parse_log(self.ledger()[0])

    def append(self, line: str, what: str) -> None:
        if self.gh.dry_run:
            self.out(f"DRY  append {LEDGER_BRANCH}:{LEDGER_PATH}  {what}")
            return
        for _ in range(3):
            text, sha = self.ledger()
            if sha is None:
                self.ensure_ledger_branch()
            body = {"message": f"[skip ci] runs.log: {what}"[:120], "branch": LEDGER_BRANCH,
                    "content": base64.b64encode((text + line + "\n").encode("utf-8")).decode("ascii")}
            if sha:
                body["sha"] = sha
            try:
                got = self.gh.write("PUT", f"repos/{self.gh.repo}/contents/{LEDGER_PATH}", body, what)
            except GitHubError as err:
                if "HTTP 409" in err.detail or "HTTP 422" in err.detail:   # someone appended first: re-read
                    self._ledger = None
                    continue
                raise
            self._ledger = (text + line + "\n", got["content"]["sha"])
            return
        raise money.BudgetConfigError(f"could not append to {LEDGER_PATH} after 3 tries")

    def ensure_ledger_branch(self) -> None:
        try:
            self.gh.get(f"repos/{self.gh.repo}/git/ref/heads/{LEDGER_BRANCH}")
        except GitHubError as err:
            if "HTTP 404" not in err.detail:
                raise
            trunk = self.gh.get(f"repos/{self.gh.repo}/git/ref/heads/{rules.TRUNK}")["object"]["sha"]
            self.gh.write("POST", f"repos/{self.gh.repo}/git/refs",
                          {"ref": f"refs/heads/{LEDGER_BRANCH}", "sha": trunk}, f"create {LEDGER_BRANCH}")

    def job_cap(self, ctx: dict) -> str:
        issue = ctx.get("issue")
        if not issue:
            return ""
        comments = self.gh.paged(f"repos/{self.gh.repo}/issues/{issue['number']}/comments")
        return rules.budget_override(((c.get("body") or "", c["user"]["login"]) for c in comments), self.team)

    def gate(self, kind: str, job: str, cost: Decimal, cap: str, key: str) -> money.Check:
        """The budget check before a paid call; a refusal is logged once per key per UTC day."""
        entries = self.entries()
        verdict = money.check(self.budget, entries, self.now, job, cost, cap)
        if not verdict.ok:
            today = self.now.date()
            if not any(e.phase == "refused" and e.note.startswith(key) and e.ts.date() == today for e in entries):
                self.append(money.record(self.now, uuid.uuid4().hex[:12], "refused", kind, job, Decimal(0),
                                         f"{key} {verdict.reason}"), f"refused {key}")
        return verdict

    def claude(self):
        if self._claude is None:
            import claude_calls
            if not os.environ.get("ANTHROPIC_API_KEY"):
                raise money.BudgetConfigError("ANTHROPIC_API_KEY is not set; no paid call can run")
            self._claude = claude_calls.client()
        return self._claude

    def paid(self, kind: str, job: str, call: str, system: str, content: list, schema: dict, cap: str, key: str):
        """Count tokens, gate on the budget, reserve, call, record the actual cost. None when refused."""
        import claude_calls
        spec = self.budget.calls[call]
        if self.gh.dry_run:
            worst = money.worst_case(self.budget, call, spec.max_input_tokens)
            v = money.check(self.budget, self.entries(), self.now, job, worst, cap)
            self.out(f"DRY  {kind} {key} on {spec.model}: {'would run' if v.ok else 'REFUSED'} ({v.reason}; upper bound)")
            return None
        n = claude_calls.count_tokens(self.claude(), spec.model, system, content)
        if n > spec.max_input_tokens:
            self.out(f"SKIP {kind} {key}: {n} input tokens is above the {spec.max_input_tokens} limit")
            return "too-large"
        worst = money.worst_case(self.budget, call, n)
        v = self.gate(kind, job, worst, cap, key)
        if not v.ok:
            self.out(f"REFUSED {kind} {key}: {v.reason}")
            return None
        run = uuid.uuid4().hex[:12]
        self.append(money.record(self.now, run, "reserve", kind, job, worst, f"{key} in={n}"), f"reserve {key}")
        try:
            res = claude_calls.ask_json(self.claude(), spec.model, spec.max_tokens, spec.effort, system, content, schema)
        except claude_calls.ClaudeCallError as err:
            self.out(f"FAILED {kind} {key}: {err}")   # the reserve stands: an error may still have been billed
            return "error"
        actual = worst
        note = f"{key} {res.model} in={res.input_tokens} out={res.output_tokens} stop={res.stop_reason}"
        if res.model in self.budget.prices:
            actual = self.budget.prices[res.model].cost(res.input_tokens, res.output_tokens)
        else:
            note += " unpriced-model:reserve-kept"
        self.append(money.record(self.now, run, "end", kind, job, actual, note), f"end {key}")
        return res

    # ---------- steer ----------
    def close_superseded(self, facts: rules.PrFacts, ctx: dict) -> None:
        """A ready PR whose body says `Supersedes #N` closes #N, so replaced work can never be merged (Cory, 11-04)."""
        if facts.draft:
            return
        for n in rules.superseded_prs(ctx["body"], facts.number):
            if n == INBOX_PR:   # the integrator's inbox is never closed (J-051)
                continue
            try:
                old = self.gh.get(f"repos/{self.gh.repo}/pulls/{n}")
            except GitHubError:   # not a PR (an issue number, or gone): nothing to close
                continue
            if old.get("state") != "open":
                continue
            self.gh.comment(n, f"Switchboard: closing this PR. #{facts.number} supersedes it (its body says so); "
                               f"the work continues there.", what=f"note on #{n}: superseded by #{facts.number}")
            self.gh.write("PATCH", f"repos/{self.gh.repo}/pulls/{n}", {"state": "closed"},
                          what=f"close #{n}: superseded by #{facts.number}")
            self.out(f"SUPERSEDED  #{n} closed: #{facts.number} supersedes it")

    def steer_pr(self, facts: rules.PrFacts, ctx: dict, record: list | None = None) -> None:
        """The merge rule and the review asks for one PR; `record` (the sweep's list) gets what was decided."""
        author = self.team.vendor_of_branch(facts.head_ref)
        token, key = bool(self.merge_token), bool(self.review_key)
        d = rules.merge_decision(facts, author, self.team, token, key)
        if d.action == "WAIT" and d.reasons == ("GitHub is still computing mergeability",):
            time.sleep(3)   # the first read starts GitHub's computation; read once more
            facts = dataclasses.replace(facts, mergeable=self.gh.get(f"repos/{self.gh.repo}/pulls/{facts.number}").get("mergeable"))
            d = rules.merge_decision(facts, author, self.team, token, key)
        if d.action in ("MERGE", "HUMAN-MERGE"):
            block, never = self.rule_check_block(facts.number, facts.head_sha)
            facts = dataclasses.replace(facts, rule_check_block=block, never_edit=never)
            d = rules.merge_decision(facts, author, self.team, token, key)
        self.out(f"{d.action:<11} #{facts.number} {facts.head_ref} @{facts.head_sha[:8]} author={author} "
                 f"proof={facts.proof} job={ctx['job'] or '-'}: {'; '.join(d.reasons)}")
        seen = Steered(facts, ctx, d)
        if record is not None:
            record.append(seen)
        if d.action == "MERGE":
            seen.merge_note = "the merge step raised an error (see the switchboard log)"   # kept if it raises
            try:
                seen.merge_note = self.merge(facts, ctx, d)
            except MergeTokenRefused as err:   # the inbox lists it as MERGE-BY-HAND, with GitHub's status (J-056)
                seen.decision, seen.merge_note = rules.MergeDecision("HUMAN-MERGE", (str(err),), d.land_by), ""
                self.out(f"HUMAN-MERGE #{facts.number}: {err}")
        if rules.needs_review(facts, author, self.team) and self.stale(facts):
            self.out(f"STALE       #{facts.number} head older than {ACT_WITHIN_DAYS} days: no review asked")
        elif rules.needs_review(facts, author, self.team) and self.codex_pending(facts, ctx):
            self.out(f"WAIT-CODEX  #{facts.number} Codex is reviewing @{facts.head_sha[:8]}: no other review for up to "
                     f"{rules.CODEX_WAIT_MINUTES} minutes after the proof went green")
        elif rules.needs_review(facts, author, self.team) and self.team.pool:
            _, who, asked = self.pooled_reviewer(facts, ctx, author)
            if not who:
                self.out(f"NOFALLBACK  #{facts.number} everyone in the pool was asked and nobody answered")
            elif who != rules.API_REVIEWER and who not in asked:
                quiet = [self.team.agent_name.get(a, a) for a in asked]
                why = (f"{', '.join(quiet)} did not answer in {self.team.fallback_after_hours:g} hours, "
                       f"so you are asked instead. ") if quiet else ""
                self.ask_review(facts, ctx, who, why)
                self.note_assigned(who)
        elif rules.needs_review(facts, author, self.team):
            pick = rules.pick_reviewer(author, facts.needs, self.team)
            if pick.route == "ask":
                self.ask_review(facts, ctx, pick.agent, "")
            self.escalate_review(facts, ctx, author)

    def codex_pending(self, facts: rules.PrFacts, ctx: dict) -> bool:
        """Codex is still reviewing this head and the proof went green under CODEX_WAIT_MINUTES ago: wait for it
        rather than pay for a second review (J-031). Past the wait, the review goes on as if Codex were not there."""
        if rules.codex_wait_over(ctx.get("proof_done_at") or "", self.now.strftime("%Y-%m-%dT%H:%M:%SZ")):
            return False
        reactions = self.gh.paged(f"repos/{self.gh.repo}/issues/{facts.number}/reactions")
        return rules.codex_in_flight(ctx["comments"], reactions, facts.head_sha)

    def api_ok(self, ctx: dict) -> bool:
        """May the automatic review run today: the worst case of one call fits under every cap."""
        spec = self.budget.calls["review"]
        worst = money.worst_case(self.budget, "review", spec.max_input_tokens)
        return money.check(self.budget, self.entries(), self.now, ctx["job"], worst, self.job_cap(ctx)).ok

    def pooled_reviewer(self, facts: rules.PrFacts, ctx: dict, author: str,
                        api: bool | None = None) -> tuple[list[str], str, dict[str, str]]:
        """review.pool: the rotation for this PR, the one reviewer its head gets now, and who was asked on it when.
        The API is out of the rotation for a head it already failed on (its fallback ask carries an api-out mark),
        so a quiet fallback passes on to the next unasked reviewer instead of the API being picked again."""
        _, comments = self.ask_thread(facts, ctx)
        if comments is not ctx["comments"]:
            comments = list(comments) + list(ctx["comments"])   # Codex is asked on the PR itself
        out = MARK.format(kind="api-out", pr=facts.number, head=facts.head_sha)
        api_failed = any(out in (c.get("body") or "") and c["user"]["login"] == rules.BOT_LOGIN for c in comments)
        api_ok = (self.api_ok(ctx) if api is None else api) and not api_failed
        rotation = rules.review_rotation(facts.number, author, facts.needs, self.team, api_ok)
        asked: dict[str, str] = {}
        for agent in self.team.agent_vendor:
            tag = MARK.format(kind=f"ask-{agent}", pr=facts.number, head=facts.head_sha)
            times = [c["created_at"] for c in comments
                     if tag in (c.get("body") or "") and c["user"]["login"] == rules.BOT_LOGIN]
            if times:
                asked[agent] = min(times)
        now = self.now.strftime("%Y-%m-%dT%H:%M:%SZ")
        hours = self.team.fallback_after_hours
        if any(w in asked and not rules.ask_is_overdue(asked[w], now, hours) for w in rotation):
            outstanding, recent = {}, {}   # the one asked keeps it: no need to read everyone's load
        else:
            outstanding, recent = self.review_load()
        return rotation, rules.choose_reviewer(rotation, asked, now, hours, outstanding, recent), asked

    def api_fallback(self, facts: rules.PrFacts, ctx: dict, author: str) -> str:
        """Who is asked when the automatic review cannot run on a PR: chosen as the pool chooses, without the API,
        so never the author's own vendor and never someone already asked and quiet on this head (Codex on #178)."""
        if self.team.pool:
            _, who, _ = self.pooled_reviewer(facts, ctx, author, api=False)
            if who:
                return who
        for agent in self.team.human_reviewers:
            if self.team.agent_vendor.get(agent) not in ("anthropic", author):
                return agent
        return self.team.human_reviewers[0]

    def note_assigned(self, who: str) -> None:
        """Count a review handed out in this run, so the next PR in the same sweep sees it (Codex on #178)."""
        if self._load is None:
            self._load = self.review_load()
        outstanding, recent = self._load
        if who != rules.API_REVIEWER:
            outstanding[who] = outstanding.get(who, 0) + 1
        recent[who] = recent.get(who, 0) + 1

    def review_load(self) -> tuple[dict[str, int], dict[str, int]]:
        """Over the open PRs, once per run: per reviewer, the asks on a PR's current head still without that
        reviewer's verdict (busy), and the asks or automatic reviews of the last 24 hours (recent)."""
        if self._load is not None:
            return self._load
        outstanding: dict[str, int] = {}
        recent: dict[str, int] = {}
        day_ago = (self.now - dt.timedelta(hours=24)).strftime("%Y-%m-%dT%H:%M:%SZ")
        for pr in self.target_prs(0, ""):
            number, sha = pr["number"], pr["head"]["sha"]
            verdicts, comments = self.verdicts(number)
            threads = list(comments)
            issue = self.jobs().get(rules.job_of_pr(pr.get("body")) or "")
            if issue:
                threads += self.gh.paged(f"repos/{self.gh.repo}/issues/{issue['number']}/comments")
            bot = [c for c in threads if c["user"]["login"] == rules.BOT_LOGIN]
            for agent, vendor in self.team.agent_vendor.items():
                tag = MARK.format(kind=f"ask-{agent}", pr=number, head=sha)
                times = [c["created_at"] for c in bot if tag in (c.get("body") or "")]
                if not times:
                    continue
                if max(times) >= day_ago:
                    recent[agent] = recent.get(agent, 0) + 1
                if not any(v.kind == "review" and v.vendor == vendor and (v.who == "Codex") == (agent == "codex")
                           and rules.names_head(v, sha) for v in verdicts):
                    outstanding[agent] = outstanding.get(agent, 0) + 1
            tag = MARK.format(kind="review", pr=number, head=sha)
            if any(tag in (c.get("body") or "") and c["created_at"] >= day_ago for c in bot):
                recent[rules.API_REVIEWER] = recent.get(rules.API_REVIEWER, 0) + 1
        self._load = (outstanding, recent)
        return self._load

    def merge(self, facts: rules.PrFacts, ctx: dict, d: rules.MergeDecision) -> str:
        """Merge at the head. Returns "" when merged (or in a dry run), else why not.

        A PR that changes .github/workflows merges with the merge token (J-056): the workflow token cannot. That
        merge pushes to trunk as a person's would, so the push starts the trunk proof and none is dispatched.
        GitHub refusing the token (401, 403, 404 or 405) raises MergeTokenRefused, which never names the token."""
        workflows = facts.touches_workflows
        if workflows and not self.merge_token:
            return "merge not done: it changes .github/workflows and no merge token is set"
        put = (f"repos/{self.gh.repo}/pulls/{facts.number}/merge",
               {"sha": facts.head_sha, "merge_method": "merge",
                "commit_title": f"Merge pull request #{facts.number} from {facts.head_ref} (switchboard)"},
               f"merge #{facts.number} at @{facts.head_sha[:8]}" + (" with the merge token" if workflows else ""))
        try:
            got = self.gh.write("PUT", *put, token=self.merge_token) if workflows else self.gh.write("PUT", *put)
        except GitHubError as err:
            code = http_status(err.detail)
            if workflows and code in MERGE_TOKEN_REFUSED:
                raise MergeTokenRefused(f"GitHub refused the merge token (HTTP {code}): {MERGE_TOKEN_ENV} needs "
                                        f"{MERGE_TOKEN_NEEDS} on {self.gh.repo}") from None
            raise
        if not self.gh.dry_run and not (got or {}).get("merged"):
            self.out(f"NOTE  #{facts.number} merge not done: {(got or {}).get('message', '')[:200]}")
            return f"merge not done: {(got or {}).get('message', '')[:200]}"
        if workflows:
            proof = "Its push to trunk starts the trunk proof."
        else:   # a merge by the workflow token starts no push workflow, so the trunk proof is dispatched
            self.gh.write("POST", f"repos/{self.gh.repo}/actions/workflows/{PROOF_WORKFLOW}/dispatches",
                          {"ref": rules.TRUNK}, f"dispatch the proof on {rules.TRUNK}")
            proof = "The trunk proof is dispatched."
        line = (f"Switchboard, {central(self.now)}: merged #{facts.number} at @{facts.head_sha[:8]} "
                f"({'; '.join(d.reasons)}; proof {ctx['proof_url']}). {proof} "
                f"The owner can overturn any merge.")
        self.gh.comment(facts.number, line, f"merged-note on #{facts.number}")
        if ctx.get("issue"):
            self.gh.comment(ctx["issue"]["number"], line, f"merged-note on job issue #{ctx['issue']['number']}")
        return ""

    # ---------- auto-open (J-047) ----------
    def prior_pr(self, branch: str) -> bool:
        """Has this branch ever had a PR (open, closed or merged)? The head filter names the branch."""
        owner = self.gh.repo.split("/")[0]
        return bool(self.gh.paged(f"repos/{self.gh.repo}/pulls?state=all&head={owner}:{branch}"))

    def open_pr(self, branch: str, sha: str, job: str, message: str) -> str:
        """Open the PR for a finished branch: title is the commit subject, body starts with the Job line.
        Returns "" when opened (or in a dry run), else why not."""
        subject, _, rest = message.partition("\n")
        body = f"Job: {job}\n\nOpened by the switchboard from {branch} @ {sha}"
        if rest.strip():
            body += f"\n\n{rest.strip()}"
        got = self.gh.write("POST", f"repos/{self.gh.repo}/pulls",
                            {"title": subject, "head": branch, "base": rules.TRUNK, "body": body},
                            f"open PR for {branch} @{sha[:8]} (job {job})")
        if self.gh.dry_run:
            self.out(f"DRY  open PR for {branch} @{sha[:8]} (job {job})")
            return ""
        if got and got.get("number"):
            self.out(f"OPENED  #{got['number']} {branch} @{sha[:8]} (job {job})")
            return ""
        why = f"PR not opened: {(got or {}).get('message', '')[:200]}"
        self.out(f"NOTE  {why} ({branch} @{sha[:8]})")
        return why

    def auto_pr_failure(self, branch: str, sha: str, job: str, reason: str, refused: bool = False) -> None:
        """One AUTO-PR-FAILED inbox item (J-051), keyed by branch and head: a head is listed once, however many
        sweeps fail on it. The branch and GitHub's words are outside text: in code spans, no verdict string."""
        where = f"{untrusted(branch)} head {sha[:8]}" + (f", {job}" if job else "")
        why = no_verdict(untrusted(" ".join(reason.split()), 160))
        if refused:
            text = (f"GitHub refused to open the PR ({why}). Cory turns on Settings → Actions → General → "
                    f"\"Allow GitHub Actions to create and approve pull requests\"; until then the integrator opens "
                    f"the PR by hand. This sweep's auto-open stopped here.")
        else:
            text = f"the auto-open failed ({why}); the next sweep tries again."
        self.auto_pr_failed += (InboxItem("AUTO-PR-FAILED", branch, sha, f"AUTO-PR-FAILED {where}: {text}"),)

    def auto_open_prs(self) -> int:
        """Open PRs for finished fb-bot/integrate-* branches with no PR yet (J-047).

        A branch qualifies when its head commit message carries a `Job: J-NNN` line and the proof on
        that exact head is green. At most MAX_AUTO_PRS per sweep, in branch-name order. A branch
        whose head already had a PR (open, closed or merged) is never opened again. Every failure on a
        branch is an AUTO-PR-FAILED item for this sweep's inbox (J-051); GitHub's refusal also stops this
        sweep's auto-open. Returns how many PRs were opened (or would be, in a dry run)."""
        self.auto_pr_failed = ()
        branches = [b for b in self.gh.paged(f"repos/{self.gh.repo}/branches")
                    if b["name"].startswith(AUTO_PR_PREFIX)]
        if not branches:
            return 0
        open_refs = {p["head"]["ref"] for p in self.gh.paged(f"repos/{self.gh.repo}/pulls?state=open")}
        opened = 0
        for b in sorted(branches, key=lambda x: x["name"]):
            if opened >= MAX_AUTO_PRS:
                break
            branch, sha, job = b["name"], b["commit"]["sha"], ""
            if branch in open_refs:
                continue
            try:
                if self.prior_pr(branch):
                    self.out(f"SKIP       {branch} @{sha[:8]}: head already had a PR")
                    continue
                message = self.gh.get(f"repos/{self.gh.repo}/commits/{sha}")["commit"]["message"]
                job = rules.job_of_pr(message)   # the Job: line, read from the commit message, not a PR body
                if not job:
                    continue
                proof, url, _, _ = self.proof(sha)
                if proof != "green":
                    self.out(f"SKIP       {branch} @{sha[:8]} ({job}): proof is {proof}")
                    continue
                why = self.open_pr(branch, sha, job, message)
                if why:
                    self.auto_pr_failure(branch, sha, job, why)
                    continue
                opened += 1
            except GitHubError as err:
                if AUTO_PR_REFUSED in err.detail:
                    self.out(f"NOTE  GitHub refused to open {branch} @{sha[:8]}: {err.detail[:200]} — a repo setting "
                             "Cory flips; stopping this sweep's auto-open, nothing worked around")
                    self.auto_pr_failure(branch, sha, job, err.detail, refused=True)
                    return opened
                self.out(f"ERROR       auto-open {branch}: {type(err).__name__}: {err}")
                self.auto_pr_failure(branch, sha, job, err.detail)
            except (RuntimeError, KeyError, ValueError) as err:
                self.out(f"ERROR       auto-open {branch}: {type(err).__name__}: {err}")
                self.auto_pr_failure(branch, sha, job, f"{type(err).__name__}: {err}")
        return opened

    # ---------- the integrator inbox (J-051) ----------
    def inbox(self, steered: list[Steered]) -> int:
        """After a full steer sweep, one comment on the inbox PR listing what needs the integrator and was not
        listed before. Each item's key (kind, PR or branch, head sha) goes in the comment's hidden marker, which
        carries the earlier keys forward too: a key is listed once, a new head is a new key. Nothing new, no
        comment. Returns how many items were posted (or printed, in a dry run)."""
        reported = self.inbox_reported()
        seen = set(reported)
        new: list[InboxItem] = []
        for it in self.inbox_items(steered, frozenset(reported)):
            if it.key not in seen:
                seen.add(it.key)
                new.append(it)
        if not new:
            self.out("INBOX       nothing new")
            return 0
        shown = new[:MAX_INBOX_LINES]
        keys = (reported + [it.key for it in shown])[-MAX_INBOX_KEYS:]
        body = inbox_body(shown, len(new) - len(shown), keys, central(self.now))
        self.out(body)
        self.gh.comment(INBOX_PR, body, f"inbox: {len(shown)} new item(s) on #{INBOX_PR}")
        return len(shown)

    def muse_keys(self, comments: list[dict]) -> list[str]:
        """The muse-<KIND> keys in the switch's own muse markers on these comments, oldest first."""
        keys: list[str] = []
        for c in comments:
            if (c.get("user") or {}).get("login") == rules.BOT_LOGIN:
                for m in MUSE_KEYS.finditer(c.get("body") or ""):
                    keys += [k for k in m.group(1).split(",") if k]
        return list(dict.fromkeys(keys))

    def muse(self, steered: list[Steered]) -> int:
        """Muse's items (J-052): one comment per fb-bot/ PR or branch with new items, labeled wake:muse so
        the bridge mails it. A muse-<KIND> key is sent once; a new head is a new key. At most
        MAX_MUSE_COMMENTS per sweep; the rest come next sweep. Returns how many comments were posted.
        A key counts as delivered only when its label write succeeded: the marker is edited into the
        comment after the label lands, so a failed label leaves no marker and the next sweep retries."""
        reported: set[str] = set()
        for s in steered:
            reported |= set(self.muse_keys(s.ctx.get("comments") or []))
        items = [it for it in self.muse_items(steered, frozenset(reported)) if it.line]
        if any(not it.target.startswith("#") for it in items):
            reported |= set(self.muse_keys(self.gh.paged(f"repos/{self.gh.repo}/issues/{BOARD_ISSUE}/comments")))
            for old in OLD_BOARDS:   # a key sent while an older board was live is still sent (#107 -> #340 re-sent them)
                reported |= set(self.muse_keys(self.bounded_comments(old, MAX_OLD_BOARD_PAGES)))
        seen, new = set(reported), []
        for it in items:
            if it.muse_key not in seen:
                seen.add(it.muse_key)
                new.append(it)
        if not new:
            self.out("MUSE        nothing new")
            return 0
        jobs = {f"#{s.facts.number}": s.ctx.get("job") or s.facts.head_ref for s in steered}
        by_target: dict[str, list[InboxItem]] = {}
        for it in new:
            by_target.setdefault(it.target, []).append(it)
        posted = 0
        for target, group in sorted(by_target.items()):
            if posted >= MAX_MUSE_COMMENTS:
                break
            where = BOARD_ISSUE if not target.startswith("#") else int(target[1:])
            name = jobs.get(target) or target
            kinds = " ".join(sorted({it.kind for it in group}, key=INBOX_ORDER.index))
            lines = [f"CLAUDE_TO_MUSE: {name} {kinds} on #{where} @ {group[0].head[:8]}", ""]
            for it in group:
                lines.append(f"- {it.line}")
                lines += [f"  {line}" for line in it.finding.split("\n")] if it.finding else []
            body = re.sub(r"@(?=\w)", "", "\n".join(lines))   # mentions would ping; "@ <sha>" is not one
            marked = f"{body}\n\n{MUSE_MARK.format(keys=','.join(it.muse_key for it in group))}"
            self.out(marked)
            # The marker goes in AFTER the label lands: a failed label leaves no marker, so the next
            # sweep retries. (Label first is not enough: the bridge could take it before the comment exists.)
            resp = self.gh.comment(where, body, f"muse: {len(group)} new item(s) on #{where}")
            self.gh.write("POST", f"repos/{self.gh.repo}/issues/{where}/labels", {"labels": [MUSE_LABEL]},
                          f"label #{where} {MUSE_LABEL} (the bridge emails Muse, then removes it)")
            cid = (resp if isinstance(resp, dict) else {}).get("id")
            if cid is not None:
                self.gh.write("PATCH", f"repos/{self.gh.repo}/issues/comments/{cid}", {"body": marked},
                              f"muse: mark #{where} delivered")
            posted += 1
        return posted

    def bounded_comments(self, number: int, pages: int) -> list[dict]:
        """An issue's comments, oldest first, at most `pages` pages of 100."""
        out: list[dict] = []
        for page in range(1, pages + 1):
            got = self.gh.get(f"repos/{self.gh.repo}/issues/{number}/comments?per_page=100&page={page}") or []
            out += got
            if len(got) < 100:
                break
        return out

    # ---------- the Muse mail pipe's health (bridge-health) ----------
    def pipe_health(self) -> list[str]:
        """Each full sweep: is the bridge moving mail both ways? (a) an open issue whose wake:muse label is older
        than PIPE_WAKE_STUCK: the bridge has not mailed it (outbound stuck). (b) a CLAUDE_TO_MUSE ask in the last
        PIPE_INBOUND_QUIET with no bridge post in that window (inbound stuck: Muse's mail is not reaching GitHub,
        which GitHub cannot show directly). Every finding is printed each run; the board gets one PIPE ALARM
        comment per incident. An outbound incident is new when every label now stuck went on after the last
        outbound alarm; an inbound one, when a bridge post came after the last inbound alarm (or that alarm is
        older than PIPE_LOOKBACK). Returns the finding lines."""
        last: dict[str, dt.datetime] = {}
        for c in self.gh.paged(f"repos/{self.gh.repo}/issues/{BOARD_ISSUE}/comments"):
            at = parse_time(c.get("created_at") or "")
            if (c.get("user") or {}).get("login") != rules.BOT_LOGIN or at is None:
                continue
            for m in PIPE_KINDS.finditer(c.get("body") or ""):
                for kind in filter(None, m.group(1).split(",")):
                    last[kind] = max(at, last.get(kind, at))
        found: list[str] = []
        new: list[str] = []
        stuck = self.wake_stuck()
        if stuck:
            found.append("OUTBOUND STUCK: `wake:muse` has sat on " + ", ".join(
                f"#{n} since {central(at)} ({hours(self.now - at)})" for n, at in stuck) +
                ". The bridge removes it within 5 min of mailing Muse, so these asks never reached her.")
            if "outbound" not in last or min(at for _, at in stuck) > last["outbound"]:
                new.append("outbound")
        quiet = self.inbound_quiet(last.get("inbound"))
        if quiet:
            asks, last_post, fresh = quiet
            seen = f"the last one was {central(last_post)}, {hours(self.now - last_post)} ago" if last_post else \
                "none was seen in the window read"
            found.append(f"INBOUND STUCK: {asks} CLAUDE_TO_MUSE ask(s) in the last {hours(PIPE_INBOUND_QUIET)} "
                         f"and no `{BRIDGE_POST.strip('*')}` post ({seen}). Muse's MUSE_TO_CLAUDE mail is not "
                         "reaching GitHub.")
            if fresh:
                new.append("inbound")
        for line in found:
            self.out(f"PIPE ALARM  {line}")
        if not found:
            self.out("PIPE        healthy")
        if new:
            body = "\n".join([f"{PIPE_HEADER} the Muse mail bridge looks down ({central(self.now)}).", ""] +
                             [f"- {plain(line)}" for line in found] +
                             ["", "The cause is in the bridge's Apps Script Executions log, which only Cory can open. "
                              "One alarm per incident; the switch prints the state every sweep.", "",
                              PIPE_MARK.format(kinds=",".join(new))])
            self.gh.comment(BOARD_ISSUE, body, f"pipe alarm ({', '.join(new)}) on #{BOARD_ISSUE}")
        return found

    def wake_stuck(self) -> list[tuple[int, dt.datetime]]:
        """Open issues and PRs whose wake:muse label has been on longer than PIPE_WAKE_STUCK: (number, labeled at).
        The label's age is read from the issue's events: the first `labeled` after the last `unlabeled`."""
        out: list[tuple[int, dt.datetime]] = []
        label = urllib.parse.quote(MUSE_LABEL, safe="")
        for it in self.gh.get(f"repos/{self.gh.repo}/issues?state=open&labels={label}&per_page={MAX_WAKE_ISSUES}") or []:
            number = int(it["number"])
            on: dt.datetime | None = None
            for e in self.gh.paged(f"repos/{self.gh.repo}/issues/{number}/events"):
                if (e.get("label") or {}).get("name") != MUSE_LABEL:
                    continue
                if e.get("event") == "unlabeled":
                    on = None
                elif e.get("event") == "labeled" and on is None:
                    on = parse_time(e.get("created_at") or "")
            if on is None:
                self.out(f"NOTE  #{number} has {MUSE_LABEL} but no labeled event was read")
            elif self.now - on > PIPE_WAKE_STUCK:
                out.append((number, on))
        return sorted(out, key=lambda x: x[1])

    def inbound_quiet(self, prev: dt.datetime | None) -> tuple[int, dt.datetime | None, bool] | None:
        """None while the bridge's inbound looks alive. Else (CLAUDE_TO_MUSE asks in the window, the newest bridge
        post read or None, is this a new incident). `prev` is the last inbound alarm: the read reaches back to it
        (at most PIPE_LOOKBACK), so a bridge post after it ends that incident. Newest comments first, so a
        bounded read keeps the window that decides."""
        start = self.now - PIPE_INBOUND_QUIET
        since = start if prev is None else max(min(start, prev), self.now - PIPE_LOOKBACK)
        stamp = since.strftime("%Y-%m-%dT%H:%M:%SZ")
        asks, posts = 0, []
        for c in self.gh.paged(f"repos/{self.gh.repo}/issues/comments?since={stamp}&sort=created&direction=desc"):
            at = parse_time(c.get("created_at") or "")
            if at is None:
                continue
            body = (c.get("body") or "").lstrip()
            if body.startswith(BRIDGE_POST):
                posts.append(at)
            elif body.startswith("CLAUDE_TO_MUSE:") and at >= start:
                asks += 1
        last_post = max(posts) if posts else None
        if not asks or (last_post is not None and last_post >= start):
            return None
        fresh = prev is None or prev < self.now - PIPE_LOOKBACK or (last_post is not None and last_post > prev)
        return asks, last_post, fresh

    def inbox_reported(self) -> list[str]:
        """The keys in the switch's own inbox markers, oldest first. Only the inbox PR's newest 200 comments are
        read: the newest marker carries every earlier key forward. A marker by any other login never counts."""
        count = int((self.gh.get(f"repos/{self.gh.repo}/issues/{INBOX_PR}") or {}).get("comments") or 0)
        last = max(1, -(-count // 100))
        keys: list[str] = []
        for page in range(max(1, last - 1), last + 1):
            for c in self.gh.get(f"repos/{self.gh.repo}/issues/{INBOX_PR}/comments?per_page=100&page={page}") or []:
                if c["user"]["login"] == rules.BOT_LOGIN:
                    for m in INBOX_KEYS.finditer(c.get("body") or ""):
                        keys += [k for k in m.group(1).split(",") if k]
        return list(dict.fromkeys(keys))

    def inbox_items(self, steered: list[Steered], reported: frozenset[str]) -> list[InboxItem]:
        """Every item that needs the integrator now, listed before or not, in INBOX_ORDER."""
        return self._items(steered, reported, "")

    def muse_items(self, steered: list[Steered], reported: frozenset[str]) -> list[InboxItem]:
        """Muse's items (J-052): the fb-bot/ items, keyed muse-<KIND>, in INBOX_ORDER."""
        return self._items(steered, reported, "muse-")

    def _items(self, steered: list[Steered], reported: frozenset[str], prefix: str) -> list[InboxItem]:
        """Both audiences' items from one read: prefix "" is the integrator's, "muse-" is Muse's (J-052)."""
        since = (self.now - INBOX_RUNS_WINDOW).strftime("%Y-%m-%dT%H:%M:%SZ")
        runs = self.gh.paged(f"repos/{self.gh.repo}/actions/workflows/{PROOF_WORKFLOW}/runs?created=%3E%3D{since}",
                             "workflow_runs")
        items: list[InboxItem] = []
        for s in steered:
            items += self.pr_inbox_items(s, runs, reported, prefix)
        items += self.branch_inbox_items(runs, reported, prefix)
        if not prefix:
            items += self.auto_pr_failed   # set by auto_open_prs earlier in the sweep
            items += self.ask_inbox_items(reported)
        return sorted(items, key=lambda it: INBOX_ORDER.index(it.kind))

    def ask_inbox_items(self, reported: frozenset[str]) -> list[InboxItem]:
        """TO-CLAUDE (J-052): an ask to the integrator in a comment on an open thread, once per comment id.
        The first line holds `To Claude` or `proposal`; the bridge's `**Muse (via email bridge): <subject>**`
        first line counts, since the subject is in it. Every agent posts as IssisX, so the author is read
        from the body: Claude's posts end with its footer, the switchboard's own are its login, Codex's
        start with `<!-- codex`."""
        since = (self.now - INBOX_RUNS_WINDOW).strftime("%Y-%m-%dT%H:%M:%SZ")
        open_threads = {int(i["number"]) for i in
                        self.gh.paged(f"repos/{self.gh.repo}/issues?state=open")}
        out: list[InboxItem] = []
        for c in self.gh.paged(f"repos/{self.gh.repo}/issues/comments?since={since}"):
            body = c.get("body") or ""
            if body.lstrip().startswith("<!-- codex"):
                continue
            if (c.get("user") or {}).get("login") == rules.BOT_LOGIN:
                continue
            if "Generated by [Claude Code]" in body:
                continue
            cid = c.get("id")
            if cid is None:
                continue
            try:
                number = int((c.get("issue_url") or "").rsplit("/", 1)[1])
            except (ValueError, IndexError):
                continue
            if number not in open_threads:
                continue
            first = (body.split("\n", 1)[0] or "").strip()
            low = first.lower()
            if "to claude" not in low and "proposal" not in low:
                continue
            key = inbox_key("TO-CLAUDE", f"#{number}", str(cid))
            if key in reported:
                continue
            out.append(InboxItem("TO-CLAUDE", f"#{number}", str(cid),
                                 f"TO-CLAUDE #{number} {no_verdict(plain(first[:100]))} "
                                 f"{c.get('html_url') or ''}"))
        return out

    def pr_inbox_items(self, s: Steered, runs: list[dict], reported: frozenset[str], prefix: str = "") -> list[InboxItem]:
        """One PR's items, for one audience (J-052): prefix "" is the integrator's, "muse-" is Muse's.
        (a) MERGE-BY-HAND, (b) NOT-MERGED, (c) RED, RED-TWICE and RED-STUCK, (d) FIX, (f) QUIET when nothing
        else is. Each item goes to its owner: MERGE-BY-HAND and AUTO-PR-FAILED to the integrator;
        NOT-MERGED and RED-TWICE to both on an fb-bot/ PR; the rest to the branch's owner."""
        f, ctx, d, now = s.facts, s.ctx, s.decision, self.now
        if f.number == INBOX_PR or f.head_ref == INBOX_BRANCH:
            return []
        mine = prefix == "muse-"          # this call is for Muse (fb-bot/ items), not the integrator
        owner = owner_of(f.head_ref)
        target = f"#{f.number}"
        where = (f"[#{f.number}](https://github.com/{self.gh.repo}/pull/{f.number}) {untrusted(f.head_ref)} "
                 f"head {f.head_sha[:8]}" + (f", {ctx['job']}" if ctx.get("job") else ""))
        proof_link = f"[proof]({ctx['proof_url']})" if ctx.get("proof_url") else "proof"

        def item(kind: str, text: str, dest: str = "claude", finding: str = "") -> InboxItem:
            return InboxItem(kind, target, f.head_sha, f"{kind} {where}: {text}", dest, finding)

        def kept(it: InboxItem) -> bool:
            return it.dest in ("muse", "both") if mine else it.dest in ("claude", "both")

        out: list[InboxItem] = []
        author = self.team.vendor_of_branch(f.head_ref)
        lands = sorted((v for v in f.verdicts if v.kind == "review" and v.verdict == "LAND"
                        and rules.names_head(v, f.head_sha) and rules.land_counts(v, author, self.team)),
                       key=lambda v: v.created_at)
        ready = f.proof == "green" and bool(lands) and d.action in ("HUMAN-MERGE", "WAIT", "MERGE")
        never = f.never_edit or (bool(f.rule_check_block) and "/integrate-protected-" not in f.head_ref)
        if ready and (d.action == "HUMAN-MERGE" or (d.action == "WAIT" and never)):
            out.append(item("MERGE-BY-HAND", f"green, LAND by {lands[-1].who}, but the switchboard will not merge it: "
                                             f"{plain('; '.join(d.reasons))}"))
        elif ready and (d.action == "WAIT" or s.merge_note):   # a MERGE that merged (or a dry run's) is no item
            times = [t for t in (parse_time(ctx.get("proof_done_at") or ""), parse_time(lands[0].created_at)) if t]
            if not times or now - max(times) >= INBOX_READY_AFTER:
                why = s.merge_note or "; ".join(d.reasons)
                out.append(item("NOT-MERGED", f"green, LAND by {lands[-1].who}, still open a sweep later: {plain(why)}",
                                "both" if owner == "muse" else "claude"))
        if d.action == "FIX" and owner:
            out.append(item("FIX", plain("; ".join(d.reasons)), owner, self.fix_finding(f, ctx)))
        if d.action == "FIX-CI":
            jobs = red_jobs(f.proof)
            both = "both" if owner == "muse" else "claude"
            runs_here = sorted((r for r in runs if r.get("head_branch") == f.head_ref), key=lambda r: r["created_at"], reverse=True)
            ids = [r["id"] for r in runs_here]
            if inbox_key(prefix + "RED-TWICE", target, f.head_sha) in reported:   # listed at this head: no need to read it again
                out.append(InboxItem("RED-TWICE", target, f.head_sha, "", both))
            elif ctx.get("run_id") in ids and ids.index(ctx["run_id"]) + 1 < len(runs_here):
                before = self.run_state(runs_here[ids.index(ctx["run_id"]) + 1])[0]
                failed = [j for j in jobs if j in red_jobs(before)]
                if failed:
                    out.append(item("RED-TWICE", f"{plain(', '.join(failed))} failed on two proofs in a row ({proof_link})", both))
            if owner == "muse":
                out.append(item("RED", f"proof red on {plain(', '.join(jobs))} ({proof_link})", "muse"))
            on_head = [parse_time(r["created_at"]) for r in runs_here if r.get("head_sha") == f.head_sha]
            head_since = min(on_head) if on_head else parse_time(f.head_committed_at)
            if head_since and now - head_since >= INBOX_RED_STUCK and owner:
                out.append(item("RED-STUCK", f"proof red on {plain(', '.join(jobs))}, no new head for "
                                             f"{hours(now - head_since)} ({proof_link})", owner))
        if not [it for it in out if kept(it)]:   # QUIET, as tools/board/sweep.py: FIX, FIX-CI or DISPATCH with the PR untouched for QUIET_HOURS
            action = ("FIX-CI" if f.proof.startswith("red") else
                      "FIX" if d.action == "FIX" or rules.open_fixes(f.verdicts, f.head_sha, f.head_committed_at, "review") else
                      "DISPATCH" if f.proof == "none" else "")
            idle = parse_time(ctx.get("updated_at") or "")
            if action and idle and (now - idle).total_seconds() >= QUIET_HOURS * 3600 and owner:
                out.append(item("QUIET", f"no activity on the PR for {hours(now - idle)} while it is {action}", owner))
        return [it for it in out if kept(it)]

    def fix_finding(self, f: rules.PrFacts, ctx: dict) -> str:
        """The newest open FIX verdict's first 600 chars and its link, for Muse's comment (J-052)."""
        fixes = rules.open_fixes(f.verdicts, f.head_sha, f.head_committed_at, "review")
        if not fixes:
            return ""
        v = max(fixes, key=lambda x: x.created_at)
        for c in ctx.get("comments") or []:
            if c.get("created_at") == v.created_at and (c.get("user") or {}).get("login") == v.author_login:
                text = no_verdict(plain((c.get("body") or "").strip()[:MAX_MUSE_FINDING]))
                link = c.get("html_url") or ""
                return f"{text}\n{link}" if link else text
        return ""

    def branch_inbox_items(self, runs: list[dict], reported: frozenset[str], prefix: str = "") -> list[InboxItem]:
        """(e) RED-BRANCH: an fb-bot/ or claude/integrate-* branch with no open PR whose head's proof went red an
        hour ago or more. Only heads proved in the run window are looked at, and only up to MAX_INBOX_BRANCHES.
        Each item goes to the branch's owner (J-052)."""
        from urllib.parse import quote
        mine = prefix == "muse-"
        open_refs = {p["head"]["ref"] for p in self.gh.paged(f"repos/{self.gh.repo}/pulls?state=open")}
        newest: dict[str, dict] = {}
        for r in runs:
            name = r.get("head_branch") or ""
            if name.startswith(INBOX_BRANCH_PREFIXES) and name != INBOX_BRANCH and name not in open_refs:
                if name not in newest or r["created_at"] > newest[name]["created_at"]:
                    newest[name] = r
        out: list[InboxItem] = []
        read = 0
        for name in sorted(newest):
            run = newest[name]
            head, done = run.get("head_sha") or "", parse_time(run.get("updated_at") or "")
            if (run.get("status") == "completed" and run.get("conclusion") == "success") or not done \
                    or (self.now - done) < INBOX_BRANCH_RED or inbox_key(prefix + "RED-BRANCH", name, head) in reported:
                continue
            if read >= MAX_INBOX_BRANCHES:
                self.out(f"INBOX       {MAX_INBOX_BRANCHES} branches read; the rest wait for the next sweep")
                break
            read += 1
            try:
                now_head = self.gh.get(f"repos/{self.gh.repo}/branches/{quote(name, safe='/')}")["commit"]["sha"]
            except GitHubError as err:
                if "HTTP 404" in err.detail:   # deleted since its proof ran
                    continue
                raise
            if now_head != head:   # a newer head: its own proof decides, next sweep
                continue
            proof, url, _, done_at = self.run_state(run)
            done = parse_time(done_at) or done
            if proof.startswith("red") and (self.now - done) >= INBOX_BRANCH_RED:
                out.append(InboxItem("RED-BRANCH", name, head,
                                     f"RED-BRANCH {untrusted(name)} head {head[:8]}: proof red on "
                                     f"{plain(', '.join(red_jobs(proof)))}, no PR for {hours(self.now - done)} ([proof]({url}))",
                                     owner_of(name)))
        return [it for it in out if (mine and it.dest in ("muse", "both")) or (not mine and it.dest in ("claude", "both"))]

    def ask_thread(self, facts: rules.PrFacts, ctx: dict) -> tuple[int, list[dict]]:
        """Where review asks live (the job issue, else the PR) and its comments."""
        where = ctx["issue"]["number"] if ctx.get("issue") else facts.number
        comments = ctx["comments"] if where == facts.number else self.gh.paged(f"repos/{self.gh.repo}/issues/{where}/comments")
        return where, comments

    def ask_review(self, facts: rules.PrFacts, ctx: dict, agent: str, why: str) -> None:
        where, comments = self.ask_thread(facts, ctx)
        if agent == "codex":   # the Codex app answers "@codex review" on the PR itself
            where, comments = facts.number, ctx["comments"]
        tag = MARK.format(kind=f"ask-{agent}", pr=facts.number, head=facts.head_sha)
        if self.marked(comments, f"ask-{agent}", facts.number, facts.head_sha):
            self.out(f"ASKED       #{facts.number} review already asked of {agent} on #{where}")
            return
        name = self.team.agent_name.get(agent, agent)
        job = ctx["job"] or f"PR #{facts.number}"
        if agent == "codex":
            self.gh.comment(where, f"@codex review\n\nSwitchboard: {job}, head {facts.head_sha[:8]}. {why}\n\n{tag}",
                            f"ask Codex to review #{facts.number}")
            return
        head = {"muse": f"CLAUDE_TO_MUSE: {job} review #{facts.number} @ {facts.head_sha[:8]}",
                "grok": f"To Grok: {job} review #{facts.number} @ {facts.head_sha[:8]}",
                "chatgpt": f"To ChatGPT: {job} review #{facts.number} @ {facts.head_sha[:8]}"}.get(agent) or \
               f"**Switchboard → {name}:** review #{facts.number} @ {facts.head_sha[:8]} ({job})"
        body = (f"{head}\n\n{why}Proof is green on the head ({ctx['proof_url']}). Review it with "
                f"`03_EXECUTION/REVIEW_CHECKLIST.md` and post one verdict on #{facts.number} whose first line is\n"
                f"`**{name} (review of #{facts.number} @ {facts.head_sha}): LAND**` (or FIX with command, input, "
                f"expected, observed; a FIX without them counts as a NOTE).\n\n"
                f"{self.review_facts(facts, ctx)}\n\n{tag}")
        self.gh.comment(where, body, f"ask {name} to review #{facts.number} on #{where}")
        if agent == "muse":
            self.gh.write("POST", f"repos/{self.gh.repo}/issues/{where}/labels", {"labels": [MUSE_LABEL]},
                          f"label #{where} {MUSE_LABEL} (the bridge emails Muse, then removes it)")

    def escalate_review(self, facts: rules.PrFacts, ctx: dict, author: str) -> None:
        """An ask the switchboard made on this head that has had no verdict for review.fallback_after_hours goes
        to the next agent in review.fallback_order, once per head (each ask has its own marker). The caller
        only gets here while needs_review holds: any verdict on the head, from anyone, ends this."""
        where, comments = self.ask_thread(facts, ctx)
        asked: list[tuple[str, str]] = []   # (created_at, agent) of the bot's own asks on this head
        for agent in self.team.agent_vendor:
            tag = MARK.format(kind=f"ask-{agent}", pr=facts.number, head=facts.head_sha)
            asked += [(c["created_at"], agent) for c in comments
                      if tag in (c.get("body") or "") and c["user"]["login"] == rules.BOT_LOGIN]
        if not asked:
            return   # the first ask is made by the caller; its clock starts when it is posted
        last_at, last = max(asked)
        hours = self.team.fallback_after_hours
        if not rules.ask_is_overdue(last_at, self.now.strftime("%Y-%m-%dT%H:%M:%SZ"), hours):
            return
        nxt = rules.next_fallback([a for _, a in asked], author, self.team)
        if not nxt:
            self.out(f"NOFALLBACK  #{facts.number} {last} has not answered in {hours:g} hours and nobody is left to ask")
            return
        self.out(f"FALLBACK    #{facts.number} {last} asked {last_at}, no verdict in {hours:g} hours: asking {nxt}")
        self.ask_review(facts, ctx, nxt, f"{self.team.agent_name.get(last, last)} has not answered in {hours:g} hours, so you are asked instead. ")

    def owner_labels(self, number: int, just_added: str) -> None:
        issue = self.gh.get(f"repos/{self.gh.repo}/issues/{number}")
        owners = [l["name"] for l in issue["labels"] if l["name"].startswith("owner:")]
        if len(owners) < 2:
            return
        if just_added in owners:
            keep_order = [o for o in owners if o != just_added] + [just_added]
        else:  # the hourly sweep: order by when each label was added
            events = self.gh.paged(f"repos/{self.gh.repo}/issues/{number}/events")
            added = {}
            for e in events:
                if e.get("event") == "labeled" and e.get("label", {}).get("name") in owners:
                    added[e["label"]["name"]] = e["created_at"]
            keep_order = sorted(owners, key=lambda o: added.get(o, ""))
        for extra in rules.extra_owner_labels(keep_order):
            self.out(f"OWNER       #{number} has {', '.join(keep_order)}: removing {extra}")
            self.gh.write("DELETE", f"repos/{self.gh.repo}/issues/{number}/labels/{extra}", None, f"remove {extra} from #{number}")
            self.gh.comment(number, f"Switchboard, {central(self.now)}: one owner per job. `{keep_order[0]}` holds "
                                    f"#{number}, so `{extra}` was removed. Free the job first to hand it over.")

    def sweep_owner_labels(self) -> None:
        for it in self.jobs().values():
            if sum(1 for l in it["labels"] if l["name"].startswith("owner:")) > 1:
                self.owner_labels(it["number"], "")

    # ---------- ask (paid) ----------
    def ask_pr(self, facts: rules.PrFacts, ctx: dict) -> None:
        author = self.team.vendor_of_branch(facts.head_ref)
        if self.stale(facts):
            return
        if rules.needs_review(facts, author, self.team) and self.codex_pending(facts, ctx):
            self.out(f"WAIT-CODEX  #{facts.number} Codex is reviewing @{facts.head_sha[:8]}: no automatic review yet")
            api_turn = False
        elif self.team.pool:
            api_turn = rules.needs_review(facts, author, self.team) and self.pooled_reviewer(facts, ctx, author)[1] == rules.API_REVIEWER
        else:
            api_turn = rules.needs_review(facts, author, self.team) and rules.pick_reviewer(author, facts.needs, self.team).route == "claude-api"
        # J-056: a workflow-file PR merges only on a clean, signed automatic review of its head, so it gets one even
        # when Codex already reviewed that head and no other review is asked. Every automatic review of such a head is
        # its security pass, whichever route asked for it (Codex on #297: the API's own turn ran the gameplay prompt).
        security = facts.touches_workflows
        signed = security and bool(self.review_key)   # only a mark the switch signed stops a second security pass
        if api_turn or rules.needs_security_pass(facts):
            if self.marked(ctx["comments"], "review", facts.number, facts.head_sha, signed=signed):
                self.out(f"REVIEWED    #{facts.number} automatic review already posted for @{facts.head_sha[:8]}")
            elif not api_turn:
                self.out(f"SECURITY    #{facts.number} changes .github/workflows: the automatic review is its security pass")
                self.review(facts, ctx, author, security=True)
            else:
                self.review(facts, ctx, author, security=security)
                if self.team.pool:
                    self.note_assigned(rules.API_REVIEWER)
        if rules.needs_eye(facts):
            if self.marked(ctx["comments"], "eye", facts.number, facts.head_sha):
                self.out(f"LOOKED      #{facts.number} eye already posted for @{facts.head_sha[:8]}")
            else:
                self.eye(facts, ctx)

    def review(self, facts: rules.PrFacts, ctx: dict, author: str, security: bool = False) -> None:
        """The automatic Claude review of one head. `security`: it is the security pass of a workflow-file PR
        (J-056), so when it cannot run no other reviewer is asked: no one else can stand in for it."""
        import claude_calls
        key = f"review#{facts.number}@{facts.head_sha[:8]}"
        self.out(f"REVIEW      #{facts.number} author={author}: automatic Claude review")
        # The diff of the head this review is labeled with, never the PR's head of the moment (J-056): a push between
        # the sweep's read and this one must not put a clean review of one commit under another's mark. Three dots:
        # from the merge base, as the PR's own diff is (the two read byte for byte the same on #297).
        base = urllib.parse.quote(facts.base_ref, safe="/")
        diff = "" if self.gh.dry_run else self.gh.get_text(f"repos/{self.gh.repo}/compare/{base}...{facts.head_sha}",
                                                           "application/vnd.github.diff")
        if len(diff) > MAX_DIFF_CHARS:
            self.review_failed(facts, ctx, author, security, f"The diff is {len(diff)} characters, too large for the automatic review. ")
            return
        checklist = (self.root / "03_EXECUTION/REVIEW_CHECKLIST.md").read_text(encoding="utf-8")
        # The security pass judges against SECURITY.md with its own prompt (Codex on #297): the gameplay prompt's
        # LAND would let a secret leak through, since it blocks only physics, gameplay, correctness and tests.
        guidelines = [claude_calls.text_block("<security_guidelines>\n"
                                              + (self.root / "SECURITY.md").read_text(encoding="utf-8")
                                              + "\n</security_guidelines>")] if security else []
        system = claude_calls.SECURITY_REVIEW_SYSTEM if security else claude_calls.REVIEW_SYSTEM
        content = guidelines + [claude_calls.text_block(f"<checklist>\n{checklist}\n</checklist>"),
                   claude_calls.text_block(f"<pr number=\"{facts.number}\" head=\"{facts.head_sha}\" branch=\"{facts.head_ref}\">\n"
                                           f"<title>{ctx['title']}</title>\n<body>\n{ctx['body'][:20000]}\n</body>\n</pr>"),
                   claude_calls.text_block(f"<diff>\n{diff}\n</diff>")]
        # The security pass has its own budget line (max effort); ordinary reviews run at high (Cory, 11-08).
        call = "security_review" if security else "review"
        res = self.paid("review", ctx["job"], call, system, content, claude_calls.REVIEW_SCHEMA,
                        self.job_cap(ctx), key)
        if res is None:   # refused by the budget, or a dry run: the next run tries again
            return
        if isinstance(res, str) or res.data is None:
            why = {"too-large": "The diff is too large for the automatic review. ",
                   "error": "The automatic review failed to run. "}.get(res) if isinstance(res, str) else \
                  f"The automatic review stopped ({res.stop_reason}). "
            self.review_failed(facts, ctx, author, security, why)
            return
        d = res.data
        verdict = d["verdict"]
        # The whole head sha (J-056): a workflow-file PR's gates never read an 8-character prefix.
        lines = [f"**Claude (automatic review of #{facts.number} @ {facts.head_sha}): {verdict}**", "", d["summary"]]
        if verdict == "FIX":
            lines += [""] + [f"{k}: {d['repro'][k]}" for k in ("command", "input", "expected", "observed")]
        if d["notes"]:
            lines += ["", "Notes (never blocking):"] + [f"- {n}" for n in d["notes"][:12]]
        lines += ["", f"_{res.model} through the API; {central(self.now)}._",
                  MARK.format(kind="review", pr=facts.number, head=facts.head_sha)]
        if self.review_key:   # the MAC that tells this review from a forged one (J-056); without the key, unsigned
            lines.append(review_sig(self.review_key, self.gh.repo, facts.number, facts.head_sha, verdict))
        self.gh.comment(facts.number, "\n".join(lines), f"post automatic review {verdict} on #{facts.number}")

    def review_failed(self, facts: rules.PrFacts, ctx: dict, author: str, security: bool, why: str) -> None:
        """The automatic review could not give a verdict on this head. As the security pass (J-056): one note on the
        PR with the review's mark, so it is not paid for again on this head, and the PR stays merge-by-hand.
        Otherwise the next reviewer is asked, with the api-out mark."""
        if security:
            self.gh.comment(facts.number, f"Switchboard, {central(self.now)}: the security pass (the automatic Claude "
                                          f"review of a .github/workflows change) gave no verdict on {facts.head_sha[:8]}. "
                                          f"{why}The switchboard will not merge this head; it is listed to merge by hand.\n\n"
                                          + MARK.format(kind="review", pr=facts.number, head=facts.head_sha)
                                          + ("\n" + review_sig(self.review_key, self.gh.repo, facts.number, facts.head_sha, "NONE")
                                             if self.review_key else ""),
                            f"security pass failed on #{facts.number}")
            return
        self.ask_review(facts, ctx, self.api_fallback(facts, ctx, author),
                        why + MARK.format(kind="api-out", pr=facts.number, head=facts.head_sha) + " ")

    def eye(self, facts: rules.PrFacts, ctx: dict) -> None:
        import claude_calls
        key = f"eye#{facts.number}@{facts.head_sha[:8]}"
        self.out(f"EYE         #{facts.number}: Claude vision on the {self.looks['artifact_prefix']}* screenshots")
        if self.gh.dry_run:
            self.paid("eye", ctx["job"], "eye", claude_calls.EYE_SYSTEM, [], claude_calls.EYE_SCHEMA, "", key)
            return
        frames = self.frames(ctx["run_id"])
        content: list = []
        for name, s in self.looks["sets"].items():
            rubric = (self.root / s["rubric"]).read_text(encoding="utf-8")
            content.append(claude_calls.text_block(f"<set name=\"{name}\">\n<rubric>\n{rubric}\n</rubric>\n</set>"))
            for f in s["frames"]:
                if f not in frames:
                    self.out(f"NOTE  #{facts.number} eye: frame {f} is missing from the artifact")
                    return
                content += [claude_calls.text_block(f"Set {name}, frame {f}:"), claude_calls.png_block(frames[f])]
        res = self.paid("eye", ctx["job"], "eye", claude_calls.EYE_SYSTEM, content, claude_calls.EYE_SCHEMA,
                        self.job_cap(ctx), key)
        if not isinstance(res, claude_calls.Result) or res.data is None:
            self.out(f"NOTE  #{facts.number} eye gave no judgment ({res if isinstance(res, str) else getattr(res, 'stop_reason', 'refused')})")
            return
        sets = res.data["sets"]
        expected = set(self.looks["sets"])
        judged = {s["set"] for s in sets}
        # A set's known defect, owned by a still-open job, blocks only that job's PR (J-068).
        overall, waived = rules.eye_overall(sets, self.looks["sets"], ctx.get("job") or "", self.jobs())
        lines = [f"**Claude eye (look of #{facts.number} @ {facts.head_sha[:8]}): {overall}**", ""]
        for s in sets:
            known = f" (known defect of {', '.join(waived[s['set']])}: shown, not blocking this PR)" \
                if s["verdict"] != "LAND" and s["set"] in waived else ""
            lines.append(f"- {s['set']}: {s['verdict']}{known}; rubric: \"{s['rubric_line']}\"; frame `{s['frame']}`. {s['reason']}")
        if judged != expected:
            lines.append(f"- sets not judged: {', '.join(sorted(expected - judged)) or '-'}")
        if overall == "FIX":
            lines += ["", f"command: download the `{self.looks['artifact_prefix']}*` artifact of {ctx['proof_url']}",
                      "input: the frame named above", "expected: the rubric line holds", "observed: it does not (reason above)"]
        lines += ["", f"_{res.model} through the API; {central(self.now)}. Taste stays the owner's._",
                  MARK.format(kind="eye", pr=facts.number, head=facts.head_sha)]
        self.gh.comment(facts.number, "\n".join(lines), f"post eye {overall} on #{facts.number}")

    def frames(self, run_id: int) -> dict[str, bytes]:
        """The named PNGs from the proof run's audits artifact, read by name (never extracted to disk)."""
        arts = self.gh.get(f"repos/{self.gh.repo}/actions/runs/{run_id}/artifacts?per_page=100")["artifacts"]
        art = next((a for a in arts if a["name"].startswith(self.looks["artifact_prefix"]) and not a["expired"]), None)
        if not art:
            raise RuntimeError(f"run {run_id} has no {self.looks['artifact_prefix']}* artifact")
        if art["size_in_bytes"] > MAX_ARTIFACT_BYTES:
            raise RuntimeError(f"artifact {art['name']} is {art['size_in_bytes']} bytes, above {MAX_ARTIFACT_BYTES}")
        blob = self.gh.get_bytes(f"repos/{self.gh.repo}/actions/artifacts/{art['id']}/zip")
        wanted = {f for s in self.looks["sets"].values() for f in s["frames"]}
        out = {}
        with zipfile.ZipFile(io.BytesIO(blob)) as z:
            for info in z.infolist():
                if info.filename in wanted and info.file_size <= MAX_IMAGE_BYTES:
                    data = z.read(info)
                    if data[:8] == b"\x89PNG\r\n\x1a\n":
                        out[info.filename] = data
        return out

    def orchestrate(self, number: int) -> None:
        """Design 2.3 step 1, behind `needs:orchestrator` and budget.yml (off in v0)."""
        import claude_calls
        issue = self.gh.get(f"repos/{self.gh.repo}/issues/{number}")
        labels = {l["name"] for l in issue["labels"]}
        if not {"job", "needs:orchestrator"} <= labels:
            return
        comments = self.gh.paged(f"repos/{self.gh.repo}/issues/{number}/comments")
        if self.start_blocked(comments, number):
            self.out(f"ORCH        #{number} already handled")
            return
        b = self.budget
        job = rules.job_of_title(issue["title"])
        tag = MARK.format(kind="orchestrator", pr=number, head="-")
        if not (b.orchestrator_enabled and b.orchestrator_agent_id and b.orchestrator_environment_id):
            self.out(f"ORCH        #{number}: off in budget.yml (orchestrator.enabled, agent_id, environment_id); not started")
            self.gh.comment(number, f"Switchboard, {central(self.now)}: `needs:orchestrator` is set, but the orchestrator "
                                    f"is off in `work/budget.yml` until step C defines the hosted agent. Nothing was started.\n\n{tag}")
            return
        if not os.environ.get("AGENT_GITHUB_TOKEN") and not self.gh.dry_run:
            self.out(f"ORCH        #{number}: no AGENT_GITHUB_TOKEN in this job; not started")
            self.gh.comment(number, f"Switchboard, {central(self.now)}: `needs:orchestrator` is set, but the workflow has no "
                                    f"`AGENT_GITHUB_TOKEN` secret, so the API agent cannot push. Nothing was started.\n\n{tag}")
            return
        key = f"orchestrator#{number}"
        v = self.gate("orchestrator", job, b.orchestrator_session_usd, self.job_cap({"issue": issue}), key)
        if not v.ok:
            self.out(f"REFUSED orchestrator #{number}: {v.reason}")
            return
        if self.gh.dry_run:
            self.out(f"DRY  beta.sessions.create for #{number} with a ${b.orchestrator_session_usd} budget ({v.reason})")
            return
        client = self.claude()   # before the reserve: no key, no reserve
        run = uuid.uuid4().hex[:12]
        self.append(money.record(self.now, run, "reserve", "orchestrator", job, b.orchestrator_session_usd, key), f"reserve {key}")
        message = (f"Job {job}, GitHub issue #{number} in {self.gh.repo}: {issue['title']}\n\n{(issue.get('body') or '')[:30000]}\n\n"
                   f"Read work/tasks/api-agent.md first, then the issue's newest comments, then do the job.")
        try:
            session = claude_calls.start_session(client, b.orchestrator_agent_id, b.orchestrator_environment_id,
                                                 f"{job} #{number}", message, b.orchestrator_session_usd,
                                                 f"https://github.com/{self.gh.repo}", os.environ.get("AGENT_GITHUB_TOKEN", ""),
                                                 rules.TRUNK, b.orchestrator_vault_id,
                                                 {"switchboard_key": key, "run": run})
        except claude_calls.SessionStartRejected as err:
            # A 4xx: the server refused the request, so no session exists and the reserve is released at once.
            self.out(f"FAILED orchestrator #{number}: {err}")
            self.append(money.record(self.now, run, "end", "orchestrator", job, Decimal(0),
                                     f"{key} start rejected HTTP {err.status}"), f"end {key} (start rejected)")
            self.gh.comment(number, f"Switchboard, {central(self.now)}: the orchestrator session was not started. "
                                    f"`beta.sessions.create` answered HTTP {err.status} ({err.error_class}). No session "
                                    f"exists, and the reserve is released. Fix the cause, then re-add `needs:orchestrator`."
                                    f"\n\n{MARK.format(kind='orchestrator-failed', pr=number, head=run)}")
            return
        except claude_calls.SessionStartUnknown as err:
            # No usable answer (no connection, or a 5xx): a session may exist. The reserve stands, and the issue
            # is marked handled so a re-label cannot start a second session beside it; a person looks in the Console.
            self.out(f"FAILED orchestrator #{number}: {err}")
            heard = f"answered HTTP {err.status}" if err.status else "got no answer"
            self.gh.comment(number, f"Switchboard, {central(self.now)}: `beta.sessions.create` {heard} "
                                    f"({', '.join(err.classes)}), so a session may exist. The ${b.orchestrator_session_usd} "
                                    f"reserve stays on the ledger (run `{run}`) and no second session is started. The next "
                                    f"sweeps look for it by its metadata, and write the reserve off after "
                                    f"{int(GIVE_UP_AFTER.total_seconds() // 60)} minutes if it never appears.\n\n{tag}\n"
                                    f"{UNKNOWN_MARK.format(run=run, at=self.now.strftime('%Y-%m-%dT%H:%M:%SZ'))}")
            return
        except claude_calls.ClaudeCallError as err:
            # Refused locally before any request was sent: no session exists.
            self.out(f"FAILED orchestrator #{number}: {err}")
            self.append(money.record(self.now, run, "end", "orchestrator", job, Decimal(0),
                                     f"{key} start refused locally"), f"end {key} (start refused)")
            self.gh.comment(number, f"Switchboard, {central(self.now)}: the orchestrator session was not started: {err}. "
                                    f"No session exists, and the reserve is released.\n\n"
                                    f"{MARK.format(kind='orchestrator-failed', pr=number, head=run)}")
            return
        self.out(f"ORCH        #{number}: session {session} started on run {run}")
        self.gh.comment(number, self.started_comment(number, session, run, b.orchestrator_session_usd, ""))

    def started_comment(self, number: int, session: str, run: str, budget: Decimal, note: str) -> str:
        return (f"Switchboard, {central(self.now)}: orchestrator session `{session}` started{note} with a ${budget} budget. "
                f"Trace: {CONSOLE_SESSION.format(session=session)}\n\n"
                f"{MARK.format(kind='orchestrator', pr=number, head='-')}\n{RUN_MARK.format(run=run)}")

    def start_blocked(self, comments: list[dict], number: int) -> bool:
        """Has the switchboard already handled this issue's start? Its own orchestrator-marked comment says so,
        except an unknown-start comment the watcher has since written off (a later `orchestrator-cleared` for its run)."""
        tag = MARK.format(kind="orchestrator", pr=number, head="-")
        mine = [c.get("body") or "" for c in comments if c["user"]["login"] == rules.BOT_LOGIN]
        for body in mine:
            if tag not in body:
                continue
            m = UNKNOWN_RUN.search(body)
            if m and any(MARK.format(kind="orchestrator-cleared", pr=number, head=m.group(1)) in b for b in mine):
                continue
            return True
        return False

    def reconcile_unknown(self, issue: dict, comments: list[dict]) -> None:
        """A start whose answer was lost (a 5xx or no connection) left an unknown-start comment. Look once per
        sweep for the session by its metadata: found, post the start comment so it is watched as usual; not
        found 30 minutes after the attempt, write the reserve off at $0 and clear the block so a relabel can retry."""
        import claude_calls
        number, b = issue["number"], self.budget
        mine = [c.get("body") or "" for c in comments if c["user"]["login"] == rules.BOT_LOGIN]
        pending = [m.groups() for m in (UNKNOWN_RUN.search(x) for x in mine) if m]
        if not pending:
            return
        run, at = pending[-1]
        if any(MARK.format(kind="orchestrator-cleared", pr=number, head=run) in x for x in mine):
            return
        if self.gh.dry_run and not os.environ.get("ANTHROPIC_API_KEY"):
            self.out(f"DRY  WATCH #{number}: would look for the session of run {run}")
            return
        key = f"orchestrator#{number}"
        attempt = dt.datetime.fromisoformat(at.replace("Z", "+00:00"))
        since = (attempt - dt.timedelta(minutes=2)).strftime("%Y-%m-%dT%H:%M:%SZ")
        search = claude_calls.find_session(self.claude(), b.orchestrator_agent_id, since, {"switchboard_key": key, "run": run})
        if search.session:
            self.out(f"WATCH       #{number} found the session of run {run}: {search.session}")
            self.gh.comment(number, self.started_comment(number, search.session, run, b.orchestrator_session_usd, " (found after an unanswered start)"))
            return
        if not search.exhausted:
            # The page cap came first: "not found" proves nothing, so the reserve stays and a person is told, once.
            self.out(f"WATCH       #{number} run {run}: {claude_calls.MAX_LIST_PAGES} pages of sessions read without a match or an end: unresolved")
            if not any(MARK.format(kind="orchestrator-unresolved", pr=number, head=run) in x for x in mine):
                self.gh.comment(number, f"Switchboard, {central(self.now)}: could not tell whether the session of run `{run}` exists: "
                                        f"{claude_calls.MAX_LIST_PAGES} pages of the agent's newer sessions held no match and the listing went on. "
                                        f"The reserve stays on the ledger and no second session is started; look for it in the Console."
                                        f"\n\n{MARK.format(kind='orchestrator-unresolved', pr=number, head=run)}")
            return
        if self.now - attempt < GIVE_UP_AFTER:
            self.out(f"WATCH       #{number} no session yet for run {run}; looking again next sweep")
            return
        if not any(e.run == run and e.phase == "end" for e in self.entries()):
            self.append(money.record(self.now, run, "end", "orchestrator", rules.job_of_title(issue["title"]), Decimal(0),
                                     f"{key} no session found after unanswered start"), f"end {key} (no session found)")
        self.out(f"WATCH       #{number} no session for run {run} after {GIVE_UP_AFTER}: reserve released")
        self.gh.comment(number, f"Switchboard, {central(self.now)}: no session carrying run `{run}` appeared in {int(GIVE_UP_AFTER.total_seconds() // 60)} "
                                f"minutes after the unanswered start, so none was created. The reserve is released at $0. Re-add "
                                f"`needs:orchestrator` to try again.\n\n{MARK.format(kind='orchestrator-cleared', pr=number, head=run)}")

    # ---------- the session watcher (ask) ----------
    def watch_sessions(self) -> int:
        """Each open `job` + `needs:orchestrator` issue with a start comment: answer a paused call with deny
        (nobody is watching), and when the session is over write its `end` and say so once. Returns failures."""
        listed = self.gh.get(f"repos/{self.gh.repo}/issues?labels=job,needs:orchestrator&state=open&per_page={MAX_WATCHED}") or []
        failed = 0
        for it in listed[:MAX_WATCHED]:
            if "pull_request" in it:
                continue
            try:   # one session's trouble never stops the others; every action is safe to repeat next run
                self.watch_session(it)
            except (GitHubError, RuntimeError, KeyError, ValueError) as err:
                failed += 1
                self.out(f"ERROR       #{it['number']}: {type(err).__name__}: {err}")
        return failed + self.watch_closed_reserves({it["number"] for it in listed[:MAX_WATCHED]})

    def watch_closed_reserves(self, seen: set[int]) -> int:
        """A reserve still open on the ledger whose issue has closed is never in the open listing, so it would count
        in full for ever (the $8.00 of #136): its issue is read once and watched like an open one, which ends the run
        at the session's real cost, or writes off a start that made no session. Returns failures."""
        ended = {e.run for e in self.entries() if e.phase == "end"}
        numbers: list[int] = []
        for e in self.entries():
            m = re.fullmatch(r"orchestrator#(\d+)", e.note)
            if e.phase == "reserve" and e.kind == "orchestrator" and e.run not in ended and m:
                n = int(m.group(1))
                if n not in seen and n not in numbers:
                    numbers.append(n)
        failed = 0
        for n in numbers[:MAX_WATCHED]:
            try:
                try:
                    it = self.gh.get(f"repos/{self.gh.repo}/issues/{n}")
                except GitHubError as err:
                    if "HTTP 404" not in err.detail:
                        raise
                    self.out(f"WATCH       #{n} holds a reserve but the issue is gone: left for a person")
                    continue
                if it.get("state") != "closed" or "pull_request" in it:
                    continue
                self.out(f"WATCH       #{n} is closed with a reserve still open: watching it once more")
                self.watch_session(it)
            except (GitHubError, RuntimeError, KeyError, ValueError) as err:
                failed += 1
                self.out(f"ERROR       #{n}: {type(err).__name__}: {err}")
        return failed

    def start_comment(self, comments: list[dict], number: int) -> tuple[str, str] | None:
        """(session id, run id or "") from the switchboard's own start comment; None if there is none."""
        tag = MARK.format(kind="orchestrator", pr=number, head="-")
        for c in comments:
            body = c.get("body") or ""
            if c["user"]["login"] == rules.BOT_LOGIN and tag in body:
                sid = re.search(r"orchestrator session `(sesn_[A-Za-z0-9]+)`", body)
                if sid:
                    run = RUN_ID.search(body)
                    return sid.group(1), run.group(1) if run else ""
        return None

    def run_of(self, number: int, found_run: str) -> str:
        """The ledger run this session pays: carried in the start comment, else (sessions started before the
        comment carried it) the newest orchestrator reserve noted `orchestrator#N`."""
        if found_run:
            return found_run
        notes = [e.run for e in self.entries() if e.phase == "reserve" and e.kind == "orchestrator" and e.note == f"orchestrator#{number}"]
        return notes[-1] if notes else ""

    def watch_session(self, issue: dict) -> None:
        import claude_calls
        number = issue["number"]
        comments = self.gh.paged(f"repos/{self.gh.repo}/issues/{number}/comments")
        found = self.start_comment(comments, number)
        if not found:
            self.reconcile_unknown(issue, comments)
            return
        session, found_run = found
        if self.marked(comments, "session-end", number, session):
            return
        if self.gh.dry_run and not os.environ.get("ANTHROPIC_API_KEY"):
            self.out(f"DRY  WATCH #{number}: would read session {session}")
            return
        st = claude_calls.session_state(self.claude(), session)
        trace = CONSOLE_SESSION.format(session=session)
        if st.status in ("running", "rescheduling"):
            self.out(f"WATCH       #{number} {session} is {st.status}")
        elif st.status == "idle" and st.stop == "requires_action":
            self.out(f"WATCH       #{number} {session} paused on {len(st.pending)} call(s): denying")
            if self.gh.dry_run:
                self.out(f"DRY  deny {len(st.pending)} pending call(s) of {session}")
            else:
                claude_calls.deny_pending(self.claude(), session, st.pending)   # deny first: a comment without it leaves the stall
            if not self.marked(comments, "session-pause", number, st.idle_event):
                self.gh.comment(number, f"Switchboard, {central(self.now)}: session `{session}` paused on {len(st.pending)} "
                                        f"call(s) that needed approval and nobody is watching, so each was denied and the agent "
                                        f"carries on without it. Trace: {trace}\n\n"
                                        f"{MARK.format(kind='session-pause', pr=number, head=st.idle_event)}")
        elif st.status == "terminated" or (st.status == "idle" and st.stop in claude_calls.FINISHED_STOPS):
            self.finish_session(issue, session, found_run, "terminated" if st.status == "terminated" else st.stop,
                                st.list_cost_cents, comments, trace)
        else:
            self.out(f"WATCH       #{number} {session} is {st.status} ({st.stop or 'no idle event'}): nothing to do")

    def finish_session(self, issue: dict, session: str, found_run: str, outcome: str, cents: int | None,
                       comments: list[dict], trace: str) -> None:
        number = issue["number"]
        if cents is None:   # the reserve keeps counting in full until a cost is reported
            self.out(f"WATCH       #{number} {session} is over ({outcome}) but reports no cost yet: the reserve stands")
            return
        cost = Decimal(cents).scaleb(-2)
        run = self.run_of(number, found_run)
        job = rules.job_of_title(issue["title"])
        if not run:
            ledger = "No reserve for it was found on the ledger, so none was closed."
        elif any(e.run == run and e.phase == "end" for e in self.entries()):
            ledger = f"The ledger already holds the end of run `{run}`."
        else:
            self.append(money.record(self.now, run, "end", "orchestrator", job, cost,
                                     f"orchestrator#{number} {session} stop={outcome} list_cost_cents={cents}"), f"end orchestrator#{number}")
            ledger = f"Ledger run `{run}` now counts ${cost:.2f} instead of the ${self.budget.orchestrator_session_usd} reserve."
        self.out(f"WATCH       #{number} {session} {OUTCOMES.get(outcome, outcome)}: ${cost:.2f}")
        self.gh.comment(number, f"Switchboard, {central(self.now)}: orchestrator session `{session}` {OUTCOMES.get(outcome, outcome)}. "
                                f"Cost at list price: ${cost:.2f}. {ledger} Trace: {trace}\n\n"
                                f"{MARK.format(kind='session-end', pr=number, head=session)}")

    # ---------- the digest ----------
    def digest(self, force: bool) -> None:
        local = self.now.astimezone(CENTRAL)
        if local.hour < DIGEST_HOUR and not force:
            self.out(f"DIGEST      not yet: {central(self.now)}")
            return
        issues = self.gh.paged(f"repos/{self.gh.repo}/issues?labels={DIGEST_LABEL}&state=open")
        # Found by its label, not its title, so the owner can rename the issue freely.
        issue = next((i for i in issues if "pull_request" not in i), None)
        today = local.strftime("%A %B %-d")
        if issue:
            recent = self.gh.paged(f"repos/{self.gh.repo}/issues/{issue['number']}/comments?since="
                                   f"{(self.now - dt.timedelta(hours=26)).strftime('%Y-%m-%dT%H:%M:%SZ')}")
            if any(c["user"]["login"] == rules.BOT_LOGIN and f"Digest, {today}" in (c.get("body") or "") for c in recent) and not force:
                self.out(f"DIGEST      already posted for {today}")
                return
        body = self.digest_text(today)
        self.out(body)
        if issue is None:
            got = self.gh.write("POST", f"repos/{self.gh.repo}/issues",
                                {"title": DIGEST_TITLE, "labels": [DIGEST_LABEL],
                                 "body": "The switchboard's 7:00 AM Central digest, one comment a day. Subscribe to this issue."},
                                "create the Digest issue")
            if got is None:
                return
            issue = got
        self.gh.comment(issue["number"], body, f"digest for {today} on #{issue['number']}")

    def digest_text(self, today: str) -> str:
        since = self.now - dt.timedelta(hours=24)
        closed = self.gh.paged(f"repos/{self.gh.repo}/pulls?state=closed&base={rules.TRUNK}&sort=updated&direction=desc")[:100]
        landed = [p for p in closed if p.get("merged_at") and dt.datetime.fromisoformat(p["merged_at"].replace("Z", "+00:00")) >= since]
        red, waiting, asked = [], [], []
        for pr in self.target_prs(0, ""):
            facts, ctx = self.pr_facts(pr)
            author = self.team.vendor_of_branch(facts.head_ref)
            d = rules.merge_decision(facts, author, self.team, bool(self.merge_token), bool(self.review_key))
            if d.action == "FIX-CI":
                red.append(f"#{facts.number} {facts.head_ref}: {facts.proof}")
            if d.action == "HUMAN-MERGE":
                waiting.append(f"#{facts.number} is ready but changes workflow files: the integrator merges it")
            if rules.needs_review(facts, author, self.team) and not self.stale(facts):
                if self.team.pool:
                    who = self.pooled_reviewer(facts, ctx, author)[1]
                else:
                    pick = rules.pick_reviewer(author, facts.needs, self.team)
                    who = pick.agent if pick.route == "ask" else ""
                if who and who != rules.API_REVIEWER:
                    asked.append(f"#{facts.number} waits on a review from {who}")
        for it in self.jobs().values():
            if any(l["name"] == "state:blocked" for l in it["labels"]):
                waiting.append(f"#{it['number']} {it['title'][:70]} is blocked")
        entries = self.entries()
        paused = [e.note for e in entries if e.phase == "refused" and e.ts >= since]
        s = money.spent(entries, self.now, "")
        sessions = []
        for it in self.jobs().values():
            claude = [l["name"][6:] for l in it["labels"] if l["name"].startswith("owner:claude")]
            if claude:
                sessions.append(f"{claude[0]} on #{it['number']} {it['title'][:60]}")
        orch = [e.note for e in entries if e.kind == "orchestrator" and e.phase == "reserve" and e.ts >= self.now - dt.timedelta(days=7)]

        def block(title: str, items: list[str]) -> str:
            return f"**{title}**\n" + ("\n".join(f"- {i}" for i in items) if items else "- none")

        return "\n\n".join([
            f"Digest, {today} ({central(self.now)})",
            block("Landed since yesterday", [f"#{p['number']} {p['title'][:80]}" for p in landed]),
            block("Red", red),
            block("Waits on you or the integrator", waiting + asked),
            block("Paused by the budget", paused),
            f"**API spend:** ${s.day:.2f} today, ${s.month:.2f} this month of ${self.budget.month:.2f}.",
            block("Claude sessions holding jobs", sessions + [f"orchestrator: {o}" for o in orch]),
        ])


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--repo", default=REPO)
    ap.add_argument("--root", default=str(pathlib.Path(__file__).resolve().parents[2]))
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--part", choices=("ask", "steer", "all"), default="all")
    ap.add_argument("--pr", type=int, default=0)
    ap.add_argument("--sha", default="")
    ap.add_argument("--issue", type=int, default=0)
    ap.add_argument("--label", default="")
    ap.add_argument("--digest", action="store_true")
    ap.add_argument("--force-digest", action="store_true")
    args = ap.parse_args()
    if args.sha and not all(c in "0123456789abcdef" for c in args.sha):
        ap.error("--sha must be hex")
    now = dt.datetime.now(dt.timezone.utc)
    sw = Switch(GitHub(args.repo, args.dry_run), pathlib.Path(args.root), now)
    # J-056: out of the environment at once, so git, the rule check and every other gh call never inherit them.
    sw.merge_token = os.environ.pop(MERGE_TOKEN_ENV, "").strip()
    sw.review_key = os.environ.pop(REVIEW_KEY_ENV, "").strip()
    sw.out(f"switchboard {args.part}{' DRY RUN' if args.dry_run else ''} at {central(now)} on {args.repo}")
    steer, ask = args.part in ("steer", "all"), args.part in ("ask", "all")
    if args.issue:
        if steer and args.label.startswith("owner:"):
            sw.owner_labels(args.issue, args.label)
        if ask:
            sw.orchestrate(args.issue)
        return 0
    if args.digest or args.force_digest:
        if steer:
            sw.digest(args.force_digest)
        return 0
    if steer and not (args.pr or args.sha):
        sw.sweep_owner_labels()
    failed = 0
    if ask:
        failed += sw.watch_sessions()
    steered: list[Steered] = []
    for pr in sw.target_prs(args.pr, args.sha):
        try:   # one PR's trouble never stops the others; every action is safe to repeat next run
            facts, ctx = sw.pr_facts(pr)
            if ask:
                sw.ask_pr(facts, ctx)
            if steer:
                sw.close_superseded(facts, ctx)
                sw.steer_pr(facts, ctx, steered)
        except (GitHubError, RuntimeError, KeyError, ValueError, zipfile.BadZipFile) as err:
            failed += 1
            sw.out(f"ERROR       #{pr['number']}: {type(err).__name__}: {err}")
    if steer:
        try:
            sw.auto_open_prs()
        except (GitHubError, RuntimeError, KeyError, ValueError, zipfile.BadZipFile) as err:
            failed += 1
            sw.out(f"ERROR       auto-open: {type(err).__name__}: {err}")
    if steer and not (args.pr or args.sha):   # the inbox reads a whole sweep, so a run for one PR posts none
        try:
            sw.inbox(steered)
        except (GitHubError, RuntimeError, KeyError, ValueError) as err:
            failed += 1
            sw.out(f"ERROR       inbox: {type(err).__name__}: {err}")
        try:   # J-052: Muse's items go to Muse by comment + wake:muse, not only to the integrator's inbox
            sw.muse(steered)
        except (GitHubError, RuntimeError, KeyError, ValueError) as err:
            failed += 1
            sw.out(f"ERROR       muse: {type(err).__name__}: {err}")
        try:   # bridge-health: a stuck Muse mail pipe is loud, once per incident on the board, every run here
            alarms = sw.pipe_health()
        except (GitHubError, RuntimeError, KeyError, ValueError) as err:
            failed += 1
            sw.out(f"ERROR       pipe health: {type(err).__name__}: {err}")
        else:
            summary = os.environ.get("GITHUB_STEP_SUMMARY")
            if alarms and summary:
                with open(summary, "a", encoding="utf-8") as f:
                    f.write("### PIPE ALARM\n\n" + "".join(f"- {plain(a)}\n" for a in alarms))
    return 2 if failed else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (GitHubError, money.BudgetConfigError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as err:
        print(f"switchboard: {type(err).__name__}: {err}", file=sys.stderr)
        sys.exit(2)
