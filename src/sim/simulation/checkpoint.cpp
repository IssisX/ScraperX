#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

bool Simulation::PhysicsWorld::debug_restart_at(const Vector3 centre) noexcept {
    if (!std::isfinite(centre.x) || !std::isfinite(centre.y) || !std::isfinite(centre.z) ||
        centre.y < kPlayerHalfHeight - .02 || centre.y > Simulation::kTowerHeightMeters + 100.0 ||
        std::abs(centre.x) > 10000.0 || std::abs(centre.z) > 10000.0) return false;
    const JPH::RVec3 destination(centre.x, centre.y, centre.z);
    if (!shape_pose_is_clear(player_shape_.GetPtr(), destination, .02F)) return false;
    auto &bodies = physics_system_.GetBodyInterface();
    if (slingshot_) slingshot_->cancel_player_interaction();
    if (grappling_gun_) grappling_gun_->clear_transients();
    grapple_equipped_ = false;
    slingshot_flight_ = false;
    causal_airborne_ = false;
    physical_step_pending_ = false;
    wall_contact_ = {};
    wall_kick_consumed_body_ = JPH::BodyID();
    step_receiver_ = JPH::BodyID();
    release_carry();
    clear_traversal();
    crouched_ = false;
    reset_soft_foot_policy();
    bodies.SetShape(player_id_, player_shape_.GetPtr(), false, JPH::EActivation::Activate);
    bodies.SetPositionAndRotation(player_id_, destination, JPH::Quat::sIdentity(), JPH::EActivation::Activate);
    bodies.SetLinearAndAngularVelocity(player_id_, JPH::Vec3::sZero(), JPH::Vec3::sZero());
    rider_specific_acceleration_ = JPH::Vec3::sZero();
    clear_catch_history();
    trailing_foot_contact_tick_ = 0;
    grounded_ = false;
    state_.checkpoint_footing_valid = false;
    air_control_force_ = JPH::Vec3::sZero();
    support_entity_id_ = 0;
    support_sample_ = {};
    airborne_inherited_velocity_ = JPH::Vec3::sZero();
    parachute_deployed_ = false;
    fall_peak_speed_mps_ = pre_contact_fall_speed_mps_ = last_impact_speed_mps_ = 0.0F;
    clear_landing_recovery();
    jump_vault_ticks_left_ = regrab_lockout_ticks_ = ordinary_jump_ticks_left_ = 0;
    // An explicit debug relocation creates its own retry point. Ordinary
    // walking still commits only on verified firm footing.
    checkpoint_position_ = destination;
    checkpoint_crouched_ = false;
    commit_machine_checkpoint(bodies);
    update_affordance(bodies);
    read_state();
    return true;
}

bool Simulation::PhysicsWorld::select_service_lift_retry() noexcept {
    if (!service_lift_retry_available_) return false;
    checkpoint_ = service_lift_retry_checkpoint_;
    checkpoint_position_ = service_lift_retry_position_;
    checkpoint_crouched_ = service_lift_retry_crouched_;
    return true;
}

[[nodiscard]] bool Simulation::PhysicsWorld::can_restart_gravity_reclaim_attempt() const noexcept {
    return gravity_reclaim_retry_available_;
}

bool Simulation::PhysicsWorld::select_gravity_reclaim_retry() noexcept {
    if (!gravity_reclaim_retry_available_) return false;
    checkpoint_ = gravity_reclaim_retry_checkpoint_;
    checkpoint_position_ = gravity_reclaim_retry_position_;
    checkpoint_crouched_ = gravity_reclaim_retry_crouched_;
    return true;
}

bool Simulation::PhysicsWorld::restart_checkpoint() noexcept {
    auto &bodies = physics_system_.GetBodyInterface();
    release_carry();
    clear_traversal();
    const auto deaths = death_count_;
    restore_from_checkpoint(bodies);
    death_count_ = deaths; // A menu restart is not a fatal fall.
    update_affordance(bodies);
    read_state();
    return true;
}

