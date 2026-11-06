"""Unit tests for the switchboard's integrator inbox (J-051): one comment on PR #282 per sweep, new items only.

Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
No network, no clock, no files: GitHub and the time are given.
"""

from __future__ import annotations

import datetime as dt
import pathlib
import re
import sys
import types
import unittest
from urllib.parse import unquote

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import rules  # noqa: E402
import switch  # noqa: E402
import test_auto_pr  # noqa: E402

AGENTS = {
    "agents": {
        "claude": {"vendor": "anthropic", "names": ["Claude"], "branch_prefixes": ["claude/"]},
        "chatgpt": {"vendor": "openai", "names": ["ChatGPT"], "branch_prefixes": ["chatgpt/"]},
        "grok": {"vendor": "xai", "names": ["Grok", "PE"], "branch_prefixes": ["grok/", "pe/"]},
        "muse": {"vendor": "meta", "names": ["Muse", "FB"], "branch_prefixes": ["muse/", "fb-bot/"]},
    },
    "review": {"claude_authored_asks": ["chatgpt", "muse"], "matron_reviewer": "muse"},
    "verdict_logins": ["IssisX", "github-actions[bot]"],
}
TEAM = rules.team_from_config(AGENTS)
NOW = dt.datetime(2026, 11, 6, 20, 0, tzinfo=dt.timezone.utc)
HEAD = "1a2b3c4d5e6f708192a3b4c5d6e7f8091a2b3c4d"
OTHER_HEAD = "ffffffff00000000ffffffff00000000ffffffff"
OLD_HEAD = "0123456789abcdef0123456789abcdef01234567"
RUN_URL = "https://github.com/IssisX/ScraperX/actions/runs/7"
BOT = rules.BOT_LOGIN
INBOX_COMMENTS = f"repos/IssisX/ScraperX/issues/{switch.INBOX_PR}/comments"
RED_JOBS = [{"name": "native-tests", "status": "completed", "conclusion": "failure"},
            {"name": "host-build", "status": "completed", "conclusion": "success"},
            {"name": "publish-checkpoint", "status": "completed", "conclusion": "failure"}]
GREEN_OFF_TRUNK = [{"name": "native-tests", "status": "completed", "conclusion": "success"},
                   {"name": "publish-checkpoint", "status": "completed", "conclusion": "failure"}]
# GitHub's words in steer job 128075717626 (run 42271059947), as gh prints them.
REFUSED = "gh: GitHub Actions is not permitted to create or approve pull requests. (HTTP 403)"


def ago(hours: float) -> str:
    return (NOW - dt.timedelta(hours=hours)).strftime("%Y-%m-%dT%H:%M:%SZ")


def land(head: str = HEAD, hours: float = 1.0, who: str = "Codex") -> rules.Verdict:
    return rules.Verdict("openai", who, "review", "LAND", head[:8], ago(hours), rules.CODEX_LOGIN, False)


def fix(head: str = HEAD, hours: float = 1.0) -> rules.Verdict:
    return rules.Verdict("openai", "Codex", "review", "FIX", head[:8], ago(hours), rules.CODEX_LOGIN, False)


def run(rid: int, branch: str, head: str, hours: float, conclusion: str = "failure", status: str = "completed") -> dict:
    return {"id": rid, "head_branch": branch, "head_sha": head, "status": status, "conclusion": conclusion,
            "created_at": ago(hours + 0.5), "updated_at": ago(hours),
            "html_url": f"https://github.com/IssisX/ScraperX/actions/runs/{rid}"}


def steered(number=256, ref="fb-bot/integrate-j042-windup", head=HEAD, proof="green", verdicts=(), workflows=False,
            draft=False, mergeable=True, block=False, never=False, committed=None, job="J-042", proof_done_at=None,
            updated_at=None, run_id=7, merge_note="") -> switch.Steered:
    """One PR as steer_pr leaves it: the facts, the context and the switch's own decision on them."""
    facts = rules.PrFacts(number, head, ref, rules.TRUNK, draft, mergeable, committed or ago(5), proof,
                          tuple(verdicts), frozenset(), block, workflows, never)
    ctx = {"job": job, "proof_url": RUN_URL, "run_id": run_id, "proof_done_at": proof_done_at or ago(1),
           "updated_at": updated_at or ago(0.1)}
    return switch.Steered(facts, ctx, rules.merge_decision(facts, TEAM.vendor_of_branch(ref), TEAM), merge_note)


