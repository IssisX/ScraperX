#include "sim/simulation/world.hpp"
#include "sim/traversal_receiver.hpp"

namespace scraperx::sim {
using namespace detail;

namespace {
bool receiving_path_is_clear(const JPH::PhysicsSystem &system, JPH::BodyID player,
                             const JPH::Shape *shape, JPH::RVec3 from, JPH::RVec3 to) {
    const JPH::Vec3 movement(to-from);
    if (movement.IsNearZero()) return true;
    class Collector final : public JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> {
    public:
        explicit Collector(JPH::Vec3 movement) : movement_(movement) {}
        void AddHit(const JPH::ShapeCastResult &hit) override {
            const auto normal = -hit.mPenetrationAxis.NormalizedOr(JPH::Vec3::sZero());
            // Existing resting contact may separate or slide tangentially.
            // A new hit or motion deeper into any contact still blocks.
            if (hit.mFraction <= 0 && !normal.IsNearZero() && normal.Dot(movement_) >= 0) return;
            JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector>::AddHit(hit);
        }
    private:
        JPH::Vec3 movement_;
    } collector(movement);
    const JPH::RShapeCast sweep(shape,JPH::Vec3::sReplicate(1),
        JPH::RMat44::sTranslation(from),movement);
    const JPH::IgnoreSingleBodyFilter filter(player);
    system.GetNarrowPhaseQuery().CastShape(sweep,JPH::ShapeCastSettings(),from,
        collector,{},{},filter);
    return !collector.HadHit();
}
} // namespace

[[nodiscard]] bool Simulation::PhysicsWorld::try_top_out(JPH::BodyInterface &bodies) noexcept {
    if (physical_hands_->active() && !physical_hands_->compatible_hands()) return false;
    // Action may reaffirm this physical handoff, never change its owner.
    if (cargo_transfer_active_ || cargo_push_seconds_>=0.0F) return true;
    const JPH::RVec3 origin = bodies.GetPosition(player_id_);
    LedgeProbe probe;
    if (!facing_.IsNearZero()) {
        probe = probe_ledge(origin, facing_, kMantleMinimumRise, kMantleMaximumRise, true, true);
    }
    if (!probe.valid) {
        probe = probe_ledge(origin, traversal_normal_, kMantleMinimumRise, kMantleMaximumRise,
                            true, true);
    }
    if (!probe.valid) {
        return false;
    }
    if (!regression_fixtures_ || (cargo_net_ && traversal_body_ == cargo_net_->body()) ||
        entity_is_causal_section_support(traversal_entity_id_) ||
        entity_is_causal_section_support(probe.ledge_entity_id) ||
        // North-frame ladder top-outs retain their actual hands until
        // receiving footing; other legacy mantle/vault owners stay scoped.
        (traversal_entity_id_ == 1901 && probe.ledge_entity_id == 1901) ||
        (!probe.landing_body.IsInvalid() &&
         entity_is_causal_section_support(bodies.GetUserData(probe.landing_body)))) {
        return begin_cargo_transfer(bodies,probe,origin);
    }
    // Other sections retain their existing mantle controller.
    physical_hands_->clear();
    hands_[0].valid = false;
    hands_[1].valid = false;
    begin_mantle(bodies, probe, origin);
    return true;
}

[[nodiscard]] bool Simulation::PhysicsWorld::plank_entry_overlap_is_legal() const noexcept {
    // Retain only the already-started physical stroke. The leg continues
    // to own material, traction, reach and its remaining finite budget.
    return foot_push_purpose_ == FootPushPurpose::PlankJump &&
        physical_foot_push_->active() && !physical_foot_push_->stop_requested() &&
        !cargo_transfer_active_ && cargo_push_seconds_ < 0 &&
        (traversal_state_ == TraversalState::Climbing || traversal_state_ == TraversalState::Hanging) &&
        physical_hands_->compatible_hands() && !physical_hands_->radial_active() &&
        !physical_hands_->swinging() && deformable_plank_ && !crouched_ &&
        deformable_plank_->capture().broken_mask == foot_push_start_broken_mask_ &&
        deformable_plank_->fragment_supported(physical_foot_push_->readback().support);
}

[[nodiscard]] bool Simulation::PhysicsWorld::foot_overlap_is_legal() const noexcept {
    return plank_entry_overlap_is_legal() ||
        (cargo_transfer_active_ && traversal_plan_.kind == TraversalPlanKind::LedgeCatchTopOut &&
        traversal_state_ == TraversalState::Climbing && physical_hands_->active() &&
        physical_hands_->compatible_hands() && !physical_hands_->radial_active() &&
        !physical_hands_->swinging() && cargo_push_seconds_ < 0 &&
        foot_push_purpose_ == FootPushPurpose::RigidTransfer &&
        foot_push_contact_.valid && foot_push_contact_.body == traversal_plan_.receiver &&
        traversal_plan_.receiver_patch.contains(foot_push_contact_.subshape));
}

void Simulation::PhysicsWorld::record_foot_contact(const JPH::BodyInterface &bodies, const SupportSample &support) noexcept {
    foot_contact_ = {};
    if (!support.grounded || !support.bottom_contact || support.body_id.IsInvalid() ||
        net_for_body(support.body_id) || support.normal_y < kSupportNormalThreshold) return;
    foot_contact_.valid = true;
    foot_contact_.body = support.body_id;
    foot_contact_.subshape = support.subshape;
    foot_contact_.entity_id = support.entity_id;
    foot_contact_.solved_tick = contact_solve_serial_;
    foot_contact_.local_point = support.material_point;
    foot_contact_.world_point = from_support_local(bodies, support.body_id, support.material_point);
    foot_contact_.local_normal = support.material_normal;
    foot_contact_.world_normal = bodies.GetRotation(support.body_id) * support.material_normal;
    foot_contact_.point_velocity = support_point_velocity(bodies, support.body_id, foot_contact_.world_point);
    foot_contact_.friction = support.friction;
    foot_contact_.shape_identity = support.shape_identity;
}

void Simulation::PhysicsWorld::refresh_rigid_transfer_foot() noexcept {
    rigid_transfer_foot_valid_ = false;
    if ((foot_push_purpose_ != FootPushPurpose::RigidTransfer &&
         foot_push_purpose_ != FootPushPurpose::TrailingPlant &&
         foot_push_purpose_ != FootPushPurpose::CoyotePush &&
         foot_push_purpose_ != FootPushPurpose::WallKick &&
         foot_push_purpose_ != FootPushPurpose::WallStride &&
         foot_push_purpose_ != FootPushPurpose::WalkingStep) || !physical_foot_push_->active() ||
        crouched_ || !foot_push_contact_.valid) return;
    JPH::Vec3 walking_normal = JPH::Vec3::sZero();
    bool walking_top = false;
    {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(), foot_push_contact_.body);
        rigid_transfer_foot_valid_ = lock.Succeeded() && lock.GetBody().IsInBroadPhase() &&
            !lock.GetBody().IsSensor() && lock.GetBody().GetShape() == foot_push_contact_.shape_identity &&
            lock.GetBody().GetUserData() == foot_push_contact_.entity_id;
        if (rigid_transfer_foot_valid_ && foot_push_purpose_ == FootPushPurpose::WalkingStep) {
            const auto contact = lock.GetBody().GetCenterOfMassTransform()*foot_push_contact_.local_point;
            walking_top = walking_top_face_normal(lock.GetBody(),foot_push_contact_.subshape,contact,walking_normal);
        }
    }
    if (walking_top) (void)physical_foot_push_->refresh_walking_contact_normal(walking_normal);
    // The foot owner continuously checks real distance, traction and
    // compression itself. A new ray cannot renew this material anchor.
}

