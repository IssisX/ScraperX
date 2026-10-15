#include "intake_geometry.hpp"
#include "test_suites.hpp"
#include "test_utilities.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

namespace scraperx::tests {

namespace {

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
    using scraperx::sim::Simulation;
    // Either entry can leave the real dropped bar across the rack-to-door
    // lane. Read its current long axis, rather than assuming a particular
    // set-down pose or choosing Jump from the entry route's identity.
    bool jumped_over_bar = false;
    for (int tick = 0; tick < 4 * 90; ++tick) {
        const auto state = simulation.snapshot();
        if (state.carrying_entity_id != Simulation::kHook5BlockEntityId) {
            return false;
        }
        if (std::hypot(kHook5DoorwayX - state.player_position.x,
                       kHook5MinZ + 1.2 - state.player_position.z) <= 0.15) {
            (void)simulation.set_move_input(0.0, 0.0);
            return walk_to(simulation, kHook5DoorwayX, kHook5MinZ - 2.0, 4.0);
        }
        steer_toward(simulation, kHook5DoorwayX, kHook5MinZ + 1.2);
        const auto bar = state.hook5_bar_position;
        const auto q = state.hook5_bar_rotation;
        const double axis_x = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
        const double axis_z = 2.0 * (q.x * q.z - q.w * q.y);
        const double dx = state.player_position.x - bar.x;
        const double dz = state.player_position.z - bar.z;
        const double along = std::clamp(
            (dx * axis_x + dz * axis_z) /
                std::max(axis_x * axis_x + axis_z * axis_z, 1.0e-6),
            -1.025, 1.025); // native bar half-length
        const double bar_gap = std::hypot(dx - along * axis_x, dz - along * axis_z);
        // 0.35 m capsule radius + bar section + takeoff approach margin.
        // Jump before pushing the bar into the doorway; only from real
        // grounding, once, and on the bar's north side. Native collision
        // still owns doorway-header clearance.
        if (!jumped_over_bar && state.player_grounded && dz > 0.0 && bar_gap < 0.75) {
            (void)simulation.request_jump();
            jumped_over_bar = true;
        }
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    }
    return false;
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

} // namespace

void run_hook5_rack_tests() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;

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
    // Approach sideways with the real load along the face. Facing the
    // waypoint pushes the freely rotating 0.6 m block into the pack: this
    // target has only the nominal block/hand clearance, so its orientation
    // can physically prevent the old position-only walk from arriving.
    // Keep the same target and budget; the following falsifier still turns
    // toward the pack and presses against it with both hands occupied.
    bool carry_reached_pack = false;
    for (std::uint32_t tick = 0; tick < 90 * 10; ++tick) {
        const auto state = cage.snapshot();
        if (state.player_grounded &&
            std::hypot(held_pack.x + 2.0 - state.player_position.x,
                       held_pack.z - state.player_position.z) <= 0.15) {
            carry_reached_pack = true;
            break;
        }
        steer_toward(cage, held_pack.x + 2.0, held_pack.z);
        (void)cage.set_facing(0.0, 1.0);
        (void)cage.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)cage.set_move_input(0.0, 0.0);
    require(carry_reached_pack,
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
    // A position-only arrival still carries almost 5 m/s westward; the
    // subsequent northward press then slides past the rung's west face.
    // Arrive with real slow footing before testing the occupied hands.
    require(walk_to(cage, 13.5, kHook5MinZ - 4.0, 12.0) && walk_to(cage, 13.5, kSkinFootZ, 12.0) &&
                walk_to(cage, kSkinX, kSkinFootZ, 20.0, 0.15, true),
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
    // The fixed walking intervals and horizontal arrival do not identify
    // which physical landing carries the loaded body. Preserve snapshots
    // before the assertions so a platform-specific miss distinguishes lost
    // footing from a released carry constraint. The stair entity groups all
    // six flights; its actual contact height is the footing evidence.
    const auto report_loaded_stair = [&](const char *phase, const int flight,
                                         const double target_x, const double target_z) {
        const auto state = hook.snapshot();
        std::cerr << "INFO AS-003 loaded stair: phase=" << phase << " flight_command=" << flight
                  << " tick=" << state.tick_index << " time=" << state.simulation_time_seconds
                  << " target=" << target_x << ',' << target_z
                  << " pos=" << state.player_position.x << ',' << state.player_position.y
                  << ',' << state.player_position.z
                  << " velocity=" << state.player_linear_velocity.x << ','
                  << state.player_linear_velocity.y << ',' << state.player_linear_velocity.z
                  << " grounded=" << state.player_grounded << " support=" << state.support_entity_id
                  << " contact=" << state.support_contact_point.x << ','
                  << state.support_contact_point.y << ',' << state.support_contact_point.z
                  << " held=" << state.carrying_entity_id
                  << " block=" << state.hook5_block_position.x << ','
                  << state.hook5_block_position.y << ',' << state.hook5_block_position.z
                  << " deaths=" << state.death_count
                  << " traversal=" << static_cast<int>(state.traversal_state)
                  << " traversal_target=" << state.traversal_target_point.x << ','
                  << state.traversal_target_point.y << ',' << state.traversal_target_point.z
                  << " on_handoff=" << (state.support_entity_id == Simulation::kIntakeHandoffEntityId)
                  << " block_held=" << (state.carrying_entity_id == Simulation::kHook5BlockEntityId)
                  << '\n';
    };
    const double to_throat[][2] = {{13.5, kHook5MinZ - 2.0}, {13.5, -79.5}, {2.0, -79.5},
                                   {-1.2, -85.0}, {-1.2, -106.0}};
    for (const auto &point : to_throat) {
        require(walk_to(hook, point[0], point[1], 25.0),
                "the block must be carried round the belt to the throat");
    }
    walk_toward(hook, kThroatX, -107.5, 12.0, true);
    walk_toward(hook, kThroatX, -113.0, 8.0, true);
    // Onto the first flight along its lane, from beyond its foot: a load
    // carried at the belly strikes the rising slab's 1.1 m side edge that an
    // empty-handed body slides along to reach the foot the way AS-001 walks.
    require(walk_to(hook, -8.3, -113.2, 12.0) && walk_to(hook, -8.3, -116.0, 6.0),
            "the carry must come round to the foot of MOD-STAIR-A's first flight");
    report_loaded_stair("foot", 0, -8.3, -116.0);
    for (int flight = 0; flight < 6; ++flight) {
        const double side = (flight % 2 == 0) ? 1.0 : -1.0;
        const double lane = -118.0 + side * 2.0;
        walk_toward(hook, -side * kStairLandingX, lane, 8.0, true);
        report_loaded_stair("turn", flight, -side * kStairLandingX, lane);
        walk_toward(hook, side * kStairLandingX, lane, 14.0, true);
        report_loaded_stair("climb", flight, side * kStairLandingX, lane);
    }
    // Cross the flush overlap along its centre, then turn only after the
    // loaded feet actually settle on the receiver. (-8,-113.5) is 0.7 m past
    // the stair landing's end, on handoff-only footing. Horizontal arrival on
    // any lower deck is a failure. Both legs share the original 12 s budget;
    // Movement retains walk_to's settled PD law. Hold facing per leg: changing
    // heading during braking rotates the real carried load's hand anchor.
    const auto walk_loaded_handoff = [&]() {
        constexpr std::uint32_t budget_ticks = 12 * Simulation::kTickRateHz;
        const auto start_tick = hook.snapshot().tick_index;
        bool receiver_reached = false;
        bool heading_set = false;
        bool reported_support_loss = false;
        bool reported_contact_drop = false;
        bool reported_traversal_change = false;
        const auto report_transfer_state = [](const char *boundary, const Snapshot &state) {
            std::cerr << "INFO AS-003 loaded transfer: boundary=" << boundary
                      << " tick=" << state.tick_index << " time=" << state.simulation_time_seconds
                      << " pos=" << state.player_position.x << ',' << state.player_position.y
                      << ',' << state.player_position.z
                      << " velocity=" << state.player_linear_velocity.x << ','
                      << state.player_linear_velocity.y << ',' << state.player_linear_velocity.z
                      << " grounded=" << state.player_grounded << " support=" << state.support_entity_id
                      << " contact=" << state.support_contact_point.x << ','
                      << state.support_contact_point.y << ',' << state.support_contact_point.z
                      << " held=" << state.carrying_entity_id
                      << " traversal=" << static_cast<int>(state.traversal_state) << '\n';
        };
        for (std::uint32_t tick = 0; tick <= budget_ticks; ++tick) {
            const auto state = hook.snapshot();
            const bool held = state.carrying_entity_id == Simulation::kHook5BlockEntityId;
            const bool high_contact = state.player_grounded &&
                std::abs(state.support_contact_point.y - kHandoffSurfaceY) <= 0.10;
            if (!held || state.player_position.y < kHandoffSurfaceY + 0.70 ||
                (state.player_grounded && !high_contact)) {
                report_transfer_state("rejected_height_or_carry", state);
                break;
            }
            double target_x = receiver_reached ? -9.0 : -8.0;
            double target_z = receiver_reached ? -109.0 : -113.5;
            const bool settled = std::hypot(target_x - state.player_position.x,
                                             target_z - state.player_position.z) <= 0.15 &&
                std::hypot(state.player_linear_velocity.x, state.player_linear_velocity.z) < 0.15;
            if (settled && high_contact &&
                state.support_entity_id == Simulation::kIntakeHandoffEntityId) {
                if (receiver_reached) {
                    (void)hook.set_move_input(0.0, 0.0);
                    std::cerr << "INFO AS-003 loaded transfer: arrived=1 elapsed_ticks="
                              << state.tick_index - start_tick << " budget_ticks=" << budget_ticks << '\n';
                    return true;
                }
                receiver_reached = true;
                heading_set = false;
                report_loaded_stair("handoff_receiver", 6, target_x, target_z);
                target_x = -9.0;
                target_z = -109.0;
            }
            if (tick == budget_ticks) break;
            const double dx = target_x - state.player_position.x;
            const double dz = target_z - state.player_position.z;
            const double ix = dx * 1.8 - state.player_linear_velocity.x * .28;
            const double iz = dz * 1.8 - state.player_linear_velocity.z * .28;
            const double scale = std::max(1.0, std::hypot(ix, iz));
            (void)hook.set_move_input(ix / scale, iz / scale);
            if (!heading_set && std::hypot(dx, dz) > .001) {
                (void)hook.set_facing(dx, dz);
                heading_set = true;
            }
            const bool accepted = hook.advance_frame(Simulation::kFixedStepSeconds).accepted;
            const auto after = hook.snapshot();
            const bool support_loss = !reported_support_loss && state.player_grounded &&
                (!after.player_grounded || (after.support_entity_id != Simulation::kIntakeStairEntityId &&
                 after.support_entity_id != Simulation::kIntakeHandoffEntityId));
            const bool contact_drop = !reported_contact_drop && after.player_grounded &&
                after.support_contact_point.y < kHandoffSurfaceY - 0.10;
            const bool traversal_change = !reported_traversal_change &&
                state.traversal_state != after.traversal_state;
            if (support_loss || contact_drop || traversal_change) {
                std::cerr << "INFO AS-003 loaded transfer: first_support_loss=" << support_loss
                          << " first_contact_drop=" << contact_drop
                          << " first_traversal_change=" << traversal_change
                          << " target=" << target_x << ',' << target_z
                          << " input=" << ix / scale << ',' << iz / scale << '\n';
                report_transfer_state("previous", state);
                report_transfer_state("after", after);
            }
            reported_support_loss = reported_support_loss || support_loss;
            reported_contact_drop = reported_contact_drop || contact_drop;
            reported_traversal_change = reported_traversal_change || traversal_change;
            if (!accepted) break;
        }
        (void)hook.set_move_input(0.0, 0.0);
        std::cerr << "INFO AS-003 loaded transfer: arrived=0 receiver_reached=" << receiver_reached
                  << " elapsed_ticks=" << hook.snapshot().tick_index - start_tick
                  << " budget_ticks=" << budget_ticks << '\n';
        return false;
    };
    const bool carry_reached_handoff = walk_loaded_handoff();
    report_loaded_stair("handoff", 6, -9.0, -109.0);
    std::cerr << "INFO AS-003 loaded stair: waypoint_reached=" << carry_reached_handoff << '\n';
    require(carry_reached_handoff, "the carry must reach the handoff deck");
    require(hook.snapshot().support_entity_id == Simulation::kIntakeHandoffEntityId &&
                hook.snapshot().carrying_entity_id == Simulation::kHook5BlockEntityId,
            "MOD-STAIR-A must carry the body and the block to the +24 m handoff deck");
    walk_toward(hook, -6.0, -112.5, 25.0, true, true);
    // The loaded body stops at the rounded foot before reaching the ramp.
    // Jump onto it, then duck under rung 17 after actual takeoff. Carry
    // sideways so the block does not lead the feet into the rising slab.
    (void)hook.request_jump();
    bool hook_crouch_started = false;
    bool hook_crouch_released = false;
    for (int i = 0; i < 20 * 90; ++i) {
        const auto state = hook.snapshot();
        if (!hook_crouch_started && !state.player_grounded) {
            (void)hook.set_crouch_input(true);
            hook_crouch_started = true;
        }
        if (!hook_crouch_released && state.player_position.x > -5.35) {
            (void)hook.set_crouch_input(false);
            hook_crouch_released = true;
        }
        double dx = kLegalFortyHingeX - state.player_position.x;
        double dz = kLegalFortyHingeZ - state.player_position.z;
        const double len = std::hypot(dx, dz);
        if (len > 1.0e-6) {
            dx /= len;
            dz /= len;
        }
        (void)hook.set_move_input(dx, dz);
        (void)hook.set_facing(0.0, -1.0);
        (void)hook.advance_frame(Simulation::kFixedStepSeconds);
    }
    walk_toward(hook, kLegalFortyMidLandingX - 0.85, kLegalFortyHingeZ, 10.0, true, true);
    walk_toward(hook, kLegalFortyMidLandingX, kLegalFortyMidLandingZ, 10.0, true, true);
    walk_toward(hook, kLegalFortyMidLandingX, kLegalFortyUpperFlightZ, 10.0, true, true);
    // Clear the identical upper foot with ordinary Jump while still carrying.
    (void)hook.request_jump();
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
        (void)hook.set_facing(0.0, -1.0);
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
}

} // namespace scraperx::tests
