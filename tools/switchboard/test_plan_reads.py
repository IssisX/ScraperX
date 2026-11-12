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


if __name__ == "__main__":
    unittest.main()
