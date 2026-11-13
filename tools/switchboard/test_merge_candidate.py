"""11-13: a green, landed head behind trunk's game code is proved merged with trunk by the switchboard itself.

The skew rule (no merge on a proof of an older trunk: J-127 #482 and J-129 #483 were green alone, trunk red a day
together) would have held 13 of the last 25 merges for their owners to merge trunk in, all of them Muse's. Now
the switchboard builds its own branch (switch.CANDIDATE_PREFIX<pr>) at the head, has GitHub merge trunk into it,
dispatches the proof on it and merges the PR only when that proof is green. Only a real conflict, or a red merge,
goes back to the owner. It never writes an agent's branch.
Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
"""

from __future__ import annotations

import dataclasses
import pathlib
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import machine as M  # noqa: E402
import rules  # noqa: E402
import switch  # noqa: E402
import test_inbox as ti  # noqa: E402

REPO = ti.FakeGH.repo
HEAD, TRUNK, TRUNK2, CAND = "a" * 40, "b" * 40, "c" * 40, "d" * 40
BRANCH = f"{switch.CANDIDATE_PREFIX}256"


class CandGH:
    """The reads and writes a merge candidate makes; every write kept."""
    repo = REPO

    def __init__(self, merge_status=201, runs=(), jobs=(), trunk_files=("docs/notes.md",)):
        self.dry_run, self.state, self.writes, self.gets = False, {}, [], []
        self.merge_status, self.runs, self.jobs, self.trunk_files = merge_status, list(runs), list(jobs), trunk_files
        self.parents = [HEAD, TRUNK]

    def get(self, path, accept=""):
        self.gets.append(path)
        base = path.split("?")[0]
        if base == f"repos/{REPO}/commits/{CAND}":
            return {"sha": CAND, "parents": [{"sha": p} for p in self.parents]}
        if "/runs" in base and "head_sha=" in path:
            return {"workflow_runs": list(self.runs)}
        if base.endswith("/jobs"):
            return {"jobs": list(self.jobs)}
        if base == f"repos/{REPO}/compare/{TRUNK}...{TRUNK2}":
            return {"status": "ahead", "ahead_by": 1, "commits": [{"sha": TRUNK2}],
                    "files": [{"filename": f} for f in self.trunk_files]}
        raise switch.GitHubError("GET", path, "gh: Not Found (HTTP 404)")

    def write(self, method, path, body=None, what="", token=""):
        self.writes.append((method, path, body))
        if method == "PATCH" and "/git/refs/heads/" in path:
            raise switch.GitHubError(method, path, "gh: Reference does not exist (HTTP 422)", 422)
        if path.endswith("/merges"):
            if self.merge_status == 409:
                raise switch.GitHubError(method, path, "gh: Merge conflict (HTTP 409)", 409)
            return {"sha": CAND}
        return None


def facts(head=HEAD) -> rules.PrFacts:
    """Green, a LAND on the head, mergeable: only the skew holds it."""
    return rules.PrFacts(256, head, "fb-bot/integrate-j129", rules.TRUNK, False, True, ti.ago(5), "green",
                         (ti.land(head),),
                         frozenset(), False, False,
                         trunk_skew="trunk is 1 commit(s) ahead with 1 file(s) its proof reads, e.g. src/x.cpp")


def switch_on(gh, trunk=TRUNK):
    sw = ti.make(gh)
    sw._skew_trunk = {256: trunk}
    return sw


def run(conclusion="success", status="completed", event="workflow_dispatch", branch=BRANCH) -> dict:
    return {"id": 9, "status": status, "conclusion": conclusion, "event": event, "head_branch": branch,
            "head_sha": CAND, "created_at": "2026-11-13T15:00:00Z", "html_url": "https://x/9"}


GREEN_JOBS = [{"name": "native-tests (a)", "conclusion": "success"}]
RED_JOBS = [{"name": "native-tests (a)", "conclusion": "failure"}]



class SkewGH:
    """base_skew's two compares: trunk's commits the head lacks, and the head's own change since its merge base."""
    repo = REPO

    def __init__(self, trunk_files=("src/sim/matron.cpp",), own=("tools/switchboard/switch.py",), own_status="diverged",
                 own_down=False, renamed_from=""):
        self.trunk_files, self.own, self.own_status, self.own_down = trunk_files, own, own_status, own_down
        self.renamed_from, self.gets, self.dry_run = renamed_from, [], False

    def get(self, path, accept=""):
        self.gets.append(path)
        if path == f"repos/{REPO}/compare/{HEAD}...{rules.TRUNK}":
            return {"status": "diverged", "ahead_by": 7, "commits": [{"sha": TRUNK}] * 7,
                    "files": [{"filename": f} for f in self.trunk_files]}
        if path == f"repos/{REPO}/compare/{rules.TRUNK}...{HEAD}":
            if self.own_down:
                raise switch.GitHubError("GET", path, "gh: Server Error (HTTP 502)", 502)
            files = [{"filename": f} for f in self.own]
            if self.renamed_from:
                files[0]["previous_filename"] = self.renamed_from
            return {"status": self.own_status, "ahead_by": 3, "commits": [{"sha": HEAD}] * 3, "files": files}
        raise switch.GitHubError("GET", path, "gh: Not Found (HTTP 404)")


