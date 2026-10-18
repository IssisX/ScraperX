#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

bool Simulation::PhysicsWorld::begin_corner_shimmy(JPH::BodyInterface &bodies,
                                                  float direction) noexcept {
    if (traversal_state_ != TraversalState::Hanging || cargo_transfer_active_ ||
        !physical_hands_->compatible_hands() || physical_hands_->radial_active()) return false;
    const auto ledge = from_support_local(bodies,traversal_body_,traversal_local_ledge_);
    auto old_normal = traversal_normal_;
    old_normal.SetY(0);
    old_normal = old_normal.NormalizedOr(JPH::Vec3::sZero());
    if (old_normal.IsNearZero()) return false;
    const JPH::Vec3 travel(-old_normal.GetZ()*direction,0,old_normal.GetX()*direction);
    const auto new_normal = -travel;
    JPH::Vec3 measured_old, measured_new;
    const auto old_lip = probe_lip(ledge,old_normal,&measured_old);
    const auto next_lip = probe_lip(ledge+travel*.25F,new_normal,&measured_new);
    // This slice supports an actual perpendicular convex edge of one part,
    // not a rotated guess at another face or a jump between compounds.
    constexpr float normal_roundoff = 1.0e-4F;
    if (!old_lip.valid || !next_lip.valid || old_lip.body != traversal_body_ ||
        next_lip.body != old_lip.body || next_lip.sub_shape != old_lip.sub_shape ||
        measured_old.Dot(old_normal) < 1-normal_roundoff ||
        measured_new.Dot(new_normal) < 1-normal_roundoff ||
        std::abs(measured_old.Dot(measured_new)) > normal_roundoff ||
        std::abs(next_lip.ledge.GetY()-old_lip.ledge.GetY()) > .15F) return false;
    const auto old_face = old_lip.ledge-old_normal*kTopProbeInset;
    const auto new_face = next_lip.ledge-new_normal*kTopProbeInset;
    auto corner = old_face+new_normal*JPH::Vec3(new_face-old_face).Dot(new_normal);
    corner.SetY(old_lip.ledge.GetY());
    if (JPH::Vec3(corner-old_lip.ledge).Length() > kClimbSideLead) return false;
    const auto hold = from_support_local(bodies,traversal_body_,traversal_local_hold_);
    const float stand_off = kPlayerRadius+kHangWallGap;
    auto miter = corner-(old_normal+new_normal)*stand_off;
    miter.SetY(hold.GetY());
    if (!shape_pose_is_clear(player_shape_.GetPtr(),miter+JPH::Vec3(0,hanging_posture_offset_m_,0)) ||
        !standing_path_is_clear(bodies.GetCenterOfMassPosition(player_id_),
            miter+JPH::Vec3(0,hanging_posture_offset_m_,0))) return false;
    auto &plan = corner_transfer_;
    plan.clear();
    {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(),traversal_body_);
        if (!lock.Succeeded() || !lock.GetBody().IsInBroadPhase() || lock.GetBody().IsSensor()) return false;
        const auto &support = lock.GetBody();
        plan.support = traversal_body_;
        plan.part = old_lip.sub_shape;
        plan.shape = support.GetShape();
        plan.entity = support.GetUserData();
        plan.local_corner = JPH::Vec3(support.GetInverseCenterOfMassTransform()*corner);
        plan.local_old_normal = support.GetRotation().Conjugated()*old_normal;
        plan.local_new_normal = support.GetRotation().Conjugated()*new_normal;
        plan.direction = direction;
        plan.hang_drop_m = float(corner.GetY()-hold.GetY());
    }
    for (unsigned hand=0;hand<2;++hand)
        plan.generations[hand] = physical_hands_->grip_readback(hand).generation;
    plan.active = true;
    return true;
}

