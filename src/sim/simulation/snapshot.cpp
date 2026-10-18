#include "sim/simulation/world.hpp"

namespace scraperx::sim {
using namespace detail;

[[nodiscard]] std::uint32_t Simulation::PhysicsWorld::entity_body_count(const std::uint64_t entity) const noexcept {
    JPH::BodyIDVector ids;
    physics_system_.GetBodies(ids);
    std::uint32_t count = 0;
    for (const auto id : ids) {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(), id);
        if (lock.Succeeded() && lock.GetBody().GetUserData() == entity) { ++count; }
    }
    return count;
}

[[nodiscard]] std::uint32_t Simulation::PhysicsWorld::moving_body_count() const noexcept {
    JPH::BodyIDVector ids;
    physics_system_.GetBodies(ids);
    std::uint32_t count = 0;
    for (const auto id : ids) {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(), id);
        if (lock.Succeeded() && !lock.GetBody().IsStatic()) { ++count; }
    }
    return count;
}

[[nodiscard]] const CargoNet *Simulation::PhysicsWorld::cargo_net() const noexcept { return cargo_net_.get(); }

[[nodiscard]] const CargoNet *Simulation::PhysicsWorld::catch_net() const noexcept { return catch_net_.get(); }

[[nodiscard]] CatchNetSnapshot Simulation::PhysicsWorld::catch_net_state() const noexcept { return catch_net_state_; }

[[nodiscard]] Simulation::RenderHandAnchor Simulation::PhysicsWorld::render_hand_anchor(const std::uint32_t hand) const noexcept {
    RenderHandAnchor result;
    const auto &bodies = physics_system_.GetBodyInterfaceNoLock();
    JPH::BodyID body;
    JPH::Vec3 point = JPH::Vec3::sZero();
    if ((traversal_state_ == TraversalState::Climbing ||
         traversal_state_ == TraversalState::Hanging) && hands_[hand].valid) {
        body = hands_[hand].body;
        point = hands_[hand].local;
        result.generation = hands_[hand].generation;
    } else if ((traversal_state_ == TraversalState::Hanging || traversal_state_ == TraversalState::Lowering) &&
               !traversal_body_.IsInvalid() && !traversal_normal_.IsNearZero()) {
        body = traversal_body_;
        point = traversal_local_ledge_;
        // Hanging hands use the existing world-space span around the lip.
        // Rotate that span into the body's frame before interpolating it.
        point += bodies.GetRotation(body).Conjugated() *
            (traversal_right() * (hand == 0 ? -kHangHandSpan : kHangHandSpan));
    } else if (physical_palms_[hand] && physical_palms_[hand]->active()) {
        body = physical_palms_[hand]->readback().support;
        point = physical_palms_[hand]->support_local_anchor();
        result.generation = physical_palms_[hand]->readback().generation;
    } else return result;
    if (body.IsInvalid()) return result;
    result.support = body.GetIndexAndSequenceNumber();
    result.valid = true;
    result.deforming = cargo_net_ && body == cargo_net_->body();
    if (!result.deforming) {
        const auto index = kit_->body_for_entity(bodies.GetUserData(body));
        if (index.valid() && kit_->body_id(index) == body) result.kit_body = index.value;
        // Physical support locals are COM-relative. Kit draws GetPosition
        // (the shape origin), so add the shape's actual COM offset once.
        point += bodies.GetShape(body)->GetCenterOfMass();
        result.origin = to_vector3(bodies.GetPosition(body));
        const auto q = bodies.GetRotation(body);
        result.rotation = {q.GetX(), q.GetY(), q.GetZ(), q.GetW()};
    }
    result.local = {point.GetX(), point.GetY(), point.GetZ()};
    return result;
}

[[nodiscard]] const Snapshot &Simulation::PhysicsWorld::state() const noexcept {
    return state_;
}

[[nodiscard]] bool Simulation::PhysicsWorld::traversal_committed() const noexcept {
    return traversal_state_ != TraversalState::None;
}

[[nodiscard]] const scraperx::sim::kit::Kit &Simulation::PhysicsWorld::kit() const noexcept {
    return *kit_;
}

[[nodiscard]] bool Simulation::PhysicsWorld::inspection_span_seat_contact() const noexcept {
    return contact_listener_.inspection_span_seat_contact();
}

[[nodiscard]] const PipeBridge *Simulation::PhysicsWorld::pipe_bridge() const noexcept { return pipe_bridge_.get(); }

[[nodiscard]] const Slingshot *Simulation::PhysicsWorld::slingshot() const noexcept { return slingshot_.get(); }

[[nodiscard]] Vector3 Simulation::PhysicsWorld::rider_specific_acceleration() const noexcept {
    return {rider_specific_acceleration_.GetX(), rider_specific_acceleration_.GetY(),
            rider_specific_acceleration_.GetZ()};
}

