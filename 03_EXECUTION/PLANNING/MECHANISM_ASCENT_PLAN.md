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
| S1 | Machine: **water-balance hoist** (archetype 01, counter-mass; water as the mass) | 0 → 22 m | south face, east of centre | walks into the cage and takes hold of the valve chain hanging in it: held, it pulls the lever overhead down, which opens the header tank's valve and lets the bucket's catch go; water runs into the empty bucket at the head until it outweighs cage and rider, the bucket falls 21.8 m and the cage rides up to a gangway onto deck 2. At the foot the bucket drains on a striker and the cage comes back down by itself | built and proven (cycle 1; control repaired, cycle 1b) |
| C1 | Climb: **the facade** | 22 → 44 m | south face, east of S1 | out onto a loading landing, a mantle onto its switchgear cabinet, a jump to hang from the duct along the face, up onto the duct and along it, up a vent stack over deck 3's edge; out along deck 3's monorail under the ladder hung from deck 4's davit, a turn and a leap for it, up it onto the davit's arm and back along the arm onto deck 4 | built and proven (cycle 2) |
| S2 | Machine: **rolling-ballast walking beam hoist** | 44 → 66 m | the shaft, south | walking beam on central trunnion, rolling ballast car on west arm, 350 kg cage on east arm, chock lever, catch, trip line and chain handle; rider boards cage at Deck 4, pulls trip handle, 22 m governed ride to Deck 6 | built and proven (cycle 3) |
| C2 | Climb: **the east machinery hall & pipe rack** | 66 → 88 m | east side | switchgear enclosure mantle, exhaust manifold duct leap and mantle, wall ladder to Deck 7 equipment hatch, pipe rack girder mantle, 0.35 m monorail balance beam, davit hanging ladder leap, crossover bridge over Deck 8 ShaftRail onto Deck 8 North band | built and proven (cycle 4) |
| S3 | Machine: **brake-override counterweight hoist** (archetype 17) | 88 → 132 m | the shaft, north | overloaded 3,500 kg freight car at Deck 12 held by caliper brake; rider boards 400 kg counterweight carriage at Deck 8, trips lanyard brake handle, 44 m governed ride (≤ 2.6 m/s) to Deck 12 North band | built and proven (cycle 5) |
| C3 | Climb: **the crown trusses & high riser ladder** | 132 → 154 m | shaft and north/south band | stepped incline girder over Deck 12 ShaftRail, atrium ventilation duct mantle (137.2 m), Deck 13 wall ladder (143.0 m), Deck 13 walkway plate to X = -6.0 m, high riser ladder (11 m rungs) clearing Stage A cage, crossover bridge over Deck 14 ShaftRail, steps down to Deck 14 South perimeter runway, walk to Deck 14 landing (InitialSpawn::Deck154) | built and proven (cycle 6) |
| Stack | Ascent: **The Stack continuous ascent** | 0 → 154 m | south & north shaft / face | unbroken continuous physics simulation: S1 → C1 → S2 → C2 → S3 → C3 in one single run without dying, worst tick displacement ≤ 0.15 m | built and proven (166.4 s) |
| — | AS-006 Counterweight Well | 154 → 220 m | shaft | skip lift, derrick boom, debris chute; non-lift climbing spine | built; cycle 7/8 integration target |
| — | AS-007 Wet Isolation | 220 → 340 m | shaft | water, air and hydraulics; non-lift climbing route | built; cycle 10 integration target |

The route above 340 m (AS-008, AS-009) stays as built until the ascent reaches it.

## 4. Cycle 1 — S1, the water-balance hoist (contract, as built)

