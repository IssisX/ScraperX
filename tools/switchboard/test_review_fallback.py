"""Unit tests for the reviewer fallback and the facts every review ask carries (J-031).

Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
No network, no clock, no files: GitHub, the rule check and the time are given.
"""

from __future__ import annotations

import datetime as dt
import pathlib
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import rules  # noqa: E402
import switch  # noqa: E402

AGENTS = {
    "agents": {
        "claude": {"vendor": "anthropic", "names": ["Claude"], "branch_prefixes": ["claude/"]},
        "chatgpt": {"vendor": "openai", "names": ["ChatGPT"], "branch_prefixes": ["chatgpt/"]},
        "grok": {"vendor": "xai", "names": ["Grok", "PE"], "branch_prefixes": ["grok/", "pe/"]},
        "muse": {"vendor": "meta", "names": ["Muse"], "branch_prefixes": ["muse/"]},
    },
    "review": {"claude_authored_asks": ["chatgpt", "muse"], "matron_reviewer": "muse",
               "fallback_after_hours": 3, "fallback_order": ["chatgpt", "grok"]},
    "verdict_logins": ["IssisX", "github-actions[bot]"],
}
TEAM = rules.team_from_config(AGENTS)
HEAD = "1a2b3c4d5e6f708192a3b4c5d6e7f8091a2b3c4d"
OTHER_HEAD = "ffffffff00000000ffffffff00000000ffffffff"
NOW = dt.datetime(2026, 11, 3, 15, 0, tzinfo=dt.timezone.utc)
BOT = rules.BOT_LOGIN


def ago(hours: float) -> str:
    return (NOW - dt.timedelta(hours=hours)).strftime("%Y-%m-%dT%H:%M:%SZ")


def ask_comment(agent: str, hours_ago: float, head: str = HEAD, login: str = BOT) -> dict:
    return {"body": f"**Switchboard → x:** review\n\n{switch.MARK.format(kind=f'ask-{agent}', pr=9, head=head)}",
            "user": {"login": login}, "created_at": ago(hours_ago)}


class FakeGH:
    repo, dry_run = "IssisX/ScraperX", False

    def __init__(self, thread=()):
        self.thread, self.posted, self.writes, self.gets = list(thread), [], [], []

    def paged(self, path, key=""):
        return list(self.thread)

    def get(self, path, accept=""):
        self.gets.append(path)
        return {"jobs": [{"name": "native-tests", "status": "completed", "conclusion": "success"},
                         {"name": "host-build", "status": "completed", "conclusion": "success"},
                         {"name": "publish-checkpoint", "status": "completed", "conclusion": "skipped"},
                         {"name": "touch-eyes", "status": "completed", "conclusion": "failure"}]}

    def comment(self, number, body, what=""):
        self.posted.append((number, body))

    def write(self, method, path, body=None, what=""):
        self.writes.append((method, path))


RC_CLEAN = (0, "rule_check: 0 BLOCK, 0 REVIEW\n")
RC_FINDINGS = (1, "REVIEW godot/a.gd:12 velocity-write: body.linear_velocity = v\n"
                  "BLOCK  tests/x.cpp:3 test-skip-added: + GTEST_SKIP()\n"
                  "rule_check: 1 BLOCK, 1 REVIEW\n")


def make(gh, rc=RC_CLEAN, team=TEAM):
    sw = switch.Switch.__new__(switch.Switch)
    sw.gh, sw.now, sw.team, sw.lines = gh, NOW, team, []
    sw.out = sw.lines.append
    sw.run_rule_check = lambda number, head: rc
    return sw


def pr_facts(**kw) -> rules.PrFacts:
    base = dict(number=9, head_sha=HEAD, head_ref="claude/integrate-x", base_ref=rules.TRUNK, draft=False, mergeable=True,
                head_committed_at=ago(5), proof="green", verdicts=(), needs=frozenset(), rule_check_block=False,
                touches_workflows=False)
    base.update(kw)
    return rules.PrFacts(**base)


def ctx(issue=138, comments=()):
    return {"job": "J-031", "issue": {"number": issue} if issue else None, "proof_url": "https://x/runs/7", "run_id": 7,
            "comments": list(comments), "title": "t", "body": "", "files": []}


