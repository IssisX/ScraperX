"""The collaboration holes closed on 11-13, one class each (03_EXECUTION/SYSTEM_PLAN.md item 3, lifted from #502):

RedTrunk     trunk's full proof red pages its owners at once, once per red streak, until a green full proof (J-129's
             doubled order key 1100 sat red about a day, hidden by notes-only pushes whose runs skip the build).
Merged       a merge the switch makes is one MERGED inbox line (11-14: #555 and #396 were found only by hand).
Held         a job held in the plan gets no idle nag (QUIET, FIX-STUCK, RED-STUCK); its new red or FIX still shows.
Take         a Muse job red twice on the same failure is Claude's: Muse is mailed once, the integrator told until the
             plan says claude (11-14: J-100 and J-014 sat in Muse's hands, missed).
Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
No network, no clock, no files beyond a temporary checkout: GitHub and the time are given.
"""

from __future__ import annotations

import pathlib
import sys
import tempfile
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import rules  # noqa: E402
import switch  # noqa: E402
import test_inbox as ti  # noqa: E402

NOW = ti.NOW
TRUNK = rules.TRUNK
BROKE = "b" * 40      # the merge that broke trunk
NOTES = "c" * 40      # a notes-only push after it
FIXED = "d" * 40      # the fix's merge
BUILT_RED = [{"name": "native-tests", "status": "completed", "conclusion": "failure"},
             {"name": "godot-touch", "status": "completed", "conclusion": "success"}]
HOST_RED = [{"name": "host-build", "status": "completed", "conclusion": "failure"},     # run 43320094966 on 11-12:
            {"name": "native-tests", "status": "completed", "conclusion": "skipped"},  # its tests skip behind it
            {"name": "godot-touch", "status": "completed", "conclusion": "skipped"}]
SKIPPED = [{"name": "host-build", "status": "completed", "conclusion": "skipped"},
           {"name": "native-tests", "status": "completed", "conclusion": "skipped"},
           {"name": "godot-touch", "status": "completed", "conclusion": "skipped"},
           {"name": "scope", "status": "completed", "conclusion": "success"}]
BUILT_GREEN = [{"name": "native-tests", "status": "completed", "conclusion": "success"},
               {"name": "godot-touch", "status": "completed", "conclusion": "success"}]


class TrunkGH(ti.FakeGH):
    """The inbox's GitHub, with trunk's commits (their merge messages) readable."""

    def __init__(self, *a, messages=None, **kw):
        super().__init__(*a, **kw)
        self.messages = dict(messages or {})

    def route(self, path):
        base = path.split("?")[0]
        if "/commits/" in base and base.rsplit("/", 1)[1] in self.messages:
            self.gets.append(path)
            return {"sha": base.rsplit("/", 1)[1], "commit": {"message": self.messages[base.rsplit("/", 1)[1]]}}
        return super().route(path)


