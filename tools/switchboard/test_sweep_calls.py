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
IDLE = 5             # what one makes now
TRUNK_BRIDGE = switch.bridge_version((HERE / "muse_bridge.gs").read_text(encoding="utf-8"))
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

    def __init__(self, prs, dry_run=True, wake=(), **kw):
        super().__init__(open_prs=prs, dry_run=dry_run, **kw)
        self.calls: list[str] = []
        self.wake = list(wake)   # the open issues labeled wake:muse

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
        if base == f"repos/{REPO}/issues" and "labels=wake" in path:
            return [{"number": n} for n in self.wake]
        if re.fullmatch(rf"repos/{REPO}/issues/\d+/events", base):
            return [{"event": "labeled", "label": {"name": switch.MUSE_LABEL}, "created_at": iso(NOW - dt.timedelta(minutes=5))}]
        return super().route(path)

    def comment(self, number, body, what=""):
        self.calls.append(f"POST issues/{number}/comments")
        return super().comment(number, body, what)

    def write(self, method, path, body=None, what=""):
        self.calls.append(f"{method} {path}")
        return super().write(method, path, body, what)


def sweep(gh, state=None, now=NOW):
    sw = test_inbox.make(gh, now)
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
    status = switch.run_sweep(sw, args, now)
    return status, read, sw


def last_state(prs, minutes=10, cold=True) -> dict:
    stamp = iso(NOW - dt.timedelta(minutes=minutes))
    return {"stamp": stamp, "full_at": stamp,
            "prs": {str(p["number"]): {"head": p["head"]["sha"], "updated": p["updated_at"], "job_issue": None,
                                       "cold": cold} for p in prs},
            # the full pass found the pipe healthy: trunk's bridge seen, no wake:muse label on, no clock running
            "timers": {"pipe": {"next_at": None, "version_at": iso(NOW + dt.timedelta(minutes=20)), "live": TRUNK_BRIDGE,
                                "wake": []}}}


class SweepCalls(unittest.TestCase):
    def setUp(self):
        self.prs = [pr(n) for n in range(361, 368)]   # the 7 open PRs of the live sweep measured on 11-09

    def test_a_sweep_with_nothing_changed_makes_at_most_15_calls(self):
        gh = CountingGH(self.prs)
        status, read, _ = sweep(gh, last_state(self.prs))
        self.assertEqual(status, 0)
        self.assertEqual(read, [], "an unchanged PR is not read")
        self.assertLessEqual(len(gh.calls), IDLE_MAX, gh.calls)
        # the PR listing, the proof runs and the comments since the last sweep, the wake:muse listing (pipe health's
        # one trigger no comment shows), and the free rate read
        self.assertEqual(len(gh.calls), IDLE, gh.calls)

    def test_the_idle_cost_does_not_grow_with_the_open_prs(self):
        many = [pr(n) for n in range(400, 460)]
        gh = CountingGH(many)
        sweep(gh, last_state(many))
        self.assertEqual(len(gh.calls), IDLE, gh.calls)

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


RED_ITEM_MAX = 6     # a ripe RED-BRANCH: its branch head, its proof's jobs, the inbox's two marker reads, the post


