# SCRAPERX — MECHANISM ASCENT PLAN

**Status:** the current plan for the whole ascent, from grade.
**Provenance:** re-planned with the owner's direction of 2026-09-25: *"start from the ground level,
and do all the mechanics and mechanisms all over again ... maybe a couple of mechanisms can be
stacked, and then a climb, and then another mechanism, and then a climb"*, and the goal set with
it: stacked, working machines and deliberately placed industrial obstacles, climbed and ridden
from the ground to about 300 m. Built one machine per work cycle with the mechanism-chain-forge
method (backward from the effect a stage must produce; contract; causal proof).
**Supersedes:** this plan's 2026-09-24 version (the ascent above the tower stair). The owner's
20 lift archetypes (`Mechanism-Ideas-and-archetypes.md`) remain the source of every machine.

## 1. What changed, and why

The owner played the build and found every mechanism incomplete, the ramps no fun to climb,
test cubes all over the yard, and machines that did not work. So, from 2026-09-25:

| Before | Now |
|---|---|
| A stair of 14 ramp flights walked the player to 154 m; floating block steps and swing flights climbed the south face | No ramp or stair is a route. A level is gained by a machine or a designed climb |
| Movement test blocks, three kernel test rigs, a steam plant and three half-mechanisms stood in the player's world | The game world holds only the building, its machines and its climbs. Movement test blocks exist only in a world started at one of their test spawns |
| Parked lift cages, static gears, jib cranes with hanging crates stood on the tower as dressing | No machine is dressing: every machine in the world works |
| The first stage stood at 154 m | The first stage stands at grade |

## 2. Rules every stage obeys

1. **No link, no lift.** Stored energy stays stored until the player completes the chain.
2. **Energy pays for height.** A payload never gains more potential energy than its source
   released; checked from body states.
3. **Every link is physical** — contact, constraint or force; no script, flag or teleport between
   links. A simplified model (water, air, gravel) is declared in the stage record.
4. **Rides are moving supports.** The rider inherits the payload's motion.
5. **Nothing strands.** Dying restores the last commit, machines included.
6. **Built to be read.** Every part is held by visible structure (no floating rails, sheaves or
   blocks); what the player must handle is marked (hazard or yellow); the stored energy, the
   missing link and the destination are visible from where the stage is found.
7. **Climbs are designed, not free.** A climb is a sequence of different moves (mantle,
   jump-and-hang, climb, shimmy, balance, drop) through industrial objects that are there for a
   reason; no staircase of blocks. A climb never lets the player bypass the machine below it.

## 3. The chain, grade to TP-340

Band 0, **The Stack** (grade → 154 m), is new. Above it the existing bands carry on.

| # | Kind | Heights | Where | What the player does | Status |
|---|---|---|---|---|---|
| S1 | Machine: **water-balance hoist** (archetype 01, counter-mass; water as the mass) | 0 → 22 m | south face, east of centre | holds the fill chain until the empty bucket at the head is full from the header tank, walks into the cage and pulls the trip cord; the bucket falls 21.8 m and the cage rides up to a gangway onto deck 2. At the foot the bucket drains on a striker and the cage comes back down by itself | built and proven (cycle 1) |
| C1 | Climb: **the facade** | 22 → 44 m | south face, east of S1 | out onto a loading landing, a mantle onto its switchgear cabinet, a jump to hang from the duct along the face, up onto the duct and along it, up a vent stack over deck 3's edge; out along deck 3's monorail under the ladder hung from deck 4's davit, a turn and a leap for it, up it onto the davit's arm and back along the arm onto deck 4 | built and proven (cycle 2) |
| S2 | Machine: **ingot bucket** (archetype 01, player-supplied counter-mass) | 44 → 66 m | the shaft, south-east | the platform's counterweight bucket is empty; the player carries steel ingots off a pallet into it until it outweighs platform and rider, boards and pulls the trip | planned |
| C2 | Climb: **the east hall** | 66 → 88 m | east side | the machine hall's roof and window sills, a pipe rack and a jump to deck 8 | planned |
| S3 | Machine: **brake override** (archetype 17) | 88 → 132 m | the shaft, north | a freight car overloaded with pallets is held by a crowbar jammed in its brake; the player rides the counterweight and pulls the crowbar's cord; the car falls, the counterweight rises 44 m | planned |
| C3 | Climb: **the crown** | 132 → 154 m | shaft and north band | beams, a hang-and-shimmy along deck 13's edge, a pipe to deck 14 | planned |
| — | AS-006 Counterweight Well | 154 → 220 m | shaft | skip lift, derrick boom, debris chute; climbing route | built; to be reviewed against rule 6 |
| — | AS-007 Wet Isolation | 220 → 340 m | shaft | water, air and hydraulics; climbing route | built; to be reviewed against rule 6 |

