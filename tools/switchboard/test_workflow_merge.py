"""Unit tests for merging workflow-file PRs with the merge token (J-056).

Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
No network, no clock: GitHub, the rule check, the token and the time are given.
"""

from __future__ import annotations

import contextlib
import dataclasses
import datetime as dt
import os
import pathlib
import subprocess
import sys
import types
import unittest
from unittest import mock

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import claude_calls  # noqa: E402
import ghapi  # noqa: E402
import rules  # noqa: E402
import switch  # noqa: E402

AGENTS = {
    "agents": {
        "claude": {"vendor": "anthropic", "names": ["Claude"], "branch_prefixes": ["claude/"]},
        "chatgpt": {"vendor": "openai", "names": ["ChatGPT"], "branch_prefixes": ["chatgpt/"]},
        "codex": {"vendor": "openai", "names": ["Codex"], "branch_prefixes": ["codex/"]},
        "grok": {"vendor": "xai", "names": ["Grok"], "branch_prefixes": ["grok/"]},
        "muse": {"vendor": "meta", "names": ["Muse"], "branch_prefixes": ["muse/", "fb-bot/"]},
    },
    "review": {"claude_authored_asks": ["chatgpt"], "pool": ["claude-api"], "api_reviews_claude": True,
               "fallback_after_hours": 1, "fallback_order": ["grok", "chatgpt"]},
    "verdict_logins": ["IssisX", "github-actions[bot]"],
}
TEAM = rules.team_from_config(AGENTS)
HEAD = "1a2b3c4d5e6f708192a3b4c5d6e7f8091a2b3c4d"
OTHER_HEAD = "ffffffff00000000ffffffff00000000ffffffff"
NOW = dt.datetime(2026, 11, 6, 23, 0, tzinfo=dt.timezone.utc)
T0, T1, T2 = "2026-11-06T20:00:00Z", "2026-11-06T21:00:00Z", "2026-11-06T22:00:00Z"
BOT = rules.BOT_LOGIN
TOKEN = "github_pat_11TEST0000000000secret"
KEY = "review-key-TEST-0123456789abcdef"   # SWITCHBOARD_REVIEW_KEY in these tests
REPRO = "\ncommand: ./run x\ninput: seed 7\nexpected: PASS\nobserved: FAIL at 3.2 m\n"
RC_CLEAN = (0, "rule_check: 0 BLOCK, 0 REVIEW\n")
NEEDS = "Contents, Pull requests and Workflows: read/write"


def v(body: str, at: str = T1, login: str = "IssisX") -> rules.Verdict:
    got = rules.parse_verdict(body, at, login, TEAM)
    assert got is not None, body
    return got


def codex(sha: str = HEAD, unresolved: bool = False, at: str = T1) -> rules.Verdict:
    """The Codex app's verdict as switch.verdicts() reads it: its state comment, which names the whole commit sha."""
    body = ("<!-- codex-review-state:v1 &#123;\"code_review\":&#123;\"commit_sha\":\"" + sha +
            f"\",\"has_unresolved_findings\":{str(unresolved).lower()},\"status\":\"completed\"&#125;&#125; -->")
    got = rules.parse_codex_state(body, at, rules.CODEX_LOGIN)
    assert got is not None, body
    return got


def api(verdict: str = "LAND", sha: str = HEAD, extra: str = "", at: str = T1, width: int = 40,
        signed: bool = True) -> rules.Verdict:
    """The automatic Claude review as switch.verdicts() reads one switch.review() posted, by the workflow bot: the
    whole head sha (width=8 is the mark it posted before J-056's security fixes), and signed with the review key
    (TheSignedReview reads real bodies, signed and forged)."""
    got = v(f"**Claude (automatic review of #9 @ {sha[:width]}): {verdict}**\n\nsummary{extra}", at, BOT)
    return dataclasses.replace(got, signed=signed)


def facts(**kw) -> rules.PrFacts:
    base = dict(number=9, head_sha=HEAD, head_ref="muse/integrate-j056", base_ref=rules.TRUNK, draft=False,
                mergeable=True, head_committed_at=T0, proof="green", verdicts=(codex(), api()),
                needs=frozenset(), rule_check_block=False, touches_workflows=True)
    base.update(kw)
    return rules.PrFacts(**base)


def decide(f: rules.PrFacts, token: bool = True, key: bool = True) -> rules.MergeDecision:
    return rules.merge_decision(f, TEAM.vendor_of_branch(f.head_ref), TEAM, merge_token=token, review_key=key)


def ctx(comments=()):
    # fresh: these facts stand for GitHub's own uncached answers, so steer_pr merges on them without the re-read
    # (J-072; test_cache_trust.FreshMerge tests the re-read itself)
    return {"job": "J-056", "issue": None, "proof_url": "https://x/runs/7", "run_id": 7, "proof_done_at": T1,
            "comments": list(comments), "title": "t", "body": "", "files": [], "updated_at": T2, "fresh": True}


class RecordingGH:
    """The GitHub calls merge() and steer_pr make: every write recorded with the token it was sent with."""

    repo, dry_run = "IssisX/ScraperX", False

    def __init__(self, fail: str = ""):
        self.fail, self.writes, self.posted = fail, [], []

    @contextlib.contextmanager
    def fresh(self):   # ghapi.GitHub.fresh: this stub has no cache, so every read is already GitHub's own
        yield self

    def write(self, method, path, body=None, what="", token=""):
        self.writes.append((method, path, token, what))
        if method == "PUT" and self.fail:
            raise switch.GitHubError(method, path, self.fail)
        return {"merged": True} if method == "PUT" else None

    def comment(self, number, body, what=""):
        self.posted.append((number, body))

    def get(self, path, accept=""):
        raise AssertionError(f"unexpected read {path}")

    def paged(self, path, key=""):
        return []


