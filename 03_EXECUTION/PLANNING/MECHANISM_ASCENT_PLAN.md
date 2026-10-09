# SCRAPERX — MECHANISM ASCENT PLAN

**Status:** the current plan for the whole ascent, from grade.
**Provenance:** re-planned with the owner's direction of 2026-09-25: *"start from the ground level,
and do all the mechanics and mechanisms all over again ... maybe a couple of mechanisms can be
stacked, and then a climb, and then another mechanism, and then a climb"*. Revised 2026-09-26 on
the owner's direction that the earlier plans *"didn't work, and we used them the first time"*
and that the `ChatGPT` branch's new macro-mechanism planning should be taken and fixed: its
authoring contract is adopted below (§2), its opening mechanism is audited with numbers (§13),
and its catalogue is mapped onto what this engine can build (§14). Revised 2026-09-28 when the
owner had the `Gemini` branch's world merged into this branch: its stages from deck 4 to deck 14
and its link into the well to +221 m are §6–§12.
**Method:** mechanism-chain-forge (backward from the effect; contract; causal proof), with the
deterministic evaluator (`stage1dof.py`) for drive, terminal and band claims where it applies.
**No list of allowed parts** (the owner, 2026-10-08). The `MACRO-TRAVERSAL-STRICT` profile this
plan once adopted banned gears, cams, ratchets, pawls, clutches, fluid drives, buoyancy and motors
from a machine's load path; that ban is lifted. Any machine that works in the world may be built:
the test is the physics, not a parts list. What stays: motion is caused (the solver moves every
body; nothing is keyframed or scripted), every force is finite, a powered machine's motor has a
rating it can stall against, and every machine is proven with the real player.
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

And from 2026-09-28, the owner's direction on easy paths:

| Before | Now |
|---|---|
| No stair, ramp or ladder was a route at all | The route is the machines and the parkour climbs. Stairs, ladders and other easy paths are the **backup** route, for a player who does not feel like working out the climb or the machine (§2.6, rule 8) |

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
8. **Easy paths are the backup.** A stair, ladder or plain walkway may take a player past a
   stage when they do not feel like working out its climb or its machine. It is the backup, not
   the route: each stage is designed around its machine or climb, and the backup is the plainer,
   slower way beside it. It is the one sanctioned bypass (rule 7 still holds for climbs).
9. **Nothing repeats.** The owner (2026-09-30): *"there are way too many machines or mechanisms
   that are the same. You walk up to a block hanging from a string, you grab and pull, and that
   is seen over and over and over and over … I don't want repetitive anything. That is extremely
   important and you need to make note of this."* Each new stage gives the player an action, a form and a look that no earlier
   stage has, and its record names the action it introduces. The kit's parts (ropes, guides,
   catches, pins) are reused freely; the encounter is not. The hanging handle that the player
   grabs and steps back from is spent: it trips about twenty machines from S1 to O, and no new
   stage uses it. Reworking the existing ones is open for the owner.
10. **The initial state determines the final state.** The owner's law for this physics world
    (2026-09-30): *"The initial state should determine the final state."* The same world state
    and the same inputs give the same final state, bit for bit: no random numbers, no clock time
    and no frame-rate dependence in the simulation (it steps at a fixed 90 Hz), and no outcome
    decided by script, so a machine ends where its as-found physics takes it. The native
    `determinism` check runs the same start and the same inputs in two simulations and compares
    every body's state at the end. Not established: the same result across machines (the
    Android ARM build against the x86 CI build).
11. **Not chutes and ladders.** The owner (2026-10-06): *"I didn't ask you to make me a game of
    3D chutes and ladders … There's lifts that need a pull of something, and fucking ladders
    throughout the whole damn game. So boring."* No new ladder on the route (a ladder may only
    be the backup of rule 8), and no lift that starts when the player pulls something. A stage
    is a physical experience the route has not had yet: thrown by stored energy, swung on a
    hanging mass, carried by something falling, tipping or rolling, a run and leap the
    player's momentum decides. The ladders and pull-started lifts already on the route from
    grade to the +221 m ring stay as they are (the owner, 2026-10-08: *"Leave S2, S3 and the
    ladders alone, just build machines"*); S1's weight start (cycle 1c) was the only one changed.
12. **Fill the gaps with machines.** The owner (2026-10-08): *"I want you to fill empty gaps with
    machines."* The supplied vertical-machine packs (`ScraperX-Vertical-Machines`,
    `…-Expansion-01`: gravity balance, traction tram, rack climber, crown gondola, barrel helix,
    cascade mast, luffing derrick, pitman lift) and the owner's stone wheel and slab-and-incline
    drawings are built as designed, motors included, where the route has no way up; working
    machines are not redesigned. The aim (the owner, 2026-10-08): fill the band and level gaps
    *"so the player can canonically climb from grade/ground all the way up the tower"*, one
    unbroken route from grade to the summit. The summit is at **800 m** (the owner, 2026-10-08:
    *"I think the tower's summit will be 800 m, not 1,600m"*): the route's gaps are the 264 ring's
    dead end and the 750 deck to 800 m. What stands at the very top is the owner's to decide
    (2026-10-08: *"I haven't figured out what will be at the very top yet"*): the machines climb to
    800 m and the top is left open; the tower is not cut down until the owner decides.

13. **Irresistible at first sight.** The owner (2026-10-08) described the bar as a scene: a kid
    holds the game in a store, sees it, and thinks *"I must play this now!"*. The game should
    look, feel and play so that anyone who glimpses it reacts that way, *"as it is too enticing and
    gorgeous to pass up because it was created with such comprehensive & delicate care"*. It is
    not a literal instruction or a deadline; it is the reaction to earn. Cheap is not a goal: a thing is worth doing for its value. Open work toward it
    (the owner, 2026-10-08): shortcuts that reward breaking the intended route, because the
    physics allows them; the height felt (wind that grows and pushes, the body bracing, the city
    dropping away, the structure creaking under load); pressure from the world on one or two
    bands (scaffolding collapsing below, water rising, fire spreading), not a chaser; a reason to
    reach the top, visible from the ground once the owner decides what stands there; and a care
    pass on the look, feel and first minutes, then every stage. No ghost racer.

14. **It is a paid product.** The owner (2026-10-08): *"This game will be public-facing and real
    money will be exchanged for it. It must look, feel, and play like it."* The work toward a paid
    release, and the decisions it waits on, are in `RELEASE_PLAN.md` beside this file.

## 3. The chain

Band 0, **The Stack** (grade → 154 m). Only what is proven or designed has heights and verbs.

