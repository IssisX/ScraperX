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


def comment(cid: int, number: int, body: str, hours: float = 1.0, login: str = "IssisX") -> dict:
    """One issue comment, as repos/{repo}/issues/comments returns it."""
    return {"id": cid, "issue_url": f"https://api.github.com/repos/IssisX/ScraperX/issues/{number}",
            "html_url": f"https://github.com/IssisX/ScraperX/issues/{number}#issuecomment-{cid}",
            "body": body, "user": {"login": login}, "created_at": ago(hours), "updated_at": ago(hours)}


def steered(number=256, ref="fb-bot/integrate-j042-windup", head=HEAD, proof="green", verdicts=(), workflows=False,
            draft=False, mergeable=True, block=False, never=False, committed=None, job="J-042", proof_done_at=None,
            updated_at=None, run_id=7, merge_note="", comments=()) -> switch.Steered:
    """One PR as steer_pr leaves it: the facts, the context and the switch's own decision on them."""
    facts = rules.PrFacts(number, head, ref, rules.TRUNK, draft, mergeable, committed or ago(5), proof,
                          tuple(verdicts), frozenset(), block, workflows, never)
    ctx = {"job": job, "proof_url": RUN_URL, "run_id": run_id, "proof_done_at": proof_done_at or ago(0.5),
           "updated_at": updated_at or ago(0.1), "comments": list(comments)}
    return switch.Steered(facts, ctx, rules.merge_decision(facts, TEAM.vendor_of_branch(ref), TEAM), merge_note)


def human_merge(head=HEAD, number=256) -> switch.Steered:
    return steered(number=number, head=head, verdicts=[land(head, hours=0.05)], workflows=True, proof_done_at=ago(0.05))


class FakeGH:
    """The GitHub calls the inbox makes: reads from tables, comments recorded (only logged in a dry run)."""

    repo = "IssisX/ScraperX"

    def __init__(self, runs=(), jobs=None, open_prs=(), heads=None, inbox=(), dry_run=False,
                 comments=(), open_issues=()):
        self.runs, self.jobs, self.open_prs = list(runs), dict(jobs or {}), list(open_prs)
        self.heads = dict(heads or {})
        self.inbox = list(inbox)
        self.comments, self.open_issues = list(comments), list(open_issues)
        self.by_issue: dict[int, list] = {}
        # the issues labeled `board`, open and closed (Switch.board_issue, old_boards); an entry is a number or an item
        self.boards, self.closed_boards = [switch.BOARD_ISSUE], []
        self.dry_run = dry_run
        self.gets, self.posts, self.writes, self.log = [], [], [], []
        self._comment_id = 0   # ids the real API returns on a comment POST
        self.fail_labels = 0   # label POSTs to fail (transient 403/5xx) before succeeding
        self.state: dict = {}   # the sweep state, as ghapi saves it with the cache (11-13: the inbox ring and hold)

    def route(self, path):
        self.gets.append(path)
        base = path.split("?")[0]
        if base == f"repos/{self.repo}/issues/{switch.INBOX_PR}":
            return {"number": switch.INBOX_PR, "comments": len(self.inbox)}
        if base == INBOX_COMMENTS:
            page = int(re.search(r"[?&]page=(\d+)", path).group(1))
            return self.inbox[(page - 1) * 100: page * 100]
        if base == f"repos/{self.repo}/issues/comments":
            since = re.search(r"[?&]since=([^&]+)", path)
            since = since.group(1) if since else ""
            page = re.search(r"[?&]page=(\d+)", path)
            page = int(page.group(1)) if page else 1
            fresh = [c for c in self.comments if (c.get("updated_at") or c.get("created_at") or "") >= since]
            if "direction=desc" in path:
                fresh = sorted(fresh, key=lambda c: c.get("created_at") or "", reverse=True)
            return fresh[(page - 1) * 100: page * 100]
        if base == f"repos/{self.repo}/issues" and "labels=board" in path:
            listed = self.closed_boards if "state=closed" in path else self.boards
            return [dict(b) if isinstance(b, dict) else {"number": b} for b in listed]
        if base == f"repos/{self.repo}/issues":
            page = re.search(r"[?&]page=(\d+)", path)
            page = int(page.group(1)) if page else 1
            return [{"number": n} for n in self.open_issues[(page - 1) * 100: page * 100]]
        m = re.fullmatch(r"repos/[^/]+/[^/]+/issues/(\d+)/comments", base)
        if m:
            return list(self.by_issue.get(int(m.group(1)), []))
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
        if path.split("?")[0] != f"repos/{self.repo}/issues/comments":
            got = self.route(path)
            return list(got[key] if key else got)
        items, sep = [], "&" if "?" in path else "?"
        for page in range(1, switch.ghapi.MAX_PAGES + 1):   # as ghapi.paged: stops at the cap
            batch = self.route(f"{path}{sep}page={page}")
            items.extend(batch)
            if len(batch) < 100:
                break
        return items

    def comment(self, number, body, what=""):
        if self.dry_run:
            self.log.append(f"DRY  POST repos/{self.repo}/issues/{number}/comments  {what}")
            return None
        self._comment_id += 1
        self.posts.append((number, body))
        entry = {"id": self._comment_id, "body": body, "user": {"login": BOT}, "created_at": NOW.isoformat()}
        if number == switch.INBOX_PR:
            self.inbox.append(entry)
        else:
            self.by_issue.setdefault(number, []).append(entry)
        return {"id": self._comment_id}

    def write(self, method, path, body=None, what=""):
        if method == "POST" and path.endswith("/labels") and self.fail_labels > 0:
            self.fail_labels -= 1
            raise switch.GitHubError("POST", path, "gh: Label write failed (HTTP 403)")
        if method == "PATCH" and "/issues/comments/" in path:
            cid = int(path.rsplit("/", 1)[1])
            for entries in list(self.by_issue.values()) + [self.inbox]:
                for entry in entries:
                    if entry.get("id") == cid:
                        entry["body"] = body["body"]
                        self.writes.append((method, path, body))
                        return {}
            raise switch.GitHubError("PATCH", path, "gh: Not Found (HTTP 404)")
        self.writes.append((method, path, body))
        return {}


