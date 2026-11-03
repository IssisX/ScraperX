# Handoff: the fatality's hang at a bare lip (north band spot -6, -129)

From the hold session (session_01W3SSMzVbgoYyKh8XvAaneP), 11-03. The owner's call, 11-02: *"Rework"*,
meaning make the hang work at the north spot instead of her floor version. Then *"Fresh"*: the job
goes to a new Claude session.

## Goal

The band test's north spot, `matron fatality band (-6.0, -129.0)` in `tests/sim_tests_matron_grab.cpp`,
ends in the hang and its let-go like the six east spots, with every band check passing. Once it does,
remove the floor-version branch for that spot (a protected test change in PR #103).

## What is true now

- The base is PRs #102 (behavior) and #103 (checks), on `claude/matron-hold-real-arms-20261102` and
  `-checks-`. At the north spot she does her floor fatality, and #103 accepts that.
- Her lethal edges in reach of (-6, -129):
  - The band's outside, z -124 (normal +z), x -4.9 to -7.4. It is a **bare lip, no rail** (not a
    beam: an earlier note was wrong). Past the lip there is a 0.2 m strip about 0.18 m down, then
    open air. From the real-arm hang (0.95 m out from her middle, his middle about 0.95 m under her
    deck, her 0.9 m/s flick) the fall kills at 46 m/s.
  - Her other edges in reach drop onto a deck 2 m down (the shaft side) or onto a structure (the -x
    side).
- `choose_fatal_edge` rejects all of these. The old `hang_room` / `lethal_from` size the old 3 m heave
  and its high drop, which an overhead beam blocks. A real-arm pair fixes the choice:
  `hang_room_real` (his body under her hand clear, allowing the deck's face and lip strip) and
  `lethal_from_real`. Tried as a second pass only when no edge passes the first, so the east spots
  are unchanged.
- The over at a bare lip: her second hand cannot reach his ankle (he lies on her deck, legs far past
  her 0.5 m arm). The working idea: she slides him out by his belt till his middle is 0.25 m past
  the lip, opens that hand, he tips head first, and both her hands take his ankle as his legs come
  up past her shoulder (about 0.4 m). Also needed:
  - no handback while his ankle comes over;
  - her second hand carries the ankle to her centre line so her first can take it;
  - the hang's hand at 0.30 m over the deck (0.08 m is past her bent arm).
- All of that is commit `021f70f` (reverted in `5ddd340`; cherry-pick it to start). It works when he
  lies across the lip: in the scratch probe the whole fatality ran, catch to let-go about 9.1 s.
- **What fails:** in the test's exact setup her pin lays him **along** the lip (body axis parallel
  to the edge). With his middle past the lip he can only roll off sideways; his ankle never swings
  up to her, so no catch happens and she sets him back on her deck.
- A first try at squaring him across the lip: collar 0.25 m farther out, collar and belt 0.25 m to
  her left, belt 0.10 m farther in. The patch is in the hold session's scratchpad; recreate it from
  this line. It improved his angle to about 47 degrees but did not tip him, and it pushed the over's
  start past 11 s.

## Why it is hard

- Her real arm (0.50 m) cannot reach his ankle or below about 0.16 m over the deck while bent over.
- A point grip at his belt cannot turn him: it holds his middle up.
- The fatality's length bar in #103 is 7.5 to 13 s, and the north path is already about 9 s before
  any squaring.

## Laws

- Her force and power stay 8 kN / 44 kW.
- Every arm bone at most 5% over rest; hand within 5 cm of what it holds.
- No let-go over open air except the fatality's.
- Gate every bare-lip change to `fatal_hold_` and a bare lip (`rail_up_ < 0.2`, `!pin_here_`), so
  ordinary holds and the east rails stay as proved.
- Test-check changes go only in #103, with a plain before/after.
- Effort high (the owner set every session to high on 11-02).

## Test only what a change reaches (CLAUDE.md)

1. While fixing a failure, rerun only that test, and only its failing part where it lets you pick
   one (`SCRAPERX_ONLY=<group>`, `SCRAPERX_WAY=<n>`). Never edit a test to add a picker.
2. Once fixed, run once the native groups and touch scenarios the changed code can reach (which
   functions changed, which tests run them); say in the report which and why.
3. A test that passed on this exact code is not run again; nothing runs both in a scratch copy and in
   the tree being shipped.
4. The full native suite, every touch scenario and the Android build are CI's job, never also local.
   Before a push to `ScraperX-Claude`, run `wo000-delivery-spine.yml` once on
   `<agent>/integrate-<job>` (pushing it runs the proof) or dispatch it: that run is the proof (every
   job green; `publish-checkpoint` runs on trunk only, skipped elsewhere since 11-02). The full
   native suite runs locally only when a change reaches the whole world (the kit's body order, the
   world's solids, shared physics or tuning, the contact listener, save layout) and no CI run is
   coming.
5. Read the PASS/FAIL lines and the lines around a failure, not whole logs.
6. Handing work to another agent: put this section in the brief.

Here, that means `SCRAPERX_ONLY=matron-fatality` (about 10 minutes). A scratch probe copying the
band test's setup (`caught_at` plus `still_dangerous`) iterates in about 1 minute per spot.

## Also open, for the integrator

CI's step "Verify no owner-blocked change rides this branch" fails any branch carrying rule-check
BLOCK lines. #103's check changes are exactly those, so run 41542994430 on
`claude/integrate-matron-hold-real-arms-20261102` stopped there and no tests ran. How a protected PR
gets its proof is the integrator's or the owner's call.
