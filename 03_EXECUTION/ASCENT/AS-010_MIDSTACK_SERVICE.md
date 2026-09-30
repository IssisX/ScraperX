# SCRAPERX — AS-010 MIDSTACK SERVICE (B06, 640 → 780 m)

**Ascent Slice:** `AS-010`
**Lifecycle:** `IN PROGRESS` — stage M, the service lift from TP-640 to the 662 deck; C4, the
service gantry climbed from the 662 deck to the 684 deck; stage N, the granular discharge hoist
from the 684 deck to the 706 deck; C5, the cooling plant climbed from the 706 deck to the 728 deck;
and stage O, the gravel wheel from the 728 deck to the 750 deck, built and played through the
game's input; everything above the 750 deck is unbuilt
**Provenance:** derived here under `03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md` and the Atlas's
B06 (`01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md`: `MOD-SERVICE-LIFT`, a second cage), from the owner's
archetypes 11, the industrial spool unwind (M), 09, the granular discharge hoist (N), and 16, the
gravel hopper waterwheel (O) (`Mechanism-Ideas-and-archetypes.md`).
**Profile:** `MACRO-TRAVERSAL-STRICT`, the plan's. Unlike every lift below it, stage M has no
governor: its arrival is its own drive's.
**Implementation gate:** this write-up. The plan's rules §3 are this slice's test contract.
**Depends on:** `AS-009` in source and green (the rider stands on TP-640).

## Objective

The first stage of the band above TP-640, and the first lift on the route whose speed and stop are
the machine's own. Only this stage is designed (plan §1: only the next stage from a proven exit);
the rest of B06 is an option.

## Existing truth (`b35f1ed`)

| Datum | Value |
|---|---|
| TP-640 | 24 m square about (0, −150), top 640.25; L's cab hole at (−3.0, −160.1); the route's hatch at x 4.1–5.7, z −152.4 to −150.25 |
| AS-009's exit | the rider off L's cab at (−3.0, 641.15, −156.5) |
| Above 640 m | nothing but the neighbouring shaft's mass (`TowerMass`), 140 m north. This slice builds corner columns and the 662 deck over TP-640's footprint |

## Stage M — the service lift on the cable reel (640.45 → 662.15)

- **Found:** a 1.5 t service cage standing on TP-640 at (−8.0, −143.5), railed north and south, on
  a guide with safety dogs every 0.1 m and shoes of 100 N friction, no governor. Beside it, in its
  own shaft through the 662 deck, a reel of lift cable — drum and yoke 1,339.5 kg, 23 m of cable at
  22 kg/m wound on it — held at the top by a chock, its cable's end made fast to the head beam over
  it. The hoist rope runs from the reel's hub over its sheave, across the head to the sheave over the
  cage, down to a shackle hanging free over the cage's middle.
- **Link:** hook the shackle on the cage's eye.
- **Set off:** from the cage, pull the chock's lanyard over the north rail. The reel drops down its
  shaft; every metre it falls pays out a metre of cable whose weight leaves it for the head beam, so
  the drive fades as it goes and turns before the top. The cage rises, slows, tops out a little past
  the deck and settles back onto its dogs; the rider steps off onto the 662 deck.
- **No link:** the reel falls its shaft paying out its cable and lies over its crib; the cage stays.
  The stage is spent; the climbing route remains.

## Declared models

- **Cable reel** (`Kit::add_reel`). The reel weighs its drum and yoke and the cable still wound on
  it; a metre paid out leaves it for the anchor and stays paid out (a reel drawn back up slackens its
  cable). The drum's spin is drawn, not simulated.
- **No air drag** on the cage and the reel: at 3 m/s it is some 15 N on the cage, under the friction
  band's resolution. Jolt's default damping (0.05 s⁻¹, some 400 N here) is set to zero on both.
- **Safety dogs** as the kit's: a pawl in a rack tooth every 0.1 m of the cage's travel.

## Evaluator (stage1dof.py, INTEGRATED)

