# Second shaft: first contact

Isolated branch: `dot/twin-tower-first-contact`. Base: `28afbc4ab142e9e2b207dd5ce909cf9f5778b879` (`ScraperX-Claude`, fetched and rechecked 2026-10-21). No active branch was modified or merged.

The existing solid shaft moves +66 m in Z, keeping X=-30. Nominal face gap: 48 m (157.5 ft); including the nearest facade projections: 44.6 m (146.3 ft). Its mass and facade move together; A's upper continuation stays put. `world_solids.inc` was regenerated with the existing `--export-solids` pipeline, not edited by hand.

From the existing 220 ring, walk around its west band to (-7,-172.5), cross the 2.4 m trussed bridge to B at (-7,-218), walk the incline to (-18.4,-218), then mantle west onto the 225.65 m plinth. Return by stepping down onto the incline and walking back. The overhead member at (-20.5,228.5,-218) accepts the existing hook; from the receiving deck, aim roughly 0.7 m above it for projectile drop, reel, then release over the upper landing. Range and force are unchanged. No new spawn, controls, transport, save machinery, interior or door through the mass.

In plain English: the towers remain visibly separated. A narrow maintenance bridge lets the player visit the second tower, climb a short section outside it, and get back. Grappling is optional.

## Evidence and limits

- `SCRAPERX_ONLY=twin`: native inputs from existing `Ring220North`, actual rack acquisition, center and off-center bridge approach, incline, mantle, upper checkpoint save/reload, retreat, lower checkpoint reload, ballistic hook shot/reel/release and grounded upper landing. No test teleport or forced state; zero deaths on both paths.
- `SCRAPERX_ONLY=twin-geometry`: actual collision rays verify front=-224, back=-304, no ghost mass at the old position, zero rejected hulls.
- `python3 tests/twin_geometry_test.py 28afbc4`: all 44 exported shaft boxes moved exactly +66 m; all unrelated boxes and hull data unchanged; conservative rotated-box audit finds no unrelated exported box overlapping the relocated mass. No new hull AABB overlap candidates; four broad mountain bounds intersected both old and new mass bounds (existing terrain embedding). This is an exported-geometry audit, not exhaustive dynamic swept-volume proof.
- Godot 4.7 `--export-solids` repeated to a separate file: byte-identical. Editor binary SHA-256 matches the repository's CI pin.
- Godot 4.7 headless `--fixed-fps 60 -- --uitest=touch_twin`: PASS, existing touch movement and Action/swipe controls through the viewport; rack -> bridge -> incline -> mantle -> return. Like adjacent tests, the existing test helper steers the camera. Native GDExtension loads; no script errors.
- Focused existing native checks passed: bay, yard crane, counterweight well, C6, shared survival, parkour flow.
- Full native CTest: **not complete**. The first run found the old exact appended-body inventory; both affected inventories now require the new frame explicitly. The second broad run was deliberately stopped at 10m34s under the owner's bounded-effort instruction, with 52 PASS proof lines recorded (including grade-to-340 ascent and the corrected E guard); it is not a green aggregate. Shared-survival and parkour-flow CTests passed. A focused runner then called the actual `run_wet_e_guard(true)` and `run_yard_crane_magnet()` tests: both passed, including save/load. Historical body/stock/zipline hashes are unchanged. Remaining verification before integration: finish the full native suite.

Proof starts at the existing reachable 220 ring; this new test does not replay the entire ascent from grade. The hook shortcut has native proof, not a new touch shortcut test. Headless input/presentation integration is not a rendered visual review, Android execution, Fold performance or phone ergonomics proof. No APK was published and no signing/application ID/deployment configuration changed.

## Files and integration

- `src/sim/twin_tower.{hpp,cpp}`: shaft placement and the new static exterior geometry, appended as entity **1949** after existing kit bodies to preserve their indices. Existing kit presentation draws those same parts.
- `src/sim/physics_world.cpp`, `godot/presentation/main_world.gd`, `src/sim/world_solids.inc`: native/visual/exported relocation and construction seam.
- `tests/sim_tests_twin.cpp`, `tests/twin_geometry_test.py`, `tests/sim_tests.hpp`, `tests/simulation_tests.cpp`, `CMakeLists.txt`: checks and build registration. `sim_tests_wet.cpp` and `sim_tests_yard_crane.cpp` extend the exact appended-body inventories, keeping their old-order assertions.
- `godot/presentation/ui/ui_test_driver.gd`, `ui_test_rides.gd`: bounded touch scenario.

The Claude agent should fetch this branch and review/cherry-pick its implementation commit in its own integration workspace. Do not reset or force-push an active branch. If Claude has advanced, reconcile the constructor's late body order and reserve entity 1949; regenerate collision from the combined Godot builders and rerun native tests plus `touch_twin`. Do not copy only the generated collision table. This branch is delivered without merging.
