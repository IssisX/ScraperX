# The glass spares tube: three designs

## For the owner

### The three designs

**1. The thrill ride (from the earlier run).** You step into a cage with its padding torn out at a booth on the corner of the 484 m ring and press SEND. A blower winds up far above, the steel collars creak, and about 8 seconds later the floor shoves you up at 1.4 times your weight. You surge to 6.5 m/s with the glass joints thudding past like rail joints, a meter from the runaway wagon's rails. Halfway up, the tube leans 14° round the corner: the floor and your view tilt, the carrier turns a quarter turn, and the view swings from the far peaks to the yard 600 m below, until two latches clack and a curved glass door rolls round onto the 640 m plate, about 45 seconds after SEND.

**2. The real machine (new).** This is a freight vacuum lift built the way an engineer would: a 1.32 m glass tube with steel guide rails inside it, and a carrier with a glass lid you can look straight up the tube through. After SEND you stay held on the station's catches while the blower roars up and the glass above you mists over. The catches clack open only when the vacuum is a tenth more than your weight, so you rise firmly and smoothly (about 1.2 times your weight, no surge), with a magnetic rail brake humming louder up to about 4 m/s. The bend is gentler (10°), and you slow and hear more hiss there because bent glass seals worse. At the top, switches click, the catches take you, and a valve breaks the vacuum with a whoosh you can see: grit sucked into a grille, a gust out of the exhaust louvers, the mist clearing. About 55 seconds in all.

**3. The lean build (new).** It has the same booth and SEND, but the carrier rests with its lid in a flared bell at the tube's foot. As the blower winds up, a roaring draft streams up past you, the torn canvas strips flick upward and grit lifts off the floor. Then a thunk as the seal grips, and you're sucked up into the glass with a firm push (1.3 times your weight). You ride a steady 5 m/s on a floor that never tilts, while the tube makes its quarter turn slowly all the way up, wrapping round the corner from the peaks side to the yard side. At the top you creep in, the valve gives a whoosh at the head house, and you push out through a light glass door that swings shut behind you, about 47 seconds after SEND.

### Every good idea
Tags: [1] thrill ride, [2] real machine, [3] lean build. **(pick)** marks what I'd add to the lean build.

- Tube stands upright on brackets just outside the tower: up the wagon's face, round the corner, up the face over the yard [1][3]
- A sharp elbow round the corner (14° lean, floor and view tilt, carrier turns a quarter turn) [1]; a gentler 10° elbow [2]; a slow quarter-twist over the whole climb, so the floor never tilts [3]
- No bracket at the 528 m ring, where the wagon's wreck is climbed [1] (pick)
- The old hatch in the 640 m plate covered with a steel plate, built in the old ladders' own place [1] (pick)
- The whole tube built in the old ladders' place and drawn the way the game already draws its machines [3]
- Steel guide rails inside the glass, so the glass only seals [2]
- A glass lid on the carrier: look straight up the tube as it rushes at you [2] (pick)
- Catches that let go only at 10% more pull than the load: a smooth start, and an overload never moves [2]
- The flared bell at the foot: draft, thunk, sucked up into the tube [3]
- The 1.4× shove and the surge past 6 m/s [1]
- A magnetic rail brake that calms the air's bounce [2]; the lifts' own speed governor used as a rail brake to cap the speed and park the carrier [3]
- Slower and louder through the bend, where the bent glass seals worse [2]
- If power is lost, the carrier is held by its own vacuum and sinks slowly home [1][2][3]; catches on the rails stop it if it ever falls fast [2]
- Too heavy is refused with a lamp and TOO HEAVY on the button [1][2][3]
- A 30 kW blower option for a plainer, slower ride (75 s) [1]
- Later, when the welder or nail gun can break glass: a broken pane above the carrier stops it [2]
- NO RIDERS stencil over SEND, torn padding and hanging strips, a rack of dented spares carriers [1] (pick)
- The crew re-hung the SEND panel inside the doorway on a bent bracket; the old bolt holes show on the post [2] (pick)
- A curved glass door that rolls round the tube like a bank drive-through [1]; or a light glass door, pressed shut by the air during the ride, pushed open to get out, swinging shut by itself [3]
- The whoosh at the top: air sucked into the head's grille and a gust out of the exhaust louvers [2][3] (pick the gust)
- The whoosh at the foot: the draft before lift-off, and the vacuum letting go when the carrier comes back down into the bell [3]
- The glass mists as the vacuum takes hold and clears with the whoosh, because the air is drawn from inside the cloud band [2] (pick, subtle)
- Riding down too (about 70 s) [1]; or riders go up only, CALL sends it down empty, and the parachute is the way down [3]
- Joint thuds [1] (pick); roller clicks, slowdown-switch clicks and the brake's hum [2]; relay clack, blower whine, creaking collars, wind muffled inside the glass [1] (pick)
- The view tilting with the floor in the bend [1]; a shove felt in the phone at lift-off and a rumble while moving [1][3]
- Old saves keep loading [1][3]; nothing in the save changes at all [3]

