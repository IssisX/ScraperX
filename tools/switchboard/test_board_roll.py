"""J-072 (#350): the switchboard rolls the board itself at BOARD_ROLL_AT comments.

The board went past its 100-comment cap three times (the last at 235) because a human had to roll it. Now a
sweep that already read the `board` listing (board_issue) rolls it from that read's comment count: it opens the next
board (labeled, the old body, a mark naming the old one), takes the label off the old one, posts one line on the new
board and one inbox line, and records the old one; a LATER sweep closes it (work/CONTEXT.md: closing #340 mid-sweep
let that sweep post on a closed board), once GitHub says it is an issue that is safe to close. A sweep that dies between steps is finished by the next one, never with a
second new board. These tests run the real switch against a fake GitHub whose writes take effect.
Run: cd tools/switchboard && python3 -m unittest test_board_roll
"""

from __future__ import annotations

import copy
import pathlib
import re
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
OLD = 546
BODY = "# Board: live jobs (4)\n\n**This body is the board.**\n\n<!-- plan:start -->\njobs\n<!-- plan:end -->\n"


class Crash(Exception):
    """The sweep dies here (the runner is killed): nothing after it runs and the sweep state is not saved."""


class RollGH(tsc.CountingGH):
    """test_sweep_calls' counting fake, with the board writes taking effect: a POST of an issue opens a labeled
    board, a DELETE of the label unlabels one (404 when it carries none), a PATCH closes one. `crash` names the
    write (its "METHOD path" or "COMMENT #n" prefix) the sweep dies at, once. `labels` adds labels to an issue's
    record (the GET the close makes)."""

    def __init__(self, boards, dry_run=False, **kw):
        super().__init__([], dry_run=dry_run, **kw)
        self.boards = [dict(b) for b in boards]   # the open issues labeled `board`
        self.unlabeled: dict[int, dict] = {}
        self.closed: set[int] = set()
        self.crash = ""
        self.next_number = 600
        self.labels: dict[int, list] = {}

    def route(self, path):
        m = re.fullmatch(rf"repos/{REPO}/issues/(\d+)", path)
        if m and int(m.group(1)) == switch.INBOX_PR:   # the inbox is a pull request
            return dict(super().route(path), pull_request={"url": "x"})
        if m:
            self.calls.append(path)
            self.gets.append(path)
            number = int(m.group(1))
            names = (["board"] if any(b["number"] == number for b in self.boards) else []) + self.labels.get(number, [])
            return {"number": number, "state": "closed" if number in self.closed else "open",
                    "labels": [{"name": n} for n in names]}
        return super().route(path)

    def _die(self, what):
        if self.crash and what.startswith(self.crash):
            self.crash = ""
            raise Crash(what)

    def comment(self, number, body, what=""):
        self._die(f"COMMENT #{number}")
        return super().comment(number, body, what)

    def write(self, method, path, body=None, what=""):
        self._die(f"{method} {path}")
        if self.dry_run:   # ghapi.GitHub.write: printed, never sent
            self.calls.append(f"DRY {method} {path}")
            return None
        base = f"repos/{REPO}/issues"
        if method == "POST" and path == base:
            super().write(method, path, body, what)
            number, self.next_number = self.next_number, self.next_number + 1
            self.boards.append({"number": number, "title": body["title"], "body": body["body"], "comments": 0})
            return {"number": number}
        if method == "DELETE" and path.endswith("/labels/board"):
            number = int(path.split("/issues/")[1].split("/")[0])
            hit = [b for b in self.boards if b["number"] == number]
            if not hit:
                raise switch.GitHubError(method, path, "gh: Label does not exist (HTTP 404)", 404)
            self.boards.remove(hit[0])
            self.unlabeled[number] = hit[0]
        if method == "PATCH" and (body or {}).get("state") == "closed":
            self.closed.add(int(path.rsplit("/", 1)[1]))
        return super().write(method, path, body, what)


def board(number=OLD, comments=95, title="Board: live jobs (5)", body=BODY) -> dict:
    return {"number": number, "title": title, "body": body, "comments": comments}


def sweep(gh, read=True):
    """One sweep's roll part: a fresh Switch on the same fake (its saved sweep state carried over), the board listing
    read first when `read` (as a full pass reads it for the plan), then board_roll."""
    sw = ti.make(gh)
    if read:
        sw.board_issue()
    sw.board_roll()
    return sw


