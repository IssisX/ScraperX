#pragma once
#include "sim/traversal_affordance.hpp"
namespace JPH { struct RRayCast; class RayCastResult; }
namespace scraperx::sim {
// Recast the accepted ray against its exact receiver. Only coincident faces
// join the patch; a ray never supplies a solved foot contact.
[[nodiscard]] bool collect_receiver_surface_patch(const JPH::PhysicsSystem &system,
    const JPH::RRayCast &ray, const JPH::RayCastResult &canonical,
    ReceiverSurfacePatch &patch) noexcept;
}
