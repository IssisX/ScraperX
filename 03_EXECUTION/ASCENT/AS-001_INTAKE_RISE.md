# SCRAPERX — AS-001 INTAKE RISE (~0 m TO +24 m)

**Ascent Slice:** `AS-001`
**Lifecycle:** `IMPLEMENTED` — source present on `ScraperX-Claude`
**Provenance:** re-derived against this branch's real source
**Implementation gate:** none — already implemented
**Evidence:** see the ledger in `00_START_HERE.md`. Lifecycle never implies proof.
**Depends on:** the kernel slice (WO-000–WO-003, WO-008–WO-009, WO-011–WO-013) proven on `ScraperX-Claude`

> **Provenance.** Adopted from `ScraperX-Grok`, where this slice was authored as `WO-009_INTAKE_RISE`.
> Ticket numbers remapped per `03_EXECUTION/PLANNING/ASCENT_PRE_RESOLUTION.md` §5.1. Geometry, ratings and
> falsifiers carry over as **DESIGN TARGET**: they were never proven in source on that branch
> (it has not compiled since 2026-09-20). Positions are re-sited against this branch's tower
> at source `(0, —, -150)`.

## Objective

Author the first real campaign slice of `B00 - Apron and Intake`, not another kernel fixture and not the whole 0–40 m band.

Required modules as real world content:

```text
MOD-APRON
  → MOD-INTAKE-BELT
  → pendant access
  → MOD-YARD-JIB
  → 4 t crate pack
  → MOD-DOG-A
  → physically moving gate/landing mechanism
  → newly traversable MOD-STAIR-A
  → stable player support around +24 m
     with the unfinished +40 m continuation visibly above
```

## Existing truth

The kernel is in source on `ScraperX-Claude` and green. Observed 2026-09-21,
`build/host/scraperx_sim_tests`, exit 0, 9 of 9 `PASS`:

```
PASS scraperx_sim moving-support truth
PASS scraperx_sim athletic traversal
PASS scraperx_sim player is the plant's missing component
PASS scraperx_sim first full causal chain
PASS scraperx_sim coupled machine
PASS scraperx_sim fall/parachute/checkpoint
PASS scraperx_sim first freight
PASS scraperx_sim first structural coupling
PASS scraperx_sim first process coupling
```

Kernel `KX-JIB`, `KX-NEEDLE`, `KX-SUMP` sit at source x ≈ 200 and remain regression substrate;
this slice does not move or retitle them. The tower frame is at source `(0, —, -150)`,
half-extent 26 m, and is presentation-plus-collision only — no campaign machine is sited on it
yet. Fold-device execution is not proven. Android APK export is proven by CI on this branch.

The claim on `ScraperX-Grok` that this slice was already "DONE in source, proven by Actions run
`35454049354`" does not hold: that run is the kernel causal-chain run at `b90c5911`, and it
predates every campaign commit. See `03_EXECUTION/PLANNING/ASCENT_PRE_RESOLUTION.md` §7.

## Authority

- Laws 2–6, 11–17, 22–27, 29, 32–36
- GDD freight / traversal / sequence-break sections
- Atlas §§4, 6 (B00), 7 (K0 Intake), 12
- TDD: native 90 Hz / Jolt owns contact; Godot mirrors
- Execution Protocol §§4–7, 12

## Owner

Native 90 Hz C++/Jolt simulation. Godot presents authoritative state. Mission/UI may observe predicates only.

## Allowed seam

New B00 spawn/world using kernel primitives (moving support, finite machine, distance-constraint hook, kinematic travel, contact-ranked support). Do not retitle `KX-JIB` as `MOD-YARD-JIB`.

## Required causal path

```text
ACT[ride/walk belt → enter CAP-PENDANT → Drive/Raise/Lower/Brake]
  → STATE[finite actuator torque/speed/brake/travel; crate pose from Jolt]
  → WORLD[crate leaves MOD-DOG-A pin envelope; dog body travels]
  → PLAY[MOD-STAIR-A throat is physically open; walk to +24 m]

alternate: ACT[climb MOD-SKIN-LADDER-S] → PLAY[+24 m] with freight state unchanged
```

