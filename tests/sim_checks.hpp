#pragma once

// Shared by the simulation tests. Inline on purpose: the helpers
// used to live in one file, and every section still calls them.

#include "sim/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <utility>



inline void require(const bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

inline bool nearly_equal(const double a, const double b, const double epsilon = 1.0e-12) {
    return std::abs(a - b) <= epsilon;
}

inline double horizontal_distance(const scraperx::sim::Vector3 &a,
                           const scraperx::sim::Vector3 &b) {
    return std::hypot(a.x - b.x, a.z - b.z);
}

inline double horizontal_magnitude(const scraperx::sim::Vector3 &value) {
    return std::hypot(value.x, value.z);
}

inline double horizontal_dot(const scraperx::sim::Vector3 &a,
                      const scraperx::sim::Vector3 &b) {
    return a.x * b.x + a.z * b.z;
}

// Steps the authoritative clock one fixed step at a time until the predicate
// holds against a real snapshot, or the budget expires. Tests never reach into
// the simulation to force a state.
template <typename Predicate>
inline bool advance_until(scraperx::sim::Simulation &simulation,
                   Predicate predicate,
                   const double budget_seconds) {
    const auto budget_ticks = static_cast<std::uint32_t>(
        budget_seconds * static_cast<double>(scraperx::sim::Simulation::kTickRateHz));
    for (std::uint32_t tick = 0; tick < budget_ticks; ++tick) {
        if (!simulation.advance_frame(scraperx::sim::Simulation::kFixedStepSeconds).accepted) {
            return false;
        }
        if (predicate(simulation.snapshot())) {
            return true;
        }
    }
    return false;
}

// Walks the player toward a horizontal target for a fixed interval, steering
// every tick the way a human holding a stick does. Tests never teleport.
// Returns the deepest (most negative) z reached at any point during the walk,
// so an impassability claim is checked against the whole attempt rather than
// against wherever the player happened to end up on the last tick.
inline double walk_toward(scraperx::sim::Simulation &simulation,
                   const double target_x,
                   const double target_z,
                   const double seconds) {
    const double step = scraperx::sim::Simulation::kFixedStepSeconds;
    const auto ticks = static_cast<std::uint32_t>(seconds / step);
    double deepest_z = simulation.snapshot().player_position.z;
    for (std::uint32_t tick = 0; tick < ticks; ++tick) {
        const auto state = simulation.snapshot();
        double dx = target_x - state.player_position.x;
        double dz = target_z - state.player_position.z;
        const double length = std::hypot(dx, dz);
        if (length > 1.0e-6) {
            dx /= length;
            dz /= length;
        }
        (void)simulation.set_move_input(dx, dz);
        (void)simulation.set_facing(dx, dz);
        (void)simulation.advance_frame(step);
        deepest_z = std::min(deepest_z, simulation.snapshot().player_position.z);
    }
    return deepest_z;
}

inline scraperx::sim::Snapshot run_mantle_command_stream(const bool single_fixed_steps) {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;

    Simulation simulation(InitialSpawn::MantleApproach);
    require(simulation.set_facing(1.0, 0.0), "partition run must accept facing");

    const auto run_one_second = [&simulation, single_fixed_steps]() {
        if (single_fixed_steps) {
            for (std::uint32_t tick = 0; tick < Simulation::kTickRateHz; ++tick) {
                require(simulation.advance_frame(Simulation::kFixedStepSeconds).accepted,
                        "partition run fixed step must be accepted");
            }
        } else {
            require(simulation.advance_frame(1.0).accepted,
                    "partition run batched second must be accepted");
        }
    };

    run_one_second();
    require(simulation.request_traversal(), "partition run traversal request must be accepted");
    run_one_second();
    return simulation.snapshot();
}


// While armed, the largest horizontal step the body makes in one tick inside
// the helpers that advance the world a tick at a time (walk_to, wait_for,
// hold_stick): a body moved by the native's own walking, climbing and
// mantling never jumps; a mantle snapped across a blocked path does. Each
// helper call starts a fresh baseline, so a batched advance between calls is
// never read as one step.
struct PathWatch final {
    bool armed = false;
    bool primed = false;
    double last_x = 0.0;
    double last_z = 0.0;
    double worst = 0.0;
    scraperx::sim::Vector3 worst_at{};
    double worst_from_x = 0.0;
    double worst_from_z = 0.0;
    int worst_traversal = 0;
};
inline PathWatch g_path_watch;

inline void observe_path(const scraperx::sim::Simulation &simulation, const bool measure) {
    if (!g_path_watch.armed) {
        return;
    }
    const auto state = simulation.snapshot();
    if (measure && g_path_watch.primed) {
        const double step = std::hypot(state.player_position.x - g_path_watch.last_x,
                                       state.player_position.z - g_path_watch.last_z);
        if (step > g_path_watch.worst) {
            g_path_watch.worst = step;
            g_path_watch.worst_at = state.player_position;
            g_path_watch.worst_from_x = g_path_watch.last_x;
            g_path_watch.worst_from_z = g_path_watch.last_z;
            g_path_watch.worst_traversal = static_cast<int>(state.traversal_state);
        }
    }
    g_path_watch.primed = true;
    g_path_watch.last_x = state.player_position.x;
    g_path_watch.last_z = state.player_position.z;
}

// One tick of steering at a horizontal target, easing off over the last
// 0.6 m so the body arrives instead of orbiting the point.
inline void steer_toward(scraperx::sim::Simulation &simulation, const double x, const double z,
                  const double speed = 1.0) {
    const auto state = simulation.snapshot();
    const double dx = x - state.player_position.x;
    const double dz = z - state.player_position.z;
    const double length = std::hypot(dx, dz);
    if (length < 0.05) {
        (void)simulation.set_move_input(0.0, 0.0);
        return;
    }
    const double scale = std::min(1.0, length / 0.6) * speed;
    (void)simulation.set_move_input(dx / length * scale, dz / length * scale);
    (void)simulation.set_facing(dx / length, dz / length);
}

// Walks to a horizontal target and stops on it; true once within tolerance.
inline bool walk_to(scraperx::sim::Simulation &simulation, const double x, const double z,
             const double budget_seconds, const double tolerance = 0.15) {
    using scraperx::sim::Simulation;
    const auto ticks = static_cast<std::uint32_t>(
        budget_seconds * static_cast<double>(Simulation::kTickRateHz));
    observe_path(simulation, false);
    for (std::uint32_t tick = 0; tick < ticks; ++tick) {
        const auto state = simulation.snapshot();
        if (std::hypot(x - state.player_position.x, z - state.player_position.z) <= tolerance) {
            (void)simulation.set_move_input(0.0, 0.0);
            return true;
        }
        steer_toward(simulation, x, z);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(simulation, true);
    }
    (void)simulation.set_move_input(0.0, 0.0);
    return false;
}

// ---- AS-006, the Counterweight Well ----------------------------------------

constexpr double kGravity = 9.81;
constexpr double kRiderMassKg = 85.0;
constexpr double kWellACageMassKg = 350.0;
constexpr double kWellASkipMassKg = 800.0;
constexpr double kWellACageFloorTop = 154.25;
constexpr double kWellATravel = 22.0;
// The energy rule is checked from body states every tick. A standing capsule
// settles by millimetres on whatever floor it is on; 1 cm of that for the
// 85 kg rider is the tolerance, so stance noise is never read as a motor.
constexpr double kStanceJitterJ = kRiderMassKg * kGravity * 0.01;

inline double kit_y(const scraperx::sim::Simulation &simulation, const std::uint64_t entity) {
    return simulation.kit_body_position(simulation.kit_body_index(entity)).y;
}

// From the 154 m deck into Stage A's cage through its open north side,
// east of the trip handle hanging in front of it.
inline bool board_well_a(scraperx::sim::Simulation &simulation) {
    return walk_to(simulation, -10.0, -129.2, 5.0) && walk_to(simulation, -10.2, -130.6, 4.0);
}

// Inside the cage: face the bollard south-west and take the rope's end off
// it; face the cage's eye west and hook it on. Every step is the rig verb
// the snapshot offers at that moment, never a forced state.
inline bool rig_well_a(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, -11.35, -131.95, 4.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(-0.7, -0.7);
    (void)simulation.advance_frame(0.4);
    const auto at_bollard = simulation.snapshot();
    if (at_bollard.rig_action != 2 ||
        at_bollard.rig_target_entity_id != Simulation::kWellAShackleEntityId) {
        return false;
    }
    (void)simulation.request_rig();
    (void)simulation.advance_frame(0.3);
    if (simulation.snapshot().carrying_entity_id != Simulation::kWellAShackleEntityId ||
        !walk_to(simulation, -11.35, -131.40, 3.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(-1.0, 0.0);
    (void)simulation.advance_frame(0.5);
    const auto at_eye = simulation.snapshot();
    if (at_eye.rig_action != 1 || at_eye.rig_target_entity_id != Simulation::kWellACageEntityId) {
        return false;
    }
    (void)simulation.request_rig();
    (void)simulation.advance_frame(0.3);
    const auto hooked = simulation.snapshot();
    return hooked.carrying_entity_id == 0 &&
           hooked.well_a_rope_end_entity_id == Simulation::kWellACageEntityId;
}

// Inside the cage at its open north side: take the trip handle hanging in
// front of it and step back south at `pull` of full stick until the catch
// lets go (or `seconds` pass), then let go of the handle. True once the
// catch has been seen open.
inline bool pull_well_a(scraperx::sim::Simulation &simulation, const double pull, const double seconds) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, -11.05, -130.35, 3.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(0.5);
    const auto facing = simulation.snapshot();
    if (facing.carry_target_entity_id != Simulation::kWellAHandleEntityId ||
        facing.carry_target_kind != 2) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.3);
    if (simulation.snapshot().carrying_entity_id != Simulation::kWellAHandleEntityId) {
        return false;
    }
    bool opened = false;
    const auto ticks =
        static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (std::uint32_t tick = 0; tick < ticks && !opened; ++tick) {
        (void)simulation.set_move_input(0.0, -0.7 * pull);
        (void)simulation.set_facing(0.0, 1.0);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        opened = !simulation.snapshot().well_a_catch_latched;
    }
    (void)simulation.set_move_input(0.0, 0.0);
    if (simulation.snapshot().carrying_entity_id != 0) {
        (void)simulation.request_set_down();
    }
    return opened;
}

constexpr double kWellBBoomMassKg = 2500.0;
constexpr double kWellBBoomLength = 22.0;
constexpr double kWellBCageFloorTop = 176.25;
constexpr double kWellCPlatformMassKg = 700.0;
constexpr double kWellCDumpsterMassKg = 400.0;
constexpr double kWellCPlatformFloorTop = 198.25;
constexpr double kWellRubbleKg = 900.0;
// The dogs that hold B's cage and C's platform at the top stand every
// 0.05 m: a car on them rests at most one pitch under its stop.
constexpr double kWellDogPitch = 0.05;

inline double kit_com_y(const scraperx::sim::Simulation &simulation, const std::uint64_t entity) {
    return simulation.kit_body_center_of_mass(simulation.kit_body_index(entity)).y;
}

inline double kit_com_z(const scraperx::sim::Simulation &simulation, const std::uint64_t entity) {
    return simulation.kit_body_center_of_mass(simulation.kit_body_index(entity)).z;
}

// From inside a cage: stand at (ux, uz) facing (ufx, ufz) and take the rope's
// end off where it is made fast; step back into the cage so the held end
// clears the posts; stand at (hx, hz) facing (hfx, hfz) and hook it on the
// eye. Every step is the rig verb the snapshot offers at that moment.
inline bool rig_end(scraperx::sim::Simulation &simulation, const double ux, const double uz,
             const double ufx, const double ufz, const std::uint64_t shackle, const double hx,
             const double hz, const double hfx, const double hfz, const std::uint64_t eye) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, ux, uz, 4.0, 0.1)) {
        return false;
    }
    (void)simulation.set_facing(ufx, ufz);
    (void)simulation.advance_frame(0.4);
    const auto at_end = simulation.snapshot();
    if (at_end.rig_action != 2 || at_end.rig_target_entity_id != shackle) {
        return false;
    }
    (void)simulation.request_rig();
    (void)simulation.advance_frame(0.3);
    if (simulation.snapshot().carrying_entity_id != shackle) {
        return false;
    }
    for (std::uint32_t tick = 0; tick < 30; ++tick) {
        (void)simulation.set_move_input(-0.5 * ufx, -0.5 * ufz);
        (void)simulation.set_facing(ufx, ufz);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)simulation.set_move_input(0.0, 0.0);
    if (!walk_to(simulation, hx, hz, 4.0, 0.1)) {
        return false;
    }
    (void)simulation.set_facing(hfx, hfz);
    (void)simulation.advance_frame(0.6);
    const auto at_eye = simulation.snapshot();
    if (at_eye.rig_action != 1 || at_eye.rig_target_entity_id != eye) {
        return false;
    }
    (void)simulation.request_rig();
    (void)simulation.advance_frame(0.3);
    return simulation.snapshot().carrying_entity_id == 0;
}

