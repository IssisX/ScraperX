# SCRAPERX — AS-006 COUNTERWEIGHT WELL (B02, 154 → 220 m)

**Ascent Slice:** `AS-006`
**Lifecycle:** `PLANNED` — contract exists, no corresponding source
**Provenance:** re-derived here, against `77a4364`, under
`03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md`. Replaces the plan imported from `ScraperX-Grok`
(their `WO-014_CW_PIN`), whose geometry, ids and persist versions describe a world that does not
exist here. The file name is kept so `MANIFEST.txt` stays true.
**Implementation gate:** this write-up. The plan's rules §3 are this slice's test contract.
**Depends on:** `AS-001`–`AS-003` in source and green; the tower stair walkable to 154 m.

## Objective

The first band of the mechanism ascent: from the tower stair's 154 m top deck to the 220 m ring,
the top of Atlas band B02, *Counterweight Well*. Every lift in it runs on a counterweight: a skip,
a derrick boom's own weight, a dumpster filled with rubble. Each is found unlinked, the player
supplies the link and sets it off, and what each leaves behind feeds its neighbours. The band also
has a climbing route that needs no lift.

## Existing truth

Measured from the native world at `77a4364` (bodies probed; `world_solids.inc` is the frame's
source).

| Datum | Value |
|---|---|
| Stair top deck (native `build_stack`, level 14) | top `154.00`; north band `x ∈ [-26, 26]`, `z ∈ [-133, -124]`; east and west bands `x ∈ ±[17, 26]`, `z ∈ [-167, -133]`; the stair arrives on the south band |
| Well guard rail at 154 m | posts at `z = -133` and `z = -167`, `x ∈ {-17, -8.5, 0, 8.5, 17}`, and along `x = ±17`; top rail `155.01–155.09` |
| Frame above 154 m (`world_solids.inc`) | tapering continuation to 418 m: corner columns, diagonal braces on the north and south faces, ring decks 4 m wide at every second level |
| Ring 176 (top `176.25`) | north `z ∈ [-128.91, -124.91]`; south `z ∈ [-175.09, -171.09]`; sides `x ∈ ±[21.09, 25.09]` |
| Ring 198 (top `198.25`) | north `z ∈ [-129.82, -125.82]`; south `z ∈ [-174.18, -170.18]`; sides `x ∈ ±[20.18, 24.18]` |
| Ring 220 (top `220.25`) | north `z ∈ [-130.73, -126.73]`; south `z ∈ [-173.27, -169.27]`; sides `x ∈ ±[19.27, 23.27]` |
| Open well | inside the rings' inner edges; nothing stands in it from 154 m up |
| Reachable ceiling | 154 m walked in CI; 156.1 m upper bound over every body top (`tests/probes/ceiling`) |
| Movement | walk, jump, vault, mantle, ledge hang, crouch, carry, parachute. **No** sprint, climb, shimmy or balance yet (plan §5 Step 2) |
| Rigging | carry exists (`AS-003`); ropes exist only as fixed kernel links (`PulleyConstraint` on the treadle cable). **No** hook, pin, trip line, governor, breakable or flow kit yet |

## Band layout

Plan view, north up (−z is south). Every stage stands on or beside the north side of the well,
where the stair's top deck is 9 m deep and the rings above step in by 0.9 m per 22 m.

```
 z=-124  ───────── 154 north deck (9 m) ─────────
         [ A cage ][ B cage ]        C platform
 z=-131  x -12..-9  x -8.7..-5.7     (198 ring, east)
 z=-133  ── guard rail ── well ──────────────────
         [ A skip ]                  [ C dumpster shaft above A ]
```

| Stage | Archetype | Travel | Payload | Source of energy | Kind |
|---|---|---|---|---|---|
| **A — skip lift** | 01 counter-mass | 154.25 → 176.25 | cage 350 kg + rider | 800 kg skip, 22 m | re-armable |
| **B — derrick boom** | 13, the boom as a swinging counterweight | 176.25 → 198.25 | cage 350 kg + rider | 2 500 kg lattice boom, centre of mass falls 11 m | one-shot |
| **C — debris chute** (finale) | 12 debris-chute counterweight, cascading into A | 198.25 → 220.25 | platform 700 kg + rider | 900 kg of rubble in a dumpster, 44 m on a 2:1 purchase | one-shot source; its leftover re-arms A |

### Links between stages (what each leaves behind)

- **A → B.** A's cage, parked at the top of its travel, is the step into B's cage, which hangs 0.3 m
  east of it at 176 m. From the 176 ring the gap to B's cage is 1.1 m: a jump, legal but not the
  designed way.