void Simulation::PhysicsWorld::try_begin_transfer_foot(JPH::BodyInterface &bodies, JPH::RVec3 receiving_goal) noexcept {
    if (traversal_plan_.foot_started || physical_foot_push_->active() ||
        !traversal_plan_.accepts(foot_contact_, contact_solve_serial_) || crouched_ ||
        (deformable_plank_ && deformable_plank_->logical_member_for_body(foot_contact_.body))) return;
    const auto relative = foot_contact_.local_point - traversal_plan_.receiver_local_target;
    // Bound the real contact to the accepted receiver footprint, rather
    // than accepting some other face of a large compound body.
    const auto world_offset = bodies.GetRotation(foot_contact_.body) * relative;
    if (std::hypot(world_offset.GetX(), world_offset.GetZ()) > kPlayerRadius) return;
    // Refresh BOTH coordinates from the same material frame at acquisition:
    // a kinematic support may have moved since the last completed solve.
    foot_contact_.world_point = from_support_local(bodies,foot_contact_.body,foot_contact_.local_point);
    foot_contact_.world_normal = bodies.GetRotation(foot_contact_.body) * foot_contact_.local_normal;
    // The leg extends along COM-to-material distance, including the inward
    // travel. Vertical rise alone leaves this compression-only row slack.
    const auto centre = bodies.GetCenterOfMassPosition(player_id_);
    const float remaining = JPH::Vec3(receiving_goal-foot_contact_.world_point).Length() -
        JPH::Vec3(centre-foot_contact_.world_point).Length();
    const float stroke = std::clamp(remaining, kLandingSkin, PhysicalFootPush::maximum_stroke_m());
    if (!physical_foot_push_->begin(foot_contact_.body, foot_contact_.world_point,
            foot_contact_.world_normal, foot_contact_.friction, stroke)) return;
    traversal_plan_.foot_started = true;
    foot_push_contact_ = foot_contact_;
    foot_push_purpose_ = FootPushPurpose::RigidTransfer;
    ++foot_push_start_count_;
    foot_push_started_this_tick_ = true;
    traversal_plan_.last_contact_tick = foot_contact_.solved_tick;
    traversal_plan_.phase = TraversalPhase::SharedHandFoot;
    refresh_rigid_transfer_foot();
}

