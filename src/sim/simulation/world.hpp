#pragma once

#include "sim/simulation.hpp"

#include "sim/bands.hpp"
#include "sim/mechanism_kit.hpp"
#include "sim/pipe_bridge.hpp"
#include "sim/slingshot.hpp"
#include "sim/swing_stair.hpp"
#include "sim/upper_ascent.hpp"
#include "sim/teeter_rise.hpp"
#include "sim/braced_bay.hpp"
#include "sim/north_service_frame.hpp"
#include "sim/facade_route.hpp"
#include "sim/cargo_net_route.hpp"
#include "sim/catch_net_route.hpp"
#include "sim/split_inspection_route.hpp"
#include "sim/grapple_practice_route.hpp"
#include "sim/grappling_gun.hpp"
#include "sim/cargo_net.hpp"
#include "sim/physical_hand_climb.hpp"
#include "sim/physical_foot_push.hpp"
#include "sim/traversal_plan.hpp"
#include "sim/suspended_ladder.hpp"
#include "sim/service_lift.hpp"
#include "sim/supplied_ascent.hpp"
#include "sim/parkour_route.hpp"
#include "sim/deformable_plank.hpp"
#include "sim/water_screw.hpp"

#ifndef SCRAPERX_HAS_JOLT
#error "WO-003 requires the pinned Jolt physics substrate"
#endif

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/PulleyConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/SoftBody/SoftBodyContactListener.h>
#include <Jolt/Physics/SoftBody/SoftBodyManifold.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>
#include <cmath>
#include <limits>
#include <mutex>
#include <utility>

#include "sim/simulation/constants.hpp"
#include "sim/simulation/fixture_constants.hpp"
#include "sim/simulation/runtime.hpp"
#include "sim/simulation/contacts.hpp"
#include "sim/simulation/queries.hpp"

namespace scraperx::sim {
using namespace detail;

// Private shared world declaration. State is owned here once; implementation
// modules never create parallel controllers or copy solver/contact state.
class Simulation::PhysicsWorld final : private JPH::PhysicsStepListener {
    struct MachineCheckpoint;
    // Published by the simulation owner before Update and read-only throughout
    // native soft-contact jobs. The context lives until both nets are destroyed.
    struct SoftFootPolicy final {
        JPH::BodyID player;
        float half_height = kPlayerHalfHeight;
        bool passive = false;
    };

public:
    explicit PhysicsWorld(const InitialSpawn initial_spawn, const WorldContent content);

    ~PhysicsWorld();

    struct StepCommands final {
        double move_input_x = 0.0;
        double move_input_z = 0.0;
        double facing_x = 0.0;
        double facing_z = 0.0;
        bool jump_requested = false;
        bool cancel_jump_requested = false;
        bool traversal_requested = false;
        bool release_requested = false;
        bool crouch_held = false;
        bool sprint_held = false;
        bool pick_up_requested = false;
        bool set_down_requested = false;
        bool rig_requested = false;
        bool parachute_toggle_requested = false;
        double jib_slew_input = 0.0;
        double jib_hoist_input = 0.0;
        double needle_hoist_input = 0.0;
        double service_lift_input = 0.0;
        double supplied_machine_input = 0.0;
        bool valve_toggle_requested = false;
        bool water_screw_toggle_requested = false;
        bool water_lift_valve_toggle_requested = false;
        bool water_lift_release_requested = false;
        bool water_lift_reset_requested = false;
        double intake_slew_input = 0.0;
        double intake_hoist_input = 0.0;
        bool intake_sling_release_requested = false;
        bool intake_sling_attach_requested = false;
        double sling_draw = 0.0, sling_yaw = 0.0, sling_elevation = 1.4311699866353502;
        bool sling_action = false, sling_drop = false;
        Vector3 grapple_aim{0, 0, -1};
        double grapple_reel = 0;
        bool grapple_equip_requested = false, grapple_equipped = false;
        bool grapple_action = false, grapple_release = false;
        bool grapple_auto_reel_requested = false, grapple_auto_reel = false;
    };

    static bool nearby_grapple_hook(const GrapplingGun::Readback &state);
    [[nodiscard]] GrappleGunSnapshot grapple_gun_state() const noexcept;

    void apply_grapple_commands(const StepCommands &commands);

    void OnStep(const JPH::PhysicsStepListenerContext &context) override;

    void step(const StepCommands &commands,
              const float delta_seconds,
              const double next_time_seconds) noexcept;

    [[nodiscard]] std::uint32_t entity_body_count(const std::uint64_t entity) const noexcept;

    [[nodiscard]] std::uint32_t moving_body_count() const noexcept;

    [[nodiscard]] const CargoNet *cargo_net() const noexcept;
    [[nodiscard]] const CargoNet *catch_net() const noexcept;
    [[nodiscard]] CatchNetSnapshot catch_net_state() const noexcept;

    [[nodiscard]] RenderHandAnchor render_hand_anchor(const std::uint32_t hand) const noexcept;

    [[nodiscard]] const Snapshot &state() const noexcept;

    bool debug_restart_at(const Vector3 centre) noexcept;

    bool select_service_lift_retry() noexcept;

    [[nodiscard]] bool can_restart_gravity_reclaim_attempt() const noexcept;

    bool select_gravity_reclaim_retry() noexcept;

    bool restart_checkpoint() noexcept;

    [[nodiscard]] bool traversal_committed() const noexcept;

    [[nodiscard]] const scraperx::sim::kit::Kit &kit() const noexcept;
    [[nodiscard]] bool inspection_span_seat_contact() const noexcept;
    [[nodiscard]] const PipeBridge *pipe_bridge() const noexcept;
    [[nodiscard]] const Slingshot *slingshot() const noexcept;
    [[nodiscard]] Vector3 rider_specific_acceleration() const noexcept;

    [[nodiscard]] std::vector<Vector3> slingshot_prediction() const;

    void set_feed_enabled(const bool enabled) noexcept;
    void set_water_screw_motor_torque_limit_nm(const double torque_nm) noexcept;
    void set_water_screw_outlet_blocked(const bool blocked) noexcept;
    void set_water_screw_drive_direction(const int direction) noexcept;
    void set_water_screw_basin_volume_m3(const double volume_m3) noexcept;
    void set_water_screw_tank_volume_m3_for_proof(const double volume_m3) noexcept;
    void set_water_lift_rope_connected(const bool connected) noexcept;
    void set_water_lift_cage_mass_kg(const double mass_kg) noexcept;

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

    // ---- coupled machine ------------------------------------------------
    //
    // One causal chain, all of it authoritative:
    //   hoist scoop lifts ballast -> tips it onto a chute -> ballast falls onto a
    //   hinged tipper -> tipper rotation drags a tension-only rope -> rope pulls a
    //   counterweighted valve lever -> lever angle sets a real orifice area ->
    //   orifice vents a finite pressure vessel into an actuator cylinder ->
    //   cylinder pressure pushes a piston -> piston lifts a counterweighted
    //   platform the player can stand on and ride.
    //
    // Nothing in the chain is scripted. Each link reads the previous link's
    // actual body state, so the player can enter it mid-cycle, block it, ride it,
    // or start it early by standing on the tipper.
    void build_machine(JPH::BodyInterface &bodies);