The route above 340 m (AS-008, AS-009) stays as built until the ascent reaches it.

## 4. Cycle 1 — S1, the water-balance hoist (contract, as built)

| Contract | Content |
|---|---|
| **Purpose and boundary** | Carry a rider from grade to deck 2 (22.0 m) on the tower's south face. Includes the cage, its guide and governor, the bucket and its guide, the rope, the catch with its lever and trip cord, the header tank with its valve, lever and fill chain, the striker and the bucket's drain, the headframe, the bucket's fence and deck 2's gangway (`src/sim/band_stack.cpp`). Excludes C1 and everything above deck 2 |
| **Output** | The rider standing on deck 2's south band (support: the tower), at rest, within 25 s of pulling the cord |
| **Receiver** | Deck 2, south band, top 22.00 m, reached over a fixed gangway from the cage's open north side. Acceptance: grounded on the tower with the body's centre above 22.5 m and north of z = −124.0 |
| **State and rules** | Cage 300 kg on a vertical guide, governor 2.5 m/s brake-only (12 kN), easing into each stop. Bucket 150 kg empty, a bin of up to 1000 kg of water, on its own guide, held at the top by a relatching catch. One rope over two head sheaves (1:1), tension only, exactly long enough for the cage down and the bucket up. Header tank 12 t (a declared pool); its downpipe's valve opens with its lever (Q = 0.6 A f √(2 g Δh), A 0.03 m², ≈ 165 kg/s full open) and shuts by the lever's own weight when the chain is let go. At the foot of its guide the bucket lands on a striker that opens its drain (80 kg/s) |
| **Input** | The rider's verbs only: GRAB the fill chain and hold it back (the valve opens; water runs into the bucket), LET GO; GRAB the trip handle and step back (the cord turns the catch lever past its release), LET GO |
| **Capacity** | Full: net (1150 − 300 − 85) g ≈ 7.5 kN, braked by the governor to ≤ 2.5 m/s; 21.8 m in about 9.4 s. The rider needs more than 235 kg of water; the empty cage goes up on more than 150 kg |
| **Use and recovery** | Found from the approach: the headframe with its tank is the tallest thing at the tower's foot; both handles hang yellow from a gantry at the cage's mouth. Nothing strands: at the foot the bucket drains, and once lighter than the cage it rises, the cage comes down, and the catch seats the bucket at the top, as found (the rider has about 10 s at the top to step off, or rides back down). A lethal fall restores the committed state, machine included |
| **Proof** (native, `run_stack` in `tests/simulation_tests.cpp`) | (a) no link, no lift: tripped empty → cage travel ≤ 0.05 m, the catch seats again; (b) from the game's spawn on player inputs: fill to 900 kg, trip from inside, cage floor 22.04 m, peak 2.53 m/s, rider on the cage throughout, the payload's energy gain never above the bucket's release (margin +22 J); (d) the rider walks off over the gangway onto deck 2 (support: the tower); recovery: the bucket drains, the cage returns, the catch relatches; (c) intervention: filled and tripped from the yard with nobody aboard → the cage goes up empty, comes back by itself, and is filled and ridden again |

**Decision record.** The first build of S1 was a skip of scrap with the rope's end found on a
bollard (the AS-006 A pattern). Two moves spent it for good: a trip pulled from the yard sends
the cage up empty, and a trip with the rope's end dropped loose lets the skip drag it up to the
head. Either strands the ascent at its first stage (rule 5). The water balance keeps the same
counter-mass principle but returns itself: the counter-mass is let go at the bottom. The kit
already carried every part of it (pools, pipes, water bins, strikers: AS-007 E).

**Defects found and repaired in this cycle.**

