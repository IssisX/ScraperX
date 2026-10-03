# Elastic ladder — production solver binding check

Private feasibility evidence, not production implementation. Worker writes were limited to this directory. Repo recovered at IssisX/ScraperX branch `ChatGPT`, head `0ecf34196c59d9e521f11e529e3da65644ec29a9`, with concurrent parent audio/documentation changes preserved. Frozen elastic source/results remain unchanged.

## Exact delta from the frozen assembly

Copied `rope-elastic/elastic_ladder_quiet.cpp` and `elastic_ladder_rider.cpp`. Only the global solver settings, local constraint overrides, identifying comment and output fields changed. Geometry, masses, actual-inertia energy observer, solved initial prestress, spring stiffness, zero strand material damping, hand coupling and player schedule remain identical.

Pinned Jolt source `e77f175595e64cb44218cc9d9d56fc365ad0e36a` confirms:

- `PhysicsSettings.h`: defaults 10 velocity / 2 position iterations, penetration slop 0.02 m.
- `Constraint.h`: inherited `mNumVelocityStepsOverride` and `mNumPositionStepsOverride` are real native constraint settings; 0 requests the global default.
- `CalculateSolverSteps.h`: active constraints/body overrides and defaults are combined by **maximum within the connected island**. Thus the 40/8 override has island cost; it does not run exclusively on one strand.
- Existing `src/sim/mechanism_kit.cpp` `add_tie` uses local 40/8. Both candidate strand DistanceConstraints and hand SixDOF constraints now explicitly use 40/8. Actual getter output confirms 40/8 on created strands.

Global settings are now 10/2, actual float slop 0.019999999553 m; no global 64/8 budget remains. Each outer tick is exactly one `Update(float(h),4)` at 90 Hz, actual internal dt 0.00277777784504 s. Both runs lasted 16 seconds: 1440 ticks, 5760 internal observer callbacks. No other rates, sweeps, parameter tuning or production edits occurred.

## Actual results

Both native processes exited 0, with Update errors 0 and all measurements finite. Compilation via `sh build.sh` exited 0. The build script uses pinned Jolt static library and the existing proot Ubuntu C++ compiler; full command is in the script. Run commands are retained in `receipt.json`. `python3 verify_receipt.py` checks the recorded bounded acceptance, hashes and same-case comparison.

| Measured quantity | Quiet | 85 kg central catch/hold/release |
|---|---:|---:|
| Peak rung COM speed | 6.3371e−6 m/s | 1.824598 m/s |
| Maximum position/span change | 0 / 0 m | 0.184705 / 0.006214 m |
| Peak strand tension, last internal step | 864.47414 N | 2423.04611 N |
| Maximum compression lambda | 0 | 0 |
| Maximum hand force per hand | n/a | 866.02536 N |
| Maximum hand point separation | n/a | 0.231218 m |
| Actual capsule gravity factor | n/a | 1 |
| Instantaneous neutral-release velocity jump | n/a | 0 m/s |
| Actual capsule departure speed | n/a | 0.056321 m/s |
| Actual rung point departure speed | n/a | 0.040468 m/s |
| All-body contact events | 0 | 1 Added + 1898 Persisted |
| Quiet max absolute mechanical E−E0 | 8.2946e−10 J | n/a |

Quiet results match the frozen global-64/8 90 Hz results exactly. Initial spring energy remains 61.08704645 J, physical root prestrain 3.44655 mm, largest initial float preload force error 0.00373166 N. The balance is gravity against real stored strand strain, not an added preload force or scripted pose.

Rider fixture remains an actual 84.9999983376 kg dynamic capsule: straight half-height 0.55 m, radius 0.35 m, total half-height 0.90 m; shipping rotation lock retained. At t=3 s, insert the capsule at rung index 26 with actual rung-point velocity plus 2.5 m/s downward. Gravity remains on. Both finite compliant native hand constraints use 5000 N/m, 180 Ns/m, each axis force bounded by 1500/√3 N. Remove both constraints at t=9 s without changing any velocity. No other pose/velocity writes, wind or extra player-weight force are applied. Whole-system strand and hand loads arise from gravity and the actual initial catch state. Recorded contact events are exactly capsule–floor, with no other pair events; maximum capsule speed 16.866 m/s includes the subsequent floor impact. This is not a safe authored recovery route.

Mean loaded window [7,8.5 s): root vertical reaction 2590.47642 N, left total physical strand extension 0.15177887921 m. Mean unloaded window [12,15 s): root reaction 1726.41017 N. Expected actual static weights: ladder 1728.94739 N; ladder+rider 2562.79741 N. The transient loaded mean is 1.08% above the static reaction and exceeds the **static** 0.15 m extension screening target by 1.779 mm. The static target is not a catch-excursion cap; its settled verification remains unresolved. Catch maximum total left extension is 0.289779752493 m, unchanged from the previous 90 Hz run. No passing static-extension claim is made.

## Energy and verification boundaries

Quiet energy contains actual mass/inertia kinetic energy, actual gravity PE and all unilateral strand spring energy; no contact, wind or hand exists. That is the closed quiet energy discriminator.

For the rider, insertion energy 15733.5444162 J is booked explicitly, but hand compliance storage, motor/net rigid-body work, total positive muscle/controller work, motor dissipation, implicit integration loss and contact work are **not separated**. `final_unclosed_mechanical_delta_j` = −12493.6799773 J is not a conservation residual or a source-power proof. Previous global-64/8 value was −12480.9873747 J; final energy differs by 12.69 J after actual floor contacts and a different slop/global contact budget. No causal partition of that difference is established. Strand material damping remains zero; implicit spring integration still attenuates unresolved modes.

The original frequency bound ω≈826.509 rad/s and ωdt≈2.296 at this 90 Hz/4 cadence are unchanged. This check closes the **production-global/local-constraint solver-settings compatibility blocker for these two bounded fixtures**. It does not close high-frequency dynamic fidelity, settled load, material strength, slack/breeze behavior on the elastic model, full hand energy ports, runtime cost in the full tower, native grip discovery, two-hand reach/transfer, presentation, swept placement, normal-route entry/receiver/recovery, APK or device verification.

Next production owner action: implement the explicitly identified lightweight cylindrical-rung acquisition path with native stable identities and the actual gravity-on hand constraints, bypassing the legacy gravity-off/root-servo/scripted-weight mode together. Preserve the numerical limits above as adoption gates; a private pass is not evidence that this integration exists.
