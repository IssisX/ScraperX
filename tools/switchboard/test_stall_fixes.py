"""Green PRs that stalled (11-12): #453 held by the eye's FIX on a defect another job owns, #343's clean Codex
receipt never read, FIX PRs (#389, #396) that sat a day with nobody told, and one 502 on a jobs read ending the
inbox part. Each test fails on trunk before the fix."""

from __future__ import annotations

import pathlib
import sys
import unittest
import unittest.mock

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import rules  # noqa: E402
import switch  # noqa: E402
import test_inbox as ti  # noqa: E402

ROOT = HERE.parent.parent
TEAM = rules.team_from_config(switch.load_yaml(ROOT / "work/agents.yml"))
LOOKS = switch.load_yaml(ROOT / "work/looks/sets.yml")["sets"]
HEAD_453 = "4e84c905d76f0fb71646bf1455567f3bb9f0a9be"
HEAD_343 = "b1177151bd189a5395de85ba140431a5a61bfb6c"
CODEX = rules.CODEX_LOGIN

# Codex's receipt on #453 (comment 6823237386), its first lines as GitHub holds them.
RECEIPT = ("<!-- codex-review-receipt:v1 -->\n\nCodex Review: Didn't find any major issues. Keep it up!\n\n"
           "**Reviewed commit:** `{sha}`\n\n<details> <summary>About Codex in GitHub</summary>\n</details>")

# The eye's comment on #453 @ 4e84c905 (comment 6823205172), its reasons shortened.
EYE_453 = f"""**Claude eye (look of #453 @ 4e84c905): FIX**

- approach: LAND; rubric: ""; frame ``. All seven lines hold.
- summit: LAND; rubric: ""; frame ``. Every frame holds.
- fight_contact: FIX (known defect of J-060: shown, not blocking this PR); rubric: "6. `face_front`: dark rings"; frame `screenshots/injury/injury_face_front.png`. Only her left eye has a ring.
- ladder_descent: FIX; rubric: "1. Her hands are closed round rungs"; frame `screenshots/ladder_grade_ladder_down.png`. Both hands are open.

command: download the `godot-audits-*` artifact of https://github.com/IssisX/ScraperX/actions/runs/43210773716
input: the frame named above
expected: the rubric line holds
observed: it does not (reason above)

_claude-opus-5-5 through the API. Taste stays the owner's._
<!-- switchboard:eye pr=453 head={HEAD_453} -->"""
OPEN_JOBS = ("J-060", "J-063", "J-088")


def state(sha: str, unresolved: bool) -> str:
    return ('<!-- codex-pull-request-review-summary -->\n<!-- codex-review-state:v1 {"code_review":{"commit_sha":"'
            f'{sha}","has_any_findings":{str(unresolved).lower()},"has_unresolved_findings":{str(unresolved).lower()},'
            '"status":"completed"}} -->')


def facts(head: str, verdicts, needs=frozenset(), ref="fb-bot/integrate-j063-torso-drive", workflows=False):
    return rules.PrFacts(453, head, ref, rules.TRUNK, False, True, "2026-11-12T06:00:00Z", "green", tuple(verdicts),
                         needs, False, workflows)


def claude_land(head: str) -> rules.Verdict:
    """The integrator's LAND on #453 (comment 6823663818), posted as IssisX."""
    v = rules.parse_verdict(f"**Claude (review of #453 @ {head}): LAND**\n\nruling", "2026-11-12T12:30:50Z", "IssisX", TEAM)
    assert v is not None
    return v


def eye(body: str = EYE_453, login: str = rules.BOT_LOGIN) -> rules.Verdict:
    v = rules.parse_verdict(body, "2026-11-12T12:09:14Z", login, TEAM)
    assert v is not None and v.kind == "eye"
    return v


