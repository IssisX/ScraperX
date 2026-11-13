"""SYSTEM_PLAN 6 (J-072, 11-13): a merge never waits for a review.

The sweep's run has no ask job. When steer sees a green head whose turn is the automatic review's or the eye's, it
starts one "Switchboard reviews" run (switchboard.yml part=ask): one dispatch a sweep, none in a dry run, none while a
reviews run is queued or running (one read of the run list). The sweeps here are the real run_sweep on
test_sweep_calls' fake GitHub; the turn itself (Switch.reviews_due) is tested on test_workflow_merge's facts.
Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
"""

from __future__ import annotations

import dataclasses
import pathlib
import sys
import unittest
from unittest import mock

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import rules  # noqa: E402
import switch  # noqa: E402
import test_sweep_calls as T  # noqa: E402
import test_workflow_merge as W  # noqa: E402

RUNS = f"repos/{T.REPO}/actions/workflows/{switch.SWITCHBOARD_WORKFLOW}/runs"
DISPATCH = f"repos/{T.REPO}/actions/workflows/{switch.SWITCHBOARD_WORKFLOW}/dispatches"


class ReviewsGH(T.CountingGH):
    """CountingGH with this workflow's run list and its dispatch: `reviews` are the runs listed, `refuse` fails it."""

    def __init__(self, prs, reviews=(), refuse="", dry_run=False):
        super().__init__(prs, dry_run=dry_run)
        self.reviews, self.refuse = list(reviews), refuse
        self.listed: list[str] = []
        self.dispatched: list = []

    def route(self, path):
        if path.split("?")[0] == RUNS:
            self.calls.append(path)
            self.listed.append(path)
            return {"workflow_runs": list(self.reviews)}
        return super().route(path)

    def write(self, method, path, body=None, what=""):
        if path == DISPATCH:
            self.calls.append(f"{method} {path}")
            if self.dry_run:
                raise AssertionError("a dry run dispatches nothing")
            self.dispatched.append((method, body))
            if self.refuse:
                raise switch.GitHubError(method, path, self.refuse, 403)
            return None
        return super().write(method, path, body, what)


def run(n: int, title: str = switch.REVIEWS_RUN, status: str = "in_progress") -> dict:
    return {"id": n, "display_title": title, "status": status, "created_at": T.iso(T.NOW)}


class TheSweepStartsTheReviews(unittest.TestCase):
    def setUp(self):
        self.prs = [T.pr(n) for n in range(361, 365)]

    def sweep(self, due=(), **kw):
        """A fast sweep that reads every PR (each waits on a clock); `due`: the PRs whose turn is a paid one."""
        gh = ReviewsGH(self.prs, **kw)
        asked = []

        def reviews_due(sw, seen):
            asked.append(seen.facts.number)
            return "the automatic review" if seen.facts.number in due else ""

        with mock.patch.object(switch.Switch, "reviews_due", reviews_due):
            status, read, sw = T.sweep(gh, T.last_state(self.prs, cold=False))
        self.assertEqual(read, [p["number"] for p in self.prs])
        return status, gh, sw, asked

    def test_a_head_waiting_for_the_api_starts_one_reviews_run(self):
        status, gh, sw, asked = self.sweep(due={362})
        self.assertEqual(status, 0)
        self.assertEqual(gh.dispatched, [("POST", {"ref": rules.TRUNK, "inputs": {"part": "ask"}})])
        self.assertEqual(len(gh.listed), 1)   # one read of the run list
        self.assertEqual(asked, [361, 362])    # stops at the first head due
        self.assertTrue(any(l.startswith("WAKE-REVIEWS #362") and "started" in l for l in sw.lines), sw.lines)

    def test_two_heads_waiting_still_start_one_run(self):
        status, gh, _, asked = self.sweep(due={361, 363, 364})
        self.assertEqual((status, len(gh.dispatched), len(gh.listed)), (0, 1, 1))
        self.assertEqual(asked, [361])

    def test_no_head_waiting_starts_nothing_and_reads_no_run_list(self):
        status, gh, sw, asked = self.sweep()
        self.assertEqual((status, gh.dispatched, gh.listed), (0, [], []))
        self.assertEqual(asked, [p["number"] for p in self.prs])
        self.assertFalse(any(l.startswith("WAKE-REVIEWS") for l in sw.lines))

    def test_none_while_a_reviews_run_is_queued_or_running(self):
        for status in ("queued", "in_progress", "waiting", "requested", "pending"):
            got, gh, sw, _ = self.sweep(due={363}, reviews=[run(7, status=status)])
            self.assertEqual((got, gh.dispatched, len(gh.listed)), (0, [], 1), status)
            self.assertTrue(any(l.startswith("WAKE-REVIEWS #363") and "none started" in l for l in sw.lines), sw.lines)

    def test_a_finished_reviews_run_or_another_run_in_flight_does_not_hold_it(self):
        others = [run(1, status="completed"), run(2, title="Switchboard sweep"),
                  run(3, title="Switchboard reviews DRY run (takes no action)", status="queued")]
        _, gh, _, _ = self.sweep(due={364}, reviews=others)
        self.assertEqual(len(gh.dispatched), 1)

    def test_a_dry_run_reads_and_starts_nothing(self):
        status, gh, _, asked = self.sweep(due={361}, dry_run=True)
        self.assertEqual((status, gh.dispatched, gh.listed, asked), (0, [], [], []))

    def test_a_refused_dispatch_is_a_line_and_never_a_failed_sweep(self):
        status, gh, sw, _ = self.sweep(due={361}, refuse="gh: Resource not accessible by integration (HTTP 403)")
        self.assertEqual((status, len(gh.dispatched)), (0, 1))
        self.assertTrue(any(l.startswith("WAKE-REVIEWS no reviews run started") for l in sw.lines), sw.lines)

    def test_a_run_list_that_cannot_be_read_still_starts_the_run(self):
        class Unread(ReviewsGH):
            def route(self, path):
                if path.split("?")[0] == RUNS:
                    raise switch.GitHubError("GET", path, "gh: Server Error (HTTP 502)", 502)
                return super().route(path)

        gh = Unread(self.prs)
        with mock.patch.object(switch.Switch, "reviews_due", lambda sw, s: "the eye"):
            status, _, _ = T.sweep(gh, T.last_state(self.prs, cold=False))
        self.assertEqual((status, len(gh.dispatched)), (0, 1))

    def test_the_rate_floors_merge_path_starts_nothing(self):
        gh = ReviewsGH(self.prs)
        with mock.patch.object(switch.Switch, "reviews_due", lambda sw, s: "the eye"), \
                mock.patch.object(switch, "rate_floor", lambda limit: 10 ** 9):
            _, _, sw = T.sweep(gh, T.last_state(self.prs, cold=False))
        self.assertTrue(sw.merge_only)
        self.assertEqual((gh.dispatched, gh.listed), ([], []))