h = 1 ms, every case rerun at h/4. The friction band is centred on the resistance the engine
shows: at 100 N of shoe friction the engine's cage tops out 0.3 m below the evaluator's, an
unmodelled 1.5 kJ (0.45 % of the reel's release); 135 N in the evaluator reproduces it.

```json
{
 "name": "AS-010 M service lift on the cable reel, TP-640 to the 662 deck",
 "g": 9.81,
 "model_kind": "counterweight",
 "model": {"block_mass": 1339.5, "chain_density": 22.0, "chain_hang0": 23.0, "cage_mass": 1500.0,
           "rider_mass": 85.0, "n_falls": 1, "sheave_ieq": 0, "friction_kinetic": 135,
           "friction_static": 280},
 "terminal": {"catch_q": 21.1, "catch_pitch": 0.1, "stop_q": 22.9, "catch_pad_stroke": 0.05},
 "band": {"model.friction_kinetic": [85, 135, 185]},
 "require": {"allowed_outcomes": ["CAPTURED_FALLBACK"], "min_breakaway_ratio": 1.5,
             "max_peak_accel_g": 0.3, "max_catch_pad_accel_g": 0.5, "max_stop_impact_speed": 0.0,
             "max_peak_speed": 3.5, "max_time_s": 25,
             "convergence": {"catch_speed_up": {"abs": 0.01}, "apex_q": {"abs": 0.01},
                             "fallback_speed": {"abs": 0.01}}},
 "rest_variants": [{"name": "rider_exits", "set": {"model.rider_mass": 0}}]
}
```

| Case | Outcome | Apex | Held on dogs | Peak | Time | Pad | Rider gone |
|---|---|---|---|---|---|---|---|
| 85 N | CAPTURED_FALLBACK | 22.32 m | 22.3 m | 2.91 m/s | 12.3 s | 0.03 g | held |
| 135 N | CAPTURED_FALLBACK | 21.88 m | 21.8 m | 2.85 m/s | 12.6 s | 0.11 g | held |
| 185 N | CAPTURED_FALLBACK | 21.44 m | 21.4 m | 2.79 m/s | 12.4 s | 0.05 g | held |

Breakaway 9.1; peak acceleration 0.08 g; ledger residual under 10⁻⁴. With the deck at 662.1 m, the
cage's floor stands between 0.25 m below and 0.65 m above it across the band: the rider steps off
either way.

## C4 — the service gantry, a climb (662.1 → 684.1)

After stage M, a climb, as the owner asked of the route (2026-09-25: *"a couple of mechanisms
can be stacked, and then a climb, and then another mechanism"*): from 154 m to the 662 deck the
route had been machines only. C4 is one static body (`kServiceC4EntityId`, 1014) over the
footprint's east half, climbed on the player's movement and nothing else, to the **684 deck**
(frame 1012: x −12 to −2 over M, top 684.1, a yellow edge girder at x −2, and a hatch for the
backup ladder).

| # | From → to (top, m) | Move | Envelope (`simulation.cpp`) |
|---|---|---|---|
| 1 | 662 deck → switchgear cabinet (663.65) | mantle 1.55 m | 0.9–1.85 m |
| 2 | cabinet → duct (666.85) | jump from its back edge, hang on a 1.35 m face, climb up: 3.2 m | lip ≤ 3.76 m above take-off |
| 3 | duct → pump deck (666.85) | east along the duct, over a level beam 0.4 m wide, 4.5 m long | beam ≤ 0.50 m wide, ≥ 1.50 m long |
| 4 | pump deck → hoist runway (674.6) | the standpipe, 7.75 m hand over hand, over the top | hold ≤ 0.18 m section |
| 5 | runway east → runway west | the winch house (4.2 m, past any jump's 3.76 m) fills the runway, its face flush with the +z lip from 0.35 m up over a plinth set 0.2 m back: on foot a body would have to stand 0.35 m out beyond the lip, where the edge no longer holds it. Back to the +z edge, DROP into a hang, along the lip under the overhang 3.4 m, climb up | drop ≥ 1.5 m, lip flush; shimmy 0.6 m/s; lip probe 0.25 m up, 0.12 m in |
| 6 | runway → landing (674.6) | a run along +z and a jump over a 3 m gap | 6.17 m walking, 8.98 m sprinting |
| 7 | landing → gallery (677.8) | jump, hang, climb up: 3.2 m | as 2 |
| 8 | gallery → riser (679.35) | mantle 1.55 m | as 1 |
| 9 | riser → hoist platform (682.55) | jump, hang, climb up: 3.2 m | as 2 |
| 10 | hoist platform → 684 deck (684.1) | mantle 1.55 m over the yellow girder | as 1 |

A faced ledge and its top are one body, as the ledge probe needs; every column is 0.4 m, thicker
than a hold, so nothing but the standpipe is climbed hand over hand. Where the duct, the pump deck
and the runway come within 1 m of the tower's faces, a parapet 0.8 m high and 0.25 m thick (under
the 0.9 m the ledge probe starts at, over a step, thicker than a hold) stops a body walking into it
from going off past the faces; a jump clears it. A miss elsewhere lands on a lower platform or the
662 deck, less than the 20.4 m a body survives, except from the hoist platform; off the 684 deck's
east edge it is 22 m to the 662 deck, and lethal.

Other lines are the player's to find. Three found by the review work in the engine on this build
(probes run 2026-09-30, no death in any): a diagonal jump, walking or sprinting, off the runway east
of the winch house lands on the landing, so the hang is one way past the house and not the only
one; a sprint off the runway west of the house catches the gallery's lip straight, and the body
pulls up onto it; and a jump from the duct clears the beam's gap to the pump deck.

## Stage N — the granular discharge hoist (684.3 → 706.4)

After C4, a machine again: the owner's archetype 09 (*"a continuous flow of high-density material
into a suspended bucket to gradually out-mass the payload"*), the catalogue's one entry the kit's
bins already model. No catch and no governor: the gravel starts it, a chain stops it.

- **Found:** on the 684 deck, 3 m west of where C4's climber steps off, a 2 t service cage railed
  north and south (`kServiceNCageEntityId`, 2135), on a guide with safety dogs every 0.1 m and shoes
  of 100 N friction. West of it, a steel hopper (500 kg, 1.2 m square and 1.4 m deep) hangs empty at
  the top of its own guide from the hoist rope, which runs over the head and down to the cage's eye,
  made fast; from the middle of the hopper's floor a chain of 33 kg/m hangs its whole 23 m to the
  deck. The empty hopper and its chain (1,259 kg) are lighter than the cage, which stands on the
  deck. Over the hopper, on the headframe, an aggregate bin (`kServiceNSiloEntityId`, 1015) holds
  1,213 kg of gravel above a chute shut by a flap: a 3.3 m arm on a pin west of the chute, reaching
  east under the chute's mouth and on over the cage, its counterweight up and west of the pin, so it
  rests shut on its stop. From the arm's end a chain hangs down past a guide into the cage, its
  handle 1.95 m over the cage's floor: in reach of someone standing in the cage and nowhere else.
- **Set off:** GRAB the flap chain in the cage. It comes down to the hands, some 0.43 m, and draws
  the arm past its dead point (0.13 rad); the flap falls open onto its other stop and stays there.
  The bin pours 400 kg/s into the hopper (a chute 0.5 m square: Beverloo gives some 400 kg/s of
  10 mm gravel). About 930 kg in, the hopper outweighs the cage and its rider and sinks, the last
  of the gravel falling in with it. Every metre it sinks sets 33 kg of its chain down on the deck,
  so the drive fades through the stroke and turns halfway; the cage rises past the 706 deck, slows,
  and settles back onto its dogs. The rider steps off east onto the 706 deck.
- **No pull, or a tug short of the dead point:** the flap falls shut again, no gravel moves, and the
  cage stays.
- **Pulled, then stepped out:** the empty cage outweighs the hopper by 85 kg less; it runs into its
  head stop (3.8 m/s in the engine) and is held on the top tooth there. The stage is spent; the
  ladder remains.

The 706 deck (frame 1012, top 706.1) is a strip east of the cage from x −5.1 to −2.0, on columns
from the 684 deck, railed with C4's 0.8 m parapets on every side but where the cage comes up to it;
the ladder comes up through its hatch.

## Declared models, stage N

- **Gravel** as the kit's bins (AS-006 Stage C): a bin's contents add to its body's mass; the stream
  falls straight down from the mouth to the hopper at a declared rate and carries no momentum into it.
- **The hanging chain** (`Kit::add_chain`, new here): the hopper carries the chain's hanging part,
  33 kg for every metre of its floor over the deck up to 23 m, both ways; what lies on the deck is
  off it. The pile's own motion is not simulated. It is the evaluator's `chain_hang0` exactly.
- **Safety dogs** as the kit's, the top of the guide a tooth; **no air drag** on the cage and the
  hopper (Jolt's default linear damping set to zero, as M's).

## Evaluator, stage N (stage1dof.py, INTEGRATED)

h = 1 ms, every case rerun at h/4. The ride from the hopper's breakaway with its whole charge in
it: in the engine the last 280 kg arrive in the first 0.1 m of travel.

```json
{
 "name": "AS-010 N granular discharge hoist, the 684 deck to the 706 deck",
 "g": 9.81,
 "model_kind": "counterweight",
 "model": {"block_mass": 1713.0, "chain_density": 33.0, "chain_hang0": 23.0, "cage_mass": 2000.0,
           "rider_mass": 85.0, "n_falls": 1, "sheave_ieq": 0, "friction_kinetic": 135,
           "friction_static": 280},
 "terminal": {"catch_q": 21.1, "catch_pitch": 0.1, "stop_q": 22.9, "catch_pad_stroke": 0.05},
 "band": {"model.friction_kinetic": [85, 135, 185]},
 "require": {"allowed_outcomes": ["CAPTURED_FALLBACK"], "min_breakaway_ratio": 1.5,
             "max_peak_accel_g": 0.3, "max_catch_pad_accel_g": 0.5, "max_stop_impact_speed": 0.0,
             "max_peak_speed": 3.5, "max_time_s": 25,
             "convergence": {"catch_speed_up": {"abs": 0.01}, "apex_q": {"abs": 0.01},
                             "fallback_speed": {"abs": 0.01}}},
 "rest_variants": [{"name": "rider_exits", "set": {"model.rider_mass": 0}}]
}
```

| Case | Outcome | Apex | Held on dogs | Peak | Time | Pad | Rider gone |
|---|---|---|---|---|---|---|---|
| 85 N | CAPTURED_FALLBACK | 22.28 m | 22.2 m | 3.10 m/s | 11.7 s | 0.14 g | held |
| 135 N | CAPTURED_FALLBACK | 21.99 m | 21.9 m | 3.06 m/s | 11.8 s | 0.14 g | held |
| 185 N | CAPTURED_FALLBACK | 21.69 m | 21.6 m | 3.02 m/s | 11.8 s | 0.15 g | held |

Breakaway 13.6; peak acceleration 0.09 g; the rope never below 18.6 kN; ledger residual under
1.1 × 10⁻⁴. At the engine's own 100 N the evaluator's apex is 22.19 m and it holds at 22.1 m; the
engine holds at 22.10 m. With the 706 deck at 706.1, the cage's floor stands between 0.2 m under
and 0.4 m over it across the band. Rejected on the way: with no chain and the same start, the drive
never turns and the cage runs into its stop (checked in the engine, below); a hopper that starts
while gravel is still arriving was kept, not caught and released full, because a fast pour
finishes in the first 0.1 m of travel.

## C5 — the cooling plant, a climb (706.1 → 728.1)

After stage N, a climb again, as the owner laid the route out (2026-09-25). C5 is one static body
(`kServiceC5EntityId`, 1016) and a pipe manifold of its own (`kServiceC5ManifoldEntityId`, 1017, so
a vault over it lands on another body), over the footprint's north-east on columns from the 662
deck, climbed on the player's movement and nothing else, to the **728 deck** (frame 1012: x −2 to
12, z −162 to −142.2, top 728.1, a yellow girder along its north edge, and a hatch for the backup
ladder). It asks for the three verbs C4 did not: a vault, a crawl and a sprint.

| # | From → to (top, m) | Move | Envelope (`simulation.cpp`) |
|---|---|---|---|
| 1 | 706 deck → plant floor (706.1) | east through the opening in the 706 deck's east parapet, level | — |
| 2 | over the manifold | a vault over a bundle of pipes 1.0 m high and 0.5 m deep, parapet to parapet | vault 0.9–1.15 m, onto another body |
| 3 | under the duct bank | crouched, 2 m under ducts 1.45 m clear that span the floor over its parapets; standing, a walker stops at them | crouched 1.2 m tall, standing 1.8 m |
| 4 | plant floor → tank (707.7) | mantle 1.6 m | 0.9–1.85 m |
| 5 | tank → the platform over it (714.0) | its standpipe, 6.1 m hand over hand, over the top | hold ≤ 0.18 m section |
| 6 | → the landing (714.0) | west along the platform at a sprint, and a jump over a 6.5 m gap. The landing is a 0.4 m slab, under the 0.45 m of face a hang needs, so a jump short of it is not saved by a hang: it falls to the plant floor. A wall 1.2 m high at its far end to run out against | 6.17 m walking, 8.98 m sprinting |
| 7 | landing → first platform (717.2) | jump, hang, climb up: 3.2 m | lip ≤ 3.76 m above take-off |
| 8 | first → second platform (720.4) | jump, hang, climb up: 3.2 m | as 7 |
| 9 | second → top platform (726.5) | the second standpipe, 5.9 m, over the top | as 5 |
| 10 | top platform → 728 deck (728.1) | mantle 1.6 m over the yellow girder | as 4 |

The plant floor and the tank's top are railed where they meet the air. North of the landing, the
first and second platforms stand over the void down to the 662 deck, so their edges there are
closed: the first is railed on its west edge, where a step off would land by the 706 deck's hatch
and slide into it; the second is a block standing on the first's level, and the top platform a
block standing on the second's, so each one's face closes the edge of the one below; the second's
slab reaches as far south as the top platform, so a step off the top platform's west edge anywhere
lands on it; and the duct bank, which a body can drop onto from the second platform, is railed at
its ends. What is left open drops a body onto something it survives: from the landing, 6.9–7.9 m to
the plant floor; from the first platform's south edge, 11.1 m; from the second's, 11.8 m onto the
duct bank or 14.3 m to the floor (16.8 m/s); from the top platform's west edge, 6.1 m onto the
second (probes run 2026-09-30, slow steps and full runs, no death in any). The hatches are the
exception, as on every deck of AS-010: the 706 deck's and the 728 deck's open 22 m down to the
floor below, past the 20.4 m a body survives.

## Stage O — the gravel wheel (728.3 → 750.4)

After C5, a machine again: the owner's archetype 16, the gravel hopper waterwheel (*"the massive
weight of the stone forces the wheel to rotate, generating high continuous torque on its central
axle"*; *"a low-gear hoist"*). Built as its trace lays it out with two changes, declared: the
gravel falls into a bucket hung on the wheel's rope, not onto blades on its rim (the same torque,
with a stroke to it), and the trace's first step, a rolling pipe striking the latch, is a swinging
ram the rider frees, as every stage's trigger is the rider's own act. No governor and no catch: a
swinging weight starts it, the gravel drives it, a chain stops it.