def dies(gh, at: str, read=True):
    """A sweep that dies at the write `at`: the sweep state is the one saved before it (the cache is saved at the end)."""
    saved, gh.crash = copy.deepcopy(gh.state), at
    with unittest.TestCase().assertRaises(Crash):
        sweep(gh, read)
    gh.state = saved


def opened(gh):
    return [w for w in gh.writes if w[:2] == ("POST", f"repos/{REPO}/issues")]


def closes(gh):
    return [p for m, p, b in gh.writes if m == "PATCH" and (b or {}).get("state") == "closed"]


class Threshold(unittest.TestCase):
    def test_below_90_no_call_past_the_listing(self):
        for count in (0, 50, 89):
            gh = RollGH([board(comments=count)])
            sweep(gh)
            self.assertEqual(gh.calls, [OPEN], count)   # the read board_issue makes anyway
            self.assertEqual((gh.writes, gh.posts), ([], []), count)

    def test_at_90_the_board_rolls(self):
        gh = RollGH([board(comments=90)])
        sweep(gh)
        self.assertEqual(len(opened(gh)), 1)

    def test_no_listing_read_this_run_no_call(self):
        gh = RollGH([board(comments=200)])   # a fast sweep that never needed the board
        sweep(gh, read=False)
        self.assertEqual((gh.calls, gh.writes), ([], []))

    def test_a_count_that_is_not_a_number_never_rolls(self):
        for count in (None, "95", 95.0, True):
            gh = RollGH([board(comments=count)])
            sweep(gh)
            self.assertEqual(gh.writes, [], count)

    def test_a_failed_listing_never_rolls(self):
        gh = RollGH([board(comments=200)])
        real = gh.route

        def route(path):
            if path.startswith(OPEN):
                raise switch.GitHubError("GET", path, "gh: Server Error (HTTP 502)", 502)
            return real(path)
        gh.route = route
        sw = sweep(gh)   # board_issue falls back (a WARN), nothing is rolled blind
        self.assertEqual(gh.writes, [])
        self.assertIsNone(sw._board_listing)

    def test_two_labeled_boards_neither_names_are_left_alone(self):
        gh = RollGH([board(500, comments=120), board(comments=120)])
        sweep(gh)
        self.assertEqual(gh.writes, [])


