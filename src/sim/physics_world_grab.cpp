// Her hold on the player, the physics of it (matron.hpp "Her hold";
// matron_hold.cpp decides what her hand does). Members of
// Simulation::PhysicsWorld.
//
// Laws: nothing teleports (her hand is a body that moves at finite speed from
// her shoulder; the player's DOFs are unlocked in place, position and velocity
// kept); no force is infinite (her muscle is a capped spring-damper, the grip
// tears over kGripMaxForceN or on its strain, the player's brace on a rail
// tears on the ledge grip's limits); recovery is always possible (thrashing
// strains the grip till it tears, though once she has a wrist the arm stays
// in her hand; a brace holds her off a while); no health numbers (what
// happens is a fall, judged by the fall rules).

#include "sim/physics_world.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>

#include <algorithm>
#include <cmath>

namespace scraperx::sim {
namespace {
// Her muscle: the hand's spring and damper to its pose.
constexpr float kHandSpringNpm = 3500.0F;
constexpr float kHandDampNspm = 500.0F;
// The grip's hinge at the ankle: the leg swings in a 35 degree cone, twists
// +-20 (the cone opens wide while the body comes over into the hang, and
// closes only as far as the body already is).
constexpr float kGripConeRad = 0.61F;
constexpr float kGripTwistRad = 0.35F;
// Thrashing: the stick's speed (units a second) for a full thrash; a kick
// (a reversal of the stick, or a jump) is kKickNs along the body, away from
// her hand, at most every kKickGapTicks; a twist of kThrashTorqueNm.
constexpr double kThrashFullRate = 8.0;
constexpr float kThrashSmoothS = 0.25F;
constexpr float kKickNs = 25.0F;
constexpr int kKickGapTicks = 11;
// Of a kick's impulse, what comes onto her fingers as a slip (the strain).
constexpr float kKickSlipNs = 12.0F;
constexpr float kThrashTorqueNm = 40.0F;
// A thrash past this share of a full one is the player fighting her grip.
constexpr float kFoughtThrash = 0.15F;
// And the legs' pull against her hand while thrashing, along the body.
constexpr float kThrashPullN = 600.0F;
// The handback: a PD torque to upright, capped; the DOFs lock under
// kUprightDoneRad with no more than kUprightOverlapM of penetration.
constexpr float kUprightKp = 900.0F;
constexpr float kUprightKd = 120.0F;
constexpr float kUprightMaxNm = 600.0F;
constexpr float kUprightDoneRad = 0.14F;   // 8 degrees
constexpr float kUprightOverlapM = 0.02F;
constexpr float kUprightWedgedS = 2.0F;
constexpr float kDownFrictionMps2 = 6.9F;   // mu 0.7 on steel, a body down
constexpr int kUprightContactTicks = 3;
// The brace: the player's hands on a rail within kBraceReachM of the chest.
constexpr float kBraceReachM = 0.75F;
constexpr float kBraceChestUpM = 0.45F;
constexpr float kBraceStrainFloorN = 1000.0F;
} // namespace

void Simulation::PhysicsWorld::destroy_grab_constraint(JPH::Ref<JPH::TwoBodyConstraint> &constraint) noexcept {
    if (constraint != nullptr) {
        physics_system_.RemoveConstraint(constraint);
        constraint = nullptr;
    }
}

void Simulation::PhysicsWorld::set_player_free_rotation(JPH::BodyInterface &bodies, const bool free) noexcept {
    if (player_free_rot_ == free) {
        return;
    }
    {
        // Jolt sets the allowed DOFs with the mass properties
        // (MotionProperties::SetMassProperties): in place, the body's ID,
        // position and velocity kept. No recreate is needed.
        JPH::BodyLockWrite lock(physics_system_.GetBodyLockInterface(), player_id_);
        if (!lock.Succeeded()) {
            return;
        }
        JPH::Body &body = lock.GetBody();
        JPH::MassProperties mass = body.GetShape()->GetMassProperties();
        mass.ScaleToMass(kPlayerMassKg);
        const JPH::EAllowedDOFs dofs = free ? JPH::EAllowedDOFs::All
                                            : JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY |
                                                  JPH::EAllowedDOFs::TranslationZ;
        body.GetMotionProperties()->SetMassProperties(dofs, mass);
        if (!free) {
            body.SetAngularVelocity(JPH::Vec3::sZero());
        }
    }
    if (!free) {
        // Upright (within kUprightDoneRad, checked clear): the capsule is a
        // line about its own axis, so this is its last few degrees, no move.
        bodies.SetRotation(player_id_, JPH::Quat::sIdentity(), JPH::EActivation::Activate);
    }
    player_free_rot_ = free;
    upright_s_ = 0.0F;
    upright_contact_ticks_ = 0;
}

bool Simulation::PhysicsWorld::upright_pose_clear(const JPH::RVec3 centre) const noexcept {
    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
    JPH::CollideShapeSettings settings;
    settings.mMaxSeparationDistance = 0.0F;
    JPH::IgnoreMultipleBodiesFilter filter;
    filter.IgnoreBody(player_id_);
    if (!hand_id_.IsInvalid()) {
        filter.IgnoreBody(hand_id_);
    }
    JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(), player_id_);
    const JPH::Shape *shape = lock.Succeeded() ? lock.GetBody().GetShape() : player_shape_.GetPtr();
    physics_system_.GetNarrowPhaseQuery().CollideShape(shape, JPH::Vec3::sReplicate(1.0F),
                                                       JPH::RMat44::sTranslation(centre), settings, centre, collector,
                                                       {}, {}, filter);
    for (const JPH::CollideShapeResult &hit : collector.mHits) {
        if (hit.mPenetrationDepth > kUprightOverlapM) {
            return false;
        }
    }
    return true;
}

