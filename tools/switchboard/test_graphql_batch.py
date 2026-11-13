"""11-13: the changed PRs' facts in GraphQL queries, not five REST reads a PR.

Measured on the live state (a dry fast sweep on trunk's code): of 198 counted calls, the per-PR fan-out (the PR for
mergeable, its head commit, its comments, reviews and files) was 61 for six PRs read twice. GraphQL has its own
hourly points, so one query for up to GRAPHQL_PRS PRs moves that load off the REST hour the switchboard runs dry.
A merge is still decided on fresh REST reads (fresh_facts reads everything again, never the batch).
Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
"""

from __future__ import annotations

import pathlib
import re
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import switch  # noqa: E402
import test_sweep_calls as tsc  # noqa: E402

FANOUT = re.compile(r"repos/[^/]+/[^/]+/(pulls/4[78]\d(/files|/reviews)?|commits/[0-9a-f]+|issues/4[78]\d/comments)(\?|$)")


def node(n: int, comments: int = 2, head: str = "", bot: bool = True) -> dict:
    head = head or f"{n:040x}"
    return {"number": n, "headRefOid": head, "mergeable": "MERGEABLE",
            "commits": {"nodes": [{"commit": {"oid": head, "committedDate": "2026-11-06T17:00:00Z"}}]},
            "files": {"totalCount": 1, "nodes": [{"path": "src/x.cpp"}]},
            "comments": {"totalCount": comments, "nodes": [
                {"databaseId": 900 + i, "body": f"note {i}", "createdAt": "2026-11-06T18:00:00Z",
                 "updatedAt": "2026-11-06T18:00:00Z", "url": f"https://x/{i}",
                 "author": {"__typename": "Bot" if bot else "User", "login": "github-actions" if bot else "IssisX"}}
                for i in range(min(comments, 100))]},
            "reviews": {"totalCount": 0, "nodes": []}}


class GraphGH(tsc.CountingGH):
    """The counting fake, plus GraphQL: each aliased PR answered from `nodes` (a missing one is not read)."""

    def __init__(self, prs, nodes=None, fail=False, **kw):
        super().__init__(prs, **kw)
        self.nodes, self.fail, self.queries = dict(nodes or {}), fail, []
        self.graphql_calls = 0

    def graphql(self, query, variables=None):
        self.queries.append(query)
        self.calls.append("POST graphql")
        if self.fail:
            raise switch.GitHubError("POST", "graphql", "Field 'x' doesn't exist on type 'PullRequest'", 0)
        return {"repository": {f"p{n}": self.nodes.get(int(n)) for n in re.findall(r"p(\d+): pullRequest", query)}}


def fast(gh, prs, changed):
    state = tsc.last_state(prs)
    for n in changed:
        state["prs"][str(n)]["cold"] = False
    return tsc.sweep(gh, state)


class Batch(unittest.TestCase):
    prs = [tsc.pr(n) for n in range(470, 488)]
    changed = (470, 473, 476, 479, 482)

    def fanout(self, gh) -> int:
        return sum(1 for c in gh.calls if FANOUT.search(c))

    def test_five_changed_prs_cost_one_query_not_their_rest_fan_out(self):
        before = tsc.CountingGH(self.prs)
        fast(before, self.prs, self.changed)
        after = GraphGH(self.prs, {n: node(n) for n in self.changed})
        status, read, _ = fast(after, self.prs, self.changed)
        self.assertEqual(status, 0)
        self.assertEqual(sorted(read), sorted(self.changed))
        self.assertEqual(len(after.queries), 1)
        self.assertGreaterEqual(self.fanout(before), 5 * 5)
        self.assertEqual(self.fanout(after), 0, after.calls)
        self.assertLess(len(after.calls), len(before.calls) - 20, (len(before.calls), len(after.calls)))

    def test_the_facts_read_are_the_rest_ones(self):
        gh = GraphGH(self.prs, {470: node(470), 473: node(473)})
        sw = tsc.test_inbox.make(gh)
        sw.prefetch([470, 473])
        got = sw._batch[470]
        self.assertEqual(got["pr"]["mergeable"], True)
        self.assertEqual(got["commit"]["commit"]["committer"]["date"], "2026-11-06T17:00:00Z")
        self.assertEqual(got["comments"][0]["user"]["login"], "github-actions[bot]", "a Bot's login as REST gives it")
        self.assertEqual(got["comments"][0]["id"], 900)
        self.assertEqual(got["files"], [{"filename": "src/x.cpp"}])
        self.assertEqual(switch.rest_shapes(node(1, bot=False))["comments"][0]["user"]["login"], "IssisX")
        self.assertIsNone(switch.rest_shapes(dict(node(1), mergeable="UNKNOWN"))["pr"]["mergeable"])

    def test_more_than_a_page_a_moved_head_or_a_failed_query_is_read_over_rest(self):
        big = switch.rest_shapes(node(470, comments=140))
        self.assertIsNone(big["comments"], "140 comments: the newest 100 would miss the rest")
        moved = dict(node(470), commits={"nodes": [{"commit": {"oid": "f" * 40, "committedDate": "x"}}]})
        self.assertIsNone(switch.rest_shapes(moved))
        gh = GraphGH(self.prs, fail=True)
        status, read, sw = fast(gh, self.prs, self.changed)
        self.assertEqual((status, sorted(read)), (0, sorted(self.changed)))
        self.assertTrue(any("GraphQL read of 5 PR(s) failed" in line for line in sw.lines), sw.lines)
        self.assertGreaterEqual(self.fanout(gh), 5 * 5, "every fact still read, over REST")

    def test_a_merge_is_never_decided_on_the_batch(self):
        gh = GraphGH(self.prs, {470: node(470)})
        sw = tsc.test_inbox.make(gh)
        sw._jobs = sw._board = None
        sw.prefetch([470, 473])
        gh.calls.clear()
        with gh.fresh() if hasattr(gh, "fresh") else _Fresh(gh):
            sw.pr_facts(self.prs[0])
        self.assertTrue(any(c.endswith("/pulls/470") for c in gh.calls), gh.calls)

    def test_one_pr_is_read_over_rest(self):
        gh = GraphGH(self.prs, {470: node(470)})
        fast(gh, self.prs, (470,))
        self.assertEqual(gh.queries, [])


class _Fresh:
    def __init__(self, gh):
        self.gh = gh

    def __enter__(self):
        self.gh.fresh_only = True

    def __exit__(self, *a):
        self.gh.fresh_only = False


if __name__ == "__main__":
    unittest.main()
