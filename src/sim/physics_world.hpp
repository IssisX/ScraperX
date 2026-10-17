#pragma once

// Simulation::PhysicsWorld, the native's one authority over the bodies:
// its state and the declaration of every member. The members are defined
// by subsystem in physics_world*.cpp, each named at its first declaration.
// Private to the native.

#include "sim/physics_world_support.hpp"

#include <array>

namespace scraperx::sim {

using namespace scraperx::sim::detail;

class Simulation::PhysicsWorld final {
public:
    // (Defined in physics_world.cpp.)
    explicit PhysicsWorld(const InitialSpawn initial_spawn);

    ~PhysicsWorld();

    struct StepCommands final {
        double time_seconds = 0.0;   // the simulation's clock at the end of this tick
        double move_input_x = 0.0;
        double move_input_z = 0.0;
        double facing_x = 0.0;
        double facing_z = 0.0;
        double look_pitch = 0.0;   // radians, up positive
        bool jump_requested = false;
        bool traversal_requested = false;
        bool release_requested = false;
        bool crouch_held = false;
        bool sprint_held = false;
        bool pick_up_requested = false;
        bool set_down_requested = false;
        bool rig_requested = false;
        bool parachute_toggle_requested = false;
        // The ground slingshot: draw effort and aim, held; Action and Drop,
        // one-shot.
        double sling_draw = 0.0;
        double sling_yaw = 0.0;
        double sling_elevation = 1.4311699866353502;
        bool sling_action = false;
        bool sling_drop = false;
        // The swing: Action and Drop, one-shot.
        bool swing_action = false;
        bool swing_drop = false;
        // The trapeze: Action takes the bar (or the pole), Drop lets go;
        // Crouch held pulls up.
        bool trapeze_action = false;
        bool trapeze_drop = false;
        // A vertical machine's control: Action, one-shot.
        bool lift_action = false;
        bool lift_haul = false;
        // The yard's forklift: Action (take or let go of the controls),
        // one-shot; the forks' buttons, held (-1 lower .. 1 raise).
        bool forklift_action = false;
        double forklift_lift = 0.0;
        // The grappling hook: aim (a unit world vector), Fire and Release
        // one-shot, Reel held, Take one-shot.
        double hook_aim_x = 0.0;
        double hook_aim_y = 0.0;
        double hook_aim_z = -1.0;
        bool hook_fire = false;
        bool hook_reel = false;
        bool hook_release = false;
        bool hook_take = false;
    };

    void step(const StepCommands &commands,
              const float delta_seconds,
              const double next_time_seconds) noexcept;

    [[nodiscard]] const Snapshot &state() const noexcept {
        return state_;
    }

    [[nodiscard]] bool traversal_committed() const noexcept {
        return traversal_state_ != TraversalState::None;
    }

    [[nodiscard]] const Slingshot &slingshot() const noexcept { return *slingshot_; }
    [[nodiscard]] bool surface_below(const JPH::RVec3 from, const float reach, JPH::RVec3 &point) const noexcept;
    [[nodiscard]] const Swing &swing() const noexcept { return *swing_; }
    [[nodiscard]] const Matron &matron() const noexcept { return *matron_; }
    [[nodiscard]] Summit &summit() noexcept { return *summit_; }
    [[nodiscard]] const Summit &summit() const noexcept { return *summit_; }
    [[nodiscard]] const Trapeze &trapeze() const noexcept { return *trapeze_; }
    [[nodiscard]] const Forklift &forklift() const noexcept { return *forklift_; }
    [[nodiscard]] const Zipline &zipline() const noexcept { return *zipline_; }
    [[nodiscard]] const LooseStock &loose_stock() const noexcept { return *loose_stock_; }
    // On the forklift's floor within reach of its wheel.
    [[nodiscard]] bool at_forklift_controls() const noexcept;
    [[nodiscard]] const scraperx::sim::vertical::Route &vertical() const noexcept { return *vertical_; }
    [[nodiscard]] std::uint64_t standing_entity() const noexcept { return grounded_ ? support_entity_id_ : 0; }
    [[nodiscard]] bool slingshot_flight() const noexcept { return slingshot_flight_; }

    [[nodiscard]] std::vector<Vector3> slingshot_prediction() const;

    [[nodiscard]] const scraperx::sim::kit::Kit &kit() const noexcept {
        return *kit_;
    }

    [[nodiscard]] const scraperx::sim::bands::WetIsolation &wet() const noexcept {
        return wet_;
    }

    [[nodiscard]] const scraperx::sim::bands::PlateShop &shop() const noexcept {
        return shop_;
    }

    [[nodiscard]] const scraperx::sim::bands::FacadeCrane &crane() const noexcept {
        return crane_;
    }

    [[nodiscard]] const scraperx::sim::bands::MidstackService &service() const noexcept {
        return service_;
    }

    [[nodiscard]] const scraperx::sim::bands::Stack &stack() const noexcept {
        return stack_;
    }

private:
    static void remove_and_destroy(JPH::BodyInterface &bodies, const JPH::BodyID body_id);

