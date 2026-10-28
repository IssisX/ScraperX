# How the Matron moves: one skill, six movers, and the retirement plan

Written 2026-10-27 against `ScraperX-Claude` `711bc2d6`, for the owner's question of that morning: *"it seems
like we are doing each ladder, each crate, each object its own shit rather than a general skill that
allows Matron to understand it all ... am I right? ... are we making real progress or spinning our
wheels?"* Every claim below names its file and line or the command that produced it.

## The verdict, in three lines

1. **The general skill exists, is on trunk, and works.** One scanner reads the real collision world at
   load, one set of physical moves climbs what it finds, and one planner strings them together. It
   climbed the whole 154 to 220 m chain and 100 structures nobody authored, clean, on this commit.
2. **What is still per-object is the wrapper she lives in, not the skill.** Her 4,818-line state
   machine still carries a hand-built corner-pipe executor pinned to one riser's coordinates, an
   authored walking graph per deck, an authored route map, and a priority chain that picks between
   six separate movers every tick. Fixes land in that wrapper and regress a neighbor.
3. **Not too far gone.** The general skill was bolted on in five days and already drives the tower.
   The remaining job is a retirement plan for the wrapper, step by step behind exams that already
   exist. It is a code job for the implementation sessions, with the steps and finish facts below.

## What is general today (proof run on this tree, 2026-10-27)

The skill lives in `src/sim/matron_traversal.hpp` (the contract) and `matron_traversal.cpp`:

- **Scanner** `AffordanceMap::scan` (`matron_traversal.hpp:356`): reads every static and kinematic body
  in the tower's bounds, tags walkable tops, rungs and ladders, poles and lips, and joins them into
  regions and directed edges with her real limits. Run once at tower load
  (`physics_world.cpp:1370`). No authored list, no coordinates.
- **Moves** `Climber` (`matron_traversal.hpp:194`): walk, rung up and down, mantle, step down, pole up
  and down, jump-catch, hang-drop, gap jump. Every force comes out of one `Budget` (8,000 N, 22 kW),
  gravity stays on, a hold is a body plus sub-shape plus anchor checked every tick, a failed move is
  a fall.
- **Planner** `AffordanceMap::plan` (`matron_traversal.hpp:374`): a directed search over the scanned
  edges, with edges a journey could not start avoided.
- **Exams, not tables** (`tests/matron_exam.hpp`): a seeded generator builds ladders, poles, panels and
  ledge stacks in an empty world; a judge enforces the laws every tick. Runtime code never reads a
  seed.

Run here, on `711bc2d6`, `build/host/scraperx_sim_tests` built from this tree:

```
$ SCRAPERX_ONLY=matron-climb-scan build/host/scraperx_sim_tests      (44 s wall clock, exit 0)
  leg to 132:    54.85 s, moves 3,  failed 0, slips 0, peak force 8000 N
  leg to 143:    15.02 s, moves 5,  failed 0, slips 0, peak force 8000 N
  leg to 154:    24.55 s, moves 6,  failed 0, slips 0, peak force 8000 N
  leg to 176.25: 47.60 s, moves 7,  failed 0, slips 0, peak force 8000 N
  leg to 154:    38.90 s, moves 8,  failed 0, slips 0, peak force 4586 N
  leg to 143:    32.28 s, moves 9,  failed 0, slips 0, peak force 8000 N
  leg to 132:    20.22 s, moves 11, failed 0, slips 0, peak force 4153 N
  leg to 176.25: 42.40 s, moves 1,  failed 0, slips 0, peak force 8000 N
  leg to 198.25: 46.45 s, moves 2,  failed 0, slips 0, peak force 8000 N
  matron 198.25 -> 220.25 panel: arrived after 39.62 s, failed starts 0, standing on 220.246

$ SCRAPERX_ONLY=matron-exam-climber build/host/scraperx_sim_tests    (5 s wall clock, exit 0)
PASS scraperx_sim matron exam climber: laws held on 200 runs; clean (any) up/down:
  ladder 22(22)/22(22) of 22  pole 23(23)/23(23) of 23  panel 27(27)/27(27) of 27  ledge stack 28(28)/28(28) of 28
```

Beside those, Grok's every-ladder audit on `c5bef54d` (issue #3, 2026-10-26) sent her up and down each
ladder her scan reads: 33 of 33 that got a plan passed with rung drift under 0.001 m, and the
first-ladder top-out fix "holds everywhere. The drift came from the general mantle code, not from one
ladder." The protected test branch `claude/protected-matron-ladders-20261027` measures 35 of 35 with 6
refused for reasons in the world's steel (ladders into solid slabs, one inside a column, one 0.14 m
tight against a rail). Those six are world tweaks for the owner, not code.

So the owner's law holds where it matters: **a ladder is never fixed; the mantle law is fixed, and
every ladder gets it.**

## What is still per-object

