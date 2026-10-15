# Source ownership map

The 2026-10-15 modularization separates responsibilities without changing the public simulation API, native tick order, physics laws, input controls or authored geometry. Split by the owner of a decision, not by line count. Add behavior to its feature module instead of growing the entry points again.

## Native simulation

`src/sim/simulation.cpp` owns the public Simulation API, ordered input queue, fixed clock and render interpolation. `simulation.hpp` remains the public data/API contract. The private `simulation/world.hpp` declares the existing single PhysicsWorld, its lifecycle and state. It is not a new controller or a second world. State members and initialization/destruction order are preserved.

| Implementation under `src/sim/simulation/` | Responsibility |
| --- | --- |
| `world.cpp` | Construction/destruction, body creation and entity lookup |
| `tick.cpp` | Authoritative native tick sequence and physics-step callback |
| `runtime.hpp/.cpp`, `contacts.hpp` | Shared Jolt registration lease, collision layers and solved-contact observations |
| `constants.hpp`, `fixture_constants.hpp` | Existing movement bounds and authored fixture dimensions, respectively |
| `queries.hpp/.cpp`, `geometry.cpp` | Geometry records, body conversion and clearance/reach/footing queries |
| `support.cpp` | Actual soft/rigid support observations and support motion |
| `locomotion.cpp` | Existing movement, jump, crouch, landing recovery and parachute control |
| `climb.cpp`, `hand_transfer.cpp`, `affordance.cpp` | Real hand ownership/regrip, hand-foot transfer, and prospective opportunities/cleanup |
| `legacy_traversal.cpp` | Existing legacy path traversal, explicitly isolated pending the separately authorized physical migration; no new production authority |
| `grapple.cpp`, `carry.cpp` | Grapple command/readback and carried/rigged object lifecycle |
| `world_geometry.cpp`, `fixture_build.cpp` | Ordinary collision dressing/stack and retained explicit regression fixtures |
| `machine_build.cpp`, `machine_update.cpp` | Existing coupled-machine constraints/construction and finite machine operation |
| `checkpoint.cpp`, `snapshot.cpp` | Checkpoint/retry restoration and authoritative readback |

Feature source files compile independently and are explicitly registered in CMake. Header-only `.cpp` inclusion, duplicate state, extra constraints and new force/velocity owners are not substitutes for a module boundary. Changes to private shared state require rebuilding its dependent modules; an implementation-only feature change now rebuilds its own unit. The allocator/type-registration lease still has exactly one process-wide mutex/count definition.

## Godot presentation

| Source | Responsibility |
| --- | --- |
| `presentation/main.gd` | Lifecycle, ordered input delivery, native integration, camera/arms/UI and operation routing |
| `presentation/world_geometry.gd` | Shared mesh/material factories and bump textures; explicit presentation root |
| `presentation/stack_world_builder.gd` | Authored tower/stack geometry and its decorative gear phase |
| `presentation/alpine_world_builder.gd` | Mountain, basin, waterfall and vegetation construction |
| `presentation/worksite_machine_view.gd` | Worksite machine meshes, native transform mirroring and ambient effects |

Builders receive explicit geometry/light/native inputs. Main retains construction and update order. No presentation helper owns consequential simulation, caches a second gameplay state or writes player physics.

## Test scenarios

`presentation/ui/ui_test_driver.gd` keeps the stable scenario registry, fixture setup, explicit dispatch and process completion. Its small `test_scenarios/ui_test_harness.gd` supplies shared input/timing/native/capture utilities to nine typed suites: launch, opening, frames, campaign, crown, haul, ladder, controls and audio. Suites use the same harness instance; input sequences, assertions, await points and output/exit semantics are retained. No generic dynamic dispatcher or copied harness state is required.

The native `tests/simulation/` suites group the existing broad athletic test by domain. The existing executable and CTest name remain stable. Related same-life machine/route sequences remain together; the short main runner preserves test order.

## Boundaries and acceptance

A refactor must preserve member defaults/order, callback lifetime, native solve sequence and external script/test hooks. Relocated source-body comparisons support mechanical preservation; they do not replace linked runtime checks. Keep pre-existing failures distinct from regressions. Do not change numeric thresholds or test expectations to make a reorganization pass. Exact source/build/runtime evidence is recorded under `evidence/refactor-2026-10-15/`.

Other long files such as bridge binding tables and existing cohesive actuator/kit owners are not automatically candidates for splitting. Reassess them when their responsibilities or change coupling justify extraction; avoid arbitrary size limits and ownership-free helper dumps.