    // WO-011. Ascent Atlas v1.0 kernel (section 9): KX-JIB + KX-CRATE. A
    // pendant-controlled crane, not an autonomous cycle -- everything here
    // moves only in response to a real command, through a real, finite-force
    // Jolt constraint motor, never a scripted animation or a teleport.
    void build_kernel_jib(JPH::BodyInterface &bodies);

    // WO-012. Ascent Atlas v1.0 kernel (section 9): KX-NEEDLE + KX-POCKETS. A
    // second, minimal jib-pattern hoist -- mast plus one finite-force
    // vertical motor, no slew -- lowers the beam on a fixed vertical line
    // directly above the gap. Two piers stand in for the atlas's KX-POCKETS;
    // seating pins them to the beam at runtime (update_needle), never here.
    void build_kernel_needle(JPH::BodyInterface &bodies);

    // The stack: the tower's climbable lower section, as real static
    // collision. A perimeter deck ring per level around an open central
    // shaft, corner and mid-span columns carrying each deck, and a stair
    // ramp per level alternating sides so the ascent spirals. The player is
    // inside this, not looking at it.
    // Everything the player can see is something the player can hit. The
    // table is generated from the Godot builders (godot/presentation/
    // solid_export.gd), so the world these tests walk is the world that is
    // drawn. Built last, so a drawn mirror of a body already owned here is
    // recognised by its bounds and skipped rather than doubled -- a second
    // coincident body would split ledge probes and support identity.
    void build_world_solids(JPH::BodyInterface &bodies);

    [[nodiscard]] bool mirrors_owned_body(const JPH::AABox &bounds) const;

    void build_stack(JPH::BodyInterface &bodies);

    // WO-013. Ascent Atlas v1.0 kernel (section 9): KX-SUMP + KX-GRATE. Fixed
    // approach/far decking flank one grate panel; only the grate's own
    // collidability changes, driven by update_sump every tick. Starts wet
    // (grate is a sensor -- see create) since the sump starts full.
    // AS-001. Ascent Atlas §6 band B00: MOD-APRON, MOD-INTAKE-BELT,
    // CAP-PENDANT, MOD-YARD-JIB, the 4 t pack, MOD-DOG-A, MOD-STAIR-A,
    // MOD-SKIN-LADDER-S. The ground plane already reaches here, so MOD-APRON
    // is the yard furniture standing on it rather than a second slab.
    void build_intake_rise(JPH::BodyInterface &bodies);

    // AS-002. Ascent Atlas §6 band B00's 24.1872-40.1872 m leftover, chain K0
    // PLAY. Everything above the handoff deck stacks off AS-001's own
    // tread-surface offset (kIntakeHandoffY + slab-half-thickness / cos(pitch)),
    // recomputed here at this run's own 30 deg pitch, so it stays flush with
    // the deck below to the millimetre rather than by matching a second copy
    // of the same literal.

    void build_water_screw(JPH::BodyInterface &bodies);

    void build_legal_forty(JPH::BodyInterface &bodies);

    // ---- AS-003 MOD-HOOK5-RACK ------------------------------------------------
    // A cage locked by its height, a door held shut by a bar lying in its
    // swing, and the hook block on a rack inside. No lock object, no flag:
    // see the kHook5* constants for every clearance.
    void build_hook5_rack(JPH::BodyInterface &bodies);

    void build_kernel_sump(JPH::BodyInterface &bodies);

    // Lets the linkage reach its own resting pose before the rope is measured, so
    // slack is slack against the machine as it actually hangs.
    void settle_machine() noexcept;

    void add_hinge(const JPH::BodyID anchor_id,
                   const JPH::BodyID moving_id,
                   const JPH::RVec3 point,
                   const float limit_min,
                   const float limit_max,
                   JPH::Ref<JPH::HingeConstraint> *out,
                   const std::uint64_t);

    void add_slider(const JPH::BodyID anchor_id,
                    const JPH::BodyID moving_id,
                    const float limit_min,
                    const float limit_max);

    // WO-011. A vertical-axis hinge with a real, torque-limited Jolt motor --
    // the jib's slew. EMotorState::Velocity drives toward a commanded angular
    // velocity "limited only by max force/torque the motor can apply" (Jolt's
    // own doc comment on EMotorState): exceeding that torque does not snap to
    // the target, the body simply cannot reach it. That is the finite-actuator
    // requirement (WO-005 forbidden shortcuts: "unlimited winch force"),
    // enforced by the engine's own constraint solver, not by application code.
    void add_vertical_hinge(const JPH::BodyID anchor_id,
                            const JPH::BodyID moving_id,
                            const JPH::RVec3 point,
                            const float limit_min,
                            const float limit_max,
                            const float max_motor_torque_nm,
                            JPH::Ref<JPH::HingeConstraint> *out,
                            const JPH::Vec3 axis = JPH::Vec3::sAxisY());

    // A vertical slider with a real, force-limited motor -- the jib's hoist
    // winch, and the capacity-proving stand that shares its rating. Same
    // honesty property as the slew motor: EMotorState::Velocity can only push
    // as hard as mMaxForceLimit, so an overweight load is not held, it sags or
    // falls at a rate the deficit between weight and rated force actually
    // produces -- not scripted, read back from the solver.
    void add_motorized_slider(const JPH::BodyID anchor_id,
                              const JPH::BodyID moving_id,
                              const float limit_min,
                              const float limit_max,
                              const float max_motor_force_n,
                              JPH::Ref<JPH::SliderConstraint> *out);

    // A real pin between two bodies at one shared world point -- the hook-to-
    // crate rigging. WO-005 allows the attachment pre-placed for this WO
    // ("CAP-HOOK5 may be pre-placed on the crate... but the hook must still be
    // a real constraint"); a PointConstraint fixes the pin but leaves rotation
    // free, so the crate genuinely swings under the hook rather than being
    // welded to it.
    void add_point_link(const JPH::BodyID first_id,
                        const JPH::BodyID second_id,
                        const JPH::RVec3 point);

    // Tension-only rope: it can pull the valve lever but never push it, and it
    // carries deliberate slack so the valve opens a beat after the strike.
    void add_rope();

    // WO-010 control cable. A real two-sheave run: pressing the treadle pays out
    // cable on the catwalk side, which must be taken up on the valve side, so the
    // valve lever's counterweight end is hauled up and the orifice opens. Like
    // every rope here it can only pull -- when the player steps off, the treadle
    // is returned by its own counterweight and the valve by its own, not by the
    // cable pushing anything.
    void add_treadle_cable();

    // A tension-only rope over two sheaves: mMinLength=0 lets it go fully
    // slack; mMaxLength caps it (Jolt auto-derives a negative mMaxLength from
    // the configuration's length AT CONSTRUCTION -- PulleyConstraint.cpp's own
    // "Calculate min/max length if it was not provided" -- so an explicit,
    // computed mMaxLength is what makes this a real inextensible rope rather
    // than one silently locked to whatever pose happened to be built).
    // Generalised (AS-002) to take its own four points, bodies, ratio and
    // length, so every rope in this file -- the Kellerworks lift and AS-002's
    // counterweight cradle alike -- is one real JPH::PulleyConstraint, not a
    // copy of this function.
    JPH::PulleyConstraint *add_pulley(const JPH::RVec3 body_point_1,
                                      const JPH::RVec3 fixed_point_1,
                                      const JPH::BodyID body_1,
                                      const JPH::RVec3 body_point_2,
                                      const JPH::RVec3 fixed_point_2,
                                      const JPH::BodyID body_2,
                                      const float ratio,
                                      const float min_length,
                                      const float max_length);

