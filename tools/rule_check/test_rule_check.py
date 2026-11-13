#!/usr/bin/env python3
"""Regression tests for rule_check.py: its test-check rules, the Settled section, and world geometry (J-115).

Run: python3 tools/rule_check/test_rule_check.py
"""

import importlib.util
import pathlib
import unittest

_PATH = pathlib.Path(__file__).resolve().parent / "rule_check.py"


def _load():
    spec = importlib.util.spec_from_file_location("rule_check", _PATH)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _findings(removed_line):
    """rule_check's findings for a diff that removes one line from a test file."""
    rc = _load()  # a fresh module: findings is a module-level list
    diff = (
        "diff --git a/tests/sim_tests_x.cpp b/tests/sim_tests_x.cpp\n"
        "--- a/tests/sim_tests_x.cpp\n"
        "+++ b/tests/sim_tests_x.cpp\n"
        "@@ -1,3 +1,2 @@\n"
        " void run_x() {\n"
        f"-{removed_line}\n"
        " }\n"
    )
    rc.check(rc.parse(diff))
    return [(level, rule) for level, _f, _n, rule, _t in rc.findings]


class ThrowRule(unittest.TestCase):
    def test_removed_throw_statement_blocks(self):
        for line in ('    throw std::runtime_error("matron: she left the deck");',
                     '    if (!ok) throw std::logic_error("bad");',
                     '    ok ? void() : throw std::runtime_error("x");'):
            with self.subTest(line=line):
                self.assertIn(("BLOCK", "test-check-removed"), _findings(line))

    def test_removed_name_ending_in_throw_is_not_a_check(self):
        for line in ("        run_up_throw();",
                     '    if (std::string(only) == "up-throw") {'):
            with self.subTest(line=line):
                self.assertEqual([], _findings(line))



def _edit_findings(path, old, new, ctx=""):
    """rule_check's findings for one file going from old to new (lists of lines), as `git diff -U3` gives it.
    ctx is the hunk header's function context (git puts the definition the hunk starts in there)."""
    import difflib
    rc = _load()
    body = list(difflib.unified_diff(old, new, f"a/{path}", f"b/{path}", lineterm="", n=3))
    if ctx:
        body = [line + " " + ctx if line.startswith("@@") else line for line in body]
    rc.check(rc.parse(f"diff --git a/{path} b/{path}\n" + "\n".join(body) + "\n"))
    return [(level, rule) for level, _f, _n, rule, _t in rc.findings]


TEST_CPP = ["void run_x() {",
            "    constexpr double kMaxDriftM = 0.05;",
            "    constexpr double kSpawnX = 1.0;",
            "    Simulation s;",
            '    require(drift < 0.5, "x: she stays put");  // the rail holds her',
            '    require(near(s.y(), 1.5, 0.02), "x: on the deck");',
            "    require(until(s, [&] { return s.done(); },",
            "                  600),",
            '            "x: she arrives");',
            "}"]


