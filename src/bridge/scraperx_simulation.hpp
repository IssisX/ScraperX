#pragma once

#include "sim/simulation.hpp"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <memory>

namespace scraperx::bridge {

class ScraperXSimulation final : public godot::RefCounted {
    GDCLASS(ScraperXSimulation, godot::RefCounted)

public:
    ScraperXSimulation();

    [[nodiscard]] bool configure_initial_spawn(std::int64_t initial_spawn);
    [[nodiscard]] bool configure_regression_spawn(std::int64_t initial_spawn);
    [[nodiscard]] std::int64_t get_entity_body_count(std::int64_t entity) const;
    [[nodiscard]] std::int64_t get_moving_body_count() const;
    [[nodiscard]] bool set_move_input(double world_x, double world_z);
    [[nodiscard]] bool set_facing(double world_x, double world_z);
    [[nodiscard]] bool request_jump();
    [[nodiscard]] bool request_traversal();
    [[nodiscard]] bool request_release();
    [[nodiscard]] bool request_parachute();
    [[nodiscard]] bool set_crouch_input(bool held);
    [[nodiscard]] std::int64_t advance_frame(double frame_delta_seconds);
    [[nodiscard]] std::int64_t get_tick_index() const;
    [[nodiscard]] double get_simulation_time_seconds() const;
    [[nodiscard]] double get_fixed_step_seconds() const;
    [[nodiscard]] double get_interpolation_alpha() const;
    [[nodiscard]] godot::Vector3 get_player_position() const;
    [[nodiscard]] godot::Vector3 get_player_linear_velocity() const;
    [[nodiscard]] bool is_player_grounded() const;
    [[nodiscard]] bool is_player_crouched() const;
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
    [[nodiscard]] double get_treadle_angle_radians() const;
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

    // WO-008 fall / parachute / checkpoint.
    [[nodiscard]] std::int64_t get_fall_state() const;
    [[nodiscard]] double get_fall_peak_speed_mps() const;
    [[nodiscard]] double get_last_impact_speed_mps() const;
    [[nodiscard]] double get_lethal_impact_speed_mps() const;
    [[nodiscard]] bool is_parachute_deployed() const;
    [[nodiscard]] godot::Vector3 get_checkpoint_position() const;
    [[nodiscard]] std::int64_t get_checkpoint_commit_count() const;
    [[nodiscard]] std::int64_t get_death_count() const;

    // WO-011 KX-JIB / KX-CRATE (Ascent Atlas v1.0 kernel).
    [[nodiscard]] bool set_jib_slew_input(double value);
    [[nodiscard]] bool set_jib_hoist_input(double value);
    [[nodiscard]] bool is_jib_station_active() const;
    [[nodiscard]] double get_jib_boom_angle_radians() const;
    [[nodiscard]] godot::Vector3 get_jib_hook_position() const;
    [[nodiscard]] godot::Vector3 get_jib_hook_linear_velocity() const;
    [[nodiscard]] godot::Vector3 get_jib_crate_position() const;
    [[nodiscard]] godot::Vector3 get_jib_crate_linear_velocity() const;
    [[nodiscard]] godot::Vector3 get_jib_capacity_stand_load_position() const;

    // WO-012 KX-NEEDLE / KX-POCKETS (Ascent Atlas v1.0 kernel).
    [[nodiscard]] bool set_needle_hoist_input(double value);
    [[nodiscard]] bool is_needle_station_active() const;
    [[nodiscard]] bool is_needle_seated() const;
    [[nodiscard]] godot::Vector3 get_needle_position() const;
    [[nodiscard]] godot::Vector3 get_needle_linear_velocity() const;

    // WO-013 KX-SUMP / KX-GRATE (Ascent Atlas v1.0 kernel).
    [[nodiscard]] bool request_valve_toggle();
    [[nodiscard]] bool is_sump_station_active() const;
    [[nodiscard]] bool is_sump_isolated() const;
    [[nodiscard]] double get_sump_volume_kg() const;
    [[nodiscard]] bool is_grate_safe() const;

    // Ground Archimedes screw: gameplay exposes only the real control and
    // authoritative observations; proof-boundary load setters stay native-only.
    [[nodiscard]] bool request_water_screw_toggle();
    [[nodiscard]] bool is_water_screw_station_active() const;
    [[nodiscard]] bool is_water_screw_motor_enabled() const;
    [[nodiscard]] double get_water_screw_shaft_angle_radians() const;
    [[nodiscard]] double get_water_screw_rpm() const;
    [[nodiscard]] double get_water_screw_motor_torque_nm() const;
    [[nodiscard]] double get_water_screw_flow_m3_s() const;
    [[nodiscard]] double get_water_screw_basin_volume_m3() const;
    [[nodiscard]] double get_water_screw_tank_volume_m3() const;
    [[nodiscard]] double get_water_screw_leakage_m3() const;
    [[nodiscard]] double get_water_screw_shaft_work_j() const;

