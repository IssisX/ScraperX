#include "sim/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
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

    Simulation simulation(InitialSpawn::MantleApproach, scraperx::sim::WorldContent::RegressionFixtures);
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


// Runs the machine for a whole number of cycles and reports what the chain did.
struct MachineCycleReport final {
    double peak_valve_fraction = 0.0;
    double peak_lift_height = 0.0;
    double peak_piston_force = 0.0;
    double mass_flow_while_shut = 0.0;
    double final_available_energy = 0.0;
};

MachineCycleReport run_machine_cycles(scraperx::sim::Simulation &simulation,
                                      const double seconds) {
    using scraperx::sim::Simulation;
    MachineCycleReport report;
    const auto ticks = static_cast<std::uint32_t>(
        seconds * static_cast<double>(Simulation::kTickRateHz));
    for (std::uint32_t tick = 0; tick < ticks; ++tick) {
        require(simulation.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "machine cycle step must be accepted");
        const auto state = simulation.snapshot();
        report.peak_valve_fraction =
            std::max(report.peak_valve_fraction, state.valve_open_fraction);
        report.peak_lift_height =
            std::max(report.peak_lift_height, state.lift_platform_position.y);
        report.peak_piston_force = std::max(report.peak_piston_force, state.piston_force_n);
        if (state.valve_open_fraction <= 0.0) {
            report.mass_flow_while_shut =
                std::max(report.mass_flow_while_shut, state.orifice_mass_flow_kg_per_s);
        }
        report.final_available_energy = state.vessel_available_energy_j;
    }
    return report;
}


// ---- AS-003 MOD-HOOK5-RACK helpers ---------------------------------------
// Geometry read off the kHook5* constants in simulation.cpp.
constexpr double kHook5MinX = 8.0;
constexpr double kHook5MaxX = 12.0;
constexpr double kHook5MinZ = -88.0;
constexpr double kHook5MaxZ = -84.0;
constexpr double kHook5TopY = 4.45;
// In the doorway, east of the leaf once it has travelled inward.
constexpr double kHook5DoorwayX = 10.7;

// MOD-INTAKE-BELT's deck centre at a simulation time: AS-001's own stroke,
// the law the presentation mirrors.
double belt_centre_z(const double t) {
    return -96.0 + 9.0 * std::sin(0.40 * t);
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
    for (std::uint32_t tick = 0; tick < ticks; ++tick) {
        const auto state = simulation.snapshot();
        if (std::hypot(x - state.player_position.x, z - state.player_position.z) <= tolerance) {
            (void)simulation.set_move_input(0.0, 0.0);
            return true;
        }
        steer_toward(simulation, x, z);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)simulation.set_move_input(0.0, 0.0);
    return false;
}

// What the snapshot says about the cage: a cage body under the player, in its
// hands, or offered to them as a ledge.
struct CageReach final {
    bool touched = false;
    double peak_y = 0.0;
    // Horizontal gap between the body's centre and the cage footprint.
    double closest = std::numeric_limits<double>::infinity();
};

void note_cage(const scraperx::sim::Snapshot &state, CageReach &reach) {
    using scraperx::sim::Simulation;
    using scraperx::sim::TraversalState;
    reach.peak_y = std::max(reach.peak_y, state.player_position.y);
    const double gap_x = std::max({kHook5MinX - state.player_position.x, 0.0,
                                   state.player_position.x - kHook5MaxX});
    const double gap_z = std::max({kHook5MinZ - state.player_position.z, 0.0,
                                   state.player_position.z - kHook5MaxZ});
    reach.closest = std::min(reach.closest, std::hypot(gap_x, gap_z));
    if (state.support_entity_id == Simulation::kHook5CageEntityId ||
        (state.traversal_state != TraversalState::None &&
         state.traversal_support_entity_id == Simulation::kHook5CageEntityId) ||
        (state.ledge_available && state.ledge_entity_id == Simulation::kHook5CageEntityId)) {
        reach.touched = true;
    }
}

// Runs flat out at a point, jumps within `jump_within` of it, and requests
// traversal on every airborne tick until `seconds` have passed.
void leap_at(scraperx::sim::Simulation &simulation, const double x, const double z,
             const double jump_within, const double seconds, CageReach &reach) {
    using scraperx::sim::Simulation;
    bool jumped = false;
    const auto ticks =
        static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (std::uint32_t tick = 0; tick < ticks; ++tick) {
        const auto state = simulation.snapshot();
        note_cage(state, reach);
        if (state.support_entity_id == Simulation::kIntakeBeltEntityId) {
            break;  // the deck is the key this run is not testing
        }
        const double dx = x - state.player_position.x;
        const double dz = z - state.player_position.z;
        const double length = std::max(std::hypot(dx, dz), 1.0e-6);
        (void)simulation.set_move_input(dx / length, dz / length);
        (void)simulation.set_facing(dx / length, dz / length);
        if (!jumped && state.player_grounded && length <= jump_within) {
            (void)simulation.request_jump();
            jumped = true;
        } else if (jumped) {
            (void)simulation.request_traversal();
        }
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)simulation.set_move_input(0.0, 0.0);
    for (int tick = 0; tick < 45; ++tick) {
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        note_cage(simulation.snapshot(), reach);
    }
}

// From the B00 pendant catwalk: wait for the deck near its south stroke,
// step across onto its west half, ride it north past the 9 t pack, move out
// to its east edge, and jump-grab the cage's west buttress when the deck has
// carried the body alongside. True once standing on a cage body.
bool ride_belt_onto_cage(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    int phase = 0;
    double phase_start = simulation.snapshot().simulation_time_seconds;
    for (std::uint32_t tick = 0; tick < 90 * 70; ++tick) {
        const auto state = simulation.snapshot();
        const double t = state.simulation_time_seconds;
        const double deck = belt_centre_z(t);
        const bool on_belt = state.support_entity_id == Simulation::kIntakeBeltEntityId;
        if (phase == 0) {
            steer_toward(simulation, 3.0, -101.0);
            if (deck < -104.0) {
                phase = 1;
                phase_start = t;
            }
        } else if (phase == 1) {
            steer_toward(simulation, 5.3, -101.0);
            if (on_belt && state.player_position.x > 5.0) {
                phase = 2;
                phase_start = t;
            } else if (t - phase_start > 3.0) {
                return false;
            }
        } else if (phase == 2) {
            // 1 m ahead of the deck's centre; on its west half until clear
            // of the 9 t pack and its mast, then out to its east edge.
            steer_toward(simulation, state.player_position.z > -95.3 ? 7.35 : 5.3, deck + 1.0);
            if (deck >= -88.6 && state.player_position.x > 7.2 &&
                state.player_position.z > kHook5MinZ + 0.3) {
                phase = 3;
                phase_start = t;
            } else if (t - phase_start > 25.0) {
                return false;
            }
        } else if (phase == 3) {
            (void)simulation.set_move_input(1.0, 0.0);
            (void)simulation.set_facing(1.0, 0.0);
            if (state.player_grounded && state.player_position.x > 7.45) {
                (void)simulation.request_jump();
                phase = 4;
                phase_start = t;
            } else if (t - phase_start > 2.0) {
                return false;
            }
        } else {
            (void)simulation.set_move_input(1.0, 0.0);
            (void)simulation.set_facing(1.0, 0.0);
            (void)simulation.request_traversal();
            if (state.player_grounded &&
                state.support_entity_id == Simulation::kHook5CageEntityId) {
                (void)simulation.set_move_input(0.0, 0.0);
                return true;
            }
            if (t - phase_start > 6.0) {
                return false;
            }
        }
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    }
    return false;
}

// From the roof, across to the hatch and down it onto the cage floor.
bool drop_through_hatch(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    for (std::uint32_t tick = 0; tick < 90 * 8; ++tick) {
        steer_toward(simulation, 10.3, kHook5MinZ + 2.2);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = simulation.snapshot();
        if (state.player_grounded && state.player_position.y < 1.2 &&
            state.player_position.x > kHook5MinX && state.player_position.x < kHook5MaxX &&
            state.player_position.z > kHook5MinZ && state.player_position.z < kHook5MaxZ) {
            (void)simulation.set_move_input(0.0, 0.0);
            return true;
        }
    }
    return false;
}

// Inside the cage: lift MOD-HOOK5-BAR out of its brackets, back away north
// with it, and set it down on the floor.
bool lift_bar_clear(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, 10.51, kHook5MinZ + 1.1, 4.0, 0.1)) {
        return false;
    }
    (void)simulation.set_facing(0.0, -1.0);
    (void)simulation.advance_frame(0.3);
    if (simulation.snapshot().carry_target_entity_id != Simulation::kHook5BarEntityId) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.3);
    if (simulation.snapshot().carrying_entity_id != Simulation::kHook5BarEntityId) {
        return false;
    }
    // Back away north until the bar hangs in the room's middle band: clear
    // of the leaf's open stop to the south and of the rack to the north.
    for (int tick = 0; tick < 3 * 90 && simulation.snapshot().player_position.z <
                                            kHook5MinZ + 2.3;
         ++tick) {
        (void)simulation.set_move_input(0.0, 0.6);
        (void)simulation.set_facing(0.0, -1.0);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)simulation.set_move_input(0.0, 0.0);
    (void)simulation.advance_frame(0.3);
    (void)simulation.request_set_down();
    (void)simulation.advance_frame(0.3);
    return simulation.snapshot().carrying_entity_id == 0;
}

// Inside the cage, north of the dropped bar: face the rack in the north-east
// corner and take the hook block off it.
bool take_block(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, 10.5, kHook5MaxZ - 0.75, 4.0, 0.1)) {
        return false;
    }
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(0.3);
    if (simulation.snapshot().carry_target_entity_id != Simulation::kHook5BlockEntityId) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.3);
    return simulation.snapshot().carrying_entity_id == Simulation::kHook5BlockEntityId;
}

// Out through the travelled doorway to the apron south of the cage.
bool carry_out_of_cage(scraperx::sim::Simulation &simulation) {
    return walk_to(simulation, kHook5DoorwayX, kHook5MinZ + 1.2, 4.0) &&
           walk_to(simulation, kHook5DoorwayX, kHook5MinZ - 2.0, 4.0);
}

// Walks slowly up to the hook block lying on the ground, faces it, and picks
// it up; true once it is on the carry point.
bool pick_block_from_ground(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    for (std::uint32_t tick = 0; tick < 90 * 8; ++tick) {
        const auto state = simulation.snapshot();
        const double dx = state.hook5_block_position.x - state.player_position.x;
        const double dz = state.hook5_block_position.z - state.player_position.z;
        const double length = std::max(std::hypot(dx, dz), 1.0e-6);
        (void)simulation.set_facing(dx / length, dz / length);
        if (length > 0.8) {
            (void)simulation.set_move_input(0.35 * dx / length, 0.35 * dz / length);
        } else {
            (void)simulation.set_move_input(0.0, 0.0);
            if (state.carry_target_entity_id == Simulation::kHook5BlockEntityId) {
                (void)simulation.request_pick_up();
                (void)simulation.advance_frame(0.3);
                return simulation.snapshot().carrying_entity_id ==
                       Simulation::kHook5BlockEntityId;
            }
        }
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    }
    return false;
}

// ---- AS-006, the Counterweight Well ----------------------------------------

constexpr double kGravity = 9.81;
constexpr double kRiderMassKg = 85.0;
constexpr double kWellACageMassKg = 350.0;
constexpr double kWellASkipMassKg = 800.0;
constexpr double kWellACageFloorTop = 154.25;
constexpr double kWellATravel = 22.0;
constexpr double kWellBCageMassKg = 350.0;
constexpr double kWellBCounterweightMassKg = 2500.0;
constexpr double kWellBCageFloorTop = 176.25;
constexpr double kWellBTravel = 22.0;
constexpr double kWellBCounterweightHalfHeight = 11.0;
// The energy rule is checked from body states every tick. A standing capsule
// settles by millimetres on whatever floor it is on; 1 cm of that for the
// 85 kg rider is the tolerance, so stance noise is never read as a motor.
constexpr double kStanceJitterJ = kRiderMassKg * kGravity * 0.01;

double kit_y(const scraperx::sim::Simulation &simulation, const std::uint64_t entity) {
    return simulation.kit_body_position(simulation.kit_body_index(entity)).y;
}

// From the stair's top deck into Stage A's cage through its open north side,
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


bool board_well_b(scraperx::sim::Simulation &simulation) {
    return walk_to(simulation, -8.65, -131.40, 3.0, 0.08) &&
           walk_to(simulation, -7.55, -131.40, 3.0, 0.08);
}

bool rig_well_b(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    const auto fail = [&](const char *step) {
        const auto s = simulation.snapshot();
        std::cout << "INFO AS-006 B rig fail: step=" << step
                  << " pos=(" << s.player_position.x << ',' << s.player_position.y << ','
                  << s.player_position.z << ") grounded=" << int(s.player_grounded)
                  << " support=" << s.support_entity_id
                  << " rig_action=" << int(s.rig_action)
                  << " rig_target=" << s.rig_target_entity_id
                  << " carrying=" << s.carrying_entity_id
                  << " rope_end=" << s.well_b_rope_end_entity_id
                  << " rope_tension=" << s.well_b_rope_tension_n
                  << " cw_travel=" << s.well_b_counterweight_travel << '\n';
        return false;
    };
    // The interaction, not centimeter-perfect foot placement, is the
    // contract. The cage/post collision resolves this stance about 0.11 m
    // from the nominal point while already offering the correct UNHOOK verb.
    if (!walk_to(simulation, -8.45, -131.95, 3.0, 0.15)) return fail("walk_bollard");
    (void)simulation.set_facing(-0.65, -0.75);
    (void)simulation.advance_frame(0.4);
    const auto at_bollard = simulation.snapshot();
    if (at_bollard.rig_action != 2 ||
        at_bollard.rig_target_entity_id != Simulation::kWellBShackleEntityId)
        return fail("offer_unhook");
    (void)simulation.request_rig();
    (void)simulation.advance_frame(0.3);
    if (simulation.snapshot().carrying_entity_id != Simulation::kWellBShackleEntityId)
        return fail("take_shackle");
    if (!walk_to(simulation, -6.55, -131.40, 3.0, 0.08)) return fail("walk_eye");
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(0.5);
    const auto at_eye = simulation.snapshot();
    if (at_eye.rig_action != 1 ||
        at_eye.rig_target_entity_id != Simulation::kWellBCageEntityId)
        return fail("offer_hook");
    (void)simulation.request_rig();
    (void)simulation.advance_frame(0.3);
    const auto hooked = simulation.snapshot();
    if (hooked.carrying_entity_id != 0 ||
        hooked.well_b_rope_end_entity_id != Simulation::kWellBCageEntityId)
        return fail("hook_commit");
    return true;
}

bool pull_well_b(scraperx::sim::Simulation &simulation, const double pull, const double seconds) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, -7.35, -130.35, 3.0, 0.08)) return false;
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(0.5);
    const auto facing = simulation.snapshot();
    if (facing.carry_target_entity_id != Simulation::kWellBHandleEntityId ||
        facing.carry_target_kind != 2) return false;
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.3);
    if (simulation.snapshot().carrying_entity_id != Simulation::kWellBHandleEntityId) return false;
    bool opened = false;
    const auto ticks =
        static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (std::uint32_t tick = 0; tick < ticks && !opened; ++tick) {
        (void)simulation.set_move_input(0.0, -0.7 * pull);
        (void)simulation.set_facing(0.0, 1.0);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        opened = !simulation.snapshot().well_b_catch_latched;
    }
    (void)simulation.set_move_input(0.0, 0.0);
    if (simulation.snapshot().carrying_entity_id != 0) (void)simulation.request_set_down();
    return opened;
}

} // namespace

