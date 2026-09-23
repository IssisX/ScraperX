# SCRAPERX — AS-002 LEGAL FORTY (+24.19 m TO FIRST STAND AT +40.19 m)

**Ascent Slice:** `AS-002`
**Lifecycle:** `IMPLEMENTED` — falsifier green, source and tests in `claude/android-game-dev-continue-m045pq`
**Provenance:** re-derived against this branch's real `AS-001` exit state; built against that derivation, with deviations recorded in the Result record below where the solver disagreed with the quasi-static design calc
**Implementation gate:** `AS-001` implemented and its falsifier green. **Satisfied.**
**Evidence:**

```
PASS scraperx_sim AS-002 Legal Forty: unrouted_deepest_y=25.0872 flag_hinge=0
  cradle_drop=1.49949 deployed_travel=0.863122 mid_landing_y=33.0872
  forty_y=41.0872 checkpoint_commits=22787 checkpoint_y=41.0872
  retracted_travel=0.196974 skin_forty_mantles=20 skin_forty_y=41.0872
  jib_capped_hook_y=11.2325
```
**Depends on:** `AS-001_INTAKE_RISE.md` in source and green; kernel falsifiers
(`WO-000`–`WO-003`, `WO-008`–`WO-009`, `WO-011`–`WO-013`) green; protocol
`03_EXECUTION/PLANNING/ASCENT_PRE_RESOLUTION.md` §8.

> **Authoring note.** This file replaces the plan adopted from `ScraperX-Grok`
> (their `WO-010_LEGAL_FORTY`). That version was not portable: its entire
> "Existing truth" section quotes geometry that does not exist here — a handoff
> at source `(10.40, 24.00, 38.40)`, 68 discrete STAIR-A treads at `x = 10.40`,
> SKIN at `x = -17.00`, `InitialSpawn::IntakeHandoff`. This branch's `AS-001`
> built a switchback stair in a walled bay, a handoff deck at `x = -6`, and a
> stepped SKIN line at `x = -6`. A job ticket whose inherited constants are all
> false is not a job ticket. The one thing carried across unchanged is its
> finding that the atlas's third B00 exit does not close — independently
> re-derived in §8.4 below, with a stronger reason than reach.

## Objective

Close Atlas §7 **K0 PLAY**: the player first stands with stable support at
`z ≥ 40 m` and the automatic checkpoint commits there.

This is the leftover 24–40 m of Atlas band `B00 — Apron and Intake`. It is not
`MOD-HOOK5-RACK` (`AS-003`), and it is not band B01's needles (`AS-004`).

The slice owns **one mechanism**: `MOD-STAIR-A`'s lower upper-flight is a
counterweighted bascule that hangs uselessly in the bay until the yard jib
loads its counterweight. That is the first time in the campaign that the
player's own climb is paid for by a mass balance rather than by a gate.

## Existing truth

Observed on `ScraperX-Claude` at `af8beca`, CI run `35651140478` green:

```
PASS scraperx_sim B00 intake rise: pinned_impassable=1 pinned_deepest_z=-110.517
  forced_impassable=1 overweight_y=0.899998 lifted_y=4.54117 dog_rad=1.20062
  handoff_y=25.0872 skin_mantles=15 skin_freight_untouched=1
```

Frozen source this file consumes (`src/sim/simulation.cpp`, verbatim):

| Constant | Value | Meaning |
|---|---|---|
| `kIntakeHandoffY` | `24.0` | handoff deck datum |
| tread surface offset | `0.1872` | `kIntakeStairSlabHalfY / cos(pitch)`, pitch `15.95°` |
| — handoff **walking surface** | **`24.1872`** | derived; the player's feet stand here |
| `kIntakeHandoffCenterX` / `HalfX` | `-6.0` / `4.0` | deck spans `x ∈ [-10, -2]` |
| `kIntakeHandoffSouthZ` / `NorthZ` | `-107.7` / `-117.5` | deck z span |
| `kIntakeBayHalfX` | `10.0` | bay `x ∈ [-10, 10]`, inner faces `±9.7` |
| `kIntakeBayFrontZ` / `BackZ` | `-110.5` / `-122.5` | front wall occupies `z ∈ [-110.8, -110.2]` |
| `kIntakeStairZ` / `LaneOffset` | `-118.0` / `2.0` | 0–24 lanes at `z = -116` and `-120`, half-width `1.8` |
| `kIntakeJibMastZ` | `-100.2` | `MOD-YARD-JIB` mast at `x = 0` |
| `kIntakeBoomLength` / `BoomHeight` | `12.0` / `11.50` | **frozen**; do not lengthen |
| `kIntakeJibSlewLimitRadians` | `0.90` | slew about the `-Z` bearing |
| `kIntakeJibWinchForceN` | `49050` | `5000 × 9.81` |
| `kIntakePackMassKg` | `4000` | weight `39240 N` |
| hook travel | `y ∈ [2.0, 11.05]` | `hoist_travel = (11.50 - 0.45) - 2.0 = 9.05` |
| `kIntakeSkinRungRise` | `1.6` | SKIN head rung tops at `24.0` |
| `kIntakeStationRadius` | `3.40` | `CAP-PENDANT` at `(3.0, ~1.4, -100.0)` |
| `kSupportNormalThreshold` | `0.55` | a surface is support up to `acos(0.55) = 56.63°` |
| `kMantleMaximumRise` / `kMantleMinimumRise` | `1.85` / `0.35` | |
| `kLedgeTopNormalThreshold` | `0.7` | a mantle landing must be `≤ 45.6°` |
| `kPlayerMassKg` / radius / half-height | `85.0` / `0.35` / `0.90` | |
| `kLethalImpactSpeedMps` | `20.0` | a 40 m fall arrives at `28.0 m/s` — lethal |

**The pack is pre-slung and cannot currently be let go.** `build_intake_rise`
rigs it to the hook with `add_point_link`, a `JPH::PointConstraint`, which is
bidirectional — it carries compression as readily as tension. A pack lowered
into a cradle with the winch slacked is still held by that constraint; its
weight does **not** transfer. This slice must therefore author a real release,
and that release is the hinge of its causal path.

