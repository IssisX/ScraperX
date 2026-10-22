# Matron routecraft handoff

Status: focused native verification passed; final full suite is running. This
checkpoint is not yet a fully verified integration delivery.

Exact base: `e098f5897066497700b7c93a2c8c2b6dc7b3314f`.
Branch: `dot/matron-routecraft-20261022`. Initial implementation checkpoint:
`750c52d5cef9c9e303f2f71a43621ffce05a0f1a`.
Final code/test revision: `d5898b3208a08a357b610e123bcdaf5aa34d0fea`.
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
solutions supplied to the search.

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
limited to 28 m vertical separation and 8 m on each horizontal axis. Accepted searches contain at
most 192 extracted grip samples, 768 body/contact nodes, and 2048 collision hits
per collector; collector overflow rejects the result rather than selecting an
unstable prefix. At most five samples are taken along one horizontal member.
Candidates sort by world coordinates, then body and subshape identity.

Each grip contributes body-center offsets of -0.65, +0.35, and +1.45 m vertically,
0.55 m away from its approach side. The supplied goal can also be a body/contact
node. Breadth-first search is not monotonic in height. An edge acquires the new
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
Contacts are checked every tick against body presence, motion type, subshape,
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
first bring Matron within reach of some legal starting support. The new controller
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
normal full run, and wired in CMake. Focused coverage comprises 18 fixture/world
runs in 17 groups: two identical replays; production compound ladder; full tower;
shifted lateral transfer; downward backtracking; blocked body path; all grips
removed and landing; new obstruction and lateral replanning; one grip removed
and interrupted recovery; disturbed power saturation and restore; direct capture
interruption; displaced static anchors; an unfamiliar shifted sparse rung pitch; a selected
contact subsequently occluded; a removed mutable-compound child; kinematic support
rejection; and dense-panel budget rejection. Upper-level establishment, safe lower
deck recovery, and rejected goal replacement are asserted in the full-tower case. Each direction
ends with 90 ticks standing on the actual destination with no grip or carried
object, alive, after an initial settling interval. Every full-tower tick is checked
for movement below 0.2 m; observed maximum was 0.0466659 m.

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

Final focused run, one sample at each full-tower stage:

| Operation | Host elapsed ms | Actual native queries |
| --- | ---: | ---: |
| Initial full-tower ladder request | 2.83631 | 3051 |
| Mid-climb same-goal replan request at y=162.802 | 1.9924 | 2098 |
| Mid-climb same-goal replan request at y=170.666 | 0.931427 | 951 |
| New-obstruction fixture replan | 0.65617 | 1984 |
| Dense held-out panel request, rejected by budget | 0.160868 | 1 |

These are host observations, not worst-case guarantees or a representative Android
benchmark. The full-tower replans are explicit mid-climb requests, not geometry
faults. Fault timings include the entire native Matron pre-step that performed the
replan, but exclude the physics update. An earlier focused iteration observed
3.07959 ms for the initial full-tower request. The 200000-operation budget limits
work counts, not wall time; worst-budget synchronous latency is unmeasured. Callers
should qualify deployment regions before adopting this opt-in controller in chase.

The held-out 198.25–220.25 scaffold uses the production panel and catwalk from
`band_counterweight_well.cpp`. The request started at `(11,199.32,-131.41)` and
asked for `(11,221.37,-130.31)`, facing +z. Frozen rules returned `BudgetExceeded`
because the dense member sampling exceeded 192 accepted grips. This negative
qualification is explicitly tested and logged. No panel execution, jump necessity,
or physical impossibility is established. The disputed lowest-bar reach remains
a follow-up after a separately reviewed budget/sampling qualification; no jump,
pole, dynamic support, or walking rewrite was added to make the panel pass.

Project-specific peer discussion is tracked at
[repository issue 3](https://github.com/IssisX/ScraperX/issues/3). It prompted the
explicit timing measurements, held-out panel qualification, and shared grounded
arrival criterion. The integration owner handles further macro-route adoption.

An independent code/test review identified and led to fixes for stale compound
IDs, query bounds, failed-approach handling, actual-position transfer clearance,
impact sensing, failure reporting, descent recovery, transactional replacement,
and immediate capture cleanup. A final inspection confirmed those lifecycle fixes.

Focused verbatim output is in `routecraft_evidence/focused.log`. Full-suite status,
counts, and logs will be finalized after the ongoing run. Player climbing selector
passed with exit 0. Capture/arm/fatality passed with exit 0 on the final controller
code. The other two CTest executables (shared survival and parkour flow) passed
with exit 0. The previous full-suite revision passed with 177 PASS records; that
is supporting regression evidence, not the final revision result.

Concise verbatim focused evidence:

```text
routecraft full tower grounded top-out y=177.3 standing_ticks=90
routecraft full tower lower standing_ticks=90 maximum_step_m=0.0466659
routecraft backtrack min_y=156.065 final_y=160.98
routecraft interrupted recovery peak_overlap=0.0694488 controlled_overlap=0
routecraft measured saturation N=4500 W=9000
held-out panel accepted=0 status=5 plan_ms=0.160868 native_queries=1
PASS: matron-routecraft
```

No Android build, installation, performance, thermals, or Fold execution was
measured. These are host correctness results only. Whole-body torque, automatic
chase adoption, and moving machines remain separate integration work.
