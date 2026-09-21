# SCRAPERX — WO-016 HOOK5 RACK (CAP-HOOK5 ACQUIRE)

**Work Order:** `WO-016`  
**Status:** READY after `WO-015_LEGAL_FORTY` source on `ScraperX-Claude`  
**Depends on:** `03_WORK_ORDERS/WO-014_INTAKE_RISE.md` and `03_WORK_ORDERS/WO-015_LEGAL_FORTY.md` in source; WO-011 crate sling remains kernel/campaign pre-placement; protocol `03_WORK_ORDERS/SECTION_PRE_RESOLVE_PROTOCOL.md` WO-016

> **Provenance.** Adopted from `ScraperX-Grok`, where this slice was authored as `WO-011_HOOK5_RACK`.
> Ticket numbers remapped per `SECTION_PRE_RESOLVE_PROTOCOL.md` §5.1. Geometry, ratings and
> falsifiers carry over as **DESIGN TARGET**: they were never proven in source on that branch
> (it has not compiled since 2026-09-20). Positions are re-sited against this branch's tower
> at source `(0, —, -150)`.

## Objective

Author `MOD-HOOK5-RACK` and close atlas §4 **`CAP-HOOK5` acquire**.

The player gets a portable 5 t hook block + rated sling from a locked cage at grade. The cage opens because the 4 t crate stops occupying the door, **or** because the player circles `MOD-INTAKE-BELT` to the west hatch.

This is not needle seating. This is not a second jib. This is not a retcon of the crate already being slung.

## Existing truth

HEAD `6a327ef` on `ScraperX-Claude` already has:

- B00 0–24 m intake (`WO-014_INTAKE_RISE`)
- B00 24–40 m stand (`WO-015_LEGAL_FORTY`): STAIR-A switchback, SKIN-S continuation, `MOD-HALL-DECK` lower landing, SKIN crown
- yard jib frozen: boom 12.00 m, boom height 11.50 m, SWL 5000 kg, winch `[2.80, 10.20]` m, slew `[-0.90, 0.90]` rad
- 4 t pack `kB00CrateKilograms = 4000`, source `(3.50, 2.28, 11.80)`, half `(1.10, 0.90, 1.15)`
- latch source `(4.00, 1.70, 12.40)`, half `(1.60, 0.12, 0.55)`, clear Y `3.08`
- belt source `(0.00, 1.20, 6.00)`, half `(2.00, 0.18, 6.00)`, stroke 18.00 m, `ω = 0.40` rad/s
- WO-011 exception in source: `attach_hook(HookLoad::Crate)` at IntakeRise spawn. That sling is **pre-placed on the crate**. It is not `CAP-HOOK5`
- kernel `kJibSlingMaxMeters = 0.60` stays the crate-hook distance limit
- player capsule: radius 0.35 m, standing half-height 0.90 m
- `kSkinCrownEntityId` is the last B00 24–40 entity. New IDs start after it
- Fold-device execution is not proven

Atlas does **not** freeze XYZ for `MOD-HOOK5-RACK`. Placement below is **DESIGN TARGET**, derived from the frozen crate/belt/latch AABBs so the two atlas open conditions are occupancy, not a flag.

## Authority

- Laws 2–7, 11–18, 21–27, 29, 32
- GDD §§7.2–7.4, 11, 15–17, 23–24
- Atlas §§3, 4 (`CAP-HOOK5`), 6 B00 module `MOD-HOOK5-RACK`, 7 grammar, 8.1, 12, 13
- TDD §§6, 8–9, 14, 19.1
- Execution Protocol §§3–7, 11–12
- `WO-015_LEGAL_FORTY.md` §8.12 exit
- `SECTION_PRE_RESOLVE_PROTOCOL.md` WO-016
- WO-011: crate may start pre-slung; hook must still be a real constraint

## Owner

Native 90 Hz C++/Jolt. Godot presents. Mission/UI observe predicates only.

## Allowed seam

New campaign bodies in the existing IntakeRise world: cage, door, west hatch opening, hook-block body, carry/attach constraint.

Reuse: kinematic occupancy (same class as `MOD-DOG-A`), finite distance-constraint hook (same class as WO-011), checkpoint blob (WO-009).

Do not retitle `KX-*`. Do not lengthen `MOD-YARD-JIB`. Do not add `MOD-NEEDLE-A/B`.

## Required causal path

