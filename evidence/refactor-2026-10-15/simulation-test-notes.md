# Native simulation test modularization — 2026-10-15

Baseline: `ChatGPT` at `dc53244d99a833888f1004af1b74a392887b34d5`.
Ownership: `tests/simulation_tests.cpp`, new files under `tests/simulation/`,
and this note. Existing production, CMake, UI and concurrent evidence edits
were preserved. Root owns the build integration and executable acceptance.

## Result and interfaces

`simulation_tests.cpp` shrank from 4,365 to 22 lines. It still provides `main`
and returns `EXIT_SUCCESS` only after every original domain finishes. The
existing `scraperx_sim_tests` executable and `scraperx_sim.athletic_traversal`
CTest registration remain root's CMake integration contract.

The runner calls twelve compiled domain functions, in original execution
order. All shared test symbols live in `scraperx::tests`. Domain-specific
helpers stay in each suite's unnamed namespace; there are no implementation
`.inc` files or header inclusions hiding an uncompiled test monolith.

Shared `test_utilities.hpp/.cpp` owns the original assertion/failure behavior,
comparison/vector helpers, fixed-clock predicate stepping, steering and
walking. `advance_until` remains a header template. Default argument values
stay on declarations. `intake_geometry.hpp` holds ten unchanged test geometry
constants shared by the intake, Legal Forty and loaded Hook5 route.

`test_suites.hpp` declares suite functions plus immutable scalar diagnostics:
`SupportReport` (7 fields) and `TraversalReport` (13 fields). They carry only
the earlier tests' values printed by two historically delayed PASS lines.
`run_plant_recovery_tests(const SupportReport &, const TraversalReport &)`
prints those lines at their original position between initial plant checks
and fall/recovery tests. No `Simulation`, full `Snapshot`, shared mutable
context, or gameplay authority crosses a suite boundary.

Independent worlds now end their lifetime at their domain function's end.
Continuation stages retain the same local world and exact statement order:
Hook5 carry/skin/loaded freight/checkpoint, water-lift onward transfer and
uninterrupted screw-to-lift ascent, and AS-006 Stage A ride/death/restore →
Stage B remain together. No assertion, input, timeout, world-content choice,
helper body, test expectation or PASS/FAIL string was weakened or removed.

## Explicit CMake source list and map

Add these to the existing `scraperx_sim_tests` target beside the existing runner:

| Source under `tests/simulation/` | Lines | Domain |
| --- | ---: | --- |
| `athletic_traversal_tests.cpp` | 410 | Vault, double-tap, oblique/standing mantle, stationary/moving hangs and release |
| `counterweight_well_tests.cpp` | 464 | AS-006 A and B rigging, no-link falsifiers, ride, energy and recovery |
| `foundation_support_tests.cpp` | 290 | Shipping foundation, default recovery, clock partitioning, translating/rotating supports |
| `freight_jib_tests.cpp` | 171 | Jib controls, moving crate, finite capacity, saved hold restoration |
| `ground_water_tests.cpp` | 459 | Conserved screw, lift falsifiers/ride/reset/transfer, uninterrupted screw ascent |
| `hook5_rack_tests.cpp` | 976 | Cage access, bar/door, real carry, restored hands, loaded ascent and checkpoint |
| `intake_rise_tests.cpp` | 170 | Pinned intake, freight opening and skin bypass |
| `legal_forty_tests.cpp` | 545 | Deploy/retract causal stair, loaded support, upper path and skin walkway |
| `plant_recovery_tests.cpp` | 553 | Coupled process plant, fall/parachute/checkpoint, enabled-access chain and treadle |
| `process_sump_tests.cpp` | 118 | Sump isolation, grate access and reversible process state |
| `structural_needle_tests.cpp` | 132 | Needle seating, crossing, unseating and checkpoint |
| `test_utilities.cpp` | 116 | Shared typed test operations |
| `world_crouch_tests.cpp` | 205 | Physical solids/basin, absent fallback, crouch clearance and movement |

Headers: `test_suites.hpp` (50 lines), `test_utilities.hpp` (51),
`intake_geometry.hpp` (18). Largest compiled suite: Hook5, 976 lines, including
its private causal traversal/carry helpers and uninterrupted loaded route.

## Checks actually run

- Exact ordered C++/string token comparison for **12/12 domain bodies: PASS**.
  Normalization removes only ten relocated geometry declarations, two added
  report returns, original local `using` declarations, and reconstructs the
  two diagnostic print expressions from the exact scalar report field map.
  Comments/whitespace do not participate; all assertions, loops, device/input
  values and other execution statements retain exact tokens and ordering.
- **748/748 original main `require` call sites retained** within those bodies.
- **9/9 shared helper bodies: PASS**, plus exact declaration/default arguments.
- **18/18 private helper definitions: PASS**, including original signatures.
- **10/10 shared geometry definitions: PASS**, exact original literal values.
- Runner function sequence, report field values/order, delayed print placement:
  **PASS**.
- Extracted helper dependency scan: **PASS**, no function calls another suite's
  private helper.
- `git diff --check -- tests/simulation_tests.cpp tests/simulation`: **PASS**.

Comparison script used on this host:
`python /data/data/com.termux/files/usr/tmp/check_sim_test_extraction.py`.
It compares the captured original source; baseline is recoverable with
`git show dc53244:tests/simulation_tests.cpp`.

No build, compiler invocation, test executable, CTest, Godot import/runtime,
APK or device check was run by this worker. Root must compile the thirteen
new `.cpp` files and run the existing affected test target. Static source
comparison is not executable acceptance.
