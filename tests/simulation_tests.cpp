#include "sim/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <utility>

namespace {

void require(const bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool nearly_equal(const double a, const double b, const double epsilon = 1.0e-12) {
    return std::abs(a - b) <= epsilon;
}

double horizontal_distance(const scraperx::sim::Vector3 &a,
                           const scraperx::sim::Vector3 &b) {
    return std::hypot(a.x - b.x, a.z - b.z);
}

double horizontal_magnitude(const scraperx::sim::Vector3 &value) {
    return std::hypot(value.x, value.z);
}

double horizontal_dot(const scraperx::sim::Vector3 &a,
                      const scraperx::sim::Vector3 &b) {
    return a.x * b.x + a.z * b.z;
}

// Steps the authoritative clock one fixed step at a time until the predicate
// holds against a real snapshot, or the budget expires. Tests never reach into
// the simulation to force a state.
template <typename Predicate>
bool advance_until(scraperx::sim::Simulation &simulation,
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
double walk_toward(scraperx::sim::Simulation &simulation,
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

scraperx::sim::Snapshot run_mantle_command_stream(const bool single_fixed_steps) {
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
PathWatch g_path_watch;

void observe_path(const scraperx::sim::Simulation &simulation, const bool measure) {
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
void steer_toward(scraperx::sim::Simulation &simulation, const double x, const double z,
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
bool walk_to(scraperx::sim::Simulation &simulation, const double x, const double z,
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

double kit_y(const scraperx::sim::Simulation &simulation, const std::uint64_t entity) {
    return simulation.kit_body_position(simulation.kit_body_index(entity)).y;
}

// From the 154 m deck into Stage A's cage through its open north side,
// east of the trip handle hanging in front of it.
bool board_well_a(scraperx::sim::Simulation &simulation) {
    return walk_to(simulation, -10.0, -129.2, 5.0) && walk_to(simulation, -10.2, -130.6, 4.0);
}

// Inside the cage: face the bollard south-west and take the rope's end off
// it; face the cage's eye west and hook it on. Every step is the rig verb
// the snapshot offers at that moment, never a forced state.
bool rig_well_a(scraperx::sim::Simulation &simulation) {
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
bool pull_well_a(scraperx::sim::Simulation &simulation, const double pull, const double seconds) {
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

double kit_com_y(const scraperx::sim::Simulation &simulation, const std::uint64_t entity) {
    return simulation.kit_body_center_of_mass(simulation.kit_body_index(entity)).y;
}

double kit_com_z(const scraperx::sim::Simulation &simulation, const std::uint64_t entity) {
    return simulation.kit_body_center_of_mass(simulation.kit_body_index(entity)).z;
}

// From inside a cage: stand at (ux, uz) facing (ufx, ufz) and take the rope's
// end off where it is made fast; step back into the cage so the held end
// clears the posts; stand at (hx, hz) facing (hfx, hfz) and hook it on the
// eye. Every step is the rig verb the snapshot offers at that moment.
bool rig_end(scraperx::sim::Simulation &simulation, const double ux, const double uz,
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
bool pull_handle(scraperx::sim::Simulation &simulation, const double x, const double z,
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
bool rig_well_b(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return rig_end(simulation, -6.3, -130.35, 0.0, 1.0, Simulation::kWellBShackleEntityId, -6.35,
                   -131.40, 1.0, 0.0, Simulation::kWellBCageEntityId) &&
           simulation.snapshot().well_b_rope_end_entity_id == Simulation::kWellBCageEntityId;
}

bool pull_well_b(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_handle(simulation, -8.0, -130.25, Simulation::kWellBHandleEntityId, 0.5, 3.0,
                       [](const scraperx::sim::Snapshot &state) {
                           return !state.well_b_catch_latched;
                       });
}

// Stage C from its platform: throw the rebar clear of the chute's mouth (a
// full pull takes it over upright), then wait for the dumpster to fill.
bool clear_well_c_chute(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_handle(simulation, -4.4, -131.6, Simulation::kWellCHandleEntityId, 0.5, 3.0,
                       [](const scraperx::sim::Snapshot &state) {
                           return state.well_c_rebar_angle > 1.7;
                       });
}

bool fill_well_c(scraperx::sim::Simulation &simulation, const double seconds) {
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

bool pull_well_c_latch(scraperx::sim::Simulation &simulation, const double seconds) {
    using scraperx::sim::Simulation;
    return pull_handle(simulation, -3.0, -131.6, Simulation::kWellCLatchHandleEntityId, 0.5,
                       seconds, [](const scraperx::sim::Snapshot &state) {
                           return !state.well_c_catch_latched;
                       });
}

// Waits up to `seconds` for done(snapshot).
template <typename Done>
bool wait_for(scraperx::sim::Simulation &simulation, const double seconds, Done done) {
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
bool hold_stick(scraperx::sim::Simulation &simulation, const double x, const double z,
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

double horizontal_speed(const scraperx::sim::Snapshot &state) {
    return std::hypot(state.player_linear_velocity.x, state.player_linear_velocity.z);
}

bool is_climbing(const scraperx::sim::Snapshot &state) {
    return state.traversal_state == scraperx::sim::TraversalState::Climbing;
}

bool standing_above(const scraperx::sim::Snapshot &state, const double y) {
    return state.traversal_state == scraperx::sim::TraversalState::None &&
           state.player_grounded && state.player_position.y > y;
}

// AS-006's climbing route, a leg at a time, on player inputs. Each returns
// false as soon as a leg fails.
//
// From the 154 m deck: round the ladder's west side to its foot, take hold
// (Action) and climb until the top-out mantles onto the 176 ring.
bool climb_route_ladder(scraperx::sim::Simulation &simulation) {
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
bool climb_route_pipe(scraperx::sim::Simulation &simulation) {
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
bool climb_route_panel(scraperx::sim::Simulation &simulation) {
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

} // namespace


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

// D's spool, from where it lies on the 220 ring, carried north and set down
// between the guides across its gap.
bool seat_wet_spool(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, 9.5, -130.6, 8.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carry_target_entity_id != Simulation::kWetDSpoolEntityId) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carrying_entity_id != Simulation::kWetDSpoolEntityId) {
        return false;
    }
    for (std::uint32_t tick = 0; tick < 90 * 6; ++tick) {
        if (simulation.snapshot().player_position.z >= -128.25) {
            break;
        }
        (void)simulation.set_move_input(0.0, 0.35);
        (void)simulation.set_facing(0.0, 1.0);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)simulation.set_move_input(0.0, 0.0);
    (void)simulation.advance_frame(0.6);
    (void)simulation.request_set_down();
    (void)simulation.advance_frame(1.5);
    return simulation.wet_state().d_pipe_whole;
}

// From the 220 ring to D's platform by its walkway, east of the fill bar's
// handle hanging over it, then throw the bar.
bool throw_wet_fill(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, 13.4, -129.6, 8.0) || !walk_to(simulation, 13.4, -132.7, 4.0)) {
        return false;
    }
    return pull_facing(simulation, 12.3, -132.7, 0.0, 1.0, Simulation::kWetDValveHandleEntityId,
                       0.5, 4.0, [&](const scraperx::sim::Snapshot &) {
                           return simulation.wet_state().d_valve_angle > 1.8;
                       });
}

// Inside E's cab, east of its door (it opens into the cab): wait for the
// leaf to stop swinging, take its handle and draw it east until the latch
// drops.
bool shut_wet_door(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, 13.05, -136.4, 6.0, 0.08)) {
        return false;
    }
    const auto door_index = simulation.kit_body_index(Simulation::kWetEDoorEntityId);
    for (std::uint32_t tick = 0; tick < 90 * 4; ++tick) {
        const auto v = simulation.kit_body_velocity(door_index);
        if (std::hypot(v.x, v.z) < 0.2) {
            break;
        }
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    }
    const auto door = simulation.kit_body_center_of_mass(door_index);
    const auto here = simulation.snapshot().player_position;
    const double dx = door.x - here.x;
    const double dz = door.z - here.z;
    const double length = std::hypot(dx, dz);
    (void)simulation.set_facing(dx / length, dz / length);
    (void)simulation.advance_frame(0.6);
    if (simulation.snapshot().carry_target_entity_id != Simulation::kWetEDoorEntityId) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.3);
    bool latched = false;
    for (std::uint32_t tick = 0; tick < 90 * 6 && !latched; ++tick) {
        (void)simulation.set_move_input(0.4, 0.0);
        (void)simulation.set_facing(dx / length, dz / length);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        latched = simulation.wet_state().e_door_latched;
    }
    (void)simulation.set_move_input(0.0, 0.0);
    if (simulation.snapshot().carrying_entity_id != 0) {
        (void)simulation.request_set_down();
    }
    (void)simulation.advance_frame(0.3);
    return latched;
}

// The chiller's trip handle hangs inside the cab's east wall.
bool pull_wet_trip(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_facing(simulation, 13.4, -136.65, 1.0, 0.0, Simulation::kWetEHandleEntityId, 0.5,
                       4.0, [&](const scraperx::sim::Snapshot &) {
                           return !simulation.wet_state().e_catch_latched;
                       });
}

// On F's platform: the hose off the deck and onto the ram's inlet on the
// platform's west rail.
bool couple_wet_hose(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, 12.6, -139.95, 6.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(-0.3, -0.95);
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carry_target_entity_id != Simulation::kWetFHoseEntityId) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carrying_entity_id != Simulation::kWetFHoseEntityId ||
        !walk_to(simulation, 12.4, -139.55, 4.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(-0.6, 0.8);
    (void)simulation.advance_frame(1.2);
    const auto at_inlet = simulation.snapshot();
    if (at_inlet.rig_action != 1 ||
        at_inlet.rig_target_entity_id != Simulation::kWetFPlatformEntityId) {
        return false;
    }
    (void)simulation.request_rig();
    (void)simulation.advance_frame(0.3);
    return simulation.wet_state().f_hose_coupled;
}

// F's stop valve's handle hangs off the platform's south rail.
bool pull_wet_stop_valve(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_facing(simulation, 12.25, -140.7, 0.0, -1.0, Simulation::kWetFHandleEntityId, 0.5,
                       4.0, [&](const scraperx::sim::Snapshot &) {
                           return !simulation.wet_state().f_catch_latched;
                       });
}

bool on_support(const scraperx::sim::Snapshot &state, const std::uint64_t entity) {
    return state.player_grounded && state.support_entity_id == entity;
}

void run_wet_isolation() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    constexpr double kG = 9.81;

    // ---- D: no spool, no water -----------------------------------------------
    Simulation d_dry(InitialSpawn::Ring220North);
    (void)d_dry.advance_frame(1.0);
    require(!d_dry.wet_state().d_pipe_whole, "as found, D's spool lies off its gap");
    require(throw_wet_fill(d_dry), "the rider must throw D's fill bar from the platform");
    (void)d_dry.advance_frame(10.0);
    require(d_dry.wet_state().d_platform_travel < 0.02 && d_dry.wet_state().d_tube_kg <= 0.0,
            "with the spool on the deck no water runs and D's platform stays down");

    // ---- D: the spool seated, the ride -----------------------------------------
    Simulation d_ride(InitialSpawn::Ring220North);
    (void)d_ride.advance_frame(1.0);
    require(seat_wet_spool(d_ride), "the rider must carry D's spool into its gap");
    const auto d_before = d_ride.wet_state();
    require(throw_wet_fill(d_ride), "the rider must throw D's fill bar with the spool seated");
    const double d_rider_y0 = d_ride.snapshot().player_position.y;
    const bool d_arrived = wait_for(d_ride, 120.0, [&](const scraperx::sim::Snapshot &) {
        return d_ride.wet_state().d_platform_travel >= 35.95;
    });
    (void)d_ride.advance_frame(1.0);
    const auto d_after = d_ride.wet_state();
    const auto d_top = d_ride.snapshot();
    require(d_arrived, "D's float must carry the platform to its stop at 256.25");
    require(on_support(d_top, Simulation::kWetDPlatformEntityId),
            "the rider must ride D's platform all the way");
    // Released: the water's fall, tank to tube, from each pool's centre of
    // mass (the float's displacement moves the tube's by centimetres).
    const auto pool_pe = [&](const double kg, const double floor, const double level) {
        return kg * kG * 0.5 * (floor + level);
    };
    const double d_gain = kRiderMassKg * kG * (d_top.player_position.y - d_rider_y0);
    const double d_released =
        pool_pe(d_before.d_tank_kg, 227.0, d_before.d_tank_level) +
        pool_pe(d_before.d_tube_kg, 181.0, d_before.d_tube_level) -
        pool_pe(d_after.d_tank_kg, 227.0, d_after.d_tank_level) -
        pool_pe(d_after.d_tube_kg, 181.0, d_after.d_tube_level) -
        (d_after.drained_kg - d_before.drained_kg) * kG * 219.0;
    require(d_gain > 0.0 && d_gain <= d_released,
            "D's rider must never gain more than the water released");
    std::cout << "PASS scraperx_sim AS-007 D: rider_y=" << d_top.player_position.y
              << " tank_level=" << d_after.d_tank_level << " tube_level=" << d_after.d_tube_level
              << " gain_J=" << d_gain << " released_J=" << d_released << '\n';

    // ---- E: door open, the air leaves by it ---------------------------------------
    Simulation e_open(InitialSpawn::WetECab);
    (void)e_open.advance_frame(1.0);
    require(!e_open.wet_state().e_door_latched && e_open.wet_state().e_catch_latched,
            "as found, E's door stands open and the chiller hangs in its catch");
    require(pull_wet_trip(e_open), "the rider must trip the chiller from the cab");
    (void)e_open.advance_frame(15.0);
    require(e_open.wet_state().e_cab_travel < 0.05 && e_open.wet_state().e_chiller_travel < -10.0,
            "with the door open the chiller falls and the cab stays on its stop");

    // ---- E: door shut, the ride ------------------------------------------------------
    Simulation e_ride(InitialSpawn::WetECab);
    (void)e_ride.advance_frame(1.0);
    require(shut_wet_door(e_ride), "the rider must swing E's door shut until it latches");
    const double e_rider_y0 = e_ride.snapshot().player_position.y;
    const double e_chiller_y0 = kit_y(e_ride, Simulation::kWetEChillerEntityId);
    const double e_bucket_y0 = kit_y(e_ride, Simulation::kWetEBucketEntityId);
    require(pull_wet_trip(e_ride), "the rider must trip the chiller with the door shut");
    const bool e_arrived = wait_for(e_ride, 90.0, [&](const scraperx::sim::Snapshot &) {
        return e_ride.wet_state().e_cab_travel >= 41.95;
    });
    (void)e_ride.advance_frame(1.0);
    const auto e_top = e_ride.snapshot();
    require(e_arrived, "the chiller's air must lift E's cab to its stop at 298.25");
    require(on_support(e_top, Simulation::kWetECabEntityId),
            "the rider must ride E's cab all the way");
    const double e_gain = kRiderMassKg * kG * (e_top.player_position.y - e_rider_y0);
    const double e_released =
        5000.0 * kG * (e_chiller_y0 - kit_y(e_ride, Simulation::kWetEChillerEntityId)) -
        300.0 * kG * (kit_y(e_ride, Simulation::kWetEBucketEntityId) - e_bucket_y0);
    require(e_gain > 0.0 && e_gain <= e_released,
            "E's rider must never gain more than the chiller released");
    std::cout << "PASS scraperx_sim AS-007 E: rider_y=" << e_top.player_position.y
              << " cab_pa=" << e_ride.wet_state().e_cab_pa << " gain_J=" << e_gain
              << " released_J=" << e_released << '\n';

    // ---- F: hose free, the accumulator dumps -----------------------------------------
    Simulation f_free(InitialSpawn::WetFPlatform);
    (void)f_free.advance_frame(1.0);
    require(!f_free.wet_state().f_hose_coupled && f_free.wet_state().f_catch_latched,
            "as found, F's hose lies free and the accumulator stands in its catch");
    require(pull_wet_stop_valve(f_free), "the rider must throw F's stop valve");
    (void)f_free.advance_frame(40.0);
    require(f_free.wet_state().f_platform_travel < 0.05 &&
                f_free.wet_state().f_accumulator_travel < -2.9,
            "with the hose free the accumulator dumps and F's platform stays");

    // ---- F: hose coupled, the ride ----------------------------------------------------
    Simulation f_ride(InitialSpawn::WetFPlatform);
    (void)f_ride.advance_frame(1.0);
    require(couple_wet_hose(f_ride), "the rider must couple F's hose to the ram's inlet");
    const double f_rider_y0 = f_ride.snapshot().player_position.y;
    const double f_acc_y0 = kit_y(f_ride, Simulation::kWetFAccumulatorEntityId);
    require(pull_wet_stop_valve(f_ride), "the rider must throw F's stop valve, coupled");
    const bool f_arrived = wait_for(f_ride, 90.0, [&](const scraperx::sim::Snapshot &) {
        return f_ride.wet_state().f_platform_travel >= 41.95;
    });
    (void)f_ride.advance_frame(1.0);
    const auto f_top = f_ride.snapshot();
    require(f_arrived, "the accumulator must lift F's platform to TP-340");
    require(on_support(f_top, Simulation::kWetFPlatformEntityId),
            "the rider must ride F's platform all the way");
    const double f_gain = kRiderMassKg * kG * (f_top.player_position.y - f_rider_y0);
    const double f_released =
        20000.0 * kG * (f_acc_y0 - kit_y(f_ride, Simulation::kWetFAccumulatorEntityId));
    require(f_gain > 0.0 && f_gain <= f_released,
            "F's rider must never gain more than the accumulator released");
    std::cout << "PASS scraperx_sim AS-007 F: rider_y=" << f_top.player_position.y
              << " gain_J=" << f_gain << " released_J=" << f_released << '\n';
}

// The band in one run on player inputs, from the 220 ring to standing on
// TP-340, then the header's dump re-arms it.
void run_wet_band() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation band(InitialSpawn::Ring220North);
    (void)band.advance_frame(1.0);
    const double start = band.snapshot().simulation_time_seconds;
    require(seat_wet_spool(band), "band: the spool into its gap");
    require(throw_wet_fill(band), "band: D's fill bar thrown from the platform");
    require(wait_for(band, 120.0,
                     [&](const scraperx::sim::Snapshot &) {
                         return band.wet_state().d_platform_travel >= 35.95;
                     }) &&
                on_support(band.snapshot(), Simulation::kWetDPlatformEntityId),
            "band: D carries the rider to 256.25");
    require(walk_to(band, 13.2, -134.5, 6.0) && walk_to(band, 13.2, -135.8, 6.0),
            "band: from D's platform through E's door into the cab");
    require(shut_wet_door(band), "band: E's door shut behind the rider");
    require(pull_wet_trip(band), "band: E's chiller tripped");
    require(wait_for(band, 90.0,
                     [&](const scraperx::sim::Snapshot &) {
                         return band.wet_state().e_cab_travel >= 41.95;
                     }) &&
                on_support(band.snapshot(), Simulation::kWetECabEntityId),
            "band: E carries the rider to 298.25");
    require(walk_to(band, 13.4, -137.2, 4.0) && walk_to(band, 13.4, -139.9, 6.0),
            "band: from E's cab over the rim onto F's platform");
    require(couple_wet_hose(band), "band: F's hose coupled");
    require(pull_wet_stop_valve(band), "band: F's stop valve thrown");
    require(wait_for(band, 90.0,
                     [&](const scraperx::sim::Snapshot &) {
                         return band.wet_state().f_platform_travel >= 41.95;
                     }) &&
                on_support(band.snapshot(), Simulation::kWetFPlatformEntityId),
            "band: F carries the rider to TP-340");
    require(walk_to(band, 13.0, -137.2, 6.0), "band: off F's platform onto TP-340");
    (void)band.advance_frame(0.5);
    const auto on_plate = band.snapshot();
    require(on_plate.player_grounded && on_plate.player_position.y > 340.25 &&
                on_plate.support_entity_id == Simulation::kWetFrameEntityId,
            "band: the rider stands on TP-340");
    const double to_plate = on_plate.simulation_time_seconds - start;

    // The cascade: the header's dump thrown over.
    require(pull_facing(band, 11.6, -137.3, -1.0, 0.0, Simulation::kWetHeaderHandleEntityId, 0.5,
                        4.0,
                        [&](const scraperx::sim::Snapshot &) {
                            return band.wet_state().dump_angle > 1.8;
                        }),
            "band: the header's dump thrown");
    const bool rearmed = wait_for(band, 150.0, [&](const scraperx::sim::Snapshot &) {
        const auto w = band.wet_state();
        return w.e_catch_latched && w.f_catch_latched && w.f_accumulator_travel >= -0.05 &&
               w.d_tank_kg >= 240000.0 && w.e_cab_travel <= 0.05 && w.f_platform_travel <= 0.05;
    });
    const auto w = band.wet_state();
    require(rearmed, "band: the dump must hoist the chiller back, recharge the accumulator and "
                     "refill D's tank, bringing E's cab and F's platform back down");
    std::cout << "PASS scraperx_sim AS-007 band: to_340_s=" << to_plate
              << " plate_y=" << on_plate.player_position.y << " tank_kg=" << w.d_tank_kg
              << " header_kg=" << w.header_kg << " bucket_kg=" << w.e_bucket_kg << '\n';
}

// AS-007's climbing route: at (x, z) facing (fx, fz), take the hold in front
// (jumping for it when `jump`) and climb, stick toward it, until standing
// above top.
bool climb_wet_hold(scraperx::sim::Simulation &simulation, const double x, const double z,
                    const double fx, const double fz, const bool jump, const double top) {
    if (!walk_to(simulation, x, z, 8.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(fx, fz);
    (void)simulation.advance_frame(0.4);
    if (jump) {
        (void)simulation.request_jump();
        if (!hold_stick(simulation, 0.6 * fx, 0.6 * fz, fx, fz, 3.0,
                        [](const scraperx::sim::Snapshot &state) { return is_climbing(state); })) {
            return false;
        }
    } else {
        if (!simulation.snapshot().grip_available) {
            return false;
        }
        (void)simulation.request_traversal();
        (void)simulation.advance_frame(0.2);
        if (!is_climbing(simulation.snapshot())) {
            return false;
        }
    }
    return hold_stick(simulation, fx, fz, fx, fz, 60.0,
                      [top](const scraperx::sim::Snapshot &state) { return standing_above(state, top); });
}

void run_wet_route() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::TraversalState;
    Simulation route(InitialSpawn::Ring220North);
    (void)route.advance_frame(1.0);
    const double start = route.snapshot().simulation_time_seconds;
    // 220 -> 242: the L of boards, the ladder.
    require(walk_to(route, 3.5, -130.5, 6.0, 0.1) && walk_to(route, 3.5, -132.4, 6.0, 0.1) &&
                climb_wet_hold(route, 2.5, -132.4, 0.0, 1.0, false, 242.5),
            "route: boards and ladder from the 220 ring to the 242");
    // 242 -> 264: the beam, the panel caught with a jump.
    require(walk_to(route, -1.0, -131.2, 8.0, 0.1) &&
                climb_wet_hold(route, -1.0, -133.2, 0.0, 1.0, true, 264.5),
            "route: the beam and the jump to the panel, to the 264 ring");
    // 264: the bulkhead leaves only the lip. Back to the edge, drop into a
    // hang, shimmy under it, climb back up.
    require(walk_to(route, 1.2, -131.4, 6.0, 0.08), "route: west of the bulkhead on the 264 ring");
    (void)route.set_facing(0.0, 1.0);
    (void)hold_stick(route, 0.0, -0.4, 0.0, 1.0, 2.0, [](const scraperx::sim::Snapshot &state) {
        return state.player_position.z <= -132.09;
    });
    (void)route.advance_frame(0.3);
    require(route.snapshot().edge_drop_available, "route: the 264 ring's edge offers a drop");
    (void)route.request_release();
    require(wait_for(route, 1.5,
                     [](const scraperx::sim::Snapshot &state) {
                         return state.traversal_state == TraversalState::Hanging;
                     }),
            "route: the drop lowers the body into a hang on the lip");
    require(hold_stick(route, 1.0, 0.0, 0.0, 1.0, 6.0,
                       [](const scraperx::sim::Snapshot &state) { return state.player_position.x >= 3.45; }),
            "route: the shimmy carries the body under the bulkhead");
    (void)route.request_jump();
    require(wait_for(route, 2.5,
                     [](const scraperx::sim::Snapshot &state) { return standing_above(state, 264.5); }),
            "route: up from the hang east of the bulkhead");
    // 264 -> 286: the L of boards, the standpipe.
    require(walk_to(route, 5.35, -132.2, 6.0, 0.1) && walk_to(route, 5.35, -134.4, 6.0, 0.1) &&
                walk_to(route, 5.0, -134.4, 6.0, 0.1) &&
                climb_wet_hold(route, 5.0, -134.2, 0.0, 1.0, false, 286.5),
            "route: boards and standpipe to the 286 ring");
    // 286 -> 308: the L of boards, the ladder.
    require(walk_to(route, 4.0, -133.0, 6.0, 0.1) && walk_to(route, 4.0, -135.1, 6.0, 0.1) &&
                climb_wet_hold(route, 3.0, -135.1, 0.0, 1.0, false, 308.5),
            "route: boards and ladder to the 308 ring");
    // 308 -> 330: the catwalk, the panel caught with a jump.
    require(walk_to(route, 1.5, -134.0, 6.0, 0.1) &&
                climb_wet_hold(route, 1.5, -135.95, 0.0, 1.0, true, 330.5),
            "route: the catwalk and the jump to the panel, to the 330 ring");
    // 330 -> TP-340: the ladder on the plate's north face, facing south.
    require(climb_wet_hold(route, 3.0, -134.9, 0.0, -1.0, false, 340.5),
            "route: the ladder on TP-340's face, onto the plate");
    const auto top = route.snapshot();
    const auto w = route.wet_state();
    require(w.d_platform_travel < 0.02 && w.e_cab_travel < 0.05 && w.f_platform_travel < 0.05 &&
                w.e_catch_latched && w.f_catch_latched,
            "route: no lift in the band moved");
    std::cout << "PASS scraperx_sim AS-007 route: seconds=" << top.simulation_time_seconds - start
              << " top_y=" << top.player_position.y << " lifts_untouched=1\n";
}

// ---- AS-008, the Plate Shop (340 -> 484 m) ------------------------------------

// From TP-340's north side by F's hole, west past the header, to the south of
// G's cleat.
bool walk_to_shop_cleat(scraperx::sim::Simulation &simulation) {
    return walk_to(simulation, 11.3, -138.0, 6.0) && walk_to(simulation, 1.0, -141.0, 16.0) &&
           walk_to(simulation, -5.0, -153.5, 12.0) && walk_to(simulation, -9.7, -152.9, 8.0, 0.08);
}

// From TP-340's north side into G's shaft between its posts and west onto the
// platform.
bool board_shop_g(scraperx::sim::Simulation &simulation) {
    return walk_to(simulation, 11.3, -138.0, 6.0) && walk_to(simulation, 1.0, -141.0, 16.0) &&
           walk_to(simulation, -8.0, -147.0, 12.0) && walk_to(simulation, -8.0, -150.0, 4.0) &&
           walk_to(simulation, -11.2, -150.0, 6.0, 0.08);
}

// G's rope off its cleat, carried round the shaft's posts onto the platform
// and hooked on the platform's eye.
bool rig_shop_g(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(0.4);
    const auto at_cleat = simulation.snapshot();
    if (at_cleat.rig_action != 2 || at_cleat.rig_target_entity_id != Simulation::kShopGShackleEntityId) {
        return false;
    }
    (void)simulation.request_rig();
    (void)simulation.advance_frame(0.3);
    if (simulation.snapshot().carrying_entity_id != Simulation::kShopGShackleEntityId ||
        !(walk_to(simulation, -8.5, -152.9, 4.0) && walk_to(simulation, -8.5, -150.1, 4.0) &&
          walk_to(simulation, -11.2, -150.0, 6.0, 0.08))) {
        return false;
    }
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(1.5);
    const auto at_eye = simulation.snapshot();
    if (at_eye.rig_action != 1 || at_eye.rig_target_entity_id != Simulation::kShopGPlatformEntityId) {
        return false;
    }
    (void)simulation.request_rig();
    (void)simulation.advance_frame(0.3);
    return simulation.shop_state().g_rope_on_eye;
}

// G's prop pin, drawn by its lanyard's handle over the platform's north rail.
bool pull_shop_g(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_facing(simulation, -11.3, -149.25, 0.0, 1.0, Simulation::kShopGHandleEntityId, 0.5, 4.0,
                       [&](const scraperx::sim::Snapshot &) {
                           return !simulation.shop_state().g_tower_latched;
                       });
}

// From the 374 ring's west band round to its north band, the girder's tail
// pin carried out of its socket and set down.
bool pull_shop_girder_pin(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!(walk_to(simulation, -14.5, -134.3, 16.0) && walk_to(simulation, -2.8, -134.0, 16.0) &&
          walk_to(simulation, -3.6, -134.0, 4.0, 0.08))) {
        return false;
    }
    (void)simulation.set_facing(-1.0, 0.0);
    (void)simulation.advance_frame(0.5);
    if (simulation.snapshot().carry_target_entity_id != Simulation::kShopHGirderPinEntityId) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carrying_entity_id != Simulation::kShopHGirderPinEntityId ||
        !walk_to(simulation, -2.6, -134.0, 4.0)) {
        return false;
    }
    (void)simulation.request_set_down();
    (void)simulation.advance_frame(1.0);
    return !simulation.shop_state().h_girder_latched;
}

// Round the ring to the gangway, over it onto H's platform.
bool board_shop_h(scraperx::sim::Simulation &simulation) {
    return walk_to(simulation, -14.5, -134.3, 16.0) && walk_to(simulation, -14.5, -145.6, 16.0) &&
           walk_to(simulation, -9.6, -145.6, 10.0);
}

// H's chock, yanked by its lanyard's handle over the platform's north rail.
bool pull_shop_chock(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_facing(simulation, -9.4, -144.9, 0.0, 1.0, Simulation::kShopHHandleEntityId, 0.5, 4.0,
                       [&](const scraperx::sim::Snapshot &) {
                           return !simulation.shop_state().h_trolley_latched;
                       });
}

// On I's cage: the rope's shackle, hanging a metre west of the eye, onto it.
bool rig_shop_i(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, -7.3, -145.6, 6.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carry_target_entity_id != Simulation::kShopIShackleEntityId) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carrying_entity_id != Simulation::kShopIShackleEntityId ||
        !walk_to(simulation, -6.3, -145.6, 4.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(0.8);
    const auto at_eye = simulation.snapshot();
    if (at_eye.rig_action != 1 || at_eye.rig_target_entity_id != Simulation::kShopICageEntityId) {
        return false;
    }
    (void)simulation.request_rig();
    (void)simulation.advance_frame(0.3);
    return simulation.shop_state().i_rope_on_eye;
}

// The domino's pin, drawn by its lanyard's handle over the cage's north rail.
bool pull_shop_domino(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_facing(simulation, -6.5, -144.9, 0.0, 1.0, Simulation::kShopIHandleEntityId, 0.5, 4.0,
                       [&](const scraperx::sim::Snapshot &) {
                           return !simulation.shop_state().i_domino_latched;
                       });
}

void run_plate_shop() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    constexpr double kG = 9.81;

    // ---- G: the rope on its cleat, the tower hangs on it ------------------------------
    Simulation g_fast(InitialSpawn::PlateTop);
    (void)g_fast.advance_frame(1.0);
    require(!g_fast.shop_state().g_rope_on_eye && g_fast.shop_state().g_tower_latched,
            "as found, G's rope is made fast on its cleat and the tower stands pinned");
    require(board_shop_g(g_fast) && pull_shop_g(g_fast), "the rider must pull G's pin from the platform");
    (void)g_fast.advance_frame(10.0);
    require(g_fast.shop_state().g_platform_travel < 0.02 && g_fast.shop_state().g_tower_travel > -0.2,
            "with the rope on its cleat the tower hangs on it and G's platform stays");

    // ---- G: the rope on the eye, the ride ------------------------------------------
    Simulation g_ride(InitialSpawn::PlateTop);
    (void)g_ride.advance_frame(1.0);
    require(walk_to_shop_cleat(g_ride) && rig_shop_g(g_ride),
            "the rider must take G's rope off its cleat and hook it on the platform's eye");
    const double g_rider_y0 = g_ride.snapshot().player_position.y;
    const double g_tower_y0 = kit_com_y(g_ride, Simulation::kShopGTowerEntityId);
    require(pull_shop_g(g_ride), "the rider must pull G's pin, the rope on the eye");
    const bool g_arrived = wait_for(g_ride, 60.0, [&](const scraperx::sim::Snapshot &) {
        return g_ride.shop_state().g_platform_travel >= 33.7;
    });
    (void)g_ride.advance_frame(1.0);
    const auto g_top = g_ride.snapshot();
    require(g_arrived, "the slumping tower must lift G's platform to the 374 ring");
    require(on_support(g_top, Simulation::kShopGPlatformEntityId), "the rider must ride G's platform all the way");
    const double g_gain = kRiderMassKg * kG * (g_top.player_position.y - g_rider_y0);
    const double g_released = 3000.0 * kG * (g_tower_y0 - kit_com_y(g_ride, Simulation::kShopGTowerEntityId));
    require(g_gain > 0.0 && g_gain <= g_released, "G's rider must never gain more than the tower released");
    std::cout << "PASS scraperx_sim AS-008 G: rider_y=" << g_top.player_position.y << " gain_J=" << g_gain
              << " released_J=" << g_released << '\n';

    // ---- H: the girder's tail pinned, the girder stays -------------------------------
    Simulation h_pinned(InitialSpawn::Ring374West);
    (void)h_pinned.advance_frame(1.0);
    require(h_pinned.shop_state().h_girder_latched && h_pinned.shop_state().h_trolley_latched,
            "as found, H's girder is pinned down by its tail and the trolley stands chocked");
    require(walk_to(h_pinned, -9.6, -145.6, 10.0) && pull_shop_chock(h_pinned),
            "the rider must yank H's chock from the platform");
    (void)h_pinned.advance_frame(15.0);
    require(h_pinned.shop_state().h_platform_travel < 0.05 && h_pinned.shop_state().h_girder_angle < 0.05,
            "with the tail pinned the trolley runs out, the girder stays and H's platform stays");

    // ---- H: the tail pin out, the ride --------------------------------------------------
    Simulation h_ride(InitialSpawn::Ring374West);
    (void)h_ride.advance_frame(1.0);
    require(pull_shop_girder_pin(h_ride), "the rider must carry the girder's tail pin out of its socket");
    require(board_shop_h(h_ride), "the rider must cross the gangway onto H's platform");
    const double h_rider_y0 = h_ride.snapshot().player_position.y;
    const double h_trolley_y0 = kit_com_y(h_ride, Simulation::kShopHTrolleyEntityId);
    const double h_girder_y0 = kit_com_y(h_ride, Simulation::kShopHGirderEntityId);
    require(pull_shop_chock(h_ride), "the rider must yank H's chock, the tail free");
    const bool h_arrived = wait_for(h_ride, 90.0, [&](const scraperx::sim::Snapshot &) {
        return h_ride.shop_state().h_platform_travel >= 43.9;
    });
    (void)h_ride.advance_frame(1.0);
    const auto h_top = h_ride.snapshot();
    require(h_arrived, "the tipping girder must haul H's platform to the 418 ring");
    require(on_support(h_top, Simulation::kShopHPlatformEntityId), "the rider must ride H's platform all the way");
    const double h_gain = kRiderMassKg * kG * (h_top.player_position.y - h_rider_y0);
    const double h_released =
        14000.0 * kG * (h_trolley_y0 - kit_com_y(h_ride, Simulation::kShopHTrolleyEntityId)) +
        4000.0 * kG * (h_girder_y0 - kit_com_y(h_ride, Simulation::kShopHGirderEntityId));
    require(h_gain > 0.0 && h_gain <= h_released, "H's rider must never gain more than the trolley and girder released");
    std::cout << "PASS scraperx_sim AS-008 H: rider_y=" << h_top.player_position.y
              << " girder_angle=" << h_ride.shop_state().h_girder_angle << " gain_J=" << h_gain
              << " released_J=" << h_released << '\n';

    // ---- I: the shackle free, the monolith falls for nothing --------------------------
    Simulation i_free(InitialSpawn::ShopICage);
    (void)i_free.advance_frame(1.0);
    require(!i_free.shop_state().i_rope_on_eye && i_free.shop_state().i_domino_latched &&
                i_free.shop_state().i_monolith_latched,
            "as found, I's shackle hangs free and the domino and monolith stand caught");
    require(pull_shop_domino(i_free), "the rider must pull the domino's pin from the cage");
    (void)i_free.advance_frame(20.0);
    require(!i_free.shop_state().i_monolith_latched && i_free.shop_state().i_monolith_angle > 0.9 &&
                i_free.shop_state().i_cage_travel < 0.05,
            "with the shackle free the domino trips the monolith, which falls, and I's cage stays");

    // ---- I: the shackle on the eye, the cascade and the ride ----------------------------
    Simulation i_ride(InitialSpawn::ShopICage);
    (void)i_ride.advance_frame(1.0);
    require(rig_shop_i(i_ride), "the rider must hook I's shackle on the cage's eye");
    const double i_rider_y0 = i_ride.snapshot().player_position.y;
    const double i_monolith_y0 = kit_com_y(i_ride, Simulation::kShopIMonolithEntityId);
    const double i_domino_y0 = kit_com_y(i_ride, Simulation::kShopIDominoEntityId);
    require(pull_shop_domino(i_ride), "the rider must pull the domino's pin, hooked on");
    const bool i_arrived = wait_for(i_ride, 90.0, [&](const scraperx::sim::Snapshot &) {
        return i_ride.shop_state().i_cage_travel >= 43.9;
    });
    (void)i_ride.advance_frame(1.0);
    const auto i_top = i_ride.snapshot();
    require(i_arrived, "the monolith's fall must haul I's cage to 462.25");
    require(on_support(i_top, Simulation::kShopICageEntityId), "the rider must ride I's cage all the way");
    const double i_gain = kRiderMassKg * kG * (i_top.player_position.y - i_rider_y0);
    const double i_released =
        20000.0 * kG * (i_monolith_y0 - kit_com_y(i_ride, Simulation::kShopIMonolithEntityId)) +
        800.0 * kG * (i_domino_y0 - kit_com_y(i_ride, Simulation::kShopIDominoEntityId));
    require(i_gain > 0.0 && i_gain <= i_released, "I's rider must never gain more than the beams released");
    std::cout << "PASS scraperx_sim AS-008 I: rider_y=" << i_top.player_position.y
              << " monolith_angle=" << i_ride.shop_state().i_monolith_angle << " gain_J=" << i_gain
              << " released_J=" << i_released << '\n';
}

// The band in one run on player inputs: from TP-340 by F's hole, G's rope
// onto its platform and its pin out, the ride to the 374 ring; round the ring
// for the girder's tail pin, over the gangway onto H, its chock, the ride to
// the 418 ring; across onto I's cage, its shackle on, the domino's pin, the
// cascade and the ride to 462; up the ladder onto the 484 ring.
void run_plate_band() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation band(InitialSpawn::PlateTop);
    (void)band.advance_frame(1.0);
    const double start = band.snapshot().simulation_time_seconds;
    require(walk_to_shop_cleat(band) && rig_shop_g(band) && pull_shop_g(band),
            "band: G rigged and its pin pulled from the platform");
    require(wait_for(band, 60.0,
                     [&](const scraperx::sim::Snapshot &) { return band.shop_state().g_platform_travel >= 33.7; }),
            "band: G's platform reaches the 374 ring");
    (void)band.advance_frame(1.0);
    require(on_support(band.snapshot(), Simulation::kShopGPlatformEntityId), "band: the rider rides G");
    require(walk_to(band, -14.5, -149.3, 8.0) && pull_shop_girder_pin(band),
            "band: off G onto the 374 ring and the girder's tail pin out");
    require(board_shop_h(band) && pull_shop_chock(band), "band: over the gangway onto H and its chock out");
    require(wait_for(band, 90.0,
                     [&](const scraperx::sim::Snapshot &) { return band.shop_state().h_platform_travel >= 43.9; }),
            "band: H's platform reaches the 418 ring");
    (void)band.advance_frame(1.0);
    require(on_support(band.snapshot(), Simulation::kShopHPlatformEntityId), "band: the rider rides H");
    require(rig_shop_i(band) && pull_shop_domino(band), "band: across onto I's cage, hooked on, the domino's pin out");
    require(wait_for(band, 90.0,
                     [&](const scraperx::sim::Snapshot &) { return band.shop_state().i_cage_travel >= 43.9; }),
            "band: the cascade hauls I's cage to 462.25");
    (void)band.advance_frame(1.0);
    require(on_support(band.snapshot(), Simulation::kShopICageEntityId), "band: the rider rides I");
    require(climb_wet_hold(band, -7.6, -145.6, -1.0, 0.0, false, 484.5),
            "band: up the ladder from the cage onto the 484 ring");
    const auto top = band.snapshot();
    std::cout << "PASS scraperx_sim AS-008 band: to_484_s=" << top.simulation_time_seconds - start
              << " ring_y=" << top.player_position.y << '\n';
}

// AS-008's climbing route, on player inputs: from TP-340 by F's hole round the
// hole and the header to the east band, then one climb per ring gap to the
// 484 ring, every lift in the band where it was found.
void run_plate_route() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation route(InitialSpawn::PlateTop);
    (void)route.advance_frame(1.0);
    const double start = route.snapshot().simulation_time_seconds;
    require(walk_to(route, 11.0, -138.0, 6.0) && walk_to(route, 10.4, -142.3, 8.0) &&
                walk_to(route, 13.07, -157.0, 12.0) &&
                climb_wet_hold(route, 13.07, -158.0, 1.0, 0.0, false, 352.5),
            "route: across TP-340 and the ladder onto the 352 ring");
    for (double h = 352.0; h < 483.9; h += 22.0) {
        const double s = 14.72 - 0.91 * (h - 330.0) / 22.0;
        const bool leg = walk_to(route, s + 0.25, -157.0, 12.0, 0.1) && walk_to(route, s - 1.67, -157.0, 8.0, 0.1) &&
                         climb_wet_hold(route, s - 1.67, -158.0, 1.0, 0.0, false, h + 22.5);
        if (!leg) {
            std::cout << "route: stuck above " << h << " at y=" << route.snapshot().player_position.y << '\n';
        }
        require(leg, "route: boards and a ladder to the next ring");
    }
    const auto top = route.snapshot();
    const auto shop = route.shop_state();
    require(shop.g_platform_travel < 0.02 && shop.h_platform_travel < 0.02 && shop.i_cage_travel < 0.02 &&
                shop.g_tower_latched && shop.h_girder_latched && shop.i_domino_latched,
            "route: no lift in the band moved");
    std::cout << "PASS scraperx_sim AS-008 route: seconds=" << top.simulation_time_seconds - start
              << " top_y=" << top.player_position.y << " lifts_untouched=1\n";
}

// Takes hold of the wreck `entity` faced from (x, z) along (fx, fz) and climbs
// it until standing above `top`; false as soon as the hold is not the wreck's.
bool climb_wreck(scraperx::sim::Simulation &simulation, const double x, const double z, const double fx,
                 const double fz, const std::uint64_t entity, const double top) {
    if (!walk_to(simulation, x, z, 10.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(fx, fz);
    (void)simulation.advance_frame(0.4);
    if (!simulation.snapshot().grip_available) {
        return false;
    }
    (void)simulation.request_traversal();
    (void)simulation.advance_frame(0.2);
    const auto hold = simulation.snapshot();
    if (!is_climbing(hold) || hold.traversal_support_entity_id != entity) {
        return false;
    }
    return hold_stick(simulation, fx, fz, fx, fz, 60.0,
                      [top](const scraperx::sim::Snapshot &state) { return standing_above(state, top); });
}

// One-shot wreckage is climbable (MECHANISM_ASCENT_PLAN.md §3 rule 6), on
// player inputs: each of AS-008's spent machines is reached and climbed.
void run_plate_wreckage() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;

    // ---- G, fired from the plate with no one aboard ---------------------------------
    Simulation g(InitialSpawn::PlateTop);
    (void)g.advance_frame(1.0);
    require(walk_to_shop_cleat(g) && rig_shop_g(g), "G wreck: its rope hooked on the platform's eye");
    require(walk_to(g, -8.3, -150.0, 6.0) && walk_to(g, -8.3, -147.6, 6.0) && walk_to(g, -11.3, -147.6, 6.0) &&
                pull_facing(g, -11.3, -147.8, 0.0, -1.0, Simulation::kShopGHandleEntityId, 0.5, 4.0,
                            [&](const scraperx::sim::Snapshot &) { return !g.shop_state().g_tower_latched; }),
            "G wreck: off the platform and its pin pulled from the plate");
    require(wait_for(g, 60.0, [&](const scraperx::sim::Snapshot &) { return g.shop_state().g_platform_travel >= 33.7; }) &&
                g.snapshot().player_position.y < 342.0,
            "G wreck: the tower slumps and lifts the platform without the rider");
    require(climb_wreck(g, -8.0, -147.9, 0.0, -1.0, Simulation::kShopGTowerEntityId, 374.3) &&
                g.snapshot().support_entity_id == Simulation::kShopGTowerEntityId,
            "G wreck: the slumped tower climbed from the plate to its deck");
    const double g_deck_y = g.snapshot().player_position.y;
    require(walk_to(g, -8.0, -150.0, 6.0) && walk_to(g, -11.3, -150.0, 8.0) && walk_to(g, -14.5, -150.0, 8.0) &&
                standing_above(g.snapshot(), 374.2),
            "G wreck: from the tower's deck over the platform onto the 374 ring");
    std::cout << "PASS scraperx_sim AS-008 wreckage G: deck_y=" << g_deck_y
              << " ring_y=" << g.snapshot().player_position.y << '\n';

    // ---- I, fired from the cage with its shackle free -----------------------------------
    Simulation i(InitialSpawn::ShopICage);
    (void)i.advance_frame(1.0);
    require(pull_shop_domino(i) && wait_for(i, 20.0,
                                            [&](const scraperx::sim::Snapshot &) {
                                                return i.shop_state().i_monolith_angle > 0.95;
                                            }) &&
                i.shop_state().i_cage_travel < 0.05,
            "I wreck: the cascade drops the monolith beside the cage, which stays");
    require(walk_to(i, -5.6, -145.6, 6.0, 0.08), "I wreck: to the cage's east side");
    (void)i.set_facing(1.0, 0.0);
    (void)i.advance_frame(0.4);
    (void)i.request_traversal();
    require(wait_for(i, 3.0,
                     [](const scraperx::sim::Snapshot &state) {
                         return state.support_entity_id == scraperx::sim::Simulation::kShopIMonolithEntityId &&
                                standing_above(state, 419.3);
                     }),
            "I wreck: up from the cage onto the fallen monolith");
    const double i_slab_y = i.snapshot().player_position.y;
    require(walk_to(i, -4.2, -138.2, 16.0) && walk_to(i, -4.2, -136.4, 6.0) &&
                wait_for(i, 2.0, [](const scraperx::sim::Snapshot &state) { return standing_above(state, 418.2); }) &&
                i.snapshot().player_position.y < 419.3,
            "I wreck: along the monolith and down onto the 418 ring");
    std::cout << "PASS scraperx_sim AS-008 wreckage I: slab_y=" << i_slab_y
              << " ring_y=" << i.snapshot().player_position.y << '\n';

    // ---- H, its chock pulled with the tail pinned, then the pin drawn from the ring ----
    Simulation h(InitialSpawn::Ring374West);
    (void)h.advance_frame(1.0);
    require(board_shop_h(h) && pull_shop_chock(h) && walk_to(h, -9.6, -145.6, 6.0) &&
                walk_to(h, -14.5, -145.6, 12.0) && pull_shop_girder_pin(h),
            "H wreck: the chock pulled, back over the gangway, the girder's tail pin drawn");
    require(wait_for(h, 60.0,
                     [&](const scraperx::sim::Snapshot &) { return h.shop_state().h_platform_travel >= 43.9; }) &&
                h.snapshot().player_position.y < 376.0,
            "H wreck: the girder tips and lifts the platform without the rider");
    require(walk_to(h, -6.6, -134.0, 8.0) &&
                climb_wreck(h, -6.6, -136.75, 0.0, -1.0, Simulation::kShopHGirderEntityId, 378.7) &&
                h.snapshot().support_entity_id == Simulation::kShopHGirderEntityId,
            "H wreck: the tipped girder's ladder climbed from the 374 ring onto its landing");
    std::cout << "PASS scraperx_sim AS-008 wreckage H: girder_angle=" << h.shop_state().h_girder_angle
              << " landing_y=" << h.snapshot().player_position.y << '\n';
}

// ---- AS-009, the Facade Crane Stack (484 -> 640 m) ------------------------------

// From the 484 ring's north band, the rail joint off the ring and laid in its
// cradle from J's traveler.
bool lay_crane_joint(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, 2.8, -139.0, 12.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carry_target_entity_id != Simulation::kCraneJJointEntityId) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carrying_entity_id != Simulation::kCraneJJointEntityId ||
        !(walk_to(simulation, -0.3, -138.3, 8.0) && walk_to(simulation, -0.3, -136.2, 6.0, 0.08) &&
          walk_to(simulation, 0.9, -136.2, 6.0, 0.08))) {
        return false;
    }
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(0.8);
    (void)simulation.request_set_down();
    (void)simulation.advance_frame(1.5);
    return simulation.crane_state().j_rail_whole;
}

// On J's traveler, the wagon's chock lever pulled over by its lanyard's
// handle beyond the traveler's north edge.
bool pull_crane_chock(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_facing(simulation, 0.0, -135.5, 0.0, 1.0, Simulation::kCraneJHandleEntityId, 0.5, 4.0,
                       [&](const scraperx::sim::Snapshot &) {
                           return !simulation.crane_state().j_wagon_latched;
                       });
}

// On K's cage: round its hanging shackle, take it, and hook it on the eye.
bool rig_crane_k(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    // Straight to the cage's north side, along it clear of the shackle on its
    // long rope, and round behind it.
    const double x0 = simulation.snapshot().player_position.x;
    if (!(walk_to(simulation, x0, -154.2, 6.0) && walk_to(simulation, -13.3, -154.2, 10.0) &&
          walk_to(simulation, -13.3, -155.0, 4.0, 0.08))) {
        return false;
    }
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carry_target_entity_id != Simulation::kCraneKShackleEntityId) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.4);
    if (simulation.snapshot().carrying_entity_id != Simulation::kCraneKShackleEntityId ||
        !walk_to(simulation, -12.3, -155.0, 4.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(0.8);
    const auto at_eye = simulation.snapshot();
    if (at_eye.rig_action != 1 || at_eye.rig_target_entity_id != Simulation::kCraneKCageEntityId) {
        return false;
    }
    (void)simulation.request_rig();
    (void)simulation.advance_frame(0.3);
    return simulation.crane_state().k_rope_on_eye;
}

// The jib's pendant pin, drawn by its lanyard's handle beyond the cage's
// north edge.
bool pull_crane_pendant(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_facing(simulation, -12.6, -154.3, 0.0, 1.0, Simulation::kCraneKHandleEntityId, 0.5, 4.0,
                       [&](const scraperx::sim::Snapshot &) {
                           return !simulation.crane_state().k_jib_latched;
                       });
}

// On L's cab: the winch's clutch lever thrown in by its handle beyond the
// cab's north edge.
bool throw_crane_clutch(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_facing(simulation, -3.7, -159.4, 0.0, 1.0, Simulation::kCraneLClutchHandleEntityId, 0.5, 4.0,
                       [&](const scraperx::sim::Snapshot &) { return simulation.crane_state().l_clutch_in; });
}

// The drop weight's pin, drawn by its lanyard's handle beyond the cab's south
// edge.
bool pull_crane_drop(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    return pull_facing(simulation, -3.0, -160.8, 0.0, -1.0, Simulation::kCraneLPinHandleEntityId, 0.5, 4.0,
                       [&](const scraperx::sim::Snapshot &) {
                           return !simulation.crane_state().l_weight_latched;
                       });
}

void run_facade_crane() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    constexpr double kG = 9.81;

    // ---- J: the joint out, the traveler cannot pass the gap ----------------------
    Simulation j_gap(InitialSpawn::Ring484North);
    (void)j_gap.advance_frame(1.0);
    require(!j_gap.crane_state().j_rail_whole && j_gap.crane_state().j_wagon_latched,
            "as found, J's rail is missing its joint and the wagon stands chocked");
    require(walk_to(j_gap, 0.0, -138.3, 8.0) && walk_to(j_gap, 0.0, -136.2, 6.0) && pull_crane_chock(j_gap),
            "the rider must pull the wagon's chock from the traveler");
    (void)j_gap.advance_frame(10.0);
    require(j_gap.crane_state().j_traveler_travel < 0.05 && j_gap.crane_state().j_wagon_travel < 0.3,
            "with the rail's joint out the traveler holds at the gap and the wagon on its rope");

    // ---- J: the joint in, the ride -----------------------------------------------
    Simulation j_ride(InitialSpawn::Ring484North);
    (void)j_ride.advance_frame(1.0);
    require(lay_crane_joint(j_ride), "the rider must lay the rail's joint in its cradle");
    const double j_rider_y0 = j_ride.snapshot().player_position.y;
    const double j_wagon_y0 = kit_com_y(j_ride, Simulation::kCraneJWagonEntityId);
    require(pull_crane_chock(j_ride), "the rider must pull the wagon's chock, the rail whole");
    const bool j_arrived = wait_for(j_ride, 60.0, [&](const scraperx::sim::Snapshot &) {
        return j_ride.crane_state().j_traveler_travel >= 43.9;
    });
    (void)j_ride.advance_frame(1.0);
    const auto j_top = j_ride.snapshot();
    require(j_arrived, "the runaway wagon must drag J's traveler to the 528 ring");
    require(on_support(j_top, Simulation::kCraneJTravelerEntityId), "the rider must ride J's traveler all the way");
    const double j_gain = kRiderMassKg * kG * (j_top.player_position.y - j_rider_y0);
    const double j_released = 4000.0 * kG * (j_wagon_y0 - kit_com_y(j_ride, Simulation::kCraneJWagonEntityId));
    require(j_gain > 0.0 && j_gain <= j_released, "J's rider must never gain more than the wagon released");
    std::cout << "PASS scraperx_sim AS-009 J: rider_y=" << j_top.player_position.y << " gain_J=" << j_gain
              << " released_J=" << j_released << '\n';

    // ---- K: the shackle free, the jib swings for nothing ----------------------------
    Simulation k_free(InitialSpawn::CraneKCage);
    (void)k_free.advance_frame(1.0);
    require(!k_free.crane_state().k_rope_on_eye && k_free.crane_state().k_jib_latched,
            "as found, K's shackle hangs free and the jib stands pinned level");
    require(pull_crane_pendant(k_free), "the rider must pull the pendant's pin from the cage");
    (void)k_free.advance_frame(20.0);
    require(k_free.crane_state().k_jib_angle > 1.3 && k_free.crane_state().k_cage_travel < 0.05,
            "with the shackle free the jib swings down and K's cage stays");

    // ---- K: hooked on, the ride ----------------------------------------------------
    Simulation k_ride(InitialSpawn::CraneKCage);
    (void)k_ride.advance_frame(1.0);
    require(rig_crane_k(k_ride), "the rider must hook K's shackle on the cage's eye");
    const double k_rider_y0 = k_ride.snapshot().player_position.y;
    const double k_jib_y0 = kit_com_y(k_ride, Simulation::kCraneKJibEntityId);
    require(pull_crane_pendant(k_ride), "the rider must pull the pendant's pin, hooked on");
    const bool k_arrived = wait_for(k_ride, 90.0, [&](const scraperx::sim::Snapshot &) {
        return k_ride.crane_state().k_cage_travel >= 43.9;
    });
    (void)k_ride.advance_frame(1.0);
    const auto k_top = k_ride.snapshot();
    require(k_arrived, "the jib's swing must haul K's cage to the 572 ring");
    require(on_support(k_top, Simulation::kCraneKCageEntityId), "the rider must ride K's cage all the way");
    const double k_gain = kRiderMassKg * kG * (k_top.player_position.y - k_rider_y0);
    const double k_released = 8000.0 * kG * (k_jib_y0 - kit_com_y(k_ride, Simulation::kCraneKJibEntityId));
    require(k_gain > 0.0 && k_gain <= k_released, "K's rider must never gain more than the jib released");
    std::cout << "PASS scraperx_sim AS-009 K: rider_y=" << k_top.player_position.y
              << " jib_angle=" << k_ride.crane_state().k_jib_angle << " gain_J=" << k_gain
              << " released_J=" << k_released << '\n';

    // ---- L: the clutch out, the cart runs for nothing ---------------------------------
    Simulation l_free(InitialSpawn::CraneLCab);
    (void)l_free.advance_frame(1.0);
    require(!l_free.crane_state().l_clutch_in && l_free.crane_state().l_cart_latched &&
                l_free.crane_state().l_weight_latched,
            "as found, L's clutch is out, the cart caught and the drop weight pinned");
    require(pull_crane_drop(l_free), "the rider must pull the drop weight's pin from the cab");
    (void)l_free.advance_frame(40.0);
    require(!l_free.crane_state().l_cart_latched && l_free.crane_state().l_cart_travel > 40.0 &&
                l_free.crane_state().l_cab_travel < 0.05,
            "with the clutch out the weight trips the cart, it runs down, and L's cab stays");

    // ---- L: the clutch in, the cascade and the ride -------------------------------------
    Simulation l_ride(InitialSpawn::CraneLCab);
    (void)l_ride.advance_frame(1.0);
    require(throw_crane_clutch(l_ride), "the rider must throw the winch's clutch in");
    const double l_rider_y0 = l_ride.snapshot().player_position.y;
    const double l_cart_y0 = kit_com_y(l_ride, Simulation::kCraneLCartEntityId);
    const double l_weight_y0 = kit_com_y(l_ride, Simulation::kCraneLWeightEntityId);
    require(pull_crane_drop(l_ride), "the rider must pull the drop weight's pin, the clutch in");
    const bool l_arrived = wait_for(l_ride, 90.0, [&](const scraperx::sim::Snapshot &) {
        return l_ride.crane_state().l_cab_travel >= 67.9;
    });
    (void)l_ride.advance_frame(1.0);
    const auto l_top = l_ride.snapshot();
    require(l_arrived, "the cart's run must wind L's cab up into TP-640");
    require(on_support(l_top, Simulation::kCraneLCabEntityId), "the rider must ride L's cab all the way");
    const double l_gain = kRiderMassKg * kG * (l_top.player_position.y - l_rider_y0);
    const double l_released =
        4000.0 * kG * (l_cart_y0 - kit_com_y(l_ride, Simulation::kCraneLCartEntityId)) +
        200.0 * kG * (l_weight_y0 - kit_com_y(l_ride, Simulation::kCraneLWeightEntityId));
    require(l_gain > 0.0 && l_gain <= l_released, "L's rider must never gain more than the cart and weight released");
    std::cout << "PASS scraperx_sim AS-009 L: rider_y=" << l_top.player_position.y << " gain_J=" << l_gain
              << " released_J=" << l_released << '\n';
}

// AS-009's band from wherever on the 484 ring's west or north band: round to
// J's joint and traveler, the ride to 528; round the 528 ring and over the
// board to K's cage, hooked on, the pendant's pin, the ride to 572; over the
// board and round the 572 ring to L's cab, the clutch, the drop weight, the
// ride into TP-640; off onto the plate.
bool climb_crane_band(scraperx::sim::Simulation &band) {
    using scraperx::sim::Simulation;
    const auto here = band.snapshot().player_position;
    if (here.x < -8.0 && !walk_to(band, here.x, -140.3, 12.0)) {
        std::cout << "band: round to the 484 ring's north band\n";
        return false;
    }
    if (!(walk_to(band, -3.0, -140.3, 12.0) && lay_crane_joint(band) && pull_crane_chock(band))) {
        std::cout << "band: J rigged and set off\n";
        return false;
    }
    if (!wait_for(band, 60.0, [&](const scraperx::sim::Snapshot &) {
            return band.crane_state().j_traveler_travel >= 43.9;
        })) {
        std::cout << "band: J's traveler to 528\n";
        return false;
    }
    (void)band.advance_frame(1.0);
    if (!(walk_to(band, 0.0, -138.6, 8.0) && walk_to(band, 0.0, -141.5, 8.0) && walk_to(band, -8.5, -141.5, 12.0) &&
          walk_to(band, -8.5, -155.0, 16.0) && walk_to(band, -11.6, -155.0, 8.0))) {
        const auto p = band.snapshot().player_position;
        std::cout << "band: round the 528 ring to K's board, at " << p.x << ' ' << p.y << ' ' << p.z << '\n';
        return false;
    }
    if (!(rig_crane_k(band) && pull_crane_pendant(band))) {
        const auto p = band.snapshot().player_position;
        std::cout << "band: K rigged and set off, at " << p.x << ' ' << p.y << ' ' << p.z << '\n';
        return false;
    }
    if (!wait_for(band, 90.0, [&](const scraperx::sim::Snapshot &) {
            return band.crane_state().k_cage_travel >= 43.9;
        })) {
        std::cout << "band: K's cage to 572\n";
        return false;
    }
    (void)band.advance_frame(1.0);
    if (!(walk_to(band, -11.9, -155.0, 6.0) && walk_to(band, -6.5, -155.0, 10.0) &&
          walk_to(band, -6.5, -156.7, 6.0) && walk_to(band, -3.0, -156.7, 8.0) &&
          walk_to(band, -3.0, -159.6, 6.0) && throw_crane_clutch(band) && pull_crane_drop(band))) {
        std::cout << "band: over the board and round the 572 ring to L, clutch in and set off\n";
        return false;
    }
    if (!wait_for(band, 90.0, [&](const scraperx::sim::Snapshot &) {
            return band.crane_state().l_cab_travel >= 67.9;
        })) {
        std::cout << "band: L's cab to TP-640\n";
        return false;
    }
    (void)band.advance_frame(1.0);
    if (!walk_to(band, -3.0, -156.5, 8.0)) {
        std::cout << "band: off L's cab onto TP-640\n";
        return false;
    }
    (void)band.advance_frame(0.5);
    return standing_above(band.snapshot(), 640.2);
}

void run_crane_band() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation band(InitialSpawn::Ring484North);
    (void)band.advance_frame(1.0);
    const double start = band.snapshot().simulation_time_seconds;
    require(climb_crane_band(band), "band: the 484 ring to standing on TP-640 through J, K and L");
    const auto top = band.snapshot();
    std::cout << "PASS scraperx_sim AS-009 band: to_640_s=" << top.simulation_time_seconds - start
              << " plate_y=" << top.player_position.y << '\n';
}

// AS-009's climbing route, on player inputs: from the 484 ring's north band to
// the east band, one L of boards and a ladder per ring gap to 616, then the
// ladder up through TP-640's hatch; every lift in the band where it was found.
void run_crane_route() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation route(InitialSpawn::Ring484North);
    (void)route.advance_frame(1.0);
    const double start = route.snapshot().simulation_time_seconds;
    require(walk_to(route, -3.0, -140.8, 6.0) && walk_to(route, 10.3, -140.8, 16.0) &&
                walk_to(route, 10.3, -149.0, 12.0),
            "route: along the 484 ring to its east band");
    for (double h = 484.0; h < 615.9; h += 22.0) {
        const double s = 14.72 - 0.91 * (h - 330.0) / 22.0;
        const bool leg = walk_to(route, s + 0.25, -149.0, 12.0, 0.1) && walk_to(route, s - 1.67, -149.0, 8.0, 0.1) &&
                         climb_wet_hold(route, s - 1.67, -150.0, 1.0, 0.0, false, h + 22.5);
        if (!leg) {
            std::cout << "route: stuck above " << h << " at y=" << route.snapshot().player_position.y << '\n';
        }
        require(leg, "route: boards and a ladder to the next ring");
    }
    require(climb_wet_hold(route, 4.9, -151.02, 0.0, 1.0, false, 640.5),
            "route: the ladder up through TP-640's hatch");
    const auto top = route.snapshot();
    const auto crane = route.crane_state();
    require(crane.j_traveler_travel < 0.02 && crane.k_cage_travel < 0.02 && crane.l_cab_travel < 0.02 &&
                crane.j_wagon_latched && crane.k_jib_latched && crane.l_cart_latched,
            "route: no lift in the band moved");
    std::cout << "PASS scraperx_sim AS-009 route: seconds=" << top.simulation_time_seconds - start
              << " top_y=" << top.player_position.y << " lifts_untouched=1\n";
}