class FallbackConfig(unittest.TestCase):
    def test_defaults_and_values(self):
        self.assertEqual((TEAM.fallback_order, TEAM.fallback_after_hours), (("chatgpt", "grok"), 3.0))
        bare = rules.team_from_config({k: v for k, v in AGENTS.items() if k != "review"} | {"review": {"claude_authored_asks": ["chatgpt"]}})
        self.assertEqual((bare.fallback_order, bare.fallback_after_hours), ((), 3.0))
        self.assertEqual(TEAM.agent_name["grok"], "Grok")

    def test_bad_config_names_the_problem(self):
        def with_review(**kw):
            return dict(AGENTS, review=dict(AGENTS["review"], **kw))
        for bad, word in ((with_review(fallback_order=["codex"]), "codex"), (with_review(fallback_order=["claude"]), "never reviewed by Claude"),
                          (with_review(fallback_after_hours=0), "fallback_after_hours"), (with_review(fallback_after_hours="soon"), "fallback_after_hours"),
                          (with_review(fallback_after_hours=10_000), "fallback_after_hours")):
            with self.assertRaisesRegex(ValueError, word):
                rules.team_from_config(bad)

    def test_the_repo_file_carries_the_fallback(self):
        try:
            import yaml
        except ImportError:
            self.skipTest("PyYAML not installed")
        team = rules.team_from_config(yaml.safe_load((HERE.parents[1] / "work/agents.yml").read_text(encoding="utf-8")))
        self.assertEqual((team.fallback_order, team.fallback_after_hours), (("muse", "chatgpt", "grok"), 1.0))
        self.assertEqual(team.other_authored_asks, ("muse", "chatgpt"))


class FallbackPick(unittest.TestCase):
    def test_next_is_the_first_unasked_other_vendor(self):
        self.assertEqual(rules.next_fallback(["chatgpt"], "anthropic", TEAM), "grok")
        self.assertEqual(rules.next_fallback(["muse"], "anthropic", TEAM), "chatgpt")
        self.assertEqual(rules.next_fallback(["muse", "chatgpt"], "anthropic", TEAM), "grok")

    def test_nobody_left_gives_empty(self):
        self.assertEqual(rules.next_fallback(["chatgpt", "grok"], "anthropic", TEAM), "")

    def test_never_the_authors_own_vendor_and_never_claude(self):
        self.assertEqual(rules.next_fallback(["chatgpt"], "xai", TEAM), "")       # Grok's own PR: Grok cannot review it
        self.assertEqual(rules.next_fallback([], "openai", TEAM), "grok")
        loose = rules.Team({}, [], {}, ["chatgpt"], "", {"claude": "anthropic", "chatgpt": "openai"}, frozenset(), {}, ("claude", "chatgpt"))
        self.assertEqual(rules.next_fallback([], "xai", loose), "chatgpt")        # even a hand-built team skips Claude

    def test_overdue_boundary(self):
        now = "2026-11-03T15:00:00Z"
        self.assertFalse(rules.ask_is_overdue("2026-11-03T12:00:01Z", now, 3))
        self.assertTrue(rules.ask_is_overdue("2026-11-03T12:00:00Z", now, 3))

    def test_a_fallback_verdict_counts_like_the_first(self):
        land = rules.parse_verdict("**Grok (review of #9 @ 1a2b3c4d): LAND**", ago(1), "IssisX", TEAM)
        self.assertEqual(land.vendor, "xai")
        d = rules.merge_decision(pr_facts(verdicts=(land,)), "anthropic")
        self.assertEqual((d.action, d.land_by), ("MERGE", "Grok"))
        own = rules.parse_verdict("**PE (review of #9 @ 1a2b3c4d): LAND**", ago(1), "IssisX", TEAM)
        self.assertEqual(rules.merge_decision(pr_facts(head_ref="grok/x", verdicts=(own,)), "xai").action, "WAIT")


