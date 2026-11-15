"""#350 (J-072): the switchboard fixes an outside review ranked first and third (03_EXECUTION/SYSTEM_DIAGNOSIS_11-14.md
sections 6 and 8 to 10), each read against the code before it was changed.

1. A job with an open PR takes its state from its PRs: a dead side head with no PR never shadows one (J-014 #491,
   J-040 #472), and a held job whose only PRs are drafts reads as held (J-015, draft #466, read "late").
2. A late step the switchboard owes itself wakes the integrator (J-049, J-072 reached nobody).
3. A named commit GitHub was seen to have is kept in the sweep state, so each pass reads only unseen ones (78 past
   the 10-read bound every pass; J-077's be74b436 was never reached).
4. A tripped Muse mail pipe gets one PIPE OK once it is healthy again (Muse believed it down for 36 h).
5. An AUTO-PR-FAILED head older than 7 days is listed once, and never again after the inbox's ring loses its key.
6. A plan job whose issue title is not `J-NNN ` first is flagged: the mail bridge finds a job issue only so.

Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
"""
from __future__ import annotations

import dataclasses
import datetime as dt
import pathlib
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import machine as M  # noqa: E402
import mail_reader  # noqa: E402
import plan  # noqa: E402
import rules  # noqa: E402
import switch  # noqa: E402
import test_auto_pr as TA  # noqa: E402
import test_inbox as ti  # noqa: E402
import test_mail_reader as TMR  # noqa: E402
import test_machine as TM  # noqa: E402
import test_plan_sweep as TP  # noqa: E402
import test_switchboard as TS  # noqa: E402


class PrOverSideHead(unittest.TestCase):
    """1. machine._step: the job's state comes from its open PRs when it has any."""

    def world(self):
        w = TM.World()
        w.branches.append(TM.br("fb-bot/integrate-j101-side", "s1"))   # a dead side head: red, no PR
        w.works["s1"] = M.branch_work("fb-bot/integrate-j101-side", "s1", "red: athletic-traversal", TM.at(1))
        w.branches.append(TM.br("fb-bot/integrate-j101", "h1"))
        w.prs.append({"number": 900, "state": "open", "body": "Job: J-101\n", "draft": False,
                      "head": {"ref": "fb-bot/integrate-j101", "sha": "h1"}})
        return w

    def test_a_side_head_with_no_pr_never_shadows_the_open_pr(self):   # J-014 #491, J-040 #472
        w = self.world()
        w.works["h1"] = M.Work("fb-bot/integrate-j101", "h1", "green", land=True, at=TM.at(5), pr=900)
        r = w.rows()["J-101"]
        self.assertEqual((r.state, r.gate, r.owes, r.next), ("reviewed", "", "switchboard", "merge #900"))
        self.assertNotIn("athletic-traversal", r.next + r.why)

    def test_the_prs_own_red_is_still_the_jobs(self):
        w = self.world()
        w.works["h1"] = M.Work("fb-bot/integrate-j101", "h1", "red: host-build", gate="red", why="host-build",
                               at=TM.at(5), pr=900)
        r = w.rows()["J-101"]
        self.assertEqual((r.state, r.gate, r.why), ("pushed", "red", "host-build"))

    def test_with_no_pr_the_side_head_still_counts(self):
        w = self.world()
        w.prs.clear()
        w.works["h1"] = M.branch_work("fb-bot/integrate-j101", "h1", "green", TM.at(5), auto_pr=True)
        r = w.rows()["J-101"]
        self.assertEqual((r.state, r.gate, r.why), ("pushed", "red", "athletic-traversal"))   # the least advanced head


