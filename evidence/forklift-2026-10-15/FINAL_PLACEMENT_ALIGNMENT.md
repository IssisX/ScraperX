# Final placement steering correction

Selected-source world trial8 fails the unchanged offset-stack acceptance: upper crate centerX -99.7993 versus target -100, after the loaded turn ends with residual forward-axisX and drive_straight_to commands zero steering. TruckX drifts from -100.029 to -99.9299 during the final approach. Actual cargo load transfer itself passes. This is measured driver misalignment, not evidence of a physics-owner failure.

The final forward placement command now uses actual cargo/receiver lateral position plus chassis heading and actual forward reach to choose finite rear-steer input. Existing steering rate, angle, traction and energy apply. All force/geometry/placement thresholds remain unchanged. Earlier accepted drives are untouched. No cargo transform, latch or fictitious support is introduced.

Only tests/warehouse_forklift_route_tests.cpp changed. Selected native archive ca8ce5fb26f35f16cb38f523ff115357396488646c3e22cff04bb0db3e176189 and matching Main19 results remain reusable. Strict route-only recompile/relink passed; route source e9f371cad33f8a133252fa87bf7a3cee8bac9ee5009f4db27f45f63268393f89 and binary db30cfea14bbb0214dff59c6f5f591d7d08d41c7a55ef8e226df409c279eb3f1.

Reassess the same persistent world window because the remaining time is shorter than the measured269s connected replay, following this supported driver correction. Preserve trial8 as FAIL and all earlier failures. The next replay must still prove actual independent cargo, released-hand player footing, gallery transfer, onward movement and grade retry; no pass is claimed here.