Primary (move the crate):

    ACT[raise or slew the pre-slung 4 t pack with MOD-YARD-JIB until crate AABB clears the rack door AABB]
      → STATE[door occupancy stall false; door angle can travel]
      → WORLD[east door throat ≥ 0.90 m; cage floor reachable from the apron]
      → PLAY[player can take the hook block]

Alternate (circle the belt; freight may stay unsolved):

    ACT[walk west of MOD-INTAKE-BELT and/or ride the belt to the west hatch]
      → STATE[support identity is apron west aisle or belt; contact with west hatch opening]
      → WORLD[cage floor reachable without door travel]
      → PLAY[same acquire; crate pose and dog angle unchanged]

Acquire:

    ACT[pick up the hook block from the cage floor]
      → STATE[hook-block body leaves the rack; player hold or hip carry is real]
      → WORLD[CAP-HOOK5 is a world fact: portable 5 t attach hardware is not in the cage]
      → PLAY[legal attachment to declared padeyes ≤ 5 t; `WO-017_NEEDLE_SEAT` may consume this]

Illegal:

    ACT[mission_flag cap_hook5 = true]
    ACT[animation opens the cage]
    ACT[take the crate's pre-placed sling and call it CAP-HOOK5]

## Forbidden shortcuts

- `rack_open` / `has_hook5` flags as WORLD
- deleting the crate sling and pretending the rack was always the jib hook
- invisible wall that blocks SKIN or the west aisle until freight is solved
- lengthening the yard jib so the rack can live at +40 m
- putting `MOD-HOOK5-RACK` on the hall soffit
- teleporting the block into inventory with no body
- mesh-swap of a locked cage into an open cage with no door travel / hatch
- loading `MOD-NEEDLE-POCKETS` / `MOD-NEEDLE-A/B` / `MOD-CAGE-1`
- claiming Fold-device execution
- resetting crate/dog on acquire

## Implementation scope

Native IntakeRise extension + falsifiers; Godot twins of cage/door/block; persist fields listed in §8.10; CI still runs B00 0–24 and 24–40 tests.

Climbable vs filler is in §8.3.

## Out of scope

`WO-017_NEEDLE_SEAT` (next file). Needle pockets at 96 m. Needle racks at 48 m. Using the yard jib as a 48 m hoist (frozen boom still cannot). `MOD-CAGE-1`. Fold 45 FPS. Art/VO. Hard-fail missions. Reopening WO-009. Changing frozen 0–24 or 24–40 constants.

## Proof path

This planning pass does not execute. Later coding must exercise:

1. Native tests in §8.11
2. Source inspection: crate sling still attaches at spawn; new rack IDs exist; 0–24 and 24–40 constants unchanged
3. Godot mirrors door angle, block pose, hold state at Fold aspect 1080×928
4. One screenshot: cage, crate, belt, and both openings readable at grade
5. Second screenshot: player holding the block; cage empty; crate sling still on the pack if freight unsolved
6. Android arm64 APK still contains `libscraperx_native.so`
7. WO-011–008, B00 0–24, and B00 legal-forty tests stay green

Claim classes stay distinct (Law 29).

## Completion

- `MOD-HOOK5-RACK` exists as collision-honest cage + door + west hatch
- door cannot travel while crate AABB overlaps door AABB
- door can travel after the crate is lifted or slewed off that overlap
- west hatch is reachable by circling/riding the belt without moving the crate
- hook block is a real body, DESIGN TARGET 36 kg, pickable from the cage floor
- `CAP-HOOK5` is derived from block pose (not in rack), never from a mission bool
- crate pre-sling still lifts the 4 t pack without `CAP-HOOK5`
- without `CAP-HOOK5`, campaign code must not attach the jib to any padeye other than the pre-slung crate
- acquire persists across commit/reload
- SKIN / 24–40 routes still work if the player never takes the block
- Fold install / on-device play remain unverified

## Result record

pending. This pass does not execute.

---

## Mechanical close

### 8.1 Identity

| Field | Value |
|---|---|
| Band | B00 Apron and Intake |
| Slice authored | grade rack / capability only. No new elevation. Atlas z ≈ 0–3 m at the intake |
| Chain | K0 leftover capability. K0 PLAY (+40 m) is already closed. K1 is not this file |
| Live braids | SHAFT (return by STAIR-A), SKIN (return by SKIN-S), belt (moving support). FLOW not live |
| Transfer Plate | none new. Apron and +40 soffit already exist |
| Modules allowed | `MOD-HOOK5-RACK`, inherited `MOD-YARD-JIB`, `MOD-INTAKE-BELT`, `MOD-DOG-A`, `MOD-APRON` |
| Modules forbidden | `MOD-NEEDLE-POCKETS`, `MOD-NEEDLE-A/B`, `MOD-CAGE-1`, `MOD-GUIDE-RACK`, `MOD-EAST-OUTRIGGER`, any atlas band B02–B11 ID |
| Capability authored | `CAP-HOOK5` |

### 8.2 Entry state

Inherited from `WO-015_LEGAL_FORTY.md` §8.12 and source:

- player **can** already stand at source `(-1.76, 40.20, 28.78)` on `MOD-HALL-DECK`, or on SKIN crown, or still be on the apron
- 24–40 STAIR-A and SKIN-S are two-way static. Descent to grade is legal. Do not add a one-way drop
- crate/dog/jib/belt are whatever the player left: crate on latch **or** crate clear, dog travelled
- `attach_hook(HookLoad::Crate)` is already true at IntakeRise spawn. Leave it
- `MOD-HOOK5-RACK` does not exist yet
- persist blob already stores player, crate, jib, dog, belt phase
- automatic commit at y ≥ 40 already exists

If the player is at +40 with freight unsolved, they go down SKIN or STAIR-A 24–40 (grade dog still only blocks treads 1–N). That is backtracking on real stairs, not a loading screen.

### 8.3 Geometry

Frame: `source.x = atlas.x`, `source.y = atlas.z`, `source.z = atlas.y`.

Frozen crate AABB (do not change):

    center source (3.50, 2.28, 11.80)
    half (1.10, 0.90, 1.15)
    x [2.40, 4.60], y [1.38, 3.18], z [10.65, 12.95]

Frozen belt AABB at rest, then translates in Z:

    rest center (0.00, 1.20, 6.00), half (2.00, 0.18, 6.00)
    x [-2.00, 2.00]
    z_center(t) = 6.00 + 9.00 * sin(0.40 * t) ∈ [-3.00, 15.00]
    belt top y = 1.38

#### Members this slice adds (all DESIGN TARGET)

| ID | Role | Atlas (x, y, z) | Source (x, y, z) | Extents | Climbable? | Rating |
|---|---|---|---|---|---|---|
| `HOOK5-CAGE` | bar cage, floor + walls, east door, west hatch | `(2.00, 12.00, 1.50)` | `(2.00, 1.50, 12.00)` | half `(0.75, 1.20, 0.85)` | floor yes | player + 40 kg. DESIGN TARGET 5 kN cage floor |
| `HOOK5-DOOR` | kinematic east door | closed `(3.65, 12.00, 1.50)` | closed `(3.65, 1.50, 12.00)` | half `(0.90, 1.10, 0.70)` | no (filler gate) | n/a — occupancy body |
| `HOOK5-HATCH-W` | opening, not a body | west face of cage | plane x = 1.25, y [0.40, 2.30], z [11.55, 12.45] | 0.90 m wide × 1.90 m high | pass-through | n/a |
| `CAP-HOOK5` block | portable snatch block + sling coil | seated `(2.00, 12.00, 0.62)` | seated `(2.00, 0.62, 12.00)` | half `(0.18, 0.22, 0.18)` | no (carryable) | see §8.4 |
| cage floor (part of cage) | standable | same cage plan, top y = 0.40 | source y_top = 0.40 | half-y 0.10 if split | yes | player + block |

Cage AABB:

    x [1.25, 2.75], y [0.30, 2.70], z [11.15, 12.85]

East door **closed** AABB:

    x [2.75, 4.55], y [0.40, 2.60], z [11.30, 12.70]

Crate ∩ door (crate on latch):

    x [2.75, 4.55] ∩ [2.40, 4.60] = [2.75, 4.55]  → overlap Δx = 1.80 m
    z [11.30, 12.70] ∩ [10.65, 12.95] = [11.30, 12.70] → overlap Δz = 1.40 m
    y [0.40, 2.60] ∩ [1.38, 3.18] = [1.38, 2.60] → overlap Δy = 1.22 m

Occupancy is real. Door cannot travel.

West hatch (always an opening in the west wall; no door body):

    x = 1.25 (cage west face)
    y [0.40, 2.30]  height 1.90 m
    z [11.55, 12.45]  width 0.90 m
    capsule diameter 0.70 m → residual 0.20 m
    standing height 1.80 m vs hatch 1.90 m → residual 0.10 m

Cage sits on the east edge of the belt deck (belt east face x = 2.00; hatch at x = 1.25 is over the belt).

West wall is omitted. North wall, south wall, and roof are filler. East wall is the kinematic door.

#### Why this XY (not invented tower)

Atlas: cage is opened by **moving the crate** or **circling the belt**. Both objects already exist at this plan. Putting the rack on the +40 soffit would require a new hoist the frozen 12 m jib cannot perform. Grade only.

#### Inherited, not rebuilt

| ID | Climbable? |
|---|---|
| crate (pre-slung) | yes if top is stood on (already true) |
| latch / dog | kinematic grade gate |
| belt | moving support, riding legal |
| STAIR-A 0–40, SKIN-S, hall soffit | yes, unchanged |
| mill piers | sequence-break massing, unchanged |

#### Entity IDs (DESIGN TARGET)

Assign after `kSkinCrownEntityId` (source: 284). Do not reuse 0.

- `kHook5CageEntityId` — 1 (floor; climbable)
- `kHook5DoorEntityId` — 1
- `kHook5BlockEntityId` — 1

Hatch is an opening, not an entity. North/south/roof filler may use entity 0.

#### Clearance (meters)

| Location | Opening | Capsule | Residual |
|---|---|---|---|
| east door throat after 1.20 rad swing | 1.80 m plan × sin(1.20) ≈ 1.68 m | 0.70 | 0.98 |
| west hatch width | 0.90 | 0.70 | 0.20 |
| west hatch height | 1.90 | 1.80 standing | 0.10 |
| cage interior x | 1.50 | 0.70 | 0.80 |
| cage interior z | 1.70 | 0.70 | 1.00 |
| west aisle x < −2.20 | belt west face at −2.00, walk at x = −3.20 | 0.70 | apron is open |
| headroom in cage | 2.40 interior | 1.80 | 0.60 |

West-aisle circle path (DESIGN TARGET waypoints, source):

    1. apron `(-3.20, 0.00, 2.00)`
    2. north `(-3.20, 0.00, 12.00)`  — west of belt
    3. onto belt or along belt north when aligned, then hatch at `(1.25, 0.40–2.30, 12.00)`

Riding the belt is also legal: when `z_center ≥ 6` the belt top overlaps the hatch in z. Player on belt (moving support) steps east-north into the hatch. Inherited belt velocity applies (Law 5).

### 8.4 Mechanism

#### A. East door (new kinematic)

Bodies: `HOOK5-DOOR` kinematic box. Hinge: vertical, source `(2.75, 1.50, 11.30)` (south-east cage corner). Travel: `θ ∈ [0.00, 1.20]` rad, sense +Y. Speed: `0.70` rad/s.

Mass DESIGN TARGET 42 kg (unaided shove 200–400 kg; this is a door, not freight).

Commands: none on the pendant. Door travels when crate occupancy is clear (same class as `MOD-DOG-A` retract). Player walking into the free door may also drive `θ`. Do not add a new Drive channel on the jib.

Occupancy stall: if crate AABB overlaps the **closed** door AABB, `θ` is held.

Not allowed to write: `cap_hook5`, crate transform, dog angle.

Closed-door stall arithmetic:

    door center (3.65, 1.50, 12.00) half (0.90, 1.10, 0.70)
    crate start (3.50, 2.28, 11.80) half (1.10, 0.90, 1.15)
    overlap at spawn: yes (Δx 0.15, Δy 0.78, Δz 0.20 vs sums 2.00 / 2.00 / 1.85)
    free when crate center y ≥ 3.51 (bottom clears door top 2.60) or crate xz leaves the door AABB

Latch clear Y 3.08 is **not** sufficient by itself (crate bottom at that pose is 2.18, door top 2.60).

#### B. Hook block (new body)

Mass DESIGN TARGET **36 kg**. Weight `36 × 9.81 = 353 N`.

Sling: DESIGN TARGET WLL = 5000 kg, working tension `5000 × 9.81 = 49050 N` (matches jib SWL). Portable sling length DESIGN TARGET **1.20 m** when used on a new padeye. The crate's existing kernel sling stays **0.60 m**. Do not change `kJibSlingMaxMeters` for the crate hook.

Padeyes this slice declares:

| Padeye | Owner | Compatible now? |
|---|---|---|
| crate top (existing) | 4 t pack | already slung by WO-011 pre-placement; `CAP-HOOK5` is **not** required |
| hook-block self eye | portable block | carry only in this slice |
| needle padeyes | atlas band B01 / `WO-017_NEEDLE_SEAT` | **forbidden this file**. Predicate for next file: attach only if `CAP-HOOK5` true |

Carry: when player is in the cage (plan in cage AABB, y < 2.70) and context-acquires within 1.20 m of block center, block becomes a kinematic carry at source offset `(0.35, 0.20, 0.40)` from player (right hip). Drop: context again; block becomes dynamic.

`CAP-HOOK5` WORLD predicate (derived, not stored as the cause):

    cap_hook5 = (block is not in the seated rack pose)
                AND (player holds it OR it is distance-constrained to a declared padeye)

Seated rack pose: source `(2.00, 0.62, 12.00)` ± 0.15 m.

#### C. Inherited jib (not re-specified)

    τ = 12 × 5000 × 9.81 = 588600 N·m
    winch force = 49050 N
    4 t → F = 39240 N  (legal)
    9 t must still stall
    max hook y ≈ 8.70 m  (still cannot reach +40 or +48)

The jib may still lift the pre-slung crate without `CAP-HOOK5`. That is WO-011. This file must not break it.

Energy this slice adds: door rotation is kinematic occupancy, not a motor. Block lift work = `353 N × Δh`. If Δh from floor 0.40 m to hip ~1.10 m, `W = 353 × 0.70 ≈ 247 J`. No fatigue solver.

### 8.5 Occupancy and interlocks

| Envelope | Body | Effect |
|---|---|---|
| door closed AABB | 4 t crate | stalls door `θ` |
| latch clear Y 3.08 | crate | existing dog stall; **not** sufficient alone to free the door |
| west hatch | none | never stalled by crate. Crate min x = 2.40; hatch at x = 1.25 |
| cage interior | door when `θ ≈ 0` | east entry blocked; west hatch still open |
| jib SWL / travel | jib | unchanged |
| dog at grade | dog | unchanged; does not occupy the rack |

No `rack_unlocked` flag. Tests may derive `door_overlap == false`. HUD may show “CAGE CLEAR” from that overlap. WORLD is AABBs / contacts.

### 8.6 Required causal path

Primary:

    ACT[pendant raise until crate AABB clears door AABB]
      → STATE[door occupancy false; door travels to θ ≥ 1.00 rad]
      → WORLD[east throat open; cage floor standable from apron]
      → PLAY[block pickable]

    ACT[pick up block]
      → STATE[kHook5BlockEntityId carried; seated-pose false]
      → WORLD[CAP-HOOK5 true]
      → PLAY[legal attach to future padeyes ≤ 5 t]

Belt alternate:

    ACT[circle west of belt and/or ride belt into west hatch]
      → STATE[support = belt or cage floor; door may stay at θ = 0]
      → WORLD[block pickable; crate xz and dog angle unchanged within 0.45 m / 0.15 rad]
      → PLAY[same CAP-HOOK5; K0 freight may remain unsolved]

Illegal:

    MISSION_FLAG → WORLD[cage open]
    ACT[take crate sling] → PLAY[CAP-HOOK5]
    ACT[stand at +40] → PLAY[CAP-HOOK5]

### 8.7 Support / traversal handoff

Primary (from +40 or apron):

| Step | Member | Source Y | Type | Inherit v? |
|---|---|---|---|---|
| 0 | hall / SKIN / apron (entry) | 40 or ~0 | static | no |
| 1 | STAIR-A or SKIN descent if needed | 40 → 0 | static | no |
| 2 | apron / belt catwalk | ~0 | static | no |
| 3 | pendant station (if using jib) | 1.58 | static | no |
| 4 | cage floor | 0.40 | static | no |
| 5 | carry block; walk out east door or west hatch | 0–2 | carry, not a floor | n/a |

Belt alternate inserts: belt top y=1.38, kinematic, inherit v = yes, then hatch, then cage floor.

Handoff is contact. No teleport of player or block.

### 8.8 Failure states

| Trigger | World | Player can | Must not |
|---|---|---|---|
| shove door while crate overlaps | door `θ` held | circle belt; use jib | auto-open because “they need the hook” |
| jib 9 t | stall, crate stays | belt circle | door frees |
| jib winch at 2.80 m | hook y ≈ 8.70 | operate within limits | free the door by a flag |
| miss west hatch from belt | fall to apron / ride continues | chute if high; here fall is short | catch net |
| drop block on belt | block is dynamic on moving support | pick up again | delete block |
| drop block down the well | gravity; may be lost | last commit if unrecoverable | auto-return to cage |
| presentation `has_hook = true` | nothing | no attach | needle attach in `WO-017_NEEDLE_SEAT` |
| take block, crate still slung | both facts true | lift crate **and** hold CAP-HOOK5 | stealing the crate sling |
| stay at +40, never descend | rack stays locked/full | SKIN/STAIR still live | spawning the block at 40 m |

### 8.9 Recovery

- east door stalled: west hatch / belt circle. Does not require freight
- west hatch missed: east door after moving crate, or re-ride belt
- block dropped on apron: pick up; it is 36 kg
- block lost in an unrecoverable volume: last commit. Do not auto-repair
- player at +40 without the block: descend real STAIR-A / SKIN. Do not strand
- freight unsolved forever: CAP-HOOK5 still available via belt. SKIN braid stays legal

Atlas §3: a Transfer Plate keeps one recovery that does not need the lost capability. Apron and +40 soffit stay standable if the player never takes the hook.

### 8.10 Persist

Consumes WO-009 persist. Add to the blob (additive fields; export version 2; import accepts version 1 with hook5 defaults = in rack):

- `hook5_x, hook5_y, hook5_z`
- `hook5_held` (uint8)
- `hook5_in_rack` (uint8)
- `hook5_door_angle` (double)

Derived `cap_hook5` is **not** stored as the cause. Reload restores block pose/hold/door; predicate is recomputed.

Crate/jib/dog/player/belt fields unchanged. Do not reset the rack on “band load.”

Commit: no new elevation commit. Existing y≥40 commit and apron dwell still apply. If the player acquires the block, the next dwell commit must include the new fields.

### 8.11 Falsifiers (deterministic proof)

1. `b00_hook5_door_stalled_by_crate` — IntakeRise spawn, crate on latch, wait 2 s; `θ` stays < 0.10 rad. Fail if door opens.
2. `b00_hook5_door_frees_after_lift` — pendant raise until crate AABB clears door; door then reaches `θ ≥ 1.00` rad. Fail if door stays stalled after overlap is gone.
3. `b00_hook5_crate_sling_still_works` — same lift uses existing crate constraint; `jib_hook_load == Crate`. Fail if lift requires `cap_hook5`.
4. `b00_hook5_belt_circle_without_freight` — crate stays on latch, dog ≈ 0; player reaches cage floor via west hatch; crate xz delta < 0.45 m; dog delta < 0.15 rad.
5. `b00_hook5_acquire_is_a_body` — pick up; `kHook5BlockEntityId` is not at seated pose; player hold true. Fail if only a bool flipped and the body stays in the cage.
6. `b00_hook5_flag_is_not_world` — `commit_checkpoint()` without moving the block must leave seated pose and `cap_hook5` false.
7. `b00_hook5_reload` — acquire, dwell commit, export/import; block still held or at dropped pose; door angle matches; crate/dog match.
8. `b00_hook5_skips_needles` — after acquire, no campaign `MOD-NEEDLE-*` bodies exist in IntakeRise.
9. `b00_forty_and_intake_still_pass` — existing B00 0–24 and 24–40 tests remain `PASS`.
10. Kernel WO-011–008 remain `PASS` (kernel needle attach is **not** IntakeRise and must not be gated by campaign `CAP-HOOK5`).

### 8.12 Exit state

The next file may assume:

- `MOD-HOOK5-RACK` exists at source `(2.00, 1.50, 12.00)`
- player **can** possess `CAP-HOOK5` (block out of rack) by door-after-lift **or** belt hatch
- player may also have skipped the rack; then `CAP-HOOK5` is false and `WO-017_NEEDLE_SEAT` must not attach needles to the jib
- crate may still be pre-slung; that is not `CAP-HOOK5`
- yard jib still cannot hoist to 48 m or 96 m
- +40 m stand still exists
- `MOD-NEEDLE-A/B` and pockets are **not** in this world yet
- frozen 0–24 and 24–40 numbers are unchanged

Next file inherits **whether** `CAP-HOOK5` is true as a world fact to consume, not as a job to re-author the rack.

---

**Stop. Do not begin the next file inside this one.**  
Next file: `03_WORK_ORDERS/WO-017_NEEDLE_SEAT.md`
