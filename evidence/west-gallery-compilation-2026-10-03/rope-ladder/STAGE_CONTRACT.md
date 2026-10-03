# Long rope ladder — native feasibility counterexample

Profile: **MACRO-TRAVERSAL-STRICT** [DEFAULT]. Status: **NOT COMPILED FOR GAMEPLAY**.

## Outcome and scope

The requested experience is a long hanging ladder that sways under a slight breeze, deforms under a rider, carries actual hand load, and continues moving after release. One private topology was tested: lightweight rigid wooden rungs and two unilateral side-strand limits per gap. This is an engine feasibility probe, not a shipped encounter. Repository was `IssisX/ScraperX`, branch `ChatGPT`, head `2942fab1420aa2ffd600a425b590941e6a3e5916` at assignment. All prototype writes are outside the repository; concurrent native/UI work was untouched.

**Decision:** keep the lightweight-rung and real-hand-load direction; reject the stock hard DistanceConstraint strand binding at the tested 90 Hz cadence. The no-source equilibrium falsifier fails. A physically elastic tensile strand with a solved tare equilibrium is the next deciding model; its constants and acceptance are not supplied as completed design here.

Rejected alternatives: a single rigid suspended ladder does not provide local rope flexibility; decorative sway cannot transfer rider load; 400 kg rungs only bypass an unrelated controller filter and destroy the intended material response. Extra articulated rope bodies were not explored after the hard-strand falsifier rejected the simpler binding.

## Assembly and typed parameters

- [CHOSEN] 60 rungs, nominal pitch 0.4 m, 24 m free length; 1 m wide solid round rungs, radius 0.03 m, knot positions x=±0.44 m. A visible two-point overhead beam carries the strands. No lower attachment or guide exists.
- [CHOSEN] Rope shown diameter 0.05 m; effective rope density 700 kg/m³; wood density 650 kg/m³. These are authoring constitutive choices, not measured real material properties. The long assembly remains visible; the slender rungs replace the compiler's generic minimum-part-dimension default for this explicitly requested ladder.
- [DERIVED] Wood mass = 650π(0.03)²(1)=1.837831 kg/rung. Rope linear mass = 700π(0.05)²/4=1.374447 kg/m. Each pair of 0.4 m spans contributes 1.099557 kg, lumped at its lower rung. Moving node mass=2.937389 kg; 60-node mass=176.243353 kg. No fabricated 400 kg hold exists.
- [DERIVED] Rung-axis inertia Iparallel=½mwood r². Perpendicular inertia Iperp=mwood(3r²+w²)/12+mrope(0.44)². These exact chosen diagonal inertias are supplied to Jolt in the cylinder's local frame; the energy observer uses that same frame.
- [MEASURED] Production rider mass 85 kg (`simulation.cpp`, kPlayerMassKg); Jolt gravity 9.81 m/s². Prototype matches the shipping translation-only capsule: radius 0.35 m, straight half-height 0.55 m, friction 0, gravity factor 1. Its upright rotation lock is explicit, not an articulated body claim.
- [CHOSEN] Rungs friction 0.6, restitution 0; all bodies have zero generic linear/angular damping and sleeping disabled. CCD is enabled. Physical rung/player/world collisions remain enabled; no collision exclusion makes the catch work.
- [CHOSEN] Each hand uses free-axis SixDOF translation motors only: k=5000 N/m, c=180 Ns/m, per-hand vector force bound 1500 N through conservative per-axis bounds 1500/√3=866.025 N. This is a reduced muscular coupling, not a motor installed in the world. Rotation motors are off. Gravity and hand constraints provide the weight path; no extra player-weight force is applied.
- [CHOSEN] Air density 1.225 kg/m³, Cd=1.2; prescribed external velocity is u=(0.25s sin(0.23t+0.7),0,s[1.6+0.35sin(0.41t+0.271828)+0.2sin(0.83t+1.17)]) m/s, with s=0,±1. Forces, not poses, follow F=½ρCd A(q)|u−v|(u−v). Cylinder projected area follows current orientation; rope-area lumping assumes near-vertical spans. No random or body-presence animation is used.
- [CHOSEN] Private attachment beam at y=30 m, real floor top y=3 m. Those are isolated test coordinates, not approved tower placement. The service-mast +121→about132 m climb and a tail below entry remain [UNRESOLVED]; parent must test actual approach, sweep, receiver, sightlines and checkpoint recovery.