bool Simulation::PhysicsWorld::begin_cargo_transfer(JPH::BodyInterface &bodies,const LedgeProbe &probe,
                          JPH::RVec3 origin) noexcept {
    if (physical_hands_->active() && !physical_hands_->compatible_hands()) return false;
    const auto shoulder=origin+JPH::Vec3(0,kClimbHandMid,0);
    const float shoulder_to_lip = JPH::Vec3(probe.ledge_point-shoulder).Length();
    if (shoulder_to_lip>kTransferShoulderReachM) return false;
    Lip lips[2];
    if(!probe_lip_pair(probe.ledge_point,probe.inward,probe.ledge_body,lips)) return false;
    // Revalidate a real receiving surface, including reconstructed hang
    // probes. This prospective ray only identifies the plan; it cannot
    // start the physical foot row.
    const auto receiver_origin = probe.landing_centre - JPH::Vec3(0,kPlayerHalfHeight-kLandingSupportProbeUp,0);
    const JPH::Vec3 down(0,-(kLandingSupportProbeUp+kLandingSupportTolerance),0);
    JPH::RayCastResult receiver_hit;
    if (!cast_ray(receiver_origin,down,receiver_hit) || receiver_hit.mBodyID != probe.landing_body ||
        !shape_pose_is_clear(player_shape_.GetPtr(),probe.landing_centre)) return false;
    const auto receiver_point = JPH::RRayCast(receiver_origin,down).GetPointOnRay(receiver_hit.mFraction);
    if (surface_normal(receiver_hit.mBodyID,receiver_hit.mSubShapeID2,receiver_point).GetY() < kSupportNormalThreshold)
        return false;
    TraversalAffordance candidate;
    candidate.rigid.body = probe.ledge_body;
    candidate.rigid.subshape = probe.ledge_sub_shape;
    candidate.rigid.entity_id = probe.ledge_entity_id;
    candidate.rigid.geometry_generation = std::uint64_t(probe.ledge_body.GetIndexAndSequenceNumber())+1;
    candidate.rigid.local_point = to_support_local(bodies,probe.ledge_body,probe.ledge_point);
    candidate.rigid.local_normal = bodies.GetRotation(probe.ledge_body).Conjugated()*JPH::Vec3::sAxisY();
    candidate.rigid.local_tangent = bodies.GetRotation(probe.ledge_body).Conjugated()*probe.inward.Cross(JPH::Vec3::sAxisY());
    candidate.receiver_body = probe.landing_body;
    candidate.receiver_subshape = receiver_hit.mSubShapeID2;
    if (!collect_receiver_surface_patch(physics_system_, JPH::RRayCast(receiver_origin,down),
            receiver_hit, candidate.receiver_patch)) return false;
    candidate.receiver_entity_id = bodies.GetUserData(probe.landing_body);
    candidate.grip_capable = candidate.receiver_footing_capable = true;
    {
        const JPH::BodyID ids[]{probe.ledge_body,probe.landing_body};
        const JPH::BodyLockMultiRead locks(physics_system_.GetBodyLockInterface(),ids,2);
        if (!locks.GetBody(0) || !locks.GetBody(1)) return false;
        candidate.rigid.shape_identity = locks.GetBody(0)->GetShape();
        candidate.receiver_shape_identity = locks.GetBody(1)->GetShape();
    }
    const auto generation = traversal_affordances_.remember(candidate,contact_solve_serial_);
    const auto *accepted = traversal_affordances_.refresh(generation,physics_system_,contact_solve_serial_);
    if (!accepted) return false;
    // A rejected top-out must retain the actual radial hang. Change the load
    // profile only after reach, grip, receiver and clearance gates all pass.
    if (!leave_radial_profile(bodies)) return false;
    hanging_posture_offset_m_ = 0.0F;
    for (unsigned hand=0;hand<2;++hand) {
        if (!physical_hands_->attach(hand,lips[hand].body,lips[hand].ledge)) {
            clear_traversal();
            return false;
        }
        hands_[hand].body=lips[hand].body;
        hands_[hand].local=to_support_local(bodies,lips[hand].body,lips[hand].ledge);
        hands_[hand].valid=true;
        ++hands_[hand].generation;
    }
    const bool from_hang = traversal_state_ == TraversalState::Hanging;
    traversal_state_=TraversalState::Climbing;
    traversal_normal_=probe.inward;
    traversal_body_=probe.ledge_body;
    traversal_entity_id_=probe.ledge_entity_id;
    traversal_local_hold_=to_support_local(bodies,traversal_body_,origin);
    traversal_target_body_=probe.landing_body;
    traversal_local_ledge_=to_support_local(bodies,traversal_body_,probe.ledge_point);
    traversal_local_target_=to_support_local(bodies,traversal_target_body_,probe.landing_centre);
    // CHOSEN480Ns/m for North ladder top-outs;400 for its longer hang
    // pull-up. Less dissipative delay, same1500N/hand and6600W bounds.
    const float damping = traversal_entity_id_ == 1901 ? (from_hang ? 400.0F : 480.0F) : 600.0F;
    physical_hands_->set_transfer_profile(damping);
    cargo_transfer_active_=true;
    traversal_plan_.latch(*accepted,traversal_local_target_,candidate.receiver_shape_identity,contact_solve_serial_);
    cargo_transfer_peak_hand_load_n_=0;
    // Attach/detach changes the load path without resetting rider velocity.
    return true;
}