- **Found:** on the 728 deck, in its east half, a 2.4 t cab (`kServiceOCabEntityId`, 2139) railed
  north and south, on a guide with safety dogs every 0.1 m and shoes of 100 N friction, its rope made
  fast to an eye on its west side. Over it, 26 m up, a wheel (2141: a rim of 2.0 m radius on 16
  segments and 8 spokes, a drum of 1.0 m radius on the same axle, 2,200 kg, turning on two bearings
  on posts from the 750 deck) carries the rope: off the drum's south side down to the cab, and round
  the rim from its north side down to a bucket (2140), 400 kg, 1.2 m square and 1.4 m deep, hanging
  empty at the top of its own well. The well is a 1.5 m square shaft through the 750 and 728 decks
  to the plant floor at 706.1; from the bucket's floor a chain of 16 kg/m hangs its whole 45 m
  (720 kg). The rim is twice the drum, so the bucket is pulled half as hard as the cab and goes twice
  as far. Over the well on the 750 deck a bin (1018) holds 498 kg of gravel above a chute shut by a
  plate (2142) weighted to fall open, held by a latch (2143), a 2 m post leaning east onto its stop.
  Beside them an 800 kg ram (2144), four pipes on rigging bars on a 4 m arm from a pivot at 759.0,
  hangs drawn back 35° east, held there by a chock (2145) standing on its post; the chock's lanyard
  runs over a sheave on a beam between the cab's guide posts and down into the cab to a T-handle
  (2146) 1.6 m over the cab's floor: in reach of someone standing in the cab and nowhere else. The
  empty bucket and its chain, 1,120 kg that count twice for going twice as far (2,240 against the
  cab's 2,400), are lighter than the cab.
- **Set off:** GRAB the handle in the cab and step back with it. The lanyard draws the chock over
  its pin, past 0.5 rad, and the ram is free: it swings west into the latch within a second and
  knocks it past 0.15 rad; the gate lets go, falls open onto its stop, and the bin pours 400 kg/s
  into the bucket (as N's). About half a second into the pour the bucket outweighs the cab and its
  rider, at two to one, and sinks, turning the wheel; the rope hauls the cab up. Every metre the
  bucket sinks sets 16 kg of its chain down on the plant floor, so the drive fades through the stroke
  and turns before the top: the cab rises 22 m, slows, and settles onto its dogs with its floor
  0.3 m over the 750 deck. The rider steps off east onto the deck.
- **No pull, or a tug short of the chock's release:** the chock falls back onto its post, the ram
  stays held and nothing moves.
- **Pulled, then stepped out:** the cab, 85 kg lighter, goes higher; the bucket meets its end stop
  on the plant floor and the cab overshoots it by 0.13 m and is held on a tooth at 22.6 m, its floor
  0.8 m over the deck. The stage is spent; the ladder remains.

The 750 deck (frame 1012, top 750.1) covers the 728 deck's footprint (x −2 to 12, z −162 to −142.2)
on columns from the 728 deck, with holes for the cab (x 1.6 to 4.4, z −152.45 to −149.35), the well
(x 5.25 to 6.75, z −148.65 to −147.15) and the ladder's hatch (x 10.0 to 11.4, z −158.2 to −156.0).
It is railed with C4's 0.8 m parapets on all four edges, round the well, and round the cab's hole on
every side but the east, where the cab lands, as N's cage lands over the 706 deck's west edge; while
the cab is down that opening, like the hatches, drops a body 22 m, past what it survives. The 728
deck is railed round the well.

## Declared models, stage O

- **Gravel** and **the hanging chain** as N's (the kit's bins and `Kit::add_chain`): the bucket's
  contents add to its body's mass, the stream falls straight down from the mouth at a declared rate
  and carries no momentum into it; the bucket carries the chain's hanging part, 16 kg for every metre
  of its floor over the plant floor up to 45 m, both ways.
- **The wheel** (`Kit::add_wheel`, new here): a hinge on the axle, free, and a rack-and-pinion tying
  its angle to the bucket's travel down its guide (1 rad for 2.0 m of the bucket, 1.0 m of the cab),
  so the wheel's inertia about its axle, 3,893 kg·m² from Jolt's own mass properties for its parts
  (973 kg reflected at the bucket), is spun up by the bucket and given back. The rope's wound length
  is drawn, not simulated (`Kit::set_wound`): its polyline runs from the bucket's bail to the rim's
  north point, and on from the drum's south point to the cab's eye.
- **No air drag** on the cab, the bucket and the wheel (Jolt's default damping set to zero, as M's);
  the ram's rigging friction and the air take its swing down (0.1 s⁻¹ linear, 0.3 s⁻¹ angular,
  declared), and the handle's damping is N's chain's. **Safety dogs** as the kit's.