class RedTrunk(unittest.TestCase):
    def gh(self, runs, jobs, state=None):
        gh = TrunkGH(runs=runs, jobs=jobs, open_issues=[340],
                     messages={BROKE: "Merge pull request #490 from fb-bot/integrate-j129-objective-core (switchboard)",
                               "e" * 40: "Merge pull request #497 from IssisX/claude/integrate-wake-cut\n\nJob"})
        gh.state = dict(state or {})
        return gh

    def test_a_red_full_trunk_proof_wakes_the_integrator_and_mails_the_breakers_owner_at_once(self):
        gh = self.gh([ti.run(41, TRUNK, BROKE, 5)], {41: BUILT_RED})
        sw = ti.make(gh)
        self.assertEqual(sw.inbox([]), 1)                      # at once: no hold, no digest
        (number, body), = gh.posts
        self.assertEqual(number, switch.INBOX_PR)
        self.assertIn("- RED-TRUNK trunk's full proof is red on native-tests", body)
        self.assertIn("the merge of #490 (`fb-bot/integrate-j129-objective-core`)", body)
        # the plan's words: the owner fixes; no green fix head in 60 min, a helper reverts that merge; no freeze
        self.assertIn("owner Muse. Muse fixes it; no green fix head in 60 min, a helper reverts it "
                      "(`git revert -m 1 bbbbbbbb`, its own PR). No merge freeze", body)
        kept = gh.state["trunk_proof"]
        self.assertEqual((kept["state"][:3], kept["first_red"], kept["pr"]), ("red", BROKE, 490))
        # the owner of the commit that broke it: Muse's PR, so Muse is mailed too (comment + wake:muse on the board)
        self.assertEqual(ti.make(gh).muse([]), 1)
        (where, mail), = [p for p in gh.posts if p[0] != switch.INBOX_PR]
        self.assertEqual(where, switch.BOARD_ISSUE)
        self.assertTrue(mail.startswith("CLAUDE_TO_MUSE: "), mail)
        self.assertIn("RED-TRUNK", mail)
        self.assertIn(("POST", f"repos/{gh.repo}/issues/{switch.BOARD_ISSUE}/labels", {"labels": [switch.MUSE_LABEL]}),
                      gh.writes)
        self.assertEqual(ti.later(gh).inbox([]), 0)            # once per red streak

    def test_a_notes_only_green_run_never_clears_it_and_a_green_full_proof_does(self):
        gh = self.gh([ti.run(41, TRUNK, BROKE, 5)], {41: BUILT_RED})
        ti.make(gh).inbox([])
        gh.runs.append(ti.run(42, TRUNK, NOTES, 3, conclusion="success"))
        gh.jobs[42] = SKIPPED
        sw = ti.later(gh, 60)
        self.assertEqual(sw.inbox([]), 0)                      # the same streak: listed once
        self.assertEqual((sw.trunk_proof(gh.runs)["state"][:3], gh.state["trunk_proof"]["first_red"]), ("red", BROKE))
        ti.later(gh, 90).inbox([])                             # a judged run's jobs are never read again
        self.assertEqual(sum("/actions/runs/42/jobs" in g for g in gh.gets), 1)
        gh.runs.append(ti.run(43, TRUNK, FIXED, 1, conclusion="success"))
        gh.jobs[43] = BUILT_GREEN
        sw = ti.later(gh, 120)
        self.assertEqual(sw.trunk_items(gh.runs, frozenset()), [])
        self.assertEqual((gh.state["trunk_proof"]["state"], gh.state["trunk_proof"]["sha"]), ("green", FIXED))

    def test_a_failed_build_is_a_built_run_though_its_tests_skip(self):
        self.assertTrue(switch.built_run(HOST_RED))
        self.assertFalse(switch.built_run(SKIPPED))
        gh = self.gh([ti.run(41, TRUNK, BROKE, 5)], {41: HOST_RED})
        (it,) = ti.make(gh).trunk_items(gh.runs, frozenset())
        self.assertIn("red on host-build", it.line)

    def test_a_red_older_than_the_runs_window_stays_red_from_the_state(self):
        kept = {"sha": BROKE, "run": 41, "url": "u", "state": "red: native-tests", "at": ti.ago(30),
                "created": ti.ago(31), "first_red": BROKE, "pr": 490, "branch": "fb-bot/integrate-j129-objective-core"}
        gh = self.gh([ti.run(42, TRUNK, NOTES, 3, conclusion="success")], {42: SKIPPED}, {"trunk_proof": kept})
        (it,) = ti.make(gh).trunk_items(gh.runs, frozenset())
        self.assertEqual((it.kind, it.dest, it.head), ("RED-TRUNK", "both", BROKE))

    def test_a_red_run_still_going_when_newer_notes_only_runs_finish_is_read_once_it_ends(self):
        # 11-14, run 43595346281: the full proof on the merge of #547 was still running when notes-only pushes after
        # it finished; that sweep marked them judged past its created time, so its red end never fired RED-TRUNK
        gh = self.gh([ti.run(40, TRUNK, FIXED, 3, conclusion="success"),
                      ti.run(41, TRUNK, BROKE, 1, conclusion=None, status="in_progress"),
                      ti.run(42, TRUNK, NOTES, 0.5, conclusion="success"),
                      ti.run(43, TRUNK, "f" * 40, 0.3, conclusion="success")],
                     {40: BUILT_GREEN, 42: SKIPPED, 43: SKIPPED})
        self.assertEqual(ti.make(gh).trunk_items(gh.runs, frozenset()), [])   # sweep 1: green so far, 41 running
        self.assertEqual(gh.state["trunk_proof"]["state"], "green")
        self.assertLess(gh.state["trunk_proof"]["checked"], gh.runs[1]["created_at"])
        gh.runs[1].update(status="completed", conclusion="failure")           # sweep 2: it ended red
        gh.jobs[41] = BUILT_RED
        (it,) = ti.later(gh, 25).trunk_items(gh.runs, frozenset())
        self.assertEqual((it.kind, it.head), ("RED-TRUNK", BROKE))
        self.assertIn("red on native-tests", it.line)
        self.assertEqual(gh.state["trunk_proof"]["checked"], gh.runs[3]["created_at"])   # nothing unfinished: judged

    def test_more_skip_the_build_runs_than_one_sweep_reads_are_walked_past_to_the_red_run(self):
        # 11-14: 8 finished notes-only runs above a finished red built run, with 6 reads a sweep, read the same newest
        # 6 every sweep and never reached the red one; a run judged skip-the-build is remembered and never read again
        notes = [ti.run(50 + k, TRUNK, f"{k:x}" * 40, 1 - 0.1 * k, conclusion="success") for k in range(8)]
        gh = self.gh([ti.run(41, TRUNK, BROKE, 2)] + notes, {41: BUILT_RED, **{r["id"]: SKIPPED for r in notes}})
        self.assertGreater(len(notes), switch.TRUNK_RUN_READS)
        fired = [ti.later(gh, 25 * n).trunk_items(gh.runs, frozenset()) for n in range(3)]
        self.assertTrue(any(fired), "RED-TRUNK never fired")
        (it,) = next(f for f in fired if f)
        self.assertEqual((it.kind, it.head), ("RED-TRUNK", BROKE))
        for r in notes:   # each skip-the-build run's jobs were read once, never again
            self.assertEqual(sum(f"/actions/runs/{r['id']}/jobs" in g for g in gh.gets), 1, r["id"])
        self.assertLessEqual(len(gh.state["trunk_skipped"]), switch.TRUNK_SKIPPED_KEPT)

    def test_claudes_own_trunk_push_pages_only_the_integrator(self):
        gh = self.gh([ti.run(41, TRUNK, "e" * 40, 5)], {41: BUILT_RED})
        (it,) = ti.make(gh).trunk_items(gh.runs, frozenset())
        self.assertEqual((it.dest, gh.state["trunk_proof"]["branch"]), ("claude", "claude/integrate-wake-cut"))
        self.assertIn("owner Claude. Claude fixes it", it.line)
        self.assertEqual(ti.make(gh).muse_items([], frozenset()), [])



