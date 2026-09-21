# SCRAPERX — WO-017 NEEDLE SEAT (K1)

**Work Order:** `WO-017`  
**Status:** READY after `WO-016_HOOK5_RACK` source on `ScraperX-Claude`  
**Depends on:** `WO-014_INTAKE_RISE.md`, `WO-015_LEGAL_FORTY.md`, `WO-016_HOOK5_RACK.md` in source; WO-012 kernel needle remains regression substrate; protocol `03_WORK_ORDERS/SECTION_PRE_RESOLVE_PROTOCOL.md` WO-017

> **Provenance.** Adopted from `ScraperX-Grok`, where this slice was authored as `WO-012_NEEDLE_SEAT`.
> Ticket numbers remapped per `SECTION_PRE_RESOLVE_PROTOCOL.md` §5.1. Geometry, ratings and
> falsifiers carry over as **DESIGN TARGET**: they were never proven in source on that branch
> (it has not compiled since 2026-09-20). Positions are re-sited against this branch's tower
> at source `(0, —, -150)`.

## Objective

Close atlas §7 **K1 ACT/STATE/WORLD**: seat `MOD-NEEDLE-A` and `MOD-NEEDLE-B` into `MOD-NEEDLE-POCKETS` at atlas `z = 96 m`.

Both seated → guide tolerance in range → cage interlock true.

This is not the ride to TP-120. This is not `MOD-EAST-OUTRIGGER`. This is not a second yard jib.

## Existing truth

HEAD `cf84dda` on `ScraperX-Claude` already has:

- B00 0–24, 24–40 stand, `CAP-HOOK5` acquire
- hall soffit `kB00Hall*` source `(-1.76, 40.00, 28.78)` half `(8.00, 0.20, 6.00)`, top y = 40.20
- yard jib frozen: boom 12.00 m, height 11.50 m, winch `[2.80, 10.20]` m, max hook y ≈ 8.70 m
- `CAP-HOOK5` is a 36 kg body. Crate pre-sling is not `CAP-HOOK5`
- last campaign entity `kHook5BlockEntityId` (287). New IDs start after it
- kernel needle (WO-012): 7.20 m, 620 kg, pockets at mill y ≈ 4.86, `NeedleBay` fixture only. IntakeRise currently dummies those IDs. Do not retitle them as `MOD-NEEDLE-A/B`
- kernel cage stroke y `[8.52, 21.82]`, mill xz `(9.50, 34.20)`. That fixture is not campaign `MOD-CAGE-1`
- player capsule: radius 0.35 m, standing half-height 0.90 m
- Fold-device execution is not proven

Atlas freezes pocket elevation (`z = 96 m`), pocket spacing (18 m), storage elevation (48 m), and the two legal hoists. It does not freeze XYZ in plan. Placement below is **DESIGN TARGET**, derived from the frozen hall soffit so the 18 m span crosses the well and the east opening.

## Authority

- Laws 2–7, 11–18, 21–27, 29, 32
- GDD §§7.2–7.4, 11, 15–17, 23–24
- Atlas §§3, 4 (`CAP-HOOK5`, `CAP-NEEDLE`), 6 B01, 7 K1, 8.1, 8.3, 12, 13
- TDD §§6, 8–11, 14
- Execution Protocol §§3–7, 11–12
- `WO-016_HOOK5_RACK.md` §8.12 exit
- `SECTION_PRE_RESOLVE_PROTOCOL.md` WO-017
- WO-012: kernel seating proof. Do not reopen it. Do not gate it on campaign `CAP-HOOK5`

## Owner

Native 90 Hz C++/Jolt. Godot presents. Mission/UI observe predicates only.

## Allowed seam

New campaign bodies in the existing IntakeRise world: well opening in `MOD-HALL-DECK`, needle racks at 48 m, pockets at 96 m, two needle beams, `MOD-CAGE-1` as a vertical winch, `MOD-GUIDE-RACK` rails.

Reuse: finite machine, distance-constraint hook, seatable member (WO-012 class), occupancy/interlock, checkpoint (WO-009).