class TwoSteps(unittest.TestCase):
    def test_the_roll_then_the_close_on_the_next_sweep(self):
        gh = RollGH([board()])
        sw = sweep(gh)
        ((_, _, made),) = opened(gh)
        self.assertEqual(made["title"], "Board: live jobs (6)")
        self.assertEqual(made["labels"], ["board"])
        self.assertIn(switch.BOARD_FROM_MARK.format(old=OLD), made["body"])
        self.assertTrue(made["body"].startswith("# Board: live jobs (6)\n"))   # its heading, the new name
        self.assertTrue(made["body"].endswith(BODY.partition("\n\n")[2]))   # the rest of the old body, as it stands
        new = 600
        self.assertEqual([b["number"] for b in gh.boards], [new])   # never two labeled
        self.assertEqual(closes(gh), [])   # never closed by the sweep that rolled
        self.assertEqual([n for n, _ in gh.posts], [new, switch.INBOX_PR])   # one board line, one inbox line
        self.assertIn(f"#{OLD}", gh.posts[0][1])
        self.assertEqual(gh.state["board_roll"], {"old": OLD, "new": new, "closed": False, "past": []})
        self.assertEqual(gh.state["board"], new)   # board_issue's fallback, when a listing fails
        self.assertTrue(any(line.startswith("BOARD") for line in sw.lines))
        # the cost of the roll pass: the listing (already read) and four writes
        self.assertEqual([c for c in gh.calls if not c.startswith(OPEN)],
                         [f"POST repos/{REPO}/issues", f"DELETE repos/{REPO}/issues/{OLD}/labels/board",
                          f"POST issues/{new}/comments", f"POST issues/{switch.INBOX_PR}/comments"])
        gh.calls.clear()
        sweep(gh, read=False)   # the next sweep, even one that reads no board: one read, one write
        self.assertEqual(gh.calls, [f"repos/{REPO}/issues/{OLD}", f"PATCH repos/{REPO}/issues/{OLD}"])
        self.assertEqual(gh.closed, {OLD})
        self.assertTrue(gh.state["board_roll"]["closed"])
        gh.calls.clear()
        sweep(gh)   # and after: nothing more
        self.assertEqual(gh.calls, [OPEN])

    def test_the_new_board_rolls_in_its_turn_and_keeps_the_boards_before(self):
        gh = RollGH([board()])
        sweep(gh)
        sweep(gh)
        gh.boards[0]["comments"] = 91
        sweep(gh)
        third = gh.boards[0]
        self.assertEqual(third["title"], "Board: live jobs (7)")
        self.assertEqual(switch.BOARD_FROM.findall(third["body"]), ["600"])   # the older mark is taken out
        self.assertEqual(third["body"].count("_Continues #"), 1)
        self.assertEqual(gh.state["board_roll"], {"old": 600, "new": 601, "closed": False, "past": [OLD]})
        sweep(gh)
        self.assertEqual(gh.closed, {OLD, 600})

    def test_a_label_already_gone_is_done(self):
        gh = RollGH([board()])
        sw = ti.make(gh)
        sw.board_issue()
        real = RollGH.write   # someone took the label off by hand between the read and the roll

        def write(m, p, b=None, w=""):
            if m == "DELETE":
                raise switch.GitHubError(m, p, "gh: Label does not exist (HTTP 404)", 404)
            return real(gh, m, p, b, w)
        gh.write = write
        sw.board_roll()
        self.assertEqual(gh.state["board_roll"]["old"], OLD)
        self.assertEqual(len(gh.posts), 2)

    def test_a_dry_run_prints_and_records_nothing(self):
        gh = RollGH([board()], dry_run=True)
        sw = sweep(gh)
        self.assertIn(f"DRY POST repos/{REPO}/issues", gh.calls)
        self.assertEqual((gh.writes, gh.posts), ([], []))
        self.assertNotIn("board_roll", gh.state)
        self.assertEqual(gh.state["board"], OLD)   # board_issue's own memo; the roll moved nothing
        self.assertTrue(any("dry run" in line for line in sw.lines), sw.lines)
        gh2 = RollGH([board(comments=10)], dry_run=True)
        gh2.state = {"board_roll": {"old": 500, "new": OLD, "closed": False, "past": []}}
        sweep(gh2)
        self.assertEqual(gh2.state["board_roll"]["closed"], False)   # the close only printed
        self.assertNotIn(f"PATCH repos/{REPO}/issues/500", gh2.calls)
        self.assertEqual(gh2.writes, [])
        gh2.state["board_roll"]["new"] = 999   # 500 is older than #999: the close is printed only
        sweep(gh2)
        self.assertIn(f"DRY PATCH repos/{REPO}/issues/500", gh2.calls)
        self.assertEqual(gh2.state["board_roll"]["closed"], False)


