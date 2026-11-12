"""The switch's plan reads: history and dormant heads leave WIP, real work stays, nothing is hidden."""
import datetime as dt
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
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
                                                 "head": {"sha": "s106"}}]
        self.model = plan.parse(PLAN)

    def test_only_real_lanes_count_and_every_job_stays_visible(self):
        r = self.sw.plan_reads(self.model)
        self.assertEqual(r["inactive"], {"s101", "s102"})
        self.assertEqual(r["dormant"], {"s103"})
        self.assertFalse(any("s101" in g or "s106" in g for g in self.sw.gh.gets))  # no compare for known heads
        got = {ln.job: plan.status(ln, prs=self.sw.listing(""), branches=r["branches"], inactive=r["inactive"],
                                   merged=r["merged"], dormant=r["dormant"]) for ln in self.model.lines}
        self.assertEqual(got, {"J-101": "landed", "J-102": "queued", "J-103": "dormant", "J-104": "pushed",
                               "J-105": "contested", "J-106": "review"})
        found = plan.findings(self.model, self.sw.gh.paged("/issues?"), r["branches"], self.sw.listing(""),
                              inactive=r["inactive"], merged=r["merged"], dormant=r["dormant"])
        self.assertFalse(any("OVER-LIMIT" in f for f in found), found)   # J-104 + J-106 = 2/2
        self.assertTrue(any(f.startswith("DORMANT J-103: claude/integrate-j103") for f in found), found)
        self.assertTrue(any(f.startswith("NOT-OWNER J-105: fb-bot/integrate-j105") for f in found), found)

    def test_closed_unmerged_pr_keeps_its_head_visible_as_unresolved_work(self):
        old = self.sw.gh.paged

        def paged(path, key=""):
            if "state=closed" in path:
                return [{"number": 1, "head": {"sha": "s101"},
                         "merged_at": "2026-11-10T00:00:00Z", "body": "Job: J-101\\n"},
                        {"number": 2, "head": {"sha": "s105"},
                         "merged_at": None, "body": "Job: J-105\\n"}]
            return old(path, key)

        self.sw.gh.paged = paged
        observed = self.sw.plan_reads(self.model)
        self.assertIn("s101", observed["inactive"])
        self.assertNotIn("s105", observed["inactive"])
        issues = self.sw.gh.paged("/issues?")
        problems = plan.findings(self.model, issues, observed["branches"], self.sw.listing(""),
                                 inactive=observed["inactive"], merged=observed["merged"],
                                 dormant=observed["dormant"])
        self.assertTrue(any(x.startswith("NOT-OWNER J-105: fb-bot/integrate-j105") for x in problems), problems)

    def test_reads_once_per_run(self):
        self.assertIs(self.sw.plan_reads(self.model), self.sw.plan_reads(self.model))

    def test_failed_compare_counts_as_live(self):
        def boom(path, accept=""):
            raise switch.GitHubError("GET", path, "HTTP 502")
        self.sw.gh.get = boom
        r = self.sw.plan_reads(self.model)
        self.assertEqual(r["inactive"], {"s101"})
        self.assertEqual(plan.status(self.model.by_job()["J-102"], branches=r["branches"], inactive=r["inactive"]),
                         "pushed")


class QueueSignal(unittest.TestCase):
    def test_owner_conflict_does_not_pretend_the_ready_queue_is_low(self):
        model = plan.parse("limit muse 6\\n" + "".join(
            f"J-{n} muse #{n} | ready {n}\\n" for n in range(101, 110)))
        issues = [{"number": n, "title": f"J-{n} job", "state": "open"} for n in range(101, 110)]
        sw = switch.Switch.__new__(switch.Switch)
        sw.plan_mode = lambda: "live"
        sw.read_plan = lambda: model
        sw.plan_reads = lambda _: {
            "issues": issues, "branches": [{"name": "claude/integrate-j109", "commit": {"sha": "foreign"}}],
            "prs": [], "inactive": frozenset(), "merged": frozenset(), "dormant": frozenset(),
        }
        self.assertTrue(any(x.startswith("NOT-OWNER J-109") for x in plan.findings(
            model, issues, sw.plan_reads(model)["branches"])))
        self.assertEqual(sw.queue_check(), ())


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