class Escalation(unittest.TestCase):
    def escalate(self, thread, author="anthropic", **kw):
        gh = FakeGH(thread)
        sw = make(gh)
        sw.escalate_review(pr_facts(**kw), ctx(), author)
        return sw, gh

    def test_an_unanswered_ask_goes_to_the_next_agent_once(self):
        sw, gh = self.escalate([ask_comment("chatgpt", 4)])
        (number, body), = gh.posted
        self.assertEqual(number, 138)
        self.assertTrue(body.startswith("To Grok: J-031 review #9 @ 1a2b3c4d"), body[:80])   # what the Grok app answers
        self.assertIn("ChatGPT has not answered in 3 hours", body)
        self.assertIn("`**Grok (review of #9 @ 1a2b3c4d): LAND**`", body)
        self.assertIn(switch.MARK.format(kind="ask-grok", pr=9, head=HEAD), body)
        self.assertTrue(any(l.startswith("FALLBACK    #9") for l in sw.lines))
        # The next sweep sees both asks: grok's clock has just started, so nothing more is posted.
        sw2, gh2 = self.escalate([ask_comment("chatgpt", 4), {"body": body, "user": {"login": BOT}, "created_at": ago(0)}])
        self.assertEqual(gh2.posted, [])

    def test_not_yet_due_or_no_first_ask_posts_nothing(self):
        self.assertEqual(self.escalate([ask_comment("chatgpt", 2.9)])[1].posted, [])
        self.assertEqual(self.escalate([])[1].posted, [])

    def test_the_chain_ends_and_says_so(self):
        sw, gh = self.escalate([ask_comment("chatgpt", 9), ask_comment("grok", 4)])
        self.assertEqual(gh.posted, [])
        self.assertTrue(any(l.startswith("NOFALLBACK  #9") for l in sw.lines))

    def test_the_clock_is_the_latest_ask_so_a_late_second_ask_is_not_stacked(self):
        self.assertEqual(self.escalate([ask_comment("chatgpt", 9), ask_comment("muse", 1)])[1].posted, [])

    def test_matron_job_asked_muse_first_falls_back_to_chatgpt_then_grok(self):
        sw, gh = self.escalate([ask_comment("muse", 4)])
        self.assertIn("**Switchboard → ChatGPT:**", gh.posted[0][1])
        self.assertIn("Muse has not answered", gh.posted[0][1])

    def test_asks_on_another_head_or_by_someone_else_do_not_count(self):
        self.assertEqual(self.escalate([ask_comment("chatgpt", 9, head=OTHER_HEAD)])[1].posted, [])
        self.assertEqual(self.escalate([ask_comment("chatgpt", 9, login="someone")])[1].posted, [])
        sw, gh = self.escalate([ask_comment("chatgpt", 9), ask_comment("grok", 9, login="someone")])
        self.assertIn("To Grok:", gh.posted[0][1])   # a forged ask-grok marker does not use up Grok's one ask

    def test_never_claude_and_never_the_authors_vendor(self):
        sw, gh = self.escalate([ask_comment("chatgpt", 9)], author="xai")
        self.assertEqual(gh.posted, [])
        sw, gh = self.escalate([ask_comment("grok", 9)], author="openai")
        self.assertEqual(gh.posted, [])

    def test_steer_asks_first_then_falls_back_and_stops_at_any_verdict(self):
        gh = FakeGH([ask_comment("chatgpt", 4)])
        sw = make(gh)
        sw.stale = lambda facts: False
        sw.steer_pr(pr_facts(), ctx())
        self.assertEqual(len(gh.posted), 1)
        self.assertIn("To Grok:", gh.posted[0][1])
        note = rules.parse_verdict("**ChatGPT (review of #9 @ 1a2b3c4d): NOTE**\nlooks fine", ago(1), "IssisX", TEAM)
        gh = FakeGH([ask_comment("chatgpt", 4)])
        sw = make(gh)
        sw.stale = lambda facts: False
        sw.steer_pr(pr_facts(verdicts=(note,)), ctx())
        self.assertEqual(gh.posted, [])
        gh = FakeGH([ask_comment("chatgpt", 4)])
        sw = make(gh)
        sw.stale = lambda facts: True
        sw.steer_pr(pr_facts(), ctx())
        self.assertEqual(gh.posted, [])   # a stale head gets no new ask, fallback included