int main() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;

    // The actual shipped default must contain no retired machinery, including
    // static frames or invisible colliders. Do not select a fixture spawn here.
    {
        Simulation foundation;
        for (std::uint64_t entity = 3; entity <= 59; ++entity) {
            if (entity == Simulation::kTowerEntityId || entity == Simulation::kWorldSolidEntityId) {
                continue;
            }
            require(foundation.entity_body_count(entity) == 0,
                    "default world must not construct a retired fixture or campaign body");
        }
        require(foundation.moving_body_count() == 33,
                "default world must contain the player and 32 pipe bridge bodies");
        require(foundation.kit_body_count() == 68 && foundation.kit_cable_count() == 2,
                "default world must contain only the pipe bridge Kit bodies and two control cords");
        require(foundation.entity_body_count(Simulation::kTowerEntityId) > 1 &&
                    foundation.entity_body_count(Simulation::kWorldSolidEntityId) > 0,
                "the tower frame and environment must remain real collision geometry");
        (void)foundation.advance_frame(1.0);
        require(foundation.snapshot().player_grounded, "default player must settle at grade");
        const auto start = foundation.snapshot().player_position;
        (void)foundation.set_move_input(1.0, 0.0);
        (void)foundation.advance_frame(0.5);
        (void)foundation.set_move_input(0.0, 0.0);
        require(foundation.snapshot().player_position.x > start.x + 1.0,
                "normal locomotion must work without legacy machinery");
        (void)foundation.set_crouch_input(true);
        (void)foundation.advance_frame(0.2);
        require(foundation.snapshot().player_crouched, "default crouch must remain functional");
        (void)foundation.set_crouch_input(false);
        (void)foundation.advance_frame(0.2);
        (void)foundation.request_jump();
        (void)foundation.advance_frame(0.1);
        require(!foundation.snapshot().player_grounded && foundation.snapshot().player_linear_velocity.y > 2.0,
                "default jump must remain functional");
        (void)foundation.advance_frame(2.0);
        require(foundation.snapshot().player_grounded && foundation.snapshot().death_count == 0,
                "the default jump must land safely on grade");
        std::cout << "PASS scraperx_sim ground foundation: retired_bodies=0 moving_bodies=33 kit_bodies=68 locomotion=1 crouch=1 jump=1\n";
    }

    {
        Simulation fallback;
        (void)fallback.advance_frame(0.5);
        walk_toward(fallback, -25.0, -118.0, 20.0);
        walk_toward(fallback, -25.0, -128.5, 6.0);
        for (int level = 0; level < 14; ++level) {
            const double side = level % 2 == 0 ? 1.0 : -1.0;
            const double band = -150.0 + side * 21.5;
            walk_toward(fallback, -side * 20.0, band, 12.0);
            walk_toward(fallback, side * 21.0, band, 16.0);
            const auto at = fallback.snapshot();
            const double deck = 11.0 * (level + 1);
            require(at.player_grounded && at.player_position.y > deck + 0.5 &&
                        at.player_position.y < deck + 1.5,
                    "default fallback must walk every flight without a machine or jump");
            walk_toward(fallback, side * 21.5, -150.0 - side * 21.5, 12.0);
        }
        require(fallback.snapshot().death_count == 0,
                "default fallback must reach the top without a death restore");
        std::cout << "PASS scraperx_sim fallback stairs: levels=14 top_y="
                  << fallback.snapshot().player_position.y << " jump_requests=0\n";

        Simulation drop(InitialSpawn::HighDrop);
        (void)drop.advance_frame(8.0);
        require(drop.snapshot().death_count == 1 && drop.snapshot().player_grounded,
                "default fatal fall must restore safely without any legacy body queries");
        require(drop.moving_body_count() == 33 && drop.kit_body_count() == 68,
                "checkpoint restore must not recreate retired machinery");
        std::cout << "PASS scraperx_sim default checkpoint: death_restore=1 retired_respawn=0\n";
    }

    Simulation partitioned(InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::RegressionFixtures);
    for (std::uint32_t i = 0; i < Simulation::kTickRateHz; ++i) {
        const auto result = partitioned.advance_frame(Simulation::kFixedStepSeconds);
        require(result.accepted, "fixed-step input must be accepted");
        require(result.steps_advanced == 1, "each exact fixed step must advance once");
    }

    const auto partitioned_snapshot = partitioned.snapshot();
    require(partitioned_snapshot.tick_index == 90, "90 fixed steps must produce tick 90");
    require(nearly_equal(partitioned_snapshot.simulation_time_seconds, 1.0),
            "tick-derived simulation time must equal one second");

    Simulation batched(InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::RegressionFixtures);
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

    Simulation remainder(InitialSpawn::StaticDeck, scraperx::sim::WorldContent::RegressionFixtures);
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

    Simulation supported(InitialSpawn::StaticDeck, scraperx::sim::WorldContent::RegressionFixtures);
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

    Simulation translating(InitialSpawn::TranslatingSupport, scraperx::sim::WorldContent::RegressionFixtures);
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

    Simulation rotating(InitialSpawn::RotatingSupport, scraperx::sim::WorldContent::RegressionFixtures);
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

    Simulation vault(InitialSpawn::VaultApproach, scraperx::sim::WorldContent::RegressionFixtures);
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
        Simulation run(InitialSpawn::VaultApproach, scraperx::sim::WorldContent::RegressionFixtures);
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

    Simulation mantle(InitialSpawn::MantleApproach, scraperx::sim::WorldContent::RegressionFixtures);
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

    Simulation hang(InitialSpawn::HangApproach, scraperx::sim::WorldContent::RegressionFixtures);
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

    Simulation moving(InitialSpawn::MovingLedgeApproach, scraperx::sim::WorldContent::RegressionFixtures);
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

    Simulation released(InitialSpawn::MovingLedgeApproach, scraperx::sim::WorldContent::RegressionFixtures);
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

    Simulation blocked(InitialSpawn::BlockedLedgeApproach, scraperx::sim::WorldContent::RegressionFixtures);
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


    // ---- WO-006 coupled machine -----------------------------------------

    Simulation machine(InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::RegressionFixtures);
    const auto machine_start = machine.snapshot();
    require(machine_start.player_position.z < -20.0 && machine_start.player_position.y < 2.0,
            "the default spawn must be outdoors at grade, short of the tower");
    require(machine_start.vessel_pressure_pa > 4.0e5,
            "the plant must start charged");
    require(machine_start.valve_open_fraction == 0.0, "the valve must start shut");
    require(machine_start.lift_platform_position.y < 1.5,
            "the lift must start parked at the bottom of its travel");

    const auto first_cycle = run_machine_cycles(machine, 26.0);
    require(first_cycle.peak_valve_fraction > 0.5,
            "the falling ballast must drive the rope and open the real valve past half");
    require(first_cycle.peak_piston_force > 8000.0,
            "the vented cylinder must push the piston with material force");
    require(first_cycle.peak_lift_height > 6.0,
            "the piston must lift the counterweighted platform several metres");
    require(first_cycle.mass_flow_while_shut == 0.0,
            "a shut valve must pass exactly zero mass: the plume has no source of its own");

    const auto second_cycle = run_machine_cycles(machine, 26.0);
    require(second_cycle.peak_lift_height > 6.0,
            "the machine must complete its return loop and fire again unattended");
    const auto machine_settled = machine.snapshot();
    require(machine_settled.lift_platform_position.y < 2.0,
            "the platform must sink again once the cylinder bleeds down");
    require(machine_settled.counterweight_position.y > 6.5,
            "the counterweight must return as the platform descends");

    // Governing Law 24: the plant cannot manufacture work. With the boiler feed
    // cut it is a strictly finite reservoir, and the lift must fade and stop.
    Simulation starved(InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::RegressionFixtures);
    starved.set_boiler_feed_enabled(false);
    const auto starved_first = run_machine_cycles(starved, 26.0);
    require(starved_first.peak_lift_height > 5.0,
            "the first stroke must still work on stored energy alone");
    double previous_peak = starved_first.peak_lift_height;
    double previous_energy = starved_first.final_available_energy;
    for (int cycle = 0; cycle < 4; ++cycle) {
        const auto next = run_machine_cycles(starved, 26.0);
        require(next.final_available_energy < previous_energy + 1.0,
                "a starved vessel's available energy must never increase");
        previous_energy = next.final_available_energy;
        previous_peak = std::min(previous_peak, next.peak_lift_height);
    }
    const auto starved_final = starved.snapshot();
    require(starved_final.vessel_available_energy_j < starved_first.final_available_energy,
            "repeated strokes must draw the finite reservoir down");
    require(starved_final.lift_platform_position.y < 3.0,
            "a drained plant must leave the lift low rather than holding it up for free");

    // The player is a body in the plant, but an 85 kg body is not a machine.
    // Standing on a 900 kg counterweighted tipper moves it by a fraction of a
    // milliradian and opens the valve not at all. This is the GDD 17 guard --
    // "the tower itself is a major source of power... rather than granting the
    // player industrial-scale strength directly" -- and it is load-bearing: it
    // fails the moment anyone hands the player freight-scale mass again, which
    // is exactly how the original version of this falsifier passed (the player
    // capsule took Jolt's default density, 602.9 kg, and simply outweighed the
    // machine). Player authority over the plant must come from leverage and
    // timing, which is what the catwalk treadle below provides.
    Simulation disturbed(InitialSpawn::MachineYard, scraperx::sim::WorldContent::RegressionFixtures);
    require(disturbed.set_facing(1.0, 0.0), "yard facing must be accepted");
    require(disturbed.set_move_input(1.0, 0.0), "yard approach input must be accepted");
    require(advance_until(disturbed,
                          [](const Snapshot &state) {
                              return state.player_grounded &&
                                     state.support_entity_id == Simulation::kTipperEntityId;
                          },
                          6.0),
            "the player must be able to stand on the native tipper deck");
    const auto standing = disturbed.snapshot();
    require(disturbed.advance_frame(1.5).accepted, "player-driven linkage interval must advance");
    const auto disturbed_result = disturbed.snapshot();
    require(std::abs(disturbed_result.tipper_angle_radians - standing.tipper_angle_radians) < 0.01,
            "an unaided 85 kg body must not swing a 900 kg counterweighted tipper -- the player "
            "does not get industrial-scale strength for free (GDD 17)");
    require(disturbed_result.valve_open_fraction == 0.0,
            "standing on the tipper must not open the valve by body mass alone");

    // The machine is fixed-step-owned like everything else.
    Simulation machine_partitioned(InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::RegressionFixtures);
    for (std::uint32_t tick = 0; tick < Simulation::kTickRateHz * 20; ++tick) {
        require(machine_partitioned.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "machine partition step must be accepted");
    }
    Simulation machine_batched(InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::RegressionFixtures);
    require(machine_batched.advance_frame(20.0).accepted,
            "machine batched interval must be accepted");
    const auto partitioned_machine = machine_partitioned.snapshot();
    const auto batched_machine = machine_batched.snapshot();
    require(partitioned_machine.tick_index == batched_machine.tick_index,
            "machine frame partitioning must not change tick count");
    require(nearly_equal(partitioned_machine.lift_platform_position.y,
                         batched_machine.lift_platform_position.y,
                         1.0e-6) &&
                nearly_equal(partitioned_machine.vessel_pressure_pa,
                             batched_machine.vessel_pressure_pa,
                             1.0e-6),
            "machine frame partitioning must not change authoritative machine state");

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

    Simulation lethal(InitialSpawn::HighDrop, scraperx::sim::WorldContent::RegressionFixtures);
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

    Simulation survivable(InitialSpawn::SurvivableDrop, scraperx::sim::WorldContent::RegressionFixtures);
    require(advance_until(survivable,
                          [](const Snapshot &state) { return state.player_grounded; },
                          4.0),
            "the short drop must land");
    require(survivable.snapshot().death_count == 0,
            "an ordinary ~12 m platforming fall must never be lethal (GDD 8.2)");

    Simulation chuted(InitialSpawn::HighDrop, scraperx::sim::WorldContent::RegressionFixtures);
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

    Simulation late_chute(InitialSpawn::HighDrop, scraperx::sim::WorldContent::RegressionFixtures);
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

    Simulation grounded_parachute(InitialSpawn::StaticDeck, scraperx::sim::WorldContent::RegressionFixtures);
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

    Simulation committed(InitialSpawn::MachineYard, scraperx::sim::WorldContent::RegressionFixtures);
    require(committed.set_facing(1.0, 0.0), "checkpoint test facing must be accepted");
    require(committed.set_move_input(1.0, 0.0), "checkpoint test approach input must be accepted");
    require(advance_until(committed,
                          [](const Snapshot &state) {
                              return state.player_grounded &&
                                     state.support_entity_id == Simulation::kTipperEntityId;
                          },
                          6.0),
            "the checkpoint test must reach the native tipper deck");
    // Stop walking on arrival. Without this the player simply strides off the
    // far side of the tipper and is mid-air when the jump below is requested --
    // the original version only stayed put because a 602.9 kg player sank the
    // beam and wedged there, which was never the behaviour being tested.
    require(committed.set_move_input(0.0, 0.0), "checkpoint test halt input must be accepted");
    const auto pre_commit_checkpoints = committed.snapshot().checkpoint_commit_count;
    require(committed.advance_frame(1.0).accepted,
            "standing on the tipper must advance and keep auto-committing");
    const auto disturbed_checkpoint = committed.snapshot();
    require(disturbed_checkpoint.checkpoint_commit_count > pre_commit_checkpoints,
            "standing grounded must keep advancing the automatic commit count");
    require(disturbed_checkpoint.support_entity_id == Simulation::kTipperEntityId,
            "the committed checkpoint must be taken while the player stands on a real dynamic "
            "machine body, so the commit covers machine state and not just static ground "
            "(TDD 14.1)");
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

    // Death must restore real *machine* state, not just the player -- proof that
    // this is a genuine TDD-14.1 state rollback and not a scripted respawn. The
    // hoist/ballast cycle runs autonomously (WO-006), so it changes measurably
    // within seconds with no player input at all. HighDrop's ~6 s fall to
    // death never touches ground, so the only checkpoint in play is the one
    // seeded at construction -- if restore is real, the ballast must return to
    // its pristine seeded height, not the height the autonomous cycle had
    // already carried it to by the moment of death.
    Simulation machine_restore(InitialSpawn::HighDrop, scraperx::sim::WorldContent::RegressionFixtures);
    const double seeded_ballast_y = machine_restore.snapshot().ballast_position.y;
    require(machine_restore.advance_frame(2.5).accepted,
            "letting the machine run autonomously before death must be accepted");
    const auto mid_fall = machine_restore.snapshot();
    require(!mid_fall.player_grounded && mid_fall.death_count == 0,
            "the restore-signal window must still be mid-fall, before any death or re-commit");
    require(mid_fall.ballast_position.y > seeded_ballast_y + 2.0,
            "the autonomous hoist must have measurably lifted the ballast with zero player input");
    require(advance_until(machine_restore,
                          [](const Snapshot &state) { return state.death_count >= 1; },
                          2.0),
            "the remainder of the unmitigated fall must be lethal");
    const auto post_death = machine_restore.snapshot();
    require(post_death.death_count == 1, "exactly one death must be recorded");
    require(std::abs(post_death.ballast_position.y - seeded_ballast_y) < 0.1,
            "death must restore the ballast to its seeded checkpoint height, not leave it "
            "wherever the autonomous cycle had carried it -- proving machine state, not just "
            "the player, is part of the checkpoint");

    // Frame-partition invariance for the whole subsystem.
    Simulation fall_partitioned(InitialSpawn::HighDrop, scraperx::sim::WorldContent::RegressionFixtures);
    for (std::uint32_t tick = 0; tick < Simulation::kTickRateHz * 4; ++tick) {
        require(fall_partitioned.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "fall-subsystem partition step must be accepted");
    }
    Simulation fall_batched(InitialSpawn::HighDrop, scraperx::sim::WorldContent::RegressionFixtures);
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

    // ---- WO-009 first full causal chain ------------------------------------
    // TDD section 24, "Work Order 008 -- first full causal chain" (original
    // numbering; WO-008/fall-parachute-checkpoint already used and recorded
    // this same renumbering precedent). Player intervention driving this
    // exact valve/orifice/piston mechanism was already proven in WO-006 (a
    // player standing on the tipper opens the same valve the autonomous
    // cycle uses); it is cited here, not re-derived, because there is no
    // pathfinding in this codebase to script a literal walk from the tipper
    // to the platform without inventing test-only navigation machinery the
    // claim does not need -- both triggers drive the identical code path.
    // What was never proven is that the structural/process consequence
    // (the piston lifting the platform) changes what is reachable, and that
    // the newly reached position is what the checkpoint system persists.
    Simulation chain(InitialSpawn::LiftPlatform, scraperx::sim::WorldContent::RegressionFixtures);
    require(chain.set_facing(0.0, -1.0), "chain facing toward the catwalk must be accepted");
    require(chain.set_move_input(0.0, -1.0), "chain move-to-edge input must be accepted");
    require(advance_until(chain,
                          [](const Snapshot &state) { return state.player_position.z <= -101.25; },
                          3.0),
            "the player must be able to walk from the platform's centre toward its near edge");
    require(chain.set_move_input(0.0, 0.0), "chain hold-at-edge input must be accepted");
    require(chain.advance_frame(1.0).accepted, "chain settle-at-edge interval must be accepted");
    const auto chain_rest = chain.snapshot();
    require(chain_rest.player_grounded &&
                chain_rest.support_entity_id == Simulation::kLiftPlatformEntityId,
            "the player must settle grounded on the real platform near its edge, not fall from it");
    require(!chain_rest.ledge_available,
            "the catwalk must not be reachable while the platform sits at rest -- a machine at "
            "rest must not grant a capability it has not yet earned (Governing Law 24)");

    require(advance_until(chain,
                          [](const Snapshot &state) {
                              return state.ledge_available &&
                                     state.ledge_entity_id == Simulation::kCatwalkEntityId;
                          },
                          20.0),
            "the autonomous plant cycle must eventually lift the platform into real mantle range "
            "of the catwalk -- the structural consequence must change what is reachable");
    const auto chain_gated = chain.snapshot();
    require(chain_gated.lift_platform_position.y > 5.0 && chain_gated.lift_platform_position.y < 9.0,
            "the gate must open because the platform is genuinely elevated, not at some other time");
    require(chain_gated.ledge_rise_meters > 0.3 && chain_gated.ledge_rise_meters < 1.9,
            "the offered rise must match the real mantle band, exactly like any other mantle");

    require(chain.request_traversal(), "the catwalk mantle request must be accepted");
    require(chain.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "chain mantle commit tick must advance");
    require(chain.snapshot().traversal_state == TraversalState::Mantling,
            "a genuinely reachable ledge must commit a native mantle, exactly like any other mantle");
    require(advance_until(chain,
                          [](const Snapshot &state) {
                              return state.player_grounded &&
                                     state.support_entity_id == Simulation::kCatwalkEntityId;
                          },
                          2.0),
            "the mantle must end grounded on the real catwalk entity -- the machine-enabled "
            "destination, not a scripted teleport");
    const auto chain_reached = chain.snapshot();
    require(chain_reached.accepted_traversal_count == 1,
            "the chain mantle must be accepted exactly once");
    require(chain_reached.aborted_traversal_count == 0, "a genuinely reachable mantle must not abort");
    require(chain_reached.rejected_traversal_count == 0,
            "a single, valid traversal request must not be rejected");
    require(chain_reached.player_position.z < -102.9 && chain_reached.player_position.z > -110.0,
            "the player must actually stand on the catwalk deck, not merely near it");

    const auto pre_chain_commits = chain_reached.checkpoint_commit_count;
    require(chain.advance_frame(1.0).accepted,
            "standing on the newly reached catwalk must advance and auto-commit (GDD 9.1)");
    const auto chain_committed = chain.snapshot();
    require(chain_committed.checkpoint_commit_count > pre_chain_commits,
            "standing grounded on the catwalk must keep auto-committing, exactly like any other "
            "grounded position");
    require(std::abs(chain_committed.checkpoint_position.x - chain_committed.player_position.x) <
                    0.05 &&
                std::abs(chain_committed.checkpoint_position.z - chain_committed.player_position.z) <
                    0.05,
            "the persisted checkpoint must now be the machine-enabled catwalk position -- the "
            "changed traversal capability is what gets persisted (TDD 14.1). Restore-to-an-"
            "arbitrary-committed-position was already proven position- and entity-agnostic in "
            "WO-008 (capture_body/restore_body replay whatever was captured, with no per-location "
            "special case) and is not re-derived here");

    // ---- WO-010 the player is the plant's missing component ----------------
    // An 85 kg body cannot move machine-scale mass (proved above on the tipper),
    // so player authority enters through a control built for a body: a treadle
    // on the catwalk, cabled over two sheaves to the valve gear. It is reachable
    // only by someone the lift has already carried up (WO-009), and what it buys
    // is not strength but *duration* -- the autonomous cycle only crosses the
    // catwalk mantle band for a fraction of a second every 26 s, whereas a body
    // standing on the pedal parks the lift inside that band for as long as it
    // stands there. GDD 16: change machinery -> change access -> change traversal.
    Simulation idle_plant(InitialSpawn::MachineYard, scraperx::sim::WorldContent::RegressionFixtures);
    require(idle_plant.advance_frame(8.0).accepted, "control-plant interval must be accepted");
    const auto idle_eight = idle_plant.snapshot();
    require(idle_eight.valve_open_fraction == 0.0,
            "with nobody on the treadle the valve must still be shut at this point in the cycle");
    require(idle_eight.lift_platform_position.y < 2.0,
            "with nobody on the treadle the lift must still be parked -- the machine does not "
            "open this route on its own at this moment");

    Simulation treadle(InitialSpawn::CatwalkTreadle, scraperx::sim::WorldContent::RegressionFixtures);
    const auto treadle_spawn = treadle.snapshot();
    require(std::abs(treadle_spawn.treadle_angle_radians) < 0.01 &&
                treadle_spawn.valve_open_fraction == 0.0,
            "the treadle must rest against its stop with the valve shut -- a control that is not "
            "being stood on grants nothing (Governing Law 24)");
    require(treadle.advance_frame(8.0).accepted, "treadle-held interval must be accepted");
    const auto treadle_held = treadle.snapshot();
    require(treadle_held.player_grounded &&
                treadle_held.support_entity_id == Simulation::kTreadleEntityId,
            "the player must actually be standing on the treadle, not beside it");
    require(treadle_held.treadle_angle_radians > 0.05,
            "an 85 kg body must visibly swing the treadle against its counterweight -- this is "
            "the control that is built to a human scale, unlike the 900 kg tipper");
    require(treadle_held.valve_open_fraction > 0.5,
            "standing on the treadle must haul the cable and open the real valve");
    require(treadle_held.piston_force_n > 0.0,
            "the player-opened valve must actually drive the real piston");
    require(treadle_held.lift_platform_position.y > 6.7 &&
                treadle_held.lift_platform_position.y < 8.3,
            "holding the treadle must park the lift inside the catwalk mantle band, turning a "
            "fractional-second window in the autonomous cycle into a standing route");
    require(treadle_held.vessel_available_energy_j < idle_eight.vessel_available_energy_j,
            "the held-open valve must be spending a real reservoir, not conjuring lift -- the "
            "store must be measurably lower than the untouched plant's at the same tick "
            "(Governing Law 24)");

    // Stepping off spends the capability: the treadle returns under its own
    // counterweight, the valve shuts, and the lift bleeds back down. The window
    // this leaves is the ascent -- and it closes.
    require(treadle.set_facing(0.0, 1.0), "treadle step-off facing must be accepted");
    require(treadle.set_move_input(0.0, 1.0), "treadle step-off input must be accepted");
    require(advance_until(treadle,
                          [](const Snapshot &state) {
                              return state.support_entity_id != Simulation::kTreadleEntityId;
                          },
                          3.0),
            "the player must be able to step off the treadle along the catwalk");
    require(advance_until(treadle,
                          [](const Snapshot &state) { return state.valve_open_fraction == 0.0; },
                          4.0),
            "with nobody on it the treadle must return under its own counterweight and shut the "
            "valve -- the player holds this open, nothing latches it");
    const auto treadle_released = treadle.snapshot();
    require(treadle_released.lift_platform_position.y > 6.7,
            "the lift must still be up when the valve shuts, leaving a real window to cross");
    require(advance_until(treadle,
                          [](const Snapshot &state) {
                              return state.lift_platform_position.y < 6.7;
                          },
                          8.0),
            "and that window must close on its own -- a spent store must not hold the lift up "
            "for free");

    std::cout << "PASS scraperx_sim player is the plant's missing component: rest_valve="
              << treadle_spawn.valve_open_fraction
              << " held_treadle=" << treadle_held.treadle_angle_radians
              << " held_valve=" << treadle_held.valve_open_fraction
              << " held_lift=" << treadle_held.lift_platform_position.y
              << "m idle_lift=" << idle_eight.lift_platform_position.y
              << "m store_spent_MJ="
              << (idle_eight.vessel_available_energy_j - treadle_held.vessel_available_energy_j) /
                     1.0e6
              << '\n';

    std::cout << "PASS scraperx_sim first full causal chain: gate_closed_at_rest="
              << int(!chain_rest.ledge_available) << " lift_at_gate=" << chain_gated.lift_platform_position.y
              << "m rise=" << chain_gated.ledge_rise_meters
              << "m reached_catwalk="
              << int(chain_reached.support_entity_id == Simulation::kCatwalkEntityId)
              << " commits=" << chain_committed.checkpoint_commit_count << '\n';

    std::cout << "PASS scraperx_sim coupled machine: valve=" << first_cycle.peak_valve_fraction
              << " piston=" << first_cycle.peak_piston_force
              << "N lift=" << first_cycle.peak_lift_height
              << "m second_lift=" << second_cycle.peak_lift_height
              << "m starved_energy=" << starved_final.vessel_available_energy_j
              << "J player_valve=" << disturbed_result.valve_open_fraction
              << " tower_m=" << Simulation::kTowerHeightMeters << '\n';
    std::cout << "PASS scraperx_sim fall/parachute/checkpoint: lethal_impact="
              << lethal_result.last_impact_speed_mps
              << " chuted_impact=" << chuted_result.last_impact_speed_mps
              << " late_chute_deaths=" << late_chute.snapshot().death_count
              << " grounded_deploy_blocked="
              << int(!grounded_parachute.snapshot().parachute_deployed)
              << " commits=" << airborne_checkpoint.checkpoint_commit_count
              << " machine_restored="
              << int(std::abs(post_death.ballast_position.y - seeded_ballast_y) < 0.1)
              << " deaths=" << post_death.death_count << '\n';

    // ---- WO-011 first freight: KX-JIB / KX-CRATE (Ascent Atlas v1.0 kernel) --
    // A pendant-controlled crane, not an autonomous cycle: Drive/Raise/Lower/
    // Brake through a real, finite-torque/force Jolt constraint motor, and
    // only while the player is at the station.
    Simulation jib(InitialSpawn::KernelJibStation, scraperx::sim::WorldContent::RegressionFixtures);
    require(jib.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "jib settle tick must advance");
    const auto jib_idle = jib.snapshot();
    require(jib_idle.jib_station_active, "the player must be recognised at the pendant station");
    require(std::abs(jib_idle.jib_hook_linear_velocity.y) < 0.05,
            "with no command issued the hook must hold, not drift under gravity or the motor");

    require(jib.set_jib_hoist_input(1.0), "raise command must be accepted");
    require(jib.advance_frame(4.5).accepted, "hoist interval must be accepted");
    const auto jib_raised = jib.snapshot();
    require(jib_raised.jib_hook_position.y > 4.3,
            "a sustained raise command must genuinely lift the hook, through the finite-force "
            "motor, not teleport it to a solved pose");
    require(std::abs(jib_raised.jib_hook_linear_velocity.y) < 0.1,
            "the hoist must stop at its real travel limit -- a finite mechanism, not an infinite "
            "winch (WO-005 forbidden shortcuts)");
    require(jib_raised.jib_crate_position.y > 3.5,
            "the crate must rise with the hook through the real hook-to-crate pin, not be left "
            "behind");

    require(jib.set_jib_hoist_input(0.0), "brake command must be accepted");
    const double held_hook_y = jib_raised.jib_hook_position.y;
    require(jib.advance_frame(2.0).accepted, "brake-hold interval must be accepted");
    const auto jib_braked = jib.snapshot();
    require(std::abs(jib_braked.jib_hook_position.y - held_hook_y) < 0.1,
            "a locked brake must hold position, not keep lifting -- the brake is not a second, "
            "unbounded power source (WO-005 forbidden shortcuts: 'unlimited winch force')");

    require(jib.set_jib_slew_input(1.0), "slew command must be accepted");
    require(jib.advance_frame(5.0).accepted, "slew interval must be accepted");
    const auto jib_slewed = jib.snapshot();
    require(jib_slewed.jib_boom_angle_radians > 1.9,
            "a sustained drive command must genuinely slew the boom to its real limit");
    require(std::abs(jib_slewed.jib_crate_position.x - jib_slewed.jib_hook_position.x) < 0.5 &&
                std::abs(jib_slewed.jib_crate_position.z - jib_slewed.jib_hook_position.z) < 0.5,
            "the crate must swing with the boom, still pinned under the hook, not left orbiting "
            "the old tip position");
    require(std::abs(jib_slewed.jib_crate_position.x - 206.0) > 5.0,
            "the slew must have actually carried the load somewhere new, not merely reported an "
            "angle");

    // Bring the hook back off its travel limit before the next check -- proving
    // "leaving the station stops further lift" needs real headroom to climb
    // into, otherwise a hook already pinned at its hard stop would pass by
    // accident (no room left to reveal a leak either way).
    require(jib.set_jib_hoist_input(-1.0), "lower command must be accepted");
    require(jib.advance_frame(1.5).accepted, "lower interval must be accepted");
    const double midtravel_hook_y = jib.snapshot().jib_hook_position.y;
    require(midtravel_hook_y > 2.0 && midtravel_hook_y < jib_raised.jib_hook_position.y - 0.5,
            "the lower command must move the hook measurably clear of its raised limit");

    // Walking off the station must brake the jib even mid-command -- controls
    // are local, not a standing order the machine keeps obeying unattended.
    // The hook is allowed to keep rising for the real transit time it takes
    // to clear the station radius (a few tenths of a second at the rated
    // hoist rate) -- what must stop is any *further* climb once genuinely off
    // station, which is the comparison below.
    require(jib.set_jib_hoist_input(1.0), "walk-off hoist command must be accepted");
    require(jib.set_move_input(0.0, -1.0), "walk-off move input must be accepted");
    require(advance_until(jib, [](const Snapshot &state) { return !state.jib_station_active; },
                          3.0),
            "the player must be able to walk clear of the station radius");
    const double hook_y_at_exit = jib.snapshot().jib_hook_position.y;
    require(jib.advance_frame(1.0).accepted, "off-station settle interval must be accepted");
    const auto jib_off_station = jib.snapshot();
    require(!jib_off_station.jib_station_active, "the player must still be off the station");
    require(jib_off_station.jib_hook_position.y < hook_y_at_exit + 0.1,
            "once genuinely off station, the hoist must not keep climbing on a stale command");
    require(std::abs(jib_off_station.jib_hook_linear_velocity.y) < 0.05,
            "the hoist must settle to rest once off station, not keep drifting");

    // KX-CRATE is a real moving support (WO-005 proof path item 3, WO-002
    // law) -- proven directly, not inferred from the mechanism above.
    Simulation rider(InitialSpawn::KernelCrateTop, scraperx::sim::WorldContent::RegressionFixtures);
    require(rider.advance_frame(1.0).accepted, "crate-rider settle interval must be accepted");
    const auto riding = rider.snapshot();
    require(riding.player_grounded && riding.support_entity_id == Simulation::kCrateEntityId,
            "the player must be able to stand on the crate as a real support");

    // The capacity-proving stand: same rated force as the jib's winch,
    // permanently overweight, continuously commanded to raise. If it ever
    // rises, the rated force is not real.
    Simulation capacity(InitialSpawn::KernelJibStation, scraperx::sim::WorldContent::RegressionFixtures);
    const double stand_start_y = capacity.snapshot().jib_capacity_stand_load_position.y;
    require(capacity.advance_frame(6.0).accepted, "capacity-stand interval must be accepted");
    const auto stand_after = capacity.snapshot();
    require(std::abs(stand_after.jib_capacity_stand_load_position.y - stand_start_y) < 0.01,
            "a load past the rated winch force must never rise -- 'unlimited winch force' stays "
            "forbidden whether or not the player is watching");

    std::cout << "PASS scraperx_sim first freight: station=" << int(jib_idle.jib_station_active)
              << " raised_y=" << jib_raised.jib_hook_position.y
              << " braked_delta=" << (jib_braked.jib_hook_position.y - held_hook_y)
              << " slewed_rad=" << jib_slewed.jib_boom_angle_radians
              << " crate_carried=" << int(std::abs(jib_slewed.jib_crate_position.x - 206.0) > 5.0)
              << " rides_crate=" << int(riding.support_entity_id == Simulation::kCrateEntityId)
              << " capacity_held=" << int(std::abs(stand_after.jib_capacity_stand_load_position.y -
                                                    stand_start_y) < 0.01)
              << '\n';

    // ---- WO-012 first structural coupling: KX-NEEDLE / KX-POCKETS (Ascent --
    // ---- Atlas v1.0 kernel) --------------------------------------------------
    // Geometry below is hardcoded from the design (kNeedle* constants in
    // simulation.cpp), the same convention WO-011's test above already uses
    // for the jib (e.g. 206.0). Approach pier spans x=[193.4, 198.4]; far
    // pier spans x=[201.6, 206.6]; the gap between them is the bay. The
    // pendant station sits on the approach pier at x=197.4, z=-16.0.

    // Unseated: the gap must not be crossable. Walking straight at it from
    // the pendant, with the hoist never touched, must not deliver a player
    // standing at pier-top height on the far side.
    Simulation gap(InitialSpawn::KernelNeedleStation, scraperx::sim::WorldContent::RegressionFixtures);
    require(gap.set_facing(1.0, 0.0), "gap approach facing must be accepted");
    require(gap.set_move_input(1.0, 0.0), "gap approach walk command must be accepted");
    require(gap.advance_frame(6.0).accepted, "gap crossing attempt interval must be accepted");
    const auto gap_result = gap.snapshot();
    require(gap_result.player_position.x > 197.9,
            "the walk command must have actually moved the player toward the gap, or the next "
            "check proves nothing");
    require(gap_result.player_position.y < 3.0,
            "an unseated needle must leave the bay a gap -- walking at it must drop the player, "
            "not deliver them to pier-top height on the far side (WO-006: 'player cannot cross "
            "the bay')");

    // Seat it: a sustained lower command from the pendant must settle the
    // beam into both pockets and flip the structural predicate.
    Simulation crossing(InitialSpawn::KernelNeedleStation, scraperx::sim::WorldContent::RegressionFixtures);
    require(crossing.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "needle settle tick must advance");
    const auto needle_idle = crossing.snapshot();
    require(needle_idle.needle_station_active,
            "the player must be recognised at the needle pendant station");
    require(!needle_idle.needle_seated, "the needle must start unseated");

    require(crossing.set_needle_hoist_input(-1.0), "lower command must be accepted");
    require(crossing.advance_frame(6.0).accepted, "needle lower interval must be accepted");
    const auto needle_seated_state = crossing.snapshot();
    require(needle_seated_state.needle_seated,
            "a sustained lower command must seat the needle once it settles at the pockets -- "
            "structure must accept the seat as a real topology change, not just report a "
            "position (WO-006: 'neither side independently invents seated vs broken')");
    require(std::abs(needle_seated_state.needle_linear_velocity.y) < 0.05,
            "a seated needle must be at rest, held by real pocket constraints, not still "
            "settling under a motor that is still driving it");

    // Seated: the player must be able to walk it, with the beam itself
    // registering as the support while they are over the gap -- not an
    // invisible walkbox, the actual seated body.
    require(crossing.set_facing(1.0, 0.0), "crossing facing must be accepted");
    require(crossing.set_move_input(1.0, 0.0), "crossing walk command must be accepted");
    require(advance_until(
                crossing, [](const Snapshot &state) { return state.player_position.x > 199.5; },
                3.0),
            "the player must be able to walk forward off the approach pier onto the seated "
            "needle");
    const auto mid_crossing = crossing.snapshot();
    require(mid_crossing.player_position.x < 201.6,
            "the mid-crossing check must land while still over the gap, not already on the far "
            "pier, or it proves nothing about the needle");
    require(mid_crossing.player_grounded &&
                mid_crossing.support_entity_id == Simulation::kNeedleBeamEntityId,
            "the seated needle must be a real, walkable support while the player is over the "
            "gap -- seated changed the traversal predicate (WO-006 required causal path)");
    require(advance_until(
                crossing, [](const Snapshot &state) { return state.player_position.x > 201.6; },
                3.0),
            "once seated the player must be able to walk the needle across the gap to the far "
            "pier");

    // Unseat: WO-006 proof path item 2, "if a legal unseat path exists" --
    // this one does (a sustained raise command from the pendant), so it must
    // actually remove the support it granted.
    Simulation unseat_check(InitialSpawn::KernelNeedleStation, scraperx::sim::WorldContent::RegressionFixtures);
    require(unseat_check.set_needle_hoist_input(-1.0), "lower command must be accepted");
    require(unseat_check.advance_frame(6.0).accepted, "needle lower interval must be accepted");
    require(unseat_check.snapshot().needle_seated,
            "the needle must be seated before the unseat path can be meaningfully exercised");
    require(unseat_check.set_needle_hoist_input(1.0),
            "sustained raise (unseat) command must be accepted");
    require(unseat_check.advance_frame(2.0).accepted, "unseat interval must be accepted");
    const auto unseated = unseat_check.snapshot();
    require(!unseated.needle_seated,
            "a sustained raise command while seated must unseat the needle -- pulling the "
            "pockets pins first, then lifting clear through the same real motor, not a flag "
            "flip (WO-006 proof path item 2)");
    require(unseated.needle_position.y > 4.3,
            "once unseated the beam must actually rise through the same finite-force motor now "
            "free to move it, not merely stop being flagged as support");

    // Checkpoint capture (not restore -- see WO-012's own record for why a
    // real lethal-fall restore is not reachable from the kernel in this WO):
    // commit_machine_checkpoint runs every grounded tick regardless of death,
    // so a seated needle's checkpoint fields must already read back seated
    // immediately, proving the capture side of restore_from_checkpoint's
    // topology reconciliation is exercised, even though the restore side
    // is not end-to-end falsified here.
    Simulation checkpoint_capture(InitialSpawn::KernelNeedleStation, scraperx::sim::WorldContent::RegressionFixtures);
    require(checkpoint_capture.set_needle_hoist_input(-1.0), "capture-check lower must be accepted");
    require(checkpoint_capture.advance_frame(6.0).accepted, "capture-check lower must advance");
    const auto capture_state = checkpoint_capture.snapshot();
    require(capture_state.needle_seated && capture_state.checkpoint_commit_count > 0,
            "a seated needle must be captured by the ordinary grounded-tick checkpoint commit, "
            "the same path restore_from_checkpoint reads back from");

    std::cout << "PASS scraperx_sim first structural coupling: crossable_unseated="
              << int(gap_result.player_position.y >= 3.0)
              << " seated=" << int(needle_seated_state.needle_seated)
              << " support_over_gap="
              << int(mid_crossing.support_entity_id == Simulation::kNeedleBeamEntityId)
              << " reached_far_pier=" << int(crossing.snapshot().player_position.x > 201.6)
              << " unseat_removed_support=" << int(!unseated.needle_seated) << '\n';

    // ---- WO-013 first process coupling: KX-SUMP / KX-GRATE (Ascent Atlas --
    // ---- v1.0 kernel) --------------------------------------------------------
    // Geometry hardcoded from the design (kSump* constants in simulation.cpp),
    // the same convention used above. Approach decking spans x=[195.5,198.5];
    // the grate spans x=[198.5,201.5]; far decking spans x=[201.5,204.5]. The
    // valve station sits on the approach decking at x=197.0, z=16.0.

    // A toggle request away from the station must have no effect -- isolation
    // is only legal "at the real station" (WO-006/007 text).
    Simulation away(InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::RegressionFixtures);
    require(away.request_valve_toggle(), "off-station toggle request must be accepted as a command");
    require(away.advance_frame(1.0).accepted, "off-station settle interval must be accepted");
    require(!away.snapshot().sump_isolated,
            "a valve toggle must be a no-op anywhere but the real station, exactly like the jib "
            "and needle pendants -- the far side of the map must not be able to touch it");

    // Wet (the default): the grate must not be a crossable support. Walking
    // straight at it from the station, with the valve never touched, must
    // drop the player through rather than deliver them to the far decking.
    Simulation wet(InitialSpawn::KernelSumpStation, scraperx::sim::WorldContent::RegressionFixtures);
    require(wet.advance_frame(Simulation::kFixedStepSeconds).accepted, "sump settle tick must advance");
    const auto sump_idle = wet.snapshot();
    require(sump_idle.sump_station_active,
            "the player must be recognised at the sump valve station");
    require(!sump_idle.sump_isolated && !sump_idle.grate_safe,
            "the sump must start wet and open -- 'wet sump makes KX-GRATE a hazard' is the "
            "default, existing-truth state, not something a test has to induce");
    require(wet.set_facing(1.0, 0.0), "wet-crossing facing must be accepted");
    require(wet.set_move_input(1.0, 0.0), "wet-crossing walk command must be accepted");
    require(wet.advance_frame(6.0).accepted, "wet-crossing attempt interval must be accepted");
    const auto wet_result = wet.snapshot();
    require(wet_result.player_position.x > 197.5,
            "the walk command must have actually moved the player toward the grate, or the next "
            "check proves nothing");
    require(wet_result.player_position.y < 2.0, // platform top is 3.0 m; well below it is "fell through".
            "a wet grate must not be a valid support -- walking onto it must drop the player "
            "through to the deck below, not deliver them to platform height on the far side "
            "(WO-007 forbidden shortcut: 'wet decal over an always-solid grate')");

    // Isolate and drain: a sustained toggle-and-wait at the station must
    // flip the derived predicate once the lumped volume actually reaches
    // zero -- not on a timer independent of that inventory.
    Simulation dry(InitialSpawn::KernelSumpStation, scraperx::sim::WorldContent::RegressionFixtures);
    require(dry.request_valve_toggle(), "isolate command must be accepted");
    require(dry.advance_frame(Simulation::kFixedStepSeconds).accepted, "isolate tick must advance");
    require(dry.snapshot().sump_isolated, "the valve toggle must close the isolation edge");
    require(dry.advance_frame(13.0).accepted, "drain interval must be accepted");
    const auto dry_state = dry.snapshot();
    require(dry_state.grate_safe && dry_state.sump_volume_kg <= 0.0,
            "isolating the supply must let the real drain sink actually empty the volume, not "
            "just flip a flag -- the predicate must follow the inventory to zero (WO-007 "
            "forbidden shortcut: 'timer that dries the grate without inventory')");

    // Safe: the player must be able to walk it, with the grate itself
    // registering as the support while they are over the former hazard.
    require(dry.set_facing(1.0, 0.0), "dry-crossing facing must be accepted");
    require(dry.set_move_input(1.0, 0.0), "dry-crossing walk command must be accepted");
    require(advance_until(
                dry, [](const Snapshot &state) { return state.player_position.x > 200.0; }, 3.0),
            "the player must be able to walk forward off the approach decking onto the safe "
            "grate");
    const auto mid_grate = dry.snapshot();
    require(mid_grate.player_position.x < 201.5,
            "the mid-crossing check must land while still over the grate span, not already on "
            "the far decking, or it proves nothing about the grate itself");
    require(mid_grate.player_grounded && mid_grate.support_entity_id == Simulation::kSumpGrateEntityId,
            "the safe grate must be the real support while the player is over it -- the process "
            "state is the cause of the changed support (WO-007 completion criterion)");
    require(advance_until(
                dry, [](const Snapshot &state) { return state.player_position.x > 201.5; }, 3.0),
            "once safe the player must be able to walk the grate to the far decking");

    // Dump: reopening the valve must make it unsafe again -- the same
    // reversible predicate, not a one-way flag (WO-007 proof path: "dump/
    // fail => unsafe"). A separate instance, staying at the station the
    // whole time: the crossing above ends off-station, and (correctly,
    // matching the jib/needle pendants) a toggle only takes effect there.
    Simulation dump_check(InitialSpawn::KernelSumpStation, scraperx::sim::WorldContent::RegressionFixtures);
    require(dump_check.request_valve_toggle(), "isolate command must be accepted");
    require(dump_check.advance_frame(13.0).accepted, "drain interval must be accepted");
    require(dump_check.snapshot().grate_safe, "the sump must be drained before the dump path can "
                                              "be meaningfully exercised");
    require(dump_check.request_valve_toggle(), "reopen (dump) command must be accepted");
    require(dump_check.advance_frame(0.5).accepted, "dump interval must be accepted");
    const auto dumped = dump_check.snapshot();
    require(!dumped.sump_isolated && dumped.sump_volume_kg > 0.0 && !dumped.grate_safe,
            "reopening the valve must let real inflow refill the volume and flip the predicate "
            "back to unsafe -- a reversible process, not a one-shot 'sump_clear' flag");

    std::cout << "PASS scraperx_sim first process coupling: gated_off_station="
              << int(!away.snapshot().sump_isolated) << " wet_crossable="
              << int(wet_result.player_position.y >= 2.0)
              << " drained_safe=" << int(dry_state.grate_safe)
              << " support_on_grate="
              << int(mid_grate.support_entity_id == Simulation::kSumpGrateEntityId)
              << " reached_far_deck=" << int(dry.snapshot().player_position.x > 201.5)
              << " dump_unsafe_again=" << int(!dumped.grate_safe) << '\n';

    // ---- AS-001: B00 intake rise, apron to +24 m -------------------------
    // The first campaign slice. Atlas band B00, chain K0. The claim under test
    // is the causal path itself: MOD-STAIR-A is shut because 4 t of freight is
    // physically standing in MOD-DOG-A's swing, and it opens because the jib
    // moved that freight -- not because a predicate flipped.
    //
    // Geometry literals below are read off the kIntake* constants in
    // simulation.cpp. The bay's throat is at (0, -110.5); MOD-STAIR-A
    // switchbacks in two lanes at z = -116 and z = -120 between x = -7 and
    // x = +7, landings at x = +-8; the +24 m handoff deck is at x = -6,
    // z = -117.5 .. -107.7, walking surface 24.19 m.
    constexpr double kThroatX = 0.0;
    constexpr double kThroatZ = -110.5;
    // The bay's front wall spans z = -110.8 .. -110.2. A 0.35 m capsule has
    // therefore not cleared it until its centre passes -111.15, so anything
    // south of -111.2 is still outside the bay.
    constexpr double kBayInteriorZ = -111.2;
    constexpr double kStairLandingX = 7.5;
    // Player capsule standing half-height is 0.90 m, so a stable stand on the
    // 24.19 m walking surface puts the capsule centre near 25.09 m.
    constexpr double kHandoffSurfaceY = 24.19;

    // 1. Pinned: the throat is shut, and walking straight at it does not pass.
    Simulation pinned(InitialSpawn::IntakeThroat, scraperx::sim::WorldContent::RegressionFixtures);
    require(pinned.advance_frame(1.0).accepted, "intake settling interval must be accepted");
    const auto pinned_start = pinned.snapshot();
    require(pinned_start.intake_pack_pins_dog,
            "the 4 t pack must start inside MOD-DOG-A's swing envelope");
    require(!pinned_start.intake_throat_clear,
            "MOD-DOG-A must start blocking the MOD-STAIR-A throat");
    const double pinned_deepest = walk_toward(pinned, kThroatX, kThroatZ - 8.0, 14.0);
    const auto pinned_blocked = pinned.snapshot();
    require(pinned_deepest > kBayInteriorZ,
            "while the pack pins the dog, walking at the throat must not get the player past "
            "the bay wall at any point -- MOD-STAIR-A is physically impassable, not gated");
    require(pinned_blocked.player_position.y < 2.0,
            "a blocked player must still be at grade, not somehow up the stair");
    require(pinned.snapshot().intake_pack_pins_dog,
            "walking into the dog must not shift 4 t of freight");

    // 2. No command surface opens it. Every input the player has, off-station,
    //    hammered at once: none of them is a way past a physical body.
    Simulation forced(InitialSpawn::IntakeThroat, scraperx::sim::WorldContent::RegressionFixtures);
    double forced_deepest = 0.0;
    for (std::uint32_t tick = 0; tick < 1260; ++tick) {
        (void)forced.set_intake_hoist_input(1.0);
        (void)forced.set_intake_slew_input(1.0);
        (void)forced.set_jib_hoist_input(1.0);
        (void)forced.set_needle_hoist_input(1.0);
        (void)forced.request_valve_toggle();
        (void)forced.request_jump();
        (void)forced.request_traversal();
        (void)forced.set_move_input(0.0, -1.0);
        (void)forced.set_facing(0.0, -1.0);
        (void)forced.advance_frame(Simulation::kFixedStepSeconds);
        forced_deepest = std::min(forced_deepest, forced.snapshot().player_position.z);
    }
    const auto forced_state = forced.snapshot();
    require(!forced_state.intake_station_active,
            "the B00 pendant must stay inert while the player is at the throat, 10 m off station");
    require(forced_state.intake_pack_pins_dog && !forced_state.intake_throat_clear,
            "no command issued off-station may move the pack or travel the dog");
    require(forced_deepest > kBayInteriorZ,
            "mashing every input must not produce passage a body is blocking");

    // 3. The rating is real: 9 t under the same rated winch force never rises.
    Simulation rated(InitialSpawn::IntakePendant, scraperx::sim::WorldContent::RegressionFixtures);
    require(rated.advance_frame(20.0).accepted, "overweight interval must be accepted");
    const auto rated_state = rated.snapshot();
    require(rated_state.intake_overweight_pack_position.y < 1.2,
            "9 t exceeds the 49050 N rated winch force and must stall on the stand, however long "
            "it is commanded up");

    // 4. Lift the pack, and the dog travels because nothing is in its way.
    Simulation freight(InitialSpawn::IntakePendant, scraperx::sim::WorldContent::RegressionFixtures);
    require(freight.advance_frame(0.5).accepted, "pendant settling interval must be accepted");
    require(freight.snapshot().intake_station_active,
            "the B00 pendant spawn must be inside CAP-PENDANT's station radius");
    require(freight.set_intake_hoist_input(1.0), "intake hoist command must be accepted");
    require(advance_until(freight,
                          [](const auto &state) { return state.intake_throat_clear; },
                          20.0),
            "raising the pack clear of the dog's swing must let the dog travel and open the "
            "throat");
    const auto opened = freight.snapshot();
    require(!opened.intake_pack_pins_dog,
            "the dog must only travel once the pack has physically left its envelope");
    require(opened.intake_pack_position.y > 2.5,
            "the pack must be lifted clear of the dog, not nudged");
    require(freight.set_intake_hoist_input(0.0), "intake hoist stop must be accepted");

    // 5. Now walk the route the freight opened, all the way to +24 m.
    walk_toward(freight, kThroatX, -107.5, 12.0);
    walk_toward(freight, kThroatX, -113.0, 8.0);
    for (int flight = 0; flight < 6; ++flight) {
        const double side = (flight % 2 == 0) ? 1.0 : -1.0;
        const double lane = -118.0 + side * 2.0;
        walk_toward(freight, -side * kStairLandingX, lane, 8.0);
        walk_toward(freight, side * kStairLandingX, lane, 14.0);
    }
    walk_toward(freight, -6.0, -112.0, 12.0);
    const auto arrived = freight.snapshot();
    require(arrived.support_entity_id == Simulation::kIntakeHandoffEntityId,
            "the player must finish standing on the +24 m handoff deck itself");
    require(arrived.player_grounded &&
                arrived.player_position.y > kHandoffSurfaceY + 0.7,
            "the +24 m handoff must be stable support, not a graze");

    // 6. SKIN is a legal bypass: the same +24 m deck, freight untouched. The
    //    Atlas forbids walling SKIN off to protect the freight sequence.
    Simulation skin(InitialSpawn::IntakeSkinFoot, scraperx::sim::WorldContent::RegressionFixtures);
    require(skin.advance_frame(0.5).accepted, "skin settling interval must be accepted");
    std::uint32_t skin_mantles = 0;
    bool skin_arrived = false;
    for (std::uint32_t tick = 0; tick < 90 * 120; ++tick) {
        const auto state = skin.snapshot();
        if (state.support_entity_id == Simulation::kIntakeHandoffEntityId) {
            skin_arrived = true;
            break;
        }
        (void)skin.set_move_input(0.0, -1.0);
        (void)skin.set_facing(0.0, -1.0);
        const bool ready = (state.traversal_state == TraversalState::None &&
                            state.player_grounded && state.ledge_available) ||
                           state.traversal_state == TraversalState::Hanging;
        if (ready && skin.request_traversal()) {
            ++skin_mantles;
        }
        (void)skin.advance_frame(Simulation::kFixedStepSeconds);
    }
    require(skin_arrived,
            "MOD-SKIN-LADDER-S must be climbable to the same +24 m deck with the freight "
            "sequence untouched");
    const auto skin_state = skin.snapshot();
    require(skin_state.intake_pack_pins_dog && !skin_state.intake_throat_clear,
            "climbing SKIN must not falsely mutate the freight mechanism -- the pack is still "
            "down and the dog is still pinned");
    require(skin_state.intake_pack_position.y < 1.5,
            "the pack must not have moved while the player climbed the facade");

    std::cout << "PASS scraperx_sim B00 intake rise: pinned_impassable="
              << int(pinned_deepest > kBayInteriorZ)
              << " pinned_deepest_z=" << pinned_deepest
              << " forced_impassable=" << int(forced_deepest > kBayInteriorZ)
              << " overweight_y=" << rated_state.intake_overweight_pack_position.y
              << " lifted_y=" << opened.intake_pack_position.y
              << " dog_rad=" << opened.intake_dog_angle_radians
              << " handoff_y=" << arrived.player_position.y
              << " skin_mantles=" << skin_mantles
              << " skin_freight_untouched="
              << int(skin_state.intake_pack_pins_dog) << '\n';

    // ---- AS-002: Legal Forty, first legal stand at +40 m -----------------
    // Atlas band B00's 24-40 m leftover, chain K0 PLAY. The claim under test
    // is the same shape as AS-001's own: MOD-STAIR-A-SWING opens because
    // real rope tension -- read back from the solver, off a real 4 t pack
    // physically seated in MOD-CW-CRADLE -- beats the flight's own stowed-
    // stop demand, not because a flag flipped. Geometry literals below are
    // read off the kLegalForty*/kIntake* constants in simulation.cpp.
    constexpr double kLegalFortyHandoffSurfaceY = 24.1872;
    constexpr double kLegalFortyMidLandingSurfaceY = 32.1872;
    constexpr double kLegalFortyHallDeckSurfaceY = 40.1872;
    constexpr double kLegalFortyHingeX = 7.856;
    constexpr double kLegalFortyHingeZ = -112.5;
    constexpr double kLegalFortyMidLandingX = 9.350;
    constexpr double kLegalFortyMidLandingZ = -115.80;
    constexpr double kLegalFortyWellX = -4.51;
    constexpr double kLegalFortyUpperFlightZ = -117.0;
    constexpr double kLegalFortyTravelFull = 0.907572; // stowed 8 deg .. deployed 60 deg

    // 1. wo015_unloaded_flight_is_not_a_route: the stowed flight hangs up
    //    near the hinge, nowhere over the deck it will occupy once deployed
    //    -- walking and mantling at that empty footprint gets nowhere.
    Simulation unrouted(InitialSpawn::IntakeHandoffDeck, scraperx::sim::WorldContent::RegressionFixtures);
    require(unrouted.advance_frame(0.5).accepted, "unrouted settling interval must be accepted");
    require(nearly_equal(unrouted.snapshot().legal_forty_swing_travel_radians, 0.0, 1.0e-3),
            "the swing flight must start at its stowed limit");
    double unrouted_deepest_y = unrouted.snapshot().player_position.y;
    bool unrouted_reached_flight = false;
    for (std::uint32_t tick = 0; tick < 90 * 14; ++tick) {
        const auto state = unrouted.snapshot();
        unrouted_deepest_y = std::max(unrouted_deepest_y, state.player_position.y);
        if (state.support_entity_id == Simulation::kIntakeSwingFlightEntityId) {
            unrouted_reached_flight = true;
        }
        (void)unrouted.set_move_input(-1.0, 0.0);
        (void)unrouted.set_facing(-1.0, 0.0);
        const bool ready = (state.traversal_state == TraversalState::None &&
                            state.player_grounded && state.ledge_available) ||
                           state.traversal_state == TraversalState::Hanging;
        if (ready) {
            (void)unrouted.request_traversal();
        }
        (void)unrouted.advance_frame(Simulation::kFixedStepSeconds);
    }
    const auto unrouted_state = unrouted.snapshot();
    require(unrouted_deepest_y < kLegalFortyHandoffSurfaceY + 1.0,
            "the unloaded flight must not be a route -- the deepest y reached at its deployed "
            "footprint must stay at the deck's own height, not the mid-landing's");
    require(!unrouted_reached_flight,
            "support_entity_id must never be the swing flight while it is stowed and unreachable");
    require(nearly_equal(unrouted_state.legal_forty_swing_travel_radians, 0.0, 1.0e-3),
            "walking and mantling at the stowed flight must not travel its hinge");

    // 2. wo015_flag_is_not_a_stair: every command mashed at once, off
    //    station, for the same interval -- no command surface substitutes
    //    for the physical rope tension the hinge actually needs.
    Simulation mashed(InitialSpawn::IntakeHandoffDeck, scraperx::sim::WorldContent::RegressionFixtures);
    for (std::uint32_t tick = 0; tick < 90 * 14; ++tick) {
        (void)mashed.set_intake_hoist_input(-1.0);
        (void)mashed.set_intake_slew_input(1.0);
        (void)mashed.request_intake_sling_release();
        (void)mashed.request_intake_sling_attach();
        (void)mashed.request_jump();
        (void)mashed.request_traversal();
        (void)mashed.set_move_input(-1.0, 0.0);
        (void)mashed.set_facing(-1.0, 0.0);
        (void)mashed.advance_frame(Simulation::kFixedStepSeconds);
    }
    const auto mashed_state = mashed.snapshot();
    require(!mashed_state.intake_station_active,
            "the B00 pendant must stay inert while the player is at the handoff deck, off "
            "station");
    require(nearly_equal(mashed_state.legal_forty_swing_travel_radians, 0.0, 1.0e-3),
            "no command issued off-station may travel the swing hinge");
    require(mashed_state.legal_forty_pack_slung,
            "no command issued off-station may release the sling");

    // 3. wo015_cradle_load_deploys, wo015_deployed_flight_is_support,
    //    wo015_reaches_forty_and_commits: the real causal path, start to
    //    finish -- lift, slew to the cradle bearing, load it, and walk the
    //    route that opens all the way to the hall deck.
    Simulation ascent(InitialSpawn::IntakePendant, scraperx::sim::WorldContent::RegressionFixtures);
    require(ascent.advance_frame(0.5).accepted, "ascent settling interval must be accepted");
    require(ascent.snapshot().intake_station_active,
            "the ascent spawn must be inside CAP-PENDANT's station radius");
    // Matches the verified station sequence exactly: the hoist stays
    // asserted (the winch actively holding the pack up, not a free hang)
    // all the way through the slew and settle, only reversing at Release --
    // found by direct observation that zeroing it straight after lift-clear
    // leaves enough residual swing for the pack to seat off-centre on the
    // cradle, and the mechanism that follows never recovers the margin.
    require(ascent.set_intake_hoist_input(1.0), "ascent hoist-clear command must be accepted");
    require(advance_until(ascent,
                          [](const auto &state) { return state.intake_pack_position.y > 8.0; },
                          20.0),
            "lifting the pack clear must be driven by the real winch, within budget");
    require(ascent.set_intake_slew_input(-1.0), "ascent slew command must be accepted");
    require(advance_until(
                ascent,
                [](const auto &state) { return state.intake_boom_angle_radians <= -0.80; }, 20.0),
            "slewing to the cradle bearing (negative input reaches the +X side the flight's own "
            "hinge shares) must complete within budget");
    require(ascent.set_intake_slew_input(0.0), "ascent slew-stop command must be accepted");
    require(ascent.advance_frame(6.0).accepted, "slew-settle interval must be accepted");
    const double cradle_y_before_load = ascent.snapshot().legal_forty_cradle_position.y;
    require(ascent.set_intake_hoist_input(-1.0), "ascent lower command must be accepted");
    bool ascent_released = false;
    for (std::uint32_t tick = 0; tick < 90 * 45 && !ascent_released; ++tick) {
        (void)ascent.request_intake_sling_release();
        (void)ascent.advance_frame(Simulation::kFixedStepSeconds);
        ascent_released = !ascent.snapshot().legal_forty_pack_slung;
    }
    require(ascent_released,
            "holding Release over the seated pack must free the sling within budget");
    for (int i = 0; i < 40 * 90; ++i) {
        (void)ascent.advance_frame(Simulation::kFixedStepSeconds);
        if (ascent.snapshot().legal_forty_swing_travel_radians >= 0.85) {
            break;
        }
    }
    require(ascent.snapshot().legal_forty_swing_travel_radians >= 0.85,
            "the loaded cradle's real rope tension must swing the hinge past 0.85 rad within "
            "the plan's own 40 s budget");
    const auto deployed = ascent.snapshot();
    require(cradle_y_before_load - deployed.legal_forty_cradle_position.y >= 1.40,
            "the cradle must physically descend at least 1.40 m once loaded");
    require(deployed.legal_forty_swing_travel_radians <= kLegalFortyTravelFull + 1.0e-3,
            "the hinge must never exceed its own deployed limit");
    // The loaded, deployed mechanism is at rest, and stays there: over two
    // full strokes of the B00 belt (2 * 2 pi / 0.40 s) nothing may strike the
    // car, knock the flight off its stop, or walk the seated pack. The plan's
    // 1.20 m-half-height car hung below the belt's top and was rammed once a
    // stroke until the pack fell off.
    {
        require(ascent.advance_frame(3.0).accepted, "deploy-arrival interval must be accepted");
        const auto seated = ascent.snapshot();
        double least_travel = std::numeric_limits<double>::infinity();
        double most_pack_drift = 0.0;
        for (std::uint32_t tick = 0; tick < 90 * 32; ++tick) {
            (void)ascent.advance_frame(Simulation::kFixedStepSeconds);
            const auto held = ascent.snapshot();
            least_travel = std::min(least_travel, held.legal_forty_swing_travel_radians);
            most_pack_drift = std::max(
                most_pack_drift, std::hypot(held.intake_pack_position.x - seated.intake_pack_position.x,
                                            held.intake_pack_position.z - seated.intake_pack_position.z));
        }
        require(least_travel >= 0.90,
                "the loaded flight must hold its deployed stop through two belt strokes");
        require(most_pack_drift < 0.05,
                "the seated pack must not walk on the car while the flight is deployed");
        std::cout << "INFO AS-002 deployed hold: least_travel=" << least_travel
                  << " pack_drift=" << most_pack_drift << '\n';
    }

    // Walk the route the deploy just opened: deck -> up the flight's own
    // incline to the hinge end -> across to the mid-landing -> up the
    // (now-inclined) upper flight -> through the well onto the hall deck.
    walk_toward(ascent, kThroatX, -107.5, 12.0);
    walk_toward(ascent, kThroatX, -113.0, 8.0);
    for (int flight = 0; flight < 6; ++flight) {
        const double side = (flight % 2 == 0) ? 1.0 : -1.0;
        const double lane = -118.0 + side * 2.0;
        walk_toward(ascent, -side * kStairLandingX, lane, 8.0);
        walk_toward(ascent, side * kStairLandingX, lane, 14.0);
    }
    // Lands west of the flight's own reach (it spans x in [-6, 7.856] once
    // deployed) and clear of its z in [-113.4, -111.6] -- clear of it on
    // both axes, unlike the freight braid's own final waypoint (-6, -112),
    // which the flight now physically occupies. Arrives and stops 1.3 m
    // inside the deck's north edge: a full stick dithering about (-9, -108),
    // 0.3 m inside it, walked the body off the deck -- a lethal 24 m fall
    // this run never checked for (found when AS-003 carried a load here).
    require(walk_to(ascent, -9.0, -109.0, 12.0),
            "the ascent must arrive on the +24 m handoff deck clear of its north edge");
    const auto on_deck = ascent.snapshot();
    require(on_deck.support_entity_id == Simulation::kIntakeHandoffEntityId,
            "the ascent must reach the +24 m handoff deck exactly as the freight braid does");
    // Aims past the flight's own foot rather than straight at it: the foot
    // sits only 0.06-0.25 m above the deck (by design, a walked step, not a
    // mantled one), and that is close enough that which of the two a
    // capsule ends up registering as support is not always the same run to
    // run -- found by direct observation that landing on the deck's own
    // side of that razor-thin step, even briefly, lets a capsule walk the
    // deck's flat surface all the way to its east edge and off it, never
    // climbing at all. x = -4.0 is unambiguous: the flight's own surface is
    // already 1.15 m above the deck there, well outside any such margin,
    // while still short of the deck's own east edge (-2.0).
    walk_toward(ascent, -6.0, -112.5, 25.0);
    for (int i = 0; i < 20 * 90; ++i) {
        const auto state = ascent.snapshot();
        double dx = kLegalFortyHingeX - state.player_position.x;
        double dz = kLegalFortyHingeZ - state.player_position.z;
        const double len = std::hypot(dx, dz);
        if (len > 1.0e-6) {
            dx /= len;
            dz /= len;
        }
        (void)ascent.set_move_input(dx, dz);
        (void)ascent.set_facing(dx, dz);
        (void)ascent.advance_frame(Simulation::kFixedStepSeconds);
    }
    // Crosses the hinge-to-landing gap at the hinge's own z first, staying
    // centred on the flight's own (narrow, 1.80 m) width the whole way --
    // found by direct observation that aiming straight at the landing's own
    // centre from the flight walks off its north edge diagonally, into the
    // gap, before reaching far enough east to be over the landing at all.
    walk_toward(ascent, kLegalFortyMidLandingX - 0.85, kLegalFortyHingeZ, 10.0);
    walk_toward(ascent, kLegalFortyMidLandingX, kLegalFortyMidLandingZ, 10.0);
    const auto mid_landing_state = ascent.snapshot();
    require(mid_landing_state.player_grounded &&
                mid_landing_state.player_position.y >= kLegalFortyMidLandingSurfaceY + 0.70,
            "the deployed flight must carry the player to the mid-landing at y >= 32.89");
    // Aligns to the upper flight's own (narrow, 1.80 m) z-band while still
    // on the wide mid-landing, for the same reason as the hinge crossing
    // above: the landing's z in [-121.0, -110.6] does not fully overlap
    // the flight's own z in [-117.9, -116.1], and a diagonal from the
    // landing's centre (z = -114.80) reaches the landing's west edge
    // before it reaches the flight's own band, and falls through the gap
    // between them -- found by direct observation.
    walk_toward(ascent, kLegalFortyMidLandingX, kLegalFortyUpperFlightZ, 10.0);
    // Walks onto the hall deck itself and stops steering the instant it
    // does, rather than continuing to drive into the well's own edge --
    // found by direct observation that a capsule still being steered past
    // its target there jitters between the flight, the deck and open air
    // instead of settling on either.
    bool reached_hall_deck = false;
    for (int i = 0; i < 40 * 90 && !reached_hall_deck; ++i) {
        const auto state = ascent.snapshot();
        double dx = kLegalFortyWellX - state.player_position.x;
        double dz = kLegalFortyUpperFlightZ - state.player_position.z;
        const double len = std::hypot(dx, dz);
        if (len > 1.0e-6) {
            dx /= len;
            dz /= len;
        }
        (void)ascent.set_move_input(dx, dz);
        (void)ascent.set_facing(dx, dz);
        (void)ascent.advance_frame(Simulation::kFixedStepSeconds);
        reached_hall_deck =
            ascent.snapshot().support_entity_id == Simulation::kIntakeHallDeckEntityId;
    }
    require(reached_hall_deck,
            "the route must reach MOD-HALL-DECK itself, at the top of the upper flight, within "
            "budget");
    require(ascent.set_move_input(0.0, 0.0), "ascent settle-on-forty input must be accepted");
    require(ascent.advance_frame(2.0).accepted, "settle-on-forty interval must be accepted");
    const auto on_forty = ascent.snapshot();
    require(on_forty.support_entity_id == Simulation::kIntakeHallDeckEntityId,
            "the route must finish on MOD-HALL-DECK itself, at the top of the upper flight");
    require(on_forty.player_position.y >= kLegalFortyHallDeckSurfaceY + 0.70,
            "the hall deck must be stable support at y >= 40.89, not a graze");
    require(on_forty.death_count == 0,
            "the route from the apron to the hall deck must be walked without a lethal fall");
    require(on_forty.checkpoint_commit_count > 0 && on_forty.checkpoint_position.y >= 40.0,
            "standing on the hall deck must commit a checkpoint at y >= 40.0 through the "
            "existing automatic commit path, with no new machinery");

    // 4. wo015_unload_retracts: a fresh station-side instance -- the hinge
    //    relaxes back to its stowed limit once the cradle is unloaded again,
    //    reversible rather than a one-way flag.
    Simulation retract(InitialSpawn::IntakePendant, scraperx::sim::WorldContent::RegressionFixtures);
    require(retract.advance_frame(0.5).accepted, "retract settling interval must be accepted");
    require(retract.set_intake_hoist_input(1.0), "retract hoist-clear command must be accepted");
    require(advance_until(retract,
                          [](const auto &state) { return state.intake_pack_position.y > 8.0; },
                          20.0),
            "retract's own lift-clear must complete within budget");
    require(retract.set_intake_slew_input(-1.0), "retract slew command must be accepted");
    require(advance_until(
                retract,
                [](const auto &state) { return state.intake_boom_angle_radians <= -0.80; }, 20.0),
            "retract's own slew to bearing must complete within budget");
    require(retract.set_intake_slew_input(0.0), "retract slew-stop command must be accepted");
    require(retract.advance_frame(6.0).accepted, "retract slew-settle interval must be accepted");
    require(retract.set_intake_hoist_input(-1.0), "retract lower command must be accepted");
    bool retract_released = false;
    for (std::uint32_t tick = 0; tick < 90 * 45 && !retract_released; ++tick) {
        (void)retract.request_intake_sling_release();
        (void)retract.advance_frame(Simulation::kFixedStepSeconds);
        retract_released = !retract.snapshot().legal_forty_pack_slung;
    }
    require(retract_released, "retract's own release must free the sling within budget");
    require(advance_until(retract,
                          [](const auto &state) {
                              return state.legal_forty_swing_travel_radians >= 0.85;
                          },
                          40.0),
            "retract's own deploy must reach the same 0.85 rad within budget before unloading");
    // Lets the deploy actually settle at its own hinge limit before
    // unloading, rather than interrupting it mid-swing -- found by direct
    // observation that unslinging right as travel first crosses 0.85 (while
    // still accelerating toward the limit) leaves enough residual angular
    // velocity that the flight runs on to, and then sticks at, the
    // deployed stop regardless of the now-unloaded cradle.
    require(advance_until(retract,
                          [](const auto &state) {
                              return state.legal_forty_swing_travel_radians >= 0.90;
                          },
                          10.0),
            "retract's own deploy must settle past 0.90 rad before unloading");
    require(retract.advance_frame(20.0).accepted, "post-deploy settle interval must be accepted");
    bool reattached = false;
    for (std::uint32_t tick = 0; tick < 90 * 30 && !reattached; ++tick) {
        (void)retract.request_intake_sling_attach();
        (void)retract.advance_frame(Simulation::kFixedStepSeconds);
        reattached = retract.snapshot().legal_forty_pack_slung;
    }
    require(reattached, "holding Attach over the seated hook must re-sling the pack within "
                        "budget");
    require(retract.set_intake_hoist_input(1.0), "retract lift-clear-again command must be "
                                                  "accepted");
    for (int i = 0; i < 30 * 90; ++i) {
        (void)retract.advance_frame(Simulation::kFixedStepSeconds);
        if (retract.snapshot().intake_pack_position.y > 6.0) {
            break;
        }
    }
    require(retract.snapshot().intake_pack_position.y > 6.0,
            "lifting the pack clear of the cradle again must complete within budget");
    for (int i = 0; i < 30 * 90; ++i) {
        (void)retract.advance_frame(Simulation::kFixedStepSeconds);
        if (retract.snapshot().legal_forty_swing_travel_radians <= 0.20) {
            break;
        }
    }
    require(retract.snapshot().legal_forty_swing_travel_radians <= 0.20,
            "unloading the cradle must let the flight's own weight relax the hinge back to "
            "<= 0.20 rad, reversible rather than a one-way flag");

    // 5. wo015_skin_skips_the_freight: MOD-SKIN-LADDER-S is still the
    //    always-legal bypass -- now reaching the hall deck via the
    //    mid-landing walkway and the (always-present) static upper flight,
    //    with the freight sequence and the swing hinge both untouched.
    Simulation skin_forty(InitialSpawn::IntakeSkinFoot, scraperx::sim::WorldContent::RegressionFixtures);
    require(skin_forty.advance_frame(0.5).accepted, "skin-forty settling interval must be "
                                                     "accepted");
    // IntakeSkinFoot spawns at the ladder's own base, so this climbs the
    // WHOLE ladder -- all 20 rungs, not just this continuation's 5 (16-20)
    // -- one mantle apiece, exactly like B00's own skin_mantles=15 climb of
    // rungs 1-15 alone above. Found by direct observation: counting only 5
    // stopped this loop at rung 5 (y ~= 7.3, near the ladder's own lower-
    // middle section), and the walk_toward calls meant for the TOP of the
    // ladder then steered the player sideways off it into open air there.
    //
    // Unlike the B00 climb, whose target (the +24 m handoff deck) sits at
    // the rungs' own x = kIntakeSkinCenterX = kIntakeHandoffCenterX, the
    // walkway this continuation lands on is offset east of the rung column
    // (x in [-5.0, 8.30] against the rungs' own x in [-7.0, -5.0]) -- so
    // climbing stops the instant all 20 rungs are mantled, by count, rather
    // than waiting on a support match that continued (0, -1) steering can
    // never produce.
    constexpr std::uint32_t kSkinFortyRungCount = 20;
    std::uint32_t skin_forty_mantles = 0;
    for (std::uint32_t tick = 0; tick < 90 * 150 && skin_forty_mantles < kSkinFortyRungCount;
         ++tick) {
        const auto state = skin_forty.snapshot();
        (void)skin_forty.set_move_input(0.0, -1.0);
        (void)skin_forty.set_facing(0.0, -1.0);
        const bool ready = (state.traversal_state == TraversalState::None &&
                            state.player_grounded && state.ledge_available) ||
                           state.traversal_state == TraversalState::Hanging;
        if (ready && skin_forty.request_traversal()) {
            ++skin_forty_mantles;
        }
        (void)skin_forty.advance_frame(Simulation::kFixedStepSeconds);
    }
    require(skin_forty_mantles == kSkinFortyRungCount,
            "MOD-SKIN-LADDER-S's continuation (rungs 16-20) must climb to the mid-landing's "
            "own height with the same rung law as rungs 1-15");
    // The 20th mantle's own rise is still in progress the instant the loop
    // above exits (it only just counted, on this same tick) -- a fixed 0.5 s
    // settle was not long enough to wait it out: found by direct
    // observation, the very next diagnostic checkpoint already showing the
    // player mid-fall (grounded=0) well short of the walkway. Wait for the
    // traversal to actually finish instead.
    bool skin_forty_settled = false;
    for (int i = 0; i < 5 * 90 && !skin_forty_settled; ++i) {
        (void)skin_forty.set_move_input(0.0, 0.0);
        (void)skin_forty.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = skin_forty.snapshot();
        skin_forty_settled =
            state.traversal_state == TraversalState::None && state.player_grounded;
    }
    require(skin_forty_settled,
            "the 20th mantle must finish and settle before the walkway crossing begins");
    // The walkway's lane with standing headroom is z in [-120.65, -118.25]
    // for the capsule's centre: the walkway's own south edge (-121.0) and
    // the upper flight's own south edge (-117.9), each less the 0.35 m
    // radius. North of it the flight's underside falls toward its foot at
    // x = 9.350 and leaves under 2.1 m of headroom past x = 5.7. Rung 20's
    // band alone, the walkway's old footprint, left [-118.65, -118.25]: a
    // 0.4 m lane, reported by a player as an opening too small to fit
    // through. Zigzag from one side of the lane to the other through the
    // stretch where the flight is lowest, on foot: every waypoint reached,
    // never below the walkway's own top, never by a mantle or a vault.
    // Against the old footprint the first southern waypoint is past the
    // walkway's edge. Ends at x = 9.8, east of the flight's own foot
    // (9.350) -- past that x the flight has no footprint left to overhang,
    // at any z -- so the final approach below can safely turn north.
    const double walkway_lane[][2] = {
        {0.0, -120.3}, {4.5, -118.6}, {6.5, -120.3}, {8.0, -118.6}, {9.8, -120.3}};
    double walkway_lowest_y = skin_forty.snapshot().player_position.y;
    bool walkway_traversed = false;
    for (const auto &waypoint : walkway_lane) {
        bool reached = false;
        for (int i = 0; i < 12 * 90 && !reached; ++i) {
            const auto state = skin_forty.snapshot();
            double dx = waypoint[0] - state.player_position.x;
            double dz = waypoint[1] - state.player_position.z;
            const double len = std::hypot(dx, dz);
            reached = len < 0.3;
            if (len > 1.0e-6) {
                dx /= len;
                dz /= len;
            }
            (void)skin_forty.set_move_input(dx, dz);
            (void)skin_forty.set_facing(dx, dz);
            (void)skin_forty.advance_frame(Simulation::kFixedStepSeconds);
            const auto after = skin_forty.snapshot();
            walkway_lowest_y = std::min(walkway_lowest_y, after.player_position.y);
            walkway_traversed =
                walkway_traversed || after.traversal_state != TraversalState::None;
        }
        require(reached, "every point of the walkway's lane under the upper flight must be "
                         "reachable on foot");
    }
    std::cout << "INFO AS-002 walkway lane: lowest_y=" << walkway_lowest_y << '\n';
    require(walkway_lowest_y > kLegalFortyMidLandingSurfaceY + 0.70,
            "crossing the walkway's lane must never drop below the walkway's own top");
    require(!walkway_traversed, "the walkway's lane is walked, not mantled or vaulted");
    // From here to the landing's own centre, x only decreases from 9.8 to
    // 9.350 -- never west of the flight's own foot -- while z climbs clear
    // of its band, so this straight line never passes under it.
    walk_toward(skin_forty, kLegalFortyMidLandingX, kLegalFortyMidLandingZ, 15.0);
    // Same two-step crossing the ascent route above needed at this identical
    // boundary: the landing's z in [-121.0, -110.6] does not fully overlap
    // the upper flight's own, narrower z in [-117.9, -116.1], so align to
    // the flight's own z-band first, while still on the wide landing, then
    // cross west along that band -- a direct diagonal from the landing's
    // own centre falls through the gap between them instead.
    walk_toward(skin_forty, kLegalFortyMidLandingX, kLegalFortyUpperFlightZ, 10.0);
    bool skin_forty_reached_hall_deck = false;
    for (int i = 0; i < 40 * 90 && !skin_forty_reached_hall_deck; ++i) {
        const auto state = skin_forty.snapshot();
        double dx = kLegalFortyWellX - state.player_position.x;
        double dz = kLegalFortyUpperFlightZ - state.player_position.z;
        const double len = std::hypot(dx, dz);
        if (len > 1.0e-6) {
            dx /= len;
            dz /= len;
        }
        (void)skin_forty.set_move_input(dx, dz);
        (void)skin_forty.set_facing(dx, dz);
        (void)skin_forty.advance_frame(Simulation::kFixedStepSeconds);
        skin_forty_reached_hall_deck =
            skin_forty.snapshot().support_entity_id == Simulation::kIntakeHallDeckEntityId;
    }
    require(skin_forty_reached_hall_deck,
            "SKIN's own bypass must reach MOD-HALL-DECK itself within budget");
    require(skin_forty.set_move_input(0.0, 0.0), "skin-forty settle input must be accepted");
    require(skin_forty.advance_frame(2.0).accepted, "skin-forty settle interval must be accepted");
    const auto skin_forty_state = skin_forty.snapshot();
    require(skin_forty_state.support_entity_id == Simulation::kIntakeHallDeckEntityId,
            "SKIN's own bypass must still finish on MOD-HALL-DECK itself");
    require(skin_forty_state.intake_pack_pins_dog && skin_forty_state.legal_forty_pack_slung,
            "climbing SKIN to +40 m must not falsely mutate either mechanism -- the pack is "
            "still down at the dog and still slung at the hook");
    require(skin_forty_state.legal_forty_swing_travel_radians <= 0.20,
            "climbing SKIN must never travel the swing hinge -- the walkway and the upper "
            "flight are the route, not the bascule");

    // 6. wo015_jib_cannot_reach_forty: the rated winch's own known ceiling
    //    is unmoved by anything AS-002 added -- this is Design Values
    //    8.4.5's arithmetic made executable, so the gap cannot be quietly
    //    closed later by a boom change.
    Simulation capped(InitialSpawn::IntakePendant, scraperx::sim::WorldContent::RegressionFixtures);
    require(capped.advance_frame(0.5).accepted, "capped settling interval must be accepted");
    double capped_deepest_hook_y = capped.snapshot().intake_hook_position.y;
    for (std::uint32_t tick = 0; tick < 90 * 60; ++tick) {
        (void)capped.set_intake_hoist_input(1.0);
        (void)capped.advance_frame(Simulation::kFixedStepSeconds);
        capped_deepest_hook_y = std::max(capped_deepest_hook_y, capped.snapshot().intake_hook_position.y);
    }
    require(capped_deepest_hook_y < 12.0,
            "holding the intake hoist at its limit for 60 s must never put the hook within "
            "reach of +40 m -- every jib-driven body stays below y = 12.0");

    std::cout << "PASS scraperx_sim AS-002 Legal Forty: unrouted_deepest_y=" << unrouted_deepest_y
              << " flag_hinge=" << mashed_state.legal_forty_swing_travel_radians
              << " cradle_drop=" << (cradle_y_before_load - deployed.legal_forty_cradle_position.y)
              << " deployed_travel=" << deployed.legal_forty_swing_travel_radians
              << " mid_landing_y=" << mid_landing_state.player_position.y
              << " forty_y=" << on_forty.player_position.y
              << " checkpoint_commits=" << on_forty.checkpoint_commit_count
              << " checkpoint_y=" << on_forty.checkpoint_position.y
              << " retracted_travel=" << retract.snapshot().legal_forty_swing_travel_radians
              << " skin_forty_mantles=" << skin_forty_mantles
              << " skin_forty_y=" << skin_forty_state.player_position.y
              << " jib_capped_hook_y=" << capped_deepest_hook_y << '\n';

    // ---- World solids: every visible body is a body -----------------------
    // The dressing drawn by the Godot builders (world_solids.inc) is native
    // collision, not decoration. The buttress footings at the stack's corners
    // are the case a player walked straight through: a 6 x 2.6 x 6 m block
    // under a 2.4 m splayed leg. Walked into from the yard, the capsule must
    // stop at its face -- centre never nearer than half-width (3.0 m) plus
    // most of the capsule radius (0.35 m) on the dominant axis.
    Simulation solid(InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::RegressionFixtures);
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

    // Stack stairs are walked, not jumped, and lead somewhere: from the yard,
    // up all fourteen flights to the top deck with no jump ever requested.
    // Each flight must end with the capsule standing on the deck it serves,
    // through the stairwell cut in it, not stalled under that deck's
    // underside; the route then walks the side band to the next flight's foot.
    Simulation stair(InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::RegressionFixtures);
    require(stair.advance_frame(0.5).accepted, "stair settling interval must be accepted");
    walk_toward(stair, -25.0, -118.0, 20.0);
    walk_toward(stair, -25.0, -128.5, 6.0);
    int stack_levels_reached = 0;
    for (int level = 0; level < 14; ++level) {
        const double side = (level % 2 == 0) ? 1.0 : -1.0;
        const double band = -150.0 + side * 21.5;
        walk_toward(stair, -side * 20.0, band, 12.0);
        walk_toward(stair, side * 21.0, band, 16.0);
        const auto head = stair.snapshot();
        const double deck = 11.0 * (level + 1);
        if (!(head.player_grounded && head.player_position.y > deck + 0.5 &&
              head.player_position.y < deck + 1.5)) {
            break;
        }
        stack_levels_reached = level + 1;
        walk_toward(stair, side * 21.5, -150.0 - side * 21.5, 12.0);
    }
    const auto stair_top = stair.snapshot();
    require(stack_levels_reached == 14,
            "every stack flight must be walked up onto the deck it serves, grade to the top");
    require(stair_top.step_up_count > 0,
            "the route must engage the native step-up; no jump is ever requested");

    // The yard sits in a basin, not on a slab in a void: off the grade's
    // east edge the valley floor carries the player 0.3 m lower, the step
    // back up is walked, and walking out to the rim ends against rock --
    // grounded, low, and never past the ridge line.
    Simulation basin(InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::RegressionFixtures);
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
              << " stack_levels=" << stack_levels_reached
              << " stair_top_y=" << stair_top.player_position.y
              << " step_ups=" << stair_top.step_up_count
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
    Simulation crouch(InitialSpawn::StaticDeck, scraperx::sim::WorldContent::RegressionFixtures);
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

    // ---- AS-003: MOD-HOOK5-RACK, CAP-HOOK5 acquired ------------------------
    // Atlas B00: "Hook block + slings in a locked cage opened by moving the
    // crate or circling the belt." The lock is height alone -- a 4.45 m top
    // against 3.75 m of jump-grab from grade, 5.13 m from the belt's deck --
    // sited where nothing on the apron but the deck comes within a jump. The
    // bar is a body in the door's swing; the block is a body on a real carry
    // constraint, and holding it is what stops every pull-up.

    // 1. wo016_apron_cannot_reach_the_cage: every face from grade, at a run,
    //    jump and traversal mashed; then the apron's nearest raised surface,
    //    the 9 t pack's top, jumped from at the cage. No cage body is ever
    //    stood on, grabbed, or offered.
    Simulation apron(InitialSpawn::Hook5Apron, scraperx::sim::WorldContent::RegressionFixtures);
    require(apron.advance_frame(0.5).accepted, "hook apron settling interval must be accepted");
    CageReach from_grade;
    struct Leap final {
        double from_x, from_z, at_x, at_z;
    };
    const Leap grade_leaps[] = {
        {11.5, kHook5MinZ - 4.0, 11.5, kHook5MinZ},   // south face, east of the doorway
        {10.0, kHook5MinZ - 4.0, 10.0, kHook5MinZ},   // the shut door
        {8.6, kHook5MinZ - 4.0, 8.6, kHook5MinZ},     // south face, the buttress
        {15.5, kHook5MinZ - 3.5, kHook5MaxX, kHook5MinZ},
        {16.0, kHook5MinZ + 0.7, kHook5MaxX, kHook5MinZ + 0.7},
        {16.0, kHook5MinZ + 2.0, kHook5MaxX, kHook5MinZ + 2.0},
        {16.0, kHook5MaxZ - 0.7, kHook5MaxX, kHook5MaxZ - 0.7},
        {15.5, kHook5MaxZ + 3.5, kHook5MaxX, kHook5MaxZ},
        {11.5, kHook5MaxZ + 4.0, 11.5, kHook5MaxZ},
        {10.0, kHook5MaxZ + 4.0, 10.0, kHook5MaxZ},
        {8.6, kHook5MaxZ + 4.0, 8.6, kHook5MaxZ},
    };
    for (const auto &leap : grade_leaps) {
        require(walk_to(apron, leap.from_x, leap.from_z, 10.0),
                "the apron walk must reach each approach to the cage");
        CageReach this_leap;
        leap_at(apron, leap.at_x, leap.at_z, 0.9, 2.0, this_leap);
        require(this_leap.closest < 0.45,
                "each grade leap must carry the body right up against the cage's face");
        from_grade.touched = from_grade.touched || this_leap.touched;
        from_grade.peak_y = std::max(from_grade.peak_y, this_leap.peak_y);
        require(walk_to(apron, leap.from_x, leap.from_z, 6.0),
                "the apron walk must back off each face it leapt at");
    }
    require(!from_grade.touched,
            "from grade no cage body may be stood on, grabbed, or offered as a ledge");
    require(from_grade.peak_y < 3.2,
            "from grade the body must never rise past a plain jump's apex");

    // The 9 t pack's top (1.8 m) is the highest thing on the apron within
    // 10 m of the cage: mantle it from the east, then run north along the
    // strip its mast leaves and leap at the cage from its north edge.
    CageReach from_pack;
    bool stood_on_pack = false;
    const double pack_targets[][2] = {
        {8.6, kHook5MinZ}, {10.0, kHook5MinZ}, {11.5, kHook5MinZ}, {kHook5MaxX, kHook5MinZ + 1.0}};
    for (const auto &target : pack_targets) {
        require(walk_to(apron, 13.5, kHook5MinZ - 4.0, 10.0),
                "the apron walk must come round to the pack's east side");
        const auto pack = apron.snapshot().intake_overweight_pack_position;
        require(walk_to(apron, pack.x + 2.0, pack.z, 10.0),
                "the apron walk must reach the 9 t pack's east face");
        bool on_pack = false;
        for (int tick = 0; tick < 4 * 90 && !on_pack; ++tick) {
            const auto state = apron.snapshot();
            steer_toward(apron, pack.x, pack.z, 0.3);
            if ((state.traversal_state == TraversalState::None && state.player_grounded &&
                 state.ledge_available) ||
                state.traversal_state == TraversalState::Hanging) {
                (void)apron.request_traversal();
            }
            (void)apron.advance_frame(Simulation::kFixedStepSeconds);
            const auto after = apron.snapshot();
            on_pack = after.player_grounded &&
                      after.support_entity_id == Simulation::kIntakeOverweightPackEntityId;
        }
        require(on_pack, "the 9 t pack's top must be mantled from grade on its east side");
        stood_on_pack = true;
        require(walk_to(apron, pack.x + 0.75, pack.z - 0.8, 3.0, 0.12),
                "the pack-top walk must reach the strip east of the mast");
        // Jump as the body reaches the pack's north edge.
        const double edge_x = pack.x + 0.75;
        const double edge_z = pack.z + 1.0;
        CageReach this_leap;
        leap_at(apron, target[0], target[1],
                std::hypot(target[0] - edge_x, target[1] - edge_z), 2.5, this_leap);
        from_pack.touched = from_pack.touched || this_leap.touched;
        from_pack.peak_y = std::max(from_pack.peak_y, this_leap.peak_y);
        from_pack.closest = std::min(from_pack.closest, this_leap.closest);
    }
    require(stood_on_pack, "the pack attempts must actually stand on the 9 t pack");
    require(!from_pack.touched,
            "from the 9 t pack's top no cage body may be stood on, grabbed, or offered");
    std::cout << "PASS scraperx_sim AS-003 apron: grade_peak_y=" << from_grade.peak_y
              << " pack_peak_y=" << from_pack.peak_y << " pack_closest=" << from_pack.closest
              << '\n';

    // 3. wo016_bar_is_not_a_flag: inside the shut cage, every command there is
    //    mashed for 30 s while the body faces away from the bar. The door's
    //    drive has been commanded open since build and stalls on the bar;
    //    there is no door command to find. Then the bar is lifted, carried
    //    north and set down, and the door travels by itself.
    Simulation cage(InitialSpawn::Hook5Cage, scraperx::sim::WorldContent::RegressionFixtures);
    require(cage.advance_frame(1.0).accepted, "hook cage settling interval must be accepted");
    require(cage.snapshot().hook5_door_angle_radians <= 0.05 && cage.snapshot().hook_in_rack,
            "the cage must start shut on its bar with the hook block in its rack");
    const auto bar_seat = cage.snapshot().hook5_bar_position;
    double mashed_door = 0.0;
    for (std::uint32_t tick = 0; tick < 90 * 30; ++tick) {
        (void)cage.set_facing(0.0, 1.0);
        (void)cage.request_jump();
        (void)cage.request_traversal();
        (void)cage.request_pick_up();
        (void)cage.request_set_down();
        (void)cage.set_crouch_input((tick / 45) % 2 == 0);
        (void)cage.request_valve_toggle();
        (void)cage.set_jib_slew_input(1.0);
        (void)cage.set_jib_hoist_input(1.0);
        (void)cage.set_needle_hoist_input(1.0);
        (void)cage.set_intake_slew_input(1.0);
        (void)cage.set_intake_hoist_input(1.0);
        (void)cage.request_intake_sling_release();
        (void)cage.request_intake_sling_attach();
        (void)cage.request_parachute();
        (void)cage.advance_frame(Simulation::kFixedStepSeconds);
        mashed_door = std::max(mashed_door, cage.snapshot().hook5_door_angle_radians);
    }
    (void)cage.set_crouch_input(false);
    require(cage.advance_frame(0.5).accepted, "hook cage unmash interval must be accepted");
    require(mashed_door <= 0.05, "no command may travel the door while the bar is seated");
    {
        const auto bar_after = cage.snapshot().hook5_bar_position;
        require(std::abs(bar_after.x - bar_seat.x) < 0.02 && std::abs(bar_after.y - bar_seat.y) < 0.02 &&
                    std::abs(bar_after.z - bar_seat.z) < 0.02 &&
                    cage.snapshot().carrying_entity_id == 0,
                "mashing every command away from the bar must leave it seated and nothing held");
    }
    require(lift_bar_clear(cage), "the bar must lift out of its brackets and set down clear");
    require(advance_until(cage,
                          [](const auto &state) { return state.hook5_door_angle_radians >= 1.20; },
                          6.0),
            "with the bar out of its swing the door must travel past 1.20 rad on its own drive");
    const double travelled_door = cage.snapshot().hook5_door_angle_radians;
    {
        const auto bar_down = cage.snapshot().hook5_bar_position;
        require(cage.advance_frame(0.5).accepted, "the set-down bar interval must be accepted");
        require(horizontal_distance(bar_down, bar_seat) > 0.3 &&
                    horizontal_distance(cage.snapshot().hook5_bar_position, bar_down) < 0.01,
                "the set-down bar must lie where it was put, out of its brackets and at rest");
    }

    // 4. wo016_block_is_a_body: taken off its rack, carried out of the cage
    //    and around the apron for 10 s, set down, and picked up where it lay.
    require(take_block(cage), "the hook block must come off its rack onto the carry point");
    require(carry_out_of_cage(cage), "the block must be carried out through the travelled door");
    require(!cage.snapshot().hook_in_rack,
            "carried out of the cage, CAP-HOOK5 must read held -- derived from the block's pose");
    double worst_gap = 0.0;
    bool kept_hold = true;
    const double tour[][2] = {{15.0, kHook5MinZ - 3.0}, {15.0, kHook5MaxZ + 4.0},
                              {13.0, kHook5MaxZ + 4.0}, {13.0, kHook5MinZ - 4.0}};
    for (std::uint32_t tick = 0, leg = 0; tick < 90 * 10; ++tick) {
        const auto state = cage.snapshot();
        if (std::hypot(tour[leg][0] - state.player_position.x,
                       tour[leg][1] - state.player_position.z) < 0.3) {
            leg = (leg + 1) % 4;
        }
        steer_toward(cage, tour[leg][0], tour[leg][1]);
        (void)cage.advance_frame(Simulation::kFixedStepSeconds);
        const auto after = cage.snapshot();
        kept_hold = kept_hold && after.carrying_entity_id == Simulation::kHook5BlockEntityId;
        worst_gap = std::max(worst_gap,
                             horizontal_distance(after.hook5_block_position, after.player_position));
    }
    require(kept_hold, "10 s of walking with the block must never lose it");
    require(worst_gap <= 1.0, "the carried block must track the player within 1.0 m");

    // How the 36 kg load changes the walk, measured rather than assumed
    // (AS-003 8.4.3): from rest, eastward, with the block and then without.
    const auto launch = [&cage]() {
        double speed_at_quarter = 0.0;
        double top = 0.0;
        for (int tick = 0; tick < 90; ++tick) {
            (void)cage.set_move_input(1.0, 0.0);
            (void)cage.set_facing(1.0, 0.0);
            (void)cage.advance_frame(Simulation::kFixedStepSeconds);
            const double speed = horizontal_magnitude(cage.snapshot().player_linear_velocity);
            top = std::max(top, speed);
            if (tick == 22) {
                speed_at_quarter = speed;
            }
        }
        (void)cage.set_move_input(0.0, 0.0);
        (void)cage.advance_frame(0.6);
        return std::make_pair(speed_at_quarter, top);
    };
    require(walk_to(cage, 13.5, kHook5MinZ - 4.0, 10.0) &&
                walk_to(cage, 9.5, kHook5MinZ - 4.0, 10.0),
            "the launch walk must reach its start");
    (void)cage.set_facing(1.0, 0.0);
    require(cage.advance_frame(0.8).accepted, "the loaded launch settle must be accepted");
    const auto loaded_launch = launch();
    require(cage.snapshot().carrying_entity_id == Simulation::kHook5BlockEntityId,
            "the loaded launch must keep the block");

    require(cage.request_set_down(), "set down must be accepted");
    require(cage.advance_frame(3.0).accepted, "the set-down block's fall must be accepted");
    const auto dropped = cage.snapshot();
    require(dropped.carrying_entity_id == 0 && dropped.hook5_block_position.y < 0.35,
            "set down, the block must fall and rest on the ground");
    require(cage.advance_frame(0.5).accepted, "the resting block interval must be accepted");
    require(horizontal_distance(cage.snapshot().hook5_block_position, dropped.hook5_block_position) <
                0.01,
            "the set-down block must be at rest");
    require(walk_to(cage, 9.5, kHook5MinZ - 5.5, 6.0), "the free launch walk must reach its start");
    (void)cage.set_facing(1.0, 0.0);
    require(cage.advance_frame(0.8).accepted, "the free launch settle must be accepted");
    const auto free_launch = launch();
    std::cout << "INFO AS-003 carry walk: loaded_v_at_0.25s=" << loaded_launch.first
              << " free_v_at_0.25s=" << free_launch.first << " loaded_top=" << loaded_launch.second
              << " free_top=" << free_launch.second << '\n';
    require(pick_block_from_ground(cage),
            "the block must be pickable again where it came to rest");
    const double repicked_y = cage.snapshot().hook5_block_position.y;
    require(repicked_y > 0.8, "picked off the ground, the block must come up to the hands");

    // 5. wo016_carrying_blocks_traversal: at the 9 t pack's east face -- the
    //    ledge falsifier 1 mantled -- holding the block, traversal is asked
    //    for on every tick for 5 s while pressing at the face, and nothing
    //    begins; set down, the same face mantles at once.
    require(walk_to(cage, 13.5, kHook5MinZ - 4.0, 10.0),
            "the carry must come round to the 9 t pack's side");
    const auto held_pack = cage.snapshot().intake_overweight_pack_position;
    require(walk_to(cage, held_pack.x + 2.0, held_pack.z, 10.0),
            "the carry must reach the 9 t pack's east face");
    const auto accepted_before_held = cage.snapshot().accepted_traversal_count;
    bool offered_while_held = false;
    for (std::uint32_t tick = 0; tick < 90 * 5; ++tick) {
        steer_toward(cage, held_pack.x, held_pack.z, 0.3);
        (void)cage.request_traversal();
        (void)cage.advance_frame(Simulation::kFixedStepSeconds);
        offered_while_held = offered_while_held || cage.snapshot().ledge_available;
    }
    (void)cage.set_move_input(0.0, 0.0);
    const auto refused = cage.snapshot();
    require(refused.carrying_entity_id == Simulation::kHook5BlockEntityId,
            "pressing at the pack must not cost the block");
    require(refused.accepted_traversal_count == accepted_before_held && !offered_while_held &&
                refused.player_position.y < 1.2,
            "holding the block, no ledge may be offered and no traversal may begin");
    for (int tick = 0; tick < 110; ++tick) {
        (void)cage.set_facing(0.0, 1.0);  // the hands swing the block round behind
        (void)cage.advance_frame(Simulation::kFixedStepSeconds);
    }
    require(cage.request_set_down(), "set down at the pack must be accepted");
    require(cage.advance_frame(1.5).accepted, "the pack set-down interval must be accepted");
    bool mantled_free = false;
    for (std::uint32_t tick = 0; tick < 90 * 4 && !mantled_free; ++tick) {
        const auto state = cage.snapshot();
        steer_toward(cage, held_pack.x, held_pack.z, 0.3);
        if (state.traversal_state == TraversalState::None && state.player_grounded &&
            state.ledge_available) {
            (void)cage.request_traversal();
        }
        (void)cage.advance_frame(Simulation::kFixedStepSeconds);
        const auto after = cage.snapshot();
        mantled_free = after.player_grounded &&
                       after.support_entity_id == Simulation::kIntakeOverweightPackEntityId;
    }
    require(mantled_free && cage.snapshot().accepted_traversal_count > accepted_before_held,
            "hands free, the same face must mantle at once");

    // 6-7. wo016_hands_free_restores_everything, wo016_skin_geometry_is_untouched:
    //      the same instance carries the block to MOD-SKIN-LADDER-S's foot.
    //      Held, the first rung is never offered and no request climbs it;
    //      set down on the spot, the same rung is offered at once -- the
    //      ladder did not change, the hands did -- and AS-001's SKIN climb
    //      runs to +24 m.
    constexpr double kSkinX = -6.0;
    constexpr double kSkinFootZ = -76.5;
    require(walk_to(cage, held_pack.x + 2.6, held_pack.z, 6.0),
            "walking off the pack's east edge must reach the apron");
    require(pick_block_from_ground(cage), "the block must be picked up again beside the pack");
    require(walk_to(cage, 13.5, kHook5MinZ - 4.0, 12.0) && walk_to(cage, 13.5, kSkinFootZ, 12.0) &&
                walk_to(cage, kSkinX, kSkinFootZ, 20.0),
            "the block must be carried round the belt's north end to the SKIN foot");
    require(cage.snapshot().carrying_entity_id == Simulation::kHook5BlockEntityId,
            "the block must arrive at the SKIN foot still held");
    const auto accepted_before_rung = cage.snapshot().accepted_traversal_count;
    bool rung_offered_while_held = false;
    for (std::uint32_t tick = 0; tick < 90 * 3; ++tick) {
        (void)cage.set_move_input(0.0, -0.3);
        (void)cage.set_facing(0.0, -1.0);
        (void)cage.request_traversal();
        (void)cage.advance_frame(Simulation::kFixedStepSeconds);
        rung_offered_while_held = rung_offered_while_held || cage.snapshot().ledge_available;
    }
    (void)cage.set_move_input(0.0, 0.0);
    require(cage.snapshot().accepted_traversal_count == accepted_before_rung &&
                !rung_offered_while_held && cage.snapshot().player_position.y < 1.2,
            "holding the block, MOD-SKIN-LADDER-S's first rung must be neither offered nor climbed");
    for (int tick = 0; tick < 110; ++tick) {
        (void)cage.set_facing(0.0, 1.0);
        (void)cage.advance_frame(Simulation::kFixedStepSeconds);
    }
    require(cage.request_set_down(), "set down at the SKIN foot must be accepted");
    require(cage.advance_frame(1.5).accepted, "the SKIN set-down interval must be accepted");
    bool rung_offered_free = false;
    for (int tick = 0; tick < 90 && !rung_offered_free; ++tick) {
        (void)cage.set_facing(0.0, -1.0);
        (void)cage.advance_frame(Simulation::kFixedStepSeconds);
        rung_offered_free = cage.snapshot().ledge_available;
    }
    require(rung_offered_free,
            "set down where it stood, the same first rung must be offered at once");
    std::uint32_t skin_climb_mantles = 0;
    bool skin_climb_arrived = false;
    for (std::uint32_t tick = 0; tick < 90 * 120; ++tick) {
        const auto state = cage.snapshot();
        if (state.support_entity_id == Simulation::kIntakeHandoffEntityId) {
            skin_climb_arrived = true;
            break;
        }
        (void)cage.set_move_input(0.0, -1.0);
        (void)cage.set_facing(0.0, -1.0);
        const bool ready = (state.traversal_state == TraversalState::None &&
                            state.player_grounded && state.ledge_available) ||
                           state.traversal_state == TraversalState::Hanging;
        if (ready && cage.request_traversal()) {
            ++skin_climb_mantles;
        }
        (void)cage.advance_frame(Simulation::kFixedStepSeconds);
    }
    require(skin_climb_arrived,
            "hands free, the same instance must climb MOD-SKIN-LADDER-S to the +24 m deck");
    std::cout << "PASS scraperx_sim AS-003 cage: mashed_door=" << mashed_door
              << " travelled_door=" << travelled_door << " carry_worst_gap=" << worst_gap
              << " repicked_y=" << repicked_y << " skin_mantles=" << skin_climb_mantles << '\n';

    // 2 and 8. wo016_belt_ride_reaches_the_roof, wo016_hook_reaches_forty:
    //    one instance, start to finish. AS-002's freight and cradle sequence
    //    opens the throat and deploys the flight; then the belt ride onto the
    //    cage, the hatch, the bar, the block, out through the door, round the
    //    belt's north end, through the throat and up every flight to
    //    MOD-HALL-DECK with the block held all the way.
    Simulation hook(InitialSpawn::IntakePendant, scraperx::sim::WorldContent::RegressionFixtures);
    require(hook.advance_frame(0.5).accepted, "hook ascent settling interval must be accepted");
    require(hook.set_intake_hoist_input(1.0), "hook ascent hoist command must be accepted");
    require(advance_until(hook, [](const auto &state) { return state.intake_pack_position.y > 8.0; },
                          20.0),
            "the hook ascent's lift clear must complete within budget");
    require(hook.set_intake_slew_input(-1.0), "hook ascent slew command must be accepted");
    require(advance_until(
                hook, [](const auto &state) { return state.intake_boom_angle_radians <= -0.80; },
                20.0),
            "the hook ascent's slew to the cradle bearing must complete within budget");
    require(hook.set_intake_slew_input(0.0), "hook ascent slew-stop command must be accepted");
    require(hook.advance_frame(6.0).accepted, "hook ascent slew-settle interval must be accepted");
    require(hook.set_intake_hoist_input(-1.0), "hook ascent lower command must be accepted");
    bool hook_released = false;
    for (std::uint32_t tick = 0; tick < 90 * 45 && !hook_released; ++tick) {
        (void)hook.request_intake_sling_release();
        (void)hook.advance_frame(Simulation::kFixedStepSeconds);
        hook_released = !hook.snapshot().legal_forty_pack_slung;
    }
    require(hook_released, "the hook ascent's release over the cradle must free the sling");
    require(advance_until(hook,
                          [](const auto &state) {
                              return state.legal_forty_swing_travel_radians >= 0.85;
                          },
                          40.0),
            "the loaded cradle must deploy the flight for the hook ascent");
    require(hook.set_intake_hoist_input(0.0), "hook ascent hoist-stop command must be accepted");

    const double ride_start = hook.snapshot().simulation_time_seconds;
    require(ride_belt_onto_cage(hook),
            "riding MOD-INTAKE-BELT must carry the body alongside the buttress and a jump-grab "
            "must put it on the cage");
    const auto on_roof = hook.snapshot();
    const double ride_seconds = on_roof.simulation_time_seconds - ride_start;
    require(ride_seconds <= 32.0, "the belt must deliver the body onto the cage within two strokes");
    require(on_roof.support_entity_id == Simulation::kHook5CageEntityId &&
                on_roof.player_position.y >= kHook5TopY + 0.70,
            "the body must stand on the cage's top, not graze it");
    require(drop_through_hatch(hook), "the hatch must drop the body onto the cage floor");
    const auto in_cage = hook.snapshot();
    require(in_cage.hook5_door_angle_radians <= 0.05 && in_cage.death_count == 0,
            "the hatch drop must be survivable and land inside a still-shut cage");
    require(lift_bar_clear(hook), "the hook ascent must lift the bar clear");
    require(advance_until(hook,
                          [](const auto &state) { return state.hook5_door_angle_radians >= 1.20; },
                          6.0),
            "the hook ascent's door must travel once the bar is clear");
    require(take_block(hook) && carry_out_of_cage(hook),
            "the hook ascent must take the block and carry it out of the cage");

    // Round the belt's north end -- its deck never reaches past z = -81 --
    // and down the apron's west side to the throat, then AS-002's route.
    const double to_throat[][2] = {{13.5, kHook5MinZ - 2.0}, {13.5, -79.5}, {2.0, -79.5},
                                   {-1.2, -85.0}, {-1.2, -106.0}};
    for (const auto &point : to_throat) {
        require(walk_to(hook, point[0], point[1], 25.0),
                "the block must be carried round the belt to the throat");
    }
    walk_toward(hook, kThroatX, -107.5, 12.0);
    walk_toward(hook, kThroatX, -113.0, 8.0);
    // Onto the first flight along its lane, from beyond its foot: a load
    // carried at the belly strikes the rising slab's 1.1 m side edge that an
    // empty-handed body slides along to reach the foot the way AS-001 walks.
    require(walk_to(hook, -8.3, -113.2, 12.0) && walk_to(hook, -8.3, -116.0, 6.0),
            "the carry must come round to the foot of MOD-STAIR-A's first flight");
    for (int flight = 0; flight < 6; ++flight) {
        const double side = (flight % 2 == 0) ? 1.0 : -1.0;
        const double lane = -118.0 + side * 2.0;
        walk_toward(hook, -side * kStairLandingX, lane, 8.0);
        walk_toward(hook, side * kStairLandingX, lane, 14.0);
    }
    // Arrive and stop: a full stick dithering about AS-002's own waypoint,
    // 0.3 m inside the deck's north edge, walked a loaded body off it.
    require(walk_to(hook, -9.0, -109.0, 12.0), "the carry must reach the handoff deck");
    require(hook.snapshot().support_entity_id == Simulation::kIntakeHandoffEntityId &&
                hook.snapshot().carrying_entity_id == Simulation::kHook5BlockEntityId,
            "MOD-STAIR-A must carry the body and the block to the +24 m handoff deck");
    walk_toward(hook, -6.0, -112.5, 25.0);
    for (int i = 0; i < 20 * 90; ++i) {
        const auto state = hook.snapshot();
        double dx = kLegalFortyHingeX - state.player_position.x;
        double dz = kLegalFortyHingeZ - state.player_position.z;
        const double len = std::hypot(dx, dz);
        if (len > 1.0e-6) {
            dx /= len;
            dz /= len;
        }
        (void)hook.set_move_input(dx, dz);
        (void)hook.set_facing(dx, dz);
        (void)hook.advance_frame(Simulation::kFixedStepSeconds);
    }
    walk_toward(hook, kLegalFortyMidLandingX - 0.85, kLegalFortyHingeZ, 10.0);
    walk_toward(hook, kLegalFortyMidLandingX, kLegalFortyMidLandingZ, 10.0);
    walk_toward(hook, kLegalFortyMidLandingX, kLegalFortyUpperFlightZ, 10.0);
    bool hook_on_hall = false;
    for (int i = 0; i < 40 * 90 && !hook_on_hall; ++i) {
        const auto state = hook.snapshot();
        double dx = kLegalFortyWellX - state.player_position.x;
        double dz = kLegalFortyUpperFlightZ - state.player_position.z;
        const double len = std::hypot(dx, dz);
        if (len > 1.0e-6) {
            dx /= len;
            dz /= len;
        }
        (void)hook.set_move_input(dx, dz);
        (void)hook.set_facing(dx, dz);
        (void)hook.advance_frame(Simulation::kFixedStepSeconds);
        hook_on_hall = hook.snapshot().support_entity_id == Simulation::kIntakeHallDeckEntityId;
    }
    require(hook.set_move_input(0.0, 0.0), "hook settle-on-forty input must be accepted");
    require(hook.advance_frame(2.0).accepted, "hook settle-on-forty interval must be accepted");
    const auto hook_forty = hook.snapshot();
    require(hook_on_hall && hook_forty.support_entity_id == Simulation::kIntakeHallDeckEntityId &&
                hook_forty.player_position.y >= kLegalFortyHallDeckSurfaceY + 0.70,
            "carrying the block, MOD-STAIR-A must bring the body to MOD-HALL-DECK");
    require(hook_forty.carrying_entity_id == Simulation::kHook5BlockEntityId &&
                !hook_forty.hook_in_rack &&
                horizontal_distance(hook_forty.hook5_block_position, hook_forty.player_position) <= 1.0,
            "CAP-HOOK5 must still be held on arrival at +40.19 m");
    require(hook_forty.checkpoint_position.y >= 40.0,
            "standing on the hall deck with the block must commit a checkpoint there");

    // Persist v3: the carry is topology a checkpoint restores. Walk off the
    // hall deck's north edge holding the block; the 40 m fall is lethal, and
    // the restore must put the block back in the hands on the deck.
    require(walk_to(hook, -5.5, -113.0, 12.0) && walk_to(hook, -1.2, -110.0, 12.0),
            "the hook carry must reach the hall deck's north edge");
    require(hook.advance_frame(1.0).accepted, "the edge settle interval must be accepted");
    const auto deaths_before = hook.snapshot().death_count;
    for (std::uint32_t tick = 0; tick < 90 * 8 && hook.snapshot().death_count == deaths_before;
         ++tick) {
        const auto state = hook.snapshot();
        (void)hook.set_move_input(0.0, state.player_grounded ? 0.4 : 0.0);
        (void)hook.set_facing(0.0, 1.0);
        (void)hook.advance_frame(Simulation::kFixedStepSeconds);
    }
    require(hook.snapshot().death_count == deaths_before + 1,
            "walking off MOD-HALL-DECK with the block must be a lethal 40 m fall");
    require(hook.advance_frame(1.0).accepted, "the restore settle interval must be accepted");
    const auto restored = hook.snapshot();
    require(restored.player_position.y >= kLegalFortyHallDeckSurfaceY + 0.70 &&
                restored.carrying_entity_id == Simulation::kHook5BlockEntityId &&
                horizontal_distance(restored.hook5_block_position, restored.player_position) <= 1.0,
            "the checkpoint restore must return the body to the hall deck with the block in hand");
    require(hook.advance_frame(3.0).accepted, "the post-restore interval must be accepted");
    require(hook.snapshot().death_count == deaths_before + 1 &&
                hook.snapshot().player_position.y >= kLegalFortyHallDeckSurfaceY + 0.70,
            "the restored stance must hold: no second fall from where the checkpoint put it");
    require(restored.intake_overweight_pack_position.y < 1.2,
            "the 9 t proof load must still be on the ground after all of it");
    std::cout << "PASS scraperx_sim AS-003 Hook5 Rack: ride_s=" << ride_seconds
              << " roof_y=" << on_roof.player_position.y
              << " hatch_impact=" << in_cage.last_impact_speed_mps
              << " forty_y=" << hook_forty.player_position.y
              << " restored_y=" << restored.player_position.y
              << " restored_carry=" << restored.carrying_entity_id << '\n';


    // ---- Ground Archimedes screw: conserved water is the output port -----
    Simulation screw_off(InitialSpawn::WaterScrewStation, scraperx::sim::WorldContent::RegressionFixtures);
    require(screw_off.advance_frame(12.0).accepted, "stopped screw interval must advance");
    require(std::abs(screw_off.snapshot().water_screw_tank_volume_m3) < 1.0e-9,
            "motor off must move exactly no water to the upper tank");

    Simulation low_inlet(InitialSpawn::WaterScrewStation, scraperx::sim::WorldContent::RegressionFixtures);
    low_inlet.set_water_screw_basin_volume_m3(0.10);
    require(low_inlet.request_water_screw_toggle(), "low-inlet motor start must be accepted");
    require(low_inlet.advance_frame(12.0).accepted, "low-inlet interval must advance");
    require(low_inlet.snapshot().water_screw_tank_volume_m3 < 1.0e-6,
            "an inlet below minimum immersion must not pump water");

    Simulation under_torque(InitialSpawn::WaterScrewStation, scraperx::sim::WorldContent::RegressionFixtures);
    under_torque.set_water_screw_motor_torque_limit_nm(700.0);
    require(under_torque.request_water_screw_toggle(), "under-torque start must be accepted");
    require(under_torque.advance_frame(15.0).accepted, "under-torque interval must advance");
    require(std::abs(under_torque.snapshot().water_screw_rpm) < 0.5 &&
                under_torque.snapshot().water_screw_tank_volume_m3 < 1.0e-5,
            "a motor below hydraulic plus bearing torque must stall instead of faking flow");

    Simulation blocked_screw(InitialSpawn::WaterScrewStation, scraperx::sim::WorldContent::RegressionFixtures);
    blocked_screw.set_water_screw_outlet_blocked(true);
    require(blocked_screw.request_water_screw_toggle(), "blocked-outlet start must be accepted");
    require(blocked_screw.advance_frame(10.0).accepted, "blocked-outlet interval must advance");
    require(blocked_screw.snapshot().water_screw_tank_volume_m3 < 1.0e-6,
            "a blocked outlet must not fill the upper tank");

    Simulation screw(InitialSpawn::WaterScrewStation, scraperx::sim::WorldContent::RegressionFixtures);
    require(screw.advance_frame(0.5).accepted, "water-screw station settle must advance");
    require(screw.snapshot().water_screw_station_active,
            "the real apron spawn must be in reach of the screw control");
    require(screw.request_water_screw_toggle(), "normal screw start must be accepted");

    double screw_peak_rpm = 0.0;
    double screw_peak_torque = 0.0;
    double screw_peak_flow = 0.0;
    double worst_conservation = 0.0;
    const double initial_water = screw.snapshot().water_screw_basin_volume_m3 +
                                 screw.snapshot().water_screw_tank_volume_m3;
    bool screw_full = false;
    for (std::uint32_t tick = 0; tick < 90 * 125; ++tick) {
        require(screw.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "water-screw fixed step must advance");
        const auto state = screw.snapshot();
        screw_peak_rpm = std::max(screw_peak_rpm, std::abs(state.water_screw_rpm));
        screw_peak_torque =
            std::max(screw_peak_torque, std::abs(state.water_screw_motor_torque_nm));
        screw_peak_flow = std::max(screw_peak_flow, state.water_screw_flow_m3_s);
        worst_conservation =
            std::max(worst_conservation,
                     std::abs((state.water_screw_basin_volume_m3 +
                               state.water_screw_tank_volume_m3) - initial_water));
        if (state.water_screw_tank_volume_m3 >= 2.0 - 1.0e-9) {
            screw_full = true;
            break;
        }
    }
    require(screw_full, "rated screw must deliver the next mechanism's full 2.0 m3 input");
    const auto full_screw = screw.snapshot();
    require(screw_peak_rpm <= 22.05, "finite screw drive must respect its 22 rpm rating");
    require(screw_peak_torque <= 1500.01, "finite screw drive must respect 1.5 kN m torque");
    require(screw_peak_flow > 0.020 && screw_peak_flow < 0.023,
            "rated immersed screw must produce the derived ~0.022 m3/s flow");
    require(worst_conservation < 1.0e-9,
            "basin plus upper-tank water must be exactly conserved");
    require(full_screw.water_screw_basin_volume_m3 > 0.39 &&
                full_screw.water_screw_basin_volume_m3 < 0.41,
            "2.0 m3 delivered from 2.4 m3 must leave ~0.4 m3 in the basin");
    require(full_screw.water_screw_shaft_work_j >= 196200.0,
            "full delivery must spend at least the conservative 196.2 kJ shaft budget");

    const double full_before = full_screw.water_screw_tank_volume_m3;
    require(screw.advance_frame(8.0).accepted, "full-tank recycle interval must advance");
    const auto after_full = screw.snapshot();
    require(std::abs(after_full.water_screw_tank_volume_m3 - full_before) < 1.0e-9 &&
                std::abs(after_full.water_screw_flow_m3_s) < 1.0e-9,
            "a full tank must stop net delivery rather than delete or create water");

    screw.set_water_screw_drive_direction(-1);
    require(screw.advance_frame(10.0).accepted, "reverse interval must advance");
    const auto reversed_screw = screw.snapshot();
    require(reversed_screw.water_screw_tank_volume_m3 < full_before - 0.10 &&
                reversed_screw.water_screw_basin_volume_m3 >
                    after_full.water_screw_basin_volume_m3 + 0.10,
            "reverse shaft motion must return real water down to the basin");
    require(std::abs((reversed_screw.water_screw_basin_volume_m3 +
                      reversed_screw.water_screw_tank_volume_m3) - initial_water) < 1.0e-9,
            "reverse flow must conserve the same water inventory");
    std::cout << "PASS scraperx_sim ground water screw: tank_m3="
              << full_screw.water_screw_tank_volume_m3
              << " basin_m3=" << full_screw.water_screw_basin_volume_m3
              << " peak_rpm=" << screw_peak_rpm
              << " peak_torque_nm=" << screw_peak_torque
              << " peak_flow_m3_s=" << screw_peak_flow
              << " shaft_work_J=" << full_screw.water_screw_shaft_work_j
              << " leakage_m3=" << full_screw.water_screw_leakage_m3
              << " conserved_err=" << worst_conservation
              << " full_stop=1 reverse_return=1" << '\n';


    // ---- Ground water-weight lift: upstream mass becomes player ascent ----
    // The screw's delivery is already independently proven above. These runs
    // seed that exact conserved output at the seam, then falsify the receiving
    // mechanism itself. No seed supplies force, cage travel or catch state.
    const auto seed_water_lift = [](Simulation &sim, const double tank_m3) {
        sim.set_water_screw_tank_volume_m3_for_proof(tank_m3);
        require(sim.advance_frame(0.2).accepted, "water-lift seam seed must settle");
    };
    const auto fill_bucket = [](Simulation &sim, const double expected_m3) {
        const auto before = sim.snapshot();
        require(before.water_lift_valve_station_active,
                "water-lift fill control must be physically in reach");
        require(before.water_screw_tank_volume_m3 >= expected_m3 - 0.002,
                "water for the lift must exist in the authoritative upper tank");
        require(before.water_lift_bucket_catch_latched,
                "water-lift bucket must be physically caught at the fill station");
        require(sim.request_water_lift_valve_toggle(), "water-lift fill-open request accepted");
        require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "water-lift first fill tick must advance");
        const auto first = sim.snapshot();
        std::cout << "INFO ground water lift fill: expected_m3=" << expected_m3
                  << " valve=" << first.water_lift_valve_open
                  << " tank_m3=" << first.water_screw_tank_volume_m3
                  << " bucket_m3=" << first.water_lift_bucket_water_m3
                  << " bucket_travel=" << first.water_lift_bucket_travel_m
                  << " caught=" << first.water_lift_bucket_catch_latched
                  << " station=" << first.water_lift_valve_station_active
                  << " deaths=" << first.death_count << '\n';
        require(first.water_lift_valve_open,
                "water-lift fill command must open the authoritative valve");
        require(first.water_lift_bucket_water_m3 > before.water_lift_bucket_water_m3 &&
                    first.water_screw_tank_volume_m3 < before.water_screw_tank_volume_m3,
                "an open caught-bucket valve must transfer positive conserved water");
        require(advance_until(
                    sim,
                    [expected_m3](const Snapshot &s) {
                        return s.water_lift_bucket_water_m3 >= expected_m3 - 0.002;
                    },
                    24.0),
                "tank water must physically transfer into the caught bucket");
        require(sim.request_water_lift_valve_toggle(), "water-lift fill-close request accepted");
        require(sim.advance_frame(0.2).accepted, "water-lift valve close must resolve");
        require(!sim.snapshot().water_lift_valve_open,
                "water-lift valve must be shut before release");
    };
    const auto board_lift_cage = [](Simulation &sim) {
        // The controls sit outside the west rail, leaving the north opening
        // clear. Enter through that opening, then step left inside the cage to
        // the release lever instead of pathing through a pedestal.
        require(walk_to(sim, -12.50, -107.10, 5.0, 0.12) &&
                    walk_to(sim, -13.35, -107.65, 3.0, 0.12),
                "player must be able to enter the cage through its clear opening");
        require(sim.advance_frame(0.5).accepted, "cage stance must settle");
        const auto s = sim.snapshot();
        require(s.player_grounded &&
                    s.support_entity_id == Simulation::kGroundWaterLiftCageEntityId,
                "player must stand on the real moving cage before release");
        require(s.water_lift_release_station_active,
                "release lever must be reachable from inside the cage");
    };

    Simulation water_lift_dry(InitialSpawn::WaterLiftValveStation, scraperx::sim::WorldContent::RegressionFixtures);
    require(water_lift_dry.advance_frame(0.5).accepted, "dry lift spawn settle");
    board_lift_cage(water_lift_dry);
    require(water_lift_dry.request_water_lift_release(), "dry lift release request accepted");
    require(water_lift_dry.advance_frame(4.0).accepted, "dry lift interval must advance");
    require(water_lift_dry.snapshot().water_lift_cage_travel_m < 0.05,
            "an empty 200 kg bucket must not lift the player cage");

    Simulation water_lift_partial(InitialSpawn::WaterLiftValveStation, scraperx::sim::WorldContent::RegressionFixtures);
    seed_water_lift(water_lift_partial, 0.40);
    fill_bucket(water_lift_partial, 0.40);
    board_lift_cage(water_lift_partial);
    require(water_lift_partial.request_water_lift_release(),
            "partial lift release request accepted");
    require(water_lift_partial.advance_frame(5.0).accepted, "partial lift interval must advance");
    require(water_lift_partial.snapshot().water_lift_cage_travel_m < 0.15,
            "400 kg of water must fail by force balance, not an arbitrary fill flag");

    Simulation water_lift_caught(InitialSpawn::WaterLiftValveStation, scraperx::sim::WorldContent::RegressionFixtures);
    seed_water_lift(water_lift_caught, 2.0);
    fill_bucket(water_lift_caught, 2.0);
    const double caught_before = water_lift_caught.snapshot().water_lift_cage_travel_m;
    require(water_lift_caught.advance_frame(4.0).accepted, "caught lift interval must advance");
    require(std::abs(water_lift_caught.snapshot().water_lift_cage_travel_m - caught_before) < 0.02,
            "a full bucket still held by its physical catch must not move the cage");

    Simulation water_lift_disconnected(InitialSpawn::WaterLiftValveStation, scraperx::sim::WorldContent::RegressionFixtures);
    seed_water_lift(water_lift_disconnected, 2.0);
    fill_bucket(water_lift_disconnected, 2.0);
    board_lift_cage(water_lift_disconnected);
    water_lift_disconnected.set_water_lift_rope_connected(false);
    require(water_lift_disconnected.request_water_lift_release(),
            "disconnected lift release request accepted");
    require(water_lift_disconnected.advance_frame(4.0).accepted,
            "disconnected lift interval must advance");
    require(water_lift_disconnected.snapshot().water_lift_cage_travel_m < 0.05,
            "a disconnected rope must transmit no counterweight lift");

    Simulation water_lift_overload(InitialSpawn::WaterLiftValveStation, scraperx::sim::WorldContent::RegressionFixtures);
    seed_water_lift(water_lift_overload, 2.0);
    fill_bucket(water_lift_overload, 2.0);
    water_lift_overload.set_water_lift_cage_mass_kg(1300.0);
    board_lift_cage(water_lift_overload);
    require(water_lift_overload.request_water_lift_release(),
            "overloaded lift release request accepted");
    require(water_lift_overload.advance_frame(5.0).accepted,
            "overloaded lift interval must advance");
    require(water_lift_overload.snapshot().water_lift_cage_travel_m < 0.15,
            "an overloaded cage must fail from the real mass ratio");

    Simulation water_lift(InitialSpawn::WaterLiftValveStation, scraperx::sim::WorldContent::RegressionFixtures);
    seed_water_lift(water_lift, 2.0);
    const double water_total_start =
        water_lift.snapshot().water_screw_basin_volume_m3 +
        water_lift.snapshot().water_screw_tank_volume_m3 +
        water_lift.snapshot().water_lift_bucket_water_m3;
    fill_bucket(water_lift, 2.0);
    require(water_lift.snapshot().water_lift_bucket_mass_kg > 2198.0,
            "2.0 m3 must make the real bucket approximately 2200 kg");
    board_lift_cage(water_lift);
    require(water_lift.request_water_lift_release(), "rated lift release request accepted");

    bool rode_moving_cage = false;
    double lift_peak_rope_tension = 0.0;
    bool lift_caught = false;
    for (std::uint32_t tick = 0; tick < 12 * Simulation::kTickRateHz; ++tick) {
        require(water_lift.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "rated water-lift step must advance");
        const auto s = water_lift.snapshot();
        lift_peak_rope_tension =
            std::max(lift_peak_rope_tension, s.water_lift_rope_tension_n);
        rode_moving_cage =
            rode_moving_cage ||
            (s.support_entity_id == Simulation::kGroundWaterLiftCageEntityId &&
             s.water_lift_cage_travel_m > 0.25);
        if (s.water_lift_upper_catch_latched) {
            lift_caught = true;
            break;
        }
    }
    require(lift_caught,
            "full conserved water load must lift and catch the cage at +8 m");
    const auto lift_top = water_lift.snapshot();
    require(rode_moving_cage,
            "the player must ride the cage as a real moving support during ascent");
    require(lift_top.water_lift_cage_travel_m > 7.90,
            "upper catch must hold the cage at the +8 m travel seat");
    require(lift_top.water_lift_cage_peak_speed_mps <= 2.35,
            "finite brake/governor must keep cage speed near its 2.2 m/s rating");
    require(lift_peak_rope_tension > 1000.0,
            "the successful ascent must carry measurable solver rope tension");

    require(advance_until(
                water_lift,
                [](const Snapshot &s) { return s.water_lift_bucket_water_m3 < 0.01; },
                22.0),
            "caught cage must hold while the lower bucket drains by gravity");
    const auto drained = water_lift.snapshot();
    require(drained.water_lift_upper_catch_latched &&
                drained.water_lift_cage_travel_m > 7.90,
            "drain must not release the player cage from the +8 m pawl");
    const double water_total_drained =
        drained.water_screw_basin_volume_m3 + drained.water_screw_tank_volume_m3 +
        drained.water_lift_bucket_water_m3;
    require(std::abs(water_total_drained - water_total_start) < 1.0e-8,
            "tank -> bucket -> basin cycle must conserve the same water inventory");

    require(walk_to(water_lift, -9.45, -108.20, 5.0, 0.18),
            "player must step from caught cage onto the fixed +8 m dock");
    require(water_lift.advance_frame(0.5).accepted, "upper dock stance must settle");
    const auto on_dock = water_lift.snapshot();
    require(on_dock.player_grounded &&
                on_dock.support_entity_id == Simulation::kGroundWaterLiftFrameEntityId,
            "the +8 m handoff must be fixed structural support, not the moving cage");
    // The reset pedestal itself is solid. The dock stance already lies
    // within the control's reach; walking into its centre is not a legal
    // navigation target.
    require(water_lift.snapshot().water_lift_reset_station_active,
            "upper reset control must be physically in reach");
    require(water_lift.request_water_lift_reset(), "water-lift reset request accepted");
    require(advance_until(
                water_lift,
                [](const Snapshot &s) {
                    return s.water_lift_cage_travel_m < 0.08 &&
                           s.water_lift_bucket_catch_latched;
                },
                14.0),
            "empty bucket must rise back to its top catch as the cage resets");

    const auto reset_lift = water_lift.snapshot();
    require(reset_lift.water_lift_bucket_water_m3 < 0.01 &&
                reset_lift.water_lift_cage_travel_m < 0.08,
            "reset must finish with an empty caught bucket and cage back at grade");

    // Continue the actual lift ride through its fixed dock and the new
    // transfer span. The cross brace is a real obstacle on the bridge; use the
    // existing jump, brake in air, land on that same static compound, then
    // walk onto MOD-STAIR-A and up its already-proven next flight.
    require(walk_to(water_lift, -8.55, -110.85, 4.0, 0.10),
            "the dock must feed the near end of the transfer span");
    const auto on_span = water_lift.snapshot();
    require(on_span.player_grounded &&
                on_span.support_entity_id == Simulation::kGroundWaterLiftFrameEntityId &&
                on_span.player_position.y > 9.0,
            "the grating must support the player above the gate");
    (void)walk_toward(water_lift, -8.55, -114.0, 1.0);
    const auto at_brace = water_lift.snapshot();
    require(at_brace.player_grounded &&
                at_brace.support_entity_id == Simulation::kGroundWaterLiftFrameEntityId &&
                at_brace.player_position.z > -111.70 &&
                at_brace.player_position.z < -111.40,
            "the visible cross brace must obstruct an ordinary walk on the grating");
    require(water_lift.set_facing(0.0, -1.0) &&
                water_lift.set_move_input(0.0, -1.0) &&
                water_lift.request_jump(),
            "the player must be able to jump the solid cross brace");
    bool landed_on_span = false;
    bool braked = false;
    for (int tick = 0; tick < 3 * static_cast<int>(Simulation::kTickRateHz); ++tick) {
        const auto s = water_lift.snapshot();
        if (!braked && !s.player_grounded && s.player_position.z < -113.0) {
            require(water_lift.set_move_input(0.0, 0.0),
                    "air control must accept a braking input");
            braked = true;
        }
        require(water_lift.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "dock transfer jump tick must advance");
        const auto after = water_lift.snapshot();
        if (braked && after.player_grounded &&
            after.support_entity_id == Simulation::kGroundWaterLiftFrameEntityId &&
            after.player_position.z < -112.5) {
            landed_on_span = true;
            break;
        }
    }
    require(landed_on_span,
            "jump over the brace must settle on visible grating, not a hidden support");
    require(walk_to(water_lift, -8.55, -115.5, 4.0, 0.15),
            "the span must join the existing +8 m stair landing");
    require(water_lift.advance_frame(0.3).accepted,
            "the stair handoff must settle under ordinary contact");
    require(water_lift.snapshot().player_grounded &&
                water_lift.snapshot().support_entity_id == Simulation::kIntakeStairEntityId,
            "the player must stand on the legal MOD-STAIR-A support");
    require(walk_to(water_lift, 7.85, -116.0, 9.0, 0.16),
            "the existing stair flight must carry the player to its next landing");
    require(water_lift.advance_frame(0.5).accepted,
            "the +12 m stair landing must hold a stable stance");
    const auto dock_stair_top = water_lift.snapshot();
    require(dock_stair_top.player_grounded &&
                dock_stair_top.support_entity_id == Simulation::kIntakeStairEntityId &&
                dock_stair_top.player_position.y > 12.95 &&
                dock_stair_top.player_position.y < 13.25,
            "the +8 m dock route must terminate on the stable +12 m stair landing");
    std::cout << "PASS scraperx_sim ground water lift: cage_travel="
              << lift_top.water_lift_cage_travel_m
              << " peak_speed=" << lift_top.water_lift_cage_peak_speed_mps
              << " rope_N=" << lift_peak_rope_tension
              << " bucket_kg=" << lift_top.water_lift_bucket_mass_kg
              << " dock_y=" << on_dock.player_position.y
              << " conserved_err=" << std::abs(water_total_drained - water_total_start)
              << " dry_fail=1 partial_fail=1 rope_fail=1 overload_fail=1"
              << " moving_support=1 reset=1 dock_to_stair12=1" << '\n';

    // One uninterrupted player run closes the producer/receiver seam. No
    // tank seed, cage placement, catch override, or teleport is used here.
    Simulation screw_ascent(InitialSpawn::WaterScrewStation, scraperx::sim::WorldContent::RegressionFixtures);
    require(screw_ascent.advance_frame(0.5).accepted,
            "continuous ascent screw-station settle must advance");
    const auto ascent_start = screw_ascent.snapshot();
    const double ascent_water_total =
        ascent_start.water_screw_basin_volume_m3 +
        ascent_start.water_screw_tank_volume_m3 +
        ascent_start.water_lift_bucket_water_m3;
    require(ascent_start.water_screw_station_active &&
                screw_ascent.request_water_screw_toggle(),
            "player must start the ground screw at its real station");
    require(advance_until(
                screw_ascent,
                [](const Snapshot &s) { return s.water_screw_tank_volume_m3 >= 1.999; },
                125.0),
            "running screw must supply the same tank used by the lift");
    require(screw_ascent.request_water_screw_toggle() &&
                screw_ascent.advance_frame(0.2).accepted,
            "player must be able to stop the screw before leaving its station");
    require(walk_to(screw_ascent, -14.45, -106.20, 8.0, 0.18),
            "player must walk from the screw to the bucket fill valve");
    require(screw_ascent.advance_frame(0.3).accepted &&
                screw_ascent.snapshot().water_lift_valve_station_active,
            "the walked-to valve must be physically in reach");
    fill_bucket(screw_ascent, 2.0);
    board_lift_cage(screw_ascent);
    require(screw_ascent.request_water_lift_release(),
            "boarded player must release the actual bucket catch");
    bool ascent_supported_on_moving_cage = false;
    bool ascent_caught = false;
    for (std::uint32_t tick = 0; tick < 12 * Simulation::kTickRateHz; ++tick) {
        require(screw_ascent.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "continuous player ascent tick must advance");
        const auto s = screw_ascent.snapshot();
        ascent_supported_on_moving_cage =
            ascent_supported_on_moving_cage ||
            (s.support_entity_id == Simulation::kGroundWaterLiftCageEntityId &&
             s.water_lift_cage_travel_m > 0.25);
        if (s.water_lift_upper_catch_latched) {
            ascent_caught = true;
            break;
        }
    }
    require(ascent_caught && ascent_supported_on_moving_cage &&
                screw_ascent.snapshot().water_lift_cage_travel_m > 7.90,
            "screw-fed bucket must physically carry its player to +8 m");
    require(walk_to(screw_ascent, -9.45, -108.20, 5.0, 0.18) &&
                screw_ascent.advance_frame(0.5).accepted,
            "player must step off the screw-fed lift onto its fixed dock");
    const auto ascent_dock = screw_ascent.snapshot();
    require(ascent_dock.player_grounded &&
                ascent_dock.support_entity_id == Simulation::kGroundWaterLiftFrameEntityId,
            "continuous screw ascent must end on fixed +8 m support");
    const double ascent_water_end =
        ascent_dock.water_screw_basin_volume_m3 +
        ascent_dock.water_screw_tank_volume_m3 +
        ascent_dock.water_lift_bucket_water_m3;
    require(std::abs(ascent_water_end - ascent_water_total) < 1.0e-8,
            "continuous screw ascent must conserve basin, tank, and bucket water");
    std::cout << "PASS scraperx_sim continuous ground screw ascent: dock_y="
              << ascent_dock.player_position.y
              << " cage_travel=" << ascent_dock.water_lift_cage_travel_m
              << " conserved_err=" << std::abs(ascent_water_end - ascent_water_total)
              << " moving_support=1 no_seed=1" << '\n';

    // ---- AS-006 Stage A, the skip lift (03_EXECUTION/ASCENT/AS-006_CW_PIN.md)
    //
    // No link, no lift: as found, the rope's end is made fast on the bollard.
    // Take the trip handle and keep pulling past the lever's stop: the catch
    // lets go, the bollard holds the skip within its 0.03 m of slack, and the
    // cage never moves. Let go: the lever falls back, the catch seats the
    // skip again and takes the load off the rope, and the rope's end comes
    // off the bollard onto the cage as before -- the stage is not stranded.
    Simulation unlinked(InitialSpawn::StairTop, scraperx::sim::WorldContent::RegressionFixtures);
    require(unlinked.advance_frame(1.0).accepted, "the stair-top settle interval must be accepted");
    const auto well_stair_top = unlinked.snapshot();
    require(well_stair_top.player_grounded && well_stair_top.player_position.y > 154.8 &&
                well_stair_top.player_position.y < 155.0,
            "StairTop must stand the player on the stair's 154 m top deck");
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
    Simulation well(InitialSpawn::StairTop, scraperx::sim::WorldContent::RegressionFixtures);
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

    // Dying restores the last commit, machines included. Stage B now occupies
    // the east handoff beside A, so walking east can legitimately advance the
    // player's automatic checkpoint onto B before the eventual fall. The
    // invariant here is therefore the machine commit, not which adjacent
    // support owns the latest player checkpoint: A must still restore parked
    // at the top with its rope on the cage eye.
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
                std::abs(well_restored.well_a_cage_travel - kWellATravel) <= 0.05 &&
                well_restored.well_a_rope_end_entity_id == Simulation::kWellACageEntityId,
            "death restore must preserve Stage A's committed machine state");
    std::cout << "PASS scraperx_sim AS-006 A ride: ride_s=" << well_ride_seconds
              << " floor_y=" << floor_y << " peak_speed=" << at_top.well_a_cage_peak_speed
              << " rode_on_cage=" << int(rode_on_cage) << " skip_released_J=" << skip_released
              << " payload_gained_J=" << payload_gained
              << " energy_margin_J=" << worst_energy_margin
              << " restored_y=" << well_restored.player_position.y
              << " restored_support=" << well_restored.support_entity_id << '\n';


    // ---- AS-006 Stage B, guided lattice counterweight ----------------------
    require(board_well_b(well), "the rider must step directly from A's parked cage into B's cage");
    require(well.snapshot().well_b_catch_latched &&
                well.snapshot().well_b_rope_end_entity_id == Simulation::kWellBFrameEntityId,
            "Stage B must be found caught with its rope made fast on the bollard");

    const double b_unlinked_weight_y0 = kit_y(well, Simulation::kWellBCounterweightEntityId);
    const double b_unlinked_cage_y0 = kit_y(well, Simulation::kWellBCageEntityId);
    require(pull_well_b(well, 0.55, 3.0),
            "Stage B's trip handle must withdraw the counterweight catch");
    double b_unlinked_cage_worst = 0.0;
    double b_unlinked_weight_drop = 0.0;
    double b_bollard_tension = 0.0;
    for (std::uint32_t tick = 0; tick < 90 * 4; ++tick) {
        (void)well.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = well.snapshot();
        b_unlinked_cage_worst =
            std::max(b_unlinked_cage_worst,
                     std::abs(kit_y(well, Simulation::kWellBCageEntityId) - b_unlinked_cage_y0));
        b_unlinked_weight_drop =
            std::max(b_unlinked_weight_drop,
                     b_unlinked_weight_y0 - kit_y(well, Simulation::kWellBCounterweightEntityId));
        if (!state.well_b_catch_latched)
            b_bollard_tension = std::max(b_bollard_tension, state.well_b_rope_tension_n);
    }
    const auto b_relatched = well.snapshot();
    require(b_unlinked_cage_worst <= 0.05,
            "Stage B no link, no lift: the cage must remain parked");
    require(b_unlinked_weight_drop <= 0.05,
            "Stage B's bollard must arrest the lattice within its declared slack");
    require(b_bollard_tension > 0.9 * kWellBCounterweightMassKg * kGravity,
            "Stage B's bollard must carry the released lattice through the rope");
    require(b_relatched.well_b_catch_latched && b_relatched.well_b_rope_tension_n < 60.0,
            "letting go must re-seat B's lattice and unload its rope");
    std::cout << "PASS scraperx_sim AS-006 B no link: cage_worst="
              << b_unlinked_cage_worst << " lattice_drop=" << b_unlinked_weight_drop
              << " bollard_tension=" << b_bollard_tension << " relatched=1\n";

    require(rig_well_b(well),
            "after the wasted pull, B's rope must come off the bollard and hook to its cage");
    const double b_weight_y0 = kit_y(well, Simulation::kWellBCounterweightEntityId);
    const double b_cage_y0 = kit_y(well, Simulation::kWellBCageEntityId);
    const double b_rider_y0 = well.snapshot().player_position.y;
    require(pull_well_b(well, 0.55, 3.0),
            "with the rope on the cage, B's trip line must release the lattice");
    const double b_ride_start = well.snapshot().simulation_time_seconds;
    bool b_rode_on_cage = true;
    double b_ride_seconds = 0.0;
    double b_ride_peak_speed = 0.0;
    double b_worst_energy_margin = std::numeric_limits<double>::infinity();
    const auto b_cage_index = well.kit_body_index(Simulation::kWellBCageEntityId);
    require(b_cage_index != Simulation::kKitNone,
            "Stage B's cage must remain addressable through the mechanism kit");
    for (std::uint32_t tick = 0; tick < 90 * 20; ++tick) {
        (void)well.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = well.snapshot();
        const auto cage_velocity = well.kit_body_velocity(b_cage_index);
        b_ride_peak_speed = std::max(b_ride_peak_speed, std::abs(cage_velocity.y));
        const double cage_rise = kit_y(well, Simulation::kWellBCageEntityId) - b_cage_y0;
        if (cage_rise > 0.05 && cage_rise < kWellBTravel - 0.05) {
            b_rode_on_cage = b_rode_on_cage && state.player_grounded &&
                             state.support_entity_id == Simulation::kWellBCageEntityId;
        }
        const double released =
            kWellBCounterweightMassKg * kGravity *
            (b_weight_y0 - kit_y(well, Simulation::kWellBCounterweightEntityId));
        const double gained = kWellBCageMassKg * kGravity * cage_rise +
                              kRiderMassKg * kGravity * (state.player_position.y - b_rider_y0);
        b_worst_energy_margin = std::min(b_worst_energy_margin, released - gained);
        if (b_ride_seconds == 0.0 && state.well_b_cage_travel >= kWellBTravel - 0.01)
            b_ride_seconds = state.simulation_time_seconds - b_ride_start;
    }
    const auto b_top = well.snapshot();
    const double b_floor_y = kit_y(well, Simulation::kWellBCageEntityId) + 0.10;
    const double b_lattice_top =
        kit_y(well, Simulation::kWellBCounterweightEntityId) + kWellBCounterweightHalfHeight;
    require(b_rode_on_cage, "Stage B's rider must inherit the cage motion for the whole lift");
    require(std::abs(b_floor_y - (kWellBCageFloorTop + kWellBTravel)) <= 0.05,
            "Stage B's cage floor must stop flush with the 198.25 m ring");
    // The guide's snapshot peak is lifetime-wide and can include the player's
    // earlier Stage A handoff/death contact with this already-existing cage.
    // Governor proof is ride-scoped: measure the cage's real body velocity
    // only after B's catch has released and the powered lift has begun.
    std::cout << "INFO AS-006 B governor: ride_peak=" << b_ride_peak_speed
              << " lifetime_peak=" << b_top.well_b_cage_peak_speed
              << " ride_s=" << b_ride_seconds
              << " floor_y=" << b_floor_y << '\n';
    require(b_ride_peak_speed <= 3.1,
            "Stage B's brake-only governor must hold the powered ride to 3.0 m/s");
    require(b_top.player_grounded && b_top.support_entity_id == Simulation::kWellBCageEntityId &&
                b_top.player_position.y > 199.0,
            "Stage B must arrive with the rider standing in its cage at the 198 ring");
    require(b_worst_energy_margin >= -kStanceJitterJ,
            "Stage B may not give the payload more potential energy than the lattice released");
    require(std::abs(b_lattice_top - 198.25) <= 0.05,
            "the spent 22 m lattice must finish spanning 176.25..198.25 as real structure");
    std::cout << "PASS scraperx_sim AS-006 B ride: ride_s=" << b_ride_seconds
              << " floor_y=" << b_floor_y
              << " peak_speed=" << b_ride_peak_speed
              << " lifetime_peak_speed=" << b_top.well_b_cage_peak_speed
              << " rode_on_cage=" << int(b_rode_on_cage)
              << " energy_margin_J=" << b_worst_energy_margin
              << " lattice_top_y=" << b_lattice_top << '\n';

    return EXIT_SUCCESS;
}