// AS-009's one-shot wreckage, on player inputs: each spent machine reached and
// climbed.
void run_crane_wreckage() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;

    // ---- K, its pendant pin pulled with the shackle free ------------------------------
    Simulation k(InitialSpawn::CraneKCage);
    (void)k.advance_frame(1.0);
    require(pull_crane_pendant(k) &&
                wait_for(k, 20.0, [&](const scraperx::sim::Snapshot &) { return k.crane_state().k_jib_angle > 1.56; }) &&
                k.crane_state().k_cage_travel < 0.05,
            "K wreck: the jib swings down to hang plumb, the cage stays");
    require(walk_to(k, -12.6, -155.0, 4.0) && walk_to(k, -9.5, -155.0, 8.0),
            "K wreck: off the cage over its board onto the 528 ring");
    require(walk_to(k, -10.25, -151.0, 8.0, 0.08), "K wreck: to the ring's edge beside the hanging jib");
    (void)k.set_facing(-1.0, 0.0);
    (void)k.advance_frame(0.4);
    (void)k.request_traversal();
    (void)k.advance_frame(0.2);
    const auto k_hold = k.snapshot();
    require(is_climbing(k_hold) && k_hold.traversal_support_entity_id == Simulation::kCraneKJibEntityId,
            "K wreck: a hold on the hanging jib");
    require(hold_stick(k, -1.0, 0.0, -1.0, 0.0, 40.0,
                       [](const scraperx::sim::Snapshot &state) { return state.player_position.y >= 551.3; }),
            "K wreck: up the jib to its heel");
    (void)k.request_jump();
    require(hold_stick(k, 1.0, 0.0, 1.0, 0.0, 4.0,
                       [](const scraperx::sim::Snapshot &state) { return standing_above(state, 550.2); }),
            "K wreck: from the jib's heel back onto the 550 ring");
    std::cout << "PASS scraperx_sim AS-009 wreckage K: jib_angle=" << k.crane_state().k_jib_angle
              << " ring_y=" << k.snapshot().player_position.y << '\n';

    // ---- J, ridden to the 528 ring, where its wagon's run ends beside the ring ----------
    Simulation j(InitialSpawn::Ring484North);
    (void)j.advance_frame(1.0);
    require(lay_crane_joint(j) && pull_crane_chock(j) &&
                wait_for(j, 60.0,
                         [&](const scraperx::sim::Snapshot &) { return j.crane_state().j_traveler_travel >= 43.9; }),
            "J wreck: J ridden to the 528 ring");
    (void)j.advance_frame(1.0);
    require(walk_to(j, 0.0, -138.6, 8.0) && walk_to(j, 0.0, -141.5, 8.0) && walk_to(j, 9.8, -141.5, 12.0),
            "J wreck: off the traveler and along the 528 ring to its east band");
    require(climb_wreck(j, 10.3, -142.0, 1.0, 0.0, Simulation::kCraneJWagonEntityId, 531.9) &&
                j.snapshot().support_entity_id == Simulation::kCraneJWagonEntityId,
            "J wreck: the spent wagon climbed from the ring onto its deck");
    std::cout << "PASS scraperx_sim AS-009 wreckage J: wagon_travel=" << j.crane_state().j_wagon_travel
              << " deck_y=" << j.snapshot().player_position.y << '\n';

    // ---- L, its drop weight's pin pulled with the clutch out ----------------------------
    Simulation l(InitialSpawn::CraneLCab);
    (void)l.advance_frame(1.0);
    require(pull_crane_drop(l) &&
                wait_for(l, 40.0, [&](const scraperx::sim::Snapshot &) { return l.crane_state().l_cart_travel > 51.9; }) &&
                l.crane_state().l_cab_travel < 0.05,
            "L wreck: the cart runs down beside the 572 ring, the cab stays");
    require(walk_to(l, -3.0, -158.3, 6.0) && walk_to(l, 0.1, -158.3, 8.0), "L wreck: off the cab onto the 572 ring");
    require(climb_wreck(l, 0.1, -158.45, 0.0, -1.0, Simulation::kCraneLCartEntityId, 575.9) &&
                l.snapshot().support_entity_id == Simulation::kCraneLCartEntityId,
            "L wreck: the spent cart climbed from the ring onto its deck");
    std::cout << "PASS scraperx_sim AS-009 wreckage L: cart_travel=" << l.crane_state().l_cart_travel
              << " deck_y=" << l.snapshot().player_position.y << '\n';
}

