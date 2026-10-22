# Matron routecraft handoff

Status: focused native verification passed; final full suite is running. This
checkpoint is not yet a fully verified integration delivery.

Exact base: `e098f5897066497700b7c93a2c8c2b6dc7b3314f`.
Branch: `dot/matron-routecraft-20261022`. Initial implementation checkpoint:
`750c52d5cef9c9e303f2f71a43621ffce05a0f1a`.
Final code/test revision: `65c826bfd5d0ca507957fb5f8dca38b447bacb67`.
Later documentation/evidence commits do not change the tested code.

## Implemented family and physical ownership

Static horizontal graspable box members, including native compound ladder rungs
and scaffold bars, approached from one supplied horizontal direction. Matron uses
her existing 220 kg, 2.1 m upright, translation-only capsule. One aggregate grasp
carries the load. This is a reduced climbing model, not articulated whole-body
balance, foot placement, anatomical contortion, or a second authoritative body.

The native controller discovers members from collision geometry, plans body and
contact states, and executes ascent, descent, lateral transfers, and an above-rung
posture that permits a collision-clear top transfer. It can descend before ascending.
Neither production extraction nor search reads route entity IDs or authored ladder
coordinates. Fixture placement and caller-provided goals are configuration, not
solutions supplied to the search. Final diff inspection found no target-specific
runtime selection, named-deck lookup, or authored-planner fallback in routecraft.
Existing authored pursuit systems remain outside this opt-in controller.

Execution uses `AddForce` at native 90 Hz, with gravity enabled. Actuation is capped
at 6000 N and absolute mechanical power sampled before each physics step at
9000 W. This is a discrete controller power cap, not a continuous solver-time
power constraint during impacts. Speed demand is capped at 0.9 m/s;
the velocity servo gain is 7/s. There are no routecraft transform writes, velocity
writes, gravity-factor changes, moving constraints, or collision-layer changes.
The actual capsule remains collidable throughout. `Report::reaction` is the analytical equal-and-opposite reaction of the modeled
static-anchor actuator, assigned as negative applied force at `Report::hand`.
There is no separate Jolt hand body or contact constraint, and this value is not
an independently measured hand-contact impulse. Static geometry is anchored, so
it has no dynamic momentum to update. This does not model structural yielding or
support torque. Grip capacity is a chosen finite
actuator limit, not a biological measurement.

## Source grounding

- `src/sim/physics_world_climb.cpp::find_grip` and
  `physics_world_support.hpp::{leaf_box,box_is_hold}` provide the existing geometric
  eligibility: box length at least 0.25 m and two thin sections at most 0.18 m.
  Routecraft reuses those helpers without changing player rules or execution.
- `src/sim/band_counterweight_well.cpp::build_climbing_route` creates the actual
  compound 154–176.25 ladder used in the production-geometry tests. The full-world
  test also uses the normal `Simulation` tower and real deck geometry.
- The prior finite-force prototype at `c0018eb1756e1e22c9080405bedce6a545dcea7c`
  was inspected for gravity-on control, local anchors, force limits, and releases.
  No unrelated commits were cherry-picked.
- Pinned Jolt `e77f175595e64cb44218cc9d9d56fc365ad0e36a` supplies collision sweeps,
  body forces, and shape identity. `CompoundShape::GetLeafShape` performs a bounds
  check before cached subshape decoding, unlike `GetSubShapeTransformedShape`.
  Routecraft uses that guard before revalidating a retained anchor.

## Search and execution bounds

The search region encloses start and goal with a two-meter margin. Requests are
limited to 28 m vertical separation and 8 m on each horizontal axis. Each native
collector accepts at most 2048 hits; overflow rejects the result rather than
selecting an unstable prefix. Unique body/subshape leaves are sampled once, at
most five points per horizontal member. A cheap catalog preserves all discovered
heights and lateral branches, with at most 71680 poses (2048 × 5 × 7).
There is no raw 192-grip rejection or nearest-start/height-prefix truncation.

Deterministic A* admits at most 1536 reachable body/contact states, with cached
lazy pose clearance and visibility. The two standoff choices doubled the prior
768-state allowance; the main scalability change is deduplicated extraction,
lazy validation, and spatial adjacency, not an unlimited query budget. A sorted
1.4 m spatial grid restricts subsequent edges to 27 neighboring cells; the source
uses a wider neighborhood preserving its 3.2 m acquisition-edge limit. Subsequent
edges are at most 1.4 m. Cheap endpoint reach checks precede native queries.
Euclidean path cost and heuristic permit sideways and downward detours. Geometric
keys, then body/subshape identity, determine stable candidate and tie ordering.

