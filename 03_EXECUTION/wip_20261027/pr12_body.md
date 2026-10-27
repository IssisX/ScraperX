Board v5, step 2: the authored map leaves the game. Her scan of the real collision world (the affordance map) is now her only map. The tests stop reading the old map and use an answer key of their own.

It's protected because it changes test checks. Cory has delegated the merge of the integrator's PRs once they're green.

## What leaves the game (d526c7cb, 7b33ec0e)
- `src/sim/matron_routes.cpp` and `.hpp` are deleted: the authored map with its regions, edges, plans and failure memory (-620 lines).
- Also gone:
  - `follow_route` and `RouteWay` in `matron_travel.cpp`;
  - the `route_edges` and `route_failures` views;
  - the `debug_matron_only_way` hook.
- The corner pipe keeps its own failure memory (`matron_pipe.cpp`, `pipe_ways_`). A failed pipe way is left alone for 30 s. After `kPipeWayRetries` failures it stays blocked until the world changes. This is the same rule the map held, now owned by the way it is about.
- 7b33ec0e only fixes spelling ("storey" to "story") in comments and one local name.

## Test checks changed (rule_check: 41 BLOCK, each explained)
The check first counted 9. It skipped deleted files; that is fixed in 864fa471, which lands with this. The 32 it missed are the deleted `matron-routes` group, covered after the table.

| rule_check line | Replaced by |
|---|---|
| `sim_tests_matron.cpp:2300`: `Matron::kLadderX/kLadderFootZ`, within 0.35 m | `kHerLadderX = 5.0`, `kHerLadderFootZ = -129.55` in `tests/sim_tests_decks.hpp`. Same numbers, same 0.35 m. |
| `sim_tests_matron_pursuit.cpp:95`: `route_edges >= 1` | Gone with the map, which it counted. The chase still requires her standing on the goal deck, judged by the test's own answer key (`deck_at`). |
| `pursuit.cpp:149`: `retook_after_s >= routes::kFailedBlockS - 1.0` | `>= kWayBlockS - 1.0`. Same 30 s, same message. |
| `pursuit.cpp:229` (3 lines): arrived; `route_failures == 0 && route_edges >= 1`; `worst_step < 0.2` | All stricter now. Her scan plans the way. Her journey counts an arrival (`arrivals > before`). She then stands 1 s on the far deck's steel, off any rung or pipe. Every move succeeds (`moves_failed` unchanged, `moves_done` up). Worst step stays < 0.2 m. Gravity factor stays 1 the whole way. |
| `pursuit.cpp:302`: `taken >= 10` ("every way she plans over") | `taken >= 10` ("every way between her decks that her scan finds"). The ways now come from her scan, not the map. A non-rung way is printed and skipped when a ladder's rungs reach the same region (the 152.22 bay bracket to 154 "mantle" is really climbed by rungs). |
| `sim_tests_matron_traverse.cpp:45`: `debug_matron_climb(connector)` | `debug_matron_climb_way(pipe, down)`: the corner pipe or the ladder nearest her, with no map connector. |
| `traverse.cpp:110`: `map.region_at(...) == end_region` | `deck_at(...) == end_deck`, the test's own answer key. |

The other 32 BLOCK lines are `tests/sim_tests_matron_routes.cpp`, deleted. That retires the `matron-routes` group, which tested the map itself, and the map is gone. By its four parts:
- **`run_regions`** (13 checks, lines 38-54), plus line 31 in its helper `qualify()`, which checked that a way is on the map: what the map's `region_at` answered at named spots.
  - The runtime has no `region_at` any more.
  - The tests' own answer key (`deck_at`, `sim_tests_decks.hpp`) takes its place. It is checked against the world below.
- **`run_directed`** (7 checks, lines 62-88): the map planned only over qualified ways, one direction each.
  - Her scan's plans are pinned instead. matron-ways requires that her scan plans every way between her decks, and that she takes each one.
- **`run_tops_are_steel`** (3 lines, 138-151): carried over as `run_decks_are_steel`, with the same method and the same 3 cm.
  - It requires every fixed patch of the answer key to be real steel (31 patches), so the key can't drift from the world.
  - The key gains the 176 m boards (0.2 m rise) and the 198 m catwalk, from `band_counterweight_well.cpp`.
- **`run_blocked`** (8 lines, 99-113): a failed way blocked for 30 s, then free; after the retries, blocked for good with its reason; then unblock.
  - The 30 s block is still pinned by the pursuit check above.
  - **Not covered yet:** the retry cap and the unblock. No test drives the pipe to fail past `kPipeWayRetries`. That is next in the integrator's queue as its own test.

## Proof
- CI run 40567623162 on 7b33ec0e.
- Locally on this code:
  - matron-ways: 10 ways arrived, each standing 1 s, worst step 0.17 m, failed 0.
  - decks-are-steel: 31 patches.
  - matron-traverse, matron-pursuit and matron PASS.
  - The prints match byte for byte with and without the map in the game (2b against 2c).
- A second agent reviewed f8a873a1 and failed it on three items:
  - (a) pursuit trusted a self-reported arrival;
  - (b) a mantle way that rungs really climb;
  - (c) matron-routes' checks dropped.
  5cc9418b fixes all three. The re-review goes on issue #3 before this merges.

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_01U92sp7quAMGdsPtBesvKaj
