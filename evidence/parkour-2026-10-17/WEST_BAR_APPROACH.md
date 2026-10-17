# West bar approach after flowing top-out

The west-route test expected an airborne bar grip after jumping from wherever the preceding recovery left the player. Flowing top-out changes that receiving position and momentum. At the failing apex the body was x=-23.1292; the nearest bar face was x=-24.24, 1.1108 m away. The existing hand aim is0.40 m forward with a0.35 m facing-axis search half-depth. The expected grip was outside the actual query, rather than a failure to attach to a reachable bar.

## Causal evidence

- Current native1338fb4, original inputs: exact reproduced failure at tick1124, y384.334, ordinary gravity, no grip or hands. This matches full944e0e1 x86 CI.
- Substituting only the pre944 hand-transfer owner in the same current archive restores the complete original route. This isolates the old stopped-exit assumption; it is a diagnostic comparison, not the repair.
- Current native1338fb4, ordinary supported approach added: the complete original route passes, including deliberate missed Action, unsupported descent, shelf recovery, retry, actual bar anchors, reverse climb, final396.25 m footing and onward walking.

## Correction

Before each miss/catch attempt, the test walks with ordinary bounded input to x=-23.75,z=-137.60. The0.35 m-radius capsule has0.43 m to the near shelf edge and at least0.54 m to the Z edges there. Every approach tick must retain real route support at381.9 m, stay inside the shelf footprint and have no traversal/hands. Within180ticks it must reach the position with firm checkpoint footing and horizontal speed below0.12 m/s. Loss of support or expiry fails the test. No pose staging is added.

Observed approaches: (-23.3847,-137.673)→(-23.7337,-137.602), tick1003, firm1; recovery (-23.9044,-137.586)→(-23.7676,-137.598), tick1176, firm1.

All existing catch/miss/anchor/momentum/receiving/onward expectations remain. Production movement, query reach, geometry, forces and gravity are unchanged. A read-only mechanics contract review supported this correction; runtime evidence is the actual route execution above.

## Acceptance and limits

The exact local commands, outputs, source/archive hashes and paired owner comparison are in [WEST_BAR_APPROACH_COMMANDS.json](WEST_BAR_APPROACH_COMMANDS.json). Local Linux ARM64/proot complete west route exits0. The requested native-repair CI runs the original west route and `physical_topout_onward_run` against one x86_64 native archive. On published0b1247b, native-repair run39075654268 completed GREEN: both named cases passed, tests2/failures0. Exact CMake/CTest commands and JUnit identity are in WEST_BAR_APPROACH_CI.json. This correction does not establish phone FPS or full-release green.
