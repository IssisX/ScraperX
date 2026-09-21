# SCRAPERX — WO-017 NEEDLE SEAT (SEAT MOD-NEEDLE-A/B AT +96 m)

**Work Order:** `WO-017`
**Status:** READY after `WO-016_HOOK5_RACK` is in source.
**Depends on:** `WO-014` and `WO-015` in source and green; `WO-016` in source
(`CAP-HOOK5` acquirable); kernel falsifiers green; protocol §8.

> **Authoring note.** Replaces the `ScraperX-Grok` port (their `WO-012`) for the
> reason `WO-015` and `WO-016` were replaced — inherited constants describing a
> world that does not exist here. Its falsifier *shape* is excellent and is kept;
> its geometry is re-derived against this branch.

## Objective

Close Atlas §6 band **B01 — Transfer Hall**'s structural problem: seat
`MOD-NEEDLE-A` and `MOD-NEEDLE-B` into `MOD-NEEDLE-POCKETS` at `+96 m`.

Atlas: *"First structural seating problem. A missing needle beam means the SHAFT
cage cannot take a landing at 120 m without racking the guides."*

The slice owns **one machine and one capability consumption**:

1. `MOD-CAGE-1` — the first machine in the game the player **rides while
   driving**. The yard jib is a ground machine and station-gated at grade; a
   personnel cage has a car station, because that is what a personnel cage is.
   The "you cannot ride your own crane" rule is not a physics law, it is a
   property of where a machine's controls are — and this slice is where that
   becomes legible by contrast.
2. `CAP-HOOK5` is **consumed physically**: the hook block *is* the sling. There
   is no capability check anywhere. A needle rises because a 36 kg body is
   bridging the winch hook and the needle's padeye, and for no other reason.

## Existing truth

Frozen from `WO-014` source at `af8beca`; inherited from `WO-015`/`WO-016` as
**DESIGN TARGET** until those land:

| Datum | Value | Source |
|---|---|---|
| `MOD-HALL-DECK` walking surface | `40.1872 m`, `x ∈ [-10, 10]`, `z ∈ [-122.5, -107.7]` | `WO-015` §8.3 |
| `MOD-STAIR-A` well in that deck | `x ∈ [-7.51, -1.51]`, `z ∈ [-119.2, -114.8]` | `WO-015` §8.3 |
| bascule hinge | `(7.856, 32.1872, -112.5)`, swings `y ∈ [16, 32]` | `WO-015` §8.3 |
| `MOD-SKIN-LADDER-S` head | rung 25 top `40.1872 m`, fascia `z = -106.4` | `WO-015` §8.3 |
| `MOD-HOOK5` block | `36 kg`, half `(0.30, 0.25, 0.30)`, carried on a point constraint | `WO-016` §8.4.3 |
| carrying blocks traversal | vault, mantle and hang all refuse while held | `WO-016` §8.4.3 |
| max `MOD-YARD-JIB` hook | `11.05 m` | `WO-014`, verified |
| persist | v3 | `WO-016` §8.10 |

Proven primitives this slice reuses rather than reinvents:

- `add_motorized_slider` — `EMotorState::Velocity` with `SetForceLimit`, so an
  overloaded drum **is not held**; it sags at the rate the deficit actually
  produces, read back from the solver. `WO-011` and `WO-014` both proved this.
- the `WO-012` seat/unseat pattern: pins created and removed at runtime with
  `create_constraint(..., track_for_teardown = false)`, with the seat predicate
  **gated on the command** so it cannot re-seat the tick after release.
- `restore_needle_topology()` — the reconciliation shape this slice copies.

`kSupportNormalThreshold 0.55`, `kPlayerRadius 0.35`, `kPlayerMassKg 85.0`,
`kLethalImpactSpeedMps 20.0`.

Entity IDs and spawns are assigned when this slice is coded.

## Authority

