# SCRAPERX — AS-005 CAGE OR SKIN (K1 PLAY)

**Ascent Slice:** `AS-005`
**Lifecycle:** `PLANNED` — contract exists, no corresponding source
**Provenance:** ⚠ **imported from `ScraperX-Grok`, NOT re-derived.** Its geometry, entity ids and persist versions describe a world that does not exist here. Reference provenance only
**Implementation gate:** re-author against this branch first, then `AS-004` implemented
**Evidence:** none. A plan is never implementation evidence.
**Depends on:** `AS-001_INTAKE_RISE.md`, `AS-002_LEGAL_FORTY.md`, `AS-003_HOOK5_RACK.md`, `AS-004_NEEDLE_SEAT.md` in source; `03_EXECUTION/PLANNING/ASCENT_PRE_RESOLUTION.md` §5 row `AS-005`

> **Provenance.** Adopted from `ScraperX-Grok`, where this slice was authored as `WO-013_CAGE_OR_SKIN`.
> Ticket numbers remapped per `03_EXECUTION/PLANNING/ASCENT_PRE_RESOLUTION.md` §5.1. Geometry, ratings and
> falsifiers carry over as **DESIGN TARGET**: they were never proven in source on that branch
> (it has not compiled since 2026-09-20). Positions are re-sited against this branch's tower
> at source `(0, —, -150)`.
>
> **`PORTED`, not authored.** Everything below still quotes `ScraperX-Grok` source
> constants — geometry, entity ids and persist versions that do not exist on
> `ScraperX-Claude`. Treat it as design intent, not as a job ticket. Re-derive it
> against real exit state first, as `AS-002`, `AS-003` and `AS-004` were.

## Objective

Close atlas §6 band B01 **exit** and K1 **PLAY**: stand on TP-120, **or** reach the 120 m SKIN ring.

SHAFT: if both needles are seated, `MOD-CAGE-1` may pass 118 m and land.  
SKIN: `MOD-EAST-OUTRIGGER` plus SKIN-E rungs. Needles may be 0, 1, or 2. Do not wall SKIN to protect the cage.

This is not `MOD-CW-STACK`. This is not `CAP-DOGKEY`. This is not AS-006.

## Existing truth

HEAD `b71c949` on `ScraperX-Grok` (needle source `a859f6b`) already has — **none of which is true on `ScraperX-Claude`**:

- AS-001–012 in source: 0–40 m, `CAP-HOOK5`, campaign needles, `MOD-CAGE-1` stroke y ∈ [8.00, 120.00]
- well source `(-1.76, —, 25.30)`, opening 5.00 × 5.00 (south of hall center so STAIR-A still lands)
- hall north remainder `kHallSoffitEntityId`, top y = 40.20
- needles park y = 48.36, seat y = 96.18, A z = 24.70, B z = 25.90, x = -1.76, length 18.40 m
- east pocket source x = 7.24
- interlock: `seated_count < 2` stalls cage at y ≥ 118.00
- last campaign entity `kModGuideEastEntityId` (319). New IDs start after it
- SKIN-S rungs to 40 m at source `x = -17.00`, `z = 22.40`
- yard jib still max hook y ≈ 8.70 m
- persist blob version 3
- Fold-device execution is not proven

There is **no** TP-120 deck and **no** `MOD-EAST-OUTRIGGER` yet. Cage can travel to 120 only when both needles are seated; the player currently steps into void.

## Authority

- Laws 2–7, 11–18, 21–27, 29, 32
- GDD §§7.2–7.4, 11, 15–17, 23–24
- Atlas §§3, 4 (`CAP-NEEDLE`), 5 (TP-120), 6 band B01 exit, 7 K1 PLAY, 8.1, 8.3, 12, 13
- TDD §§6, 8–11, 14
- Execution Protocol §§3–7, 11–12
- `AS-004_NEEDLE_SEAT.md` §8.12
- `03_EXECUTION/PLANNING/ASCENT_PRE_RESOLUTION.md` AS-005

## Owner

Native 90 Hz C++/Jolt. Godot presents.

## Allowed seam

New campaign bodies in the existing IntakeRise world: TP-120 ring, landing dogs, east outrigger(s), SKIN-E 40–120, 120 m SKIN ring, catwalk from ring to TP-120.

Reuse: kinematic cage, seating predicate, persist, support identity.

Do not retitle `KX-*`. Do not add `MOD-CW-STACK` / `MOD-DRUM-LOW`. Do not require `CAP-DOGKEY`. Do not lengthen the yard jib.

## Required causal path

