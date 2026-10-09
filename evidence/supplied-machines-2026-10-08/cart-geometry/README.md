# Bounded native cart geometry evidence — 2026-10-08

These receipts record private Jolt/Kit component probes of `build_gravity_cart`, not the actual player, touch route, rendered encounter or complete production controller. The final case uses an actual native 85 kg capsule with translation XYZ allowed and rotation locked, centred on the deck, with chosen friction .85, plus the actual 25 kg maintenance block. Root’s different finite impulse controller and real-player/touch path remain pending in this evidence slice.

The factory uses the existing Kit/native Machine and its constraint lifetime. No second PhysicsSystem owner, pose/velocity prescription, powered rollers, proximity catch, invisible weight or collision mask supplies success. Static receiver/support bodies exchange impulse with the foundation; finite cart, slab, rope and retainer loads remain native. Compiled libraries were read only; all probes used one private executable target, Ubuntu g++ 15.2.0 and pinned Jolt `e77f175595e64cb44218cc9d9d56fc365ad0e36a`.

## Geometry and chosen masses

The 60° track has direction (.5, sqrt(3)/2, 0), normal (-sqrt(3)/2, .5, 0), stroke 25.403412 m and level-deck centres (-41.8,330,-142) → approximately (-29.098294,352,-142). Four radius .28 m, width .08 m rollers have 4.8 m wheelbase and paired Z±.65 m. Real native hinges have world-Z axes and motors Off. Native railhead box contacts carry the cart; only joined chassis/roller pairs exclude collision. The lateral guide fixes world-Z translation and X/Y rotation, leaving XY translation and Z pitch free. Observed XY/pitch guide lambdas are zero.

Every chassis allowance belongs to an actual compound part. Chosen masses below are authoring values, not universal steel-density claims. Timber uses the explicit 600 kg/m³ choice and actual solid volume after the front X[1.25,1.6], Z±.06 m rope-well cut.

| Current actual parts | Chosen mass, kg |
|---|---:|
| Timber planks, including physical gaps/notch | 120.54 total |
| Two low longitudinal frame members | 12 each |
| Rear/front axle crossmembers | 8.307692 / 6 |
| Two downhill / two uphill posts | 7 each / 2 each |
| Two diagonal braces | 5.7512 each |
| Two deck-rim members | 4 each |
| Control pole/head, tow eye, three retaining lips | 2+1, 2, .4 each |
| Visible low steel body/ballast | 72.4499 |
| Chassis / four separate rollers / bare cart | 275 / 35 each / 415 |
| Separate maintenance block / slab | 25 / 570 (566 body+4 eye) |
| Current short retainer blade / journal | 5.1875 / 2 = **7.1875** |

The rear axle physically extends Z[-.65,+1.15] m; its added 2.307692 kg replaces the same mass in low ballast. The retainer pivot is outboard at Z+1.05, blade spans Z[.87,1.23], giving .07 m deck-side clearance. The physical resting stop spans Z[1.18,1.34]: .03 m axle-end clearance and .05 m blade overlap. Its bored bearing/brace remain outboard. The rear axle has .05 m clearance from the 341 m tongue’s Z edge. These are authored collider clearances; the loaded swept probe supplies the contact evidence.

IDs: statics 1954 track, 1955 loading receiver, 1956 intermediate tongue/apron, 1957 upper receiver, 1958 slab cage/H support, 1959 S support/bumper/retainer support. Dynamics 2320 chassis, 2321–2324 rollers, 2325 slab, 2326 retainer, 2327 carryable block; private surrogate 2330. Factory allocation is 14 actual bodies; existing 1950–1953 and ladder 1960 are preserved. Root-added `deck_control` is preserved. The slab has a native vertical slider, initial MotorOff friction 9 kN and positive-only reset force 0..9 kN. The tow is native tension-only 1:1 with two variable legs; fixed S–H is excluded from maximum length but visible in the rope polyline. Rollers are absent from Machine.drives.

## What the receipts establish

- [Initial native mass/inertia](initial-native-mass-inertia.log): actual cooked nominal prototype and initial factory COM/inertia, plus initial rail/stance/guide checks. These tensors **precede the rear-axle extension** and must not be reported as the final geometry tensor. Current body masses above were measured in the final dynamics probes; the final assembly tensor has not been remeasured.
- [Tow-loss hold](tow-loss-hold.log): loaded ascent reaches floor 352.00235 m; real tow disconnection causes .167253 m rollback and native pawl retention through slab hoist. The resulting loaded pawl cannot open at 120 Nm. This is a separate failed-tow release case, not evidence that ordinary reset is broken. Fixed bumper/rim contact manifolds also exist; these receipts do not assign every support force exclusively to the pawl.
- [Upper intact reset](upper-intact-reset.log): with tow always intact and slab braked at the upper seat, the retainer opens past 1.4 rad using peak 10.56 Nm, then positive-only finite 9 kN slab hoist returns the cart to the lower stop. Four true rail contacts and both deck loads persist on every sampled native step; XY/pitch guide reactions remain zero.

The intact case deliberately uses native **Velocity motors with finite force/torque limits**, not Root’s production finite impulse controller. Its .6 m/s slab target is not a hard cart speed cap: actual whole-case peak speed is 1.103292 m/s. Work numbers use final-substep native lambdas and are approximate positive-source work, not full four-substep energy closure. Slider lambda while MotorOff represents friction, not energized reset work. No release after the failed-tow loaded-pawl failure or full fault campaign was completed.

## Recorded commands and local diagnostics

The factory/dynamics private scripts compile probe source plus the factory source with `-std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror`, linking existing `libscraperx_sim.a` and pinned `libJolt.a`. Strict factory/dynamics compiles passed; receipt-specific runtime outcomes are in the logs. The repo was `ChatGPT` at `8f449a28a2474b96413b541ed14135f11aea65b7` during final dynamics; factory/probe changes were uncommitted alongside preserved concurrent work. These logs are compact transcriptions of actual stdout, not claims of an immutable production acceptance run.

Private working directory: `/data/data/com.termux/files/usr/tmp/scraperx-cart-native-geometry`. Actual commands were `sh build.sh`, `sh build-factory.sh` or `sh build-dynamics.sh`, then `proot-distro login ubuntu -- /data/data/com.termux/files/usr/tmp/scraperx-cart-native-geometry/cart_probe`. The latter scripts replace the same executable target. Private `intact-reset.csv`, `intact-reset-contacts.csv` and `broken-tow-final.csv` retain detailed local diagnostics; they are not checked-in portable proof or required reader dependencies. No further production edit, build or run was performed for this evidence write.
