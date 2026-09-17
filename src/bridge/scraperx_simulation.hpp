#pragma once

#include "sim/simulation.hpp"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <memory>

namespace scraperx::bridge {

class ScraperXSimulation final : public godot::RefCounted {
    GDCLASS(ScraperXSimulation, godot::RefCounted)

public:
    ScraperXSimulation();

    [[nodiscard]] bool configure_initial_spawn(std::int64_t initial_spawn);
    [[nodiscard]] bool set_move_input(double world_x, double world_z);
    [[nodiscard]] bool set_facing(double world_x, double world_z);
    [[nodiscard]] bool request_jump();
    [[nodiscard]] bool request_traversal();
    [[nodiscard]] bool request_release();
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
    [[nodiscard]] godot::Vector3 get_moving_ledge_position() const;
    [[nodiscard]] godot::Vector3 get_moving_ledge_linear_velocity() const;
    [[nodiscard]] std::int64_t get_traversal_state() const;
    [[nodiscard]] std::int64_t get_traversal_support_entity_id() const;
    [[nodiscard]] double get_traversal_progress() const;
    [[nodiscard]] godot::Vector3 get_traversal_ledge_point() const;
    [[nodiscard]] godot::Vector3 get_traversal_target_point() const;
    [[nodiscard]] bool is_ledge_available() const;
    [[nodiscard]] std::int64_t get_ledge_entity_id() const;
    [[nodiscard]] godot::Vector3 get_ledge_point() const;
    [[nodiscard]] double get_ledge_rise_meters() const;
    [[nodiscard]] std::int64_t get_accepted_traversal_count() const;
    [[nodiscard]] std::int64_t get_rejected_traversal_count() const;
    [[nodiscard]] std::int64_t get_aborted_traversal_count() const;
    [[nodiscard]] double get_tower_height_meters() const;
    [[nodiscard]] godot::Vector3 get_hoist_scoop_position() const;
    [[nodiscard]] double get_hoist_scoop_tilt_radians() const;
    [[nodiscard]] godot::Vector3 get_ballast_position() const;
    [[nodiscard]] godot::Vector3 get_ballast_linear_velocity() const;
    [[nodiscard]] godot::Vector3 get_tipper_position() const;
    [[nodiscard]] double get_tipper_angle_radians() const;
    [[nodiscard]] double get_valve_lever_angle_radians() const;
    [[nodiscard]] double get_valve_open_fraction() const;
    [[nodiscard]] double get_rope_extension_meters() const;
    [[nodiscard]] godot::Vector3 get_lift_platform_position() const;
    [[nodiscard]] godot::Vector3 get_lift_platform_linear_velocity() const;
    [[nodiscard]] godot::Vector3 get_counterweight_position() const;
    [[nodiscard]] double get_vessel_pressure_pa() const;
    [[nodiscard]] double get_cylinder_pressure_pa() const;
    [[nodiscard]] double get_orifice_mass_flow_kg_per_s() const;
    [[nodiscard]] double get_vented_mass_kg() const;
    [[nodiscard]] double get_piston_force_n() const;
    [[nodiscard]] double get_vessel_available_energy_j() const;
    [[nodiscard]] double get_machine_cycle_phase_seconds() const;

protected:
    static void _bind_methods();

private:
    std::unique_ptr<sim::Simulation> simulation_;
};

} // namespace scraperx::bridge