class ChangedTestChecks(unittest.TestCase):
    """J-115: a test check changed, not removed (its text, bound or operator), is a BLOCK, as a removed one is."""

    def edited(self, index, text):
        lines = list(TEST_CPP)
        lines[index] = text
        return _edit_findings("tests/sim_tests_x.cpp", TEST_CPP, lines)

    def test_a_tightened_bound_blocks(self):
        self.assertEqual(self.edited(4, '    require(drift < 0.4, "x: she stays put");  // the rail holds her'),
                         [("BLOCK", "test-check-changed")])

    def test_a_loosened_bound_still_blocks_as_a_tolerance_and_a_changed_operator_blocks(self):
        self.assertEqual(self.edited(4, '    require(drift < 0.9, "x: she stays put");'),
                         [("BLOCK", "test-tolerance-changed")])
        self.assertEqual(self.edited(4, '    require(drift <= 0.5, "x: she stays put");'),
                         [("BLOCK", "test-check-changed")])

    def test_a_number_outside_a_comparison_blocks(self):
        self.assertEqual(self.edited(5, '    require(near(s.y(), 1.5, 0.05), "x: on the deck");'),
                         [("BLOCK", "test-check-changed")])

    def test_a_changed_line_inside_a_check_over_several_lines_blocks(self):
        self.assertEqual(self.edited(7, "                  900),"), [("BLOCK", "test-check-changed")])
        self.assertEqual(self.edited(8, '            "x: she arrives somewhere");'), [("BLOCK", "test-check-changed")])

    def test_a_changed_check_text_blocks(self):
        self.assertEqual(self.edited(4, '    require(drift < 0.5 || s.done(), "x: she stays put");'),
                         [("BLOCK", "test-check-changed")])

    def test_a_named_bound_changed_blocks_and_other_constants_do_not(self):
        self.assertEqual(self.edited(1, "    constexpr double kMaxDriftM = 0.08;"), [("BLOCK", "test-bound-changed")])
        self.assertEqual(self.edited(2, "    constexpr double kSpawnX = 2.0;"), [])

    def test_a_comment_or_spacing_edit_on_a_check_is_clean(self):
        self.assertEqual(self.edited(4, '    require(drift < 0.5, "x: she stays put");  // her east rail holds her'), [])
        self.assertEqual(self.edited(4, '    require(drift < 0.5,  "x: she stays put");'), [])

    def test_a_gdscript_check_changed_blocks_and_its_comment_does_not(self):
        old = ["func _look() -> bool:", "\tif up.y < -0.8:  # upside down", '\t\treturn _fail("hold")', "\treturn true"]
        tighter = list(old)
        tighter[1] = "\tif up.y < -0.9:  # upside down"
        said = list(old)
        said[1] = "\tif up.y < -0.8:  # upside down, by the ankle"
        path = "godot/presentation/ui/ui_test_look.gd"
        self.assertEqual(_edit_findings(path, old, tighter), [("BLOCK", "test-check-changed")])
        self.assertEqual(_edit_findings(path, old, said), [])

    def test_a_removed_check_is_still_removed(self):
        self.assertEqual(_edit_findings("tests/sim_tests_x.cpp", TEST_CPP, TEST_CPP[:5] + TEST_CPP[6:]),
                         [("BLOCK", "test-check-removed")])


# band_stack.cpp as it stands at J-085's base, around C1's cabinet and C2's switchgear.
BAND_STACK = ["constexpr float kCabinetX1 = 20.60F;",
              "constexpr float kCabinetZ0 = -122.50F;",
              "constexpr float kCabinetZ1 = -121.80F;",
              "constexpr float kCabinetTop = kDeck2Top + 1.70F;",
              "// its south face stands 0.4 m clear of the cabinet so the jump does not meet",
              "",
              "void build_c2(std::vector<Part> &route) {",
              "    // 1. Switchgear enclosure on Deck 6 East band (floor 66.00F, top 67.70F)",
              "    route.push_back(span({20.50F, kDeck6Top, -139.00F}, {23.50F, kDeck6Top + 1.70F, -136.50F},",
              "                         Material::Steel));",
              "    route.push_back(span({20.45F, kDeck6Top + 1.62F, -136.55F},",
              "                         {23.55F, kDeck6Top + 1.70F, -136.50F}, Material::Hazard));",
              "}"]


