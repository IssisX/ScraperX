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
import contextlib
import copy
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
IDLE = 8             # what one makes now. 11-13 (J-072): was 5; +2 the running proof runs (a re-run keeps its old
                     # created_at, so only these show it finish), +1 the open job issues (their needs: labels)
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


def job_issue(n: int, job: str, labels=()) -> dict:
    return {"number": n, "title": f"{job} work", "state": "open", "labels": [{"name": l} for l in labels],
            "user": {"login": "IssisX"}}


class CountingGH(test_inbox.FakeGH):
    """test_inbox's fake, plus the per-PR reads pr_facts makes, and every call counted (reads, writes, comments)."""

    def __init__(self, prs, dry_run=True, wake=(), **kw):
        super().__init__(open_prs=prs, dry_run=dry_run, **kw)
        self.calls: list[str] = []
        self.wake = list(wake)   # the open issues labeled wake:muse

    fail_once: tuple = ()   # path prefixes whose first read fails (a transient 403)
    job_issues: tuple = ()  # the open issues labeled `job`

    def route(self, path):
        self.calls.append(path)
        base = path.split("?")[0]
        for prefix in self.fail_once:
            if base.startswith(prefix) and prefix not in self.__dict__.setdefault("_failed", set()):
                self._failed.add(prefix)
                raise switch.GitHubError("GET", path, "gh: Resource not accessible (HTTP 403)", 403)
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
            return list(self.job_issues)
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


def sweep(gh, state=None, now=NOW, whole=None, plan_mode="off", merge_token=""):
    """One sweep. `plan_mode` is what work/switchboard.yml says (`plan`): off, since the plan part has its own sweep
    tests (test_plan_sweep)."""
    sw = test_inbox.make(gh, now)
    sw.root = HERE.parents[1]
    sw.merge_token, sw.review_key = merge_token, ""
    sw._jobs = None
    sw._board = None
    sw.plan_mode = lambda: plan_mode
    gh.state = whole if whole is not None else {"steer": state} if state is not None else {}
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
            # the baselines the last sweep saved (J-072, 11-13): no proof run running, these trunk inputs, no open job
            "running": {}, "inputs": switch.trunk_inputs(HERE.parents[1]) + ":00", "jobs": [],   # no token, no key
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
            gh.job_issues = (job_issue(290, "J-365"),)   # the job issue the record names is open (11-13: it is compared)
            _, read, _ = sweep(gh, state)
            self.assertEqual(read, want)

    def test_the_switchs_own_post_off_a_pr_reads_no_pr(self):
        # 11-13: its merged notes on #350 re-read all six of that job's drafts on a dry pass of the live repo
        state = last_state(self.prs)
        state["prs"]["365"]["job_issue"] = 290
        posted = iso(NOW - dt.timedelta(minutes=2))
        for thread, want in ((290, []), (364, [364])):   # on the job issue (naming #365 too) / on the PR itself
            gh = CountingGH(self.prs, comments=[{"id": 1, "body": "Switchboard: merged #365's sibling, see #365",
                                                 "user": {"login": switch.rules.BOT_LOGIN}, "created_at": posted,
                                                 "updated_at": posted,
                                                 "issue_url": f"https://api.github.com/repos/{REPO}/issues/{thread}"}])
            gh.job_issues = (job_issue(290, "J-365"),)
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

    def test_a_full_pass_every_hour_and_on_a_cold_cache(self):
        # J-072: every clock is checked each sweep, so the full pass only bounds how late the ledger finds a new
        # input; its shortest deadline is 2 hours from the input's own time
        self.assertEqual(switch.FULL_EVERY, dt.timedelta(minutes=60))
        for state in (None, last_state(self.prs, minutes=61)):
            gh = CountingGH(self.prs)
            _, read, sw = sweep(gh, state)
            self.assertEqual(read, [p["number"] for p in self.prs])
            self.assertTrue(switch.sweep_state_ok(gh.state["steer"]))
            self.assertEqual(gh.state["steer"]["full_at"], iso(NOW))
        stale = last_state(self.prs, minutes=61)
        stale["stamp"] = iso(NOW - dt.timedelta(minutes=5))
        gh = CountingGH(self.prs)
        _, read, _ = sweep(gh, stale)
        self.assertEqual(len(read), len(self.prs))
        within = last_state(self.prs, minutes=59)
        gh = CountingGH(self.prs)
        _, read, _ = sweep(gh, within)
        self.assertEqual(read, [])

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
        """A timer of the deleted ledger (ledger.py item_json), as an old cache may still hold one."""
        it = {"kind": "JOB", "ref": f"#{thread}", "link": "", "job": "J-099", "owner": None,
              "holder": {"name": "Builder", "agent": "builder"}, "state": "claimed, no PR",
              "clock": iso(NOW - dt.timedelta(hours=4, minutes=minutes_ago)), "deadline": 14400.0, "version": "v1",
              "thread": thread, "touched": None, "unmatched": ""}
        return {"at": iso(NOW - dt.timedelta(minutes=minutes_ago)), "item": it}

    def test_a_ripe_ledger_deadline_an_old_cache_holds_posts_nothing_and_its_clock_is_dropped(self):
        # D2: the ledger left the sweep; a ledger timer the cache still holds is never fired, and it is dropped
        timer = self.ledger_timer()
        state = last_state(self.prs)
        state["timers"]["ledger"] = [timer]
        gh = CountingGH(self.prs, dry_run=False)
        sweep(gh, state)
        self.assertEqual(gh.posts, [])
        self.assertEqual(len(gh.calls), IDLE, gh.calls)
        self.assertNotIn("ledger", gh.state["steer"]["timers"])

    def test_a_ledger_deadline_not_yet_due_costs_nothing(self):
        timer = self.ledger_timer(minutes_ago=-20)
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


