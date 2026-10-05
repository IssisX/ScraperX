**Ascent Slice:** `AS-027`
**Lifecycle:** IMPLEMENTED
**Provenance:** re-derived here
**Implementation gate:** integrated on local ChatGPT; ordinary-world campaign passed; final two lift checks after guide change and exact-source Android delivery pending
**Evidence:** [integration ledger](../../evidence/as027-integration-2026-10-05/README.md); exact-source Android receipt pending

# Two-stage service lift: +121 m to +143 m

## Objective and authority

One player-operated lift provides 20–30 m actual travel after AS-026, with physical boarding, deliberate landing alignment, departure and recovery. The larger 1000+m ambition remains a sequence of bounded slices, not a claim of completed ascent. Authority: Governing Laws 25–29/33/35/37; GDD 7.4, 9.2, 15–17; Atlas AS-026/027; Execution Protocol 3/3.1, 6, 11 and 17; ASCENT_PRE_RESOLUTION §8; TDD 1.3/4/8.5. The owner's explicit powered-machine direction governs over a generic compiler profile's motor exclusion.

Existing Tower entity 11 supplies the +121 m and +143 m west rings, X=[-26,-17], Z=[-167,-133]. Heights here are walking surfaces; ordinary standing capsule centres are approximately 121.9/143.9 m. AS-026 and other authored routes are preserved.

## Integrated assembly and ownership

`src/sim/service_lift.cpp/.hpp` owns the lift's joints and finite drive state; Kit owns its rigid bodies. Simulation owns player contact, supported footing, input authorization, checkpoint capture and explicit retry. The module runs in the existing serialized native physics owner, with no second production world or extra world update. Godot renders native Kit parts and requests effort through the existing pendant.

CHOSEN geometry: two 18 m scissor stages, 15°–65°; yaw +pi/2, origin (-31,108.732514,-153). DERIVED full stroke is 18(cos15−cos65)=9.780 m and vertical travel is 36(sin65−sin15)=23.3096 m. Deck surface spans approximately +121→144.3096 m, permitting alignment with +143 before the upper limit. Maximum carriage command is 0.18 m/s with 0.11 m/s² command ramp; this is not constant deck speed or a guaranteed ride duration.

Allocated IDs: frame 1970, guide 1971, lower/upper landing assembly 1972; carriage 2970, middle carrier 2971, intermediate carriage 2972, deck 2973, upper carriage 2974, paired arm assemblies 2975–2978. Construction checks capacity and collisions with existing IDs. The frame has visible ties into the +110 m west structure; guide ties and connected-pivot shafts carry the modeled reaction paths. The two new landing tongues at +121/+143 join existing Tower rings; their local dimensions and station posts are authoritative in source. The earlier broad clearance envelope was a screening estimate, not a final collision certificate.

Masses now derive from authored constituent volumes using chosen steel density 7850 kg/m³ and timber density 550 kg/m³; Kit derives compound mass properties. The earlier 600/800 kg screening assignments and fixed 200 kN rating are not production parameters. Effective lifted mass includes stage-height weighting and the actual 85 kg rider design load. Ratings are derived once from that load at 15°: force uses 20% headroom rounded to 50 kN; brake uses 40% headroom rounded to 50 kN. Electrical rating rounds the force/speed demand at 80% efficiency plus 250 W electronics to 10 kW. Reservoir capacity rounds the gravity-work/efficiency and nominal electronics demand with 30% headroom to 250 kJ. Obstruction never increases these ratings.

## Player sequence, finite source and recovery

Approach from the +121 ring, board the real deck and select OPERATE. Hold UP/DOWN to move, release to engage the finite passive brake, align with +143 and walk onto the real receiver. Lower and upper landing controls can call/reverse the lift while energy remains. Physically legal alternate transfers remain legal; no phase flag grants arrival.

Native station reach includes spatial reach, sight, traversal/hand eligibility and actual supported footing for drive authorization. Separate reachable-station state keeps the pendant visible during brief unweighting without granting drive force. Leaving reach, jumping or losing supported authority brakes the machine. Existing carry/hang priorities remain; reachable lift operation takes precedence over generic ledge CLIMB. Pause/focus clears reuse the existing input lifecycle.

The collision-substep owner limits commanded force against the electrical rating, debits positive actuator-work estimates at 80% efficiency plus electronics, and latches cutoff before a conservative next-step reserve is exhausted. Negative work is dissipative, not regenerative refill. Motor work uses impulse/displacement quadrature; the velocity allowance in the reserve is empirical. These are bounded numerical model assumptions, not an exact continuous energy proof.

Ordinary checkpoint restore retains recorded machine energy and body state and clears held effort. A separate **RESTART LIFT ATTEMPT** pause action restores the complete supported checkpoint saved before the first authorized lift command, including player, bodies and energy. It is unavailable before that snapshot exists. This named rollback is not a hidden recharge at the current checkpoint.

## Verification and current limits

Focused native route verification passed approach, native boarding, loaded-versus-unloaded demand, release/braking, ordinary checkpoint energy preservation, +143 Tower exit, full-stroke limit, upper-call reversal, reboarding, inherited departure momentum and explicit attempt retry. `tests/service_lift_route_tests.cpp` begins with supported +121 development staging; it is not continuous grade-ascent evidence.

Focused physics verification passed finite-source cutoff with 1 kJ and 250 kJ inventories, empty-source refusal, exhausted checkpoint hold and a real colliding obstruction. `tests/service_lift_physics_tests.cpp` uses the production 90 Hz update with four collision substeps. A historical coarse 90 Hz single-substep cutoff failure remains a numerical applicability limit, not a passing production result or grounds for loosening acceptance.

Fresh normal-world staged touch reached +143. The uninterrupted ordinary grade→143 campaign also passed with zero deaths, zero launcher work and the actual +121 checkpoint retained. Explicit lift-attempt retry was tested natively; touch retry is not claimed. Settings runtime passed 3078 checks and Godot import/parse passed.

The full native run passed 34/35, with only an obsolete default-world inventory assertion failing. Its inventory contract was corrected and the individual simulation test executable passed; this is not a rerun of all 35. The final two lift cases pass after the guide change below, including full upper/lower return with16.716mm return error,1.23658MJ remaining, zero tested overdraft and power within250kW. Exact-source Android verification is pending. Prior repair source `560618372ad9ea18a6e037b1d7d197557fb350ad` has green run `37340947930`; it contains no delivered AS-027 lift. One reviewed lift publication remains pending.

The ordinary campaign exposed a numerical contact-conditioning failure absent from the staged and aged native runs: at deck pose(-31,133.007996,-162), aligned yaw pi/2, the single 37 m guide collider1971 and deck shoe2973 reported a 268 mm penetrating contact despite the authored 20 mm clearance. The guide was divided into ten flush 3.7 m sections with unchanged external envelope, clearance, drive ratings and collision masks. The same uninterrupted campaign then passed. Red/green traces and final results are preserved in the [integration ledger](../../evidence/as027-integration-2026-10-05/README.md). This is an observed conditioning limit of that collider/contact configuration, not a universal engine-bug diagnosis or structural-certification claim.

Remaining evidence boundaries: real steel is modeled as rigid constituent geometry, not stress/buckling certification; preliminary paired-link and eccentric-load probes are not production structural validation. No Fold installation, runtime feel, readability, sustained performance or thermals have been measured. Preserve native force/work/contact ownership, original route gates and exact-source delivery requirements.

Completion requires the final normal-input supported +143 arrival/recovery, relevant retained gates and a successful exact-source Actions run with independently verified APK provenance. Integration and focused passes do not close delivery.