class AskFacts(unittest.TestCase):
    def ask(self, rc, agent="chatgpt"):
        gh = FakeGH()
        sw = make(gh, rc)
        sw.ask_review(pr_facts(), ctx(), agent, "")
        return gh.posted[0][1], gh

    def test_every_ask_carries_the_proof_jobs_and_the_rule_check(self):
        body, gh = self.ask(RC_CLEAN)
        self.assertIn("Facts read by the switchboard at @1a2b3c4d", body)
        self.assertIn("Proof jobs (https://x/runs/7): 1 failure, 1 skipped, 2 success", body)
        self.assertIn("not passing: touch-eyes", body)
        self.assertIn("rule_check: 0 BLOCK, 0 REVIEW (origin/ScraperX-Claude...@1a2b3c4d)", body)
        self.assertEqual(gh.gets, ["repos/IssisX/ScraperX/actions/runs/7/jobs?per_page=50"])
        self.assertTrue(body.startswith("**Switchboard → ChatGPT:** review #9 @ 1a2b3c4d (J-031)"))

    def test_block_and_review_lines_are_quoted_in_a_fence(self):
        body, _ = self.ask(RC_FINDINGS)
        self.assertIn("rule_check: 1 BLOCK, 1 REVIEW", body)
        self.assertIn("```\nREVIEW godot/a.gd:12 velocity-write: body.linear_velocity = v\nBLOCK  tests/x.cpp:3 test-skip-added: + GTEST_SKIP()\n```", body)

    def test_quoted_lines_are_bounded_and_cannot_close_the_fence(self):
        evil = "REVIEW f.gd:1 x: ``` @everyone " + "y" * 500
        many = "\n".join([evil] + [f"REVIEW f.gd:{n} x: t" for n in range(2, 60)]) + "\nrule_check: 0 BLOCK, 59 REVIEW\n"
        body, _ = self.ask((1, many))
        fenced = body.split("```")
        self.assertEqual(len(fenced), 3)                     # one fence pair: the quoted text held no fence of its own
        self.assertLessEqual(max(len(l) for l in fenced[1].splitlines()), 200)
        self.assertEqual(len(fenced[1].strip().splitlines()), switch.MAX_FACT_LINES)
        self.assertIn("(34 more lines not shown)", body)

    def test_an_unrunnable_rule_check_is_said_not_hidden(self):
        body, _ = self.ask(None)
        self.assertIn("rule_check: could not run here", body)

    def test_no_proof_run_still_states_the_proof(self):
        gh = FakeGH()
        sw = make(gh)
        c = ctx()
        c["run_id"] = 0
        sw.ask_review(pr_facts(), c, "grok", "")
        self.assertIn("- Proof: green", gh.posted[0][1])
        self.assertEqual(gh.gets, [])

    def test_an_ask_already_made_posts_and_reads_nothing(self):
        gh = FakeGH([ask_comment("chatgpt", 1)])
        sw = make(gh)
        sw.ask_review(pr_facts(), ctx(), "chatgpt", "")
        self.assertEqual((gh.posted, gh.gets), ([], []))

    def test_muse_ask_keeps_its_wake_label(self):
        body, gh = self.ask(RC_CLEAN, "muse")
        self.assertTrue(body.startswith("CLAUDE_TO_MUSE: J-031 review #9 @ 1a2b3c4d"))
        self.assertEqual(gh.writes, [("POST", "repos/IssisX/ScraperX/issues/138/labels")])


if __name__ == "__main__":
    unittest.main()


class PoolSteer(unittest.TestCase):
    """review.pool through the switch: one ask, to the free reviewer; the one asked keeps it for an hour."""

    POOL = rules.team_from_config(dict(AGENTS, review=dict(AGENTS["review"], pool=["claude-api", "muse", "chatgpt"],
                                                          fallback_after_hours=1)))

    def steer(self, thread, load, head_ref="claude/integrate-x"):
        gh = FakeGH(thread)
        sw = make(gh, team=self.POOL)
        sw.api_ok = lambda c: True
        calls = []
        sw.review_load = lambda: calls.append(1) or load
        sw.steer_pr(pr_facts(head_ref=head_ref), ctx(comments=thread))
        asks = [b for _, b in gh.posted if "ask-" in b]
        return asks, calls

    def test_busy_muse_means_chatgpt_takes_it(self):
        asks, _ = self.steer([], ({"muse": 1}, {}))
        self.assertEqual(len(asks), 1)
        self.assertIn("ask-chatgpt", asks[0])

    def test_quiet_past_the_hour_passes_on_and_says_why(self):
        asks, _ = self.steer([ask_comment("muse", 1.5)], ({}, {}))
        self.assertEqual(len(asks), 1)
        self.assertIn("ask-chatgpt", asks[0])
        self.assertIn("Muse did not answer in 1 hours", asks[0])

    def test_the_one_asked_keeps_it_and_no_load_is_read(self):
        asks, calls = self.steer([ask_comment("muse", 0.5)], ({}, {}))
        self.assertEqual((asks, calls), ([], []))

    def test_work_not_by_claude_can_go_to_the_api_and_nobody_is_asked(self):
        asks, _ = self.steer([], ({"muse": 1, "chatgpt": 1}, {}), head_ref="grok/integrate-x")
        self.assertEqual(asks, [])


class CodexAsk(unittest.TestCase):
    def test_codex_is_asked_on_the_pr_with_a_mention(self):
        team = rules.team_from_config(dict(AGENTS, agents=dict(AGENTS["agents"], codex={
            "vendor": "openai", "names": ["Codex"], "branch_prefixes": ["codex/"]}),
            review=dict(AGENTS["review"], pool=["codex", "muse"], fallback_after_hours=1)))
        gh = FakeGH([])
        sw = make(gh, team=team)
        sw.api_ok = lambda c: True
        sw.review_load = lambda: ({}, {})
        sw.steer_pr(pr_facts(number=10), dict(ctx(), comments=[]))
        self.assertEqual(len(gh.posted), 1)
        where, body = gh.posted[0]
        self.assertEqual(where, 10)
        self.assertTrue(body.startswith("@codex review"))
        self.assertIn("ask-codex", body)
