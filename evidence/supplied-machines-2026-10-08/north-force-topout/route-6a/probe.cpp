#include "sim/simulation.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <utility>
#include <iomanip>

using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;

namespace {
double caller_frame_seconds = Simulation::kFixedStepSeconds;
double caller_remainder_seconds = 0.0;
void advance(Simulation &s, const double seconds) {
    for (int i = 0; i < static_cast<int>(seconds * 90.0); ++i) {
        caller_remainder_seconds += Simulation::kFixedStepSeconds;
        while (caller_remainder_seconds + 1.0e-9 >= caller_frame_seconds) {
            if (!s.advance_frame(caller_frame_seconds).accepted) std::abort();
            caller_remainder_seconds -= caller_frame_seconds;
        }
    }
}
bool walk_to(Simulation &s, const double x, const double z, const double seconds = 8.0) {
    const auto initial_deaths = s.snapshot().death_count;
    for (int i = 0; i < static_cast<int>(seconds * 90.0); ++i) {
        const auto p = s.snapshot();
        const double dx = x - p.player_position.x;
        const double dz = z - p.player_position.z;
        const double distance = std::hypot(dx, dz);
        if (distance < 0.12) { (void)s.set_move_input(0, 0); return true; }
        const double strength = std::min(1.0, distance / 0.6);
        (void)s.set_move_input(dx / distance * strength, dz / distance * strength);
        (void)s.set_facing(dx, dz);
        advance(s, Simulation::kFixedStepSeconds);
        if (s.snapshot().death_count != initial_deaths) return false;
    }
    (void)s.set_move_input(0, 0);
    return false;
}
}


void report_first(const char *label,const scraperx::sim::Snapshot &p){
 std::cout<<std::setprecision(10)<<label<<" tick="<<p.tick_index<<" time="<<p.simulation_time_seconds
 <<" P="<<p.player_position.x<<","<<p.player_position.y<<","<<p.player_position.z
 <<" V="<<p.player_linear_velocity.x<<","<<p.player_linear_velocity.y<<","<<p.player_linear_velocity.z
 <<" support="<<p.support_entity_id<<" grounded="<<p.player_grounded
 <<" state="<<int(p.traversal_state)<<" traversal_owner="<<p.traversal_support_entity_id
 <<" grip_offered="<<p.grip_available<<" grip_owner="<<p.grip_entity_id
 <<" grip="<<p.grip_point.x<<","<<p.grip_point.y<<","<<p.grip_point.z
 <<" target="<<p.traversal_target_point.x<<","<<p.traversal_target_point.y<<","<<p.traversal_target_point.z
 <<" hands="<<p.traversal_hand_constraint_count
 <<" LH="<<p.traversal_left_hand.x<<","<<p.traversal_left_hand.y<<","<<p.traversal_left_hand.z
 <<" RH="<<p.traversal_right_hand.x<<","<<p.traversal_right_hand.y<<","<<p.traversal_right_hand.z
 <<" LF="<<p.traversal_left_hand_force.x<<","<<p.traversal_left_hand_force.y<<","<<p.traversal_left_hand_force.z
 <<" RF="<<p.traversal_right_hand_force.x<<","<<p.traversal_right_hand_force.y<<","<<p.traversal_right_hand_force.z
 <<" gravity="<<p.player_gravity_factor<<" deaths="<<p.death_count<<'\n';
}

int main(int argc, char **argv) {
    if (argc > 1) {
        const int caller_hz = std::atoi(argv[1]);
        if (caller_hz != 60 && caller_hz != 360) {
            std::cerr << "FAIL unsupported caller partition; use 60 or 360 Hz\n";
            return EXIT_FAILURE;
        }
        caller_frame_seconds = 1.0 / static_cast<double>(caller_hz);
    }
    Simulation s(InitialSpawn::NorthFrameEntry, scraperx::sim::WorldContent::PipeBridge);
    advance(s, 0.5);
    const auto entry = s.snapshot();
    report_first("ENTRY",entry);
    if (!entry.player_grounded || entry.support_entity_id != Simulation::kTowerEntityId ||
        std::abs(entry.player_position.y - 88.9) > 0.1) {
        std::cerr << "FAIL north entry is not the +88 m tower ring\n";
        return EXIT_FAILURE;
    }
    const bool arrived = walk_to(s, 16.0, -179.55);
    const auto landing = s.snapshot();
    report_first("WALK_RETURN",landing);
    if (!arrived || !landing.player_grounded || landing.support_entity_id != 1901 ||
        std::abs(landing.player_position.y - 88.9) > 0.1) {
        std::cerr << "FAIL north frame entry not connected to +88 m ring at "
                  << landing.player_position.x << ',' << landing.player_position.y << ','
                  << landing.player_position.z << " support=" << landing.support_entity_id << '\n';
        return EXIT_FAILURE;
    }
    (void)s.set_facing(0, -1);
    advance(s, 0.4);
    report_first("AFTER_NEUTRAL",s.snapshot());
    if (!s.snapshot().grip_available || s.snapshot().grip_entity_id != 1901) {
        std::cerr << "FAIL first short climb has no real handhold at "
                  << s.snapshot().player_position.x << ',' << s.snapshot().player_position.y << ','
                  << s.snapshot().player_position.z << '\n';
        return EXIT_FAILURE;
    }
    (void)s.request_traversal();
    advance(s, 0.3);
    report_first("CLIMB_ACCEPTED",s.snapshot());
    if (s.snapshot().traversal_state != scraperx::sim::TraversalState::Climbing) {
        std::cerr << "FAIL first ladder does not accept climb\n";
        return EXIT_FAILURE;
    }
    (void)s.set_move_input(0, -1);
    bool first_rest = false;
    for (int tick = 0; tick < 12 * 90; ++tick) {
        advance(s, Simulation::kFixedStepSeconds);
        const auto p = s.snapshot();
        if ((tick % 90)==0) report_first("CLIMB_TRACE",p);
        if (p.player_grounded && p.support_entity_id == 1901 &&
            std::abs(p.player_position.y - 94.9) < 0.12) {
            first_rest = true;
            break;
        }
    }
    (void)s.set_move_input(0, 0);
    report_first("FIRST_RESULT",s.snapshot());
    if (!first_rest) {
        const auto p = s.snapshot();
        std::cerr << "FAIL first climb did not top out on +94 m rest "
                  << p.player_position.x << ',' << p.player_position.y << ','
                  << p.player_position.z << " state=" << static_cast<int>(p.traversal_state)
                  << " support=" << p.support_entity_id << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "FIRST_REST " << s.snapshot().player_position.x << ','
              << s.snapshot().player_position.y << ',' << s.snapshot().player_position.z << '\n';
    std::cout << "BASELINE_FIRST_CLIMB PASS source6a=1 original_inputs=1 no_stage_or_velocity_writes=1 stopped_at_first_rest=1\n";
    return EXIT_SUCCESS;
}