[[nodiscard]] bool Simulation::PhysicsWorld::try_finish_cargo_transfer(JPH::BodyInterface &bodies,
                                            std::uint64_t event_tick) noexcept {
    if (!cargo_transfer_active_ || traversal_plan_.shared_solved_ticks == 0 ||
        state_.hand_foot_last_solve_tick != event_tick ||
        state_.hand_foot_last_hand_load_n <= 0 || state_.hand_foot_last_foot_load_n <= 0 ||
        !traversal_plan_.accepts(foot_contact_,contact_solve_serial_)) return false;
    const auto landing=from_support_local(bodies,traversal_target_body_,traversal_local_target_);
    if (!grounded_ || !footing_is_firm(bodies) ||
        support_sample_.body_id != traversal_target_body_ ||
        !shape_pose_is_clear(player_shape_.GetPtr(),landing)) return false;
    // This event must carry actual hand AND foot load, not just two active
    // constraints. Fresh receiver contact and firm footing complete it.
    // Onward movement need not settle at the planned aim point first.
    cargo_transfer_peak_hand_load_n_=std::max(cargo_transfer_peak_hand_load_n_,
        double((physical_hands_->hand_force(0)+physical_hands_->hand_force(1)).Length()));
    ++accepted_traversal_count_;
    ++state_.foot_transfer_count;
    state_.foot_transfer_tick=event_tick;
    state_.foot_transfer_support_entity_id=support_sample_.entity_id;
    state_.foot_transfer_peak_hand_load_n=cargo_transfer_peak_hand_load_n_;
    const bool preserve_foot = foot_push_purpose_ == FootPushPurpose::RigidTransfer && physical_foot_push_->active();
    // Firm receiving contact completes the pull-up command. Retain the actual
    // finite foot row and its compression, but do not keep extending its target.
    if (preserve_foot) physical_foot_push_->hold_target();
    clear_traversal(preserve_foot);
    return true;
}