- Governing Laws 2–7, 12–18, 21–27, 29, 32
- GDD §§4, 6, 7.2–7.4, 11, 15–17, 23, 24
- Atlas §§3, 4 (`CAP-HOOK5`), 5, 6 band B01, 7 K1, 8.1, 8.3, 12, 13
- TDD §§6, 8–11, 14
- `WO-016_HOOK5_RACK.md` §8.12
- `SECTION_PRE_RESOLVE_PROTOCOL.md` §§4, 6, 8, 9

## Owner

Native 90 Hz C++/Jolt. Godot presents. Mission/UI observe predicates only.

## Allowed seam

New B01 bodies: `MOD-CAGE-1` and its well, `MOD-NEEDLE-A/B`, their `48 m` racks,
`MOD-NEEDLE-POCKETS` at `96 m`, `MOD-GUIDE-RACK`. Reuse the motorized slider,
the seat/unseat topology pattern, the carry constraint, moving-support identity.

Do not retitle `KX-NEEDLE` — the kernel needle at `x ≈ 200` stays regression
substrate and **must remain ungated by `CAP-HOOK5`** (falsifier 8). Do not
author `TP-120`, the `120 m` landing, `MOD-EAST-OUTRIGGER` or `SKIN-E` — all
`WO-018`. Do not lengthen `MOD-YARD-JIB`.

## Required causal path

```
ACT  [carry CAP-HOOK5 up MOD-STAIR-A to MOD-HALL-DECK, board MOD-CAGE-1,
      drive the car station down to the 48 m racks]
  → STATE[support identity = the cage, a moving support; the car station is live
          because the player is standing on the car, not because a flag says so]
  → WORLD[the cage is beside MOD-NEEDLE-A on its rack]
  → PLAY [the rack deck and the needle itself are standable]

ACT  [step onto the rack, set the hook block into the needle's padeye while the
      winch hook is lowered to it]
  → STATE[two point constraints exist: hook-to-block and block-to-padeye. The
          block is the only load path. With it in hand or on the ground there is
          no path at all]
  → WORLD[the needle is rigged]
  → PLAY [the winch can take it]

ACT  [raise the winch]
  → STATE[drum force limit 32 000 N against 25 359 N of cage + player + needle;
          net 6 641 N, so it rises. With both needles rigged it is 41 055 N and
          the drum sags -- not refused, sagged, at the rate the deficit makes]
  → WORLD[MOD-NEEDLE-A arrives at 96 m and settles into MOD-NEEDLE-POCKETS;
          two pins are created; guide_offset for that line collapses to ~0]
  → PLAY [the needle is a walkable 18.4 m span at 96 m -- literally, by its own
          collision, the way WO-012's kernel needle already is]

ACT  [repeat for MOD-NEEDLE-B]
  → STATE[guide_offset for both lines within tolerance]
  → WORLD[MOD-CAGE-1's drum no longer stalls above 118 m]
  → PLAY [the 120 m landing is reachable -- which WO-018 authors]
```

**Atlas coupling 2 — one needle only:**

```
ACT[seat A, leave B on its rack]
  → WORLD[a walkable springy span at 96 m; guide_offset on line B still ~48 m]
  → PLAY[a parkour beam to the facade, and the cage still stalls at 118]
```

**Atlas coupling 3 — neither needle:** `WO-018` authors the SKIN route to 120 m.
Nothing in this slice may block it.

**Illegal:**

```
MISSION_FLAG[needles_seated] → WORLD[the cage passes 118]
ACT[raise with no block rigged] → PLAY[the needle lifts]
ACT[commit a checkpoint] → PLAY[a standable span at 96 m]
gating the kernel KX-NEEDLE on CAP-HOOK5
```

## Forbidden shortcuts

- `CAP-HOOK5` as a boolean consulted by an attach routine
- teleporting or snapping a needle to its seat rather than lowering it
- an unlimited drum, or a drum that refuses an overload instead of sagging
- a `guide_offset` that is stored rather than measured from body positions
- walling off any SKIN route to protect the seating sequence
- retitling `KX-NEEDLE` as `MOD-NEEDLE-A`
- claiming Fold-device execution