    // track_for_teardown=false hands ownership entirely to the caller (WO-012
    // needle pins, created and removed at runtime as the seat predicate
    // changes): Jolt's ConstraintManager::Remove asserts on an already-
    // invalidated constraint index, so a constraint that might be removed
    // before the destructor runs must never also sit in machine_constraints_,
    // which is unconditionally removed there once. Exactly one owner removes
    // it, on every path.
    [[nodiscard]] JPH::TwoBodyConstraint *create_constraint(
        const JPH::TwoBodyConstraintSettings &settings,
        const JPH::BodyID first,
        const JPH::BodyID second,
        const bool track_for_teardown = true);

    // Kinematic skip-hoist cycle. Every phase is a function of authoritative
    // simulation time, never wall clock, so the loop is deterministic and can be
    // walked into at any point.
    void update_scoop(JPH::BodyInterface &bodies,
                      const float delta_seconds,
                      const double next_time_seconds) noexcept;

    // Reads the real valve lever angle, advances the plant on the same fixed
    // step, and pushes the piston. Presentation never touches any of this.
    void update_plant(JPH::BodyInterface &bodies, const float delta_seconds) noexcept;

    // WO-011 KX-JIB. Commands take effect only within the pendant station
    // radius (WO-005: "Action to enter station"). Away from it, queued input
    // is discarded: slew requests zero velocity and the hoist engages its
    // rated powered position hold at the actual departure extension.
    void update_jib(const JPH::BodyInterface &bodies,
                    const double slew_input,
                    const double hoist_input) noexcept;

    // WO-012 KX-NEEDLE. Same station-gated, continuous-axis contract as
    // update_jib. Seating and unseating are real topology changes -- two
    // PointConstraint pockets added or removed at runtime -- driven entirely
    // by this tick's measured position/speed or command, never a flag.
    void update_needle(const JPH::BodyInterface &bodies, const double hoist_input) noexcept;

    // Pins the beam into both piers at the fixed pocket points. The hoist
    // slider is left connected (Governing Law 26 sidestep: no attach/detach
    // system to invent -- see WO-012's design notes), which over-constrains
    // the beam slightly but consistently, since the slider's own rest point
    // already coincides exactly with these pocket points.
    void seat_needle() noexcept;

    // Removes both pocket pins. The beam is still hoist-connected, so it does
    // not fall -- the same motor that lowered it now lifts it clear on the
    // next sustained raise, exactly reversing how it was seated.
    void unseat_needle() noexcept;

    // WO-012 checkpoint topology reconciliation: restore_from_checkpoint
    // already restores the beam's continuous transform via restore_body
    // (called just before this); this reconciles the discrete seated/unseated
    // state to match what was actually committed, rather than leaving
    // whatever pins happened to exist at the moment of death. Forbidden-
    // shortcuts list (WO-006): "resetting seat on play-mode restart without
    // going through checkpoint rules" -- this is that checkpoint rule.
    void restore_needle_topology(const bool checkpoint_seated) noexcept;

    // WO-013 KX-SUMP. One lumped volume, one isolation edge, one drain sink,
    // one derived predicate -- updated every authoritative tick, same as
    // every other machine link in this file. The valve toggle is a one-shot
    // Action (WO-006 text: "Player Action may... close a valve only at the
    // real station"), gated by station radius exactly like the jib/needle
    // pendants, but flips a binary state rather than driving a motor.
    // AS-001 MOD-YARD-JIB / MOD-DOG-A. Same station-gated, continuous-axis
    // contract as update_jib. The dog is deliberately absent from the command
    // path: its motor was commanded open at build time and is never touched
    // here, so the only thing that can change the throat is the pack moving.

    void update_water_screw(const JPH::BodyInterface &bodies,
                            const double delta_seconds,
                            const bool toggle_requested) noexcept;


    void update_water_lift(const JPH::BodyInterface &bodies,
                           const double delta_seconds,
                           const bool valve_toggle_requested,
                           const bool release_requested,
                           const bool reset_requested) noexcept;

    void update_intake(const JPH::BodyInterface &bodies,
                       const double slew_input,
                       const double hoist_input) noexcept;

    // AS-002 MOD-STAIR-A-SWING / MOD-CW-CRADLE. Reads the hinge every tick
    // regardless of station (the flight keeps swinging under gravity/rope
    // tension whether or not the player is anywhere near the pendant); the
    // sling commands are gated on intake_station_active_, which update_intake
    // -- called immediately before this every tick -- has just refreshed.
    // Release and Attach are one-shot, exactly like the sump's valve toggle,
    // gated additionally on physical tolerance: a command outside tolerance
    // is accepted but produces no state change, rather than a pose-only
    // predicate silently re-firing the instant geometry happens to line up
    // -- WO-012's seat_needle/unseat_needle race, generalised in advance.
    void update_legal_forty(const JPH::BodyInterface &bodies,
                            const bool release_requested,
                            const bool attach_requested) noexcept;

    // Shared by release_pack_to_cradle (may release fire) and
    // update_legal_forty's retract nudge (is the cradle genuinely unloaded)
    // -- the same seated-pose tolerance, asked from two directions.
    bool intake_pack_seated_on_cradle(const JPH::BodyInterface &bodies) const noexcept;

    // Removes the hook-pack sling once the pack is physically seated on
    // MOD-CW-CRADLE and settled. The rope-tension jump this causes -- read
    // back from the solver on the next tick, never asserted here -- is the
    // whole mechanism; this function's only job is deciding whether a real
    // release may happen right now.
    void release_pack_to_cradle(const JPH::BodyInterface &bodies) noexcept;

    // Re-creates the hook-pack sling once the hook is physically back over
    // the pack's padeye and settled. Exact mirror of release_pack_to_cradle.
    void sling_pack(const JPH::BodyInterface &bodies) noexcept;

    // Raw, unconditional creation/removal -- shared by the tolerance-gated
    // sling_pack()/release_pack_to_cradle() command path, the initial build
    // in build_intake_rise, and restore_legal_forty_topology()'s checkpoint
    // reconciliation, which must NOT re-apply the tolerance gate against
    // bodies restore_from_checkpoint does not reposition (the pack/hook/dog
    // are an AS-001 gap this ticket does not reopen -- see MachineCheckpoint).
    void create_sling_pin(const JPH::RVec3 point) noexcept;

    void remove_sling_pin() noexcept;

    // WO-012 checkpoint topology reconciliation, generalised: restore the
    // discrete sling presence to match what was actually committed, rather
    // than leaving whatever pin happened to exist at the moment of death.
    void restore_legal_forty_topology(const bool checkpoint_slung) noexcept;

    void update_sump(JPH::BodyInterface &bodies, const float delta_seconds,
                     const bool valve_toggle_requested) noexcept;

