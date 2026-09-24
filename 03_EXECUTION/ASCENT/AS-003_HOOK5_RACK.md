# SCRAPERX — AS-003 HOOK5 RACK (CAP-HOOK5 ACQUIRE)

**Ascent Slice:** `AS-003`
**Lifecycle:** `IMPLEMENTED` — falsifiers green, source and tests in `claude/android-game-dev-continue-m045pq`
**Provenance:** re-derived against this branch; built against that derivation, with deviations recorded in the Result record below wherever measurement disagreed with the plan
**Implementation gate:** `AS-002` implemented and its exit revalidated in source. **Satisfied.**
**Evidence:**

```
PASS scraperx_sim AS-003 apron: grade_peak_y=2.41139 pack_peak_y=4.21324
  pack_closest=0.309441
INFO AS-003 carry walk: loaded_v_at_0.25s=4.90744 free_v_at_0.25s=5.5
  loaded_top=5.58061 free_top=5.5
PASS scraperx_sim AS-003 cage: mashed_door=0.0352734 travelled_door=1.20154
  carry_worst_gap=0.848285 repicked_y=0.987821 skin_mantles=15
PASS scraperx_sim AS-003 Hook5 Rack: ride_s=10.6111 roof_y=5.36879
  hatch_impact=9.35161 forty_y=41.0753 restored_y=41.0654 restored_carry=56
SCRAPERX_UITEST PASS touch_carry door_rad=1.45 block=(9.87,0.30,-90.66)
```

Local runs of `scraperx_sim_tests` (every prior group green in the same run)
and `--uitest=touch_carry`; CI integration is recorded in `00_START_HERE.md`.
**Depends on:** `AS-001_INTAKE_RISE.md` in source and green; `AS-002_LEGAL_FORTY.md`
in source; kernel falsifiers green; protocol `03_EXECUTION/PLANNING/ASCENT_PRE_RESOLUTION.md` §8.

> **Authoring note.** This file replaces the plan adopted from `ScraperX-Grok`
> (their `WO-011_HOOK5_RACK`) for the same reason `AS-002` was replaced: its
> inherited constants describe a world that does not exist here. Re-authored
> against this branch's real `AS-001` source and `AS-002`'s planned exit.

## Objective

Author `MOD-HOOK5-RACK` and the acquisition of `CAP-HOOK5`.

Atlas §6 band B00 names it: *"Hook block + slings in a locked cage opened by
moving the crate or circling the belt."* The capability it grants is consumed by
`AS-004`, where nothing can be attached to a needle without it.

The slice owns **one mechanism and one new player state**:

1. The cage is locked by **height**, not by a flag — its only opening is 2.90 m
   up against a 1.85 m mantle ceiling, and the only surface that gets you there
   is `MOD-INTAKE-BELT`'s deck while it is alongside.
2. `CAP-HOOK5` is a **carried body**, not an inventory bit. Carrying it occupies
   both hands, which is the first time in the campaign that the two braids mean
   different things: the stair takes cargo, the ladder does not.

## Existing truth

From source at `af8beca`, `MOD-INTAKE-BELT` verbatim:

| Constant | Value |
|---|---|
| `kIntakeBeltX` | `6.0` |
| `kIntakeBeltHalfX` / `HalfZ` / `HalfY` | `2.0` / `6.0` / `0.18` |
| `kIntakeBeltTopY` | `1.38` |
| `kIntakeBeltCenterZ` | `-96.0` |
| `kIntakeBeltStrokeMeters` | `18.0` |
| `kIntakeBeltAngularFrequency` | `0.40` |

driven in `update_support_motion` as
`belt_z = -96.0 + 9.0 · sin(0.40 · t)`, kinematic, and already in
`entity_is_moving_support` — so riding it inherits its point velocity under the
`WO-002` law. Deck spans `x ∈ [4.0, 8.0]`, top `1.38 m`, period `15.708 s`.

Traversal constants this slice leans on:
`kMantleMinimumRise 0.35`, `kMantleMaximumRise 1.85`, `kTraversalReach 0.95`,
`kLandingInset 0.47`, `kPlayerRadius 0.35`, `kPlayerHalfHeight 0.90`,
`kPlayerMassKg 85.0`, `kLethalImpactSpeedMps 20.0`.

`probe_ledge` requires the wall ray and the top ray to strike the **same body**,
and with `require_supported_landing` the landing probe must strike it too. Every
mantle target below is therefore one box whose front face is the wall and whose
top face is the landing — the law `AS-001` established for `MOD-SKIN-LADDER-S`.

**Inherited from `AS-002` as DESIGN TARGET, not source:** the `+40.1872 m`
`MOD-HALL-DECK`, the bascule and its cradle at `(8.608, —, -108.560)`, persist
v2. If `AS-002` is re-sited before it lands, §8.3's clearances here must be
re-derived against what actually shipped.

