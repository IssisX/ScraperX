# SCRAPERX — AS-007 WET ISOLATION (K3)

**Ascent Slice:** `AS-007`
**Lifecycle:** `PLANNED` — contract exists, no corresponding source
**Provenance:** ⚠ **imported from `ScraperX-Grok`, NOT re-derived.** Reference provenance only
**Implementation gate:** re-author against this branch first, then `AS-006` implemented
**Evidence:** none. A plan is never implementation evidence.
**Depends on:** `AS-001`–`AS-006`; kernel `WO-013` is regression substrate only;
`03_EXECUTION/PLANNING/ASCENT_PRE_RESOLUTION.md` §5 row `AS-007`

> **Provenance.** Adopted from `ScraperX-Grok`, where this slice was authored as `WO-015_WET_ISOLATION`.
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

Close atlas §7 **K3**: insert `CAP-BLIND` and drain `MOD-SUMP-3`, **or** climb north SKIN around the wet core, and stand on TP-340.

Blind + drain → header isolated, sump empty → `MOD-ISO-STAIR` is not a hazard → FLOW to 340 m.

This is not the plate shop. This is not `MOD-GIRDER-T`.

## Existing truth

After `AS-006` is in source:

- 220 well-head ledge at well xz, y = 220.20
- SKIN-W head at `(-17.00, 220.20, 25.30)`
- stack / pin as the player left them
- kernel `KX-SUMP` remains mill-only. Do not retitle it `MOD-SUMP-3`
- persist v5
- Fold-device execution is not proven

Atlas §14: band B03 fluid species may stay “wet and drainable.” DESIGN TARGET: water. Predicates do not depend on naming steam vs water.

Until AS-006 is coded, this file still inherits the DESIGN TARGET exit of AS-006. Do not implement this file before AS-006 is in source.

## Authority

- Laws 2–7, 11–18, 21–27, 29, 32
- GDD §§7.2–7.4, 11, 15–17, 23–24
- Atlas §§3, 4 (`CAP-BLIND`), 5 (TP-340), 6 band B03, 7 K3, 8.1, 8.3, 12, 13, 14
- TDD §§6, 8–11, 14 (process)
- WO-013: grate/sump class. Do not reopen. Do not gate kernel on `CAP-BLIND`
- `AS-006_CW_PIN.md` §8.12
- protocol AS-007

## Owner

Native 90 Hz C++/Jolt. Godot presents.

## Allowed seam

New bodies: `MOD-HEADER-W`, `MOD-SUMP-3`, `MOD-BLIND-STATION` + blind body, `MOD-ISO-STAIR`, `MOD-WET-LOCK`, TP-340, SKIN-N 220–340.

Reuse: WO-013 isolate/drain inventory, hook5-style carryable, occupancy stall (wet lock like hook5 door / dog).

Do not retitle `KX-SUMP`. Do not add `MOD-SHOP-CRANE`.

## Required causal path

FLOW (K3):

    ACT[take CAP-BLIND from MOD-BLIND-STATION; insert into MOD-HEADER-W; vent; open drain]
      → STATE[header isolated; sump inventory → dry]
      → WORLD[ISO-STAIR collision is ordinary; WET-LOCK can travel]
      → PLAY[walk 220→340 on the stair; TP-340]

Dump (atlas coupling 2):

    ACT[open dump without blind]
      → STATE[sump fills; stair hazard true]
      → WORLD[ISO-STAIR not a route; SKIN-N still legal]
      → PLAY[north SKIN to 340]

Skip:

    ACT[ignore blinds]
      → STATE[header live; lock closed]
      → WORLD[stair remains a hazard]
      → PLAY[SKIN-N 220→340]

Stitch from previous:

    220 well-head → north 8 m of timber → blind station and header
    220 SKIN-W head → walk 220 ring north → SKIN-N
    stack does not become the 220–340 elevator

Stitch to next (`AS-008_SHOP_GIRDER`, not in this pack):

    TP-340 is the shop floor lip. Girder seating is the next ticket.

## Forbidden shortcuts

- `header_safe` flag as WORLD without blind pose + inventory
- stair mesh-swap without collision change
- SKIN-N walled because the header is live
- kernel sump gated on `CAP-BLIND`
- teleport 220→340
- hard-fail mission (atlas §8.2: none)
- Fold claimed

## Implementation scope

Native + falsifiers; Godot twins; persist v6.

## Out of scope

`AS-008_SHOP_GIRDER`. `CAP-TROLLEY`. Fold 45 FPS. Art/VO. Reopening K2.

## Proof path

§8.11; screenshot live header vs drained stair; kernel WO-013 still isolates mill sump without `CAP-BLIND`. Pending.

## Completion

