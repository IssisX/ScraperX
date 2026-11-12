"""Plan authority, dependencies, remote work, limits and deterministic rendering."""
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import plan


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

    def test_no_line_detects_bare_issues(self):
        p = plan.parse(self.text)
        errors = plan.findings(p, [self.issue(101, "J-101"), self.issue(102, "J-102"),
                                   self.issue(103, "J-103"), self.issue(109, "J-109")], [])
        self.assertIn("NO-LINE J-109 #109", errors)

    def test_bad_issue_number_not_silently_accepted(self):
        p = plan.parse(self.text)
        errors = plan.findings(p, [self.issue(100, "J-101"), self.issue(102, "J-102"),
                                   self.issue(103, "J-103")], [])
        self.assertIn("NO-ISSUE J-101 #101", errors)

    def test_held_and_waiting_are_not_ready(self):
        p = plan.parse(self.text)
        self.assertEqual(plan.status(p.by_job()["J-102"]), "waiting")
        self.assertEqual(plan.status(p.by_job()["J-102"], closed={"J-101"}), "queued")
        self.assertEqual(plan.status(p.by_job()["J-103"]), "held")

    def test_remote_branch_receipt(self):
        self.assertEqual(plan.job_of_branch("fb-bot/wip-j044c"), "J-044c")
        self.assertEqual(plan.job_of_branch("fb-bot/integrate-protected-j044c"), "J-044c")
        self.assertEqual(plan.job_of_branch("claude/integrate-j109"), "J-109")
        self.assertEqual(plan.job_of_branch("unrelated"), "")

    def test_wrong_vendor_head_detected(self):
        p = plan.parse(self.text)
        errors = plan.findings(p, [self.issue(n, f"J-{n:03d}") for n in (101,102,103)],
                               ["claude/integrate-j101"])
        self.assertTrue(any("NOT-OWNER J-101" in x for x in errors))

    def test_started_without_remote_is_an_error(self):
        p = plan.parse(self.text)
        errors = plan.findings(p, [self.issue(n, f"J-{n:03d}") for n in (101,102,103)], [],
                               started={"J-101"})
        self.assertIn("UNPUSHED J-101: started without a remote head", errors)

    def test_wip_limit_observed_not_hand_edited(self):
        p = plan.parse("limit muse 1\nJ-101 muse #101 | one\nJ-102 muse #102 | two\n")
        errors = plan.findings(p, [self.issue(101, "J-101"), self.issue(102, "J-102")],
                               ["fb-bot/wip-j101", "fb-bot/integrate-j102"])
        self.assertTrue(any("OVER-LIMIT muse: 2/1" in x for x in errors))

    def test_view_changes_when_job_body_changes(self):
        p = plan.parse(self.text)
        v1 = plan.view(p, "muse", {"J-101": "pushed"}, {"J-101": "old"})[1]
        v2 = plan.view(p, "muse", {"J-101": "pushed"}, {"J-101": "new"})[1]
        self.assertNotEqual(v1, v2)

    def test_pr_state_beats_unstarted(self):
        x = plan.parse(self.text).by_job()["J-101"]
        self.assertEqual(plan.status(x, prs=[{"body":"Job: J-101\n", "state":"open"}]), "review")


if __name__ == "__main__":
    unittest.main()
