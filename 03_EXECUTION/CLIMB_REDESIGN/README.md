# The climb's new experiences: designs and every option found

Design work for the experiences that take the place of ladders and pulled starts on the route
(00_START_HERE.md directives, 2026-10-20). Nothing here is built until the owner has talked it
through. Every design a workflow produces is listed with its ideas, chosen or not, so the owner
can pick any of them (the owner, 2026-10-20: *"Maybe I'll like one that you didn't choose"*).

> **Archived 2026-10-29:** the workflows' verbatim JSON (`panel_results/`) is out of the tree;
> their designs, ideas and scores are below and in `glass_tube_designs.md`.
> `03_EXECUTION/ARCHIVE_INDEX.md` says how to get the JSON back.

## The chiller drop, 256 to 298 m

`chiller_drop_spec.md` is the panel's one spec. `panel_results/wf_de938b84-896.json` holds the
route map, the three designs and the judge's scores verbatim.

| Design | The idea | Judge (wow / honest physics / touch / rules / buildable) |
|---|---|---|
| 1. Drop in, blown out (chosen) | Walk through the doors onto no floor, drop 3 m onto the car's deck; the landing trips the 5 t chiller and its air drives the car up and out of the shaft's mouth | 9 / 8 / 9 / 7 / 7 |
| 2. The man engine | Two water-driven rods rock up and down out of step; step from one rod's step to the other's at the top of each stroke, 7 m at a time | 8 / 8 / 5 / 6 / 4 |
| 3. The glass air tube | The same machine with glass walls to watch the chiller fall; start it by throwing an iron flange to knock a trip weight loose | 5 / 6 / 6 / 5 / 7 |

Ideas from designs 2 and 3, and whether the spec takes them:

| Idea | From | In the spec? |
|---|---|---|
| The rider's own weight is part of the water balance on every stroke | 2 | no |
| Timed steps between two counter-moving rods | 2 | no (a 2.2 s window five times, 18 s lost per miss: the judge feared a chore; its bucket drive repeats S1's) |
| Work lamps riding up with the car, light climbing the wet walls | 2 | yes |
| Drips off the deck while it moves | 2 | yes |
| A rumble felt in the phone while the car moves | 2 | yes |
| A check that no zipline or stock piece meets any changed body | 2 | yes |
| Glass walls to watch the chiller fall | 3 | yes, as three large spans (cheaper on the phone than 26 panes) |
| A door on a leaning hinge that swings shut on its own weight | 3 | yes, as a pair of light leaves |
| Start by throwing an iron flange to knock a trip weight loose | 3 | no (the fall starts it) |
| Door held open: the machine spends itself | 3 | yes, as a test |

### Picked (2026-10-20)

The owner asked Claude to choose as the owner would: design 1 as specified, with the glass panels,
the self-closing doors, the lights riding with the car, the drips, the rumble and the float at the
top. Not taken: the throw start (the throw stays the Matron's) and the rider's weight as power. The
deck is far enough from S1's scale plate (a fall sets it off, not weight); old saves refused is
accepted for now. Built on `claude/chiller-drop` and merged 2026-10-21: played by touch
(`touch_wet`) from the float lift's stop through the doors and the drop to the deck at 298 m, nothing
offered to pull, the air heard and felt, and the seal's burst seen 3.7 m from the eye.
The owner confirmed the picks, and added: *"Make sure we see a whoosh of air/wind when vacuum seal
is broken on glass machine."* At the chiller drop's top, the seal's break is a visible blast of air
from the collar, not only heard; the glass spares tube does the same at its stations.

## The glass spares tube, 484 to 640 m

In place of the crane band's backup ladders; the runaway wagon, crane jib and kinetic winch stay
as built. The design workflow was stopped on 2026-10-20 when the owner lowered effort, with both maps
(the ladder route and the tower around it; what the engine can do) and one of its three designs
finished: `panel_results/wf_047fa3a0-b28_partial.json`. Left to do: the other two designs (the
machine-first and the build-first angles), then the options listed for the owner to pick from.

### Picked (2026-10-20)

The owner: *"Thrill ride and add your extras"*. Design 1 (the shove at lift-off, the surge past
6 m/s, the 14-degree leaning elbow round the corner, the curved glass door), with: the carrier's glass
lid, the glass misting and clearing with the whoosh, joint thuds and blower whine, NO RIDERS, torn
padding and a rack of dented carriers, the SEND panel re-hung on a bent bracket, the old hatch plated
over. Built after the chiller drop (it reuses its glass, vent renderer and sounds). Designs:
`glass_tube_designs.md`.