class MergeGH(ti.FakeGH):
    """The inbox's GitHub, whose merge PUT answers merged (or refuses, with `refuse`)."""

    def __init__(self, *a, refuse="", **kw):
        super().__init__(*a, **kw)
        self.refuse = refuse

    def write(self, method, path, body=None, what=""):
        if method == "PUT" and path.endswith("/merge"):
            self.writes.append((method, path, body))
            return {"merged": False, "message": self.refuse} if self.refuse else {"merged": True}
        return super().write(method, path, body, what)


class Merged(unittest.TestCase):
    def merge(self, gh, ref, number=555, now=NOW):
        s = ti.steered(number=number, ref=ref, verdicts=[ti.land()])
        return ti.make(gh, now).merge(s.facts, s.ctx, s.decision)

    def lines(self, gh):
        return [line for _, body in gh.posts if _ == switch.INBOX_PR for line in body.splitlines()
                if line.startswith(("- MERGED #", "- digest, nothing to decide: MERGED #"))]

    def test_the_integrators_own_merge_posts_at_once_one_line(self):
        gh = MergeGH()
        self.assertEqual(self.merge(gh, "claude/integrate-j072-merge-notice"), "")
        self.assertEqual(ti.make(gh).inbox([]), 1)                         # at once: no hold, no digest
        (line,) = self.lines(gh)
        self.assertIn("MERGED #555 `claude/integrate-j072-merge-notice` at " + ti.HEAD[:8], line)
        self.assertIn("trunk's proof now runs (The trunk proof is dispatched)", line)
        self.assertEqual(ti.later(gh, 60).inbox([]), 0)                    # one line per merge, never repeated
        self.assertEqual(len(self.lines(gh)), 1)
        self.assertEqual(gh.state["merged"], [])

    def test_a_muse_merge_never_posts_alone_and_rides_the_next_post(self):
        gh = MergeGH()
        self.merge(gh, "fb-bot/integrate-j129-objective-core")
        self.assertEqual(ti.make(gh).inbox([]), 0)                         # a report: never a post alone
        self.assertEqual([n for n, _ in gh.posts if n == switch.INBOX_PR], [])
        self.merge(gh, "claude/integrate-j072-merge-notice", number=556, now=ti.NOW)
        self.assertEqual(ti.later(gh, 5).inbox([]), 2)                     # the next post carries it
        self.assertEqual(sorted(l.split("MERGED ")[1].split()[0] for l in self.lines(gh)), ["#555", "#556"])
        self.assertTrue(any(l.startswith("- digest, nothing to decide: MERGED #555") for l in self.lines(gh)))
        self.assertEqual(ti.later(gh, 90).inbox([]), 0)
        self.assertEqual(len(self.lines(gh)), 2)

    def test_a_merge_in_a_sweep_that_posts_no_inbox_is_listed_by_the_next_post(self):
        gh = MergeGH()
        self.merge(gh, "claude/integrate-j072-merge-notice")              # a targeted or quiet sweep: no inbox part
        sw = ti.later(gh, 10)
        self.assertTrue(sw.inbox_clock())                                  # a quiet sweep's clock sees it due
        self.assertEqual(sw.inbox([]), 1)
        self.assertEqual(len(self.lines(gh)), 1)

    def test_a_refused_merge_and_a_dry_run_list_nothing(self):
        gh = MergeGH(refuse="Base branch was modified")
        self.assertIn("merge not done", self.merge(gh, "claude/integrate-j072-merge-notice"))
        dry = MergeGH(dry_run=True)
        self.assertEqual(self.merge(dry, "claude/integrate-j072-merge-notice"), "")
        for g in (gh, dry):
            self.assertEqual(g.state.get("merged") or [], [])
            self.assertEqual(ti.make(g).merged_items(), ())


