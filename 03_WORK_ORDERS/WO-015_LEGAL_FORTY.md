# SCRAPERX — WO-015 LEGAL FORTY (~+24 m TO FIRST STAND AT +40 m)

**Work Order:** `WO-015`  
**Status:** READY after `WO-014_INTAKE_RISE` source on `ScraperX-Claude`  
**Depends on:** `03_WORK_ORDERS/WO-014_INTAKE_RISE.md` implemented in source; the kernel falsifiers (WO-000–WO-003, WO-008–WO-009, WO-011–WO-013) remain green; protocol `03_WORK_ORDERS/SECTION_PRE_RESOLVE_PROTOCOL.md`

> **Provenance.** Adopted from `ScraperX-Grok`, where this slice was authored as `WO-010_LEGAL_FORTY`.
> Ticket numbers remapped per `SECTION_PRE_RESOLVE_PROTOCOL.md` §5.1. Geometry, ratings and
> falsifiers carry over as **DESIGN TARGET**: they were never proven in source on that branch
> (it has not compiled since 2026-09-20). Positions are re-sited against this branch's tower
> at source `(0, —, -150)`.

## Objective

Close atlas §7 **K0 PLAY**: the player first stands with stable support at atlas `z ≥ 40 m`.

This is the leftover 24–40 m of atlas band `B00 - Apron and Intake`. It is not a new kernel fixture, not `MOD-HOOK5-RACK`, and not atlas band B01 needles (`WO-017_NEEDLE_SEAT.md`).

Two atlas exits close under frozen B00 ratings:

- `MOD-STAIR-A` continues from the +24 m handoff to `MOD-HALL-DECK` lower landing
- `MOD-SKIN-LADDER-S` continues to the same landing

The third atlas exit does **not** close. Report is in Mechanical close §8.4. Do not lengthen `MOD-YARD-JIB`.

## Existing truth

HEAD on `ScraperX-Claude` contains B00 0–24 m (`WO-014_INTAKE_RISE`). Native 90 Hz Jolt owns the dog, 12 m / 5 t jib, 18 m belt, 68 STAIR-A treads, 48 SKIN rungs plus hang/decks, and the +24 m handoff body.

Frozen source consumed by this file:

- handoff `kB00Handoff*` source `(10.40, 24.00, 38.40)` half `(4.80, 0.20, 3.40)`
- STAIR-A 0–24: `kB00StairX = 10.40`, `kB00StairY0 = 0.00`, `kB00StairDY = 0.353`, `kB00StairZ0 = 14.20`, `kB00StairDZ = 0.320`, `kB00StairHalfX = 1.30`, `kB00StairHalfZ = 0.18`, count `kFillStairCount = 68`
- last 0–24 tread t=68: source top `(10.40, 24.004, 35.96)`
- SKIN: `kB00SkinX = -17.00`, `kB00SkinZ = 16.60`, rise 0.450 m for rungs 1–40 then 0.40 m jog, `kIntakeSkinCount = 48`
- jib: boom 12.00 m, boom height 11.50 m, SWL 5000 kg, winch `[2.80, 10.20]` m, slew `[-0.90, 0.90]` rad
- dog hinge source `(11.90, 1.45, 13.50)`, retract `+1.45` rad — **grade mechanism**, not a +24 gate
- player capsule: radius 0.35 m, cylinder half-height 0.55 m, standing half-height 0.90 m
- gravity 9.81 m/s², 90 Hz, autocommit dwell 0.35 s
- spawn `InitialSpawn::IntakeHandoff` already exists at the +24 m deck
- Godot `UnfinishedFortyA/B/C` at source z 41.90 / 42.28 / 42.66 are unfinished silhouette, not frozen STAIR-A

Fold-device execution is not proven. Kernel `KX-*` IDs remain regression substrate.

## Authority

