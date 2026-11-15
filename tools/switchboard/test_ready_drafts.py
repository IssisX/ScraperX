"""11-15: a green draft PR is marked ready for review by the switch (green drafts sat 5 to 17 hours unreviewed:
#568, #466, #569). Red, held, inbox, wip, fork and non-trunk drafts are not.
Run: cd tools/switchboard && python3 -m unittest test_ready_drafts
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
import types
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import ghapi  # noqa: E402
import rules  # noqa: E402
import switch  # noqa: E402
import test_inbox  # noqa: E402
import test_sweep_calls  # noqa: E402

HEAD = "96" * 20
NODE = "PR_kwDOtest569"


def plan(**holds) -> object:
    """A read plan: J-096 and J-044 not held unless named in `holds` (job id with _ for -: J_044="parked")."""
    lines = {"J-096": "", "J-044": "", "J-015": ""}
    lines.update({k.replace("_", "-"): v for k, v in holds.items()})
    model = {j: types.SimpleNamespace(job=j, hold=h) for j, h in lines.items()}
    return types.SimpleNamespace(by_job=lambda: model)


def facts(**kw) -> rules.PrFacts:
    base = dict(number=569, head_sha=HEAD, head_ref="claude/integrate-j096", base_ref=rules.TRUNK, draft=True,
                mergeable=True, head_committed_at="2026-11-14T10:00:00Z", proof="green", verdicts=(),
                needs=frozenset(), rule_check_block=False, touches_workflows=False)
    base.update(kw)
    return rules.PrFacts(**base)


class GH:
    """The client calls ready_draft makes: the GraphQL undraft and the one comment."""
    repo = "IssisX/ScraperX"

    def __init__(self, answer=True, refuse=False):
        self.answer, self.refuse = answer, refuse
        self.marked, self.comments = [], []

    def mark_ready(self, node_id, number):
        if self.refuse:
            raise switch.GitHubError("POST", "graphql", "Resource not accessible by integration")
        self.marked.append((node_id, number))
        return self.answer

    def comment(self, number, body, what=""):
        self.comments.append((number, body))


def make(gh, f: rules.PrFacts, ctx: dict, model=None, plan_error=False):
    sw = test_inbox.make(gh)
    fresh = []

    def read_plan():
        if plan_error:
            raise RuntimeError("invalid collaboration plan: x")
        return model or plan()

    def fresh_facts(number):   # GitHub's own answer: the same PR, read again
        fresh.append(number)
        return f, dict(ctx, fresh=True)

    sw.read_plan, sw.fresh_facts = read_plan, fresh_facts
    return sw, fresh


def ctx(**kw) -> dict:
    base = {"job": "J-096", "own_head": True, "node_id": NODE, "body": "Job: J-096"}
    base.update(kw)
    return base


class ReadyDraft(unittest.TestCase):
    def run_one(self, f, c=None, model=None, gh=None, plan_error=False):
        gh = gh or GH()
        c = ctx() if c is None else c
        sw, fresh = make(gh, f, c, model, plan_error)
        got = sw.ready_draft(f, c)
        return gh, sw, got, fresh

    def test_a_green_unheld_draft_is_marked_ready_with_one_comment(self):
        gh, sw, got, fresh = self.run_one(facts())
        self.assertEqual(gh.marked, [(NODE, 569)])
        self.assertEqual(len(gh.comments), 1)
        self.assertEqual(gh.comments[0][0], 569)
        self.assertIn("ready for review", gh.comments[0][1])
        self.assertFalse(got.draft)   # the rest of this sweep reads it as ready
        self.assertEqual(fresh, [569])   # decided on GitHub's own answers, uncached
        self.assertEqual(sum(l.startswith("READY       #569") for l in sw.lines), 1)
        self.assertEqual(sw.readied, 1)

    def test_drafts_that_are_never_marked(self):
        cases = {
            "red": (facts(proof="red: native"), ctx(), None),
            "running": (facts(proof="running"), ctx(), None),
            "held": (facts(), ctx(), plan(J_096="parked 11-14")),
            "held lettered split": (facts(head_ref="claude/integrate-j044c"), ctx(job="J-044c"),
                                    plan(J_044="parked")),
            "held by branch job": (facts(head_ref="fb-bot/integrate-protected-j015"), ctx(job=""),
                                   plan(J_015="paused")),
            "inbox number": (facts(number=switch.INBOX_PR), ctx(), None),
            "inbox branch": (facts(head_ref=switch.INBOX_BRANCH), ctx(), None),
            "wip": (facts(head_ref="fb-bot/wip-j096"), ctx(), None),
            "non-trunk": (facts(base_ref="main"), ctx(), None),
            "stacked": (facts(base_ref="claude/integrate-j090"), ctx(), None),
            "fork": (facts(), ctx(own_head=False), None),
            "no job": (facts(head_ref="claude/integrate-wake-cut"), ctx(job=""), None),
            "job with no plan line": (facts(head_ref="claude/integrate-j999"), ctx(job="J-999"), None),
        }
        for name, (f, c, model) in cases.items():
            with self.subTest(name):
                gh, sw, got, fresh = self.run_one(f, c, model)
                self.assertEqual(gh.marked, [])
                self.assertEqual(gh.comments, [])
                self.assertTrue(got.draft)
                self.assertEqual(fresh, [])   # refused before any read

    def test_an_unread_plan_marks_nothing(self):
        gh, sw, got, _ = self.run_one(facts(), plan_error=True)
        self.assertEqual((gh.marked, gh.comments), ([], []))
        self.assertTrue(got.draft)

    def test_a_ready_pr_is_left_alone(self):
        gh, sw, got, fresh = self.run_one(facts(draft=False))
        self.assertEqual((gh.marked, gh.comments, fresh), ([], [], []))

    def test_a_refused_undraft_is_a_note_not_an_error(self):
        gh, sw, got, _ = self.run_one(facts(), gh=GH(refuse=True))
        self.assertEqual(gh.comments, [])
        self.assertTrue(got.draft)
        self.assertTrue(any(l.startswith("NOTE        #569 not marked ready") for l in sw.lines))

    def test_a_dry_run_or_a_still_draft_answer_comments_nothing(self):
        gh, sw, got, _ = self.run_one(facts(), gh=GH(answer=False))
        self.assertEqual(gh.comments, [])
        self.assertTrue(got.draft)


class MarkReadyClient(unittest.TestCase):
    def client(self, answer: dict, dry_run=False):
        sent = []

        def runner(args, input=None, **kw):
            sent.append((args, json.loads(input) if input else None))
            head = b"HTTP/2.0 200 OK\r\nx-ratelimit-resource: graphql\r\nx-ratelimit-remaining: 4999\r\n\r\n"
            return types.SimpleNamespace(returncode=0, stdout=head + json.dumps(answer).encode(), stderr=b"")

        gh = ghapi.GitHub("IssisX/ScraperX", dry_run=dry_run, log=lambda line: None, runner=runner)
        return gh, sent

    def test_the_mutation_takes_the_node_id_as_a_variable(self):
        gh, sent = self.client({"data": {"markPullRequestReadyForReview": {"pullRequest": {"isDraft": False}}}})
        gh.remaining = 700
        self.assertTrue(gh.mark_ready(NODE, 569))
        args, body = sent[0]
        self.assertEqual(args[args.index("-X") + 1:args.index("-X") + 3], ["POST", "graphql"])
        self.assertIn("markPullRequestReadyForReview", body["query"])
        self.assertEqual(body["variables"], {"id": NODE})
        self.assertEqual(gh.remaining, 700)   # GraphQL's own bucket never moves the REST rate floor

    def test_an_errors_answer_raises(self):
        gh, _ = self.client({"errors": [{"message": "Resource not accessible by integration"}]})
        with self.assertRaises(ghapi.GitHubError):
            gh.mark_ready(NODE, 569)

    def test_a_dry_run_sends_nothing(self):
        gh, sent = self.client({}, dry_run=True)
        self.assertFalse(gh.mark_ready(NODE, 569))
        self.assertEqual(sent, [])


class SweepWiring(unittest.TestCase):
    def test_the_sweep_marks_a_green_draft_ready_and_steers_it_as_ready(self):
        gh = test_sweep_calls.CountingGH([test_sweep_calls.pr(96)], dry_run=False)
        gh.marked = []
        gh.mark_ready = lambda node_id, number: (gh.marked.append(number), True)[1]
        sw = test_inbox.make(gh, test_sweep_calls.NOW)
        sw.root = HERE.parents[1]
        sw.merge_token, sw.review_key, sw._jobs, sw._board = "", "", None, None
        sw.plan_mode = lambda: "off"
        sw.read_plan = plan
        gh.state, gh.remaining, gh.limit, gh.reset = {}, None, None, None
        f = facts(number=96, head_ref="fb-bot/integrate-j096")
        sw.pr_facts = lambda p: (f, ctx())
        sw.fresh_facts = lambda n: (f, ctx(fresh=True))
        seen = []
        sw.steer_pr = lambda fx, c, record=None: seen.append(fx.draft)
        args = argparse.Namespace(part="steer", pr=0, sha="", issue=0, label="", digest=False, force_digest=False,
                                  dry_run=False)
        switch.run_sweep(sw, args, test_sweep_calls.NOW)
        self.assertEqual(gh.marked, [96])
        self.assertEqual(seen, [False])
        self.assertIn("READY       1 green draft(s) marked ready for review this sweep", sw.lines)


if __name__ == "__main__":
    unittest.main()