// Stand at (x, z) facing north, take the trip handle hanging there, and step
// back south at `pull` of full stick until done(snapshot) or `seconds` pass;
// then let go. True once done was seen.
template <typename Done>
inline bool pull_handle(scraperx::sim::Simulation &simulation, const double x, const double z,
                 const std::uint64_t handle, const double pull, const double seconds, Done done) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, x, z, 4.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(0.5);
    const auto facing = simulation.snapshot();
    if (facing.carry_target_entity_id != handle || facing.carry_target_kind != 2) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.3);
    if (simulation.snapshot().carrying_entity_id != handle) {
        return false;
    }
    bool seen = false;
    const auto ticks =
        static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (std::uint32_t tick = 0; tick < ticks && !seen; ++tick) {
        (void)simulation.set_move_input(0.0, -0.7 * pull);
        (void)simulation.set_facing(0.0, 1.0);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        seen = done(simulation.snapshot());
    }
    (void)simulation.set_move_input(0.0, 0.0);
    if (simulation.snapshot().carrying_entity_id != 0) {
        (void)simulation.request_set_down();
    }
    return seen;
}

// Stage B's rig from inside its cage: the line's end off the cleat in front
// of the opening, onto the eye on the cage's east face.
inline bool rig_well_b(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return rig_end(simulation, -6.3, -130.35, 0.0, 1.0, Simulation::kWellBShackleEntityId, -6.35,
                   -131.40, 1.0, 0.0, Simulation::kWellBCageEntityId) &&
           simulation.snapshot().well_b_rope_end_entity_id == Simulation::kWellBCageEntityId;
}