- Laws 2–7, 9–10, 12–18, 21–27, 29, 32
- GDD §§3, 4, 6, 7.2–7.4, 9, 11, 16, 17, 23, 24, 28
- Atlas §§2, 3, 5 (B00 0–40), 6 B00, 7 K0, 8.1, 8.3, 12, 13
- TDD §§6, 8.2–8.4, 14.1
- Execution Protocol §§3–7, 11–12
- `WO-014_INTAKE_RISE.md` Completion / out-of-scope (+24–40 m)
- `SECTION_PRE_RESOLVE_PROTOCOL.md` WO-015

## Owner

Native 90 Hz C++/Jolt simulation. Godot presents authoritative state. Mission/UI may observe predicates only.

## Allowed seam

Extend the existing B00 IntakeRise world with new static climbable members. Reuse kernel primitives: contact-ranked support, athletic traversal, checkpoint commit. Do not retitle `KX-*`. Do not add a second jib. Do not add a new actuator.

## Required causal path

Primary (STAIR-A, dog already travelled in 0–24 **or** player already on the +24 deck):

    ACT[walk opened MOD-STAIR-A from the +24 m handoff, then the 24–32 west flight, then the 32–40 south flight]
      → STATE[support identity is STAIR-A treads, then landing 32, then treads, then MOD-HALL-DECK lower landing]
      → WORLD[stable support at atlas z ≥ 40 m]
      → PLAY[atlas band B01 is physically reachable; automatic commit on 0.35 s grounded dwell]

Alternate (SKIN, freight unsolved):

    ACT[climb MOD-SKIN-LADDER-S extension from the +24 m skin decks]
      → STATE[support identity is SKIN rungs, then SKIN crown deck, then MOD-HALL-DECK lower landing]
      → WORLD[stable support at atlas z ≥ 40 m; crate pose and dog angle unchanged]
      → PLAY[same stand and commit; K0 freight may remain unsolved]

Illegal third atlas sentence (does not close — see §8.4):

    ACT[ride MOD-YARD-JIB onto the +40 m timber soffit]
      → cannot produce WORLD[support at atlas z ≥ 40 m] under frozen 12 m / 11.50 m jib

Hybrid (legal sequence break): SKIN to +24, then STAIR-A 24–40. The grade dog does not occupy the 24–40 throat.

## Forbidden shortcuts

- duplicating `WO-014_INTAKE_RISE.md` or rebuilding 0–24 m
- renaming `KX-*` as `MOD-HALL-DECK`
- `gate_open` / `reached_forty` / mission flags as the 40 m predicate
- invisible walls on mill_obstacle massing to “protect” the stair
- lengthening `MOD-YARD-JIB` boom or winch to fake the third atlas exit
- teleport from +24 to +40
- chute as powered lift
- loading atlas band B01 needles, `MOD-CAGE-1`, or `MOD-HOOK5-RACK`
- treating Godot stacked-tower `BayFloorRing` at y=40 as native support
- leaving `UnfinishedFortyA/B/C` as the only 24–40 treads
- blocking SKIN until the dog travels
- claiming Fold-device execution

## Implementation scope

Native B00 world extension + falsifier tests; Godot presentation twins; CI Fold-aspect screenshot of the 24–40 rise and the +40 stand; Android arm64 APK still contains `libscraperx_native.so`.

Climbable vs filler is in Mechanical close §8.3.

## Out of scope

`WO-016_HOOK5_RACK.md` / `CAP-HOOK5` acquire (next file). Atlas band B01 `MOD-NEEDLE-POCKETS`, `MOD-NEEDLE-A/B`, `MOD-CAGE-1`, `MOD-GUIDE-RACK`, `MOD-EAST-OUTRIGGER`. Atlas bands B02–B11. Fold 45 FPS certification. Art/VO. Hard-fail missions (atlas §8.2: none). Reopening WO-009. Changing frozen 0–24 constants.

## Proof path

This planning pass does not execute. The later coding pass must exercise:

1. Native deterministic tests named in §8.11
2. Source inspection: new members exist; 0–24 constants unchanged
3. Godot runtime mirrors native support at Fold aspect 1080×928
4. One screenshot: 24–40 mechanism readable as continuation of the same mill stair / skin, not a new toy level
5. Second screenshot: player-equivalent camera at the +40 m soffit, unfinished atlas band B01 volume visible above
6. Android arm64 APK contains `libscraperx_native.so`
7. The kernel falsifiers (WO-000–WO-003, WO-008–WO-009, WO-011–WO-013) and the B00 0–24 tests remain green

Claim classes stay distinct (Law 29).

## Completion

- player can obtain stable grounded support at source `y ≥ 40.00` m (atlas `z ≥ 40.00` m) on `MOD-HALL-DECK` lower landing **or** the SKIN crown deck **or** any other collision-honest body at that elevation
- STAIR-A 24–40 is real support after the +24 m handoff; it is not a painted stripe
- SKIN 24–40 reaches the same elevation without mutating crate pose or dog angle
- moving only a mission/presentation variable cannot create the +40 m support
- `MOD-YARD-JIB` still cannot lift a body to y=40 (overload / travel / geometry all still bind)
- 0–24 dog still pins the **grade** stair throat when the crate occupies the latch
- automatic commit fires on first 0.35 s stable stand at y≥40; reload restores crate/dog/jib/belt/player
- atlas band B01 volume is visible and unfinished; needles are not seated because they are not in this slice
- Fold install / on-device play remain unverified

## Result record

pending. This pass does not execute.

---

## Mechanical close

### 8.1 Identity

| Field | Value |
|---|---|
| Band | B00 Apron and Intake |
| Slice authored | atlas `z = 24.00–40.00` m (source `y = 24.00–40.00`) |
| Chain | K0 Intake, PLAY only. ACT/STATE/WORLD of the crate/dog are already 0–24 |
| Live braids | SHAFT (stair), SKIN. FLOW is not live in B00 |
| Transfer Plate | Apron slab (already present). +40 m soffit is a marked refuge, not a new TP |
| Modules allowed | `MOD-STAIR-A` (extension), `MOD-SKIN-LADDER-S` (extension), `MOD-HALL-DECK` **lower landing only**, existing `MOD-APRON` / `MOD-YARD-JIB` / `MOD-DOG-A` / `MOD-INTAKE-BELT` as inherited state |
| Modules forbidden | `MOD-HOOK5-RACK`, `MOD-NEEDLE-POCKETS`, `MOD-NEEDLE-A/B`, `MOD-CAGE-1`, `MOD-GUIDE-RACK`, `MOD-EAST-OUTRIGGER`, `MOD-CW-STACK`, any atlas band B02–B11 ID |

### 8.2 Entry state

Inherited from `WO-014_INTAKE_RISE` Completion and source:

- player has (or can have) stable support at the +24 m handoff, source `(10.40, 24.00, 38.40)` = atlas `(10.40, 38.40, 24.00)`
- support identity at entry: `kIntakeHandoffEntityId` and/or catwalk IDs `kCatwalkEntityIdBegin+5/+6` and/or SKIN decks
- `MOD-STAIR-A` 68 treads exist and end on the south edge of that landing
- `MOD-SKIN-LADDER-S` 48 rungs + hang rails + three skin decks exist and can reach y>23.4 without freight
- dog and crate are **either**: crate clear, dog travelled (**or**) crate on latch, dog blocking the **grade** throat. Both are legal entry. 24–40 must work in both
- jib frozen as 12 m / 5 t / winch `[2.80, 10.20]`
- belt may still run
- persist blob already stores player, crate, jib, dog, needle/sump/cage kernel fields (kernel fields unused in IntakeRise)

Godot `UnfinishedFortyA/B/C`, `BrokenRise`, `TornPlate40` are **not** entry support the next coder should extend. They are silhouette. This slice replaces A/B/C.