class WorldGeometry(unittest.TestCase):
    """J-115: world geometry is a protected change. A changed constant or box of the world's builders is a BLOCK; a
    new body after the existing ones is REVIEW; a comment is clean."""

    PATH = "src/sim/band_stack.cpp"

    def test_a_moved_face_in_band_stack_blocks(self):
        moved = list(BAND_STACK)
        moved[1] = "constexpr float kCabinetZ0 = -122.55F;   // J-085: her take-off in reach (was -122.50)"
        moved[2] = "constexpr float kCabinetZ1 = -121.88F;   // J-085: with Z0 (was -121.80)"
        moved[8] = "    route.push_back(span({20.50F, kDeck6Top, -139.10F}, {23.50F, kDeck6Top + 1.70F, -136.50F},"
        self.assertEqual(_edit_findings(self.PATH, BAND_STACK, moved), [("BLOCK", "world-geometry-changed")] * 3)

    def test_a_removed_body_or_a_changed_material_blocks(self):
        self.assertEqual(_edit_findings(self.PATH, BAND_STACK, BAND_STACK[:10] + BAND_STACK[12:]),
                         [("BLOCK", "world-geometry-changed")] * 2)
        steel = list(BAND_STACK)
        steel[9] = "                         Material::Rust));"
        self.assertEqual(_edit_findings(self.PATH, BAND_STACK, steel), [("BLOCK", "world-geometry-changed")])

    def test_a_new_body_added_at_the_end_is_review(self):
        added = BAND_STACK[:12] + ["    // 3. A step up onto the switchgear.",
                                   "    route.push_back(span({23.60F, kDeck6Top, -138.00F}, {24.20F, kDeck6Top + 0.85F, "
                                   "-137.40F}, Material::Steel));"] + BAND_STACK[12:]
        self.assertEqual(_edit_findings(self.PATH, BAND_STACK, added), [("REVIEW", "world-geometry-added")])
        kept = BAND_STACK[:12] + ["    stack.s2_cart = kit.add_body(Sim::kStackS2CartEntityId, s2_cart_parts(),"] + BAND_STACK[12:]
        self.assertEqual(_edit_findings(self.PATH, BAND_STACK, kept), [("REVIEW", "world-geometry-added")])

    def test_an_added_statement_that_moves_an_existing_body_blocks(self):
        shifted = BAND_STACK[:12] + ["    route[0].center += JPH::Vec3(0.0F, 0.0F, -0.1F);"] + BAND_STACK[12:]
        self.assertEqual(_edit_findings(self.PATH, BAND_STACK, shifted), [("BLOCK", "world-geometry-changed")])
        local = BAND_STACK[:12] + ["    float z = -138.0F;", "    z += 0.5F;"] + BAND_STACK[12:]
        self.assertEqual(_edit_findings(self.PATH, BAND_STACK, local), [("REVIEW", "world-geometry-added")])

    def test_a_mechanisms_tuning_is_review_and_a_line_that_also_moves_a_face_blocks(self):
        old = ["constexpr float kS2BeamMassKg = 2400.0F;",
               "constexpr float kS2BeamZ0 = -139.00F;",
               "void build_s2(kit::Kit &kit, Stack &stack) {",
               "    kit.set_guide_friction(stack.s2_guide, 0.40F);",
               "    kit.set_guide_friction(stack.s2_guide, 0.40F); route.push_back(span({20.50F, 66.0F, -139.00F}, "
               "{23.50F, 67.7F, -136.50F}, Material::Steel));",
               "}"]

        def edit(index, text):
            new = list(old)
            new[index] = text
            return _edit_findings(self.PATH, old, new)

        self.assertEqual(edit(0, "constexpr float kS2BeamMassKg = 900.0F;"), [("REVIEW", "mechanism-tuning-changed")])
        self.assertEqual(edit(3, "    kit.set_guide_friction(stack.s2_guide, 0.30F);"),
                         [("REVIEW", "mechanism-tuning-changed")])
        self.assertEqual(edit(1, "constexpr float kS2BeamZ0 = -139.10F;"), [("BLOCK", "world-geometry-changed")])
        both = old[4].replace("0.40F", "0.30F").replace("-139.00F", "-139.10F")
        self.assertEqual(edit(4, both), [("BLOCK", "world-geometry-changed")])

    def test_a_comment_only_edit_is_clean(self):
        said = list(BAND_STACK)
        said[4] = "// its south face stands 0.35 m clear of the cabinet so the jump does not meet"
        said[7] = "    // 1. The switchgear enclosure on Deck 6's East band (floor 66.00F, top 67.70F)"
        said[2] = "constexpr float kCabinetZ1 = -121.80F;  // the cabinet's north face"
        self.assertEqual(_edit_findings(self.PATH, BAND_STACK, said), [])

    def test_a_header_constant_blocks_and_a_declaration_without_a_number_does_not(self):
        old = ["struct TwinTower final {", "    static constexpr float kBalconyWestX = 2.0F;",
               "    kit::BodyIndex frame;", "};"]
        moved, renamed = list(old), list(old)
        moved[1] = "    static constexpr float kBalconyWestX = 2.2F;"
        renamed[2] = "    kit::BodyIndex frame_body;"
        self.assertEqual(_edit_findings("src/sim/twin_tower.hpp", old, moved), [("BLOCK", "world-geometry-changed")])
        self.assertEqual(_edit_findings("src/sim/twin_tower.hpp", old, renamed), [])

    def test_the_stacks_builder_in_physics_world_blocks_and_its_other_functions_do_not(self):
        old = ["    const float band_center = kStackHalfExtent - kStackDeckBandDepth * 0.5F;",
               "    for (int level = 1; level <= kStackLevelCount; ++level) {", "        frame(half, at);", "    }"]
        new = list(old)
        new[0] = "    const float band_center = kStackHalfExtent - kStackDeckBandDepth * 0.45F;"
        path = "src/sim/physics_world.cpp"
        self.assertEqual(_edit_findings(path, old, new, "void Simulation::PhysicsWorld::build_stack(JPH::BodyInterface &bodies) {"),
                         [("BLOCK", "world-geometry-changed")])
        self.assertEqual(_edit_findings(path, old, new, "void Simulation::PhysicsWorld::step(const Input &input) {"), [])

    def test_a_number_in_another_build_function_is_review_but_not_the_climbers_map(self):
        old = ["    const float pad = 0.40F;", "    parts.push_back(box(half, at, Material::Steel));"]
        new = ["    const float pad = 0.45F;", "    parts.push_back(box(half, at, Material::Steel));"]
        self.assertEqual(_edit_findings("src/sim/summit.cpp", old, new, "void Summit::build_ending() {"),
                         [("REVIEW", "possible-world-geometry")])
        self.assertEqual(_edit_findings("src/sim/matron_traversal.cpp", old, new,
                                        "void AffordanceMap::build_regions(const PhysicsSystem &physics) {"), [])

    def test_the_generated_solids_still_block(self):
        old = ["constexpr WorldSolidBox kWorldSolidBoxes[] = {", "    {0.0F, 1.0F},  // TowerMass", "};"]
        new = ["constexpr WorldSolidBox kWorldSolidBoxes[] = {", "    {0.0F, 1.1F},  // TowerMass", "};"]
        self.assertIn(("BLOCK", "world-geometry"), _edit_findings("src/sim/world_solids.inc", old, new))


