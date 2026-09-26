# SCRAPERX — ASCENT ATLAS: GROUND RESET

**Status:** Product content authority, revised by owner direction on 2026-09-26. Subordinate to Laws and GDD. **All proposed mechanism dimensions below are DESIGN TARGETS unless explicitly described as source.**

## 1. Content decision

Retire the previous campaign, including its ground water screw/lift. Rebuild from grade using large, visibly connected basic mechanics. Keep the tower, atmosphere and full athletic movement. The previous fixed bands, intake problem, capability chores and AS-001–015 queue are not requirements for the new ascent. Upper content is unauthored rather than silently restored.

## 2. Datum and current world

Use metres, kilograms, seconds and radians. Current native/Godot world is Y-up; grade walking surface is Y=0. Tower frame centre is X=0, Z=-150, half width 26 m. Lower deck rings are spaced 11 m in existing source. The distant tower mass reaches 1,600 m. Body centres, capsule centres, COM heights and walking surfaces are distinct.

Normal player spawn is the exterior grade approach. Removed machine-linked stairs and machines cannot supply a handoff. The ordinary tower staircase remains an optional walkable fallback to the existing 154 m deck. The retained structure is a static collision representation, not proof of a deformable/load-rated skyscraper. The summit remains a product destination, not a currently reachable or fully authored route.

## 3. Spatial and visual rules

Place a source, transfer and useful outcome so the player can inspect them from supported ground. Show major moving masses and joint axes at ordinary first-person distance; a large enclosing shell does not make an invisible release train readable. Collisions match the apparent surfaces, including open gaps. Use broad capture regions, contained rolling lanes and visible receivers instead of requiring a bullet-sized projectile to hit a hidden latch.

Player waiting must buy visible, consequential motion. Target roughly 5–15 seconds of main motion for an opening mechanism, measured in runtime; this is a pacing target, not a timer controlling success. Recovery must be faster than repeating unexplained setup. Do not add a water-filling delay merely to justify ascent.

## 4. Player capabilities and alternate routes

Retain existing parkour, crouch, carry, jump and parachute contracts. Do not add invisible barriers or nerf jump/reach to protect a puzzle. If real frame geometry permits climbing, that route is valid. Keep ordinary ramps, stairs and, where a real supported ladder interaction exists, ladders as an uncomplicated fallback. Their longer route is the natural cost; do not lock them behind the puzzle or a fail counter. The current source retains ordinary stairs, not a new ladder-climbing verb. Remove the rejected machine-linked stair assemblies without removing this fallback.

A mechanism's output should become a stable support, bridge or staircase. Avoid mandatory player launches across a skyscraper gap as the opening ascent. Physics may throw loose loads, but a launch must have a capture envelope and its misses must have a real aftermath.

## 5. Receiving-support convention

Every implemented handoff names its native support, top surface, pose, clear standing area, approach and exit, stability at arrival and failure landing. A height label alone is insufficient. The receiving support and route away from it are built and tested before promoting an upstream spectacle to a playable mechanism.

## 6. Opening candidate: the pipe-loaded balance bridge

**Not implemented. AS-016 is resolving this design with isolated geometry and native contact probes.** Promotion to normal play remains blocked until loaded release, pipe capture, arrest and the actual player handoff pass.

The player sees a rack of heavy steel pipes above a wide short-arm pan. Releasing the rack lets pipes roll into the pan while a visible prop holds the beam. A second physical release frees the loaded beam. Its long arm is a broad walking deck; the pan descends onto a visible crush receiver that absorbs the motion and carries the residual load. The player climbs from a permanent grade approach, exits sideways onto a fixed landing, and continues to the first tower deck. Gravity and leverage supply the work.

Revised design envelope. Values below are CHOSEN unless a relation is shown; they are not production measurements. The former 4 t straight-tail sketch is rejected: its surplus energy is excessive, an underside long-arm seat has the wrong reaction direction, and a landing directly ahead of the tip obstructs the rising sweep.

| Quantity | Candidate value / consequence |
|---|---|
| Useful receiver | +8.0 m walking surface; clear X=[7.56,10.56], Z=[−112.580635,−108.580635], a 3 × 4 m landing beside the bridge |
| Long arm | 20 m walking deck, 3 m usable width, 0.4 m collider depth; 8,500 kg; COM local (10,−0.2) relative to the walking-surface pivot in the rotation plane |
| Pivot surface datum | World (6,+0.6,−90), long arm toward −Z, hinge axis +X. Deck centre is P + 10(0,sinθ,−cosθ) − 0.2(0,cosθ,sinθ). |
| Rotation | 0 to asin(7.4/20) ≈ 21.72 degrees; far end moves horizontally from 20 m to 18.58 m |
| Raised short arm | Attachment vector (−5,+2.8) in the rotation plane; pin starts at Y=3.4 and ends at Y=1.351289, a DERIVED 2.048711 m descent. The arm is diagonal rather than a low horizontal member. |
| Level pan linkage | Two equal 5.73 m arms; fixed trunnions and corresponding pan pins separated vertically by 1.2 m. Arms outside the pan at X=3.4 and X=8.6; combined arm mass 1,000 kg. Crosshead and pan clearances still require native proof. |
| Pan and pipe ballast | Pan 1,000 kg; twenty separate 800 kg pipes = 16,000 kg captured load. Four lanes of five sections; each section 1 m long, outer radius 0.4 m. With chosen steel density 7,850 kg/m³, inner radius sqrt(0.4²−800/(π×7850×1)) ≈ 0.357157 m. Use hollow-cylinder inertia. |
| Pan envelope | Preliminary clear 4.4 m across X × 4.3 m along Z; attachment-to-lowest-bottom at most 1.10 m. Keep its full stroke above grade and keep linkage members outside the loading volume. |
| Loading rack | Four contained lanes south of the pan, feeding toward −Z at a chosen 5° slope. Gate, release tackle, delivery clearance and complete capture remain native-probe work. |
| Motion budget | Loaded pan releases 341.664 kJ; arms release 10.049 kJ; deck COM rise consumes 309.708 kJ. DERIVED net 42.004 kJ before bearings, crush, rider and other losses. An 85 kg rider at the far end consumes another 6.171 kJ. Rack impact is not credited as free lifting work. |
| Reduced swing model | For rider mass m at distance r, Qg=9.81[(2500−mr)cosθ+47300sinθ] N·m; I=1,703,013.333+mr² kg·m². These derive from the declared mass layout; actual constructed mass properties must agree. |
| Arrest reaction | Upward force under the descending pan produces opposing torque F(5cosθ+2.8sinθ). Finite crush work and used stroke are consequential state. Constitutive parameters, rebound and native agreement are being evaluated in AS-016. |

