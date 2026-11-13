"""Unit tests for QUEUE-LOW (J-097): the switchboard counts the really-unstarted jobs in Muse's queue and, under 8,
lists `QUEUE-LOW: N unstarted` in the integrator's inbox (#282), once per change.

Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
No network, no clock, no files: GitHub, the time and the queue text are given.
"""

from __future__ import annotations

import contextlib
import datetime as dt
import io
import pathlib
import re
import sys
import tempfile
import unittest
from unittest import mock

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "board"))

import muse_queue  # noqa: E402
import queue_check  # noqa: E402
import switch  # noqa: E402
import test_inbox  # noqa: E402
import test_sweep_calls  # noqa: E402

NOW = test_inbox.NOW
REPO = test_inbox.FakeGH.repo
BRIDGE = "**Muse (via email bridge): MUSE_TO_CLAUDE: {job} started**\n\nOn it."


def iso(hours_ago: float) -> str:
    return (NOW - dt.timedelta(hours=hours_ago)).strftime("%Y-%m-%dT%H:%M:%SZ")


def queue(n: int, first: int = 500, extra: str = "") -> str:
    """A muse.md with n queued jobs J-<first>.. on issues #<first>.., then the next block."""
    lines = [f"{i}. J-{first + i:03d} #{first + i}: job {i}." for i in range(n)]
    return ("## Queue\n\nIn flight: J-001 #1.\nUnstarted, in order (11-11):\n" + "\n".join(lines) + extra
            + "\n\nLive lanes: J-002 #2.\n")


class QueueGH(test_inbox.FakeGH):
    """test_inbox's fake (the inbox PR's reads and comments), plus the reads muse_queue makes."""

    def __init__(self, issues=(), prs=(), branches=(), commits=None, comments=None, **kw):
        super().__init__(**kw)
        self.issues = {int(n): {"number": int(n), "state": "open", "comments": 0} for n in issues}
        self.all_prs = list(prs)
        self.branch_list = list(branches)
        self.commits = dict(commits or {})
        for n, cs in (comments or {}).items():
            self.by_issue[int(n)] = list(cs)
            self.issues.setdefault(int(n), {"number": int(n), "state": "open"})["comments"] = len(cs)

    def route(self, path):
        base = path.split("?")[0]
        got = re.search(r"[?&]page=(\d+)", path)
        page = int(got.group(1)) if got else 1
        m = re.fullmatch(rf"repos/{REPO}/issues/(\d+)", base)
        if m and int(m.group(1)) in self.issues:
            self.gets.append(path)
            return dict(self.issues[int(m.group(1))])
        if base == f"repos/{REPO}/pulls" and "state=all" in path:
            self.gets.append(path)
            ordered = sorted(self.all_prs, key=lambda p: -p["number"])
            return ordered[(page - 1) * 100: page * 100]
        if base == f"repos/{REPO}/branches":
            self.gets.append(path)
            return self.branch_list[(page - 1) * 100: page * 100]
        m = re.fullmatch(rf"repos/{REPO}/commits/([0-9a-f]+)", base)
        if m:
            self.gets.append(path)
            return {"sha": m.group(1), "commit": {"committer": {"date": self.commits[m.group(1)]}}}
        return super().route(path)


def pr(number: int, job: str, state: str = "open", merged: bool = False) -> dict:
    return {"number": number, "state": state, "merged_at": iso(1) if merged else None, "body": f"Job: {job}\n\nWork."}


def branch(name: str, sha: str) -> dict:
    return {"name": name, "commit": {"sha": sha}}


def bridged(cid: int, number: int, job: str, hours: float = 1.0) -> dict:
    return test_inbox.comment(cid, number, BRIDGE.format(job=job), hours)


def make(gh) -> switch.Switch:
    return test_inbox.make(gh)


