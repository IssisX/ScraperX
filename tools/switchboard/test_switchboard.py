"""Unit tests for the switchboard's decisions: verdict parsing, the merge rule, the budget, the reviewer pick.

Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
No network, no clock: every time is given.
"""

from __future__ import annotations

import dataclasses
import datetime as dt
import json
import os
import pathlib
import re
import sys
import types
import unittest
from decimal import Decimal

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import budget as money  # noqa: E402
import claude_calls  # noqa: E402
import rules  # noqa: E402
import switch  # noqa: E402

try:
    import anthropic  # noqa: F401
    import httpx2
    HAVE_SDK = True
except ImportError:   # the free parts of the switch run without the SDK
    HAVE_SDK = False

AGENTS = {
    "agents": {
        "claude": {"vendor": "anthropic", "names": ["Claude"], "branch_prefixes": ["claude/"]},
        "chatgpt": {"vendor": "openai", "names": ["ChatGPT"], "branch_prefixes": ["chatgpt/"]},
        "grok": {"vendor": "xai", "names": ["Grok", "PE", "Builder"], "branch_prefixes": ["grok/", "pe/", "builder/"]},
        "muse": {"vendor": "meta", "names": ["Muse"], "branch_prefixes": ["muse/"]},
    },
    "review": {"claude_authored_asks": ["chatgpt", "muse"], "matron_reviewer": "muse"},
    "verdict_logins": ["IssisX", "github-actions[bot]"],
}
TEAM = rules.team_from_config(AGENTS)
HEAD = "1a2b3c4d5e6f708192a3b4c5d6e7f8091a2b3c4d"
OLD = "ffffffff00000000ffffffff00000000ffffffff"
T0 = "2026-11-02T10:00:00Z"     # head committed
T1 = "2026-11-02T11:00:00Z"
T2 = "2026-11-02T12:00:00Z"
REPRO = "\ncommand: ./run x\ninput: seed 7\nexpected: PASS\nobserved: FAIL at 3.2 m\n"


def v(body: str, at: str = T1, login: str = "IssisX") -> rules.Verdict:
    got = rules.parse_verdict(body, at, login, TEAM)
    assert got is not None, body
    return got


def land(who: str, sha: str = HEAD, at: str = T1) -> rules.Verdict:
    return v(f"**{who} (review of #9 @ {sha[:8]}): LAND**", at)


def facts(**kw) -> rules.PrFacts:
    base = dict(number=9, head_sha=HEAD, head_ref="grok/integrate-x", base_ref=rules.TRUNK, draft=False,
                mergeable=True, head_committed_at=T0, proof="green", verdicts=(land("ChatGPT"),),
                needs=frozenset(), rule_check_block=False, touches_workflows=False)
    base.update(kw)
    return rules.PrFacts(**base)


class VerdictParsing(unittest.TestCase):
    def test_outsider_login_is_never_a_verdict(self):
        body = "**ChatGPT (review of #9 @ 1a2b3c4d): LAND**"
        self.assertIsNone(rules.parse_verdict(body, T1, "some-stranger", TEAM))
        self.assertIsNone(rules.parse_verdict("**ChatGPT (review of #9 @ 1a2b3c4d): FIX**" + REPRO, T1, "x", TEAM))
        self.assertIsNotNone(rules.parse_verdict(body, T1, "issisx", TEAM))  # logins compare case-blind

    def test_outsider_cannot_merge_or_clear_a_fix(self):
        fix = v("**Claude (review of #9 @ 1a2b3c4d): FIX**" + REPRO, T1)
        forged = [rules.parse_verdict("**ChatGPT (review of #9 @ 1a2b3c4d): LAND**", T2, "outsider", TEAM),
                  rules.parse_verdict("**Claude (review of #9 @ 1a2b3c4d): LAND**", T2, "outsider", TEAM)]
        kept = tuple(x for x in [fix] + forged if x is not None)
        self.assertEqual(rules.merge_decision(facts(head_ref="claude/integrate-x", verdicts=kept), "anthropic").action, "FIX")
        self.assertEqual(rules.merge_decision(facts(head_ref="claude/integrate-x", verdicts=()), "anthropic").action, "WAIT")

    def test_team_needs_verdict_logins(self):
        cfg = dict(AGENTS)
        del cfg["verdict_logins"]
        with self.assertRaises(ValueError):
            rules.team_from_config(cfg)

    def test_team_format(self):
        got = v("**ChatGPT (review of #88 @ f9575008): LAND**\n\nlooks right")
        self.assertEqual((got.vendor, got.kind, got.verdict, got.sha), ("openai", "review", "LAND", "f9575008"))

    def test_only_the_first_line_counts(self):
        self.assertIsNone(rules.parse_verdict("thanks\n**ChatGPT (review of #9 @ 1a2b3c4d): LAND**", T1, "IssisX", TEAM))

    def test_fix_without_repro_is_a_note(self):
        got = v("**Grok (review of #9 @ 1a2b3c4d): FIX**\nthe arm looks off")
        self.assertEqual((got.verdict, got.downgraded), ("NOTE", True))

    def test_fix_with_repro_stays_fix(self):
        got = v("**Grok (review of #9 @ 1a2b3c4d): FIX**" + REPRO)
        self.assertEqual((got.verdict, got.downgraded), ("FIX", False))

    def test_fix_missing_one_repro_field_is_a_note(self):
        got = v("**Grok (review of #9 @ 1a2b3c4d): FIX**\ncommand: x\ninput: y\nexpected: z\n")
        self.assertEqual(got.verdict, "NOTE")

    def test_block_reads_as_fix(self):
        self.assertEqual(v("**PE (review of #9 @ 1a2b3c4d): BLOCK**" + REPRO).verdict, "FIX")
        self.assertEqual(v("**PE (review of #9 @ 1a2b3c4d): BLOCK**").verdict, "NOTE")

    def test_eye_and_unknown_vendor(self):
        self.assertEqual(v("**Claude eye (look of #9 @ 1a2b3c4d): LAND**").kind, "eye")
        self.assertEqual(v("**Gemini (review of #9 @ 1a2b3c4d): LAND**").vendor, "unknown")

    def test_alias_names(self):
        self.assertEqual(v("**Builder (review of #9 @ 1a2b3c4d): LAND**").vendor, "xai")

    def test_not_a_verdict(self):
        for body in ("**Claude (master integrator), 12:32 PM Central → ChatGPT, on your #88 FIX.** Thanks",
                     "LAND", "plain text", ""):
            self.assertIsNone(rules.parse_verdict(body, T1, "IssisX", TEAM), body)

    def test_branch_vendor_longest_prefix(self):
        self.assertEqual(TEAM.vendor_of_branch("pe/integrate-wreck"), "xai")
        self.assertEqual(TEAM.vendor_of_branch("claude/integrate-switchboard-v0"), "anthropic")
        self.assertEqual(TEAM.vendor_of_branch("someone/else"), "unknown")


class MergeRule(unittest.TestCase):
    def test_merges_when_every_condition_holds(self):
        d = rules.merge_decision(facts(), "xai")
        self.assertEqual((d.action, d.land_by), ("MERGE", "ChatGPT"))

    def test_land_from_the_authors_vendor_does_not_count(self):
        d = rules.merge_decision(facts(verdicts=(land("PE"),)), "xai")
        self.assertEqual(d.action, "WAIT")

    def test_land_from_unknown_vendor_does_not_count(self):
        self.assertEqual(rules.merge_decision(facts(verdicts=(land("Gemini"),)), "xai").action, "WAIT")

    def test_land_must_name_the_head(self):
        self.assertEqual(rules.merge_decision(facts(verdicts=(land("ChatGPT", OLD),)), "xai").action, "WAIT")
        no_sha = v("**ChatGPT (review of #9): LAND**", T2)
        self.assertEqual(rules.merge_decision(facts(verdicts=(no_sha,)), "xai").action, "WAIT")

    def test_proof_red_running_none(self):
        self.assertEqual(rules.merge_decision(facts(proof="red: native-tests"), "xai").action, "FIX-CI")
        self.assertEqual(rules.merge_decision(facts(proof="running"), "xai").action, "WAIT")
        self.assertEqual(rules.merge_decision(facts(proof="none"), "xai").action, "WAIT")

    def test_open_fix_blocks_until_the_same_reviewer_lands(self):
        fix = v("**Claude (review of #9 @ 1a2b3c4d): FIX**" + REPRO, T1)
        self.assertEqual(rules.merge_decision(facts(verdicts=(land("ChatGPT"), fix)), "xai").action, "FIX")
        later = land("Claude", at=T2)
        self.assertEqual(rules.merge_decision(facts(verdicts=(land("ChatGPT"), fix, later)), "xai").action, "MERGE")

    def test_fix_without_repro_never_blocks(self):
        note = v("**Claude (review of #9 @ 1a2b3c4d): FIX**\nfeels wrong", T2)
        self.assertEqual(rules.merge_decision(facts(verdicts=(land("ChatGPT"), note)), "xai").action, "MERGE")

    def test_fix_on_an_older_head_is_not_open(self):
        old_fix = v("**Claude (review of #9 @ ffffffff): FIX**" + REPRO, T1)
        self.assertEqual(rules.merge_decision(facts(verdicts=(old_fix, land("ChatGPT"))), "xai").action, "MERGE")

    def test_fix_naming_no_head_counts_when_after_the_head(self):
        fix = v("**Claude (review of #9): FIX**" + REPRO, T2)
        self.assertEqual(rules.merge_decision(facts(verdicts=(land("ChatGPT"), fix)), "xai").action, "FIX")
        before = v("**Claude (review of #9): FIX**" + REPRO, "2026-11-02T09:00:00Z")
        self.assertEqual(rules.merge_decision(facts(verdicts=(land("ChatGPT"), before)), "xai").action, "MERGE")

    def test_rule_check_block_or_missing_waits(self):
        self.assertEqual(rules.merge_decision(facts(rule_check_block=True), "xai").action, "WAIT")
        self.assertEqual(rules.merge_decision(facts(rule_check_block=None), "xai").action, "WAIT")

    def test_a_declared_protected_change_merges_on_green_and_land(self):
        protected = "fb-bot/integrate-protected-j033-sweep-scenario"
        self.assertEqual(rules.merge_decision(facts(head_ref=protected, rule_check_block=True), "xai").action, "MERGE")
        self.assertEqual(rules.merge_decision(facts(head_ref=protected, rule_check_block=True, verdicts=()), "xai").action,
                         "WAIT")
        self.assertEqual(rules.merge_decision(facts(head_ref=protected, rule_check_block=True, proof="running"), "xai").action,
                         "WAIT")
        # A never-edited file (MANIFEST, SHA256SUMS, the Governing Laws) blocks on every branch (Codex P1 on #203).
        self.assertEqual(rules.merge_decision(facts(head_ref=protected, rule_check_block=True, never_edit=True), "xai").action,
                         "WAIT")

    def test_eyes_need_the_bots_eye_land_on_the_head(self):
        need = frozenset({"eyes"})
        self.assertEqual(rules.merge_decision(facts(needs=need), "xai").action, "WAIT")
        spoofed = v("**Claude eye (look of #9 @ 1a2b3c4d): LAND**", T1, "IssisX")
        self.assertEqual(rules.merge_decision(facts(needs=need, verdicts=(land("ChatGPT"), spoofed)), "xai").action, "WAIT")
        eye = v("**Claude eye (look of #9 @ 1a2b3c4d): LAND**", T1, rules.BOT_LOGIN)
        self.assertEqual(rules.merge_decision(facts(needs=need, verdicts=(land("ChatGPT"), eye)), "xai").action, "MERGE")

    def test_eye_fix_blocks(self):
        eye_fix = v("**Claude eye (look of #9 @ 1a2b3c4d): FIX**" + REPRO, T1, rules.BOT_LOGIN)
        d = rules.merge_decision(facts(needs=frozenset({"eyes"}), verdicts=(land("ChatGPT"), eye_fix)), "xai")
        self.assertEqual(d.action, "FIX")

    def test_eye_land_is_not_a_review_land(self):
        eye = v("**Claude eye (look of #9 @ 1a2b3c4d): LAND**", T1, rules.BOT_LOGIN)
        self.assertEqual(rules.merge_decision(facts(verdicts=(eye,)), "xai").action, "WAIT")

    def test_conflict_draft_stacked_workflows(self):
        self.assertEqual(rules.merge_decision(facts(mergeable=False), "xai").action, "WAIT")
        self.assertEqual(rules.merge_decision(facts(mergeable=None), "xai").action, "WAIT")
        self.assertEqual(rules.merge_decision(facts(draft=True), "xai").action, "SKIP")
        self.assertEqual(rules.merge_decision(facts(base_ref="grok/other"), "xai").action, "SKIP")
        self.assertEqual(rules.merge_decision(facts(touches_workflows=True), "xai").action, "HUMAN-MERGE")


