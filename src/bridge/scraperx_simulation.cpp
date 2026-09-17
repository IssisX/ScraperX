#include "bridge/scraperx_simulation.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace scraperx::bridge {

void ScraperXSimulation::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("set_move_input", "world_x", "world_z"),
                                &ScraperXSimulation::set_move_input);
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
    godot::ClassDB::bind_method(godot::D_METHOD("get_support_entity_id"),
                                &ScraperXSimulation::get_support_entity_id);
}

bool ScraperXSimulation::set_move_input(const double world_x, const double world_z) {
    const bool accepted = simulation_.set_move_input(world_x, world_z);
    if (!accepted) {
        godot::UtilityFunctions::push_error(
            "ScraperX native authority rejected non-finite movement input; state was not mutated.");
    }
    return accepted;
}

std::int64_t ScraperXSimulation::advance_frame(const double frame_delta_seconds) {
    const auto result = simulation_.advance_frame(frame_delta_seconds);
    if (!result.accepted) {
        godot::UtilityFunctions::push_error(
            "ScraperX native authority rejected an invalid frame delta; state was not mutated.");
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
    const auto position = simulation_.snapshot().player_position;
    return {static_cast<godot::real_t>(position.x),
            static_cast<godot::real_t>(position.y),
            static_cast<godot::real_t>(position.z)};
}

godot::Vector3 ScraperXSimulation::get_player_linear_velocity() const {
    const auto velocity = simulation_.snapshot().player_linear_velocity;
    return {static_cast<godot::real_t>(velocity.x),
            static_cast<godot::real_t>(velocity.y),
            static_cast<godot::real_t>(velocity.z)};
}

bool ScraperXSimulation::is_player_grounded() const {
    return simulation_.snapshot().player_grounded;
}

std::int64_t ScraperXSimulation::get_support_entity_id() const {
    return static_cast<std::int64_t>(simulation_.snapshot().support_entity_id);
}

} // namespace scraperx::bridge
