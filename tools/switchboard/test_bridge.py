"""J-072: the Muse bridge (muse_bridge.gs) as the repo holds it.

The version check needs no node. The smoke harness (test_bridge_smoke.js: the label race, mail to a closed issue,
the clock's version, the watchdog's incidents) runs under node when node is on the path, as on the ubuntu-24.04
runner; without node it is skipped, and says so.
Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
"""

from __future__ import annotations

import pathlib
import shutil
import subprocess
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import switch  # noqa: E402

GS = HERE / "muse_bridge.gs"


class Version(unittest.TestCase):
    def test_the_constant_is_the_files_own_hash(self):
        # any edit of the bridge changes its version, so the sweep can tell the live script is stale
        text = GS.read_text(encoding="utf-8")
        m = switch.BRIDGE_VERSION_LINE.search(text)
        self.assertIsNotNone(m, "muse_bridge.gs has no `var BRIDGE_VERSION = '...';` line")
        self.assertEqual(m.group(1), switch.bridge_version(text),
                         "set BRIDGE_VERSION in muse_bridge.gs to switch.bridge_version() of the file")

    def test_the_hash_ignores_the_constant_and_sees_any_other_edit(self):
        text = GS.read_text(encoding="utf-8")
        v = switch.bridge_version(text)
        self.assertEqual(switch.bridge_version(switch.BRIDGE_VERSION_LINE.sub("var BRIDGE_VERSION = 'ffffffffffff';",
                                                                              text, count=1)), v)
        self.assertNotEqual(switch.bridge_version(text + "\n// an edit"), v)

    def test_the_run_name_and_the_switch_agree(self):
        wf = (HERE.parents[1] / ".github/workflows/switchboard.yml").read_text(encoding="utf-8")
        self.assertIn("format('Switchboard sweep (bridge {0})', inputs.bridge_version)", wf)
        self.assertRegex("Switchboard sweep (bridge 0123456789ab)", switch.BRIDGE_RUN)


class LiveVersion(unittest.TestCase):
    """The sweep's check that the live bridge is trunk's (switch.py bridge_version_health, in pipe_health)."""

    def sw(self, titles_ages):
        import datetime as dt
        import test_inbox
        now = test_inbox.NOW
        runs = [{"display_title": t, "created_at": (now - dt.timedelta(minutes=m)).strftime("%Y-%m-%dT%H:%M:%SZ")}
                for t, m in titles_ages]
        gh = test_inbox.FakeGH(runs=runs)
        sw = test_inbox.make(gh)
        sw.root = HERE.parents[1]
        return sw, gh

    def want(self):
        return switch.bridge_version(GS.read_text(encoding="utf-8"))

    def test_the_live_bridge_at_trunks_version_is_healthy(self):
        sw, _ = self.sw([(f"Switchboard sweep (bridge {self.want()})", 5)])
        self.assertIsNone(sw.bridge_version_health())

    def test_a_stale_or_unnamed_bridge_is_found(self):
        sw, _ = self.sw([("Switchboard sweep (bridge 0123456789ab)", 5), (f"Switchboard sweep (bridge {self.want()})", 40)])
        line, pair = sw.bridge_version_health()
        self.assertEqual(pair, ("0123456789ab", self.want()))
        self.assertIn("stale", line)
        sw, _ = self.sw([("Switchboard sweep", 5)])   # today's live bridge: its clock names no version
        line, pair = sw.bridge_version_health()
        self.assertEqual(pair, ("none", self.want()))

    def test_one_pipe_alarm_per_version_pair(self):
        sw, gh = self.sw([("Switchboard sweep", 5)])
        found = sw.pipe_health()
        self.assertTrue(any(line.startswith("BRIDGE VERSION") for line in found))
        self.assertEqual(len(gh.posts), 1)
        self.assertIn(switch.BRIDGE_TOLD_MARK.format(live="none", trunk=self.want()), gh.posts[0][1])
        gh.by_issue[switch.BOARD_ISSUE][0]["created_at"] = "2026-11-06T19:00:00Z"
        sw.pipe_health()
        self.assertEqual(len(gh.posts), 1)   # the same pair is not alarmed twice


class Smoke(unittest.TestCase):
    def test_the_bridge_smoke_harness_passes(self):
        node = shutil.which("node")
        if not node:
            self.skipTest("node is not on the path: test_bridge_smoke.js not run")
        got = subprocess.run([node, str(HERE / "test_bridge_smoke.js")], capture_output=True, text=True, timeout=60)
        self.assertEqual(got.returncode, 0, got.stdout[-2000:] + got.stderr[-1000:])
        self.assertIn("0 failed", got.stdout)


if __name__ == "__main__":
    unittest.main()
