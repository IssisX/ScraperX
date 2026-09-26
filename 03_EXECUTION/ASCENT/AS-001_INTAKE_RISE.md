# SCRAPERX — AS-001 INTAKE RISE

**Ascent Slice:** `AS-001`
**Lifecycle:** IMPLEMENTED
**Provenance:** reconciled against `ChatGPT` source at `7e66eb6` on 2026-09-26.
**Implementation gate:** maintenance only; reproduce a relevant runtime failure before repair.
**Evidence:** `00_START_HERE.md` §§2, 7. Historical diagnosis below is dated evidence, not current instructions.
**Write branch:** `ChatGPT`.

## Objective and authority

Preserve the complete apron-to-+24 m intake route and its legal alternatives. Laws 4–5,
9, 17, 22, 24–26 govern contacts, recovery, ownership and finite work; GDD §§7, 9, 11,
14–17 and Atlas B00/K0 govern the route. The new water entrance is an alternate into
this existing stair, not a requirement to reoperate the crane.

## Existing truth and owner

`src/sim/simulation.cpp` owns `build_intake_rise`, `update_intake`, the `kIntake*`
constants, rigid bodies, constraints and player support. Godot presents that state;
static world additions follow `solid_export.gd` → `world_solids.inc`. The six-switchback
stair, outside ledges and independent tower stair all exist. The tower stair reaches
154 m; the gate does not control every way up the tower.

## Mechanical close — work backward from the receiving support

All coordinates below are source `(x,y,z)`, Y up. See Atlas §2 for the coordinate map.
Heights refer to walking surfaces unless identified as body centres.

| Required result | Immediate physical cause | Producer / input | Boundary |
|---|---|---|---|
| Stable +24.1872 m handoff | Static deck, x [-10,-2], z [-117.5,-107.7] | Six 4 m rises from the intake stair | `kIntakeHandoffEntityId`; AS-002 receives this support, not the player's centre at +25.0872 m |
| Walkable approach to that deck | Inclined slabs with 14 m horizontal run per 4 m rise, alternating lanes and flush landings | Gate opens the grade throat, or player joins above it | Fixed support; no velocity imparted by a stationary surface |
| Grade throat opens | 900 kg dynamic dog rotates under finite 12000 N·m drive toward 1.45 rad, 0.62 rad/s command | Crate collision ceases to obstruct its sweep | No `crate_moved` permission; solid obstructions still obstruct |
| Crate clears sweep | Real crane hook/pack attachment carries the 4000 kg pack | 49050 N hoist, 0.85 m/s command, 12 m boom at 11.5 m | 9000 kg overload exceeds hoist force; machine cannot lift it by command alone |
| Player can operate crane | Reach the station from grade via ramp/catwalk; local control request accepted | Finite hoist and slew controls in `update_intake` | No portable pendant acquisition gate currently exists |

The crane slew torque limit is 588600 N·m, command 0.22 rad/s, travel ±0.90 rad.
`5000 × 9.81 = 49050 N` sizes the hoist. Although source sets the slew limit using
`12 × 5000 × 9.81`, this is a configured rating, **not** a derivation of gravitational
resisting torque about a vertical slew axis. Gravity creates boom bending; dynamic
slew inertia and bearing resistance need their own load case. Do not infer a full
structural or brake certification from the overload test.

### Alternate inputs and outputs

- Outside climb: 15 real ledges, 1.60 m rise and 2.00 m depth, reach the same +24 m
  handoff using the existing mantle controller, freight untouched. This is not a
  vertical ladder verb. AS-002 adds five more ledges to +32 m and a walkway/upper flight.
- Water entrance: `GROUND_WATER_ASCENT.md` owns screw → tank → caught bucket → cage.
  The caught cage reaches a fixed dock at +8.25 m. The supported grating joins the
  existing +8 m stair landing through a 0.70 m solid brace jump, then the stair reaches
  +12.1872 m. This bypasses the grade dog physically. It does not deploy AS-002's
  swinging flight or relocate the freight pack.
- The intake belt is a level translating deck: centre z = -96 + 9 sin(0.40t), top
  y = 1.38 m. The pack starts at x = 0, z = -112.2, outside the belt's x = 4..8 span.
  It is not being conveyed into the gate. Riding it serves AS-003's approach.

### Contacts, failure and recovery

The gate's failed grade approach and its cleared stair traversal must be tested at
that throat; a valid outside/tower/water bypass must never fail a global
“unsolved cannot ascend” assertion. Preserve support-point motion on the belt,
crane and other moving bodies. Do not globally retune movement to repair a local
stair lip or capsule clearance.