- **B → the band.** B's boom, once its guy has gone, hangs vertically from its pivot at 222 m down
  to 200 m: a lattice you can climb from the 198 ring to a landing at the 220 ring. The wreckage
  is a second way up the last section.
- **C → A.** C's dumpster falls 44 m and stops just above A's parked cage. Its bottom gate trips on
  the stop and the rubble drops into A's cage. A's cage then outweighs A's skip, sinks to 154 m,
  and hauls the skip back up into its catch. The finale's leftover re-arms the band's first lift.
- **The cascade.** One pull on C's chute gate: rubble fills the dumpster → the dumpster sinks and
  lifts the platform to 220 m → it dumps into A's cage → A's cage sinks → A's skip is re-armed.

## Stage A — skip lift

**Found:** a steel cage on the 154 north deck at `x ∈ [-12.0, -9.0]`, `z ∈ [-132.8, -130.0]`, on
vertical guides. In the well beside it, an 800 kg skip of steel billets hangs 22 m up, held in a
catch. Its rope runs over head sheaves above the cage and down to a shackle hanging beside the
cage's lifting eye. The rope is taut on the catch; nothing moves.

**Missing link:** the shackle is not on the cage. The player takes it (carry) and hooks it onto the
cage's eye (Action).

**Trigger:** a trip line from the skip's catch lever hangs to a handle on the deck. The player grabs
the handle and pulls; the lever turns and the catch releases once the pin has left its hole.

**Ride:** the skip falls 22 m; the cage rises 22 m. Net force with a rider is
`(800 − 435) × 9.81 = 3 581 N`. A speed governor on the cage guide (a motor that can only brake,
never push) holds the cage to 2.5 m/s; a sprung buffer stops it with the floor flush at 176.25.

**Energy:** the skip releases `800 × 9.81 × 22 = 172.7 kJ`; the cage and rider gain
`435 × 9.81 × 22 = 93.9 kJ`. The governor turns the rest into heat.

**Other links:** the shackle on the cage lifts the cage. On the deck's ring bolt it holds the skip
up and nothing moves. On a load heavier than 800 kg nothing lifts.

**Re-arming (physics decides: the skip survives).** Put 22 m of skip height back: load A's parked
cage until it outweighs the skip. C's rubble does this (above). The cage sinks under the same
governor, the skip rises into its catch and latches (a spring catch that closes when the skip is
seated and slow), and the player tips the rubble out of the cage bed at 154 m.

## Stage B — derrick boom

**Found:** a construction derrick on the 198 north ring. Its 22 m lattice boom (2 500 kg) is
pivoted at 222 m and held horizontal over the well by a rusted guy to the mast head. The boom's
hoist line runs from a two-part block on the boom's tip over a fixed block above B's cage
(`≈ 209.5 m`) and down to a shackle beside B's cage, which rests on its bottom stop at 176 m,
0.3 m east of A's cage top position.

**Missing link:** hook the hoist line's shackle onto B's cage.

**Trigger:** the guy's turnbuckle pin, pulled by a trip line from inside B's cage.

**Ride:** the boom swings down. Its weight's moment, `2 500 × 9.81 × 11 × cos φ`, exceeds the line's
demand at every angle until the cage reaches its top: a statics sweep over block positions finds
the cage's 22 m reached at `φ ≈ 48°` with a least margin of ~114 kN·m (two-part purchase). The cage
governor holds 3 m/s.

**One-shot (physics decides: the source is destroyed).** At the top buffer the line's tension rises
past its rating and it parts. The boom swings on to hang vertically against the mast. The guy and
the line are both gone, so nothing puts the boom back. B's cage stays at 198 on its safety dogs
(a spring catch on the guide). The hanging boom is the climbable wreckage (above).

**Energy:** the boom's centre of mass falls 11 m: `2 500 × 9.81 × 11 = 269.8 kJ`; the cage and rider
gain 93.9 kJ.

## Stage C — debris chute (finale cascade)

**Found:** a platform on the 198 north ring's east half, on guides to 220 m, hung by a 2:1 rope
over a sheave at ≈ 224 m from a steel dumpster (250 kg empty) in a shaft directly above A's cage.
A rubble hopper on the 220 ring holds 900 kg; its chute mouth is over the dumpster, jammed shut by
a length of rebar.

**Missing link:** the dumpster's bail is off the rope's hook: hook it.

**Trigger:** pull the rebar (trip line from the platform).