inline bool pull_well_b(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_handle(simulation, -8.0, -130.25, Simulation::kWellBHandleEntityId, 0.5, 3.0,
                       [](const scraperx::sim::Snapshot &state) {
                           return !state.well_b_catch_latched;
                       });
}

// Stage C from its platform: throw the rebar clear of the chute's mouth (a
// full pull takes it over upright), then wait for the dumpster to fill.
inline bool clear_well_c_chute(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_handle(simulation, -4.4, -131.6, Simulation::kWellCHandleEntityId, 0.5, 3.0,
                       [](const scraperx::sim::Snapshot &state) {
                           return state.well_c_rebar_angle > 1.7;
                       });
}

inline bool fill_well_c(scraperx::sim::Simulation &simulation, const double seconds) {
    using scraperx::sim::Simulation;
    const auto ticks =
        static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (std::uint32_t tick = 0; tick < ticks; ++tick) {
        if (simulation.snapshot().well_c_dumpster_kg >= kWellRubbleKg - 1.0) {
            return true;
        }
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    }
    return simulation.snapshot().well_c_dumpster_kg >= kWellRubbleKg - 1.0;
}

inline bool pull_well_c_latch(scraperx::sim::Simulation &simulation, const double seconds) {
    using scraperx::sim::Simulation;
    return pull_handle(simulation, -3.0, -131.6, Simulation::kWellCLatchHandleEntityId, 0.5,
                       seconds, [](const scraperx::sim::Snapshot &state) {
                           return !state.well_c_catch_latched;
                       });
}

