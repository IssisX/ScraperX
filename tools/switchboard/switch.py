#!/usr/bin/env python3
"""The switchboard (design v3 step B, job J-031): GitHub's wake switch and merger. No model decides here.

Grown from tools/board/sweep.py. Each run reads the state from GitHub and acts on it, so a run that
is dropped or repeated changes nothing: every write is either guarded by a marker comment for the
head it acts on, or idempotent at GitHub (a merge names the head sha it merges).

Parts (a workflow job runs one; `all` runs both, for a dry run):
  ask    the paid calls, the only part that holds ANTHROPIC_API_KEY: the automatic Claude review of
         a non-Claude PR, the eye on a `needs:eyes` PR's screenshots, the watcher of orchestrator sessions
  steer  everything free: the merge rule, review asks (never to a `wake: manual` agent), one owner:* label per
         issue, opening PRs for finished fb-bot/ and claude/ integrate-* branches, the 7:00 AM Central digest, and
         after a full sweep one comment on the integrator's inbox PR (#282) for what needs it (J-051; QUEUE-LOW when
         work/tasks/muse.md holds under 8 really-unstarted jobs, J-097; a Muse "J-NNN started" or bare ack, and the
         one re-run of a proof red only in setup, ride as a digest line on the next comment that wakes, J-103),
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
import urllib.error
import urllib.parse
import urllib.request
import uuid
import zipfile
from decimal import Decimal
from zoneinfo import ZoneInfo

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import budget as money  # noqa: E402
import rules  # noqa: E402
import ghapi  # noqa: E402
import muse_queue  # noqa: E402
import plan as collab_plan  # noqa: E402
import machine  # noqa: E402
import mail_reader  # noqa: E402
import work_reader  # noqa: E402
from ghapi import GitHub, GitHubError, RateLimited  # noqa: E402

REPO = "IssisX/ScraperX"
PROOF_WORKFLOW = "wo000-delivery-spine.yml"
NOT_COUNTED = {"publish-checkpoint", "playtest-apk", "wake-switchboard"}   # as sweep.py: skipped or signing-only off trunk
WAKE_JOB = "wake-switchboard"   # the proof's last job, which dispatches this sweep (wo000-delivery-spine.yml)


def only_wake_left(jobs: list[dict]) -> bool:
    """True when the proof's one unfinished job is the one waking this sweep: every proof job is done."""
    return any(j["name"] == WAKE_JOB for j in jobs) and all(
        j["status"] == "completed" for j in jobs if j["name"] != WAKE_JOB)


BUILD_JOBS = ("host-build", "native-tests", "godot-touch", "android-build")   # the game's build and tests


def built_run(jobs: list[dict]) -> bool:
    """Did this proof run build or test the game: a host-build, native-tests, godot-touch or android-build job that
    was not skipped (a failed host-build counts: its tests are skipped because it failed)? A push that changes only
    notes, rules or the switchboard skips them all (ci/proof_scope.py) and reads green, so such a run says nothing
    about trunk's code (RED-TRUNK)."""
    return any((j.get("name") or "").startswith(BUILD_JOBS) and j.get("conclusion") != "skipped" for j in jobs)


LEDGER_BRANCH = "switchboard/ledger"
LEDGER_PATH = "work/archive/runs.log"
DIGEST_TITLE = "Digest"
DIGEST_LABEL = "digest"
MUSE_LABEL = "wake:muse"
# J-072: the live board is Switch.board_issue(), the one open issue labeled BOARD_LABEL, read from GitHub each run,
# so a board that rolls over at 100 comments needs no code change. BOARD_ISSUE is only the fallback when GitHub
# names none and no sweep has found one (#340 opened 11-09, when #107 closed).
BOARD_LABEL = "board"
BOARD_ISSUE = 340
# J-072 (#350): the board rolls itself (Switch.board_roll). The board went past its 100-comment cap three times (the
# last at 235) waiting for a human to roll it. At BOARD_ROLL_AT comments a sweep opens the next board (labeled, the old
# body, a BOARD_FROM mark naming the old one) and takes the label off the old one; a later sweep closes the old one
# (work/CONTEXT.md: closing #340 mid-sweep let that sweep post on a closed board).
BOARD_ROLL_AT = 90
BOARD_FROM_MARK = "<!-- switchboard:board-from #{old} -->"
BOARD_FROM = re.compile(r"^<!-- switchboard:board-from #(\d+) -->$", re.MULTILINE)
BOARD_CONTINUES = re.compile(r"^_Continues #\d+[^\n]*_\n?", re.MULTILINE)
CENTRAL = ZoneInfo("America/Chicago")
DIGEST_HOUR = 7
MAX_DIFF_CHARS = 400_000
MAX_ARTIFACT_BYTES = 60_000_000
MAX_IMAGE_BYTES = 3_750_000            # base64 of this stays under 5 MB
MAX_PRS = 60
MAX_AUTO_PRS = 10                  # PRs the auto-open opens in one sweep (J-047): Muse's lane limit (SYSTEM_PLAN 9)
# The branches the switch opens a PR for by itself (J-047), integrate-protected-* too. claude/ since 11-13: a PR a
# helper opened by hand subscribed the integrator session to it, so every event on it woke the integrator (#497, #498).
AUTO_PR_PREFIX = ("fb-bot/integrate-", "claude/integrate-")
AUTO_PR_OLD = dt.timedelta(days=7)   # #350: a head this old, its AUTO-PR-FAILED listed once, is never listed again
MAX_AUTO_PR_OLD = 500                # such heads kept in the sweep state ("auto_pr_old")
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
INBOX_FIX_STUCK = dt.timedelta(hours=6)        # FIX with no new head this long, past the reconciler's send and re-send
                                               # (11-12: #389 and #396 each sat a day on a Codex FIX)
MERGEABLE_READS = 3                            # reads of a PR whose mergeability GitHub is still computing, after the first
MERGEABLE_WAIT_S = 3.0                         # seconds before each
INBOX_BRANCH_RED = dt.timedelta(hours=1)       # a red integrate branch with no PR this long
INBOX_RUNS_WINDOW = dt.timedelta(hours=24)     # proof runs read per sweep, as tools/board/sweep.py's branch window
RED_RUN_KEYS = ("id", "head_branch", "head_sha", "status", "conclusion", "created_at", "updated_at", "html_url",
                "run_attempt")
QUIET_HOURS = 3.0                              # tools/board/sweep.py --quiet-hours
MAX_INBOX_LINES = 30                           # items in one inbox comment; the rest come next sweep
MAX_INBOX_KEYS = 600                           # keys kept in the sweep state's inbox ring (gh.state["inbox"])
# 11-13 (Cory's "A", #282 measure): every inbox comment is a full wake of the integrator, priced by its context, not
# by the message. So asks wake at once, the integrator's own work lines post at most once per INBOX_HOLD, and
# reports ride the next post that wakes, never a post alone (6854673147). The keys left the comment
# for the sweep state: the relay to the session cuts a comment near 4,000 characters, and the keys were most of it.
INBOX_HOLD = dt.timedelta(minutes=30)
INBOX_MAX_CHARS = 3500                         # a comment's whole text, under the relay's cut with room to spare
# a late job's next step owed by these reaches the integrator. #350: "switchboard" too: a step the switch owes itself
# and has not taken (J-049's proof read, J-072's PR open) is the integrator's to fix, and else reached nobody
PLAN_WAKES = frozenset({"claude", "integrator", "switchboard"})
MAX_INBOX_BRANCHES = 40                        # integrate branches whose proof jobs are read per sweep
MAX_PLAN_LINES = 20                            # late jobs, label changes printed (and, live, listed) per pass
MAX_PLAN_LABELS = 20                           # issues whose labels a live pass writes
# Shadow only prints, and one pass is about 245 reads on a cold cache (11-12, measured: 90 job threads, 60
# compares, 60 proof reads, the listings), a third of a full pass's own. So in shadow it runs in the full pass of
# every sixth hour, four times a day, and only with twice the rate floor left. Live, it runs every full pass.
PLAN_SHADOW_EVERY_H = 6
# RED-TRUNK (11-13: trunk's full proof was red about a day, J-129's doubled order key 1100, and nobody owned it: the
# notes-only pushes after it skip the build, so trunk's newest run read green). Trunk's proof is its newest run that
# RAN the build (built_run), kept in the sweep state (gh.state["trunk_proof"]) until a newer such run replaces it, so
# a red stays red until a green full proof clears it. Its jobs are read newest first, at most this many a sweep.
TRUNK_RUN_READS = 6
# J-072 (11-14): finished trunk runs judged skip-the-build, kept in the sweep state (gh.state["trunk_skipped"]),
# newest last: passed over unread and uncounted, so more notes-only runs than one sweep reads never hide a red run.
TRUNK_SKIPPED_KEPT = 50
# The switch merges as "Merge pull request #483 from fb-bot/integrate-j129 (switchboard)"; GitHub's own merge names
# the repo's owner first ("from IssisX/claude/integrate-wake-cut"), which trunk_proof strips.
MERGE_FROM = re.compile(r"^Merge pull request #(\d+) from (\S+)")
INBOX_ORDER = ("RED-TRUNK", "MERGE-BY-HAND", "MERGED", "TO-CLAUDE", "TAKE", "PLAN", "QUEUE-LOW", "CODEX-ASK", "NOT-MERGED", "AUTO-PR-FAILED", "NOTE-REVIEW", "CONFLICT", "FIX", "FIX-STUCK", "RED", "RED-TWICE", "RED-STUCK", "RED-BRANCH", "QUIET", "FYI", "STARTED", "ACK", "RERUN")
# J-103 (Cory 11-11, "Teach"): kinds that never wake the integrator alone. They ride as one digest line on the next inbox
# comment that does wake, never alone (11-13: every #282 comment wakes the subscribed integrator, so the digest-only
# post 6854673147 woke it for nothing); held between sweeps in the state, so nothing is lost. FYI is a Muse mail that
# only reports (muse_report).
# Beside these, inbox_class rides the late-job (PLAN) lines whose next step another owner holds.
DIGEST_KINDS = frozenset({"STARTED", "ACK", "RERUN", "FYI"})
MAX_DIGEST_ITEMS = 20                          # digest entries on one inbox comment; the rest ride on the next
MAX_DIGEST_HELD = 200                          # digest entries kept between sweeps (timers["digest"]); past it the oldest
                                               # are dropped and named in the run's log (keep_held)
# J-072 (11-14: #555 and #396 merged and the integrator found each only by hand): a merge the switch makes is one
# MERGED item, kept in the sweep state (gh.state["merged"]) until an inbox post lists it, so a sweep that posts no
# inbox loses none. The integrator's own (claude/) merges post at once; Muse's ride the next post.
MAX_MERGED_HELD = 50
MAX_INBOX_HELD = 200                           # wake items held between sweeps (gh.state["inbox"]["held"]), likewise
MAX_RERUN_HEADS = 200                          # heads re-run, kept between sweeps (timers["rerun"]): one re-run per head
# A Muse bridge title that is only "J-NNN started" (J-040 take_blow started: one name may sit between), or a bare ack.
MUSE_STARTED = re.compile(r"^(?:MUSE_TO_CLAUDE:\s*)?J-\d{3,}[a-z]?(?:\s+(?!not\b|never\b)[A-Za-z0-9_][\w.-]*)?\s+started[.!]?$",
                          re.IGNORECASE)
# Muse's acks of a notice read "J-078 #396 re-ack" (11-11): a job, up to three words, then the ack.
MUSE_ACK = re.compile(r"^(?:MUSE_TO_CLAUDE:\s*)?(?:J-\d{3,}[a-z]?(?:\s+(?!not\b)[#\w.-]+){0,3}\s*[:,-]?\s*)?(?:re-?)?"
                      r"(?:ack(?:ed|nowledged)?|got it|received|noted|ok(?:ay)?|roger|will do|thanks?|thank you)[.!]?$",
                      re.IGNORECASE)
ACK_ID = re.compile(r"/ack\s+O-[0-9a-f]+", re.IGNORECASE)   # the reconciler's obligation id in an ack: no sha
# What still wakes at once in a started or ack post (#422): a question, a sha, a red or failure, a finding, "taken".
# 11-13 (#282 measure): "to claude" left it: Muse opens every mail body "To Claude:", so it woke every started post.
MUSE_WAKES = re.compile(r"\?|\b(?=[0-9a-f]*[0-9])(?=[0-9a-f]*[a-f])[0-9a-f]{7,40}\b|\btaken\b|\bred\b|\bfail\w*|"
                        r"\bblock\w*|\bstuck\b|\bfinding\w*|\bfound\b|\bbug\w*|\berror\w*|\bbroke\w*|\bcannot\b|"
                        r"\bcan't\b|\bproposal\b|\bneeds? (?:your|a)\b", re.IGNORECASE)
# 11-13: a Muse mail is a report (it rides the digest) only when its title says so and nothing in it asks; anything
# else wakes at once. The title: tagged FYI, or a routine word (pushed, in work, done, status, parked, ...), or a LAND
# review verdict. Tagged ASK, or a `To <Name>:` in the title (work/tasks/muse.md's ask subject), always wakes.
MUSE_TAG = re.compile(r"(?:^|[\s:(\[])(ASK|FYI)(?=$|[\s:,.)\]-])")
MUSE_ASK_SUBJECT = re.compile(r"\bTo [A-Z][\w-]*:")
# The routine word must be the title's headline, in its first clause or opening its last ("J-009 #377 FIX — ack"):
# "J-082 sha — lane touched it, pushed now" answers an ask (it wakes); "J-078 pushed @ ddceb919 — ..." reports a push.
MUSE_REPORT_TITLE = re.compile(r"\b(?:started|re-?ack(?:ed)?|ack(?:ed|nowledged)?|in work|in progress|pushed|"
                               r"dispatched|done|status|parked|passed|green|landed|merged|running|resumed)\b",
                               re.IGNORECASE)
TITLE_CLAUSE = re.compile(r"\s+[—–-]+\s+|:\s")
MUSE_LAND_TITLE = re.compile(r"\breview of #\d+ @ [0-9a-f]{7,40}:?\s*LAND\**$")
# What turns a report into an ask: a question mark (outside quoted lines and links), a relay to another agent, or a
# word that asks for a ruling, a call, an answer, help, a stop, a resend or Cory.
MUSE_ASKS = re.compile(
    r"\?|\bTo (?!Claude\b)[A-Z][\w-]*:|"
    r"\b(?:not|never)\s+(?:yet\s+)?(?:started|pushed|done|landed|merged|passed|green|running)\b|"
    r"\b(?:need|needs|needed|want|wants|waiting on|wait for|waits for|awaiting|asking for|ask for|asks for)\s+"
    r"(?:your|you|a|an|claude|cory|the integrator)\b|"
    r"(?<!per )(?<!on )(?<!after )(?<!under )\byour (?:call|ruling|answer|decision|ok|okay|go|pick|view)\b"
    r"(?!\s*(?:\)|,|\.|;|$|-|—))|"
    r"\b(?:ruling needed|rule on|decide|decision needed|which (?:way|one|option)|or split|waiver|question\w*|please|"
    r"approve|approval|propos\w*|blocked|blocker|stop|stopped|halt\w*|resend|re-send|taken|handover|yours|deviat\w*|"
    r"cory|stale|wrong|unclear|unsure|not sure)\b", re.IGNORECASE)
# A routine start or ack that names its own test plan ("failing test first") reports no failure.
FAILING_FIRST = re.compile(r"\bfailing[- ](?:first|tests?)\b", re.IGNORECASE)
MAILED_FOOTER = re.compile(r"^_Mailed .*_$", re.MULTILINE)
LINK = re.compile(r"https?://\S+")
# A proof job's setup steps (wo000-delivery-spine.yml): the runner's own, the checkout, the cache restores, the package,
# NDK, SDK, Godot and render-prerequisite installs, Java. Every build, test and verify step is named otherwise.
SETUP_STEP = re.compile(r"^(?:Set up job|Check ?out|Restore|Install|Configure)\b")
MUSE_MARK = "<!-- switchboard:muse keys={keys} -->"
MUSE_KEYS = re.compile(r"^<!-- switchboard:muse keys=([0-9a-f,]*) -->$", re.MULTILINE)
MAX_MUSE_COMMENTS = 10          # Muse comments posted in one sweep; the rest come next sweep
MAX_MUSE_FINDING = 600          # chars of a FIX finding quoted in a Muse comment
OLD_BOARDS = (107, 87, 3)       # the boards closed before the label named them: Switch.old_boards() adds the labeled ones
MAX_OLD_BOARDS = 4              # closed boards whose muse markers still count, the newest first (a move never re-sends)
MAX_OLD_BOARD_PAGES = 3         # comment pages read from each closed board (a board closes at 100 comments)
# The Muse mail pipe's health (bridge-health): the bridge (muse_bridge.gs) is an Apps Script whose errors only Cory
# can see, so the switch watches what it leaves on GitHub and posts one alarm per incident on the board.
PIPE_HEADER = "Claude (integrator): PIPE ALARM:"
PIPE_MARK = "<!-- switchboard:pipe-alarm kinds={kinds} -->"
PIPE_KINDS = re.compile(r"^<!-- switchboard:pipe-alarm kinds=([a-z,]*) -->$", re.MULTILINE)
# #350: the all-clear when a tripped pipe is healthy again. It starts CLAUDE_TO_MUSE: (the bridge mails only that); its
# mark keeps it from counting as an ask to Muse (inbound_quiet, the fast sweep's pipe trigger)
PIPE_OK_HEAD = "CLAUDE_TO_MUSE: PIPE OK on #{board}:"
PIPE_OK_MARK = "<!-- switchboard:pipe-ok -->"
PIPE_WAKE_STUCK = dt.timedelta(minutes=20)    # a wake:muse label this old: the bridge's outbound is stuck (it runs every 5 min)
PIPE_INBOUND_QUIET = dt.timedelta(hours=24)   # no bridge post this long while asks went to Muse: its inbound is stuck
PIPE_LOOKBACK = dt.timedelta(days=7)          # how far back an open inbound incident is read; older, it alarms again
# #350 (run 43587341746: 12 h with no Muse push and no mail either way, 13 Muse lanes late, and nothing woke anyone;
# inbound_quiet needs a CLAUDE_TO_MUSE ask in its day). While a Muse lane is in flight, this long with no fb-bot push
# and no bridge post is one QUIET item to the integrator, at most once per MUSE_QUIET_AGAIN while it lasts.
MUSE_QUIET = dt.timedelta(hours=1)        # Cory 11-14: Muse posts about every half hour; 6 h was far too long
MUSE_QUIET_AGAIN = dt.timedelta(hours=2)
# #350 (J-124, J-078): a TO-CLAUDE ask that names a job stays open in the sweep state until a later Claude comment or
# mail names the job, and is listed again at most once per ASK_AGAIN while it is open. At most MAX_OPEN_ASKS kept.
ASK_AGAIN = dt.timedelta(hours=1)          # Cory 11-14: an ask to Claude is answered within the hour
# J-072 (Cory 11-14: J-100, J-014 and J-053 failed twice in Muse's hands and the integrator missed all three): a Muse
# job whose PR went red on two heads since its last green, on a same failure (same_failure) trunk's red proof does
# not share, is one TAKE: Muse is mailed once to push what it has and stop (work/tasks/muse.md), and the integrator
# is told at once, again at most once per ASK_AGAIN, until work/plan.md gives the job to claude. The sweep state
# keeps each such branch's red heads since its last green ("red_heads") and the open takes ("takes").
TAKE_HEADS_KEPT = 5
MAX_TAKE_BRANCHES = 50
MAX_TAKES = 50
NUMBERS = re.compile(r"\d+(?:\.\d+)?")
MAX_OPEN_ASKS = 50
ASK_FIELDS = ("job", "target", "at", "listed", "said", "url")   # one open ask in the sweep state, every field a string
JOB_NAMED = re.compile(r"\bJ-\d{3,}[a-z]?(?![\w])")   # machine.JOB_ID
BRIDGE_POST = "**Muse (via email bridge)"     # the first line of every inbound bridge post
LEDGER_NOTICE = "<!-- switchboard:ledger keys="   # the hidden marker of a ledger notice (ledger.py NOTICE_MARK)
# J-072: the live bridge's version. muse_bridge.gs's BRIDGE_VERSION is bridge_version() of its own text; its clock
# sends it, and switchboard.yml names the run with it. A live bridge that is not trunk's, or names none, alarms once.
BRIDGE_FILE = "tools/switchboard/muse_bridge.gs"
SWITCHBOARD_WORKFLOW = "switchboard.yml"
REVIEWS_RUN = "Switchboard reviews"   # switchboard.yml's run name for a part=ask run (SYSTEM_PLAN 6, wake_reviews)
BRIDGE_VERSION_LINE = re.compile(r"^var BRIDGE_VERSION = '([0-9a-f]*)';", re.MULTILINE)
BRIDGE_RUN = re.compile(r"^Switchboard sweep \(bridge ([0-9a-f]{12})\)$")
BRIDGE_SEEN = dt.timedelta(minutes=25)    # the clock dispatches every 10 min: two missed in a row is no clock
BRIDGE_TOLD_MARK = "<!-- switchboard:bridge-version live={live} trunk={trunk} -->"
BRIDGE_TOLD = re.compile(r"<!-- switchboard:bridge-version live=([0-9a-f]{12}|none) trunk=([0-9a-f]{12}) -->")
MAX_WAKE_ISSUES = 30                          # wake:muse issues read per sweep (one page)
# The switchboard workflow wakes on `: LAND`, `: FIX`, `: NOTE` or `: BLOCK` anywhere in a comment, in any case.
VERDICT_TRIGGER = re.compile(r":(?= ?(?:land|fix|note|block))", re.IGNORECASE)
# J-056: the merge of a PR that changes .github/workflows uses this secret (a fine-grained PAT), held only by the
# steer job's one step; main() takes it out of the environment so no child process inherits it.
MERGE_TOKEN_ENV = "SWITCHBOARD_MERGE_TOKEN"
MERGE_TOKEN_REFUSED = {401, 403, 404, 405}   # GitHub's answers to a token that cannot merge here
MERGE_TOKEN_NEEDS = "Contents, Pull requests and Workflows: read/write"
CODEX_MENTION = "@codex review"   # what wakes the Codex app, from a person's login only (codex_mention)
# J-056: the automatic review signs its verdict with this secret (HMAC-SHA256). Only the ask and steer jobs' switch
# steps hold it, through environment switchboard-merge; main() takes it out of the environment like the merge token.
# Any branch's push workflow posts as the workflow bot, so on a workflow-file PR only a signed review is the pass.
REVIEW_KEY_ENV = "SWITCHBOARD_REVIEW_KEY"
# J-072: an optional Healthchecks.io ping URL (repo secret), held only by the steer job's switch step and taken out
# of the environment at start like the tokens. A full non-dry sweep pings it once at its end: the URL on success,
# URL/fail when any part errored. Healthchecks alerts on a missed ping, so a sweep that stops running is loud too.
HC_SWEEP_ENV = "HC_SWEEP_URL"
# J-072: the ETag cache file (ghapi.GitHub.load_etags), carried between runs by the Actions cache in switchboard.yml.
ETAG_CACHE_ENV = "SWITCHBOARD_ETAG_CACHE"
BRIDGE_VERSION_ENV = "SWITCHBOARD_BRIDGE_VERSION"   # the dispatch's bridge_version input (switchboard.yml): free each sweep
# J-072: under this share of the hour's limit (x-ratelimit-limit, read at run time) only the merge path runs
# (run_sweep); the rest waits. 16% is 160 of GITHUB_TOKEN's 1,000 (the merge path reads about 6 calls a PR) and 800
# of an App's 5,000.
RATE_FLOOR_SHARE = 0.16
RATE_FLOOR_MIN = 150
AGENT_TOKEN_ENV = "AGENT_GITHUB_TOKEN"   # the hosted agent's token: its rate is logged once, never used by the switch
HC_TIMEOUT_S = 10
DRY_SUMMARY = ("### DRY RUN: no action taken\n\nEvery action this run printed was printed only, none was taken "
               "(`dry_run=true` or `--dry-run`). This run does not count as a sweep.\n")
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


def sweep_heartbeat(url: str, failed: int, out=print, opener=urllib.request.urlopen) -> bool:
    """J-072: ping Healthchecks.io at the end of a full non-dry sweep: `url` when nothing failed, `url/fail` when
    something did. Never raises, never prints the URL (anyone holding it can ping the check). True if sent."""
    if not url:
        return False
    if not url.startswith("https://"):
        out(f"NOTE        {HC_SWEEP_ENV} is not an https URL; no heartbeat sent")
        return False
    target = url.rstrip("/") + ("/fail" if failed else "")
    req = urllib.request.Request(target, data=f"switchboard sweep: {failed} part(s) failed\n".encode(), method="POST")
    try:
        opener(req, timeout=HC_TIMEOUT_S).close()
    except (urllib.error.URLError, OSError, ValueError) as err:
        out(f"NOTE        heartbeat not sent ({type(err).__name__})")
        return False
    out(f"HEARTBEAT   sent ({'fail' if failed else 'ok'})")
    return True


def bridge_version(text: str) -> str:
    """The version of a muse_bridge.gs text: sha256 of it with its BRIDGE_VERSION value blanked, 12 hex. So the
    constant changes with every edit of the script (test_bridge checks the file's constant is its own hash)."""
    blank = BRIDGE_VERSION_LINE.sub("var BRIDGE_VERSION = '';", text.replace("\r\n", "\n"), count=1)
    return hashlib.sha256(blank.encode("utf-8")).hexdigest()[:12]


def note_dry_run(summary_path: str | None) -> None:
    """J-072: a dry run says so at the top of its job summary, so a green dry run never reads as a sweep."""
    if summary_path:
        with open(summary_path, "a", encoding="utf-8") as f:
            f.write(DRY_SUMMARY)


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


def board_next(old: dict) -> tuple[str, str]:
    """The next board's (title, body) from the old one's listing item (board_roll): "Board: live jobs (5)" becomes
    "(6)" (a title with no number gets " (2)"); the body is the old one's as it stands, its "# " heading (if it starts
    with one) the new title, an earlier roll's mark and line taken out, and a line and BOARD_FROM mark naming the
    board it continues put under the heading."""
    number, title = old["number"], str(old.get("title") or "Board: live jobs").rstrip()
    m = re.search(r"\((\d+)\)$", title)
    new_title = f"{title[:m.start()].rstrip()} ({int(m.group(1)) + 1})" if m else f"{title} (2)"
    body = BOARD_FROM.sub("", BOARD_CONTINUES.sub("", str(old.get("body") or ""))).lstrip("\n")
    head = ""
    if body.startswith("# "):   # the old heading names the old board: the new one's own name
        head, body = f"# {new_title}\n\n", body.partition("\n")[2].lstrip("\n")
    note = (f"_Continues #{number}, rolled by the switchboard at {old.get('comments')} comments; its comments stay "
            f"there._\n{BOARD_FROM_MARK.format(old=number)}\n\n")
    return new_title, head + note + body


