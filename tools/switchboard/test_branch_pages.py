"""Every branch listing the switch reads is whole (11-12: 647 branches; ghapi.paged stops at 500 without saying so, and
fb-bot/integrate-j109 was never seen). The auto-open and the queue check read past the client's cap through
plan.whole, and a listing longer than plan.ALL_PAGES pages raises instead of being read short. No network."""
from __future__ import annotations

import datetime as dt
import pathlib
import re
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import muse_queue  # noqa: E402
import plan  # noqa: E402
import switch  # noqa: E402
import test_auto_pr  # noqa: E402

LATE = "fb-bot/integrate-j642-late"   # the 641st branch: on page 7


def many(n: int, late_at: int | None = None, late_sha: str = "") -> list[dict]:
    out = [{"name": f"claude/b{i:04d}", "commit": {"sha": f"{i + 1:040x}"}} for i in range(n)]
    if late_at is not None:
        out[late_at] = {"name": LATE, "commit": {"sha": late_sha}}
    return out


class Paged(test_auto_pr.FakeGH):
    """test_auto_pr's GitHub, with the branch listing paged the way ghapi reads it: paged() stops at
    ghapi.MAX_PAGES pages, silently; get() reads any one page."""

    def paged(self, path, key=""):
        if path.startswith(f"repos/{self.repo}/branches"):
            self.gets.append(path)
            return list(self.branches[:switch.ghapi.MAX_PAGES * 100])
        return super().paged(path, key)

    def get(self, path, accept=""):
        if path.startswith(f"repos/{self.repo}/branches?"):
            self.gets.append(path)
            page = int(re.search(r"[?&]page=(\d+)", path).group(1))
            return self.branches[(page - 1) * 100: page * 100]
        return super().get(path, accept)


class AutoOpen(unittest.TestCase):
    def test_a_branch_on_page_7_is_seen_and_opened(self):
        gh, head = test_auto_pr.green_setup(n=900, name=LATE, job="J-642")
        gh.__class__ = Paged
        gh.branches = many(647, late_at=640, late_sha=head)
        sw = test_auto_pr.make(gh)
        self.assertEqual(sw.auto_open_prs(), 1)
        (method, path, body), = gh.writes
        self.assertEqual((path, body["head"]), ("repos/IssisX/ScraperX/pulls", LATE))
        self.assertTrue(any("page=7" in g for g in gh.gets), gh.gets)

    def test_past_the_bound_it_raises_and_says_the_bound(self):
        gh, _ = test_auto_pr.green_setup()
        gh.__class__ = Paged
        gh.branches = many(plan.ALL_PAGES * 100 + 1)
        with self.assertRaisesRegex(RuntimeError, f"more than {plan.ALL_PAGES * 100} items"):
            test_auto_pr.make(gh).auto_open_prs()
        self.assertEqual(gh.writes, [])


class QueueBranches(unittest.TestCase):
    class GH:
        repo = "IssisX/ScraperX"

        def __init__(self, branches):
            self.branches, self.gets = branches, []

        def get(self, path, accept=""):
            self.gets.append(path)
            page = int(re.search(r"[?&]page=(\d+)", path).group(1))
            return self.branches[(page - 1) * 100: page * 100]

        def paged(self, path, key=""):
            items = []
            for page in range(1, switch.ghapi.MAX_PAGES + 1):   # as ghapi.paged: stops at the cap, silently
                batch = self.get(f"{path}?per_page=100&page={page}")
                items.extend(batch)
                if len(batch) < 100:
                    break
            return items

    NOW = dt.datetime(2026, 11, 12, 20, 0, tzinfo=dt.timezone.utc)

    def test_a_branch_on_page_7_is_seen(self):
        r = muse_queue.Reads(self.GH(many(647, late_at=640, late_sha="a" * 40)), self.NOW, 1)
        names = [b["name"] for b in r.branches()]
        self.assertEqual(len(names), 647)
        self.assertIn(LATE, names)

    def test_past_the_bound_the_check_raises_rather_than_count_unstarted(self):
        r = muse_queue.Reads(self.GH(many(plan.ALL_PAGES * 100 + 1)), self.NOW, 1)
        with self.assertRaisesRegex(RuntimeError, f"more than {plan.ALL_PAGES * 100} items"):
            r.branches()


if __name__ == "__main__":
    unittest.main()
