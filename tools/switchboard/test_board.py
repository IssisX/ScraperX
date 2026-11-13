"""J-072: the board's issue number comes from GitHub at run time, not from the code.

The board is the one open issue labeled `board` (work/tasks/README.md). It rolls over to a new issue at 100
comments: #107 to #340 on 11-09 needed a code change and a paste into the deployed bridge, and until both were
done the bridge posted on the closed board. Now the switch reads the label once per run (Switch.board_issue) and a
roll needs neither. These tests run the real switch against a fake GitHub.
Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
"""

from __future__ import annotations

import datetime as dt
import pathlib
import sys
import types
import unittest
from unittest import mock

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import switch  # noqa: E402
import test_inbox as ti  # noqa: E402
import test_sweep_calls as tsc  # noqa: E402

REPO = ti.FakeGH.repo
OPEN = f"repos/{REPO}/issues?labels=board&state=open"
CLOSED = f"repos/{REPO}/issues?labels=board&state=closed"
NAME = "fb-bot/integrate-j044c-wreck"
NEW = 400   # the board after a roll: #340 closed, #400 open and labeled `board`


class BoardGH(ti.FakeGH):
    """test_inbox's fake; `down` names the board listings GitHub fails (502)."""

    down: tuple = ()

    def route(self, path):
        if any(path.startswith(p) for p in self.down):
            self.gets.append(path)
            raise switch.GitHubError("GET", path, "gh: Server Error (HTTP 502)", 502)
        return super().route(path)


def gh_with(boards=(switch.BOARD_ISSUE,), closed=(), state=None, down=(), **kw) -> BoardGH:
    gh = BoardGH(**kw)
    gh.boards, gh.closed_boards, gh.down = list(boards), list(closed), tuple(down)
    gh.state = {} if state is None else state
    return gh


def reads(gh, prefix: str) -> list[str]:
    return [p for p in gh.gets if p.startswith(prefix)]


def warns(sw) -> list[str]:
    return [line for line in sw.lines if line.startswith("WARN        board:")]


def red_branch(**kw):
    """A red fb-bot branch with no PR: its Muse notice goes on the board. (the fake, the notice's key: the fake has
    no job logs, so the notice says "log unread" and is keyed apart, J-104)"""
    gh = gh_with(runs=[ti.run(40, NAME, ti.HEAD, hours=2)], jobs={40: ti.RED_JOBS}, heads={NAME: ti.HEAD}, **kw)
    return gh, switch.inbox_key("muse-RED-BRANCH" + switch.UNREAD_KEY, NAME, ti.HEAD)


def marked(key: str, number: int) -> dict:
    return {"id": 9, "body": f"CLAUDE_TO_MUSE: x RED-BRANCH on #{number}\n\n" + switch.MUSE_MARK.format(keys=key),
            "user": {"login": ti.BOT}}