Each grip contributes body-center offsets of -0.65, +0.35, and +1.45 m vertically,
at both 0.40 and 0.55 m from its approach side. Both use identical swept clearance;
the narrower stance permits legal passage beside nearby steel without shrinking
the capsule. The supplied goal can also be a body/contact
node. An edge acquires the new
support from the current body posture before releasing the old support. The
capsule is swept continuously; hand visibility/reach is sampled every 0.2 m along
the transition. This is a point-hand model, not an arm-volume sweep. Reach is at
most 1.65 m from capsule center. The terminal ray must hit the selected member,
with at most 0.13 m of terminal distance inside the member's thin section.

`Report::native_queries` and `Simulation::matron_climb_queries()` count actual
ray, overlap, and sweep calls. The combined geometric operation budget is 200000
per request, including
execution validation. `Report::queries` counts charged operations: one clearance
operation can issue both overlap and sweep queries, so at most 400000 native query
calls are possible. A request lasts at most 120 s; its accumulated sampled absolute
power times timestep is bounded by 1.08 MJ. Solver-time work can differ from this
discrete estimate, especially during impacts; no exact work-integral claim is made. Three replans are allowed after failures;
stalls time out after 2.5 s. Failed approaches are excluded locally for that
request and forgotten on a new request, rather than globally blacklisting a hold.
Discovery, hand rays, overlaps, and sweeps use the actual Matron body collision-layer
filters and exclude self and sensors. Collector overflow during clearance is
remembered: unsuccessful searches report `BudgetExceeded`, not a false exhaustive
`NoRoute`. Contacts are checked every tick against body presence, motion type, subshape,
local position, reach, visibility, and current capsule clearance. Changed geometry
triggers a new local extraction. Replanning failure applies no contact force.

`NoRoute` means no route in this supported sampled posture set. It is not proof
that every physically possible climb is impossible. Discrete posture and bar
sampling can miss narrow or otherwise awkward routes. The search does not cover
all approach orientations, vertical pipes, curved members, mesh edges, or moving
machines. Motion-type changes and displaced static anchors invalidate a retained
contact; moving-support traversal and momentum transfer are explicitly deferred.

## Interfaces and integration

Files: `src/sim/matron_routecraft.hpp/.cpp`, additive calls in `matron.cpp`, and
native wrappers in `simulation.hpp/.cpp`.

```cpp
// Between fixed steps; goal is capsule center, not feet or hand position.
const bool accepted = simulation.request_matron_climb_goal(
    {5.0, 177.37, -128.6}, {0.0, 0.0, 1.0});
const int status = simulation.matron_climb_status();
// Explicitly release before ordinary pursuit should resume.
simulation.cancel_matron_climb_goal();
```

Those coordinates are only an example for the existing ladder. The caller must
first bring Matron within reach and unobstructed visibility of some legal starting
support. A released landing is not automatically a reacquirable departure; callers
choose an appropriate landing/approach goal. The new controller
does not autonomously replace the existing chase routing or approach walking.
`Matron::request_climb_goal(goal,facing)` is the lower-level native interface;
`climb_report()` exposes charged operation counts, plans, selected-contact counts,
applied-force peaks, power, reach, and analytical static reaction. `acquisitions`
increments for the initially selected route state and subsequent cursor handoffs;
it is not an independently measured contact count. Older verbatim test output
labels this counter as `grips` or `acquisitions`; interpret it with these semantics.
A selected route applies no force by itself. Every force-producing step first
validates the current anchor and the actual-position-to-target transition. Failed
replanning returns before `AddForce`. No Godot gameplay simulation was added.

Statuses are `Idle=0`, `Moving=1`, `Holding=2`, `NoRoute=3`, `Released=4`, and
`BudgetExceeded=5`. `Holding` means physically within 8 cm with speed below
0.25 m/s; it retains the finite grip controller and can occur while suspended.
It does not assert a grounded arrival. A caller may request a new descent from
that hold or explicitly release after checking the intended landing. A rejected
replacement goal preserves the active controller. Rejected inactive requests
expose their failure status. Timeout/retry exhaustion releases rather than looping.

