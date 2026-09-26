# SCRAPERX — MECHANISM ASCENT PLAN

**Status:** the current plan for the whole ascent, from grade.
**Provenance:** re-planned with the owner's direction of 2026-09-25: *"start from the ground level,
and do all the mechanics and mechanisms all over again ... maybe a couple of mechanisms can be
stacked, and then a climb, and then another mechanism, and then a climb"*. Revised 2026-09-26 on
the owner's direction that the earlier plans *"didn't work, and we used them the first time"*
and that the `ChatGPT` branch's new macro-mechanism planning should be taken and fixed: its
authoring contract is adopted below (§2), its opening mechanism is audited with numbers (§7),
and its catalogue is mapped onto what this engine can build (§8).
**Method:** mechanism-chain-forge (backward from the effect; contract; causal proof) with the
causal-mechanism-compiler's `MACRO-TRAVERSAL-STRICT` profile and its deterministic evaluator
(`stage1dof.py`) for every drive, terminal and band claim.
**Supersedes:** this plan's 2026-09-25 chain table, whose stages above C1 were listed as if they
were designed. They were not; they are options now (§3). The owner's 20 lift archetypes
(`Mechanism-Ideas-and-archetypes.md`) remain a source of ideas, not of designs.

## 1. What changed, and why

The owner played the build and found every mechanism incomplete, the ramps no fun to climb,
test cubes all over the yard, and machines that did not work. So, from 2026-09-25:

| Before | Now |
|---|---|
| A stair of 14 ramp flights walked the player to 154 m; floating block steps and swing flights climbed the south face | No ramp or stair is a route. A level is gained by a machine or a designed climb |
| Movement test blocks, three kernel test rigs, a steam plant and three half-mechanisms stood in the player's world | The game world holds only the building, its machines and its climbs. Movement test blocks exist only in a world started at one of their test spawns |
| Parked lift cages, static gears, jib cranes with hanging crates stood on the tower as dressing | No machine is dressing: every machine in the world works |
| The first stage stood at 154 m | The first stage stands at grade |

And from 2026-09-26, after the owner could not operate S1 on the device and asked that no build be
green unless the ascent itself is proven:

| Before | Now |
|---|---|
| Stages were planned many storeys ahead, with heights, machines and verbs written down before any of them was derived | Only the next stage from a proven exit is designed, and only with numbers the evaluator has integrated. Everything above it is an option (§3) |
| "Proven" meant native tests on player inputs | Proven means the stage is also played through the game's own input pipeline (touch, pad, keyboard) from the game's start, and CI builds the APK only after that passes |
| A drive was sized for enough energy; surplus was margin | Surplus energy is the arrival speed. Every drive is shaped and every terminal is a catch, catch rack or pad sized for the whole band (§2.3) |

## 2. The authoring contract (adopted from `ChatGPT`, corrected)

### 2.1 Work backward, prove forward

Start with the player's supported destination. Resolve the transfer geometry, the load path, the
source's energy and the reachable control in that order, then run the whole chain forward from the
game's start through normal input. Backward derivation is a design method, not evidence.

| Contract | Must be stated before a line of code |
|---|---|
| Receiver | the native support the player ends on, its walking surface and clear standing area, stability with the player on it, the way on, and where a fall lands |
| Transfer | capture geometry, clearances along the whole swept path, relative speeds at the handoff, allowed placement variation |
| Transmission | the actual bodies, hinges, guides, ropes and contacts, their arm lengths and how the mechanical advantage changes over the stroke |
| Source | mass, centre of mass and inertia; usable height; the drive Q(q) over the stroke; losses; what is left at the terminal |
| Player access | a supported approach, a visible control, bounded effort (≤ 150 N over ≤ 0.3 m, DEFAULT), free hands, and what an early or late input does |
| Persistence and retry | what is spent, what is lost, the way back, and a coherent checkpoint restore of the whole machine |

Inputs and outputs match at the same instant in position, orientation, velocity, load, energy,
constraint state and player access; the same altitude or the same label does not close a handoff.
No hidden stubs, remote attachments, completion flags or pose snapping.

### 2.2 Readability and pace

