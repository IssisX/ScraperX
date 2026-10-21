# Stage E rebuilt: step into the shaft and get blown out of its mouth (256.25 to 298.25 m)

*This is the one spec for replacing stage E, the chiller-drop lift. Per the 2026-10-20 directive, the owner talks it through and refines it before anything is built. It changes only the wet band at 220 to 340 m. It does not touch the Matron's code or tests.*

## For the owner, in plain words

The float lift stops at 256 m beside a steel tube 2.7 m across. In its side is a pair of hazard-striped steel doors. You walk into them and they swing in ahead of you, but there is no floor behind them. The lift's car is three meters down the tube: a bare steel deck. You step off and drop onto it. The landing knocks the deck down a hand's breadth, and that sets everything off. A bang sounds overhead as a 5-ton chiller lets go in the tube next door, and the doors swing back and bang shut behind you. Half a second later the floor heaves under you at about 1 g, and the car runs up the tube at over 3 m/s on the air the falling chiller drives under it.

Through glass in the wall you see the chiller rush down past you a meter away, and the tower's rings slide down outside. At the top the car bursts out of the tube's mouth into open air in a roar of venting air and spray. It floats for a moment so your feet go light, bobs on its trapped air and settles level with the next platform, about 20 seconds after you stepped off. You press no button, grab no handle and climb no ladder: you walk in, drop and ride.

**Two things for you to decide:**

1. **Is the deck too close to the first lift's scale plate?** The deck you land on hangs on a line to a lever, built the same way as the scale plate in the first lift. Standing on it does nothing. Only a falling body sets it off. Does that still count as a repeat? If it does, there is a fallback: the car itself dips onto a counterweighted buffer. It is weaker, because its margins are only centimeters.
2. **Old saves will not load.** Saves made before this change will be refused, as with earlier changes of this kind.

## What changed from the panel's winning design, and why

- **The door is now a pair of light leaves on leaning hinges.** This takes the leaning hinge from design 3 and fits it to the winner. The leaves are body 2042 and the old latch body 2043, 35 kg each, on hinges leaning 8°.
  - Checked first: a single 1.1 m leaf at 6–8° takes 2.0–3.0 s to shut, and it sweeps 1.1 m into the shaft where the rider stands and rises. The pair shuts in about 1.3 s and sweeps only 0.55 m.
  - Reusing body 2043 as a leaf also removes the "parked before its hinge" fault the judge found.
- **The collar and chiller numbers changed after modeling.** A 90 Hz lumped model of the panel's numbers (a 1 m collar at 2 m² per meter, 52.2 m of chiller travel, a 25 kN bucket governor) has the cab reach its hard top within 2 cm, and in two variants hit it at 3 m/s. The model is `/tmp/claude-0/-home-user-ScraperX/290f45ef-c41d-5b08-903d-c4b10970a44f/scratchpad/e_spec_check.py`. The new numbers clear the hard top by 0.28 m across every variant in the sweep:
  - collar 1.15 m at 3.0 m² per meter;
  - chiller travel 50.0 m;
  - bucket governor 50 kN.
- **The trigger has more margin.** The line is fixed 0.25 m out from the lever's pivot (was 0.27 m), so the catch lets go at 0.12 m of the deck's 0.15 m (3 cm to spare, was 1.4 cm).
  - The lever keeps its exact old parts, so the zipline sweep sees the same swept volume for it. Only the part masses change.
- **The seal leaks much less.** The bleed is cut from 0.004 to 0.0005 m². With it, the car holds within 4 cm for 90 s and is 0.31 m low after 5 minutes, still within the 0.35 m step-up.
- **A collision risk was found and is now tested.** Open leaves stand in the car's path. A rider who holds the doors open while a heavy load is thrown in would jam the car under them. This is now a falsifier, with a declared fallback.

## 1. The mechanism

### Geometry (world meters; doorway x 12.4–13.6 in the shaft's north wall)

