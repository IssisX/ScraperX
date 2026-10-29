#pragma once

#include "sim/simulation.hpp"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
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
    [[nodiscard]] bool set_look_pitch(double radians);
    [[nodiscard]] bool is_precision_sighted() const;
    [[nodiscard]] godot::Vector3 get_precision_point() const;
    [[nodiscard]] std::int64_t get_precision_jump_count() const;
    [[nodiscard]] std::int64_t get_precision_landing_count() const;
    [[nodiscard]] bool request_jump();
    bool request_strike();
    [[nodiscard]] godot::Dictionary get_strike_state() const;
    [[nodiscard]] bool request_traversal();
    [[nodiscard]] bool request_up();
    [[nodiscard]] std::int64_t get_up_move() const;
    [[nodiscard]] std::int64_t get_up_resolved() const;
    [[nodiscard]] std::int64_t get_up_count() const;
    [[nodiscard]] bool request_release();
    [[nodiscard]] bool request_parachute();
    [[nodiscard]] bool set_crouch_input(bool held);
    [[nodiscard]] bool set_room_loudness(double loudness);
    [[nodiscard]] bool set_sprint_input(bool held);
    [[nodiscard]] std::int64_t advance_frame(double frame_delta_seconds);
    [[nodiscard]] std::int64_t get_tick_index() const;
    [[nodiscard]] double get_simulation_time_seconds() const;
    [[nodiscard]] double get_fixed_step_seconds() const;
    [[nodiscard]] double get_interpolation_alpha() const;
    [[nodiscard]] godot::Vector3 get_player_position() const;
    [[nodiscard]] godot::Vector3 get_player_render_position() const;
    [[nodiscard]] godot::Vector3 get_player_linear_velocity() const;
    [[nodiscard]] bool is_player_grounded() const;
    [[nodiscard]] bool is_player_crouched() const;
    [[nodiscard]] bool is_player_sliding() const;
    [[nodiscard]] bool is_player_wall_running() const;
    [[nodiscard]] godot::Vector3 get_player_wall_normal() const;
    [[nodiscard]] bool is_player_wall_climbing() const;
    [[nodiscard]] bool is_player_tucked() const;
    [[nodiscard]] bool is_player_springboarding() const;
    [[nodiscard]] std::int64_t get_springboard_count() const;
    [[nodiscard]] godot::PackedVector3Array get_throw_prediction() const;
    [[nodiscard]] std::int64_t get_throw_count() const;
    [[nodiscard]] std::int64_t get_swing_launch_count() const;
    [[nodiscard]] std::int64_t get_wall_climb_count() const;
    [[nodiscard]] std::int64_t get_wall_step_count() const;
    [[nodiscard]] std::int64_t get_wall_kick_count() const;
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
    [[nodiscard]] double get_step_up_meters() const;

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
    // The body's mass now, kg (a bin's contents included).
    [[nodiscard]] double get_kit_body_mass(std::int64_t body) const;
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
    [[nodiscard]] godot::PackedInt32Array get_loose_stock() const;
    // Every pile of spilled rubble as 4 floats: where it lies, and its kg.
    [[nodiscard]] godot::PackedFloat32Array get_kit_piles() const;
    // AS-007 water: per pool its box (min, max) and level; per pipe whether
    // its spout pours this frame and from where to where.
    [[nodiscard]] godot::PackedFloat32Array get_kit_pools() const;
    [[nodiscard]] godot::PackedFloat32Array get_kit_spouts() const;
    [[nodiscard]] godot::PackedFloat32Array get_kit_vents() const;
    // Stage A, the skip lift, read back.
    [[nodiscard]] double get_well_a_cage_travel() const;
    [[nodiscard]] bool is_well_a_catch_latched() const;
    [[nodiscard]] std::int64_t get_well_a_rope_end_entity_id() const;
    // Stage B, the derrick boom hoist, read back.
    [[nodiscard]] double get_well_b_cage_travel() const;
    [[nodiscard]] bool is_well_b_catch_latched() const;
    [[nodiscard]] std::int64_t get_well_b_rope_end_entity_id() const;
    [[nodiscard]] double get_well_b_boom_angle() const;
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
    [[nodiscard]] bool is_stack_s1_valve_pawled() const;
    // S2, the Stack's walking-beam hoist, read back.
    [[nodiscard]] double get_stack_s2_cage_travel() const;
    [[nodiscard]] double get_stack_s2_beam_angle() const;
    [[nodiscard]] bool is_stack_s2_bolt_shot() const;
    // S3, the Stack's brake-override hoist, read back.
    [[nodiscard]] double get_stack_s3_cage_travel() const;
    [[nodiscard]] double get_stack_s3_brake_angle() const;
    [[nodiscard]] bool is_stack_s3_brake_latched() const;
    // AS-007, AS-008 and AS-009, read back whole: the native's own state,
    // keyed by its field names.
    // Phase 4: the last landing and the recovery from it (Snapshot fields).
    [[nodiscard]] godot::Dictionary get_landing_state() const;
    [[nodiscard]] godot::Dictionary get_water_state() const;
    [[nodiscard]] godot::Dictionary get_hand_state() const;
    [[nodiscard]] double get_wind_mps() const;
    [[nodiscard]] godot::Dictionary get_wet_state() const;
    [[nodiscard]] godot::Dictionary get_shop_state() const;
    [[nodiscard]] godot::Dictionary get_crane_state() const;
    [[nodiscard]] godot::Dictionary get_service_state() const;

    // The ground slingshot: held draw effort (0..1) and aim, one-shot Action
    // and Drop, its state keyed by field name, and the predicted shot.
    [[nodiscard]] bool set_slingshot_input(double draw, double yaw, double elevation);
    [[nodiscard]] bool request_slingshot_action();
    [[nodiscard]] bool request_slingshot_drop();
    [[nodiscard]] godot::Dictionary get_slingshot_state() const;
    [[nodiscard]] godot::PackedVector3Array get_slingshot_prediction() const;
    // The faint route hint: what the hands could take ahead.
    [[nodiscard]] godot::PackedVector3Array get_route_ahead() const;
    // AS-012, the swing: one-shot Action (strap in, kick, unbuckle) and Drop.
    [[nodiscard]] bool request_swing_action();
    [[nodiscard]] bool request_swing_drop();
    [[nodiscard]] godot::Dictionary get_swing_state() const;
    [[nodiscard]] godot::Dictionary get_summit_state() const;
    [[nodiscard]] godot::Dictionary get_matron_state() const;
    // Test setup: her home deck (Simulation::place_matron_home), after configure_initial_spawn.
    bool place_matron_home(double deck_top);
    // Her hands on the player: the hold, the arms, the fatality and the
    // one-armed body (presentation: matron_hold_camera.gd, matron_view.gd).
    [[nodiscard]] godot::Dictionary get_matron_hold_state() const;
    [[nodiscard]] godot::Dictionary get_bay_state() const;
    void light_summit();
    void prepare_matron();
    // TEST-ONLY (the look suite, tools/look and ui_test_look.gd): the native
    // tests' own setup hooks (Simulation::debug_*), so a look scenario can put
    // her where a still needs her and let the simulation take her on from
    // there. Nothing in play calls them; they change no rule of hers.
    void debug_place_matron(godot::Vector3 feet);
    void debug_set_matron_stage(std::int64_t stage);
    [[nodiscard]] bool debug_matron_climb_way(bool pipe, bool down);
    void debug_matron_hear(godot::Vector3 at);
    void debug_launch_player(godot::Vector3 at, godot::Vector3 velocity);
    void debug_lose_arm(std::int64_t side);
    [[nodiscard]] bool request_trapeze_action();
    [[nodiscard]] bool request_trapeze_drop();
    [[nodiscard]] godot::Dictionary get_trapeze_state() const;
    [[nodiscard]] bool request_forklift_action();
    [[nodiscard]] bool set_forklift_lift(double lift);
    [[nodiscard]] godot::Dictionary get_forklift_state() const;
    [[nodiscard]] std::int64_t get_forklift_count() const;
    [[nodiscard]] godot::Dictionary get_forklift_state_at(std::int64_t index) const;
    [[nodiscard]] bool request_yard_crane_take();
    [[nodiscard]] bool request_yard_crane_operate();
    [[nodiscard]] bool request_yard_crane_device();
    [[nodiscard]] bool set_yard_crane_hoist(double hoist);
    [[nodiscard]] godot::Dictionary get_yard_crane_state() const;
    [[nodiscard]] godot::Array get_impacts() const;
    [[nodiscard]] std::int64_t get_last_impact_sequence() const;
    [[nodiscard]] godot::Array get_zipline_lines() const;
    [[nodiscard]] godot::PackedFloat32Array get_zipline_trolleys() const;
    [[nodiscard]] godot::Dictionary get_zipline_state() const;
    [[nodiscard]] godot::Array get_corpses() const;
    [[nodiscard]] godot::Array get_corpse_plan() const;
    [[nodiscard]] godot::Dictionary get_death_state() const;
    [[nodiscard]] godot::Dictionary get_hook_state() const;
    // The yard crew: kWorkerFloats per worker (x, y, z, yaw, state, speed,
    // stride, grounded), and what the player has made them do.
    static constexpr std::int64_t kWorkerFloats = 8;
    [[nodiscard]] godot::PackedFloat32Array get_workers() const;
    bool set_lift_haul(bool held);
    [[nodiscard]] godot::Dictionary get_worker_counts() const;
    // The height of the first surface below `from` within `reach`, or NAN.
    [[nodiscard]] double get_surface_below(godot::Vector3 from, double reach) const;
    // What a look from `from` along `direction` meets first within `reach`:
    // {hit, entity, point, normal, distance}. The player, the load in the
    // hands and sensors are seen through. Read only.
    [[nodiscard]] godot::Dictionary get_look_target(godot::Vector3 from, godot::Vector3 direction, double reach) const;
    bool set_hook_aim(godot::Vector3 direction);
    bool request_hook_fire();
    bool set_hook_reel(bool held);
    bool request_hook_release();
    bool request_hook_take();
    // The owner's vertical machines: one-shot Action (send the deck, or call it).
    [[nodiscard]] bool request_lift_action();
    // A saved climb (Simulation::save_game / load_game), as bytes.
    [[nodiscard]] godot::PackedByteArray get_save_game() const;
    [[nodiscard]] bool load_save_game(const godot::PackedByteArray &bytes);
    [[nodiscard]] godot::Dictionary get_lift_state() const;

    // Live fabrication (03_EXECUTION/LIVE_FABRICATION/DESIGN.md): a delivery
    // as received, its hull file, signature and GLB, checked here against
    // the game's key and the GLB's hash (fabricated_delivery.hpp) before the
    // native reads a byte of it; then the native bounds it again and builds
    // the body, its underside's middle at `at`, turned `yaw` about +y.
    // Returns its entity; 0 when refused, get_fabricated_refusal saying why
    // ("signature", "model", "size", or the native's: "too heavy", ...).
    [[nodiscard]] std::int64_t add_fabricated(const godot::PackedByteArray &hull, const godot::PackedByteArray &signature,
                                              const godot::PackedByteArray &model, godot::Vector3 at, double yaw);
    [[nodiscard]] godot::String get_fabricated_refusal() const;
    // The game layer's check alone (the delivery is not added): "ok" or why not.
    [[nodiscard]] godot::String check_fabricated_delivery(const godot::PackedByteArray &hull,
                                                          const godot::PackedByteArray &signature,
                                                          const godot::PackedByteArray &model) const;
    [[nodiscard]] std::int64_t get_fabricated_count() const;
    // One fabricated object: entity, body (its kit index), material (0 steel,
    // 1 timber, 2 rubber, 3 concrete), mass (kg), half_extent (its box about
    // its origin) and glb_sha512 (hex: the model it is drawn with).
    [[nodiscard]] godot::Dictionary get_fabricated_object(std::int64_t index) const;
    // Its hulls as triangles in its own frame: the plain crate drawn for it
    // when its model is not to hand.
    [[nodiscard]] godot::PackedVector3Array get_fabricated_hull_triangles(std::int64_t index) const;

protected:
    static void _bind_methods();

private:
    std::unique_ptr<sim::Simulation> simulation_;
    godot::String fabricated_refusal_ = "none";
};

} // namespace scraperx::bridge