SLICES = "native-tests (matron-slices-3)"
TRAVERSAL = "native-tests (athletic-traversal-2)"
PLAN = "limit claude 4\nlimit muse 10\nJ-100 {owner} #418 | She reads a load hung over her\n"


class Take(unittest.TestCase):
    """The real records: J-100 (#478) red on 9458db9f, de39529d and 966fb721, the same two native jobs failing on
    each; J-014 (#491) red on 2cbb1481, green on f6c4311c, red on a9c31437 (host-build alone: a trunk merge's
    build break) and red on 8d9cb6f3 (its tests)."""

    def setUp(self):
        self.root = pathlib.Path(tempfile.mkdtemp())
        (self.root / "work").mkdir()
        self.plan("muse")
        self.gh = ti.FakeGH(open_issues=[340])
        self.tests: dict[int, tuple[str, ...]] = {}
        self.minutes = 0

    def plan(self, owner):
        (self.root / "work/plan.md").write_text(PLAN.format(owner=owner), encoding="utf-8")

    def sweep(self, head, rid, tests=(), proof=None, ref="fb-bot/integrate-j100", number=478, job="J-100", gap=30):
        """One sweep after the PR's proof on `head` (run `rid`) finished: the inbox, then Muse's mail."""
        self.minutes += gap
        jobs = sorted({t.split(": ", 1)[0] for t in tests})
        proof = proof or ("red: " + ", ".join(jobs))
        self.tests[rid] = tuple(tests)
        self.gh.runs.append(ti.run(rid, ref, head, 0.1, conclusion="success" if proof == "green" else "failure"))
        self.gh.jobs[rid] = [{"name": j, "status": "completed", "conclusion": "failure"} for j in jobs]
        s = ti.steered(number=number, ref=ref, head=head, proof=proof, run_id=rid, job=job)
        posts = len(self.gh.posts)
        sw = ti.later(self.gh, self.minutes)
        sw.root = self.root
        sw.red_tests = lambda run_id: self.tests.get(run_id, ())
        sw.inbox([s])
        sw.muse([s])
        new = self.gh.posts[posts:]
        return ([b for n, b in new if n == switch.INBOX_PR], [b for n, b in new if n != switch.INBOX_PR])

    def takes(self, bodies):
        return [line for b in bodies for line in b.splitlines() if line.startswith("- TAKE ")]

    def test_j100_two_red_heads_on_the_same_failure_mail_muse_once_and_tell_the_integrator_until_the_plan_says_claude(self):
        inbox, mail = self.sweep("9458db9f" + "0" * 32, 1, [f"{SLICES}: FAIL: slab trap: she stopped 1.27 m from the rim",
                                                          f"{TRAVERSAL}: FAIL: climb: 0.31 m short"])
        self.assertEqual((self.takes(inbox), self.takes(mail)), ([], []))       # one red: no take yet
        # the next head fails the same checks by other amounts: the same failure
        inbox, mail = self.sweep("de39529d" + "0" * 32, 2, [f"{SLICES}: FAIL: slab trap: she stopped 1.99 m from the rim",
                                                          f"{TRAVERSAL}: FAIL: climb: 0.12 m short"])
        (line,) = self.takes(inbox)
        self.assertIn("TAKE J-100 [#478]", line)
        self.assertIn("Give it to claude in work/plan.md", line)
        self.assertIn("slab trap: she stopped # m from the rim", "\n".join(inbox))   # its failing tests, named
        (to_muse,) = self.takes(mail)
        self.assertIn("please push what you have as it is, mail MUSE_TO_CLAUDE that you stopped J-100", to_muse)
        said = to_muse.split(", J-100: ", 1)[1]   # the TAKE's own words (the PR prefix is every Muse item's)
        for word in ("must", "ignore", "supersede", "replace", "`", "<!--", "right now"):
            self.assertNotIn(word, said.lower())
        # 966fb721: a cancelled push run, then a red dispatch on the same head; within the hour nothing is re-sent
        self.gh.runs.append(ti.run(30, "fb-bot/integrate-j100", "966fb721" + "0" * 32, 0.2, conclusion="cancelled"))
        inbox, mail = self.sweep("966fb721" + "0" * 32, 3, [f"{SLICES}: FAIL: slab trap: she stopped 1.99 m from the rim"])
        self.assertEqual((self.takes(inbox), self.takes(mail)), ([], []))       # Muse once per job; the hour not up
        inbox, mail = self.sweep("966fb721" + "0" * 32, 3, gap=35)            # past the hour: listed again, once
        self.assertEqual((len(self.takes(inbox)), self.takes(mail)), (1, []))
        self.plan("claude")                                                    # the integrator gave it to claude
        inbox, mail = self.sweep("966fb721" + "0" * 32, 3, gap=70)
        self.assertEqual((self.takes(inbox), self.takes(mail)), ([], []))
        self.assertNotIn("J-100", self.gh.state["takes"])

    def test_j014_a_green_resets_and_a_different_failure_is_no_take_until_the_same_one_fails_again(self):
        ref, tests = "fb-bot/integrate-protected-j014", [f"{SLICES}: FAIL: stock: 0 carryable on ring 594.25 m"]
        run = dict(ref=ref, number=491, job="J-100")
        self.assertEqual(self.takes(self.sweep("2cbb1481" + "0" * 32, 11, tests, **run)[0]), [])
        self.sweep("f6c4311c" + "0" * 32, 12, proof="green", **run)               # green: the earlier red is fixed
        self.assertEqual(self.takes(self.sweep("a9c31437" + "0" * 32, 13,
                                               ["host-build: error: no member named hang_slab"], **run)[0]), [])
        self.assertEqual(self.takes(self.sweep("8d9cb6f3" + "0" * 32, 14, tests, **run)[0]), [])   # not the same failure
        inbox, mail = self.sweep("e" * 40, 15, [t.replace("0 carryable", "1 carryable") for t in tests], **run)
        self.assertEqual((len(self.takes(inbox)), len(self.takes(mail))), (1, 1))   # the stock check failed twice

    def test_a_failure_trunk_shares_and_a_dry_run_make_no_take(self):
        tests = [f"{SLICES}: FAIL: slab trap: she stopped 1.27 m from the rim"]
        self.gh.state["trunk_proof"] = {"sha": "t" * 40, "run": 99, "state": f"red: {SLICES}", "at": ti.ago(1)}
        self.tests[99] = tuple(tests)
        self.sweep("a" * 40, 1, tests)
        self.assertEqual(self.takes(self.sweep("b" * 40, 2, tests)[0]), [])     # trunk is red on it too: not J-100's
        dry = Take()
        dry.setUp()
        dry.gh.dry_run = True
        dry.sweep("a" * 40, 1, tests)
        dry.sweep("b" * 40, 2, tests)
        self.assertNotIn("takes", dry.gh.state)