def make(gh, token: str = TOKEN, key: str = KEY):
    sw = switch.Switch.__new__(switch.Switch)
    sw.gh, sw.now, sw.team, sw.lines, sw._load = gh, NOW, TEAM, [], None
    sw.out = sw.lines.append
    sw.run_rule_check = lambda number, head: RC_CLEAN
    sw.merge_token, sw.review_key = token, key
    return sw


class TheRule(unittest.TestCase):
    def test_no_token_is_human_merge_exactly_as_before(self):
        today = ("HUMAN-MERGE", ("ready, but it changes .github/workflows; the workflow token cannot merge it",))
        for f, by in ((facts(), "Claude"), (facts(verdicts=(codex(),)), "Codex")):
            d = decide(f, token=False)
            self.assertEqual((d.action, d.reasons, d.land_by), today + (by,))
            self.assertEqual(rules.merge_decision(f, "meta", TEAM), d)   # the default is no token

    def test_every_other_gate_still_holds_with_the_token(self):
        for f, action in ((facts(proof="running"), "WAIT"), (facts(proof="red: native-tests"), "FIX-CI"),
                          (facts(verdicts=(api(sha=OTHER_HEAD),)), "WAIT"), (facts(mergeable=False), "WAIT"),
                          (facts(rule_check_block=True), "WAIT"), (facts(rule_check_block=None), "WAIT"),
                          (facts(never_edit=True, rule_check_block=True, head_ref="muse/integrate-protected-x"), "WAIT"),
                          (facts(draft=True), "SKIP"), (facts(needs=frozenset({"eyes"})), "WAIT")):
            self.assertEqual(decide(f).action, action, f)

class LandsLikeAnyPR(unittest.TestCase):
    """SYSTEM_PLAN 4.1 (11-13): a PR that changes .github/workflows lands like any other PR. Green proof, a LAND
    from another vendor, no open FIX and a clean rule check merge it with the merge token; no security pass and no
    Codex-only LAND. MERGE-BY-HAND stays only when no merge token is set."""

    def steer(self, f, token: str = TOKEN, key: str = KEY):
        gh = RecordingGH()
        sw = make(gh, token=token, key=key)
        seen: list = []
        sw.steer_pr(f, ctx(), seen)
        (s,) = seen
        return sw, gh, s

    def test_a_codex_land_and_the_token_merge_it_with_the_token(self):
        f = facts(head_ref="claude/integrate-j072", verdicts=(codex(),))   # no automatic Claude review at all
        d = decide(f)
        self.assertEqual((d.action, d.land_by), ("MERGE", "Codex"))
        self.assertIn("with the merge token", d.reasons[0])
        self.assertEqual(decide(f, key=False), d)   # the review key no longer holds a merge
        sw, gh, s = self.steer(f, key="")
        self.assertEqual((s.decision.action, s.merge_note), ("MERGE", ""))
        self.assertEqual([(m, p.rsplit("/", 2)[-2:], t) for m, p, t, _ in gh.writes], [("PUT", ["9", "merge"], TOKEN)])
        ((_, note),) = gh.posted
        self.assertIn("with the merge token", note)   # the merged note says how it landed
        self.assertNotIn(TOKEN, note + "\n".join(sw.lines))

    def test_a_typed_chatgpt_land_merges(self):
        typed = v(f"**ChatGPT (review of #9 @ {HEAD}): LAND**")   # posted by the team login, typed by name
        for ref in ("muse/integrate-j072", "claude/integrate-j072"):
            d = decide(facts(head_ref=ref, verdicts=(typed,)))
            self.assertEqual((d.action, d.land_by), ("MERGE", "ChatGPT"), ref)
        # the author's own vendor still never lands it: ChatGPT on Codex's work is the same vendor
        self.assertEqual(decide(facts(head_ref="codex/integrate-j072", verdicts=(typed,))).action, "WAIT")

    def test_no_token_is_merge_by_hand(self):
        f = facts(head_ref="claude/integrate-j072", verdicts=(codex(),))
        sw, gh, s = self.steer(f, token="")
        self.assertEqual(s.decision.action, "HUMAN-MERGE")
        self.assertIn("the workflow token cannot merge it", s.decision.reasons[0])
        self.assertEqual(gh.writes, [])
        (item,) = sw.pr_inbox_items(s, [], frozenset())
        self.assertEqual(item.kind, "MERGE-BY-HAND")

    def test_an_open_fix_is_fix(self):
        typed_fix = v(f"**ChatGPT (review of #9 @ {HEAD}): FIX**{REPRO}")
        for verdicts in ((codex(unresolved=True),),                       # the Codex app's open findings
                         (codex(), api("FIX", extra=REPRO)),               # the automatic review's FIX with a repro
                         (codex(), typed_fix)):                            # any vendor's FIX with a repro
            f = facts(head_ref="claude/integrate-j072", verdicts=verdicts)
            self.assertEqual(decide(f).action, "FIX", verdicts)
            sw, gh, s = self.steer(f)
            self.assertEqual((s.decision.action, gh.writes), ("FIX", []), verdicts)

    def test_ask_pr_on_it_makes_no_paid_call(self):
        def ask(f, comments=()):
            sw = make(RecordingGH())
            sw.api_ok = lambda c: True
            sw.review_load = lambda: ({}, {})
            sw.stale = lambda f: False
            sw.codex_pending = lambda f, c: False
            sw.paid = lambda *a, **kw: self.fail("a paid call")
            calls = []
            sw.review = lambda facts, c, author, security=False: calls.append((facts.number, security))
            sw.ask_pr(f, ctx(comments))
            self.assertFalse(any(l.startswith("SECURITY") for l in sw.lines), sw.lines)
            return calls
        # Codex already landed the head: no automatic review, no security pass
        for ref in ("claude/integrate-j072", "muse/integrate-j072"):
            self.assertEqual(ask(facts(head_ref=ref, verdicts=(codex(),))), [], ref)
        self.assertFalse(hasattr(rules, "needs_security_pass"))
        # The API's own turn on an unreviewed workflow head is the ordinary review, never the security pass
        self.assertEqual(ask(facts(verdicts=())), [(9, False)])
        # and a plain review mark on the head, unsigned, stops a second one
        mark = said("review\n" + switch.MARK.format(kind="review", pr=9, head=HEAD))
        self.assertEqual(ask(facts(verdicts=()), [mark]), [])