class Count(unittest.TestCase):
    def verdicts(self, gh, text) -> dict[str, str]:
        return {v.line.job: v.text for v in muse_queue.check(gh, text, NOW).verdicts}

    def test_only_the_unstarted_block_is_read(self):
        text = queue(3)
        self.assertEqual([q.job for q in muse_queue.queue_lines(text)], ["J-500", "J-501", "J-502"])
        self.assertEqual(muse_queue.queue_lines("no queue here"), [])

    def test_a_started_merged_open_pr_branched_or_closed_job_is_not_counted(self):
        gh = QueueGH(issues=range(500, 507),
                     prs=[pr(600, "J-501", state="closed", merged=True), pr(601, "J-502"),
                          pr(602, "J-505", state="closed")],   # closed unmerged: not work
                     branches=[branch("fb-bot/integrate-j503-door", "a" * 40), branch("muse/j506-old", "b" * 40)],
                     commits={"a" * 40: iso(5), "b" * 40: iso(80)},   # j506's branch has not moved in 72 h
                     comments={504: [bridged(1, 504, "J-504")]})
        gh.issues[500]["state"] = "closed"
        got = self.verdicts(gh, queue(7))
        self.assertEqual(got, {"J-500": "DONE", "J-501": "DONE", "J-502": "OPEN-PR",
                               "J-503": "BRANCH fb-bot/integrate-j503-door", "J-504": "STARTED", "J-505": "OK",
                               "J-506": "OK"})

    def test_a_job_line_only_names_the_job_when_the_body_starts_with_it(self):
        gh = QueueGH(issues=range(500, 502), prs=[{"number": 600, "state": "open", "merged_at": None,
                                                   "body": "Follow-up.\n\nJob: J-500 is next"}])
        self.assertEqual(self.verdicts(gh, queue(2))["J-500"], "OK")

    def test_never_started_is_not_started(self):
        gh = QueueGH(issues=[500], comments={500: [test_inbox.comment(
            1, 500, "**Muse (via email bridge): J-500 never started, parked**")]})
        self.assertEqual(self.verdicts(gh, queue(1))["J-500"], "OK")

    def test_a_missing_issue_is_not_counted_and_is_named(self):
        gh = QueueGH(issues=[500])
        report = muse_queue.check(gh, queue(2), NOW)
        self.assertEqual([v.verdict for v in report.verdicts], ["OK", "UNREAD"])
        self.assertIn("J-501 #501: not found on GitHub; not counted as unstarted", report.notes)

    def test_the_pr_listing_stops_below_the_oldest_queued_issue(self):
        prs = [pr(n, "J-999", state="closed") for n in range(100, 800)]   # 700 PRs, newest #799
        gh = QueueGH(issues=range(650, 652), prs=prs)
        muse_queue.check(gh, queue(2, first=650), NOW)
        pages = [g for g in gh.gets if "pulls?state=all" in g]
        self.assertEqual(len(pages), 2, pages)   # #799-#700, then #699-#600 reaches below #650

    def test_each_listing_is_read_once_per_check(self):
        gh = QueueGH(issues=range(500, 510), branches=[branch(f"claude/b{i}", "c" * 40) for i in range(150)])
        muse_queue.check(gh, queue(10), NOW)
        self.assertEqual(len([g for g in gh.gets if "pulls?state=all" in g]), 1)
        self.assertEqual(len([g for g in gh.gets if "/branches" in g]), 2)   # 150 branches: two pages, once
        self.assertEqual(len([g for g in gh.gets if re.search(r"issues/\d+$", g)]), 10)


