# SCRAPERX — AS-007 WET ISOLATION (B03, 220 → 340 m)

**Ascent Slice:** `AS-007`
**Lifecycle:** `BUILT` — stages D, E, F, the band run, its cascade and the climbing route built
and tested on player inputs
**Provenance:** re-derived here under `03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md`. Replaces the
plan imported from `ScraperX-Grok` (their `WO-015_WET_ISOLATION`), whose geometry and ids describe a
world that does not exist here. The file name is kept so `MANIFEST.txt` stays true.
**Implementation gate:** this write-up. The plan's rules §3 are this slice's test contract.
**Depends on:** `AS-006` in source and green (the rider arrives on the 220 ring); Step 2 movement.

## Objective

The second band of the mechanism ascent: from the 220 ring to the top of Transfer Plate TP-340, the
top of Atlas band B03, *Wet Isolation*. Every lift in it runs on water or air: a float that rises
on a flooding tube, a chiller that falls through a sealed duct and drives a cab up on the air it
pushes, and a weight-loaded hydraulic accumulator. Each is found unlinked; the player links it and
sets it off; what each leaves behind feeds its neighbours; a header on TP-340 re-arms the band in one
cascade. A climbing route needs no lift.

## Existing truth (`573dd84`, `world_solids.inc`)

| Datum | Value |
|---|---|
| Ring decks | tops `220.25, 242.25, 264.25, 286.25, 308.25, 330.25`, 4 m wide |
| Ring inner edges, north band (z) | `-130.73, -131.64, -132.55, -133.46, -134.37, -135.28` |
| Ring inner edges, east band (x) | `19.27, 18.36, 17.45, 16.54, 15.63, 14.72` |
| North-face braces | `z ≥ -131.5` up to 335.7 m; nothing else stands in the well's NE quarter |
| AS-006 in the way | route catwalk and panel `x ∈ [9.97, 12.03]`, `z ≥ -132.0`, up to 220.2; boom and gallows at `x ≤ 0` |
| TP-340 | does not exist; built here |

Every B03 body keeps south of each ring's north inner edge and west of its east inner edge at the
height it reaches, so nothing rising meets a ring.

## Band layout

Plan view, north up. Column x ranges are the shafts' outer faces.

```
 z=-130.7 ── 220 ring ───── walkway ─────
          [ head tank 226-238 ][ D tube <220 / D platform 220-256 ]   x 5.5..11.45 | 11.55..14.45
 z=-135.2 [ bucket ][ drop duct 254-306   ][ E cab shaft 254-298  ]
 z=-138.4           [ accumulator 299-305 ][ F platform 298-340   ]
          ── TP-340 (338.75-340.25) over the well, a hole over F ──
```

| Stage | Archetype | Travel | Payload | Energy source | Kind |
|---|---|---|---|---|---|
| **D — float ram** | 02 buoyancy, as a float on a mast in a flooding tube | 220.25 → 256.25 | platform, mast and float 2 500 kg + rider | 180 t in the head tank, falling into the tube | re-armable |
| **E — chiller drop** | 05 pneumatic piston | 256.25 → 298.25 | cab 900 kg + rider | 5 t chiller falling 45 m through a sealed duct | re-armable (the cascade hoists it) |
| **F — accumulator** | 08 hydraulic accumulator, weight-loaded | 298.25 → 340.25 | platform 1 000 kg + rider | 20 t accumulator falling 3 m, a 1:14 line | re-armable (the header recharges it) |

### Links between stages

- **D → E.** D's platform parks at 256.25, flush with E's cab and 0.25 m from its door.
- **E → F.** E's cab parks at 298.25; F's platform waits 0.4 m south of its shaft's rim.
- **F → TP-340.** F's platform stops flush in the plate's hole.
- **The cascade.** On TP-340 the rider throws the header's dump. The riser feeds three places: a
  spout over E's hoist bucket, which fills, outweighs the chiller, sinks and hoists it back into its
  catch (the cab sinks back as the chiller draws its air), then tips out at the bottom; the charge
  line under F's accumulator, whose head lifts it back 3 m while F's platform sinks to 298 in
  exchange; and D's head tank, which refills. Pulling D's drain at 220 lets its platform down.

## Stage D — float ram (220.25 → 256.25)

- **Found:** a platform on a mast, flush with the 220 ring's walkway. The mast runs down through a
  gland in the lid of a sealed tube (181–220) to a float resting on the tube's floor. A pipe runs from
  the head tank (hung under the 242 ring) along the 220 ring, turns south off the ring east of the
  walkway, crosses the walkway's foot and drops into the tube west of it. Where it crosses the walkway
  one spool of it is out: the gap is the way onto the platform, and the spool (a length of pipe with a
  round flange at each end, 50 kg) lies on the ring west of the walkway. The pipe has no valve; its two
  ends at the gap are self-sealing couplings, shut until a spool's flange seats against them.
