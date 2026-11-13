"""The collaboration holes closed on 11-13 (claude/integrate-system-gaps), one class each:

RedTrunk     trunk's full proof red pages its owners at once and stays on the board until a green full proof (J-129's
             doubled order key 1100 sat red about a day, hidden by notes-only pushes whose runs skip the build).
AskLate      an ask to Claude still open past 1 h reaches its inbox again, once.
Waiting      a job whose owner's ask is open shows "waiting on <Name>'s answer" on the board and as `state:waiting`.
Notices      Grok and Builder are told their plan order by a comment their runs read, and reply with the receipt;
             every notice to Grok Bot opens `To Builder:` (it never read the `To PE:` the reconciler sent on #350).
Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
No network, no clock, no files beyond a temporary checkout: GitHub and the time are given.
"""

from __future__ import annotations

import datetime as dt
import pathlib
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.append(str(HERE.parent / "board"))   # after this dir: tools/board has a plan.py too

import asks  # noqa: E402
import ledger as L  # noqa: E402
import machine as M  # noqa: E402
import plan  # noqa: E402
import rules  # noqa: E402
import switch  # noqa: E402
import test_inbox as ti  # noqa: E402
import test_plan_sweep as tps  # noqa: E402

NOW = ti.NOW
TRUNK = rules.TRUNK
BROKE = "b" * 40      # the merge that broke trunk
NOTES = "c" * 40      # a notes-only push after it
FIXED = "d" * 40      # the fix's merge
BUILT_RED = [{"name": "native-tests", "status": "completed", "conclusion": "failure"},
             {"name": "godot-touch", "status": "completed", "conclusion": "success"}]
SKIPPED = [{"name": "native-tests", "status": "completed", "conclusion": "skipped"},
           {"name": "godot-touch", "status": "completed", "conclusion": "skipped"},
           {"name": "scope", "status": "completed", "conclusion": "success"}]
BUILT_GREEN = [{"name": "native-tests", "status": "completed", "conclusion": "success"},
               {"name": "godot-touch", "status": "completed", "conclusion": "success"}]


class TrunkGH(ti.FakeGH):
    """The inbox's GitHub, with trunk's commits (their merge messages) readable."""

    def __init__(self, *a, messages=None, **kw):
        super().__init__(*a, **kw)
        self.messages = dict(messages or {})

    def route(self, path):
        base = path.split("?")[0]
        if "/commits/" in base and base.rsplit("/", 1)[1] in self.messages:
            self.gets.append(path)
            return {"sha": base.rsplit("/", 1)[1], "commit": {"message": self.messages[base.rsplit("/", 1)[1]]}}
        return super().route(path)


def roster() -> L.Roster:
    return L.Roster({"agents": {"claude": {"names": ["Claude"], "branch_prefixes": ["claude/"]},
                                "muse": {"names": ["Muse", "FB"], "branch_prefixes": ["muse/", "fb-bot/"]},
                                "grok_bot": {"names": ["PE", "Builder"], "address": "Builder",
                                             "branch_prefixes": ["pe/", "builder/"]}}})


