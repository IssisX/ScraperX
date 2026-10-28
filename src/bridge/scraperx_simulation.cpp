#include "bridge/scraperx_simulation.hpp"

#include "bridge/fabricated_delivery.hpp"
#include "sim/corpse.hpp"
#include "sim/fabricated.hpp"
#include "sim/matron.hpp"

#include <cstring>
#include <limits>
#include <string>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
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

// One past the last spawn, the enum's own sentinel: a literal (21, behind
// IntakeHandoffDeck) and then the last enumerator (Deck4South, behind the 66,
// 88 and 132 m decks) each fell behind the spawns added after them.
constexpr std::int64_t kInitialSpawnCount = static_cast<std::int64_t>(sim::InitialSpawn::Count);

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
    godot::ClassDB::bind_method(godot::D_METHOD("set_look_pitch", "radians"),
                                &ScraperXSimulation::set_look_pitch);
    godot::ClassDB::bind_method(godot::D_METHOD("is_precision_sighted"),
                                &ScraperXSimulation::is_precision_sighted);
    godot::ClassDB::bind_method(godot::D_METHOD("get_precision_point"),
                                &ScraperXSimulation::get_precision_point);
    godot::ClassDB::bind_method(godot::D_METHOD("get_precision_jump_count"),
                                &ScraperXSimulation::get_precision_jump_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_precision_landing_count"),
                                &ScraperXSimulation::get_precision_landing_count);
    godot::ClassDB::bind_method(godot::D_METHOD("request_jump"),
                                &ScraperXSimulation::request_jump);
    godot::ClassDB::bind_method(godot::D_METHOD("request_traversal"),
                                &ScraperXSimulation::request_traversal);
    godot::ClassDB::bind_method(godot::D_METHOD("request_up"), &ScraperXSimulation::request_up);
    godot::ClassDB::bind_method(godot::D_METHOD("get_up_move"), &ScraperXSimulation::get_up_move);
    godot::ClassDB::bind_method(godot::D_METHOD("get_up_resolved"), &ScraperXSimulation::get_up_resolved);
    godot::ClassDB::bind_method(godot::D_METHOD("get_up_count"), &ScraperXSimulation::get_up_count);
    godot::ClassDB::bind_method(godot::D_METHOD("request_release"),
                                &ScraperXSimulation::request_release);
    godot::ClassDB::bind_method(godot::D_METHOD("request_parachute"),
                                &ScraperXSimulation::request_parachute);
    godot::ClassDB::bind_method(godot::D_METHOD("set_crouch_input", "held"),
                                &ScraperXSimulation::set_crouch_input);
    godot::ClassDB::bind_method(godot::D_METHOD("set_room_loudness", "loudness"),
                                &ScraperXSimulation::set_room_loudness);
    godot::ClassDB::bind_method(godot::D_METHOD("set_sprint_input", "held"),
                                &ScraperXSimulation::set_sprint_input);
    godot::ClassDB::bind_method(godot::D_METHOD("get_traversal_left_hand"),
                                &ScraperXSimulation::get_traversal_left_hand);
    godot::ClassDB::bind_method(godot::D_METHOD("get_traversal_right_hand"),
                                &ScraperXSimulation::get_traversal_right_hand);
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
    godot::ClassDB::bind_method(godot::D_METHOD("get_player_render_position"),
                                &ScraperXSimulation::get_player_render_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_player_linear_velocity"),
                                &ScraperXSimulation::get_player_linear_velocity);
    godot::ClassDB::bind_method(godot::D_METHOD("is_player_grounded"),
                                &ScraperXSimulation::is_player_grounded);
    godot::ClassDB::bind_method(godot::D_METHOD("is_player_wall_running"),
                                &ScraperXSimulation::is_player_wall_running);
    godot::ClassDB::bind_method(godot::D_METHOD("get_player_wall_normal"),
                                &ScraperXSimulation::get_player_wall_normal);
    godot::ClassDB::bind_method(godot::D_METHOD("is_player_wall_climbing"),
                                &ScraperXSimulation::is_player_wall_climbing);
    godot::ClassDB::bind_method(godot::D_METHOD("get_wall_climb_count"), &ScraperXSimulation::get_wall_climb_count);
    godot::ClassDB::bind_method(godot::D_METHOD("is_player_tucked"), &ScraperXSimulation::is_player_tucked);
    godot::ClassDB::bind_method(godot::D_METHOD("is_player_springboarding"),
                                &ScraperXSimulation::is_player_springboarding);
    godot::ClassDB::bind_method(godot::D_METHOD("get_springboard_count"), &ScraperXSimulation::get_springboard_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_swing_launch_count"), &ScraperXSimulation::get_swing_launch_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_wall_step_count"),
                                &ScraperXSimulation::get_wall_step_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_wall_kick_count"),
                                &ScraperXSimulation::get_wall_kick_count);
    godot::ClassDB::bind_method(godot::D_METHOD("is_player_sliding"),
                                &ScraperXSimulation::is_player_sliding);
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
    godot::ClassDB::bind_method(godot::D_METHOD("get_step_up_meters"),
                                &ScraperXSimulation::get_step_up_meters);






    godot::ClassDB::bind_method(godot::D_METHOD("request_pick_up"),
                                &ScraperXSimulation::request_pick_up);
    godot::ClassDB::bind_method(godot::D_METHOD("request_set_down"),
                                &ScraperXSimulation::request_set_down);
    godot::ClassDB::bind_method(godot::D_METHOD("get_carrying_entity_id"),
                                &ScraperXSimulation::get_carrying_entity_id);
    godot::ClassDB::bind_method(godot::D_METHOD("get_carry_target_entity_id"),
                                &ScraperXSimulation::get_carry_target_entity_id);

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
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_body_mass", "body"),
                                &ScraperXSimulation::get_kit_body_mass);
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
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_bins"), &ScraperXSimulation::get_kit_bins);
    godot::ClassDB::bind_method(godot::D_METHOD("get_loose_stock"), &ScraperXSimulation::get_loose_stock);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_piles"),
                                &ScraperXSimulation::get_kit_piles);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_pools"),
                                &ScraperXSimulation::get_kit_pools);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_spouts"),
                                &ScraperXSimulation::get_kit_spouts);
    godot::ClassDB::bind_method(godot::D_METHOD("get_kit_vents"),
                                &ScraperXSimulation::get_kit_vents);
    godot::ClassDB::bind_method(godot::D_METHOD("get_well_a_cage_travel"),
                                &ScraperXSimulation::get_well_a_cage_travel);
    godot::ClassDB::bind_method(godot::D_METHOD("is_well_a_catch_latched"),
                                &ScraperXSimulation::is_well_a_catch_latched);
    godot::ClassDB::bind_method(godot::D_METHOD("get_well_a_rope_end_entity_id"),
                                &ScraperXSimulation::get_well_a_rope_end_entity_id);
    godot::ClassDB::bind_method(godot::D_METHOD("get_well_b_cage_travel"),
                                &ScraperXSimulation::get_well_b_cage_travel);
    godot::ClassDB::bind_method(godot::D_METHOD("is_well_b_catch_latched"),
                                &ScraperXSimulation::is_well_b_catch_latched);
    godot::ClassDB::bind_method(godot::D_METHOD("get_well_b_rope_end_entity_id"),
                                &ScraperXSimulation::get_well_b_rope_end_entity_id);
    godot::ClassDB::bind_method(godot::D_METHOD("get_well_b_boom_angle"),
                                &ScraperXSimulation::get_well_b_boom_angle);
    godot::ClassDB::bind_method(godot::D_METHOD("get_well_c_platform_travel"),
                                &ScraperXSimulation::get_well_c_platform_travel);
    godot::ClassDB::bind_method(godot::D_METHOD("is_well_c_catch_latched"),
                                &ScraperXSimulation::is_well_c_catch_latched);
    godot::ClassDB::bind_method(godot::D_METHOD("get_well_c_rebar_angle"),
                                &ScraperXSimulation::get_well_c_rebar_angle);
    godot::ClassDB::bind_method(godot::D_METHOD("get_well_c_dumpster_kg"),
                                &ScraperXSimulation::get_well_c_dumpster_kg);
    godot::ClassDB::bind_method(godot::D_METHOD("get_stack_s1_cage_travel"),
                                &ScraperXSimulation::get_stack_s1_cage_travel);
    godot::ClassDB::bind_method(godot::D_METHOD("get_stack_s1_bucket_water_kg"),
                                &ScraperXSimulation::get_stack_s1_bucket_water_kg);
    godot::ClassDB::bind_method(godot::D_METHOD("get_stack_s1_valve_angle"),
                                &ScraperXSimulation::get_stack_s1_valve_angle);
    godot::ClassDB::bind_method(godot::D_METHOD("is_stack_s1_catch_latched"),
                                &ScraperXSimulation::is_stack_s1_catch_latched);
    godot::ClassDB::bind_method(godot::D_METHOD("is_stack_s1_valve_pawled"),
                                &ScraperXSimulation::is_stack_s1_valve_pawled);
    godot::ClassDB::bind_method(godot::D_METHOD("get_stack_s2_cage_travel"),
                                &ScraperXSimulation::get_stack_s2_cage_travel);
    godot::ClassDB::bind_method(godot::D_METHOD("get_stack_s2_beam_angle"),
                                &ScraperXSimulation::get_stack_s2_beam_angle);
    godot::ClassDB::bind_method(godot::D_METHOD("is_stack_s2_bolt_shot"),
                                &ScraperXSimulation::is_stack_s2_bolt_shot);
    godot::ClassDB::bind_method(godot::D_METHOD("get_stack_s3_cage_travel"),
                                &ScraperXSimulation::get_stack_s3_cage_travel);
    godot::ClassDB::bind_method(godot::D_METHOD("get_stack_s3_brake_angle"),
                                &ScraperXSimulation::get_stack_s3_brake_angle);
    godot::ClassDB::bind_method(godot::D_METHOD("is_stack_s3_brake_latched"),
                                &ScraperXSimulation::is_stack_s3_brake_latched);
    godot::ClassDB::bind_method(godot::D_METHOD("get_landing_state"), &ScraperXSimulation::get_landing_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_water_state"), &ScraperXSimulation::get_water_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_hand_state"), &ScraperXSimulation::get_hand_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_wind_mps"), &ScraperXSimulation::get_wind_mps);
    godot::ClassDB::bind_method(godot::D_METHOD("get_wet_state"), &ScraperXSimulation::get_wet_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_shop_state"), &ScraperXSimulation::get_shop_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_crane_state"), &ScraperXSimulation::get_crane_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_service_state"), &ScraperXSimulation::get_service_state);
    godot::ClassDB::bind_method(godot::D_METHOD("set_slingshot_input", "draw", "yaw", "elevation"),
                                &ScraperXSimulation::set_slingshot_input);
    godot::ClassDB::bind_method(godot::D_METHOD("request_slingshot_action"),
                                &ScraperXSimulation::request_slingshot_action);
    godot::ClassDB::bind_method(godot::D_METHOD("request_slingshot_drop"),
                                &ScraperXSimulation::request_slingshot_drop);
    godot::ClassDB::bind_method(godot::D_METHOD("get_slingshot_state"), &ScraperXSimulation::get_slingshot_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_throw_prediction"), &ScraperXSimulation::get_throw_prediction);
    godot::ClassDB::bind_method(godot::D_METHOD("get_throw_count"), &ScraperXSimulation::get_throw_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_slingshot_prediction"),
                                &ScraperXSimulation::get_slingshot_prediction);
    godot::ClassDB::bind_method(godot::D_METHOD("get_route_ahead"), &ScraperXSimulation::get_route_ahead);
    godot::ClassDB::bind_method(godot::D_METHOD("request_swing_action"), &ScraperXSimulation::request_swing_action);
    godot::ClassDB::bind_method(godot::D_METHOD("request_swing_drop"), &ScraperXSimulation::request_swing_drop);
    godot::ClassDB::bind_method(godot::D_METHOD("get_swing_state"), &ScraperXSimulation::get_swing_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_summit_state"), &ScraperXSimulation::get_summit_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_matron_state"), &ScraperXSimulation::get_matron_state);
    godot::ClassDB::bind_method(godot::D_METHOD("place_matron_home", "deck_top"), &ScraperXSimulation::place_matron_home);
    godot::ClassDB::bind_method(godot::D_METHOD("get_matron_hold_state"), &ScraperXSimulation::get_matron_hold_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_bay_state"), &ScraperXSimulation::get_bay_state);
    godot::ClassDB::bind_method(godot::D_METHOD("light_summit"), &ScraperXSimulation::light_summit);
    godot::ClassDB::bind_method(godot::D_METHOD("prepare_matron"), &ScraperXSimulation::prepare_matron);
    godot::ClassDB::bind_method(godot::D_METHOD("request_trapeze_action"), &ScraperXSimulation::request_trapeze_action);
    godot::ClassDB::bind_method(godot::D_METHOD("request_trapeze_drop"), &ScraperXSimulation::request_trapeze_drop);
    godot::ClassDB::bind_method(godot::D_METHOD("get_trapeze_state"), &ScraperXSimulation::get_trapeze_state);
    godot::ClassDB::bind_method(godot::D_METHOD("request_forklift_action"),
                                &ScraperXSimulation::request_forklift_action);
    godot::ClassDB::bind_method(godot::D_METHOD("set_forklift_lift", "lift"), &ScraperXSimulation::set_forklift_lift);
    godot::ClassDB::bind_method(godot::D_METHOD("get_forklift_state"), &ScraperXSimulation::get_forklift_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_forklift_count"), &ScraperXSimulation::get_forklift_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_forklift_state_at", "index"),
                                &ScraperXSimulation::get_forklift_state_at);
    godot::ClassDB::bind_method(godot::D_METHOD("request_yard_crane_take"), &ScraperXSimulation::request_yard_crane_take);
    godot::ClassDB::bind_method(godot::D_METHOD("request_yard_crane_operate"),
                                &ScraperXSimulation::request_yard_crane_operate);
    godot::ClassDB::bind_method(godot::D_METHOD("request_yard_crane_device"),
                                &ScraperXSimulation::request_yard_crane_device);
    godot::ClassDB::bind_method(godot::D_METHOD("set_yard_crane_hoist", "hoist"),
                                &ScraperXSimulation::set_yard_crane_hoist);
    godot::ClassDB::bind_method(godot::D_METHOD("get_yard_crane_state"), &ScraperXSimulation::get_yard_crane_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_impacts"), &ScraperXSimulation::get_impacts);
    godot::ClassDB::bind_method(godot::D_METHOD("get_last_impact_sequence"),
                                &ScraperXSimulation::get_last_impact_sequence);
    godot::ClassDB::bind_method(godot::D_METHOD("get_zipline_lines"), &ScraperXSimulation::get_zipline_lines);
    godot::ClassDB::bind_method(godot::D_METHOD("get_zipline_trolleys"), &ScraperXSimulation::get_zipline_trolleys);
    godot::ClassDB::bind_method(godot::D_METHOD("get_zipline_state"), &ScraperXSimulation::get_zipline_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_corpses"), &ScraperXSimulation::get_corpses);
    godot::ClassDB::bind_method(godot::D_METHOD("get_corpse_plan"), &ScraperXSimulation::get_corpse_plan);
    godot::ClassDB::bind_method(godot::D_METHOD("get_death_state"), &ScraperXSimulation::get_death_state);
    godot::ClassDB::bind_method(godot::D_METHOD("get_hook_state"), &ScraperXSimulation::get_hook_state);
    godot::ClassDB::bind_method(godot::D_METHOD("set_hook_aim", "direction"), &ScraperXSimulation::set_hook_aim);
    godot::ClassDB::bind_method(godot::D_METHOD("get_workers"), &ScraperXSimulation::get_workers);
    godot::ClassDB::bind_method(godot::D_METHOD("set_lift_haul", "held"), &ScraperXSimulation::set_lift_haul);
    godot::ClassDB::bind_method(godot::D_METHOD("get_worker_counts"), &ScraperXSimulation::get_worker_counts);
    godot::ClassDB::bind_method(godot::D_METHOD("get_surface_below", "from", "reach"),
                                &ScraperXSimulation::get_surface_below);
    godot::ClassDB::bind_method(godot::D_METHOD("get_look_target", "from", "direction", "reach"),
                                &ScraperXSimulation::get_look_target);
    godot::ClassDB::bind_method(godot::D_METHOD("request_hook_fire"), &ScraperXSimulation::request_hook_fire);
    godot::ClassDB::bind_method(godot::D_METHOD("set_hook_reel", "held"), &ScraperXSimulation::set_hook_reel);
    godot::ClassDB::bind_method(godot::D_METHOD("request_hook_release"), &ScraperXSimulation::request_hook_release);
    godot::ClassDB::bind_method(godot::D_METHOD("request_hook_take"), &ScraperXSimulation::request_hook_take);
    godot::ClassDB::bind_method(godot::D_METHOD("request_lift_action"), &ScraperXSimulation::request_lift_action);
    godot::ClassDB::bind_method(godot::D_METHOD("get_save_game"), &ScraperXSimulation::get_save_game);
    godot::ClassDB::bind_method(godot::D_METHOD("load_save_game", "bytes"), &ScraperXSimulation::load_save_game);
    godot::ClassDB::bind_method(godot::D_METHOD("add_fabricated", "hull", "signature", "model", "at", "yaw"),
                                &ScraperXSimulation::add_fabricated);
    godot::ClassDB::bind_method(godot::D_METHOD("get_fabricated_refusal"), &ScraperXSimulation::get_fabricated_refusal);
    godot::ClassDB::bind_method(godot::D_METHOD("check_fabricated_delivery", "hull", "signature", "model"),
                                &ScraperXSimulation::check_fabricated_delivery);
    godot::ClassDB::bind_method(godot::D_METHOD("get_fabricated_count"), &ScraperXSimulation::get_fabricated_count);
    godot::ClassDB::bind_method(godot::D_METHOD("get_fabricated_object", "index"),
                                &ScraperXSimulation::get_fabricated_object);
    godot::ClassDB::bind_method(godot::D_METHOD("get_fabricated_hull_triangles", "index"),
                                &ScraperXSimulation::get_fabricated_hull_triangles);
    godot::ClassDB::bind_method(godot::D_METHOD("get_lift_state"), &ScraperXSimulation::get_lift_state);
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

bool ScraperXSimulation::place_matron_home(const double deck_top) {
    return simulation_->place_matron_home(static_cast<float>(deck_top));
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

bool ScraperXSimulation::set_look_pitch(const double radians) {
    return simulation_->set_look_pitch(radians);
}

bool ScraperXSimulation::is_precision_sighted() const {
    return simulation_->snapshot().precision_sighted;
}

godot::Vector3 ScraperXSimulation::get_precision_point() const {
    const auto &p = simulation_->snapshot().precision_point;
    return {static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z)};
}

std::int64_t ScraperXSimulation::get_precision_jump_count() const {
    return static_cast<std::int64_t>(simulation_->snapshot().precision_jump_count);
}

std::int64_t ScraperXSimulation::get_precision_landing_count() const {
    return static_cast<std::int64_t>(simulation_->snapshot().precision_landing_count);
}

bool ScraperXSimulation::request_jump() {
    return simulation_->request_jump();
}

bool ScraperXSimulation::request_traversal() {
    return simulation_->request_traversal();
}

bool ScraperXSimulation::request_up() {
    return simulation_->request_up();
}

std::int64_t ScraperXSimulation::get_up_move() const {
    return static_cast<std::int64_t>(simulation_->snapshot().up_move);
}

std::int64_t ScraperXSimulation::get_up_resolved() const {
    return static_cast<std::int64_t>(simulation_->snapshot().up_resolved);
}

std::int64_t ScraperXSimulation::get_up_count() const {
    return static_cast<std::int64_t>(simulation_->snapshot().up_count);
}

bool ScraperXSimulation::request_release() {
    return simulation_->request_release();
}

bool ScraperXSimulation::request_parachute() {
    return simulation_->request_parachute();
}

bool ScraperXSimulation::set_room_loudness(const double loudness) {
    return simulation_->set_room_loudness(loudness);
}

bool ScraperXSimulation::set_crouch_input(const bool held) {
    return simulation_->set_crouch_input(held);
}

bool ScraperXSimulation::set_sprint_input(const bool held) {
    return simulation_->set_sprint_input(held);
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

godot::Vector3 ScraperXSimulation::get_player_render_position() const {
    return to_godot(simulation_->render_player_position());
}

godot::Vector3 ScraperXSimulation::get_player_linear_velocity() const {
    return to_godot(simulation_->snapshot().player_linear_velocity);
}

bool ScraperXSimulation::is_player_grounded() const {
    return simulation_->snapshot().player_grounded;
}

bool ScraperXSimulation::is_player_wall_running() const {
    return simulation_->snapshot().player_wall_running;
}

godot::Vector3 ScraperXSimulation::get_player_wall_normal() const {
    return to_godot(simulation_->snapshot().player_wall_normal);
}

bool ScraperXSimulation::is_player_wall_climbing() const {
    return simulation_->snapshot().player_wall_climbing;
}

std::int64_t ScraperXSimulation::get_wall_climb_count() const {
    return static_cast<std::int64_t>(simulation_->snapshot().wall_climb_count);
}

bool ScraperXSimulation::is_player_tucked() const {
    return simulation_->snapshot().player_tucked;
}

bool ScraperXSimulation::is_player_springboarding() const {
    return simulation_->snapshot().player_springboarding;
}

std::int64_t ScraperXSimulation::get_springboard_count() const {
    return static_cast<std::int64_t>(simulation_->snapshot().springboard_count);
}

std::int64_t ScraperXSimulation::get_swing_launch_count() const {
    return static_cast<std::int64_t>(simulation_->snapshot().swing_launch_count);
}

std::int64_t ScraperXSimulation::get_wall_step_count() const {
    return static_cast<std::int64_t>(simulation_->snapshot().wall_step_count);
}

std::int64_t ScraperXSimulation::get_wall_kick_count() const {
    return static_cast<std::int64_t>(simulation_->snapshot().wall_kick_count);
}

bool ScraperXSimulation::is_player_sliding() const {
    return simulation_->snapshot().player_sliding;
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

double ScraperXSimulation::get_step_up_meters() const {
    return simulation_->snapshot().step_up_meters;
}

double ScraperXSimulation::get_tower_height_meters() const {
    return sim::Simulation::kTowerHeightMeters;
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

double ScraperXSimulation::get_kit_body_mass(const std::int64_t body) const {
    return simulation_->kit_body_mass(kit_index(body));
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
              source.rotation.w, static_cast<double>(source.material), static_cast<double>(source.shape)}) {
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

godot::PackedFloat32Array ScraperXSimulation::get_kit_bins() const {
    godot::PackedFloat32Array out;
    for (std::uint32_t index = 0; index < simulation_->kit_bin_count(); ++index) {
        const sim::KitBin bin = simulation_->kit_bin(index);
        out.push_back(static_cast<float>(bin.body));
        out.push_back(static_cast<float>(bin.contents_kg));
        out.push_back(static_cast<float>(bin.capacity_kg));
        out.push_back(bin.flowing ? 1.0F : 0.0F);
        out.push_back(static_cast<float>(bin.stream_from.x));
        out.push_back(static_cast<float>(bin.stream_from.y));
        out.push_back(static_cast<float>(bin.stream_from.z));
        out.push_back(static_cast<float>(bin.stream_to.x));
        out.push_back(static_cast<float>(bin.stream_to.y));
        out.push_back(static_cast<float>(bin.stream_to.z));
        out.push_back(bin.water ? 1.0F : 0.0F);
    }
    return out;
}

// Per piece of loose stock: its kit body index, its kind and its paint.
godot::PackedInt32Array ScraperXSimulation::get_loose_stock() const {
    godot::PackedInt32Array out;
    for (std::uint32_t index = 0; index < simulation_->loose_stock_count(); ++index) {
        const sim::Simulation::LooseStockPiece piece = simulation_->loose_stock_piece(index);
        out.push_back(static_cast<std::int32_t>(
            simulation_->kit_body_index(sim::Simulation::kLooseStockFirstEntity + index)));
        out.push_back(piece.kind);
        out.push_back(piece.variant);
    }
    return out;
}

godot::PackedFloat32Array ScraperXSimulation::get_kit_pools() const {
    godot::PackedFloat32Array out;
    for (std::uint32_t index = 0; index < simulation_->kit_pool_count(); ++index) {
        const sim::KitPool pool = simulation_->kit_pool(index);
        out.push_back(static_cast<float>(pool.min_corner.x));
        out.push_back(static_cast<float>(pool.min_corner.y));
        out.push_back(static_cast<float>(pool.min_corner.z));
        out.push_back(static_cast<float>(pool.max_corner.x));
        out.push_back(static_cast<float>(pool.max_corner.y));
        out.push_back(static_cast<float>(pool.max_corner.z));
        out.push_back(static_cast<float>(pool.level_m));
    }
    return out;
}

godot::PackedFloat32Array ScraperXSimulation::get_kit_spouts() const {
    godot::PackedFloat32Array out;
    for (std::uint32_t index = 0; index < simulation_->kit_spout_count(); ++index) {
        const sim::KitSpout spout = simulation_->kit_spout(index);
        out.push_back(spout.pouring ? 1.0F : 0.0F);
        out.push_back(static_cast<float>(spout.from.x));
        out.push_back(static_cast<float>(spout.from.y));
        out.push_back(static_cast<float>(spout.from.z));
        out.push_back(static_cast<float>(spout.to.x));
        out.push_back(static_cast<float>(spout.to.y));
        out.push_back(static_cast<float>(spout.to.z));
    }
    return out;
}

// Each vent collar: x0, z0, x1, z1, from_y, to_y and the air it let out last
// step (m^3/s at atmospheric pressure).
godot::PackedFloat32Array ScraperXSimulation::get_kit_vents() const {
    godot::PackedFloat32Array out;
    for (std::uint32_t index = 0; index < simulation_->kit_vent_count(); ++index) {
        const sim::KitVent vent = simulation_->kit_vent(index);
        out.push_back(static_cast<float>(vent.low.x));
        out.push_back(static_cast<float>(vent.low.z));
        out.push_back(static_cast<float>(vent.high.x));
        out.push_back(static_cast<float>(vent.high.z));
        out.push_back(static_cast<float>(vent.low.y));
        out.push_back(static_cast<float>(vent.high.y));
        out.push_back(static_cast<float>(vent.flow_m3s));
    }
    return out;
}

godot::PackedFloat32Array ScraperXSimulation::get_kit_piles() const {
    godot::PackedFloat32Array out;
    for (std::uint32_t index = 0; index < simulation_->kit_pile_count(); ++index) {
        const sim::KitPile pile = simulation_->kit_pile(index);
        out.push_back(static_cast<float>(pile.at.x));
        out.push_back(static_cast<float>(pile.at.y));
        out.push_back(static_cast<float>(pile.at.z));
        out.push_back(static_cast<float>(pile.kg));
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

double ScraperXSimulation::get_well_b_cage_travel() const {
    return simulation_->snapshot().well_b_cage_travel;
}

bool ScraperXSimulation::is_well_b_catch_latched() const {
    return simulation_->snapshot().well_b_catch_latched;
}

std::int64_t ScraperXSimulation::get_well_b_rope_end_entity_id() const {
    return static_cast<std::int64_t>(simulation_->snapshot().well_b_rope_end_entity_id);
}

double ScraperXSimulation::get_well_b_boom_angle() const {
    return simulation_->snapshot().well_b_boom_angle;
}

double ScraperXSimulation::get_well_c_platform_travel() const {
    return simulation_->snapshot().well_c_platform_travel;
}

bool ScraperXSimulation::is_well_c_catch_latched() const {
    return simulation_->snapshot().well_c_catch_latched;
}

double ScraperXSimulation::get_well_c_rebar_angle() const {
    return simulation_->snapshot().well_c_rebar_angle;
}

double ScraperXSimulation::get_well_c_dumpster_kg() const {
    return simulation_->snapshot().well_c_dumpster_kg;
}

double ScraperXSimulation::get_stack_s1_cage_travel() const {
    return simulation_->stack_state().s1_cage_travel;
}

double ScraperXSimulation::get_stack_s1_bucket_water_kg() const {
    return simulation_->stack_state().s1_bucket_water_kg;
}

double ScraperXSimulation::get_stack_s1_valve_angle() const {
    return simulation_->stack_state().s1_valve_angle;
}

bool ScraperXSimulation::is_stack_s1_catch_latched() const {
    return simulation_->stack_state().s1_catch_latched;
}

bool ScraperXSimulation::is_stack_s1_valve_pawled() const {
    return simulation_->stack_state().s1_valve_pawled;
}

double ScraperXSimulation::get_stack_s2_cage_travel() const {
    return simulation_->stack_state().s2_cage_travel;
}

double ScraperXSimulation::get_stack_s2_beam_angle() const {
    return simulation_->stack_state().s2_beam_angle;
}

bool ScraperXSimulation::is_stack_s2_bolt_shot() const {
    return simulation_->stack_state().s2_bolt_shot;
}

double ScraperXSimulation::get_stack_s3_cage_travel() const {
    return simulation_->stack_state().s3_cage_travel;
}

double ScraperXSimulation::get_stack_s3_brake_angle() const {
    return simulation_->stack_state().s3_brake_angle;
}

bool ScraperXSimulation::is_stack_s3_brake_latched() const {
    return simulation_->stack_state().s3_brake_latched;
}

godot::Dictionary ScraperXSimulation::get_water_state() const {
    const sim::Snapshot &s = simulation_->snapshot();
    godot::Dictionary out;
    out["in_water"] = s.in_water;
    out["swimming"] = s.swimming;
    out["head_under"] = s.head_underwater;
    out["submersion"] = s.water_submersion;
    out["surface_y"] = s.water_surface_y;
    out["pool"] = static_cast<std::int64_t>(s.water_pool);
    out["entries"] = static_cast<std::int64_t>(s.water_entry_count);
    out["exits"] = static_cast<std::int64_t>(s.water_exit_count);
    out["entry_speed"] = s.water_entry_speed_mps;
    out["kicks"] = static_cast<std::int64_t>(s.swim_kick_count);
    out["position"] = to_godot(s.player_position);
    return out;
}

godot::Dictionary ScraperXSimulation::get_landing_state() const {
    const sim::Snapshot &s = simulation_->snapshot();
    godot::Dictionary out;
    out["response"] = static_cast<std::int64_t>(s.landing_response);
    out["normal_speed"] = s.landing_normal_speed_mps;
    out["tangent_speed"] = s.landing_tangent_speed_mps;
    out["loss"] = s.landing_balance_loss;
    out["balance"] = s.recovery_balance;
    out["roll_progress"] = s.roll_progress;
    out["roll_travel"] = s.roll_travel_m;
    out["count"] = static_cast<std::int64_t>(s.landing_count);
    out["rolls"] = static_cast<std::int64_t>(s.roll_count);
    out["rolls_refused"] = static_cast<std::int64_t>(s.roll_refused_count);
    out["stumbles"] = static_cast<std::int64_t>(s.stumble_count);
    out["limping"] = s.limping;
    out["limp_left"] = s.limp_seconds_left;
    out["ankle_breaks"] = static_cast<std::int64_t>(s.ankle_break_count);
    out["arm_broken"] = s.arm_broken;
    out["arm_left"] = s.arm_broken_seconds_left;
    out["arm_breaks"] = static_cast<std::int64_t>(s.arm_break_count);
    out["arm_side"] = static_cast<std::int64_t>(s.broken_arm_side);
    out["teetering"] = s.player_teetering;
    out["beam_xcom"] = s.beam_xcom;
    out["beam_lean"] = to_godot(s.beam_lean);
    out["teeters"] = static_cast<std::int64_t>(s.teeter_count);
    out["summit_reached"] = s.summit_reached;
    out["summit_seconds"] = s.summit_seconds;
    out["summit_deaths"] = static_cast<std::int64_t>(s.summit_deaths);
    out["route_top"] = sim::Simulation::kRouteTopMeters;
    return out;
}

// What the hands have done, as counts the presentation turns into feel:
// hangs torn off by a load past the grip, bars and pipes caught overhead,
// and reaches from one to the next.
godot::Dictionary ScraperXSimulation::get_hand_state() const {
    const sim::Snapshot &s = simulation_->snapshot();
    godot::Dictionary out;
    out["hang_slips"] = static_cast<std::int64_t>(s.hang_slip_count);
    out["swing_catches"] = static_cast<std::int64_t>(s.swing_catch_count);
    out["swing_reaches"] = static_cast<std::int64_t>(s.swing_reach_count);
    return out;
}

// The wind off the west face at the body (m/s), 0 outside its zone.
double ScraperXSimulation::get_wind_mps() const {
    return simulation_->snapshot().wind_mps;
}

godot::Dictionary ScraperXSimulation::get_wet_state() const {
    const sim::WetState state = simulation_->wet_state();
    godot::Dictionary out;
    out["d_pipe_whole"] = state.d_pipe_whole;
    out["d_fill_kg_s"] = state.d_fill_kg_s;
    out["d_tank_kg"] = state.d_tank_kg;
    out["d_tube_kg"] = state.d_tube_kg;
    out["d_tank_level"] = state.d_tank_level;
    out["d_tube_level"] = state.d_tube_level;
    out["d_platform_travel"] = state.d_platform_travel;
    out["d_drain_angle"] = state.d_drain_angle;
    out["dump_angle"] = state.dump_angle;
    out["e_door_shut"] = state.e_door_shut;
    out["e_door_angle"] = state.e_door_angle;
    out["e_deck_drop"] = state.e_deck_drop;
    out["e_lever_angle"] = state.e_lever_angle;
    out["e_cab_speed"] = state.e_cab_speed;
    out["e_throttle_m3s"] = state.e_throttle_m3s;
    out["e_vent_m3s"] = state.e_vent_m3s;
    out["e_catch_latched"] = state.e_catch_latched;
    out["e_duct_pa"] = state.e_duct_pa;
    out["e_cab_pa"] = state.e_cab_pa;
    out["e_cab_travel"] = state.e_cab_travel;
    out["e_chiller_travel"] = state.e_chiller_travel;
    out["e_bucket_kg"] = state.e_bucket_kg;
    out["f_catch_latched"] = state.f_catch_latched;
    out["f_hose_coupled"] = state.f_hose_coupled;
    out["f_platform_travel"] = state.f_platform_travel;
    out["f_accumulator_travel"] = state.f_accumulator_travel;
    out["header_kg"] = state.header_kg;
    out["drained_kg"] = state.drained_kg;
    return out;
}

godot::Dictionary ScraperXSimulation::get_shop_state() const {
    const sim::ShopState state = simulation_->shop_state();
    godot::Dictionary out;
    out["g_rope_on_eye"] = state.g_rope_on_eye;
    out["g_tower_latched"] = state.g_tower_latched;
    out["g_platform_travel"] = state.g_platform_travel;
    out["g_tower_travel"] = state.g_tower_travel;
    out["h_girder_latched"] = state.h_girder_latched;
    out["h_trolley_latched"] = state.h_trolley_latched;
    out["h_girder_angle"] = state.h_girder_angle;
    out["h_platform_travel"] = state.h_platform_travel;
    out["i_rope_on_eye"] = state.i_rope_on_eye;
    out["i_domino_latched"] = state.i_domino_latched;
    out["i_monolith_latched"] = state.i_monolith_latched;
    out["i_domino_angle"] = state.i_domino_angle;
    out["i_trip_angle"] = state.i_trip_angle;
    out["i_monolith_angle"] = state.i_monolith_angle;
    out["i_cage_travel"] = state.i_cage_travel;
    return out;
}

godot::Dictionary ScraperXSimulation::get_crane_state() const {
    const sim::CraneState state = simulation_->crane_state();
    godot::Dictionary out;
    out["j_rail_whole"] = state.j_rail_whole;
    out["j_wagon_latched"] = state.j_wagon_latched;
    out["j_traveler_travel"] = state.j_traveler_travel;
    out["j_wagon_travel"] = state.j_wagon_travel;
    out["k_rope_on_eye"] = state.k_rope_on_eye;
    out["k_jib_latched"] = state.k_jib_latched;
    out["k_jib_angle"] = state.k_jib_angle;
    out["k_cage_travel"] = state.k_cage_travel;
    out["l_clutch_in"] = state.l_clutch_in;
    out["l_weight_latched"] = state.l_weight_latched;
    out["l_cart_latched"] = state.l_cart_latched;
    out["l_cart_travel"] = state.l_cart_travel;
    out["l_cab_travel"] = state.l_cab_travel;
    return out;
}

godot::Dictionary ScraperXSimulation::get_service_state() const {
    const sim::ServiceState state = simulation_->service_state();
    godot::Dictionary out;
    out["m_rope_on_eye"] = state.m_rope_on_eye;
    out["m_reel_latched"] = state.m_reel_latched;
    out["m_cage_travel"] = state.m_cage_travel;
    out["m_reel_travel"] = state.m_reel_travel;
    out["m_reel_kg"] = state.m_reel_kg;
    out["m_cable_paid"] = state.m_cable_paid;
    out["m_rope_tension"] = state.m_rope_tension;
    out["n_cage_travel"] = state.n_cage_travel;
    out["n_hopper_travel"] = state.n_hopper_travel;
    out["n_gate_angle"] = state.n_gate_angle;
    out["n_silo_kg"] = state.n_silo_kg;
    out["n_hopper_kg"] = state.n_hopper_kg;
    out["n_chain_kg"] = state.n_chain_kg;
    out["o_ram_latched"] = state.o_ram_latched;
    out["o_gate_latched"] = state.o_gate_latched;
    out["o_cab_travel"] = state.o_cab_travel;
    out["o_bucket_travel"] = state.o_bucket_travel;
    out["o_wheel_angle"] = state.o_wheel_angle;
    out["o_ram_angle"] = state.o_ram_angle;
    out["o_latch_angle"] = state.o_latch_angle;
    out["o_gate_angle"] = state.o_gate_angle;
    out["o_bin_kg"] = state.o_bin_kg;
    out["o_bucket_kg"] = state.o_bucket_kg;
    out["o_chain_kg"] = state.o_chain_kg;
    out["o_rope_tension"] = state.o_rope_tension;
    return out;
}

bool ScraperXSimulation::set_slingshot_input(const double draw, const double yaw, const double elevation) {
    return simulation_->set_slingshot_input(draw, yaw, elevation);
}

bool ScraperXSimulation::request_slingshot_action() {
    return simulation_->request_slingshot_action();
}

bool ScraperXSimulation::request_slingshot_drop() {
    return simulation_->request_slingshot_drop();
}

godot::Dictionary ScraperXSimulation::get_slingshot_state() const {
    const sim::SlingshotSnapshot state = simulation_->slingshot_state();
    godot::Dictionary out;
    out["station_available"] = state.station_available;
    out["seated"] = state.seated;
    out["drawing"] = state.drawing;
    out["released"] = state.released;
    out["release_ready"] = state.release_ready;
    out["can_retrieve"] = state.can_retrieve;
    out["recovering"] = state.recovering;
    out["guided_launch"] = state.guided_launch;
    out["aim_ready"] = state.aim_ready;
    out["aim_locked"] = state.aim_locked;
    out["flight"] = state.flight;
    out["draw_m"] = state.draw_m;
    out["max_draw_m"] = state.max_draw_m;
    out["energy_j"] = state.energy_j;
    out["work_j"] = state.work_j;
    out["yaw_rad"] = state.yaw_rad;
    out["elevation_rad"] = state.elevation_rad;
    out["target_yaw_rad"] = state.target_yaw_rad;
    out["target_elevation_rad"] = state.target_elevation_rad;
    out["band_rest_m"] = state.band_rest_m;
    out["leather_deflection_m"] = state.leather_deflection_m;
    out["anchor_left"] = to_godot(state.anchor_left);
    out["anchor_right"] = to_godot(state.anchor_right);
    out["pouch_position"] = to_godot(state.pouch_position);
    out["neutral_position"] = to_godot(state.neutral_position);
    out["retrieval_control_position"] = to_godot(state.retrieval_control_position);
    out["launch_track_start"] = to_godot(state.launch_track_start);
    out["launch_track_end"] = to_godot(state.launch_track_end);
    out["launch_count"] = static_cast<std::int64_t>(state.launch_count);
    return out;
}

godot::PackedVector3Array ScraperXSimulation::get_throw_prediction() const {
    godot::PackedVector3Array result;
    for (const sim::Vector3 &point : simulation_->throw_prediction()) {
        result.push_back(godot::Vector3(static_cast<real_t>(point.x), static_cast<real_t>(point.y),
                                        static_cast<real_t>(point.z)));
    }
    return result;
}

std::int64_t ScraperXSimulation::get_throw_count() const {
    return static_cast<std::int64_t>(simulation_->snapshot().throw_count);
}

godot::PackedVector3Array ScraperXSimulation::get_slingshot_prediction() const {
    godot::PackedVector3Array out;
    for (const sim::Vector3 &point : simulation_->slingshot_prediction()) {
        out.push_back(to_godot(point));
    }
    return out;
}

godot::PackedVector3Array ScraperXSimulation::get_route_ahead() const {
    godot::PackedVector3Array out;
    for (const sim::Vector3 &point : simulation_->route_ahead()) {
        out.push_back(to_godot(point));
    }
    return out;
}

bool ScraperXSimulation::request_swing_action() {
    return simulation_->request_swing_action();
}

bool ScraperXSimulation::request_swing_drop() {
    return simulation_->request_swing_drop();
}

bool ScraperXSimulation::request_trapeze_action() {
    return simulation_->request_trapeze_action();
}

bool ScraperXSimulation::request_trapeze_drop() {
    return simulation_->request_trapeze_drop();
}

godot::Dictionary ScraperXSimulation::get_hook_state() const {
    const sim::Snapshot state = simulation_->snapshot();
    godot::Dictionary out;
    out["has_hook"] = state.has_hook;
    out["take_available"] = state.hook_take_available;
    out["state"] = static_cast<std::int64_t>(state.hook_state);
    out["position"] = to_godot(state.hook_position);
    out["line_length"] = state.hook_line_length;
    out["tension_n"] = state.hook_tension_n;
    // The line as it lies: chest, each bend round the structure, the hook.
    godot::PackedVector3Array points;
    for (std::uint32_t i = 0; i < state.hook_line_point_count; ++i) {
        points.push_back(to_godot(state.hook_line_points[i]));
    }
    out["points"] = points;
    out["bends"] = static_cast<std::int64_t>(state.hook_bend_count);
    out["tears"] = static_cast<std::int64_t>(state.hook_tear_count);
    return out;
}

bool ScraperXSimulation::set_lift_haul(const bool held) {
    return simulation_->set_lift_haul(held);
}

godot::PackedFloat32Array ScraperXSimulation::get_workers() const {
    const sim::Snapshot state = simulation_->snapshot();
    godot::PackedFloat32Array out;
    out.resize(static_cast<std::int64_t>(state.worker_count) * kWorkerFloats);
    for (std::size_t index = 0; index < state.worker_count; ++index) {
        const sim::WorkerView &worker = state.workers[index];
        const std::int64_t base = static_cast<std::int64_t>(index) * kWorkerFloats;
        out.set(base + 0, static_cast<float>(worker.position.x));
        out.set(base + 1, static_cast<float>(worker.position.y));
        out.set(base + 2, static_cast<float>(worker.position.z));
        out.set(base + 3, static_cast<float>(worker.yaw));
        out.set(base + 4, static_cast<float>(worker.state));
        out.set(base + 5, static_cast<float>(worker.speed));
        out.set(base + 6, static_cast<float>(worker.stride));
        out.set(base + 7, worker.grounded ? 1.0F : 0.0F);
    }
    return out;
}

godot::Dictionary ScraperXSimulation::get_worker_counts() const {
    const sim::Snapshot state = simulation_->snapshot();
    godot::Dictionary out;
    out["yields"] = static_cast<std::int64_t>(state.worker_yields);
    out["startles"] = static_cast<std::int64_t>(state.worker_startles);
    out["stumbles"] = static_cast<std::int64_t>(state.worker_stumbles);
    return out;
}

double ScraperXSimulation::get_surface_below(const godot::Vector3 from, const double reach) const {
    double hit_y = 0.0;
    if (!simulation_->surface_below(from.x, from.y, from.z, reach, hit_y)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return hit_y;
}

godot::Dictionary ScraperXSimulation::get_look_target(const godot::Vector3 from, const godot::Vector3 direction,
                                                      const double reach) const {
    godot::Dictionary out;
    sim::Simulation::LookHit hit;
    const bool met = simulation_->look_target(from.x, from.y, from.z, direction.x, direction.y, direction.z, reach,
                                              hit);
    out["hit"] = met;
    out["entity"] = met ? static_cast<std::int64_t>(hit.entity) : static_cast<std::int64_t>(0);
    out["point"] = met ? to_godot(hit.point) : godot::Vector3();
    out["normal"] = met ? to_godot(hit.normal) : godot::Vector3();
    out["distance"] = met ? hit.distance : reach;
    return out;
}

bool ScraperXSimulation::set_hook_aim(const godot::Vector3 direction) {
    return simulation_->set_hook_aim(direction.x, direction.y, direction.z);
}

bool ScraperXSimulation::request_hook_fire() {
    return simulation_->request_hook_fire();
}

bool ScraperXSimulation::set_hook_reel(const bool held) {
    return simulation_->set_hook_reel(held);
}

bool ScraperXSimulation::request_hook_release() {
    return simulation_->request_hook_release();
}

bool ScraperXSimulation::request_hook_take() {
    return simulation_->request_hook_take();
}

godot::Dictionary ScraperXSimulation::get_trapeze_state() const {
    const sim::TrapezeSnapshot state = simulation_->trapeze_state();
    godot::Dictionary out;
    out["catch_available"] = state.catch_available;
    out["catch_near"] = state.catch_near;
    out["harnessed"] = state.harnessed;
    out["pulled"] = state.pulled;
    out["angle_rad"] = state.angle_rad;
    out["angular_speed"] = state.angular_speed;
    out["peak_angle_rad"] = state.peak_angle_rad;
    out["pulled_in_m"] = state.pulled_in_m;
    out["catch_speed_mps"] = state.catch_speed_mps;
    out["catch_loss_j"] = state.catch_loss_j;
    out["hand_force_n"] = state.hand_force_n;
    out["peak_hand_force_n"] = state.peak_hand_force_n;
    out["arm_work_j"] = state.arm_work_j;
    out["arm_work_out_j"] = state.arm_work_out_j;
    out["mechanical_j"] = state.mechanical_j;
    out["energy_residual_j"] = state.energy_residual_j;
    out["pumps"] = static_cast<std::int64_t>(state.pumps);
    out["catches"] = static_cast<std::int64_t>(state.catches);
    out["slips"] = static_cast<std::int64_t>(state.slips);
    out["bar"] = to_godot(state.bar);
    return out;
}

bool ScraperXSimulation::request_forklift_action() {
    return simulation_->request_forklift_action();
}

bool ScraperXSimulation::set_forklift_lift(const double lift) {
    return simulation_->set_forklift_lift(lift);
}

namespace {

// A forklift: driving and offered, the forks' height, speed and steering,
// and each wheel as drawn (a cylinder along its own y, the axle).
godot::Dictionary forklift_dictionary(const sim::ForkliftSnapshot &state) {
    godot::Dictionary out;
    out["index"] = static_cast<std::int64_t>(state.index);
    out["chassis_entity"] = static_cast<std::int64_t>(state.chassis_entity);
    out["driving"] = state.driving;
    out["at_controls"] = state.at_controls;
    out["lift_m"] = state.lift_m;
    out["speed_mps"] = state.speed_mps;
    out["steer"] = state.steer;
    out["drive"] = state.drive;
    out["lift"] = state.lift;
    out["lift_n"] = state.lift_n;
    out["lift_max_n"] = state.lift_max_n;
    out["pitch_rad"] = state.pitch_rad;
    godot::PackedFloat32Array loads;
    godot::Array contacts;
    for (std::size_t wheel = 0; wheel < state.wheel_load_n.size(); ++wheel) {
        loads.push_back(static_cast<float>(state.wheel_load_n[wheel]));
        contacts.push_back(state.wheel_contact[wheel]);
    }
    out["wheel_load_n"] = loads;
    out["wheel_contact"] = contacts;
    godot::Array wheels;
    godot::PackedFloat32Array radii;
    godot::PackedFloat32Array widths;
    for (std::size_t wheel = 0; wheel < state.wheel_position.size(); ++wheel) {
        wheels.push_back(godot::Transform3D(godot::Basis(to_godot(state.wheel_rotation[wheel])),
                                            to_godot(state.wheel_position[wheel])));
        radii.push_back(static_cast<float>(state.wheel_radius[wheel]));
        widths.push_back(static_cast<float>(state.wheel_width[wheel]));
    }
    out["wheels"] = wheels;
    out["wheel_radius"] = radii;
    out["wheel_width"] = widths;
    godot::PackedFloat32Array spins;
    godot::PackedInt64Array grounds;
    for (std::size_t wheel = 0; wheel < state.wheel_spin_rad_s.size(); ++wheel) {
        spins.push_back(static_cast<float>(state.wheel_spin_rad_s[wheel]));
        grounds.push_back(static_cast<std::int64_t>(state.wheel_ground[wheel]));
    }
    out["wheel_spin_rad_s"] = spins;
    out["wheel_ground"] = grounds;
    out["gear"] = static_cast<std::int64_t>(state.gear);
    out["throttle"] = state.throttle;
    out["engine_rpm"] = state.engine_rpm;
    out["lift_mps"] = state.lift_mps;
    out["fork_gap_m"] = state.fork_gap_m;
    out["fork_drag_mps"] = state.fork_drag_mps;
    out["fork_ground"] = static_cast<std::int64_t>(state.fork_ground);
    return out;
}

} // namespace

// The forklift being driven, else the one at whose wheel the player stands,
// else the yard's.
godot::Dictionary ScraperXSimulation::get_forklift_state() const {
    return forklift_dictionary(simulation_->forklift_state());
}

std::int64_t ScraperXSimulation::get_forklift_count() const {
    return static_cast<std::int64_t>(simulation_->forklift_count());
}

godot::Dictionary ScraperXSimulation::get_forklift_state_at(const std::int64_t index) const {
    if (index < 0) {
        return {};
    }
    return forklift_dictionary(simulation_->forklift_state(static_cast<std::uint32_t>(index)));
}

bool ScraperXSimulation::request_yard_crane_take() {
    return simulation_->request_yard_crane_take();
}

bool ScraperXSimulation::request_yard_crane_operate() {
    return simulation_->request_yard_crane_operate();
}

bool ScraperXSimulation::request_yard_crane_device() {
    return simulation_->request_yard_crane_device();
}

bool ScraperXSimulation::set_yard_crane_hoist(const double hoist) {
    return simulation_->set_yard_crane_hoist(hoist);
}

// The yard crane: its remote and whether it is being worked, its drives as
// they go, the hoist's pull, the hoist rope's ends and the hook as drawn, and
// the device on its sling (the magnet's switch, the spreader's seat; what it
// holds and how hard).
godot::Dictionary ScraperXSimulation::get_yard_crane_state() const {
    const sim::YardCraneSnapshot state = simulation_->yard_crane_state();
    godot::Dictionary out;
    out["has_remote"] = state.has_remote;
    out["remote_in_reach"] = state.remote_in_reach;
    out["able"] = state.able;
    out["operating"] = state.operating;
    out["slew_rad"] = state.slew_rad;
    out["slew_rate"] = state.slew_rate;
    out["trolley_m"] = state.trolley_m;
    out["trolley_rate"] = state.trolley_rate;
    out["hoist_m"] = state.hoist_m;
    out["hoist"] = state.hoist;
    out["hoist_n"] = state.hoist_n;
    out["hoist_max_n"] = state.hoist_max_n;
    out["hoist_stalled"] = state.hoist_stalled;
    out["upper_limit"] = state.upper_limit;
    out["slack"] = state.slack;
    out["load_entity"] = static_cast<std::int64_t>(state.load_entity);
    out["rope_top"] = to_godot(state.rope_top);
    out["rope_bottom"] = to_godot(state.rope_bottom);
    out["hook"] = to_godot(state.hook);
    out["hoist_rate"] = state.hoist_rate;
    out["slew_measured"] = state.slew_measured;
    out["slew_torque_nm"] = state.slew_torque_nm;
    out["trolley_mps"] = state.trolley_mps;
    out["trolley_force_n"] = state.trolley_force_n;
    out["rope_mps"] = state.rope_mps;
    out["hook_mps"] = state.hook_mps;
    out["sling_n"] = state.sling_n;
    out["device"] = state.device;
    out["magnet_on"] = state.magnet_on;
    out["holding_entity"] = static_cast<std::int64_t>(state.holding_entity);
    out["hold_n"] = state.hold_n;
    out["hold_rated_n"] = state.hold_rated_n;
    out["spreader_seated"] = state.spreader_seated;
    out["spreader_landed"] = state.spreader_landed;
    return out;
}

// The ziplines, as built: each line's top and bottom anchors and its brake
// block's travel from its rest to the stop.
godot::Array ScraperXSimulation::get_zipline_lines() const {
    godot::Array out;
    for (std::uint32_t index = 0; index < simulation_->zipline_count(); ++index) {
        const sim::Simulation::ZiplineLine line = simulation_->zipline_line(index);
        godot::Dictionary entry;
        entry["top"] = to_godot(line.top);
        entry["bottom"] = to_godot(line.bottom);
        entry["length"] = line.length;
        entry["brake_m"] = line.brake_m;
        out.push_back(entry);
    }
    return out;
}

// Where each line's trolley and brake block are down it from the top
// anchor, in turn: [trolley_m, block_m] a line.
godot::PackedFloat32Array ScraperXSimulation::get_zipline_trolleys() const {
    godot::PackedFloat32Array out;
    for (std::uint32_t index = 0; index < simulation_->zipline_count(); ++index) {
        const sim::Simulation::ZiplineLine line = simulation_->zipline_line(index);
        out.push_back(static_cast<float>(line.trolley_m));
        out.push_back(static_cast<float>(line.block_m));
    }
    return out;
}

// Riding: which line, the trolley's place down it and its speed, and the
// bar the hands hold.
godot::Dictionary ScraperXSimulation::get_zipline_state() const {
    const sim::Snapshot &state = simulation_->snapshot();
    godot::Dictionary out;
    out["riding"] = state.zipline_riding;
    out["line"] = state.zipline_line;
    out["rides"] = static_cast<std::int64_t>(state.zipline_ride_count);
    out["speed_mps"] = state.zipline_speed_mps;
    out["along_m"] = state.zipline_along_m;
    out["braking"] = state.zipline_braking;
    out["hand_brake"] = state.zipline_hand_brake;
    out["hands_load_n"] = state.zipline_hands_load_n;
    out["torn"] = static_cast<std::int64_t>(state.zipline_torn_count);
    out["bar"] = to_godot(simulation_->zipline_bar());
    return out;
}

// The corpses, oldest first: each a Dictionary of its parts' transforms
// (sim/corpse.hpp's order) and whether it has come to rest.
// The last hits the tracked bodies took (sim/impacts.hpp), oldest first:
// {sequence, entity, at, energy_j, speed_mps, mass_kg, sound (ImpactSound)}.
godot::Array ScraperXSimulation::get_impacts() const {
    godot::Array out;
    for (const sim::ImpactSnapshot &hit : simulation_->impacts()) {
        godot::Dictionary entry;
        entry["sequence"] = static_cast<std::int64_t>(hit.sequence);
        entry["entity"] = static_cast<std::int64_t>(hit.entity);
        entry["at"] = to_godot(hit.at);
        entry["energy_j"] = hit.energy_j;
        entry["speed_mps"] = hit.speed_mps;
        entry["mass_kg"] = hit.mass_kg;
        entry["sound"] = static_cast<int>(hit.sound);
        out.push_back(entry);
    }
    return out;
}

std::int64_t ScraperXSimulation::get_last_impact_sequence() const {
    return static_cast<std::int64_t>(simulation_->last_impact_sequence());
}

godot::Array ScraperXSimulation::get_corpses() const {
    godot::Array out;
    const sim::Snapshot &state = simulation_->snapshot();
    for (std::uint32_t corpse = 0; corpse < state.corpse_count; ++corpse) {
        godot::Array parts;
        for (std::uint32_t part = 0; part < static_cast<std::uint32_t>(sim::Corpses::kParts); ++part) {
            const sim::Simulation::CorpsePart pose = simulation_->corpse_part(corpse, part);
            const godot::Quaternion q(static_cast<float>(pose.rotation.x), static_cast<float>(pose.rotation.y),
                                      static_cast<float>(pose.rotation.z), static_cast<float>(pose.rotation.w));
            parts.push_back(godot::Transform3D(godot::Basis(q), to_godot(pose.position)));
        }
        godot::Dictionary entry;
        entry["parts"] = parts;
        entry["resting"] = simulation_->corpse_resting(corpse);
        out.push_back(entry);
    }
    return out;
}

// The climber's proportions (sim/corpse.hpp): each part's capsule radius,
// half-length and mass, in the parts' order.
godot::Array ScraperXSimulation::get_corpse_plan() const {
    godot::Array out;
    for (const sim::Corpses::PartPlan &plan : sim::Corpses::kPlan) {
        godot::Dictionary entry;
        entry["radius"] = plan.radius;
        entry["half_length"] = plan.half_length;
        entry["mass_kg"] = plan.mass_kg;
        out.push_back(entry);
    }
    return out;
}

// The latest death: how many deaths have left a body, where it met what
// killed it, and how fast.
godot::Dictionary ScraperXSimulation::get_death_state() const {
    const sim::Snapshot &state = simulation_->snapshot();
    godot::Dictionary out;
    out["corpses_spawned"] = static_cast<std::int64_t>(state.corpses_spawned);
    out["corpse_count"] = static_cast<std::int64_t>(state.corpse_count);
    out["point"] = to_godot(state.death_point);
    out["speed_mps"] = state.death_speed_mps;
    out["side_on"] = state.death_side_on;
    out["hits"] = static_cast<std::int64_t>(state.hit_count);
    out["hit_speed_mps"] = state.hit_speed_mps;
    return out;
}

godot::Dictionary ScraperXSimulation::get_matron_state() const {
    const sim::MatronSnapshot state = simulation_->matron_state();
    godot::Dictionary out;
    out["state"] = static_cast<std::int64_t>(state.state);
    out["alive"] = state.alive;
    out["released"] = state.released;
    out["sees_player"] = state.sees_player;
    out["position"] = godot::Vector3(static_cast<real_t>(state.position.x), static_cast<real_t>(state.position.y),
                                     static_cast<real_t>(state.position.z));
    out["yaw"] = state.yaw;
    // Where she is drawn between two ticks, as the player is (render_*).
    out["render_position"] = to_godot(simulation_->render_matron_position());
    out["render_yaw"] = simulation_->render_matron_yaw();
    out["speed"] = state.speed;
    out["stride"] = state.stride;
    out["strain"] = state.strain;
    out["bolts"] = state.bolts_popped;
    out["noise"] = state.noise_heard;
    out["turn"] = static_cast<std::int64_t>(state.turn);
    out["turn_s"] = state.turn_s;
    out["fooled"] = static_cast<std::int64_t>(state.fooled);
    out["wary"] = state.wary;
    out["vaults"] = static_cast<std::int64_t>(state.vaults);
    out["leaps"] = static_cast<std::int64_t>(state.leaps);
    out["unrigs"] = static_cast<std::int64_t>(state.unrigs);
    out["on_line"] = state.on_line;
    out["cable_slides"] = static_cast<std::int64_t>(state.cable_slides);
    out["climbing"] = state.climbing;
    out["climb_move"] = godot::String(sim::traversal::move_name(state.climb_move));
    out["climb_move_active"] = state.climb_move_active;
    out["climb_phase"] = static_cast<std::int64_t>(state.climb_phase);
    out["climb_airborne"] = state.climb_airborne;
    // Aliases under the names the presentation prep originally read, so a
    // view written against climb_move_phase / climb_move_airborne gets the
    // same values. These are aliases, not new sim data.
    out["climb_move_phase"] = static_cast<std::int64_t>(state.climb_phase);
    out["climb_move_airborne"] = state.climb_airborne;
    // Feet support for the pose, under climb_ names: the floor under her
    // current level and the force on that support. Both are already
    // exported as floor_y / support_n; these aliases keep the climb pose
    // reading from one place. The second hold and the feet support point
    // below are real MatronSnapshot fields (see Matron::refresh_view),
    // not view-script derivations: when climb_hand2_known is false the
    // rig keeps the single-hold pose.
    out["climb_feet_floor_y"] = state.floor_y;
    out["climb_support_n"] = state.support_n;
    out["climb_hand2"] = godot::Vector3(state.climb_hand2.x, state.climb_hand2.y, state.climb_hand2.z);
    out["climb_hand2_known"] = state.climb_hand2_known;
    out["climb_feet_support"] =
        godot::Vector3(state.climb_feet_support.x, state.climb_feet_support.y, state.climb_feet_support.z);
    out["climb_feet_support_known"] = state.climb_feet_support_known;
    // Her contacts as the simulation has them: the presentation plants a limb
    // only on these, and lets every plant go when she is not supported.
    out["contact_supported"] = state.contact_supported;
    out["contact_feet"] = godot::Vector3(static_cast<real_t>(state.contact_feet.x),
                                         static_cast<real_t>(state.contact_feet.y),
                                         static_cast<real_t>(state.contact_feet.z));
    out["contact_feet_body"] = static_cast<std::int64_t>(state.contact_feet_body);
    out["contact_feet_sub"] = static_cast<std::int64_t>(state.contact_feet_sub);
    out["contact_hand_held"] = state.contact_hand_held;
    out["contact_hand"] = godot::Vector3(static_cast<real_t>(state.contact_hand.x),
                                         static_cast<real_t>(state.contact_hand.y),
                                         static_cast<real_t>(state.contact_hand.z));
    out["contact_hand_body"] = static_cast<std::int64_t>(state.contact_hand_body);
    out["contact_hand_sub"] = static_cast<std::int64_t>(state.contact_hand_sub);
    out["contact_hand2_held"] = state.contact_hand2_held;
    out["contact_hand2"] = godot::Vector3(static_cast<real_t>(state.contact_hand2.x),
                                          static_cast<real_t>(state.contact_hand2.y),
                                          static_cast<real_t>(state.contact_hand2.z));
    out["contact_line"] = state.contact_line;
    out["contact_braced"] = state.contact_braced;
    out["contact_brace"] = godot::Vector3(static_cast<real_t>(state.contact_brace.x),
                                          static_cast<real_t>(state.contact_brace.y),
                                          static_cast<real_t>(state.contact_brace.z));
    out["climb_sliding"] = state.climb_sliding;
    out["climb_leg_driving"] = state.climb_leg_driving;
    out["climb_bar_traverse"] = state.climb_bar_traverse;
    out["climbs"] = static_cast<std::int64_t>(state.climbs);
    out["clinging"] = state.clinging;
    out["floor_y"] = state.floor_y;
    out["lead_floor_y"] = state.lead_floor_y;
    out["climb_hand"] = godot::Vector3(state.climb_hand.x, state.climb_hand.y, state.climb_hand.z);
    out["climb_force_n"] = state.climb_force_n;
    out["climb_grips"] = static_cast<std::int64_t>(state.climb_grips);
    out["climb_releases"] = static_cast<std::int64_t>(state.climb_releases);
    out["mantling"] = state.mantling;
    out["lead_x"] = state.lead_x;
    out["lead_z"] = state.lead_z;
    out["lead_age_s"] = state.lead_age_s;
    out["lead_seen"] = state.lead_seen;
    out["lead_valid"] = state.lead_valid;
    out["acoustic_queries"] = static_cast<std::int64_t>(state.acoustic_queries);
    out["rage"] = state.rage;
    out["stage"] = static_cast<std::int64_t>(state.stage);
    out["exposure"] = state.exposure;
    out["shoves"] = static_cast<std::int64_t>(state.shoves);
    out["carrying"] = static_cast<std::int64_t>(state.carrying);
    out["takes"] = static_cast<std::int64_t>(state.takes);
    out["throws"] = static_cast<std::int64_t>(state.throws);
    out["staggers"] = static_cast<std::int64_t>(state.staggers);
    out["knockdowns"] = static_cast<std::int64_t>(state.knockdowns);
    out["blow_mps"] = state.blow_mps;
    out["fall"] = godot::Vector2(static_cast<real_t>(state.fall_x), static_cast<real_t>(state.fall_z));
    out["kneeling"] = state.kneeling;
    out["down_s"] = state.down_s;
    out["down_left_s"] = state.down_left_s;
    out["on_pipe"] = state.on_pipe;
    out["pipe_head_down"] = state.pipe_head_down;
    out["pipe_sliding"] = state.pipe_sliding;
    out["pipe_stunned"] = state.pipe_stunned;
    out["pipe_listening"] = state.pipe_listening;
    out["pipe_links"] = static_cast<std::int64_t>(state.pipe_links);
    out["pipe_holds"] = static_cast<std::int64_t>(state.pipe_holds);
    out["pipe_flanges"] = static_cast<std::int64_t>(state.pipe_flanges);
    out["pipe_slams"] = static_cast<std::int64_t>(state.pipe_slams);
    out["pipe_slam_mps"] = state.pipe_slam_mps;
    out["catches"] = static_cast<std::int64_t>(state.catches);
    out["planks_fallen"] = static_cast<std::int64_t>(state.planks_fallen);
    godot::PackedFloat32Array planks;
    planks.resize(static_cast<std::int64_t>(state.planks.size()));
    for (std::size_t i = 0; i < state.planks.size(); ++i) {
        planks[static_cast<std::int64_t>(i)] = static_cast<float>(state.planks[i]);
    }
    out["planks"] = planks;
    out["bracing"] = state.bracing;
    out["support_n"] = state.support_n;
    out["trap"] = static_cast<std::int64_t>(state.trap);
    out["traps"] = static_cast<std::int64_t>(state.traps);
    return out;
}

namespace {
[[nodiscard]] godot::Quaternion to_godot_quat(const double x, const double y, const double z, const double w) {
    return to_godot(sim::Quaternion{x, y, z, w});
}
} // namespace

godot::Dictionary ScraperXSimulation::get_matron_hold_state() const {
    const sim::Snapshot &p = simulation_->snapshot();
    const sim::MatronSnapshot m = simulation_->matron_state();
    godot::Dictionary out;
    // Her side.
    out["state"] = static_cast<std::int64_t>(m.state);
    out["grip_on"] = m.grip_on;
    out["hold_s"] = m.hold_s;
    out["hand"] = to_godot(m.hand);
    out["hand2"] = to_godot(m.hand2);
    out["armplay_beat"] = static_cast<std::int64_t>(m.armplay_beat);
    out["fatal_beat_view"] = static_cast<std::int64_t>(m.fatal_beat);
    out["grabs"] = static_cast<std::int64_t>(m.grabs);
    out["carries"] = static_cast<std::int64_t>(m.carries);
    out["dangles"] = static_cast<std::int64_t>(m.dangles);
    out["armplays"] = static_cast<std::int64_t>(m.armplays);
    out["releases"] = static_cast<std::int64_t>(m.releases);
    out["spares"] = static_cast<std::int64_t>(m.spares);
    out["arm_tears"] = static_cast<std::int64_t>(m.arm_tears);
    out["claims"] = static_cast<std::int64_t>(m.claims);
    out["waste_not"] = static_cast<std::int64_t>(m.waste_not);
    // The body in her hand.
    out["held"] = p.held_by_matron;
    out["free_rotation"] = p.player_free_rotation;
    out["body_rotation"] = to_godot_quat(p.player_rot_x, p.player_rot_y, p.player_rot_z, p.player_rot_w);
    out["body_angular_velocity"] = to_godot(p.player_angular_velocity);
    out["grip_load_n"] = p.grip_load_n;
    out["grip_strain_ns"] = p.grip_strain_ns;
    out["thrash"] = p.thrash;
    out["braced"] = p.braced_on_rail;
    // Arms.
    out["left_arm_lost"] = p.left_arm_lost;
    out["right_arm_lost"] = p.right_arm_lost;
    out["one_arm"] = p.one_arm;
    out["arm_tear_count"] = static_cast<std::int64_t>(p.arm_tear_count);
    out["arm_throw_count"] = static_cast<std::int64_t>(p.arm_throw_count);
    out["severed_arm_present"] = p.severed_arm_present;
    out["severed_arm_position"] = to_godot(p.severed_arm_position);
    out["severed_arm_rotation"] =
        to_godot_quat(p.severed_arm_rot_x, p.severed_arm_rot_y, p.severed_arm_rot_z, p.severed_arm_rot_w);
    out["wrist_load_n"] = p.wrist_load_n;
    out["wrist_strain_ns"] = p.wrist_strain_ns;
    out["wrist_side"] = static_cast<std::int64_t>(p.wrist_side);
    out["wrist_tear_ns"] = static_cast<double>(sim::Matron::kArmTearNs);
    out["last_severed_side"] = static_cast<std::int64_t>(p.last_severed_side);
    out["hang_hand_side"] = static_cast<std::int64_t>(p.hang_hand_side);
    out["dyno_in_flight"] = p.dyno_in_flight;
    out["dyno_catch_count"] = static_cast<std::int64_t>(p.dyno_catch_count);
    out["dyno_miss_count"] = static_cast<std::int64_t>(p.dyno_miss_count);
    out["heel_hook_count"] = static_cast<std::int64_t>(p.heel_hook_count);
    // The fatality.
    out["claimed"] = p.claimed_by_matron;
    out["fatality_beat"] = static_cast<std::int64_t>(p.fatality_beat);
    godot::PackedInt64Array edges;
    edges.resize(static_cast<std::int64_t>(p.fatality_beat_edges.size()));
    for (std::size_t i = 0; i < p.fatality_beat_edges.size(); ++i) {
        edges[static_cast<std::int64_t>(i)] = static_cast<std::int64_t>(p.fatality_beat_edges[i]);
    }
    out["fatality_beat_edges"] = edges;
    out["left_leg_lost"] = p.left_leg_lost;
    out["right_leg_lost"] = p.right_leg_lost;
    out["leg_tear_count"] = static_cast<std::int64_t>(p.leg_tear_count);
    out["leg_load_n"] = p.leg_load_n;
    out["leg_strain_ns"] = godot::Vector2(static_cast<real_t>(p.leg_strain_ns[0]), static_cast<real_t>(p.leg_strain_ns[1]));
    out["leg_tear_ns"] = static_cast<double>(sim::Matron::kLegTearNs);
    out["knee_pin_count"] = static_cast<std::int64_t>(p.knee_pin_count);
    out["matron_hand"] = to_godot(p.matron_hand);
    out["matron_hand2"] = to_godot(p.matron_hand2);
    out["player_position"] = to_godot(p.player_position);
    godot::Array legs;
    for (std::size_t k = 0; k < 2; ++k) {
        godot::Dictionary leg;
        leg["present"] = p.severed_leg_present[k];
        leg["position"] = to_godot(p.severed_leg_position[k]);
        const sim::Quaternion &q = p.severed_leg_rotation[k];
        leg["rotation"] = to_godot_quat(q.x, q.y, q.z, q.w);
        legs.append(leg);
    }
    out["severed_legs"] = legs;
    out["knee_down"] = p.matron_knee_down;
    out["knee"] = to_godot(p.matron_knee);
    out["knot_count"] = static_cast<std::int64_t>(p.knot_count);
    out["knot_load_n"] = p.knot_load_n;
    out["brake_kick_count"] = static_cast<std::int64_t>(p.brake_kick_count);
    out["haul_count"] = static_cast<std::int64_t>(p.haul_count);
    out["haul_peak_y"] = p.haul_peak_y;
    out["knot_part_count"] = static_cast<std::int64_t>(p.knot_part_count);
    out["fall_landing_count"] = static_cast<std::int64_t>(p.fall_landing_count);
    out["fall_landing_speed_mps"] = p.fall_landing_speed_mps;
    out["waste_not_count"] = static_cast<std::int64_t>(p.waste_not_count);
    out["wipe_s"] = p.wipe_s;
    out["runs_ended_by_matron"] = static_cast<std::int64_t>(p.runs_ended_by_matron);
    return out;
}

godot::Dictionary ScraperXSimulation::get_bay_state() const {
    const sim::BaySnapshot state = simulation_->bay_state();
    godot::Dictionary out;
    out["leaf_angle"] = state.leaf_angle;
    out["leaf_speed"] = state.leaf_speed;
    out["leaf_running"] = state.leaf_running;
    out["leaf_open"] = state.leaf_open;
    out["fan_fraction"] = state.fan_fraction;
    out["fan_running"] = state.fan_running;
    out["hooked"] = state.hooked;
    out["chain_n"] = state.chain_n;
    out["hands"] = state.hands;
    out["support_n"] = state.support_n;
    out["hand"] = godot::Vector3(static_cast<real_t>(state.hand.x), static_cast<real_t>(state.hand.y),
                                 static_cast<real_t>(state.hand.z));
    out["trips"] = static_cast<std::int64_t>(state.trips);
    out["restarts"] = static_cast<std::int64_t>(state.restarts);
    out["cover"] = state.cover;
    return out;
}

godot::Dictionary ScraperXSimulation::get_summit_state() const {
    const sim::SummitSnapshot state = simulation_->summit_state();
    godot::Dictionary out;
    out["lit"] = state.lit;
    out["striker_travel"] = state.striker_travel_m;
    out["striker_peak"] = state.striker_peak_m;
    out["trip_travel"] = state.trip_travel_m;
    out["lamp_yaw"] = state.lamp_yaw_rad;
    out["lamp_speed"] = state.lamp_speed_rad_s;
    return out;
}

void ScraperXSimulation::light_summit() { simulation_->light_summit(); }

// Her look over the tower (every way she climbs is one her scan finds) is
// seconds of work. The game asks for it before play, so it never lands in
// the tick she leaves her locker; asked again, it does nothing.
void ScraperXSimulation::prepare_matron() { (void)simulation_->matron_affordances(); }

godot::Dictionary ScraperXSimulation::get_swing_state() const {
    const sim::SwingSnapshot state = simulation_->swing_state();
    godot::Dictionary out;
    out["station_available"] = state.station_available;
    out["seated"] = state.seated;
    out["may_leave"] = state.may_leave;
    out["ram_held"] = state.ram_held;
    out["tripped"] = state.tripped;
    out["held_at_top"] = state.held_at_top;
    out["tooth"] = static_cast<std::int64_t>(state.tooth);
    out["seat_angle_rad"] = state.seat_angle_rad;
    out["ram_angle_rad"] = state.ram_angle_rad;
    out["seat_speed_mps"] = state.seat_speed_mps;
    out["ram_speed_mps"] = state.ram_speed_mps;
    out["seat_floor_y"] = state.seat_floor_y;
    out["apex_floor_y"] = state.apex_floor_y;
    out["kick_travel_m"] = state.kick_travel_m;
    out["buffer_compression_m"] = state.buffer_compression_m;
    out["buffer_force_n"] = state.buffer_force_n;
    out["peak_buffer_force_n"] = state.peak_buffer_force_n;
    out["peak_seat_accel_mps2"] = state.peak_seat_accel_mps2;
    out["buffer_loss_j"] = state.buffer_loss_j;
    out["energy_residual_j"] = state.energy_residual_j;
    out["mechanical_j"] = state.mechanical_j;
    out["seat_pivot"] = to_godot(state.seat_pivot);
    out["ram_pivot"] = to_godot(state.ram_pivot);
    out["seat_pin"] = to_godot(state.seat_pin);
    out["ram_pin"] = to_godot(state.ram_pin);
    return out;
}

bool ScraperXSimulation::request_lift_action() {
    return simulation_->request_lift_action();
}

godot::PackedByteArray ScraperXSimulation::get_save_game() const {
    const std::vector<std::uint8_t> bytes = simulation_->save_game();
    godot::PackedByteArray out;
    out.resize(static_cast<std::int64_t>(bytes.size()));
    if (!bytes.empty()) {
        std::memcpy(out.ptrw(), bytes.data(), bytes.size());
    }
    return out;
}

bool ScraperXSimulation::load_save_game(const godot::PackedByteArray &bytes) {
    return simulation_->load_game(bytes.ptr(), static_cast<std::size_t>(bytes.size()));
}

godot::Dictionary ScraperXSimulation::get_lift_state() const {
    const sim::LiftSnapshot state = simulation_->lift_state();
    godot::Dictionary out;
    out["machine"] = static_cast<std::int64_t>(state.machine);
    out["role"] = static_cast<std::int64_t>(state.role);
    out["travel"] = state.travel;
    out["target"] = state.target;
    out["moving"] = state.moving;
    out["machine_count"] = static_cast<std::int64_t>(state.machine_count);
    out["incline_state"] = static_cast<std::int64_t>(state.incline_state);
    out["incline_brake_n"] = state.incline_brake_n;
    out["incline_speed_mps"] = state.incline_speed_mps;
    out["tram_departs_s"] = state.tram_departs_s;
    out["derrick_iron_aboard"] = static_cast<std::int64_t>(state.derrick_iron_aboard);
    out["derrick_holding"] = state.derrick_holding;
    out["pitman"] = static_cast<std::int64_t>(state.pitman);
    out["pitman_haul_n"] = state.pitman_haul_n;
    out["wheel_hopper_kg"] = state.wheel_hopper_kg;
    out["wheel_skip_kg"] = state.wheel_skip_kg;
    godot::PackedFloat64Array travels;
    for (int i = 0; i < state.machine_count && i < static_cast<int>(state.travels.size()); ++i) {
        travels.push_back(state.travels[static_cast<std::size_t>(i)]);
    }
    out["travels"] = travels;
    return out;
}

static_assert(kMaxHullBytes == sim::fabricated::kMaxFileBytes,
              "the delivery check's bound on a hull file is the native's own");

std::int64_t ScraperXSimulation::add_fabricated(const godot::PackedByteArray &hull,
                                                const godot::PackedByteArray &signature,
                                                const godot::PackedByteArray &model, const godot::Vector3 at,
                                                const double yaw) {
    // The game layer's check first: nothing of the delivery reaches the
    // native unless its signature is the game's and its GLB is the one the
    // signed hull file names.
    const DeliveryCheck check = check_delivery(hull.ptr(), static_cast<std::size_t>(hull.size()), signature.ptr(),
                                               static_cast<std::size_t>(signature.size()), model.ptr(),
                                               static_cast<std::size_t>(model.size()));
    if (check != DeliveryCheck::Ok) {
        fabricated_refusal_ = delivery_check_name(check);
        return 0;
    }
    sim::FabricatedRefusal refusal = sim::FabricatedRefusal::None;
    const std::uint64_t entity = simulation_->add_fabricated(
        hull.ptr(), static_cast<std::size_t>(hull.size()),
        {static_cast<double>(at.x), static_cast<double>(at.y), static_cast<double>(at.z)}, yaw, &refusal);
    fabricated_refusal_ = sim::fabricated::refusal_name(refusal);
    return static_cast<std::int64_t>(entity);
}

godot::String ScraperXSimulation::get_fabricated_refusal() const {
    return fabricated_refusal_;
}

godot::String ScraperXSimulation::check_fabricated_delivery(const godot::PackedByteArray &hull,
                                                            const godot::PackedByteArray &signature,
                                                            const godot::PackedByteArray &model) const {
    return delivery_check_name(check_delivery(hull.ptr(), static_cast<std::size_t>(hull.size()), signature.ptr(),
                                              static_cast<std::size_t>(signature.size()), model.ptr(),
                                              static_cast<std::size_t>(model.size())));
}

std::int64_t ScraperXSimulation::get_fabricated_count() const {
    return static_cast<std::int64_t>(simulation_->fabricated_count());
}

godot::Dictionary ScraperXSimulation::get_fabricated_object(const std::int64_t index) const {
    godot::Dictionary out;
    if (index < 0 || index >= static_cast<std::int64_t>(simulation_->fabricated_count())) {
        return out;
    }
    const sim::FabricatedObject object = simulation_->fabricated_object(static_cast<std::uint32_t>(index));
    static const char kHex[] = "0123456789abcdef";
    std::string hash;
    for (const std::uint8_t byte : object.glb_sha512) {
        hash.push_back(kHex[byte >> 4U]);
        hash.push_back(kHex[byte & 0x0FU]);
    }
    out["entity"] = static_cast<std::int64_t>(object.entity);
    out["body"] = static_cast<std::int64_t>(object.body);
    out["material"] = static_cast<std::int64_t>(object.material);
    out["mass"] = object.mass_kg;
    out["half_extent"] = to_godot(object.half_extent);
    out["glb_sha512"] = godot::String(hash.c_str());
    return out;
}

godot::PackedVector3Array ScraperXSimulation::get_fabricated_hull_triangles(const std::int64_t index) const {
    godot::PackedVector3Array out;
    if (index < 0 || index >= static_cast<std::int64_t>(simulation_->fabricated_count())) {
        return out;
    }
    for (const sim::Vector3 &point : simulation_->fabricated_hull_triangles(static_cast<std::uint32_t>(index))) {
        out.push_back(to_godot(point));
    }
    return out;
}

} // namespace scraperx::bridge
