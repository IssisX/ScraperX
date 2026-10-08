#include "sim/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;

namespace {
void require(const bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL AS-021 " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void advance(Simulation &s, const double seconds) {
    for (int tick = 0; tick < static_cast<int>(seconds * 90.0); ++tick) {
        require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "fixed tick accepted");
    }
}

void report(const Simulation &s, const char *label) {
    const auto p = s.snapshot();
    std::cout << label << " position=" << p.player_position.x << ','
              << p.player_position.y << ',' << p.player_position.z
              << " velocity=" << p.player_linear_velocity.x << ','
              << p.player_linear_velocity.y << ',' << p.player_linear_velocity.z
              << " grounded=" << p.player_grounded << " support=" << p.support_entity_id
              << " balance=" << p.player_balancing << " crouch=" << p.player_crouched
              << " deaths=" << p.death_count << '\n';
}

bool walk_to(Simulation &s, const double x, const double z, const double seconds = 15.0,
             int *balance_ticks = nullptr) {
    const auto deaths_before = s.snapshot().death_count;
    for (int tick = 0; tick < static_cast<int>(seconds * 90.0); ++tick) {
        const auto p = s.snapshot();
        const double dx = x - p.player_position.x;
        const double dz = z - p.player_position.z;
        const double distance = std::hypot(dx, dz);
        if (distance < 0.10) {
            (void)s.set_move_input(0, 0);
            return true;
        }
        const double scale = std::min(1.0, distance / 0.5) / distance;
        (void)s.set_move_input(dx * scale, dz * scale);
        (void)s.set_facing(dx / distance, dz / distance);
        advance(s, Simulation::kFixedStepSeconds);
        if (balance_ticks && s.snapshot().player_balancing) ++*balance_ticks;
        if (s.snapshot().death_count != deaths_before) return false;
    }
    (void)s.set_move_input(0, 0);
    return false;
}

void reach_first_landing(Simulation &s) {
    advance(s, 0.5);
    require(s.snapshot().player_grounded &&
            s.snapshot().support_entity_id == Simulation::kTowerEntityId &&
            std::abs(s.snapshot().player_position.y - 77.9) < 0.1,
            "entry stands on existing +77 m tower ring");
    require(walk_to(s, 28.0, -131.0), "ring connects to flat brace approach");
    advance(s, 0.5);
    report(s, "BRACE_FOOT");
    int balancing = 0;
    const bool arrived = walk_to(s, 28.0, -144.0, 15.0, &balancing);
    advance(s, 0.4);
    report(s, "FIRST_LANDING");
    require(arrived && s.snapshot().player_grounded &&
            s.snapshot().player_position.y > 82.7 &&
            s.snapshot().player_position.y < 83.1,
            "inclined girder leads to supported +82 m landing");
    require(balancing > 180, "narrow inclined girder engages real balance control");
}

void cross_gap(Simulation &s, const double takeoff_x, const double lane_z) {
    require(walk_to(s, 29.0, lane_z), "first landing supplies a run-up");
    advance(s, 0.4);
    (void)s.set_facing(1, 0);
    (void)s.set_move_input(1, 0);
    for (int tick = 0; tick < 90 && s.snapshot().player_position.x < takeoff_x; ++tick) {
        advance(s, Simulation::kFixedStepSeconds);
    }
    report(s, "GAP_TAKEOFF");
    require(s.snapshot().player_grounded, "gap jump starts from real support");
    (void)s.request_jump();
    bool airborne = false;
    double peak = s.snapshot().player_position.y;
    for (int tick = 0; tick < 270; ++tick) {
        const auto p = s.snapshot();
        double x = (33.7 - p.player_position.x) / 1.4;
        double z = (lane_z - p.player_position.z) / 1.4;
        const double scale = std::max(1.0, std::hypot(x, z));
        (void)s.set_move_input(x / scale, z / scale);
        advance(s, Simulation::kFixedStepSeconds);
        const auto next = s.snapshot();
        airborne = airborne || !next.player_grounded;
        peak = std::max(peak, next.player_position.y);
        if (airborne && next.player_grounded && next.player_position.x > 33.2 &&
            std::abs(next.player_position.y - 82.9) < 0.12) break;
    }
    (void)s.set_move_input(0, 0);
    advance(s, 0.4);
    report(s, "GAP_LANDING");
    require(airborne && peak > 84.0 && s.snapshot().player_grounded &&
            s.snapshot().player_position.x > 33.2 &&
            s.snapshot().player_position.x < 34.5 &&
            std::abs(s.snapshot().player_position.y - 82.9) < 0.12 &&
            s.snapshot().death_count == 0,
            "jump crosses the unsupported gap onto the far landing");
}