Do not retitle `KX-*`. Do not lengthen `MOD-YARD-JIB`. Do not add `MOD-EAST-OUTRIGGER` or TP-120 deck.

## Required causal path

Primary (both needles, SHAFT):

    ACT[carry CAP-HOOK5 to the 48 m rack; attach cage hook to MOD-NEEDLE-A padeye]
      → STATE[campaign hook load = NeedleA; cage winch live]
      → WORLD[NeedleA lifts; mass 1600 kg < cage SWL 2000 kg]
      → PLAY[beam can be lowered into MOD-NEEDLE-POCKETS]

    ACT[lower NeedleA into both pockets, then repeat for MOD-NEEDLE-B]
      → STATE[NeedleA seated AND NeedleB seated; guide offset ≤ 0.02 m]
      → WORLD[MOD-GUIDE-RACK in tolerance; cage interlock true]
      → PLAY[K1 WORLD closed; ride to TP-120 is the next file]

One needle only:

    ACT[seat only NeedleA (or only NeedleB)]
      → STATE[one beam seated; guide offset stays 0.12 m]
      → WORLD[that beam is walkable springy span; cage interlock still false]
      → PLAY[cross the well on the beam; SKIN skip remains legal]

Leave needles (SKIN skip; outrigger is next file):

    ACT[do not attach CAP-HOOK5; do not hoist]
      → STATE[both beams stay on the 48 m racks; guide offset 0.12 m]
      → WORLD[cage interlock false; hall and SKIN-S to 40 m unchanged]
      → PLAY[WO-018_CAGE_OR_SKIN may still author MOD-EAST-OUTRIGGER]

Illegal third atlas sentence (does not close — see §8.4):

    ACT[hoist needles with MOD-YARD-JIB]
      → cannot produce WORLD[beam at y = 48 or y = 96] under frozen 12 m / 11.50 m jib

## Forbidden shortcuts

- `needles_seated` / `guides_ok` flags as WORLD
- retitling kernel `KX-NEEDLE` as `MOD-NEEDLE-A`
- lengthening the yard jib to reach 48 m
- seating by animation or mesh-swap
- unaided lift of a 1600 kg beam
- attaching campaign needles without `CAP-HOOK5`
- gating kernel `NeedleBay` on campaign `CAP-HOOK5`
- building `MOD-EAST-OUTRIGGER` or TP-120 deck
- invisible walls on SKIN-S / hall to “protect” the puzzle
- reset-on-band-load of seated beams
- claiming Fold-device execution

## Implementation scope

Native IntakeRise extension + falsifiers; Godot twins; persist fields in §8.10; CI still runs B00 and kernel tests.

Climbable vs filler is in §8.3.

## Out of scope

`WO-018_CAGE_OR_SKIN` (next file): TP-120 landing deck, `MOD-EAST-OUTRIGGER`, SKIN ring at 120 m. `CAP-DOGKEY`. Atlas band B02 drum room at 128 m. Fold 45 FPS. Art/VO. Hard-fail missions. Reopening WO-012 or WO-009. Changing frozen 0–40 / hook5 constants.

## Proof path

This planning pass does not execute. Later coding must exercise:

1. Native tests in §8.11
2. Source inspection: yard jib constants unchanged; kernel `NeedleBay` still seats without `CAP-HOOK5`; new campaign IDs exist
3. Godot mirrors cage y, both beams, pocket contact, guide offset
4. Screenshot: 48 m racks, well, cage, 96 m pockets readable as one hall
5. Screenshot: one seated beam walked; second unseated; interlock still false
6. Android arm64 APK still contains `libscraperx_native.so`
7. WO-011–008, B00 0–24, legal-forty, hook5 tests stay green

Claim classes stay distinct (Law 29).

## Completion

- `MOD-NEEDLE-A/B` exist as 18.4 m, 1600 kg bodies on 48 m racks
- `MOD-NEEDLE-POCKETS` exist 18 m apart at source y = 96.00
- `MOD-CAGE-1` hoists a hooked needle vertically; SWL 2000 kg; stall above that
- attach to campaign padeyes requires `CAP-HOOK5` (body not in the grade rack)
- `MOD-YARD-JIB` still cannot lift any body to y = 48
- both seated → guide offset ≤ 0.02 m and cage interlock true
- one seated → that beam is standable; interlock false
- zero seated → SKIN/hall still work; interlock false
- kernel needle tests still pass without campaign `CAP-HOOK5`
- Fold install / on-device play remain unverified