| Piece | Where | What pins it | In play? | What replaces it |
|---|---|---|---|---|
| The corner pipe | `matron_pipe.cpp` (517 lines); 23 constants `matron.hpp:613-635` (`kPipeLineX 14.85`, `kPipeDeckZ -132.55`, `kPipeLowestFloor 132` ...) | one riser's world coordinates, its rail height, its flange pitch | yes, first in the chain (`matron.cpp:3311`) | a scanned column climb: the scanner reads the riser (a 0.70 m square column with 1.10 m flanges, `world_solids.inc:430`) as a pole or a column, and the pole move's wrap law decides whether her arms hold it |
| Her walking graph | `levels_`, built in the constructor `matron.cpp:86-135`: nodes, edges, patrols and posts per deck and ring, by coordinate | each deck's band lines, the catwalk, the ladder foot, the cages | yes, every hunt walk | the scan's regions and `AffordanceMap::walk_path` / `grid_walk` (`matron_traversal.hpp:305,376`); the posts stay as named places she favors, which is authoring of her habits, not of her movement |
| The authored route map | `matron_routes.hpp` (7 regions, 7 connectors with coordinates), `matron_routes.cpp`, `follow_route` | `HomeLadder 154 -> 176.25, x 5.0`, `RiserLadder x -6.0`, ... | test-only since `a0d490b` (`matron.cpp:3320-3322`), still compiled, still consulted at `matron.cpp:3155-3157` | delete; the one-way test drives a scanned edge instead |
| The legacy ladder executor | `Matron::climb` `matron.cpp:1367-1508`, `begin_climb` `:1304`, `find_climb_hold` `:1225` | nothing geometric any more (fed by scanned rung edges since `3443d5e`) but it is a second rung climber beside `Climber::step_rung_up` | yes (`matron.cpp:3325`) | `Climber`; keep its force cap and reaction, which match |
| The cables | `follow_line`, `ride_line`, `line_to_take` `matron.cpp:870-1145` | zipline indices and their stands | yes (`matron.cpp:3312,3315,3326`) | a `Line` mover behind the same contract as the rest; the ride itself is a real skill (a cable is a machine), only its slot in the chain goes |
| The second climbing brain | `matron_routecraft.*` (603 lines): its own A* over bar states, its own report; `matron_found_climb.cpp` glues it to the hunt | nothing geometric; it duplicates the planner | yes, via found climbs (`matron.cpp:3319`) and `request_climb_goal` (`:2657,2753`) | one planner (the scan), routecraft kept as the executor of bar panels under the one `Budget`, one force report |

Count of world elevations written into Matron source today: 36 in `matron_routes.cpp`, 19 in
`matron.hpp`, 2 in `matron.cpp`, 1 in `matron_pipe.cpp`; none in the traversal, journey, routecraft,
found-climb, moves or hold files. That number going to zero is the finish line for this document.

## Why fixes keep regressing neighbors (the causal account)

`Matron::pre_step` runs from `matron.cpp:2672` to `:3604`, about 930 lines in one function. When she
has a lead on another floor it tries, in order (`matron.cpp:3308-3326`): the pipe, a line he took, a
line's ramp she is on, a found climb (routecraft's hands), the authored map's way (tests only), a
scanned rung way through the legacy climber, then any line. Journeys over the scan run from a
different entry (`:2707`), routecraft from another (`:2724-2753`). Six movers, three entry points, one
set of shared fields (`climb_`, `on_line_`, `pipe_phase_`, `journey_active_`, `found_.active`,
`routecraft_mine_`, `travel_.active`), and every guard reads several of them (`:2680`, `:2697`,
`:2925`, `:3201`).

That structure is what the last week looked like from the outside:

| Fix | Regressed | Why |
|---|---|---|
| first-ladder top-out on the rung line (`a217a046`) | narrow-shelf mantle (`run 681`, fixed `90eaea5`) | one mantle snap for every lip |
| rung preference in the planner (`a36889bc`) | the knock test's plan went round the deck (`run 693`, fixed `409ecd0b`) | a cost change seen by every route |
| force-law feet (`238ba54`, `e7d5f9f`) | cable let-go at 143 (`matron line`), routecraft's force accounting (`run 690`, fixed `80a37f66`) | two movers pushing in one tick, each reporting alone |
| stronger lunge (`8f5debd`) | the chase check's cap (12/20) | one cap read by two checks |

None of these was a wrong idea. Each was a general fix meeting a neighbor it could not see. That is
integration debt, not an impossible task, and it is paid by removing movers, not by more fixes.

## The retirement plan

Order matters: each step removes coupling the next one would otherwise fight. Each has a finish fact
anyone can check and names the tests that prove it (run only those; CI runs the rest).

