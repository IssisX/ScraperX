# The broken bridge: first contact with the second tower, as built (2026-10-21)

The owner asked for the second tower to come into play, then for the new side branches to be
weighed and the strongest direction taken (00_START_HERE.md, rows of 2026-10-21). This is the
consolidation:

- **`dot/twin-tower-first-contact`'s move, taken whole.** The second shaft stands 66 m closer:
  its face at z -224, a 48 m gap at the Stack and 62 m at the 640-750 m decks. The native box,
  the drawn box, its relief and the regenerated collision moved together, with that branch's
  geometry audit (`tests/twin_geometry_test.py`) and native checks (`twin geometry`). The full
  native suite passed on the move alone (990 s, all three groups).
- **Its plain walkway at the 220 ring became a broken bridge.** The owner's brief says not a
  decorative bridge, and reaching the second tower is earned.
- **`feature/second-tower-crossing` is not merged.** It was built for the old position (its bars
  and landing would sit inside the moved shaft), it hung bars on drawn cables, and it had a ladder
  and no way back. Its idea is kept: the first crossing is by hook, at the height where the hook
  is found.

## What the player meets

On the 220 ring's north band, where the hook is found on its rack, the crew's bridge reaches out
toward the second shaft and stops. The near half comes 18 m out from the ring (to z -190), hung
from the 242 ring by two stays. The far half comes 12 m out from the shaft (to z -208), carried
on the shaft's brackets and hung from two eyes in its face. Between them, 18 m of open air, 220 m
up. Each half is two lanes wide.

- **The gap.** A walk off the near tip falls. There is nothing to jump to.
- **The swing.** Over the gap's middle reaches the shaft's old erection arm, with a yellow bar
  across its tip. Climb the near step (1.2 m, at the tip), hook the bar, step off. The line swings
  you down under the arm and up to the far half; at the top of the swing you are over its deck,
  and DROP lets you down onto it.
- **Why it works.** A pendulum hung over the middle carries a body to the height it started from,
  as far beyond as it started before. From a standing start your feet sweep a circle that clips
  the far tip's edge. From the step it clears that edge, and the swing tops out over the deck.
- **The way back** is the same, from the far step: hook the bar, swing, DROP onto the near half.
- **The leaf.** The drop-in span that closed the gap stands raised on the far half's west lane,
  21.3 m tall, leaning toward the gap, held by a catch. Its trip is a steel flag hung in a hood
  behind it, with the hood's back to the gap and its mouth to the shaft. From the shaft's side,
  a hook shot into the hood swings the flag in, the catch lets go, and the leaf comes down across
  the gap onto the near half under its hydraulic buffer in about 6 s. It stays down. From then
  on the gap is a walk both ways, through deaths, saves and loads.
- **What tower one cannot do.** The trip cannot be thrown from the near half: a shot there meets
  the raised leaf or the hood's back, and from the 242 ring above the flag is past the line's 30 m.
- **From the ground, nothing.** The highest surface a body can reach near the shaft from the
  ground is 2.6 m up, measured over 65,366 standable surfaces sampled on a 1 m grid. The bridge
  at 220 m is far past the hook's 30 m.

## The numbers (from the native test, `SCRAPERX_ONLY=twin`)

| Quantity | How | Value |
|---|---|---|
| Line from the step to the bar | the bite sets it: chest to bar | 13.35 m |
| Speed at the bottom of the swing | √(2 g × drop), drop ≈ 4 m below the start | about 9.9 m/s |
| Line's pull in the swing | m (g + v² / L) | about 1.5 kN, under the bite's 6 kN hold |
| The snap as the line comes taut off the step | the pull over 6 kN, summed over each run of ticks over it | 348 N s over, 340 N s back; the bite tears at 500 N s |
| Top of the swing over the far deck | as run, from a walk off the step | about 3 m over it |
| The leaf coming down | let go to lying across (0.14 rad lean, 6 t, buffer 2.5 per second) | 5.6 s |

## Where it lives

| Part | File |
|---|---|
| The halves, the arm, the steps, the leaf, the hood and flag, the catch | `src/sim/twin_tower.{hpp,cpp}` (frame entity 1949, leaf 2660, flag 2661, appended after every older kit body, so older saves load) |
| The shaft's place | `TwinTower::kCenterZ`, mirrored in `godot/presentation/main_world.gd` (`TWIN_TOWER_CENTER`, `TWIN_TOWER_FACE_Z`) |
| Native tests | `tests/sim_tests_twin.cpp`: `twin geometry` (face, no ghost mass, the ground can't reach it), `twin` (gap, hood, hook over, hook back, leaf down and walked both ways, save and load) |
| Touch | `touch_twin` (`godot/presentation/ui/ui_test_rides.gd`), in CI's list |

## Surviving objections

1. **The snap.** Stepping off the step drops about 2 m before the line comes taut: a jolt of
   about 345 N s against a 500 N s tear. A running jump off the step would come closer to it.
   Not yet seen on the phone.
2. **No sound or feel yet** for the swing's snap, the flag's clank or the leaf coming down. The
   leaf and flag are drawn as kit bodies, but nothing is heard yet.
3. **Rule 9.** A leaf that comes down across a gap is kin to the bay's walkway leaf. Here it is
   released by a hook shot and stays down as the earned way across, not a trap.
4. **Not played on the owner's phone.** Aiming at the bar 13 m away by thumb was not measured;
   the bar is 2.8 m long to make it forgiving.
5. **The second tower's own climb** above its 220 m landing is still dot's short incline and
   plinth. The proposal's top slice (`PROPOSAL.md`) is the next way up it.
