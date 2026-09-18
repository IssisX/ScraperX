#include "bridge/scraperx_simulation.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace {

[[nodiscard]] godot::Vector3 to_godot(const scraperx::sim::Vector3 &value) {
    return {static_cast<godot::real_t>(value.x),
            static_cast<godot::real_t>(value.y),
            static_cast<godot::real_t>(value.z)};
}

} // namespace

namespace scraperx::bridge {

void ScraperXSimulation::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("set_move_input", "world_x", "world_z"), &ScraperXSimulation::set_move_input);
    godot::ClassDB::bind_method(godot::D_METHOD("request_jump"), &ScraperXSimulation::request_jump);
    godot::ClassDB::bind_method(godot::D_METHOD("can_traverse"), &ScraperXSimulation::can_traverse);
    godot::ClassDB::bind_method(godot::D_METHOD("request_traversal"), &ScraperXSimulation::request_traversal);
    godot::ClassDB::bind_method(godot::D_METHOD("request_drop_from_hang"), &ScraperXSimulation::request_drop_from_hang);
    godot::ClassDB::bind_method(godot::D_METHOD("can_operate_hopper"), &ScraperXSimulation::can_operate_hopper);
    godot::ClassDB::bind_method(godot::D_METHOD("request_hopper_release"), &ScraperXSimulation::request_hopper_release);
    godot::ClassDB::bind_method(godot::D_METHOD("request_parachute"), &ScraperXSimulation::request_parachute);
    godot::ClassDB::bind_method(godot::D_METHOD("commit_checkpoint"), &ScraperXSimulation::commit_checkpoint);
    godot::ClassDB::bind_method(godot::D_METHOD("can_enter_jib_station"), &ScraperXSimulation::can_enter_jib_station);
    godot::ClassDB::bind_method(godot::D_METHOD("request_enter_jib_station"), &ScraperXSimulation::request_enter_jib_station);
    godot::ClassDB::bind_method(godot::D_METHOD("request_exit_jib_station"), &ScraperXSimulation::request_exit_jib_station);
    godot::ClassDB::bind_method(godot::D_METHOD("set_jib_hoist_input", "hoist"), &ScraperXSimulation::set_jib_hoist_input);
    godot::ClassDB::bind_method(godot::D_METHOD("set_jib_slew_input", "slew"), &ScraperXSimulation::set_jib_slew_input);
    godot::ClassDB::bind_method(godot::D_METHOD("set_jib_brake", "engaged"), &ScraperXSimulation::set_jib_brake);
    godot::ClassDB::bind_method(godot::D_METHOD("advance_frame", "frame_delta_seconds"), &ScraperXSimulation::advance_frame);

    godot::ClassDB::bind_method(godot::D_METHOD("get_tick_index"), &ScraperXSimulation::get_tick_index);
    godot::ClassDB::bind_method(godot::D_METHOD("get_simulation_time_seconds"), &ScraperXSimulation::get_simulation_time_seconds);
    godot::ClassDB::bind_method(godot::D_METHOD("get_fixed_step_seconds"), &ScraperXSimulation::get_fixed_step_seconds);
    godot::ClassDB::bind_method(godot::D_METHOD("get_interpolation_alpha"), &ScraperXSimulation::get_interpolation_alpha);

    godot::ClassDB::bind_method(godot::D_METHOD("get_player_position"), &ScraperXSimulation::get_player_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_player_linear_velocity"), &ScraperXSimulation::get_player_linear_velocity);
    godot::ClassDB::bind_method(godot::D_METHOD("is_player_grounded"), &ScraperXSimulation::is_player_grounded);
    godot::ClassDB::bind_method(godot::D_METHOD("get_support_entity_id"), &ScraperXSimulation::get_support_entity_id);
    godot::ClassDB::bind_method(godot::D_METHOD("get_support_contact_point"), &ScraperXSimulation::get_support_contact_point);
    godot::ClassDB::bind_method(godot::D_METHOD("get_support_point_linear_velocity"), &ScraperXSimulation::get_support_point_linear_velocity);

    godot::ClassDB::bind_method(godot::D_METHOD("get_traversal_mode"), &ScraperXSimulation::get_traversal_mode);
    godot::ClassDB::bind_method(godot::D_METHOD("get_traversal_candidate_mode"), &ScraperXSimulation::get_traversal_candidate_mode);
    godot::ClassDB::bind_method(godot::D_METHOD("get_traversal_target_position"), &ScraperXSimulation::get_traversal_target_position);

    godot::ClassDB::bind_method(godot::D_METHOD("get_translating_support_position"), &ScraperXSimulation::get_translating_support_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_translating_support_linear_velocity"), &ScraperXSimulation::get_translating_support_linear_velocity);
    godot::ClassDB::bind_method(godot::D_METHOD("get_rotating_support_position"), &ScraperXSimulation::get_rotating_support_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_rotating_support_yaw_radians"), &ScraperXSimulation::get_rotating_support_yaw_radians);
    godot::ClassDB::bind_method(godot::D_METHOD("get_rotating_support_angular_velocity"), &ScraperXSimulation::get_rotating_support_angular_velocity);

    godot::ClassDB::bind_method(godot::D_METHOD("get_hopper_control_position"), &ScraperXSimulation::get_hopper_control_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_hopper_gate_position"), &ScraperXSimulation::get_hopper_gate_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_hopper_load_position"), &ScraperXSimulation::get_hopper_load_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_hopper_load_linear_velocity"), &ScraperXSimulation::get_hopper_load_linear_velocity);
    godot::ClassDB::bind_method(godot::D_METHOD("has_hopper_release_started"), &ScraperXSimulation::has_hopper_release_started);
    godot::ClassDB::bind_method(godot::D_METHOD("is_hopper_gate_open"), &ScraperXSimulation::is_hopper_gate_open);
    godot::ClassDB::bind_method(godot::D_METHOD("has_hopper_load_moved"), &ScraperXSimulation::has_hopper_load_moved);

    godot::ClassDB::bind_method(godot::D_METHOD("get_impact_rocker_position"), &ScraperXSimulation::get_impact_rocker_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_impact_rocker_angular_velocity"), &ScraperXSimulation::get_impact_rocker_angular_velocity);
    godot::ClassDB::bind_method(godot::D_METHOD("get_impact_rocker_angle_radians"), &ScraperXSimulation::get_impact_rocker_angle_radians);
    godot::ClassDB::bind_method(godot::D_METHOD("has_impact_rocker_been_struck"), &ScraperXSimulation::has_impact_rocker_been_struck);
    godot::ClassDB::bind_method(godot::D_METHOD("is_parachute_deployed"), &ScraperXSimulation::is_parachute_deployed);
    godot::ClassDB::bind_method(godot::D_METHOD("is_parachute_allowed"), &ScraperXSimulation::is_parachute_allowed);
    godot::ClassDB::bind_method(godot::D_METHOD("get_fall_severity"), &ScraperXSimulation::get_fall_severity);
    godot::ClassDB::bind_method(godot::D_METHOD("get_fear_event_id"), &ScraperXSimulation::get_fear_event_id);
    godot::ClassDB::bind_method(godot::D_METHOD("is_checkpoint_committed"), &ScraperXSimulation::is_checkpoint_committed);
    godot::ClassDB::bind_method(godot::D_METHOD("get_jib_pendant_position"), &ScraperXSimulation::get_jib_pendant_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_jib_mast_position"), &ScraperXSimulation::get_jib_mast_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_jib_boom_tip_position"), &ScraperXSimulation::get_jib_boom_tip_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_jib_hook_position"), &ScraperXSimulation::get_jib_hook_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_jib_crate_position"), &ScraperXSimulation::get_jib_crate_position);
    godot::ClassDB::bind_method(godot::D_METHOD("get_jib_crate_linear_velocity"), &ScraperXSimulation::get_jib_crate_linear_velocity);
    godot::ClassDB::bind_method(godot::D_METHOD("get_jib_slew_radians"), &ScraperXSimulation::get_jib_slew_radians);
    godot::ClassDB::bind_method(godot::D_METHOD("get_jib_winch_length_meters"), &ScraperXSimulation::get_jib_winch_length_meters);
    godot::ClassDB::bind_method(godot::D_METHOD("get_jib_crate_mass_kg"), &ScraperXSimulation::get_jib_crate_mass_kg);
    godot::ClassDB::bind_method(godot::D_METHOD("get_jib_swl_kg"), &ScraperXSimulation::get_jib_swl_kg);
    godot::ClassDB::bind_method(godot::D_METHOD("is_jib_station_occupied"), &ScraperXSimulation::is_jib_station_occupied);
    godot::ClassDB::bind_method(godot::D_METHOD("is_jib_brake_engaged"), &ScraperXSimulation::is_jib_brake_engaged);
    godot::ClassDB::bind_method(godot::D_METHOD("is_jib_stalled"), &ScraperXSimulation::is_jib_stalled);
    godot::ClassDB::bind_method(godot::D_METHOD("is_jib_at_hoist_limit"), &ScraperXSimulation::is_jib_at_hoist_limit);
    godot::ClassDB::bind_method(godot::D_METHOD("is_jib_hook_attached"), &ScraperXSimulation::is_jib_hook_attached);
}

bool ScraperXSimulation::set_move_input(const double world_x, const double world_z) {
    const bool accepted = simulation_.set_move_input(world_x, world_z);
    if (!accepted) {
        godot::UtilityFunctions::push_error("ScraperX native authority rejected non-finite movement input; state was not mutated.");
    }
    return accepted;
}

bool ScraperXSimulation::request_jump() {
    return simulation_.request_jump();
}

bool ScraperXSimulation::can_traverse() const {
    return simulation_.can_traverse();
}

bool ScraperXSimulation::request_traversal() {
    const bool accepted = simulation_.request_traversal();
    if (!accepted) {
        godot::UtilityFunctions::push_warning(
            "ScraperX native authority rejected traversal: no physically valid candidate is in reach or a traversal request is already queued.");
    }
    return accepted;
}

bool ScraperXSimulation::request_drop_from_hang() {
    const bool accepted = simulation_.request_drop_from_hang();
    if (!accepted) {
        godot::UtilityFunctions::push_warning(
            "ScraperX native authority rejected hang drop: the player is not currently hanging or a drop is already queued.");
    }
    return accepted;
}

bool ScraperXSimulation::can_operate_hopper() const {
    return simulation_.can_operate_hopper();
}

bool ScraperXSimulation::request_hopper_release() {
    const bool accepted = simulation_.request_hopper_release();
    if (!accepted) {
        godot::UtilityFunctions::push_warning(
            "ScraperX native authority rejected hopper release: player is out of range or the release has already started.");
    }
    return accepted;
}

bool ScraperXSimulation::request_parachute() {
    return simulation_.request_parachute();
}

bool ScraperXSimulation::commit_checkpoint() {
    return simulation_.commit_checkpoint();
}

bool ScraperXSimulation::can_enter_jib_station() const {
    return simulation_.can_enter_jib_station();
}

bool ScraperXSimulation::request_enter_jib_station() {
    const bool accepted = simulation_.request_enter_jib_station();
    if (!accepted) {
        godot::UtilityFunctions::push_warning(
            "ScraperX native authority rejected jib station entry: player is out of range or already at the pendant.");
    }
    return accepted;
}

bool ScraperXSimulation::request_exit_jib_station() {
    return simulation_.request_exit_jib_station();
}

bool ScraperXSimulation::set_jib_hoist_input(const double hoist) {
    return simulation_.set_jib_hoist_input(hoist);
}

bool ScraperXSimulation::set_jib_slew_input(const double slew) {
    return simulation_.set_jib_slew_input(slew);
}

bool ScraperXSimulation::set_jib_brake(const bool engaged) {
    return simulation_.set_jib_brake(engaged);
}

std::int64_t ScraperXSimulation::advance_frame(const double frame_delta_seconds) {
    const auto result = simulation_.advance_frame(frame_delta_seconds);
    if (!result.accepted) {
        godot::UtilityFunctions::push_error("ScraperX native authority rejected an invalid frame delta; state was not mutated.");
        return -1;
    }
    return static_cast<std::int64_t>(result.steps_advanced);
}

std::int64_t ScraperXSimulation::get_tick_index() const {
    return static_cast<std::int64_t>(simulation_.snapshot().tick_index);
}

double ScraperXSimulation::get_simulation_time_seconds() const {
    return simulation_.snapshot().simulation_time_seconds;
}

double ScraperXSimulation::get_fixed_step_seconds() const {
    return simulation_.snapshot().fixed_step_seconds;
}

double ScraperXSimulation::get_interpolation_alpha() const {
    return simulation_.snapshot().interpolation_alpha;
}

godot::Vector3 ScraperXSimulation::get_player_position() const {
    return to_godot(simulation_.snapshot().player_position);
}

godot::Vector3 ScraperXSimulation::get_player_linear_velocity() const {
    return to_godot(simulation_.snapshot().player_linear_velocity);
}

bool ScraperXSimulation::is_player_grounded() const {
    return simulation_.snapshot().player_grounded;
}

std::int64_t ScraperXSimulation::get_support_entity_id() const {
    return static_cast<std::int64_t>(simulation_.snapshot().support_entity_id);
}

godot::Vector3 ScraperXSimulation::get_support_contact_point() const {
    return to_godot(simulation_.snapshot().support_contact_point);
}

godot::Vector3 ScraperXSimulation::get_support_point_linear_velocity() const {
    return to_godot(simulation_.snapshot().support_point_linear_velocity);
}

std::int64_t ScraperXSimulation::get_traversal_mode() const {
    return static_cast<std::int64_t>(simulation_.snapshot().traversal_mode);
}

std::int64_t ScraperXSimulation::get_traversal_candidate_mode() const {
    return static_cast<std::int64_t>(simulation_.snapshot().traversal_candidate_mode);
}

godot::Vector3 ScraperXSimulation::get_traversal_target_position() const {
    return to_godot(simulation_.snapshot().traversal_target_position);
}

godot::Vector3 ScraperXSimulation::get_translating_support_position() const {
    return to_godot(simulation_.snapshot().translating_support_position);
}

godot::Vector3 ScraperXSimulation::get_translating_support_linear_velocity() const {
    return to_godot(simulation_.snapshot().translating_support_linear_velocity);
}

godot::Vector3 ScraperXSimulation::get_rotating_support_position() const {
    return to_godot(simulation_.snapshot().rotating_support_position);
}

double ScraperXSimulation::get_rotating_support_yaw_radians() const {
    return simulation_.snapshot().rotating_support_yaw_radians;
}

godot::Vector3 ScraperXSimulation::get_rotating_support_angular_velocity() const {
    return to_godot(simulation_.snapshot().rotating_support_angular_velocity);
}

godot::Vector3 ScraperXSimulation::get_hopper_control_position() const {
    return to_godot(simulation_.snapshot().hopper_control_position);
}

godot::Vector3 ScraperXSimulation::get_hopper_gate_position() const {
    return to_godot(simulation_.snapshot().hopper_gate_position);
}

godot::Vector3 ScraperXSimulation::get_hopper_load_position() const {
    return to_godot(simulation_.snapshot().hopper_load_position);
}

godot::Vector3 ScraperXSimulation::get_hopper_load_linear_velocity() const {
    return to_godot(simulation_.snapshot().hopper_load_linear_velocity);
}

bool ScraperXSimulation::has_hopper_release_started() const {
    return simulation_.snapshot().hopper_release_started;
}

bool ScraperXSimulation::is_hopper_gate_open() const {
    return simulation_.snapshot().hopper_gate_open;
}

bool ScraperXSimulation::has_hopper_load_moved() const {
    return simulation_.snapshot().hopper_load_moved;
}

godot::Vector3 ScraperXSimulation::get_impact_rocker_position() const {
    return to_godot(simulation_.snapshot().impact_rocker_position);
}

godot::Vector3 ScraperXSimulation::get_impact_rocker_angular_velocity() const {
    return to_godot(simulation_.snapshot().impact_rocker_angular_velocity);
}

double ScraperXSimulation::get_impact_rocker_angle_radians() const {
    return simulation_.snapshot().impact_rocker_angle_radians;
}

bool ScraperXSimulation::has_impact_rocker_been_struck() const {
    return simulation_.snapshot().impact_rocker_struck;
}

bool ScraperXSimulation::is_parachute_deployed() const {
    return simulation_.snapshot().parachute_deployed;
}

bool ScraperXSimulation::is_parachute_allowed() const {
    return simulation_.snapshot().parachute_allowed;
}

std::int64_t ScraperXSimulation::get_fall_severity() const {
    return static_cast<std::int64_t>(simulation_.snapshot().fall_severity);
}

std::int64_t ScraperXSimulation::get_fear_event_id() const {
    return static_cast<std::int64_t>(simulation_.snapshot().fear_event_id);
}

bool ScraperXSimulation::is_checkpoint_committed() const {
    return simulation_.snapshot().checkpoint_committed;
}

godot::Vector3 ScraperXSimulation::get_jib_pendant_position() const {
    return to_godot(simulation_.snapshot().jib_pendant_position);
}

godot::Vector3 ScraperXSimulation::get_jib_mast_position() const {
    return to_godot(simulation_.snapshot().jib_mast_position);
}

godot::Vector3 ScraperXSimulation::get_jib_boom_tip_position() const {
    return to_godot(simulation_.snapshot().jib_boom_tip_position);
}

godot::Vector3 ScraperXSimulation::get_jib_hook_position() const {
    return to_godot(simulation_.snapshot().jib_hook_position);
}

godot::Vector3 ScraperXSimulation::get_jib_crate_position() const {
    return to_godot(simulation_.snapshot().jib_crate_position);
}

godot::Vector3 ScraperXSimulation::get_jib_crate_linear_velocity() const {
    return to_godot(simulation_.snapshot().jib_crate_linear_velocity);
}

double ScraperXSimulation::get_jib_slew_radians() const {
    return simulation_.snapshot().jib_slew_radians;
}

double ScraperXSimulation::get_jib_winch_length_meters() const {
    return simulation_.snapshot().jib_winch_length_meters;
}

double ScraperXSimulation::get_jib_crate_mass_kg() const {
    return simulation_.snapshot().jib_crate_mass_kg;
}

double ScraperXSimulation::get_jib_swl_kg() const {
    return simulation_.snapshot().jib_swl_kg;
}

bool ScraperXSimulation::is_jib_station_occupied() const {
    return simulation_.snapshot().jib_station_occupied;
}

bool ScraperXSimulation::is_jib_brake_engaged() const {
    return simulation_.snapshot().jib_brake_engaged;
}

bool ScraperXSimulation::is_jib_stalled() const {
    return simulation_.snapshot().jib_stalled;
}

bool ScraperXSimulation::is_jib_at_hoist_limit() const {
    return simulation_.snapshot().jib_at_hoist_limit;
}

bool ScraperXSimulation::is_jib_hook_attached() const {
    return simulation_.snapshot().jib_hook_attached;
}

} // namespace scraperx::bridge
