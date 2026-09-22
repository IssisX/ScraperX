# SCRAPERX — AS-006 CW PIN (K2)

**Ascent Slice:** `AS-006`
**Lifecycle:** `PLANNED` — contract exists, no corresponding source
**Provenance:** ⚠ **imported from `ScraperX-Grok`, NOT re-derived.** Reference provenance only
**Implementation gate:** re-author against this branch first, then `AS-005` implemented
**Evidence:** none. A plan is never implementation evidence.
**Depends on:** `AS-001`–`AS-005`; `03_EXECUTION/PLANNING/ASCENT_PRE_RESOLUTION.md` §5 row `AS-006`

> **Provenance.** Adopted from `ScraperX-Grok`, where this slice was authored as `WO-014_CW_PIN`.
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

Close atlas §7 **K2**: pull `MOD-CW-PIN` **or** ride `MOD-CW-STACK` **or** climb west SKIN to 220 m.

Pin pulled → stack mass/travel change → dumped block is a new body on a timber catch → ride or climb that ledge.

This is not wet isolation. This is not `CAP-BLIND`. This is not TP-340.

## Existing truth

After `AS-005` is in source:

- TP-120 ring around well `(-1.76, 120.00, 25.30)`
- 120 m SKIN ring with **west stub** `(-17.00, 120.00, 25.30)`
- `MOD-CAGE-1` lands at 120 only with `CAP-NEEDLE`; it is not the 120–220 elevator
- well opening 5.00 × 5.00
- last IDs after SKIN-E / TP-120 (coding assigns after that file’s last id)
- persist v4
- Fold-device execution is not proven

Atlas: band B02 has **no** Transfer Plate. 220 is a well-head landing inside `MOD-WELL-LEDGES`, not a new TP id.

Until AS-005 is coded, this file still inherits AS-004 geometry plus the DESIGN TARGET exit of AS-005. Do not implement this file before AS-005 is in source.

## Authority

- Laws 2–7, 11–18, 21–27, 29, 32
- GDD §§7.2–7.4, 11, 15–17, 23–24
- Atlas §§3, 5 (band B02 has no TP), 6 band B02, 7 K2, 8.1, 8.3, 12, 13
- TDD §§6, 8–11, 14
- `AS-005_CAGE_OR_SKIN.md` §8.12
- protocol AS-006

## Owner

Native 90 Hz C++/Jolt. Godot presents.

## Allowed seam

New bodies: `MOD-CW-STACK` (4 blocks), `MOD-CW-PIN`, timber catch, `MOD-DRUM-LOW` at 128 m, drum-access treads 120→128, `MOD-WELL-LEDGES`, SKIN-W rungs 120–220.

Reuse: kinematic moving support (belt/cage class), carryable 32 kg (hook5 class), persist.

Do not retitle kernel screw/cage as the stack. Do not add `MOD-HEADER-W`. Do not author TP-340.

## Required causal path

Ride (pin in):

    ACT[board stack top from TP-120; run MOD-DRUM-LOW]
      → STATE[stack_top_y increases; 4 blocks, 16000 kg]
      → WORLD[travel clamped to 188.00 m while pin is in]
      → PLAY[moving support to 188; jump inherits stack v]

Pin (K2):

    ACT[pull MOD-CW-PIN while stack braked]
      → STATE[pin body leaves the stack; bottom block detaches]
      → WORLD[dumped block seated on catch at y = 136; remaining 3 blocks 12000 kg; travel max = 220.00]
      → PLAY[climb the dumped block + well ledges, or ride the lighter stack to 220]

SKIN skip:

    ACT[from 120 west stub, climb SKIN-W]
      → STATE[support = SKIN-W rungs]
      → WORLD[220 west head reachable; stack pose unchanged]
      → PLAY[AS-007 entry without K2]

Stitch from previous:

    TP-120 east/north remainders → walk to well lip → stack top is the next floor
    120 west stub → SKIN-W is the next ladder
    cage at 120 is parked north; it does not become the stack

Stitch to next:

    220 well-head ledge and 220 SKIN-W head are AS-007’s floor

## Forbidden shortcuts

- `pin_pulled` flag as WORLD without the block body on the catch
- stack teleport 120→220
- SKIN-W walled off because the pin is in
- using `MOD-CAGE-1` stroke above 120
- animation dump
- claiming Fold-device execution

## Implementation scope

Native + falsifiers; Godot twins; persist v5.