class Held(unittest.TestCase):
    """Inbox post 6880167407: QUIET #466 claude/integrate-protected-j015 (J-015) for 3.1 h, while work/plan.md holds
    J-015 ("paused by Cory 11-13")."""

    def sw(self, hold):
        root = pathlib.Path(tempfile.mkdtemp())
        (root / "work").mkdir()
        (root / "work/plan.md").write_text("limit claude 4\nlimit muse 10\nJ-015 claude #122" + (
            " hold: paused by Cory 11-13" if hold else "") + " | Stock\nJ-100 muse #418" + (
            " hold: parked" if hold else "") + " | Hung load\n", encoding="utf-8")
        sw = ti.make(ti.FakeGH())
        sw.root = root
        return sw

    def kinds(self, s, hold, muse=False):
        sw = self.sw(hold)
        return [it.kind for it in (sw.muse_items if muse else sw.inbox_items)([s], frozenset())]

    def test_a_held_jobs_idle_pr_gets_no_quiet_and_the_same_pr_unheld_still_does(self):
        quiet = ti.steered(number=466, ref="claude/integrate-protected-j015", job="J-015", proof="none",
                           updated_at=ti.ago(3.1), proof_done_at="", run_id=0)
        self.assertEqual(self.kinds(quiet, hold=True), [])
        self.assertEqual(self.kinds(quiet, hold=False), ["QUIET"])

    def test_a_held_jobs_new_red_or_fix_still_shows_but_never_the_stuck_nag(self):
        red = ti.steered(number=478, ref="fb-bot/integrate-j100", job="J-100", proof="red: native-tests",
                         committed=ti.ago(6), updated_at=ti.ago(5))
        self.assertEqual(self.kinds(red, hold=False, muse=True), ["RED", "RED-STUCK"])
        self.assertEqual(self.kinds(red, hold=True, muse=True), ["RED"])
        fix = ti.steered(number=478, ref="fb-bot/integrate-j100", job="J-100", verdicts=[ti.fix(hours=7)],
                         committed=ti.ago(8), updated_at=ti.ago(7))
        self.assertEqual(self.kinds(fix, hold=False, muse=True), ["FIX", "FIX-STUCK"])
        self.assertEqual(self.kinds(fix, hold=True, muse=True), ["FIX"])

if __name__ == "__main__":
    unittest.main()