class ReviewerPick(unittest.TestCase):
    def test_non_claude_authors_get_the_automatic_claude_review_when_no_one_is_listed(self):
        for vendor in ("openai", "xai", "meta", "unknown"):
            self.assertEqual(rules.pick_reviewer(vendor, frozenset(), TEAM), rules.ReviewPick("claude-api", ""))

    def test_non_claude_authors_ask_muse_first_then_chatgpt_for_muses_own(self):
        cfg = dict(AGENTS, review=dict(AGENTS["review"], other_authored_asks=["muse", "chatgpt"]))
        team = rules.team_from_config(cfg)
        self.assertEqual(rules.pick_reviewer("openai", frozenset(), team), rules.ReviewPick("ask", "muse"))
        self.assertEqual(rules.pick_reviewer("xai", frozenset({"matron"}), team), rules.ReviewPick("ask", "muse"))
        self.assertEqual(rules.pick_reviewer("meta", frozenset(), team), rules.ReviewPick("ask", "chatgpt"))
        self.assertEqual(rules.pick_reviewer("unknown", frozenset(), team), rules.ReviewPick("claude-api", ""))
        only_muse = rules.team_from_config(dict(AGENTS, review=dict(AGENTS["review"], other_authored_asks=["muse"])))
        self.assertEqual(rules.pick_reviewer("meta", frozenset(), only_muse), rules.ReviewPick("claude-api", ""))

    def test_other_authored_asks_never_names_claude(self):
        cfg = dict(AGENTS, review=dict(AGENTS["review"], other_authored_asks=["claude"]))
        with self.assertRaises(ValueError):
            rules.team_from_config(cfg)

    def test_claude_authored_asks_chatgpt_or_muse_for_matron(self):
        self.assertEqual(rules.pick_reviewer("anthropic", frozenset(), TEAM), rules.ReviewPick("ask", "chatgpt"))
        self.assertEqual(rules.pick_reviewer("anthropic", frozenset({"matron"}), TEAM), rules.ReviewPick("ask", "muse"))

    def test_needs_review_and_eye(self):
        self.assertFalse(rules.needs_review(facts()))
        self.assertTrue(rules.needs_review(facts(verdicts=())))
        self.assertFalse(rules.needs_review(facts(verdicts=(), proof="running")))
        self.assertTrue(rules.needs_eye(facts(needs=frozenset({"eyes"}))))
        self.assertFalse(rules.needs_eye(facts()))

    def test_config_errors_name_the_problem(self):
        bad = dict(AGENTS, review={"claude_authored_asks": ["codex"]})
        with self.assertRaisesRegex(ValueError, "codex"):
            rules.team_from_config(bad)


class Board(unittest.TestCase):
    def test_owner_labels_keep_the_first(self):
        self.assertEqual(rules.extra_owner_labels(["job", "owner:grok", "state:ready", "owner:chatgpt"]), ["owner:chatgpt"])
        self.assertEqual(rules.extra_owner_labels(["owner:muse"]), [])

    def test_job_ids_and_budget_comments(self):
        self.assertEqual(rules.job_of_pr("Fixes it.\nJob: J-012\n"), "J-012")
        self.assertEqual(rules.job_of_pr(None), "")
        self.assertEqual(rules.job_of_title("J-031 Switchboard v0"), "J-031")
        # a lettered sub-job is its own job, in a PR body, a commit message and an issue title
        self.assertEqual(rules.job_of_pr("Job: J-044b: the grant is capped at real speed"), "J-044b")
        self.assertEqual(rules.job_of_title("J-044b Fight math and risk"), "J-044b")
        self.assertEqual(rules.job_of_pr("Job: J-044bc"), "")
        self.assertEqual(rules.job_of_title("J-044B upper case is not a job"), "")
        self.assertEqual(rules.budget_override([("budget 12", "IssisX"), ("ok", "IssisX"), ("Budget 15.50\nthanks", "IssisX")], TEAM), "15.50")
        self.assertEqual(rules.budget_override([("my budget 12 is fine", "IssisX")], TEAM), "")

    def test_superseded_prs_are_read_from_the_body(self):
        self.assertEqual(rules.superseded_prs("Drawn-body checks.\nSupersedes #43, per the spec.", 100), [43])
        self.assertEqual(rules.superseded_prs("supersedes #43 and #44; also Supersedes #43, #45", 100), [43, 44, 45])
        self.assertEqual(rules.superseded_prs("Supersedes #100", 100), [])
        self.assertEqual(rules.superseded_prs("Fixes #43. Supersedes nothing.", 100), [])
        self.assertEqual(rules.superseded_prs(None, 1), [])

    def test_a_ready_pr_closes_the_open_prs_it_supersedes(self):
        class GH:
            repo = "IssisX/ScraperX"
            def __init__(self):
                self.writes, self.comments = [], []
            def get(self, path):
                n = int(path.rsplit("/", 1)[1])
                if n == 9:
                    raise switch.GitHubError("GET", path, "gh: Not Found (HTTP 404)")
                return {"state": "closed" if n == 44 else "open"}
            def comment(self, number, body, what=""):
                self.comments.append(number)
            def write(self, method, path, body=None, what=""):
                self.writes.append((method, path, body))
        ctx = {"body": "Supersedes #43, #44 and #9"}
        for draft, closed in ((False, [43]), (True, [])):
            gh, lines = GH(), []
            # J-072: which are open comes from the open-PR listing (#44 is closed, #9 is not a PR)
            me = types.SimpleNamespace(gh=gh, out=lines.append, listing=lambda path: [{"number": 43}, {"number": 100}])
            facts = types.SimpleNamespace(number=100, draft=draft)
            switch.Switch.close_superseded(me, facts, ctx)
            self.assertEqual(gh.comments, closed)
            self.assertEqual(gh.writes, [("PATCH", f"repos/IssisX/ScraperX/pulls/{n}", {"state": "closed"}) for n in closed])

    def test_budget_override_ignores_outsiders_and_the_bot(self):
        self.assertEqual(rules.budget_override([("budget 100", "outside-team")], TEAM), "")
        self.assertEqual(rules.budget_override([("budget 9", "IssisX"), ("budget 100", "outside-team")], TEAM), "9")
        self.assertEqual(rules.budget_override([("budget 100", "github-actions[bot]")], TEAM), "")

    def test_jobs_found_through_their_titles(self):
        got = rules.jobs_by_pr_mention({"J-012": "J-012 Her drawn-body checks (#100, supersedes #43)",
                                        "J-030": "J-030 Claim or close the old PRs: #14, #29, #43"})
        self.assertEqual(got.get(100), "J-012")
        self.assertEqual(got.get(14), "J-030")
        self.assertNotIn(43, got)   # named by two jobs: neither

    def test_stale_heads(self):
        self.assertFalse(rules.is_stale(T0, "2026-11-05T09:59:59Z", 3))
        self.assertTrue(rules.is_stale(T0, "2026-11-05T10:00:01Z", 3))


BUDGET = {
    "caps": {"month_usd": "100.00", "day_usd": "10.00", "job_usd": "8.00"},
    "prices_per_mtok": {"claude-opus-5-5": {"input": "4.00", "output": "20.00"}},
    "calls": {"review": {"model": "claude-opus-5-5", "max_tokens": 16000, "effort": "high", "max_input_tokens": 150000}},
    "orchestrator": {"enabled": False, "session_usd": "2.00"},
}
NOW = dt.datetime(2026, 11, 2, 18, 0, tzinfo=dt.timezone.utc)


def log(*rows: tuple[str, str, str, str, str]) -> list[money.Entry]:
    """rows of (ts, run, phase, job, usd)."""
    lines = [money.record(dt.datetime.fromisoformat(ts.replace("Z", "+00:00")), run, phase, "review", job, Decimal(u))
             for ts, run, phase, job, u in rows]
    return money.parse_log("\n".join(lines))


class Budget(unittest.TestCase):
    def setUp(self):
        self.b = money.budget_from_config(BUDGET)

    def test_cost_is_exact_decimal(self):
        self.assertEqual(self.b.price("claude-opus-5-5").cost(150_000, 16_000), Decimal("0.920000"))
        self.assertEqual(money.worst_case(self.b, "review", 1000), Decimal("0.324000"))

    def test_month_cap_above_the_credit_is_refused(self):
        bad = dict(BUDGET, caps={"month_usd": "100.01", "day_usd": "1", "job_usd": "1"})
        with self.assertRaisesRegex(money.BudgetConfigError, "API credit"):
            money.budget_from_config(bad)

    def test_unpriced_model_is_refused(self):
        bad = dict(BUDGET, calls={"x": {"model": "claude-fable-5-1", "max_tokens": 1, "effort": "low", "max_input_tokens": 1}})
        with self.assertRaisesRegex(money.BudgetConfigError, "no price"):
            money.budget_from_config(bad)

    def test_allows_under_every_cap(self):
        self.assertTrue(money.check(self.b, [], NOW, "J-001", Decimal("0.92")).ok)

    def test_day_cap(self):
        e = log(("2026-11-02T01:00:00Z", "a", "end", "J-001", "9.50"))
        c = money.check(self.b, e, NOW, "J-002", Decimal("0.92"))
        self.assertFalse(c.ok)
        self.assertIn("day cap", c.reason)
        self.assertTrue(money.check(self.b, e, NOW + dt.timedelta(days=1), "J-002", Decimal("0.92")).ok)

    def test_month_cap(self):
        e = log(*[(f"2026-11-01T{h:02d}:00:00Z", f"r{h}", "end", f"J-{h:03d}", "4.50") for h in range(22)])  # $99.00
        c = money.check(self.b, e, NOW + dt.timedelta(days=1), "J-900", Decimal("1.01"))
        self.assertFalse(c.ok)
        self.assertIn("month cap", c.reason)
        self.assertTrue(money.check(self.b, e, dt.datetime(2026, 12, 1, tzinfo=dt.timezone.utc), "J-900", Decimal("1.01")).ok)

    def test_job_cap_and_its_override(self):
        e = log(("2026-10-30T01:00:00Z", "a", "end", "J-005", "7.50"))
        self.assertFalse(money.check(self.b, e, NOW, "J-005", Decimal("0.92")).ok)
        self.assertTrue(money.check(self.b, e, NOW, "J-005", Decimal("0.92"), "12").ok)
        self.assertTrue(money.check(self.b, e, NOW, "J-006", Decimal("0.92")).ok)

    def test_override_never_passes_the_month_cap(self):
        c = money.check(self.b, [], NOW, "J-005", Decimal("150"), "500")
        self.assertFalse(c.ok)

    def test_reserve_without_end_counts_in_full(self):
        e = log(("2026-11-02T01:00:00Z", "a", "reserve", "J-001", "9.50"))
        self.assertFalse(money.check(self.b, e, NOW, "J-002", Decimal("0.92")).ok)

    def test_end_replaces_the_reserve(self):
        e = log(("2026-11-02T01:00:00Z", "a", "reserve", "J-001", "9.50"),
                ("2026-11-02T01:01:00Z", "a", "end", "J-001", "0.30"))
        self.assertEqual(money.spent(e, NOW, "J-001").day, Decimal("0.30"))

    def test_refusals_cost_nothing(self):
        e = log(("2026-11-02T01:00:00Z", "a", "refused", "J-001", "0"))
        self.assertEqual(money.spent(e, NOW, "J-001").month, Decimal("0"))

    def test_a_bad_log_line_refuses_everything(self):
        with self.assertRaises(money.BudgetConfigError):
            money.parse_log('{"ts": "2026-11-02T01:00:00Z", "run": "a", "phase": "spent", "usd": "1"}')
        with self.assertRaises(money.BudgetConfigError):
            money.parse_log("not json")
        with self.assertRaises(money.BudgetConfigError):
            money.parse_log('{"ts": "2026-11-02T01:00:00Z", "run": "a", "phase": "end", "usd": "-1"}')

    def test_input_over_the_call_limit_is_refused(self):
        with self.assertRaises(money.BudgetConfigError):
            money.worst_case(self.b, "review", 150_001)


class RepoConfig(unittest.TestCase):
    """The checked-in config files load and agree with each other."""

    def test_repo_files_load(self):
        try:
            import yaml
        except ImportError:
            self.skipTest("PyYAML not installed")
        root = HERE.parents[1]
        team = rules.team_from_config(yaml.safe_load((root / "work/agents.yml").read_text(encoding="utf-8")))
        self.assertEqual(team.agent_vendor.get("codex"), "openai")   # Codex reviews again (11-03), as the GitHub app
        b = money.budget_from_config(yaml.safe_load((root / "work/budget.yml").read_text(encoding="utf-8")))
        self.assertLessEqual(b.month, Decimal("100"))
        # The API agent is on (Cory, 11-03); its hard per-session budget must fit the job and day caps.
        self.assertLessEqual(b.orchestrator_session_usd, b.job)
        self.assertLessEqual(b.orchestrator_session_usd, b.day)
        self.assertGreater(b.orchestrator_session_usd, Decimal("0"))
        looks = yaml.safe_load((root / "work/looks/sets.yml").read_text(encoding="utf-8"))
        for s in looks["sets"].values():
            self.assertTrue((root / s["rubric"]).is_file(), s["rubric"])
            for job in rules.known_defect_jobs(s):   # J-068: a known defect names a job, J-NNN
                self.assertRegex(job, rules.JOB_TITLE.pattern + "$")