def human_merge(head=HEAD, number=256) -> switch.Steered:
    return steered(number=number, head=head, verdicts=[land(head, hours=0.05)], workflows=True, proof_done_at=ago(0.05))


class FakeGH:
    """The GitHub calls the inbox makes: reads from tables, comments recorded (only logged in a dry run)."""

    repo = "IssisX/ScraperX"

    def __init__(self, runs=(), jobs=None, open_prs=(), heads=None, inbox=(), dry_run=False):
        self.runs, self.jobs, self.open_prs = list(runs), dict(jobs or {}), list(open_prs)
        self.heads = dict(heads or {})
        self.inbox = list(inbox)
        self.dry_run = dry_run
        self.gets, self.posts, self.log = [], [], []

    def route(self, path):
        self.gets.append(path)
        base = path.split("?")[0]
        if base == f"repos/{self.repo}/issues/{switch.INBOX_PR}":
            return {"number": switch.INBOX_PR, "comments": len(self.inbox)}
        if base == INBOX_COMMENTS:
            page = int(re.search(r"[?&]page=(\d+)", path).group(1))
            return self.inbox[(page - 1) * 100: page * 100]
        if "/actions/workflows/" in base and base.endswith("/runs"):
            return {"workflow_runs": list(self.runs)}
        if "/actions/runs/" in base and base.endswith("/jobs"):
            return {"jobs": list(self.jobs.get(int(base.split("/actions/runs/")[1].split("/")[0]), []))}
        if base == f"repos/{self.repo}/pulls":
            return list(self.open_prs)
        if "/branches/" in base:
            name = unquote(base.split("/branches/", 1)[1])
            if name in self.heads:
                return {"name": name, "commit": {"sha": self.heads[name]}}
            raise switch.GitHubError("GET", path, "gh: Branch not found (HTTP 404)")
        raise switch.GitHubError("GET", path, "gh: Not Found (HTTP 404)")

    def get(self, path, accept=""):
        return self.route(path)

    def paged(self, path, key=""):
        got = self.route(path)
        return list(got[key] if key else got)

    def comment(self, number, body, what=""):
        if self.dry_run:
            self.log.append(f"DRY  POST repos/{self.repo}/issues/{number}/comments  {what}")
            return None
        self.posts.append((number, body))
        if number == switch.INBOX_PR:
            self.inbox.append({"body": body, "user": {"login": BOT}, "created_at": NOW.isoformat()})
        return {}


def make(gh, now=NOW):
    sw = switch.Switch.__new__(switch.Switch)
    sw.gh, sw.now, sw.team, sw.lines, sw._load = gh, now, TEAM, [], None
    sw.out = sw.lines.append
    return sw


def marker_keys(body: str) -> list[str]:
    found = switch.INBOX_KEYS.findall(body)
    return found[-1].split(",") if found else []


def items_of(gh, *s) -> list[switch.InboxItem]:
    return make(gh).inbox_items(list(s), frozenset())


