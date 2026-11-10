"""Unit tests for the reviewer fallback and the facts every review ask carries (J-031).

Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
No network, no clock, no files: GitHub, the rule check and the time are given.
"""

from __future__ import annotations

import contextlib
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
        self.as_team = []   # (path, body) of each write made with another token (the team token)

    @contextlib.contextmanager
    def fresh(self):   # ghapi.GitHub.fresh: this stub has no cache, so every read is already GitHub's own
        yield self

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

    def write(self, method, path, body=None, what="", token=""):
        if token and getattr(self, "refuse", 0):   # GitHub refusing another token's call (the team token)
            raise switch.GitHubError(method, path, f"gh: Forbidden (HTTP {self.refuse})", self.refuse)
        self.writes.append((method, path))
        if token:
            self.as_team.append((path, (body or {}).get("body", "")))


RC_CLEAN = (0, "rule_check: 0 BLOCK, 0 REVIEW\n")
RC_FINDINGS = (1, "REVIEW godot/a.gd:12 velocity-write: body.linear_velocity = v\n"
                  "BLOCK  tests/x.cpp:3 test-skip-added: + GTEST_SKIP()\n"
                  "rule_check: 1 BLOCK, 1 REVIEW\n")


def make(gh, rc=RC_CLEAN, team=TEAM):
    sw = switch.Switch.__new__(switch.Switch)
    sw.gh, sw.now, sw.team, sw.lines, sw._load = gh, NOW, team, [], None
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
            "comments": list(comments), "title": "t", "body": "", "files": [], "own_head": True}


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
        # 11-09: ChatGPT's review task polls hourly, so it is the first fallback, Grok (twice a day) the second.
        self.assertEqual((team.fallback_order, team.fallback_after_hours), (("chatgpt", "grok"), 1.0))
        self.assertEqual(team.other_authored_asks, ("chatgpt", "grok"))
        self.assertEqual(team.manual, frozenset())
        routed = tuple(team.pool) + tuple(team.fallback_order) + tuple(team.human_reviewers) + tuple(team.other_authored_asks)
        self.assertNotIn("muse", routed)
        for author in ("anthropic", "meta", "xai"):
            self.assertEqual(rules.next_fallback([], author, team), "chatgpt", author)
            self.assertEqual(rules.next_fallback(["chatgpt"], author, team), "" if author == "xai" else "grok", author)
        # an OpenAI-authored PR (Codex, ChatGPT) never asks ChatGPT: its LAND would not count
        self.assertEqual(rules.next_fallback([], "openai", team), "grok")


