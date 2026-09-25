# SCRAPERX — AS-007 WET ISOLATION (B03, 220 → 340 m)

**Ascent Slice:** `AS-007`
**Lifecycle:** `IN PROGRESS` — stages D, E, F, the band run and its cascade built and tested;
the climbing route is next
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
  the head tank (hung under the 242 ring) along the 220 ring and down into the tube's foot. One spool
  of it lies on the ring beside its gap. The fill valve's lever stands beside the platform.
- **Link:** carry the spool into its saddles. The pipe carries water only while the spool is seated
  (axis within 0.08 m and 0.15 rad of the gap's).
- **Set off:** from the platform, throw the fill valve over. Water runs tank → tube; the float rises
  and pushes the platform 36 m to its stop. An overflow at 219.0 keeps the tube below its lid.
- **No link:** throw the valve with the spool on the deck: nothing flows, nothing moves.
- **Re-arm:** the drain lever on the walkway empties the tube (the platform sinks back); the cascade
  refills the head tank.

## Stage E — chiller drop (256.25 → 298.25)

- **Found:** a cab in a sealed shaft, its floor a piston, its door hooked open toward D's platform. A
  5 t chiller hangs at the top of a sealed duct beside it on a catch; the ducts' plenums meet at the
  bottom through a throttle. A trip handle from the chiller's catch hangs in the cab.
- **Link:** shut the door behind you. It swings to and its spring latch drops. Open, it leaks the
  whole doorway; the cab needs 1.4 kPa and an open door holds under 0.1 kPa.
- **Set off:** pull the handle. The chiller falls; the air it pushes through the throttle lifts the
  cab 42 m to its stop, where the pressure holds it.
- **No link:** pull the handle with the door open: the chiller falls, the air leaves by the door, the
  cab stays on its stop. The chiller then waits for the cascade.
- **Re-arm:** the cascade's bucket hoists the chiller back; the vacuum breaker lets air in behind it.

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
  `0.6 A √(2 ρ ΔP)` (linear under 50 Pa). An open door leaks its area times its opening. A vacuum
  breaker admits air below `P_atm − 100 Pa`. Pistons are sealed to their shafts.
- **Hydraulics.** Incompressible and push-only: a pulley constraint with a minimum length, ratio the
  area ratio. Coupling takes whatever fluid the line holds then. The charge line pushes
  `ρ g Δh A` up the accumulator while the dump is open.

## Climbing route, no lift

On the well side of the rings, like AS-006's: a ladder up the head tank from a walkway on the 220
ring and a short ladder to the 242 ring; a board from the 242 ring (balance) to a panel on the 264
ring's face; a shimmy along the 264 ring's lip to a pipe to the 286 ring; a ladder to the 308 ring; a
controlled drop from the 308 ring onto a plank under a panel to the 330 ring; a ladder on TP-340's
north face.

## Falsifiers (lean, per the owner: one test each where it counts)

1. `AS-007 D` — no spool: valve thrown, platform still after 20 s; spool seated: platform to 256.25,
   rider's gain ≤ the water's released head.
2. `AS-007 E` — door open: handle pulled, cab still; door shut: cab to 298.25, gain ≤ chiller's drop.
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
| `AS-007 E` | door open: trip pulled, chiller falls 10 m+, cab still. Door shut: cab to 298.25 in about 28 s at 1.55 m/s on 1.3 kPa; gain 35.0 kJ against 2.07 MJ of chiller drop |
| `AS-007 F` | hose free: valve thrown, accumulator dumps 3 m, platform still. Coupled: platform to 340.25; gain 35.0 kJ against 588.6 kJ of accumulator drop |
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