SHAFT (needs `CAP-NEEDLE`):

    ACT[ride MOD-CAGE-1 with both needles seated, command up through 118]
      → STATE[cage y → 120.00; landing dogs retract]
      → WORLD[TP-120 ring is standable; support identity is TP-120]
      → PLAY[K1 PLAY closed; AS-006 may board the stack from this deck]

SKIN, zero needles:

    ACT[walk hall east onto MOD-EAST-OUTRIGGER at 40 m; climb SKIN-E]
      → STATE[support = outrigger then SKIN-E rungs]
      → WORLD[120 m SKIN ring reachable; cage interlock still false]
      → PLAY[same band B01 exit; cage unused]

SKIN, one needle (atlas coupling 2):

    ACT[walk the seated needle to x = 7.24 at y = 96.18; step the 96 m stub to SKIN-E]
      → STATE[support = NeedleA or B, then 96 m stub]
      → WORLD[SKIN-E joined at 96 m]
      → PLAY[climb SKIN-E 96 → 120; interlock still false]

Reconnect at 120:

    ACT[walk the 120 m SKIN ring east catwalk]
      → STATE[support = ring then TP-120]
      → WORLD[SHAFT and SKIN share TP-120]
      → PLAY[AS-006 entry is the same deck whether you rode or climbed]

Illegal:

    MISSION_FLAG → WORLD[landed]
    ACT[cage to 120 with 0–1 needles] → PLAY[TP-120]
    invisible wall on SKIN-E because needles are unseated

## Forbidden shortcuts

- `landed_120` flag as WORLD
- dogs that vanish without pose
- SKIN blocked to force the cage
- teleport from 40 m to 120 m
- building `MOD-CW-STACK` / `MOD-DRUM-LOW`
- requiring `CAP-DOGKEY`
- claiming Fold-device execution
- reset-on-band-load of needles or cage y

## Implementation scope

Native IntakeRise extension + falsifiers; Godot twins; persist v4; CI still runs AS-001–012 and kernel tests.

## Out of scope

`AS-006_CW_PIN` (next file): stack, pin, drum, west SKIN 120–220. `CAP-DOGKEY`. `CAP-BLIND`. Fold 45 FPS. Art/VO. Reopening needle seating.

## Proof path

This planning pass does not execute. Later coding must exercise §8.11, inspect frozen jib/well/needle numbers, screenshot TP-120 and the outrigger as one hall, keep kernel + AS-001–012 tests green.

## Completion

- TP-120 exists at source y = 120.00 around the frozen well
- cage with `CAP-NEEDLE` can land; player support is a TP-120 slab
- cage without `CAP-NEEDLE` still stalls at 118; no TP-120 from that ride
- `MOD-EAST-OUTRIGGER` at 40 m is climbable with zero needles
- one seated needle plus 96 m stub reaches SKIN-E
- SKIN-E 40–120 is real support; 120 m ring is standable
- ring connects to TP-120 without a teleport
- persist keeps cage y, dogs, and needle poses
- Fold install remains unverified

## Result record

pending. This pass does not execute.

---

## Mechanical close

### 8.1 Identity

| Field | Value |
|---|---|
| Atlas band | B01 Transfer Hall |
| Slice | band B01 exit: TP-120 **or** 120 m SKIN ring |
| Chain | K1 PLAY only. ACT/STATE/WORLD already closed by `AS-004` |
| Live braids | SHAFT (cage land). SKIN (outrigger + SKIN-E). FLOW not live |
| Transfer Plate | **TP-120** authored here |
| Modules allowed | `MOD-CAGE-1` (landing), `MOD-EAST-OUTRIGGER`, `MOD-HALL-DECK` (east opening already open), inherited needles/guides |
| Modules forbidden | `MOD-CW-STACK`, `MOD-DRUM-LOW`, `MOD-CW-PIN`, any atlas band B02–B11 ID |
| Capability consumed | `CAP-NEEDLE` (may be false; then SKIN only) |
| Capability authored | none. `CAP-DOGKEY` stays later |

### 8.2 Entry state

From `AS-004_NEEDLE_SEAT.md` §8.12 and source `a859f6b`:

- player can stand on hall y = 40.20
- `seated_count` ∈ {0, 1, 2} as left
- `CAP-HOOK5` may be false
- `CAP-NEEDLE` true only if both beams are in the pocket pose
- cage y ∈ [8, 120], interlock true only if both seated
- well xz frozen `(-1.76, 25.30)`
- east needle tip / east pocket at x = 7.24
- SKIN-S alive to 40 m at x = -17.00

### 8.3 Geometry

Frame: `source.x = atlas.x`, `source.y = atlas.z`, `source.z = atlas.y`.

Well (frozen):

    xz = (-1.76, 25.30)
    opening 5.00 × 5.00
    x ∈ [-4.26, 0.74], z ∈ [22.80, 27.80]