### 8.3 Geometry

Frame mapping (every row): `source.x = atlas.x`, `source.y = atlas.z`, `source.z = atlas.y`.

Player capsule (frozen): diameter 0.70 m, standing height 1.80 m.

**Why a switchback (not more +Z run):**  
If STAIR-A kept `dz = +0.320` for 16.00 m of rise from the landing north edge `source z = 41.80`, arrival would be `z = 56.52`, which is 11.52 m north of the 90 m primary frame face at `z = +45`. Atlas `MOD-HALL-DECK` lives in the frame. Do not invent extra tower. Switch west, then south, using the frozen rise/run.

Replace Godot/native stub treads at `(10.40, 24.40, 41.90)`, `(10.40, 24.80, 42.28)`, `(10.40, 25.20, 42.66)`.

#### Climbable members this slice adds

| ID | Role | Atlas (x, y, z) origin | Source (x, y, z) | Extents / rule | Rating |
|---|---|---|---|---|---|
| `STAIR-A` flight 24–32 | mill stair, run west | starts `(5.42, 38.40, 24.00)` | starts `(5.42, 24.00, 38.40)` | 23 treads; see formula | player + 40 kg portable. DESIGN TARGET 5 kN service per tread |
| `LANDING-32` | mill half-landing | `(-1.62, 38.40, 32.119)` | `(-1.62, 32.119, 38.40)` | half `(2.40, 0.20, 2.40)` | same |
| `STAIR-A` flight 32–40 | mill stair, run south | starts `(-1.76, 35.82, 32.119)` | starts `(-1.76, 32.119, 35.82)` | 23 treads; see formula | same |
| `MOD-HALL-DECK` lower landing | timber-on-steel soffit; B00 exit | `(-1.76, 28.78, 40.00)` | `(-1.76, 40.00, 28.78)` | half `(8.00, 0.20, 6.00)`; top source y = 40.20 | player + pack; DESIGN TARGET 15 kN/m² timber on joists (reduced-order: rigid static until atlas band B01 seats needles) |
| `SKIN-S` rungs 24–40 | exposed climb | `(-17.00, 22.40, y)` | `(-17.00, y, 22.40)` | 40 rungs; see formula | player only; not a freight path |
| `SKIN-S` crown deck | west-to-hall walkway at 40 m | `(-8.50, 25.50, 40.00)` | `(-8.50, 40.00, 25.50)` | half `(10.00, 0.12, 4.00)` | player + 40 kg |

`LANDING-32` source y is the box **center**. Top = 32.119 + 0.20 = 32.319 m.  
`MOD-HALL-DECK` top = 40.00 + 0.20 = **40.20 m** ≥ 40.00.

#### STAIR-A flight 24–32 formula (DESIGN TARGET, inherits `kB00StairDY` / `kB00StairDZ`)

23 treads, i = 1…23:

    y_top = 24.00 + 0.353 * i
    x     = 5.42 - 0.320 * (i - 1)
    z     = 38.40
    half  = (0.18, 0.10, 1.30)   # run is west, width is north-south
    body center y = y_top - 0.10

i=1: source `(5.42, 24.353, 38.40)` — overlaps the frozen handoff west face x=5.60 by 0.18 m. Gap = 0. No fall-through.  
i=23: source `(-1.62, 32.119, 38.40)`.

Rise 8.119 m. Run 7.04 m. Angle `atan(0.353/0.320) = 47.8°` (same as 0–24).  
Width 2.60 m. Capsule 0.70 m. Side clearance 1.90 m.

#### STAIR-A flight 32–40 formula (DESIGN TARGET)

23 treads, i = 1…23:

    y_top = 32.119 + 0.353 * i
    x     = -1.76
    z     = 35.82 - 0.320 * (i - 1)
    half  = (1.30, 0.10, 0.18)   # run is south, width is east-west

