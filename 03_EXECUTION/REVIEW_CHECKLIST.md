# Review checklist: write, then break

One agent writes a change; a second agent tries to break it before it lands. The second agent
reads the diff against the rules below and answers each with PASS, FAIL (file:line, why) or N/A.
Any FAIL stops the landing until it is fixed or the owner decides.

## Step 1: the script

Run `python3 tools/rule_check/rule_check.py <base> <head>` (default: `origin/ScraperX-Claude...HEAD`).
It already flags, so do not redo them by hand:

- protected files changed (`MANIFEST.txt`, `SHA256SUMS.txt`, `01_PRODUCT_AUTHORITY/00_GOVERNING_LAWS.md`,
  export presets / app id, keys);
- a test check removed, or a test tolerance moved, with its direction (LOOSER is a BLOCK);
- her budget numbers changed (`Limits`, `total_force_n`, 8000.5 N, 9,800 N, 65 kW, 220 kg and the
  other named limits);
- REVIEW lines: possible per-route / per-deck / per-transition cases, velocity and position writes in
  her code, structured bindings near lambdas.

Every BLOCK means the change goes only as a PR into `ScraperX-Claude`, which only the owner merges.
Every REVIEW line gets a one-line verdict from the reviewer.

## Step 2: what the script cannot see

1. **Budget only.** Every force on her comes out of her one budget (`budget.take`) as force over
   time. No velocity is set or added outside it, not even a small one, and a push-off is
   force times time, never a speed change. Check each REVIEW velocity line: is it a read, or does
   the change in speed come from a budgeted force?
2. **The lunge is its own move**, checked against its own limits (9,800 N, 65 kW), never folded
   into the general budget or used to cover another move.
3. **No per-route code.** Nothing keyed on which deck, ladder, route, region or transition she is
   on, by id, name or a world coordinate. Climbing is general skills over the steel she scans. A
   number that only makes sense at one place in the tower is a per-route case.
4. **No teleport or snap.** At most 0.20 m of movement per 1/90 s tick, through the physics, with
   gravity on. Falls are real: a miss falls.
5. **Tests.** No test weakened, skipped, deleted, or given a new escape (an early return, a wider
   window, a check moved behind a flag). A changed expected number must be explained by the change.
   A small world-geometry tweak is allowed only as a protected PR, and says so.
6. **Android -Werror.** Android builds are clang C++17 with `-Werror`. No structured binding
   captured by a lambda (clang rejects it under C++17); no unused variables or parameters left by
   the change; no narrowing that g++ allows and clang warns on. Best proof: the Android cross-compile
   (`scraperx_native`) built locally or in the proof run.
7. **Tests run were the right ones.** The groups named in the handoff reach the changed functions
   (look them up in `REPO_MAP.md`), and nothing was run twice.
8. **No duplicate work.** The change does not redo or undo another agent's in-flight work (check
   the assignment board on issue #3).
9. **Repo text.** American spelling; no AI model names in repo files; docs-only commits end with
   `[skip ci]`, and nothing else does.

## Step 3: the verdict

Post on issue #3, addressed to the writer: the script's last line, then each item above as
`n PASS` / `n FAIL file:line why` / `n N/A`, then `LAND` or `DO NOT LAND`.
