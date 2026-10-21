# Multi-deck Matron: an outside proposal, kept as input (2026-10-21)

The owner passed on a proposal by another agent (Gemini) for the Matron climbing
and travelling through many more decks, up and down (the owner's standing
directive in `00_START_HERE.md`; next after the fatality).

**Checked against this repo before keeping it: its code references are not
real.** None of the files it cites exist (`src/sim/matron.h`,
`src/sim/matron_moves.h`, `src/sim/world_solids.h/.cpp`, `src/sim/climb.*`,
`src/sim/tower_gen.*`, `src/sim/sound.*`), nor do the functions it names
(`NearestLevelIndex`, `PropagateSoundOnGraph`, `ExecuteLadderClimb`,
`BuildMatronNavGraph`). Her levels are two, not three: `levels_ = {deck, ring}`
(`src/sim/matron.cpp`), the 154 deck and the 176 ring; 143 is reached by her
corner pipe. The world's solids are a generated table (`world_solids.inc`), not
a class. Its "current behavior" sections, file touchpoints and line references
therefore describe code that is not here, and it cannot be followed as a plan.
Its branch `grok/matron-multi-deck` was not pushed.

**What is kept, as ideas to weigh when the multi-deck work starts:**

1. A route graph across decks: walkable decks, rings and catwalks as nodes;
   ladders (both ways, getting on and off at any deck they pass), pipes (down,
   and up only the slow way), short mantles and safe drops as edges; routes of
   several hops planned only when her target's deck changes, in fixed storage.
2. Physical climbing on the tower's real holds (rungs, flanges, braces,
   ledges) for short steps and mantles, as bounded forces on her one body.
3. The tower's machines ridden only when already moving or when the player is
   on them; she never sets off a one-shot machine the player's route needs.
4. Hearing that carries through open shafts and is muffled through solid
   decks, with the deck she hunts held a moment before it changes.
5. Its risks worth testing: snagging on a deck's lip getting off a ladder,
   overshooting a stop sliding down a pipe, chasing back and forth at a
   ladder head, and pathfinding cost per tick.

The owner's rules any design must keep: no teleporting, no infinite force, she
never degrades the player's climbing by proximity (only her actual grab tears
a grip), and nothing hard-wired to two levels.

The proposal as given follows.

---

```
MATRON MULTI-DECK PHYSICAL TRAVERSAL PROPOSAL (Gemini, as given)

1. STAGE 1: MAP
1.1 Track A (her vertical envelope): claims levels_ holds 143, 154 and
176 m; ascent by the ladder 143 -> 154 -> 176 with bounded upward impulses
on rung contact; descent by the corner pipe from 176 or 154 to 143 with
velocity-proportional friction braking. Bottlenecks claimed: one canonical
ladder and pipe per level pair; no rung-by-rung descent; no mid-ladder
dismount; steering assumes flat topology and no multi-hop portal queue.
1.2 Track B (tower substrate): claims decks every 11-22 m from 0 m to
300 m+, ladders registered as climbable volumes or rung arrays (spacing
0.30-0.35 m, width 0.50-0.60 m) across most transitions, corner and core
pipes (r 0.08-0.25 m) across several decks, cables and guy wires, and
machines (counterweights, sabots, seesaws, gear platforms, crane cradles,
screw lifts) as dynamic or kinematic Jolt bodies.
1.3 Track C (hearing): claims sounds snap to the nearest of levels_ (so a
player decks away is heard on 143 or 176); straight-line attenuation with a
same/different level penalty (false hearing through slabs, missed open
shafts); a single-edge vertical dispatch that cannot chain several decks.

2. STAGE 2: THREE DESIGNS (invariants: no teleport; bounded force, climb
thrust <= 2.2 m g; zero degradation of the player's climbing in 0-5 m)
A. DeckNavGraph: extract a directed multigraph from the world at start:
walkable patches as nodes; ladders as two-way edges with mid-shaft
boarding within 0.85 m; pipes as down slides (slow up friction climb only
without a ladder); safe drops <= 6.0 m. Ladder and pipe controllers on her
one capsule (gait phased to rung height; pipe radial spring-damper, descent
held to 2.5-6.5 m/s). Player isolation: no constraints to rungs; soft
separation on a shared ladder.
B. FreeformHoldClimber: query holds within 1.35 m of her chest (rungs,
flanges, braces, pipes, cables), score by direction, grip and footing;
four virtual limb anchors without Jolt constraints, at least two anchored
to push, total force <= 2.2 m g; mantles of 1.5-2.5 m, columns, trusses.
C. MachineRider: machine descriptors (body, travel, velocity, capacity,
trigger); wait for a carrier arriving within 8 s; board within 0.25 m and
under 1.2 m/s; ride with station-keeping on the support's point velocity,
her weight and reaction on the carrier; step off at the target deck.

3. STAGE 3: JUDGES AND SYNTHESIS
A alone: disconnected deck islands where decks join by ledges or trusses;
ladder kiting if she can only leave at the ends. B alone: stalls under deck
soffits (no global knowledge); contact with a climbing player merges solver
islands. C alone: she could dump a counterweight's energy the player's route
needs; machines cover few spans.
Synthesis: macro route graph over all decks (Ladder both ways with mid-shaft
exits; Pipe down plus a slow climb; LedgeMantle <= 2.4 m; ControlledDrop
<= 5.5 m; PassivePlatform only when already moving or ridden by the player,
never triggering one-shot machines); meso hearing propagated on the graph
(-3.5 dB per open shaft, -12 dB through a slab), the route of up to 16 hops
stored with her target; micro bounded locomotion: ladder up and down with
exits at any deck (F <= 2.0 m g), pipe entry and bail-out at any deck it
passes with a pre-brake 2.5 m above, mantles by a single-body wrench; while
the player has hand or foot holds, Matron-player solver contacts masked and
replaced by her soft avoidance within 1.1 m.

4. STEPS, FILES, TESTS, RISKS
Steps: extract decks and connectors; multi-hop planner with fixed storage
(A* only when the target deck changes or a connector completes, <= 64
nodes); bidirectional climb, pipe bail-out and mantle; proximity isolation.
Tests proposed: graph extraction finds the decks and edges; a multi-hop
ascent two or three decks with no step over 12 m/s and no force over
2.2 m g; descent by pipe and ladder with a bail-out at 154; a player hanging
on the 154-176 ladder with her climbing within 0.5 m, the player's hold
error and swing unchanged to 1e-5.
Risks: solver island merge near a climbing player (mask contacts, steer
away); snagging on a deck lip leaving a ladder (lift 0.18 m first); pipe
bail-out overshoot (pre-brake to <= 1.2 m/s); chasing back and forth at a
ladder head (1.5 s hysteresis unless 3 m into the new deck); A* cost per
tick (only on change).

(Its file touchpoints, symbol names and line references are omitted here:
they name files and functions this repo does not have; see the note above.)
```