| Item | Was | Now |
|---|---|---|
| D's deck at the top / door sill / door top | 256.25 / 256.30 / 258.50 | unchanged |
| Cab top plate (2040 origin) as found | 256.25 | **253.25** (plate 0.10 thick) |
| Landing deck (2045) top as found | handle | **253.45** (0.05 thick, 0.15 above the plate); drop from the sill 2.85 m |
| Skirt sealing edge (1.75 m under the plate's top) | none | 251.50 |
| Cab plenum floor / cab cell | 254.0 / 1.3 m³ | **249.50 / 15.08 m³** (0.5 + 7.29 × 2.0) |
| Duct floor / duct cell | 254.0 / 348 m³ | **247.75 / 392.3 m³** |
| Chiller bottom as found / travel / stop | 301.5 / 45.0 | 301.5 / **50.0** / bottom at 251.5 |
| Shaft rim | 298.20 | **298.00** |
| Vent collar | none | **296.85–298.00**, 3.0 m² per meter of skirt edge inside it (3.45 m² full) |
| Cab guide | 0–42, governor 2.0 m/s | **0–46.5, no governor** (hard top: skirt edge at the rim, deck 299.80, never reached) |
| Bucket floor as found / travel | 257.76 / 45 | **252.76 / 50** (top of travel unchanged at 302.76, under the same spout) |
| Bucket basin center y / striker pivot y | 254.5 / 257.76 | **249.5 / 252.76** |
| Trip lever pivot | (10.0, 305.3, −134.8) | unchanged |
| Line: lever eye, sheave 1 | arm tip 1.0 m, sheave (9.0, 304.6, −134.8) | eye **0.25 m** west (9.75, 305.3, −134.8); sheave 1 (9.75, 304.6, −134.8) directly below |
| Line: sheave 2, deck eye | inside the cab's east wall | (14.15, 304.6, −135.5), on an arm from a new post at the shaft's north-east outer corner (x 14.35–14.50, z −135.30 to −135.15, y 298.0–305.0); deck eye at local (1.15, 0.03, 1.15) |
| Glass (material only, same steel thickness) | none | shaft west wall, shaft east wall and duct east wall, each 258.0–296.0 |

### Masses and forces

| Item | Value |
|---|---|
| Chiller | 5,000 kg (unchanged) |
| Cab | 900 kg; a skirted box: plate, four 0.04 m skirt walls down to the sealing edge, east and west rails 1.0 m (none north or south) |
| Landing deck | 60 kg grating, 2.5 m square, galvanized with hazard edges, on its own vertical guide (travel −0.15 to 46.65, no governor) |
| Rider | 85 kg |
| Trip lever 2044 | 155 kg: block 151 kg at 0.35 m east, arm 4 kg (old shapes kept). Rest torque 499 N·m, so the line holds 1.99 kN. Inertia about 24 kg·m², about 386 kg felt at the eye. Catch release 0.5 rad = 0.12 m of deck; deck bottoms at about 0.65 rad. |
| Leaves (2042 west, 2043 east) | 35 kg each, 0.545 × 2.14 m, hinged 5 cm off each jamb, meeting at x 13.0 with 1 cm between. Hinge axis leans 8° with its top outward (west leaf (0, cos 8°, sin 8°), east leaf mirrored (0, −cos 8°, −sin 8°)), so a positive angle opens each leaf inward and lifts it. Stops at 0 and 1.35 rad; angular damping 1.0/s. Closing torque 13 N·m each when shut; about 30 N at the free edge opens one. |
| Seal friction | 3,000 N (`set_guide_friction` on the cab guide) |
| Throttle / breaker / bleed | **0.42 m²** / 0.3 m² (on the cab cell) / **0.0005 m²** |
| Bucket guide | 300 kg, travel 50, governor **4.0 m/s, 50 kN, 3.0 m/s²** |

### The causal chain (every link is a body, a constraint or a declared force)

1. **The blow.**
   - The deck hangs on E's trip line, 0.15 m proud of the cab's plate. The line is held by lever 2044's counterweight up to 1.99 kN.
   - Rider and deck standing weigh 1.42 kN, so standing never moves it.
   - A falling body's momentum does. The rider reaches the deck at 7.5 m/s (635 N·s) and meets about 531 kg (rider, deck and the lever as felt at the eye). The rider, deck and lever move off at 1.20 m/s with 380 J against 573 N of spare hold, enough for 0.66 m: 5.5 times the 0.12 m needed.
   - The deck slams onto the plate, and the lever passes 0.5 rad about 0.1 s after the landing.
2. **The release.** The chiller's catch (`e_catch`, unchanged) lets go. The chiller falls in its duct and drives its air through the throttle into the cab's cell.
3. **The return of the lever.** The counterweight lifts deck and rider back up the 0.15 m (about 1 m/s²) and the lever goes back to rest. When the cab rises it meets the deck and carries it, and the line goes slack.
4. **The lift.**
   - The cab feels (P − Patm) × 7.29 m² at its skirt. It has seal friction and no governor; the throttle sets its speed.
   - Steady state: duct about 6.3 kPa, cab cell about 1.8 kPa, 3.1–3.5 m/s.
5. **The doors** (declared change: door exposure).
   - The leaves start swinging shut as the body leaves them, about 0.3 s after the step-off, and are shut by about 1.6 s.
   - Declared change: a doorway now leaks, and its leaves are pressed, only by the fraction of its height below the cab's sealing edge. Today it leaks wherever the cab is (`mechanism_kit.cpp:1242-1245`).
   - The deck reaches sill height about 2.6 s after the step-off; the skirt edge reaches the sill about 3.2 s after. Shut, the leaves are pressed onto their frame by the cell's air (they open inward).
6. **The top** (declared: a slotted vent collar).
   - Once the skirt edge rises above 296.85, the cell leaks 3.0 m² per meter of edge above it.
   - The cab rises out of the tube's mouth: the deck peaks about 1.5 m above the rim, with the skirt 0.28 m inside the tube.
   - Above that line the cell vents, and the cab slows at up to 12.7 m/s², more than gravity, so feet and deck lift for a moment.
   - It falls back, recompresses its trapped air, and bobs twice (deck between about 297.75 and 299.2).
7. **The settle.**
   - The bucket's 50 kN governor eases the chiller onto its stop at about 0.3 m/s.
   - The cab rests on its trapped air, held by seal friction, with the collar 0.32 m above its skirt.
   - The small bleed lowers it by less than 0.05 m in 90 s.
8. **The re-arm** (unchanged in kind).
   - The TP-340 dump fills the bucket, which hoists the chiller 50 m back into its catch.
   - The rising chiller draws the cab's air. The cab sinks at about 2 m/s to its bottom stop, and the breaker admits air once it is there.
   - On the way down the deck's line goes taut 0.15 m above the plate, leaving the deck proud. That jerk turns the lever 0.06 rad, under the 0.5 rad release, before the chiller has relatched.

Rider: a moving support (rule 4), on 2045, which rides on 2040. Determinism (rule 10): no clocks, no randomness, fixed 90 Hz; the rest height is the product of the air budget and is pinned by test.

### What sets off the deck and what does not

| Event | Deck travel it could drive (need 0.12 m) | Result |
|---|---|---|
| Walk off the sill (2.85 m drop) | 0.66 m (5.5×) | launches |
| Hang from the sill and let go (0.65–0.9 m drop) | 0.15–0.21 m (1.3–1.75×) | launches |
| Standing, or walking about on the deck | 1.42 kN on a 1.99 kN hold | never |
| Standing holding a 55 kg drum | 1.96 kN on 1.99 kN | holds |
| 18 kg toolbox dropped from the sill | 0.016 m | no |
| 55 kg drum dropped from the sill | 0.195 m | launches empty (causal) |
| A jump on the spot | not counted on | — |

### The ride (lumped model, 90 Hz; a 1.8 kHz run agrees within 0.03 m and 0.6 s)

Times are from the step-off:

| Time | Event |
|---|---|
| 0.76 s | Landing at 7.5 m/s. |
| 0.86 s | The catch lets go. |
| about 1.6 s | The leaves are shut. |
| 1.44 s | The floor heaves: peak 10.1 m/s² (1.03 g) for about 0.4 s, then 3.1–3.5 m/s. |
| about 8.9 s | The chiller passes the rider's eye at about 277.7 m, closing at about 6.2 m/s. |
| 16.0 s | The skirt enters the collar. |
| 16.4 s | The apex: deck 299.52. |
| 17.4 s | The chiller seats. |
| about 19.6 s | Still, deck **298.33** (empty: 298.30). |

Today the same stage takes about 37 s (door, trip, then 28 s at 1.55 m/s).

How sensitive the rest height is:
- seal friction 2 kN gives 298.15; 4 kN gives 297.90;
- moving the collar's lower edge moves the rest height about one for one (the tuning knob);
- the bleed lowers it to 298.29 at 90 s and 297.94 at 300 s;
- no variant reaches the hard top (worst peak 299.63).

Energy (rule 2): the rider gains 85 × 9.81 × 42.08 ≈ 35.1 kJ. The chiller releases 2.45 MJ, less the bucket's 0.15 MJ, so 2.31 MJ. The rider's own 2.4 kJ drop is only the trigger.

## 2. Native changes, file by file

**`/home/user/ScraperX/src/sim/band_wet_isolation.cpp`**

*Stage E constants (lines 99–130):*
- `kPlenumFloor` splits into `kCabPlenumFloor 249.5` and `kDuctFloor 247.75`.
- `kECabTop 253.25`; `kETravel` becomes the cab's hard top, 46.5; `kShaftRim 298.0`.
- `kChillerTravel 50.0`; `kThrottleArea 0.42`; bleed 0.0005.
- New:
  - `kSkirt 1.75`
  - `kVentFrom 296.85`, `kVentPerM 3.0`
  - `kSealFrictionN 3000`
  - `kDeckProud 0.15`, `kDeckHalf 1.25`, `kDeckMassKg 60`
  - leaf constants: 35 kg, `kHingeLean` 8°, stop 1.35 rad
  - lever eye 0.25 m, block 151 kg, arm 4 kg
  - `kESheave1 (9.75, 304.6, −134.8)`, `kESheave2 (14.15, 304.6, −135.5)`
- `kBucketFloorY 252.76`; `kEStrikerPivot` y 252.76; basin y 249.5.

*`build_stage_e` (lines 344–472):*
- **Frame parts.**
  - E's 15 existing frame parts keep their order. They are resized in place: shaft walls from 249.35 to the rim, shaft floor 249.35–249.5, duct walls from 247.6 to 304.2, duct floor 247.6–247.75, basin at 249.5.
  - The shaft's west and east walls and the duct's east wall each keep their existing part as a steel span below 258.0. Each gets a `Material::Glass` span (258.0–296.0) and a steel span above it (296.0 to the rim, or 304.2 for the duct).
  - Those 6 spans, the corner post and its sheave arm (8 parts) go into a `late` vector. `build_wet_isolation` appends it after the plate's and header's parts, so every earlier frame part keeps its index.
- **Cab.** `platform_parts(0)` with its deck replaced by the 0.10 m plate, plus four skirt walls to −1.75. The guide is 0–46.5 with no governor, then `set_guide_friction(e_cab_guide, 3000)`.
- **Chiller.** Same parts; guide −50 to 0.
- **Cells and pistons.** Resized as in the table; throttle 0.42; breaker unchanged; bleed 0.0005.
- **Leaves.**
  - 2042: west leaf, built shut, no `set_carry`, no `place_body`, `set_damping(0.5, 1.0)`. `e_door_hinge` gets the tilted axis, normal +x, 0–1.35.
  - 2043: east leaf, built in the latch's slot the same way, with no park. `e_latch` becomes its hinge (mirrored axis, normal −x, 0–1.35).
  - `e_door_catch` is built in its slot with `relatch = false`, then opened, so it is inert. With relatch on it would pin the west leaf shut whenever both leaves closed.
  - Two door entries (1.32 m² each, full angle 0.3, normal +z), each with exposure (piston 2040, edge local y −1.75, sill 256.3, top 258.5).
- **Lever 2044.** A dedicated builder with the same two part shapes as `add_trip_lever` and part masses 4 kg arm and 151 kg block (body 155 kg). `e_catch` is unchanged.
- **Deck 2045.**
  - Built at (13.0, 253.45, −136.65): grating, hazard edge strips, yellow corner eye. No `set_carry`.
  - The existing `add_trip_line` call is re-laid: eye (−0.25, 0, 0) on 2044, over `kESheave1` and `kESheave2`, to the deck's eye.
- **Vent.** `kit.add_vent(e_cab_cell, e_cab, −1.75, {11.65, 296.85, −138.0}, {14.35, 298.0, −135.3}, 3.0)`.
- **Bucket and striker.** 5.0 m lower. Bucket guide: travel 50, governor 4.0 m/s, 50 kN, 3.0 m/s². The rope length uses the existing formula.
- **New `build_wet_late(kit, wet)`.** `wet.e_deck_guide = kit.add_guide(e_deck, +y, −0.15, 46.65, 0, 0, 0)`. Comment: "built after every other body and guide, so older bodies line up".

**`/home/user/ScraperX/src/sim/bands.hpp` (lines 86–107)**
- `e_latch_body` becomes `e_leaf_east`; `e_handle` becomes `e_deck`.
- Add `e_deck_guide` and `e_vent`.
- Declare `build_wet_late`.

**`/home/user/ScraperX/src/sim/physics_world.cpp`**
- One call, `bands::build_wet_late(*kit_, wet_)`, after the throwables hand-off block (about line 366) and before the `ImpactLedger`.
- It only sits there because that is the end of the build. No Matron code changes.

**`/home/user/ScraperX/src/sim/mechanism_kit.hpp` / `.cpp`**
- `Material::Glass = 8`, presentation only (no native code reads `Material`).
- `add_door` overload taking `(…, BodyIndex piston, float edge_local_y, float sill_y, float top_y)`. In `flow_air` and `press`, leak and press are multiplied by the doorway's height fraction below the edge. The default is always exposed, so no other door changes (E has the kit's only doors).
- `VentIndex add_vent(CellIndex, BodyIndex piston, float edge_local_y, JPH::Vec3 low, JPH::Vec3 high, float area_per_m)`. Its leak is added to the cell's in `flow_air`.
- Read-backs, stored per step and never saved: `cell_inflow(CellIndex)` (throttle flow into the cell, m³/s at atmospheric pressure), `vent_flow(VentIndex)`, `vent_count()`, `vent_box(VentIndex)`.
- None of this enters `Checkpoint`, `capture` or `restore`.

