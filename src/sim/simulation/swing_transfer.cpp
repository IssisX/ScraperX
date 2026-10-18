#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

bool Simulation::PhysicsWorld::try_swing_regrip(const JPH::BodyInterface &bodies,
                                               const StepCommands &commands) noexcept {
    // This operation retains the existing Cartesian swing owner. Passive
    // radial catches and material grips keep their separate lifecycle.
    if (traversal_state_ != TraversalState::Climbing || !physical_hands_->swinging() ||
        physical_hands_->radial_active() || !physical_hands_->compatible_hands() ||
        physical_foot_push_->active() || carry_constraint_ || facing_.IsNearZero() ||
        regrip_cooldown_ticks_ > 0 || !actual_hands_in_reach(bodies)) return false;
    const std::array<PhysicalHandClimb::GripReadback,2> grips{
        physical_hands_->grip_readback(0),physical_hands_->grip_readback(1)};
    if (grips[0].material || grips[1].material) return false;
    const unsigned hand = JPH::Vec3(grips[0].world_anchor-grips[1].world_anchor).Dot(facing_) <= 0 ? 0U : 1U;
    const auto centre = bodies.GetCenterOfMassPosition(player_id_);
    const JPH::Vec3 right(-facing_.GetZ(),0,facing_.GetX());
    const auto shoulder = centre+JPH::Vec3(0,kClimbHandMid,0)+
        right*(hand == 0 ? -kClimbHandSpan : kClimbHandSpan);
    Grip selected;
    float closest = std::numeric_limits<float>::max();
    // Reuse the existing small hand query at a fixed set of reach offsets.
    // The actual shoulder and body envelopes still admit the chosen point.
    for (float height : {kClimbHandMid,kClimbHandHigh}) {
        for (unsigned lead=0;lead<3;++lead) {
            const auto aim = centre+JPH::Vec3(0,height,0)+
                facing_*(kClimbHandReach+float(lead)*kClimbSideLead)+
                right*(hand == 0 ? -kClimbHandSpan : kClimbHandSpan);
            const auto candidate = find_grip(centre,aim,facing_,grips[hand].support);
            if (!candidate.valid || !is_parkour_swing_entity(candidate.entity_id) ||
                (grips[0].support != grips[1].support && candidate.body != grips[1U-hand].support) ||
                JPH::Vec3(candidate.point-centre).Length() > 1.75F ||
                JPH::Vec3(candidate.point-shoulder).Length() > kTransferShoulderReachM ||
                JPH::Vec3(candidate.point-grips[hand].world_anchor).Dot(facing_) <= kLandingSkin ||
                JPH::Vec3(candidate.point-grips[1U-hand].world_anchor).Length() < 2*.041F) continue;
            const float distance = JPH::Vec3(candidate.point-shoulder).LengthSq();
            if (distance < closest) {
                selected = candidate;
                closest = distance;
            }
        }
    }
    if (!selected.valid || !physical_hands_->regrip(hand,selected.body,selected.point)) return false;
    // Reapply the existing swing profile to the replacement row; no new
    // spring law, force cap, work budget or opposite impulse is supplied.
    physical_hands_->set_swing_profile();
    hands_[hand].body = selected.body;
    hands_[hand].local = to_support_local(bodies,selected.body,selected.point);
    hands_[hand].valid = true;
    ++hands_[hand].generation;
    regrip_cooldown_ticks_ = kClimbRegripTicks;
    const auto midpoint = .5F*(hand_point(bodies,0)+hand_point(bodies,1));
    if (hands_[0].body == hands_[1].body) {
        traversal_body_ = selected.body;
        traversal_entity_id_ = selected.entity_id;
        traversal_normal_ = facing_;
        traversal_local_ledge_ = to_support_local(bodies,selected.body,midpoint);
    }
    const auto commanded = physical_hands_->commanded_position();
    const JPH::Vec3 radius(midpoint-parkour_swing_pivot(traversal_entity_id_));
    const auto tangent = JPH::Vec3(-radius.GetY(),radius.GetX(),0).NormalizedOr(JPH::Vec3::sAxisX());
    // Rebase the existing goal at its current command. A new principal
    // support must not reset the body below the bar or replay the lean.
    swing_rest_offset_ = JPH::Vec3(commanded-midpoint)-tangent*float(.15*commands.move_input_x)-
        JPH::Vec3(0,hanging_posture_offset_m_,0);
    traversal_local_hold_ = to_support_local(bodies,traversal_body_,commanded);
    return true;
}

void Simulation::PhysicsWorld::advance_swing_targets(const JPH::BodyInterface &bodies,
    const StepCommands &commands, float dt) noexcept {
    const auto midpoint = .5F*(hand_point(bodies,0)+hand_point(bodies,1));
    const JPH::Vec3 radius(midpoint-parkour_swing_pivot(traversal_entity_id_));
    const auto tangent = JPH::Vec3(-radius.GetY(),radius.GetX(),0).NormalizedOr(JPH::Vec3::sAxisX());
    // Existing single-hanger lean:0.15m, at most0.6m/s and500W through
    // the finite hand owner. Gravity and the real passive hinge drive motion.
    auto goal = midpoint+swing_rest_offset_+tangent*float(.15*commands.move_input_x);
    JPH::Vec3 transfer = JPH::Vec3::sZero();
    if (hands_[0].body != hands_[1].body) {
        const unsigned receiving = hands_[0].body != traversal_body_ ? 0U : 1U;
        const auto destination = hand_point(bodies,receiving);
        auto direction = JPH::Vec3(destination-hand_point(bodies,1U-receiving));
        direction.SetY(0);
        direction = direction.NormalizedOr(JPH::Vec3::sZero());
        const double intent = commands.move_input_x*direction.GetX()+commands.move_input_z*direction.GetZ();
        if (intent > kClimbInputDeadzone) {
            // Bring the torso toward the actual receiving hand. A regrip
            // alone cannot make its original rest offset cross the gap.
            const float stand_off = kPlayerRadius+.5F*kGripMaxSection+kHangWallGap;
            const float remaining = JPH::Vec3(destination-direction*stand_off-goal).Dot(direction);
            transfer = direction*std::clamp(remaining,0.0F,
                kClimbSideSpeed*float(std::min(intent,1.0))*dt);
            const auto body_goal = goal+transfer+JPH::Vec3(0,hanging_posture_offset_m_,0);
            if (!shape_pose_is_clear(player_shape_.GetPtr(),body_goal) ||
                !standing_path_is_clear(bodies.GetCenterOfMassPosition(player_id_),body_goal))
                transfer = JPH::Vec3::sZero();
        }
    }
    goal += transfer;
    const auto before = physical_hands_->commanded_position();
    const float desired_posture = hanging_posture_eligible(bodies) && hanging_posture_input_ ? .25F : 0.0F;
    const auto combined = JPH::Vec3(goal-before)+JPH::Vec3(0,desired_posture,0);
    // Exactly one hand-owner command per tick shares the original speed and
    // work budget between following, lean, torso transfer and paid posture.
    advance_hanging_posture(bodies,JPH::Vec3(goal-before)+
        JPH::Vec3(0,hanging_posture_offset_m_,0),dt,.12F);
    const auto delivered = JPH::Vec3(physical_hands_->commanded_position()-before);
    if (!transfer.IsNearZero() && combined.LengthSq() > 0) {
        const float fraction = std::clamp(delivered.Dot(combined)/combined.LengthSq(),0.0F,1.0F);
        swing_rest_offset_ += transfer*fraction;
    }
}

} // namespace scraperx::sim