Entities `1`–`46` and spawns `0`–`20` are taken. Fold-device execution is not
proven. Kernel `KX-*` IDs at `x ≈ 200` remain regression substrate and are not
retitled here.

## Authority

- Governing Laws 2–7, 9–10, 12–18, 21–27, 29, 32
- GDD §§3, 4, 6, 7.2–7.4, 9, 11, 16, 17, 23, 24
- Atlas §§2, 3, 5, 6 (band B00 exit states), 7 (K0), 8.1, 8.3, 12, 13
- TDD §§6, 8–11, 14
- Execution Protocol §§3–7, 11–12
- `AS-001_INTAKE_RISE.md` Result record / Out of scope
- `03_EXECUTION/PLANNING/ASCENT_PRE_RESOLUTION.md` §§4, 6, 8, 9

## Owner

Native 90 Hz C++/Jolt. Godot presents authoritative state. Mission/UI observe
predicates only.

## Allowed seam

New campaign bodies continuing `MOD-STAIR-A`, `MOD-SKIN-LADDER-S`, and the
lower landing of `MOD-HALL-DECK` (Atlas band B01's floor, authored here only as
the surface B00 exits onto). Reuse proven primitives: `add_hinge` (world-Z
axis, limited — the exact shape of a bascule), `add_pulley`
(`JPH::PulleyConstraint`, `mMinLength = 0`, a tension-only rope over two
sheaves), `add_point_link`, `create_constraint(..., track_for_teardown = false)`
for runtime topology, the station-gating pattern, and the existing automatic
`commit_checkpoint`.

Do not retitle `KX-*`. Do not author `MOD-NEEDLE-*`, `MOD-CAGE-1`,
`MOD-GUIDE-RACK` or `MOD-EAST-OUTRIGGER` — those are `AS-004`/`AS-005`. Do not
lengthen `MOD-YARD-JIB`. Do not add a second jib.

## Required causal path

**Primary — SHAFT, via the bascule:**

```
ACT  [at CAP-PENDANT: slew MOD-YARD-JIB to bearing 0.80 rad, lower the 4 t pack
      into MOD-CW-CRADLE, hold Release]
  → STATE[the hook-to-pack PointConstraint is removed; cradle gross mass goes
          1800 kg -> 5800 kg; rope tension read back from the solver goes
          17.66 kN -> 56.90 kN]
  → WORLD[56.90 kN exceeds the 47.14 kN that the flight's own weight demands at
          its stowed stop, so MOD-STAIR-A-SWING travels its full 0.9076 rad and
          presses into the deployed stop; the cradle descends 1.620 m]
  → PLAY [MOD-STAIR-A is continuous from the +24.1872 m handoff to the
          +32.1872 m mid-landing and on to MOD-HALL-DECK at +40.1872 m; the
          player climbs it and the automatic checkpoint commits above 40 m]
```

**Alternate — SKIN, freight untouched (Atlas B00 coupling 3):**

```
ACT  [from the +24.1872 m deck, climb MOD-SKIN-LADDER-S rungs 16-25]
  → STATE[support identity = SKIN rungs]
  → WORLD[the hall deck's south edge is reachable; cradle and flight poses
          are unchanged]
  → PLAY [same +40 m commit, with the pack still on the apron]
```

**Reconnect:** both braids arrive on `MOD-HALL-DECK`. `AS-003` and `AS-004`
enter from the same deck whichever way the player got there.

**Illegal:**

```
MISSION_FLAG[forty_open] → WORLD[a walkable flight]
ACT[Release with the pack nowhere near the cradle] → PLAY[the stair deploys]
ACT[pendant command from the +24 m deck] → anything
an invisible wall on SKIN because the pack is still on the apron
```

## Forbidden shortcuts

- a `stair_deployed` / `pack_loaded` flag as WORLD without the bodies moving
- lengthening the 12 m boom or the 11.50 m boom height to close §8.4's gap
- a second jib, a winch at +24 m, or a powered lift the player rides
- teleporting the pack into the cradle, or snapping the flight to its stop
  without the solver driving it
- walling off `MOD-SKIN-LADDER-S` to protect the bascule
- a kill plane in the bay in place of the real 40 m fall
- claiming Fold-device execution

## Implementation scope

Native bodies, constraints and falsifiers; Godot presentation twins; persist
version bump for the sling topology; CI proof lines.

## Out of scope

`AS-003_HOOK5_RACK` (`CAP-HOOK5` acquire). Band B01 above the hall's lower
landing. Fold 45 FPS certification. Art/VO. Reopening `AS-001`.

## Proof path

The ten named falsifiers in §8.11, each a sentence that can fail. Two Godot
screenshots at Fold aspect: the bascule stowed and deployed from the same
camera. Android arm64 APK carrying `libscraperx_native.so`. Kernel and
`AS-001` regressions green.

## Completion

- with the pack on the apron, no route exists between `+24.1872 m` and
  `+32.1872 m`, and the gap is `8.000 m` against a `1.85 m` mantle ceiling
- loading the cradle makes the flight travel, measurably, under solver forces
- the deployed flight is real support the player walks
- the player stands on `MOD-HALL-DECK` at `≥ 40 m` and the checkpoint commits
- unloading the cradle retracts the flight and closes the route again
- SKIN reaches the same deck with the freight untouched
- the jib gap in §8.4 is reported, not engineered around
- Fold install / on-device play remain unverified

## Result record

All ten §8.11 falsifiers pass (`tests/simulation_tests.cpp`, the AS-002 section
appended before `EXIT_SUCCESS`), evidence line above. Both braids close;
retraction is real and reversible; the jib gap is confirmed, not engineered
around. Kernel and `AS-001` falsifiers remain green (`PASS scraperx_sim B00
intake rise` unchanged in the same run).

Deviations from the plan's own literal numbers, each verified against the
running solver rather than assumed, each with a code comment at its own
constant explaining why:

- **`kLegalFortyFlightMassKg = 400`, not the plan's `1900`.** §8.4.1's own
  `T_crit(theta)` derivation is sound and re-checked independently — the
  plan's own worked margins (loaded `1.21x`, empty `0.37x`) are arithmetically
  correct at `1900`/`1800` kg. They are not sufficient in the actual solver: a
  hinge built exactly at its own stowed hard limit needs real headroom to
  depart it, not a bare `>1x` quasi-static margin. At `1900` kg the flight sat
  inert under the real `4000` kg pack for `80+ s` of simulated time. `400` kg
  is the value that deploys reliably, verified, not calculated to a target
  ratio.
- **`kIntakeCwCradleTareMassKg = 500`, not the plan's `1800`.** Same finding,
  the loaded side of it: the ratio between "loaded" and "tare-only" demand on
  the flight's own `T_crit` is fixed by tare mass alone once the (frozen,
  `AS-001`-owned) `4000` kg pack sets the loaded side.
- **`MOD-CW-CRADLE` car half-height `0.90`, not the plan's `1.20`; top face
  unchanged at `5.200 m`.** §8.4's clearance list never checks the `B00`
  belt. At the plan's deployed centre (`2.380`) the car's underside is at
  `1.18 m`, under the belt's `1.38 m` top, and the belt's `18 m` stroke
  carries its south end to `z = -111`, across the car's `x ∈ [7.108, 8.0]`.
  Once per stroke (`15.7 s`) the belt rammed the loaded car, knocked the
  flight `0.045 rad` off its stop, and walked the seated pack `0.3–0.6 m`
  per blow until it fell off (observed within `~40 s`, nobody near it).
  Keeping the top face — the seat and the rope's body point — where the plan
  puts it leaves the rope, the `1.620 m` stroke and every statics number
  unchanged; only the underside rises, to `1.78 m` deployed. The slider limits
  are restated on the top face (`[3.00, 5.80]`), the same range. Falsifier:
  the loaded flight holds `>= 0.90 rad` and the pack drifts `< 0.05 m` over two
  full belt strokes (`INFO AS-002 deployed hold`).
- **`MOD-SKIN-LADDER-S` rungs 17–20 step `0.8 m` west of the column.**
  Directly over the deployed flight's foot, rung 17 on the column line left
  `1.4 m` of headroom over the flight's first metre. The walkway's west end
  follows rung 20 to `x = -5.8`.
- **The retract falsifier's own failure was not a mass problem at all**, though
  it looked exactly like one and cost the most iteration: unloaded and
  re-slung, the flight sat frozen at its exact deployed limit for `20+ s`
  with a real, non-zero pulley tension favouring departure the whole time.
  Direct diagnostic tracing (logging the real world-space contact point of
  every contact touching the flight, not just entity IDs) found the flight in
  continuous, ordinary rigid-body contact with its **own hinge anchor** —
  coincident with it at every sweep angle by construction, since the anchor
  sits at the hinge pivot the flight's own cross-section always occupies.
  This is the same class of defect the plan's own `MOD-DOG-A` retrospective at
  §8.3 already names (a hinge fixture sized bigger than its sweep clearance),
  now recurring against a fixture too small to trip that same check by
  inspection alone. Fixed by excluding that one body pair from collision
  (`kIntakeSwingAnchorEntityId`, `OnContactValidate` in `simulation.cpp`),
  the same mechanism already used for the flight-vs-handoff-deck exclusion
  §8.3 itself calls for ("the stop is the hinge limit, not the deck"). Once
  fixed, retraction works cleanly at the already-verified deploy masses above
  — no further mass or geometry retuning was needed for it.
