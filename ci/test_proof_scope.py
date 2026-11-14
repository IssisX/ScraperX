"""A test group is skipped only when no changed file reaches it, and the whole proof runs on any doubt."""
import contextlib
import io
import os
import subprocess
import tempfile
import unittest
from unittest import mock

import proof_scope
from proof_scope import EVERY, NONE, decide, groups_of, outputs, reaches_game

TRUNK = "refs/heads/ScraperX-Claude"
BRANCH = "refs/heads/claude/integrate-x"
PERSON = "a-person"                # a login: who a hand dispatch or a re-run of a run is by
BOT = "github-actions[bot]"        # the workflow token: who the switchboard's dispatch is by


def run(*changed, ref=BRANCH):
    return decide("push", ref, list(changed))[0]


class ProofScope(unittest.TestCase):
    def test_notes_and_switchboard_only_skip(self):
        self.assertEqual(run("CLAUDE.md", "work/budget.yml", "tools/switchboard/switch.py",
                             ".github/workflows/switchboard.yml", ref=TRUNK), NONE)

    def test_one_game_file_runs_everything(self):
        groups, why = decide("push", BRANCH, ["CLAUDE.md", "src/sim/band_stack.cpp"])
        self.assertEqual(groups, EVERY)
        self.assertIn("src/sim/band_stack.cpp", why)

    def test_doubt_runs_everything(self):
        self.assertEqual(decide("push", TRUNK, None)[0], EVERY)
        self.assertEqual(decide("push", TRUNK, [])[0], EVERY)
        self.assertEqual(decide("workflow_dispatch", TRUNK, ["CLAUDE.md"])[0], EVERY)

    def test_the_proof_itself_runs_everything(self):
        for p in (".github/workflows/wo000-delivery-spine.yml", ".github/scripts/check_ctest_shard_coverage.py",
                  ".github/scripts/check_apk_identity.sh", "ci/proof_scope.py", "ci/verify_story_radio_package.py"):
            self.assertEqual(groups_of(p), EVERY, p)

    def test_native_graph_runs_everything(self):
        # src/sim and the delivery check compile into the native tests and the GDExtension alike.
        for p in ("src/sim/matron.cpp", "src/sim/world_solids.inc", "src/sim/vertical/stone_wheel.cpp",
                  "src/bridge/fabricated_delivery.cpp", "src/bridge/new_file.cpp", "CMakeLists.txt",
                  "third_party/jolt_patch.cmake", "tools/fab/dev_public_key.hex"):
            self.assertEqual(groups_of(p), EVERY, p)

    def test_unknown_paths_run_everything(self):
        for p in ("work", "src/new_dir/x.cpp", "newdir/thing.py", "build.sh", "src/sim/notes.txt"):
            self.assertEqual(groups_of(p), EVERY, p)

    def test_godot_only_skips_the_native_shards(self):
        groups = run("godot/presentation/main.gd", "godot/main.tscn", "godot/presentation/ui/ui_test_driver.gd",
                     "godot/tests/her_takes_test.gd", "godot/project.godot", "godot/export_presets.cfg")
        self.assertEqual(groups, {"godot", "android"})

    def test_audio_and_art_skip_the_native_shards_but_not_godot(self):
        # The touch scenarios count cues and load every asset; the APK packages them.
        self.assertEqual(run("godot/audio/story_radio/a.ogg", "godot/audio/story_radio/a.ogg.import",
                             "godot/audio/story_radio/CREDITS.md", "godot/presentation/textures/rust.png"),
                         {"godot", "android"})

    def test_fabricated_objects_reach_the_native_tests(self):
        # sim_tests_fabricated.cpp reads godot/tests/fabricated/ at run time.
        self.assertEqual(groups_of("godot/tests/fabricated/keg.sig"), EVERY)

    def test_godot_only_bridge_files_skip_the_native_shards(self):
        self.assertEqual(run("src/bridge/scraperx_simulation.cpp", "src/bridge/register_types.hpp"),
                         {"godot", "android"})

    def test_matron_probe_runs_the_godot_touch_gate(self):
        # The godot-touch touch-2 "Her drawn body" step records with
        # tools/matron_probe/probe.gd and gates on tools/matron_probe/check.py,
        # and host-build's drawn-body guard reads check.py's EXPECTED keys.
        # A probe-only push must therefore run godot (which also runs
        # host-build via game=true), not skip the whole proof.
        for p in ("tools/matron_probe/check.py", "tools/matron_probe/probe.gd",
                  "tools/matron_probe/score.py", "tools/matron_probe/shape_fit.py"):
            self.assertEqual(groups_of(p), {"godot"}, p)
        self.assertEqual(run("tools/matron_probe/check.py"), {"godot"})

    def test_native_tests_only_skip_godot_and_android(self):
        self.assertEqual(run("tests/sim_tests_matron_chase.cpp", "tests/data/matron_drawn_limbs.inc"), {"native"})

    def test_native_tests_and_godot_run_both(self):
        self.assertEqual(run("tests/sim_tests_bay.cpp", "godot/presentation/main.gd"), EVERY)

    def test_docs_and_offline_tools_skip_everything(self):
        for p in ("03_EXECUTION/GAP_LIST.md", "03_EXECUTION/ENVIRONMENT/ref.jpg",
                  "03_EXECUTION/MATRON_REDESIGN/multi_deck_inventory.json", "01_PRODUCT_AUTHORITY/01_SCRAPERX_GDD.md",
                  "02_ENGINEERING_AUTHORITY/EVIDENCE_SNAPSHOT.md", "README.md", "SECURITY.md", "MANIFEST.txt",
                  "third_party/README.md", "tests/README.md", "tools/rule_check/rule_check.py",
                  "tools/body/build_matron_body.py", "tools/test_reach/cg.py", "tools/shared-toolchain/install.sh",
                  "tools/fab/README.md", "src/voice/generate_fall_voice.py"):
            self.assertFalse(reaches_game(p), p)
        self.assertEqual(run("03_EXECUTION/GAP_LIST.md", "tools/audio/mix.py", "src/voice/x.py"), NONE)

    def test_a_branch_is_proven_on_everything_it_changes(self):
        # On an integrate branch the list is merge-base..HEAD, so a docs push still carries the branch's game code.
        self.assertEqual(run("03_EXECUTION/GAP_LIST.md", "godot/presentation/main.gd"), {"godot", "android"})

    def test_outputs(self):
        self.assertEqual(outputs(EVERY), {"game": "true", "native": "true", "godot": "true", "android": "true"})
        self.assertEqual(outputs(NONE), {"game": "false", "native": "false", "godot": "false", "android": "false"})
        self.assertEqual(outputs(frozenset({"native"})),
                         {"game": "true", "native": "true", "godot": "false", "android": "false"})

    def test_why_names_what_reaches_each_group(self):
        _, why = decide("push", BRANCH, ["godot/main.tscn"])
        self.assertIn("native skipped", why)
        self.assertIn("godot runs: 1 of 1 changed files reach it (godot/main.tscn)", why)