i=1: source `(-1.76, 32.472, 35.82)` — overlaps `LANDING-32` south face z=36.00 by 0.18 m.  
i=23: source `(-1.76, 40.238, 28.78)`.

`MOD-HALL-DECK` plan: x `[-9.76, 6.24]`, z `[22.78, 34.78]`. Last tread `(x=-1.76, z=28.78)` is on the landing center. Overlap vs capsule: landing half-z 6.00 m vs tread, no gap.

Inside primary frame `±45` m in x and z.

#### SKIN-S rungs 24–40 formula (DESIGN TARGET, inherits 0.40 m rise from existing i≥40 skin)

40 rungs, i = 1…40:

    y_top = 24.00 + 0.400 * i
    x     = -17.00
    z     = 22.40
    half  = (0.90, 0.10, 0.28)   # kB00SkinHalf*

i=1: source `(-17.00, 24.40, 22.40)` — meets SkinDeckA/B elevation band (SkinDeckA y=21.60, hang 21.40/22.80, SkinDeckB y=22.80). Implementer must keep a ≤0.45 m vertical step from the highest existing west skin deck onto rung 1 (existing skin already uses 0.40–0.45). If the highest west contact is SkinHangHigh top ≈ 22.90, add **two** connector rungs at y=23.30 and 23.70, same x/z, before i=1. Those two are climbable, same rating.

i=40: source `(-17.00, 40.00, 22.40)`.

SKIN crown: x `[-18.50, 1.50]`, z `[21.50, 29.50]`, top y=40.12. Overlaps rung 40 and hall west/south.

Rung rise 0.40 m is a climb/hang pitch, not a walk pitch. Traversal uses WO-003 mantle/hang. Do not convert it to a walkable stair.

#### Inherited members this slice does not rebuild

| ID | Source | Climbable? |
|---|---|---|
| `MOD-STAIR-A` treads 1–68 | `kFillStairEntityIdBegin` | yes |
| +24 handoff | `kIntakeHandoffEntityId` | yes |
| catwalks 5–6 | `kCatwalkEntityIdBegin+5/+6` | yes |
| SKIN rungs 1–48 + hang 0–5 | existing skin/hang IDs | yes |
| `MOD-DOG-A` | `kIntakeDogEntityId` | kinematic gate at grade |
| `MOD-YARD-JIB` | existing | finite machine, not a 40 m lift |
| `MOD-INTAKE-BELT` | existing | moving support at y≈1.20 |
| mill_obstacle[0] box | center `(-18, 22, 36)` half `(4, 22, 4)`, **top y=44** | collision-honest massing. If the player stands on it, that is a legal sequence break to y=44. Do **not** add invisible walls. Do **not** call it `MOD-HALL-DECK` |
| mill_obstacle[1] | center `(18, 22, 36)` half `(4, 22, 4)`, top y=44 | same |
| Godot `BrokenRise` / `TornPlate40` | presentation wreckage north of the old stubs | filler: keep only if collision matches a small tilted plate. Not the route. Not a teleport |
| Godot stacked `BayFloorRing*` at y=40 | presentation silhouette | **not** native support until twinned to `MOD-HALL-DECK`. Coding pass must not let presentation be stood on without a native body |

#### Entity IDs (DESIGN TARGET)

Assign after `kIntakeHandoffEntityId`. Do not reuse 0 (current dummy on mill_obstacle 6/7/8). Suggested layout:

- `kStairAWestEntityIdBegin` — 23
- `kLanding32EntityId` — 1
- `kStairASouthEntityIdBegin` — 23
- `kHallSoffitEntityId` — 1
- `kSkinFortyEntityIdBegin` — 40 (+2 connectors if needed)
- `kSkinCrownEntityId` — 1

Stable across save/load. Independent of Godot node IDs.

#### Clearance summary (meters, not “enough”)

