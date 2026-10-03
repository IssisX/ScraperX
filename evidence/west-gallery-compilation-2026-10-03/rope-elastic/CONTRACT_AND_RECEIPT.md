# Elastic rope ladder — private native feasibility candidate

Status: unloaded prestressed equilibrium verified; bounded central player catch/hold/release verified. Dynamic accuracy, settled loaded extension and gameplay acceptance remain unresolved. This is not a shipping obstacle or normal gameplay proof.

Repository inspected: IssisX/ScraperX, branch `ChatGPT`, head `0ecf34196c59d9e521f11e529e3da65644ec29a9`. Writes are restricted to this private directory. The earlier rejected hard model and exact shipping cadence counterexample remain unchanged: all 30 hard-model and all 4 cadence receipt hashes matched after these experiments.

## Assembly and causal binding

The existing 24 m topology is retained: 60 cylindrical wooden rungs, 1 m wide, radius 0.03 m, pitch 0.4 m. Two strands attach at x = ±0.44 m to actual local rung knots; the fixed visible-proxy support is a 1.4 × 0.3 × 0.6 m box with its knot elevation at 30 m. Each strand is a real unilateral native Jolt DistanceConstraint: min 0, max its independently solved rest length; `ESpringMode::StiffnessAndDamping`, stiffness and damping explicit. No compressive strand force, presence-driven motion or added player-weight force is used.

Wood density is CHOSEN 650 kg/m³; effective rope density CHOSEN 700 kg/m³ and diameter CHOSEN 0.05 m. Rope span-pair mass is lumped onto each lower rung, giving nominal rung-plus-rope-node mass 2.93738921285 kg and total 176.2433528 kg. Actual Jolt inverse mass gives 2.93738926061 kg per node. The rope node mass has transverse inertia at the two ±0.44 m knots; rod inertia uses the actual cylinder shape frame. This is a low-order rope mass/inertia approximation, not resolved internal strand bending or distributed wave physics. Rung friction is 0.6, restitution 0, gravity factor 1, CCD enabled, sleeping and generic damping disabled. Attachment strength and material failure capacity are UNRESOLVED; force peaks are observations, not strength certification.

Uniform side-strand stiffness is CHOSEN from the static extension screening target, not a measured material: derived k = 250822.807895133 N/m; actual float k = 250822.8125 N/m. Actual gravity is 9.81000041962 m/s². For strand i, unloaded tension is DERIVED as `(60−i)*actual_mass*actual_g/2`. Constructed initial endpoint distance `li` is measured once; rest length `l0i = float(li−Ti/k)`. Therefore gravity and the actual stored initial spring strain balance before simulation. No warm-start impulse, external force or position correction is injected. Actual left rest-length sum is 23.8948802054 m; largest initial float preload force error is 0.00373166 N. The initial stored spring energy is 61.08704645 J.

Screening calculation predicts own-weight extension 0.1051198111 m plus 0.0448801889 m for an 85 kg rider at index 26, total 0.15 m. It is a **static equilibrium prediction**, not a bound on catch excursion. The recorded loaded window has not established settled equilibrium.

## Pinned solver and timestep

Pinned Jolt is `e77f175595e64cb44218cc9d9d56fc365ad0e36a`. Source inspection confirms DistanceConstraint's stretched branch passes `length−maxDistance` into `AxisConstraintPart::CalculateConstraintPropertiesWithSettingsForLimit`. `SpringPart::CalculateSpringPropertiesWithSettings` maps `StiffnessAndDamping` to true k and c, not mass-normalized parameters. The implicit softness is `1/[dt*(c+dt*k)]`; spring-active distance constraints skip position projection. Material strand damping c = 0; implicit integration still dissipates unresolved dynamic modes.

Every run uses **one `sys.Update(float(h),4)` per outer tick**, 16 seconds, 64 velocity and 8 position iterations, penetration slop 0.002 m, one worker thread. These global prototype settings differ from current production: source search found no global settings override in `src`; pinned Jolt defaults are 10 velocity / 2 position iterations and penetration slop 0.02 m. Existing mechanism/slingshot constraints have local overrides (commonly 40 / 8). The shipping call cadence has been reproduced; the shipping global solver budget has not. Before production adoption, reproduce this assembly under production globals with explicitly justified mechanism-local overrides, rather than raising the whole world budget. A read-only native PhysicsStepListener observes internal collision-step boundaries, with an additional observation after the complete Update. Actual internal dt is 0.00277777784504 s at 90 Hz and 0.00069444446126 s at 360 Hz. Constraint lambda getters are the last collision-step impulses; division by that actual dt produces explicitly labelled last-step force observations. They are not aggregate outer-tick work or impulse.

Highest longitudinal mode upper bound is DERIVED ω ≈ 826.509 rad/s, giving internal-step ωdt ≈ 2.296 / 0.574. Quiet equilibrium does not verify those frequencies. Two timesteps do not establish convergence.

## Actual verification

Four simulation processes were run: quiet and one central 85 kg catch at 90 and 360 Hz. All exited 0 with no Update errors. Frozen source/executable/log hashes and parsed results are in `receipt.json`; `verify_receipt.py` checks the bounded proof and explicitly prints the missing gameplay and dynamic gates.