def _const_resolver(uses):
    """A stub usage resolver: constant name -> [(path, lineno, text, func)] of its uses at the head."""
    def resolve(name):
        return uses.get(name, [])
    return resolve


def _const_findings(path, old, new, uses, ctx=""):
    """rule_check's findings for one file going from old to new, with a stub usage resolver."""
    import difflib
    rc = _load()
    body = list(difflib.unified_diff(old, new, f"a/{path}", f"b/{path}", lineterm="", n=3))
    if ctx:
        body = [line + " " + ctx if line.startswith("@@") else line for line in body]
    rc.check(rc.parse(f"diff --git a/{path} b/{path}\n" + "\n".join(body) + "\n"), _const_resolver(uses))
    return [(level, rule) for level, _f, _n, rule, _t in rc.findings]


TUNING_HPP = "src/sim/physics_world_tuning.hpp"
RAIL_USE = ("src/sim/physics_world.cpp", 1445,
            "                  JPH::RVec3(kHangRailX, kHangRailTopY - 0.5F * kHangRailSection,",
            "void Simulation::PhysicsWorld::build_stack(JPH::BodyInterface &bodies) {")
RAIL_OLD = ["// the hang rail: a hold-sized beam along the crossing.",
            "constexpr float kHangRailX = -2.15F;",
            "constexpr float kHangRailSection = 0.12F;",
            "constexpr float kHangRailTopY =",
            "    kStackLevelCount * kStackLevelHeight - 2.0F * kStackDeckHalfThickness;   // the 154 deck's soffit, 153.50",
            "constexpr float kHangRailNorthZ = -129.35F;"]