class Posting(unittest.TestCase):
    def test_a_new_item_is_posted_once(self):
        gh = FakeGH()
        s = human_merge()
        self.assertEqual(make(gh).inbox([s]), 1)
        (number, body), = gh.posts
        self.assertEqual(number, 282)
        self.assertEqual(body.split("\n", 1)[0], "Switchboard (inbox):")
        self.assertIn("- MERGE-BY-HAND [#256](https://github.com/IssisX/ScraperX/pull/256) "
                      "`fb-bot/integrate-j042-windup` head 1a2b3c4d, J-042: green, LAND by Codex, but the switchboard "
                      "will not merge it: ready, but it changes .github/workflows", body)
        self.assertEqual(marker_keys(body), [switch.inbox_key("MERGE-BY-HAND", "#256", HEAD)])
        self.assertTrue(body.endswith("-->"))
        sw = make(gh)   # the next sweep, same head: nothing new
        self.assertEqual(sw.inbox([s]), 0)
        self.assertEqual(len(gh.posts), 1)
        self.assertIn("INBOX       nothing new", sw.lines)

    def test_a_key_in_an_earlier_comment_is_not_posted_again(self):
        key = switch.inbox_key("MERGE-BY-HAND", "#256", HEAD)
        gh = FakeGH(inbox=[{"body": "Switchboard (inbox):\n\n- old\n\n" + switch.INBOX_MARK.format(keys=key),
                            "user": {"login": BOT}}])
        self.assertEqual(make(gh).inbox([human_merge()]), 0)
        self.assertEqual(gh.posts, [])

    def test_a_new_head_is_posted_again_and_the_marker_carries_the_old_key(self):
        gh = FakeGH()
        make(gh).inbox([human_merge(HEAD)])
        self.assertEqual(make(gh).inbox([human_merge(OTHER_HEAD)]), 1)
        self.assertEqual(len(gh.posts), 2)
        body = gh.posts[1][1]
        self.assertIn("head ffffffff", body)
        self.assertNotIn("head 1a2b3c4d", body)
        self.assertEqual(marker_keys(body), [switch.inbox_key("MERGE-BY-HAND", "#256", HEAD),
                                             switch.inbox_key("MERGE-BY-HAND", "#256", OTHER_HEAD)])

    def test_only_the_new_items_are_listed(self):
        gh = FakeGH()
        make(gh).inbox([human_merge(number=256)])
        self.assertEqual(make(gh).inbox([human_merge(number=256), human_merge(number=260)]), 1)
        body = gh.posts[1][1]
        self.assertIn("[#260]", body)
        self.assertNotIn("[#256]", body)

    def test_nothing_new_posts_nothing(self):
        gh = FakeGH()
        waiting = steered(verdicts=())   # green, no LAND yet: the reviewers' job, not the integrator's
        self.assertEqual(make(gh).inbox([waiting]), 0)
        self.assertEqual(make(gh).inbox([]), 0)
        self.assertEqual(gh.posts, [])

    def test_a_marker_by_any_other_login_does_not_count(self):
        key = switch.inbox_key("MERGE-BY-HAND", "#256", HEAD)
        gh = FakeGH(inbox=[{"body": switch.INBOX_MARK.format(keys=key), "user": {"login": "mallory"}}])
        self.assertEqual(make(gh).inbox([human_merge()]), 1)

    def test_only_the_newest_two_pages_of_the_inbox_are_read(self):
        key = switch.inbox_key("MERGE-BY-HAND", "#256", HEAD)
        filler = [{"body": "chatter", "user": {"login": "IssisX"}} for _ in range(249)]
        gh = FakeGH(inbox=filler + [{"body": switch.INBOX_MARK.format(keys=key), "user": {"login": BOT}}])
        self.assertEqual(make(gh).inbox([human_merge()]), 0)
        pages = sorted(int(re.search(r"[?&]page=(\d+)", g).group(1)) for g in gh.gets if g.startswith(INBOX_COMMENTS))
        self.assertEqual(pages, [2, 3])

    def test_a_long_list_is_capped_and_the_rest_come_next_sweep(self):
        gh = FakeGH()
        many = [human_merge(number=300 + n) for n in range(switch.MAX_INBOX_LINES + 5)]
        self.assertEqual(make(gh).inbox(many), switch.MAX_INBOX_LINES)
        body = gh.posts[0][1]
        self.assertIn("- (5 more next sweep)", body)
        self.assertEqual(len(marker_keys(body)), switch.MAX_INBOX_LINES)
        self.assertEqual(make(gh).inbox(many), 5)
        self.assertEqual(len(marker_keys(gh.posts[1][1])), switch.MAX_INBOX_LINES + 5)
        self.assertEqual(make(gh).inbox(many), 0)

    def test_dry_run_prints_the_comment_and_posts_nothing(self):
        fake = FakeGH()
        sent = []

        def run(method, path, body=None, accept="", binary=False):
            if method != "GET":
                sent.append((method, path))
                raise AssertionError(f"a dry run sent {method} {path}")
            return fake.route(path)

        gh = switch.GitHub("IssisX/ScraperX", True, log=fake.log.append)
        gh._run = run
        sw = make(gh)
        self.assertEqual(sw.inbox([human_merge()]), 1)
        self.assertEqual(sent, [])
        self.assertEqual(fake.inbox, [])
        printed = [l for l in sw.lines if l.startswith("Switchboard (inbox):")]
        self.assertEqual(len(printed), 1)
        self.assertIn("MERGE-BY-HAND [#256]", printed[0])
        self.assertTrue(any(l.startswith("DRY  POST repos/IssisX/ScraperX/issues/282/comments") for l in fake.log))


