"""Unit tests for J-103 (#422): the switchboard does the routine so the integrator wakes only for decisions.

(1) A bridged Muse post whose title is only "J-NNN started", or a bare ack, is not posted to #282 on its own: it
rides as one digest line on the next inbox comment that wakes, and on the hourly full pass. (2) A proof red only in
setup steps is re-run once per head and listed in the digest; red again on that head wakes as RED-TWICE; a build
or test step red is never re-run. (3) The J-072 call budgets: a re-run adds its one write and no read.
Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
No network, no clock, no files: GitHub and the time are given.
"""

from __future__ import annotations

import datetime as dt
import pathlib
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import switch  # noqa: E402
import test_inbox as ti  # noqa: E402
import test_sweep_calls as tsc  # noqa: E402

HEAD, OLD_HEAD, NOW = ti.HEAD, ti.OLD_HEAD, ti.NOW
REF = "fb-bot/integrate-j044l"


def step(name: str, conclusion: str = "success") -> dict:
    return {"name": name, "status": "completed", "conclusion": conclusion}


def job(name: str, conclusion: str, *steps: dict) -> dict:
    return {"name": name, "status": "completed", "conclusion": conclusion, "steps": list(steps)}


# 11-11: the runners' apt mirror outage. host-build failed in its package install, the audits in the render
# prerequisites, the Android build in the NDK install; the jobs after them were skipped. Step names are the proof's own.
SETUP_RED = [
    job("scope", "success", step("Set up job"), step("Checkout exact source")),
    job("host-build", "failure", step("Set up job"), step("Checkout exact source"),
        step("Verify story radio source assets and provenance"), step("Restore the compiler cache"),
        step("Install host build prerequisites", "failure"), step("Build native host graph", "skipped")),
    job("native-tests", "skipped"),
    job("godot-audits-and-mix", "failure", step("Set up job"), step("Checkout exact source"),
        step("Install render prerequisites and verified Godot 4.7 editor", "failure"),
        step("Run Godot at Fold aspect through native machine authority and capture proof", "skipped")),
    job("android-build", "failure", step("Set up job"), step("Configure Java 17"),
        step("Install Android NDK", "failure"), step("Cross-compile native graph for Android arm64", "skipped")),
    job("publish-checkpoint", "failure"),   # not counted off trunk
]
SETUP_PROOF = "red: host-build, godot-audits-and-mix, android-build"
# A real red: a test step failed.
TEST_RED = [
    job("host-build", "success", step("Install host build prerequisites"), step("Build native host graph")),
    job("native-tests", "failure", step("Set up job"), step("Download host native binaries"),
        step("Run CTest Matron slice group 1 (ladder, hearing, climb-support, crash, tower)", "failure")),
]


def run(rid: int, head: str = HEAD, hours: float = 0.2, attempt: int = 1, branch: str = REF, **kw) -> dict:
    r = ti.run(rid, branch, head, hours, **kw)
    r["run_attempt"] = attempt
    return r


class GH(ti.FakeGH):
    """test_inbox's fake, with the proof-run listing filtered by head_sha as GitHub filters it, and a re-run that
    can be refused."""

    refuse_rerun = False

    def route(self, path):
        if "/actions/workflows/" in path and "head_sha=" in path:
            sha = path.split("head_sha=", 1)[1].split("&", 1)[0]
            self.gets.append(path)
            return {"workflow_runs": [r for r in self.runs if r.get("head_sha") == sha]}
        return super().route(path)

    def write(self, method, path, body=None, what=""):
        if path.endswith("/rerun") and self.refuse_rerun:
            raise switch.GitHubError(method, path, "gh: Resource not accessible by integration (HTTP 403)", 403)
        return super().write(method, path, body, what)

    def reruns(self) -> list[str]:
        return [p for m, p, _ in self.writes if m == "POST" and p.endswith("/rerun")]


def bridge(cid: int, title: str, text: str, number: int = 292, hours: float = 1.0) -> dict:
    """A Muse mail as muse_bridge.gs posts it."""
    return ti.comment(cid, number, f"**Muse (via email bridge): {title}**\n\n{text}\n\n_Mailed Nov 6 1:00 PM Central._",
                      hours)