    [[nodiscard]] CargoNet *net_for_body(JPH::BodyID id) const noexcept;
    [[nodiscard]] JPH::Vec3 support_point_velocity(const JPH::BodyInterface &bodies,
        JPH::BodyID id, JPH::RVec3 point) const noexcept;
    [[nodiscard]] JPH::Vec3 support_material(const CargoNet &net, JPH::BodyID id,
        JPH::RVec3 point) const noexcept;
    [[nodiscard]] JPH::Vec3 support_traction_normal() const noexcept;
    void clear_catch_history() noexcept;
    void refresh_catch_geometry() noexcept;
    [[nodiscard]] static bool contact_on_bottom_capsule_cap(const JPH::Vec3 delta,
                                                           const float half) noexcept;
    [[nodiscard]] bool passive_soft_feet_requested(const StepCommands &commands) const noexcept;
    void publish_soft_foot_policy(const StepCommands &commands) noexcept;
    void reset_soft_foot_policy() noexcept;
    [[nodiscard]] static float soft_foot_contact_friction(void *context, const JPH::BodyID rigid,
        const JPH::Vec3 rigid_local_contact, const JPH::Vec3 world_normal_toward_rigid,
        const float original_friction) noexcept;
    [[nodiscard]] SupportSample observe_soft_support(const JPH::BodyInterface &bodies) const;
    void measure_catch_load(JPH::Vec3 velocity_before, const float dt,
        const JPH::BodyInterface &bodies, const SupportSample &soft_support) noexcept;

    [[nodiscard]] JPH::Vec3 current_support_point_velocity(
        const JPH::BodyInterface &bodies) const noexcept;

    void update_support_motion(JPH::BodyInterface &bodies,
                               const float delta_seconds,
                               const double next_time_seconds) noexcept;

    // ---- geometry probes -------------------------------------------------

    [[nodiscard]] bool cast_ray(const JPH::RVec3 origin,
                                const JPH::Vec3 direction,
                                JPH::RayCastResult &hit,
                                const bool see_past_soft_holds = false) const noexcept;

    // A ray for a wall. With see_past_holds, members a hand could close
    // round (rungs, pipes, bars) are seen through, the way a climber on a
    // ladder sees the wall behind it.
    [[nodiscard]] bool cast_wall(const JPH::RVec3 origin, const JPH::Vec3 direction,
                                 const bool see_past_holds, JPH::RayCastResult &hit,
                                 JPH::RVec3 &point) const noexcept;

    [[nodiscard]] bool is_hold(const JPH::BodyID body_id,
                               const JPH::SubShapeID &sub_shape_id) const noexcept;

    [[nodiscard]] JPH::Vec3 surface_normal(const JPH::BodyID body_id,
                                           const JPH::SubShapeID &sub_shape_id,
                                           const JPH::RVec3 point) const noexcept;

    // A checkpoint must restore to a resting capsule on a small support
    // patch. One centre ray also sees lower decking while the capsule is
    // perched on a higher rounded edge: that state slowly slides off again
    // after restore. Require the actual capsule/plane rest height and a
    // cross of nearby points on that plane. The carried body is not footing.
    [[nodiscard]] bool footing_is_firm(const JPH::BodyInterface &bodies) const noexcept;

    // The capsule the body has now: standing, or crouched.
    [[nodiscard]] const JPH::Shape *active_player_shape() const noexcept;

    [[nodiscard]] bool capsule_pose_is_clear(const JPH::RVec3 centre) const noexcept;

    [[nodiscard]] bool shape_pose_is_clear(const JPH::Shape *shape,
                                           const JPH::RVec3 centre,
                                           const float contact_slop = 0.0F) const noexcept;

    // Finds a ledge in front of `origin`. Every returned field comes from a real
    // cast against the authoritative world; a failed reach, a missing top
    // surface, a too-steep top, an out-of-band rise, an unsupported landing, or
    // an obstructed landing pose all return an invalid probe.
    [[nodiscard]] LedgeProbe probe_ledge(const JPH::RVec3 origin,
                                         const JPH::Vec3 facing,
                                         const float minimum_rise,
                                         const float maximum_rise,
                                         const bool require_supported_landing,
                                         const bool see_past_holds = false,
                                         const bool require_clear_landing = true) const noexcept;

    // Shared wall-to-top/receiver validation. Hanging supplies a swept wall
    // seed, but rise is still measured from the original player centre.
    [[nodiscard]] LedgeProbe probe_ledge_from_wall(
        const JPH::RVec3 origin, const JPH::Vec3 facing,
        const float minimum_rise, const float maximum_rise,
        const bool require_supported_landing, const bool require_clear_landing,
        const JPH::RayCastResult &wall_hit, const JPH::RVec3 wall_point,
        const bool require_same_sub_shape = false) const noexcept;

    // Finds the far-side landing that makes an obstacle vaultable rather than
    // mantleable. Without a real, clear landing beyond the obstacle there is no
    // vault.
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
    [[nodiscard]] static JPH::Vec3 carry_hand_offset(const JPH::Vec3 bearing) noexcept;

    // The bearing the hands hold the load on this tick: the load's present
    // bearing from the body, turned toward the facing by at most
    // kCarryTurnRadiansPerSecond. Derived from poses, so it adds no state.
    [[nodiscard]] JPH::Vec3 carry_hand_bearing(const JPH::BodyInterface &bodies,
                                               const float delta_seconds) const noexcept;

    // The middle of the carried body's top face, in its own frame; a kit
    // body's handle is where its build put it.
    [[nodiscard]] JPH::Vec3 carry_handle(const std::uint64_t entity) const noexcept;

    // What a pick-up would take now: the nearest carryable within reach of the
    // body's centre, at rest, not behind it -- and only for a grounded body
    // outside a traversal with its hands free.
    JPH::BodyID carry_candidate(const JPH::BodyInterface &bodies,
                                std::uint64_t &entity) const noexcept;

    // A point constraint between the hands and the body's handle. Rotation is
    // left free, so a block hangs and swings from it.
    void attach_carry(const JPH::BodyID id, const std::uint64_t entity) noexcept;

    // Removes the constraint and nothing else: the body falls and rests.
    void release_carry() noexcept;

    void update_carry(JPH::BodyInterface &bodies, const StepCommands &commands,
                      const float delta_seconds) noexcept;

    // Checkpoint reconciliation, the restore_needle_topology shape: after the
    // bodies' poses are restored, make the carry match what was committed.
    void restore_carry_topology(const std::uint64_t committed_entity) noexcept;

    // ---- AS-006 rigging ------------------------------------------------------
    // Where the hands are this tick: the carry point, whether or not anything
    // is held.
    [[nodiscard]] JPH::RVec3 hand_position(const JPH::BodyInterface &bodies) const noexcept;

    // What the rig command would do now, from poses and the carry alone.
    void find_rig_action(const JPH::BodyInterface &bodies) noexcept;