RAIL_NEW = ["// the hang rail: a hold-sized beam along the crossing.",
            "constexpr float kHangRailX = -2.15F;",
            "constexpr float kHangRailSection = 0.12F;",
            "constexpr float kHangRailTopY =",
            "    kStackLevelCount * kStackLevelHeight - 2.0F * kStackDeckHalfThickness - 0.50F;   // 0.50 m below the soffit, 153.00",
            "constexpr float kHangRailNorthZ = -129.35F;"]


class TuningHeaderGeometry(unittest.TestCase):
    """J-135: a tuning-header constant that places a world body is a protected change, found by what uses it —
    not by a hand list. J-043's rail drop (kHangRailTopY 153.50 -> 153.00) went through clean; it must BLOCK."""

    def test_a_tuning_constant_used_to_place_a_body_blocks(self):
        self.assertEqual(_const_findings(TUNING_HPP, RAIL_OLD, RAIL_NEW, {"kHangRailTopY": [RAIL_USE]}),
                         [("BLOCK", "world-geometry-changed")])

    def test_a_constant_used_through_another_constant_blocks(self):
        # kStackLevelHeight moves the rail through kHangRailTopY's definition: the use is a continuation
        # line of kHangRailTopY's multi-line definition, so its function context names that definition.
        uses = {"kStackLevelHeight": [(TUNING_HPP, 804,
                                       "    kStackLevelCount * kStackLevelHeight - 2.0F * kStackDeckHalfThickness - 0.50F;",
                                       "constexpr float kHangRailTopY =")],
                "kHangRailTopY": [RAIL_USE]}
        old = ["constexpr float kStackLevelHeight = 11.0F;"]
        new = ["constexpr float kStackLevelHeight = 11.5F;"]
        self.assertEqual(_const_findings(TUNING_HPP, old, new, uses), [("BLOCK", "world-geometry-changed")])

    def test_a_non_geometry_tuning_change_does_not_block(self):
        old = ["constexpr float kGripNewtons = 900.0F;"]
        new = ["constexpr float kGripNewtons = 800.0F;"]
        uses = {"kGripNewtons": [("src/sim/matron.cpp", 100, "    grip = kGripNewtons * hold;",
                                  "void Matron::grip(const Input &input) {")]}
        self.assertEqual(_const_findings(TUNING_HPP, old, new, uses), [])

    def test_a_comment_only_change_to_a_geometry_constant_is_clean(self):
        said = list(RAIL_NEW)
        said[4] = ("    kStackLevelCount * kStackLevelHeight - 2.0F * kStackDeckHalfThickness - 0.50F;"
                   "   // 0.50 m below the soffit")
        self.assertEqual(_const_findings(TUNING_HPP, RAIL_NEW, said, {"kHangRailTopY": [RAIL_USE]}), [])

    def test_a_constant_used_only_in_another_build_function_is_review(self):
        old = ["constexpr float kEndingPad = 0.40F;"]
        new = ["constexpr float kEndingPad = 0.45F;"]
        uses = {"kEndingPad": [("src/sim/summit.cpp", 10, "    const float pad = kEndingPad;",
                                "void Summit::build_ending() {")]}
        self.assertEqual(_const_findings(TUNING_HPP, old, new, uses), [("REVIEW", "possible-world-geometry")])

    def test_a_renamed_constant_is_not_a_value_change(self):
        old = ["constexpr float kOldRailY = 153.50F;"]
        new = ["constexpr float kNewRailY = 153.50F;"]
        uses = {"kNewRailY": [RAIL_USE]}
        self.assertEqual(_const_findings(TUNING_HPP, old, new, uses), [])


CLAUDE_MD = ["# Rules", "", "Intro.", "", "## Settled: never reopen (Cory's calls)", "", "- Trunk is `ScraperX-Claude`.",
             "- No per-job money cap.", "", "## Test only what a change reaches", "", "- Rerun only that test."]


