# Work Order 003 — Athletic Traversal

## Objective

Give the native player controller bounded mantle, vault, and ledge-hang primitives that act on **real collision geometry** and preserve moving-support truth:

`facing/move/action intent → native geometry probe → validated traversal commitment → constrained native trajectory in the support body's frame → inherited support motion on completion → Godot render mirror`

This work order ends at those three primitives. It does not add the fall/parachute/checkpoint loop, freight machinery, structural failure, or process simulation.

## Existing proven truth

At source commit `ce8c930c7e92bff192942c01c351b49f6dd29257`:

- the Godot 4.7 host loads the current-source C++ GDExtension;
- `scraperx_sim` advances at the authoritative 90 Hz fixed step and owns the player rigid body;
- native contact establishes support identity for a static deck (entity 1), a translating kinematic support (entity 3), and a rotating kinematic support (entity 4);
- grounded locomotion is support-relative and a jump inherits `v_point = v_linear + ω × r`;
- host tests, Godot runtime capture, Android arm64 cross-compilation, and APK export are proven by ScraperX-2 checkpoint CI;
- no mantle, vault, ledge grab, or hang state exists;
- APK installation, Android execution, Fold 6 observation, and sustained frame rate remain unproven.

## Governing authority

- Governing Law 4 — athletic industrial parkour is foundational; assistance may not fabricate support, teleport the player, erase meaningful motion, bypass geometry, or convert invalid traversal into valid traversal.
- Governing Law 5 — moving supports preserve motion.
- Governing Law 17 — physical sequence breaking is valid play; a validated physical route is accepted.
- Governing Laws 22–23 — one owner per consequential fact; authority boundaries precede integration.
- Governing Law 26 — no physics theater; animation may not own machine or traversal success.
- Governing Law 27 — contextual Action is a gateway, not a solve button.
- Governing Laws 28–29 — Fold-class Android is shipping truth; delivery claims remain distinct.
- GDD §7.2 — vaulting/mantling, ledge grabbing and hanging are required movement where physically valid.
- GDD §7.3 — traversal readability comes from world geometry, not painted routes.
- TDD §4 — authoritative fixed-step ordering.
- TDD §8.3 — the native controller owns climb/hang/mantle state.
- TDD §8.5 — traversal assistance executes a constrained trajectory only when collision/support geometry exists, reach/clearance rules pass, the target state is physically valid, the operation does not teleport through blockers, and relevant platform motion is included. The assist is a controller, not a route flag.
- TDD §24 — WO-003 adds bounded mantle/vault/ledge/hang primitives on real geometry, preserving moving-support behavior.

## Owner

`scraperx_sim` owns the traversal state machine, the geometry probe, the committed trajectory, traversal support identity, and the velocity handed back on completion or abort.

Native Jolt owns the collision geometry queried by the probe and continues to resolve contact during every traversal tick.

Godot owns input (move, facing, action, release) and presentation only. It may render native traversal state and the native ledge affordance; it may not decide that a traversal is possible, move the player along a traversal path, or parent the player to a ledge.

## Allowed seam

Extend the existing native `PhysicsWorld`, the immutable `Snapshot`, the thin `ScraperXSimulation` GDExtension bridge, the current Godot presentation mirror, and the current host/runtime/Android verification workflow.

The probe must use the same Jolt world the player collides against, through `NarrowPhaseQuery` ray casts and a capsule `CollideShape` clearance test. Traversal targets are stored in the support body's local frame so a moving support carries the hold and the landing.

## Forbidden shortcuts

- A traversal that is granted by a trigger volume, tag, route flag, distance check, or authored marker instead of a collision query against real geometry.
- Setting the player position directly, or any trajectory that is not resolved by Jolt each tick.
- A trajectory that continues after real geometry has blocked it.
- Ledge hang implemented by Godot node parenting, animation, or a presentation-side transform.
- A hang or mantle on a moving support that drops the support's motion, or a completion that injects a fabricated launch velocity instead of the support's actual point velocity.
- A vault that erases the player's approach momentum, or that teleports across the obstacle.
- Clearance "checked" by a constant instead of by a real capsule query at the actual landing pose.
- Starting WO-004 fall/parachute/checkpoint work inside this work order.

## Implementation scope

- Native traversal geometry probe and state machine in `src/sim/`.
- Native traversal test features in the same Jolt world: vault rail, mantle ledge, hang ledge, a kinematic moving ledge, and a clearance-blocked ledge.
- New authoritative snapshot fields for traversal state, traversal support identity, ledge affordance, progress, and traversal accept/reject counters.
- New bridge commands `set_facing`, `request_traversal`, `request_release`, and `configure_initial_spawn`, plus the matching snapshot accessors.
- Godot presentation: matching tower geometry, traversal telemetry, touch/desktop action and release inputs, and a CI sequence that walks to a real ledge and mantles it.
- Host tests, Linux Godot runtime capture, Android arm64 build, APK export, and bounded CI evidence.

