"""J-072: what one sweep costs in GitHub calls, counted on a fake GitHub.

Run 42667757904 failed on "API rate limit exceeded for installation": every sweep read every open PR in full
(about 419 calls a sweep, 395 measured dry). A sweep now reads only what changed since the last one (switch.py
sweep_delta): with nothing changed it makes a fixed handful of calls whatever the number of PRs, and one changed PR
adds only that PR's own reads. The real switch runs here (run_sweep, sweep_delta, pr_facts, steer_pr, the inbox,
Muse); only GitHub is fake, and every read and write it is asked for is counted.
Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
"""

from __future__ import annotations

import argparse
import datetime as dt
import pathlib
import re
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import switch  # noqa: E402
import test_inbox  # noqa: E402

NOW = test_inbox.NOW
REPO = test_inbox.FakeGH.repo
IDLE_MAX = 15        # the coordinator's bound for a sweep where nothing changed
FIRST_PR_MAX = 16    # one changed PR: its eight reads, the shared job and board reads, the inbox's whole-sweep reads
PER_PR_MAX = 8       # each further changed PR: pr_facts' own reads (the PR, its commit, proof run and jobs, comments,
                     # reviews, files, job issue comments)


def iso(t: dt.datetime) -> str:
    return t.strftime("%Y-%m-%dT%H:%M:%SZ")


def pr(n: int, updated: str = "2026-11-06T18:00:00Z") -> dict:
    return {"number": n, "title": f"J-{n:03d} work", "body": f"Job: J-{n:03d}\n", "draft": False, "labels": [],
            "updated_at": updated, "head": {"sha": f"{n:040x}", "ref": f"fb-bot/integrate-j{n:03d}"},
            "base": {"ref": "ScraperX-Claude"}, "user": {"login": "IssisX"}}


class CountingGH(test_inbox.FakeGH):
    """test_inbox's fake, plus the per-PR reads pr_facts makes, and every call counted (reads, writes, comments)."""

    def __init__(self, prs, **kw):
        super().__init__(open_prs=prs, dry_run=True, **kw)
        self.calls: list[str] = []

    def route(self, path):
        self.calls.append(path)
        base = path.split("?")[0]
        m = re.fullmatch(rf"repos/{REPO}/pulls/(\d+)", base)
        if m:
            return {"number": int(m.group(1)), "mergeable": True}
        if re.fullmatch(rf"repos/{REPO}/pulls/\d+/(reviews|files)", base):
            return []
        m = re.fullmatch(rf"repos/{REPO}/commits/([0-9a-f]+)", base)
        if m:
            return {"sha": m.group(1), "commit": {"message": "x", "committer": {"date": "2026-11-06T17:00:00Z"}}}
        if base == "rate_limit":
            return {"resources": {"core": {"limit": 1000, "remaining": 900, "reset": 1794250000}}}
        if base == f"repos/{REPO}/issues" and "labels=job" in path:
            return []
        if base == f"repos/{REPO}/branches":
            return []
        return super().route(path)

    def comment(self, number, body, what=""):
        self.calls.append(f"POST issues/{number}/comments")
        return super().comment(number, body, what)

    def write(self, method, path, body=None, what=""):
        self.calls.append(f"{method} {path}")
        return super().write(method, path, body, what)


def sweep(gh, state=None):
    sw = test_inbox.make(gh)
    sw.root = HERE.parents[1]
    sw.merge_token = sw.review_key = ""
    sw._jobs = None
    sw._board = None
    gh.state = {"steer": state} if state is not None else {}
    gh.remaining = gh.limit = gh.reset = None
    args = argparse.Namespace(part="steer", pr=0, sha="", issue=0, label="", digest=False, force_digest=False,
                              dry_run=True)
    read = []
    real = sw.pr_facts
    sw.pr_facts = lambda p: (read.append(p["number"]), real(p))[1]
    status = switch.run_sweep(sw, args, NOW)
    return status, read, sw


def last_state(prs, minutes=10, cold=True) -> dict:
    stamp = iso(NOW - dt.timedelta(minutes=minutes))
    return {"stamp": stamp, "full_at": stamp,
            "prs": {str(p["number"]): {"head": p["head"]["sha"], "updated": p["updated_at"], "job_issue": None,
                                       "cold": cold} for p in prs}}