- **`MOD-SKIN-LADDER-S` continues only to rung 20 (5 rungs), not rung 25 (10),
  and does not turn to climb the hall's south fascia in `+X`.** The plan's own
  mantle-clearance law, correctly re-derived, is a floor on the GAP between
  rungs, not on each rung's own depth: `step - kLandingInset >
  kTraversalReach + kPlayerRadius`, i.e. `step > 1.77 m`, independent of rung
  depth. A climb of nine rungs at that spacing needs `>= 14.2 m` of clear run;
  `MOD-HALL-DECK`'s own well is `4.4 m` deep and the run north of rung 16 is
  capped at `12.5 m` by `build_stack()`'s own southern columns. No single
  straight column threads both. SKIN instead climbs only to the mid-landing's
  own height (`32.1872 m`, clear of the well problem entirely), and a short
  static walkway (`kLegalFortySkinWalkway*`) carries the remaining horizontal
  distance to the mid-landing, from which the upper flight is the rest of the
  route — the same one the SHAFT braid uses past that point.
- **The mid-landing's own south edge moved from the plan's `z = -111.6`
  (`half_z = 3.20`, its literal geometry-table value) to `z = -119.0`
  (`half_z = 4.20`), and later to `z = -121.0` (`center -115.80`,
  `half_z = 5.20`) with the walkway below.** The walkway above and the landing touch at `x = 8.30`
  with zero x-overlap; the only safe crossing is whatever z-band both cover
  at once. The upper flight's own underside, directly overhead near that
  seam, leaves under `2.1 m` of capsule headroom (the same figure the hall
  well's own east-edge widening below required) for `x` past about `5.7`,
  ruling out its own z-band as a crossing corridor; south of it, at the
  plan's original `-111.6` edge, was not covered by the landing at all.
  Widening the landing's own south edge to meet the walkway's is the fix that
  needed no change to the walkway or the flight.
- **The SKIN walkway is `4.0 m` deep, `z` in `[-121, -117]`, not rung 20's own
  `2.0 m` band.** That band runs under the upper flight (`z` in
  `[-117.9, -116.1]`), whose underside falls to `0.6 m` over the walkway at
  `x = 8`: a standing capsule had one `0.4 m` lane (`z` in
  `[-118.65, -118.25]`), and a player reported the top of the climb as an
  opening too small to fit through. South of the flight the air is clear to
  the hall deck's underside (`39.79 m`), so the walkway grows `2 m` south and
  the mid-landing's south edge follows it; the lane is `2.4 m`. Falsifier: after
  the 20-rung climb, a walk that zigzags between `z = -120.3` and `-118.6`
  through the flight's low stretch reaches every point on foot and never drops
  (`INFO AS-002 walkway lane: lowest_y=33.086`); on the old footprint it falls
  off the walkway's edge (`lowest_y=20.63`).
- **The hall well moved from the plan's `center x = -4.51`, `half_x = 3.00`
  to `center x = -1.65`, `half_x = 3.15`.** East edge: a capsule needs
  `~2.1 m` of headroom over an inclined surface, not the `~0.9 m` flat floor
  needs, so the upper flight's own climb does not clear the original east
  strip's underside until past where that edge sat. West edge: the flight's
  own top (local `-X` end, `x = -4.506`) left a `3.0 m` open gap to the
  nearest deck strip at the plan's original edge; the new edge overlaps the
  flight's own last `0.3 m` instead, harmless since Jolt does not solve
  contact response between two static bodies.
- **The swing flight vs. the handoff deck needed an explicit collision
  exclusion**, not just the "the stop is the hinge limit, not the deck"
  design intent §8.3 already states in words: the final third of the sweep
  (`theta` in `[48.8 deg, 60 deg]`) drags the flight's own solid body through
  the deck's near corner by construction (up to `0.36 m`, `0.156 m` still
  present at the rest pose itself) — no `AS-002`-owned knob clears it without
  unpicking the geometry §8.3's own numbers derive from. `OnContactValidate`
  rejects contact between `kIntakeSwingFlightEntityId` and
  `kIntakeHandoffEntityId` specifically; every other pair, flight vs. player
  included, keeps ordinary collision.

Godot presentation twins exist for every new body (the swing flight and its
rope, the cradle and its guide mast, the mid-landing, upper flight, hall deck
and well, the SKIN continuation and its walkway) and R/G pendant bindings for
Release/Attach, with a `LEGAL 40` HUD line. Verified headless (`godot
--headless --path godot --quit-after 300`, and the existing `--ci` WO-004/005/006
proof sequence through `MACHINE_PROVEN`): the extension loads, the scene
builds without a script error, and the full existing proof chain is
unregressed. Not verified: rendered visual correctness on an actual display,
and Fold-device execution, both out of reach of this environment.

Persist: `MachineCheckpoint` captures `intake_swing_flight` and
`intake_cw_cradle` as full `BodyCheckpoint`s (pose and velocity, the same
mechanism every other machine body already uses) plus `intake_pack_slung`,
and `restore_legal_forty_topology()` reconciles the sling constraint against
the restored flag before the next tick reads contacts — §8.10's own
requirement, met with the codebase's existing generic body-checkpoint
mechanism rather than the plan's bespoke `flight_hinge_angle`/`cradle_y`
scalar fields. Neither this ticket nor `AS-001` before it tracks a literal
persist-version number anywhere in source; "version 2" in §8.10 is this
file's own description of the blob's shape, not a field to assert against.

---

## Mechanical close

### 8.1 Identity

| Field | Value |
|---|---|
| Atlas band | B00 — Apron and Intake, the 24–40 m leftover |
| Slice | `+24.1872 m` → first stable stand at `+40.1872 m`, with commit |
| Chain | K0 (Intake) PLAY |
| Live braids | SHAFT (the bascule stair). SKIN (the facade line). FLOW not live |
| Transfer Plate | none. B00's exit commit is a refuge ledger per Atlas §8.1 |
| Modules allowed | `MOD-STAIR-A` (upper section), `MOD-SKIN-LADDER-S` (continuation), `MOD-HALL-DECK` (lower landing only), `MOD-YARD-JIB` (reused), the 4 t pack (reused) |
| Modules forbidden | `MOD-HOOK5-RACK`, `MOD-NEEDLE-*`, `MOD-CAGE-1`, `MOD-GUIDE-RACK`, `MOD-EAST-OUTRIGGER` |
| Capability consumed | none |
| Capability authored | none. The release is the pre-placed sling being let go, not `CAP-HOOK5` |

### 8.2 Entry state

Player support at entry is **either**:

- `kIntakeHandoffEntityId` at walking surface `24.1872 m`, arrived by
  `MOD-STAIR-A` after deploying `MOD-DOG-A`; **or**
- the same deck, arrived by `MOD-SKIN-LADDER-S` with the pack still pinning
  the dog.

Machine poses that may be true at entry, all legal:

- pack on the apron at `(0, 0.90, -112.2)` pinning the dog, dog at `≤ 0.28 rad`
- pack lifted anywhere on the hook up to `y = 10.10`, dog at its `1.45 rad` stop
- pack set back down anywhere inside the jib's 12 m working circle

`MOD-DOG-A`'s state is **irrelevant to this slice**. The bascule does not read
it. A player who came up SKIN with the dog still pinned can still deploy the
bascule, because doing so requires only the jib and the cradle.

Persist: inherit `AS-001`'s blob. No fields consumed.

### 8.3 Geometry

Atlas frame is z-up; this tree is y-up. Every row gives both.
`source.x = atlas.x`, `source.y = atlas.z`, `source.z = atlas.y`.

**Derived datums** (all from frozen constants, shown with their arithmetic):

```
handoff walking surface   24.1872 m     = 6 * 4.000 + 0.18 / cos(15.95°)
flight rise (each)         8.000  m     chosen; two flights close 16.000 m
flight pitch              30.0°         support_normal_y = 0.8660 vs 0.55 floor
flight length L           16.000  m     = 8.000 / sin(30°)
flight run                13.856  m     = 8.000 / tan(30°)
mid-landing surface       32.1872 m
hall deck surface         40.1872 m     = 24.1872 + 16.000
SKIN rung 25 top          40.1872 m     = 24.1872 + 10 * 1.6   -- exactly flush
```

The hall surface is `40.1872 m`, not a round `40.0`, precisely so the SKIN line
lands on it without a step: ten more 1.6 m rungs off the 24.1872 m deck arrive
at it to the millimetre. Atlas's `z ≥ 40 m` is satisfied with 0.19 m to spare.

| ID / member | atlas (x, y, z) | source (x, y, z) | extents (half) | climbable | rating |
|---|---|---|---|---|---|
| `MOD-STAIR-A-SWING` hinge | `(7.856, -112.5, 32.1872)` | `(7.856, 32.1872, -112.5)` | — | — | Z-axis hinge |
| `MOD-STAIR-A-SWING` slab | body-local | centre driven by hinge | `(8.00, 0.18, 0.90)` | **yes, deployed only** | player + 365 kg |
| `MOD-STAIR-A` upper flight | `(2.422, -117.0, 36.1872)` | `(2.422, 36.1872, -117.0)` | `(8.00, 0.18, 0.90)` | yes | player + pack |
| mid-landing | `(9.350, -114.80, 32.0072)` | `(9.350, 32.0072, -114.80)` | `(1.05, 0.18, 3.20)` | yes | spans both lanes |
| `MOD-HALL-DECK` lower landing | `(0.0, -115.1, 39.9872)` | `(0.0, 39.9872, -115.1)` | `(10.00, 0.20, 7.40)` | yes | B01's floor |
| — stair well in that deck | `(-4.51, -117.0, —)` | `(-4.51, —, -117.0)` | `(3.00, —, 2.20)` | opening | `6.0 × 4.4 m` |
| `MOD-CW-CRADLE` car | `(8.608, -108.560, 4.000)` | `(8.608, 4.000, -108.560)` | `(1.50, 1.20, 1.20)` | no (filler) | 1800 kg tare |
| `MOD-CW-CRADLE` guide mast | same xz | `y ∈ [0, 8.0]` | `(0.35, 4.00, 0.35)` | no | slider anchor |
| sheave A (flight side) | `(7.856, -112.5, 36.1872)` | `(7.856, 36.1872, -112.5)` | fixed point | — | `h_s = 4.000 m` above hinge |
| sheave B (cradle side) | `(8.608, -108.560, 36.1872)` | `(8.608, 36.1872, -108.560)` | fixed point | — | directly over the car |
| `MOD-SKIN-LADDER-S` rungs 16–25 | `x = -6.0`, `z` stepping | see below | `(1.00, 0.50, 1.00)` | yes | player + 40 kg |

**Swing flight, both stops** (hinge at the slab's top-end centreline, local
`(+8.00, 0, 0)`; `θ` measured from the downward vertical):

```
stowed   θ =  8°  foot ( 5.630, 16.343,  -112.5)  hanging in open bay air
deployed θ = 60°  foot (-6.000, 24.1872, -112.5)  over the handoff deck
hinge travel = 60° - 8° = 52° = 0.9076 rad
```

Built **at the stowed pose**, so the hinge's as-built angle is `0` and its
limits are `[0.0, 0.9076]` — the same convention `MOD-DOG-A` already uses.

**Clearances, stated in metres, not as "enough room":**

- swing flight lane `z ∈ [-113.4, -111.6]`; `AS-001`'s nearest stair lane face
  is `-114.2` → **0.800 m**
- stowed flight occupies `x ∈ [5.630, 7.856]`; the handoff deck ends at
  `x = -2.0` → **7.630 m**, so the stowed flight is nowhere near the deck
- **mid-landing to hinge:** landing west edge `8.300`, hinge `7.856` → a
  `0.444 m` gap. It is bridged, not fallen through: the capsule is `0.70 m`
  across. It also holds the `0.30 m` sweep setback below with `0.264 m` to spare
- **haul rope vs the mid-landing:** the rope runs from the flight's bracket up
  to sheave A and crosses the landing's plane at `x = 7.413` when stowed and
  `x = 3.338` when deployed — west of the landing's `8.300` edge in both poses,
  worst case **0.887 m**. The rope stays in the flight's own centre plane, so
  the hinge carries no out-of-plane couple. Siting the foot at `x = -6.000`
  rather than `-5.000` is what buys this; at `-5.000` the rope crossed the deck
- cradle `z ∈ [-109.760, -107.360]`; bay front wall face `-110.200` → **0.440 m**
- cradle `x ∈ [7.108, 10.108]`; bay east wall inner face `9.7` → the cradle sits
  **outside** the bay, south of its front wall, which is where the jib can reach
- **hinge sweep:** the slab's top-end corners lie `0.18 m` off the hinge axis and
  sweep a `0.18 m` circle. The mid-landing must be set back **≥ 0.30 m** from the
  hinge line. `AS-001` lost a day to exactly this: `MOD-DOG-A` bound at
  `0.31 rad` on its own trailing corner because its hinge sat flush with the
  jamb. Do not repeat it.
- **deployed foot must not bear:** the stop is the hinge limit, not the deck.
  Hold `≥ 0.06 m` between the flight's structural underside and the deck on
  every path. The resulting step from deck to first tread is `0.06–0.25 m`,
  under `kMantleMinimumRise = 0.35` and under the `0.35 m` capsule radius, so it
  is walked, not mantled — the same reasoning that made `AS-001`'s `0.187 m`
  landing lip safe.
- throat width on the deployed flight: `1.80 m` clear vs `0.70 m` capsule
- hall well `6.0 × 4.4 m` vs `0.70 m` capsule: a chute through it is
  geometrically legal and must not be fake-denied

**SKIN continuation rungs 16–25.** The 0–24 line steps north and its head rung
is at `z = -106.7`; continuing north walks into the bay's front wall at
`-110.5`. The line therefore turns and climbs the hall's south fascia, stepping
in `+X`:

```
rung k (k = 16..25):  top_y = 24.1872 + 1.6 * (k - 15)
                      x     = -6.0 + 2.0 * (k - 16)      -> -6.0 .. 12.0
                      z     = -106.4                     (fascia, protruding south)
                      half  = (1.00, 0.50, 1.00)