class EyeKnownDefect(unittest.TestCase):
    """J-068: a set's known defect, owned by an open job, blocks only that job's PR; other sets are unchanged."""

    LOOKS = {"artifact_prefix": "godot-audits-", "sets": {
        "approach": {"rubric": "approach.md", "frames": ["a.png"]},
        "ladder": {"rubric": "ladder.md", "known_defect": "J-063", "frames": ["l.png"]}}}

    def run_eye(self, verdicts: dict, pr_job: str, open_jobs=("J-063",)) -> tuple[str, str]:
        import tempfile
        tmp = pathlib.Path(tempfile.mkdtemp())
        for name in ("approach.md", "ladder.md"):
            (tmp / name).write_text("1. a rubric line\n", encoding="utf-8")
        gh = FakeGH()
        sw = switch.Switch.__new__(switch.Switch)
        sw.gh, sw.now, sw.root, sw.looks, sw.lines = gh, NOW, tmp, self.LOOKS, []
        sw.out = sw.lines.append
        sw._jobs = {j: {"number": 1, "title": f"{j} a job", "labels": []} for j in open_jobs}
        sw.frames = lambda run_id: {"a.png": b"\x89PNG", "l.png": b"\x89PNG"}
        sw.job_cap = lambda ctx: Decimal("1")
        answer = {"sets": [{"set": k, "verdict": v, "rubric_line": "1. a rubric line", "frame": "f.png",
                            "reason": "seen"} for k, v in verdicts.items()]}
        sw.paid = lambda *a, **k: claude_calls.Result(answer, "end_turn", "claude-opus-5-5", 1, 1)
        sw.eye(facts(number=9), {"job": pr_job, "run_id": 7, "proof_url": "https://example.invalid/run/7"})
        self.assertEqual(len(gh.posted), 1)
        body = gh.posted[0][1]
        return body.splitlines()[0], body

    def test_owner_jobs_pr_is_blocked_by_its_sets_fix(self):
        head, body = self.run_eye({"approach": "LAND", "ladder": "FIX"}, "J-063")
        self.assertTrue(head.endswith(": FIX**"), head)
        self.assertNotIn("known defect", body)

    def test_other_pr_is_shown_the_fix_but_not_blocked(self):
        head, body = self.run_eye({"approach": "LAND", "ladder": "FIX"}, "J-067")
        self.assertTrue(head.endswith(": LAND**"), head)
        self.assertIn("- ladder: FIX (known defect of J-063: shown, not blocking this PR)", body)
        got = v(body, T1, rules.BOT_LOGIN)   # the merge rule reads the posted comment as an eye LAND
        self.assertEqual((got.kind, got.verdict), ("eye", "LAND"))

    def test_closed_owner_job_makes_the_set_count_again(self):
        head, _ = self.run_eye({"approach": "LAND", "ladder": "FIX"}, "J-067", open_jobs=())
        self.assertTrue(head.endswith(": FIX**"), head)

    def test_set_without_the_field_is_unchanged(self):
        head, body = self.run_eye({"approach": "FIX", "ladder": "LAND"}, "J-067")
        self.assertTrue(head.endswith(": FIX**"), head)
        self.assertNotIn("known defect", body)
        head, _ = self.run_eye({"approach": "LAND", "ladder": "LAND"}, "J-067")
        self.assertTrue(head.endswith(": LAND**"), head)

    def test_an_unjudged_set_still_blocks(self):
        head, _ = self.run_eye({"approach": "LAND"}, "J-067")
        self.assertTrue(head.endswith(": FIX**"), head)


class SessionBudget(unittest.TestCase):
    def test_cents(self):
        import claude_calls
        self.assertEqual(claude_calls.cents(Decimal("2.00")), "200")
        self.assertEqual(claude_calls.cents(Decimal("1.259")), "125")
        with self.assertRaises(claude_calls.ClaudeCallError):
            claude_calls.cents(Decimal("0.004"))


class FakeGH:
    """The GitHub calls the watcher and orchestrate() make: reads from a table, writes recorded (printed in a dry run)."""

    def __init__(self, issues=None, comments=None, dry_run=False):
        self.repo, self.dry_run = "IssisX/ScraperX", dry_run
        self.issues, self.comments = issues or [], comments or {}
        self.gets, self.posted, self.log = [], [], []

    def get(self, path, accept=""):
        self.gets.append(path)
        if "/issues?" in path:
            return self.issues
        found = next((i for i in self.issues if path.endswith(f"/issues/{i['number']}")), None)
        if found is None:
            raise switch.GitHubError("GET", path, "gh: Not Found (HTTP 404)")
        return found

    def paged(self, path, key=""):
        return list(self.comments.get(int(path.split("/issues/")[1].split("/")[0]), []))

    def comment(self, number, body, what=""):
        if self.dry_run:
            self.log.append(f"DRY comment on #{number}")
        else:
            self.posted.append((number, body))

    def write(self, method, path, body=None, what=""):
        self.log.append(f"{method} {path}")


class FakeSessions:
    """The slice of client.beta.sessions the watcher uses; `states` maps a session id to what the API would say."""

    def __init__(self, states):
        self.states, self.sent, self.reads = states, [], []
        self.listing, self.pages, self.lists, self.next_fetches = [], None, [], 0
        self.events = types.SimpleNamespace(list=self._events, send=self._send)

    def list(self, agent_id, created_at_gte, limit, order):
        self.lists.append((agent_id, created_at_gte, limit, order))
        return self._page(0)

    def _page(self, i):
        pages = self.pages if self.pages is not None else [self.listing]

        def advance():
            self.next_fetches += 1
            return self._page(i + 1)
        return types_ns(data=list(pages[i]), has_next_page=lambda: i + 1 < len(pages), get_next_page=advance)

    def retrieve(self, session_id):
        self.reads.append(session_id)
        st = self.states[session_id]
        if isinstance(st, Exception):
            raise st
        cost = None if st.get("cents") is None else types.SimpleNamespace(amount=st["cents"], currency=st.get("cur", "USD"))
        return types.SimpleNamespace(status=st["status"], usage=types.SimpleNamespace(list_cost=cost))

    def _events(self, session_id, limit, order, types):
        assert (limit, order, types) == (1, "desc", ["session.status_idle"])
        st = self.states[session_id]
        if not st.get("stop"):
            return types_ns(data=[])
        reason = types_ns(type=st["stop"], event_ids=st.get("pending", []))
        return types_ns(data=[types_ns(id=st.get("idle", "sevt_idle1"), stop_reason=reason)])

    def _send(self, session_id, events):
        self.sent.append((session_id, events))


types_ns = types.SimpleNamespace


def fake_client(states):
    sessions = FakeSessions(states)
    return types.SimpleNamespace(beta=types.SimpleNamespace(sessions=sessions)), sessions


BOT = rules.BOT_LOGIN
SESSION = "sesn_011CZxAbc123Def456"
RUN = "a1b2c3d4e5f6"


def issue(number=138, title="J-031 Switchboard v0", labels=("job", "needs:orchestrator")):
    return {"number": number, "title": title, "labels": [{"name": n} for n in labels]}


def start_comment(number=138, session=SESSION, run=RUN, login=BOT):
    body = (f"Switchboard, 9:00 AM Central: orchestrator session `{session}` started with a $8.00 budget. Trace: x\n\n"
            f"{switch.MARK.format(kind='orchestrator', pr=number, head='-')}")
    if run:
        body += "\n" + switch.RUN_MARK.format(run=run)
    return {"body": body, "user": {"login": login}}


def make_switch(gh, states, ledger_lines=(), key="sk-ant-test"):
    sw = switch.Switch.__new__(switch.Switch)
    sw.gh, sw.now, sw.out = gh, NOW, lambda line: sw.lines.append(line)
    sw.lines, sw.appended = [], []
    sw.team = TEAM
    sw.budget = money.budget_from_config(dict(BUDGET, orchestrator={
        "enabled": True, "agent_id": "agent_1", "environment_id": "env_1", "session_usd": "8.00"}))
    sw.initial = list(ledger_lines)
    sw.entries = lambda: money.parse_log("\n".join(sw.initial + sw.appended))
    sw._claude, sw.sessions = fake_client(states)
    if not gh.dry_run:
        sw.append = lambda line, what: sw.appended.append(line)
    else:
        sw._ledger = ("", None)
    os.environ["ANTHROPIC_API_KEY"] = key
    return sw


def reserve_line(run=RUN, note="orchestrator#138", usd="8.00"):
    return money.record(NOW - dt.timedelta(hours=1), run, "reserve", "orchestrator", "J-031", Decimal(usd), note)