class SweepCalls(unittest.TestCase):
    def setUp(self):
        self.prs = [pr(n) for n in range(361, 368)]   # the 7 open PRs of the live sweep measured on 11-09

    def test_a_sweep_with_nothing_changed_makes_at_most_15_calls(self):
        gh = CountingGH(self.prs)
        status, read, _ = sweep(gh, last_state(self.prs))
        self.assertEqual(status, 0)
        self.assertEqual(read, [], "an unchanged PR is not read")
        self.assertLessEqual(len(gh.calls), IDLE_MAX, gh.calls)
        # the PR listing, the proof runs and the comments since the last sweep (and the free rate read)
        self.assertEqual(len(gh.calls), 4, gh.calls)

    def test_the_idle_cost_does_not_grow_with_the_open_prs(self):
        many = [pr(n) for n in range(400, 460)]
        gh = CountingGH(many)
        sweep(gh, last_state(many))
        self.assertEqual(len(gh.calls), 4, gh.calls)

    def test_one_changed_pr_adds_only_its_own_reads(self):
        idle = CountingGH(self.prs)
        sweep(idle, last_state(self.prs))
        prs = [dict(p) for p in self.prs]
        prs[2] = dict(prs[2], updated_at="2026-11-06T19:55:00Z")   # a push, a label, a review or a PR comment
        gh = CountingGH(prs)
        status, read, _ = sweep(gh, last_state(self.prs))
        self.assertEqual(status, 0)
        self.assertEqual(read, [363])
        self.assertLessEqual(len(gh.calls) - len(idle.calls), FIRST_PR_MAX, gh.calls)
        two = [dict(p) for p in prs]
        two[4] = dict(two[4], head={"sha": "f" * 40, "ref": two[4]["head"]["ref"]})   # a new head
        gh2 = CountingGH(two)
        sweep(gh2, last_state(self.prs))
        per_pr = len(gh2.calls) - len(gh.calls)
        self.assertLessEqual(per_pr, PER_PR_MAX, gh2.calls)
        self.assertGreater(per_pr, 0)

    def test_a_comment_on_a_pr_or_its_job_issue_reads_that_pr(self):
        state = last_state(self.prs)
        state["prs"]["365"]["job_issue"] = 290
        posted = iso(NOW - dt.timedelta(minutes=2))
        for thread, want in ((364, [364]), (290, [365])):
            gh = CountingGH(self.prs, comments=[{"id": 1, "body": "To Muse: see this", "created_at": posted,
                                                 "updated_at": posted,
                                                 "issue_url": f"https://api.github.com/repos/{REPO}/issues/{thread}"}])
            _, read, _ = sweep(gh, state)
            self.assertEqual(read, want)

    def test_a_finished_proof_run_reads_its_pr(self):
        finished = iso(NOW - dt.timedelta(minutes=1))
        run = {"id": 9, "head_branch": "fb-bot/integrate-j366", "head_sha": f"{366:040x}", "status": "completed",
               "conclusion": "success", "created_at": iso(NOW - dt.timedelta(minutes=40)), "updated_at": finished,
               "html_url": "https://example.invalid/run/9"}
        gh = CountingGH(self.prs, runs=[run])
        _, read, _ = sweep(gh, last_state(self.prs))
        self.assertEqual(read, [366])

    def test_a_pr_waiting_on_the_clock_is_read_every_sweep(self):
        # its last decision was not COLD (a Codex wait, the review fallback, a merge): time alone can change it
        gh = CountingGH(self.prs)
        _, read, _ = sweep(gh, last_state(self.prs, cold=False))
        self.assertEqual(read, [p["number"] for p in self.prs])

    def test_a_full_pass_every_30_minutes_and_on_a_cold_cache(self):
        for state in (None, last_state(self.prs, minutes=31)):
            gh = CountingGH(self.prs)
            _, read, sw = sweep(gh, state)
            self.assertEqual(read, [p["number"] for p in self.prs])
            self.assertTrue(switch.sweep_state_ok(gh.state["steer"]))
            self.assertEqual(gh.state["steer"]["full_at"], iso(NOW))
        stale = last_state(self.prs, minutes=31)
        stale["full_at"] = iso(NOW - dt.timedelta(minutes=31))
        stale["stamp"] = iso(NOW - dt.timedelta(minutes=5))
        gh = CountingGH(self.prs)
        _, read, _ = sweep(gh, stale)
        self.assertEqual(len(read), len(self.prs))

    def test_a_fast_sweep_keeps_the_full_pass_stamp_and_every_record(self):
        state = last_state(self.prs)
        state["full_at"] = iso(NOW - dt.timedelta(minutes=20))
        gh = CountingGH(self.prs)
        sweep(gh, state)
        saved = gh.state["steer"]
        self.assertEqual(saved["full_at"], state["full_at"])
        self.assertEqual(saved["stamp"], iso(NOW))
        self.assertEqual(set(saved["prs"]), {str(p["number"]) for p in self.prs})

    def test_a_closed_pr_drops_out_of_the_state(self):
        gh = CountingGH(self.prs[1:])
        sweep(gh, last_state(self.prs))
        self.assertNotIn(str(self.prs[0]["number"]), gh.state["steer"]["prs"])

    def test_a_page_of_comments_or_more_reads_everything(self):
        posted = iso(NOW - dt.timedelta(minutes=2))
        many = [{"id": i, "body": "x", "created_at": posted, "updated_at": posted,
                 "issue_url": f"https://api.github.com/repos/{REPO}/issues/1"} for i in range(100)]
        gh = CountingGH(self.prs, comments=many)
        _, read, _ = sweep(gh, last_state(self.prs))
        self.assertEqual(len(read), len(self.prs))


if __name__ == "__main__":
    unittest.main()