[[nodiscard]] std::vector<Vector3> Simulation::PhysicsWorld::slingshot_prediction() const {
    std::vector<Vector3> result;
    if (!slingshot_ || !slingshot_->state().seated) return result;
    JPH::IgnoreMultipleBodiesFilter filter;
    filter.IgnoreBody(player_id_);
    filter.IgnoreBody(kit_->body_id(slingshot_->pouch_body()));
    filter.IgnoreBody(kit_->body_id(kit_->body_for_entity(Slingshot::kRailEntity)));
    filter.IgnoreBody(kit_->body_id(kit_->body_for_entity(Slingshot::kLaunchRailEntity)));
    const auto points = slingshot_->prediction();
    if (points.empty()) return result;
    result.push_back(to_vector3(points.front()));
    for (std::size_t n = 1; n < points.size(); ++n) {
        const auto from = points[n - 1];
        const JPH::Vec3 displacement(points[n] - from);
        JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
        const JPH::RShapeCast sweep(player_shape_.GetPtr(), JPH::Vec3::sReplicate(1.0F),
            JPH::RMat44::sTranslation(from), displacement);
        physics_system_.GetNarrowPhaseQuery().CastShape(sweep, JPH::ShapeCastSettings(),
            from, collector, {}, {}, filter);
        if (collector.HadHit()) {
            result.push_back(to_vector3(from + displacement * collector.mHit.mFraction));
            break;
        }
        result.push_back(to_vector3(points[n]));
    }
    return result;
}

void Simulation::PhysicsWorld::set_feed_enabled(const bool enabled) noexcept {
    steam_plant_.set_feed_enabled(enabled);
}

void Simulation::PhysicsWorld::set_water_screw_motor_torque_limit_nm(const double torque_nm) noexcept {
    water_screw_.set_motor_torque_limit_nm(torque_nm);
}

void Simulation::PhysicsWorld::set_water_screw_outlet_blocked(const bool blocked) noexcept {
    water_screw_.set_outlet_blocked(blocked);
}

void Simulation::PhysicsWorld::set_water_screw_drive_direction(const int direction) noexcept {
    water_screw_.set_drive_direction(direction);
}

void Simulation::PhysicsWorld::set_water_screw_basin_volume_m3(const double volume_m3) noexcept {
    water_screw_.set_basin_volume_m3(volume_m3);
}

void Simulation::PhysicsWorld::set_water_screw_tank_volume_m3_for_proof(const double volume_m3) noexcept {
    water_screw_.set_tank_volume_m3_preserving_total(volume_m3);
}

void Simulation::PhysicsWorld::set_water_lift_rope_connected(const bool connected) noexcept {
    kit_->set_rope_connected(ground_water_lift_.rope, connected);
}

void Simulation::PhysicsWorld::set_water_lift_cage_mass_kg(const double mass_kg) noexcept {
    if (std::isfinite(mass_kg) && mass_kg > 0.0) {
        kit_->set_body_mass(ground_water_lift_.cage, static_cast<float>(mass_kg));
    }
}

[[nodiscard]] SuppliedAscent::Selection Simulation::PhysicsWorld::supplied_machine_station(const JPH::BodyInterface &bodies,const bool require_footing)const noexcept {
    using Station=SuppliedAscent::Station;SuppliedAscent::Selection nearest;
    if(!supplied_ascent_||(require_footing&&!grounded_)||traversal_state_!=TraversalState::None||carried_entity_!=0)return nearest;
    const auto player=bodies.GetPosition(player_id_);const auto eye=player+JPH::Vec3(0,.55F,0);double limit=1.65*1.65;
    for(unsigned i=0;i<supplied_ascent_->count();++i)for(auto station:{Station::Deck,Station::Lower,Station::Upper,Station::Intermediate})for(unsigned panel=0;panel<supplied_ascent_->station_count(station,i);++panel){
        const auto entity=supplied_ascent_->station_entity(station,i,panel);
        const bool supported=station==Station::Deck||station==Station::Intermediate?
            support_entity_id_==entity:(support_entity_id_==11||support_entity_id_==entity);
        if(require_footing&&!supported)continue;
        const auto target=supplied_ascent_->station_position(station,i,panel);const double distance=(target-player).LengthSq();
        if(distance>limit)continue;
        const auto ray=JPH::Vec3(target-eye);JPH::RayCastResult hit;
        if(!cast_ray(eye,ray,hit))continue;
        if(bodies.GetUserData(hit.mBodyID)!=entity||(1.F-hit.mFraction)*ray.Length()>.28F)continue;
        nearest={static_cast<int>(i),station};limit=distance;
    }
    return nearest;
}

[[nodiscard]] ServiceLift::Station Simulation::PhysicsWorld::service_lift_station(const JPH::BodyInterface &bodies, const bool require_footing) const noexcept {
    using Station = ServiceLift::Station;
    if (!service_lift_ || (require_footing && !grounded_) || traversal_state_ != TraversalState::None || carried_entity_ != 0)
        return Station::None;
    const auto player = bodies.GetPosition(player_id_);
    const auto eye = player + JPH::Vec3(0, .55F, 0);
    Station nearest = Station::None;
    double nearest_distance = 1.65 * 1.65;
    for (const auto station : {Station::Deck, Station::Lower, Station::Upper}) {
        const bool supported = station == Station::Deck ? support_entity_id_ == ServiceLift::kDeckEntity :
            (support_entity_id_ == 11 || support_entity_id_ == ServiceLift::kLandingEntity);
        if (require_footing && !supported) continue;
        const auto target = service_lift_->station_position(station);
        const double distance = (target - player).LengthSq();
        if (distance > nearest_distance) continue;
        const auto ray = JPH::Vec3(target - eye);
        JPH::RayCastResult hit;
        if (!cast_ray(eye, ray, hit)) continue;
        const auto expected = station == Station::Deck ? ServiceLift::kDeckEntity : ServiceLift::kLandingEntity;
        // Reject intervening structure, including another part of the same compound.
        if (bodies.GetUserData(hit.mBodyID) != expected || (1.0F - hit.mFraction) * ray.Length() > .28F) continue;
        nearest = station;
        nearest_distance = distance;
    }
    return nearest;
}