@unittest.skipUnless(HAVE_SDK, "anthropic not installed: claude_calls imports it for its error classes")
class SessionWatcher(unittest.TestCase):
    def tearDown(self):
        os.environ.pop("ANTHROPIC_API_KEY", None)

    def watch(self, states, comments=None, ledger=(reserve_line(),), gh=None, issues=None):
        gh = gh or FakeGH(issues or [issue()], {138: comments if comments is not None else [start_comment()]})
        sw = make_switch(gh, states, ledger)
        return sw, gh, sw.watch_sessions()

    def test_a_running_session_is_left_alone(self):
        sw, gh, failed = self.watch({SESSION: {"status": "running"}})
        self.assertEqual((failed, gh.posted, sw.appended, sw.sessions.sent), (0, [], [], []))

    def test_a_pause_is_denied_never_allowed_and_commented_once(self):
        paused = {"status": "idle", "stop": "requires_action", "pending": ["sevt_1", "sevt_2"], "idle": "sevt_idle7"}
        sw, gh, _ = self.watch({SESSION: paused})
        (sid, events), = sw.sessions.sent
        self.assertEqual(sid, SESSION)
        self.assertEqual([(e["type"], e["tool_use_id"], e["result"]) for e in events],
                         [("user.tool_confirmation", "sevt_1", "deny"), ("user.tool_confirmation", "sevt_2", "deny")])
        self.assertEqual(len(gh.posted), 1)
        self.assertIn(switch.MARK.format(kind="session-pause", pr=138, head="sevt_idle7"), gh.posted[0][1])
        # The next sweep sees the same pause (the deny not yet taken): denied again, but no second comment.
        again = [start_comment(), {"body": gh.posted[0][1], "user": {"login": BOT}}]
        sw2, gh2, _ = self.watch({SESSION: paused}, comments=again)
        self.assertEqual((len(sw2.sessions.sent), gh2.posted), (1, []))
        # A new pause is a new event id: a new comment.
        sw3, gh3, _ = self.watch({SESSION: dict(paused, idle="sevt_idle8")}, comments=again)
        self.assertEqual(len(gh3.posted), 1)

    def test_a_pause_comment_forged_by_someone_else_does_not_silence_the_bot(self):
        paused = {"status": "idle", "stop": "requires_action", "pending": ["sevt_1"], "idle": "sevt_idle7"}
        forged = {"body": switch.MARK.format(kind="session-pause", pr=138, head="sevt_idle7"), "user": {"login": "someone"}}
        _, gh, _ = self.watch({SESSION: paused}, comments=[start_comment(), forged])
        self.assertEqual(len(gh.posted), 1)

    def test_a_finished_session_ends_its_run_at_the_actual_cost_once(self):
        done = {"status": "idle", "stop": "end_turn", "cents": "137"}
        sw, gh, failed = self.watch({SESSION: done})
        self.assertEqual(failed, 0)
        (line,) = sw.appended
        got = money.parse_log(line)[0]
        self.assertEqual((got.run, got.phase, got.kind, got.job, got.usd), (RUN, "end", "orchestrator", "J-031", Decimal("1.37")))
        (number, body), = gh.posted
        self.assertEqual(number, 138)
        for want in ("$1.37", f"/sessions/{SESSION}", "finished its turn",
                     switch.MARK.format(kind="session-end", pr=138, head=SESSION)):
            self.assertIn(want, body)
        # The ledger now counts $1.37, not the $8.00 reserve.
        self.assertEqual(money.spent(money.parse_log("\n".join([reserve_line(), line])), NOW, "J-031").day, Decimal("1.37"))
        # Once reported, the next sweep reads nothing from the API.
        sw2, gh2, _ = self.watch({SESSION: done}, comments=[start_comment(), {"body": body, "user": {"login": BOT}}])
        self.assertEqual((sw2.sessions.reads, sw2.appended, gh2.posted), ([], [], []))

    def test_every_way_a_session_is_over_ends_the_run(self):
        for state, word in (({"status": "idle", "stop": "budget_reached", "cents": "801"}, "session budget"),
                            ({"status": "terminated", "cents": "42"}, "terminated"),
                            ({"status": "idle", "stop": "retries_exhausted", "cents": "5"}, "repeated errors"),
                            ({"status": "idle", "stop": "refusal", "cents": "5"}, "refusal")):
            sw, gh, _ = self.watch({SESSION: state})
            self.assertEqual(len(sw.appended), 1, state)
            self.assertIn(word, gh.posted[0][1])

    def test_no_reported_cost_keeps_the_reserve_and_says_nothing(self):
        sw, gh, _ = self.watch({SESSION: {"status": "idle", "stop": "end_turn", "cents": None}})
        self.assertEqual((sw.appended, gh.posted), ([], []))

    def test_an_end_already_on_the_ledger_is_not_written_twice(self):
        ended = money.record(NOW, RUN, "end", "orchestrator", "J-031", Decimal("1.37"))
        sw, gh, _ = self.watch({SESSION: {"status": "idle", "stop": "end_turn", "cents": "137"}}, ledger=(reserve_line(), ended))
        self.assertEqual(sw.appended, [])
        self.assertEqual(len(gh.posted), 1)   # the comment a failed earlier run never posted

    def test_a_session_from_before_the_run_id_rode_in_the_comment_is_found_by_its_ledger_note(self):
        old = start_comment(run="")
        other = reserve_line(run="ffffffffffff", note="orchestrator#139")
        sw, gh, _ = self.watch({SESSION: {"status": "idle", "stop": "end_turn", "cents": "10"}}, comments=[old],
                               ledger=(other, reserve_line()))
        self.assertEqual(money.parse_log(sw.appended[0])[0].run, RUN)
        sw, gh, _ = self.watch({SESSION: {"status": "idle", "stop": "end_turn", "cents": "10"}}, comments=[old], ledger=())
        self.assertEqual(sw.appended, [])   # no reserve to close: say so, invent nothing
        self.assertIn("No reserve", gh.posted[0][1])

    def test_only_the_bots_start_comment_binds_a_session(self):
        unknown = {"body": "orchestrator session `sesn_x` started\n" + switch.MARK.format(kind="orchestrator", pr=138, head="-"),
                   "user": {"login": BOT}}   # the no-answer comment names no session
        no_id = {"body": "no answer\n" + switch.MARK.format(kind="orchestrator", pr=138, head="-"), "user": {"login": BOT}}
        for comments in ([start_comment(login="someone")], [no_id], [{"body": "orchestrator session `" + SESSION + "`", "user": {"login": BOT}}], []):
            sw, gh, _ = self.watch({SESSION: {"status": "terminated", "cents": "1"}}, comments=comments)
            self.assertEqual((sw.sessions.reads, sw.appended, gh.posted), ([], [], []), comments)
        del unknown

    def test_one_sessions_trouble_never_stops_the_others(self):
        other = "sesn_other1"
        gh = FakeGH([issue(138), issue(140)], {138: [start_comment()], 140: [start_comment(140, other, "0123456789ab")]})
        boom = claude_calls.ClaudeCallError("session read answered HTTP 500")
        sw = make_switch(gh, {SESSION: boom, other: {"status": "idle", "stop": "end_turn", "cents": "7"}},
                         (reserve_line(), reserve_line("0123456789ab", "orchestrator#140")))
        self.assertEqual(sw.watch_sessions(), 1)
        self.assertEqual([n for n, _ in gh.posted], [140])
        self.assertTrue(any(l.startswith("ERROR       #138") for l in sw.lines))

    def test_a_reserve_whose_issue_closed_is_watched_and_ended_at_its_cost(self):
        # The $8.00 of #136 (J-031, 11-04): the open listing never shows a closed issue, so its reserve stood for ever.
        gh = FakeGH([dict(issue(136, "J-029 x"), state="closed")], {136: [start_comment(136)]})
        gh.get = lambda path, accept="", get=gh.get: (gh.gets.append(path), [])[1] if "/issues?" in path else get(path)
        sw = make_switch(gh, {SESSION: {"status": "terminated", "cents": "250"}}, (reserve_line(note="orchestrator#136"),))
        self.assertEqual(sw.watch_sessions(), 0)
        (line,) = sw.appended
        got = money.parse_log(line)[0]
        self.assertEqual((got.run, got.phase, got.usd), (RUN, "end", Decimal("2.50")))
        self.assertEqual([n for n, _ in gh.posted], [136])
        # Ended: the next sweep does not read that issue again.
        sw.initial, sw.appended, gh.gets = sw.initial + sw.appended, [], []
        sw.watch_sessions()
        self.assertEqual(gh.gets, ["repos/IssisX/ScraperX/issues?labels=job,needs:orchestrator&state=open&per_page=30"])

    def test_a_reserve_whose_issue_is_open_or_gone_is_not_touched_here(self):
        gh = FakeGH([dict(issue(140), state="open")], {140: [start_comment(140)]})
        gh.get = lambda path, accept="", get=gh.get: (gh.gets.append(path), [])[1] if "/issues?" in path else get(path)
        sw = make_switch(gh, {SESSION: {"status": "terminated", "cents": "250"}},
                         (reserve_line(note="orchestrator#140"), reserve_line("0123456789ab", "orchestrator#141")))
        self.assertEqual(sw.watch_sessions(), 0)   # #141 answers 404: a note, not a failure
        self.assertEqual((sw.appended, gh.posted, sw.sessions.reads), ([], [], []))
        self.assertTrue(any("#141 holds a reserve but the issue is gone" in l for l in sw.lines))

    def test_the_issue_list_is_one_bounded_page_of_open_job_issues_and_skips_prs(self):
        gh = FakeGH([dict(issue(5), pull_request={})], {})
        sw = make_switch(gh, {})
        sw.watch_sessions()
        self.assertEqual(gh.gets, ["repos/IssisX/ScraperX/issues?labels=job,needs:orchestrator&state=open&per_page=30"])
        self.assertEqual(sw.lines, [])

    def unknown_comment(self, minutes_ago, run=RUN, number=138):
        at = (NOW - dt.timedelta(minutes=minutes_ago)).strftime("%Y-%m-%dT%H:%M:%SZ")
        body = (f"Switchboard: `beta.sessions.create` answered HTTP 500 (InternalServerError), so a session may exist.\n\n"
                f"{switch.MARK.format(kind='orchestrator', pr=number, head='-')}\n{switch.UNKNOWN_MARK.format(run=run, at=at)}")
        return {"body": body, "user": {"login": BOT}}

    def reconcile(self, comments, listing=(), ledger=(reserve_line(),), pages=None):
        gh = FakeGH([issue()], {138: comments})
        sw = make_switch(gh, {}, ledger)
        sw.sessions.listing = list(listing)
        if pages is not None:
            sw.sessions.pages = pages
        return sw, gh, sw.watch_sessions()

    def test_an_unknown_start_is_found_by_its_metadata_and_watched_as_usual(self):
        mine = types_ns(id=SESSION, metadata={"switchboard_key": "orchestrator#138", "run": RUN})
        decoys = [types_ns(id="sesn_other", metadata={"switchboard_key": "orchestrator#138", "run": "ffffffffffff"}),
                  types_ns(id="sesn_other2", metadata={"switchboard_key": "orchestrator#139", "run": RUN}),
                  types_ns(id="sesn_other3", metadata={})]
        sw, gh, failed = self.reconcile([self.unknown_comment(10)], decoys + [mine])
        self.assertEqual(failed, 0)
        at = (NOW - dt.timedelta(minutes=12)).strftime("%Y-%m-%dT%H:%M:%SZ")
        self.assertEqual(sw.sessions.lists, [("agent_1", at, 20, "desc")])   # one bounded page, newest first
        (number, body), = gh.posted
        self.assertIn("found after an unanswered start", body)
        self.assertEqual(sw.start_comment([{"body": body, "user": {"login": BOT}}], 138), (SESSION, RUN))
        self.assertEqual(sw.appended, [])
        # With that comment on the issue the next sweep reads the session like any other.
        sw2, gh2, _ = self.watch({SESSION: {"status": "running"}}, comments=[self.unknown_comment(10), {"body": body, "user": {"login": BOT}}])
        self.assertEqual(sw2.sessions.reads, [SESSION])

    def test_a_match_on_the_second_page_is_found(self):
        mine = types_ns(id=SESSION, metadata={"switchboard_key": "orchestrator#138", "run": RUN})
        newer = [types_ns(id=f"sesn_n{i}", metadata={"run": "ffffffffffff"}) for i in range(20)]
        sw, gh, _ = self.reconcile([self.unknown_comment(40)], pages=[newer, [mine]])
        self.assertEqual((sw.sessions.next_fetches, sw.appended), (1, []))   # 40 minutes old, but it exists: not written off
        self.assertEqual(sw.start_comment([{"body": gh.posted[0][1], "user": {"login": BOT}}], 138), (SESSION, RUN))

    def test_not_found_counts_only_after_every_page_was_read(self):
        newer = [types_ns(id=f"sesn_n{i}", metadata={}) for i in range(20)]
        sw, gh, _ = self.reconcile([self.unknown_comment(40)], pages=[newer, newer, newer])
        self.assertEqual(sw.sessions.next_fetches, 2)
        self.assertEqual(money.parse_log(sw.appended[0])[0].usd, Decimal("0"))   # exhausted: written off

    def test_hitting_the_page_cap_keeps_the_reserve_and_says_unresolved_once(self):
        newer = [types_ns(id=f"sesn_n{i}", metadata={}) for i in range(20)]
        unknown = self.unknown_comment(40)
        sw, gh, _ = self.reconcile([unknown], pages=[newer] * 25)
        self.assertEqual(sw.sessions.next_fetches, claude_calls.MAX_LIST_PAGES - 1)   # ten pages read, no more
        self.assertEqual(sw.appended, [])                                              # no $0 end
        (number, body), = gh.posted
        self.assertIn(switch.MARK.format(kind="orchestrator-unresolved", pr=138, head=RUN), body)
        self.assertNotIn("orchestrator-cleared", body)
        self.assertTrue(sw.start_blocked([unknown, {"body": body, "user": {"login": BOT}}], 138))   # still no second session
        sw2, gh2, _ = self.reconcile([unknown, {"body": body, "user": {"login": BOT}}], pages=[newer] * 25)
        self.assertEqual((gh2.posted, sw2.appended), ([], []))                         # the note is not repeated

    def test_an_unknown_start_not_found_yet_waits(self):
        sw, gh, _ = self.reconcile([self.unknown_comment(29)])
        self.assertEqual((gh.posted, sw.appended, len(sw.sessions.lists)), ([], [], 1))

    def test_an_unknown_start_not_found_after_30_minutes_is_written_off_and_can_be_retried(self):
        unknown = self.unknown_comment(31)
        sw, gh, _ = self.reconcile([unknown])
        end = money.parse_log(sw.appended[0])[0]
        self.assertEqual((end.run, end.phase, end.usd), (RUN, "end", Decimal("0")))
        (number, body), = gh.posted
        self.assertIn(switch.MARK.format(kind="orchestrator-cleared", pr=138, head=RUN), body)
        self.assertIn("Re-add `needs:orchestrator`", body)
        cleared = {"body": body, "user": {"login": BOT}}
        self.assertTrue(sw.start_blocked([unknown], 138))
        self.assertFalse(sw.start_blocked([unknown, cleared], 138))               # a relabel may start again
        self.assertTrue(sw.start_blocked([unknown, cleared, start_comment(run="0123456789ab")], 138))   # and then blocks again
        sw2, gh2, _ = self.reconcile([unknown, cleared])
        self.assertEqual((sw2.sessions.lists, sw2.appended, gh2.posted), ([], [], []))   # done: nothing read or written
        # A forged clear from someone else does not lift the block.
        self.assertTrue(sw.start_blocked([unknown, dict(cleared, user={"login": "someone"})], 138))

    def test_writing_off_does_not_double_an_end_already_on_the_ledger(self):
        ended = money.record(NOW, RUN, "end", "orchestrator", "J-031", Decimal("0"))
        sw, gh, _ = self.reconcile([self.unknown_comment(45)], ledger=(reserve_line(), ended))
        self.assertEqual((sw.appended, len(gh.posted)), ([], 1))

    def test_the_newest_unknown_start_is_the_one_reconciled(self):
        old_run = "0123456789ab"
        old = self.unknown_comment(300, run=old_run)
        cleared = {"body": switch.MARK.format(kind="orchestrator-cleared", pr=138, head=old_run), "user": {"login": BOT}}
        sw, gh, _ = self.reconcile([old, cleared, self.unknown_comment(5)])
        self.assertEqual((gh.posted, sw.appended, len(sw.sessions.lists)), ([], [], 1))

    def test_a_forged_unknown_comment_is_ignored(self):
        forged = dict(self.unknown_comment(60), user={"login": "someone"})
        sw, gh, _ = self.reconcile([forged])
        self.assertEqual((sw.sessions.lists, gh.posted, sw.appended), ([], [], []))

    def test_a_dry_run_acts_on_nothing(self):
        gh = FakeGH([issue()], {138: [start_comment()]}, dry_run=True)
        sw = make_switch(gh, {SESSION: {"status": "idle", "stop": "requires_action", "pending": ["sevt_1"]}}, (reserve_line(),))
        sw.watch_sessions()
        self.assertEqual((sw.sessions.sent, gh.posted, gh.log), ([], [], ["DRY comment on #138"]))
        self.assertTrue(any(l.startswith("DRY  deny 1") for l in sw.lines))
        gh = FakeGH([issue()], {138: [start_comment()]}, dry_run=True)
        sw = make_switch(gh, {SESSION: {"status": "idle", "stop": "end_turn", "cents": "137"}}, (reserve_line(),))
        sw.watch_sessions()
        self.assertEqual((gh.posted, gh.log), ([], ["DRY comment on #138"]))
        self.assertTrue(any(l.startswith("DRY  append") for l in sw.lines))
        # Without a key a dry run does not touch the API at all.
        sw = make_switch(gh, {}, (reserve_line(),), key="")
        os.environ.pop("ANTHROPIC_API_KEY")
        sw.watch_sessions()
        self.assertTrue(any("would read session" in l for l in sw.lines))


