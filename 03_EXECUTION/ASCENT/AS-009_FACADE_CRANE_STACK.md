# SCRAPERX — AS-009 FACADE CRANE STACK (B05, 484 → 640 m)

**Ascent Slice:** `AS-009`
**Lifecycle:** `BUILT` — stages J, K, L, the band run, the climbing route, every stage's climbable
wreckage and the goal's one-run ascent from the 154 m deck to TP-640 built and tested on player
inputs
**Provenance:** derived here under `03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md` and the Atlas's
B05 (`01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md`): exterior as ordinary play, a facade traveler on
rails with a missing rail joint (`MOD-TRAVELER`, `MOD-RAIL-JOINT`), a crane jib, TP-640.
**Implementation gate:** this write-up. The plan's rules §3 are this slice's test contract.
**Depends on:** `AS-008` in source and green (the rider stands on the 484 ring).

## Objective

The fourth band of the mechanism ascent, and the last of this goal: from the 484 ring to standing on
Transfer Plate TP-640. The frame has narrowed to a 17 m well; every machine works outside it on the
tower's faces, and every lift runs on a crane or a vehicle: a runaway rail wagon that drags a facade
traveler up its rails, a retired tower crane whose jib swings down like a pendulum, and a freight cart
whose run down an incline drives a two-drum winch. Each is found broken or unlinked; the player links
it and sets it off; the last is the band's cascade. A climbing route needs no lift.

## Existing truth (`c87a5a1`)

| Datum | Value |
|---|---|
| Frame rings | built to 484 on the taper: the ring at `330 + 22 k` has inner half-size `14.72 − 0.91 k` about `(0, −150)`, 4 m wide |
| The 484 ring | top `484.25`, inner half-size 8.35, outer 12.35 |
| AS-008's exit | I's cage parks at 462.25 by a ladder up the 484 ring's west inner face |
| Above 484 m | nothing. This slice builds the rings at 506 to 616, the columns, and TP-640 at 638.75–640.25 |

## Band layout

