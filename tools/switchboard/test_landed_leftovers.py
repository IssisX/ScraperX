"""A landed or superseded job's leftover heads are history (11-14: J-014's old idle branch read as Muse's late work
while #491 had landed): the machine drops them, sweep.py raises no FIX-CI on them, `plan.py keep` takes a lettered
split's head, and a refused `plan.py hold` exits non-zero."""
import contextlib
import datetime as dt
import importlib.util
import io
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import machine as M  # noqa: E402
from test_machine import World, at, br, iso  # noqa: E402

BOARD = pathlib.Path(__file__).resolve().parents[1] / "board"


def load(name: str, file: str):
    spec = importlib.util.spec_from_file_location(name, BOARD / file)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


editor = load("leftovers_plan_editor", "plan.py")
sweep = load("leftovers_sweep", "sweep.py")


class OlderHeads(unittest.TestCase):
    """machine.collect: an older head of a job stops counting once the job has a merged PR or a newer head after it."""

    def test_an_integrate_head_older_than_the_jobs_merge_is_history(self):
        w = World()
        w.branches.append(br("fb-bot/integrate-j101-old", "o1"))   # the J-014 shape: an idle branch, its PR's work landed
        w.head_at["o1"] = at(1)
        w.works["o1"] = M.branch_work("fb-bot/integrate-j101-old", "o1", "red: native-tests", at=at(1))
        w.merged_at["J-101"] = iso(2)
        r = w.rows()["J-101"]
        self.assertEqual((r.state, r.gate), ("landed", ""))   # not pushed/red: no fix is owed on the old head
        self.assertEqual(M.wip(w.model, w.table)["muse"], (0, 2))
        w.head_at["o1"] = at(3)   # a commit after the merge is the next part's work
        self.assertEqual((w.rows()["J-101"].state, w.rows()["J-101"].gate), ("pushed", "red"))

    def test_an_older_integrate_head_is_superseded_by_the_owners_newer_one(self):
        w = World()
        w.branches += [br("fb-bot/integrate-j101", "o1"), br("fb-bot/integrate-j101-v2", "n1")]
        w.head_at.update({"o1": at(1), "n1": at(5)})
        w.works["o1"] = M.branch_work("fb-bot/integrate-j101", "o1", "red: native-tests", at=at(1))
        w.works["n1"] = M.branch_work("fb-bot/integrate-j101-v2", "n1", "running", at=at(5))
        r = w.rows()["J-101"]
        self.assertEqual((r.state, r.gate, r.since), ("pushed", "running", at(5)))
        self.assertEqual([h.ref for h in M.collect(w.model, self.reads(w))["J-101"].heads], ["fb-bot/integrate-j101-v2"])

    def reads(self, w):
        return {"issues": w.issues, "branches": w.branches, "prs": w.prs, "head_at": w.head_at,
                "merged_at": w.merged_at, "inactive": frozenset(w.inactive)}

    def heads(self, w):
        return sorted(h.ref for h in M.collect(w.model, self.reads(w))["J-101"].heads)

    def test_what_stays_visible(self):
        # a head with no commit time read (observe did not compare it) is never proven older
        w = World()
        w.branches += [br("fb-bot/integrate-j101", "o1"), br("fb-bot/integrate-j101-v2", "n1")]
        w.head_at["n1"] = at(5)
        w.merged_at["J-101"] = iso(6)
        self.assertEqual(self.heads(w), ["fb-bot/integrate-j101"])
        # an open PR's head stays, older than the merge or another head or not
        w = World()
        w.branches += [br("fb-bot/integrate-j101", "o1"), br("fb-bot/integrate-j101-v2", "n1")]
        w.head_at.update({"o1": at(1), "n1": at(5)})
        w.prs.append({"number": 900, "state": "open", "body": "Job: J-101\n",
                      "head": {"ref": "fb-bot/integrate-j101", "sha": "o1"}})
        self.assertEqual(self.heads(w), ["fb-bot/integrate-j101", "fb-bot/integrate-j101-v2"])
        w.merged_at["J-101"] = iso(6)
        self.assertEqual(self.heads(w), ["fb-bot/integrate-j101"])
        # another owner's older head is still unlanded work beside the plan's owner: the conflict stands
        w = World()
        w.branches += [br("claude/integrate-j101", "c1"), br("fb-bot/integrate-j101", "n1")]
        w.head_at.update({"c1": at(1), "n1": at(5)})
        self.assertEqual(self.heads(w), ["claude/integrate-j101", "fb-bot/integrate-j101"])
        self.assertEqual(w.rows()["J-101"].state, "conflict")
        # a protected branch and the plain one are separate lanes (CLAUDE.md), and so are lettered splits
        w = World()
        w.branches += [br("fb-bot/integrate-protected-j101", "p1"), br("fb-bot/integrate-j101", "n1"),
                       br("fb-bot/integrate-j101a", "a1"), br("fb-bot/integrate-j101c", "c1")]
        w.head_at.update({"p1": at(1), "n1": at(5), "a1": at(1), "c1": at(5)})
        self.assertEqual(self.heads(w), ["fb-bot/integrate-j101", "fb-bot/integrate-j101a", "fb-bot/integrate-j101c",
                                         "fb-bot/integrate-protected-j101"])