    static JPH::Body *add_shape_body(JPH::BodyInterface &bodies,
                                     const JPH::Shape *shape,
                                     const JPH::RVec3 position,
                                     const JPH::Quat rotation,
                                     const JPH::EMotionType motion_type,
                                     const JPH::ObjectLayer layer,
                                     const float friction,
                                     const std::uint64_t entity_id,
                                     const float mass_kg);

    static JPH::BodyID add_box(JPH::BodyInterface &bodies,
                               const JPH::Vec3 half_extent,
                               const JPH::RVec3 position,
                               const JPH::EMotionType motion_type,
                               const JPH::ObjectLayer layer,
                               const float friction,
                               const std::uint64_t entity_id,
                               const JPH::Quat rotation = JPH::Quat::sIdentity(),
                               const float mass_kg = 0.0F);

    [[nodiscard]] static JPH::Vec3 normalized_horizontal(const double x, const double z) noexcept;

    [[nodiscard]] JPH::BodyID body_id_for_entity(const std::uint64_t entity_id) const noexcept;

    void build_world_solids(JPH::BodyInterface &bodies);

    [[nodiscard]] bool mirrors_owned_body(const JPH::AABox &bounds) const;

    void build_stack(JPH::BodyInterface &bodies);

    [[nodiscard]] JPH::TwoBodyConstraint *create_constraint(
        const JPH::TwoBodyConstraintSettings &settings,
        const JPH::BodyID first,
        const JPH::BodyID second,
        const bool track_for_teardown = true);

    [[nodiscard]] JPH::Vec3 current_support_point_velocity(
        const JPH::BodyInterface &bodies) const noexcept;

    void update_support_motion(JPH::BodyInterface &bodies,
                               const float delta_seconds,
                               const double next_time_seconds) noexcept;

    // ---- geometry probes -------------------------------------------------

    // (Defined in physics_world_probes.cpp.)
    [[nodiscard]] bool cast_ray(const JPH::RVec3 origin,
                                const JPH::Vec3 direction,
                                JPH::RayCastResult &hit) const noexcept;

    [[nodiscard]] bool cast_wall(const JPH::RVec3 origin, const JPH::Vec3 direction,
                                 const bool see_past_holds, JPH::RayCastResult &hit,
                                 JPH::RVec3 &point) const noexcept;

    [[nodiscard]] bool is_hold(const JPH::BodyID body_id,
                               const JPH::SubShapeID &sub_shape_id) const noexcept;

    [[nodiscard]] JPH::Vec3 surface_normal(const JPH::BodyID body_id,
                                           const JPH::SubShapeID &sub_shape_id,
                                           const JPH::RVec3 point) const noexcept;

    [[nodiscard]] bool footing_is_firm(const JPH::BodyInterface &bodies) const noexcept;

    // The capsule the body has now: standing, or crouched.
    [[nodiscard]] const JPH::Shape *active_player_shape() const noexcept {
        return crouched_ ? player_crouch_shape_.GetPtr() : player_shape_.GetPtr();
    }

    [[nodiscard]] bool capsule_pose_is_clear(const JPH::RVec3 centre) const noexcept {
        return shape_pose_is_clear(active_player_shape(), centre);
    }

    [[nodiscard]] bool shape_pose_is_clear(const JPH::Shape *shape,
                                           const JPH::RVec3 centre) const noexcept;

    // A springboard off what is close ahead of a run, begun (see
    // kSpringboardMinimumSpeed); false where there is none.
    bool try_begin_springboard(JPH::BodyInterface &bodies, JPH::Vec3 relative_velocity) noexcept;
    [[nodiscard]] LedgeProbe probe_ledge(const JPH::RVec3 origin,
                                         const JPH::Vec3 facing,
                                         const float minimum_rise,
                                         const float maximum_rise,
                                         const bool require_supported_landing,
                                         const bool see_past_holds = false,
                                         const float reach = kTraversalReach) const noexcept;

    [[nodiscard]] bool probe_vault_landing(const LedgeProbe &obstacle,
                                           const JPH::Vec3 facing,
                                           const float feet_y,
                                           JPH::RVec3 &landing_centre,
                                           JPH::BodyID &landing_body) const noexcept;

    // ---- support-frame helpers ------------------------------------------

    [[nodiscard]] JPH::Vec3 to_support_local(const JPH::BodyInterface &bodies,
                                             const JPH::BodyID body_id,
                                             const JPH::RVec3 world_point) const noexcept;

    [[nodiscard]] JPH::RVec3 from_support_local(const JPH::BodyInterface &bodies,
                                                const JPH::BodyID body_id,
                                                const JPH::Vec3 local_point) const noexcept;

    // ---- traversal state machine ----------------------------------------

    // ---- AS-003 carry ---------------------------------------------------------
    // Where the carried body's handle is held, relative to the player's
    // centre: forward along a horizontal bearing, a little above the centre.
    [[nodiscard]] static JPH::Vec3 carry_hand_offset(const JPH::Vec3 bearing) noexcept {
        return bearing * kCarryHandForward + JPH::Vec3(0.0F, kCarryHandUp, 0.0F);
    }

    [[nodiscard]] JPH::Vec3 carry_hand_bearing(const JPH::BodyInterface &bodies,
                                               const float delta_seconds) const noexcept;