## Result record

pending. This pass does not execute.

---

## Mechanical close

### 8.1 Identity

| Field | Value |
|---|---|
| Band | atlas band B01 Transfer Hall |
| Slice authored | seat needles. Atlas z = 40–96 m geometry for racks, well, pockets, cage-as-winch. Not TP-120 |
| Chain | K1 Needles, ACT/STATE/WORLD. PLAY ride to 120 m is the next file |
| Live braids | SHAFT (cage winch + hall). SKIN-S to 40 m remains. FLOW not live |
| Transfer Plate | none new. Apron and +40 soffit already exist. TP-120 is next file |
| Modules allowed | `MOD-HALL-DECK` (well opening + 48 m rack climb), `MOD-NEEDLE-POCKETS`, `MOD-NEEDLE-A/B`, `MOD-CAGE-1`, `MOD-GUIDE-RACK`, inherited B00 machines |
| Modules forbidden | `MOD-EAST-OUTRIGGER`, `MOD-CW-STACK`, `MOD-DRUM-LOW`, any atlas band B02–B11 ID |
| Capability consumed | `CAP-HOOK5` (may be false; then this chain does not hoist) |
| Capability authored | `CAP-NEEDLE` as seated pair (world fact, not a picked item) |

### 8.2 Entry state

Inherited from `WO-016_HOOK5_RACK.md` §8.12 and source:

- player **can** stand on `MOD-HALL-DECK` at source `(-1.76, 40.20, 28.78)`
- `CAP-HOOK5` may be true (block held or out of rack) or false (still in grade cage)
- crate/dog/jib/belt are whatever the player left
- yard jib max hook y ≈ 8.70 m
- campaign needles do not exist yet
- kernel needle/cage fixtures are not this hall
- persist blob is version 2 (hook5 fields)

If `CAP-HOOK5` is false, this slice must still load. Hoist attach fails. SKIN/hall remain.

### 8.3 Geometry

Frame: `source.x = atlas.x`, `source.y = atlas.z`, `source.z = atlas.y`.

Frozen hall (do not move the 40 m stand):

    center source (-1.76, 40.00, 28.78)
    half (8.00, 0.20, 6.00)
    x [-9.76, 6.24], z [22.78, 34.78], top y = 40.20

Well and span are DESIGN TARGET on that plan. Pockets 18.00 m apart along X, centered on the hall x.

    well / cage xz = (-1.76, 28.78)
    west pocket x = -1.76 - 9.00 = -10.76
    east pocket x = -1.76 + 9.00 =  7.24
    spacing = 18.00 m

East pocket sits past the current soffit (x = 6.24) into the atlas east opening. West pocket sits just west of the soffit. The beam is the span.

#### Well opening in MOD-HALL-DECK

Replace the single solid soffit with a ring so the 40 m stand stays and a shaft exists.

| ID | Role | Atlas (x, y, z) | Source (x, y, z) | Extents | Climbable? | Rating |
|---|---|---|---|---|---|---|
| `HALL-RING` (4 slabs) | soffit minus well | same hall plan, y = 40 | y = 40.00 | see opening | yes | inherit 15 kN/m² |
| well opening | void | `(-1.76, 28.78, 40)` | `(-1.76, 40.00, 28.78)` | 5.00 × 5.00 m plan | no | n/a — fall to apron |

Cage deck 3.10 × 3.10. Opening 5.00. Residual 0.95 m each side. Capsule 0.70. Do not leave a walkable lip thinner than 0.90 m on the north/south/west remainders. East remainder is the opening toward facade (may be thinner; not the 40 m stand).

Coding: keep `kHallSoffitEntityId` as the largest remaining slab (north remainder) so existing +40 tests that key on that ID still see a y ≥ 40 body. Add ring IDs after hook5.