**Ride:** rubble flows into the dumpster at 150 kg/s (a declared simplified granular model: mass
moves from the hopper's inventory to the dumpster body; the stream is drawn). Empty, the dumpster
cannot lift the platform: on the 2:1 purchase it must exceed `(700 + 85) / 2 = 392.5 kg`. Filled,
it sinks 44 m while the platform rises 22 m under a 3 m/s governor, and the platform's dogs catch
it at 220.25.

**Energy:** rubble and dumpster fall 44 m: up to `1 150 × 9.81 × 44 = 496 kJ`; platform and rider gain
`785 × 9.81 × 22 = 169.4 kJ`.

**Cascade into A:** described under Links. C's source (the hopper's rubble) is spent: one-shot. Its
wreckage is the emptied dumpster hanging above A's cage, climbable.

## Climbing route, no lift

Built from the Step 2 moves on structure of each kind, all on the west half of the well:

| Section | Structure | Moves |
|---|---|---|
| 154 → 176 | a rung ladder at the 154 west band's inner edge, topping out at the 176 ring's edge | climb, mantle |
| 176 → 198 | a vertical standpipe beside the 176 west ring, a shimmy along the 198 ring's edge to a gap in its rail | climb, shimmy, mantle |
| 198 → 220 | a narrow beam across the well's south-west corner to a scaffold lattice panel rising to the 220 ring | balance, climb, mantle |

Sprint and controlled drops are exercised between stages on the band's decks and rings.

## Kit this slice builds

Native, in a mechanism module owned by the physics world (plan §4):

- **Ropes:** tension-only `PulleyConstraint`s with a fixed length, retargetable ends, and a ratio
  for purchases. An end is either on a body's anchor or on a loose shackle body the player can
  carry.
- **Anchors:** named points on bodies (eyes, lugs, bails, ring bolts) with a hook radius.
- **Guides:** `SliderConstraint`s with sprung end buffers, a two-way speed governor (a velocity
  motor whose force limits are set each tick so it only ever opposes motion), and spring catches
  that latch a seated, slow body.
- **Catches and pins:** a constraint that exists while its pin is seated; a pin lever on a hinge that
  a trip line turns.
- **Trip lines and handles:** a rope from a handle body to a lever; the player pulls a handle with a
  force-limited grip (`≤ 900 N`).
- **Breakables:** a constraint removed when its solver impulse exceeds its rating over a step.
- **Granular flow:** mass moved between bodies at a rate while a gate is open and the receiver is
  under the stream.
- **Energy ledger:** per stage, the source's potential-energy loss and the payload's gain, from
  body states every step.
- **Generic presentation:** every mechanism body and rope is declared by the native side (shape,
  size, material class) and drawn by Godot from that declaration. Nothing is authored twice.

Player verbs, all on the contextual Action (touch Action button, pad X, keyboard E): take a shackle
(existing pick-up), **HOOK** it onto an anchor, **UNHOOK** a slack one, **GRAB** a handle and pull,
**LET GO**. The HUD names the verb.

## Falsifiers

Groups in `tests/simulation_tests.cpp`, each printing a `PASS scraperx_sim AS-006 …` line that CI
gates:

1. **A no link, no lift** — pull A's trip line with the shackle unhooked: the skip falls, the cage
   stays within 0.05 m of 154.25.
2. **A ride** — hook, pull, ride from the deck: the rider stays grounded on the cage all the way;
   the cage stops with its floor at 176.25 ± 0.05; its speed never exceeds 2.6 m/s; the rider's
   and cage's energy gain is below the skip's release.
3. **B no link, no lift**, and **B ride** — step from A's cage into B's cage, hook, pull: the cage
   reaches 198.25; the line parts; the boom hangs within 5° of vertical; the cage is held at 198 by
   its dogs; energy as in 2.
4. **C and the cascade** — hook, pull the rebar: the platform reaches 220.25 and is held; the rubble
   ends in A's cage; A's cage sinks to 154.25; A's skip is latched at its catch; with the rubble
   tipped out, A lifts a rider to 176.25 again.
5. **Climbing route** — from the 154 deck to the 220 ring with no lift touched.
6. **Band, one run** — from the 154 deck to the 220 ring through A, B and C, driven only by player
   inputs.
7. **Checkpoint** — a lethal fall after A fires restores the rope, the catch and every stage body to
   the committed state.
8. **Prior** — every existing group, the Fold capture and the uitests unchanged.

## Out of scope

Bands above 220 m (`AS-007`–`AS-009`). Fluids (B03). The Fold device.

## Exit state

The player stands on the 220 ring, arrived by A → B → C, by the climbing route, or by B's wreckage.
A may be re-armed; B and C are spent. `AS-007` (B03, 220 → 340) begins from the 220 ring.

## Result record

pending.