class ManualWake(unittest.TestCase):
    """J-072: an agent with `wake: manual` runs only when Cory starts it, so no review ask is ever routed to it,
    whatever the lists name: a Claude-authored PR goes to Codex and the API first, then Grok, never ChatGPT."""

    CFG = dict(AGENTS, agents=dict(AGENTS["agents"], chatgpt=dict(AGENTS["agents"]["chatgpt"], wake="manual")),
               review={"claude_authored_asks": ["chatgpt", "grok"], "other_authored_asks": ["chatgpt", "grok"],
                       "pool": ["claude-api", "chatgpt"], "api_reviews_claude": True,
                       "fallback_after_hours": 1, "fallback_order": ["grok", "chatgpt"]})
    TEAM = rules.team_from_config(CFG)

    def test_never_in_the_rotation_or_the_fallback(self):
        self.assertEqual(rules.review_rotation(9, "anthropic", frozenset(), self.TEAM, api_ok=True), [rules.API_REVIEWER])
        self.assertEqual(rules.review_rotation(9, "anthropic", frozenset(), self.TEAM, api_ok=False), ["grok"])
        self.assertEqual(rules.next_fallback(["grok"], "anthropic", self.TEAM), "")   # nobody left: never ChatGPT

    def test_never_picked_from_the_ask_lists(self):
        self.assertEqual(rules.pick_reviewer("anthropic", frozenset(), self.TEAM), rules.ReviewPick("ask", "grok"))
        self.assertEqual(rules.pick_reviewer("meta", frozenset(), self.TEAM), rules.ReviewPick("ask", "grok"))

    def test_a_list_of_only_manual_agents_is_refused(self):
        with self.assertRaisesRegex(ValueError, "wake: manual"):
            rules.team_from_config(dict(self.CFG, review={"claude_authored_asks": ["chatgpt"]}))


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
        self.assertIn(f"`**Grok (review of #9 @ {HEAD}): LAND**`", body)   # the whole head (J-056)
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
        self.assertIn("To ChatGPT:", gh.posted[0][1])
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

    def test_steer_asks_first_then_falls_back_and_stops_at_a_land_or_a_fix_never_at_a_note(self):
        gh = FakeGH([ask_comment("chatgpt", 4)])
        sw = make(gh)
        sw.stale = lambda facts: False
        sw.steer_pr(pr_facts(), ctx())
        self.assertEqual(len(gh.posted), 1)
        self.assertIn("To Grok:", gh.posted[0][1])
        # J-072: a NOTE lands nothing (#376 sat on one with no one asked), so the ask still passes on to Grok
        note = rules.parse_verdict("**ChatGPT (review of #9 @ 1a2b3c4d): NOTE**\nlooks fine", ago(1), "IssisX", TEAM)
        gh = FakeGH([ask_comment("chatgpt", 4)])
        sw = make(gh)
        sw.stale = lambda facts: False
        sw.steer_pr(pr_facts(verdicts=(note,)), ctx())
        self.assertEqual(len(gh.posted), 1)
        self.assertIn("To Grok:", gh.posted[0][1])
        fix = rules.parse_verdict("**ChatGPT (review of #9 @ 1a2b3c4d): FIX**\ncommand: x\ninput: y\nexpected: a\n"
                                  "observed: b", ago(1), "IssisX", TEAM)
        self.assertEqual(fix.verdict, "FIX")
        gh = FakeGH([ask_comment("chatgpt", 4)])
        sw = make(gh)
        sw.stale = lambda facts: False
        sw.steer_pr(pr_facts(verdicts=(fix,)), ctx())
        self.assertEqual(gh.posted, [])   # a FIX is an answer: the author owes the next step, no one else is asked
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
        self.assertTrue(body.startswith("To ChatGPT: J-031 review #9 @ 1a2b3c4d"))

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
    def test_codex_is_asked_on_the_pr_without_the_bots_mention(self):
        # J-072: the switchboard's own "@codex review" never woke Codex (2 of 2, #191 and #150): the ask is recorded
        # on the PR with its mark, and the integrator's inbox gets a CODEX-ASK item to post the mention itself
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
        self.assertNotIn("@", body)
        self.assertIn("ask-codex", body)
        self.assertIn("The integrator posts the Codex review command", body)


class PoolFallbackAndLoad(unittest.TestCase):
    POOL = rules.team_from_config(dict(AGENTS, review=dict(AGENTS["review"], pool=["claude-api", "chatgpt", "muse"],
                                                          claude_authored_asks=["chatgpt", "muse"])))

    def test_api_fallback_never_asks_the_authors_vendor(self):
        sw = make(FakeGH([]), team=self.POOL)
        sw.api_ok = lambda c: True
        sw.review_load = lambda: ({}, {})
        self.assertEqual(sw.api_fallback(pr_facts(number=9, head_ref="chatgpt/x"), ctx(comments=[]), "openai"), "muse")
        self.assertNotEqual(sw.api_fallback(pr_facts(number=9, head_ref="muse/x"), ctx(comments=[]), "meta"), "muse")

    def test_after_the_api_failed_a_quiet_fallback_passes_on(self):
        # The API failed on this head (its fallback ask to ChatGPT carries the api-out mark) and ChatGPT
        # has been quiet past the hour: the next pick is Muse, never the API again (Codex on #178).
        failed = {"body": f"ask {switch.MARK.format(kind='api-out', pr=9, head=HEAD)} "
                          f"{switch.MARK.format(kind='ask-chatgpt', pr=9, head=HEAD)}",
                  "user": {"login": BOT}, "created_at": ago(1.5)}
        gh = FakeGH([failed])
        sw = make(gh, team=rules.team_from_config(dict(AGENTS, review=dict(
            AGENTS["review"], pool=["claude-api", "chatgpt", "muse"], fallback_after_hours=1, api_reviews_claude=True))))
        sw.api_ok = lambda c: True
        sw.review_load = lambda: ({}, {})
        sw.gh = FakeGH([])
        clean, _, _ = sw.pooled_reviewer(pr_facts(number=9), ctx(comments=[]), "anthropic")
        sw.gh = gh
        self.assertIn(rules.API_REVIEWER, clean)             # without the failure the API is in the rotation
        rotation, who, _ = sw.pooled_reviewer(pr_facts(number=9), ctx(comments=[failed]), "anthropic")
        self.assertNotIn(rules.API_REVIEWER, rotation)
        self.assertEqual(who, "muse")

    def test_assignments_in_one_run_spread(self):
        gh = FakeGH([])
        sw = make(gh, team=self.POOL)
        sw.api_ok = lambda c: True
        sw._load = ({}, {})
        sw.review_load = lambda: sw._load
        for n in (20, 21, 22, 23):
            sw.steer_pr(pr_facts(number=n), dict(ctx(), comments=[]))
        asked = [b.split("ask-", 1)[1].split(" ", 1)[0] for _, b in gh.posted if "ask-" in b]
        self.assertEqual(sorted(asked), ["chatgpt", "chatgpt", "muse", "muse"])