void Simulation::PhysicsWorld::tear_grip(const bool torn) noexcept {
    if (grip_ == nullptr) {
        return;
    }
    if (wrist_ != nullptr && matron_->pulling_wrist()) {
        // Her other hand has his wrist: once the rip has begun nothing breaks
        // it (Cory, 2026-10-27). Her ankle grip lost, however (his kick, his
        // thrashing, the world), the arm stays in her hand as the body pulls
        // away.
        JPH::BodyInterface &bodies = physics_system_.GetBodyInterface();
        sever_arm(bodies);
        matron_->arm_torn();
    }
    destroy_grab_constraint(grip_);
    grip_strain_ns_ = 0.0F;
    grip_over_ticks_ = 0;
    if (torn) {
        ++grip_tear_count_;
    } else {
        ++grip_release_count_;
    }
    destroy_grab_constraint(brace_);
    matron_->grip_lost(torn, grip_fought_);
}

void Simulation::PhysicsWorld::snag_player_for_test(const float max_n) noexcept {
    destroy_grab_constraint(snag_);
    const JPH::RVec3 at = physics_system_.GetBodyInterface().GetPosition(player_id_);
    JPH::PointConstraintSettings snag;
    snag.mSpace = JPH::EConstraintSpace::WorldSpace;
    snag.mPoint1 = at;
    snag.mPoint2 = at;
    JPH::BodyLockWrite lock(physics_system_.GetBodyLockInterface(), player_id_);
    if (lock.Succeeded()) {
        snag_ = static_cast<JPH::TwoBodyConstraint *>(snag.Create(JPH::Body::sFixedToWorld, lock.GetBody()));
    }
    if (snag_ != nullptr) {
        physics_system_.AddConstraint(snag_);
        snag_max_n_ = max_n;
    }
}

JPH::RVec3 Simulation::PhysicsWorld::player_ankle(const JPH::BodyInterface &bodies) const noexcept {
    return bodies.GetWorldTransform(player_id_) * JPH::Vec3(0.0F, -(kPlayerHalfHeight - 0.08F), 0.0F);
}

void Simulation::PhysicsWorld::grab_reset(JPH::BodyInterface &bodies) noexcept {
    destroy_grab_constraint(grip_);
    destroy_grab_constraint(brace_);
    destroy_grab_constraint(snag_);
    destroy_grab_constraint(hand_arm_);
    if (!hand_id_.IsInvalid()) {
        remove_and_destroy(bodies, hand_id_);
        hand_id_ = JPH::BodyID();
    }
    set_player_free_rotation(bodies, false);
    thrash_ = 0.0F;
    grip_strain_ns_ = 0.0F;
    brace_strain_ns_ = 0.0F;
    grip_load_n_ = 0.0F;
    brace_load_n_ = 0.0F;
}

