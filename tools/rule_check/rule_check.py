#!/usr/bin/env python3
"""Rule check over a diff: flags what the owner's standing rules forbid, before review.

Usage (from the repository root):
  python3 tools/rule_check/rule_check.py                 # diff of origin/ScraperX-Claude...HEAD
  python3 tools/rule_check/rule_check.py <base> [<head>] # diff of base...head
  git diff <a> <b> | python3 tools/rule_check/rule_check.py -   # any unified diff on stdin

Prints one line per finding:  LEVEL file:line rule: text
  BLOCK   a protected change: a protected file, a test check removed or changed (its text, bound or
          operator), world geometry changed (a constant or a box of an existing body), her budget changed.
          Those changes go only as a PR into ScraperX-Claude, and only the owner merges it.
  REVIEW  a reviewer must look (possible per-route special case, velocity write, position write,
          structured binding near a lambda, new world geometry, a number in another build_* function).
Exit status: 0 nothing found, 1 REVIEW only, 2 any BLOCK. It reads the diff only; it changes nothing.
It is a first pass: a clean result does not replace 03_EXECUTION/REVIEW_CHECKLIST.md.
"""
import re
import subprocess
import sys

PROTECTED = {"MANIFEST.txt", "SHA256SUMS.txt", "01_PRODUCT_AUTHORITY/00_GOVERNING_LAWS.md"}
PROTECTED_RE = [re.compile(r"(^|/)export_presets\.cfg$"), re.compile(r"\.(keystore|jks|pem|key)$"),
                re.compile(r"^\.github/actions/stable-apk/")]
# World geometry: every solid the builders make is generated into this file (CI fails when it drifts),
# so any change to the world's collision shows here. A protected change.
GEOMETRY_FILE = "src/sim/world_solids.inc"
# The native world's builders, a protected change too (J-115). Every line of these files builds the world: the
# bands (band_*.cpp, the Stack from grade among them) and the climbs and shafts built on their own; in their .hpp
# files only the lines with a number (their constants) are geometry.
GEOMETRY_SRC_RE = re.compile(r"^src/sim/(band_\w+|twin_tower|climb_c6|climb_c7|pipe_gallery)\.(cpp|hpp)$")
# In these files only these functions build the world: the stack's decks and frame, and the generated dressing.
GEOMETRY_FUNCS = {"src/sim/physics_world.cpp": re.compile(r"\bPhysicsWorld::(build_stack|build_world_solids)\s*\(")}
# Any other build_* function in src/sim may build world bodies: a number changed in one is REVIEW. Not these: the
# climber's map, a fabricated shape, her stance shapes.
BUILD_FN_RE = re.compile(r"\bbuild_\w*\s*\(")
BUILD_FN_NOT = re.compile(r"\bAffordanceMap::build_|\bbuild_shape\s*\(|\bbuild_stance_shapes\s*\(")
# A statement that changes what is already built: an assignment to an existing name (not a new add_* kept in it),
# or a part taken out.
MUTATE_RE = re.compile(r"^\s*(?:(?P<name>[A-Za-z_]\w*)[\w.\[\]>-]*\s*[-+*/]?=(?!=)|.*\.(?:erase|pop_back|clear)\s*\()")
NEW_PART_RE = re.compile(r"=\s*(\w+(\.|->))?add_\w+\s*\(")
LITERAL_RE = re.compile(r"(?<![\w.])-?\d+(?:\.\d+)?")
# CLAUDE.md's "## Settled" section holds the owner's calls: only he changes a line, and the automatic review is told
# they are his (tools/switchboard claude_calls.settled_calls). A PR that edits the section is never merged by the
# switch: the finding is a protected-file BLOCK, which nothing lifts.
SETTLED_FILE = "CLAUDE.md"
SETTLED_HEADING = "## Settled"

# (a) test checks
CHECK_RE = re.compile(r"\b(check|expect|require|assert|fail|FAIL|EXIT_FAILURE|ok\s*&=|ok\s*=\s*false|"
                      r"passed\s*&=|return\s+false)\b|std::abort|(?<![\w-])throw\b")
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


class Hunk(list):
    """A hunk's lines as (kind, line number, text); ctx is the text after its @@ header (git's function context)."""
    ctx = ""