class Raise(unittest.TestCase):
    def test_under_8_raises_one_inbox_item(self):
        gh = QueueGH(issues=range(500, 507))
        sw = make(gh)
        items = sw.queue_check(queue(7))
        self.assertEqual(len(items), 1)
        self.assertEqual(items[0].kind, "QUEUE-LOW")
        self.assertEqual(sw.inbox([]), 1)
        (number, body), = gh.posts
        self.assertEqual(number, switch.INBOX_PR)
        self.assertIn("- QUEUE-LOW: 7 unstarted in `work/tasks/muse.md` (under 8): refill to 15. Unstarted: "
                      "J-500, J-501, J-502, J-503, J-504, J-505, J-506", body)
        self.assertIn("QUEUE       7 of 7 queued job(s) unstarted (QUEUE-LOW under 8)", sw.lines)

    def test_8_or_more_is_quiet(self):
        for n in (8, 15):
            gh = QueueGH(issues=range(500, 500 + n))
            sw = make(gh)
            self.assertEqual(sw.queue_check(queue(n)), ())
            self.assertEqual(sw.inbox([]), 0)
            self.assertEqual(gh.posts, [])

    def test_taken_jobs_do_not_count_toward_the_8(self):
        gh = QueueGH(issues=range(500, 509), prs=[pr(600, "J-500")], comments={501: [bridged(1, 501, "J-501")]})
        sw = make(gh)
        items = sw.queue_check(queue(9))   # 9 listed, 2 taken: 7 really unstarted
        self.assertEqual(len(items), 1)
        self.assertIn("QUEUE-LOW: 7 unstarted", items[0].line)
        self.assertIn("taken: J-500 OPEN-PR, J-501 STARTED", items[0].line)

    def test_the_same_count_twice_does_not_repost(self):
        gh = QueueGH(issues=range(500, 505))
        sw = make(gh)
        sw.queue_check(queue(5))
        self.assertEqual(sw.inbox([]), 1)
        sw = make(gh)   # the next sweep: the same five
        sw.queue_check(queue(5))
        self.assertEqual(sw.inbox([]), 0)
        self.assertEqual(len(gh.posts), 1)

    def test_a_new_count_is_posted_again(self):
        gh = QueueGH(issues=range(500, 505))
        sw = make(gh)
        sw.queue_check(queue(5))
        sw.inbox([])
        gh.by_issue[500] = [bridged(1, 500, "J-500")]   # Muse starts one
        gh.issues[500]["comments"] = 1
        sw = test_inbox.later(gh)   # a sweep past the hold on the integrator's own work (11-13)
        sw.queue_check(queue(5))
        self.assertEqual(sw.inbox([]), 1)
        self.assertIn("QUEUE-LOW: 4 unstarted", gh.posts[1][1])

    def test_a_missing_queue_file_reads_as_empty_and_raises(self):
        gh = QueueGH()
        sw = make(gh)
        with tempfile.TemporaryDirectory() as d:
            sw.root = pathlib.Path(d)
            items = sw.queue_check()
        self.assertIn("QUEUE-LOW: 0 unstarted", items[0].line)
        self.assertTrue(any("could not be read" in line for line in sw.lines))


