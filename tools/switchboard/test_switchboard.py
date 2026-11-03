"""Unit tests for the switchboard's decisions: verdict parsing, the merge rule, the budget, the reviewer pick.

Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
No network, no clock: every time is given.
"""

from __future__ import annotations

import datetime as dt
import pathlib
import sys
import unittest
from decimal import Decimal

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import budget as money  # noqa: E402
import rules  # noqa: E402

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
    def test_non_claude_authors_get_the_automatic_claude_review(self):
        for vendor in ("openai", "xai", "meta", "unknown"):
            self.assertEqual(rules.pick_reviewer(vendor, frozenset(), TEAM), rules.ReviewPick("claude-api", ""))

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
        self.assertEqual(rules.budget_override([("budget 12", "IssisX"), ("ok", "IssisX"), ("Budget 15.50\nthanks", "IssisX")], TEAM), "15.50")
        self.assertEqual(rules.budget_override([("my budget 12 is fine", "IssisX")], TEAM), "")

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
        self.assertNotIn("codex", {n for n in team.vendor_of_name})
        b = money.budget_from_config(yaml.safe_load((root / "work/budget.yml").read_text(encoding="utf-8")))
        self.assertLessEqual(b.month, Decimal("100"))
        # The API agent is on (Cory, 11-03); its hard per-session budget must fit the job and day caps.
        self.assertLessEqual(b.orchestrator_session_usd, b.job)
        self.assertLessEqual(b.orchestrator_session_usd, b.day)
        self.assertGreater(b.orchestrator_session_usd, Decimal("0"))
        looks = yaml.safe_load((root / "work/looks/sets.yml").read_text(encoding="utf-8"))
        for s in looks["sets"].values():
            self.assertTrue((root / s["rubric"]).is_file(), s["rubric"])


class SessionBudget(unittest.TestCase):
    def test_cents(self):
        import claude_calls
        self.assertEqual(claude_calls.cents(Decimal("2.00")), "200")
        self.assertEqual(claude_calls.cents(Decimal("1.259")), "125")
        with self.assertRaises(claude_calls.ClaudeCallError):
            claude_calls.cents(Decimal("0.004"))


if __name__ == "__main__":
    unittest.main()
