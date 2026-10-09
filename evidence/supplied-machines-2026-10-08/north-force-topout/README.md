# North-frame physical top-out: native and viewport-touch closure

The first North-service-frame1901 ladder now reaches the real +94m roof through the existing finite two-hand-to-foot transfer, with gravity1 and a physical completion receipt. The retained1.7s response bound passes: **native1.68889s**, and **viewport-touch1.666667s conservative upper bound**. The touch proof then holds firm roof footing for270 native ticks (3s), walks2.119m onward and holds another90 ticks, with zero deaths. Native geometry and force/power limits remain authoritative.

The [maintained native owner log](native-route-braking.log) passes its complete +88→110m route at90Hz caller cadence in the existing `WorldContent::PipeBridge` test world. The first transfer has zero deaths, gravity1, real1901 support, released hand constraints and one loaded foot-transfer receipt. **The complete owner gate retains its later fatal/retry sequence: death_count1.** Its final PASS therefore does not describe a zero-death full route or conversion of every later mantle.

The [private touch log](touch-first.log) uses the normal shipping world and the existing viewport-touch/Main dispatcher. It stages only the initial supported88m stance, then uses ordinary touch approach, grip, climb and walking inputs. Actual settled approach is P(15.99991,88.9,−179.5606), V0. The target first appears at native tick692: (15.99991,94.92,−180.46). Previous sampletick691 bounds the start conservatively; physical foot receipt tick841 gives(841−691)/90=1.666667s. The final P(13.98527,94.89921,−181.1901) has V0/support1901/hands0/deaths0. Across1,016 viewport samples, gravity1, real two-hand climbing, finite work receipts and per-hand force limits pass; no legacy Mantling/Vaulting state or parachute is accepted. [Harness](touch-harness.gdpart), [touch receipt](touch-receipt.txt) and [exact final hashes](final-receipt.json) preserve the scope. This is first-ladder/roof/onward proof, not continuous ascent from grade or a physical phone observation.

The immutable [6a first-climb reproduction](route-6a/receipt.json) previously failed at P(16,91.760590,−179.828354), Climbing/support0/two hands/gravity1/deaths0. Its original distance-only `walk_to` accepted arrival at4.7588m/s; neutral braking stopped0.3245m beyond the intended(16,−179.55) approach. [Trace](route-6a/before.log) preserves the target, hand forces and motion. That first-climb jam predates this conversion; exact rung contact versus grip-progress arrest was not independently measured. The maintained native test now uses ordinary velocity-aware PD braking, retaining distance<.12m and requiring speed<.15m/s. It changes device-independent test input, not geometry, reach, player pose or gameplay authority. The successful touch stance and climb pass the former91.76m arrest point without that overshoot.

A separate [normal-world before probe](legacy/probe.cpp) uses one supported stage(16,88.9,−179.55), then ordinary native climb input. Its [legacy log](legacy/before.log) shows the defect: a gravity-off0.433333s transfer, released hands and no physical foot-transfer receipt. [Before source/library receipt](legacy/before-source-receipt.json) identifies the working source, already containing the air-work repair. Intermediate profiles remain saved as failed response-bound diagnostics:

| Saved result | Gravity off seen | Transfer time | Foot transfers |1.7s gate |
|---|---:|---:|---:|---|
| [Legacy](legacy/before.log) | yes |0.433333s |0 | gravity bypass |
| [Finite600](intermediate/finite-600.log) | no |1.81111s |1 | not met |
| [Finite600 lead adjustment](intermediate/finite-600-lead.log) | no |1.81111s |1 | not met |
| [Finite500](intermediate/finite-500.log) | no |1.71111s |1 | not met |
| [Final scoped480 native owner](native-route-braking.log) | no during first transfer |1.68889s |1 at first roof | met |
| [Final scoped480 touch](touch-first.log) | no |≤1.666667s |1 | met |

The staged legacy/intermediate diagnostics finish on real support1901 with zero deaths. Their full source/library hashes were not captured and are not inferred from final artifacts. [Manifest](manifest.json) retains their exact saved hashes and immutable extraction commands alongside final receipts.

Earlier touch startup attempts stopped before gameplay because the supplied private runtime stage had stale scripts, then mismatched current dependencies/assets/ARM64 extension mapping. [Saved startup logs](touch-stage-startup) classify those as environment/staging failures. Refreshing only the private copy from maintained Godot source, retaining the declared bridge and importing assets, resolved them; no gameplay check or input was weakened. Successful touch startup has no script/resource/native failures, and bridge/Main/private-driver hashes match before and after.

No new build or runtime check was run while closing this evidence folder. Large binaries and owner-source copies are omitted; the exact host test binary path/hash is recorded. Actions/APK delivery, device vibration/feel and later physical beam work remain separate evidence boundaries.