def make(gh, now=NOW):
    sw = switch.Switch.__new__(switch.Switch)
    sw.gh, sw.now, sw.team, sw.lines, sw._load = gh, now, TEAM, [], None
    sw.out = sw.lines.append
    return sw


def marker_keys(body: str) -> list[str]:
    """The keys in an inbox comment's old marker (comments carried them until 11-13)."""
    found = switch.INBOX_KEYS.findall(body)
    return found[-1].split(",") if found else []


def ring(gh) -> list[str]:
    """11-13: the keys listed so far, oldest first, as the sweep state keeps them (no longer in the comment)."""
    return list((gh.state.get("inbox") or {}).get("keys") or [])


def later(gh, minutes: float = 31):
    """The switch of a sweep `minutes` after NOW: past INBOX_HOLD, so the integrator's own work posts again."""
    return make(gh, NOW + dt.timedelta(minutes=minutes))


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
        self.assertEqual(ring(gh), [switch.inbox_key("MERGE-BY-HAND", "#256", HEAD)])
        self.assertNotIn("switchboard:inbox", body)   # 11-13: the keys stay out of the comment the relay cuts
        self.assertEqual(marker_keys(body), [])
        sw = later(gh)   # the next sweep, same head: nothing new
        self.assertEqual(sw.inbox([s]), 0)
        self.assertEqual(len(gh.posts), 1)
        self.assertIn("INBOX       nothing new", sw.lines)

    def test_a_key_in_an_earlier_comment_is_not_posted_again(self):
        key = switch.inbox_key("MERGE-BY-HAND", "#256", HEAD)
        gh = FakeGH(inbox=[{"body": "Switchboard (inbox):\n\n- old\n\n" + switch.INBOX_MARK.format(keys=key),
                            "user": {"login": BOT}}])
        self.assertEqual(make(gh).inbox([human_merge()]), 0)
        self.assertEqual(gh.posts, [])

    def test_a_new_head_is_posted_again_and_the_ring_carries_the_old_key(self):
        gh = FakeGH()
        make(gh).inbox([human_merge(HEAD)])
        self.assertEqual(later(gh).inbox([human_merge(OTHER_HEAD)]), 1)
        self.assertEqual(len(gh.posts), 2)
        body = gh.posts[1][1]
        self.assertIn("head ffffffff", body)
        self.assertNotIn("head 1a2b3c4d", body)
        self.assertEqual(ring(gh), [switch.inbox_key("MERGE-BY-HAND", "#256", HEAD),
                                    switch.inbox_key("MERGE-BY-HAND", "#256", OTHER_HEAD)])

    def test_only_the_new_items_are_listed(self):
        gh = FakeGH()
        make(gh).inbox([human_merge(number=256)])
        self.assertEqual(later(gh).inbox([human_merge(number=256), human_merge(number=260)]), 1)
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
        # 35 short asks: MAX_INBOX_LINES go, the 5 left are asks too, so they go the very next sweep (no hold)
        gh = FakeGH(comments=[comment(900 + n, 292, f"To Claude: ask {n}") for n in range(switch.MAX_INBOX_LINES + 5)],
                    open_issues=[292])
        self.assertEqual(make(gh).inbox([]), switch.MAX_INBOX_LINES)
        body = gh.posts[0][1]
        self.assertIn("- (5 more held for the next post)", body)
        self.assertEqual(len(ring(gh)), switch.MAX_INBOX_LINES)
        self.assertEqual(make(gh).inbox([]), 5)
        self.assertEqual(len(ring(gh)), switch.MAX_INBOX_LINES + 5)
        self.assertEqual(make(gh).inbox([]), 0)

    def test_a_comment_stays_under_the_relays_cut_and_nothing_is_lost_to_it(self):
        # 11-13: the relay to the session cuts a comment near 4,000 characters; 35 long lines go over several posts
        gh = FakeGH()
        many = [human_merge(number=300 + n) for n in range(switch.MAX_INBOX_LINES + 5)]
        posted, sweeps = 0, 0
        while posted < len(many):
            got = later(gh, 31 * sweeps).inbox(many)
            self.assertGreater(got, 0, "every due post carries at least one item")
            posted, sweeps = posted + got, sweeps + 1
        self.assertEqual(posted, len(many))
        self.assertGreater(len(gh.posts), 1)
        for _, body in gh.posts:
            self.assertLessEqual(len(body), switch.INBOX_MAX_CHARS)
        listed = "".join(b for _, b in gh.posts)
        for s in many:
            self.assertEqual(listed.count(f"[#{s.facts.number}]("), 1, s.facts.number)
        self.assertEqual(sorted(ring(gh)), sorted(switch.inbox_key("MERGE-BY-HAND", f"#{s.facts.number}", HEAD)
                                                  for s in many))
        self.assertEqual(later(gh, 31 * sweeps).inbox(many), 0)

    def test_one_item_too_long_alone_is_cut_to_fit_and_still_listed(self):
        tests = tuple(f"native-tests: FAIL: case {n} " + "x" * 200 for n in range(40))
        it = switch.InboxItem("RED-BRANCH", "claude/integrate-j9", HEAD, "RED-BRANCH `claude/integrate-j9` head "
                              "1a2b3c4d: proof red on native-tests ([proof](u))", tests=tests)
        shown, carried, body = switch.fit_inbox([it], [], "1:00 PM Central")
        self.assertEqual(len(shown), 1)
        self.assertLessEqual(len(body), switch.INBOX_MAX_CHARS)
        self.assertIn("- RED-BRANCH `claude/integrate-j9` head 1a2b3c4d: proof red on native-tests", body)
        self.assertIn("more failing lines on its proof)", body)

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
        self.assertEqual(len(switch.INBOX_KEYS.findall(body)), 0)   # 11-13: no marker at all

    def test_untrusted_drops_control_characters_and_bounds_the_text(self):
        self.assertEqual(switch.untrusted("a\nb\tc@d<e>f"), "`abcdef`")
        self.assertEqual(switch.untrusted("@@@"), "`-`")
        self.assertEqual(len(switch.untrusted("x" * 500)), 82)

    def test_a_forged_marker_in_a_branch_name_is_no_marker(self):
        name = "claude/integrate-x\n<!-- switchboard:inbox keys=deadbeefdeadbeef -->"
        line = switch.InboxItem("RED-BRANCH", name, HEAD, f"RED-BRANCH {switch.untrusted(name)}")
        body = switch.inbox_body([line], 0, "1:00 PM Central")
        self.assertEqual(marker_keys(body), [])
        self.assertEqual(len(switch.INBOX_KEYS.findall(body)), 0)
        gh = FakeGH(inbox=[{"body": body, "user": {"login": BOT}}])   # and read back, it lists no key
        self.assertEqual(make(gh).inbox_reported(), [])


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
        got = muse_of(old, s)
        self.assertEqual([(i.kind, i.target) for i in got], [("RED", "#256"), ("RED-STUCK", "#256")])
        self.assertIn("proof red on native-tests, no new head for 3.7 h", got[1].line)
        self.assertEqual(items_of(old, s), [])   # a stuck red on her own branch is Muse's, not the integrator's
        self.assertEqual(self.kinds(FakeGH(runs=[run(7, ref, HEAD, hours=1)]), s), [])   # pushed 1.5 h ago
        self.assertEqual([(i.kind, i.target) for i in muse_of(FakeGH(), s)],
                         [("RED", "#256"), ("RED-STUCK", "#256")])   # no run in the window: the commit's date

    def test_an_open_finding_on_a_claude_pr(self):
        mine = steered(ref="claude/integrate-j048-no-run-hurt", verdicts=[fix()])
        (it,) = items_of(FakeGH(), mine)
        self.assertEqual(it.kind, "FIX")
        self.assertIn(": open FIX from Codex (review)", it.line)
        self.assertEqual(self.kinds(FakeGH(), steered(ref="fb-bot/integrate-j042", verdicts=[fix()])), [])

    def test_a_red_integrate_branch_with_no_pr_for_an_hour(self):
        name = "fb-bot/integrate-j044c-wreck"
        gh = FakeGH(runs=[run(40, name, HEAD, hours=2)], jobs={40: RED_JOBS}, heads={name: HEAD})
        (it,) = muse_of(gh)
        self.assertEqual((it.kind, it.target), ("RED-BRANCH", name))
        self.assertIn("RED-BRANCH `fb-bot/integrate-j044c-wreck` head 1a2b3c4d: proof red on native-tests, "
                      "no PR for 2.0 h ([proof](https://github.com/IssisX/ScraperX/actions/runs/40))", it.line)
        self.assertEqual(items_of(gh), [])
        mine = "claude/integrate-j051-x"
        self.assertEqual(self.kinds(FakeGH(runs=[run(41, mine, HEAD, 2)], jobs={41: RED_JOBS}, heads={mine: HEAD})),
                         [("RED-BRANCH", mine)])
        with_pr = FakeGH(runs=gh.runs, jobs=gh.jobs, heads=gh.heads, open_prs=[{"number": 9, "head": {"ref": name}}])
        self.assertEqual(self.kinds(with_pr), [])
        self.assertEqual(muse_of(with_pr), [])
        recent = FakeGH(runs=[run(40, name, HEAD, hours=0.5)], jobs={40: RED_JOBS}, heads={name: HEAD})
        self.assertEqual(self.kinds(recent), [])
        self.assertFalse(any("/jobs" in g or "/branches/" in g for g in recent.gets))
        self.assertEqual(self.kinds(FakeGH(runs=gh.runs, jobs={40: GREEN_OFF_TRUNK}, heads=gh.heads)), [])
        self.assertEqual(self.kinds(FakeGH(runs=gh.runs, jobs=gh.jobs, heads={name: OTHER_HEAD})), [])   # moved on
        self.assertEqual(self.kinds(FakeGH(runs=gh.runs, jobs=gh.jobs)), [])   # deleted since
        other = "muse/j044c-wreck"
        self.assertEqual(self.kinds(FakeGH(runs=[run(42, other, HEAD, 2)], jobs={42: RED_JOBS}, heads={other: HEAD})), [])

    def test_quiet_as_the_sweep_defines_it(self):
        quiet = steered(proof="none", updated_at=ago(5), proof_done_at="", run_id=0)
        (it,) = muse_of(FakeGH(), quiet)
        self.assertEqual((it.kind, it.target), ("QUIET", "#256"))
        self.assertIn(": no activity on the PR for 5.0 h while it is DISPATCH", it.line)
        self.assertEqual(items_of(FakeGH(), quiet), [])
        self.assertEqual(self.kinds(FakeGH(), steered(ref="fb-bot/integrate-j042", verdicts=[fix()], updated_at=ago(1))), [])
        draft = steered(draft=True, proof="red: native-tests", updated_at=ago(5))
        self.assertEqual(draft.decision.action, "SKIP")
        self.assertEqual([(i.kind, i.target) for i in muse_of(FakeGH(), draft)], [("QUIET", "#256")])
        stuck = steered(proof="red: native-tests", committed=ago(6), updated_at=ago(5))
        self.assertEqual([(i.kind, i.target) for i in muse_of(FakeGH(), stuck)],
                         [("RED", "#256"), ("RED-STUCK", "#256")])   # QUIET only when nothing else is


    def test_no_inbox_line_reads_as_a_verdict(self):
        """The switchboard workflow runs on a PR comment holding `: LAND`, `: FIX`, `: NOTE` or `: BLOCK`."""
        ref, name = "fb-bot/integrate-j044l", "fb-bot/integrate-j044c-wreck"
        gh = FakeGH(runs=[run(7, ref, HEAD, hours=3.2), run(6, ref, OLD_HEAD, hours=4), run(40, name, HEAD, 2)],
                    jobs={6: RED_JOBS, 40: RED_JOBS}, heads={name: HEAD},
                    comments=[comment(42424243, 291, "To Claude: review of #291 @ da8e64c6: LAND\n\nPlease rule.")],
                    open_issues=[291])
        every = [human_merge(number=1), steered(number=2, verdicts=[land()], mergeable=False),
                 steered(number=3, ref="claude/integrate-j9", verdicts=[fix()]),
                 steered(number=4, ref=ref, proof="red: native-tests", run_id=7),
                 steered(number=5, ref="fb-bot/integrate-j042", verdicts=[fix()], updated_at=ago(4)),
                 steered(number=6, draft=True, proof="red: native-tests", updated_at=ago(5))]
        sw = make(gh)
        sw.auto_pr_failure("fb-bot/integrate-sense-near-followup", HEAD, "J-047", REFUSED, refused=True)
        self.assertEqual(sw.inbox(every), 6)
        self.assertEqual(sw.muse(every), 5)
        for _, body in gh.posts:
            for word in ("LAND", "FIX", "NOTE", "BLOCK"):
                self.assertNotIn(f": {word}", body)
        inbox_body = gh.posts[0][1]
        for kind in ("MERGE-BY-HAND", "TO-CLAUDE", "NOT-MERGED", "AUTO-PR-FAILED", "FIX", "RED-TWICE"):
            self.assertIn(f"- {kind} ", inbox_body)
        for kind in ("RED", "RED-STUCK", "RED-BRANCH", "QUIET"):
            self.assertNotIn(f"- {kind} ", inbox_body)


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

    def sweep(self, auto, box, minutes: float = 0) -> switch.Switch:
        """A steer sweep's tail: the auto-open, then the inbox. Each fake answers its own part's calls."""
        sw = test_auto_pr.make(auto)
        sw.now += dt.timedelta(minutes=minutes)
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
        self.assertEqual(ring(box), [switch.inbox_key("AUTO-PR-FAILED", self.NAME, head)])

    def test_the_same_head_is_not_listed_again_and_a_new_head_is(self):
        auto, head, _ = self.finished(REFUSED)
        box = FakeGH()
        self.sweep(auto, box)
        again = self.sweep(auto, box, 31)   # the next sweep is refused on the same head
        self.assertEqual(len(auto.writes), 2)
        self.assertEqual(len(box.posts), 1)
        self.assertIn("INBOX       nothing new", again.lines)
        new = test_auto_pr.sha(3)
        auto.branches[0] = test_auto_pr.branch(self.NAME, 3)
        auto.commits[new], auto.runs[new] = auto.commits[head], [test_auto_pr.green_run(new)]
        self.sweep(auto, box, 62)
        self.assertEqual(len(box.posts), 2)
        self.assertIn(f"`{self.NAME}` head {new[:8]}, J-047: GitHub refused", box.posts[1][1])
        self.assertEqual(ring(box), [switch.inbox_key("AUTO-PR-FAILED", self.NAME, head),
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
        me = types.SimpleNamespace(gh=gh, out=lambda line: None,   # J-072: both open, in the open-PR listing
                                   listing=lambda path: [{"number": 282}, {"number": 43}, {"number": 300}])
        switch.Switch.close_superseded(me, types.SimpleNamespace(number=300, draft=False),
                                       {"body": "Supersedes #282 and #43"})
        self.assertEqual(closed, ["repos/IssisX/ScraperX/pulls/43"])

    def test_no_auto_pr_and_no_branch_item_for_the_inbox_branch(self):
        gh, _ = test_auto_pr.green_setup(name=switch.INBOX_BRANCH, job="J-051")
        self.assertEqual(test_auto_pr.make(gh).auto_open_prs(), 0)
        self.assertEqual(gh.writes, [])
        red = FakeGH(runs=[run(50, switch.INBOX_BRANCH, HEAD, 3)], jobs={50: RED_JOBS}, heads={switch.INBOX_BRANCH: HEAD})
        self.assertEqual(items_of(red), [])


def muse_of(gh, *s) -> list[switch.InboxItem]:
    return make(gh).muse_items(list(s), frozenset())


class AskToClaude(unittest.TestCase):
    """An ask to the integrator in a comment wakes it (J-052): one TO-CLAUDE item per comment id."""
    ASK = "**Muse (via email bridge): MUSE_TO_CLAUDE: J-001 To Claude: hands first?**\n\nHands draw first."
    CID = 42424242

    def gh_with(self, body=ASK, hours=1.0, login="IssisX", number=292, cid=CID):
        return FakeGH(comments=[comment(cid, number, body, hours, login)], open_issues=[number])

    def test_a_bridge_post_with_to_claude_gives_one_item(self):
        (it,) = items_of(self.gh_with())
        self.assertEqual((it.kind, it.target, it.head), ("TO-CLAUDE", "#292", str(self.CID)))
        self.assertEqual(it.key, switch.inbox_key("TO-CLAUDE", "#292", str(self.CID)))

    def test_the_same_comment_in_the_next_sweep_gives_none(self):
        gh = self.gh_with()
        self.assertEqual(make(gh).inbox([]), 1)
        self.assertEqual(make(gh).inbox([]), 0)
        self.assertEqual(len(gh.posts), 1)

    def test_posts_that_are_not_asks_give_none(self):
        cases = [
            (self.ASK, 25.0, "IssisX", 292),
            ("A long ruling\n\nTo Claude: what about this?", 1.0, "IssisX", 292),
            ("<!-- codex review -->\nTo Claude: ...", 1.0, "IssisX", 292),
            (self.ASK, 1.0, rules.BOT_LOGIN, 292),
            (self.ASK + "\n\nGenerated by [Claude Code]", 1.0, "IssisX", 292),
        ]
        for body, hours, login, number in cases:
            self.assertEqual(items_of(self.gh_with(body, hours, login, number)), [], body.split("\n")[0][:60])

    def test_every_bridge_post_gives_one_item(self):
        # 11-10: Muse's "cause 2 needs your ruling" had no "To Claude" in its subject and sat unread all night
        for body in ("**Muse (via email bridge): MUSE_TO_CLAUDE: J-049 leak list**\n\nThe list.",
                     "**Muse (via email bridge): J-044d cause 1 fixed; cause 2 needs your ruling**\n\nWhich?"):
            (it,) = items_of(self.gh_with(body=body))
            self.assertEqual(it.kind, "TO-CLAUDE", body[:60])

    def test_an_ask_past_the_listing_cap_still_gives_one_item(self):
        # 11-10: 620 comments in the window; the oldest-first listing stopped at 500 and the newest asks were lost
        old = [comment(1000 + i, 292, f"chatter {i}", 2.0, "IssisX") for i in range(switch.COMMENT_LISTING_CAP)]
        ask = comment(self.CID, 292, self.ASK, 0.5, "IssisX")
        gh = FakeGH(comments=old + [ask], open_issues=[292])
        (it,) = items_of(gh)
        self.assertEqual((it.kind, it.head), ("TO-CLAUDE", str(self.CID)))

    def test_a_proposal_first_line_gives_one_item(self):
        (it,) = items_of(self.gh_with(body="Proposal: let the switchboard wake Claude on asks\n\nDetails."))
        self.assertEqual(it.kind, "TO-CLAUDE")

    def test_a_closed_thread_gives_none(self):
        gh = FakeGH(comments=[comment(self.CID, 292, self.ASK)], open_issues=[switch.BOARD_ISSUE])
        self.assertEqual(items_of(gh), [])

    def test_the_ledgers_notice_to_claude_gives_one_item(self):
        # J-072: notice 6746048774 on #340 (7 Claude items) never reached the inbox: bot comments were all skipped
        notice = ("To Claude: 7 item(s) past deadline\n\n- x: green, no PR: open one\n\n"
                  "<!-- switchboard:ledger keys=ab12 -->")
        (it,) = items_of(self.gh_with(body=notice, login=rules.BOT_LOGIN, number=340, cid=6746048774))
        self.assertEqual((it.kind, it.target, it.head), ("TO-CLAUDE", "#340", "6746048774"))
        self.assertIn("7 item(s) past deadline", it.line)
        for body in ("To Grok: 1 item(s) past deadline\n\n<!-- switchboard:ledger keys=ab -->",   # not Claude's
                     "To Claude: a bot comment with no ledger marker"):
            self.assertEqual(items_of(self.gh_with(body=body, login=rules.BOT_LOGIN, number=340)), [], body)
        forged = notice   # the marker in a non-bot comment changes nothing: it is read as any ask
        self.assertEqual([i.kind for i in items_of(self.gh_with(body=forged, number=340))], ["TO-CLAUDE"])

    def test_a_codex_ask_is_the_integrators_to_post(self):
        tag = switch.MARK.format(kind="ask-codex", pr=256, head=HEAD)
        marked = comment(5, 256, f"Switchboard: a Codex review of this head is due.\n\n{tag}", login=BOT)
        s = steered(ref="claude/integrate-j042", comments=[marked])
        items = [it for it in items_of(FakeGH(), s) if it.kind == "CODEX-ASK"]
        self.assertEqual(len(items), 1)
        self.assertIn("post codex review with an at-sign in front on #256", items[0].line)
        old = comment(5, 256, switch.MARK.format(kind="ask-codex", pr=256, head=OLD_HEAD), login=BOT)
        self.assertFalse([it for it in items_of(FakeGH(), steered(ref="claude/integrate-j042", comments=[old]))
                          if it.kind == "CODEX-ASK"])   # an ask for an older head is no item


class MuseRouted(unittest.TestCase):
    """Every item goes to its owner (J-052): fb-bot/ items reach Muse by comment + wake:muse."""
    FINDING = "The hold spring is uncharged. " + "x" * 700

    def fix_pr(self, number=256, ref="fb-bot/integrate-j042", head=HEAD, comments=None, **kw):
        c = comment(99, number, "**Codex (review of #%d @ %s): FIX**\n\n%s" % (number, head[:8], self.FINDING),
                    1.0, rules.CODEX_LOGIN)
        return steered(number=number, ref=ref, head=head, verdicts=[fix(head)],
                       comments=[c] if comments is None else comments, **kw)

    def test_a_fix_on_an_fb_bot_pr_reaches_muse_not_the_inbox(self):
        s = self.fix_pr()
        gh = FakeGH()
        self.assertEqual(make(gh).muse([s]), 1)
        (number, posted), = gh.posts
        # Never on the PR: the bridge's Issues-only token can neither clear wake:muse there nor comment (11-09).
        # No job issue here, so the board; the PR and its head stay in the subject line.
        self.assertEqual(number, switch.BOARD_ISSUE)
        self.assertTrue(posted.startswith("CLAUDE_TO_MUSE: J-042 FIX on #256 @ 1a2b3c4d"))
        self.assertIn("The hold spring is uncharged. " + "x" * 500, posted)
        self.assertNotIn("x" * 650, posted)
        self.assertIn("https://github.com/IssisX/ScraperX/issues/256#issuecomment-99", posted)
        key = switch.inbox_key("muse-FIX", "#256", HEAD)
        self.assertEqual(gh.writes[0], ("POST", "repos/IssisX/ScraperX/issues/340/labels", {"labels": ["wake:muse"]}))
        self.assertFalse(any(w[1].startswith("repos/IssisX/ScraperX/issues/256") for w in gh.writes))
        self.assertEqual(gh.writes[1][0], "PATCH")
        self.assertEqual(gh.writes[1][1], "repos/IssisX/ScraperX/issues/comments/1")
        self.assertIn(key, gh.writes[1][2]["body"])
        self.assertNotIn("switchboard:muse keys=", posted)   # posted without its marker; the label comes first
        self.assertIn(key, gh.by_issue[340][0]["body"])       # the marker is edited in after the label lands
        self.assertEqual(make(gh).inbox([s]), 0)
        self.assertEqual(make(gh).muse([self.fix_pr()]), 0)   # the board's marker is read: not sent again
        self.assertEqual(len(gh.posts), 1)
        self.assertEqual(make(gh).muse([self.fix_pr(head=OTHER_HEAD)]), 1)

    def test_a_merge_conflict_on_an_fb_bot_pr_is_mailed_by_the_sweep(self):
        # 11-10 #343: only the reconciler's hourly pass told Muse of a conflict; the 10-minute sweep now does
        s = steered(number=343, ref="fb-bot/integrate-j059-injury-feed", mergeable=False, job="J-059")
        self.assertIn(rules.CONFLICT, s.decision.reasons)
        gh = FakeGH()
        self.assertEqual(make(gh).muse([s]), 1)
        (_, posted), = gh.posts
        self.assertTrue(posted.startswith("CLAUDE_TO_MUSE: J-059 CONFLICT on #343 @ 1a2b3c4d"), posted)
        self.assertIn("merge ScraperX-Claude into fb-bot/integrate-j059-injury-feed and push", posted)
        self.assertIn(switch.inbox_key("muse-CONFLICT", "#343", HEAD), gh.writes[1][2]["body"])
        self.assertEqual(make(gh).muse([steered(number=343, ref="fb-bot/integrate-j059-injury-feed",
                                                mergeable=False, job="J-059")]), 0)   # once per head
        self.assertEqual(make(gh).inbox([s]), 0)   # Muse's, not the integrator's

    def test_an_api_note_on_a_green_head_asks_the_integrator(self):
        # Cory, 11-10: the integrator takes the backup review when the API answers NOTE (#383 waited 2 h on ChatGPT)
        note = rules.Verdict("anthropic", "Claude", "review", "NOTE", HEAD, ago(0.5), BOT, True)
        s = steered(number=383, ref="fb-bot/integrate-protected-j049-pr1-r2", verdicts=[note], job="J-049")
        gh = FakeGH()
        self.assertEqual([i.kind for i in items_of(gh, s)], ["NOTE-REVIEW"])
        self.assertEqual([i.kind for i in muse_of(gh, s)], [])
        # answered: a LAND or FIX on the head ends it
        self.assertEqual(items_of(FakeGH(), steered(number=383, ref="fb-bot/integrate-j049", verdicts=[note, land()],
                                                    job="J-049")), [])

    def test_a_notice_on_a_pr_with_a_job_issue_goes_on_the_job_issue(self):
        s = self.fix_pr()
        s.ctx["issue"] = {"number": 300, "labels": []}
        gh = FakeGH()
        self.assertEqual(make(gh).muse([s]), 1)
        (number, posted), = gh.posts
        self.assertEqual(number, 300)
        self.assertTrue(posted.startswith("CLAUDE_TO_MUSE: J-042 FIX on #256 @ 1a2b3c4d"))
        self.assertEqual(gh.writes[0], ("POST", "repos/IssisX/ScraperX/issues/300/labels", {"labels": ["wake:muse"]}))
        again = self.fix_pr()
        again.ctx["issue"] = {"number": 300, "labels": []}
        self.assertEqual(make(gh).muse([again]), 0)   # the job issue's marker is read: not sent again
        self.assertEqual(len(gh.posts), 1)

    def test_a_key_marked_on_the_pr_before_the_move_is_not_sent_again(self):
        key = switch.inbox_key("muse-FIX", "#256", HEAD)
        old = comment(5, 256, "CLAUDE_TO_MUSE: J-042 FIX on #256\n\n" + switch.MUSE_MARK.format(keys=key), 2.0, BOT)
        c = comment(99, 256, "**Codex (review of #256 @ 1a2b3c4d): FIX**\n\nx", 1.0, rules.CODEX_LOGIN)
        self.assertEqual(make(FakeGH()).muse([self.fix_pr(comments=[c, old])]), 0)

    def test_a_failed_label_write_is_retried_and_not_mailed_twice(self):
        """A label POST that fails after the comment lands must not lose the notification:
        sweep 1 posts the comment but no marker, sweep 2 retries the label, sweep 3 sends nothing."""
        s = self.fix_pr()
        gh = FakeGH()
        gh.fail_labels = 1
        with self.assertRaises(switch.GitHubError):
            make(gh).muse([s])
        (number, posted), = gh.posts
        self.assertEqual(number, switch.BOARD_ISSUE)
        self.assertNotIn("switchboard:muse keys=", posted)   # no marker: the key is not delivered
        self.assertEqual(gh.writes, [])
        again = self.fix_pr()
        self.assertEqual(make(gh).muse([again]), 1)
        self.assertEqual(len(gh.posts), 2)   # the notification is sent again
        labels = [w for w in gh.writes if w[0] == "POST" and w[1].endswith("/labels")]
        self.assertEqual(len(labels), 1)     # one successful label: one mail
        key = switch.inbox_key("muse-FIX", "#256", HEAD)
        self.assertIn(key, gh.by_issue[340][-1]["body"])   # the retry marks the key delivered
        third = self.fix_pr()
        self.assertEqual(make(gh).muse([third]), 0)
        self.assertEqual(len(gh.posts), 2)
        self.assertEqual(len([w for w in gh.writes if w[0] == "POST" and w[1].endswith("/labels")]), 1)

    def test_a_first_red_goes_to_muse_not_to_claude(self):
        ref = "fb-bot/integrate-j044l"
        s = steered(ref=ref, proof="red: native-tests", committed=ago(1), run_id=7)
        gh = FakeGH(runs=[run(7, ref, HEAD, hours=0.2)], jobs={7: RED_JOBS})
        self.assertEqual([i.kind for i in muse_of(gh, s)], ["RED"])
        self.assertEqual(make(gh).muse([s]), 1)
        self.assertTrue(gh.posts[0][1].startswith("CLAUDE_TO_MUSE: J-042 RED on #256 @ 1a2b3c4d"))
        self.assertEqual(make(gh).inbox([s]), 0)

    def test_red_twice_goes_to_both(self):
        ref = "fb-bot/integrate-j044l"
        runs = [run(7, ref, HEAD, hours=0.2), run(6, ref, OLD_HEAD, hours=1)]
        s = steered(ref=ref, proof="red: native-tests", committed=ago(1), run_id=7)
        gh = FakeGH(runs=runs, jobs={6: RED_JOBS})
        self.assertEqual([i.kind for i in muse_of(gh, s)], ["RED", "RED-TWICE"])
        self.assertEqual([i.kind for i in items_of(gh, s)], ["RED-TWICE"])
        self.assertEqual(make(gh).muse([s]), 1)
        self.assertEqual(make(gh).inbox([s]), 1)

    def test_a_fix_on_a_claude_pr_goes_only_to_claude(self):
        s = steered(ref="claude/integrate-j048-no-run-hurt", verdicts=[fix()])
        gh = FakeGH()
        self.assertEqual(make(gh).muse([s]), 0)
        self.assertEqual([i.kind for i in items_of(gh, s)], ["FIX"])
        self.assertEqual(make(gh).inbox([s]), 1)

    def test_a_codex_p1_with_a_reply_gives_nothing(self):
        s = steered(ref="fb-bot/integrate-j042", verdicts=[fix(hours=1.0), land(hours=0.5)])
        gh = FakeGH()
        self.assertEqual(make(gh).muse([s]), 0)
        self.assertEqual(make(gh).inbox([s]), 0)

    def test_not_merged_on_an_fb_bot_pr_goes_to_both(self):
        s = steered(verdicts=[land(hours=1)], mergeable=False, proof_done_at=ago(1))
        gh = FakeGH()
        self.assertEqual([i.kind for i in muse_of(gh, s)], ["NOT-MERGED"])
        self.assertEqual([i.kind for i in items_of(gh, s)], ["NOT-MERGED"])
        self.assertEqual(make(gh).muse([s]), 1)
        self.assertEqual(make(gh).inbox([s]), 1)

    def test_a_red_branch_with_no_pr_goes_to_muse_on_the_board(self):
        name = "fb-bot/integrate-j044c-wreck"
        gh = FakeGH(runs=[run(40, name, HEAD, hours=2)], jobs={40: RED_JOBS}, heads={name: HEAD})
        self.assertEqual([(i.kind, i.target) for i in muse_of(gh)], [("RED-BRANCH", name)])
        self.assertEqual(items_of(gh), [])
        self.assertEqual(make(gh).muse([]), 1)
        (number, posted), = gh.posts
        self.assertEqual(number, switch.BOARD_ISSUE)
        self.assertTrue(posted.startswith("CLAUDE_TO_MUSE: fb-bot/integrate-j044c-wreck RED-BRANCH on #340 @ 1a2b3c4d"))
        # the fake has no job logs: the notice says "log unread" and is keyed apart (J-104, InboxItem.unread)
        key = switch.inbox_key("muse-RED-BRANCH" + switch.UNREAD_KEY, name, HEAD)
        self.assertEqual(gh.writes[0], ("POST", "repos/IssisX/ScraperX/issues/340/labels", {"labels": ["wake:muse"]}))
        self.assertEqual(gh.writes[1][0], "PATCH")
        self.assertEqual(gh.writes[1][1], "repos/IssisX/ScraperX/issues/comments/1")
        self.assertIn(key, gh.writes[1][2]["body"])
        self.assertNotIn("switchboard:muse keys=", posted)
        self.assertIn(key, gh.by_issue[switch.BOARD_ISSUE][0]["body"])
        self.assertEqual(make(gh).inbox([]), 0)

    def test_twelve_prs_give_ten_comments_then_two(self):
        many = [self.fix_pr(number=300 + n) for n in range(12)]
        gh = FakeGH()
        self.assertEqual(make(gh).muse(many), switch.MAX_MUSE_COMMENTS)
        self.assertEqual(len(gh.posts), 10)
        self.assertEqual(len(gh.writes), 20)   # a label and a marker edit per comment
        again = [self.fix_pr(number=300 + n, comments=gh.by_issue.get(300 + n, [])) for n in range(12)]
        self.assertEqual(make(gh).muse(again), 2)
        self.assertEqual(len(gh.posts), 12)
        self.assertEqual(len(gh.writes), 24)


if __name__ == "__main__":
    unittest.main()