void Simulation::PhysicsWorld::update_cargo_transfer(JPH::BodyInterface &bodies,double up_input,float dt) noexcept {
    {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(),traversal_plan_.receiver);
        if (!lock.Succeeded() || !lock.GetBody().IsInBroadPhase() || lock.GetBody().IsSensor() ||
            lock.GetBody().GetShape() != traversal_plan_.receiver_shape ||
            lock.GetBody().GetUserData() != traversal_plan_.receiver_entity) {
            traversal_plan_.phase = TraversalPhase::HandSupported;
            traversal_plan_.rejection = TraversalRejection::InvalidReceiver;
            return;
        }
    }
    cargo_transfer_peak_hand_load_n_=std::max(cargo_transfer_peak_hand_load_n_,
        double((physical_hands_->hand_force(0)+physical_hands_->hand_force(1)).Length()));
    const auto actual=bodies.GetPosition(player_id_);
    const auto landing=from_support_local(bodies,traversal_target_body_,traversal_local_target_);
    const double horizontal=std::hypot(landing.GetX()-actual.GetX(),landing.GetZ()-actual.GetZ());
    const auto ledge=from_support_local(bodies,traversal_body_,traversal_local_ledge_);
    if (!shape_pose_is_clear(player_shape_.GetPtr(),landing)) {
        traversal_plan_.rejection = TraversalRejection::BlockedClearance;
        return;
    }
    traversal_plan_.rejection = TraversalRejection::None;
    bool receiving = traversal_plan_.phase == TraversalPhase::ReceiverApproach || traversal_plan_.foot_started;
    if (!traversal_plan_.foot_started && !receiving)
        traversal_plan_.phase = TraversalPhase::AwaitingFootContact;
    // A deliberate top-out/Action authorizes the bounded muscle action.
    // Releasing the stick does not cancel that input transaction. Before
    // receiving clearance, pushing away pauses the real hand-supported lift.
    JPH::RVec3 hand_goal=landing;
    // Reconstruct inward from refreshed material points for both approach
    // and onward commands, including a receiver that has rotated.
    auto inward=JPH::Vec3(landing-ledge);
    inward.SetY(0);
    if (!inward.IsNearZero()) inward=inward.Normalized();
    if (up_input > kClimbInputDeadzone) {
        if (!inward.IsNearZero()) {
            const auto onward=landing+inward*(kPlayerRadius*float(std::min(up_input,1.0)));
            const auto origin=onward-JPH::Vec3(0,kPlayerHalfHeight-kLandingSupportProbeUp,0);
            const JPH::Vec3 down(0,-(kLandingSupportProbeUp+kLandingSupportTolerance),0);
            if (shape_pose_is_clear(player_shape_.GetPtr(),onward) &&
                receiver_supports_continuation(physics_system_,JPH::RRayCast(origin,down),
                    traversal_plan_,player_id_,kSupportNormalThreshold)) {
                hand_goal=onward;
            }
        }
    }
    const auto receiving_goal=hand_goal+JPH::Vec3(0,0.02F,0);
    if (receiving || actual.GetY()-kPlayerHalfHeight >= ledge.GetY()+0.04F) {
        if (!shape_pose_is_clear(player_shape_.GetPtr(),receiving_goal) ||
            !receiving_path_is_clear(physics_system_,player_id_,player_shape_.GetPtr(),actual,receiving_goal)) {
            traversal_plan_.rejection = TraversalRejection::BlockedClearance;
            return;
        }
        // Clearance admits only a bounded target command, never footing.
        // Keep lowering after subsequent steering so a raised hand target
        // cannot strand the body above the receiver. Release still cancels.
        receiving = true;
        if (!traversal_plan_.foot_started) traversal_plan_.phase = TraversalPhase::ReceiverApproach;
    }
    // Request the finite leg stroke toward the accepted receiving goal,
    // never toward the higher pre-clearance hand-lift target.
    if (shape_pose_is_clear(player_shape_.GetPtr(),receiving_goal))
        try_begin_transfer_foot(bodies,receiving_goal);
    if (up_input < -kClimbInputDeadzone && !receiving) return;
    JPH::RVec3 goal=receiving ? receiving_goal : hand_goal+JPH::Vec3(0,0.15F,0);
    // Close the free space to the wall while lifting instead of first
    // pulling straight up at arm's length. Before sole clearance, the
    // target stays outside the face by the full capsule radius and skin.
    // The lip probe is inset from that face by kTopProbeInset.
    if (!receiving && actual.GetY()-kPlayerHalfHeight<ledge.GetY()+0.04 && horizontal>0.10 &&
        !inward.IsNearZero()) {
        const float outside = -(kTopProbeInset+kPlayerRadius+kLandingSkin);
        const float limit = std::max(outside,JPH::Vec3(actual-ledge).Dot(inward));
        const float excess = JPH::Vec3(goal-ledge).Dot(inward)-limit;
        if (excess>0) goal-=inward*excess;
    }
    auto lead=JPH::Vec3(goal-actual);
    if (lead.Length()>0.45F) lead*=0.45F/lead.Length();
    auto command=JPH::Vec3(actual+lead-physical_hands_->commanded_position());
    constexpr float transfer_speed=2.2F; // CHOSEN brief pull-up, not ladder climb.
    // Leave float-representation margin below the conservative work bound.
    const float stroke=0.999F*transfer_speed*dt;
    if (command.Length()>stroke) command*=stroke/command.Length();
    (void)physical_hands_->advance_targets(command,dt);
}

