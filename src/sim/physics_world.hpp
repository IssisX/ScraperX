#pragma once

// Simulation::PhysicsWorld, the native's one authority over the bodies:
// its state and the declaration of every member. The members are defined
// by subsystem in physics_world*.cpp, each named at its first declaration.
// Private to the native.

#include "sim/physics_world_support.hpp"
#include "sim/fabricated.hpp"
#include "sim/matron_traversal.hpp"

#include <array>
#include <set>

namespace scraperx::sim {

using namespace scraperx::sim::detail;

class Simulation::PhysicsWorld final {
public:
    // (Defined in physics_world.cpp.)
    PhysicsWorld(const InitialSpawn initial_spawn, std::vector<Simulation::TestExtraSolid> test_extra_solids);

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
        // The gesture controls' "up": resolved at the top of the tick into
        // Jump, Action or the trapeze's request (resolve_up).
        bool up_requested = false;
        bool release_requested = false;
        bool strike_requested = false;   // the Action button's punch (physics_world_strike.cpp)
        bool crouch_held = false;
        bool sprint_held = false;
        // The player's real room through the microphone, 0..1 (held; see
        // Simulation::set_room_loudness): heard by the Matron as the
        // player's own noise.
        double room_loudness = 0.0;
        bool pick_up_requested = false;
        bool set_down_requested = false;
        bool throw_requested = false;   // UpMove::Throw, resolved from the up gesture
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
        // The yard crane: Take the remote, its switch and the device switch
        // (the device's on the sling: the magnet on or off, the spreader's
        // LOCK or UNLOCK), one-shot; the hoist's buttons, held (-1 lowering ..
        // 1 raising). Working it, the stick sends the hook.
        bool yard_crane_take = false;
        bool yard_crane_operate = false;
        bool yard_crane_device = false;
        double yard_crane_hoist = 0.0;
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
    // What a look along `run` from `from` meets first: the entity of the body
    // it meets, the point and the surface's normal there. The player's own
    // body, the load in the hands and sensors are seen through. Read only
    // (physics_world_probes.cpp).
    [[nodiscard]] bool look_hit(const JPH::RVec3 from, const JPH::Vec3 run, std::uint64_t &entity, JPH::RVec3 &point,
                                JPH::Vec3 &normal) const noexcept;
    [[nodiscard]] const Swing &swing() const noexcept { return *swing_; }
    [[nodiscard]] const Matron &matron() const noexcept { return *matron_; }
    [[nodiscard]] Matron &matron() noexcept { return *matron_; }
    [[nodiscard]] const JPH::PhysicsSystem &physics_system() const noexcept { return physics_system_; }
    // What the tower's steel offers the Matron's climbing (matron_traversal.hpp):
    // read from the real collision geometry the first time it is asked for.
    [[nodiscard]] const traversal::AffordanceMap &matron_affordances();
    [[nodiscard]] double matron_gravity_factor() const;
    [[nodiscard]] std::size_t matron_grid_walk(JPH::Vec3 from, JPH::Vec3 to, float margin, int max_nodes) const;
    [[nodiscard]] const Bay &bay() const noexcept { return *bay_; }
    [[nodiscard]] Bay &bay() noexcept { return *bay_; }
    // Tests only (never bound to the bridge): a kit body put at `at` moving at
    // `velocity`, not heard as a hit where it was put.
    bool place_matron_home(float deck_top);
    void launch_matron_for_test(JPH::RVec3 at);
    void launch_player_for_test(JPH::RVec3 at, JPH::Vec3 velocity);
    [[nodiscard]] float support_y_for_test(JPH::RVec3 at, float depth) const noexcept;
    void launch_kit_body_for_test(std::uint32_t body, JPH::RVec3 at, JPH::Vec3 velocity);
    [[nodiscard]] Summit &summit() noexcept { return *summit_; }
    [[nodiscard]] const Summit &summit() const noexcept { return *summit_; }
    [[nodiscard]] const Trapeze &trapeze() const noexcept { return *trapeze_; }
    // The forklifts: the yard's first (forklift()), then the yard crane's on
    // its laydown, then those on the 11 m and 44 m decks it serves.
    [[nodiscard]] const Forklift &forklift() const noexcept { return *forklifts_.front(); }
    [[nodiscard]] int forklift_count() const noexcept { return static_cast<int>(forklifts_.size()); }
    [[nodiscard]] const Forklift &forklift(const int index) const noexcept {
        return *forklifts_[static_cast<std::size_t>(index)];
    }
    // The forklift being driven, else the one at whose wheel the player
    // stands, else -1.
    [[nodiscard]] int active_forklift() const noexcept;
    [[nodiscard]] const YardCrane &yard_crane() const noexcept { return *yard_crane_; }
    // The player could work the crane's remote now: hands free, on no hold,
    // in no seat and at no wheel, within its range.
    [[nodiscard]] bool yard_crane_able() const noexcept;
    // The crane's remote within the hands' reach in its cabinet.
    [[nodiscard]] bool yard_crane_remote_in_reach() const noexcept;
    [[nodiscard]] const Zipline &zipline() const noexcept { return *zipline_; }
    [[nodiscard]] const Corpses &corpses() const noexcept { return *corpses_; }
    [[nodiscard]] const LooseStock &loose_stock() const noexcept { return *loose_stock_; }
    [[nodiscard]] const ImpactLedger &impacts() const noexcept { return *impacts_; }
    // On a forklift's floor within reach of its wheel (that forklift's, or
    // any's).
    [[nodiscard]] bool at_forklift_controls(int index) const noexcept;
    [[nodiscard]] bool at_forklift_controls() const noexcept { return active_forklift() >= 0; }
    [[nodiscard]] const scraperx::sim::vertical::Route &vertical() const noexcept { return *vertical_; }
    [[nodiscard]] std::uint64_t standing_entity() const noexcept { return grounded_ ? support_entity_id_ : 0; }
    [[nodiscard]] bool slingshot_flight() const noexcept { return slingshot_flight_; }