class StartFailures(unittest.TestCase):
    def setUp(self):
        os.environ["AGENT_GITHUB_TOKEN"] = "ghp_test"
        self.addCleanup(os.environ.pop, "AGENT_GITHUB_TOKEN", None)

    def orchestrate(self, outcome):
        gh = FakeGH([dict(issue(138), body="do it")], {138: []})
        sw = make_switch(gh, {})
        self.addCleanup(os.environ.pop, "ANTHROPIC_API_KEY", None)

        self.start_args = []

        def start(*args, **kw):
            self.start_args.append(args)
            if isinstance(outcome, Exception):
                raise outcome
            return outcome
        original, claude_calls.start_session = claude_calls.start_session, start
        self.addCleanup(setattr, claude_calls, "start_session", original)
        sw.orchestrate(138)
        return sw, gh

    def test_an_http_status_error_releases_the_reserve_and_names_class_and_status_only(self):
        sw, gh = self.orchestrate(claude_calls.SessionStartRejected("beta.sessions.create answered HTTP 400", 400, "BadRequestError"))
        reserve, end = (money.parse_log(l)[0] for l in sw.appended)
        self.assertEqual((reserve.phase, reserve.usd, end.phase, end.usd, end.run), ("reserve", Decimal("8.00"), "end", Decimal("0"), reserve.run))
        self.assertEqual(money.spent(money.parse_log("\n".join(sw.appended)), NOW, "J-031").day, Decimal("0"))
        (number, body), = gh.posted
        self.assertIn("HTTP 400 (BadRequestError)", body)
        self.assertNotIn(switch.MARK.format(kind="orchestrator", pr=138, head="-"), body)   # a retry is allowed
        self.assertIn("orchestrator-failed", body)

    def test_a_local_refusal_before_any_request_also_releases_the_reserve(self):
        sw, gh = self.orchestrate(claude_calls.ClaudeCallError("session budget 0.004 is under one cent"))
        self.assertEqual(money.parse_log(sw.appended[1])[0].usd, Decimal("0"))
        self.assertIn("under one cent", gh.posted[0][1])

    def test_no_answer_keeps_the_reserve_and_blocks_a_second_session(self):
        sw, gh = self.orchestrate(claude_calls.SessionStartUnknown("beta.sessions.create could not connect (x)", ["APIConnectionError", "ConnectError"]))
        self.assertEqual([money.parse_log(l)[0].phase for l in sw.appended], ["reserve"])
        body = gh.posted[0][1]
        self.assertIn("APIConnectionError, ConnectError", body)
        self.assertIn(switch.MARK.format(kind="orchestrator", pr=138, head="-"), body)
        self.assertIsNone(sw.start_comment([{"body": body, "user": {"login": BOT}}], 138))   # nothing for the watcher to bind
        run = money.parse_log(sw.appended[0])[0].run
        self.assertIn(switch.UNKNOWN_MARK.format(run=run, at="2026-11-02T18:00:00Z"), body)   # what the watcher reconciles
        self.assertEqual(self.start_args[0][-1], {"switchboard_key": "orchestrator#138", "run": run})   # what it matches on

    def test_a_5xx_keeps_the_reserve_and_blocks_a_retry_like_no_answer(self):
        sw, gh = self.orchestrate(claude_calls.SessionStartUnknown("beta.sessions.create answered HTTP 500", ["InternalServerError"], 500))
        self.assertEqual([money.parse_log(l)[0].phase for l in sw.appended], ["reserve"])   # no $0 end
        body = gh.posted[0][1]
        self.assertIn("answered HTTP 500 (InternalServerError)", body)
        self.assertIn(switch.MARK.format(kind="orchestrator", pr=138, head="-"), body)       # a re-label finds it handled
        self.assertNotIn("orchestrator-failed", body)
        self.assertIn("switchboard:orchestrator-unknown run=", body)

    def test_a_started_session_carries_its_run_id_to_the_watcher(self):
        sw, gh = self.orchestrate(SESSION)
        run = money.parse_log(sw.appended[0])[0].run
        found = sw.start_comment([{"body": gh.posted[0][1], "user": {"login": BOT}}], 138)
        self.assertEqual(found, (SESSION, run))


@unittest.skipUnless(HAVE_SDK, "anthropic and httpx2 not installed")
class SdkRequests(unittest.TestCase):
    """The watcher's and the start's requests, rendered through the pinned SDK onto a mock transport."""

    def client(self, handler):
        import anthropic
        return anthropic.Anthropic(api_key="sk-ant-test", max_retries=0,
                                   http_client=httpx2.Client(transport=httpx2.MockTransport(handler)))

    def test_read_a_paused_session_and_deny_each_pending_call(self):
        seen = []

        def handler(req):
            seen.append((req.method, req.url.path, req.url.query.decode(), req.content.decode()))
            if req.url.path.endswith("/events") and req.method == "GET":
                return httpx2.Response(200, json={"data": [{"id": "sevt_idle1", "type": "session.status_idle", "processed_at": "2026-11-02T18:00:00Z",
                                                            "stop_reason": {"type": "requires_action", "event_ids": ["sevt_1", "sevt_2"]}}], "next_page": None})
            if req.url.path.endswith("/events"):
                return httpx2.Response(200, json={"data": []})
            return httpx2.Response(200, json={"id": SESSION, "status": "idle", "usage": {"list_cost": {"amount": "137", "currency": "USD"}}})
        c = self.client(handler)
        st = claude_calls.session_state(c, SESSION)
        self.assertEqual(st, claude_calls.SessionState("idle", "requires_action", ("sevt_1", "sevt_2"), "sevt_idle1", 137))
        self.assertEqual(claude_calls.deny_pending(c, SESSION, st.pending), 2)
        sent = json.loads(seen[-1][3])["events"]
        self.assertEqual([(e["type"], e["tool_use_id"], e["result"]) for e in sent],
                         [("user.tool_confirmation", "sevt_1", "deny"), ("user.tool_confirmation", "sevt_2", "deny")])
        self.assertIn("limit=1", seen[1][2])
        self.assertIn("order=desc", seen[1][2])
        self.assertIn("session.status_idle", seen[1][2])

    def test_create_carries_the_metadata_and_find_session_reads_a_bounded_page(self):
        seen = []

        def handler(req):
            seen.append((req.method, req.url.path, req.url.query.decode(), req.content.decode()))
            if req.method == "GET":
                return httpx2.Response(200, json={"data": [
                    {"id": "sesn_other", "metadata": {"switchboard_key": "orchestrator#138", "run": "ffffffffffff"}},
                    {"id": SESSION, "metadata": {"switchboard_key": "orchestrator#138", "run": RUN}}], "next_page": None})
            return httpx2.Response(200, json={"id": SESSION})
        c = self.client(handler)
        args = ("agent_1", "env_1", "J-031 #138", "go", Decimal("8.00"), "https://github.com/IssisX/ScraperX", "ghp_test", "ScraperX-Claude", "")
        self.assertEqual(claude_calls.start_session(c, *args, {"switchboard_key": "orchestrator#138", "run": RUN}), SESSION)
        self.assertEqual(json.loads(seen[0][3])["metadata"], {"switchboard_key": "orchestrator#138", "run": RUN})
        self.assertEqual(claude_calls.find_session(c, "agent_1", "2026-11-03T14:00:00Z", {"switchboard_key": "orchestrator#138", "run": RUN}),
                         claude_calls.Search(SESSION, False))
        self.assertEqual(claude_calls.find_session(c, "agent_1", "2026-11-03T14:00:00Z", {"run": "000000000000"}),
                         claude_calls.Search(None, True))   # the only page, no next: exhausted
        method, path, query, body = seen[1]
        self.assertEqual((method, path, body), ("GET", "/v1/sessions", ""))
        for want in ("agent_id=agent_1", "created_at%5Bgte%5D=2026-11-03T14%3A00%3A00Z", "limit=20", "order=desc"):
            self.assertIn(want, query)

    def test_find_session_follows_the_cursor_to_the_next_page_with_the_same_filters(self):
        seen = []

        def handler(req):
            seen.append(req.url.query.decode())
            if "page=p2" in req.url.query.decode():
                return httpx2.Response(200, json={"data": [{"id": SESSION, "metadata": {"run": RUN}}], "next_page": None})
            return httpx2.Response(200, json={"data": [{"id": f"sesn_n{i}", "metadata": {}} for i in range(20)], "next_page": "p2"})
        got = claude_calls.find_session(self.client(handler), "agent_1", "2026-11-03T14:00:00Z", {"run": RUN})
        self.assertEqual(got, claude_calls.Search(SESSION, False))
        self.assertEqual(len(seen), 2)
        for q in seen:
            for want in ("agent_id=agent_1", "created_at%5Bgte%5D=2026-11-03T14%3A00%3A00Z", "limit=20", "order=desc"):
                self.assertIn(want, q)
        self.assertIn("page=p2", seen[1])

    def test_find_session_stops_at_the_page_cap_and_says_it_did_not_finish(self):
        seen = []

        def endless(req):
            seen.append(req.url.query.decode())
            return httpx2.Response(200, json={"data": [{"id": "sesn_x", "metadata": {}}], "next_page": f"p{len(seen) + 1}"})
        got = claude_calls.find_session(self.client(endless), "agent_1", "2026-11-03T14:00:00Z", {"run": RUN})
        self.assertEqual(got, claude_calls.Search(None, False))
        self.assertEqual(len(seen), claude_calls.MAX_LIST_PAGES)

    def test_a_session_id_is_checked_before_it_reaches_a_url(self):
        with self.assertRaises(claude_calls.ClaudeCallError):
            claude_calls.session_state(self.client(lambda r: httpx2.Response(200, json={})), "sesn_x/../agents")

    def test_start_status_error_and_connection_error_are_told_apart(self):
        import anthropic

        def status(code):
            def reject(req):
                return httpx2.Response(code, json={"type": "error", "error": {"type": "api_error", "message": "secret-ish text"}},
                                       headers={"x-api-key": "sk-ant-leak"})
            return reject
        args = ("agent_1", "env_1", "J-031 #138", "go", Decimal("8.00"), "https://github.com/IssisX/ScraperX", "ghp_test", "ScraperX-Claude", "")
        # A 4xx proves the server refused the request: no session exists.
        for code, name in ((400, "BadRequestError"), (401, "AuthenticationError"), (404, "NotFoundError"), (429, "RateLimitError")):
            with self.assertRaises(claude_calls.SessionStartRejected, msg=code) as rej:
                claude_calls.start_session(self.client(status(code)), *args)
            self.assertEqual((rej.exception.status, rej.exception.error_class), (code, name))
        # A 5xx proves nothing: the work may have been done before the answer failed.
        for code in (500, 502, 529):
            with self.assertRaises(claude_calls.SessionStartUnknown, msg=code) as unk5:
                claude_calls.start_session(self.client(status(code)), *args)
            self.assertEqual(unk5.exception.status, code)
            self.assertNotIn("secret-ish", str(unk5.exception))

        def drop(req):
            raise httpx2.ConnectError("boom", request=req)
        with self.assertRaises(claude_calls.SessionStartUnknown) as unk:
            claude_calls.start_session(self.client(drop), *args)
        self.assertIn("APIConnectionError", unk.exception.classes)
        self.assertIsNone(unk.exception.status)
        self.assertIsInstance(unk.exception.__cause__, anthropic.APIConnectionError)


def sse(*events: dict) -> bytes:
    return b"".join(f"event: {e['type']}\ndata: {json.dumps(e)}\n\n".encode() for e in events)


def streamed_answer(text: str, stop: str = "end_turn", thinking: str = "") -> bytes:
    """A Messages API stream as the server sends it: an optional thinking block, then one text block."""
    events = [{"type": "message_start", "message": {
        "id": "msg_1", "type": "message", "role": "assistant", "model": "claude-opus-5-5", "content": [],
        "stop_reason": None, "stop_sequence": None, "usage": {"input_tokens": 1200, "output_tokens": 1}}}]
    i = 0
    if thinking:
        events += [{"type": "content_block_start", "index": 0, "content_block": {"type": "thinking", "thinking": "", "signature": ""}},
                   {"type": "content_block_delta", "index": 0, "delta": {"type": "thinking_delta", "thinking": thinking}},
                   {"type": "content_block_delta", "index": 0, "delta": {"type": "signature_delta", "signature": "sig"}},
                   {"type": "content_block_stop", "index": 0}]
        i = 1
    if text:
        events += [{"type": "content_block_start", "index": i, "content_block": {"type": "text", "text": ""}}]
        events += [{"type": "content_block_delta", "index": i, "delta": {"type": "text_delta", "text": text[k:k + 7]}}
                   for k in range(0, len(text), 7)]
        events += [{"type": "content_block_stop", "index": i}]
    events += [{"type": "message_delta", "delta": {"stop_reason": stop, "stop_sequence": None}, "usage": {"output_tokens": 4321}},
               {"type": "message_stop"}]
    return sse(*events)