class Sweep(unittest.TestCase):
    """run_sweep: the count runs on a full pass or a fast sweep that saw a possible start; an unlisted item wakes even
    a quiet sweep's inbox, and a listed one costs nothing."""

    def setUp(self):
        self.prs = [test_sweep_calls.pr(n) for n in range(361, 364)]

    def state(self, queue_timer=None) -> dict:
        st = test_sweep_calls.last_state(self.prs)
        if queue_timer is not None:
            st["timers"]["queue"] = queue_timer
        return st

    def test_an_unlisted_queue_low_wakes_a_quiet_sweeps_inbox(self):
        item = {"head": "5:J-500", "line": "QUEUE-LOW: 5 unstarted in `work/tasks/muse.md` (under 8): refill to 15."}
        gh = test_sweep_calls.CountingGH(self.prs, dry_run=False)
        with mock.patch.object(switch.Switch, "queue_check") as counted:
            status, read, sw = test_sweep_calls.sweep(gh, self.state({"item": item, "listed": ""}))
        self.assertEqual(status, 0, sw.lines)
        counted.assert_not_called()   # a quiet sweep does not count again: the last count stands
        (number, body), = gh.posts
        self.assertEqual(number, switch.INBOX_PR)
        self.assertIn("- QUEUE-LOW: 5 unstarted", body)
        self.assertEqual(gh.state["steer"]["timers"]["queue"]["listed"], switch.queue_item_of(item)[0].key)

    def test_a_listed_queue_low_costs_a_quiet_sweep_nothing(self):
        item = {"head": "5:J-500", "line": "QUEUE-LOW: 5 unstarted"}
        listed = switch.queue_item_of(item)[0].key
        idle = test_sweep_calls.CountingGH(self.prs)
        test_sweep_calls.sweep(idle, self.state())
        gh = test_sweep_calls.CountingGH(self.prs)
        status, _, sw = test_sweep_calls.sweep(gh, self.state({"item": item, "listed": listed}))
        self.assertEqual(status, 0, sw.lines)
        self.assertEqual(gh.calls, idle.calls)
        self.assertEqual(gh.posts, [])

    def test_a_bridged_post_makes_a_fast_sweep_count(self):
        gh = test_sweep_calls.CountingGH(self.prs, comments=[bridged(9, 410, "J-092", hours=0.05)])
        with mock.patch.object(switch.Switch, "queue_check", autospec=True, return_value=()) as counted:
            status, _, sw = test_sweep_calls.sweep(gh, self.state())
        self.assertEqual(status, 0, sw.lines)
        counted.assert_called_once()

    def test_a_proof_run_counts_only_on_a_branch_naming_a_queued_job(self):
        for name, want in (("fb-bot/integrate-j501-door", 1), ("fb-bot/integrate-j042-other", 0)):
            gh = test_sweep_calls.CountingGH(self.prs, runs=[test_inbox.run(77, name, "d" * 40, 0.01)])
            with mock.patch.object(switch.Switch, "queue_text", return_value=queue(3)), \
                    mock.patch.object(switch.Switch, "queue_check", autospec=True, return_value=()) as counted:
                test_sweep_calls.sweep(gh, self.state())
            self.assertEqual(counted.call_count, want, name)

    def test_live_a_proof_run_counts_on_a_branch_naming_a_muse_job_of_the_plan(self):   # muse.md's block is gone
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            (root / "work").mkdir()
            (root / "work/plan.md").write_text("limit muse 2\nlimit claude 1\nJ-501 muse #501 | Muse's\n"
                                              "J-601 claude #601 | Claude's\n", encoding="utf-8")
            sw = test_inbox.make(test_sweep_calls.CountingGH(self.prs))
            sw.root, sw.plan_mode = root, lambda: "live"
            for name, want in (("fb-bot/integrate-j501-door", True), ("claude/integrate-j601", False)):
                delta = switch.SweepDelta(set(), 0, set(), set(), active={name})
                self.assertEqual(sw.queue_started(delta, False), want, name)

    def test_a_full_pass_counts(self):
        gh = test_sweep_calls.CountingGH(self.prs)
        with mock.patch.object(switch.Switch, "queue_check", autospec=True, return_value=()) as counted:
            test_sweep_calls.sweep(gh, None)
        counted.assert_called_once()


class Cli(unittest.TestCase):
    def test_the_cli_prints_the_same_lines_and_exit_code(self):
        gh = QueueGH(issues=list(range(500, 503)) + [63], prs=[pr(600, "J-501")],
                     branches=[branch("fb-bot/integrate-j502-x", "a" * 40)],   # the CLI reads the real clock
                     commits={"a" * 40: dt.datetime.now(dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")})
        gh.get = lambda path, accept="": (
            {"items": [{"number": 63, "title": "J-063 chest", "state": "open"}]} if path.startswith("search/issues")
            else gh.route(path))
        text = queue(3, extra=" after J-063 lands") + "Parked: J-053 #82 (on Grok).\n"
        gh.by_issue[82] = [test_inbox.comment(5, 82, "To Grok: the bar\nmore")]
        with tempfile.TemporaryDirectory() as d:
            path = pathlib.Path(d) / "muse.md"
            path.write_text(text, encoding="utf-8")
            out = io.StringIO()
            with mock.patch.object(queue_check, "GitHub", return_value=gh), mock.patch.object(sys, "argv", ["q", str(path)]), \
                    contextlib.redirect_stdout(out):
                code = queue_check.main()
        self.assertEqual(code, 1)
        self.assertEqual(out.getvalue().splitlines(), [
            "OK       J-500 #500",
            "OPEN-PR  J-501 #501",
            "BRANCH fb-bot/integrate-j502-x  WAITS-ON J-063 #63 (open) J-502 #502",
            f"PARKED   J-053 #82  newest: {iso(1)} To Grok: the bar",
            "QUEUE STALE: 2 not unstarted",
        ])


# The owner checks' scenarios (another owner's job, the parked line, a bare J-NNN issue) moved with the checks to
# the job machine at the cutover: test_machine.Owners.


if __name__ == "__main__":
    unittest.main()
