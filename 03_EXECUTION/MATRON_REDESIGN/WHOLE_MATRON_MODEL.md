# Whole Matron — one compact model and ranked repairs

Base: ScraperX-Claude d131b68c, 2026-10-29. Audit base d28d3486 is stale;
S6 (#25, 8d1d8f1) and L1 (9899ca3e, 2026-10-28) have since landed.
This file is docs-only. It does not change code, tests, or gameplay.

## One compact model

Intent -> feasible action -> actual physical outcome -> complete rendered
body -> player perception and response -> recovery and next decision -> reset.

The chain breaks in three places, and every ranked finding below is one
of those breaks, not a subsystem bug in isolation:

1. No encounter owner. No object, state, or proof channel spans
   wake -> pursuit -> contact -> weight transfer -> interruption ->
   recovery -> next decision. Subsystem exams pass while the encounter fails.
2. Sim posture != rendered posture. The sim hand, the rendered hand, and
   the drawn hair/skirt are checked by different tools, or not at all.
3. Proof is strong in the middle, weak at the ends. Timers, latches,
   substitutions, and fixed coordinates at the start and end of a move
   are not covered by the move's own exam.

Design exceptions, preserved as approved: her supernatural abilities and
contortion, as specific design exceptions. Everything else — hair,
clothing, bodies, props, attachments, contacts — must express solidity,
gravity, inertia, and weight.

## Counterexample (why every subsystem test can pass while she is broken)

Standing guard pose passes. On the 176 ring she crawls, twists her neck
110 degrees, and grabs the player. Rendered ribbons pass through
nose/cheek/shoulder, the left rung hand is pose-only, fingers close by
state not around the ankle, upright feet skate, she walks toward a fixed
pin spot, stalls, gives up beat by beat to Wipe, yet the run still ends.
The player sees first-person spin, hears the identical single-variant
laugh, and cannot punch. Each exam isolates its subsystem, uses test
hooks, and stages on deck 154.

## Ranked repairs, in order (integrator, 2026-10-29)

### M1 — hair posed-volume repair
Problem: hair through face, neck, shoulders, lower torso; worst in
close/hold shots.
Verified cause (d28d3486, re-check on d131b68c before landing):
avoidance is exactly two spheres on chain points only in
`matron_rig.gd:step_hair()` — skull centre [0.0, 1.7285, 0.0852] radius
0.10 m, back sphere radius 0.17 m. Skull sphere misses lower face/ears
(eyes ~0.086 m inside, nose/mouth/chin ~0.125-0.135 m outside); front
130-degree chain gap, front strands clamped to side chains; endpoint-only
plus straight chords (0.09 m chord on 0.10 m sphere dips ~10.7 mm inside,
root clearance only ~6.5 mm, no ribbon margin); no side memory
(pitch-forward crawl/climb pushes points toward/through the chest); no
collider below back-sphere bottom (~y 1.26) while hair reaches y 1.139;
rendered tail last ~0.08 m rigid, simulated tip 0.43 m vs rendered tip
0.53 m below root; shoulders/arms/neck outside back sphere; breaking
poses (neck twist up to 1.92 rad, crawl pitch, Turn head-back up to
2.6 rad, head-down hanging, close holds with no player/held-body/ladder
collider); hair exempt from `govern()`, `step_hair()` runs after
`contact()`/`govern()`, damping frame-rate dependent (dt clamped
1/240-1/30). Plus: rest direction is head-local (`head.basis·DOWN`),
and dt clamped to 1/30 s means at 20 fps hair moves two-thirds of real
time while the body moves all of it.
Repair, general, rig only: posed body-volume test (capsules/spheres
fitted per frame from bone transforms: head incl. lower-face protrusion,
neck, shoulders, upper+lower torso) against chain segments with
ribbon-half-width margin and a remembered-side rule; simulate/clamp the
rigid tail. Do NOT hide, shorten, or immobilise hair.
Evidence: rendered-vertex test, not control points. `tools/matron_probe`
(on `claude/matron-film-tools-20261028`) records her final drawn body
per frame from any touch scenario, every skinned part as posed plus hair
as drawn, and scores hair/skirt penetration against drawn meshes.
Poses: standing, crawl-pitch, 110-degree neck twist, head-down, hold.
Builder: 20 chains x 5 bones, 0.09 m segments, 1,467 strands,
lengths 0.26-0.55 m.

### M2 — her collision follows her pose
Scope, files, proof: as spelled out in issue #3 comment 6498164466.
Why it comes before rung/feet: a body you can hit that is not the body
you see breaks belief in every encounter.

### M3 — her hands on the prop
Scope, files, proof: as spelled out in issue #3 comment 6498164466.
Includes the grip finding: fingers through / floating beside the ankle,
arm stretches like rubber. `hold_reach()` moves only the wrist; arm
stretch up to `HOLD_STRETCH_MAX = 2.7x`, shoulder pull up to 0.4 m;
finger curl is a global state value, never wrapped around ankle/arm
radius. General repair: aim wrist at the held limb, drive finger curl
from held limb radius + wrist-to-surface distance.

### M4 — the load tips her
Scope, files, proof: as spelled out in issue #3 comment 6498164466.
Includes the fatality finding: fixed pin/bollard/trip coordinates
(`matron.hpp:1221-1223`, all 154-deck), no Mover/climb to 154, each beat
gives up after 20 s to Wipe, run still ends. General repair: work-spot
selection via the climber's scan, or an explicit failed-fatality path
that does not end the run as a win. Whether Black plays before reset
was not line-traced; check it in M4, do not assume it.

### M5 — the rung's second hand really grips (no fixed 2.6 m)
Rung climbs: right wrist is reached to validated `climb_hand`;
two-handed reaching applies only to mantle, jump_catch, pole_up,
pole_down, hang_drop; left wrist + feet are pose-driven, no
target/support check; non-`climb_hand` path rides by fixed
`HANDS_ON_RUNG_Y = 2.6`. General repair: extend validated
second-hold/feet-support logic to rung moves in
`matron_rig.gd:_contact()`.
Re-check on trunk first: S6 landed `contact_*` exports and claims
(plants, ride, hand2, feet, hand, hang, brace) and the pipe brace.
The d28d3486 finding "climb_hand2 reported, never gripped" may be
stale; if it is still true on d131b68c, it belongs here in M5.

### M6 — upright feet lock
Upright feet skate and float/sink. World-locked plants exist only on
all fours; upright has only a spring ride (`RIDE_W = 7.0 rad/s`,
clamped -0.15...+0.30 m), no stance-phase foot lock. General repair:
apply the all-fours world-lock idea to upright stance feet.

### Her pipe climb, general (2026-10-29, exit board item 6)
`src/sim/matron_pipe.cpp` `riser_routes` reads every riser her scan
records (`traversal::Riser`, fb-bot J1) into her ways between the decks
beside it: stand, edge top (rail or bare edge), face, line, collars.
No riser coordinate is left in her code (the corner pipe's place lives
only in its exams). On the face both hands hold (`contact_hand2`), one
over the other; her hips sit at the least stand-off that fits (0.04 m,
widening to 0.2 m past a flange); feet brace on it. `travel_to` takes a
riser where her map has no other way (`plan_with_risers`). Proof:
`matron-riser` (a 0.4 m post by a deck, up and down), `matron-pipe`
(both hands every face tick), `touch_matron_pipe` in CI.

## Other findings, not in M1-M6, not re-claimed

- No production punch: `take_blow()` frees only if
  `blow_by_player_ && !rip_begun()`, but no production caller exists;
  `player_blow_landed()` appears only in debug code and one test; no
  punch verb in `input_router.gd`. Punch design/range is a Cory owner
  decision. integrator punch lane / PR #23 covers it; do not duplicate.
- No third-person encounter camera (shipped at d28d3486): superseded
  by L1 third-person hold camera landing (9899ca3e / 14e556b5).
  Framing distances are a design call.
- Contact sounds do not match contact, lower impact: plant/step sounds
  from presentation constants / stride counter, not native contacts;
  hold sounds from counters, not finger closure; single-variant
  laugh/effort/cluck with with-replacement pick and no no-repeat guard.
  The integrator owns all game sound; route via the integrator, do not assign to Builder.
- Per-executor budgets, not one creature budget: each `Mover::step`
  takes a Budget it ignores; `pre_step` mints two full 8000 N / 44 kW
  accounts/tick; Climber/pipe/routecraft each mint or cap independently;
  grab hand muscle and fatality caps draw on no Budget. Owner is the
  existing mover-contract one-budget job (step 6). Do not re-claim.
- Ladder exemption + missing 30 s back-off: capture gate exempts
  climbing bodies (`physics_world.cpp:847`), contradicting the
  2026-10-28 owner call in `00_START_HERE.md:20`. No 30 s back-off
  state exists. Assignee via integrator/PE, not here.
- Candidate, unproven rendered: skirt vs legs in wide poses; Turn
  contortion self-penetration (shipped code, ambiguous by design —
  interpenetration vs intentional horror is an owner judgement);
  toppled / pipe-head-down figure placed by formula, not surface;
  holding landing takes neither her feet nor him (unrepresented weight
  transfer, held-body mass unverified).

## Superseded, do not revive

`fb-bot/matron-chase`, `fb-bot/matron-touch-fixes-20261024`,
`fb-bot/gemini-argon-force-law-test-20261025`, the 8000.5 N figure
(trunk is 8000 N / 44 kW, lunge 9800 N / 65 kW, weight 220 kg),
any claim that ladders 35/35 means "all ladders fixed" (6 refused),
any claim that chase 9/10 + 10/10 equals a 17/20 full pass.

## Proof rule for every batch

General, at the correct owner, bounded batches. Each batch: its own
tests green, regressions green, branch CI green except the known
off-mainline step-19 halt, mainline CI green after landing, and a
before/after comparison through the same production path. Never weaken
a test, never delete the object a test measures, never stack geometry
tweaks in the same spot. Report: Problem -> player impact -> verified
cause -> repair -> evidence -> remaining uncertainty.