SOURCE = "limit muse 2\nlimit claude 4\nJ-044 muse #44 | split\nJ-045 muse #45 | next\n"


class KeepSplit(unittest.TestCase):
    """`plan.py keep` takes a lettered split's head as its base job's record."""

    def test_a_lettered_split_maps_to_its_job(self):
        got = editor.change(SOURCE, "keep", ["J-044", "fb-bot/integrate-j044c@abc1234"],
                            branches=[br("fb-bot/integrate-j044c", "abc1234def")])
        self.assertIn("J-044 muse #44 kept fb-bot/integrate-j044c@abc1234 | split", got)
        model = M.P.parse(got)
        ev = M.collect(model, {"issues": [{"number": 44, "title": "J-044 split", "state": "open"}],
                               "branches": [br("fb-bot/integrate-j044c", "abc1234def")]})
        self.assertEqual(ev["J-044"].heads, ())   # the kept head is a record, not a lane

    def test_another_jobs_split_or_a_split_with_its_own_line_is_refused(self):
        with self.assertRaisesRegex(ValueError, "branch naming the job"):
            editor.change(SOURCE, "keep", ["J-044", "fb-bot/integrate-j045c@abc1234"],
                          branches=[br("fb-bot/integrate-j045c", "abc1234def")])
        own = SOURCE + "J-044c muse #46 | its own line\n"
        with self.assertRaisesRegex(ValueError, "branch naming the job"):
            editor.change(own, "keep", ["J-044", "fb-bot/integrate-j044c@abc1234"],
                          branches=[br("fb-bot/integrate-j044c", "abc1234def")])


class EditorGuard(unittest.TestCase):
    """plan.py's in-flight guard reads a head older than the job's merge as history, as the machine does."""

    def run_editor(self, *argv, seen, issue=None):
        with tempfile.TemporaryDirectory() as d:
            path = pathlib.Path(d) / "plan.md"
            path.write_text(SOURCE, encoding="utf-8")
            err = io.StringIO()
            with mock.patch.object(editor, "GitHub") as hub, \
                    mock.patch.object(editor.core, "observe", return_value=seen), \
                    mock.patch.object(editor, "recent_flip", return_value=False), contextlib.redirect_stderr(err):
                hub.return_value.get.return_value = issue or {}
                code = editor.main(["--plan", str(path), "--expect", editor.sha(SOURCE), *argv])
            return code, err.getvalue(), path.read_text(encoding="utf-8")

    def seen(self, **more):
        return {"branches": [br("fb-bot/integrate-j044", "s1")], "prs": [], "inactive": frozenset(),
                "issues": [{"number": 45, "title": "J-045 next", "state": "open"}], **more}

    def test_a_refused_hold_exits_non_zero(self):
        code, err, text = self.run_editor("hold", "J-044", "pause", seen=self.seen())
        self.assertEqual(code, 2)
        self.assertIn("remote work in flight", err)
        self.assertEqual(text, SOURCE)

    def test_a_head_older_than_the_jobs_merge_is_not_in_flight(self):
        old = self.seen(merged_at={"J-044": "2026-11-14T10:00:00Z"},
                        head_at={"s1": dt.datetime(2026, 11, 14, 8, tzinfo=dt.timezone.utc)})
        code, err, text = self.run_editor("hold", "J-044", "pause", seen=old)
        self.assertEqual((code, err), (0, ""))
        self.assertIn("J-044 muse #44 hold: pause | split", text)
        code, err, text = self.run_editor("tidy", seen=old, issue={"state": "closed", "title": "J-044 split"})
        self.assertEqual((code, err), (0, ""))
        self.assertNotIn("J-044", text)
        new = self.seen(merged_at={"J-044": "2026-11-14T10:00:00Z"},
                        head_at={"s1": dt.datetime(2026, 11, 14, 11, tzinfo=dt.timezone.utc)})
        self.assertEqual(self.run_editor("hold", "J-044", "pause", seen=new)[0], 2)   # newer than the merge: live
        self.assertEqual(self.run_editor("hold", "J-044", "pause", seen=self.seen(
            merged_at={"J-044": "2026-11-14T10:00:00Z"}))[0], 2)   # no commit time read: never proven older