| # | Step | Finish fact | Proof |
|---|---|---|---|
| 0 | Land what is green (`409ecd0b` proved on run 694 steps 1-18; the ladder-audit integration). The owner merges the two protected pull requests (law pins, `matron-ladders` in CI). | `ScraperX-Claude` carries the ladder audit and `matron-ladders` runs in CI | the proof run |
| 1 | ✅ landed `2dc872c6`. **One mover contract, one active mover.** A `Mover` with `begin(goal)`, `step(dt, budget)`, `cancel()`, `status()`, `view()`. Wrap the existing executors (Climber journey, routecraft, line, pipe) behind it without changing what they do. `pre_step` holds exactly one active mover and one planner call chooses it; the six-way chain at `:3308-3326` becomes that call. | `pre_step`'s "elsewhere" branch makes one call; no mover reads another's private fields; `grep -c 'pipe_phase_\|on_line_\|climb_ >= 0' matron.cpp` outside the movers is 0 | `matron`, `matron-remaining`, `matron-chase`, `matron-pursuit`, `matron-climb-scan`, `touch_matron_line`, `touch_matron_act` |
| 2 | ✅ landed `eb617c3d` (PR #12). **Delete the authored map from runtime.** `matron_routes.*` out of CMake; `follow_route`, `found_climbs_off_`, `route_on_edge_` gone; the one-way test (`matron-ways`) drives a scanned edge. | no `routes::Region` in `src/sim`; `matron_routes.cpp` absent | `matron-ways`, `matron-routes` retired or rewritten on the scan, `matron-pursuit` |
| 3 | **The corner pipe becomes a scanned climb.** The scanner reads columns with flanges; the pole move's wrap law (squeeze times friction against her weight, her reach round the section) says whether she holds it. `matron_pipe.cpp` and the `kPipe*` constants go. Her ride down it stays as fast and as loud as the owner likes it, from the law, not a constant per riser. | no `kPipe` in `matron.hpp`; the pipe tests pass through the general move | `matron-pipe`, `matron`, `touch_matron_line`, `matron-climb-scan` |
| 4 | ✅ landed `384f1d0c` (PR #16). **The legacy ladder executor goes.** `Matron::climb`, `begin_climb`, `find_climb_hold` replaced by `Climber` rung moves, with the same reaction. The cap is the Climber's one budget (8,000 N / 22 kW), not the old climber's 5,500 N grip rating. | one rung climber in `src/sim` | `matron-ladder`, `matron-ladders`, `matron-climb-support`, `matron-chain` |
| 5 | **Walking from the scan.** `levels_` nodes and edges replaced by scan regions and `walk_path`; patrols become loops the scan offers on her deck; posts stay as named places. | no node coordinates in the constructor | `matron`, `matron-remaining`, `matron-hearing`, `matron-moves`, `touch_matron`, `touch_matron_heave`, `touch_bay` |
| 6 | **One brain.** The scan is the only planner; routecraft keeps its bar execution as a move kind under the one `Budget` and one report; `matron_found_climb.cpp` folds into the planner's edge building. | one planner, one `Budget`, one force report; `routecraft: reported force matches actual body accumulator` passes with no special case | `matron-routecraft`, `matron-exam-routecraft`, `matron-found-climb`, `matron-exam-climber`, `matron-towers` |

Steps 1 and 2 are a few days of one session. Steps 3 to 6 are each their own job and each leaves the
game playable and the suite green; they can stop after any step without loss.

## Rules that do not change

- The laws on issue #3 and in `CLAUDE.md`: gravity on; her one budget of 8,000.5 N with the Turn's
  lunge its own move at 9,800 N / 65 kW; 220 kg; a push-off is a force over time; at most 0.20 m a
  tick; real falls; no per-route, per-deck, per-ladder code, tables, waypoints or tuned constants for
  one place; held-out targets are exams; the inventory is an answer key.
- **Exams are per object. Code never is.** A failing ladder, crate, cable or panel is a fact about
  a law (reach, mantle, wrap, cost, clearance) or about the world's steel. A fix names which. A fix
  that names a place is refused in review.
- Tests never weaken; a check changes only by the owner's decision, by pull request (issue #3,
  comment 6442285390). World tweaks within about 0.2 m are proposed with numbers; the owner approves.
- Test only what a change reaches (`CLAUDE.md`); the proof is CI on an `<agent>/integrate-*` branch.

## Open for the owner

1. The six ladders her scan refuses: world tweaks (a hatch, a ladder moved off a column, a rail 0.14
   m further) or left as they are. Numbers are in the `matron-ladders` output on
   `claude/protected-matron-ladders-20261027`.
2. The corner pipe's wrap: whether a 0.70 m square column is something her arms close round (step 3
   above). If not, the general move is "shimmy a flanged column", still a law, never a coordinate.
3. `grok/standpipe-board-landing` (`a5eea2b`): her lethal fall moved from a 12 m to a 13 m drop and
   the knockdown line from 7.60 to 8.20 m/s, with a world edit. A protected change: yours to merge or
   refuse.
