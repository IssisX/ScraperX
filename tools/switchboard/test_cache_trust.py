"""J-072, the security pass on #360 (comment 6748673674): the ETag cache never decides a merge.

The cache file comes back from the Actions cache by key prefix, and any job on trunk or main can save a newer entry
there. An entry planted under GitHub's real ETag gets a 304, and its body would come back as GitHub's own answer:
forged LANDs, a forged security pass, a forged green proof. So:
  - a MERGE is decided only on a fresh read of the PR's merge facts, with no If-None-Match (switch.fresh_facts);
  - the file is signed with a key derived from SWITCHBOARD_REVIEW_KEY, which only switchboard-merge holds, and a
    file whose MAC does not verify is read as a cold cache (ghapi.load_etags);
  - a dry run never saves the file, so it cannot consume the sweep state.
These tests run the real ghapi client against a fake `gh` that answers 304 to any If-None-Match it is sent, as
GitHub does when the planted ETag is its current one.
Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
"""

from __future__ import annotations

import json
import os
import pathlib
import sys
import tempfile
import types
import unittest
from unittest import mock

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import ghapi  # noqa: E402
import rules  # noqa: E402
import switch  # noqa: E402
import test_review_fallback as fb  # noqa: E402

REPO = "IssisX/ScraperX"
HEAD = fb.HEAD
RUNS = f"repos/{REPO}/actions/workflows/{switch.PROOF_WORKFLOW}/runs?head_sha={HEAD}&per_page=5"
JOBS = f"repos/{REPO}/actions/runs/7/jobs?per_page=50"
COMMENTS = f"repos/{REPO}/issues/9/comments?per_page=100&page=1"


def codex_clean() -> dict:
    body = ("<!-- codex-review-state:v1 &#123;\"code_review\":&#123;"
            f"\"commit_sha\":\"{HEAD}\",\"has_unresolved_findings\":false,\"status\":\"completed\"&#125;&#125; -->")
    return {"id": 1, "body": body, "user": {"login": rules.CODEX_LOGIN}, "created_at": fb.ago(1), "updated_at": fb.ago(1)}


def github(comments: list, conclusion: str) -> dict:
    """GitHub's own answers for PR #9, path -> body."""
    return {
        f"repos/{REPO}/pulls/9": {"number": 9, "title": "t", "body": "", "draft": False, "labels": [], "mergeable": True,
                                  "updated_at": fb.ago(1), "head": {"sha": HEAD, "ref": "claude/integrate-x"},
                                  "base": {"ref": rules.TRUNK}},
        f"repos/{REPO}/commits/{HEAD}": {"sha": HEAD, "commit": {"message": "x", "committer": {"date": fb.ago(5)}}},
        RUNS: {"workflow_runs": [{"id": 7, "status": "completed", "conclusion": conclusion, "created_at": fb.ago(3),
                                  "updated_at": fb.ago(2), "html_url": "https://x/runs/7", "head_branch": "claude/integrate-x"}]},
        JOBS: {"jobs": [{"name": "native-tests", "status": "completed", "conclusion": conclusion}]},
        COMMENTS: comments,
        f"repos/{REPO}/pulls/9/reviews?per_page=100&page=1": [],
        f"repos/{REPO}/issues?labels=job&state=open&per_page=100&page=1": [],
        f"repos/{REPO}/issues/{switch.BOARD_ISSUE}/comments?per_page=100&page=1": [],
        f"repos/{REPO}/pulls/9/files?per_page=100&page=1": [{"filename": "tools/x.py"}],
    }


class FakeGh:
    """`gh api -i` as GitHub answers it: a request carrying If-None-Match gets 304 (the planted ETag is GitHub's
    current one), any other GET the real body; writes are recorded."""

    def __init__(self, real: dict):
        self.real, self.writes, self.uncached = real, [], []

    def __call__(self, args, input=None, capture_output=True, timeout=0, env=None):
        method, path = args[args.index("-X") + 1], args[args.index("-X") + 2]
        if method != "GET":
            self.writes.append((method, path))
            return types.SimpleNamespace(returncode=0, stdout=b"HTTP/2.0 200 OK\r\n\r\n{}", stderr=b"")
        if any(a.startswith("If-None-Match") for a in args):
            return types.SimpleNamespace(returncode=1, stdout=b"HTTP/2.0 304 Not Modified\r\n\r\n", stderr=b"gh: HTTP 304")
        self.uncached.append(path)
        body = json.dumps(self.real[path]).encode()
        return types.SimpleNamespace(returncode=0, stdout=b'HTTP/2.0 200 OK\r\nETag: "real"\r\n\r\n' + body, stderr=b"")


def sweep_one(real: dict, planted: dict) -> tuple[list, list[str], object]:
    """One steer of PR #9: its reads answered from a cache file holding `planted` bodies (a poisoned entry)."""
    gh = ghapi.GitHub(REPO, dry_run=False, log=lambda line: None, runner=FakeGh(real), sleep=lambda s: None)
    gh.etags = {f"{p}|": {"etag": '"planted"', "body": json.dumps(b), "t": 1.0} for p, b in planted.items()}
    sw = fb.make(gh, team=fb.CodexFirst.TEAM)
    sw._jobs, sw._board = None, None
    sw.merge_token = sw.review_key = ""
    sw.api_ok = lambda c: True
    sw.review_load = lambda: ({}, {})
    pr = gh.get(f"repos/{REPO}/pulls/9")
    facts, ctx = sw.pr_facts(pr)
    steered: list = []
    sw.steer_pr(facts, ctx, steered)
    return steered, gh.runner.writes, (facts, sw)


