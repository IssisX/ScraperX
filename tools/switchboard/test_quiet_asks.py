"""#350 (J-072), two holes the switchboard let through:

A. 12 h with no Muse push and no mail either way while 13 Muse lanes were late raised nothing (run 43587341746):
   inbound_quiet needs a CLAUDE_TO_MUSE ask in its day. Now, while a Muse lane is in flight, no fb-bot push and no
   bridge post for MUSE_QUIET is one QUIET item that wakes the integrator, at most once a day while it lasts.
B. A TO-CLAUDE ask was listed once and forgotten, and the job machine then said the job "belongs to muse" (J-124
   waited 37 h on a ruling, J-078 42 h). Now an ask that names a job stays open until a later Claude comment or mail
   names the job, is listed again once per ASK_AGAIN, and the job's step waits on Claude's answer.

Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
"""
from __future__ import annotations

import datetime as dt
import pathlib
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import machine as M  # noqa: E402
import rules  # noqa: E402
import switch  # noqa: E402
import test_inbox as T  # noqa: E402
import test_machine as TM  # noqa: E402
import test_plan_sweep as TP  # noqa: E402

NOW = T.NOW
BOARD = switch.BOARD_ISSUE


def lane(job: str = "J-124", state: str = "pushed", owner: str = "muse") -> M.Step:
    """A Muse row in flight, late (pushed and red past its 3 h)."""
    return M.Step(job, owner, state, "red", owner, "push a head that fixes the red proof (x)", "x",
                  NOW - dt.timedelta(hours=37), M.hours(3))


def at(gh, hours: float):
    """The switch of a sweep `hours` after NOW, on the same GitHub and sweep state."""
    return T.make(gh, NOW + dt.timedelta(hours=hours))


def facts(ref: str, committed: str) -> switch.Steered:
    return T.steered(number=700, ref=ref, committed=committed)