#### Members this slice adds (DESIGN TARGET)

| ID | Role | Atlas (x, y, z) | Source (x, y, z) | Extents / rule | Climbable? | Rating |
|---|---|---|---|---|---|---|
| `NEEDLE-RACK` west saddle | storage at 48 m | `(-10.76, 28.78, 48.00)` | `(-10.76, 48.00, 28.78)` | half `(0.40, 0.20, 1.20)` | yes | player + 40 kg |
| `NEEDLE-RACK` east saddle | storage at 48 m | `(7.24, 28.78, 48.00)` | `(7.24, 48.00, 28.78)` | half `(0.40, 0.20, 1.20)` | yes | same |
| `NEEDLE-RACK` rungs | climb 40.20 → 48.00 | west of well, x = -4.50 | x = -4.50, z = 28.78 | 20 rungs; see formula | yes | player only |
| `MOD-NEEDLE-A` | south twin beam | parked `( -1.76, 28.18, 48.36 )` | parked `(-1.76, 48.36, 28.18)` | half `(9.20, 0.18, 0.28)` | when seated or on rack | see §8.4 |
| `MOD-NEEDLE-B` | north twin beam | parked `( -1.76, 29.38, 48.36 )` | parked `(-1.76, 48.36, 29.38)` | same | same | same |
| `POCKET-W` | west seat | `(-10.76, 28.78, 96.00)` | `(-10.76, 96.00, 28.78)` | half `(0.40, 0.20, 0.40)` | no (seat) | machine load |
| `POCKET-E` | east seat | `(7.24, 28.78, 96.00)` | `(7.24, 96.00, 28.78)` | same | no (seat) | machine load |
| `MOD-CAGE-1` | material/personnel cage | xz hall center | xz `(-1.76, 28.78)` | half `(1.55, 0.18, 1.55)` | yes | player + 2000 kg SWL hook |
| `GUIDE-W` / `GUIDE-E` | cage guides | well faces | x = -4.26 / 0.74, z = 28.78 | rail y 8–120, half `(0.12, 56, 0.12)` | no | n/a |

#### Rack rungs 40–48 (DESIGN TARGET, inherit 0.40 m skin pitch)

20 rungs, i = 1…20:

    y_top = 40.20 + 0.400 * i
    x     = -4.50
    z     = 28.78
    half  = (0.90, 0.10, 0.28)

i=1: source `(-4.50, 40.60, 28.78)` — overlaps hall top 40.20 by a 0.40 m climb step (hang/mantle, not walk).  
i=20: source `(-4.50, 48.20, 28.78)` — onto west saddle.

#### Needle body

    length  = 18.40 m   (18.00 m pocket spacing + 0.20 m seat each end)
    section = 0.56 m high × 0.56 m wide  (half 0.18 × 0.28)
    mass    = DESIGN TARGET 1600 kg
              kernel 620 kg / 7.20 m = 86.11 kg/m
              86.11 × 18.40 = 1584 kg → 1600 kg
    weight  = 1600 × 9.81 = 15696 N

Parked A center z = 28.18 (0.60 m south of well center).  
Parked B center z = 29.38 (0.60 m north). Twin pair, same pockets.

Seated pose (both): same x = -1.76, y = 96.18 (pocket top 96.20 − 0.02), z = 28.18 / 29.38.

Padeye on each beam: top center, local `(0, +0.18, 0)`.

#### MOD-CAGE-1

    xz      = (-1.76, 28.78)
    y       ∈ [8.00, 120.00]     DESIGN TARGET stroke
    speed   = 0.60 m/s loaded, 1.25 m/s empty (inherit kernel empty speed)
    SWL     = 2000 kg
    F_rated = 2000 × 9.81 = 19620 N
    hook    = 0.14 m sphere, 0.35 m below deck
    lever   = local (0.70, 0.95, -0.35), radius 3.00 m (inherit kernel lever law)

This slice uses the cage as a **winch**. Travel through 48 m and 96 m must be real. Landing at 120 m is next file: interlock stalls y ≥ 118.00 until both needles seated. Do not add a 120 m deck here.