## Implementation scope

Native bodies, the cage drum and car station, the rigging topology, the guide
measurement, falsifiers; Godot twins; persist v4; CI proof lines.

## Out of scope

`WO-018_CAGE_OR_SKIN` — the `120 m` landing, `TP-120`, `MOD-EAST-OUTRIGGER`,
`SKIN-E`. Fold 45 FPS. Art/VO. Reopening `WO-014`–`WO-016`.

## Completion

- the yard jib cannot reach the `48 m` racks, and a falsifier says so in metres
- with no block rigged, a raise command moves no needle
- with the block rigged, one needle rises and seats
- two needles rigged at once sags the drum; neither arrives
- one needle seated is a real walkable span and the cage still stalls at `118 m`
- both seated and the stall is gone
- no flag, commit or reload produces a span at `96 m`
- the kernel `KX-NEEDLE` still seats with no `CAP-HOOK5` anywhere near it
- Fold install / on-device play remain unverified

## Result record

pending.

---

## Mechanical close

### 8.1 Identity

| Field | Value |
|---|---|
| Atlas band | B01 — Transfer Hall, `40–120 m`, the seating problem only |
| Slice | seat both needles at `+96 m`; clear the cage's `118 m` interlock |
| Chain | K1 |
| Live braids | SHAFT (`MOD-CAGE-1`). SKIN inherited from `WO-015`, untouched |
| Transfer Plate | none. `TP-120` is `WO-018` |
| Modules allowed | `MOD-CAGE-1`, `MOD-NEEDLE-A/B`, `MOD-NEEDLE-POCKETS`, `MOD-GUIDE-RACK`, the `48 m` racks |
| Modules forbidden | `MOD-EAST-OUTRIGGER`, `SKIN-E`, `TP-120`, `MOD-CW-STACK` |
| Capability consumed | **`CAP-HOOK5`**, physically — the block is the load path |
| Capability authored | none |

### 8.2 Entry state

Player on `MOD-HALL-DECK` at `40.1872 m`, arrived by either braid. `CAP-HOOK5`
held, or set down anywhere — including at grade, in which case the slice cannot
start until it is fetched, which is a cost, not a lock.

Machine poses, all legal: pack anywhere in the jib's circle; bascule at either
stop; cage door travelled; bar wherever it was dropped. None is read here.

Persist: inherit v3.

### 8.3 Geometry

| ID / member | source (x, y, z) | extents (half) | climbable | rating |
|---|---|---|---|---|
| `MOD-CAGE-1` well (opening in the hall deck) | `(0.0, 40.1872, -110.5)` | `(1.40, —, 1.50)` | opening | `2.80 × 3.00 m` |
| `MOD-CAGE-1` platform | driven by its drum | `(1.20, 0.125, 1.20)` | **yes, moving support** | `2000 kg` payload |
| `MOD-CAGE-1` car station rail | on the car | `(1.20, 0.55, 0.06)` ×2 | no | control is here |
| drum head | `(0.0, 121.0, -110.5)` | `(0.60, 0.40, 0.60)` | no | slider anchor |
| `MOD-NEEDLE-A` seated | `(0.0, 96.0, -112.8)` | `(9.20, 0.25, 0.45)` | **yes, seated only** | player + `CAP-HOOK5` |
| `MOD-NEEDLE-B` seated | `(0.0, 96.0, -108.2)` | `(9.20, 0.25, 0.45)` | **yes, seated only** | player + `CAP-HOOK5` |
| `MOD-NEEDLE-POCKETS` west ×2 | `(-9.0, 95.75, -112.8 / -108.2)` | `(0.50, 0.30, 0.65)` | no | tapered seat |
| `MOD-NEEDLE-POCKETS` east ×2 | `(+9.0, 95.75, -112.8 / -108.2)` | `(0.50, 0.30, 0.65)` | no | tapered seat |
| `48 m` racks ×2 | `(0.0, 48.11, -112.8 / -108.2)` | `(1.40, 0.25, 0.65)` | yes | rack deck |
| `MOD-NEEDLE-A/B` stowed | `(0.0, 48.36, -112.8 / -108.2)` | as above | yes | on the rack |
| `MOD-GUIDE-RACK` rails ×2 | `(0.0, 80.0, -112.0 / -109.0)` | `(0.10, 40.0, 0.10)` | no | `y ∈ [40, 120]` |