Large geometric causes: something rolls, tips, swings, falls, lands or becomes a support. The
source, the transfer and the outcome are seen from where the stage is found, without the HUD. The
main motion lasts about 5–15 s (a pacing target measured in the run, never a timer). A stage's
output is a stable support, bridge, stair or ride the player can use and leave; no mandatory
launches across a gap. One or two player actions per stage; no compulsory carrying of pins,
cables or tools ahead of a gravity event.

### 2.3 Drive and terminal (the correction)

The `ChatGPT` plan treats energy left over after the lift as margin. It is the arrival: an
unshaped drive accelerates through the whole stroke and delivers that surplus into the receiver
(§7: 11 m/s into the landing in 1.35 s). So every stage shows, with the evaluator:

- the drive Q(q) shaped by geometry (an offset tail, a hanging chain, a changing arm) so that it
  fades, crosses zero or reverses near the terminal;
- a terminal that captures every band case: a catch rack where the incoming energy is uncertain
  (a pile, a pour, a friction band), a pad or crush bed sized for the fastest case, a seat the
  residual drive holds the body into;
- the rest matrix: rider on, rider off, load spilled, handoff actuated;
- h/4 convergence (`INTEGRATED`), and later the same numbers from the engine.

### 2.4 Movement and units

SI, Y up; the standing capsule is 1.8 m tall and 0.35 m in radius, 1.2 m crouched; step-up 0.35 m;
mantle 0.35–1.85 m; hang catch up to 3.79 m above take-off at a jump's apex (source:
`simulation.cpp`). Body centre, centre of mass and walking surface are different numbers. Movement
is never retuned to hide a local geometry flaw.

### 2.5 Evidence, readiness and proof

Every number is typed CHOSEN, DERIVED, INTEGRATED, MEASURED, DEFAULT or UNRESOLVED. A critical
unresolved release load, receiver, stopping law or recovery path makes a stage BLOCKED; a guessed
number does not unblock it. A stage is proven when: the native falsifiers pass (no source → no
motion; no link → no motion; the ledger closes; the rest matrix holds; the swept path is clear);
it is played through the game's input pipeline from the game's start on touch, pad and keyboard;
and CI builds the APK only after those pass. A fresh APK proves packaging only; the device is its
own gate.

### 2.6 Rules kept from the first plan

1. **No link, no lift.** Stored energy stays stored until the player completes the chain.
2. **Energy pays for height,** checked from body states every tick.
3. **Every link is physical:** contact, constraint or force; a simplified model (water, rubble)
   is declared in the stage record.
4. **Rides are moving supports.** The rider inherits the payload's motion.
5. **Nothing strands.** Dying restores the last commit, machines included; a machine that can be
   spent without the player riding it returns by itself or leaves another way on.
6. **Built to be read.** Every part is held by visible structure; what the player handles is
   marked (hazard or yellow).
7. **Climbs are designed, not free**, and a climb never lets the player bypass the machine below it.

## 3. The chain

Band 0, **The Stack** (grade → 154 m). Only what is proven or designed has heights and verbs.