class Resolve(unittest.TestCase):
    def test_the_open_issue_labeled_board_is_the_board(self):
        gh = gh_with(boards=[NEW], closed=[switch.BOARD_ISSUE])
        sw = ti.make(gh)
        self.assertEqual(sw.board_issue(), NEW)
        self.assertEqual(warns(sw), [])
        self.assertEqual(gh.state["board"], NEW)   # the next run's fallback

    def test_it_is_read_once_per_run(self):
        gh = gh_with(boards=[NEW])
        sw = ti.make(gh)
        sw._board = None
        for _ in range(3):
            self.assertEqual(sw.board_issue(), NEW)
        sw.board_comments()
        sw.board_comments()
        self.assertEqual(len(reads(gh, OPEN)), 1)
        self.assertEqual(len(reads(gh, f"repos/{REPO}/issues/{NEW}/comments")), 1)

    def test_no_open_board_is_the_last_one_known_and_one_warning(self):
        gh = gh_with(boards=[], state={"board": NEW})
        sw = ti.make(gh)
        self.assertEqual(sw.board_issue(), NEW)
        self.assertEqual(sw.board_issue(), NEW)
        (line,) = warns(sw)
        self.assertIn("no open issue is labeled `board`", line)
        self.assertIn(f"#{NEW}", line)
        self.assertEqual(gh.state["board"], NEW)

    def test_no_open_board_and_none_known_is_the_fallback_constant(self):
        gh = gh_with(boards=[])
        sw = ti.make(gh)
        self.assertEqual(sw.board_issue(), switch.BOARD_ISSUE)
        self.assertEqual(len(warns(sw)), 1)
        self.assertNotIn("board", gh.state)   # a fallback is never kept as a board that was found

    def test_a_failed_read_never_raises_and_is_the_last_one_known(self):
        gh = gh_with(boards=[999], state={"board": NEW}, down=[OPEN])
        sw = ti.make(gh)
        self.assertEqual(sw.board_issue(), NEW)
        (line,) = warns(sw)
        self.assertIn("could not be read", line)
        gh = gh_with(down=[OPEN])
        sw = ti.make(gh)
        self.assertEqual(sw.board_issue(), switch.BOARD_ISSUE)
        self.assertEqual(len(warns(sw)), 1)

    def test_a_last_board_that_is_not_a_plain_number_is_never_used(self):
        for bad in ("400", 0, -4, True, 4.5, [400], None):
            gh = gh_with(boards=[], state={"board": bad})
            self.assertEqual(ti.make(gh).board_issue(), switch.BOARD_ISSUE, repr(bad))

    def test_two_open_boards_is_the_newest_and_one_warning_naming_them(self):
        gh = gh_with(boards=[switch.BOARD_ISSUE, NEW, 12])
        sw = ti.make(gh)
        self.assertEqual(sw.board_issue(), NEW)
        (line,) = warns(sw)
        for n in (12, switch.BOARD_ISSUE, NEW):
            self.assertIn(f"#{n}", line)

    def test_a_pull_request_carrying_the_label_is_never_the_board(self):
        gh = gh_with(boards=[{"number": 410, "pull_request": {"url": "x"}}, switch.BOARD_ISSUE])
        sw = ti.make(gh)
        self.assertEqual(sw.board_issue(), switch.BOARD_ISSUE)
        self.assertEqual(warns(sw), [])
        gh = gh_with(boards=[{"number": 410, "pull_request": {"url": "x"}}], state={"board": NEW})
        self.assertEqual(ti.make(gh).board_issue(), NEW)   # only a PR carries it: no board is open

    def test_an_answer_that_is_not_a_listing_is_no_board(self):
        gh = gh_with(state={"board": NEW})
        gh.boards = [{"title": "no number"}, {"number": "401"}, {"number": True}]
        sw = ti.make(gh)
        self.assertEqual(sw.board_issue(), NEW)
        self.assertEqual(len(warns(sw)), 1)


