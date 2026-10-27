#!/usr/bin/env python3
"""Rule check over a diff: flags what the owner's standing rules forbid, before review.

Usage (from the repository root):
  python3 tools/rule_check/rule_check.py                 # diff of origin/ScraperX-Claude...HEAD
  python3 tools/rule_check/rule_check.py <base> [<head>] # diff of base...head
  git diff <a> <b> | python3 tools/rule_check/rule_check.py -   # any unified diff on stdin

Prints one line per finding:  LEVEL file:line rule: text
  BLOCK   needs the owner: a protected file, a test check removed or loosened, her budget changed.
          Those changes go only as a PR into ScraperX-Claude, and only the owner merges it.
  REVIEW  a reviewer must look (possible per-route special case, velocity write, position write,
          structured binding near a lambda).
Exit status: 0 nothing found, 1 REVIEW only, 2 any BLOCK. It reads the diff only; it changes nothing.
It is a first pass: a clean result does not replace 03_EXECUTION/REVIEW_CHECKLIST.md.
"""
import re
import subprocess
import sys

PROTECTED = {"MANIFEST.txt", "SHA256SUMS.txt", "01_PRODUCT_AUTHORITY/00_GOVERNING_LAWS.md"}
PROTECTED_RE = [re.compile(r"(^|/)export_presets\.cfg$"), re.compile(r"\.(keystore|jks|pem|key)$"),
                re.compile(r"^\.github/actions/stable-apk/")]

# (a) test checks
CHECK_RE = re.compile(r"\b(check|expect|require|assert|fail|FAIL|EXIT_FAILURE|ok\s*&=|ok\s*=\s*false|"
                      r"passed\s*&=|return\s+false)\b|std::abort|throw\b")
CMP_RE = re.compile(r"(<=|>=|<|>|==|!=)\s*(-?\d+(?:\.\d+)?)F?\b")
NUM_RE = re.compile(r"-?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?F?\b")
# (b) per-route / per-deck / per-transition special cases
ROUTE_RE = [
    re.compile(r"\b(deck|route|transition|edge|region|way|band|island|ring)(_?(id|index|name|no|num))?\s*(==|!=)\s*(?!nullptr|NULL\b|0\b|-1\b|false|true)[\w\"']"),
    re.compile(r"(==|!=)\s*\"[^\"]*(deck|route|ladder|pipe|ring|hatch|ways?|tp-?\d)[^\"]*\"", re.I),
    re.compile(r"\bif\s*\(.*\b(pos|position|p|at|here|feet|origin)\.?[xyz]\b\s*[<>]=?\s*-?\d{2,}\.\d"),
    re.compile(r"\bif\s*\(.*\b[xyz]\s*[<>]=?\s*-?\d{2,}\.\d+F?\s*&&.*\b[xyz]\s*[<>]=?\s*-?\d{2,}"),
    re.compile(r"\b(TP-?\d{3}|tp_?\d{3})\b"),
]
# (c) her budget
BUDGET_NAMES = (r"total_force_n|foot_force_n|grip_force_n|power_w|mass_kg|kMassKg|kTurnLungeForceN|"
                r"kTurnLungePowerW|kFootAccel|kTurnFootAccel|kLungeForceN|kLungePowerW")
BUDGET_RE = re.compile(r"\b(" + BUDGET_NAMES + r")\s*(=|\{)\s*[-\d(]"      # a limit given a value
                       r"|\bLimits\s*\{\s*[^}\s]"                              # a Limits built with overrides
                       r"|\b(8000\.5|8000(\.0)?F|9800(\.0)?F?|65000(\.0)?F?|65\s*kW)\b")  # her numbers, raw
LIMITS_FILE = "src/sim/matron_traversal.hpp"
# Extra reviewer flags
VEL_RE = re.compile(r"(SetLinearVelocity|set_linear_velocity|AddLinearVelocity|SetVelocity|set_velocity|"
                    r"(?<![\w.])(v[xyz]|vel|velocity)(\.[xyz]|\.Set[XYZ]\()?\s*([-+*/]?=)(?!=))")
VEL_OK = re.compile(r"\bGet\w*Velocity|\bbudget\.velocity\b|^\s*(const\s+)?(auto|float|Vec3|JPH::Vec3)\b")
POS_RE = re.compile(r"(SetPosition|set_position|SetPositionAndRotation|\.pos(ition)?\s*=[^=]|teleport)", re.I)
SB_RE = re.compile(r"\bauto\s*&{0,2}\s*\[")
LAMBDA_RE = re.compile(r"\[[&=]?[\w&=, ]*\]\s*(\(|\{|mutable)")

findings = []


def add(level, f, n, rule, text):
    findings.append((level, f, n, rule, text.strip()[:140]))


def skeleton(s):
    return NUM_RE.sub("N", s.strip())


def nums(s):
    return [float(x.rstrip("Ff")) for x in NUM_RE.findall(s)]


def loosened(old, new):
    """For a paired - / + line with the same shape, say how a comparison's limit moved."""
    o, n = CMP_RE.findall(old), CMP_RE.findall(new)
    if not o or len(o) != len(n):
        return None
    out = []
    for (op, a), (op2, b) in zip(o, n):
        a, b = float(a), float(b)
        if op != op2:
            out.append(f"operator {op} -> {op2}")
        elif a != b:
            looser = (op in ("<", "<=") and b > a) or (op in (">", ">=") and b < a)
            out.append(f"{op} {a:g} -> {b:g}" + (" (LOOSER)" if looser else " (tighter)" if op in "<><=>=" else ""))
    return out or None


