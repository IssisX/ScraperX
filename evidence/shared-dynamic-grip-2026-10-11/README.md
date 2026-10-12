# Lightweight dynamic rigid grips — 2026-10-11

The bounded lightweight rigid-grip integration is verified. Rejecting old-source
acquisition and obstructed-fallback cases, focused new-source behavior, maintained
simulation and exact-bridge headless touch are recorded below.

The previous `find_grip` admitted dynamic rigid bodies only above400kg or through
identity exceptions. The finite hand backend already accepts lighter supports
and applies reciprocal bounded forces. Discovery now uses physical grip geometry,
unobstructed approach and actual hand ownership; the real mass, joints and
contacts determine the support response.

Rigid candidate visibility uses actual player COM plus the requested hand aim's
height/lateral offset. Planned regrip centres cannot place that ray beyond a wall.
The first solid hit must match the target body and leaf subshape; sensors/player
are ignored, while real soft bodies and carried objects remain occluders. Only
the query extends by the existing landing skin. Actual points, search/forward
bounds, support dynamics, finite forces/work and gravity are unchanged.
Supported timber cell geometry and the CargoNet material-grip adapter remain
separate existing contracts. This slice does not claim universal shapes or new
material-net grip visibility.

## Rejecting exact-source baseline

Baseline HEAD is `790c25b2942629baec1d0c5d87f706e5fd572924`, native Simulation
SHA256 `fa9cda0e37720b713a61ac8af1544af6ba6562f5f9aeb86ec3e28f66160e5f60`,
archive `e2ee438aae4bc76ef8a4477c8deecef1227aa08d1d196e24550b149382f02196`.
Apply `baseline-test.patch` to reconstruct test source
`e6d38f917f38861eb4c0f0f2b2d5ea4bc52b64fe7e89d42b547bc073a1255f32`.
Compilation with unchanged warning checks passed; actual execution exited1.

The existing normal PipeBridge world's hinged UpperAscent arm2702 reports actual
mass40kg, position `(4,46.5,-114)`, identity rotation and arm half-extents
`(.75,.05,.05)` at local offset `(-.75,0,0)`. Initial-only player placement
`(3,45.8,-113.3)`, facing−Z and ordinary movement−0.35 acquired no hands in30ticks.
The existing search overlaps its grip geometry, and the old mass gate necessarily
excludes it. Other geometry is not independently excluded as a possible cause
until the new-source runtime succeeds. No new geometry or subsequent player
position/velocity writes are used.

The closed-door negative runs first in explicit RegressionFixtures: actual
grounded stance `(10.525,.9,-88.4)`, closed door54 at angle0.0352769rad, bar55
inside the existing hand search, then one ordinary Action. Its affordance and
actual hands remain unavailable. Action queries holds even on ground, so this
test does not merely exercise the airborne auto-catch ground refusal.

## Verified implementation and proof

Super_Agent MCP sessions `01a12d53-612e-7fc1-8667-d82652e39665` and
`01a12d5d-5ab1-71c3-864b-c3ed768de1d4` changed only Simulation.cpp.
Root integrated the initial compatible-pair guard and inspected actual changes;
independent High review found no remaining blocker in this slice. Final native
SHA256 `23e535f4d23800b3275204c930d932cfb7601492b08f2ea80b34a356f4d12fb9`;
maintained supplied-machine test SHA256
`8ce62a00db0a029055c847c8e7bffc53b1b2c6899657351689cf29de3d4fe9cd`.

Actual arm2702 acquisition occurs at tick1 with two finite hands, airborne
Climbing and gravity1. Neutral hanging produces mean upward hand force176.765N;
the lever differs from the same-initial-state no-approach control by0.447626m/s
and0.397452rad. This response includes all real contacts and hands; it does not
claim isolated hand-only momentum closure. Ordinary Drop removes both hands
and retains momentum with the expected gravity step.

The meaningful closed-door55 negative passes. A separately rejecting right-jamb
case first requires the centre seed to be available: the earlier implementation
then incorrectly attached two hidden fallback hands. Final source checks each
fallback hand's own physical approach and attaches none. The current two-hand
controller rejects an incomplete initial pair before reading paired averages.
Generalized one-hand control and material-net approach visibility remain open.
`fallback-baseline-test.patch` and `fallback-red-receipt.json` bind the rejecting
source and actual runtime; nominal hand-ray geometry is not a ray-hit readback.

`native-final-receipt.json` records passing focused grip/ledge/physical-traversal
checks. `simulation-final-receipt.json` records the maintained simulation suite
passing on final native source. `touch-final-receipt.json` records the full
headless touch hanging-bar receiver/miss/recovery/retry/onward encounter passing
on exact bridge SHA256
`41e8b420a6848c6996993cb4743239e3711609ab45ede7e199565fc7f305d524`.
These are local native/headless results, not Android or phone observations.

## Existing plank failure and next goal

The existing deformable-plank suite fails its airborne hanging-load case: catch
at586 loses both hands at619, before its required270-tick sustained hang. The
unchanged test SHA256 is
`880d51943d47f04ffd4b9e96c6c4df9d7bb2746f8f060e84a6f54fcaf18eb954`.
Its final-source log is byte-identical to execution against the saved790c25b
native archive. `plank-existing-baseline-receipt.json`, `plank-existing-red.log`
and `plank-current-red.log` establish that this failure predates this dynamic
slice; no plank completion credit is claimed. Its passive coupling is the next
bounded movement goal. Full movement, articulated limbs, universal shape policy,
phone feel and fullGREEN/APK delivery remain unfinished.