class WarmFullPass(unittest.TestCase):
    """J-072: a warm full pass answered only 270 of 405 reads with 304; these are the reads that came back 200."""

    def test_the_comment_window_repeats_for_hours(self):
        # an hour's floor changed at every hourly full pass, so it never answered 304; 6 hours repeats
        paths = []
        for minutes in (2, 50, 110, 230):
            gh = CountingGH([])
            sw = test_inbox.make(gh, NOW + dt.timedelta(minutes=minutes))
            sw.ask_inbox_items(frozenset())
            paths.append([c for c in gh.calls if "issues/comments" in c])
        self.assertEqual(paths.count(paths[0]), 4, paths)
        self.assertIn("since=2026-11-05T18:00:00Z", paths[0][0])

    def test_the_hours_floor_reads_no_extra_ask(self):
        early = {"id": 1, "body": "To Claude: an old ask", "created_at": "2026-11-05T20:10:00Z",
                 "updated_at": "2026-11-05T20:10:00Z", "html_url": "u1", "user": {"login": "IssisX"},
                 "issue_url": f"https://api.github.com/repos/{REPO}/issues/290"}
        gh = CountingGH([], comments=[early], open_issues=[290])
        sw = test_inbox.make(gh, NOW + dt.timedelta(minutes=30))   # the window starts 20:30; the read starts 20:00
        self.assertEqual(sw.ask_inbox_items(frozenset()), [])

    def test_inbound_quiet_shares_the_inboxs_read(self):
        gh = CountingGH([])
        sw = test_inbox.make(gh)
        sw.ask_inbox_items(frozenset())
        sw.inbound_quiet(None)
        self.assertEqual(sum("issues/comments" in c for c in gh.calls), 1, gh.calls)

    def test_a_superseded_line_needs_no_read_per_pr(self):
        gh = CountingGH([pr(43), pr(100)], dry_run=False)
        sw = test_inbox.make(gh)
        facts = type("F", (), {"number": 100, "draft": False})()
        sw.close_superseded(facts, {"body": "Supersedes #43, #44 and #9"})
        self.assertEqual(gh.calls, [f"repos/{REPO}/pulls?state=open", "POST issues/43/comments",
                                    f"PATCH repos/{REPO}/pulls/43"])

    def test_the_cache_holds_a_warm_full_passs_reads(self):
        import ghapi
        import os
        import tempfile
        gh = ghapi.GitHub(REPO, True)
        gh.etags = {f"k{i}|": {"etag": '"e"', "body": "x" * 50_000, "t": 1.0} for i in range(500)}   # 25 MB
        with tempfile.TemporaryDirectory() as d:
            gh.save_etags(os.path.join(d, "e.json"), now=2.0)
            back = ghapi.GitHub(REPO, True)
            back.load_etags(os.path.join(d, "e.json"))
        self.assertEqual(len(back.etags), 500)   # at the old 20 MB cap, 51 prior-PR entries were lost

    def test_a_head_that_had_a_pr_is_not_asked_again(self):
        gh = CountingGH([])
        gh.state = {}
        calls = []
        old, new = "a" * 40, "b" * 40
        gh.paged = lambda path, key="": (calls.append(path),
                                         [{"number": 5, "head": {"sha": old}}] if "had-one" in path else [])[1]
        sw = test_inbox.make(gh)
        for _ in range(3):
            self.assertTrue(sw.prior_pr("fb-bot/integrate-had-one", old))
            self.assertFalse(sw.prior_pr("fb-bot/integrate-never", old))
        self.assertEqual(sum("had-one" in c for c in calls), 1)   # a yes is kept; a no is asked each time
        self.assertEqual(sum("never" in c for c in calls), 3)
        self.assertEqual(gh.state["prior_pr"], [f"fb-bot/integrate-had-one@{old}"])
        self.assertFalse(sw.prior_pr("fb-bot/integrate-had-one", new))   # SYSTEM_PLAN item 2: asked of the head
        self.assertEqual(sum("had-one" in c for c in calls), 2)