| Location | Opening | Capsule | Residual |
|---|---|---|---|
| STAIR-A width (both new flights) | 2.60 | 0.70 | 1.90 |
| Tread depth | 0.36 (2×0.18) | foot, not capsule diameter | already frozen 0–24 |
| First west tread vs handoff | 0.18 overlap | 0.35 radius | overlap, no slot |
| First south tread vs landing 32 | 0.18 overlap | 0.35 radius | overlap, no slot |
| Last south tread vs hall | on-center | landing 12.00 m in z | no slot |
| SKIN rung width | 1.80 (2×0.90) | 0.70 | 1.10 |
| SKIN rise | 0.40 | hang/mantle, not walk | inherit 0–24 skin |
| Headroom 24–40 | open sky until hall soffit | 1.80 standing | unlimited |
| Hall soffit thickness | 0.40 | n/a | rigid static |
| Dog vs 24–40 throat | dog at y≈1.45, flights at y≥24 | — | **no occupancy**. Grade dog cannot stall 24–40 |

### 8.4 Mechanism

No new actuator. STAIR-A, SKIN, landings, and hall soffit are **static** (or kinematic with ω=0). Support-point velocity is 0. Inherited momentum on jump is the player’s own velocity (Law 5 still applies; the support is not moving).

`MOD-YARD-JIB` is inherited, not re-specified. Frozen arithmetic that **closes the gap** on atlas exit 3:

    boom length            L = 12.00 m
    boom height            H = 11.50 m          (kB00BoomHeightMeters)
    winch                  w ∈ [2.80, 10.20] m
    hook height (horizontal boom)  h_hook = H − w ∈ [1.30, 8.70] m
    crate half-y           0.90 m
    max crate-top-as-deck  ≈ 8.70 + 0.90 = 9.60 m
    player standing on boom top ≈ 11.50 + 0.22 = 11.72 m
    atlas target soffit    40.00 m
    deficit                40.00 − 11.72 = 28.28 m

Slew `±0.90 rad` changes plan position, not height. Rated moment `τ = 12 × 5000 × 9.81 = 588600 N·m` and winch force `49050 N` already stall 9 t and hold 4 t. None of that creates 28 m of extra hoist.

**Gap (atlas §6 B00 exit 3):** “yard jib used to place the player on the +40 m timber soffit” cannot be WORLD under frozen ratings. Do not lengthen the boom. Do not add a second jib. Do not write a fake “jib ride to 40” path. Keep the two routes that close.

Commands this slice adds: none. Pendant still only Drive/Raise/Lower/Brake on the yard jib.

Energy: static members do no work. Player locomotion is the existing controller. Do not add a fatigue-calorie solver (GDD §7.5 is not frozen as a number).

Tread load: player weight DESIGN TARGET 80 kg × 9.81 = 785 N, << 5 kN tread rating. No stall. No structural solver in this slice (TDD §10.3 still open; hall is rigid static until atlas band B01 / `WO-017_NEEDLE_SEAT`).

### 8.5 Occupancy and interlocks

| Envelope | Body | Effect on this slice |
|---|---|---|
| Latch at source `(4.00, 1.70, 12.40)` half `(1.60, 0.12, 0.55)`, clear Y 3.08 | 4 t crate | stalls **grade** dog only. Does not stall 24–40 |
| Dog at hinge `(11.90, 1.45, 13.50)` | dog box | blocks STAIR-A treads 1–N at grade when angle ≈ 0. Does not intersect flights 24–32 or 32–40 |
| Jib SWL / travel | jib | cannot author +40 support |
| Hall soffit | static | no interlock. Always standable once built |
| SKIN crown | static | always standable. Must not set dog angle |

No `reached_forty` flag. Tests may derive `player_grounded && player_position.y ≥ 40.00`. HUD may show that predicate. WORLD is the body under the feet.

### 8.6 Required causal path

Primary:

    ACT[walk STAIR-A 24–32 west, LANDING-32, STAIR-A 32–40 south]
      → STATE[support_entity_id is those tread/landing IDs in order]
      → WORLD[player feet on MOD-HALL-DECK, source y ≥ 40.00]
      → PLAY[atlas band B01 reachable; commit after 0.35 s dwell]