class TheMerge(unittest.TestCase):
    def test_a_workflow_pr_merges_with_the_token_and_dispatches_nothing(self):
        gh = RecordingGH()
        sw = make(gh)
        f = facts()
        self.assertEqual(sw.merge(f, ctx(), decide(f)), "")
        self.assertEqual([(m, p.rsplit("/", 2)[-2:], t) for m, p, t, _ in gh.writes],
                         [("PUT", ["9", "merge"], TOKEN)])
        self.assertTrue(gh.posted and all("starts the trunk proof" in b and TOKEN not in b for _, b in gh.posted))

    def test_a_pr_without_workflow_files_keeps_the_workflow_token_and_the_dispatch(self):
        gh = RecordingGH()
        sw = make(gh)
        f = facts(touches_workflows=False)
        self.assertEqual(sw.merge(f, ctx(), decide(f)), "")
        self.assertEqual([(m, t) for m, _, t, _ in gh.writes], [("PUT", ""), ("POST", "")])
        self.assertTrue(gh.writes[1][1].endswith(f"/actions/workflows/{switch.PROOF_WORKFLOW}/dispatches"))
        self.assertIn("The trunk proof is dispatched.", gh.posted[0][1])

    def test_a_refused_token_is_listed_to_merge_by_hand_with_the_status(self):
        for code in (401, 403, 404, 405):
            gh = RecordingGH(fail=f"gh: Resource not accessible by personal access token {TOKEN} (HTTP {code})")
            sw = make(gh)
            seen: list = []
            sw.steer_pr(facts(), ctx(), seen)
            (s,) = seen
            self.assertEqual((s.decision.action, s.merge_note), ("HUMAN-MERGE", ""), code)
            self.assertIn(f"HTTP {code}", s.decision.reasons[0])
            self.assertIn(NEEDS, s.decision.reasons[0])
            (item,) = sw.pr_inbox_items(s, [], frozenset())
            self.assertEqual(item.kind, "MERGE-BY-HAND")
            self.assertIn(f"HTTP {code}", item.line)
            self.assertIn(NEEDS, item.line)
            said = "\n".join(sw.lines + [item.line] + [b for _, b in gh.posted])
            self.assertNotIn(TOKEN, said)
            self.assertTrue(any(l.startswith("HUMAN-MERGE #9") for l in sw.lines))
            self.assertEqual([m for m, *_ in gh.writes], ["PUT"])   # nothing dispatched, nothing merged

    def test_other_merge_errors_still_raise(self):
        sw = make(RecordingGH(fail="gh: Head branch was modified (HTTP 409)"))
        with self.assertRaises(switch.GitHubError):
            sw.merge(facts(), ctx(), decide(facts()))

    def test_no_token_never_tries_a_workflow_merge(self):
        gh = RecordingGH()
        self.assertIn("no merge token", make(gh, token="").merge(facts(), ctx(), decide(facts())))
        self.assertEqual(gh.writes, [])