**`/home/user/ScraperX/src/sim/simulation.hpp`**
- `WetState`: remove `e_door_latched`; add `e_door_shut` (both leaves under 0.05 rad), `e_deck_drop`, `e_lever_angle`, `e_cab_speed`, `e_throttle_m3s`, `e_vent_m3s`. `e_door_angle` becomes the larger of the two leaves.
- Renames, same values: `kWetELatchEntityId` becomes `kWetEEastLeafEntityId` (2043); `kWetEHandleEntityId` becomes `kWetEDeckEntityId` (2045).
- Add a tests-only reader for the vent box if the bridge needs it.

**`/home/user/ScraperX/src/sim/simulation.cpp` (`wet_state`, lines 312–340):** fill the new fields.

**`/home/user/ScraperX/src/sim/physics_world_support.hpp` (line 501):** comment only. `WetECab` stays at (13.0, 257.2, −137.2), which is now over the deck, so starting there drops onto it and launches E. Only tests use it, and it is not in the START list.

**`/home/user/ScraperX/src/bridge/scraperx_simulation.cpp` (about line 1019):** wet_state keys for the new fields; drop `e_door_latched`; bind `get_kit_vents()` returning [x0, z0, x1, z1, from_y, to_y, flow] per vent.

**Unchanged:** D's and F's code, the header and riser (the bucket spout is where it was), the climbing route, `world_solids.inc`, `solid_export.gd`, `main_world.gd`.

