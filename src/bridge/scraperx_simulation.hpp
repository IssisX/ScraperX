#pragma once

#include "sim/simulation.hpp"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace scraperx::bridge {

class ScraperXSimulation final : public godot::RefCounted {
    GDCLASS(ScraperXSimulation, godot::RefCounted)

public:
    [[nodiscard]] bool set_move_input(double world_x, double world_z);
    [[nodiscard]] bool request_jump();
    [[nodiscard]] bool can_traverse() const;
    [[nodiscard]] bool request_traversal();
    [[nodiscard]] bool request_drop_from_hang();
    [[nodiscard]] bool can_operate_hopper() const;
    [[nodiscard]] bool request_hopper_release();
    [[nodiscard]] bool request_parachute();
    [[nodiscard]] bool commit_checkpoint();
    [[nodiscard]] bool can_enter_jib_station() const;
    [[nodiscard]] bool request_enter_jib_station();
    [[nodiscard]] bool request_exit_jib_station();
    [[nodiscard]] bool set_jib_hoist_input(double hoist);
    [[nodiscard]] bool set_jib_slew_input(double slew);
    [[nodiscard]] bool set_jib_brake(bool engaged);
    [[nodiscard]] std::int64_t advance_frame(double frame_delta_seconds);

    [[nodiscard]] std::int64_t get_tick_index() const;
    [[nodiscard]] double get_simulation_time_seconds() const;
    [[nodiscard]] double get_fixed_step_seconds() const;
    [[nodiscard]] double get_interpolation_alpha() const;

    [[nodiscard]] godot::Vector3 get_player_position() const;
    [[nodiscard]] godot::Vector3 get_player_linear_velocity() const;
    [[nodiscard]] bool is_player_grounded() const;
    [[nodiscard]] std::int64_t get_support_entity_id() const;
    [[nodiscard]] godot::Vector3 get_support_contact_point() const;
    [[nodiscard]] godot::Vector3 get_support_point_linear_velocity() const;

    [[nodiscard]] std::int64_t get_traversal_mode() const;
    [[nodiscard]] std::int64_t get_traversal_candidate_mode() const;
    [[nodiscard]] godot::Vector3 get_traversal_target_position() const;

    [[nodiscard]] godot::Vector3 get_translating_support_position() const;
    [[nodiscard]] godot::Vector3 get_translating_support_linear_velocity() const;
    [[nodiscard]] godot::Vector3 get_rotating_support_position() const;
    [[nodiscard]] double get_rotating_support_yaw_radians() const;
    [[nodiscard]] godot::Vector3 get_rotating_support_angular_velocity() const;

    [[nodiscard]] godot::Vector3 get_hopper_control_position() const;
    [[nodiscard]] godot::Vector3 get_hopper_gate_position() const;
    [[nodiscard]] godot::Vector3 get_hopper_load_position() const;
    [[nodiscard]] godot::Vector3 get_hopper_load_linear_velocity() const;
    [[nodiscard]] bool has_hopper_release_started() const;
    [[nodiscard]] bool is_hopper_gate_open() const;
    [[nodiscard]] bool has_hopper_load_moved() const;

    [[nodiscard]] godot::Vector3 get_impact_rocker_position() const;
    [[nodiscard]] godot::Vector3 get_impact_rocker_angular_velocity() const;
    [[nodiscard]] double get_impact_rocker_angle_radians() const;
    [[nodiscard]] bool has_impact_rocker_been_struck() const;
    [[nodiscard]] bool is_parachute_deployed() const;
    [[nodiscard]] bool is_parachute_allowed() const;
    [[nodiscard]] std::int64_t get_fall_severity() const;
    [[nodiscard]] std::int64_t get_fear_event_id() const;
    [[nodiscard]] bool is_checkpoint_committed() const;

    [[nodiscard]] godot::Vector3 get_jib_pendant_position() const;
    [[nodiscard]] godot::Vector3 get_jib_mast_position() const;
    [[nodiscard]] godot::Vector3 get_jib_boom_tip_position() const;
    [[nodiscard]] godot::Vector3 get_jib_hook_position() const;
    [[nodiscard]] godot::Vector3 get_jib_crate_position() const;
    [[nodiscard]] godot::Vector3 get_jib_crate_linear_velocity() const;
    [[nodiscard]] double get_jib_slew_radians() const;
    [[nodiscard]] double get_jib_winch_length_meters() const;
    [[nodiscard]] double get_jib_crate_mass_kg() const;
    [[nodiscard]] double get_jib_swl_kg() const;
    [[nodiscard]] bool is_jib_station_occupied() const;
    [[nodiscard]] bool is_jib_brake_engaged() const;
    [[nodiscard]] bool is_jib_stalled() const;
    [[nodiscard]] bool is_jib_at_hoist_limit() const;
    [[nodiscard]] bool is_jib_hook_attached() const;
    [[nodiscard]] std::int64_t get_jib_hook_load() const;
    [[nodiscard]] godot::Vector3 get_needle_position() const;
    [[nodiscard]] godot::Vector3 get_needle_linear_velocity() const;
    [[nodiscard]] double get_needle_yaw_radians() const;
    [[nodiscard]] bool is_needle_seated() const;
    [[nodiscard]] godot::Vector3 get_needle_near_landing_position() const;
    [[nodiscard]] godot::Vector3 get_needle_far_landing_position() const;
    [[nodiscard]] godot::Vector3 get_needle_bay_floor_position() const;

protected:
    static void _bind_methods();

private:
    sim::Simulation simulation_;
};

} // namespace scraperx::bridge