class CodexReceipt(unittest.TestCase):
    """#343 and #453: a clean pass that comes only as a receipt is Codex's LAND on the head it names."""

    def receipt(self, sha: str = HEAD_343[:10], login: str = CODEX, body: str = RECEIPT) -> rules.Verdict | None:
        return rules.parse_codex_receipt(body.format(sha=sha), "2026-11-12T13:22:44Z", login)

    def test_a_clean_receipt_on_the_head_is_a_land_that_merges(self):
        v = self.receipt()
        self.assertEqual((v.vendor, v.who, v.kind, v.verdict, v.sha, v.receipt), ("openai", "Codex", "review", "LAND",
                                                                                  HEAD_343[:10], True))
        old = rules.parse_codex_state(state("959bf0138f88ae98051ee84cb3d4cc38da95b2d1", True), "2026-11-11T06:22:37Z", CODEX)
        d = rules.merge_decision(facts(HEAD_343, [old, v], ref="fb-bot/integrate-j059-injury-feed"), "meta", TEAM)
        self.assertEqual((d.action, d.land_by), ("MERGE", "Codex"))   # the older head's FIX names another head

    def test_a_receipt_for_an_older_head_counts_for_nothing(self):
        v = self.receipt(sha="959bf0138f")
        d = rules.merge_decision(facts(HEAD_343, [v], ref="fb-bot/integrate-j059-injury-feed"), "meta", TEAM)
        self.assertEqual(d.action, "WAIT")
        self.assertIn(f"no LAND on @{HEAD_343[:8]}", d.reasons[0])

    def test_only_the_codex_app_and_only_the_clean_wording(self):
        self.assertIsNone(self.receipt(login="IssisX"))
        self.assertIsNone(self.receipt(login=rules.BOT_LOGIN))
        self.assertIsNone(self.receipt(body=RECEIPT.replace("Didn't find any major issues", "Here are some automated "
                                                                                             "review suggestions")))
        self.assertIsNone(self.receipt(body=RECEIPT.replace("<!-- codex-review-receipt:v1 -->", "")))
        self.assertIsNone(self.receipt(body=RECEIPT + "\n**Reviewed commit:** `0123456789`"))   # two commits: neither
        self.assertIsNotNone(self.receipt(body=RECEIPT.replace("Didn't", "Didn’t")))

    def test_a_receipt_never_clears_the_state_comments_fix_on_the_same_head(self):
        # #343 @ b1177151, 11-12: the receipt says clean, the state 3 s later says an open P1 thread (restore regional
        # injury from the checkpoint) is unresolved on this head. The FIX holds, whichever came last.
        fix = rules.parse_codex_state(state(HEAD_343, True), "2026-11-12T13:22:47Z", CODEX)
        for when in ("2026-11-12T13:22:44Z", "2026-11-12T13:30:00Z"):
            v = rules.parse_codex_receipt(RECEIPT.format(sha=HEAD_343[:10]), when, CODEX)
            d = rules.merge_decision(facts(HEAD_343, [fix, v], ref="fb-bot/integrate-j059-injury-feed"), "meta", TEAM)
            self.assertEqual(d.action, "FIX", when)

    def test_a_receipt_is_never_a_workflow_prs_codex_review(self):
        v = rules.parse_codex_receipt(RECEIPT.format(sha=HEAD_453), "2026-11-12T12:10:44Z", CODEX)   # even a whole sha
        self.assertTrue(rules.names_whole_head(v, HEAD_453))
        self.assertIsNone(rules.codex_review_on([v], HEAD_453))

    def test_the_switch_reads_the_receipt_from_the_prs_comments(self):
        gh = ti.FakeGH()
        gh.by_issue[343] = [{"id": 1, "body": RECEIPT.format(sha=HEAD_343[:10]), "user": {"login": CODEX},
                             "created_at": "2026-11-12T13:22:44Z", "updated_at": "2026-11-12T13:22:44Z"}]
        gh.open_prs = []
        sw = ti.make(gh)
        sw.team, sw.review_key = TEAM, ""
        gh_route = gh.route
        gh.route = lambda path: [] if path.split("?")[0].endswith("/pulls/343/reviews") else gh_route(path)
        verdicts, _ = sw.verdicts(343)
        self.assertEqual([(v.who, v.verdict, v.sha) for v in verdicts], [("Codex", "LAND", HEAD_343[:10])])


