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

constexpr std::int64_t kInitialSpawnCount = 8;

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

} // namespace scraperx::bridge
