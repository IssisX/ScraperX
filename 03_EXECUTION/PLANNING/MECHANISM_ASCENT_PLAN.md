# SCRAPERX — MECHANISM ASCENT PLAN

**Withdrawn.** This plan, and the band contracts that follow it, did not produce lifts a person could read. Small links, hidden ratios, and parts you cannot see doing the work. Do not build the next stage from this file.

The build is [the apron chain](COLOSSUS_APRON.md): pipes, a scale, a trapdoor, a wrecking ball, slabs, and a seesaw that lifts you. Ground level. About 12 m. Big parts only.

The rules below are kept only as a record of what was tried. They are not an instruction to keep going.

---

# SCRAPERX — MECHANISM ASCENT PLAN (withdrawn record)

**Status:** withdrawn record. Not the build. See `COLOSSUS_APRON.md`.
**Provenance:** locked with the owner on 2026-09-24 in a four-decision design pass over
`Mechanism-Ideas-and-archetypes.md` (repository root; the owner's 20 lift archetypes).
**Supersedes:** `AS-004` (needle seat) as the next job. `AS-004` stays `PLANNED` for a later
pass; its Revalidation section records why.

## 1. What the climb is

The player climbs a 1 600 m working tower that is still being built upward. Its lift machinery
is found broken or unlinked. The player completes each chain with what is lying around, rides
the consequence upward, and climbs everything in between.

Every lift is one archetype chain from the owner's document:

```
stored energy above  ->  a link the player supplies  ->  a trigger  ->  a linkage
(mass, water, gravel,    (hook, pin, chock, wedge,       (pull, shove,   (pulley, lever,
 spring, vehicle,         cable, counterweight,           strike, cut)    gears, piston,
 a structure to fall)     brake)                                          buoyancy, capstan)
                    ->  the player rides the payload  ->  an arrest at the next level
```

One lift gains roughly 10–60 m, so the 1 450 m above the stair takes dozens of them, mutated and
recombined from the 20 archetypes: keep each causal chain, change the objects, sizes, ratios
and layout. The fuel for every lift is stored above it, because energy only flows downhill.

## 2. Locked decisions

| # | Decision | What it commits |
|---|---|---|
| 1 | **Complete the chain, then ride** | Mechanisms are found broken or unlinked; the player supplies the missing link with objects in the world, and how it is linked changes the outcome (load → speed and force, anchor → destination). A general rigging kit works on any anchor in the building, so player-invented links work too |
| 2 | **Linked stages** | Each stage is its own lift with a declared input (its fuel) and output (what it leaves). Neighbours are placed so outputs matter: a fallen mass sets off or becomes structure for the next stage, drained water fills another shaft, a swung boom becomes a bridge. Each band ends in one larger cascade |
| 3 | **Physics decides re-arming** | A lift is re-armable when its energy source survives and energy can physically be put back (counterweights, hoppers, spools, re-dammed water); re-arming usually borrows energy from another lift. A lift that destroys its source (collapse, burst, snapped wire, breached wall) is one-shot and leaves climbable wreckage |
| 4 | **First band above the stair** | From the 154 m top deck to 220 m, the top of Atlas band B02. The first height the game has never had |

Standing, by the owner's direction (*"if a route genuinely works in the world, it works in the
game"*): the tower stair stays. No working route is removed to make a mechanism necessary.

## 3. Rules every stage obeys

These are the test contract. Each stage's falsifiers assert all of them.

1. **No link, no lift.** Until the player supplies the missing link, the stored energy stays
   stored. A trigger on an unlinked stage moves no payload.
2. **Energy pays for height.** The payload's gain in potential energy never exceeds the energy
   its source released, measured from body states on every run. No hidden motor.
3. **Every link is physical.** Contact, constraint or force. No script, flag, trigger volume or
   teleport between links. Any simplified model (water, air, gravel) is declared in the stage
   record and couples to real bodies.
4. **Leftovers are real.** What a stage leaves stays in the world and feeds its neighbour as
   designed; a player may link neighbours differently.
5. **Rides are moving supports.** The player inherits the payload's motion; an arrest can
   launch them, and that is physics, not a failure.
6. **Nothing strands.** Every band keeps a climbing route that needs no lift; one-shot wreckage
   is climbable; from any committed state the band can be climbed or re-armed. Dying restores the
   last commit, machines included (GDD §9, Law 9).

## 4. What the archetypes are built from

Every primitive below exists in the vendored Jolt. Pulleys already run in the game (the treadle
cable); the rest are unused so far.

| Kit | Engine primitive | Archetypes |
|---|---|---|
| Cables, pulleys, snatch blocks, block and tackle (tension only, slack allowed) | `PulleyConstraint`, `DistanceConstraint` | 01, 09, 11, 12, 13, 14, 15, 17, 20 |
| Rigging: hook and unhook cable ends, insert and pull pins, chocks and wedges; drag and shove heavy loads | constraints created and removed at runtime, one owner, restored by the checkpoint | all |
| Breakable parts: bolts shear, wires snap, straps part, walls breach at a load | constraint and contact impulse read each step | 01, 03, 05, 07, 08, 13, 15, 20 |
| Levers, seesaws, pendulums, swinging structure | `HingeConstraint` | 03, 06, 08, 13, 18, 19, 20 |
| Gears, spools, capstans, flywheels, rack and pinion | `GearConstraint`, `RackAndPinionConstraint` | 04, 06, 07, 16, 19 |
| Springs and ratchets | `SpringSettings`; hinge limits that follow the pawl | 03, 07, 10 |
| Rolling masses, carts, vehicles, treads, rails | rigid bodies, wheeled and tracked vehicle controllers, `PathConstraint` | 04, 06, 10, 11, 14, 15, 16, 18, 19 |
| Water, air and oil (declared simplified models) | buoyancy impulse on real bodies; volume and pressure driving pistons | 02, 05, 08, 20 |
| Gravel and debris flow (declared simplified model) | mass flowing into real bodies; stream impulse on wheel blades | 09, 12, 16 |
| Structural collapse | many rigid bodies with breakable joints | 03, 08, 15 |

## 5. Build order

| Step | Work | Proves |
|---|---|---|
| 0 | This plan; the archetype document on the write branch; the ledger pointed here | the direction survives a new session |
| 1 | Re-author `AS-006` as the band's slice (§6). Then the rigging kit, and the band's first stage: a counter-mass lift (Archetype 01) off the 154 m deck that the player completes with a hook and a release pin | rules 1, 2 and 5 on real bodies; the first metre above the stair's reach |
| 2 | Movement: sprint; climbing on ladders, girders, pipes and lattice; shimmy; balance; controlled drop; momentum on and off moving machinery; touch, pad and keyboard; first-person hands | the parkour between and around stages |
| 3 | The power-train, flow and breakable kits as the band's stages need them; the band's other stages from different families, including one one-shot stage with climbable wreckage, one re-armable stage, and one set off by a neighbour's leftovers; the finale cascade to 220 m; the band's climbing route | rules 3, 4 and 6 |
| 4 | Band test: 154 m to 220 m by the linked stages and, separately, by the climbing route; a re-arm test; CI, Fold capture and Android green | the band as the pattern for every band above |

After the first band: re-derive the Atlas above 220 m as linked-stage bands and build them
upward, band by band, to the 1 600 m summit predicate (Atlas B11).

## 6. First band sketch — 154 m to 220 m

A sketch, not a spec; stage choices, masses and heights are tuned while building. It is
re-authored as `AS-006` (Atlas B02) before code.

- **Stage A — counter-mass lift (01), re-armable.** A cage on the 154 m deck under a sheave on
  the frame above; a loaded skip stored higher up. The player hooks the cable to the cage and
  pulls the release pin; the skip falls, the cage rises. The skip survives, so it can be winched
  back up with energy borrowed from another stage.
- **Stage B — one-shot with climbable wreckage.** For example a crane-jib pendulum (13): an
  overloaded guy wire snaps, the swinging boom yanks a cage upward, and the boom comes to rest
  against its mast as a climbable ramp.
- **Stage C — set off by a neighbour's leftovers.** For example Stage A's falling skip lands on a
  cantilever beam (18) or strikes a capstan (19) and powers another lift.
- **Finale — a cascade of two families to 220 m.** For example a debris-chute counterweight (12)
  set off by Stage B's wreckage.
- **Climbing route.** The band's own structure, climbable end to end with the Step 2 moves.

## 7. Consequences elsewhere

- `AS-004` (needles) and `AS-005` (cage landing) are not the next jobs: the stair already reaches
  their heights. Their machine routes may return later as optional stages.
- `AS-006` (B02) is the next authoring job: re-authored against this tree as the first band.
- `AS-007` onward are re-derived as linked-stage bands when their heights come up.
- The Atlas keeps its band heights, datum and the 1 600 m summit predicate. The modules inside
  each band are re-authored as linked stages band by band.
- Performance on the Fold: bands out of range sleep; water, air and gravel use the declared
  simplified models, not particles; each band's body budget is measured before it is accepted.

## 8. Step 2, the movement, as specified before it is built

Every move is native (the simulation decides; Godot shows and asks), works on any structure of
its kind by what the structure is, not by a tag, and is driven like the existing traversals: the
body is steered toward a point held in the frame of the body it is on, so a move on moving
machinery rides with it and letting go keeps that body's velocity.

| Move | What it works on | Rules |
|---|---|---|
| **Sprint** | any walkable ground | held (Shift, left-stick click, the touch stick pushed past its ring); standing, hands free, not balancing, stick at least 0.7 and within 45° of the facing: top speed 8.0 m/s against 5.5. A jump keeps its takeoff speed in the air while the stick holds forward: air control steers and slows, it never drags a running jump back to walking speed |
| **Climb** | any member a hand can close round: its two thinner dimensions at most 0.18 m, its length at least 0.25 m (rungs, pipes, bars, lattice members, chords), static or moving | facing one within 0.8 m at hand height, Action (CLIMB) or moving into it in the air takes hold. The stick toward the structure climbs up (0.9 m/s), away climbs down (1.2 m/s), sideways moves across (0.6 m/s), each only while the hands find the next hold; the hands go hand over hand. At the top, climbing on with a ledge in reach mantles over; at the bottom the feet step off onto ground. Jump springs back off; Drop lets go |
| **Shimmy** | any ledge a hang holds | hanging, the stick sideways moves along the ledge (0.6 m/s) while the ledge continues under the hands, re-probed every step; hand over hand |
| **Balance** | any support narrower than 0.5 m and at least 1.5 m long (a beam, a pipe run, a rail) | walking on it: top speed 2.0 m/s along it; sideways input under 0.8 is held on its line, over 0.8 steps off; arms out |
| **Controlled drop** | any edge with a drop behind it | at an edge, Drop lowers the body over it into a hang facing it (0.6 s); from a hang, Drop lets go; holding toward the wall while falling catches the next ledge below (the existing catch) |

Falsifiers, one native group per move, each with its negative: no hold on a wide wall; no climb
past the last hold; no sprint crouched or carrying; no balance on a wide deck; no drop-hang where
nothing lies beyond the edge. And the band's no-lift route, 154 m to 220 m. Godot: every move on
touch, pad and keyboard; the hands hold the native's hold points and travel between them at their
reach speed; the camera never snaps.