#### TP-120 (DESIGN TARGET, same ring trick as hall)

| ID | Role | Atlas (x, y, z) | Source (x, y, z) | Extents | Climbable? | Rating |
|---|---|---|---|---|---|---|
| `TP-120` north remainder | transfer plate | `(-1.76, 31.29, 120)` | `(-1.76, 120.00, 31.29)` | half `(8.00, 0.20, 3.49)` | yes | 15 kN/m² |
| `TP-120` west | ring | `(-7.01, 24.64, 120)` | `(-7.01, 120.00, 24.64)` | half `(2.75, 0.20, 1.86)` | yes | same |
| `TP-120` east | ring toward SKIN | `(3.49, 24.64, 120)` | `(3.49, 120.00, 24.64)` | half `(2.75, 0.20, 1.86)` | yes | same |
| well void at 120 | shaft | same well | y = 120 | 5.00 × 5.00 | no | fall |
| landing dogs | dogs-in only if `CAP-NEEDLE` and cage y ∈ [119.50, 120.50] | well north lip | source y = 120.35 | half `(0.20, 0.15, 1.40)` | no | n/a |

North remainder top = 120.20. Commit when player y ≥ 120.00 and support is a TP-120 id (atlas §8.1).

Cage at y = 120.00: deck top = 120.18. Residual onto TP-120 ≤ 0.10 m step.

Dogs **out** (default): occupy the east/west lips so a cage that cheated 118 cannot dump the player onto the plate. Dogs **in**: kinematic retract +1.20 rad, throat ≥ 0.90 m.

#### MOD-EAST-OUTRIGGER

| ID | Role | Source (x, y, z) | Extents | Climbable? | Rating |
|---|---|---|---|---|---|
| outrigger 40 | hall east → east facade | center `(11.62, 40.20, 25.30)` | half `(5.38, 0.12, 0.40)` | yes | player + 40 kg |
| stub 96 | needle east tip → SKIN-E | center `(12.12, 96.18, 25.30)` | half `(4.88, 0.12, 0.40)` | **only if** `seated_count ≥ 1` | player + 40 kg |

40 m beam: x from 6.24 (soffit east) to 17.00. Always present. Zero-needle skip.

96 m stub: x from 7.24 to 17.00. Collision **off** until `seated_count ≥ 1`. Then climbable. The seated needle is the first 9 m; the stub is the rest. Do not leave a 9 m air gap.

#### SKIN-E 40–120 (DESIGN TARGET, inherit 0.40 m pitch)

    x = 17.00
    z = 25.30
    y_top(i) = 40.20 + 0.400 * i
    i = 1…200
    last y_top = 120.20
    half = (0.28, 0.10, 0.90)

i=1 overlaps outrigger 40. i=200 is the 120 ring. 96 m stub meets i ≈ 140 (`40.20 + 0.40*140 = 96.20`).

#### 120 m SKIN ring (DESIGN TARGET)

| ID | Role | Source center | half | Climbable? |
|---|---|---|---|---|
| ring east | SKIN-E head | `(17.00, 120.00, 25.30)` | `(0.40, 0.12, 6.00)` | yes |
| ring south | join SKIN-S lineage | `(0.00, 120.00, 22.40)` | `(17.40, 0.12, 0.40)` | yes |
| ring west stub | AS-006 SKIN-W start | `(-17.00, 120.00, 25.30)` | `(0.40, 0.12, 6.00)` | yes |
| catwalk E | ring → TP-120 east | `(10.24, 120.00, 25.30)` | `(6.50, 0.12, 0.40)` | yes |

West stub is standable now so AS-006 does not invent a 120 m floor. AS-006 authors the climb **up** from here. Do not author SKIN-W rungs above 120 in this file.

#### Entity IDs (after 319)

`kTp120NorthEntityId` (reuse as commit support), west, east, dogs, outrigger40, stub96, `kSkinEEntityIdBegin` count 200, ring east/south/west, catwalk E.

### 8.4 Mechanism

Cage travel and interlock are **frozen**. This slice only adds landing geometry and dog pose.

    if CAP-NEEDLE and cage_y ∈ [119.50, 120.50] and |cage_command| < 0.05:
        dogs_in = true   # θ → 1.20 rad
    else:
        dogs_in = false  # θ = 0, lips blocked

Player walk-off is contact. No teleport.

96 m stub: `climbable = (needle_a_seated || needle_b_seated)`. Derived from beam poses, not a flag named `stub_ok`.

SKIN-E is always climbable from the 40 m outrigger.

After a legal land, the cage parks north on TP-120 so the well is free for AS-006's stack:

    landed cage park xz = (-1.76, 29.60)
    kinematic slide ≤ 4.3 m on the plate, speed 0.40 m/s, brake on when flush
    not a teleport