// The stick and the buttons while she has (or reaches for) the player: the
// thrash read from them; held, nothing else of the player's moves happens.
bool Simulation::PhysicsWorld::grab_pre_step(JPH::BodyInterface &bodies, StepCommands &commands,
                                             const float dt) noexcept {
    const double sx = commands.move_input_x;
    const double sz = commands.move_input_z;
    const double rate = std::hypot(sx - last_stick_x_, sz - last_stick_z_) / std::max(1.0e-4F, dt);
    last_stick_x_ = sx;
    last_stick_z_ = sz;
    // The stick's speed, averaged (a flip is one tick's jump): full at kThrashFullRate.
    const float raw = static_cast<float>(std::min(30.0, rate / kThrashFullRate));
    kick_now_ = false;
    if (kick_cooldown_ticks_ > 0) {
        --kick_cooldown_ticks_;
    }
    const double stick = std::hypot(sx, sz);
    const bool reversal = stick > 0.6 && (sx * kick_dir_x_ + sz * kick_dir_z_) < -0.2 * stick;
    if (stick > 0.6) {
        if (reversal || (kick_dir_x_ == 0.0 && kick_dir_z_ == 0.0)) {
            kick_dir_x_ = sx / stick;
            kick_dir_z_ = sz / stick;
        }
    }
    const bool held = grip_ != nullptr;
    if (held && kick_cooldown_ticks_ == 0 && (reversal || commands.jump_requested)) {
        kick_now_ = true;
        kick_cooldown_ticks_ = kKickGapTicks;
        ++thrash_kick_count_;
    }
    thrash_avg_ += (raw - thrash_avg_) * std::min(1.0F, dt / kThrashSmoothS);
    thrash_ = std::min(1.0F, thrash_avg_);
    if (!held) {
        if (brace_ != nullptr && (commands.release_requested || !matron_->holding())) {
            destroy_grab_constraint(brace_);
        }
    }
    // A brace: the grab button with a rail at the chest, while her hand is on
    // them or reaching; let go with release.
    if ((held || matron_->reaching()) && commands.traversal_requested && brace_ == nullptr) {
        const JPH::RVec3 at = bodies.GetPosition(player_id_);
        const JPH::RVec3 chest = at + JPH::Vec3(0.0F, kBraceChestUpM, 0.0F);
        JPH::SphereShape probe(kBraceReachM);
        probe.SetEmbedded();
        JPH::ClosestHitCollisionCollector<JPH::CollideShapeCollector> collector;
        JPH::CollideShapeSettings settings;
        const JPH::DefaultBroadPhaseLayerFilter bp(physics_system_.GetObjectVsBroadPhaseLayerFilter(),
                                                   object_layers::kPlayer);
        const JPH::SpecifiedObjectLayerFilter only_static(object_layers::kStatic);
        physics_system_.GetNarrowPhaseQuery().CollideShape(&probe, JPH::Vec3::sReplicate(1.0F),
                                                           JPH::RMat44::sTranslation(chest), settings, chest,
                                                           collector, bp, only_static);
        if (collector.HadHit()) {
            const JPH::RVec3 rail = chest + collector.mHit.mContactPointOn2;
            JPH::PointConstraintSettings brace;
            brace.mSpace = JPH::EConstraintSpace::WorldSpace;
            brace.mPoint1 = rail;
            brace.mPoint2 = rail;
            JPH::BodyLockWrite lock(physics_system_.GetBodyLockInterface(), player_id_);
            if (lock.Succeeded()) {
                brace_ = static_cast<JPH::TwoBodyConstraint *>(brace.Create(JPH::Body::sFixedToWorld, lock.GetBody()));
            }
            if (brace_ != nullptr) {
                physics_system_.AddConstraint(brace_);
                ++brace_count_;
                brace_strain_ns_ = 0.0F;
            }
        }
    }
    if (brace_ != nullptr && commands.release_requested) {
        destroy_grab_constraint(brace_);
    }
    const bool ladder_grip = traversal_state_ == TraversalState::Climbing;
    if ((held && !ladder_grip) || player_free_rot_ || matron_->claimed()) {
        // Held, or not yet back on their feet: the body is hers, or the
        // ground's; only the thrash is theirs.
        commands.move_input_x = 0.0;
        commands.move_input_z = 0.0;
        commands.jump_requested = false;
        commands.traversal_requested = false;
        commands.up_requested = false;
        commands.crouch_held = false;
        commands.sprint_held = false;
        commands.pick_up_requested = false;
        commands.throw_requested = false;
        commands.rig_requested = false;
        commands.parachute_toggle_requested = false;
    }
    if (held && !ladder_grip) {
        release_carry();
        clear_traversal();
        set_player_free_rotation(bodies, true);
        parachute_deployed_ = false;
    }
    return held && !ladder_grip;
}