    // Where the carried kit body's handle is, in its own frame: where its
    // build put it.
    [[nodiscard]] JPH::Vec3 carry_handle(const std::uint64_t entity) const noexcept {
        return kit_->carry_handle(entity);
    }

    JPH::BodyID carry_candidate(const JPH::BodyInterface &bodies,
                                std::uint64_t &entity) const noexcept;

    void attach_carry(const JPH::BodyID id, const std::uint64_t entity) noexcept;

    void release_carry() noexcept;

    [[nodiscard]] float load_yaw(const JPH::BodyInterface &bodies) const noexcept;

    void hold_load_yaw(JPH::BodyInterface &bodies) noexcept;

    [[nodiscard]] float carried_load_mass() const noexcept;

    [[nodiscard]] float carry_speed_scale() const noexcept;

    [[nodiscard]] float carry_ground_acceleration() const noexcept;

    void update_carry(JPH::BodyInterface &bodies, const StepCommands &commands,
                      const float delta_seconds) noexcept;

    void restore_carry_topology(const std::uint64_t committed_entity) noexcept;

    // ---- AS-006 rigging ------------------------------------------------------
    // Where the hands are this tick: the carry point, whether or not anything
    // is held.
    [[nodiscard]] JPH::RVec3 hand_position(const JPH::BodyInterface &bodies) const noexcept {
        return bodies.GetPosition(player_id_) + carry_hand_offset(facing_);
    }

    void find_rig_action(const JPH::BodyInterface &bodies) noexcept;