// ---- The mechanism ascent in one run ---------------------------------------------

// Fails the run with where it stopped: the leg, the height and the time.
void require_leg(const scraperx::sim::Simulation &simulation, const bool ok, const char *leg) {
    if (!ok) {
        const auto state = simulation.snapshot();
        std::cout << "ascent: stopped at " << leg << ", y=" << state.player_position.y
                  << " t=" << state.simulation_time_seconds << '\n';
    }
    require(ok, leg);
}

// The goal's test (MECHANISM_ASCENT_PLAN.md, AS-006 to AS-009): one run on
// player inputs from the 154 m deck to standing on TP-640,
// riding and climbing through all four bands' linked stages -- the
// Counterweight Well, Wet Isolation, the Plate Shop and the Facade Crane
// Stack -- with no spawn, placement or teleport between them.
void run_ascent() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    Simulation run(InitialSpawn::Deck154);
    (void)run.advance_frame(1.0);
    const double start = run.snapshot().simulation_time_seconds;
    const auto deaths = run.snapshot().death_count;

    // B02, the Counterweight Well: A, B, C to the 220 ring.
    require_leg(run, board_well_a(run) && rig_well_a(run) && pull_well_a(run, 0.5, 3.0), "A rigged and tripped");
    require_leg(run,
                wait_for(run, 16.0,
                         [](const Snapshot &state) { return state.well_a_cage_travel >= kWellATravel - 0.01; }),
                "A to the 176 ring");
    (void)run.advance_frame(1.0);
    require_leg(run,
                walk_to(run, -9.4, -131.2, 4.0) && walk_to(run, -7.6, -131.2, 4.0) && rig_well_b(run) &&
                    pull_well_b(run),
                "across into B, rigged and tripped");
    require_leg(run, wait_for(run, 16.0, [](const Snapshot &state) {
                    return state.well_b_cage_travel >= kWellATravel - kWellDogPitch - 0.01;
                }),
                "B to the 198 ring");
    (void)run.advance_frame(3.0);
    require_leg(run,
                walk_to(run, -6.2, -131.9, 4.0) && walk_to(run, -4.4, -131.9, 4.0) && clear_well_c_chute(run) &&
                    fill_well_c(run, 10.0) && pull_well_c_latch(run, 3.0),
                "across onto C, its chute cleared, filled and let go");
    require_leg(run, wait_for(run, 16.0, [](const Snapshot &state) {
                    return state.well_c_platform_travel >= kWellATravel - kWellDogPitch - 0.01;
                }),
                "C to the 220 ring");
    (void)run.advance_frame(2.0);
    require_leg(run, walk_to(run, -3.0, -129.0, 4.0), "off C onto the 220 ring");
    const double at_220 = run.snapshot().simulation_time_seconds - start;

    // B03, Wet Isolation: D, E, F to TP-340.
    require_leg(run, walk_to(run, 2.5, -129.6, 8.0) && walk_to(run, 4.0, -129.2, 6.0) && seat_wet_spool(run) &&
                         throw_wet_fill(run),
                "along the 220 ring, D's spool seated and its fill thrown");
    require_leg(run,
                wait_for(run, 120.0,
                         [&](const Snapshot &) { return run.wet_state().d_platform_travel >= 35.95; }),
                "D to 256.25");
    require_leg(run,
                walk_to(run, 13.2, -134.5, 6.0) && walk_to(run, 13.2, -135.8, 6.0) && shut_wet_door(run) &&
                    pull_wet_trip(run),
                "into E's cab, its door shut and the chiller tripped");
    require_leg(run,
                wait_for(run, 90.0, [&](const Snapshot &) { return run.wet_state().e_cab_travel >= 41.95; }),
                "E to 298.25");
    require_leg(run,
                walk_to(run, 13.4, -137.2, 4.0) && walk_to(run, 13.4, -139.9, 6.0) && couple_wet_hose(run) &&
                    pull_wet_stop_valve(run),
                "onto F, its hose coupled and its valve thrown");
    require_leg(run,
                wait_for(run, 90.0,
                         [&](const Snapshot &) { return run.wet_state().f_platform_travel >= 41.95; }),
                "F to TP-340");
    require_leg(run, walk_to(run, 13.0, -137.2, 6.0), "off F onto TP-340");
    const double at_340 = run.snapshot().simulation_time_seconds - start;

    // B04, the Plate Shop: G, H, I and the ladder to the 484 ring.
    require_leg(run, walk_to_shop_cleat(run) && rig_shop_g(run) && pull_shop_g(run), "G rigged and its pin pulled");
    require_leg(run,
                wait_for(run, 60.0,
                         [&](const Snapshot &) { return run.shop_state().g_platform_travel >= 33.7; }),
                "G to the 374 ring");
    (void)run.advance_frame(1.0);
    require_leg(run, walk_to(run, -14.5, -149.3, 8.0) && pull_shop_girder_pin(run) && board_shop_h(run) &&
                         pull_shop_chock(run),
                "the girder's tail pin out, over the gangway onto H, its chock pulled");
    require_leg(run,
                wait_for(run, 90.0,
                         [&](const Snapshot &) { return run.shop_state().h_platform_travel >= 43.9; }),
                "H to the 418 ring");
    (void)run.advance_frame(1.0);
    require_leg(run, rig_shop_i(run) && pull_shop_domino(run), "onto I's cage, hooked on, the domino's pin pulled");
    require_leg(run,
                wait_for(run, 90.0, [&](const Snapshot &) { return run.shop_state().i_cage_travel >= 43.9; }),
                "I to 462.25");
    (void)run.advance_frame(1.0);
    require_leg(run, climb_wet_hold(run, -7.6, -145.6, -1.0, 0.0, false, 484.5), "up the ladder onto the 484 ring");
    const double at_484 = run.snapshot().simulation_time_seconds - start;

    // B05, the Facade Crane Stack: J, K, L into TP-640.
    require_leg(run, climb_crane_band(run), "J, K and L to standing on TP-640");
    const auto top = run.snapshot();
    require(top.death_count == deaths && standing_above(top, 640.2),
            "ascent: one run from the 154 m deck must end standing on TP-640, never having died");
    std::cout << "PASS scraperx_sim ascent 154 to TP-640: seconds=" << top.simulation_time_seconds - start
              << " at_220=" << at_220 << " at_340=" << at_340 << " at_484=" << at_484
              << " plate_y=" << top.player_position.y << '\n';
}

// ---- Band 0, the Stack: S1, the water-balance hoist -------------------------

constexpr double kS1CageMassKg = 300.0;
constexpr double kS1BucketMassKg = 150.0;
constexpr double kS1Travel = 21.8;
constexpr double kS1FloorTopUp = 22.05;
constexpr double kDeck2Top = 22.0;
constexpr double kDeck4Top = 44.0;

// Where S1's valve chain hangs at rest, inside the cage at grade: its
// handle's centre.
constexpr double kS1ChainX = 9.50;
constexpr double kS1ChainY = 2.16;
constexpr double kS1ChainZ = -120.10;
// Water that balances the cage and an 85 kg rider against the empty bucket.
constexpr double kS1BalanceKg = kS1CageMassKg + 85.0 - kS1BucketMassKg;

// From wherever the player stands in the yard, in through S1's open south
// side to stand beside the valve chain hanging inside the cage: face it and
// take hold. Hanging above the hands, it comes down to them, and that pulls
// the valve's lever down. True once the chain is in the hands.
bool take_s1_chain(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!(walk_to(simulation, 10.0, -117.3, 30.0) && walk_to(simulation, 10.0, kS1ChainZ, 6.0, 0.08))) {
        return false;
    }
    (void)simulation.set_facing(-1.0, 0.0);
    (void)simulation.advance_frame(0.5);
    const auto facing = simulation.snapshot();
    if (facing.carry_target_entity_id != Simulation::kStackS1ChainEntityId || facing.carry_target_kind != 2) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    return simulation.snapshot().carrying_entity_id == Simulation::kStackS1ChainEntityId;
}

// Whether S1's chain hangs where a rider at grade can take it again.
bool s1_chain_at_rest(const scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    const auto at = simulation.kit_body_position(simulation.kit_body_index(Simulation::kStackS1ChainEntityId));
    return std::abs(at.x - kS1ChainX) <= 0.10 && std::abs(at.y - kS1ChainY) <= 0.06 &&
           std::abs(at.z - kS1ChainZ) <= 0.10 && simulation.snapshot().carrying_entity_id == 0;
}

// What one ride of S1 saw, from taking hold of the chain at grade to the top.
struct S1Ride final {
    bool reached_top = false;
    bool rode_on_cage = true;
    double seconds = 0.0;          // from taking hold to the top
    double lift_water_kg = 0.0;    // in the bucket as the cage left the yard
    double shut_travel = -1.0;     // the cage's travel when the valve shut
    double shut_water_kg = 0.0;
    double shut_tank_kg = 0.0;
    double top_water_kg = 0.0;
    double top_tank_kg = 0.0;
    double worst_rise = 0.0;
    double worst_margin_j = std::numeric_limits<double>::infinity();
};

// Holding the chain, with no other input, until the cage stops at the top of
// its travel (or `seconds` pass); then let go. Every tick, the energy the
// bucket and the water in it have released falling is compared with what the
// cage and the rider have gained rising.
S1Ride ride_s1(scraperx::sim::Simulation &simulation, const double seconds) {
    using scraperx::sim::Simulation;
    S1Ride ride;
    const double start = simulation.snapshot().simulation_time_seconds;
    const double cage_y0 = kit_y(simulation, Simulation::kStackS1CageEntityId);
    const double rider_y0 = simulation.snapshot().player_position.y;
    double bucket_y = kit_y(simulation, Simulation::kStackS1BucketEntityId);
    double released = 0.0;
    const auto ticks = static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (std::uint32_t tick = 0; tick < ticks && !ride.reached_top; ++tick) {
        const double water = simulation.stack_state().s1_bucket_water_kg;
        (void)simulation.set_move_input(0.0, 0.0);
        (void)simulation.set_facing(-1.0, 0.0);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = simulation.snapshot();
        const auto stack = simulation.stack_state();
        const double bucket_now = kit_y(simulation, Simulation::kStackS1BucketEntityId);
        released += (kS1BucketMassKg + water) * kGravity * (bucket_y - bucket_now);
        bucket_y = bucket_now;
        const double rise = kit_y(simulation, Simulation::kStackS1CageEntityId) - cage_y0;
        if (ride.lift_water_kg == 0.0 && rise > 0.01) {
            ride.lift_water_kg = stack.s1_bucket_water_kg;
        }
        if (ride.shut_travel < 0.0 && rise > 0.01 && stack.s1_valve_angle < 0.06) {
            ride.shut_travel = rise;
            ride.shut_water_kg = stack.s1_bucket_water_kg;
            ride.shut_tank_kg = stack.s1_tank_water_kg;
        }
        if (rise > 0.05 && rise < kS1Travel - 0.05) {
            ride.rode_on_cage = ride.rode_on_cage && state.player_grounded &&
                                state.support_entity_id == Simulation::kStackS1CageEntityId;
        }
        const double gained =
            kS1CageMassKg * kGravity * rise + kRiderMassKg * kGravity * (state.player_position.y - rider_y0);
        if (released - gained < ride.worst_margin_j) {
            ride.worst_margin_j = released - gained;
            ride.worst_rise = rise;
        }
        if (stack.s1_cage_travel >= kS1Travel - 0.01) {
            ride.reached_top = true;
            ride.seconds = state.simulation_time_seconds - start;
            ride.top_water_kg = stack.s1_bucket_water_kg;
            ride.top_tank_kg = stack.s1_tank_water_kg;
        }
    }
    if (simulation.snapshot().carrying_entity_id != 0) {
        (void)simulation.request_set_down();
    }
    (void)simulation.advance_frame(0.5);
    return ride;
}

