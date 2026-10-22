# Matron routecraft checkpoint

Status: implementation checkpoint, final verification pending. Not integration-ready yet.

Base: `e098f5897066497700b7c93a2c8c2b6dc7b3314f`.
Branch: `dot/matron-routecraft-20261022`.

Native source: `src/sim/matron_routecraft.hpp` and `.cpp`. Entry points:
`Simulation::request_matron_climb_goal(Vector3 goal, Vector3 facing)`,
`cancel_matron_climb_goal()`, `matron_climb_status()`;
`Matron::request_climb_goal`, `cancel_climb_goal`, `climb_report`.
The goal is capsule center position. Facing is the horizontal approach direction
into the supports. It selects an approach side, not authored coordinates or a route.

The controller discovers static horizontal graspable box members using existing
`detail::leaf_box` and `detail::box_is_hold` geometry rules, searches bounded body/contact
states with capsule sweeps and sampled hand visibility, and applies finite forces
at native 90 Hz with gravity enabled. It does not command body transforms or velocities.
Static support reactions are recorded at the actual contact; static world geometry
has no dynamic momentum to change. One aggregate grip supports the existing upright
translation-only capsule. No moving supports, anatomical balance, neural training,
or automatic chase adoption are claimed.

Physical focused runs before the latest lifecycle fixes passed production ladder
ascent/mantle/descent, full-tower ascent/descent, lateral transfer, descent before
ascent, repeated deterministic replay, removed grips, new obstruction replanning,
release/recovery, and measured 9000 W saturation. Final focused rerun and complete
suite result are pending. Existing player climbing selector passed with exit 0;
existing capture/fatality selector passed with exit 0. A full suite is running.

Independent review prompted fixes for stale compound subshape decoding, bounded
query collectors, failed approach retry handling, actual-position transfer checks,
impact sensing, transactional goal replacement, immediate capture cleanup, and
fall-reference ownership after a supported descent. Latest lifecycle fixes are
not yet verified at this checkpoint.

Search limitation: no-route is a result in the supported sampled posture family,
not an exhaustive impossibility proof. Three vertical postures and at most five
samples along each member can miss physically possible routes. No Android timing
or device behavior has been measured.