class HeldDraft(unittest.TestCase):
    """1. A held job whose only PRs are drafts is held: no lane, no clock (J-015, draft #466)."""

    def world(self, draft: bool, gate: str = "red"):
        w = TM.World()   # J-105 is held: "Cory parked it"
        w.branches.append(TM.br("fb-bot/integrate-j105", "h5"))
        w.prs.append({"number": 905, "state": "open", "body": "Job: J-105\n", "draft": draft,
                      "head": {"ref": "fb-bot/integrate-j105", "sha": "h5"}})
        w.works["h5"] = M.Work("fb-bot/integrate-j105", "h5", "red: x" if gate == "red" else "green", gate=gate,
                               why="x" if gate == "red" else "", at=TM.at(1), pr=905)
        return w

    def test_a_held_job_with_only_a_draft_pr_is_held(self):
        for gate in ("red", "draft"):   # work_of names a red draft's red first, and a green one's draft gate
            w = self.world(True, gate)
            r = w.rows()["J-105"]
            self.assertEqual((r.state, r.owes, r.limit), ("held", "", None), gate)
            self.assertIn("Cory parked it", r.next)
            self.assertFalse(r.late(TM.at(400)))
            self.assertEqual(M.wip(w.model, w.table)["muse"][0], 0)   # it holds no lane

    def test_a_ready_pr_of_a_held_job_still_lands_on_its_own_proof(self):
        r = self.world(False).rows()["J-105"]
        self.assertEqual((r.state, r.gate, r.owes), ("pushed", "red", "muse"))

    def test_a_draft_of_a_job_not_held_is_its_pr_as_before(self):
        w = TM.World()
        w.branches.append(TM.br("fb-bot/integrate-j101", "h1"))
        w.prs.append({"number": 900, "state": "open", "body": "Job: J-101\n", "draft": True,
                      "head": {"ref": "fb-bot/integrate-j101", "sha": "h1"}})
        w.works["h1"] = M.Work("fb-bot/integrate-j101", "h1", "green", gate="draft", at=TM.at(1), pr=900)
        r = w.rows()["J-101"]
        self.assertEqual((r.state, r.gate, r.next), ("proven", "draft", "mark #900 ready for review"))

    def test_work_of_carries_the_draft(self):
        team, ready = TP.TEAM, TP.Sweeps.pr(None, 6.8).facts   # pr() reads nothing of its test
        for f in (ready, dataclasses.replace(ready, draft=True)):
            d = rules.merge_decision(f, team.vendor_of_branch(f.head_ref), team, True, True)
            self.assertEqual(M.work_of(f, d, team).draft, f.draft)


class SwitchboardStepWakes(TP.Sweeps):
    """2. A late step the switch owes itself reaches the integrator's inbox as a wake item (PLAN_WAKES)."""

    def test_a_green_head_the_switch_did_not_open_is_a_wake_item_once_late(self):
        hub = self.hub
        hub.push(TP.WIP, TP.W1, 5)
        hub.push(TP.INTEGRATE, TP.I1, 6)
        hub.proofs[TP.I1] = ("green", "", 1, TP.iso(6.5))
        sw, rows = self.sweep(6.6)
        self.assertEqual((rows["J-501"].gate, rows["J-501"].owes), ("auto-pr", "switchboard"))
        self.assertFalse([i for i in sw.plan_items if i.target == "J-501"])   # inside its hour: nothing
        sw, rows = self.sweep(8)
        (item,) = [i for i in sw.plan_items if i.target == "J-501"]
        self.assertIn("belongs to switchboard", item.line)
        self.assertEqual((item.dest, switch.inbox_class(item)), ("switchboard", "hold"))   # it wakes the integrator
        gh = ti.FakeGH()
        inbox = ti.make(gh, ti.NOW)
        inbox.plan_items = (item,)
        self.assertEqual(inbox.inbox([]), 1)
        self.assertIn("belongs to switchboard", gh.posts[0][1])