// ---- Band 0, the Stack: C1, the facade -----------------------------------------

// Where a leg of C1 left the body, for a failure's message.
void report_c1(const scraperx::sim::Simulation &simulation, const char *leg) {
    const auto state = simulation.snapshot();
    std::cout << "C1 " << leg << ": at=" << state.player_position.x << "," << state.player_position.y << ","
              << state.player_position.z << " grounded=" << state.player_grounded
              << " traversal=" << int(state.traversal_state) << " support=" << state.support_entity_id
              << " ledge=" << state.ledge_available << " grip=" << state.grip_available << "\n";
}

// From deck 2's south band up C1 to deck 4, on player inputs: out onto the
// landing, up onto the cabinet, a jump to hang from the duct's lip, up onto
// the duct and along it, up the vent stack over deck 3's edge; along deck 3,
// out along the monorail under the davit's ladder, a turn and a leap for it,
// up it onto the davit's arm, and back along the arm onto deck 4.
// What a climb of C1 saw on the way, for its falsifiers.
struct C1Notes final {
    bool ladder_in_reach_standing = true;
};

bool climb_c1(scraperx::sim::Simulation &simulation, C1Notes *notes = nullptr) {
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    // Out onto the landing, round the cabinet to its south side.
    if (!(walk_to(simulation, 21.2, -124.4, 20.0) && walk_to(simulation, 21.2, -121.3, 6.0) &&
          walk_to(simulation, 20.0, -121.3, 6.0, 0.08))) {
        report_c1(simulation, "to the cabinet");
        return false;
    }
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.4);
    if (!simulation.snapshot().ledge_available) {
        report_c1(simulation, "cabinet offered");
        return false;
    }
    (void)simulation.request_traversal();
    if (!wait_for(simulation, 2.0, [](const Snapshot &state) { return standing_above(state, 24.5); })) {
        report_c1(simulation, "mantle onto the cabinet");
        return false;
    }
    // On the cabinet, clear of the duct's underside: jump, stick to the duct.
    (void)walk_to(simulation, 20.0, -122.10, 2.0, 0.05);
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.3);
    (void)simulation.request_jump();
    if (!hold_stick(simulation, 0.0, -0.4, 0.0, -1.0, 2.0,
                    [](const Snapshot &state) { return state.traversal_state == TraversalState::Hanging; })) {
        report_c1(simulation, "hang on the duct's lip");
        return false;
    }
    (void)simulation.advance_frame(0.3);
    (void)simulation.request_jump();
    if (!wait_for(simulation, 2.5, [](const Snapshot &state) { return standing_above(state, 28.0); })) {
        report_c1(simulation, "up onto the duct");
        return false;
    }
    // Along the duct to the vent stack, and up it over deck 3's edge.
    if (!(walk_to(simulation, 22.5, -123.05, 6.0, 0.1) && walk_to(simulation, 24.0, -123.00, 6.0, 0.06))) {
        report_c1(simulation, "along the duct");
        return false;
    }
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.4);
    if (!simulation.snapshot().grip_available) {
        report_c1(simulation, "vent offered");
        return false;
    }
    (void)simulation.request_traversal();
    (void)simulation.advance_frame(0.2);
    if (!is_climbing(simulation.snapshot()) ||
        !hold_stick(simulation, 0.0, -1.0, 0.0, -1.0, 20.0,
                    [](const Snapshot &state) { return standing_above(state, 33.5); })) {
        report_c1(simulation, "up the vent onto deck 3");
        return false;
    }
    // Along deck 3 to the monorail, out along it under the ladder to its end.
    if (!(walk_to(simulation, 22.0, -125.2, 6.0) && walk_to(simulation, 12.5, -125.2, 12.0, 0.08) &&
          walk_to(simulation, 12.5, -119.65, 12.0, 0.06))) {
        report_c1(simulation, "out along the monorail");
        return false;
    }
    (void)simulation.advance_frame(0.3);
    const auto at_end = simulation.snapshot();
    if (!at_end.player_grounded || at_end.player_position.y < 34.0) {
        report_c1(simulation, "standing at the monorail's end");
        return false;
    }
    // Turn to the ladder and leap for it: standing, it is out of reach.
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.3);
    if (notes != nullptr) {
        notes->ladder_in_reach_standing = simulation.snapshot().grip_available;
    }
    (void)simulation.request_jump();
    if (!hold_stick(simulation, 0.0, -0.3, 0.0, -1.0, 2.0,
                    [](const Snapshot &state) { return is_climbing(state); })) {
        report_c1(simulation, "catch the ladder");
        return false;
    }
    // Up the ladder onto the davit's arm, and back along it onto deck 4.
    if (!hold_stick(simulation, 0.0, -1.0, 0.0, -1.0, 20.0,
                    [](const Snapshot &state) { return standing_above(state, 44.5); })) {
        report_c1(simulation, "up the ladder onto the arm");
        return false;
    }
    if (!walk_to(simulation, 12.5, -125.0, 10.0, 0.1)) {
        report_c1(simulation, "back along the arm");
        return false;
    }
    if (!walk_to(simulation, 11.2, -125.3, 4.0, 0.1)) {
        report_c1(simulation, "off the arm onto deck 4");
        return false;
    }
    (void)simulation.advance_frame(0.5);
    const auto on_deck4 = simulation.snapshot();
    return on_deck4.player_grounded && on_deck4.player_position.y > kDeck4Top + 0.5 &&
           on_deck4.support_entity_id == Simulation::kTowerEntityId;
}

// ---- Band 0, the Stack: S2, the rolling-ballast walking beam ----------------

constexpr double kDeck6Top = 66.0;
constexpr double kS2CageMassKg = 300.0;
constexpr double kS2CartMassKg = 3500.0;
constexpr double kS2Travel = 22.0;
constexpr double kS2FloorTopUp = 66.05;

bool take_s2_handle(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!(walk_to(simulation, 10.0, -130.5, 30.0) && walk_to(simulation, 10.0, -137.4, 20.0, 0.08))) {
        return false;
    }
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.5);
    const auto facing = simulation.snapshot();
    if (facing.carry_target_entity_id != Simulation::kStackS2HandleEntityId || facing.carry_target_kind != 2) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    return simulation.snapshot().carrying_entity_id == Simulation::kStackS2HandleEntityId;
}

bool s2_handle_at_rest(const scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    const auto at = simulation.kit_body_position(simulation.kit_body_index(Simulation::kStackS2HandleEntityId));
    return std::abs(at.x - 10.00) <= 0.15 && std::abs(at.y - (44.05 + 1.95)) <= 0.10 &&
           std::abs(at.z - (-138.00)) <= 0.15 && simulation.snapshot().carrying_entity_id == 0;
}

struct S2Ride final {
    bool reached_top = false;
    bool rode_on_cage = true;
    double seconds = 0.0;
    double worst_margin_j = std::numeric_limits<double>::infinity();
};

S2Ride ride_s2(scraperx::sim::Simulation &simulation, const double seconds) {
    using scraperx::sim::Simulation;
    S2Ride ride;
    const double start = simulation.snapshot().simulation_time_seconds;
    const double cage_y0 = kit_y(simulation, Simulation::kStackS2CageEntityId);
    const double rider_y0 = simulation.snapshot().player_position.y;
    const auto ticks = static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (std::uint32_t tick = 0; tick < ticks && !ride.reached_top; ++tick) {
        (void)simulation.set_move_input(0.0, 0.0);
        (void)simulation.set_facing(0.0, -1.0);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = simulation.snapshot();
        const auto stack = simulation.stack_state();
        const double rise = kit_y(simulation, Simulation::kStackS2CageEntityId) - cage_y0;
        const double rider_rise = state.player_position.y - rider_y0;
        const double payload_gained = (kS2CageMassKg + 80.0) * kGravity * rider_rise;
        const double released = kS2CartMassKg * kGravity * rise;
        if (rise > 0.05) {
            ride.worst_margin_j = std::min(ride.worst_margin_j, released - payload_gained);
            if (state.support_entity_id != Simulation::kStackS2CageEntityId) {
                ride.rode_on_cage = false;
            }
        }
        if (stack.s2_cage_travel >= kS2Travel - 0.10) {
            ride.reached_top = true;
            ride.seconds = state.simulation_time_seconds - start;
            (void)simulation.request_set_down();
            (void)simulation.advance_frame(0.2);
            break;
        }
    }
    return ride;
}

void run_stack() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;

    // The yard's kerb is 0.5 m wide and 86 m long, but a step high: it is
    // walked on and off, not balanced along. Held on its line as a beam, a
    // body on it could not step down off it with a gentle stick.
    Simulation kerb(InitialSpawn::ExteriorGrade);
    require(kerb.advance_frame(0.5).accepted, "kerb settle interval must be accepted");
    require(walk_to(kerb, 20.0, -117.0, 40.0) && walk_to(kerb, 20.0, -118.0, 6.0, 0.08),
            "the walk must reach the top of the yard's kerb");
    const auto on_kerb = kerb.snapshot();
    for (std::uint32_t tick = 0; tick < 90; ++tick) {
        (void)kerb.set_move_input(0.0, 0.35);
        (void)kerb.set_facing(0.0, -1.0);
        (void)kerb.advance_frame(Simulation::kFixedStepSeconds);
    }
    const auto off_kerb = kerb.snapshot();
    require(on_kerb.player_position.y > 1.1 && !on_kerb.player_balancing,
            "standing on the yard's kerb must not be balancing");
    require(off_kerb.player_position.z > on_kerb.player_position.z + 0.4 &&
                off_kerb.player_position.y < 0.95,
            "a gentle stick must step down off the yard's kerb");
    std::cout << "PASS scraperx_sim kerb: on_y=" << on_kerb.player_position.y
              << " stepped_z=" << off_kerb.player_position.z - on_kerb.player_position.z << "\n";

    // (a) A tug is not a ride: taken and let go at once, the chain opens the
    // valve for a moment and lets the catch go, but the water that runs in is
    // far short of what outweighs the cage and its rider, so nothing moves,
    // and let go, the catch seats the bucket again. The water stays in the
    // bucket: tugs add up, and held again the chain finishes the job.
    // Left alone, S1 waits as found: the lever on its stop under the chain's
    // weight, the valve shut, the bucket dry on its catch, the cage at grade.
    Simulation idle(InitialSpawn::ExteriorGrade);
    require(idle.advance_frame(30.0).accepted, "S1 idle interval must be accepted");
    const auto idle_state = idle.stack_state();
    require(idle_state.s1_valve_angle < 0.005 && idle_state.s1_bucket_water_kg < 0.01 &&
                idle_state.s1_catch_latched && std::abs(idle_state.s1_cage_travel) <= 0.01 &&
                s1_chain_at_rest(idle),
            "left alone for 30 s, S1 must wait as found");

    Simulation tug(InitialSpawn::ExteriorGrade);
    require(tug.advance_frame(0.5).accepted, "S1 settle interval must be accepted");
    const double tug_tank0 = tug.stack_state().s1_tank_water_kg;
    require(take_s1_chain(tug), "the player must walk from the yard into S1's cage and take hold of its chain");
    (void)tug.advance_frame(0.3);
    const auto tugged = tug.stack_state();
    require(tugged.s1_valve_angle >= 0.24 && !tugged.s1_catch_latched,
            "held, S1's chain must turn its lever past full open and let the catch go");
    (void)tug.request_set_down();
    double tug_cage_worst = 0.0;
    double tug_bucket_worst = 0.0;
    for (std::uint32_t tick = 0; tick < 90 * 4; ++tick) {
        (void)tug.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = tug.stack_state();
        tug_cage_worst = std::max(tug_cage_worst, std::abs(state.s1_cage_travel));
        tug_bucket_worst = std::max(tug_bucket_worst, std::abs(state.s1_bucket_travel));
    }
    const auto after_tug = tug.stack_state();
    require(after_tug.s1_bucket_water_kg > 10.0 && after_tug.s1_bucket_water_kg < kS1BalanceKg - 20.0,
            "a tug on S1's chain must let some water in, short of what lifts the cage");
    require(std::abs(tug_tank0 - after_tug.s1_tank_water_kg - after_tug.s1_bucket_water_kg) < 2.0,
            "the bucket's water must have come out of S1's tank");
    require(tug_cage_worst <= 0.05 && tug_bucket_worst <= 0.05,
            "after a tug, S1's cage and bucket must not move");
    require(after_tug.s1_catch_latched && after_tug.s1_valve_angle < 0.03,
            "let go, S1's lever must come back up, shut the valve and seat the catch");
    require(s1_chain_at_rest(tug), "let go, S1's chain must hang back where it was");
    const double tug_water = after_tug.s1_bucket_water_kg;
    require(take_s1_chain(tug), "S1's chain must be taken again after a tug");
    const auto tug_ride = ride_s1(tug, 20.0);
    require(tug_ride.reached_top && tug_ride.lift_water_kg > tug_water,
            "held again after a tug, S1's chain must fill the bucket the rest of the way and lift the rider");
    std::cout << "PASS scraperx_sim S1 tug: water_kg=" << tug_water << " cage_worst=" << tug_cage_worst
              << " bucket_worst=" << tug_bucket_worst << " then_lift_kg=" << tug_ride.lift_water_kg << "\n";

    // (b) The ride, from the game's spawn on player inputs: into the cage, take
    // hold of the chain and hold it. Water runs into the bucket until it
    // outweighs the cage and the rider (235 kg); the bucket falls 21.8 m and the
    // cage carries the rider 21.8 m under a governor that can only brake. Its
    // first half metre lets the chain go slack and the valve shut. At no tick
    // has the payload gained more energy than the bucket and its water
    // released.
    Simulation ride(InitialSpawn::ExteriorGrade);
    require(ride.advance_frame(0.5).accepted, "S1 ride settle interval must be accepted");
    const double tank0 = ride.stack_state().s1_tank_water_kg;
    require(take_s1_chain(ride), "the rider must walk from the yard into S1's cage and take hold of its chain");
    const auto rode = ride_s1(ride, 20.0);
    const auto top_state = ride.stack_state();
    const double floor_y = kit_y(ride, Simulation::kStackS1CageEntityId) + 0.10;
    require(rode.reached_top, "holding S1's chain must carry the rider to the top of the cage's travel");
    require(rode.lift_water_kg >= kS1BalanceKg - 5.0 && rode.lift_water_kg <= kS1BalanceKg + 60.0,
            "S1's cage must leave the yard only once the bucket's water outweighs the cage and rider");
    require(rode.shut_travel > 0.0 && rode.shut_travel < 1.0,
            "rising, the rider must let S1's chain go slack and its valve shut within the first metre");
    require(std::abs(tank0 - rode.shut_tank_kg - rode.shut_water_kg) < 2.0,
            "every kilogram in S1's bucket must have come out of its tank");
    require(rode.shut_tank_kg - rode.top_tank_kg < 1.0 && rode.top_water_kg - rode.shut_water_kg < 1.0,
            "with the valve shut, no more water may leave S1's tank on the way up");
    require(rode.rode_on_cage, "the rider must stand on S1's cage for the whole ride");
    require(std::abs(floor_y - kS1FloorTopUp) <= 0.05, "S1's floor must stop at 22.05 m");
    require(top_state.s1_cage_peak_speed <= 2.6, "S1's governor must hold the cage to 2.5 m/s");
    require(rode.worst_margin_j >= -kStanceJitterJ,
            "at no tick may S1's payload have gained more energy than the bucket released");

    // (d) The receiver: off the cage's open north side, along the gangway and
    // onto deck 2's south band, standing on the tower.
    require(walk_to(ride, 10.0, -123.2, 6.0) && walk_to(ride, 10.0, -126.5, 6.0),
            "the rider must walk off S1's cage over the gangway onto deck 2");
    (void)ride.advance_frame(0.5);
    const auto on_deck = ride.snapshot();
    require(on_deck.player_grounded && on_deck.player_position.y > kDeck2Top + 0.5 &&
                on_deck.player_position.z < -124.0 &&
                on_deck.support_entity_id == Simulation::kTowerEntityId,
            "the rider must stand on deck 2's south band");

    // Recovery: at the foot of its guide the bucket sits on the striker and
    // drains; lighter than the cage again, it rises, the cage comes back down
    // and the catch seats the bucket at the top. S1 is as it was found, its
    // chain hanging in the cage where the next rider takes it.
    bool returned = false;
    double descent_water = -1.0;
    for (std::uint32_t tick = 0; tick < 90 * 60 && !returned; ++tick) {
        (void)ride.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = ride.stack_state();
        if (descent_water < 0.0 && state.s1_cage_travel < kS1Travel - 0.10) {
            descent_water = state.s1_bucket_water_kg;
        }
        returned = std::abs(state.s1_cage_travel) <= 0.02 && state.s1_catch_latched &&
                   std::abs(state.s1_bucket_travel) <= 0.05;
    }
    (void)ride.advance_frame(2.0);
    const auto back = ride.stack_state();
    require(returned, "drained, S1's bucket must rise and bring the cage back down to the yard");
    require(descent_water > 0.0 && descent_water < kS1CageMassKg - kS1BucketMassKg,
            "S1's empty cage must start down only once the bucket is lighter than it");
    require(s1_chain_at_rest(ride), "S1's chain must hang back in the cage at grade, in a rider's reach");
    std::cout << "PASS scraperx_sim S1 ride: ride_s=" << rode.seconds << " lift_water_kg=" << rode.lift_water_kg
              << " valve_shut_at_m=" << rode.shut_travel << " top_water_kg=" << rode.top_water_kg
              << " floor_y=" << floor_y << " peak_speed=" << top_state.s1_cage_peak_speed
              << " energy_margin_J=" << rode.worst_margin_j << " deck2_y=" << on_deck.player_position.y
              << " descent_water_kg=" << descent_water << " return_water_kg=" << back.s1_bucket_water_kg << "\n";

    // (c) The rider who stays aboard: let go at the top and stand still, the
    // bucket drains until it is lighter than the cage and rider, and the cage
    // brings them back down to the yard. There the chain hangs in reach again
    // and the machine carries them up a second time: nobody is stranded, and
    // the tank holds water for many rides.
    Simulation stay(InitialSpawn::ExteriorGrade);
    require(stay.advance_frame(0.5).accepted, "S1 stay-aboard settle interval must be accepted");
    require(take_s1_chain(stay) && ride_s1(stay, 20.0).reached_top, "S1 must carry the rider up");
    bool brought_down = false;
    bool aboard = true;
    double stay_descent_water = -1.0;
    for (std::uint32_t tick = 0; tick < 90 * 60 && !brought_down; ++tick) {
        (void)stay.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = stay.stack_state();
        if (stay_descent_water < 0.0 && state.s1_cage_travel < kS1Travel - 0.10) {
            stay_descent_water = state.s1_bucket_water_kg;
        }
        if (state.s1_cage_travel > 0.05 && state.s1_cage_travel < kS1Travel - 0.05) {
            const auto snap = stay.snapshot();
            aboard = aboard && snap.player_grounded && snap.support_entity_id == Simulation::kStackS1CageEntityId;
        }
        brought_down = std::abs(state.s1_cage_travel) <= 0.02 && state.s1_catch_latched;
    }
    (void)stay.advance_frame(2.0);
    require(brought_down && aboard, "a rider who stays aboard must be brought back down to the yard on S1's cage");
    require(stay_descent_water > 0.0 && stay_descent_water < kS1BalanceKg,
            "S1's cage must start down with its rider only once the bucket is lighter than both");
    require(s1_chain_at_rest(stay), "back at grade, S1's chain must hang in reach again");
    const double stay_tank = stay.stack_state().s1_tank_water_kg;
    require(take_s1_chain(stay) && ride_s1(stay, 20.0).reached_top, "S1 must carry the rider up a second time");
    const double per_ride = stay_tank - stay.stack_state().s1_tank_water_kg;
    require(per_ride > 0.0 && stay.stack_state().s1_tank_water_kg > 10.0 * per_ride,
            "S1's tank must hold water for many more rides");
    std::cout << "PASS scraperx_sim S1 stay aboard: descent_water_kg=" << stay_descent_water
              << " per_ride_kg=" << per_ride << " tank_kg=" << stay.stack_state().s1_tank_water_kg << "\n";

    // C1: from deck 2's south band, where S1 leaves its rider, up the facade
    // to deck 4 on player inputs.
    Simulation facade(InitialSpawn::Deck2South);
    require(facade.advance_frame(1.0).accepted, "C1 settle interval must be accepted");
    const double facade_start = facade.snapshot().simulation_time_seconds;
    C1Notes notes;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    const bool climbed = climb_c1(facade, &notes);
    g_path_watch.armed = false;
    require(climbed, "C1 must carry a climber from deck 2 to deck 4");
    if (g_path_watch.worst > 0.15) {
        std::cout << "C1 jump: step=" << g_path_watch.worst << " at=" << g_path_watch.worst_at.x << ","
                  << g_path_watch.worst_at.y << "," << g_path_watch.worst_at.z
                  << " traversal=" << g_path_watch.worst_traversal << "\n";
    }
    require(g_path_watch.worst <= 0.15,
            "climbing C1, the body must never move more than 0.15 m sideways in one tick");
    require(!notes.ladder_in_reach_standing,
            "the davit's ladder must be out of reach from the monorail without the leap");
    const auto facade_top = facade.snapshot();
    std::cout << "PASS scraperx_sim C1 climb: seconds=" << facade_top.simulation_time_seconds - facade_start
              << " deck4_y=" << facade_top.player_position.y << " ladder_needs_leap=1"
              << " worst_tick_step_m=" << g_path_watch.worst << "\n";

    // The Stack so far in one run from the game's spawn, on player inputs:
    // hold S1's chain, ride it to deck 2, climb C1 to deck 4.
    Simulation band(InitialSpawn::ExteriorGrade);
    require(band.advance_frame(0.5).accepted, "the Stack's settle interval must be accepted");
    const double band_start = band.snapshot().simulation_time_seconds;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    require(take_s1_chain(band), "the Stack: into S1's cage and take hold of its chain");
    require(ride_s1(band, 20.0).reached_top, "the Stack: S1 carries the rider to deck 2");
    require(walk_to(band, 10.0, -123.2, 6.0) && walk_to(band, 10.0, -126.0, 6.0),
            "the Stack: off S1 onto deck 2");
    const double at_deck2 = band.snapshot().simulation_time_seconds - band_start;
    require(climb_c1(band), "the Stack: up C1 to deck 4");
    g_path_watch.armed = false;
    require(g_path_watch.worst <= 0.15,
            "the Stack: from the yard to deck 4 the body must never move more than 0.15 m sideways in one tick");
    const auto band_top = band.snapshot();
    require(band_top.death_count == 0, "the Stack: from the yard to deck 4 without dying");
    std::cout << "PASS scraperx_sim Stack to deck 4: seconds=" << band_top.simulation_time_seconds - band_start
              << " at_deck2=" << at_deck2 << " deck4_y=" << band_top.player_position.y
              << " worst_tick_step_m=" << g_path_watch.worst << "\n";

    // S2 at rest: left alone for 10 s, the walking beam and cage wait as found.
    Simulation s2_idle(InitialSpawn::Deck4South);
    require(s2_idle.advance_frame(10.0).accepted, "S2 idle interval must be accepted");
    const auto s2_idle_state = s2_idle.stack_state();
    require(std::abs(s2_idle_state.s2_cage_travel) <= 0.03 && s2_idle_state.s2_chock_latched &&
                s2_handle_at_rest(s2_idle),
            "left alone, S2 must wait as found with chock latched");
    std::cout << "PASS scraperx_sim S2 at rest: cage_travel=" << s2_idle_state.s2_cage_travel
              << " chock_latched=" << s2_idle_state.s2_chock_latched << "\n";

    // S2 ride: board cage at deck 4, pull chock lanyard, ride walking beam to deck 6.
    Simulation s2_sim(InitialSpawn::Deck4South);
    require(s2_sim.advance_frame(0.5).accepted, "S2 settle interval must be accepted");
    require(take_s2_handle(s2_sim), "the player must walk from deck 4 into S2's cage and take hold of its lanyard");
    const auto s2_rode = ride_s2(s2_sim, 20.0);
    require(s2_rode.reached_top, "pulling S2's lanyard must carry the rider to deck 6");
    require(s2_rode.rode_on_cage, "the rider must stand on S2's cage for the whole ride");
    const auto s2_top_state = s2_sim.stack_state();
    require(s2_top_state.s2_cage_peak_speed <= 2.6, "S2's governor must hold the cage to 2.5 m/s");
    require(s2_rode.worst_margin_j >= 0.0, "S2 payload energy gain must not exceed source energy released");

    // Walk off onto deck 6's south band
    require(walk_to(s2_sim, 10.0, -133.0, 6.0) && walk_to(s2_sim, 10.0, -126.0, 6.0),
            "the rider must walk off S2's cage over the upper gangway onto deck 6");
    (void)s2_sim.advance_frame(0.5);
    const auto on_deck6 = s2_sim.snapshot();
    require(on_deck6.player_grounded && on_deck6.player_position.y > kDeck6Top + 0.5 &&
                on_deck6.player_position.z < -124.0 &&
                on_deck6.support_entity_id == Simulation::kTowerEntityId,
            "the rider must stand on deck 6's south band");
    const double s2_floor_y = kit_y(s2_sim, Simulation::kStackS2CageEntityId) + 0.10;
    require(std::abs(s2_floor_y - kS2FloorTopUp) <= 0.05, "S2 floor must stop at 66.05 m");
    std::cout << "PASS scraperx_sim S2 ride: ride_s=" << s2_rode.seconds
              << " floor_y=" << s2_floor_y
              << " peak_speed=" << s2_top_state.s2_cage_peak_speed
              << " deck6_y=" << on_deck6.player_position.y << "\n";

    // Full Stack ascent in one run from grade: S1 (0->22m) -> C1 (22->44m) -> S2 (44->66m)
    require(take_s2_handle(band), "the Stack: from deck 4 into S2's cage and take hold of its lanyard");
    const auto band_s2_rode = ride_s2(band, 20.0);
    require(band_s2_rode.reached_top, "the Stack: S2 carries the rider to deck 6");
    require(walk_to(band, 10.0, -133.0, 6.0) && walk_to(band, 10.0, -126.0, 6.0),
            "the Stack: off S2 onto deck 6");
    (void)band.advance_frame(0.5);
    const auto band_deck6_top = band.snapshot();
    require(band_deck6_top.death_count == 0, "the Stack: from the yard to deck 6 without dying");
    require(band_deck6_top.player_grounded && band_deck6_top.player_position.y > kDeck6Top + 0.5,
            "the Stack: standing on deck 6");
    std::cout << "PASS scraperx_sim Stack to deck 6: seconds="
              << band_deck6_top.simulation_time_seconds - band_start
              << " deck6_y=" << band_deck6_top.player_position.y << "\n";
}

