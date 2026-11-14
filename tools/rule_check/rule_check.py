#!/usr/bin/env python3
"""Rule check over a diff: flags what the owner's standing rules forbid, before review.

Usage (from the repository root):
  python3 tools/rule_check/rule_check.py                 # diff of origin/ScraperX-Claude...HEAD
  python3 tools/rule_check/rule_check.py <base> [<head>] # diff of base...head
  git diff <a> <b> | python3 tools/rule_check/rule_check.py -   # any unified diff on stdin

Prints one line per finding:  LEVEL file:line rule: text
  BLOCK   a protected change: a protected file, a test check removed or changed (its text, bound or
          operator), world geometry changed (a constant or a box of an existing body — a tuning-header
          constant counts when it places a body, J-135), her budget changed, the save layout changed (a
          saved climb's reader or writer, a Saved* struct, a type save_format_key saves whole, J-138), the budget
          audit's list of known leaks changed (a raised peak, a new row, a loosened flag or bound, J-138).
          Those changes go only as a PR into ScraperX-Claude, and only the owner merges it.
  REVIEW  a reviewer must look (possible per-route special case, velocity write, position write,
          structured binding near a lambda, new world geometry, a mechanism's tuning number changed (a mass,
          friction, damping, speed ... in a world-building file), a number in another build_* function).
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
# A mechanism's tuning, not geometry: a number on a constant or a call named for one of these (kS2BeamMassKg,
# set_guide_friction(...), set_damping(...), kCageGovernorSpeed). Her own limits stay BLOCK (budget-number-changed).
TUNING_WORDS = {"mass", "kg", "friction", "damping", "damp", "stiffness", "stiff", "tension", "speed", "mps", "torque",
                "time", "delay", "period", "restitution", "drag", "rate", "gain", "spring", "hz", "force", "power"}
PASS_CALLS = {"max", "min", "clamp", "abs", "sqrt", "static_cast", "float", "double"}   # look past these to the owner
LITERAL_RE = re.compile(r"(?<![\w.])-?\d+(?:\.\d+)?")
# (d2) J-135: a constant that places or sizes a world body, wherever it is defined. The checker only watched
# the files that build the world, so a geometry change made through a tuning constant (J-043: kHangRailTopY in
# physics_world_tuning.hpp, used in PhysicsWorld::build_stack) went through clean. The constants are found by
# what uses them at the head revision — the solids table and the body builders — not by a hand list that goes
# stale. A constant used in another constant's definition is followed through it (kStackLevelHeight moves the
# rail through kHangRailTopY). Uses are resolved with `git grep` at the head; without a resolver (a hand-fed
# diff with no repo, or a test that passes none) this rule stays silent.
CONST_DEF_RE = re.compile(r"^\s*(?:static\s+)?(?:inline\s+)?(?:constexpr|const)\s+(?:[\w:<>,]+[\s&*]+)?"
                          r"(\w+)\s*(?:\[[^\]]*\])?\s*(?::\s*\w+\s*)?(?::?=|\{)")
CONST_NAME_RE = re.compile(r"^(?:k[A-Z]\w*|[A-Z][A-Z0-9_]{1,})$")
CPP_EXTS = (".hpp", ".cpp", ".h", ".inc")
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
    """A hunk's lines as (kind, line number, text); ctx is the text after its @@ header (git's function context);
    old_start is its first line in the old file."""
    ctx = ""
    old_start = 0


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
            mo = re.search(r"-(\d+)", line)
            hunk.old_start = int(mo.group(1)) if mo else 0
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


def tuning_name(name):
    return bool({w.lower() for w in re.findall(r"[A-Z]?[a-z]+|[A-Z]+(?![a-z])", name or "")} & TUNING_WORDS)


def number_owner_is_tuning(c, pos):
    """Is the number at c[pos] a mechanism's tuning? Its owner is the innermost call it is an argument of (past
    max/min/casts), or, outside any call, the constant or name it is given to."""
    depth, i = 0, pos - 1
    while i >= 0:
        ch = c[i]
        if ch in ")]}":
            depth += 1
        elif ch in "([{":
            if depth:
                depth -= 1
            elif ch == "(":
                m = re.search(r"(\w+)\s*$", c[:i])
                if m and m.group(1) not in PASS_CALLS:
                    return tuning_name(m.group(1))
        i -= 1
    m = None
    for m in re.finditer(r"(\w+)\s*(?:=(?!=)|\{)", c[:pos]):
        pass
    return bool(m) and tuning_name(m.group(1))


def tuning_only(old, new):
    """For a changed line paired with its new self (the same skeleton): True when every number that changed is a
    mechanism's tuning, so the change is REVIEW (mechanism-tuning-changed), not world geometry."""
    a, b = list(NUM_RE.finditer(old)), list(NUM_RE.finditer(new))
    changed = [y for x, y in zip(a, b) if x.group(0) != y.group(0)]
    return len(a) == len(b) and bool(changed) and all(number_owner_is_tuning(new, y.start()) for y in changed)