SKIN:

    ACT[climb SKIN-S rungs 24–40, walk crown]
      → STATE[support_entity_id is skin-forty IDs then kSkinCrownEntityId]
      → WORLD[same y ≥ 40.00; jib_crate_position and dog_angle_radians unchanged within 0.45 m / 0.15 rad]
      → PLAY[same]

Hybrid:

    ACT[arrive +24 via SKIN with dog still pinned; walk handoff west onto flight 24–32]
      → STATE[grade dog stays ~0; 24–40 treads are already static and present]
      → WORLD[y ≥ 40 on hall]
      → PLAY[K0 freight unsolved; K0 PLAY still true]

Sequence break (not authored, must not be blocked):

    ACT[mantle mill_obstacle[0] or [1] to its top]
      → STATE[support is that massing]
      → WORLD[y = 44]
      → PLAY[atlas: physically valid unhighlighted route is accepted]

### 8.7 Support / traversal handoff

Ordered authored primary:

| Step | Member | Source Y (atlas Z) | Type | Inherit v? |
|---|---|---|---|---|
| 0 | +24 handoff `kIntakeHandoffEntityId` | 24.00 | static | no |
| 1 | STAIR-A west treads i=1…23 | 24.35 → 32.12 | static | no |
| 2 | `LANDING-32` | 32.12 | static | no |
| 3 | STAIR-A south treads i=1…23 | 32.47 → 40.24 | static | no |
| 4 | `MOD-HALL-DECK` lower landing | 40.00 (top 40.20) | static | no |

Ordered authored SKIN:

| Step | Member | Source Y | Type | Inherit v? |
|---|---|---|---|---|
| 0 | existing SKIN / hang / SkinDeckC / +24 decks | ≤24 | static | no |
| 1 | connector rungs if needed | 23.30–23.70 | static | no |
| 2 | SKIN-S forty rungs i=1…40 | 24.40 → 40.00 | static | no |
| 3 | SKIN crown | 40.00 | static | no |
| 4 | optional step to hall soffit | 40.00 | static | no |

Handoff is a change of `support_entity_id` by contact. No teleport. No Godot parenting.

### 8.8 Failure states

| Trigger | World | Player can | Must not |
|---|---|---|---|
| Walk grade STAIR-A while crate in latch | dog occupancy stalls dog at angle 0; throat blocked | SKIN; belt; apron | auto-open for “they’re doing 24–40” |
| Walk 24–40 while crate in latch | 24–40 static treads exist | continue to 40 | freeze 24–40 because freight unsolved |
| Fall from 24–40 | gravity, WO-008 chute if deployed | land apron / belt / jib boom if geometry allows (atlas §8.3) | chute powered return to 40 |
| Miss SKIN rung | fall | chute / land / re-climb | invisible catch net |
| Jib raise toward 40 | winch hits `w = 2.80`; hook Y ≈ 8.70 | operate within limits | free hoist to 40 |
| 9 t on jib | stall, crate Y stays < 3.00 | SKIN / 24–40 if already at 24 | unpin dog |
| Presentation `commit_checkpoint()` at 24 without new bodies | persist only | nothing new to stand on | 40 m support appearing |
| Godot bay ring at y=40 with no native body | presentation only | cannot stand (if native has no collider) | presentation-owned stand |

### 8.9 Recovery

Soft-lock of the grade dog is already recovered by SKIN (0–24). This slice adds:

- if STAIR-A 24–40 is missed or fallen from: SKIN-S 24–40 is live
- if SKIN-S 24–40 is missed: STAIR-A 24–40 is live (from the +24 deck)
- if both flights are left and the player is in air: chute to apron 180 m field; belt and jib boom are legal catch if present; then re-ascent by real routes
- unrecoverable (tight well, no chute clearance): last commit. Do not auto-repair geometry

