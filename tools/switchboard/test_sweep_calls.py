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

    fail_once: tuple = ()   # path prefixes whose first read fails (a transient 403)

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


def sweep(gh, state=None, now=NOW, whole=None, plan_mode="off"):
    """One sweep. `plan_mode` is what work/switchboard.yml says (`plan`): off, since the plan part has its own sweep
    tests (test_plan_sweep)."""
    sw = test_inbox.make(gh, now)
    sw.root = HERE.parents[1]
    sw.merge_token = sw.review_key = ""
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
        import ledger
        it = ledger.Item("JOB", f"#{thread}", "", "J-099", None, ledger.Person("Builder", "builder"), "claimed, no PR",
                         NOW - dt.timedelta(hours=4, minutes=minutes_ago), dt.timedelta(hours=4), "v1", thread)
        return ledger, it, {"at": iso(NOW - dt.timedelta(minutes=minutes_ago)), "item": ledger.item_json(it)}

    def test_a_ripe_ledger_deadline_an_old_cache_holds_posts_nothing_and_its_clock_is_dropped(self):
        # D2: the ledger left the sweep; a ledger timer the cache still holds is never fired, and it is dropped
        _, _, timer = self.ledger_timer()
        state = last_state(self.prs)
        state["timers"]["ledger"] = [timer]
        gh = CountingGH(self.prs, dry_run=False)
        sweep(gh, state)
        self.assertEqual(gh.posts, [])
        self.assertEqual(len(gh.calls), IDLE, gh.calls)
        self.assertNotIn("ledger", gh.state["steer"]["timers"])

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

    def test_a_newest_first_listing_is_read_only_back_to_what_was_kept(self):
        import ledger
        events = [{"id": i, "event": "labeled", "created_at": iso(NOW - dt.timedelta(minutes=i)),
                   "actor": {"login": "x"}, "issue": {"number": 7, "title": "big"}} for i in range(1, 251)]

        class Pages:
            repo = REPO

            def __init__(self, items, state=None):
                self.items, self.calls, self.state = items, 0, state if state is not None else {}

            def get(self, path):
                self.calls += 1
                page = int(re.search(r"[?&]page=(\d+)", path).group(1))
                return self.items[(page - 1) * 100: page * 100]

        when = lambda e: ledger.parse(e.get("created_at"))   # noqa: E731
        sw = type("S", (), {})()
        sw.gh = Pages(events)
        got, _ = ledger.newest_first(sw, "events", f"repos/{REPO}/issues/events", NOW - dt.timedelta(days=2), when,
                                     ledger.EVENT_KEYS)
        self.assertEqual((sw.gh.calls, len(got)), (3, 250))
        self.assertEqual(got[0]["issue"], {"number": 7})   # trimmed to what the ledger reads
        new = [{"id": 1000 + i, "event": "closed", "created_at": iso(NOW + dt.timedelta(minutes=i)),
                "actor": {"login": "y"}, "issue": {"number": 8}} for i in range(3)]
        sw.gh = Pages(list(reversed(new)) + events, {"ledger_events": got})
        got2, _ = ledger.newest_first(sw, "events", f"repos/{REPO}/issues/events", NOW - dt.timedelta(days=2), when,
                                      ledger.EVENT_KEYS)
        self.assertEqual(sw.gh.calls, 1)                    # one page: it reached what was kept
        self.assertEqual(len(got2), 253)
        self.assertEqual([e["id"] for e in got2[:3]], [1002, 1001, 1000])
        late, _ = ledger.newest_first(sw, "events", f"repos/{REPO}/issues/events", NOW + dt.timedelta(minutes=1),
                                      when, ledger.EVENT_KEYS)
        self.assertEqual([e["id"] for e in late], [1002, 1001])   # trimmed to the window

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

    def test_a_branch_that_had_a_pr_is_not_asked_again(self):
        gh = CountingGH([])
        gh.state = {}
        calls = []
        gh.paged = lambda path, key="": (calls.append(path), [{"number": 5}] if "had-one" in path else [])[1]
        sw = test_inbox.make(gh)
        for _ in range(3):
            self.assertTrue(sw.prior_pr("fb-bot/integrate-had-one"))
            self.assertFalse(sw.prior_pr("fb-bot/integrate-never"))
        self.assertEqual(sum("had-one" in c for c in calls), 1)   # a yes is kept; a no is asked each time
        self.assertEqual(sum("never" in c for c in calls), 3)
        self.assertEqual(gh.state["prior_pr"], ["fb-bot/integrate-had-one"])



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



if __name__ == "__main__":
    unittest.main()
