# Gallery handoff diagnosis — 2026-10-15

Bounded read-only production diagnosis on `ChatGPT`, HEAD `1f92dca096c007e95681dc0f73f400a171c17f94`, preserving all dirty/concurrent work. Only this notes file and private diagnostic files were written. Root owns integration. No public-world replay or production fix was performed.

## Result and repair boundary

The existing physical gallery deck catch and top-out are feasible without geometry, forces, coefficients or clearance changes. In the outside-lip diagnostic, ordinary forward input acquires two actual hands on gallery1440's **5.9m deck** at tick1. A fresh public Action queued after observing that actual catch starts the finite transfer at tick2. Actual gallery footing, released hands and completed transfer occur at tick160, with zero rejections/deaths.

The smallest supported next correction is in `tests/forklift_route_walk.hpp::ascend_stack`: distinguish acquisition from top-out, retain forward intent until actual gallery hands are acquired, then neutralize and queue a fresh Action after the observed Hanging/Climbing state rather than consuming the only Action at a coordinate threshold. Preserve the actual receiver1440, hands-released grounding, 90tick footing, onward walk, grade exit and retry acceptance. Do not weaken production gates or alter gallery geometry from this evidence.

**Causal limit:** original world trial7 omitted traversal state/anchors/input transaction ticks. Its exact Action-time branch and original caught subshape remain unavailable. The supported driver defect is that it declares `action=true` at a coordinate threshold, neutralizes movement, and never issues a top-out Action after actual acquisition. The diagnostic proves the proposed acquisition-then-top-out transaction works at the same real deck; it does not prove the complete original public route or establish that its prior Action was definitely early. No first rejecting production branch was observed: the correctly ordered diagnostic passes every instrumented rejection branch.

## Baseline and rival causes

Root's actual world trial7 proves earlier real truck/cargo route and actual released-hand crate footing; gallery stalls at body(-100.074,4.61319,-91.5274), no support. Serious rivals were (1) only Action consumed before catch, (2) actual recessed beam/header catch with blocked standing landing, (3) valid deck catch rejected by retained lip/receiver revalidation. Cheapest discriminator: two bounded initial diagnostic contexts with actual state/hand/lip/target readbacks, fresh Action after actual catch, private logging at `begin_mantle_from_hang` and `begin_cargo_transfer` rejection branches.

1. Initial body(-100.074,4.61319,-91.5274), face(0,-1), forward(0,-.9), no later pose/velocity writes. No hands or traversal acquired through600ticks; falls onto real grade. This start cannot reconstruct original material catch, consistent with initial overlap/query visibility; does not diagnose old subshape.
2. Separate Simulation, initial body(-100.074,5.25,-90.8), identical ordinary intent. Tick1 actual Hanging(1),2hands, support1440; ledge(-100.073997,5.9000001,-91.6200027), hands x-99.8539963/-100.293999 at same5.9m lip; retained target(-100.073997,6.82000017,-91.9700012). Queue fresh Action and neutral at tick1 after observing catch. Tick2 Climbing(4), AwaitingFootContact(4), receiver1440, rejection0. Tick45 y5.91824 rising2.0496m/s; tick90 y6.86049; tick135 z-91.67095. Tick160 actual grounded1440, body(-100.073997,6.80863667,-91.9976654), traversalNone, hands0, foot load728.791N, completed transfers1, accepted1, rejected0, deaths0. Diagnostic stops on first footing; no90tick/onward/device acceptance. The final log's `tick=600` is a fixed label; its state is tick160 because the loop breaks at footing.

The deck lip/receiver is valid and unblocked in context2. Recessed-header geometry modification has no support from these measurements. The failing body's height alone did not identify the held surface.

## Authoritative owners

- `tests/forklift_route_walk.hpp:77`: current coordinate-only single Action and immediate neutral, no post-catch top-out transaction.
- `src/sim/simulation/locomotion.cpp:73`: Hanging requires a fresh Jump/Action for `begin_mantle_from_hang`. `:82` Climbing requires fresh Action for `try_top_out`; `:140` None/free-flight Action handles a distinct reach transaction.
- `src/sim/simulation/climb.cpp:151`: ordinary forward airborne catch; `:188` latches actual Hanging lip/receiver and acquires real hands.
- `src/sim/simulation/legacy_traversal.cpp:149`: standing capsule clearance, retained lip identity and finite transfer gates.
- `src/sim/simulation/hand_transfer.cpp:124`: actual reach, paired lip, receiver, shape clearance and affordance validation before changing hand topology.
- `src/sim/warehouse_forklift_yard.cpp:109`: gallery decktop5.9, underside5.66, frontz-91.5; physical recessed girder exists but was not the caught lip in context2.

## Reproducible artifacts and limits

Private workspace `/data/data/com.termux/files/usr/tmp/scraperx-forklift-gallery-diagnosis-20261015/` contains `probe.cpp`, `build.py`, `build-receipt.json`, private logging-only owner copies, `build-trace.py`, `trace-build-receipt.json`, `probe`, `probe-trace`, `window.json`, and both `window.trial-*.log`. Scoped persistent600second diagnostic window recorded both actual runs; no root world-window reset. Both executables exit0 as diagnostic processes; that does not declare world route acceptance. Context1 ran600ticks; context2 stopped at160.

Native archive SHA256 `1b99a1332e46e44ebee52b5b96ea8f35c8f139ff546418263c64f7e80d1214ae`; build receipt records actual archive/header/probe hashes and flags. Jolt archive is the assigned `scraperx-ci-soft-velocity-20261015/jolt-build/libJolt.a`. The traced executable overrides only copies of the two full owner translation units, adding tagged stderr logging at rejection branches; no tags fired in context2. Actual commands were run through `ci/mechanics_trial.py --state <private>/window.json run ... -- proot-distro login ubuntu -- <private>/probe` and `.../probe-trace 2`. Build uses `proot-distro login ubuntu -- python3 <private>/build.py` / `build-trace.py`.

Classification: existing public route failure captured; original Action branch unresolved because unavailable instrumentation. Feasibility and ordered input transaction confirmed in a focused native diagnostic. Full public replay, released-hand90tick holding, onward route and Android/device behavior remain unrun here.
