"""The switch's plan reads: landed heads are history, unlanded work stays visible, nothing is hidden."""
import datetime as dt
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import machine  # noqa: E402
import plan  # noqa: E402
import switch  # noqa: E402

NOW = dt.datetime(2026, 11, 12, 18, 0, tzinfo=dt.timezone.utc)
PLAN = ("limit claude 2\nlimit muse 6\n"
        "J-101 claude #101 | landed by a merged PR\n"
        "J-102 claude #102 | branch already in trunk\n"
        "J-103 claude #103 | old work, no PR\n"
        "J-104 claude #104 | live work\n"
        "J-105 claude #105 | Muse pushed it\n"
        "J-106 claude #106 | open PR\n")


def br(name, sha):
    return {"name": name, "commit": {"sha": sha}}


class FakeGH:
    repo = "IssisX/ScraperX"

    def __init__(self):
        self.gets = []
        self.issue = {}

    def paged(self, path, key=""):
        if "/issues?" in path:
            return [{"number": n, "title": f"J-{n} job", "state": "open"} for n in range(101, 107)]
        if "/branches" in path:
            return [br("claude/integrate-j101", "s101"), br("claude/integrate-j102", "s102"),
                    br("claude/integrate-j103", "s103"), br("claude/integrate-j104", "s104"),
                    br("fb-bot/integrate-j105", "s105"), br("claude/integrate-j106", "s106")]
        if "state=closed" in path:
            return [{"number": 1, "head": {"sha": "s101"}, "merged_at": "2026-11-10T00:00:00Z", "body": "Job: J-101\n"}]
        raise AssertionError(path)

    def get(self, path, accept=""):
        self.gets.append(path)
        if "/commits?path=" in path:
            return [{"commit": {"committer": {"date": "2026-11-12T10:00:00Z"}}}]
        if "/issues/" in path:
            return self.issue[int(path.rsplit("/", 1)[1])]
        sha = path.rsplit("...", 1)[1]
        if sha == "s102":
            return {"status": "behind", "commits": []}
        date = "2026-11-01T00:00:00Z" if sha == "s103" else "2026-11-12T12:00:00Z"
        return {"status": "diverged", "commits": [{"sha": sha, "commit": {"committer": {"date": date}}}]}


class PlanReads(unittest.TestCase):
    def setUp(self):
        self.sw = switch.Switch.__new__(switch.Switch)
        self.sw.gh, self.sw.now, self.sw.lines = FakeGH(), NOW, []
        self.sw.out = self.sw.lines.append
        self.sw.listing = lambda path, key="": [{"number": 9, "state": "open", "body": "Job: J-106\n",
                                                 "head": {"sha": "s106", "ref": "claude/integrate-j106"}}]
        self.model = plan.parse(PLAN)

    def rows(self, r, model=None):
        model = model or self.model
        self.table = machine.table(model, machine.collect(model, r, planned_at=r.get("planned_at")))
        return {x.job: x for x in self.table}

    def test_only_real_lanes_count_and_every_job_stays_visible(self):
        r = self.sw.plan_reads(self.model)
        self.assertEqual(r["inactive"], {"s101", "s102"})
        self.assertEqual(r["head_at"]["s103"], dt.datetime(2026, 11, 1, tzinfo=dt.timezone.utc))
        self.assertFalse(any("s101" in g or "s106" in g for g in self.sw.gh.gets))  # no compare for known heads
        rows = self.rows(r)
        self.assertEqual({j: x.state for j, x in rows.items()},
                         {"J-101": "landed", "J-102": "acknowledged", "J-103": "pushed", "J-104": "pushed",
                          "J-105": "conflict", "J-106": "pushed"})
        # A head with no PR and no commit for 3 days (it was DORMANT) is the late job, and it still holds its
        # lane: it is unlanded work (the plan's own count on 11-12, 7/4 and 17/6, has these in it). A hold or a
        # `kept` ruling frees the lane; nothing drops out of the count by going quiet.
        self.assertTrue(rows["J-103"].late(NOW))
        self.assertIn("no PR and no commit in 3 days", rows["J-103"].next)
        self.assertFalse(rows["J-104"].late(NOW))
        self.assertEqual(machine.wip(self.model, self.table)["claude"], (3, 2))   # J-103, J-104, J-106
        self.assertIn("fb-bot/integrate-j105", rows["J-105"].why)                 # it was NOT-OWNER
        self.assertEqual(r["planned_at"], dt.datetime(2026, 11, 12, 10, tzinfo=dt.timezone.utc))

    def test_closed_unmerged_pr_keeps_its_head_visible_as_unresolved_work(self):
        old = self.sw.gh.paged

        def paged(path, key=""):
            if "state=closed" in path:
                return [{"number": 1, "head": {"sha": "s101"},
                         "merged_at": "2026-11-10T00:00:00Z", "body": "Job: J-101\n"},
                        {"number": 2, "head": {"sha": "s105"},
                         "merged_at": None, "body": "Job: J-105\n"}]
            return old(path, key)

        self.sw.gh.paged = paged
        observed = self.sw.plan_reads(self.model)
        self.assertIn("s101", observed["inactive"])
        self.assertNotIn("s105", observed["inactive"])
        row = self.rows(observed)["J-105"]
        self.assertEqual(row.state, "conflict")
        self.assertIn("fb-bot/integrate-j105", row.why)

    def test_reads_once_per_run(self):
        self.assertIs(self.sw.plan_reads(self.model), self.sw.plan_reads(self.model))

    def test_failed_compare_counts_as_live(self):
        def boom(path, accept=""):
            raise switch.GitHubError("GET", path, "HTTP 502")
        self.sw.gh.get = boom
        r = self.sw.plan_reads(self.model)
        self.assertEqual(r["inactive"], {"s101"})
        self.assertEqual(self.rows(r)["J-102"].state, "pushed")
        self.assertIsNone(r["planned_at"])

    def test_a_line_whose_issue_left_the_listing_is_closed_only_when_read_closed(self):
        model = plan.parse(PLAN + "J-107 claude #107 | closed\nJ-108 claude #108 | another job's number\n")
        self.sw.gh.issue = {107: {"number": 107, "state": "closed", "title": "J-107 done"},
                            108: {"number": 108, "state": "closed", "title": "J-999 not this job"}}
        r = self.sw.plan_reads(model)
        self.assertEqual(r["closed"], {"J-107"})
        rows = self.rows(r, model)
        self.assertEqual((rows["J-107"].state, rows["J-108"].state), ("closed", "conflict"))


