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
BOARDS = f"repos/{REPO}/issues?labels=board&state=open&per_page=100&page=1"   # the board is found by label (board_issue)
BOARD_COMMENTS = f"repos/{REPO}/issues/{switch.BOARD_ISSUE}/comments?per_page=100&page=1"


def codex_clean() -> dict:
    body = ("<!-- codex-review-state:v1 &#123;\"code_review\":&#123;"
            f"\"commit_sha\":\"{HEAD}\",\"has_unresolved_findings\":false,\"status\":\"completed\"&#125;&#125; -->")
    return {"id": 1, "body": body, "user": {"login": rules.CODEX_LOGIN}, "created_at": fb.ago(1), "updated_at": fb.ago(1)}


def github(comments: list, conclusion: str) -> dict:
    """GitHub's own answers for PR #9, path -> body."""
    return {
        f"repos/{REPO}/pulls/9": {"number": 9, "title": "t", "body": "", "draft": False, "labels": [], "mergeable": True,
                                  "updated_at": fb.ago(1),
                                  "head": {"sha": HEAD, "ref": "claude/integrate-x", "repo": {"full_name": REPO}},
                                  "base": {"ref": rules.TRUNK}},
        f"repos/{REPO}/commits/{HEAD}": {"sha": HEAD, "commit": {"message": "x", "committer": {"date": fb.ago(5)}}},
        RUNS: {"workflow_runs": [{"id": 7, "status": "completed", "conclusion": conclusion, "created_at": fb.ago(3),
                                  "updated_at": fb.ago(2), "html_url": "https://x/runs/7", "head_branch": "claude/integrate-x"}]},
        JOBS: {"jobs": [{"name": "native-tests", "status": "completed", "conclusion": conclusion}]},
        COMMENTS: comments,
        f"repos/{REPO}/pulls/9/reviews?per_page=100&page=1": [],
        f"repos/{REPO}/issues?labels=job&state=open&per_page=100&page=1": [],
        BOARDS: [{"number": switch.BOARD_ISSUE}],
        BOARD_COMMENTS: [],
        f"repos/{REPO}/pulls/9/files?per_page=100&page=1": [{"filename": "tools/x.py"}],
        f"repos/{REPO}/issues/9/reactions?per_page=100&page=1": [],   # read before Codex is asked (codex_first)
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
    sw.rc_fresh = []   # gh.fresh_only at each rule check: the check and the decision on it read nothing cached

    def rule_check(number, head):
        sw.rc_fresh.append(gh.fresh_only)
        return fb.RC_CLEAN
    sw.run_rule_check = rule_check
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


    def test_a_human_merge_is_decided_on_fresh_reads_too(self):
        # the security pass's note on #360: a HUMAN-MERGE (a MERGE-BY-HAND line in the inbox) and the rule check
        # behind it are read under gh.fresh() as a MERGE is, never from the cache file
        real = github([codex_clean()], "success")
        real[f"repos/{REPO}/pulls/9/files?per_page=100&page=1"] = [{"filename": ".github/workflows/x.yml"}]
        steered, writes, (_, sw) = sweep_one(real, {p: b for p, b in real.items()})   # every read cached
        self.assertEqual(steered[-1].decision.action, "HUMAN-MERGE")   # no merge token in this run
        self.assertTrue(any(l.startswith("FRESH       #9") for l in sw.lines), sw.lines)
        for path in (COMMENTS, RUNS, JOBS, f"repos/{REPO}/pulls/9"):
            self.assertIn(path, sw.gh.runner.uncached)
        self.assertTrue(sw.rc_fresh and all(sw.rc_fresh), sw.rc_fresh)
        self.assertFalse(any(path.endswith("/merge") for _, path in writes), writes)

    def test_a_planted_board_never_hides_the_real_boards_fix(self):
        # J-072: the board is found by label. Muse's mailed FIX on a PR with no job issue is posted on the board, so
        # a planted listing that names another issue would hide it: the merge facts read the listing fresh too.
        fix = {"id": 5, "user": {"login": "IssisX"}, "created_at": fb.ago(1), "updated_at": fb.ago(1),
               "body": f"**Muse (via email bridge): review of #9 @ {HEAD[:8]}: FIX**\n\ncommand: make test\n"
                       "input: none\nexpected: green\nobserved: red"}
        real = github([codex_clean()], "success")
        real[BOARD_COMMENTS] = [fix]
        real[f"repos/{REPO}/issues/999/comments?per_page=100&page=1"] = []
        steered, writes, (cached_facts, sw) = sweep_one(real, {BOARDS: [{"number": 999}]})
        self.assertEqual(rules.merge_decision(cached_facts, "anthropic", fb.CodexFirst.TEAM).action, "MERGE",
                         "the poisoned cache alone would have merged")
        self.assertEqual(steered[-1].decision.action, "FIX")
        self.assertFalse(any(path.endswith("/merge") for _, path in writes), writes)
        self.assertIn(BOARDS, sw.gh.runner.uncached)

    def test_the_merge_facts_never_take_the_board_from_the_cache_files_state(self):
        # GitHub lists no open board (the label came off): the sweep state rides in the cache file, so under fresh
        # the last board it names is not used; the fallback is the code's own
        real = github([codex_clean()], "success")
        real[BOARDS] = []
        gh = ghapi.GitHub(REPO, dry_run=False, log=lambda line: None, runner=FakeGh(real), sleep=lambda s: None)
        gh.state = {"board": 999}
        sw = fb.make(gh, team=fb.CodexFirst.TEAM)
        self.assertEqual(sw.board_issue(), 999)   # an ordinary read: the last board a sweep found
        sw._board_issue = None
        with gh.fresh():
            self.assertEqual(sw.board_issue(), switch.BOARD_ISSUE)

    def test_a_board_read_that_fails_under_fresh_raises_like_any_merge_fact(self):
        class Down(FakeGh):
            def __call__(self, args, **kw):
                if BOARDS in args:
                    return types.SimpleNamespace(returncode=1, stdout=b"HTTP/2.0 502 Bad Gateway\r\n\r\n{}",
                                                 stderr=b"gh: Bad Gateway (HTTP 502)")
                return super().__call__(args, **kw)
        gh = ghapi.GitHub(REPO, dry_run=False, log=lambda line: None, sleep=lambda s: None,
                          runner=Down(github([codex_clean()], "success")))
        sw = fb.make(gh, team=fb.CodexFirst.TEAM)
        with mock.patch("time.sleep"):   # ghapi's retry of a failed GET
            self.assertEqual(sw.board_issue(), switch.BOARD_ISSUE)   # an ordinary read falls back, with a warning
            sw._board_issue = None
            with gh.fresh(), self.assertRaises(switch.GitHubError):
                sw.board_issue()

    def test_the_rule_check_of_a_merge_runs_under_fresh(self):
        real = github([codex_clean()], "success")
        steered, _, (_, sw) = sweep_one(real, {p: b for p, b in real.items()})
        self.assertEqual(steered[-1].decision.action, "MERGE")
        self.assertTrue(sw.rc_fresh and all(sw.rc_fresh), sw.rc_fresh)


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
            self.assertFalse(os.path.exists(path))   # read as cold, so removed: it is never handed on (BadFile)
            with open(path, "w", encoding="utf-8") as f:
                json.dump({"v": 2, "etags": {"a|": {"etag": '"e"', "body": "[9]"}}, "state": {"steer": {}}}, f)
            local = self.client("")   # a local run with no key reads it (the dry measurements)
            local.load_etags(path)
            self.assertEqual(list(local.etags), ["a|"])

    def test_the_workflow_gives_the_key_to_both_jobs_that_read_the_cache(self):
        wf = (HERE.parents[1] / ".github/workflows/switchboard.yml").read_text(encoding="utf-8")
        for job in ("ask:", "steer:"):
            part = wf.split(f"\n  {job}\n", 1)[1].split("\n  steer:\n" if job == "ask:" else "\n\n\n", 1)[0]
            self.assertIn("SWITCHBOARD_REVIEW_KEY: ${{ secrets.SWITCHBOARD_REVIEW_KEY }}", part, job)
            self.assertIn("environment: switchboard-merge", part, job)


class BadFile(unittest.TestCase):
    """The security pass's note on #360: a restored file whose mac is not ASCII made compare_digest raise TypeError,
    and a lone surrogate in its payload made _mac raise UnicodeEncodeError, both before main's try, so the switch
    died with the bad file still on disk and the save step handed it on as the newest entry. Each is a cold cache now,
    the file is removed at once, and main goes on and saves a fresh signed file."""

    KEY = "review-key"

    def signed(self, d: str) -> tuple[str, dict]:
        path = os.path.join(d, "etags.json")
        gh = ghapi.GitHub(REPO, True, log=lambda line: None)
        gh.set_cache_key(self.KEY)
        gh.etags, gh.state, gh.used = {"a|": {"etag": '"e"', "body": "[1]", "t": 1.0}}, {"steer": {"stamp": "x"}}, {"a|"}
        gh.save_etags(path, now=2.0)
        with open(path, encoding="utf-8") as f:
            return path, json.load(f)

    @staticmethod
    def cases(good: dict) -> list[tuple[str, str]]:
        """(what, the file's text): each must read as a cold cache. json.dumps writes ASCII, so a lone surrogate goes
        in as its escape, which json.load turns back into the lone surrogate."""
        return [
            ("a non-ASCII mac", json.dumps(dict(good, mac="é" * 64))),
            ("a lone surrogate in the mac", json.dumps(dict(good, mac="\ud800" + good["mac"][1:]))),
            ("a lone surrogate in the payload", json.dumps(dict(good, payload=good["payload"][:-1] + "\ud800}"))),
            ("a mac that is a number", json.dumps(dict(good, mac=7))),
            ("a payload that is a list", json.dumps(dict(good, payload=["x"]))),
            ("a list, not an object", json.dumps([good])),
            ("bytes that are not UTF-8", "\udcff"),   # written with surrogateescape: the byte 0xff
            ("not JSON", "{"),
        ]

    @staticmethod
    def write(path: str, text: str) -> None:
        with open(path, "w", encoding="utf-8", errors="surrogateescape") as f:
            f.write(text)

    def test_each_bad_file_is_a_cold_cache_never_an_exception_and_is_removed(self):
        with tempfile.TemporaryDirectory() as d:
            path, good = self.signed(d)
            for what, text in self.cases(good):
                self.write(path, text)
                lines: list[str] = []
                gh = ghapi.GitHub(REPO, True, log=lines.append)
                gh.set_cache_key(self.KEY)
                gh.load_etags(path)   # raises nothing
                self.assertEqual((gh.etags, gh.state), ({}, {}), what)
                self.assertFalse(os.path.exists(path), what)   # a run that dies after this hands nothing on
                self.assertTrue(any("read as a cold cache" in l for l in lines), (what, lines))

    def test_main_goes_on_and_saves_a_fresh_signed_file_not_the_bad_one(self):
        def stub(gh, root, now):
            return types.SimpleNamespace(gh=gh, out=lambda line: None, merge_token="", review_key="")
        with tempfile.TemporaryDirectory() as d:
            path, good = self.signed(d)
            for what, text in self.cases(good):
                self.write(path, text)
                with mock.patch.dict(os.environ, {switch.ETAG_CACHE_ENV: path, switch.REVIEW_KEY_ENV: self.KEY}), \
                        mock.patch.object(sys, "argv", ["switch.py", "--part", "steer"]), \
                        mock.patch.object(switch, "Switch", side_effect=stub), \
                        mock.patch.object(switch, "run_sweep", return_value=0):
                    self.assertEqual(switch.main(), 0, what)   # never an exception before main's try
                with open(path, encoding="utf-8") as f:
                    saved = json.load(f)
                self.assertEqual(saved.get("v"), 3, what)
                self.assertNotIn(saved["mac"], text, what)   # never the bad file handed on
                again = ghapi.GitHub(REPO, True, log=lambda line: None)
                again.set_cache_key(self.KEY)
                again.load_etags(path)
                self.assertTrue(os.path.exists(path), what)   # it verifies: a fresh signed file, read and kept
                self.assertEqual((again.etags, again.state), ({}, {}), what)


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