**Clearances, in metres:**

- cage `z ∈ [-111.7, -109.3]`; needle A `z ∈ [-113.25, -112.35]` → **`0.65`**;
  needle B `z ∈ [-108.65, -107.75]` → **`0.65`**. The cage rises **between** the
  two seated needles, which is why they are `4.60 m` apart and not the `18 m` of
  their span
- well opening `z ∈ [-112.0, -109.0]` vs cage `z ∈ [-111.7, -109.3]` → `0.30`
  running clearance. No boundary is coincident
- cage well vs `WO-015`'s stair well (`z` to `-114.8`) → **`2.80`**
- needle walking width `0.90 m` against a `0.70 m` capsule → **`0.20`** spare.
  This is deliberately tight: Atlas calls it *"a springy span"* and it should
  read as a beam you balance on, not a footbridge
- needle length `18.40 m` over pockets `18.00 m` apart → `0.20 m` of bearing each
  end. Pockets are **tapered**, and the seated needle's underside sits `0.06 m`
  clear of the pocket floor before its pins take it, so no two box boundaries are
  ever coincident — the `JPH::BoxShape` convex-radius jam that stopped `WO-012`'s
  beam `0.04 m` short of its seat, and bound `WO-014`'s dog at `0.31 rad`, is
  designed out here rather than rediscovered a third time
- seated needle top `96.25 m`; the cage passes at `y = 96` with its platform
  `0.125 m` thick, so nothing fouls
- the `48 m` racks and the seated pockets share `x` and `z`, so a needle rises on
  a **pure vertical** line. Its rigging does not
- drum head at `121.0 m`; this slice limits cage travel to `118.0 m`. `WO-018`
  takes it to `120`

### 8.4 Mechanism

#### 8.4.1 `MOD-CAGE-1` — a ridden machine, and why that is not a contradiction

A motorized vertical slider between the drum head and the car, exactly the
primitive `WO-011`'s hoist and `WO-014`'s winch already use:

```
add_motorized_slider(drum_head, cage, 0.0, 77.81, 32 000 N, &cage_drum)
travel  y ∈ [40.19, 118.00]   (WO-018 extends the top to 120.00)
speed   0.90 m/s
```

The car station is live **while the player's `support_entity_id` is the cage**.
That is not a radius test and not a flag: it is the contact-ranked support
identity the game already computes every tick. Step off and the controls go dead
mid-travel, which is correct and is a stated hazard in §8.8.

This is the first machine the player rides while driving, and the contrast is the
point. `MOD-YARD-JIB`'s pendant is bolted to a catwalk at grade, so nothing can
ride it — `WO-015` §8.4.5 showed that is what actually closes the Atlas's third
B00 exit, not boom length. `MOD-CAGE-1` has a car station because a personnel
cage has one. Same engine, same station-gating shape, opposite consequence.

**Capacity, worked:**

```
rated drum force                 32 000 N        DESIGN TARGET
cage 900 kg + player 85 kg                =  9 663 N   net +22 337 N, rises
  + one needle 1600 kg                    = 25 359 N   net  +6 641 N, rises
  + two needles 3200 kg                   = 41 055 N   short  9 055 N, SAGS
```

Two needles is not *refused*. The motor is `EMotorState::Velocity` with a force
limit, so it applies its 32 kN, loses, and the whole assembly descends at the
rate a 9 055 N deficit on 4 185 kg produces — `2.16 m/s²`. The player watches it
go down. That is the same honesty `WO-011`'s capacity stand and `WO-014`'s 9 t
proof load already enforce, and it is why the falsifier asserts *sag*, not a
rejected command.

