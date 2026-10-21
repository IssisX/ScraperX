# Multi-Deck Matron Proposal

## Scope and provenance

- **Repository:** `IssisX/ScraperX`
- **Source branch:** `claude/matron-physics`
- **Audited source commit:** `27d86a96d89e129a20c10c9298361d6e11581076` (the branch head has since moved to `baebeab`; this proposal is pinned to the audited commit)
- **Delivery branch:** `grok/matron-multi-deck`, based on the audited commit
- **Change authorization:** Proposal only; no implementation, source edits, merges, or changes to `main`
- **Related work inspected separately:** `dot/matron-hears-and-climbs-20261021` at `c0018eb1756e1e22c9080405bedce6a545dcea7c`
- **Workflow:** Three parallel read-only mapping passes, three independently framed designs, adversarial reviews, and a synthesized implementation plan
- **Verification boundary:** Source inspection and focused native baseline tests; no new multi-deck behavior implemented or tested, no Android validation, and no fresh reproduction of the dot branch's reported results

The dot branch is **not** the audited source branch. Its bounded-force ladder implementation and hearing fixes are useful candidate dependencies, not features that can be assumed present in `claude/matron-physics`. Its handoff explicitly excludes conversion of the older pipe, cable, and vault behavior. [Dot handoff](https://github.com/IssisX/ScraperX/blob/c0018eb1756e1e22c9080405bedce6a545dcea7c/03_EXECUTION/MATRON_REDESIGN/HEARING_CLIMB_HANDOFF.md)

## Recommended architecture

Use a **directed, capability-aware route graph whose edges are executed through real contacts and finite forces**, with machine rides as optional, state-dependent edges.

The graph decides **where she can try to go**. A physical executor determines **whether she actually gets there**. The tower's machines offer shortcuts and alternate connections, but neither their availability nor their reversibility is guaranteed.

Do not solve this by merely adding more entries to `levels_`. The current implementation already knows about numerous floors without having a physical route to them. [Current level initialization](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/matron.cpp#L76-L157)

### Nonnegotiable invariants

- No traversal position snapping, teleporting, or remote relocation after failure.
- Gravity remains active during ordinary climbing, pipe travel, hanging, and riding.
- Every climb-support force has a finite cap; overlapping controllers cannot each spend the full cap.
- Dynamic supports receive the corresponding reaction at the actual contact point.
- Missing, blocked, broken, overloaded, or unreachable holds can defeat a route.
- Reaching a height does not establish arrival; real supporting geometry and clearance must establish it.
- Upward and downward traversal are separate capabilities, not automatically reversible edges.
- Matron proximity never changes the player's climbing strength, grip limits, reach, stamina, input response, or acquisition rules.
- Shared geometry may transmit ordinary physical loads, but no hidden player debuff or invisible hold reservation is allowed.
- Perception supplies evidence, not privileged access to the player's current coordinates.
- Existing capture and fatality behavior must not be silently weakened to make mobility tests pass.

## Map: Current Matron

### Levels and connectivity

| Area | Current representation | Consequence |
|---|---|---|
| Home deck | Explicit 154 m graph, including bands, catwalk, ladder approach, bay crossing, and guard positions | Most authored behavior is concentrated here |
| First upper ring | Explicit 176.25 m graph, including cage approaches and a gap link | More specialized than the generated upper rings |
| Higher rings | Ten additional ring levels, from 198.25 through 396.25 m | Knowing these floors does not provide a way to climb between them |
| Lower Stack | Thirteen levels at 11 m intervals, from 11 through 143 m | Copied band-loop knowledge, not a complete lower-tower traversal plan |
| Bay bracket | Separate small level below 154 m, with its own escape climb | Must remain a special physical escape, not become a generic nearest-height floor |
| Outside this list | Grade, transfer plates, higher decks, external machine receivers, and summit structures are not comprehensively represented | Extending a height list alone cannot describe the actual tower |

These distinctions are explicit in the constructor: the higher rings are described as having no known way up, while the lower levels reuse the home deck's band-loop layout without its special connections. [Level construction](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/matron.cpp#L76-L157)

### Authored climbs

| Connection | Current status | Redesign implication |
|---|---|---|
| 154 → 176.25 | Known usable ladder, subject to Matron stage restrictions | First regression fixture and first physical bidirectional conversion |
| 143 → 154 | Known riser, explicitly marked unavailable for her to take hold of | Real geometry exists; enabling it requires a physical controller and validated approach/exit |
| Bay bracket → 154 | Usable escape regardless of ordinary stage restrictions | Preserve escape priority and interruption behavior |

`follow_climb` currently selects an upward climb whose foot matches her level and whose head is above it. That selection model cannot express descending a ladder or composing a route through several decks. [Climb definitions and selection](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/matron.cpp)

### Corner pipe

The pipe controller is restricted to the 132–154 m portion of the Stack, despite the exported riser and flange geometry extending farther down. It therefore represents a small authored traversal mode, not a general tower pipe network. [Pipe constants](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/matron.hpp#L550-L583) [Exported riser geometry](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/world_solids.inc#L430-L444)

The existing pipe controller changes her collision layer, turns gravity off, and writes velocity while moving between authored waypoints. Its slide-and-flange sequence is also controller-driven. This is not an acceptable unchanged foundation for the requested all-physical expansion. [Pipe implementation](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/matron.cpp#L940-L1088)

**Required treatment:** Convert the current pipe before extending its range. Preserve its recognizable mount, climb, descent, slip, flange impact, stun, and dismount behavior, but make support and failure physical.

### Local navigation

The current routing operates inside the selected level's graph. Its blocked-node memory uses horizontal coordinates; breadcrumbs store a floor height; and the stall watchdog measures horizontal progress. These assumptions become problematic when many routes repeat the same horizontal coordinates on different decks or when progress is primarily vertical. [Local routing](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/matron.cpp) [Movement memory and watchdog](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/matron_moves.cpp)

**Required treatment:**

- Key blocked locations by support region and edge identity, not just `x,z`.
- Key breadcrumbs by spatial region and evidence provenance, not just a float floor.
- Measure progress in the active edge's coordinate system.
- Retain local avoidance, but prevent its recovery logic from fighting an active climb or machine ride.

## Map: Tower Opportunities

### Geometry authority

The inventory must include both exported world collision and native bodies built by the simulation. `world_solids.inc` is not the entire traversal world: ladders, scaffold panels, machine frames, decks, and moving mechanisms are also created in native builders. [World construction](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/physics_world.cpp) [Native band builders](https://github.com/IssisX/ScraperX/tree/27d86a96d89e129a20c10c9298361d6e11581076/src/sim)

The generated collision file identifies its export pipeline. Future geometry changes must go through the authoritative scene/export process rather than hand-editing the generated file. Visual pipework or bracing must never qualify as a hold merely because it is rendered. [Collision export](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/godot/presentation/solid_export.gd) [Generated collision](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/world_solids.inc)

The following is a source inventory of candidate route families. **It is not a claim that every candidate already fits Matron's capsule or can support bidirectional travel.**

### Deck inventory

| Tower section | Decks and landings to represent | Source |
|---|---|---|
| Lower Stack | Grade; 11, 22, 33, 44, 55, 66, 77, 88, 99, 110, 121, 132, 143, and 154 m decks; intermediate traversal platforms | [Stack builder](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/band_stack.cpp) |
| Counterweight Well | 176.25, 198.25, and 220.25 m ring surfaces, cage stops, and scaffold approaches | [Well builder](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/band_counterweight_well.cpp) |
| Wet Isolation | Rings at 242.25, 264.25, 286.25, 308.25, and 330.25 m; TP-340 at 340.25 m; machinery-specific landings and header-tank rim | [Wet builder](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/band_wet_isolation.cpp) |
| Plate Shop | 352.25, 374.25, 396.25, 418.25, 440.25, 462.25, and 484.25 m rings, plus machinery landings | [Plate Shop builder](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/band_plate_shop.cpp) |
| Facade Crane Stack | 506.25, 528.25, 550.25, 572.25, 594.25, and 616.25 m rings; TP-640 at 640.25 m | [Facade builder](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/band_facade_crane.cpp) |
| Midstack Service | 662.1, 684.1, 706.1, 728.1, and 750.1 m decks, with their hatch approaches | [Midstack builder](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/band_midstack_service.cpp) |
| External and summit routes | Machine receivers, connecting walkways, summit deck, and runway arm | [Vertical route](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/vertical/vertical_route.cpp) [Summit](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/summit.cpp) |

Use actual support surfaces rather than nominal deck labels. A cage at "154" can have a different floor top from the adjacent deck, and the upper tower includes landings that do not belong to the regular ring sequence. [Well floor constants](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/band_counterweight_well.cpp) [Vertical receiver placement](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/vertical/vertical_route.cpp)

### Climbing inventory

| Section | Actual climbing opportunities | Principal difficulty |
|---|---|---|
| Grade → 22 | Yard equipment, tray gantry, maintenance platforms, and service ladders through intermediate heights | Reaching the first ladder and negotiating narrow platforms |
| 22 → 44, C1 | Cabinet, duct, vent stack, monorail, davit, and hanging ladder | Mixed mantles, narrow travel, and discontinuous holds |
| 66 → 88, C2 | Machinery hall, pipe rack, traversal additions, and davit ladder | Transitions between unlike support types |
| 132 → 154, C3 | Stepped girder, duct mantle, wall rungs to 143, and high riser ladder to 154 | Long compound route and separate descent behavior |
| 154 → 176 | Rung ladder | Validating both top-out and top-entry descent |
| 176 → 198 | Narrow L-shaped boards and a smooth standpipe | A rung-only hold finder cannot climb the pipe |
| 198 → 220 | Catwalk and scaffold panel reached with a jump | Bounded launch, catch, and missed-catch recovery |
| 220 → 242 | Boards and ladder | Narrow approach and mantle clearance |
| 242 → 264 | Beam and scaffold panel | Jump-to-hold transition |
| 264 → 286 | Bulkhead/lip route, boards, and standpipe | Shimmy, edge transition, and pipe grip |
| 286 → 308 | Boards and ladder | Approach and top-out |
| 308 → 330 | Catwalk and scaffold panel | Jump-to-hold transition |
| 330 → 340 | Ladder to transfer plate | Different landing geometry |
| Header tank | Internal escape ladder and rim plank | Must not become a false connection to the tank floor |
| 340 → 484 | Initial ladder and repeated board/ladder connections along the east well band | Tapered rings and repeated coordinates |
| 484 → 640 | Repeated board/ladder connections and final hatch ladder | Hatch clearance and top-entry descent |
| 640 → 750 | Five backup ladder connections through deck hatches | Sequential narrow transitions and occupancy |

These routes are authored in the native band builders; the tower is not missing vertical geometry merely because Matron does not currently use it. [Stack climbing](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/band_stack.cpp) [Well climbing route](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/band_counterweight_well.cpp#L978-L1086) [Wet climbing route](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/band_wet_isolation.cpp#L692-L773) [Plate Shop climbing](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/band_plate_shop.cpp#L458-L481) [Facade climbing](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/band_facade_crane.cpp#L517-L535) [Midstack climbing](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/band_midstack_service.cpp#L1193-L1206)

C6 and C7 provide additional authored physical routes around the 242–264 and 308–330 intervals. They are alternate candidates, not justification to assume that every player maneuver is immediately available to Matron. [C6](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/climb_c6.cpp) [C7](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/climb_c7.cpp)

### Pipes and structural holds

- The corner riser and its flanges are candidates for extending lower-Stack travel after physical conversion.
- The Well's 176–198 standpipe and Wet Isolation's 264–286 standpipe require a pipe-grip primitive, not the existing upward-facing rung search.
- Machinery pipe racks, vent stacks, columns, rails, beams, scaffold members, and panel crossbars may provide local holds or intermediate supports.
- Rendered pipework, inaccessible members, sensors, and machinery interiors must remain excluded unless actual collision and reach tests establish usability.

The relevant geometry is spread across exported solids, band builders, and specialized climb builders. [Exported solids](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/world_solids.inc) [Stack](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/band_stack.cpp) [Well](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/band_counterweight_well.cpp) [Wet Isolation](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/band_wet_isolation.cpp)

### Cable inventory

`Zipline::build` creates 22 lines: three initial lines and a generated descending network. Their endpoints, arrival floors, trolley state, availability, and landing geometry should become directed transport-edge data. Do not infer their connections from deck numbering. [Cable construction](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/zipline.cpp#L76-L173)

Matron already has a separate cable-following path, but its entry and release behavior includes velocity writes. Cable support, rider sharing, trolley availability, finite attachment load, and safe release need their own audit before the mode qualifies for the new physical contract. [Matron cable travel](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/matron.cpp#L692-L817) [Cable mechanics](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/zipline.cpp)

### Machine inventory

| Family | Candidate connections or use | Restrictions |
|---|---|---|
| S1 water-balance hoist | Grade → deck 22 | Water, load, catch, and reset state |
| S2 walking-beam hoist | 44 → 66 | Ballast, beam, latch, and load state |
| S3 brake-override counterweight hoist | 88 → 132 | Freight-car/counterweight arrangement and brake trip |
| Well A | 154.25 → 176.25 | Rigging, skip, catch, and cage availability |
| Well B | 176.25 → 198.25 | Boom-driven and explicitly one-shot |
| Well C | 198.25 → 220.25 | Keeper, debris flow, weight balance, and cascade state |
| Wet D | 220.25 → 256.25 | Hydraulic state and physical pipe connection |
| Wet E | Cab begins around 253 m and settles near the next machinery landing | Drop trigger, air system, chiller, and actual settled height |
| Wet F | 298.25 → 340.25 | Accumulator load and hydraulic state |
| Plate G/H/I | Successive machinery routes through the 340–484 section | Actual machine endpoints do not replace all intermediate ring connections |
| Facade J/K/L | Machinery routes through 484–640 | Rail joint, counterweights, rigging, catches, and available travel |
| Midstack M/N/O | 640→662, 684→706, and 728→750 intervals | Reel, chain/granular state, safety dogs, and payload |
| Placed vertical machines | Cascade mast, pitman lifts, barrel helix, crown gondola, luffing derrick, gravity balance, traction tram, slab incline, and stone wheel | Use actual placed instances and ports, not factory availability |
| Yard equipment | Forklifts and yard crane as possible local moving supports | Not a continuous tower transport system; operator and load conflicts |
| Bay leaf | Existing local moving walkway/trap component | Preserve its dedicated encounter and escape behavior |

Machine routes are distributed between the band builders and `vertical::Route`. The machine factory also supports types whose existence in the catalog does not establish a placed, connected instance. [Stack machinery](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/band_stack.cpp) [Band machinery](https://github.com/IssisX/ScraperX/tree/27d86a96d89e129a20c10c9298361d6e11581076/src/sim) [Placed vertical machinery](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/vertical/vertical_route.cpp) [Machine factory](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/vertical/vertical_machine.cpp)

**Important distinction:** Machine travel may skip intermediate decks. A 44 m lift does not automatically give Matron access to the ring halfway through its stroke.

## Map: Perception Assumptions

### Current assumptions

| Assumption | Code behavior | Multi-deck failure |
|---|---|---|
| A floor is primarily a height | `floor_of` searches known floor heights and falls back to her current level | Unknown landings can be assigned to the wrong deck |
| The encounter fits a fixed footprint | Player gating uses approximately `x = ±27`, `z = -177…-123` | External receivers and upper routes fall outside it |
| A climb starts on her current level | Player-climb inference searches authored climb columns from that level | Descent and multi-hop traversal are poorly represented |
| A pipe connection is one nearby story | Special gating recognizes the current pipe and adjacent Stack pitch | Does not scale into a route network |
| Sound bends around one central shaft model | Base `carry_m` uses known deck heights and a fixed shaft projection | Tapered rings, transfer plates, walls, and external machinery are not faithfully represented |
| Floor movement is vertical | `feel_floor` examines support linear vertical velocity | Horizontal and rotating machinery need point velocity and support identity |
| Arrival is near a known floor height | `settle_level` switches by vertical proximity | Passing a deck or standing on an unrelated platform can be mistaken for arrival |
| Trails are spatial truth | Current breadcrumb collection is gated by deck membership rather than explicit visual confirmation | Hidden movement can leak into pursuit |

These are visible in `floor_of`, `carry_m`, `feel_floor`, `settle_level`, and `pre_step`; expanding traversal without addressing them would expand existing perception errors. [Floor and acoustic helpers](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/matron.cpp#L270-L359) [Support detection](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/matron.cpp#L639-L689) [Perception and pursuit](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/matron.cpp#L1690-L2110)

### Dot work worth preserving

The dot implementation separates selected sound evidence from visual confirmation, retains source provenance, bounds acoustic work, and replaces ladder ascent/mantling with reachable real contacts and capped forces. Its hold finder uses body-local anchors, rejects sensors as support, and applies reaction force to dynamic supports. [Dot implementation](https://github.com/IssisX/ScraperX/blob/c0018eb1756e1e22c9080405bedce6a545dcea7c/src/sim/matron.cpp#L870-L1060)

However, its climb search is still designed around advancing upward on an authored climb. It is not already a general descending, pipe-gripping, jumping, shimmying, multi-deck controller. [Dot climb implementation](https://github.com/IssisX/ScraperX/blob/c0018eb1756e1e22c9080405bedce6a545dcea7c/src/sim/matron.cpp#L822-L1060)

**Proposed evidence model:**

- Store source position, source kind, observation time, confidence, and optional support-region estimate.
- Distinguish observed traversal from inferred destination.
- Allow a heard player on a ladder to suggest an exit without confirming arrival.
- Keep decoys independent of the hidden player's subsequent motion.
- Expire evidence during long travel; a planned route does not refresh it.
- Permit authored patrol knowledge across many decks without making the player globally audible.
- Keep acoustic connectivity separate from traversable connectivity.

## Design A: Route Graph

### Independent approach

Build an authored directed graph across support regions and traversal endpoints. Each edge specifies its physical executor, prerequisites, entry corridor, exit region, direction, expected duration, and failure behavior.

Suggested edge kinds:

- Walk or balance
- Step or mantle
- Ladder ascent
- Ladder descent
- Pipe ascent
- Pipe descent
- Jump-to-hold
- Shimmy
- Cable ride
- Machine boarding, ride, and dismount

Each edge remains a **request**, not an animation or transform path. Before engagement, the executor must establish actual support and clearance.

### Strengths

- Makes disconnected decks and missing capabilities explicit.
- Allows predictable patrols and pursuit across many levels.
- Supports deterministic route selection and authored difficulty.
- Provides an inspectable answer to "why did she stop?"
- Allows different up/down routes and stage-dependent capabilities.

### Adversarial judge

**Refutation:** Adding inter-level edges to the current `Level` model does not fix its height-based localization, horizontal blocked-node memory, one-hop pursuit, or physical traversal. A graph can confidently route her through a missing rung or to an upper ring whose actual approach is too narrow. The current code demonstrates that represented levels and usable routes are already different things. [Current graph and movement](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/matron.cpp) [Movement recovery](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/matron_moves.cpp)

**Required repairs:**

- Introduce spatial support regions rather than extending float-height identity.
- Validate every edge against real geometry at engagement.
- Revalidate active support every tick.
- Replan after physical failure without relocating her.
- Store bounded route-search scratch space instead of multiplying per-tick allocations.
- Give every directed edge an explicit cancellation and recovery contract.

**Verdict:** Best planning foundation; insufficient without physical executors.

## Design B: Physical Holds

### Independent approach

Use local physical hold discovery to climb the tower's actual ladders, pipes, panels, lips, and structural members. The planner chooses a destination region, while a short-horizon contact planner searches reachable supports.

Retain the single NPC capsule initially. Do not require a ragdoll or independently simulated fingers to prove multi-deck travel.

### Physical contract

- Hold identity includes body identity, generation, subshape or equivalent contact identity, and body-local anchor.
- Reach and line-of-reach checks precede acquisition.
- Contact, support existence, and geometry remain valid before force application.
- Force is support-relative and capped across all simultaneous traversal actuators.
- A dynamic support receives the opposite force at the anchor.
- Overload, excessive reach, lost contact, or stalled progress releases the hold.
- Release preserves momentum.
- Descent uses controlled lowering and downward hold transitions.
- Pipe climbing uses an explicitly bounded grip/friction model; an arbitrary point on a smooth vertical wall is not a rung.
- Mantles require a clear swept corridor and actual landing support.

### Adversarial judge

**Refutation:** The dot ladder code proves one important primitive, not universal climbing. Its search advances to higher upward-facing contacts; a smooth standpipe has no sequence of rung tops. Descent, jump catches, lateral shimmies, ring transitions, and hatch entry need different acquisition and control logic. [Dot hold acquisition](https://github.com/IssisX/ScraperX/blob/c0018eb1756e1e22c9080405bedce6a545dcea7c/src/sim/matron.cpp#L886-L1058)

**Additional failure cases:**

- Revalidating only a body ID can accept the wrong member of a compound support.
- Clear poses at the beginning and end do not prove that the intervening capsule sweep is clear.
- A bounded force can still act like an invisible thruster if it is not constrained by a valid grip and plausible load direction.
- Two individually capped controllers can exceed the intended total effort.
- Unbounded local search can consume too much simulation time.
- A locally reachable hold may lead into a dead end.

**Required repairs:**

- Use separate rung, ledge, pipe, and jump-catch capability rules.
- Preserve contact provenance through compound geometry.
- Bound probes, candidate storage, and search depth.
- Use route hints to constrain local search.
- Reject unsupported transitions instead of increasing force until they work.

**Verdict:** Required execution foundation; too risky as the sole tower-scale planner.

## Design C: Tower Machines

### Independent approach

Make Matron a physical passenger and operator of existing machinery. She walks to a receiver, boards when the actual gap and relative velocity permit it, operates reachable controls, rides under ordinary load, and dismounts onto verified support.

The machine supplies motion through its existing mechanisms. Matron must not receive a private motor, payload exemption, or script that sets machine travel.

### Strengths

- Produces distinctive movement rather than endless ladder climbing.
- Reuses the tower's mechanical identity.
- Allows long vertical travel with comparatively few climbing transitions.
- Creates physical interactions with catches, ballast, rigging, and machinery noise.

### Adversarial judge

**Refutation:** The existing vertical-route update accepts player-oriented action and support inputs, and includes special machine state logic. Calling it again for Matron could advance machines twice, conflict with player commands, or operate controls remotely. Some machines are one-shot or depend on consumable state; none can be presumed to provide a return journey. [Vertical update logic](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/vertical/vertical_route.cpp#L343-L430) [One-shot Well B](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/band_counterweight_well.cpp#L299-L338)

**Required repairs:**

- Separate read-only machine state from actor interaction requests.
- Arbitrate player and NPC requests before one machine update per tick.
- Require proximity, reach, and any physical handle/contact conditions.
- Include Matron's actual mass in the mechanism's load.
- Represent spent, broken, unavailable, occupied, or unrigged machines as unavailable edges.
- Use support point velocity, including rotation, while boarding and riding.
- Provide a different return route where a machine cannot reverse.

**Verdict:** Valuable optional transport; unsuitable as the sole guarantee of broad up/down mobility.

## Synthesis: One Plan

### Architecture boundaries

| Component | Responsibility | Must not do |
|---|---|---|
| Support-region catalog | Describe real walkable patches, landings, hatch approaches, and machine receivers | Infer floor identity from height alone |
| Directed route graph | Choose a sequence of feasible traversal attempts | Move bodies |
| Traversal executor | Acquire contacts and apply finite physical control | Read hidden player coordinates to repair pursuit |
| Perception memory | Supply bounded, aging evidence | Confirm an inferred destination |
| Machine adapter | Expose state and arbitrate legal interaction requests | Update machinery twice or bypass its mechanics |
| Presentation | Render native pose, holds, and attention | Supply collision support or own body movement |

### Persistent identities

Use stable authored identifiers for regions and graph edges. Physical references must also account for body lifetime and compound-contact identity.

A region should include its footprint and support association. It may be a ring band, a transfer plate, a narrow approach board, a machine receiver, or a moving platform, not necessarily an entire deck.

### Traversal lifecycle

1. **Plan:** Select a directed route from actual support to a patrol destination or evidence-backed search region.
2. **Approach:** Reach the edge's physical entry corridor.
3. **Validate:** Check support, geometry, capability, occupancy, machine state, and load.
4. **Engage:** Establish a real hold or board a real platform.
5. **Execute:** Apply bounded control with continuous support validation.
6. **Exit:** Establish actual landing or next support.
7. **Commit:** Update region identity only after physical arrival.
8. **Recover:** Release, fall, land, wait, or replan without teleporting.

Ordinary changes of target should not make her let go in midair. Evidence expiry can cancel the destination while the executor finishes reaching the next safe support.

### Player independence

The player's existing traversal code already contains physical support reactions, bounded hang stabilization, and grip-load failure behavior. Reuse geometric ideas where useful, but do not introduce a shared strength multiplier or Matron-dependent player branch. [Player climb mechanics](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/physics_world_climb.cpp) [Player hang mechanics](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/physics_world_traversal.cpp#L662-L715)

Required contention policy:

- Matron waits or selects another approach when her capsule would obstruct the player's required clearance.
- Route reservations guide NPC decisions only; they do not disable player holds.
- Matron cannot use the player as climbing support.
- Real collision and shared-support loading remain physical, not hidden proximity penalties.
- Player traversal receives its own query budget; NPC work must not exhaust it.

### Capture and fatality integration

Existing edge selection for capture already consults the current level, but also uses fixed shaft dimensions. More decks and tapered geometry require region-specific rim candidates and clearance. [Capture rim selection](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/matron_hold.cpp#L111-L225)

The fatality implementation also uses specific physical facilities. Do not route a captured player toward facilities on another disconnected deck, invent equivalent hooks, or replace the full sequence with a weaker shortcut. Treat capture/fatality availability as an explicit physical capability, and preserve the existing encounter's intensity and regression requirements. [Fatality behavior](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/matron_fatality.cpp) [Physical fatality integration](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/src/sim/physics_world_fatality.cpp)

## Execution Steps

All files listed below are **proposed implementation targets**, not changes made by this proposal.

| Step | Work | Files | Exit gate |
|---|---|---|---|
| Establish baseline | Reproduce existing failures, record deterministic fixtures, define physical invariants | `tests/sim_tests_matron.cpp`, `tests/sim_tests_matron_grab.cpp`, `tests/sim_tests_matron_moves.cpp` | Existing failures identified without relaxed assertions |
| Reconcile dot work | Review and integrate selected hearing/physical-climb changes without replacing newer capture/fatality work | `matron.cpp`, `matron.hpp`, relevant bridge/presentation/tests | Source provenance recorded; existing encounter preserved |
| Catalog support regions | Register authored regions and route endpoints beside their geometry owners | New `matron_routes.hpp/.cpp`; `band_*.cpp`; `vertical_route.*`; `physics_world.cpp` | Complete machine-readable candidate inventory with unresolved edges disabled |
| Separate planning and execution | Introduce directed edges, bounded search, region-scoped blocked memory, and edge-specific progress | `matron_routes.*`, `matron.cpp`, `matron_moves.cpp` | No body motion from graph code |
| Physical ladder travel | Generalize verified rung acquisition, ascent, descent, top entry, and mantle | New `matron_traversal.hpp/.cpp`; `matron.*` | 143↔154↔176 passes in both directions |
| Physical pipe travel | Replace legacy gravity-off pipe staging; support finite grip, slip, flange impact, and recovery | `matron_traversal.*`, `matron.*` | Existing 132–154 pipe works physically before range expansion |
| Expand nearby routes | Add 176↔198↔220 and ladder/board connections farther upward | Route registrations and traversal executors | Real holds, blocked-route failure, and return routes verified |
| Add machine rides | Introduce actor-neutral requests, state snapshots, boarding, load, ride, and dismount | `vertical_route.*`, machine adapters, `physics_world.cpp`, relevant band interfaces | Exactly one machine update per tick; no remote operation |
| Expand tower coverage | Enable verified lower-Stack, transfer-plate, upper-ring, and hatch connections | `band_*.cpp`, route registrations | Reachability matrix distinguishes up, down, conditional, and unavailable |
| Integrate perception | Replace fixed footprint/height assumptions and hidden breadcrumb leakage | `matron.cpp`, `matron_moves.cpp`, hearing tests | Decoys, occlusion, expiry, and hidden movement remain correct across decks |
| Integrate capture/save/view | Region-aware encounter capabilities, traversal-safe restore, native telemetry | `matron_hold.cpp`, `matron_fatality.cpp`, `simulation.*`, bridge and figure scripts | No stale anchors, phantom arrival, or presentation-owned movement |

### First playable milestone

The first implementation should prove a small but meaningful network:

- Physical travel among **132, 143, 154, and 176.25 m**
- Both ladder ascent and descent
- Converted corner-pipe ascent/descent
- Bay escape preserved
- A route failure that causes a genuine release and recovery
- Evidence-backed pursuit up and down, plus an independent patrol circuit
- Player climbing unchanged with Matron nearby

Only then add the mixed 176→198→220 route. Its smooth pipe and jump-to-panel transition make it a better second milestone than immediately enabling every upper ladder.

### Coverage target

"Many more decks" should mean a verified connected network, not a larger floor array.

The eventual acceptance inventory should account for every listed deck and candidate connection through the upper backup ladder chain, plus external machine and summit routes. Any unavailable connection must have a concrete reason: missing capability, capsule clearance, inaccessible approach, unavailable machine, excessive load, or absent return route.

Do not claim continuous grade-to-summit pursuit until the entire required chain has been physically exercised.

## Tests and Acceptance

### Physical traversal

| Test | Required assertion |
|---|---|
| Full 143↔154↔176 circuit | No traversal transform snapping; actual entry, holds, exits, and supporting floor |
| Ladder descent | Controlled top entry, downward progression, and physical landing |
| Missing rung | No invented hold; valid fallback, release, or route refusal |
| Removed held member | Contact invalidation releases support before another actuator step |
| Same compound, wrong member | Matching body ID alone cannot preserve the hold |
| Sensor-only ladder | Sensors never support her weight |
| Blocked mantle corridor | No passage through the obstruction |
| Smooth standpipe | Bounded pipe grip, or explicit inability, not rung acquisition in empty space |
| Dynamic support | Equal reaction applied at the support contact |
| Overload | Finite cap maintained; overload can cause failure |
| Interrupted climb | Destination change does not erase momentum or force unsafe disengagement |
| Fall across decks | Passing a floor height does not mark arrival |
| Repeated vertical coordinates | A blocked node on one deck does not block the same `x,z` on another |

### Perception

- A hidden player changing decks does not update the selected lead.
- A louder crate impact remains the lead after the player moves elsewhere.
- A heard ladder movement permits an exit hypothesis, not confirmed arrival.
- Evidence expires during a long climb.
- Quiet can lose pursuit without deleting patrol knowledge.
- Unknown landing height is not silently assigned to Matron's current deck.
- External machine receivers are not rejected by the old fixed footprint.
- Acoustic and traversal queries remain bounded under saturation.
- Visible reacquisition replaces estimates with a real observation.

### Machines

- Matron cannot operate an unreachable control.
- Player and NPC requests are arbitrated before a single machine update.
- One-shot machinery remains spent.
- Boarding fails safely when the platform is absent or moving too fast.
- Her mass affects load, speed, balance, and overload normally.
- A rotating or horizontally moving platform uses point-relative motion.
- A machine stalled under load does not trigger teleporting or increased force.
- Return planning does not assume that an ascent machine reverses.

### Player nonregression

Run identical player inputs with:

- Matron absent
- Matron present but distant
- Matron nearby without contact
- Matron on the same static structure
- Matron on a separate dynamic support
- Matron sharing a genuinely loaded dynamic structure
- Matron waiting at the same ladder or hatch

For the no-contact cases, verify unchanged player grip parameters, reach, climb command response, hold selection, and progress within the established deterministic tolerance. For shared dynamic structures, attribute differences to measured physical loading rather than an NPC-dependent modifier.

### Long-route scenarios

- Patrol from below home deck to above the first ring and return.
- Chase an audible target upward, lose it, and continue to safe support.
- Investigate a decoy below her, descend, search, and abandon it.
- Replan around a spent machine.
- Traverse the 340 and 640 transfer-plate boundaries.
- Ascend and descend the upper hatch-ladder chain.
- Restore during approach, grip, ride, fall, and post-landing recovery.
- Repeat identical scenarios to compare route decisions and physical outcomes.

### Instrumentation

Expose:

- Current support region and physical support
- Planned edge and executor phase
- Hold position and contact identity
- Force cap, applied force, and support reaction
- Reach, overload duration, and release reason
- Route failure and replanning reason
- Machine state and pending interaction request
- Evidence source, age, confidence, and inferred destination
- Query counts and bounded-storage saturation

Debug displays must describe native state; they must not influence movement or support.

## Baseline Findings

An out-of-tree Release native build completed with GNU 15.2. Focused verification produced the following results; these are baseline observations, **not** validation of the proposed redesign.

| Check | Observed result |
|---|---|
| `SCRAPERX_ONLY=matron-moves` | Passed; two PASS records |
| `SCRAPERX_ONLY=matron-grab` | Failed after seven PASS records at `matron fatality: every beat, in order` |
| `SCRAPERX_ONLY=bay` | Passed; three PASS records |
| `SCRAPERX_ONLY=climb` | Passed; nine PASS records |
| `SCRAPERX_ONLY=zipline` | Passed |
| `SCRAPERX_ONLY=twin-geometry` | Passed; two PASS records |
| CTest `shared_survival` and `parkour_flow` | Both passed |
| Isolated `SCRAPERX_ONLY=matron` rerun | Timed out at 90 seconds after fourteen PASS records; not a clean pass |

The existing test selector and target definitions are in the native harness and CMake configuration. [Native selectors](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/tests/simulation_tests.cpp) [Test targets](https://github.com/IssisX/ScraperX/blob/27d86a96d89e129a20c10c9298361d6e11581076/CMakeLists.txt)

Earlier overlapping test execution produced inconsistent/interleaved logs and is excluded from acceptance evidence. The isolated fatality failure and broad Matron timeout must remain visible; neither should be relabeled as a redesign regression or waived by weakening the tests.

The dot handoff reports successful focused ladder/hearing verification under a different build environment. Those reported results were inspected but were not reproduced during this audit. [Dot verification record](https://github.com/IssisX/ScraperX/blob/c0018eb1756e1e22c9080405bedce6a545dcea7c/03_EXECUTION/MATRON_REDESIGN/HEARING_CLIMB_HANDOFF.md)

## Risks and Gates

| Risk | Severity | Required gate |
|---|---|---|
| Broad dot-branch integration overwrites newer encounter work | High | Review selected changes against the pinned base; no wholesale file replacement |
| Larger graph hides nonphysical legacy movement | High | Audit every enabled executor, including pipe, cable, vault, and mantle |
| Smooth pipes become invisible elevators | High | Explicit contact/grip model, finite force, overload and missing-contact tests |
| Downward travel is assumed from successful ascent | High | Separate directed descent fixtures |
| Height-only localization creates phantom arrival | High | Real support and spatial-region confirmation |
| Machine integration bypasses controls or updates twice | High | One update owner and actor-neutral request arbitration |
| Player climbing degrades nearby | High | Parameter invariance and same-input proximity tests |
| Upper hatches do not fit her capsule | High | Full-capsule corridor checks; disable impossible edges |
| Long pursuit becomes omniscient | High | Evidence provenance and absolute expiry |
| More queries create device stalls | Medium–high | Separate bounded budgets, profiling, and Android validation |
| Existing fatality failure obscures integration quality | High | Reproduce and resolve or document an explicitly accepted baseline before implementation acceptance |
| Save/restore retains invalid support anchors | High | Stable identity, generation validation, and physical fallback |
| Generated collision drifts from presentation | Medium | Authoritative export and geometry consistency checks |

## Completion Definition

Implementation is complete only when:

- The route inventory accounts for every deck and candidate traversal family in scope.
- Enabled connections are demonstrated physically in their declared directions.
- Matron can repeatedly travel both above and below her original encounter area.
- Unsupported or unavailable routes fail explicitly and recover without teleporting.
- No enabled traversal relies on gravity-off waypoint transport or unbounded attachment forces.
- Machinery retains its actual state, load, controls, and update ownership.
- Hidden player movement cannot steer pursuit.
- Player climbing remains independent of Matron proximity.
- Existing encounter, bay, capture, fatality, parkour, cable, save/restore, and deterministic replay gates are satisfied.
- Performance is measured on the intended device class before making Android suitability claims.