**Docs:**
- `/home/user/ScraperX/03_EXECUTION/ASCENT/AS-007_WET_ISOLATION.md`: rewrite Stage E. Declare three models (door exposure, vent collar, seal friction), and update falsifier 2 and the result record.
- `/home/user/ScraperX/03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md`: cycle 10's E line.
- `/home/user/ScraperX/00_START_HERE.md`: the 2026-10-06 row's "Replaced so far" (E on the rider's drop) and "Next: F to O".
- `/home/user/ScraperX/03_EXECUTION/ACTIVE_GOAL_PROGRESS.md`: a new entry.

## 3. Presentation, sound and feel

**`/home/user/ScraperX/godot/presentation/main_machines.gd`**
- `palette[8]`, glass:
  - transparent, alpha about 0.25, a slight green-gray, roughness about 0.08, metallic 0;
  - double-sided, drawn after opaque, with the wet band's streak normal;
  - steel mullions every 3 m drawn by the material, not by parts.
- Static glass batches under its own material key: three spans, one transparent draw on the Fold.
- **`_render_vent()`** (beside `_render_water`, lines 597–620), from `get_kit_vents()`: a slotted-collar band over the vent box. Four sheets of spray from its slots, length and opacity scaled by the vent's flow, and mist that hangs over the mouth and fades.
- **Life in the shaft** (from design 2):
  - two warm work lamps (OmniLight3D, 6 m range, no shadows) parented to the cab's node, so light climbs the wet walls;
  - a CPUParticles3D drip off the deck's edges while `e_cab_speed > 0.3`.
