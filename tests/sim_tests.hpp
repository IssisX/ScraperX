#pragma once

// The native tests' shared ground: the asserts and walking helpers every
// test uses, and the declarations of each stage's helpers that other files
// use (simulation_tests.cpp runs them all, in order; see its main()).

#include "sim/simulation.hpp"
#include "sim/vertical/vertical_route.hpp"
#include "sim/trapeze.hpp"
#include "sim/pipe_gallery.hpp"
#include "sim/climb_c7.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

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
        if (std::getenv("SCRAPERX_WALK_TRACE") != nullptr) {
            const auto t = simulation.snapshot();
            std::cout << "    walk(" << x << "," << z << ") p=(" << t.player_position.x << "," << t.player_position.y
                      << "," << t.player_position.z << ") bal=" << t.player_balancing << " xcom=" << t.beam_xcom
                      << " g=" << t.player_grounded << " sup=" << t.support_entity_id << " c=(" << t.support_contact_point.x << ","
                      << t.support_contact_point.y << "," << t.support_contact_point.z << ") v=(" << t.player_linear_velocity.x
                      << "," << t.player_linear_velocity.z << ")\n";
        }
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

// ---- AS-007 Wet Isolation (03_EXECUTION/ASCENT/AS-007_WET_ISOLATION.md) ------

// Stand at (x, z) facing (fx, fz), take the handle hanging there, and step
// back away from it at `pull` of full stick until done(snapshot) or `seconds`
// pass; then let go. True once done was seen.
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

// The ride, from the reel let go until the cage stands still on its dogs:
// its highest travel, its fastest speed, and how long it took.
struct ServiceRide final {
    bool settled = false;
    double apex = 0.0;
    double peak_speed = 0.0;
    double seconds = 0.0;
};

// What a climb of C4 saw on the way, for its falsifiers.
struct C4Notes final {
    bool balanced = false;          // the beam held the body on its line
    bool house_offered = false;     // anything offered facing the winch house from the runway
    bool house_caught = false;      // a jump at the winch house, pushing in, caught anything
    double shimmied = 0.0;          // how far the hang carried the body along the runway's lip
};

// One ride of N from the flap falling open to the cage at rest on its dogs:
// every tick, the energy the hopper released sinking (its gravel and the
// chain hanging from it, as they are that tick) against what the cage and its
// rider gained rising.
struct NRide final {
    bool settled = false;
    bool rode_on_cage = true;
    double apex = 0.0;
    double peak_speed = 0.0;
    double seconds = 0.0;
    double gain = 0.0;
    double released = 0.0;
};

// What a climb of C5 saw on the way, for its falsifiers.
struct C5Notes final {
    bool vaulted = false;        // the manifold went by in a vault
    bool crawled = false;        // the body passed under the ducts crouched
    double gap_speed = 0.0;      // the body's speed leaving the gap's edge, m/s
};

// One ride of O from the ram let go to the cab at rest on its dogs. Every
// tick, the energy the bucket released sinking (its gravel and the chain
// hanging from it, as they are that tick) against what the cab and its rider
// gained rising; and the order the causes came in.
struct ORide final {
    bool settled = false;
    bool rode_on_cab = true;
    bool in_order = false;
    double apex = 0.0;
    double peak_speed = 0.0;
    double seconds = 0.0;
    double gain = 0.0;
    double released = 0.0;
    double min_tension = 1.0e9;     // the rope, from the cab's first metre to its last two
    double wheel_gap = 0.0;         // the wheel's turn against the cab's rise, rad
};

// Everything a run can leave different, as raw bits: the clock and the player
// (the first kClockAndPlayerWords), then every kit body, every rope as drawn,
// every bin's contents.
constexpr std::size_t kClockAndPlayerWords = 12;

// ---- Band 0, the Stack: S1, the water-balance hoist -------------------------

constexpr double kS1CageMassKg = 300.0;

constexpr double kS1BucketMassKg = 150.0;

constexpr double kS1Travel = 21.8;