// Waits up to `seconds` for done(snapshot).
template <typename Done>
inline bool wait_for(scraperx::sim::Simulation &simulation, const double seconds, Done done) {
    using scraperx::sim::Simulation;
    const auto ticks =
        static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
    observe_path(simulation, false);
    for (std::uint32_t tick = 0; tick < ticks; ++tick) {
        if (done(simulation.snapshot())) {
            return true;
        }
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(simulation, true);
    }
    return done(simulation.snapshot());
}

// ---- Step 2 movement (MECHANISM_ASCENT_PLAN.md §8) --------------------------

// Holds the stick at (x, z) facing (fx, fz) until done(snapshot) or `seconds`
// pass; lets go of the stick either way. True once done was seen.
template <typename Done>
inline bool hold_stick(scraperx::sim::Simulation &simulation, const double x, const double z,
                const double fx, const double fz, const double seconds, Done done) {
    using scraperx::sim::Simulation;
    const auto ticks =
        static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
    bool seen = false;
    observe_path(simulation, false);
    for (std::uint32_t tick = 0; tick < ticks && !seen; ++tick) {
        (void)simulation.set_move_input(x, z);
        (void)simulation.set_facing(fx, fz);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        observe_path(simulation, true);
        seen = done(simulation.snapshot());
    }
    (void)simulation.set_move_input(0.0, 0.0);
    return seen;
}

inline double horizontal_speed(const scraperx::sim::Snapshot &state) {
    return std::hypot(state.player_linear_velocity.x, state.player_linear_velocity.z);
}