class Rerun(unittest.TestCase):
    """The sweep dies after each step; the next sweep finishes the roll and never opens a second board."""

    def finish(self, gh):
        sweep(gh)   # the next sweep finishes the roll (or records it)
        self.assertEqual(len(opened(gh)), 1)
        self.assertEqual([b["number"] for b in gh.boards], [600])
        self.assertEqual(closes(gh), [])
        sweep(gh)   # the one after closes the old board
        self.assertEqual(gh.closed, {OLD})
        self.assertEqual(len(opened(gh)), 1)
        self.assertEqual(gh.state["board"], 600)

    def test_dies_on_the_create(self):
        gh = RollGH([board()])
        dies(gh, f"POST repos/{REPO}/issues")
        self.assertEqual((gh.writes, gh.state), ([], {}))
        self.finish(gh)
        self.assertEqual(len(gh.posts), 2)

    def test_dies_after_the_create(self):
        gh = RollGH([board()])
        dies(gh, "DELETE")
        self.assertEqual([b["number"] for b in gh.boards], [OLD, 600])   # two labeled, for one sweep
        self.finish(gh)
        self.assertEqual([n for n, _ in gh.posts], [600, switch.INBOX_PR])

    def test_dies_after_the_label_comes_off(self):
        gh = RollGH([board()])
        dies(gh, "COMMENT #600")
        self.assertEqual(gh.state, {})   # the record was not saved
        self.finish(gh)
        self.assertEqual(gh.posts, [])   # the notices are the rolling sweep's; a recovery records only

    def test_dies_after_the_notices(self):
        gh = RollGH([board()])
        sw = ti.make(gh)
        sw.board_issue()
        saved = copy.deepcopy(gh.state)
        sw.board_roll()
        gh.state = saved   # the sweep died before the cache (and its state) was saved
        self.finish(gh)
        self.assertEqual(len(gh.posts), 2)   # not posted again

    def test_dies_on_the_close(self):
        gh = RollGH([board()])
        sweep(gh)
        dies(gh, "PATCH")
        self.assertEqual(gh.closed, set())
        sweep(gh)
        self.assertEqual(gh.closed, {OLD})
        self.assertEqual(len(opened(gh)), 1)

    def test_a_cold_state_after_the_roll_closes_once(self):
        gh = RollGH([board()])
        sweep(gh)
        gh.state = {}   # the cache was lost
        sweep(gh)       # the live board's mark names #546: recorded
        sweep(gh)
        self.assertEqual(gh.closed, {OLD})
        self.assertEqual(len(opened(gh)), 1)
        self.assertEqual(len(gh.posts), 2)

    def test_a_board_labeled_again_is_never_closed(self):
        gh = RollGH([board()])
        sweep(gh)
        gh.boards.insert(0, gh.unlabeled.pop(OLD))   # someone puts the label back
        sw = sweep(gh)
        self.assertEqual(closes(gh), [])
        self.assertTrue(any("again" in line for line in sw.lines), sw.lines)
        self.assertTrue(gh.state["board_roll"]["closed"])   # told once, not read again every sweep
        gh.calls.clear()
        sweep(gh, read=False)
        self.assertEqual(gh.calls, [])

    def test_a_record_naming_a_pull_request_never_closes_it(self):
        # a sweep state (or a board body's mark) is the only source of the number: GitHub is asked before a close
        for old, new in ((switch.INBOX_PR, 600), (700, 600)):
            gh = RollGH([board(600, comments=3)])
            gh.state = {"board_roll": {"old": old, "new": new, "closed": False, "past": []}}
            sw = sweep(gh)
            self.assertEqual(closes(gh), [], old)
            self.assertTrue(any(line.startswith("WARN        board:") for line in sw.lines), sw.lines)
            self.assertTrue(gh.state["board_roll"]["closed"])
        self.assertNotIn(f"repos/{REPO}/issues/700", gh.calls)   # not older than the board it rolled to: no read

    def test_a_wake_label_still_on_the_old_board_holds_the_close(self):
        # the rolling sweep's Muse notice went on the old board; the bridge mails only open issues' wake:muse
        gh = RollGH([board()])
        sweep(gh)
        gh.labels[OLD] = [switch.MUSE_LABEL]
        sw = sweep(gh)
        self.assertEqual(closes(gh), [])
        self.assertFalse(gh.state["board_roll"]["closed"])
        self.assertTrue(any("stays open" in line for line in sw.lines), sw.lines)
        gh.labels[OLD] = []   # the bridge mailed it and took the label off
        sweep(gh)
        self.assertEqual(gh.closed, {OLD})

    def test_an_old_board_closed_by_hand_costs_no_write(self):
        gh = RollGH([board()])
        sweep(gh)
        gh.closed.add(OLD)
        gh.writes.clear()
        sweep(gh)
        self.assertEqual(gh.writes, [])
        self.assertTrue(gh.state["board_roll"]["closed"])