| # | Kind | Heights | Where | What the player does | Status |
|---|---|---|---|---|---|
| S1 | Machine: **water-balance hoist** (counter-mass; water as the mass) | 0 → 22 m | south face, east of centre | walks into the cage and takes hold of the valve chain hanging in it: held, it pulls the lever overhead down, which opens the header tank's valve and lets the bucket's catch go; water runs into the empty bucket at the head until it outweighs cage and rider, the bucket falls 21.8 m and the cage rides up to a gangway onto deck 2. At the foot the bucket drains on a striker and the cage comes back down by itself | built and proven, natively and through the game's input on three devices (cycles 1, 1b) |
| C1 | Climb: **the facade** | 22 → 44 m | south face, east of S1 | out onto a loading landing, a mantle onto its switchgear cabinet, a jump to hang from the duct along the face, up onto the duct and along it, up a vent stack over deck 3's edge; out along deck 3's monorail under the ladder hung from deck 4's davit, a turn and a leap for it, up it onto the davit's arm and back along the arm onto deck 4 | built and proven, natively and through the game's input on three devices (cycles 2, 1b) |
| S2 | Machine: **the swinging stair** (ROTATE → DEPLOY; counterbalanced bascule) | 44 → 55 m | south face, west of centre | walks west along deck 4 onto a landing beside a 17 m steel stair standing upright outside the face, and takes hold of the chain hanging from a trip lever: it throws, the hook lifts off the stair's lug, and the stair swings down over 8 s, slowed by the 13.5 t counterweight behind its hinge, until a blade under its top landing drives into timber jaws at deck 5's edge; the player walks up it onto deck 5 | built and proven, natively and through the game's input on three devices (cycle 3) |
| — | Options above deck 5 | 55 → 154 m | — | a climb from deck 5; then candidates from §8 chosen from the exit S2 actually leaves. Nothing here is designed | options |
| — | AS-006 … AS-009 (legacy bands) | 154 → 640 m | shaft | built by the retired plans, proven only natively (AS-006 A and C also through the game's input). Not part of the proven route | owner's decision: keep in the world for review, or retire from the default world as the `ChatGPT` branch did |

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

**Open ends.** S2 (deck 4 → deck 5), downstream of C1, is built and proven in §6.

## 6. Cycle 3 — S2, the swinging stair (contract, as built)

Profile `MACRO-TRAVERSAL-STRICT`. Evaluator: `stage1dof.py`, beam model, h = 1 ms, every case
rerun at h/4 (spec at the end of this section). Engine: Jolt at 90 Hz; MEASURED numbers are the
engine's, from `tests/simulation_tests.cpp` (`run_s2`) on player inputs.

### 6.1 Outcome and receiver

The player stands on deck 5's south band (support: the tower, top 55.00 m) having walked up a 17 m
steel stair that swung down from upright and rests with its treads level and its top landing level
with deck 5. Receiver: deck 5, reached from the top landing's open north side over a static plate
(x −2.45 … −1.20, z −124.05 … −123.14, top 55.00). A fall from the stair lands on deck 4's band or
past the face (lethal: restores the last footing).

### 6.2 Topology and rejected candidates

ROTATE → DEPLOY: a counterbalanced bascule. The flight's own weight is the source; a cast
counterweight behind the hinge shapes the drive; timber jaws on the last of the hinge's travel are
the terminal; one pull on a chain is the only action.

| Candidate | Rejected by |
|---|---|
| Ingot skip (the old plan's S2: carry four ingots into a counterweight skip, board, pull a trip) | a carrying chore ahead of a gravity event (§2.2); two ordered actions (the S1 failure's pattern); stepping off the platform with three ingots across while holding the trip sends it up empty and strands the player (DERIVED from the masses: 300 + 3·40 kg against 255 + 3·40 kg). Draft code parked, not pushed |
| A second water-balance hoist | the same verb and the same motion as S1 |
| Uncounterweighted falling stair | 114.7 kJ released into the seat: 0.77 rad/s, the top at 13 m/s (DERIVED) |
| A gravity hook as the catch (the hook falls back when the chain is let go) | let go at once, the hook fell back across the lug's path and caught the stair after 0.018 rad; with 3.8 kN of lug on it, lifting it again needed about 2 kN at the chain, past the grip (900 N): a softlock (MEASURED). Replaced by a trip lever that throws over its dead point and stays thrown |
| Open treads | stood upright, open treads lie at 41° one above another, and a body pushing into them rode up the stack (MEASURED). Every step is closed by a riser: the upright face then leans back only 8°, and a jump into it gets no higher than a jump |

### 6.3 Geometry (as built)

| Item | Value |
|---|---|
| Hinge | (x −15.00, y 43.70, z −122.10), axis along z; its shaft's ends in bearings on the landing's side beams |
| Flight | 43 treads and a top landing, rise 0.25, going 0.2917 (level at 40.6°), every step closed by a riser; two 0.5 m stringers at z ±0.95; flat 0.20 × 0.06 handrails and posts, too broad to grip; the 1.0 m top landing spans the full width, its north side open; a 0.5 m blade under it |
| Stored | upright at 82° on its catch; the top landing at 61.3 m |
| Seated | 40.6°: the foot plate at 44.00 (deck 4's level), the top landing at 55.00 (deck 5's), x −2.21 … −1.21 |
| Counterweight | a cast block 1.1 × 1.1 × 1.78 m, 13.5 t (declared 6,268 kg/m³), 2.5 m from the hinge, 2° round from opposite the flight's centre, on two arms in line with the stringers, under the landing |
| Landing (deck 4) | x −19.50 … −15.43, z −124.05 … −120.60, top 44.00, over deck 4's edge beam; side beams carry the bearings; rails on its south edge and west end |
| Catch | a trip lever (60 kg, 50 of them a cast weight on a mast 11.5° east of upright over its pivot) on a post at the landing's south edge; its hook over a lug on the south stringer; the chain hangs from its west arm over a guide, the handle's top at 45.95 m |
| Receiver (deck 5) | timber jaws (x −2.20 … −1.05, y 53.55 … 54.20) either side of the blade's path on a base plate, on two brackets hung from deck 5's edge beam; the plate onto the deck |
| Swept volume | every stair part against every static part and the trip lever (down, at its dead point, thrown), at 401 angles from stored to the stop: 0.03 m clear (DERIVED) |

### 6.4 Mass, drive and energy

| Quantity | Value | Evidence |
|---|---|---|
| Flight | 4,000 kg over its parts by volume; centre 8.42 m from the hinge at 41.7° (seated); I_cm 140,612 kg·m² | CHOSEN; DERIVED from its parts |
| Pair | 17.5 t, centred 6.8 cm from the hinge on the flight's side; 510,984 kg·m² about the hinge | DERIVED; mass and centre MEASURED as built |
| Q(q) | 11.5 kN·m on the catch, 7.3 kN·m seated: the stair presses into its jaws | DERIVED |
| Breakaway | 2.55 × the 4.5 kN·m static-friction corner of the band | INTEGRATED |
| One swing | the flight drops 102.9 kJ; the counterweight takes up 95.9 kJ (93 %); 6.9 kJ is the swing | MEASURED |

### 6.5 Band sweep (INTEGRATED) and the engine

Band: hinge friction 0 / 1.5 / 3 kN·m. Every case HELD_BY_BUFFER_FRICTION in the jaws (300 kN·m
from 0.0232 rad before the seat): swing 8.3–9.7 s, peak 0.137–0.165 rad/s (the top landing at
2.4–2.9 m/s), rest 40.57–41.00° (the top landing within 0.13 m of deck 5), ledger residual ≤ 0.35 %,
h/4 differences ≤ 7.3e−6 rad/s and 0.00075 s. The engine's hinge is frictionless: into the jaws
at 7.96 s at 0.1645 rad/s, at rest at 40.56° (MEASURED; the evaluator's frictionless case gives
0.1645 and 40.57°).

### 6.6 Modes and rest matrix

HELD (catch) --the chain pulls the trip lever past its dead point (0.20 rad); its weight throws it
to its far stop, and the catch lets go at 0.26 rad--> SWINGING --the blade enters the jaws
(q ≥ 0.6994)--> GRIPPED --ω = 0--> SEATED. The lever stays thrown and nothing relatches: the stair
stays down, and every checkpoint from then on is above it.

| Rest variant | Result | Evidence |
|---|---|---|
| no rider | held by the jaws, pressing into them with 7.3 kN·m | MEASURED |
| a rider walking up it | the stair moves 0.0001 rad | MEASURED |
| a rider on the top landing through the swing | held by the jaws in three band cases; frictionless, it rides on to the jaws' bottom (the hinge's stop, 0.015 rad past level) at 0.050 rad/s | INTEGRATED |
| a rider running up it as it falls | rests on the jaws' bottom at 39.74°, the top landing 0.26 m below deck 5 — a step; the rider walks off onto deck 5 | MEASURED |