def hours(span: dt.timedelta) -> str:
    return f"{span.total_seconds() / 3600:.1f} h"


def red_jobs(proof: str) -> list[str]:
    return proof[len("red: "):].split(", ") if proof.startswith("red: ") else []


def same_failure(test: str) -> str:
    """J-072: a failing test as a TAKE compares it, "<job>: <FAIL line>" (red_tests) with its numbers cut, so the
    same check failing by another measured amount on the next head is the same failure."""
    job, _, line = test.partition(": ")
    return f"{job}: {NUMBERS.sub('#', line)}"


def take_mail(take: dict) -> str:
    """J-072: Muse's TAKE line, plain words (work/tasks/mail-to-muse.md: no order, no override, no urgency)."""
    return (f"the proof went red on two heads since its last green, on the same failing tests, listed below. By the rule in "
            f"work/tasks/muse.md, a fix that fails twice on the same failure goes to Claude's helpers: please push what "
            f"you have as it is, mail MUSE_TO_CLAUDE that you stopped {take['job']}, and take your next lane. The "
            f"plan's next version names the job's new owner.")


# J-104: a red notice names the failing tests, read from the failed jobs' logs (Muse's #108 answer: the check
# annotations name no test inside a red group, so every lane re-ran locally just to learn which one failed).
FAIL_LOG_JOBS = ("native-tests", "godot-touch", "host-build")   # the jobs whose logs name what failed
MAX_FAIL_LINES = 6       # unique FAIL lines per job in a notice
MAX_FAIL_JOBS = 8        # job logs read per proof run
MAX_FAIL_CHARS = 200     # of one FAIL line
# A log read that fails never stops reads of other runs' logs. Only a timeout (ghapi.TimedOut: a GitHub that hangs,
# up to GET_ATTEMPTS x TIMEOUT_S each) counts against the sweep; any other failure (an answer: a 4xx at once, else
# three quick tries) counts against its own run only. Before (11-11): any two failures in a sweep, on one old run,
# left every later notice "log unread" with no read tried.
MAX_LOG_FAILURES = 2      # log reads that time out in one sweep before no more are tried
MAX_RUN_LOG_FAILURES = 2  # log reads that fail in one proof run before its other jobs' logs are not tried
MAX_ERR_CHARS = 200       # of gh's error text in a NOTE line
NATIVE_FAIL = re.compile(r"\bFAIL: .*")
TOUCH_FAIL = re.compile(r"\bSCRAPERX_UITEST FAIL\b.*")
# 11-13 (#350): host-build names no test; its failed step's first failing line (a check's FAIL, a compiler or CMake
# error, a Python check's exception) and the lines after it, as `ASSET_CREDITS FAIL files=363 ... unrecorded=9`
HOST_FAIL = re.compile(r"\b[A-Z][A-Z0-9_]* FAIL\b|\bFAIL: |\berror: |\bCMake Error\b|\b\w+(?:Error|Exception): ")
LOG_STAMP = re.compile(r"^\d{4}-\d\d-\d\dT[\d:.]+Z ?")   # the runner's timestamp on each log line
ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
LOG_UNREAD = "log unread"
# A red notice that went out with a log unread is keyed apart (InboxItem.key): a later sweep that reads the lines posts
# the notice once more, with them, under the plain key. So once per head and per reading, never more than twice.
UNREAD_KEY = "+unread"
READ_NOW = "the failing tests, read now (their logs were unread at the first notice)"
URL_QUERY = re.compile(r"(https?://[^\s?#\"']+)[?#][^\s\"']*")   # a signed blob URL's query is a credential
TOKENISH = re.compile(r"\b(?:gh[pousr]_[A-Za-z0-9_]{10,}|github_pat_[A-Za-z0-9_]{10,})")


def fail_lines(log: str, job: str) -> tuple[list[str], int]:
    """A failed job's unique FAIL lines in log order, at most MAX_FAIL_LINES, and how many more there were: `FAIL: `
    for native-tests, `SCRAPERX_UITEST FAIL` for godot-touch, the failed step's lines for host-build (host_fail_lines).
    The runner's timestamp and colour codes are dropped."""
    if job.startswith("host-build"):
        return host_fail_lines(log)
    pattern = TOUCH_FAIL if job.startswith("godot-touch") else NATIVE_FAIL
    found: list[str] = []
    for line in ANSI.sub("", log).splitlines():
        m = pattern.search(line)
        if m and m.group(0).strip() not in found:
            found.append(m.group(0).strip())
    return found[:MAX_FAIL_LINES], max(0, len(found) - MAX_FAIL_LINES)


def host_fail_lines(log: str) -> tuple[list[str], int]:
    """host-build's failing lines (#350, 11-13): the output of the step that failed (after its header, before the
    runner's first ##[error] line), unique, from its first HOST_FAIL line on; with none, the step's last line. At most
    MAX_FAIL_LINES, and how many more there were, as fail_lines."""
    lines = [LOG_STAMP.sub("", line).strip() for line in ANSI.sub("", log).splitlines()]
    end = next((i for i, line in enumerate(lines) if line.startswith("##[error]")), len(lines))
    start = max((i + 1 for i, line in enumerate(lines[:end]) if line.startswith("##[endgroup]")), default=0)
    step = [line for line in lines[start:end] if line]
    first = next((i for i, line in enumerate(step) if HOST_FAIL.search(line)), None)
    found = list(dict.fromkeys(step[first:] if first is not None else step[-1:]))
    return found[:MAX_FAIL_LINES], max(0, len(found) - MAX_FAIL_LINES)


def log_text(text: str, limit: int = MAX_FAIL_CHARS) -> str:
    """A log line in a notice: the branch's own code wrote it, so no backtick, @, angle bracket (no marker this
    switchboard reads), control character or verdict trigger, bounded."""
    clean = "".join(ch for ch in text.replace("`", "'") if ch.isprintable() and ch not in "@<>")
    clean = no_verdict(clean)
    return clean if len(clean) <= limit else clean[:limit - 3] + "..."


def err_text(err: Exception) -> str:
    """Why a read failed, for a NOTE line: the exception's kind, its HTTP status when known, and gh's error text on
    one line, with no URL query or token-shaped text, at most MAX_ERR_CHARS."""
    detail = " ".join(str(getattr(err, "detail", "") or err).split())
    detail = TOKENISH.sub("[token]", URL_QUERY.sub(r"\1?[cut]", detail))
    status = getattr(err, "status", 0)
    return f"{type(err).__name__}{f' HTTP {status}' if status else ''}: {log_text(detail, MAX_ERR_CHARS) or '-'}"


def setup_only(jobs: list[dict]) -> str:
    """J-103: where a red proof failed, when every counted job that failed did so in a setup step (SETUP_STEP: the
    package, NDK or render-prerequisite install, a cache restore, the checkout), so an outage of the runners' mirrors,
    not the change; "" when any failed in a build, test or verify step, or failed with no failed step named. A job
    cancelled beside it (a matrix sibling) neither counts nor stops it."""
    where: list[str] = []
    for j in jobs:
        if j.get("name") in NOT_COUNTED or j.get("conclusion") in ("success", "skipped", "cancelled"):
            continue
        bad = [st.get("name") or "" for st in j.get("steps") or [] if st.get("conclusion") == "failure"]
        if j.get("conclusion") != "failure" or not bad or not all(SETUP_STEP.match(n) for n in bad):
            return ""
        where.append(f"{j.get('name')}: {bad[0]}")
    return "; ".join(where)


def muse_routine(first: str, body: str) -> str:
    """J-103: "STARTED" for a Muse bridge post whose title is only "J-NNN started", "ACK" for a bare ack, "" for
    anything else. A post holding a question, a sha, a red, a finding or "taken" is never routine: it wakes at once."""
    if not first.startswith(BRIDGE_POST):
        return ""
    title = first[len(BRIDGE_POST):].lstrip(":").strip()
    title = title[:-2].strip() if title.endswith("**") else title
    kind = "STARTED" if MUSE_STARTED.match(title) else "ACK" if MUSE_ACK.match(title) else ""
    if kind:
        return "" if MUSE_WAKES.search(FAILING_FIRST.sub(" ", ACK_ID.sub("", body))) else kind
    return "FYI" if muse_report(title, body) else ""


def ask_said(kind: str, first: str) -> str:
    """An ask's first line as an inbox line shows it: bounded, no mention, nothing that wakes the switchboard."""
    said = first if kind == "TO-CLAUDE" else "Muse: " + first[len(BRIDGE_POST):].strip(": *")
    return no_verdict(plain(said[:100]))


def thread_job(thread: dict | None) -> str:
    """The job an open issue or PR is (#350): a PR's `Job: J-NNN` line, an issue's title; "" for neither."""
    if not isinstance(thread, dict):
        return ""
    if thread.get("pull_request") is not None:
        return rules.job_of_pr(thread.get("body") or "")
    return rules.job_of_title(thread.get("title") or "")


def same_job(job: str, named) -> bool:
    """Does an id in `named` name `job` (#350)? The same id, or one is the other's base (J-044c and J-044): a
    split's answer answers its base's ask, and the base's answers the split's."""
    def base(j: str) -> str:
        return j.rstrip("abcdefghijklmnopqrstuvwxyz")
    return any(n and (n == job or n == base(job) or base(n) == job) for n in named)


def unquoted(body: str) -> str:
    """A mail's own words: no quoted (`>`) line, no link (a query's `?` is no question), no ack id, no bridge footer."""
    own = "\n".join(line for line in body.split("\n") if not line.lstrip().startswith(">"))
    return LINK.sub(" ", ACK_ID.sub(" ", MAILED_FOOTER.sub("", own)))


def muse_report(title: str, body: str) -> bool:
    """11-13: a Muse mail that only reports (FYI): its title is tagged FYI or reads as a routine report, the title
    holds no ASK tag and no `To <Name>:` ask, and neither the title nor the body asks (MUSE_ASKS). Unsure is an ask:
    an untagged title with no routine word wakes, and so does any question mark."""
    tag = MUSE_TAG.findall(title)
    if "ASK" in tag or MUSE_ASK_SUBJECT.search(title):
        return False
    clauses = TITLE_CLAUSE.split(title)
    if "FYI" not in tag and not MUSE_REPORT_TITLE.search(clauses[0]) \
            and not MUSE_REPORT_TITLE.match(clauses[-1].strip()) and not MUSE_LAND_TITLE.search(title):
        return False
    return not MUSE_ASKS.search(title) and not MUSE_ASKS.search(unquoted(body))


def http_status(detail: str) -> int:
    """The HTTP status in gh's error text ("gh: Not Found (HTTP 404)"), 0 when it names none."""
    m = re.search(r"\bHTTP (\d{3})\b", detail)
    return int(m.group(1)) if m else 0


TOKEN_REFUSED = "GitHub refused the merge token"   # MergeTokenRefused's first words: such a HUMAN-MERGE is never cold


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
    tests: tuple[str, ...] = ()   # for a red: "<job>: <FAIL line>" per failing test (J-104), already log_text
    # for a red whose logs this audience may want: its proof run, so muse() reads them only once it knows the item
    # is not sent yet (its keys are on the board and the job issues, read after the items are built)
    run_id: int | None = dataclasses.field(default=None, compare=False)
    since: str = dataclasses.field(default="", compare=False)   # when it was first held, unposted (11-13)

    @property
    def unread(self) -> str:
        """UNREAD_KEY when a failing test job's log went unread, else "": that notice is keyed apart, so a later sweep
        that reads the lines posts them once under the plain key (J-104)."""
        return UNREAD_KEY if any(t.endswith(f": {LOG_UNREAD}") for t in self.tests) else ""

    @property
    def key(self) -> str:
        return inbox_key(self.kind + self.unread, self.target, self.head)

    def listed(self, seen, prefix: str = "") -> bool:
        """Listed before: under its own key, or under the plain key (the lines went out already, or the notice went out
        before UNREAD_KEY existed), so a log unread now never sends a notice again."""
        return inbox_key(prefix + self.kind + self.unread, self.target, self.head) in seen \
            or inbox_key(prefix + self.kind, self.target, self.head) in seen

    def read_now(self, reported, prefix: str = "") -> "InboxItem":
        """This item, its line saying READ_NOW when its tests read and an earlier notice at this head went out with
        a log unread (that notice's key is in `reported`)."""
        if self.tests and not self.unread and inbox_key(prefix + self.kind + UNREAD_KEY, self.target, self.head) \
                in reported:
            return dataclasses.replace(self, line=f"{self.line}; {READ_NOW}")
        return self

    @property
    def muse_key(self) -> str:
        return inbox_key("muse-" + self.kind + self.unread, self.target, self.head)


def queue_item_record(items: tuple[InboxItem, ...]) -> dict | None:
    """The QUEUE-LOW item as the sweep state keeps it between counts (J-097); None when the queue was not low. Its
    target too (#350, 11-13): the live count's is work/plan.md, and a kept item on another target is another key."""
    return {"head": items[0].head, "line": items[0].line, "target": items[0].target} if items else None


def queue_item_of(record) -> tuple[InboxItem, ...]:
    """The QUEUE-LOW item back from the sweep state; () for none or anything malformed."""
    if not isinstance(record, dict) or not isinstance(record.get("head"), str) or not isinstance(record.get("line"), str):
        return ()
    target = record.get("target") if isinstance(record.get("target"), str) else muse_queue.QUEUE_FILE   # an old record
    return (InboxItem("QUEUE-LOW", target, record["head"], record["line"]),)


def inbox_class(it: InboxItem) -> str:
    """11-13: how an item reaches the integrator. "now": an ask, posted at once (TO-CLAUDE: every non-Muse ask, and
    every Muse mail muse_report cannot call a report). "ride": a report, never a post alone (DIGEST_KINDS, a late
    job whose next step another owner holds). "hold": the integrator's own work,
    posted at most once per INBOX_HOLD, or sooner riding an ask. Any kind not named here holds, so a new kind is
    never only a digest entry."""
    if it.kind in ("TO-CLAUDE", "RED-TRUNK"):   # RED-TRUNK: every agent's proof stands on trunk
        return "now"
    if it.kind in DIGEST_KINDS:
        return "ride"
    if it.kind == "PLAN":
        return "hold" if it.dest in PLAN_WAKES else "ride"
    if it.kind == "TAKE":   # J-072: a Muse job failed twice on the same failure is Claude's to take now
        return "now"
    if it.kind == "MERGED":   # J-072: the integrator's own merge wakes it at once; Muse's rides the next post
        return "ride" if it.dest == "muse" else "now"
    return "hold"   # Muse's FIX-STUCK and RED-TWICE too: a fix failed twice is the integrator's to route (CLAUDE.md)


def digest_record(it: InboxItem) -> dict:
    """A held entry as the sweep state keeps it between sweeps (J-103; 11-13: any class, with its dest, its failing
    tests and when it was first held)."""
    rec = {"kind": it.kind, "target": it.target, "head": it.head, "line": it.line}
    if it.dest != "claude":
        rec["dest"] = it.dest
    if it.tests:
        rec["tests"] = list(it.tests)
    if it.since:
        rec["since"] = it.since
    return rec


def held_of(records, classes: frozenset[str]) -> tuple[InboxItem, ...]:
    """Held entries back from the sweep state, only those whose class is in `classes`; anything malformed is dropped."""
    out = []
    for r in records if isinstance(records, list) else []:
        if not (isinstance(r, dict) and r.get("kind") in INBOX_ORDER and all(isinstance(r.get(k), str)
                                                                             for k in ("target", "head", "line"))):
            continue
        dest, tests, since = r.get("dest", "claude"), r.get("tests") or [], r.get("since", "")
        if not (isinstance(dest, str) and isinstance(since, str) and isinstance(tests, list)
                and all(isinstance(t, str) for t in tests)):
            continue
        it = InboxItem(r["kind"], r["target"], r["head"], r["line"], dest, tests=tuple(tests), since=since)
        if inbox_class(it) in classes:
            out.append(it)
    return tuple(out)


def digest_of(records) -> tuple[InboxItem, ...]:
    """The held digest entries back from the sweep state: reports only; anything else or malformed is dropped."""
    return held_of(records, frozenset({"ride"}))


def item_text(it: InboxItem, room: int) -> str:
    """One item's lines in an inbox comment, within `room` characters: its failing tests are cut first (the proof
    link in its line names them all), then the line itself (11-13: the relay cuts a long comment)."""
    head = f"- {it.line}"
    tests = [f"  - {untrusted(t, MAX_FAIL_CHARS + 40)}" for t in it.tests]
    for n in range(len(tests), -1, -1):
        more = [f"  - ({len(tests) - n} more failing lines on its proof)"] if n < len(tests) else []
        text = "\n".join([head] + tests[:n] + more)
        if len(text) <= room:
            return text
    return head[:max(0, room - 3)] + "..."


def inbox_body(items: list[InboxItem], held: int, when: str,
               digest: list[InboxItem] = (), digest_left: int = 0) -> str:
    """The inbox comment: the header line first, one line per item, then the digest's one line (J-103). 11-13: no
    keys in it any more (the sweep state holds them)."""
    lines = [INBOX_HEADER, "", f"{len(items)} new for the integrator at {when} (each is listed once per head):" if items
             else f"Nothing to decide at {when}: the digest of what did not wake you:", ""]
    lines += [item_text(it, INBOX_MAX_CHARS) for it in items]
    if held:
        lines.append(f"- ({held} more held for the next post)")
    if digest:
        lines.append("- digest, nothing to decide: " + "; ".join(it.line for it in digest)
                     + (f" (and {digest_left} more next time)" if digest_left else ""))
    return "\n".join(lines).replace("@", "")   # nothing in an inbox comment can mention anyone


def fit_inbox(wake: list[InboxItem], digest: list[InboxItem], when: str
              ) -> tuple[list[InboxItem], list[InboxItem], str]:
    """11-13: the items and digest entries one comment carries, in order, while its whole text stays within
    INBOX_MAX_CHARS (and MAX_INBOX_LINES, MAX_DIGEST_ITEMS); the rest stay held. The first wake item always goes,
    its failing tests cut to fit (item_text). Returns (shown, carried, body)."""
    def body(shown, carried):
        return inbox_body(shown, len(wake) - len(shown), when, carried, len(digest) - len(carried))
    shown: list[InboxItem] = []
    for it in wake[:MAX_INBOX_LINES]:
        if shown and len(body(shown + [it], [])) > INBOX_MAX_CHARS:
            break
        shown.append(it)
    if shown and len(body(shown, [])) > INBOX_MAX_CHARS:   # one item, too long whole: its tests are cut to fit
        room = INBOX_MAX_CHARS - len(body([], [])) - 1
        shown = [dataclasses.replace(shown[0], line=item_text(shown[0], room)[2:], tests=())]
    carried: list[InboxItem] = []
    for it in digest[:MAX_DIGEST_ITEMS]:
        if len(body(shown, carried + [it])) > INBOX_MAX_CHARS:
            break
        carried.append(it)
    return shown, carried, body(shown, carried)