class KeptCommits(unittest.TestCase):
    """3. mail_reader._missing: a commit GitHub was seen to have is not read again on a later pass."""
    NOW = "2026-11-12T20:00:00Z"

    def setup(self, n_present: int):
        jobs = n_present + 1
        text = "limit muse 40\n" + "".join(f"J-{500 + i} muse #{500 + i} | job {i}\n" for i in range(jobs))
        model = plan.parse(text)
        # job 0's mail is the oldest and names a commit GitHub does not have (J-077's be74b436, local only)
        posts = [TMR.comment(10 + i, 500 + i, f"**Muse (via email bridge): J-{500 + i} pushed**\n\nHead {i:04d}abcdef0.",
                             f"2026-11-12T19:{i:02d}:00Z") for i in range(jobs)]
        gh = TMR.GH(posts, absent=["0000abcdef0"])
        gh.state = {}
        reads = {"issues": [TMR.issue(500 + i, f"J-{500 + i} job") for i in range(jobs)]}
        return gh, model, reads

    def test_a_later_pass_reads_only_the_shas_it_never_saw_and_reaches_the_missing_one(self):
        n = mail_reader.SHA_READS + 3
        gh, model, reads = self.setup(n)
        first = mail_reader.read(TMR.make(gh, self.NOW), model, reads)
        self.assertEqual(len(gh.commits_read()), mail_reader.SHA_READS)
        self.assertEqual(first.missing, {})   # the missing one is past the bound: unchecked, never counted
        self.assertEqual(len(gh.state["mail_commits"]), mail_reader.SHA_READS)
        gh.gets.clear()
        second = mail_reader.read(TMR.make(gh, self.NOW), model, reads)   # a new run: only the sweep state carries
        self.assertEqual(sorted(gh.commits_read()), sorted([f"{i:04d}abcdef0" for i in range(4)]))
        self.assertEqual(second.missing, {"J-500": "0000abcdef0"})
        self.assertNotIn("0000abcdef0", gh.state["mail_commits"])   # a 404 is never kept: Muse can push it later
        gh.gets.clear()
        mail_reader.read(TMR.make(gh, self.NOW), model, reads)
        self.assertEqual(gh.commits_read(), ["0000abcdef0"])   # steady state: the one missing sha alone
        # the calls saved: without the ring every pass reads SHA_READS (10); with it, only what it never saw
        self.assertLess(len(gh.commits_read()), mail_reader.SHA_READS)

    def test_the_ring_is_bounded_and_bad_entries_are_dropped(self):
        gh, model, reads = self.setup(2)
        gh.state["mail_commits"] = ["NOT HEX", 7] + [f"{i:07x}" for i in range(mail_reader.SHA_KEPT + 5)]
        mail_reader.read(TMR.make(gh, self.NOW), model, reads)
        kept = gh.state["mail_commits"]
        self.assertEqual(len(kept), mail_reader.SHA_KEPT)
        self.assertNotIn("NOT HEX", kept)
        self.assertEqual(kept[-2:], ["0002abcdef0", "0001abcdef0"])   # newest mail read first, newest at the end
        self.assertEqual(kept[0], f"{7:07x}")                          # the oldest seven went over the bound

    def test_no_state_reads_as_before(self):   # a client with no sweep state (a dry tool run) persists nothing
        gh, model, reads = self.setup(2)
        del gh.state
        mail_reader.read(TMR.make(gh, self.NOW), model, reads)
        self.assertEqual(len(gh.commits_read()), 3)


class StatefulPipe(TS.PipeGH):
    def __init__(self, *a, dry_run=False, **k):
        super().__init__(*a, **k)
        self.state, self.dry_run, self.writes = {}, dry_run, []

    def write(self, method, path, body=None, what=""):
        self.writes.append((method, path, body))
        return {}