void Simulation::PhysicsWorld::update_corner_shimmy(JPH::BodyInterface &bodies,
    const StepCommands &commands, float dt, bool regrip_ready) noexcept {
    auto &plan = corner_transfer_;
    if (!plan.active) return;
    JPH::RVec3 corner;
    JPH::Vec3 old_normal, new_normal;
    bool support_valid = false;
    {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(),plan.support);
        support_valid = lock.Succeeded() && lock.GetBody().IsInBroadPhase() &&
            !lock.GetBody().IsSensor() && lock.GetBody().GetShape() == plan.shape &&
            lock.GetBody().GetUserData() == plan.entity;
        if (support_valid) {
            const auto &support = lock.GetBody();
            corner = support.GetCenterOfMassTransform()*plan.local_corner;
            old_normal = support.GetRotation()*plan.local_old_normal;
            new_normal = support.GetRotation()*plan.local_new_normal;
        }
    }
    const std::array<PhysicalHandClimb::GripReadback,2> grips{
        physical_hands_->grip_readback(0),physical_hands_->grip_readback(1)};
    for (unsigned hand=0;hand<2;++hand)
        support_valid = support_valid && grips[hand].attached && !grips[hand].material &&
            grips[hand].support == plan.support && grips[hand].generation == plan.generations[hand];
    if (!support_valid || !actual_hands_in_reach(bodies)) {
        release_hang(bodies);
        return;
    }
    // Only an upright rigid corner is admitted in this slice. A support
    // tilt pauses the intent; its actual attached hands still solve.
    if (std::abs(old_normal.GetY()) > 1.0e-4F || std::abs(new_normal.GetY()) > 1.0e-4F) return;
    const JPH::Vec3 old_right(-old_normal.GetZ(),0,old_normal.GetX());
    const double side = commands.move_input_x*old_right.GetX()+commands.move_input_z*old_right.GetZ();
    if (std::abs(side) <= kClimbInputDeadzone) return;
    const bool forward = side*plan.direction > 0;
    const auto face_normal = forward ? new_normal : old_normal;
    const auto tangent = forward ? old_normal : new_normal;
    const auto actual = bodies.GetCenterOfMassPosition(player_id_);
    const auto valid_lip = [&](const Lip &lip, JPH::Vec3 observed) {
        return lip.valid && lip.body == plan.support && lip.sub_shape == plan.part &&
            observed.Dot(face_normal) >= 1.0F-1.0e-4F &&
            std::abs(lip.ledge.GetY()-corner.GetY()) <= .15F;
    };
    JPH::Vec3 exit_normal;
    auto exit = probe_lip(corner+tangent*kClimbSideLead+face_normal*kTopProbeInset,
                          face_normal,&exit_normal);
    if (!valid_lip(exit,exit_normal)) return;
    if (regrip_ready && plan.destination_hands != (forward ? 3U : 0U)) {
        unsigned hand = 0;
        if (forward) {
            if (plan.destination_hands) hand = (plan.destination_hands & 1U) ? 1U : 0U;
            else {
                const auto travel = -new_normal;
                hand = JPH::Vec3(grips[0].world_anchor-grips[1].world_anchor).Dot(travel) <= 0 ? 0U : 1U;
            }
        } else hand = (plan.destination_hands & 1U) ? 0U : 1U;
        const float progress = std::max(JPH::Vec3(grips[0].world_anchor-corner).Dot(tangent),
                                       JPH::Vec3(grips[1].world_anchor-corner).Dot(tangent));
        JPH::Vec3 observed;
        const auto next = probe_lip(corner+tangent*std::max(kClimbSideLead,progress+kClimbSideLead)+
                                    face_normal*kTopProbeInset,face_normal,&observed);
        const auto from = actual+old_right*(hand == 0 ? -kHangHandSpan : kHangHandSpan)+
            JPH::Vec3(0,kClimbHandMid,0);
        const auto approach = JPH::Vec3(next.ledge-from);
        JPH::RayCastResult hit;
        if (valid_lip(next,observed) &&
            JPH::Vec3(next.ledge-grips[1U-hand].world_anchor).Length() >= .10F &&
            JPH::Vec3(next.ledge-actual).Length() <= 1.75F && !approach.IsNearZero() &&
            cast_ray(from,approach+approach.Normalized()*kLandingSkin,hit) && hit.mFraction > 0 &&
            hit.mBodyID == plan.support && hit.mSubShapeID2 == plan.part &&
            physical_hands_->regrip(hand,plan.support,next.ledge)) {
            ++hands_[hand].generation;
            hands_[hand].body = plan.support;
            hands_[hand].local = to_support_local(bodies,plan.support,next.ledge);
            hands_[hand].valid = true;
            plan.generations[hand] = physical_hands_->grip_readback(hand).generation;
            if (forward) plan.destination_hands |= std::uint8_t(1U<<hand);
            else plan.destination_hands &= std::uint8_t(~(1U<<hand));
            regrip_cooldown_ticks_ = kClimbRegripTicks;
        }
    }
    // Body travel starts only after a genuine destination hand exists.
    // Reversal returns along the same outside path and revalidates grips.
    if (forward && plan.destination_hands == 0) return;
    const float stand_off = kPlayerRadius+kHangWallGap;
    auto miter = corner-(old_normal+new_normal)*stand_off;
    miter.SetY(corner.GetY()-plan.hang_drop_m);
    exit.hold.SetY(miter.GetY());
    const bool outside_face = JPH::Vec3(actual-corner).Dot(face_normal) <=
        -(kPlayerRadius+kLandingSkin);
    const auto goal = outside_face ? exit.hold : miter;
    const auto hold = from_support_local(bodies,plan.support,traversal_local_hold_);
    auto change = JPH::Vec3(goal-hold);
    const float maximum = kShimmySpeed*float(std::min(std::abs(side),1.0))*dt;
    if (change.Length() > maximum) change *= maximum/change.Length();
    const auto next_hold = hold+change;
    const auto body_goal = next_hold+JPH::Vec3(0,hanging_posture_offset_m_,0);
    const std::array<PhysicalHandClimb::GripReadback,2> now{
        physical_hands_->grip_readback(0),physical_hands_->grip_readback(1)};
    for (const auto &grip : now)
        if (JPH::Vec3(grip.world_anchor-body_goal).Length() > 1.75F) return;
    if (!shape_pose_is_clear(player_shape_.GetPtr(),body_goal) ||
        !standing_path_is_clear(actual,body_goal)) return;
    traversal_local_hold_ = to_support_local(bodies,plan.support,next_hold);
    if (outside_face && plan.destination_hands == (forward ? 3U : 0U) &&
        JPH::Vec3(next_hold-goal).Length() <= 1.0e-4F) {
        traversal_normal_ = face_normal;
        traversal_local_ledge_ = to_support_local(bodies,plan.support,exit.ledge);
        traversal_target_body_ = plan.support;
        traversal_local_target_ = to_support_local(bodies,plan.support,exit.landing);
        plan.clear();
    }
}

} // namespace scraperx::sim
