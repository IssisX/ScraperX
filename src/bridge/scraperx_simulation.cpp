#include "bridge/scraperx_simulation.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace scraperx::bridge {
namespace {

[[nodiscard]] godot::Vector3 to_godot(const sim::Vector3 &value) {
    return {static_cast<godot::real_t>(value.x),
            static_cast<godot::real_t>(value.y),
            static_cast<godot::real_t>(value.z)};
}

[[nodiscard]] godot::Quaternion to_godot(const sim::Quaternion &value) {
    return {static_cast<godot::real_t>(value.x), static_cast<godot::real_t>(value.y),
            static_cast<godot::real_t>(value.z), static_cast<godot::real_t>(value.w)};
}

// One past the last spawn, derived from the enum so a new spawn is never
// silently refused (the literal 21 had fallen behind IntakeHandoffDeck).
constexpr std::int64_t kInitialSpawnCount =
    static_cast<std::int64_t>(sim::InitialSpawn::StairTop) + 1;

// A kit index from script: negative or past the end reads as no body.
[[nodiscard]] std::uint32_t kit_index(const std::int64_t index) {
    return index >= 0 && index < static_cast<std::int64_t>(sim::Simulation::kKitNone)
               ? static_cast<std::uint32_t>(index)
               : sim::Simulation::kKitNone;
}

} // namespace

ScraperXSimulation::ScraperXSimulation()
    : simulation_(std::make_unique<sim::Simulation>()) {}