STARTED = ("J-094 started", "J-094 started 10:15. Lane building the weight-key probe on fb-bot/integrate-j094: plate/pan/"
                            "stop rig, measure energy + peak force at the stop at 90 Hz.")


class StartedAndAcks(unittest.TestCase):
    def test_a_started_only_post_is_digested_not_posted(self):
        gh = GH(comments=[bridge(501, *STARTED)], open_issues=[292])
        sw = ti.make(gh)
        self.assertEqual(sw.inbox([]), 0)
        self.assertEqual(gh.posts, [], "a started post alone wakes no one")
        (held,) = sw.digest_held
        self.assertEqual((held.kind, held.target, held.head), ("STARTED", "#292", "501"))
        self.assertIn("INBOX       nothing new; 1 held for the digest (J-103)", sw.lines)
        # the next sweep has an item that wakes: the started post rides on it as one digest line
        sw2 = ti.make(gh)
        sw2.digest_held = sw.digest_held
        self.assertEqual(sw2.inbox([ti.human_merge()]), 2)
        (number, body), = gh.posts
        self.assertEqual(number, switch.INBOX_PR)
        lines = body.split("\n")
        self.assertIn("1 new for the integrator at", lines[2])
        self.assertTrue(lines[4].startswith("- MERGE-BY-HAND [#256]"))
        self.assertTrue(lines[5].startswith("- digest, nothing to decide: STARTED #292 Muse: J-094 started "
                                            "https://github.com/IssisX/ScraperX/issues/292#issuecomment-501"), lines[5])
        self.assertEqual(ti.ring(gh), [switch.inbox_key("MERGE-BY-HAND", "#256", ti.HEAD),
                                       switch.inbox_key("STARTED", "#292", "501")])
        self.assertEqual(sw2.digest_held, ())
        sw3 = ti.make(gh)   # carried once: never again
        self.assertEqual(sw3.inbox([ti.human_merge()]), 0)
        self.assertEqual(sw3.digest_held, ())

    def test_started_with_a_name_and_bare_acks_are_digested(self):
        for title, text, kind in (("J-040 take_blow started", "J-040 take_blow started 10:32. Lane on fb-bot/"
                                   "integrate-j040 per your ruling: real external blows count while Down.", "STARTED"),
                                  ("MUSE_TO_CLAUDE: J-043 started", "Lane building your two rulings.", "STARTED"),
                                  ("J-101 ack", "Got it, on the lane now.", "ACK"),
                                  ("Acknowledged", "Will do.", "ACK"),
                                  # the reconciler's notice id is no sha
                                  ("J-078 #396 re-ack", "/ack O-d7fa3f70\n\nRe-ack (re-notice arrived late). Status: "
                                                        "lane running, main pushes next.", "ACK")):
            gh = GH(comments=[bridge(502, title, text)], open_issues=[292])
            (it,) = ti.items_of(gh)
            self.assertEqual(it.kind, kind, title)
            self.assertEqual(ti.make(gh).inbox([]), 0, title)
            self.assertEqual(gh.posts, [], title)

    def test_a_post_with_a_question_or_a_finding_still_posts(self):
        for title, text in (("J-094 started", "Should the stop sit at 2 m or 3 m?"),
                            ("J-094 started", "Lane on fb-bot/integrate-j094 @ 56a593ef."),            # a sha
                            ("J-094 started", "host-build is red on the mirror again."),               # a red
                            ("J-101 ack", "Found the leak: the grip spring is uncharged."),            # a finding
                            ("J-092 taken", "No work here."),                                          # taken
                            ("J-094 not started", "Waiting on J-063."),
                            # 11-11, real: a re-ack whose status names a sha, and one about a red
                            ("J-059 #343 re-ack", "/ack O-2c7fcb6a\n\nRe-ack. Status: P1 #2 fixed @ b1177151, CI SUCCESS."),
                            ("J-085 proof red scope ack", "/ack O-9b77f12e\n\nThe scope red is real."),
                            ("J-040 take_blow — not deliberate, the guard is over-broad", "The guard."),
                            ("J-082 sha — lane touched it, pushed now", "Pushed.")):
            gh = GH(comments=[bridge(503, title, text)], open_issues=[292])
            (it,) = ti.items_of(gh)
            self.assertEqual(it.kind, "TO-CLAUDE", title + " / " + text)
            self.assertEqual(ti.make(gh).inbox([]), 1, title)
            self.assertTrue(gh.posts[0][1].split("\n")[4].startswith("- TO-CLAUDE #292 **Muse (via email bridge)"))

    def test_only_a_bridge_post_is_ever_digested(self):
        gh = GH(comments=[ti.comment(504, 292, "To Claude: J-094 started")], open_issues=[292])
        self.assertEqual([i.kind for i in ti.items_of(gh)], ["TO-CLAUDE"])

    def test_a_flush_carries_the_digest_alone(self):   # 11-13: no longer every hourly full pass (test_wake_cut)
        gh = GH(comments=[bridge(505, *STARTED)], open_issues=[292])
        sw = ti.make(gh)
        self.assertEqual(sw.inbox([], flush=True), 1)
        (number, body), = gh.posts
        lines = body.split("\n")
        self.assertTrue(lines[2].startswith("Nothing to decide at "), lines[2])
        self.assertTrue(lines[4].startswith("- digest, nothing to decide: STARTED #292"), lines[4])
        self.assertEqual(ti.ring(gh), [switch.inbox_key("STARTED", "#292", "505")])
        self.assertEqual(ti.make(gh).inbox([], flush=True), 0)   # listed once
        self.assertEqual(len(gh.posts), 1)
        self.assertEqual(ti.make(GH()).inbox([], flush=True), 0)   # nothing held: no comment
        self.assertEqual(len(gh.posts), 1)

    def test_a_held_entry_survives_the_sweep_state_and_a_long_digest_is_capped(self):
        held = tuple(switch.InboxItem("STARTED", f"#{300 + i}", str(i), f"STARTED #{300 + i} Muse: J-{i:03d} started")
                     for i in range(switch.MAX_DIGEST_ITEMS + 3))
        back = switch.digest_of([switch.digest_record(it) for it in held] + [{"kind": "FIX", "target": "#1", "head": "x",
                                                                               "line": "a wake kind is never held"},
                                                                              "junk", {"kind": "ACK"}])
        self.assertEqual(back, held)
        gh = GH()
        sw = ti.make(gh)
        sw.digest_held = back
        self.assertEqual(sw.inbox([ti.human_merge()]), 1 + switch.MAX_DIGEST_ITEMS)
        self.assertIn(" (and 3 more next time)", gh.posts[0][1])
        self.assertEqual(len(sw.digest_held), 3)


