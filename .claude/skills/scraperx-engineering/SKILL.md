---
name: scraperx-engineering
description: Deliver or review a real ScraperX change (gameplay, physics, presentation, tests, shipping) against live trunk ScraperX-Claude. Use for a bug fix, a feature, an architecture call, a PR review or a proof of completion; it rejects proxy success, duplicate authority, needless process and speculative polish.
---

# ScraperX engineering: make the game real, then stop

The rules in force are `CLAUDE.md` and `00_START_HERE.md`; this skill adds only how to do the work. Where it
seems to disagree with them, they win. It is not an approval gate and never resumes paused work or seizes a
job another agent owns.

## 1. Know what is true now

- Work against the live head of `ScraperX-Claude` (never `main`). Unmerged branches, old comments, plans and
  memory are leads, not truth.
- Read the owning function and its tests, found via `REPO_MAP.md` or grep; not whole subsystems, not long logs.
- The job's issue (its `owner:` label) and the open issue labeled `board` say who owns what. One owner per
  function.

## 2. Decide what is worth doing

Write one sentence before editing: **after this change, [actor/input] causes [authoritative consequence] that
[player/runtime] can observe, proven on [named real path], with [invariants] still true.** If you cannot
write it, investigate until you can; never ship a cosmetic stand-in.

Kill a change that duplicates an existing capability, adds a second owner of the same truth, only looks
right, uses a test or UI as a proxy for gameplay, or cannot survive the Android shipping path. Add no
checklist, workflow, layer or test only to look rigorous.

## 3. Diagnose before building

For a fight contact, perception, movement or force, determinism, save/load, native-versus-drawn or test-drift
failure, open only the matching section of `references/ENGINEERING-FAULT-TREES.md` (ChatGPT, 11-10): its rival
causes and one discriminating probe each. They are hypotheses; check names and numbers at the live head.


- **Bug:** reproduce the exact symptom at the right revision; find the first invalid state transition; name at
  least one rival cause; run the cheapest check that separates them; fix the owner of that transition; rerun the
  same failing path. Two failed patches: stop patching and re-trace the whole causal chain. Never fix by
  waiting longer, padding a retry, loosening a threshold, or a special case for one place.
- **Feature:** the smallest general mechanic that gives the full player-visible causal consequence (input,
  state, physical effect, presentation, persistence, proof), not per-route scripting. A small diff is not a
  complete slice.
- **Architecture:** only for a measured deficiency in the existing seam. Fewer sources of truth win.
- **Review:** the diff at the exact PR head, its owners and tests; try to break it. `LAND`, `FIX` with a
  reproducible command/input/expected/observed, or `NOTE`. Style never blocks.

## 4. Keep the game causal

The sim (`src/sim/`, C++17 + Jolt) owns every consequential fact; Godot presents. No physics or mission
decision in presentation, no teleport, invisible safety, scripted outcome or flag posing as a mechanism. Her
perception uses what she sensed, never his true state. Determinism: stable ordering and IDs, no unseeded
randomness or wall-clock decisions, sort before taking the nearest N. No engine object or raw pointer kept
across frames; no blocking native work on Godot's main thread. Limits and settled calls: read them in
`CLAUDE.md`; never restate or relitigate them here.

## 5. Prove it, at the right strength

These are different claims; state the strongest you actually have, never a stronger one:

`IMPLEMENTED → BUILT → EXECUTED → OBSERVED → VERIFIED ON FOLD`

Source present is not built; a passing native test is not observed gameplay; an APK is not a Fold run; a
configured workflow, posted comment or old green run is not proof of this head. Visual work is looked at in
the production scene (`CLAUDE.md`, "Look at it"); if that is impossible, say the eye proof is missing.

## 6. Ship and stop

Once the sentence from section 2 holds on its real path, the reached contracts still hold and the evidence
exists: push through the normal gate and stop. A nonblocking discovery becomes its own job, not an
expansion of this one. Reopen only for a demonstrated failure of the same condition. The goal is to build and
play ScraperX, not to keep developing the development system.

Report compactly: **Result** (the consequence, or the exact blocker), **Proof** (the check and its observed
result; not run is not failed), **Next** (one action, or none).
