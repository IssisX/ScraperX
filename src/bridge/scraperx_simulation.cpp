#include "bridge/scraperx_simulation.hpp"
#include "sim/grappling_hook_geometry.hpp"
#include "sim/slingshot.hpp"
#include "sim/supplied_ascent.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <ctime>

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
    static_cast<std::int64_t>(sim::InitialSpawn::FreightYardArrival) + 1;

// A kit index from script: negative or past the end reads as no body.
[[nodiscard]] std::uint32_t kit_index(const std::int64_t index) {
    return index >= 0 && index < static_cast<std::int64_t>(sim::Simulation::kKitNone)
               ? static_cast<std::uint32_t>(index)
               : sim::Simulation::kKitNone;
}

} // namespace

ScraperXSimulation::ScraperXSimulation()
    : simulation_(std::make_unique<sim::Simulation>(sim::InitialSpawn::FreightYardArrival)) {}

void ScraperXSimulation::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("set_supplied_machine_input", "value"), &ScraperXSimulation::set_supplied_machine_input);
    godot::ClassDB::bind_method(godot::D_METHOD("get_supplied_machine_state"), &ScraperXSimulation::get_supplied_machine_state);
    godot::ClassDB::bind_method(godot::D_METHOD("can_restart_gravity_reclaim_attempt"), &ScraperXSimulation::can_restart_gravity_reclaim_attempt);
    godot::ClassDB::bind_method(godot::D_METHOD("restart_gravity_reclaim_attempt"), &ScraperXSimulation::restart_gravity_reclaim_attempt);
    godot::ClassDB::bind_method(godot::D_METHOD("set_service_lift_input", "value"), &ScraperXSimulation::set_service_lift_input);
    godot::ClassDB::bind_method(godot::D_METHOD("get_service_lift_state"), &ScraperXSimulation::get_service_lift_state);
    godot::ClassDB::bind_method(godot::D_METHOD("set_forklift_input", "drive", "steer", "lift", "tilt", "brake"), &ScraperXSimulation::set_forklift_input);
    godot::ClassDB::bind_method(godot::D_METHOD("get_forklift_state"), &ScraperXSimulation::get_forklift_state);
    godot::ClassDB::bind_method(godot::D_METHOD("request_forklift_couple"), &ScraperXSimulation::request_forklift_couple);
    godot::ClassDB::bind_method(godot::D_METHOD("request_forklift_detach"), &ScraperXSimulation::request_forklift_detach);
    godot::ClassDB::bind_method(godot::D_METHOD("cancel_forklift_tool_request"), &ScraperXSimulation::cancel_forklift_tool_request);
    godot::ClassDB::bind_method(godot::D_METHOD("get_forklift_render_state"), &ScraperXSimulation::get_forklift_render_state);
    godot::ClassDB::bind_method(godot::D_METHOD("set_yard_crane_input", "slew", "trolley", "hoist"), &ScraperXSimulation::set_yard_crane_input);
    godot::ClassDB::bind_method(godot::D_METHOD("request_yard_crane_attach"), &ScraperXSimulation::request_yard_crane_attach);
    godot::ClassDB::bind_method(godot::D_METHOD("request_yard_crane_detach"), &ScraperXSimulation::request_yard_crane_detach);
    godot::ClassDB::bind_method(godot::D_METHOD("get_yard_crane_state"), &ScraperXSimulation::get_yard_crane_state);
    godot::ClassDB::bind_method(godot::D_METHOD("restart_service_lift_attempt"), &ScraperXSimulation::restart_service_lift_attempt);
    godot::ClassDB::bind_method(godot::D_METHOD("set_slingshot_input", "draw", "yaw", "elevation"), &ScraperXSimulation::set_slingshot_input);
    godot::ClassDB::bind_method(godot::D_METHOD("request_slingshot_action"), &ScraperXSimulation::request_slingshot_action);
    godot::ClassDB::bind_method(godot::D_METHOD("request_slingshot_drop"), &ScraperXSimulation::request_slingshot_drop);
    godot::ClassDB::bind_method(godot::D_METHOD("get_slingshot_state"), &ScraperXSimulation::get_slingshot_state);
    godot::ClassDB::bind_method(godot::D_METHOD("set_grapple_aim", "x", "y", "z"), &ScraperXSimulation::set_grapple_aim);
    godot::ClassDB::bind_method(godot::D_METHOD("set_grapple_equipped", "equipped"), &ScraperXSimulation::set_grapple_equipped);
    godot::ClassDB::bind_method(godot::D_METHOD("set_grapple_reel_input", "input"), &ScraperXSimulation::set_grapple_reel_input);
    godot::ClassDB::bind_method(godot::D_METHOD("request_grapple_action"), &ScraperXSimulation::request_grapple_action);
    godot::ClassDB::bind_method(godot::D_METHOD("request_grapple_release"), &ScraperXSimulation::request_grapple_release);
    godot::ClassDB::bind_method(godot::D_METHOD("set_grapple_auto_reel", "enabled"), &ScraperXSimulation::set_grapple_auto_reel);
    godot::ClassDB::bind_method(godot::D_METHOD("get_grapple_gun_state"), &ScraperXSimulation::get_grapple_gun_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_grapple_gun_render_state"), &ScraperXSimulation::get_grapple_gun_render_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_grapple_hook_geometry"), &ScraperXSimulation::get_grapple_hook_geometry);
    godot::ClassDB::bind_method(godot::D_METHOD("get_slingshot_render_state"), &ScraperXSimulation::get_slingshot_render_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_landing_state"), &ScraperXSimulation::get_landing_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_slingshot_prediction"), &ScraperXSimulation::get_slingshot_prediction);
    godot::ClassDB::bind_method(godot::D_METHOD("debug_restart_at", "capsule_centre"), &ScraperXSimulation::debug_restart_at);
    godot::ClassDB::bind_method(godot::D_METHOD("restart_checkpoint"), &ScraperXSimulation::restart_checkpoint);
    godot::ClassDB::bind_method(godot::D_METHOD("configure_pipe_bridge_fixture"), &ScraperXSimulation::configure_pipe_bridge_fixture);
    godot::ClassDB::bind_method(godot::D_METHOD("configure_regression_spawn", "initial_spawn"), &ScraperXSimulation::configure_regression_spawn);
    godot::ClassDB::bind_method(godot::D_METHOD("get_pipe_bridge_tip_height"), &ScraperXSimulation::get_pipe_bridge_tip_height);
    godot::ClassDB::bind_method(godot::D_METHOD("get_pipe_bridge_crush_front"), &ScraperXSimulation::get_pipe_bridge_crush_front);
    godot::ClassDB::bind_method(godot::D_METHOD("get_pipe_bridge_retained_pipes"), &ScraperXSimulation::get_pipe_bridge_retained_pipes);
    godot::ClassDB::bind_method(godot::D_METHOD("get_pipe_bridge_audio_state"), &ScraperXSimulation::get_pipe_bridge_audio_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_entity_body_count", "entity"), &ScraperXSimulation::get_entity_body_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_moving_body_count"), &ScraperXSimulation::get_moving_body_count);
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
    godot::ClassDB::bind_method(godot::D_METHOD("set_sprint_input", "held"),
                                &ScraperXSimulation::set_sprint_input);
    godot::ClassDB::bind_method(godot::D_METHOD("get_traversal_left_hand"),
                                &ScraperXSimulation::get_traversal_left_hand);
    godot::ClassDB::bind_method(godot::D_METHOD("get_traversal_right_hand"),
                                &ScraperXSimulation::get_traversal_right_hand);
    godot::ClassDB::bind_method(godot::D_METHOD("get_traversal_left_hand_render_position"),
                                &ScraperXSimulation::get_traversal_left_hand_render_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_traversal_right_hand_render_position"),
                                &ScraperXSimulation::get_traversal_right_hand_render_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_traversal_normal"),
                                &ScraperXSimulation::get_traversal_normal);
    godot::ClassDB::bind_method(godot::D_METHOD("is_player_sprinting"),
                                &ScraperXSimulation::is_player_sprinting);
    godot::ClassDB::bind_method(godot::D_METHOD("is_player_balancing"),
                                &ScraperXSimulation::is_player_balancing);
    godot::ClassDB::bind_method(godot::D_METHOD("is_grip_available"),
                                &ScraperXSimulation::is_grip_available);
    godot::ClassDB::bind_method(godot::D_METHOD("get_grip_point"),
                                &ScraperXSimulation::get_grip_point);
    godot::ClassDB::bind_method(godot::D_METHOD("is_edge_drop_available"),
                                &ScraperXSimulation::is_edge_drop_available);
    godot::ClassDB::bind_method(godot::D_METHOD("advance_frame", "frame_delta_seconds"),
                                &ScraperXSimulation::advance_frame);
    godot::ClassDB::bind_method(godot::D_METHOD("get_last_advance_process_cpu_usec"),
                                &ScraperXSimulation::get_last_advance_process_cpu_usec);
    godot::ClassDB::bind_method(godot::D_METHOD("get_tick_index"),
                                &ScraperXSimulation::get_tick_index);
    godot::ClassDB::bind_method(godot::D_METHOD("get_simulation_time_seconds"),
                                &ScraperXSimulation::get_simulation_time_seconds);
    godot::ClassDB::bind_method(godot::D_METHOD("get_fixed_step_seconds"),
                                &ScraperXSimulation::get_fixed_step_seconds);
    godot::ClassDB::bind_method(godot::D_METHOD("is_initial_world_settled"),
                                &ScraperXSimulation::is_initial_world_settled);
    godot::ClassDB::bind_method(godot::D_METHOD("get_interpolation_alpha"),
                                &ScraperXSimulation::get_interpolation_alpha);
    godot::ClassDB::bind_method(godot::D_METHOD("get_player_position"),
                                &ScraperXSimulation::get_player_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_player_render_position"),
                                &ScraperXSimulation::get_player_render_position);
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

    godot::ClassDB::bind_method(godot::D_METHOD("request_water_screw_toggle"),
                                 &ScraperXSimulation::request_water_screw_toggle);
    godot::ClassDB::bind_method(godot::D_METHOD("is_water_screw_station_active"),
                                 &ScraperXSimulation::is_water_screw_station_active);
    godot::ClassDB::bind_method(godot::D_METHOD("is_water_screw_motor_enabled"),
                                 &ScraperXSimulation::is_water_screw_motor_enabled);
    godot::ClassDB::bind_method(godot::D_METHOD("get_water_screw_shaft_angle_radians"),
                                 &ScraperXSimulation::get_water_screw_shaft_angle_radians);
    godot::ClassDB::bind_method(godot::D_METHOD("get_water_screw_rpm"),
                                 &ScraperXSimulation::get_water_screw_rpm);
    godot::ClassDB::bind_method(godot::D_METHOD("get_water_screw_motor_torque_nm"),
                                 &ScraperXSimulation::get_water_screw_motor_torque_nm);
    godot::ClassDB::bind_method(godot::D_METHOD("get_water_screw_flow_m3_s"),
                                 &ScraperXSimulation::get_water_screw_flow_m3_s);
    godot::ClassDB::bind_method(godot::D_METHOD("get_water_screw_basin_volume_m3"),
                                 &ScraperXSimulation::get_water_screw_basin_volume_m3);
    godot::ClassDB::bind_method(godot::D_METHOD("get_water_screw_tank_volume_m3"),
                                 &ScraperXSimulation::get_water_screw_tank_volume_m3);
    godot::ClassDB::bind_method(godot::D_METHOD("get_water_screw_leakage_m3"),
                                 &ScraperXSimulation::get_water_screw_leakage_m3);
    godot::ClassDB::bind_method(godot::D_METHOD("get_water_screw_shaft_work_j"),
                                 &ScraperXSimulation::get_water_screw_shaft_work_j);

    godot::ClassDB::bind_method(godot::D_METHOD("request_water_lift_valve_toggle"),
                                &ScraperXSimulation::request_water_lift_valve_toggle);
    godot::ClassDB::bind_method(godot::D_METHOD("request_water_lift_release"),
                                &ScraperXSimulation::request_water_lift_release);
    godot::ClassDB::bind_method(godot::D_METHOD("request_water_lift_reset"),
                                &ScraperXSimulation::request_water_lift_reset);
    godot::ClassDB::bind_method(godot::D_METHOD("is_water_lift_valve_station_active"),
                                &ScraperXSimulation::is_water_lift_valve_station_active);
    godot::ClassDB::bind_method(godot::D_METHOD("is_water_lift_release_station_active"),
                                &ScraperXSimulation::is_water_lift_release_station_active);
    godot::ClassDB::bind_method(godot::D_METHOD("is_water_lift_reset_station_active"),
                                &ScraperXSimulation::is_water_lift_reset_station_active);
    godot::ClassDB::bind_method(godot::D_METHOD("is_water_lift_valve_open"),
                                &ScraperXSimulation::is_water_lift_valve_open);
    godot::ClassDB::bind_method(godot::D_METHOD("get_water_lift_valve_flow_m3_s"),
                                &ScraperXSimulation::get_water_lift_valve_flow_m3_s);
    godot::ClassDB::bind_method(godot::D_METHOD("get_water_lift_bucket_water_m3"),
                                &ScraperXSimulation::get_water_lift_bucket_water_m3);
    godot::ClassDB::bind_method(godot::D_METHOD("get_water_lift_bucket_mass_kg"),
                                &ScraperXSimulation::get_water_lift_bucket_mass_kg);
    godot::ClassDB::bind_method(godot::D_METHOD("get_water_lift_bucket_travel_m"),
                                &ScraperXSimulation::get_water_lift_bucket_travel_m);
    godot::ClassDB::bind_method(godot::D_METHOD("get_water_lift_cage_travel_m"),
                                &ScraperXSimulation::get_water_lift_cage_travel_m);
    godot::ClassDB::bind_method(godot::D_METHOD("get_water_lift_cage_peak_speed_mps"),
                                &ScraperXSimulation::get_water_lift_cage_peak_speed_mps);
    godot::ClassDB::bind_method(godot::D_METHOD("get_water_lift_rope_tension_n"),
                                &ScraperXSimulation::get_water_lift_rope_tension_n);
    godot::ClassDB::bind_method(godot::D_METHOD("is_water_lift_bucket_catch_latched"),
                                &ScraperXSimulation::is_water_lift_bucket_catch_latched);
    godot::ClassDB::bind_method(godot::D_METHOD("is_water_lift_upper_catch_latched"),
                                &ScraperXSimulation::is_water_lift_upper_catch_latched);

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
    godot::ClassDB::bind_method(godot::D_METHOD("get_cargo_net_vertices"), &ScraperXSimulation::get_cargo_net_vertices);
    godot::ClassDB::bind_method(godot::D_METHOD("get_cargo_net_indices"), &ScraperXSimulation::get_cargo_net_indices);
    godot::ClassDB::bind_method(godot::D_METHOD("get_catch_net_vertices"), &ScraperXSimulation::get_catch_net_vertices);
    godot::ClassDB::bind_method(godot::D_METHOD("get_catch_net_indices"), &ScraperXSimulation::get_catch_net_indices);
    godot::ClassDB::bind_method(godot::D_METHOD("get_catch_net_tether_vertices"), &ScraperXSimulation::get_catch_net_tether_vertices);
    godot::ClassDB::bind_method(godot::D_METHOD("get_catch_net_patch_vertices"), &ScraperXSimulation::get_catch_net_patch_vertices);
    godot::ClassDB::bind_method(godot::D_METHOD("get_catch_net_state"), &ScraperXSimulation::get_catch_net_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_body_count"),
                                &ScraperXSimulation::get_kit_body_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_body_entity_id", "body"),
                                &ScraperXSimulation::get_kit_body_entity_id);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_body_material_key", "body"),
                                &ScraperXSimulation::get_kit_body_material_key);
    godot::ClassDB::bind_method(godot::D_METHOD("is_kit_body_dynamic", "body"),
                                &ScraperXSimulation::is_kit_body_dynamic);
    godot::ClassDB::bind_method(godot::D_METHOD("is_kit_body_enabled", "body"),
                                &ScraperXSimulation::is_kit_body_enabled);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_body_parts", "body"),
                                &ScraperXSimulation::get_kit_body_parts);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_body_part_mesh", "body", "part"),
                                &ScraperXSimulation::get_kit_body_part_mesh);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_body_linear_velocity", "body"),
                                &ScraperXSimulation::get_kit_body_linear_velocity);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_body_transform", "body"),
                                &ScraperXSimulation::get_kit_body_transform);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_body_render_transform", "body"),
                                &ScraperXSimulation::get_kit_body_render_transform);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_carry_grip_position", "body"),
                                &ScraperXSimulation::get_kit_carry_grip_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_body_index", "entity_id"),
                                &ScraperXSimulation::get_kit_body_index);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_cable_count"),
                                &ScraperXSimulation::get_kit_cable_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_cable_points", "cable"),
                                &ScraperXSimulation::get_kit_cable_points);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_cable_render_points", "cable"),
                                &ScraperXSimulation::get_kit_cable_render_points);
    godot::ClassDB::bind_method(godot::D_METHOD("get_well_a_cage_travel"),
                                &ScraperXSimulation::get_well_a_cage_travel);
    godot::ClassDB::bind_method(godot::D_METHOD("is_well_a_catch_latched"),
                                &ScraperXSimulation::is_well_a_catch_latched);
    godot::ClassDB::bind_method(godot::D_METHOD("get_well_a_rope_end_entity_id"),
                                &ScraperXSimulation::get_well_a_rope_end_entity_id);
}