void Simulation::PhysicsWorld::read_machine_state(const JPH::BodyInterface &bodies) noexcept {
    if(supplied_ascent_){
        const auto station=supplied_machine_station(bodies),reachable=supplied_machine_station(bodies,false);
        const unsigned index=station.machine>=0?static_cast<unsigned>(station.machine):reachable.machine>=0?static_cast<unsigned>(reachable.machine):supplied_ascent_->active();
        const auto value=supplied_ascent_->state(index);state_.supplied_machine_index=static_cast<std::uint8_t>(index);
        state_.supplied_machine_station=static_cast<std::uint8_t>(station.station);state_.supplied_machine_reachable_station=static_cast<std::uint8_t>(reachable.station);
        state_.supplied_machine_surface_y=supplied_ascent_->walking_surface_y(index);
        state_.supplied_machine_energy_j=value.energy_j;state_.supplied_machine_capacity_j=supplied_ascent_->capacity_j(index);
        state_.supplied_machine_positive_work_j=value.positive_work_j;state_.supplied_machine_heat_j=value.heat_j;
        state_.supplied_machine_force_n=value.force_n;state_.supplied_machine_energy_overdraft_j=value.energy_overdraft_j;
        state_.supplied_machine_cart_brake_heat_j=value.cart_passive_brake_heat_j;
        state_.supplied_machine_cart_brake_residual_j=value.cart_passive_brake_positive_residual_j;
        state_.supplied_machine_cart_actuator_rounding_residual_j=value.cart_actuator_rounding_residual_j;
        state_.supplied_machine_hopper_mass_kg=value.hopper_mass_kg;state_.supplied_machine_wheel_angle_rad=value.wheel_angle_rad;state_.supplied_machine_bearing_heat_j=value.bearing_heat_j;
        state_.supplied_machine_power_w=value.power_w;state_.supplied_machine_braking=value.braking;state_.supplied_machine_energy_cutoff=value.energy_cutoff;
        state_.supplied_machine_wheel_motor_enabled=value.wheel_motor_enabled;
    }
    if (service_lift_) {
        const auto lift = service_lift_->state();
        state_.service_lift_station = static_cast<std::uint8_t>(service_lift_station(bodies));
        state_.service_lift_reachable_station = static_cast<std::uint8_t>(service_lift_station(bodies, false));
        state_.service_lift_retry_available = service_lift_retry_available_;
        state_.service_lift_surface_y = service_lift_->walking_surface_y();
        state_.service_lift_energy_j = lift.energy_j;
        state_.service_lift_capacity_j = service_lift_->design().capacity_j;
        state_.service_lift_force_n = lift.actuator_force_n;
        state_.service_lift_power_w = lift.electrical_power_w;
        state_.service_lift_braking = lift.braking;
        state_.service_lift_energy_cutoff = lift.energy_cutoff;
    }
    state_.carrying_entity_id = carried_entity_;
    state_.carry_target_entity_id = carry_target_entity_;
    state_.rig_action = rig_action_;
    state_.rig_target_entity_id = rig_target_entity_;
    switch (kit_->carry_kind(carry_target_entity_)) {
    case scraperx::sim::kit::CarryKind::Shackle:
        state_.carry_target_kind = 1;
        break;
    case scraperx::sim::kit::CarryKind::Handle:
        state_.carry_target_kind = 2;
        break;
    default:
        state_.carry_target_kind = 0;
        break;
    }
    if (!regression_fixtures_) { return; }
    state_.hoist_scoop_position = {kScoopX, scoop_height_, kScoopZ};
    state_.hoist_scoop_tilt_radians = scoop_tilt_;

    const JPH::RVec3 ballast_position = bodies.GetPosition(ballast_id_);
    const JPH::Vec3 ballast_velocity = bodies.GetLinearVelocity(ballast_id_);
    state_.ballast_position = to_vector3(ballast_position);
    state_.ballast_linear_velocity =
        {ballast_velocity.GetX(), ballast_velocity.GetY(), ballast_velocity.GetZ()};

    state_.tipper_position = to_vector3(bodies.GetPosition(tipper_id_));
    state_.tipper_angle_radians =
        tipper_hinge_ != nullptr ? tipper_hinge_->GetCurrentAngle() : 0.0;
    state_.valve_lever_angle_radians = valve_lever_angle_;
    state_.treadle_angle_radians = treadle_angle_;

    const JPH::RVec3 platform_position = bodies.GetPosition(lift_platform_id_);
    const JPH::Vec3 platform_velocity = bodies.GetLinearVelocity(lift_platform_id_);
    state_.lift_platform_position = to_vector3(platform_position);
    state_.lift_platform_linear_velocity =
        {platform_velocity.GetX(), platform_velocity.GetY(), platform_velocity.GetZ()};
    state_.counterweight_position = to_vector3(bodies.GetPosition(counterweight_id_));

    const auto &plant = steam_plant_.state();
    state_.valve_open_fraction = plant.valve_open_fraction;
    state_.vessel_pressure_pa = plant.vessel_pressure_pa;
    state_.cylinder_pressure_pa = plant.cylinder_pressure_pa;
    state_.orifice_mass_flow_kg_per_s = plant.orifice_mass_flow_kg_per_s;
    state_.vented_mass_kg = plant.vented_mass_kg;
    state_.piston_force_n = plant.piston_force_n;
    state_.vessel_available_energy_j = steam_plant_.vessel_available_energy_j();
    state_.machine_cycle_phase_seconds = machine_cycle_phase_seconds_;

    state_.jib_station_active = jib_station_active_;
    state_.jib_boom_angle_radians = jib_boom_angle_;
    const JPH::Vec3 hook_velocity = bodies.GetLinearVelocity(jib_hook_id_);
    state_.jib_hook_position = to_vector3(bodies.GetPosition(jib_hook_id_));
    state_.jib_hook_linear_velocity =
        {hook_velocity.GetX(), hook_velocity.GetY(), hook_velocity.GetZ()};
    state_.jib_hoist_position_meters = 0.0;
    state_.jib_hoist_velocity_mps = 0.0;
    state_.jib_hoist_motor_force_n = 0.0;
    if (jib_hoist_slider_ != nullptr) {
        state_.jib_hoist_position_meters = jib_hoist_slider_->GetCurrentPosition();
        state_.jib_hoist_motor_force_n = jib_hoist_slider_->GetTotalLambdaMotor() /
            Simulation::kFixedStepSeconds;
        const auto frame1 = jib_hoist_slider_->GetConstraintToBody1Matrix();
        const auto frame2 = jib_hoist_slider_->GetConstraintToBody2Matrix();
        const auto anchor1 = bodies.GetCenterOfMassTransform(jib_boom_id_) *
            JPH::RVec3(frame1.GetTranslation());
        const auto anchor2 = bodies.GetCenterOfMassTransform(jib_hook_id_) *
            JPH::RVec3(frame2.GetTranslation());
        const auto axis = bodies.GetRotation(jib_boom_id_) * frame1.GetAxisX();
        // Differentiate u.dot(axis), including the axis's angular motion.
        state_.jib_hoist_velocity_mps =
            (bodies.GetPointVelocity(jib_hook_id_, anchor2) -
             bodies.GetPointVelocity(jib_boom_id_, anchor1)).Dot(axis) +
            JPH::Vec3(anchor2 - anchor1).Dot(
                bodies.GetAngularVelocity(jib_boom_id_).Cross(axis));
    }
    const JPH::Vec3 crate_velocity = bodies.GetLinearVelocity(crate_id_);
    state_.jib_crate_position = to_vector3(bodies.GetPosition(crate_id_));
    state_.jib_crate_linear_velocity =
        {crate_velocity.GetX(), crate_velocity.GetY(), crate_velocity.GetZ()};
    state_.jib_capacity_stand_load_position =
        to_vector3(bodies.GetPosition(capacity_stand_load_id_));

    state_.needle_station_active = needle_station_active_;
    state_.needle_seated = needle_seated_;
    const JPH::Vec3 needle_velocity = bodies.GetLinearVelocity(needle_beam_id_);
    state_.needle_position = to_vector3(bodies.GetPosition(needle_beam_id_));
    state_.needle_linear_velocity =
        {needle_velocity.GetX(), needle_velocity.GetY(), needle_velocity.GetZ()};

    state_.sump_station_active = sump_station_active_;
    state_.sump_isolated = sump_isolated_;
    state_.sump_volume_kg = sump_volume_kg_;
    state_.grate_safe = grate_safe_;

    const auto &screw = water_screw_.state();
    state_.water_screw_station_active = water_screw_station_active_;
    state_.water_screw_motor_enabled = screw.motor_enabled;
    state_.water_screw_shaft_angle_radians = screw.shaft_angle_radians;
    state_.water_screw_rpm = screw.shaft_rpm;
    state_.water_screw_motor_torque_nm = screw.motor_torque_nm;
    state_.water_screw_flow_m3_s = screw.delivered_flow_m3_s;
    state_.water_screw_basin_volume_m3 = screw.basin_volume_m3;
    state_.water_screw_tank_volume_m3 = screw.tank_volume_m3;
    state_.water_screw_leakage_m3 = screw.cumulative_leakage_or_recycle_m3;
    state_.water_screw_shaft_work_j = screw.shaft_work_j;

    state_.water_lift_valve_station_active = water_lift_valve_station_active_;
    state_.water_lift_release_station_active = water_lift_release_station_active_;
    state_.water_lift_reset_station_active = water_lift_reset_station_active_;
    state_.water_lift_valve_open = water_lift_valve_open_;
    state_.water_lift_valve_flow_m3_s = water_lift_valve_flow_m3_s_;
    state_.water_lift_bucket_water_m3 = water_lift_bucket_water_m3_;
    state_.water_lift_bucket_mass_kg =
        kit_->body_mass(ground_water_lift_.bucket);
    state_.water_lift_bucket_travel_m =
        kit_->guide_travel(ground_water_lift_.bucket_guide);
    state_.water_lift_cage_travel_m =
        kit_->guide_travel(ground_water_lift_.cage_guide);
    state_.water_lift_cage_peak_speed_mps =
        kit_->guide_peak_speed(ground_water_lift_.cage_guide);
    state_.water_lift_rope_tension_n =
        kit_->rope_tension(ground_water_lift_.rope);
    state_.water_lift_bucket_catch_latched =
        kit_->catch_latched(ground_water_lift_.bucket_top_catch);
    state_.water_lift_upper_catch_latched =
        kit_->catch_latched(ground_water_lift_.cage_upper_catch);

    state_.intake_station_active = intake_station_active_;
    state_.intake_boom_angle_radians = intake_boom_angle_;
    state_.intake_hook_position = to_vector3(bodies.GetPosition(intake_hook_id_));
    state_.intake_pack_position = to_vector3(bodies.GetPosition(intake_pack_id_));
    state_.intake_overweight_pack_position =
        to_vector3(bodies.GetPosition(intake_overweight_pack_id_));
    state_.intake_dog_angle_radians = intake_dog_angle_;
    state_.intake_pack_pins_dog = intake_pack_pins_dog_;
    state_.intake_throat_clear = intake_throat_clear_;

    state_.legal_forty_pack_slung = intake_pack_slung_;
    state_.legal_forty_swing_travel_radians = intake_swing_travel_;
    state_.legal_forty_swing_flight_position =
        to_vector3(bodies.GetPosition(intake_swing_flight_id_));
    state_.legal_forty_cradle_position = to_vector3(bodies.GetPosition(intake_cw_cradle_id_));

    state_.carrying_entity_id = carried_entity_;
    state_.carry_target_entity_id = carry_target_entity_;
    state_.hook5_door_angle_radians =
        hook5_door_hinge_ != nullptr ? hook5_door_hinge_->GetCurrentAngle() : 0.0;
    const JPH::RVec3 block_position = bodies.GetPosition(hook5_block_id_);
    state_.hook5_block_position = to_vector3(block_position);
    state_.hook5_block_rotation = to_quaternion(bodies.GetRotation(hook5_block_id_));
    state_.hook5_bar_position = to_vector3(bodies.GetPosition(hook5_bar_id_));
    state_.hook5_bar_rotation = to_quaternion(bodies.GetRotation(hook5_bar_id_));
    state_.hook_in_rack =
        JPH::Vec3(block_position - JPH::RVec3(kHook5BlockSeatX, kHook5BlockSeatY,
                                              kHook5BlockSeatZ))
            .Length() <= kHook5InRackTolerance;

    state_.rig_action = rig_action_;
    state_.rig_target_entity_id = rig_target_entity_;
    const auto &kit = *kit_;
    state_.well_a_cage_travel = kit.guide_travel(well_.a_cage_guide);
    state_.well_a_skip_travel = kit.guide_travel(well_.a_skip_guide);
    state_.well_a_cage_peak_speed = kit.guide_peak_speed(well_.a_cage_guide);
    state_.well_a_catch_latched = kit.catch_latched(well_.a_catch);
    state_.well_a_rope_end_entity_id = kit.rope_end_entity(well_.a_rope);
    state_.well_a_rope_tension_n = kit.rope_tension(well_.a_rope);
    state_.well_a_lever_angle = kit.lever_angle(well_.a_lever);
    state_.well_b_cage_travel = kit.guide_travel(well_.b_cage_guide);
    state_.well_b_counterweight_travel = kit.guide_travel(well_.b_counterweight_guide);
    state_.well_b_cage_peak_speed = kit.guide_peak_speed(well_.b_cage_guide);
    state_.well_b_catch_latched = kit.catch_latched(well_.b_catch);
    state_.well_b_rope_end_entity_id = kit.rope_end_entity(well_.b_rope);
    state_.well_b_rope_tension_n = kit.rope_tension(well_.b_rope);
    state_.well_b_lever_angle = kit.lever_angle(well_.b_lever);

    const JPH::RVec3 rope_tipper =
        bodies.GetCenterOfMassTransform(tipper_id_) * JPH::RVec3(3.0, -0.2, 0.0);
    const JPH::RVec3 rope_lever =
        bodies.GetCenterOfMassTransform(valve_lever_id_) * JPH::RVec3(-1.60, 0.0, 0.0);
    state_.rope_extension_meters =
        JPH::Vec3(rope_lever - rope_tipper).Length() - rope_rest_length_;
}

