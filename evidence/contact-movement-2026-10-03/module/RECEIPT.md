# Native rigid-hand coupling receipt

Implemented only `src/sim/physical_hand_climb.hpp/.cpp` and `tests/physical_hand_climb_tests.cpp` in IssisX/ScraperX, branch `ChatGPT`, inspected HEAD `543d13e04c615af6869d46826214878446513200`. Parent Simulation/CMake/docs/tests edits were present and preserved. Frozen source copies and SHA-256 receipt are beside this file. No commit/push, shared build, production runtime, or prior prototype files were changed.

## Assembly and ownership

Two independently acquired native SixDOF constraints connect body1=actual player to body2=actual static/dynamic rigid support. Both anchors start at the caller-supplied actual world grip with zero target/error. All six hard axes are free; only translation POSITION motors operate, with zero target velocity, stiffness 5000 N/m, damping 180 N·s/m, per-axis cap 866.025390625 N (each hand vector <=1500 N). Local solver40/8, global10/2 retained. The player's production translation-only DOFs keep world motor axes fixed. Soft/self/invalid/sensor/out-of-world holds and a rotatable/non-dynamic player are rejected. Invalid replacement preserves the old hand.

Attach/detach/clear never modify gravity, mass, body pose/velocity or collision permissions, and never apply an extra player-weight force. Real native constraint impulses own both bodies' reactions. Wake calls only activate bodies. Clear must precede either body's destruction and the PhysicsSystem's destruction.

The target sign is negative requested player displacement because Jolt solves separation p2-p1. Commands charge the actual representable target changes at Fmax*distance, summed over attached hands. One shared CHOSEN3000 W*dt budget covers both. Cumulative debit survives detach/clear; it is a conservative target-port bound, **not measured motor work, positive muscle work or energy closure**. Spring storage and controller input are not closed by this observer.

## Actual checks

Build command is recorded exactly in `build.sh` and `receipt.json`; it uses the cached Jolt revision `e77f175595e64cb44218cc9d9d56fc365ad0e36a` and read-only static library. `red-tests` compiled with the inert coupling stub, then ran exit1/0of5: every case rejected missing acquisition. `red-module.cpp` and `red-run.log` preserve that actual failure.

The first real implementation ran4of5: the one-hand static assertion sampled at4s while force was still saturated. Diagnosis measured sag0.278939 m, vy -0.0288089 m/s and866.025 N upward. The specified per-axis cap leaves only about32.18 N above85kg weight. The equilibrium check now waits another12s with the same actuator and unchanged tolerances; the failing first run and diagnosis are preserved. No physics constants were tuned to force a pass.

Final actual run `green-tests` / `green-run.log`: **exit0,5of5**. Shipping-shaped85kg capsule half-cylinder0.55 m/radius0.35 m, gravity1, friction0, rotation locked; 90Hz native `Update(h,4)`:

- Static two-hand equilibrium: sag0.0833883 m vs mg/(2k), upward reaction833.846 N. Mass/gravity retained. After one-hand detach, equilibrium sag0.166768 m vs mg/k and actual weight reaction pass. Release leaves instantaneous velocity exactly unchanged; subsequent velocity follows gravity. No constraints remain after clear.
- Positive0.20 m target command produces +0.199789 m player ascent. Huge commands obey the aggregate3000 W bound with either two or one hands; NaN/unattached commands debit zero; cumulative debit is retained. Final bound666.667 J is rounded printed data, not a measured energy total.
- Real free25kg support receives recoil: support vx1.44746 m/s/player vx1.57428 m/s; horizontal total170 kg·m/s conserved within0.05 tolerance. Acquisition/release leave both velocities unchanged.
- Real oblique incoming velocity(-6,-6,-6) m/s is unchanged at capture. Native compliant braking settles without snapping; minimum playerY2.60494 m from4.3 m start. All-axis force reaches1500 N vector cap and remains bounded. The earlier purely vertical catch result is retained separately; it exercised only866 N.
- Actual soft-body hold rejection, invalid index/ID/self/nonfinite grip and destructor constraint cleanup pass.

All native hanging/recoil/catch fixtures asserted **zero external body contacts**, so contact cannot fake support or momentum. Force sampling uses a native step listener to read the previous internal collision-step lambda, plus a final sample after each outer Update. Together these observe every internal step for this fixed schedule. The getter is a sampled peak, not an automatic all-substep peak: production outer-only `post_step(h/4)` sees only the last step. Passing h rather than h/4 would understate force by4.

## Remaining boundary

The module accepts the caller's actual rigid world grip; reach/surface eligibility, hand transfer/regripping, changing support frames, ordinary input/top-out, rendered hand/capsule agreement and route regressions remain parent integration work. Cargo-net material grips require their separate soft adapter. The one-hand transient/catch drop are actual limitations for that movement design. Native fixtures establish coupling behavior, not ordinary gameplay, full energy closure, Android delivery or Fold device quality. Reset/checkpoint callers must clear constraints before restoring/removing bodies and must not rotate the player while attached.