int main() {
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-007") {
        run_wet_isolation();
        run_wet_band();
        run_wet_route();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-007-route") {
        run_wet_route();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-007-band") {
        run_wet_band();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-008") {
        run_plate_shop();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-008-band") {
        run_plate_band();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-008-route") {
        run_plate_route();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-008-wreck") {
        run_plate_wreckage();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-009") {
        run_facade_crane();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-009-band") {
        run_crane_band();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-009-route") {
        run_crane_route();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "AS-009-wreck") {
        run_crane_wreckage();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "stack") {
        run_stack();
        return EXIT_SUCCESS;
    }
    if (const char *only = std::getenv("SCRAPERX_ONLY");
        only != nullptr && std::string(only) == "ascent") {
        run_ascent();
        return EXIT_SUCCESS;
    }
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::Vector3;
    using scraperx::sim::TraversalState;

    Simulation partitioned;
    for (std::uint32_t i = 0; i < Simulation::kTickRateHz; ++i) {
        const auto result = partitioned.advance_frame(Simulation::kFixedStepSeconds);
        require(result.accepted, "fixed-step input must be accepted");
        require(result.steps_advanced == 1, "each exact fixed step must advance once");
    }

    const auto partitioned_snapshot = partitioned.snapshot();
    require(partitioned_snapshot.tick_index == 90, "90 fixed steps must produce tick 90");
    require(nearly_equal(partitioned_snapshot.simulation_time_seconds, 1.0),
            "tick-derived simulation time must equal one second");

    Simulation batched;
    const auto batched_result = batched.advance_frame(1.0);
    const auto batched_snapshot = batched.snapshot();
    require(batched_result.accepted, "one-second frame input must be accepted");
    require(batched_result.steps_advanced == 90, "one second must advance 90 authoritative ticks");
    require(batched_snapshot.tick_index == partitioned_snapshot.tick_index,
            "frame partitioning must not change tick count");
    require(nearly_equal(batched_snapshot.simulation_time_seconds,
                         partitioned_snapshot.simulation_time_seconds),
            "frame partitioning must not change simulation time");
    require(nearly_equal(batched_snapshot.player_position.x,
                         partitioned_snapshot.player_position.x,
                         1.0e-6) &&
                nearly_equal(batched_snapshot.player_position.y,
                             partitioned_snapshot.player_position.y,
                             1.0e-6) &&
                nearly_equal(batched_snapshot.player_position.z,
                             partitioned_snapshot.player_position.z,
                             1.0e-6),
            "frame partitioning must not change native player position");

    Simulation remainder(InitialSpawn::StaticDeck);
    require(remainder.advance_frame(Simulation::kFixedStepSeconds * 0.5).steps_advanced == 0,
            "half a fixed step must remain buffered");
    require(remainder.advance_frame(Simulation::kFixedStepSeconds * 0.5).steps_advanced == 1,
            "two half steps must advance exactly once");
    require(nearly_equal(remainder.snapshot().interpolation_alpha, 0.0),
            "exactly consumed time must leave no interpolation remainder");

    const auto before_invalid = remainder.snapshot();
    require(!remainder.advance_frame(-0.001).accepted, "negative delta must be rejected");
    require(!remainder.advance_frame(std::numeric_limits<double>::quiet_NaN()).accepted,
            "NaN delta must be rejected");
    require(remainder.snapshot().tick_index == before_invalid.tick_index,
            "rejected frame input must not mutate authoritative state");

    Simulation supported(InitialSpawn::StaticDeck);
    require(supported.advance_frame(2.0).accepted,
            "static-deck settling interval must be accepted");
    const auto supported_snapshot = supported.snapshot();
    require(supported_snapshot.player_grounded,
            "the native player capsule must be grounded after falling onto the static deck");
    require(supported_snapshot.support_entity_id == Simulation::kStaticDeckEntityId,
            "static support must expose the stable deck entity ID");
    require(nearly_equal(supported_snapshot.player_position.y, 0.9, 0.03),
            "the supported capsule center must settle at the static deck contact height");
    require(horizontal_magnitude(supported_snapshot.support_point_linear_velocity) < 1.0e-5,
            "the static deck support-point velocity must be zero");

    require(supported.set_move_input(1.0, 0.0),
            "finite desired movement input must be accepted");
    require(supported.advance_frame(0.5).accepted,
            "static-deck locomotion interval must be accepted");
    const auto moved_snapshot = supported.snapshot();
    require(moved_snapshot.player_position.x > supported_snapshot.player_position.x + 1.0,
            "desired relative velocity input must move the native body across the static deck");
    require(moved_snapshot.player_grounded,
            "horizontal locomotion must preserve static-deck support");
    require(moved_snapshot.support_entity_id == Simulation::kStaticDeckEntityId,
            "static locomotion support identity must remain native and stable");

    require(!supported.set_move_input(std::numeric_limits<double>::quiet_NaN(), 0.0),
            "non-finite desired movement input must be rejected");
    const auto before_rejected_command = supported.snapshot();
    require(supported.advance_frame(0.25).accepted,
            "simulation must remain usable after rejecting a movement command");
    const auto after_rejected_command = supported.snapshot();
    require(horizontal_distance(after_rejected_command.player_position,
                                before_rejected_command.player_position) > 0.5,
            "a rejected movement command must not replace the last accepted command");

    Simulation translating(InitialSpawn::TranslatingSupport);
    require(translating.set_move_input(0.0, 0.0),
            "zero relative movement must be accepted on a translating support");
    require(translating.advance_frame(1.0).accepted,
            "translating-support settling interval must be accepted");
    const auto translating_grounded = translating.snapshot();
    require(translating_grounded.player_grounded,
            "the player must settle on the native translating support");
    require(translating_grounded.support_entity_id == Simulation::kTranslatingSupportEntityId,
            "translating support must expose its stable native entity ID");
    require(std::abs(translating_grounded.support_point_linear_velocity.x) > 0.5,
            "translating support must expose non-zero contact-point velocity");
    require(std::abs(translating_grounded.player_linear_velocity.x -
                     translating_grounded.support_point_linear_velocity.x) < 0.75,
            "zero-input grounded locomotion must remain relative to translating support velocity");

    const auto translating_support_velocity =
        translating_grounded.support_point_linear_velocity;
    require(translating.request_jump(), "grounded jump request must be accepted");
    require(translating.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "jump tick must advance");
    const auto translating_jump = translating.snapshot();
    require(!translating_jump.player_grounded,
            "jump must detach the player from translating support");
    require(translating_jump.player_linear_velocity.y > 4.0,
            "jump must create upward velocity without teleporting");
    require(translating_jump.player_linear_velocity.x * translating_support_velocity.x > 0.0,
            "jump must preserve the translating support velocity direction");
    require(std::abs(translating_jump.player_linear_velocity.x) >
                std::abs(translating_support_velocity.x) * 0.45,
            "jump must preserve a material fraction of inherited translating support velocity");

    require(translating.advance_frame(0.20).accepted,
            "airborne inherited-momentum interval must advance");
    const auto translating_airborne = translating.snapshot();
    require(!translating_airborne.player_grounded,
            "short post-jump interval must remain airborne");
    require(translating_airborne.player_linear_velocity.x * translating_support_velocity.x > 0.0,
            "air control must not immediately cancel inherited translating support momentum");

    Simulation rotating(InitialSpawn::RotatingSupport);
    require(rotating.set_move_input(0.0, 0.0),
            "zero relative movement must be accepted on a rotating support");
    require(rotating.advance_frame(1.2).accepted,
            "rotating-support settling interval must be accepted");
    const auto rotating_grounded = rotating.snapshot();
    require(rotating_grounded.player_grounded,
            "the player must settle on the native rotating support");
    require(rotating_grounded.support_entity_id == Simulation::kRotatingSupportEntityId,
            "rotating support must expose its stable native entity ID");
    require(std::abs(rotating_grounded.rotating_support_angular_velocity.y) > 0.5,
            "rotating support must expose material native angular velocity");
    require(horizontal_magnitude(rotating_grounded.support_point_linear_velocity) > 0.5,
            "rotating support must produce non-zero support-point linear velocity away from its axis");

    const double radius_x =
        rotating_grounded.support_contact_point.x - rotating_grounded.rotating_support_position.x;
    const double radius_z =
        rotating_grounded.support_contact_point.z - rotating_grounded.rotating_support_position.z;
    const double omega_y = rotating_grounded.rotating_support_angular_velocity.y;
    const double expected_point_x = omega_y * radius_z;
    const double expected_point_z = -omega_y * radius_x;
    require(nearly_equal(rotating_grounded.support_point_linear_velocity.x,
                         expected_point_x,
                         0.12) &&
                nearly_equal(rotating_grounded.support_point_linear_velocity.z,
                             expected_point_z,
                             0.12),
            "rotating support point velocity must obey omega cross r at the actual contact point");
    require(horizontal_dot(rotating_grounded.player_linear_velocity,
                           rotating_grounded.support_point_linear_velocity) > 0.15,
            "rotating support motion must be imparted to the grounded player");


    // ---- WO-003 athletic traversal ---------------------------------------

    Simulation vault(InitialSpawn::VaultApproach);
    require(vault.set_facing(1.0, 0.0), "vault facing must be accepted");
    require(vault.set_move_input(0.0, 0.0), "vault settle input must be accepted");
    require(vault.advance_frame(1.0).accepted, "vault settling interval must be accepted");
    require(vault.snapshot().player_grounded, "vault approach must settle on the static deck");
    require(vault.set_move_input(1.0, 0.0), "vault approach input must be accepted");
    require(advance_until(vault,
                          [](const Snapshot &state) {
                              return state.ledge_available &&
                                     state.ledge_entity_id == Simulation::kVaultRailEntityId;
                          },
                          2.0),
            "the native geometry probe must offer the real vault rail");

    const auto vault_ready = vault.snapshot();
    require(vault_ready.player_linear_velocity.x > 3.0,
            "the vault must be requested with material approach momentum");
    require(vault.request_traversal(), "vault traversal request must be accepted");
    require(vault.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "vault commit tick must advance");
    const auto vault_committed = vault.snapshot();
    require(vault_committed.traversal_state == TraversalState::Vaulting,
            "a real rail with a clear far landing must commit a native vault");
    require(vault_committed.traversal_support_entity_id == Simulation::kVaultRailEntityId,
            "the committed vault must name the real rail entity");
    require(advance_until(vault,
                          [](const Snapshot &state) {
                              return state.accepted_traversal_count >= 1;
                          },
                          1.5),
            "the committed vault must complete on the authoritative clock");

    const auto vaulted = vault.snapshot();
    require(vaulted.player_position.x > 5.6,
            "the vault must cross the rail and land on the far side");
    require(horizontal_magnitude(vaulted.player_linear_velocity) > 3.0,
            "the vault must not erase the player's approach momentum");
    require(vaulted.aborted_traversal_count == 0, "a valid vault must not abort");
    require(vaulted.rejected_traversal_count == 0, "a valid vault must not be rejected");

    // Double-tap Jump: the second press inside the window after a takeoff
    // vaults the same rail, on the ground vault's own terms; a second press
    // after the window, or with nothing vaultable ahead, is only a jump.
    const auto jump_vault_attempt = [](const double facing_x, const std::uint32_t gap_ticks) {
        Simulation run(InitialSpawn::VaultApproach);
        (void)run.set_facing(facing_x, 0.0);
        (void)run.set_move_input(0.0, 0.0);
        (void)run.advance_frame(1.0);
        (void)run.set_move_input(facing_x, 0.0);
        if (facing_x > 0.0) {
            (void)advance_until(run,
                                [](const Snapshot &state) {
                                    return state.ledge_available &&
                                           state.ledge_entity_id == Simulation::kVaultRailEntityId;
                                },
                                2.0);
        } else {
            (void)run.advance_frame(0.5);
        }
        (void)run.request_jump();
        for (std::uint32_t tick = 0; tick <= gap_ticks; ++tick) {
            (void)run.advance_frame(Simulation::kFixedStepSeconds);
        }
        (void)run.request_jump();
        (void)run.advance_frame(Simulation::kFixedStepSeconds);
        const bool vaulting = run.snapshot().traversal_state == TraversalState::Vaulting;
        (void)advance_until(run, [](const Snapshot &state) { return state.player_grounded &&
                                                                    state.traversal_state ==
                                                                        TraversalState::None; },
                            3.0);
        return std::make_pair(vaulting, run.snapshot());
    };
    const auto [double_tap_vaulting, double_tapped] = jump_vault_attempt(1.0, 12);
    require(double_tap_vaulting && double_tapped.jump_vault_count == 1,
            "a second Jump 0.13 s after takeoff at the rail must commit a vault");
    require(double_tapped.player_position.x > 5.6 && double_tapped.aborted_traversal_count == 0,
            "the double-tap vault must cross the rail and land on the far side");
    const auto [late_tap_vaulting, late_tapped] = jump_vault_attempt(1.0, 40);
    require(!late_tap_vaulting && late_tapped.jump_vault_count == 0,
            "a second Jump after the 0.30 s window must not vault");
    const auto [away_tap_vaulting, away_tapped] = jump_vault_attempt(-1.0, 12);
    require(!away_tap_vaulting && away_tapped.jump_vault_count == 0,
            "a double-tap with nothing vaultable ahead must not vault");

    Simulation mantle(InitialSpawn::MantleApproach);
    require(mantle.set_facing(1.0, 0.0), "mantle facing must be accepted");
    require(mantle.advance_frame(1.0).accepted, "mantle settling interval must be accepted");
    const auto mantle_ready = mantle.snapshot();
    require(mantle_ready.player_grounded, "mantle approach must settle on the static deck");
    require(mantle_ready.ledge_available &&
                mantle_ready.ledge_entity_id == Simulation::kMantleLedgeEntityId,
            "the native geometry probe must offer the real mantle ledge");
    require(mantle_ready.ledge_rise_meters > 1.4 && mantle_ready.ledge_rise_meters < 1.7,
            "the offered ledge rise must match the real ledge geometry");
    require(mantle.request_traversal(), "mantle traversal request must be accepted");
    require(mantle.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "mantle commit tick must advance");
    require(mantle.snapshot().traversal_state == TraversalState::Mantling,
            "a real ledge above vault height must commit a native mantle");
    // A standing mantle closes on the wall before it climbs. The ledge is
    // offered an arm's length back (x = 7.9 against the face at 9.0); by the
    // time the body has risen 0.1 m the capsule must stand at the hang
    // standoff: outside the face (ledge point - top-probe inset 0.12 -
    // radius 0.35 = 0.47 m back) and within reach of the lip.
    double mantle_rise_x = mantle_ready.player_position.x;
    require(advance_until(mantle,
                          [&](const Snapshot &state) {
                              mantle_rise_x = state.player_position.x;
                              return state.player_position.y > mantle_ready.player_position.y + 0.1;
                          },
                          1.0),
            "the standing mantle must begin to rise");
    const double mantle_rise_setback = mantle_ready.ledge_point.x - mantle_rise_x;
    require(mantle_rise_setback > 0.47 && mantle_rise_setback < 0.60,
            "a standing mantle must step in to the hang standoff before it rises");
    require(advance_until(mantle,
                          [](const Snapshot &state) {
                              return state.player_grounded &&
                                     state.support_entity_id == Simulation::kMantleLedgeEntityId;
                          },
                          2.0),
            "the mantle must end grounded on the real ledge entity");

    const auto mantled = mantle.snapshot();
    require(mantled.player_position.y > 2.3 && mantled.player_position.y < 2.6,
            "the mantled player must stand at the real ledge contact height");
    require(mantled.accepted_traversal_count == 1, "the mantle must record one accepted traversal");
    require(mantled.aborted_traversal_count == 0, "a valid mantle must not abort");

    Simulation hang(InitialSpawn::HangApproach);
    require(hang.set_facing(1.0, 0.0), "hang facing must be accepted");
    require(hang.set_move_input(1.0, 0.0), "hang approach input must be accepted");
    require(advance_until(hang,
                          [](const Snapshot &state) {
                              return state.traversal_state == TraversalState::Hanging;
                          },
                          2.0),
            "falling beside a real high ledge must produce a native hang");

    const auto hang_start = hang.snapshot();
    require(hang_start.traversal_support_entity_id == Simulation::kHangLedgeEntityId,
            "the hang must name the real ledge entity");
    require(!hang_start.player_grounded, "a hang is not grounded support");
    require(hang.advance_frame(1.0).accepted, "hang hold interval must be accepted");
    const auto hang_held = hang.snapshot();
    require(hang_held.traversal_state == TraversalState::Hanging,
            "the hang must hold against gravity on real geometry");
    require(std::abs(hang_held.player_position.y - hang_start.player_position.y) < 0.05,
            "a hang on a static ledge must not drift");

    require(hang.request_jump(), "mantle-from-hang request must be accepted");
    require(hang.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "hang mantle commit tick must advance");
    require(hang.snapshot().traversal_state == TraversalState::Mantling,
            "a jump from a hang must commit the validated mantle");
    require(advance_until(hang,
                          [](const Snapshot &state) {
                              return state.player_grounded &&
                                     state.support_entity_id == Simulation::kHangLedgeEntityId;
                          },
                          2.0),
            "the hang mantle must end grounded on the same real ledge");
    require(hang.snapshot().player_position.y > 4.4,
            "the hang mantle must lift the player onto the real ledge top");

    Simulation moving(InitialSpawn::MovingLedgeApproach);
    require(moving.set_facing(1.0, 0.0), "moving-ledge facing must be accepted");
    require(moving.set_move_input(1.0, 0.0), "moving-ledge approach input must be accepted");
    require(advance_until(moving,
                          [](const Snapshot &state) {
                              return state.traversal_state == TraversalState::Hanging;
                          },
                          2.0),
            "falling beside the kinematic moving ledge must produce a native hang");
    require(moving.snapshot().traversal_support_entity_id == Simulation::kMovingLedgeEntityId,
            "the moving hang must name the kinematic ledge entity");

    require(moving.advance_frame(0.5).accepted, "moving-hang interval must be accepted");
    const auto carried = moving.snapshot();
    require(carried.traversal_state == TraversalState::Hanging,
            "the hang must survive the support moving beneath it");
    require(std::abs(carried.moving_ledge_linear_velocity.z) > 0.3,
            "the moving ledge must actually be translating");
    require(std::abs(carried.player_linear_velocity.z - carried.moving_ledge_linear_velocity.z) < 0.35,
            "a hang on a moving support must be carried at the support's own velocity");

    const double hang_offset_before = carried.player_position.z - carried.moving_ledge_position.z;
    require(moving.advance_frame(0.5).accepted, "second moving-hang interval must be accepted");
    const auto carried_later = moving.snapshot();
    const double hang_offset_after =
        carried_later.player_position.z - carried_later.moving_ledge_position.z;
    require(std::abs(hang_offset_after - hang_offset_before) < 0.05,
            "the hang hold must stay fixed in the moving support's own frame");
    require(std::abs(carried_later.player_position.z - carried.player_position.z) > 0.15,
            "the carried hang must move through the world with its support");

    require(moving.request_traversal(), "moving-ledge mantle request must be accepted");
    require(moving.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "moving-ledge mantle commit tick must advance");
    require(moving.snapshot().traversal_state == TraversalState::Mantling,
            "a traversal request from a moving hang must commit the validated mantle");
    require(advance_until(moving,
                          [](const Snapshot &state) {
                              return state.player_grounded &&
                                     state.support_entity_id == Simulation::kMovingLedgeEntityId;
                          },
                          2.0),
            "the moving mantle must end grounded on the kinematic support");

    Simulation released(InitialSpawn::MovingLedgeApproach);
    require(released.set_facing(1.0, 0.0), "release-test facing must be accepted");
    require(released.set_move_input(1.0, 0.0), "release-test approach input must be accepted");
    require(advance_until(released,
                          [](const Snapshot &state) {
                              return state.traversal_state == TraversalState::Hanging;
                          },
                          2.0),
            "release test must first reach a native hang on the moving ledge");
    require(released.advance_frame(0.4).accepted, "release-test hang interval must be accepted");
    const auto before_release = released.snapshot();
    require(std::abs(before_release.moving_ledge_linear_velocity.z) > 0.3,
            "the release test must run while the support is actually moving");
    require(released.request_release(), "a hanging player must be allowed to let go");
    require(released.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "release tick must advance");
    const auto after_release = released.snapshot();
    require(after_release.traversal_state == TraversalState::None,
            "releasing a hang must end the traversal state");
    require(std::abs(after_release.player_linear_velocity.z -
                     before_release.moving_ledge_linear_velocity.z) < 0.35,
            "releasing a moving-support hang must inherit the support point velocity");
    require(released.advance_frame(0.3).accepted, "post-release fall interval must be accepted");
    const auto falling = released.snapshot();
    require(falling.player_position.y < after_release.player_position.y - 0.2,
            "a released hang must fall under gravity again");
    require(!released.request_release(),
            "release must be refused when the player is not hanging");

    Simulation blocked(InitialSpawn::BlockedLedgeApproach);
    require(blocked.set_facing(-1.0, 0.0), "blocked-ledge facing must be accepted");
    require(blocked.advance_frame(1.0).accepted, "blocked-ledge settling interval must be accepted");
    const auto blocked_ready = blocked.snapshot();
    require(blocked_ready.player_grounded, "blocked-ledge approach must settle on the static deck");
    require(!blocked_ready.ledge_available,
            "a ledge whose landing pose is obstructed must not be offered");
    require(blocked.request_traversal(), "a first traversal request must always be queued");
    require(blocked.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "blocked-ledge decision tick must advance");
    const auto blocked_result = blocked.snapshot();
    require(blocked_result.traversal_state == TraversalState::None,
            "an obstructed landing must not start a traversal");
    require(blocked_result.rejected_traversal_count == 1,
            "the authoritative reject counter must record the refusal");
    require(blocked_result.accepted_traversal_count == 0,
            "a refused traversal must not be counted as accepted");
    require(blocked_result.player_position.y < blocked_ready.player_position.y + 0.05,
            "a refused traversal must not raise the player through the blocker");

    const auto partitioned_mantle = run_mantle_command_stream(true);
    const auto batched_mantle = run_mantle_command_stream(false);
    require(partitioned_mantle.tick_index == batched_mantle.tick_index,
            "traversal frame partitioning must not change tick count");
    require(partitioned_mantle.accepted_traversal_count == 1 &&
                batched_mantle.accepted_traversal_count == 1,
            "both partitions must complete exactly one traversal");
    require(nearly_equal(partitioned_mantle.player_position.x,
                         batched_mantle.player_position.x,
                         1.0e-6) &&
                nearly_equal(partitioned_mantle.player_position.y,
                             batched_mantle.player_position.y,
                             1.0e-6) &&
                nearly_equal(partitioned_mantle.player_position.z,
                             batched_mantle.player_position.z,
                             1.0e-6),
            "traversal frame partitioning must not change native player position");


    // The game starts outdoors at grade, short of the tower.
    Simulation start(InitialSpawn::ExteriorGrade);
    const auto start_state = start.snapshot();
    require(start_state.player_position.z < -20.0 && start_state.player_position.y < 2.0,
            "the default spawn must be outdoors at grade, short of the tower");

    std::cout << "PASS scraperx_sim moving-support truth: translating_support="
              << translating_grounded.support_entity_id
              << " translating_vx=" << translating_support_velocity.x
              << " jump_vx=" << translating_jump.player_linear_velocity.x
              << " rotating_support=" << rotating_grounded.support_entity_id
              << " rotating_point_v=("
              << rotating_grounded.support_point_linear_velocity.x << ','
              << rotating_grounded.support_point_linear_velocity.z << ')'
              << " omega_y=" << omega_y
              << " hz=" << Simulation::kTickRateHz << '\n';
    std::cout << "PASS scraperx_sim athletic traversal: vault_x=" << vaulted.player_position.x
              << " double_tap_vault_x=" << double_tapped.player_position.x
              << " vault_speed=" << horizontal_magnitude(vaulted.player_linear_velocity)
              << " mantle_support=" << mantled.support_entity_id
              << " mantle_rise_setback=" << mantle_rise_setback
              << " mantle_y=" << mantled.player_position.y
              << " hang_support=" << hang_start.traversal_support_entity_id
              << " moving_hang_support=" << carried.traversal_support_entity_id
              << " moving_hang_vz=" << carried.player_linear_velocity.z
              << " moving_ledge_vz=" << carried.moving_ledge_linear_velocity.z
              << " release_vz=" << after_release.player_linear_velocity.z
              << " rejected=" << blocked_result.rejected_traversal_count
              << " accepted=" << moving.snapshot().accepted_traversal_count << '\n';
    // ---- WO-008 fall / parachute / checkpoint ----------------------------

    Simulation lethal(InitialSpawn::HighDrop);
    const auto lethal_start = lethal.snapshot();
    require(lethal_start.death_count == 0, "a fresh simulation must start with zero deaths");
    require(!advance_until(lethal,
                           [](const Snapshot &state) { return state.death_count >= 1; },
                           0.01),
            "death must not be instantaneous: the drop must actually take real time");
    require(advance_until(lethal,
                          [](const Snapshot &state) { return state.death_count >= 1; },
                          6.0),
            "an unmitigated ~61 m fall must be lethal");
    const auto lethal_result = lethal.snapshot();
    require(lethal_result.last_impact_speed_mps > 20.0,
            "the recorded impact speed must actually exceed the lethal threshold");
    require(nearly_equal(lethal_result.checkpoint_position.x, 0.0, 0.05) &&
                nearly_equal(lethal_result.checkpoint_position.y, 0.9, 0.05) &&
                nearly_equal(lethal_result.checkpoint_position.z, 0.0, 0.05),
            "with no prior real commit, death must restore the seeded safe checkpoint");
    require(std::abs(lethal_result.player_position.x - lethal_result.checkpoint_position.x) <
                    0.05 &&
                std::abs(lethal_result.player_position.y - lethal_result.checkpoint_position.y) <
                    0.05 &&
                std::abs(lethal_result.player_position.z - lethal_result.checkpoint_position.z) <
                    0.05,
            "death must actually move the player to the checkpoint, not leave them at the "
            "fatal impact site");
    require(std::abs(lethal_result.player_linear_velocity.y) < 0.05,
            "a checkpoint restore must zero velocity, not merely reposition the body");

    Simulation survivable(InitialSpawn::SurvivableDrop);
    require(advance_until(survivable,
                          [](const Snapshot &state) { return state.player_grounded; },
                          4.0),
            "the short drop must land");
    require(survivable.snapshot().death_count == 0,
            "an ordinary ~12 m platforming fall must never be lethal (GDD 8.2)");

    Simulation chuted(InitialSpawn::HighDrop);
    require(chuted.advance_frame(0.5).accepted, "early free-fall interval must be accepted");
    require(chuted.snapshot().fall_state == scraperx::sim::FallState::Airborne,
            "the player must be genuinely airborne before deploying");
    require(chuted.request_parachute(), "an airborne parachute deploy request must be accepted");
    require(chuted.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "the deploy tick must advance");
    require(chuted.snapshot().parachute_deployed, "the parachute must show as deployed");
    require(chuted.snapshot().fall_state == scraperx::sim::FallState::Parachuting,
            "fall_state must report Parachuting once deployed");
    require(advance_until(chuted,
                          [](const Snapshot &state) { return state.player_grounded; },
                          10.0),
            "a parachuted fall must still land");
    const auto chuted_result = chuted.snapshot();
    require(chuted_result.death_count == 0,
            "deploying early enough must make a lethal-height fall survivable");
    require(chuted_result.last_impact_speed_mps < 12.0,
            "the parachute must measurably reduce impact speed toward its terminal value");
    require(chuted_result.last_impact_speed_mps > 5.0,
            "drag must be a real decelerating force, not an instant velocity clamp to near-zero");

    Simulation late_chute(InitialSpawn::HighDrop);
    require(advance_until(late_chute,
                          [](const Snapshot &state) {
                              return state.player_position.y < 3.0;
                          },
                          6.0),
            "the late-deploy test must reach low altitude while still airborne");
    require(!late_chute.snapshot().player_grounded,
            "the late-deploy test must still be airborne at low altitude");
    require(late_chute.request_parachute(),
            "a late airborne parachute deploy request must still be accepted");
    require(advance_until(late_chute,
                          [](const Snapshot &state) { return state.death_count >= 1; },
                          2.0),
            "deploying too late must not fabricate a save: the fall must still kill");

    Simulation grounded_parachute(InitialSpawn::StaticDeck);
    require(grounded_parachute.advance_frame(1.0).accepted,
            "grounded settling interval must be accepted");
    require(grounded_parachute.snapshot().player_grounded,
            "the grounded-parachute test must start grounded");
    require(grounded_parachute.request_parachute(),
            "a parachute request while grounded must be queued, not rejected");
    require(grounded_parachute.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "the grounded deploy-attempt tick must advance");
    require(!grounded_parachute.snapshot().parachute_deployed,
            "a deploy request while grounded must produce no state change (GDD 8.3)");

    // The checkpoint follows the last firm footing, and freezes in the air.
    Simulation committed(InitialSpawn::StaticDeck);
    require(committed.set_facing(0.0, 1.0), "checkpoint test facing must be accepted");
    require(committed.set_move_input(0.0, 1.0), "checkpoint test walk input must be accepted");
    require(committed.advance_frame(1.0).accepted, "the checkpoint test walk must advance");
    require(committed.set_move_input(0.0, 0.0), "checkpoint test halt input must be accepted");
    const auto pre_commit_checkpoints = committed.snapshot().checkpoint_commit_count;
    require(committed.advance_frame(1.0).accepted,
            "standing must advance and keep auto-committing");
    const auto disturbed_checkpoint = committed.snapshot();
    require(disturbed_checkpoint.player_grounded, "the checkpoint test must be standing");
    require(disturbed_checkpoint.checkpoint_commit_count > pre_commit_checkpoints,
            "standing grounded must keep advancing the automatic commit count");
    require(std::abs(disturbed_checkpoint.checkpoint_position.x -
                     disturbed_checkpoint.player_position.x) < 0.05 &&
                std::abs(disturbed_checkpoint.checkpoint_position.z -
                         disturbed_checkpoint.player_position.z) < 0.05,
            "the committed checkpoint position must track the player's current grounded spot");

    require(committed.set_facing(0.0, 1.0), "checkpoint jump-away facing must be accepted");
    require(committed.set_move_input(0.0, 0.0), "checkpoint jump-away input must be accepted");
    require(committed.request_jump(), "the checkpoint test jump must be accepted");
    require(committed.advance_frame(0.15).accepted, "the jump-away tick must advance");
    require(!committed.snapshot().player_grounded, "the checkpoint test must now be airborne");
    const auto airborne_checkpoint = committed.snapshot();
    require(nearly_equal(airborne_checkpoint.checkpoint_position.x,
                         disturbed_checkpoint.checkpoint_position.x, 1.0e-6) &&
                nearly_equal(airborne_checkpoint.checkpoint_position.y,
                             disturbed_checkpoint.checkpoint_position.y, 1.0e-6) &&
                nearly_equal(airborne_checkpoint.checkpoint_position.z,
                             disturbed_checkpoint.checkpoint_position.z, 1.0e-6),
            "leaving the ground must freeze the checkpoint at the last grounded position, not "
            "track mid-air position");

    // Frame-partition invariance for the whole subsystem.
    Simulation fall_partitioned(InitialSpawn::HighDrop);
    for (std::uint32_t tick = 0; tick < Simulation::kTickRateHz * 4; ++tick) {
        require(fall_partitioned.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "fall-subsystem partition step must be accepted");
    }
    Simulation fall_batched(InitialSpawn::HighDrop);
    require(fall_batched.advance_frame(4.0).accepted,
            "fall-subsystem batched interval must be accepted");
    const auto partitioned_fall = fall_partitioned.snapshot();
    const auto batched_fall = fall_batched.snapshot();
    require(partitioned_fall.tick_index == batched_fall.tick_index,
            "fall-subsystem frame partitioning must not change tick count");
    require(partitioned_fall.death_count == batched_fall.death_count,
            "fall-subsystem frame partitioning must not change death count");
    require(nearly_equal(partitioned_fall.player_position.x, batched_fall.player_position.x,
                         1.0e-5) &&
                nearly_equal(partitioned_fall.player_position.y, batched_fall.player_position.y,
                             1.0e-5) &&
                nearly_equal(partitioned_fall.player_position.z, batched_fall.player_position.z,
                             1.0e-5),
            "fall-subsystem frame partitioning must not change native player position");

    std::cout << "PASS scraperx_sim fall/parachute/checkpoint: lethal_impact="
              << lethal_result.last_impact_speed_mps
              << " chuted_impact=" << chuted_result.last_impact_speed_mps
              << " late_chute_deaths=" << late_chute.snapshot().death_count
              << " grounded_deploy_blocked="
              << int(!grounded_parachute.snapshot().parachute_deployed)
              << " commits=" << airborne_checkpoint.checkpoint_commit_count << '\n';

    // ---- World solids: every visible body is a body -----------------------
    // The dressing drawn by the Godot builders (world_solids.inc) is native
    // collision, not decoration. The buttress footings at the stack's corners
    // are the case a player walked straight through: a 6 x 2.6 x 6 m block
    // under a 2.4 m splayed leg. Walked into from the yard, the capsule must
    // stop at its face -- centre never nearer than half-width (3.0 m) plus
    // most of the capsule radius (0.35 m) on the dominant axis.
    Simulation solid(InitialSpawn::ExteriorGrade);
    require(solid.advance_frame(0.5).accepted, "world-solids settling interval must be accepted");
    const auto solid_loaded = solid.snapshot();
    require(solid_loaded.world_solid_rejected == 0,
            "every generated hull must be a valid Jolt convex shape");
    require(solid_loaded.world_solid_bodies > 1400,
            "the generated world-solid table must load as native bodies");
    require(solid_loaded.world_solid_mirrors > 0,
            "drawn mirrors of owned bodies must be recognised and not doubled");
    constexpr double kFootX = -39.0;
    constexpr double kFootZ = -111.0;
    double foot_closest = 1.0e9;
    const auto track_foot = [&]() {
        const auto at = solid.snapshot().player_position;
        foot_closest = std::min(foot_closest,
                                std::max(std::abs(at.x - kFootX), std::abs(at.z - kFootZ)));
    };
    for (std::uint32_t tick = 0; tick < 90 * 14; ++tick) {
        walk_toward(solid, -30.0, -100.0, Simulation::kFixedStepSeconds);
        track_foot();
    }
    for (std::uint32_t tick = 0; tick < 90 * 4; ++tick) {
        walk_toward(solid, kFootX, kFootZ, Simulation::kFixedStepSeconds);
        track_foot();
    }
    require(foot_closest < 3.6, "the walk must actually reach the buttress footing");
    require(foot_closest > 3.25,
            "walking into a buttress footing must be stopped at its face by native collision");
    require(solid.snapshot().player_grounded,
            "the player stopped by the footing must still stand on the yard");

    // No ramp and no stair: walking the stack's bands at grade, under the
    // decks where fourteen flights once climbed, never leaves the ground.
    Simulation stair(InitialSpawn::ExteriorGrade);
    require(stair.advance_frame(0.5).accepted, "stack walk settling interval must be accepted");
    walk_toward(stair, -25.0, -118.0, 20.0);
    walk_toward(stair, -25.0, -128.5, 6.0);
    double stack_walk_peak_y = 0.0;
    for (const double side : {1.0, -1.0}) {
        const double band = -150.0 + side * 21.5;
        for (std::uint32_t tick = 0; tick < 90 * 12; ++tick) {
            walk_toward(stair, side * 21.0, band, Simulation::kFixedStepSeconds);
            stack_walk_peak_y = std::max(stack_walk_peak_y, stair.snapshot().player_position.y);
        }
    }
    require(stack_walk_peak_y < 2.0, "no ramp or stair may carry a walker off the grade");

    // The yard sits in a basin, not on a slab in a void: off the grade's
    // east edge the valley floor carries the player 0.3 m lower, the step
    // back up is walked, and walking out to the rim ends against rock --
    // grounded, low, and never past the ridge line.
    Simulation basin(InitialSpawn::ExteriorGrade);
    require(basin.advance_frame(0.5).accepted, "basin settling interval must be accepted");
    walk_toward(basin, 270.0, -60.0, 70.0);
    const auto on_valley = basin.snapshot();
    require(on_valley.player_grounded && on_valley.player_position.x > 250.0 &&
                std::abs(on_valley.player_position.y - (-0.3 + 0.9)) < 0.1,
            "walking off the grade must land on the valley floor, not fall");
    walk_toward(basin, 200.0, -60.0, 20.0);
    const auto back_on_grade = basin.snapshot();
    require(back_on_grade.player_grounded && back_on_grade.player_position.x < 230.0 &&
                back_on_grade.player_position.y > 0.8,
            "the 0.3 m step from the valley floor back onto the grade must be walked");
    walk_toward(basin, 200.0, 1400.0, 320.0);
    const auto at_rim = basin.snapshot();
    const double rim_reach = std::hypot(at_rim.player_position.x - 0.0,
                                        at_rim.player_position.z - (-100.0));
    require(at_rim.player_grounded && at_rim.player_position.y < 5.0 && rim_reach < 1100.0,
            "walking out to the basin rim must end against its rock, low and grounded");

    std::cout << "PASS scraperx_sim world solids: bodies=" << solid_loaded.world_solid_bodies
              << " mirrors=" << solid_loaded.world_solid_mirrors
              << " rejected=" << solid_loaded.world_solid_rejected
              << " buttress_foot_closest=" << foot_closest
              << " stack_walk_peak_y=" << stack_walk_peak_y
              << " rim_reach=" << rim_reach << '\n';

    // Crouch (GDD 7.2, Governing Law 4). The crawl beam beside the traversal
    // fixtures has its underside 1.45 m over the deck: under the standing
    // capsule's 1.8 m, over the crouched one's 1.2 m. Standing, the beam
    // stops the body at its face; crouched, it walks under; beneath it,
    // letting go of crouch does not stand it up, and neither Jump nor Action
    // does anything; walked out past it, it stands up by itself.
    constexpr double kCrawlLaneX = -2.0;
    constexpr double kCrawlBeamZ = -13.0;
    constexpr double kCrawlBeamHalfZ = 0.3;
    constexpr double kCrouchSpeedLimit = 5.5 * 0.45;
    Simulation crouch(InitialSpawn::StaticDeck);
    require(crouch.advance_frame(1.0).accepted, "crouch settling interval must be accepted");
    require(crouch.snapshot().player_grounded && !crouch.snapshot().player_crouched,
            "the crouch run must start standing on the deck");
    const double standing_deepest = walk_toward(crouch, kCrawlLaneX, kCrawlBeamZ - 2.0, 5.0);
    require(standing_deepest > kCrawlBeamZ + kCrawlBeamHalfZ,
            "a standing capsule must not get under a beam 1.45 m over the deck");
    walk_toward(crouch, kCrawlLaneX, kCrawlBeamZ + 3.0, 2.0);
    require(crouch.set_move_input(0.0, 0.0), "crouch stop input must be accepted");
    require(crouch.advance_frame(0.5).accepted, "crouch standing settle must be accepted");
    const auto crouch_standing = crouch.snapshot();
    require(crouch_standing.player_grounded && !crouch_standing.player_crouched,
            "backing off the beam must leave the body standing");

    require(crouch.set_crouch_input(true), "crouch input must be accepted");
    require(crouch.advance_frame(0.3).accepted, "crouch interval must be accepted");
    const auto crouched = crouch.snapshot();
    const double crouch_drop = crouch_standing.player_position.y - crouched.player_position.y;
    require(crouched.player_crouched && crouched.player_grounded,
            "crouch held on the ground must crouch the body");
    require(std::abs(crouch_drop - 0.3) < 0.03,
            "crouching must lower the centre by 0.3 m and leave the feet where they were");

    double crouched_top_speed = 0.0;
    for (int i = 0; i < 4 * 90; ++i) {
        const auto state = crouch.snapshot();
        double dx = kCrawlLaneX - state.player_position.x;
        double dz = kCrawlBeamZ - state.player_position.z;
        const double len = std::hypot(dx, dz);
        if (len < 0.05) {
            break;
        }
        (void)crouch.set_move_input(dx / len, dz / len);
        (void)crouch.set_facing(dx / len, dz / len);
        (void)crouch.advance_frame(Simulation::kFixedStepSeconds);
        crouched_top_speed = std::max(crouched_top_speed,
                                      horizontal_magnitude(crouch.snapshot().player_linear_velocity));
    }
    require(crouch.set_move_input(0.0, 0.0), "crouch stop input must be accepted");
    require(crouch.advance_frame(0.3).accepted, "crouch under-beam settle must be accepted");
    const auto under = crouch.snapshot();
    require(std::abs(under.player_position.z - kCrawlBeamZ) < 0.3 && under.player_crouched &&
                under.player_grounded,
            "a crouched capsule must walk in under the beam and stand on the deck there");
    require(crouched_top_speed > 2.0 && crouched_top_speed < kCrouchSpeedLimit + 0.05,
            "crouched walking must run at the crouch speed, not the standing one");

    require(crouch.set_crouch_input(false), "crouch release must be accepted");
    require(crouch.advance_frame(0.5).accepted, "crouch release interval must be accepted");
    require(crouch.snapshot().player_crouched,
            "under the beam, letting go of crouch must not stand the body up into it");
    const double under_y = crouch.snapshot().player_position.y;
    const auto rejected_before = crouch.snapshot().rejected_traversal_count;
    require(crouch.request_jump(), "a jump request under the beam must be queued");
    require(crouch.advance_frame(0.3).accepted, "under-beam jump interval must be accepted");
    const auto after_jump = crouch.snapshot();
    require(after_jump.player_crouched && after_jump.player_grounded &&
                after_jump.player_position.y < under_y + 0.05,
            "with no room to stand there is no jump");
    require(crouch.request_traversal(), "a traversal request under the beam must be queued");
    require(crouch.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "under-beam traversal tick must advance");
    require(crouch.snapshot().rejected_traversal_count == rejected_before + 1 &&
                crouch.snapshot().traversal_state == TraversalState::None,
            "a traversal request with no room to stand must be refused, and counted");

    walk_toward(crouch, kCrawlLaneX, kCrawlBeamZ - 2.0, 3.0);
    require(crouch.set_move_input(0.0, 0.0), "crouch exit stop input must be accepted");
    require(crouch.advance_frame(0.5).accepted, "crouch exit settle must be accepted");
    const auto beyond = crouch.snapshot();
    require(beyond.player_position.z < kCrawlBeamZ - kCrawlBeamHalfZ - 0.35,
            "the released-crouch body must still walk on out from under the beam");
    require(!beyond.player_crouched && beyond.player_grounded &&
                std::abs(beyond.player_position.y - crouch_standing.player_position.y) < 0.03,
            "clear of the beam, a released crouch must stand the body back up");

    std::cout << "PASS scraperx_sim crouch: standing_deepest_z=" << standing_deepest
              << " crouch_drop=" << crouch_drop << " crouched_top_speed=" << crouched_top_speed
              << " under_z=" << under.player_position.z
              << " stood_y=" << beyond.player_position.y << '\n';

    // ---- AS-006 Stage A, the skip lift (03_EXECUTION/ASCENT/AS-006_CW_PIN.md)
    //
    // No link, no lift: as found, the rope's end is made fast on the bollard.
    // Take the trip handle and keep pulling past the lever's stop: the catch
    // lets go, the bollard holds the skip within its 0.03 m of slack, and the
    // cage never moves. Let go: the lever falls back, the catch seats the
    // skip again and takes the load off the rope, and the rope's end comes
    // off the bollard onto the cage as before -- the stage is not stranded.
    Simulation unlinked(InitialSpawn::Deck154);
    require(unlinked.advance_frame(1.0).accepted, "the stair-top settle interval must be accepted");
    const auto well_stair_top = unlinked.snapshot();
    require(well_stair_top.player_grounded && well_stair_top.player_position.y > 154.8 &&
                well_stair_top.player_position.y < 155.0,
            "Deck154 must stand the player on the 154 m deck");
    require(well_stair_top.well_a_catch_latched &&
                well_stair_top.well_a_rope_end_entity_id == Simulation::kWellAFrameEntityId,
            "as found, the skip must be in its catch and the rope made fast on the bollard");
    const double unlinked_skip_y0 = kit_y(unlinked, Simulation::kWellASkipEntityId);
    const double unlinked_cage_y0 = kit_y(unlinked, Simulation::kWellACageEntityId);
    require(board_well_a(unlinked), "the player must walk into Stage A's cage");
    require(walk_to(unlinked, -11.05, -130.35, 3.0, 0.08),
            "the player must reach the trip handle from inside the cage");
    (void)unlinked.set_facing(0.0, 1.0);
    (void)unlinked.advance_frame(0.5);
    require(unlinked.snapshot().carry_target_entity_id == Simulation::kWellAHandleEntityId &&
                unlinked.snapshot().carry_target_kind == 2,
            "the trip handle must be offered to a player facing it from the cage");
    (void)unlinked.request_pick_up();
    (void)unlinked.advance_frame(0.3);
    require(unlinked.snapshot().carrying_entity_id == Simulation::kWellAHandleEntityId,
            "the player must take the trip handle");
    double unlinked_cage_worst = 0.0;
    double unlinked_skip_drop = 0.0;
    double bollard_tension = 0.0;
    bool unlinked_opened = false;
    const auto note_unlinked = [&]() {
        const auto state = unlinked.snapshot();
        unlinked_opened = unlinked_opened || !state.well_a_catch_latched;
        unlinked_cage_worst =
            std::max(unlinked_cage_worst,
                     std::abs(kit_y(unlinked, Simulation::kWellACageEntityId) - unlinked_cage_y0));
        unlinked_skip_drop = std::max(
            unlinked_skip_drop, unlinked_skip_y0 - kit_y(unlinked, Simulation::kWellASkipEntityId));
        if (!state.well_a_catch_latched) {
            bollard_tension = std::max(bollard_tension, state.well_a_rope_tension_n);
        }
    };
    for (std::uint32_t tick = 0; tick < 90 * 3; ++tick) {
        (void)unlinked.set_move_input(0.0, -0.35);
        (void)unlinked.set_facing(0.0, 1.0);
        (void)unlinked.advance_frame(Simulation::kFixedStepSeconds);
        note_unlinked();
    }
    (void)unlinked.set_move_input(0.0, 0.0);
    if (unlinked.snapshot().carrying_entity_id != 0) {
        (void)unlinked.request_set_down();
    }
    for (std::uint32_t tick = 0; tick < 90 * 4; ++tick) {
        (void)unlinked.advance_frame(Simulation::kFixedStepSeconds);
        note_unlinked();
    }
    const auto after_unlinked = unlinked.snapshot();
    require(unlinked_opened, "pulling the trip handle must open the catch, linked or not");
    require(unlinked_cage_worst <= 0.05,
            "no link, no lift: with the rope on the bollard the cage must not move");
    require(unlinked_skip_drop <= 0.05,
            "the bollard must hold the skip within its slack when the catch opens");
    require(bollard_tension > 0.9 * kWellASkipMassKg * kGravity,
            "with the catch open, the bollard must carry the skip's weight through the rope");
    require(after_unlinked.well_a_catch_latched && after_unlinked.well_a_rope_tension_n < 60.0,
            "let go, the catch must seat the skip again and take the load off the rope");
    require(rig_well_a(unlinked),
            "after the wasted pull the rope's end must still come off the bollard onto the cage");
    std::cout << "PASS scraperx_sim AS-006 A no link: cage_worst=" << unlinked_cage_worst
              << " skip_drop=" << unlinked_skip_drop << " bollard_tension=" << bollard_tension
              << " relatched=" << int(after_unlinked.well_a_catch_latched) << " rerigged=1\n";

    // The ride: take the rope's end off the bollard, hook it on the cage's
    // eye, take the handle and step back. The skip falls 22 m and the cage
    // carries the rider 22 m under a governor that can only brake. The rider
    // stands on the cage the whole way; the floor stops flush with the 176
    // ring; at no tick has the payload gained more energy than the skip
    // released.
    Simulation well(InitialSpawn::Deck154);
    require(well.advance_frame(1.0).accepted, "the ride's settle interval must be accepted");
    require(board_well_a(well) && rig_well_a(well),
            "the player must rig Stage A from inside its cage");
    const double skip_y0 = kit_y(well, Simulation::kWellASkipEntityId);
    const double cage_y0 = kit_y(well, Simulation::kWellACageEntityId);
    const double rider_y0 = well.snapshot().player_position.y;
    require(pull_well_a(well, 0.5, 3.0), "stepping back with the handle must trip the catch");
    const double well_ride_start = well.snapshot().simulation_time_seconds;
    bool rode_on_cage = true;
    double well_ride_seconds = 0.0;
    double worst_energy_margin = std::numeric_limits<double>::infinity();
    for (std::uint32_t tick = 0; tick < 90 * 20; ++tick) {
        (void)well.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = well.snapshot();
        const double cage_rise = kit_y(well, Simulation::kWellACageEntityId) - cage_y0;
        if (cage_rise > 0.05 && cage_rise < kWellATravel - 0.05) {
            rode_on_cage = rode_on_cage && state.player_grounded &&
                           state.support_entity_id == Simulation::kWellACageEntityId;
        }
        const double released =
            kWellASkipMassKg * kGravity * (skip_y0 - kit_y(well, Simulation::kWellASkipEntityId));
        const double gained = kWellACageMassKg * kGravity * cage_rise +
                              kRiderMassKg * kGravity * (state.player_position.y - rider_y0);
        worst_energy_margin = std::min(worst_energy_margin, released - gained);
        if (well_ride_seconds == 0.0 && state.well_a_cage_travel >= kWellATravel - 0.01) {
            well_ride_seconds = state.simulation_time_seconds - well_ride_start;
        }
    }
    const auto at_top = well.snapshot();
    const double floor_y = kit_y(well, Simulation::kWellACageEntityId) + 0.10;
    const double skip_released =
        kWellASkipMassKg * kGravity * (skip_y0 - kit_y(well, Simulation::kWellASkipEntityId));
    const double payload_gained =
        kWellACageMassKg * kGravity * (kit_y(well, Simulation::kWellACageEntityId) - cage_y0) +
        kRiderMassKg * kGravity * (at_top.player_position.y - rider_y0);
    require(rode_on_cage, "the rider must stand on the cage for the whole ride");
    require(std::abs(floor_y - (kWellACageFloorTop + kWellATravel)) <= 0.05,
            "the cage's floor must stop flush with the 176 ring at 176.25 m");
    require(at_top.well_a_cage_peak_speed <= 2.6, "the governor must hold the cage to 2.5 m/s");
    require(at_top.player_grounded && at_top.support_entity_id == Simulation::kWellACageEntityId &&
                at_top.player_position.y > 177.0,
            "the rider must arrive standing in the cage at the top");
    require(worst_energy_margin >= -kStanceJitterJ,
            "at no tick may the payload have gained more energy than the skip released");

    // Dying restores the last commit, machines included: from the parked
    // cage walk east, across Stage B's cage waiting flush beside it, and off
    // B's open east side into the well. The fall is lethal, and the restore
    // returns the rider to the last firm footing, on the two cages' floor at
    // 176.25, with Stage A as it was committed: the cage at the top, the rope
    // on its eye.
    const auto well_deaths = at_top.death_count;
    for (std::uint32_t tick = 0; tick < 90 * 8 && well.snapshot().death_count == well_deaths;
         ++tick) {
        (void)well.set_move_input(well.snapshot().player_grounded ? 0.6 : 0.0, 0.0);
        (void)well.set_facing(1.0, 0.0);
        (void)well.advance_frame(Simulation::kFixedStepSeconds);
    }
    require(well.snapshot().death_count == well_deaths + 1,
            "walking off the parked cage must be a lethal fall");
    (void)well.set_move_input(0.0, 0.0);
    require(well.advance_frame(1.0).accepted, "the restore settle interval must be accepted");
    const auto well_restored = well.snapshot();
    require(well_restored.player_position.y > 177.0 &&
                (well_restored.support_entity_id == Simulation::kWellACageEntityId ||
                 well_restored.support_entity_id == Simulation::kWellBCageEntityId) &&
                std::abs(well_restored.well_a_cage_travel - kWellATravel) <= 0.05 &&
                well_restored.well_a_rope_end_entity_id == Simulation::kWellACageEntityId,
            "the restore must return the rider to the parked cages' floor with Stage A as committed");
    std::cout << "PASS scraperx_sim AS-006 A ride: ride_s=" << well_ride_seconds
              << " floor_y=" << floor_y << " peak_speed=" << at_top.well_a_cage_peak_speed
              << " rode_on_cage=" << int(rode_on_cage) << " skip_released_J=" << skip_released
              << " payload_gained_J=" << payload_gained
              << " energy_margin_J=" << worst_energy_margin
              << " restored_y=" << well_restored.player_position.y << '\n';

    // ---- AS-006 Stage B, the derrick boom ------------------------------------
    //
    // No link, no lift: as found, the hoist line's end is made fast on the
    // cleat in front of B's cage. Take the trip handle and keep pulling past
    // the lever's stop: the catch lets go of the 2.5 t boom, the cleat holds
    // it through the line within its 0.03 m of slack, and the cage never
    // moves. Let go: the catch seats the boom again and takes the load off
    // the line, and the line's end still comes off the cleat onto the cage.
    Simulation b_unlinked(InitialSpawn::WellBCage);
    require(b_unlinked.advance_frame(1.0).accepted, "the Stage B settle interval must be accepted");
    const auto b_found = b_unlinked.snapshot();
    require(b_found.player_grounded &&
                b_found.support_entity_id == Simulation::kWellBCageEntityId &&
                b_found.well_b_catch_latched &&
                b_found.well_b_rope_end_entity_id == Simulation::kWellBFrameEntityId,
            "as found, the rider stands in B's cage, the boom is in its catch and the line is "
            "made fast on the cleat");
    const double b_boom_y0 = kit_com_y(b_unlinked, Simulation::kWellBBoomEntityId);
    const double b_cage_y0 = kit_y(b_unlinked, Simulation::kWellBCageEntityId);
    // Whatever line holds the boom level has a moment arm no longer than
    // the boom: at least the boom's weight moment over its length.
    const double b_pivot_z =
        b_unlinked.kit_body_position(b_unlinked.kit_body_index(Simulation::kWellBBoomEntityId)).z;
    const double b_line_floor =
        kWellBBoomMassKg * kGravity *
        std::abs(kit_com_z(b_unlinked, Simulation::kWellBBoomEntityId) - b_pivot_z) /
        kWellBBoomLength;
    require(walk_to(b_unlinked, -8.0, -130.25, 3.0, 0.08),
            "the player must reach B's trip handle from inside the cage");
    (void)b_unlinked.set_facing(0.0, 1.0);
    (void)b_unlinked.advance_frame(0.5);
    require(b_unlinked.snapshot().carry_target_entity_id == Simulation::kWellBHandleEntityId &&
                b_unlinked.snapshot().carry_target_kind == 2,
            "B's trip handle must be offered to a player facing it from the cage");
    (void)b_unlinked.request_pick_up();
    (void)b_unlinked.advance_frame(0.3);
    require(b_unlinked.snapshot().carrying_entity_id == Simulation::kWellBHandleEntityId,
            "the player must take B's trip handle");
    double b_cage_worst = 0.0;
    double b_boom_drop = 0.0;
    double b_cleat_tension = 0.0;
    bool b_opened = false;
    const auto note_b_unlinked = [&]() {
        const auto state = b_unlinked.snapshot();
        b_opened = b_opened || !state.well_b_catch_latched;
        b_cage_worst =
            std::max(b_cage_worst,
                     std::abs(kit_y(b_unlinked, Simulation::kWellBCageEntityId) - b_cage_y0));
        b_boom_drop = std::max(
            b_boom_drop, b_boom_y0 - kit_com_y(b_unlinked, Simulation::kWellBBoomEntityId));
        if (!state.well_b_catch_latched) {
            b_cleat_tension = std::max(b_cleat_tension, state.well_b_rope_tension_n);
        }
    };
    for (std::uint32_t tick = 0; tick < 90 * 3; ++tick) {
        (void)b_unlinked.set_move_input(0.0, -0.35);
        (void)b_unlinked.set_facing(0.0, 1.0);
        (void)b_unlinked.advance_frame(Simulation::kFixedStepSeconds);
        note_b_unlinked();
    }
    (void)b_unlinked.set_move_input(0.0, 0.0);
    if (b_unlinked.snapshot().carrying_entity_id != 0) {
        (void)b_unlinked.request_set_down();
    }
    for (std::uint32_t tick = 0; tick < 90 * 4; ++tick) {
        (void)b_unlinked.advance_frame(Simulation::kFixedStepSeconds);
        note_b_unlinked();
    }
    const auto after_b_unlinked = b_unlinked.snapshot();
    require(b_opened, "pulling B's trip handle must open the boom's catch, linked or not");
    require(b_cage_worst <= 0.05,
            "no link, no lift: with the line on the cleat B's cage must not move");
    require(b_boom_drop <= 0.05,
            "the cleat must hold the boom through the line within its slack when the catch opens");
    require(b_cleat_tension >= b_line_floor,
            "with the catch open, the line must carry the boom's weight to the cleat");
    require(after_b_unlinked.well_b_catch_latched && after_b_unlinked.well_b_rope_tension_n < 60.0,
            "let go, the catch must seat the boom again and take the load off the line");
    require(rig_well_b(b_unlinked),
            "after the wasted pull the line's end must still come off the cleat onto the cage");
    std::cout << "PASS scraperx_sim AS-006 B no link: cage_worst=" << b_cage_worst
              << " boom_drop=" << b_boom_drop << " cleat_tension=" << b_cleat_tension
              << " line_floor=" << b_line_floor
              << " relatched=" << int(after_b_unlinked.well_b_catch_latched) << " rerigged=1\n";

    // The ride: the line's end off the cleat onto the cage's eye, the handle
    // pulled. The boom swings down about its pivot on the 220 ring, its block
    // draws away from S2 under the 242 ring, and the two-part purchase takes
    // the cage 22 m under a brake-only governor. At the top the cage's rail
    // lifts the striker and the slip hook lets the line run: the boom hangs
    // from its pivot, spent, and the cage stays up on its dogs. At no tick
    // has the payload gained more energy than the boom released.
    Simulation b_well(InitialSpawn::WellBCage);
    require(b_well.advance_frame(1.0).accepted, "B's ride settle interval must be accepted");
    require(rig_well_b(b_well), "the player must rig Stage B from inside its cage");
    const double b_ride_boom_y0 = kit_com_y(b_well, Simulation::kWellBBoomEntityId);
    const double b_ride_cage_y0 = kit_y(b_well, Simulation::kWellBCageEntityId);
    const double b_rider_y0 = b_well.snapshot().player_position.y;
    require(pull_well_b(b_well), "stepping back with B's handle must trip the boom's catch");
    const double b_ride_start = b_well.snapshot().simulation_time_seconds;
    bool b_rode_on_cage = true;
    double b_ride_seconds = 0.0;
    double b_worst_margin = std::numeric_limits<double>::infinity();
    for (std::uint32_t tick = 0; tick < 90 * 20; ++tick) {
        (void)b_well.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = b_well.snapshot();
        const double rise = kit_y(b_well, Simulation::kWellBCageEntityId) - b_ride_cage_y0;
        // While the line hauls: once it runs, the cage drops the last few
        // centimetres onto its dogs under the rider.
        if (rise > 0.05 && !state.well_b_rope_let_go) {
            b_rode_on_cage = b_rode_on_cage && state.player_grounded &&
                             state.support_entity_id == Simulation::kWellBCageEntityId;
        }
        const double released = kWellBBoomMassKg * kGravity *
                                (b_ride_boom_y0 - kit_com_y(b_well, Simulation::kWellBBoomEntityId));
        const double gained = kWellACageMassKg * kGravity * rise +
                               kRiderMassKg * kGravity * (state.player_position.y - b_rider_y0);
        b_worst_margin = std::min(b_worst_margin, released - gained);
        if (b_ride_seconds == 0.0 && state.well_b_cage_travel >= kWellATravel - kWellDogPitch) {
            b_ride_seconds = state.simulation_time_seconds - b_ride_start;
        }
    }
    const auto b_top = b_well.snapshot();
    const double b_floor_y = kit_y(b_well, Simulation::kWellBCageEntityId) + 0.10;
    const double b_top_floor = kWellBCageFloorTop + kWellATravel;
    require(b_rode_on_cage, "the rider must stand on B's cage for the whole ride");
    require(b_top.well_b_rope_let_go,
            "at the top the striker must open the slip hook and let the line run");
    require(b_top.well_b_boom_angle > 1.45,
            "let go, the spent boom must swing on to hang from its pivot");
    require(b_floor_y <= b_top_floor + 0.02 && b_floor_y >= b_top_floor - kWellDogPitch - 0.02,
            "with the line gone, B's cage must stand on its dogs flush with the 198 ring");
    require(b_top.well_b_cage_peak_speed <= 3.1, "the governor must hold B's cage to 3 m/s");
    require(b_worst_margin >= -kStanceJitterJ,
            "at no tick may B's payload have gained more energy than the boom released");
    require(b_top.player_grounded && b_top.support_entity_id == Simulation::kWellBCageEntityId &&
                b_top.player_position.y > 199.0,
            "the rider must arrive standing in B's cage at the top");
    require(walk_to(b_well, -7.2, -128.8, 4.0), "from B's parked cage the rider must step onto the 198 ring");
    (void)b_well.advance_frame(0.5);
    const auto on_198 = b_well.snapshot();
    require(on_198.player_grounded && on_198.player_position.y > 199.0 &&
                on_198.player_position.y < 199.3 && on_198.player_position.z > -129.82 &&
                on_198.support_entity_id != Simulation::kWellBCageEntityId,
            "the rider must stand on the 198 ring");
    // Dying restores the last commit with the spent stage as committed: the
    // line run out, the boom hanging, the cage on its dogs.
    const auto b_deaths = on_198.death_count;
    require(walk_to(b_well, 2.5, -128.8, 6.0), "the rider must walk the 198 ring east of the boom");
    for (std::uint32_t tick = 0; tick < 90 * 10 && b_well.snapshot().death_count == b_deaths;
         ++tick) {
        (void)b_well.set_move_input(0.0, b_well.snapshot().player_grounded ? -0.6 : 0.0);
        (void)b_well.set_facing(0.0, -1.0);
        (void)b_well.advance_frame(Simulation::kFixedStepSeconds);
    }
    require(b_well.snapshot().death_count == b_deaths + 1,
            "walking off the 198 ring into the well must be a lethal fall");
    (void)b_well.set_move_input(0.0, 0.0);
    require(b_well.advance_frame(1.0).accepted, "B's restore settle interval must be accepted");
    const auto b_restored = b_well.snapshot();
    require(b_restored.player_grounded && b_restored.player_position.y > 199.0 &&
                b_restored.player_position.y < 199.3 && b_restored.well_b_rope_let_go &&
                b_restored.well_b_boom_angle > 1.45 &&
                b_restored.well_b_cage_travel >= kWellATravel - kWellDogPitch - 0.01,
            "the restore must return the rider to the 198 ring with Stage B spent as committed");
    std::cout << "PASS scraperx_sim AS-006 B ride: ride_s=" << b_ride_seconds
              << " floor_y=" << b_floor_y << " peak_speed=" << b_top.well_b_cage_peak_speed
              << " boom_angle=" << b_top.well_b_boom_angle
              << " let_go=" << int(b_top.well_b_rope_let_go)
              << " rode_on_cage=" << int(b_rode_on_cage) << " energy_margin_J=" << b_worst_margin
              << " restored_y=" << b_restored.player_position.y << '\n';

    // B's wreckage: along the 198 ring and out on the board beside the spent
    // boom, a hold on its lower end, the climb, and a look round at the 220
    // ring to top out onto it.
    require(walk_to(b_well, 1.0, -128.6, 14.0) && walk_to(b_well, 1.0, -131.8, 6.0, 0.08),
            "B wreck: along the 198 ring and out on the board beside the hanging boom");
    (void)b_well.set_facing(-1.0, 0.0);
    (void)b_well.advance_frame(0.4);
    require(b_well.snapshot().grip_available, "B wreck: the hanging boom's lower end in reach from the board");
    (void)b_well.request_traversal();
    (void)b_well.advance_frame(0.2);
    require(is_climbing(b_well.snapshot()) &&
                b_well.snapshot().traversal_support_entity_id == Simulation::kWellBBoomEntityId,
            "B wreck: a hold on the hanging boom");
    require(hold_stick(b_well, -1.0, 0.0, -1.0, 0.0, 40.0,
                       [](const Snapshot &state) { return state.player_position.y >= 219.8; }),
            "B wreck: up the boom to the 220 ring's edge");
    require(hold_stick(b_well, -1.0, 0.0, 0.0, 1.0, 4.0,
                       [](const Snapshot &state) { return standing_above(state, 220.2); }),
            "B wreck: over the 220 ring's edge from the boom");
    std::cout << "PASS scraperx_sim AS-006 wreckage B: ring_y=" << b_well.snapshot().player_position.y << '\n';

    // ---- AS-006 Stage C, the debris chute, the band's finale ------------------
    //
    // No link, no lift, both ways round. With the chute jammed the dumpster
    // is empty and lighter than the platform: pull the keeper latch's handle
    // and hold it, and nothing moves; let go, the latch seats itself again.
    // Throw the rebar clear and the dumpster fills with 900 kg of rubble,
    // but with the latch shut the full dumpster just hangs.
    Simulation c_unlinked(InitialSpawn::WellCPlatform);
    require(c_unlinked.advance_frame(1.0).accepted, "the Stage C settle interval must be accepted");
    const auto c_found = c_unlinked.snapshot();
    require(c_found.player_grounded &&
                c_found.support_entity_id == Simulation::kWellCPlatformEntityId &&
                c_found.well_c_catch_latched && c_found.well_c_hopper_kg >= kWellRubbleKg - 1.0 &&
                c_found.well_c_dumpster_kg <= 0.0,
            "as found, the rider stands on C's platform, latched, with the rubble in the hopper");
    const double c_platform_y0 = kit_y(c_unlinked, Simulation::kWellCPlatformEntityId);
    const double c_dumpster_y0 = kit_y(c_unlinked, Simulation::kWellCDumpsterEntityId);
    double c_platform_worst = 0.0;
    double c_dumpster_worst = 0.0;
    std::uint32_t c_open_ticks = 0;
    const auto note_c_unlinked = [&]() {
        const auto state = c_unlinked.snapshot();
        c_open_ticks += state.well_c_catch_latched ? 0U : 1U;
        c_platform_worst = std::max(
            c_platform_worst,
            std::abs(kit_y(c_unlinked, Simulation::kWellCPlatformEntityId) - c_platform_y0));
        c_dumpster_worst = std::max(
            c_dumpster_worst,
            std::abs(kit_y(c_unlinked, Simulation::kWellCDumpsterEntityId) - c_dumpster_y0));
    };
    // The latch pulled open and held there, the dumpster empty.
    require(walk_to(c_unlinked, -3.0, -131.6, 3.0, 0.08),
            "the player must reach the latch's handle from the platform");
    (void)c_unlinked.set_facing(0.0, 1.0);
    (void)c_unlinked.advance_frame(0.5);
    require(c_unlinked.snapshot().carry_target_entity_id ==
                    Simulation::kWellCLatchHandleEntityId &&
                c_unlinked.snapshot().carry_target_kind == 2,
            "the latch's handle must be offered to a player facing it from the platform");
    (void)c_unlinked.request_pick_up();
    (void)c_unlinked.advance_frame(0.3);
    require(c_unlinked.snapshot().carrying_entity_id == Simulation::kWellCLatchHandleEntityId,
            "the player must take the latch's handle");
    for (std::uint32_t tick = 0; tick < 90 * 4; ++tick) {
        const bool open = !c_unlinked.snapshot().well_c_catch_latched;
        (void)c_unlinked.set_move_input(0.0, open ? 0.0 : -0.35);
        (void)c_unlinked.set_facing(0.0, 1.0);
        (void)c_unlinked.advance_frame(Simulation::kFixedStepSeconds);
        note_c_unlinked();
    }
    if (c_unlinked.snapshot().carrying_entity_id != 0) {
        (void)c_unlinked.request_set_down();
    }
    require(wait_for(c_unlinked, 3.0,
                     [](const Snapshot &state) { return state.well_c_catch_latched; }),
            "let go, the latch must seat itself on the unmoved platform again");
    const std::uint32_t c_latch_open_ticks = c_open_ticks;
    // The rebar thrown clear, the latch shut.
    require(clear_well_c_chute(c_unlinked),
            "a full pull on the rebar's handle must throw it over upright, clear of the mouth");
    (void)c_unlinked.advance_frame(1.0);
    require(c_unlinked.snapshot().well_c_rebar_angle > 1.7,
            "thrown over upright, the rebar must lie back on its far stop, the mouth clear");
    // Mid-pour, the hopper's stream runs from its mouth down to the rubble's
    // floor in the dumpster, past the dumpster's bail.
    require(wait_for(c_unlinked, 3.0,
                     [](const Snapshot &state) { return state.well_c_dumpster_kg > 100.0; }),
            "the rubble must start pouring into the dumpster");
    const std::uint32_t dumpster_body = c_unlinked.kit_body_index(Simulation::kWellCDumpsterEntityId);
    const auto dumpster_floor = c_unlinked.kit_body_part(dumpster_body, 0);
    const double dumpster_floor_top = kit_y(c_unlinked, Simulation::kWellCDumpsterEntityId) +
                                      dumpster_floor.offset.y + dumpster_floor.half.y;
    bool c_stream_seen = false;
    for (std::uint32_t bin = 0; bin < c_unlinked.kit_bin_count(); ++bin) {
        const auto stream = c_unlinked.kit_bin(bin);
        if (stream.flowing) {
            c_stream_seen = true;
            require(std::abs(stream.stream_to.y - dumpster_floor_top) <= 0.02 &&
                        stream.stream_from.y > dumpster_floor_top + 1.5,
                    "the hopper's stream must fall from its mouth to the dumpster's floor");
        }
    }
    require(c_stream_seen, "mid-pour, one bin must be pouring");
    require(fill_well_c(c_unlinked, 10.0), "the rubble must pour into the dumpster");
    for (std::uint32_t tick = 0; tick < 90 * 3; ++tick) {
        (void)c_unlinked.advance_frame(Simulation::kFixedStepSeconds);
        note_c_unlinked();
    }
    const auto c_full = c_unlinked.snapshot();
    require(c_latch_open_ticks >= Simulation::kTickRateHz,
            "the latch must have been held open for at least a second");
    require(c_platform_worst <= 0.05 && c_dumpster_worst <= 0.05,
            "no link, no lift: C's platform must not move with the latch open and the dumpster "
            "empty, nor with the dumpster full and the latch shut");
    require(c_full.well_c_catch_latched && c_full.well_c_hopper_kg <= 1.0 &&
                c_full.well_c_dumpster_kg >= kWellRubbleKg - 1.0,
            "with the latch shut, the full dumpster must just hang");
    std::cout << "PASS scraperx_sim AS-006 C no link: platform_worst=" << c_platform_worst
              << " dumpster_worst=" << c_dumpster_worst << " latch_open_s="
              << static_cast<double>(c_latch_open_ticks) / Simulation::kTickRateHz
              << " dumpster_kg=" << c_full.well_c_dumpster_kg
              << " rebar=" << c_full.well_c_rebar_angle << '\n';

    // The ride: throw the rebar, let the dumpster fill, pull the latch. The
    // full dumpster (1 150 kg) outweighs platform and rider (785 kg) and
    // sinks 22 m, lifting them 22 m under a brake-only governor; at no tick
    // has the payload gained more energy than the dumpster and its rubble
    // released. At the foot of its guide the dumpster presses the striker
    // that opens its gate and empties it -- here, with A's cage as found at
    // the foot of its well, into A's cage. Emptied, the dumpster no longer
    // holds the platform up: the platform's dogs do.
    Simulation c_well(InitialSpawn::WellCPlatform);
    require(c_well.advance_frame(1.0).accepted, "C's ride settle interval must be accepted");
    require(clear_well_c_chute(c_well) && fill_well_c(c_well, 10.0),
            "the rider must clear the chute and let the dumpster fill");
    const double c_ride_platform_y0 = kit_y(c_well, Simulation::kWellCPlatformEntityId);
    const double c_rider_y0 = c_well.snapshot().player_position.y;
    double c_dumpster_prev = kit_y(c_well, Simulation::kWellCDumpsterEntityId);
    require(pull_well_c_latch(c_well, 3.0), "stepping back with the latch's handle must free the platform");
    const double c_ride_start = c_well.snapshot().simulation_time_seconds;
    double c_released = 0.0;
    double c_worst_margin = std::numeric_limits<double>::infinity();
    bool c_rode_on_platform = true;
    double c_ride_seconds = 0.0;
    for (std::uint32_t tick = 0; tick < 90 * 25; ++tick) {
        (void)c_well.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = c_well.snapshot();
        const double rise = kit_y(c_well, Simulation::kWellCPlatformEntityId) - c_ride_platform_y0;
        if (rise > 0.05 && rise < kWellATravel - kWellDogPitch) {
            c_rode_on_platform = c_rode_on_platform && state.player_grounded &&
                                 state.support_entity_id == Simulation::kWellCPlatformEntityId;
        }
        const double dumpster_y = kit_y(c_well, Simulation::kWellCDumpsterEntityId);
        c_released += (kWellCDumpsterMassKg + state.well_c_dumpster_kg) * kGravity *
                      (c_dumpster_prev - dumpster_y);
        c_dumpster_prev = dumpster_y;
        const double gained = kWellCPlatformMassKg * kGravity * rise +
                              kRiderMassKg * kGravity * (state.player_position.y - c_rider_y0);
        c_worst_margin = std::min(c_worst_margin, c_released - gained);
        if (c_ride_seconds == 0.0 && state.well_c_platform_travel >= kWellATravel - kWellDogPitch) {
            c_ride_seconds = state.simulation_time_seconds - c_ride_start;
        }
    }
    const auto c_top = c_well.snapshot();
    const double c_floor_y = kit_y(c_well, Simulation::kWellCPlatformEntityId) + 0.10;
    const double c_top_floor = kWellCPlatformFloorTop + kWellATravel;
    require(c_rode_on_platform, "the rider must stand on C's platform for the whole ride");
    require(c_top.well_c_dumpster_travel <= -kWellATravel + 0.01 &&
                c_top.well_c_dumpster_kg <= 1.0 &&
                c_top.well_a_cage_rubble_kg >= kWellRubbleKg - 1.0,
            "at the foot of its guide the dumpster's gate must open and empty it into A's cage");
    require(c_floor_y <= c_top_floor + 0.02 && c_floor_y >= c_top_floor - kWellDogPitch - 0.02,
            "emptied, the dumpster no longer holds the platform: it must stand on its dogs flush "
            "with the 220 ring");
    require(c_top.well_c_platform_peak_speed <= 3.1, "the governor must hold C's platform to 3 m/s");
    require(c_worst_margin >= -kStanceJitterJ,
            "at no tick may C's payload have gained more energy than the dumpster released");
    require(c_top.player_grounded &&
                c_top.support_entity_id == Simulation::kWellCPlatformEntityId &&
                c_top.player_position.y > 221.0,
            "the rider must arrive standing on C's platform at the top");
    require(walk_to(c_well, -3.0, -129.0, 4.0), "from C's platform the rider must step onto the 220 ring");
    (void)c_well.advance_frame(0.5);
    const auto on_220 = c_well.snapshot();
    require(on_220.player_grounded && on_220.player_position.y > 221.0 &&
                on_220.player_position.y < 221.3 && on_220.player_position.z > -130.73 &&
                on_220.support_entity_id != Simulation::kWellCPlatformEntityId,
            "the rider must stand on the 220 ring");
    // Dying restores the last commit with the spent stage as committed: the
    // platform on its dogs, the dumpster down and empty, the rubble in A's
    // cage and none left in the hopper.
    const auto c_deaths = on_220.death_count;
    require(walk_to(c_well, 2.5, -129.2, 8.0), "the rider must walk the 220 ring east of the boom");
    for (std::uint32_t tick = 0; tick < 90 * 10 && c_well.snapshot().death_count == c_deaths;
         ++tick) {
        (void)c_well.set_move_input(0.0, c_well.snapshot().player_grounded ? -0.6 : 0.0);
        (void)c_well.set_facing(0.0, -1.0);
        (void)c_well.advance_frame(Simulation::kFixedStepSeconds);
    }
    require(c_well.snapshot().death_count == c_deaths + 1,
            "walking off the 220 ring into the well must be a lethal fall");
    (void)c_well.set_move_input(0.0, 0.0);
    require(c_well.advance_frame(1.0).accepted, "C's restore settle interval must be accepted");
    const auto c_restored = c_well.snapshot();
    require(c_restored.player_grounded && c_restored.player_position.y > 221.0 &&
                c_restored.player_position.y < 221.3 &&
                c_restored.well_c_platform_travel >= kWellATravel - kWellDogPitch - 0.01 &&
                c_restored.well_c_dumpster_travel <= -kWellATravel + 0.01 &&
                c_restored.well_c_dumpster_kg <= 1.0 && c_restored.well_c_hopper_kg <= 1.0 &&
                c_restored.well_a_cage_rubble_kg >= kWellRubbleKg - 1.0,
            "the restore must return the rider to the 220 ring with Stage C spent as committed");
    std::cout << "PASS scraperx_sim AS-006 C ride: ride_s=" << c_ride_seconds
              << " floor_y=" << c_floor_y << " peak_speed=" << c_top.well_c_platform_peak_speed
              << " released_J=" << c_released << " energy_margin_J=" << c_worst_margin
              << " a_cage_rubble_kg=" << c_top.well_a_cage_rubble_kg
              << " rode_on_platform=" << int(c_rode_on_platform)
              << " restored_y=" << c_restored.player_position.y << '\n';

    // ---- AS-006, the band in one run -----------------------------------------
    //
    // From the 154 m deck to standing on the 220 ring on player
    // inputs alone: rig and ride A; step across into B's cage waiting flush
    // beside A's; rig and ride B; step across onto C's platform; throw the
    // rebar, let the dumpster fill, pull the latch, ride C; step off onto the
    // 220 ring. Then the cascade: C's spent dumpster empties into A's parked
    // cage, which sinks and hauls A's skip back up into its catch -- C's
    // source re-arms A. Down again by parachute, the rider holds A's tip-out
    // handle down to dump the rubble on the deck and rides the re-armed A up.
    Simulation band(InitialSpawn::Deck154);
    require(band.advance_frame(1.0).accepted, "the band's settle interval must be accepted");
    const double band_start = band.snapshot().simulation_time_seconds;
    require(board_well_a(band) && rig_well_a(band) && pull_well_a(band, 0.5, 3.0),
            "the rider must board, rig and trip Stage A");
    require(wait_for(band, 16.0,
                     [](const Snapshot &state) {
                         return state.well_a_cage_travel >= kWellATravel - 0.01;
                     }),
            "A must carry the rider to the 176 ring");
    (void)band.advance_frame(1.0);
    require(walk_to(band, -9.4, -131.2, 4.0) && walk_to(band, -7.6, -131.2, 4.0),
            "from A's parked cage the rider must step across into B's");
    require(rig_well_b(band) && pull_well_b(band), "the rider must rig and trip Stage B");
    require(wait_for(band, 16.0,
                     [](const Snapshot &state) {
                         return state.well_b_cage_travel >= kWellATravel - kWellDogPitch - 0.01;
                     }),
            "B must carry the rider to the 198 ring");
    (void)band.advance_frame(3.0);
    require(band.snapshot().player_grounded &&
                band.snapshot().support_entity_id == Simulation::kWellBCageEntityId,
            "the rider must stand in B's parked cage");
    require(walk_to(band, -6.2, -131.9, 4.0) && walk_to(band, -4.4, -131.9, 4.0),
            "from B's parked cage the rider must step across onto C's platform");
    require(clear_well_c_chute(band) && fill_well_c(band, 10.0) && pull_well_c_latch(band, 3.0),
            "the rider must clear C's chute, let the dumpster fill and pull the latch");
    require(wait_for(band, 16.0,
                     [](const Snapshot &state) {
                         return state.well_c_platform_travel >= kWellATravel - kWellDogPitch - 0.01;
                     }),
            "C must carry the rider to the 220 ring");
    (void)band.advance_frame(2.0);
    require(walk_to(band, -3.0, -129.0, 4.0), "from C's platform the rider must step onto the 220 ring");
    (void)band.advance_frame(1.0);
    const auto band_220 = band.snapshot();
    require(band_220.player_grounded && band_220.player_position.y > 221.0 &&
                band_220.player_position.y < 221.3 && band_220.player_position.z > -130.73 &&
                band_220.support_entity_id != Simulation::kWellCPlatformEntityId,
            "one run from the 154 m deck must end standing on the 220 ring");
    const double band_seconds = band_220.simulation_time_seconds - band_start;
    require(wait_for(band, 25.0,
                     [](const Snapshot &state) {
                         return state.well_a_catch_latched && state.well_a_cage_travel <= 0.01;
                     }),
            "C's spent dumpster must empty into A's parked cage, which must sink and haul A's "
            "skip back up into its catch");
    const auto rearmed = band.snapshot();
    require(rearmed.well_a_cage_rubble_kg >= kWellRubbleKg - 1.0 &&
                rearmed.well_c_dumpster_kg <= 1.0 && rearmed.well_a_skip_travel >= -0.01 &&
                rearmed.well_a_rope_end_entity_id == Simulation::kWellACageEntityId,
            "re-armed, A's skip must be up in its catch, the rubble in its cage, the rope on its eye");
    // Down by parachute, east of the boom, onto the deck.
    require(walk_to(band, 2.5, -129.6, 8.0), "the rider must walk the 220 ring east of the boom");
    const auto band_deaths = band.snapshot().death_count;
    bool chute = false;
    bool landed = false;
    for (std::uint32_t tick = 0; tick < 90 * 40 && !landed; ++tick) {
        const auto state = band.snapshot();
        if (!chute && !state.player_grounded) {
            (void)band.request_parachute();
            chute = true;
        }
        landed = chute && state.player_grounded && state.player_position.y < 160.0;
        (void)band.set_move_input(0.0, chute ? 0.0 : -0.6);
        (void)band.set_facing(0.0, -1.0);
        (void)band.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)band.set_move_input(0.0, 0.0);
    (void)band.advance_frame(0.5);
    const auto band_landed = band.snapshot();
    require(landed && band_landed.death_count == band_deaths && band_landed.player_grounded &&
                band_landed.player_position.y > 154.8 && band_landed.player_position.y < 155.0,
            "a parachute must set the rider down alive on the 154 m deck");
    // Back into A's cage: take the tip-out handle, pull it down until the
    // rubble runs, and hold it there until the cage is empty.
    require(walk_to(band, 2.6, -128.2, 6.0) && walk_to(band, -10.0, -128.2, 14.0) &&
                board_well_a(band) && walk_to(band, -9.6, -130.35, 4.0, 0.08),
            "the rider must walk back into A's cage to the tip-out handle");
    (void)band.set_facing(0.0, 1.0);
    (void)band.advance_frame(0.5);
    require(band.snapshot().carry_target_entity_id == Simulation::kWellATipHandleEntityId &&
                band.snapshot().carry_target_kind == 2,
            "A's tip-out handle must be offered to a player facing it from the cage");
    (void)band.request_pick_up();
    (void)band.advance_frame(0.3);
    require(band.snapshot().carrying_entity_id == Simulation::kWellATipHandleEntityId,
            "the player must take A's tip-out handle");
    bool running = false;
    for (std::uint32_t tick = 0; tick < 90 * 6 && band.snapshot().well_a_cage_rubble_kg > 0.0;
         ++tick) {
        running = running || band.snapshot().well_a_cage_rubble_kg < kWellRubbleKg - 1.0;
        (void)band.set_move_input(0.0, running ? 0.0 : -0.35);
        (void)band.set_facing(0.0, 1.0);
        (void)band.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)band.set_move_input(0.0, 0.0);
    if (band.snapshot().carrying_entity_id != 0) {
        (void)band.request_set_down();
    }
    (void)band.advance_frame(1.0);
    const auto tipped = band.snapshot();
    require(tipped.well_a_cage_rubble_kg <= 0.0 &&
                tipped.well_rubble_spilled_kg >= kWellRubbleKg - 1.0,
            "held down, the tip-out must dump all of A's rubble onto the deck");
    require(tipped.well_a_cage_travel <= 0.01 && tipped.well_a_catch_latched &&
                tipped.well_a_rope_end_entity_id == Simulation::kWellACageEntityId,
            "emptied, A must stand re-armed: the skip up in its catch, the rope on the cage's eye");
    const double a_cage_x = band.kit_body_position(band.kit_body_index(Simulation::kWellACageEntityId)).x;
    const double a_cage_z = band.kit_body_position(band.kit_body_index(Simulation::kWellACageEntityId)).z;
    require(band.kit_pile_count() == 1 &&
                std::abs(band.kit_pile(0).kg - tipped.well_rubble_spilled_kg) <= 0.5 &&
                std::abs(band.kit_pile(0).at.y - 154.0) <= 0.05 &&
                std::hypot(band.kit_pile(0).at.x - a_cage_x, band.kit_pile(0).at.z - a_cage_z) <= 0.1,
            "the tip-out's rubble must lie in one pile on the deck under A's cage");
    require(pull_well_a(band, 0.5, 3.0) &&
                wait_for(band, 16.0,
                         [](const Snapshot &state) {
                             return state.well_a_cage_travel >= kWellATravel - 0.01;
                         }),
            "the re-armed A must carry the rider up again");
    (void)band.advance_frame(1.0);
    const auto band_again = band.snapshot();
    require(band_again.player_grounded &&
                band_again.support_entity_id == Simulation::kWellACageEntityId &&
                band_again.player_position.y > 177.0,
            "the rider must arrive standing in A's cage at the 176 ring again");
    // Dying now restores the band as committed, the spilled pile with it: off
    // the parked cage's open east side, B's cage no longer waits beside it.
    const auto again_deaths = band_again.death_count;
    for (std::uint32_t tick = 0; tick < 90 * 8 && band.snapshot().death_count == again_deaths;
         ++tick) {
        (void)band.set_move_input(band.snapshot().player_grounded ? 0.6 : 0.0, 0.0);
        (void)band.set_facing(1.0, 0.0);
        (void)band.advance_frame(Simulation::kFixedStepSeconds);
    }
    require(band.snapshot().death_count == again_deaths + 1,
            "with B's cage gone up, walking east off A's parked cage must be a lethal fall");
    (void)band.set_move_input(0.0, 0.0);
    require(band.advance_frame(1.0).accepted, "the band's restore settle interval must be accepted");
    const auto band_restored = band.snapshot();
    require(band_restored.player_grounded &&
                band_restored.support_entity_id == Simulation::kWellACageEntityId &&
                band_restored.well_a_cage_travel >= kWellATravel - 0.01 &&
                band_restored.well_a_cage_rubble_kg <= 0.0 && band.kit_pile_count() == 1 &&
                std::abs(band_restored.well_rubble_spilled_kg - tipped.well_rubble_spilled_kg) <= 0.5,
            "the restore must return the rider to A's parked cage with the rubble still on the "
            "deck, not back in the cage");
    std::cout << "PASS scraperx_sim AS-006 band: to_220_s=" << band_seconds
              << " y_220=" << band_220.player_position.y
              << " rearmed_skip_travel=" << rearmed.well_a_skip_travel
              << " landed_impact=" << band_landed.last_impact_speed_mps
              << " spilled_kg=" << tipped.well_rubble_spilled_kg
              << " again_y=" << band_again.player_position.y << '\n';

    // C's wreckage: from A's parked cage, the spent dumpster hangs just over
    // it; up its grab bar from the cage's west side and onto its grate.
    require(walk_to(band, -11.45, -132.25, 6.0, 0.08), "C wreck: to the cage's west side under the spent dumpster");
    (void)band.set_facing(1.0, 0.0);
    (void)band.advance_frame(0.4);
    (void)band.request_traversal();
    (void)band.advance_frame(0.2);
    require(is_climbing(band.snapshot()) &&
                band.snapshot().traversal_support_entity_id == Simulation::kWellCDumpsterEntityId,
            "C wreck: a hold on the spent dumpster's grab bar");
    require(hold_stick(band, 1.0, 0.0, 1.0, 0.0, 20.0,
                       [](const Snapshot &state) { return standing_above(state, 180.6); }) &&
                band.snapshot().support_entity_id == Simulation::kWellCDumpsterEntityId,
            "C wreck: up the bar and onto the dumpster's grate");
    std::cout << "PASS scraperx_sim AS-006 wreckage C: grate_y=" << band.snapshot().player_position.y << '\n';

    // ---- Step 2 movement (03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md §8) --
    //
    // Sprint: held with the stick forward the body runs at 8 m/s, and a jump
    // keeps that speed in the air and carries past where a walking jump
    // lands. Crouched, or with the stick across the facing, it walks.
    Simulation sprint(InitialSpawn::Deck154);
    require(sprint.advance_frame(1.0).accepted, "the sprint settle interval must be accepted");
    // Runs along the deck, east (+1) or west (-1).
    const auto run_east = [&](const double seconds, double &peak, bool &flag,
                              const double way = 1.0) {
        const auto ticks = static_cast<std::uint32_t>(seconds * Simulation::kTickRateHz);
        for (std::uint32_t tick = 0; tick < ticks; ++tick) {
            (void)sprint.set_move_input(way, 0.0);
            (void)sprint.set_facing(way, 0.0);
            (void)sprint.advance_frame(Simulation::kFixedStepSeconds);
            peak = std::max(peak, horizontal_speed(sprint.snapshot()));
            flag = flag || sprint.snapshot().player_sprinting;
        }
    };
    // A jump east at whatever speed the body has, stick held: its distance
    // and its slowest speed in the air.
    const auto jump_east = [&](double &distance, double &air_slowest, const double way = 1.0) {
        const double x0 = sprint.snapshot().player_position.x;
        (void)sprint.request_jump();
        air_slowest = std::numeric_limits<double>::infinity();
        for (std::uint32_t tick = 0; tick < 2U * Simulation::kTickRateHz; ++tick) {
            (void)sprint.set_move_input(way, 0.0);
            (void)sprint.set_facing(way, 0.0);
            (void)sprint.advance_frame(Simulation::kFixedStepSeconds);
            const auto state = sprint.snapshot();
            if (!state.player_grounded) {
                air_slowest = std::min(air_slowest, horizontal_speed(state));
            } else if (tick > 10U) {
                break;
            }
        }
        distance = (sprint.snapshot().player_position.x - x0) * way;
    };
    (void)sprint.set_sprint_input(true);
    double sprint_peak = 0.0;
    bool sprint_flag = false;
    run_east(2.0, sprint_peak, sprint_flag);
    double sprint_jump = 0.0;
    double sprint_air_slowest = 0.0;
    jump_east(sprint_jump, sprint_air_slowest);
    (void)sprint.set_sprint_input(false);
    // Walking back west: settled to a walk, then measured.
    double settle_peak = 0.0;
    bool settle_flag = false;
    run_east(0.6, settle_peak, settle_flag, -1.0);
    double walk_peak = 0.0;
    bool walk_flag = false;
    run_east(1.0, walk_peak, walk_flag, -1.0);
    double walk_jump = 0.0;
    double walk_air_slowest = 0.0;
    jump_east(walk_jump, walk_air_slowest, -1.0);
    require(sprint_flag && sprint_peak >= 7.9 && sprint_peak <= 8.05,
            "sprinting, the body must run at 8 m/s");
    require(sprint_air_slowest >= 7.9, "a sprinting jump must keep its speed in the air");
    require(sprint_jump >= walk_jump + 2.0,
            "a sprinting jump must carry at least 2 m past a walking one");
    require(!walk_flag && walk_peak <= 5.55, "without sprint held the body walks at 5.5 m/s");
    (void)sprint.set_sprint_input(true);
    (void)sprint.set_crouch_input(true);
    (void)sprint.advance_frame(0.3);
    double crouched_peak = 0.0;
    bool crouched_flag = false;
    for (std::uint32_t tick = 0; tick < 135U; ++tick) {
        (void)sprint.set_move_input(-1.0, 0.0);
        (void)sprint.set_facing(-1.0, 0.0);
        (void)sprint.advance_frame(Simulation::kFixedStepSeconds);
        if (tick > 45U) {
            crouched_peak = std::max(crouched_peak, horizontal_speed(sprint.snapshot()));
        }
        crouched_flag = crouched_flag || sprint.snapshot().player_sprinting;
    }
    (void)sprint.set_crouch_input(false);
    (void)sprint.set_move_input(0.0, 0.0);
    (void)sprint.advance_frame(0.5);
    double across_peak = 0.0;
    bool across_flag = false;
    for (std::uint32_t tick = 0; tick < 135U; ++tick) {
        (void)sprint.set_move_input(-1.0, 0.0);
        (void)sprint.set_facing(0.0, 1.0);
        (void)sprint.advance_frame(Simulation::kFixedStepSeconds);
        across_peak = std::max(across_peak, horizontal_speed(sprint.snapshot()));
        across_flag = across_flag || sprint.snapshot().player_sprinting;
    }
    (void)sprint.set_move_input(0.0, 0.0);
    require(!crouched_flag && crouched_peak <= 2.6, "crouched, sprint held must not sprint");
    require(!across_flag && across_peak <= 5.55,
            "with the stick across the facing, sprint held must not sprint");
    std::cout << "PASS scraperx_sim step2 sprint: peak=" << sprint_peak
              << " air_slowest=" << sprint_air_slowest << " sprint_jump_m=" << sprint_jump
              << " walk_jump_m=" << walk_jump << " crouched_peak=" << crouched_peak
              << " across_peak=" << across_peak << '\n';

    // Climb: facing a hold at hand height, Action takes it; the stick climbs
    // at 0.9 m/s hand over hand, the hands always within an arm of the
    // shoulders; the top-out mantles onto the ring. A column too thick to
    // close a hand round is no hold.
    Simulation climb(InitialSpawn::Deck154);
    require(climb.advance_frame(1.0).accepted, "the climb settle interval must be accepted");
    require(walk_to(climb, -13.2, -128.2, 5.0) && walk_to(climb, -13.2, -131.6, 5.0, 0.08),
            "the player must reach Stage A's head post");
    (void)climb.set_facing(0.0, -1.0);
    (void)climb.advance_frame(0.4);
    const auto facing_post = climb.snapshot();
    (void)climb.request_traversal();
    (void)climb.advance_frame(0.3);
    require(!facing_post.grip_available && !is_climbing(climb.snapshot()),
            "a 0.4 m column must not be a hold");
    require(walk_to(climb, -13.2, -128.2, 5.0) && walk_to(climb, -10.0, -128.2, 4.0),
            "the player must step back onto the deck");
    bool climb_grip_offered = false;
    double climb_rate_peak = 0.0;
    double hand_reach_worst = 0.0;
    std::uint32_t regrips = 0;
    Vector3 last_left{};
    {
        // The ladder, instrumented: rate, hands, regrips.
        require(walk_to(climb, 3.5, -128.2, 16.0) && walk_to(climb, 3.5, -129.8, 4.0) &&
                    walk_to(climb, 5.0, -129.6, 4.0, 0.08),
                "the player must reach the ladder's foot");
        (void)climb.set_facing(0.0, 1.0);
        (void)climb.advance_frame(0.4);
        climb_grip_offered = climb.snapshot().grip_available;
        (void)climb.request_traversal();
        (void)climb.advance_frame(0.2);
        require(is_climbing(climb.snapshot()), "Action facing the ladder must take hold of it");
        double last_y = climb.snapshot().player_position.y;
        last_left = climb.snapshot().traversal_left_hand;
        for (std::uint32_t tick = 0; tick < 40U * Simulation::kTickRateHz; ++tick) {
            (void)climb.set_move_input(0.0, 1.0);
            (void)climb.set_facing(0.0, 1.0);
            (void)climb.advance_frame(Simulation::kFixedStepSeconds);
            const auto state = climb.snapshot();
            if (!is_climbing(state)) {
                break;
            }
            climb_rate_peak = std::max(climb_rate_peak,
                                       (state.player_position.y - last_y) * Simulation::kTickRateHz);
            last_y = state.player_position.y;
            const Vector3 shoulder{state.player_position.x, state.player_position.y + 0.45,
                                   state.player_position.z};
            for (const Vector3 &hand : {state.traversal_left_hand, state.traversal_right_hand}) {
                hand_reach_worst = std::max(
                    hand_reach_worst, std::hypot(hand.x - shoulder.x, hand.y - shoulder.y, hand.z - shoulder.z));
            }
            if (std::abs(state.traversal_left_hand.y - last_left.y) > 0.05) {
                ++regrips;
            }
            last_left = state.traversal_left_hand;
        }
        require(hold_stick(climb, 0.0, 1.0, 0.0, 1.0, 3.0,
                           [](const Snapshot &state) { return standing_above(state, 176.5); }),
                "at the ladder's head the climb must mantle onto the 176 ring");
    }
    const auto on_176 = climb.snapshot();
    require(climb_grip_offered, "the ladder's rungs must be offered as a hold");
    require(climb_rate_peak <= 0.91, "the climb must be no faster than 0.9 m/s");
    require(hand_reach_worst <= 1.0, "the hands must stay within an arm's reach of the shoulders");
    require(regrips >= 20, "the hands must go hand over hand up the ladder");
    require(on_176.player_position.z > -128.91 && on_176.player_position.y > 177.0 &&
                on_176.player_position.y < 177.3,
            "the climb must end standing on the 176 ring");
    std::cout << "PASS scraperx_sim step2 climb: rate_peak=" << climb_rate_peak
              << " hand_reach_worst=" << hand_reach_worst << " left_regrips=" << regrips
              << " top_y=" << on_176.player_position.y
              << " column_hold=" << int(facing_post.grip_available) << '\n';

    // Past the last hold: climbing down the scaffold panel stops at its
    // bottom horizontal, over the catwalk, and letting go drops onto it.
    Simulation panel(InitialSpawn::Ring198East);
    require(panel.advance_frame(1.0).accepted, "the panel settle interval must be accepted");
    require(walk_to(panel, 11.0, -129.3, 6.0, 0.1) && walk_to(panel, 11.0, -131.40, 6.0, 0.08),
            "the player must reach the catwalk under the scaffold panel");
    (void)panel.set_facing(0.0, 1.0);
    (void)panel.advance_frame(0.4);
    (void)panel.request_jump();
    require(hold_stick(panel, 0.0, 0.6, 0.0, 1.0, 3.0,
                       [](const Snapshot &state) { return is_climbing(state); }),
            "a jump at the scaffold panel must catch it");
    const double caught_y = panel.snapshot().player_position.y;
    (void)hold_stick(panel, 0.0, -1.0, 0.0, 1.0, 4.0, [](const Snapshot &) { return false; });
    const auto bottom = panel.snapshot();
    // The lowest hold is the bottom horizontal at 200.30: a body climbing
    // down stops with it at the reach of its lower hand, feet 0.6 m over the
    // catwalk -- not stepping off onto it (0.1 m) and not falling past it.
    require(is_climbing(bottom) && bottom.player_position.y > 199.6 &&
                bottom.player_position.y < caught_y,
            "climbing down must stop at the panel's last hold, not run on past it");
    (void)panel.request_release();
    require(wait_for(panel, 2.0,
                     [](const Snapshot &state) {
                         return state.player_grounded && state.traversal_state == TraversalState::None;
                     }),
            "let go, the body must drop onto the catwalk");
    const auto panel_dropped = panel.snapshot();
    require(panel_dropped.death_count == 0 && panel_dropped.player_position.y > 199.0 &&
                panel_dropped.player_position.y < 199.3,
            "the drop from the panel's foot must land on the catwalk alive");
    std::cout << "PASS scraperx_sim step2 last hold: caught_y=" << caught_y
              << " stopped_y=" << bottom.player_position.y << " dropped_y="
              << panel_dropped.player_position.y << '\n';

    // Controlled drop and shimmy: on a deck, Drop does nothing; with the
    // ring's edge behind, it lowers the body over it into a hang under the
    // lip. Hanging, the stick moves it along the lip at 0.6 m/s until the lip
    // stops (a scaffold board lies across it); Jump climbs back up.
    Simulation ledge(InitialSpawn::Ring176East);
    require(ledge.advance_frame(1.0).accepted, "the ledge settle interval must be accepted");
    (void)ledge.set_facing(0.0, 1.0);
    (void)ledge.advance_frame(0.3);
    const auto mid_deck = ledge.snapshot();
    (void)ledge.request_release();
    (void)ledge.advance_frame(0.3);
    require(!mid_deck.edge_drop_available &&
                ledge.snapshot().traversal_state == TraversalState::None &&
                ledge.snapshot().player_grounded,
            "on the middle of a deck, Drop must not lower the body anywhere");
    (void)hold_stick(ledge, 0.0, -0.4, 0.0, 1.0, 1.0,
                     [](const Snapshot &state) { return state.player_position.z <= -128.45; });
    (void)ledge.advance_frame(0.3);
    require(ledge.snapshot().edge_drop_available,
            "backed up to the ring's inner edge, a drop must be offered");
    (void)ledge.request_release();
    require(wait_for(ledge, 1.5,
                     [](const Snapshot &state) {
                         return state.traversal_state == TraversalState::Hanging;
                     }),
            "Drop with the edge behind must lower the body into a hang");
    (void)ledge.advance_frame(0.3);
    const auto hanging = ledge.snapshot();
    require(hanging.player_position.y > 175.1 && hanging.player_position.y < 175.3 &&
                hanging.player_position.z < -128.91 && hanging.traversal_left_hand.y > 176.2,
            "the hang must hold the body under the 176 ring's lip, hands on it");
    const double shimmy_x0 = hanging.player_position.x;
    (void)hold_stick(ledge, 1.0, 0.0, 0.0, 1.0, 2.0, [](const Snapshot &) { return false; });
    const double shimmy_east = ledge.snapshot().player_position.x - shimmy_x0;
    (void)hold_stick(ledge, -1.0, 0.0, 0.0, 1.0, 10.0, [](const Snapshot &) { return false; });
    const auto stopped = ledge.snapshot();
    (void)ledge.request_jump();
    require(wait_for(ledge, 2.0, [](const Snapshot &state) { return standing_above(state, 177.0); }),
            "Jump from the hang must climb back onto the ring");
    require(shimmy_east >= 1.15 && shimmy_east <= 1.25,
            "the shimmy must move along the lip at 0.6 m/s");
    require(stopped.traversal_state == TraversalState::Hanging && stopped.player_position.x > 7.2 &&
                stopped.player_position.x < 7.9,
            "the shimmy must stop where a board across the lip ends the hold");
    std::cout << "PASS scraperx_sim step2 ledge: mid_deck_edge=" << int(mid_deck.edge_drop_available)
              << " hang_y=" << hanging.player_position.y << " shimmy_east_m=" << shimmy_east
              << " stopped_x=" << stopped.player_position.x
              << " up_y=" << ledge.snapshot().player_position.y << '\n';

    // Balance: on the scaffold boards (0.3 m wide) the body walks at 2 m/s
    // along them and a sideways push under 0.8 keeps it on their line; a full
    // push steps it off. On the ring's deck it is not balancing, nor on the
    // boards' first metre, which lies over the deck: a step off them there
    // lands on it. Out over the well (z < -129.0) the boards are a beam, and
    // by -129.35 the walk has settled to the balance pace.
    Simulation beam(InitialSpawn::Ring176East);
    require(beam.advance_frame(1.0).accepted, "the balance settle interval must be accepted");
    const bool deck_balancing = beam.snapshot().player_balancing;
    require(walk_to(beam, 7.05, -128.4, 8.0, 0.1), "the player must reach the boards");
    (void)beam.advance_frame(0.5);
    require(walk_to(beam, 7.05, -128.4, 2.0, 0.06), "the player must stand at the boards' end");
    (void)beam.advance_frame(0.5);
    double beam_peak = 0.0;
    bool beam_flag = true;
    (void)hold_stick(beam, 0.0, -1.0, 0.0, -1.0, 0.6, [&](const Snapshot &state) {
        if (state.player_position.z < -129.35) {
            beam_peak = std::max(beam_peak, horizontal_speed(state));
            beam_flag = beam_flag && state.player_balancing;
        }
        return false;
    });
    (void)beam.advance_frame(0.3);
    // Out over the well on the first board, well short of the corner.
    double beam_drift = 0.0;
    (void)hold_stick(beam, 0.5, 0.0, 0.0, -1.0, 1.5, [&](const Snapshot &state) {
        beam_drift = std::max(beam_drift, std::abs(state.player_position.x - 7.05));
        return false;
    });
    const auto still_on = beam.snapshot();
    const auto beam_deaths = still_on.death_count;
    (void)hold_stick(beam, 1.0, 0.0, 0.0, -1.0, 6.0,
                     [&](const Snapshot &state) { return state.death_count > beam_deaths; });
    require(!deck_balancing, "on the ring's deck the body must not be balancing");
    require(beam_flag && beam_peak <= 2.05, "on the boards the body must balance at 2 m/s");
    require(still_on.player_grounded && still_on.player_position.y > 177.3 && beam_drift <= 0.15,
            "a half sideways push must keep the body on the boards' line");
    require(beam.snapshot().death_count == beam_deaths + 1,
            "a full sideways push must step the body off the boards");
    std::cout << "PASS scraperx_sim step2 balance: peak=" << beam_peak << " drift_m=" << beam_drift
              << " deck_balancing=" << int(deck_balancing) << " stepped_off=1\n";

    // AS-006's no-lift route: from the 154 m deck to standing on the 220 ring
    // by ladder, boards and standpipe, catwalk and scaffold, with every lift
    // in the band left where it was found.
    Simulation route(InitialSpawn::Deck154);
    require(route.advance_frame(1.0).accepted, "the route settle interval must be accepted");
    const double route_start = route.snapshot().simulation_time_seconds;
    require(climb_route_ladder(route), "the route's ladder must take the player to the 176 ring");
    require(climb_route_pipe(route), "the route's boards and standpipe must take the player to 198");
    require(climb_route_panel(route), "the route's catwalk and scaffold must take the player to 220");
    (void)route.advance_frame(0.5);
    const auto route_top = route.snapshot();
    require(route_top.player_grounded && route_top.player_position.y > 221.0 &&
                route_top.player_position.y < 221.3 && route_top.player_position.z > -130.73 &&
                route_top.death_count == 0,
            "the no-lift route must end standing on the 220 ring");
    require(std::abs(route_top.well_a_cage_travel) <= 0.01 &&
                std::abs(route_top.well_b_cage_travel) <= 0.01 &&
                std::abs(route_top.well_c_platform_travel) <= 0.01 &&
                route_top.well_a_catch_latched && route_top.well_b_catch_latched &&
                route_top.well_c_catch_latched,
            "the no-lift route must leave every lift in the band where it was found");
    std::cout << "PASS scraperx_sim AS-006 route: seconds="
              << route_top.simulation_time_seconds - route_start
              << " top_y=" << route_top.player_position.y << " climbs=" << route_top.climb_count
              << " lifts_untouched=1\n";

    run_stack();
    run_wet_isolation();
    run_wet_band();
    run_wet_route();
    run_plate_shop();
    run_plate_band();
    run_plate_route();
    run_plate_wreckage();
    run_facade_crane();
    run_crane_band();
    run_crane_route();
    run_crane_wreckage();
    run_ascent();

    return EXIT_SUCCESS;
}
