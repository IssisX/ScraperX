# One real downhill causal-support Jump: positive-work receipt undercount

Captured against the same privately frozen host archives/header as the vault baseline: exact `ChatGPT` / `3f295ca85bd5d9913cae94b689c50d20dc29e2c9`. Repository remained clean; no shared rebuild, root source edits, velocity assignment, traversal injection, position sweep or full suite.

The scenario is the normal world's actual lower taper inspection girder1932. Its walking face connects `(-24,363,-163)` and `(-24,352,-139.5)` (`src/sim/parkour_route.cpp:134`). One clear starting placement at `(-24,358.6,-151.25)` above that existing face uses the sanctioned `debug_restart_at`;180normal ticks establish actual capsule contact. Held `(0,1)` movement/facing for60ticks walks downhill on that face. Then release movement, request one ordinary supported Jump, and advance one native90Hz tick.

At tick240 the player is grounded on1932, no traversal/hands, gravity1, deaths0. Observed centre `(-24,357.494201660156,-149.23698425293)`, contact `(-24,356.632843017578,-149.397399902344)`, world velocity `(0.000003199465027,-2.02933311462402,4.32989978790283)`; actual support-point velocity is `(0,0,0)`. Thus relative vertical velocity `u=-2.02933311462402m/s`. Source-derived face normal is `(0,0.905690226558,0.423940106049)`; this is derived from inspected geometry, not a reported manifold normal because Snapshot does not expose that field. Body1932 is static and explicitly causal through `is_parkour_route_entity`.

Tick241 is real free flight: centreY357.554565429688, `vy=5.39100074768066m/s`, gravity1, hands0, traversalNone, support0, deaths0. Horizontal velocity is preserved. With actual native `float(9.81)*float(1/90)` gravity increment.109000004828m/s, delivered vertical impulse inferred from before/after velocity is639.993378706276N·s. The source's finite vertical push-off runs before `PhysicsSystem::Update`; its nominal requested impulse is approximately639.9933N·s. Static support makes `k=1/85kg⁻¹` exact for the reduced-order player response.

Independent calculations on that observed impulse:

| Quantity | Joules |
| --- | ---: |
| Signed command work `u*j + 0.5*k*j*j` |1110.60215396815|
| Absorbed work before velocity reversal `0.5*u*u/k` |175.023197829660|
| Positive work after reversal `0.5*k*max(0,j+u/k)^2` |1285.62535179781|
| Native `landing_jump_work_j` delta |1110.60168457031|
| Positive work minus reported receipt |175.023667227501|

The receipt matches signed net work within.000470J, while undercounting positive actuator work by175.0237J. Native float arithmetic explains the sub-millijoule difference; it cannot explain the175J braking credit. This isolates an actual positive-work ledger discrepancy in the causal push-off branch (`simulation.cpp:5780–5830`). It does **not** demonstrate meaningful burst overspend: the static-support positive work here is approximately the existing1285.625J burst budget, and the inferred0.000352J difference is a float residual. No complete collision/solver energy closure is claimed.

`causal-jump-before.log` retains all four rows and the independent arithmetic; `causal-jump-before.json` records geometry provenance and SHA-256 hashes. Actual command exited0, with only the prior proot `/proc/self/fd` binding warnings:

```sh
proot-distro login ubuntu -- /bin/sh -c 'cd /data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/physical-vault-baseline && /usr/bin/c++ -O3 -DNDEBUG -std=c++17 -pthread -Iinclude causal-jump-before.cpp libscraperx_sim.a libJolt.a -lpthread -o causal-jump-before && ./causal-jump-before > causal-jump-before.log'
```