Drum is **below**, pit source y = 4.00 at the same xz. Not `MOD-DRUM-LOW` (atlas band B02, 128 m). Player does not need 128 m to run this winch.

#### MOD-GUIDE-RACK

Two rails on the well faces. Reduced-order pose:

    seated_count = 0 or 1  → guide_offset = 0.12 m  (out of tolerance)
    seated_count = 2       → guide_offset = 0.02 m  (in range)

Offset is a real kinematic shift of the rails, not a HUD bool. Cage deck AABB vs rail AABB: if offset = 0.12 m and cage y > 90, scrape/stall when the coder wires contact. Minimum this slice: interlock uses `guide_offset` derived from seated poses.

#### Clearance (meters)

| Location | Opening | Capsule | Residual |
|---|---|---|---|
| well opening | 5.00 | cage 3.10 / player 0.70 | 0.95 / 2.15 |
| rack rung width | 1.80 | 0.70 | 1.10 |
| rack rise | 0.40 | hang/mantle | inherit skin |
| pocket throat | 0.80 × 0.80 | beam 0.56 | 0.12 each side |
| twin gap (A to B) | 0.60 m center, 0.04 m face gap | foot | walk one beam at a time |
| cage interior | 3.10 | 0.70 | 2.40 |
| headroom hall → 48 | open | 1.80 | n/a |

#### Entity IDs (DESIGN TARGET)

After `kHook5BlockEntityId` (287). Do not reuse kernel `kNeedleEntityId` / `kCageEntityId`.

- `kHallWellNorthEntityId` … ring slabs (keep `kHallSoffitEntityId` as north remainder)
- `kNeedleRackWestEntityId`, `kNeedleRackEastEntityId`
- `kNeedleRackRungEntityIdBegin` — 20
- `kModNeedleAEntityId`, `kModNeedleBEntityId`
- `kModPocketWestEntityId`, `kModPocketEastEntityId`
- `kModCage1EntityId`
- `kModGuideWestEntityId`, `kModGuideEastEntityId`

Stable across save/load. Independent of Godot node IDs.

### 8.4 Mechanism

#### A. Yard jib (inherited — does not close atlas coupling 1)

    boom 12.00 m, height 11.50 m, winch [2.80, 10.20]
    max hook y ≈ 8.70 m
    rack y = 48.00 m
    deficit = 48.00 − 8.70 = 39.30 m

SWL 5000 kg would lift 1600 kg. Height does not exist. Do not attach campaign needle padeyes to `MOD-YARD-JIB`. Do not lengthen the boom.

**Gap (atlas §6 B01 coupling 1, jib clause):** yard jib cannot hoist needles at 48 m or 96 m. Keep the cage-as-winch clause. That closes.

#### B. MOD-CAGE-1 winch (new campaign actuator)

Bodies: kinematic cage deck + sensor hook. Command: lever in range → Raise / Lower / Brake. Same class as kernel cage, new IDs, new stroke.

    τ not required (vertical drum, not a slew jib)
    F_rated = 19620 N
    one needle: 15696 N  → legal
    two needles on one hook: 31392 N → stall (SWL 2000 kg)
    empty raise: 1.25 m/s
    loaded raise: 0.60 m/s
    brake hold: F_rated at any y in [8.00, 120.00]
    out-of-travel: y = 8.00 and y = 120.00
    interlock: if seated_count < 2 and cage y ≥ 118.00 and command > 0 → stall, y held

Attach (campaign only):

    if CAP-HOOK5
       and hook-to-padeye distance ≤ 1.20 m
       and cage brake on or speed < 0.05 m/s
    then distance constraint, mMaxDistance = 1.20 m
    else no attach

Without `CAP-HOOK5`, the cage still travels with the player. It does not pick needles.

Detach: Lower until beam seats, or Raise away after unseat, or explicit drop if constraint breaks on overload.

Not allowed to write: `needles_seated` flag, kernel `needle_id_` pose, yard-jib winch.

Energy: lift one beam 48 → 96 = `15696 N × 48 m = 753408 J`. Drum must supply that over 48 / 0.60 = 80 s. Mean power `753408 / 80 ≈ 9418 W`. DESIGN TARGET. Stall if F > 19620 N.