class TheTurn(unittest.TestCase):
    """Switch.reviews_due: the turn ask_pr acts on (Switch.api_turn, one decision for both), the mark not yet posted,
    and money for the call (a refused call posts nothing, so a run for it would do nothing)."""

    def due(self, f, comments=(), action="WAIT", codex=False, api=True, eye=True):
        sw = W.make(W.RecordingGH())
        sw.stale = lambda f: False
        sw.codex_pending = lambda f, c: codex
        sw.api_ok = lambda c: api
        sw.eye_ok = lambda c: eye
        sw.review_load = lambda: ({}, {})
        sw.board_comments = lambda: []
        seen = switch.Steered(f, W.ctx(comments), rules.MergeDecision(action, ("x",), ""))
        return sw.reviews_due(seen)

    def mark(self, kind):
        return {"body": switch.MARK.format(kind=kind, pr=9, head=W.HEAD), "user": {"login": rules.BOT_LOGIN},
                "created_at": W.T1}

    def test_a_green_head_the_pool_gives_the_api_waits_for_the_automatic_review(self):
        self.assertEqual(self.due(W.facts(verdicts=())), "the automatic review")

    def test_not_its_turn_or_already_posted_or_no_money(self):
        green = W.facts(verdicts=())
        self.assertEqual(self.due(green, comments=[self.mark("review")]), "")   # posted
        self.assertEqual(self.due(green, codex=True), "")                       # Codex's turn
        self.assertEqual(self.due(green, api=False), "")                        # the pool asks someone else
        self.assertEqual(self.due(W.facts(verdicts=(), proof="red")), "")       # not green
        self.assertEqual(self.due(W.facts(verdicts=(), draft=True)), "")
        self.assertEqual(self.due(W.facts()), "")                               # already landed
        self.assertEqual(self.due(green, action="MERGE"), "")                   # merged in this sweep

    def test_a_needs_eyes_head_with_no_eye_waits_for_the_eye_when_it_can_be_paid(self):
        eyes = W.facts(needs=frozenset({"eyes"}))   # reviewed already: only the eye is left
        self.assertEqual(self.due(eyes), "the eye")
        self.assertEqual(self.due(eyes, eye=False), "")                         # the cap refuses it: no empty run
        self.assertEqual(self.due(eyes, comments=[self.mark("eye")]), "")       # posted
        self.assertEqual(self.due(dataclasses.replace(eyes, proof="red")), "")

    def test_ask_pr_acts_on_the_same_turn(self):
        sw = W.make(W.RecordingGH())
        sw.stale = lambda f: False
        sw.codex_pending = lambda f, c: False
        sw.api_ok = lambda c: True
        sw.review_load = lambda: ({}, {})
        sw.board_comments = lambda: []
        reviewed = []
        sw.review = lambda facts, c, author, security=False: reviewed.append(facts.number)
        green = W.facts(verdicts=())
        seen = switch.Steered(green, W.ctx(), rules.MergeDecision("WAIT", ("x",), ""))
        self.assertEqual(sw.reviews_due(seen), "the automatic review")
        sw.ask_pr(green, W.ctx())
        self.assertEqual(reviewed, [9])


if __name__ == "__main__":
    unittest.main()