## Evaluator, stage O (stage1dof.py, INTEGRATED)

h = 1 ms, every case rerun at h/4. `q` is the bucket's descent; the cab rises `q/2`. The ride from
the bucket's breakaway with its whole charge in it; the wheel at the engine's reflected inertia
(973) ±10 %, and the shoe friction band as M's and N's.

```json
{
 "name": "AS-010 O gravel wheel, the 728 deck to the 750 deck",
 "g": 9.81,
 "model_kind": "counterweight",
 "model": {"block_mass": 898, "chain_density": 16.0, "chain_hang0": 45.0, "cage_mass": 2400.0,
           "rider_mass": 85.0, "n_falls": 2, "sheave_ieq": 973, "friction_kinetic": 135,
           "friction_static": 280},
 "terminal": {"catch_q": 42.2, "catch_pitch": 0.2, "stop_q": 45.8, "catch_pad_stroke": 0.1},
 "band": {"model.friction_kinetic": [85, 135, 185], "model.sheave_ieq": [876, 973, 1070]},
 "require": {"allowed_outcomes": ["CAPTURED_FALLBACK"], "min_breakaway_ratio": 1.5,
             "max_peak_accel_g": 0.3, "max_catch_pad_accel_g": 0.5, "max_stop_impact_speed": 0.0,
             "max_peak_speed": 3.5, "max_time_s": 30,
             "convergence": {"catch_speed_up": {"abs": 0.01}, "apex_q": {"abs": 0.01},
                             "fallback_speed": {"abs": 0.01}}},
 "rest_variants": [{"name": "rider_exits", "set": {"model.rider_mass": 0}}]
}
```

