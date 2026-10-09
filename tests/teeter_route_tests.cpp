#include "sim/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <utility>

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
             const double seconds = 12.0, const double pace = 1.0,
             const bool touch_effort = false) {
    double accumulated_x = 0, accumulated_z = 0;
    for (int tick = 0; tick < static_cast<int>(seconds * 90.0); ++tick) {
        const auto state = simulation.snapshot();
        const auto p = state.player_position;
        const auto v = state.player_linear_velocity;
        const double dx = x - p.x;
        const double dz = z - p.z;
        const double d = std::hypot(dx, dz);
        // Reaching the target in flight is not supported arrival. Deliberate
        // braking must remove approach momentum before releasing the stick.
        if (d < 0.12 && std::hypot(v.x, v.z) < 0.15 && state.player_grounded) {
            (void)simulation.set_move_input(0, 0);
            return true;
        }
        if (touch_effort) {
            // Match ordinary touch's early braking and bounded integral. A
            // rounded tongue must not require a saturated, test-only approach.
            if (d > 0.75) accumulated_x = accumulated_z = 0;
            else if (d > 0.12) {
                accumulated_x += dx * 0.4 * Simulation::kFixedStepSeconds;
                accumulated_z += dz * 0.4 * Simulation::kFixedStepSeconds;
                const double accumulated = std::hypot(accumulated_x, accumulated_z);
                if (accumulated > 0.35) {
                    accumulated_x *= 0.35 / accumulated;
                    accumulated_z *= 0.35 / accumulated;
                }
            }
            const auto support = state.support_point_linear_velocity;
            (void)simulation.set_move_input(
                std::clamp(dx / 2.75 + accumulated_x - (v.x - support.x) * 0.12, -1.0, 1.0),
                std::clamp(dz / 2.75 + accumulated_z - (v.z - support.z) * 0.12, -1.0, 1.0));
        } else {
            (void)simulation.set_move_input(
                std::clamp(dx * 1.8 * pace - v.x * 0.28, -1.0, 1.0),
                std::clamp(dz * 1.8 * pace - v.z * 0.28, -1.0, 1.0));
        }
        if (d > 0.0001) (void)simulation.set_facing(dx / d, dz / d);
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

bool ballast_mantle_uses_real_hands() {
    using scraperx::sim::TraversalState;
    Simulation mantle(InitialSpawn::TeeterEntry);
    advance(mantle, 0.5);
    for (const auto &target : {std::pair<double, double>{27.2, -139.0},
             {27.8, -138.58}, {30.5, -138.58}, {29.7, -139.45}}) {
        if (!walk_to(mantle, target.first, target.second, 15.0)) return false;
        advance(mantle, 0.2);
        const auto v = mantle.snapshot();
        if (!v.player_grounded || v.support_entity_id != 2800 || v.death_count != 0) return false;
    }
    (void)mantle.set_move_input(0, 0);
    (void)mantle.set_facing(-1, 0);
    advance(mantle, 0.1);
    const auto before = mantle.snapshot();
    if (!before.player_grounded || before.support_entity_id != 2800 ||
        !before.ledge_available || before.ledge_entity_id != 2801 ||
        !mantle.request_traversal()) return false;
    double command = before.traversal_command_work_bound_j;
    double positive = before.traversal_actuator_positive_work_j;
    bool reaction = false, complete = false;
    int ticks = 0;
    for (; ticks < 153; ++ticks) {
        if (!mantle.advance_frame(Simulation::kFixedStepSeconds).accepted) return false;
        const auto v = mantle.snapshot();
        if (v.player_gravity_factor != 1.0 || v.death_count != 0) return false;
        for (const auto &f : {v.traversal_left_hand_force, v.traversal_right_hand_force}) {
            const double magnitude = std::hypot(f.x, f.y, f.z);
            if (!std::isfinite(magnitude) || magnitude > 1501.0) return false;
            reaction = reaction || magnitude > 0.01;
        }
        const double debit = v.traversal_command_work_bound_j - command;
        const double supplied = v.traversal_actuator_positive_work_j - positive;
        // Separate conservative command debit and actual spring-target work.
        // These do not establish complete solver/contact energy closure.
        const double budget = 6600.0 * Simulation::kFixedStepSeconds + 0.001;
        if (!std::isfinite(debit) || !std::isfinite(supplied) ||
            debit < -0.000001 || supplied < -0.000001 ||
            debit > budget || supplied > budget) return false;
        command = v.traversal_command_work_bound_j;
        positive = v.traversal_actuator_positive_work_j;
        if (v.traversal_state == TraversalState::None) {
            complete = v.player_grounded && v.support_entity_id == 2801 &&
                v.traversal_hand_constraint_count == 0 &&
                v.accepted_traversal_count == before.accepted_traversal_count + 1;
            ++ticks;
            break;
        }
        if (v.traversal_state != TraversalState::Climbing ||
            v.traversal_hand_constraint_count != 2 ||
            v.traversal_support_entity_id != 2801) return false;
    }
    if (!complete || !reaction || command <= before.traversal_command_work_bound_j ||
        positive <= before.traversal_actuator_positive_work_j) return false;
    for (int tick = 0; tick < 270; ++tick) {
        if (!mantle.advance_frame(Simulation::kFixedStepSeconds).accepted) return false;
        const auto v = mantle.snapshot();
        if (!v.player_grounded || v.support_entity_id != 2801 ||
            v.player_gravity_factor != 1.0 || v.traversal_hand_constraint_count != 0 ||
            v.death_count != 0) return false;
    }
    const auto held = mantle.snapshot();
    if (held.foot_transfer_count != before.foot_transfer_count + 1 ||
        held.foot_transfer_tick <= before.tick_index ||
        held.foot_transfer_tick > before.tick_index + std::uint64_t(ticks) ||
        held.foot_transfer_support_entity_id != 2801 ||
        !std::isfinite(held.foot_transfer_peak_hand_load_n) ||
        held.foot_transfer_peak_hand_load_n <= 0 || held.foot_transfer_peak_hand_load_n > 3001) return false;
    if (std::hypot(held.player_linear_velocity.x - held.support_point_linear_velocity.x,
                   held.player_linear_velocity.z - held.support_point_linear_velocity.z) >= 0.2) return false;
    if (!walk_to(mantle, 27.8, -139.45, 15.0)) return false;
    advance(mantle, 0.4);
    if (!mantle.snapshot().player_grounded || mantle.snapshot().support_entity_id != 2800 ||
        mantle.snapshot().traversal_hand_constraint_count != 0 || mantle.snapshot().death_count != 0) return false;
    if (!walk_to(mantle, 25.3, -139.0, 15.0, 1.0, true)) return false;
    advance(mantle, 0.4);
    const auto returned = mantle.snapshot();
    if (!returned.player_grounded || returned.support_entity_id != Simulation::kTowerEntityId ||
        returned.traversal_hand_constraint_count != 0 || returned.death_count != 0 ||
        returned.foot_transfer_count != held.foot_transfer_count) return false;
    std::cout << "PASS teeter ballast physical mantle seconds=" << ticks / 90.0
              << " command_bound_j=" << command - before.traversal_command_work_bound_j
              << " spring_positive_j=" << positive - before.traversal_actuator_positive_work_j
              << " foot_transfer_tick=" << held.foot_transfer_tick
              << " peak_hand_load_n=" << held.foot_transfer_peak_hand_load_n
              << " stable_roof_seconds=3 onward_beam=1 return_tower=1\n";
    return true;
}

int main() {
    if (!ballast_mantle_uses_real_hands()) {
        std::cerr << "FAIL production ballast mantle needs gravity, real hands, finite work and footing\n";
        return 32;
    }
    Simulation simulation(scraperx::sim::InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::PipeBridge);
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
    Simulation no_ballast(InitialSpawn::TeeterEntry, scraperx::sim::WorldContent::PipeBridge);
    advance(no_ballast, 0.5);
    if (!walk_to(no_ballast, 27.2, -139.0)) return 22;
    const auto unadjusted_ballast = ballast_position(no_ballast);
    // Inspect on the clear side of the visible carriage; walking through
    // its contact envelope would physically operate it during this control.
    if (!walk_to(no_ballast, 27.8, -138.58) ||
        !walk_to(no_ballast, 29.8, -138.58) ||
        !walk_to(no_ballast, 32.8, -138.58)) return 22;
    advance(no_ballast, 3.0);
    report(no_ballast, "RIDER_ALONE");
    if (no_ballast.snapshot().support_entity_id != 2800 ||
        angle(no_ballast) < 0.02 ||
        std::abs(ballast_position(no_ballast) - unadjusted_ballast) > 0.05) {
        std::cerr << "FAIL rider alone tipped the unadjusted beam\n";
        return 23;
    }
    Simulation rider(InitialSpawn::TeeterEntry, scraperx::sim::WorldContent::PipeBridge);
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
    Simulation drop(InitialSpawn::TeeterFarDrop, scraperx::sim::WorldContent::PipeBridge);
    double drop_min_angle = angle(drop);
    double angle_after_prompt_response = 0.0;
    for (int tick = 0; tick < 720; ++tick) {
        (void)drop.advance_frame(Simulation::kFixedStepSeconds);
        drop_min_angle = std::min(drop_min_angle, angle(drop));
        // Finite traction carries real slip instead of overwriting velocity.
        // Sample with one90Hz-tick timing margin; retain the same early angle,
        // full loaded sweep, stable lower stop and no-death acceptance.
        if (tick == 90) angle_after_prompt_response = angle(drop);
    }
    report(drop, "LANDING");
    std::cout << "LANDING_SWEEP prompt_seconds=" << 91.0 / 90.0
              << " angle=" << angle_after_prompt_response
              << " minimum=" << drop_min_angle << "\n";
    if (!drop.snapshot().player_grounded || drop.snapshot().support_entity_id != 2800 ||
        angle_after_prompt_response > -0.05 || drop_min_angle > -0.40 ||
        angle(drop) > -0.40 || angle(drop) < -0.55 ||
        drop.snapshot().death_count != 0) return 11;
    Simulation recovery(InitialSpawn::TeeterEntry, scraperx::sim::WorldContent::PipeBridge);
    advance(recovery, 0.5);
    if (!walk_to(recovery, 30.5, -139.0)) return 18;
    // The finite entry push changes the carriage's settled position. Walk
    // around its real envelope before deliberately missing the open edge;
    // asking for a path through the 65 kg carriage is not a fall setup.
    if (!walk_to(recovery, 32.8, -138.58)) return 18;
    (void)walk_to(recovery, 32.8, -140.5, 4.0);
    // Wait for physical arrival, not an assumed flight duration after the
    // helper's bounded attempt. Never accept flight as recovery footing.
    bool released_catch = false;
    for (int tick = 0; tick < 6 * 90; ++tick) {
        const auto state = recovery.snapshot();
        if (!released_catch && state.traversal_state == scraperx::sim::TraversalState::Hanging) {
            // The unobstructed miss can honestly catch the real far shelf.
            // Deliberate Drop chooses the lower recovery apron, not a rescue.
            if (!recovery.request_release()) return 19;
            released_catch = true;
        }
        if (state.player_grounded && state.support_entity_id == 1800) break;
        (void)recovery.advance_frame(Simulation::kFixedStepSeconds);
    }
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
    Simulation depart(InitialSpawn::TeeterFarDrop, scraperx::sim::WorldContent::PipeBridge);
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
        // The body has already earned its departure momentum through real
        // contacts. Taking off must not snap XZ to the support-point target.
        const double steering_increment = 0.85 * 9.81 * Simulation::kFixedStepSeconds;
        const double horizontal_change = std::hypot(
            airborne.player_linear_velocity.x - contact.player_linear_velocity.x,
            airborne.player_linear_velocity.z - contact.player_linear_velocity.z);
        if (std::abs(airborne.player_linear_velocity.y - expected_y) > 0.3 ||
            std::abs(airborne.player_linear_velocity.x) < 0.02 ||
            horizontal_change > steering_increment + 0.001) {
            std::cerr << "FAIL takeoff replaced earned momentum or exceeded finite contact steering\n";
            return 17;
        }
        jumped_from_motion = true;
        break;
    }
    if (!jumped_from_motion) return 16;
    Simulation restored_motion(InitialSpawn::TeeterFarDrop, scraperx::sim::WorldContent::PipeBridge);
    bool restored_from_motion = false;
    for (int tick = 0; tick < 180; ++tick) {
        (void)restored_motion.advance_frame(Simulation::kFixedStepSeconds);
        const auto contact = restored_motion.snapshot();
        if (!contact.player_grounded || contact.support_entity_id != 2800 ||
            std::abs(contact.support_point_linear_velocity.y) < 0.5 ||
            std::abs(contact.checkpoint_position.y - contact.player_position.y) > 0.02) continue;
        if (!restored_motion.restart_checkpoint()) return 28;
        const auto restored = restored_motion.snapshot();
        const auto com_velocity = restored_motion.kit_body_velocity(restored_motion.kit_body_index(2800));
        std::cout << "ROTATING_RESTORE player_vy=" << restored.player_linear_velocity.y
                  << " contact_vy=" << contact.support_point_linear_velocity.y
                  << " com_vy=" << com_velocity.y << "\n";
        if (restored.player_grounded ||
            std::abs(restored.player_linear_velocity.y - contact.support_point_linear_velocity.y) > 0.5 ||
            std::abs(restored.player_linear_velocity.y - com_velocity.y) < 0.5) return 29;
        advance(restored_motion, 0.5);
        if (!restored_motion.snapshot().player_grounded ||
            restored_motion.snapshot().support_entity_id != 2800 ||
            restored_motion.snapshot().death_count != 0) return 30;
        restored_from_motion = true;
        break;
    }
    if (!restored_from_motion) return 31;
    Simulation replay(InitialSpawn::TeeterEntry, scraperx::sim::WorldContent::PipeBridge);
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
