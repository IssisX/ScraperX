#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

void Simulation::PhysicsWorld::OnStep(const JPH::PhysicsStepListenerContext &context) {
    // One listener owns every rider write. Jolt may run separate listeners
    // concurrently; serial dispatch keeps hand forces and launcher reads ordered.
    kit_->begin_weld_step(context.mDeltaTime);
    if (deformable_plank_) deformable_plank_->begin_collision_step(context.mDeltaTime);
    contact_listener_.begin_collision_step();
    soft_contact_listener_.begin_collision_step();
    if (cargo_net_) cargo_net_->pre_step(context.mDeltaTime);
    if (catch_net_) catch_net_->pre_step(context.mDeltaTime);
    if (traversal_line_) traversal_line_->collision_step(context.mDeltaTime);
    if (rigging_) rigging_->collision_step(context.mDeltaTime);
    physical_hands_->pre_step(context.mDeltaTime, true);
    for (auto &palm : physical_palms_) if (palm) palm->pre_step(context.mDeltaTime);
    if (grappling_gun_) grappling_gun_->pre_step(context.mDeltaTime);
    pre_step_physical_foot(context.mDeltaTime);
    slingshot_->collision_step(context);
    if (service_lift_) service_lift_->collision_step(context.mDeltaTime);
    if (supplied_ascent_) supplied_ascent_->collision_step(context.mDeltaTime);
}

void Simulation::PhysicsWorld::pre_step_physical_foot(const float delta_seconds) noexcept {
    // The same finite actuator law runs once per collision step in every
    // world. Slingshot worlds invoke this from their substep listener;
    // single-step worlds invoke it after lifecycle/face validation below.
    const bool rigid_foot = foot_push_purpose_ == FootPushPurpose::RigidTransfer ||
        foot_push_purpose_ == FootPushPurpose::TrailingPlant || foot_push_purpose_ == FootPushPurpose::CoyotePush ||
        foot_push_purpose_ == FootPushPurpose::WallKick || foot_push_purpose_ == FootPushPurpose::WallStride ||
        foot_push_purpose_ == FootPushPurpose::WalkingStep;
    const bool face_valid = rigid_foot
        ? rigid_transfer_foot_valid_ : deformable_plank_ && !crouched_ &&
            deformable_plank_->fragment_supported(physical_foot_push_->readback().support);
    physical_foot_push_->pre_step(delta_seconds, face_valid);
}

