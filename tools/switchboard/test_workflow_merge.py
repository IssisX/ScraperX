"""Unit tests for merging workflow-file PRs with the merge token (J-056).

Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
No network, no clock: GitHub, the rule check, the token and the time are given.
"""

from __future__ import annotations

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


def api(verdict: str = "LAND", sha: str = HEAD, extra: str = "", at: str = T1, width: int = 40) -> rules.Verdict:
    """The automatic Claude review as switch.review() posts it, by the workflow bot: the whole head sha
    (width=8 is the mark it posted before J-056's security fixes)."""
    return v(f"**Claude (automatic review of #9 @ {sha[:width]}): {verdict}**\n\nsummary{extra}", at, BOT)


def facts(**kw) -> rules.PrFacts:
    base = dict(number=9, head_sha=HEAD, head_ref="muse/integrate-j056", base_ref=rules.TRUNK, draft=False,
                mergeable=True, head_committed_at=T0, proof="green", verdicts=(codex(), api()),
                needs=frozenset(), rule_check_block=False, touches_workflows=True)
    base.update(kw)
    return rules.PrFacts(**base)


def decide(f: rules.PrFacts, token: bool = True) -> rules.MergeDecision:
    return rules.merge_decision(f, TEAM.vendor_of_branch(f.head_ref), TEAM, merge_token=token)


def ctx(comments=()):
    return {"job": "J-056", "issue": None, "proof_url": "https://x/runs/7", "run_id": 7, "proof_done_at": T1,
            "comments": list(comments), "title": "t", "body": "", "files": [], "updated_at": T2}


class RecordingGH:
    """The GitHub calls merge() and steer_pr make: every write recorded with the token it was sent with."""

    repo, dry_run = "IssisX/ScraperX", False

    def __init__(self, fail: str = ""):
        self.fail, self.writes, self.posted = fail, [], []

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


def make(gh, token: str = TOKEN):
    sw = switch.Switch.__new__(switch.Switch)
    sw.gh, sw.now, sw.team, sw.lines, sw._load = gh, NOW, TEAM, [], None
    sw.out = sw.lines.append
    sw.run_rule_check = lambda number, head: RC_CLEAN
    sw.merge_token = token
    return sw