| Stage | Archetype | Travel | Payload | Energy source | Face |
|---|---|---|---|---|---|
| **J — traveler and runaway wagon** | 14 runaway vehicle tether, with `MOD-TRAVELER` and `MOD-RAIL-JOINT` | 484.25 → 528.25 | traveler 900 kg + rider | a 4 t rail wagon running 44 m down a 70° incline, direct tow | north |
| **K — crane jib pendulum** | 13 crane jib pendulum | 528.25 → 572.25 | cage 900 kg + rider | an 8 t jib swinging from level to hanging, through a four-part purchase | west |
| **L — kinetic winch** (the band's cascade) | 04 kinetic winch hoist | 572.25 → 640.25 | cab 900 kg + rider | a 4 t freight cart running 51 m down an 80° incline, through a two-drum winch (4 : 3) | south |

### Links between stages

- **J → K.** J's traveler parks at 528.25 by a board to the 528 ring; K's cage waits by the same ring.
- **K → L.** K's cage parks at 572.25 by a board to the 572 ring; L's cab waits by the same ring.
- **L → TP-640.** L's cab stops flush in a hole in the plate.
- **The cascade.** L's pin lets a drop weight fall onto the cart's catch lever; the cart runs; the
  winch hauls the cab.

## Stage J — traveler and runaway wagon (484.25 → 528.25)

- **Found:** a facade traveler on the north face at the 484 ring, its east rail missing the joint just
  above its rollers. The joint's splice block, 30 kg, lies on the ring; its tray beside the gap is
  empty. A 4 t wagon stands chocked at the head of a 70° incline off the east face, 44 m above the
  528 ring; its tow rope runs over a sheave at the incline's head and a head sheave over the
  traveler down to the traveler's eye.
- **Link:** carry the splice block onto the traveler and set it down in its tray.
- **Set off:** from the traveler, pull the wagon's chock lever over by its lanyard. The wagon runs
  down its incline and drags the traveler 44 m up its rails, governed.
- **No link:** the traveler's rollers cannot pass the gap; the chock pulled, the wagon holds on its
  rope, nothing moves.
- **Wreckage:** the wagon's run ends beside the 528 ring's east band, near where the traveler parks.
  It is built as a funicular car, its deck level and a grab bar plumb down its west side: from the
  ring the rider climbs the spent wagon onto its deck.

## Stage K — crane jib pendulum (528.25 → 572.25)

- **Found:** a retired tower crane on the 528 ring's west edge, its 24 m jib level over the void, held
  by its pendant pinned to the mast. The jib's hoist rope runs from its tip over a snatch block at the
  mast's head and a head sheave over the cage; its shackle hangs at the rope's end by the cage's eye.
- **Link:** hook the shackle on the cage's eye.
- **Set off:** from the cage, pull the pendant's pin out by its lanyard. The jib swings down to hang
  plumb from its heel; its tip pays out 11 m of rope through the four-part purchase and hauls the
  cage 44 m; safety dogs hold it.
- **No link:** the jib swings, the free shackle runs up, the cage stays.
- **Wreckage:** the jib hangs plumb from its heel just off the 528 ring's west edge, south of the
  mast. From the ring the rider climbs the chord along its underside 23 m to the heel and springs
  back from it onto the 550 ring.

## Stage L — kinetic winch, the band's cascade (572.25 → 640.25)

- **Found:** a 4 t freight cart at the head of an 80° incline off the south face, 51 m above the
  572 ring, held by a catch. Its
  rope runs to the small drum of a two-drum winch whose big drum hauls the cab. The winch's dog clutch
  is out. Over the catch's lever hangs a 200 kg drop weight on a pin.
- **Link:** throw the winch's clutch lever in.
- **Set off:** from the cab, pull the drop weight's pin by its lanyard. The weight falls on the catch's
  lever, the cart runs, the winch hauls the cab 68 m into TP-640's hole.
- **No link:** the clutch out, the drums turn free: the cart runs down, the cab stays.
- **Wreckage:** the cart's run ends beside the 572 ring's south band, east of the cab. It is built
  as J's wagon is: from the ring the rider climbs its grab bar onto its deck.

## Declared models

- **Rail joint.** A traveler's rollers cannot pass a missing rail section: its guide stops at the gap
  until the joint's splice block lies in its tray (within 0.25 m, any way up), as AS-007's pipe spool.
- **Dog clutch.** A winch's drums turn together only while its clutch lever is in; engaged, the rope
  takes up its length as it is at that moment.
- **Ratio purchases and winches** are the kit's ropes with a ratio (plan §4); the two-drum winch is a
  ratio of 3 : 4.
- **Rolling** on the inclines is a guide along the incline with a governor.

## Climbing route, no lift

Up the east band as in AS-008: an L of boards and a ladder per ring gap to the 616 ring, then a ladder
up through a hatch in TP-640.

## Falsifiers (lean)

`AS-009 J`, `K`, `L` (no link, no lift; linked, the ride; energy within the source's release);
`AS-009 band` (the 484 ring to TP-640 on player inputs); `AS-009 route`; and the goal's
`ascent` test, one run from the 154 m deck to standing on TP-640 through all four bands.

## Result record

Built on `ScraperX-Claude` after `c87a5a1`. Native falsifiers, all on player inputs:

| Group | Result |
|---|---|
| `AS-009 J` | joint out: chock pulled, the traveler holds at the gap and the wagon on its rope, 10 s. Joint in: traveler to 528.25 with the rider; gain 36.7 kJ against 1.62 MJ of wagon drop |
| `AS-009 K` | shackle free: pin pulled, the jib swings down, the free shackle runs up; cage still. Hooked on: cage to 572.35, a step above the 572 ring's board, jib at 1.57 rad; gain 36.8 kJ against 0.96 MJ of jib drop |
| `AS-009 L` | clutch out: pin pulled, the weight trips the cart, which runs down; cab still. Clutch in: cab to 640.25, flush in TP-640; gain 56.7 kJ against 1.98 MJ of cart and weight drop |
| `AS-009 band` | the 484 ring to standing on TP-640 in 96.0 s of sim time |
| `AS-009 route` | the 484 ring to standing on TP-640 in 183.9 s with no lift: six L's of boards and ladders, then the ladder through the plate's hatch. Every lift in the band where it was found |
| `AS-009 wreckage` | K fired with its shackle free: the jib hangs plumb at 1.5708 rad, climbed from the 528 ring to its heel and sprung from onto the 550 ring. J ridden: the spent wagon, 44.02 m down its incline beside the 528 ring, climbed onto its deck. L fired with its clutch out: the spent cart, 52 m down its incline beside the 572 ring, climbed onto its deck |
| `ascent 154 to TP-640` | one run from the 154 m deck to standing on TP-640 in 370.2 s of sim time, never dying: the 220 ring at 52.2 s, TP-340 at 156.0 s, the 484 ring at 272.3 s, twelve stages ridden, every link made by the rider's hands |

Changed on the way:

- **The rail joint is a splice block in a tray beside the gap, any way up:** a 1 m rail section carried
  by its middle turned in the hands and fell past its cradle.
- **J's chock lever stands east of the wagon's track and is thrown away from it;** thrown toward the
  wagon it jammed its run.
- **J's incline has no struts to the rings:** the strut at the 528 ring's height met the wagon.
- **K's cage is railed along its north and south sides:** boarded over a plank from the east, a rail on
  that side had barred the way.
- **The rider walks round K's shackle,** which hangs on 45 m of rope: bumped, it swings away for a
  quarter of a minute.
- **L's winch pedestal stands clear of the catch lever's counterweight;** since the incline's head
  moved, the winch hangs from TP-640's underside.
- **Each spent machine is a climb** (plan §3 rule 6). J's and L's inclines were steepened to 70° and
  80° and lifted, so each vehicle's run ends beside the ring below its head (the 528 and the 572),
  and both vehicles are funicular cars with level decks and plumb grab bars. K's jib swings in a
  plane south of the mast, from a heel just off the 528 ring's edge, to hang plumb on its stop: a
  catwalk along its top sets its weight so that it hangs hard on the stop instead of swinging
  about it, and its snatch block stands 15.56 m over the heel so that a hooked cage reaches its
  landing as the jib comes plumb. The cage's guide runs 0.4 m past its landing.