class TheToken(unittest.TestCase):
    def test_gh_gets_the_token_in_its_environment_only(self):
        seen = []

        def run(args, **kw):
            seen.append((args, kw.get("env")))
            return subprocess.CompletedProcess(args, 0, b'{"merged": true}', b"")
        gh = ghapi.GitHub("IssisX/ScraperX", False, log=lambda line: None)
        with mock.patch.object(ghapi.subprocess, "run", run):
            gh.write("PUT", "repos/IssisX/ScraperX/pulls/9/merge", {"sha": HEAD}, "merge", token=TOKEN)
            gh.write("PUT", "repos/IssisX/ScraperX/pulls/9/merge", {"sha": HEAD}, "merge")
        (args, env), (_, plain) = seen
        self.assertEqual(env["GH_TOKEN"], TOKEN)
        self.assertNotIn(TOKEN, " ".join(args))
        self.assertIsNone(plain)   # every other call: whatever gh holds (the workflow token)

    def test_an_error_never_carries_the_token(self):
        def run(args, **kw):
            return subprocess.CompletedProcess(args, 1, b"", f"gh: Bad credentials {TOKEN} (HTTP 401)".encode())
        gh = ghapi.GitHub("IssisX/ScraperX", False, log=lambda line: None)
        with mock.patch.object(ghapi.subprocess, "run", run), self.assertRaises(ghapi.GitHubError) as caught:
            gh.write("PUT", "repos/IssisX/ScraperX/pulls/9/merge", {}, "merge", token=TOKEN)
        self.assertNotIn(TOKEN, str(caught.exception) + caught.exception.detail)
        self.assertEqual(switch.http_status(caught.exception.detail), 401)

    def test_main_takes_the_token_out_of_the_environment(self):
        made = []

        class Stub(mock.MagicMock):
            def __init__(self, *a, **kw):
                super().__init__()
                made.append(self)
        with mock.patch.dict(os.environ, {switch.MERGE_TOKEN_ENV: f" {TOKEN}\n"}), \
                mock.patch.object(switch, "Switch", Stub), \
                mock.patch.object(sys, "argv", ["switch.py", "--part", "steer", "--pr", "9"]):
            self.assertEqual(switch.main(), 0)
            self.assertNotIn(switch.MERGE_TOKEN_ENV, os.environ)
        self.assertEqual(made[0].merge_token, TOKEN)

    def test_main_takes_the_review_key_out_of_the_environment(self):
        made = []

        class Stub(mock.MagicMock):
            def __init__(self, *a, **kw):
                super().__init__()
                made.append(self)
        with mock.patch.dict(os.environ, {switch.REVIEW_KEY_ENV: f" {KEY}\n"}), \
                mock.patch.object(switch, "Switch", Stub), \
                mock.patch.object(sys, "argv", ["switch.py", "--part", "steer", "--pr", "9"]):
            self.assertEqual(switch.main(), 0)
            self.assertNotIn(switch.REVIEW_KEY_ENV, os.environ)
        self.assertEqual(made[0].review_key, KEY)

    def test_the_review_key_reaches_only_the_ask_and_steer_switch_steps_in_the_environment(self):
        try:
            import yaml
        except ImportError:
            self.skipTest("PyYAML not installed")
        text = (HERE.parents[1] / ".github/workflows/switchboard.yml").read_text(encoding="utf-8")
        flow = yaml.safe_load(text)
        self.assertNotIn(switch.REVIEW_KEY_ENV, str(flow.get("env")))
        holders = []
        for name, job in flow["jobs"].items():
            self.assertNotIn(switch.REVIEW_KEY_ENV, str(job.get("env")), name)
            holders += [(name, step["name"]) for step in job["steps"] if switch.REVIEW_KEY_ENV in str(step.get("env"))]
        self.assertEqual(sorted(holders), [("ask", "Run the switch"), ("steer", "Run the switch")])
        self.assertEqual(text.count("secrets." + switch.REVIEW_KEY_ENV), 2)
        # The jobs that hold a secret deploy to the environment whose branch limit is trunk; no other job does.
        self.assertEqual(sorted(n for n, j in flow["jobs"].items() if j.get("environment")), ["ask", "provision", "steer"])
        for name in ("ask", "steer", "provision"):
            self.assertEqual(flow["jobs"][name]["environment"], "switchboard-merge", name)

    def test_the_api_key_and_agent_token_reach_only_steps_in_the_environment(self):
        # J-057: they are environment secrets, so a step outside switchboard-merge would get them empty; pin the
        # holders so a new job cannot ask for them without joining the environment (branch limit: trunk).
        try:
            import yaml
        except ImportError:
            self.skipTest("PyYAML not installed")
        text = (HERE.parents[1] / ".github/workflows/switchboard.yml").read_text(encoding="utf-8")
        flow = yaml.safe_load(text)
        for secret in ("ANTHROPIC_API_KEY", "AGENT_GITHUB_TOKEN"):
            self.assertNotIn("secrets." + secret, str(flow.get("env")), secret)
            holders = []
            for name, job in flow["jobs"].items():
                self.assertNotIn("secrets." + secret, str(job.get("env")), (secret, name))
                holders += [(name, step["name"]) for step in job["steps"] if "secrets." + secret in str(step.get("env"))]
            self.assertEqual(sorted(holders), [("ask", "Run the switch"), ("provision", "Create the environment, agent and vault")], secret)
            self.assertEqual(text.count("secrets." + secret), 2, secret)
            for name, _ in holders:
                self.assertEqual(flow["jobs"][name].get("environment"), "switchboard-merge", (secret, name))

    def test_only_the_steer_jobs_switch_step_holds_the_secret(self):
        try:
            import yaml
        except ImportError:
            self.skipTest("PyYAML not installed")
        path = HERE.parents[1] / ".github/workflows/switchboard.yml"
        text = path.read_text(encoding="utf-8")
        flow = yaml.safe_load(text)
        self.assertNotIn(switch.MERGE_TOKEN_ENV, str(flow.get("env")))
        holders = []
        for name, job in flow["jobs"].items():
            self.assertNotIn(switch.MERGE_TOKEN_ENV, str(job.get("env")), name)
            for step in job["steps"]:
                if switch.MERGE_TOKEN_ENV in str(step.get("env")):
                    holders.append((name, step["name"], step.get("uses"), step["run"]))
        self.assertEqual(text.count("secrets." + switch.MERGE_TOKEN_ENV), 1)
        (job, step, uses, run), = holders
        self.assertEqual((job, step, uses), ("steer", "Run the switch", None))
        self.assertNotIn("checkout", run)
        self.assertNotIn("git ", run)
        self.assertIn("src/tools/switchboard/switch.py", run)
        clone = flow["jobs"]["steer"]["steps"][0]["run"]
        self.assertIn('--branch "$TRUNK" --single-branch', clone)   # src is trunk, never the PR's head