class PipeAllClear(unittest.TestCase):
    """4. pipe_health: the first healthy pass after a tripped one posts one PIPE OK that the bridge mails Muse."""

    def stuck(self, gh):
        gh.wake = {343: [TS.label_event("labeled", 45)]}

    def test_one_pipe_ok_once_healthy_again_and_none_after(self):
        gh = StatefulPipe(comments=[TS.pipe_comment(TS.BRIDGE, 30, login="IssisX")])
        self.stuck(gh)
        self.assertEqual(len(TS.pipe_switch(gh).pipe_health()), 1)   # the alarm
        self.assertEqual(gh.state["pipe_trip"], {"outbound": switch.iso_z(TS.PIPE_NOW)})
        self.assertEqual(len(gh.posted), 1)
        TS.pipe_switch(gh, TS.PIPE_NOW + dt.timedelta(minutes=15)).pipe_health()   # still down: nothing more
        self.assertEqual((len(gh.posted), gh.writes), (1, []))
        gh.wake = {}   # the bridge mailed it and took the label off
        sw = TS.pipe_switch(gh, TS.PIPE_NOW + dt.timedelta(minutes=30))
        self.assertEqual(sw.pipe_health(), [])
        (number, body) = gh.posted[-1]
        self.assertEqual(number, switch.BOARD_ISSUE)
        self.assertTrue(body.startswith(f"CLAUDE_TO_MUSE: PIPE OK on #{switch.BOARD_ISSUE}:"))   # the bridge mails it
        self.assertIn(switch.PIPE_OK_MARK, body)
        self.assertIsNone(switch.VERDICT_TRIGGER.search(body))
        self.assertEqual(gh.writes, [("POST", f"repos/{gh.repo}/issues/{switch.BOARD_ISSUE}/labels",
                                      {"labels": [switch.MUSE_LABEL]})])
        self.assertNotIn("pipe_trip", gh.state)
        self.assertTrue(any(line.startswith("PIPE OK") for line in sw.lines))
        TS.pipe_switch(gh, TS.PIPE_NOW + dt.timedelta(minutes=45)).pipe_health()   # healthy again: no second one
        self.assertEqual((len(gh.posted), len(gh.writes)), (2, 1))

    def test_an_inbound_alarm_ends_only_on_a_bridge_post_not_when_its_asks_age_out(self):
        gh = StatefulPipe(comments=[TS.pipe_comment(TS.BRIDGE, 26 * 60, login="IssisX"), TS.pipe_comment(TS.ASK, 120)])
        (line,) = TS.pipe_switch(gh).pipe_health()
        self.assertTrue(line.startswith("INBOUND STUCK"))
        self.assertEqual(list(gh.state["pipe_trip"]), ["inbound"])
        sw = TS.pipe_switch(gh, TS.PIPE_NOW + dt.timedelta(hours=30))   # the ask is a day old: no finding, no proof
        self.assertEqual(sw.pipe_health(), [])
        self.assertEqual((len(gh.posted), gh.writes), (1, []))
        self.assertIn("pipe_trip", gh.state)
        self.assertTrue(any("no all-clear yet" in ln for ln in sw.lines), sw.lines)
        gh.comments.append(TS.pipe_comment(TS.BRIDGE, -31 * 60, login="IssisX"))   # Muse's mail reaches GitHub again
        self.assertEqual(TS.pipe_switch(gh, TS.PIPE_NOW + dt.timedelta(hours=31.5)).pipe_health(), [])
        self.assertEqual(len(gh.posted), 2)
        self.assertIn("PIPE OK", gh.posted[-1][1])
        self.assertNotIn("pipe_trip", gh.state)

    def test_a_kind_tripped_later_keeps_its_own_time(self):
        gh = StatefulPipe(comments=[TS.pipe_comment(TS.BRIDGE, 30, login="IssisX")])
        self.stuck(gh)
        TS.pipe_switch(gh).pipe_health()
        gh.comments += [TS.pipe_comment(TS.ASK, -20 * 60)]   # an ask a day after the last bridge post
        TS.pipe_switch(gh, TS.PIPE_NOW + dt.timedelta(hours=26)).pipe_health()   # outbound still, inbound too now
        self.assertEqual(gh.state["pipe_trip"], {"outbound": switch.iso_z(TS.PIPE_NOW),
                                                 "inbound": switch.iso_z(TS.PIPE_NOW + dt.timedelta(hours=26))})

    def test_a_healthy_pipe_never_tripped_posts_nothing(self):
        gh = StatefulPipe(comments=[TS.pipe_comment(TS.ASK, 60), TS.pipe_comment(TS.BRIDGE, 30, login="IssisX")])
        self.assertEqual(TS.pipe_switch(gh).pipe_health(), [])
        self.assertEqual((gh.posted, gh.writes, gh.state), ([], [], {}))

    def test_a_dry_run_keeps_no_tripped_state(self):
        gh = StatefulPipe(comments=[TS.pipe_comment(TS.BRIDGE, 30, login="IssisX")], dry_run=True)
        self.stuck(gh)
        TS.pipe_switch(gh).pipe_health()
        self.assertEqual(gh.state, {})

    def test_the_all_clear_is_no_ask_to_muse(self):
        # inbound_quiet counts CLAUDE_TO_MUSE asks: an all-clear nobody answers must not raise INBOUND STUCK a day on
        ok = f"{switch.PIPE_OK_HEAD.format(board=switch.BOARD_ISSUE)} works\n\n{switch.PIPE_OK_MARK}"
        gh = StatefulPipe(comments=[TS.pipe_comment(TS.BRIDGE, 26 * 60, login="IssisX"), TS.pipe_comment(ok, 120)])
        self.assertEqual(TS.pipe_switch(gh).pipe_health(), [])
        gh.comments.append(TS.pipe_comment(TS.ASK, 60))   # a real ask still counts
        (line,) = TS.pipe_switch(gh).pipe_health()
        self.assertTrue(line.startswith("INBOUND STUCK: 1 CLAUDE_TO_MUSE ask(s)"))