class EyeRelook(unittest.TestCase):
    """#453: the eye's FIX was on a ladder defect the integrator ruled J-088's; the PR is J-063's torso drive."""

    def test_the_453_eye_fix_is_a_land_under_todays_sets(self):
        self.assertEqual(rules.known_defect_jobs(LOOKS["ladder_descent"]), ("J-088",))
        v = rules.eye_relook(eye(), EYE_453, LOOKS, "J-063", OPEN_JOBS)
        self.assertEqual(v.verdict, "LAND")
        d = rules.merge_decision(facts(HEAD_453, [claude_land(HEAD_453), v], needs=frozenset({"eyes"})), "meta", TEAM)
        self.assertEqual((d.action, d.land_by), ("MERGE", "Claude"))

    def test_before_the_fix_the_eye_fix_held_453(self):
        v = eye()
        d = rules.merge_decision(facts(HEAD_453, [claude_land(HEAD_453), v], needs=frozenset({"eyes"})), "meta", TEAM)
        self.assertEqual(d.action, "FIX")
        self.assertEqual(d.reasons, ("open FIX from Claude (eye)",))

    def test_the_owner_jobs_own_pr_is_still_held(self):
        self.assertEqual(rules.eye_relook(eye(), EYE_453, LOOKS, "J-088", OPEN_JOBS).verdict, "FIX")
        old = dict(LOOKS, ladder_descent=dict(LOOKS["ladder_descent"], known_defect="J-063"))
        self.assertEqual(rules.eye_relook(eye(), EYE_453, old, "J-063", OPEN_JOBS).verdict, "FIX")

    def test_once_the_owner_job_closes_the_fix_holds(self):
        self.assertEqual(rules.eye_relook(eye(), EYE_453, LOOKS, "J-063", ("J-060", "J-063")).verdict, "FIX")

    def test_an_added_line_never_lifts_a_fix(self):
        body = EYE_453.replace("Both hands are open.", "Both hands are open.\n- ladder_descent: LAND; rubric: \"\"")
        old = dict(LOOKS, ladder_descent=dict(LOOKS["ladder_descent"], known_defect="J-063"))
        self.assertEqual(rules.eye_relook(eye(body), body, old, "J-063", OPEN_JOBS).verdict, "FIX")

    def test_a_set_not_judged_or_missing_is_never_lifted(self):
        missing = EYE_453.replace("- summit: LAND", "- sets not judged: summit\n- other: LAND")
        self.assertEqual(rules.eye_relook(eye(missing), missing, LOOKS, "J-063", OPEN_JOBS).verdict, "FIX")
        dropped = "\n".join(l for l in EYE_453.split("\n") if not l.startswith("- summit:"))
        self.assertEqual(rules.eye_relook(eye(dropped), dropped, LOOKS, "J-063", OPEN_JOBS).verdict, "FIX")

    def test_only_the_bots_eye(self):
        typed = eye(login="IssisX")
        self.assertEqual(rules.eye_relook(typed, EYE_453, LOOKS, "J-063", OPEN_JOBS), typed)

    def test_the_switch_relooks_when_it_reads_the_prs_verdicts(self):
        gh = ti.FakeGH()
        gh.by_issue[453] = [{"id": 1, "body": EYE_453, "user": {"login": rules.BOT_LOGIN},
                             "created_at": "2026-11-12T12:09:14Z", "updated_at": "2026-11-12T12:09:14Z"}]
        gh.open_issues = []
        sw = ti.make(gh)
        sw.team, sw.review_key, sw.looks = TEAM, "", {"sets": LOOKS}
        sw._jobs = {j: {"number": 1, "title": f"{j} x", "labels": []} for j in OPEN_JOBS}
        gh_route = gh.route
        gh.route = lambda path: [] if path.split("?")[0].endswith("/pulls/453/reviews") else gh_route(path)
        (v,), _ = sw.verdicts(453, "J-063")
        self.assertEqual((v.kind, v.verdict), ("eye", "LAND"))
        self.assertTrue(any(line.startswith("RELOOK      #453") for line in sw.lines))
        (v,), _ = sw.verdicts(453)   # the review load's read: as posted
        self.assertEqual(v.verdict, "FIX")


