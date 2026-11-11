# The bait lift: one test spot (a plan for Cory to OK; no game code)

The idea under test: **she is the heaviest thing he can move.** One spot where letting her get close is what
lifts him. If it is fun in the hand, the idea is worth more spots. If not, it is two platforms to take out.

## What the player sees

Off the east edge of the 684 deck hang two steel platforms side by side, level with the deck: a near one and a
far one. He walks across the near one onto the far one and waits. She comes after him. The moment she stands
on the near platform it sinks under her, and the rope over the head pulls his platform up. She goes down one
floor to the 662 deck; he goes up one floor and steps off onto the cooling plant's floor. He is two floors
ahead.

If he is not on the far platform when she steps on, she still drops a floor, but the lift is spent and he
climbs the usual way. If she is not hunting, nothing happens and the usual ways are all still there.

## Where

- Near platform (A): 2.6 m square, at x -1.9 to 0.7, z -140.9 to -138.3, level with the 684 deck (684.1), just
  outside its east girder (`kUpperEast` = -2.0). Under it is a clear 22.0 m to the 662 deck (662.1), which
  covers the whole footprint.
- Far platform (B): the same size, beside A at x 0.8 to 3.4. It rises 22.0 m to 706.1, level with C5's plant
  floor, whose north edge is at z = -141 (`kC5FloorZ1`). He steps south onto it.
- C4's parts on that side stop at z = -149.8 and the 662 deck's hatch is at x 8.1, so the drop is clear of
  them. Not traced: C5's tank and second platform above 706 near that corner, and the plant floor's north
  rail. The builder checks those first.

## How it works (the pieces N already uses)

Two bodies on vertical guides, one rope over the head between them, a chain hanging from A's underside to the
662 deck, safety dogs on B, friction on the guides: `add_guide`, `add_rope`, `add_chain`, `set_dogs`,
`set_guide_friction` in `band_midstack_service.cpp`. No handle, no latch, no catch, no script.

| | kg |
|---|---|
| A | 250 |
| A's chain, 2.65 kg/m x 22 m | 58 |
| B | 373 (65 more than A and its chain) |
| Guide friction, both guides | 400 N in all |

| Who is aboard | What happens | Why |
|---|---|---|
| Nobody | Stays | B is 65 kg heavier: it sits on its stop with 638 N |
| He alone on A | Stays | 85 - 65 = 20 kg, 196 N, under the 400 N of friction |
| She on A, he on B | A sinks, B rises | 220 - 85 - 65 = 70 kg, 687 N against 400 N |
| She alone on A | A sinks faster, B rises empty | 155 kg net; she lands at 6.4 m/s, under her 7.6 m/s limit |

With both aboard the chain sets down on the 662 deck as A sinks, so the pull fades: top speed about 1.8 m/s
halfway, near rest at the top, about 19 s for the 22 m. B settles on its dogs. That is the pace of the lifts.
These are sums, not a run: the builder puts them through the same one-line evaluator M, N and O used, over
friction from 250 to 600 N (below 196 N he alone would creep down; above 687 N it would not start).

Her side of it: she only stands there. Her weight is her 220 kg. No force of hers is spent, nothing is
written to her speed, gravity does the work.

## What it needs that does not exist yet

1. **One change in her scan.** Today she does not see anything that can move as floor
   (`matron_traversal.cpp`: the scan skips every dynamic body). So she would never step onto A. The change:
   a level platform resting on its guide's stop counts as floor. Her legs already cope with a floor that
   moves under her (the same file, the planted-leg code). This is the same reason she cannot follow him onto
   any cage today.
2. **Her way back up from the 662 deck.** Today there is none she fits, so she would stand there for good.
   Any of the ways up for that floor does it.
3. **A mesh shaft round A's drop**, so she cannot walk off the platform mid-ride and fall a floor (her
   injuries persist).

## What could go wrong

- **It skips the gravel hoist.** It lifts him 684 to 706, N's floor. It only works with her on his heels, and
  N stays the way up every other time, but on this floor it is a second lift. If that is not wanted, the same
  pair fits beside C4 or C5, which are climbs.
- **Both of them on A:** it sinks at once, landing at about 8.9 m/s, a 4 m fall's worth. He lives; it is under
  her 4.6 m drop.
- **She may wait instead of stepping on.** If her hunt holds off when he stands still, the bait never takes.
  That is the first thing to watch on screen.
- **He jumps off B mid-ride:** it becomes "she alone"; B runs up empty.

## What "fun" means here

Play it ten times. It passes if he chooses to wait for her on purpose, and the wait feels like nerve, not
like a queue.