class CodexFirst(unittest.TestCase):
    """Cory, 11-04: Codex reviews every PR; the API takes a head Codex has not reviewed; Grok and ChatGPT only
    when neither can, by an ask that starts "To Grok:" or "To ChatGPT:"."""

    TEAM = rules.team_from_config(dict(AGENTS, agents=dict(AGENTS["agents"], codex={
        "vendor": "openai", "names": ["Codex"], "branch_prefixes": ["codex/"]}),
        review={"claude_authored_asks": ["chatgpt"], "pool": ["claude-api"], "api_reviews_claude": True,
                "fallback_after_hours": 1, "fallback_order": ["grok", "chatgpt"]}))

    def codex(self, sha, unresolved=False):
        body = ("<!-- codex-review-state:v1 &#123;\"code_review\":&#123;"
                f"\"commit_sha\":\"{sha}\",\"has_unresolved_findings\":{str(unresolved).lower()},"
                "\"status\":\"completed\"&#125;&#125; -->")
        return rules.parse_codex_state(body, ago(1), rules.CODEX_LOGIN)

    def steer(self, facts, api_ok=True, thread=()):
        gh = FakeGH(thread)
        sw = make(gh, team=self.TEAM)
        sw.api_ok = lambda c: api_ok
        sw.review_load = lambda: ({}, {})
        sw.steer_pr(facts, dict(ctx(comments=list(thread)), fresh=True))   # GitHub's own facts (J-072: no re-read)
        who = sw.pooled_reviewer(facts, ctx(comments=list(thread)), "anthropic")[1] if rules.needs_review(facts, "anthropic", self.TEAM) else ""
        return gh, who

    def test_codex_clean_at_the_head_is_the_review(self):
        f = pr_facts(verdicts=(self.codex(HEAD),))
        self.assertFalse(rules.needs_review(f, "anthropic", self.TEAM))
        self.assertEqual(rules.merge_decision(f, "anthropic", self.TEAM).action, "MERGE")
        gh, who = self.steer(f)
        self.assertEqual((gh.posted, who), ([], ""))

    def test_codex_findings_are_a_fix_and_no_one_else_is_asked(self):
        f = pr_facts(verdicts=(self.codex(HEAD, unresolved=True),))
        self.assertEqual(rules.merge_decision(f, "anthropic", self.TEAM).action, "FIX")
        gh, who = self.steer(f)
        self.assertEqual((gh.posted, who), ([], ""))

    CODEX_ASKED = ask_comment("codex", 2)   # Codex had its one ask on this head, and its turn is over

    def test_codex_on_another_head_is_asked_once_for_this_head_then_the_api_reviews(self):
        # J-072: Codex reviews by itself only the head a person's PR opened at; every other head sat unreviewed
        # by it (#376, #367), so the switch asks, once per head, before the API's turn
        for facts in (pr_facts(verdicts=(self.codex(OTHER_HEAD),)), pr_facts()):   # an older head; no state at all
            gh, who = self.steer(facts)
            (where, body), = gh.posted
            self.assertEqual((where, who), (9, rules.API_REVIEWER))
            self.assertIn(switch.MARK.format(kind="ask-codex", pr=9, head=HEAD), body)
            gh, who = self.steer(facts, thread=[self.CODEX_ASKED])   # asked already: never a second time
            self.assertEqual((gh.posted, who), ([], rules.API_REVIEWER))

    def test_api_out_of_budget_asks_grok_once_by_a_to_grok_comment(self):
        gh, who = self.steer(pr_facts(), api_ok=False, thread=[self.CODEX_ASKED])
        (where, body), = gh.posted
        self.assertEqual((where, who), (138, "grok"))
        self.assertTrue(body.startswith("To Grok: J-031 review #9 @ 1a2b3c4d"), body[:60])

    def test_grok_quiet_past_the_hour_passes_to_chatgpt(self):
        gh, _ = self.steer(pr_facts(), api_ok=False, thread=[self.CODEX_ASKED, ask_comment("grok", 1.5)])
        (where, body), = gh.posted
        self.assertTrue(body.startswith("To ChatGPT: J-031 review #9 @ 1a2b3c4d"), body[:60])

    def test_api_failed_on_the_head_falls_back_to_grok_never_the_api_again(self):
        failed = {"body": f"x {switch.MARK.format(kind='api-out', pr=9, head=HEAD)}", "user": {"login": BOT}, "created_at": ago(0.2)}
        sw = make(FakeGH([failed]), team=self.TEAM)
        sw.api_ok = lambda c: True
        sw.review_load = lambda: ({}, {})
        rotation, who, _ = sw.pooled_reviewer(pr_facts(), ctx(comments=[failed]), "anthropic")
        self.assertEqual((rotation, who), (["grok", "chatgpt"], "grok"))
        self.assertEqual(sw.api_fallback(pr_facts(), ctx(comments=[failed]), "anthropic"), "grok")

    def test_the_fallbacks_never_join_the_normal_path_or_review_their_own_vendor(self):
        self.assertEqual(rules.review_rotation(9, "anthropic", frozenset(), self.TEAM), [rules.API_REVIEWER])
        self.assertEqual(rules.review_rotation(9, "openai", frozenset(), self.TEAM, api_ok=False), ["grok"])
        self.assertEqual(rules.review_rotation(9, "xai", frozenset(), self.TEAM, api_ok=False), ["chatgpt"])


