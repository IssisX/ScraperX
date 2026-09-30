#include "sim/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

using scraperx::sim::Simulation;
using scraperx::sim::InitialSpawn;

namespace {
void advance(Simulation &simulation, const double seconds) {
    for (int tick = 0; tick < static_cast<int>(seconds * 90.0); ++tick) {
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    }
}

double angle(const Simulation &simulation) {
    const auto q = simulation.kit_body_rotation(simulation.kit_body_index(2800));
    return 2.0 * std::atan2(q.z, q.w);
}

double ballast_position(const Simulation &simulation) {
    const auto p = simulation.kit_body_position(simulation.kit_body_index(2801));
    const auto beam = simulation.kit_body_position(simulation.kit_body_index(2800));
    const double q = angle(simulation);
    return (p.x - beam.x) * std::cos(q) + (p.y - beam.y) * std::sin(q);
}

bool walk_to(Simulation &simulation, const double x, const double z,
             const double seconds = 12.0, const double pace = 1.0) {
    for (int tick = 0; tick < static_cast<int>(seconds * 90.0); ++tick) {
        const auto p = simulation.snapshot().player_position;
        const double dx = x - p.x;
        const double dz = z - p.z;
        const double d = std::hypot(dx, dz);
        if (d < 0.12) {
            (void)simulation.set_move_input(0, 0);
            return true;
        }
        const double scale = std::min(1.0, d / 0.6) / d;
        (void)simulation.set_move_input(dx * scale * pace, dz * scale * pace);
        (void)simulation.set_facing(dx / d, dz / d);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)simulation.set_move_input(0, 0);
    return false;
}

void report(const Simulation &simulation, const char *label) {
    const auto p = simulation.snapshot();
    std::cout << label << " angle=" << angle(simulation) << " player="
              << p.player_position.x << "," << p.player_position.y << ","
              << p.player_position.z << " support=" << p.support_entity_id
              << " grounded=" << p.player_grounded << " deaths=" << p.death_count
              << "\n";
}
} // namespace

