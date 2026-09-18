# Work Order 010 — The Player's Weight Is A Real Force

## Objective

Make the player a load-bearing component of the machine rather than a passenger, so that the ascent is something they *cause*:

`body on a human-scale control → real cable tension → real valve opening → real piston work → the lift parks inside the mantle band → a route exists that the machine never opens on its own`

## Existing proven truth

WO-006 established a causally complete plant; WO-009 proved the player can ride it to the catwalk and that the catwalk mantle is gated by real geometry. Two things were not true, and both were discovered by measurement in this work order rather than assumed:

- **The player massed 602.92 kg.** The capsule was created with no mass override, so it took Jolt's default density (0.603 m³ at 1000 kg/m³). Measured, not inferred.
- **Two WO-006 assertions passed only because of that.** "A player standing on the tipper opens the same valve" was true only for a freight-scale body. Re-measured at 85 kg: the 900 kg counterweighted tipper deflects **0.000144 rad** and the valve opens **0.00**.

## Governing authority

- GDD §17 — "the tower itself is a major source of power. Industrial-scale work should often require access to the correct machinery rather than granting the player industrial-scale strength directly." A 602.92 kg player is industrial-scale strength granted directly.
- GDD §16 — causal stacking: `reposition mass → alter load/alignment → change machinery or process behavior → change geometry/hazard/access → change traversal`. Also: complexity must stay "legible enough that the player can form useful hypotheses from visible, audible, spatial, or operational evidence," and mechanisms should prefer "multiple plausible uses over one-purpose puzzle devices."
- GDD §4.3 — the common progression blocker is missing physical capability or access to an appropriate mechanism, and must be explainable in-world.
- Governing Law 24 — machinery cannot create capability from nothing.
- Governing Law 26 — no physics theater, no second authority.

## Owner

`scraperx_sim`. The treadle is a real Jolt body on a real hinge, coupled to the existing valve lever by a real `PulleyConstraint` over two sheaves. Nothing about the linkage is scripted, flagged, or latched.

## Design decisions (stated, not left implicit)

- **Player mass is 85 kg**, an athletic climber with gear. This is a correction of an unset value, not a tuning preference; 602.92 kg contradicts frozen GDD text under any reading. The ripple was contained and is recorded below.
- **Player authority enters through leverage and placement, not mass.** An 85 kg body cannot move a 900 kg beam and should not be able to. It *can* work a pedal wired to valve gear that only needs ~190 N·m — a control built to human scale, which is what real plants have. The treadle is deliberately sited on the catwalk, so it is reachable only by someone the lift has already carried up (WO-009).
- **What the control buys is duration, not force.** The autonomous cycle crosses the catwalk mantle band (≈6.7–8.2 m) for a fraction of a second once every 26 s. A body on the pedal parks the lift at **7.7 m** — inside that band — for as long as it stands there. The player does not gain strength; they convert a pulsed machine into a held one. That is a change in *access*, exactly the GDD §16 chain.
- **Nothing latches.** Step off and the treadle returns under its own counterweight, the valve shuts, and the lift bleeds down. The route stays open for roughly four seconds after release, which is the window the player has to cross. The cost is a measured **3.10 MJ** drawn from the vessel versus an untouched plant at the same tick.
- **The cable is drawn where the constraint actually runs.** Presentation spans the two rendered cable segments between the same sheave points the native `PulleyConstraint` uses, so what the player sees crossing the yard is the linkage, not decoration. This is what makes the pedal's purpose legible from the catwalk (GDD §16).

## Forbidden shortcuts

- Restoring the 602.92 kg player to keep a green test.
- A latch, flag, or timer that keeps the valve open after the player leaves.
- Any capability the treadle grants while nobody is standing on it.
- Retuning the tipper so a human can shift it — that would re-grant industrial strength by the back door and would break the autonomous reset the seven-cycle WO-006 proof depends on.

## Proof path