class Untrusted(unittest.TestCase):
    def test_at_signs_are_stripped_and_backticks_cannot_break_out(self):
        evil = "claude/integrate-j9`@IssisX`<b>"
        gh = FakeGH(runs=[run(40, evil, OLD_HEAD, hours=2)], jobs={40: RED_JOBS}, heads={evil: OLD_HEAD})
        note = "merge not done: ping @everyone <img src=x>"
        s = steered(ref="fb-bot/integrate-@cory`x", verdicts=[land()], merge_note=note,
                    proof_done_at=ago(2))
        s.decision = rules.MergeDecision("MERGE", ("proof green",), "Codex")
        self.assertEqual(make(gh).inbox([s]), 2)
        body = gh.posts[0][1]
        self.assertNotIn("@", body)
        self.assertIn("`claude/integrate-j9'IssisX'b`", body)
        self.assertIn("`fb-bot/integrate-cory'x`", body)
        self.assertIn("ping everyone &lt;img src=x&gt;", body)
        self.assertEqual(len(switch.INBOX_KEYS.findall(body)), 1)

    def test_untrusted_drops_control_characters_and_bounds_the_text(self):
        self.assertEqual(switch.untrusted("a\nb\tc@d<e>f"), "`abcdef`")
        self.assertEqual(switch.untrusted("@@@"), "`-`")
        self.assertEqual(len(switch.untrusted("x" * 500)), 82)

    def test_a_forged_marker_in_a_branch_name_is_no_marker(self):
        name = "claude/integrate-x\n<!-- switchboard:inbox keys=deadbeefdeadbeef -->"
        line = switch.InboxItem("RED-BRANCH", name, HEAD, f"RED-BRANCH {switch.untrusted(name)}")
        body = switch.inbox_body([line], 0, [line.key], "1:00 PM Central")
        self.assertEqual(marker_keys(body), [line.key])
        self.assertEqual(len(switch.INBOX_KEYS.findall(body)), 1)