class OutageRerun(unittest.TestCase):
    def red(self, ref=REF, proof=SETUP_PROOF):
        s = ti.steered(ref=ref, proof=proof, committed=ti.ago(1), run_id=7)
        self.assertEqual(s.decision.action, "FIX-CI")
        return s

    def test_a_setup_only_red_reruns_once_and_the_second_red_wakes(self):
        s = self.red()
        gh = GH(runs=[run(7), run(6, OLD_HEAD, hours=1)], jobs={7: SETUP_RED, 6: SETUP_RED})
        sw = ti.make(gh)
        self.assertEqual(sw.inbox([s]), 0, "the first outage red wakes no one")   # was RED-TWICE: run 6 was red too
        self.assertEqual(sw.muse([s]), 0, "and mails Muse nothing")
        self.assertEqual(gh.reruns(), ["repos/IssisX/ScraperX/actions/runs/7/rerun"])
        self.assertEqual(gh.posts, [])
        (held,) = sw.digest_held
        self.assertEqual((held.kind, held.target, held.head), ("RERUN", "#256", HEAD))
        self.assertIn("proof red only in setup (`host-build: Install host build prerequisites; godot-audits-and-mix: "
                      "Install render prerequisites and verified Godot 4.7 editor; android-build: Install Android NDK`)"
                      ", re-run once ([proof](https://github.com/IssisX/ScraperX/actions/runs/7))", held.line)
        self.assertEqual(sw.reran_now, [HEAD])
        # the ledger and the reconciler read `steered` next: the PR is running again, no red of theirs to send
        sw.mark_reruns([s])
        self.assertEqual((s.facts.proof, s.decision.action), ("running", "WAIT"))
        s = self.red()
        # the re-run (attempt 2 of run 7) is red again on the same head: RED-TWICE wakes, the digest rides on it
        gh.runs = [run(7, attempt=2), run(6, OLD_HEAD, hours=1)]
        sw2 = ti.make(gh)
        sw2.digest_held, sw2.rerun_heads = sw.digest_held, frozenset(sw.reran_now)
        self.assertEqual(sw2.inbox([s]), 2)
        body = gh.posts[0][1]
        self.assertIn("- RED-TWICE [#256]", body)
        self.assertIn("host-build, godot-audits-and-mix, android-build failed again on attempt 2 of the same proof run",
                      body)
        self.assertIn("- digest, nothing to decide: RERUN [#256]", body)
        self.assertEqual(len(gh.reruns()), 1, "never a second re-run on a head")
        self.assertEqual([i.kind for i in ti.muse_of(gh, s)], ["RED", "RED-TWICE"])

    def test_a_head_rerun_before_is_not_rerun_again(self):
        s = self.red()
        for runs, heads in (([run(7, attempt=2)], frozenset()),                         # this run is past attempt 1
                            ([run(7), run(8, hours=0.1, attempt=2)], frozenset()),      # another run on the head is
                            ([run(7)], frozenset({HEAD}))):                             # an earlier sweep re-ran it
            gh = GH(runs=runs, jobs={7: SETUP_RED})
            sw = ti.make(gh)
            sw.rerun_heads = heads
            kinds = [i.kind for i in sw.inbox_items([s], frozenset())] + [i.kind for i in sw.muse_items([s], frozenset())]
            self.assertEqual(gh.reruns(), [], runs)
            self.assertNotIn("RERUN", kinds)
            self.assertIn("RED", kinds)

    def test_a_test_step_red_never_reruns(self):
        cases = {
            "a test step": TEST_RED,
            "a setup job beside a test step": SETUP_RED + TEST_RED[1:],
            "a failed job with no failed step": [job("host-build", "failure", step("Set up job"))],
            "a timed-out job": [job("host-build", "timed_out", step("Install host build prerequisites", "failure"))],
            "a verify step": [job("host-build", "failure", step("Verify every CTest test runs in exactly one shard",
                                                                "failure"))],
        }
        for why, jobs in cases.items():
            gh = GH(runs=[run(7), run(6, OLD_HEAD, hours=1)], jobs={7: jobs, 6: TEST_RED})
            sw = ti.make(gh)
            sw.inbox([self.red(proof="red: native-tests")])
            sw.muse([self.red(proof="red: native-tests")])
            self.assertEqual(gh.reruns(), [], why)
            self.assertEqual(sw.digest_held, (), why)
        gh = GH(runs=[run(7, conclusion="cancelled")], jobs={7: SETUP_RED})   # a cancelled run is not red here
        ti.make(gh).inbox([self.red()])
        self.assertEqual(gh.reruns(), [])

    def test_a_refused_rerun_leaves_the_red_as_it_was(self):
        gh = GH(runs=[run(7), run(6, OLD_HEAD, hours=1)], jobs={7: SETUP_RED, 6: SETUP_RED})
        gh.refuse_rerun = True
        sw = ti.make(gh)
        self.assertEqual([i.kind for i in sw.inbox_items([self.red()], frozenset())], ["RED-TWICE"])
        self.assertTrue(any(line.startswith("NOTE        re-run of proof run 7 refused") for line in sw.lines))

    def test_only_an_integrate_branch_is_rerun(self):
        ref = "muse/j044l"
        gh = GH(runs=[run(7, branch=ref)], jobs={7: SETUP_RED})
        ti.make(gh).inbox([self.red(ref=ref)])
        self.assertEqual(gh.reruns(), [])

    def test_a_setup_only_red_branch_with_no_pr_reruns_instead_of_red_branch(self):
        name = "claude/integrate-j103"
        gh = GH(runs=[run(40, hours=2, branch=name)], jobs={40: SETUP_RED}, heads={name: HEAD})
        sw = ti.make(gh)
        self.assertEqual([(i.kind, i.target) for i in sw.inbox_items([], frozenset())], [("RERUN", name)])
        self.assertEqual(sw.muse_items([], frozenset()), [])
        self.assertEqual(gh.reruns(), ["repos/IssisX/ScraperX/actions/runs/40/rerun"])
        gh.runs = [run(40, hours=2, branch=name, attempt=2)]   # red again on the re-run: RED-BRANCH, as before
        self.assertEqual([i.kind for i in ti.make(gh).inbox_items([], frozenset())], ["RED-BRANCH"])
        self.assertEqual(len(gh.reruns()), 1)