void Simulation::PhysicsWorld::grab_drive(JPH::BodyInterface &bodies, const float /*dt*/) noexcept {
    ladder_pull_force_n_ = 0.0F;
    ladder_pull_power_w_ = 0.0F;
    matron_->set_player_braced(brace_ != nullptr);
    matron_->set_thrash(grip_ != nullptr ? thrash_ : 0.0F);
    const bool holding = matron_->holding();
    if (!holding && !hand_id_.IsInvalid()) {
        // Her hand is her own again (the hold over, or a blow took her). A
        // let-go she made as the hold ended (the fatality's, with its toss
        // out; her flinch from his blow) is made here, with its push.
        JPH::Vec3 push;
        const bool let_go = matron_->take_let_go(push);
        tear_grip(false);
        if (let_go) {
            bodies.AddImpulse(player_id_, push * kPlayerMassKg);
        }
        destroy_grab_constraint(hand_arm_);
        remove_and_destroy(bodies, hand_id_);
        hand_id_ = JPH::BodyID();
    }
    if (!holding) {
        // No hand to let go with: no let-go left over for the next hold.
        JPH::Vec3 stale;
        (void)matron_->take_let_go(stale);
    }
    if (holding && hand_id_.IsInvalid()) {
        // Her hand starts where it is: at her shoulder.
        if (hand_shape_ == nullptr) {
            hand_shape_ = new JPH::SphereShape(0.06F);
        }
        const JPH::RVec3 shoulder = bodies.GetPosition(matron_->body()) + matron_->shoulder_local();
        JPH::BodyCreationSettings settings(hand_shape_, shoulder, JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic,
                                           object_layers::kHand);
        settings.mAllowedDOFs =
            JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY | JPH::EAllowedDOFs::TranslationZ;
        settings.mGravityFactor = 0.0F;
        settings.mAllowSleeping = false;
        settings.mLinearDamping = 0.0F;
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = Matron::kHandMassKg;
        settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
        hand_id_ = bodies.CreateAndAddBody(settings, JPH::EActivation::Activate);
        bodies.SetLinearVelocity(hand_id_, bodies.GetLinearVelocity(matron_->body()));
        JPH::DistanceConstraintSettings arm;
        arm.mSpace = JPH::EConstraintSpace::WorldSpace;
        arm.mPoint1 = shoulder;
        arm.mPoint2 = shoulder;
        arm.mMinDistance = 0.0F;
        arm.mMaxDistance = matron_->arm_reach_m();
        arm.mLimitsSpringSettings.mFrequency = 6.0F;
        arm.mLimitsSpringSettings.mDamping = 0.8F;
        hand_arm_ = create_constraint(arm, matron_->body(), hand_id_, false);
    }
    if (hand_id_.IsInvalid()) {
        arm_drive(bodies, 0.0F);
        fatality_drive(bodies, 0.0F);
        return;
    }
    if (hand_arm_ != nullptr) {
        static_cast<JPH::DistanceConstraint *>(hand_arm_.GetPtr())->SetDistance(0.0F, matron_->arm_reach_m());
    }
    const JPH::RVec3 hand = bodies.GetPosition(hand_id_);
    const JPH::Vec3 hand_v = bodies.GetLinearVelocity(hand_id_);
    // Her fingers close on the ankle when the hand is at it.
    const JPH::RVec3 player_at = bodies.GetPosition(player_id_);
    if (matron_->reaching() && grip_ == nullptr) {
        const JPH::RVec3 ankle = player_ankle(bodies);
        if (JPH::Vec3(ankle - hand).Length() <= Matron::kGripCloseM) {
            if (traversal_state_ != TraversalState::Climbing) set_player_free_rotation(bodies, true);
            // First a bare point: the body comes over into the hang about it
            // (the hinge's cone is taken on once it is in it).
            JPH::PointConstraintSettings grip;
            grip.mSpace = JPH::EConstraintSpace::WorldSpace;
            grip.mPoint1 = hand;
            grip.mPoint2 = hand;
            grip_ = create_constraint(grip, hand_id_, player_id_, false);
            grip_swing_ = false;
            if (grip_ != nullptr) {
                grip_on_body_ = JPH::Vec3(bodies.GetWorldTransform(player_id_).Inversed() * hand);
                ++grip_count_;
                grip_fought_ = false;
                grip_strain_ns_ = 0.0F;
                grip_peak_n_ = 0.0F;
                hold_last_at_ = player_at;
                matron_->grip_taken();
            }
        }
    }
    // Her muscle: the hand to its pose, capped; its reaction on her.
    const JPH::RVec3 target = matron_->hand_target();
    JPH::Vec3 force = kHandSpringNpm * JPH::Vec3(target - hand) +
                      kHandDampNspm * (matron_->hand_target_velocity() - hand_v);
    // She holds the weight in her hand (the body's, while she has it).
    force += JPH::Vec3(0.0F, (grip_ != nullptr ? kPlayerMassKg : 0.0F) * 9.81F * 0.9F, 0.0F);
    float cap = matron_->hand_pull_n();
    const bool ladder_pull = traversal_state_ == TraversalState::Climbing;
    if (ladder_pull) {
        cap = std::min(cap, std::max(0.0F, Matron::kLadderPullMaxForceN - matron_->foot_force_n()));
    }
    if (force.Length() > cap) {
        force *= cap / force.Length();
    }
    if (ladder_pull) {
        const JPH::Vec3 relative_v = hand_v - bodies.GetLinearVelocity(matron_->body());
        const float power = force.Dot(relative_v);
        const float available = std::max(0.0F, Matron::kLadderPullMaxPowerW - matron_->foot_power_w());
        if (power > available) force *= available / power;
        ladder_pull_force_n_ = force.Length() + matron_->foot_force_n();
        ladder_pull_power_w_ = std::max(0.0F, force.Dot(relative_v)) + matron_->foot_power_w();
    }
    bodies.AddForce(hand_id_, force);
    bodies.AddForce(matron_->body(), -force);
    if (grip_ != nullptr) {
        // In the hang (the body within the cone of her hand's down), the
        // grip takes on its hinge: a 35 degree swing cone, +-20 twist, made
        // where the body already is (no snap); the point stays where it is.
        const JPH::Quat rot = bodies.GetRotation(player_id_);
        const JPH::Vec3 up = rot.RotateAxisY();
        if (!grip_swing_ && -up.GetY() >= std::cos(kGripConeRad - 0.05F)) {
            const JPH::RVec3 pivot = bodies.GetPosition(hand_id_);
            JPH::SwingTwistConstraintSettings hinge;
            hinge.mSpace = JPH::EConstraintSpace::WorldSpace;
            hinge.mPosition1 = pivot;
            hinge.mPosition2 = pivot;
            hinge.mTwistAxis1 = -JPH::Vec3::sAxisY();
            hinge.mTwistAxis2 = up;
            const JPH::Vec3 x = JPH::Vec3::sAxisX();
            hinge.mPlaneAxis1 = x;
            JPH::Vec3 plane2 = x - up * x.Dot(up);
            hinge.mPlaneAxis2 = plane2.Length() > 1.0e-3F ? plane2.Normalized() : up.GetNormalizedPerpendicular();
            hinge.mNormalHalfConeAngle = kGripConeRad;
            hinge.mPlaneHalfConeAngle = kGripConeRad;
            hinge.mTwistMinAngle = -kGripTwistRad;
            hinge.mTwistMaxAngle = kGripTwistRad;
            JPH::Ref<JPH::TwoBodyConstraint> made = create_constraint(hinge, hand_id_, player_id_, false);
            if (made != nullptr) {
                destroy_grab_constraint(grip_);
                grip_ = made;
                grip_swing_ = true;
                grip_on_body_ = JPH::Vec3(bodies.GetWorldTransform(player_id_).Inversed() * pivot);
            }
        }
        if (grip_swing_) {
            // The fatality's grip is firm on the ankle: his swing about her
            // fingers meets their friction (Matron::kFatalGripFrictionNm).
            static_cast<JPH::SwingTwistConstraint *>(grip_.GetPtr())
                ->SetMaxFrictionTorque(matron_->fatal_holding() ? Matron::kFatalGripFrictionNm : 0.0F);
        }
        // Fought: a kick, a thrash, a brace (a twitch of the stick is not one).
        grip_fought_ = grip_fought_ || kick_now_ || thrash_ > kFoughtThrash || brace_ != nullptr;
        // Thrash: a kick along the body, away from her hand; a twist.
        if (kick_now_) {
            bodies.AddImpulse(player_id_, up * kKickNs);
            // The heel wrenched in her fingers: they slip a little each time.
            grip_strain_ns_ += kKickSlipNs;
        }
        bodies.AddForce(player_id_, up * (kThrashPullN * thrash_));
        const JPH::Vec3 stick(static_cast<float>(last_stick_x_), 0.0F, static_cast<float>(last_stick_z_));
        JPH::Vec3 axis = up.Cross(stick);
        if (axis.Length() > 1.0e-3F) {
            bodies.AddTorque(player_id_, axis.Normalized() * (kThrashTorqueNm * thrash_));
        }
        // No teleport: the body's step each tick while held.
        const float step = static_cast<float>(JPH::Vec3(player_at - hold_last_at_).Length());
        max_hold_step_m_ = std::max(max_hold_step_m_, step);
        hold_last_at_ = player_at;
    }
    arm_drive(bodies, 0.0F);
    fatality_drive(bodies, 0.0F);
    JPH::Vec3 push;
    if (matron_->take_let_go(push)) {
        // She lets go: the push is her arm's, at most kReleasePushMps.
        tear_grip(false);
        bodies.AddImpulse(player_id_, push * kPlayerMassKg);
    }
}