class FixStuck(unittest.TestCase):
    """#389 and #396: a Codex FIX with no new head for a day; the integrator was never told by the inbox."""

    def test_a_fix_with_no_new_head_past_the_bound_is_told_to_both_naming_the_owner(self):
        s = ti.steered(ref="fb-bot/integrate-j078", verdicts=[ti.fix(hours=7)], committed=ti.ago(7))
        (it,) = ti.items_of(ti.FakeGH(), s)
        self.assertEqual((it.kind, it.target, it.dest), ("FIX-STUCK", "#256", "both"))
        self.assertIn("no new head from its owner Muse for 7.0 h", it.line)
        self.assertEqual([i.kind for i in ti.muse_of(ti.FakeGH(), s)], ["FIX", "FIX-STUCK"])

    def test_not_before_the_bound(self):
        s = ti.steered(ref="fb-bot/integrate-j078", verdicts=[ti.fix(hours=5)], committed=ti.ago(5.9))
        self.assertEqual(ti.items_of(ti.FakeGH(), s), [])

    def test_a_codex_fix_tells_muse_where_its_findings_are(self):
        s = ti.steered(ref="fb-bot/integrate-j078", verdicts=[ti.fix()], committed=ti.ago(1))
        (it,) = ti.muse_of(ti.FakeGH(), s)
        self.assertIn("unresolved review threads on the diff", it.finding)
        self.assertIn("https://github.com/IssisX/ScraperX/pull/256/files", it.finding)


class Answers(dict):
    """GitHub's answers by path; a callable answer is asked anew at each read."""

    def __getitem__(self, key):
        got = super().__getitem__(key)
        return got() if callable(got) else got


class Mergeability(unittest.TestCase):
    """#453 in a dry run (11-12): GitHub was still computing mergeability 3 s after the first read (trunk moves every
    few minutes and resets it), so the one read again left a ready PR on WAIT."""

    def steer(self, nulls: int) -> list:
        import test_cache_trust as tc
        real = Answers(tc.github([tc.codex_clean()], "success"))
        pr = dict(real[f"repos/{tc.REPO}/pulls/9"])
        reads = iter([None] * nulls)
        real[f"repos/{tc.REPO}/pulls/9"] = lambda: dict(pr, mergeable=next(reads, True))
        with unittest.mock.patch.object(switch.time, "sleep"):
            steered, writes, _ = tc.sweep_one(real, {})
        return [steered[-1].decision.action, any(path.endswith("/pulls/9/merge") for _, path in writes)]

    def test_a_pr_still_computing_after_one_read_again_is_merged(self):
        self.assertEqual(self.steer(nulls=3), ["MERGE", True])   # the listing, the PR read, the first read again

    def test_still_computing_after_every_read_waits(self):
        self.assertEqual(self.steer(nulls=2 + switch.MERGEABLE_READS), ["WAIT", False])


class JobsRead502(unittest.TestCase):
    """11-12: GitHub answered 502 on actions/runs/*/jobs; one failed read must not end the inbox part."""

    def test_one_branch_whose_jobs_read_fails_waits_and_the_others_are_listed(self):
        bad, good = "fb-bot/integrate-j044c-wreck", "fb-bot/integrate-j045-zz"
        gh = ti.FakeGH(runs=[ti.run(40, bad, ti.HEAD, hours=2), ti.run(41, good, ti.OTHER_HEAD, hours=2)],
                       jobs={41: ti.RED_JOBS}, heads={bad: ti.HEAD, good: ti.OTHER_HEAD})
        route = gh.route

        def flaky(path):
            if "/actions/runs/40/jobs" in path:
                raise switch.GitHubError("GET", path, "gh: HTTP 502: Bad Gateway", 502)
            return route(path)
        gh.route = flaky
        sw = ti.make(gh)
        got = sw.muse_items([], frozenset())
        self.assertEqual([(i.kind, i.target) for i in got], [("RED-BRANCH", good)])
        self.assertIn(bad, sw.red_pending)   # read again next sweep
        self.assertTrue(any("not read" in line for line in sw.lines))

    def test_a_rate_limit_still_raises(self):
        name = "fb-bot/integrate-j044c-wreck"
        gh = ti.FakeGH(runs=[ti.run(40, name, ti.HEAD, hours=2)], heads={name: ti.HEAD})
        route = gh.route
        gh.route = lambda path: (_ for _ in ()).throw(switch.RateLimited("GET", path, "rate", 403)) \
            if "/jobs" in path else route(path)
        with self.assertRaises(switch.RateLimited):
            ti.make(gh).muse_items([], frozenset())


if __name__ == "__main__":
    unittest.main()
