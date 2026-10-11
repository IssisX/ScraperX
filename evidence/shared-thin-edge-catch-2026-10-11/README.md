# Shared thin-edge airborne catch — 2026-10-11

This bounded integration is complete. Focused native acceptance, affected
static/moving ledges, the existing simulation suite and an exact-bridge
headless touch encounter all pass.

The old airborne ledge query cast a wall ray at the player's COM. A reachable
thin shelf entirely above that ray was missed. The repair searches the existing
0.45–1.35m hand-height band continuously, then validates the actual side, top,
receiver, both lips, unobstructed hand approaches, capsule clearance and reach.
Only airborne hang discovery changes; mantle/vault retain their original query
and validation flags. Actual finite hands own acquisition and load.

## Rejecting baseline and focused result

Baseline HEAD is `4c84e5135bb42535e260850961c523963be32dd5`.
Apply `baseline-test.patch` to that test source to reconstruct the same rejecting
test (`22aac9ac3741e89afe732fc5ea80c5068cc78467428f4bc39df5f9d3b95b3f77`).
The baseline native archive is bound to the committed source by its receipt.
Compilation passed; the actual native run exited1 after missing shelf1800 and
falling to Y55.9 with no hands. A separate erroneous compiler-prefix launcher
also exited1 and is identified in the receipt; it is not the behavioral baseline.

Final focused source `src/sim/simulation.cpp` SHA256:
`63decfa120157d6686e483626c209138332e60cafcb76185d6168020e0d158fa`.
Simulation compiled with `-Wall -Wextra -Wpedantic -Werror`; native test exited0.
Only initial placement is staged. There are no subsequent position/velocity
writes, new geometry or test-specific gameplay branches.

| Actual test path | Result |
| --- | --- |
| Existing shelf1800, initial `(34.4,62.8,-137.0)`, facing/moving−Z | Real two-hand Hanging at tick1; airborne, gravity1 |
| Neutral30ticks after catch | Native radial ownership, mean upward hand load615.13N |
| Ordinary Drop | Hands/radial ownership clear; X/Z velocity retained, Y changes by−9.81/90m/s |
| Facing away, below reach band, too far forward | All three refuse target hands and physically fall without death |

`parkour_thin_edge` is registered as `scraperx_sim.shared_thin_edge_catch`.
Root independently checked71 native source/header hashes, cached dependencies,
reused bridge objects and every focused step's log/binary hash against the receipt.
The new native bridge SHA256 is
`2f04208b67e41b66aac1117a217f2595112e08fbe18d46e54ad942fbfbec2372`.

## Affected checks

The standalone `parkour_ledge_hang` mode runs only the unchanged existing
stationary7 and moving8 posture cases. Both pass: actual tuck0.213077/0.216376m,
command0.25m, stable held target, extension, ordinary Jump/mantle and Drop.
The existing simulation suite exits0. Their test source hash is
`b4c05f11c18e22ceefeb511148458b5ef446e74055ed75183c2a9b400796faa3`.
Root checked the individual logs and binaries against `regression-receipt.json`.

Godot4.7 headless60FPS using the new2f04208 bridge exits0 through actual catch,
paid tuck/extension, timed receiver, deliberate miss, recovery and retry, onward
Tower support11 and zero deaths. The actual posture change is0.162756m for
0.203534m command, with266.665162J positive target work. Presentation files match
the maintained source exactly; this is headless execution, not phone observation.

## Ownership and limits

Super_Agent MCP session `01a12c87-c6a8-79f3-a595-e125862f7fac` wrote only
Simulation.cpp. Root inspected the actual diff. An independent Sol/High review
found no source blocker and checked pinned Jolt contact coordinates, same-part
validation, reach/occlusion and the unchanged mantle/vault wrapper. Source review
is separate from the execution above.

The sweep is narrow across the facing centreline, rejects initial overlaps and
limits expensive validation to16 candidates. These are explicit implementation
limits. Rising-jump ledge catches, general mass/shape acquisition policy,
articulated limbs and phone feel remain open. This result does not establish
universal catchability, full system energy closure or fullGREEN/APK delivery.
