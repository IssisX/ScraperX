"""A test group is skipped only when no changed file reaches it, and the whole proof runs on any doubt."""
import unittest

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


if __name__ == "__main__":
    unittest.main()