    [[nodiscard]] std::vector<Vector3> slingshot_prediction() const;
    // The throw's path from the hands now (physics_world_probes.cpp).
    [[nodiscard]] std::vector<Vector3> throw_prediction() const;
    // The faint route hint (the owner's to-do 4): what the hands could take
    // ahead along the facing, standing: from the body and from points a
    // stride apart ahead of it, as far as the way there is clear, the lip a
    // mantle would take and the hold a hand would close round. The probes
    // the moves themselves use; read only.
    [[nodiscard]] std::vector<Vector3> route_ahead() const;

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

    // ---- live fabrication (sim/fabricated.hpp) ---------------------------
    // (Defined in physics_world_fabricated.cpp.)
    // A fabricated object as the world holds it: its entity, its kit body,
    // its hull file as it came (what a save carries) and as read, its shape.
    struct Fabricated final {
        std::uint64_t entity = 0;
        scraperx::sim::kit::BodyIndex body;
        std::vector<std::uint8_t> data;
        fabricated::Spec spec;
        JPH::RefConst<JPH::Shape> shape;
        float mass_kg = 0.0F;
    };
    // Simulation::add_fabricated: 0 and why when refused, the world unchanged.
    std::uint64_t add_fabricated(const std::uint8_t *data, std::size_t size, JPH::RVec3 at, float yaw,
                                 FabricatedRefusal &refusal);
    [[nodiscard]] const std::vector<Fabricated> &fabricated() const noexcept { return fabricated_; }
    [[nodiscard]] std::vector<Vector3> fabricated_hull_triangles(std::size_t index) const;
    [[nodiscard]] std::uint32_t bodies_with_entity_in(std::uint64_t first, std::uint64_t limit) const;

private:
    // The kit's bodies built with the world: every one before the first
    // fabricated object (those are always the kit's last).
    [[nodiscard]] std::size_t kit_built_count() const noexcept { return kit_->body_count() - fabricated_.size(); }
    // The body of a fabricated object, at `origin` turned `rotation`, as
    // loose stock: carried by the top of its box over its center of mass,
    // swept so a throw never passes through a rail, asleep once at rest,
    // heard when struck. Invalid when the physics has no room for a body.
    scraperx::sim::kit::BodyIndex build_fabricated(const Fabricated &object, JPH::RVec3 origin, JPH::Quat rotation);
    // Something solid where the shape would be put.
    [[nodiscard]] bool fabricated_overlaps(const JPH::Shape *shape, JPH::RVec3 origin, JPH::Quat rotation) const;
    // A save's fabricated objects (after its grips): each one's entity, hull
    // file and body state, read and bounded; false refuses the save.
    struct SavedFabricated final {
        Fabricated object;
        scraperx::sim::kit::Kit::BodyState state;
    };
    bool read_fabricated(SaveReader &r, std::vector<SavedFabricated> &out) const;
    void write_fabricated(SaveWriter &w) const;

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
    // The test-only extra solids whose band holds the box at `band_y`, or
    // every one still unbuilt when `band_y` is NaN (the end of the table), or
    // the at_end ones; each once.
    void build_test_extra_solids(JPH::BodyInterface &bodies, double band_y, bool at_end);

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
    // The springboard's own test, nothing begun: the top it would plant on.
    [[nodiscard]] LedgeProbe springboard_ahead(const JPH::BodyInterface &bodies,
                                               JPH::Vec3 relative_velocity) const noexcept;
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
    // center: forward along a horizontal bearing, a little above the center.
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
    void shove_player(JPH::BodyInterface &bodies, JPH::Vec3 direction) noexcept;

