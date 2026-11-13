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


class Record(unittest.TestCase):
    """J-072, REC: the bridge posts each CLAUDE_TO_MUSE mail the integrator sent by hand as a comment, under the
    token's login (every agent's login, and Cory's). These read a record the real bridge made (test_bridge_smoke.js
    --record) from a mail whose text tries every parser that reads comments from that login: the trackers must read
    it as Claude's post to Muse, and nothing in the mail's text may act (a verdict, a command, a budget, a keep)."""

    @classmethod
    def setUpClass(cls):
        import json
        node = shutil.which("node")
        if not node:
            raise unittest.SkipTest("node is not on the path: no record to read")
        got = subprocess.run([node, str(HERE / "test_bridge_smoke.js"), "--record"], capture_output=True, text=True,
                             timeout=60)
        assert got.returncode == 0, got.stdout[-2000:] + got.stderr[-1000:]
        made = json.loads(got.stdout)
        cls.body, cls.login, cls.mail = made["body"], made["login"], made["mail"]
        import yaml
        import rules
        cls.team = rules.team_from_config(yaml.safe_load((HERE.parents[1] / "work/agents.yml").read_text(encoding="utf-8")))

    def test_the_mail_itself_would_act_if_posted_as_it_is(self):
        # the control: posted raw under this login, the same text is a command, a budget and a keep
        import ledger
        import reconciler
        import rules
        self.assertTrue(list(reconciler.CMD.finditer(self.mail)))
        self.assertEqual(rules.budget_override([(self.mail, self.login)], self.team), "500")
        self.assertTrue(ledger.KEEP.findall(self.mail))

    def test_no_line_of_the_record_is_a_command_a_budget_or_a_keep(self):
        import ledger
        import reconciler
        import rules
        self.assertEqual(list(reconciler.CMD.finditer(self.body)), [])
        self.assertEqual(rules.budget_override([(self.body, self.login)], self.team), "")
        self.assertEqual(ledger.KEEP.findall(self.body), [])

    def test_the_record_is_never_a_verdict(self):
        import ledger   # puts tools/board on the path
        import asks
        import rules
        self.assertIsNone(rules.parse_verdict(self.body, "2026-11-10T15:00:00Z", self.login, self.team))
        sw = switch.Switch.__new__(switch.Switch)
        sw.team = self.team
        c = {"body": self.body, "user": {"login": self.login}, "created_at": "2026-11-10T15:00:00Z"}
        self.assertEqual(sw.bridged_verdicts(343, [c]), [])
        self.assertEqual(asks.VERDICT.findall(self.body), [])
        self.assertIsNone(rules.parse_codex_state(self.body, "2026-11-10T15:00:00Z", self.login))
        del ledger

    def test_the_record_holds_no_marker_and_no_mention(self):
        import ledger
        import reconciler
        import re
        self.assertNotIn("<!--", self.body)
        for marker in (switch.MUSE_KEYS, switch.INBOX_KEYS, switch.PIPE_KINDS, switch.BRIDGE_TOLD, ledger.NOTICE_KEYS,
                       reconciler.SENT_RE):
            self.assertEqual(marker.findall(self.body), [], marker.pattern)
        self.assertIsNone(re.search(r"@(?=[\w-])", self.body))
        self.assertFalse(self.body.lstrip().startswith(switch.BRIDGE_POST))

    def test_the_trackers_read_it_as_claudes_post_to_muse_and_as_the_answer_to_her_ask(self):
        import datetime as dt
        import ledger   # puts tools/board on the path
        import asks
        import reconciler
        self.assertEqual(asks.author(self.body), "Claude")
        self.assertEqual(reconciler.poster(self.body), "Claude")
        self.assertIn("Muse", asks.addressees(self.body))
        self.assertIn("Muse", reconciler.addressees(self.body))
        t = dt.datetime(2026, 11, 10, 15, 0, tzinfo=dt.timezone.utc)
        ask = {"id": "1", "n": 350, "t": t, "url": "u1",
               "body": "**Muse (via email bridge): J-044 To Claude: take the wreck fix?**\n\nWhich one?"}
        record = {"id": "2", "n": 350, "t": t + dt.timedelta(minutes=30), "url": "u2", "body": self.body}
        start = t - dt.timedelta(hours=1)
        to_claude = lambda cs: [a["id"] for a in asks.unanswered(cs, {350}, start) if a["target"] == "Claude"]  # noqa: E731
        self.assertEqual(to_claude([dict(ask)]), ["1"])                    # Muse's ask, open until Claude answers
        self.assertEqual(to_claude([dict(ask), dict(record)]), [])        # the record of Claude's mail answers it
        del ledger

    def test_the_record_never_wakes_the_integrator_about_its_own_mail(self):
        import test_inbox as ti
        first = "J-044 your proposal: yes. To Claude the rest."
        for body, want in ((f"CLAUDE_TO_MUSE: {first}\n\n{self.body}", 0), (f"{first}\n\n{self.body}", 1)):
            gh = ti.FakeGH(comments=[ti.comment(77, 350, body, 1.0, self.login)], open_issues=[350])
            self.assertEqual(len(ti.items_of(gh)), want, body[:40])


if __name__ == "__main__":
    unittest.main()