## Out of scope

- Fall severity, parachute, checkpoint commit, and death rollback.
- Freight machinery, load-bearing structure, rigging, and process/isolation systems.
- Climbing sustained vertical surfaces, balancing, crouching, and sprinting.
- Destruction, persistence, streaming, NPCs, missions, and final art.

## Proof path

1. Host tests retain every WO-001 and WO-002 invariant: fixed-step partitioning, invalid-input rejection, static support, static locomotion, translating-support relative locomotion, inherited jump momentum, and rotating-support `v_point = v_linear + ω × r`.
2. Host tests prove a vault over a real rail crosses the obstacle, lands on the far side, and retains material approach momentum.
3. Host tests prove a mantle from the deck onto a real ledge ends grounded on that ledge's stable entity ID at the ledge contact height.
4. Host tests prove a fall beside a real high ledge produces a native hang on that ledge's entity, that the hang holds against gravity without drifting, and that a jump from the hang mantles onto that same ledge.
5. Host tests prove a hang on the **kinematic moving ledge** is carried by the support: the hold stays fixed in the support's own frame, player world velocity tracks the support point velocity, the mantle that follows lands the player grounded on that moving support, and a deliberate release hands back the support point velocity before gravity resumes.
6. Host tests prove the clearance-blocked ledge is **rejected**: the native probe does not offer it, the authoritative reject counter increments when the action is issued anyway, no traversal starts, and the player does not rise. Commands are queued and resolved on the authoritative tick, so the refusal is recorded in native state rather than returned from the request call.
7. Host tests prove traversal remains fixed-step-owned: the same command stream produces the same result whether the frame is delivered in one batch or in single fixed steps.
8. Godot 4.7 loads the same native extension, walks the player to a real ledge using native ledge-affordance state, issues the contextual action, and logs a completed native mantle onto that ledge entity; one first-person frame is captured from that runtime.
9. The same source cross-compiles for Android arm64 and exports an APK containing the native library.

## Completion

WO-003 is complete only when all proof steps above pass from one source commit.

That proves: **implemented + host-built/tested + desktop Godot-executed/observed + Android arm64 built + APK produced**.

It does not prove: **APK installed, Android executed, observed on Fold 6, sustained 45 FPS, thermals, or touch ergonomics**. Those claims remain separate.

## Result record

- **Changed:** `src/sim/simulation.{hpp,cpp}` (traversal state machine, geometry probes, five new native world bodies, new snapshot fields), `src/bridge/scraperx_simulation.{hpp,cpp}` (facing/action/release/spawn commands and traversal accessors), `tests/simulation_tests.cpp` (WO-003 suite), `godot/presentation/main.gd` and `godot/main.tscn` (traversal telemetry, contextual action and release input, native-authoritative geometry mirrored exactly, set dressing visually demoted), `CMakeLists.txt` (test name), `.github/workflows/wo000-delivery-spine.yml` (WO-003 proof path), `README.md`.
- **Built:** host `scraperx_sim` + `scraperx_sim_tests` and the Linux `scraperx_native` GDExtension, Release, GCC 13.3, `-Wall -Wextra -Wpedantic -Werror`, pinned Jolt `e77f175` and godot-cpp `507ed9d`.
- **Executed:** `ctest --test-dir build/bridge` → `100% tests passed, 0 tests failed out of 1`; `tools/godot/godot --path godot --rendering-method gl_compatibility` under Xvfb with `--ci`.
- **Observed:** native suite printed `vault_x=6.08 vault_speed=5.5 mantle_support=6 mantle_y=2.46879 hang_support=7 moving_hang_support=8 moving_hang_vz=0.875645 moving_ledge_vz=0.865345 rejected=1 accepted=1`. The Godot runtime loaded the current-source extension, jumped from translating support entity 3 with `inherited=1`, walked to the real ledge, and printed `SCRAPERX_WO003_RUNTIME_PROOF ... mantle_support=6 position=(9.470,2.450,-6.269) grounded=1 accepted=1 refused=0 aborted=0 ledge_point=(9.120,1.550,-6.269)`, capturing one first-person frame.
- **Unverified boundary:** Android arm64 cross-compilation and APK export were not run on this machine and are claimed only by CI; APK installation, Android execution, Fold 6 observation, touch ergonomics, sustained frame rate, thermals, and every WO-004+ system remain unproven.
- **Regressions:** none observed. Every WO-001 and WO-002 host assertion is retained and passing.