```

This reuses `AS-001`'s proven rung law verbatim, which is a **derived
constraint, not a style choice**. With rise `R`, half-height `H`, half-depth `D`:

```
R - 2H <= 0.90                              chest-height wall ray strikes the next rung
R <= kMantleMaximumRise = 1.85
2D - kLandingInset > kTraversalReach + kPlayerRadius
   -> 2.00 - 0.47 = 1.53 > 1.30             the stand is outside probe reach
```

The third line is the one that makes a ladder climbable at all. `AS-001` proved
by observation that violating it produces a mantle→hang→fall cycle that makes
no height, because a mantle drops the player `kLandingInset` in from the rung's
near edge and the next rung is then still inside `probe_ledge`'s reach. Rungs
step in one direction for the same reason: staggered rungs put the next target
exactly where the mantle lands you.

Rung 25 tops at `40.1872` and abuts the hall deck's south edge at `z = -107.7`,
flush.

### 8.4 Mechanism

#### 8.4.1 `MOD-STAIR-A-SWING` — a counterweighted bascule flight

One rigid slab, hinged at its **top** end on a world-Z axis. Gravity pulls it
toward the vertical; a rope pulls its foot out and up. It is a route only while
the rope wins.

Let `θ` be the angle from the downward vertical, `m_f` the flight mass, `L` its
length, `r_a` the rope's lever arm from the hinge, `h_s` the sheave's height
above the hinge, `T` the rope tension.

Rope length from the attachment to sheave A, by the cosine rule:

```
ℓ(θ) = sqrt(r_a² + h_s² + 2·r_a·h_s·cos θ)
```

The rope's moment about the hinge works out to `T · r_a · h_s · sin θ / ℓ(θ)`
— the `sin θ` survives, and gravity's restoring moment is
`m_f · g · (L/2) · sin θ`. Setting them equal, **`sin θ` cancels**:

```
T_crit(θ) = m_f · g · (L/2) · ℓ(θ) / (r_a · h_s)
```

`ℓ` falls as `θ` rises, so `T_crit` falls as the flight swings out. A
constant-tension counterweight therefore has **no stable intermediate
equilibrium**: once it beats `T_crit` at the stowed stop it beats it everywhere
after, and the flight runs hard to the deployed stop and presses into it. That
is not a defect to damp out — it is what a counterweighted stair does, and it is
why both stops are hinge limits rather than soft targets.

**Design values:**

```
m_f = 1900 kg     16.000 × 1.80 m open grating flight = 66 kg/m²  DESIGN TARGET
L   = 16.000 m    L/2 = 8.000 m
r_a = 15.000 m    rope bracket 1.000 m inboard of the foot
h_s =  4.000 m    sheave A above the hinge
```

**Worked balance** (`g = 9.81`):

| | `ℓ(θ)` | `T_crit` | `m_c` needed |
|---|---|---|---|
| stowed, `θ = 8°` | `18.969 m` | `47 142 N` | `4 805.5 kg` |
| deployed, `θ = 60°` | `17.349 m` | `43 117 N` | `4 395.2 kg` |

```
T_crit(8°) = 1900 × 9.81 × 8.000 × 18.969 / (15.000 × 4.000) = 47 142 N
cradle stroke = ℓ(8°) − ℓ(60°) = 18.969 − 17.349 = 1.620 m
```

| cradle | gross | `T = m_c·g` | vs stowed | vs stop | result |
|---|---|---|---|---|---|
| empty | `1800 kg` | `17 658 N` | **`0.37×`** | `0.41×` | stays stowed |
| + 4 t pack | `5800 kg` | `56 898 N` | **`1.21×`** | `1.32×` | deploys and holds |

The empty cradle is **2.67× short** of the tension the stowed stop demands;
the loaded cradle is **1.21× over** it. Nothing here sits on a knife-edge,
and no intermediate cradle mass produces an ambiguous half-deployed stair.

**Reserve at the deployed stop, and the flight's real SWL:**

```
τ_rope(60°) = 56 898 × 15.000 × 4.000 × sin60° / 17.349 = 170 410 N·m
τ_grav(60°) = 1900 × 9.81 × 8.000 × sin60°             = 129 135 N·m
net                                                     =  41 275 N·m