bool ScraperXSimulation::configure_regression_spawn(const std::int64_t initial_spawn) {
    if (initial_spawn < 0 || initial_spawn >= kInitialSpawnCount ||
        simulation_->snapshot().tick_index != 0) {
        return false;
    }
    simulation_ = std::make_unique<sim::Simulation>(
        static_cast<sim::InitialSpawn>(initial_spawn), sim::WorldContent::RegressionFixtures);
    return true;
}

double ScraperXSimulation::get_pipe_bridge_tip_height() const {
    return simulation_->pipe_bridge_tip_height();
}

double ScraperXSimulation::get_pipe_bridge_crush_front() const {
    return simulation_->pipe_bridge_crush_front();
}

godot::Vector3 ScraperXSimulation::get_kit_carry_grip_position(const std::int64_t body) const {
    return to_godot(simulation_->kit_carry_grip_position(kit_index(body)));
}


std::int64_t ScraperXSimulation::get_pipe_bridge_retained_pipes() const {
    return simulation_->pipe_bridge_retained_pipes();
}

godot::Dictionary ScraperXSimulation::get_pipe_bridge_audio_state() const {
    godot::Dictionary out;
    const auto pan = simulation_->kit_body_index(2502);
    if (pan == sim::Simulation::kKitNone) return out;
    double energy = 0;
    for (std::uint64_t pipe = 2509; pipe <= 2528; ++pipe)
        energy += simulation_->kit_body_kinetic_energy(simulation_->kit_body_index(pipe));
    out["pan"] = to_godot(simulation_->kit_body_position(pan));
    out["velocity"] = to_godot(simulation_->kit_body_velocity(pan));
    out["pipe_energy"] = energy;
    out["crush_front"] = simulation_->pipe_bridge_crush_front();
    return out;
}