def add(level, f, n, rule, text):
    findings.append((level, f, n, rule, text.strip()[:140]))


def skeleton(s):
    return NUM_RE.sub("N", s.strip())


def nums(s):
    return [float(x.rstrip("Ff")) for x in NUM_RE.findall(s)]


def code(t, gd=False):
    """The line with its comment gone (a comment mark inside a string, after an odd count of quotes, is kept).
    gd: a GDScript or Python line, whose comments start with #."""
    s = t if gd else re.sub(r"/\*.*?\*/", "", t)
    if not gd and re.match(r"\s*(/\*|\*(\s|/|$))", s):
        return ""
    for m in re.finditer(r"#" if gd else r"//", s):
        if s.count('"', 0, m.start()) % 2 == 0:
            return s[:m.start()].rstrip()
    return s.rstrip()


def norm(t, gd=False):
    """The line's code with every space gone too: a change that leaves this alone is a comment or a reformat."""
    return re.sub(r"\s+", "", code(t, gd))


def check_lines(h, gd=False, side="-"):
    """The removed (side "-") or added (side "+") lines of a hunk that belong to a check: a check's own line, or a
    line of a check that runs on over several lines (its parentheses still open, or a line ended by a backslash)."""
    out, depth, running = set(), 0, False
    for kind, n, t in h:
        if kind not in (" ", side):
            continue
        c = code(t, gd)
        if running or CHECK_RE.search(c) or CMP_RE.search(c):
            if kind == side:
                out.add((n, t))
            depth = (depth if running else 0) + c.count("(") - c.count(")")
            running = depth > 0 or (gd and c.endswith("\\"))
    return out


BOUND_WORDS = {"tol", "tolerance", "max", "min", "limit", "bound", "margin", "eps", "epsilon", "slack", "within",
               "least", "most"}


def bound_constant(c):
    """True for a test's named bound: a constant whose name says it is a limit (kMaxDrift, kTolerance, LOOK_MIN_Y)."""
    m = re.match(r"\s*(?:static\s+)?(?:constexpr|const)\s+(?:[\w:<>,]+[\s&*]+)?(\w+)\s*(?::\s*\w+\s*)?(?::?=|\{)", c)
    if not m or not LITERAL_RE.search(c[m.end():]):
        return False
    words = {w.lower() for w in re.findall(r"[A-Z]?[a-z]+|[A-Z]+(?![a-z])", m.group(1))}
    return bool(words & BOUND_WORDS)


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
    files, cur, hunk, newno, old_path, deleted = {}, None, None, 0, None, False
    for line in diff.splitlines():
        if line.startswith("diff --git"):
            cur, old_path, deleted = None, None, False
        elif line.startswith("+++ "):
            path = line[4:]
            # A deleted file is checked under its old path: every check in it is removed.
            deleted = path == "/dev/null"
            cur = old_path if deleted else path[2:] if path.startswith("b/") else path
            if cur:
                files.setdefault(cur, [])
        elif line.startswith("--- "):
            old = line[4:]
            if old != "/dev/null":
                old_path = old[2:] if old.startswith("a/") else old
                files.setdefault("__old__", []).append(old_path)
        elif line.startswith("@@"):
            m = re.search(r"-(\d+)" if deleted else r"\+(\d+)", line)
            newno = int(m.group(1)) if m else 0
            hunk = Hunk()
            hunk.ctx = line.split("@@", 2)[2] if line.count("@@") >= 2 else ""
            if cur:
                files[cur].append(hunk)
        elif hunk is not None and cur:
            if line.startswith("+"):
                hunk.append(("+", newno, line[1:])); newno += 1
            elif line.startswith("-"):
                hunk.append(("-", newno, line[1:]))
                newno += 1 if deleted else 0
            else:
                hunk.append((" ", newno, line[1:])); newno += 1
    return files