class MuseQuiet(unittest.TestCase):
    def test_silence_with_no_claude_to_muse_ask_alarms_and_wakes_the_integrator(self):
        # the failed case: nothing from Muse and no CLAUDE_TO_MUSE ask in the day, 13 lanes late
        gh = T.FakeGH(comments=[T.comment(1, BOARD, "a person's note", 2.0)], open_issues=[BOARD])
        sw = T.make(gh)
        self.assertIsNone(sw.inbound_quiet(None))   # the old alarm stays silent here: the hole
        rows = [lane(f"J-{100 + i}") for i in range(13)]
        (item,) = sw.muse_quiet(rows, {"branches": [], "head_at": {}})
        self.assertEqual((item.kind, item.target, item.dest), ("QUIET", "muse", "claude"))
        self.assertEqual(switch.inbox_class(item), "hold")   # it wakes; it never rides the digest alone
        self.assertIn("no fb-bot push and no bridge post", item.line)
        self.assertIn("13 Muse lane(s) are in flight", item.line)
        self.assertIn("13 of them late", item.line)
        sw.plan_items = (item,)
        self.assertEqual(sw.inbox([]), 1)
        (number, body), = gh.posts
        self.assertEqual(number, switch.INBOX_PR)
        self.assertIn("QUIET Muse: no fb-bot push", body)

    def test_once_a_day_while_it_lasts_and_again_after_muse_is_heard(self):
        gh = T.FakeGH(open_issues=[BOARD])
        rows, reads = [lane()], {"branches": [], "head_at": {}}

        def sweep(hours: float, inbox: bool = True) -> int:
            """One full pass `hours` after NOW: the plan part, then (unless it failed) the inbox; posts made."""
            sw = at(gh, hours)
            sw.plan_items = sw.muse_quiet(rows, reads)
            before = len(gh.posts)
            if inbox:
                sw.inbox([])
            return len(gh.posts) - before

        self.assertEqual(sweep(0, inbox=False), 0)   # the inbox part failed this pass: nothing is lost
        self.assertEqual(sweep(1), 1)
        self.assertIn("QUIET Muse", gh.posts[-1][1])
        self.assertEqual(sweep(3), 0)
        self.assertEqual(sweep(23.9), 0)
        self.assertEqual(sweep(24), 1)               # a day on: once more
        # Muse posts; the silence ends, and a new one alarms once it is MUSE_QUIET long
        gh.comments.append(T.comment(5, BOARD, "**Muse (via email bridge): J-124 pushed**", -25.0))
        self.assertEqual(sweep(26), 0)
        self.assertNotIn("quiet_at", gh.state["inbox"])
        self.assertEqual(sweep(31.1), 1)

    def test_a_recent_push_or_bridge_post_is_no_alarm(self):
        rows = [lane()]
        post = T.comment(5, BOARD, "**Muse (via email bridge): J-124 status**", 2.0)
        self.assertEqual(T.make(T.FakeGH(comments=[post])).muse_quiet(rows, {}), ())
        head = {"branches": [{"name": "fb-bot/wip-j124", "commit": {"sha": "s1"}}],
                "head_at": {"s1": NOW - dt.timedelta(hours=3)}}
        self.assertEqual(T.make(T.FakeGH()).muse_quiet(rows, head), ())
        pr = facts("fb-bot/integrate-j124", T.ago(1))
        self.assertEqual(T.make(T.FakeGH()).muse_quiet(rows, {}, [pr]), ())
        # older than MUSE_QUIET, another agent's push, a post from a login outside the team: still quiet
        old = T.comment(6, BOARD, "**Muse (via email bridge): J-124 status**", 7.0)
        stranger = T.comment(7, BOARD, "**Muse (via email bridge): J-124 status**", 1.0, login="someone")
        sw = T.make(T.FakeGH(comments=[old, stranger]))
        (item,) = sw.muse_quiet(rows, {}, [facts("claude/integrate-j124", T.ago(1))])
        self.assertIn("for 7.0 h", item.line)

    def test_no_muse_lane_in_flight_is_no_alarm(self):
        gh = T.FakeGH()
        self.assertEqual(T.make(gh).muse_quiet([lane(state="acknowledged"), lane(owner="claude")], {}), ())
        self.assertEqual(gh.gets, [])   # and it reads nothing

    def test_it_reads_nothing_the_inbox_does_not(self):
        gh = T.FakeGH(open_issues=[BOARD])
        sw = T.make(gh)
        sw.ask_inbox_items(frozenset())
        before = len(gh.gets)
        sw.muse_quiet([lane()], {})
        self.assertEqual(len(gh.gets), before, gh.gets)