bool Simulation::PhysicsWorld::body_down_on_ground(JPH::BodyInterface &bodies) const noexcept {
    // A body lying down (not on its feet: grounded_ is the feet's): steel
    // within a hand's breadth under its centre line.
    JPH::RayCastResult hit;
    return cast_ray(bodies.GetPosition(player_id_), JPH::Vec3(0.0F, -(kPlayerRadius + 0.15F), 0.0F), hit);
}

void Simulation::PhysicsWorld::grab_post_step(JPH::BodyInterface &bodies, const float dt) noexcept {
    arm_post_step(bodies, dt);
    fatality_post_step(bodies, dt);
    if (grip_ != nullptr) {
        const JPH::Vec3 lambda =
            grip_swing_ ? static_cast<JPH::SwingTwistConstraint *>(grip_.GetPtr())->GetTotalLambdaPosition()
                        : static_cast<JPH::PointConstraint *>(grip_.GetPtr())->GetTotalLambdaPosition();
        grip_load_n_ = lambda.Length() / std::max(dt, 1.0e-4F);
        grip_peak_n_ = std::max(grip_peak_n_, grip_load_n_);
        grip_over_ticks_ = grip_load_n_ > Matron::kGripMaxForceN ? grip_over_ticks_ + 1 : 0;
        grip_strain_ns_ = std::max(0.0F, grip_strain_ns_ +
                                             std::max(0.0F, grip_load_n_ - matron_->grip_strain_floor_n()) * dt -
                                             Matron::kGripStrainDecay * dt);
        if (grip_over_ticks_ >= 2 || grip_strain_ns_ > Matron::kGripTearNs) {
            tear_grip(true);
        }
    } else {
        grip_load_n_ = 0.0F;
    }
    if (snag_ != nullptr &&
        static_cast<JPH::PointConstraint *>(snag_.GetPtr())->GetTotalLambdaPosition().Length() / std::max(dt, 1.0e-4F) >
            snag_max_n_) {
        destroy_grab_constraint(snag_);   // a test's snag (snag_player_for_test), pulled off
    }
    if (brace_ != nullptr) {
        auto *point = static_cast<JPH::PointConstraint *>(brace_.GetPtr());
        brace_load_n_ = point->GetTotalLambdaPosition().Length() / std::max(dt, 1.0e-4F);
        const float grip = one_arm() ? kOneArmGripScale : 1.0F;
        brace_strain_ns_ += std::max(0.0F, brace_load_n_ - kBraceStrainFloorN) * dt;
        if (brace_load_n_ > kLedgeGripMaxForceN * grip || brace_strain_ns_ > kLedgeGripTearImpulseNs * grip) {
            // Her pull wins: the hands come off the rail.
            ++brace_tear_count_;
            destroy_grab_constraint(brace_);
        }
    } else {
        brace_load_n_ = 0.0F;
    }
    if (player_free_rot_ && grip_ == nullptr && (grounded_ || body_down_on_ground(bodies))) {
        // A body down on the steel slides to a stop: the player's own
        // shape has no friction (its feet do that, and they are not
        // under it), so the ground's is applied here, mu ~0.7.
        JPH::Vec3 v = bodies.GetLinearVelocity(player_id_);
        const JPH::Vec3 flat(v.GetX(), 0.0F, v.GetZ());
        const float speed = flat.Length();
        if (speed > 1.0e-4F) {
            const float lose = std::min(speed, kDownFrictionMps2 * dt);
            v -= flat * (lose / speed);
            bodies.SetLinearVelocity(player_id_, v);
        }
        // The same friction along its length stops it turning about the
        // floor's normal: a rod of length L, 3 mu g / L. L is what lies
        // flat of it, at least its breadth.
        JPH::Vec3 w = bodies.GetAngularVelocity(player_id_);
        if (std::abs(w.GetY()) > 1.0e-4F) {
            const JPH::Vec3 axis = bodies.GetRotation(player_id_).RotateAxisY();
            const float length = 2.0F * bodies.GetShape(player_id_)->GetLocalBounds().GetExtent().GetY();
            const float flat_length =
                std::max(2.0F * kPlayerRadius, length * std::hypot(axis.GetX(), axis.GetZ()));
            const float lose = std::min(std::abs(w.GetY()), 3.0F * kDownFrictionMps2 / flat_length * dt);
            w.SetY(w.GetY() - std::copysign(lose, w.GetY()));
            bodies.SetAngularVelocity(player_id_, w);
        }
    }
    if (player_free_rot_ && grip_ == nullptr && !matron_->claimed()) {
        // The handback: on the ground (or a hold in hand), a capped PD torque
        // to upright; the DOFs lock once it is and the pose is clear.
        upright_contact_ticks_ = grounded_ || traversal_state_ != TraversalState::None || in_water_
                                     ? upright_contact_ticks_ + 1
                                     : 0;
        if (upright_contact_ticks_ >= kUprightContactTicks || upright_s_ > 0.0F) {
            upright_s_ += dt;
            const JPH::Quat rot = bodies.GetRotation(player_id_);
            const JPH::Vec3 up = rot.RotateAxisY();
            JPH::Vec3 axis = up.Cross(JPH::Vec3::sAxisY());
            const float s = axis.Length();
            const float angle = std::atan2(s, up.GetY());
            if (s < 1.0e-4F) {
                axis = angle > 1.0F ? JPH::Vec3(-facing_.GetZ(), 0.0F, facing_.GetX()) : JPH::Vec3::sZero();
            } else {
                axis /= s;
            }
            const JPH::Vec3 omega = bodies.GetAngularVelocity(player_id_);
            JPH::Vec3 torque = axis * (kUprightKp * angle) - omega * kUprightKd;
            if (torque.Length() > kUprightMaxNm) {
                torque *= kUprightMaxNm / torque.Length();
            }
            bodies.AddTorque(player_id_, torque);
            const bool clear = upright_pose_clear(bodies.GetPosition(player_id_));
            if (angle < kUprightDoneRad && clear) {
                ++upright_count_;
                set_player_free_rotation(bodies, false);
            } else if (upright_s_ > kUprightWedgedS && clear && angle < 0.6F) {
                // Wedged a long while but nearly there and clear: the get-up.
                ++upright_wedged_count_;
                ++upright_count_;
                set_player_free_rotation(bodies, false);
            }
        }
    }
}

} // namespace scraperx::sim