def settled_edit(hunks):
    """(line, why) of the first change to CLAUDE.md's Settled section, else None. The section runs from its
    heading to the next "## " heading, and a line's membership is followed in the old file and in the new one: a
    line added to it, taken from it, or moved in or out of it by a heading added or removed is a change. A hunk
    that does not show which section a changed line is in (it starts past line 1 with no heading above the
    change) is reported too: main() reads this file with its whole text as context, so that only happens for a
    hand-fed diff."""
    why = "the owner's settled calls change: only he changes a line, and no PR does"
    for h in hunks:
        old = new = None if not h or h[0][1] > 1 else False   # None: not known yet; line 1 is above every section
        for kind, n, text in h:
            if text.startswith("## "):
                settled = text.startswith(SETTLED_HEADING)
                if settled and kind != " ":
                    return n, why
                if kind != "+":
                    old = settled
                if kind != "-":
                    new = settled
            elif kind == "-" and old is not False:
                return n, why if old else "a change whose section this diff does not show: run with <base> <head>"
            elif kind == "+" and new is not False:
                return n, why if new else "a change whose section this diff does not show: run with <base> <head>"
            elif kind == " " and old != new and None not in (old, new):
                return n, why   # a heading added or removed above moved this line in or out of the section
    return None


def world_geometry(f, hunks, whole, fn_re):
    """(d) J-115: world geometry. In a world-building file (whole) or function (fn_re), a changed or removed line
    (a constant or a box of what is built) is a BLOCK, and so is an added statement that changes what is already
    built (an assignment to a name the addition does not declare, a part erased). Other added lines, a new body or
    constant, are REVIEW: the reviewer checks they come after the existing bodies and move none. A comment, spacing,
    or a line moved within its hunk is no change; a line moved to another hunk is REVIEW (the body order changes).
    In any other build_* function of src/sim a number changed is REVIEW."""
    header = f.endswith(".hpp")

    def counts(t):
        c = code(t).strip()
        if not c or c.startswith("#") or re.fullmatch(r"[{}();,\s]*", c):
            return False
        return bool(LITERAL_RE.search(c)) if header else True

    added_all = {norm(t) for h in hunks for k, _, t in h if k == "+" and counts(t)}
    for h in hunks:
        fn, lines = h.ctx, []
        for kind, n, t in h:
            if re.match(r"[A-Za-z_]", t):
                fn = t   # a definition at column 0: the lines after it are in it
            lines.append((kind, n, t, fn))
        world = [(k, n, t) for k, n, t, fl in lines if counts(t) and (whole or (fn_re and fn_re.search(fl)))]
        if not world:
            build = [(k, n, t) for k, n, t, fl in lines if k != " " and LITERAL_RE.search(code(t))
                     and BUILD_FN_RE.search(fl) and not BUILD_FN_NOT.search(fl)]
            mine = {norm(t) for k, _, t in build if k == "-"}
            theirs = {norm(t) for k, _, t in build if k == "+"}
            moved = [(n, t) for k, n, t in build if norm(t) not in (theirs if k == "-" else mine)]
            if moved:
                add("REVIEW", f, moved[0][0], "possible-world-geometry",
                    "a number changes in a build_* function: if it builds the world, a protected change | " + moved[0][1])
            continue
        minus = [(n, t) for k, n, t in world if k == "-"]
        plus = [(n, t) for k, n, t in world if k == "+"]
        plus_norm, minus_norm = {norm(t) for _, t in plus}, {norm(t) for _, t in minus}
        plus_skel = {}
        for n, t in plus:
            plus_skel.setdefault(skeleton(code(t)), []).append((n, t))
        blocked = False
        for n, t in minus:
            if norm(t) in plus_norm:
                continue
            if norm(t) in added_all:
                add("REVIEW", f, n, "world-geometry-moved", "built in another place, so the body order changes | " + t)
                continue
            pair = plus_skel.get(skeleton(code(t)))
            pair = pair.pop(0) if pair else None   # paired in order: the first changed line with the first added
            add("BLOCK", f, pair[0] if pair else n, "world-geometry-changed",
                (code(t).strip() + " -> " + code(pair[1]).strip() if pair else "removed: " + code(t).strip())
                + " (an existing body or constant: protected)")
            blocked = True
        if blocked:
            continue
        new = [(n, t) for n, t in plus if norm(t) not in minus_norm]
        declared = " ".join(code(t) for _, t in new)
        for n, t in new:
            m = MUTATE_RE.match(code(t))
            name = m and m.group("name")
            if m and NEW_PART_RE.search(code(t)):
                continue   # a new body, guide or rope kept in its band's struct: added, not changed
            if m and not (name and re.search(r"[\w>&*]\s+[&*]?" + re.escape(name) + r"\s*(=|\{|;|\[|:)", declared)):
                add("BLOCK", f, n, "world-geometry-changed",
                    "an added statement changes what is built (protected) | " + code(t).strip())
                blocked = True
        if new and not blocked:
            add("REVIEW", f, new[0][0], "world-geometry-added",
                "new geometry: built after every existing body, moving none? | " + code(new[0][1]).strip())