#### 8.4.2 The rigging — `CAP-HOOK5` as a load path, not a permission

There is no attach check and no capability flag. The winch hook and the needle's
padeye are simply **too far apart to be connected by anything but the block**:

```
winch hook lowered, rest pose     (0.0, y_hook, -110.5)   on the car centreline
MOD-NEEDLE-A padeye               (0.0, 48.61, -112.8)    2.30 m off that line
hook block                        36 kg, 0.60 m across
```

Rigging is two point constraints created at runtime:

```
rig_block_to_hook():
    requires  block carried or within 0.80 m of the hook
              |v_hook| ≤ 0.20 m/s, Rig command this tick
    effect    release the carry constraint if held; create hook→block

rig_block_to_padeye():
    requires  hook→block exists
              |block − padeye| ≤ 0.70 m, |v_needle| ≤ 0.10 m/s
              Rig command this tick
    effect    create block→padeye

unrig():  Release command; removes block→padeye, then hook→block.
```

All three created with `track_for_teardown = false`, one owner, removed on every
path — the `WO-012` rule, because `ConstraintManager::Remove` asserts on an
already-invalidated index. Every predicate is **gated on its command**, not on
pose alone, for the reason `WO-012` found by observation: a pose-only predicate
re-fires the tick after release, while the body is still at the seat pose with
near-zero velocity.

**The off-plumb pickup is a real hazard, not an oversight.** The padeye sits
`2.30 m` off the hook's line, so the rope starts at an angle and the needle swings
to plumb the instant it leaves the rack:

| hook above the padeye | rope off plumb |
|---|---|
| `2.0 m` | `49.0°` |
| `4.0 m` | `29.9°` |
| `6.0 m` | `21.0°` |

Specify the winch hook lowered to at least **`5.0 m`** above the padeye before
rigging, giving `≤ 25°`. Rigging closer is legal and the swing is the price —
`1600 kg` swinging `2.3 m` through the shaft is exactly the sort of thing that
puts a player off a rack deck, and it must be left to do so.

#### 8.4.3 Seating — the `WO-012` pattern at campaign scale

```
seat_needle(n):
    requires  both ends within 0.18 m of their pocket seats
              |v_needle| ≤ 0.05 m/s
              the winch is not commanded up this tick
    effect    two PointConstraints, needle end ↔ pocket

unseat_needle(n):
    requires  seated, winch commanded up past the release threshold
    effect    remove both pins
```

The gate on *"not commanded up"* is `WO-012`'s third defect, fixed in advance: a
real motor needs real time to move a beam, so the beam sits at the seat pose with
near-zero velocity for at least one tick after the pins come out, and an
ungated predicate re-seats it before a held raise can ever take effect.

#### 8.4.4 `MOD-GUIDE-RACK` — a declared lumped model

Atlas: *"Cage guides that stay out of tolerance until needles are seated."*

**Measured every tick from real body positions:**

```
guide_offset = max over both needles, both ends, of
               |needle_end_position − pocket_seat_position|

both seated   ≤ 0.02 m
one seated    ≈ 47.6 m   (the other needle is still on its rack at 48.36)
neither       ≈ 47.6 m
```

The interlock: while `guide_offset > 0.02` **and** the cage is above `118.0 m`,
the drum's target velocity is driven to zero. A real machine refusing a state it
cannot survive — the same class as the overload sag above, and nothing is stored.

**What this abstracts, stated plainly.** A literal model would have the rails as
slender dynamic members that genuinely deflect under the cage's shoe loads and
are genuinely braced by the needles. That is a flexible-body problem this engine
does not do well, and forcing it with a chain of short segments would produce a
jittery mechanism whose failures look like bugs. So the *racking* is lumped into
one measured distance, and declared here, in the same way `WO-013`'s sump lumps a
fluid into one scalar inventory and says so.

