#pragma once

#include "sim/traversal_affordance.hpp"

namespace scraperx::sim {

enum class TraversalPlanKind : std::uint8_t {
    None, LedgeCatchTopOut, GripClimb, SwingTransfer, VaultProp, LandingRecovery
};
enum class TraversalPhase : std::uint8_t {
    None, CandidateLatched, CatchPending, HandSupported, AwaitingFootContact,
    SharedHandFoot, FootSupported, Releasing, Aborting
};
enum class TraversalRejection : std::uint8_t {
    None, InvalidReceiver, StaleContact, WrongContact, BlockedClearance, LostHands
};

// Receipt of a completed native solve, never a prospective ray hit.
struct FootContactSample {
    bool valid = false;
    JPH::BodyID body;
    JPH::SubShapeID subshape;
    std::uint64_t entity_id = 0, solved_tick = 0;
    JPH::RVec3 world_point = JPH::RVec3::sZero();
    JPH::Vec3 local_point = JPH::Vec3::sZero();
    JPH::Vec3 local_normal = JPH::Vec3::sAxisY();
    JPH::Vec3 world_normal = JPH::Vec3::sAxisY();
    JPH::Vec3 point_velocity = JPH::Vec3::sZero();
    float friction = 0;
    const JPH::Shape *shape_identity = nullptr;
};

// A lifecycle record for the existing physical owners. No body path, force
// allocator, actuator budget or contact fabrication belongs here.
struct TraversalPlanState {
    TraversalPlanKind kind = TraversalPlanKind::None;
    TraversalPhase phase = TraversalPhase::None;
    TraversalRejection rejection = TraversalRejection::None;
    std::uint64_t generation = 0, input_tick = 0, last_contact_tick = 0;
    std::uint64_t shared_solved_ticks = 0;
    JPH::BodyID receiver;
    JPH::SubShapeID receiver_subshape;
    std::uint64_t receiver_entity = 0;
    const JPH::Shape *receiver_shape = nullptr;
    JPH::Vec3 receiver_local_target = JPH::Vec3::sZero();
    bool foot_started = false;

    void latch(const TraversalAffordance &candidate, JPH::Vec3 local_target,
               const JPH::Shape *shape, std::uint64_t tick) noexcept;
    [[nodiscard]] bool accepts(const FootContactSample &contact,
                               std::uint64_t latest_solved_tick) const noexcept;
    void clear() noexcept { *this = TraversalPlanState{}; }
};

} // namespace scraperx::sim