player 85 kg at mid-span costs 85 × 9.81 × 8.000 × sin60° = 5 777 N·m
  -> net 35 498 N·m, i.e. the player consumes 14 % of the reserve
sag point at mid-span = 41 275 / (9.81 × 8.000 × sin60°) = 607.3 kg gross
```

Declare the flight's **SWL = 450 kg** (player plus 365 kg). Past `607 kg` at
mid-span the rope loses and the flight sags off its stop — which is a correct,
physical overload, not a failure mode to suppress. It is unreachable in this
slice, since nothing the player can carry approaches it.

#### 8.4.2 The rope — one `JPH::PulleyConstraint`

`add_pulley` already builds exactly this for the Kellerworks lift. It must be
generalised to take its four points, mass ratio and length rather than hardcoding
them. Jolt's constraint is `|p₁ − f₁| + ratio·|p₂ − f₂| = length`, which is a
real inextensible rope over two sheaves:

```
bodyPoint1  flight, local (+7.000, -0.18, 0)   -> r_a = 15.000 m from the hinge
fixedPoint1 sheave A  (7.856, 36.1872, -112.5)
bodyPoint2  cradle top (8.608,  5.200, -108.560)
fixedPoint2 sheave B  (8.608, 36.1872, -108.560)
ratio       1.0
minLength   0.0        tension-only: the rope may go slack, never push
maxLength   ℓ(8°) + (36.1872 − 5.200) = 18.969 + 30.987 = 49.956 m
```

At full deploy the cradle side becomes `49.956 − 17.349 = 32.607 m`, putting the
cradle's top at `3.580 m` and its centre at `2.380 m` — the `1.620 m` stroke.
The cradle rides a free vertical `SliderConstraint` (no motor) with limits
`y_centre ∈ [1.80, 4.60]`, deliberately **wider** than the working stroke so the
guide never becomes the stop. The hinge limits are the only stops.

The rope is never slack in either rest state: stowed, the cradle's `17 658 N`
still hangs on it, it simply loses to the flight's `47 142 N` demand.

Rope rating: `56 898 N` working, declare `8 t MBL`. DESIGN TARGET.

#### 8.4.3 The sling release — the actual hinge of the causal path

`AS-001`'s `add_point_link` is bidirectional. The pack cannot be let go, so this
slice authors a real release, mirroring `WO-012`'s seat/unseat exactly, including
its race fix.

```
release_pack_to_cradle():
    requires  |pack.xz − cradle_seat.xz| ≤ 0.55 m
              |pack.y  − cradle_seat.y | ≤ 0.35 m
              |v_pack| ≤ 0.15 m/s
              pendant Release command held this tick
    effect    remove the hook↔pack PointConstraint