class CodexRunning(unittest.TestCase):
    """J-031: a review Codex is still running on the head is waited for, up to 30 minutes after the proof went green."""

    TEAM = CodexFirst.TEAM
    EYES = {"content": "eyes", "user": {"login": rules.CODEX_LOGIN}, "created_at": ago(0.3)}

    def state(self, sha, status):
        return {"body": ("<!-- codex-review-state:v1 &#123;\"code_review\":&#123;"
                         f"\"commit_sha\":\"{sha}\",\"has_unresolved_findings\":null,\"status\":\"{status}\"&#125;&#125; -->"),
                "user": {"login": rules.CODEX_LOGIN}, "created_at": ago(0.3)}

    def go(self, comments=(), reactions=(), green_minutes_ago=10):
        gh = FakeGH(comments)
        gh.paged = lambda path, key="": list(reactions) if path.endswith("/reactions") else list(comments)
        sw = make(gh, team=self.TEAM)
        sw.api_ok = lambda c: True
        sw.review_load = lambda: ({}, {})
        reviewed = []
        sw.review = lambda facts, c, author, security=False: reviewed.append(facts.head_sha)
        sw.marked = lambda *a, **kw: False
        sw.stale = lambda facts: False
        c = dict(ctx(comments=list(comments)), proof_done_at=ago(green_minutes_ago / 60))
        sw.ask_pr(pr_facts(), c)
        sw.steer_pr(pr_facts(), c)
        return reviewed, gh.posted, sw.lines

    def test_running_state_on_the_head_waits(self):
        reviewed, posted, lines = self.go([self.state(HEAD, "running")])
        self.assertEqual((reviewed, posted), ([], []))
        self.assertTrue(any(l.startswith("WAIT-CODEX") for l in lines))

    def test_eyes_without_a_completed_state_waits(self):
        reviewed, posted, _ = self.go(reactions=[self.EYES])
        self.assertEqual((reviewed, posted), ([], []))

    def test_running_past_30_minutes_falls_back_to_the_api(self):
        reviewed, posted, lines = self.go([self.state(HEAD, "running")], [self.EYES], green_minutes_ago=31)
        self.assertEqual((reviewed, posted), ([HEAD], []))
        self.assertFalse(any(l.startswith("WAIT-CODEX") for l in lines))

    def test_running_on_another_head_or_nothing_running_does_not_wait(self):
        self.assertEqual(self.go([self.state(OTHER_HEAD, "running")])[0], [HEAD])
        self.assertEqual(self.go()[0], [HEAD])
        # an eyes reaction next to a completed state for the head is not a running review
        self.assertFalse(rules.codex_in_flight([self.state(HEAD, "completed")], [self.EYES], HEAD))


