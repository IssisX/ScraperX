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
    godot::ClassDB::bind_method(godot::D_METHOD("can_operate_hopper"), &ScraperXSimulation::can_operate_hopper);
    godot::ClassDB::bind_method(godot::D_METHOD("request_hopper_release"), &ScraperXSimulation::request_hopper_release);
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

} // namespace scraperx::bridge