void Simulation::PhysicsWorld::capture_machine_checkpoint(const JPH::BodyInterface &bodies,
                                MachineCheckpoint &checkpoint) const noexcept {
    checkpoint.support_body = JPH::BodyID();
    checkpoint.support_entity = 0;
    checkpoint.support_local_contact = JPH::Vec3::sZero();
    if (grounded_ && !support_sample_.body_id.IsInvalid()) {
        const auto support = kit_->body_for_entity(support_sample_.entity_id);
        // Kit restores these exact dynamic bodies, including their pose
        // and momentum. Nonrewound kinematic fixtures provide no saved
        // transport, and a nearby body is never a substitute for contact.
        if (support.valid() && kit_->body_dynamic(support) && kit_->body_enabled(support) &&
            kit_->body_id(support) == support_sample_.body_id) {
            checkpoint.support_body = support_sample_.body_id;
            checkpoint.support_entity = support_sample_.entity_id;
            const JPH::RVec3 point(support_sample_.contact_point.x,
                                  support_sample_.contact_point.y,
                                  support_sample_.contact_point.z);
            checkpoint.support_local_contact =
                bodies.GetCenterOfMassTransform(checkpoint.support_body).Inversed() * point;
        }
    }
    if (!regression_fixtures_) {
        checkpoint.carrying_entity = carried_entity_;
        kit_->capture(checkpoint.kit);
        if (deformable_plank_) checkpoint.deformable_plank = deformable_plank_->capture();
        if (pipe_bridge_) checkpoint.pipe_bridge = pipe_bridge_->state();
        if (slingshot_) checkpoint.slingshot = slingshot_->state();
        if (swing_stair_) checkpoint.swing_stair = swing_stair_->state();
        if (upper_ascent_) checkpoint.upper_ascent = upper_ascent_->state();
        if (cargo_net_) checkpoint.cargo_net = cargo_net_->capture();
        if (catch_net_) checkpoint.catch_net = catch_net_->capture();
        if (service_lift_) checkpoint.service_lift = service_lift_->state();
        if (warehouse_forklift_) warehouse_forklift_->capture(checkpoint.forklift);
        if (yard_crane_) checkpoint.yard_crane = yard_crane_->state();
        if (supplied_ascent_) checkpoint.supplied_ascent = supplied_ascent_->capture();
        return;
    }
    checkpoint.ballast = capture_body(bodies, ballast_id_);
    checkpoint.tipper = capture_body(bodies, tipper_id_);
    checkpoint.valve_lever = capture_body(bodies, valve_lever_id_);
    checkpoint.treadle = capture_body(bodies, treadle_id_);
    checkpoint.lift_platform = capture_body(bodies, lift_platform_id_);
    checkpoint.counterweight = capture_body(bodies, counterweight_id_);
    checkpoint.jib_boom = capture_body(bodies, jib_boom_id_);
    checkpoint.jib_hook = capture_body(bodies, jib_hook_id_);
    if (jib_hoist_slider_ != nullptr) {
        checkpoint.jib_hoist_motor_state = jib_hoist_slider_->GetMotorState();
        checkpoint.jib_hoist_target_position = jib_hoist_slider_->GetTargetPosition();
        checkpoint.jib_hoist_target_velocity = jib_hoist_slider_->GetTargetVelocity();
    }
    checkpoint.crate = capture_body(bodies, crate_id_);
    checkpoint.needle_beam = capture_body(bodies, needle_beam_id_);
    checkpoint.needle_seated = needle_seated_;
    checkpoint.sump_volume_kg = sump_volume_kg_;
    checkpoint.sump_isolated = sump_isolated_;
    checkpoint.vessel_mass_kg = steam_plant_.state().vessel_mass_kg;
    checkpoint.cylinder_mass_kg = steam_plant_.state().cylinder_mass_kg;
    checkpoint.water_screw = water_screw_.state();
    checkpoint.water_lift_bucket_water_m3 = water_lift_bucket_water_m3_;
    checkpoint.water_lift_valve_open = water_lift_valve_open_;
    checkpoint.intake_swing_flight = capture_body(bodies, intake_swing_flight_id_);
    checkpoint.intake_cw_cradle = capture_body(bodies, intake_cw_cradle_id_);
    checkpoint.hook5_door = capture_body(bodies, hook5_door_id_);
    checkpoint.hook5_bar = capture_body(bodies, hook5_bar_id_);
    checkpoint.hook5_block = capture_body(bodies, hook5_block_id_);
    checkpoint.carrying_entity = carried_entity_;
    checkpoint.intake_pack_slung = intake_pack_slung_;
    kit_->capture(checkpoint.kit);
}

