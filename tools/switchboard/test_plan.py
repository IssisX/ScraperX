"""The plan file: authority, dependencies and rendering; and its old checks as states of the machine."""
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import datetime as dt

import machine
import plan

NOW = dt.datetime(2026, 11, 12, 18, 0, tzinfo=dt.timezone.utc)


class PlanTests(unittest.TestCase):
    def setUp(self):
        self.text = ("limit claude 4\nlimit muse 2\n"
                     "J-101 muse #101 | first\n"
                     "J-102 muse #102 after J-101 | second\n"
                     "J-103 claude #103 hold: blocked on proof | held\n")

    def issue(self, n, job):
        return {"number": n, "title": f"{job} work", "state": "open", "labels": []}

    def test_one_order_has_both_owners_and_work_order(self):
        p = plan.parse(self.text)
        self.assertEqual(p.errors, ())
        self.assertEqual([ln.job for ln in p.owner_lines("muse")], ["J-101", "J-102"])
        self.assertEqual(p.by_job()["J-102"].after, ("J-101",))
        self.assertEqual(p.by_job()["J-103"].hold, "blocked on proof")

    def test_plan_round_trip_is_stable(self):
        p = plan.parse(self.text)
        self.assertEqual(plan.parse(plan.render(p)), p)
        self.assertEqual(p.fingerprint(), plan.parse(plan.render(p)).fingerprint())

    def test_bad_lines_and_repeated_job_fail_closed(self):
        p = plan.parse(self.text + "J-101 muse #111 | duplicate\nmalformed\n")
        self.assertTrue(any("duplicate J-101" in x for x in p.errors))
        self.assertTrue(any("malformed" in x for x in p.errors))

    def test_cycle_detected(self):
        p = plan.parse("limit muse 2\nJ-101 muse #101 after J-102 | one\n"
                       "J-102 muse #102 after J-101 | two\n")
        self.assertTrue(any("cycle" in x for x in p.errors))

    def test_unknown_owner_rejected(self):
        self.assertTrue(any("no limit" in x for x in
                            plan.parse("limit muse 1\nJ-101 claude #101 | one\n").errors))

    # The checks below were plan.status and plan.findings (NO-LINE, DUP-ISSUE, NO-ISSUE, NOT-OWNER, UNPUSHED,
    # OVER-LIMIT, DORMANT). Each is now a state of the one job machine, read from the same evidence.
    def rows(self, text=None, issues=None, branches=(), prs=(), mail=None, **reads):
        p = plan.parse(text or self.text)
        issues = [self.issue(ln.issue, ln.job) for ln in p.lines] if issues is None else issues
        self.table = machine.table(p, machine.collect(p, dict(issues=issues, branches=list(branches), prs=list(prs),
                                                              **reads), mail=mail))
        self.model = p
        return {r.job: r for r in self.table}

    def conflict(self, row, *needles):
        self.assertEqual((row.state, row.owes), ("conflict", "integrator"), row)
        for needle in needles:
            self.assertIn(needle, row.why)

    def test_no_line_detects_bare_issues(self):
        rows = self.rows(issues=[self.issue(101, "J-101"), self.issue(102, "J-102"),
                                 self.issue(103, "J-103"), self.issue(109, "J-109")])
        self.conflict(rows["J-109"], "#109", "no line")

    def test_two_open_issues_for_one_job_detected(self):
        rows = self.rows(issues=[self.issue(101, "J-101"), self.issue(201, "J-101"), self.issue(102, "J-102"),
                                 self.issue(103, "J-103")])
        self.conflict(rows["J-101"], "#101 and #201")

    def test_bad_issue_number_not_silently_accepted(self):
        rows = self.rows(issues=[self.issue(100, "J-101"), self.issue(102, "J-102"), self.issue(103, "J-103")])
        self.conflict(rows["J-101"], "the plan says #101")

    def test_held_and_waiting_are_not_ready(self):
        rows = self.rows()
        self.assertEqual((rows["J-102"].state, rows["J-102"].why), ("blocked", "J-101"))
        self.assertEqual(rows["J-103"].state, "held")
        issues = [self.issue(102, "J-102"), self.issue(103, "J-103")]
        self.assertEqual(self.rows(issues=issues, closed=frozenset({"J-101"}))["J-102"].state, "planned")

    def test_remote_branch_receipt(self):
        self.assertEqual(plan.job_of_branch("fb-bot/wip-j044c"), "J-044c")
        self.assertEqual(plan.job_of_branch("fb-bot/integrate-protected-j044c"), "J-044c")
        self.assertEqual(plan.job_of_branch("claude/integrate-j109"), "J-109")
        self.assertEqual(plan.job_of_branch("unrelated"), "")

    def test_wrong_vendor_head_detected(self):
        self.conflict(self.rows(branches=["claude/integrate-j101"])["J-101"], "claude/integrate-j101")

    def test_started_without_remote_is_an_error(self):
        said = machine.Mail((machine.Post(1, 340, NOW, "J-101 started", "", False),))
        row = self.rows(mail=said)["J-101"]
        self.assertEqual((row.gate, row.owes, row.limit), ("claimed", "muse", machine.hours(2)))
        self.assertIn("no head is on the remote", row.next)
        self.assertTrue(row.late(NOW + machine.hours(3)))

    def test_wip_limit_observed_not_hand_edited(self):
        self.rows("limit muse 1\nJ-101 muse #101 | one\nJ-102 muse #102 | two\n",
                  branches=["fb-bot/wip-j101", "fb-bot/integrate-j102"])
        self.assertEqual(machine.wip(self.model, self.table)["muse"], (2, 1))

    def test_view_changes_when_a_line_changes_and_never_with_a_job_body(self):   # the order is work/plan.md alone
        p = plan.parse(self.text)
        self.assertNotEqual(machine.order_version(p, "muse"),
                            machine.order_version(plan.parse(self.text.replace("J-101 muse #101", "J-101 muse #101 "
                                                                               "hold: wait")), "muse"))
        self.assertEqual(machine.versions(p), {o: machine.order_version(p, o) for o in p.limits})

    def test_pr_state_beats_unstarted(self):
        pr = {"number": 7, "body": "Job: J-101\n", "state": "open", "head": {"ref": "fb-bot/integrate-j101", "sha": "x"}}
        self.assertIn(self.rows(prs=[pr])["J-101"].state, machine.IN_FLIGHT)

    # False WIP overload from historical branches: a branch whose head is a merged PR's head, or is already in
    # trunk, is history; a bare name with no sha cannot be proven landed and still counts.
    def br(self, name, sha):
        return {"name": name, "commit": {"sha": sha}}

    def test_landed_or_closed_branch_heads_are_not_in_flight(self):
        text = "limit claude 1\nJ-101 claude #101 | one\nJ-102 claude #102 | two\n"
        branches = [self.br("claude/integrate-j101", "aaa"), self.br("claude/integrate-j102", "bbb")]
        self.rows(text, branches=branches)
        self.assertEqual(machine.wip(self.model, self.table)["claude"], (2, 1))
        rows = self.rows(text, branches=branches, inactive={"aaa"}, merged_at={"J-101": "2026-11-10T00:00:00Z"})
        self.assertEqual(machine.wip(self.model, self.table)["claude"], (1, 1))
        self.assertEqual(rows["J-101"].state, "landed")
        self.assertEqual(rows["J-102"].state, "pushed")

    def test_unproven_branch_name_still_counts(self):
        self.assertEqual(self.rows(branches=["fb-bot/wip-j101"], inactive={"aaa"})["J-101"].state, "started")

    def test_landed_history_from_another_vendor_is_not_duplicate_ownership(self):
        old = [self.br("claude/integrate-j101", "old")]
        self.assertNotEqual(self.rows(branches=old, inactive={"old"})["J-101"].state, "conflict")
        live = old + [self.br("claude/integrate-protected-j101", "new")]
        row = self.rows(branches=live, inactive={"old"})["J-101"]
        self.conflict(row, "claude/integrate-protected-j101")
        self.assertNotIn("claude/integrate-j101,", row.why)

    def test_hold_outranks_pushed_work_but_never_an_open_pr(self):
        self.assertEqual(self.rows(branches=[self.br("claude/integrate-j103", "c")])["J-103"].state, "held")
        pr = {"number": 7, "body": "Job: J-103\n", "state": "open", "head": {"ref": "claude/integrate-j103", "sha": "c"}}
        self.assertIn(self.rows(prs=[pr])["J-103"].state, machine.IN_FLIGHT)

    # The integrator's ruling on a head that is not a lane: `kept <branch>@<sha>` on the job's line.
    def test_a_kept_head_is_a_record_until_it_moves(self):
        text = self.text.replace("J-101 muse #101", "J-101 muse #101 kept claude/integrate-j101@abc1234")
        p = plan.parse(text)
        self.assertEqual(p.errors, ())
        self.assertEqual(p.by_job()["J-101"].kept, (("claude/integrate-j101", "abc1234"),))
        self.assertEqual(plan.parse(plan.render(p)), p)
        self.assertNotEqual(self.rows(text, branches=[self.br("claude/integrate-j101", "abc1234def")])["J-101"].state,
                            "conflict")
        self.conflict(self.rows(text, branches=[self.br("claude/integrate-j101", "fff0000aaa")])["J-101"],
                      "claude/integrate-j101")

    def test_a_ruling_is_not_part_of_the_owners_order(self):
        a = plan.parse(self.text)
        b = plan.parse(self.text.replace("J-101 muse #101", "J-101 muse #101 kept claude/integrate-j101@abc1234"))
        self.assertEqual(machine.order_version(a, "muse"), machine.order_version(b, "muse"))


if __name__ == "__main__":
    unittest.main()