- Cab, deck, leaves and lever are drawn from native parts as now. The re-laid line is drawn by `get_kit_cable_points`.

**`/home/user/ScraperX/godot/presentation/audio/audio_director.gd`.** Machines only, no voices; synthesized from seeded noise like the rest of the bank.
- **Throttle roar:** band-passed noise under the cab's floor (moving with it). Gain follows `e_throttle_m3s`; its pitch rises with `e_duct_pa`.
- **Collar blast:** broadband hiss at the mouth, gain following `e_vent_m3s`.
- **Catch bang:** one shot at the lever (10.0, 305.3) when `e_catch_latched` goes false.
- **Chiller seat:** a deep one-shot thump when `e_chiller_travel` reaches within 0.02 of −50. It lands eased, under the clang threshold.
- **Leaves banging shut and the deck's slam:** the existing `update_machines` clang (line 468). Leaf tip about 1.1 m/s and deck about 1.2 m/s, both over `MACHINE_CLANG_MPS` 0.8.
- Counter `air_heard` (beside `pours_heard`) when the roar plays above -30 dB. The wet-band drips (lines 67–75) stay underneath.

**`/home/user/ScraperX/godot/presentation/main_feel.gd`.** HAPTICS gains `&"heave": [60, 0.8]` and `&"rumble": [20, 0.25]`. While the rider's support is 2045 or 2040:
- heave when the cab's vertical acceleration (from `e_cab_speed`) is over 5 m/s², at the launch and at each bob;
- rumble every 0.25 s while above 1 m/s.