class Budgets(unittest.TestCase):
    """J-072: a re-run costs its one write and no read: its jobs and the head's runs are the reads pr_facts made."""

    def sweep_with(self, jobs, attempt=1, state=None):
        prs = [tsc.pr(n) for n in range(361, 368)]
        head, ref = f"{366:040x}", "fb-bot/integrate-j366"
        finished = tsc.iso(NOW - dt.timedelta(minutes=1))
        r = {"id": 9, "head_branch": ref, "head_sha": head, "status": "completed", "conclusion": "failure",
             "run_attempt": attempt, "created_at": tsc.iso(NOW - dt.timedelta(minutes=40)), "updated_at": finished,
             "html_url": "https://example.invalid/run/9"}
        gh = tsc.CountingGH(prs, runs=[r], jobs={9: jobs})
        status, read, sw = tsc.sweep(gh, state or tsc.last_state(prs))
        self.assertEqual((status, read), (0, [366]))
        return gh, sw

    @staticmethod
    def split(calls):
        writes = [c for c in calls if c.split(" ", 1)[0] in ("POST", "PATCH", "PUT", "DELETE")]
        return [c for c in calls if c not in writes], writes

    def test_a_rerun_adds_one_write_and_no_read(self):
        real, _ = self.sweep_with(TEST_RED)
        outage, sw = self.sweep_with(SETUP_RED)
        real_reads, real_writes = self.split(real.calls)
        reads, writes = self.split(outage.calls)
        self.assertEqual(writes, ["POST repos/IssisX/ScraperX/actions/runs/9/rerun"])   # no Muse mail, no inbox post
        self.assertTrue(real_writes)   # the real red mails Muse
        for path in set(reads):   # every read the re-run sweep made, the real red's sweep made as often or more
            self.assertLessEqual(reads.count(path), real_reads.count(path), path)
        self.assertLessEqual(len(outage.calls), tsc.IDLE + tsc.FIRST_PR_MAX + 1, outage.calls)
        timers = outage.state["steer"]["timers"]
        self.assertEqual(timers["rerun"], [f"{366:040x}"])
        self.assertEqual([d["kind"] for d in timers["digest"]], ["RERUN"])
        # the next sweep reads the state: the head is not re-run again, and the digest rides on the RED-TWICE
        self.assertNotIn(f"POST issues/{switch.INBOX_PR}/comments", outage.calls)
        again, sw2 = self.sweep_with(SETUP_RED, attempt=2, state=dict(outage.state["steer"]))
        self.assertFalse([c for c in again.calls if c.endswith("/rerun")])
        self.assertEqual(again.calls.count(f"POST issues/{switch.INBOX_PR}/comments"), 1)   # a dry run: printed
        inbox = [line for line in sw2.lines if line.startswith(switch.INBOX_HEADER)]
        self.assertEqual(len(inbox), 1)
        self.assertIn("- RED-TWICE [#366]", inbox[0])
        self.assertIn("- digest, nothing to decide: RERUN [#366]", inbox[0])
        self.assertNotIn("digest", again.state["steer"]["timers"])

    def test_the_idle_sweep_is_unchanged(self):
        prs = [tsc.pr(n) for n in range(361, 368)]
        gh = tsc.CountingGH(prs)
        tsc.sweep(gh, tsc.last_state(prs))
        self.assertEqual(len(gh.calls), tsc.IDLE, gh.calls)


if __name__ == "__main__":
    unittest.main()
