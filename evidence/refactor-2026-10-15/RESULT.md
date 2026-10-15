# Large-file modularization

Behavior-preserving refactor of four mixed-responsibility monoliths:

| Source | Before | After |
| --- | ---: | ---: |
| `src/sim/simulation.cpp` | 10633 | 894 |
| `godot/presentation/main.gd` | 4920 | 2347 |
| `godot/presentation/ui/ui_test_driver.gd` | 5564 | 297 |
| `tests/simulation_tests.cpp` | 4365 | 22 |

Native feature implementations are separately compiled. The single private PhysicsWorld retains all state and method contracts. Godot builders and typed scenario suites receive explicit inputs; no generic forwarding, arbitrary include chunks, duplicate controller or copied gameplay state. See `02_ENGINEERING_AUTHORITY/SOURCE_MODULE_MAP.md`.

## Acceptance actually completed

- All public/native feature units compiled with the matched compiler/macros and `-Werror`; linked refactor archive `04e4c40080c53c49e0550fa3ed47c55c467f4aedbacc0f349490ba0db7959d22`.
- Fourteen existing native public-path cases PASS: topout/blocked/restart/frame delivery; natural entry overlap/release/expiry/radial; second hand; shimmy; primary/independent407 grapple; physical traversal; landing.
- All thirteen extracted native test units plus runner compiled with `-Werror`; the complete broad athletic runner PASS in182.55s, including unchanged same-life Hook5, screw→lift and A→B counterweight sequences.
- Matching rebuilt bridge plus actual Godot4.7 host import PASS. Routed keyboard-core and second-hand reach bridge PASS25.
- Ordinary AND regression fixture collision exports match their original generated tables byte for byte (1316boxes/666hulls and1149boxes/660hulls).
- Independent native review: all202methods,13helpers,14types,441constants, constructor initialization and state order preserved. UI source checks preserve142method bodies/66scenario names; broad native test extraction preserves748main assertion sites.

The integration checks caught an unqualified Node call after moving to RefCounted, then caught its wrong scene parent through the real collision exporter. Correct repair passes the original Main scene root explicitly to WorksiteMachineView; its native-owned water screw remains outside TowerPresentation exactly as before. No collision classifications, assertions, numeric limits or gameplay laws were weakened. Root also repaired relocation-only include/default-argument/nested-return qualification errors. Initial failed logs remain locally preserved; final results are source-bound here.

This does not claim full-suite GREEN or installed Android play. The pre-refactor f6ac5c4 run was71/92 with21failures; owner separately requested those actual defects be repaired in depth. The refactor's subsequent exact-source GitHub build/full-proof results must be recorded from execution.
