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

**Not implemented; critical release/stop/clearance details remain BLOCKED.** This replaces the water screw idea, rather than repairing its visual rotor.

The player sees a rack of heavy steel pipes above a wide short-arm pan. Releasing the rack lets pipes roll into the pan. Their weight rotates one large beam; its long arm is a broad deck that rises into a fixed landing. The beam rests on visible structural seats. The player then climbs the inclined deck from a permanent grade approach to that landing. Heavy work is gravity and leverage; no hidden lift or triggered teleport.

Preliminary design envelope, to be placed in the yard without intersecting current tower footings:

| Quantity | Candidate value / consequence |
|---|---|
| Useful receiver | +8.0 m walking surface; 3 × 4 m clear landing with a supported onward endpoint, not an assumed connection to the old stairs |
| Long arm | 20 m from pivot to far support; 3 m usable width; preliminary moving deck mass 4,000 kg, COM 10 m from pivot |
| Pivot surface datum | +0.6 m; permanent approach from grade must be authored and swept clear |
| Rotation | 0 to asin(7.4/20) ≈ 21.72 degrees; far end moves horizontally from 20 m to 18.58 m |
| Short arm | 5 m; pan attachment descends 1.85 m; reserve a visible pit with clearance below the lowest body, not an underground collider shortcut |
| Pipe ballast | Twenty 800 kg pipes = 16,000 kg captured load; pipe shape/inertia must reflect hollow steel cylinders, not a solid cylinder accidentally using the same inertia |
| Loading rack | Elevated relative to pan, contained by side walls; exact placement, pipe delivery height and catch geometry unresolved |
| Initial motion budget | Ballast descent after capture releases about 290.4 kJ. Deck COM rise needs 145.2 kJ; an 85 kg player rising 7.4 m needs 6.2 kJ. About 139 kJ remains **before** pan/beam inertia, bearings, collisions, braking and other losses are closed. Rack-to-pan impact is not credited as useful extra lift work. |
| Static torque check | Captured ballast: 16,000g × 5 cos(theta); long deck: 4,000g × 10 cos(theta), plus player and all omitted member weights. These simplified moments favour raising the deck; they are not a structural or dynamic rating. |

The pan remains level through a real hinge and needs enough lateral restraint to retain pipes throughout the stroke. The player crosses after seating; normal physics still applies if they board early. The visible seat must carry the loaded beam and prevent rebound/slip. Use finite braking/damping with an energy ledger; never pose-snap or zero velocity on a completion condition.

Critical work before implementation: dimensional CAD/primitive layout against actual tower coordinates; release geometry and required human effort under full preload; pipe containment and anti-wedge margins; full inertia and mass ledger; bearing/stop loads; finite arrest and seat stability; controller walking/step transitions at both ends; misses, unloading/rearming and checkpoint state. A 10 kg bowling ball does not automatically dislodge a wedge supporting tonnes. A large accessible release member may only retain/release energy; it must not supply the lifting work.

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