@unittest.skipUnless(HAVE_SDK, "anthropic and httpx2 not installed")
class StreamedAsk(unittest.TestCase):
    """The review and the eye stream (Cory, 11-04: Opus at max effort; a long think must not time out)."""

    SCHEMA = {"type": "object", "properties": {"verdict": {"type": "string"}}, "required": ["verdict"], "additionalProperties": False}

    def client(self, handler):
        import anthropic
        return anthropic.Anthropic(api_key="sk-ant-test", max_retries=0,
                                   http_client=httpx2.Client(transport=httpx2.MockTransport(handler)))

    def ask(self, handler):
        return claude_calls.ask_json(self.client(handler), "claude-opus-5-5", 64000, "max", "sys",
                                     [claude_calls.text_block("diff")], self.SCHEMA)

    def test_the_request_streams_and_keeps_every_parameter(self):
        seen = []

        def handler(req):
            seen.append(json.loads(req.content.decode()))
            return httpx2.Response(200, headers={"content-type": "text/event-stream"},
                                   content=streamed_answer('{"verdict": "LAND"}', thinking="long think"))
        res = self.ask(handler)
        self.assertEqual(res, claude_calls.Result({"verdict": "LAND"}, "end_turn", "claude-opus-5-5", 1200, 4321))
        body = seen[0]
        self.assertIs(body["stream"], True)
        self.assertEqual((body["model"], body["max_tokens"], body["system"]), ("claude-opus-5-5", 64000, "sys"))
        self.assertEqual(body["messages"], [{"role": "user", "content": [{"type": "text", "text": "diff"}]}])
        self.assertEqual(body["output_config"], {"effort": "max", "format": {"type": "json_schema", "schema": self.SCHEMA}})
        self.assertNotIn("thinking", body)   # adaptive is the only mode on claude-opus-5-5: left unset

    def test_a_think_that_hits_max_tokens_gives_no_verdict_but_its_usage(self):
        def handler(req):
            return httpx2.Response(200, headers={"content-type": "text/event-stream"},
                                   content=streamed_answer("", stop="max_tokens", thinking="x" * 50))
        self.assertEqual(self.ask(handler), claude_calls.Result(None, "max_tokens", "claude-opus-5-5", 1200, 4321))

    def test_a_status_error_before_the_stream_is_a_call_error(self):
        for code in (400, 429, 529):
            def reject(req, code=code):
                return httpx2.Response(code, json={"type": "error", "error": {"type": "api_error", "message": "no"}})
            with self.assertRaises(claude_calls.ClaudeCallError, msg=code) as err:
                self.ask(reject)
            self.assertIn(f"HTTP {code}" if code != 429 else "rate-limited", str(err.exception))

    def test_an_error_event_mid_stream_is_a_call_error(self):
        def handler(req):
            body = streamed_answer('{"verdict": "LAND"}')
            cut = body.index(b"event: message_delta")
            return httpx2.Response(200, headers={"content-type": "text/event-stream"}, content=body[:cut] + sse(
                {"type": "error", "error": {"type": "overloaded_error", "message": "Overloaded"}}))
        with self.assertRaises(claude_calls.ClaudeCallError) as err:
            self.ask(handler)
        self.assertIn("messages.stream(claude-opus-5-5) answered HTTP", str(err.exception))

    def test_a_stream_cut_off_mid_answer_is_a_call_error(self):
        def handler(req):
            body = streamed_answer('{"verdict": "LAND"}', thinking="long think")

            def chunks():
                yield body[:200]
                raise httpx2.ReadTimeout("timed out")
            return httpx2.Response(200, headers={"content-type": "text/event-stream"}, content=chunks())
        with self.assertRaises(claude_calls.ClaudeCallError) as err:
            self.ask(handler)
        self.assertIn("cut off mid-answer (ReadTimeout)", str(err.exception))

    def test_a_connection_error_is_a_call_error(self):
        def drop(req):
            raise httpx2.ConnectError("boom", request=req)
        with self.assertRaises(claude_calls.ClaudeCallError) as err:
            self.ask(drop)
        self.assertIn("could not connect", str(err.exception))

    def test_the_client_waits_long_enough_for_a_streamed_answer(self):
        self.assertEqual(claude_calls.REQUEST_TIMEOUT_S, 900.0)
        self.assertLess(claude_calls.REQUEST_TIMEOUT_S, 60 * 60)   # the ask job's timeout-minutes


class FakeStreamClient:
    """A stand-in client whose messages.stream is a context manager, for callers that do not need the SDK."""

    def __init__(self, final):
        self.final, self.calls = final, []
        self.messages = types.SimpleNamespace(stream=self.stream)

    def stream(self, **kw):
        self.calls.append(kw)
        client = self

        class Manager:
            def __enter__(self):
                return types.SimpleNamespace(get_final_message=lambda: client.final)

            def __exit__(self, *exc):
                return False
        return Manager()


@unittest.skipUnless(HAVE_SDK, "anthropic not installed: claude_calls imports it for its error classes")
class StreamedAskFake(unittest.TestCase):
    def test_ask_json_reads_the_final_message_from_the_stream(self):
        final = types.SimpleNamespace(stop_reason="end_turn", model="claude-opus-5-5",
                                      content=[types.SimpleNamespace(type="thinking"), types.SimpleNamespace(type="text", text='{"ok": 1}')],
                                      usage=types.SimpleNamespace(input_tokens=10, output_tokens=20))
        c = FakeStreamClient(final)
        res = claude_calls.ask_json(c, "claude-opus-5-5", 8000, "medium", "sys", [], {"type": "object"})
        self.assertEqual(res, claude_calls.Result({"ok": 1}, "end_turn", "claude-opus-5-5", 10, 20))
        self.assertEqual(c.calls[0]["output_config"], {"effort": "medium", "format": {"type": "json_schema", "schema": {"type": "object"}}})
        self.assertEqual(c.calls[0]["max_tokens"], 8000)


if __name__ == "__main__":
    unittest.main()


class ReviewPool(unittest.TestCase):
    """Cory, 11-03: one reviewer per PR, whoever is available, the work spread around."""

    POOL = rules.team_from_config(dict(AGENTS, review=dict(AGENTS["review"], pool=["claude-api", "muse", "chatgpt"])))
    API = rules.API_REVIEWER

    def test_rotation_order_differs_between_prs(self):
        firsts = {rules.review_rotation(n, "xai", frozenset(), self.POOL)[0] for n in (30, 31, 32)}
        self.assertEqual(firsts, {self.API, "muse", "chatgpt"})

    def test_rotation_skips_the_authors_vendor_and_keeps_claude_cross_model(self):
        for n in range(6):
            self.assertNotIn("muse", rules.review_rotation(n, "meta", frozenset(), self.POOL))
            self.assertEqual(sorted(rules.review_rotation(n, "anthropic", frozenset(), self.POOL)), ["chatgpt", "muse"])
        self.assertEqual(rules.review_rotation(5, "unknown", frozenset(), self.POOL), [self.API])

    def test_api_out_of_budget_is_not_available(self):
        self.assertNotIn(self.API, rules.review_rotation(30, "xai", frozenset(), self.POOL, api_ok=False))
        self.assertEqual(rules.review_rotation(30, "unknown", frozenset(), self.POOL, api_ok=False), [])

    def test_matron_job_starts_at_its_specialist(self):
        self.assertEqual(rules.review_rotation(30, "openai", frozenset({"matron"}), self.POOL)[0], "muse")

    def test_the_free_reviewer_takes_it(self):
        rot, now, ch = ["muse", "chatgpt"], "2026-11-02T15:00:00Z", rules.choose_reviewer
        self.assertEqual(ch(rot, {}, now, 1, {}, {}), "muse")                                  # all free: rotation order
        self.assertEqual(ch(rot, {}, now, 1, {"muse": 2}, {}), "chatgpt")                      # Muse is busy
        self.assertEqual(ch(rot, {}, now, 1, {}, {"muse": 3, "chatgpt": 1}), "chatgpt")        # spread the day's work
        self.assertEqual(ch([self.API, "muse"], {}, now, 1, {"muse": 1}, {self.API: 1}), self.API)  # the API is never busy
        self.assertEqual(ch([self.API, "muse"], {}, now, 1, {}, {self.API: 2}), "muse")

    def test_the_one_asked_keeps_it_until_quiet_too_long(self):
        rot, now, ch = ["muse", "chatgpt"], "2026-11-02T15:00:00Z", rules.choose_reviewer
        self.assertEqual(ch(rot, {"chatgpt": "2026-11-02T14:30:00Z"}, now, 1, {"chatgpt": 5}, {}), "chatgpt")
        self.assertEqual(ch(rot, {"chatgpt": "2026-11-02T13:30:00Z"}, now, 1, {"muse": 5}, {}), "muse")
        self.assertEqual(ch(rot, {"muse": "2026-11-02T12:00:00Z", "chatgpt": "2026-11-02T13:00:00Z"}, now, 1, {}, {}), "")

    def test_pool_config_is_checked(self):
        for pool, word in ((["claude"], "Claude agent"), (["nobody"], "unknown agent"), (["muse", "muse"], "twice")):
            with self.assertRaisesRegex(ValueError, word):
                rules.team_from_config(dict(AGENTS, review=dict(AGENTS["review"], pool=pool)))

    def test_the_repo_file_sets_the_pool(self):
        try:
            import yaml
        except ImportError:
            self.skipTest("PyYAML not installed")
        path = pathlib.Path(__file__).resolve().parents[2] / "work/agents.yml"
        team = rules.team_from_config(yaml.safe_load(path.read_text(encoding="utf-8")))
        self.assertEqual(tuple(team.pool), (self.API,))   # Codex reviews on open by itself; the API takes what it leaves


class RepoReviewModel(unittest.TestCase):
    def test_the_api_review_is_opus_high_and_the_security_pass_max(self):
        try:
            import yaml
        except ImportError:
            self.skipTest("PyYAML not installed")
        root = HERE.parents[1]
        b = money.budget_from_config(yaml.safe_load((root / "work/budget.yml").read_text(encoding="utf-8")))
        spec = b.calls["review"]
        self.assertEqual((spec.model, spec.effort, spec.max_tokens), ("claude-opus-5-5", "high", 64000))   # Cory, 11-08
        sec = b.calls["security_review"]   # the workflow-file PR's security pass keeps max (Cory, 11-08: "split")
        self.assertEqual((sec.model, sec.effort, sec.max_tokens), ("claude-opus-5-5", "max", 128000))
        self.assertTrue(money.check(b, [], NOW, "J-1", money.worst_case(b, "security_review", sec.max_input_tokens)).ok)
        # The 64000 reserve passes the caps: the worst case at the full input limit fits one job's cap.
        worst = money.worst_case(b, "review", spec.max_input_tokens)
        self.assertEqual(worst, Decimal("1.880000"))   # 150,000 x $4/MTok + 64,000 x $20/MTok
        self.assertTrue(money.check(b, [], dt.datetime(2026, 11, 4, tzinfo=dt.timezone.utc), "J-031", worst).ok)


class CodexState(unittest.TestCase):
    """The Codex app's summary comment is its verdict on the commit it names (Cory, 11-03)."""

    def body(self, sha="2e6a845e068fbbe39b440d9c818d5a289d5f0084", status="completed", unresolved="false"):
        return ("<!-- codex-pull-request-review-summary -->\n<!-- codex-review-state:v1 &#123;\"code_review\":&#123;"
                f"\"commit_sha\":\"{sha}\",\"has_any_findings\":{unresolved},\"has_unresolved_findings\":{unresolved},"
                f"\"status\":\"{status}\"&#125;,\"security_review\":&#123;\"commit_sha\":null,\"status\":null&#125;&#125; -->\n\n## Codex Review Summary")

    def test_clean_review_is_a_land_on_its_commit(self):
        v = rules.parse_codex_state(self.body(), T1, rules.CODEX_LOGIN)
        self.assertEqual((v.vendor, v.who, v.kind, v.verdict, v.sha), ("openai", "Codex", "review", "LAND",
                                                                        "2e6a845e068fbbe39b440d9c818d5a289d5f0084"))

    def test_findings_hold_the_merge(self):
        self.assertEqual(rules.parse_codex_state(self.body(unresolved="true"), T1, rules.CODEX_LOGIN).verdict, "FIX")

    def test_running_unknown_or_forged_is_no_verdict(self):
        self.assertIsNone(rules.parse_codex_state(self.body(status="running"), T1, rules.CODEX_LOGIN))
        self.assertIsNone(rules.parse_codex_state(self.body(unresolved="null"), T1, rules.CODEX_LOGIN))
        self.assertIsNone(rules.parse_codex_state(self.body(), T1, "IssisX"))
        self.assertIsNone(rules.parse_codex_state(self.body(sha="not-a-sha"), T1, rules.CODEX_LOGIN))

    def test_codex_land_counts_for_claude_work_not_for_chatgpt_work(self):
        land = rules.parse_codex_state(self.body(sha=HEAD), T1, rules.CODEX_LOGIN)
        pr = facts(verdicts=(land,), head_ref="claude/integrate-x")
        self.assertEqual(rules.merge_decision(pr, "anthropic").action, "MERGE")
        self.assertEqual(rules.merge_decision(dataclasses.replace(pr, head_ref="chatgpt/x"), "openai").action, "WAIT")


class CodexFindingsOn178(unittest.TestCase):
    """The three findings Codex made on #178's review pool."""

    def codex_land(self, sha=HEAD):
        body = ("<!-- codex-review-state:v1 &#123;\"code_review\":&#123;\"commit_sha\":\"" + sha +
                "\",\"has_unresolved_findings\":false,\"status\":\"completed\"&#125;&#125; -->")
        return rules.parse_codex_state(body, T1, rules.CODEX_LOGIN)

    def test_a_same_vendor_land_does_not_answer_the_ask(self):
        pr = facts(verdicts=(self.codex_land(),), head_ref="chatgpt/x")
        self.assertTrue(rules.needs_review(pr, "openai"))       # Codex on ChatGPT's work: still ask someone else
        self.assertFalse(rules.needs_review(pr, "anthropic"))   # Codex on Claude's work: answered
        self.assertFalse(rules.needs_review(pr))                # old callers keep the old meaning


class ApiReviewsClaude(unittest.TestCase):
    """Cory, 11-03: the GitHub Actions Claude review gets real work: Claude's PRs too, and its LAND counts there."""

    ON = rules.team_from_config(dict(AGENTS, review=dict(AGENTS["review"], pool=["claude-api", "muse", "chatgpt"],
                                                        api_reviews_claude=True)))

    def api(self, verdict):
        return v(f"**Claude (automatic review of #9 @ {HEAD[:8]}): {verdict}**" + (REPRO if verdict == "FIX" else ""),
                 T1, rules.BOT_LOGIN)

    def test_the_api_can_take_claude_prs(self):
        self.assertIn(rules.API_REVIEWER, rules.review_rotation(4, "anthropic", frozenset(), self.ON))

    def test_its_land_counts_on_claude_work_only_when_on(self):
        pr = facts(verdicts=(self.api("LAND"),), head_ref="claude/integrate-x")
        self.assertEqual(rules.merge_decision(pr, "anthropic", self.ON).action, "MERGE")
        self.assertEqual(rules.merge_decision(pr, "anthropic", TEAM).action, "WAIT")
        self.assertFalse(rules.needs_review(pr, "anthropic", self.ON))
        self.assertTrue(rules.needs_review(pr, "anthropic", TEAM))

    def test_a_claude_session_land_never_counts_on_claude_work(self):
        pr = facts(verdicts=(land("Claude"),), head_ref="claude/integrate-x")
        self.assertEqual(rules.merge_decision(pr, "anthropic", self.ON).action, "WAIT")

    def test_its_fix_still_blocks(self):
        pr = facts(verdicts=(land("ChatGPT"), self.api("FIX")), head_ref="claude/integrate-x")
        self.assertEqual(rules.merge_decision(pr, "anthropic", self.ON).action, "FIX")


