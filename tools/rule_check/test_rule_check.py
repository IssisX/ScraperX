#!/usr/bin/env python3
"""Regression tests for rule_check.py's test-check rule (CHECK_RE).

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


if __name__ == "__main__":
    unittest.main()
