# SCRAPERX — AS-008 PLATE SHOP (B04, 340 → 484 m)

**Ascent Slice:** `AS-008`
**Lifecycle:** `BUILT` — stages G, H, I, the band run and the climbing route built and tested on
player inputs
**Provenance:** derived here under `03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md` and the Atlas's
B04 (`01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md`): constructive structure as the main verb, a transfer
girder, K-braces, plate tables.
**Implementation gate:** this write-up. The plan's rules §3 are this slice's test contract.
**Depends on:** `AS-007` in source and green (the rider stands on TP-340).

## Objective

The third band of the mechanism ascent: from TP-340 to the 484 ring, where the Facade Crane Stack
(B05) begins. Every lift in it runs on collapse or a lever: a scaffold tower slumping down its shaft,
a transfer girder tipping as a plate trolley rolls past its pivot, and a domino that trips a 20 t
monolith whose fall hauls a cage. Each is found unlinked; the player links it and sets it off; what
each leaves behind is climbable. A climbing route needs no lift.

## Existing truth (`575d2b0`)

| Datum | Value |
|---|---|
| TP-340 | top `340.25`, `x ∈ [-14.6, 14.6]`, `z ∈ [-164.5, -135.6]`, a hole over AS-007 F at `x ∈ [11.6, 14.4]`; the header on it at `x ∈ [2, 10]`, `z ∈ [-152, -143]` |
| Frame rings (`world_solids.inc`) | tops `352.25, 374.25, 396.25`; inner half-size `s = 14.72 − 0.91 k` about `(0, −150)` for the ring at `330 + 22 k`, outer `s + 4` |
| Face braces | diagonals on the north and south faces between the rings, on the rings' outer edge lines (`z = −133.1` at 374–385); nothing on the east and west faces |
| Frame columns | four corners to 418 m |
| Above 418 m | nothing. This slice builds rings at 418, 440, 462 and 484 and columns to 484 on the same taper |

## Band layout

The stages climb the well's west half; the route its east band.