class ScopedProof(unittest.TestCase):
    """ci/proof_scope.py skips the test groups a push does not reach; a skipped job (a job-level `if`, which
    leaves one skipped record for a whole matrix) reads as passing to the switchboard and to sweep.py."""

    RUNS = {"workflow_runs": [{"id": 7, "status": "completed", "created_at": "2026-11-04T10:00:00Z",
                               "html_url": "https://x/runs/7", "updated_at": "2026-11-04T10:30:00Z"}]}

    def jobs(self, touch="success"):
        return {"jobs": [{"name": "scope", "status": "completed", "conclusion": "success"},
                         {"name": "host-build", "status": "completed", "conclusion": "success"},
                         {"name": "native-tests", "status": "completed", "conclusion": "skipped"},
                         {"name": "godot-touch (touch-1)", "status": "completed", "conclusion": touch},
                         {"name": "godot-touch (touch-2)", "status": "completed", "conclusion": "success"},
                         {"name": "android-build", "status": "completed", "conclusion": "skipped"},
                         {"name": "android-apk", "status": "completed", "conclusion": "skipped"},
                         {"name": "publish-checkpoint", "status": "completed", "conclusion": "skipped"}]}

    def fake_get(self, touch):
        return lambda path, accept="": self.RUNS if "/runs?head_sha=" in path else self.jobs(touch)

    def switch_proof(self, touch="success"):
        sw = switch.Switch.__new__(switch.Switch)
        sw.gh = types.SimpleNamespace(repo="IssisX/ScraperX", get=self.fake_get(touch))
        return sw.proof("abc")[0]

    def sweep_proof(self, touch="success"):
        sys.path.insert(0, str(HERE.parent / "board"))
        try:
            import sweep
        finally:
            sys.path.pop(0)
        get = self.fake_get(touch)
        real, sweep.gh = sweep.gh, lambda path: get(path)
        try:
            return sweep.proof_state("IssisX/ScraperX", "abc")[0]
        finally:
            sweep.gh = real

    def test_skipped_groups_read_green(self):
        self.assertEqual(self.switch_proof(), "green")
        self.assertEqual(self.sweep_proof(), "green")

    def test_a_failed_shard_still_reads_red(self):
        self.assertEqual(self.switch_proof("failure"), "red: godot-touch (touch-1)")
        self.assertEqual(self.sweep_proof("failure"), "red: godot-touch (touch-1)")

    def with_wake(self, run_status, touch_status):
        """A run still in progress: the wake job running, touch-1 in touch_status."""
        runs = {"workflow_runs": [dict(self.RUNS["workflow_runs"][0], status=run_status)]}
        jobs = self.jobs()
        jobs["jobs"][3] = dict(jobs["jobs"][3], status=touch_status,
                               conclusion="success" if touch_status == "completed" else None)
        jobs["jobs"].append({"name": "wake-switchboard", "status": "in_progress", "conclusion": None})
        self.fake_get = lambda touch: (lambda path, accept="": runs if "/runs?head_sha=" in path else jobs)

    def test_only_the_wake_job_left_reads_finished(self):
        self.with_wake("in_progress", "completed")
        self.assertEqual(self.switch_proof(), "green")
        self.assertEqual(self.sweep_proof(), "green")

    def test_a_proof_job_still_running_reads_running(self):
        self.with_wake("in_progress", "in_progress")
        self.assertEqual(self.switch_proof(), "running")
        self.assertEqual(self.sweep_proof(), "running")


class OldBoardsKeepMuseKeys(unittest.TestCase):
    """bridge-health: a muse key sent while #107 was the board is still sent after the board moved to #340."""

    NAME = "fb-bot/integrate-j044c-wreck"

    def red_branch(self):
        import test_inbox as ti
        gh = ti.FakeGH(runs=[ti.run(40, self.NAME, ti.HEAD, hours=2)], jobs={40: ti.RED_JOBS},
                       heads={self.NAME: ti.HEAD})
        return ti, gh, switch.inbox_key("muse-RED-BRANCH", self.NAME, ti.HEAD)

    def test_a_key_marked_on_a_closed_board_is_not_sent_again(self):
        ti, gh, key = self.red_branch()
        gh.by_issue[107] = [{"id": 9, "body": "CLAUDE_TO_MUSE: x RED-BRANCH on #107\n\n" +
                             switch.MUSE_MARK.format(keys=key), "user": {"login": ti.BOT}}]
        self.assertEqual(ti.make(gh).muse([]), 0)
        self.assertEqual(gh.posts, [])
        for old in switch.OLD_BOARDS:
            self.assertIn(f"repos/IssisX/ScraperX/issues/{old}/comments?per_page=100&page=1", gh.gets)

    def test_a_marker_by_another_login_on_a_closed_board_never_counts(self):
        ti, gh, key = self.red_branch()
        gh.by_issue[3] = [{"id": 9, "body": switch.MUSE_MARK.format(keys=key), "user": {"login": "IssisX"}}]
        self.assertEqual(ti.make(gh).muse([]), 1)

    def test_a_closed_board_read_is_bounded(self):
        sw = switch.Switch.__new__(switch.Switch)
        pages = []

        def get(path, accept=""):
            pages.append(path)
            return [{"id": i} for i in range(100)]   # every page full: only the bound stops it
        sw.gh = types.SimpleNamespace(repo="IssisX/ScraperX", get=get)
        self.assertEqual(len(sw.bounded_comments(107, switch.MAX_OLD_BOARD_PAGES)), 100 * switch.MAX_OLD_BOARD_PAGES)
        self.assertEqual(len(pages), switch.MAX_OLD_BOARD_PAGES)


PIPE_NOW = dt.datetime(2026, 11, 9, 15, 0, tzinfo=dt.timezone.utc)


def pipe_at(minutes_ago: float) -> str:
    return (PIPE_NOW - dt.timedelta(minutes=minutes_ago)).strftime("%Y-%m-%dT%H:%M:%SZ")


def label_event(event: str, minutes_ago: float, name: str = "wake:muse") -> dict:
    return {"event": event, "label": {"name": name}, "created_at": pipe_at(minutes_ago)}


def pipe_comment(body: str, minutes_ago: float, login: str = rules.BOT_LOGIN) -> dict:
    return {"body": body, "user": {"login": login}, "created_at": pipe_at(minutes_ago)}


class PipeGH:
    """The reads pipe_health makes: wake:muse issues, their events, the board, repo comments since a time."""

    repo = "IssisX/ScraperX"

    def __init__(self, wake=None, board=(), comments=()):
        self.wake = dict(wake or {})          # issue number -> its events
        self.board, self.comments = list(board), list(comments)
        self.gets, self.posted = [], []

    def get(self, path, accept=""):
        self.gets.append(path)
        if path.startswith(f"repos/{self.repo}/issues?") and "labels=wake%3Amuse" in path:
            return [{"number": n} for n in self.wake]
        raise switch.GitHubError("GET", path, "gh: Not Found (HTTP 404)")

    def paged(self, path, key=""):
        self.gets.append(path)
        base = path.split("?")[0]
        if base == f"repos/{self.repo}/issues/{switch.BOARD_ISSUE}/comments":
            return list(self.board)
        if base == f"repos/{self.repo}/issues/comments":
            since = re.search(r"[?&]since=([^&]+)", path).group(1)
            return [c for c in self.comments if c["created_at"] >= since]
        m = re.fullmatch(rf"repos/{self.repo}/issues/(\d+)/events", base)
        if m:
            return list(self.wake[int(m.group(1))])
        raise switch.GitHubError("GET", path, "gh: Not Found (HTTP 404)")

    def comment(self, number, body, what=""):
        self.posted.append((number, body))
        self.board.append(pipe_comment(body, 0))


def pipe_switch(gh, now=PIPE_NOW):
    sw = switch.Switch.__new__(switch.Switch)
    sw.gh, sw.now, sw.lines = gh, now, []
    sw.out = sw.lines.append
    return sw


BRIDGE = "**Muse (via email bridge): J-047 done**\n\nok"
ASK = "CLAUDE_TO_MUSE: J-047 RED on #344 @ 1a2b3c4d\n\n- red"


class PipeHealth(unittest.TestCase):
    """bridge-health: a stuck Muse mail pipe is loud, once per incident (11-07: #107 sat labeled for 42 h)."""

    def test_a_healthy_pipe_posts_nothing(self):
        gh = PipeGH(wake={343: [label_event("labeled", 4)]},
                    comments=[pipe_comment(ASK, 60), pipe_comment(BRIDGE, 30, login="IssisX")])
        sw = pipe_switch(gh)
        self.assertEqual(sw.pipe_health(), [])
        self.assertEqual(gh.posted, [])
        self.assertIn("PIPE        healthy", sw.lines)

    def test_a_wake_label_older_than_20_min_alarms_once(self):
        gh = PipeGH(wake={343: [label_event("labeled", 300), label_event("unlabeled", 290), label_event("labeled", 45)],
                          337: [label_event("labeled", 10)]},
                    comments=[pipe_comment(BRIDGE, 30, login="IssisX")])
        (line,) = pipe_switch(gh).pipe_health()
        self.assertTrue(line.startswith("OUTBOUND STUCK:"))
        self.assertIn("#343 since 8:15 AM Central (0.8 h)", line)   # the label's latest stint, not its first
        self.assertNotIn("#337", line)
        (number, body), = gh.posted
        self.assertEqual(number, switch.BOARD_ISSUE)
        self.assertTrue(body.startswith("Claude (integrator): PIPE ALARM:"))
        self.assertTrue(body.endswith(switch.PIPE_MARK.format(kinds="outbound")))
        self.assertIsNone(switch.VERDICT_TRIGGER.search(body))   # the alarm cannot wake the switchboard
        sw = pipe_switch(gh, PIPE_NOW + dt.timedelta(minutes=15))   # next sweep, same incident: printed, not posted
        self.assertEqual(len(sw.pipe_health()), 1)
        self.assertEqual(len(gh.posted), 1)
        self.assertTrue(any(l.startswith("PIPE ALARM  OUTBOUND STUCK") for l in sw.lines))

    def test_a_new_outbound_incident_alarms_again(self):
        mark = "x\n\n" + switch.PIPE_MARK.format(kinds="outbound")
        gh = PipeGH(wake={343: [label_event("labeled", 30)]}, board=[pipe_comment(mark, 600)])
        pipe_switch(gh).pipe_health()   # every label now stuck went on after the last alarm: a new incident
        self.assertEqual(len(gh.posted), 1)
        gh = PipeGH(wake={343: [label_event("labeled", 700)], 299: [label_event("labeled", 30)]},
                    board=[pipe_comment(mark, 600)])
        pipe_switch(gh).pipe_health()   # #343 was stuck at the last alarm: the same incident
        self.assertEqual(gh.posted, [])

    def test_an_alarm_marker_by_another_login_never_counts(self):
        gh = PipeGH(wake={343: [label_event("labeled", 30)]},
                    board=[pipe_comment("x\n\n" + switch.PIPE_MARK.format(kinds="outbound"), 10, login="IssisX")])
        pipe_switch(gh).pipe_health()
        self.assertEqual(len(gh.posted), 1)

    def test_asks_with_no_bridge_post_in_24_h_alarm_once(self):
        gh = PipeGH(comments=[pipe_comment(BRIDGE, 26 * 60, login="IssisX"), pipe_comment(ASK, 120)])
        (line,) = pipe_switch(gh).pipe_health()
        self.assertTrue(line.startswith("INBOUND STUCK: 1 CLAUDE_TO_MUSE ask(s) in the last 24.0 h"))
        (number, body), = gh.posted
        self.assertEqual(number, switch.BOARD_ISSUE)
        self.assertTrue(body.endswith(switch.PIPE_MARK.format(kinds="inbound")))
        self.assertIsNone(switch.VERDICT_TRIGGER.search(body))
        gh.comments.append(pipe_comment(ASK, -29 * 60))   # 30 h on, still quiet: the same incident, no repeat
        self.assertEqual(len(pipe_switch(gh, PIPE_NOW + dt.timedelta(hours=30)).pipe_health()), 1)
        self.assertEqual(len(gh.posted), 1)

    def test_inbound_recovered_then_stuck_again_is_a_new_incident(self):
        gh = PipeGH(board=[pipe_comment("x\n\n" + switch.PIPE_MARK.format(kinds="inbound"), 40 * 60)],
                    comments=[pipe_comment(BRIDGE, 30 * 60, login="IssisX"), pipe_comment(ASK, 60)])
        pipe_switch(gh).pipe_health()
        (_, body), = gh.posted
        self.assertIn("INBOUND STUCK", body)

    def test_no_asks_means_no_inbound_alarm(self):
        gh = PipeGH(comments=[pipe_comment(BRIDGE, 30 * 60, login="IssisX")])
        self.assertEqual(pipe_switch(gh).pipe_health(), [])
        self.assertEqual(gh.posted, [])