If the player arrived by SKIN, cage is already elsewhere.

### 8.5 Occupancy and interlocks

| Envelope | Body | Effect |
|---|---|---|
| cage y ≥ 118, seated_count < 2 | cage | frozen stall; dogs stay out |
| cage y ∈ [119.50, 120.50], CAP-NEEDLE | dogs | retract; TP-120 boardable |
| 96 m stub, seated_count = 0 | player | no support (air) |
| 96 m stub, seated_count ≥ 1 | player | walk to SKIN-E |
| SKIN-E any seated_count | player | climb legal |
| yard jib | — | still cannot reach 48 / 96 / 120 |

### 8.6 Required causal path

See Objective. Stitch to **next** file:

    PLAY[stand on TP-120 or 120 ring]
      → AS-006 entry: same well, stack top will sit at y = 120.20
      → west ring stub is where SKIN-W 120–220 begins

### 8.7 Support / traversal handoff

| Step | Member | Source Y | Type | Inherit v? |
|---|---|---|---|---|
| 0 | hall / outrigger 40 | 40.20 | static | no |
| 1a | cage deck | 8–120 | kinematic | yes |
| 1b | SKIN-E rungs | 40.60–120.20 | static | no |
| 1c | seated needle + stub 96 | 96.18 | kinematic / static | no |
| 2 | TP-120 or 120 ring | 120.20 | static | no |
| 3 | (next) stack top / SKIN-W | 120.20 | — | — |

### 8.8 Failure states

| Trigger | World | Player can | Must not |
|---|---|---|---|
| ride cage, 0–1 needles | stall 118 | lower; SKIN | TP-120 appear under them |
| miss well at 120 | fall toward apron / belt / jib / hall | chute | catch net |
| presentation `landed=true` | nothing | no new support | walking empty air |
| 96 stub with 0 needles | air | 40 m outrigger | invisible floor |

### 8.9 Recovery

- no `CAP-NEEDLE`: 40 m outrigger. Always.
- one needle: 96 m beam + stub, or outrigger.
- fall from 120: apron 180 m field; belt and jib boom legal catch.
- cage stuck at 118: lower, or leave the cage at 96/48/8 and take SKIN.
- do not strand on SKIN-E: rungs two-way.

### 8.10 Persist

Export version 4. Import 1–3: dogs out, no extra fields.

Additive:

- `tp120_dogs_angle`
- `cage1_park_x`, `cage1_park_z` if landed
- SKIN/outrigger are static; no pose fields
- cage1 / needles unchanged from v3

Commit: dwell 0.35 s on TP-120 **or** 120 ring, player y ≥ 120.00. Do not require `CAP-NEEDLE` for the SKIN commit.

### 8.11 Falsifiers

1. `wo013_cage_no_needle_still_stalls` — seated_count 0; cage cannot exceed 118; player support is never a TP-120 id.
2. `wo013_cage_lands_when_both_seated` — both seated; cage to 120; dogs θ ≥ 1.00; support is TP-120; player y ≥ 120.
3. `wo013_outrigger_zero_needles` — seated_count 0; player walks 40 m outrigger; support is outrigger entity.
4. `wo013_skin_e_to_120` — climb SKIN-E; y ≥ 120; support is ring or SKIN-E; `CAP-NEEDLE` may be false.
5. `wo013_one_needle_stub` — only A seated; player support becomes stub96 or SKIN-E at y ≥ 96; interlock still false.
6. `wo013_stub_absent_without_needle` — seated_count 0; stub is not standable.
7. `wo013_ring_to_tp` — from 120 ring walk catwalk; support becomes TP-120 without a teleport (xz continuous).
8. `wo013_flag_is_not_a_floor` — commit at 40 m does not create TP-120 under the player.
9. `wo013_prior_still_pass` — AS-001–012 + kernel PASS.

### 8.12 Exit state

The next file may assume:

- player **can** be standing on TP-120 **or** the 120 m SKIN ring (or still below, if they have not finished this job)
- well xz still `(-1.76, 25.30)`
- cage exists; may be at 120 with dogs in and parked north, or lower
- west ring stub at `(-17.00, 120.00, 25.30)` exists and is standable
- `MOD-CW-STACK`, `MOD-CW-PIN`, `MOD-DRUM-LOW` do **not** exist yet
- frozen 0–96 and hook5 numbers unchanged

Next file boards a stack from TP-120, **or** climbs west SKIN from the 120 ring stub.

---

**Stop. Do not begin the next file inside this one.**  
Next file: `03_EXECUTION/ASCENT/AS-006_CW_PIN.md`
