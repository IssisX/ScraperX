"""The proof is skipped only when every changed file is outside the game, and run on any doubt."""
import unittest

from proof_scope import decide, reaches_game


class ProofScope(unittest.TestCase):
    def test_notes_and_switchboard_only_skip(self):
        game, _ = decide("push", "refs/heads/ScraperX-Claude",
                         ["CLAUDE.md", "work/budget.yml", "tools/switchboard/switch.py", ".github/workflows/switchboard.yml"])
        self.assertFalse(game)

    def test_one_game_file_runs_everything(self):
        game, why = decide("push", "refs/heads/claude/integrate-x", ["CLAUDE.md", "src/sim/band_stack.cpp"])
        self.assertTrue(game)
        self.assertIn("src/sim/band_stack.cpp", why)

    def test_doubt_runs_everything(self):
        self.assertTrue(decide("push", "refs/heads/ScraperX-Claude", None)[0])
        self.assertTrue(decide("push", "refs/heads/ScraperX-Claude", [])[0])
        self.assertTrue(decide("workflow_dispatch", "refs/heads/ScraperX-Claude", ["CLAUDE.md"])[0])

    def test_the_proof_itself_and_other_docs_reach_the_game(self):
        for p in (".github/workflows/wo000-delivery-spine.yml", "ci/proof_scope.py", "03_EXECUTION/GAP_LIST.md",
                  "godot/audio/story_radio/CREDITS.md", "tools/rule_check/rule_check.py", "work"):
            self.assertTrue(reaches_game(p), p)


if __name__ == "__main__":
    unittest.main()