def world_geometry(f, hunks, whole, fn_re):
    """(d) J-115: world geometry. In a world-building file (whole) or function (fn_re), a changed or removed line
    (a constant or a box of what is built: a position, size, extent or the list of bodies) is a BLOCK, unless every
    number it changes is a mechanism's tuning (a mass, friction, damping, speed ...: REVIEW). So is an added statement that changes what is already
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
        blocked, tuned = False, set()
        for n, t in minus:
            if norm(t) in plus_norm:
                continue
            if norm(t) in added_all:
                add("REVIEW", f, n, "world-geometry-moved", "built in another place, so the body order changes | " + t)
                continue
            pair = plus_skel.get(skeleton(code(t)))
            pair = pair.pop(0) if pair else None   # paired in order: the first changed line with the first added
            if pair and tuning_only(code(t), code(pair[1])):
                add("REVIEW", f, pair[0], "mechanism-tuning-changed",
                    code(t).strip() + " -> " + code(pair[1]).strip() + " (a mechanism's tuning, not geometry)")
                tuned.add(pair)
                continue
            add("BLOCK", f, pair[0] if pair else n, "world-geometry-changed",
                (code(t).strip() + " -> " + code(pair[1]).strip() if pair else "removed: " + code(t).strip())
                + " (an existing body or constant: protected)")
            blocked = True
        if blocked:
            continue
        new = [(n, t) for n, t in plus if norm(t) not in minus_norm and (n, t) not in tuned]
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


def git_uses(head):
    """A usage resolver backed by `git grep` at <head>: constant name -> [(path, lineno, text, func)] of its
    uses in src/. The func is git's -p function context (the enclosing definition, or ""), so a use inside a
    body builder can be told from one in a mechanism. A name git cannot find has no uses. Results are cached:
    a diff that changes several constants, and the constants they are built from, still runs a handful of greps."""
    cache = {}

    def resolve(name):
        if name in cache:
            return cache[name]
        uses, func = [], ""
        try:
            out = subprocess.check_output(["git", "grep", "-n", "-p", "-w", name, head, "--", "src"],
                                          text=True, stderr=subprocess.DEVNULL)
        except (subprocess.CalledProcessError, FileNotFoundError):
            out = ""
        for line in out.splitlines():
            _rev, rest = line.split(":", 1)
            m = re.match(r"^([^:=]+)=(\d+)=(.*)$", rest)
            if m:
                func = m.group(3)
                continue
            m = re.match(r"^([^:=]+):(\d+):(.*)$", rest)
            if m:
                uses.append((m.group(1), int(m.group(2)), m.group(3), func))
        cache[name] = uses
        return uses

    return resolve


def use_class(path, func):
    """'block' | 'review' | None for one use of a constant: does it place or size a world body? The solids
    table and the files that build the world are BLOCK, as a change to them is; a use in another build_*
    function of src/sim is REVIEW, as a number changed there is."""
    if path == GEOMETRY_FILE or GEOMETRY_SRC_RE.match(path):
        return "block"
    fn_re = GEOMETRY_FUNCS.get(path)
    if fn_re and func and fn_re.search(func):
        return "block"
    if path.startswith("src/sim/") and func and BUILD_FN_RE.search(func) and not BUILD_FN_NOT.search(func):
        return "review"
    return None


def constant_is_geometry(name, resolve, seen=None):
    """('block', (path, lineno)) | ('review', None) | (None, None): does changing this constant move world
    geometry, by what uses it at the head? A constant used in another constant's definition is followed
    through it; a use that is only a comment or a string is not a use."""
    if seen is None:
        seen = set()
    if name in seen:
        return None, None
    seen.add(name)
    best = None
    for path, _n, text, func in resolve(name):
        c = code(text)
        if not re.search(r"\b" + re.escape(name) + r"\b", re.sub(r'"[^"]*"', '""', c)):
            continue
        m = CONST_DEF_RE.match(c)
        if m and CONST_NAME_RE.match(m.group(1)):
            if m.group(1) == name:
                continue  # its own definition
            kind, where = constant_is_geometry(m.group(1), resolve, seen)
        else:
            fm = CONST_DEF_RE.match(func or "")
            if fm and CONST_NAME_RE.match(fm.group(1)):
                if fm.group(1) == name:
                    continue  # a continuation line of its own definition
                kind, where = constant_is_geometry(fm.group(1), resolve, seen)
            else:
                kind = use_class(path, func)
                where = (path, _n) if kind == "block" else None
        if kind == "block":
            return "block", where
        if kind == "review":
            best = "review"
    return best, None


def _definitions(lines):
    """{name: [(lineno, code)]} for the constant definitions in one side's lines, in hunk order. A definition
    may run over several lines (the value on the line after its `constexpr` line); the lines after the start
    belong to it until the `;` that ends it. Each side is read on its own, so the old version's `;` and the
    new version's `;` — two lines in the diff — do not cut each other short."""
    defs = {}
    cur = None
    for n, c in lines:
        if cur is None:
            m = CONST_DEF_RE.match(c)
            if not (m and CONST_NAME_RE.match(m.group(1))):
                continue
            cur = [m.group(1), []]
        cur[1].append((n, c))
        if ";" in c:
            defs[cur[0]] = cur[1]
            cur = None
    return defs