## Out of scope

`AS-007_WET_ISOLATION`. `CAP-BLIND`. `CAP-DOGKEY` (no new SHAFT cage landing this band). Fold 45 FPS. Art/VO. Reopening TP-120.

## Proof path

§8.11 tests; screenshot stack in the same well as the cage; kernel + AS-001–013 tests green. Result pending.

## Completion

- stack top is standable at y = 120.20 at spawn
- drum at 128 m drives it; finite; brake holds
- pin in: max y = 188.00; 4 blocks remain one welded stack
- pin out: one 4000 kg block on catch at 136 m, climbable; remaining stack max y = 220.00
- well ledges every 8 m, climbable
- SKIN-W 120–220 real support
- 220 well-head standable; commit there
- Fold unverified

## Result record

pending.

---

## Mechanical close

### 8.1 Identity

| Field | Value |
|---|---|
| Atlas band | B02 Counterweight Well 120–220 |
| Slice | K2 pin + stack ride + SKIN-W |
| Live braids | SHAFT (stack). SKIN (west). FLOW not live |
| Transfer Plate | none new. TP-120 inherited. 220 well-head is a refuge ledger (atlas §8.1) |
| Modules | `MOD-CW-STACK`, `MOD-DRUM-LOW`, `MOD-CW-PIN`, `MOD-WELL-LEDGES` |
| Forbidden | `MOD-HEADER-W`, `MOD-SUMP-3`, `MOD-BLIND-STATION`, `MOD-GIRDER-T` |
| Capability consumed | none required. `CAP-NEEDLE` only affects whether the player arrived by cage |
| Capability authored | none |

### 8.2 Entry state

- player **can** be on TP-120 or 120 west stub (or still in AS-004/013)
- well xz `(-1.76, 25.30)`
- cage may sit parked north of the well after a land; treat as parked kinematic furniture
- west stub standable

### 8.3 Geometry

Well frozen. Stack must fit: plan 3.10 × 3.10.

**Authoritative occupancy:** stack xz = `(-1.76, 25.30)`. After a legal land, AS-005 parks the cage north. If the player arrived by SKIN, the well is already empty at 120.

#### MOD-CW-STACK (DESIGN TARGET)

    xz = (-1.76, 25.30)
    4 blocks, each half (1.55, 1.25, 1.55), mass 4000 kg
    block i height center = stack_top_y - 1.25 - 2.50*i   # i = 0 top … 3 bottom
    stack_top_y ∈ [120.20, 220.20] after pin-out
    stack_top_y ∈ [120.20, 188.20] pin-in
    speed: 4 blocks 0.55 m/s; 3 blocks 0.70 m/s
    raise force:
        F_4 = 16000 * 9.81 = 156960 N
        F_3 = 12000 * 9.81 = 117720 N
        F_rated = 160000 N  (DESIGN TARGET; stalls if extra freight is slung)

Player stands on top block. Moving support rank 2. Jump inherits v.

#### MOD-CW-PIN

    mass 32 kg (atlas 25–40 kg unaided)
    seated in top block, local (0.55, 0.20, 0.00)
    pickup radius 1.20 m
    pull only if stack |v| < 0.05 m/s and brake on
    after pull: carryable like CAP-HOOK5. Not a mission flag.

#### Timber catch

    source (-1.76, 136.00, 22.50)
    half (2.00, 0.20, 0.80)
    climbable
    dumped block snaps kinematic onto it, top ≈ 138.50 (2.5 m block)

#### MOD-DRUM-LOW

    room slab (4.00, 128.00, 31.00), half (4.00, 0.20, 3.00)
    lever local, radius 3.00 m (inherit cage lever law)
    access treads 120→128: inherit `kB00StairDY = 0.353`, `kB00StairDZ = 0.320`, 23 treads, run north from TP-120 north remainder
    i = 1…23: y = 120.20 + 0.353*i, z = 31.29 + 0.320*i, x = -1.76
    last y = 128.32

This is the CW drum. It is not the cage winch in the pit at y = 4.

#### MOD-WELL-LEDGES

    y = 128, 136, 144, 152, 160, 168, 176, 184, 192, 200, 208, 216, 220
    each: x = -4.90 (west lip), z = 25.30, half (0.80, 0.12, 1.20)
    climbable, player + 40 kg
    220 ledge is the **well-head**: extend half x to 4.00 so it meets AS-007’s north walk