void ScraperXSimulation::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("configure_initial_spawn", "initial_spawn"),
                                &ScraperXSimulation::configure_initial_spawn);
    godot::ClassDB::bind_method(godot::D_METHOD("set_move_input", "world_x", "world_z"),
                                &ScraperXSimulation::set_move_input);
    godot::ClassDB::bind_method(godot::D_METHOD("set_facing", "world_x", "world_z"),
                                &ScraperXSimulation::set_facing);
    godot::ClassDB::bind_method(godot::D_METHOD("request_jump"),
                                &ScraperXSimulation::request_jump);
    godot::ClassDB::bind_method(godot::D_METHOD("request_traversal"),
                                &ScraperXSimulation::request_traversal);
    godot::ClassDB::bind_method(godot::D_METHOD("request_release"),
                                &ScraperXSimulation::request_release);
    godot::ClassDB::bind_method(godot::D_METHOD("request_parachute"),
                                &ScraperXSimulation::request_parachute);
    godot::ClassDB::bind_method(godot::D_METHOD("set_crouch_input", "held"),
                                &ScraperXSimulation::set_crouch_input);
    godot::ClassDB::bind_method(godot::D_METHOD("advance_frame", "frame_delta_seconds"),
                                &ScraperXSimulation::advance_frame);
    godot::ClassDB::bind_method(godot::D_METHOD("get_tick_index"),
                                &ScraperXSimulation::get_tick_index);
    godot::ClassDB::bind_method(godot::D_METHOD("get_simulation_time_seconds"),
                                &ScraperXSimulation::get_simulation_time_seconds);
    godot::ClassDB::bind_method(godot::D_METHOD("get_fixed_step_seconds"),
                                &ScraperXSimulation::get_fixed_step_seconds);
    godot::ClassDB::bind_method(godot::D_METHOD("get_interpolation_alpha"),
                                &ScraperXSimulation::get_interpolation_alpha);
    godot::ClassDB::bind_method(godot::D_METHOD("get_player_position"),
                                &ScraperXSimulation::get_player_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_player_linear_velocity"),
                                &ScraperXSimulation::get_player_linear_velocity);
    godot::ClassDB::bind_method(godot::D_METHOD("is_player_grounded"),
                                &ScraperXSimulation::is_player_grounded);
    godot::ClassDB::bind_method(godot::D_METHOD("is_player_crouched"),
                                &ScraperXSimulation::is_player_crouched);
    godot::ClassDB::bind_method(godot::D_METHOD("get_support_entity_id"),
                                &ScraperXSimulation::get_support_entity_id);
    godot::ClassDB::bind_method(godot::D_METHOD("get_support_contact_point"),
                                &ScraperXSimulation::get_support_contact_point);
    godot::ClassDB::bind_method(godot::D_METHOD("get_support_point_linear_velocity"),
                                &ScraperXSimulation::get_support_point_linear_velocity);
    godot::ClassDB::bind_method(godot::D_METHOD("get_translating_support_position"),
                                &ScraperXSimulation::get_translating_support_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_translating_support_linear_velocity"),
                                &ScraperXSimulation::get_translating_support_linear_velocity);
    godot::ClassDB::bind_method(godot::D_METHOD("get_rotating_support_position"),
                                &ScraperXSimulation::get_rotating_support_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_rotating_support_yaw_radians"),
                                &ScraperXSimulation::get_rotating_support_yaw_radians);
    godot::ClassDB::bind_method(godot::D_METHOD("get_rotating_support_angular_velocity"),
                                &ScraperXSimulation::get_rotating_support_angular_velocity);
    godot::ClassDB::bind_method(godot::D_METHOD("get_moving_ledge_position"),
                                &ScraperXSimulation::get_moving_ledge_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_moving_ledge_linear_velocity"),
                                &ScraperXSimulation::get_moving_ledge_linear_velocity);
    godot::ClassDB::bind_method(godot::D_METHOD("get_traversal_state"),
                                &ScraperXSimulation::get_traversal_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_traversal_support_entity_id"),
                                &ScraperXSimulation::get_traversal_support_entity_id);
    godot::ClassDB::bind_method(godot::D_METHOD("get_traversal_progress"),
                                &ScraperXSimulation::get_traversal_progress);
    godot::ClassDB::bind_method(godot::D_METHOD("get_traversal_ledge_point"),
                                &ScraperXSimulation::get_traversal_ledge_point);
    godot::ClassDB::bind_method(godot::D_METHOD("get_traversal_target_point"),
                                &ScraperXSimulation::get_traversal_target_point);
    godot::ClassDB::bind_method(godot::D_METHOD("is_ledge_available"),
                                &ScraperXSimulation::is_ledge_available);
    godot::ClassDB::bind_method(godot::D_METHOD("get_ledge_entity_id"),
                                &ScraperXSimulation::get_ledge_entity_id);
    godot::ClassDB::bind_method(godot::D_METHOD("get_ledge_point"),
                                &ScraperXSimulation::get_ledge_point);
    godot::ClassDB::bind_method(godot::D_METHOD("get_ledge_rise_meters"),
                                &ScraperXSimulation::get_ledge_rise_meters);
    godot::ClassDB::bind_method(godot::D_METHOD("get_accepted_traversal_count"),
                                &ScraperXSimulation::get_accepted_traversal_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_rejected_traversal_count"),
                                &ScraperXSimulation::get_rejected_traversal_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_aborted_traversal_count"),
                                &ScraperXSimulation::get_aborted_traversal_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_tower_height_meters"),
                                &ScraperXSimulation::get_tower_height_meters);
    godot::ClassDB::bind_method(godot::D_METHOD("get_hoist_scoop_position"),
                                &ScraperXSimulation::get_hoist_scoop_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_hoist_scoop_tilt_radians"),
                                &ScraperXSimulation::get_hoist_scoop_tilt_radians);
    godot::ClassDB::bind_method(godot::D_METHOD("get_ballast_position"),
                                &ScraperXSimulation::get_ballast_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_ballast_linear_velocity"),
                                &ScraperXSimulation::get_ballast_linear_velocity);
    godot::ClassDB::bind_method(godot::D_METHOD("get_tipper_position"),
                                &ScraperXSimulation::get_tipper_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_tipper_angle_radians"),
                                &ScraperXSimulation::get_tipper_angle_radians);
    godot::ClassDB::bind_method(godot::D_METHOD("get_valve_lever_angle_radians"),
                                &ScraperXSimulation::get_valve_lever_angle_radians);
    godot::ClassDB::bind_method(godot::D_METHOD("get_treadle_angle_radians"),
                                &ScraperXSimulation::get_treadle_angle_radians);
    godot::ClassDB::bind_method(godot::D_METHOD("get_valve_open_fraction"),
                                &ScraperXSimulation::get_valve_open_fraction);
    godot::ClassDB::bind_method(godot::D_METHOD("get_rope_extension_meters"),
                                &ScraperXSimulation::get_rope_extension_meters);
    godot::ClassDB::bind_method(godot::D_METHOD("get_lift_platform_position"),
                                &ScraperXSimulation::get_lift_platform_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_lift_platform_linear_velocity"),
                                &ScraperXSimulation::get_lift_platform_linear_velocity);
    godot::ClassDB::bind_method(godot::D_METHOD("get_counterweight_position"),
                                &ScraperXSimulation::get_counterweight_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_vessel_pressure_pa"),
                                &ScraperXSimulation::get_vessel_pressure_pa);
    godot::ClassDB::bind_method(godot::D_METHOD("get_cylinder_pressure_pa"),
                                &ScraperXSimulation::get_cylinder_pressure_pa);
    godot::ClassDB::bind_method(godot::D_METHOD("get_orifice_mass_flow_kg_per_s"),
                                &ScraperXSimulation::get_orifice_mass_flow_kg_per_s);
    godot::ClassDB::bind_method(godot::D_METHOD("get_vented_mass_kg"),
                                &ScraperXSimulation::get_vented_mass_kg);
    godot::ClassDB::bind_method(godot::D_METHOD("get_piston_force_n"),
                                &ScraperXSimulation::get_piston_force_n);
    godot::ClassDB::bind_method(godot::D_METHOD("get_vessel_available_energy_j"),
                                &ScraperXSimulation::get_vessel_available_energy_j);
    godot::ClassDB::bind_method(godot::D_METHOD("get_machine_cycle_phase_seconds"),
                                &ScraperXSimulation::get_machine_cycle_phase_seconds);
    godot::ClassDB::bind_method(godot::D_METHOD("get_fall_state"),
                                &ScraperXSimulation::get_fall_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_fall_peak_speed_mps"),
                                &ScraperXSimulation::get_fall_peak_speed_mps);
    godot::ClassDB::bind_method(godot::D_METHOD("get_last_impact_speed_mps"),
                                &ScraperXSimulation::get_last_impact_speed_mps);
    godot::ClassDB::bind_method(godot::D_METHOD("get_lethal_impact_speed_mps"),
                                &ScraperXSimulation::get_lethal_impact_speed_mps);
    godot::ClassDB::bind_method(godot::D_METHOD("is_parachute_deployed"),
                                &ScraperXSimulation::is_parachute_deployed);
    godot::ClassDB::bind_method(godot::D_METHOD("get_checkpoint_position"),
                                &ScraperXSimulation::get_checkpoint_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_checkpoint_commit_count"),
                                &ScraperXSimulation::get_checkpoint_commit_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_death_count"),
                                &ScraperXSimulation::get_death_count);

    godot::ClassDB::bind_method(godot::D_METHOD("set_jib_slew_input", "value"),
                                &ScraperXSimulation::set_jib_slew_input);
    godot::ClassDB::bind_method(godot::D_METHOD("set_jib_hoist_input", "value"),
                                &ScraperXSimulation::set_jib_hoist_input);
    godot::ClassDB::bind_method(godot::D_METHOD("is_jib_station_active"),
                                &ScraperXSimulation::is_jib_station_active);
    godot::ClassDB::bind_method(godot::D_METHOD("get_jib_boom_angle_radians"),
                                &ScraperXSimulation::get_jib_boom_angle_radians);
    godot::ClassDB::bind_method(godot::D_METHOD("get_jib_hook_position"),
                                &ScraperXSimulation::get_jib_hook_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_jib_hook_linear_velocity"),
                                &ScraperXSimulation::get_jib_hook_linear_velocity);
    godot::ClassDB::bind_method(godot::D_METHOD("get_jib_crate_position"),
                                &ScraperXSimulation::get_jib_crate_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_jib_crate_linear_velocity"),
                                &ScraperXSimulation::get_jib_crate_linear_velocity);
    godot::ClassDB::bind_method(godot::D_METHOD("get_jib_capacity_stand_load_position"),
                                &ScraperXSimulation::get_jib_capacity_stand_load_position);

    godot::ClassDB::bind_method(godot::D_METHOD("set_needle_hoist_input", "value"),
                                &ScraperXSimulation::set_needle_hoist_input);
    godot::ClassDB::bind_method(godot::D_METHOD("is_needle_station_active"),
                                &ScraperXSimulation::is_needle_station_active);
    godot::ClassDB::bind_method(godot::D_METHOD("is_needle_seated"),
                                &ScraperXSimulation::is_needle_seated);
    godot::ClassDB::bind_method(godot::D_METHOD("get_needle_position"),
                                &ScraperXSimulation::get_needle_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_needle_linear_velocity"),
                                &ScraperXSimulation::get_needle_linear_velocity);

    godot::ClassDB::bind_method(godot::D_METHOD("request_valve_toggle"),
                                &ScraperXSimulation::request_valve_toggle);
    godot::ClassDB::bind_method(godot::D_METHOD("is_sump_station_active"),
                                &ScraperXSimulation::is_sump_station_active);
    godot::ClassDB::bind_method(godot::D_METHOD("is_sump_isolated"),
                                &ScraperXSimulation::is_sump_isolated);
    godot::ClassDB::bind_method(godot::D_METHOD("get_sump_volume_kg"),
                                &ScraperXSimulation::get_sump_volume_kg);
    godot::ClassDB::bind_method(godot::D_METHOD("is_grate_safe"),
                                &ScraperXSimulation::is_grate_safe);

    godot::ClassDB::bind_method(godot::D_METHOD("set_intake_slew_input", "value"),
                                &ScraperXSimulation::set_intake_slew_input);
    godot::ClassDB::bind_method(godot::D_METHOD("set_intake_hoist_input", "value"),
                                &ScraperXSimulation::set_intake_hoist_input);
    godot::ClassDB::bind_method(godot::D_METHOD("is_intake_station_active"),
                                &ScraperXSimulation::is_intake_station_active);
    godot::ClassDB::bind_method(godot::D_METHOD("get_intake_boom_angle_radians"),
                                &ScraperXSimulation::get_intake_boom_angle_radians);
    godot::ClassDB::bind_method(godot::D_METHOD("get_intake_hook_position"),
                                &ScraperXSimulation::get_intake_hook_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_intake_pack_position"),
                                &ScraperXSimulation::get_intake_pack_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_intake_overweight_pack_position"),
                                &ScraperXSimulation::get_intake_overweight_pack_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_intake_dog_angle_radians"),
                                &ScraperXSimulation::get_intake_dog_angle_radians);
    godot::ClassDB::bind_method(godot::D_METHOD("does_intake_pack_pin_dog"),
                                &ScraperXSimulation::does_intake_pack_pin_dog);
    godot::ClassDB::bind_method(godot::D_METHOD("is_intake_throat_clear"),
                                &ScraperXSimulation::is_intake_throat_clear);

    godot::ClassDB::bind_method(godot::D_METHOD("request_intake_sling_release"),
                                &ScraperXSimulation::request_intake_sling_release);
    godot::ClassDB::bind_method(godot::D_METHOD("request_intake_sling_attach"),
                                &ScraperXSimulation::request_intake_sling_attach);
    godot::ClassDB::bind_method(godot::D_METHOD("is_legal_forty_pack_slung"),
                                &ScraperXSimulation::is_legal_forty_pack_slung);
    godot::ClassDB::bind_method(godot::D_METHOD("get_legal_forty_swing_travel_radians"),
                                &ScraperXSimulation::get_legal_forty_swing_travel_radians);
    godot::ClassDB::bind_method(godot::D_METHOD("get_legal_forty_swing_flight_position"),
                                &ScraperXSimulation::get_legal_forty_swing_flight_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_legal_forty_cradle_position"),
                                &ScraperXSimulation::get_legal_forty_cradle_position);

    godot::ClassDB::bind_method(godot::D_METHOD("request_pick_up"),
                                &ScraperXSimulation::request_pick_up);
    godot::ClassDB::bind_method(godot::D_METHOD("request_set_down"),
                                &ScraperXSimulation::request_set_down);
    godot::ClassDB::bind_method(godot::D_METHOD("get_carrying_entity_id"),
                                &ScraperXSimulation::get_carrying_entity_id);
    godot::ClassDB::bind_method(godot::D_METHOD("get_carry_target_entity_id"),
                                &ScraperXSimulation::get_carry_target_entity_id);
    godot::ClassDB::bind_method(godot::D_METHOD("is_hook_in_rack"),
                                &ScraperXSimulation::is_hook_in_rack);
    godot::ClassDB::bind_method(godot::D_METHOD("get_hook5_door_angle_radians"),
                                &ScraperXSimulation::get_hook5_door_angle_radians);
    godot::ClassDB::bind_method(godot::D_METHOD("get_hook5_bar_position"),
                                &ScraperXSimulation::get_hook5_bar_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_hook5_bar_rotation"),
                                &ScraperXSimulation::get_hook5_bar_rotation);
    godot::ClassDB::bind_method(godot::D_METHOD("get_hook5_block_position"),
                                &ScraperXSimulation::get_hook5_block_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_hook5_block_rotation"),
                                &ScraperXSimulation::get_hook5_block_rotation);

    godot::ClassDB::bind_method(godot::D_METHOD("request_rig"), &ScraperXSimulation::request_rig);
    godot::ClassDB::bind_method(godot::D_METHOD("get_rig_action"),
                                &ScraperXSimulation::get_rig_action);
    godot::ClassDB::bind_method(godot::D_METHOD("get_rig_target_entity_id"),
                                &ScraperXSimulation::get_rig_target_entity_id);
    godot::ClassDB::bind_method(godot::D_METHOD("get_carry_target_kind"),
                                &ScraperXSimulation::get_carry_target_kind);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_body_count"),
                                &ScraperXSimulation::get_kit_body_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_body_entity_id", "body"),
                                &ScraperXSimulation::get_kit_body_entity_id);
    godot::ClassDB::bind_method(godot::D_METHOD("is_kit_body_dynamic", "body"),
                                &ScraperXSimulation::is_kit_body_dynamic);
    godot::ClassDB::bind_method(godot::D_METHOD("is_kit_body_enabled", "body"),
                                &ScraperXSimulation::is_kit_body_enabled);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_body_parts", "body"),
                                &ScraperXSimulation::get_kit_body_parts);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_body_transform", "body"),
                                &ScraperXSimulation::get_kit_body_transform);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_body_index", "entity_id"),
                                &ScraperXSimulation::get_kit_body_index);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_cable_count"),
                                &ScraperXSimulation::get_kit_cable_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_cable_points", "cable"),
                                &ScraperXSimulation::get_kit_cable_points);
    godot::ClassDB::bind_method(godot::D_METHOD("get_well_a_cage_travel"),
                                &ScraperXSimulation::get_well_a_cage_travel);
    godot::ClassDB::bind_method(godot::D_METHOD("is_well_a_catch_latched"),
                                &ScraperXSimulation::is_well_a_catch_latched);
    godot::ClassDB::bind_method(godot::D_METHOD("get_well_a_rope_end_entity_id"),
                                &ScraperXSimulation::get_well_a_rope_end_entity_id);
}