[[nodiscard]] Lip Simulation::PhysicsWorld::probe_lip(const JPH::RVec3 near, const JPH::Vec3 normal) const noexcept {
    Lip lip;
    const JPH::RVec3 outside =
        near - normal * 0.5F - JPH::Vec3(0.0F, kLipProbeDepth, 0.0F);
    const JPH::Vec3 across = normal * 0.9F;
    JPH::RayCastResult face_hit;
    if (!cast_ray(outside, across, face_hit)) {
        return lip;
    }
    const JPH::RVec3 face = JPH::RRayCast(outside, across).GetPointOnRay(face_hit.mFraction);
    const JPH::RVec3 above = face + normal * kTopProbeInset + JPH::Vec3(0.0F, 0.30F, 0.0F);
    const JPH::Vec3 down(0.0F, -0.45F, 0.0F);
    JPH::RayCastResult top_hit;
    if (!cast_ray(above, down, top_hit) || top_hit.mBodyID != face_hit.mBodyID) {
        return lip;
    }
    const JPH::RVec3 top = JPH::RRayCast(above, down).GetPointOnRay(top_hit.mFraction);
    if (surface_normal(top_hit.mBodyID, top_hit.mSubShapeID2, top).GetY() <
        kLedgeTopNormalThreshold) {
        return lip;
    }
    lip.valid = true;
    lip.body = face_hit.mBodyID;
    lip.sub_shape = top_hit.mSubShapeID2;
    lip.entity_id = physics_system_.GetBodyInterfaceNoLock().GetUserData(face_hit.mBodyID);
    lip.ledge = top;
    const JPH::RVec3 off_face = face - normal * (kPlayerRadius + kHangWallGap);
    lip.hold = JPH::RVec3(off_face.GetX(), top.GetY() - kHangDropBelowLedge, off_face.GetZ());
    const JPH::RVec3 inside = face + normal * kLandingInset;
    lip.landing =
        JPH::RVec3(inside.GetX(), top.GetY() + kPlayerHalfHeight + kLandingSkin, inside.GetZ());
    return lip;
}