#### SKIN-W 120–220

    x = -17.00
    z = 25.30
    y_top(i) = 120.20 + 0.400 * i
    i = 1…250
    last = 220.20
    start overlaps 120 west stub

### 8.4 Mechanism

Drum lever: Raise / Lower / Brake (same toggle class as campaign cage).

Pin-in travel: clamp stack_top_y to 188.20. At 188, stall `at_limit`. Ledges still exist above; player can step off at 184/188 and keep climbing.

Pin-out: bottom block (i=3) becomes independent kinematic, snaps to catch (needle-seat class). Remaining three: clamp max 220.20.

Catch exists before the dump. Do not drop 4000 kg as an un-held dynamic onto the player.

Energy 3-block 120→220: `117720 N × 100 m = 11.8 MJ` over 100/0.70 ≈ 143 s. Mean power ~82 kW. DESIGN TARGET for the drum room.

### 8.5 Occupancy

| Envelope | Effect |
|---|---|
| pin in, command up, y → 188 | stall at_limit |
| pin out, command up, y → 220 | stall at_limit |
| raise with brake on | stall |
| player in drum lever radius | drum live |
| dumped block vs catch | snap; climbable |
| SKIN-W | always legal |
| cage vs stack AABB | after land, cage parks north; SKIN arrival: cage not in well |

### 8.6 Causal path

See Objective. Next file starts on 220 well-head **or** 220 SKIN-W head.

### 8.7 Support handoff

| Step | Member | Y | Type | Inherit v? |
|---|---|---|---|---|
| 0 | TP-120 / 120 west stub | 120.20 | static | no |
| 1a | stack top | 120–220 | kinematic | **yes** |
| 1b | SKIN-W | 120–220 | static | no |
| 1c | dumped block + ledges | 136+ | static/kinematic | no |
| 2 | 220 well-head | 220.20 | static | no |

### 8.8 Failure

| Trigger | World | Can | Must not |
|---|---|---|---|
| miss stack while moving | fall in well | chute; ledges | kill plane at 120 |
| pin pulled in motion | rejected; pin stays | brake first | dump while slewing |
| flag `pin_pulled` | no new body | — | walking empty catch |

Well opening 5.00 vs capsule 0.70: chute **is** geometrically legal. Do not fake-deny.

### 8.9 Recovery

- never pull pin: ride to 188, climb ledges 188→220, or SKIN-W
- never board stack: SKIN-W from 120 stub
- dumped block: new floor at 136; down-climb ledges or stack
- fall: apron field; TP-120 catch if they steer
- two-way SKIN-W and ledges

### 8.10 Persist

Version 5. Import ≤4: pin in, 4 blocks, stack_top = 120.20.

Fields:

- `stack_top_y`, `stack_command`, `stack_brake`
- `cw_pin_x,y,z`, `cw_pin_held`, `cw_pin_in_stack`
- `block3_x,y,z`, `block3_dumped`

Commit: dwell on 220 well-head or SKIN-W y ≥ 220.00. Also inherit TP-120 commits.

### 8.11 Falsifiers

1. `wo014_stack_is_support` — board at 120; drum raise; support_entity_id is stack; support v.y > 0.
2. `wo014_pin_in_caps_188` — pin in; cannot exceed 188.5.
3. `wo014_pin_dump_is_a_body` — brake; pull pin; block3 on catch; player can stand on it at y ≥ 136; `cw_pin_in_stack` false.
4. `wo014_pin_out_reaches_220` — after dump, stack_top ≥ 220.
5. `wo014_skin_w_skips_pin` — pin still in; player y ≥ 220 on SKIN-W; stack_top still 120.2 ± 0.5.
6. `wo014_flag_is_not_a_block` — commit without pull; catch has no standable dumped mass.
7. `wo014_jump_inherits` — jump from moving stack; player vx/vz matches stack before air.
8. `wo014_prior_still_pass` — AS-001–013 + kernel PASS.

### 8.12 Exit state

- player **can** stand at y ≥ 220 on well-head **or** SKIN-W
- stack pose as left (pin in at ≤188, or dumped, or at 220)
- 220 well-head ledge exists
- `MOD-HEADER-W` / blinds / sump **do not** exist yet
- TP-120 still exists below
- next file: wet header on the north side of this 220 floor

---

**Stop. Do not begin the next file inside this one.**  
Next file: `03_EXECUTION/ASCENT/AS-007_WET_ISOLATION.md`