bool ScraperXSimulation::configure_initial_spawn(const std::int64_t initial_spawn) {
    if (initial_spawn < 0 || initial_spawn >= kInitialSpawnCount) {
        godot::UtilityFunctions::push_error(
            "ScraperX native authority rejected an unknown initial spawn; state was not mutated.");
        return false;
    }
    if (simulation_->snapshot().tick_index != 0) {
        godot::UtilityFunctions::push_error(
            "ScraperX native authority rejected a spawn change after the authoritative clock "
            "advanced; state was not mutated.");
        return false;
    }
    simulation_ = std::make_unique<sim::Simulation>(
        static_cast<sim::InitialSpawn>(static_cast<std::uint8_t>(initial_spawn)));
    return true;
}

bool ScraperXSimulation::set_move_input(const double world_x, const double world_z) {
    const bool accepted = simulation_->set_move_input(world_x, world_z);
    if (!accepted) {
        godot::UtilityFunctions::push_error(
            "ScraperX native authority rejected non-finite movement input; state was not mutated.");
    }
    return accepted;
}

bool ScraperXSimulation::set_facing(const double world_x, const double world_z) {
    return simulation_->set_facing(world_x, world_z);
}

bool ScraperXSimulation::request_jump() {
    return simulation_->request_jump();
}

