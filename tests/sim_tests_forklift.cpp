// The yard's forklift (src/sim/forklift.hpp): parked it stays put; at its
// wheel the player takes the controls, drives and steers it, slides its forks
// under a pallet of drums, lifts it, carries it and sets it down elsewhere;
// the same inputs give the same end.
#include "sim/forklift.hpp"
#include "sim/loose_stock.hpp"
#include "sim_tests.hpp"

#include <array>
#include <cmath>
#include <iostream>

using scraperx::sim::Forklift;
using scraperx::sim::LooseStock;
using scraperx::sim::Simulation;

namespace {

scraperx::sim::Vector3 body_at(const Simulation &s, const std::uint64_t entity) {
    return s.kit_body_position(s.kit_body_index(entity));
}

double truck_yaw(const Simulation &s) {
    const auto q = s.kit_body_rotation(s.kit_body_index(Forklift::kChassisEntity));
    return 2.0 * std::atan2(q.y, q.w);
}

void hold(Simulation &s, const double x, const double z, const double seconds) {
    const auto ticks = static_cast<int>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (int i = 0; i < ticks; ++i) {
        (void)s.set_move_input(x, z);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
}

// From the game's start: round the beam stack's west end, onto the truck's
// floor from its west side, to the wheel, facing its forward (+z).
bool board(Simulation &s) {
    if (!(walk_to(s, 30.0, 10.0, 30.0) && walk_to(s, 36.4, 16.35, 20.0) &&
          walk_to(s, Forklift::kParkX, 16.35, 8.0, 0.1))) {
        return false;
    }
    (void)s.set_facing(0.0, 1.0);
    (void)s.advance_frame(0.5);
    return s.forklift_state().at_controls;
}

// The pallet of drums lifted and carried back 5 m, from a fresh start;
// returns the truck's, the pallet's and the first drum's end positions.
std::array<double, 6> fork_the_pallet(Simulation &s, const int pallet) {
    require(board(s) && s.request_forklift_action(), "forklift: aboard, at the wheel, the controls taken");
    (void)s.advance_frame(0.2);
    // Creep in until the forks are under the pallet's deck, the carriage
    // short of it.
    const double stop_z = body_at(s, LooseStock::kFirstEntity + static_cast<std::uint64_t>(pallet)).z - 1.75;
    int ticks = 0;
    while (body_at(s, Forklift::kChassisEntity).z < stop_z && ticks++ < 900) {
        (void)s.set_move_input(0.0, 0.25);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0.0, 0.0);
    (void)s.advance_frame(1.5);
    (void)s.set_forklift_lift(1.0);
    (void)s.advance_frame(1.0);
    (void)s.set_forklift_lift(0.0);
    (void)s.advance_frame(0.5);
    const auto lifted = body_at(s, LooseStock::kFirstEntity + static_cast<std::uint64_t>(pallet));
    std::cout << "  forklift: the pallet lifted to y " << lifted.y << ", the forks " << s.forklift_state().lift_m
              << " m up\n";
    require(lifted.y > 0.3, "forklift: the forks under the pallet lift it, its drums on it");
    hold(s, 0.0, -0.4, 2.5);
    (void)s.advance_frame(1.5);
    (void)s.set_forklift_lift(-1.0);
    (void)s.advance_frame(1.5);
    (void)s.set_forklift_lift(0.0);
    hold(s, 0.0, -0.4, 1.5);
    (void)s.advance_frame(1.5);
    const auto truck = body_at(s, Forklift::kChassisEntity);
    const auto pallet_end = body_at(s, LooseStock::kFirstEntity + static_cast<std::uint64_t>(pallet));
    const auto drum_end = body_at(s, LooseStock::kFirstEntity + static_cast<std::uint64_t>(pallet + 1));
    return {truck.x, truck.z, pallet_end.x, pallet_end.z, drum_end.y, drum_end.z};
}

} // namespace

void run_forklift() {
    // Parked, it stays put on its wheels.
    {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        const auto parked = body_at(s, Forklift::kChassisEntity);
        (void)s.advance_frame(10.0);
        const auto after = body_at(s, Forklift::kChassisEntity);
        const double crept = std::hypot(after.x - parked.x, after.z - parked.z);
        std::cout << "  forklift: parked 10 s, it moved " << crept << " m, its frame at y " << after.y << "\n";
        require(crept < 0.01 && std::abs(after.y) < 0.05, "forklift: parked, it stands on its wheels and stays put");
        require(!s.forklift_state().at_controls && !s.request_forklift_action(),
                "forklift: off the truck, its controls are not offered");
    }
    // Driven: forward, then forward and right; let go of, it stops.
    {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        (void)s.advance_frame(1.0);
        require(board(s), "forklift: on its floor at the wheel, DRIVE is offered");
        require(s.request_forklift_action(), "forklift: at the wheel the controls are taken");
        (void)s.advance_frame(0.2);
        require(s.forklift_state().driving, "forklift: driving");
        const auto start = body_at(s, Forklift::kChassisEntity);
        hold(s, 0.0, 1.0, 2.0);
        const auto ahead = body_at(s, Forklift::kChassisEntity);
        const double peak = s.forklift_state().speed_mps;
        // Facing +z, the view's right is -x.
        hold(s, -0.6, 0.8, 2.0);
        const double yaw = truck_yaw(s);
        (void)s.advance_frame(2.0);
        const auto snap = s.snapshot();
        std::cout << "  forklift: driven " << ahead.z - start.z << " m in 2 s (" << peak << " m/s), turned right to yaw "
                  << yaw << ", stopped at " << s.forklift_state().speed_mps << " m/s\n";
        require(ahead.z - start.z > 1.5 && std::abs(ahead.x - start.x) < 0.1, "forklift: the stick forward drives it forward");
        require(yaw < -0.3, "forklift: the stick right turns it right");
        require(std::abs(s.forklift_state().speed_mps) < 0.05, "forklift: the stick let go, it stops");
        require(snap.support_entity_id == Forklift::kChassisEntity && snap.death_count == 0,
                "forklift: the driver rides on its floor");
        require(s.request_forklift_action(), "forklift: Action lets go of the controls");
        (void)s.advance_frame(0.2);
        require(!s.forklift_state().driving, "forklift: let go, it is parked");
    }
    // The pallet of drums: forked, lifted, carried back and set down, the
    // drums still on it; twice, alike to the bit.
    const auto &table = LooseStock::placements();
    int pallet = -1;
    for (std::size_t i = 0; i < table.size() && pallet < 0; ++i) {
        if (table[i].kind == LooseStock::Kind::Pallet) {
            pallet = static_cast<int>(i);
        }
    }
    require(pallet >= 0, "forklift: the drums' first pallet");
    std::array<double, 6> runs[2];
    for (auto &run : runs) {
        Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
        (void)s.advance_frame(1.0);
        const auto before = body_at(s, LooseStock::kFirstEntity + static_cast<std::uint64_t>(pallet));
        run = fork_the_pallet(s, pallet);
        const double carried = before.z - run[3];
        std::cout << "  forklift: the pallet carried " << carried << " m and set down, its first drum at y " << run[4]
                  << "\n";
        require(carried > 3.0, "forklift: the pallet is carried back and set down elsewhere");
        require(run[4] > 0.45 && std::abs(run[5] - run[3]) < 0.5, "forklift: its drums ride on it and stand on it");
    }
    require(runs[0] == runs[1], "forklift: the same inputs end the same, bit for bit");
    std::cout << "PASS scraperx_sim forklift: carried_m=" << table[static_cast<std::size_t>(pallet)].z - runs[0][3] << "\n";
}