| # | Kind | Heights | Where | What the player does | Status |
|---|---|---|---|---|---|
| S1 | Machine: **water-balance hoist** (archetype 01, counter-mass; water as the mass) | 0 → 22 m | south face, east of centre | walks into the cage and onto the scale plate in its floor; nothing to pull (rule 11). Hung on a line from the valve's lever 0.1 m proud of the floor, the plate sinks under the rider's weight, and the line turns the lever: it lets the bucket's catch go and opens the header tank's valve, and a pawl drops behind it and holds it open. Water runs into the empty bucket at the head until it outweighs cage and rider; the bucket falls 21.8 m, knocking the pawl out as it leaves the head, and the cage rides up to a gangway onto deck 2. At the foot the bucket drains on a striker and the cage comes back down by itself | built and proven (cycle 1; control repaired, cycle 1b; started by the rider's weight, cycle 1c) |
| C1 | Climb: **the facade** | 22 → 44 m | south face, east of S1 | out onto a loading landing, a mantle onto its switchgear cabinet, a jump to hang from the duct along the face, up onto the duct and along it, up a vent stack over deck 3's edge; out along deck 3's monorail under the ladder hung from deck 4's davit, a turn and a leap for it, up it onto the davit's arm and back along the arm onto deck 4 | built and proven (cycle 2) |
| S2 | Machine: **walking beam hoist with fixed ballast cart** (archetype 18 mutated) | 44 → 66 m | the shaft, south | walking beam on central trunnion, fixed 1,500 kg pig-iron ballast cart mounted on west arm (2,400 kg total beam assembly incl. 900 kg steel frame), 300 kg cage on east arm, 35 kg chock lever, catch (release at 0.15 rad), trip line and chain handle; rider boards cage at Deck 4, pulls trip handle, 22 m governed ride (2.5 m/s, 40 kN) to Deck 6 | built and proven (cycle 3) |
| C2 | Climb: **the east machinery hall & pipe rack** | 66 → 88 m | east side | switchgear enclosure mantle, exhaust manifold duct leap and mantle, wall ladder to Deck 7 equipment hatch, pipe rack girder mantle, 0.35 m monorail balance beam, davit hanging ladder leap, crossover bridge over Deck 8 ShaftRail onto Deck 8 North band | built and proven (cycle 4) |
| S3 | Machine: **brake-override counterweight hoist** (archetype 17) | 88 → 132 m | the shaft, north | overloaded 3,500 kg freight car at Deck 12 held by caliper brake; rider boards 400 kg counterweight carriage at Deck 8, trips lanyard brake handle, 44 m governed ride (≤ 2.6 m/s) to Deck 12 North band | built and proven (cycle 5) |
| C3 | Climb: **the crown trusses & high riser ladder** | 132 → 154 m | shaft and north/south band | stepped incline girder over Deck 12 ShaftRail, atrium ventilation duct mantle (137.2 m), Deck 13 wall ladder (143.0 m), Deck 13 walkway plate to X = -6.0 m, high riser ladder (11 m rungs) clearing Stage A cage, crossover bridge over Deck 14 ShaftRail, steps down to Deck 14 South perimeter runway, walk to Deck 14 landing (InitialSpawn::Deck154) | built and proven (cycle 6) |
| Stack | Ascent: **The Stack continuous ascent** | 0 → 154 m | south & north shaft / face | unbroken continuous physics simulation: S1 → C1 → S2 → C2 → S3 → C3 in one single run without dying, worst tick displacement ≤ 0.15 m | built and proven (166.4 s) |
| — | Backup path (rule 8) | 0 → 221 m | — | a plain stair or ladder way past the stages for a player who does not want to do them | not built |
| — | AS-006 Counterweight Well | 154 → 220 m | shaft | skip lift, derrick boom, debris chute; non-lift climbing spine | built; cycle 7/8 integration target |
| — | AS-007 Wet Isolation | 220 → 340 m | shaft | water, air and hydraulics; non-lift climbing route | built; played through the game's input on touch, pad and keyboard, in one run from grade to TP-340 (cycle 10, §11) |
| — | AS-008 Plate Shop | 340 → 484 m | the well's west half | G, a netted scaffold tower slumping down its shaft; H, a transfer girder tipped by a plate trolley rolling past its fulcrum; I, a domino beam tripping a 20 t monolith whose fall hauls a cage; a ladder to the 484 ring | built; played through the game's input on touch, pad and keyboard, in one run from grade to TP-640 (cycle 10, §11) |
| — | AS-009 Facade Crane Stack | 484 → 640 m | the tower's faces | J, a runaway rail wagon dragging a facade traveler up its rails once its missing rail joint is laid; K, a retired tower crane's jib swinging down like a pendulum; L, a freight cart's run down an incline driving a two-drum winch into TP-640 | built; played through the game's input on touch, pad and keyboard, in one run from grade to TP-640 (cycle 10, §11) |
| — | AS-010 Midstack Service, stage M | 640 → 662 m | over TP-640 | the service lift: a 1.5 t cage hauled by a falling reel of lift cable that sheds its cable's weight to the head beam as it pays out, so its drive fades; held on its dogs by the deck. **No governor** | built; played through the game's input on touch, pad and keyboard, in one run from grade to the 662 deck (§11) |
| C4 | Climb: **the service gantry** | 662 → 684 m | over the 662 deck's east half | a mantle onto a switchgear cabinet, a jump to hang from the duct and up, along a level beam to the pump deck, 7.75 m up the standpipe onto the hoist runway, over its edge into a hang and along its lip past the winch house, a run and a jump over a 3 m gap, a hang up onto the gallery, a mantle onto the riser, a hang up onto the hoist platform, a mantle over the yellow girder onto the 684 deck | built; played through touch from the 662 deck and in one run from grade to the 684 deck (§11) |
| — | AS-010 Midstack Service, stage N | 684 → 706 m | the 684 deck's west half | the granular discharge hoist (the owner's archetype 09): GRAB the flap chain hanging in the cage and it draws a counterweighted flap past its dead point; the flap falls open and an aggregate bin pours 1,213 kg of gravel into a hopper hanging on the other end of the cage's rope. Two-thirds poured, the hopper outweighs the cage and rider and sinks; its 23 m chain sets 33 kg down on the deck for every metre it falls, so the drive fades and turns, and the cage settles onto its dogs at the 706 deck. **No governor, no catch** | built; played through touch from the 684 deck and in one run from grade to the 706 deck (§11) |
| C5 | Climb: **the cooling plant** | 706 → 728 m | over the footprint's north-east, off the 706 deck | east onto a plant floor, a vault over a pipe manifold, a crawl under a duct bank, a mantle onto a tank, up its standpipe, a sprint and a jump over a 6.5 m gap (past any walking jump), a hang up onto a platform and another, up a second standpipe, a mantle over the yellow girder onto the 728 deck | built; played through touch from the 706 deck and in one run from grade to the 728 deck (§11) |
| — | AS-010 Midstack Service, stage O | 728 → 750 m | the 728 deck's east half | the gravel wheel (the owner's archetype 16): GRAB the lanyard hanging in the cab and step back with it: it draws a chock off an 800 kg swinging ram, which strikes a latch; the gate under a bin of 498 kg of gravel falls open and the gravel pours into a bucket hung on the rope over a wheel whose drum is half its rim. The bucket outweighs the cab and rider at two to one and sinks down its well, turning the wheel and hauling the cab up; its 45 m chain sets 16 kg down for every metre it falls, so the drive fades and turns, and the cab settles onto its dogs at the 750 deck. **No governor, no catch** | built; played through touch from the 728 deck and in one run from grade to the 750 deck (§11) |