`Matron::pre_step` owns the actuator while active; normal pursuit is unchanged
when it is inactive. Native floor sensing and impact sensing continue during
climbing. Supported descent tracks the last physically supported height so the
old level-based fall rule does not kill a safe descent on controller exit. Native
ground contact establishes the recovery level before the fall decision.
`begin_grab`, knockdown/stagger, death/removal, restore, and explicit exit clear
contact ownership. Existing save data does not persist transient climbing state.
Native view fields expose climbing, hand location, and contact force.

`launch_matron_for_test` is fixture initialization only, never exposed to Godot.
It positions the real Matron before test execution; no test uses it during a route.

## Verification and review

Release host build used GCC 14.2.0, CMake 4.4.4 and Ninja 1.13.2. The environment
initially lacked CMake/Ninja; they were installed outside the repository. Initial
configure attempt exited 127 (missing tool), then the requested configure completed
with exit 0. Final build and focused selector both exited 0:

```sh
cmake -S . -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --target scraperx_sim_tests -j 4
SCRAPERX_ONLY=matron-routecraft build/host/scraperx_sim_tests
SCRAPERX_ONLY=climb build/host/scraperx_sim_tests
SCRAPERX_ONLY=matron-grab build/host/scraperx_sim_tests
build/host/scraperx_sim_tests
```

The new selector is registered in `tests/simulation_tests.cpp`, included in the
normal full run, and wired in CMake. Focused coverage comprises 23 fixture/world
runs in 22 groups: two identical replays; production compound ladder; full tower;
shifted lateral transfer; downward backtracking; blocked body path; all grips
removed and landing; new obstruction and lateral replanning; one grip removed
and interrupted recovery; disturbed power saturation and restore; direct capture
interruption; displaced static anchors; an unfamiliar shifted sparse rung pitch; a selected
contact subsequently occluded; a removed mutable-compound child; kinematic support
rejection; sensor/noncolliding-layer grip and barrier exclusion with actual floor
recovery; full-tower panel ascent/descent; independently measured production-panel
execution; a translated 280-grip fixture with distractors and lateral obstruction;
a genuine 2050-body collector overflow; and a separate 6000 N force saturation case. Upper-level establishment, safe lower
deck recovery, and rejected goal replacement are asserted in the full-tower case. Each direction
ends with 90 ticks standing on the actual destination with no grip or carried
object, alive, after an initial settling interval. Every full-tower tick is checked
for movement below 0.2 m; observed ladder maximum was 0.0466674 m; panel maximum was 0.0173981 m.
All fixture ticks also enforce the 0.2 m displacement bound.

Tests inspect Jolt's actual accumulated force and power before integration,
contact reach, gravity factor, real capsule penetration, physical progress,
selected contact-state transitions, and repeated end positions. The accumulated
body force, motion, and overlap measurements are independent Jolt observations;
the contact selection and analytical reaction are controller state. Planning allows at most 15 mm of
initial/resting overlap. Tests require actuator-controlled overlap at most 25 mm,
a small solver/contact tolerance relative to the 300 mm radius. Controlled overlap
was zero in the observed lateral and interrupted-recovery fixtures.

Ordinary released falling is measured separately: the five-meter release reached
about 9.8 m/s and transiently overlapped the floor by 0.0694488 m before settling.
Its test bound is one 90 Hz impact travel step at 10 m/s plus 15 mm contact tolerance
(0.1261 m), followed by an actual settled floor-height check. This is a wider bound
than the controlled-climb 25 mm bound, and is not a claim of penetration-free
recovery. A temporary continuous-collision mode change was tested, gave no measured
benefit for this shallow-overlap case, and was removed. The final code preserves
existing falling/collision-mode behavior. The body lands rather than tunneling
through the floor in these tested releases. Deliberate velocity disturbance is
supplied by a real impulse: the measured actuator saturates at 9000 W and 4500 N
at 2 m/s.

## Host planning measurements and held-out qualification

Final focused run, one sample per full-tower stage:

| Operation | Host elapsed ms | Actual native queries |
| --- | ---: | ---: |
| Initial ladder request | 6.83817 | 6986 |
| Ladder replan at y=162.802 | 4.52942 | 4666 |
| Ladder replan at y=170.522 | 2.46971 | 2458 |
| New-obstruction fixture replan | 1.51178 | 3481 |
| Panel ascent request | 27.9307 | 24340 |
| Panel ascent replan at y=206.758 | 22.9661 | 20136 |
| Panel ascent replan at y=213.864 | 12.1098 | 9958 |
| Panel descent request | 16.2267 | 15083 |
| Panel descent replan at y=214.871 | 11.8846 | 11462 |
| Panel descent replan at y=207.859 | 8.00875 | 7871 |

