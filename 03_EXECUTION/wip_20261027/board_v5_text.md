## 📌 ASSIGNMENT BOARD v5: one skill, six movers, the retirement plan

Supersedes board v4 (FB Bot, 2026-10-24, 4:22 PM CDT) and the later-edited v1. Written by Claude (the architecture session, 2026-10-27, 9:15 AM CDT) for Cory's question this morning: *"are we doing each ladder, each crate, each object its own shit rather than a general skill ... real progress or spinning our wheels?"*

**The verdict, checked by running the code on `ScraperX-Claude` `711bc2d6`:**

1. **The general skill exists and works.** `SCRAPERX_ONLY=matron-climb-scan`: 154→132→143→154→176.25→154→143→132, then →176.25→198.25 (standpipe) and 198.25→220.25 (panel), **0 failed moves, 0 slips, every tick within 8,000 N**, 44 s wall clock. `SCRAPERX_ONLY=matron-exam-climber`: **200 of 200 runs clean** on 100 seeded ladders, poles, panels and ledge stacks nobody authored. The ladder audit says the same: the first-ladder fix "holds everywhere ... the drift came from the general mantle code, not from one ladder."
2. **What is still per-object is the wrapper, not the skill**: the corner pipe (`matron_pipe.cpp`, 517 lines, 23 constants pinned to one riser), her walking graph (`levels_`, coordinates per deck in the constructor), the authored route map (`matron_routes.*`, test-only but still compiled and consulted), a second rung climber (`Matron::climb`), and a six-way priority chain in `pre_step` (`matron.cpp:3308-3326`) that picks a mover every tick. That chain is why each general fix regressed a neighbor this week (top-out → shelf; rung preference → knock; force-law → cable let-go and routecraft accounting; lunge → chase cap).
3. **Not too far gone.** The retirement plan below removes movers one at a time behind exams that already exist. Every step leaves the game playable and the suite green.

The full account, with file and line for every claim and the proof output: `03_EXECUTION/MATRON_REDESIGN/HOW_SHE_MOVES.md` on branch `ccr-b6a86472-vntrv1` @ `5f8d7f4` (docs only, `[skip ci]`; the integrator merges it onto trunk).

### Rules of this board
- **Exams are per object. Code never is.** A failing ladder, crate, cable or panel is a fact about a law (reach, mantle, wrap, cost, clearance) or about the world's steel. A fix names which. A fix that names a place is refused.
- **No new mover, no new `follow_*` in `pre_step`, no new coordinate constant.** The count of world elevations in Matron source (36 + 19 + 2 + 1 today) only goes down.
- Each step lands alone on a green proof run; protected changes by pull request for Cory (comment 6442285390). Steps 1 and 2 go first; 3 to 6 wait for the mover contract, except where noted.
- Post results here with SHAs and real PASS/FAIL lines; one owner per step; mark ⏳/✅ on this comment.

### Playtest first (Cory, 2026-10-27 1:10 PM Central; comment 6450207364)
Cory's playtest is now the top of both queues, ahead of steps 3b, 4 and 5:
1. He loses track of her; start her near the ground.
2. A 5 s freeze when she bursts out.
3. Both arms from the start, and the arm rip.
4. She gets stuck on the bay's bracket.
5. She faces the wrong way in contact.
6. He can't fight back, and she never really attacks.

**Claude:** 2, 5, an on-screen tracker for 1, and the APK. **Grok:** A (start her low: 1, 4), then B (both arms and the arm rip: 3), then C (the fight, with the crate shove made lawful: 6).

### Queues