bool ScraperXSimulation::request_traversal() {
    return simulation_->request_traversal();
}

bool ScraperXSimulation::request_release() {
    return simulation_->request_release();
}

bool ScraperXSimulation::request_parachute() {
    return simulation_->request_parachute();
}

bool ScraperXSimulation::set_crouch_input(const bool held) {
    return simulation_->set_crouch_input(held);
}

std::int64_t ScraperXSimulation::advance_frame(const double frame_delta_seconds) {
    const auto result = simulation_->advance_frame(frame_delta_seconds);
    if (!result.accepted) {
        godot::UtilityFunctions::push_error(
            "ScraperX native authority rejected an invalid frame delta; state was not mutated.");
        return -1;
    }
    return static_cast<std::int64_t>(result.steps_advanced);
}

std::int64_t ScraperXSimulation::get_tick_index() const {
    return static_cast<std::int64_t>(simulation_->snapshot().tick_index);
}

double ScraperXSimulation::get_simulation_time_seconds() const {
    return simulation_->snapshot().simulation_time_seconds;
}

double ScraperXSimulation::get_fixed_step_seconds() const {
    return simulation_->snapshot().fixed_step_seconds;
}

double ScraperXSimulation::get_interpolation_alpha() const {
    return simulation_->snapshot().interpolation_alpha;
}