| Stage | Archetype | Travel | Payload | Energy source | Kind |
|---|---|---|---|---|---|
| **G — scaffold slump** | 15 cascading scaffold collapse | 340.45 → 374.25 | platform 800 kg + rider | a 3 t scaffold tower slumping 34 m down its shaft | one-shot; the slumped tower's posts are a climb |
| **H — girder tip** | 18 cantilever beam tip | 374.25 → 418.25 | platform 800 kg + rider | a 14 t plate trolley rolling out along a 4 t girder that tips 1 rad, through a four-part purchase | one-shot; the tipped girder's handrails are a climb |
| **I — domino and monolith** (the band's cascade) | 03 leveraged impulse, as a cascade | 418.25 → 462.25 | cage 900 kg + rider | a domino tripping a 20 t monolith that falls 57°, through a four-part purchase | one-shot; the fallen monolith lies over the well |

### Links between stages

- **G → H.** G's platform parks at 374.25 beside the 374 ring's west band; H's platform waits by the
  same band, reached over a 0.3 m gangway (balance).
- **H → I.** H's platform parks at 418.25 beside I's cage.
- **I → B05.** I's cage parks at 462.25 beside a ladder up the 484 ring's inner face.
- **The cascade.** One pin sets the domino falling onto the trip lever's arm; the arm's pawl lets the
  monolith go; its fall hauls the cage.

## Stage G — scaffold slump (340.25 → 374.25)

- **Found:** a platform on TP-340. Beside it a scaffold tower, 34 m tall, stands pinned in its shaft
  with its foot at the 374 ring's height. The rope runs from the tower's head over two sheaves at
  409.5 m down to a shackle made fast on a cleat on the plate.
- **Link:** take the rope off its cleat and hook it on the platform's eye.
- **Set off:** from the platform, pull the prop's pin out by its lanyard. The tower slumps; the platform
  rises 34 m, governed.
- **No link:** pin pulled with the rope on the cleat: the tower hangs on the cleat, the platform stays.

## Stage H — girder tip (374.25 → 418.25)

- **Found:** a transfer girder on a pivot 5.25 m above the 374 ring's north band, its outboard arm
  13 m over the well. Its tail is tied down to the ring by a drop pin. A 14 t plate trolley stands
  on the inboard end, held by the pawl of a chock lever beside the track. The purchase runs from the
  girder's tip to the platform's eye.
- **Link:** carry the tail pin out of its socket (from the ring).
- **Set off:** from the platform, pull the chock lever over by its lanyard. The trolley rolls out;
  past 2.5 m the girder tips and hauls the platform 44 m.
- **No link:** chock pulled with the tail pinned: the trolley runs to the tip, the girder stays, the
  platform stays.

## Stage I — the domino and the monolith, the band's cascade (418.25 → 462.25)

- **Found:** on the 418 ring's north band a 20 t monolith leans 30° over the well on the edge of its
  foot, held by the pawl of a trip lever. East of it a 5 m domino stands pinned, leaning toward the
  lever's arm. The purchase runs from the monolith's head over a gallows sheave at 441.5 m and a head
  sheave over the cage; its shackle hangs at the rope's end a metre from the cage's eye.
- **Link:** hook the shackle on the cage's eye.
- **Set off:** from the cage, pull the domino's pin out by its lanyard. The domino falls onto the
  lever's arm; the pawl lets the monolith go; it falls toward the well and hauls the cage 44 m up its
  guide; safety dogs hold it there.
- **No link:** pin pulled with the shackle free: the cascade runs, the monolith falls to its stop and
  the free shackle runs up to 0.8 m under the head sheave; the cage stays.

## Declared models

- **Ratio purchases** are the kit's ropes with a ratio: a four-part purchase moves its load a quarter
  as far as its hauling part with four times the pull (plan §4).
- **Rolling** is a low-friction slide: the trolley's wheels are a friction coefficient of 0.01 on the
  girder's 0.05.
- **Collapse** is rigid: the tower slumps as one body down its shaft; the domino and the monolith
  fall as rigid bodies on hinges at their feet's leading edges.
- **Pawls** are the kit's catches: a pin catch holds its body while its pin is in its seat; a lever
  catch holds its body until its lever passes the release angle.

## Climbing route, no lift

Up the well's east band at `z = −158`, each climb facing the ring it tops out onto: a ladder on the
352 ring's inner face from TP-340; then on each ring an L of boards out into the well to a ladder on
the next ring's inner face, which stands 0.91 m further in with the rings' taper; to the 484 ring.

## Falsifiers (lean)

`AS-008 G`, `H`, `I` (no link, no lift; linked, the ride; energy within the source's release);
`AS-008 band` (TP-340 to the 484 ring on player inputs); `AS-008 route`.

## Result record

Built on `ScraperX-Claude` after `f0c8d6b`. Native falsifiers, all on player inputs:

| Group | Result |
|---|---|
| `AS-008 G` | rope on its cleat: pin pulled, the tower hangs on the cleat, platform still 10 s. Rope on the eye: platform to 374.25 with the rider; gain 28.2 kJ against 1.00 MJ of tower drop |
| `AS-008 H` | tail pinned: chock pulled, the trolley runs out, girder and platform still 15 s. Tail pin out: platform to 418.25, girder at 1.0 rad; gain 36.7 kJ against 1.73 MJ of trolley and girder drop |
| `AS-008 I` | shackle free: pin pulled, the domino trips the monolith, which falls to its stop; cage still. Hooked on: cage to 462.25; gain 36.7 kJ against 0.91 MJ of monolith and domino drop |
| `AS-008 band` | TP-340 to standing on the 484 ring in 116.0 s of sim time: G, the tail pin, the gangway, H, I's shackle and pin, the cascade, the ladder from the cage |
| `AS-008 route` | TP-340 to standing on the 484 ring in 168.9 s with no lift: across the plate, a ladder, then six L's of boards and ladders. Every lift in the band where it was found |

Changed on the way:

- **The lanyards' handles are light T-handles** (0.5 kg): a 3 kg handle's weight on its line drew
  the pins out of their seats while no one touched them.
- **H's girder is pinned down by its tail**, not propped by a folding K-brace: the brace's foot dug
  into the ring as it folded, and folded it would have lain under the tipping girder.
- **H's chock is a pawl lever beside the track.** A loose chock on the sloping girder crept out of
  its seat; a stop bar across the track fell back in front of the rolling trolley.
- **H's inboard arm stops short of the north face's braces:** built 3 m long it ran through one, and
  the girder could not turn.
- **H's pivot stands 5.25 m above the ring:** at 2.75 m the tipping girder's underside met the ring's
  inner edge at 0.86 rad, 5 m short of the ride.
- **The domino and the monolith hinge on their feet's leading edges:** hinged at their centres, a
  corner drove into the ring as they fell.
- **The trip lever's counterweight hangs north of the domino's fall;** the domino had come to rest on
  it with the arm short of its release.
- **I's shackle hangs at the rope's end by the eye, and the monolith's stop pays out 11.1 m of rope:**
  free, the shackle then stops short of the head sheave instead of running into it.
- **The band ends with a climb:** the cage parks at 462.25 beside a ladder up the 484 ring's inner
  face.
