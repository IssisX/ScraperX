# Finite powered cable — 2026-10-15

Implemented `src/sim/powered_cable.{hpp,cpp}` and focused native fixture `tests/powered_cable_tests.cpp` on ChatGPT, base8569c721432e4139df6286b8e8d1cce94b2db9ca. Existing/concurrent work preserved; no shared Simulation/Kit/CMake, forklift or grapple edits, commits, publication, or dependency rebuild.

## Contract and ownership

`PoweredCable(PhysicsSystem&, Config)` owns one private Jolt TwoBodyConstraint. Config supplies support/load BodyID, shape-origin-local support_eye/load_eye, initial/min/max_length_m, max_speed_mps (.35), pull_force_n/brake_force_n (50000), electrical_power_w (25000), efficiency (.8), capacity_j (5e6). Endpoints convert shape origin to COM-local once, rotate with each body, and use actual world COM/inverse inertia in the point Jacobian.

The host calls pre_step(effort) outside Update (positive retracts, negative pays out), collision_step(actual_collision_dt) in its serialized collision-step listener, then post_step() after the final solve. This class installs no listener. Create/destroy/restore only at legal owner boundaries; destroy the cable before either body or the PhysicsSystem. Commands wake both bodies outside Update. The row projects its cumulative reciprocal tensile impulse across native solver iterations, capped in total at force times the actual substep interval, with no manually duplicated reaction, position correction, body following or velocity assignment. Every signed correction separately admits its positive event work against the same substep reservation; dissipation never refunds that budget. Neutral/empty is a finite passive brake; slack has zero tensile force and never pushes. Finite brake overload permits separation drift rather than inventing a hard position correction.

State provides actual/rest length, actual endpoints, signed target separation speed, last tensile impulse/force and load force, peak force, source inventory/use, positive motor event work, absorbed work, conservative command work bound, heat, power, limits and cutoff. `restore(State)` requires a separately restored body snapshot, preserves finite inventory/target/cumulative accounts and clears the always-zero warm start/instantaneous drive. Native Jolt StateRecorder alone explicitly throws because it cannot silently omit the actuator inventory; use the public owner snapshot. Removing the cable does not edit body momentum.

## Finite work law

A retract command admits a rest-target shortening dx bounded by speed, range, available inventory, and electrical power. It reserves F_pull*dx mechanical J, charges that divided by efficiency from the source, and rounds target travel toward zero before debit. This deliberately conservative reservation charges even a stalled or slack commanded retraction; it does not claim ideal actual winch efficiency. The solved tensile impulse is capped at F*h and at the requested finite separation velocity. Rest-target motion has no spring storage or hard positional correction.

Actual event work uses the frozen point Jacobian and actual COM/angular effective inverse mass w: relative separation speed u becomes u-wJ. Work is split at zero relative speed into positive inward acceleration and absorbed outward energy. Incoming kinetic energy never replenishes the source. The motor cannot exceed its reserved F*dx positive work; neutral/payout/empty brake cannot reverse the relative motion into powered inward acceleration. Heat is reserved source use minus actual positive event work plus absorbed work; it includes conversion loss and unused conservative reservation. `energy_overdraft_j` remains zero because admission bounds source draw before solving.

Each impulse event uses the exact two-body point response; iterative re-solving transmits load through connected constraints. For dynamic pairs the native velocity-iteration override is derived from the actual pair masses: ceil(log(1e-4)/log(max(m1,m2)/(m1+m2))), bounded to10..255. The actual200kgspreader/3217kgtruck passive sling rows request153 iterations. There is no guessed hidden payload mass inside the hoist. Extremely ill-conditioned longer chains may need a separate supported convergence design beyond this bounded two-mass estimate. Obstruction checks belong to the crane owner using the reported real segment endpoints; no cable collision/wrap/obstruction-clear claim is made here.

## Build and historical direct-pair evidence

Private build directory: `/data/data/com.termux/files/usr/tmp/scraperx-crane-hoist-20261015`. Jolt was reused unchanged, SHA256 `8711a784f394fdf91af3366ab0b6695e80322cb73b46a8d1659e775b20aced1b`. The selected native archive was unnecessary: fixture uses raw actual Jolt bodies and no Kit/Simulation layouts.

Exact successful build command:

```sh
proot-distro login ubuntu -- /usr/bin/c++ -O3 -DNDEBUG -std=c++17 -fno-rtti -pthread -Wall -Wextra -Wpedantic -Werror -DSCRAPERX_HAS_JOLT=1 -DJPH_DEBUG_RENDERER -DJPH_OBJECT_STREAM -DJPH_PROFILE_ENABLED -I /data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/repo/src -isystem /data/data/com.termux/files/usr/tmp/scraperx-ci-soft-velocity-20261015/jolt-src /data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/repo/src/sim/powered_cable.cpp /data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/repo/tests/powered_cable_tests.cpp /data/data/com.termux/files/usr/tmp/scraperx-ci-soft-velocity-20261015/jolt-build/libJolt.a -o /data/data/com.termux/files/usr/tmp/scraperx-crane-hoist-20261015/powered_cable_tests
```

Build exit0, warnings treated as errors. Earlier environment/toolchain links failed: Termux libc++ cannot link the supplied Linux libstdc++ archive; Ubuntu with RTTI emitted undefined `typeinfo for JPH::TwoBodyConstraint`. The successful compile uses the matching Linux compiler and `-fno-rtti`; no engine change.

Persistent10minute experiment window: [hoist-window.json](hoist-window.json). Actual execution command (from repository):

```sh
python3 ci/mechanics_trial.py --state evidence/crane-2026-10-15/hoist-window.json run --stage rigid-cable-load --source src/sim/powered_cable.hpp --source src/sim/powered_cable.cpp --source tests/powered_cable_tests.cpp --source /data/data/com.termux/files/usr/tmp/scraperx-crane-hoist-20261015/powered_cable_tests --source /data/data/com.termux/files/usr/tmp/scraperx-ci-soft-velocity-20261015/jolt-build/libJolt.a -- proot-distro login ubuntu -- /data/data/com.termux/files/usr/tmp/scraperx-crane-hoist-20261015/powered_cable_tests
```

Trial1 exit1 on an unsupported98%-of-mgh predicate for *local instantaneous event work*. Actual3217kg load already rose1.739m; source work exceeded mgh. The supported cause was semi-implicit gravity/position integration at h=1/360, not uncharged motor work. For N substeps from rest, its independently derived injected mechanical energy is `N*.5*m*g*g*h*h + m*g*h*v_final`. Trial2 replaced the heuristic with a strict2J mechanical balance including that explicit term, final kinetic energy and actual absorbed work. Source>=mgh remains required. No force/speed/mass/geometry coefficients changed, no sweep or timebox reset.

Trial2 exit0, [actual log](hoist-window.trial-2.log):

-3217kg (3132kg truck plus85kg operator representative mass) rises1.739039302m in5s. Source109326.839447J; mgh54881.941349J; positive motor event work52899.583915J. Steady tension31558.759766N; peak49999.996094N.
- Gravity discretization term2180.609938J; energy closure residual-0.016028J. This term is identified explicitly, not called motor work or heat.
- Neutral holds within15mm with exactly unchanged source use; passive payout lowers actual load to0.858907461m, again no source draw. Cumulative heat after cycle86305.657840J.
- Slack empty source produces zero impulse/zero body travel in zero gravity. Taut empty source holds with brake and no positive motor work.
-10000kg overload drops2.411689520m in1s at49999.996094N, proving no upward rescue above finite force.
- Moving/rotating6000kg support plus3217kg load with(.4,0,.2)m eyes: linear momentum residual0N*s, actual tensile impulse138.888885498N*s, load eye angular impulse error0.000001617N*m*s. Support opposite eye torque and actual pair linear/angular kinetic event balance pass. The first impulse brakes the initially separating pair (positive motor work0), with absorbed work accounted.
- Release preserves actual linear/angular velocities. Restore retains finite source/target with zero warm impulse. One versus four actual collision substeps yields equal body motion and exact matching source/event-work totals.

Historical trial2 source/artifact hashes (superseded by trial4 below):

- hpp `566226c6b5b16df19e967fd336150cbc2964a41c4f971fec87fc782329b9f5cb`
- cpp `b003f7bb23faa99d13f1fc92e46f15d0c3d7ac53e87064a1419de22fadd3ef45`
- tests `53a5f7eb847de6e0510fdbc87a85c79cde1fa893156fe0564399c2206ae3ff19`
- binary `0fed7cbad92a99b76e80ed6cab3abd2f0424856a14c4fba629d58384ab661528`

These establish the primitive's isolated native behavior. Crane assembly stability, real forklift eye/rigging geometry, cargo contacts, exterior receiving dock/drive-off, integrated checkpoint behavior, obstruction, APK and device behavior remain root-owned and unverified here. The root's later lifting-eye steel changes its exact truck mass; this representative isolated test is not a claim of that final world mass or route completion. Trial4 below extends current acceptance through actual articulated spreader/slings.