class TheRule(unittest.TestCase):
    def test_token_every_gate_and_a_clean_api_review_merge(self):
        d = decide(facts())
        self.assertEqual((d.action, d.land_by), ("MERGE", "Codex"))   # the Codex app's LAND is the one that counts
        self.assertIn("clean automatic Claude review (LAND) on @1a2b3c4d", d.reasons[0])
        self.assertEqual(decide(facts(verdicts=(codex(), api("NOTE")))).action, "MERGE")   # a NOTE is no FIX or BLOCK
        # The Claude API's own LAND is never the other vendor's LAND on a workflow PR, on any branch.
        d = decide(facts(verdicts=(api(),)))
        self.assertEqual(d.action, "HUMAN-MERGE")
        self.assertIn("no LAND from a vendor other than meta", d.reasons[0])

    def test_no_token_is_human_merge_exactly_as_before(self):
        today = ("HUMAN-MERGE", ("ready, but it changes .github/workflows; the workflow token cannot merge it",))
        for f, by in ((facts(), "Claude"), (facts(verdicts=(codex(),)), "Codex")):
            d = decide(f, token=False)
            self.assertEqual((d.action, d.reasons, d.land_by), today + (by,))
            self.assertEqual(rules.merge_decision(f, "meta", TEAM), d)   # the default is no token

    def test_api_review_fix_or_block_is_not_merge(self):
        # Without a repro a FIX or BLOCK reads as a NOTE elsewhere; for the security pass it still holds the merge.
        for raw in ("FIX", "BLOCK"):
            d = decide(facts(verdicts=(codex(), api(raw))))
            self.assertEqual(d.action, "HUMAN-MERGE", raw)
            self.assertIn("automatic Claude review on this head is FIX or BLOCK", d.reasons[0])
            self.assertEqual(decide(facts(verdicts=(codex(), api(raw, extra=REPRO)))).action, "FIX", raw)
        # A hand-posted Claude LAND after the API's FIX clears open_fixes, never the security pass.
        later = v("**Claude (review of #9 @ 1a2b3c4d): LAND**", T2)
        self.assertEqual(decide(facts(verdicts=(codex(), api("FIX", extra=REPRO), later))).action, "HUMAN-MERGE")

    def test_no_api_review_on_the_head_is_not_merge(self):
        hand = v("**Claude (automatic review of #9 @ 1a2b3c4d): LAND**", T1, "IssisX")   # not the bot: not the API
        for verdicts in ((codex(),), (codex(), api(sha=OTHER_HEAD)), (codex(), hand)):
            d = decide(facts(verdicts=verdicts))
            self.assertEqual(d.action, "HUMAN-MERGE")
            self.assertIn("no automatic Claude review on this head (the security pass)", d.reasons[0])

    def test_the_apis_own_land_on_claudes_work_is_not_the_other_vendors_land(self):
        ref = "claude/integrate-j056-workflow-merge"
        self.assertEqual(decide(facts(head_ref=ref, verdicts=(api(),), touches_workflows=False)).action, "MERGE")
        d = decide(facts(head_ref=ref, verdicts=(api(),)))
        self.assertEqual(d.action, "HUMAN-MERGE")
        self.assertIn("no LAND from a vendor other than anthropic", d.reasons[0])
        self.assertEqual(decide(facts(head_ref=ref, verdicts=(api(), codex()))).action, "MERGE")

    def test_every_other_gate_still_holds_with_the_token(self):
        for f, action in ((facts(proof="running"), "WAIT"), (facts(proof="red: native-tests"), "FIX-CI"),
                          (facts(verdicts=(api(),), head_ref="claude/integrate-x"), "HUMAN-MERGE"),
                          (facts(verdicts=(api(sha=OTHER_HEAD),)), "WAIT"), (facts(mergeable=False), "WAIT"),
                          (facts(rule_check_block=True), "WAIT"), (facts(rule_check_block=None), "WAIT"),
                          (facts(never_edit=True, rule_check_block=True, head_ref="muse/integrate-protected-x"), "WAIT"),
                          (facts(draft=True), "SKIP"), (facts(needs=frozenset({"eyes"})), "WAIT")):
            self.assertEqual(decide(f).action, action, f)

    def test_the_security_pass_is_asked_only_where_it_is_missing(self):
        self.assertTrue(rules.needs_security_pass(facts(verdicts=(codex(),))))
        for f in (facts(), facts(touches_workflows=False, verdicts=(codex(),)), facts(proof="running", verdicts=(codex(),)),
                  facts(draft=True, verdicts=(codex(),)), facts(verdicts=(codex(), api("FIX", extra=REPRO)))):
            self.assertFalse(rules.needs_security_pass(f), f)


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

    def test_a_workflow_pr_codex_landed_gets_the_automatic_review(self):
        calls, lines = self.ask(facts(head_ref="claude/integrate-j056", verdicts=(codex(),)))
        self.assertEqual(calls, [(9, True)])
        self.assertTrue(any(l.startswith("SECURITY    #9") for l in lines))

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

    def test_1_an_8_character_prefix_collision_is_refused(self):
        # HEAD was reviewed under the old 8-character marks; COLLIDE shares those 8 characters and nothing else.
        for verdicts in ((api(width=8), codex(COLLIDE)),            # the API's old mark names only the prefix
                         (api(sha=COLLIDE), codex(HEAD[:8])),       # a Codex state naming only the prefix
                         (api(width=8), codex(HEAD[:8]))):
            d = decide(facts(head_sha=COLLIDE, verdicts=verdicts))
            self.assertEqual(d.action, "HUMAN-MERGE", verdicts)
        self.assertIn("no automatic Claude review on this head",
                      decide(facts(head_sha=COLLIDE, verdicts=(api(width=8), codex(COLLIDE)))).reasons[0])
        # The ground head gets its own security pass: an old mark on another head does not stand in for it.
        self.assertTrue(rules.needs_security_pass(facts(head_sha=COLLIDE, verdicts=(codex(COLLIDE), api(width=8)))))
        self.assertEqual(decide(facts(head_sha=COLLIDE, verdicts=(api(sha=COLLIDE), codex(COLLIDE)))).action, "MERGE")
        # Off .github/workflows an old 8-character mark keeps its meaning.
        self.assertEqual(decide(facts(touches_workflows=False, verdicts=(api(width=8),))).action, "MERGE")

    def test_1_the_automatic_review_and_the_ask_name_the_whole_head(self):
        gh = DiffGH({})
        sw = make(gh)
        sw.root = HERE.parents[1]
        sw.paid = lambda *a: claude_calls.Result({"verdict": "LAND", "summary": "ok", "notes": []}, "end_turn", "m", 1, 1)
        sw.review(facts(), ctx(), "meta", security=True)
        ((_, body),) = gh.posted
        self.assertTrue(body.startswith(f"**Claude (automatic review of #9 @ {HEAD}): LAND**"), body[:120])
        got = rules.parse_verdict(body, T2, BOT, TEAM)
        self.assertEqual((got.sha, rules.api_review_on([got], HEAD)), (HEAD, got))
        self.assertIsNone(rules.api_review_on([got], COLLIDE))

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

    def test_3_a_typed_name_land_is_refused_on_a_workflow_pr(self):
        typed = v(f"**ChatGPT (review of #9 @ {HEAD}): LAND**")          # any agent can type it: one login
        typed_codex = v(f"**Codex (review of #9 @ {HEAD}): LAND**")      # the name alone, posted by IssisX
        for lands in ((typed,), (typed_codex,), (typed, typed_codex), ()):
            for ref in ("muse/integrate-j056", "claude/integrate-j056", "grok/integrate-j056"):
                d = decide(facts(head_ref=ref, verdicts=(api(),) + lands))
                self.assertEqual(d.action, "HUMAN-MERGE", (ref, lands))
                self.assertIn("no LAND from a vendor other than", d.reasons[0])
        # A typed Codex LAND after the Codex app's FIX on the head clears open_fixes, never this gate.
        d = decide(facts(verdicts=(api(), codex(unresolved=True), dataclasses.replace(typed_codex, created_at=T2))))
        self.assertEqual(d.action, "HUMAN-MERGE")
        # The Codex app's clean review on the whole head, by its own login, is the LAND that counts.
        for ref in ("muse/integrate-j056", "claude/integrate-j056", "grok/integrate-j056"):
            d = decide(facts(head_ref=ref, verdicts=(api(), typed, codex())))
            self.assertEqual((d.action, d.land_by), ("MERGE", "Codex"), ref)
        # Codex is the author's own vendor on ChatGPT's or Codex's work: no LAND counts there.
        self.assertEqual(decide(facts(head_ref="chatgpt/integrate-j056", verdicts=(api(), codex()))).action, "HUMAN-MERGE")
        # Off .github/workflows a typed LAND counts as before.
        self.assertEqual(decide(facts(touches_workflows=False, verdicts=(typed,))).action, "MERGE")


if __name__ == "__main__":
    unittest.main()
