# SCRAPERX — ASCENT ATLAS: GROUND RESET

**Status:** Product content authority, revised by owner direction on 2026-09-26. Subordinate to Laws and GDD. Section 6 records the implemented AS-016 design envelope; its exact-source runtime evidence is separate. Section 7 records the delivered AS-018 +33→44 m stair; section 10 records AS-017 and the local AS-019 +44→66 m candidate. Dimensions for later, unimplemented mechanisms are DESIGN TARGETS unless explicitly described as source.

## 1. Content decision

Retire the previous campaign, including its ground water screw/lift. Rebuild from grade using large, visibly connected basic mechanics. Keep the tower, atmosphere and full athletic movement. The previous fixed bands, intake problem, capability chores and AS-001–015 queue are not requirements for the new ascent. AS-017 authors a static façade route to +33 m, followed by AS-018's swinging stair to +44 m and the new AS-019 lift and exterior climb to +66 m. Other upper mechanism routes are not silently restored.

## 2. Datum and current world

Use metres, kilograms, seconds and radians. Current native/Godot world is Y-up; grade walking surface is Y=0. Tower frame centre is X=0, Z=-150, half width 26 m. Lower deck rings are spaced 11 m in existing source. The distant tower mass reaches 1,600 m. Body centres, capsule centres, COM heights and walking surfaces are distinct.

Normal player spawn is the exterior grade approach. Removed machine-linked stairs and machines cannot supply a handoff. The ordinary tower staircase remains an optional walkable fallback to the existing 154 m deck. The retained structure is a static collision representation, not proof of a deformable/load-rated skyscraper. The summit remains a product destination, not a currently reachable or fully authored route.

## 3. Spatial and visual rules

Place a source, transfer and useful outcome so the player can inspect them from supported ground. Show major moving masses and joint axes at ordinary first-person distance; a large enclosing shell does not make an invisible release train readable. Collisions match the apparent surfaces, including open gaps. Use broad capture regions, contained rolling lanes and visible receivers instead of requiring a bullet-sized projectile to hit a hidden latch.

Player waiting must buy visible, consequential motion. Target roughly 5–15 seconds of main motion for an opening mechanism, measured in runtime; this is a pacing target, not a timer controlling success. Recovery must be faster than repeating unexplained setup. Do not add a water-filling delay merely to justify ascent.

## 4. Player capabilities and alternate routes

Retain existing parkour, crouch, carry, jump and parachute contracts. Do not add invisible barriers or nerf jump/reach to protect a puzzle. If real frame geometry permits climbing, that route is valid. Keep an uncomplicated last-option route for players who do not want a mechanism or parkour challenge; its longer route is the natural cost. The current source's 14 inclined fallback slabs are ramps with cosmetic treads, visually dominant throughout the tower. This fails the intended route hierarchy and needs redesign; do not describe them as ordinary stairs or treat them as approved layout. AS-017 adds a native climb on reachable thin holds. Remove rejected machine-linked stair assemblies without casually deleting the requested fallback.

A mechanism's output should become a stable support, bridge or staircase. Avoid mandatory player launches across a skyscraper gap as the opening ascent. Physics may throw loose loads, but a launch must have a capture envelope and its misses must have a real aftermath.

## 5. Receiving-support convention

Every implemented handoff names its native support, top surface, pose, clear standing area, approach and exit, stability at arrival and failure landing. A height label alone is insufficient. The receiving support and route away from it are built and tested before promoting an upstream spectacle to a playable mechanism.

## 6. Opening mechanism: the pipe-loaded balance bridge

**Implemented in native source as the normal opening mechanism.** AS-016 records current native/input/render observations; exact-commit workflow and APK status remain in `00_START_HERE.md`. Desktop proof does not establish Android execution.

The player sees a rack of heavy steel pipes above a wide short-arm pan. Releasing the rack lets pipes roll into the pan while a visible prop holds the beam. A second physical release frees the loaded beam. Its long arm is a broad walking deck; the pan descends onto a visible crush receiver that absorbs the motion and carries the residual load. The player climbs from a permanent grade approach, exits sideways onto a fixed landing, and continues to the first tower deck. Gravity and leverage supply the work.