class OpenAsks(unittest.TestCase):
    ASK = "**Muse (via email bridge): MUSE_TO_CLAUDE: J-124 550.25 E, ruling needed**\n\nWhich way?"

    def gh(self, body=ASK, number=BOARD):
        return T.FakeGH(comments=[T.comment(42, number, body, 1.0)], open_issues=[number])

    def asks(self, sw, kind="TO-CLAUDE"):
        return [it for it in sw.ask_inbox_items(frozenset(T.ring(sw.gh))) if it.kind == kind]

    def test_an_unanswered_ask_stays_open_and_is_listed_again(self):
        gh = self.gh()
        self.assertEqual(T.make(gh).inbox([]), 1)                    # listed once, under its comment id
        self.assertEqual(list(gh.state["inbox"]["asks"]), ["42"])
        self.assertEqual(self.asks(at(gh, 2)), [])                   # not again before ASK_AGAIN
        (again,) = self.asks(at(gh, 30))                              # past the comment window: still open
        self.assertEqual((again.kind, again.target), ("TO-CLAUDE", f"#{BOARD}"))
        self.assertNotEqual(again.head, "42")
        self.assertIn("still open after 31.0 h", again.line)
        self.assertIn("J-124", again.line)
        self.assertEqual(switch.inbox_class(again), "now")
        self.assertEqual(self.asks(at(gh, 32)), [])                  # listed at 30 h: not again until 34 h
        self.assertEqual(len(self.asks(at(gh, 34))), 1)

    def test_a_claude_mail_naming_the_job_answers_it_and_a_bot_notice_does_not(self):
        gh = self.gh()
        T.make(gh).inbox([])
        gh.comments.append(T.comment(50, BOARD, "CLAUDE_TO_MUSE: plan 0123456789ab\n\nJ-124 is yours", -2.0,
                                     login=rules.BOT_LOGIN))
        self.assertIn("42", at(gh, 3).open_asks())
        gh.comments.append(T.comment(51, BOARD, "CLAUDE_TO_MUSE: J-124 ruling: 550.25 E is a geometry case", -3.0))
        sw = at(gh, 4)
        self.assertEqual(sw.open_asks(), {})
        self.assertTrue(any(line.startswith("ASK ") and "J-124" in line for line in sw.lines), sw.lines)
        self.assertEqual(self.asks(at(gh, 30)), [])
        self.assertEqual(gh.state["inbox"]["asks"], {})

    def test_a_claude_comment_answers_every_job_it_names_and_only_after_the_ask(self):
        gh = T.FakeGH(comments=[
            T.comment(40, 393, "Claude (integrator): J-078 old note\n\nGenerated by [Claude Code]", 3.0),
            T.comment(41, 393, "**Muse (via email bridge): J-078 FIX @ ed5cafad: no push, retitle needed**", 2.0),
            T.comment(42, BOARD, self.ASK, 1.0)], open_issues=[393, BOARD])
        self.assertEqual(sorted(T.make(gh).open_asks()), ["41", "42"])   # the older note answers nothing
        gh.comments.append(T.comment(60, 393, "Claude (integrator): J-078 merge trunk in; J-124 is a geometry "
                                              "case\n\nGenerated by [Claude Code]", -1.0))
        self.assertEqual(at(gh, 2).open_asks(), {})

    def test_an_ask_naming_no_job_is_listed_once_as_before(self):
        gh = self.gh(body="To Claude: can the board be split?")
        self.assertEqual(T.make(gh).inbox([]), 1)
        self.assertEqual(T.make(gh).open_asks(), {})
        self.assertEqual(self.asks(at(gh, 30)), [])

    def test_a_strangers_ask_is_listed_once_and_never_kept(self):
        # anyone can comment on the public repo; a kept ask would move a job's step to Claude
        gh = T.FakeGH(comments=[T.comment(42, BOARD, "To Claude: J-124 is yours now", 1.0, login="someone")],
                      open_issues=[BOARD])
        self.assertEqual([it.kind for it in T.make(gh).ask_inbox_items(frozenset())], ["TO-CLAUDE"])
        self.assertEqual(T.make(gh).open_asks(), {})

    def test_a_dry_run_keeps_no_ask(self):
        gh = self.gh()
        gh.dry_run = True
        self.assertIn("42", T.make(gh).open_asks())
        self.assertNotIn("asks", gh.state.get("inbox") or {})

    def test_the_open_asks_read_nothing_the_inbox_does_not(self):
        gh = self.gh()
        sw = T.make(gh)
        sw.ask_inbox_items(frozenset())
        before = len(gh.gets)
        sw.open_asks()
        sw.ask_jobs()
        self.assertEqual(len(gh.gets), before, gh.gets)