class Rollover(unittest.TestCase):
    """#340 closed at 100 comments and #400 is open and labeled: the same code posts and reads on #400."""

    def test_a_muse_notice_goes_on_the_new_board(self):
        gh, key = red_branch(boards=[NEW], closed=[switch.BOARD_ISSUE])
        self.assertEqual(ti.make(gh).muse([]), 1)
        ((number, body),) = gh.posts
        self.assertEqual(number, NEW)
        self.assertIn(f"on #{NEW} @", body)
        self.assertEqual(gh.writes[0], ("POST", f"repos/{REPO}/issues/{NEW}/labels", {"labels": ["wake:muse"]}))
        self.assertIn(key, gh.by_issue[NEW][0]["body"])

    def test_a_key_sent_on_the_board_that_just_closed_is_not_sent_again(self):
        gh, key = red_branch(boards=[NEW], closed=[switch.BOARD_ISSUE])
        gh.by_issue[switch.BOARD_ISSUE] = [marked(key, switch.BOARD_ISSUE)]
        self.assertEqual(ti.make(gh).muse([]), 0)
        self.assertEqual(gh.posts, [])

    def test_the_boards_closed_before_the_label_still_count(self):
        # #107, #87 and #3 closed before the board was found by label: with or without the label, their keys count
        for closed in ([], [switch.BOARD_ISSUE], [switch.BOARD_ISSUE, 107]):
            gh, key = red_branch(boards=[NEW], closed=closed)
            gh.by_issue[87] = [marked(key, 87)]
            self.assertEqual(ti.make(gh).muse([]), 0, closed)

    def test_the_closed_boards_read_are_bounded_and_never_a_pr_or_the_live_board(self):
        closed = [{"number": 990, "pull_request": {"url": "x"}}, NEW] + list(range(900, 880, -1))
        gh = gh_with(boards=[NEW], closed=closed)
        old = ti.make(gh).old_boards()
        self.assertEqual(old, [900, 899, 898, 897][:switch.MAX_OLD_BOARDS])
        self.assertLessEqual(len(old), switch.MAX_OLD_BOARDS)

    def test_a_failed_read_of_the_closed_boards_fails_the_part_rather_than_send_again(self):
        gh, key = red_branch(boards=[NEW], closed=[switch.BOARD_ISSUE], down=[CLOSED])
        gh.by_issue[switch.BOARD_ISSUE] = [marked(key, switch.BOARD_ISSUE)]
        with self.assertRaises(switch.GitHubError):   # run_sweep's part(): retried at the next sweep
            ti.make(gh).muse([])
        self.assertEqual(gh.posts, [])

    def test_the_pipe_alarm_is_read_from_and_posted_on_the_new_board(self):
        gh = gh_with(boards=[NEW], runs=[{"display_title": "Switchboard sweep",
                                          "created_at": (ti.NOW - dt.timedelta(minutes=5)).strftime("%Y-%m-%dT%H:%M:%SZ")}])
        sw = ti.make(gh)
        sw.root = HERE.parents[1]
        self.assertTrue(any(line.startswith("BRIDGE VERSION") for line in sw.pipe_health()))
        ((number, _),) = gh.posts
        self.assertEqual(number, NEW)
        sw2 = ti.make(gh)
        sw2.root = sw.root
        gh.by_issue[NEW][0]["created_at"] = "2026-11-06T19:00:00Z"
        sw2.pipe_health()
        self.assertEqual(len(gh.posts), 1)   # the alarm's marker is found on the new board: not posted twice

    def test_a_bridged_verdict_on_the_new_board_counts(self):
        import test_switchboard as ts
        gh = ts.FactsGH(with_issue=False)
        gh.table[OPEN] = [{"number": NEW}]
        gh.table[f"repos/{REPO}/issues/{NEW}/comments"] = [ts.bridged("review of #256 @ 1a2b3c4d: LAND")]
        sw = switch.Switch.__new__(switch.Switch)
        sw.gh, sw.team, sw.review_key, sw._jobs, sw._board = gh, ts.TEAM, "", None, None
        sw.out = lambda line: None
        facts, _ = sw.pr_facts(ts.BridgedVerdicts.PR)
        (v,) = facts.verdicts
        self.assertEqual((v.who, v.verdict), ("Muse", "LAND"))