What is **not** lumped, and needs no model at all: the seated needle is a real
`18.40 m` collidable span at `96 m` that the player walks. Atlas coupling 2 — one
needle seated, a springy parkour beam to the facade, cage still stalled — is
literal geometry. Falsifier 7 proves a flag cannot produce it.

#### 8.4.5 Why the yard jib cannot do this job

```
max MOD-YARD-JIB hook height   11.05 m
MOD-NEEDLE racks               48.36 m
deficit                        37.31 m
```

Falsifier 1 holds the jib's hoist at its limit and asserts every jib-driven body
stays below `y = 12`. This is not decoration: it is what makes `MOD-CAGE-1`
necessary, and it keeps a later slice from quietly closing the gap with a boom
change. `WO-015` §8.4.5 and `WO-016` §8.4.4 record the same jib limit reached
from two other directions.

### 8.5 Occupancy and interlocks

| Envelope | Effect |
|---|---|
| player's support identity is the cage | car station live |
| player steps off mid-travel | station dead; the cage keeps its last command |
| gross load `> 3 262 kg` | drum sags; it never refuses |
| `guide_offset > 0.02` and cage `y > 118.0` | drum target driven to zero |
| needle ends within `0.18 m` of seats, settled, winch not up | pins created |
| block not bridging hook and padeye | no load path exists at all |

`guide_offset`, `needle_seated`, `rigged` and `car_station_live` are **derived**
every tick from pose and constraint topology, for the HUD and the falsifiers.
The single branch that reads any of them is the `118 m` interlock, which reads
`guide_offset` — a measured distance between two real bodies, not a bit.

### 8.6 Required causal path

See above. The next file begins with the cage able to pass `118 m` and nothing
built at `120`.

### 8.7 Support / traversal handoff

| Step | Member | source y | type | inherits v? |
|---|---|---|---|---|
| 0 | `MOD-HALL-DECK` | `40.1872` | static | no |
| 1 | `MOD-CAGE-1` platform | `40.19 → 118.0` | **kinematic** | **yes** |
| 2 | `48 m` rack deck | `48.36` | static | no |
| 3 | `MOD-NEEDLE-A/B` on the rack | `48.61` | dynamic, at rest | yes |
| 4 | `MOD-NEEDLE-A/B` seated | `96.25` | **dynamic, pinned** | yes |
| 5 | back onto the cage | `→ 118.0` | kinematic | yes |

Rows 1 and 4 both belong in `entity_is_moving_support`. Row 4 matters: a seated
needle is pinned, not static, so a player standing on it while it settles
inherits its point velocity under the `WO-002` law. `WO-012` already proved
exactly this for the kernel needle, so the machinery exists.

### 8.8 Failure states

| Trigger | World | Player can still | Must not happen |
|---|---|---|---|
| raise with nothing rigged | winch rises empty | come back down and rig | the needle following a flag |
| rig both needles, raise | drum sags at `2.16 m/s²` | unrig one | a refusal message instead of physics |
| step off the car mid-travel | station dead, cage holds its command | ride the next one | the cage stopping because the player left |
| needle swings on pickup | `1600 kg` through the shaft | be somewhere else | the swing being damped away |
| player struck by the swinging needle | shoved, possibly off the rack | chute — it is `48 m` to the apron | a scripted exception |
| seat one, try to pass `118 m` | drum target zero | ride down, seat the other | an invisible wall; the drum stalls, it does not deny |
| fall down the well | free fall up to `78 m`, `39 m/s`, lethal | chute; the well is `2.80 × 3.00 m`, legal clearance | fake-denying the chute |
| drop `CAP-HOOK5` down the well | it lands on the hall deck or the apron | fetch it | the block despawning |

### 8.9 Recovery

Atlas §3. Nothing strands:

- **block left at grade** → walk down and get it; the stair is open
- **needle left dangling on the hook** → lower it back onto its rack, or onto the
  hall deck, or drop it; it is a body and it comes to rest somewhere reachable
