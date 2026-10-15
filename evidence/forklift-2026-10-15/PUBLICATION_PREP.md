This is the initial preparation report. Root subsequently staged the selected31source files and built/tested the matching snapshot; current source/patch/route and publication status are in the live checkpoint and `selected-source-manifest.json`. Historical statements below describe this report’s original preparation boundary.

# Selective takeoff/forklift publication proposal — 2026-10-15

Prepared against actual `ChatGPT` HEAD `1f92dca096c007e95681dc0f73f400a171c17f94`. Root working files, index, branches, commits and remote were not modified. Private source snapshot and textual patch only; no native compilation or gameplay tests performed for this proposal.

Proposal directory: `/data/data/com.termux/files/usr/tmp/scraperx-forklift-publication-prep-20261015`. `base/` contains HEAD source via `git archive`; `source/` contains proposed complete src/Godot/tests/CMake/cmake/third_party/ci source surface. `proposed.patch` applies exactly to HEAD and only changes31feature files. Patch SHA256 `38aaa4b5c99c516a9fe51ad47ba0e668cd20e3e0fe416a639fef84b0afc77868`. `manifest.json` records original working hash, HEAD hash and proposed hash for every selected file, transformations, excluded owner hashes and route-refresh dependencies.

## Selected/excluded boundaries

- Include finite planted foot motor, retained real trailing plant, takeoff input timing/cancellation/readback owner and new buffer/flow tests; preserve existing native simulation owners.
- Include real warehouse forklift/yard native owners, Simulation cache/input/checkpoint/tick integration, bridge dictionaries/bindings, native mechanical/station/full-route tests, wheel view, Main input/HUD/operation flow, touch extension and controlled frontend test.
- Shared CMake includes fork/takeoff source and test wiring; physical_hook_gate removed from source and no-RTTI lists. Proposed new `scraperx_warehouse_forklift_route_tests` / `scraperx_sim.warehouse_forklift_route` sits alongside existing Jolt-only forklift targets. Root must freeze and accept route driver before publication.
- All grapple owners and existing grapple tests/view equal HEAD byte-for-byte; exclude dirty grappling_gun/reel/hook geometry/gate implementation and Simulation grapple snapshot changes. Restore HEAD single-argument Godot hook configuration and HEAD bridge hook geometry implementation.
- Remove only added gate snapshot members/interpolation/bind fields/API from shared Simulation/bridge sources. Retain all forklift and takeoff additions.
- Exclude Main ParkourYard preload/build, unintegrated parkour_yard.gd and dirty solid_export.gd changes. HEAD original rendering/world/export behavior otherwise preserved.
- Exclude historical dirty docs/evidence and unrelated untracked work. Root owns authoritative acceptance/checkpoint/backlog/doc updates for final delivery.

## Review/application mechanics

Read-only review:
```sh
python /data/data/com.termux/files/usr/tmp/scraperx-forklift-publication-prep-20261015/review.py
```

After root finalizes route files, refresh only those two captured drivers and regenerate patch:
```sh
python /data/data/com.termux/files/usr/tmp/scraperx-forklift-publication-prep-20261015/refresh_route.py
```

Root can inspect `proposed.patch` or `source/` directly. For deliberate staging after review, `git apply --cached --check <absolute proposed.patch>` followed by `git apply --cached <absolute proposed.patch>` would stage exact selected content while preserving dirty working files. These commands were NOT run; root must account for its existing index and refreshed route hashes before staging. A subsequent `git diff` against that selected index will intentionally still show excluded grapple/ParkourYard WIP in the working tree. Do not overwrite working files from the private source snapshot.

## Actual preparation evidence and remaining dependencies

`git apply --check` against extracted HEAD passed. Static selected-file quoted local includes and all CMake source paths resolve in the proposed snapshot. Excluded tracked grapple/test/view owners were asserted equal HEAD; excluded untracked gate/ParkourYard owners are absent. These are patch/static checks only. No source-build/runtime/device acceptance is claimed for the selected snapshot.

Existing a662/1b99 native archives and their accepted Godot bridges contain unrelated grapple WIP and larger gate snapshot layout. They do NOT match this selective proposal; rebuilding native closure plus BOTH bridge translation units is required. Reuse earlier behavioral observations as diagnostic history only until affected selected-source acceptance establishes matching artifacts. Current Jolt/dependency reproduction remains part of that fresh build.

Root-owned changing route capture hashes (refresh after final handoff repair):
- `tests/warehouse_forklift_route_tests.cpp`: `789726158aa7e4e64ba70d4280d9f1f2f47f803c46b6eeb6402d4b8a304dd8eb`.
- `tests/forklift_route_walk.hpp`: `e03246c58e9bc50720cadb893ec460baa88af943932134ec92bc048d1712ba58`.

No unresolved mixed-hunk dependency found: grapple additions were cleanly removable without altering the forklift/takeoff implementation. Uncertainty is selected-source integration/build/runtime behavior, final route-driver capture, and authoritative delivery docs. Root decides final inclusion and performs the necessary matching build after route freeze.

## Selected files

- `CMakeLists.txt` — shared file; selective transformation recorded
- `godot/presentation/forklift_view.gd`
- `godot/presentation/main.gd` — shared file; selective transformation recorded
- `godot/presentation/ui/touch_controls.gd`
- `godot/tests/forklift_controls_test.gd`
- `src/bridge/scraperx_simulation.cpp` — shared file; selective transformation recorded
- `src/bridge/scraperx_simulation.hpp` — shared file; selective transformation recorded
- `src/sim/physical_foot_push.cpp`
- `src/sim/physical_foot_push.hpp`
- `src/sim/simulation.cpp` — shared file; selective transformation recorded
- `src/sim/simulation.hpp` — shared file; selective transformation recorded
- `src/sim/simulation/checkpoint.cpp`
- `src/sim/simulation/constants.hpp`
- `src/sim/simulation/forklift.cpp`
- `src/sim/simulation/hand_transfer.cpp`
- `src/sim/simulation/parkour_intent.cpp`
- `src/sim/simulation/snapshot.cpp`
- `src/sim/simulation/tick.cpp`
- `src/sim/simulation/world.cpp`
- `src/sim/simulation/world.hpp`
- `src/sim/warehouse_forklift.cpp`
- `src/sim/warehouse_forklift.hpp`
- `src/sim/warehouse_forklift_yard.cpp`
- `src/sim/warehouse_forklift_yard.hpp`
- `tests/forklift_route_walk.hpp`
- `tests/parkour_intent_buffer_tests.cpp`
- `tests/parkour_intent_flow_tests.cpp`
- `tests/physical_foot_push_tests.cpp`
- `tests/warehouse_forklift_flow_tests.cpp`
- `tests/warehouse_forklift_route_tests.cpp`
- `tests/warehouse_forklift_tests.cpp`