class Kinds(unittest.TestCase):
    def kinds(self, gh, *s):
        return [(it.kind, it.target) for it in items_of(gh, *s)]

    def test_merge_by_hand_is_listed_at_once(self):
        self.assertEqual(self.kinds(FakeGH(), human_merge()), [("MERGE-BY-HAND", "#256")])
        never = steered(verdicts=[land(hours=0.05)], never=True, block=True, proof_done_at=ago(0.05))
        self.assertEqual(never.decision.action, "WAIT")
        self.assertEqual(self.kinds(FakeGH(), never), [("MERGE-BY-HAND", "#256")])
        named = steered(ref="claude/integrate-j9", verdicts=[land(hours=0.05)], block=True, proof_done_at=ago(0.05))
        self.assertEqual(self.kinds(FakeGH(), named), [("MERGE-BY-HAND", "#256")])
        protected = steered(ref="claude/integrate-protected-j9", verdicts=[land()], block=True)
        self.assertEqual(protected.decision.action, "MERGE")
        self.assertEqual(self.kinds(FakeGH(), protected), [])   # merged by the switch: no item

    def test_ready_and_still_open_a_sweep_later(self):
        conflict = steered(verdicts=[land(hours=1)], mergeable=False, proof_done_at=ago(1))
        (it,) = items_of(FakeGH(), conflict)
        self.assertEqual(it.kind, "NOT-MERGED")
        self.assertIn("still open a sweep later: merge conflict with the base", it.line)
        fresh = steered(verdicts=[land(hours=0.1)], mergeable=False, proof_done_at=ago(1))
        self.assertEqual(self.kinds(FakeGH(), fresh), [])   # the LAND came 6 minutes ago: the next sweep may merge it
        refused = steered(verdicts=[land()], merge_note="merge not done: Base branch was modified")
        self.assertIn("Base branch was modified", items_of(FakeGH(), refused)[0].line)
        merged = steered(verdicts=[land()])
        self.assertEqual(merged.decision.action, "MERGE")
        self.assertEqual(self.kinds(FakeGH(), merged), [])

    def test_fix_ci_twice_in_a_row_on_the_same_job(self):
        ref = "fb-bot/integrate-j044l"
        runs = [run(7, ref, HEAD, hours=0.2), run(6, ref, OLD_HEAD, hours=1)]
        s = steered(ref=ref, proof="red: native-tests", committed=ago(1), run_id=7)
        self.assertEqual(s.decision.action, "FIX-CI")
        gh = FakeGH(runs=runs, jobs={6: RED_JOBS})
        (it,) = items_of(gh, s)
        self.assertEqual(it.kind, "RED-TWICE")
        self.assertIn("native-tests failed on two proofs in a row ([proof](" + RUN_URL + "))", it.line)
        other = [{"name": "host-build", "status": "completed", "conclusion": "failure"}]
        self.assertEqual(self.kinds(FakeGH(runs=runs, jobs={6: other}), s), [])
        gh = FakeGH(runs=runs, jobs={6: RED_JOBS})   # listed before at this head: the older run is not read again
        sw = make(gh)
        got = sw.inbox_items([s], frozenset({switch.inbox_key("RED-TWICE", "#256", HEAD)}))
        self.assertEqual([i.kind for i in got], ["RED-TWICE"])
        self.assertFalse(any("/actions/runs/6/" in g for g in gh.gets))

    def test_fix_ci_with_no_new_head_for_three_hours(self):
        ref = "fb-bot/integrate-j044l"
        s = steered(ref=ref, proof="red: native-tests", committed=ago(9), run_id=7)
        old = FakeGH(runs=[run(7, ref, HEAD, hours=3.2)])
        (it,) = items_of(old, s)
        self.assertEqual(it.kind, "RED-STUCK")
        self.assertIn("proof red on native-tests, no new head for 3.7 h", it.line)
        self.assertEqual(self.kinds(FakeGH(runs=[run(7, ref, HEAD, hours=1)]), s), [])   # pushed 1.5 h ago
        self.assertEqual(self.kinds(FakeGH(), s), [("RED-STUCK", "#256")])   # no run in the window: the commit's date

    def test_an_open_finding_on_a_claude_pr(self):
        mine = steered(ref="claude/integrate-j048-no-run-hurt", verdicts=[fix()])
        (it,) = items_of(FakeGH(), mine)
        self.assertEqual(it.kind, "FIX")
        self.assertIn(": open FIX from Codex (review)", it.line)
        self.assertEqual(self.kinds(FakeGH(), steered(ref="fb-bot/integrate-j042", verdicts=[fix()])), [])

    def test_a_red_integrate_branch_with_no_pr_for_an_hour(self):
        name = "fb-bot/integrate-j044c-wreck"
        gh = FakeGH(runs=[run(40, name, HEAD, hours=2)], jobs={40: RED_JOBS}, heads={name: HEAD})
        (it,) = items_of(gh)
        self.assertEqual((it.kind, it.target), ("RED-BRANCH", name))
        self.assertIn("RED-BRANCH `fb-bot/integrate-j044c-wreck` head 1a2b3c4d: proof red on native-tests, "
                      "no PR for 2.0 h ([proof](https://github.com/IssisX/ScraperX/actions/runs/40))", it.line)
        mine = "claude/integrate-j051-x"
        self.assertEqual(self.kinds(FakeGH(runs=[run(41, mine, HEAD, 2)], jobs={41: RED_JOBS}, heads={mine: HEAD})),
                         [("RED-BRANCH", mine)])
        with_pr = FakeGH(runs=gh.runs, jobs=gh.jobs, heads=gh.heads, open_prs=[{"number": 9, "head": {"ref": name}}])
        self.assertEqual(self.kinds(with_pr), [])
        recent = FakeGH(runs=[run(40, name, HEAD, hours=0.5)], jobs={40: RED_JOBS}, heads={name: HEAD})
        self.assertEqual(self.kinds(recent), [])
        self.assertFalse(any("/jobs" in g or "/branches/" in g for g in recent.gets))
        self.assertEqual(self.kinds(FakeGH(runs=gh.runs, jobs={40: GREEN_OFF_TRUNK}, heads=gh.heads)), [])
        self.assertEqual(self.kinds(FakeGH(runs=gh.runs, jobs=gh.jobs, heads={name: OTHER_HEAD})), [])   # moved on
        self.assertEqual(self.kinds(FakeGH(runs=gh.runs, jobs=gh.jobs)), [])   # deleted since
        other = "muse/j044c-wreck"
        self.assertEqual(self.kinds(FakeGH(runs=[run(42, other, HEAD, 2)], jobs={42: RED_JOBS}, heads={other: HEAD})), [])

    def test_quiet_as_the_sweep_defines_it(self):
        quiet = steered(ref="fb-bot/integrate-j042", verdicts=[fix()], updated_at=ago(4))
        (it,) = items_of(FakeGH(), quiet)
        self.assertEqual(it.kind, "QUIET")
        self.assertIn(": no activity on the PR for 4.0 h while it is FIX", it.line)
        self.assertEqual(self.kinds(FakeGH(), steered(ref="fb-bot/integrate-j042", verdicts=[fix()], updated_at=ago(1))), [])
        draft = steered(draft=True, proof="red: native-tests", updated_at=ago(5))
        self.assertEqual(draft.decision.action, "SKIP")
        self.assertEqual(self.kinds(FakeGH(), draft), [("QUIET", "#256")])
        none = steered(proof="none", updated_at=ago(5), proof_done_at="", run_id=0)
        self.assertIn("while it is DISPATCH", items_of(FakeGH(), none)[0].line)
        stuck = steered(proof="red: native-tests", committed=ago(6), updated_at=ago(5))
        self.assertEqual(self.kinds(FakeGH(), stuck), [("RED-STUCK", "#256")])   # QUIET only when nothing else is


    def test_no_inbox_line_reads_as_a_verdict(self):
        """The switchboard workflow runs on a PR comment holding `: LAND`, `: FIX`, `: NOTE` or `: BLOCK`."""
        ref, name = "fb-bot/integrate-j044l", "fb-bot/integrate-j044c-wreck"
        gh = FakeGH(runs=[run(7, ref, HEAD, hours=3.2), run(6, ref, OLD_HEAD, hours=4), run(40, name, HEAD, 2)],
                    jobs={6: RED_JOBS, 40: RED_JOBS}, heads={name: HEAD})
        every = [human_merge(number=1), steered(number=2, verdicts=[land()], mergeable=False),
                 steered(number=3, ref="claude/integrate-j9", verdicts=[fix()]),
                 steered(number=4, ref=ref, proof="red: native-tests", run_id=7),
                 steered(number=5, ref="fb-bot/integrate-j042", verdicts=[fix()], updated_at=ago(4)),
                 steered(number=6, draft=True, proof="red: native-tests", updated_at=ago(5))]
        sw = make(gh)
        sw.auto_pr_failure("fb-bot/integrate-sense-near-followup", HEAD, "J-047", REFUSED, refused=True)
        self.assertEqual(sw.inbox(every), 9)
        body = gh.posts[0][1]
        for kind in switch.INBOX_ORDER:
            self.assertIn(f"- {kind} ", body)
        for word in ("LAND", "FIX", "NOTE", "BLOCK"):
            self.assertNotIn(f": {word}", body)


