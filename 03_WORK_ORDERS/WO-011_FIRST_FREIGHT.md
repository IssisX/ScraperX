# Work Order 011 — First Freight (KX-JIB / KX-CRATE)

**Note on numbering:** this is Ascent Atlas v1.0's kernel content for "Work Order 005 — first freight mechanism" (`WO-005_FIRST_FREIGHT.md` in the v1.1 build package). It is renumbered WO-011 because repo WO-005..010 already cover other, earlier objectives committed before the atlas existed. Per the absorption commit (`39b2c87`): the Kellerworks machine already built (hoist/tipper/valve/lift/catwalk/treadle) is band-scale content that does not match the atlas's literal kernel, so this work order builds the atlas's kernel as new, separate, correctly-scoped work rather than retrofitting the existing machine.

## Objective

One real machine moves one real load under finite limits, in the atlas's bounded kernel volume, not the Kellerworks yard.

Atlas kernel (§9): `KX-JIB` actuates; `KX-CRATE` is the load; a pendant issues Drive/Raise/Lower/Brake commands. Machine success is native mechanism state, not animation.

## Existing proven truth

The Kellerworks machine (repo WO-006) proved an autonomous, non-interactive cycling mechanism. Nothing in the repo before this WO gave the player direct, commanded authority over a machine's motion — the treadle (WO-010) opens a valve by weight alone, with no continuous player-driven axis. WO-002's moving-support law and WO-003's traversal probes are real and general, and both are reused here unchanged.

## Governing authority

- Ascent Atlas v1.0 §§4, 9 (`KX-JIB`, `KX-CRATE`, `KX-BELT` may remain as support), §0.3 (masses/forces below this line are design targets, not proof requirements, until a benchmark scene falsifies them — this WO is that scene).
- `03_WORK_ORDERS/WO-005_FIRST_FREIGHT.md` in the v1.1 build package (Laws 11, 12, 22, 24, 26, 27; GDD §§11, 15; TDD §§9, 19.1 item 2).
- Governing Law 24 — machinery cannot create capability from nothing.
- Governing Law 26 — no physics theater / no second authority.

## Owner

Native freight/machinery. Jolt owns contact of hook/load/deck and the constraint motors that bound every actuator. Presentation reads positions and a boom angle; it does not decide motion.

## Allowed seam

One mechanism definition: mast, slewing boom, hoist winch, hook, crate, hook-to-crate pin. Godot exposes Drive/Raise/Lower only while the player is within the pendant station radius.

## Design decisions (stated, not left implicit)

- **The kernel is sited at a separate location (x≈200), not inside the Kellerworks yard.** Atlas §9 frames the kernel as its own bounded proof volume ("z=0–24 m, plan cut 36 m × 36 m"), explicitly prior to and separate from band content. Keeping it spatially distinct avoids conflating the two, and avoids retrofitting geometry into an already-built, differently-scaled yard.
- **Finite actuator effort comes from real Jolt constraint motors, not application-level force caps.** The slew hinge and hoist slider both carry `EMotorState::Velocity` motors with `SetTorqueLimit`/`SetForceLimit`. Jolt's own solver enforces the bound — exceeding it does not snap to the commanded target, the body simply cannot reach it. This is Governing Law 26 in a specific, checkable form: the "no unlimited winch force" requirement is enforced by the engine, not asserted in a comment.
- **The hook is pre-rigged to the crate via a real `PointConstraint`.** WO-005's own text permits this ("`CAP-HOOK5` may be pre-placed on the crate for this WO, but the hook must still be a real constraint"). A point constraint fixes one shared pin but leaves rotation free, so the crate genuinely swings under the hook rather than being welded to it — out of scope was a full proximity-based attach/detach system, which the WO text does not ask for.
- **A separate, permanently-overweight capacity-proving stand shares the jib's exact rated force.** Proving "unlimited winch force is forbidden" on the real jib would mean staging an unsafe lift on the one mechanism the player operates. Instead a second, small, always-visible stand — same motor rating, a load past it, continuously commanded to raise — proves the rating is real without ever putting the working jib in an unsafe state. It is not an atlas-named module; it is proof scaffolding, and is documented as such rather than presented as a kernel entity.
- **Command axes are continuous and persistent, matching `set_move_input`.** `set_jib_slew_input`/`set_jib_hoist_input` behave like a held joystick axis, not a one-shot event (unlike `request_jump`). Zero on either axis is the brake — a velocity motor targeting zero, holding against gravity up to its rated force — not "let go and free-fall." This gives Drive, Raise, Lower, and Brake as two signed axes rather than four separate commands, with no loss of the named verbs.
- **Commands only take effect within the pendant station radius.** Matches WO-005's "Action to enter station." Away from the station, both motor targets are forced to zero every tick regardless of a still-held command, so walking away always safely brakes the jib rather than leaving it obeying a stale order.

## Forbidden shortcuts (checked against)