void Simulation::PhysicsWorld::read_state() noexcept {
    refresh_forklift_snapshot(physics_system_.GetBodyInterfaceNoLock());
    refresh_yard_crane_snapshot(physics_system_.GetBodyInterfaceNoLock());
    const auto &bodies = physics_system_.GetBodyInterface();

    const JPH::RVec3 player_position = bodies.GetPosition(player_id_);
    const JPH::Vec3 player_velocity = bodies.GetLinearVelocity(player_id_);
    state_.player_position = to_vector3(player_position);
    state_.player_linear_velocity =
        {player_velocity.GetX(), player_velocity.GetY(), player_velocity.GetZ()};
    state_.player_grounded = grounded_;
    state_.player_crouched = crouched_;
    state_.support_entity_id = support_entity_id_;
    state_.support_contact_point = support_sample_.contact_point;
    state_.support_point_linear_velocity = support_sample_.point_velocity;

    if (regression_fixtures_) {
        const JPH::RVec3 translating_position = bodies.GetPosition(translating_support_id_);
        const JPH::Vec3 translating_velocity = bodies.GetLinearVelocity(translating_support_id_);
        state_.translating_support_position = to_vector3(translating_position);
        state_.translating_support_linear_velocity =
            {translating_velocity.GetX(), translating_velocity.GetY(), translating_velocity.GetZ()};

        const JPH::RVec3 rotating_position = bodies.GetPosition(rotating_support_id_);
        const JPH::Vec3 rotating_angular_velocity = bodies.GetAngularVelocity(rotating_support_id_);
        state_.rotating_support_position = to_vector3(rotating_position);
        state_.rotating_support_yaw_radians = rotating_support_yaw_radians_;
        state_.rotating_support_angular_velocity =
            {rotating_angular_velocity.GetX(),
             rotating_angular_velocity.GetY(),
             rotating_angular_velocity.GetZ()};

        const JPH::RVec3 moving_ledge_position = bodies.GetPosition(moving_ledge_id_);
        const JPH::Vec3 moving_ledge_velocity = bodies.GetLinearVelocity(moving_ledge_id_);
        state_.moving_ledge_position = to_vector3(moving_ledge_position);
        state_.moving_ledge_linear_velocity =
            {moving_ledge_velocity.GetX(), moving_ledge_velocity.GetY(), moving_ledge_velocity.GetZ()};

    }

    state_.traversal_state = traversal_state_;
    state_.traversal_support_entity_id = traversal_entity_id_;
    state_.traversal_progress = traversal_progress_;
    if (traversal_state_ == TraversalState::None) {
        state_.traversal_ledge_point = {};
        state_.traversal_target_point = {};
    } else {
        state_.traversal_ledge_point =
            to_vector3(from_support_local(bodies, traversal_body_, traversal_local_ledge_));
        state_.traversal_target_point = to_vector3(
            from_support_local(bodies, traversal_target_body_, traversal_local_target_));
    }

    // Step 2: the hands' points, the structure's direction, the legs,
    // and what a grounded body is offered.
    state_.traversal_left_hand = {};
    state_.traversal_right_hand = {};
    state_.traversal_left_hand_generation = 0;
    state_.traversal_right_hand_generation = 0;
    state_.traversal_normal = {};
    if (traversal_state_ != TraversalState::None) {
        state_.traversal_normal = {traversal_normal_.GetX(), 0.0, traversal_normal_.GetZ()};
    }
    if (traversal_state_ == TraversalState::Climbing ||
        traversal_state_ == TraversalState::Hanging) {
        const auto left = physical_hands_->grip_readback(0);
        const auto right = physical_hands_->grip_readback(1);
        if (left.attached) {
            state_.traversal_left_hand = to_vector3(left.world_anchor);
            state_.traversal_left_hand_generation = left.generation;
        }
        if (right.attached) {
            state_.traversal_right_hand = to_vector3(right.world_anchor);
            state_.traversal_right_hand_generation = right.generation;
        }
    } else if ((traversal_state_ == TraversalState::Hanging ||
                traversal_state_ == TraversalState::Lowering) &&
               !traversal_normal_.IsNearZero()) {
        const JPH::RVec3 lip = from_support_local(bodies, traversal_body_, traversal_local_ledge_);
        const JPH::Vec3 span = traversal_right() * kHangHandSpan;
        state_.traversal_left_hand = to_vector3(lip - span);
        state_.traversal_right_hand = to_vector3(lip + span);
    }
    {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterface(), player_id_);
        state_.player_gravity_factor = lock.Succeeded() ?
            lock.GetBody().GetMotionProperties()->GetGravityFactor() : 1.0;
    }
    state_.traversal_hand_constraint_count = physical_hands_ ?
        unsigned(physical_hands_->attached(0)) + unsigned(physical_hands_->attached(1)) : 0;
    for (unsigned hand = 0; hand < 2; ++hand) {
        auto &published = state_.hand_props[hand];
        published = {};
        if (!physical_palms_[hand]) continue;
        const auto &palm = physical_palms_[hand]->readback();
        published.active = palm.active;
        published.stop_reason = static_cast<std::uint8_t>(palm.stop_reason);
        published.generation = palm.generation;
        if (palm.active) published.support_entity_id = bodies.GetUserData(palm.support);
        published.anchor = to_vector3(palm.world_anchor);
        published.normal = to_vector3(palm.surface_normal);
        published.point_velocity = to_vector3(palm.point_velocity);
        published.player_force = to_vector3(palm.player_force);
        published.elapsed_seconds = palm.elapsed_seconds;
        published.yielded_normal_distance_m = palm.yielded_normal_distance_m;
        published.last_impulse_ns = palm.last_impulse_ns;
        published.last_event_work_j = palm.last_event_work_j;
        published.absorbed_event_work_j = palm.absorbed_event_work_j;
        published.command_work_j = palm.command_work_j;
    }
    state_.palm_vault_count = palm_vault_count_;
    state_.traversal_plan_kind = static_cast<std::uint8_t>(traversal_plan_.kind);
    state_.traversal_phase = static_cast<std::uint8_t>(traversal_plan_.phase);
    state_.traversal_rejection_reason = static_cast<std::uint8_t>(traversal_plan_.rejection);
    state_.traversal_plan_generation = traversal_plan_.generation;
    state_.traversal_receiver_entity_id = traversal_plan_.receiver_entity;
    state_.foot_contact_tick = foot_contact_.valid ? foot_contact_.solved_tick : 0;
    const bool rigid_foot_source = foot_push_purpose_ == FootPushPurpose::RigidTransfer ||
        foot_push_purpose_ == FootPushPurpose::TrailingPlant || foot_push_purpose_ == FootPushPurpose::CoyotePush ||
        foot_push_purpose_ == FootPushPurpose::WalkingStep || foot_push_purpose_ == FootPushPurpose::WallKick ||
        foot_push_purpose_ == FootPushPurpose::WallStride;
    state_.foot_push_source_contact_tick = rigid_foot_source && foot_push_contact_.valid
        ? foot_push_contact_.solved_tick : 0;
    state_.hand_foot_overlap_active = physical_hands_->active() && physical_foot_push_->active();
    state_.air_catch_intent_pending = air_catch_action_ticks_left_ > 0;
    state_.actual_hand_load_n = physical_hands_->hand_force(0).Length() + physical_hands_->hand_force(1).Length();
    state_.actual_foot_load_n = physical_foot_push_->readback().player_force.Length();
    const auto actuator_load = state_.actual_hand_load_n + state_.actual_foot_load_n;
    state_.actual_hand_load_fraction = actuator_load > 0 ? state_.actual_hand_load_n/actuator_load : 0;
    state_.actual_foot_load_fraction = actuator_load > 0 ? state_.actual_foot_load_n/actuator_load : 0;
    state_.traversal_left_hand_force = physical_hands_ ?
        to_vector3(physical_hands_->hand_force(0)) : Vector3{};
    state_.traversal_right_hand_force = physical_hands_ ?
        to_vector3(physical_hands_->hand_force(1)) : Vector3{};
    state_.traversal_command_work_bound_j = physical_hands_ ?
        physical_hands_->command_work_bound_j() : 0;
    state_.traversal_actuator_positive_work_j = physical_hands_ ? physical_hands_->actuator_positive_work_j() : 0;
    state_.traversal_actuator_absorbed_work_j = physical_hands_ ? physical_hands_->actuator_absorbed_work_j() : 0;
    state_.hanging_free_arc_active = physical_hands_ && physical_hands_->radial_active();
    state_.hanging_commanded_span_m = state_.hanging_free_arc_active ? physical_hands_->radial_commanded_span_m() : 0.0;
    state_.hanging_actual_span_m = state_.hanging_free_arc_active ? physical_hands_->radial_actual_span_m() : 0.0;
    state_.missing_hand_grip_available = missing_radial_grip(bodies).valid;
    state_.swing_tuck_available = hanging_posture_eligible(bodies);
    state_.swing_tuck_requested = state_.swing_tuck_available && hanging_posture_input_;
    state_.swing_body_below_grip_m = 0.0;
    state_.swing_commanded_body_below_grip_m = 0.0;
    if (state_.swing_tuck_available) {
        const auto grip = physical_hands_->radial_grip_centroid();
        state_.swing_body_below_grip_m = grip.GetY() - bodies.GetPosition(player_id_).GetY();
        state_.swing_commanded_body_below_grip_m = grip.GetY() - physical_hands_->commanded_position().GetY();
    }
    if (deformable_plank_) {
        const auto plank = deformable_plank_->capture();
        state_.plank_broken_joint_mask = plank.broken_mask;
        state_.plank_peak_strength_ratio = plank.peak_strength_ratio;
        state_.plank_fracture_count = plank.fracture_serial;
        state_.plank_discarded_strain_energy_j = plank.discarded_strain_energy_j;
    }
    if (physical_foot_push_) {
        const auto &foot = physical_foot_push_->readback();
        state_.foot_push_active = foot.active;
        state_.foot_push_start_count = foot_push_start_count_;
        state_.foot_push_support_entity_id = foot.support.IsInvalid() ? 0 : bodies.GetUserData(foot.support);
        state_.foot_push_stop_reason = static_cast<std::uint8_t>(foot.stop_reason);
        state_.foot_push_command_work_bound_j = foot.command_work_bound_j;
        state_.foot_push_stroke_m = foot.commanded_stroke_m;
        state_.foot_push_stroke_limit_m = foot.stroke_limit_m;
        state_.foot_push_peak_load_n = foot.peak_load_n;
        state_.foot_push_last_load_n = foot.last_load_n;
        state_.foot_push_last_impulse_ns = foot.last_impulse_ns;
        state_.foot_push_elapsed_seconds = foot.elapsed_seconds;
        state_.foot_push_actual_distance_m = foot.actual_distance_m;
        state_.foot_push_initial_distance_m = foot.initial_distance_m;
        state_.foot_push_player_force = to_vector3(foot.player_force);
        state_.foot_push_world_anchor = foot.active ? to_vector3(foot.world_anchor) : Vector3{};
        state_.foot_push_world_normal = foot.active ? to_vector3(foot.world_surface_normal) : Vector3{};
    }
    if (cargo_net_ && traversal_body_ == cargo_net_->body() && physical_hands_->active())
        state_.traversal_target_point = to_vector3(physical_hands_->commanded_position());
    state_.player_swinging = physical_hands_ && physical_hands_->swinging();
    state_.player_sprinting = sprinting_;
    state_.player_sliding = ground_sliding_ && grounded_ && crouched_ &&
        !support_sample_.body_id.IsInvalid();
    state_.slide_count = ground_slide_count_;
    state_.wall_stride_count = wall_stride_count_;
    state_.wall_stride_active = !grounded_ && physical_foot_push_->active() &&
        foot_push_purpose_ == FootPushPurpose::WallStride && rigid_transfer_foot_valid_;
    state_.wall_stride_pushing_off = state_.wall_stride_active &&
        !physical_foot_push_->readback().walking_step;
    state_.wall_stride_normal = {};
    if (state_.wall_stride_active) {
        const auto &bodies = physics_system_.GetBodyInterface();
        state_.wall_stride_normal = to_vector3(bodies.GetRotation(foot_push_contact_.body) *
                                              foot_push_contact_.local_normal);
    }
    state_.slide_speed_mps = 0.0;
    if (state_.player_sliding) {
        const auto &bodies = physics_system_.GetBodyInterface();
        const auto relative = bodies.GetLinearVelocity(player_id_) -
            current_support_point_velocity(bodies);
        const auto normal = support_traction_normal();
        state_.slide_speed_mps = (relative - normal * relative.Dot(normal)).Length();
    }
    state_.player_balancing = balancing_;
    state_.grip_available = grip_affordance_.valid;
    state_.grip_entity_id = grip_affordance_.valid ? grip_affordance_.entity_id : 0;
    state_.grip_point = grip_affordance_.valid ? to_vector3(grip_affordance_.point) : Vector3{};
    state_.edge_drop_available = edge_affordance_.valid;
    state_.edge_drop_point =
        edge_affordance_.valid ? to_vector3(edge_affordance_.ledge) : Vector3{};
    state_.climb_count = climb_count_;

    state_.ledge_available = affordance_.valid;
    state_.ledge_entity_id = affordance_.valid ? affordance_.ledge_entity_id : 0;
    state_.ledge_point = affordance_.valid ? to_vector3(affordance_.ledge_point) : Vector3{};
    state_.ledge_rise_meters = affordance_.valid ? affordance_.rise : 0.0;

    read_machine_state(bodies);

    state_.accepted_traversal_count = accepted_traversal_count_;
    state_.world_solid_bodies = world_solid_bodies_;
    state_.step_up_count = step_up_count_;
    state_.step_up_pose_count = step_up_pose_count_;
    state_.jump_vault_count = jump_vault_count_;
    state_.world_solid_mirrors = world_solid_mirrors_;
    state_.world_solid_rejected = world_solid_rejected_;
    state_.rejected_traversal_count = rejected_traversal_count_;
    state_.aborted_traversal_count = aborted_traversal_count_;

    state_.fall_state = !grounded_
        ? (parachute_deployed_ ? FallState::Parachuting : FallState::Airborne)
        : FallState::Grounded;
    state_.fall_peak_speed_mps = fall_peak_speed_mps_;
    state_.last_impact_speed_mps = last_impact_speed_mps_;
    state_.landing_response = landing_response_;
    state_.landing_recovery_progress = landing_recovery_duration_ > 0 ?
        1.0-std::clamp(landing_recovery_seconds_/landing_recovery_duration_,0.0,1.0) : 1.0;
    state_.landing_count = landing_count_;
    state_.landing_support_entity_id = landing_support_entity_id_;
    state_.landing_approach_energy_j = landing_approach_energy_j_;
    state_.landing_tangent_energy_j = landing_tangent_energy_j_;
    state_.landing_normal_speed_mps = landing_normal_speed_mps_;
    state_.landing_tangent_speed_mps = landing_tangent_speed_mps_;
    state_.landing_observed_normal_impulse_ns = landing_observed_normal_impulse_ns_;
    state_.landing_recovery_seconds = landing_recovery_seconds_;
    state_.landing_recovery_work_j = landing_recovery_work_j_;
    state_.landing_jump_work_j = landing_jump_work_j_;
    state_.air_control_command_positive_work_j = air_control_command_positive_work_j_;
    state_.air_control_command_absorbed_work_j = air_control_command_absorbed_work_j_;
    state_.air_control_force_n = to_vector3(air_control_force_);
    state_.landing_recovering = slingshot_ && grounded_ && landing_recovery_seconds_ > 0;
    const double recovery_ratio = landing_recovery_duration_ > 0 ?
        std::clamp(landing_recovery_seconds_ / landing_recovery_duration_, 0.0, 1.0) : 0;
    state_.landing_balance = 1.0 - landing_initial_balance_loss_ * recovery_ratio;
    if (slingshot_ && grounded_ && !support_sample_.body_id.IsInvalid()) {
        const auto normal = support_traction_normal();
        const auto relative = bodies.GetLinearVelocity(player_id_) - current_support_point_velocity(bodies);
        landing_slip_velocity_ = relative - normal * relative.Dot(normal);
    }
    state_.landing_slip_velocity = grounded_ ? Vector3{landing_slip_velocity_.GetX(), landing_slip_velocity_.GetY(),
                                                      landing_slip_velocity_.GetZ()} : Vector3{};
    state_.parachute_deployed = parachute_deployed_;
    state_.checkpoint_position = to_vector3(checkpoint_position_);
    state_.checkpoint_commit_count = checkpoint_commit_count_;
    state_.death_count = death_count_;
}

} // namespace scraperx::sim
