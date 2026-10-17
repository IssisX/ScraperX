#pragma once
#include "sim/traversal_affordance.hpp"
namespace JPH { struct RRayCast; class RayCastResult; }
namespace scraperx::sim {
struct TraversalPlanState;
// Recast the accepted ray against its exact receiver. Only coincident faces
// join the patch; a ray never supplies a solved foot contact.
[[nodiscard]] bool collect_receiver_surface_patch(const JPH::PhysicsSystem &system,
    const JPH::RRayCast &ray, const JPH::RayCastResult &canonical,
    ReceiverSurfacePatch &patch) noexcept;
// Prospective continuation only. This query cannot create/renew foot contact.
[[nodiscard]] bool receiver_supports_continuation(const JPH::PhysicsSystem &system,
    const JPH::RRayCast &ray, const TraversalPlanState &plan,
    JPH::BodyID ignored_body, float minimum_normal_y) noexcept;
}