int main() {
    Simulation simulation;
    std::cout << "INVENTORY moving=" << simulation.moving_body_count()
              << " kit=" << simulation.kit_body_count() << "\n";
    if (simulation.entity_body_count(2800) != 1) {
        std::cerr << "FAIL active route has no player-loaded teeter body\n";
        return 1;
    }
    advance(simulation, 3.0);
    if (angle(simulation) < 0.07 || angle(simulation) > 0.13) {
        std::cerr << "FAIL unloaded teeter did not settle against its upper stop: "
                  << angle(simulation) << "\n";
        return 2;
    }
    // A rider alone must be able to inspect the outboard end without
    // operating the machine. The captive ballast supplies the missing
    // moment only when the player has moved it along the visible rail.
    Simulation no_ballast(InitialSpawn::TeeterEntry);
    advance(no_ballast, 0.5);
    if (!walk_to(no_ballast, 27.2, -139.0) ||
        !walk_to(no_ballast, 32.8, -139.0)) return 22;
    advance(no_ballast, 3.0);
    report(no_ballast, "RIDER_ALONE");
    if (no_ballast.snapshot().support_entity_id != 2800 ||
        angle(no_ballast) < 0.02) {
        std::cerr << "FAIL rider alone tipped the unadjusted beam\n";
        return 23;
    }
    Simulation rider(InitialSpawn::TeeterEntry);
    advance(rider, 0.5);
    report(rider, "ENTRY");
    if (rider.snapshot().support_entity_id != Simulation::kTowerEntityId ||
        !walk_to(rider, 27.2, -139.0)) return 3;
    advance(rider, 0.5);
    report(rider, "OPPOSITE");
    if (rider.snapshot().support_entity_id != 2800 || angle(rider) < 0.07 ||
        !walk_to(rider, 27.8, -139.45)) return 4;
    (void)rider.set_facing(1.0, 0.0);
    advance(rider, 0.2);
    std::cout << "BALLAST_READY position=" << ballast_position(rider) << "\n";
    if (!walk_to(rider, 29.8, -139.45)) return 25;
    advance(rider, 0.5);
    report(rider, "NEAR");
    if (rider.snapshot().support_entity_id != 2800 || angle(rider) < 0.05 ||
        !walk_to(rider, 31.95, -139.45)) return 5;
    std::cout << "BALLAST_MOVED position=" << ballast_position(rider) << "\n";
    if (ballast_position(rider) < 4.15) return 26;
    if (!walk_to(rider, 32.8, -138.58)) return 27;
    advance(rider, 4.0);
    report(rider, "FAR");
    const double far_angle = angle(rider);
    const auto far_state = rider.snapshot();
    if (!rider.snapshot().player_grounded || rider.snapshot().support_entity_id != 2800 ||
        angle(rider) > -0.40 || angle(rider) < -0.55 ||
        !walk_to(rider, 34.4, -139.0)) return 6;
    advance(rider, 4.0);
    report(rider, "UNLOADED");
    if (!rider.snapshot().player_grounded || rider.snapshot().support_entity_id != 1800 ||
        angle(rider) < 0.07 || !walk_to(rider, 34.72, -139.45)) return 7;
    (void)rider.set_facing(0.0, -1.0);
    advance(rider, 0.4);
    report(rider, "GRIP_READY");
    if (!rider.snapshot().grip_available) return 8;
    (void)rider.request_traversal();
    advance(rider, 0.5);
    report(rider, "GRIP_TAKEN");
    (void)rider.set_move_input(0.0, -1.0);
    for (int tick = 0; tick < 1800; ++tick) {
        (void)rider.advance_frame(Simulation::kFixedStepSeconds);
        if (tick % 180 == 179) report(rider, "CLIMB_STEP");
        if (rider.snapshot().player_position.y > 77.5 ||
            rider.snapshot().death_count > 0) break;
    }
    (void)rider.set_move_input(0.0, 0.0);
    advance(rider, 1.0);
    report(rider, "UPPER_EXIT");
    if (!rider.snapshot().player_grounded || rider.snapshot().player_position.y < 77.5 ||
        rider.snapshot().support_entity_id != 1800 || rider.snapshot().death_count != 0 ||
        !walk_to(rider, 25.1, -140.8, 20.0)) return 9;
    advance(rider, 0.5);
    report(rider, "RING_77");
    if (!rider.snapshot().player_grounded || rider.snapshot().player_position.y < 77.5 ||
        rider.snapshot().support_entity_id != Simulation::kTowerEntityId ||
        rider.snapshot().death_count != 0) return 10;
    Simulation drop(InitialSpawn::TeeterFarDrop);
    double drop_min_angle = angle(drop);
    double angle_after_one_second = 0.0;
    for (int tick = 0; tick < 720; ++tick) {
        (void)drop.advance_frame(Simulation::kFixedStepSeconds);
        drop_min_angle = std::min(drop_min_angle, angle(drop));
        if (tick == 89) angle_after_one_second = angle(drop);
    }
    report(drop, "LANDING");
    std::cout << "LANDING_SWEEP one_second=" << angle_after_one_second
              << " minimum=" << drop_min_angle << "\n";
    if (!drop.snapshot().player_grounded || drop.snapshot().support_entity_id != 2800 ||
        angle_after_one_second > -0.05 || drop_min_angle > -0.40 ||
        angle(drop) > -0.40 || angle(drop) < -0.55 ||
        drop.snapshot().death_count != 0) return 11;
    Simulation recovery(InitialSpawn::TeeterEntry);
    advance(recovery, 0.5);
    if (!walk_to(recovery, 30.5, -139.0)) return 18;
    (void)walk_to(recovery, 30.5, -140.5, 4.0);
    advance(recovery, 3.0);
    report(recovery, "FALL_RECOVERY");
    if (!recovery.snapshot().player_grounded ||
        recovery.snapshot().support_entity_id != 1800 ||
        recovery.snapshot().player_position.y < 55.5 ||
        recovery.snapshot().death_count != 0) return 19;
    if (!walk_to(recovery, 25.1, -140.9)) return 20;
    advance(recovery, 0.5);
    report(recovery, "RECOVERY_RING_55");
    if (!recovery.snapshot().player_grounded ||
        recovery.snapshot().support_entity_id != Simulation::kTowerEntityId ||
        std::abs(recovery.snapshot().player_position.y - 55.9) > 0.1 ||
        recovery.snapshot().death_count != 0) return 21;
    Simulation depart(InitialSpawn::TeeterFarDrop);
    bool jumped_from_motion = false;
    for (int tick = 0; tick < 180; ++tick) {
        (void)depart.advance_frame(Simulation::kFixedStepSeconds);
        const auto contact = depart.snapshot();
        if (!contact.player_grounded || contact.support_entity_id != 2800 ||
            std::abs(contact.support_point_linear_velocity.y) < 0.5 ||
            std::abs(contact.player_linear_velocity.x -
                     contact.support_point_linear_velocity.x) > 0.15) continue;
        std::cout << "MOVING_SUPPORT velocity="
                  << contact.support_point_linear_velocity.x << ","
                  << contact.support_point_linear_velocity.y
                  << " player_velocity=" << contact.player_linear_velocity.x << ","
                  << contact.player_linear_velocity.y << "\n";
        (void)depart.request_jump();
        (void)depart.advance_frame(Simulation::kFixedStepSeconds);
        const auto airborne = depart.snapshot();
        std::cout << "DEPARTURE velocity=" << airborne.player_linear_velocity.x << ","
                  << airborne.player_linear_velocity.y << " support="
                  << airborne.support_entity_id << "\n";
        const double expected_y = contact.support_point_linear_velocity.y + 5.5 -
                                  9.81 * Simulation::kFixedStepSeconds;
        if (std::abs(airborne.player_linear_velocity.y - expected_y) > 0.3 ||
            airborne.player_linear_velocity.x < 0.02) {
            std::cerr << "FAIL takeoff did not inherit rotating support momentum\n";
            return 17;
        }
        jumped_from_motion = true;
        break;
    }
    if (!jumped_from_motion) return 16;
    Simulation replay(InitialSpawn::TeeterEntry);
    advance(replay, 0.5);
    if (!walk_to(replay, 27.2, -139.0)) return 12;
    advance(replay, 0.5);
    if (!walk_to(replay, 27.8, -139.45)) return 13;
    (void)replay.set_facing(1.0, 0.0);
    advance(replay, 0.2);
    if (!walk_to(replay, 29.8, -139.45)) return 13;
    advance(replay, 0.5);
    if (!walk_to(replay, 31.95, -139.45) ||
        !walk_to(replay, 32.8, -138.58)) return 14;
    advance(replay, 4.0);
    const auto repeated = replay.snapshot();
    report(replay, "REPLAY_FAR");
    if (std::abs(angle(replay) - far_angle) > 0.00001 ||
        std::abs(repeated.player_position.x - far_state.player_position.x) > 0.00001 ||
        std::abs(repeated.player_position.y - far_state.player_position.y) > 0.00001 ||
        repeated.support_entity_id != far_state.support_entity_id ||
        repeated.death_count != far_state.death_count) return 15;
    std::cout << "PASS scraperx_sim AS-020 loaded teeter, recovery and +77 route\n";
    return 0;
}