class RedTrunk(unittest.TestCase):
    def gh(self, runs, jobs, state=None):
        gh = TrunkGH(runs=runs, jobs=jobs, open_issues=[340],
                     messages={BROKE: "Merge pull request #490 from IssisX/fb-bot/integrate-j129-objective-core\n\nJob"})
        gh.state = dict(state or {})
        return gh

    def test_a_red_full_trunk_proof_wakes_the_integrator_and_mails_the_breakers_owner_at_once(self):
        gh = self.gh([ti.run(41, TRUNK, BROKE, 5)], {41: BUILT_RED})
        sw = ti.make(gh)
        self.assertEqual(sw.inbox([]), 1)                      # at once: no hold, no digest
        (number, body), = gh.posts
        self.assertEqual(number, switch.INBOX_PR)
        self.assertIn("- RED-TRUNK trunk's full proof is red on native-tests", body)
        self.assertIn("the merge of #490 (`fb-bot/integrate-j129-objective-core`)", body)
        kept = gh.state["trunk_proof"]
        self.assertEqual((kept["state"][:3], kept["first_red"], kept["pr"]), ("red", BROKE, 490))
        # the owner of the commit that broke it: Muse's PR, so Muse is mailed too (comment + wake:muse on the board)
        self.assertEqual(ti.make(gh).muse([]), 1)
        (where, mail), = [p for p in gh.posts if p[0] != switch.INBOX_PR]
        self.assertEqual(where, switch.BOARD_ISSUE)
        self.assertTrue(mail.startswith("CLAUDE_TO_MUSE: "), mail)
        self.assertIn("RED-TRUNK", mail)
        self.assertIn(("POST", f"repos/{gh.repo}/issues/{switch.BOARD_ISSUE}/labels", {"labels": [switch.MUSE_LABEL]}),
                      gh.writes)
        self.assertEqual(ti.later(gh).inbox([]), 0)            # once per red streak

    def test_a_notes_only_green_run_never_clears_it_and_a_green_full_proof_does(self):
        gh = self.gh([ti.run(41, TRUNK, BROKE, 5)], {41: BUILT_RED})
        ti.make(gh).inbox([])
        gh.runs.append(ti.run(42, TRUNK, NOTES, 3, conclusion="success"))
        gh.jobs[42] = SKIPPED
        sw = ti.later(gh, 60)
        self.assertEqual(sw.inbox([]), 0)                      # the same streak: listed once
        self.assertEqual((sw.trunk_proof(gh.runs)["state"][:3], gh.state["trunk_proof"]["first_red"]), ("red", BROKE))
        gh.runs.append(ti.run(43, TRUNK, FIXED, 1, conclusion="success"))
        gh.jobs[43] = BUILT_GREEN
        sw = ti.later(gh, 120)
        self.assertEqual(sw.trunk_items(gh.runs, frozenset()), [])
        self.assertEqual((gh.state["trunk_proof"]["state"], gh.state["trunk_proof"]["sha"]), ("green", FIXED))

    def test_a_red_older_than_the_runs_window_stays_red_from_the_state(self):
        kept = {"sha": BROKE, "run": 41, "url": "u", "state": "red: native-tests", "at": ti.ago(30),
                "created": ti.ago(31), "first_red": BROKE, "pr": 490, "branch": "fb-bot/integrate-j129-objective-core"}
        gh = self.gh([ti.run(42, TRUNK, NOTES, 3, conclusion="success")], {42: SKIPPED}, {"trunk_proof": kept})
        (it,) = ti.make(gh).trunk_items(gh.runs, frozenset())
        self.assertEqual((it.kind, it.dest, it.head), ("RED-TRUNK", "both", BROKE))

    def test_claudes_own_trunk_push_pages_only_the_integrator(self):
        gh = self.gh([ti.run(41, TRUNK, "e" * 40, 5)], {41: BUILT_RED})
        (it,) = ti.make(gh).trunk_items(gh.runs, frozenset())
        self.assertEqual(it.dest, "claude")
        self.assertEqual(ti.make(gh).muse_items([], frozenset()), [])

    def test_the_board_keeps_the_trunk_line_until_a_green_full_proof(self):
        red = {"sha": NOTES, "run": 41, "url": "https://example/41", "state": "red: native-tests", "at": ti.ago(20),
               "first_red": BROKE, "pr": 490, "branch": "fb-bot/integrate-j129-objective-core"}
        runs = [L.RunIn(41, "u41", TRUNK, BROKE, NOW - dt.timedelta(hours=21), NOW - dt.timedelta(hours=20), "failure"),
                L.RunIn(42, "u42", TRUNK, NOTES, NOW - dt.timedelta(hours=3), NOW - dt.timedelta(hours=3), "success")]

        def trunk(tp):
            feed = L.Feed([], [], [], [], [], runs, trunk_proof=tp)
            return [it for it in L.classify(feed, NOW, roster()).items if it.kind == "TRUNK"]

        self.assertEqual(len(trunk(None)), 0)                  # before: the notes-only run hid the red one
        (it,) = trunk(red)
        self.assertIn("since bbbbbbbb (the merge of #490): until a green full proof", it.state)
        self.assertEqual((it.owner.agent, it.holder.agent, it.version), ("muse", "claude", BROKE))
        self.assertEqual(trunk(dict(red, state="green")), [])