void Simulation::PhysicsWorld::commit_machine_checkpoint(const JPH::BodyInterface &bodies) noexcept {
    capture_machine_checkpoint(bodies, checkpoint_);
}

void Simulation::PhysicsWorld::commit_checkpoint(const JPH::BodyInterface &bodies) noexcept {
    checkpoint_position_ = bodies.GetPosition(player_id_);
    checkpoint_crouched_ = crouched_;
    commit_machine_checkpoint(bodies);
    ++checkpoint_commit_count_;
}

void Simulation::PhysicsWorld::restore_from_checkpoint(JPH::BodyInterface &bodies) noexcept {
    clear_forklift_commands();
    if (grappling_gun_) grappling_gun_->clear_transients();
    grapple_equipped_ = false;
    clear_traversal();
    if (slingshot_) slingshot_->detach();
    slingshot_flight_ = false;
    causal_airborne_ = false;
    physical_step_pending_ = false;
    wall_contact_ = {};
    wall_kick_consumed_body_ = JPH::BodyID();
    step_receiver_ = JPH::BodyID();
    // The capsule the checkpoint was committed in: a crouched commit's
    // centre is a crouched centre, and may sit under a low ceiling.
    crouched_ = checkpoint_crouched_;
    reset_soft_foot_policy();
    bodies.SetShape(player_id_, active_player_shape(), false, JPH::EActivation::Activate);
    bodies.SetPositionAndRotation(player_id_, checkpoint_position_, JPH::Quat::sIdentity(),
                                  JPH::EActivation::Activate);
    bodies.SetLinearAndAngularVelocity(player_id_, JPH::Vec3::sZero(), JPH::Vec3::sZero());

    rider_specific_acceleration_ = JPH::Vec3::sZero();

    if (regression_fixtures_) {
        restore_body(bodies, ballast_id_, checkpoint_.ballast);
        restore_body(bodies, tipper_id_, checkpoint_.tipper);
        restore_body(bodies, valve_lever_id_, checkpoint_.valve_lever);
        restore_body(bodies, treadle_id_, checkpoint_.treadle);
        restore_body(bodies, lift_platform_id_, checkpoint_.lift_platform);
        restore_body(bodies, counterweight_id_, checkpoint_.counterweight);
        restore_body(bodies, jib_boom_id_, checkpoint_.jib_boom);
        restore_body(bodies, jib_hook_id_, checkpoint_.jib_hook);
        restore_body(bodies, crate_id_, checkpoint_.crate);
        if (jib_hoist_slider_ != nullptr) {
            jib_hoist_slider_->SetTargetPosition(checkpoint_.jib_hoist_target_position);
            jib_hoist_slider_->SetTargetVelocity(checkpoint_.jib_hoist_target_velocity);
            jib_hoist_slider_->SetMotorState(checkpoint_.jib_hoist_motor_state);
            jib_hoist_slider_->ResetWarmStart();
        }
        restore_body(bodies, needle_beam_id_, checkpoint_.needle_beam);
        restore_needle_topology(checkpoint_.needle_seated);
        restore_body(bodies, intake_swing_flight_id_, checkpoint_.intake_swing_flight);
        restore_body(bodies, intake_cw_cradle_id_, checkpoint_.intake_cw_cradle);
        restore_legal_forty_topology(checkpoint_.intake_pack_slung);
        restore_body(bodies, hook5_door_id_, checkpoint_.hook5_door);
        restore_body(bodies, hook5_bar_id_, checkpoint_.hook5_bar);
        restore_body(bodies, hook5_block_id_, checkpoint_.hook5_block);
    }

    // A kit body in the hands may be one the restore takes out of the
    // world (a shackle hooked at the commit): let go of it first.
    if (carry_constraint_ != nullptr &&
        scraperx::sim::kit::is_kit_entity(carried_entity_)) {
        release_carry();
    }
    kit_->restore(checkpoint_.kit);
    if (deformable_plank_) deformable_plank_->restore(checkpoint_.deformable_plank);
    // Restoring topology is not a new fracture. Preserve the event high
    // water immediately, including callers reading before the next tick.
    state_.reclaim_break_serial = kit_->weld_break_serial();
    state_.reclaim_break_valid = false;
    if (pipe_bridge_) pipe_bridge_->restore(checkpoint_.pipe_bridge);
    if (slingshot_) slingshot_->restore(checkpoint_.slingshot);
    if (swing_stair_) swing_stair_->restore(checkpoint_.swing_stair);
    if (upper_ascent_) upper_ascent_->restore(checkpoint_.upper_ascent);
    if (cargo_net_) cargo_net_->restore(checkpoint_.cargo_net);
    if (catch_net_) catch_net_->restore(checkpoint_.catch_net);
    clear_catch_history();
    if (service_lift_) service_lift_->restore(checkpoint_.service_lift);
    if (warehouse_forklift_) warehouse_forklift_->restore(checkpoint_.forklift);
    if (yard_crane_) yard_crane_->restore(checkpoint_.yard_crane);
    if (supplied_ascent_) supplied_ascent_->restore(checkpoint_.supplied_ascent);
    JPH::Vec3 restored_transport = JPH::Vec3::sZero();
    const auto saved_support = kit_->body_for_entity(checkpoint_.support_entity);
    if (saved_support.valid() && saved_support.value < checkpoint_.kit.bodies.size() &&
        checkpoint_.kit.bodies[saved_support.value].enabled &&
        kit_->body_dynamic(saved_support) && kit_->body_enabled(saved_support) &&
        kit_->body_id(saved_support) == checkpoint_.support_body) {
        const JPH::RVec3 point = bodies.GetCenterOfMassTransform(checkpoint_.support_body) *
                                checkpoint_.support_local_contact;
        restored_transport = bodies.GetPointVelocity(checkpoint_.support_body, point);
    }
    // Restart at rest relative to the restored support, rather than
    // replaying player walking or discarding the machine's transport.
    // Contacts remain cleared below and must be earned on the next tick.
    bodies.SetLinearAndAngularVelocity(player_id_, restored_transport, JPH::Vec3::sZero());
    restore_carry_topology(checkpoint_.carrying_entity);
    // The translation-only holder and its load share the transport. Do
    // not replay walking speed or independently corotate the held load.
    // Replaying committed walking speed previously swung the load out
    // of the still hands and pulled the player off MOD-HALL-DECK's edge.
    if (carry_constraint_ != nullptr) {
        bodies.SetLinearAndAngularVelocity(carried_id_, restored_transport,
                                           JPH::Vec3::sZero());
    }
    // No topology reconciliation call needed here, unlike the needle:
    // update_sump recomputes grate_safe_ and reasserts the grate's
    // sensor flag from sump_volume_kg_ unconditionally every tick, so
    // restoring the scalar is the whole restore.
    if (regression_fixtures_) {
        sump_volume_kg_ = checkpoint_.sump_volume_kg;
        sump_isolated_ = checkpoint_.sump_isolated;
        steam_plant_.restore_state(checkpoint_.vessel_mass_kg, checkpoint_.cylinder_mass_kg);
        water_screw_.restore_state(checkpoint_.water_screw);
        water_lift_bucket_water_m3_ = checkpoint_.water_lift_bucket_water_m3;
        water_lift_valve_open_ = checkpoint_.water_lift_valve_open;
        kit_->set_body_mass(
            ground_water_lift_.bucket,
            static_cast<float>(kWaterLiftBucketDryMassKg +
                               kWaterDensityKgM3 * water_lift_bucket_water_m3_));

    }

    trailing_foot_contact_tick_ = 0;
    grounded_ = false;
    jump_vault_ticks_left_ = ordinary_jump_ticks_left_ = 0;
    state_.checkpoint_footing_valid = false;
    support_entity_id_ = 0;
    support_sample_ = {};
    airborne_inherited_velocity_ = restored_transport;
    air_full_speed_ = kPlayerMaximumRelativeSpeed;
    sprinting_ = false;
    balancing_ = false;
    fall_peak_speed_mps_ = 0.0F;
    pre_contact_fall_speed_mps_ = 0.0F;
    clear_landing_recovery();
    parachute_deployed_ = false;
    air_control_force_ = JPH::Vec3::sZero();
    ++death_count_;
}

} // namespace scraperx::sim