    void update_rig(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept;

    void update_crouch(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept;

    // ---- the grappling hook (see kHookLaunchMps) ---------------------------

    void build_hook_rack();

    [[nodiscard]] bool hook_take_in_reach(const JPH::BodyInterface &bodies) const noexcept;

    void sync_hook_from_rack() noexcept;

    enum class HookState : std::uint8_t { Stowed = 0, Flying = 1, Bitten = 2 };

    [[nodiscard]] JPH::RVec3 hook_chest(const JPH::BodyInterface &bodies) const noexcept {
        return bodies.GetPosition(player_id_) + JPH::Vec3(0.0F, 0.3F, 0.0F);
    }

    void hook_pre_step(JPH::BodyInterface &bodies, const StepCommands &commands, const float dt) noexcept;

    void fire_hook(JPH::BodyInterface &bodies, JPH::Vec3 aim) noexcept;

    void stow_hook(JPH::BodyInterface &bodies) noexcept;

    [[nodiscard]] static JPH::RVec3 closest_on_hold(const LeafBox &box, const std::uint32_t long_axis,
                                                    const JPH::RVec3 point) noexcept;

    [[nodiscard]] JPH::RVec3 hook_end(const JPH::BodyInterface &bodies) const noexcept;

    void hook_post_step(JPH::BodyInterface &bodies, const float dt) noexcept;

    void attach_hook_line(JPH::BodyInterface &bodies, JPH::BodyID anchor, JPH::RVec3 at, float max_length) noexcept;

    [[nodiscard]] JPH::RVec3 hook_bend_point(const JPH::BodyInterface &bodies, std::uint32_t bend) const noexcept;

    [[nodiscard]] JPH::RVec3 hook_anchor(const JPH::BodyInterface &bodies) const noexcept;

    [[nodiscard]] bool hook_line_blocked(JPH::RVec3 from, JPH::RVec3 to, JPH::RayCastResult &hit) const noexcept;

    void bend_hook_line(JPH::BodyInterface &bodies) noexcept;
    // The bite gives (kHookHoldForceN): the hook comes loose and flies again.
    void tear_hook(JPH::BodyInterface &bodies) noexcept;

    [[nodiscard]] bool wall_running() const noexcept { return wall_run_seconds_left_ > 0.0; }

    // (Defined in physics_world_movement.cpp.)
    bool try_untuck(JPH::BodyInterface &bodies) noexcept;
    [[nodiscard]] bool try_wall_climb(JPH::BodyInterface &bodies, const StepCommands &commands, JPH::Vec3 &velocity,
                                      JPH::Vec3 reference) noexcept;
    void update_wall_run(JPH::BodyInterface &bodies, const StepCommands &commands, JPH::Vec3 &velocity,
                         const JPH::Vec3 reference) noexcept;

    void begin_slide_if_running(JPH::BodyInterface &bodies) noexcept;

    void slide_locomotion(JPH::Vec3 &velocity, const JPH::Vec3 reference, const StepCommands &commands,
                          const float dt) noexcept;

    [[nodiscard]] bool try_stand(JPH::BodyInterface &bodies) noexcept;

    void apply_traversal_commands(JPH::BodyInterface &bodies,
                                  const StepCommands &commands) noexcept;

    [[nodiscard]] bool try_begin_jump_vault(JPH::BodyInterface &bodies) noexcept;

    // ---- Phase 4: landing recovery and the roll (see kLandingFreeSpeed) ----

    [[nodiscard]] float active_half_height() const noexcept {
        return crouched_ ? kPlayerCrouchHalfHeight : kPlayerHalfHeight;
    }

    [[nodiscard]] bool feet_supported_at(const JPH::RVec3 at, const float soles) const noexcept;

    [[nodiscard]] bool ground_runs_to(const JPH::RVec3 centre, const JPH::Vec3 offset, const float soles) const noexcept;

    [[nodiscard]] bool rolling() const noexcept {
        return landing_response_ == LandingResponse::Roll && roll_seconds_left_ > 0.0;
    }

    [[nodiscard]] double recovery_balance() const noexcept;

    void clear_recovery() noexcept;

    [[nodiscard]] bool roll_lane_is_clear(const JPH::RVec3 standing_centre, const JPH::Vec3 direction,
                                          const float length) const noexcept;

    void record_landing(JPH::BodyInterface &bodies, const bool prepared) noexcept;

    void roll_locomotion(JPH::Vec3 &velocity, const JPH::Vec3 reference, const StepCommands &commands,
                         const float dt) noexcept;

    [[nodiscard]] float ground_grip(const JPH::BodyInterface &bodies) const noexcept;

    void track_support_jolt(const JPH::Vec3 support_velocity, const float dt) noexcept;

    void apply_wind(JPH::BodyInterface &bodies, const double t) noexcept;

    void take_shove(const JPH::Vec3 velocity_now) noexcept;

    void clear_support_jolt() noexcept;

    void advance_recovery(JPH::BodyInterface &bodies, const float dt) noexcept;

    [[nodiscard]] bool apply_locomotion(JPH::BodyInterface &bodies,
                                        const StepCommands &commands,
                                        const float delta_seconds) noexcept;

    [[nodiscard]] bool cast_capsule(const JPH::RVec3 from, const JPH::Vec3 displacement,
                                    float &fraction, JPH::Vec3 &normal) const;

    void try_step_up(JPH::BodyInterface &bodies, const JPH::Vec3 relative_velocity,
                     const float delta_seconds);
    // The feet just off the ground at a walk's speeds (kCoyoteTicks) with
    // walkable support within a step below: put back on it. True when they
    // were.
    bool try_step_down(JPH::BodyInterface &bodies, JPH::Vec3 &player_velocity, JPH::Vec3 reference_velocity);

    void apply_parachute_drag(JPH::BodyInterface &bodies, const float delta_seconds) noexcept;

    // (Defined in physics_world_traversal.cpp.)
    void try_begin_hang(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept;
    // In flight with the hands up at a zipline's parked trolley
    // (Zipline::kCatchReachM): its bar taken and the body hung from it. True
    // when caught.
    bool try_catch_zipline(JPH::BodyInterface &bodies, JPH::RVec3 origin) noexcept;
    // Hung from a zipline's trolley: a swing whose hold is the slide along
    // the cable (Zipline::clip), not the arms' constraint.
    void begin_zip_ride(JPH::BodyInterface &bodies, int line) noexcept;

    void begin_hang(JPH::BodyInterface &bodies, const LedgeProbe &probe, const JPH::RVec3 origin) noexcept;

    [[nodiscard]] float vault_reach(const JPH::BodyInterface &bodies) const noexcept;

    [[nodiscard]] bool try_begin_ground_traversal(JPH::BodyInterface &bodies) noexcept;

    [[nodiscard]] bool step_is_supported(const JPH::RVec3 centre) const noexcept;

    [[nodiscard]] JPH::RVec3 mantle_approach_end(const JPH::BodyInterface &bodies,
                                                 const LedgeProbe &probe,
                                                 const JPH::RVec3 origin) const noexcept;

    void begin_mantle(JPH::BodyInterface &bodies,
                      const LedgeProbe &probe,
                      const JPH::RVec3 origin) noexcept;

    void begin_mantle_from_hang(JPH::BodyInterface &bodies) noexcept;

    void begin_vault(JPH::BodyInterface &bodies,
                     const LedgeProbe &probe,
                     const JPH::RVec3 landing_centre,
                     const JPH::BodyID landing_body,
                     const JPH::RVec3 origin) noexcept;

    [[nodiscard]] JPH::RVec3 traversal_point(const JPH::BodyInterface &bodies,
                                             const float progress) const noexcept;

    void drive_traversal(JPH::BodyInterface &bodies, const float delta_seconds) noexcept;

    void resolve_traversal_outcome(JPH::BodyInterface &bodies) noexcept;

    [[nodiscard]] JPH::Vec3 traversal_support_point_velocity(
        const JPH::BodyInterface &bodies) const noexcept;

    void complete_traversal(JPH::BodyInterface &bodies) noexcept;

    void abort_traversal(JPH::BodyInterface &bodies) noexcept;

    [[nodiscard]] JPH::Vec3 hang_shoulder_offset() const noexcept;

    void attach_hang_arms(JPH::BodyInterface &bodies) noexcept;

    void drop_hang_arms() noexcept;

    void steady_hang(JPH::BodyInterface &bodies) noexcept;

    void draw_in_hang_arms(const float delta_seconds) noexcept;

    void check_hang_grip(JPH::BodyInterface &bodies, const float delta_seconds) noexcept;

    void release_hang(JPH::BodyInterface &bodies) noexcept;
    // Jump on a swing (see kSwingLaunchMps): thrown on in its forward half,
    // let go in its back half.
    void launch_from_swing(JPH::BodyInterface &bodies) noexcept;

    // ---- Step 2 movement (MECHANISM_ASCENT_PLAN.md §8) --------------------

    // (Defined in physics_world_climb.cpp.)
    void load_hold(JPH::BodyInterface &bodies) noexcept;

    // Horizontal right of the structure a hang or climb faces.
    [[nodiscard]] JPH::Vec3 traversal_right() const noexcept {
        return JPH::Vec3(-traversal_normal_.GetZ(), 0.0F, traversal_normal_.GetX());
    }

    [[nodiscard]] Grip find_grip(const JPH::RVec3 centre, const JPH::RVec3 aim,
                                 const JPH::Vec3 facing,
                                 const float min_ahead = 0.5F * kPlayerRadius) const noexcept;

    [[nodiscard]] JPH::RVec3 hand_aim(const JPH::RVec3 centre, const float height,
                                      const float across) const noexcept;

    [[nodiscard]] JPH::RVec3 hand_point(const JPH::BodyInterface &bodies,
                                        const std::uint32_t hand) const noexcept;

    void set_hand(const JPH::BodyInterface &bodies, const std::uint32_t hand,
                  const Grip &grip) noexcept;

    void take_hand_holds(const JPH::BodyInterface &bodies, const JPH::RVec3 centre,
                         const Grip &fallback) noexcept;

    [[nodiscard]] bool climb_move_clear(const JPH::RVec3 from, const JPH::RVec3 to) const noexcept;

    void begin_swing(JPH::BodyInterface &bodies, const Grip &grip) noexcept;

    void update_swing(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept;

    void begin_climb(JPH::BodyInterface &bodies, const Grip &grip) noexcept;

    [[nodiscard]] Grip grip_in_front(const JPH::BodyInterface &bodies) const noexcept;

    void update_climb(JPH::BodyInterface &bodies, const StepCommands &commands,
                      const float delta_seconds) noexcept;

    void move_hands(const JPH::BodyInterface &bodies, const JPH::RVec3 centre,
                    const double up_input, const double side_input) noexcept;

    [[nodiscard]] bool feet_on_ground(const JPH::RVec3 centre) const noexcept;

    [[nodiscard]] bool try_top_out(JPH::BodyInterface &bodies) noexcept;

    void step_off_climb(JPH::BodyInterface &bodies) noexcept;

    void let_go_climb(JPH::BodyInterface &bodies) noexcept;

    void jump_off_climb(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept;
    // The fireman's slide down a climb's holds (kClimbSlideMps), one tick:
    // true while the body stays on them.
    bool slide_down_climb(JPH::BodyInterface &bodies, float delta_seconds) noexcept;

    [[nodiscard]] Lip probe_lip(const JPH::RVec3 near, const JPH::Vec3 normal) const noexcept;

    void update_shimmy(JPH::BodyInterface &bodies, const StepCommands &commands,
                       const float delta_seconds) noexcept;

    [[nodiscard]] Lip probe_edge_drop(const JPH::BodyInterface &bodies) const noexcept;

    [[nodiscard]] bool try_begin_lowering(JPH::BodyInterface &bodies) noexcept;

    void finish_lowering() noexcept;

    // The support under a walking body, if it is a beam: a box narrower
    // than kBalanceMaxWidth and at least kBalanceMinLength long, lying level.
    struct Beam final {
        bool valid = false;
        JPH::Vec3 along{JPH::Vec3::sZero()};
        JPH::Vec3 across{JPH::Vec3::sZero()};
        float offset = 0.0F;
        float half_width = 0.0F;
    };

    [[nodiscard]] Beam beam_underfoot(const JPH::BodyInterface &bodies) const noexcept;

    void clear_traversal() noexcept;

    // (Defined in physics_world_state.cpp.)
    void update_precision_sight(const JPH::BodyInterface &bodies) noexcept;

    void update_affordance(const JPH::BodyInterface &bodies) noexcept;

    void commit_machine_checkpoint() noexcept;

    void commit_checkpoint(const JPH::BodyInterface &bodies) noexcept;

    void restore_from_checkpoint(JPH::BodyInterface &bodies) noexcept;

public:
    bool load_save(SaveReader &r) { return read_save(r, physics_system_.GetBodyInterface()); }

    [[nodiscard]] std::vector<std::uint64_t> body_entities() const;

    void write_save(SaveWriter &w) const;

    bool read_save(SaveReader &r, JPH::BodyInterface &bodies);

    void read_machine_state() noexcept;

    void read_state() noexcept;

    JoltRuntimeLease runtime_;
    JPH::TempAllocatorImpl temp_allocator_;
    JPH::JobSystemThreadPool job_system_;
    BroadPhaseLayerInterface broadphase_layer_interface_;
    ObjectVsBroadPhaseFilter object_vs_broadphase_filter_;
    ObjectLayerPairFilter object_layer_pair_filter_;
    JPH::PhysicsSystem physics_system_;
    PlayerContactListener contact_listener_;
    JPH::RefConst<JPH::Shape> player_shape_;
    JPH::RefConst<JPH::Shape> player_crouch_shape_;
    JPH::RefConst<JPH::Shape> grip_region_shape_;
    JPH::RefConst<JPH::Shape> climb_sweep_shape_;
    JPH::BodyID deck_id_;
    JPH::BodyID translating_support_id_;
    JPH::BodyID rotating_support_id_;
    JPH::BodyID vault_rail_id_;
    JPH::BodyID mantle_ledge_id_;
    JPH::BodyID hang_ledge_id_;
    JPH::BodyID moving_ledge_id_;
    JPH::BodyID blocked_ledge_id_;
    JPH::BodyID blocked_ledge_canopy_id_;
    JPH::BodyID tower_id_;
    JPH::BodyID player_id_;

    std::vector<JPH::BodyID> machine_bodies_;
    std::vector<JPH::Ref<JPH::TwoBodyConstraint>> machine_constraints_;
    // AS-003: the carry, created/removed at runtime (track_for_teardown=false).
    JPH::Ref<JPH::PointConstraint> carry_constraint_;
    // The hands on a hang's lip (see kHangShoulderAboveCentre), null when not
    // hanging; the ticks they have been over their grip, and how many hangs
    // have slipped.
    JPH::Ref<JPH::DistanceConstraint> hang_arms_;
    int hang_over_grip_ticks_ = 0;
    float hang_over_grip_impulse_ = 0.0F;
    std::uint64_t hang_slip_count_ = 0;
    JPH::BodyID carried_id_;
    std::uint64_t carried_entity_ = 0;
    std::uint64_t carry_target_entity_ = 0;
    float grip_over_seconds_ = 0.0F;
    // AS-006: the mechanism kit and the bands built from it.
    std::unique_ptr<scraperx::sim::kit::Kit> kit_;
    std::unique_ptr<Slingshot> slingshot_;
    std::unique_ptr<Swing> swing_;
    std::unique_ptr<Trapeze> trapeze_;
    std::unique_ptr<Summit> summit_;
    std::unique_ptr<LooseStock> loose_stock_;
    std::unique_ptr<Forklift> forklift_;
    std::unique_ptr<Zipline> zipline_;
    // The swing under way hangs from a zipline's trolley (Zipline), riding it.
    bool zip_riding_ = false;
    std::unique_ptr<Matron> matron_;
    std::uint64_t matron_landing_seen_ = 0;
    bool matron_checkpoint_pending_ = false;
    [[nodiscard]] float matron_loudness(JPH::BodyInterface &bodies) noexcept;
    std::unique_ptr<Crew> crew_;
    std::unique_ptr<scraperx::sim::vertical::Route> vertical_;
    // Thrown off the slingshot's bands and not yet down: the stick steers by
    // a bounded force instead of air control.
    bool slingshot_flight_ = false;
    scraperx::sim::bands::CounterweightWell well_{};
    scraperx::sim::bands::WetIsolation wet_{};
    scraperx::sim::bands::PlateShop shop_{};
    scraperx::sim::bands::FacadeCrane crane_{};
    scraperx::sim::bands::MidstackService service_{};
    scraperx::sim::bands::Stack stack_{};
    mutable std::vector<scraperx::sim::kit::Kit::CarryCandidate> kit_carryables_;
    std::uint8_t rig_action_ = 0;
    std::uint64_t rig_target_entity_ = 0;
    scraperx::sim::kit::AnchorIndex rig_anchor_;
    scraperx::sim::kit::RopeIndex rig_rope_;
    SupportSample support_sample_{};
    JPH::Vec3 airborne_inherited_velocity_{JPH::Vec3::sZero()};
    JPH::Vec3 facing_{JPH::Vec3::sZero()};
    bool grounded_ = false;
    std::uint64_t support_entity_id_ = 0;
    double rotating_support_yaw_radians_ = 0.0;

    std::uint64_t step_up_count_ = 0;
    double step_up_meters_ = 0.0;
    std::uint64_t step_down_count_ = 0;
    std::uint64_t jump_vault_count_ = 0;
    std::uint32_t jump_vault_ticks_left_ = 0;
    std::uint32_t jump_buffer_ticks_ = 0;   // kJumpBufferTicks
    std::uint32_t coyote_ticks_ = 0;        // kCoyoteTicks, left since the feet walked off
    std::uint32_t step_up_grace_ticks_ = 0; // kStepUpGraceTicks, left since a step up
    float jump_takeoff_feet_y_ = 0.0F;
    bool crouched_ = false;
    // Crouched in the air with the legs drawn up (see kTuckMaximumFallMps).
    bool tucked_ = false;
    std::uint64_t tuck_count_ = 0;
    std::uint64_t springboard_count_ = 0;
    std::uint32_t world_solid_bodies_ = 0;
    std::uint32_t world_solid_mirrors_ = 0;
    std::uint32_t world_solid_rejected_ = 0;

    TraversalState traversal_state_ = TraversalState::None;
    JPH::BodyID traversal_body_;
    JPH::BodyID traversal_target_body_;
    std::uint64_t traversal_entity_id_ = 0;
    JPH::Vec3 traversal_local_start_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_local_approach_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_local_hold_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_local_ledge_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_local_apex_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_local_target_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_exit_relative_velocity_{JPH::Vec3::sZero()};
    float traversal_vault_horizontal_slope_ = 0.0F;
    // A vault begun close to its obstacle (begin_vault): the share of its
    // crossing that reaches the face, and the progress at which the body has
    // risen clear of the top; 0 when the eased path needs neither.
    float traversal_vault_wall_fraction_ = 1.0F;
    float traversal_vault_clear_progress_ = 0.0F;
    JPH::RVec3 traversal_desired_{JPH::RVec3::sZero()};
    // What is left of a caught ledge's pull-in (see kHangPullInSpeed).
    JPH::Vec3 traversal_entry_offset_{JPH::Vec3::sZero()};
    // What is left of a taken handle's pull-in (see kCarryPullInSpeed).
    JPH::Vec3 carry_entry_offset_{JPH::Vec3::sZero()};
    // A free load's turn about the vertical as the hands took it (see
    // hold_load_yaw).
    float carry_yaw_ = 0.0F;
    double traversal_progress_ = 0.0;
    double traversal_duration_ = kMantleDurationSeconds;
    double traversal_approach_fraction_ = 0.0;
    float traversal_approach_entry_slope_ = 0.0F;
    std::uint32_t traversal_stall_ticks_ = 0;
    std::uint32_t regrab_lockout_ticks_ = 0;
    std::uint64_t accepted_traversal_count_ = 0;
    std::uint64_t rejected_traversal_count_ = 0;
    std::uint64_t aborted_traversal_count_ = 0;
    LedgeProbe affordance_{};
    // Step 2 movement. The direction a hang or a climb faces its structure;
    // each hand's hold, in the frame of the body it is on; the affordances a
    // grounded body is offered; and what the legs are doing.
    struct HandHold final {
        JPH::BodyID body;
        JPH::Vec3 local{JPH::Vec3::sZero()};
        bool valid = false;
    };
    JPH::Vec3 traversal_normal_{JPH::Vec3::sZero()};
    HandHold hands_[2]{};
    std::uint32_t regrip_cooldown_ticks_ = 0;
    std::uint64_t climb_count_ = 0;
    float climb_slide_mps_ = 0.0F;        // the fireman's slide's speed down the holds
    bool climb_sliding_ = false;
    std::uint64_t climb_slide_count_ = 0;
    std::uint64_t swing_catch_count_ = 0;
    std::uint64_t swing_reach_count_ = 0;
    std::uint64_t swing_launch_count_ = 0;
    Grip grip_affordance_{};
    Lip edge_affordance_{};
    bool sprinting_ = false;
    bool balancing_ = false;
    // The XCoM across a beam, as a fraction of the centre of pressure's reach.
    float beam_xcom_ = 0.0F;
    JPH::Vec3 beam_lean_{JPH::Vec3::sZero()};
    // The pendulum's velocity across the beam (relative to it), and whether
    // the last tick held the body on a beam's line.
    JPH::Vec3 line_velocity_{JPH::Vec3::sZero()};
    // A jolt of the support not yet re-matched by the soles (kSupportJoltMargin).
    JPH::Vec3 support_jolt_{JPH::Vec3::sZero()};
    JPH::Vec3 jolt_support_velocity_{JPH::Vec3::sZero()};
    std::uint64_t jolt_support_entity_ = 0;
    bool jolt_stepped_ = false;
    float support_jolt_peak_m_ = 0.0F;
    std::uint64_t support_stumble_count_ = 0;
    bool line_held_ = false;
    // Airborne since a jump's takeoff (a committed arc; see the air branch).
    bool flight_from_jump_ = false;
    // Jumps whose push-off went into a dynamic support.
    std::uint64_t jump_reactions_ = 0;
    float wind_mps_ = 0.0F;
    float wind_force_n_ = 0.0F;
    float wind_peak_force_n_ = 0.0F;
    // Phase 4 shoves (take_shove): the last velocity the feet set, whether
    // it was set on the ground, the fastest closing at a side contact this
    // tick, and the blows counted.
    JPH::Vec3 last_set_velocity_ = JPH::Vec3::sZero();
    bool last_set_valid_ = false;
    float struck_closing_mps_ = 0.0F;
    std::uint64_t shove_count_ = 0;
    // Top speed the air steers toward: the walk, or what the ground gave the
    // body as it left it (a running jump keeps its speed).
    float air_full_speed_ = kPlayerMaximumRelativeSpeed;

    struct MachineCheckpoint final {
        // What was being carried, and every kit body, rope end, parted rope
        // and catch.
        std::uint64_t carrying_entity = 0;
        scraperx::sim::kit::Kit::Checkpoint kit{};
        Slingshot::State slingshot{};
        Swing::State swing{};
        Matron::Saved matron{};
        scraperx::sim::vertical::Route::State vertical{};
    };
    JPH::RVec3 checkpoint_position_{JPH::RVec3::sZero()};
    bool checkpoint_crouched_ = false;
    MachineCheckpoint checkpoint_{};
    std::uint64_t checkpoint_commit_count_ = 0;
    std::uint64_t death_count_ = 0;
    bool parachute_deployed_ = false;
    float fall_peak_speed_mps_ = 0.0F;
    float pre_contact_fall_speed_mps_ = 0.0F;
    float last_impact_speed_mps_ = 0.0F;
    JPH::Vec3 pre_contact_velocity_{JPH::Vec3::sZero()};

    // Phase 4 (see kLandingFreeSpeed).
    enum class LandingResponse : std::uint8_t { None = 0, Absorbed = 1, Heavy = 2, Roll = 3, Stumble = 4 };
    LandingResponse landing_response_ = LandingResponse::None;
    double landing_loss_ = 0.0;
    double recovery_duration_ = 0.0;
    double recovery_seconds_left_ = 0.0;
    double roll_seconds_left_ = 0.0;
    bool sliding_ = false;
    JPH::Vec3 slide_direction_ = JPH::Vec3::sZero();
    std::uint64_t slide_count_ = 0;
    // The vault's progress at which it starts down (0.5, or past the far edge).
    float traversal_vault_descend_at_ = 0.5F;
    // The wall-run (see kWallStepPushMps).
    std::uint32_t wall_steps_this_flight_ = 0;
    std::uint32_t wall_step_cooldown_ = 0;
    double wall_run_seconds_left_ = 0.0;
    JPH::Vec3 wall_normal_ = JPH::Vec3::sZero();
    std::uint32_t wall_kicks_this_flight_ = 0;
    // The wall climb (see kWallClimbMinimumSpeed).
    bool wall_climbed_this_flight_ = false;
    std::uint64_t wall_climb_count_ = 0;
    // M3, the precision jump (see kPrecisionMinM).
    float look_pitch_ = 0.0F;
    bool precision_sighted_ = false;
    JPH::RVec3 precision_point_ = JPH::RVec3::sZero();
    bool precision_target_valid_ = false;
    JPH::RVec3 precision_target_ = JPH::RVec3::sZero();
    std::uint64_t precision_jump_count_ = 0;
    std::uint64_t precision_landing_count_ = 0;
    std::uint64_t wall_step_count_ = 0;
    std::uint64_t wall_kick_count_ = 0;
    // The grappling hook.
    bool has_hook_ = false;
    HookState hook_state_ = HookState::Stowed;
    JPH::BodyID hook_body_;
    JPH::BodyID hook_rack_id_;
    kit::BodyIndex hook_gun_{};
    JPH::BodyID hook_bite_target_;
    JPH::Vec3 hook_bite_local_ = JPH::Vec3::sZero();
    JPH::Ref<kit::RopeConstraint> hook_line_;
    // The line's bends (see kHookBendsMax), from the bite outward: each held
    // on its body, with the rope laid from it back to the anchor before it
    // and the sense the line turned round it.
    struct HookBend final {
        JPH::BodyID body;
        JPH::Vec3 local = JPH::Vec3::sZero();
        JPH::Vec3 turn = JPH::Vec3::sZero();
        float laid_m = 0.0F;
    };
    std::array<HookBend, kHookBendsMax> hook_bends_{};
    std::uint32_t hook_bend_count_ = 0;
    JPH::RVec3 hook_last_chest_ = JPH::RVec3::sZero();
    std::uint64_t hook_wrap_count_ = 0;
    std::uint64_t hook_unwrap_count_ = 0;
    std::uint64_t hook_bends_refused_ = 0;
    int hook_over_ticks_ = 0;
    float hook_over_impulse_ = 0.0F;
    std::uint64_t hook_tear_count_ = 0;
    double hook_flight_seconds_ = 0.0;
    float hook_tension_n_ = 0.0F;
    std::uint64_t hook_fire_count_ = 0;
    std::uint64_t hook_bite_count_ = 0;
    std::uint64_t hook_stall_ticks_ = 0;
    // A speed vault ends past the far edge in free flight, not on a placed descent.
    bool traversal_speed_vault_ = false;
    // The vault path under way is a springboard's plant (kSpringboardSpeedKept):
    // it ends in a jump off the top.
    bool traversal_springboard_ = false;
    JPH::Vec3 roll_direction_{JPH::Vec3::sZero()};
    float roll_entry_speed_ = 0.0F;
    double roll_blocked_loss_ = 0.0;
    JPH::RVec3 roll_start_{JPH::RVec3::sZero()};
    double roll_travel_m_ = 0.0;
    float landing_normal_speed_mps_ = 0.0F;
    float landing_tangent_speed_mps_ = 0.0F;
    std::uint64_t landing_count_ = 0;
    std::uint64_t roll_count_ = 0;
    std::uint64_t roll_refused_count_ = 0;
    std::uint64_t roll_blocked_count_ = 0;
    std::uint64_t stumble_count_ = 0;
    bool teetering_ = false;
    bool summit_reached_ = false;
    double summit_seconds_ = 0.0;
    std::uint64_t summit_deaths_ = 0;
    std::uint64_t teeter_count_ = 0;

    Snapshot state_{};
};

} // namespace scraperx::sim
