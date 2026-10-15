## Final operator geometry update — trial3 PASS

The real side treads add38.009088kg of steel, so root invalidated only the previous geometry-dependent load result and ran the same unchanged fixture against nativev2 archive18af7c94f9048807d90036ce33a1b9d48c4ff38dcb38868c06171c170eaddf9b. Trial3 PASS/exit0,1.17s. Final far case maxforwardpitch.294601rad, actualrearunload, blockends ongroundZ4.09461/Y.136218 with realgroundcontact and recovered rearcontact23257N. Positivehydraulicwork17557.7J, conservativebatterydebit96000J, excess0. All original corrected finite/contact/derived-angle guards unchanged. Exactnewvalues/initialmasses andnearcase are in `load-stability-window.trial-3.log`; earliertrial1/2 remain historical below. CurrentyardSHA733248d2501d2fb655055f33b83c4120b88a613aad19d42c807f1ce677bfe6d2, test29b893fbff1ba54730178e442b78c8f8baab08135504d5f62b4930bba86218b0, binary8e1901428212fa0115878905cdb116e5141dd9a5d3185bca309c7b49f79bbd66.

# Production forklift heavy-load stability: accepted native result

Native acceptance **PASS**, 2026-10-15, branch `ChatGPT`, base revision `8569c721432e4139df6286b8e8d1cce94b2db9ca` plus the recorded working sources. The same1700kg block remains stable near the truck; placed1m farther forward, it physically unloads the rear wheel, tips the chassis16.8°, sheds onto actual ground, and allows the truck to recover rear support. Both cases use unchanged production laws and geometry. Original trial1 failed an unsupported successful-lift oracle; root inspected that evidence and authorized its bounded correction. One corrected native run passed. Both receipts remain preserved below.

The new `tests/warehouse_forklift_load_tests.cpp` uses the real, unchanged `build_warehouse_forklift_yard` and `WarehouseForklift`, including the newly authored physical cab. Initial construction translates the yard by `(0,0,-14)`, putting the chassis shape origin at `(0,.65,0)`. The added ground is static. Both cases add the same independent 1700 kg solid steel block, density7800 kg/m³, half extents `(.5,.4,.136218)m`, initially centered at `(0,.62,z)`. Only initial load-center Z differs:2.3m versus3.3m. Real fork blades end atZ3.53m. The existing yard cargo is remote from the truck.

Commands consist of180 neutral settling ticks, then720 lift-up ticks with the default service brake applied. Native90Hz/four collision substeps, normal gravity, actual Kit pre/post and native forklift pre/post are used. There are no operational pose/velocity writes, external impulses/forces, gravity changes, cargo constraints/parenting, adhesion, forced COM, rating changes, or production geometry changes. The new test is passing but not wired into CMake by this worker; root owns that integration decision.

The six mechanical predictions and limits are:

1. Outcome: the near block should rise on collision-supported forks with rear support; moving the same load forward should remove rear support and cause forward rotation. This fixture is not a playable approach, receiving route, APK, or phone handling proof.
2. Cause: existing finite18kN lift acts through actual72kg fork frame/40kg carriage,220kg mast and chassis.1700kg plus112kg moving hardware requires approximately17.776kN before acceleration. Neutral tilt uses its existing17kN·m passive brake.
3. Reaction: steel block→actual forks→coupler/carriage→slider/mast hinge→chassis→native front/rear wheel contacts→static ground. The block can lose contact and fall.
4. Storage/loss: gravity and actual body motion; existing collision/friction and finite motor/brake laws. Positive hydraulic work and conservative battery debit are reported separately; this is not full energy closure.
5. Operating cases: these are two initial configurations, not a validated operating range. Actual Jolt inverse masses and Kit/Jolt COM are read back, rather than treating shape origins as COM.
6. Failure/exit: far load moves forward, rear support disappears, truck pitches forward, cargo sheds, and rear support returns. Do not infer sustained overturning with retained cargo or safe handling.

Actual initial truck mass is3334.95kg, including3002.95kg chassis. About the authored front-wheelZ=.75m line, actual body COMs give3896.49kg·m restoring moment. The near block contributes2635kg·m, the far block4335kg·m overturning moment. This predicts the sign of instability; it does not predict a final resting angle or keep cargo aboard.

| Observed quantity | Near centerZ2.3m | Far centerZ3.3m |
| --- | ---: | ---: |
| Maximum forward chassis pitch | .0143835rad (.824°) | .293353rad (16.808°) |
| Minimum rear normal force |3212.9N |0N |
| Maximum load COM rise from settled height |2.24486m |.0389578m |
| Positive hydraulic work at end |42244.9J |17413.8J |
| Conservative battery debit |96000J |96000J |
| Energy reservation excess |0J |0J |

First far-case rear-unloading receipt: tick303, rear `contact=false`, normal0N, forward pitch.0406246rad, front normal46268.4N, chassis COM `(0.000714386,.801825,-.601059)m`, cargo COM `(-.00262202,.530394,3.35562)m`, lift.356992m, measured lift force15261.2N, neutral tilt torque6781.25N·m, positive hydraulic work4359.7J. At tick451 pitch is.224554rad, rear remains unloaded, and cargoZ=3.60023m. At tick541 cargoZ=4.1651m with pitch.265381rad; by631 rear support has returned and cargo is on the ground. The actual measured caps and zero-reservation-excess assertions passed throughout both cases.

