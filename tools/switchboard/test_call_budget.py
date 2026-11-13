"""11-13: the hard per-run GitHub call budget, and the reads that ran the token's 5,000 an hour out at 14:02.

The cause, measured (a warm fast sweep, run 43436037307, and the same sweep replayed dry on trunk's code: 198
counted calls): five HUMAN-MERGE workflow PRs read every sweep, twice each (the sweep's read and the uncached
re-read), about 120 calls; six job logs gh refused to print ("terminal escape sequences"), tried 3 times each,
every sweep, 19 calls; the whole day's comment window read every sweep, 11 pages. The earlier fix (J-072,
test_sweep_calls) bounded only an idle sweep and one changed PR on a fake; nothing bounded a whole pass, so the
cost crept from 15 to 205 to 449 unseen. Now every run has a budget it cannot pass (ghapi.GitHub.budget), the
GITHUB line says what it spent of it, the CALLS line which reads, and these tests hold each cause shut.
Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
"""

from __future__ import annotations

import argparse
import datetime as dt
import pathlib
import subprocess
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import ghapi  # noqa: E402
import switch  # noqa: E402
import test_inbox as ti  # noqa: E402
import test_red_tests as trt  # noqa: E402
import test_sweep_calls as tsc  # noqa: E402

NOW = ti.NOW


def answer(status=200, body="{}", etag="", rc=0, stderr=b"") -> subprocess.CompletedProcess:
    head = f"HTTP/2.0 {status} OK\r\nx-ratelimit-remaining: 4000\r\n" + (f"etag: {etag}\r\n" if etag else "")
    return subprocess.CompletedProcess([], rc, (head + "\r\n" + body).encode(), stderr)


class Runner:
    """gh, faked: each call's arguments kept, each answer taken from `answers` in turn (the last one repeats)."""

    def __init__(self, *answers):
        self.answers, self.args = list(answers), []

    def __call__(self, args, **kw):
        self.args.append(list(args))
        return self.answers.pop(0) if len(self.answers) > 1 else self.answers[0]


def client(runner) -> ghapi.GitHub:
    return ghapi.GitHub("IssisX/ScraperX", dry_run=True, log=lambda line: None, runner=runner, sleep=lambda s: None)


class Budget(unittest.TestCase):
    def test_no_read_goes_past_the_budget_and_none_past_it_costs_a_call(self):
        runner = Runner(answer())
        gh = client(runner)
        gh.budget = 3
        for _ in range(3):
            gh.get("repos/IssisX/ScraperX/pulls/1")
        for _ in range(4):
            with self.assertRaises(ghapi.OverBudget):
                gh.get("repos/IssisX/ScraperX/pulls/1")
        self.assertEqual(len(runner.args), 3)
        self.assertEqual((gh.spent(), gh.over_budget), (3, 4))

    def test_over_budget_is_a_rate_limit_so_every_part_degrades_and_nothing_fails(self):
        self.assertTrue(issubclass(ghapi.OverBudget, ghapi.RateLimited))

    def test_a_304_is_free_against_the_budget(self):
        runner = Runner(answer(etag='W/"a"', body="[]"), answer(status=304, body=""))
        gh = client(runner)
        gh.budget = 2
        for _ in range(5):   # one counted read, then four 304s answered from the cache
            gh.get("repos/IssisX/ScraperX/pulls")
        self.assertEqual((gh.calls, gh.not_modified, gh.spent()), (5, 4, 1))
        self.assertEqual(gh.tally, {"GET pulls": 1})

    def test_a_write_still_goes_past_the_budget(self):
        runner = Runner(answer())
        gh = client(runner)
        gh.budget = 0
        gh.dry_run = False
        gh._run("POST", "repos/IssisX/ScraperX/issues/5/comments", {"body": "x"})
        self.assertEqual(len(runner.args), 1)

    def test_each_pass_has_its_budget_and_an_hour_of_them_fits_the_limit(self):
        def args(**kw):
            base = dict(part="steer", pr=0, sha="", issue=0, label="", digest=False, force_digest=False, dry_run=False)
            return argparse.Namespace(**{**base, **kw})
        b = switch.CALL_BUDGET
        self.assertEqual(switch.call_budget(args(), False, True, True), b["full"])
        self.assertEqual(switch.call_budget(args(), False, True, False), b["fast"])
        self.assertEqual(switch.call_budget(args(part="ask"), True, False, False), b["ask-fast"])
        self.assertEqual(switch.call_budget(args(dry_run=True), True, True, True), b["dry"])
        self.assertEqual(switch.call_budget(args(pr=5), False, True, True), b["one"])
        hour = b["full"] + 9 * b["fast"] + b["ask-full"] + 9 * b["ask-fast"] + 2 * b["dry"]
        self.assertLessEqual(hour, 5000 - switch.rate_floor(5000), "ten sweeps and two selftests an hour")

    def test_a_read_made_once_is_never_kept_for_its_etag(self):
        # 11-13: sweep_delta's "since the last sweep" listings and newest-first pages filled the file to its cap
        # every hour and pushed out what the full pass reads again
        once = ("repos/IssisX/ScraperX/issues/comments?since=2026-11-13T13:39:12Z&per_page=100",
                "repos/IssisX/ScraperX/actions/workflows/p.yml/runs?created=%3E%3D2026-11-13T07:39:12Z&per_page=100",
                "repos/IssisX/ScraperX/issues/comments?since=2026-11-13T12:00:00Z&sort=created&direction=desc&page=1")
        again = ("repos/IssisX/ScraperX/issues/comments?since=2026-11-13T12:00:00Z&per_page=100&page=2",
                 "repos/IssisX/ScraperX/pulls/5")
        gh = client(Runner(answer(etag='W/"a"', body="[]")))
        for path in once + again:
            gh.get(path)
        self.assertEqual(sorted(k.split("|")[0] for k in gh.etags), sorted(again))

    def test_the_breakdown_names_each_kind_of_read(self):
        self.assertEqual(ghapi.call_kind("GET", "repos/IssisX/ScraperX/pulls/488/files?per_page=100&page=2"),
                         "GET pulls/N/files")
        self.assertEqual(ghapi.call_kind("GET", "repos/IssisX/ScraperX/commits/0123abcd"), "GET commits/S")