class FactsGH:
    """The reads pr_facts makes for PR #256 (job J-042, issue #300): a table by path."""

    repo = "IssisX/ScraperX"

    def __init__(self, pr_comments=(), issue_comments=(), board=(), with_issue=True):
        self.table = {
            "repos/IssisX/ScraperX/pulls/256": {"mergeable": True},
            f"repos/IssisX/ScraperX/commits/{HEAD}": {"commit": {"committer": {"date": T0}}},
            "repos/IssisX/ScraperX/issues/256/comments": list(pr_comments),
            "repos/IssisX/ScraperX/pulls/256/reviews": [],
            "repos/IssisX/ScraperX/pulls/256/files": [],
            "repos/IssisX/ScraperX/issues/300/comments": list(issue_comments),
            "repos/IssisX/ScraperX/issues?labels=board&state=open": [{"number": switch.BOARD_ISSUE}],
            f"repos/IssisX/ScraperX/issues/{switch.BOARD_ISSUE}/comments": list(board),
            "repos/IssisX/ScraperX/issues?labels=job&state=open":
                [{"number": 300, "title": "J-042 windup", "labels": []}] if with_issue else [],
        }

    def get(self, path, accept=""):
        if "/actions/workflows/" in path:
            return {"workflow_runs": []}
        return self.table[path]

    def paged(self, path, key=""):
        return list(self.table[path])


def bridged(first: str, login: str = "IssisX", at: str = T1) -> dict:
    return {"body": f"**Muse (via email bridge): {first}**\n\nLooks right.\n\n_Mailed 9:00 AM Central._",
            "user": {"login": login}, "created_at": at}


class BridgedVerdicts(unittest.TestCase):
    """The bridge's Issues-only token cannot comment on a PR (11-09), so Muse's mailed verdict on #256 lands on the
    job issue or the board. The switch must still count it for #256, and only for #256."""

    PR = {"number": 256, "head": {"sha": HEAD, "ref": "fb-bot/integrate-j042"}, "base": {"ref": rules.TRUNK},
          "draft": False, "labels": [], "title": "J-042", "body": "Job: J-042\n"}

    def facts(self, gh):
        sw = switch.Switch.__new__(switch.Switch)
        sw.gh, sw.team, sw.review_key, sw._jobs, sw._board = gh, TEAM, "", None, None
        return sw.pr_facts(self.PR)

    def test_a_verdict_on_the_job_issue_counts_for_its_pr(self):
        facts, ctx = self.facts(FactsGH(issue_comments=[bridged("J-042 review of #256 @ 1a2b3c4d: LAND")]))
        (v,) = facts.verdicts
        self.assertEqual((v.who, v.verdict, v.sha, v.kind), ("Muse", "LAND", "1a2b3c4d", "review"))
        self.assertTrue(rules.names_head(v, HEAD))
        self.assertEqual(ctx["comments"], [])   # the PR's own thread is unchanged: markers are read there as before

    def test_a_verdict_on_the_board_counts_when_there_is_no_job_issue(self):
        gh = FactsGH(board=[bridged("review of #256 @ 1a2b3c4d: FIX")], with_issue=False)
        gh.table["repos/IssisX/ScraperX/issues/256/comments"] = []
        facts, _ = self.facts(gh)
        (v,) = facts.verdicts
        self.assertEqual((v.who, v.verdict), ("Muse", "NOTE"))   # a FIX with no repro lines reads as a NOTE, as on the PR

    def test_only_bridge_posts_naming_this_pr_count(self):
        facts, _ = self.facts(FactsGH(
            issue_comments=[bridged("review of #2560 @ 1a2b3c4d: LAND"),                     # another PR
                            bridged("J-042 done, see #256: LAND"),                           # not a review of #256
                            {"body": "**Muse (review of #256 @ 1a2b3c4d): LAND**", "user": {"login": "IssisX"},
                             "created_at": T1},                                              # not the bridge's
                            bridged("review of #256 @ 1a2b3c4d: LAND", login="someone")],    # not a verdict login
            board=[bridged("review of #256 @ 1a2b3c4d: LAND", login="someone")]))
        self.assertEqual(list(facts.verdicts), [])

    def test_a_verdict_on_the_pr_still_counts_once(self):
        facts, _ = self.facts(FactsGH(pr_comments=[bridged("review of #256 @ 1a2b3c4d: LAND")]))
        self.assertEqual(len(facts.verdicts), 1)


class MuseAskNeverOnAPr(unittest.TestCase):
    """A review ask to Muse goes on the job issue, else the board, never on the PR (its wake:muse label stuck the
    bridge from 11-07 to 11-09: the Issues-only token cannot remove a PR's label)."""

    def test_no_job_issue_asks_on_the_board(self):
        import test_review_fallback as rf
        gh = rf.FakeGH()
        rf.make(gh).ask_review(rf.pr_facts(), rf.ctx(issue=None), "muse", "")
        ((number, body),) = gh.posted
        self.assertEqual(number, switch.BOARD_ISSUE)
        self.assertTrue(body.startswith("CLAUDE_TO_MUSE: J-031 review #9 @ 1a2b3c4d"))
        self.assertEqual(gh.writes, [("POST", f"repos/IssisX/ScraperX/issues/{switch.BOARD_ISSUE}/labels")])

    def test_an_ask_on_the_board_is_found_next_sweep(self):
        import test_review_fallback as rf
        gh = rf.FakeGH()
        rf.make(gh).ask_review(rf.pr_facts(), rf.ctx(issue=None), "muse", "")
        gh2 = rf.FakeGH([dict(rf.ask_comment("muse", 0.5))])   # the board (and the PR) now hold the ask's marker
        rf.make(gh2).ask_review(rf.pr_facts(), rf.ctx(issue=None), "muse", "")
        self.assertEqual(gh2.posted, [])

    def test_other_agents_still_ask_on_the_pr(self):
        import test_review_fallback as rf
        gh = rf.FakeGH()
        rf.make(gh).ask_review(rf.pr_facts(), rf.ctx(issue=None), "chatgpt", "")
        self.assertEqual(gh.posted[0][0], 9)


class SweepHeartbeat(unittest.TestCase):
    """J-072: a full non-dry sweep pings Healthchecks.io once at its end; the URL is a secret and never printed."""

    URL = "https://hc-ping.com/0f0e0d0c-aaaa-bbbb-cccc-123456789abc"

    def ping(self, url, failed, fail_with=None):
        sent, out = [], []

        def opener(req, timeout):
            sent.append((req.full_url, req.get_method(), timeout))
            if fail_with:
                raise fail_with
            return types.SimpleNamespace(close=lambda: None)
        return switch.sweep_heartbeat(url, failed, out.append, opener), sent, out

    def test_unset_is_a_no_op(self):
        self.assertEqual(self.ping("", 0), (False, [], []))

    def test_success_pings_the_url_and_failure_pings_fail(self):
        ok, sent, out = self.ping(self.URL, 0)
        self.assertTrue(ok)
        self.assertEqual(sent, [(self.URL, "POST", switch.HC_TIMEOUT_S)])
        ok, sent, out2 = self.ping(self.URL + "/", 2)
        self.assertEqual(sent[0][0], self.URL + "/fail")
        self.assertFalse(any("hc-ping" in line for line in out + out2))

    def test_only_https_and_a_broken_ping_never_raises_or_leaks(self):
        self.assertEqual(self.ping("http://hc-ping.com/x", 0)[1], [])
        import urllib.error
        ok, _, out = self.ping(self.URL, 0, urllib.error.URLError(f"refused {self.URL}"))
        self.assertFalse(ok)
        self.assertFalse(any("0f0e0d0c" in line for line in out), out)

    def test_main_wires_it_to_the_end_of_a_full_non_dry_steer_sweep_only(self):
        src = (HERE / "switch.py").read_text(encoding="utf-8")
        main = src[src.index("def main() -> int:"):]
        self.assertIn('hc_sweep = os.environ.pop(HC_SWEEP_ENV, "").strip()', main)
        call = main.index("sweep_heartbeat(hc_sweep, failed, sw.out)")
        guard = main.rindex("if steer and not (args.pr or args.sha):", 0, call)
        self.assertIn("if not args.dry_run:", main[guard:call])
        self.assertLess(main.index("if args.digest or args.force_digest:"), guard)   # a digest returns first


SWEPT = dt.datetime(2026, 11, 6, 20, 0, tzinfo=dt.timezone.utc)   # test_inbox.NOW: the fake GitHub's clock


class LiveReconcilerRetry(unittest.TestCase):
    """J-072 (ChatGPT, comment 6752048443): the live reconciler is a part like the ledger. A pass that raised or gave
    up fails the sweep (heartbeat /fail); one that backed off on the rate limit is left for the next sweep. Either
    way it is due again at the very next sweep (a full pass), and once it commits it is not run again until due."""

    def sweep(self, whole, now, rec):
        from unittest import mock
        import test_inbox
        import test_sweep_calls as tsc
        gh = tsc.CountingGH([tsc.pr(n) for n in range(361, 364)], dry_run=False)
        sw = test_inbox.make(gh, now)
        sw.root = HERE.parents[1]
        sw.plan_mode = lambda: "off"   # the plan part has its own sweep tests (test_plan_sweep); not the live config
        sw.merge_token = sw.review_key = ""
        sw._jobs = sw._board = None
        gh.state = json.loads(json.dumps(whole))
        gh.remaining = gh.limit = gh.reset = None
        args = types.SimpleNamespace(part="steer", pr=0, sha="", issue=0, label="", digest=False,
                                     force_digest=False, dry_run=False)
        beats: list[int] = []
        with mock.patch("ledger.sweep", side_effect=tsc.no_ledger), \
                mock.patch("reconciler.sweep", side_effect=rec), \
                mock.patch.object(switch, "sweep_heartbeat", side_effect=lambda url, failed, out: beats.append(failed)):
            status = switch.run_sweep(sw, args, now, "https://hc-ping.com/x")
        return status, beats, gh.state, sw.lines

    def test_a_failed_or_uncommitted_live_pass_is_due_at_the_next_sweep(self):
        import reconciler
        ok = reconciler.Result("committed", "h", {}, 1, 3)
        for first, want_status, want_beat in (
                (switch.GitHubError("GET", "x", "gh: (HTTP 502)", 502), 2, 1),
                (reconciler.Result("gave-up", None, {}, 4, 0), 2, 1),
                (reconciler.Result("backoff", "h", {}, 1, 2), 0, 0)):
            with self.subTest(first=getattr(first, "status", type(first).__name__)):
                calls: list = []

                def rec(*a, first=first, calls=calls):
                    calls.append(1)
                    if len(calls) > 1:
                        return ok
                    if isinstance(first, Exception):
                        raise first
                    return first
                status, beats, state, lines = self.sweep({}, SWEPT, rec)
                self.assertEqual((status, beats), (want_status, [want_beat]), lines)
                self.assertIn("the reconciler", state["steer"]["retry"])
                status, beats, state, _ = self.sweep(state, SWEPT + dt.timedelta(minutes=10), rec)
                self.assertEqual(len(calls), 2, "retried at the very next sweep")
                self.assertEqual((status, beats, state["steer"]["retry"]), (0, [0], []))
                self.sweep(state, SWEPT + dt.timedelta(minutes=20), rec)
                self.assertEqual(len(calls), 2, "then not again before the next full pass")


class DryRunVisible(unittest.TestCase):
    """J-072: a dry run names itself, in its job summary and its run-name; only a real sweep is "Switchboard sweep"."""

    def test_a_dry_run_writes_dry_at_the_top_of_the_summary(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "summary.md")
            switch.note_dry_run(path)
            self.assertTrue(pathlib.Path(path).read_text(encoding="utf-8").startswith("### DRY RUN"))
        switch.note_dry_run(None)   # no summary file outside Actions: nothing to write

    @staticmethod
    def workflow(name="switchboard.yml"):
        import yaml
        return yaml.safe_load((HERE.parents[1] / ".github/workflows" / name).read_text(encoding="utf-8"))

    def test_run_name_and_the_bridge_agree_on_the_sweep_title(self):
        title = re.search(r"var SWEEP_TITLE = '([^']+)'", (HERE / "muse_bridge.gs").read_text(encoding="utf-8")).group(1)
        run_name = self.workflow()["run-name"]
        self.assertEqual(title, "Switchboard sweep")
        self.assertIn(f"'{title}'", run_name)
        # the dry test comes before the sweep fallback, so a dry dispatch is never named a sweep
        self.assertLess(run_name.index("inputs.dry_run && 'Switchboard DRY run"), run_name.index(f"'{title}'"))

    def test_a_dispatch_is_real_unless_asked_dry(self):
        self.assertIs(self.workflow()[True]["workflow_dispatch"]["inputs"]["dry_run"]["default"], False)

    def test_every_workflow_run_name_matches_a_workflow_literally(self):
        # `workflows:` entries are glob patterns: an unescaped + * ? [ ] ! is not literal, so the proof's
        # "013 + ascent" never matched its own name (the trigger last fired 11-05). Escaped, `\+` is literal.
        names = {str(self.workflow(p.name).get("name") or "") for p in (HERE.parents[1] / ".github/workflows").glob("*.yml")}
        for entry in (self.workflow()[True].get("workflow_run") or {}).get("workflows", []):
            tokens = re.findall(r"\\.|.", entry, re.DOTALL)
            self.assertFalse([t for t in tokens if t in "+*?[]!"], f"unescaped glob character in {entry!r}")
            self.assertIn("".join(t[-1] for t in tokens), names, entry)
