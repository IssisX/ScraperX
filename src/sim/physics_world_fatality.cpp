// Claimed, the fatality (matron.hpp "Fatal", matron_fatality.cpp): the
// physics of what her hands do. Her knee is a body on a capped muscle, its
// reaction on her; it pins the torso by a point. Each leg is made a body of
// its own when her fist closes on its ankle, hung from the hip by a stiff
// swing-twist whose load, past kLegHoldN, over time past kLegTearNs, tears
// it (the arm's law); torn, it is a corpse body she tosses aside. The torso
// is dragged by its collar onto the stage-A skip lift's rope (the shackle
// unhooked from its bollard, her knot a point constraint that parts past
// kKnotTearN), she wrenches the catch's trip handle down, and the kit does
// the rest: 800 kg of skip down the well, the torso up the rope, the jerk
// at the buffer, the knot gone, the fall. No death is counted in it: the run
// is hers, and it ends (run_ended_by_matron_) when she has wiped her hands.

#include "sim/physics_world.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>

#include <algorithm>
#include <cmath>

namespace scraperx::sim {
namespace {
constexpr float kKneeSpringNpm = 6000.0F;
constexpr float kKneeDampNspm = 600.0F;
constexpr float kKneeReachM = 1.1F;      // her hip to her knee
constexpr float kKneeCloseM = 0.35F;
constexpr float kFistCloseM = 0.16F;
constexpr float kKnotCloseM = 0.45F;   // the collar to the shackle: a short lashing (the torso is 0.35 thick)
// The legs, in the body's frame (the capsule's centre at the hips' height
// less 0.41): the hip joint and the ankle.
constexpr float kLegSideM = 0.12F;
constexpr float kHipUp = 0.0F;
constexpr float kAnkleUp = -0.82F;
constexpr float kCollarUp = 0.72F;
constexpr float kLegConeRad = 0.25F;
constexpr float kFallLandSpeedMps = 1.0F;
} // namespace

JPH::Vec3 Simulation::PhysicsWorld::ankle_local(const int side) const noexcept {
    return JPH::Vec3(side == 0 ? -kLegSideM : kLegSideM, kAnkleUp, 0.0F);
}

void Simulation::PhysicsWorld::set_player_mass_for_losses(JPH::BodyInterface & /*bodies*/) noexcept {
    JPH::BodyLockWrite lock(physics_system_.GetBodyLockInterface(), player_id_);
    if (lock.Succeeded()) {
        JPH::Body &body = lock.GetBody();
        JPH::MassProperties mass = body.GetShape()->GetMassProperties();
        const float kg = kPlayerMassKg - Matron::kArmMassKg * float(arms_lost()) -
                         Matron::kLegMassKg * float(int(leg_off_[0]) + int(leg_off_[1]));
        mass.ScaleToMass(std::max(20.0F, kg));
        body.GetMotionProperties()->SetMassProperties(body.GetMotionProperties()->GetAllowedDOFs(), mass);
    }
}

void Simulation::PhysicsWorld::fatality_reset(JPH::BodyInterface &bodies) noexcept {
    destroy_grab_constraint(knee_pin_);
    destroy_grab_constraint(knee_arm_);
    destroy_grab_constraint(fist_);
    destroy_grab_constraint(knot_);
    destroy_grab_constraint(handle_fist_);
    for (int k = 0; k < 2; ++k) {
        destroy_grab_constraint(hip_[k]);
        if (!leg_id_[k].IsInvalid()) {
            remove_and_destroy(bodies, leg_id_[k]);
            leg_id_[k] = JPH::BodyID();
        }
        leg_off_[k] = false;
        leg_strain_ns_[k] = 0.0F;
    }
    if (!knee_id_.IsInvalid()) {
        remove_and_destroy(bodies, knee_id_);
        knee_id_ = JPH::BodyID();
    }
    fist_on_ = FistOn::None;
    leg_in_fist_ = -1;
    fatal_prev_beat_ = -1;
    knot_load_n_ = 0.0F;
    haul_peak_y_ = 0.0F;
    fall_from_y_ = 0.0F;
    fall_peak_speed_ = 0.0F;
    haul_seen_ = false;
}

JPH::BodyID Simulation::PhysicsWorld::make_limb_body(JPH::BodyInterface &bodies, const JPH::RVec3 at,
                                                     const JPH::Quat rot, const float half, const float radius,
                                                     const float kg, const std::uint64_t entity,
                                                     const JPH::ObjectLayer layer) {
    JPH::Ref<JPH::Shape> shape = new JPH::CapsuleShape(half, radius);
    JPH::BodyCreationSettings settings(shape, at, rot, JPH::EMotionType::Dynamic, layer);
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = kg;
    settings.mAllowSleeping = false;
    settings.mUserData = entity;
    settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
    return bodies.CreateAndAddBody(settings, JPH::EActivation::Activate);
}

void Simulation::PhysicsWorld::fatality_drive(JPH::BodyInterface &bodies, const float dt) noexcept {
    if (!matron_->claimed()) {
        if (!knee_id_.IsInvalid() || fist_ != nullptr || knot_ != nullptr) {
            destroy_grab_constraint(knee_pin_);
            destroy_grab_constraint(knee_arm_);
            destroy_grab_constraint(fist_);
            destroy_grab_constraint(knot_);
            destroy_grab_constraint(handle_fist_);
            if (!knee_id_.IsInvalid()) {
                remove_and_destroy(bodies, knee_id_);
                knee_id_ = JPH::BodyID();
            }
        }
        return;
    }
    const int beat = static_cast<int>(matron_->fatal_beat());
    if (beat != fatal_prev_beat_) {
        fatality_beat_edges_[static_cast<std::size_t>(std::clamp(beat, 0, 9))] += 1;
        fatal_prev_beat_ = beat;
    }
    const JPH::RMat44 frame = bodies.GetWorldTransform(player_id_);
    const JPH::RVec3 torso = bodies.GetPosition(player_id_);
    JPH::Body *shackle_body = nullptr;
    const JPH::BodyID shackle_id = kit_->body_id(well_.a_shackle);
    const JPH::BodyID handle_id = kit_->body_id(well_.a_handle);
    Matron::FatalPoints points;
    points.torso = torso;
    points.collar = frame * JPH::Vec3(0.0F, kCollarUp, 0.0F);
    for (int k = 0; k < 2; ++k) {
        points.ankle[k] = !leg_id_[k].IsInvalid() && !leg_off_[k]
                              ? bodies.GetWorldTransform(leg_id_[k]) * JPH::Vec3(0.0F, -0.41F, 0.0F)
                              : frame * ankle_local(k);
    }
    points.shackle = shackle_id.IsInvalid() ? torso : bodies.GetPosition(shackle_id);
    points.handle = handle_id.IsInvalid() ? torso : bodies.GetPosition(handle_id);
    points.torso_grounded = (grounded_ || body_down_on_ground(bodies)) &&
                            JPH::Vec3(bodies.GetLinearVelocity(player_id_)).Length() < 0.5F;
    matron_->set_fatal_points(points);
    (void)shackle_body;

    // ---- her knee ----------------------------------------------------------
    const JPH::RVec3 her = bodies.GetPosition(matron_->body());
    if (matron_->knee_down() && knee_id_.IsInvalid()) {
        JPH::Ref<JPH::Shape> sphere = new JPH::SphereShape(0.1F);
        JPH::BodyCreationSettings settings(sphere, her, JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic,
                                           object_layers::kHand);
        settings.mAllowedDOFs =
            JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY | JPH::EAllowedDOFs::TranslationZ;
        settings.mGravityFactor = 0.0F;
        settings.mAllowSleeping = false;
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = 8.0F;
        knee_id_ = bodies.CreateAndAddBody(settings, JPH::EActivation::Activate);
        JPH::DistanceConstraintSettings leg;
        leg.mSpace = JPH::EConstraintSpace::WorldSpace;
        leg.mPoint1 = her;
        leg.mPoint2 = her;
        leg.mMinDistance = 0.0F;
        leg.mMaxDistance = kKneeReachM;
        leg.mLimitsSpringSettings.mFrequency = 8.0F;
        leg.mLimitsSpringSettings.mDamping = 0.9F;
        knee_arm_ = create_constraint(leg, matron_->body(), knee_id_, false);
    }
    if (!matron_->knee_down() && !knee_id_.IsInvalid()) {
        destroy_grab_constraint(knee_pin_);
        destroy_grab_constraint(knee_arm_);
        remove_and_destroy(bodies, knee_id_);
        knee_id_ = JPH::BodyID();
    }
    if (!knee_id_.IsInvalid()) {
        const JPH::RVec3 knee = bodies.GetPosition(knee_id_);
        if (knee_pin_ == nullptr && JPH::Vec3(knee - torso).Length() <= kKneeCloseM) {
            JPH::PointConstraintSettings pin;
            pin.mSpace = JPH::EConstraintSpace::WorldSpace;
            pin.mPoint1 = knee;
            pin.mPoint2 = knee;
            knee_pin_ = create_constraint(pin, knee_id_, player_id_, false);
            if (knee_pin_ != nullptr) {
                ++knee_pin_count_;
                matron_->fatal_pinned();
            }
        }
        JPH::Vec3 force = kKneeSpringNpm * JPH::Vec3(matron_->knee_target() - knee) -
                          kKneeDampNspm * bodies.GetLinearVelocity(knee_id_);
        if (force.Length() > Matron::kKneePinN) {
            force *= Matron::kKneePinN / force.Length();
        }
        bodies.AddForce(knee_id_, force);
        bodies.AddForce(matron_->body(), -force);
    }

    // ---- her fist: legs, collar ------------------------------------------
    if (hand_id_.IsInvalid()) {
        return;
    }
    const JPH::RVec3 fist = bodies.GetPosition(hand_id_);
    const int side = matron_->leg_side();
    if (matron_->reaching_leg() && fist_ == nullptr && !leg_off_[side]) {
        const JPH::RVec3 ankle = frame * ankle_local(side);
        if (JPH::Vec3(ankle - fist).Length() <= kFistCloseM) {
            // The leg is its own body from here: hung at the hip.
            const JPH::Quat rot = bodies.GetRotation(player_id_);
            const JPH::Vec3 hip_local(side == 0 ? -kLegSideM : kLegSideM, kHipUp, 0.0F);
            const JPH::RVec3 hip = frame * hip_local;
            const JPH::RVec3 middle = frame * JPH::Vec3(hip_local.GetX(), kHipUp - 0.41F, 0.0F);
            leg_id_[side] = make_limb_body(bodies, middle, rot, 0.33F, 0.08F, Matron::kLegMassKg,
                                           Simulation::kSeveredLegEntityId + std::uint64_t(side), object_layers::kHand);
            bodies.SetGravityFactor(leg_id_[side], 0.0F);
            bodies.SetLinearVelocity(leg_id_[side], bodies.GetLinearVelocity(player_id_));
            JPH::SwingTwistConstraintSettings joint;
            joint.mSpace = JPH::EConstraintSpace::WorldSpace;
            joint.mPosition1 = hip;
            joint.mPosition2 = hip;
            const JPH::Vec3 down = -rot.RotateAxisY();
            joint.mTwistAxis1 = down;
            joint.mTwistAxis2 = down;
            joint.mPlaneAxis1 = rot.RotateAxisX();
            joint.mPlaneAxis2 = rot.RotateAxisX();
            joint.mNormalHalfConeAngle = kLegConeRad;
            joint.mPlaneHalfConeAngle = kLegConeRad;
            joint.mTwistMinAngle = -0.2F;
            joint.mTwistMaxAngle = 0.2F;
            hip_[side] = create_constraint(joint, player_id_, leg_id_[side], false);
            JPH::PointConstraintSettings grip;
            grip.mSpace = JPH::EConstraintSpace::WorldSpace;
            grip.mPoint1 = fist;
            grip.mPoint2 = fist;
            fist_ = create_constraint(grip, hand_id_, leg_id_[side], false);
            fist_on_ = FistOn::Leg;
            leg_in_fist_ = side;
            leg_strain_ns_[side] = 0.0F;
            leg_peak_n_ = 0.0F;
            set_player_mass_for_losses(bodies);
            matron_->fatal_leg_taken();
        }
    }
    if (matron_->reaching_collar() && fist_ == nullptr) {
        if (JPH::Vec3(points.collar - fist).Length() <= kFistCloseM) {
            JPH::PointConstraintSettings grip;
            grip.mSpace = JPH::EConstraintSpace::WorldSpace;
            grip.mPoint1 = fist;
            grip.mPoint2 = fist;
            fist_ = create_constraint(grip, hand_id_, player_id_, false);
            fist_on_ = FistOn::Collar;
            ++collar_count_;
            // The shackle off its bollard, onto the deck where it was made fast.
            if (kit_->unhook(well_.a_rope) != 0) {
                ++unhook_count_;
            }
            matron_->fatal_collar_taken();
        }
    }
    if (matron_->hooking() && knot_ == nullptr && fist_on_ == FistOn::Collar) {
        const JPH::BodyID shackle = kit_->body_id(well_.a_shackle);
        if (!shackle.IsInvalid() && bodies.IsAdded(shackle) &&
            JPH::Vec3(bodies.GetPosition(shackle) - points.collar).Length() <= kKnotCloseM) {
            // Her knot: the torso to the shackle, at the collar.
            JPH::PointConstraintSettings knot;
            knot.mSpace = JPH::EConstraintSpace::WorldSpace;
            knot.mPoint1 = bodies.GetPosition(shackle);
            knot.mPoint2 = points.collar;
            knot_ = create_constraint(knot, shackle, player_id_, false);
            if (knot_ != nullptr) {
                ++knot_count_;
                destroy_grab_constraint(fist_);
                fist_on_ = FistOn::None;
                matron_->fatal_hooked();
            }
        }
    }
    // ---- her second hand on the trip handle --------------------------------
    if (matron_->reaching_handle() && handle_fist_ == nullptr && !hand2_id_.IsInvalid() && !handle_id.IsInvalid()) {
        const JPH::RVec3 h2 = bodies.GetPosition(hand2_id_);
        if (JPH::Vec3(points.handle - h2).Length() <= kFistCloseM) {
            JPH::PointConstraintSettings grip;
            grip.mSpace = JPH::EConstraintSpace::WorldSpace;
            grip.mPoint1 = h2;
            grip.mPoint2 = h2;
            handle_fist_ = create_constraint(grip, hand2_id_, handle_id, false);
            if (handle_fist_ != nullptr) {
                matron_->fatal_handle_taken();
            }
        }
    }
    if (matron_->fatal_beat() == Matron::Fatal::Kick && !kit_->catch_latched(well_.a_catch)) {
        // The pin is out of its hole: the skip goes.
        ++brake_kick_count_;
        destroy_grab_constraint(handle_fist_);
        haul_from_y_ = static_cast<float>(torso.GetY());
        haul_peak_y_ = haul_from_y_;
        matron_->fatal_tripped();
    }
    if (!matron_->hand2_active()) {
        destroy_grab_constraint(handle_fist_);
    }
    // A leg torn: out of her fist with a toss.
    JPH::Vec3 toss;
    if (matron_->take_leg_toss(toss) && leg_in_fist_ >= 0) {
        destroy_grab_constraint(fist_);
        fist_on_ = FistOn::None;
        const JPH::BodyID leg = leg_id_[leg_in_fist_];
        bodies.SetObjectLayer(leg, object_layers::kCorpse);
        bodies.SetGravityFactor(leg, 1.0F);
        bodies.SetLinearVelocity(leg, toss);
        ++leg_toss_count_;
        leg_in_fist_ = -1;
    }
    (void)dt;
}

void Simulation::PhysicsWorld::fatality_post_step(JPH::BodyInterface &bodies, const float dt) noexcept {
    if (!matron_->claimed()) {
        return;
    }
    // The hips: the strain law, as the arms'.
    for (int k = 0; k < 2; ++k) {
        if (hip_[k] == nullptr) {
            continue;
        }
        const auto *joint = static_cast<JPH::SwingTwistConstraint *>(hip_[k].GetPtr());
        const float load = joint->GetTotalLambdaPosition().Length() / std::max(dt, 1.0e-4F);
        leg_load_n_ = load;
        leg_peak_n_ = std::max(leg_peak_n_, load);
        leg_strain_ns_[k] += std::max(0.0F, load - Matron::kLegHoldN) * dt;
        if (leg_strain_ns_[k] >= Matron::kLegTearNs) {
            destroy_grab_constraint(hip_[k]);
            leg_off_[k] = true;
            ++leg_tear_count_;
            last_leg_peak_n_ = leg_peak_n_;
            set_player_mass_for_losses(bodies);
            matron_->fatal_leg_torn();
        }
    }
    const JPH::RVec3 torso = bodies.GetPosition(player_id_);
    const JPH::Vec3 v = bodies.GetLinearVelocity(player_id_);
    if (knot_ != nullptr) {
        const auto *knot = static_cast<JPH::PointConstraint *>(knot_.GetPtr());
        knot_load_n_ = knot->GetTotalLambdaPosition().Length() / std::max(dt, 1.0e-4F);
        knot_peak_n_ = std::max(knot_peak_n_, knot_load_n_);
        if (matron_->fatal_beat() == Matron::Fatal::Haul) {
            haul_peak_y_ = std::max(haul_peak_y_, static_cast<float>(torso.GetY()));
            if (haul_peak_y_ > haul_from_y_ + 1.0F && !haul_seen_) {
                haul_seen_ = true;
                ++haul_count_;
            }
        }
        if (knot_load_n_ > Matron::kKnotTearN) {
            // The jerk: her knot parts.
            destroy_grab_constraint(knot_);
            ++knot_part_count_;
            fall_from_y_ = static_cast<float>(torso.GetY());
            fall_peak_speed_ = 0.0F;
            matron_->fatal_dropped();
        }
    }
    if (matron_->fatal_beat() == Matron::Fatal::Fall) {
        fall_peak_speed_ = std::max(fall_peak_speed_, -v.GetY());
        const bool down = fall_peak_speed_ > 3.0F && v.Length() < kFallLandSpeedMps;
        if (down || ((grounded_ || body_down_on_ground(bodies)) && fall_peak_speed_ > 3.0F)) {
            ++fall_landing_count_;
            landing_speed_mps_ = fall_peak_speed_;
            landing_drop_m_ = fall_from_y_ - static_cast<float>(torso.GetY());
            matron_->fatal_landed();
        }
    }
    if (matron_->fatality_over()) {
        run_ended_by_matron_ = true;
    }
}

} // namespace scraperx::sim
