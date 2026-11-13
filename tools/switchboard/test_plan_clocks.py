"""11-13: each job's own start clock, lettered parts as their own jobs, and a plan edit's bounded read.

1. tools/switchboard/plan.py gave every job one clock, the plan file's last commit: any edit to any line restarted
   every unstarted job's start alarm, so it never fired. Now a line carries `at <time>`, written by
   tools/board/plan.py when the job is added or given (existing lines once, from their own history: `stamp`).
2. J-044 had one line (Muse's) while claude/integrate-j044c-upper-body existed: two agents on one job. A lettered
   part with its own line is its own job everywhere (its heads, its mail, its claim), with its own owner and lane.
3. `plan.py hold` read every remote branch and PR, up to 60 compares and 30 issues, for each job held: over a
   minute a hold. Now `hold J-001 J-015 ... --reason` holds them all on one read, and the read covers only the jobs
   edited.
Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
"""

from __future__ import annotations

import datetime as dt
import importlib.util
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import machine as M  # noqa: E402
import plan  # noqa: E402
import test_machine as tm  # noqa: E402

spec = importlib.util.spec_from_file_location("collab_plan_editor_clocks", HERE.parent / "board" / "plan.py")
editor = importlib.util.module_from_spec(spec)
spec.loader.exec_module(editor)

T0 = tm.T0
SOURCE = ("limit muse 2\nlimit claude 4\n"
          "J-101 muse #101 at 2026-11-10T09:00Z | first\n"
          "J-102 muse #102 | second\n")


def clocks(text: str, planned_at: dt.datetime) -> dict[str, dt.datetime | None]:
    model = plan.parse(text)
    assert not model.errors, model.errors
    evs = M.collect(model, {"issues": [tm.issue(ln.issue, ln.job) for ln in model.lines]}, planned_at=planned_at)
    return {j: ev.planned_at for j, ev in evs.items()}


class OwnClock(unittest.TestCase):
    def test_the_time_round_trips_and_is_not_part_of_the_order(self):
        model = plan.parse(SOURCE)
        self.assertEqual(model.errors, ())
        self.assertEqual(model.by_job()["J-101"].at, "2026-11-10T09:00Z")
        self.assertIn("J-101 muse #101 at 2026-11-10T09:00Z | first", plan.render(model))
        bare = plan.parse(SOURCE.replace(" at 2026-11-10T09:00Z", ""))
        self.assertEqual(M.order_version(model, "muse"), M.order_version(bare, "muse"), "no new version, no re-notice")
        self.assertEqual(M.line_hash(model.by_job()["J-101"]), M.line_hash(bare.by_job()["J-101"]))
        held = plan.parse(SOURCE.replace("#101 at 2026-11-10T09:00Z |", "#101 at 2026-11-10T09:00Z hold: later |"))
        self.assertEqual((held.by_job()["J-101"].at, held.by_job()["J-101"].hold), ("2026-11-10T09:00Z", "later"))
        self.assertTrue(plan.parse(SOURCE.replace("09:00Z", "29:00Z")).errors)

    def test_editing_another_line_does_not_move_a_jobs_clock(self):
        before = clocks(SOURCE, T0)
        edited = editor.change(SOURCE, "hold", ["J-102", "Cory", "parked", "it"], now=T0 + dt.timedelta(hours=5))
        after = clocks(edited, T0 + dt.timedelta(hours=5))   # the edit is the plan file's newest commit
        own = dt.datetime(2026, 11, 10, 9, 0, tzinfo=dt.timezone.utc)
        self.assertEqual((before["J-101"], after["J-101"]), (own, own))
        self.assertEqual(after["J-102"], T0 + dt.timedelta(hours=5), "a line with no time keeps the old fallback")

    def test_add_and_give_write_the_time_and_a_give_to_the_same_owner_keeps_it(self):
        now = dt.datetime(2026, 11, 13, 15, 4, tzinfo=dt.timezone.utc)
        got = editor.change(SOURCE, "add", ["J-103", "claude", "#103", "third"], now=now)
        self.assertIn("J-103 claude #103 at 2026-11-13T15:04Z | third", got)
        got = editor.change(SOURCE, "give", ["J-101", "claude"], now=now)
        self.assertIn("J-101 claude #101 at 2026-11-13T15:04Z | first", got)
        got = editor.change(SOURCE, "give", ["J-101", "muse"], now=now)
        self.assertEqual(got, SOURCE)

    def test_existing_lines_get_their_time_once_from_their_own_history(self):
        log = ("COMMIT 2026-11-12T10:00:00Z\n+J-101 muse #101 | first\n+J-102 claude #102 | second\n"
               "COMMIT 2026-11-12T12:00:00Z\n-J-102 claude #102 | second\n+J-102 muse #102 | second\n"
               "COMMIT 2026-11-13T08:00:00Z\n-J-101 muse #101 | first\n+J-101 muse #101 hold: x | first\n")
        hist = editor.owner_times(log)
        self.assertEqual(hist, {"J-101": "2026-11-12T10:00Z", "J-102": "2026-11-12T12:00Z"},
                         "the time it got its present owner, not its last edit")
        got = editor.change(SOURCE, "stamp", [], history=hist)
        self.assertIn("J-101 muse #101 at 2026-11-10T09:00Z | first", got, "a line with a time keeps it")
        self.assertIn("J-102 muse #102 at 2026-11-12T12:00Z | second", got)