class FreshMerge(unittest.TestCase):
    def test_a_planted_review_never_yields_merge(self):
        real = github([], "success")   # GitHub: green, but no review at all
        steered, writes, (cached_facts, sw) = sweep_one(real, {COMMENTS: [codex_clean()]})
        self.assertEqual(rules.merge_decision(cached_facts, "anthropic", fb.CodexFirst.TEAM).action, "MERGE",
                         "the poisoned cache alone would have merged")
        self.assertNotEqual(steered[-1].decision.action, "MERGE")
        self.assertFalse(any(path.endswith("/merge") for _, path in writes), writes)
        self.assertTrue(any(l.startswith("FRESH       #9") for l in sw.lines))

    def test_a_planted_green_proof_never_yields_merge(self):
        real = github([codex_clean()], "failure")   # GitHub: reviewed, but the proof is red
        steered, writes, _ = sweep_one(real, {RUNS: real[RUNS] | {"workflow_runs": [dict(real[RUNS]["workflow_runs"][0],
                                                                                         conclusion="success")]},
                                              JOBS: {"jobs": [{"name": "native-tests", "status": "completed",
                                                               "conclusion": "success"}]}})
        self.assertEqual(steered[-1].decision.action, "FIX-CI")
        self.assertFalse(any(path.endswith("/merge") for _, path in writes), writes)

    def test_the_fresh_read_sends_no_etag_and_still_merges_real_facts(self):
        real = github([codex_clean()], "success")   # GitHub itself says: reviewed and green
        steered, writes, (_, sw) = sweep_one(real, {p: b for p, b in real.items()})   # every read cached (real bodies)
        self.assertEqual(steered[-1].decision.action, "MERGE")
        self.assertIn(("PUT", f"repos/{REPO}/pulls/9/merge"), writes)
        uncached = sw.gh.runner.uncached
        for path in (COMMENTS, RUNS, JOBS, f"repos/{REPO}/pulls/9"):   # read again from GitHub, uncached
            self.assertIn(path, uncached)


class SignedFile(unittest.TestCase):
    def client(self, key: str) -> ghapi.GitHub:
        gh = ghapi.GitHub(REPO, True, log=lambda line: None)
        gh.set_cache_key(key)
        return gh

    def test_a_signed_file_is_read_and_a_forged_one_is_a_cold_cache(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "etags.json")
            first = self.client("review-key")
            first.etags = {"a|": {"etag": '"e"', "body": "[1]", "t": 1.0}}
            first.state = {"steer": {"stamp": "x"}}
            first.used = {"a|"}
            first.save_etags(path, now=2.0)
            ok = self.client("review-key")
            ok.load_etags(path)
            self.assertEqual((list(ok.etags), ok.state), (["a|"], {"steer": {"stamp": "x"}}))
            with open(path, encoding="utf-8") as f:
                signed = json.load(f)
            forged = dict(signed, payload=signed["payload"].replace("[1]", "[2]"))
            with open(path, "w", encoding="utf-8") as f:
                json.dump(forged, f)
            bad = self.client("review-key")
            bad.load_etags(path)
            self.assertEqual((bad.etags, bad.state), ({}, {}))
            other = self.client("another-key")   # signed by someone else (no key, or another one)
            with open(path, "w", encoding="utf-8") as f:
                json.dump(signed, f)
            other.load_etags(path)
            self.assertEqual((other.etags, other.state), ({}, {}))

    def test_a_run_with_the_key_never_reads_an_unsigned_file(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "etags.json")
            with open(path, "w", encoding="utf-8") as f:
                json.dump({"v": 2, "etags": {"a|": {"etag": '"e"', "body": "[9]"}}, "state": {"steer": {}}}, f)
            gh = self.client("review-key")
            gh.load_etags(path)
            self.assertEqual((gh.etags, gh.state), ({}, {}))
            local = self.client("")   # a local run with no key reads it (the dry measurements)
            local.load_etags(path)
            self.assertEqual(list(local.etags), ["a|"])

    def test_the_workflow_gives_the_key_to_both_jobs_that_read_the_cache(self):
        wf = (HERE.parents[1] / ".github/workflows/switchboard.yml").read_text(encoding="utf-8")
        for job in ("ask:", "steer:"):
            part = wf.split(f"\n  {job}\n", 1)[1].split("\n  provision:\n" if job == "ask:" else "\n\n\n", 1)[0]
            self.assertIn("SWITCHBOARD_REVIEW_KEY: ${{ secrets.SWITCHBOARD_REVIEW_KEY }}", part, job)
            self.assertIn("environment: switchboard-merge", part, job)


class DryRun(unittest.TestCase):
    def test_a_dry_run_reads_the_cache_but_never_saves_it(self):
        for argv, saved in ((["--dry-run"], False), ([], True), (["--dry-run", "--save-dry-cache"], True)):
            with tempfile.TemporaryDirectory() as d, \
                    mock.patch.dict(os.environ, {switch.ETAG_CACHE_ENV: os.path.join(d, "etags.json")}), \
                    mock.patch.object(sys, "argv", ["switch.py", *argv]), \
                    mock.patch.object(switch, "run_sweep", return_value=0), \
                    mock.patch.object(switch.GitHub, "load_etags") as load, \
                    mock.patch.object(switch.GitHub, "save_etags") as save:
                self.assertEqual(switch.main(), 0)
                load.assert_called_once()
                self.assertEqual(save.called, saved, argv)


if __name__ == "__main__":
    unittest.main()
