# Receiver surface patch implementation — 2026-10-15

Base: `74d9c9320c46148df03af9507c77605564eb1f12`, branch `ChatGPT`, IssisX/ScraperX.

Parent-supplied causal evidence: unchanged mantle-feel cabinet transfer has fresh actual grounded bottom contact with matching body/entity/root shape/friction; only planned leaf `0xfffffffe` versus actual `0xffffffff` differs. The structural top and Hazard cover are coincident children of one body. No geometry, actuator, friction, work, timing, or contact freshness limit was changed.

The prospective receiver now stores at most eight exact deduplicated leaf IDs. The original canonical ID remains mandatory. Collection recasts the original accepted ray against only its exact body using a stack collector, and admits actual hits only when their world normal and plane coincide within four float epsilons, scaled by world-coordinate/ray/body extent for plane reconstruction. Collection rejects capacity overflow and invalid bodies. It does not create a physical contact. Existing candidates with an empty explicit patch become canonical singletons.

The cache preserves age/entity/body-sequence/root-shape/geometry-generation invalidations and validates every patch leaf. The plan requires canonical membership plus exact actual-contact membership, and retains solved-tick, body, root-shape, entity and friction checks. The listener copies the patch before Update and selects only real solved bottom contacts from its members. Foot acquisition keeps its actual point/material/footprint/force owner unchanged.

API: `collect_receiver_surface_patch(const JPH::PhysicsSystem&, const JPH::RRayCast&, const JPH::RayCastResult&, ReceiverSurfacePatch&) noexcept`; `PlayerContactListener::set_transfer_receiver(JPH::BodyID, const ReceiverSurfacePatch&)`. Parent owns tick call integration and CMake addition of `src/sim/traversal_receiver.cpp`.

Regressions added: real Jolt compound query admits structural top plus coplanar cover; excludes raised/neighboring/sloped faces; downstream contact acceptance rejects unlisted/wrong-body/stale contacts; empty candidates retain singleton behavior; query overflow and invalid bodies fail; explicit patch missing canonical ID and patch overflow cannot enter cache. Existing cache identity and age negatives retained.

Checks actually run by this worker: `git diff --check` PASS. No compiler, tests, runtime, import or shared build was run, per parent assignment. Parent owns combined frozen-source validation. Runtime repair is not claimed until those checks pass.

## Implementation source SHA-256

- `src/sim/traversal_affordance.hpp`: `45f839f4f1a53e2a7843ac42bc6232924e4c7f2e00377894f6ff1601bf72c8df`
- `src/sim/traversal_affordance.cpp`: `99f4efed39371e0d8f89c9c461fbdc6d464e0d9bb55ebf53db63870f1a0c3c47`
- `src/sim/traversal_plan.hpp`: `5908b990a5cc8c60b613ba50a48eb2ee006fad45b49b57660d689fc00639dcb0`
- `src/sim/traversal_plan.cpp`: `aeb67aa48b5bba34a94b4400993dc0649a78c14370a619d845e0f45bf7ca1b3f`
- `src/sim/traversal_receiver.hpp`: `11eae72c147afee853fa8dfe5fea94cf991edb9732142d0dd14c68e63505aa1e`
- `src/sim/traversal_receiver.cpp`: `92fbd3a4c759070b62b7a8d6614430776d396d7b80f4862577a4e5e1865020d1`
- `src/sim/simulation/contacts.hpp`: `8ecf6a243601648fdf793d01fbf39ad8bcf4dba534f326c82a6ff1918838d64d`
- `src/sim/simulation/hand_transfer.cpp`: `67f7045939cefb144d667667fb0b04814350027e229b3da610c82a9dd310371e`
- `tests/traversal_affordance_tests.cpp`: `ca50d82e07a552a11749230529f5ecd21ec5d336c6299e1ecfe2d1577a811976`