class OldMarks(unittest.TestCase):
    """The old board loses its label, so old_boards' closed listing misses it: its muse and plan marks still count."""

    def test_the_rolled_board_counts_open_or_closed_with_or_without_state(self):
        gh = RollGH([board()])
        sweep(gh)
        for state in (copy.deepcopy(gh.state), {}):
            gh.state = state
            sw = ti.make(gh)
            self.assertIn(OLD, sw.old_boards())
            self.assertNotIn(600, sw.old_boards())

    def test_a_key_sent_on_the_rolled_board_is_not_sent_again(self):
        gh = RollGH([board()], runs=[ti.run(40, "fb-bot/integrate-j044c-wreck", ti.HEAD, hours=2)],
                    jobs={40: ti.RED_JOBS}, heads={"fb-bot/integrate-j044c-wreck": ti.HEAD})
        key = switch.inbox_key("muse-RED-BRANCH" + switch.UNREAD_KEY, "fb-bot/integrate-j044c-wreck", ti.HEAD)
        gh.by_issue[OLD] = [{"id": 9, "body": f"CLAUDE_TO_MUSE: x\n\n" + switch.MUSE_MARK.format(keys=key),
                             "user": {"login": ti.BOT}}]
        sweep(gh)
        posts = len(gh.posts)
        for state in (copy.deepcopy(gh.state), {}):   # the old board still open, then a lost state
            gh.state = state
            self.assertEqual(ti.make(gh).muse([]), 0)
        sweep(gh)   # closed now, no label
        self.assertEqual(ti.make(gh).muse([]), 0)
        self.assertEqual(len(gh.posts), posts)

    def test_a_failed_listing_after_the_roll_is_the_new_board(self):
        gh = RollGH([board()])
        sweep(gh)
        gh.boards = "down"   # an answer that is no listing: board_issue falls back to the state's last board
        self.assertEqual(ti.make(gh).board_issue(), 600)


class NextBoard(unittest.TestCase):
    def test_titles(self):
        for title, want in (("Board: live jobs (5)", "Board: live jobs (6)"), ("Board: live jobs (9) ", "Board: live jobs (10)"),
                            ("Board", "Board (2)"), ("", "Board: live jobs (2)")):
            self.assertEqual(switch.board_next(board(title=title))[0], want, title)

    def test_the_body_is_the_old_one_with_one_mark_under_its_heading(self):
        title, body = switch.board_next(board(body=BODY))
        head, note, rest = body.split("\n\n", 2)
        self.assertEqual(head, "# Board: live jobs (6)")
        self.assertTrue(note.startswith(f"_Continues #{OLD}, rolled by the switchboard at 95 comments"))
        self.assertEqual(switch.BOARD_FROM.findall(body), [str(OLD)])
        self.assertEqual(rest, BODY.partition("\n\n")[2])
        plain = "no heading\n\ntext\n"
        self.assertTrue(switch.board_next(board(body=plain))[1].endswith("\n\n" + plain))


class Sweep(unittest.TestCase):
    """run_sweep: the roll is the last board part of a sweep, and a pass below the mark reads the listing once."""

    def run_one(self, gh, whole):
        sw = ti.make(gh, ti.NOW)
        sw.root = HERE.parents[1]
        sw.plan_mode = lambda: "off"
        sw.merge_token = sw.review_key = ""
        sw._jobs = sw._board = None
        gh.state = whole
        gh.remaining = gh.limit = gh.reset = None
        args = types.SimpleNamespace(part="steer", pr=0, sha="", issue=0, label="", digest=False,
                                     force_digest=False, dry_run=False)
        with mock.patch.object(switch, "sweep_heartbeat"):
            return switch.run_sweep(sw, args, ti.NOW), sw

    def test_a_full_pass_rolls_and_a_later_sweep_closes(self):
        gh = RollGH([board()])
        gh.open_prs = [tsc.pr(361)]
        status, sw = self.run_one(gh, {})
        self.assertEqual(status, 0, sw.lines)
        self.assertEqual(len([c for c in gh.calls if c.startswith(OPEN)]), 1, gh.calls)   # no read of its own
        self.assertEqual(len(opened(gh)), 1)
        self.assertEqual(closes(gh), [])
        status, sw = self.run_one(gh, gh.state)
        self.assertEqual(status, 0, sw.lines)
        self.assertEqual(gh.closed, {OLD})
        self.assertEqual(len(opened(gh)), 1)

    def test_a_full_pass_below_the_mark_writes_nothing_for_the_board(self):
        gh = RollGH([board(comments=89)])
        gh.open_prs = [tsc.pr(361)]
        status, sw = self.run_one(gh, {})
        self.assertEqual(status, 0, sw.lines)
        self.assertEqual(len([c for c in gh.calls if c.startswith(OPEN)]), 1, gh.calls)
        self.assertEqual([w for w in gh.writes if "/issues" in w[1] and "/labels/board" in w[1]], [])
        self.assertEqual(opened(gh), [])


if __name__ == "__main__":
    unittest.main()
