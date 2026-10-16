# Same-truck freight crane — historical topology preparation

Historical design record. Current implementation and acceptance frontier: LIVE_RESUME_20261015.md and FORKLIFT_AND_CRANE_ROUTE_SYSTEM_20261015.md. Draft masses and proposed geometry below are not current measured receipts.

Basis: read-only Sol/high architecture worker on published8569c721432e4139df6286b8e8d1cce94b2db9ca. Root accepts this as next-feature preparation, **not engineered or runtime-accepted machinery**. Current APK source stays fixed. Owner explicitly authorizes a powered huge yard crane; no gravity-only restriction is inferred from generic skills.

## Selected topology and real world constraints

One proposed36m yard jib crane,35m working radius, serving staggered outboard receiving docks. Actual ring source: `src/sim/simulation/fixture_constants.hpp:254`, `world_geometry.cpp:109`. Square52m rings center(0,-150), outerx±26,z[-176,-124],9m perimeter bands, tops11/22m. The deck above prevents directly lowering a suspended truck into an interior ring.

Proposed, not existing: mast(-60,0,-150), sheave elevation36m; pickup pad(-80,0,-150). Lower dockx[-36,-25.5],z[-144,-136],top11; upper sameX,z[-164,-156],top22. Landing truck centerX-31 with local+Z toward world+X. Load stays outboard until supported; staggeredZ keeps the upper dock out of the lower hoist shaft. Dock radius sqrt(29²+10²)=30.68m, pickup radius20m [DERIVED from CHOSEN coordinates].

Worker's exported-box AABB inspection found no boxes intersecting proposed mast/pickup or grade corridorx[-104,-76],z[-155,-100],y[0,4]. This excludes native Kit/hulls and establishes no swept clearance. Actual truck approximately1.56m wide,5.74m long,3.65m tall. Existing galleryx[-103.4,-96.6],z[-96.5,-91.5] requires departure around actual legs. West-face braces in `world_solids.inc:717,725,726,734` leave marginal mast clearance: next-story brace center~4.23m above receiving deck. **First unresolved observation:** sweep actual neutral-mast truck shapes through both entrances against every production solid/hull/Kit obstacle. Reject coordinates if blocked; preserve braces/accepted routes.

## Native ownership and finite work

New separate `YardCrane` owner in yard_crane.hpp/.cpp; Kit remains body/mass/COM owner. Dynamic mast attaches to visible concrete via a rated foundation connection; boom/counterweight on vertical HingeConstraint, trolley on two-body SliderConstraint, tensile powered cable to actual spreader, four removable slings to real chassis lifting eyes. Visible braced receivers tie into the existing frame. Counterweight, bearings, foundation and rig geometry/rating are not yet sized.

Reuse `Kit::add_body`, `body_id`, distributed mass/COM, CCD and pair ownership (`mechanism_kit.hpp:127–147,318–324`), hinge/breakable-weld patterns. `Kit::add_tie` tensile DistanceConstraint pattern (`mechanism_kit.cpp:424`) lacks removable lifecycle: crane owns removable sling references. `Kit::add_rope` is immutable-length/world-fixed-sheave, so it cannot model a moving trolley without changed ownership. Private GrappleRope is player-COM specialized: its finite tensile-row pattern is reference only. Do not modify the accepted gun owner just to share this application.

A crane cable must use both real rotating body-local eyes, reciprocal angular effective mass, finite strength, slack and obstruction. No hidden orientation restraint or double reaction. Reuse ServiceLift per-collision reserve/account seam (`service_lift.cpp:135,226`) with finite passive braking on neutral/cutoff; slew/trolley share the finite energy source and take real reactions.

## Quantities and limits

Authored truck3132kg [MEASURED source:2800chassis+220mast+40carriage+72forks]; weight30.73kN and ideal ground→11/22m work338/676kJ [DERIVED at9.81m/s²]. Player85kg [MEASURED source constants]. Rig200kg is a **CHOSEN estimate pending geometry-derived mass/inertia**, not an established rig. With both, total3417kg, weight33.52kN, ideal lift369/737kJ and load moment1.03MN·m at30.68m [DERIVED, excludes boom/counterweight dynamics].

Proposed starting design envelope [CHOSEN, untested]:50kN pull,0.35m/s,25kW electrical,80%efficiency,5MJ finite reserve. Actual force/power sharing, discrete motor work, cable strength, braking/slack dynamics, bearing/counterweight/foundation loads require the production-owner experiment. These numbers are not runtime claims or invitation to coefficient sweeps.

## Player transaction and first useful implementation

Drive the same truck to pickup, connect actual aligned slow sling hardware without snapping and retain BodyIDs, battery and fork coupling. A supported rear-platform operator uses a pendant, contributing their actual weight. Forklift drive neutralizes while rigged. Hoist/slew/trolley onto outboard dock; actual wheel support and unloaded/slack slings precede pin removal. Preserve velocity; drive inward to actual ring footing. Reverse transaction is recovery; explicit retry restores Kit/forklift/crane/joint/energy states together.

After envelope clearance, first native production-owner slice is full-radius crane plus real truck:1m hoist, short loaded slew/trolley, actual landing and release. Falsifiers: absent reciprocal force/torque, cable compression, excess source work/power, hidden yaw/upright lock, unsupported release or changed truck identity. Run one bounded mechanics experiment before whole-world construction. This is implementation of a new owner, not retesting accepted forklift movement.

Rejected shortcut: weld truck to trolley/fork coupler and animate height. It bypasses suspension, rigging, support and release. Higher freight beyond11/22m remains staged later; this single crane is not assumed to cover1600m. Public modular exchange/claw/clamp and all parkour/canonical tasks remain in the owner register.