#### C. Seating (WO-012 class, campaign bodies)

A beam is seated when all are true:

    |beam.x − (−1.76)| ≤ 0.25 m
    beam.y ∈ [96.00, 96.50]
    |beam.z − parked_z| ≤ 0.20 m
    |yaw| ≤ 0.08 rad
    both pocket AABBs overlap the beam AABB
    hook constraint absent OR slack (distance ≥ 1.10 m)

Then: motion type kinematic, pose snapped to seated pose (same snap kernel already does). Unseat if cage hook is over the padeye, raise live, horiz ≤ 0.90 m.

One seated beam is climbable. Two seated beams are climbable and set `guide_offset = 0.02`.

`CAP-NEEDLE` WORLD predicate:

    cap_needle = NeedleA seated AND NeedleB seated

Derived. Not stored as the cause. Persist stores both beam poses.

#### D. Guides

Kinematic rails. `guide_offset` is a function of seated count (see §8.3). Interlock reads `guide_offset`, not a mission bool.

### 8.5 Occupancy and interlocks

| Envelope | Body | Effect |
|---|---|---|
| well 5.00 × 5.00 at hall | player / cage | fall or ride; not a kill plane |
| pocket AABBs | needle A/B | seating snap when aligned |
| cage hook ≤ 1.20 m of padeye | CAP-HOOK5 body fact | attach allowed |
| same, no CAP-HOOK5 | — | no attach |
| two needles on one hook | 3200 kg | stall |
| cage y ≥ 118, seated_count < 2 | cage | interlock stall |
| cage y ≥ 118, seated_count = 2 | cage | travel legal; no 120 deck yet |
| yard jib vs campaign padeye | — | never attaches |
| kernel NeedleBay | kernel needle | unchanged; no CAP-HOOK5 gate |
| grade dog / hook5 door | crate | unchanged |

No `guides_ok` flag. Tests derive `guide_offset` from beam poses.

### 8.6 Required causal path

Primary:

    ACT[CAP-HOOK5 held; climb rack rungs 40→48; stand on west saddle]
      → STATE[player support = rack; block still held]
      → WORLD[padeye in 1.20 m of a parked beam]
      → PLAY[attach available when cage hook is also there]

    ACT[call cage to y = 48; attach; raise to y = 96; lower into pockets]
      → STATE[NeedleA seated]
      → WORLD[south twin is walkable; guide_offset still 0.12]
      → PLAY[cross well; interlock false]

    ACT[repeat for NeedleB]
      → STATE[both seated; guide_offset = 0.02]
      → WORLD[interlock true]
      → PLAY[K1 WORLD closed]

Skip hoist:

    ACT[leave both beams on racks]
      → STATE[seated_count = 0]
      → WORLD[interlock false]
      → PLAY[next file may still author SKIN outrigger]

Illegal:

    MISSION_FLAG → WORLD[seated]
    ACT[yard jib raise] → WORLD[beam at 96]
    ACT[stand at 40] → PLAY[CAP-NEEDLE]

### 8.7 Support / traversal handoff

| Step | Member | Source Y | Type | Inherit v? |
|---|---|---|---|---|
| 0 | hall soffit ring | 40.20 | static | no |
| 1 | rack rungs i=1…20 | 40.60 → 48.20 | static | no |
| 2 | rack saddle / parked beam | 48.20 / 48.54 | static / dynamic | no |
| 3 | cage deck (optional) | 8–120 | kinematic | yes |
| 4 | seated NeedleA and/or B | 96.18 | kinematic | no |
| 5 | (next file) TP-120 or outrigger | 120 | — | — |

Handoff is contact. No teleport of beams. Cage motion imparts support-point velocity.

### 8.8 Failure states