Implemented design envelope. Dimensions/material constants below are CHOSEN unless a relation is shown; runtime observations are separately classified in AS-016. The former 4 t straight-tail sketch is rejected: its surplus energy is excessive, an underside long-arm seat has the wrong reaction direction, and a landing directly ahead of the tip obstructs the rising sweep.

| Quantity | AS-016 design value / consequence |
|---|---|
| Useful receiver | +8.0 m walking surface; clear X=[7.56,10.56], Z=[−112.580635,−108.580635], a 3 × 4 m landing beside the bridge |
| Long arm | 20 m walking deck, 3 m usable width, 0.4 m collider depth; 8,500 kg; COM local (10,−0.2) relative to the walking-surface pivot in the rotation plane |
| Pivot surface datum | World (6,+0.6,−90), long arm toward −Z, hinge axis +X. Deck centre is P + 10(0,sinθ,−cosθ) − 0.2(0,cosθ,sinθ). |
| Rotation | 0 to asin(7.4/20) ≈ 21.72 degrees; far end moves horizontally from 20 m to 18.58 m |
| Raised short arm | Attachment vector (−5,+2.8) in the rotation plane; pin starts at Y=3.4 and ends at Y=1.351289, a DERIVED 2.048711 m descent. The arm is diagonal rather than a low horizontal member. |
| Level pan linkage | Two equal 5.73 m arms; fixed trunnions and corresponding pan pins separated vertically by 1.2 m. Arms outside the pan at X=3.2 and X=8.8. Main assembly is 9,000 kg including the 8,500 kg deck and 500 kg arm/crosshead/shoe; auxiliary arm is 500 kg. The wider pin planes clear the retaining sides. |
| Pan and pipe ballast | Pan 1,000 kg; twenty separate 800 kg pipes = 16,000 kg captured load. Four lanes of five sections; each section 1 m long, outer radius 0.4 m. With chosen steel density 7,850 kg/m³, inner radius sqrt(0.4²−800/(π×7850×1)) ≈ 0.357157 m. Use hollow-cylinder inertia. |
| Pan envelope | Floor 4.6 × 4.5 m, sloped 3° toward the front. Open side rails retain the pipes while exposing the load. Pan origin begins (6,2.4,−85); central striker underside is local Y=−0.15. A 0.3 m rear lip retains the delivered cargo. |
| Loading rack | Four contained lanes south of the pan, feeding toward −Z at a chosen 5° slope. Rack nose is at +2.9 m. A top-hinged gate rests on a physical roller/prop, released through a 3:1 horizontal lever and direct cord. Twenty native pipes feed into the moving pan. |
| Motion budget | Loaded pan releases 341.664 kJ; arms release 10.049 kJ; deck COM rise consumes 309.708 kJ. DERIVED net 42.004 kJ before bearings, crush, rider and other losses. An 85 kg rider at the far end consumes another 6.171 kJ. Rack impact is not credited as free lifting work. |
| Reduced swing model | For rider mass m at distance r, Qg=9.81[(2500−mr)cosθ+47300sinθ] N·m; I=1,703,013.333+mr² kg·m². These derive from the declared mass layout; actual constructed mass properties must agree. |
| Arrest reaction | Upward force under the descending pan produces opposing torque F(5cosθ+2.8sinθ). CHOSEN material: 90 kN yield, 15 MN/m stiffness, 0.65 m constitutive stroke; the isolated sweep varied yield ±5%. Geometric floor contact limits usable compression to approximately 0.58 m. Finite crush work and used stroke are consequential state. These are design values, not measured timber properties; native source owns full loading and contact; measured final-source results are in AS-016. |

The side receiver avoids the tip's retracting arc. Its inclined entry occupies X=[7.56,10.56], Z=[−108.580635,−105.580635], with top plane Y=7.75−0.398264105(Z+108.580635). It ends with a 0.25 m rise onto the flat landing. A supported 3 m wide connector continues from Z=−112.580635,Y=8 to Z=−124,Y=11 and the existing first deck. The grade approach is west of the bridge: a 2.5 m wide approach centred at X=1.69 reaches +0.8 at Z=−90.5, then a 4 m wide side cheek centred at X=2.44 joins the first 2 m of the deck. This clears the pivot crosshead. The operator returns around the south end of the rack rather than crossing the pan's swept volume.