class Clocks(unittest.TestCase):
    """J-072, the coordinator on #360: no fix makes anything slower. A timer ripens on the clock, not on a read, so
    every fast sweep checks the full-pass parts' clocks against the facts the last read cached; only the item a
    ripe clock is about is read, never the whole pass."""

    def setUp(self):
        self.prs = [pr(n) for n in range(361, 368)]

    def test_a_red_branch_ripens_on_a_fast_sweep_with_no_full_pass(self):
        name = "claude/integrate-j099-thing"
        red = test_inbox.run(40, name, test_inbox.HEAD, hours=0.5)   # red half an hour ago: not an item yet
        red["created_at"] = iso(NOW - dt.timedelta(minutes=50))
        red["updated_at"] = iso(NOW - dt.timedelta(minutes=30))
        first = CountingGH(self.prs, dry_run=False, runs=[red], jobs={40: test_inbox.RED_JOBS},
                           heads={name: test_inbox.HEAD})
        _, read, _ = sweep(first, None)                    # a full pass: the inbox runs and finds it not ripe
        self.assertEqual(len(read), len(self.prs))
        state = first.state["steer"]
        self.assertEqual([r["head_branch"] for r in state["timers"]["red"]], [name])
        self.assertFalse(any("RED-BRANCH" in body for _, body in first.posts))

        later = NOW + dt.timedelta(minutes=31)             # the hour is up 1 minute ago; the full pass is not due
        state = dict(state, full_at=iso(NOW + dt.timedelta(minutes=5)), stamp=iso(NOW + dt.timedelta(minutes=21)))
        idle = CountingGH(self.prs, dry_run=False, runs=[red], jobs={40: test_inbox.RED_JOBS},
                          heads={name: test_inbox.HEAD})
        _, read, _ = sweep(idle, state, now=later)
        self.assertEqual(read, [], "no PR is read: nothing about them changed")
        posts = [body for n, body in idle.posts if n == switch.INBOX_PR]
        self.assertEqual(len(posts), 1)
        self.assertIn(f"RED-BRANCH `{name}` head 1a2b3c4d", posts[0])
        self.assertLessEqual(len(idle.calls), IDLE + RED_ITEM_MAX, idle.calls)
        self.assertEqual(idle.state["steer"]["full_at"], state["full_at"], "no full pass ran")
        self.assertEqual(idle.state["steer"]["timers"]["red"], [])

    def test_a_red_branch_not_yet_ripe_costs_nothing(self):
        red = test_inbox.run(40, "claude/integrate-j099-thing", test_inbox.HEAD, hours=0.5)
        state = last_state(self.prs)
        state["timers"]["red"] = [red]
        gh = CountingGH(self.prs)
        sweep(gh, state)
        self.assertEqual(len(gh.calls), IDLE, gh.calls)
        self.assertEqual(gh.state["steer"]["timers"]["red"], [red])

    def ledger_timer(self, minutes_ago=1, thread=290):
        import ledger
        it = ledger.Item("JOB", f"#{thread}", "", "J-099", None, ledger.Person("Builder", "builder"), "claimed, no PR",
                         NOW - dt.timedelta(hours=4, minutes=minutes_ago), dt.timedelta(hours=4), "v1", thread)
        return ledger, it, {"at": iso(NOW - dt.timedelta(minutes=minutes_ago)), "item": ledger.item_json(it)}

    def test_a_ledger_deadline_is_sent_on_a_fast_sweep_with_no_read(self):
        ledger, it, timer = self.ledger_timer()
        state = last_state(self.prs)
        state["timers"]["ledger"] = [timer]
        gh = CountingGH(self.prs, dry_run=False)
        sweep(gh, state)
        (number, body), = gh.posts
        self.assertEqual(number, 290)
        self.assertTrue(body.startswith("To Builder:"), body)
        self.assertIn(it.key(1), body)                                       # its marker: the full pass sees it sent
        self.assertEqual(len(gh.calls), IDLE + 1, gh.calls)                 # the post, and no read
        left, = gh.state["steer"]["timers"]["ledger"]                        # it waits for stage 2 now
        self.assertEqual(left["at"], iso(it.clock + 2 * it.deadline))

    def test_a_ledger_deadline_on_a_changed_item_runs_the_ledger_in_full(self):
        from unittest import mock
        _, _, timer = self.ledger_timer(thread=364)
        state = last_state(self.prs)
        state["timers"]["ledger"] = [timer]
        posted = iso(NOW - dt.timedelta(minutes=2))
        gh = CountingGH(self.prs, comments=[{"id": 1, "body": "pushed a fix", "created_at": posted, "updated_at": posted,
                                             "issue_url": f"https://api.github.com/repos/{REPO}/issues/364"}])
        with mock.patch("ledger.sweep") as full:
            full.return_value.pending = []
            sweep(gh, state)
        full.assert_called_once()

    def test_a_ledger_deadline_not_yet_due_costs_nothing(self):
        _, _, timer = self.ledger_timer(minutes_ago=-20)
        state = last_state(self.prs)
        state["timers"]["ledger"] = [timer]
        gh = CountingGH(self.prs)
        sweep(gh, state)
        self.assertEqual(len(gh.calls), IDLE, gh.calls)

    def test_pipe_health_runs_when_its_clock_or_a_trigger_says_so(self):
        cases = {
            "a label's 20 minutes are up": lambda st, kw: st["timers"]["pipe"].update(next_at=iso(NOW - dt.timedelta(minutes=1))),
            "trunk's bridge not seen for 25 minutes": lambda st, kw: st["timers"]["pipe"].update(
                version_at=iso(NOW - dt.timedelta(minutes=1))),
            "a new wake:muse label": lambda st, kw: kw.update(wake=[290]),
            "a CLAUDE_TO_MUSE ask": lambda st, kw: kw.update(comments=[{
                "id": 1, "body": "CLAUDE_TO_MUSE: J-099 the goal", "created_at": iso(NOW - dt.timedelta(minutes=1)),
                "updated_at": iso(NOW - dt.timedelta(minutes=1)),
                "issue_url": f"https://api.github.com/repos/{REPO}/issues/290"}]),
        }
        for why, setup in cases.items():
            with self.subTest(why):
                state, kw = last_state(self.prs), {}
                setup(state, kw)
                gh = CountingGH(self.prs, **kw)
                sweep(gh, state)
                self.assertTrue(any(c.endswith(f"issues/{switch.BOARD_ISSUE}/comments") for c in gh.calls), gh.calls)

    def test_the_bridge_clocks_own_sweep_needs_no_read(self):
        import os
        from unittest import mock
        state = last_state(self.prs)
        state["timers"]["pipe"]["version_at"] = iso(NOW + dt.timedelta(minutes=1))
        gh = CountingGH(self.prs)
        with mock.patch.dict(os.environ, {switch.BRIDGE_VERSION_ENV: TRUNK_BRIDGE}):
            sweep(gh, state)
        self.assertEqual(len(gh.calls), IDLE, gh.calls)
        self.assertEqual(gh.state["steer"]["timers"]["pipe"]["version_at"], iso(NOW + switch.BRIDGE_SEEN +
                                                                                  dt.timedelta(seconds=1)))
        gh = CountingGH(self.prs)                          # a pasted bridge of another version: checked at once
        with mock.patch.dict(os.environ, {switch.BRIDGE_VERSION_ENV: "0123456789ab"}):
            sweep(gh, last_state(self.prs))
        self.assertTrue(any("switchboard.yml/runs" in c for c in gh.calls), gh.calls)

    def test_a_pr_inbox_clock_reads_a_cold_pr_when_it_ripens(self):
        state = last_state(self.prs)
        state["prs"]["362"]["ripen"] = iso(NOW - dt.timedelta(minutes=1))   # its QUIET or RED-STUCK hour
        state["prs"]["363"]["ripen"] = iso(NOW + dt.timedelta(minutes=30))
        gh = CountingGH(self.prs)
        _, read, _ = sweep(gh, state)
        self.assertEqual(read, [362])
        self.assertEqual(gh.state["steer"]["prs"]["363"]["ripen"], state["prs"]["363"]["ripen"])

    def test_the_inbox_notes_each_pr_clock(self):
        sw = test_inbox.make(test_inbox.FakeGH())
        s = test_inbox.steered(proof="none")
        s.ctx["updated_at"] = iso(NOW - dt.timedelta(hours=1))
        sw.pr_inbox_items(s, [], frozenset())
        self.assertEqual(sw._ripen[s.facts.number], NOW + dt.timedelta(hours=2))   # QUIET at 3 hours untouched