class NoGameHead(unittest.TestCase):
    """11-13: #510 changes only the switchboard, yet trunk's game commits held it on candidate-510's 13-job proof.
    A head that changes no file a proof reads merges with trunk's own game code, which trunk's proof covers."""

    def skew(self, **kw) -> str:
        return ti.make(SkewGH(**kw)).base_skew(facts())

    def test_a_head_that_changes_no_proof_read_file_has_no_skew(self):
        self.assertEqual(self.skew(), "")
        self.assertEqual(self.skew(own=("work/plan.md", "CLAUDE.md", "tools/board/sweep.py")), "")

    def test_a_head_that_changes_game_code_keeps_its_skew_check(self):
        for own in (("src/sim/matron.cpp",), ("tools/switchboard/switch.py", "godot/scripts/x.gd"),
                    ("ci/proof_scope.py",), (".github/workflows/wo000-delivery-spine.yml",)):
            self.assertIn("its proof reads", self.skew(own=own), own)

    def test_any_doubt_keeps_the_skew(self):
        self.assertIn("its proof reads", self.skew(own_down=True))   # the head's compare not read
        self.assertIn("its proof reads", self.skew(own=()))           # nothing listed
        self.assertIn("its proof reads", self.skew(own=("a.md",) * switch.COMPARE_FILES_CAP))   # cut short
        self.assertIn("its proof reads", self.skew(own_status="identical"))
        self.assertIn("its proof reads", self.skew(renamed_from="src/sim/matron.cpp"))   # moved out of the game

    def test_trunk_moving_only_notes_reads_no_second_compare(self):
        gh = SkewGH(trunk_files=("work/plan.md",))
        self.assertEqual(ti.make(gh).base_skew(facts()), "")
        self.assertEqual(len(gh.gets), 1)


class Build(unittest.TestCase):
    def test_it_builds_its_own_branch_merges_trunk_in_and_dispatches_the_proof(self):
        gh = CandGH()
        state, why = switch_on(gh).merge_candidate(facts())
        self.assertEqual(state, "proving", why)
        writes = [(m, p.replace(f"repos/{REPO}/", "")) for m, p, _ in gh.writes]
        self.assertEqual(writes, [("PATCH", f"git/refs/heads/{BRANCH}"), ("POST", "git/refs"), ("POST", "merges"),
                                  ("POST", f"actions/workflows/{switch.PROOF_WORKFLOW}/dispatches")])
        self.assertEqual(gh.writes[1][2], {"ref": f"refs/heads/{BRANCH}", "sha": HEAD})
        self.assertEqual(gh.writes[2][2]["base"], BRANCH)
        self.assertEqual(gh.writes[2][2]["head"], TRUNK)
        self.assertEqual(gh.writes[3][2], {"ref": BRANCH})
        for _, path, _ in gh.writes:   # never an agent's branch
            self.assertNotIn("fb-bot/", path)
            self.assertNotIn("integrate-", path)
        self.assertEqual(gh.state["candidates"]["256"]["sha"], CAND)

    def test_a_real_conflict_goes_back_to_the_owner(self):
        gh = CandGH(merge_status=409)
        sw = switch_on(gh)
        state, _ = sw.merge_candidate(facts())
        self.assertEqual(state, "conflict")
        f = dataclasses.replace(facts(), candidate=state)
        d = rules.merge_decision(f, "meta", ti.TEAM, False, False)
        self.assertEqual(d.reasons, (f"{rules.SKEW} ({f.trunk_skew})",))

    def test_a_dry_run_writes_nothing(self):
        gh = CandGH()
        gh.dry_run = True
        self.assertEqual(switch_on(gh).merge_candidate(facts())[0], "proving")
        self.assertEqual(gh.writes, [])

    def test_with_no_exact_trunk_from_the_compare_it_is_the_owners_as_before(self):
        gh = CandGH()
        self.assertEqual(switch_on(gh, trunk="").merge_candidate(facts())[0], "")
        self.assertEqual(gh.writes, [])


