# Matron hears and climbs — implementation handoff, 2026-10-21

Branch `dot/matron-hears-and-climbs-20261021` starts from remote
`ScraperX-Claude` at `b9f7d14`. Capture was not integrated there; its completed
commit was reused as `900a28f`. The original capture branch remains at
`19fe452`. No merge into Claude, water changes, or paused load-bearing work.

## Playable scope

The existing 154 m north band, loose crates, and ladder to the 176.25 m ring
provide the encounter. Walking/sprinting/climbing/landing, forklift machinery,
yard-crane events and physical ImpactLedger collisions already produce native
sound; these existing callers remain the sources. A pushed crate can draw her
to the collision while the player stays out of sight. Heard ladder movement
can lead her to its known upper exit. She must acquire actual rung contacts,
climb the 22 m elevation, and cross the upper lip to investigate the ring.
Quiet can lose the trail; missing or blocked holds can defeat the route.

`Matron::hear`, `carry_m`, and `pre_step` now preserve the selected source and
separate an acoustic estimate from visual confirmation. Hidden player movement
no longer overwrites the waking source, a louder decoy's route, bay decisions,
stock decisions, or the presentation's head attention. Authored ladder/cable
exit inference applies only to observed or actually heard player traversal.
It is an estimate, not confirmation that the player reached that exit.

Static geometry muffles direct paths; existing shaft paths are also checked.
The collector stores at most 16 hits, stops on saturation conservatively, and
all acoustic paths share a hard 68-query/tick budget. Event storage remains
16 entries. Acoustic evidence has a 40-second ceiling, with shorter ordinary
hunt/search timers; cautious travel uses the bounded longer lifetime. No
new general acoustics engine or permanent target coordinates.

## Physical climb

The existing single upright NPC capsule is retained. A ragdoll would introduce
unneeded ownership/collider handoffs. Body-local anchors come from real,
reachable, visible upward-facing surfaces; sensors cannot support her.
Gravity and collision response remain active. A velocity-error actuator applies
at most 5500 N (tuning, not measured anatomy), with opposite point force on a
dynamic support. Hand reach is bounded at 1.9 m. Support contact is revalidated
before force each tick. No production climb position/velocity writes.

The existing full-capsule clearance query checks lift/landing at the lip, and
contacts arbitrate the path. Missing support, excessive reach, prolonged force
overload and stalled motion release the hold without resetting momentum.
A missed next rung or obstructed ascent can result in a fall and floor recovery.
Normal native routing resumes afterwards; landing is not guaranteed.

Godot receives the native hand point, force, grip/release counts and perceived
lead provenance/age. Hand IK follows that point and hidden-player movement
cannot steer her head. Presentation neither owns the body nor supplies support.

## Verification

Release native library, tests and Linux GDExtension built with existing pinned
Jolt/godot-cpp, GCC 14 and CMake. Commands use `/workspace/matron-hears-build`:

```
ctest -R 'matron-|matron_capture|parkour_flow|shared_survival' --output-on-failure
SCRAPERX_ONLY=matron ./scraperx_sim_tests
```

Final focused CTest: **7/7 passed**, approximately 15 seconds total on this host.
The broader `SCRAPERX_ONLY=matron` run is **not green**: chase, catwalk, Turn,
stock, mind, crate crash, vault and moving-cage/ride cases passed, then it stopped
at `matron unrig: she comes for the noise, sees the player and hunts`.
That remaining older noise/chase fixture has not been adapted or diagnosed
further; downstream cases were not reached. Do not report a full regression pass.
Log preserved during implementation: `/tmp/hearing-fullmatron.log`.

Focused coverage:
- Actual 154→176 ladder caller and physical mantle: measured ascent 0.600128 m/s,
  peak actuator 3390.2 N, maximum hand reach 1.53578 m, no player deaths.
- Six real Jolt support fixtures, each replayed: missing steel, sensor-only
  rungs, removed held rung, overhead blockage, obscured reach, incomplete ladder.
  Checks finite force, continuous per-tick motion, release, falling and floor
  recovery; identical inputs must reproduce final physical results.
- Quiet/loud, open/occluded/saturated paths, retained wake point, decoy provenance,
  hidden movement, evidence expiry, visual reacquisition, query budget and replay.
- Actual crate-S/crate-N collision caller: 239.035 J impact, loudness 0.55983,
  heard from 17.82 m; unseen player 18.817 m away, exact collision lead,
  investigation within 0.012 m, then abandoned. No synthetic hear call here.
- Five existing physical capture scenarios, parkour flow and shared survival.

Official pinned Godot 4.7 CLI: `matron_perception_climb_test.gd` passes bridge,
hand-anchor and unseen-attention checks; main `--ci` boot/approach passes. The
existing Matron figure test also passes all nine checks. Separate native `bay`
(walkway, physical trap exit, chain) and `matron-room` regressions pass. The
new presentation smoke is wired into the existing CI workflow. The original
capture quaternion test remains available.

## Limits

This slice replaces ladder ascent/mantle authority, not every pre-existing NPC
traversal mode. Existing pipe/cable/vault behavior remains; the pipe controller
still uses its older gravity/velocity staging and is not claimed as converted.
No articulated hand/finger mechanics, general bar acrobatics, injury redesign,
route redesign or guaranteed pursuit. Static occlusion is deliberately coarse;
dynamic objects emit sounds but do not form a general moving acoustic model.
Existing route-graph allocations are unchanged; new hearing/contact query
storage is bounded. Desktop test times are not Android performance evidence.
No device, phone, Android build or visual-comfort proof is claimed.
