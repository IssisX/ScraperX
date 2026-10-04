# Native delivery failure checkpoint — 2026-10-04

Repository `IssisX/ScraperX`, branch `ChatGPT`. Inspected clean documentation head `a8a30f27f0fe271b507d3713b61faa4ff4f7282a`; executable candidate is `61361c6b5db417da8ec5003f4e671297cff4fe27`. This checkpoint changes documentation and evidence only. The owner requested a pause without expanding work. All worker assignments and local checks have finished.

## Actual verification

[Actions run 37162561352](https://github.com/IssisX/ScraperX/actions/runs/37162561352), job `111318847111`, completed with failure in “Build native host graph and run the full native suite.” Compilation succeeded; CTest passed 28/33. Two tests were not run because their executables were absent:

- `scraperx_sim.physical_hand_climb`
- `scraperx_sim.physical_traversal`

Three tests failed at “released lip does not recover on the real +103.5 m rest”:

- `scraperx_sim.north_service_frame`
- `scraperx_sim.north_service_frame_60hz`
- `scraperx_sim.north_service_frame_360hz`

[The CI excerpt](ci-failure-excerpt.log) retains actual failure lines. Later Godot/rendered/Android/export gates were skipped; no APK was delivered. The earlier five affected native checks and ordinary headless route retain their narrower proof scope.

A fresh local rebuild of `scraperx_north_service_frame_tests` against the current source, followed by `ctest --test-dir host-build --output-on-failure -R north_service_frame`, reproduces all three failures, exit 8, 6.47 s total CTest time. [The complete local output](north-reproduce.log) is retained. This is an actual native regression, not an Android or rendering observation.

## Supported diagnosis and unresolved boundary

The workflow's explicit build list omits `scraperx_physical_hand_climb_tests` and `scraperx_physical_traversal_tests`, although CMake registers their tests. That omission explains the two Not Run results; no workflow repair is applied yet.

The completed read-only diagnosis and lead source inspection identify a consequential seam in `src/sim/simulation.cpp`: Hanging acquisition disables gravity and chooses an ideal root hold; `drive_traversal` assigns `(desired-current)/dt` as player velocity. Passive release now preserves current velocity. The failing north test releases on the first Hanging snapshot. Therefore acquisition correction can survive release as artificial momentum. Actual catch/release velocity, impact speed and death count were not captured, so lethal vertical landing, horizontal miss and recovery timing remain unresolved. The approximately −54 m/s catch-speed estimate is an inference from probe/hold geometry, not measured evidence.

The existing finite hand module rejects kinematic supports. `tests/simulation_tests.cpp` exercises moving kinematic ledges. Any physical Hanging repair must resolve that eligibility contract, lowering-to-hang handoff, bounded shimmy, mantle release and actual receiving surfaces together. Do not restore copied zero/support velocity or weaken immediate-release recovery to hide the catch seam.

## Next work after the owner's continuation

1. Add the two missing native executables to the existing CI build targets.
2. Capture pre-catch, first Hanging, first release and one-second position/velocity/support/impact/death states through the unchanged failing path.
3. Repair the supported authoritative native cause; preserve real momentum, gravity/contact responsibilities and moving-ledge contracts.
4. Re-run the three original north cases and relevant hand/traversal/route regressions; publish repaired source and close exact-source Actions/rendered/APK delivery.

The wider rigid/rotating/deforming movement objective remains unfinished. Exact completed worker prototype sources and earlier scoped receipts are already preserved in the existing contact and west-gallery ledgers. No new content or broader movement implementation starts at this pause.