class Retry(unittest.TestCase):
    """ChatGPT on #360 @ a1a67229: a full pass whose ledger or inbox read failed was saved as done (stamp and full_at
    now, the timers empty), so the work waited up to an hour for the next full pass. A part that fails or is skipped
    is now due again at the very next sweep, and its old timers stay until a good run replaces them."""

    def setUp(self):
        self.prs = [pr(n) for n in range(361, 368)]

    def carry(self, gh, key="review-key"):
        """The run's state through the signed cache file, as main() saves it in `finally` and the next run loads it."""
        import ghapi
        import os
        import tempfile
        out = ghapi.GitHub(REPO, True, log=lambda line: None)
        out.set_cache_key(key)
        out.state = gh.state
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "etags.json")
            out.save_etags(path)
            back = ghapi.GitHub(REPO, True, log=lambda line: None)
            back.set_cache_key(key)
            back.load_etags(path)
        self.assertTrue(back.state, "the signed file was read back")
        return back.state

    def test_a_failed_inbox_read_is_retried_at_the_next_quiet_sweep(self):
        name = "claude/integrate-j099-thing"
        red = test_inbox.run(40, name, test_inbox.HEAD, hours=2)   # red two hours ago: an item now

        def gh_now(**kw):
            return CountingGH(self.prs, dry_run=False, runs=[red], jobs={40: test_inbox.RED_JOBS},
                              heads={name: test_inbox.HEAD}, **kw)

        first = gh_now()
        first.fail_once = (f"repos/{REPO}/issues/{switch.INBOX_PR}",)   # the inbox's marker read
        sweep(first, None)
        self.assertFalse([b for n, b in first.posts if n == switch.INBOX_PR])
        retry_mark = first.state["steer"].get("retry") or []
        state = self.carry(first)
        inbox_posts = []
        for minutes in (10, 20):
            gh = gh_now()
            _, read, _ = sweep(gh, now=NOW + dt.timedelta(minutes=minutes), whole=state)
            self.assertEqual(read, [], "a fast sweep: no PR changed")
            inbox_posts += [b for n, b in gh.posts if n == switch.INBOX_PR]
            state = self.carry(gh)
        self.assertEqual(len(inbox_posts), 1, inbox_posts)
        self.assertIn(f"RED-BRANCH `{name}`", inbox_posts[0])
        self.assertIn("inbox", retry_mark)

    def test_a_failed_full_pass_part_makes_the_next_sweep_a_full_pass_once(self):
        # was the ledger's retry test (D2): auto-open is a full-pass part, so its retry needs every PR read again
        from unittest import mock
        calls = []

        def auto_open_once(only):
            calls.append(only)
            if len(calls) == 1:
                raise switch.GitHubError("GET", f"repos/{REPO}/branches", "gh: (HTTP 403)", 403)
            return 0

        full_at = []
        state = None
        with mock.patch.object(switch.Switch, "auto_open_prs", side_effect=auto_open_once):
            for minutes in (0, 10, 20):                              # the hour's full pass is not due again
                gh = CountingGH(self.prs, dry_run=False)
                status, _, _ = sweep(gh, now=NOW + dt.timedelta(minutes=minutes), whole=state)
                if minutes == 0:
                    self.assertEqual(status, 2, "the auto-open part failed: the run exits non-zero")
                    self.assertEqual(gh.state["steer"]["retry"], ["auto-open"])
                full_at.append(gh.state["steer"]["full_at"])
                state = self.carry(gh)
        retried = iso(NOW + dt.timedelta(minutes=10))
        self.assertEqual(full_at, [iso(NOW), retried, retried], "a full pass at the very next sweep, then not again")
        self.assertEqual(calls, [None, None], "auto-open ran again over every branch, once")
        self.assertEqual(state["steer"]["retry"], [])

    def test_a_failed_part_keeps_its_old_timers(self):
        # was the ledger's (D2): a full pass whose auto-open or one PR read failed keeps every cached timer no good
        # run replaced, here the heads already re-run (J-103), so nothing is lost before the retry
        from unittest import mock
        heads = ["a" * 40, "b" * 40]

        def fails(only):
            raise switch.GitHubError("GET", f"repos/{REPO}/branches", "gh: (HTTP 403)", 403)

        for failing in ("auto-open", "prs"):
            with self.subTest(failing=failing):
                state = last_state(self.prs, minutes=61)                # a full pass is due
                state["timers"]["rerun"] = list(heads)
                gh = CountingGH(self.prs)
                if failing == "prs":
                    gh.fail_once = (f"repos/{REPO}/pulls/{self.prs[0]['number']}",)
                    sweep(gh, state)
                else:
                    with mock.patch.object(switch.Switch, "auto_open_prs", side_effect=fails):
                        sweep(gh, state)
                saved = gh.state["steer"]
                self.assertEqual(saved["retry"], [failing])
                self.assertEqual(saved["timers"].get("rerun"), heads)

    def test_under_the_rate_floor_nothing_is_marked_done(self):
        class Low(CountingGH):
            def route(self, path):
                if path == "rate_limit":
                    self.calls.append(path)
                    return {"resources": {"core": {"limit": 1000, "remaining": 100, "reset": 1794250000}}}
                return super().route(path)
        state = last_state(self.prs, minutes=61)
        gh = Low(self.prs)
        whole = {"steer": state}
        sweep(gh, whole=whole)
        self.assertIs(gh.state["steer"], state, "the merge path only: the state is left as it was, all of it due")


