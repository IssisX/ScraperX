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
    """11-13: every dispatched proof ran all 13 jobs (run 43468415707, the switchboard's merge candidate for #510)."""

    def test_a_candidate_or_integrate_dispatch_runs_only_what_it_reaches(self):
        for ref in (CANDIDATE, BRANCH, "refs/heads/fb-bot/integrate-protected-j116"):
            self.assertEqual(decide("workflow_dispatch", ref, ["tests/x.cpp", "work/plan.md"])[0], {"native"}, ref)
            self.assertEqual(decide("workflow_dispatch", ref, ["work/plan.md"])[0], NONE, ref)

    def test_a_dispatch_on_trunk_or_another_ref_or_with_doubt_runs_everything(self):
        for ref in (TRUNK, "refs/heads/switchboard/ledger", "refs/heads/claude/wip-j102", "refs/tags/v1",
                    "refs/heads/claude/integrate-x;rm", "refs/heads/switchboard/candidate"):
            self.assertEqual(decide("workflow_dispatch", ref, ["work/plan.md"])[0], EVERY, ref)
        for changed in (None, []):
            self.assertEqual(decide("workflow_dispatch", CANDIDATE, changed)[0], EVERY)
        self.assertEqual(decide("schedule", BRANCH, ["work/plan.md"])[0], EVERY)


def git(cwd, *args) -> str:
    return subprocess.run(["git", *args], cwd=cwd, check=True, capture_output=True, text=True,
                          env=dict(os.environ, GIT_AUTHOR_NAME="t", GIT_AUTHOR_EMAIL="t@t", GIT_COMMITTER_NAME="t",
                                   GIT_COMMITTER_EMAIL="t@t")).stdout.strip()


class CandidateRange(unittest.TestCase):
    """A real shallow checkout of a merge candidate, as actions/checkout leaves it (depth 1)."""

    PR_FILE = "tests/pr.cpp"

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        root = self.tmp.name
        self.origin, self.work = f"{root}/origin", f"{root}/work"
        os.mkdir(self.origin)
        git(self.origin, "init", "-q", "-b", "ScraperX-Claude")
        git(self.origin, "config", "uploadpack.allowAnySHA1InWant", "true")
        self.commit("work/plan.md", "base")
        git(self.origin, "checkout", "-q", "-b", "claude/integrate-x")
        self.commit(self.PR_FILE, "the PR's own change")
        git(self.origin, "checkout", "-q", "ScraperX-Claude")
        self.commit("godot/main.gd", "trunk moved the game")
        git(self.origin, "checkout", "-q", "-b", "switchboard/candidate-510", "claude/integrate-x")

    def tearDown(self):
        self.tmp.cleanup()

    def commit(self, path, msg):
        full = os.path.join(self.origin, path)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "a", encoding="utf-8") as fh:
            fh.write(msg + "\n")
        git(self.origin, "add", path)
        git(self.origin, "commit", "-q", "-m", msg)

    def changed(self, merge=True) -> list[str] | None:
        if merge:
            git(self.origin, "merge", "-q", "--no-ff", "-m", "candidate", "ScraperX-Claude")
        git(self.tmp.name, "clone", "-q", "--depth=1", "--branch", "switchboard/candidate-510",
            f"file://{self.origin}", self.work)
        was = os.getcwd()
        os.chdir(self.work)
        try:
            return proof_scope.changed_files(CANDIDATE, "")
        finally:
            os.chdir(was)

    def test_both_sides_since_the_prs_proof_are_compared(self):
        got = sorted(self.changed())
        self.assertEqual(got, ["godot/main.gd", "tests/pr.cpp"])
        self.assertEqual(decide("workflow_dispatch", CANDIDATE, got)[0], EVERY)   # native from the PR, godot from trunk

    def test_a_candidate_that_is_not_a_merge_runs_everything(self):
        self.assertIsNone(self.changed(merge=False))