| Defect | Invalid state | Transition that allowed it | Change that makes it unreachable |
|---|---|---|---|
| The stack's face braces were ramps | a 0.45 m brace at 23° is walkable (support normal 0.92 ≥ 0.55): deck to deck, grade to 154 m, round every machine | one brace per storey, 11 m over 26 m | a two-storey diagrid, 22 m over 13 m: 59°, normal 0.51, not a support (`main.gd`, regenerated `world_solids.inc`) |
| The yard's kerb held the body on it | a body on the 0.5 m kerb could not step down off it with a gentle stick | `beam_underfoot` called every narrow long box a beam | a beam must stand over a fall: ground within a step beside it makes it a floor (`simulation.cpp`); regression test `kerb` |
| The headframe's own brace blocked S1's exit | the east bay's top diagonal crossed the cage's north side at 22.8 m and ran through the gangway | a brace in every bay | the cage's bay is a portal in its top storey, with knee braces above the doorway |
| The catch lever lay through a brace | the lever's arm overlapped the west-face brace, which pushed it past its release on the first step | the pivot placed without a clearance check | the pivot moved south of that brace |

**Status.** Specified and implemented; verified natively on player inputs from the game's spawn
through the real receiver (deck 2) and the machine's return. Screenshot pass: the headframe,
tank, fence, handles, cage and gangway are held by structure and read from the yard.
Not yet verified on a device (the next APK).

## 5. Cycle 2 — C1, the facade (contract, as built)

| Contract | Content |
|---|---|
| **Purpose and boundary** | Carry a climber from deck 2's south band (S1's receiver) to deck 4 (44.0 m) with the movement verbs, on the south face east of S1 (`build_c1` in `src/sim/band_stack.cpp`, one static body). Excludes S2 |
| **Output** | The climber standing on deck 4's south band (support: the tower) |
| **Receiver** | Deck 4, south band, top 44.00 m. Acceptance: grounded on the tower with the body's centre above 44.5 m |
| **State and rules** | A sequence of distinct moves through objects with a reason to be there: a loading landing on knee braces off deck 2's edge beam (walk out over the drop); a switchgear cabinet 1.7 m tall on it (mantle); a duct 1.5 m deep along the face, hung on straps from deck 3's edge beam, its lip 3.6 m over the cabinet and 0.4 m clear of it (jump and hang at the top of the jump; climb up); the duct's top, 0.8 m wide over the drop (walk); a vent stack from the duct to 1 m above deck 3 (climb; the top-out is over deck 3, not its edge beam); deck 3's monorail, 0.3 m wide and 4.4 m past the face (balance); the ladder hung from deck 4's davit, its bottom rung 2.3 m over the monorail and out of reach standing (a leap, facing back to the building); the ladder's stiles and rungs stop at the davit's arm, so a climber tops out over them onto the arm; the arm, 0.35 m wide (balance back to the deck) |
| **Input** | Walk, mantle (Action), jump, hang, climb up (jump from the hang), take hold (Action), climb, balance |
| **Capacity** | Every rise inside the body's envelope: mantle 1.70 (≤ 1.85), hang 3.60 over the take-off (≤ 3.79 at the top of a jump), ladder rung 2.3 over the monorail (in reach only at the top of a jump). Braces and columns clear of every body pose on the route (the diagrid's braces cross deck 3's edge at x ±6.5 and ±19.5; the route tops out at x 24.0 and 12.5) |
| **Use and recovery** | Seen from S1's gangway: the landing, cabinet and duct east along the face, the yellow monorail and the davit's ladder above. A fall from the route is lethal and restores the last footing on it or on the deck below it; nothing on the route is a dead end. Nothing of it reaches below deck 2, so its only foot is S1 |
| **Proof** (native, `run_stack`) | From deck 2 on player inputs to deck 4 (31.7 s); standing at the monorail's end the ladder is out of reach (the leap is required); the Stack in one run from the game's spawn: fill and ride S1, climb C1, deck 4 in 69.6 s |

**Changed on the way.** The verdigris main (dressing, 0.8 m, the face's full height) ran
through the landing and the duct; it now climbs the west half of the south face. A ladder's
rungs above a top-out point stopped the mantle over them (it stalled and aborted, and the
climber carried on up the ladder): the davit's ladder ends at the arm, its grab handles stand
wider than a body.

**Open ends.** S2 (deck 4 → 66 m) is not built: its acceptance is "from deck 4's south band,
reach 66 m (deck 6) by a machine the player completes". It is the missing cause closest to the
goal, downstream of C1. Status: next cycle (cycle 3).