# ---------- J-072 (11-13): read only what changed, on the live repo's numbers ----------
T0 = NOW - dt.timedelta(minutes=30)        # the full pass
H399, H399B = "9" * 40, "b" * 40
REF399 = "claude/integrate-j399-thing"
GREEN = [{"name": "host-build", "status": "completed", "conclusion": "success"}]
RED = [{"name": "host-build", "status": "completed", "conclusion": "failure"}]
FILLER = 145                               # proof runs in the 6-hour span on 11-13; one page held 100


def at(t: dt.datetime) -> str:
    return iso(t)


def proof_run(rid: int, branch: str, head: str, created: dt.datetime, updated: dt.datetime,
              conclusion: str = "success", status: str = "completed") -> dict:
    return {"id": rid, "head_branch": branch, "head_sha": head, "status": status, "conclusion": conclusion,
            "created_at": at(created), "updated_at": at(updated), "html_url": f"https://example.invalid/run/{rid}"}


def muse_says(cid: int, verdict: str, head: str, when: dt.datetime, repro: bool = False) -> dict:
    """Muse's mailed verdict as the bridge posts it on the job issue #499 (it cannot comment on a PR)."""
    body = f"**Muse (via email bridge): J-399 review of #399 @ {head[:8]}: {verdict}**\n\nRead it.\n"
    if repro:
        body += "command: make test\ninput: the door\nexpected: it opens\nobserved: it sticks\n"
    return {"id": cid, "issue_url": f"https://api.github.com/repos/{REPO}/issues/499", "body": body,
            "user": {"login": "IssisX"}, "created_at": at(when), "updated_at": at(when)}