class SweepLeftovers(unittest.TestCase):
    """tools/board/sweep.py: a red branch whose job merged a PR after the branch's head raises no FIX-CI."""

    NOW = "2026-11-14T12:00:00+00:00"

    def run_sweep(self, merged):
        refs = [f"fb-bot/integrate-j014 {'a' * 40} 2026-11-14T08:00:00+00:00",
                f"fb-bot/integrate-j040 {'b' * 40} 2026-11-14T08:00:00+00:00"]
        reads = []

        def gh(path):
            reads.append(path)
            if path.startswith("repos/IssisX/ScraperX/pulls?state=open"):
                return []
            if path.startswith("repos/IssisX/ScraperX/pulls?state=closed"):
                return merged
            if "/actions/workflows/" in path:
                return {"workflow_runs": [{"id": 7, "created_at": "2026-11-14T08:01:00Z", "status": "completed",
                                           "html_url": "https://run/7"}]}
            if "/actions/runs/7/jobs" in path:
                return {"jobs": [{"name": "native-tests", "status": "completed", "conclusion": "failure"}]}
            raise AssertionError(path)

        def run(cmd, **kw):
            out = "\n".join(refs) if "for-each-ref" in cmd else ""
            return subprocess.CompletedProcess(cmd, 0, stdout=out, stderr="")

        real = sweep.dt.datetime

        class Clock(real):
            @classmethod
            def now(cls, tz=None):
                return real.fromisoformat(SweepLeftovers.NOW)

        out = io.StringIO()
        with mock.patch.object(sweep, "gh", gh), mock.patch.object(sweep.subprocess, "run", run), \
                mock.patch.object(sweep.dt, "datetime", Clock), mock.patch.object(sys, "argv", ["sweep.py"]), \
                contextlib.redirect_stdout(out):
            self.assertEqual(sweep.main(), 0)
        return out.getvalue(), reads

    def test_a_landed_jobs_old_red_branch_raises_no_fix_ci(self):
        merged = [{"number": 491, "merged_at": "2026-11-14T10:00:00Z", "body": "Job: J-014\n\nWork.",
                   "head": {"ref": "fb-bot/integrate-j014-v2"}},
                  {"number": 472, "merged_at": "2026-11-14T07:00:00Z", "body": "Job: J-040\n",
                   "head": {"ref": "fb-bot/integrate-j040-a"}},
                  {"number": 470, "merged_at": None, "body": "Job: J-040\n", "head": {"ref": "x"}}]
        text, _ = self.run_sweep(merged)
        self.assertNotIn("fb-bot/integrate-j014 ", text)   # #491 merged after its head: history
        self.assertIn("FIX-CI", text)
        self.assertIn("fb-bot/integrate-j040 ", text)      # #472 merged before its head: new red work, still FIX-CI

    def test_closed_prs_are_read_only_when_a_red_branch_needs_them(self):
        text, reads = self.run_sweep([])
        self.assertEqual(text.count("FIX-CI"), 2)
        self.assertEqual(sum("state=closed" in p for p in reads), 1)


if __name__ == "__main__":
    unittest.main()