class TheCacheBoundary(unittest.TestCase):
    """J-072, the security pass on #360 (P1): actions/cache/restore unpacks a whole archive that any job on trunk can
    save, so it may never run in a job that holds a secret. cache-in restores it with no secret, no token scope and no
    code of its own, and hands on the one file as this run's artifact; ask and steer download only that artifact,
    into $RUNNER_TEMP, and hand the new file back the same way; cache-out, with no secret either, saves it."""

    RUNNER_TEMP = "${{ runner.temp }}/etags"

    def flow(self) -> dict:
        try:
            import yaml
        except ImportError:
            self.skipTest("PyYAML not installed")
        return yaml.safe_load((HERE.parents[1] / ".github/workflows/switchboard.yml").read_text(encoding="utf-8"))

    @staticmethod
    def holds_secret(job: dict) -> bool:
        return bool(job.get("environment")) or "secrets." in str(job.get("env")) or \
            any("secrets." in str(s.get("env")) or "secrets." in str(s.get("with")) for s in job["steps"])

    def test_no_job_that_holds_a_secret_touches_the_actions_cache(self):
        flow = self.flow()
        self.assertNotIn("secrets.", str(flow.get("env")))
        secret_jobs = [n for n, j in flow["jobs"].items() if self.holds_secret(j)]
        self.assertEqual(sorted(secret_jobs), ["ask", "provision", "steer"])
        for name in secret_jobs:
            for step in flow["jobs"][name]["steps"]:
                uses = step.get("uses") or ""
                self.assertFalse(uses.startswith("actions/cache"), (name, step["name"]))
                self.assertNotIn("actions/caches", str(step.get("run")), (name, step["name"]))   # nor the cache API
                # the only actions in a secret job: this run's one named artifact in, the new file out
                self.assertIn(uses.split("@")[0], ("", "actions/download-artifact", "actions/upload-artifact"),
                              (name, step["name"]))

    def test_the_secret_jobs_download_only_the_named_artifact_into_runner_temp(self):
        flow = self.flow()
        for name in ("ask", "steer"):
            job = flow["jobs"][name]
            downloads = [s for s in job["steps"] if str(s.get("uses")).startswith("actions/download-artifact@")]
            (down,) = downloads
            self.assertEqual(down["with"], {"name": f"switchboard-etags-{name}", "path": self.RUNNER_TEMP}, name)
            self.assertNotIn("run-id", str(down["with"]))   # this run's artifact, never another run's
            self.assertIn("cache-in", job["needs"])
            uploads = [s for s in job["steps"] if str(s.get("uses")).startswith("actions/upload-artifact@")]
            (up,) = uploads
            self.assertEqual((up["with"]["name"], up["with"]["path"]),
                             (f"switchboard-etags-{name}-out", self.RUNNER_TEMP + "/etags.json"), name)
            self.assertIn("inputs.dry_run != true", up["if"])   # a dry run hands nothing on
            (switch_step,) = [s for s in job["steps"] if s["name"] == "Run the switch"]
            self.assertEqual(switch_step["env"][switch.ETAG_CACHE_ENV], self.RUNNER_TEMP + "/etags.json", name)
            # the download comes after every step that sets up code (the clone, the venv) and before the switch
            names = [s["name"] for s in job["steps"]]
            self.assertLess(names.index("Install the hash-pinned dependencies (wheels only)"), names.index(down["name"]))
            self.assertLess(names.index(down["name"]), names.index("Run the switch"))
        self.assertNotIn(switch.ETAG_CACHE_ENV, str(flow.get("env")))   # never a path in the workspace

    def test_the_cache_jobs_hold_no_secret_and_run_no_code_of_ours(self):
        flow = self.flow()
        jobs = {n: j for n, j in flow["jobs"].items()
                if any(str(s.get("uses")).startswith("actions/cache") for s in j["steps"])}
        self.assertEqual(sorted(jobs), ["cache-in", "cache-out"])
        self.assertEqual(jobs["cache-in"]["permissions"], {})
        self.assertEqual(jobs["cache-out"]["permissions"], {"actions": "write"})
        for name, job in jobs.items():
            self.assertFalse(self.holds_secret(job), name)
            self.assertNotIn("environment", job, name)
            for step in job["steps"]:
                self.assertIn(str(step.get("uses", "")).split("@")[0],
                              ("", "actions/cache/restore", "actions/cache/save", "actions/upload-artifact",
                               "actions/download-artifact"), (name, step.get("name")))
                run = str(step.get("run", ""))
                for code in ("python", ".venv", "src/", "git ", "node ", "bash ", "./"):
                    self.assertNotIn(code, run, (name, step.get("name")))
        self.assertFalse(any("run" in s for s in jobs["cache-in"]["steps"]))
        # cache-in hands on exactly the two files, nothing else from the archive
        ups = [s["with"] for s in jobs["cache-in"]["steps"] if str(s.get("uses")).startswith("actions/upload-artifact@")]
        self.assertEqual([(u["name"], u["path"]) for u in ups],
                         [("switchboard-etags-ask", "ask/etags.json"), ("switchboard-etags-steer", "steer/etags.json")])

    def test_one_cache_scope_so_two_states_never_both_fire_a_timer(self):
        # Caches are scoped by ref. The default branch is trunk, so every event but a pull_request runs there; a run
        # on another ref (a dispatch) skips the cache, the provision run (its own concurrency group) skips it, and
        # the sweeps share one concurrency group that never runs two at once.
        flow = self.flow()
        cin, cout = flow["jobs"]["cache-in"], flow["jobs"]["cache-out"]
        self.assertTrue(cin["if"].startswith("github.ref == 'refs/heads/ScraperX-Claude' && "
                                             "github.event_name != 'pull_request' && !inputs.provision && "))
        self.assertIn("needs.cache-in.result == 'success'", cout["if"])
        self.assertIn("inputs.dry_run != true", cout["if"])
        self.assertFalse(flow["concurrency"]["cancel-in-progress"])
        self.assertIn("|| 'switchboard'))", flow["concurrency"]["group"])
        keys = [s["with"]["key"] for j in (cin, cout) for s in j["steps"] if "key" in s.get("with", {})]
        self.assertEqual(sorted(set(k.split("${{")[0] for k in keys)), ["switchboard-etags-ask-", "switchboard-etags-steer-"])