class WaitsOnClaude(unittest.TestCase):
    """The job machine: an open ask on a job moves the step its owner owes to Claude; state and gate stay."""

    def rows(self, asks=None):
        w = TM.World()
        w.tell(1)
        w.muse(2, f"J-101 note, plan {w.version()}")
        w.branches.append(TM.br("fb-bot/integrate-j101", "h1"))
        w.works["h1"] = M.branch_work("fb-bot/integrate-j101", "h1", "red: native", TM.at(3))
        reads = {"issues": w.issues, "branches": w.branches, "prs": w.prs, "inactive": frozenset(), "closed": frozenset(),
                 "merged_at": {}, "head_at": {}}
        return w.model, M.table(w.model, M.collect(w.model, reads, works=w.works, mail=w.mail(), planned_at=TM.T0,
                                                   asks=asks))

    def test_an_unanswered_ask_changes_who_owes_the_next_step(self):
        model, plain = self.rows()
        before = {r.job: r for r in plain}["J-101"]
        self.assertEqual((before.state, before.gate, before.owes), ("pushed", "red", "muse"))
        self.assertIn("belongs to muse", before.alarm(TM.at(40)))
        model, asked = self.rows({"J-101": (TM.at(5), "https://example.invalid/ask")})
        r = {r.job: r for r in asked}["J-101"]
        self.assertEqual((r.state, r.gate, r.owes, r.since, r.limit),
                         ("pushed", "red", "integrator", TM.at(5), M.ASK_LIMIT))
        line = r.alarm(TM.at(40))
        self.assertIn("waits on Claude's answer", line)
        self.assertNotIn("belongs to muse", line)
        self.assertIn(before.next, r.next)                        # Muse's own step comes after the answer
        self.assertIn("integrator", switch.PLAN_WAKES)            # so its PLAN line wakes the integrator
        self.assertEqual(M.wip(model, asked), M.wip(model, plain))   # the lanes read it as before
        self.assertEqual(M.queue(model, asked, "muse"), M.queue(model, plain, "muse"))

    def test_a_lettered_ask_is_its_base_jobs_and_a_held_or_claude_job_is_left_alone(self):
        _, asked = self.rows({"J-101a": (TM.at(5), "u"), "J-105": (TM.at(5), "u"), "J-201": (TM.at(5), "u")})
        rows = {r.job: r for r in asked}
        self.assertEqual(rows["J-101"].owes, "integrator")
        self.assertEqual((rows["J-105"].state, rows["J-105"].owes), ("held", ""))
        self.assertEqual(rows["J-201"].owes, "claude")


class AskInThePlanPass(TP.Sweeps):
    """The wiring: the plan pass reads the open asks the inbox keeps (no read of its own) into the machine."""

    def test_a_muse_ask_on_the_job_moves_its_step_to_claude_until_claude_names_it(self):
        hub = self.hub
        self.sweep(0.5)                                            # the notice goes out: J-501 is told, Muse owes
        hub.mail(0.8, "MUSE_TO_CLAUDE: J-501 which grip cap? ruling needed", number=501)
        _, rows = self.sweep(1)
        r = rows["J-501"]
        self.assertEqual((r.state, r.owes), ("told", "integrator"))
        self.assertIn("waits on Claude's answer", r.next)
        again = M.NOTICE_AGAIN.format(owner="muse", ver=self.version())
        _, rows = self.sweep(3)   # told past its 2 h, but on Claude's clock: the notice is not sent again
        self.assertEqual(rows["J-501"].owes, "integrator")
        self.assertEqual(hub.by(rules.BOT_LOGIN, again), [])
        hub.post(501, "Claude (integrator): J-501 ruling: 8 kN, as the budget says", "IssisX", TP.iso(3.2))
        _, rows = self.sweep(3.5)
        self.assertEqual((rows["J-501"].state, rows["J-501"].owes), ("told", "muse"))
        self.assertEqual(len(hub.by(rules.BOT_LOGIN, again)), 1)   # Muse's clock again: the one re-send

    def test_the_plan_pass_lists_a_quiet_muse(self):
        hub = self.hub
        self.sweep(0.5)
        hub.mail(1, f"MUSE_TO_CLAUDE: plan {self.version()} read")
        hub.push(TP.WIP, TP.W1, 2)
        sw, rows = self.sweep(2.1)
        self.assertEqual(rows["J-501"].state, "started")
        self.assertEqual([i for i in sw.plan_items if i.kind == "QUIET"], [])
        sw, _ = self.sweep(8.5)                                    # 6.5 h since the push, 7.5 h since the mail
        (quiet,) = [i for i in sw.plan_items if i.kind == "QUIET"]
        self.assertIn("for 6.5 h while 1 Muse lane(s) are in flight (J-501)", quiet.line)
        hub.mail(9, "J-501 status")
        sw, _ = self.sweep(9.5)
        self.assertEqual([i for i in sw.plan_items if i.kind == "QUIET"], [])


if __name__ == "__main__":
    unittest.main()
