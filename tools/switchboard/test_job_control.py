"""Issue-owned job routing regression tests. No network or local branch authority."""
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import job_control as jc


def issue(n, job, *labels, body="", state="open"):
    return {"number": n, "title": f"{job} a job", "state": state, "labels": [{"name": x} for x in labels],
            "body": body}


def ready(n, job, owner="muse", state="ready", body=""):
    return issue(n, job, "job", "owner:" + owner, "state:" + state, body=body)


class Control(unittest.TestCase):
    def test_missing_label_is_visible_not_discarded(self):
        inv = jc.inventory([issue(424, "J-105", "owner:muse", "state:ready")])
        self.assertIn("J-105", inv.jobs)
        self.assertTrue(any("missing job label" in e for e in inv.violations))
        self.assertFalse(inv.ready_muse)

    def test_one_owner_one_state_or_fail_closed(self):
        inv = jc.inventory([issue(428, "J-109", "job", "owner:muse", "owner:claude", "state:active",
                                  "state:parked")])
        self.assertEqual(len(inv.violations), 2)
        self.assertTrue(inv.start_problem("J-109", "muse"))

    def test_issues_not_queue_text_are_orders(self):
        inv = jc.inventory([ready(1, "J-116"), ready(2, "J-117"), ready(3, "J-118", state="parked")])
        self.assertEqual([j.id for j in inv.ready_muse], ["J-116", "J-117"])
        self.assertEqual(inv.start_problem("J-116", "muse"), "")
        self.assertIn("parked", inv.start_problem("J-118", "muse"))

    def test_reassignment_before_mail_is_mandatory(self):
        inv = jc.inventory([ready(428, "J-109", owner="claude")])
        self.assertIn("claude", inv.start_problem("J-109", "muse"))
        self.assertIn("owner is claude", jc.merge_problem("J-109", "fb-bot/integrate-j109", ready(428, "J-109", "claude")))
        self.assertEqual(jc.merge_problem("J-109", "claude/integrate-j109", ready(428, "J-109", "claude")), "")

    def test_no_remote_start_is_flagged(self):
        inv = jc.inventory([ready(321, "J-063", state="active")])
        self.assertTrue(any("STARTED but no remote" in e for e in inv.violations))
        inv = jc.inventory([ready(321, "J-063", state="active")], ["fb-bot/integrate-j063-wip"])
        self.assertFalse(any("no remote" in e for e in inv.violations))

    def test_two_vendors_same_job_caught(self):
        inv = jc.inventory([ready(169, "J-038", state="active")],
                           ["fb-bot/integrate-j038", "claude/integrate-j038"])
        self.assertTrue(any("two vendors" in e for e in inv.violations))

    def test_parked_cannot_merge_even_on_green(self):
        self.assertIn("parked", jc.merge_problem("J-055", "fb-bot/integrate-j055",
                                                  ready(294, "J-055", state="parked")))

    def test_wip_cap_blocks_new_starts_without_parked_inflight(self):
        rows = [ready(i, f"J-{i:03d}", state="active") for i in range(101, 105)]
        rows.append(ready(105, "J-105"))
        inv = jc.inventory(rows)
        self.assertEqual(inv.active_muse, 4)
        self.assertIn("WIP cap", inv.start_problem("J-105", "muse"))
        self.assertEqual(len(inv.ready_muse), 1)

    def test_blockers_are_declared_and_a_cycle_is_error(self):
        rows = [ready(1, "J-101", body="Blocked-by: J-102"),
                ready(2, "J-102", body="Blocked-by: J-101")]
        inv = jc.inventory(rows)
        self.assertTrue(any("dependency cycle" in e for e in inv.violations))
        self.assertFalse(inv.ready_muse)
        self.assertIn("blocked by", inv.start_problem("J-101", "muse"))

    def test_unknown_nonjob_and_closed_issue_ignored(self):
        inv = jc.inventory([issue(1, "Question", "job"), ready(2, "J-102", state="parked"),
                            ready(3, "J-103", state="active", body="", owner="muse")])
        self.assertNotIn("Question", inv.jobs)
        self.assertEqual(set(inv.jobs), {"J-102", "J-103"})

    def test_duplicate_job_issue_detected(self):
        inv = jc.inventory([ready(1, "J-109"), ready(2, "J-109")])
        self.assertTrue(any("duplicate issues" in e for e in inv.violations))

    def test_ready_with_remote_work_is_not_offered_again(self):
        inv = jc.inventory([ready(1, "J-109")], ["fb-bot/integrate-j109"])
        self.assertFalse(inv.ready_muse)
        self.assertTrue(any("ready despite pushed work" in e for e in inv.violations))

    def test_branch_job_matches_lettered_and_protected(self):
        self.assertEqual(jc.branch_job("fb-bot/integrate-protected-j044c-foo"), "J-044C")
        self.assertEqual(jc.branch_job("claude/integrate-j109"), "J-109")
        self.assertEqual(jc.branch_job("claude/integrate-j1099"), "J-1099")
        self.assertEqual(jc.branch_job("fb-bot/other"), "")

    def test_unrelated_mail_is_not_authority(self):
        self.assertTrue(jc.merge_problem("J-109", "claude/integrate-j109", None))
        self.assertEqual(jc.inventory([]).start_problem("J-109", "muse"), "J-109: no open job issue")


if __name__ == "__main__":
    unittest.main()