    [[nodiscard]] float carried_load_mass() const noexcept;
    // The throw (UpMove::Throw): a piece of the tower's loose stock in the
    // hands (route parts, a spool or a pin, are set down, never thrown away);
    // the velocity the arms would send it off at now; sending it.
    [[nodiscard]] bool holding_throwable() const noexcept;
    [[nodiscard]] JPH::Vec3 throw_velocity(const JPH::BodyInterface &bodies) const noexcept;
    void throw_carry(JPH::BodyInterface &bodies) noexcept;

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
    // The ground the hook rack is set for: the band from the rack to the
    // member over the well its hook first bites (kHookPracticeMember), 2 m
    // about. A post on it would take the rope's every sweep from the rack.
    [[nodiscard]] static JPH::AABox hook_practice_ground() noexcept;

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

    // request_up, made: up_move_ (chosen at the end of the last tick, the
    // state this tick starts from) turned into the request that makes it.
    void resolve_up(StepCommands &commands) noexcept;
    // (Defined in physics_world_state.cpp, after the affordances.)
    void update_up_move(const JPH::BodyInterface &bodies) noexcept;

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
    // A death: the body left where it died (Corpses), moving at velocity;
    // the blow that killed it, `impact_mps`, taken `side_on` (a hit, not a
    // fall onto the floor) or not.
    void leave_corpse(JPH::BodyInterface &bodies, JPH::Vec3 velocity, float impact_mps, bool side_on) noexcept;
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
    // try_top_out's own test, nothing begun: the top a climb would go over on to.
    [[nodiscard]] LedgeProbe top_out_ahead(const JPH::BodyInterface &bodies) const noexcept;

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
    // Tests only: see Simulation::TestExtraSolid, and which of them are built.
    const std::vector<Simulation::TestExtraSolid> test_extra_solids_;
    std::set<std::size_t> test_extra_built_;
    BroadPhaseLayerInterface broadphase_layer_interface_;
    ObjectVsBroadPhaseFilter object_vs_broadphase_filter_;
    ObjectLayerPairFilter object_layer_pair_filter_;
    JPH::PhysicsSystem physics_system_;
    PlayerContactListener contact_listener_;
    JPH::RefConst<JPH::Shape> player_shape_;
    JPH::RefConst<JPH::Shape> player_crouch_shape_;
    JPH::RefConst<JPH::Shape> player_torso_shape_;
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
    // Live fabrication: the objects made at run time, in the order they were
    // (their kit bodies the kit's last).
    std::vector<Fabricated> fabricated_;
    std::unique_ptr<Slingshot> slingshot_;
    std::unique_ptr<Swing> swing_;
    std::unique_ptr<Trapeze> trapeze_;
    std::unique_ptr<Summit> summit_;
    // The bay on the 154 m deck (bay.hpp): after every other kit body.
    std::unique_ptr<Bay> bay_;
    std::unique_ptr<LooseStock> loose_stock_;
    std::unique_ptr<ImpactLedger> impacts_;
    std::vector<std::unique_ptr<Forklift>> forklifts_;
    std::unique_ptr<YardCrane> yard_crane_;
    std::unique_ptr<Zipline> zipline_;
    // The bodies deaths left (Corpses); the latest death's place and speed.
    std::unique_ptr<Corpses> corpses_;
    JPH::RVec3 death_point_ = JPH::RVec3::sZero();
    float death_speed_mps_ = 0.0F;
    bool death_side_on_ = false;
    // Hits (see kHitFeltSpeed): this tick's hardest, those felt, the last.
    float hit_closing_mps_ = 0.0F;
    JPH::Vec3 hit_toward_ = JPH::Vec3::sZero();
    std::uint64_t hit_count_ = 0;
    float hit_speed_mps_ = 0.0F;
    // The swing under way hangs from a zipline's trolley (Zipline), riding it.
    bool zip_riding_ = false;
    // Rides ended by the stop's jolt tearing the hands off the bar.
    std::uint64_t zip_torn_count_ = 0;
    std::unique_ptr<Matron> matron_;
    traversal::AffordanceMap matron_affordances_;
    bool matron_affordances_scanned_ = false;
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
    std::uint64_t throw_count_ = 0;
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
    JPH::Vec3 carry_offset_last_{JPH::Vec3::sZero()};   // the hand point's offset last tick: its velocity
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
    // The grounded affordance is a vault (a landing beyond), not a mantle.
    bool affordance_vault_ = false;
    UpMove up_move_ = UpMove::None;
    UpMove up_resolved_ = UpMove::None;
    std::uint64_t up_count_ = 0;
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
    // The XCoM across a beam, as a fraction of the center of pressure's reach.
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
        Bay::State bay{};
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
    // Injuries (see kAnkleBreakSpeed): the limp's time left, every break.
    double limp_seconds_left_ = 0.0;
    std::uint64_t ankle_break_count_ = 0;
    [[nodiscard]] bool limping() const noexcept { return limp_seconds_left_ > 0.0; }
    // Injuries (see kArmBreakHitSpeed): the broken arm's time left, which it
    // is (0 the left, 1 the right), every break.
    double arm_broken_seconds_left_ = 0.0;
    int broken_arm_side_ = 1;
    std::uint64_t arm_break_count_ = 0;
    [[nodiscard]] bool arm_broken() const noexcept { return arm_broken_seconds_left_ > 0.0; }
    // Water (physics_world_water.cpp): measured each tick before the step.
    bool in_water_ = false;
    bool swimming_ = false;
    bool head_underwater_ = false;
    float water_submersion_ = 0.0F;
    float water_surface_y_ = 0.0F;
    int water_pool_ = -1;
    float water_entry_speed_mps_ = 0.0F;
    std::uint64_t water_entry_count_ = 0;
    std::uint64_t water_exit_count_ = 0;
    std::uint64_t swim_kick_count_ = 0;
    int swim_kick_cooldown_ticks_ = 0;
    void update_water(JPH::BodyInterface &bodies, float dt) noexcept;
    void swim_locomotion(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept;

    // Her hold on the player (physics_world_grab.cpp). Her hand is a body of
    // its own on her arm (a soft distance limit from her shoulder), pushed at
    // its pose by a capped spring-damper (its reaction on her); its grip on
    // the ankle a SwingTwist whose load and strain tear it. Held, the player's
    // body turns freely (all DOFs); let go, a capped PD torque rights it on
    // the ground and the DOFs lock again only once upright and clear.
    JPH::BodyID hand_id_;
    JPH::Ref<JPH::Shape> hand_shape_;
    JPH::Ref<JPH::TwoBodyConstraint> hand_arm_;
    JPH::Ref<JPH::TwoBodyConstraint> grip_;
    JPH::Ref<JPH::TwoBodyConstraint> brace_;   // the player's hands on a rail against her pull
    bool player_free_rot_ = false;
    bool grip_swing_ = false;   // the grip's hinge taken on (else a bare point)
    int upright_contact_ticks_ = 0;
    float upright_s_ = 0.0F;
    float grip_load_n_ = 0.0F;
    float grip_peak_n_ = 0.0F;
    float grip_strain_ns_ = 0.0F;
    int grip_over_ticks_ = 0;
    float brace_load_n_ = 0.0F;
    float brace_strain_ns_ = 0.0F;
    float thrash_ = 0.0F;
    float thrash_avg_ = 0.0F;
    double last_stick_x_ = 0.0, last_stick_z_ = 0.0;
    double kick_dir_x_ = 0.0, kick_dir_z_ = 0.0;
    int kick_cooldown_ticks_ = 0;
    bool kick_now_ = false;
    // This grip fought by the player (a kick, a thrash, a brace on a rail):
    // its tear is theirs, an escape; unfought, the world's (matron_hold.cpp).
    bool grip_fought_ = false;
    std::uint64_t grip_count_ = 0;
    std::uint64_t grip_tear_count_ = 0;
    std::uint64_t grip_release_count_ = 0;
    std::uint64_t brace_count_ = 0;
    std::uint64_t brace_tear_count_ = 0;
    std::uint64_t thrash_kick_count_ = 0;
    std::uint64_t upright_count_ = 0;
    std::uint64_t upright_wedged_count_ = 0;
    float max_hold_step_m_ = 0.0F;   // the player's biggest move in a tick while held (no teleport)
    JPH::RVec3 hold_last_at_ = JPH::RVec3::sZero();
    bool grab_holds_player() const noexcept { return grip_ != nullptr; }
    bool grab_pre_step(JPH::BodyInterface &bodies, StepCommands &commands, float dt) noexcept;
    // The player's ankle: the feet end of the body, however it lies.
    [[nodiscard]] JPH::RVec3 player_ankle(const JPH::BodyInterface &bodies) const noexcept;
    void grab_drive(JPH::BodyInterface &bodies, float dt) noexcept;
    void grab_post_step(JPH::BodyInterface &bodies, float dt) noexcept;
    // The player's strike (physics_world_strike.cpp).
    enum class StrikePhase : std::uint8_t { Guard = 0, Windup = 1, Reach = 2, Recoil = 3 };
    void strike_step(JPH::BodyInterface &bodies, const StepCommands &commands, float dt) noexcept;
    void read_strike_state() noexcept;
    void strike_reset() noexcept;
    // A punch under way: no other hand action starts till its recoil ends;
    // the world taking his hands from it (her fingers, a fall) ends it.
    void strike_owns_hands(StepCommands &commands) const noexcept;
    void strike_cancel() noexcept;
    // The fist he can throw with: 1 the right, 0 the left, -1 none (one torn
    // off, the other broken).
    [[nodiscard]] int strike_arm() const noexcept;
    [[nodiscard]] bool fist_usable(int side) const noexcept;
    [[nodiscard]] bool strike_would_land(JPH::RVec3 chest, JPH::Vec3 line, JPH::RVec3 c) const noexcept;
    int strike_side_ = 1;           // the fist this strike is thrown with
    bool strike_ready_ = false;     // one can be thrown now (the PUNCH button lit)
    [[nodiscard]] bool hands_on_a_machine() const noexcept;
    [[nodiscard]] bool fist_path_clear(JPH::RVec3 from, JPH::RVec3 to) const noexcept;
    bool strike_in_hand_ = false;   // this strike began with him in her hand
    StrikePhase strike_phase_ = StrikePhase::Guard;
    int strike_ticks_ = 0;
    int strike_contact_left_ = 0;
    bool strike_landed_ = false;
    bool strike_offered_ = false;
    std::uint64_t strike_count_ = 0;
    std::uint64_t strike_hit_count_ = 0;
    JPH::RVec3 strike_point_ = JPH::RVec3::sZero();
    JPH::Vec3 strike_normal_ = JPH::Vec3::sZero();
    float strike_impulse_ns_ = 0.0F;
    void grab_reset(JPH::BodyInterface &bodies) noexcept;
    void set_player_free_rotation(JPH::BodyInterface &bodies, bool free) noexcept;
    void destroy_grab_constraint(JPH::Ref<JPH::TwoBodyConstraint> &constraint) noexcept;
    void tear_grip(bool torn) noexcept;
    [[nodiscard]] bool upright_pose_clear(JPH::RVec3 centre) const noexcept;

    // Arms (physics_world_arm.cpp): lost per side (0 left, 1 right), until a
    // death or a restart; her second hand on a wrist; the severed arm a body.
    std::array<bool, 2> arm_lost_{false, false};
    JPH::BodyID hand2_id_;
    JPH::Ref<JPH::TwoBodyConstraint> hand2_arm_;
    JPH::Ref<JPH::TwoBodyConstraint> wrist_;
    JPH::Ref<JPH::TwoBodyConstraint> arm_in_hand_;
    JPH::BodyID severed_arm_id_;
    JPH::Ref<JPH::Shape> severed_arm_shape_;
    int wrist_side_ = -1;
    int last_severed_side_ = -1;
    float wrist_load_n_ = 0.0F;
    float wrist_strain_ns_ = 0.0F;
    float severed_arm_s_ = 0.0F;
    bool severed_arm_thrown_ = false;
    float severed_arm_thrown_y_ = 0.0F;
    std::uint64_t arm_tear_count_ = 0;
    std::uint64_t arm_throw_count_ = 0;
    std::uint64_t severed_arm_despawn_count_ = 0;
    float claimed_s_ = 0.0F;
    bool run_ended_by_matron_ = false;
    std::uint64_t claimed_count_ = 0;
    [[nodiscard]] int arms_lost() const noexcept { return int(arm_lost_[0]) + int(arm_lost_[1]); }
    // One good arm: one lost, or one broken (a hang holds half, a climb hauls
    // at half, no hand over hand).
    [[nodiscard]] bool one_arm() const noexcept { return arms_lost() == 1 || arm_broken(); }
    [[nodiscard]] JPH::Vec3 wrist_local(int side) const noexcept;
    void arm_drive(JPH::BodyInterface &bodies, float dt) noexcept;
    void arm_post_step(JPH::BodyInterface &bodies, float dt) noexcept;
    void arm_reset(JPH::BodyInterface &bodies) noexcept;
    void sever_arm(JPH::BodyInterface &bodies) noexcept;
    void despawn_severed_arm(JPH::BodyInterface &bodies) noexcept;
    // One good arm (physics_world_one_arm.cpp).
    bool dyno_active_ = false;
    bool dyno_no_catch_ = false;
    bool dyno_short_ = false;
    float dyno_t_ = 0.0F, dyno_apex_t_ = 0.0F, dyno_last_error_s_ = 0.0F, dyno_target_y_ = 0.0F;
    std::uint64_t dyno_launch_count_ = 0, dyno_catch_count_ = 0, dyno_miss_count_ = 0, dyno_refused_count_ = 0;
    std::uint64_t heel_hook_count_ = 0, heel_hook_refused_count_ = 0;
    [[nodiscard]] int good_arm_side() const noexcept;
    [[nodiscard]] JPH::Vec3 one_arm_shoulder_lean() const noexcept;
    bool one_arm_hang_command(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept;
    bool dyno_step(JPH::BodyInterface &bodies, const StepCommands &commands, float dt) noexcept;
    // The fatality (physics_world_fatality.cpp).
    enum class FistOn : std::uint8_t { None, Leg, Collar };
    JPH::BodyID knee_id_;
    JPH::Ref<JPH::TwoBodyConstraint> knee_arm_, knee_pin_, fist_, knot_, handle_fist_;
    std::array<JPH::Ref<JPH::TwoBodyConstraint>, 2> hip_{};
    std::array<JPH::BodyID, 2> leg_id_{};
    std::array<bool, 2> leg_off_{false, false};
    std::array<float, 2> leg_strain_ns_{0.0F, 0.0F};
    FistOn fist_on_ = FistOn::None;
    int leg_in_fist_ = -1;
    int fatal_prev_beat_ = -1;
    std::array<std::uint64_t, 10> fatality_beat_edges_{};
    float leg_load_n_ = 0.0F, leg_peak_n_ = 0.0F, last_leg_peak_n_ = 0.0F;
    float knot_load_n_ = 0.0F, knot_peak_n_ = 0.0F;
    float haul_from_y_ = 0.0F, haul_peak_y_ = 0.0F;
    bool haul_seen_ = false;
    float fall_from_y_ = 0.0F, fall_peak_speed_ = 0.0F;
    float landing_speed_mps_ = 0.0F, landing_drop_m_ = 0.0F;
    std::uint64_t knee_pin_count_ = 0, leg_tear_count_ = 0, leg_toss_count_ = 0, collar_count_ = 0;
    std::uint64_t unhook_count_ = 0, knot_count_ = 0, brake_kick_count_ = 0, haul_count_ = 0;
    std::uint64_t knot_part_count_ = 0, fall_landing_count_ = 0;
    [[nodiscard]] JPH::Vec3 ankle_local(int side) const noexcept;
    void set_player_mass_for_losses(JPH::BodyInterface &bodies) noexcept;
    void set_player_shape(JPH::BodyInterface &bodies, const JPH::Shape *shape) noexcept;
    void fatality_drive(JPH::BodyInterface &bodies, float dt) noexcept;
    [[nodiscard]] bool body_down_on_ground(JPH::BodyInterface &bodies) const noexcept;
    void fatality_post_step(JPH::BodyInterface &bodies, float dt) noexcept;
    void fatality_reset(JPH::BodyInterface &bodies) noexcept;
    JPH::BodyID make_limb_body(JPH::BodyInterface &bodies, JPH::RVec3 at, JPH::Quat rot, float half, float radius,
                               float kg, std::uint64_t entity, JPH::ObjectLayer layer);
public:
    [[nodiscard]] bool take_run_ended_by_matron() noexcept {
        const bool ended = run_ended_by_matron_;
        run_ended_by_matron_ = false;
        return ended;
    }
    void lose_arm_for_test(const int side) noexcept { arm_lost_[static_cast<std::size_t>(side & 1)] = true; }
    void break_arm_for_test(const int side) noexcept { break_arm(side & 1); }
    // Tests only: the player's body caught on steel where it is (a point at
    // its middle fixed to the world), till the load on it passes `max_n`.
    void snag_player_for_test(float max_n) noexcept;
    [[nodiscard]] bool player_snagged_for_test() const noexcept { return snag_ != nullptr; }
private:
    JPH::Ref<JPH::TwoBodyConstraint> snag_;   // snag_player_for_test
    float snag_max_n_ = 0.0F;
    // The arm on `side` breaks: one good arm (again) from kArmBrokenSeconds.
    void break_arm(int side) noexcept;
    // A landing at `normal_speed` into the ground breaks an ankle at
    // `break_speed` or more: the limp starts (again) from kLimpSeconds.
    void break_ankle_if(float normal_speed, float break_speed) noexcept;
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
    // A springboard's jump leaves the top at the end of a tick, the soles
    // kLandingSkin over it (inside the contacts' speculative reach): the
    // next tick is the jump's start, and like a jump's it reads no support.
    bool springboard_takeoff_ = false;
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
    // This tick's grip for the solve (PlayerContactListener::set_planted_grip):
    // set by apply_locomotion when the feet hold the body at rest, else 0.
    float planted_grip_ = 0.0F;
    bool teetering_ = false;
    bool summit_reached_ = false;
    double summit_seconds_ = 0.0;
    std::uint64_t summit_deaths_ = 0;
    std::uint64_t teeter_count_ = 0;

    Snapshot state_{};
};

} // namespace scraperx::sim
