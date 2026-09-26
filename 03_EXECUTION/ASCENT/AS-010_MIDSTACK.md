# AS-010 — Midstack Service, first lift

Atlas band B06 starts at TP-640. This slice is one machine, not the band.

## The effect

A rider who starts standing on TP-640 ends standing on the service cage, 22 m higher, because an 800 kg skip fell 22 m.

The landing above that cage, the rest of the band, and a climb that needs no lift are not built.

## The chain, built backward from that effect

The rider is supported by the cage floor at the top of its guides. The cage moved up the guides because the rope, hooked to the cage's eye and run over the head sheaves, was pulled by the skip. The skip was held in a catch until the rider pulled the trip handle. As found, the rope's free end is on the bollard, not on the cage, so pulling the handle does not lift anything.

| | |
|---|---|
| Output | Cage travel 22 m, floor top 662.50 m. Rider grounded on the cage, above 662.2 m. |
| Receiver | The cage floor. The next machine, which would take the rider off this landing, is not built. |
| Source | Skip, 800 kg, caught 22 m above its buffer. |
| Restraint | Catch on the skip, held by the counterweighted lever. |
| Link the rider supplies | Shackle off the bollard, onto the cage eye. |
| Trigger | Trip handle, pulled from inside the cage. |
| Transmission | Rope, two sheaves, 1:1. |
| Governor | Friction on the cage guide, 2.5 m/s, brake only. |
| Masses | Cage 350 kg. Rider 85 kg, the mass in the simulation. Skip 800 kg. |
| Energy | Skip releases about 173 kJ over 22 m. Cage plus rider gain about 94 kJ. The rest goes into the guide brake and the buffer. |
| Re-arm | The skip is still a body at the bottom. Putting it back needs another machine, which this slice does not build. |
| Open, upstream | None for this lift. The player is already on TP-640. |
| Open, downstream | A floor and a way off the cage at 662 m, then the band's other two lifts and the no-lift climb to 780 m. Not built. |

## Proof

`SCRAPERX_ONLY=AS-010` passed locally: rider_y=663.335, cage_rise=21.95 m, skip_drop=21.95 m, source 172 kJ, gain 94 kJ. Unhooked, the cage did not rise. The same ride also starts from where the crane steps off onto the 640 m floor (`Plate640Dismount`, x=-3, z=-156.5) and ends at the same height.

- Rope left on the bollard, catch tripped: the cage does not rise.
- Rope hooked to the cage, catch tripped: the cage rises 22 m with the rider on it, and the skip's lost energy covers the cage and the rider.