class NoteLandsNothing(unittest.TestCase):
    """J-072: #376 @ 7b79e7cd held the API's signed NOTE and nothing else. needs_review took the NOTE for an
    answer, so no one was asked; merge_decision merges only on a LAND, so nothing could land it either."""

    TEAM = rules.team_from_config(dict(AGENTS, agents=dict(AGENTS["agents"], codex={
        "vendor": "openai", "names": ["Codex"], "branch_prefixes": ["codex/"]}),
        review={"claude_authored_asks": ["chatgpt"], "pool": ["claude-api"], "api_reviews_claude": True,
                "fallback_after_hours": 1, "fallback_order": ["chatgpt", "grok"]}))
    MUSE = "muse/integrate-protected-j069"

    def verdict(self, who, verdict, login="IssisX", hours=1.0):
        mid = "automatic review" if login == BOT else "review"
        body = f"**{who} ({mid} of #9 @ {HEAD}): {verdict}**\n" + \
               ("command: x\ninput: y\nexpected: a\nobserved: b" if verdict == "FIX" else "a suspicion, no repro")
        return rules.parse_verdict(body, ago(hours), login, self.TEAM)

    def steer(self, verdicts, thread=(), api_ok=True):
        gh = FakeGH(thread)
        sw = make(gh, team=self.TEAM)
        sw.api_ok = lambda c: api_ok
        sw.review_load = lambda: ({}, {})
        facts = pr_facts(head_ref=self.MUSE, verdicts=tuple(verdicts))
        c = ctx(comments=list(thread))
        sw.steer_pr(facts, c)
        return gh, sw, facts, c

    def test_a_note_is_no_answer_and_a_land_or_a_fix_is(self):
        note, land, fix = (self.verdict("Claude", v, login=BOT) for v in ("NOTE", "LAND", "FIX"))
        self.assertTrue(rules.is_api_review(note))
        self.assertTrue(rules.needs_review(pr_facts(head_ref=self.MUSE, verdicts=(note,)), "meta", self.TEAM))
        self.assertFalse(rules.needs_review(pr_facts(head_ref=self.MUSE, verdicts=(land,)), "meta", self.TEAM))
        self.assertFalse(rules.needs_review(pr_facts(head_ref=self.MUSE, verdicts=(fix,)), "meta", self.TEAM))
        # the gate itself is unchanged: a NOTE never merges
        d = rules.merge_decision(pr_facts(head_ref=self.MUSE, verdicts=(note,)), "meta", self.TEAM)
        self.assertEqual(d.action, "WAIT")

    def test_after_the_apis_note_codex_is_asked_then_the_fallback_one_at_a_time_never_the_api_again(self):
        note = self.verdict("Claude", "NOTE", login=BOT)
        gh, sw, facts, c = self.steer([note])   # Codex has not been asked on this head: its ask comes first
        (where, body), = gh.posted
        self.assertEqual(where, 9)
        self.assertIn("ask-codex", body)
        asked = [ask_comment("codex", 2)]       # Codex's turn is over: the first of the fallback order
        gh, sw, facts, c = self.steer([note], asked)
        (where, body), = gh.posted
        self.assertTrue(body.startswith("To ChatGPT: J-031 review #9 @ 1a2b3c4d"), body[:60])
        self.assertIn("Claude gave this head a NOTE (it is on #9), which lands nothing: a LAND or a FIX is asked", body)
        rotation, who, _ = sw.pooled_reviewer(facts, c, "meta")
        self.assertEqual((rotation, who), (["chatgpt", "grok"], "chatgpt"))   # the API is out of this head's rotation
        asked.append(ask_comment("chatgpt", 0.5))   # ChatGPT holds it for the hour: no second reviewer at once
        self.assertEqual(self.steer([note], asked)[0].posted, [])
        asked[-1] = ask_comment("chatgpt", 1.5)     # quiet past the hour: Grok
        (where, body), = self.steer([note], asked)[0].posted
        self.assertTrue(body.startswith("To Grok:"), body[:40])

    def test_a_fallback_reviewers_note_passes_on_at_once_not_after_its_hour(self):
        notes = [self.verdict("Claude", "NOTE", login=BOT), self.verdict("ChatGPT", "NOTE", hours=0.1)]
        asked = [ask_comment("codex", 2), ask_comment("chatgpt", 0.5)]
        gh, sw, facts, c = self.steer(notes, asked)
        (where, body), = gh.posted
        self.assertTrue(body.startswith("To Grok:"), body[:40])
        self.assertEqual(rules.reviewers_done(facts.verdicts, HEAD, self.TEAM), {rules.API_REVIEWER, "chatgpt"})

    def test_the_paid_review_never_runs_twice_on_a_note(self):
        note = self.verdict("Claude", "NOTE", login=BOT)
        gh = FakeGH([ask_comment("codex", 2)])
        sw = make(gh, team=self.TEAM)
        sw.api_ok = lambda c: True
        sw.review_load = lambda: ({}, {})
        sw.stale = lambda facts: False
        reviewed = []
        sw.review = lambda facts, c, author, security=False: reviewed.append(facts.head_sha)
        sw.ask_pr(pr_facts(head_ref=self.MUSE, verdicts=(note,)), ctx(comments=gh.thread))
        self.assertEqual(reviewed, [])

    def test_reviewers_done_reads_the_head_only(self):
        other = rules.parse_verdict(f"**ChatGPT (review of #9 @ {OTHER_HEAD}): NOTE**\nx", ago(1), "IssisX", self.TEAM)
        self.assertEqual(rules.reviewers_done([other], HEAD, self.TEAM), frozenset())
        self.assertEqual(rules.reviewers_done([other], OTHER_HEAD, self.TEAM), {"chatgpt"})


