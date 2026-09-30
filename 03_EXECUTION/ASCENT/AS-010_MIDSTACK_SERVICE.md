# SCRAPERX — AS-010 MIDSTACK SERVICE (B06, 640 → 780 m)

**Ascent Slice:** `AS-010`
**Lifecycle:** `IN PROGRESS` — stage M, the service lift from TP-640 to the 662 deck, and C4, the
service gantry climbed from the 662 deck to the 684 deck, built and played through the game's
input; everything above the 684 deck is unbuilt
**Provenance:** derived here under `03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md` and the Atlas's
B06 (`01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md`: `MOD-SERVICE-LIFT`, a second cage), from the owner's
archetype 11, the industrial spool unwind (`Mechanism-Ideas-and-archetypes.md`).
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
than a hold, so nothing but the standpipe is climbed hand over hand. A miss lands on a lower
platform or the 662 deck, less than the 20.4 m a body survives, except from the hoist platform.
Other lines are the player's to find. By the envelope's numbers, and not proven in the engine: a
sprint off the runway may carry across the gap straight into the gallery's hang, a running diagonal
off the runway's east end may reach the landing past the winch house, and the beam's 4.5 m gap is
inside a walking jump.

## Climbing route, no lift

The backup (plan §2.6 rule 8): a ladder from TP-640 up through a hatch in the 662 deck at x 8.1–9.7,
z −147.4 to −145.25, and a second from the 662 deck up through a hatch in the 684 deck at x −4.8 to
−3.2, z −147.4 to −145.25.

## Falsifiers

`AS-010 M` (no link: the reel falls paying out, the cage stays; hooked on: held on its dogs by the
deck, never past 3.5 m/s, the gain within the reel's release; empty, the cage stays on its dogs);
`AS-010 C4` (the 662 deck to standing on the 684 deck on player inputs, no death, no body step over
0.15 m a tick; the beam holds its walker on its line; the winch house offers nothing to climb and
catches nothing jumped at, and on foot, pushed west along the lip, into it or into the house,
nothing gets past it; the body hangs all the way along the runway's lip past the house); `AS-010 band` (the TP-640 start through
M and C4 to standing on the 684 deck); `AS-010 route` (both ladders, the lift untouched). Through
the game's input: `touch_service`, `pad_service`, `keyboard_service` (M), `touch_c4` (C4, from the
662 deck), and the stack runs from the game's start: `touch_stack` on to the 684 deck, `pad_stack`
and `keyboard_stack` to the 662 deck (touch is the device the game is played on; the owner,
2026-09-24: no keyboard or gamepad needed).

## Result record

Built on `ScraperX-Claude` after `b35f1ed`.

| Group | Result |
|---|---|
| `AS-010 M` | shackle free: the reel falls 22.9 m paying out its cable, the cage stays. Hooked on: apex 21.85 m, held at 21.80 m, peak 2.94 m/s, 12.2 s; gain 339.0 kJ against 343.3 kJ released; empty, it stays on its dogs |
| `AS-010 C4` (2026-09-30) | the 662 deck to standing on the 684 deck (685.0 m) in 34.5 s, no death, worst body step 0.085 m a tick; balancing on the beam; the winch house offers nothing standing, catches nothing jumped at, and lets nothing past on foot (three pushes west along its lip); the body hangs all the way, 3.42 m along the runway's lip |
| `AS-010 band` | TP-640 to standing on the 662 deck in 21.4 s, through C4 to the 684 deck in 57.8 s |
| `AS-010 route` | both ladders, TP-640 to the 684 deck in 54.5 s, the lift untouched |
| Input | `touch_service`, `pad_service`, `keyboard_service` from the TP-640 start: held at 21.60–21.70 m, off onto the deck; `touch_c4` from the 662 deck to the 684 deck (36.6 s); `touch_stack` from the game's start to the 684 deck (643.4 s), `pad_stack` and `keyboard_stack` to the 662 deck |

What a review of C4 taught: the first winch house stood 0.3 m short of the runway's lip, and a
walker got round it on the lip's edge, the capsule's rim holding it up to 0.29 m out beyond the lip
(support down to a 0.55 normal), so the hang was not needed. The house now stands flush with the lip
from 0.35 m up, and `AS-010 C4` pushes a walker west along the lip three ways; that check fails on
the first house and passes on this one.

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