- **Link and set off, one act (2026-10-19; rule 11):** pick the spool up, carry it through the gap
  onto the platform, turn, and set it down in the gap behind you. A cradle of two rails across the
  gap takes its flanges: set down anywhere over them within a quarter metre, it rolls to the middle
  and lies in line with the pipe (seated within 0.28 m and 0.35 rad). Seated, it opens both couplings:
  water runs tank → tube, and within a second the float rises and pushes the platform, rider aboard,
  36 m to its stop. An overflow at 219.0 keeps the tube below its lid. (Until 2026-10-19 the spool was
  seated on the ring and the fill started by a valve bar thrown from a hanging handle on the
  platform; the bar's and handle's bodies are kept parked, so older saves still line up.)
- **No link:** walk through the gap with the spool left on the deck: the couplings hold the water,
  nothing flows, nothing moves.
- **Re-arm:** the drain lever on the walkway empties the tube (the platform sinks back); the cascade
  refills the head tank.

## Stage E — chiller drop (256.25 → 298.25)

Rebuilt on 2026-10-20 to `03_EXECUTION/CLIMB_REDESIGN/chiller_drop_spec.md` (the owner's design 1,
with the glass, the self-closing leaves, the lamps, the drips and the float at the top). Nothing is
pulled and nothing is taken in the hands: the rider walks in, drops and rides.

- **Found:** at D's top, a pair of hazard-striped leaves in the shaft's north wall, shut, on hinges
  leaning 8° top outward. Behind them is no floor: the car is 3 m down the shaft, a skirted cab
  whose skirt seals on the walls, and on its plate a 60 kg deck hung 0.15 m proud on the chiller's
  trip line. Beside it, the 5 t chiller hangs at the top of its sealed duct on a catch; the two
  cells meet at the bottom through a throttle. Glass from 258 to 296 m in the shaft's west and east
  walls and the duct's east wall.
- **Link:** none to make. Walking in pushes the leaves open; they swing shut on their own weight
  about a second after the body leaves them.
- **Set off:** the drop. Walking off the sill (2.85 m), the rider lands on the deck at 7.4 m/s; the
  blow carries the deck onto the plate and turns the trip lever past its release (0.12 m of deck)
  0.11 s after the landing. Standing on the deck, or a toolbox dropped on it, never does: the
  lever's counterweight holds the line to about 2 kN. A 55 kg drum dropped on it does.
- **The ride:** the chiller falls; its air drives the car up the shaft at 3 to 3.5 m/s after a heave
  of about 1 g. Through the glass the chiller rushes down past the rider at about 277.7 m. At the
  shaft's mouth the skirt rises into the slotted vent collar, the cell vents, the car floats a
  moment, bobs and comes to rest on its trapped air, held by its seal, level with F's platform:
  still about 22 s after the landing, the deck at 298.28 m.
- **No link:** a rider holding the leaves open while a load is thrown in: the door interlock stops
  the car with its deck 0.1 m under the sill, and it stays stopped until it is back on its bottom
  stop. The chiller hangs on the air it has trapped until the cascade.
- **Re-arm:** the cascade's bucket hoists the chiller back; the cab sinks onto its stop, the breaker
  lets air in behind it, and the deck's line goes taut again, leaving the deck hung proud.

## Stage F — accumulator (298.25 → 340.25)

- **Found:** a platform on a ram, and beside it a 20 t accumulator held 3 m up by its stop valve. The
  hose from the accumulator lies on the platform, its coupling free.
- **Link:** couple the hose to the ram's inlet on the platform (the hook verb).
- **Set off:** throw the stop valve. The accumulator sinks 3 m under a flow-limited governor; the
  line lifts the platform 42 m into the plate's hole. Its fluid then holds it there.
- **No link:** throw the valve uncoupled: the accumulator dumps its line through the open hose;
  the platform stays.
- **Re-arm:** the cascade's charge line.

## Declared models

- **Water.** Incompressible. A pool is a sealed prism: its level follows from its volume and the
  displacement of any float in it. Pipes flow `Q = 0.6 A f √(2 g Δh)` from the higher head to the
  lower, `f` the valve's opening; a spout pours into the first bin or pool below it. Water above a
  pool's overflow, and water spilled from a bin, drains away.
- **Buoyancy.** `ρ g A d` on a float, `d` its submerged depth clamped to its height, applied at its
  centre of mass, with water drag on its vertical speed.
- **Air.** Each cell is an isothermal ideal gas. A piston feels `(P − P_atm) A`. A throttle passes
  `0.6 A √(2 ρ ΔP)` (linear under 50 Pa). An open door leaks its area times its opening (a door
  pressed onto its frame past its shut stop is shut). A vacuum breaker admits air below
  `P_atm − 100 Pa`. Pistons are sealed to their shafts.