constexpr double kS1FloorTopUp = 22.05;

constexpr double kDeck2Top = 22.0;

// Water that balances the cage and an 85 kg rider against the empty bucket.
constexpr double kS1BalanceKg = kS1CageMassKg + 85.0 - kS1BucketMassKg;

// What one ride of S1 saw, from stepping onto the plate at grade to the top.
struct S1Ride final {
    bool reached_top = false;
    bool rode_on_cage = true;
    double seconds = 0.0;          // from the pawl dropping to the top
    double lift_water_kg = 0.0;    // in the bucket as the cage left the yard
    double shut_travel = -1.0;     // the cage's travel when the valve shut
    double shut_water_kg = 0.0;
    double shut_tank_kg = 0.0;
    double top_water_kg = 0.0;
    double top_tank_kg = 0.0;
    double worst_rise = 0.0;
    double worst_margin_j = std::numeric_limits<double>::infinity();
};

// From deck 2's south band up C1 to deck 4, on player inputs: out onto the
// landing, up onto the cabinet, a jump to hang from the duct's lip, up onto
// the duct and along it, up the vent stack over deck 3's edge; along deck 3,
// out along the monorail under the davit's ladder, a turn and a leap for it,
// up it onto the davit's arm, and back along the arm onto deck 4.
// What a climb of C1 saw on the way, for its falsifiers.
struct C1Notes final {
    bool ladder_in_reach_standing = true;
};

// ---- Band 0, the Stack: S2, the walking beam hoist with fixed ballast cart --

constexpr double kDeck6Top = 66.0;

constexpr double kS2FloorTopUp = 66.05;

struct S2Ride final {
    bool reached_top = false;
    bool rode_on_cage = true;
    double seconds = 0.0;
    double worst_margin_j = std::numeric_limits<double>::infinity();
};

struct C2Notes final {
    bool ladder_in_reach_standing = true;
};

// ---- S3: The brake-override counterweight hoist (88 -> 132 m) ----------------

constexpr double kDeck12Top = 132.0;

constexpr double kS3FloorTopUp = 132.05;

struct S3Ride final {
    bool reached_top = false;
    bool rode_on_cage = true;
    double seconds = 0.0;
    double worst_margin_j = std::numeric_limits<double>::infinity();
};

constexpr double kDeck14Top = 154.0;

// ---- Shared stage helpers, defined in their stage's file ---------------------

bool seat_wet_spool(scraperx::sim::Simulation &simulation);
bool throw_wet_fill(scraperx::sim::Simulation &simulation);
bool shut_wet_door(scraperx::sim::Simulation &simulation);
bool pull_wet_trip(scraperx::sim::Simulation &simulation);
bool couple_wet_hose(scraperx::sim::Simulation &simulation);
bool pull_wet_stop_valve(scraperx::sim::Simulation &simulation);
bool on_support(const scraperx::sim::Snapshot &state, const std::uint64_t entity);
void run_loose_stock();
void run_forklift();
void run_forklift_strain();
void run_wet_isolation();
void run_wet_band();
bool climb_wet_hold(scraperx::sim::Simulation &simulation, const double x, const double z,
                    const double fx, const double fz, const bool jump, const double top);
void run_wet_route();
bool walk_to_shop_cleat(scraperx::sim::Simulation &simulation);
bool rig_shop_g(scraperx::sim::Simulation &simulation);
bool pull_shop_g(scraperx::sim::Simulation &simulation);
bool pull_shop_girder_pin(scraperx::sim::Simulation &simulation);
bool board_shop_h(scraperx::sim::Simulation &simulation);
bool pull_shop_chock(scraperx::sim::Simulation &simulation);
bool rig_shop_i(scraperx::sim::Simulation &simulation);
bool pull_shop_domino(scraperx::sim::Simulation &simulation);
void run_pulley_static_end();
void run_plate_shop();
void run_plate_band();
void run_plate_route();
bool climb_wreck(scraperx::sim::Simulation &simulation, const double x, const double z, const double fx,
                 const double fz, const std::uint64_t entity, const double top);