def parse(diff):
    files, cur, hunk, newno = {}, None, None, 0
    for line in diff.splitlines():
        if line.startswith("diff --git"):
            cur = None
        elif line.startswith("+++ "):
            path = line[4:]
            cur = None if path == "/dev/null" else path[2:] if path.startswith("b/") else path
            if cur:
                files.setdefault(cur, [])
        elif line.startswith("--- "):
            old = line[4:]
            if old != "/dev/null":
                files.setdefault("__old__", []).append(old[2:] if old.startswith("a/") else old)
        elif line.startswith("@@"):
            m = re.search(r"\+(\d+)", line)
            newno = int(m.group(1)) if m else 0
            hunk = []
            if cur:
                files[cur].append(hunk)
        elif hunk is not None and cur:
            if line.startswith("+"):
                hunk.append(("+", newno, line[1:])); newno += 1
            elif line.startswith("-"):
                hunk.append(("-", newno, line[1:]))
            else:
                hunk.append((" ", newno, line[1:])); newno += 1
    return files


def check(files):
    old_paths = set(files.pop("__old__", []))
    for f in sorted(set(files) | old_paths):
        if f in PROTECTED or any(r.search(f) for r in PROTECTED_RE):
            add("BLOCK", f, 0, "protected-file", "this file is never edited")
    for f, hunks in files.items():
        is_test = f.startswith("tests/") or "/tests/" in f or f.startswith("godot/presentation/ui/ui_test")
        is_src = f.startswith("src/")
        is_cpp = f.endswith((".cpp", ".hpp", ".h", ".inc"))
        for h in hunks:
            minus = [(n, t) for k, n, t in h if k == "-"]
            plus = [(n, t) for k, n, t in h if k == "+"]
            plus_text = {t.strip() for _, t in plus}
            plus_skel = {}
            for n, t in plus:
                plus_skel.setdefault(skeleton(t), []).append((n, t))
            hunk_text = "\n".join(t for _, _, t in h)
            # (a) test checks removed or loosened
            if is_test:
                for n, t in minus:
                    if not (CHECK_RE.search(t) or CMP_RE.search(t)) or t.strip().startswith("//"):
                        continue
                    if t.strip() in plus_text:
                        continue  # moved, not removed
                    pair = plus_skel.get(skeleton(t))
                    if pair:
                        why = loosened(t, pair[0][1])
                        if why:
                            lvl = "BLOCK" if any("LOOSER" in w or "operator" in w for w in why) else "REVIEW"
                            add(lvl, f, pair[0][0], "test-tolerance-changed", "; ".join(why) + " | " + pair[0][1])
                        continue
                    add("BLOCK", f, n, "test-check-removed", t)
                for n, t in plus:
                    if re.search(r"getenv\(\"SCRAPERX_(SKIP|LOOSE|TOLERANCE)", t) or re.search(r"\b(skip|disabled?)_?test\b", t, re.I):
                        add("BLOCK", f, n, "test-skip-added", t)
            # (c) her budget numbers
            if is_src or is_test:
                for n, t in minus + plus:
                    in_limits = f == LIMITS_FILE and re.match(r"\s*float\s+\w+\s*=\s*-?\d", t)
                    if (BUDGET_RE.search(t) or in_limits) and not t.strip().startswith("//"):
                        sign = "+" if (n, t) in plus else "-"
                        if sign == "-" and t.strip() in plus_text:
                            continue
                        add("BLOCK", f, n, "budget-number-changed", sign + " " + t)
            # (b) and the rest: only added source lines
            if is_src and is_cpp:
                for n, t in plus:
                    s = t.strip()
                    if s.startswith("//") or s.startswith("*"):
                        continue
                    if any(r.search(t) for r in ROUTE_RE):
                        add("REVIEW", f, n, "possible-per-route-case", t)
                    if "/matron" in f and VEL_RE.search(t) and not VEL_OK.search(t):
                        add("REVIEW", f, n, "velocity-write", t)
                    if "/matron" in f and POS_RE.search(t):
                        add("REVIEW", f, n, "position-write", t)
                    if SB_RE.search(t) and LAMBDA_RE.search(hunk_text):
                        add("REVIEW", f, n, "structured-binding-near-lambda",
                            t + "  (clang -Werror on Android: never capture a structured binding in a lambda)")


def main():
    a = sys.argv[1:]
    if a == ["-"]:
        diff = sys.stdin.read()
    else:
        base = a[0] if a else "origin/ScraperX-Claude"
        head = a[1] if len(a) > 1 else "HEAD"
        diff = subprocess.check_output(["git", "diff", "-U3", f"{base}...{head}"], text=True)
    check(parse(diff))
    for lvl, f, n, rule, text in findings:
        print(f"{lvl:6} {f}:{n} {rule}: {text}")
    blocks = sum(1 for x in findings if x[0] == "BLOCK")
    print(f"rule_check: {blocks} BLOCK, {len(findings) - blocks} REVIEW")
    sys.exit(2 if blocks else 1 if findings else 0)


if __name__ == "__main__":
    main()