Do not strand on a 32 m landing with no down-climb. Both flights are two-way static stairs.

### 8.10 Persist

Consumes WO-009 persist.

New machine fields: **none**. New members are static authored geometry.

Commit rule for this slice (atlas B00 checkpoint intent + existing 0.35 s dwell):

- when `player_grounded` and `player_position.y ≥ 40.00` and dwell ≥ 0.35 s, store a commit even if a lower commit already exists
- stored: player pose/velocity, crate pose/velocity, jib slew/winch/brake/occupied/hook, dog angle, belt phase, kernel needle/sump/cage/screw fields (unchanged)
- reload must restore those and leave the new static 24–40 members in place (they are not reset because they are not aftermath; they are building)

Do not reset crate/dog on “band load” because the player reached 40.

### 8.11 Falsifiers (deterministic proof)

Native tests the coding pass must add (names are suggested; sentences are the fail condition):

1. `b00_forty_stair_from_handoff` — `InitialSpawn::IntakeHandoff`, walk west then south, must report `player_grounded && player_position.y ≥ 40.00` with `support_entity_id == kHallSoffitEntityId` (or crown). Fail if timeout without that support.
2. `b00_forty_stair_is_real_support` — during that climb, at least one support ID in the new west/south stair ranges. Fail if y jumps 24→40 with no intermediate stair support (teleport).
3. `b00_forty_skin_without_freight` — `InitialSpawn::IntakeSkin` (or handoff after a skin-only fixture), dog angle and crate xz captured at t=0; climb SKIN forty; y≥40; dog delta < 0.15 rad; crate xz delta < 0.45 m.
4. `b00_forty_hybrid_dog_pinned` — crate still in latch, dog ≈ 0, spawn `IntakeHandoff`; 24–40 stair still reaches y≥40. Fail if 24–40 is blocked by the grade dog.
5. `b00_forty_flag_is_not_a_floor` — `commit_checkpoint()` / HUD predicate without the new bodies must not place the player at y≥40 on hall support. Implementation: a world **without** the new EntityIds (or a test that only flips a bool) must fail to stand at 40.
6. `b00_jib_cannot_reach_forty` — pendant raise to winch min; hook/crate/player y all < 12.00. Fail if any jib-driven body reaches y≥40.
7. `b00_grade_dog_still_pins` — `IntakeStair` with crate in latch still cannot cross z>16.5 at y>3.2 (existing 0–24 falsifier stays green).
8. `b00_forty_commit_reload` — stand at 40, dwell 0.35 s, export/import; crate/dog/jib match; player y≥40 on the soffit.
9. `b00_unfinished_stubs_are_gone` — no standable unique route whose only treads are the three z=41.90/42.28/42.66 stubs.
10. Kernel + B00 0–24 suite remains `PASS`.

Optional observation, not a fail: standing on mill_obstacle[0] top y=44 is legal. Do not write a test that forbids it. Do not write a test that requires it.

### 8.12 Exit state

The next file may assume:

- player **can** have stable support at source `(-1.76, 40.20, 28.78)` on `MOD-HALL-DECK` lower landing (atlas `(-1.76, 28.78, 40.20)`)
- equivalently on SKIN crown top y=40.12
- automatic commit at that stand exists
- crate/dog/jib/belt are whatever the player left them (possibly still unsolved)
- `MOD-HOOK5-RACK` is **not** opened, moved, or authored
- atlas band B01 needles are visible at most as unfinished volume above 40 m; pockets at atlas z=96 m are not built
- +48 m needle racks are not in this slice
- frozen 0–24 numbers are unchanged
- yard jib still cannot reach 40 m

Next file inherits this stand as **entry**, not as a job to re-solve.

---

**Stop. Do not begin the next file inside this one.**  
Next file: `03_WORK_ORDERS/WO-016_HOOK5_RACK.md`