def changed_constants(hunks):
    """[(name, new_lineno, old, new)] for constant definitions whose value changed in these hunks. A definition
    only added (no old side) or only removed (no new side) is not a value change: a new constant comes with the
    builder call that uses it, and the existing rule flags that call; a removed one takes its uses with it. A
    comment-only change is not a change. The finding sits on the definition's last line in the new file, which
    is the changed line for a value on its own line."""
    out = []
    for h in hunks:
        old_defs = _definitions([(n, code(t)) for kind, n, t in h if kind in (" ", "-")])
        new_defs = _definitions([(n, code(t)) for kind, n, t in h if kind in (" ", "+")])
        for name, new_lines in new_defs.items():
            if name not in old_defs:
                continue
            old = " ".join(p.strip() for _n, p in old_defs[name])
            new = " ".join(p.strip() for _n, p in new_lines)
            if norm(old) == norm(new):
                continue
            out.append((name, new_lines[-1][0], old, new))
    return out


def tuning_constant_geometry(f, hunks, resolve):
    """(d2) J-135: a constant that places or sizes a world body, changed wherever it is defined. The constants
    are found by what uses them at the head revision, not by a hand list. A changed constant with a
    world-building use is a BLOCK, the same as a world_solids.inc change; one used only in another build_*
    function is REVIEW. A constant world_geometry already blocked on the same line is not reported twice; one
    it called tuning is upgraded, because the use is the truth."""
    for name, n, old, new in changed_constants(hunks):
        kind, where = constant_is_geometry(name, resolve)
        if kind == "block":
            for x in list(findings):
                if x[1] == f and x[2] == n and x[3] == "mechanism-tuning-changed":
                    findings.remove(x)
            if any(x[0] == "BLOCK" and x[1] == f and x[2] == n and x[3] == "world-geometry-changed"
                   for x in findings):
                continue
            add("BLOCK", f, n, "world-geometry-changed",
                f"places a world body (used in {where[0]}:{where[1]}): {old} -> {new}")
        elif kind == "review":
            if any(x[1] == f and x[2] == n for x in findings):
                continue
            add("REVIEW", f, n, "possible-world-geometry",
                "a constant used in a build_* function: if it places a world body, a protected change | " + new)