These are host observations, not worst-case guarantees or Android measurements.
An earlier run of the same planner observed 31.5482 ms for the first panel replan.
Planning is synchronous and these spikes exceed one 90 Hz tick's wall-clock
budget. Physics/controller integration still uses fixed 90 Hz steps; this is not
proof of sustained real-time scheduling. Explicit requests perform full replans;
fault timings include Matron pre-step but exclude physics integration. The fixed
200000-operation cap limits work counts, not wall time. Worst-budget latency is
unmeasured. Qualify deployment regions and scheduling before automatic chase adoption.

The previously held-out 198.25–220.25 panel exposed a real raw-candidate cutoff:
71 native hits, 71 unique leaves, 50 eligible horizontal leaves, and 200 samples.
There were no duplicate leaves in that case. The old 192 cutoff alone rejected it.
The final general search preserves its four samples per horizontal member; a
center-only sample would lie at a vertical-member crossing and can fail visibility.

The full native tower now executes from supported `(11,199.32,-131.41)` to
`(11,221.37,-130.55)`, releases onto the 220.25 ring, stands 90 ticks without a
hold, reacquires and descends to `(11,199.4,-131.41)`, then releases and stands
90 ticks on the 198.25 catwalk. Both requests face +z. These are fixture/deployment
goals, never runtime route constants. The initially tested top goal at z=-130.31
allowed normal pursuit to drift farther onto the ring during the released wait;
its grip ray then crossed steel. The nearer-edge goal preserves reacquisition.
A real bracket also blocks the original 0.55 m standoff; the generic 0.40 m
alternative clears it. Neither geometry nor collision tolerances were changed.

Terminal `Holding` is required before release in each direction; a lost grip and
lucky landing cannot pass. The lowest bar is reached without a jump in this
aggregate 1.65 m center-reach model. This does not establish anatomical reach for
an articulated rig. A separate production-panel fixture measures actual body
force (peak 3538.01 N), sampled power (2069.13 W), gravity, reach, and controlled
penetration (0 m). The full-tower case measures actual arrival and released support.
No pole, jump, dynamic-support, or ground-walking rewrite was added.

Project-specific peer discussion is tracked at
[repository issue 3](https://github.com/IssisX/ScraperX/issues/3). It prompted the
explicit timing measurements, held-out panel qualification, and shared grounded
arrival criterion. The integration owner handles further macro-route adoption.

An independent code/test review identified and led to fixes for stale compound
IDs, query bounds, failed-approach handling, actual-position transfer clearance,
impact sensing, failure reporting, descent recovery, transactional replacement,
and immediate capture cleanup. A final inspection confirmed those lifecycle fixes.

Focused verbatim output is in `routecraft_evidence/focused.log`. Final full-suite
verification of `65c826b` is running; this checkpoint is not yet integration-ready.
The preserved pre-scalability baseline `d5898b3` completed with exit 0 and 177 PASS
records (`baseline-full.log`), but does not validate the new search. Final targeted
regression and full-suite exit codes will be recorded in `verification.txt`.
An independent final review found no significant defect in the lazy search,
additional stance, or caller-owned goal adjustment; it specifically identified
the reacquisition and synchronous-latency limitations documented above.

Concise verbatim focused evidence:

```text
routecraft full tower grounded top-out y=177.3 standing_ticks=90
routecraft full tower lower standing_ticks=90 maximum_step_m=0.0466674
routecraft backtrack min_y=156.352 final_y=160.93
routecraft interrupted recovery peak_overlap=0.0694488 controlled_overlap=0
routecraft measured saturation N=4500 W=9000
routecraft measured force saturation N=6000 W=7800
panel arrived_y=221.3 standing_ticks=90 max_step=0.012384
panel arrived_y=199.3 standing_ticks=90 max_step=0.0173981
panel measured_force=3538.01 sampled_power=2069.13 controlled_overlap=0
dense fixture min_x=19.0664 max_x=19.9999 final_y=155.22
PASS: matron-routecraft
```

No Android build, installation, performance, thermals, or Fold execution was
measured. These are host correctness results only. Whole-body torque, automatic
chase adoption, and moving machines remain separate integration work.
