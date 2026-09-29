# SCRAPERX — AS-010 MIDSTACK SERVICE (B06, 640 → 780 m)

**Ascent Slice:** `AS-010`
**Lifecycle:** `IN PROGRESS` — stage M, the service lift from TP-640 to the 662 deck, built and
played through the game's input; everything above the 662 deck is unbuilt
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

## Climbing route, no lift

A ladder from TP-640 up through a hatch in the 662 deck at x 8.1–9.7, z −147.4 to −145.25.

## Falsifiers

`AS-010 M` (no link: the reel falls paying out, the cage stays; hooked on: held on its dogs by the
deck, never past 3.5 m/s, the gain within the reel's release; empty, the cage stays on its dogs);
`AS-010 band` (the TP-640 start to standing on the 662 deck on player inputs); `AS-010 route`.
Through the game's input: `touch_service`, `pad_service`, `keyboard_service`, and the stack runs
from the game's start.

## Result record

Built on `ScraperX-Claude` after `b35f1ed`.

| Group | Result |
|---|---|
| `AS-010 M` | shackle free: the reel falls 22.9 m paying out its cable, the cage stays. Hooked on: apex 21.85 m, held at 21.80 m, peak 2.94 m/s, 12.2 s; gain 339.0 kJ against 343.3 kJ released; empty, it stays on its dogs |
| `AS-010 band` | TP-640 to standing on the 662 deck in 21.2 s |
| `AS-010 route` | the ladder, the lift untouched |
| Input | `touch_service`, `pad_service`, `keyboard_service` from the TP-640 start: held at 21.60–21.70 m, off onto the deck; the stack runs from the game's start to the 662 deck (~606 s) |

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