From the 750 deck the route goes on to about 800 m with two more of the owner's vertical machines (rule 12). **The crown gondola** (`sx.crown_gondola.v1`) east of the deck: a vault over the deck's east parapet onto its lower receiver, onto the cabin hung from its rim, and a 400 kNm motor turns the 28 m, 7 t crown through 178 degrees, carrying the cabin south, up and over to its upper receiver at 778.09 m. **The luffing derrick** (`sx.luffing_derrick.v1`), its mast 41 m north of the crown and its boom pointing south: its lower receiver against the crown's upper one; a 50 kN slider and its 1.2 t ballast's weight draw in the cable that luffs the 32 m boom from 8 to 65 degrees, lifting the hung cradle 24.5 m up and 18 m in, to its upper receiver at 802.64 m, the route's highest point. What stands at the top is the owner's to decide (rule 12). Native `high`: the vault, the crown (26.5 s) and the derrick (35.6 s) to standing at 803.54 m; `touch_high` plays it through the touch controls (74.3 s). Both machines carry their rider sideways: the rider now inherits a kit body's motion underfoot (rule 4; before, a kit body's velocity read as zero and a sideways ride braked its rider toward world-still). The vault off a standing start close to an obstacle now rises clear of its top before crossing; it used to push on the face and catch up its path in a 0.24 m lurch.

Three more machines give the Stack's first bands a second way up, where every player sees them (rule 12; the owner, 2026-10-08: *"build the tram and my two drawings too - you choose best place to build each one"*). **The slab incline** (the owner's drawing; `sx.slab_incline.v1`), on the south face west of S1, in view from the game's start: a ramp from the yard up to its boarding platform (3.4 m), the trolley hauled 30 m up a 60-degree incline by a 2.5 t slab falling 34.6 m in the tower beside its head (the trolley's motor only brakes the ascent and winds the slab back up when sent down), and a plate from the head onto deck 3 (native `incline`: 30.0 s at 1.2 m/s; `touch_incline` from the game's start, 53.8 s). **The stone wheel** (the owner's drawing; `sx.stone_wheel.v1`), in the yard south of S1: a ramp down from deck 2's edge into the bucket at the bottom of a 26 m overshot wheel; RAISE opens the chute-fed hopper's gate, stone pours into the bucket just past the top, and the loaded side turns the wheel (its motor only brakes and holds), carrying the rider over to the top, where each spent bucket has emptied on the striker onto the pile at its foot; a ramp down onto deck 4 (native `wheel`: 47.1 s; `touch_wheel` 58.6 s). **The traction tram** (`sx.traction_tram.v1`) beside the east bridge: off the bridge's open east end onto a landing, down a gangway onto its carrier, whose two motors drive its 4 m wheels up the 35-degree track by their grip on it, 26 m up and 44 m west, and a plate on to deck 8's east edge (native `tram`: 36.7 s; `touch_tram` from deck 6, 66.8 s). The east and west bridges' under-trusses now hang level under their level decks; their line used to fall 3 m along the span, so near the tower their webs stood 1.2 m up through the deck.

The owner's slingshot (2026-09-30: *"a working slingshot (like angry birds) on the ground that can launch the player far up the structure (for dynamic fun) with working stretching of large rubber bands"*) is built as `AS-011` (03_EXECUTION/ASCENT/AS-011_GROUND_SLINGSHOT.md), ported from the `ChatGPT` branch's working machine: from the yard, a 10.2 m draw throws the rider to 254 m and the chute brings them onto the 220 ring, the top of the route from grade.

From the 220 ring the canonical climb goes on with `AS-012`, the swing (03_EXECUTION/ASCENT/AS-012_SWING.md; rule 11, "swung on a hanging mass"): out a gangway off the ring's south face into a seat hung on a 28.8 m arm; strapped in, the rider kicks the trip, and a ram of the seat's weight with the rider aboard, hung back on an arm of the same make, comes down and strikes the seat's buffer, stops nearly dead, and sends the seat up its arc to the 242 ring's edge, where a rack holds it. The rider steps off onto the 242 ring. No ladder, no pull.