void Simulation::PhysicsWorld::update_shimmy(JPH::BodyInterface &bodies, const StepCommands &commands,
                   const float delta_seconds) noexcept {
    // Climbing and Hanging are exclusive owners of this existing clock.
    // Idle/blocked ticks count too; it gates placement, not body actuation.
    const bool regrip_ready = regrip_cooldown_ticks_ == 0;
    if (regrip_cooldown_ticks_ > 0) --regrip_cooldown_ticks_;
    if (physical_hands_->active() && !physical_hands_->compatible_hands()) return;
    if (physical_hands_->radial_active()) return;
    const JPH::Vec3 right = traversal_right();
    const double side_input =
        commands.move_input_x * right.GetX() + commands.move_input_z * right.GetZ();
    if (std::abs(side_input) <= kClimbInputDeadzone || traversal_normal_.IsNearZero()) {
        return;
    }
    const std::array<PhysicalHandClimb::GripReadback,2> grips{
        physical_hands_->grip_readback(0),physical_hands_->grip_readback(1)};
    // This owner shimmies one continuous rigid ledge. Material climbing
    // and a handoff between separate bodies retain their existing owners.
    for (const auto &grip : grips)
        if (!grip.attached || grip.material || grip.support != traversal_body_) return;
    if (!actual_hands_in_reach(bodies)) {
        release_hang(bodies);
        return;
    }
    const float sign = side_input > 0.0 ? 1.0F : -1.0F;
    const float lead = side_input > 0.0 ? 0.25F : -0.25F;
    const JPH::RVec3 hold = from_support_local(bodies, traversal_body_, traversal_local_hold_);
    const JPH::RVec3 ledge = from_support_local(bodies, traversal_body_, traversal_local_ledge_);
    const JPH::Vec3 step = right * (static_cast<float>(side_input) * kShimmySpeed * delta_seconds);
    const Lip at = probe_lip(ledge + step, traversal_normal_);
    const Lip ahead = probe_lip(ledge + step + right * lead, traversal_normal_);
    if (!at.valid || !ahead.valid || at.body != traversal_body_ || ahead.body != traversal_body_ ||
        std::abs(at.ledge.GetY() - ledge.GetY()) > 0.15 ||
        !climb_move_clear(bodies.GetPosition(player_id_),
                          bodies.GetPosition(player_id_) + JPH::Vec3(at.hold - hold))) {
        return;
    }
    Lip pair[2];
    // Acquisition may use a narrower verified pair at a corner. Refresh that
    // same legal footprint while moving; a full-width-only probe would reject
    // the legitimate hold before the body could move inboard.
    if (!probe_lip_pair(at.ledge, traversal_normal_, at.body, pair)) return;
    for (const auto &lip : pair)
        if (std::abs(lip.ledge.GetY() - at.ledge.GetY()) > 0.15) return;
    // Recompute the actual trailing hand on every direction change. A
    // leading placement must remain distinct from the planted material
    // point; the existing0.35m climbing lead avoids coincident palms.
    const unsigned hand = JPH::Vec3(grips[0].world_anchor-grips[1].world_anchor).Dot(right)*sign <= 0 ? 0U : 1U;
    const Lip next = probe_lip(at.ledge + right*(sign*kClimbSideLead),traversal_normal_);
    const bool next_valid = next.valid && next.body == traversal_body_ &&
        std::abs(next.ledge.GetY()-at.ledge.GetY()) <= .15F;
    // Missing optional lead space stops placement, not valid movement
    // still supported by the existing planted pair.
    const bool place = regrip_ready && next_valid &&
        JPH::Vec3(next.ledge-grips[1U-hand].world_anchor).Dot(right)*sign >= .10F &&
        JPH::Vec3(next.ledge-bodies.GetCenterOfMassPosition(player_id_)).Length() <= 1.75F;
    // A cooldown cannot let the target run indefinitely beyond the real
    // planted pair. Reuse the existing attached-hand reach envelope.
    for (unsigned i=0;i<2;++i) {
        const auto anchor = place && i==hand ? next.ledge : grips[i].world_anchor;
        if (JPH::Vec3(anchor-at.hold).Length() > 1.75F) return;
    }
    if (place) {
        // replace validates first and preserves spring extension and
        // momentum. A rejected placement leaves both old rows intact.
        if (!physical_hands_->regrip(hand, traversal_body_, next.ledge)) return;
        ++hands_[hand].generation;
        hands_[hand].body = traversal_body_;
        hands_[hand].local = to_support_local(bodies, traversal_body_, next.ledge);
        hands_[hand].valid = true;
        regrip_cooldown_ticks_ = kClimbRegripTicks;
    }
    traversal_body_ = at.body;
    traversal_entity_id_ = at.entity_id;
    traversal_local_hold_ = to_support_local(bodies, at.body, at.hold);
    traversal_local_ledge_ = to_support_local(bodies, at.body, at.ledge);
    traversal_target_body_ = at.body;
    traversal_local_target_ = to_support_local(bodies, at.body, at.landing);
}