godot::Vector3 ScraperXSimulation::get_player_position() const {
    return to_godot(simulation_->snapshot().player_position);
}

godot::Vector3 ScraperXSimulation::get_player_linear_velocity() const {
    return to_godot(simulation_->snapshot().player_linear_velocity);
}

bool ScraperXSimulation::is_player_grounded() const {
    return simulation_->snapshot().player_grounded;
}

bool ScraperXSimulation::is_player_crouched() const {
    return simulation_->snapshot().player_crouched;
}

std::int64_t ScraperXSimulation::get_support_entity_id() const {
    return static_cast<std::int64_t>(simulation_->snapshot().support_entity_id);
}

godot::Vector3 ScraperXSimulation::get_support_contact_point() const {
    return to_godot(simulation_->snapshot().support_contact_point);
}

godot::Vector3 ScraperXSimulation::get_support_point_linear_velocity() const {
    return to_godot(simulation_->snapshot().support_point_linear_velocity);
}

godot::Vector3 ScraperXSimulation::get_translating_support_position() const {
    return to_godot(simulation_->snapshot().translating_support_position);
}

godot::Vector3 ScraperXSimulation::get_translating_support_linear_velocity() const {
    return to_godot(simulation_->snapshot().translating_support_linear_velocity);
}

godot::Vector3 ScraperXSimulation::get_rotating_support_position() const {
    return to_godot(simulation_->snapshot().rotating_support_position);
}

double ScraperXSimulation::get_rotating_support_yaw_radians() const {
    return simulation_->snapshot().rotating_support_yaw_radians;
}

godot::Vector3 ScraperXSimulation::get_rotating_support_angular_velocity() const {
    return to_godot(simulation_->snapshot().rotating_support_angular_velocity);
}

godot::Vector3 ScraperXSimulation::get_moving_ledge_position() const {
    return to_godot(simulation_->snapshot().moving_ledge_position);
}

godot::Vector3 ScraperXSimulation::get_moving_ledge_linear_velocity() const {
    return to_godot(simulation_->snapshot().moving_ledge_linear_velocity);
}

std::int64_t ScraperXSimulation::get_traversal_state() const {
    return static_cast<std::int64_t>(
        static_cast<std::uint8_t>(simulation_->snapshot().traversal_state));
}

std::int64_t ScraperXSimulation::get_traversal_support_entity_id() const {
    return static_cast<std::int64_t>(simulation_->snapshot().traversal_support_entity_id);
}

double ScraperXSimulation::get_traversal_progress() const {
    return simulation_->snapshot().traversal_progress;
}

godot::Vector3 ScraperXSimulation::get_traversal_ledge_point() const {
    return to_godot(simulation_->snapshot().traversal_ledge_point);
}

godot::Vector3 ScraperXSimulation::get_traversal_target_point() const {
    return to_godot(simulation_->snapshot().traversal_target_point);
}

