# Portable return rail diagnosis — 2026-10-15

Read-only production investigation; only this note is owned. Assignment: Sol/medium. Checkout branch ChatGPT, observed HEAD dc53244d99a833888f1004af1b74a392887b34d5; supplied CI baseline f6ac5c4/run38733308608. Root's concurrent modularization and all existing dirty/untracked work preserved. No build, runtime experiment, production edit, commit, or push performed.

## Captured failure and scope

Saved log `/data/data/com.termux/files/usr/tmp/scraperx-refactor-20261015/baseline-ci-failure.log`, lines1000–1163 (particularly1152–1155): `scraperx_sim.grapple_portable_rail` fails after31.23s with `FAIL GRAPPLE_ROUTE phase=portable_rail_launch reason=actual square return rail capture/dwell`.

At tick2405 before second shot: player(19.600803,485.165955,-135.305130), grounded on receiver1968, near-zero velocity; released first line, cartridges3. At failure tick2946: grounded receiver1968, PayingOut state1, failureNone0, cartridges2, attached line (independently required every loop tick), line10.117993m, tension0; hook eye(16.532957,490.873108,-135.298355), aperture/crook center in cup coordinates(.606652,.584590,.189547). Intended return rail center local(.6,.6,.19), so aperture is nearby, but geometric retention never stays true45 consecutive ticks within540ticks. This is a capture/dwell failure before lock, reel, load, release, or return acceptance. It is not stock refusal, muzzle refusal, rope obstruction, death, or loss of receiving footing.

The same CI run passes primary practice route, independent407, recovery and gun units. Root separately supplied matching baseline/refactor primary+independent407 passes; do not duplicate these accepted runs.

## Authoritative path

- `CMakeLists.txt:189`: real public route executable argument `portable-rail`.
- `tests/grapple_practice_route_tests.cpp:470–501`, `portable_return_rail`: ordinary walk onto receiver, native aim/Action, actual square capture45ticks, passive180ticks, finite reel/airborne load, release/receiving support.
- `tests/grapple_practice_route_tests.cpp:461`, `return_rail_retained`: actual active hook/body rotations, square corners and finite rail length, through `tests/grappling_retention_observation.hpp::retained_square_in_crook`. Every square corner must remain inside every finger face and inward of actual open tips. Existing solver slop only applies to closed faces; mouth escape has no tolerance. Preserve these truths.
- `src/sim/simulation/grapple.cpp::apply_grapple_commands` routes ordinary aim/Action to the existing native gun; distant released first hook permits second finite shot. Current line/stock evidence proves that path ran.
- `src/sim/grappling_gun.cpp::GrapplingGun::fire` (383+), `Impl::Impl` (340+): actual compound convex steel, density7850, geometry-derived COM/inertia, yaw/pitch initial launch, reciprocal180J impulse, actual free angular dynamics/contact, finite rope. `src/sim/grappling_gun.hpp::launch_origin`: .35m up and .85m along aim; no fixed hook/anchor.
- `src/sim/grapple_practice_route.cpp:158–164`: real50mm square return leg on cup1966, world authored(14.6,491.1,-135.85)..(14.6,491.1,-134.77), translated+2X. Cup body origin(16,490.5,-135.5), so observed local center(.6,.6,.19), half-length.54 is correct. Actual weld/support chain remains.

## Cheapest source discrimination completed

Archived accepted fixture `evidence/grappling-gun-2026-10-14/portable-rail/final-portable-rail.log` proves prior PASS3394,403.455770J and22 consecutive airborne-loaded ticks on archive96b121638ac05c9e0d8af846a5b033a90306a69b92c8b60d4678d93a990e1508. Its frozen `final-*` source and receipt are present.

Diffs establish current versus frozen successful source:

- `portable_return_rail` and `return_rail_retained` unchanged; added independent407 functions do not alter them.
- Full practice route geometry unchanged (empty diff).
- Full retention observation header unchanged (empty diff).
- Gun implementation differs only by equivalent extraction of muzzle helper expressions (.35up/.85along aim); yaw-covariant launch already existed in accepted rail source.
- Actual hook geometry materially changed:47.1437mm stems became175mm, eye moved downward, finger thickness redistributed to retain actual2kg steel. Design evidence `worldwide/physical-hook/design/RESULT.md` gives COMY.108595→.068195m and substantial inertia changes. Current gun cooks these real polygons into Jolt; this is consequential physics, not presentation.

Do not diagnose `.95` in ballistic aim as a stale .85m barrel constant: launch body position advances .85m, crook center adds rotated localY.10m, giving initial aperture .95m along aim. Same solver was present in the old passing rail test and the current passing primary/407 routes. No canonical-control mismatch is supported by current evidence.

## Supported conclusion and remaining rivals

Strongest supported localization: formerly accepted reverse square-rail capture regressed across changed physical hook mass distribution/contact dynamics; test controls, receiver geometry and geometric assertion remained unchanged. This is pre-existing to root's modularization. Source comparison alone does not prove the exact contact failure mechanism or rule out other changed world/contact owners.

Serious remaining rivals that change the repair:

1. Actual capture/settling changed under new COM/inertia/eye (perch, contact spin, or transient catch/escape).
2. Hook is collision-supported but square orientation/finite corner geometry violates the true capture predicate (nearby aperture alone is insufficient).
3. Earlier world contact/launch trajectory differs enough that the new hook never enters the rail correctly; initial state differs only slightly from archived pass, but absence of contact trace prevents exclusion.

Saved log has no second-shot rotation, velocity, contact/subshape, maximum dwell, or predicate reject component. Final aperture offset alone cannot discriminate these; do not claim a specific roll/perch, air-control defect, or narrow-phase defect as proved.

## Next sufficient observation; repair boundary

Propose one root-authorized isolated replay of unchanged public `portable-rail`, retaining all production owners and acceptance. Add observation only for second shot: launch aim/actual COM+pose, first actual hook/rail contact and subshape, hook relative pose/linear/angular velocity on descending rail approach, maximum consecutive capture dwell, and the first failing retention component (finite end, actual mouth, or closed face) at near-rest/contact. This separates trajectory miss, perch/rotation/escape, and actual geometric mismatch. No parameter/geometry sweep needed. Root's existing isolated public runner can relink this observer; no shared imports/builds were touched here.

Smallest supported repair surface is not yet selected: if ordinary aim does not deliver the actual cavity into the rail, correct only route control calculation; if physical capture dynamics reject the geometry, repair the real hook/receiver/contact owner after the above discriminator, preserving actual steel2kg/inertia, finite forces/180J, honest contacts, real rope obstruction, stock and anchors. Never weaken the square predicate/dwell/load assertions or add a latch/pose/orientation authority. Re-run the same portable rail path after an actual repair; affected existing hook routes require acceptance only if their shared physical owner changes.
