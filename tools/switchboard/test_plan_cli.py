"""Plan editor refuses implicit reassignment and stale writes."""
import importlib.util
import pathlib
import tempfile
import unittest

HERE = pathlib.Path(__file__).resolve().parents[1] / "board"
spec = importlib.util.spec_from_file_location("collab_plan_editor", HERE / "plan.py")
editor = importlib.util.module_from_spec(spec)
spec.loader.exec_module(editor)

SOURCE = "limit muse 2\nlimit claude 4\nJ-101 muse #101 | first\nJ-102 muse #102 | second\n"


class Editor(unittest.TestCase):
    def test_add_and_round_trip(self):
        got = editor.change(SOURCE, "add", ["J-103", "muse", "#103", "third", "job"])
        self.assertIn("J-103 muse #103 | third job", got)

    def test_give_requires_pushed_lane_handoff_reason(self):
        with self.assertRaisesRegex(ValueError, "remote work in flight"):
            editor.change(SOURCE, "give", ["J-101", "claude"], branches=["fb-bot/wip-j101"])
        got = editor.change(SOURCE, "give", ["J-101", "claude"],
                            branches=["fb-bot/wip-j101"], stop_work="pushed to remote")
        self.assertIn("J-101 claude #101", got)

    def test_hold_fails_if_job_in_review(self):
        with self.assertRaisesRegex(ValueError, "remote work in flight"):
            editor.change(SOURCE, "hold", ["J-101", "pause"], prs=[{"body": "Job: J-101\n", "state": "open"}])

    def test_recent_flip_requires_reason(self):
        with self.assertRaisesRegex(ValueError, "recently changed"):
            editor.change(SOURCE, "release", ["J-101"], flipped=True)
        self.assertIn("J-101 muse #101 | first", editor.change(SOURCE, "release", ["J-101"], flipped=True,
                                                                  again="Cory ordered it"))

    def test_limit_and_move_are_deterministic(self):
        got = editor.change(SOURCE, "limit", ["muse", "3"])
        self.assertIn("limit muse 3", got)
        got = editor.change(SOURCE, "move", ["J-102", "J-101"])
        self.assertLess(got.index("J-102"), got.index("J-101"))

    def test_tidy_requires_closed_issue_evidence(self):
        with self.assertRaisesRegex(ValueError, "explicit verified closed-job input"):
            editor.change(SOURCE, "tidy", [])

    def test_corrupt_source_is_never_edited(self):
        with self.assertRaisesRegex(ValueError, "plan invalid"):
            editor.change("bad order\n", "limit", ["muse", "3"])

    def test_atomic_write_checks_source(self):
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / "plan.md"
            p.write_text(SOURCE)
            with self.assertRaisesRegex(RuntimeError, "changed during editing"):
                editor.write_atomic(p, "outdated", "would erase newer work")
            self.assertEqual(p.read_text(), SOURCE)
            editor.write_atomic(p, SOURCE, SOURCE + "J-103 muse #103 | new\n")
            self.assertIn("J-103", p.read_text())


if __name__ == "__main__":
    unittest.main()