bool ScraperXSimulation::is_ledge_available() const {
    return simulation_->snapshot().ledge_available;
}

std::int64_t ScraperXSimulation::get_ledge_entity_id() const {
    return static_cast<std::int64_t>(simulation_->snapshot().ledge_entity_id);
}

godot::Vector3 ScraperXSimulation::get_ledge_point() const {
    return to_godot(simulation_->snapshot().ledge_point);
}

double ScraperXSimulation::get_ledge_rise_meters() const {
    return simulation_->snapshot().ledge_rise_meters;
}

std::int64_t ScraperXSimulation::get_accepted_traversal_count() const {
    return static_cast<std::int64_t>(simulation_->snapshot().accepted_traversal_count);
}

std::int64_t ScraperXSimulation::get_rejected_traversal_count() const {
    return static_cast<std::int64_t>(simulation_->snapshot().rejected_traversal_count);
}

std::int64_t ScraperXSimulation::get_aborted_traversal_count() const {
    return static_cast<std::int64_t>(simulation_->snapshot().aborted_traversal_count);
}

godot::Vector3 ScraperXSimulation::get_hoist_scoop_position() const {
    return to_godot(simulation_->snapshot().hoist_scoop_position);
}

double ScraperXSimulation::get_hoist_scoop_tilt_radians() const {
    return simulation_->snapshot().hoist_scoop_tilt_radians;
}

godot::Vector3 ScraperXSimulation::get_ballast_position() const {
    return to_godot(simulation_->snapshot().ballast_position);
}

godot::Vector3 ScraperXSimulation::get_ballast_linear_velocity() const {
    return to_godot(simulation_->snapshot().ballast_linear_velocity);
}

godot::Vector3 ScraperXSimulation::get_tipper_position() const {
    return to_godot(simulation_->snapshot().tipper_position);
}

double ScraperXSimulation::get_tipper_angle_radians() const {
    return simulation_->snapshot().tipper_angle_radians;
}

double ScraperXSimulation::get_valve_lever_angle_radians() const {
    return simulation_->snapshot().valve_lever_angle_radians;
}

double ScraperXSimulation::get_treadle_angle_radians() const {
    return simulation_->snapshot().treadle_angle_radians;
}

double ScraperXSimulation::get_valve_open_fraction() const {
    return simulation_->snapshot().valve_open_fraction;
}

double ScraperXSimulation::get_rope_extension_meters() const {
    return simulation_->snapshot().rope_extension_meters;
}

godot::Vector3 ScraperXSimulation::get_lift_platform_position() const {
    return to_godot(simulation_->snapshot().lift_platform_position);
}

godot::Vector3 ScraperXSimulation::get_lift_platform_linear_velocity() const {
    return to_godot(simulation_->snapshot().lift_platform_linear_velocity);
}

godot::Vector3 ScraperXSimulation::get_counterweight_position() const {
    return to_godot(simulation_->snapshot().counterweight_position);
}

double ScraperXSimulation::get_vessel_pressure_pa() const {
    return simulation_->snapshot().vessel_pressure_pa;
}

double ScraperXSimulation::get_cylinder_pressure_pa() const {
    return simulation_->snapshot().cylinder_pressure_pa;
}

double ScraperXSimulation::get_orifice_mass_flow_kg_per_s() const {
    return simulation_->snapshot().orifice_mass_flow_kg_per_s;
}

double ScraperXSimulation::get_vented_mass_kg() const {
    return simulation_->snapshot().vented_mass_kg;
}

double ScraperXSimulation::get_piston_force_n() const {
    return simulation_->snapshot().piston_force_n;
}

double ScraperXSimulation::get_vessel_available_energy_j() const {
    return simulation_->snapshot().vessel_available_energy_j;
}

double ScraperXSimulation::get_machine_cycle_phase_seconds() const {
    return simulation_->snapshot().machine_cycle_phase_seconds;
}

std::int64_t ScraperXSimulation::get_fall_state() const {
    return static_cast<std::int64_t>(static_cast<std::uint8_t>(simulation_->snapshot().fall_state));
}

double ScraperXSimulation::get_fall_peak_speed_mps() const {
    return simulation_->snapshot().fall_peak_speed_mps;
}

double ScraperXSimulation::get_last_impact_speed_mps() const {
    return simulation_->snapshot().last_impact_speed_mps;
}

double ScraperXSimulation::get_lethal_impact_speed_mps() const {
    return sim::Simulation::kLethalImpactSpeedMps;
}

bool ScraperXSimulation::is_parachute_deployed() const {
    return simulation_->snapshot().parachute_deployed;
}

godot::Vector3 ScraperXSimulation::get_checkpoint_position() const {
    return to_godot(simulation_->snapshot().checkpoint_position);
}

std::int64_t ScraperXSimulation::get_checkpoint_commit_count() const {
    return static_cast<std::int64_t>(simulation_->snapshot().checkpoint_commit_count);
}

std::int64_t ScraperXSimulation::get_death_count() const {
    return static_cast<std::int64_t>(simulation_->snapshot().death_count);
}