### 6.7 Falsifiers (engine) — all pass

1. No link: untouched for 60 s, the stair stands on its catch, the lever down, the chain in reach.
2. The counterweight does the shaping, and 3. the ledger: of the flight's 102.9 kJ the counterweight
   takes up 95.9 kJ; the rest is the swing's kinetic energy, to within 6 J at every tick before
   the jaws (limit 2 % of the swing).
4. Arrival: 0.1645 rad/s peak (limit 0.19) into the jaws at 7.96 s; rest within 0.04° of level
   (limit 0.35°).
5. Swept path: the 6 J ledger bounds any contact before the jaws; the sweep is 0.03 m clear.
6. Rest: walked up, the stair moves 0.0001 rad; an eager rider brings it onto the jaws' bottom
   and still walks off onto deck 5.
7. Not a ladder: six tries at the upright stair — jumps into it, climbs, mantles — reach 1.79 m
   (one jump) and end back on the landing.
8. The game's input: `touch_stack`, `pad_stack` and `keyboard_stack` walk from the game's start at
   grade to deck 5 through the real input pipeline (95–97 s), and CI builds the APK only after
   they pass. They also hear the swing: the hinge creaking while the stair turns, a clang where
   the trip lever hits its stop and where the jaws stop the stair (the audio reads the kit's own
   motion: a machine is any moving kit body of 40 kg or more).