class OldAutoPrHeads(unittest.TestCase):
    """5. auto_open_prs: an AUTO-PR-FAILED head older than AUTO_PR_OLD is listed once, never again."""

    def setup(self, days_old: float, name="fb-bot/integrate-windup"):
        gh, head = TA.green_setup(name=name)
        gh.state = {}
        gh.commits[head] = "Budget the windup lift\n\nNo job line here."
        stamp = (TA.NOW - dt.timedelta(days=days_old)).strftime("%Y-%m-%dT%H:%M:%SZ")
        get = gh.get

        def dated(path, accept=""):
            got = get(path, accept)
            if "/commits/" in path:
                got["commit"]["committer"] = {"date": stamp}
            return got
        gh.get = dated
        return gh, head

    def listed_then_lost(self, days_old: float):
        gh, head = self.setup(days_old)
        sw = TA.make(gh)
        sw.auto_open_prs()
        (item,) = sw.auto_pr_failed
        gh.state["inbox"] = {"keys": [item.key]}   # the inbox listed it
        sw.auto_open_prs()
        self.assertEqual(sw.auto_pr_failed, ())
        gh.state["inbox"] = {"keys": []}           # and its ring lost the key (MAX_INBOX_KEYS)
        sw.auto_open_prs()
        return gh, sw, item

    def test_an_old_head_is_never_listed_again_once_the_ring_loses_it(self):
        gh, sw, item = self.listed_then_lost(20)
        self.assertEqual(sw.auto_pr_failed, ())
        self.assertEqual(gh.state["auto_pr_old"], [f"fb-bot/integrate-windup@{item.head}"])
        gh.gets.clear()
        sw.auto_open_prs()
        self.assertFalse(any("workflows/" in g for g in gh.gets), gh.gets)   # and no proof is read for it

    def test_a_young_head_is_listed_again_as_before(self):
        gh, sw, item = self.listed_then_lost(2)
        self.assertEqual(sw.auto_pr_failed, (item,))
        self.assertFalse(gh.state.get("auto_pr_old"))

    def test_an_old_heads_pr_still_opens_once_the_plan_has_its_line(self):
        gh, sw, item = self.listed_then_lost(20)
        gh.commits[item.head] = "Budget the windup lift\n\nJob: J-042"
        self.assertEqual(sw.auto_open_prs(), 1)

    def test_an_old_head_whose_open_fails_is_listed_once_and_still_tried(self):
        for days, again in ((20, False), (2, True)):
            gh, head = self.setup(days)
            gh.commits[head] = "Budget the windup lift\n\nJob: J-042"   # a plan line: the open is tried, and fails
            gh.fail_write = "gh: Validation Failed (HTTP 422)"
            sw = TA.make(gh)
            sw.auto_open_prs()
            (item,) = sw.auto_pr_failed
            gh.state["inbox"] = {"keys": [item.key]}
            gh.writes.clear()
            sw.auto_open_prs()
            self.assertEqual(len(gh.writes), 1, days)                      # the open is tried every sweep
            self.assertEqual(sw.auto_pr_failed, (item,) if again else (), days)

    def test_a_gone_head_is_forgotten(self):
        gh, sw, item = self.listed_then_lost(20)
        gh.branches = []
        sw.auto_open_prs()
        self.assertEqual(gh.state["auto_pr_old"], [])


class TitleCheck(TP.Sweeps):
    """6. machine.bad_titles and the plan pass: a plan job's issue titled other than `J-NNN ` first is flagged."""

    def test_bad_titles(self):
        model = plan.parse("limit muse 4\nJ-501 muse #501 | a\nJ-502 muse #502 | b\nJ-503b muse #503 | c\n"
                           "J-504 muse #504 | d\n")
        issues = [{"number": 501, "title": "J-501 fine", "state": "open"},
                  {"number": 502, "title": "J-502: a colon", "state": "open"},
                  {"number": 503, "title": "J-503b—a dash", "state": "open"},
                  {"number": 504, "title": "Board", "state": "open"}]   # not titled with its job: clashes() says so
        self.assertEqual(M.bad_titles(model, issues), [("J-502", 502, "J-502: a colon"),
                                                       ("J-503b", 503, "J-503b—a dash")])
        issues[1]["state"] = "closed"
        self.assertEqual([j for j, _, _ in M.bad_titles(model, issues)], ["J-503b"])

    def test_the_plan_pass_wakes_the_integrator_once_per_title(self):
        self.hub.issue(501, "J-501: The job this test walks", ["job"])
        sw, _ = self.sweep(0.5)
        (item,) = [i for i in sw.plan_items if i.target == "#501"]
        self.assertEqual((item.kind, item.dest, switch.inbox_class(item)), ("PLAN", "integrator", "hold"))
        self.assertIn("rename #501", item.line)
        self.assertTrue(any(line.startswith("PLAN-TITLE  J-501 #501") for line in sw.lines))
        sw2, _ = self.sweep(1)
        self.assertEqual([i.head for i in sw2.plan_items if i.target == "#501"], [item.head])   # the same key
        self.hub.issue(501, "J-501 The job this test walks", ["job"])
        sw3, _ = self.sweep(1.5)
        self.assertFalse([i for i in sw3.plan_items if i.target == "#501"])


if __name__ == "__main__":
    unittest.main()