| Trigger | World | Player can | Must not |
|---|---|---|---|
| attach without CAP-HOOK5 | no constraint | climb racks; SKIN; wait | beams rising |
| two beams on one hook | stall | drop one | free lift of 3200 kg |
| 9 t on yard jib | existing stall | cage path | jib reaching 48 m |
| cage raise to 118, 0–1 seated | interlock stall | lower; SKIN | TP-120 appear |
| miss well from hall | fall toward apron / belt / jib boom (atlas §8.3) | chute if high | catch net |
| seat misaligned | no snap | re-hoist | flag snap |
| presentation `needles_seated = true` | nothing | no new support | walking a missing beam |
| kernel NeedleBay without CAP-HOOK5 | kernel still seats | — | campaign gate on kernel |

### 8.9 Recovery

- lost CAP-HOOK5: descend real STAIR/SKIN to grade rack; or skip K1
- beam dropped down the well: last commit if unrecoverable; do not auto-return
- cage stalled at interlock: lower; SKIN/hall still live
- one beam seated, second dropped: walk the seated beam; interlock stays false
- never take the hook: leave needles; next file’s outrigger is the atlas skip
- fall from 96: apron 180 m field; belt and jib boom are legal catch if present

Do not strand on a 48 m saddle with no down-climb. Rungs are two-way.

### 8.10 Persist

Consumes WO-009 persist. Additive fields, export version 3; import accepts 1–2 with needles at park and cage at y = 8.00:

- `needle_a_x,y,z` + `needle_a_seated`
- `needle_b_x,y,z` + `needle_b_seated`
- `cage1_y`, `cage1_command`, `cage1_brake`
- `cage1_hook_load` (0 none, 1 A, 2 B)
- `guide_offset` may be derived on import; do not store it as the cause

`cap_hook5` still derived from hook5 body. `cap_needle` derived from both seated poses.

Do not reset seated beams on “band load.”

Commit: no new elevation commit required. Existing y ≥ 40 and apron dwell still apply. Next file will add TP-120 commit.

### 8.11 Falsifiers (deterministic proof)

1. `b01_jib_cannot_reach_racks` — pendant raise to winch min; hook/crate/player y all < 12. Fail if any jib-driven body reaches y ≥ 48.
2. `b01_no_cap_no_attach` — `CAP-HOOK5` false; cage hook at NeedleA padeye; raise; NeedleA y stays < 49. Fail if the beam lifts.
3. `b01_cage_lifts_one_with_cap` — `CAP-HOOK5` true; attach A; raise; NeedleA y ≥ 90. Fail if timeout or stall on 1600 kg.
4. `b01_two_on_hook_stalls` — attach A and B at once (or 3200 kg); raise; stall; neither reaches 96.
5. `b01_one_seat_is_walkable` — seat only A; player support_entity_id is NeedleA at y ≥ 96; `guide_offset` > 0.05; cage y cannot exceed 118.
6. `b01_both_seat_closes_k1` — seat A and B; `guide_offset` ≤ 0.02; cage command up at y = 117 does not stall for interlock (still no 120 deck).
7. `b01_flag_is_not_a_beam` — flip a bool / commit without the bodies; no standable span at y = 96.
8. `b01_kernel_needle_ungated` — `InitialSpawn::NeedleBay` still seats kernel needle without campaign `CAP-HOOK5`.
9. `b01_hook5_and_forty_still_pass` — existing B00 tests remain `PASS`.
10. Kernel WO-011–008 remain `PASS`.

### 8.12 Exit state

The next file may assume:

- `MOD-NEEDLE-A/B`, pockets at y = 96, racks at y = 48, `MOD-CAGE-1`, `MOD-GUIDE-RACK` exist
- seated_count is 0, 1, or 2 as the player left it
- `CAP-HOOK5` may still be false
- `CAP-NEEDLE` is true only if both beams are in the pocket pose
- cage interlock is true only if both are seated
- there is **no** TP-120 deck and **no** `MOD-EAST-OUTRIGGER`
- yard jib still cannot reach 48 m
- hall 40 m stand still exists around the well
- frozen 0–40 and hook5 numbers are unchanged

Next file inherits seating as a world fact to consume: land the cage at 120 if interlock true, **or** build the east outrigger SKIN path.

---

**Stop. Do not begin the next file inside this one.**  
Next file: `03_WORK_ORDERS/WO-018_CAGE_OR_SKIN.md`