Missed lower transitions leave the player on actual lower structure/apron, subject
to impact and parachute rules, not guaranteed survival. An obstructed crane or lost
pack is a physical state: alternate routes remain legal, but they do not deliver
that pack to AS-002's cradle. Do not promise every dropped load can be recovered.

### Checkpoint boundary

`MachineCheckpoint` currently omits the intake pack, hook, dog and crane states,
while later ticket bodies and attachment state are captured. This is an existing
Law 9/20 continuation gap, not proof of complete B00 restoration. AS-002 must not
claim a coherently restored loaded cradle from its own body snapshot alone.
No serialized save-file version is established here. TDD §14 is the required
persistence contract; the present in-memory implementation does not complete it.

## Proof path and completion

Existing `tests/simulation_tests.cpp` groups exercise pinned throat, off-station
commands, overload, six-flight ascent and the 15-mantle alternative. The water
lift group also exercises the new dock-to-+12 seam. Preserve those real paths,
nearby machine contacts and the shipping proof path when modifying implementation.
A proof spawn is evidence for its exercised segment, not an apron-to-summit playthrough.

This slice ends at stable +24.1872 m support. Stop; no automatic next implementation.

## Historical implementation record

The following is preserved from the pre-audit ticket. Its measurements describe
that execution, not current constants, lifecycle, exclusive routes or future work.
The active contract above supersedes its planning assignments and broad completion claims.

<details>
<summary>Original implementation observations and causal diagnoses</summary>

- **Changed:** `src/sim/simulation.{hpp,cpp}` (entity IDs 35–46 `kIntakeApronEntityId`..`kIntakeSkinEntityId`; `InitialSpawn::IntakePendant`/`IntakeThroat`/`IntakeSkinFoot` = 18–20; the `kIntake*` constant block; `build_intake_rise`; `update_intake`; MOD-INTAKE-BELT stroke in `update_support_motion`; belt/hook/pack/dog added to `entity_is_moving_support`; `StepCommands::intake_slew_input`/`intake_hoist_input`; `Simulation::set_intake_slew_input`/`set_intake_hoist_input`; eight new `Snapshot` fields); `src/bridge/scraperx_simulation.{hpp,cpp}` (ten accessors/setters, spawn count 18→21); `tests/simulation_tests.cpp` (the AS-001 falsifier and a `walk_toward` helper that returns the deepest z reached); `godot/presentation/main.gd` (`_build_intake_rise`, render mirror, pendant input, `INTAKE` HUD line, and a `_layout_hud` fix); `godot/main.tscn` (`Intake` HUD label); `.github/workflows/wo000-delivery-spine.yml` (six proof greps on the new line).
- **Built:** host and bridge configurations, Release, GCC 13.3, `-Wall -Wextra -Wpedantic -Werror`, clean.
- **Executed:** `./build/host/scraperx_sim_tests` (exit 0); `ctest --test-dir build/bridge` (1/1 passed); Godot 4.7.stable under Xvfb at Fold aspect with `--rendering-method gl_compatibility` and the workflow's own `--ci --capture=` sequence.
- **Observed:**

  ```
  PASS scraperx_sim B00 intake rise: pinned_impassable=1 pinned_deepest_z=-110.517
    forced_impassable=1 overweight_y=0.899998 lifted_y=4.54117 dog_rad=1.20062
    handoff_y=25.0872 skin_mantles=15 skin_freight_untouched=1
  ```

  All nine prior falsifiers pass unchanged in the same run (10 of 10, exit 0). Every Godot proof line the workflow greps reproduces unchanged: `SCRAPERX_EXTENSION_LOADED api=4.7`, `width=2160 height=1856 aspect=1.164 stretch=expand`, `tower_height=1600`, `peak_valve=0.72 peak_lift=7.37 shut_flow=0.00000`.

  Reading the numbers: with the 4 t pack on the apron the player walking flat at the throat never gets their capsule past **z = -110.517**, which is still inside the bay wall's own 0.6 m band — MOD-STAIR-A is shut by a body, and the deepest point of the whole 14 s attempt is recorded, not just where the player ended up. Mashing every command in the game from off-station changes nothing. The 9 t proof load, permanently commanded up at the same 49050 N rated winch force, is still at **y = 0.90** after 20 s. Raising the 4 t pack to **4.54 m** clears the dog's swing, and the dog — under a permanent 12000 N·m opening torque it has had since build time — travels to **1.20 rad** and keeps going to its 1.45 rad limit. The player then walks all six switchback flights and finishes standing on `kIntakeHandoffEntityId` at **25.09 m** (feet on the 24.19 m surface). Separately, the SKIN braid climbs the same 24 m in **15 mantles** with the pack still on the ground and the dog still pinned.

