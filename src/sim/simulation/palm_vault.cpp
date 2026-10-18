#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

bool Simulation::PhysicsWorld::palm_props_active() const noexcept {
    return (physical_palms_[0] && physical_palms_[0]->active()) ||
           (physical_palms_[1] && physical_palms_[1]->active());
}

void Simulation::PhysicsWorld::clear_palm_props(PhysicalHandProp::StopReason reason) noexcept {
    for (auto &palm : physical_palms_) if (palm) palm->clear(reason);
}

void Simulation::PhysicsWorld::update_palm_vault(JPH::BodyInterface &bodies,
                                               const StepCommands &commands) noexcept {
    // Brief capsule contact with the rail must not renew a cancelled palm
    // episode while Crouch remains held. Receiving feet plus stance release
    // rearm the next deliberate approach.
    if (grounded_ && !commands.crouch_held) palm_vault_used_ = false;
    const bool excluded = regression_fixtures_ || grounded_ || !commands.crouch_held ||
        traversal_state_ != TraversalState::None || physical_hands_->active() ||
        carry_constraint_ != nullptr || grapple_equipped_ || parachute_deployed_ ||
        commands.traversal_requested || commands.jump_requested || commands.release_requested ||
        commands.cancel_jump_requested || commands.pick_up_requested || commands.set_down_requested ||
        commands.rig_requested || commands.sling_action || commands.sling_drop ||
        (slingshot_ && slingshot_->controls_player());
    if (excluded) {
        clear_palm_props();
        return;
    }

    // Each planted palm retains its own actual material point and identity.
    // Fresh occlusion cancels rather than letting an arm reach through a wall.
    if (palm_props_active()) {
        for (auto &palm : physical_palms_) {
            if (!palm || !palm->active()) continue;
            const auto shoulder = bodies.GetPosition(player_id_) +
                JPH::Vec3(0, palm->shoulder_offset_m(), 0);
            const auto &contact = palm->readback();
            bool valid = false;
            {
                const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(), contact.support);
                valid = lock.Succeeded() && lock.GetBody().IsInBroadPhase();
            }
            if (!valid) {
                palm->clear(PhysicalHandProp::StopReason::InvalidBody);
                continue;
            }
            // Refresh from the owning material frame before the approach
            // query. Queries and constraint removal run without a held lock.
            const auto point = from_support_local(bodies, contact.support, palm->support_local_anchor());
            JPH::RayCastResult first;
            const JPH::Vec3 approach(point - shoulder);
            if (approach.IsNearZero() ||
                !cast_ray(shoulder, approach + approach.Normalized()*kLandingSkin, first) ||
                first.mBodyID != contact.support || first.mSubShapeID2 != contact.subshape)
                palm->clear(PhysicalHandProp::StopReason::SurfaceLost);
        }
        return;
    }
    if (palm_vault_used_ || !crouched_ || facing_.IsNearZero()) return;
    const auto at = bodies.GetPosition(player_id_);
    const auto velocity = bodies.GetLinearVelocity(player_id_);
    const float forward_speed = velocity.Dot(facing_);
    const double forward_intent = commands.move_input_x*facing_.GetX() +
                                  commands.move_input_z*facing_.GetZ();
    // A compact, committed descending jump may place a palm. It never turns
    // a stationary reach or an ascending separation into hidden lift.
    if (velocity.GetY() > 0 || forward_speed < kPlayerMaximumRelativeSpeed*kCrouchSpeedScale ||
        forward_intent < kHangIntentDotThreshold ||
        !shape_pose_is_clear(player_crouch_shape_.GetPtr(), at)) return;

    const JPH::Vec3 right(facing_.GetZ(), 0, -facing_.GetX());
    bool acquired = false;
    for (unsigned hand = 0; hand < 2; ++hand) {
        const auto shoulder = at + JPH::Vec3(0, physical_palms_[hand]->shoulder_offset_m(), 0);
        const auto aim = shoulder + facing_*kClimbHandReach +
            right*(hand == 0 ? -kHangHandSpan : kHangHandSpan);
        const JPH::Vec3 down(0, -kTransferShoulderReachM, 0);
        JPH::RayCastResult hit;
        if (!cast_ray(aim, down, hit) || hit.mFraction <= 0) continue;
        const auto point = aim + down*hit.mFraction;
        const auto normal = surface_normal(hit.mBodyID, hit.mSubShapeID2, point);
        const JPH::Vec3 approach(point - shoulder);
        if (normal.GetY() < kSupportNormalThreshold || approach.IsNearZero() ||
            approach.Length() > kTransferShoulderReachM) continue;
        JPH::RayCastResult first;
        if (!cast_ray(shoulder, approach + approach.Normalized()*kLandingSkin, first) ||
            first.mBodyID != hit.mBodyID || first.mSubShapeID2 != hit.mSubShapeID2) continue;
        // Discovery does not report load or footing. Only this finite
        // non-gripping Jolt contact can later publish a solved palm force.
        acquired = physical_palms_[hand]->begin(hit.mBodyID, hit.mSubShapeID2, point, normal) || acquired;
    }
    if (acquired) {
        palm_vault_used_ = true;
        ++palm_vault_count_;
    }
}

} // namespace scraperx::sim