def check(files):
    old_paths = set(files.pop("__old__", []))
    hit = settled_edit(files.get(SETTLED_FILE, []))
    if hit:
        add("BLOCK", SETTLED_FILE, hit[0], "protected-file", hit[1])
    for f in sorted(set(files) | old_paths):
        if f in PROTECTED or any(r.search(f) for r in PROTECTED_RE):
            add("BLOCK", f, 0, "protected-file", "this file is never edited")
        if f == GEOMETRY_FILE:
            add("BLOCK", f, 0, "world-geometry", "the world's solids change: a protected change, alone in its PR")
    for f, hunks in files.items():
        is_test = f.startswith("tests/") or "/tests/" in f or f.startswith("godot/presentation/ui/ui_test")
        is_src = f.startswith("src/")
        is_cpp = f.endswith((".cpp", ".hpp", ".h", ".inc"))
        if GEOMETRY_SRC_RE.match(f) or f in GEOMETRY_FUNCS or (f.startswith("src/sim/") and f.endswith(".cpp")):
            world_geometry(f, hunks, bool(GEOMETRY_SRC_RE.match(f)), GEOMETRY_FUNCS.get(f))
        for h in hunks:
            minus = [(n, t) for k, n, t in h if k == "-"]
            plus = [(n, t) for k, n, t in h if k == "+"]
            plus_text = {t.strip() for _, t in plus}
            hunk_text = "\n".join(t for _, _, t in h)
            # (a) test checks removed or changed: a check's line (or a line of a check over several lines), and a
            # named bound. Moved within the hunk, or only its comment or spacing changed: not a change.
            if is_test:
                gd = f.endswith((".gd", ".py"))   # a # comment (and // is a division)
                plus_norm = {norm(t, gd) for _, t in plus}
                plus_code_skel = {}
                for n, t in plus:
                    plus_code_skel.setdefault(skeleton(code(t, gd)), []).append((n, t))
                checks = check_lines(h, gd)
                for n, t in minus:
                    c = code(t, gd)
                    is_check = (n, t) in checks and c.strip()
                    if not (is_check or bound_constant(c)) or norm(t, gd) in plus_norm:
                        continue
                    rule = "test-check-changed" if is_check else "test-bound-changed"
                    pair = plus_code_skel.get(skeleton(c))
                    if pair:
                        pn, pt = pair.pop(0) if len(pair) > 1 else pair[0]
                        why = loosened(c, code(pt, gd)) or ["a number changed"]
                        add("BLOCK", f, pn, "test-tolerance-changed" if any(
                            "LOOSER" in w or "operator" in w for w in why) else rule, "; ".join(why) + " | " + pt)
                        continue
                    if is_check and not check_lines(h, gd, "+"):
                        rule = "test-check-removed"   # no check put in its place
                    add("BLOCK", f, n, rule, t)
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
        diff = subprocess.check_output(["git", "diff", "-U3", f"{base}...{head}", "--", ".", f":(exclude){SETTLED_FILE}"],
                                       text=True)
        # CLAUDE.md with its whole text as context, so each change's section is known (settled_edit)
        diff += subprocess.check_output(["git", "diff", "-U100000", f"{base}...{head}", "--", SETTLED_FILE], text=True)
    check(parse(diff))
    for lvl, f, n, rule, text in findings:
        print(f"{lvl:6} {f}:{n} {rule}: {text}")
    blocks = sum(1 for x in findings if x[0] == "BLOCK")
    print(f"rule_check: {blocks} BLOCK, {len(findings) - blocks} REVIEW")
    sys.exit(2 if blocks else 1 if findings else 0)


if __name__ == "__main__":
    main()