- **Defects found and fixed by this work order's own falsifiers, none assumed away:**
  1. **The pack intersected MOD-STAIR-A's south lane.** The winch crept at 0.02 m/s against its rated 0.85 — the whole rig was wedged on a stair slab passing through the freight. Found by watching `pack_y` over 12 s of commanded hoist, not by reading the code. Fixed by moving the stair lanes north to z = -118.
  2. **MOD-DOG-A bound at ~0.31 rad and never travelled.** The hinge sits on the plate's own end face, so the plate's trailing corner sweeps a circle of its half-thickness — straight into the west jamb. Fixed by setting the plate back by its half-thickness plus clearance. The same class of defect as WO-012's pocket clearance: `JPH::BoxShape` carries a rounded convex radius, and exactly-coincident boundaries jam.
  3. **Consecutive stair flights in one lane pinch below standing height.** Two opposing flights sharing a lane meet in a V whose apex closes to under 1.80 m; the player climbed to the pinch and stopped. That is a stair that cannot be walked. Fixed with two lanes joined by a landing at each turn, and landings raised by the inclined slab's own surface offset so a turn is flush rather than a 0.19 m lip.
  4. **MOD-SKIN-LADDER-S was unclimbable as a staggered ledge line.** A mantle drops the player `kLandingInset` in from the rung's near edge; with 1.0 m staggered rungs the next rung's face was still inside probe reach from there, so the player auto-grabbed a hang the instant they landed, never became grounded, and cycled mantle-hang-fall making no height. Fixed by making rungs 2.00 m deep in the climb direction and stepping them straight, which leaves 1.53 m of stand — outside the 1.30 m reach. The constraint is now written down beside the constants.
  5. **The HUD squeezed thirteen readouts into room for eight.** `_layout_hud` scaled only the first eight labels and the VBox kept its authored height, so the machine lines overlapped into an unreadable stack. Found by looking at the rendered frame. Fixed by scaling every readout and sizing the box to its content.

- **Deviations from the adopted plan, stated rather than absorbed:** the plan's `WO-009` geometry table places B00 at a bare origin with its own numbers (`kB00StairDY = 0.353` treads, an 18 m belt along z, a mast at `(-6.50, —, 8.00)`). This branch already has a tower at source `(0, —, -150)`, so B00 is re-sited against that tower's south face and the positions are re-derived; ratings, strokes, masses and moments carry over unchanged (12 m boom, 11.50 m boom height, 5 t SWL, `τ = 588600 N·m`, `F = 49050 N`, 0.85 m/s hoist, 0.22 rad/s slew, ±0.90 rad, 18 m belt stroke, ω = 0.40, 1.45 rad dog retract at 0.62 rad/s). MOD-STAIR-A is built as six inclined switchback slabs rather than 68 discrete treads, matching `build_stack`'s existing house pattern. MOD-SKIN-LADDER-S is a stepped ledge line climbing north up the apron rather than a facade rung line, for the reason in defect 4.

- **Unverified boundary:** interactive desktop/Fold operation of the yard-jib pendant (it shares the kernel jib's Raise/Lower/Slew axes, wired and exercised headlessly but not hand-tested in a live session). Android install/execution, Fold 6 panel observation, touch ergonomics and sustained frame rate remain unverified — no device access. **`CAP-PENDANT` is not gated in this slice:** the Atlas has the pendant cold until the belt catwalk is reachable, and this branch makes the catwalk reachable from grade by a ramp, so "reach the pendant" is not yet a puzzle. That is `AS-003`'s capability work, not this slice's. **Riding MOD-INTAKE-BELT is legal but not separately falsified here** — the belt is kinematic and in the moving-support set, so the WO-002 support-point law already covers it; a dedicated ride falsifier is left to the slice that makes riding it necessary. The apron is the ground plane rather than a distinct `MOD-APRON` slab, since the existing 480 m ground already reaches the tower base. No persist fields were added: nothing in this slice has state that outlives its bodies.

- **Regressions:** none observed. All nine prior native falsifiers and every Godot runtime proof line passed unchanged after this work order's changes.


---

</details>
