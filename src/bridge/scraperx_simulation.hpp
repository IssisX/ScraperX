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
    [[nodiscard]] bool set_move_input(double world_x, double world_z);
    [[nodiscard]] bool set_facing(double world_x, double world_z);
    [[nodiscard]] bool request_jump();
    [[nodiscard]] bool request_traversal();
    [[nodiscard]] bool request_release();
    [[nodiscard]] bool request_parachute();
    [[nodiscard]] bool set_crouch_input(bool held);
    [[nodiscard]] bool set_sprint_input(bool held);
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
    // Step 2 movement read back: the points a traversal's hands are on, the
    // direction it faces its structure, the legs, and what is offered.
    [[nodiscard]] godot::Vector3 get_traversal_left_hand() const;
    [[nodiscard]] godot::Vector3 get_traversal_right_hand() const;
    [[nodiscard]] godot::Vector3 get_traversal_normal() const;
    [[nodiscard]] bool is_player_sprinting() const;
    [[nodiscard]] bool is_player_balancing() const;
    [[nodiscard]] bool is_grip_available() const;
    [[nodiscard]] godot::Vector3 get_grip_point() const;
    [[nodiscard]] bool is_edge_drop_available() const;
    [[nodiscard]] std::int64_t get_ledge_entity_id() const;
    [[nodiscard]] godot::Vector3 get_ledge_point() const;
    [[nodiscard]] double get_ledge_rise_meters() const;
    [[nodiscard]] std::int64_t get_accepted_traversal_count() const;
    [[nodiscard]] std::int64_t get_rejected_traversal_count() const;
    [[nodiscard]] std::int64_t get_aborted_traversal_count() const;
    [[nodiscard]] double get_tower_height_meters() const;

    // WO-008 fall / parachute / checkpoint.
    [[nodiscard]] std::int64_t get_fall_state() const;
    [[nodiscard]] double get_fall_peak_speed_mps() const;
    [[nodiscard]] double get_last_impact_speed_mps() const;
    [[nodiscard]] double get_lethal_impact_speed_mps() const;
    [[nodiscard]] bool is_parachute_deployed() const;
    [[nodiscard]] godot::Vector3 get_checkpoint_position() const;
    [[nodiscard]] std::int64_t get_checkpoint_commit_count() const;
    [[nodiscard]] std::int64_t get_death_count() const;

    // The carry: pick up and set down, and what is held or would be.
    [[nodiscard]] bool request_pick_up();
    [[nodiscard]] bool request_set_down();
    [[nodiscard]] std::int64_t get_carrying_entity_id() const;
    [[nodiscard]] std::int64_t get_carry_target_entity_id() const;

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
    // Rubble (the native's declared granular model), every bin as 11 floats:
    // its kit body index, contents kg, capacity kg, flowing (0 or 1), the
    // stream's from and to points (zero unless flowing), and 1 for water.
    [[nodiscard]] godot::PackedFloat32Array get_kit_bins() const;
    // Every pile of spilled rubble as 4 floats: where it lies, and its kg.
    [[nodiscard]] godot::PackedFloat32Array get_kit_piles() const;
    // AS-007 water: per pool its box (min, max) and level; per pipe whether
    // its spout pours this frame and from where to where.
    [[nodiscard]] godot::PackedFloat32Array get_kit_pools() const;
    [[nodiscard]] godot::PackedFloat32Array get_kit_spouts() const;
    // Stage A, the skip lift, read back.
    [[nodiscard]] double get_well_a_cage_travel() const;
    [[nodiscard]] bool is_well_a_catch_latched() const;
    [[nodiscard]] std::int64_t get_well_a_rope_end_entity_id() const;
    // Stage C, the debris chute, read back.
    [[nodiscard]] double get_well_c_platform_travel() const;
    [[nodiscard]] bool is_well_c_catch_latched() const;
    [[nodiscard]] double get_well_c_rebar_angle() const;
    [[nodiscard]] double get_well_c_dumpster_kg() const;
    // S1, the Stack's water-balance hoist, read back.
    [[nodiscard]] double get_stack_s1_cage_travel() const;
    [[nodiscard]] double get_stack_s1_bucket_water_kg() const;
    [[nodiscard]] double get_stack_s1_valve_angle() const;
    [[nodiscard]] bool is_stack_s1_catch_latched() const;

protected:
    static void _bind_methods();

private:
    std::unique_ptr<sim::Simulation> simulation_;
};

} // namespace scraperx::bridge
