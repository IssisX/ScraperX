# Multi-deck Matron: the proposal, checked against the code (2026-10-21)

The owner wants the Matron to climb and travel through many more decks of the
tower, up and down (standing directive in `00_START_HERE.md`; next after the
fatality). Two outside agents were asked for a proposal. The first
(`MULTI_DECK_INPUT_GEMINI.md`) named files and functions this repo does not
have. This one, audited at `27d86a9` on `claude/matron-physics`, was checked
claim by claim by a verification workflow (four read-only checkers, one
synthesis, each claim re-read at file:line): **19 claims, 15 confirmed, 4
partly right, 0 wrong.** The Matron sources are unchanged since `27d86a9`, so
its line numbers still hold.

## What the check found that matters before any multi-deck work

1. **The corner pipe is not physical** (breaks the owner's all-physical rule).
   Getting on, she is moved to the steel layer, her gravity switched off and
   her velocity zeroed (`matron.cpp` 953-955); each tick her velocity is set
   to the distance to a fixed waypoint over the step (1081-1086,
   `SetLinearVelocity(body_, move / dt)`), and the stun, tear-off and
   dismount write velocity too (1009, 1031, 1116). Nothing holds her weight.
   It is also allowed only on the 132, 143 and 154 m decks
   (`kPipeLowestFloor`, `pipe_reaches`), though the riser and its 13 flanges
   run 0 to 154 m (`world_solids.inc` 430-444).
2. **Her ladder climb on this branch is the same in a milder form**: `climb()`
   (856-899) sets her velocity toward a target on the ladder's line with
   gravity off until the mantle (881, 895); her hands never take a rung. The
   dot hearing-and-climbing work replaces this with real contacts and capped
   forces (its port is on the to-do).
3. **She can follow a trail she never saw.** Crumbs are dropped whenever the
   player is inside a fixed box round her deck (|x| < 27, z -177 to -123;
   1734-1735, 1756-1758) with no sight check, and Searching follows them
   (2143-2151), including after a knockdown (3089).
4. **The cable is mostly physical** (a real slider and brake carry her), but
   the jump on (759-761) and the swing off (800-803) set velocity directly.
5. Checks that misread stacked decks even today: `settle_level` changes her
   level by height alone within 0.6 m (675-688); blocked nodes and the stall
   watchdog key on x,z only (`matron_moves.cpp` 153-160, 189-193);
   `feel_floor` reads only the floor's vertical velocity (657); the deck
   footprint and `carry_m`'s shaft are fixed boxes (308-311).

## Corrections to the proposal

- The 154 to 176.25 ladder is not "subject to stage": she takes hold of it
  only in the Turn (846); stage only sets how often the Turn comes (Pest about
  45 s, Dangerous 18 s, Changed 4 s; 1439-1448). The bay rungs bypass the
  Turn gate. The 143 to 154 riser is marked not hers (130).
- `follow_climb` (807-829) also needs the climb to be hers and its head not
  above her lead's floor; a climb stopping partway is allowed, so climbs
  chain over later ticks (today: the bay rungs, then the ladder). She never
  climbs down.
- There is no `Zipline::build`: the lines are placed in `physics_world.cpp`
  (one yard line at 314, then up to 13 tries across the Stack's well and 10 in
  the upper rings, 339-348); `add_line` stops at 24; the total of 22 is held
  by `tests/sim_tests_wet.cpp` 272.
- Stack deck heights come from `physics_world_tuning.hpp` (11 m pitch, 14
  levels); the rings 176 to 396 are `UpperDeck` solids in `world_solids.inc`;
  the band builders make 418 to 616, TP-340, TP-640 and the Midstack. Its
  machine update is `Route::pre_step` (`vertical_route.cpp` 341).

## The proposal's map (confirmed)

- **Her levels** (`matron.cpp` 75-140): the 154 home deck with its full
  graph; the 176.25 ring; ten plain upper rings 198.25 to 396.25 "with no way
  up of hers known"; thirteen Stack levels 11 to 143 copying only the deck's
  8-node band loop (no catwalk, ladder, posts or way); the bay bracket at
  152.22 with its escape rungs. Knowing a floor is not having a way to it.
- **The tower's decks**: the Stack 11 to 154 every 11 m; the Counterweight
  Well rings 176.25, 198.25, 220.25; Wet Isolation 242.25 to 330.25 and TP-340
  (340.25); the Plate Shop 352.25 to 484.25; the Facade Crane Stack 506.25 to
  616.25 and TP-640 (640.25); the Midstack 662.1 to 750.1; the external
  receivers and the summit.
- **The player's climbing routes**, no lift, run unbroken from 154 to 750 m
  (`band_counterweight_well.cpp` 978-1054, `band_wet_isolation.cpp` 692-773,
  `band_plate_shop.cpp` 458-481, `band_facade_crane.cpp` 517-535,
  `band_midstack_service.cpp` 1193-1206, `climb_c6.cpp`, `climb_c7.cpp`); in
  the lower Stack the climbs C1, C2, C3 and the machines S1 to S3 take turns.
  Per section: grade to 22 (yard, tray gantry, service ladders); 22 to 44 C1
  (cabinet, duct, vent stack, monorail, davit, hanging ladder); 66 to 88 C2;
  132 to 154 C3 (stepped girder, duct mantle, wall rungs to 143, the riser
  ladder to 154); 154 to 176 rung ladder; 176 to 198 L-boards and a smooth
  standpipe; 198 to 220 catwalk and a scaffold panel caught with a jump; 220
  to 242 boards and ladder; 242 to 264 beam and scaffold panel; 264 to 286
  bulkhead lip, boards and standpipe; 286 to 308 boards and ladder; 308 to 330
  catwalk and panel; 330 to 340 ladder to the transfer plate; the header
  tank's ladder and rim plank (not a way to the tank floor); 340 to 640
  board-and-ladder connections up the east well band and the hatch ladders;
  640 to 750 five backup ladders through deck hatches.
- **Machines** (band builders and `vertical_route.cpp`): S1 grade to 22, S2 44
  to 66, S3 88 to 132; Well A 154.25 to 176.25, Well B 176.25 to 198.25 (one
  shot), Well C 198.25 to 220.25; Wet D, E, F; Plate G, H, I; Facade J, K, L;
  Midstack M, N, O; the placed vertical machines (cascade mast, pitman lifts,
  barrel helix, crown gondola, luffing derrick, gravity balance, traction
  tram, slab incline, stone wheel); yard equipment; the bay leaf. A machine's
  travel may skip the decks it passes.
- **Her perception** (all confirmed): `floor_of` falls back to her level;
  the fixed footprint; `carry_m`'s fixed shaft; the vertical-only floor
  velocity; height-only `settle_level`; crumbs keyed by x,z with a floor
  height; blocked memory and watchdog by x,z; crumbs dropped without sight.

## The proposal's plan (kept as the foundation)

**Architecture**: a directed, capability-aware route graph whose edges are
carried out through real contacts and finite forces, with machine rides as
optional, state-dependent edges. The graph decides where she can try to go;
a physical executor decides whether she gets there.

| Part | Does | Must not |
|---|---|---|
| Support regions | Real walkable patches, landings, hatch approaches, machine receivers | Take a floor's identity from its height alone |
| Route graph | Choose a sequence of traversal attempts | Move bodies |
| Traversal executor | Take real holds, apply capped forces | Read the hidden player's position |
| Perception memory | Bounded, aging evidence with its source | Confirm a guessed destination |
| Machine adapter | Expose state; arbitrate requests; one update per tick | Run a machine twice or bypass its mechanism |
| Presentation | Draw her pose, holds, attention | Support or move her |

**Edges**: walk or balance; step or mantle; ladder up; ladder down; pipe up;
pipe down; jump to a hold; shimmy; cable ride; machine board, ride and get
off. Up and down are separate edges, not one reversible one. Each edge is a
request: before she commits, the executor checks support and clearance;
during it, support every tick; she changes deck only on physical arrival;
after a failure she lets go, falls, lands, waits or replans, never relocated.

**Invariants**: no snapping or teleporting; gravity always on while climbing,
on a pipe, hanging or riding; every climbing force capped, and controllers
together within one cap; dynamic supports take the reaction at the contact;
missing, broken, blocked or overloaded holds can defeat a route; height alone
is not arrival; the player's climbing (strength, grip, reach, response) never
changes with her nearness, and no invisible hold reservations; perception is
evidence, never the player's live coordinates; capture and fatality never
weakened to make movement tests pass.

**Physical holds**: hold identity includes body, generation and the contact's
sub-shape; reach and line of reach before taking hold; separate rules for
rungs, ledges, pipes (a bounded grip and friction model: a smooth pipe is not
a rung) and jump catches; descent as controlled lowering; mantles need a clear
swept path and real landing; bounded probes and search; route hints narrow
the local search; she never pushes harder to force an unsupported move.

**Machines**: separate read-only state from actor requests; arbitrate the
player's and her requests before one update per tick; she must reach the
control; her mass counts in the load; spent, broken, occupied or unrigged
machines are unavailable edges; point velocity (with rotation) while boarding
and riding; a different way back where a machine cannot reverse; she never
sets off a one-shot machine the player's route needs.

**Player independence**: she waits or picks another approach when her body
would block the player's clearance; route reservations guide only her; she
never uses the player as support; real collision and shared loads stay
physical; the player's traversal keeps its own query budget.

**Capture and fatality**: rim candidates and clearance per region, not a
fixed shaft; never route a captured player to another deck's facilities,
invent hooks, or shorten the sequence.

## Steps (proposed)

| Step | Work | Gate |
|---|---|---|
| Baseline | Reproduce today's failures; deterministic fixtures | Known failures named, no assertion relaxed |
| Dot work | Port the hearing and physical ladder climb onto this branch without losing the newer capture and fatality | Encounter kept |
| Regions | Register regions and route endpoints beside the geometry that owns them (new `matron_routes.*`, the band builders, `vertical_route.*`) | Candidate inventory, unresolved edges off |
| Plan and execute apart | Directed edges, bounded search, region-scoped blocked memory, progress along the edge | No body moved by graph code |
| Ladders | Rung acquisition up and down, top entry, mantle (new `matron_traversal.*`) | 143, 154, 176.25 both ways |
| Pipe | Replace the gravity-off waypoint pipe with a real grip, slip, flange strike and recovery | Today's 132 to 154 pipe works physically before it reaches farther |
| Near routes | 176 to 198 to 220 and the board-and-ladder connections above | Real holds, blocked-route failure, ways back |
| Machines | Actor-neutral requests, boarding, load, ride, getting off | One machine update per tick, nothing remote |
| Tower | Lower Stack, transfer plates, upper rings, hatches | A reachability table: up, down, conditional, unavailable, each with its reason |
| Perception | Per-region footprint and heights; no crumbs without sight | Decoys, occlusion, expiry and hidden movement hold across decks |
| Encounter, save, view | Region-aware capture, restore mid-traversal, native telemetry | No stale anchors, no phantom arrival |

**First playable milestone**: physical travel among 132, 143, 154 and 176.25,
ladders both ways, the converted pipe both ways, the bay escape kept, a route
failure that truly lets go and recovers, pursuit up and down on evidence, an
independent patrol circuit, the player's climbing unchanged near her. Second:
176 to 198 to 220 (the smooth standpipe and the jump to the panel).
"Many more decks" means a connected, verified network, not a longer floor list.

## Tests (proposed)

Traversal: the full 143-154-176 circuit; ladder descent with a real landing;
a missing rung (no invented hold); a held member removed (she lets go before
the next force); the wrong member of the same compound body; a sensor-only
ladder (never support); a blocked mantle corridor; a smooth standpipe
(bounded grip or an explicit no); a dynamic support (equal reaction); overload
(the cap holds, the hold can fail); a destination change mid-climb (no letting
go in the air); a fall past decks (no false arrival); the same x,z blocked on
one deck and open on another.
Perception: a hidden player changing decks does not move her lead; a louder
crash stays the lead; heard ladder movement suggests an exit, not arrival;
evidence expires during a long climb; quiet loses pursuit but not patrol
knowledge; unknown landings not put on her deck; receivers outside the old box
heard; bounded queries under load; seeing the player replaces guesses.
Machines: no unreachable control; one update per tick after arbitration;
one-shot stays spent; boarding fails safely; her mass counts; point velocity
on moving platforms; a stalled machine gives no teleport or extra force; no
assumed return trip.
Player: identical inputs with her absent, far, near without contact, on the
same structure, on a separate moving support, on a shared loaded structure,
and waiting at the same ladder: unchanged where there is no contact, and only
measured physical loading where there is.
Long routes: patrol from below her deck to above the first ring and back; a
chase up that loses the player; a decoy below her, searched and abandoned; a
spent machine routed round; the 340 and 640 transfer plates; the hatch-ladder
chain both ways; restore during approach, grip, ride, fall and landing;
identical runs, identical routes.

## Its baseline (matches ours)

`matron-moves` passes; `matron-grab` fails after seven passes at "matron
fatality: every beat, in order" (being reworked now); `bay`, `climb`,
`zipline`, `twin-geometry`, `shared_survival` and `parkour_flow` pass.

## Its risks

Integrating the dot branch over newer encounter work (port selectively);
legacy non-physical movement hidden in a larger graph (audit pipe, cable,
vault and mantle); smooth pipes as invisible elevators; descent assumed from
ascent; phantom arrival from height-only levels; machines updated twice or
operated remotely; the player's climbing degraded near her; hatches too
narrow for her capsule; omniscient pursuit; query cost on the phone; save
and restore keeping invalid anchors; collision drifting from the presentation.