- AnimationPlayer moving the crate — no animation exists; every mesh reads a native transform every tick.
- Unlimited winch force — the winch and slew motors both carry a finite, measured `mMaxForceLimit`/`mMaxTorqueLimit`; the capacity stand falsifies this directly.
- Teleporting the load to a solved pose — the crate only moves through the real hook pin and the real hoist slider.
- Skipping attachment compatibility — the hook-to-crate link is a real `PointConstraint`, not a parented transform.
- One-button "solve hoist" — there is no such action; Raise/Lower/Drive are continuous, held commands.

## Proof path

1. Host tests prove the player is recognised at the pendant station and that, with no command issued, the hook holds rather than drifting.
2. Host tests prove a sustained raise command genuinely lifts the hook and crate through the finite-force motor, and that the mechanism stops at its real travel limit rather than continuing under an unlimited force.
3. Host tests prove a locked brake holds position rather than continuing to lift — the brake is not a second, unbounded power source.
4. Host tests prove a sustained drive command genuinely slews the boom to its real limit, carrying the crate to a measurably different position, still pinned under the hook.
5. Host tests prove leaving the station radius stops any further climb on a still-held raise command, once the real transit time to clear the radius is accounted for.
6. Host tests prove the crate is a real moving support the player can stand on (WO-002 law, WO-005 proof-path item 3).
7. Host tests prove the capacity-proving stand — same rated force, permanently overweight, continuously commanded to raise — never rises.
8. The Godot runtime reproduces every WO-004..010 proof line unchanged, and the kernel geometry renders correctly from a manual vista capture.

## Completion

- The crate moves because `KX-JIB` did bounded work.
- Controls are local and non-magical.
- Limits are observable (stall, stop, hold).

Stop. Needle seating (`KX-NEEDLE`/`KX-POCKETS`) belongs to WO-012.

## Result record

- **Changed:** `src/sim/simulation.{hpp,cpp}` (kernel entity IDs `kJibMastEntityId`..`kCapacityStandEntityId`; `InitialSpawn::KernelJibStation`/`KernelCrateTop`; `add_vertical_hinge`/`add_motorized_slider`/`add_point_link` helpers; `build_kernel_jib`; `update_jib`; `set_jib_slew_input`/`set_jib_hoist_input`; jib/crate added to `entity_is_moving_support`; jib boom/hook/crate added to the checkpoint commit/restore blob); `src/bridge/scraperx_simulation.{hpp,cpp}` (nine new accessors/setters; spawn count 14→16); `tests/simulation_tests.cpp` (the WO-011 falsifier); `godot/presentation/main.gd` (kernel mesh construction, render mirror, arrow-key pendant input, JIB HUD line, status text); `godot/main.tscn` (new `Jib` HUD label).
- **Built:** host and bridge configurations, Release, GCC 13.3, `-Wall -Wextra -Wpedantic -Werror`, clean.
- **Executed:** `./build/host/scraperx_sim_tests`; `ctest --test-dir build/bridge`; the Godot runtime under Xvfb at Fold aspect with the standard `--ci --capture=` sequence; one manual vista capture (temporary camera-pose override only, reverted before commit — `git diff` confirmed clean against the committed values).
- **Observed:** `PASS scraperx_sim first freight: station=1 raised_y=4.76117 braked_delta=-0.00368643 slewed_rad=2.00023 crate_carried=1 rides_crate=1 capacity_held=1`. The player is recognised at the pendant; a sustained raise lifts the hook to 4.76 m and stops there (real travel limit); a locked brake holds within 0.004 m over two seconds; a sustained drive slews the boom to its 2.0 rad limit, carrying the crate more than 5 m from its rest position while staying pinned under the hook; leaving the station stops further climb on a still-held command; the player can stand on the crate as a real support; the capacity stand — rated identically to the jib's winch, permanently loaded past that rating — never rose over six seconds of continuous raise command. One real tuning defect was caught and fixed before this passed: the slew motor's first torque value (6000 N·m, an unmeasured guess) produced 0.006 rad/s against a target of 0.5 rad/s — the boom+crate's moment of inertia at the tip radius (~31,600 kg·m²) needed far more; corrected to 45,000 N·m and reverified. The Godot runtime reproduced every WO-004..010 proof line unchanged: `viewport 2160x1856 aspect=1.164`, `tower_height=1600`, `peak_valve=0.73 peak_lift≈7.4-7.9 shut_flow=0.00000`; a manual vista capture shows the mast, slewed boom, hook, and crate rendering correctly, with the Kellerworks tower visible at distance confirming the two areas coexist without overlapping.
- **Unverified boundary:** interactive desktop/Fold operation of the pendant (arrow keys are wired but not hand-tested in a live session — the proof is the deterministic host simulation, matching every prior architectural claim in this project); Android install/execution, Fold 6 panel observation, touch ergonomics and sustained frame rate remain unverified as before (no device access); a touch-control affordance for the pendant (arrow keys only — no on-screen buttons added, since this WO's scope is the mechanism, not the full control surface); the capacity stand's exact rated force (20,000 N) and the crate's mass (1200 kg) are design targets per atlas §0.3, chosen to be physically coherent and clearly inside/outside the rating respectively, not derived from a real-world 5 t crane specification.
- **Regressions:** none observed. Every prior host-test assertion and Godot runtime proof line passed unchanged after this work order's changes.