Pre-resolved mechanism (authoritative numbers):

- `MOD-YARD-JIB`: 12 m boom, 5 t SWL. Rated slew moment `τ = r × F = 12 × 5000 × 9.81 = 588600 N·m`. Rated winch force `5000 × 9.81 = 49050 N`.
- 4 t pack: `F = 39240 N`. At working radius ~11.1 m, `τ ≈ 436000 N·m` — inside rating, slower than empty. 9 t pack exceeds both force and moment and must stall.
- Hook is a real distance constraint. No teleporting load. Brake holds. Travel limits stop the winch/slew.
- `MOD-DOG-A` is a kinematic hinged gate whose travel stalls while the crate body occupies the latch envelope. Stair accessibility is the dog body's collision pose, never `crate_moved` / `gate_open` / mission flags / animation / timers / collision toggles.
- `MOD-INTAKE-BELT`: 18 m stroke kinematic slat deck. Support-point velocity and inherited momentum are law. Riding is legal.
- 4 t cannot be shoved by the unaided player (atlas unaided range 200–400 kg).

## Forbidden shortcuts

- renaming `KX-*` objects and calling that B00
- `crate_moved` / `gate_open` / mission flags / animation events as causality
- unlimited-force or teleporting hoist
- blocking `MOD-SKIN-LADDER-S` to protect the freight sequence
- building the rest of B00, B01, or new process systems
- neon, filler scaffold forest, toy mechanism arena, decorative dead machinery
- claiming Fold-device execution

## Implementation scope

Native B00 world + falsifier tests; Godot presentation twins; CI two Fold-aspect screenshots; Android arm64 APK with `libscraperx_native.so`.

## Out of scope

B00 +24–40 m completion, B01–B11, NPCs, new process graphs, Fold 45 FPS certification.

## Proof path

1. Native deterministic causal/falsifier tests.
2. Fold-aspect 1080×928 grade-level screenshot: tower reads as a giant machine.
3. Second screenshot: intake mechanism and traversal consequence.
4. Godot runtime using authoritative native state.
5. Android arm64 APK contains `libscraperx_native.so`.
6. The kernel falsifiers (WO-000–WO-003, WO-008–WO-009, WO-011–WO-013) remain green.

## Completion

- before the crate is removed, the main stair route is physically impassable
- moving only a mission/presentation variable cannot open it
- an overloaded or out-of-travel jib cannot solve the mechanism
- after the crate physically clears the dog, the mechanism travels and the main route becomes physically passable
- the SKIN bypass works without falsely mutating the freight mechanism
- player has stable support around +24 m; +40 m continuation is visible and unfinished
- Fold install / on-device play remain unverified

Stop at the first stable ~+24 m handoff.


---

## Result record

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

## 2026-09-25 B00 ground-water route amendment

The historical AS-001 result record above remains evidence for the crate → gate → stair route and is
not rewritten. Current B00 product authority additionally contains `MOD-WATER-SCREW`,
`MOD-WATER-TANK`, and the separately sequenced `MOD-WATER-LIFT`.

The screw is a ground-level causal input mechanism: finite shaft work moves conserved basin water to
the +5..+5.5 m tank. Its output contract is 2.0 m³ / 2000 kg measured in that tank. The water lift is
a later mechanism cycle and may not be represented by a flag or implied by the screw's completion.
Existing AS-001 freight/stair and outside-climb routes remain legal.

The dock-to-stair continuation is a short fixed transfer span supported by
the existing +8 m dock and `MOD-STAIR-A`'s +8 m landing, with a solid
0.70 m transverse brace to jump. The direct walk previously left the dock
over a 4.35 m unsupported opening and fell to the lower flight; a timed
jump could clear it, but there was no continuous structural handoff. Native
lift-to-dock-to-span-to-stair traversal now reaches the stable +12 m landing
without changing movement rules or the grade-level dog. This is an
additional entrance to the stair above the dog, not a retroactive change to
the historical crate/gate evidence or a completed +40 m exit.