class Hour(unittest.TestCase):
    """11-13: runs pile up (33 triggers in the hour from 14:13; ten sweeps ran, spending 271-459 each fast and 879 on
    the full pass, plus the ask part's 15-147), so each run also paces itself on the hour (switch.hour_pace)."""

    START = dt.datetime(2026, 11, 13, 14, 32, tzinfo=dt.timezone.utc)

    def hour(self, runs: int, fast_demand=459 + 147, full_demand=879 + 147) -> list[int]:
        """`runs` sweeps spread over one rate window, each wanting today's measured spend; the token's remaining
        after each sweep that was not merge-only. A cut run spends its budget and leaves the rest (OverBudget)."""
        args = argparse.Namespace(part="all", pr=0, sha="", issue=0, label="", digest=False, force_digest=False,
                                  dry_run=False)
        remaining, reset, left = 5000, int((self.START + dt.timedelta(hours=1)).timestamp()), []
        for i in range(runs):
            now = self.START + dt.timedelta(seconds=3600 * i / runs)
            full = i == 0
            want = switch.call_budget(args, True, True, full)
            budget, full = switch.hour_pace(want, full, remaining, reset, 5000, now,
                                            switch.CALL_BUDGET["fast"] + switch.CALL_BUDGET["ask-fast"], movable=False)
            if remaining <= switch.rate_floor(5000):   # run_sweep's rate floor: the merge path alone, from the reserve
                remaining -= min(20, budget)
                continue
            remaining -= min(full_demand if full else fast_demand, budget)
            left.append(remaining)
        return left

    def test_no_sweep_eats_into_the_merge_reserve_at_todays_trigger_rate_or_four_times_it(self):
        floor = switch.rate_floor(5000)
        for runs in (10, 20, 40):   # today's ten sweeps an hour; every trigger a sweep; twice that
            left = self.hour(runs)
            self.assertGreaterEqual(min(left), floor, (runs, left))
        self.assertGreater(self.hour(10)[-1], 2500, "today's rate spends under half the hour (it spent 3,414 in 80 min)")

    def test_without_the_pace_forty_runs_would_cross_the_floor(self):
        # the per-run budgets alone: a full pass and 39 fast sweeps of today's demand cut to their budgets
        b = switch.CALL_BUDGET
        spent = b["full"] + b["ask-full"] + 39 * (b["fast"] + b["ask-fast"])
        self.assertGreater(spent, 5000 - switch.rate_floor(5000))

    def test_a_full_pass_the_hour_cannot_afford_waits_unless_it_is_a_retry_or_late(self):
        reset = int((self.START + dt.timedelta(hours=1)).timestamp())
        per = switch.CALL_BUDGET["fast"] + switch.CALL_BUDGET["ask-fast"]
        budget, full = switch.hour_pace(1200, True, 1500, reset, 5000, self.START, per, movable=True)
        self.assertEqual((full, budget <= per), (False, True))
        budget, full = switch.hour_pace(1200, True, 1500, reset, 5000, self.START, per, movable=False)
        self.assertTrue(full)
        self.assertLessEqual(budget, (1500 - switch.rate_floor(5000)) // 12 + 1, "a retry goes, on the run's share")
        self.assertEqual(switch.hour_pace(1200, True, None, None, None, self.START, per), (1200, True))


class EscapedLogs(unittest.TestCase):
    """gh refused six red job logs with colour codes, 3 tries each, every sweep (run 43436037307)."""
    REFUSED = (b"the response contains terminal escape sequences; pass --allow-escape-sequences to output it anyway")

    def test_a_log_gh_will_not_print_is_read_once_more_with_the_flag(self):
        ok = subprocess.CompletedProcess([], 0, b"\x1b[31mFAIL: ammo hands\x1b[0m\n", b"")
        runner = Runner(subprocess.CompletedProcess([], 1, b"", self.REFUSED), ok)
        got = client(runner).get_bytes("repos/IssisX/ScraperX/actions/jobs/501/logs")
        self.assertIn(b"FAIL: ammo hands", got)
        self.assertEqual(len(runner.args), 2)
        self.assertNotIn(ghapi.ESCAPES_FLAG, runner.args[0])
        self.assertIn(ghapi.ESCAPES_FLAG, runner.args[1])

    def test_a_refusal_with_the_flag_too_is_not_tried_again(self):
        runner = Runner(subprocess.CompletedProcess([], 1, b"", self.REFUSED))
        with self.assertRaises(ghapi.GitHubError):
            client(runner).get_bytes("repos/IssisX/ScraperX/actions/jobs/501/logs")
        self.assertEqual(len(runner.args), 2)


class KeptBetweenSweeps(unittest.TestCase):
    def test_a_red_runs_tests_are_read_once_not_every_sweep(self):
        gh = trt.LogGH({501: trt.NATIVE_LOG, 502: trt.TOUCH_LOG, 503: trt.NATIVE_LOG},
                       runs=[ti.run(40, trt.NAME, ti.HEAD, hours=2)], jobs={40: trt.JOBS}, heads={trt.NAME: ti.HEAD})
        first = ti.make(gh).red_tests(40)
        reads = len(gh.log_reads)
        self.assertEqual(ti.make(gh, NOW + dt.timedelta(minutes=10)).red_tests(40), first)
        self.assertEqual(len(gh.log_reads), reads, "a finished run's logs never change")

    def test_an_unread_log_is_tried_a_few_sweeps_then_only_now_and_then(self):
        gh = trt.LogGH({}, runs=[ti.run(40, trt.NAME, ti.HEAD, hours=2)], jobs={40: trt.JOBS[:1]},
                       heads={trt.NAME: ti.HEAD})
        for minute in range(0, 60, 5):
            ti.make(gh, NOW + dt.timedelta(minutes=minute)).red_tests(40)
        self.assertEqual(len(gh.log_reads), switch.RED_TESTS_TRIES)
        ti.make(gh, NOW + switch.RED_TESTS_RETRY + dt.timedelta(minutes=15)).red_tests(40)
        self.assertEqual(len(gh.log_reads), switch.RED_TESTS_TRIES + 1)

    def test_the_comment_window_is_read_whole_once_then_only_since_the_last_read(self):
        old = [ti.comment(n, 292, f"post {n}", hours=5 - n / 100) for n in range(1, 120)]
        gh = ti.FakeGH(comments=old)
        got = ti.make(gh).comments_since(NOW - dt.timedelta(hours=24))
        self.assertEqual(len(got), 119)
        whole = len(gh.gets)
        gh.comments = old + [ti.comment(900, 292, "To Claude: new", hours=-0.05)]
        gh.comments[0] = dict(gh.comments[0], body="edited", updated_at=ti.ago(-0.04))
        got = ti.make(gh, NOW + dt.timedelta(minutes=5)).comments_since(NOW + dt.timedelta(minutes=5) - dt.timedelta(hours=24))
        self.assertEqual(len(gh.gets) - whole, 1, "one page: what was posted or edited since the last read")
        self.assertEqual([c["id"] for c in got][-1], 900)
        self.assertEqual(next(c for c in got if c["id"] == 1)["body"], "edited")
        self.assertEqual(len(got), 120)


class HumanMerge(unittest.TestCase):
    def test_a_human_merge_pr_is_cold_and_read_again_after_its_recheck(self):
        s = ti.human_merge(number=488)
        sw = ti.make(ti.FakeGH())
        sw.team = ti.TEAM
        rec = switch.pr_record(sw, {"number": 488, "head": {"sha": s.facts.head_sha}, "updated_at": "x"},
                               s.facts, s.ctx, s, ask=False)
        self.assertTrue(rec["cold"])
        self.assertEqual(sw._ripen[488], NOW + switch.HUMAN_RECHECK)


class WholePass(unittest.TestCase):
    """The pass budgets hold on the fake with the live repo's shape (18 open PRs): a creep in any part's reads
    fails here before it eats the hour."""

    def test_a_full_pass_and_a_fast_sweep_with_five_changed_prs_stay_inside_their_budgets(self):
        prs = [tsc.pr(n) for n in range(470, 488)]
        gh = tsc.CountingGH(prs)
        tsc.sweep(gh)                                           # cold: the full pass
        self.assertLessEqual(len(gh.calls), switch.CALL_BUDGET["full"], gh.calls)
        gh2 = tsc.CountingGH(prs)
        state = tsc.last_state(prs)
        for n in (470, 473, 476, 479, 482):
            state["prs"][str(n)]["cold"] = False
        tsc.sweep(gh2, state)
        self.assertLessEqual(len(gh2.calls), switch.CALL_BUDGET["fast"], gh2.calls)



class PaidWall(unittest.TestCase):
    """11-13: one sweep's model calls ran one after another in the run holding the shared concurrency group (run
    43459528814: five reviews, 1570 s; 43465457221: one security pass, 744 s), and every later sweep waited on them."""

    def sw(self, clock_ago=None, runs=0):
        import time
        gh = ti.FakeGH()
        sw = ti.make(gh)
        if clock_ago is not None:
            sw.paid_clock = time.monotonic() - clock_ago
        if runs:
            sw.paid_runs = runs
        sw.review_failed = lambda *a, **k: self.fail("a review put off is never a failed one")
        return sw, gh

    def facts(self):
        s = ti.steered(number=507, ref="claude/integrate-muse-git-channel")
        return s.facts, dict(s.ctx, title="t", body="b")

    def test_past_the_wall_after_one_call_the_next_review_and_eye_wait_with_no_read_and_no_mark(self):
        sw, gh = self.sw(clock_ago=switch.PAID_WALL.total_seconds() + 1, runs=1)
        facts, ctx = self.facts()
        self.assertIs(sw.review(facts, ctx, "anthropic"), False)
        self.assertIsNone(sw.eye(facts, ctx))
        self.assertEqual((gh.gets, gh.posts, gh.writes), ([], [], []))
        self.assertEqual(len([l for l in sw.lines if l.startswith("DEFER")]), 2, sw.lines)

    def test_the_first_call_always_runs_and_a_run_for_one_pr_is_never_cut(self):
        for clock_ago, runs in ((switch.PAID_WALL.total_seconds() + 60, 0),   # past the wall, nothing paid yet
                                (1, 3),                                       # inside the wall
                                (None, 9)):                                   # a --pr run sets no clock
            sw, _ = self.sw(clock_ago, runs)
            self.assertFalse(sw.paid_deferred("review#507@1a2b3c4d"), (clock_ago, runs))

    def test_a_put_off_review_keeps_its_pr_warm(self):
        sw, gh = self.sw(clock_ago=switch.PAID_WALL.total_seconds() + 1, runs=1)
        facts, ctx = self.facts()
        sw.team = ti.TEAM
        rec = switch.pr_record(sw, {"head": {"sha": facts.head_sha}, "updated_at": "x"}, facts, dict(ctx, comments=[]),
                               None, ask=True)
        self.assertFalse(rec["cold"])   # read again by the next sweep, which makes the review


if __name__ == "__main__":
    unittest.main()