class AskLate(unittest.TestCase):
    def ask(self, hours: float, target: str = "Claude", body: str = "To Claude: which head lands next?") -> dict:
        return {"a": "Muse", "target": target, "n": 307, "t": NOW - dt.timedelta(hours=hours), "id": "6850000001",
                "url": "https://github.com/IssisX/ScraperX/issues/307#issuecomment-6850000001", "body": body,
                "login": "IssisX"}

    def make(self):
        sw = ti.make(ti.FakeGH())
        sw.agent_of_name = lambda name: name.lower()
        return sw

    def test_an_ask_to_claude_past_an_hour_is_listed_once_at_once(self):
        sw = self.make()
        self.assertEqual(sw.ask_late_items([self.ask(0.5)]), ())            # inside its hour
        (it,) = sw.ask_late_items([self.ask(1.5)])
        self.assertEqual((it.kind, it.target, it.head, switch.inbox_class(it)), ("ASK-LATE", "#307", "6850000001", "now"))
        self.assertIn("ASK-LATE #307 Muse asked Claude 1.5 h ago and has no answer", it.line)
        self.assertEqual(sw.ask_late_items([self.ask(30)]), ())             # past the day: the reconciler's line to Cory
        self.assertEqual(sw.ask_late_items([self.ask(2, target="Grok")]), ())   # Grok's: the reconciler sends it
        gh = ti.FakeGH()
        sw = ti.make(gh)
        sw.ask_items = self.make().ask_late_items([self.ask(1.5)])
        self.assertEqual(sw.inbox([]), 1)
        self.assertEqual(ti.later(gh).inbox([]), 0)

    def test_muses_tagged_subject_is_still_an_ask(self):
        # 11-13 (work/tasks/muse.md): subjects carry ASK or FYI right after the job; the judge missed every such ask
        for subject, target in (("J-002 ASK To Claude: which head?", "Claude"), ("#384 J-114 ASK To Grok: why?", "Grok"),
                                ("J-021 FYI To Claude: pushed", "Claude")):
            body = f"**Muse (via email bridge): {subject}**\n\nBody."
            self.assertEqual([m.group(1) for m in asks.ASK.finditer(body)], [target], subject)
        self.assertEqual(list(asks.ASK.finditer("**Muse (via email bridge): J-002 ASK second hit sounds**")), [])

    def test_only_a_question_or_blocked_waits_on_an_answer(self):
        self.assertTrue(asks.blocking("To Claude: which head?"))
        self.assertTrue(asks.blocking("To Claude: BLOCKED: the rail, needs a ruling"))
        self.assertFalse(asks.blocking("To Claude: J-021 pushed, CI dispatched. https://x/y?z=1 `a?b`"))


class Waiting(unittest.TestCase):
    PLAN = tps.PLAN

    def test_the_owners_open_ask_on_its_job_is_what_the_job_waits_on(self):
        model = plan.parse(self.PLAN)
        t = NOW - dt.timedelta(hours=2)
        opened = [{"a": "Muse", "target": "Claude", "n": 501, "t": t, "url": "https://x/501#c1", "body": "To Claude: which?"},
                  {"a": "Muse", "target": "Claude", "n": 340, "t": t, "url": "https://x/340#c2",
                   "body": "J-502 To Claude: may I?"},                     # on the board, its opening names the job
                  {"a": "Muse", "target": "Claude", "n": 601, "t": t, "url": "u", "body": "To Claude: which?"},   # not J-601's owner
                  {"a": "Muse", "target": "Muse", "n": 501, "t": t, "url": "u", "body": "To Muse: self?"}]
        got = M.waits(model, opened, {501: "J-501", 502: "J-502", 601: "J-601"}, lambda n: n.lower())
        self.assertEqual(sorted(got), ["J-501", "J-502"])
        self.assertEqual(M.wait_text(got["J-501"], NOW), "waiting on Claude's answer ([ask](https://x/501#c1), 2 h)")