| Contract | Content |
|---|---|
| **Purpose and boundary** | Carry a rider from grade to deck 2 (22.0 m) on the tower's south face. Includes the cage, its guide and governor, the bucket and its guide, the rope, the header tank and its valve, the valve's lever and its chain, the catch on the same lever, the striker and the bucket's drain, the headframe, the bucket's fence and deck 2's gangway (`src/sim/band_stack.cpp`). Excludes C1 and everything above deck 2 |
| **Output** | The rider standing on deck 2's south band (support: the tower), at rest, within 25 s of taking hold of the chain |
| **Receiver** | Deck 2, south band, top 22.00 m, reached over a fixed gangway from the cage's open north side. Acceptance: grounded on the tower with the body's centre above 22.5 m and north of z = −124.0 |
| **State and rules** | Cage 300 kg on a vertical guide, governor 2.5 m/s brake-only (12 kN), easing into each stop. Bucket 150 kg empty, a bin of up to 1000 kg of water, on its own guide, held at the top by a relatching catch. One rope over two head sheaves (1:1), tension only, exactly long enough for the cage down and the bucket up. Header tank 12 t (a declared pool); its downpipe's valve and the bucket's catch both answer one lever, which turns on the valve's own spindle over the bucket's bay: its 2.4 m arm reaches east over the headframe's middle legs and the cage, and a counterweight 0.6 m west of the spindle holds it up on its stop (150 N·m; the chain's 3 kg handle takes 71 of it, so ≈ 33 N more at the chain turns it). The catch lets go past 0.15 rad; the valve opens from 0.06 rad to full at 0.24 rad (Q = 0.6 A f √(2 g Δh), A 0.03 m², ≈ 165 kg/s full open). The chain runs from the arm's end down past a guide 1.3 m under it to a yellow handle hanging in the cage, its top 2.2 m over grade and 0.15 m over a rider's head: 0.97 m of pull takes the lever to its dead point (0.50 rad), and the 0.7 m the hands take it down turns it past full open. At the foot of its guide the bucket lands on a striker that opens its drain (25 kg/s) |
| **Input** | The rider's verbs only: GRAB the valve chain (hanging above the hands, it comes down to them) and hold it; LET GO |
| **Capacity** | The rider needs more than 235 kg of water: the cage leaves the yard at 250 kg (1.5 s of holding). Rising, the rider's hands rise with it and the chain goes slack: the valve shuts within the cage's first 0.56 m. Net (≈ 537 − 385) g ≈ 1.5 kN, braked to ≤ 2.5 m/s: 11.9 s from GRAB to the top. Drained below 235 kg (≈ 6 s after the top) the cage brings a rider who stays aboard back down; empty, it starts down below 150 kg. A ride takes ≈ 170–390 kg of the tank's 12 t |
| **Use and recovery** | Found from the approach: the headframe with its tank is the tallest thing at the tower's foot; a long hazard-striped lever runs from the tank's valve out over the cage, its rust counterweight over the bucket, and inside the cage a yellow handle hangs on a chain from its end. Left alone, S1 waits as found. From the cage floor the HUD offers GRAB · VALVE CHAIN; held, LET GO · BUCKET n KG counts the water in. A tug adds water that stays in the bucket, so tugs add up and nothing is lost. The chain is only in reach standing in the cage (0.65 m round it at hand height); pulled away from its line it reaches the lever's dead point and tears out of the hands. Nothing strands: at the foot the bucket drains, and once lighter than the cage it rises, the cage comes down, and the catch seats the bucket at the top, as found. Let go at the top the chain lies on the cage floor and comes down with it; a chain carried off onto deck 2 stays there, which cannot strand the player: every checkpoint above grade restores to the last footing, never below it. A lethal fall restores the committed state, machine included |
| **Proof** (native, `run_stack` in `tests/simulation_tests.cpp`) | left alone 30 s: the lever on its stop, the valve shut, the bucket dry, the cage at grade; (a) a tug: GRAB, 0.3 s, LET GO → 94 kg in the bucket (all of it out of the tank), the cage and bucket move ≤ 0.001 m, the catch seats again, the chain hangs back where it was; held again the tug's water stays and the cage lifts at 269 kg; (b) from the game's spawn on player inputs: GRAB and hold → lift-off at 250 kg (predicted 235 + what accelerates it), valve shut at 0.56 m of travel, no water after, cage floor 22.05 m, peak 2.51 m/s, rider on the cage throughout, the payload's energy gain never above the bucket's release (worst −6.8 J at rest before lift-off, inside the 8.3 J stance tolerance); (d) the rider walks off over the gangway onto deck 2 (support: the tower); recovery: the empty cage starts down at 133 kg (< 150), the catch relatches, the chain hangs back in reach; (c) a rider who stays aboard is brought back down at 222 kg (< 235), takes the chain again and rides up a second time; the tank has water for 60 more rides |
| **Proof** (the game's input path, `ui_test_driver.gd` `touch_stack`, `pad_stack`, `keyboard_stack`) | From the game's own start at grade, events injected through the viewport on each device: walk into the cage, the HUD offers GRAB · VALVE CHAIN, press it, the native lever passes 0.42 rad and the catch opens, the HUD reads LET GO · BUCKET n KG past 120 kg, the cage rises and reaches the top, LET GO, walk off onto deck 2 and climb C1 to deck 4 (y 44.89, 71 s). The hands never jump more than 0.07 m in a frame on the chain; the body never moves more than 0.19 m sideways in a frame; no deaths. CI builds the playtest APK only after these pass |

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

**Repair, cycle 1b: the owner could not operate S1** (MACRO-TRAVERSAL-STRICT).

| Field | Record |
|---|---|
| Intended edges | player → fill chain → valve → water → bucket's mass; player → trip handle → cord → catch lever → catch → bucket falls → rope → cage rises |
| Observed | on the device, the owner took hold of a hanging handle and nothing happened; the ascent ended at the first stage |
| First broken seam | player → control. Two handles, in a fixed order, in two places, both worked by walking backwards while holding; the trip, pulled first, opens the catch on an empty bucket, so the only feedback to the first thing a player tries is nothing |
| Rival causes | R1 the verb (step back while holding) is not discoverable; R2 the order (fill, then trip) is not discoverable; R3 the input path loses the pull on a device; R4 a yard trip sends the cage up empty and away |
| Discriminating evidence | `touch_rig`, the same grab-and-step-back verb through the touch pipeline, passes: R3 is not it. Nothing in the world or HUD states R1 or R2; a native trip-first run moves nothing, by design |
| Failure mechanism | an operation of two remote controls with an unannounced order and an unannounced verb, whose first try is silent |
| Owner | S1's control (`build_s1`), its HUD reading (`main.gd`), and the proof, which had checked the machine and not the operation |
| Repair | one control where the rider stands: the valve and the catch on one lever turning on the valve's spindle, its chain hanging in the cage, operated by the verb every handle already has (GRAB, which pulls a handle hanging above the hands down to them), held while the water runs, with the water counted on the HUD. The gantry, its sheaves, the second lever and both yard handles are gone. The drain slowed from 80 to 25 kg/s so that a rider at the top has time to step off |
| Reclosure | C1 unchanged in its moves; the rest of the band above deck 2 untouched |
| Falsifier and signature | a tug fills < 235 kg and moves nothing (observed 94 kg, ≤ 0.001 m); holding, the cage leaves only past 235 kg (observed 250 kg); rising, the chain slackens and the valve shuts in the first metre (0.56 m); every run through the real input pipeline from the game's start reaches deck 4 on each device |

A first build of the repair hung the lever from the head 1.2 m east of the valve, with nothing
between them to show what the lever did; moved onto the valve's spindle, its arm grew to 2.4 m
and its counterweight with it. Too small a counterweight let the chain's own weight sag the
lever past the catch's release and open the valve: the bucket filled and the empty cage left by
itself. The idle check above holds that shut.

**Status.** Specified and implemented; verified natively on player inputs from the game's spawn
through the real receiver (deck 2) and the machine's return, and through the game's own input
pipeline on touch, pad and keyboard from the game's start to deck 4. Not yet verified on the
owner's device (the next APK).

## 5. Cycle 2 — C1, the facade (contract, as built)

| Contract | Content |
|---|---|
| **Purpose and boundary** | Carry a climber from deck 2's south band (S1's receiver) to deck 4 (44.0 m) with the movement verbs, on the south face east of S1 (`build_c1` in `src/sim/band_stack.cpp`, one static body). Excludes S2 |
| **Output** | The climber standing on deck 4's south band (support: the tower) |
| **Receiver** | Deck 4, south band, top 44.00 m. Acceptance: grounded on the tower with the body's centre above 44.5 m |
| **State and rules** | A sequence of distinct moves through objects with a reason to be there: a loading landing on knee braces off deck 2's edge beam (walk out over the drop); a switchgear cabinet 1.7 m tall on it (mantle); a duct 1.5 m deep along the face, hung on straps from deck 3's edge beam, its lip 3.6 m over the cabinet and 0.4 m clear of it (jump and hang at the top of the jump; climb up); the duct's top, 0.8 m wide over the drop (walk); a vent stack from the duct to deck 3's edge, where a steel plate on the deck and its fascia down the edge beam make one lip (climb; the top-out is over the plate onto deck 3, not onto the edge beam), the stack teeing off under the deck's top into two outlets 1 m either side of the mantle's path; deck 3's monorail, 0.3 m wide and 4.4 m past the face (balance); the ladder hung from deck 4's davit, its bottom rung 2.3 m over the monorail and out of reach standing (a leap, facing back to the building); the ladder's stiles and rungs stop at the davit's arm, so a climber tops out over them onto the arm; the arm, 0.35 m wide (balance back to the deck) |
| **Input** | Walk, mantle (Action), jump, hang, climb up (jump from the hang), take hold (Action), climb, balance |
| **Capacity** | Every rise inside the body's envelope: mantle 1.70 (≤ 1.85), hang 3.60 over the take-off (≤ 3.79 at the top of a jump), ladder rung 2.3 over the monorail (in reach only at the top of a jump). Braces and columns clear of every body pose on the route (the diagrid's braces cross deck 3's edge at x ±6.5 and ±19.5; the route tops out at x 24.0 and 12.5) |
| **Use and recovery** | Seen from S1's gangway: the landing, cabinet and duct east along the face, the yellow monorail and the davit's ladder above. A fall from the route is lethal and restores the last footing on it or on the deck below it; nothing on the route is a dead end. Nothing of it reaches below deck 2, so its only foot is S1 |
| **Proof** (native, `run_stack`) | From deck 2 on player inputs to deck 4 (30.8 s); standing at the monorail's end the ladder is out of reach (the leap is required); the body never moves more than 0.15 m sideways in a tick (observed 0.085 m, a walking step); the Stack in one run from the game's spawn: hold S1's chain, ride, climb C1, deck 4 in 62.4 s without dying. Through the game's input pipeline on each device: `touch_stack`, `pad_stack`, `keyboard_stack` (S1's contract) |

**Changed on the way.** The verdigris main (dressing, 0.8 m, the face's full height) ran
through the landing and the duct; it now climbs the west half of the south face. A ladder's
rungs above a top-out point stopped the mantle over them (it stalled and aborted, and the
climber carried on up the ladder): the davit's ladder ends at the arm, its grab handles stand
wider than a body.

| Defect (cycle 1b) | Invalid state | Transition that allowed it | Change that makes it unreachable |
|---|---|---|---|
| The vent's top-out snapped the view 0.9 m sideways | the mantle over deck 3's edge ran the body into the vent stack's last metre above the deck; pushed aside, it was set back onto its path in one tick (0.58 m, native; 0.89 m in one frame through the game) | the stack ran 1 m above the deck so a climber's hands could reach a height the top-out probe accepts, and that metre stood in the mantle's path | a plate on the deck with its fascia down the edge beam gives one lip the probe accepts from lower down; the stack tees off under the deck and its outlets stand clear of the path. The C1 and Stack runs now fail on any sideways step over 0.15 m in a tick |

## 6. Cycle 3 — S2, the rolling-ballast walking beam hoist (contract, as built)

| Contract | Content |
|---|---|
| **Purpose and boundary** | Carry a rider from Deck 4's south band (44.0 m) to Deck 6 (66.0 m) inside the central shaft (`build_s2` in `src/sim/band_stack.cpp`). Includes the 350 kg cage on a vertical guide with 2.5 m/s brake-only governor, the teeter-totter walking beam (1,200 kg) on a central trunnion hinge, the rolling ballast car (1,800 kg) on the west arm, sheaves, 1:1 tow rope, chock lever, catch, trip line, and chain handle |
| **Output** | The rider standing on Deck 6's south band (support: the tower) within 15 s of pulling the trip handle |
| **Receiver** | Deck 6, south band, top 66.00 m. Acceptance: grounded on the tower with the body's centre above 66.5 m and support `Simulation::kTowerEntityId` |
| **State and rules** | Found at rest: cage latched at Deck 4 (44.05 m), beam horizontal at rest angle = 0, chock lever latched holding beam. Rider boards cage, pulls chain handle hanging at eye height; trip line rotates chock lever past 0.05 rad release; catch drops, beam begins rotation, rolling ballast car rolls down inclined west track shifting centre of mass, towing cage up 22 m to Deck 6. Safety dogs engage at top stop (66.05 m) |
| **Input** | Walk into cage, GRAB chain handle, step back / pull |
| **Proof** (`run_stack` in `tests/simulation_tests.cpp`) | S2 at rest: cage travel = 0, chock latched = 1. S2 ride: 10.2 s, floor_y = 66.04 m, peak speed = 2.51 m/s (≤ 2.6 m/s governor limit), rider aboard throughout; rider walks off upper gangway onto Deck 6 (y = 66.9 m, support = tower) |

---

## 7. Cycle 4 — C2, the east machinery hall & pipe rack (contract, as built)

| Contract | Content |
|---|---|
| **Purpose and boundary** | Carry a climber from Deck 6's south band (66.0 m) along the East machinery hall to Deck 8's north band (88.0 m), a 22 m vertical athletic climb (`build_c2` in `src/sim/band_stack.cpp`). Excludes S3 |
| **Output** | The climber standing on Deck 8's north band (support: the tower) |
| **Receiver** | Deck 8, north band, top 88.00 m. Acceptance: grounded on the tower with body centre above 88.5 m and support `Simulation::kTowerEntityId` |
| **State and rules** | Sequence of authentic industrial obstacles: switchgear enclosure (1.7 m mantle); exhaust manifold duct (jump-and-hang at 70.8 m, mantle onto duct top); wall ladder up to Deck 7 through a dedicated equipment hatch cutout; mantle onto pipe rack girder; 0.35 m monorail balance beam; leap to grab davit hanging ladder (bottom rung 2.3 m above beam, reachable only at leap peak); top out over ladder stiles onto davit arm (89.2 m); crossover bridge over Deck 8 ShaftRail (89.05 m) and step down onto Deck 8 North band |
| **Input** | Walk, mantle, jump, hang, climb, balance, leap |
| **Proof** (`run_stack` in `tests/simulation_tests.cpp`) | From Deck 6 spawn to Deck 8: 28.2 s; ladder leap verified (`ladder_needs_leap = 1`); body tick step ≤ 0.15 m; landed on Deck 8 (y = 88.91 m, support = tower) |

---

## 8. Cycle 5 — S3, the brake-override counterweight hoist (contract, as built)

| Contract | Content |
|---|---|
| **Purpose and boundary** | Carry a rider from Deck 8's north band (88.0 m) to Deck 12's north band (132.0 m) in the North central shaft (`build_s3` in `src/sim/band_stack.cpp`). Uses Archetype 17 (brake override) |
| **Output** | The rider standing on Deck 12's north band (support: the tower) |
| **Receiver** | Deck 12, north band, top 132.00 m. Acceptance: grounded on the tower with body centre above 132.5 m and support `Simulation::kTowerEntityId` |
| **State and rules** | Overloaded 3,500 kg freight car parked at Deck 12 (132.05 m) held by a brake caliper catch; 400 kg counterweight carriage at Deck 8 (88.05 m); 44 m vertical travel. Rider boards cage, takes lanyard handle and pulls; caliper trips, released 3,500 kg car descends 44 m under 2.5 m/s governor (45 kN), hoisting counterweight carriage 44 m to Deck 12. Payload energy gained never exceeds source released |
| **Input** | Walk to cage, GRAB brake trip handle, pull |
| **Proof** (`run_stack` in `tests/simulation_tests.cpp`) | S3 at rest: travel = 0, latched = 1, handle = 1; S3 ride: 18.1 s, floor_y = 132.05 m, peak speed = 2.55 m/s (governor ≤ 2.6 m/s), rider aboard throughout; walk off upper gangway onto Deck 12 North band (y = 132.9 m, support = tower) |

---

## 9. Cycle 6 — C3, the crown trusses & high riser ladder (contract, as built)

| Contract | Content |
|---|---|
| **Purpose and boundary** | Carry a climber from Deck 12's north band (132.0 m) across the crown structure to Deck 14's south perimeter band (154.0 m), landing at `InitialSpawn::Deck154` (`build_c3` in `src/sim/band_stack.cpp`). Excludes AS-006 Stage A |
| **Output** | The climber standing on Deck 14 at `InitialSpawn::Deck154` (-10.5, 154.9, -128.2) with support `Simulation::kTowerEntityId` |
| **Receiver** | Deck 14, south band, top 154.00 m. Acceptance: grounded on the tower with body centre above 154.5 m |
| **State and rules** | Stepped incline girder from Deck 12 North band over ShaftRail; mantle onto atrium ventilation duct (137.2 m); wall ladder to Deck 13 (143.0 m); Deck 13 steel walkway plate to X = -6.0 m; high vertical riser ladder (11 m of rungs from 143.4 to 155.15 m at X = -6.0 m, completely clear of Stage A's cage and guide rails at X ∈ [-12, -9]); top out onto crossover bridge platform (155.18 m) embedding Deck 14 ShaftRail; 3 wide open steps down south to Deck 14 runway; direct walk along Deck 14 to `InitialSpawn::Deck154` |
| **Input** | Walk, mantle, climb ladder, step down |
| **Proof** (`run_stack` in `tests/simulation_tests.cpp`) | C3 climb: 30.5 s, deck14_y = 154.9 m, worst tick step ≤ 0.15 m, landing position (-10.58, 154.9, -128.21), support = `kTowerEntityId` |

---

## 10. The Continuous Full Stack Ascent Contract (Grade → Deck 14, 0 → 154.9 m)

| Property | Measured & Verified Truth |
|---|---|
| **Sequence** | Ground Grade (0.0 m) → S1 ride (22 m) → C1 climb (44 m) → S2 ride (66 m) → C2 climb (88 m) → S3 ride (132 m) → C3 climb (154.9 m) |
| **Duration** | 166.38 seconds continuous physical simulation |
| **Deaths** | 0 deaths (`death_count == 0`) |
| **Path Watch** | Worst tick horizontal displacement ≤ 0.15 m everywhere (no teleports, no physics explosions) |
| **End State** | Grounded on Deck 14 slab (`support_entity_id == Simulation::kTowerEntityId`), y = 154.90 m, pos = (-10.59, 154.90, -128.21) |
| **CI Verification** | 100% tests passed in `scraperx_sim.athletic_traversal` (133.6 s) and `scraperx_sim.parkour_flow` (0.29 s); proof lines asserted in `wo000-delivery-spine.yml` |

---

## 11. Canonical Addition Goalset — The Mega-Ascent (154 → 220 m → 340 m)

This goalset establishes the vertical progression beyond 154 m, coupling the completed Stack (Band 0) into Counterweight Well (Band 1, 154 → 220 m) and Wet Isolation (Band 2, 220 → 340 m).

### Goalset Overview

```
Elev. (m)   Band / Section                      Coupling Mechanism / Challenge
+340.0 ───  TRANSFER PLATE 340 (TP-340)  ─────  Finale of Band 2 (Wet Isolation)
            AS-007 Stage F (Hydraulic Platform) 42 m high-pressure water lift (298 → 340 m)
            AS-007 Stage E (Pressurized Cab)    42 m pneumatic piston lift (256 → 298 m)
            AS-007 Stage D (Pipe Spool)         Hydraulic fill line repair & valve trip (220 → 256 m)
+220.25 ──  RING 220 (Atlas Band B02 Top) ────  Handoff from Band 1 to Band 2
            AS-006 Stage C (Debris Chute)       900 kg rubble dumpster trip; cascades rubble to re-arm A
            AS-006 Stage B (Derrick Boom)       2,500 kg lattice boom falling counterweight (176 → 198 m)
            AS-006 Stage A (Skip Lift)          800 kg billet skip lift (154 → 176 m)
+154.00 ──  DECK 14 (InitialSpawn::Deck154) ──  HANDOFF NODE: C3 exit → Stage A entry
            The Stack (Band 0: S1→C1→S2→C2→S3→C3) Complete continuous ground-up ascent (0 → 154 m)
  0.00 ───  GROUND GRADE (Exterior Yard)  ────  Game start
```

### Cycle 7 — S3/C3 → Stage A Handshake & Grade-to-176m Continuous Run

| Field | Contract Specification |
|---|---|
| **Objective** | Couple C3's exit landing on Deck 14 into AS-006 Stage A boarding and shackle rigging, creating an unbroken simulation from Ground Grade (0.0 m) to Ring 176 (176.25 m) |
| **Input / Verbs** | Step off C3 runway at (-10.5, 154.9, -128.2) → `board_well_a` (walk to -10.0, -129.2 then into cage at -10.2, -130.6) → `rig_well_a` (walk to bollard at -11.35, -131.95, GRAB shackle, walk to cage eye at -11.35, -131.40, RIG) → ride Stage A to 176.25 m |
| **Acceptance Criteria** | 1. `death_count == 0` across continuous run from Grade to Ring 176.<br>2. Stage A lifts rider 22 m to floor y = 176.25 m under 2.5 m/s governor.<br>3. Payload energy gain does not exceed skip energy released.<br>4. Rider steps off onto Ring 176 (y > 176.7 m, support = tower). |
| **Verification Gate** | New test `require(continuous_stack_to_stage_a(band))` passing in `scraperx_sim_tests` and asserted in `wo000-delivery-spine.yml`. |

### Cycle 8 — Band 1 Full Coupler & Grade-to-220m Continuous Mega-Ascent

| Field | Contract Specification |
|---|---|
| **Objective** | Complete the entire Counterweight Well sequence in continuity after The Stack: S1 → C1 → S2 → C2 → S3 → C3 → Stage A → Stage B → Stage C, reaching Ring 220 (+220.25 m) in one single continuous run |
| **Chain Links** | 1. Stage A (154 → 176 m): skip counter-mass.<br>2. A → B handoff: walk across Ring 176 into B's cage, trip derrick boom lanyard, ride 22 m to 198 m.<br>3. B → C handoff: walk across Ring 198 to C's platform, shove debris clearing chute, trip dumpster catch, ride 22 m to 220 m.<br>4. Cascade proof: C's dumpster dumps 900 kg rubble down chute directly into Stage A's spent skip at 154 m, re-arming Stage A. |
| **Acceptance Criteria** | 1. Unbroken run from Grade (0.0 m) to Ring 220 (220.25 m) in 0 deaths.<br>2. Rider stands on Ring 220 (y > 220.7 m, support = tower).<br>3. Stage A is confirmed re-armed (`stage_a_rearmed == 1`).<br>4. Worst tick step ≤ 0.15 m throughout all transitions. |
| **Verification Gate** | New test `require(mega_ascent_grade_to_ring220(band))` passing in `scraperx_sim_tests` and asserted in CI workflow. |

### Cycle 9 — Counterweight Well Non-Lift Climbing Spine (154 → 220 m)

| Field | Contract Specification |
|---|---|
| **Objective** | Build and prove the manual athletic parkour route from Deck 14 to Ring 220 as the designated non-lift alternative (per Rule 7 and AS-006 §The climbing route) |
| **Route Geometry** | 1. 154 → 176 m: timber balk footing over the well, rung ladder against 176 ring inner face, mantle onto Ring 176 north deck.<br>2. 176 → 198 m: east ring catwalk, pipe rack diagonal brace balance beam, jump-and-hang on 198 ring soffit strap, climb onto Ring 198.<br>3. 198 → 220 m: monorail beam out into well, leap to hanging ladder under 220 ring davit, top out onto Ring 220. |
| **Acceptance Criteria** | 1. 154 → 220 m climbed without operating any lift mechanism.<br>2. Zero bypass of machines below 154 m.<br>3. All rises and reaches within strict body envelope (mantle ≤ 1.85 m, jump reach ≤ 3.79 m).<br>4. Worst tick step ≤ 0.15 m. |
| **Verification Gate** | New test `require(climb_cw_well_spine(simulation))` in `scraperx_sim_tests`. |

### Cycle 10 — Band 2 Ascent Coupler: Grade → TP-340 (340 m)

| Field | Contract Specification |
|---|---|
| **Objective** | Connect Ring 220 into AS-007 (Wet Isolation) to achieve a continuous ascent from Grade (0 m) to Transfer Plate 340 (TP-340, +340 m) |
| **Mechanisms** | 1. Stage D (220 → 256 m): seat D's pipe spool, throw hydraulic fill valve.<br>2. Stage E (256 → 298 m): shut cab door, trip air reservoir, ride pressurized cab.<br>3. Stage F (298 → 340 m): open main accumulator, ride hydraulic ram platform to TP-340. |
| **Acceptance Criteria** | 1. Grade to TP-340 in one unbroken physical simulation.<br>2. Complete execution across all three macro systems: Freight (S1/S2/S3/A/B/C), Traversal (C1/C2/C3/spine), and Process/Flow (D/E/F).<br>3. Rider arrives at TP-340 (y = 340.0 m) with 0 deaths. |
| **Verification Gate** | `PASS scraperx_sim mega_ascent_grade_to_tp340` emitted and asserted in CI workflow. |