**Claude (integration session)**
- ✅ 0 (trunk `c6f68531`; protected PRs #5 and #6 merged at Cory's request; comments 6444484456, 6445014678). The ladder audit landed with `ccr-b6a86472-vntrv1` (this document) and `ccr-33b88ada-pkj1e1` (tools).
- ✅ 1. **One mover contract, one active mover.** Landed: trunk `2dc872c6` (comment 6450548023). Proof run 40532685534: steps 1-18 green. Second-agent review: LAND. Finish fact: the "elsewhere" branch makes one call and no mover reads another's fields. Proof: `matron`, `matron-remaining`, `matron-chase`, `matron-pursuit`, `matron-climb-scan`, `touch_matron_line`, `touch_matron_act`.
- ⏳ 2. **Delete the authored map from runtime.** Stacked on step 1, landing in this order:
  - the budget fix `f572ba19` and the crash fix `91992673` (proofs green), with the review's lunge fix `e1086587` (proof running on `claude/integrate-boarding-lunge-20261027`), landing as one merge;
  - the protected test PR, `f8a873a1`: the review found three checks weaker than before; they get fixed before it opens;
  - the map's removal, `f3c2c42f` (proof running on `claude/integrate-routes-out-20261027`).

  Finish fact: no `routes::Region` in `src/sim`, and `matron_routes.cpp` is gone.
- **Playtest:** the freeze when she bursts out (her whole-tower scan in one tick, `physics_world.cpp:707`); her facing in contact; a tracker on screen (her direction, floors up or down, distance); the APK.
- Then: **the 462 ladder nudge**, as a protected PR (comment 6444295070):
  - the 462 ladder's plank, 4 cm short;
  - the C2 cradle ladder: Cory, **leave** (its top is 1.1 m sideways from the 82 m walkway; a spot she can't follow).
- Then 4 (moved from Grok). **The legacy ladder executor goes**: `Matron::climb`, `begin_climb` and `find_climb_hold` replaced by `Climber` rung moves, same cap, same reaction. Proof: `matron-ladder`, `matron-ladders`, `matron-climb-support`, `matron-chain`.
- Always: reviews and lands the other agents' work with `03_EXECUTION/REVIEW_CHECKLIST.md`.

**Grok** (talks to Claude here directly: `To Claude:` / `To Grok:`)
- ⏳ **One-budget forces**: `grok/one-budget-forces` @ `5e6ac5c2`, proof 40543584615 (comment 6447828930). Claude reviews it and lands it after step 1 and the budget fix.
- ✅ **Workflow tools**: CI ccache, `rule_check.py` and the repo map generator, on trunk (`71896024`).
- ✅ 3a, reported in comment 6447859763. **Cory: hook** (12:10 PM Central): hooking both far corners counts as holding the riser.
- **Playtest A, B, C** (comment 6450207364):
  - A: start her low (locker, post and eye to grade or deck 1-2);
  - B: both arms from the start, and the arm rip;
  - C: the fight. Her real attacks, his punch, and the crate shove made lawful (Cory: **lawful**).
- **The stronger Matron, as a protected PR** (`a5eea2bc` rebased on trunk; Cory: yes). Small.
- 5 (moved from Claude; plan in comment 6448428903, reviewed in 6448668549). **Walking from the scan**: `levels_` nodes, edges and patrols, `deck_steel_` and `kLeapX` replaced by her scan's regions, `walk_path` and `GapJump`. Posts stay as named places. Finish fact: no node coordinates in the constructor. The handoff is comment 6448231005.
- 3b (unblocked; handoff in comment 6448430277). **Retire `matron_pipe.cpp` and the `kPipe*` constants** onto that general climb. Her ride down stays fast and loud, from the law, and she arrives no sooner than today. Proof: `matron-pipe`, `matron`, `touch_matron_line`, `matron-climb-scan`.

**ChatGPT**
- 6. (after 1) **One brain**: the scan is the only planner; routecraft keeps its bar execution as a move kind under the one `Budget` and one force report; `matron_found_climb.cpp` folds into the planner's edge building. Finish fact: one planner, one `Budget`, one report; `routecraft: reported force matches actual body accumulator` passes with no special case. Proof: `matron-routecraft`, `matron-exam-routecraft`, `matron-found-climb`, `matron-exam-climber`, `matron-towers`.
- Until then: the hand-to-hold constraint stays parked (Cory's call), no new proposal threads.

**FB Bot**: stopped (comment 6442945905). Nothing queued.

**Cory**
- Answered 2026-10-27, 1:10 PM Central:
  - crate shove **lawful**;
  - 300 s stuck-climb cap **keep**;
  - cradle ladder **leave**;
  - Claude deletes its own landed branches **yes**.
- After the next APK, with headphones: does her wake-up rumble sound right? **good** or **fix**.
- 👍 on comment 6442285390, so the agents know the protected-change rule is yours.

— Claude (architecture session); queues updated by Claude (integration session), 2026-10-27 1:40 PM CDT

---
_Generated by [Claude Code](https://claude.ai/code)_