class Switch:
    defer_logs = False   # muse() builds its items with no log read, then reads the logs of the unsent ones (J-104)
    auto_pr_failed: tuple[InboxItem, ...] = ()   # this sweep's AUTO-PR-FAILED items, for the inbox (J-051)
    queue_items: tuple[InboxItem, ...] = ()      # this sweep's QUEUE-LOW item, for the inbox (J-097)
    digest_held: tuple[InboxItem, ...] = ()      # digest entries no inbox comment has carried yet (J-103), in the state
    rerun_heads: frozenset[str] = frozenset()    # heads re-run by an earlier sweep (J-103), from the state
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
        self._board: list[dict] | None = None   # the board's comments, once per run (board_comments)
        self._board_issue: int | None = None    # the board's issue number, once per run (board_issue)

    # ---------- facts ----------
    def fresh_facts(self, number: int) -> tuple[rules.PrFacts, dict]:
        """pr_facts for one PR with every read GitHub's own answer (no ETag cache, no per-run memo)."""
        saved = (self._jobs, getattr(self, "_board", None), getattr(self, "_board_issue", None))
        self._jobs, self._board, self._board_issue = None, None, None
        try:
            with self.gh.fresh():
                pr = self.gh.get(f"repos/{self.gh.repo}/pulls/{number}")
                facts, ctx = self.pr_facts(pr)
        finally:
            self._jobs, self._board, self._board_issue = saved
        ctx["fresh"] = True
        return facts, ctx

    # ---------- the board (J-072) ----------
    def board_issue(self) -> int:
        """The live board's issue number, read once per run: the one open issue labeled BOARD_LABEL (the way
        work/tasks/README.md tells every agent to find it), so a new board needs no code change (#107 to #340 on
        11-09 needed one, and a paste into the deployed bridge). Several open: the newest, and a WARN naming them.
        None, or the read failed: the last board a sweep found (kept in the sweep state, as prior_pr keeps its
        answers), else BOARD_ISSUE, and a WARN; a sweep never stops over this. A pull request is never the board.
        Inside gh.fresh() (the merge facts, fresh_facts) nothing from the cache file takes part: the listing is
        GitHub's own answer, a failed read raises as any other merge fact's does, and the sweep state is not used."""
        known = getattr(self, "_board_issue", None)
        if known:
            return known
        fresh = getattr(self.gh, "fresh_only", False) is True
        label = urllib.parse.quote(BOARD_LABEL, safe="")
        try:   # read as jobs() reads the job issues
            got = self.gh.paged(f"repos/{self.gh.repo}/issues?labels={label}&state=open")
            why = f"no open issue is labeled `{BOARD_LABEL}`"
        except GitHubError as err:
            if fresh:
                raise
            got, why = [], f"the open issues labeled `{BOARD_LABEL}` could not be read ({plain(str(err)[:160])})"
        found = sorted(self._issue_numbers(got))
        self._board_listing = {it["number"]: it for it in (got if isinstance(got, list) else [])   # board_roll's
                               if isinstance(it, dict) and it.get("number") in found} or None   # count, no new read
        last = 0 if fresh else self._last_board()
        if found:
            board = found[-1]
            if len(found) > 1:
                self._board_warn(f"{len(found)} open issues are labeled `{BOARD_LABEL}` "
                                 f"({', '.join(f'#{n}' for n in found)}): using the newest, #{board}; close the others")
            if isinstance(getattr(self.gh, "state", None), dict):
                self.gh.state["board"] = board
        elif last:
            board = last
            self._board_warn(f"{why}: using the last board a sweep found, #{board}")
        else:
            board = BOARD_ISSUE
            self._board_warn(f"{why} and no sweep has found one: using the fallback, #{board}")
        self._board_issue = board
        return board

    def _last_board(self) -> int:
        """The last board a sweep found, kept in the sweep state; 0 when it holds none (or anything but a number)."""
        state = self.gh.state if isinstance(getattr(self.gh, "state", None), dict) else {}
        last = state.get("board")
        return last if type(last) is int and last > 0 else 0

    @staticmethod
    def _issue_numbers(listing) -> list[int]:
        """The issue numbers in an issues listing; a pull request (the listing returns those too) is left out."""
        return [it["number"] for it in (listing if isinstance(listing, list) else [])
                if isinstance(it, dict) and "pull_request" not in it
                and type(it.get("number")) is int and it["number"] > 0]

    def _board_warn(self, text: str) -> None:
        """One WARN line per finding per run (a merge's fresh read resolves the board again)."""
        told = self.__dict__.setdefault("_board_told", set())
        if text not in told:
            told.add(text)
            self.out(f"WARN        board: {text}")

    def old_boards(self) -> list[int]:
        """The closed boards whose muse markers still count (muse): the closed issues labeled BOARD_LABEL and the
        boards closed before the label named them (OLD_BOARDS), the newest MAX_OLD_BOARDS, never the live one. A
        failed read raises: the muse part is retried next sweep, rather than a key already sent going out again."""
        label = urllib.parse.quote(BOARD_LABEL, safe="")
        got = self.gh.get(f"repos/{self.gh.repo}/issues?labels={label}&state=closed&sort=created&direction=desc"
                          "&per_page=30")   # one page: the newest closed ones
        live = self.board_issue()
        closed = (set(self._issue_numbers(got)) | set(OLD_BOARDS) | set(self.rolled_boards())) - {live}
        return sorted(closed, reverse=True)[:MAX_OLD_BOARDS]

    # ---------- the board roll (J-072, #350) ----------
    def rolled_boards(self) -> list[int]:
        """The boards board_roll moved away from: they lose the label, so old_boards' closed listing misses them,
        and their muse and plan marks must still count. The live board's BOARD_FROM mark (no state needed) and the
        sweep state's roll record (the board before, and the ones before it)."""
        item = (getattr(self, "_board_listing", None) or {}).get(getattr(self, "_board_issue", None)) or {}
        m = BOARD_FROM.search(str(item.get("body") or ""))
        rec = self._roll_record() or {}
        return ([int(m.group(1))] if m else []) + ([rec["old"]] + rec["past"] if rec else [])

    def _roll_record(self) -> dict | None:
        """The sweep state's last roll, {old, new, closed, past}, or None when it holds none (or anything else)."""
        rec = self.gh.state.get("board_roll") if isinstance(getattr(self.gh, "state", None), dict) else None
        if not isinstance(rec, dict) or not all(type(rec.get(k)) is int and rec[k] > 0 for k in ("old", "new")):
            return None
        past = rec.get("past") if isinstance(rec.get("past"), list) else []
        past = [n for n in past if type(n) is int and n > 0]
        return {"old": rec["old"], "new": rec["new"], "closed": rec.get("closed") is True, "past": past}

    def board_roll(self) -> None:
        """#350: the switch rolls the board at BOARD_ROLL_AT comments, in steps a later sweep can finish. Each step
        is decided from GitHub's listing of the open `board` issues, the read board_issue already made this run
        (none made: only a recorded close can run), so a normal pass costs no call:
        1. a roll an EARLIER sweep recorded: the old board is closed (never by the sweep that rolled, CONTEXT.md;
           checked on GitHub first, _board_close);
        2. the newest labeled board names (BOARD_FROM) another still labeled: a sweep died after opening it, so the
           label comes off the old one, then the notices and the record (no second board is opened);
        3. the live board names one no longer labeled and the state has no record of it: a sweep died after the
           label came off, so only the record is written (the next sweep closes it);
        4. one labeled board at BOARD_ROLL_AT comments or more: the next one is opened (labeled, board_next), then
           step 2 runs at once.
        Two labeled boards neither names stay as they are (board_issue's WARN). A dry run prints and records nothing."""
        state = self.gh.state if isinstance(getattr(self.gh, "state", None), dict) else None
        rec = self._roll_record()
        if rec and not rec["closed"]:   # an earlier sweep's: a record is made only below, and this runs once a sweep
            self._board_close(rec, state)
            return
        listing = getattr(self, "_board_listing", None)
        if not listing:
            return
        live = max(listing)
        item = listing[live]
        m = BOARD_FROM.search(str(item.get("body") or ""))
        src = int(m.group(1)) if m else 0
        if src and src != live and src in listing:
            self._board_unlabel(listing[src], live)
            return
        if src and src != live and not (rec and rec["old"] == src):
            self._board_record(src, live, rec)
            self.out(f"BOARD       #{live} continues #{src}, which no longer carries `{BOARD_LABEL}`: recorded; "
                     "the next sweep closes it")
            return
        count = item.get("comments")
        if len(listing) > 1 or type(count) is not int or count < BOARD_ROLL_AT:
            return
        title, body = board_next(item)
        got = self.gh.write("POST", f"repos/{self.gh.repo}/issues", {"title": title, "body": body,
                                                                      "labels": [BOARD_LABEL]},
                            f"board: #{live} has {count} comments (roll at {BOARD_ROLL_AT}): open {title!r}")
        new = got.get("number") if isinstance(got, dict) else None
        if type(new) is not int or new <= 0:
            self.out(f"BOARD       #{live} has {count} comments (roll at {BOARD_ROLL_AT}): " +
                     ("would open the next board, take the label off this one, and close it next sweep (dry run)"
                      if self.gh.dry_run else "no issue number came back; the next sweep looks again"))
            return
        self.out(f"BOARD       #{live} has {count} comments (roll at {BOARD_ROLL_AT}): opened #{new}")
        self._board_unlabel(item, new)

    def _board_close(self, rec: dict, state: dict | None) -> None:
        """Roll step 1: close the board an earlier sweep rolled away from. The number comes from the sweep state or a
        board body's mark, so it is checked on GitHub first (one read): never a pull request (the inbox), never an
        issue that carries the board label again or is not older than the board it rolled to (a WARN, and the
        record is done). A `wake:muse` still on it holds the close (the bridge reads only open issues, so a notice
        the rolling sweep posted there would never be mailed); the next sweep looks again."""
        old, new = rec["old"], rec["new"]
        got = self.gh.get(f"repos/{self.gh.repo}/issues/{old}") if old < new else {}
        got = got if isinstance(got, dict) else {}
        names = {lb.get("name") for lb in got.get("labels") or [] if isinstance(lb, dict)}
        why = (f"it is not older than #{new}" if old >= new else "it is a pull request" if "pull_request" in got
               else f"it carries `{BOARD_LABEL}` again" if BOARD_LABEL in names else "")
        if not why and MUSE_LABEL in names:
            self.out(f"BOARD       #{old} stays open for now: `{MUSE_LABEL}` is still on it (a notice not mailed yet)")
            return
        if why:
            self.out(f"WARN        board: #{old} (rolled to #{new}) is not closed: {why}")
        elif got.get("state") != "closed":
            self.gh.write("PATCH", f"repos/{self.gh.repo}/issues/{old}", {"state": "closed", "state_reason": "completed"},
                          f"board: close #{old}, rolled to #{new} by an earlier sweep")
            self.out(f"BOARD       #{old} closed (rolled to #{new} by an earlier sweep)")
        if not self.gh.dry_run and state is not None:
            state["board_roll"] = dict(rec, closed=True)

    def _board_unlabel(self, old: dict, new: int) -> None:
        """Roll step 2: the label off the old board (already off: done), the record, then one line on the new board
        and one inbox line."""
        number = old["number"]
        try:
            label = urllib.parse.quote(BOARD_LABEL, safe="")
            self.gh.write("DELETE", f"repos/{self.gh.repo}/issues/{number}/labels/{label}", None,
                          f"board: take `{BOARD_LABEL}` off #{number}, rolled to #{new}")
        except GitHubError as err:
            if err.status != 404 and "HTTP 404" not in err.detail:
                raise
        self._board_record(number, new, self._roll_record())
        count = old.get("comments")
        self.gh.comment(new, f"Switchboard (board): this board continues #{number}, which reached {count} comments "
                             f"(it rolls at {BOARD_ROLL_AT}); #{number} closes at the next sweep and its comments "
                             "stay there.",
                        f"board: #{new} names #{number}")
        self.gh.comment(INBOX_PR, f"Switchboard (board): the board rolled from #{number} ({count} comments) to "
                                  f"#{new}; #{number} closes at the next sweep. The new body is the old one's, its "
                                  "heading renamed: any number in its text is the integrator's to edit.",
                        f"inbox: the board rolled to #{new}")
        self.out(f"BOARD       #{number} rolled to #{new}: label off; it closes at the next sweep")

    def _board_record(self, old: int, new: int, rec: dict | None) -> None:
        """The roll's record in the sweep state (the next sweep closes `old`), and the fallback board moved to the new
        one (board_issue uses it when the listing fails). Never in a dry run."""
        state = self.gh.state if isinstance(getattr(self.gh, "state", None), dict) else None
        if self.gh.dry_run or state is None:
            return
        past = ([rec["old"]] + rec["past"] if rec and rec["old"] != old else (rec or {}).get("past") or [])
        state["board_roll"] = {"old": old, "new": new, "closed": False, "past": past[:MAX_OLD_BOARDS]}
        state["board"] = new

    def jobs(self) -> dict[str, dict]:
        """PR-review lookup, as before. The plan audit separately finds bare J-NNN issues."""
        if self._jobs is None:
            self._jobs = {}
            for it in self.gh.paged(f"repos/{self.gh.repo}/issues?labels=job&state=open"):
                if "pull_request" not in it and rules.job_of_title(it["title"]):
                    self._jobs[rules.job_of_title(it["title"])] = it
        return self._jobs

    def proof(self, sha: str) -> tuple[str, str, int, str]:
        """The newest proof run on this commit: (state, url, run id)."""
        runs = self.head_runs(sha)
        if not runs:
            return "none", "", 0, ""
        return self.run_state(max(runs, key=lambda r: r["created_at"]))

    def head_runs(self, sha: str) -> list[dict]:
        """The proof runs on one commit, read once per run (J-103: the re-run's once-per-head check reuses the read).
        Inside gh.fresh() (fresh_facts, before a merge) GitHub is read again, never the memo."""
        memo = self.__dict__.setdefault("_head_runs", {})
        if sha not in memo or getattr(self.gh, "fresh_only", False):
            memo[sha] = self.gh.get(f"repos/{self.gh.repo}/actions/workflows/{PROOF_WORKFLOW}/runs?head_sha={sha}"
                                    "&per_page=5")["workflow_runs"]
        return memo[sha]

    def run_jobs(self, run_id: int) -> list[dict]:
        """One proof run's jobs with their steps, read once per run (J-103: the setup-only check reuses the read).
        Inside gh.fresh() GitHub is read again, never the memo."""
        memo = self.__dict__.setdefault("_run_jobs", {})
        if run_id not in memo or getattr(self.gh, "fresh_only", False):
            memo[run_id] = self.gh.get(f"repos/{self.gh.repo}/actions/runs/{run_id}/jobs?per_page=50")["jobs"]
        return memo[run_id]

    def red_tests(self, run_id) -> tuple[str, ...]:
        """J-104: the failing tests of one red proof run, for its notice: for each failed native-tests, godot-touch or
        host-build job (at most MAX_FAIL_JOBS), "<job>: <FAIL line>" per unique FAIL line (fail_lines), read from the job's log
        through the switch's own client and its timeouts. Read once per run, for both audiences (memo). A log that
        cannot be read never stops the notice: its line says LOG_UNREAD and the NOTE line says why (err_text). After
        MAX_LOG_FAILURES timed-out reads in a sweep no more are tried, so a GitHub that hangs costs the sweep a bounded
        wait; any other failure stops only its own run's reads, after MAX_RUN_LOG_FAILURES."""
        memo = self.__dict__.setdefault("_red_tests", {})
        if run_id in memo:
            return memo[run_id]
        out: list[str] = []
        try:
            jobs = self.run_jobs(run_id) if isinstance(run_id, int) else []
        except Exception:   # noqa: BLE001  the notice goes without its tests
            jobs = []
        failed = [j for j in jobs if (j.get("name") or "").startswith(FAIL_LOG_JOBS)
                  and j.get("conclusion") not in ("success", "skipped", "cancelled", None)]
        failures = 0   # this run's failed reads
        for j in failed[:MAX_FAIL_JOBS]:
            name = log_text(j.get("name") or "?", 60)
            if self.__dict__.get("_log_timeouts", 0) >= MAX_LOG_FAILURES or failures >= MAX_RUN_LOG_FAILURES:
                out.append(f"{name}: {LOG_UNREAD}")
                continue
            try:
                log = self.gh.get_bytes(f"repos/{self.gh.repo}/actions/jobs/{int(j['id'])}/logs")
                text = log.decode("utf-8", "replace") if isinstance(log, (bytes, bytearray)) else str(log or "")
            except Exception as err:   # noqa: BLE001  a refused, missing or timed-out log: the notice still goes
                failures += 1
                if isinstance(err, ghapi.TimedOut):
                    self._log_timeouts = self.__dict__.get("_log_timeouts", 0) + 1
                self.out(f"NOTE        the log of {name} in proof run {run_id} was not read ({err_text(err)})")
                out.append(f"{name}: {LOG_UNREAD}")
                continue
            lines, more = fail_lines(text, j.get("name") or "")
            out += [f"{name}: {log_text(line)}" for line in lines] or [f"{name}: no FAIL line in its log"]
            if more:
                out.append(f"{name}: and {more} more FAIL lines")
        if len(failed) > MAX_FAIL_JOBS:
            out.append(f"and {len(failed) - MAX_FAIL_JOBS} more failed test jobs, their logs not read")
        memo[run_id] = tuple(out)
        return memo[run_id]

    def run_state_or_none(self, run: dict) -> tuple[str, str, int, str] | None:
        """run_state, or None when GitHub would not answer its jobs read (11-12: intermittent 502s on
        actions/runs/*/jobs). Only for a side read that the next sweep can make again; a rate limit still raises."""
        try:
            return self.run_state(run)
        except RateLimited:
            raise
        except GitHubError as err:
            self.out(f"NOTE        proof run {run.get('id')}'s jobs were not read ({err_text(err)}): read again next sweep")
            return None

    def run_state(self, run: dict) -> tuple[str, str, int, str]:
        """One proof run, judged by its jobs: (state, url, run id, finished at)."""
        jobs = self.run_jobs(run["id"])
        if run["status"] != "completed" and not only_wake_left(jobs):
            return "running", run["html_url"], run["id"], ""
        failed = [j["name"] for j in jobs if j["name"] not in NOT_COUNTED and j["conclusion"] not in ("success", "skipped")]
        return ("red: " + ", ".join(failed) if failed else "green"), run["html_url"], run["id"], run.get("updated_at") or ""

    def verdicts(self, number: int, job: str | None = None) -> tuple[list[rules.Verdict], list[dict]]:
        """The PR's verdicts and its comments. With `job` (the PR's job, "" for none), the eye's FIX is read again
        under today's look sets (rules.eye_relook, 11-12 #453)."""
        comments = self.gh.paged(f"repos/{self.gh.repo}/issues/{number}/comments")
        reviews = self.gh.paged(f"repos/{self.gh.repo}/pulls/{number}/reviews")
        looks = (getattr(self, "looks", None) or {}).get("sets") if job is not None else None
        out = []
        for c in comments:
            body = c.get("body") or ""
            v = rules.parse_verdict(body, c["created_at"], c["user"]["login"], self.team) or \
                rules.parse_codex_state(body, c.get("updated_at") or c["created_at"], c["user"]["login"]) or \
                rules.parse_codex_receipt(body, c["created_at"], c["user"]["login"])
            # J-056: an automatic review whose MAC checks for this PR, the whole head it names and its verdict.
            if v and rules.is_api_review(v) and signed_for(body, self.review_key, self.gh.repo, number, v.sha,
                                                           signed_verdict(v)):
                v = dataclasses.replace(v, signed=True)
            if v and looks and v.kind == "eye" and v.verdict == "FIX":
                again = rules.eye_relook(v, body, looks, job or "", self.jobs())
                if again.verdict != v.verdict:
                    self.out(f"RELOOK      #{number} the eye's FIX @{v.sha[:8]} is a LAND under today's "
                             "work/looks/sets.yml: every set it failed is now another open job's known defect")
                v = again
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
        job, issue = self.job_issue(pr)
        verdicts, comments = self.verdicts(number, job)
        # Muse's mailed verdicts land on the job issue or the board (the bridge cannot comment on a PR).
        off_pr = self.board_comments() + (self.gh.paged(f"repos/{self.gh.repo}/issues/{issue['number']}/comments")
                                          if issue else [])
        verdicts += self.bridged_verdicts(number, off_pr)
        labels = [l["name"] for l in pr.get("labels", [])] + ([l["name"] for l in issue["labels"]] if issue else [])
        needs = frozenset(l.split(":", 1)[1] for l in labels if l.startswith("needs:"))
        files = self.gh.paged(f"repos/{self.gh.repo}/pulls/{number}/files")
        facts = rules.PrFacts(
            number=number, head_sha=sha, head_ref=pr["head"]["ref"], base_ref=pr["base"]["ref"], draft=bool(pr["draft"]),
            mergeable=full.get("mergeable"), head_committed_at=commit["commit"]["committer"]["date"], proof=proof,
            verdicts=verdicts, needs=needs, rule_check_block=False,
            touches_workflows=any(f["filename"].startswith(".github/workflows/") for f in files),
            codex_failed=rules.codex_failed(comments, sha))
        ctx = {"job": job, "issue": issue, "proof_url": url, "run_id": run_id, "proof_done_at": proof_done_at, "comments": comments,
               "title": pr["title"], "body": pr.get("body") or "", "files": files, "updated_at": pr.get("updated_at") or "",
               "created_at": pr.get("created_at") or "",
               # the head is a branch of this repo, not a fork's: only then is the team token ever used for it
               "own_head": (((pr.get("head") or {}).get("repo") or {}).get("full_name") or "") == self.gh.repo}
        return facts, ctx

    def job_issue(self, pr: dict) -> tuple[str, dict | None]:
        """A PR's job and its open job issue (None for none), as pr_facts reads them; sweep_delta compares the issue."""
        job = rules.job_of_pr(pr.get("body")) or \
            rules.jobs_by_pr_mention({j: it["title"] for j, it in self.jobs().items()}).get(pr["number"], "")
        return job, self.jobs().get(job)

    def target_prs(self, only_pr: int, only_sha: str) -> list[dict]:
        """The open PRs the switch acts on: never the integrator's inbox PR (J-051)."""
        prs = [p for p in self.listing(f"repos/{self.gh.repo}/pulls?state=open") if not is_inbox(p)][:MAX_PRS]
        if only_pr:
            prs = [p for p in prs if p["number"] == only_pr]
        if only_sha:
            prs = [p for p in prs if p["head"]["sha"] == only_sha]
        return prs

    def listing(self, path: str, key: str = "") -> list:
        """A whole-repo listing read once per run (J-072): the open PRs and the proof runs of the inbox window were
        each read two or three times a sweep. Only read-only listings go through here, and the auto-open's PR is
        never one they need (a green branch is never a RED-BRANCH)."""
        memo = self.__dict__.setdefault("_listings", {})
        if (path, key) not in memo:
            memo[(path, key)] = self.gh.paged(path, key)
        return list(memo[(path, key)])

    def comments_since(self, since: dt.datetime) -> list:
        """Every comment updated since `since`, floored to COMMENT_FLOOR (J-072): the URL then repeats for hours, and
        as the listing is oldest first, every page but the newest answers 304. Callers filter to their exact window."""
        floor = comment_floor(since)
        got = self.listing(f"repos/{self.gh.repo}/issues/comments?since={iso_z(floor)}")
        if len(got) < COMMENT_LISTING_CAP:
            return got
        # The listing is oldest first and paged reads stop at the cap, so a full one has lost its NEWEST comments:
        # 11-10, 620 comments in the window, the last ~120 (Muse's mails, new asks) were never read and nothing woke
        # the integrator. Read the window again newest first and merge by id.
        self.out(f"NOTE        comment listing since {iso_z(floor)} hit its {COMMENT_LISTING_CAP} cap; read newest first too")
        newest = self.listing(f"repos/{self.gh.repo}/issues/comments?since={iso_z(since)}&sort=created&direction=desc")
        seen = {c.get("id") for c in got}
        return got + [c for c in reversed(newest) if c.get("id") not in seen]

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
        open_now = {p["number"] for p in self.listing(f"repos/{self.gh.repo}/pulls?state=open")}
        for n in rules.superseded_prs(ctx["body"], facts.number):
            if n == INBOX_PR:   # the integrator's inbox is never closed (J-051)
                continue
            if n not in open_now:   # J-072: closed, merged, an issue or gone: nothing to close, and no read to know it
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
        for _ in range(MERGEABLE_READS):
            if d.action != "WAIT" or d.reasons != ("GitHub is still computing mergeability",):
                break
            # the first read starts GitHub's computation. Trunk moves every few minutes (notes commits), which resets it,
            # so one read 3 s later was often still null (11-12, #453 in a dry run): read again, GitHub's own answer
            time.sleep(MERGEABLE_WAIT_S)
            with self.gh.fresh():
                facts = dataclasses.replace(facts, mergeable=self.gh.get(f"repos/{self.gh.repo}/pulls/{facts.number}").get("mergeable"))
            d = rules.merge_decision(facts, author, self.team, token, key)
        if d.action in ("MERGE", "HUMAN-MERGE") and not ctx.get("fresh"):
            # J-072, the security pass on #360: the sweep's reads may be answered from the ETag cache, whose file
            # an older run saved. A merge, and a MERGE-BY-HAND line in the inbox, is decided only on GitHub's own
            # answers: the PR, its head, proof runs and jobs, comments, reviews, job issue and board, read again uncached.
            facts, ctx = self.fresh_facts(facts.number)
            author = self.team.vendor_of_branch(facts.head_ref)
            d = rules.merge_decision(facts, author, self.team, token, key)
            self.out(f"FRESH       #{facts.number} merge facts read again from GitHub, uncached: {d.action}")
        if d.action in ("MERGE", "HUMAN-MERGE"):
            with self.gh.fresh():   # the rule check and the decision on it: no answer from the cache either
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
        if getattr(self, "merge_only", False):   # J-072 rate floor: the merge path only, no review asks
            return
        if rules.needs_review(facts, author, self.team) and self.stale(facts):
            self.out(f"STALE       #{facts.number} head older than {ACT_WITHIN_DAYS} days: no review asked")
        elif rules.needs_review(facts, author, self.team) and self.codex_first(facts, ctx, author):
            self.out(f"ASK-CODEX   #{facts.number} Codex is asked to review @{facts.head_sha[:8]}, once for this head")
        elif rules.needs_review(facts, author, self.team) and self.codex_pending(facts, ctx):
            self.out(f"WAIT-CODEX  #{facts.number} it is Codex's turn on @{facts.head_sha[:8]}: no other review for "
                     f"{rules.CODEX_START_MINUTES} minutes, or up to {rules.CODEX_WAIT_MINUTES} while its review runs")
        elif rules.needs_review(facts, author, self.team) and self.team.pool:
            _, who, asked = self.pooled_reviewer(facts, ctx, author)
            if not who:
                self.out(f"NOFALLBACK  #{facts.number} everyone in the pool was asked and nobody answered")
            elif who != rules.API_REVIEWER and who not in asked:
                quiet = [self.team.agent_name.get(a, a) for a in asked if a != "codex"]
                why = (f"{', '.join(quiet)} did not answer in {self.team.fallback_after_hours:g} hours, "
                       f"so you are asked instead. ") if quiet else ""
                # a NOTE lands nothing (rules.needs_review): the next reviewer is told what is wanted. The names are
                # the team's own (work/agents.yml), never text taken from a comment.
                noted = sorted({"the automatic Claude review" if rules.is_api_review(v) else
                                self.team.agent_name.get(self.team.agent_of_name.get(v.who.lower(), ""), "")
                                for v in facts.verdicts if v.kind == "review" and v.verdict == "NOTE"
                                and rules.names_head(v, facts.head_sha)} - {""})
                if noted:
                    who_noted = ", ".join(noted)
                    why = (f"{who_noted[0].upper()}{who_noted[1:]} gave this head a NOTE (it is on #{facts.number}), "
                           f"which lands nothing: a LAND or a FIX is asked of you. ") + why
                self.ask_review(facts, ctx, who, why)
                self.note_assigned(who)
        elif rules.needs_review(facts, author, self.team):
            pick = rules.pick_reviewer(author, facts.needs, self.team)
            if pick.route == "ask":
                self.ask_review(facts, ctx, pick.agent, "")
            self.escalate_review(facts, ctx, author)

    def codex_counts(self, author: str, ctx: dict) -> bool:
        """Is Codex asked to review this PR: it is on the team, its review can land this author's work
        (rules.land_counts: another vendor's), and the head is a branch of this repo (a fork's PR never gets the
        team login's mention). Its unasked findings still block anyone's PR."""
        return bool(ctx.get("own_head")) and self.team.agent_vendor.get("codex") not in (None, "unknown", author)

    def codex_asked(self, facts: rules.PrFacts, ctx: dict) -> str:
        """When Codex was asked to review this head, "" when it was not: the switch's ask mark for the head (posted
        with the team token, or by the workflow bot as a note for the integrator), or a team login's own
        `@codex review` on the PR after the head commit. One ask per head: Codex's usage is limited."""
        tag = MARK.format(kind="ask-codex", pr=facts.number, head=facts.head_sha)
        times = []
        for c in ctx.get("comments") or []:
            login = ((c.get("user") or {}).get("login") or "").strip().lower()
            body = c.get("body") or ""
            if login == rules.BOT_LOGIN.lower():
                mine = tag in body
            elif login in self.team.verdict_logins:
                mine = tag in body or (body.lstrip().lower().startswith(CODEX_MENTION)
                                       and (c.get("created_at") or "") >= facts.head_committed_at)
            else:
                mine = False   # the repo is public: anyone's comment could carry the mark
            if mine:
                times.append(c.get("created_at") or "")
        return max(times, default="")

    def codex_reactions(self, facts: rules.PrFacts, ctx: dict) -> list:
        """The PR's reactions (Codex puts 👀 on it while it reviews), read once per PR and run."""
        if "_reactions" not in ctx:
            ctx["_reactions"] = self.gh.paged(f"repos/{self.gh.repo}/issues/{facts.number}/reactions")
        return ctx["_reactions"]

    def codex_first(self, facts: rules.PrFacts, ctx: dict, author: str) -> bool:
        """Codex reviews each green head once, before anyone else is asked or paid (Cory, 11-04). It reviews a PR
        by itself only when a person's login opens it, and only that first head; the PRs the switchboard opens
        for Muse and every later head got none (#376, #367), so the switch asks: once per head, and not while
        Codex is already on it. True when the ask was made in this call."""
        if not self.codex_counts(author, ctx) or self.codex_asked(facts, ctx):
            return False
        if rules.codex_in_flight(ctx.get("comments") or [], self.codex_reactions(facts, ctx), facts.head_sha):
            return False
        self.ask_review(facts, ctx, "codex", "")
        return True

    def codex_pending(self, facts: rules.PrFacts, ctx: dict) -> bool:
        """It is still Codex's turn on this head (rules.codex_turn): wait for it rather than ask or pay for a
        second review (J-031). Past the wait, the review goes on as if Codex were not there.
        J-072 (plan step 7): while Codex is expected and has not been asked on the head, it is still its turn, however
        long ago the proof went green; once asked, its clock runs from the ask. A review Codex runs unasked (it is
        never asked then) keeps the old clock, so a stuck running state cannot hold the head for ever."""
        if rules.codex_failed(ctx.get("comments") or [], facts.head_sha):
            return False   # Codex could not review this head (its usage limit): no wait
        now = self.now.strftime("%Y-%m-%dT%H:%M:%SZ")
        expected = self.codex_counts(self.team.vendor_of_branch(facts.head_ref), ctx)
        asked = self.codex_asked(facts, ctx)
        if expected and asked:
            since = [asked]   # the start clock runs from the ask, not from when the proof went green
        elif expected and not rules.codex_in_flight(ctx.get("comments") or [], self.codex_reactions(facts, ctx),
                                                    facts.head_sha):
            return True   # not asked yet: the steer part asks it first; no paid review before that
        else:
            since = [ctx.get("proof_done_at") or "", asked]
            if expected:
                since.append(ctx.get("created_at") or "")   # a PR the switch just opened was green long before
        if not rules.codex_turn(since, True, expected, now):
            return False   # past every wait: no reaction read
        if rules.codex_turn(since, False, expected, now):
            return True
        return rules.codex_in_flight(ctx.get("comments") or [], self.codex_reactions(facts, ctx), facts.head_sha)

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
        done = rules.reviewers_done(facts.verdicts, facts.head_sha, self.team)   # a NOTE-giver is not asked twice
        api_ok = (self.api_ok(ctx) if api is None else api) and not api_failed
        rotation = rules.review_rotation(facts.number, author, facts.needs, self.team, api_ok, done)
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
                raise MergeTokenRefused(f"{TOKEN_REFUSED} (HTTP {code}): {MERGE_TOKEN_ENV} needs "
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
        self.merged_notice(facts, proof)
        return ""

    def merged_notice(self, facts: rules.PrFacts, proof: str) -> None:
        """J-072: this merge as one MERGED inbox item, kept in the sweep state until an inbox post lists it (never
        in a dry run). Its dest is the branch's owner: Muse's (fb-bot/) rides the next post, any other wakes at once."""
        state = getattr(self.gh, "state", None)
        if getattr(self.gh, "dry_run", False) or not isinstance(state, dict):
            return
        it = InboxItem("MERGED", f"#{facts.number}", facts.head_sha,
                       f"MERGED #{facts.number} {untrusted(facts.head_ref)} at {facts.head_sha[:8]}: the switchboard "
                       f"merged it; trunk's proof now runs ({proof.rstrip('.')})",
                       "muse" if owner_of(facts.head_ref) == "muse" else "claude")
        kept = [r for r in state.get("merged") or [] if isinstance(r, dict)
                and (r.get("target"), r.get("head")) != (it.target, it.head)]
        state["merged"] = (kept + [digest_record(it)])[-MAX_MERGED_HELD:]

    def merged_items(self) -> tuple[InboxItem, ...]:
        """The MERGED items no inbox post has listed yet (merged_notice), from the sweep state."""
        state = getattr(self.gh, "state", None)
        return held_of(state.get("merged"), frozenset({"now", "ride"})) if isinstance(state, dict) else ()

    def plan_line(self, job: str):
        """J-072: the job's line in work/plan.md, read once per run; None when it has none or the plan cannot be read."""
        memo = self.__dict__.setdefault("_plan_owner", {})
        if "model" not in memo:
            try:
                memo["model"] = self.read_plan().by_job()
            except (AttributeError, RuntimeError) as err:
                self.out(f"NOTE        work/plan.md not read ({err_text(err)})")
                memo["model"] = None
        return (memo["model"] or {}).get(job)

    def plan_owner(self, job: str) -> str | None:
        """J-072: the job's owner in work/plan.md, or None when the plan cannot be read (a TAKE then stays open)."""
        line = self.plan_line(job)
        return line.owner if line is not None else None

    def plan_held(self, job: str) -> bool:
        """J-072 (11-14, QUIET on #466 while J-015 was paused): the job has a `hold:` in work/plan.md, so its PR going
        quiet is expected. An unread plan holds nothing: the nag still goes."""
        line = self.plan_line(job) if job else None
        return bool(line is not None and line.hold)

    def note_red_head(self, f: rules.PrFacts, ctx: dict, rid) -> dict | None:
        """J-072: a Muse job's PR head, once per sweep. A green (rid None) forgets the branch's reds. A red is kept
        with its failures (red_tests, read once per head, then from the sweep state); when an earlier red head since
        the last green shares one that trunk's red proof does not, the job is one open TAKE. Returns the job's open
        TAKE, or None. Never in a dry run."""
        state = getattr(self.gh, "state", None)
        job = ctx.get("job") or ""
        if not job or not isinstance(state, dict) or getattr(self.gh, "dry_run", False):
            return None
        takes = state["takes"] if isinstance(state.get("takes"), dict) else {}
        memo = self.__dict__.setdefault("_red_noted", {})
        if (f.head_ref, f.head_sha) in memo:
            return memo[(f.head_ref, f.head_sha)]
        heads = state["red_heads"] if isinstance(state.get("red_heads"), dict) else {}
        state["red_heads"], state["takes"] = heads, takes
        memo[(f.head_ref, f.head_sha)] = takes.get(job)
        if rid is None:
            heads.pop(f.head_ref, None)
            return None
        kept = [e for e in heads.get(f.head_ref) or [] if isinstance(e, dict) and isinstance(e.get("tests"), list)]
        if any(e.get("head") == f.head_sha for e in kept):
            return takes.get(job)
        tests = self.red_tests(rid)
        if any(t.endswith(f": {LOG_UNREAD}") for t in tests):
            return takes.get(job)   # a log unread: this head is read again next sweep
        failures = {same_failure(t) for t in tests} or {f"{j}: red" for j in red_jobs(f.proof)}
        trunk = state["trunk_proof"] if isinstance(state.get("trunk_proof"), dict) else {}   # as trunk_proof keeps it
        if str(trunk.get("state", "")).startswith("red"):   # a failure trunk shares is not this job's
            failures -= {same_failure(t) for t in self.red_tests(trunk.get("run"))} | \
                {f"{j}: red" for j in red_jobs(trunk["state"])}
        shared = sorted({t for e in kept for t in e["tests"]} & failures)
        heads.pop(f.head_ref, None)   # newest last, so the oldest branch goes first past the cap
        heads[f.head_ref] = (kept + [{"head": f.head_sha, "run": rid, "tests": sorted(failures)[:20]}])[-TAKE_HEADS_KEPT:]
        while len(heads) > MAX_TAKE_BRANCHES:
            heads.pop(next(iter(heads)))
        if shared and job not in takes and self.plan_owner(job) != "claude":
            takes[job] = {"job": job, "pr": f.number, "branch": f.head_ref, "head": f.head_sha,
                          "tests": [t[:MAX_FAIL_CHARS] for t in shared[:MAX_FAIL_LINES]], "at": iso_z(self.now),
                          "listed": ""}
            while len(takes) > MAX_TAKES:
                takes.pop(next(iter(takes)))
            self.out(f"TAKE        {job} #{f.number}: red twice on the same failure ({len(shared)} test line(s))")
        memo[(f.head_ref, f.head_sha)] = takes.get(job)
        return takes.get(job)

    def take_items(self) -> list[InboxItem]:
        """J-072: the integrator's TAKE items: each open take, at once and again at most once per ASK_AGAIN (a new key
        per listing, the first keeps the bare head), until work/plan.md gives the job to claude, which closes it."""
        state = getattr(self.gh, "state", None)
        takes = state.get("takes") if isinstance(state, dict) else None
        if not isinstance(takes, dict):
            return []
        out: list[InboxItem] = []
        dry = getattr(self.gh, "dry_run", False)
        for job, rec in list(takes.items()):
            if not (isinstance(rec, dict) and all(isinstance(rec.get(k), str) for k in ("head", "branch", "listed"))
                    and isinstance(rec.get("pr"), int) and isinstance(rec.get("tests"), list)):
                takes.pop(job, None)
                continue
            if self.plan_owner(job) == "claude":
                if not dry:
                    takes.pop(job, None)
                    self.out(f"TAKE        {job} closed: work/plan.md gives it to claude")
                continue
            last = parse_time(rec["listed"])
            if last is not None and self.now - last < ASK_AGAIN:
                continue
            head = f"{rec['head']}@{iso_z(self.now)}" if last is not None else rec["head"]
            age = hours(self.now - (parse_time(str(rec.get("at") or "")) or self.now))
            out.append(InboxItem("TAKE", job, head,
                                 f"TAKE {job} [#{rec['pr']}](https://github.com/{self.gh.repo}/pull/{rec['pr']}) "
                                 f"{untrusted(rec['branch'])} head {rec['head'][:8]}: Muse's proof went red on two "
                                 f"heads on the same failing tests, so Claude takes it (work/tasks/job-routing.md). "
                                 f"Muse was mailed to push and stop. Give it to claude in work/plan.md and start a "
                                 f"helper; listed each hour until the plan does (open {age})",
                                 tests=tuple(str(t) for t in rec["tests"])))
            if not dry:
                rec["listed"] = iso_z(self.now)
        return out

    # ---------- auto-open (J-047) ----------
    def prior_pr(self, branch: str, sha: str) -> bool:
        """Has this head of this branch ever been a PR's head (open, closed or merged)? The head filter names the
        branch; a PR counts only at its own head, so a branch reused past a merged or closed PR gets a new one
        (SYSTEM_PLAN item 2). A yes never turns into a no, so it is kept in the sweep state as "branch@sha" (J-072:
        a PR listing answers 200 even with its ETag, so the warm full pass spent 45 of its 72 counted calls asking
        again about branches that had a PR long ago)."""
        state = self.gh.state if isinstance(getattr(self.gh, "state", None), dict) else {}
        known = state.setdefault("prior_pr", [])
        key = f"{branch}@{sha}"
        if key in known:
            return True
        owner = self.gh.repo.split("/")[0]
        prs = self.gh.paged(f"repos/{self.gh.repo}/pulls?state=all&head={owner}:{branch}")
        had = any((p.get("head") or {}).get("sha") == sha for p in prs or [])
        if had:
            known.append(key)
        return had

    def open_pr(self, branch: str, sha: str, job: str, message: str, title: str = "") -> str:
        """Open the PR for a finished branch: title is `title`, else the commit subject; body starts with the Job
        line. Returns "" when opened (or in a dry run), else why not."""
        subject, _, rest = message.partition("\n")
        body = f"Job: {job}\n\nOpened by the switchboard from {branch} @ {sha}"
        if rest.strip():
            body += f"\n\n{rest.strip()}"
        got = self.gh.write("POST", f"repos/{self.gh.repo}/pulls",
                            {"title": title or subject, "head": branch, "base": rules.TRUNK, "body": body},
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
        state = getattr(self.gh, "state", None)
        if isinstance(state, dict) and f"{branch}@{sha}" in (state.get("auto_pr_old") or []):
            return   # #350: an old head listed once already; the open is still tried, only the listing is not
        self.auto_pr_failed += (InboxItem("AUTO-PR-FAILED", branch, sha, f"AUTO-PR-FAILED {where}: {text}"),)

    def auto_pr_aged(self, key: str, committed: str) -> None:
        """#350 (60 of 69 inbox lines in six posts were AUTO-PR-FAILED lines for heads from weeks before, each re-listed
        once the inbox's ring had lost its key): a head older than AUTO_PR_OLD whose AUTO-PR-FAILED item the inbox has
        listed is recorded in the sweep state ("auto_pr_old", at most MAX_AUTO_PR_OLD, the heads still live), and is
        never listed again. Its PR is still opened when it can be (a plan line given later)."""
        state = getattr(self.gh, "state", None)
        at = parse_time(committed)
        if (not isinstance(state, dict) or at is None or self.now - at <= AUTO_PR_OLD
                or getattr(self.gh, "dry_run", False)):
            return
        old = [k for k in state.get("auto_pr_old") or [] if isinstance(k, str)]
        if key not in old:
            state["auto_pr_old"] = (old + [key])[-MAX_AUTO_PR_OLD:]

    def auto_open_prs(self, only: set[str] | None = None) -> int:
        """Open PRs for finished fb-bot/integrate-* and claude/integrate-* heads with no PR yet (J-047).

        SYSTEM_PLAN item 2: the job is the head commit's `Job: J-NNN` line, else the branch name's
        (plan.job_of_branch); a head whose subject starts "Merge" is titled "J-NNN: <the plan line's title>". A head
        opens when the proof on it is green, the plan has a line for its job, the line has no `hold:`, the plan does
        not list the head as `kept`, and no PR ever had this head (a branch past its merged or closed PR opens
        again). A held job or a kept head opens nothing. A green head with no job, or a job the plan has no line
        for, is one AUTO-PR-FAILED item per head. At most MAX_AUTO_PRS per sweep, in branch-name order. Every
        failure on a branch is an AUTO-PR-FAILED item for this sweep's inbox (J-051); GitHub's refusal also stops
        this sweep's auto-open. `only` (a fast sweep, J-072): just these branches, whose proof run just finished.
        Returns how many PRs were opened (or would be, in a dry run)."""
        self.auto_pr_failed = ()
        # 11-12: 647 branches, and paged() stops at 500 without saying so, so a finished branch past it never got
        # its PR. Once the plan is live every page is read (past ALL_PAGES it raises). Until then this reads what
        # it always read: the rebuild changes nothing the live switch does while it is in shadow.
        path = f"repos/{self.gh.repo}/branches"
        listed = collab_plan.whole(self.gh, path) if self.plan_mode() == "live" else self.gh.paged(path)
        state = self.gh.state if isinstance(getattr(self.gh, "state", None), dict) else {}
        if only is None and isinstance(getattr(self.gh, "state", None), dict):   # forget heads gone or moved
            current = {f"{b['name']}@{b['commit']['sha']}" for b in listed}
            state["prior_pr"] = [k for k in state.get("prior_pr") or [] if k in current]
            state["auto_pr_old"] = [k for k in state.get("auto_pr_old") or [] if k in current][-MAX_AUTO_PR_OLD:]
        branches = [b for b in listed if b["name"].startswith(AUTO_PR_PREFIX) and (only is None or b["name"] in only)]
        if not branches:
            return 0
        model = self.read_plan()
        lines = model.by_job()
        kept = {(ref, ksha) for ln in model.lines for ref, ksha in ln.kept}
        # a no-plan-job item is made once per head: until the inbox lists it (its ring, inbox_box), not just once made,
        # so a sweep whose inbox never ran (a fast or quiet one) loses nothing
        box = state.get("inbox") if isinstance(state.get("inbox"), dict) else {}
        listed_keys = {k for k in box.get("keys") or [] if isinstance(k, str)}
        open_refs = {p["head"]["ref"] for p in self.listing(f"repos/{self.gh.repo}/pulls?state=open")}
        opened = 0
        for b in sorted(branches, key=lambda x: x["name"]):
            if opened >= MAX_AUTO_PRS:
                break
            branch, sha, job = b["name"], b["commit"]["sha"], ""
            if branch in open_refs:
                continue
            key = f"{branch}@{sha}"
            try:
                if key in (state.get("prior_pr") or []):
                    self.out(f"SKIP       {branch} @{sha[:8]}: head already had a PR")
                    continue
                if any(ref == branch and sha.startswith(ksha) for ref, ksha in kept):
                    self.out(f"SKIP       {branch} @{sha[:8]}: the plan keeps this head as a record")
                    continue
                commit = self.gh.get(f"repos/{self.gh.repo}/commits/{sha}")["commit"]
                message = commit["message"]
                job = rules.job_of_pr(message) or collab_plan.job_of_branch(branch)   # the Job: line, else the name
                line = lines.get(collab_plan.plan_job(job, lines)) if job else None
                if inbox_key("AUTO-PR-FAILED", branch, sha) in listed_keys:
                    self.auto_pr_aged(key, (commit.get("committer") or {}).get("date") or "")
                    if line is None:
                        continue
                if line is None and key in (state.get("auto_pr_old") or []):
                    continue
                if line is not None and line.hold:
                    self.out(f"SKIP       {branch} @{sha[:8]} ({job}): the plan holds {line.job}")
                    continue
                if self.prior_pr(branch, sha):
                    self.out(f"SKIP       {branch} @{sha[:8]}: head already had a PR")
                    continue
                proof, url, _, _ = self.proof(sha)
                if proof != "green":
                    self.out(f"SKIP       {branch} @{sha[:8]} ({job or 'no job'}): proof is {proof}")
                    continue
                if line is None:
                    why = (f"{job} has no line in work/plan.md" if job else
                           "its head commit has no `Job: J-NNN` line and its name no job")
                    self.out(f"NOTE  {branch} @{sha[:8]} is green with no PR: {why}; nothing opened")
                    where = f"{untrusted(branch)} head {sha[:8]}" + (f", {job}" if job else "")
                    self.auto_pr_failed += (InboxItem(
                        "AUTO-PR-FAILED", branch, sha,
                        f"AUTO-PR-FAILED {where}: green with no PR, and {why}, so the switch opens none. The "
                        f"integrator gives the job a plan line (the next sweep opens it) or opens the PR by hand."),)
                    continue
                subject = message.partition("\n")[0]
                why = self.open_pr(branch, sha, job, message,
                                   f"{job}: {line.title}" if subject.startswith("Merge") else "")
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
    def inbox_box(self) -> dict:
        """11-13: the inbox's own part of the sweep state, saved with the signed cache (ghapi.save_etags): "keys",
        every key listed (the ring, MAX_INBOX_KEYS), "at", the last post, and "held", wake items not posted yet."""
        state = getattr(self.gh, "state", None)
        if not isinstance(state, dict):
            state = self.gh.state = {}
        box = state.get("inbox")
        if not isinstance(box, dict):
            box = state["inbox"] = {}
        return box

    def inbox_due(self, held: list[InboxItem], digest: list[InboxItem], last: dt.datetime | None) -> bool:
        """11-13: does a comment go now? An ask: at once. Else only INBOX_HOLD after the last post, and only for the
        integrator's own work held. The digest never makes a post: every #282 comment wakes the subscribed
        integrator, so a digest-only post ("Nothing to decide", 6854673147) woke it for nothing. Its entries ride
        the next post that carries a wake item, as many as fit, the rest the one after (`digest` is kept in the
        signature for the callers)."""
        if any(inbox_class(it) == "now" for it in held):
            return True
        if last is not None and self.now - last < INBOX_HOLD:
            return False
        return bool(held)

    def inbox_clock(self) -> bool:
        """11-13: a quiet sweep's question: is a held wake item due by its clock alone (the digest never is)? Reads no GitHub."""
        box = self.inbox_box()
        return self.inbox_due(list(held_of(box.get("held"), frozenset({"now", "hold"})))
                              + [it for it in self.merged_items() if inbox_class(it) == "now"], list(self.digest_held),
                              parse_time(str(box.get("at") or "")))

    def inbox(self, steered: list[Steered], flush: bool = False) -> int:
        """After a sweep, at most one comment on the inbox PR listing what needs the integrator and was not listed
        before. An item's key (kind, PR or branch, head sha) is listed once; a new head is a new key. 11-13: the keys
        live in the sweep state's ring (inbox_box), no longer in the comment (inbox_reported still reads the old
        markers). Each item has a class (inbox_class): an ask posts at once; the integrator's own work posts at most
        once per INBOX_HOLD, riding any ask sooner; a report (J-103's digest) never posts alone, not even with
        `flush`: it rides the next post that carries a wake item (inbox_due). What waits is held in the state, never
        dropped: a comment carries what fits under INBOX_MAX_CHARS (fit_inbox) and the rest waits for the next. `flush` posts the wake items held now. Returns how
        many items were posted (or printed, in a dry run), digest entries included."""
        box = self.inbox_box()
        reported = self.inbox_reported()
        posted_before = frozenset(reported)
        when = iso_z(self.now)
        heads = {f"#{s.facts.number}": s.facts.head_sha for s in steered}
        held: dict[tuple, InboxItem] = {}   # (kind, target, head): one item, however its tests read
        for it in held_of(box.get("held"), frozenset({"now", "hold"})):
            # a PR read this sweep at a newer head: this head's items are moot, the new head's come below
            if not it.listed(posted_before) and heads.get(it.target, it.head) == it.head:
                held.setdefault((it.kind, it.target, it.head), it)
        # held items count as listed for the reads: no log is read again for one, unless its log went unread
        seen = set(reported) | {it.key for it in held.values()}
        new: list[InboxItem] = []
        for it in self.inbox_items(steered, frozenset(seen)):
            if not it.line or it.listed(seen):
                continue
            seen.add(it.key)
            it = dataclasses.replace(it.read_now(posted_before), since=when)
            prev = held.get((it.kind, it.target, it.head))
            if prev is not None:   # held with its log unread, read now: the read copy replaces it, its clock kept
                if prev.unread and not it.unread:
                    held[(it.kind, it.target, it.head)] = dataclasses.replace(it, since=prev.since)
                continue
            new.append(it)
            if inbox_class(it) != "ride":
                held[(it.kind, it.target, it.head)] = it
        wake = sorted(held.values(), key=lambda it: (inbox_class(it) != "now", INBOX_ORDER.index(it.kind)))
        rides: dict[str, InboxItem] = {}
        for it in list(self.digest_held) + [it for it in new if inbox_class(it) == "ride"]:
            if it.key not in reported:
                rides.setdefault(it.key, it if it.since else dataclasses.replace(it, since=when))
        digest = list(rides.values())
        self.inbox_keys = set(reported)   # J-097: what the inbox holds after this call (run_sweep's queue clock)
        last = parse_time(str(box.get("at") or ""))
        self.digest_held = self.keep_held(digest, MAX_DIGEST_HELD, "digest")
        if not getattr(self.gh, "dry_run", False):   # kept until a comment carries it, even if the post fails
            box["held"] = [digest_record(it) for it in self.keep_held(wake, MAX_INBOX_HELD, "wake")]
        if not wake or not (flush or self.inbox_due(wake, digest, last)):   # a digest alone never posts
            self.out("INBOX       nothing new" + (f"; {len(wake)} held for the next post (the hold)" if wake else "")
                     + (f"; {len(digest)} held for the digest (J-103)" if digest else ""))
            return 0
        shown, carried, body = fit_inbox(wake, digest, central(self.now))
        posted = wake[:len(shown)] + digest[:len(carried)]   # the items as held: their keys, whatever fit_inbox cut
        self.out(body)
        self.gh.comment(INBOX_PR, body, f"inbox: {len(shown)} new item(s)"
                                        + (f" and {len(carried)} digest entries" if carried else "") + f" on #{INBOX_PR}")
        self.inbox_keys |= {it.key for it in posted}
        self.digest_held = tuple(digest[len(carried):][-MAX_DIGEST_HELD:])
        if not getattr(self.gh, "dry_run", False):
            ring = [k for k in box.get("keys") or [] if isinstance(k, str)]
            # the old comment markers (inbox_reported reads them back every sweep) are the oldest keys, so they go
            # first and are cut first: after the ring they pushed its newest keys out once over the cap (#350, 11-13)
            in_ring = set(ring)
            old = [k for k in reported if k not in in_ring]
            box["keys"] = list(dict.fromkeys(old + ring + [it.key for it in posted]))[-MAX_INBOX_KEYS:]
            box["at"] = when
            box["held"] = [digest_record(it) for it in wake[len(shown):][-MAX_INBOX_HELD:]]
            listed = set(box["keys"])   # J-072: a MERGED item a post listed is never kept, nor listed, again
            if isinstance(self.gh.state.get("merged"), list):
                self.gh.state["merged"] = [r for r in self.gh.state["merged"] if not (isinstance(r, dict) and
                                           inbox_key("MERGED", str(r.get("target")), str(r.get("head"))) in listed)]
        return len(shown) + len(carried)

    def keep_held(self, items: list[InboxItem], cap: int, what: str) -> tuple[InboxItem, ...]:
        """The newest `cap` held items; any dropped are named in the run's log, never lost silently."""
        if len(items) > cap:
            self.out(f"NOTE        inbox: {len(items) - cap} {what} item(s) over the cap of {cap} dropped, the oldest: "
                     + plain("; ".join(it.line[:80] for it in items[:len(items) - cap][:3])))
        return tuple(items[-cap:])

    def muse_keys(self, comments: list[dict]) -> list[str]:
        """The muse-<KIND> keys in the switch's own muse markers on these comments, oldest first."""
        keys: list[str] = []
        for c in comments:
            if (c.get("user") or {}).get("login") == rules.BOT_LOGIN:
                for m in MUSE_KEYS.finditer(c.get("body") or ""):
                    keys += [k for k in m.group(1).split(",") if k]
        return list(dict.fromkeys(keys))

    def muse(self, steered: list[Steered]) -> int:
        """Muse's items (J-052): one comment per fb-bot/ PR or branch with new items, on the PR's job issue or the board
        (never the PR: the bridge's Issues-only token cannot clear a PR's label), labeled wake:muse so
        the bridge mails it. A muse-<KIND> key is sent once; a new head is a new key. At most
        MAX_MUSE_COMMENTS per sweep; the rest come next sweep. Returns how many comments were posted.
        A key counts as delivered only when its label write succeeded: the marker is edited into the
        comment after the label lands, so a failed label leaves no marker and the next sweep retries."""
        reported: set[str] = set()
        for s in steered:
            reported |= set(self.muse_keys(s.ctx.get("comments") or []))
        self.defer_logs = True   # the keys sent are known only below: no log is read before they are (J-104)
        try:
            items = [it for it in self.muse_items(steered, frozenset(reported)) if it.line]
        finally:
            self.defer_logs = False
        homes = {f"#{s.facts.number}": self.muse_home(s.ctx) for s in steered}   # 0: no job issue, so the board
        board = self.board_issue() if items else 0
        wheres = {homes.get(it.target) or board for it in items}
        for where in sorted(wheres - {board}):   # a job issue: where this sweep's notices on its PRs go
            reported |= set(self.muse_keys(self.gh.paged(f"repos/{self.gh.repo}/issues/{where}/comments")))
        if board in wheres:
            reported |= set(self.muse_keys(self.gh.paged(f"repos/{self.gh.repo}/issues/{board}/comments")))
            for old in self.old_boards():   # a key sent while an older board was live is still sent (11-09 re-sent them)
                reported |= set(self.muse_keys(self.bounded_comments(old, MAX_OLD_BOARD_PAGES)))
        seen, new = set(reported), []
        for it in items:
            if it.run_id is not None and not it.tests:
                if inbox_key("muse-" + it.kind, it.target, it.head) in seen:
                    continue   # sent with its tests (or before UNREAD_KEY): its logs are not read again
                it = dataclasses.replace(it, tests=self.red_tests(it.run_id))
            if not it.listed(seen, "muse-"):
                seen.add(it.muse_key)
                new.append(it.read_now(seen, "muse-"))
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
            # Never on the PR itself: the bridge's token is Issues-only, so it can neither remove wake:muse from a
            # PR nor comment there (a PR's label stuck the bridge from 11-07 to 11-09). The PR stays in the subject.
            where = homes.get(target) or board
            name = jobs.get(target) or target
            kinds = " ".join(sorted({it.kind for it in group}, key=INBOX_ORDER.index))
            about = target if target.startswith("#") else f"#{where}"
            lines = [f"CLAUDE_TO_MUSE: {name} {kinds} on {about} @ {group[0].head[:8]}", ""]
            for it in group:
                lines.append(f"- {it.line}")
                lines += [f"  {line}" for line in it.finding.split("\n")] if it.finding else []
                lines += [f"  {t}" for t in it.tests]
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

    @staticmethod
    def muse_home(ctx: dict) -> int:
        """Where a Muse notice about a PR goes: its open job issue, else 0, the board. Never the PR (see muse())."""
        return int((ctx.get("issue") or {}).get("number") or 0)

    def board_comments(self) -> list[dict]:
        """The board's comments, read once per run (the review asks and off-PR verdicts of PRs with no job issue)."""
        if getattr(self, "_board", None) is None:
            self._board = self.gh.paged(f"repos/{self.gh.repo}/issues/{self.board_issue()}/comments")
        return self._board

    def bridged_verdicts(self, number: int, comments: list[dict]) -> list[rules.Verdict]:
        """Muse's verdicts on PR #number that the mail bridge posted off the PR (on the job issue or the board,
        since its Issues-only token cannot comment on a PR): a bridge post whose first line names
        `review of #number`. The verdict itself is read as on the PR (parse_verdict: first line, logins)."""
        out = []
        names = re.compile(rf"\breview of #{number}\b", re.IGNORECASE)
        for c in comments:
            body = (c.get("body") or "").lstrip()
            if not body.startswith(BRIDGE_POST) or not names.search(body.split("\n", 1)[0]):
                continue
            v = rules.parse_verdict(body, c["created_at"], c["user"]["login"], self.team)
            if v:
                out.append(v)
        return out

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
        told: set[tuple[str, str]] = set()   # (live, trunk) bridge versions already alarmed
        board = self.board_issue()
        for c in self.gh.paged(f"repos/{self.gh.repo}/issues/{board}/comments"):
            at = parse_time(c.get("created_at") or "")
            if (c.get("user") or {}).get("login") != rules.BOT_LOGIN or at is None:
                continue
            for m in PIPE_KINDS.finditer(c.get("body") or ""):
                for kind in filter(None, m.group(1).split(",")):
                    last[kind] = max(at, last.get(kind, at))
            told |= set(BRIDGE_TOLD.findall(c.get("body") or ""))
        found: list[str] = []
        new: list[str] = []
        marks: list[str] = []
        version = self.bridge_version_health()
        if version:
            line, pair = version
            found.append(line)
            if pair not in told:
                new.append("version")
                marks.append(BRIDGE_TOLD_MARK.format(live=pair[0], trunk=pair[1]))
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
                              PIPE_MARK.format(kinds=",".join(new))] + marks)
            self.gh.comment(board, body, f"pipe alarm ({', '.join(new)}) on #{board}")
        self.pipe_all_clear(board, [k for k, on in (("version", version), ("outbound", stuck), ("inbound", quiet)) if on],
                            getattr(self, "pipe_last_post", None))
        return found

    def pipe_all_clear(self, board: int, kinds: list[str], last_post: dt.datetime | None) -> None:
        """#350 (Muse believed the bridge down for 36 h after it recovered: an alarm had no all-clear). The sweep state
        keeps each finding kind the pipe tripped on and when (gh.state["pipe_trip"]); the first pass that finds none
        posts one PIPE OK on the board, labeled wake:muse so the bridge mails it, and only then clears it (a failed
        write posts it again next pass). An inbound trip ends only on a bridge post newer than it (`last_post`, the
        newest one inbound_quiet read): its finding also ends when the day's asks age out, which proves nothing. A dry
        run writes no state. Reads nothing; two writes, only on recovery."""
        state = getattr(self.gh, "state", None)
        if not isinstance(state, dict) or getattr(self.gh, "dry_run", False):
            return
        box = state.get("pipe_trip") if isinstance(state.get("pipe_trip"), dict) else {}
        trips = {k: t for k, v in box.items() if isinstance(k, str) and (t := parse_time(str(v or ""))) is not None}
        if kinds:
            state["pipe_trip"] = {k: iso_z(t) for k, t in {**{k: self.now for k in kinds}, **trips}.items()}
            return
        if not trips:
            return
        if "inbound" in trips and (last_post is None or last_post <= trips["inbound"]):
            self.out("PIPE        no finding now, but no bridge post since the inbound alarm: no all-clear yet")
            return
        since = min(trips.values())
        body = "\n".join([f"{PIPE_OK_HEAD.format(board=board)} the Muse mail bridge works again ({central(self.now)}).",
                          "", f"The PIPE ALARM raised {central(since)} is over: this pass found none of its findings "
                          "(the switch checks the bridge every full sweep). Nothing to do, no reply needed.", "",
                          PIPE_OK_MARK])
        self.gh.comment(board, body, f"pipe all-clear on #{board}")
        self.gh.write("POST", f"repos/{self.gh.repo}/issues/{board}/labels", {"labels": [MUSE_LABEL]},
                      f"label #{board} {MUSE_LABEL} (the bridge emails Muse, then removes it)")
        self.out(f"PIPE OK     the alarm raised {central(since)} is over: all-clear posted on #{board}")
        state.pop("pipe_trip", None)

    def pipe_ripen(self, at: dt.datetime) -> None:
        """J-072: the next time a clock alone changes pipe_health's findings; a fast sweep runs it then."""
        if at > self.now:
            self.pipe_next = min(at, getattr(self, "pipe_next", None) or at)

    def wake_listing(self) -> list[int]:
        """The open issues and PRs labeled wake:muse (one read; wake_stuck reads the same path)."""
        label = urllib.parse.quote(MUSE_LABEL, safe="")
        got = self.gh.get(f"repos/{self.gh.repo}/issues?state=open&labels={label}&per_page={MAX_WAKE_ISSUES}") or []
        return sorted(int(it["number"]) for it in got)

    def bridge_version_health(self) -> tuple[str, tuple[str, str]] | None:
        """J-072: is the live bridge trunk's? Its clock names each sweep it dispatches "Switchboard sweep (bridge <v>)"
        (switchboard.yml run-name), <v> being the .gs's BRIDGE_VERSION, the hash bridge_version() gives of its own
        text. None when the newest such run in BRIDGE_SEEN carries trunk's version; else (the finding, the (live,
        trunk) pair), live being "none" when no run in the window carries one (a bridge pasted before
        BRIDGE_VERSION existed, or its clock is off). One read: the newest dispatched runs."""
        root = getattr(self, "root", None)
        try:
            want = bridge_version((root / BRIDGE_FILE).read_text(encoding="utf-8")) if root else ""
        except OSError:
            want = ""
        if not want:
            return None
        runs = (self.gh.get(f"repos/{self.gh.repo}/actions/workflows/{SWITCHBOARD_WORKFLOW}/runs?event=workflow_dispatch"
                            f"&branch={rules.TRUNK}&per_page=20&exclude_pull_requests=true") or {}).get("workflow_runs") or []
        live = ""
        for r in sorted(runs, key=lambda r: r.get("created_at") or "", reverse=True):
            at = parse_time(r.get("created_at") or "")
            m = BRIDGE_RUN.match(r.get("display_title") or "")
            if m and at and self.now - at <= BRIDGE_SEEN:
                live = m.group(1)
                if live == want:   # J-072: missing once BRIDGE_SEEN passes with no newer run naming it
                    self.bridge_until = at + BRIDGE_SEEN + dt.timedelta(seconds=1)
                break
        self.bridge_live = live or "none"
        if live == want:
            self.out(f"PIPE        the live bridge is trunk's ({want})")
            return None
        fix = (f"Cory pastes trunk's {BRIDGE_FILE} into the bridge's Apps Script project (replace all, save; the "
               "triggers stay).")
        if not live:
            return (f"BRIDGE VERSION: no sweep in the last {hours(BRIDGE_SEEN)} was dispatched by a bridge that names "
                    f"its version; trunk's is {want}. The live bridge predates it, or its clock is off. {fix}",
                    ("none", want))
        return (f"BRIDGE VERSION: the live bridge is {live}, trunk's is {want}: the deployed script is stale. {fix}",
                (live, want))

    def wake_stuck(self) -> list[tuple[int, dt.datetime]]:
        """Open issues and PRs whose wake:muse label has been on longer than PIPE_WAKE_STUCK: (number, labeled at).
        The label's age is read from the issue's events: the first `labeled` after the last `unlabeled`."""
        out: list[tuple[int, dt.datetime]] = []
        label = urllib.parse.quote(MUSE_LABEL, safe="")
        listed = self.gh.get(f"repos/{self.gh.repo}/issues?state=open&labels={label}&per_page={MAX_WAKE_ISSUES}") or []
        self.wake_numbers = sorted(int(it["number"]) for it in listed)   # J-072: a fast sweep compares its own read
        for it in listed:
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
            else:
                self.pipe_ripen(on + PIPE_WAKE_STUCK + dt.timedelta(seconds=1))
        return sorted(out, key=lambda x: x[1])

    def inbound_quiet(self, prev: dt.datetime | None) -> tuple[int, dt.datetime | None, bool] | None:
        """None while the bridge's inbound looks alive. Else (CLAUDE_TO_MUSE asks in the window, the newest bridge
        post read or None, is this a new incident). `prev` is the last inbound alarm: the read reaches back to it
        (at most PIPE_LOOKBACK), so a bridge post after it ends that incident. Newest comments first, so a
        bounded read keeps the window that decides."""
        start = self.now - PIPE_INBOUND_QUIET
        since = start if prev is None else max(min(start, prev), self.now - PIPE_LOOKBACK)
        stamp = since.strftime("%Y-%m-%dT%H:%M:%SZ")
        asks, posts, newest_ask = 0, [], None
        # J-072: the day's window is the inbox's own read (on the hour, oldest first: its pages answer 304); only an
        # open incident's longer window is read newest first, so a bounded read keeps the part that decides
        listed = (self.comments_since(since) if since >= start else
                  self.gh.paged(f"repos/{self.gh.repo}/issues/comments?since={stamp}&sort=created&direction=desc"))
        for c in listed:
            at = parse_time(c.get("created_at") or "")
            if at is None:
                continue
            body = (c.get("body") or "").lstrip()
            if body.startswith(BRIDGE_POST):
                posts.append(at)
            elif body.startswith("CLAUDE_TO_MUSE:") and PIPE_OK_MARK not in body:   # an all-clear asks nothing
                newest_ask = max(at, newest_ask or at)
                if at >= start:
                    asks += 1
        last_post = self.pipe_last_post = max(posts) if posts else None   # #350: pipe_all_clear reads it
        if last_post is not None and newest_ask is not None and newest_ask > last_post:
            self.pipe_ripen(last_post + PIPE_INBOUND_QUIET + dt.timedelta(seconds=1))   # quiet a day after the last post
        if not asks or (last_post is not None and last_post >= start):
            return None
        fresh = prev is None or prev < self.now - PIPE_LOOKBACK or (last_post is not None and last_post > prev)
        if not fresh and prev is not None:
            self.pipe_ripen(prev + PIPE_LOOKBACK + dt.timedelta(seconds=1))   # an open incident alarms again then
        return asks, last_post, fresh

    def inbox_reported(self) -> list[str]:
        """The keys listed before, oldest first: the sweep state's ring (11-13), then the keys in the switch's own
        inbox markers, which comments carried until 11-13. Only the inbox PR's newest 200 comments are read: the
        newest marker carries every earlier key forward. A marker by any other login never counts."""
        keys = [k for k in self.inbox_box().get("keys") or [] if isinstance(k, str)]
        count = int((self.gh.get(f"repos/{self.gh.repo}/issues/{INBOX_PR}") or {}).get("comments") or 0)
        last = max(1, -(-count // 100))
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
        ripe = getattr(self, "ripe_runs", None)   # J-072: a quiet fast sweep checks only the RED-BRANCH runs now ripe
        runs = ripe if ripe is not None else self.listing(
            f"repos/{self.gh.repo}/actions/workflows/{PROOF_WORKFLOW}/runs?created=%3E%3D{since}", "workflow_runs")
        items: list[InboxItem] = []
        for s in steered:
            items += self.pr_inbox_items(s, runs, reported, prefix)
        items += self.branch_inbox_items(runs, reported, prefix)
        if not prefix:
            items += getattr(self, "plan_items", ())
            items += self.queue_items   # the last count's QUEUE-LOW, set by run_sweep before the inbox (J-097)
            items += self.merged_items()   # J-072: merges no post has listed yet, a quiet or targeted sweep's too
            items += self.take_items()   # J-072: Muse jobs failed twice on the same failure, until the plan says claude
        if ripe is None:
            audience = ("muse", "both") if prefix else ("claude", "both")
            items += [it for it in self.trunk_items(runs, reported, prefix) if it.dest in audience]
        if not prefix and ripe is None:
            items += self.auto_pr_failed   # set by auto_open_prs earlier in the sweep
            items += self.ask_inbox_items(reported)
        return sorted(items, key=lambda it: INBOX_ORDER.index(it.kind))

    def plan_mode(self) -> str:
        """work/switchboard.yml `plan`: shadow (compute and print, write nothing), live, or off."""
        if not hasattr(self, "root"):
            return "off"  # isolated merge-rule tests have no checkout
        try:
            import yaml
            config = yaml.safe_load((self.root / "work/switchboard.yml").read_text(encoding="utf-8")) or {}
            return config.get("plan") if config.get("plan") in ("shadow", "live") else "off"
        except (OSError, ValueError):
            return "off"

    def read_plan(self) -> "collab_plan.Plan":
        path = self.root / "work/plan.md"
        try:
            model = collab_plan.parse(path.read_text(encoding="utf-8"))
        except OSError as err:
            raise RuntimeError(f"read collaboration plan {path}: {err}") from err
        if model.errors:
            raise RuntimeError("invalid collaboration plan: " + "; ".join(model.errors[:5]))
        return model

    def plan_reads(self, model: "collab_plan.Plan") -> dict:
        """collab_plan.observe, once per run: the one read of what the plan is checked against."""
        memo = self.__dict__.get("_plan_reads")
        if memo is None:
            memo = collab_plan.observe(self.gh, model, self.now, self.out, listing=self.listing,
                                       trunk=rules.TRUNK, errors=(GitHubError,))
            self.__dict__["_plan_reads"] = memo
        return memo

    def plan_table(self, steered=()) -> tuple:
        """(the plan, its reads, the mail, every job's row): the job machine's table, computed once per run from the
        plan and the evidence (machine.py). Reads only."""
        memo = self.__dict__.get("_plan_table")
        if memo is None:
            model = self.read_plan()
            reads = self.plan_reads(model)
            works = work_reader.read(self, model, reads, list(steered))
            notes = self.plan_notice_comments(model, reads)   # the switch's own notices: never lost to a read window
            mail = mail_reader.read(self, model, reads, also=notes)   # posts and notices from one comment set
            mail = dataclasses.replace(mail, receipts={**machine.recorded_receipts(notes), **mail.receipts})
            rows = machine.table(model, machine.collect(model, reads, works=works, mail=mail,
                                                        planned_at=reads.get("planned_at"), asks=self.ask_jobs()))
            memo = self.__dict__["_plan_table"] = (model, reads, mail, rows)
        return memo

    def plan_notice_comments(self, model, reads) -> list[dict]:
        """The comments that can hold the switch's plan marks: the live board's, and, only while the live board
        holds no mark of an owner's present version, the closed boards' too (a board closes at 100 comments; a
        notice sent on the last one is still sent, and is not sent again; the receipt filed beside it is read with
        it, as mail_reader reads these comments too)."""
        memo = self.__dict__.get("_plan_notes")
        if memo is None:
            memo = list(self.board_comments())
            vers = machine.versions(model)
            sent = {(n.owner, n.ver) for n in machine.sent_notices(memo)}
            marked = (machine.MAIL_OWNERS | machine.SELF_TOLD) & {ln.owner for ln in model.lines}
            if any((o, vers[o]) not in sent for o in marked):
                for old in self.old_boards():
                    memo += self.bounded_comments(old, MAX_OLD_BOARD_PAGES)
            self.__dict__["_plan_notes"] = memo
        return memo

    def plan_check(self, steered=()) -> None:
        """The job machine's pass. Shadow: every job's state, the late ones, and the views (labels, notices, the
        board section) are computed and printed, and nothing is written; an error here is a printed note, never a
        failed sweep. Live: the late jobs go to the integrator's inbox once each, the plan notice goes to a mail
        owner once per order version, the labels and the board's job section are made to match."""
        self.plan_items = ()
        mode = self.plan_mode()
        if mode == "off":
            return
        try:
            self.plan_pass(mode, steered)
        except Exception as err:   # noqa: BLE001
            if mode == "live":
                if isinstance(err, (GitHubError, RuntimeError, KeyError, ValueError)):
                    raise
                raise RuntimeError(f"plan: {type(err).__name__}: {err}") from err
            self.out(f"NOTE        plan (shadow): not computed this pass ({type(err).__name__}: {plain(str(err)[:200])})")

    def plan_pass(self, mode: str, steered=()) -> None:
        model, reads, mail, rows = self.plan_table(steered)
        late = machine.alarms(rows, self.now)
        lanes = ", ".join(f"{o} {n}/{limit}" for o, (n, limit) in machine.wip(model, rows).items() if n)
        self.out(f"PLAN        {len(model.lines)} jobs, {len(late)} late, lanes in flight: {lanes or 'none'}, mode={mode}")
        for state in machine.STATES:
            jobs = [r.job for r in rows if r.state == state]
            if jobs:
                self.out(f"PLAN-STATE  {state:<12} {len(jobs):>3}  {plain(', '.join(jobs[:40]))}")
        lines = machine.alarm_lines(rows, self.now)
        for line in lines[:MAX_PLAN_LINES]:
            self.out(f"PLAN-LATE   {plain(line[:400])}")
        aliases = {key: frozenset(a.get("owner_labels") or ())
                   for key, a in ((load_yaml(self.root / "work/agents.yml") or {}).get("agents") or {}).items()}
        changes = machine.labels(model, reads["issues"], aliases)
        for number, add, remove in changes[:MAX_PLAN_LINES]:
            self.out(f"PLAN-LABEL  #{number} add {', '.join(add) or '-'}; remove {', '.join(remove) or '-'}")
        titles = machine.bad_titles(model, reads["issues"])[:MAX_PLAN_LINES]
        for job, number, title in titles:
            self.out(f"PLAN-TITLE  {job} #{number} {plain(untrusted(title, 120))}: not `{job} ` first")
        notices, receipts = [], []
        notes = self.plan_notice_comments(model, reads)
        vers = machine.versions(model)
        owners = {ln.owner for ln in model.lines}
        seen = [o for o in sorted(machine.SELF_TOLD & owners)   # each job's clock needs its line's first-seen time
                if not any(n.ver == vers[o] for n in machine.marks(mail, o))]
        for owner in seen:
            self.out(f"PLAN-SEEN   {owner} plan {vers[owner]}" + ("" if mode == "live" else " (shadow: not written)"))
        for owner in sorted(machine.MAIL_OWNERS & owners):
            ver = vers[owner]
            sent = machine.notice_at(mail, owner, ver)
            answer = machine.receipt_post(mail, ver, sent) if sent else None
            if answer and (owner, ver) not in mail.receipts:
                receipts.append((owner, ver, answer))
            # #350: a told job waiting on an open ask is late on Claude's clock, not on the receipt's
            unread = sent and any(r.owner == owner and r.owes == owner and r.state == "told" and r.late(self.now)
                                  for r in rows)
            again = machine.NOTICE_AGAIN.format(owner=owner, ver=ver)
            if sent and not (unread and not any(again in (c.get("body") or "") and
                                                (c.get("user") or {}).get("login") == rules.BOT_LOGIN for c in notes)):
                continue
            text = machine.notice(model, rows, owner, ver, machine.notice_jobs(notes, owner), again=bool(sent))
            notices.append((owner, ver, text, bool(sent)))
            self.out(f"PLAN-NOTICE {owner} plan {ver}{' (again)' if sent else ''}" +
                     ("" if mode == "live" else " (shadow: not sent)"))
            self.out(text)
        board = machine.board(model, rows, self.now)
        if mode != "live":
            return
        # 11-13: dest is who owes the next step; only the integrator's own wake it (inbox_class), the rest ride
        self.plan_items = tuple(InboxItem("PLAN", r.job, r.key(), f"PLAN {plain(line[:600])}", r.owes or "claude")
                                for r, line in machine.alarm_groups(rows, self.now))[:MAX_PLAN_LINES]
        self.plan_items += self.muse_quiet(rows, reads, steered)   # #350: after the cut, so late lines never crowd it out
        # #350: a job issue the mail bridge cannot find by its title, once per title (its head); the integrator renames it
        self.plan_items += tuple(InboxItem(
            "PLAN", f"#{number}", hashlib.sha256(title.encode()).hexdigest()[:16],
            f"PLAN {job}: #{number} is titled {no_verdict(untrusted(title, 120))}, not `{job} ` first (the id, "
            f"then a space), so the mail bridge files {job}'s mail on the board, not on #{number}: rename #{number}",
            "integrator")
            for job, number, title in titles)
        for owner, ver, answer in receipts:   # the receipt, written into the notice it answers: it outlives the window
            c = machine.notice_comment(notes, owner, ver)
            if c and c.get("id"):
                mark = machine.RECEIPT_MARK.format(owner=owner, ver=ver, at=iso_z(answer.at), comment=answer.id)
                self.gh.write("PATCH", f"repos/{self.gh.repo}/issues/comments/{c['id']}",
                              {"body": f"{c.get('body') or ''}\n{mark}"}, f"plan: record {owner}'s receipt of plan {ver}")
        for owner, ver, text, resend in notices:
            self.send_plan_notice(model, mail, owner, ver, text, resend, reads.get("planned_at"))
        for owner in seen:
            mark = machine.NOTICE_MARK.format(owner=owner, ver=vers[owner], jobs=machine.mark_jobs(
                model, mail, owner, self.now, reads.get("planned_at")))
            self.gh.comment(self.board_issue(), f"{machine.SELF_HEAD.format(ver=vers[owner], owner=owner)}\n\n{mark}",
                            f"plan: record plan {vers[owner]} for {owner} on #{self.board_issue()}")
        for number, add, remove in changes[:MAX_PLAN_LABELS]:
            if add:
                self.gh.write("POST", f"repos/{self.gh.repo}/issues/{number}/labels", {"labels": list(add)},
                              f"plan: label #{number} {', '.join(add)}")
            for name in remove:
                from urllib.parse import quote
                self.gh.write("DELETE", f"repos/{self.gh.repo}/issues/{number}/labels/{quote(name, safe='')}", None,
                              f"plan: take {name} off #{number}")
        self.write_plan_board(board)

    def send_plan_notice(self, model, mail, owner: str, ver: str, text: str, resend: bool, planned_at=None) -> None:
        """One plan notice to a mail owner, on the board, the way muse() sends: the comment, then the wake:muse
        label, then the marker (so a failed label leaves no marker and the next pass sends it again). The marker
        carries each job's line hash and when it was first told and acknowledged (machine.mark_jobs)."""
        where = self.board_issue()
        jobs = machine.mark_jobs(model, mail, owner, self.now, planned_at)
        mark = (machine.NOTICE_AGAIN.format(owner=owner, ver=ver) if resend else
                machine.NOTICE_MARK.format(owner=owner, ver=ver, jobs=jobs))
        resp = self.gh.comment(where, text, f"plan: notice of plan {ver} to {owner} on #{where}")
        self.gh.write("POST", f"repos/{self.gh.repo}/issues/{where}/labels", {"labels": [MUSE_LABEL]},
                      f"label #{where} {MUSE_LABEL} (the bridge emails Muse, then removes it)")
        cid = (resp if isinstance(resp, dict) else {}).get("id")
        if cid is not None:
            self.gh.write("PATCH", f"repos/{self.gh.repo}/issues/comments/{cid}", {"body": f"{text}\n\n{mark}"},
                          f"plan: mark the notice of plan {ver} delivered")

    def write_plan_board(self, text: str) -> None:
        """The board body's job section, between its own markers; the rest of the body is left as it is."""
        number = self.board_issue()
        with self.gh.fresh():
            body = (self.gh.get(f"repos/{self.gh.repo}/issues/{number}") or {}).get("body") or ""
        new = machine.splice(body, text)
        if new is None:
            self.out(f"ERROR       plan: #{number}'s body has broken plan markers; not written (fix them by hand)")
        elif new != body:
            self.gh.write("PATCH", f"repos/{self.gh.repo}/issues/{number}", {"body": new},
                          f"plan: rewrite #{number}'s job section")

    def queue_text(self) -> str:
        """Trunk's work/tasks/muse.md, read once per run from this checkout; "" (an empty queue) when it cannot be."""
        if getattr(self, "_queue_text", None) is None:
            try:
                self._queue_text = (self.root / muse_queue.QUEUE_FILE).read_text(encoding="utf-8")
            except OSError as err:
                self.out(f"NOTE        {muse_queue.QUEUE_FILE} could not be read ({type(err).__name__}): counted as empty")
                self._queue_text = ""
        return self._queue_text

    def queue_source(self) -> str:
        """A digest of the queue's own text (work/plan.md when the plan is live, else muse.md): a change counts again."""
        try:
            text = (self.root / "work/plan.md").read_text(encoding="utf-8") if self.plan_mode() == "live" \
                else self.queue_text()
        except OSError:
            text = ""
        return hashlib.sha256(text.encode("utf-8")).hexdigest()[:16]

    def queue_started(self, delta: "SweepDelta", new_pr: bool, last: dict | None = None) -> bool:
        """J-097: could this fast sweep's changes have started a queued job? A bridged post (or any mail), a new PR, or
        a proof run on a branch that names a queued job (`j0NN`). Reads no GitHub: the delta is the sweep's own.
        J-072 (11-13): with the last count's unstarted jobs (`last`, run_sweep's queue timer) and the queue text
        unchanged, only those jobs can lower the count: a proof run on a branch naming one, a bridged post naming one
        (or naming no job), or a new PR. Counted on every Muse push and post, it was 225 of a fast sweep's 501 calls.
        Not seen here: a push to a wip- branch (no proof run) with no mail; the full pass counts it, up to an hour late."""
        waiting = (last or {}).get("waiting")
        if isinstance(waiting, list) and (last or {}).get("source") == self.queue_source():
            names = {str(j).lower().replace("-", "") for j in waiting}
            mail = delta.mail or set()
            return (new_pr or any(j in b.lower() for b in delta.active for j in names)
                    or "*" in mail or bool(mail & set(waiting)))
        if delta.pipe or new_pr:
            return True
        if self.plan_mode() == "live":   # the queue is Muse's lines of work/plan.md, not muse.md's old block
            jobs = {ln.job.lower().replace("-", "") for ln in self.read_plan().lines if ln.owner == "muse"}
        else:
            jobs = {f"j{q.num}" for q in muse_queue.queue_lines(self.queue_text())}
        return any(j in b.lower() for b in delta.active for j in jobs)

    def queue_check(self, text: str | None = None) -> tuple[InboxItem, ...]:
        """QUEUE-LOW (J-097, Cory 11-11: "you should be making sure muse DOES NOT RUN LOW ON QUEUE at any point"):
        fewer than muse_queue.LOW_UNDER really-unstarted jobs in work/tasks/muse.md's Unstarted block (muse_queue's
        rules, read with this run's client). The item's head is the count and the unstarted jobs, so it is listed once
        per change, and a queue refilled and run low again is listed again. Sets and returns queue_items."""
        # A live plan is the only route for new work: the count comes from the job machine's table. An explicit
        # text argument remains for the pre-cutover queue parser's regression tests.
        if text is None and self.plan_mode() == "live":
            model, _, _, rows = self.plan_table()
            waiting = [r.job for r in rows if r.owner == "muse" and r.state in ("planned", "told", "acknowledged")]
            self.queue_items, self.queue_waiting = (), sorted(waiting)
            if len(waiting) < muse_queue.LOW_UNDER:
                line = (f"QUEUE-LOW: {len(waiting)} unstarted Muse jobs in work/plan.md (under {muse_queue.LOW_UNDER}). "
                        f"Unstarted: {', '.join(waiting) or 'none'}")
                self.queue_items = (InboxItem("QUEUE-LOW", "work/plan.md",
                                              model.fingerprint() + ":" + ",".join(waiting), line),)
            return self.queue_items
        text = self.queue_text() if text is None else text
        self.queue_items = ()
        report = muse_queue.check(self.gh, text, self.now)
        ok = report.unstarted
        n = len(ok)
        self.queue_waiting = sorted(v.line.job for v in ok)
        for note in report.notes:
            self.out(f"NOTE        queue check: {plain(note[:200])}")
        self.out(f"QUEUE       {n} of {len(report.verdicts)} queued job(s) unstarted (QUEUE-LOW under {muse_queue.LOW_UNDER})")
        if n >= muse_queue.LOW_UNDER:
            return self.queue_items
        jobs = sorted(v.line.job for v in ok)
        taken = [f"{v.line.job} {v.verdict}" for v in report.verdicts if v.verdict != "OK"]
        line = (f"QUEUE-LOW: {n} unstarted in `{muse_queue.QUEUE_FILE}` (under {muse_queue.LOW_UNDER}): refill to "
                f"{muse_queue.REFILL_TO}. Unstarted: {', '.join(jobs) or 'none'}"
                + (f"; taken: {', '.join(taken)}" if taken else ""))
        self.queue_items = (InboxItem("QUEUE-LOW", muse_queue.QUEUE_FILE, f"{n}:{','.join(jobs)}", line),)
        return self.queue_items

    def ask_inbox_items(self, reported: frozenset[str]) -> list[InboxItem]:
        """TO-CLAUDE (J-052): an ask to the integrator in a comment on an open thread, once per comment id.
        The first line holds `To Claude` or `proposal`, or is the bridge's `**Muse (via email bridge):
        <subject>**`: every Muse mail is to Claude, so every one wakes it. Every agent posts as IssisX, so the author is read
        from the body: Claude's posts end with its footer, the switchboard's own are its login, Codex's
        start with `<!-- codex`. #350: an ask that names a job is listed again, once per ASK_AGAIN, while no Claude
        post has named the job since (open_asks)."""
        out: list[InboxItem] = []
        for c, number, kind, first in self.asks_seen():
            cid = str(c["id"])
            if inbox_key(kind, f"#{number}", cid) in reported:
                continue
            out.append(InboxItem(kind, f"#{number}", cid, f"{kind} #{number} {ask_said(kind, first)} "
                                                          f"{c.get('html_url') or ''}"))
        for cid, rec in self.open_asks().items():
            last = parse_time(rec["listed"])
            if last is not None and self.now - last < ASK_AGAIN:
                continue
            head = f"{cid}@{iso_z(self.now)}"   # a new key per listing; the first listing keeps the bare id
            if inbox_key("TO-CLAUDE", rec["target"], head) in reported:
                continue
            age = self.now - (parse_time(rec["at"]) or self.now)
            out.append(InboxItem("TO-CLAUDE", rec["target"], head,
                                 f"TO-CLAUDE {rec['target']} still open after {hours(age)}: no Claude comment or mail "
                                 f"has named {rec['job']} since. {rec['said']} {rec['url']}"))
            rec["listed"] = iso_z(self.now)
        return out

    def asks_seen(self) -> list[tuple[dict, int, str, str]]:
        """The asks to the integrator in the inbox's comment window, as (comment, thread, kind, first line): kind
        TO-CLAUDE, or a routine Muse post's digest kind (muse_routine). Read once per run (#350: open_asks too)."""
        memo = self.__dict__.get("_asks_seen")
        if memo is not None:
            return memo
        since = (self.now - INBOX_RUNS_WINDOW).strftime("%Y-%m-%dT%H:%M:%SZ")
        threads = {int(i["number"]): i for i in self.listing(f"repos/{self.gh.repo}/issues?state=open")}
        self.__dict__["_ask_threads"] = threads
        out: list[tuple[dict, int, str, str]] = []
        for c in self.comments_since(self.now - INBOX_RUNS_WINDOW):
            if (c.get("updated_at") or c.get("created_at") or "") < since:   # the hour's floor read a little more
                continue
            body = c.get("body") or ""
            if body.lstrip().startswith("<!-- codex"):
                continue
            # The switch's own comments are never asks, except the ledger's "To Claude:" notices (J-072: notice
            # 6746048774 on #340, 7 Claude items, never reached this inbox).
            if (c.get("user") or {}).get("login") == rules.BOT_LOGIN and not (
                    LEDGER_NOTICE in body and body.lstrip().lower().startswith("to claude")):
                continue
            if "Generated by [Claude Code]" in body:
                continue
            if body.lstrip().startswith("CLAUDE_TO_MUSE:"):   # Claude's own mail to Muse, as the bridge records it
                continue
            if c.get("id") is None:
                continue
            try:
                number = int((c.get("issue_url") or "").rsplit("/", 1)[1])
            except (ValueError, IndexError):
                continue
            if number not in threads:
                continue
            first = (body.split("\n", 1)[0] or "").strip()
            low = first.lower()
            # Every bridge post is Muse writing to Claude (its subject starts MUSE_TO_CLAUDE): a report or an
            # ask, it wakes the integrator. Gating it on "To Claude" in the subject left Muse's ruling asks
            # ("cause 2 needs your ruling", 11-10) unread all night.
            if "to claude" not in low and "proposal" not in low and not first.startswith(BRIDGE_POST):
                continue
            # J-103: a bridged "J-NNN started" or a bare ack is digested, never posted alone (muse_routine); a question,
            # a sha, a red, a finding or "taken" in it still wakes as TO-CLAUDE. queue_check counts STARTED either way.
            out.append((c, number, muse_routine(first, body) or "TO-CLAUDE", first))
        self.__dict__["_asks_seen"] = out
        return out

    def open_asks(self) -> dict[str, dict]:
        """#350 (J-124 waited 37 h on a ruling, J-078 42 h on a landing path, each listed once and then forgotten):
        comment id -> the TO-CLAUDE asks from a team login that name a job (the subject's, else the job whose issue
        or PR the thread is) and that no later Claude post has answered: a comment from a team login, never the
        switch's own, that carries Claude's footer, opens `Claude (integrator)` or is a hand mail to Muse
        (`CLAUDE_TO_MUSE:`), and that names the job or sits on the job's own thread. One post naming several jobs
        answers each of them. Kept in the sweep state (the inbox's
        box), past the comment window, at most MAX_OPEN_ASKS, the newest. An ask that names no job is listed once,
        as before. No read of its own: the inbox's comment window and open-thread listing, once per run."""
        memo = self.__dict__.get("_open_asks")
        if memo is not None:
            return memo
        box = self.inbox_box()
        stored = box.get("asks") if isinstance(box.get("asks"), dict) else {}
        kept = {str(k): v for k, v in stored.items()
                if isinstance(v, dict) and all(isinstance(v.get(f), str) for f in ASK_FIELDS)
                and parse_time(v["at"]) is not None}
        seen = self.asks_seen()
        threads = self.__dict__.get("_ask_threads") or {}
        logins = {login.lower() for login in self.team.verdict_logins}
        for c, number, kind, first in seen:
            cid = str(c["id"])
            at = parse_time(c.get("created_at") or "")
            job = (machine.JOB_ID.findall(first) or [thread_job(threads.get(number))])[0]
            # only a team login's ask is kept: anyone can comment on the public repo, and a kept ask moves a job's
            # step to Claude; a stranger's is still listed once, as before
            if kind != "TO-CLAUDE" or cid in kept or not job or at is None or \
                    ((c.get("user") or {}).get("login") or "").lower() not in logins:
                continue
            kept[cid] = {"job": job, "target": f"#{number}", "at": iso_z(at), "listed": iso_z(self.now),
                         "said": ask_said(kind, first), "url": str(c.get("html_url") or "")}
        team = logins - {rules.BOT_LOGIN.lower()}
        for c in self.comments_since(self.now - INBOX_RUNS_WINDOW):
            body = (c.get("body") or "").lstrip()
            at = parse_time(c.get("created_at") or "")
            if at is None or ((c.get("user") or {}).get("login") or "").lower() not in team or not (
                    body.startswith(("CLAUDE_TO_MUSE:", "Claude (integrator)")) or "Generated by [Claude Code]" in body):
                continue
            try:
                number = int((c.get("issue_url") or "").rsplit("/", 1)[1])
            except (ValueError, IndexError):
                number = 0
            named = set(machine.JOB_ID.findall(body)) | {thread_job(threads.get(number))}
            for cid, rec in list(kept.items()):
                if at > (parse_time(rec["at"]) or at) and same_job(rec["job"], named):
                    self.out(f"ASK         {rec['target']} {rec['job']}: answered by {c.get('html_url') or c.get('id')}")
                    del kept[cid]
        kept = dict(sorted(kept.items(), key=lambda kv: kv[1]["at"])[-MAX_OPEN_ASKS:])
        if not getattr(self.gh, "dry_run", False):
            box["asks"] = kept
        self.__dict__["_open_asks"] = kept
        return kept

    def ask_jobs(self) -> dict[str, tuple[dt.datetime, str]]:
        """job -> (when, link) of its oldest open ask (open_asks), for the job machine. A read that fails leaves the
        machine without them this pass; the plan part goes on."""
        try:
            asks = self.open_asks()
        except GitHubError as err:
            self.out(f"NOTE        open asks not read this pass ({plain(str(err)[:160])})")
            return {}
        out: dict[str, tuple[dt.datetime, str]] = {}
        for rec in asks.values():
            at = parse_time(rec["at"])
            if at is not None and (rec["job"] not in out or at < out[rec["job"]][0]):
                out[rec["job"]] = (at, rec["url"] or rec["target"])
        return out

    def muse_heard(self, reads: dict, steered=()) -> dt.datetime | None:
        """#350: the newest sign of Muse on GitHub that a full pass reads anyway: a head commit on its branches
        with no PR (the plan's compares) or on its open PRs (the steered PRs' facts), and a bridge post from a team
        login (the inbox's comment window). None when none of them shows one."""
        def muses(ref: str) -> bool:
            return any(machine.owns(o, ref) for o in machine.MAIL_OWNERS)
        head_at = reads.get("head_at") or {}
        times = [head_at[collab_plan.branch_sha(b)] for b in reads.get("branches") or []
                 if muses(collab_plan.branch_name(b)) and collab_plan.branch_sha(b) in head_at]
        times += [t for s in steered if muses(s.facts.head_ref)
                  and (t := parse_time(s.facts.head_committed_at or "")) is not None]
        team = {login.lower() for login in self.team.verdict_logins}
        times += [t for c in self.comments_since(self.now - INBOX_RUNS_WINDOW)
                  if (c.get("body") or "").lstrip().startswith(BRIDGE_POST)
                  and ((c.get("user") or {}).get("login") or "").lower() in team
                  and (t := parse_time(c.get("created_at") or "")) is not None]
        return max(times, default=None)

    def muse_quiet(self, rows, reads: dict, steered=()) -> tuple[InboxItem, ...]:
        """#350: one QUIET item to the integrator (class "hold": it wakes, never digest-only) while a Muse lane is in
        flight and nothing of Muse's (muse_heard) is newer than MUSE_QUIET; at most once per MUSE_QUIET_AGAIN while
        the silence lasts (its head is the incident's stamp, the inbox box's quiet_at), and again once Muse is heard
        and then goes quiet again. Needs no CLAUDE_TO_MUSE ask (inbound_quiet does) and makes no read of its own; a
        read that fails drops only this item."""
        box = self.inbox_box()
        write = not getattr(self.gh, "dry_run", False)
        lanes = [r for r in rows if r.owner in machine.MAIL_OWNERS and machine.flying(r)]
        try:
            heard = self.muse_heard(reads, steered) if lanes else None
        except GitHubError as err:
            self.out(f"NOTE        Muse quiet check not made this pass ({plain(str(err)[:160])})")
            return ()
        if not lanes or (heard is not None and self.now - heard < MUSE_QUIET):
            if write:
                box.pop("quiet_at", None)
            return ()
        # the incident's stamp is the item's head: every silent pass returns it and the inbox's ring lists it once,
        # so an inbox part that fails this pass loses nothing; a day on, a new stamp lists it again
        stamp = parse_time(str(box.get("quiet_at") or ""))
        if stamp is None or self.now - stamp >= MUSE_QUIET_AGAIN:
            stamp = self.now
            if write:
                box["quiet_at"] = iso_z(stamp)
        late = [r.job for r in lanes if r.owes == r.owner and r.late(self.now)]   # an ask's clock is Claude's
        jobs = ", ".join(r.job for r in lanes[:8]) + (", ..." if len(lanes) > 8 else "")
        span = hours(self.now - heard) if heard is not None else \
            f"at least {hours(INBOX_RUNS_WINDOW)} (none in the window read)"
        self.out(f"QUIET       Muse: no push and no bridge post for {span}, {len(lanes)} lane(s) in flight")
        return (InboxItem("QUIET", "muse", iso_z(stamp),
                          f"QUIET Muse: no fb-bot push and no bridge post for {span} while {len(lanes)} Muse lane(s) "
                          f"are in flight ({jobs}), {len(late)} of them late. Muse may be down or holding: mail "
                          f"Muse, or check the bridge."),)

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

        def item(kind: str, text: str, dest: str = "claude", finding: str = "", run_id=None) -> InboxItem:
            # J-104: a red item names its failing tests, its logs read only when this audience has not had it yet
            fresh = run_id is not None and inbox_key(prefix + kind, target, f.head_sha) not in reported
            wanted = dest in ("muse", "both") if mine else dest in ("claude", "both")
            read = fresh and wanted
            return InboxItem(kind, target, f.head_sha, f"{kind} {where}: {text}", dest, finding,
                             self.red_tests(run_id) if read and not self.defer_logs else (),
                             run_id=run_id if read else None)

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
            if times:
                self.ripen(f.number, max(times) + INBOX_READY_AFTER)
            if not times or now - max(times) >= INBOX_READY_AFTER:
                why = s.merge_note or "; ".join(d.reasons)
                out.append(item("NOT-MERGED", f"green, LAND by {lands[-1].who}, still open a sweep later: {plain(why)}",
                                "both" if owner == "muse" else "claude"))
        if d.action == "FIX" and owner:
            out.append(item("FIX", plain("; ".join(d.reasons)), owner, self.fix_finding(f, ctx)))
            # 11-12 (#389, #396): the FIX item goes once per head and the reconciler re-sends it once, so a FIX whose
            # owner never moved sat a day with nobody told. No new head for INBOX_FIX_STUCK: the integrator hears it,
            # naming the owner, and so does Muse on its own PR.
            on_head = [parse_time(r["created_at"]) for r in runs
                       if r.get("head_branch") == f.head_ref and r.get("head_sha") == f.head_sha]
            since = min(on_head) if on_head else parse_time(f.head_committed_at)
            if since:
                self.ripen(f.number, since + INBOX_FIX_STUCK)
            if since and now - since >= INBOX_FIX_STUCK and not self.plan_held(ctx.get("job") or ""):
                name = self.team.agent_name.get(owner, owner)
                out.append(item("FIX-STUCK", f"{plain('; '.join(d.reasons))}, and no new head from its owner "
                                             f"{name} for {hours(now - since)}", "both" if owner == "muse" else "claude"))
        api_note = [v for v in f.verdicts if v.kind == "review" and v.verdict == "NOTE" and v.author_login == rules.BOT_LOGIN
                    and rules.names_head(v, f.head_sha)]
        if api_note and f.proof == "green" and rules.needs_review(f, author, self.team):
            # Cory, 11-10 ("take the backup reviews yourself"): the API was unsure; the integrator rules within the
            # hour rather than the head waiting on ChatGPT's and Grok's scheduled runs
            out.append(item("NOTE-REVIEW", f"the automatic Claude review answered NOTE; read it and post "
                                           f"**Claude (review of #{f.number} @ {f.head_sha}): LAND** or FIX"))
        if rules.CONFLICT in d.reasons and owner and not any(it.kind == "NOT-MERGED" for it in out):
            # 11-10 #343: a conflict was told to its owner only by the reconciler's hourly pass; this sweep tells it now
            out.append(item("CONFLICT", f"{rules.CONFLICT}: merge {rules.TRUNK} into {plain(f.head_ref)} and push",
                            owner))
        codex_tag = MARK.format(kind="ask-codex", pr=f.number, head=f.head_sha)
        if any(codex_tag in (c.get("body") or "") and (c.get("user") or {}).get("login") == rules.BOT_LOGIN
               for c in ctx.get("comments") or []) and rules.needs_review(f, author, self.team):
            out.append(item("CODEX-ASK", f"post codex review with an at-sign in front on #{f.number} as IssisX: the "
                                         "switchboard's own mention does not wake Codex (J-072)"))
        rerun = None
        if d.action == "FIX-CI":
            runs_here = sorted((r for r in runs if r.get("head_branch") == f.head_ref), key=lambda r: r["created_at"], reverse=True)
            this = next((r for r in runs_here if r.get("id") == ctx.get("run_id") and r.get("head_sha") == f.head_sha), None)
            # J-103: red only in setup (the runners' mirrors), not yet re-run on this head: re-run once, no red item yet
            rerun = self.outage_rerun(target, where, this) if this else None
            if rerun:
                out.append(rerun)
        if f.proof == "green" and owner == "muse":
            self.note_red_head(f, ctx, None)   # J-072: a green ends the run of reds a TAKE counts
        if d.action == "FIX-CI" and not rerun:
            jobs = red_jobs(f.proof)
            both = "both" if owner == "muse" else "claude"
            rid = (this or {}).get("id") or ctx.get("run_id")   # the head's red proof run, whose logs name its tests
            take = self.note_red_head(f, ctx, rid) if owner == "muse" else None
            if take and mine:   # once per job: the key is the head the take was found at
                out.append(InboxItem("TAKE", target, take["head"], f"TAKE {where}: {plain(take_mail(take))}", "muse",
                                     tests=tuple(take["tests"])))
            ids = [r["id"] for r in runs_here]
            attempt = int((this or {}).get("run_attempt") or 1)
            if inbox_key(prefix + "RED-TWICE", target, f.head_sha) in reported:   # listed at this head: no need to read it again
                out.append(InboxItem("RED-TWICE", target, f.head_sha, "", both))
            elif attempt >= 2:   # J-103: red again on a re-run of the same head (the switch's one re-run, or a person's)
                out.append(item("RED-TWICE", f"{plain(', '.join(jobs))} failed again on attempt {attempt} of the same "
                                             f"proof run ({proof_link})", both, run_id=rid))
            elif ctx.get("run_id") in ids and ids.index(ctx["run_id"]) + 1 < len(runs_here):
                before = self.run_state_or_none(runs_here[ids.index(ctx["run_id"]) + 1])
                failed = [j for j in jobs if j in red_jobs(before[0])] if before else []   # unread: next sweep
                if failed:
                    out.append(item("RED-TWICE", f"{plain(', '.join(failed))} failed on two proofs in a row ({proof_link})",
                                    both, run_id=rid))
            if owner == "muse":
                out.append(item("RED", f"proof red on {plain(', '.join(jobs))} ({proof_link})", "muse", run_id=rid))
            on_head = [parse_time(r["created_at"]) for r in runs_here if r.get("head_sha") == f.head_sha]
            head_since = min(on_head) if on_head else parse_time(f.head_committed_at)
            if head_since:
                self.ripen(f.number, head_since + INBOX_RED_STUCK)
            if head_since and now - head_since >= INBOX_RED_STUCK and owner and not self.plan_held(ctx.get("job") or ""):
                out.append(item("RED-STUCK", f"proof red on {plain(', '.join(jobs))}, no new head for "
                                             f"{hours(now - head_since)} ({proof_link})", owner, run_id=rid))
        if not rerun and not [it for it in out if kept(it)]:   # QUIET, as tools/board/sweep.py: FIX, FIX-CI or DISPATCH with the PR untouched for QUIET_HOURS
            action = ("FIX-CI" if f.proof.startswith("red") else
                      "FIX" if d.action == "FIX" or rules.open_fixes(f.verdicts, f.head_sha, f.head_committed_at, "review") else
                      "DISPATCH" if f.proof == "none" else "")
            idle = parse_time(ctx.get("updated_at") or "")
            if action and idle:
                self.ripen(f.number, idle + dt.timedelta(hours=QUIET_HOURS))
            if action and idle and (now - idle).total_seconds() >= QUIET_HOURS * 3600 and owner \
                    and not self.plan_held(ctx.get("job") or ""):   # J-072: a held job's PR is quiet by plan
                out.append(item("QUIET", f"no activity on the PR for {hours(now - idle)} while it is {action}", owner))
        return [it for it in out if kept(it)]

    def outage_rerun(self, target: str, where: str, run: dict) -> InboxItem | None:
        """J-103 (#422): a completed red proof run on an integrate branch whose every failed job failed in a setup step
        (setup_only) is re-run as a whole run, once per head, and its RERUN entry rides in the inbox digest; None when
        it is not one, or this head was re-run already (by an earlier sweep, timers["rerun"], or by anyone: a run on
        the head past attempt 1). A build or test step red is never re-run. A refused re-run leaves the red as it was.
        Called for both audiences in one sweep: the second call gets the first one's answer and writes nothing."""
        head, rid = run.get("head_sha") or "", run.get("id")
        memo = self.__dict__.setdefault("_reran", {})
        if (target, head) in memo:
            return memo[(target, head)]
        memo[(target, head)] = None
        if (run.get("status") != "completed" or run.get("conclusion") != "failure" or not isinstance(rid, int)
                or int(run.get("run_attempt") or 1) >= 2 or not head or head in self.rerun_heads
                or not (run.get("head_branch") or "").startswith(INBOX_BRANCH_PREFIXES)):
            return None
        try:
            steps = setup_only(self.run_jobs(rid))
            if not steps or any(int(r.get("run_attempt") or 1) >= 2 for r in self.head_runs(head)):
                return None
        except RateLimited:
            raise
        except GitHubError as err:   # 11-12: one jobs read GitHub answered 502 never ends the inbox part
            self.out(f"NOTE        proof run {rid}'s jobs were not read ({err_text(err)}): no re-run this sweep")
            return None
        try:
            self.gh.write("POST", f"repos/{self.gh.repo}/actions/runs/{rid}/rerun", None,
                          f"re-run proof run {rid} on {head[:8]}: red only in setup ({plain(steps)[:120]})")
        except GitHubError as err:
            self.out(f"NOTE        re-run of proof run {rid} refused ({plain(err.detail[:160])}): its red stands")
            return None
        self.__dict__.setdefault("reran_now", []).append(head)
        url = run.get("html_url") or ""
        # job and step names come from the branch's own workflow file: one bounded code span that reads as no verdict
        memo[(target, head)] = InboxItem("RERUN", target, head, f"RERUN {where}: proof red only in setup "
                                                                f"({untrusted(no_verdict(steps), 300)}), re-run once "
                                                                f"([proof]({url}))")
        return memo[(target, head)]

    def mark_reruns(self, steered: list[Steered]) -> None:
        """J-103: a PR whose proof this sweep re-ran is running again: the ledger and the reconciler, which read
        `steered` after the inbox, see it so and send no red notice of their own for the outage."""
        memo = self.__dict__.get("_reran") or {}
        for s in steered:
            if memo.get((f"#{s.facts.number}", s.facts.head_sha)):
                s.facts = dataclasses.replace(s.facts, proof="running")
                s.decision = rules.merge_decision(s.facts, self.team.vendor_of_branch(s.facts.head_ref), self.team,
                                                  bool(self.merge_token), bool(self.review_key))

    def ripen(self, number: int, at: dt.datetime) -> None:
        """J-072: the next time a clock alone changes this PR's inbox items (a fast sweep reads a cold PR again then).
        Only future times count: one already past fired this sweep."""
        if at > self.now:
            got = self.__dict__.setdefault("_ripen", {})
            got[number] = min(at, got.get(number, at))

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
        if rules.is_codex_app(v):   # 11-12: the state comment holds no finding; they are inline threads, so say where
            return (f"Codex's findings are unresolved review threads on the diff; fix them, or answer and resolve each: "
                    f"https://github.com/{self.gh.repo}/pull/{f.number}/files")
        return ""

    def branch_inbox_items(self, runs: list[dict], reported: frozenset[str], prefix: str = "") -> list[InboxItem]:
        """(e) RED-BRANCH: an fb-bot/ or claude/integrate-* branch with no open PR whose head's proof went red an
        hour ago or more. Only heads proved in the run window are looked at, and only up to MAX_INBOX_BRANCHES.
        Each item goes to the branch's owner (J-052)."""
        from urllib.parse import quote
        mine = prefix == "muse-"
        open_refs = {p["head"]["ref"] for p in self.listing(f"repos/{self.gh.repo}/pulls?state=open")}
        newest: dict[str, dict] = {}
        for r in runs:
            name = r.get("head_branch") or ""
            if name.startswith(INBOX_BRANCH_PREFIXES) and name != INBOX_BRANCH and name not in open_refs:
                if name not in newest or r["created_at"] > newest[name]["created_at"]:
                    newest[name] = r
        out: list[InboxItem] = []
        read = 0
        pending = self.__dict__.setdefault("red_pending", {})   # J-072: runs that ripen later (switch state "red")
        checked = self.__dict__.setdefault("_red_checked", {})   # (branch, head) -> its branch head and proof, this run
        for name in sorted(newest):
            run = newest[name]
            head, done = run.get("head_sha") or "", parse_time(run.get("updated_at") or "")
            if (run.get("status") == "completed" and run.get("conclusion") == "success") or not done \
                    or inbox_key(prefix + "RED-BRANCH", name, head) in reported:
                continue
            if (self.now - done) < INBOX_BRANCH_RED:
                if run.get("status") == "completed":   # red, not yet an hour: a fast sweep checks it when it ripens
                    pending[name] = {k: run.get(k) for k in RED_RUN_KEYS}
                continue
            if (name, head) not in checked and read >= MAX_INBOX_BRANCHES:
                self.out(f"INBOX       {MAX_INBOX_BRANCHES} branches read; the rest wait for the next sweep")
                pending[name] = {k: run.get(k) for k in RED_RUN_KEYS}   # ripe already: the next sweep checks it
                continue
            if (name, head) not in checked:
                read += 1
                try:
                    now_head = self.gh.get(f"repos/{self.gh.repo}/branches/{quote(name, safe='/')}")["commit"]["sha"]
                    state = self.run_state(run) if now_head == head else None
                except RateLimited:
                    raise
                except GitHubError as err:
                    if "HTTP 404" in err.detail:
                        now_head, state = "", None   # deleted since its proof ran
                    else:   # 11-12: a 502 on one branch's read never ends the inbox part; the next sweep reads it
                        self.out(f"NOTE        {untrusted(name)}: its head or proof jobs were not read ({err_text(err)}); "
                                 "the next sweep reads it")
                        pending[name] = {k: run.get(k) for k in RED_RUN_KEYS}
                        continue
                checked[(name, head)] = (now_head, state)
            now_head, state = checked[(name, head)]
            if now_head != head or state is None:   # gone, or a newer head: its own proof decides
                continue
            proof, url, rid, done_at = state
            done = parse_time(done_at) or done
            if proof.startswith("red") and (self.now - done) >= INBOX_BRANCH_RED:
                # J-103: red only in setup and not yet re-run on this head: re-run once instead (its digest entry is the
                # integrator's alone); red again on the re-run is RED-BRANCH an hour later, as before
                rerun = self.outage_rerun(name, f"{untrusted(name)} head {head[:8]}", run)
                if rerun:
                    out.append(rerun)
                    continue
                dest = owner_of(name)
                wanted = (mine and dest in ("muse", "both")) or (not mine and dest in ("claude", "both"))
                out.append(InboxItem("RED-BRANCH", name, head,
                                     f"RED-BRANCH {untrusted(name)} head {head[:8]}: proof red on "
                                     f"{plain(', '.join(red_jobs(proof)))}, no PR for {hours(self.now - done)} ([proof]({url}))",
                                     dest, tests=self.red_tests(rid) if wanted and not self.defer_logs else (),
                                     run_id=rid if wanted else None))
        return [it for it in out if (mine and it.dest in ("muse", "both")) or (not mine and it.dest in ("claude", "both"))]

    def trunk_proof(self, runs: list[dict]) -> dict:
        """Trunk's proof as the sweep state keeps it: {"sha", "run", "url", "state", "at", "first_red", "pr",
        "branch"}, from the newest trunk run in `runs` that ran the build (built_run), read newest first back to the
        run kept (at most TRUNK_RUN_READS jobs reads). A run that skipped the build (a notes-only push,
        ci/proof_scope.py) never replaces it, so a red stays until a green full proof. `first_red`: the head of the
        first red run of this red streak, and `pr`/`branch`: the PR whose merge it is (its merge message), whose
        owner is paged. Once per run; {} when no full trunk run has been seen."""
        memo = self.__dict__.get("_trunk_proof")
        if memo is not None:
            return memo
        state = self.gh.state if isinstance(getattr(self.gh, "state", None), dict) else {}
        kept = dict(state.get("trunk_proof") or {}) if isinstance(state.get("trunk_proof"), dict) else {}
        trunk = sorted((r for r in runs if r.get("head_branch") == rules.TRUNK and r.get("status") == "completed"),
                       key=lambda r: r.get("created_at") or "", reverse=True)
        checked = max(str(kept.get("checked") or ""), str(kept.get("created") or ""))
        # J-072 (11-14, run 43595346281): a trunk run still going when the sweep marks newer finished runs judged
        # was never read once it went red. `checked` stays below the oldest unfinished trunk run not yet judged.
        unfinished = [r.get("created_at") or "" for r in runs if r.get("head_branch") == rules.TRUNK
                      and r.get("status") != "completed" and (r.get("created_at") or "") > checked]
        below = min(unfinished) if unfinished else None
        newest = next((r.get("created_at") or "" for r in trunk if below is None or (r.get("created_at") or "") < below), "")
        done_all = True
        skipped = [i for i in state.get("trunk_skipped") or [] if isinstance(i, int)][-TRUNK_SKIPPED_KEPT:]
        was, reads = list(skipped), 0
        for r in trunk:
            if r.get("id") == kept.get("run") or (r.get("created_at") or "") <= checked:
                break   # nothing newer than the runs already judged
            if r.get("id") in skipped:
                continue   # judged skip-the-build on an earlier sweep: never read again
            if reads >= TRUNK_RUN_READS:
                done_all = False   # the rest wait for the next sweep: nothing past them is marked judged
                break
            reads += 1
            try:
                if not built_run(self.run_jobs(r["id"])):
                    skipped = (skipped + [r["id"]])[-TRUNK_SKIPPED_KEPT:]
                    continue   # the build was skipped: this run proves nothing about trunk's code
                proof, url, rid, done = self.run_state(r)
            except RateLimited:
                raise
            except GitHubError as err:
                self.out(f"NOTE        trunk proof run {r.get('id')}'s jobs were not read ({err_text(err)}): read next sweep")
                done_all = False
                break
            sha = r.get("head_sha") or ""
            red = proof.startswith("red")
            first = (kept.get("first_red") or sha) if red and str(kept.get("state", "")).startswith("red") else sha
            new = {"sha": sha, "run": rid, "url": url, "state": proof, "at": done, "created": r.get("created_at") or ""}
            if red:
                new["first_red"] = first
                if first == kept.get("first_red") and kept.get("pr") is not None:
                    new["pr"], new["branch"] = kept.get("pr"), kept.get("branch") or ""
                else:   # the commit that broke it: trunk's commits are PR merges, so its message names the branch
                    try:
                        msg = (self.gh.get(f"repos/{self.gh.repo}/commits/{first}") or {}).get("commit", {}).get("message", "")
                    except GitHubError:
                        msg = ""
                    m = MERGE_FROM.match(msg or "")
                    branch = m.group(2).removeprefix(self.gh.repo.split("/")[0] + "/") if m else ""
                    new["pr"], new["branch"] = (int(m.group(1)), branch) if m else (0, "")
            kept = new
            break
        if done_all and newest > checked and kept:   # every run down to the judged ones was read: none is read again
            kept = dict(kept, checked=newest)
        if not getattr(self.gh, "dry_run", False) and kept and isinstance(getattr(self.gh, "state", None), dict):
            self.gh.state["trunk_proof"] = kept
        if not getattr(self.gh, "dry_run", False) and skipped != was and isinstance(getattr(self.gh, "state", None), dict):
            self.gh.state["trunk_skipped"] = skipped
        self.__dict__["_trunk_proof"] = kept
        return kept

    def trunk_items(self, runs: list[dict], reported: frozenset[str], prefix: str = "") -> list[InboxItem]:
        """RED-TRUNK: trunk's full proof is red (trunk_proof). One item per red streak (its first red head), at once
        (inbox_class "now"), to the integrator, and to Muse too when the PR whose merge first went red was hers
        (its fb-bot/ branch): the owner of the commit that broke it. The owner fixes; with no green fix head in 60 min a
        helper reverts that merge in its own PR. No merge freeze: the fix lands by the normal rule."""
        t = self.trunk_proof(runs)
        if not str(t.get("state", "")).startswith("red"):
            return []
        first = t.get("first_red") or t.get("sha") or ""
        branch = t.get("branch") or ""
        dest = "both" if owner_of(branch) == "muse" else "claude"   # Claude's own trunk pushes, too: the integrator
        owner = "Muse" if dest == "both" else "Claude"
        by = f"; first red at {first[:8]}, the merge of #{t['pr']} ({untrusted(branch)})" if t.get("pr") else \
            f"; first red at {first[:8]}"
        undo = f"`git revert -m 1 {first[:8]}`" if t.get("pr") else f"`git revert {first[:8]}`"
        since = parse_time(str(t.get("at") or "")) or self.now
        wanted = dest in ("muse", "both") if prefix == "muse-" else True
        return [InboxItem("RED-TRUNK", rules.TRUNK, first,
                          f"RED-TRUNK trunk's full proof is red on {plain(', '.join(red_jobs(t['state'])))} "
                          f"({hours(self.now - since)}){by}; owner {owner}. {owner} fixes it; no green fix head in "
                          f"60 min, a helper reverts it ({undo}, its own PR). No merge freeze: the fix lands by the "
                          f"normal rule ([proof]({t.get('url') or ''}))", dest,
                          tests=self.red_tests(t.get("run")) if wanted and not getattr(self, "defer_logs", False) else (),
                          run_id=t.get("run") if wanted else None)]

    def ask_thread(self, facts: rules.PrFacts, ctx: dict) -> tuple[int, list[dict]]:
        """Where review asks live (the job issue, else the PR) and its comments. With no job issue, the board's
        comments are read too: Muse's ask goes there, never on the PR (see muse())."""
        where = ctx["issue"]["number"] if ctx.get("issue") else facts.number
        if where != facts.number:
            return where, self.gh.paged(f"repos/{self.gh.repo}/issues/{where}/comments")
        return where, list(ctx["comments"]) + self.board_comments()

    def ask_review(self, facts: rules.PrFacts, ctx: dict, agent: str, why: str) -> None:
        where, comments = self.ask_thread(facts, ctx)
        if agent == "codex":   # the Codex app answers "@codex review" on the PR itself
            where, comments = facts.number, ctx["comments"]
        if agent == "muse" and where == facts.number:   # wake:muse never goes on a PR (the bridge's token cannot clear it)
            where = self.board_issue()
        tag = MARK.format(kind=f"ask-{agent}", pr=facts.number, head=facts.head_sha)
        if self.marked(comments, f"ask-{agent}", facts.number, facts.head_sha):
            self.out(f"ASKED       #{facts.number} review already asked of {agent} on #{where}")
            return
        name = self.team.agent_name.get(agent, agent)
        job = ctx["job"] or f"PR #{facts.number}"
        if agent == "codex" and self.codex_mention(facts, ctx, tag):
            return
        if agent == "codex":   # J-072: this login's "@codex review" never woke Codex (2 of 2, #191 and #150), so the
            # mark only records the ask; the integrator's inbox gets a CODEX-ASK item and posts the mention itself
            self.gh.comment(where, f"Switchboard: {job}, head {facts.head_sha[:8]}: a Codex review of this head is due. "
                                   f"{why}The integrator posts the Codex review command: the switchboard's own does "
                                   f"not wake Codex.\n\n{tag}".replace("@", ""),
                            f"record the Codex ask on #{facts.number} (the integrator posts it)")
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
        if getattr(self, "_board", None) is not None and where == self.board_issue():   # this run's later reads see the ask
            self._board.append({"body": body, "user": {"login": rules.BOT_LOGIN},
                                "created_at": self.now.strftime("%Y-%m-%dT%H:%M:%SZ")})
        if agent == "muse":
            self.gh.write("POST", f"repos/{self.gh.repo}/issues/{where}/labels", {"labels": [MUSE_LABEL]},
                          f"label #{where} {MUSE_LABEL} (the bridge emails Muse, then removes it)")

    def codex_mention(self, facts: rules.PrFacts, ctx: dict, tag: str) -> bool:
        """Post `@codex review` on the PR as the team login, with the team token (J-072). The Codex app answers a
        person's mention and ignores the workflow bot's (2 of 2, #191 and #150), and it reviews by itself only a PR
        a person opened, so the heads the switchboard opens for Muse sat with no Codex review until the integrator
        typed this by hand (#376). The comment is the mention and the ask's mark, nothing else: Codex reads any
        other words as instructions. False when there is no token here or GitHub refused it: the caller then
        records the ask for the integrator, as before. Never on a fork's PR, whoever calls (codex_counts checks it
        too; a pool or fallback order that named Codex would not pass through that)."""
        if not self.merge_token or not ctx.get("own_head"):
            return False
        try:
            got = self.gh.write("POST", f"repos/{self.gh.repo}/issues/{facts.number}/comments",
                                {"body": f"{CODEX_MENTION}\n\n{tag}"},
                                f"ask Codex to review #{facts.number} @{facts.head_sha[:8]} as the team login",
                                token=self.merge_token)
        except GitHubError as err:
            code = http_status(err.detail)
            if code not in MERGE_TOKEN_REFUSED:
                raise   # a rate limit or an outage: this PR waits for the next sweep, as any failed write does
            self.out(f"NOTE  #{facts.number} GitHub refused the team token for the Codex mention (HTTP {code}; "
                     f"{MERGE_TOKEN_ENV} needs {MERGE_TOKEN_NEEDS}): the integrator is asked to post it")
            return False
        login = str(((got or {}).get("user") or {}).get("login") or "").strip().lower() if isinstance(got, dict) else ""
        if login and login not in self.team.verdict_logins:
            # codex_asked reads the mark only from a team login, so a token of any other account would post the
            # mention again every sweep: the caller's own note (the workflow bot's, with the mark) records the ask
            self.out(f"NOTE  #{facts.number} the Codex mention was posted by a login that is not in work/agents.yml "
                     f"verdict_logins: {MERGE_TOKEN_ENV} must be the team login's; the ask is recorded once")
            return False
        return True

    def escalate_review(self, facts: rules.PrFacts, ctx: dict, author: str) -> None:
        """An ask the switchboard made on this head that has had no verdict for review.fallback_after_hours goes
        to the next agent in review.fallback_order, once per head (each ask has its own marker). The caller
        only gets here while needs_review holds: a LAND or a FIX on the head, from anyone, ends this."""
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
    def api_turn(self, facts: rules.PrFacts, ctx: dict, author: str, say: bool = True) -> bool:
        """Is this head's next review the automatic one (its mark not yet checked)? One decision for ask_pr, which
        runs it, and wake_reviews, which starts the run that does (SYSTEM_PLAN 6). `say`: print the Codex wait."""
        if not rules.needs_review(facts, author, self.team):
            return False
        if self.codex_pending(facts, ctx):
            if say:
                self.out(f"WAIT-CODEX  #{facts.number} Codex is reviewing @{facts.head_sha[:8]}: no automatic review yet")
            return False
        if self.team.pool:
            return self.pooled_reviewer(facts, ctx, author)[1] == rules.API_REVIEWER
        return rules.pick_reviewer(author, facts.needs, self.team).route == "claude-api"

    def eye_ok(self, ctx: dict) -> bool:
        """May the eye run today: the worst case of one call fits under every cap (as api_ok for the review)."""
        spec = self.budget.calls["eye"]
        worst = money.worst_case(self.budget, "eye", spec.max_input_tokens)
        return money.check(self.budget, self.entries(), self.now, ctx["job"], worst, self.job_cap(ctx)).ok

    def reviews_due(self, seen: Steered) -> str:
        """What a reviews run would do for this PR as steer left it: "the automatic review" or "the eye" when it is
        its turn on the head, it is not posted yet and the budget lets it run (a refused call posts nothing, so a
        run for it would do nothing); "" otherwise. A head merged in this sweep needs neither."""
        facts, ctx = seen.facts, seen.ctx
        if seen.decision.action == "MERGE" or self.stale(facts):
            return ""
        author = self.team.vendor_of_branch(facts.head_ref)
        # the pool leaves the API out of its rotation when api_ok refuses (pooled_reviewer)
        if self.api_turn(facts, ctx, author, say=False) and (self.team.pool or self.api_ok(ctx)) and \
                not self.marked(ctx["comments"], "review", facts.number, facts.head_sha):
            return "the automatic review"
        if rules.needs_eye(facts) and not self.marked(ctx["comments"], "eye", facts.number, facts.head_sha) and \
                self.eye_ok(ctx):
            return "the eye"
        return ""

    def wake_reviews(self, steered: list[Steered]) -> None:
        """SYSTEM_PLAN 6 (11-13): a sweep's run has no ask job, so a merge never waits behind a review. When a head
        this sweep read waits for the automatic review or the eye, steer starts one "Switchboard reviews" run
        (switchboard.yml part=ask, its own concurrency group), which runs ask alone; the review's comment wakes the
        usual sweep, which merges. One dispatch a sweep, at the first head due; none in a dry run; none while a
        reviews run is queued or running (one read of the run list). A refused dispatch is a WAKE-REVIEWS line, never
        a failed sweep: the next sweep that reads the head tries again, and the fallback clock asks the next reviewer."""
        if self.gh.dry_run or getattr(self, "merge_only", False):
            return
        try:
            due = next(((s, what) for s in steered for what in (self.reviews_due(s),) if what), None)
            if due is None:
                return
            seen, what = due
            n, head = seen.facts.number, seen.facts.head_sha[:8]
            try:
                runs = (self.gh.get(f"repos/{self.gh.repo}/actions/workflows/{SWITCHBOARD_WORKFLOW}/runs"
                                    f"?event=workflow_dispatch&branch={rules.TRUNK}&per_page=20&exclude_pull_requests=true")
                        or {}).get("workflow_runs") or []
            except GitHubError as err:   # not known: start one anyway (GitHub keeps one pending run in the group)
                self.out(f"NOTE        the run list was not read ({err.detail[:120]}): the reviews run is started anyway")
                runs = []
            busy = [r for r in runs if r.get("display_title") == REVIEWS_RUN and r.get("status") != "completed"]
            if busy:
                self.out(f"WAKE-REVIEWS #{n} waits for {what} @{head}: reviews run {busy[0].get('id')} is "
                         f"{busy[0].get('status')}; none started")
                return
            self.gh.write("POST", f"repos/{self.gh.repo}/actions/workflows/{SWITCHBOARD_WORKFLOW}/dispatches",
                          {"ref": rules.TRUNK, "inputs": {"part": "ask"}},
                          f"start the reviews run: #{n} waits for {what} @{head}")
            self.out(f"WAKE-REVIEWS #{n} waits for {what} @{head}: the reviews run is started")
        except (GitHubError, RuntimeError, KeyError, ValueError) as err:
            self.out(f"WAKE-REVIEWS no reviews run started: {type(err).__name__}: {err}")

    def ask_pr(self, facts: rules.PrFacts, ctx: dict) -> None:
        author = self.team.vendor_of_branch(facts.head_ref)
        if self.stale(facts):
            return
        api_turn = self.api_turn(facts, ctx, author)
        # A workflow-file PR is reviewed like any other: no security pass (SYSTEM_PLAN 4.1, 11-13).
        if api_turn:
            if self.marked(ctx["comments"], "review", facts.number, facts.head_sha):
                self.out(f"REVIEWED    #{facts.number} automatic review already posted for @{facts.head_sha[:8]}")
            else:
                self.review(facts, ctx, author)
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
        # The owner's settled calls (CLAUDE.md): #376's review answered NOTE to ask him to confirm one of them
        # (a dead Matron stays dead, 11-05), and a NOTE lands nothing. The security pass judges by SECURITY.md alone.
        settled = claude_calls.settled_calls((self.root / "CLAUDE.md").read_text(encoding="utf-8"))
        if settled and not security:
            guidelines = [claude_calls.text_block(f"<settled_calls>\n{settled}\n</settled_calls>")]
        content = guidelines + [claude_calls.text_block(f"<checklist>\n{checklist}\n</checklist>"),
                   claude_calls.text_block(claude_calls.author_text(
                       f"<pr number=\"{facts.number}\" head=\"{facts.head_sha}\" branch=\"{facts.head_ref}\">\n"
                       f"<title>{ctx['title']}</title>\n<body>\n{ctx['body'][:20000]}\n</body>\n</pr>")),
                   claude_calls.text_block(claude_calls.author_text(f"<diff>\n{diff}\n</diff>"))]
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
        # Every blocking finding in one review (11-09: one per round cost an hour each): the most severe is the repro,
        # the rest are notes starting "Also blocking:", listed apart so the author fixes them all in one push.
        also = [n for n in d["notes"] if verdict == "FIX" and n.lstrip().lower().startswith("also blocking")]
        rest = [n for n in d["notes"] if n not in also]
        if also:
            lines += ["", "Also blocking (fix with the one above, in the same push):"] + [f"- {n}" for n in also[:12]]
        if rest:
            lines += ["", "Notes (never blocking):"] + [f"- {n}" for n in rest[:12]]
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

    def blocked_lines(self) -> list[str]:
        """The digest's blocked jobs. With `plan: live`, the job machine's `blocked` state (a job it comes `after`
        is not closed), so the hand-set state:blocked label can come off; otherwise that label, as before."""
        if self.plan_mode() == "live":
            return [f"#{r.issue} {r.job}: {r.title[:62]} is blocked on {r.why}"
                    for r in self.plan_table()[3] if r.state == "blocked"]
        return [f"#{it['number']} {it['title'][:70]} is blocked" for it in self.jobs().values()
                if any(lab["name"] == "state:blocked" for lab in it["labels"])]

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
        waiting += self.blocked_lines()
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
    ap.add_argument("--save-dry-cache", action="store_true", help=argparse.SUPPRESS)   # a local measurement only
    args = ap.parse_args()
    if args.sha and not all(c in "0123456789abcdef" for c in args.sha):
        ap.error("--sha must be hex")
    now = dt.datetime.now(dt.timezone.utc)
    sw = Switch(GitHub(args.repo, args.dry_run), pathlib.Path(args.root), now)
    # J-056: out of the environment at once, so git, the rule check and every other gh call never inherit them.
    sw.merge_token = os.environ.pop(MERGE_TOKEN_ENV, "").strip()
    sw.review_key = os.environ.pop(REVIEW_KEY_ENV, "").strip()
    hc_sweep = os.environ.pop(HC_SWEEP_ENV, "").strip()
    cache = os.environ.get(ETAG_CACHE_ENV, "").strip()
    # J-072: a dry run (a dispatch with dry_run) reads the cache but never saves it, so it cannot consume the sweep
    # state, the PR records or a ripe timer (the security pass on #360). --save-dry-cache is for a local measurement.
    save_cache = bool(cache) and (not args.dry_run or args.save_dry_cache)
    if cache:
        sw.gh.set_cache_key(sw.review_key)   # signed with a key only switchboard-merge holds: see ghapi.load_etags
        sw.gh.load_etags(cache)
    sw.out(f"switchboard {args.part}{' DRY RUN' if args.dry_run else ''} at {central(now)} on {args.repo}")
    if args.dry_run:
        note_dry_run(os.environ.get("GITHUB_STEP_SUMMARY"))
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    try:
        return run_sweep(sw, args, now, hc_sweep, summary)
    except RateLimited as err:   # J-072: never an error; whatever was not reached waits for the next sweep
        sw.out(f"RATE LIMIT  {err}: the rest of this run waits for the next sweep")
        write_summary(summary, "Rate floor", [f"GitHub's rate limit answered ({plain(err.detail[:160])}); "
                                              "the rest of this run waits for the next sweep."])
        return 0
    finally:
        if save_cache:
            sw.gh.save_etags(cache)
        sw.out(f"GITHUB      {sw.gh.calls} call(s), {sw.gh.not_modified} answered 304 from the ETag cache"
               + (f", {sw.gh.remaining} left this hour" if sw.gh.remaining is not None else ""))


def write_summary(path: str | None, title: str, lines: list[str]) -> None:
    if path and lines:
        with open(path, "a", encoding="utf-8") as f:
            f.write(f"### {title}\n\n" + "".join(f"- {line}\n" for line in lines) + "\n")


def rate_floor(limit: int | None) -> int:
    """The calls kept for the merge path: a share of the hour's limit read at run time (GITHUB_TOKEN has 1,000 an
    hour per repository, an App installation 5,000 or more), never under RATE_FLOOR_MIN."""
    return max(RATE_FLOOR_MIN, round(RATE_FLOOR_SHARE * limit)) if isinstance(limit, int) and limit > 0 else RATE_FLOOR_MIN


def core_rate(gh, token: str = "") -> dict:
    """GET rate_limit's core numbers (limit, remaining, reset): that call does not count against the limit."""
    got = gh._run("GET", "rate_limit", token=token) if token else gh.get("rate_limit")
    core = ((got or {}).get("resources") or {}).get("core") or {}
    return core if isinstance(core, dict) else {}


def rate_probe(sw: "Switch") -> None:
    """The rate left and the limit, before the first counted call. The hosted agent's token (AGENT_GITHUB_TOKEN, ask
    job only) is read once and logged, never used here: is moving reads to it a real gain? (J-072)"""
    try:
        core = core_rate(sw.gh)
    except GitHubError:
        return
    if isinstance(core.get("remaining"), int):
        sw.gh.remaining, sw.gh.reset = core["remaining"], core.get("reset")
    if isinstance(core.get("limit"), int):
        sw.gh.limit = core["limit"]
    sw.out(f"RATE        this run's token: {core.get('remaining')} of {core.get('limit')} left this hour; "
           f"the merge path keeps {rate_floor(sw.gh.limit if isinstance(sw.gh.limit, int) else None)}")
    agent = os.environ.get(AGENT_TOKEN_ENV, "").strip()
    if agent:
        try:
            other = core_rate(sw.gh, agent)
            sw.out(f"RATE        {AGENT_TOKEN_ENV}: {other.get('remaining')} of {other.get('limit')} left this hour (logged only)")
        except GitHubError as err:
            sw.out(f"RATE        {AGENT_TOKEN_ENV}: its rate could not be read ({http_status(err.detail) or 'no status'})")


# A full pass at least this often: every PR read, owner labels. Every clock is checked each sweep (the timers).
# (J-072: was 30 minutes. D2: the ledger and the reconciler no longer run in it.)
FULL_EVERY = dt.timedelta(minutes=60)
SWEEP_OVERLAP = dt.timedelta(minutes=3)   # each delta read starts this long before the last sweep's stamp
PROOF_SPAN = dt.timedelta(hours=6)        # proof runs created this long before the stamp can finish after it
# J-072 (11-13): HUMAN-MERGE waits on a person or a new review, never a clock: a review, a Muse LAND by mail, a label
# or a push all show in the delta (sweep_delta). Read every sweep it cost about 28 uncached calls a PR (fresh_facts).
COLD_ACTIONS = ("FIX", "FIX-CI", "SKIP", "HUMAN-MERGE")
RUN_PAGES_CAP = ghapi.MAX_PAGES * 100     # a proof-run listing this long may have lost rows: read everything
RUNNING = ("in_progress", "queued")       # a proof run not finished yet (running_runs); one listing each
# What the switch loads from its trunk checkout and decides by (its code, agents, look sets, budget, switchboard.yml):
# a change re-reads every PR once (trunk_inputs). Trunk's head itself moves 12 to 27 times an hour (notes commits).
TRUNK_INPUTS = ("tools/switchboard/*.py", "tools/board/*.py", "work/*.yml", "work/looks/**/*")
FULL_PARTS = frozenset({"prs", "auto-open"})   # need the full pass's reading to redo  # waiting on the author's next head: nothing changes them by the clock


COMMENT_LISTING_CAP = ghapi.MAX_PAGES * 100   # a paged listing never returns more (ghapi.paged)
COMMENT_FLOOR_HOURS = 6   # a comment listing's since: 00, 06, 12 or 18 UTC. An hour's floor changed at every hourly
                          # full pass, so none of its pages was ever a 304 (the warm pass: 9 counted reads)


def comment_floor(since: dt.datetime) -> dt.datetime:
    t = since.astimezone(dt.timezone.utc)
    return t.replace(hour=t.hour - t.hour % COMMENT_FLOOR_HOURS, minute=0, second=0, microsecond=0)


def iso_z(t: dt.datetime) -> str:
    return t.astimezone(dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def sweep_state_ok(state) -> bool:
    return (isinstance(state, dict) and isinstance(state.get("prs"), dict) and
            parse_time(str(state.get("stamp") or "")) is not None and parse_time(str(state.get("full_at") or "")) is not None)


@dataclasses.dataclass
class SweepDelta:
    prs: set[int]                 # the open PRs to read this sweep
    comments: int                 # comments posted since the last sweep
    finished: set[str]            # branches whose proof run finished since the last sweep
    finished_auto: set[str]       # of those, the auto-open's (AUTO_PR_PREFIX)
    threads: set[int] = dataclasses.field(default_factory=set)    # every issue or PR something touched (the ledger's)
    active: set[str] = dataclasses.field(default_factory=set)     # branches with a proof run started or finished
    pipe: bool = False            # a CLAUDE_TO_MUSE ask or a bridge post: pipe health reads again
    mail: set[str] | None = None  # the jobs (J-NNN) Muse's bridged posts name, "*" for one naming none (queue_started)


def trunk_inputs(root: pathlib.Path) -> str:
    """A digest of TRUNK_INPUTS in this checkout (J-072, 11-13): the code and config every decision is made by.
    Tests are left out. Reads no GitHub."""
    h = hashlib.sha256()
    files = sorted({f for pattern in TRUNK_INPUTS for f in root.glob(pattern)
                    if f.is_file() and not f.name.startswith("test_")})
    for f in files:
        h.update(str(f.relative_to(root)).encode("utf-8") + b"\0")
        try:
            h.update(f.read_bytes())
        except OSError:
            h.update(b"unreadable")
        h.update(b"\0")
    return h.hexdigest()[:16]


def sweep_inputs(sw) -> str:
    """trunk_inputs, and whether the merge token and the review key are set: a HUMAN-MERGE waits on those too."""
    return f"{trunk_inputs(sw.root)}:{int(bool(getattr(sw, 'merge_token', '')))}{int(bool(getattr(sw, 'review_key', '')))}"


def running_runs(sw) -> dict[str, str] | None:
    """The proof runs not finished now, {run id: branch}, one listing per RUNNING status (J-072, 11-13). A re-run
    keeps its first created_at (43109809366: created 11-11, re-run 11-13), so the delta's created listing never sees
    it finish: the next sweep finds it gone from here. None when a listing failed or may have lost rows."""
    out: dict[str, str] = {}
    try:
        for status in RUNNING:
            got = sw.gh.paged(f"repos/{sw.gh.repo}/actions/workflows/{PROOF_WORKFLOW}/runs?status={status}",
                              "workflow_runs")
            if len(got) >= RUN_PAGES_CAP:
                return None
            out.update({str(r["id"]): r.get("head_branch") or "" for r in got
                        if isinstance(r, dict) and r.get("status") in RUNNING and "id" in r})
    except RateLimited:
        raise
    except GitHubError as err:
        sw.out(f"NOTE        the running proof runs were not read ({err_text(err)}): every PR is read")
        return None
    return out


def issue_needs(issue: dict | None) -> list[str]:
    """The `needs:` labels of a job issue, the only ones pr_facts takes from it (wake:muse comes and goes)."""
    return sorted(str(l.get("name")) for l in (issue or {}).get("labels") or []
                  if isinstance(l, dict) and str(l.get("name") or "").startswith("needs:"))


def sweep_delta(sw, prs: list[dict], state: dict, running: dict[str, str] | None = None) -> SweepDelta:
    """What changed since the last sweep (J-072): the proof runs created in the span and those running now
    (`running`, read by run_sweep before this), the comments since its stamp, and the open job issues.
    A PR is read when it is new, its head or updated_at moved (a push, label, review or PR comment), a proof run
    on its branch finished (a re-run too: it was running at the last sweep and is not now), a comment landed on
    it, on its job issue or names it (a bridged LAND on the job issue or the board), its job issue or that issue's
    needs: labels changed, the open jobs changed while it holds the eye's FIX (rules.eye_relook), the switch's
    trunk inputs or its tokens changed (sweep_inputs), or its last decision was not COLD (a clock-driven wait: Codex, the review
    fallback, a merge). With no baseline, or a listing that may have lost rows, every PR is read."""
    since = parse_time(state["stamp"]) - SWEEP_OVERLAP
    now = sw.now
    repo = sw.gh.repo
    # paged (11-13): 145 proof runs in the span, so one page of 100 always hit the bail-out below and every fast
    # sweep read every PR (24 of 24)
    runs = sw.gh.paged(f"repos/{repo}/actions/workflows/{PROOF_WORKFLOW}/runs?created=%3E%3D{iso_z(since - PROOF_SPAN)}",
                       "workflow_runs")
    finished = {r.get("head_branch") or "" for r in runs if r.get("status") == "completed"
                and (parse_time(r.get("updated_at") or "") or since) >= since}
    was_running = state.get("running") if isinstance(state.get("running"), dict) else None
    if running is not None and was_running is not None:
        finished |= {str(b) for k, b in was_running.items() if k not in running}
    comments = sw.gh.get(f"repos/{repo}/issues/comments?since={iso_z(since)}&per_page=100") or []
    nums = {p["number"] for p in prs}
    recs = state.get("prs") or {}
    auto = {b for b in finished if b.startswith(AUTO_PR_PREFIX)}
    active = finished | {r.get("head_branch") or "" for r in runs if (parse_time(r.get("created_at") or "") or since) >= since}
    pipe = any((c.get("body") or "").lstrip().startswith(("CLAUDE_TO_MUSE:", BRIDGE_POST))
               and PIPE_OK_MARK not in (c.get("body") or "") for c in comments)   # #350: the all-clear asks nothing
    mail: set[str] = set()
    for c in comments:
        body = (c.get("body") or "").lstrip()
        if body.startswith(BRIDGE_POST):
            mail |= set(re.findall(r"\bJ-\d{3,}[a-z]?\b", body.split("\n", 1)[0])) or {"*"}   # J-044b too
    threads: set[int] = set()
    for c in comments:
        try:
            threads.add(int((c.get("issue_url") or "").rsplit("/", 1)[1]))
        except (ValueError, IndexError):
            continue
    if len(comments) >= 100:   # a page of comments or more: a mail may be in the rows not read, so the queue counts
        mail.add("*")
    if (len(runs) >= RUN_PAGES_CAP or len(comments) >= 100 or running is None or was_running is None
            or state.get("inputs") != sweep_inputs(sw)):   # a listing may have lost rows, or no baseline
        return SweepDelta(set(nums), len(comments), finished, auto, threads | set(nums), active, True, mail)
    relook = sorted(sw.jobs()) != state.get("jobs")   # a job opened or closed: an eye FIX may read as a LAND now
    changed: set[int] = set()
    for p in prs:
        rec = recs.get(str(p["number"]))
        ripen = parse_time(str(rec.get("ripen") or "")) if isinstance(rec, dict) else None
        _, issue = sw.job_issue(p)
        if (not isinstance(rec, dict) or rec.get("head") != (p.get("head") or {}).get("sha")
                or rec.get("updated") != p.get("updated_at") or not rec.get("cold")
                or (ripen is not None and ripen <= now)   # an inbox clock (NOT-MERGED, RED-STUCK, QUIET) ripened
                or (p.get("head") or {}).get("ref") in finished
                or rec.get("job_issue") != (issue or {}).get("number") or (rec.get("needs") or []) != issue_needs(issue)
                or (relook and rec.get("eye_fix"))):
            changed.add(p["number"])
    by_issue: dict[int, set[int]] = {}
    for k, rec in recs.items():
        if isinstance(rec, dict) and rec.get("job_issue"):
            by_issue.setdefault(int(rec["job_issue"]), set()).add(int(k))
    for c in comments:
        try:
            thread = int((c.get("issue_url") or "").rsplit("/", 1)[1])
        except (ValueError, IndexError):
            continue
        hit = {thread}
        # 11-13: the switch's own posts off a PR's thread (its merged notes on #350 re-read all six of that job's
        # drafts) change no PR's facts; any other comment there still reads its PRs
        if (c.get("user") or {}).get("login") != rules.BOT_LOGIN or (c.get("body") or "").lstrip().startswith(BRIDGE_POST):
            hit |= by_issue.get(thread, set()) | {int(r) for r in re.findall(r"#(\d{1,6})\b", (c.get("body") or "")[:600])}
        changed |= hit & nums
    return SweepDelta(changed, len(comments), finished, auto, threads | changed, active, pipe, mail)


def pr_record(sw, pr: dict, facts, ctx: dict, seen, ask: bool) -> dict:
    """What the next sweep compares (sweep_delta): the head, updated_at, the job issue and its needs: labels, whether
    it holds the eye's FIX, and whether only a new head or comment can change this PR (cold)."""
    cold = seen is None or (seen.decision.action in COLD_ACTIONS   # a refused merge token is tried again next sweep
                            and not any(str(r).startswith(TOKEN_REFUSED) for r in seen.decision.reasons))
    if ask and not sw.stale(facts):   # the ask part is done with a head once its review and eye are posted
        author = sw.team.vendor_of_branch(facts.head_ref)
        comments = ctx.get("comments") or []
        review = rules.needs_review(facts, author, sw.team)
        cold = (cold and (not review or sw.marked(comments, "review", facts.number, facts.head_sha))
                and (not rules.needs_eye(facts) or sw.marked(comments, "eye", facts.number, facts.head_sha)))
    issue = (ctx.get("issue") or {}) if isinstance(ctx, dict) else {}
    eye_fix = any(v.kind == "eye" and v.verdict == "FIX" and v.author_login == rules.BOT_LOGIN
                  for v in getattr(facts, "verdicts", ()))
    return {"head": (pr.get("head") or {}).get("sha"), "updated": pr.get("updated_at"), "job_issue": issue.get("number"),
            "needs": issue_needs(issue), "eye_fix": eye_fix, "cold": bool(cold)}


def run_sweep(sw, args, now: dt.datetime, hc_sweep: str = "", summary: str | None = None) -> int:
    """One run's parts (main). Exit status: 2 only when the sweep itself could not finish: a part failed, or
    every PR read failed. One PR's error is that PR's line and a step-summary line, not an outage (J-072: run
    42663592988 failed for one write to #357, so the watchdog read the switchboard as down). Under rate_floor()
    calls left only the merge path runs; a rate-limited item waits for the next sweep."""
    steer, ask = args.part in ("steer", "all"), args.part in ("ask", "all")
    rate_probe(sw)
    skipped: list[str] = []

    def floor() -> int:
        return rate_floor(sw.gh.limit if isinstance(sw.gh.limit, int) else None)

    def low(part: str) -> bool:
        if isinstance(sw.gh.remaining, int) and sw.gh.remaining < floor():
            skipped.append(part)
            return True
        return False

    def rate_lines() -> list[str]:
        if not skipped:
            return []
        reset = (f", the hour resets at {central(dt.datetime.fromtimestamp(sw.gh.reset, dt.timezone.utc))}"
                 if sw.gh.reset else "")
        return [f"rate floor: {sw.gh.remaining} GitHub call(s) left (floor {floor()} of {sw.gh.limit}){reset}. Skipped this run: "
                f"{', '.join(dict.fromkeys(skipped))}. The merge path ran; the rest waits for the next sweep."]

    if args.issue:
        if steer and args.label.startswith("owner:"):
            sw.owner_labels(args.issue, args.label)
        return 0
    if args.digest or args.force_digest:
        if steer:
            sw.digest(args.force_digest)
        return 0
    if ask and low("the ask part (reviews, the eye, the sessions)"):
        ask = False
        if not steer:
            sw.out(f"RATE FLOOR  {sw.gh.remaining} calls left: the ask part waits for the next sweep")
            write_summary(summary, "Rate floor", rate_lines())
            return 0
    sw.merge_only = low("review asks, owner labels, superseded closes")
    if sw.merge_only:
        sw.out(f"::warning::RATE FLOOR  {sw.gh.remaining} of {sw.gh.limit} calls left (floor {floor()}): the merge path "
               "only this run. The call cut was not enough.")
    targeted = bool(args.pr or args.sha)
    # J-072: read only what changed. A full pass every FULL_EVERY (and on a cold cache); between, only the PRs that
    # changed since the last sweep are read, and the whole-sweep parts run only when something changed.
    part_key = "ask" if ask and not steer else "steer" if not ask else "all"
    state = sw.gh.state.get(part_key) if isinstance(sw.gh.state, dict) else None
    full = targeted or not sweep_state_ok(state) or now - parse_time(state["full_at"]) >= FULL_EVERY
    # J-072 (ChatGPT on #360): a part that failed or was skipped is due again at the next sweep, never at the next
    # hour. A full-pass part (it needs every PR read) makes the next sweep a full pass.
    retry = set((state or {}).get("retry") or []) if not full else set()
    if retry & FULL_PARTS:
        sw.out(f"RETRY       {', '.join(sorted(retry & FULL_PARTS))} did not finish last sweep: a full pass now")
        full = True
    failed = 0
    if ask:
        failed += sw.watch_sessions()
    steered: list[Steered] = []
    item_errors: list[str] = []
    deferred: list[str] = []
    prs = sw.target_prs(args.pr, args.sha)
    # J-072 (11-13): the proof runs not finished now, every sweep (the full pass too), so the next one sees a re-run end
    running = None if targeted else running_runs(sw)
    delta: SweepDelta | None = None if full else sweep_delta(sw, prs, state, running)
    if steer and not targeted and not sw.merge_only and full:
        sw.sweep_owner_labels()
    if delta is not None:
        sw.out(f"FAST SWEEP  {len(delta.prs)} of {len(prs)} PR(s) changed since {central(parse_time(state['stamp']))}; "
               f"the full pass (every PR read, owner labels) is every {hours(FULL_EVERY)}; every clock is checked now")
    records: dict[str, dict] = dict((state or {}).get("prs") or {}) if delta is not None else {}
    read = 0
    for pr in prs:
        if delta is not None and pr["number"] not in delta.prs:
            continue
        read += 1
        before = len(steered)
        try:   # one PR's trouble never stops the others; every action is safe to repeat next run
            facts, ctx = sw.pr_facts(pr)
            if ask:
                sw.ask_pr(facts, ctx)
            if steer:
                if not sw.merge_only:
                    sw.close_superseded(facts, ctx)
                sw.steer_pr(facts, ctx, steered)
            records[str(pr["number"])] = pr_record(sw, pr, facts, ctx, steered[-1] if len(steered) > before else None, ask)
        except RateLimited as err:
            deferred.append(f"#{pr['number']}")
            records.pop(str(pr["number"]), None)   # read again next sweep
            sw.out(f"RATE LIMIT  #{pr['number']}: left for the next sweep ({err.detail[:120]})")
        except (GitHubError, RuntimeError, KeyError, ValueError, zipfile.BadZipFile) as err:
            item_errors.append(f"#{pr['number']}: {type(err).__name__}: {err}")
            records.pop(str(pr["number"]), None)
            sw.out(f"ERROR       #{pr['number']}: {type(err).__name__}: {err}")
    if read >= 2 and len(item_errors) == read:
        failed += 1   # no PR could be read: the sweep did not finish
        sw.out(f"ERROR       every one of the {read} PRs read failed: the sweep did not finish")
    if steer and not ask:   # SYSTEM_PLAN 6: the reviews run apart; a head waiting for one starts it (never in a dry run)
        sw.wake_reviews(steered)
    quiet = delta is not None and not delta.prs and not delta.comments and not delta.finished and \
        not retry & {"inbox", "muse"}
    timers: dict = dict((state or {}).get("timers") or {}) if sweep_state_ok(state) else {}   # kept until replaced
    timers.pop("ledger", None)   # D2: the ledger left the sweep; an old cache's clock for it is dropped
    dirty: dict = dict((state or {}).get("dirty") or {}) if delta is not None else {}
    not_done: set[str] = set()   # the parts that failed or were skipped: retried at the next sweep
    if delta is not None:   # what changed since the full pass
        dirty = {"threads": sorted(set(dirty.get("threads") or []) | delta.threads),
                 "branches": sorted(set(dirty.get("branches") or []) | delta.active)}

    def part(name: str, fn, *extra) -> object:
        nonlocal failed
        if low(name):
            not_done.add(name)
            return None
        try:
            return fn()
        except RateLimited as err:
            skipped.append(name)
            not_done.add(name)
            sw.out(f"RATE LIMIT  {name}: left for the next sweep ({err.detail[:120]})")
        except (GitHubError, RuntimeError, KeyError, ValueError, zipfile.BadZipFile, *extra) as err:
            failed += 1
            not_done.add(name)
            sw.out(f"ERROR       {name}: {type(err).__name__}: {err}")
        return None

    # The job machine's pass runs on the full pass only (a fast sweep keeps its read count). Live, it runs here:
    # the inbox below lists its late jobs and the queue check counts from its table. In shadow it only prints,
    # so it runs at the very end of this function, after every live part.
    plan_due = bool(steer and not targeted and delta is None)
    if plan_due and sw.plan_mode() == "live":
        part("plan", lambda: sw.plan_check(steered))

    if steer and (delta is None or delta.finished_auto):   # a fast sweep: only branches whose proof just finished
        part("auto-open", lambda: sw.auto_open_prs(None if delta is None else delta.finished_auto))
    red_ripe = [r for r in timers.get("red") or []
                if (parse_time(str(r.get("updated_at") or "")) or now) + INBOX_BRANCH_RED <= now]
    # J-097: Muse's queue is counted on every full pass and on every fast sweep that saw something that can start a
    # job (a bridged Muse post, a new PR, a proof run on a branch naming a queued job); a sweep with none of those
    # costs no call (the J-072 budgets). timers["queue"] keeps the last count's item and the key last seen in the
    # inbox, so a QUEUE-LOW not yet listed wakes even a quiet sweep's inbox.
    queue_timer = dict(timers.get("queue")) if isinstance(timers.get("queue"), dict) else {}
    queue_key, inbox_ran = "", False
    if steer and not targeted:
        new_pr = delta is not None and any(str(p["number"]) not in (state.get("prs") or {}) for p in prs)
        count_now = delta is None or sw.queue_started(delta, new_pr, queue_timer)
        if count_now:
            sw.queue_waiting = None
            part("queue check", sw.queue_check)
        if "queue check" in not_done or not count_now:
            sw.queue_items = queue_item_of(queue_timer.get("item"))   # not counted now: the last count stands
        else:   # J-072 (11-13): the count's unstarted jobs and queue text, what the next fast sweep's trigger compares
            queue_timer = {"item": queue_item_record(sw.queue_items), "listed": queue_timer.get("listed") or ""}
            if isinstance(getattr(sw, "queue_waiting", None), list):
                queue_timer.update(waiting=sw.queue_waiting, source=sw.queue_source())
        queue_key = sw.queue_items[0].key if sw.queue_items else ""
    queue_due = bool(queue_key) and queue_key != queue_timer.get("listed")
    # J-103: the digest entries no inbox comment carried yet, and the heads already re-run, from the last sweep
    reran_before = [h for h in timers.get("rerun") or [] if isinstance(h, str)]
    if steer and not targeted:
        sw.digest_held = digest_of(timers.get("digest"))
        sw.rerun_heads = frozenset(reran_before)
    if steer and not targeted and not quiet:   # the inbox reads a whole sweep, so a run for one PR posts none
        inbox_ran = True
        # 11-13: no hourly flush any more, and the digest never posts alone (inbox_due), so a full pass wakes no one
        part("inbox", lambda: sw.inbox(steered))
        part("muse", lambda: sw.muse(steered))   # J-052: Muse's items go to Muse by comment + wake:muse
        if not not_done & {"inbox", "muse"}:   # a failed read found no red run: the old ones are kept
            timers["red"] = list(getattr(sw, "red_pending", {}).values())
    elif steer and not targeted and (red_ripe or queue_due or sw.inbox_clock()):   # J-072: a quiet sweep, but a clock is
        if red_ripe:                                                                 # up: just those (11-13: or the inbox's)
            sw.out(f"RIPE        RED-BRANCH clock: {', '.join(sorted(r.get('head_branch') or '?' for r in red_ripe))}")
        if queue_due:
            sw.out("RIPE        QUEUE-LOW: the queue changed and its item is not in the inbox yet")
        if not red_ripe and not queue_due:
            sw.out("RIPE        inbox: held items are due (an ask or the hold's end)")
        sw.ripe_runs = red_ripe
        inbox_ran = True
        part("inbox", lambda: sw.inbox(steered))
        if red_ripe:
            part("muse", lambda: sw.muse(steered))
        sw.ripe_runs = None
        if red_ripe and not not_done & {"inbox", "muse"}:
            fired = {r.get("id") for r in red_ripe}
            timers["red"] = [r for r in timers.get("red") or [] if r.get("id") not in fired] + \
                list(getattr(sw, "red_pending", {}).values())
    if steer and not targeted:
        sw.mark_reruns(steered)   # J-103
        if not queue_key or (inbox_ran and "inbox" not in not_done and queue_key in getattr(sw, "inbox_keys", set())):
            queue_timer["listed"] = queue_key
        if queue_timer.get("item") or queue_timer.get("listed") or "waiting" in queue_timer:
            timers["queue"] = queue_timer
        else:
            timers.pop("queue", None)
        reran = list(dict.fromkeys(reran_before + list(getattr(sw, "reran_now", []))))[-MAX_RERUN_HEADS:]
        held = [digest_record(it) for it in sw.digest_held]
        for name, value in (("rerun", reran), ("digest", held)):
            if value:
                timers[name] = value
            else:
                timers.pop(name, None)
    pipe_state = dict(timers.get("pipe") or {})
    bridge_env = os.environ.get(BRIDGE_VERSION_ENV, "").strip()
    if steer and not targeted and delta is None:
        run_pipe = True
    elif steer and not targeted:   # J-072: pipe health's clocks and triggers, from the cached facts
        if bridge_env and bridge_env == pipe_state.get("live") and pipe_state.get("version_at"):
            pipe_state["version_at"] = iso_z(now + BRIDGE_SEEN + dt.timedelta(seconds=1))   # this sweep is the clock's
        at = [parse_time(str(pipe_state.get(k) or "")) for k in ("next_at", "version_at")]
        wake = part("pipe wake read", sw.wake_listing)
        run_pipe = ("pipe health" in retry or any(t is not None and t <= now for t in at) or delta.pipe
                    or (bool(bridge_env) and bridge_env != pipe_state.get("live"))
                    or (wake is not None and wake != pipe_state.get("wake")))
    else:
        run_pipe = False
    if run_pipe:
        sw.pipe_next = sw.bridge_until = None
        alarms = part("pipe health", sw.pipe_health)   # bridge-health: once per incident on the board
        write_summary(summary, "PIPE ALARM", [plain(a) for a in alarms or []])
        until = getattr(sw, "bridge_until", None)
    if run_pipe and "pipe health" not in not_done:   # a failed read leaves the last good clocks in place
        pipe_state = {"next_at": iso_z(sw.pipe_next) if getattr(sw, "pipe_next", None) else None,
                      "version_at": iso_z(until) if until else None, "live": getattr(sw, "bridge_live", ""),
                      "wake": getattr(sw, "wake_numbers", None)}
    timers["pipe"] = pipe_state
    if steer and not targeted and not sw.merge_only:   # #350: after every part that reads or posts on the board
        part("board roll", sw.board_roll)

    if delta is None and (item_errors or deferred):
        not_done.add("prs")   # those PRs were not read: a full pass again
    if not_done:
        sw.out(f"RETRY       next sweep: {', '.join(sorted(not_done))}")
    if steer and not (args.pr or args.sha):
        if not args.dry_run:   # J-072: the end of a full non-dry sweep (a run for one PR or sha is not one)
            sweep_heartbeat(hc_sweep, failed, sw.out)
    if not targeted and isinstance(sw.gh.state, dict) and not sw.merge_only:
        open_now = {str(p["number"]) for p in prs}
        ripen = getattr(sw, "_ripen", {})
        for k, rec in records.items():   # the inbox's clocks, read after the loop: a cold PR is read again then
            if int(k) in ripen:
                rec["ripen"] = iso_z(ripen[int(k)])
            elif str(k) in {str(p["number"]) for p in prs if delta is None or p["number"] in delta.prs}:
                rec.pop("ripen", None)   # read this sweep, and no clock is left on it
        sw.gh.state[part_key] = {"stamp": iso_z(now), "full_at": iso_z(now) if delta is None else state["full_at"],
                                 "prs": {k: v for k, v in records.items() if k in open_now},
                                 "timers": timers, "retry": sorted(not_done),
                                 "dirty": {} if delta is None else dirty,
                                 # J-072 (11-13): the baselines sweep_delta compares; a failed read keeps the last one
                                 "running": running if running is not None else (state or {}).get("running"),
                                 "inputs": sweep_inputs(sw),
                                 "jobs": sorted(sw._jobs) if isinstance(getattr(sw, "_jobs", None), dict)
                                 else (state or {}).get("jobs")}
    write_summary(summary, "Item errors (the sweep finished; each is retried next sweep)",
                  [plain(e[:200]) for e in item_errors])
    write_summary(summary, "Rate floor", rate_lines() + ([f"rate-limited, left for the next sweep: {', '.join(deferred)}"]
                                                         if deferred else []))
    if plan_due and sw.plan_mode() == "shadow" and now.hour % PLAN_SHADOW_EVERY_H == 0:
        # Shadow: after every live part, the heartbeat and the sweep state. No live part waits for its reads, and
        # nothing in it fails the sweep, is retried, or changes what the next sweep does.
        if isinstance(sw.gh.remaining, int) and sw.gh.remaining < 2 * floor():
            sw.out("NOTE        plan (shadow): left out this pass, less than twice the rate floor is left")
        else:
            try:
                sw.plan_check(steered)
            except Exception as err:   # noqa: BLE001  plan_check keeps its own errors in shadow; this is the last guard
                sw.out(f"NOTE        plan (shadow): not computed this pass ({type(err).__name__})")
    return 2 if failed else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except RateLimited as err:   # J-072: GitHub's rate limit is never an outage; the next sweep goes on
        print(f"switchboard: rate limited, left for the next sweep: {err}", file=sys.stderr)
        sys.exit(0)
    except (GitHubError, money.BudgetConfigError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as err:
        print(f"switchboard: {type(err).__name__}: {err}", file=sys.stderr)
        sys.exit(2)