class CodexMention(unittest.TestCase):
    """J-072: the Codex app answers a person's `@codex review`, never the workflow bot's, and reviews by itself only
    a PR a person opened. The steer part posts the mention with the team token, once per head."""

    TEAM = NoteLandsNothing.TEAM
    TAG = switch.MARK.format(kind="ask-codex", pr=9, head=HEAD)

    def steer(self, thread=(), token="t0ken", refuse=0, head_ref="muse/integrate-x", green_minutes_ago=3.0, **c):
        gh = FakeGH(thread)
        gh.refuse = refuse
        sw = make(gh, team=self.TEAM)
        sw.merge_token = token
        sw.api_ok = lambda c: True
        sw.review_load = lambda: ({}, {})
        sw.stale = lambda facts: False
        sw.steer_pr(pr_facts(head_ref=head_ref), dict(ctx(comments=list(thread)), proof_done_at=ago(green_minutes_ago / 60), **c))
        return gh, sw

    def test_the_mention_goes_as_the_team_login_with_the_mark_and_nothing_else(self):
        gh, sw = self.steer()
        self.assertEqual(gh.as_team, [("repos/IssisX/ScraperX/issues/9/comments", f"@codex review\n\n{self.TAG}")])
        self.assertEqual(gh.posted, [])   # no bot note beside it, and no CODEX-ASK item for the integrator
        self.assertTrue(any(l.startswith("ASK-CODEX   #9") for l in sw.lines))
        self.assertFalse(any("t0ken" in l for l in sw.lines))

    def test_one_ask_per_head_whoever_posted_it(self):
        mine = {"body": f"@codex review\n\n{self.TAG}", "user": {"login": "IssisX"}, "created_at": ago(0.05)}
        gh, sw = self.steer([mine])
        self.assertEqual((gh.as_team, gh.posted), ([], []))
        self.assertTrue(any(l.startswith("WAIT-CODEX") for l in sw.lines))   # it has ten minutes to start
        typed = {"body": "@codex review", "user": {"login": "IssisX"}, "created_at": ago(1)}   # the integrator's own
        self.assertEqual(self.steer([typed])[0].as_team, [])
        old = dict(typed, created_at=ago(9))   # typed before this head was pushed (5 h ago): an older head's ask
        self.assertEqual(len(self.steer([old])[0].as_team), 1)
        forged = dict(mine, user={"login": "someone"})   # the repo is public: a stranger's mark counts for nothing
        self.assertEqual(len(self.steer([forged])[0].as_team), 1)
        other = dict(mine, body=f"@codex review\n\n{switch.MARK.format(kind='ask-codex', pr=9, head=OTHER_HEAD)}",
                     created_at=ago(9))
        self.assertEqual(len(self.steer([other])[0].as_team), 1)   # a new head gets its own ask

    def test_no_ask_while_codex_is_already_reviewing_the_head_or_when_its_land_would_not_count(self):
        running = CodexRunning.state(None, HEAD, "running")
        self.assertEqual(self.steer([running])[0].as_team, [])
        self.assertEqual(self.steer(head_ref="chatgpt/integrate-x")[0].as_team, [])   # same vendor: its LAND counts for nothing

    def test_without_the_token_or_when_github_refuses_it_the_integrator_is_asked_as_before(self):
        for gh, sw in (self.steer(token=""), self.steer(refuse=403)):
            self.assertEqual(gh.as_team, [])
            (where, body), = gh.posted
            self.assertEqual(where, 9)
            self.assertNotIn("@", body)
            self.assertIn(self.TAG, body)
            self.assertIn("The integrator posts the Codex review command", body)
        self.assertTrue(any("refused the team token" in l and "403" in l for l in sw.lines))

    def test_an_outage_is_not_a_refusal_the_pr_waits_for_the_next_sweep(self):
        with self.assertRaises(switch.GitHubError):
            self.steer(refuse=502)

    def test_a_pr_the_switch_just_opened_waits_for_codex_though_its_proof_went_green_long_ago(self):
        gh = FakeGH([])
        sw = make(gh, team=self.TEAM)
        sw.api_ok = lambda c: True
        sw.review_load = lambda: ({}, {})
        sw.stale = lambda facts: False
        reviewed = []
        sw.review = lambda facts, c, author, security=False: reviewed.append(facts.head_sha)
        c = dict(ctx(), proof_done_at=ago(6), created_at=ago(0.05))
        sw.ask_pr(pr_facts(head_ref="muse/integrate-x"), c)   # the ask part runs before the steer part asks Codex
        self.assertEqual(reviewed, [])
        sw.ask_pr(pr_facts(head_ref="muse/integrate-x"), dict(c, created_at=ago(0.5)))   # ten minutes on: the API
        self.assertEqual(reviewed, [HEAD])

    def test_the_clock(self):
        turn = rules.codex_turn
        self.assertTrue(turn([ago(0.1)], False, True, ago(0)))            # expected: ten minutes to start
        self.assertFalse(turn([ago(0.2)], False, True, ago(0)))           # not started in ten: no longer its turn
        self.assertTrue(turn([ago(0.4)], True, True, ago(0)))             # running: waited for
        self.assertFalse(turn([ago(0.6)], True, True, ago(0)))            # past thirty minutes: on without it
        self.assertFalse(turn([ago(0.1)], False, False, ago(0)))          # not expected and not running
        self.assertTrue(turn([ago(5), "", ago(0.1)], False, True, ago(0)))   # the clock runs from the latest
        self.assertFalse(turn(["", ""], True, True, ago(0)))              # nothing to count from

    def test_a_forks_pr_never_gets_the_team_logins_mention(self):
        gh, sw = self.steer(own_head=False)
        self.assertEqual((gh.as_team, [b for _, b in gh.posted if "ask-codex" in b]), ([], []))