class Read(unittest.TestCase):
    def built(self, **kw) -> CandGH:
        gh = CandGH(**kw)
        gh.state = {"candidates": {"256": {"head": HEAD, "trunk": TRUNK, "sha": CAND, "at": "2026-11-13T15:00:00Z"}}}
        return gh

    def test_a_green_candidate_lets_the_merge_go(self):
        gh = self.built(runs=[run()], jobs=GREEN_JOBS)
        state, why = switch_on(gh).merge_candidate(facts())
        self.assertEqual(state, "green", why)
        self.assertEqual(gh.writes, [], "nothing built again")
        f = dataclasses.replace(facts(), candidate=state)
        self.assertNotIn(f"{rules.SKEW} ({f.trunk_skew})", rules.merge_decision(f, "meta", ti.TEAM, False, False).reasons)

    def test_while_it_runs_the_merge_waits_on_the_switchboard_not_the_owner(self):
        gh = self.built(runs=[run(status="in_progress", conclusion=None)], jobs=GREEN_JOBS)
        state, _ = switch_on(gh).merge_candidate(facts())
        self.assertEqual(state, "proving")
        f = dataclasses.replace(facts(), candidate=state)
        self.assertEqual(rules.merge_decision(f, "meta", ti.TEAM, False, False).reasons,
                         (f"{rules.PROVING} ({f.trunk_skew})",))

    def test_a_red_candidate_is_the_owners(self):
        gh = self.built(runs=[run(conclusion="failure")], jobs=RED_JOBS)
        self.assertEqual(switch_on(gh).merge_candidate(facts())[0], "red")

    def test_trunk_moving_only_notes_keeps_the_candidate_and_game_code_rebuilds_it(self):
        gh = self.built(runs=[run()], jobs=GREEN_JOBS)
        self.assertEqual(switch_on(gh, trunk=TRUNK2).merge_candidate(facts())[0], "green")
        gh = self.built(runs=[run()], jobs=GREEN_JOBS, trunk_files=("src/sim/matron.cpp",))
        state, _ = switch_on(gh, trunk=TRUNK2).merge_candidate(facts())
        self.assertEqual(state, "proving")
        self.assertEqual(gh.state["candidates"]["256"]["trunk"], TRUNK2)

    def test_a_record_that_is_not_this_heads_merge_with_trunk_is_not_trusted(self):
        # the state only says which commit to read: a commit with other parents, or a run that is not the
        # switchboard's dispatch on its own branch, proves nothing
        gh = self.built(runs=[run()], jobs=GREEN_JOBS)
        gh.parents = [HEAD, "e" * 40]
        self.assertEqual(switch_on(gh).merge_candidate(facts())[0], "proving")   # built afresh
        gh = self.built(runs=[run(event="push")], jobs=GREEN_JOBS)
        self.assertEqual(switch_on(gh).merge_candidate(facts())[0], "proving")
        gh = self.built(runs=[run(branch="fb-bot/integrate-j129")], jobs=GREEN_JOBS)
        self.assertEqual(switch_on(gh).merge_candidate(facts())[0], "proving")

    def test_a_new_head_builds_a_new_candidate(self):
        gh = self.built(runs=[run()], jobs=GREEN_JOBS)
        state, _ = switch_on(gh).merge_candidate(facts(head="f" * 40))
        self.assertEqual(state, "proving")
        self.assertTrue(gh.writes)


class Inbox(unittest.TestCase):
    def steered(self, candidate: str):
        f = rules.PrFacts(256, ti.HEAD, "fb-bot/integrate-j129", rules.TRUNK, False, True, ti.ago(5), "green",
                          (ti.land(),), frozenset(), False, False, trunk_skew="trunk is 1 commit(s) ahead",
                          candidate=candidate)
        d = rules.merge_decision(f, "meta", ti.TEAM)
        return switch.Steered(f, {"job": "J-129", "proof_url": ti.RUN_URL, "run_id": 7, "proof_done_at": ti.ago(1),
                                  "updated_at": ti.ago(0.1), "comments": []}, d)

    def test_the_owner_hears_only_of_a_conflict_or_a_red_and_nobody_is_woken_meanwhile(self):
        for cand, told in (("proving", ""), ("red", "fix the red"), ("conflict", "resolve the conflict"), ("", "")):
            s = self.steered(cand)
            items = ti.make(ti.FakeGH()).muse_items([s], frozenset())
            if cand == "proving":
                self.assertEqual(items, [], "the switchboard is proving it: the owner does nothing")
                w = M.work_of(s.facts, s.decision, ti.TEAM)
                self.assertEqual((w.gate, w.land), ("proving", True))
                self.assertEqual(M.TABLE[("*", "proving")].owes, M.SWITCH)
            else:
                self.assertEqual([it.kind for it in items], ["CONFLICT"], cand)
                self.assertIn(f"merge ScraperX-Claude into fb-bot/integrate-j129", items[0].line)
                self.assertIn(told, items[0].line)
            self.assertEqual(ti.make(ti.FakeGH()).inbox_items([s], frozenset()), [], cand)


class Tidy(unittest.TestCase):
    def test_a_closed_prs_candidate_branch_is_deleted_on_the_full_pass(self):
        gh = CandGH()
        gh.state = {"candidates": {"256": {"head": HEAD}, "300": {"head": HEAD}}}
        switch_on(gh).drop_candidates({"300"})
        self.assertEqual([(m, p) for m, p, _ in gh.writes], [("DELETE", f"repos/{REPO}/git/refs/heads/{BRANCH}")])
        self.assertEqual(set(gh.state["candidates"]), {"300"})


if __name__ == "__main__":
    unittest.main()