From the 242 ring, **C6, the west band** (242 → 264 m; `src/sim/climb_c6.cpp`), a climb with no ladder and no standpipe: a mantle onto kentledge on the band, two concrete ballast blocks; a run along its top and, from its hazard-striped end, a leap north over a 4.75 m gap, the hands catching the yellow lip of a platform 3.5 m above the take-off (leapt from the last 0.7 m of the run, it catches; leapt from further back, or from a stand at the edge, the body falls short, under the platform, onto the band, alive); west along a 0.3 m girder out over the void on the balance, 6.8 m to a landing hung from an outrigger; a hang up onto the outrigger's end and back east along it; north up a 0.4 m girder rising at 24° on the balance, 4.2 m up to a platform; a hang up onto a crossbeam's end and east along it over the well; a hang up onto a block, and from it onto the 264 ring's inner edge. The actions it introduces (rule 9): the running leap caught by the hands, and the walk up a rising girder on the balance. Native `C6`: 26.0 s from the ring to the 264 ring (265.15 m), alive; from the ring a jump under the platform catches nothing, and a standing leap from the kentledge's edge falls short; `touch_c6` plays it through the touch controls. Above the 264 ring the route goes on with the owner's vertical machines (rule 12; `src/sim/vertical/`, imported from the packs as designed, motors included; `vertical_route.cpp` places them). **The cascade mast** (`sx.cascade_mast.v1`), in the well's west half beside the 264 ring's west band: a 110 kN slider raises its 900 kg outer stage 9 m; one rope makes the 750 kg inner stage rise twice that and a second makes the 1,200 kg deck rise three times, 27 m. Its lower receiver and deck are flush with the band (264.25 m); its upper receiver is at 291.25 m. The player's control is the machine's own: on the deck, Action (RAISE / LOWER) sends it to the other end; on a receiver, CALL brings it there. Native `mast`: 27 m in 23.7 s at 1.2 m/s, off onto the upper receiver at 292.15 m, back down, Action refused off the machine, the same ride twice bit for bit, and a death off the deck mid-ride restored onto the deck, which rides on to the top; `touch_mast` plays it through the touch controls (32.9 s from the band to the upper receiver). **The pitman lift** (`sx.pitman_lift.v1`) outside the west face, reached by a walkway west from the mast's upper receiver through the open face: a 600 kNm motor turns its 24 m, 5.2 t crank through 160 degrees; the crank pin drives an 18 m rod whose foot pulls the deck up its guide 23.6 m, slowing at both ends as the crank's geometry gears it down; 291.25 to 314.89 m. **The barrel helix** (`sx.barrel_helix.v1`) west of it, its lower receiver against the pitman's upper one: a 100 kNm motor turns a 12 m barrel whose spiral flight, 24 m to the turn, pushes a free roller on the deck's arm up its guide, 24 m, to 338.89 m; a walkway runs east from there over the pitman's frame and through the face to TP-340's west edge, 1.36 m under the plate's top, and a mantle puts the rider on the plate, where AS-008 begins. So the 264 ring is no longer a dead end: the route from the 220 ring by the swing, C6 and the three machines rejoins the main route at TP-340. Native `pitman` (23.8 s) and `helix` (36.9 s, then the walkway and the mantle onto TP-340); `touch_machines` plays the three through the touch controls from the 264 ring to standing on TP-340 (112 s).

Retired 2026-09-28: `ScraperX-Claude`'s own S2, the swinging stair (44 → 55 m, cycle 3,
`a96b60b`), when the owner had `Gemini`'s world merged into this branch; its contract, evaluator
spec and falsifiers stay in this file's history at `19e04ba` (§6 there).

## 4. Cycle 1 — S1, the water-balance hoist (contract, as built)