class LedgerTimers(unittest.TestCase):
    def test_the_full_pass_keeps_the_next_notice_of_each_item(self):
        import ledger
        builder = ledger.Person("Builder", "builder")
        due_later = ledger.Item("JOB", "#1", "", "", None, builder, "s", NOW - dt.timedelta(hours=1), dt.timedelta(hours=4), "a", 1)
        past_sent = ledger.Item("JOB", "#2", "", "", None, builder, "s", NOW - dt.timedelta(hours=5), dt.timedelta(hours=4), "b", 2)
        past_unsent = ledger.Item("JOB", "#3", "", "", None, builder, "s", NOW - dt.timedelta(hours=5), dt.timedelta(hours=4), "c", 3)
        board = ledger.Item("JOB", "#4", "", "", None, ledger.SWITCHBOARD, "s", NOW - dt.timedelta(hours=1), dt.timedelta(hours=4), "d", 4)
        got = {t["item"]["ref"]: t["at"] for t in ledger.pending([due_later, past_sent, past_unsent, board], NOW,
                                                                {past_sent.key(1)})}
        self.assertEqual(got, {"#1": iso(NOW + dt.timedelta(hours=3)), "#2": iso(NOW + dt.timedelta(hours=3)),
                               "#3": iso(NOW)})   # #3: owed now (past MAX_NOTICES or a failed post)
        back = ledger.item_of(ledger.item_json(due_later))
        self.assertEqual(back, due_later)