1. Host tests prove an unaided 85 kg body does **not** move the tipper and does **not** open the valve — the GDD §17 guard, which fails the moment anyone hands the player freight-scale mass again.
2. Host tests prove the treadle grants nothing at rest: it sits against its stop with the valve shut.
3. Host tests prove a body on the treadle swings it, opens the real valve, drives the real piston, and parks the lift inside the catwalk mantle band.
4. Host tests prove causation against a control plant: an untouched plant at the same tick has the valve shut and the lift parked, so the lift is the player's doing and not the cycle's.
5. Host tests prove the cost: the held-open valve leaves the store measurably below the untouched plant's.
6. Host tests prove the capability is spent, not kept: stepping off returns the treadle, shuts the valve, and the lift falls back out of the band on its own.
7. Every WO-001..009 assertion is unchanged, and the Godot runtime reproduces every WO-004..009 proof line.

## Completion

Complete when all proof steps pass from one source commit and no existing assertion regresses.

## Result record

- **Changed:** `src/sim/simulation.{hpp,cpp}` (85 kg player mass override; `kTreadleEntityId`; treadle body, pylon, two sheave masts, hinge and `PulleyConstraint` cable; treadle as a moving support; treadle in the checkpoint; `treadle_angle_radians`; `InitialSpawn::CatwalkTreadle`; **`create_constraint` body-locking fix**); `src/bridge/scraperx_simulation.{hpp,cpp}` (`get_treadle_angle_radians`, spawn count 13→14); `tests/simulation_tests.cpp` (the WO-010 falsifier, plus two WO-006 assertions rewritten and one WO-008 choreography repair); `godot/presentation/main.gd` (treadle, masts, live cable spans, HUD status).
- **Built:** host and bridge configurations, Release, GCC 13.3, `-Wall -Wextra -Wpedantic -Werror`, clean.
- **Executed:** `./build/host/scraperx_sim_tests`; `ctest --test-dir build/bridge`; the Godot runtime under Xvfb at Fold aspect with the standard `--ci --capture=` sequence.
- **Observed:** `PASS scraperx_sim player is the plant's missing component: rest_valve=0 held_treadle=0.135899 held_valve=0.776894 held_lift=7.7m idle_lift=1.2m store_spent_MJ=3.10307`. The treadle grants nothing unoccupied; an 85 kg body swings it 0.136 rad, opens the valve to 0.78, and parks the lift at 7.7 m against an untouched plant's 1.2 m at the same tick, for 3.10 MJ of real store. Releasing it returns the pedal, shuts the valve within ~2 s, holds the lift above 6.7 m for ~4 s more, then drops it out of the band. Every prior proof line is unchanged: `lift=8.80227m`, `shut_flow=0.00000`, `lethal_impact=34.6619`, `machine_restored=1`, and the Godot runtime still reports `viewport 2160x1856 aspect=1.164`, `tower_height=1600`, `peak_valve=0.73 peak_lift=7.68 shut_flow=0.00000`.
- **Defects found and fixed:** two, both latent and both found by measuring rather than assuming.
  1. `src/sim/simulation.cpp` — the player capsule had no mass override and massed 602.92 kg, which silently made the player the heaviest object in any mechanism they stood on and made two WO-006 assertions pass for the wrong reason. Fixed; the assertions were rewritten to state the true physics rather than restored to green.
  2. `src/sim/simulation.cpp:create_constraint` — it took two sequential `BodyLockWrite` locks. Jolt stripes body mutexes across a fixed-size array, so two distinct bodies can share one and the second lock throws `Resource deadlock avoided`. Every constraint pair built since WO-006 had simply not collided; the treadle's pylon/plate pair did. Fixed with `BodyLockMultiWrite`, which sorts and dedupes. This would have struck any future body pair at random.
- **Regressions:** none outstanding. One WO-008 test choreography was repaired rather than regressed: it walked the player onto the tipper and never stopped the walk input, so at 85 kg the player strode off the far side and was airborne when the jump fired. It only stayed put before because a 602.9 kg body sank the beam and wedged there, which was never the behaviour under test.
- **Unverified boundary:** the treadle has not been operated by hand in an interactive session — its proof is the deterministic host simulation, as with every prior architectural claim, and the Godot run confirms only that nothing regressed. The step onto the plate is 0.44 m, inside the mantle band, so it is a mantle rather than a walk-on; that was forced by geometry (the plate needs its full throw to clear the deck) and has not been assessed for feel. Player mass is now a pinned, load-bearing constant at 85 kg and any future change to it must re-run this suite. Android install/execution, Fold 6 panel observation, touch ergonomics and sustained frame rate remain unverified — no device access.
