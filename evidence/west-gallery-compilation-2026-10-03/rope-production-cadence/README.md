# Quiet rope ladder at the shipping call cadence

**Result: quiet failure survives `Update(float(h), 4)`.** Exactly two runs were performed; both native processes exit0 with no Jolt update error. Those exits are execution evidence, not assembly acceptance.

Same 60rung/120strand topology, float-constructed taut geometry, mass/inertia and64velocity/8position settings as the private hard-rope probe. No rider, wind, aerodynamic drag, generic damping or additional force is applied. The original quiet probe included tiny passive aerodynamic drag; this follow-up excludes it explicitly for a pure mechanical equilibrium test. Sleep remains disabled, real collisions remain enabled, and there are no post-construction body pose or velocity setters.

A read-only native PhysicsStepListener observes states before all4collision steps, and the caller observes after each complete Update. Peaks therefore include collision-step boundaries, not only rendered/outer-tick states. GetTotalLambda is not used or converted into a whole-tick work ledger.

| Measurement |90Hz |360Hz |
|---|---:|---:|
| Outer ticks over16s |1440 |5760 |
| Actual step callbacks |5760 |23040 |
| Peak rung speed, m/s |0.580201507 |0.121916972 |
| Maximum strand extension, mm |2.594947815 |0.157356262 |
| Peak total kinetic energy, J |8.601074453 |0.352604528 |
| Minimum signed E(t)−E(0), J |-19.215513111 |-1.028666645 |
| Maximum signed E(t)−E(0), J |0.000000000 |0.000000000 |
| Final signed E(t)−E(0), J |-1.580268776 |-0.669434020 |
| All-body contacts added/persisted |0/0 |0/0 |

E is the direct sum of rung rigid KE and gravity PE, with rotational KE evaluated against the supplied body-local inertia. No hand/wind/contact contribution exists. No microscopic compliance energy is inferred for this ideal hard-strand model.

The observed failure is timestep-dependent quiet relaxation: the initially straight taut assembly moves, exceeds the pre-existing1mm strand-error gate at90Hz, and its peak speed changes by0.458285m/s at h/4. The measured signed mechanical energy **decreases**; these runs do not demonstrate energy injection. They do not establish a complete constraint-dissipation mechanism either. The hard inextensible binding remains rejected; a tensile elastic constitutive model with a solved initial tare load is the next deciding model.

**Contact-evidence correction:** the prior probe's contact observer counted only player pairs, so its quiet zero-contact fields did not establish all-body absence. This follow-up counts every added/persisted contact and actually observes zero. Original source/logs/receipts were left unchanged, and their recorded hashes still match.

Files: `quiet_shipping_cadence.cpp`, `quiet-shipping-cadence`, `quiet-90.log`, `quiet-360.log`, `receipt.json`. The receipt includes exact metrics, hashes and boundaries. Both binaries use pinned Jolt e77f175595e64cb44218cc9d9d56fc365ad0e36a; only private files here were written. No repository, shared build or shipping runtime changes were performed.