std::int64_t ScraperXSimulation::get_entity_body_count(const std::int64_t entity) const {
    return simulation_->entity_body_count(static_cast<std::uint64_t>(entity));
}

std::int64_t ScraperXSimulation::get_moving_body_count() const {
    return simulation_->moving_body_count();
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

bool ScraperXSimulation::debug_restart_at(const godot::Vector3 centre) {
    return simulation_->debug_restart_at({centre.x, centre.y, centre.z});
}

bool ScraperXSimulation::restart_checkpoint() {
    return simulation_->restart_checkpoint();
}

bool ScraperXSimulation::configure_pipe_bridge_fixture() {
    if (simulation_->snapshot().tick_index != 0) return false;
    simulation_ = std::make_unique<sim::Simulation>(
        sim::InitialSpawn::ExteriorGrade, sim::WorldContent::PipeBridge);
    return true;
}

bool ScraperXSimulation::set_slingshot_input(double draw, double yaw, double elevation) {
    return simulation_->set_slingshot_input(draw, yaw, elevation);
}
bool ScraperXSimulation::set_supplied_machine_input(double value){return simulation_->set_supplied_machine_input(value);}
bool ScraperXSimulation::can_restart_gravity_reclaim_attempt() const {
    return simulation_->can_restart_gravity_reclaim_attempt();
}
bool ScraperXSimulation::restart_gravity_reclaim_attempt() {
    return simulation_->restart_gravity_reclaim_attempt();
}
godot::Dictionary ScraperXSimulation::get_supplied_machine_state()const {
 const auto&s=simulation_->snapshot();godot::Dictionary out;
 out["station"]=static_cast<std::int64_t>(s.supplied_machine_station);
 out["reachable_station"]=static_cast<std::int64_t>(s.supplied_machine_reachable_station);
 out["surface_y"]=s.supplied_machine_surface_y;out["energy_j"]=s.supplied_machine_energy_j;
 out["capacity_j"]=s.supplied_machine_capacity_j;out["power_w"]=s.supplied_machine_power_w;
 out["positive_work_j"]=s.supplied_machine_positive_work_j;out["heat_j"]=s.supplied_machine_heat_j;
 out["force_n"]=s.supplied_machine_force_n;out["energy_overdraft_j"]=s.supplied_machine_energy_overdraft_j;
 out["cart_brake_heat_j"]=s.supplied_machine_cart_brake_heat_j;out["cart_brake_positive_residual_j"]=s.supplied_machine_cart_brake_residual_j;
 out["cart_actuator_rounding_residual_j"]=s.supplied_machine_cart_actuator_rounding_residual_j;
 out["braking"]=s.supplied_machine_braking;out["energy_cutoff"]=s.supplied_machine_energy_cutoff;
 out["wheel_motor_enabled"]=s.supplied_machine_wheel_motor_enabled;
 out["rubble_impact_count"]=static_cast<std::int64_t>(s.reclaim_impact_count);out["rubble_impact_speed_mps"]=s.reclaim_impact_speed_mps;out["rubble_impact_position"]=to_godot(s.reclaim_impact_position);
 out["rubble_break_serial"]=static_cast<std::int64_t>(s.reclaim_break_serial);out["rubble_break_valid"]=s.reclaim_break_valid;
 out["rubble_break_position"]=to_godot(s.reclaim_break_position);out["rubble_break_force_n"]=s.reclaim_break_force_n;out["rubble_break_torque_nm"]=s.reclaim_break_torque_nm;
 out["hopper_mass_kg"]=s.supplied_machine_hopper_mass_kg;out["wheel_angle_rad"]=s.supplied_machine_wheel_angle_rad;out["bearing_heat_j"]=s.supplied_machine_bearing_heat_j;
 out["index"]=s.supplied_machine_index;out["name"]=scraperx::sim::SuppliedAscent::name(s.supplied_machine_index);return out;
}
bool ScraperXSimulation::set_service_lift_input(double value) {
    return simulation_->set_service_lift_input(value);
}

namespace {
godot::Dictionary forklift_dictionary(const sim::ForkliftSnapshot &state) {
    godot::Dictionary out;
    out["available"] = state.available;
    out["controls_reachable"] = state.controls_reachable;
    out["chassis_entity"] = static_cast<std::int64_t>(state.chassis_entity);
    out["station_position"] = to_godot(state.station_position);
    out["steering_mount"] = to_godot(state.steering_mount);
    out["lift_lever_mount"] = to_godot(state.lift_lever_mount);
    out["tilt_lever_mount"] = to_godot(state.tilt_lever_mount);
    out["battery_j"] = state.battery_j;
    out["capacity_j"] = state.capacity_j;
    out["lift_position_m"] = state.lift_position_m;
    out["tilt_angle_rad"] = state.tilt_angle_rad;
    out["lift_force_n"] = state.lift_force_n;
    out["tilt_torque_nm"] = state.tilt_torque_nm;
    out["energy_cutoff"] = state.energy_cutoff;
    out["coupled"] = state.coupled;
    out["tool_entity"] = static_cast<std::int64_t>(state.tool_entity);
    out["coupling_generation"] = static_cast<std::int64_t>(state.coupling_generation);
    out["can_couple"] = state.can_couple;
    out["can_detach"] = state.can_detach;
    out["tool_refusal"] = state.tool_refusal;
    out["applied_drive"] = state.applied_drive;
    out["applied_brake"] = state.applied_brake;
    out["forward_speed_mps"] = state.forward_speed_mps;
    out["steering_rad"] = state.steering_rad;
    out["lift_target_mps"] = state.lift_target_mps;
    out["tilt_target_rad_s"] = state.tilt_target_rad_s;
    out["positive_work_j"] = state.positive_work_j;
    out["reservation_excess_j"] = state.reservation_excess_j;
    godot::Array wheels;
    for (const auto &wheel : state.wheels) {
        godot::Dictionary receipt;
        receipt["transform"] = godot::Transform3D(godot::Basis(to_godot(wheel.rotation)), to_godot(wheel.position));
        receipt["contact"] = wheel.contact;
        receipt["radius_m"] = wheel.radius_m;
        receipt["width_m"] = wheel.width_m;
        receipt["normal_force_n"] = wheel.normal_force_n;
        receipt["contact_body"] = static_cast<std::int64_t>(wheel.contact_body);
        receipt["contact_position"] = to_godot(wheel.contact_position);
        receipt["contact_normal"] = to_godot(wheel.contact_normal);
        receipt["angular_velocity_rad_s"] = wheel.angular_velocity_rad_s;
        receipt["steer_rad"] = wheel.steer_rad;
        wheels.append(receipt);
    }
    out["wheels"] = wheels;
    return out;
}
} // namespace

bool ScraperXSimulation::set_forklift_input(double drive, double steer, double lift, double tilt, double brake) {
    return simulation_->set_forklift_input(drive, steer, lift, tilt, brake);
}
bool ScraperXSimulation::request_forklift_couple() {
    return simulation_->request_forklift_couple();
}
bool ScraperXSimulation::request_forklift_detach() {
    return simulation_->request_forklift_detach();
}
void ScraperXSimulation::cancel_forklift_tool_request() {
    simulation_->cancel_forklift_tool_request();
}
godot::Dictionary ScraperXSimulation::get_forklift_state() const {
    return forklift_dictionary(simulation_->forklift_state());
}
godot::Dictionary ScraperXSimulation::get_forklift_render_state() const {
    return forklift_dictionary(simulation_->render_forklift_state());
}
bool ScraperXSimulation::restart_service_lift_attempt() {
    return simulation_->restart_service_lift_attempt();
}
godot::Dictionary ScraperXSimulation::get_service_lift_state() const {
    const auto &state = simulation_->snapshot();
    godot::Dictionary out;
    out["station"] = static_cast<std::int64_t>(state.service_lift_station);
    out["reachable_station"] = static_cast<std::int64_t>(state.service_lift_reachable_station);
    out["retry_available"] = state.service_lift_retry_available;
    out["surface_y"] = state.service_lift_surface_y;
    out["energy_j"] = state.service_lift_energy_j;
    out["capacity_j"] = state.service_lift_capacity_j;
    out["force_n"] = state.service_lift_force_n;
    out["power_w"] = state.service_lift_power_w;
    out["braking"] = state.service_lift_braking;
    out["energy_cutoff"] = state.service_lift_energy_cutoff;
    return out;
}
bool ScraperXSimulation::request_slingshot_action() {
    return simulation_->request_slingshot_action();
}
bool ScraperXSimulation::request_slingshot_drop() {
    return simulation_->request_slingshot_drop();
}
bool ScraperXSimulation::set_grapple_aim(double x, double y, double z) {
    return simulation_->set_grapple_aim(x, y, z);
}
bool ScraperXSimulation::set_grapple_equipped(bool equipped) {
    return simulation_->set_grapple_equipped(equipped);
}
bool ScraperXSimulation::set_grapple_reel_input(double input) {
    return simulation_->set_grapple_reel_input(input);
}
bool ScraperXSimulation::request_grapple_action() { return simulation_->request_grapple_action(); }
bool ScraperXSimulation::set_grapple_auto_reel(bool enabled) { return simulation_->set_grapple_auto_reel(enabled); }
bool ScraperXSimulation::request_grapple_release() { return simulation_->request_grapple_release(); }

namespace {
godot::Dictionary grapple_dictionary(const scraperx::sim::GrappleGunSnapshot &state) {
    godot::Dictionary out;
    const char *const names[]{"READY", "PAYING OUT", "LOCKED", "RELEASED", "FAILED"};
    const char *const failures[]{"", "INVALID AIM", "NO CARTRIDGE", "LINE BUSY", "MUZZLE BLOCKED",
        "HOOK UNAVAILABLE", "LINE SNAGGED", "LINE BROKEN", "HOOK OUT OF REACH", "LINE LOADED",
        "HOOK BLOCKED", "HOOK MOVING"};
    out["available"] = state.available;
    out["acquired"] = state.acquired;
    out["equipped"] = state.equipped;
    out["station_available"] = state.station_available;
    out["rack_position"] = godot::Vector3(state.rack_position.x, state.rack_position.y, state.rack_position.z);
    out["launch_origin"] = godot::Vector3(state.launch_origin.x, state.launch_origin.y, state.launch_origin.z);
    out["launch_direction"] = godot::Vector3(state.launch_direction.x, state.launch_direction.y, state.launch_direction.z);
    out["line_attached"] = state.line_attached;
    out["action_recovers_hook"] = state.action_recovers_hook;
    out["state"] = state.state;
    out["state_name"] = names[std::min(state.state, 4U)];
    out["last_failure"] = failures[std::min(state.failure, 11U)];
    out["cartridges_remaining"] = state.cartridges;
    out["hooks_remaining"] = state.available_hooks;
    out["rope_start"] = godot::Vector3(state.player_endpoint.x, state.player_endpoint.y, state.player_endpoint.z);
    out["rope_end"] = godot::Vector3(state.hook_endpoint.x, state.hook_endpoint.y, state.hook_endpoint.z);
    out["paid_out_length"] = state.line_length_m;
    out["tension_n"] = state.tension_n;
    out["extension_m"] = state.extension_m;
    out["launch_energy_j"] = state.last_launch_work_j;
    out["total_launch_work_j"] = state.launch_work_j;
    out["reel_work_j"] = state.reel_work_j;
    out["auto_reel"] = state.auto_reel;
    out["elastic_energy_j"] = state.stored_elastic_energy_j;
    out["spool_absorbed_energy_j"] = state.spool_absorbed_energy_j;
    out["release_discarded_elastic_energy_j"] = state.release_discarded_elastic_energy_j;
    godot::Array hooks;
    for (const auto &hook : state.hooks) {
        godot::Dictionary pose;
        pose["active"] = hook.present;
        pose["position"] = godot::Vector3(hook.position.x, hook.position.y, hook.position.z);
        pose["rotation"] = godot::Quaternion(hook.rotation.x, hook.rotation.y, hook.rotation.z, hook.rotation.w);
        hooks.push_back(pose);
    }
    out["hooks"] = hooks;
    return out;
}
}
godot::Dictionary ScraperXSimulation::get_grapple_gun_state() const {
    return grapple_dictionary(simulation_->grapple_gun_state());
}
godot::Dictionary ScraperXSimulation::get_grapple_gun_render_state() const {
    return grapple_dictionary(simulation_->render_grapple_gun_state());
}

godot::Array ScraperXSimulation::get_grapple_hook_geometry() const {
    godot::Array out;
    for (const auto &part : sim::grappling_hook_layout().parts) {
        godot::PackedVector3Array polygon;
        for (const auto &point : part.polygon) {
            polygon.push_back(godot::Vector3(point.GetX(), point.GetY(), point.GetZ()));
        }
        godot::Dictionary prism;
        prism["polygon"] = polygon;
        prism["z_low"] = part.z_low;
        prism["z_high"] = part.z_high;
        out.push_back(prism);
    }
    return out;
}

godot::Dictionary ScraperXSimulation::get_slingshot_state() const {
    const auto s = simulation_->slingshot_state();
    godot::Dictionary out;
    out["seat_surface_position"] = to_godot(s.seat_surface_position);
    out["harness_rest_local"] = to_godot(s.harness_rest_local);
    out["rider_specific_acceleration"] = to_godot(s.rider_specific_acceleration);
    out["simulation_time_seconds"] = s.simulation_time_seconds;
    out["fixed_step_seconds"] = s.fixed_step_seconds;
    out["tick_index"] = s.tick_index;
    out["available"] = s.available;
    out["station_available"] = s.station_available;
    out["seated"] = s.seated;
    out["drawing"] = s.drawing;
    out["released"] = s.released;
    out["can_retrieve"] = s.can_retrieve;
    out["recovering"] = s.recovering;
    out["guided_launch"] = s.guided_launch;
    out["track_exit"] = s.track_exit;
    out["release_ready"] = s.release_ready;
    out["ledger_valid"] = s.ledger_valid;
    out["pouch_pair_excluded"] = s.pouch_pair_excluded;
    out["aim_control_work_j"] = s.aim_control_work_j;
    out["aim_ready"] = s.aim_ready;
    out["aim_locked"] = s.aim_locked;
    out["aim_source_power_w"] = s.aim_source_power_w;
    out["target_yaw_rad"] = s.target_yaw_rad;
    out["target_elevation_rad"] = s.target_elevation_rad;
    out["launch_track_start"] = to_godot(s.launch_track_start);
    out["launch_track_end"] = to_godot(s.launch_track_end);
    out["max_draw_m"] = s.max_draw_m;
    out["max_source_power_w"] = s.max_source_power_w;
    out["power_limit_w"] = s.max_source_power_w;
    out["band_rest_m"] = s.band_rest_m;
    out["neutral_position"] = to_godot(s.neutral_position);
    out["retrieval_control_position"] = to_godot(s.retrieval_control_position);
    out["retrieval_work_j"] = s.retrieval_work_j;
    out["retrieval_source_power_w"] = s.retrieval_source_power_w;
    out["energy_residual_j"] = s.energy_residual_j;
    out["draw_m"] = s.draw_m;
    out["energy_j"] = s.energy_j;
    out["work_j"] = s.work_j;
    out["source_power_w"] = s.source_power_w;
    out["yaw_rad"] = s.yaw_rad;
    out["elevation_rad"] = s.elevation_rad;
    out["launch_count"] = s.launch_count;
    out["anchor_left"] = to_godot(s.anchor_left);
    out["anchor_right"] = to_godot(s.anchor_right);
    out["leather_deflection_m"] = s.leather_deflection_m;
    out["leather_energy_j"] = s.leather_energy_j;
    out["pouch_position"] = to_godot(s.pouch_position);
    return out;
}
godot::PackedVector3Array ScraperXSimulation::get_slingshot_prediction() const {
    godot::PackedVector3Array out;
    for (const auto point : simulation_->slingshot_prediction()) out.push_back(to_godot(point));
    return out;
}

godot::Dictionary ScraperXSimulation::get_slingshot_render_state() const {
    auto out = get_slingshot_state();
    godot::PackedVector3Array leather_vertices;
    for (int row=0; row<=8; ++row) for (int column=0; column<=18; ++column) {
        const auto v = sim::Slingshot::leather_surface(column/18.0F, row/8.0F);
        leather_vertices.push_back(godot::Vector3(v.GetX(),v.GetY(),v.GetZ()));
    }
    out["leather_vertices"] = leather_vertices;
    const auto s = simulation_->render_slingshot_state();
    out["seat_surface_position"] = to_godot(s.seat_surface_position);
    out["harness_rest_local"] = to_godot(s.harness_rest_local);
    out["rider_specific_acceleration"] = to_godot(s.rider_specific_acceleration);
    out["simulation_time_seconds"] = s.simulation_time_seconds;
    out["fixed_step_seconds"] = s.fixed_step_seconds;
    out["tick_index"] = s.tick_index;
    out["leather_deflection_m"] = s.leather_deflection_m;
    out["leather_energy_j"] = s.leather_energy_j;
    out["pouch_position"] = to_godot(s.pouch_position);
    out["anchor_left"] = to_godot(s.anchor_left);
    out["anchor_right"] = to_godot(s.anchor_right);
    return out;
}

godot::Dictionary ScraperXSimulation::get_landing_state() const {
    const auto &s = simulation_->snapshot();
    godot::Dictionary out;
    out["support_sound_surface"] = static_cast<std::int64_t>(simulation_->support_sound_surface());
    out["landing_sound_surface"] = static_cast<std::int64_t>(simulation_->landing_sound_surface());
    out["player_swinging"] = s.player_swinging;
    out["checkpoint_footing_valid"] = s.checkpoint_footing_valid;
    out["player_gravity_factor"] = s.player_gravity_factor;
    out["traversal_hand_constraint_count"] = s.traversal_hand_constraint_count;
    out["traversal_plan_kind"] = s.traversal_plan_kind;
    out["traversal_phase"] = s.traversal_phase;
    out["traversal_rejection_reason"] = s.traversal_rejection_reason;
    out["traversal_plan_generation"] = s.traversal_plan_generation;
    out["traversal_receiver_entity_id"] = s.traversal_receiver_entity_id;
    out["foot_contact_tick"] = s.foot_contact_tick;
    out["foot_push_source_contact_tick"] = s.foot_push_source_contact_tick;
    out["hand_foot_overlap_active"] = s.hand_foot_overlap_active;
    out["air_catch_intent_pending"] = s.air_catch_intent_pending;
    out["hand_foot_overlap_solved_ticks"] = s.hand_foot_overlap_solved_ticks;
    out["hand_foot_last_solve_tick"] = s.hand_foot_last_solve_tick;
    out["hand_foot_last_hand_load_n"] = s.hand_foot_last_hand_load_n;
    out["hand_foot_last_foot_load_n"] = s.hand_foot_last_foot_load_n;
    out["actual_hand_load_n"] = s.actual_hand_load_n;
    out["actual_foot_load_n"] = s.actual_foot_load_n;
    out["actual_hand_load_fraction"] = s.actual_hand_load_fraction;
    out["actual_foot_load_fraction"] = s.actual_foot_load_fraction;
    out["palm_left_active"] = s.hand_props[0].active;
    out["palm_right_active"] = s.hand_props[1].active;
    out["palm_left_normal"] = to_godot(s.hand_props[0].normal);
    out["palm_right_normal"] = to_godot(s.hand_props[1].normal);
    out["palm_left_force_n"] = to_godot(s.hand_props[0].player_force);
    out["palm_right_force_n"] = to_godot(s.hand_props[1].player_force);
    out["palm_left_generation"] = s.hand_props[0].generation;
    out["palm_right_generation"] = s.hand_props[1].generation;
    out["palm_vault_count"] = s.palm_vault_count;
    const bool physical_hand_state = s.traversal_hand_constraint_count > 0 &&
        (s.traversal_state == sim::TraversalState::Climbing || s.traversal_state == sim::TraversalState::Hanging);
    // Both slots are real whenever the backend reports two attachments,
    // including Lowering whose legacy point presentation has no generations.
    out["left_hand_attached"] = s.traversal_hand_constraint_count == 2 ||
        (physical_hand_state && s.traversal_left_hand_generation > 0);
    out["right_hand_attached"] = s.traversal_hand_constraint_count == 2 ||
        (physical_hand_state && s.traversal_right_hand_generation > 0);
    out["left_hand_generation"] = s.traversal_left_hand_generation;
    out["right_hand_generation"] = s.traversal_right_hand_generation;
    out["traversal_command_work_bound_j"] = s.traversal_command_work_bound_j;
    out["left_hand_force_n"] = to_godot(s.traversal_left_hand_force);
    out["right_hand_force_n"] = to_godot(s.traversal_right_hand_force);
    out["hand_actuator_positive_work_j"] = s.traversal_actuator_positive_work_j;
    out["hand_actuator_absorbed_work_j"] = s.traversal_actuator_absorbed_work_j;
    out["hanging_free_arc_active"] = s.hanging_free_arc_active;
    out["missing_hand_grip_available"] = s.missing_hand_grip_available;
    out["hanging_commanded_span_m"] = s.hanging_commanded_span_m;
    out["hanging_actual_span_m"] = s.hanging_actual_span_m;
    out["swing_tuck_available"] = s.swing_tuck_available;
    out["swing_tuck_requested"] = s.swing_tuck_requested;
    out["swing_body_below_grip_m"] = s.swing_body_below_grip_m;
    out["swing_commanded_body_below_grip_m"] = s.swing_commanded_body_below_grip_m;
    out["plank_broken_joint_mask"] = s.plank_broken_joint_mask;
    out["plank_peak_strength_ratio"] = s.plank_peak_strength_ratio;
    out["plank_fracture_count"] = s.plank_fracture_count;
    out["plank_discarded_strain_energy_j"] = s.plank_discarded_strain_energy_j;
    out["foot_push_active"] = s.foot_push_active;
    const auto foot_anchor = simulation_->render_foot_anchor();
    out["foot_push_world_anchor"] = to_godot(foot_anchor.point);
    out["foot_push_world_normal"] = to_godot(foot_anchor.normal);
    out["foot_push_start_count"] = s.foot_push_start_count;
    out["foot_push_support_entity_id"] = s.foot_push_support_entity_id;
    out["foot_push_stop_reason"] = s.foot_push_stop_reason;
    out["foot_push_command_work_bound_j"] = s.foot_push_command_work_bound_j;
    out["foot_push_stroke_m"] = s.foot_push_stroke_m;
    out["foot_push_stroke_limit_m"] = s.foot_push_stroke_limit_m;
    out["foot_push_peak_load_n"] = s.foot_push_peak_load_n;
    out["foot_push_last_load_n"] = s.foot_push_last_load_n;
    out["foot_push_last_impulse_ns"] = s.foot_push_last_impulse_ns;
    out["foot_push_elapsed_seconds"] = s.foot_push_elapsed_seconds;
    out["foot_push_actual_distance_m"] = s.foot_push_actual_distance_m;
    out["foot_push_initial_distance_m"] = s.foot_push_initial_distance_m;
    out["foot_push_player_force"] = to_godot(s.foot_push_player_force);
    out["foot_transfer_count"] = s.foot_transfer_count;
    out["foot_transfer_tick"] = s.foot_transfer_tick;
    out["foot_transfer_support_entity_id"] = s.foot_transfer_support_entity_id;
    out["foot_transfer_peak_hand_load_n"] = s.foot_transfer_peak_hand_load_n;
    out["landing_response"] = s.landing_response;
    out["player_sliding"] = s.player_sliding;
    out["slide_speed_mps"] = s.slide_speed_mps;
    out["slide_count"] = s.slide_count;
    out["wall_stride_active"] = s.wall_stride_active;
    out["wall_stride_pushing_off"] = s.wall_stride_pushing_off;
    out["wall_stride_count"] = s.wall_stride_count;
    out["wall_stride_normal"] = to_godot(s.wall_stride_normal);
    out["landing_recovery_progress"] = s.landing_recovery_progress;
    out["landing_count"] = s.landing_count;
    out["landing_support_entity_id"] = s.landing_support_entity_id;
    out["landing_approach_energy_j"] = s.landing_approach_energy_j;
    out["landing_tangent_energy_j"] = s.landing_tangent_energy_j;
    out["landing_normal_speed_mps"] = s.landing_normal_speed_mps;
    out["landing_tangent_speed_mps"] = s.landing_tangent_speed_mps;
    out["landing_observed_normal_impulse_ns"] = s.landing_observed_normal_impulse_ns;
    out["landing_balance"] = s.landing_balance;
    out["landing_recovery_seconds"] = s.landing_recovery_seconds;
    out["landing_recovering"] = s.landing_recovering;
    out["landing_slip_velocity"] = to_godot(s.landing_slip_velocity);
    out["landing_recovery_work_j"] = s.landing_recovery_work_j;
    out["landing_jump_work_j"] = s.landing_jump_work_j;
    out["air_control_command_positive_work_j"] = s.air_control_command_positive_work_j;
    out["air_control_command_absorbed_work_j"] = s.air_control_command_absorbed_work_j;
    out["air_control_force_n"] = to_godot(s.air_control_force_n);
    return out;
}

bool ScraperXSimulation::request_parachute() {
    return simulation_->request_parachute();
}

bool ScraperXSimulation::set_crouch_input(const bool held) {
    return simulation_->set_crouch_input(held);
}

bool ScraperXSimulation::set_sprint_input(const bool held) {
    return simulation_->set_sprint_input(held);
}

std::int64_t ScraperXSimulation::advance_frame(const double frame_delta_seconds) {
    last_advance_process_cpu_usec_ = -1;
    const auto cpu_start = std::clock();
    const auto result = simulation_->advance_frame(frame_delta_seconds);
    const auto cpu_end = std::clock();
    if (!result.accepted) {
        godot::UtilityFunctions::push_error(
            "ScraperX native authority rejected an invalid frame delta; state was not mutated.");
        return -1;
    }
    if (cpu_start != static_cast<std::clock_t>(-1) &&
        cpu_end != static_cast<std::clock_t>(-1) && cpu_end >= cpu_start) {
        const double elapsed_usec = (static_cast<double>(cpu_end) - static_cast<double>(cpu_start)) *
                                    1000000.0 / static_cast<double>(CLOCKS_PER_SEC);
        last_advance_process_cpu_usec_ = static_cast<std::int64_t>(elapsed_usec);
    }
    return static_cast<std::int64_t>(result.steps_advanced);
}

std::int64_t ScraperXSimulation::get_last_advance_process_cpu_usec() const {
    return last_advance_process_cpu_usec_;
}

bool ScraperXSimulation::is_initial_world_settled() const {
    return simulation_->is_initial_world_settled();
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

godot::Vector3 ScraperXSimulation::get_player_render_position() const {
    return to_godot(simulation_->render_player_position());
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

godot::Vector3 ScraperXSimulation::get_traversal_left_hand() const {
    return to_godot(simulation_->snapshot().traversal_left_hand);
}

godot::Vector3 ScraperXSimulation::get_traversal_right_hand() const {
    return to_godot(simulation_->snapshot().traversal_right_hand);
}

godot::Vector3 ScraperXSimulation::get_traversal_left_hand_render_position() const {
    return to_godot(simulation_->render_traversal_hand(true));
}

godot::Vector3 ScraperXSimulation::get_traversal_right_hand_render_position() const {
    return to_godot(simulation_->render_traversal_hand(false));
}

godot::Vector3 ScraperXSimulation::get_traversal_normal() const {
    return to_godot(simulation_->snapshot().traversal_normal);
}

bool ScraperXSimulation::is_player_sprinting() const {
    return simulation_->snapshot().player_sprinting;
}

bool ScraperXSimulation::is_player_balancing() const {
    return simulation_->snapshot().player_balancing;
}

bool ScraperXSimulation::is_grip_available() const {
    return simulation_->snapshot().grip_available;
}

godot::Vector3 ScraperXSimulation::get_grip_point() const {
    return to_godot(simulation_->snapshot().grip_point);
}

bool ScraperXSimulation::is_edge_drop_available() const {
    return simulation_->snapshot().edge_drop_available;
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

bool ScraperXSimulation::request_water_screw_toggle() {
    return simulation_->request_water_screw_toggle();
}
bool ScraperXSimulation::is_water_screw_station_active() const {
    return simulation_->snapshot().water_screw_station_active;
}
bool ScraperXSimulation::is_water_screw_motor_enabled() const {
    return simulation_->snapshot().water_screw_motor_enabled;
}
double ScraperXSimulation::get_water_screw_shaft_angle_radians() const {
    return simulation_->snapshot().water_screw_shaft_angle_radians;
}
double ScraperXSimulation::get_water_screw_rpm() const {
    return simulation_->snapshot().water_screw_rpm;
}
double ScraperXSimulation::get_water_screw_motor_torque_nm() const {
    return simulation_->snapshot().water_screw_motor_torque_nm;
}
double ScraperXSimulation::get_water_screw_flow_m3_s() const {
    return simulation_->snapshot().water_screw_flow_m3_s;
}
double ScraperXSimulation::get_water_screw_basin_volume_m3() const {
    return simulation_->snapshot().water_screw_basin_volume_m3;
}
double ScraperXSimulation::get_water_screw_tank_volume_m3() const {
    return simulation_->snapshot().water_screw_tank_volume_m3;
}
double ScraperXSimulation::get_water_screw_leakage_m3() const {
    return simulation_->snapshot().water_screw_leakage_m3;
}
double ScraperXSimulation::get_water_screw_shaft_work_j() const {
    return simulation_->snapshot().water_screw_shaft_work_j;
}

bool ScraperXSimulation::request_water_lift_valve_toggle() {
    return simulation_->request_water_lift_valve_toggle();
}
bool ScraperXSimulation::request_water_lift_release() {
    return simulation_->request_water_lift_release();
}
bool ScraperXSimulation::request_water_lift_reset() {
    return simulation_->request_water_lift_reset();
}
bool ScraperXSimulation::is_water_lift_valve_station_active() const {
    return simulation_->snapshot().water_lift_valve_station_active;
}
bool ScraperXSimulation::is_water_lift_release_station_active() const {
    return simulation_->snapshot().water_lift_release_station_active;
}
bool ScraperXSimulation::is_water_lift_reset_station_active() const {
    return simulation_->snapshot().water_lift_reset_station_active;
}
bool ScraperXSimulation::is_water_lift_valve_open() const {
    return simulation_->snapshot().water_lift_valve_open;
}
double ScraperXSimulation::get_water_lift_valve_flow_m3_s() const {
    return simulation_->snapshot().water_lift_valve_flow_m3_s;
}
double ScraperXSimulation::get_water_lift_bucket_water_m3() const {
    return simulation_->snapshot().water_lift_bucket_water_m3;
}
double ScraperXSimulation::get_water_lift_bucket_mass_kg() const {
    return simulation_->snapshot().water_lift_bucket_mass_kg;
}
double ScraperXSimulation::get_water_lift_bucket_travel_m() const {
    return simulation_->snapshot().water_lift_bucket_travel_m;
}
double ScraperXSimulation::get_water_lift_cage_travel_m() const {
    return simulation_->snapshot().water_lift_cage_travel_m;
}
double ScraperXSimulation::get_water_lift_cage_peak_speed_mps() const {
    return simulation_->snapshot().water_lift_cage_peak_speed_mps;
}
double ScraperXSimulation::get_water_lift_rope_tension_n() const {
    return simulation_->snapshot().water_lift_rope_tension_n;
}
bool ScraperXSimulation::is_water_lift_bucket_catch_latched() const {
    return simulation_->snapshot().water_lift_bucket_catch_latched;
}
bool ScraperXSimulation::is_water_lift_upper_catch_latched() const {
    return simulation_->snapshot().water_lift_upper_catch_latched;
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

godot::PackedVector3Array ScraperXSimulation::get_cargo_net_vertices() const {
    godot::PackedVector3Array out;
    for(const auto &v:simulation_->cargo_net_vertices(simulation_->snapshot().interpolation_alpha)) out.push_back(godot::Vector3(v.x,v.y,v.z));
    return out;
}
godot::PackedInt32Array ScraperXSimulation::get_cargo_net_indices() const {
    godot::PackedInt32Array out;
    for(const auto i:simulation_->cargo_net_indices()) out.push_back(static_cast<std::int32_t>(i));
    return out;
}

godot::PackedVector3Array ScraperXSimulation::get_catch_net_vertices() const {
    godot::PackedVector3Array out;
    for (const auto &v : simulation_->catch_net_vertices(simulation_->snapshot().interpolation_alpha))
        out.push_back(godot::Vector3(v.x, v.y, v.z));
    return out;
}
godot::PackedVector3Array ScraperXSimulation::get_catch_net_tether_vertices() const {
    godot::PackedVector3Array out;
    for(const auto &v:simulation_->render_catch_net_tether_vertices()) out.push_back(to_godot(v));
    return out;
}
godot::PackedVector3Array ScraperXSimulation::get_catch_net_patch_vertices() const {
    godot::PackedVector3Array out;
    for(const auto &v:simulation_->render_catch_net_patch_vertices()) out.push_back(to_godot(v));
    return out;
}
godot::PackedInt32Array ScraperXSimulation::get_catch_net_indices() const {
    godot::PackedInt32Array out;
    for (const auto i : simulation_->catch_net_indices()) out.push_back(static_cast<std::int32_t>(i));
    return out;
}
godot::Dictionary ScraperXSimulation::get_catch_net_state() const {
    const auto state = simulation_->catch_net_state();
    godot::Dictionary out;
    out["installed"] = state.installed;
    out["current_contact"] = state.current_contact;
    out["columns"] = static_cast<std::int64_t>(state.columns);
    out["rows"] = static_cast<std::int64_t>(state.rows);
    out["contact_serial"] = static_cast<std::int64_t>(state.contact_serial);
    out["minimum_y_m"] = state.minimum_y_m;
    out["sag_m"] = state.sag_m;
    out["total_specific_load_mps2"] = state.total_specific_load_mps2;
    out["peak_total_specific_load_mps2"] = state.peak_total_specific_load_mps2;
    out["excess_load_dose_g_seconds"] = state.excess_load_dose_g_seconds;
    out["approach_speed_mps"] = state.approach_speed_mps;
    out["overloaded"] = state.overloaded;
    return out;
}

std::int64_t ScraperXSimulation::get_kit_body_count() const {
    return static_cast<std::int64_t>(simulation_->kit_body_count());
}

std::int64_t ScraperXSimulation::get_kit_body_entity_id(const std::int64_t body) const {
    return static_cast<std::int64_t>(simulation_->kit_body_entity(kit_index(body)));
}

std::int64_t ScraperXSimulation::get_kit_body_material_key(const std::int64_t body) const {
    return static_cast<std::int64_t>(simulation_->kit_body_material_key(kit_index(body)));
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
              source.rotation.w, static_cast<double>(source.material),
              static_cast<double>(source.shape), source.inner_radius}) {
            out.push_back(static_cast<float>(value));
        }
    }
    return out;
}

godot::PackedFloat32Array ScraperXSimulation::get_kit_body_part_mesh(
    const std::int64_t body, const std::int64_t part) const {
    godot::PackedFloat32Array out;
    if (body < 0 || part < 0 || body > UINT32_MAX || part > UINT32_MAX) return out;
    const auto vertices = simulation_->kit_body_part_mesh(static_cast<std::uint32_t>(body),
                                                         static_cast<std::uint32_t>(part));
    out.resize(static_cast<std::int64_t>(vertices.size() * 3));
    auto *data = out.ptrw();
    for (const auto &vertex : vertices) {
        *data++ = static_cast<float>(vertex.x);
        *data++ = static_cast<float>(vertex.y);
        *data++ = static_cast<float>(vertex.z);
    }
    return out;
}

godot::Vector3 ScraperXSimulation::get_kit_body_linear_velocity(const std::int64_t body) const {
    return to_godot(simulation_->kit_body_velocity(kit_index(body)));
}

godot::Transform3D ScraperXSimulation::get_kit_body_transform(const std::int64_t body) const {
    const std::uint32_t index = kit_index(body);
    return {godot::Basis(to_godot(simulation_->kit_body_rotation(index))),
            to_godot(simulation_->kit_body_position(index))};
}

godot::Transform3D ScraperXSimulation::get_kit_body_render_transform(const std::int64_t body) const {
    const std::uint32_t index = kit_index(body);
    return {godot::Basis(to_godot(simulation_->render_kit_body_rotation(index))),
            to_godot(simulation_->render_kit_body_position(index))};
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

godot::PackedVector3Array ScraperXSimulation::get_kit_cable_render_points(const std::int64_t cable) const {
    constexpr std::uint32_t kCapacity = 64;
    sim::Vector3 points[kCapacity];
    const std::uint32_t count = simulation_->render_kit_cable_points(kit_index(cable), points, kCapacity);
    godot::PackedVector3Array out;
    for (std::uint32_t index = 0; index < count; ++index) out.push_back(to_godot(points[index]));
    return out;
}

bool ScraperXSimulation::is_well_a_catch_latched() const {
    return simulation_->snapshot().well_a_catch_latched;
}

std::int64_t ScraperXSimulation::get_well_a_rope_end_entity_id() const {
    return static_cast<std::int64_t>(simulation_->snapshot().well_a_rope_end_entity_id);
}

} // namespace scraperx::bridge
