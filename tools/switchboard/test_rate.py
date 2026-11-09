"""J-072: the switch within GitHub's rate limit (run 42667757904 failed on "API rate limit exceeded for installation").

(a) conditional requests: an ETag kept between runs, a 304 answered from the cache; (b) a rate floor: only the
merge path runs, never a failed run; (c) a rate-limited call is tried twice with jitter, then left for the next
sweep; (d) the reconciler's own reads are skipped under the floor; and one PR's error is not an outage.
Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'. No network: gh is a fake.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import ghapi  # noqa: E402
import switch  # noqa: E402

PATH = "repos/IssisX/ScraperX/issues/340"
ETAG = 'W/"abc123"'


def answer(status: int, body: str = "", **headers: str) -> bytes:
    reason = {200: "OK", 304: "Not Modified", 403: "Forbidden", 429: "Too Many Requests"}.get(status, "X")
    lines = [f"HTTP/2.0 {status} {reason}"] + [f"{k.replace('_', '-')}: {v}" for k, v in headers.items()]
    return ("\r\n".join(lines) + "\r\n\r\n" + body).encode()


class Gh:
    """A fake `gh api -i`: the answers in order, every argument list kept."""

    def __init__(self, *answers):
        self.answers, self.seen = list(answers), []

    def __call__(self, args, **kw):
        self.seen.append(list(args))
        out, rc, err = self.answers.pop(0)
        return subprocess.CompletedProcess(args, rc, out, err.encode())


def client(fake: Gh, sleeps: list | None = None) -> ghapi.GitHub:
    return ghapi.GitHub("IssisX/ScraperX", False, log=lambda line: None, runner=fake,
                        sleep=(sleeps.append if sleeps is not None else lambda s: None))


class Conditional(unittest.TestCase):
    def test_a_second_read_sends_the_etag_and_a_304_is_answered_from_the_cache(self):
        fake = Gh((answer(200, '{"n": 1}', ETag=ETAG, X_RateLimit_Remaining="4999", X_RateLimit_Reset="1794248050"), 0, ""),
                  (answer(304, "", ETag=ETAG, X_RateLimit_Remaining="4999"), 1, "gh: HTTP 304"))
        gh = client(fake)
        self.assertEqual(gh.get(PATH), {"n": 1})
        self.assertEqual(gh.get(PATH), {"n": 1})
        self.assertIn("-i", fake.seen[0])
        self.assertNotIn(f"If-None-Match: {ETAG}", fake.seen[0])
        self.assertIn(f"If-None-Match: {ETAG}", fake.seen[1])
        self.assertEqual((gh.not_modified, gh.remaining, gh.reset), (1, 4999, 1794248050))

    def test_the_cache_is_carried_between_runs_and_keeps_only_what_was_read(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "etags.json")
            first = client(Gh((answer(200, "[1]", ETag=ETAG), 0, ""), (answer(200, "[2]", ETag='"x"'), 0, "")))
            first.get(PATH)
            first.get(PATH + "/comments")
            first.save_etags(path)
            second = client(Gh((answer(304, "", ETag=ETAG), 1, "gh: HTTP 304")))
            second.load_etags(path)
            self.assertEqual(second.get(PATH), [1])
            second.save_etags(path)
            with open(path, encoding="utf-8") as f:
                self.assertEqual(list(json.load(f)["etags"]), [f"{PATH}|"])   # the comments path was not read: dropped

    def test_a_broken_cache_file_is_ignored(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "etags.json")
            with open(path, "w", encoding="utf-8") as f:
                f.write("{not json")
            gh = client(Gh((answer(200, "[]"), 0, "")))
            gh.load_etags(path)
            self.assertEqual(gh.get(PATH), [])


class RateLimitedCalls(unittest.TestCase):
    LIMIT = "gh: API rate limit exceeded for installation. If you reach out to GitHub Support (HTTP 403)"

    def test_a_rate_limit_is_tried_twice_with_jitter_then_left_for_the_next_sweep(self):
        hit = (answer(403, '{"message": "API rate limit exceeded"}', X_RateLimit_Remaining="0"), 1, self.LIMIT)
        fake, sleeps = Gh(hit, hit, hit), []
        with self.assertRaises(ghapi.RateLimited):
            client(fake, sleeps).get(PATH)
        self.assertEqual(len(fake.seen), ghapi.RATE_TRIES)
        self.assertEqual(len(sleeps), ghapi.RATE_TRIES - 1)
        self.assertTrue(all(0 <= s <= ghapi.RATE_BACKOFF_S for s in sleeps))

    def test_a_429_then_an_answer_goes_on(self):
        fake, sleeps = Gh((answer(429, "", Retry_After="3"), 1, "gh: HTTP 429"), (answer(200, "[7]"), 0, "")), []
        self.assertEqual(client(fake, sleeps).get(PATH), [7])
        self.assertEqual(sleeps, [3.0])

    def test_a_write_is_retried_too_and_an_other_403_is_not_a_rate_limit(self):
        hit = (answer(403, "", X_RateLimit_Remaining="0"), 1, self.LIMIT)
        fake = Gh(hit, (answer(201, '{"id": 5}'), 0, ""))
        self.assertEqual(client(fake).comment(340, "hi"), {"id": 5})
        refused = (answer(403, "", X_RateLimit_Remaining="4000"), 1,
                   "gh: GitHub Actions is not permitted to create or approve pull requests. (HTTP 403)")
        with self.assertRaises(ghapi.GitHubError) as caught:
            client(Gh(refused)).write("POST", "repos/IssisX/ScraperX/pulls", {})
        self.assertNotIsInstance(caught.exception, ghapi.RateLimited)


def args(**kw) -> argparse.Namespace:
    base = dict(part="steer", pr=0, sha="", issue=0, label="", digest=False, force_digest=False, dry_run=False)
    return argparse.Namespace(**{**base, **kw})


def fake_switch(remaining: int, prs=(1, 2), fail=None, limit: int = 1000) -> mock.MagicMock:
    sw = mock.MagicMock()
    sw.gh.remaining, sw.gh.reset, sw.gh.limit = None, 1794248050, None
    sw.gh.get.return_value = {"resources": {"core": {"remaining": remaining, "reset": 1794248050, "limit": limit}}}
    sw.target_prs.return_value = [{"number": n} for n in prs]
    lines: list[str] = []
    sw.out = lines.append
    sw.lines = lines

    def facts(pr):
        if fail and pr["number"] in fail:
            raise fail[pr["number"]]
        return (f"facts{pr['number']}", {})
    sw.pr_facts.side_effect = facts
    return sw


NOW = dt.datetime(2026, 11, 9, 18, 0, tzinfo=dt.timezone.utc)


class Sweep(unittest.TestCase):
    def run_sweep(self, sw, **kw):
        with tempfile.TemporaryDirectory() as d, mock.patch("ledger.sweep") as led, \
                mock.patch("reconciler.sweep") as rec:
            summary = os.path.join(d, "summary.md")
            code = switch.run_sweep(sw, args(**kw), NOW, "", summary)
            text = open(summary, encoding="utf-8").read() if os.path.exists(summary) else ""
        return code, text, led, rec

    def test_under_the_floor_only_the_merge_path_runs_and_the_run_passes(self):
        sw = fake_switch(switch.rate_floor(1000) - 1)
        code, text, led, rec = self.run_sweep(sw)
        self.assertEqual(code, 0)
        self.assertEqual(sw.steer_pr.call_count, 2)   # the merge path, for every PR
        self.assertTrue(sw.merge_only)                # steer_pr asks no reviews
        for skipped in (sw.auto_open_prs, sw.inbox, sw.muse, sw.pipe_health, sw.sweep_owner_labels,
                        sw.close_superseded, sw.watch_sessions, sw.ask_pr):
            skipped.assert_not_called()
        led.assert_not_called()
        rec.assert_not_called()   # (d) the reconciler's own reads wait too
        self.assertIn("rate floor", text)

    def test_above_the_floor_everything_runs(self):
        sw = fake_switch(switch.rate_floor(1000) + 1)
        code, text, led, rec = self.run_sweep(sw)
        self.assertEqual(code, 0)
        self.assertFalse(sw.merge_only)
        sw.inbox.assert_called_once()
        led.assert_called_once()
        rec.assert_called_once()
        self.assertNotIn("rate floor", text)

    def test_the_ask_part_waits_under_the_floor(self):
        sw = fake_switch(10)
        code, text, _, _ = self.run_sweep(sw, part="ask")
        self.assertEqual(code, 0)
        sw.ask_pr.assert_not_called()
        sw.watch_sessions.assert_not_called()
        self.assertIn("rate floor", text)

    def test_a_rate_limited_pr_is_left_for_the_next_sweep_not_an_error(self):
        err = ghapi.RateLimited("GET", "repos/x/pulls/2", "API rate limit exceeded (HTTP 403)", 403)
        sw = fake_switch(4000, fail={2: err})
        code, text, _, _ = self.run_sweep(sw)
        self.assertEqual(code, 0)
        self.assertIn("left for the next sweep: #2", text)

    def test_a_rate_limited_part_is_not_a_failed_run(self):
        sw = fake_switch(4000)
        sw.inbox.side_effect = ghapi.RateLimited("GET", "x", "API rate limit exceeded (HTTP 403)", 403)
        self.assertEqual(self.run_sweep(sw)[0], 0)
        sw.muse.assert_called_once()   # the next part still runs

    def test_one_pr_error_is_its_own_line_and_the_sweep_passes(self):
        # run 42663592988: one failed write to #357 failed the whole run, so the watchdog read it as down
        sw = fake_switch(4000, prs=(355, 357), fail={357: ghapi.GitHubError("PUT", "pulls/357/merge",
                                                                              "unexpected end of JSON input")})
        code, text, led, _ = self.run_sweep(sw)
        self.assertEqual(code, 0)
        self.assertIn("Item errors", text)
        self.assertIn("#357", text)
        self.assertTrue(any(line.startswith("ERROR       #357") for line in sw.lines))
        led.assert_called_once()

    def test_every_pr_failing_is_a_sweep_that_did_not_finish(self):
        boom = ghapi.GitHubError("GET", "x", "HTTP 502")
        sw = fake_switch(4000, prs=(1, 2), fail={1: boom, 2: boom})
        self.assertEqual(self.run_sweep(sw)[0], 2)

    def test_a_part_that_fails_still_fails_the_run(self):
        sw = fake_switch(4000)
        sw.inbox.side_effect = ghapi.GitHubError("GET", "x", "HTTP 502")
        self.assertEqual(self.run_sweep(sw)[0], 2)

    def test_the_floor_is_a_share_of_the_limit_read_at_run_time(self):
        # GITHUB_TOKEN has 1,000 an hour per repository; an App installation 5,000 or more
        self.assertEqual(switch.rate_floor(1000), 160)
        self.assertEqual(switch.rate_floor(5000), 800)
        self.assertEqual(switch.rate_floor(None), switch.RATE_FLOOR_MIN)
        sw = fake_switch(500, limit=5000)   # 500 left is low for an App, plenty for GITHUB_TOKEN
        self.run_sweep(sw)
        self.assertTrue(sw.merge_only)
        sw = fake_switch(500, limit=1000)
        self.run_sweep(sw)
        self.assertFalse(sw.merge_only)


class OtherTokens(unittest.TestCase):
    def test_the_agent_tokens_rate_is_logged_once_and_never_the_runs(self):
        mine = answer(200, json.dumps({"resources": {"core": {"limit": 1000, "remaining": 900, "reset": 1}}}),
                      X_RateLimit_Remaining="900", X_RateLimit_Limit="1000")
        agent = answer(200, json.dumps({"resources": {"core": {"limit": 5000, "remaining": 4999, "reset": 1}}}),
                       X_RateLimit_Remaining="4999", X_RateLimit_Limit="5000")
        fake = Gh((mine, 0, ""), (agent, 0, ""))
        sw = mock.MagicMock()
        sw.gh = client(fake)
        lines: list[str] = []
        sw.out = lines.append
        with mock.patch.dict(os.environ, {switch.AGENT_TOKEN_ENV: "github_pat_SECRET"}):
            switch.rate_probe(sw)
        self.assertEqual((sw.gh.remaining, sw.gh.limit), (900, 1000))   # the run's own numbers stand
        self.assertTrue(any("AGENT_GITHUB_TOKEN: 4999 of 5000" in line for line in lines), lines)
        self.assertFalse(any("SECRET" in line for line in lines + [" ".join(a) for a in fake.seen]))
        self.assertEqual(sw.gh.etags, {})   # rate_limit has no ETag, and another token's answer is never kept

    def test_a_merge_token_write_does_not_change_the_runs_rate(self):
        fake = Gh((answer(200, '{"merged": true}', X_RateLimit_Remaining="12"), 0, ""))
        gh = client(fake)
        gh.remaining = 700
        gh.write("PUT", "repos/IssisX/ScraperX/pulls/9/merge", {}, "merge", token="github_pat_X")
        self.assertEqual(gh.remaining, 700)


if __name__ == "__main__":
    unittest.main()