class RunsGH(CountingGH):
    """CountingGH whose proof-run listings filter as GitHub's do (head_sha, status, created), over FILLER other runs."""

    workflow_prs: tuple = ()   # PRs that change .github/workflows
    def __init__(self, prs, runs=(), **kw):
        filler = [proof_run(5000 + i, f"claude/integrate-other-{i}", f"{i:040x}", T0 - dt.timedelta(hours=4),
                            T0 - dt.timedelta(hours=3)) for i in range(FILLER)]
        super().__init__(prs, runs=list(runs) + filler, **kw)
        self.jobs.update({r["id"]: GREEN for r in filler})

    def route(self, path):
        base = path.split("?")[0]
        m = re.fullmatch(rf"repos/{REPO}/pulls/(\d+)/files", base)
        if m and int(m.group(1)) in self.workflow_prs:
            self.calls.append(path)
            return [{"filename": ".github/workflows/switchboard.yml"}]
        m = re.fullmatch(rf"repos/{REPO}/pulls/(\d+)", base)
        if m:   # the whole PR, as fresh_facts reads it before a merge
            self.calls.append(path)
            return next(dict(p, mergeable=True) for p in self.open_prs if p["number"] == int(m.group(1)))
        if "/actions/workflows/" in base and base.endswith("/runs"):
            self.calls.append(path)
            got = list(self.runs)
            m = re.search(r"head_sha=([0-9a-f]+)", path)
            got = [r for r in got if r["head_sha"] == m.group(1)] if m else got
            m = re.search(r"status=(\w+)", path)
            got = [r for r in got if r["status"] == m.group(1)] if m else got
            m = re.search(r"created=%3E%3D([0-9TZ:-]+)", path)
            got = [r for r in got if r["created_at"] >= m.group(1)] if m else got
            return {"workflow_runs": sorted(got, key=lambda r: r["created_at"], reverse=True)}
        return super().route(path)

    @contextlib.contextmanager
    def fresh(self):
        yield self

    def write(self, method, path, body=None, what="", token=""):   # the merge token's write, as any other here
        return super().write(method, path, body, what)