### 6.8 Kit capabilities added (they were the blockers)

1. **Per-part density** (`Part::density`): a declared part weighs density × volume, the rest of the
   body's mass is spread over its other parts by volume, and its centre and inertia follow.
   Bodies that declare none are built as before.
2. **A lever pad** (`add_lever_pad`): friction on the hinge while the lever's angle lies in a range.
3. **A catch on a hinged body**: the existing catch holds a lever body still (60 s) and lets it
   go cleanly; no change was needed.

### 6.9 Evaluator spec (as built)

```json
{
 "name": "S2 as built: counterbalanced swinging stair, deck 4 -> deck 5; jaws as a hinge slide plate",
 "model_kind": "beam",
 "model": {
  "masses": [
   {"mass": 4000.0, "r": 8.415382, "phi": 1.691575, "i_cm": 140611.549},
   {"mass": 13500.0, "r": 2.5, "phi": 4.797975, "i_cm": 2722.5}
  ],
  "theta0": 0.0,
  "torque_friction_kinetic": 1500,
  "torque_friction_static": 4500
 },
 "terminal": {
  "buffer": {"kind": "slide", "start": 0.699366, "stroke": 0.0382, "force": 300000.0},
  "stop_q": 0.737566
 },
 "band": {"model.torque_friction_kinetic": [0, 1500, 3000]},
 "require": {
  "allowed_outcomes": ["HELD_BY_BUFFER_FRICTION"],
  "min_breakaway_ratio": 1.5,
  "max_stop_impact_speed": 0.0,
  "max_time_s": 15,
  "convergence": {"peak_speed": {"abs": 0.002}, "time_s": {"abs": 0.05}}
 }
}
```

q is the stair's fall from 82° (radians); its direction at q is 180° − (82° − q) in the
evaluator's frame. The flight's mass and centre come from its parts as built (risers included).

## 7. Audit of the `ChatGPT` branch's opening mechanism

Its Atlas §6, the pipe-loaded balance bridge: 20 pipes of 800 kg roll into a pan on a 5 m short
arm; the 20 m long arm, a 4 t deck, rises 21.72° to seat on a +8 m landing. Audited as written
with the same evaluator (beam model; trunnion friction band 1.5–4.5 kN·m, DEFAULT ±50 %).