| Shoe friction | Outcome | Cab apex | Held on dogs | Peak | Fall-back | Rider gone |
|---|---|---|---|---|---|---|
| 85 N | CAPTURED_FALLBACK | 22.24–22.30 m | 22.2 m | 2.58–2.66 m/s | 0.23–0.35 m/s | held |
| 135 N | CAPTURED_FALLBACK | 22.09–22.15 m | 22.0–22.1 m | 2.56–2.64 m/s | 0.17–0.35 m/s | held |
| 185 N | CAPTURED_FALLBACK | 21.95–22.00 m | 21.9–22.0 m | 2.54–2.62 m/s | 0.06–0.31 m/s | held |

Nine cases and the nominal, all passing. Breakaway 26.3; peak acceleration 0.065–0.071 g; the rope
never under 11.29 kN at the cab's side and 10.01 kN at the bucket's; the rider's floor normal never
under 774 N; ledger residual under 7.2 × 10⁻⁵; h/4 changes the apex by 1.1 × 10⁻⁶ m and the catch
and fall-back speeds by 3 × 10⁻⁴ m/s. With the rider gone the net drive is −2.84 kN and the dogs
hold. The engine at its own 100 N: apex 22.19 m, held 22.10 m, peak 2.60 m/s, inside the band, the
cab's floor 0.30 m over the 750 deck (0.1–0.4 m over across the band).