## Native binding, force and reaction paths

Pinned Jolt: `e77f175595e64cb44218cc9d9d56fc365ad0e36a`. Cylinders are real dynamic collider bodies. Each side gap has DistanceConstraint minDistance=0 and maxDistance bound once to the actual initially taut endpoint distance, rather than ideal decimal pitch. Thus T≥0 is permitted and compression is not: Jolt's maximum-distance multiplier is λ≤0. There is no angular pose controller or fixed bottom. A finite-length strand is the tensile mechanical limit; its observed numerical violation is a failure below.

Rider schedule is a declared test input: create a gravity-driven capsule at t=3 s with actual rung-point velocity plus 2.5 m/s downward relative velocity, attach both hands to rung 26, hold, remove constraints at t=9 s, and let it land on the physical floor. The inserted body's KE/PE is separately booked, so test insertion is not credited as machine work. No pose or velocity setter runs after insertion. The ordinary campaign controller is not exercised.

Force path: gravity → rider → finite hand constraints → lightweight rung → two tensile side strands → top knots → overhead frame. Static supports supply no work. Off-centre hands load the appropriate strand, including rung torque. After release the rung unloads while the capsule keeps its actual velocity.

[MEASURED, 90 Hz] Mean unloaded knot reaction ≈1728.95 N, matching 176.243353×9.81. Central loaded reaction 2562.685 N, approximately 833.74 N extra versus the 833.85 N rider weight. Central knots carry 1281.333/1281.352 N. At −0.25 m offset they carry 1518.178/1044.500 N; opposite placement gives 1044.530/1518.173 N. Removing the hand constraints changes capsule velocity by exactly 0 in all three runs; departure speeds are 0.0580–0.0585 m/s. Floor contact is actually detected after release. That floor is a test failure landing, not a safe authored receiver or gameplay death/recovery proof.

## Deciding checks and rejected numerical claim

Each of seven concrete cases ran 16 s with outer h=1/90 and h/4=1/360. Four separately observable Jolt Update(h/4,1) calls run per outer tick. This differs from the production world's single Update(h,4) call and is stated rather than assumed equivalent. Private solver settings are 64 velocity/8 position iterations, not the shipping default. Fourteen native processes exited 0 for finite state, constraint-force signs and force-cap checks; all seven assembly acceptance cases FAIL their chosen constraint-error/refinement gates.

Metrics below are **MEASURED exploratory results**, not INTEGRATED physical predictions. Columns are 90 / 360 Hz.

| Case | Maximum strand extension, mm | Peak rung speed, m/s | Maximum signed-ledger magnitude, J |
|---|---:|---:|---:|
| still | 2.659 / 0.157 | 0.627 / 0.122 | 15.459 / 0.128 |
| breeze | 2.640 / 0.166 | 0.501 / 0.236 | 14.633 / 1.883 |
| reverse | 2.640 / 0.166 | 0.501 / 0.236 | 14.595 / 2.001 |
| free_sway | 2.543 / 0.324 | 0.693 / 0.693 | 13.612 / 4.155 |
| central_catch | 2.640 / 0.166 | 0.536 / 0.531 | 29.169 / 3.087 |
| left_catch | 2.640 / 0.166 | 0.501 / 0.435 | 25.389 / 3.963 |
| right_catch | 2.640 / 0.166 | 0.501 / 0.439 | 25.418 / 3.287 |

Most decisive: `still` has no breeze, no rider, no contacts and initially taut vertical geometry/zero velocity. It should remain at rest. At 90 Hz it reaches 0.62651 m/s and 2.65884 mm extension; h/4 changes those to 0.121916 m/s and 0.157356 mm. Raw ledger magnitude is 15.459 versus 0.128366 J. After t=2.5 s, residual drift is still 10.2882 versus 0.000159 J. Therefore the limitation is not safely described as startup alone.