# (e) J-138: the save layout, a protected change (CLAUDE.md, AGENTS.md: her injuries persist, and a reload restores
# her as saved). A saved climb's bytes are written and read by the functions that take a SaveWriter or SaveReader,
# by Simulation::save_game / load_game and save_format_key (whose sizeof list names the types saved whole), and laid
# out by the Saved* structs and the SaveWriter / SaveReader classes. These regions are found in each changed file's
# own text: at the base for a removed line, at the head for an added one, so a new field written, a field taken out,
# a reordered write or a whole reader removed is seen. A comment or spacing edit is no change; an order change is
# one (the bytes move). Without the two texts (a hand-fed diff) this rule is silent.
SAVE_KEY_FILE = "src/sim/simulation.cpp"
SAVE_FN_RE = re.compile(r"\b(?:SaveWriter|SaveReader)\s*&|(?<![.>\w])(?:\w+::)*(?:save_game|load_game|save_format_key)\s*\(")
SAVE_STRUCT_RE = re.compile(r"^(?:Saved\w*|SaveWriter|SaveReader)$")
SCOPE_RE = re.compile(r"(?<![<,\w])(?<![<,]\s)\b(?:struct|class|union|namespace)\s+(?:\w+::)*(\w+)")
CALL_BEFORE = re.compile(r"(?:[=(!|&,?]|\breturn)\s*(?:\w+(?:\.|->))*$")   # a call, not a definition


def save_key_types(text):
    """{(scope, name)} of the types save_format_key saves whole (its sizeof list), the last two parts of each name:
    kit::Kit::BodyState is (Kit, BodyState)."""
    m = re.search(r"save_format_key\s*\(\s*\)[^{;]*\{(.*?)\n\}", text or "", re.S)
    out = set()
    for name in re.findall(r"sizeof\s*\(\s*([\w:]+)\s*\)", m.group(1) if m else ""):
        parts = name.split("::")
        if len(parts) >= 2:
            out.add((parts[-2], parts[-1]))
    return out


def save_regions(text, keys=frozenset()):
    """[(first line, last line, what)] of the save layout's regions in one file's text: a function that reads or
    writes the save (SAVE_FN_RE at its definition, not a call), a Saved* struct or the SaveWriter / SaveReader
    class, and a struct save_format_key saves whole (keys, matched with its enclosing scope's name)."""
    out, stack, pending = [], [], None
    for i, raw in enumerate((text or "").split("\n"), 1):
        c = code(raw)
        if c.lstrip().startswith("#"):
            continue
        c = re.sub(r'"(?:\\.|[^"\\])*"', '""', c)
        c = re.sub(r"'(?:\\.|[^'\\])'", "''", c)
        events = [(m.start(), "scope", m.group(1)) for m in SCOPE_RE.finditer(c)]
        events += [(m.start(), "fn", m.group(0)) for m in SAVE_FN_RE.finditer(c)   # a parameter, or a definition
                   if m.group(0).startswith(("SaveWriter", "SaveReader")) or not CALL_BEFORE.search(c[:m.start()])]
        events += [(j, ch, None) for j, ch in enumerate(c) if ch in "{};"]
        for _pos, kind, name in sorted(events, key=lambda e: e[0]):
            if kind in ("scope", "fn"):
                if not (kind == "scope" and pending and pending[0] == "fn"):   # a parameter's `struct X` keeps the fn
                    pending = (kind, name, i)
            elif kind == ";":
                pending = None   # a declaration: `void write_save(SaveWriter &w) const;`, `struct X;`
            elif kind == "{":
                what, scope = None, None
                if pending:
                    if pending[0] == "fn":
                        what = "a reader or writer of the save"
                    else:
                        scope = pending[1]
                        outer = next((s for _w, s, _l in reversed(stack) if s), None)
                        if SAVE_STRUCT_RE.match(scope):
                            what = f"{scope}, a saved struct"
                        elif (outer, scope) in keys:
                            what = f"{outer}::{scope}, saved whole (save_format_key)"
                stack.append((what, scope, pending[2] if pending else i))
                pending = None
            elif kind == "}" and stack:
                what, _scope, start = stack.pop()
                if what:
                    out.append((start, i, what))
    return out