| # | Finding | Evidence |
|---|---|---|
| 1 | As written the deck reaches its landing in 1.35 s at 0.556 rad/s: the far end strikes the seat at 11.1 m/s. The "139 kJ remaining" in its budget is not margin; it is the impact. The 5–15 s pacing target is also missed | INTEGRATED, 3 cases |
| 2 | Shaping fixes the arrival: the short arm bent 49.3° below the deck's line (the tail offset) crosses the drive's zero at 0.198 rad; with a catch at the landing and a crush pad, every friction case falls back onto the catch at 0.015–0.039 rad/s in 4.0 s, held with a rider on the far end and with the pan emptied | INTEGRATED, 4 cases |
| 3 | But a pile of pipes is not a known mass. Tuned for 16.0 t, the bridge falls short of its catch with 15.9 t in the pan (an eighth of one pipe missing) in every friction case but the lowest, and in every case at 15.6 t | INTEGRATED, per mass |
| 4 | Robust form: the offset sized for the lightest credible load (47.5°, 15.2 t at the highest friction) and a catch rack every 0.005 rad (0.1 m at the far end) above a 0.05 rad crush bed: every case from 15.2 to 16.8 t captures, fall-back ≤ 0.032 rad/s (0.65 m/s at the far end), 3.2–4.0 s, held with a rider and emptied. The 16.8 t case crushes 0.0499 of the bed's 0.05 rad: a heavier load needs a longer bed, and 3.2–4.0 s is still under the 5 s pacing floor | INTEGRATED, 12 cases |
| 5 | It is not buildable in this engine as specified: rolling hollow pipes need cylinder shapes, the level pan needs a hinge between two moving bodies, and the landing needs a catch rack on a lever and a crush bed. None exists in the kit (§8). A rubble pour through a gate (the kit's bins) gives the same macro loading with a known mass, which removes finding 3 at the source | source inspection |
| 6 | Its +8 m landing has no onward route; the branch says so | its Atlas |

Adopted from it: the authoring contract (§2.1), readability and pacing (§2.2), the complexity
budget, the rule that a stage's output is a support the player uses and leaves, planning only from
a proven exit, and its physics corrections to the Colossus sequence (§8). Not adopted: ordinary
stairs to 154 m as a fallback (the owner's direction on this branch is that no stair is a route;
a decision for the owner), and removing the legacy bands from the default world (§3, the
owner's decision).

## 8. The catalogue against this engine

What the kit builds today (source: `mechanism_kit.hpp`): bodies made of boxes, a density declared
per box where it matters (§6.8); world-fixed hinges between hard stops, frictionless but for a pad
over a range of angles; straight guides with a brake-only
governor that eases into each stop, dogs (a catch rack on a guide) and rail gaps; catches released
by a lever or by pulling a pin, relatching; ropes over fixed sheaves (any ratio; a rating that
parts; a clutch; a push-only strut); trip lines to handles; bins that pour rubble or water through
a gated mouth and drain on a striker; pools and pipes with valves; rigging (shackles, anchors);
carrying. Missing: round bodies, joints between two moving bodies, springs, crush beds, fracture.

| Family | Build now? | Missing | Notes (the `ChatGPT` corrections kept) |
|---|---|---|---|
| Counterweight lift, fixed or variable ballast | yes | — | S1. Unshaped: keep strokes short or near balance, or shape it |
| Chain counterweight (drive fades as chain piles) | no | a rope end whose hanging mass falls with travel | the skill's worked example; the best shaped lift drive |
| Bascule, swinging stair, drawbridge | yes | — | S2 (§6). Offset tail places the drive's zero; a trip lever, not a gravity hook, as its catch |
| Balance bridge (beam + ballast) | partly | pan hinge on the beam, rolling bodies, catch rack on a lever | load it by a gated rubble pour, not a pipe avalanche (§7) |
| Pendulum striking a receiver | yes (world hinge + contact) | — | strike before the apex, where there is speed; restitution is a separation ratio, not an energy source |
| Toppling column, falling monoliths laying a stair | yes (boxes tipping on contact) | — | each upright slab is its own preloaded source; spacing follows the tip geometry; each rest pose must carry the player |
| Tipping platform | yes | — | tips when the combined centre of mass passes the edge |
| Gravity sled on a chute | yes (guide) | — | friction band spreads the apex by metres over long chutes: short paths, catch racks |
| Block and tackle | yes (rope ratio) | — | force times distance is conserved |
| Rolling roller, drum, spool, pipes | no | round bodies, a drum-wrap model | a drum's rotational energy counts; keep the cable engaged, never a snapped chain |
| Spring plunger, scissor jack | no | springs | a pantograph adds no energy; derive preload, force against extension, recharge |
| Newton's cradle | no, and not wanted | spheres | a rigid-body solver does not carry a compression wave; one pendulum does the same job |
| High striker | yes | — | 100 kg up 100 m needs ≥ 98.1 kJ and 44.3 m/s at the launch; strike below the apex |