class CandidateRangeGodot(CandidateRange):
    """The PR and trunk both moved only Godot files: the native shards are skipped on the candidate."""
    PR_FILE = "godot/pr.gd"

    def test_both_sides_since_the_prs_proof_are_compared(self):
        got = sorted(self.changed())
        self.assertEqual(got, ["godot/main.gd", "godot/pr.gd"])
        self.assertEqual(decide("workflow_dispatch", CANDIDATE, got)[0], {"godot", "android"})


class DispatchedCheckout(unittest.TestCase):
    """The whole step, main(), in a real shallow checkout as actions/checkout leaves it (depth 1): an integrate
    branch that changed one native test, on a trunk that has since moved the Godot project."""

    NATIVE_ONLY = {"game": "true", "native": "true", "godot": "false", "android": "false"}
    EVERYTHING = {"game": "true", "native": "true", "godot": "true", "android": "true"}

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.origin = f"{self.tmp.name}/origin"
        os.mkdir(self.origin)
        git(self.origin, "init", "-q", "-b", "ScraperX-Claude")
        self.commit("work/plan.md", "base")
        git(self.origin, "checkout", "-q", "-b", "claude/integrate-x")
        self.commit("tests/pr.cpp", "the branch's own change")
        git(self.origin, "checkout", "-q", "ScraperX-Claude")
        self.commit("godot/main.gd", "trunk moved the game")

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
        """What the scope step writes to $GITHUB_OUTPUT for this event, on a fresh depth-1 checkout of `branch`."""
        work = tempfile.mkdtemp(dir=self.tmp.name)
        git(self.tmp.name, "clone", "-q", "--depth=1", "--branch", branch, f"file://{self.origin}", work)
        out = os.path.join(work, "github_output")
        env = dict({"EVENT": event, "REF": ref, "BEFORE": "", "GITHUB_OUTPUT": out}, **(extra or {}))
        was = os.getcwd()
        os.chdir(work)
        try:
            with mock.patch.dict(os.environ, env), contextlib.redirect_stdout(io.StringIO()), \
                    contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(proof_scope.main(), 0)
        finally:
            os.chdir(was)
        with open(out, encoding="utf-8") as fh:
            return dict(line.strip().split("=", 1) for line in fh if line.strip())

    def test_a_dispatch_on_an_integrate_branch_is_scoped_as_its_push(self):
        pushed = self.step("push", BRANCH)
        self.assertEqual(pushed, self.NATIVE_ONLY)   # trunk's godot change since the branch point is not the branch's
        self.assertEqual(self.step("workflow_dispatch", BRANCH), pushed)

    def test_a_dispatch_on_trunk_runs_everything(self):
        self.assertEqual(self.step("workflow_dispatch", TRUNK, branch="ScraperX-Claude"), self.EVERYTHING)

    def test_nothing_a_dispatch_can_set_narrows_the_scope(self):
        asks = {"INPUT_SCOPE": "none", "SCOPE": "none", "GROUPS": "", "BEFORE": "HEAD"}
        self.assertEqual(self.step("workflow_dispatch", TRUNK, branch="ScraperX-Claude", extra=asks), self.EVERYTHING)
        self.assertEqual(self.step("workflow_dispatch", "refs/heads/claude/wip-x", extra=asks), self.EVERYTHING)
        self.assertEqual(self.step("workflow_dispatch", BRANCH, extra=asks), self.NATIVE_ONLY)

    def test_a_branch_with_no_way_back_to_trunk_runs_everything(self):
        git(self.origin, "checkout", "-q", "--orphan", "claude/integrate-lost")
        self.commit("work/plan.md", "no shared history")
        self.assertEqual(self.step("workflow_dispatch", "refs/heads/claude/integrate-lost",
                                   branch="claude/integrate-lost"), self.EVERYTHING)


if __name__ == "__main__":
    unittest.main()