sling_pack():
    requires  |hook − pack_padeye| ≤ 0.40 m
              |v_hook| ≤ 0.20 m/s
              pendant Attach command held this tick
    effect    create the hook↔pack PointConstraint
```

Both use `create_constraint(..., track_for_teardown = false)`: Jolt's
`ConstraintManager::Remove` asserts on an already-invalidated index, so a
constraint that may be removed at runtime must never also sit in
`machine_constraints_`, which the destructor removes unconditionally once.
Exactly one owner releases, on every path.

**Gate each predicate on its command, not on pose alone.** `WO-012` proved by
observation that a pose-only predicate re-seats on the very tick after release,
because the body is still at the seat pose with near-zero velocity — a real
mechanism needs real time to move. Same defect class, same fix, stated in advance.

Commands: `Release` and `Attach` are one-shot toggles on the existing pendant,
station-gated identically to slew and hoist. Accepted anywhere, effective only
within `kIntakeStationRadius = 3.40 m` of `(3.0, -100.0)`.

#### 8.4.4 `MOD-YARD-JIB` reach — what the jib is asked to do here

The hook hangs at the boom tip, so its working radius is **fixed at 12.000 m**;
the boom slews and the winch raises, nothing luffs. The cradle must therefore
sit on that circle:

```
bearing ψ = 0.80 rad of the 0.90 rad limit   (11 % margin)
cradle    = (12·sin ψ, —, −100.2 − 12·cos ψ) = (8.608, —, −108.560)
```

The cradle is a three-sided frame — floor and two side rails at `x = ±1.50`,
**open in ±Z** — not a closed hopper. A 2.4 m hopper cannot take a
`2.2 × 2.3 m` pack, and widening the box to suit it puts the cradle inside the
bay's front wall. An open cradle takes the pack with `0.40 m` of side clearance
and keeps `0.440 m` between the frame and the wall.

Loading it is inside everything already proven by `AS-001`: `4000 kg <
5000 kg SWL`; `39 240 N < 49 050 N` winch; hook must reach `y = 4.800` to seat
the pack, inside the `[2.0, 11.05]` travel. The 9 t proof load still stalls.

#### 8.4.5 **GAP REPORT — Atlas band B00 exit state 3 does not close**

Atlas §6 lists three B00 exits. The third is *"yard jib used to place the player
on the +40 m timber soffit."* Protocol §7 requires this be derived from the
frozen `12 m` boom and `11.50 m` boom height, and §9 requires the gap be
reported rather than engineered around.

**By reach:**

```
max hook y                        11.050 m   = (11.50 − 0.45) − 2.0 + 2.0
a rider standing on a slung pack  ≈ 9.950 m  (pack top), head ≈ 11.750 m
required                          40.1872 m
deficit                           28.44 m
boom height that would close it   ≈ 41.8 m   = 3.63 × the frozen 11.50 m
```

**By control topology — the stronger reason.** The pendant is station-gated to a
`3.40 m` radius at `(3.0, -100.0)` at grade. A player riding the hook is not at
the station, so the jib is inert. **No boom length closes this exit**, because
the player cannot simultaneously ride the machine and command it. The reach
deficit is a symptom; the gating is the cause.

This is not a defect. It is the shape of the game: you cannot ride your own
crane, so every rung of the ascent must be a configuration you set from the
ground and then climb. §8.4.1 is the first mechanism built on that premise
rather than around it.

**Resolution:** close the slice on the other two Atlas exits — `MOD-STAIR-A`
continued, and `MOD-SKIN-LADDER-S` continued. Do not lengthen the boom. Do not
add a rideable hoist. If a later authority wants exit 3, it needs either a
second operator or a latched pendant, and that is a GDD-level change, not a
work-order one.

### 8.5 Occupancy and interlocks

| Envelope | Effect |
|---|---|
| pack centre inside the cradle seat tolerance, settled, Release held | sling removed; cradle gross `→ 5800 kg` |
| cradle gross `≥ 4805.5 kg` | flight travels off the stowed stop |
| cradle gross `< 4395.2 kg` while deployed | flight sags back toward stowed |
| player anywhere in the flight's `52°` sweep | struck by a real body; no scripted exception |
| player outside `3.40 m` of the pendant | every pendant axis reads zero |
| hinge at either limit | hard stop; the solver holds it, no damping hack |
| cradle at a guide limit | not reachable in the working stroke by design |

No `stair_deployed`, `pack_loaded`, or `forty_open` flag exists. `pack_in_cradle`
and `flight_deployed` are **derived** from measured pose for the HUD and the
falsifiers only, exactly as `AS-001`'s `intake_pack_pins_dog` is. No simulation
branch may read either.

### 8.6 Required causal path

See the Required causal path section above. The next file starts on
`MOD-HALL-DECK`.

### 8.7 Support / traversal handoff

| Step | Member | source y | type | inherits v? |
|---|---|---|---|---|
| 0 | `kIntakeHandoffEntityId` | `24.1872` | static | no |
| 1a | `MOD-STAIR-A-SWING` | `24.19 → 32.19` | **dynamic** | **yes** — it is a hinged body |
| 1b | `MOD-SKIN-LADDER-S` rungs 16–25 | `25.79 → 40.19` | static | no |
| 2 | mid-landing | `32.1872` | static | no |
| 3 | `MOD-STAIR-A` upper flight | `32.19 → 40.19` | static | no |
| 4 | `MOD-HALL-DECK` | `40.1872` | static | no |

Row 1a is the interesting one: the deployed flight is a **dynamic body under
load**, not static scenery, so it must be added to `entity_is_moving_support`.
A player standing on it while it is still settling against its stop inherits its
point velocity under the `WO-002` law. That is correct and must not be special-cased.

### 8.8 Failure states

| Trigger | What the world does | What the player can still do | Must not happen |
|---|---|---|---|
| never load the cradle | flight stays stowed at `8°`, `support_normal_y ≈ 0.14` | climb SKIN to the same deck | a flag opening the route |
| Release commanded with the pack out of tolerance | rejected; sling holds | reposition and retry | pack teleporting to the seat |
| Release commanded off-station | no effect at all | walk to the pendant | any remote actuation |
| standing in the sweep when it deploys | struck and shoved by a real body | chute; the apron is the landing field | a scripted dodge or an ignore-collision |
| unload while the player is on the flight | flight sags to stowed, player falls | chute from up to 40 m | a kill plane; the fall is real, `28.0 m/s` is lethal |
| fall through the hall well | free fall into the bay | chute; `6.0 × 4.4 m` is legal clearance | fake-denying deployment |
| overload past `607 kg` at mid-span | flight sags off its stop | step back | the stop holding an impossible load |

### 8.9 Recovery

Atlas §3 soft-lock rule. No state here can strand the player:

- **never solve the bascule** → `MOD-SKIN-LADDER-S` reaches the same deck, and
  it is never blocked
- **deploy, climb, then want back down** → the flight is two-way; so is SKIN
- **fall from any height in this slice** → chute to the apron (Atlas §8.3's
  primary landing field), then re-climb `AS-001`'s stair or SKIN
- **die** → restore to the last commit; if `≥ 40 m` was reached, that commit is
  on the hall deck
- **cradle left loaded with the player above** → harmless; the deployed state is
  the useful one

Nothing auto-repairs. Nothing is deleted to make the next band load clean.

### 8.10 Persist

**Persist version 2** (first bump on this branch). `AS-001` added no fields;
this slice must, because the sling is runtime topology that outlives its bodies:

```
pack_slung          bool    is the hook↔pack PointConstraint present
cradle_pack_seated  bool    derived at capture, re-derived on restore
flight_hinge_angle  float   captured with the other machine bodies
cradle_y            float
```

`restore_pack_topology()` mirrors `restore_needle_topology()`: after a restore,
reconcile the sling constraint against `pack_slung` before the next tick reads
contacts. Import of a v1 blob assumes `pack_slung = true`, cradle empty, flight
stowed — `AS-001`'s exit state.

Commit: the existing automatic `commit_checkpoint` already fires on every
grounded, non-traversing tick, so standing on the hall deck commits with no new
machinery. The falsifier asserts `checkpoint_position.y ≥ 40.0`, not that some
new commit path ran.

### 8.11 Falsifiers (deterministic proof)

1. `wo015_unloaded_flight_is_not_a_route` — spawn on the `+24.1872 m` deck, pack
   on the apron. Walk at the flight's deployed footprint for 14 s. The deepest
   `y` reached stays below `25.2`, `support_entity_id` is never the swing flight,
   and the hinge angle stays `≤ 0.20 rad`.
2. `wo015_flag_is_not_a_stair` — from the same spawn, mash every command in the
   game for 14 s while off-station. The hinge never travels, the sling never
   releases, and no standable surface appears between `24.19` and `32.19`.
3. `wo015_gap_exceeds_mantle` — assert the vertical gap from the deck to the
   upper flight's foot is `8.000 m` against `kMantleMaximumRise = 1.85`, so the
   route is closed by geometry and not by a tuned probe.
4. `wo015_cradle_load_deploys` — from `IntakePendant`: slew to `0.80 rad`, lower,
   hold Release. Assert the sling is gone, the cradle descends `≥ 1.40 m`, and
   the hinge reaches `≥ 0.85 rad` — driven by the solver, within a 40 s budget.
5. `wo015_deployed_flight_is_support` — walk the deployed flight. While over open
   air, `support_entity_id` is the swing flight. Reach the mid-landing at
   `y ≥ 32.19 + 0.70`.
6. `wo015_reaches_forty_and_commits` — continue to the hall deck.
   `support_entity_id == MOD-HALL-DECK`, player `y ≥ 40.1872 + 0.70`,
   `checkpoint_commit_count` increased, and `checkpoint_position.y ≥ 40.0`.
7. `wo015_unload_retracts` — re-attach the sling, lift the pack clear. The hinge
   returns to `≤ 0.20 rad` and the route closes again. Reversible, not a
   one-way flag.
8. `wo015_skin_skips_the_freight` — from `IntakeSkinFoot`, climb to
   `support_entity_id == MOD-HALL-DECK` with the pack never leaving the apron and
   the hinge never leaving `≤ 0.20 rad`.
9. `wo015_jib_cannot_reach_forty` — hold the hoist at its limit for 60 s. Every
   jib-driven body stays below `y = 12.0`. This is §8.4.5's arithmetic made
   executable, so the gap cannot be quietly closed later by a boom change.
10. `wo015_prior_still_pass` — `AS-001` and all kernel falsifiers `PASS`
    unchanged, the 9 t proof load still on the ground.

### 8.12 Exit state

- the player **can** stand on `MOD-HALL-DECK` at `40.1872 m`, arrived by either
  braid, with a committed checkpoint there
- the bascule is left wherever the player left it — deployed with the pack in
  the cradle, or stowed with the pack on the apron; both are legal entry states
  for `AS-003`
- `MOD-HALL-DECK` exists only as B01's lower landing with its stair well. The
  hall's east opening, `MOD-EAST-OUTRIGGER`, `MOD-NEEDLE-POCKETS`, the 48 m
  racks and `MOD-CAGE-1` **do not exist yet**
- `MOD-HOOK5-RACK` does not exist; `CAP-HOOK5` is not held
- everything `AS-001` built still exists below
- the `+120 m` continuation is visible and unfinished
- persist is v2
- Fold-device execution remains unverified

---

**Stop. Do not begin the next file inside this one.**
Next file: `03_EXECUTION/ASCENT/AS-003_HOOK5_RACK.md`