[[nodiscard]] Lip Simulation::PhysicsWorld::probe_edge_drop(const JPH::BodyInterface &bodies) const noexcept {
    if (!grounded_ || crouched_ || carry_constraint_ != nullptr || facing_.IsNearZero()) {
        return {};
    }
    const JPH::RVec3 origin = bodies.GetPosition(player_id_);
    const double feet_y = origin.GetY() - kPlayerHalfHeight;
    const JPH::Vec3 back = -facing_;
    for (float behind = kEdgeSearchStart; behind <= kEdgeSearchReach + 1.0e-4F;
         behind += kEdgeSearchStep) {
        const JPH::RVec3 at = origin + back * behind;
        const JPH::RVec3 from(at.GetX(), feet_y + 0.05, at.GetZ());
        JPH::RayCastResult floor;
        if (cast_ray(from, JPH::Vec3(0.0F, -(0.05F + kEdgeDropMinimum), 0.0F), floor)) {
            continue;
        }
        const JPH::RVec3 last = origin + back * (behind - kEdgeSearchStep);
        const Lip lip = probe_lip(JPH::RVec3(last.GetX(), feet_y, last.GetZ()), facing_);
        if (!lip.valid || std::abs(lip.ledge.GetY() - feet_y) > 0.10 ||
            !capsule_pose_is_clear(lip.hold)) {
            return {};
        }
        return lip;
    }
    return {};
}

[[nodiscard]] bool Simulation::PhysicsWorld::try_begin_lowering(JPH::BodyInterface &bodies) noexcept {
    const Lip lip = probe_edge_drop(bodies);
    if (!lip.valid) {
        return false;
    }
    const JPH::RVec3 origin = bodies.GetPosition(player_id_);
    traversal_state_ = TraversalState::Lowering;
    traversal_body_ = lip.body;
    traversal_entity_id_ = lip.entity_id;
    traversal_target_body_ = lip.body;
    traversal_normal_ = facing_;
    traversal_local_start_ = to_support_local(bodies, lip.body, origin);
    traversal_local_hold_ = to_support_local(bodies, lip.body, lip.hold);
    traversal_local_ledge_ = to_support_local(bodies, lip.body, lip.ledge);
    traversal_local_target_ = to_support_local(bodies, lip.body, lip.landing);
    traversal_progress_ = 0.0;
    traversal_duration_ = kLoweringSeconds;
    traversal_stall_ticks_ = 0;
    traversal_desired_ = origin;
    if(!regression_fixtures_ || entity_is_causal_section_support(lip.entity_id)) {
        bodies.SetGravityFactor(player_id_,1.0F);
        if(!acquire_hang_hands(bodies)) {
            clear_traversal();
            return false;
        }
    } else bodies.SetGravityFactor(player_id_, 0.0F);
    return true;
}

void Simulation::PhysicsWorld::update_physical_lowering(JPH::BodyInterface &bodies,float dt) noexcept {
    const auto actual=bodies.GetPosition(player_id_);
    const auto hold=from_support_local(bodies,traversal_body_,traversal_local_hold_);
    const auto lip=from_support_local(bodies,traversal_body_,traversal_local_ledge_);
    const double horizontal=std::hypot(hold.GetX()-actual.GetX(),hold.GetZ()-actual.GetZ());
    // Move the feet clear of the actual edge before asking for descent.
    // Finite hands do the work; collision and gravity remain enabled.
    auto goal=hold;
    if(horizontal>0.08) goal.SetY(std::max(actual.GetY(),lip.GetY()+kPlayerHalfHeight+0.05F));
    auto lead=JPH::Vec3(goal-actual);
    if(lead.Length()>0.28F) lead*=0.28F/lead.Length();
    auto command=JPH::Vec3(actual+lead-physical_hands_->commanded_position());
    if(command.Length()>kClimbUpSpeed*dt) command*=kClimbUpSpeed*dt/command.Length();
    (void)physical_hands_->advance_targets(command,dt);
    if(!grounded_ && JPH::Vec3(actual-hold).Length()<0.12F && actual.GetY()<lip.GetY())
        traversal_state_=TraversalState::Hanging;
}

void Simulation::PhysicsWorld::finish_lowering(JPH::BodyInterface &bodies) noexcept {
    traversal_state_ = TraversalState::Hanging;
    if (!acquire_hang_hands(bodies)) {
        abort_traversal(bodies);
        return;
    }
    traversal_progress_ = 0.0;
    traversal_stall_ticks_ = 0;
    ++accepted_traversal_count_;
}

} // namespace scraperx::sim