CANDIDATE = "refs/heads/switchboard/candidate-510"


class Dispatch(unittest.TestCase):
    """11-13 and 11-14: a dispatched proof ran every group whatever its head reached (runs 43468415707, 43598215300)."""

    def test_an_integrate_dispatch_runs_only_what_it_reaches(self):
        for ref in (BRANCH, "refs/heads/fb-bot/integrate-protected-j116"):
            self.assertEqual(decide("workflow_dispatch", ref, ["tests/x.cpp", "work/plan.md"])[0], {"native"}, ref)
            self.assertEqual(decide("workflow_dispatch", ref, ["work/plan.md"])[0], NONE, ref)

    def test_a_dispatch_on_trunk_or_another_ref_or_with_doubt_runs_everything(self):
        # A merge candidate too: the switch makes none now (51fa2300), so no reader checks such a ref's parents.
        for ref in (TRUNK, "refs/heads/switchboard/ledger", "refs/heads/claude/wip-j102", "refs/tags/v1",
                    "refs/heads/claude/integrate-x;rm", "refs/heads/switchboard/candidate", CANDIDATE,
                    "refs/heads/claude/integrate-x/y", "refs/heads/integrate-x", "refs/pull/5/merge"):
            self.assertEqual(decide("workflow_dispatch", ref, ["work/plan.md"])[0], EVERY, ref)
        for changed in (None, []):
            self.assertEqual(decide("workflow_dispatch", BRANCH, changed)[0], EVERY)
        self.assertEqual(decide("schedule", BRANCH, ["work/plan.md"])[0], EVERY)