class SettledCalls(unittest.TestCase):
    """J-072: #376's automatic review answered NOTE to have the owner confirm a call he had already made (her
    injuries persist, dead included; 11-05). The review is given CLAUDE.md's settled list."""

    def test_the_section_is_read_from_claude_md_up_to_the_next_heading(self):
        import claude_calls
        text = (HERE.parent.parent / "CLAUDE.md").read_text(encoding="utf-8")
        got = claude_calls.settled_calls(text)
        self.assertTrue(got.startswith("## Settled"), got[:40])
        self.assertIn("Her injuries persist", got)
        self.assertNotIn("## Test only", got)
        self.assertEqual(claude_calls.settled_calls("# x\n\n## Other\ntext\n"), "")

    def test_the_gameplay_review_carries_them_and_the_security_pass_does_not(self):
        import claude_calls
        for security in (False, True):
            gh = FakeGH([])
            gh.get_text = lambda path, accept: "diff --git a/x b/x"
            sw = make(gh, team=NoteLandsNothing.TEAM)
            sw.root = HERE.parent.parent
            seen = []
            sw.paid = lambda kind, job, call, system, content, schema, cap, key: seen.append((system, content))
            sw.job_cap = lambda c: ""
            sw.review(pr_facts(touches_workflows=security), ctx(), "meta", security=security)
            (system, content), = seen
            text = "".join(b["text"] for b in content)
            self.assertEqual("<settled_calls>" in text, not security)
            if not security:
                self.assertIn("Her injuries persist", text)
                self.assertIn("<settled_calls>", system)
                self.assertIs(system, claude_calls.REVIEW_SYSTEM)