    // Ground water-weight lift.
    [[nodiscard]] bool request_water_lift_valve_toggle();
    [[nodiscard]] bool request_water_lift_release();
    [[nodiscard]] bool request_water_lift_reset();
    [[nodiscard]] bool is_water_lift_valve_station_active() const;
    [[nodiscard]] bool is_water_lift_release_station_active() const;
    [[nodiscard]] bool is_water_lift_reset_station_active() const;
    [[nodiscard]] bool is_water_lift_valve_open() const;
    [[nodiscard]] double get_water_lift_valve_flow_m3_s() const;
    [[nodiscard]] double get_water_lift_bucket_water_m3() const;
    [[nodiscard]] double get_water_lift_bucket_mass_kg() const;
    [[nodiscard]] double get_water_lift_bucket_travel_m() const;
    [[nodiscard]] double get_water_lift_cage_travel_m() const;
    [[nodiscard]] double get_water_lift_cage_peak_speed_mps() const;
    [[nodiscard]] double get_water_lift_rope_tension_n() const;
    [[nodiscard]] bool is_water_lift_bucket_catch_latched() const;
    [[nodiscard]] bool is_water_lift_upper_catch_latched() const;

    // AS-001 B00 intake rise (Ascent Atlas §6 band B00, §7 chain K0).
    [[nodiscard]] bool set_intake_slew_input(double value);
    [[nodiscard]] bool set_intake_hoist_input(double value);
    [[nodiscard]] bool is_intake_station_active() const;
    [[nodiscard]] double get_intake_boom_angle_radians() const;
    [[nodiscard]] godot::Vector3 get_intake_hook_position() const;
    [[nodiscard]] godot::Vector3 get_intake_pack_position() const;
    [[nodiscard]] godot::Vector3 get_intake_overweight_pack_position() const;
    [[nodiscard]] double get_intake_dog_angle_radians() const;
    [[nodiscard]] bool does_intake_pack_pin_dog() const;
    [[nodiscard]] bool is_intake_throat_clear() const;

    // AS-002 Legal Forty (Ascent Atlas §6, band B00's 24-40 m leftover).
    [[nodiscard]] bool request_intake_sling_release();
    [[nodiscard]] bool request_intake_sling_attach();
    [[nodiscard]] bool is_legal_forty_pack_slung() const;
    [[nodiscard]] double get_legal_forty_swing_travel_radians() const;
    [[nodiscard]] godot::Vector3 get_legal_forty_swing_flight_position() const;
    [[nodiscard]] godot::Vector3 get_legal_forty_cradle_position() const;

    // AS-003 MOD-HOOK5-RACK: the carry commands, and the cage read back.
    [[nodiscard]] bool request_pick_up();
    [[nodiscard]] bool request_set_down();
    [[nodiscard]] std::int64_t get_carrying_entity_id() const;
    [[nodiscard]] std::int64_t get_carry_target_entity_id() const;
    [[nodiscard]] bool is_hook_in_rack() const;
    [[nodiscard]] double get_hook5_door_angle_radians() const;
    [[nodiscard]] godot::Vector3 get_hook5_bar_position() const;
    [[nodiscard]] godot::Quaternion get_hook5_bar_rotation() const;
    [[nodiscard]] godot::Vector3 get_hook5_block_position() const;
    [[nodiscard]] godot::Quaternion get_hook5_block_rotation() const;

    // AS-006 and the mechanism kit. request_rig is the contextual rig verb
    // (hook a carried shackle onto the anchor in reach, or take a slack
    // hooked end off); get_rig_action says which it would do now: 0 none,
    // 1 hook, 2 unhook. get_carry_target_kind names what a pick-up would
    // take: 0 a load, 1 a rope's shackle, 2 a trip-line handle.
    [[nodiscard]] bool request_rig();
    [[nodiscard]] std::int64_t get_rig_action() const;
    [[nodiscard]] std::int64_t get_rig_target_entity_id() const;
    [[nodiscard]] std::int64_t get_carry_target_kind() const;
    // Every kit body as the native declares it, for a presentation that
    // draws exactly what collides: its boxes as 11 floats each (half x y z,
    // offset x y z, rotation x y z w, material class), and its pose.
    [[nodiscard]] std::int64_t get_kit_body_count() const;
    [[nodiscard]] std::int64_t get_kit_body_entity_id(std::int64_t body) const;
    [[nodiscard]] bool is_kit_body_dynamic(std::int64_t body) const;
    [[nodiscard]] bool is_kit_body_enabled(std::int64_t body) const;
    [[nodiscard]] godot::PackedFloat32Array get_kit_body_parts(std::int64_t body) const;
    [[nodiscard]] godot::Transform3D get_kit_body_transform(std::int64_t body) const;
    [[nodiscard]] std::int64_t get_kit_body_index(std::int64_t entity_id) const;
    // Cables: the kit's ropes, then its trip lines, each as the points it is
    // drawn through. Empty for a parted rope.
    [[nodiscard]] std::int64_t get_kit_cable_count() const;
    [[nodiscard]] godot::PackedVector3Array get_kit_cable_points(std::int64_t cable) const;
    // Stage A, the skip lift, read back.
    [[nodiscard]] double get_well_a_cage_travel() const;
    [[nodiscard]] bool is_well_a_catch_latched() const;
    [[nodiscard]] std::int64_t get_well_a_rope_end_entity_id() const;

protected:
    static void _bind_methods();

private:
    std::unique_ptr<sim::Simulation> simulation_;
};

} // namespace scraperx::bridge