- **one needle seated, stuck at 118** → the seated needle is itself a route to
  the facade (Atlas coupling 2), and `WO-018` authors SKIN to `120 m` for the
  case where neither is ever seated
- **cage left at the top with the player at the bottom** → the car station is on
  the car, so the cage cannot be recalled. Both braids to `40 m` remain open and
  `WO-018`'s SKIN continues past it. **This is the one place where a later slice
  must not regress**: if `WO-018` makes the `120 m` landing the only way onward,
  a cage parked at `118` with the player at `40` is a soft-lock. Flagged here so
  `WO-018` answers it rather than discovering it.

### 8.10 Persist

**Persist version 4.**

```
needle_a_x,y,z / needle_b_x,y,z    float
needle_a_seated / needle_b_seated  bool
rigged_hook_block / rigged_padeye  uint64   which body, 0 for none
cage_y                             float
```

`restore_needle_topology()` extends to the two campaign needles and the rigging
chain; reconcile **before** the next tick reads contacts. Import of a v3 blob
assumes both needles on their racks, nothing rigged, cage at `40.19`.

### 8.11 Falsifiers (deterministic proof)

1. `wo017_jib_cannot_reach_the_racks` — hold the yard jib's hoist at its limit
   for 60 s. Hook, pack and player all stay below `y = 12.0`, against racks at
   `48.36`.
2. `wo017_no_block_no_lift` — `CAP-HOOK5` left at grade. Lower the winch to the
   padeye, command Rig, raise for 30 s. Needle A's `y` stays within `0.05 m` of
   `48.36`.
3. `wo017_block_is_the_load_path` — with the block rigged, the same command
   sequence raises needle A past `y = 90` within 90 s.
4. `wo017_two_needles_sag_the_drum` — rig both, command up. The cage's `y`
   **decreases**, and neither needle reaches `96`. Assert descent, not refusal.
5. `wo017_one_seat_is_a_real_span` — seat A only. `support_entity_id` is needle A
   with the player at `x = 0`, `y ≥ 96.25 + 0.70`, over open shaft. `guide_offset
   > 1.0`. A cage command up at `y = 117` drives the drum target to zero.
6. `wo017_both_seats_clear_the_interlock` — seat A and B. `guide_offset ≤ 0.02`,
   and a cage command up at `y = 117` does **not** zero the target. The cage
   reaches `118.0` and stops on travel, not on interlock.
7. `wo017_flag_is_not_a_beam` — commit a checkpoint with both needles on their
   racks, die, restore. No standable surface exists at `y = 96`: walk the seated
   line for 10 s and the player never leaves the hall deck.
8. `wo017_kernel_needle_stays_ungated` — `InitialSpawn::KernelNeedleStation`
   still seats the kernel `KX-NEEDLE` with no `CAP-HOOK5` in the world.
   `WO-012`'s proof line reproduces unchanged.
9. `wo017_car_station_needs_the_car` — stand on the hall deck beside the well and
   command the drum for 10 s. The cage does not move. Step onto it: it does.
10. `wo017_prior_still_pass` — `WO-014`–`WO-016` and all kernel falsifiers `PASS`
    unchanged, the 9 t proof load still on the ground.

### 8.12 Exit state

- both needles **may** be seated at `96 m`, or one, or neither — all three are
  legal entry states for `WO-018`
- with both seated, `MOD-CAGE-1` reaches `118.0 m` and stops on travel
- a seated needle is a walkable `18.40 m` span at `96.25 m`
- `CAP-HOOK5` is wherever the player left it
- `TP-120`, the `120 m` landing, `MOD-EAST-OUTRIGGER` and `SKIN-E` **do not
  exist**; the cage stops in bare shaft and the player steps into void
- the `WO-018` author must answer §8.9's parked-cage case
- everything `WO-014`–`WO-016` built still exists below
- persist is v4
- Fold-device execution remains unverified

---

**Stop. Do not begin the next file inside this one.**
Next file: `03_WORK_ORDERS/WO-018_CAGE_OR_SKIN.md`