def save_layout(f, hunks, texts):
    """(e) J-138: a code change inside the save layout's regions (save_regions) is a BLOCK, one per hunk."""
    regions = {}
    for side in ("old", "new"):
        text = texts(side, f)
        regions[side] = save_regions(text, save_key_types(texts(side, SAVE_KEY_FILE))) if text else []
    if not regions["old"] and not regions["new"]:
        return
    def region(side, at):
        return next((w for a, b, w in regions[side] if a <= at <= b), None)

    for h in hunks:
        # Each side's region lines in order, context included, so a write moved past another (the bytes move) differs.
        oldno, seq, first = h.old_start, {"old": [], "new": []}, {}
        for kind, n, t in h:
            at = {"old": oldno if kind != "+" else None, "new": n if kind != "-" else None}
            if kind != "+":
                oldno += 1
            for side in ("old", "new"):
                what = region(side, at[side]) if at[side] is not None else None
                if what and norm(t):
                    seq[side].append(norm(t))
                    if kind != " ":
                        first.setdefault(side, (n, t, what))
        if seq["old"] != seq["new"]:
            n, t, what = first.get("new") or first.get("old")
            add("BLOCK", f, n, "save-layout-changed", f"in {what}: a saved climb's layout changes (protected) | {t}")


def git_texts(base, head):
    """A file-text reader for save_layout: side "old" is the diff's base (the merge base of base and head, as
    `git diff base...head` reads it), "new" is head. A file missing on a side reads as None. Cached."""
    try:
        old = subprocess.check_output(["git", "merge-base", base, head], text=True, stderr=subprocess.DEVNULL).strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        old = base
    revs, cache = {"old": old, "new": head}, {}

    def read(side, path):
        if (side, path) not in cache:
            try:
                cache[(side, path)] = subprocess.check_output(["git", "show", f"{revs[side]}:{path}"], text=True,
                                                              stderr=subprocess.DEVNULL)
            except (subprocess.CalledProcessError, FileNotFoundError):
                cache[(side, path)] = None
        return cache[(side, path)]

    return read


# (f) J-138: the budget audit's list of known leaks (kExpected in tests/sim_tests_matron_budget.cpp). A listed path
# may run up to kGrowth over its listed peak, so a raised peak, a new row, or `over` / `uncharged` turned on lets more
# force past her budget: J-040 raised matron-damage's grab spring from 1339 N / 2012 W to 1615 N / 3603 W and the
# check said nothing. Any changed row or constant of the audit is a moved limit (BLOCK); a row only dropped is the
# known-defect entry a fix clears, which rides with that fix (AGENTS.md): REVIEW.
AUDIT_FILE = "tests/sim_tests_matron_budget.cpp"
AUDIT_ROW = re.compile(r'\{\s*"([^"]+)"\s*,\s*Path::(\w+)\s*,\s*(true|false)\s*,\s*(true|false)\s*,'
                       r'\s*([^,{}]+?)\s*,\s*([^,{}]+?)\s*\}')


def _number(v):
    try:
        return float(v.rstrip("Ff"))
    except ValueError:
        return None