class TheReviewsRunApart(unittest.TestCase):
    """11-13 (SYSTEM_PLAN 6, from #503): the sweep's run held the shared concurrency group until ask's serial reviews
    ended (744 s for one security review), so merges, auto-open and the inbox waited and queued sweeps were replaced.
    The reviews now run in their own run (part=ask) and group, which steer starts (switch.py wake_reviews; no
    wake-ask job); each run restores and saves only its own part's file."""

    flow = TheCacheBoundary.flow

    def test_an_ask_run_has_its_own_group_and_name_and_a_sweep_never_runs_ask(self):
        flow = self.flow()
        group = flow["concurrency"]["group"]
        self.assertIn("inputs.part == 'ask' && 'switchboard-ask' ||", group)
        self.assertLess(group.index("'switchboard-ask'"), group.index("'switchboard-provision'"))
        name = flow["run-name"]
        self.assertIn("inputs.part == 'ask' && (inputs.dry_run &&", name)
        self.assertLess(name.index("'Switchboard reviews'"), name.index("'Switchboard sweep'"))   # never a sweep
        self.assertIn(f"'{switch.REVIEWS_RUN}'", name)   # the title wake_reviews looks for
        jobs = flow["jobs"]
        self.assertIn("inputs.part == 'ask'", jobs["ask"]["if"])
        self.assertIn("github.event_name == 'workflow_dispatch'", jobs["ask"]["if"])
        self.assertEqual(jobs["steer"]["needs"], "cache-in")   # a merge never waits behind a review
        self.assertIn("inputs.part != 'ask'", jobs["steer"]["if"])
        self.assertIn("needs.cache-in.result != 'cancelled'", jobs["steer"]["if"])
        self.assertNotIn("needs.ask", str(flow))
        self.assertEqual(flow[True]["workflow_dispatch"]["inputs"]["part"]["type"], "string")
        # a needs:orchestrator label starts no job now, so its run is idle and never replaces a pending sweep
        self.assertNotIn("needs:orchestrator", jobs["cache-in"]["if"])
        self.assertNotIn("needs:orchestrator", group)

    def test_each_run_restores_saves_and_prunes_only_its_own_part(self):
        jobs = self.flow()["jobs"]
        for step in jobs["cache-in"]["steps"]:
            part = "ask" if "ask part" in step["name"] else "steer"
            self.assertEqual(step["if"], "inputs.part == 'ask'" if part == "ask" else "inputs.part != 'ask'", step["name"])
        (prune,) = [s for s in jobs["cache-out"]["steps"] if "run" in s]
        self.assertEqual(prune["env"]["PART"], "${{ inputs.part == 'ask' && 'ask' || 'steer' }}")
        self.assertIn('for part in "$PART"; do', prune["run"])
        self.assertEqual(jobs["cache-out"]["needs"], ["cache-in", "ask", "steer"])

    def test_cache_out_fetches_only_its_own_parts_file(self):
        # 11-13 (#350, sweep 43511901124): a sweep's cache-out fetched the ask part's file too, which no ask job made
        # ("Artifact not found for name: switchboard-etags-ask-out")
        for step in self.flow()["jobs"]["cache-out"]["steps"]:
            if str(step.get("uses")).startswith("actions/download-artifact@"):
                part = step["with"]["name"].split("-")[2]
                self.assertEqual(step["if"], "inputs.part == 'ask'" if part == "ask" else "inputs.part != 'ask'",
                                 step["name"])
                self.assertTrue(step["continue-on-error"], step["name"])   # its part may have handed on nothing

    def prune(self, listing_fails: bool) -> tuple[int, list[str], str]:
        """cache-out's prune step, run with a fake gh: a listing answers ids, or GitHub's rate-limit refusal as gh gives
        it (the error's body on stdout, exit 1)."""
        import shutil
        import tempfile
        (step,) = [s for s in self.flow()["jobs"]["cache-out"]["steps"] if "run" in s]
        self.assertNotIn("${{", step["run"])
        refused = '{\n  "message": "API rate limit exceeded for installation.",\n  "status": "403"\n}'
        with tempfile.TemporaryDirectory() as tmp:
            log = pathlib.Path(tmp) / "log"
            fake = pathlib.Path(tmp) / "gh"
            fake.write_text(
                f'#!/bin/bash\necho "gh $*" >> "{log}"\n'
                'if [ "$2" = "-X" ]; then exit 0; fi\n'
                + (f"echo '{refused}'\necho 'gh: API rate limit exceeded' >&2\nexit 1\n" if listing_fails else
                   'case "$2" in *caches*) printf "11\\n12\\n" ;; *) printf "21\\n" ;; esac\n'), encoding="utf-8")
            fake.chmod(0o755)
            env = {**os.environ, **step["env"], "PATH": f"{tmp}:{os.environ['PATH']}", "REPO": "o/r", "RUN_ID": "7",
                   "PART": "steer"}
            got = subprocess.run([shutil.which("bash") or "bash", "-e", "-c", step["run"]], cwd=tmp, env=env,
                                 capture_output=True, text=True)
            return got.returncode, log.read_text(encoding="utf-8").splitlines(), got.stdout

    def test_a_prune_github_refuses_fails_nothing_and_deletes_nothing(self):
        code, calls, out = self.prune(listing_fails=True)
        self.assertEqual(code, 0, "the state was saved before the prune: a refused listing is not a failed run")
        self.assertEqual([c for c in calls if " -X DELETE " in c], [], "the error's body is never read as ids")
        self.assertEqual(len(calls), 2)
        self.assertIn("::warning::", out)
        code, calls, _ = self.prune(listing_fails=False)
        self.assertEqual(code, 0)
        self.assertEqual([c for c in calls if " -X DELETE " in c],
                         ["gh api -X DELETE repos/o/r/actions/caches/11", "gh api -X DELETE repos/o/r/actions/caches/12",
                          "gh api -X DELETE repos/o/r/actions/artifacts/21"])

    def test_steer_starts_the_reviews_run_and_no_job_does_it_after_every_sweep(self):
        flow = self.flow()
        self.assertNotIn("ask", flow["jobs"]["steer"]["needs"])   # steer runs first and starts the reviews itself
        self.assertNotIn("wake-ask", flow["jobs"])   # SYSTEM_PLAN 6: four more jobs every ten minutes, not taken
        self.assertNotIn("gh workflow run", str(flow))
        self.assertEqual(flow["jobs"]["steer"]["permissions"]["actions"], "write")   # wake_reviews' dispatch

    def run_script(self, script: str, env: dict) -> tuple[int, list[str]]:
        import shutil
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            bin_ = pathlib.Path(tmp)
            log = bin_ / "log"
            (bin_ / ".venv/bin").mkdir(parents=True)
            fake = bin_ / ".venv/bin/python"
            fake.write_text(f'#!/bin/bash\necho "python $*" >> "{log}"\n', encoding="utf-8")
            fake.chmod(0o755)
            full = {**os.environ, **env, "PATH": f"{bin_}:{os.environ['PATH']}", "TRUNK": rules.TRUNK, "REPO": "o/r"}
            got = subprocess.run([shutil.which("bash") or "bash", "-c", script], cwd=tmp, env=full,
                                 capture_output=True, text=True)
            calls = log.read_text(encoding="utf-8").splitlines() if log.exists() else []
            return got.returncode, calls

    def test_ask_runs_one_whole_ask_pass_and_keeps_the_dry_flag(self):
        (step,) = [s for s in self.flow()["jobs"]["ask"]["steps"] if s["name"] == "Run the switch"]
        self.assertNotIn("${{", step["run"])   # event values only through env
        for k in ("EVENT", "SCHEDULE", "ISSUE_NUMBER", "LABEL", "DISPATCH_DIGEST"):
            self.assertNotIn(k, step["env"])   # ask reads no event text any more
        for dry, tail in (("false", "--part ask --repo o/r --root src"), ("true", "--root src --dry-run")):
            code, calls = self.run_script(step["run"], {"PART": "ask", "DISPATCH_DRY": dry})
            self.assertEqual(code, 0, dry)
            (call,) = calls
            self.assertTrue(call.startswith("python src/tools/switchboard/switch.py --part ask"), call)
            self.assertTrue(call.endswith(tail), (dry, call))


