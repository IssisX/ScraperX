# Shared material grip approach

Bounded integration verified. Universal material compatibility is not claimed.

Existing real gantry2952 fascia blocks a reachable net1953 grip at actor
(20,9.8,-118.55), facing+Z. After180 world ticks, one actor staging and one
ordinary tick, actual material point(19.9996,10.2491,-117.782) is0.889685m away.
The live fascia OBB intersects centre and both hand approaches. Ordinary Action
incorrectly attaches two real material hands. Baseline exits1 as required.

At(20,9.25,-118.55), the actual central grip and lower-hand approach are clear;
the upper-hand approach to that grip intersects the same fascia. Baseline Action
also incorrectly attaches two hands. This rejects a repair that gates only the
central candidate while leaving per-hand fallback unvalidated. The baseline
does not identify which candidate/fallback branch each hand took.

Baseline native HEAD9aa80db, Simulation3bf90d5f, archive
be75508e88bbc43a1bd7ba84ca09bfc8791167ef331c7c2e9f307da2fe306a44.
Source-frozen test, compile/run receipts and real geometry/readback logs are
retained in the two baseline directories. Clear-front/back actual catches are
established. The original near-contact front stage produced0.00218734m/s horizontal
release difference with zero public air force and geometric vertex/capsule
overlap, supporting incidental net contact (no direct public contact-impulse
proof). Moving only the clear-front initial stage fromz-117.45 to-117.35
keeps search/reach bounds and yields an actual native vertex/capsule gap0.04868m;
Drop horizontal difference is0, vertical gravity-step error1.75e-8. Original
0.002 tolerances, inputs and timing remain. Both corrected obstruction tests
still reject old source; clear front/back controls pass against the old archive.

The native gate shares the actual-COM approach ray, ignores only player
and sensors, keeps rigid exact body/leaf checks, and requires material first hit
on the actual net within existing0.02m landing skin of the chosen point. No wider
reach, hidden collision exclusions, world edits or force tuning. Material knot
centres can deviate from deformed triangles; this conservative endpoint policy
is verified on the clear controls and existing three-lane deforming route. Folds
within the terminal window and general deformed-net compatibility remain open.
Super_Agent advisory01a12dbe-7b25-7a12-876e-223c9c2f7f0f supplied this limitation.
No Android/phone/fullGREEN APK evidence from this slice.

## Source-bound results

Super_Agent MCP write01a12dc1-2c39-7583-b8b3-7bac3b61f204 changed only
Simulation.cpp, SHA256
`e97d3b25e5a10430ce9c704bc9c3b5fb8733f23049849a1af57632108093f635`.
The finite-hand backend, material adapter, headers, forces and timing are unchanged.
Independent source review found no blocker; the conservative surface/fold limits
above remain explicit. Final rejecting test
`1c8163640a3d3a3f81f9dbd028f602db4daa2b2bb99c1d88c3750fd1a8619a50`
passes all four cases on new native and rejects both obstructed grabs on old native.
A missing required clear-case cue is now a behavioral failure, not mislabeled
staging failure; no acceptance or tolerance changed.

Only Simulation.cpp was recompiled against unchanged verified objects. New native
archive `1b3129a5fec4999bd3e937d4662be9d3a2e9515f11b5fede7429895f74ecee74`;
exact bridge `262b1fec9438d047d08fa7d55dfa80090a21f2ef06831db180570e5fc5a91772`.
Existing default physical traversal passes in61.463s, including actual net
climb/Drop and ordinary jump departure. Its unchanged default test bodies were
run at testffda9b21 before the label-only edit; no native change followed.

The exact bridge with unchanged presentation99e45603 passes headless
`touch_cargo_net` in62.383s: ordinary grade entry, actual net climb, receiver and
first ring11.00m, deaths0. LogSHA256
`76663e5f8ed14b3e44cb79dbdadbc1886c08e7dcfcd5cc8c6555913f57f3cf98`.
The wrapper validates source hashes and byte-identical staged presentation before
loading the exact bridge. This is native/headless evidence, not Android/phone proof.

The unchanged native cargo-net test16b75426 passes all three lanes19/20/21
in102.49s: real grade entry, two material hands through180-tick suspended
loaded pauses, mean hand loads872.869/836.822/858.706N, material deflection
0.5405/0.6988/0.5843m, zero neutral commanded work, actual grounded receiver
at11.895m and Tower11, deaths0. LogSHA256
`6d744b5f650d50a44840243eb1acece7b22f25fe60cc643008e517509b87741c`.
CTest registers the new four-case `scraperx_sim.material_grip_approach` mode
alongside default physical traversal. The exact mode was run privately as
recorded; no new CI/Android run is claimed. Six local maintenance surfaces
remain uncommitted and are excluded from this feature save.