Both are gated by the vibration setting and the 50 ms coalescing.

**`/home/user/ScraperX/godot/presentation/main.gd`:** read `get_wet_state()` once a frame while the camera is within 60 m of E's shaft, and hand it to the audio, the vent renderer and the haptics, beside `_sample_machines`.

**`/home/user/ScraperX/godot/presentation/main_context.gd` (lines 470–479):** delete DOOR (2042), DOOR LATCH (2043) and CHILLER TRIP (2045). Nothing in E is ever offered.

## 4. Body-order plan

- **Kit bodies: no body added, removed or split. Every `add_body` call stays where it is**, so entities, kit indices and Jolt BodyIDs are unchanged:

  | Body | Role now |
  |---|---|
  | 2040 | cab, reshaped and 3.0 m lower |
  | 2041 | chiller |
  | 2042 | west leaf |
  | 2043 | east leaf (was the latch) |
  | 2044 | lever, same shapes, new masses |
  | 2045 | deck (was the handle) |
  | 2046 | bucket, 5.0 m lower |
  | 2047 | striker, 5.0 m lower |

- **Frame 1005:**
  - existing parts keep their indices;
  - 8 new parts are appended at its end;
  - its bounding box is unchanged (it already spans y 176–345 over E's column), so its broadphase entry is the same.
- **World solids:** none touched (`world_solids.inc` and `solid_export.gd` untouched). The 2026-10-20 failure path (a world solid shifting every later body) is not involved.
- **Saved tables:**
  - Catches (2), cells (2), ropes, bins, pools, reels and latches keep their counts and order. Levers and trip lines keep their slots.
  - The second door entry and the vent are cell configuration and are not saved.
  - **One guide is appended**, the deck's, in `build_wet_late`. `dog_floor` grows by one, so a save written before this is refused by the exact-size read (`physics_world_state.cpp:373`), as with every earlier table growth. Bodies still line up.
- **What the build-time sweeps see differently:**
  - travel envelopes of 2040 (now 251.5–300.75, was 256.0–299.25), 2041 (down to 251.5) and 2046 (down to 252.7);
  - 2044's is identical (same part shapes);
  - the deck's guide is built after the sweep, but its travel lies inside the cab's;
  - work bounds lose three carried bodies (2042, 2043, 2045, at y 256–258.5, which footings on ring decks never reach in y);
  - the frame is deeper and gains the post.
- **Hand check, ring by ring.** For each candidate line, the outcome is the same before and after.
  - Ring 4, x = 6: still blocked by D's tank hanger and the bucket's envelope.
  - Ring 6, x = 10 and 14: still blocked by F's envelopes, which lie beside the post.
  - Ring 6, x = 6: the bucket envelope's top is unchanged.
  - Ring 5 and ring 7, z = −136: pass through or far above E as before.
  - Ring 3: below everything new.
- **Native guard (not trusted by hand).** These must equal values recorded on the tree before the change:
  - `kit_body_count()` and an FNV hash of `kit_body_entity(i)` in order;
  - `zipline_count()` and every line's top and bottom, to the mm;
  - `loose_stock_count()` and every piece's kind and built position, to the mm.

  It also checks that no built line's rider room (1.0 m wide, 2.7 m under the cable) and no stock piece overlaps E's new frame parts or the cab, chiller, bucket or deck envelopes.
- **If the guard moves**, re-stage everything the map lists as order-fragile:
  - native: the full suite;
  - touch: touch_stack, touch_stack_upper, touch_save, touch_forklift, touch_forklift_strain, touch_yard_crane, touch_crane_magnet, touch_crane_spreader, touch_shop, touch_crane and touch_service.

## 5. Native falsifiers: new group `SCRAPERX_ONLY=AS-007-E`, also run inside `AS-007`

In `/home/user/ScraperX/tests/sim_tests_wet.cpp`, registered in `/home/user/ScraperX/tests/simulation_tests.cpp`. Delete `shut_wet_door` and `pull_wet_trip`. New helpers:
- `drop_into_wet_e`: from D's top, `walk_to(13.2, −134.5)` then `(13.2, −136.2)` until airborne, then stick released.
- `hang_drop_into_wet_e`: walk to (13.2, −135.0), face north, `request_release` to hang, then `request_release` to let go.
- `wait_wet_e_top`: still for 2 s.

1. **As found.**
   - Deck drop 0, both leaves under 0.02 rad, chiller latched, cab travel 0, lever under 0.02 rad.
   - No body in E has a carry kind.
2. **The brink (rule 1).** Ring220North, `seat_wet_spool`, D to 256.25, walk into the doorway (leaves over 0.3 rad on the body) and stand 10 s.
   - Chiller latched, cab travel under 0.005, deck drop under 0.005, lever under 0.05 rad.
   - `carry_target_entity_id` stays 0 and no pull, rig or grab is offered.
3. **The drop and ride** (same run, `drop_into_wet_e`):
   - landing on 2045 under 8 m/s; no death; the landing no worse than Heavy;
   - within 1.0 s of landing: deck drop at least 0.12 and catch released;
   - both leaves shut before the deck's top passes 256.0 (prints the margin; expected about 1 s); no leaf ever touches the rider;
   - launch peak upward acceleration at most 12 m/s² (first 3 s); at the top at most 16 m/s²;
   - peak deck at most 299.65 (the hard top never reached);
   - still within 25 s of landing; standing surface within 0.10 m of 298.25; still within 0.12 m at 30 s and 60 s after stilling;
   - support 2045 throughout, except at most 0.6 s airborne in total, only after the skirt reaches the collar;
   - gain at most 5000 g Δy(chiller) − 300 g Δy(bucket); prints the height and time where the chiller passes the rider's eye;
   - then the walk south onto F's platform (support 2050).
4. **Hang and let go.** Separate run via D.
   - Hanging 3 s: chiller latched and leaves shut over the hands.
   - Let go: within 1.0 s of landing the catch is released; the cab reaches the top as in check 3.
5. **Loads, rider not in E** (spawn WetFPlatform; `debug_launch_kit_body`, tests only, existing).
   - The first Toolbox (18 kg) set at (13.0, 256.4, −136.65): after 5 s the catch is latched, the deck proud again and cab travel 0.
   - The first Drum (55 kg) the same way: catch released, the cab launches empty and comes to rest within 0.15 m of check 3's height.
6. **Doors held, load thrown in.** Via D, the rider stands in the doorway with the leaves on the body, then the drum is set over the deck.
   - The catch is released and the deck's top never rises above 256.40.
   - After 3 s every E body moves under 0.2 m/s and stays so for 15 s: the car is jammed under the doors until the TP-340 dump.
   - The rider is never hit at or above `kHitFeltSpeed`, and walks back onto D's platform.
   - *As built (2026-10-20):* "every E body" could not hold: the 5 t chiller, hung on the air it has trapped, bounces on it as on a spring (about ±2.3 m, a 3.7 s period, dying away about 5 % a swing). The check holds the jammed car (cab, deck, both leaves, lever) under 0.2 m/s, and the chiller to staying in its travel, never onto a stop faster than 0.5 m/s, each swing slower than the last. The declared door interlock (risk 3) is built. See `AS-007_WET_ISOLATION.md`, "Changed in E's rebuild".
7. **Save mid-ride.** Save 8 s into check 3's ride, load into a fresh world: cab, deck, leaves and chiller as saved within 1 mm, and the ride ends within 0.02 m of check 3. A save from before the change is refused by the existing exact-size path, which run_save's "cut short" case already exercises.
8. **World guard** (section 4).

**`run_wet_band` (lines 268–327).**
- The E leg becomes `drop_into_wet_e` plus `wait_wet_e_top` with support 2045; the walk to F is unchanged.
- The re-arm adds: deck drop at most 0.01, `e_door_shut`, catch latched. Then 20 s with nothing released and cab travel at most 0.05 (the returning cab's jerk is not a blow).

## 6. Touch scenario: `_wet_e` in `/home/user/ScraperX/godot/presentation/ui/ui_test_bands.gd` (lines 356–396)

1. `_go(device, Vector2(13.2, -134.5), 0.15, 6.0)` on D's deck; `_pose("wet_e_brink")`.
2. From here until the cab is still, every frame: `_action_label()` must be empty (nothing to pull or take).
3. `_move_world(device, Vector2(0.0, -1.0))` until `not _native().is_player_grounded()` and `_position().y < 257.0` (4 s budget), then `_move_dir(device, Vector2.ZERO)`.
4. `_wait_until(support == 2045, 2.0)`, then `_wait_until(not e_catch_latched, 1.5)`, then `_wait_until(e_door_shut, 3.0)`.
5. `_wait_until(e_chiller_travel <= -24.0, 15.0)`, then `_pose("wet_e_pass")`.
6. `_wait_until(e_cab_travel >= 44.6 and abs(e_cab_speed) < 0.05 held 2 s, 30.0)`.
7. Assert:
   - support 2045;
   - feet within 0.15 m of 298.25;
   - `_main._audio.air_heard > 0`;
   - `_felt(&"heave") > 0` and `_felt(&"rumble") > 0`;
   - no death.

   Then `_pose("wet_e_top")`.
8. `_wet_f` as today. It starts at (13.4, −137.2), which is on E's deck, and steps down about 8 cm onto F.

touch_stack runs the same `_wet_e` (`ui_test_stack.gd:483-485`, unchanged). pad_wet and keyboard_wet stay dormant and untouched; they would fail if run.

## 7. Existing tests this touches, and what runs locally

| Test | Change |
|---|---|
| `tests/sim_tests_wet.cpp` | E section of `run_wet_isolation` becomes `run_wet_e`; `run_wet_band` E leg and re-arm; helpers replaced; `run_wet_route` unchanged (its `e_cab_travel < 0.05` still holds) |
| `tests/simulation_tests.cpp` | register `AS-007-E`; `AS-007` calls it |
| `tests/sim_tests_stack.cpp:647-651` (stack) and `tests/sim_tests_stack_route.cpp:68-74` (ascent) | swap the door, the trip and `e_cab_travel >= 41.95` for `drop_into_wet_e && wait_wet_e_top` |
| `godot/presentation/ui/ui_test_bands.gd` | `_wet_e` (touch_wet, CI line 336; touch_stack) |

- **Run locally, per the no-waste rule:**
  - native `SCRAPERX_ONLY=AS-007-E`, `AS-007`, `stack`, `ascent`, and `save` (the save layout changes);
  - touch: `touch_wet` and `touch_stack`.
- CI plays everything else on push.
- The full suite and the order-fragile touch list are run locally only if the world guard moves.

## 8. Risks

1. **The blow may not reach the deck.** Measure the deck's travel on landing first. The player is dynamic and falls into contact unglued, and support jolt counts only horizontal change. If the solver takes the impact before the deck feels it, the fallback is to pass the landing's momentum as an impulse to the support, the mirror of the jump's push-off (`physics_world_movement.cpp:1069`). That is shared physics: it reaches S1's plate and the slab incline's trip plate, so run the full native suite.
2. **The model is lumped; the kit is Jolt at 90 Hz.** Tune the collar's lower edge (rest height) and its area (overshoot) natively, never with a timer. Checks 3 and 5 pin the outcome.
3. **The doors jammed under load (check 6).** If Jolt jitters with the cell's force pressing the deck against the hinged leaves, the declared fallback is a door interlock. The cab's guide stops 0.1 m under the sill while a leaf stands open, and stays stopped until the cab is back at rest (the real thing on lifts). The ride then never meets an open leaf.
4. **The float at the apex** may set off airborne effects (landing sounds, a yelp). If so, lower the seal friction toward 2 kN (rest then 298.15).
5. **The world guard may move** despite the hand check. The remedy is the re-stage list.
6. **Saves from before the change are refused.** The owner should know.
7. **Glass on the Fold** is untested. Keep it to three batched spans.
8. **A rider who walks in while the car is up** falls 6.8 m into the pit (hard, not lethal). Their way out is the START list. A rider who lingers 10 minutes at the top sinks about 0.8 m and must jump or mantle off.
9. **The likeness to the first lift's scale plate** (rule 9) is the owner's question above.