Entity IDs and spawns are **assigned when this slice is coded**, after `AS-002`
has taken its block. This file names bodies, not numbers.

## Authority

- Governing Laws 2–7, 12–18, 21–27, 29, 32
- GDD §§4, 6, 7.2–7.4, 11, 16, 17, 23, 24
- Atlas §§3, 4 (`CAP-HOOK5`), 6 band B00, 7 K0, 8.3, 12, 13
- TDD §§6, 8–11, 14
- `AS-002_LEGAL_FORTY.md` §8.12
- `03_EXECUTION/PLANNING/ASCENT_PRE_RESOLUTION.md` §§4, 6, 8, 9

## Owner

Native 90 Hz C++/Jolt. Godot presents. Mission/UI observe predicates only.

## Allowed seam

New campaign bodies for `MOD-HOOK5-RACK`. A carry constraint between the player
body and a carryable, created and removed at runtime. Reuse: the moving-support
law, `probe_ledge`/mantle, `add_vertical_hinge` with a permanent drive (the
`MOD-DOG-A` pattern), `create_constraint(..., track_for_teardown = false)`, and
the station-gating shape for the pick/drop commands.

Do not retitle `KX-*`. Do not author `MOD-NEEDLE-*`, `MOD-CAGE-1` or
`MOD-GUIDE-RACK` — `AS-004`. Do not lengthen `MOD-YARD-JIB`. Do not move
`AS-002`'s cradle to make §8.4.4's coupling fit.

## Required causal path

**Primary — the belt gets you in:**

```
ACT  [board MOD-INTAKE-BELT and ride it north]
  → STATE[support = the belt; the deck carries the player into the window where
          it lies alongside MOD-HOOK5-RACK, 8.97 s of every 15.708 s]
  → WORLD[the cage's west buttress top at 2.90 m is 1.52 m above the deck, inside
          the mantle band; from the apron it is 2.90 m and outside it]
  → PLAY [the roof is standable; the hatch drops the player into the cage]

ACT  [lift MOD-HOOK5-BAR out of its keepers and set it down clear]
  → STATE[the bar is a 38 kg body carried on a real constraint, then released]
  → WORLD[nothing is left in the door's swing, so the door's permanent opening
          torque travels it]
  → PLAY [the cage is open at grade, from now on, for everyone]

ACT  [lift the hook block off its rack]
  → STATE[carry constraint created; hook_in_rack goes false]
  → WORLD[CAP-HOOK5 is held: a 36 kg body attached to the player]
  → PLAY [AS-004 can attach it. Both hands are now full, so no traversal move
          begins until it is set down]
```

**The braid consequence, which is the point of the slice:**

```
carrying  -> MOD-STAIR-A is walkable end to end; MOD-SKIN-LADDER-S is not,
             because every rung is a mantle and both hands are on the block
not carrying -> both braids as before, unchanged
```

**Illegal:**

```
MISSION_FLAG[has_hook5] → WORLD[a needle attaches]
ACT[mantle the cage from the apron] → PLAY[inside]        -- 2.90 m vs 1.85 m
an invisible wall on MOD-SKIN-LADDER-S while the block is held
deleting the block on pickup and re-spawning it at the needle
```

## Forbidden shortcuts

- `CAP-HOOK5` as a bool with no body behind it
- a cage that opens on a timer, a proximity trigger, or a mission flag
- teleporting the block to the player, or to the needle in `AS-004`
- blocking `MOD-SKIN-LADDER-S` geometrically while the block is held — the
  ladder must be untouched; only the player's own state changes
- a second jib, or re-siting `AS-002`'s cradle, to make §8.4.4 close
- claiming Fold-device execution

## Implementation scope

Native bodies, the carry constraint and its commands, falsifiers; Godot twins;
persist v3 for the carry topology; CI proof lines.

## Out of scope

`AS-004_NEEDLE_SEAT`. Band B01 above `MOD-HALL-DECK`. Any second use of
`CAP-HOOK5` inside this slice. Fold 45 FPS. Art/VO. Reopening `AS-001`/`AS-002`.

## Proof path

The nine falsifiers in §8.11. Two Godot screenshots at Fold aspect: the deck
alongside the cage mid-window, and the player at grade holding the block with the
door travelled. Android arm64 APK. Kernel, `AS-001` and `AS-002` green.

## Completion

- the cage cannot be entered from the apron, at any point, by any command
- riding the belt puts the buttress inside the mantle band and the roof is reached
- the bar is a body; moving it is what travels the door; a flag cannot
- the block is a body; holding it is a real constraint
- holding it, no traversal move begins; setting it down restores every move
- `MOD-SKIN-LADDER-S` itself is bit-for-bit unchanged
- the player can stand at `+40.1872 m` holding the block, having walked the stair
- Fold install / on-device play remain unverified

## Result record

The nine §8.11 falsifiers are three native groups (`tests/simulation_tests.cpp`,
the AS-003 section before `EXIT_SUCCESS`) and one UI scenario:

- **1** is `AS-003 apron`: from the `Hook5Apron` spawn, eleven running leaps at
  every face but the belt's, each asserted to carry the body against the wall,
  with jump and traversal mashed; then four leaps at the cage off the 9 t
  pack's top, mantled from grade. No cage body is ever stood on, grabbed or
  offered as a ledge; grade never rises past a plain jump (`2.41 m`).
- **3, 4, 5, 6, 7** are `AS-003 cage`, one instance from the `Hook5Cage`
  spawn: 30 s of every command mashed leaves the door at `0.035 rad` and the
  bar in its brackets; lifted, carried north and set down, the bar frees the
  door, which travels past `1.20 rad` with no door command (none exists). The
  block comes off its rack, is carried out, toured for 10 s within `0.85 m`,
  set down to rest, and picked up again off the ground. Held at the 9 t
  pack's face, 5 s of traversal requests begin nothing and no ledge is
  offered; set down, the same face mantles at once. Carried to
  `MOD-SKIN-LADDER-S`'s foot, the first rung is neither offered nor climbed
  while held; set down on the spot, the same rung is offered at once and the
  same instance climbs to `+24 m` in 15 mantles. Falsifier 7 is asserted by
  that behaviour -- the refusal and the offer bracket a set-down with nothing
  else changing -- not by reading rung bodies, which no API exposes; the carry
  code has no path that touches them.
- **2, 8** and persist are `AS-003 Hook5 Rack`, one instance from the pendant:
  `AS-002`'s freight and cradle sequence, then the belt ride onto the roof
  (`10.6 s`, within two strokes), the hatch (`9.35 m/s`, non-lethal), the bar,
  the block, out through the door, round the belt's north end, through the
  throat, every flight of `MOD-STAIR-A`, the deployed flight and the upper
  flight to `MOD-HALL-DECK` with the block held (`y = 41.08`). Walked off the
  hall deck's north edge holding it, the 40 m fall is lethal and the restore
  returns the body to the deck with the block in hand, and holds there.
- **9**: every kernel, `AS-001` and `AS-002` group green in the same run; the
  9 t proof load is still on the ground at the end.
- `touch_carry` drives the same sequence inside the cage through the touch
  pipeline: Action reads PICK UP at the bar and at the block and SET DOWN
  while holding; the door swings open by itself; both rendered hands are on
  the block (`POSE_CARRY`); it is carried out and set down on the apron.

Measured, as §8.4.3 asked: the 36 kg load slows the launch -- `4.91 m/s` at
`0.25 s` against `5.5` empty -- and leaves top speed unchanged.

Deviations from the plan's numbers, each measured in the running solver and
each explained at its own constant in `simulation.cpp`:

- **Cage top `4.45 m`, not `2.90`.** The plan measured the lock against the
  mantle ceiling alone. A running jump into a ledge grab reaches `3.75 m` above
  the floor it leaves (measured against walls of every height), so `2.90` was
  grabbable from grade. At `4.45` grade is `0.70 m` short and the belt's deck
  (`5.13 m` reach) is `0.68 m` inside: the belt is still the key, jumped from
  and grabbed rather than mantled.