These surfaces must accept a bounded stopping range, not one exact angle. Geometry screening is not a native walking proof. The player normally crosses after arrest; early boarding is included in the load cases. A finite material law may represent visible crushing, including elastic recovery and irreversible deformation. Do not pose-snap, erase velocity or replace the loaded contacts with a completion flag.

Production ownership: `src/sim/pipe_bridge.cpp` constructs the one active assembly in Kit/Jolt. Control grips are 2530 (rack) and 2505 (bridge); no archived ballast-bridge geometry or IDs are instantiated. The material top begins at Y=0.655371263, and plastic deformation/work are checkpoint state. Native input, source controls, arrest, supported crossing and restoration have dedicated tests. A large accessible release member only frees stored energy. A 10 kg ball is not assumed to dislodge a wedge supporting tonnes.

## 7. Delivered swinging stair and later chain direction

The delivered AS-018 source connects the supported +33 m ring to the existing +44 m tower deck. Its Kit/Jolt hinge is at (−15.00, 32.70, −122.10). A fixed approach landing spans X=[−19.50,−15.43], Z=[−124.05,−120.60] at Y=33.00. The player can reach the hanging chain handle near (−17.50, 34.95, −120.90) from that landing. Pulling it past the trip lever's dead point releases the gravity-shaped 4 t flight and 13.5 t counterweight; pickup alone does not release the catch. Two visible yielding pads at X=−2.10, Z=−122.10±0.70 receive the flight's top landing. Their uncrushed top is Y=44.90 and their finite stroke is 1.75 m. A fixed side exit spans X=[−3.30,−0.30], Z=[−124.05,−123.14] with top Y=44.00, leading onto tower body 11. These are source geometry and chosen material values; [AS-018 delivery evidence](../03_EXECUTION/ASCENT/AS-018_SWING_STAIR_DELIVERY.md) owns the measured route and artifact claim.

Integrated native modes 0/1/2 reached supported +44 m from grade; mode 2 restored a moving-stair checkpoint after a fatal fall and continued. Modes 3/4 prove that waiting or grabbing then releasing the handle without pulling leaves the stair latched. Local ARM Godot headless touch and the green exact-source rendered touch route reached tower support at Y=44.90. The latest Android ARM64 artifact contains this source. Fold installation, play and performance remain separate.

The opening pipe bridge reaches its first receiver in the delivered AS-016 slice. The illustrative candidate chain remains pipe avalanche → balance bridge → released heavy roller → pendulum that seats a bridge → falling monoliths that lay a broad stair, with each output creating a new route. This is an optional design direction, not the committed phase order: the +11→33 m route is static façade parkour, the +33→44 m stair is delivered, and the +44→66 m lift/parkour route is a local candidate. A later teeter-totter may deliver ballast to an upper receiver, but only after its trajectory and capture volume close. Each sequence may be shortened; eleven effects are not an obligation.

The full corrected Colossus evaluation lives in `Mechanism-Ideas-and-archetypes.md`. It is option material. No upper elevations, connecting spans or stored energy may be assumed from that catalogue.

## 8. Fall geography and aftermath

For later roller or monolith designs, place travel in visible lanes outside required player standing areas. Physical misses can block, damage or settle into the world. Provide reachable recovery controls/supports or preserve a valid alternate path. Do not delete fallen material to keep the scene tidy. Deliberately resetting to a checkpoint must restore a coherent whole mechanism, including velocities, inventory, links and already committed routes.

## 9. Regression substrate

`WO-000`–`WO-013` records and legacy AS test scenarios remain regression provenance. They are selected explicitly in test processes, never automatically constructed in the normal scene. Their entity IDs and local fixture coordinates are not the new campaign map. A successful old water/cage test does not establish an accepted opening mechanism.

## 10. Upper tower

### AS-017 façade route: +11 → +33 m