| Observed quantity | 90 Hz | 360 Hz |
|---|---:|---:|
| Quiet peak node speed, m/s | 0.0000063371 | 0.0000223815 |
| Quiet maximum position/span change, m | 0 / 0 | 0 / 0 |
| Quiet max absolute mechanical E−E0, J | 8.2946e−10 | 1.1474e−8 |
| Quiet all-body contact events | 0 | 0 |
| Quiet maximum root strand force, N | 864.47414 | 864.47423 |
| Catch peak hand force, N per hand | 866.02536 | 866.02536 |
| Catch maximum hand point separation, m | 0.231218 | 0.232515 |
| Maximum strand force, N | 2423.046 | 2427.631 |
| Maximum node COM speed, m/s | 1.824598 | 1.875531 |
| Maximum left total physical strand extension, m | 0.289780 | 0.291314 |
| Loaded window mean left extension, m | 0.151778 | 0.152249 |
| Loaded window mean root vertical reaction, N | 2590.471 | 2597.814 |
| Unloaded window mean root vertical reaction, N | 1726.412 | 1731.873 |
| Neutral release instantaneous velocity change, m/s | 0 | 0 |
| Capsule departure speed, m/s | 0.056372 | 0.079908 |
| Rung point departure speed, m/s | 0.040807 | 0.057705 |

Quiet mechanical energy includes actual inverse-mass and principal inverse-inertia kinetic energy, actual gravity potential and `Σ 0.5*k*max(l−l0,0)²`. Quiet positive energy residual is only the tiny values above; there is no wind, rider, force API call or contact. Quiet root physical prestrain is 3.44655 mm. It is not a hard-constraint geometric error.

Rider fixture: at t = 3 s, an actual dynamic 85 kg capsule (actual stored mass 84.9999983376 kg; capsule straight half-height 0.55 m, radius 0.35 m, total half-height 0.90 m) is inserted at rung 26 with actual rung-point velocity plus −2.5 m/s downward. This is an explicit test initial condition, not a production capture path. Gravity remains on. Shipping capsule rotation lock is retained. Two real SixDOF hand couplings hold x = ±0.15 m on that rung, with free constraint axes and rotational motors off; translation motors use k = 5000 N/m, c = 180 Ns/m, each axis limited to 1500/√3 N (per-hand vector bound 1500 N). The observed central catch excites principally one axis, so 866 N is an observed peak, not a different chosen vector cap. No subsequent pose, velocity or target trajectory writes occur. Remove both hand constraints at t = 9 s without copying support velocity. Loaded observations use t ∈ [7,8.5), unloaded [12,15).

Expected actual static root weight with rider is 2562.79741 N (1728.94739 N ladder + 833.85002 N rider). The loaded-window means are 1.08% / 1.37% higher, and total extension exceeds the 0.15 m static screen by 1.78 / 2.25 mm. The window remains transient; this does not establish a different settled material law. The peak total extension is almost twice the static screening target. Maximum individual span change from constructed geometry is 6.214 / 10.903 mm, a 75.5% refinement difference; maximum **physical** strain is 9.661 / 9.678 mm. These differing metrics are kept separately. Departure speeds differ by 41.8%, with small absolute values, so dynamic fidelity cannot be signed off from matching peak forces alone.

The capsule falls onto the actual test floor after release. Recorded all-body contacts are exactly capsule–floor contacts: 1 Added + 1898 / 7593 Persisted; no other capsule pair events. Maximum capsule speeds include that fall (16.866 / 16.856 m/s). The roughly 15 m fall is **not a safe production recovery surface**.

## Energy and delivery boundaries

Rider mechanical energy includes the capsule and the strand energy. Capsule insertion energy is booked separately: 15733.544 / 15733.545 J. The printed `final_unclosed_mechanical_delta_j` is −12480.987 / −12467.713 J, dominated by the eventual inelastic floor impact and hand/controller/integrator losses. It is **not a conservation residual**. No hand motor work, hand compliance storage, total positive muscle/controller input work, contact impulse work or implicit numerical loss has been separated in this candidate. Motor damping is explicitly 180 Ns/m and strand material damping is explicitly zero. The mechanical energy observer omits hand spring/motor storage and cannot close these ports or certify a muscle power cap. Before floor impact, the mechanical delta is already roughly −362 / −363 J at release. No full power-port closure is claimed. Legacy field `max_kinetic_j` measures nodes only; energy() includes the capsule. Legacy `min_initial_span` / `max_initial_span` fields are the minimum/maximum **rest lengths** in this elastic candidate.

Remaining production seams: dedicated named cylindrical-rung grip queries and stable native rung identity; bypass the legacy climb gravity-off, principal-body root servo and scripted weight together; physically reachable two-hand acquisition/transfer and blocked-motion behavior; current capsule rotation lock versus embodied presentation; consistent rotating-point support velocity/departure; actual flexible/slack strand rendering from solved knot poses and justified curve; breeze force/work and slack cases on THIS elastic model; full load/dissipation ledger; actual material strength/strain policy; authored attachment, swept collision clearance, entry/receiving and safe fall recovery. Proposed west-gallery placement is parent's private screen, not proven by this isolated floor fixture. No Godot import, rendered normal-route run, APK, or Fold 6 verification was performed here.

Next single production blocker: reproduce the same assembly under production global solver settings with a justified local constraint budget. The present proof uses global 64 / 8, not production 10 / 2; it cannot yet authorize route integration. After that settings boundary, define loaded-settling and dynamic fidelity tolerances; measure matched quiet-to-loaded equilibrium separately from catch transients and inspect implicit attenuation within the allowed timestep budget. Do not force a pass by arbitrary rope damping. Production acquisition and route integration require their own real gameplay proof after this numerical boundary is resolved.