void complete_tower_mantle(Simulation &s) {
    (void)s.request_traversal();
    advance(s, Simulation::kFixedStepSeconds);
    const auto started = s.snapshot();
    std::cout << "TOWER_MANTLE_START gravity=" << started.player_gravity_factor
              << " hands=" << started.traversal_hand_constraint_count
              << " state=" << int(started.traversal_state) << '\n';
    require(started.player_gravity_factor == 1.0 && started.traversal_hand_constraint_count == 2,
            "tower mantle must retain gravity and engage real hands");
    const auto work_before = started.traversal_command_work_bound_j;
    int ticks = 1;
    for (; ticks < 153; ++ticks) {
        advance(s, Simulation::kFixedStepSeconds);
        const auto v = s.snapshot();
        require(v.player_gravity_factor == 1.0, "tower pull-up disabled gravity");
        if (v.player_grounded && v.support_entity_id == Simulation::kTowerEntityId &&
            v.traversal_hand_constraint_count == 0 && v.player_position.y > 88.7) break;
    }
    require(ticks < 153 && s.snapshot().death_count == 0,
            "force-earned tower mantle must complete within repaired1.7s baseline");
    require(s.snapshot().traversal_command_work_bound_j > work_before,
            "tower pull-up supplied no bounded hand work");
    std::cout << "TOWER_MANTLE_TRANSFER seconds=" << ticks / 90.0 << " supported=1 gravity=1\n";
}

void finish_route(Simulation &s) {
    require(walk_to(s, 34.0, -144.0), "align with second brace from its flat landing");
    advance(s, 0.4);
    require(walk_to(s, 34.0, -139.4), "second brace reaches the low crossmember");
    // Same input cannot walk a standing capsule through the low member.
    const bool walked_through = walk_to(s, 34.0, -136.7, 3.0);
    report(s, "STANDING_BLOCKED");
    require(!walked_through && !s.snapshot().player_crouched &&
            s.snapshot().player_position.z < -137.8 &&
            s.snapshot().player_position.z > -139.4,
            "real crossmember blocks a standing body");
    (void)s.set_crouch_input(true);
    advance(s, 0.3);
    require(walk_to(s, 34.0, -136.7), "short capsule passes under real crossmember");
    report(s, "CROUCHED_PASS");
    require(s.snapshot().player_crouched && s.snapshot().player_grounded,
            "crouched passage stays on inclined support");
    (void)s.set_crouch_input(false);
    advance(s, 0.3);
    require(!s.snapshot().player_crouched, "standing clearance returns beyond the member");
    require(walk_to(s, 34.0, -131.5), "second brace reaches upper junction");
    advance(s, 0.4);
    report(s, "UPPER_JUNCTION");
    require(s.snapshot().player_grounded &&
            std::abs(s.snapshot().player_position.y - 87.9) < 0.15,
            "upper junction is a supported +87 m surface");
    require(walk_to(s, 34.0, -132.0), "turn onto return girder");
    advance(s, 0.3);
    int balancing = 0;
    require(walk_to(s, 26.55, -132.0, 12.0, &balancing), "return girder reaches tower edge");
    (void)s.set_facing(-1, 0);
    advance(s, 0.3);
    report(s, "MANTLE_READY");
    require(balancing > 180 && s.snapshot().ledge_available,
            "return balance reaches a real mantle affordance");
    complete_tower_mantle(s);
    require(walk_to(s, 24.8, -132.0), "mantle exits onto existing tower deck");
    advance(s, 0.4);
    report(s, "RING_88");
    require(s.snapshot().player_grounded &&
            s.snapshot().support_entity_id == Simulation::kTowerEntityId &&
            std::abs(s.snapshot().player_position.y - 88.9) < 0.1 &&
            s.snapshot().death_count == 0,
            "normal parkour reaches supported +88 m with zero deaths");
}

