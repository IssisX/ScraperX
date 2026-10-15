#include "sim/traversal_plan.hpp"

namespace scraperx::sim {

void TraversalPlanState::latch(const TraversalAffordance &candidate,
                              JPH::Vec3 local_target, const JPH::Shape *shape,
                              std::uint64_t tick) noexcept {
    clear();
    kind = TraversalPlanKind::LedgeCatchTopOut;
    phase = TraversalPhase::HandSupported;
    generation = candidate.generation;
    receiver = candidate.receiver_body;
    receiver_subshape = candidate.receiver_subshape;
    receiver_patch = candidate.receiver_patch;
    if (receiver_patch.count == 0 && !receiver_patch.overflow) receiver_patch.add(receiver_subshape);
    receiver_entity = candidate.receiver_entity_id;
    receiver_shape = shape;
    receiver_local_target = local_target;
    input_tick = tick;
}

bool TraversalPlanState::accepts(const FootContactSample &contact,
                                 std::uint64_t latest_solved_tick) const noexcept {
    return kind == TraversalPlanKind::LedgeCatchTopOut && contact.valid &&
        contact.solved_tick == latest_solved_tick && contact.solved_tick > input_tick &&
        !receiver.IsInvalid() && receiver_shape && receiver_patch.contains(receiver_subshape) &&
        contact.body == receiver && receiver_patch.contains(contact.subshape) &&
        contact.entity_id == receiver_entity && contact.shape_identity == receiver_shape &&
        contact.friction > 0;
}

} // namespace scraperx::sim