def git(cwd, *args) -> str:
    return subprocess.run(["git", *args], cwd=cwd, check=True, capture_output=True, text=True,
                          env=dict(os.environ, GIT_AUTHOR_NAME="t", GIT_AUTHOR_EMAIL="t@t", GIT_COMMITTER_NAME="t",
                                   GIT_COMMITTER_EMAIL="t@t")).stdout.strip()


class DispatchedCheckout(unittest.TestCase):
    """The whole step, main(), with real git in a real clone: an integrate branch that changed one native test, on
    top of a trunk whose last commit moved the Godot project. The clone is whole and the branch sits on trunk's tip,
    so the merge-base is the trunk commit the step fetches, whatever a shallow fetch deepens (git 2.43 and CI's 2.56
    differ there; CI's own depth-1 checkout is proven by every integrate run's rule check, which needs the same
    merge-base)."""

    NATIVE_ONLY = {"game": "true", "native": "true", "godot": "false", "android": "false"}
    EVERYTHING = {"game": "true", "native": "true", "godot": "true", "android": "true"}

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.origin = f"{self.tmp.name}/origin"
        os.mkdir(self.origin)
        git(self.origin, "init", "-q", "-b", "ScraperX-Claude")
        self.commit("work/plan.md", "base")
        self.commit("godot/main.gd", "trunk moved the game")
        git(self.origin, "checkout", "-q", "-b", "claude/integrate-x")
        self.commit("tests/pr.cpp", "the branch's own change")

    def tearDown(self):
        self.tmp.cleanup()

    def commit(self, path, msg):
        full = os.path.join(self.origin, path)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "a", encoding="utf-8") as fh:
            fh.write(msg + "\n")
        git(self.origin, "add", path)
        git(self.origin, "commit", "-q", "-m", msg)

    def step(self, event, ref, branch="claude/integrate-x", extra=None) -> dict[str, str]:
        """What the scope step writes to $GITHUB_OUTPUT for this event, in a fresh clone of `branch`. What it printed
        is kept in self.said, for a failure's message."""
        work = tempfile.mkdtemp(dir=self.tmp.name)
        git(self.tmp.name, "clone", "-q", "--branch", branch, f"file://{self.origin}", work)
        out = os.path.join(work, "github_output")
        # The actors are pinned: the proof runs these tests with its own GITHUB_ACTOR, the bot's on a trunk dispatch.
        env = dict({"EVENT": event, "REF": ref, "BEFORE": "", "GITHUB_OUTPUT": out, "GITHUB_ACTOR": PERSON,
                    "GITHUB_TRIGGERING_ACTOR": PERSON}, **(extra or {}))
        was = os.getcwd()
        os.chdir(work)
        try:
            said = io.StringIO()
            with mock.patch.dict(os.environ, env), contextlib.redirect_stdout(said), contextlib.redirect_stderr(said):
                self.assertEqual(proof_scope.main(), 0)
        finally:
            os.chdir(was)
            self.said = said.getvalue()
        with open(out, encoding="utf-8") as fh:
            return dict(line.strip().split("=", 1) for line in fh if line.strip())

    def test_a_dispatch_on_an_integrate_branch_is_scoped_as_its_push(self):
        pushed = self.step("push", BRANCH)
        self.assertEqual(pushed, self.NATIVE_ONLY, self.said)   # trunk's own godot commit is not the branch's change
        self.assertEqual(self.step("workflow_dispatch", BRANCH), pushed, self.said)

    def test_a_dispatch_on_trunk_runs_everything(self):
        self.assertEqual(self.step("workflow_dispatch", TRUNK, branch="ScraperX-Claude"), self.EVERYTHING)

    def test_a_dispatch_on_a_merge_candidate_runs_everything(self):
        # A merge whose parents are the branch and a notes-only child of it: read by its parents' merge-base (as a
        # candidate was until the switch stopped making them), only the notes would count and the game be skipped.
        git(self.origin, "checkout", "-q", "-b", "notes", "claude/integrate-x")
        self.commit("work/notes.md", "notes only")
        git(self.origin, "checkout", "-q", "-b", "switchboard/candidate-510", "claude/integrate-x")
        git(self.origin, "merge", "-q", "--no-ff", "-m", "candidate", "notes")
        self.assertEqual(self.step("workflow_dispatch", CANDIDATE, branch="switchboard/candidate-510"),
                         self.EVERYTHING)

    def test_nothing_a_dispatch_can_set_narrows_the_scope(self):
        asks = {"INPUT_SCOPE": "none", "SCOPE": "none", "GROUPS": "", "BEFORE": "HEAD"}
        self.assertEqual(self.step("workflow_dispatch", TRUNK, branch="ScraperX-Claude", extra=asks), self.EVERYTHING)
        self.assertEqual(self.step("workflow_dispatch", "refs/heads/claude/wip-x", extra=asks), self.EVERYTHING)
        self.assertEqual(self.step("workflow_dispatch", BRANCH, extra=asks), self.NATIVE_ONLY, self.said)

    def test_a_branch_with_no_way_back_to_trunk_runs_everything(self):
        git(self.origin, "checkout", "-q", "--orphan", "claude/integrate-lost")
        self.commit("work/plan.md", "no shared history")
        self.assertEqual(self.step("workflow_dispatch", "refs/heads/claude/integrate-lost",
                                   branch="claude/integrate-lost"), self.EVERYTHING)