def _claude_md_findings(new_lines, context=100000):
    """rule_check's findings for CLAUDE.md going from CLAUDE_MD to new_lines, as `git diff -U<context>` gives it."""
    import difflib
    rc = _load()
    body = list(difflib.unified_diff(CLAUDE_MD, new_lines, "a/CLAUDE.md", "b/CLAUDE.md", lineterm="", n=context))
    rc.check(rc.parse("diff --git a/CLAUDE.md b/CLAUDE.md\n" + "\n".join(body) + "\n"))
    return [(level, f, rule) for level, f, _n, rule, _t in rc.findings]


class SettledCalls(unittest.TestCase):
    """J-072: the automatic review is told CLAUDE.md's Settled section is the owner's word, so no PR may edit it."""

    BLOCK = [("BLOCK", "CLAUDE.md", "protected-file")]

    def edited(self, index, text=None):
        lines = list(CLAUDE_MD)
        if text is None:
            del lines[index]
        else:
            lines[index] = text
        return lines

    def test_a_changed_added_or_removed_settled_line_blocks_as_a_never_edited_file(self):
        self.assertEqual(_claude_md_findings(self.edited(7, "- A $50 per-job money cap.")), self.BLOCK)
        self.assertEqual(_claude_md_findings(self.edited(7)), self.BLOCK)
        self.assertEqual(_claude_md_findings(CLAUDE_MD[:8] + ["- Agents may reopen any call."] + CLAUDE_MD[8:]), self.BLOCK)

    def test_renaming_or_removing_the_heading_blocks(self):
        self.assertEqual(_claude_md_findings(self.edited(4, "## Old calls")), self.BLOCK)
        self.assertEqual(_claude_md_findings(self.edited(4)), self.BLOCK)

    def test_a_line_added_at_the_sections_end_blocks_and_a_new_section_after_it_does_not(self):
        self.assertEqual(_claude_md_findings(CLAUDE_MD[:9] + ["- One more call."] + CLAUDE_MD[9:]), self.BLOCK)
        between = CLAUDE_MD[:9] + ["## A new section", "", "- A rule.", ""] + CLAUDE_MD[9:]
        self.assertEqual(_claude_md_findings(between), [])   # its own heading: the settled lines are untouched
        self.assertEqual(_claude_md_findings(CLAUDE_MD + ["", "## A new section", "", "- A rule."]), [])

    def test_a_heading_that_moves_lines_in_or_out_of_the_section_blocks(self):
        cut = CLAUDE_MD[:7] + ["## A new section"] + CLAUDE_MD[7:]   # the money-cap call would leave the section
        self.assertEqual(_claude_md_findings(cut), self.BLOCK)
        self.assertEqual(_claude_md_findings(self.edited(9)), self.BLOCK)   # the next heading gone: its rules join it

    def test_the_rest_of_the_file_is_free_to_change(self):
        self.assertEqual(_claude_md_findings(self.edited(2, "A new intro.")), [])
        self.assertEqual(_claude_md_findings(self.edited(11, "- Rerun only the failing part.")), [])
        self.assertEqual(_claude_md_findings(self.edited(9, "## Test what a change reaches")), [])

    def test_a_short_diff_that_hides_the_section_is_not_passed(self):
        self.assertEqual(_claude_md_findings(self.edited(11, "- Rerun only the failing part."), context=0), self.BLOCK)

    def test_the_real_file_has_the_section_the_guard_and_the_review_look_for(self):
        rc = _load()
        text = (pathlib.Path(__file__).resolve().parents[2] / rc.SETTLED_FILE).read_text(encoding="utf-8")
        self.assertEqual(sum(1 for line in text.splitlines() if line.startswith(rc.SETTLED_HEADING)), 1)


class SystemTestRedBranch(unittest.TestCase):
    """SYSTEM TEST (J-072, 11-13), never to merge: this branch must go red so the switchboard's red-branch notice
    is seen live. The branch is deleted after the check."""

    def test_this_branch_is_red_on_purpose(self):
        self.fail("system test: claude/integrate-systest-red is red on purpose (J-072)")


if __name__ == "__main__":
    unittest.main()
