# Native engineering decision catalog

The [owner's complete directive](../../03_EXECUTION/ADVANCED_NATIVE_PHYSICS_ENGINEERING_DIRECTIVE.md) strengthens the [complete active goal](../../03_EXECUTION/ACTIVE_GOAL_PHASED_SCOPE.md). Neither this index nor the catalog replaces those requirements. The twelve examples below are decision resources, not twelve authorized simultaneous implementations.

## Baseline and evidence boundary

Inspected source: IssisX/ScraperX, exact `ChatGPT`, `acce9c9887963669017785dee6723b6799045dfd`; existing evidence changes and original Luna worktrees preserved. Native owner: `Simulation::PhysicsWorld`, dynamic 85 kg translation-only capsule, C++17, Jolt 5.6.0 at `e77f175595e64cb44218cc9d9d56fc365ad0e36a`, 90 Hz. This host uses Ubuntu GCC 15; Android CI specifies NDK 29.0.14206865, arm64-v8a/API24, Godot 4.7-stable. Godot owns input delivery and presentation. Headers were inspected locally at the pinned revision; no dependency upgrade was made.

Catalog classifications describe technique maturity, not shipping acceptance. Each example identifies its source owner, mathematics/units/assumptions, verified APIs, implementation approach, alternatives, expected behavior, failures, instrumentation/performance implications, and a rejecting test. Proposed tests and performance measurements are expressly unexecuted unless separate receipts establish otherwise.

| Domain/example | Chapter | Current decision |
| --- | --- | --- |
| A: Dynamic capsule and support-relative locomotion | [Contact/motion](advanced-catalog-contact-and-motion.md#a-preserve-moving-support-momentum-through-footing-and-departure) | Preserve controller and movement baselines; repair demonstrated discontinuities. |
| B: Articulated wheel/cart mass and inertia | [Contact/motion](advanced-catalog-contact-and-motion.md) | Preserve separate bodies and real material/player loading. |
| E: Reciprocal finite foot traction | [Contact/motion](advanced-catalog-contact-and-motion.md) | Actual normal-load traction remains unfinished; gravity proxy is a documented limit. |
| H: Float precision and 90 Hz interpolation | [Contact/motion](advanced-catalog-contact-and-motion.md) | Use actual geometry/solver proof; no precision migration without demonstrated need. |
| C: Real articulated/grip constraints | [Machinery/control](advanced-catalog-machinery-and-control.md) | Reuse existing native constraints and reciprocal reactions. |
| D: Bounded powered return versus hydraulics | [Machinery/control](advanced-catalog-machinery-and-control.md) | Preserve finite energy bank; full hydraulics lacks a gameplay need. |
| F: Loaded roller/rail contact | [Machinery/control](advanced-catalog-machinery-and-control.md) | Preserve existing real contacts; no vehicle/terrain rewrite. |
| G: Saturated joint and grip control | [Machinery/control](advanced-catalog-machinery-and-control.md) | Preserve bounded control; do not substitute slower mantle control for fast vaulting. |
| I: RAII/world lifetime and job scheduling | [Runtime/release](advanced-catalog-runtime-and-release.md) | Preserve ownership; worker-count changes require a measured bottleneck. |
| J: Native-driven tactile/audio/visual feedback | [Runtime/release](advanced-catalog-runtime-and-release.md) | Extend existing consumers only for completed physical behavior. |
| K: Complete checkpoint recovery | [Runtime/release](advanced-catalog-runtime-and-release.md) | Preserve custom gameplay/topology state; no rollback/network project. |
| L: Update failure and commercial delivery | [Runtime/release](advanced-catalog-runtime-and-release.md) | Ignored update-error return is a concrete reliability candidate; debug APK is not release certification. |

## Execution priority

1. **Close the current wheel encounter boundary.** High gameplay impact, low implementation risk: keep the raised service tray's deliberate jump/detour, honest early miss, stable exit and connected recovery. The recovery oracle must use existing native firm-footing/contact proof rather than an unrelated strict decimal sole-height cutoff. Collision settings, support geometry, jump baseline and physical forces remain unchanged. Native full route and the changed touch crossing are separate evidence boundaries.
2. **One remaining assisted fast-vault path.** High movement impact, medium/high regression risk: legacy `begin_vault`/`drive_traversal` still disable gravity and prescribe root velocity. First recover geometry/clearances and reproduce one real path, then replace it with a short finite support/hand push and momentum-preserving release. Reusing the existing slow settled mantle controller alone would lose the successful fast-vault behavior. Acceptance includes equivalent clearance/completion, real receiver reactions, bounded work, ordinary gravity after release, and blocked/missing-support failure. This is a proposal, not an implemented correction.
3. **Causal push-off positive work.** High correctness relevance, small bounded owner surface: causal support push-off uses signed net work while ordinary support push-off separately charges positive work through reversal. Capture a reachable actual contact case before changing budgets or behavior. Do not infer a player-visible defect solely from mathematical divergence.
4. **Jolt update-error reporting.** High release reliability, low gameplay risk: capture pinned `EPhysicsUpdateError` and define native observable failure handling without manufacturing a successful tick. Implement when it supports the next gameplay slice or a demonstrated capacity failure; not a new CI repair campaign.

Physical bracing/roll, actual normal-load traction, wider balance/stumble and world/visual work remain in the canonical scope. Advanced hydraulic pressure state, full anatomical rolling, MPC/LQR, custom solvers, large-world precision migration, networking and rollback are deferred proposals because no current evidence justifies their cost. Deferral of those techniques does not defer the corresponding required gameplay outcomes.

## What this catalog changes

Super_Agent's bounded second opinion (ChatGPT-login session `01a121df-7687-7b40-9f79-608b53326261`) confirms the push-off accounting distinction mathematically. For initial relative upward velocity `u < 0`, effective inverse mass `k`, and upward impulse `J`, net work is `uJ + kJ²/2`, but positive work is `k max(0, J + u/k)²/2`. Removed downward energy cannot fund the upward actuator. The proposed correction retains the separate existing budgets and reciprocal impulses. This is advisory/model evidence only: capture accepted contact with negative relative velocity before claiming a reachable runtime defect or implementing it. No new energy framework is warranted.

The engineering guidance is now preserved and searchable, with pinned-owner/API evidence. The catalog itself delivers no new physics behavior and earns no overall-goal completion credit. Completed gameplay, actual measurements, build results and phone observations must be reported separately. Use it at the next relevant ownership boundary; do not exhaustively implement or test its examples.