# The author and committer GitHub writes on a merge made with the workflow token (read on 55ae0ca1, #551's merge).
TOKEN_MERGE = {"GIT_AUTHOR_NAME": BOT, "GIT_AUTHOR_EMAIL": "41898282+github-actions[bot]@users.noreply.github.com",
               "GIT_COMMITTER_NAME": "GitHub", "GIT_COMMITTER_EMAIL": "noreply@github.com"}
HAND = {"GIT_AUTHOR_NAME": "t", "GIT_AUTHOR_EMAIL": "t@t", "GIT_COMMITTER_NAME": "t", "GIT_COMMITTER_EMAIL": "t@t"}


class TrunkMergeDispatch(unittest.TestCase):
    """J-072, 11-14: the trunk proof the switchboard dispatches after a workflow-token merge ran every job (run
    43596588412: #551's merge, six switchboard files, 15 jobs, 24 minutes). The whole step, main(), with real git in
    a whole clone of trunk: that dispatch writes what a push of the merge writes, and every other dispatch on trunk
    still writes everything. Trunk moved the Godot project after the PR branched, so only the merge's own diff
    against its first parent keeps that commit out of the range."""

    NOTHING = {"game": "false", "native": "false", "godot": "false", "android": "false"}
    NATIVE_ONLY = DispatchedCheckout.NATIVE_ONLY
    EVERYTHING = DispatchedCheckout.EVERYTHING
    tearDown = DispatchedCheckout.tearDown
    commit = DispatchedCheckout.commit
    step = DispatchedCheckout.step

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.origin = f"{self.tmp.name}/origin"
        os.mkdir(self.origin)
        git(self.origin, "init", "-q", "-b", "ScraperX-Claude")
        git(self.origin, "config", "uploadpack.allowAnySHA1InWant", "true")   # as GitHub: HEAD^1 is fetched by sha
        self.commit("work/plan.md", "base")

    def merged(self, path, by=TOKEN_MERGE, onto="ScraperX-Claude", moved=True):
        """The branch `onto` after a PR that changes `path` is merged into it, as `by` (the merge commit's author
        and committer). With `moved`, `onto` moved the Godot project after the PR branched, in a commit of its own."""
        self.prs = getattr(self, "prs", 0) + 1
        pr = f"claude/integrate-{onto}-{self.prs}"
        git(self.origin, "checkout", "-q", "-b", pr, onto)
        self.commit(path, "the PR's own change")
        git(self.origin, "checkout", "-q", onto)
        if moved:
            self.commit("godot/main.gd", "trunk moved the game")
        subprocess.run(["git", "merge", "-q", "--no-ff", "-m", f"Merge pull request #551 from {pr} (switchboard)", pr],
                       cwd=self.origin, check=True, capture_output=True, text=True, env=dict(os.environ, **by))
        self.assertEqual(len(git(self.origin, "rev-list", "--parents", "-1", "HEAD").split()), 3)

    def dispatched(self, actor=BOT, rerun_by="", branch="ScraperX-Claude", extra=None):
        """What a dispatch on trunk writes: by `actor`, re-run by `rerun_by` if given, in a whole clone of `branch`."""
        who = {"GITHUB_ACTOR": actor, "GITHUB_TRIGGERING_ACTOR": rerun_by or actor}
        return self.step("workflow_dispatch", TRUNK, branch=branch, extra=dict(who, **(extra or {})))

    def pushed(self):
        """What the push of trunk's last commit writes: the range is BEFORE..HEAD, and BEFORE is its first parent."""
        before = git(self.origin, "rev-parse", "ScraperX-Claude^1")
        return self.step("push", TRUNK, branch="ScraperX-Claude", extra={"BEFORE": before})

    def test_a_switchboard_only_merge_runs_no_game_group(self):
        self.merged("tools/switchboard/switch.py")
        self.assertEqual(self.pushed(), self.NOTHING, self.said)
        self.assertEqual(self.dispatched(), self.NOTHING, self.said)
        self.assertIn("scoped as its push", self.said)

    def test_a_merge_that_changes_the_game_runs_what_its_push_runs(self):
        self.merged("src/sim/matron.cpp")
        self.assertEqual(self.pushed(), self.EVERYTHING, self.said)
        self.assertEqual(self.dispatched(), self.EVERYTHING, self.said)
        self.assertIn("src/sim/matron.cpp", self.said)   # by its changed file, not by the dispatch rule

    def test_a_merge_that_changes_one_native_test_runs_what_its_push_runs(self):
        self.merged("tests/pr.cpp")
        self.assertEqual(self.pushed(), self.NATIVE_ONLY, self.said)
        self.assertEqual(self.dispatched(), self.NATIVE_ONLY, self.said)

    def test_a_hand_dispatch_or_a_rerun_on_trunk_runs_everything(self):
        self.merged("tools/switchboard/switch.py")
        self.assertEqual(self.dispatched(actor=PERSON), self.EVERYTHING, self.said)
        self.assertEqual(self.dispatched(rerun_by=PERSON), self.EVERYTHING, self.said)
        self.assertEqual(self.dispatched(actor=PERSON, rerun_by=BOT), self.EVERYTHING, self.said)
        self.assertEqual(self.dispatched(actor=""), self.EVERYTHING, self.said)
        self.assertEqual(self.dispatched(actor="github-actions"), self.EVERYTHING, self.said)

    def test_nothing_a_dispatch_can_set_makes_a_hand_dispatch_the_switchboards(self):
        self.merged("tools/switchboard/switch.py")
        asks = {"ACTOR": BOT, "INPUT_ACTOR": BOT, "TRIGGERING_ACTOR": BOT, "SCOPE": "none",
                "BEFORE": git(self.origin, "rev-parse", "ScraperX-Claude^1")}
        self.assertEqual(self.dispatched(actor=PERSON, extra=asks), self.EVERYTHING, self.said)

    def test_a_head_the_switchboard_did_not_merge_runs_everything(self):
        self.merged("tools/switchboard/switch.py")
        token_author = {k: v for k, v in TOKEN_MERGE.items() if "AUTHOR" in k}
        token_committer = {k: v for k, v in TOKEN_MERGE.items() if "COMMITTER" in k}
        for name, by in (("hand", HAND),                                    # a merge pushed by hand (1aa910b0)
                         ("merge-token", dict(HAND, **token_committer)),    # GitHub's merge for a login: it has a push
                         ("committed-by-hand", dict(HAND, **token_author))):
            git(self.origin, "checkout", "-q", "-b", name, "ScraperX-Claude~2")
            self.merged("tools/switchboard/rules.py", by=by, onto=name)
            self.assertEqual(self.dispatched(branch=name), self.EVERYTHING, f"{name}: {self.said}")
        # A commit with one parent, written as the token writes its merges, on top of the switchboard's merge.
        git(self.origin, "checkout", "-q", "ScraperX-Claude")
        with mock.patch.dict(os.environ, TOKEN_MERGE):
            subprocess.run(["git", "commit", "-q", "--allow-empty", "-m", "not a merge"], cwd=self.origin, check=True)
        self.assertEqual(self.dispatched(), self.EVERYTHING, self.said)

    def test_a_dispatch_of_that_merge_on_another_ref_runs_everything(self):
        self.merged("tools/switchboard/switch.py")
        for ref in ("refs/heads/main", "refs/heads/claude/wip-x", "refs/tags/ScraperX-Claude"):
            got = self.step("workflow_dispatch", ref, branch="ScraperX-Claude",
                            extra={"GITHUB_ACTOR": BOT, "GITHUB_TRIGGERING_ACTOR": BOT})
            self.assertEqual(got, self.EVERYTHING, f"{ref}: {self.said}")

    def test_a_first_parent_that_cannot_be_fetched_runs_everything(self):
        self.merged("tools/switchboard/switch.py")
        self.assertEqual(self.dispatched(), self.NOTHING, self.said)
        # The same clone and merge, but the step's fetch of HEAD^1 is refused (the clone is made before this is set).
        self.assertEqual(self.dispatched(extra={"GIT_ALLOW_PROTOCOL": "https"}), self.EVERYTHING, self.said)
        self.assertIn("fetch", self.said)

    def test_a_merge_below_head_is_in_the_range_when_no_other_commit_is_between(self):
        # The dispatch names no commit: the first merge's own dispatch can read the second merge as HEAD.
        self.merged("src/sim/matron.cpp")
        self.merged("tools/switchboard/switch.py", moved=False)
        self.assertEqual(self.pushed(), self.NOTHING, self.said)        # the second merge's push alone
        self.assertEqual(self.dispatched(), self.EVERYTHING, self.said)
        self.assertIn("src/sim/matron.cpp", self.said)   # by the first merge's file, not by the dispatch rule

    def test_a_merge_on_a_commit_that_is_not_a_token_merge_is_scoped_to_its_own_range(self):
        self.merged("src/sim/matron.cpp")
        self.commit("work/notes.md", "a pushed commit: its push has its own run")
        self.merged("tools/switchboard/switch.py", moved=False)
        self.assertEqual(self.dispatched(), self.NOTHING, self.said)
        # The same below a merge pushed by hand, and below a merge made with the merge token.
        for by in (HAND, dict(HAND, **{k: v for k, v in TOKEN_MERGE.items() if "COMMITTER" in k})):
            self.merged("src/sim/matron.cpp", by=by, moved=False)
            self.merged("tools/switchboard/switch.py", moved=False)
            self.assertEqual(self.dispatched(), self.NOTHING, self.said)

    def test_more_token_merges_in_a_row_than_the_cap_run_everything(self):
        self.assertEqual(proof_scope.TOKEN_MERGE_CHAIN, 20)
        self.commit("godot/main.gd", "trunk moved the game")
        for n in range(20):
            self.merged(f"tools/switchboard/part{n}.py", moved=False)
        self.assertEqual(self.dispatched(), self.NOTHING, self.said)      # 20 in a row: all of them are compared
        self.assertIn("all 20 changed files", self.said)
        self.merged("tools/switchboard/part20.py", moved=False)
        self.assertEqual(self.dispatched(), self.EVERYTHING, self.said)   # 21: the whole proof
        self.assertIn("more than 20", self.said)


if __name__ == "__main__":
    unittest.main()