double ScraperXSimulation::get_tower_height_meters() const {
    return sim::Simulation::kTowerHeightMeters;
}

bool ScraperXSimulation::set_jib_slew_input(const double value) {
    return simulation_->set_jib_slew_input(value);
}

bool ScraperXSimulation::set_jib_hoist_input(const double value) {
    return simulation_->set_jib_hoist_input(value);
}

bool ScraperXSimulation::is_jib_station_active() const {
    return simulation_->snapshot().jib_station_active;
}

double ScraperXSimulation::get_jib_boom_angle_radians() const {
    return simulation_->snapshot().jib_boom_angle_radians;
}

godot::Vector3 ScraperXSimulation::get_jib_hook_position() const {
    return to_godot(simulation_->snapshot().jib_hook_position);
}

godot::Vector3 ScraperXSimulation::get_jib_hook_linear_velocity() const {
    return to_godot(simulation_->snapshot().jib_hook_linear_velocity);
}

godot::Vector3 ScraperXSimulation::get_jib_crate_position() const {
    return to_godot(simulation_->snapshot().jib_crate_position);
}

godot::Vector3 ScraperXSimulation::get_jib_crate_linear_velocity() const {
    return to_godot(simulation_->snapshot().jib_crate_linear_velocity);
}

godot::Vector3 ScraperXSimulation::get_jib_capacity_stand_load_position() const {
    return to_godot(simulation_->snapshot().jib_capacity_stand_load_position);
}

bool ScraperXSimulation::set_needle_hoist_input(const double value) {
    return simulation_->set_needle_hoist_input(value);
}

bool ScraperXSimulation::is_needle_station_active() const {
    return simulation_->snapshot().needle_station_active;
}

bool ScraperXSimulation::is_needle_seated() const {
    return simulation_->snapshot().needle_seated;
}

godot::Vector3 ScraperXSimulation::get_needle_position() const {
    return to_godot(simulation_->snapshot().needle_position);
}

godot::Vector3 ScraperXSimulation::get_needle_linear_velocity() const {
    return to_godot(simulation_->snapshot().needle_linear_velocity);
}

bool ScraperXSimulation::request_valve_toggle() {
    return simulation_->request_valve_toggle();
}

bool ScraperXSimulation::is_sump_station_active() const {
    return simulation_->snapshot().sump_station_active;
}

bool ScraperXSimulation::is_sump_isolated() const {
    return simulation_->snapshot().sump_isolated;
}

double ScraperXSimulation::get_sump_volume_kg() const {
    return simulation_->snapshot().sump_volume_kg;
}

bool ScraperXSimulation::is_grate_safe() const {
    return simulation_->snapshot().grate_safe;
}

bool ScraperXSimulation::set_intake_slew_input(const double value) {
    return simulation_->set_intake_slew_input(value);
}

bool ScraperXSimulation::set_intake_hoist_input(const double value) {
    return simulation_->set_intake_hoist_input(value);
}

bool ScraperXSimulation::is_intake_station_active() const {
    return simulation_->snapshot().intake_station_active;
}

double ScraperXSimulation::get_intake_boom_angle_radians() const {
    return simulation_->snapshot().intake_boom_angle_radians;
}

godot::Vector3 ScraperXSimulation::get_intake_hook_position() const {
    return to_godot(simulation_->snapshot().intake_hook_position);
}

godot::Vector3 ScraperXSimulation::get_intake_pack_position() const {
    return to_godot(simulation_->snapshot().intake_pack_position);
}

godot::Vector3 ScraperXSimulation::get_intake_overweight_pack_position() const {
    return to_godot(simulation_->snapshot().intake_overweight_pack_position);
}

double ScraperXSimulation::get_intake_dog_angle_radians() const {
    return simulation_->snapshot().intake_dog_angle_radians;
}

bool ScraperXSimulation::does_intake_pack_pin_dog() const {
    return simulation_->snapshot().intake_pack_pins_dog;
}

bool ScraperXSimulation::is_intake_throat_clear() const {
    return simulation_->snapshot().intake_throat_clear;
}

bool ScraperXSimulation::request_intake_sling_release() {
    return simulation_->request_intake_sling_release();
}

bool ScraperXSimulation::request_intake_sling_attach() {
    return simulation_->request_intake_sling_attach();
}

bool ScraperXSimulation::is_legal_forty_pack_slung() const {
    return simulation_->snapshot().legal_forty_pack_slung;
}

double ScraperXSimulation::get_legal_forty_swing_travel_radians() const {
    return simulation_->snapshot().legal_forty_swing_travel_radians;
}

godot::Vector3 ScraperXSimulation::get_legal_forty_swing_flight_position() const {
    return to_godot(simulation_->snapshot().legal_forty_swing_flight_position);
}

godot::Vector3 ScraperXSimulation::get_legal_forty_cradle_position() const {
    return to_godot(simulation_->snapshot().legal_forty_cradle_position);
}

bool ScraperXSimulation::request_pick_up() {
    return simulation_->request_pick_up();
}