- **Sited at `z ∈ [-88, -84]`, not `[-106, -102]`.** A height lock holds only
  if nothing but the key is within a jump, and running jumps carry far: a
  `4.45 m` ledge is still grabbed across `6.3 m` of air from `1.38 m`, `6.8 m`
  from `1.8 m`, and a `4.45 m` roof is landed on across `10.8 m` from `8 m` up.
  At the plan's site the pendant catwalk, the 9 t pack's top, the bay's wall
  top (off `MOD-STAIR-A`'s first landing) and the WO-006 lift all reached the
  roof (7 of 48 probe leaps off the 9 t pack alone). At the new site a
  1 006-leap audit -- grade round every face, the 9 t pack, the catwalk, its
  ramp, the WO-006 catwalk, the WO-006 lift at its 9 m top -- reached a cage
  body only from the lift, 3 times in 162, by an `11 m` leap: a machine-made
  route, legal under Governing Law 17, recorded here and not blocked. The deck
  lies alongside for `43 %` of its stroke (the plan's site: `57 %`), lingering
  at the north end where the jump is made. §8.4.4's gap report stands and
  grows: the crate coupling is not sited, and the cradle is now 20 m away.
- **The door opens `1.45 rad` on `600 N·m`, not `1.35` on `6 000`.** The
  rigid bar holds against any torque; at `6 000` the leaf pinned the bar in
  its brackets with about `7 kN`, which lifting would have to fight. The
  hinge is `0.20 m` behind the west jamb as §8.3 asks.
- **Brackets, not keepers, and outside the leaf's sweep.** The plan's keepers
  sat inside the `1.146 m` sweep and would have stopped the door with the bar
  gone. Each bracket is a ledge under a bar end and a stop on its north face.
- **Bar seated at `1.00 m`, rack top `0.60 m` in the north-east corner** (plan:
  `1.30`, `0.90` mid-wall). The carry hands must be above every handle they
  lift, or the constraint pulls the load into its support and hoists the
  player instead (observed at the plan's `+0.30` with the bar at `1.30`); the
  loads are seated low rather than the hands raised, because at chest height
  the carried block's top sat at the eye and filled the view (seen in the
  Fold capture). In a `2.3 m` room the `2.05 m` bar needs a clear place to be
  set down; with the rack mid-wall every such place put one end on it.
- **The carry point: `0.60 m` ahead, `0.35 m` above the body's centre**
  (`1.25 m` over the soles), not `+0.30`; it swings round to the facing at
  `3 rad/s` rather than with it -- an instant half-turn threw the hand point
  `1.2 m` in one tick and the block out of it. A held body slips only when it
  is more than `0.90 m` from the hands **and** still moving apart, so a wedged
  bar lets go while a block lifted off the floor at the edge of reach does not.
- **Persist is the existing checkpoint, extended.** This branch has no
  serialized save and no "persist v2" to version; `MachineCheckpoint` gains
  the door, bar and block bodies and the carried entity, and
  `restore_carry_topology()` reconciles the constraint after a restore. A held
  load is restored at rest with the body it is held by (restored with its
  committed walking speed, it swung out of the still hands and dragged the
  body back off the edge it had just been restored onto).
- **Action puts PICK UP ahead of CLIMB.** The rack is itself a mantle ledge,
  and whoever faces the block means the block. Drop and Back set down.
- **The cage walls are drawn barred inside the native `0.30 m` slabs**, so the
  block, the bar across the door and the hatch are seen from outside while
  everything visible stays inside what is solid.

Defects found on the way and fixed at their source, not routed around:

- `DEFECT src/sim/simulation.cpp commit_checkpoint -> committed on any grounded
  tick, including a capsule held by its rim with its centre 0.3 m past an
  edge.` A lethal fall off that edge restored into the same slide, every time.
  Commits now need firm footing: a ray straight down from the body's centre
  meets walkable ground within its half-height plus `0.15 m`, which still
  commits on the deployed 30-degree flight.
- `DEFECT tests/simulation_tests.cpp AS-002 ascent -> its (-9, -108) waypoint,
  0.3 m inside the handoff deck's north edge, dithered the body off the deck
  -- a lethal 24 m fall the test never checked for.` It passed only because
  the rim checkpoint happened to restore onto a working trajectory. The walk
  now arrives and stops 1.3 m inside the edge, and the test asserts the route
  kills no one.
- `DEFECT src/bridge/scraperx_simulation.cpp kInitialSpawnCount -> the literal
  21 had fallen behind the enum and refused IntakeHandoffDeck.` It is now
  derived from the last spawn.

Not done here: a Godot capture of the deck alongside the cage mid-window
(the local captures are the cage interior, the carry and the approach);
Fold-device execution remains unverified.

---

## Mechanical close

### 8.1 Identity

| Field | Value |
|---|---|
| Atlas band | B00 — Apron and Intake, grade level |
| Slice | acquire `CAP-HOOK5` and carry it to `+40.1872 m` |
| Chain | K0 (Intake), capability leg |
| Live braids | SHAFT (the stair, as cargo route). SKIN (unchanged, free-hands only) |
| Transfer Plate | none |
| Modules allowed | `MOD-HOOK5-RACK`, `MOD-INTAKE-BELT` (reused, unmodified) |
| Modules forbidden | `MOD-NEEDLE-*`, `MOD-CAGE-1`, `MOD-GUIDE-RACK`, `MOD-EAST-OUTRIGGER` |
| Capability consumed | none |
| Capability authored | **`CAP-HOOK5`**, as `!hook_in_rack`, derived from the block's measured pose |

### 8.2 Entry state

Player support at entry may be anywhere `AS-002` leaves them: the apron, the
`+24.1872 m` handoff, or `MOD-HALL-DECK` at `+40.1872 m`. The slice is entered
by walking back down to grade, which is not a regression — it is the shape of a
tower where the tools live at the bottom.

Machine poses, all legal at entry:

- pack on the apron, in the cradle, or slung anywhere in the jib's 12 m circle
- bascule stowed or deployed — irrelevant to this slice, which reads neither
- `MOD-DOG-A` at either stop
- belt running: it always runs, it has no control, and this slice adds none

Persist: inherit v2. `hook_in_rack` is true, `carrying` is false.

### 8.3 Geometry

`source.x = atlas.x`, `source.y = atlas.z`, `source.z = atlas.y`.

| ID / member | atlas (x, y, z) | source (x, y, z) | extents (half) | climbable | rating |
|---|---|---|---|---|---|
| cage west buttress | `(8.700, -104.0, 1.450)` | `(8.700, 1.450, -104.0)` | `(0.70, 1.45, 2.00)` | **yes — the only way in** | player, free hands |
| cage roof | `(10.700, -104.0, 2.750)` | `(10.700, 2.750, -104.0)` | `(1.30, 0.15, 2.00)` | yes | player + 40 kg |
| cage north wall | `(10.700, -102.15, 1.450)` | `(10.700, 1.450, -102.15)` | `(1.30, 1.45, 0.15)` | no (filler) | — |
| cage east wall | `(11.850, -104.0, 1.450)` | `(11.850, 1.450, -104.0)` | `(0.15, 1.45, 2.00)` | no (filler) | — |
| cage south wall, west leaf | `(9.000, -105.85, 1.450)` | `(9.000, 1.450, -105.85)` | `(1.00, 1.45, 0.15)` | no (filler) | — |
| cage south wall, header | `(10.800, -105.85, 2.550)` | `(10.800, 2.550, -105.85)` | `(0.80, 0.35, 0.15)` | no (filler) | over the doorway |
| `MOD-HOOK5-DOOR` | hinge `(10.200, -105.85, —)` | hinge `(10.200, —, -105.85)` | `(0.72, 1.10, 0.12)` | no | Y-hinge, permanent drive, **swings inward** |
| `MOD-HOOK5-BAR` keepers | `(10.800, -105.55, 1.300)` | `(10.800, 1.300, -105.55)` | `(0.12, 0.20, 0.12)` ×2 | no | — |
| `MOD-HOOK5-BAR` | seated `(10.800, -105.55, 1.300)` | `(10.800, 1.300, -105.55)` | `(0.85, 0.09, 0.09)` | no | **38 kg carryable** |
| hook block rack | `(11.400, -104.0, 0.900)` | `(11.400, 0.900, -104.0)` | `(0.45, 0.06, 0.45)` | no | — |
| **hook block (`CAP-HOOK5`)** | seated `(11.400, -104.0, 1.260)` | `(11.400, 1.260, -104.0)` | `(0.30, 0.25, 0.30)` | no | **36 kg carryable** |

Cage outer footprint `x ∈ [8.00, 12.00]`, `z ∈ [-106.00, -102.00]`, walls to
`y = 2.90`. Roof hatch is the gap the roof slab leaves: `x ∈ [9.40, 12.00]` is
roofed, so the opening is `x ∈ [8.00, 9.40] minus the buttress` — restated
plainly: the buttress occupies `x ∈ [8.00, 9.40]` and the roof `x ∈ [9.40,
12.00]`, and the **hatch is cut in the roof** at `x ∈ [10.20, 11.40]`,
`z ∈ [-104.80, -103.20]`, a `1.20 × 1.60 m` opening.

**Clearances, in metres:**

- **the lock is the height.** Buttress top `2.90 m` above the apron.
  `kMantleMaximumRise = 1.85`. Deficit `1.05 m`. No command closes it
- **the key is the deck.** Belt top `1.38 m` → buttress top `2.90 m` =
  **`1.52 m`**, inside `[0.35, 1.85]`
- chest ray while on the deck sits at `1.38 + 0.90 = 2.28 m`; the buttress spans
  `y ∈ [0, 2.90]`, so the wall ray strikes it and the top ray strikes the same
  body — `probe_ledge`'s same-body rule is satisfied by construction
- deck east edge `x = 8.00`; a player at the edge stands at `x = 7.65`; buttress
  west face `x = 8.00` → **`0.35 m`**, far inside the `1.30 m` probe reach
- landing inset puts the mantle exit at `x = 8.47`; the buttress runs to
  `x = 9.40` → **`0.58 m`** clear of the far edge after the capsule radius
- hatch `1.20 × 1.60 m` against a `0.70 m` capsule → **`0.50 m`** on the tight axis
- drop through the hatch `2.90 m` → `7.54 m/s`, against `kLethalImpactSpeedMps
  = 20.0`. An ordinary platforming fall, and deliberately one-way
- doorway `x ∈ [10.00, 11.70]` = `1.70 m` wide × `2.20 m` tall against a
  `0.70 × 1.80 m` capsule → **`1.00 m`** spare on the tight axis
- door leaf shut spans `x ∈ [10.20, 11.64]`, leaving `0.20 m` at the hinge jamb
  and `0.06 m` at the latch jamb. Both are far under `0.70 m`, so a shut door is
  impassable, and neither boundary is coincident — the `BoxShape` convex-radius
  jam `WO-012` and `AS-001` both hit is designed out rather than discovered
- **door hinge sweep:** the leaf is `0.24 m` thick, so its trailing corner
  sweeps a `0.12 m` circle about the hinge. The hinge is therefore set back to
  `x = 10.200`, `0.20 m` clear of the jamb at `x = 10.000` — `0.08 m` more than
  the corner needs. `AS-001`'s `MOD-DOG-A` bound at `0.31 rad` and never
  travelled because its hinge sat flush with its jamb; that is pre-resolved here
  and must not be rediscovered
- cage `z ∈ [-106.0, -102.0]` vs `AS-002`'s cradle `z ∈ [-109.76, -107.36]` →
  **`1.36 m`**
- cage `x ∈ [8.0, 12.0]` vs the 9 t proof stand at `(7.0, —, -98.0)` → clear in
  both axes

**The belt window** — the one number that makes this a timing problem rather
than a walk:

```
deck spans z ∈ [c - 6.00, c + 6.00],  c = -96.0 + 9.0·sin(0.40 t)
the deck lies alongside the cage while c ∈ [-105.0, -98.0]
  i.e. sin(0.40 t) ≤ -0.2222
  = 57.1 % of the cycle = 8.97 s of every 15.708 s
```

The player does not have to jump at a moving target: they **board and ride**, and
the deck carries them into the window. `8.97 s` is then ample for one mantle. The
timing is real but not punishing, which is the correct dial for the first
capability in the game.

### 8.4 Mechanism

#### 8.4.1 The lock is geometry

There is no lock object. The cage is shut because its only opening is `2.90 m`
up and the player's mantle ceiling is `1.85 m`. That gap is `1.05 m` and no
input in the game narrows it. This is the cheapest possible honest lock and it
cannot be opened by a flag, because there is no flag to set.

#### 8.4.2 `MOD-HOOK5-BAR` — the door is opened by moving a body

The door leaf hangs on a vertical hinge under a **permanent opening drive**,
commanded once at build time and never touched again — the `MOD-DOG-A` contract
exactly:

```
add_vertical_hinge(south wall leaf, door, hinge point,
                   0.0, 1.35 rad, torque limit 6 000 N·m)
door_hinge->SetTargetAngularVelocity(+0.55 rad/s)   // at build, forever
```

**The leaf swings inward**, into the cage. That is what puts
`MOD-HOOK5-BAR` — a `38 kg` steel member seated across the *inside* of the door
in two keepers at `z = -105.55`, `0.30 m` north of the door plane — squarely in
its swing. The drive stalls against the bar. Lift the bar out and set it down
anywhere clear, and the door travels on its own.

An inward leaf with the bar inside is also the whole soft-lock defence: whoever
is in the cage can always move the bar and walk out, and no arrangement of
bodies outside can shut them in.

Rated so the bar holds and nothing else does:

```
bar seated, reaction arm from the hinge   r = 0.80 m
torque the drive applies                  6 000 N·m
force at the bar                          6 000 / 0.80 = 7 500 N
bar shear capacity (DESIGN TARGET)        60 000 N   -> 8.0x, holds
bar removed, remaining resistance         hinge friction only -> travels
```

The bar is **not** a lock that the player unlocks. It is a body in the way. No
predicate reads "bar removed"; the door travels because nothing is touching it.

#### 8.4.3 `CAP-HOOK5` — a carried body, and what carrying costs

The hook block is a `36 kg` body, inside the Atlas's `25–40 kg` unaided range.
It sits on a rack inside the cage. `CAP-HOOK5` is the derived predicate
`!hook_in_rack`, computed from the block's measured pose exactly as
`AS-007` specifies `CAP-BLIND`. It is never a stored bool.

```
pick_up(body):
    requires  no carry constraint currently exists
              |player − body| ≤ 1.20 m
              |v_body| ≤ 0.50 m/s
              player grounded and not mid-traversal
              Pick command this tick
    effect    PointConstraint between the player body and the carryable,
              world point = player position + facing·0.60 + (0, 0.30, 0)
              created with track_for_teardown = false

set_down():
    requires  a carry constraint exists
              Drop command this tick
    effect    remove it. The body falls and rests. Nothing is destroyed,
              nothing respawns, and it can be picked up again where it lies.
```

A `PointConstraint` fixes the shared point but leaves rotation free, so the
block genuinely swings from the carry point as the player walks — the same
property that makes `AS-001`'s pack swing under the hook rather than weld to it.

**Both hands are on it.** While a carry constraint exists,
`try_begin_ground_traversal` and the airborne hang grab both return false. Vault,
mantle and hang are all pull-ups; none of them is available to someone holding a
`36 kg` block in both arms.

This is a **player state derived from real constraint topology**
(`carrying = carry_constraint != nullptr`), not a mission bit, and it is the
reason the distinction below matters:

> `MOD-SKIN-LADDER-S` is **not blocked**. Its bodies are bit-for-bit what
> `AS-001` built; no collider changes, no envelope is added, no probe is
> filtered. What changed is that the player's hands are full. Set the block
> down and every rung is available again, immediately. The protocol forbids
> walling SKIN off to protect a puzzle; this does not wall it off, and
> falsifier 7 exists specifically to prove the geometry is untouched.

**Mass.** The `36 kg` loads the player body through the constraint, making the
carried system `121 kg`. Whether that measurably changes walk response depends on
whether the controller drives velocity or force — `kGroundAcceleration = 22.0`
suggests an acceleration model, but the implementation must **measure** it and
record what it found rather than assume. No claim about handling is made here.

#### 8.4.4 **GAP REPORT — the Atlas's crate coupling is not sitable**

Atlas §6 offers two example couplings: *"opened by moving the crate **or**
circling the belt."* §6 is explicit that these are *"legal intended uses, not
exclusive scripts"*, so authoring one is not a violation. But the reason the
other is absent should be on the record, because it is arithmetic and not taste.

A crate-blocked cage door requires the cage to sit inside the jib's working
reach. The hook hangs at the boom tip, so that reach is not a disc — it is the
**circle of radius exactly `12.000 m`** about `(0, —, -100.2)`, swept through
`±0.90 rad` of the `-Z` bearing:

```
slew -0.90 rad  ->  ( 9.400, —, -107.659)   east limit
slew -0.80 rad  ->  ( 8.608, —, -108.560)   AS-002's counterweight cradle
slew  0.00 rad  ->  (-0.000, —, -112.200)   the pack's rest pose
slew +0.90 rad  ->  (-9.400, —, -107.659)   west limit
```

`AS-002` places the cradle at `(8.608, —, -108.560)`, `1.20 m` from the east
limit. The entire east stretch of the reach circle is spoken for. The west
stretch at `(-9.400, —, -107.659)` collides with `MOD-SKIN-LADDER-S`'s head rung
at `x ∈ [-7.0, -5.0]`, `z ∈ [-107.7, -105.7]`, and siting a freight cage on the
SKIN line is worse than not siting it at all.

**Resolution:** close the slice on the belt coupling, which needs no jib reach at
all. Do not move the cradle — it is load-bearing for `AS-002`'s bascule and
re-siting it would invalidate a mechanism already specified down to its rope
length. If a later slice wants the crate coupling, the cheapest path is a second
door on the cage's north face reached from the belt's far stroke, not a
relocation.

The end state is the same either way: once the bar is moved the door is open
permanently, so the cage is a one-time problem regardless of which coupling
opened it.

### 8.5 Occupancy and interlocks

| Envelope | Effect |
|---|---|
| bar seated in its keepers | door drive stalls at `0 rad` |
| bar anywhere else | door travels to its `1.35 rad` stop |
| carry constraint exists | no traversal move begins; pick is refused |
| player on the deck, deck alongside the cage | buttress inside the mantle band |
| player on the apron | buttress `1.05 m` above the mantle band |
| block off its rack by more than `0.35 m` | `CAP-HOOK5` reads held |

`hook_in_rack`, `carrying` and `door_open` are **derived** from pose and
constraint topology for the HUD and the falsifiers. No simulation branch reads
any of them, with the single declared exception of `carrying` gating traversal —
which is itself read from `carry_constraint != nullptr`, a real object, not a bit.

### 8.6 Required causal path

See the Required causal path section above. The next file begins on
`MOD-HALL-DECK` with `CAP-HOOK5` held or set down there.

### 8.7 Support / traversal handoff

| Step | Member | source y | type | inherits v? |
|---|---|---|---|---|
| 0 | apron / ground plane | `0.00` | static | no |
| 1 | `MOD-INTAKE-BELT` deck | `1.38` | **kinematic** | **yes** — already in the moving-support set |
| 2 | cage west buttress | `2.90` | static | no — the mantle exits a moving support onto a static one |
| 3 | cage roof | `2.90` | static | no |
| 4 | cage floor (via the hatch) | `0.00` | static | no |
| 5 | apron, through the travelled door | `0.00` | static | no |
| 6 | `MOD-STAIR-A` → `MOD-HALL-DECK` | `→ 40.1872` | static | no |

Step 2 is the one with teeth: a mantle **off a moving support onto a static
one**. `WO-003` proved the moving-ledge and moving-hang cases
(`moving_hang_support`, `moving_ledge_vz`), so the machinery exists, but this
exact direction — leaving a kinematic deck for a fixed ledge — is not separately
falsified anywhere yet. Falsifier 2 covers it. Expect the exit velocity to carry
the belt's `v` for one tick and land the player slightly along the buttress; the
`0.58 m` of far-edge clearance in §8.3 exists for that.

### 8.8 Failure states

| Trigger | What the world does | What the player can still do | Must not happen |
|---|---|---|---|
| mantle attempted from the apron | probe returns invalid, nothing moves | board the belt | a "you need X" message standing in for geometry |
| mantle attempted outside the window | no ledge in reach; the deck is elsewhere | ride one more cycle, `15.7 s` | the cage drifting to meet the player |
| miss the mantle and fall off the deck | `1.38 m` fall to the apron | walk back and re-board | a rail that makes the belt safe |
| drop into the cage before moving the bar | player is inside with the door shut | move the bar and walk out; the door opens from inside | a soft-lock — Atlas §3 |
| pick attempted while already carrying | refused; nothing changes | set down first | two bodies on one carry point |
| carrying, traversal attempted | refused; nothing changes | set the block down | the block clipping through a ledge |
| block dropped from the roof | falls, rests on the apron, still pickable | pick it up at grade | the block despawning or resetting |
| block dropped off `MOD-HALL-DECK` | falls 40 m, rests on the apron | walk down and fetch it | the block being consumed or duplicated |

The cage is one-way inbound and free outbound **by construction**: the door's
drive is permanent and the bar is inside. That is the whole soft-lock defence,
and it is structural rather than a special case.

### 8.9 Recovery

Atlas §3. Nothing here can strand:

- **cannot time the belt** → `CAP-HOOK5` is simply not held yet. Nothing else in
  `AS-001`/`AS-002` needs it, and both braids to `+40 m` remain open
- **inside the cage** → the bar is inside with you; the door opens outward from
  within; and even leaving the bar seated, the hatch drop is the only cost
- **block left somewhere awkward** → it is a body at rest and can always be
  picked up again; it is never destroyed and never respawns
- **carrying and want to climb SKIN** → set it down; every move returns

### 8.10 Persist

**Persist version 3.** Carry state is runtime topology that outlives its bodies,
so it must survive commit and restore the way `WO-012`'s needle pins do:

```
carrying_entity     uint64   which body is on the carry point, 0 for none
hook_block_x,y,z    float    wherever the block was left
bar_x,y,z           float
door_hinge_angle    float
```

`restore_carry_topology()` mirrors `restore_needle_topology()`: after a restore,
reconcile the carry constraint against `carrying_entity` **before** the next tick
reads contacts, so the snapshot never mixes a restored pose with a stale
constraint. Import of a v2 blob assumes nothing carried, block on its rack, bar
seated, door shut.

### 8.11 Falsifiers (deterministic proof)

1. `wo016_apron_cannot_reach_the_cage` — spawn on the apron beside the cage.
   Walk at every face for 20 s, requesting traversal every tick. `ledge_available`
   is never true for a cage body, and the player never exceeds `y = 1.2`.
2. `wo016_belt_ride_reaches_the_roof` — board the deck, ride, mantle when the
   probe offers the buttress. `support_entity_id` becomes the buttress, then the
   roof, within two belt cycles (`≤ 32 s`).
3. `wo016_bar_is_not_a_flag` — with the bar seated, advance 30 s with every
   command mashed. The door hinge stays `≤ 0.05 rad`. Then lift the bar, carry it
   `2 m`, drop it: the hinge reaches `≥ 1.20 rad` **without any door command**,
   because no door command exists.
4. `wo016_block_is_a_body` — pick the block up. `hook_in_rack` false,
   `CAP-HOOK5` true, and the block's position tracks the player within `1.0 m`
   over 10 s of walking. Drop it: it falls, comes to rest, and is pickable again
   at its new position.
5. `wo016_carrying_blocks_traversal` — holding the block, stand at the proven
   `MantleApproach` ledge and request traversal for 5 s. `accepted_traversal_count`
   does not change. Set the block down, request once: it does.
6. `wo016_hands_free_restores_everything` — the same instance, block down, runs
   `AS-001`'s SKIN climb to `+24 m` unchanged, `15` mantles.
7. `wo016_skin_geometry_is_untouched` — assert the SKIN rung bodies' positions
   and extents are identical with the block held and with it stowed. The ladder
   is not walled off; only the player's state differs.
8. `wo016_hook_reaches_forty` — carrying the block, walk `MOD-STAIR-A` from the
   apron to `MOD-HALL-DECK`. `support_entity_id == MOD-HALL-DECK`, player
   `y ≥ 40.1872 + 0.70`, and `CAP-HOOK5` still held on arrival.
9. `wo016_prior_still_pass` — `AS-001`, `AS-002` and all kernel falsifiers `PASS`
   unchanged, the 9 t proof load still on the ground.

### 8.12 Exit state

- `CAP-HOOK5` is held, or set down somewhere the player chose; either is legal
- the cage door has travelled and stays travelled; the cage is open at grade
- the bar lies wherever it was dropped
- the player **can** stand on `MOD-HALL-DECK` at `40.1872 m` holding the block
- `MOD-NEEDLE-A/B`, their `48 m` racks, `MOD-NEEDLE-POCKETS` at `96 m`,
  `MOD-CAGE-1` and `MOD-GUIDE-RACK` **do not exist yet**
- the hall's east opening and `MOD-EAST-OUTRIGGER` do not exist
- everything `AS-001` and `AS-002` built still exists below
- persist is v3
- Fold-device execution remains unverified

---

**Stop. Do not begin the next file inside this one.**
Next file: `03_EXECUTION/ASCENT/AS-004_NEEDLE_SEAT.md`