### Recommendation
**The lean build (3), with the picks above.** It delivers the whole tube (glass, blower, seven levels, side to side round the corner, a whoosh at both stations, the story) with the fewest new parts and no change to any machine, save or other band.

The one real choice is the slow twist against the first design's sharp leaning elbow. The twist is what keeps the floor level and the build small. The elbow can be swapped in later as a change to the tube's path, if the carrier is allowed to tilt.

---

## Engineering notes (lean build)

**Route** (+x east, +z south; the crane band's docs call +z north)
- **Corner:** the route map's corner A, from the +x face round the (+x,+z) corner to the +z face.
- **Bottom station:** carrier floor at (13.8, 484.25, −141.0).
  - A sill runs from the ring's edge at x 12.30 to x 13.0.
  - Booth posts stand at z −141.65 and −140.35, with a 2.5 m guard on the carrier's −z side.
- **Path:** a Jolt `PathConstraint`, Hermite points every 12 m, 156.2 m long.
  - Upright to 488.25.
  - A uniform quarter-helix about the vertical line (9.0, −141.0), radius 4.8 m, from 488.25 to 632.25. That is a 3.0° lean and a 1.76 km radius of curvature.
  - Upright at (9.0, −136.2) to the top floor at 640.25, which is the path's hard end (docked). The head is closed at 643.5.
- **Glass:** 1.50 m bore, 40 mm laminated, 12 flat panes per section, in straight 12 m sections (1 cm chord error) with steel collars. Gasket lines every 3 m are drawn by the material.
- **Clearances to the glass:**

| Thing | Clearance |
|---|---|
| 484 ring | 0.66 m |
| J's wagon at 527 m | 0.71 m |
| J's track | 1.3 m (the elbow had 0.61 m) |
| J's head gear and ropes | 13 m or more |
| K and L | on the far faces |
| TP-640's edge | 1.01 m |
| AS-010's corner column | 2.1 m |

- **Brackets:** at 506, 550, 572, 594 and 616, plus the corner column at about 539 m, 2.3–3.2 m long. None at 528, because J's wreck is climbed from (10.3, 528.25, −142).
- **Top station:**
  - The door is in the tube's −z side, x 8.5–9.5.
  - A sill runs from z −137.0 to −138.0.
  - The rider steps off at (9.0, 640.25, −139).
  - The head house stands on posts at (7.6, −138.3) and (10.4, −138.3).
- **Hatch:** the hole at x 4.1–5.7, z −152.4 to −150.25 is plated over.

**Carrier and door**
- **Carrier:** a raw body of 380 kg, under the 400 kg grip threshold, so its bars never offer CLIMB.
  - It moves in translation only (the swing seat's DOFs), with the path's rotation left Free.
  - Bars are 1.28 m across, 11 cm clear of the panes. The 3° lean shifts the tube's axis 6 cm over the cage's height.
  - It is open on its −x to −z quarter: you enter from the ring and leave to the plate, and that quarter faces glass for the whole ride.
  - A level 40 mm glass lid sits at +2.3 m with a cup seal: 0.019 m² leak as found, 400 N drag. A level lid still seals at a 3° lean (+0.002 m²).
- **Door:** a raw 60 kg curved glass leaf on a hinge tilted 8°, so it falls shut. It opens outward only, against a stop on the inside. Air pressure holds it shut while the column is under vacuum, and there is no motor.

**Blower and air** (the kit's air law; one column, stored as P·V, V = 1.767·(156.2 − s) + 4 m³)
- **Blower:** a 45 kW exhauster with the curve ΔP = 5.5 kPa·(σ² − (Q/14)²).
  - It soft-starts over 10 s and then runs at full speed only. There is no speed loop, so nothing can hunt.
  - It has a check flap.
- **Head valve:** 0.10 m² for descent, 0.30 m² for the break.
- **Bell:** a 0.094 m² gap at rest, closing linearly over 0.30 m of rise.
- **Rail brake:** the kit guide's governor law applied to the path motor, braking only, rated 8 kN.
  - It ramps the launch at 1.5 m/s², caps speed at 5.0 m/s, and slows the approach at 1 m/s² to a 0.08 m/s creep.
  - Parked, the path friction is set to 8 kN.
- **Model results:**

| Case | Result |
|---|---|
| Rider (465 kg) | lifts at 8.6 s, floor peak 1.27 g, cruises 5.0 m/s at 2.8 kPa and 9.8 m³/s (about 39 kW at the shaft), docked at 43.7 s |
| Rail brake | peaks at 6.0 kN, at the final creep |
| Empty | 42.9 s |
| Rider plus 90 kg | 46.9 s |
| 300 kg of spares | 56.6 s |
| About 770 kg gross | never leaves the bell; TOO HEAVY after 15 s |
| Seal leak | still goes up to about 0.10 m² (5× as found), at 3.2 m/s |
| Blower trips | held by the column, sinks at 0.4 m/s |
| Empty CALL | about 69 s, ending in a 1.2 s whoosh at the bell |
| Energy | 1.0 MJ of air work for 0.71 MJ of height |

**How it starts without a pull**
- Action offers SEND within 1.2 m of the panel, with free hands, when the carrier is at that station.
- CALL is offered when the carrier is at the other end, and TOO HEAVY after a send that could not lift.
- If you are half in the doorway at lift-off, the rising floor tips you back onto the sill.

**The whoosh** uses three vents fed to the chiller drop's vent renderer, with an "inward" flag added:
- the bell ring: about 4.4 m³/s of draft before lift-off, and the vacuum letting go when a descent arrives;
- the head grille: the 0.30 m² valve opens while the blower is still at speed, so up to 14 m³/s rushes in during its 3 s run-down;
- the exhaust louvers, blowing outward.

Sound reuses the chiller drop's roar and blast, plus one new blower whine. The phone gives a heave when the seal grips and a rumble while moving.

**Body order**
- **Kit:** unchanged in count, order and entities. Entity 1011 keeps its `add_body` at `band_facade_crane.cpp:534`; only its 543 parts change. Frame 1010 and the world solids are untouched. The magnet test's rule that the yard crane's devices, weights and throwables are the kit's last bodies still holds.
- **New bodies:** two raw Jolt bodies, carrier 3040 and door 3041. They are built after the impact ledger (around line 386 of `physics_world.cpp`), before the tests' extra solids at line 389. The range 3040–3099 is free: the kit is under 3000, the crew 3100, the Matron 3200.
- **Constraints:** the path and the hinge are moved to the front of Jolt's constraint list at build: remove all, add these two, re-add the rest in order.
  - Jolt orders an island's constraints by priority, then by list index (`ConstraintManager.cpp:45-55, 103-114`).
  - A removal moves the last constraint into the hole. The body-order fix covers contacts only.
  - **Catch:** the chiller drop's late deck guide (`build_wet_late`) lands at the end of that list the same way. Its spec doesn't account for this, and it needs the same treatment.
- **Save and checkpoint:** the tube is left out of both, as the crew and the Matron are. No checkpoint is committed while you stand on the carrier (next to the Matron's freeze at `physics_world.cpp:809-814`).
  - A mid-ride save or death puts you back on the sill, with the carrier at the bottom where it was built.
  - Old saves load, except one committed on a removed board (x 1.9–8.5, z −150.5 to −148.8, 484–640 m), which is refused.

**Tests that change**
- **Native:** `run_crane_route` (`tests/sim_tests_crane.cpp:283`; group `AS-009-route` at `simulation_tests.cpp:189` and `:349`; CI line at `wo000-delivery-spine.yml:153`) is replaced by two tests.
  - `run_crane_tube`: lift-off in 7–11 s, peak 3 m/s² or less over 0.3 s, no carrier–glass contact, docked within 50 s, out through the door to standing above 640.2, no deaths, rider's gain no more than the air work, and J, K and L as they were found.
  - `run_crane_tube_limits`: empty send then CALL, too heavy, a leak, a blower trip, half in the doorway, door blocked, a mid-ride save loading on the sill, an old save loading, a save on a removed board refused, and the ride run twice bit-identical.
- **CI:** the route line becomes "AS-009 tube" and "AS-009 tube limits". `touch_tube` (spawn 22) is added.
- **Proof:** run the full native suite on the parent commit and on the change. Every line must match except the route line, including `save: bytes=` and `determinism words=` (`world_bits` hashes kit bodies only).
- **Also:** the existing touch scenarios for the crane band, the yard crane, the forklifts, the stack and saves must stay unchanged, and the new parts must pass the floating audit.

**Before building**
- **Chiller drop dependency:** the chiller drop is not built yet; its branch has no build commits, although the README says "built, then merged". The lean build needs its glass material, its glass batching (including the far batch, or 12 m panes draw opaque past 260 m), its vent renderer, sounds and phone shakes. Build the chiller drop first, or the tube brings those pieces.
- **Swapped map labels:** in `wf_047fa3a0-b28_partial.json` the two map labels are swapped: `maps.route` holds the engine map and `maps.engine` holds the route map.

Files are in /tmp/claude-0/-home-user-ScraperX/290f45ef-c41d-5b08-903d-c4b10970a44f/scratchpad:
- tube_two_designs_v2.py
- tube_tune.py
- tube_brake_descent.py
- twist_clear.py