    // Hook: the shackle leaves the hands and the rope's end goes onto the
    // anchor. Unhook: the end comes off the anchor onto its shackle, in the
    // hands.
    void update_rig(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept;

    // Crouch and stand (GDD 7.2, Governing Law 4). The body swaps capsules
    // with its soles fixed: the centre moves by kCrouchDrop, nothing under
    // the feet does. It crouches only from the ground and never inside a
    // traversal (every traversal pose is a standing pose), and it stands
    // only where the full standing capsule is clear, so a gap entered
    // crouched keeps the body crouched until it is out of it. A Jump or a
    // traversal request asks to stand first; where the body cannot, they
    // are refused further down this tick.
    void update_crouch(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept;

    [[nodiscard]] bool try_stand(JPH::BodyInterface &bodies) noexcept;

    void apply_traversal_commands(JPH::BodyInterface &bodies,
                                  const StepCommands &commands) noexcept;

    // The vault the takeoff could have been: probed from the takeoff floor at
    // the body's current plan position, with the same rise band and the same
    // far-side landing test as a vault started on the ground.
    [[nodiscard]] bool try_begin_jump_vault(JPH::BodyInterface &bodies) noexcept;

    void clear_landing_recovery() noexcept;

    void record_landing(const JPH::BodyInterface &bodies, const SupportSample &support,
                        const bool prepared) noexcept;

    void apply_ground_contact_force(JPH::BodyInterface &bodies, JPH::Vec3 correction,
                                    const float dt) noexcept;

    void apply_landing_recovery(JPH::BodyInterface &bodies, const StepCommands &commands,
                                const float dt) noexcept;

    [[nodiscard]] bool sprint_is_eligible(const StepCommands &commands) const noexcept;

    // Reduced-order ordinary push-off: finite impulse and positive work, with
    // an actual reaction receiver. CHOSEN 3 kJ burst capacity preserves the
    // static 5.5 m/s baseline while including movable receiver recoil. This is
    // not an anatomical stance-force profile or complete solver energy ledger.
    [[nodiscard]] bool push_off_ordinary_support(JPH::BodyInterface &bodies) noexcept;

    [[nodiscard]] bool apply_locomotion(JPH::BodyInterface &bodies,
                                        const StepCommands &commands,
                                        const float delta_seconds) noexcept;

    // The capsule swept from `from` by `displacement`: true on a hit, with
    // the hit fraction and the surface normal (pointing out of what was hit).
    [[nodiscard]] bool cast_capsule(const JPH::RVec3 from, const JPH::Vec3 displacement,
                                    float &fraction, JPH::Vec3 &normal, JPH::BodyID *hit_body=nullptr,
                                    JPH::SubShapeID *hit_shape=nullptr) const;

    // Stepping up. Blocked at the feet, clear with the body raised by the
    // maximum step, and walkable support found by sweeping back down: then
    // unconverted surfaces retain the existing character step. Converted
    // members request bounded reciprocal leg work from actual foot contact;
    // their rise is resolved by gravity and collision, never a pose write.
    bool try_step_up(JPH::BodyInterface &bodies, const JPH::Vec3 relative_velocity,
                     const float delta_seconds, const bool deliberate_step = false);

    // Real quadratic drag opposing the full velocity vector, not a clamp: it
    // can only ever pull speed toward the terminal value, never accelerate
    // the player upward past what deceleration implies (Governing Law 7 --
    // no powered ascent). Applied on top of ordinary air control, so existing
    // horizontal steering doubles as the "redirection" the same law permits.
    void apply_parachute_drag(JPH::BodyInterface &bodies, const float delta_seconds) noexcept;

    [[nodiscard]] bool probe_lip_pair(JPH::RVec3 ledge,JPH::Vec3 inward,JPH::BodyID owner,
                                      Lip (&lips)[2]) const noexcept;

    [[nodiscard]] static JPH::RVec3 hang_hold(const LedgeProbe &probe) noexcept;

    [[nodiscard]] bool hang_ledge_is_reachable(const JPH::RVec3 origin,
                                               const JPH::Vec3 facing,
                                               const LedgeProbe &probe) const noexcept;

    // Only airborne hanging uses this continuous chest-to-overhead search.
    // Sweep hits seed the existing validator; none grants a grip by itself.
    [[nodiscard]] LedgeProbe probe_hang_ledge(const JPH::RVec3 origin,
                                              const JPH::Vec3 facing) const noexcept;

    [[nodiscard]] bool acquire_hang_hands(JPH::BodyInterface &bodies) noexcept;

    void try_begin_hang(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept;

    [[nodiscard]] bool try_begin_ground_traversal(JPH::BodyInterface &bodies) noexcept;

    // Real support under the capsule's axis at `centre`: walkable ground no
    // deeper than the step's own lift plus the landing tolerance.
    [[nodiscard]] bool step_is_supported(const JPH::RVec3 centre) const noexcept;

    // Where a standing mantle's step-in ends (see kMantleApproachStandoff).
    // Returns `origin` itself when no step is possible.
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

    void release_hang(JPH::BodyInterface &bodies) noexcept;


    // ---- AS-017 traversal adapted from ScraperX-Claude f872c41 --------------------

    // A hang or climb on a dynamic body hangs the climber's weight on it at
    // the hold: the structure feels who is on it. Static and kinematic
    // bodies take no force.
    void load_hold(JPH::BodyInterface &bodies) noexcept;

    // Horizontal right of the structure a hang or climb faces.
    [[nodiscard]] JPH::Vec3 traversal_right() const noexcept;

    [[nodiscard]] bool grip_approach_visible(const JPH::RVec3 centre,
                                             const JPH::RVec3 aim,
                                             const JPH::Vec3 facing,
                                             const Grip &grip) const noexcept;

    // The hold nearest `aim` that a hand in front of `centre` can close
    // round: a box whose two thinner dimensions are at most kGripMaxSection
    // and whose length is at least kGripMinLength, searched in a
    // kGripSearchHalf box round `aim` squared to `facing`. Rigid and material
    // holds need an unobstructed approach from the real player: exact rigid
    // leaf or a nearby actual net mesh hit. Rigid points lie on the member's
    // axis (or a supported timber cell's edge).
    [[nodiscard]] Grip find_grip(const JPH::RVec3 centre, const JPH::RVec3 aim,
                                 const JPH::Vec3 facing, JPH::BodyID excluded = {}) const noexcept;

    // Where a hand in front of `centre` aims: `height` over the centre,
    // kClimbHandReach toward the structure, `across` to its right.
    [[nodiscard]] JPH::RVec3 hand_aim(const JPH::RVec3 centre, const float height,
                                      const float across) const noexcept;

    [[nodiscard]] JPH::RVec3 hand_point(const JPH::BodyInterface &bodies,
                                        const std::uint32_t hand) const noexcept;

    [[nodiscard]] bool grip_matches_climb_backend(const Grip &grip) const noexcept;

    void set_hand(const JPH::BodyInterface &bodies, const std::uint32_t hand,
                  const Grip &grip) noexcept;

    // Both hands onto the holds nearest their rest aims, left a little lower
    // than right; a hand that finds none may share the initial hold if its
    // own approach is visible for either rigid or material grips.
    void take_hand_holds(const JPH::BodyInterface &bodies, const JPH::RVec3 centre,
                         const Grip &fallback) noexcept;

    // The swept move of a climbing or hanging body, slimmed so the holds it
    // already touches do not stop it.
    [[nodiscard]] bool climb_move_clear(const JPH::RVec3 from, const JPH::RVec3 to) const noexcept;

    // Take hold of `grip` where the body is, facing it.
    void begin_climb(JPH::BodyInterface &bodies, const Grip &grip,
                     const bool allow_single_hand = false) noexcept;

    // From the ground, a hold faced at hand height.
    [[nodiscard]] Grip grip_in_front(const JPH::BodyInterface &bodies) const noexcept;

    [[nodiscard]] bool radial_ownership_eligible() const noexcept;

    [[nodiscard]] bool actual_hands_in_reach(const JPH::BodyInterface &bodies) const noexcept;

    [[nodiscard]] Grip missing_radial_grip(const JPH::BodyInterface &bodies) const noexcept;

    [[nodiscard]] bool hanging_posture_eligible(const JPH::BodyInterface &bodies) const noexcept;

    bool leave_radial_profile(const JPH::BodyInterface &bodies) noexcept;

    void sync_radial_profile(JPH::BodyInterface &bodies,const StepCommands &commands) noexcept;

    void advance_hanging_posture(const JPH::BodyInterface &bodies, JPH::Vec3 baseline_displacement,
                                 float dt, float lead_limit = 0.0F) noexcept;

    // Climbing, the stick toward the structure climbs, away climbs down,
    // sideways moves across -- each only onto holds the hands can reach.
    void update_climb(JPH::BodyInterface &bodies, const StepCommands &commands,
                      const float delta_seconds) noexcept;

    // Hand over hand: the hand furthest behind the way the body moves
    // reaches ahead to the next hold, one hand every kClimbRegripTicks.
    void move_hands(const JPH::BodyInterface &bodies, const JPH::RVec3 centre,
                    const double up_input, const double side_input) noexcept;

    // Soles within kClimbFootReach of walkable ground under a body at `centre`.
    [[nodiscard]] bool feet_on_ground(const JPH::RVec3 centre) const noexcept;

    // Climbing over a ledge in reach: the mantle takes over from where the
    // body is. The ledge is looked for where the player looks, then along
    // the structure's normal: a climber on a pipe faced from an angle looks
    // round at the deck it means to climb onto.
    [[nodiscard]] bool try_top_out(JPH::BodyInterface &bodies) noexcept;

    [[nodiscard]] bool plank_entry_overlap_is_legal() const noexcept;

    [[nodiscard]] bool foot_overlap_is_legal() const noexcept;

    void record_foot_contact(const JPH::BodyInterface &bodies, const SupportSample &support) noexcept;

    void refresh_rigid_transfer_foot() noexcept;

    void pre_step_physical_foot(float delta_seconds) noexcept;

    void try_begin_transfer_foot(JPH::BodyInterface &bodies, JPH::RVec3 landing) noexcept;

    bool begin_cargo_transfer(JPH::BodyInterface &bodies,const LedgeProbe &probe,
                              JPH::RVec3 origin) noexcept;

    [[nodiscard]] bool try_finish_cargo_transfer(JPH::BodyInterface &bodies,
                                                std::uint64_t event_tick = 0) noexcept;

    void update_cargo_transfer(JPH::BodyInterface &bodies,double up_input,float dt) noexcept;

    void step_off_climb(JPH::BodyInterface &bodies) noexcept;

    [[nodiscard]] JPH::Vec3 climb_support_velocity(const JPH::BodyInterface &bodies) const noexcept;

    void let_go_climb(JPH::BodyInterface &bodies) noexcept;

    // Springing back off the structure, away from it and up.
    void jump_off_climb(JPH::BodyInterface &bodies) noexcept;

    // The lip of a ledge near `near` (a point on or near its top edge),
    // faced along `normal`: a ray across just under the top onto its face,
    // then a ray down onto the top just inside the face.
    [[nodiscard]] Lip probe_lip(const JPH::RVec3 near, const JPH::Vec3 normal) const noexcept;

    // Hanging, the stick sideways moves along the ledge while its lip goes
    // on under the hands. Something standing on the ledge back from its lip
    // does not stop a shimmy; only climbing up needs the landing clear.
    void update_shimmy(JPH::BodyInterface &bodies, const StepCommands &commands,
                       const float delta_seconds) noexcept;

    // An edge behind a standing body with a drop beyond it: the floor ends
    // within kEdgeSearchReach behind the body and nothing is under the space
    // beyond for kEdgeDropMinimum, and a hanging body fits below its lip.
    [[nodiscard]] Lip probe_edge_drop(const JPH::BodyInterface &bodies) const noexcept;

    // Drop, standing with an edge behind: lower over it into a hang.
    [[nodiscard]] bool try_begin_lowering(JPH::BodyInterface &bodies) noexcept;

    void update_physical_lowering(JPH::BodyInterface &bodies,float dt) noexcept;

    // Lowered: hanging from the lip, as if the hang had been caught there.
    void finish_lowering(JPH::BodyInterface &bodies) noexcept;

    // The support under a walking body, if it is a beam: a box narrower
    // than kBalanceMaxWidth and at least kBalanceMinLength long, lying level.
    struct Beam final {
        bool valid = false;
        JPH::Vec3 along{JPH::Vec3::sZero()};
        JPH::Vec3 across{JPH::Vec3::sZero()};
        float offset = 0.0F;
    };

    [[nodiscard]] Beam beam_underfoot(const JPH::BodyInterface &bodies) const noexcept;

    void clear_traversal(bool preserve_valid_foot = false) noexcept;

    void update_affordance(const JPH::BodyInterface &bodies) noexcept;

    // Machine half of a checkpoint (TDD 14.1: "machine/control state").
    // Kinematic bodies are deliberately excluded -- see BodyCheckpoint comment.
    void capture_machine_checkpoint(const JPH::BodyInterface &bodies,
                                    MachineCheckpoint &checkpoint) const noexcept;

    void commit_machine_checkpoint(const JPH::BodyInterface &bodies) noexcept;

    // WO-008 automatic commit (GDD 9.1): every tick the player is grounded on
    // firm footing (kCheckpointFootingNormalY) and not mid-traversal, so the
    // checkpoint is always "wherever the player was last standing." No dwell
    // timer, no player-facing save action.
    void commit_checkpoint(const JPH::BodyInterface &bodies) noexcept;

    // WO-008 death restore (Governing Laws 9, 21): the one sanctioned
    // exception to "no hidden teleportation," explicitly named by Law 21
    // itself. Restores player and every captured machine body, then clears
    // this tick's now-stale contact/traversal-adjacent state so the next
    // tick re-establishes ground truth from a fresh contact pass rather than
    // publishing a snapshot that mixes a teleported position with a contact
    // sample that referred to the pre-restore position.
    void restore_from_checkpoint(JPH::BodyInterface &bodies) noexcept;

    [[nodiscard]] SuppliedAscent::Selection supplied_machine_station(const JPH::BodyInterface &bodies,const bool require_footing=true)const noexcept;

    [[nodiscard]] ServiceLift::Station service_lift_station(const JPH::BodyInterface &bodies, const bool require_footing = true) const noexcept;

    void read_machine_state(const JPH::BodyInterface &bodies) noexcept;

    void read_state() noexcept;

    JoltRuntimeLease runtime_;
    JPH::TempAllocatorImpl temp_allocator_;
    JPH::JobSystemThreadPool job_system_;
    BroadPhaseLayerInterface broadphase_layer_interface_;
    ObjectVsBroadPhaseFilter object_vs_broadphase_filter_;
    ObjectLayerPairFilter object_layer_pair_filter_;
    JPH::PhysicsSystem physics_system_;
    const bool regression_fixtures_;
    PlayerContactListener contact_listener_;
    PlayerSoftContactListener soft_contact_listener_;
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
    SoftFootPolicy soft_foot_policy_{};

    SteamPlant steam_plant_{};
    WaterScrew water_screw_{};
    bool water_screw_station_active_ = false;
    scraperx::sim::bands::GroundWaterLift ground_water_lift_{};
    bool water_lift_valve_station_active_ = false;
    bool water_lift_release_station_active_ = false;
    bool water_lift_reset_station_active_ = false;
    bool water_lift_valve_open_ = false;
    double water_lift_valve_flow_m3_s_ = 0.0;
    double water_lift_bucket_water_m3_ = 0.0;
    std::vector<JPH::BodyID> machine_bodies_;
    std::vector<JPH::Ref<JPH::TwoBodyConstraint>> machine_constraints_;
    JPH::Ref<JPH::HingeConstraint> tipper_hinge_;
    JPH::Ref<JPH::HingeConstraint> valve_hinge_;
    JPH::Ref<JPH::HingeConstraint> treadle_hinge_;
    JPH::Ref<JPH::HingeConstraint> jib_slew_hinge_;
    JPH::Ref<JPH::SliderConstraint> jib_hoist_slider_;
    JPH::Ref<JPH::SliderConstraint> needle_hoist_slider_;
    JPH::Ref<JPH::HingeConstraint> intake_slew_hinge_;
    JPH::Ref<JPH::SliderConstraint> intake_hoist_slider_;
    JPH::Ref<JPH::HingeConstraint> intake_dog_hinge_;
    // AS-002: unmotored -- gravity and MOD-CW-CRADLE's rope tension are the
    // only actuators, so this carries no target-velocity state.
    JPH::Ref<JPH::HingeConstraint> intake_swing_hinge_;
    JPH::Ref<JPH::PulleyConstraint> intake_cw_pulley_;
    // track_for_teardown=false: created/removed at runtime by seat_needle/
    // unseat_needle, never through machine_constraints_. See create_constraint.
    JPH::Ref<JPH::PointConstraint> needle_pin_approach_;
    JPH::Ref<JPH::PointConstraint> needle_pin_far_;
    // AS-002: the hook-pack sling, created/removed at runtime by
    // sling_pack()/release_pack_to_cradle() -- same track_for_teardown=false
    // contract as the needle pins above.
    JPH::Ref<JPH::PointConstraint> intake_sling_pin_;
    // AS-003: the carry, created/removed at runtime (track_for_teardown=false).
    JPH::Ref<JPH::PointConstraint> carry_constraint_;
    JPH::BodyID carried_id_;
    std::uint64_t carried_entity_ = 0;
    std::uint64_t carry_target_entity_ = 0;
    std::uint32_t grip_over_steps_ = 0;
    // AS-006: the mechanism kit and the bands built from it.
    std::unique_ptr<scraperx::sim::kit::Kit> kit_;
    std::unique_ptr<DeformablePlank> deformable_plank_;
    std::unique_ptr<ServiceLift> service_lift_;
    std::unique_ptr<SuppliedAscent> supplied_ascent_;
    std::unique_ptr<PipeBridge> pipe_bridge_;
    std::unique_ptr<Slingshot> slingshot_;
    bool slingshot_flight_ = false;
    JPH::Vec3 rider_specific_acceleration_ = JPH::Vec3::sZero();
    std::unique_ptr<SwingStair> swing_stair_;
    std::unique_ptr<UpperAscent> upper_ascent_;
    std::unique_ptr<CargoNet> cargo_net_;
    std::unique_ptr<CargoNet> catch_net_;
    CatchNetSnapshot catch_net_state_{};
    bool catch_contact_previous_ = false;
    bool catch_episode_active_ = false;
    std::unique_ptr<PhysicalHandClimb> physical_hands_;
    std::unique_ptr<PhysicalFootPush> physical_foot_push_;
    std::unique_ptr<GrapplingGun> grappling_gun_;
    bool grapple_acquired_ = false, grapple_equipped_ = false;
    bool foot_push_started_this_tick_ = false;
    enum class FootPushPurpose { None, PlankJump, RigidTransfer };
    FootPushPurpose foot_push_purpose_ = FootPushPurpose::None;
    FootContactSample foot_contact_{}, foot_push_contact_{};
    std::uint64_t contact_solve_serial_ = 0;
    bool rigid_transfer_foot_valid_ = false;
    TraversalAffordanceCache traversal_affordances_;
    TraversalPlanState traversal_plan_;
    std::uint16_t foot_push_start_broken_mask_ = 0;
    std::uint64_t foot_push_start_count_ = 0;
    std::unique_ptr<TeeterRise> teeter_rise_;
    scraperx::sim::bands::CounterweightWell well_{};
    mutable std::vector<scraperx::sim::kit::Kit::CarryCandidate> kit_carryables_;
    std::uint8_t rig_action_ = 0;
    std::uint64_t rig_target_entity_ = 0;
    scraperx::sim::kit::AnchorIndex rig_anchor_;
    scraperx::sim::kit::RopeIndex rig_rope_;
    JPH::Ref<JPH::HingeConstraint> hook5_door_hinge_;
    JPH::BodyID hook5_door_id_;
    JPH::BodyID hook5_bar_id_;
    JPH::BodyID hook5_block_id_;
    JPH::BodyID scoop_ids_[4];
    JPH::Vec3 scoop_local_[4]{};
    JPH::BodyID ballast_id_;
    JPH::BodyID tipper_id_;
    JPH::BodyID valve_lever_id_;
    JPH::BodyID treadle_id_;
    JPH::BodyID lift_platform_id_;
    JPH::BodyID counterweight_id_;
    JPH::BodyID jib_boom_id_;
    JPH::BodyID jib_hook_id_;
    JPH::BodyID crate_id_;
    JPH::BodyID capacity_stand_load_id_;
    JPH::BodyID needle_pier_approach_id_;
    JPH::BodyID needle_pier_far_id_;
    JPH::BodyID needle_beam_id_;
    JPH::BodyID sump_grate_id_;
    JPH::BodyID intake_belt_id_;
    JPH::BodyID intake_boom_id_;
    JPH::BodyID intake_hook_id_;
    JPH::BodyID intake_pack_id_;
    JPH::BodyID intake_overweight_pack_id_;
    JPH::BodyID intake_dog_id_;
    JPH::BodyID intake_swing_flight_id_;
    JPH::BodyID intake_cw_cradle_id_;
    // AS-002: the sling's actual presence -- read back for the snapshot and
    // the checkpoint, never asserted. Starts true: AS-001 pre-slings the pack.
    bool intake_pack_slung_ = true;
    float scoop_height_ = kScoopBottomY;
    float scoop_tilt_ = 0.0F;
    float valve_lever_angle_ = kValveShutAngle;
    float treadle_angle_ = kTreadleRestAngle;
    float jib_boom_angle_ = 0.0F;
    bool jib_station_active_ = false;
    bool needle_station_active_ = false;
    bool needle_seated_ = false;
    bool sump_station_active_ = false;
    bool sump_isolated_ = false;
    float sump_volume_kg_ = 0.0F;
    bool grate_safe_ = false;
    bool intake_station_active_ = false;
    float intake_boom_angle_ = 0.0F;
    float intake_dog_angle_ = 0.0F;
    bool intake_pack_pins_dog_ = false;
    bool intake_throat_clear_ = false;
    // AS-002: non-negative "how far open" travel, derived from the hinge's
    // raw (negative-signed) angle -- see kIntakeSwingHingeTravelRadians.
    float intake_swing_travel_ = 0.0F;
    float rope_rest_length_ = 0.0F;
    double machine_cycle_phase_seconds_ = 0.0;
    SupportSample support_sample_{};
    JPH::Vec3 airborne_inherited_velocity_{JPH::Vec3::sZero()};
    JPH::Vec3 facing_{JPH::Vec3::sZero()};
    bool grounded_ = false;
    std::uint64_t support_entity_id_ = 0;
    double rotating_support_yaw_radians_ = 0.0;

    std::uint64_t step_up_count_ = 0;
    std::uint64_t step_up_pose_count_ = 0;
    std::uint64_t jump_vault_count_ = 0;
    std::uint32_t jump_vault_ticks_left_ = 0;
    std::uint32_t ordinary_jump_ticks_left_ = 0;
    float jump_takeoff_feet_y_ = 0.0F;
    bool crouched_ = false;
    std::uint32_t world_solid_bodies_ = 0;
    std::uint32_t world_solid_mirrors_ = 0;
    std::uint32_t world_solid_rejected_ = 0;

    bool cargo_transfer_active_ = false;
    double cargo_transfer_peak_hand_load_n_ = 0;
    float cargo_push_seconds_ = -1.0F;
    bool causal_airborne_ = false;
    bool physical_step_pending_ = false;
    float physical_step_start_feet_y_ = 0;
    JPH::Vec3 swing_rest_offset_ = JPH::Vec3::sZero();
    float hanging_posture_offset_m_ = 0.0F;
    bool hanging_posture_input_ = false;
    bool second_hand_added_this_tick_ = false;
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
    JPH::RVec3 traversal_desired_{JPH::RVec3::sZero()};
    double traversal_progress_ = 0.0;
    double traversal_duration_ = kMantleDurationSeconds;
    double traversal_approach_fraction_ = 0.0;
    float traversal_approach_entry_slope_ = 0.0F;
    std::uint32_t traversal_stall_ticks_ = 0;
    JPH::BodyID last_released_grip_;
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
        std::uint64_t generation = 0;
        bool valid = false;
    };
    JPH::Vec3 traversal_normal_{JPH::Vec3::sZero()};
    HandHold hands_[2]{};
    std::uint32_t regrip_cooldown_ticks_ = 0;
    std::uint64_t climb_count_ = 0;
    Grip grip_affordance_{};
    Lip edge_affordance_{};
    bool sprinting_ = false;
    bool balancing_ = false;
    // Top speed the air steers toward: the walk, or what the ground gave the
    // body as it left it (a running jump keeps its speed).
    float air_full_speed_ = kPlayerMaximumRelativeSpeed;

    struct MachineCheckpoint final {
        BodyCheckpoint ballast{};
        BodyCheckpoint tipper{};
        BodyCheckpoint valve_lever{};
        BodyCheckpoint treadle{};
        BodyCheckpoint lift_platform{};
        BodyCheckpoint counterweight{};
        BodyCheckpoint jib_boom{};
        BodyCheckpoint jib_hook{};
        JPH::EMotorState jib_hoist_motor_state = JPH::EMotorState::Velocity;
        float jib_hoist_target_position = 0.0F;
        float jib_hoist_target_velocity = 0.0F;
        BodyCheckpoint crate{};
        BodyCheckpoint needle_beam{};
        bool needle_seated = false;
        float sump_volume_kg = 0.0F;
        bool sump_isolated = false;
        double vessel_mass_kg = 0.0;
        double cylinder_mass_kg = 0.0;
        WaterScrewState water_screw{};
        double water_lift_bucket_water_m3 = 0.0;
        bool water_lift_valve_open = false;
        // AS-002. The pack/hook and the dog are AS-001 gaps this ticket does
        // not reopen (00_START_HERE.md's record-gap convention): they are not
        // captured here either, unchanged from AS-001. MOD-STAIR-A-SWING and
        // MOD-CW-CRADLE are this ticket's own bodies, so they are.
        BodyCheckpoint intake_swing_flight{};
        BodyCheckpoint intake_cw_cradle{};
        bool intake_pack_slung = true;
        // AS-003: the door, the bar and the block, and what was being carried.
        BodyCheckpoint hook5_door{};
        BodyCheckpoint hook5_bar{};
        BodyCheckpoint hook5_block{};
        std::uint64_t carrying_entity = 0;
        // AS-006: every kit body, rope end, parted rope and catch.
        scraperx::sim::kit::Kit::Checkpoint kit{};
        // Genuine contact on a Kit body included in this rollback. The
        // generation-bearing BodyID prevents a replaced support being used.
        JPH::BodyID support_body{};
        std::uint64_t support_entity = 0;
        JPH::Vec3 support_local_contact = JPH::Vec3::sZero();
        PipeBridge::State pipe_bridge{};
        Slingshot::State slingshot{};
        SwingStair::State swing_stair{};
        UpperAscent::State upper_ascent{};
        std::string cargo_net;
        std::string catch_net;
        ServiceLift::State service_lift{};
        SuppliedAscent::Checkpoint supplied_ascent{};
        DeformablePlank::State deformable_plank{};
    };
    JPH::RVec3 checkpoint_position_{JPH::RVec3::sZero()};
    bool checkpoint_crouched_ = false;
    MachineCheckpoint checkpoint_{};
    MachineCheckpoint service_lift_retry_checkpoint_{};
    JPH::RVec3 service_lift_retry_position_{JPH::RVec3::sZero()};
    bool service_lift_retry_crouched_ = false;
    bool service_lift_retry_available_ = false;
    MachineCheckpoint gravity_reclaim_retry_checkpoint_{};
    JPH::RVec3 gravity_reclaim_retry_position_{JPH::RVec3::sZero()};
    bool gravity_reclaim_retry_crouched_ = false;
    bool gravity_reclaim_retry_available_ = false;
    std::uint64_t checkpoint_commit_count_ = 0;
    std::uint64_t death_count_ = 0;
    bool parachute_deployed_ = false;
    float fall_peak_speed_mps_ = 0.0F;
    float pre_contact_fall_speed_mps_ = 0.0F;
    float last_impact_speed_mps_ = 0.0F;
    std::uint64_t landing_count_ = 0, landing_support_entity_id_ = 0;
    double landing_approach_energy_j_ = 0, landing_tangent_energy_j_ = 0;
    double landing_normal_speed_mps_ = 0, landing_tangent_speed_mps_ = 0;
    unsigned landing_response_ = 0; //0 ordinary,1 brace,2 roll,3 stumble
    double landing_observed_normal_impulse_ns_ = 0, landing_recovery_seconds_ = 0;
    double landing_recovery_duration_ = 0, landing_initial_balance_loss_ = 0;
    double landing_recovery_work_j_ = 0;
    double landing_jump_work_j_ = 0;
    double air_control_command_positive_work_j_ = 0;
    double air_control_command_absorbed_work_j_ = 0;
    JPH::Vec3 air_control_force_ = JPH::Vec3::sZero();
    JPH::Vec3 landing_slip_velocity_ = JPH::Vec3::sZero();
    JPH::Vec3 landing_applied_force_ = JPH::Vec3::sZero();
    JPH::RVec3 landing_force_start_ = JPH::RVec3::sZero();
    JPH::BodyID landing_force_support_id_;
    JPH::RVec3 landing_force_support_start_ = JPH::RVec3::sZero();
    JPH::Vec3 landing_force_support_local_point_ = JPH::Vec3::sZero();

    Snapshot state_{};
};

} // namespace scraperx::sim