void Simulation::PhysicsWorld::step(const StepCommands &queued_commands,
          const float delta_seconds,
          const double next_time_seconds) noexcept {
    auto commands = queued_commands;
    // WO-008: captured before anything this tick can change grounded_, so
    // it means exactly "was the player standing on something one tick ago."
    second_hand_added_this_tick_ = false;
    const bool was_grounded_before_tick = grounded_;
    const bool was_soft_grounded_before_tick = grounded_ && net_for_body(support_sample_.body_id);
    const auto foot_transfers_before_tick = state_.foot_transfer_count;
    const auto traversal_before_commands = traversal_state_;
    if (commands.grapple_equip_requested && commands.grapple_equipped && grapple_acquired_ && traversal_line_) {
        traversal_line_->equip(false);
        if (traversal_state_ == TraversalState::LineSwing) traversal_state_ = TraversalState::None;
    }
    apply_grapple_commands(commands);
    if (commands.grapple_equip_requested && grapple_equipped_) clear_rigging_input();
    apply_rigging_commands(commands, delta_seconds);
    const bool line_release = traversal_line_gripping() && (commands.jump_requested || commands.release_requested);
    apply_traversal_line_commands(commands, delta_seconds);
    if (line_release) { commands.jump_requested = false; commands.traversal_requested = false; }
    // Drop cancels held automatic reach across every nearby body. A new
    // deliberate action, neutral stick or real footing rearms that intent.
    if (commands.release_requested) automatic_grip_cancelled_ = true;
    else if (grounded_ || commands.traversal_requested || commands.jump_requested ||
             std::hypot(commands.move_input_x,commands.move_input_z) <= kClimbInputDeadzone)
        automatic_grip_cancelled_ = false;
    foot_push_started_this_tick_ = false;
    if (grounded_) wall_stride_available_ = false;
    const bool jump_cancelled = commands.cancel_jump_requested ||
        commands.release_requested || commands.set_down_requested || commands.sling_drop;
    if (jump_cancelled || commands.crouch_held || physical_hands_->active() ||
        traversal_state_ != TraversalState::None || carry_constraint_ ||
        (slingshot_ && slingshot_->controls_player())) wall_stride_available_ = false;
    prepare_trailing_plant(commands);
    update_wall_run(physics_system_.GetBodyInterface(), commands);
    if (foot_push_purpose_ == FootPushPurpose::WalkingStep && physical_foot_push_->active() &&
        (jump_cancelled || commands.jump_requested || commands.crouch_held ||
         step_direction_.GetX()*commands.move_input_x+
         step_direction_.GetZ()*commands.move_input_z <= .001)) {
        physical_foot_push_->clear();
        foot_push_purpose_ = FootPushPurpose::None;
        physical_step_pending_ = false;
    }
    // An involuntary hand loss must not erase an already valid real foot.
    // Explicit cancel/death/restart still use full cleanup.
    if (!jump_cancelled && !commands.crouch_held && cargo_transfer_active_ &&
        foot_push_purpose_ == FootPushPurpose::RigidTransfer && physical_foot_push_->active() &&
        !physical_hands_->compatible_hands()) clear_traversal(true);
    if (jump_cancelled || commands.crouch_held ||
        (traversal_state_ != TraversalState::None && !foot_overlap_is_legal()))
        physical_foot_push_->clear();
    const bool passive_trailing_foot = foot_push_purpose_ == FootPushPurpose::TrailingPlant;
    const bool active_wall_stride = foot_push_purpose_ == FootPushPurpose::WallStride &&
        physical_foot_push_->active();
    const bool finishing_transfer_foot = traversal_before_commands == TraversalState::None &&
        foot_push_purpose_ == FootPushPurpose::RigidTransfer && physical_foot_push_->active();
    // Crouch cancels prior intent rather than deferring a surprise landing
    // jump. A fresh Jump still owns its established stand-and-jump request.
    if (jump_cancelled || (commands.crouch_held && !commands.jump_requested) ||
        traversal_before_commands != TraversalState::None ||
        (slingshot_ && slingshot_->controls_player()) ||
        (physical_foot_push_->active() && !finishing_transfer_foot && !passive_trailing_foot &&
         !active_wall_stride)) {
        ordinary_jump_ticks_left_ = 0;
    } else if (commands.jump_requested) {
        ordinary_jump_ticks_left_ = kOrdinaryJumpIntentTicks;
    }
    if (slingshot_) landing_recovery_seconds_ = std::max(0.0, landing_recovery_seconds_ - delta_seconds);
    landing_applied_force_ = JPH::Vec3::sZero();

    auto &bodies = physics_system_.GetBodyInterface();
    prepare_forklift(commands, bodies);
    prepare_yard_crane(commands, bodies);
    air_control_force_ = JPH::Vec3::sZero();
    const auto rider_velocity_before = bodies.GetLinearVelocity(player_id_);
    if (regression_fixtures_) {
        update_support_motion(bodies, delta_seconds, next_time_seconds);
        update_scoop(bodies, delta_seconds, next_time_seconds);
        update_plant(bodies, delta_seconds);
        update_jib(bodies, commands.jib_slew_input, commands.jib_hoist_input);
        update_needle(bodies, commands.needle_hoist_input);
        update_sump(bodies, delta_seconds, commands.valve_toggle_requested);
        update_water_screw(bodies, delta_seconds, commands.water_screw_toggle_requested);
        update_water_lift(bodies, delta_seconds,
                          commands.water_lift_valve_toggle_requested,
                          commands.water_lift_release_requested,
                          commands.water_lift_reset_requested);
        update_intake(bodies, commands.intake_slew_input, commands.intake_hoist_input);
        update_legal_forty(bodies, commands.intake_sling_release_requested,
                           commands.intake_sling_attach_requested);

    }

    // A toggle while airborne only: deploying/retracting on the ground is
    // meaningless and would let a grounded button-mash pre-arm the canopy.
    if (commands.parachute_toggle_requested && !was_grounded_before_tick) {
        parachute_deployed_ = !parachute_deployed_;
    }

    if (regrab_lockout_ticks_ > 0) {
        --regrab_lockout_ticks_;
    }
    facing_ = normalized_horizontal(commands.facing_x, commands.facing_z);
    hanging_posture_input_ = commands.crouch_held;

    if (slingshot_ && slingshot_->controls_player()) air_catch_action_ticks_left_ = 0;
    if (!slingshot_ || !slingshot_->controls_player()) {
        auto posture_commands = commands;
        // A buffered ordinary Jump may request a legitimate stand attempt;
        // only the fresh edge reaches contextual hand/vault arbitration.
        posture_commands.jump_requested = commands.jump_requested ||
            (ordinary_jump_ticks_left_ > 0 && grounded_ &&
             !support_sample_.body_id.IsInvalid());
        update_crouch(bodies, posture_commands);
        update_rig(bodies, commands);
        update_carry(bodies, commands, delta_seconds);
        update_palm_vault(bodies, commands);
        if (traversal_state_ != TraversalState::LineSwing) {
            apply_traversal_commands(bodies, commands);
            sync_radial_profile(bodies, commands);
        }
        if ((traversal_state_ != TraversalState::Climbing && traversal_state_ != TraversalState::Hanging) ||
            cargo_transfer_active_ || cargo_push_seconds_ >= 0.0F || !physical_hands_->active())
            hanging_posture_offset_m_ = 0.0F;
        if (traversal_state_ == TraversalState::Climbing) {
            update_climb(bodies, commands, delta_seconds);
        } else if (traversal_state_ == TraversalState::Hanging) {
            update_shimmy(bodies, commands, delta_seconds);
        }
    }

    if (state_.foot_transfer_count > foot_transfers_before_tick)
        state_.foot_transfer_tick = static_cast<std::uint64_t>(
            std::llround(next_time_seconds / Simulation::kFixedStepSeconds));

    const bool sling_was_controlling = slingshot_ && slingshot_->controls_player();
    if (slingshot_) slingshot_->pre_step(delta_seconds, commands.sling_draw,
        commands.sling_yaw, commands.sling_elevation, commands.sling_action, commands.sling_drop,
        grounded_ && support_entity_id_ == Slingshot::kPouchEntity);
    const bool sling_controlling = slingshot_ && slingshot_->controls_player();
    if (sling_was_controlling && !sling_controlling && slingshot_->state().released)
        slingshot_flight_ = true;
    if (sling_controlling) {
        release_carry();
        clear_traversal();
        parachute_deployed_ = false;
    }
    if (grounded_ && support_entity_id_ != Slingshot::kPouchEntity && !sling_controlling) slingshot_flight_ = false;
    if (sling_was_controlling || sling_controlling || jump_cancelled ||
        traversal_before_commands != TraversalState::None ||
        traversal_state_ != TraversalState::None) ordinary_jump_ticks_left_ = 0;
    if ((traversal_state_ != TraversalState::None && !foot_overlap_is_legal()) || carry_constraint_ != nullptr)
        physical_foot_push_->clear();
    publish_soft_foot_policy(commands);
    update_ground_slide(bodies, commands);
    bool jump_started = false;
    if (!sling_controlling && traversal_state_ == TraversalState::LineSwing && grounded_) {
        auto walking = commands;
        walking.jump_requested = false;
        (void)apply_locomotion(bodies, walking, delta_seconds);
    }
    if (!sling_controlling && traversal_state_ == TraversalState::None) {
        if (slingshot_flight_ && !grounded_) {
            // Air steering is a bounded force. It does not replace the
            // elastic launch velocity with the walking speed controller.
            bodies.AddForce(player_id_, JPH::Vec3(float(commands.move_input_x * kPlayerMassKg * 2.0),
                0.0F, float(commands.move_input_z * kPlayerMassKg * 2.0)));
            if (commands.move_input_x != 0.0 || commands.move_input_z != 0.0)
                slingshot_->note_external_influence();
        } else {
            jump_started = try_begin_coyote_push(bodies);
            if (!jump_started) jump_started = try_begin_wall_kick(bodies);
            const bool wall_jump_started = jump_started &&
                (foot_push_purpose_ == FootPushPurpose::WallKick ||
                 foot_push_purpose_ == FootPushPurpose::WallStride);
            if (!jump_started) (void)try_begin_wall_stride(bodies, commands);
            if (jump_started) physical_step_pending_ = false;
            auto locomotion_commands = commands;
            locomotion_commands.jump_requested = ordinary_jump_ticks_left_ > 0 &&
                grounded_ && !support_sample_.body_id.IsInvalid() && !crouched_ &&
                !physical_foot_push_->active();
            const bool ordinary_jump_requested = locomotion_commands.jump_requested;
            jump_started = apply_locomotion(bodies, locomotion_commands, delta_seconds) || jump_started;
            // Only an earned Jump arms a later wall step. A finite walking
            // riser also returns true, but grants no airborne wall action.
            if (jump_started && !wall_jump_started &&
                (ordinary_jump_requested || foot_push_purpose_ == FootPushPurpose::CoyotePush)) {
                wall_stride_available_ = true;
                arm_wall_run();
            }
            if (ordinary_jump_requested && jump_started) {
                ordinary_jump_ticks_left_ = 0;
                physical_step_pending_ = false;
            }
        }
        if (!jump_started && !palm_props_active()) {
            try_begin_hang(bodies, commands);
        }
        if (!was_grounded_before_tick) {
            apply_parachute_drag(bodies, delta_seconds);
            // Distinct from fall_peak_speed_mps_ (a running max, telemetry
            // only): this is overwritten every tick, so it always holds
            // exactly the velocity the body carries into this tick's
            // contact resolution -- what "impact speed" has to mean for
            // late deceleration (a well-timed parachute) to matter.
            const float vertical_speed = bodies.GetLinearVelocity(player_id_).GetY();
            pre_contact_fall_speed_mps_ = std::max(0.0F, -vertical_speed);
            fall_peak_speed_mps_ =
                std::max(fall_peak_speed_mps_, pre_contact_fall_speed_mps_);
        }
    }

    if (traversal_state_ != TraversalState::None) {
        ordinary_jump_ticks_left_ = 0;
        physical_step_pending_ = false;
    }
    // A fresh onward Jump can wait for the already finite transfer row. It
    // cannot stack a second push, restart that row or extend its lifetime.
    else if (ordinary_jump_ticks_left_ > 0 && !finishing_transfer_foot) --ordinary_jump_ticks_left_;

    if (traversal_state_ == TraversalState::Lowering && physical_hands_->active())
        update_physical_lowering(bodies,delta_seconds);

    // A fresh airborne catch may have acquired hands later in this tick.
    if (traversal_state_ != TraversalState::LineSwing) sync_radial_profile(bodies, commands);
    if (traversal_state_ == TraversalState::Hanging && physical_hands_->active()) {
        // Request a bounded rest-target change, never the root velocity.
        // Measure commanded position rather than actual compliant sag.
        const auto goal = from_support_local(bodies, traversal_body_, traversal_local_hold_);
        // Absolute hold baseline; remove the delivered posture from its
        // error before combining the remaining posture stroke below.
        advance_hanging_posture(bodies, JPH::Vec3(goal - physical_hands_->commanded_position()) +
            JPH::Vec3(0, hanging_posture_offset_m_, 0), delta_seconds);
    }
    if (traversal_state_ != TraversalState::None && !physical_hands_->active()) {
        drive_traversal(bodies, delta_seconds);
        load_hold(bodies);
    }

    if (physical_hands_->active() && !was_grounded_before_tick) {
        // Finite hand catches still collide with real receivers. Sample
        // their incoming speed too, rather than reuse an earlier free fall.
        pre_contact_fall_speed_mps_ = std::max(0.0F, -bodies.GetLinearVelocity(player_id_).GetY());
        fall_peak_speed_mps_ = std::max(fall_peak_speed_mps_, pre_contact_fall_speed_mps_);
    }
    if (!slingshot_) {
        if (traversal_line_) traversal_line_->collision_step(delta_seconds);
        if (rigging_) rigging_->collision_step(delta_seconds);
        physical_hands_->pre_step(delta_seconds);
    }
    if (supplied_ascent_) {
        const auto selection=supplied_machine_station(bodies);
        if (!gravity_reclaim_retry_available_ && !jump_started &&
            selection.machine == 6 && selection.station == SuppliedAscent::Station::Lower &&
            support_entity_id_ == 1941 && footing_is_firm(bodies)) {
            // Capture the actual safe entry before the first feed/actuator
            // step. Ordinary firm-footing commits still preserve spent
            // material; only this explicit menu retry restores the entry.
            capture_machine_checkpoint(bodies, gravity_reclaim_retry_checkpoint_);
            gravity_reclaim_retry_position_ = bodies.GetPosition(player_id_);
            gravity_reclaim_retry_crouched_ = crouched_;
            gravity_reclaim_retry_available_ = true;
        }
        supplied_ascent_->pre_step(selection.machine,static_cast<float>(commands.supplied_machine_input),selection.station);
    }
    kit_->pre_step(delta_seconds);
    if (pipe_bridge_) pipe_bridge_->pre_step(delta_seconds);
    if (swing_stair_) swing_stair_->pre_step(delta_seconds);
    if (upper_ascent_) upper_ascent_->pre_step(delta_seconds);
    if (service_lift_) {
        const bool reachable = service_lift_station(bodies) != ServiceLift::Station::None;
        if (reachable && commands.service_lift_input != 0.0 && !service_lift_retry_available_) {
            // A named, explicit retry restores this complete checkpoint;
            // ordinary checkpoint commits continue preserving spent energy.
            service_lift_retry_checkpoint_ = checkpoint_;
            service_lift_retry_position_ = checkpoint_position_;
            service_lift_retry_crouched_ = checkpoint_crouched_;
            service_lift_retry_available_ = true;
        }
        service_lift_->pre_step(reachable ? static_cast<float>(commands.service_lift_input) : 0.0F);
    }
    contact_listener_.begin_tick();
    soft_contact_listener_.begin_tick();
    if (!slingshot_) kit_->begin_weld_step(delta_seconds);
    if (!landing_applied_force_.IsNearZero()) {
        // Ordinary step handling may prescribe a pose before solving.
        // Exclude that legacy displacement from the force-work receipt.
        landing_force_start_ = bodies.GetCenterOfMassPosition(player_id_);
        landing_force_support_start_ = JPH::RVec3(support_sample_.contact_point.x,
            support_sample_.contact_point.y, support_sample_.contact_point.z);
        if (const auto *net = net_for_body(landing_force_support_id_)) {
            landing_force_support_local_point_ = support_sample_.soft_material;
            landing_force_support_start_ = net->world_point(landing_force_support_local_point_);
        } else landing_force_support_local_point_ = to_support_local(bodies, landing_force_support_id_,
            landing_force_support_start_);
    }
    // Late contextual catches can start after ordinary locomotion. Remove
    // the leg before either hand/launcher authority solves this tick.
    if (((traversal_state_ != TraversalState::None || physical_hands_->active()) && !foot_overlap_is_legal()) ||
        carry_constraint_ != nullptr || sling_controlling)
        physical_foot_push_->clear();
    retain_trailing_plant(commands, jump_started);
    refresh_rigid_transfer_foot();
    // Hands/foot actions can acquire authority after locomotion. Only
    // tighten the published mode here: a consumed Jump must not re-enable
    // passive friction in the same tick as its push-off impulse.
    soft_foot_policy_.passive = soft_foot_policy_.passive && !jump_started &&
        passive_soft_feet_requested(commands);
    soft_foot_policy_.half_height = crouched_ ? kPlayerCrouchHalfHeight : kPlayerHalfHeight;
    // Hand/machine acquisition can occur late in this tick. One physical
    // interaction owns the player; remove the tool line before it solves.
    if (grappling_gun_ && (physical_hands_->active() || carry_constraint_ != nullptr || sling_controlling)) {
        grappling_gun_->release();
        grappling_gun_->set_reel_input(0);
        grapple_equipped_ = false;
    }
    const bool walking_step = foot_push_purpose_ == FootPushPurpose::WalkingStep;
    const double walking_work_before = physical_foot_push_->readback().motor_positive_work_j;
    auto receiver_patch = traversal_plan_.receiver_patch;
    auto receiver_body = cargo_transfer_active_ ? traversal_plan_.receiver : JPH::BodyID();
    if (walking_step) {
        receiver_patch = {};
        (void)receiver_patch.add(step_receiver_subshape_);
        receiver_body = step_receiver_;
    }
    contact_listener_.set_transfer_receiver(receiver_body,receiver_patch);
    if (!slingshot_) {
        pre_step_physical_foot(delta_seconds);
        for (auto &palm : physical_palms_) if (palm) palm->pre_step(delta_seconds);
    }
    prepare_story_actor(delta_seconds);
    prepare_yard_worker(delta_seconds);
    const auto update_result = physics_system_.Update(delta_seconds, slingshot_ ? 4 : 1,
        &temp_allocator_, &job_system_);
    // Existing player error policy is unchanged. Only successful native solves
    // may publish a new actor support receipt.
    finish_story_actor(update_result == JPH::EPhysicsUpdateError::None);
    finish_yard_worker(update_result == JPH::EPhysicsUpdateError::None);
    if (grappling_gun_) grappling_gun_->post_step();
    if (cargo_net_) cargo_net_->refresh();
    if (catch_net_) catch_net_->refresh();
    kit_->finish_weld_steps();
    if (deformable_plank_) deformable_plank_->finish_collision_steps();
    physical_foot_push_->post_step(delta_seconds / (slingshot_ ? 4.0F : 1.0F));
    if (walking_step)
        landing_jump_work_j_ += std::max(0.0,
            physical_foot_push_->readback().motor_positive_work_j-walking_work_before);
    if (physical_foot_push_->active() && foot_push_purpose_ == FootPushPurpose::PlankJump && deformable_plank_ &&
        (deformable_plank_->capture().broken_mask != foot_push_start_broken_mask_ ||
         !deformable_plank_->fragment_supported(physical_foot_push_->readback().support)))
        physical_foot_push_->clear(PhysicalFootPush::StopReason::MaterialFaceLost);
    physical_hands_->post_step(delta_seconds / (slingshot_ ? 4.0F : 1.0F));
    for (auto &palm : physical_palms_)
        if (palm) palm->post_step(delta_seconds / (slingshot_ ? 4.0F : 1.0F));
    if (physical_hands_->active() && physical_foot_push_->active() && foot_overlap_is_legal()) {
        const bool receiver_transfer = foot_push_purpose_ == FootPushPurpose::RigidTransfer;
        // An entry push acts on its original timber, not the receiver.
        // It cannot satisfy a later top-out plan's receiving-foot solve.
        if (receiver_transfer) ++traversal_plan_.shared_solved_ticks;
        ++state_.hand_foot_overlap_solved_ticks;
        state_.hand_foot_last_solve_tick = static_cast<std::uint64_t>(
            std::llround(next_time_seconds/Simulation::kFixedStepSeconds));
        state_.hand_foot_last_hand_load_n = physical_hands_->hand_force(0).Length()+
            physical_hands_->hand_force(1).Length();
        state_.hand_foot_last_foot_load_n = physical_foot_push_->readback().player_force.Length();
        if (receiver_transfer) traversal_plan_.phase = TraversalPhase::SharedHandFoot;
    }
    if (!physical_foot_push_->active()) foot_push_purpose_ = FootPushPurpose::None;
    if (cargo_transfer_active_ && traversal_plan_.foot_started && !physical_foot_push_->active())
        traversal_plan_.phase = TraversalPhase::HandSupported;
    if (!landing_applied_force_.IsNearZero()) {
        const auto support_point_now = from_support_local(bodies, landing_force_support_id_,
            landing_force_support_local_point_);
        const auto displacement = JPH::Vec3(bodies.GetCenterOfMassPosition(player_id_) - landing_force_start_) -
                                  JPH::Vec3(support_point_now - landing_force_support_start_);
        landing_recovery_work_j_ += std::max(0.0F, landing_applied_force_.Dot(displacement));
    }
    if (service_lift_) service_lift_->post_step();
    if (warehouse_forklift_) warehouse_forklift_->post_step();
    if (yard_crane_) yard_crane_->post_step();
    if (supplied_ascent_) supplied_ascent_->post_step();
    kit_->post_step(delta_seconds);
    if (rigging_) rigging_->post_step(delta_seconds / (slingshot_ ? 4.0F : 1.0F));
    if (traversal_line_) {
        traversal_line_->post_step();
        if (traversal_state_ == TraversalState::LineSwing && !traversal_line_gripping()) {
            traversal_state_ = TraversalState::None; traversal_body_ = JPH::BodyID();
            traversal_entity_id_ = 0; traversal_line_lean_ = JPH::Vec3::sZero();
        }
    }
    if (slingshot_) slingshot_->post_step(delta_seconds);

    const auto reclaim=contact_listener_.reclaim_impact();
    if(reclaim.speed>0){++state_.reclaim_impact_count;state_.reclaim_impact_speed_mps=reclaim.speed;
        state_.reclaim_impact_position={reclaim.position.GetX(),reclaim.position.GetY(),reclaim.position.GetZ()};}
    const auto fracture = kit_->weld_break_sample();
    state_.reclaim_break_serial = fracture.serial;
    state_.reclaim_break_valid = fracture.valid;
    state_.reclaim_break_position = {fracture.position.GetX(), fracture.position.GetY(), fracture.position.GetZ()};
    state_.reclaim_break_force_n = fracture.force_n;
    state_.reclaim_break_torque_nm = fracture.torque_nm;
    SupportSample support = contact_listener_.sample();
    const auto soft_support = observe_soft_support(bodies);
    // Actual current rigid footing wins at the rim; soft support only fills
    // an absent rigid stance. Rigid impact processing below is independent.
    if (!support.grounded && soft_support.grounded) support = soft_support;
    if ((jump_started && !foot_push_started_this_tick_) ||
        (traversal_state_ != TraversalState::None && !physical_hands_->active())) {
        support = {};
    }
    support_sample_ = support;
    ++contact_solve_serial_;
    const auto transfer_contact = contact_listener_.transfer_sample();
    record_foot_contact(bodies, ((cargo_transfer_active_ && support.grounded) || walking_step) &&
        transfer_contact.grounded ? transfer_contact : support);
    grounded_ = support.grounded;
    support_entity_id_ = support.entity_id;
    wall_contact_ = contact_listener_.wall_sample();
    wall_contact_.solved_tick = contact_solve_serial_;
    if (grounded_ || (wall_contact_.valid && wall_contact_.body != wall_kick_consumed_body_))
        wall_kick_consumed_body_ = JPH::BodyID();
    else if (!wall_kick_consumed_body_.IsInvalid()) {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(),wall_kick_consumed_body_);
        if (!lock.Succeeded() || !lock.GetBody().IsInBroadPhase())
            wall_kick_consumed_body_ = JPH::BodyID();
        else {
            const auto &wall = lock.GetBody();
            const auto point = wall.GetCenterOfMassTransform()*foot_push_contact_.local_point;
            const auto normal = wall.GetRotation()*foot_push_contact_.local_normal;
            const float capsule_extent = kPlayerRadius+
                (kPlayerHalfHeight-kPlayerRadius)*std::abs(normal.GetY());
            const float separation = JPH::Vec3(bodies.GetCenterOfMassPosition(player_id_)-point).Dot(normal);
            // A rejected foot-patch sample is not proof of separation. Rearm
            // only after the real capsule has cleared its retained wall face.
            if (separation > capsule_extent+kLandingSkin+kStepLookahead)
                wall_kick_consumed_body_ = JPH::BodyID();
        }
    }
    bool walking_receiver_contact = false;
    if (walking_step && physical_step_pending_ && foot_contact_.valid &&
        foot_contact_.solved_tick == contact_solve_serial_ && foot_contact_.body == step_receiver_ &&
        foot_contact_.subshape == step_receiver_subshape_) {
        const auto receiving = from_support_local(bodies,step_receiver_,step_receiver_local_point_);
        // Retire the leg only when the actual receiving normal is shallow
        // enough for the existing ground traction law to carry gravity.
        // Waiting for flat soles overextends a rear leg on short treads;
        // the first steep corner brush instead releases it too early.
        const float traction = std::min(foot_contact_.friction,
            entity_is_causal_section_support(foot_contact_.entity_id) ? .8F : .85F);
        const float carrying_normal_y = 1.0F/std::sqrt(1.0F+traction*traction);
        walking_receiver_contact = foot_contact_.world_point.GetY() >= receiving.GetY()-kLandingSkin &&
            foot_contact_.world_normal.GetY() >= carrying_normal_y;
        if (walking_receiver_contact) {
            physical_foot_push_->clear();
            foot_push_purpose_ = FootPushPurpose::None;
            physical_step_pending_ = false;
            ++step_up_count_;
        }
    }
    if (walking_step && !physical_foot_push_->active() && !walking_receiver_contact) {
        physical_step_pending_ = false;
        step_receiver_ = JPH::BodyID();
        foot_push_purpose_ = FootPushPurpose::None;
    }
    if(grounded_ && physical_step_pending_ && !walking_step) {
        const auto point = JPH::RVec3(support.contact_point.x,
            support.contact_point.y, support.contact_point.z);
        const auto relative = bodies.GetLinearVelocity(player_id_) -
            support_point_velocity(bodies, support.body_id, point);
        // A capsule brushing a riser while still rising is not the end of
        // its step. Firm footing or actual non-rising contact completes it;
        // narrow supports can therefore finish without a broad footprint.
        if (relative.GetY() <= 0 || footing_is_firm(bodies)) {
            if(bodies.GetPosition(player_id_).GetY()-kPlayerHalfHeight-
               physical_step_start_feet_y_>=kStepMinimumHeight) ++step_up_count_;
            physical_step_pending_=false;
        }
    }

    // WO-008: a genuine (non-traversal, non-jump) landing is the one
    // moment fall consequence is resolved. Traversal-completion landings
    // never reach here with a real fall velocity -- complete_traversal
    // always sets a support-relative landing velocity first -- so no
    // separate traversal exemption is needed.
    bool died_this_tick = false;
    const auto rigid_impact = contact_listener_.impact_sample();
    const bool ordinary_landing = grounded_ && !was_grounded_before_tick &&
        (traversal_state_ == TraversalState::None || physical_hands_->active()) && !jump_started;
    // Resolve actual upward rigid strikes even when cloth already owns the
    // stance. Support priority can never suppress a lethal rim/bottom hit.
    const bool rigid_strike = slingshot_ && rigid_impact.grounded && !sling_controlling &&
        (ordinary_landing || (was_soft_grounded_before_tick && rigid_impact.approach_normal_speed > .5F));
    if (rigid_strike) {
        record_landing(bodies, rigid_impact, commands.crouch_held);
        // Receiving an intended low step is walking, even when the rounded
        // capsule briefly loses bottom contact. Keep the real impact receipt;
        // only a harmless matched step skips a new stumble/slowdown interval.
        if (walking_receiver_contact && rigid_impact.approach_normal_speed < kJumpSpeed)
            clear_landing_recovery();
        last_impact_speed_mps_ = rigid_impact.approach_normal_speed;
    } else if (ordinary_landing && !slingshot_) {
        last_impact_speed_mps_ = pre_contact_fall_speed_mps_;
    }
    measure_catch_load(rider_velocity_before, delta_seconds, bodies, soft_support);
    if (catch_net_) catch_net_->post_step(delta_seconds, soft_contact_listener_.catch_touched());
    const bool lethal_rigid = slingshot_ ?
        rigid_impact.grounded && rigid_impact.approach_normal_speed > kLethalImpactSpeedMps :
        ordinary_landing && last_impact_speed_mps_ > kLethalImpactSpeedMps;
    if (lethal_rigid || catch_net_state_.overloaded) {
        restore_from_checkpoint(bodies);
        died_this_tick = true;
    }
    // Contact and both actuators have now solved. Finish on this evidence
    // outside Update, without an extra pre-step tick or another command.
    if (!died_this_tick && cargo_transfer_active_)
        (void)try_finish_cargo_transfer(bodies,static_cast<std::uint64_t>(
            std::llround(next_time_seconds/Simulation::kFixedStepSeconds)));
    state_.checkpoint_footing_valid = !died_this_tick && grounded_ &&
        traversal_state_ == TraversalState::None && footing_is_firm(bodies);
    if (state_.checkpoint_footing_valid && (!physical_foot_push_->active() ||
        foot_push_purpose_ == FootPushPurpose::TrailingPlant)) {
        catch_episode_active_ = false;
        commit_checkpoint(bodies);
    }
    if (grounded_) {
        fall_peak_speed_mps_ = 0.0F;
        parachute_deployed_ = false;
    }

    if (traversal_state_ != TraversalState::None && !physical_hands_->active()) {
        resolve_traversal_outcome(bodies);
    }

    update_affordance(bodies);
    // Measure the actual completed tick, including native support and
    // harness impulses. Gravity alone creates no felt load. A checkpoint
    // restore is a discontinuity, not an acceleration event.
    rider_specific_acceleration_ = died_this_tick ? JPH::Vec3::sZero() :
        (bodies.GetLinearVelocity(player_id_) - rider_velocity_before) / delta_seconds -
        physics_system_.GetGravity();
    read_state();
}

} // namespace scraperx::sim