class AutoPrFailed(unittest.TestCase):
    """An auto-open failure reaches the inbox (J-051, issue #283), not only the steer log."""
    NAME, LATER = "fb-bot/integrate-sense-near-followup", "fb-bot/integrate-windup-j042"

    def finished(self, fail_write=""):
        """Two finished branches (green, a Job line, no PR); the auto-open reaches NAME first."""
        gh, head = test_auto_pr.green_setup(name=self.NAME, job="J-047")
        later = test_auto_pr.sha(2)
        gh.branches.append(test_auto_pr.branch(self.LATER, 2))
        gh.commits[later] = "Budget the windup lift\n\nJob: J-042"
        gh.runs[later] = [test_auto_pr.green_run(later)]
        gh.fail_write = fail_write
        return gh, head, later

    def sweep(self, auto, box) -> switch.Switch:
        """A steer sweep's tail: the auto-open, then the inbox. Each fake answers its own part's calls."""
        sw = test_auto_pr.make(auto)
        sw.auto_open_prs()
        sw.gh = box
        sw.inbox([])
        return sw

    def test_githubs_real_refusal_is_one_item_and_stops_the_auto_open(self):
        auto, head, _ = self.finished(REFUSED)
        box = FakeGH()
        sw = self.sweep(auto, box)
        self.assertEqual(len(auto.writes), 1)   # the later branch was never tried
        self.assertTrue(any("NOTE  GitHub refused" in l and "not permitted to create or approve pull requests" in l
                            and "repo setting" in l for l in sw.lines))
        self.assertFalse(any(l.startswith("ERROR") for l in sw.lines))
        (number, body), = box.posts
        self.assertEqual(number, switch.INBOX_PR)
        self.assertIn(f"- AUTO-PR-FAILED `{self.NAME}` head {head[:8]}, J-047: GitHub refused to open the PR "
                      f"(`{REFUSED}`). Cory turns on Settings → Actions → General → \"Allow GitHub Actions to create "
                      f"and approve pull requests\"; until then the integrator opens the PR by hand. "
                      f"This sweep's auto-open stopped here.", body)
        self.assertEqual(body.count("- AUTO-PR-FAILED "), 1)
        self.assertEqual(marker_keys(body), [switch.inbox_key("AUTO-PR-FAILED", self.NAME, head)])

    def test_the_same_head_is_not_listed_again_and_a_new_head_is(self):
        auto, head, _ = self.finished(REFUSED)
        box = FakeGH()
        self.sweep(auto, box)
        again = self.sweep(auto, box)   # the next sweep is refused on the same head
        self.assertEqual(len(auto.writes), 2)
        self.assertEqual(len(box.posts), 1)
        self.assertIn("INBOX       nothing new", again.lines)
        new = test_auto_pr.sha(3)
        auto.branches[0] = test_auto_pr.branch(self.NAME, 3)
        auto.commits[new], auto.runs[new] = auto.commits[head], [test_auto_pr.green_run(new)]
        self.sweep(auto, box)
        self.assertEqual(len(box.posts), 2)
        self.assertIn(f"`{self.NAME}` head {new[:8]}, J-047: GitHub refused", box.posts[1][1])
        self.assertEqual(marker_keys(box.posts[1][1]), [switch.inbox_key("AUTO-PR-FAILED", self.NAME, head),
                                                        switch.inbox_key("AUTO-PR-FAILED", self.NAME, new)])

    def test_the_old_refusal_wording_still_stops_the_auto_open(self):
        auto, head, _ = self.finished("gh: Resource not accessible by integration (HTTP 403): "
                                      "not permitted to create pull requests")
        sw = test_auto_pr.make(auto)
        self.assertEqual(sw.auto_open_prs(), 0)
        self.assertEqual(len(auto.writes), 1)
        (it,) = sw.auto_pr_failed
        self.assertEqual((it.kind, it.target, it.head), ("AUTO-PR-FAILED", self.NAME, head))
        self.assertIn("Allow GitHub Actions to create and approve pull requests", it.line)

    def test_any_other_error_is_an_item_and_the_sweep_goes_on(self):
        auto, head, later = self.finished("gh: Validation Failed (HTTP 422)")
        sw = test_auto_pr.make(auto)
        self.assertEqual(sw.auto_open_prs(), 0)
        self.assertEqual(len(auto.writes), 2)   # not a refusal: the later branch is still tried
        self.assertEqual([(it.target, it.head) for it in sw.auto_pr_failed], [(self.NAME, head), (self.LATER, later)])
        self.assertEqual(sw.auto_pr_failed[0].line,
                         f"AUTO-PR-FAILED `{self.NAME}` head {head[:8]}, J-047: the auto-open failed "
                         f"(`gh: Validation Failed (HTTP 422)`); the next sweep tries again.")
        auto, head, _ = self.finished()
        del auto.commits[head]   # the commit read fails: a KeyError, before the Job line is known
        sw = test_auto_pr.make(auto)
        self.assertEqual(sw.auto_open_prs(), 1)
        sw.gh = FakeGH()
        (it,) = sw.inbox_items([], frozenset())
        self.assertEqual((it.kind, it.target), ("AUTO-PR-FAILED", self.NAME))
        self.assertTrue(it.line.startswith(f"AUTO-PR-FAILED `{self.NAME}` head {head[:8]}: the auto-open failed "
                                           f"(`KeyError: "))

    def test_a_reply_with_no_pr_number_is_an_item_and_not_counted(self):
        auto, head, _ = self.finished()
        auto.write = lambda method, path, body=None, what="": {"message": "Something odd"}
        sw = test_auto_pr.make(auto)
        self.assertEqual(sw.auto_open_prs(), 0)
        self.assertEqual(len(sw.auto_pr_failed), 2)
        self.assertIn("the auto-open failed (`PR not opened: Something odd`)", sw.auto_pr_failed[0].line)

    def test_outside_text_cannot_mention_anyone_or_read_as_a_verdict(self):
        auto, head, _ = self.finished("gh: @IssisX said: LAND it; note: Fix `x` <b>: block (HTTP 500)")
        auto.branches[0] = test_auto_pr.branch("fb-bot/integrate-@cory`x", 1)
        box = FakeGH()
        self.sweep(auto, box)
        body = box.posts[0][1]
        self.assertNotIn("@", body)
        self.assertIn("`fb-bot/integrate-cory'x`", body)
        for word in ("land", "fix", "note", "block"):
            self.assertNotIn(f": {word}", body.lower())
        self.assertIn("`gh: IssisX said - LAND it; note - Fix 'x' b - block (HTTP 500)`", body)