The empty cab, run with no rider from the start, peaks at 2.90–2.93 m/s and meets the bucket's end
stop on the plant floor (q = 45.0) at 1.6–1.7 m/s of cab speed, 3.2–3.5 m/s at the bucket: a spent
stage's ending, not the loaded ride's, so outside the `max_stop_impact_speed` above; the engine's
empty run peaks at 2.92 m/s. Rejected on the way: a bucket of gravel alone (898 kg at two to one,
1,796 kg against the cab's 2,485) never lifts the cab, so the chain is the drive as well as the
stop (derived); and the chain swapped for a fixed weight of the same starting mass, which never
fades, takes the cab to its stop at about 4 m/s and throws the rider off it (checked in the engine,
below).

## Climbing route, no lift

The backup (plan §2.6 rule 8): a ladder from TP-640 up through a hatch in the 662 deck at x 8.1–9.7,
z −147.4 to −145.25, a second from the 662 deck up through a hatch in the 684 deck at x −4.8 to
−3.2, z −147.4 to −145.25, a third from the 684 deck up through a hatch in the 706 deck at
x −4.3 to −2.7, z −141.5 to −139.3, coming up on its south side onto the open deck, and a fourth
from C5's plant floor up through a hatch in the 728 deck at x −1.9 to −0.5, z −148.6 to −146.4, and
a fifth from the 728 deck up through a hatch in the 750 deck at x 10.0 to 11.4, z −158.2 to −156.0.

## Falsifiers

`AS-010 M` (no link: the reel falls paying out, the cage stays; hooked on: held on its dogs by the
deck, never past 3.5 m/s, the gain within the reel's release; empty, the cage stays on its dogs);
`AS-010 C4` (the 662 deck to standing on the 684 deck on player inputs, no death, no body step over
0.15 m a tick; the beam holds its walker on its line; the winch house offers nothing to climb and
catches nothing jumped at, and on foot, pushed west along the lip, into it or into the house,
nothing gets past it; the body hangs all the way along the runway's lip past the house; walked
into each of the five parapets, a body stays up on its platform); `AS-010 N` (as found nothing
moves; a tug short of the dead point lets the flap fall shut and no gravel moves; pulled past it,
the bin pours its whole charge into the hopper, the cage is held on its dogs a step from the 706
deck either way, clear of its stop, never past 3.5 m/s, its rider aboard all the way and never
moved over 0.15 m a tick, the gain within what the hopper, its gravel and its chain released, tick
by tick; held with its rider aboard and after it steps off; sent up empty, held on its top tooth,
the rider left on the 684 deck); `AS-010 C5` (the 706 deck to standing on the 728 deck on player
inputs, no death, no body step over 0.15 m a tick; the manifold goes by in a vault and the duct bank
crouched, and standing a walker stops at the ducts; the gap taken at a sprint, over 7.5 m/s, and a
walking jump falls short onto the plant floor alive; walked into the tank top's rails, the first
platform's west parapet, the faces over the first and second platforms' east edges and the duct
bank's ends, a body stays up; stepped slowly off the top platform's west edge where it juts, it
lands on the second, and off the second's south edge, on the duct bank); `AS-010 O` (as found nothing moves and the rope carries the empty bucket and its chain; the handle
is out of reach from the deck on all four sides of the cab; a tug short of the chock's release lets
it fall back and nothing pours; pulled, the causes come in order, the ram free, the latch tripped,
the gate open, the gravel pouring, the cab rising, and the bin pours its whole charge into the
bucket; the cab is held on its dogs a step from the 750 deck either way, clear of its stop, never
past 3.5 m/s, its rider aboard all the way and never moved over 0.15 m a tick; the wheel turns a
radian for each metre the cab rises, within 0.1 rad; the rope stays over 5 kN all the way up and
at rest carries the weight of the bucket, its gravel and its hanging chain; the gain is within
what the bucket, its gravel and its chain released, tick by tick; held with its rider aboard and
after it steps off; sent up empty, held on a tooth, the rider left on the 728 deck; walked into the
rail round the well on the 728 deck, the 750 deck's four parapets and the three round the cab's
hole, a body stays up); `AS-010 band` (the TP-640 start through M, C4, N, C5 and O to standing on
the 750 deck); `AS-010 route` (the five ladders, the lifts untouched). Through the game's input: `touch_service`, `pad_service`, `keyboard_service` (M),
`touch_c4` (C4, from the 662 deck), `touch_n` (N, from the 684 deck), `touch_c5` (C5, from the 706
deck), `touch_o` (O, from the 728 deck), and the stack runs from the game's start: `touch_stack` on
to the 750 deck, `pad_stack` and
`keyboard_stack` to the 662 deck (touch is the device the game is played on; the owner, 2026-09-24:
no keyboard or gamepad needed).

## Result record

Built on `ScraperX-Claude` after `b35f1ed`.

| Group | Result |
|---|---|
| `AS-010 M` | shackle free: the reel falls 22.9 m paying out its cable, the cage stays. Hooked on: apex 21.85 m, held at 21.80 m, peak 2.94 m/s, 12.2 s (2.93 m/s and 12.4 s in the world before N's bodies; Jolt's ordering); gain 339.0 kJ against 343.3 kJ released; empty, it stays on its dogs |
| `AS-010 C4` (2026-09-30) | the 662 deck to standing on the 684 deck (685.0 m) in 34.5 s, no death, worst body step 0.085 m a tick; balancing on the beam; the winch house offers nothing standing, catches nothing jumped at, and lets nothing past on foot (three pushes west along its lip); the body hangs all the way, 3.42 m along the runway's lip; walked into each parapet, the body stays up (with any one parapet taken away, the walk into it ends past the tower's face) |
| `AS-010 N` (2026-09-30) | the flap drawn past its dead point by taking hold of its chain in the cage; the bin's 1,213 kg poured into the hopper; apex 22.12 m, held at 22.10 m (the cage's floor 0.30 m over the 706 deck), peak 3.07 m/s, 15.5 s from the flap falling open to the cage at rest; gain 452.0 kJ against 457.0 kJ released; worst body step 0.061 m a tick; held with its rider aboard and after it steps off onto the 706 deck; a tug short of the dead point moves nothing; sent up empty, held on its top tooth (22.90 m, 3.81 m/s into the head). With no chain and the same start the cage runs into its stop and the check fails |
| `AS-010 C5` (2026-09-30) | the 706 deck to standing on the 728 deck (729.0 m) in 27.4 s, no death, worst body step 0.128 m a tick; the manifold vaulted, the duct bank passed crouched (standing, a walker stops short of it at x < 3.0); the gap taken at 8.0 m/s, and a walking jump falls short onto the plant floor (706.99 m), alive; walked into the tank top's rails, the first platform's west parapet, the faces over the first and second platforms' east edges and the duct bank's ends, the body stays up; stepped slowly off the top platform's west edge where it juts, it lands on the second platform, and off the second's south edge, on the duct bank. With the parapet, the blocks and the duct bank's rails taken away, the walk into the first platform's parapet ends off its edge; with the second platform's south strip taken away, the step off the top platform does not land on it; each check fails |
| `AS-010 O` (2026-09-30) | the ram freed by the rider's step back with the chock's lanyard, then in order the latch tripped, the gate open, the bin's 498 kg poured into the bucket and the cab rising; apex 22.19 m, held at 22.10 m (the cab's floor 0.30 m over the 750 deck), peak 2.60 m/s, 20.2 s from the ram let go to the cab at rest; the wheel within 0.023 rad of the cab's rise (22.1 rad, 3.5 turns), the rope never under 11.5 kN on the way up and at rest 8.94 kN, the bucket's weight; gain 538.7 kJ against 548.6 kJ released; worst body step 0.061 m a tick; held with its rider aboard and after it steps off onto the 750 deck; a 0.2 s tug moves nothing; sent up empty, held on a tooth (22.63 m apex, 2.92 m/s, the bucket on its end stop). With the chain swapped for a fixed weight of the same starting mass the cab runs into its stop at about 4 m/s and the rider is thrown off it; with the well's rails taken away the walk into the well's rail ends in the well, and with the 750 deck's parapets taken away the walk into the south parapet ends past the deck's edge (z −163.6): each check fails |
| `AS-010 band` | TP-640 to standing on the 662 deck in 21.4 s, through C4 to the 684 deck in 57.7 s, through N to the 706 deck in 77.6 s, through C5 to the 728 deck in 106.4 s, through O to the 750 deck in 131.6 s |
| `AS-010 route` | the five ladders, TP-640 to the 750 deck in 132.9 s, the lifts untouched |
| Input | `touch_service`, `pad_service`, `keyboard_service` from the TP-640 start: held at 21.60–21.80 m, off onto the deck; `touch_c4` from the 662 deck to the 684 deck (36.5 s); `touch_n` from the 684 deck to the 706 deck (21.7 s, held at 22.10 m); `touch_c5` from the 706 deck to the 728 deck (30.5 s), the first input-path proof of a sprint, latched by the stick pushed past its ring; `touch_o` from the 728 deck to the 750 deck (26.6 s, held at 22.10 m, hands, body and view within their bounds); `touch_stack` from the game's start to the 750 deck (723.2 s, no death), `pad_stack` and `keyboard_stack` to the 662 deck |

