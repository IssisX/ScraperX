#!/usr/bin/env python3
"""J-072: `wake:muse` never goes on a pull request. The Muse bridge's token is Issues-only: it never mails from a
PR and cannot take the label off one, so a notice under such a label reaches no one (#337, #343 and #372 carried
it on 11-10). Every writer posts Muse's mail on an issue by construction; ghapi.GitHub.write refuses the label
on a PR whoever asks, and these tests drive each writer's own label write through it.

Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
"""

from __future__ import annotations

import pathlib
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import ghapi  # noqa: E402
import switch  # noqa: E402
import test_cache_trust as ct  # noqa: E402
import test_review_fallback as fb  # noqa: E402

REPO = "IssisX/ScraperX"
PR, ISSUE = 9, 138


def client(dry_run: bool = False):
    """The real client over a fake `gh`: #9 is a pull request, #138 an issue. Returns (client, its runner)."""
    real = {f"repos/{REPO}/issues/{PR}": {"number": PR, "pull_request": {"url": "x"}},
            f"repos/{REPO}/issues/{ISSUE}": {"number": ISSUE},
            f"repos/{REPO}/issues?labels=board&state=open&per_page=100&page=1": [{"number": switch.BOARD_ISSUE}]}
    runner = ct.FakeGh(real)
    return ghapi.GitHub(REPO, dry_run=dry_run, log=lambda line: None, runner=runner, sleep=lambda s: None), runner


def labels(runner) -> list[str]:
    return [path for method, path in runner.writes if method == "POST" and path.endswith("/labels")]


class TheGuard(unittest.TestCase):
    def test_the_label_is_refused_on_a_pr_and_goes_on_an_issue(self):
        gh, runner = client()
        with self.assertRaises(ghapi.LabelRefused) as err:
            gh.write("POST", f"repos/{REPO}/issues/{PR}/labels", {"labels": ["wake:muse"]}, "label")
        self.assertIn("#9 is a pull request", str(err.exception))
        self.assertEqual(runner.writes, [])
        gh.write("POST", f"repos/{REPO}/issues/{ISSUE}/labels", {"labels": ["wake:muse"]}, "label")
        self.assertEqual(labels(runner), [f"repos/{REPO}/issues/{ISSUE}/labels"])

    def test_any_other_label_still_goes_on_a_pr_with_no_extra_read(self):
        gh, runner = client()
        gh.write("POST", f"repos/{REPO}/issues/{PR}/labels", {"labels": ["needs:eyes"]}, "label")
        self.assertEqual((labels(runner), runner.uncached), ([f"repos/{REPO}/issues/{PR}/labels"], []))

    def test_a_dry_run_refuses_it_too(self):
        gh, runner = client(dry_run=True)
        with self.assertRaises(ghapi.LabelRefused):
            gh.write("POST", f"repos/{REPO}/issues/{PR}/labels", {"labels": ["wake:muse"]}, "label")

    def test_the_refusal_is_a_github_error_so_every_writers_own_handling_sees_it(self):
        self.assertTrue(issubclass(ghapi.LabelRefused, ghapi.GitHubError))


class EachWriter(unittest.TestCase):
    """Each writer's `wake:muse` write, aimed at a PR on purpose: it never reaches GitHub."""

    def test_the_switchs_review_ask_to_muse(self):
        gh, runner = client()
        sw = fb.make(gh)
        sw._board = []
        facts = fb.pr_facts()
        # another PR (#10) stands in for the job issue: a wrong home, as a writer bug would make it (the PR's own
        # number is already sent to the board by ask_review itself)
        other = 10
        runner.real[f"repos/{REPO}/issues/{other}"] = {"number": other, "pull_request": {"url": "x"}}
        runner.real[f"repos/{REPO}/issues/{other}/comments?per_page=100&page=1"] = []
        runner.real[f"repos/{REPO}/actions/runs/7/jobs?per_page=50"] = {"jobs": []}
        with self.assertRaises(ghapi.LabelRefused):
            sw.ask_review(facts, dict(fb.ctx(issue=other), comments=[]), "muse", "")
        self.assertEqual(labels(runner), [])


if __name__ == "__main__":
    unittest.main()