class TheSecurityPass(unittest.TestCase):
    def ask(self, f):
        sw = make(RecordingGH())
        sw.api_ok = lambda c: True
        sw.review_load = lambda: ({}, {})
        sw.stale = lambda f: False
        sw.codex_pending = lambda f, c: False
        calls = []
        sw.review = lambda facts, c, author, security=False: calls.append((facts.number, security))
        sw.ask_pr(f, ctx())
        return calls, sw.lines

    def test_no_second_review_and_none_off_workflow_files(self):
        self.assertEqual(self.ask(facts(head_ref="claude/integrate-j056"))[0], [])   # the API already reviewed it
        self.assertEqual(self.ask(facts(head_ref="claude/integrate-j056", verdicts=(codex(),),
                                        touches_workflows=False))[0], [])

    def test_a_failed_security_pass_asks_nobody_and_is_not_paid_twice(self):
        gh = RecordingGH()
        sw = make(gh)
        sw.ask_review = lambda *a: self.fail("no one else can stand in for the security pass")
        sw.review_failed(facts(verdicts=(codex(),)), ctx(), "meta", True, "The automatic review failed to run. ")
        ((number, body),) = gh.posted
        self.assertEqual(number, 9)
        self.assertIn(switch.MARK.format(kind="review", pr=9, head=HEAD), body)
        self.assertIsNone(rules.parse_verdict(body, T2, BOT, TEAM))   # a note, never a verdict
        self.assertIsNone(switch.VERDICT_TRIGGER.search(body))        # and it wakes nothing
        self.assertTrue(sw.marked([{"body": body, "user": {"login": BOT}}], "review", 9, HEAD))


COLLIDE = HEAD[:8] + "0" * 32   # a head ground to share its first 8 hex characters with the reviewed HEAD


class DiffGH(RecordingGH):
    """RecordingGH whose text reads answer by path: the PR's diff and a compare can show different heads."""

    def __init__(self, texts: dict):
        super().__init__()
        self.texts, self.reads = texts, []

    def get_text(self, path, accept):
        self.reads.append((path, accept))
        return self.texts.get(path, "")