Historical trial1 exits1 at `finite actuator actually raises independent heavy load`: far maximum rise.03896m fails the locally authored.35m guard. The far maximum pitch also does not reach the fixture's.35rad tip guard. The near case passed all its guards. **Failure classification: unsupported diagnostic acceptance for a load that physically sheds; no basis for changing production laws.** No new torque/adhesion grant is justified. The corrected trace additionally confirms the mast reaches its unchanged forward stop at approximately-.13963rad before rear unloading. Neutral tilt friction saturates at17000N·m during initial mast rotation. These are real native responses, not an authorized production repair target.

The root reassessment below resolves the intended observable failure. If sustained loaded overturning is later required, first inspect actual cargo/fork manifold and relative rotations at the first slip to distinguish block rolling, fork sliding, and mast-brake slip. Do not search coefficients/geometry to manufacture retention. Reuse these accepted cases while their sources match; broad operating-range/height/tilt/braking/turning gameplay remains separate work.

Build exit0: focused C++17/O3/pthread, `-Wall -Wextra -Wpedantic -Werror`, exact pinned Jolt feature defines, `-fno-rtti` on forklift TU. Fresh test/forklift/yard objects link before the accepted Kit archive, avoiding Simulation/Snapshot ABI dependence. Build script/complete argv and hash receipt: `/data/data/com.termux/files/usr/tmp/scraperx-forklift-load-20261015/{build.py,build-receipt.json}`. Binary SHA256 `48baa79793439f98039181263df7b4b40b16f47425fb994a69528319fdd220d0`.

Original source-bound runtime receipt: [persistent10minute/two-failure state](load-stability-window.json), [complete first-trial trace](load-stability-window.trial-1.log). Trial1 exits1 after1.58s execution; stopped at the first failed acceptance. Original test SHA256 `fc5ab6e8c7c0d2de7a7693bbf8d21cbebae02cb5b6b8bc54d8a0462b5354652d`; unchanged forklift source `3e2b07016a1548b63814197795e155a12a12b8aa01065401886ed72af63386bf`; unchanged production yard source `c1fb19ba275a5f8c733695df47b6710efa2cafddf10b81959623e8900b9caacb`. Base Kit archive `ca8ce5fb26f35f16cb38f523ff115357396488646c3e22cff04bb0db3e176189`; pinned Jolt archive `8711a784f394fdf91af3366ab0b6695e80322cb73b46a8d1659e775b20aced1b`.

## Root's acceptance correction after inspecting trial1

Root inspected the actual test and trace and identified the supported cause of the diagnostic failure: the far-case oracle incorrectly demanded successful cargo elevation and20° rotation after physical instability. Original acceptance asked for load-center-caused forward tip with actual rear unloading. Honest early shedding is a valid physical consequence; keeping cargo raised through it would add an unauthorized requirement.

Authorized correction changes only test acceptance/observation. Keep the near-case successful lift>.35m, pitch<.10rad, rear support, both actual fork-contact guards, finite-source work, native force/torque caps, and zero reservation excess. Far acceptance now requires actual rear contact loss, maximum pitch exceeding twice the suspension clearance angle `asin((.14-.06)/(.75-(-.90)))`, actual forward cargo shedding beyond its initial center+.5m, final COM below.25m with a current real ground contact, and recovered rear contact/>500N force. The complete720 lift ticks must run to capture failure and recovery. A contact set tracks add/persist/remove callbacks so the final ground observation cannot be satisfied by an earlier departed contact. No production law, command, initial geometry, coefficient, cap, or inertia changes.

Original rejected test source is preserved at `/data/data/com.termux/files/usr/tmp/scraperx-forklift-load-20261015/trial1-source.cpp`; original trial1 receipt/log/binary remain intact. Root authorized exactly one corrected affected run after test-only recompilation, with a causal reassessment recorded in the same persistent state.

## Corrected trial2: PASS

Strict test-only recompile and relink exit0, reusing the original exact forklift/yard component objects after validating their production source/header hashes. No unchanged production TU was recompiled. Actual build argv, source/header/archive/component-object hashes and executable receipt: `/data/data/com.termux/files/usr/tmp/scraperx-forklift-load-20261015/corrected-build-receipt.json`.

[Corrected complete native trace](load-stability-window.trial-2.log) exits0 in.98s and preserves the same mechanical trajectory as trial1. Derived suspension clearance is.0485039rad; required forward pitch is.0970077rad (5.558°). The far case reaches.293353rad with actual rear contact loss. At final tick900 the far cargo COM is `(-.000828441,.136218,4.08898)m`, beyond initialZ3.3+.5m, and the current actual ground-contact set is nonempty. Rear contact is restored with22717.1N normal force. Near cargo finishes atY2.7837m, rose2.24486m, rear force never fell below3212.9N, and maximum pitch is.0143835rad. All fork-contact, native force/torque, work/debit and reservation-excess guards pass. Both cases use96000J conservative source debit, while positive measured work remains separately reported.

Current accepted test SHA256 `29b893fbff1ba54730178e442b78c8f8baab08135504d5f62b4930bba86218b0`; binary `06a609d966deea1f7b67eb487806c7fbfa3f026b7f424f02935c6aee513ef86b`; cached exact forklift object `61ae7c3fdb829f6d047a014b8953727d451efe5c016a2f6abcc168fad380562a`; yard object `7d983e1d3d2934799c4763ef33b1d41ae479ffb49a6e9c9039fb2223752cb90f`. Production/header/archive hashes remain those above and in the corrected receipt. No further experiment, full-build campaign, or device claim.
