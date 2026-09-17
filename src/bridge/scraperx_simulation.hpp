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
    [[nodiscard]] bool can_operate_hopper() const;
    [[nodiscard]] bool request_hopper_release();
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

protected:
    static void _bind_methods();

private:
    sim::Simulation simulation_;
};

} // namespace scraperx::bridge