Earlier diagnostic: literal maxDistance=0.4 m produced 5.569 mm extension/1.24963 m/s at 90 Hz. Raising velocity iterations from 64 to 256 left that speed identical and did not fix extension. Source inspection shows stock DistanceConstraint deactivates inside its range without speculative outward-velocity tension. Decimal pitch plus float transform roundoff creates microscopic slack; binding measured endpoints improves startup but does not remove later activation/projection defects. No tolerance was widened to accept them.

Causal breeze sign survives: final tail z is +0.051172 / +0.051160 m for the breeze and −0.051176 / −0.051157 m when reversed. Free sway from an initial 0.025 rad tilt continues without a wind source. Maximum hand deviation in rider cases is 0.3323–0.3356 m; measured hand force is ≈866.07 N/hand, below the conservative vector bound. These partial signs/reactions do not close the failed stage.

Extended slack, actual hand-over-hand climbing, blocked grips, feet/top-out, swept tower clearance, and receiving/recovery traversal were not tested after the equilibrium falsifier rejected this binding. A dormant `climb` mode in the throwaway source is not acceptance evidence.

## Energy observer and honest boundary

The observer sums rigid-node/capsule KE and gravity PE. Eangular uses body-local ω and the supplied rung inertias. It records external wind reservoir work ΣF·u dt and aerodynamic loss ΣF·u dt−ΣF·Δx. Hand work is signed NET mechanical work from actual motor impulse dotted with averaged before/after Jacobian-relative velocity. The SixDOF Jacobian samples both bodies at p2, with r1+u=p2−COM1; the frozen constraint axes and sign match pinned Jolt. Rope impulse work is observed similarly and its negative booked as constraint loss. Floor contact loss after release is inferred independently from capsule momentum balance and averaged velocity, not from the energy residual.

Residual: R=E−Einitial−Einserted−Wwind+Daero−Whand+Drope+Dfloor. It omits position-projection work, within-step Jacobian evolution, microscopic muscle/spring storage/dissipation separation, rope-material bending/strain energy and lumped-rope spatial error. Consequently this residual is a diagnostic, **not a complete conservation proof**. Measured net motor peak is 2.55–2.60 kW; that does not prove a 3 kW aggregate muscular-source limit because opposed port work and compliant storage are not separately bounded.

Rope mass is right-endpoint lumping, preserving total weight/reaction but approximating distributed inertia/PE. For a straight vertical ladder the rope-PE quadrature offset is mrope,total g(pitch/2), about129.45 J; it is not a newly available energy source. Mid-span dynamics, large slack, rope/body contact and strand damage are outside this discretization's proven domain. This abstraction requires a spatial refinement check before fidelity claims about actual rope are made.

## Rendering and remaining owner seams

Godot consumes interpolated native cylinder poses and actual side-knot endpoints at one render alpha. Taut strand rendering must follow those endpoints. A slack curve must have the strand's available arc length and gravity/wind-oriented sag; a straight shortened line would visibly claim a rope that disappeared. Endpoint-only physics cannot claim resolved independent slack-midpoint motion. No Godot ladder mesh or screenshot was produced by this prototype.

Production integration requires a named rope-rung registry with stable native IDs and real cylinder-axis grip geometry. Existing discovery uses `leaf_box`/`box_is_hold` and rejects dynamic bodies below400 kg. Its climbing mode disables gravity, follows a principal support/root servo and applies an extra85g to that support. A dedicated lightweight-rung path must bypass those weight/servo rules together and retain physical hand/release reactions. Existing APIs and native/Godot responsibilities remain authoritative; parent owns that migration.

Next deciding check: physically tensile elastic strands with explicit material stiffness/damping, a solved tare-loaded initial state and full elastic/material/hand-port ledger, tested at the actual production cadence. Then actual two-rung transfers, approach/receiving surfaces, failure recovery, reset state, native/render pose agreement and device budget. No automatic powered reset is needed for a freely hanging ladder; checkpoint reconstruction is an explicit level-reset abstraction and must restore its stated initial physical state.

## Files and reproduction

`rope_ladder_probe.cpp`, `rope-ladder-probe`, `run_cases.py`, `native-cases.json`, `cases-driver.log`, fourteen `case-*.log` files, and retained pre-binding/256-iteration diagnostics are here. `native-cases.json` contains exact commands, hashes and failures. No commit, push, shared build or shipping runtime change was performed.
