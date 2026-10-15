import argparse
import json
from pathlib import Path
import sys
import tempfile
import time
import unittest

import mechanics_trial as trial


class TrialLimits(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.state = self.root / "state.json"
        self.source = self.root / "source.cpp"
        self.source.write_text("actual source")
        self.call("start", objective="finite physical mechanism", budget=600)

    def call(self, action, **options):
        return trial.run(argparse.Namespace(state=self.state, action=action, **options))

    def execute(self, code="pass", stage="bench", requires=()):
        return self.call("run", stage=stage, source=[self.source], requires=requires,
                         command=[sys.executable, "-c", code])

    def test_failure_status_and_persistent_two_failure_limit(self):
        self.assertEqual(self.execute("raise SystemExit(7)"), 7)
        self.assertEqual(self.execute("raise SystemExit(8)"), 8)
        with self.assertRaisesRegex(ValueError, "reassessment"):
            self.execute()
        self.assertEqual(len(json.loads(self.state.read_text())["trials"]), 2)

    def test_failed_prerequisite_blocks_world(self):
        self.assertEqual(self.execute("raise SystemExit(1)"), 1)
        marker = self.root / "world-started"
        with self.assertRaisesRegex(ValueError, "no passing prerequisite"):
            self.execute("from pathlib import Path; Path(" + repr(str(marker)) + ").touch()",
                         stage="world", requires=["bench"])
        self.assertFalse(marker.exists())

    def test_changed_source_invalidates_passing_prerequisite(self):
        self.assertEqual(self.execute(), 0)
        self.source.write_text("changed geometry")
        with self.assertRaisesRegex(ValueError, "source changed"):
            self.execute(stage="world", requires=["bench"])

    def test_same_source_pass_allows_world(self):
        self.assertEqual(self.execute(), 0)
        self.assertEqual(self.execute("print('world')", stage="world", requires=["bench"]), 0)

    def test_later_failure_invalidates_older_pass(self):
        self.execute()
        self.execute("raise SystemExit(1)")
        with self.assertRaisesRegex(ValueError, "no passing prerequisite"):
            self.execute(stage="world", requires=["bench"])

    def test_identical_repeat_is_blocked_without_spending_another_attempt(self):
        self.execute("raise SystemExit(1)")
        with self.assertRaisesRegex(ValueError, "already executed"):
            self.execute("raise SystemExit(1)")
        self.assertEqual(len(json.loads(self.state.read_text())["trials"]), 1)

    def test_matching_receipt_reuse_does_not_execute_again(self):
        receipt = self.root / "accepted.json"
        receipt.write_text(json.dumps({"exit": 0, "sources": {str(self.source): trial.digest(self.source)}}))
        self.call("reuse", stage="accepted-bench", receipt=receipt)
        self.assertEqual(self.execute(stage="world", requires=["accepted-bench"]), 0)
        self.source.write_text("changed")
        with self.assertRaises(ValueError):
            self.call("reuse", stage="stale", receipt=receipt)

    def test_time_limit_kills_trial_descendants_and_blocks_next(self):
        state = json.loads(self.state.read_text())
        state["budget_seconds"] = .15
        state["started"] = time.time()
        trial.save(self.state, state)
        marker = self.root / "escaped-child"
        descendant = "import time; from pathlib import Path; time.sleep(.5); Path(" + repr(str(marker)) + ").touch()"
        code = "import subprocess,sys,time; subprocess.Popen([sys.executable,'-c'," + repr(descendant) + "]); time.sleep(5)"
        self.assertEqual(self.execute(code), 124)
        time.sleep(.6)
        self.assertFalse(marker.exists())
        with self.assertRaisesRegex(ValueError, "budget"):
            self.execute()

    def test_reassessment_requires_evidence_and_preserves_history(self):
        self.execute("raise SystemExit(1)")
        self.execute("raise SystemExit(2)")
        evidence = self.root / "contacts.log"
        with self.assertRaises(ValueError):
            self.call("reassess", evidence=evidence, cause="pocket blocks return", change="deepen real pocket")
        evidence.write_text("actual retained-bar contact and leaf sweep receipt")
        self.call("reassess", evidence=evidence, cause="pocket blocks return", change="deepen real pocket")
        self.assertEqual(self.execute(), 0)
        state = json.loads(self.state.read_text())
        self.assertEqual(len(state["trials"]), 3)
        self.assertEqual(state["reassessments"][0]["previous_failures"], 2)


if __name__ == "__main__":
    unittest.main()