Adapt Claude C1's static service architecture to the current front frame by translating its original +22→44 m route down 11 m. One native Kit body, ID 1600, contains the fixed landing, cabinet, duct, vent, monorail, davit and hanging ladder; presentation draws its native parts. No sibling world builder or hoist is instantiated.

The landing is X=[17.8,22.2], Z=[-124.05,-120.8], top +11 m. Cabinet top +12.7 m leads by a jump/hang to duct top +16.3 m. A thin vent at X=24, Z=-123.45 leads to the +22 m ring. From there a 0.3 m wide monorail at X=12.5 reaches Z=-119.3, top +22.3 m. The ladder's bottom rung is +24.6 m, requiring a leap; it leads to a davit at +33.3 m. The exit walks back to the tower ring near (11.2,33,-125.3), native tower support. A fall remains a real fall onto lower geometry or grade; committed checkpoint restoration is unchanged in authority. Current implementation and acceptance status are tracked in `00_START_HERE.md`; coordinates alone do not prove every clearance case.

### AS-019 upper lift and exterior route: +44 → +66 m

The local normal source now places a 4 × 4 m guided lift outside the front tower face at X=[2,6], Z=[−120,−116], with its walking top at +44 m before release. The player boards from the +44 m ring and pulls a side handle at Z=−115.8, outside the moving platform's sweep. The 2.8 t platform rises as a 3.2 t counterweight at X=10, Z=−118 falls through a 1:1 overhead cable. A 1.6 m yielding timber bed begins at +45.3 m under the weight. A slow upper catch seats the platform's walking top at +54.96 m beside the fixed connector to the +55 m tower ring. Kit/Jolt owns frame 1700, parkour fixture 1701 and moving bodies 2700–2704; presentation draws the same native parts.

From the +55 m ring, the player mantles a service cabinet near X=1.4, Z=−122 (top +56.7 m), catches and tops out on a duct (top +60.3 m), then climbs a narrow vent near X=5.4, Z=−123.45 onto the +66 m tower ring, support entity 11. The current inclined-slab fallback is an optional alternate route but is a layout defect requiring redesign. Exact-source [run #182](https://github.com/IssisX/ScraperX/actions/runs/36302892635) passed continuous rendered grade→supported +66 m touch, native, retained and Android export gates. Fold installation, play and sustained performance remain unverified. [AS-019 evidence](../03_EXECUTION/ASCENT/AS-019_UPPER_COUNTERWEIGHT_AND_PARKOUR.md) owns measurements and remaining falsifiers.

The retained tower silhouette, frame and deck rings are space for future authorship. The ordinary 154 m staircase remains as the owner-requested fallback. The old 198 m cage ride, 220 m connection and 340 m wet route are retired. Do not silently rebuild that schedule. Extend upward only from a demonstrated receiving support using the same large visible mechanics and legitimate parkour.

## 11. Summit

Ultimate success requires the actual player standing on supported geometry at the 1,600 m destination. A trigger or mission flag cannot replace that condition. The intervening ascent and summit geometry are not yet implemented as a complete route.

## 12. Ownership and sources

Native Jolt bodies/constraints and validated finite force laws own motion, contact and energy. Godot draws those body poses and visible connecting spans. UI may request a reachable action; no chained success flags, autoplay timestamps or presentation transform updates drive consequential bodies.

Rolling/inertia and rotational-work derivations use [OpenStax rolling motion](https://openstax.org/books/university-physics-volume-1/pages/11-1-rolling-motion) and [rotational work](https://openstax.org/books/university-physics-volume-1/pages/10-8-work-and-power-for-rotational-motion). Solver repeatability is a tested boundary, not a synonym for a chaotic pipe pile: see [Jolt documentation](https://jrouwe.github.io/JoltPhysics/).

## 13. Acceptance

Require unseeded normal-input operation, physical failure when the source/link is removed, plausible losses, stable actual player handoff, recovery/checkpoint continuation and first-person visual observation. Test variation in initial placement, timing and frame partitioning; test Android separately. A nominal run or restitution constant alone does not establish robust chain behaviour.