class Sweep(tps.Sweeps):
    """The plan part of a full pass, end to end on the in-memory GitHub."""
    mode = "live"

    def test_a_job_waiting_on_an_answer_shows_it_and_the_late_ask_reaches_claude(self):
        hub = self.hub
        mail = hub.mail(0.2, "J-501 ASK To Claude: which head lands first?", "The two heads disagree.", number=501)
        mail["html_url"] = "https://github.com/IssisX/ScraperX/issues/501#issuecomment-1"
        sw, rows = self.sweep(0.5)
        self.assertIn("state:waiting", hub.labels(501))
        body = hub.issues[tps.BOARD]["body"]
        self.assertIn("- J-501 #501 planned; waiting on Claude's answer ([ask](https://github.com/IssisX/ScraperX/"
                      "issues/501#issuecomment-1), 18 min)", body)
        self.assertEqual(sw.ask_items, ())                      # inside its hour
        sw, rows = self.sweep(1.5)
        (it,) = sw.ask_items
        self.assertEqual((it.kind, it.target), ("ASK-LATE", "#501"))
        hub.post(501, "To Muse: J-501 lands first.\n\nGenerated by [Claude Code]", "IssisX", tps.iso(1.6))
        sw, rows = self.sweep(2)
        self.assertNotIn("state:waiting", hub.labels(501))      # answered: the label and the line go
        self.assertNotIn("waiting on", hub.issues[tps.BOARD]["body"])
        self.assertEqual(sw.ask_items, ())

    def test_grok_is_told_by_a_board_comment_with_no_mail_and_its_reply_acknowledges(self):
        self.write("work/plan.md", tps.PLAN + "limit grok 1\nJ-701 grok #701 | Grok's job\n")
        self.hub.issue(701, "J-701: Grok's job", ["job"])
        ver = M.versions(plan.parse(tps.PLAN + "limit grok 1\nJ-701 grok #701 | Grok's job\n"))["grok"]
        sw, rows = self.sweep(0.5)
        (notice,) = self.hub.by(rules.BOT_LOGIN, f"To Grok: plan {ver}")
        self.assertTrue(notice["body"].startswith(f"To Grok: plan {ver}\n"))
        self.assertTrue(notice["issue_url"].endswith(f"/{tps.BOARD}"))
        self.assertIn(M.NOTICE_MARK.format(owner="grok", ver=ver, jobs="")[:40], notice["body"])
        self.assertNotIn(("POST", f"issues/{tps.BOARD}/labels"),
                         [w for w in self.hub.writes if "wake:muse" in str(w)])
        sw, rows = self.sweep(1)
        self.assertEqual((rows["J-701"].state, rows["J-701"].gate, rows["J-701"].owes), ("told", "reply", "grok"))
        self.hub.post(tps.BOARD, f"To Claude (from Grok): plan {ver} read; J-701 next.", "IssisX", tps.iso(2))
        sw, rows = self.sweep(3)
        self.assertEqual(rows["J-701"].state, "acknowledged")
        self.assertIn(f"owner=grok ver={ver}", "".join(c["body"] for c in self.hub.by(rules.BOT_LOGIN, "plan-receipt")))
        sw, rows = self.sweep(30)                               # the reply left the day's window: the mark holds it
        self.assertEqual(rows["J-701"].state, "acknowledged")


class Notices(unittest.TestCase):
    def test_grok_bot_is_addressed_by_the_name_its_runs_read(self):
        self.assertEqual(roster().address("grok_bot"), "Builder")
        self.assertEqual(roster().address("muse"), "Muse")
        real = L.Roster(switch.load_yaml(HERE.parent.parent / "work/agents.yml"))
        self.assertEqual(real.address("grok_bot"), "Builder")


if __name__ == "__main__":
    unittest.main()