class InboxPrExcluded(unittest.TestCase):
    INBOX = {"number": 282, "head": {"ref": "claude/integrator-inbox", "sha": HEAD}}
    OTHER = {"number": 256, "head": {"ref": "fb-bot/integrate-j042", "sha": HEAD}}

    def test_the_switch_never_acts_on_the_inbox_pr(self):
        gh = FakeGH(open_prs=[self.INBOX, self.OTHER,
                              {"number": 290, "head": {"ref": "claude/integrator-inbox", "sha": OTHER_HEAD}}])
        sw = make(gh)
        self.assertEqual([p["number"] for p in sw.target_prs(0, "")], [256])
        self.assertEqual(sw.target_prs(282, ""), [])   # a comment on #282 wakes nothing
        self.assertEqual(sw.pr_inbox_items(steered(number=282, ref="claude/integrator-inbox", verdicts=[land()],
                                                   workflows=True), [], frozenset()), [])

    def test_a_supersedes_line_never_closes_the_inbox(self):
        closed = []
        gh = types.SimpleNamespace(repo="IssisX/ScraperX", get=lambda path: {"state": "open"},
                                   comment=lambda number, body, what="": None,
                                   write=lambda method, path, body=None, what="": closed.append(path))
        me = types.SimpleNamespace(gh=gh, out=lambda line: None)
        switch.Switch.close_superseded(me, types.SimpleNamespace(number=300, draft=False),
                                       {"body": "Supersedes #282 and #43"})
        self.assertEqual(closed, ["repos/IssisX/ScraperX/pulls/43"])

    def test_no_auto_pr_and_no_branch_item_for_the_inbox_branch(self):
        gh, _ = test_auto_pr.green_setup(name=switch.INBOX_BRANCH, job="J-051")
        self.assertEqual(test_auto_pr.make(gh).auto_open_prs(), 0)
        self.assertEqual(gh.writes, [])
        red = FakeGH(runs=[run(50, switch.INBOX_BRANCH, HEAD, 3)], jobs={50: RED_JOBS}, heads={switch.INBOX_BRANCH: HEAD})
        self.assertEqual(items_of(red), [])


if __name__ == "__main__":
    unittest.main()