## Articulated continuation causal decision

Root retained real spreader/slings scope. One-shot direct-pair evidence above is historical; the row is being extended to projected cumulative tensile lambda across solver iterations, with every signed impulse correction separately admitted against the same finite substep positive-work reservation (no dissipation refunds). Header/API unchanged.

Trial3 (same persistent window) lifts actual3217kg truck through200kg spreader and four empty-source tensile cable slings1.579907m/5s, while all direct tests pass. Source109326.84J, event positive work54454.54J. It fails the unchanged30mm neutral hold requirement. This is the second cumulative failure, so experiments stopped pending this recorded reassessment.

Supported convergence cause: Jolt default velocity iterations10, cold-start rows, spreader/truck mass ratio200:3217. The ideal vertical sequential impulse contraction per iteration is3217/3417; ten iterations retain about0.547 of the error. Predicted steady missed upward speed `g*h*r/(1-r)` is about.033m/s, consistent with measured(1.739039-1.579907)/5=.03183m/s lift deficit. A positional correction or unbudgeted warm impulse would conceal that supported residual. The next correction is a native velocity-iteration override derived from each dynamic pair mass ratio to a1e-4 contraction target, bounded by Jolt's255-step interface; source budgets, force, geometry and hold acceptance remain unchanged. Add explicit post-hold displacement/velocity trace. This is one supported solver-convergence correction, not a coefficient sweep or silent window reset.


## Final articulated implementation and acceptance

Trial4 PASS, exit0: [actual combined log](hoist-window.trial-4.log). The exact successful build and runner commands above were reused after the meaningful iterative/convergence change; runner stage is `articulated-spreader-cable`. Original trial2 direct evidence is historical; trial4 reruns those affected predicates once together with the new articulated slice. No API/header change was required. All builds remain warnings-as-errors with the pinned archive and no whole native/Jolt rebuild.

The iterative row clamps its cumulative lambda in0..F*h. A proposed signed change is recomputed from current actual relative point velocity after downstream rows solve, and its positive kinetic event work is capped against remaining per-substep mechanical reservation. The generic admission handles either sign of correction; absorbed work never replenishes it. Passive empty-source slings have zero positive-work admission. Native solver iteration count changes convergence, never force/work admission. No powered warm start or position correction is introduced.

Final actual observations:

-3217kg truck,200kg spreader, four real custom native tensile constraints at separate (.4,.2)m corner eyes, all body rotational DOFs free; source-free slings. Truck rises1.735601306m/5s. Source109326.839447J, actual positive event work57159.968389J, steady hoist force33521.992188N, sum sling tensions31559.948242N. Source exceeds total3417kg gravitational lift requirement.
- Neutral2s hold displacement+0.005757093m, final vertical velocity-0.000000165m/s; unchanged30mm requirement passes, exact source inventory/use unchanged. Every passive sling source use and positive event work remain zero.
- All original direct loaded/hold/lower, slack/empty, overload, moving rotating support reaction/torque/kinetic balance, release/restore and1-vs4substep predicates pass on this final source. Direct lift/work/gravity balance values remain those listed above. The actual max tensile bound remains49999.996094N.

Accepted final hashes:

- hpp `566226c6b5b16df19e967fd336150cbc2964a41c4f971fec87fc782329b9f5cb`
- cpp `3b69f4ecd6989e1eeb1b2b74a7a865bdab8d62aad730c1a2aee024ecf7e3b977`
- tests `4dab9ae60a5460dbd6280fc2f0219bb766ec5c2ffc62ca9e9205ee3fa4c96bc3`
- binary `8f1931c179080ac179cde8fd98c4fdb61e677ba8612546c6d2cbae8281b6089f`

Persistent window records both actual failures, passing historical/direct and final combined trials, and explicit causal reassessment. The exact documentation bytes used as first reassessment evidence before this final update are preserved in `hoist-window.reassessment-1.log` (its SHA matches the historical reassessment's evidence SHA). That historical record refers to the then-current documentation path, which has now been updated to describe the final implementation. No silent window reset or failure deletion occurred.

Current acceptance is isolated actual native mechanics through the spreader/slings. Full crane world support/clearance, final truck mass/geometry, actual dock release/drive-off, APK and device behavior remain root-owned. The component intentionally rejects native-only StateRecorder snapshots; use owner State plus separately restored body snapshot. Finite velocity-only rows can retain a geometric stretch under overload; there is no hidden positional rescue. Higher solver iterations have a real performance cost on the connected island; no world frame-time claim was measured here.