bool ScraperXSimulation::request_set_down() {
    return simulation_->request_set_down();
}

std::int64_t ScraperXSimulation::get_carrying_entity_id() const {
    return static_cast<std::int64_t>(simulation_->snapshot().carrying_entity_id);
}

std::int64_t ScraperXSimulation::get_carry_target_entity_id() const {
    return static_cast<std::int64_t>(simulation_->snapshot().carry_target_entity_id);
}

bool ScraperXSimulation::is_hook_in_rack() const {
    return simulation_->snapshot().hook_in_rack;
}

double ScraperXSimulation::get_hook5_door_angle_radians() const {
    return simulation_->snapshot().hook5_door_angle_radians;
}

godot::Vector3 ScraperXSimulation::get_hook5_bar_position() const {
    return to_godot(simulation_->snapshot().hook5_bar_position);
}

godot::Quaternion ScraperXSimulation::get_hook5_bar_rotation() const {
    return to_godot(simulation_->snapshot().hook5_bar_rotation);
}

godot::Vector3 ScraperXSimulation::get_hook5_block_position() const {
    return to_godot(simulation_->snapshot().hook5_block_position);
}

godot::Quaternion ScraperXSimulation::get_hook5_block_rotation() const {
    return to_godot(simulation_->snapshot().hook5_block_rotation);
}

bool ScraperXSimulation::request_rig() {
    return simulation_->request_rig();
}

std::int64_t ScraperXSimulation::get_rig_action() const {
    return static_cast<std::int64_t>(simulation_->snapshot().rig_action);
}

std::int64_t ScraperXSimulation::get_rig_target_entity_id() const {
    return static_cast<std::int64_t>(simulation_->snapshot().rig_target_entity_id);
}

std::int64_t ScraperXSimulation::get_carry_target_kind() const {
    return static_cast<std::int64_t>(simulation_->snapshot().carry_target_kind);
}

std::int64_t ScraperXSimulation::get_kit_body_count() const {
    return static_cast<std::int64_t>(simulation_->kit_body_count());
}

std::int64_t ScraperXSimulation::get_kit_body_entity_id(const std::int64_t body) const {
    return static_cast<std::int64_t>(simulation_->kit_body_entity(kit_index(body)));
}

bool ScraperXSimulation::is_kit_body_dynamic(const std::int64_t body) const {
    return simulation_->kit_body_dynamic(kit_index(body));
}

bool ScraperXSimulation::is_kit_body_enabled(const std::int64_t body) const {
    return simulation_->kit_body_enabled(kit_index(body));
}

godot::PackedFloat32Array ScraperXSimulation::get_kit_body_parts(const std::int64_t body) const {
    godot::PackedFloat32Array out;
    const std::uint32_t index = kit_index(body);
    const std::uint32_t count = simulation_->kit_body_part_count(index);
    for (std::uint32_t part = 0; part < count; ++part) {
        const sim::KitPart source = simulation_->kit_body_part(index, part);
        for (const double value :
             {source.half.x, source.half.y, source.half.z, source.offset.x, source.offset.y,
              source.offset.z, source.rotation.x, source.rotation.y, source.rotation.z,
              source.rotation.w, static_cast<double>(source.material)}) {
            out.push_back(static_cast<float>(value));
        }
    }
    return out;
}

godot::Transform3D ScraperXSimulation::get_kit_body_transform(const std::int64_t body) const {
    const std::uint32_t index = kit_index(body);
    return {godot::Basis(to_godot(simulation_->kit_body_rotation(index))),
            to_godot(simulation_->kit_body_position(index))};
}

std::int64_t ScraperXSimulation::get_kit_body_index(const std::int64_t entity_id) const {
    if (entity_id < 0) {
        return -1;
    }
    const std::uint32_t index = simulation_->kit_body_index(static_cast<std::uint64_t>(entity_id));
    return index == sim::Simulation::kKitNone ? -1 : static_cast<std::int64_t>(index);
}

std::int64_t ScraperXSimulation::get_kit_cable_count() const {
    return static_cast<std::int64_t>(simulation_->kit_cable_count());
}

godot::PackedVector3Array ScraperXSimulation::get_kit_cable_points(const std::int64_t cable) const {
    constexpr std::uint32_t kCapacity = 8;
    sim::Vector3 points[kCapacity];
    const std::uint32_t count = simulation_->kit_cable_points(kit_index(cable), points, kCapacity);
    godot::PackedVector3Array out;
    for (std::uint32_t index = 0; index < count; ++index) {
        out.push_back(to_godot(points[index]));
    }
    return out;
}

double ScraperXSimulation::get_well_a_cage_travel() const {
    return simulation_->snapshot().well_a_cage_travel;
}

bool ScraperXSimulation::is_well_a_catch_latched() const {
    return simulation_->snapshot().well_a_catch_latched;
}

std::int64_t ScraperXSimulation::get_well_a_rope_end_entity_id() const {
    return static_cast<std::int64_t>(simulation_->snapshot().well_a_rope_end_entity_id);
}

} // namespace scraperx::bridge