void run_plate_wreckage();
void run_facade_crane();
bool climb_crane_band(scraperx::sim::Simulation &band);
void run_crane_band();
void run_crane_route();
void run_crane_wreckage();
bool rig_service_m(scraperx::sim::Simulation &simulation);
bool pull_service_chock(scraperx::sim::Simulation &simulation);
ServiceRide ride_service_m(scraperx::sim::Simulation &simulation);
void run_midstack_service();
bool c4_hang_up(scraperx::sim::Simulation &simulation, const double fx, const double fz, const double top,
                const char *leg);
bool c4_mantle(scraperx::sim::Simulation &simulation, const double fx, const double fz, const double top,
               const char *leg);
bool c4_stays_up(scraperx::sim::Simulation &simulation, const double x, const double z, const double stick_x,
                 const double stick_z, const double top);
bool climb_c4(scraperx::sim::Simulation &simulation, C4Notes *notes = nullptr);
void run_service_c4();
bool open_n_flap(scraperx::sim::Simulation &simulation);
NRide ride_service_n(scraperx::sim::Simulation &simulation, const bool rider_aboard);
bool step_off_n(scraperx::sim::Simulation &simulation);
void run_service_n();
bool climb_c5(scraperx::sim::Simulation &simulation, C5Notes *notes = nullptr);
void run_service_c5();
bool pull_o_handle(scraperx::sim::Simulation &simulation, double *freed_at = nullptr);
ORide ride_service_o(scraperx::sim::Simulation &simulation, const bool rider_aboard, const double ram_freed_at);
bool step_off_o(scraperx::sim::Simulation &simulation);
void run_service_o();
void run_slingshot();
std::vector<std::uint64_t> world_bits(const scraperx::sim::Simulation &simulation);
void run_swing();
void run_c6();
void run_mast();
void run_pitman();
void run_helix();
void run_derrick();
void run_high();
void run_incline();
void run_wheel();
void run_teeter();
void run_trapeze();
void run_landing();
void run_save();
void run_tram();
void run_determinism();
void run_service_band();
void run_service_route();
void run_ascent();
bool board_s1(scraperx::sim::Simulation &simulation);
bool s1_plate_at_rest(const scraperx::sim::Simulation &simulation);
S1Ride ride_s1(scraperx::sim::Simulation &simulation, const double seconds);
bool climb_c1(scraperx::sim::Simulation &simulation, C1Notes *notes = nullptr);
bool climb_c1_to_monorail(scraperx::sim::Simulation &simulation);
bool shove_s2_bolt(scraperx::sim::Simulation &simulation);
bool s2_bolt_at_rest(const scraperx::sim::Simulation &simulation);
S2Ride ride_s2(scraperx::sim::Simulation &simulation, const double seconds);
bool climb_c2(scraperx::sim::Simulation &simulation, C2Notes *notes = nullptr);
bool take_s3_handle(scraperx::sim::Simulation &simulation);
bool s3_handle_at_rest(const scraperx::sim::Simulation &simulation);
S3Ride ride_s3(scraperx::sim::Simulation &simulation, const double seconds);
bool climb_c3(scraperx::sim::Simulation &simulation);
void run_stack();
void run_wet_handling();
void run_brachiation();
void run_pipe_gallery();
void run_slide();
void run_speed_vault();
void run_wall_run();
void run_wall_climb();
void run_tuck();
void run_flow();
void run_springboard();
void run_hook();
void run_workers();
void run_c7();
void run_summit();
void run_summit_arm();
void run_matron();
void run_jump_buffer();
void run_parkour_seams();

// ---- Each file's tests, run in order by main() ---------------------------------

void run_core_traversal();
void run_fall_checkpoint();
void run_world_solids();
void run_well_stage_a();
void run_well_stage_b();
void run_well_stage_c();
void run_well_band();
void run_step2_movement();