| Contract | Content |
|---|---|
| **Purpose and boundary** | Carry a rider from grade to deck 2 (22.0 m) on the tower's south face. Includes the cage, its guide and governor, the bucket and its guide, the rope, the header tank and its valve, the valve's lever, its pawl and the line to the scale plate in the cage's floor, the catch on the same lever, the striker and the bucket's drain, the headframe, the bucket's fence and deck 2's gangway (`src/sim/band_stack.cpp`). Excludes C1 and everything above deck 2 |
| **Output** | The rider standing on deck 2's south band (support: the tower), at rest, within 25 s of stepping onto the plate |
| **Receiver** | Deck 2, south band, top 22.00 m, reached over a fixed gangway from the cage's open north side. Acceptance: grounded on the tower with the body's centre above 22.5 m and north of z = −124.0 |
| **State and rules** | Cage 300 kg on a vertical guide, governor 2.5 m/s brake-only (12 kN), easing into each stop. Bucket 150 kg empty, a bin of up to 1000 kg of water, on its own guide, held at the top by a relatching catch. One rope over two head sheaves (1:1), tension only, exactly long enough for the cage down and the bucket up. Header tank 12 t (a declared pool); its downpipe's valve and the bucket's catch both answer one lever, which turns on the valve's own spindle over the bucket's bay: its 2.4 m arm reaches east over the headframe's middle legs and the cage, and a counterweight 0.6 m west of the spindle holds it up on its stop (150 N·m; the chain's 3 kg handle takes 71 of it, so ≈ 33 N more at the chain turns it). The catch lets go past 0.15 rad; the valve opens from 0.06 rad to full at 0.24 rad (Q = 0.6 A f √(2 g Δh), A 0.03 m², ≈ 165 kg/s full open). A line runs from a crank 0.38 m out along the arm, down over a sheave under the lever, east under the head and down through the cage's open roof to an eye on the scale plate (1.2 m square, 15 kg, yellow) that hangs on it 0.1 m proud of the cage's floor on its own vertical guide: weighed down onto the floor, the plate draws 0.1 m of line, which turns the lever 0.26 rad; its own weight takes 56 of the 150 N·m and does not. A pawl (declared, drawn) drops behind the lever at 0.25 rad and holds it there until the bucket, leaving the head, knocks it out 0.15 m down. At the foot of its guide the bucket lands on a striker that opens its drain (25 kg/s) |
| **Input** | None to press: the rider's weight on the plate (rule 11, no pull). Standing elsewhere on the cage's floor weighs nothing on the line |
| **Capacity** | The rider needs more than 235 kg of water: the cage leaves the yard at 278 kg, 1.6 s after the pawl drops. The bucket knocks the pawl out leaving the head and the valve shuts within the cage's first 0.41 m, at 411 kg; braked to ≤ 2.5 m/s, 11.7 s from the pawl to the top. Drained below 235 kg the cage brings a rider who stays aboard back down; empty, it starts down below 150 kg. A ride takes ≈ 170–410 kg of the tank's 12 t |
| **Use and recovery** | Found from the approach: the headframe with its tank is the tallest thing at the tower's foot; a long hazard-striped lever runs from the tank's valve out over the cage, its rust counterweight over the bucket, and a line drops from it through the cage's roof to the yellow plate in the middle of its floor. Left alone, S1 waits as found. In the cage at grade the HUD reads STAND ON · THE SCALE PLATE; on it, FILLING · BUCKET n KG counts the water in. It does not wait: once the pawl has dropped, a rider who steps off again sees the water send the empty cage up, and it comes back down by itself (30 s). Nothing strands: at the foot the bucket drains, and once lighter than the cage it rises, the cage comes down, the line lifts the plate proud of the floor again, and the catch seats the bucket at the top, as found. A rider who stays on the plate is brought down and carried up again. A lethal fall restores the committed state, machine and pawl included |
| **Proof** (native, `run_stack` in `tests/simulation_tests.cpp`) | left alone 30 s: the lever on its stop, the valve shut and unpawled, the bucket dry, the plate proud, the cage at grade; (a) stood in the cage off the plate for 5 s: nothing moves, no water; stepped onto the plate and straight off again: the pawl holds the valve, the water sends the empty cage up, and it comes back down by itself and waits as found (29.9 s); (b) from the game's spawn on player inputs, onto the plate → lift-off at 278 kg (balance 235), valve shut at 0.41 m of travel, no water after, cage floor 22.05 m, peak 2.51 m/s, rider on the plate on the cage throughout, the payload's energy gain never above the bucket's release (worst −1.5 J, inside the stance tolerance); (d) the rider walks off over the gangway onto deck 2 (support: the tower); recovery: the empty cage starts down at 145 kg (< 150), the catch relatches, the plate hangs proud again; (c) a rider who stays on the plate is brought back down at 231 kg (< 235) and carried up a second time by their weight alone (168 kg of water); the tank holds water for many more rides |
| **Proof** (the game's input path, `ui_test_driver.gd` `touch_stack`, `pad_stack`, `keyboard_stack`) | From the game's own start at grade, events injected through the viewport on each device: walk into the cage onto its plate, the native pawl drops and the catch opens, the HUD reads FILLING · BUCKET n KG past 120 kg, the cage rises and reaches the top, walk off onto deck 2 and climb C1 to deck 4 and on. The body never moves more than 0.25 m sideways in a frame; no deaths. CI builds the playtest APK only after these pass |

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

**Repair, cycle 1c: rule 11, no pull** (2026-10-07). The owner (2026-10-06): *"There's lifts that
need a pull of something … So boring."* The chain and its handle are gone. The rider's weight on a
scale plate in the cage's floor works the same lever through a line; the catch, the valve and the
water balance are unchanged. A plate under the whole cage was weighed first and dropped: with the
bucket fast on its catch the rope leaves the cage nowhere to sink, and once the bucket hangs on
the rope its weight lightens the cage and the scale lets go of the valve before the bucket has
its water. Weighing the rider alone needs a pawl to hold the valve open until the bucket goes:
declared in the kit (`add_latch`: a lever held past an angle until a guided body passes a
travel), drawn on the lever, saved and restored with the checkpoint.

**Status.** Specified and implemented; verified natively on player inputs from the game's spawn
through the real receiver (deck 2) and the machine's return, and through the game's own input
pipeline on touch, pad and keyboard from the game's start. Not yet verified on the owner's device
(the next APK).

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

## 6. Cycle 3 — S2, the walking beam hoist with fixed ballast cart (contract, as built)

| Contract | Content |
|---|---|
| **Purpose and boundary** | Carry a rider from Deck 4's south band (44.0 m) to Deck 6 (66.0 m) inside the central shaft (`build_s2` in `src/sim/band_stack.cpp`). Mutated from Archetype 18 (Cantilever Beam Tip): instead of a separate rolling body along an open beam track (which introduces multi-body slip and collision hazards during physics ticks), the 1,500 kg pig-iron ballast cart is mounted directly on the outer west arm of the walking beam girder (centered at t = 0.15, R ~ 12 m from fulcrum), forming a single 2,400 kg rigid beam body (with 900 kg lattice frame) on a central trunnion hinge axle at +55 m (`kS2Pivot`). Includes the 300 kg cage on a vertical guide with 2.5 m/s brake-only governor (40 kN), headframe sheaves, 1:1 tow rope, 35 kg chock lever, catch (release at 0.15 rad), trip line, and chain handle |
| **Output** | The rider standing on Deck 6's south band (support: the tower) within 15 s of pulling the trip handle |
| **Receiver** | Deck 6, south band, top 66.00 m. Acceptance: grounded on the tower with the body's centre above 66.5 m and support `Simulation::kTowerEntityId` |
| **State and rules** | Found at rest: cage latched at Deck 4 (44.05 m), beam horizontal at rest angle = 0, chock lever latched holding beam. Rider boards cage, pulls chain handle hanging at eye height; trip line rotates chock lever past 0.15 rad release; catch drops, beam begins rotation under gravitational torque of the fixed 1,500 kg ballast cart, sinking the west arm 22 m and towing cage up 22 m to Deck 6. Safety dogs engage at top stop (66.05 m) |
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

**Result, cycle 10 (as built, 2026-09-28).** Native: `PASS scraperx_sim Mega-Ascent Grade to TP-340` (one simulation, 339 s, no deaths, no body step over 0.15 m a tick). Through the game's input: `touch_stack`, `pad_stack`, `keyboard_stack` from the game's start to standing on TP-340 (341.15 m, ~350 s each); `touch_wet`, `pad_wet`, `keyboard_wet` from the 220 ring start. All block the APK. Played through the input path, AS-007 exposed five defects the native tests on scripted inputs had not:

| Defect | Invalid state | Transition that allowed it | Change that makes it unreachable |
|---|---|---|---|
| F relatched on a prompt LET GO (pad) | the 20 t accumulator drawn back up onto its seat, and the platform and rider back down, within 0.75 s of the trip | a relatching catch took any slow body inside its window, including one already leaving its seat | a catch takes back only a body at rest or returning to its seat (`mechanism_kit.cpp`); falsified by `pad_wet` |
| E's door yanked open on GRAB | the 60 kg leaf swung at 6 m/s; the hands jumped 0.100 m in a frame | the carry's point constraint closed any hand-to-handle gap in one tick | the hands close on the handle where it is and draw it in at 3 m/s (`kCarryPullInSpeed`); falsifier `handling` (0.23 m in a tick snapped, 0.05 m drawn) |
| D's spool thrown into the shaft (keyboard) | a 50 kg load carried at 5.5 m/s and stopped at 22 m/s^2: 1030 N at the hands, over the 900 N grip | carrying set no limit on speed or braking | a free load caps its carrier's speed (1 - weight / 0.8 grip) and acceleration within 80% of the grip: 1.76 m/s and 5.3 m/s^2 for the spool; falsifier `handling` |
| D's spool seated or not by luck | the spool reached its gap up to 27 degrees off (the gap takes 20) | a load hung from one point spun freely in the hands | both hands hold a free load's turn as taken, with no more torque than the grip gives at its flanks |
| The 66, 88 and 132 m START options loaded grade | the bridge refused spawns 27-29 | its spawn bound was the last enumerator at the time (Deck4South) | an enum sentinel, `InitialSpawn::Count`; falsified by `touch_pause`, which loads every START option |

Also through the input path, the ledge-catch snap (cycle 6's C2 duct, keyboard): a hang caught at arm's length now pulls the body in at 3 m/s (`kHangPullInSpeed`); falsified natively (0.70 m in a tick snapped, 0.033 m pulled). **Open for the owner:** AS-007's lifts are a float (D), compressed air (E) and a hydraulic ram (F); the owner's catalogue lists these archetypes (02, 05, 08), while the `MACRO-TRAVERSAL-STRICT` profile this plan cites (its header) bans fluid drives and buoyancy on the primary path. They stay until the owner decides.

**Result, cycle 10 continued: AS-008 (as built, 2026-09-29).** Through the game's input: `touch_shop`, `pad_shop`, `keyboard_shop` from the TP-340 start (G's rope unhooked from its cleat, carried onto its platform and hooked on the eye, its prop pin drawn by the lanyard; the girder's tail pin carried out, over the gangway onto H, its chock yanked; across onto I's cage, its shackle hooked on, the domino's pin drawn; up the ladder) to standing on the 484 ring (485.15 m, ~123 s each). AS-009 then went the same way: `touch_crane`, `pad_crane`, `keyboard_crane` from the 484 ring start (J's rail joint carried onto its traveler and laid in the cradle, the wagon's chock pulled; round the 528 ring to K's cage, its shackle hooked on, the jib's pendant pin drawn; round the 572 ring into L's cab, the winch's clutch thrown in, the drop weight's pin drawn) to standing on TP-640 (641.15 m, ~109 s each), with nothing new to repair; and `touch_stack`, `pad_stack`, `keyboard_stack` now go from the game's start to standing on TP-640 in one run (~585 s each, no deaths). All block the APK. The input path exposed one defect the native tests could not see, because it is in the view:

| Defect | Invalid state | Transition that allowed it | Change that makes it unreachable |
|---|---|---|---|
| The view popped on every step up (seen stepping onto G's platform with its rope in hand) | the eye rose 0.17 m in one frame; the hands, held on the rope's shackle, jumped 0.166 m across the view | a native step lifts the body up to 0.36 m in one tick, as Jolt's character walks stairs, and the eye was placed at the body | the native counts the lift (`step_up_meters`), the render pose takes a step tick whole, and the eye takes the lift as a lag it closes at 14/s, never slower than 1 m/s (`main.gd`); falsified by the stack and shop runs' view-lift bound, 0.10 m beyond the body's own motion in a frame (0.198 m with the ease removed, at most 0.071 m with it) |

The hands are watched on AS-008's ropes, pins and lanyards, as on AS-007's machines and S1's chain; the ladder's top-out is not, because a mantle's reach to the lip is uncapped by design (`first_person_arms.gd`: the palm must land before the body rises past it) and moves a hand about 7 m/s.

**Result, AS-010 stage M (as built, 2026-09-29).** The first stage above TP-640, from the owner's archetype 11 (the spool unwind), and the first lift on the route with no governor: a 1.5 t service cage on TP-640, hooked on and let go from inside, hauled 22 m by a reel of lift cable falling beside it, the reel shedding its cable's weight to the head beam as it pays out, so the drive fades and turns; the cage tops out past the 662 deck and settles onto its dogs. Evaluator INTEGRATED over the friction band (contract `AS-010_MIDSTACK_SERVICE.md`); in the engine, apex 21.85 m, held 21.80 m, peak 2.94 m/s, 12.2 s, 339.0 kJ gained of 343.3 kJ released. Through the game's input: `touch_service`, `pad_service`, `keyboard_service` from the TP-640 start, and the stack runs from the game's start to the 662 deck (~606 s). The input path found two faults: a knife-edge arrival (0.3 m dog teeth held the keyboard rider's cage a tooth low, 0.4 m under the deck; now 0.1 m teeth and a deck 0.15 m lower) and, exposed by the new bodies' shift of Jolt's ordering, AS-009 J's seat reaching 0.25 m where its tray reaches 0.28 m (now 0.30 m). The full native suite found two faults below it: Jolt's pulley constraint read a static body's lever arms without ever writing them, so a rope made fast to a static body could tear its load to NaN when it went taut, as the allocator's reused memory decided (AS-008 G's tower, in some full-suite runs only; patched at configure time, `third_party/jolt_patch.cmake`, proven by `pulley seam`), and the rider's support pick among equal contacts followed the order Jolt's worker threads delivered them (a total order now; six full suites side by side give byte-identical logs).

**Result, AS-010 C4 (as built, 2026-09-30).** The first designed climb above 154 m: from 154 m to the 662 deck the route had been thirteen machines in a row, where the owner asked for mechanisms stacked and then a climb (2026-09-25). C4, the service gantry, is one static body over the 662 deck's east half, ten moves from the 662 deck to a new 684 deck, each well inside the movement envelope measured from `simulation.cpp` (mantles 1.55 m of 1.85, hangs caught 3.2 m of 3.76, a 3 m gap of 6.17): a mantle, a hang, a level beam, the standpipe, a drop into a hang and a shimmy past a winch house, a gap jump, a hang, a mantle, a hang, a mantle. Contract and move table: `AS-010_MIDSTACK_SERVICE.md`. Native: `AS-010 C4` from the 662 deck (34.5 s, no death, worst body step 0.085 m a tick, shimmy 3.42 m; the winch house catches nothing jumped at, and on foot nothing gets past it: an adversarial review found a first house, a 0.3 m strip short of the lip, let a walker ride the lip's edge round it, and the check that now fails on that house passes on this one; the same review found the runway's and the pump deck's outer edges 0.2–0.5 m inside the tower's faces, so a step off them fell down the stack, and 0.8 m parapets there now stop a walk into them, each proven by taking it away), `AS-010 band` from TP-640 through M and C4 (57.7 s), `AS-010 route` up both backup ladders (54.5 s). Through touch: `touch_c4` (36.5 s) and `touch_stack` from the game's start to standing on the 684 deck (643.3 s, no death), the first input-path proof of the beam, of DROP into a hang and of the shimmy. Other lines the review found work in the engine and are kept as the player's: a diagonal jump off the runway east of the winch house to the landing (the hang is one way past the house, not the only one), a sprint off the runway straight into the gallery's hang, a jump from the duct over the beam's gap. The pad and keyboard runs end on the 662 deck (the owner, 2026-09-24: no keyboard or gamepad needed). The pause menu's START list gains the 640 m plate and the 662 m deck.

**Result, AS-010 N (as built, 2026-09-30).** The next machine after the C4 climb, derived as M was: the owner's archetype 09, the granular discharge hoist, the catalogue entry the kit's bins already model. A counterweighted flap under an aggregate bin, thrown past its dead point by a chain in the cage; the bin pours into a hopper hanging on the cage's rope, and a 23 m chain from the hopper's floor sets its weight down on the deck as the hopper sinks, so the drive fades and turns: no catch, no governor. Evaluator INTEGRATED over the friction band (held 21.6 to 22.2 m, peak 3.10 m/s); in the engine apex 22.12 m, held 22.10 m, peak 3.07 m/s, 452.0 kJ gained of 457.0 kJ released; with the chain taken out and the same start, the cage runs into its stop. The kit gained `add_chain`, and a latent fault went with it: a bin and a reel each set their body's whole mass, overwriting the other; one function now sets it. Contract, spec and falsifiers: `AS-010_MIDSTACK_SERVICE.md`. Native `AS-010 N`, `band` from TP-640 to the 706 deck (77.4 s), `route` up three ladders (79.4 s); through touch `touch_n` (21.7 s) and `touch_stack` from the game's start to standing on the 706 deck (665.0 s, no death). The pause menu's START list gains the 684 m deck.

**Result, AS-010 C5 (as built, 2026-09-30).** A climb again after N, as the owner laid the route out: the cooling plant, one static body and a pipe manifold of its own over the footprint's north-east, ten moves from the 706 deck to a new 728 deck, and the first climb on the route to need a vault, a crawl and a sprint: over the manifold, under a duct bank 1.45 m clear (a standing walker stops at it), a mantle onto a tank, 6.1 m up its standpipe, a sprint and a jump over a 6.5 m gap onto a slab too thin to hang from (a walking jump falls short, onto the plant floor, alive), two hangs of 3.2 m, 5.9 m up a second standpipe, a mantle over the 728 deck's girder. Contract and move table: `AS-010_MIDSTACK_SERVICE.md`. Native `AS-010 C5` from the 706 deck (27.4 s, no death, gap at 8.0 m/s, worst body step 0.128 m a tick), `band` from TP-640 to the 728 deck (106.2 s), `route` up four ladders (105.0 s); through touch `touch_c5` (30.5 s, the first input-path sprint) and `touch_stack` from the game's start to standing on the 728 deck (696.6 s, no death). Probes of C5's edges before it was committed found its line sound and its edges not: a slow step off the top platform where it jutted past the one below fell 20.4 m at up to 19.97 m/s and killed 2 times in 9, a step off the first platform's west edge slid into the 706 deck's hatch and killed every time, and two platform edges ran out under the next one up over the void; each is closed now and checked. DEFECT, pushed with N: the third ladder came up into a pocket behind the 706 deck's hatch, cut off from the deck by strips narrower than a body; it comes up on the open side now. The pause menu's START list gains the 706 m deck.

**Result, AS-010 O (as built, 2026-09-30).** A machine again after C5: the owner's archetype 16 as its trace lays it out, but for the gravel falling into a bucket on the wheel's rope rather than onto blades on its rim, and a swinging ram the rider frees for the rolling pipe. On the 728 deck a 2.4 t cab; over it a wheel of 2.0 m rim and 1.0 m drum carrying the rope between the cab and a bucket that hangs empty at the top of a well down to the plant floor, a 45 m chain of 16 kg/m hanging from it; over the well a bin of 498 kg of gravel behind a latched gate, and an 800 kg ram held back by a chock. GRAB the lanyard in the cab and step back: the chock lets the ram go, it trips the latch, the gate falls open, the gravel pours, the bucket sinks and the cab rises 22.1 m to the 750 deck, its dogs holding it. Contract, models and evaluator: `AS-010_MIDSTACK_SERVICE.md` (INTEGRATED over 85–185 N of shoe friction and the wheel's inertia ±10 %: held 21.9–22.2 m, peak 2.54–2.66 m/s, breakaway 26.3; the engine's 22.10 m and 2.60 m/s). Native `AS-010 O` (apex 22.19 m, held 22.10 m, 20.2 s, gain 538.7 kJ of 548.6 kJ released, the wheel within 0.023 rad of the cab's rise, the causes in order), `band` from TP-640 to the 750 deck (131.6 s), `route` up five ladders (132.9 s); through touch `touch_o` (26.6 s) and `touch_stack` from the game's start to standing on the 750 deck (723.2 s, no death). Falsified: the chain swapped for a fixed weight of the same starting mass runs the cab into its stop at about 4 m/s and throws its rider; without the well's rails or the 750 deck's parapets the edge checks fail. The kit gains `add_wheel` (a hinge and a rack-and-pinion tying a wheel's turn to a guide's travel, so its inertia is real) and `set_wound` (a rope drawn round a wheel). The pause menu's START list gains the 728 m deck.

**Open for the owner: the governors.** Every lift below TP-640 — S1, S2, S3, A, B, C, D, E, F, G, H, I, J, K, L, and J's wagon and L's cart — rides a kit guide whose governor (`Kit::add_guide`, a brake-only velocity motor of 9 to 60 kN) holds it to 2 to 3 m/s and ramps it to a stop at the end of its travel. So their arrivals are the governor's, not their drives': §2.3's "surplus energy is the arrival speed; every drive is shaped and every terminal is a catch" holds for none of them. They all work through the game's input; whether they must also work without the governor (re-derived as stage M is), or keep it as a visible brake, is the owner's call. Stage M's damping note applies to them too: Jolt's default 0.05 s⁻¹ damping (some 400 N on a tonne at 3 m/s) is on every kit body, where the governors mask it.

---

## 12. Cycle 11 — Industrial Athletic Traversal Layer & Parkour System Expansion

### Objective & Philosophy
Transform the mechanical environment into an intentional, collision-honest athletic climbing encounter. Every obstacle is physically grounded in industrial plausibility (cross-bracing, structural I-beams, broken catwalks, maintenance platforms, machinery housings, pipe runs, recovery cradles) and deliberately exercises ScraperX's core movement mechanics (jumping, vaulting, hanging, lateral shimmying, pulling up, support transfers, narrow-beam balancing, and fall recovery).

### Traversal Geometry Specifications
1. **West Yard Transformer & Compressor Station (Elevation 0.90 → 22.00 m):**
   - 0.30 m containment curb step-up (satisfying `kStepMaximumHeight = 0.35m`).
   - 1.10 m rise transformer casing (vaultable) and compressor skid (mantleable).
   - Elevated cable tray gantry (y = 3.95 m) and S1 West Service Tower bridging to Deck 2 south-west band.
2. **Deck 2 Overhead Crane Runway & Broken Catwalk (Elevation 22.00 → 24.25 m):**
   - High-voltage transformer blast barrier (2.20 m) and capacitor banks.
   - 5-step access stair (rise 0.27 m per step) ascending from y = 22.0 to 23.35 m.
   - 0.40 m wide overhead crane runway girder cantilevered across the machinery bay triggering `player_balancing`.
   - Broken catwalk with 2.20 m jump gap and angle-iron catch lip.
3. **C1 Suspended Maintenance Recovery Cradle (Elevation 30.50 → 33.95 m):**
   - Suspended 5x5 m steel grating platform under the C1 davit leap at y = 30.50 m (absorbs 2.8 m survivable falls).
   - Perimeter toe boards, corner suspension hangers, and vertical maintenance ladder mounted to Deck 3 South fascia.
   - Flared walk-through grab handles (0.90 m span) allowing smooth mantle top-out onto Deck 3 floor plate.
   - Alternative exterior diagrid route with scaffold platform and stepped transfer beam.
4. **Deck 4 Steam Receiver Skid & Runway Girder (Elevation 44.00 → 46.25 m):**
   - Steam receiver and separator vessels east of the S2 cage corridor.
   - 5-step access stair ascending from deck floor (44.0 m) to runway girder (45.35 m).
   - 0.40 m wide balance runway girder spanning north across open machinery pit.
5. **Deck 6 South Band Traversal (Elevation 66.00 → 67.55 m):**
   - Catwalk and machinery housing relocated strictly south of Z = -128.5 m to preserve 100% collision-free sweep clearance for the 14-meter S2 walking beam at Z = -141.0 m.
   - Unobstructed 3-meter walking corridor along Z = -125.5 m for C2 ascent.
6. **C2 Suspended Maintenance Recovery Cradle (Elevation 78.50 → 82.90 m):**
   - Suspended safety recovery cradle under the C2 davit leap with safety guardrails and recovery ladder.

### Visceral Feel, Haptics & Humorous Vocalizations
- **Haptic Tactile Profiles:** Custom vibration profiles for `vault` (24ms / 0.65 amp), `mantle` (42ms / 0.75 amp), and `balance` (18ms / 0.30 amp).
- **Athletic Camera Feel:** Dynamic speed vault tuck dip (-0.07m) and apex roll bank (+0.035 rad), muscular mantle heave with hand-plant compression, tightrope balance micro-sway, and +4.5° dynamic FOV rush surge.
- **Fear-Based Screaming & Profanity:** Procedural formant-modeled terror yells (`_fall_yell`) and profane expletives triggered when falling at speed (>11 m/s) or high danger (`FALL_SWEARS`), canopy deployment relief quips (`CHUTE_RELIEF`), and dry recovery quips on lethal checkpoint restore (`LETHAL_RESTORE_QUIPS`).

### Verification & Falsification
- `scraperx_sim_tests` asserts:
  - `PASS scraperx_sim yard athletic approach: on_curb_y=1.2`
  - `PASS scraperx_sim deck 2 crane runway: y=24.2495 balancing=1 crossed_x=12.363`
  - `PASS scraperx_sim C1 recovery cradle: fell 2.8m safe landed_y=31.3664 recovered_deck3_y=33.9464`
  - `PASS scraperx_sim deck 4 runway girder: y=46.25 balancing=1`
- `scraperx_parkour_flow_tests`: 100% passing (`PASS scraperx_sim parkour flow`).
- `ctest --test-dir build/host`: 100% tests passed (0 failures out of 2).
- Full continuous Mega-Ascent Grade to 440m proven with zero regressions.

## 13. Audit of the `ChatGPT` branch's opening mechanism

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
a proven exit, and its physics corrections to the Colossus sequence (§14). Its ordinary stairs as a
fallback were not adopted while the owner's direction was that no stair is a route; the owner has
since made easy paths the backup route (§2.6 rule 8), so a fallback is adopted in that role and is
not yet built (§3). Not adopted: removing the legacy bands from the default world (§3, the owner's
decision).

## 14. The catalogue against this engine

What the kit builds today (source: `mechanism_kit.hpp`): bodies made of boxes; world-fixed hinges
between hard stops, frictionless; straight guides with a brake-only
governor that eases into each stop, dogs (a catch rack on a guide) and rail gaps; catches released
by a lever or by pulling a pin, relatching; ropes over fixed sheaves (any ratio; a rating that
parts; a clutch; a push-only strut); trip lines to handles; bins that pour rubble or water through
a gated mouth and drain on a striker; pools and pipes with valves; rigging (shackles, anchors);
carrying. Missing: round bodies, joints between two moving bodies, springs, crush beds, fracture.

| Family | Build now? | Missing | Notes (the `ChatGPT` corrections kept) |
|---|---|---|---|
| Counterweight lift, fixed or variable ballast | yes | — | S1. Unshaped: keep strokes short or near balance, or shape it |
| Chain counterweight (drive fades as chain piles) | no | a rope end whose hanging mass falls with travel | the skill's worked example; the best shaped lift drive |
| Bascule, swinging stair, drawbridge | partly | a density per box, friction over a range of a hinge's angles | built once on this branch as the swinging stair, and retired with it (`19e04ba`, §6 there), kit capabilities included. Offset tail places the drive's zero; a trip lever, not a gravity hook, as its catch |
| Balance bridge (beam + ballast) | partly | pan hinge on the beam, rolling bodies, catch rack on a lever | load it by a gated rubble pour, not a pipe avalanche (§13) |
| Pendulum striking a receiver | yes, `AS-012` | built: the ram strikes the rider's seat, both hung on arms, through a declared rubber buffer (a spring that only pushes, with its loss) that the swing's own owner applies; the rack is the seat's hinge limit stepping up tooth by tooth | strike before the apex, where there is speed; restitution is a separation ratio, not an energy source; equal weights pass the swing on and the striker stops |
| Toppling column, falling monoliths laying a stair | yes (boxes tipping on contact) | — | each upright slab is its own preloaded source; spacing follows the tip geometry; each rest pose must carry the player |
| Tipping platform | yes | — | tips when the combined centre of mass passes the edge |
| Gravity sled on a chute | yes (guide) | — | friction band spreads the apex by metres over long chutes: short paths, catch racks |
| Block and tackle | yes (rope ratio) | — | force times distance is conserved |
| Rolling roller, drum, spool, pipes | no | round bodies, a drum-wrap model | a drum's rotational energy counts; keep the cable engaged, never a snapped chain |
| Spring plunger, scissor jack | no | springs | a pantograph adds no energy; derive preload, force against extension, recharge |
| Slingshot: large rubber bands drawn by the player, the player launched up the structure (the owner, 2026-09-30) | yes, `AS-011` | built: tension-only band forces from the pouch's own position, a ratcheted draw, an aimed launch rail, the rider harnessed; landing by the chute (no caught landing yet) | the bands are the source and store what the player's draw put in: energy is force over draw, never a scripted speed; the launch must arrive under the lethal 20 m/s (or into a net or bed) at a catch on the structure, with a falsifier for a draw too short to reach it and one too long to survive |
| Newton's cradle | no, and not wanted | spheres | a rigid-body solver does not carry a compression wave; one pendulum does the same job |
| High striker | yes | — | 100 kg up 100 m needs ≥ 98.1 kJ and 44.3 m/s at the launch; strike below the apex |