class QueueSignal(unittest.TestCase):
    def test_owner_conflict_does_not_pretend_the_ready_queue_is_low(self):
        model = plan.parse("limit muse 6\n" + "".join(
            f"J-{n} muse #{n} | ready {n}\n" for n in range(101, 110)))
        issues = [{"number": n, "title": f"J-{n} job", "state": "open"} for n in range(101, 110)]
        reads = {"issues": issues, "branches": [{"name": "claude/integrate-j109", "commit": {"sha": "foreign"}}],
                 "prs": [], "inactive": frozenset()}
        rows = machine.table(model, machine.collect(model, reads))
        sw = switch.Switch.__new__(switch.Switch)
        sw.plan_mode = lambda: "live"
        sw.plan_table = lambda steered=(): (model, reads, machine.Mail(), rows)
        self.assertEqual(rows[-1].state, "conflict")   # it was NOT-OWNER J-109
        self.assertEqual(sw.queue_check(), ())         # eight unstarted jobs: the conflict is not a low queue

    def test_a_low_queue_is_counted_from_the_table(self):
        model = plan.parse("limit muse 6\nJ-101 muse #101 | one\nJ-102 muse #102 hold: parked | two\n")
        issues = [{"number": n, "title": f"J-{n} job", "state": "open"} for n in (101, 102)]
        rows = machine.table(model, machine.collect(model, {"issues": issues}))
        sw = switch.Switch.__new__(switch.Switch)
        sw.plan_mode = lambda: "live"
        sw.plan_table = lambda steered=(): (model, {}, machine.Mail(), rows)
        (item,) = sw.queue_check()
        self.assertEqual(item.kind, "QUEUE-LOW")
        self.assertIn("1 unstarted Muse jobs", item.line)
        self.assertIn("J-101", item.line)
        self.assertNotIn("J-102", item.line)   # a held job is not ready work


class Whole(unittest.TestCase):
    """11-12: 647 branches, a 500-item paged() cap, and fb-bot/integrate-j109 was never seen."""

    class Capped:
        def __init__(self, n):
            self.all = [{"name": f"b{i}"} for i in range(n)]

        def paged(self, path, key=""):
            return self.all[:500]

        def get(self, path, accept=""):
            page = int(path.rsplit("page=", 1)[1])
            return self.all[(page - 1) * 100:page * 100]

    def test_reads_past_the_client_cap(self):
        self.assertEqual(len(plan.whole(self.Capped(647), "repos/x/branches")), 647)
        self.assertEqual(len(plan.whole(self.Capped(500), "repos/x/branches")), 500)
        self.assertEqual(len(plan.whole(self.Capped(42), "repos/x/branches")), 42)

    def test_refuses_a_short_count(self):
        with self.assertRaises(RuntimeError):
            plan.whole(self.Capped(plan.ALL_PAGES * 100 + 1), "repos/x/branches")


if __name__ == "__main__":
    unittest.main()