class Sweep(unittest.TestCase):
    """run_sweep hands the board it resolved to the ledger and the reconciler; a sweep that needs no board reads none."""

    def sweep(self, gh, whole):
        sw = ti.make(gh, ti.NOW)
        sw.root = HERE.parents[1]
        sw.plan_mode = lambda: "off"   # the plan part has its own sweep tests (test_plan_sweep); not the live config
        sw.merge_token = sw.review_key = ""
        sw._jobs = sw._board = None
        gh.state = whole
        gh.remaining = gh.limit = gh.reset = None
        args = types.SimpleNamespace(part="steer", pr=0, sha="", issue=0, label="", digest=False,
                                     force_digest=False, dry_run=False)
        import reconciler
        with mock.patch("ledger.sweep", side_effect=tsc.no_ledger) as led, \
                mock.patch("reconciler.sweep", return_value=reconciler.Result("committed", "h", {}, 1, 3)) as rec, \
                mock.patch("reconciler.ledger_notifies", return_value=True), \
                mock.patch.object(switch, "sweep_heartbeat"):   # the ledger's own clock, as when it notifies
            status = switch.run_sweep(sw, args, ti.NOW)
        return status, led, rec, sw

    def test_a_full_pass_gives_the_ledger_and_the_reconciler_the_new_board(self):
        gh = tsc.CountingGH([tsc.pr(361)], dry_run=False)
        gh.boards = [NEW]
        status, led, rec, sw = self.sweep(gh, {})
        self.assertEqual(status, 0, sw.lines)
        self.assertEqual(led.call_args.args[2], NEW)
        self.assertEqual(rec.call_args.args[2], NEW)
        self.assertEqual(gh.state["board"], NEW)
        self.assertEqual(len([c for c in gh.calls if c.startswith(OPEN)]), 1, gh.calls)

    def test_an_idle_sweep_reads_no_board(self):
        prs = [tsc.pr(n) for n in range(361, 364)]
        gh = tsc.CountingGH(prs)
        status, _, _, sw = self.sweep(gh, {"steer": tsc.last_state(prs)})
        self.assertEqual(status, 0, sw.lines)
        self.assertEqual([c for c in gh.calls if "labels=board" in c], [], gh.calls)

    def ledger_timer(self, thread: int) -> dict:
        import ledger
        it = ledger.Item("JOB", "#291", "", "J-099", None, ledger.Person("Builder", "builder"), "claimed, no PR",
                         ti.NOW - dt.timedelta(hours=4, minutes=1), dt.timedelta(hours=4), "v1", thread)
        return {"at": tsc.iso(ti.NOW - dt.timedelta(minutes=1)), "item": ledger.item_json(it)}

    def fast_sweep_with_a_ripe_notice(self, thread: int):
        prs = [tsc.pr(n) for n in range(361, 364)]
        state = tsc.last_state(prs)
        state["timers"]["ledger"] = [self.ledger_timer(thread)]
        gh = tsc.CountingGH(prs, dry_run=False)
        gh.boards, gh.closed_boards = [NEW], [switch.BOARD_ISSUE]
        status, _, _, sw = self.sweep(gh, {"steer": state, "board": switch.BOARD_ISSUE})   # the last sweep saw #340
        self.assertEqual(status, 0, sw.lines)
        return gh

    def test_a_fast_sweeps_ledger_notice_on_the_board_goes_on_the_new_board(self):
        # the ledger clock sends from the full pass's reading with no read, but never on a board that has closed:
        # a comment there is read by no one, and the bridge mails only from open issues
        gh = self.fast_sweep_with_a_ripe_notice(thread=0)
        ((number, _),) = gh.posts
        self.assertEqual(number, NEW)
        self.assertEqual(len([c for c in gh.calls if c.startswith(OPEN)]), 1, gh.calls)

    def test_a_fast_sweeps_ledger_notice_on_a_job_thread_reads_no_board(self):
        gh = self.fast_sweep_with_a_ripe_notice(thread=290)
        ((number, _),) = gh.posts
        self.assertEqual(number, 290)
        self.assertEqual([c for c in gh.calls if "labels=board" in c], [], gh.calls)

    def test_the_dry_run_tools_ask_the_switch_for_the_board(self):
        # ledger.py and reconciler.py's own main() (a dry run by hand) use the board the switch resolves
        for name in ("ledger.py", "reconciler.py"):
            src = (HERE / name).read_text(encoding="utf-8")
            main = src[src.index("\ndef main() -> int:"):]
            self.assertNotIn("switch.BOARD_ISSUE", main, name)
            self.assertIn("sw.board_issue()", main, name)


if __name__ == "__main__":
    unittest.main()
