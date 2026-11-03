"""Unit tests for the switchboard's decisions: verdict parsing, the merge rule, the budget, the reviewer pick.

Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
No network, no clock: every time is given.
"""

from __future__ import annotations

import datetime as dt
import json
import os
import pathlib
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
        return next(i for i in self.issues if path.endswith(f"/issues/{i['number']}"))

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


if __name__ == "__main__":
    unittest.main()