What building N taught. DEFECT, in the kit: a bin set its body's mass in the solver to the body's
own and its contents, and a reel to the body's own and its wound cable, each overwriting the other;
no body carried both until N's hopper, which carries a bin and a chain. One function now sets a
body's mass from everything on it, and stage M's numbers came out the same to the last digit with
it. And the flap's first placement put its counterweight through one of the hopper's guide posts
and a strut through its own pin's hanger: the first run showed the flap stuck at 0.09 rad, short of
its dead point. Both were moved clear, and the flap now goes over at 0.13 rad.

What building C5 taught. DEFECT, pushed with N: the third ladder came up through the 706 deck's
hatch on its north side, into a pocket 1.05 m deep behind the hatch and closed by the parapets; the
strips either side of the hatch, 0.55 m and 0.45 m wide, are narrower than a body. `AS-010 route`
stood on the 706 deck there and passed. The ladder now comes up on the hatch's south side, facing
the open deck, and the route goes on from there to the fourth ladder. Before C5 was committed,
probes of its edges found its line sound and its edges not: stepped off slowly where the top
platform jutted 1 m south of the second, a body fell 20.4 m to the plant floor at 19.0–19.97 m/s,
against a lethal 20.0, and died in 2 of 9 probes; stepped off the first platform's west edge, it
landed on the 706 deck's parapet beside the hatch and slid into the hatch, 22 m to the 684 deck,
dead in 8 of 8; and the first and second platforms' east edges ran out under the next platform up
(0.05 m and 3.1 m over a standing body's head) over the void. The closures above came from that, and `AS-010 C5`
checks each. And the input path's view watch compared the eye with the body's centre, which a
crouch moves 0.3 m in one tick while the soles stay put and the eye glides from them (`main.gd`):
the first crouch it saw, in `touch_c5`, read as a 0.283 m jump of the view. The watch measures the
body at its soles now, as the eye does, and the crouch's glide reads 0.090 m in a frame, under the
0.10 m bound.

What building O taught. The first handle hung from the head beside the cab's south rail, outside
the cab and in reach from the deck; it hangs inside the cab now, from the head, as N's chain does.
Taken at 1.8 m over the floor it sat at the carry's 1.17 m reach limit, and taken as the rider
stepped up into the cab it was swinging faster than the carry accepts (0.5 m/s) or the rider had not
landed yet; it hangs 1.6 m over the floor and the native test waits for the rider to land. The ram,
a pendulum, swung on for minutes under angular damping alone, which does little for a pendulum; a
linear damping of 0.1 s⁻¹, declared, takes its swing down. The wheel's inertia came from the engine,
not from a first guess of 1,000 kg·m² at the bucket: Jolt's own figure for the wheel's parts is
973. And the lanyard's sheave first hung in the air; a beam between the cab's guide posts carries it.

What a review of C4 taught: the first winch house stood 0.3 m short of the runway's lip, and a
walker got round it on the lip's edge, the capsule's rim holding it up to 0.29 m out beyond the lip
(support down to a 0.55 normal), so the hang was not needed. The house now stands flush with the lip
from 0.35 m up, and `AS-010 C4` pushes a walker west along the lip three ways; that check fails on
the first house and passes on this one. The same review found the runway's and the pump deck's outer
edges 0.2–0.5 m inside the tower's faces, so a step off them fell past the 662 deck and down the
stack; the parapets came from that. The run-up to the gap starts 0.2 m further in (z −161.0), clear
of the runway's parapet.

Two things the input path taught: with 0.3 m dog teeth the cage could be held a tooth low, 0.4 m
under the deck (past the walker's 0.35 m step), when the rider's own movements in the cage shifted
the apex by 0.2 m; the teeth are 0.1 m now and the deck 0.15 m lower. And the added bodies nudged
Jolt's ordering enough that AS-009 J's rail joint came to rest in its tray 0.253 m from its seat,
where the seat's 0.25 m reach under-covered the tray; it reaches 0.30 m now, the tray's own extent.

The full native suite then turned up two faults below the stage, both fixed here. Jolt's pulley
constraint never wrote a static body's lever arms and read them in every velocity solve, times zero:
allocation garbage holding a NaN there tore a rope's load to NaN the first time the rope went taut.
AS-008 G's tower, whose rope is made fast to its cleat, went NaN in some full-suite runs and never
alone, as the allocator's reused memory decided. A configure-time patch zeroes those members
(`third_party/jolt_patch.cmake`), and `pulley seam` allocates the constraint from NaN-filled memory
and fails without it. Under valgrind, AS-008 showed 197 uninitialised reads before the patch and none
after; AS-010 M and its band show none. And the rider's support pick kept the first of two equal
contacts in the order Jolt's worker threads delivered them; it is a total order now, and six full
suites run side by side give byte-identical logs.