inline bool is_climbing(const scraperx::sim::Snapshot &state) {
    return state.traversal_state == scraperx::sim::TraversalState::Climbing;
}

inline bool standing_above(const scraperx::sim::Snapshot &state, const double y) {
    return state.traversal_state == scraperx::sim::TraversalState::None &&
           state.player_grounded && state.player_position.y > y;
}

// AS-006's climbing route, a leg at a time, on player inputs. Each returns
// false as soon as a leg fails.
//
// From the 154 m deck: round the ladder's west side to its foot, take hold
// (Action) and climb until the top-out mantles onto the 176 ring.
inline bool climb_route_ladder(scraperx::sim::Simulation &simulation) {
    if (!(walk_to(simulation, 3.5, -128.2, 16.0) && walk_to(simulation, 3.5, -129.8, 4.0) &&
          walk_to(simulation, 5.0, -129.6, 4.0, 0.08))) {
        return false;
    }
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(0.4);
    if (!simulation.snapshot().grip_available) {
        return false;
    }
    (void)simulation.request_traversal();
    (void)simulation.advance_frame(0.2);
    return is_climbing(simulation.snapshot()) &&
           hold_stick(simulation, 0.0, 1.0, 0.0, 1.0, 40.0,
                      [](const scraperx::sim::Snapshot &state) { return standing_above(state, 176.5); });
}

// On the 176 ring: onto the boards, south along the first, east along the
// second to the standpipe, take hold and climb until it mantles onto 198.
inline bool climb_route_pipe(scraperx::sim::Simulation &simulation) {
    if (!(walk_to(simulation, 7.05, -128.4, 8.0, 0.1) && walk_to(simulation, 7.05, -130.90, 8.0, 0.1) &&
          walk_to(simulation, 7.60, -130.90, 6.0, 0.08))) {
        return false;
    }
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(0.4);
    if (!simulation.snapshot().grip_available) {
        return false;
    }
    (void)simulation.request_traversal();
    (void)simulation.advance_frame(0.2);
    return is_climbing(simulation.snapshot()) &&
           hold_stick(simulation, 0.0, 1.0, 0.0, 1.0, 40.0,
                      [](const scraperx::sim::Snapshot &state) { return standing_above(state, 198.5); });
}

// On the 198 ring: out along the catwalk under the scaffold panel, jump for
// its bottom horizontals, and climb until it mantles onto 220.
inline bool climb_route_panel(scraperx::sim::Simulation &simulation) {
    if (!(walk_to(simulation, 11.0, -129.3, 16.0, 0.1) &&
          walk_to(simulation, 11.0, -131.40, 6.0, 0.08))) {
        return false;
    }
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(0.4);
    (void)simulation.request_jump();
    return hold_stick(simulation, 0.0, 0.6, 0.0, 1.0, 3.0,
                      [](const scraperx::sim::Snapshot &state) { return is_climbing(state); }) &&
           hold_stick(simulation, 0.0, 1.0, 0.0, 1.0, 40.0,
                      [](const scraperx::sim::Snapshot &state) { return standing_above(state, 220.5); });
}


inline void require_leg(const scraperx::sim::Simulation &simulation, const bool ok, const char *leg) {
    if (!ok) {
        const auto state = simulation.snapshot();
        std::cout << "ascent: stopped at " << leg << ", y=" << state.player_position.y
                  << " t=" << state.simulation_time_seconds << '\n';
    }
    require(ok, leg);
}

template <typename Done>
bool pull_facing(scraperx::sim::Simulation &simulation, const double x, const double z,
                 const double fx, const double fz, const std::uint64_t handle, const double pull,
                 const double seconds, Done done) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, x, z, 6.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(fx, fz);
    (void)simulation.advance_frame(0.5);
    if (simulation.snapshot().carry_target_entity_id != handle) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.3);
    if (simulation.snapshot().carrying_entity_id != handle) {
        return false;
    }
    bool seen = false;
    const auto ticks =
        static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (std::uint32_t tick = 0; tick < ticks && !seen; ++tick) {
        (void)simulation.set_move_input(-0.7 * pull * fx, -0.7 * pull * fz);
        (void)simulation.set_facing(fx, fz);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        seen = done(simulation.snapshot());
    }
    (void)simulation.set_move_input(0.0, 0.0);
    if (simulation.snapshot().carrying_entity_id != 0) {
        (void)simulation.request_set_down();
    }
    (void)simulation.advance_frame(0.3);
    return seen;
}