The side receiver avoids the tip's retracting arc. Its inclined entry occupies X=[7.56,10.56], Z=[−108.580635,−105.580635], with top plane Y=7.8−0.398264105(Z+108.580635). It ends with a 0.20 m rise onto the flat landing. A supported 3 m wide connector continues from Z=−112.580635,Y=8 to Z=−124,Y=11 and the existing first deck. The grade approach is west of the bridge: a short ramp reaches +0.6, then a side cheek joins the first 2 m of the deck. The operator returns around the south end of the rack rather than crossing the pan's swept volume.

These surfaces must accept a bounded stopping range, not one exact angle. Geometry screening is not a native walking proof. The player normally crosses after arrest; early boarding is included in the load cases. A finite material law may represent visible crushing, including elastic recovery and irreversible deformation. Do not pose-snap, erase velocity or replace the loaded contacts with a completion flag.

Critical work before normal-play integration: loaded release convergence and reachable effort; actual pipe feeding and containment; constructed mass/inertia and joint reactions; finite arrest and stable crossing; native walking at both ends; miss aftermath and checkpoint state. A large accessible release member only frees stored energy. A 10 kg ball is not assumed to dislodge a wedge supporting tonnes.

## 7. Later chain direction

After the first receiver is proven, combine selected macro operations with each output creating a new route. Candidate sequence: pipe avalanche → balance bridge → released heavy roller → pendulum that seats a bridge → falling monoliths that lay a broad stair. A later teeter-totter may deliver ballast to an upper receiver, but only after its trajectory and capture volume close. Each sequence may be shortened; eleven effects are not an obligation.

The full corrected Colossus evaluation lives in `Mechanism-Ideas-and-archetypes.md`. It is option material. No upper elevations, connecting spans or stored energy may be assumed from that catalogue.

## 8. Fall geography and aftermath

Rollers and monoliths travel in visible lanes outside required player standing areas. Physical misses can block, damage or settle into the world. Provide reachable recovery controls/supports or preserve a valid alternate path. Do not delete fallen material to keep the scene tidy. Deliberately resetting to a checkpoint must restore a coherent whole mechanism, including velocities, inventory, links and already committed routes.

## 9. Regression substrate

`WO-000`–`WO-013` records and legacy AS test scenarios remain regression provenance. They are selected explicitly in test processes, never automatically constructed in the normal scene. Their entity IDs and local fixture coordinates are not the new campaign map. A successful old water/cage test does not establish an accepted opening mechanism.

## 10. Upper tower

The retained tower silhouette, frame and deck rings are space for future authorship. The ordinary 154 m staircase remains as the owner-requested fallback. The old 198 m cage ride, 220 m connection and 340 m wet route are retired. Do not silently rebuild that schedule. Extend upward only from a demonstrated receiving support using the same large visible mechanics and legitimate parkour.

## 11. Summit

Ultimate success requires the actual player standing on supported geometry at the 1,600 m destination. A trigger or mission flag cannot replace that condition. The intervening ascent and summit geometry are not yet implemented as a complete route.

## 12. Ownership and sources

Native Jolt bodies/constraints and validated finite force laws own motion, contact and energy. Godot draws those body poses and visible connecting spans. UI may request a reachable action; no chained success flags, autoplay timestamps or presentation transform updates drive consequential bodies.

Rolling/inertia and rotational-work derivations use [OpenStax rolling motion](https://openstax.org/books/university-physics-volume-1/pages/11-1-rolling-motion) and [rotational work](https://openstax.org/books/university-physics-volume-1/pages/10-8-work-and-power-for-rotational-motion). Solver repeatability is a tested boundary, not a synonym for a chaotic pipe pile: see [Jolt documentation](https://jrouwe.github.io/JoltPhysics/).

## 13. Acceptance

Require unseeded normal-input operation, physical failure when the source/link is removed, plausible losses, stable actual player handoff, recovery/checkpoint continuation and first-person visual observation. Test variation in initial placement, timing and frame partitioning; test Android separately. A nominal run or restitution constant alone does not establish robust chain behaviour.