- **Door exposure (E).** A doorway in a shaft whose piston rides past it leaks, and its leaves are
  pressed, only by the share of its height below the piston's sealing edge; above the edge the
  skirt covers it.
- **Vent collar (E).** Once the skirt's sealing edge rises past the collar's lower edge (296.85 m),
  the cell leaks through 3.0 m² of slot for every meter of edge above it, up to the rim (298.0 m).
  The lower edge sets where the car comes to rest; the area, how far it overshoots.
- **Seal friction (E).** The skirt's seal drags on the shaft's walls with 2.5 kN (the guide's
  friction): it holds the car still on its trapped air while a 0.0005 m² bleed lets it sink a few
  millimeters a minute.
- **Door interlock (E, the spec's declared fallback for its risk 3).** While a leaf stands open more
  than 0.05 rad with the car at or below that stop, the car's guide stops with its deck 0.1 m under
  the sill; a car that reaches that stop with a leaf open stays there until it is back at rest on its
  bottom stop.
- **Hydraulics.** Incompressible and push-only: a pulley constraint with a minimum length, ratio the
  area ratio. Coupling takes whatever fluid the line holds then. The charge line pushes
  `ρ g Δh A` up the accumulator while the dump is open.

## Climbing route, no lift

West of the machines, on the well side of the rings, each climb facing the ring it tops out onto
(AS-006's rule): an L of boards out from the 220 ring to a ladder on the 242 ring's face (balance,
climb); a beam out from the 242 ring under a scaffold panel on the 264 ring's face, caught with a
jump; on the 264 ring a bulkhead leaves only the lip, so drop over the edge, shimmy under it and climb
back up, then boards to a standpipe to the 286 ring; boards to a ladder on the 308 ring's face; a
catwalk under a panel on the 330 ring's face, jumped for; a ladder on TP-340's north face, climbed
facing south.

## Falsifiers (lean, per the owner: one test each where it counts)

1. `AS-007 D` — no spool: valve thrown, platform still after 20 s; spool seated: platform to 256.25,
   rider's gain ≤ the water's released head.
2. `AS-007 E` (`SCRAPERX_ONLY=AS-007-E`, also in `AS-007`) — as found nothing hung, shut or
   takeable; 10 s standing in the doorway sets nothing off and offers nothing; the walk off the
   sill lands under 8 m/s, trips the chiller within 1 s, the leaves shut before the deck reaches the
   doorway, no leaf touches the rider, launch at most 12 m/s², the top at most 16 m/s², the deck
   peaks at most 299.65 m, still within 25 s with the deck within 0.10 m of 298.25 and within
   0.12 m 30 s and 60 s later, gain ≤ chiller's drop, then onto F; hanging from the sill and letting
   go does the same; a toolbox dropped on the deck sets nothing off, a drum sends the car up empty to
   within 0.15 m of the ride's rest; leaves held open and a drum thrown in: the car jams under the
   doors (cab, deck, leaves and lever under 0.2 m/s from 3 s to 18 s), the rider untouched, and the
   chiller's bounce on its trapped air stays in its travel and dies away; saved 8 s into the ride
   and loaded, every body within 1 mm and the ride ending within 0.02 m; the world guard (kit bodies,
   ziplines and stock where they were, none in E's shaft, duct or bucket column).
3. `AS-007 F` — hose free: valve thrown, platform still; hose coupled: platform to 340.25, gain ≤
   accumulator's drop.
4. `AS-007 band` — one run from the 220 ring to standing on TP-340 on player inputs, then the dump:
   chiller back in its catch, accumulator back up, head tank refilled.
5. `AS-007 route` — the climb from the 220 ring to TP-340 with no lift.

Touch is the only input surface this slice adds scenarios for (owner, 2026-09-24).

## Out of scope

Water on the player (no drowning, no current), sound, the Atlas stair flooding at 240.

## Result record

Built on `ScraperX-Claude` after `573dd84`. Native falsifiers, all on player inputs:

| Group | Result |
|---|---|
| `AS-007 D` | spool off: valve thrown, platform still 10 s, tube dry. Spool in: platform to 256.25 with the rider on it; gain 29.8 kJ against 74.6 MJ of water head released |
| `AS-007 E` | rebuilt 2026-10-20 (`claude/chiller-drop`): landing 7.40 m/s (heavy), catch released 0.11 s after it, leaves shut 1.0 s before the deck reached the doorway, launch 11.1 m/s², top 13.2 m/s², deck peak 299.56, the chiller past the rider's eye at 277.7 m closing at 6.1 m/s, still 21.8 s after the landing with the deck at 298.284 (298.281 60 s later); hang and let go: landing 4.1 m/s, released 0.24 s after it, rest 298.287; toolbox: deck 0.02 m down, nothing released; drum: empty rest 298.333; jammed: deck at most 256.37 m, the car still (0.03 m/s), the chiller bouncing ±2.26 m with a 3.74 s period, each swing slower (3.80 to 2.86 m/s), never onto a stop; save loaded exactly, rest 298.287; gain 35.0 kJ against 2.31 MJ |
| `AS-007 F` | hose free: valve thrown, accumulator dumps 3 m, platform still. Coupled: platform to 340.25; gain 35.0 kJ against 588.6 kJ of accumulator drop |
| `AS-007 route` | 220 ring to standing on TP-340 in 139.8 s with no lift: boards, ladder, beam, panel, drop, shimmy under the bulkhead, standpipe, ladder, panel, the plate's ladder. Every lift in the band where it was found |
| `AS-007 band` | 220 ring to standing on TP-340 in 102.8 s of sim time; the dump then hoists the chiller back into its catch (the cab sinks to its stop), recharges the accumulator (F's platform returns to 298.25) and refills D's tank (253 t) |

Changed on the way:

- **The door opens into the cab.** Hooked open outward, it swung across the approach from D and a
  rider walking in shut it on themselves. Opening inward, the rider pushes it wide going in, and
  the cab's air presses it onto its frame.
- **A catch now draws its body onto the seat's turn as well as its place** (`DEFECT
  mechanism_kit latch`): the door latched a few degrees open and leaked 30 % of its doorway.
- **The door's leaf stands 5 cm off each jamb:** swung inward, its hinge-side corner cut into the
  wall and the contact solver shut it.
- **Pools are sealed:** a pipe into a full pool stops. With an overflow, the refilled head tank
  drained straight back through D's open fill into the full tube.
- **E's vacuum breaker is on the cab's cell, with a small bleed:** on the duct's side it let the
  rising chiller take outside air instead of the cab's, and the cab stayed 19 m up.
- The spool's gap is 1.6 m between low guides on the deck (a carried spool swings); the handles
  hang where a rider reaches them without walking into them.
- **A shimmy passes under something standing back from the lip** (Step 2's shimmy had required the
  climb-up landing clear at every step; only the climb up needs it). The route's bulkhead is the
  case: it stands 0.15 m back from the 264 ring's edge.

Changed in E's rebuild (2026-10-20), against the spec:

- **The landing's blow reaches the deck** (the spec's risk 1, measured first): the deck went the
  full 0.15 m within 0.25 s of the landing and the catch let go 0.11 s after it, so the fallback
  (an impulse handed to the support) was not needed.
- **The bucket's striker lies 3 cm lower** (252.73 m, not 252.76): level with the bucket floor's
  middle, its arm sat inside the floor with the push-out a tie, and 5 m lower than first built the
  tie went the wrong way: the floor was pushed down onto the arm's stop and the bucket, and the
  chiller hung on it, could not move.
- **The leaves pass through the frame and each other.** On hinges leaning 8°, their hinged edges'
  corners sweep a few centimeters into the jambs as they open, and their lower free corners a
  centimeter or two past each other as they close; with those contacts the pair jammed at 0.6 rad.
- **The deck is 2.46 m square**, not 2.5: the cab's corner posts stand 1.24 m out from its middle.
- **A door pressed past its shut stop is shut:** the cell's air held the leaves a third of a
  milliradian past their stops, and the old rule (any turn either way opens it) leaked six times the
  bleed and let the car sink 0.6 m a minute.
- **The deck's guide is built in E's own place**, not after every other band: the solver orders an
  island's constraints by their place in its list, and one built last would be the one swapped into
  the first hole a constraint taken away leaves, moving another island's order. Built in the
  middle, it moves every later constraint along by one, keeping their order.
- **Seal friction 2.5 kN**, not 3 (the declared knob): with the guide in E's place the launch peaked
  at 12.4 m/s² (the trip lever's return lifting deck and rider off the plate just as the car
  starts), over the 12 bound; 2.5 kN gives 11.1 m/s² and a rest of 298.28 m.
- **The door interlock is built** (the spec's declared fallback for its risk 3): pressed against the
  open leaves the car's deck went 4 mm past the 256.40 bound and wobbled with the air's pressure.
- **Check 6 holds the car, not every body, still** (ruled with the owner's session, 2026-10-20): the
  5 t chiller cannot be still within 3 s of a jam; hung on the air it has trapped it bounces on it as
  on a spring (about 13.7 kN/m). The check holds the jammed car (cab, deck, leaves, lever) under
  0.2 m/s, and the chiller to staying in its travel, never onto a stop faster than 0.5 m/s, its
  bounce dying away swing by swing.
- **The touch scenario's "nothing offered" allows a climb:** near the top the shaft's south rim comes
  within mantle reach and CLIMB +1.1 M is offered; nothing to take, pull, hook or rig ever is.