- `CAP-BLIND` is a 32 kg body at the station
- insert is a pose in the header, not a bool
- drain empties `MOD-SUMP-3` only if isolated
- live header: stair is a hazard (push / unsafe grate class); wet lock θ frozen
- isolated+dry: stair is ordinary support 220–340; lock opens
- SKIN-N reaches 340 with header still live
- TP-340 standable; commit
- Fold unverified

## Result record

pending.

---

## Mechanical close

### 8.1 Identity

| Field | Value |
|---|---|
| Atlas band | B03 Wet Isolation 220–340 |
| Slice | K3 blind + drain vs SKIN-N |
| Live braids | FLOW (stair). SHAFT (not required). SKIN (north) |
| Transfer Plate | **TP-340** |
| Modules | `MOD-HEADER-W`, `MOD-SUMP-3`, `MOD-BLIND-STATION`, `MOD-ISO-STAIR`, `MOD-WET-LOCK` |
| Forbidden | `MOD-GIRDER-T`, `MOD-SHOP-CRANE`, `MOD-TRAVELER` |
| Capability authored | `CAP-BLIND` (carryable) |
| Capability consumed | none required to skip |

### 8.2 Entry state

- player **can** stand at 220 well-head or SKIN-W 220
- header is **live** (default)
- sump inventory = 1.0 (full enough to make the stair a hazard)
- wet lock closed
- stack/pin frozen as left

### 8.3 Geometry

Service risers sit on the **north** face of the core (atlas §2.2). DESIGN TARGET north of the well.

    well xz = (-1.76, 25.30)
    north core ≈ source z = 45 (atlas y = +45 m, 90 m frame)

#### 220 north walk (DESIGN TARGET)

Timber from well-head to blind station. Climbable.

    center (-1.76, 220.20, 35.00)
    half (3.00, 0.12, 8.00)

#### MOD-BLIND-STATION

    source (2.40, 221.00, 42.00)
    rack half (0.80, 0.90, 0.50)
    blind body: 32 kg, half (0.25, 0.35, 0.08), seated (2.40, 221.10, 42.00)
    pickup radius 1.20 m (hook5 class)

`CAP-BLIND` WORLD: `!blind_in_rack` (held or inserted or dropped). Header isolation is a **separate** pose predicate.

#### MOD-HEADER-W

    line along x, source z = 44.00, y = 241.00
    pipe half (8.00, 0.35, 0.35), center (0.00, 241.00, 44.00)
    spectacle slot at (0.00, 241.00, 44.00)
    insert: blind center within 0.20 m of slot, |yaw| ≤ 0.15 rad, not held
    then kinematic snap; `header_isolated = true` (derived)

Vent: station 1.50 m west of slot, radius 1.20 m, toggle. Isolation without vent: drain rate 0 (air-locked). With vent: drain legal.

DESIGN TARGET pressure: 6 bar gauge. Not a solver. If not isolated, `MOD-ISO-STAIR` uses unsafe-grate rules (WO-013 class).

#### MOD-SUMP-3

    volume under the stair, center (0.00, 236.00, 40.00)
    half (4.00, 4.00, 3.00)
    inventory 0–1, default 1.0
    dry threshold 0.08 (inherit `kSumpDryThreshold`)
    drain rate 0.28 / s if isolated **and** vented **and** drain open
    fill rate 0.45 / s if dump open and not isolated
    drain wheel at (3.20, 221.50, 40.00), radius 2.80 m (inherit sump station)

Water collision: when inventory > 0.08, fill the stair throat as a **hazard volume** (push + not legal support), not a kill plane.

#### MOD-ISO-STAIR

    220.20 → 340.20, north run
    inherit rise 0.353, run 0.320
    treads n = (340.20-220.20)/0.353 ≈ 340
    every tread is a real box (same as STAIR-A)

    x = 0.00
    z_0 = 38.00, dz = +0.320 (north)
    y_0 = 220.20

When **not** (isolated and dry): support_rank 0 on treads that sit inside the sump AABB (y 232–248). Upper treads above the flood remain rank 1 so the player can see the stair die mid-flight.

When isolated and dry: all treads rank 1.

#### MOD-WET-LOCK

    door at y = 260.00, z = 44.00, x = 0.00
    half (1.10, 1.20, 0.12)
    occupancy/process stall: θ = 0 while `!header_isolated`
    after isolated+dry: θ → 1.20 like hook5 door
    throat ≥ 0.90 m
    blocks the stair landing at 260 until open

#### SKIN-N 220–340

    x = 0.00
    z = 45.00
    y_top(i) = 220.20 + 0.400 * i
    i = 1…300
    last = 340.20
    always climbable

220 ring north stub: from well-head/SKIN-W walk to (0.00, 220.20, 45.00).