class OnlyWhatChanged(unittest.TestCase):
    """Cory 11-13: the switchboard reads only what changed. Live, 145 proof runs in the span made every fast sweep read
    every PR (24 of 24, about 500 calls a sweep, the hour's 5,000 gone in run 43492647518). A skipped read never misses a
    merge, a red proof or a new verdict, and the full pass still reads everything every hour."""

    def setUp(self):
        self.prs = [dict(pr(398), draft=True), dict(pr(399), head={"sha": H399, "ref": REF399})]
        self.jobs_listed = (job_issue(499, "J-399"),)

    def gh(self, runs, comments=(), prs=None, job_issues=None) -> RunsGH:
        gh = RunsGH(prs or self.prs, runs=runs, comments=list(comments))
        gh.job_issues = job_issues if job_issues is not None else self.jobs_listed
        gh.jobs.setdefault(9001, RED)
        gh.by_issue[499] = [c for c in comments if c["issue_url"].endswith("/499")]
        return gh

    def sweep(self, gh, now, whole=None, merge_token=""):
        from unittest import mock
        with mock.patch.object(switch.Switch, "run_rule_check", autospec=True, return_value=(0, "rule_check: 0 BLOCK")):
            status, read, sw = sweep(gh, now=now, whole=copy.deepcopy(whole) if whole is not None else {},
                                     merge_token=merge_token)
        self.assertEqual(status, 0, sw.lines)
        return read, sw

    def full_pass(self, runs, comments=()):
        gh = self.gh(runs, comments)
        read, sw = self.sweep(gh, T0)
        self.assertEqual(read, [398, 399], "the full pass reads every PR")
        self.assertFalse(any(l.startswith(("MERGE ", "WAIT ")) for l in sw.lines), sw.lines)   # both cold
        return gh.state

    def merged(self, gh) -> bool:
        return f"PUT repos/{REPO}/pulls/399/merge" in gh.calls

    def assert_read_and_merged(self, gh, read, sw):
        self.assertEqual(read, [399, 399], "only #399, read, then read again uncached before its merge (fresh_facts)")
        self.assertTrue(self.merged(gh), sw.lines)

    def red_run(self, status="completed"):
        return proof_run(9001, REF399, H399, T0 - dt.timedelta(days=2), T0 - dt.timedelta(hours=1), "failure", status)

    def test_an_unchanged_pr_is_not_read_again(self):
        state = self.full_pass([self.red_run()])
        gh = self.gh([self.red_run()])
        read, _ = self.sweep(gh, T0 + dt.timedelta(minutes=10), state)
        self.assertEqual(read, [], "nothing about either PR changed")
        self.assertLessEqual(len(gh.calls), IDLE + 2, gh.calls)   # the created listing is two pages live

    def test_a_proof_rerun_that_turns_green_is_read_and_merges(self):
        # 43109809366: created 11-11, re-run 11-13, so a created>= listing never sees it finish
        land = muse_says(1, "LAND", H399, T0 - dt.timedelta(hours=2))
        gh0 = self.gh([self.red_run()], [land])
        read, sw = self.sweep(gh0, T0)
        self.assertIn("FIX-CI", " ".join(sw.lines))
        rerun = dict(self.red_run(), status="in_progress", conclusion=None, updated_at=at(T0 + dt.timedelta(minutes=5)))
        gh1 = self.gh([rerun], [land])
        read, _ = self.sweep(gh1, T0 + dt.timedelta(minutes=10), gh0.state)
        self.assertEqual(read, [], "running: nothing to decide yet")
        done = dict(self.red_run(), conclusion="success", updated_at=at(T0 + dt.timedelta(minutes=25)))
        gh2 = self.gh([done], [land])
        gh2.jobs[9001] = GREEN
        read, sw = self.sweep(gh2, T0 + dt.timedelta(minutes=30), gh1.state)
        self.assert_read_and_merged(gh2, read, sw)

    def test_a_new_head_is_read_and_merges(self):
        gh0 = self.gh([self.red_run()])
        self.sweep(gh0, T0)
        prs = [self.prs[0], dict(self.prs[1], head={"sha": H399B, "ref": REF399}, updated_at=at(T0 + dt.timedelta(minutes=2)))]
        green = proof_run(9002, REF399, H399B, T0 + dt.timedelta(minutes=2), T0 + dt.timedelta(minutes=20))
        gh = self.gh([self.red_run(), green], [muse_says(1, "LAND", H399B, T0 + dt.timedelta(minutes=25))], prs=prs)
        gh.jobs.update({9001: RED, 9002: GREEN})
        read, sw = self.sweep(gh, T0 + dt.timedelta(minutes=30), gh0.state)
        self.assert_read_and_merged(gh, read, sw)

    def test_a_bridged_land_on_the_job_issue_merges_an_unchanged_pr(self):
        # the coordinator, 11-13: Muse's LAND by mail lands on the job issue and moves nothing on the PR itself
        green = proof_run(9001, REF399, H399, T0 - dt.timedelta(hours=2), T0 - dt.timedelta(hours=1))
        fix = muse_says(1, "FIX", H399, T0 - dt.timedelta(hours=1), repro=True)
        gh0 = self.gh([green], [fix])
        gh0.jobs[9001] = GREEN
        read, sw = self.sweep(gh0, T0)
        self.assertIn("open FIX from Muse", " ".join(sw.lines))
        land = muse_says(2, "LAND", H399, T0 + dt.timedelta(minutes=20))
        gh = self.gh([green], [fix, land])
        gh.jobs[9001] = GREEN
        read, sw = self.sweep(gh, T0 + dt.timedelta(minutes=30), gh0.state)
        self.assert_read_and_merged(gh, read, sw)

    def test_a_needs_label_on_the_job_issue_reads_its_pr(self):
        state = self.full_pass([self.red_run()])
        gh = self.gh([self.red_run()], job_issues=(job_issue(499, "J-399", ["needs:eyes", "wake:muse"]),))
        read, _ = self.sweep(gh, T0 + dt.timedelta(minutes=10), state)
        self.assertEqual(read, [399])
        gh = self.gh([self.red_run()], job_issues=(job_issue(499, "J-399", ["wake:muse"]),))
        read, _ = self.sweep(gh, T0 + dt.timedelta(minutes=10), state)
        self.assertEqual(read, [], "wake:muse comes and goes with every Muse notice; pr_facts takes only needs:")

    def test_the_hourly_backstop_reads_every_pr(self):
        state = self.full_pass([self.red_run()])
        prs = [self.prs[0], dict(self.prs[1], updated_at=at(T0 + dt.timedelta(minutes=50)))]   # a label or a comment
        gh = self.gh([self.red_run()], prs=prs)
        read, _ = self.sweep(gh, T0 + dt.timedelta(minutes=59), state)
        self.assertEqual(read, [399], "within the hour: only the changed PR")
        state = gh.state
        gh = self.gh([self.red_run()], prs=prs)
        read, _ = self.sweep(gh, T0 + dt.timedelta(minutes=61), state)
        self.assertEqual(read, [398, 399], "past the hour: every PR")
        self.assertEqual(gh.state["steer"]["full_at"], at(T0 + dt.timedelta(minutes=61)))

    def test_a_change_to_what_the_switch_loads_from_trunk_reads_every_pr(self):
        from unittest import mock
        state = self.full_pass([self.red_run()])
        with mock.patch.object(switch, "trunk_inputs", return_value="0" * 16):
            read, _ = self.sweep(self.gh([self.red_run()]), T0 + dt.timedelta(minutes=10), state)
        self.assertEqual(read, [398, 399])

    def test_with_no_running_runs_read_every_pr_is_read(self):
        state = self.full_pass([self.red_run()])

        class Down(RunsGH):
            def route(self, path):
                if "status=queued" in path:
                    raise switch.GitHubError("GET", path, "gh: Server Error (HTTP 502)", 502)
                return super().route(path)
        gh = Down(self.prs, runs=[self.red_run()])
        gh.job_issues = self.jobs_listed
        read, _ = self.sweep(gh, T0 + dt.timedelta(minutes=10), state)
        self.assertEqual(read, [398, 399])

    def test_another_job_opening_reads_a_pr_holding_the_eyes_fix(self):
        # rules.eye_relook: an eye FIX is a LAND once every set it failed is another open job's known defect
        state = self.full_pass([self.red_run()])
        state["steer"]["prs"]["399"]["eye_fix"] = True
        more = self.jobs_listed + (job_issue(600, "J-600"),)
        read, _ = self.sweep(self.gh([self.red_run()], job_issues=more), T0 + dt.timedelta(minutes=10), state)
        self.assertEqual(read, [399])

    def test_a_human_merge_waits_unread_until_something_it_waits_on_changes(self):
        # read every sweep, a HUMAN-MERGE cost about 28 uncached calls (fresh_facts): it waits on a person, a review,
        # a mail or a push, which the delta sees, or on the merge token being set, which sweep_inputs carries
        green = proof_run(9001, REF399, H399, T0 - dt.timedelta(hours=2), T0 - dt.timedelta(hours=1))
        land = muse_says(1, "LAND", H399, T0 - dt.timedelta(hours=1))

        def world(comments=(land,)):
            gh = self.gh([green], list(comments))
            gh.jobs[9001], gh.workflow_prs = GREEN, (399,)
            return gh
        gh0 = world()
        _, sw = self.sweep(gh0, T0)
        self.assertTrue(any(l.startswith("HUMAN-MERGE #399") for l in sw.lines), sw.lines)
        read, _ = self.sweep(world(), T0 + dt.timedelta(minutes=10), gh0.state)
        self.assertEqual(read, [], "nothing it waits on changed")
        read, _ = self.sweep(world(), T0 + dt.timedelta(minutes=10), gh0.state, merge_token="t")
        self.assertEqual(sorted(set(read)), [398, 399], "the merge token was set")
        again = muse_says(2, "LAND", H399, T0 + dt.timedelta(minutes=5))
        read, _ = self.sweep(world((land, again)), T0 + dt.timedelta(minutes=10), gh0.state)
        self.assertEqual(read, [399, 399], "a new verdict by mail: #399 only, read again uncached as a HUMAN-MERGE")

    def test_a_refused_merge_token_is_tried_again_next_sweep(self):
        seen = test_inbox.human_merge()
        pr_ = {"head": {"sha": test_inbox.HEAD}, "updated_at": "2026-11-06T18:00:00Z"}
        self.assertTrue(switch.pr_record(None, pr_, seen.facts, seen.ctx, seen, False)["cold"])
        seen.decision = switch.rules.MergeDecision(
            "HUMAN-MERGE", (f"{switch.TOKEN_REFUSED} (HTTP 403): SWITCHBOARD_MERGE_TOKEN needs more",), "Codex")
        self.assertFalse(switch.pr_record(None, pr_, seen.facts, seen.ctx, seen, False)["cold"])



if __name__ == "__main__":
    unittest.main()