void missed_gap_recovers() {
    Simulation miss(InitialSpawn::BracedBayEntry, scraperx::sim::WorldContent::PipeBridge);
    reach_first_landing(miss);
    (void)walk_to(miss, 33.7, -144.0, 2.0);
    report(miss, "GAP_WITHOUT_JUMP");
    require(!(miss.snapshot().player_grounded && miss.snapshot().player_position.x > 32.8 &&
              miss.snapshot().player_position.y > 82.6),
            "walking cannot invent support across the gap");
    (void)miss.request_release();
    advance(miss, 2.0);
    report(miss, "GAP_RECOVERY");
    require(miss.snapshot().player_grounded &&
            std::abs(miss.snapshot().player_position.y - 77.9) < 0.12 &&
            miss.snapshot().death_count == 0,
            "missed transfer falls onto real maintenance deck");
    require(walk_to(miss, 32.46, -142.5) && walk_to(miss, 25.1, -142.5),
            "recovery walks around real landing posts to the +77 m ring");
}

void balance_allows_departure() {
    Simulation side(InitialSpawn::BracedBayEntry, scraperx::sim::WorldContent::PipeBridge);
    advance(side, 0.5);
    require(walk_to(side, 28, -131), "side-exit approach");
    advance(side, 0.5);
    require(walk_to(side, 28, -137), "side-exit on inclined girder");
    advance(side, 0.3);
    require(side.snapshot().player_balancing, "side-exit starts in balance");
    (void)side.set_facing(1, 0);
    (void)side.set_move_input(1, 0);
    advance(side, 0.55);
    (void)side.set_move_input(0, 0);
    advance(side, 1.5);
    report(side, "DELIBERATE_SIDE_EXIT");
    require(side.snapshot().player_grounded && side.snapshot().player_position.x > 29 &&
            std::abs(side.snapshot().player_position.y - 77.9) < 0.12 &&
            side.snapshot().death_count == 0,
            "balance permits a deliberate step off and physical fall");
}

void upper_checkpoint_restores(Simulation &s) {
    require(walk_to(s, 24.8, -128.0), "upper deck reaches an exposed edge");
    advance(s, 0.4);
    require(s.snapshot().checkpoint_position.y > 88.5,
            "upper ring supplies a real committed checkpoint");
    (void)s.set_facing(1, 0);
    for (int tick = 0; tick < 720 && s.snapshot().death_count == 0; ++tick) {
        (void)s.set_move_input(s.snapshot().player_position.x < 37.0 ? 1.0 : 0.0, 0.0);
        advance(s, Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0, 0);
    advance(s, 20.0);
    report(s, "UPPER_CHECKPOINT_RETRY");
    require(s.snapshot().death_count == 1 && s.snapshot().player_grounded &&
            s.snapshot().player_position.y > 88.6,
            "fatal fall restores upper footing without an unattended repeat fall");
    // The rendered outer edge beam is 0.15 m below the slab and is also
    // legitimate footing (world-solid entity 51). Verify the actual step
    // back to the ring instead of requiring one contact entity at restore.
    require(walk_to(s, 24.8, -128.0), "restored footing allows an ordinary step onto the ring");
    advance(s, 0.4);
    require(s.snapshot().support_entity_id == Simulation::kTowerEntityId &&
            std::abs(s.snapshot().player_position.y - 88.9) < 0.1,
            "retry returns to supported +88 m ring");
}
} // namespace

int main(int argc, char **argv) {
    if (argc == 2 && std::string(argv[1]) == "mantle") {
        Simulation s;
        require(s.debug_restart_at({26.68, 87.92, -132}), "supported production mantle staging");
        advance(s, .5);
        (void)s.set_facing(-1, 0);
        complete_tower_mantle(s);
        std::cout << "PASS production Tower mantle physical hand transfer\n";
        return 0;
    }
    // Removing the first brace or its receiving landing must fail here:
    // the player cannot manufacture five metres of rise from movement input.
    Simulation route(InitialSpawn::BracedBayEntry, scraperx::sim::WorldContent::PipeBridge);
    reach_first_landing(route);
    cross_gap(route, 30.0, -144.0);
    finish_route(route);
    upper_checkpoint_restores(route);
    // A takeoff corridor, not one exact launch tick, must work.
    for (const auto &entry : {std::pair<double, double>{29.8, -144.2}, {30.3, -143.8}}) {
        Simulation varied(InitialSpawn::BracedBayEntry, scraperx::sim::WorldContent::PipeBridge);
        reach_first_landing(varied);
        cross_gap(varied, entry.first, entry.second);
    }
    missed_gap_recovers();
    balance_allows_departure();
    std::cout << "PASS scraperx_sim AS-021 braced bay parkour to +88, gap, crouch and recovery\n";
    return 0;
}