#### TP-340

    center (0.00, 340.00, 25.30) extending north
    half (12.00, 0.20, 16.00)
    climbable
    reconnect: stair top, SKIN-N top, and a lip back toward the well for later SHAFT (AS-008 guides — not built here)

Commit: dwell, y ≥ 340.00, support is TP-340.

### 8.4 Mechanism

Predicates (derived):

    cap_blind       = !blind_in_station_rack
    header_isolated = blind snapped in slot
    header_vented   = vent_open
    sump_dry        = inventory ≤ 0.08
    stair_safe      = header_isolated AND header_vented AND sump_dry
    lock_free       = header_isolated

Drain does nothing unless `header_isolated && header_vented`.

Dump without blind: `inventory → 1.0` at fill rate; stair_safe false.

Kernel WO-013 path untouched (`!intake_rise_` mill).

### 8.5 Occupancy

| Envelope | Effect |
|---|---|
| live header, stair in sump AABB | rank 0; push |
| wet lock, !isolated | θ frozen 0 |
| isolated+dry | lock travels; stair rank 1 |
| SKIN-N | always |
| blind in slot | isolation true |
| blind in rack | cap_blind false; isolation false unless already snapped (removing from slot un-isolates) |

Unseat blind: player pickup from slot; header live again; lock closes; if drain still open, dump hazard returns. Persistent. Legal.

### 8.6 Causal path

See Objective. Next ticket (`AS-008_SHOP_GIRDER`) inherits TP-340 as the shop floor.

### 8.7 Support handoff

| Step | Member | Y | Type |
|---|---|---|---|
| 0 | 220 well-head / SKIN-W | 220.20 | static |
| 1a | ISO-STAIR (if safe) | 220–340 | static |
| 1b | SKIN-N | 220–340 | static |
| 2 | TP-340 | 340.20 | static |

### 8.8 Failure

| Trigger | World | Can | Must not |
|---|---|---|---|
| walk live stair in flood | no support / push | SKIN-N; back up | auto-drain |
| drain without blind | inventory stays / fills | SKIN | stair_safe true |
| flag `made_safe` | no collision change | — | walking live water |
| kernel sump without CAP-BLIND | mill grate still works | — | campaign gate on kernel |
| fall from 340 | long chute; apron or 220 well-head if steered | chute | powered lift |

### 8.9 Recovery

- lose the blind down the well: last commit; station does not respawn a second blind
- never take the blind: SKIN-N
- un-isolate after drain: stair dies again; SKIN still up
- two-way SKIN-N and, when safe, two-way stair

Atlas §3: TP-340 keeps a recovery that does not need `CAP-BLIND` — that **is** SKIN-N.

### 8.10 Persist

Version 6. Import ≤5: header live, inventory 1.0, blind in rack, lock closed.

Fields:

- `blind_x,y,z`, `blind_held`, `blind_in_rack`, `blind_inserted`
- `header_vent_open`, `sump_inventory`, `sump_drain_open`
- `wet_lock_angle`

`stair_safe` / `cap_blind` derived. Commit on TP-340 dwell. Do not reset process on “band load.”

### 8.11 Falsifiers

1. `wo015_live_stair_is_not_a_floor` — default spawn 220; walk stair into sump AABB; support is never an ISO-STAIR id inside the flood.
2. `wo015_lock_frozen_while_live` — θ < 0.10 after 3 s.
3. `wo015_blind_is_a_body` — pick up; rack empty; `cap_blind` true; body not at seated pose.
4. `wo015_insert_then_drain` — insert; vent; drain; inventory ≤ 0.08; stair support rank 1; lock θ ≥ 1.00; player y ≥ 340 via stair.
5. `wo015_drain_without_blind_fails` — drain wheel only; inventory > 0.5; stair still unsafe.
6. `wo015_skin_n_skips_process` — header live; player y ≥ 340 on SKIN-N; inventory still ≥ 0.9.
7. `wo015_unseat_blind_relives` — after dry, remove blind; lock closes; flood treads rank 0 again.
8. `wo015_flag_is_not_dry` — commit at 220 without insert; TP-340 does not appear under the player.
9. `wo015_kernel_sump_ungated` — mill `SumpLanding` still isolates without `cap_blind`.
10. Prior AS-001–014 + kernel PASS.

### 8.12 Exit state

- player **can** stand on TP-340 (y ≥ 340) via stair **or** SKIN-N
- header/sump/blind as left (live skip is legal)
- `MOD-GIRDER-T` does not exist yet
- 220 well-head and TP-120 still exist below
- next file: plate shop on TP-340, seat `MOD-GIRDER-T`

---

**Stop. Do not begin the next file inside this one.**  
Next file: `03_EXECUTION/ASCENT/AS-008_SHOP_GIRDER.md` — `UNAUTHORED`, does not exist yet