class TheSecurityFindings(unittest.TestCase):
    """The three holes the security review of #297 found, one test each (J-056)."""

    def test_1_the_automatic_review_and_the_ask_name_the_whole_head(self):
        gh = DiffGH({})
        sw = make(gh)
        sw.root = HERE.parents[1]
        sw.paid = lambda *a: claude_calls.Result({"verdict": "LAND", "summary": "ok", "notes": []}, "end_turn", "m", 1, 1)
        sw.review(facts(), ctx(), "meta", security=True)
        ((_, body),) = gh.posted
        self.assertTrue(body.startswith(f"**Claude (automatic review of #9 @ {HEAD}): LAND**"), body[:120])
        (got,) = read(body)   # as switch.verdicts() reads it: signed (TheSignedReview)
        self.assertEqual((got.sha, rules.api_review_on([got], HEAD)), (HEAD, got))
        self.assertIsNone(rules.api_review_on([got], COLLIDE))

    def test_every_blocking_finding_is_posted_apart_from_the_notes(self):
        # 11-09: the security pass found one hole per round on #360; every blocking finding goes in one review.
        gh = DiffGH({})
        sw = make(gh)
        sw.root = HERE.parents[1]
        repro = {k: k + " x" for k in ("command", "input", "expected", "observed")}
        notes = ["Also blocking: the cache restore runs in a job that holds the merge token.", "A typo in a comment."]
        sw.paid = lambda *a: claude_calls.Result({"verdict": "FIX", "summary": "s", "repro": repro, "notes": notes},
                                                 "end_turn", "m", 1, 1)
        sw.review(facts(), ctx(), "meta", security=True)
        ((_, body),) = gh.posted
        also, rest = body.index("Also blocking (fix with the one above"), body.index("Notes (never blocking):")
        self.assertLess(also, rest)
        self.assertLess(body.index("the merge token"), rest)
        self.assertGreater(body.index("A typo"), rest)
        for system in (claude_calls.REVIEW_SYSTEM, claude_calls.SECURITY_REVIEW_SYSTEM):
            self.assertIn("never only the first", system)

    def test_2_the_review_reads_the_diff_of_the_sha_it_labels(self):
        repo = RecordingGH.repo
        pinned = f"repos/{repo}/compare/{rules.TRUNK}...{HEAD}"
        # A push swap: the PR's head is now a harmless commit, while the sweep read HEAD and labels the review with it.
        gh = DiffGH({f"repos/{repo}/pulls/9": "diff --git a/README b/README\n+harmless\n",
                     pinned: "diff --git a/.github/workflows/x.yml b/.github/workflows/x.yml\n+run: steal\n"})
        sw = make(gh)
        sw.root = HERE.parents[1]
        sent = []

        def paid(kind, job, call, system, content, schema, cap, key):
            sent.append(content)
            return claude_calls.Result({"verdict": "LAND", "summary": "ok", "notes": []}, "end_turn", "m", 1, 1)
        sw.paid = paid
        sw.review(facts(verdicts=(codex(),)), ctx(), "meta", security=True)
        self.assertEqual(gh.reads, [(pinned, "application/vnd.github.diff")])
        ((*_, diff),) = sent
        self.assertIn("+run: steal", diff["text"])
        self.assertNotIn("harmless", diff["text"])
        ((_, body),) = gh.posted
        self.assertIn(f"@ {HEAD}): LAND", body.split("\n", 1)[0])
        self.assertIn(switch.MARK.format(kind="review", pr=9, head=HEAD), body)
        # A PR on another base: the compare uses that base, as the PR's own diff does (three dots: the merge base).
        gh = DiffGH({})
        sw = make(gh)
        sw.root, sw.paid = HERE.parents[1], paid
        sw.review(facts(base_ref="muse/stack#1"), ctx(), "meta")
        self.assertEqual(gh.reads[0][0], f"repos/{repo}/compare/muse/stack%231...{HEAD}")

    def test_4_the_security_pass_judges_against_security_md(self):
        # Codex on #297: the gameplay prompt answers LAND unless physics, gameplay or a test breaks, so a secret
        # leak passed it. The security pass sends SECURITY.md and its own prompt; the ordinary review does not.
        sent = []

        def paid(kind, job, call, system, content, schema, cap, key):
            sent.append((system, "".join(b["text"] for b in content)))
            return claude_calls.Result({"verdict": "LAND", "summary": "ok", "notes": []}, "end_turn", "m", 1, 1)
        for security in (True, False):
            sw = make(DiffGH({}))
            sw.root, sw.paid = HERE.parents[1], paid
            sw.review(facts(), ctx(), "meta", security=security)
        ((sec_system, sec_text), (plain_system, plain_text)) = sent
        self.assertIs(sec_system, claude_calls.SECURITY_REVIEW_SYSTEM)
        self.assertIn("<security_guidelines>", sec_text)
        self.assertIn("pull_request_target", sec_text)           # a SECURITY.md rule reached the reviewer
        self.assertIs(plain_system, claude_calls.REVIEW_SYSTEM)
        self.assertNotIn("<security_guidelines>", plain_text)

class ThreadGH(RecordingGH):
    """RecordingGH with a PR thread: switch.verdicts() reads these comments on #9."""

    def __init__(self, comments=()):
        super().__init__()
        self.comments = list(comments)

    def paged(self, path, key=""):
        return self.comments if path == f"repos/{self.repo}/issues/9/comments" else []


def said(body: str, login: str = BOT) -> dict:
    return {"body": body, "created_at": T1, "updated_at": T1, "user": {"login": login}}


def posted_review(verdict: str = "LAND", sha: str = HEAD, number: int = 9, key: str = KEY) -> str:
    """The body switch.review() posts for this verdict, signed with `key` ("" posts it unsigned)."""
    gh = DiffGH({})
    sw = make(gh, key=key)
    sw.root = HERE.parents[1]
    data = {"verdict": verdict, "summary": "ok", "notes": []}
    if verdict == "FIX":
        data["repro"] = {"command": "push", "input": "x", "expected": "y", "observed": "z"}
    sw.paid = lambda *a: claude_calls.Result(data, "end_turn", "m", 1, 1)
    sw.review(facts(number=number, head_sha=sha), ctx(), "meta", security=True)
    ((_, body),) = gh.posted
    if key:
        assert key not in body and not any(key in line for line in sw.lines)   # the key is never written out
    return body


def read(*bodies: str, key: str = KEY) -> tuple:
    """What switch.verdicts() makes of these bot comments on #9 with `key`."""
    return tuple(make(ThreadGH([said(b) for b in bodies]), key=key).verdicts(9)[0])


def last_line(body: str) -> str:
    return body.rstrip().rsplit("\n", 1)[1]


def without_last_line(body: str) -> str:
    return body.rstrip().rsplit("\n", 1)[0]


class TheSignedReview(unittest.TestCase):
    """Codex on #297, finding B: any branch can push a copy of a push-triggered workflow whose github.token posts
    as github-actions[bot], so on a workflow-file PR only an automatic review carrying a MAC under
    SWITCHBOARD_REVIEW_KEY (held only in environment switchboard-merge) is the security pass (J-056)."""

    def test_a_failed_security_pass_note_is_signed_so_it_is_not_paid_twice(self):
        gh = RecordingGH()
        make(gh).review_failed(facts(verdicts=(codex(),)), ctx(), "meta", True, "The automatic review failed to run. ")
        ((_, body),) = gh.posted
        self.assertTrue(make(RecordingGH()).marked([said(body)], "review", 9, HEAD, signed=True))
        self.assertFalse(make(RecordingGH()).marked([said(without_last_line(body))], "review", 9, HEAD, signed=True))
        self.assertIsNone(switch.VERDICT_TRIGGER.search(body))


if __name__ == "__main__":
    unittest.main()