def audit_moves(old, new):
    """[(what, looser)] for one audit row's (over, uncharged, peak N, peak W) going from old to new."""
    out = []
    for name, a, b in zip(("over", "uncharged"), old[:2], new[:2]):
        if a != b:
            out.append((f"{name} {a} -> {b}", b == "true"))
    for name, a, b in zip(("peak N", "peak W"), old[2:], new[2:]):
        if norm(a) != norm(b):
            x, y = _number(a), _number(b)
            looser = x is None or y is None or y > x
            out.append((f"{name} {a} -> {b}" + (" (LOOSER)" if x is not None and y is not None and y > x else
                                                 " (tighter)" if x is not None and y is not None else ""), looser))
    return out


def budget_audit(f, hunks):
    rows = {"-": {}, "+": {}}
    for h in hunks:
        for kind, n, t in h:
            if kind in "-+":
                for m in AUDIT_ROW.finditer(code(t)):
                    rows[kind].setdefault((m.group(1), m.group(2)), []).append((n, m.groups()[2:], t))
    for key, news in rows["+"].items():
        olds = rows["-"].get(key, [])
        for i, (n, vals, t) in enumerate(news):
            if i >= len(olds):
                add("BLOCK", f, n, "budget-audit-loosened",
                    f"a new known leak listed ({key[0]}, {key[1]}): it may now run past her budget | {t}")
                continue
            moves = audit_moves(olds[i][1], vals)
            if moves:
                add("BLOCK", f, n, "budget-audit-loosened" if any(lo for _w, lo in moves) else "budget-audit-changed",
                    f"{key[0]} {key[1]}: " + "; ".join(w for w, _lo in moves) + " | " + t)
    for key, olds in rows["-"].items():
        for n, _vals, t in olds[len(rows["+"].get(key, [])):]:
            add("REVIEW", f, n, "budget-audit-entry-dropped",
                f"a known leak dropped: only with the fix that clears it (AGENTS.md) | {t}")
    for name, n, old, new in changed_constants(hunks):
        if AUDIT_ROW.search(old) or AUDIT_ROW.search(new):
            continue   # the list itself: its rows are judged one by one above
        if not any(x[0] == "BLOCK" and x[1] == f and x[2] == n for x in findings):
            add("BLOCK", f, n, "budget-audit-changed", f"the audit's {name}: {old} -> {new}")


def check(files, resolve=None, texts=None):
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
        if resolve is not None and f != GEOMETRY_FILE and f.startswith("src/") and f.endswith(CPP_EXTS):
            tuning_constant_geometry(f, hunks, resolve)
        if texts is not None and f.startswith("src/") and f.endswith(CPP_EXTS):
            save_layout(f, hunks, texts)
        if f == AUDIT_FILE:
            budget_audit(f, hunks)
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
        resolve = git_uses("HEAD")   # a hand-fed diff: uses resolve against HEAD, or stay silent without git
        texts = None                 # and no base to read the save layout's old side from: that rule is silent
    else:
        base = a[0] if a else "origin/ScraperX-Claude"
        head = a[1] if len(a) > 1 else "HEAD"
        diff = subprocess.check_output(["git", "diff", "-U3", f"{base}...{head}", "--", ".", f":(exclude){SETTLED_FILE}"],
                                       text=True)
        # CLAUDE.md with its whole text as context, so each change's section is known (settled_edit)
        diff += subprocess.check_output(["git", "diff", "-U100000", f"{base}...{head}", "--", SETTLED_FILE], text=True)
        resolve = git_uses(head)
        texts = git_texts(base, head)
    check(parse(diff), resolve, texts)
    for lvl, f, n, rule, text in findings:
        print(f"{lvl:6} {f}:{n} {rule}: {text}")
    blocks = sum(1 for x in findings if x[0] == "BLOCK")
    print(f"rule_check: {blocks} BLOCK, {len(findings) - blocks} REVIEW")
    sys.exit(2 if blocks else 1 if findings else 0)


if __name__ == "__main__":
    main()
