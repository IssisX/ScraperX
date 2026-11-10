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


if __name__ == "__main__":
    unittest.main()