class LetteredParts(unittest.TestCase):
    TEXT = ("limit muse 10\nlimit claude 4\n"
            "J-044 muse #242 | Console-quality fighting\n"
            "J-044c claude #242 at 2026-11-13T15:00Z hold: paused | Upper body\n")

    def test_a_lettered_part_with_its_own_line_is_its_own_job(self):
        model = plan.parse(self.TEXT)
        self.assertEqual(model.errors, ())
        jobs = model.by_job()
        self.assertEqual(plan.plan_job("J-044c", jobs), "J-044c")
        self.assertEqual(plan.plan_job("J-044d", jobs), "J-044", "a part with no line is still its base's")
        reads = {"issues": [tm.issue(242, "J-044")],
                 "branches": [tm.br("claude/integrate-j044c-upper-body", "c" * 40),
                              tm.br("fb-bot/integrate-j044d", "d" * 40)]}
        evs = M.collect(model, reads)
        self.assertEqual([h.ref for h in evs["J-044c"].heads], ["claude/integrate-j044c-upper-body"])
        self.assertEqual([h.ref for h in evs["J-044"].heads], ["fb-bot/integrate-j044d"])

    def test_the_claim_gate_counts_a_lettered_start_against_its_own_line(self):
        model = plan.parse(self.TEXT)
        w = tm.World(self.TEXT)
        w.muse(1, "J-044c started")
        w.muse(2, "J-044d started")
        starts = M.started_posts(w.mail(), lambda j: plan.plan_job(j, model.by_job()))
        self.assertEqual(sorted(starts), ["J-044", "J-044c"])
        wrong = M.mail_clashes(model, w.mail())
        self.assertIn("the plan gives it to claude", " ".join(wrong["J-044c"]))
        self.assertNotIn("J-044", wrong, "Muse's start on d is Muse's own job's")


class BoundedEdit(unittest.TestCase):
    def test_a_batch_hold_reads_github_once_and_only_for_the_jobs_it_holds(self):
        text = SOURCE + "J-103 muse #103 | third\n"
        with tempfile.TemporaryDirectory() as d:
            path = pathlib.Path(d) / "plan.md"
            path.write_text(text, encoding="utf-8")
            seen = []

            def observe(gh, model, now, out, only=None):
                seen.append(only)
                return {"branches": [], "prs": [], "inactive": frozenset(), "issues": []}

            with mock.patch.object(editor.core, "observe", side_effect=observe), \
                    mock.patch.object(editor, "GitHub"), mock.patch.object(editor, "recent_flip", return_value=False):
                code = editor.main(["--plan", str(path), "--expect", editor.sha(text), "--reason", "Cory: paused",
                                    "hold", "J-101", "J-103"])
            got = path.read_text(encoding="utf-8")
        self.assertEqual(code, 0)
        self.assertEqual(seen, [frozenset({"J-101", "J-103"})])
        self.assertIn("J-101 muse #101 at 2026-11-10T09:00Z hold: Cory: paused | first", got)
        self.assertIn("J-103 muse #103 hold: Cory: paused | third", got)
        self.assertNotIn("hold", got.split("J-102")[1].split("\n")[0])

    def test_the_edit_read_compares_only_its_jobs_heads_and_reads_no_issue(self):
        calls = []

        class GH:
            repo = "IssisX/ScraperX"

            def paged(self, path, key=""):
                calls.append(path)
                if "/branches" in path:
                    return [tm.br("fb-bot/integrate-j101", "1" * 40), tm.br("fb-bot/integrate-j102", "2" * 40)]
                return []

            def get(self, path, accept=""):
                calls.append(path)
                return {"status": "diverged", "commits": [], "behind_by": 1}

        plan.observe(GH(), plan.parse(SOURCE), T0, out=lambda line: None, only=frozenset({"J-101"}))
        self.assertFalse(any("/issues" in c for c in calls), calls)
        self.assertEqual([c for c in calls if "/compare/" in c], [f"repos/IssisX/ScraperX/compare/ScraperX-Claude...{'1' * 40}"])
        self.assertFalse(any("/commits?path=" in c for c in calls), calls)


if __name__ == "__main__":
    unittest.main()
