#include "sim/simulation.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <utility>

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

int main(int argc, char **argv) {
    if (argc > 1) {
        const int caller_hz = std::atoi(argv[1]);
        if (caller_hz != 60 && caller_hz != 360) {
            std::cerr << "FAIL unsupported caller partition; use 60 or 360 Hz\n";
            return EXIT_FAILURE;
        }
        caller_frame_seconds = 1.0 / static_cast<double>(caller_hz);
    }
    Simulation s(InitialSpawn::NorthFrameEntry);
    advance(s, 0.5);
    const auto entry = s.snapshot();
    if (!entry.player_grounded || entry.support_entity_id != Simulation::kTowerEntityId ||
        std::abs(entry.player_position.y - 88.9) > 0.1) {
        std::cerr << "FAIL north entry is not the +88 m tower ring\n";
        return EXIT_FAILURE;
    }
    const bool arrived = walk_to(s, 16.0, -179.55);
    const auto landing = s.snapshot();
    if (!arrived || !landing.player_grounded || landing.support_entity_id != 1901 ||
        std::abs(landing.player_position.y - 88.9) > 0.1) {
        std::cerr << "FAIL north frame entry not connected to +88 m ring at "
                  << landing.player_position.x << ',' << landing.player_position.y << ','
                  << landing.player_position.z << " support=" << landing.support_entity_id << '\n';
        return EXIT_FAILURE;
    }
    (void)s.set_facing(0, -1);
    advance(s, 0.4);
    if (!s.snapshot().grip_available || s.snapshot().grip_entity_id != 1901) {
        std::cerr << "FAIL first short climb has no real handhold at "
                  << s.snapshot().player_position.x << ',' << s.snapshot().player_position.y << ','
                  << s.snapshot().player_position.z << '\n';
        return EXIT_FAILURE;
    }
    (void)s.request_traversal();
    advance(s, 0.3);
    if (s.snapshot().traversal_state != scraperx::sim::TraversalState::Climbing) {
        std::cerr << "FAIL first ladder does not accept climb\n";
        return EXIT_FAILURE;
    }
    (void)s.set_move_input(0, -1);
    bool first_rest = false;
    for (int tick = 0; tick < 12 * 90; ++tick) {
        advance(s, Simulation::kFixedStepSeconds);
        const auto p = s.snapshot();
        if (p.player_grounded && p.support_entity_id == 1901 &&
            std::abs(p.player_position.y - 94.9) < 0.12) {
            first_rest = true;
            break;
        }
    }
    (void)s.set_move_input(0, 0);
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
    if (!walk_to(s, 16.0, -181.2) || !walk_to(s, 10.0, -181.2)) {
        std::cerr << "FAIL +94 m lateral transfer has no walkable structure\n";
        return EXIT_FAILURE;
    }
    if (!s.snapshot().player_grounded || s.snapshot().support_entity_id != 1901 ||
        std::abs(s.snapshot().player_position.y - 94.9) > 0.12) {
        std::cerr << "FAIL +94 m lateral transfer lost its rest support\n";
        return EXIT_FAILURE;
    }
    if (!walk_to(s, 10.0, -179.55)) {
        const auto p = s.snapshot();
        std::cerr << "FAIL second climb approach missing at " << p.player_position.x << ','
                  << p.player_position.y << ',' << p.player_position.z << " support="
                  << p.support_entity_id << " deaths=" << p.death_count << '\n';
        return EXIT_FAILURE;
    }
    advance(s, 0.4);
    (void)s.set_facing(0, 1);
    advance(s, 0.3);
    if (!s.snapshot().grip_available || s.snapshot().grip_entity_id != 1901) {
        std::cerr << "FAIL second short climb has no real handhold\n";
        return EXIT_FAILURE;
    }
    (void)s.request_traversal();
    advance(s, 0.3);
    if (s.snapshot().traversal_state != scraperx::sim::TraversalState::Climbing) {
        std::cerr << "FAIL second ladder does not accept climb\n";
        return EXIT_FAILURE;
    }
    (void)s.set_move_input(0, 1);
    bool second_rest = false;
    for (int tick = 0; tick < 10 * 90; ++tick) {
        advance(s, Simulation::kFixedStepSeconds);
        const auto p = s.snapshot();
        if (p.player_grounded && p.support_entity_id == 1901 &&
            std::abs(p.player_position.y - 98.4) < 0.12) {
            second_rest = true;
            break;
        }
    }
    (void)s.set_move_input(0, 0);
    if (!second_rest) {
        const auto p = s.snapshot();
        std::cerr << "FAIL second climb did not reach +97.5 m rest at "
                  << p.player_position.x << ',' << p.player_position.y << ','
                  << p.player_position.z << " state=" << static_cast<int>(p.traversal_state) << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "SECOND_REST " << s.snapshot().player_position.x << ','
              << s.snapshot().player_position.y << ',' << s.snapshot().player_position.z << '\n';
    if (!walk_to(s, 10.0, -177.55)) {
        std::cerr << "FAIL +97.5 m rest has no tower approach\n";
        return EXIT_FAILURE;
    }
    advance(s, 0.4);
    (void)s.set_facing(0, 1);
    advance(s, 0.3);
    if (!s.snapshot().ledge_available) {
        std::cerr << "FAIL +99 m ring is not a real mantle affordance at "
                  << s.snapshot().player_position.x << ',' << s.snapshot().player_position.y
                  << ',' << s.snapshot().player_position.z << '\n';
        return EXIT_FAILURE;
    }
    (void)s.request_traversal();
    advance(s, 1.0);
    if (!walk_to(s, 10.0, -174.5)) {
        std::cerr << "FAIL +99 m ring exit does not connect\n";
        return EXIT_FAILURE;
    }
    advance(s, 0.4);
    const auto ring = s.snapshot();
    if (!ring.player_grounded || ring.support_entity_id != Simulation::kTowerEntityId ||
        std::abs(ring.player_position.y - 99.9) > 0.1 || ring.death_count != 0) {
        std::cerr << "FAIL +99 m intermediate ring unsupported at "
                  << ring.player_position.x << ',' << ring.player_position.y << ','
                  << ring.player_position.z << " support=" << ring.support_entity_id << '\n';
        return EXIT_FAILURE;
    }
    if (!walk_to(s, 24.0, -174.5)) {
        std::cerr << "FAIL +99 m ring cannot reach its exposed east edge\n";
        return EXIT_FAILURE;
    }
    advance(s, 0.5);
    if (s.snapshot().checkpoint_position.y < 99.5) {
        std::cerr << "FAIL +99 m ring did not commit real footing\n";
        return EXIT_FAILURE;
    }
    (void)s.set_facing(1, 0);
    for (int tick = 0; tick < 10 * 90 && s.snapshot().death_count == 0; ++tick) {
        (void)s.set_move_input(s.snapshot().player_position.x < 40.0 ? 1.0 : 0.0, 0.0);
        advance(s, Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0, 0);
    advance(s, 20.0);
    const auto intermediate_retry = s.snapshot();
    if (intermediate_retry.death_count != 1 || !intermediate_retry.player_grounded ||
        intermediate_retry.support_entity_id != Simulation::kTowerEntityId ||
        std::abs(intermediate_retry.player_position.y - 99.9) > 0.1) {
        std::cerr << "FAIL +99 m fatal-fall checkpoint not stable at "
                  << intermediate_retry.player_position.x << ',' << intermediate_retry.player_position.y
                  << ',' << intermediate_retry.player_position.z << " deaths="
                  << intermediate_retry.death_count << '\n';
        return EXIT_FAILURE;
    }
    if (!walk_to(s, 16.0, -174.5) || !walk_to(s, 16.0, -179.55)) {
        const auto p = s.snapshot();
        std::cerr << "FAIL +99 m ring does not connect to upper frame entry "
                  << p.player_position.x << ',' << p.player_position.y << ','
                  << p.player_position.z << " support=" << p.support_entity_id
                  << " deaths=" << p.death_count << '\n';
        return EXIT_FAILURE;
    }
    advance(s, 0.4);
    (void)s.set_facing(0, -1);
    advance(s, 0.3);
    if (!s.snapshot().grip_available || s.snapshot().grip_entity_id != 1901) {
        std::cerr << "FAIL upper frame has no first climb grip\n";
        return EXIT_FAILURE;
    }
    (void)s.request_traversal();
    advance(s, 0.3);
    (void)s.set_move_input(0, -1);
    bool upper_first_rest = false;
    for (int tick = 0; tick < 10 * 90; ++tick) {
        advance(s, Simulation::kFixedStepSeconds);
        const auto p = s.snapshot();
        if (p.player_grounded && p.support_entity_id == 1901 &&
            std::abs(p.player_position.y - 104.4) < 0.12) {
            upper_first_rest = true;
            break;
        }
    }
    (void)s.set_move_input(0, 0);
    if (!upper_first_rest) {
        const auto p = s.snapshot();
        std::cerr << "FAIL upper first climb did not reach +103.5 m rest at "
                  << p.player_position.x << ',' << p.player_position.y << ','
                  << p.player_position.z << " state=" << static_cast<int>(p.traversal_state) << '\n';
        return EXIT_FAILURE;
    }
    if (!walk_to(s, 15.1, -180.6)) {
        std::cerr << "FAIL +103.5 m shelf does not reach transfer takeoff\n";
        return EXIT_FAILURE;
    }
    advance(s, 0.3);
    (void)s.set_facing(-1, 0);
    (void)s.request_jump();
    (void)s.set_move_input(-0.5, 0);
    bool caught = false;
    for (int tick = 0; tick < 2 * 90; ++tick) {
        advance(s, Simulation::kFixedStepSeconds);
        if (s.snapshot().traversal_state == scraperx::sim::TraversalState::Hanging) {
            caught = true;
            break;
        }
    }
    (void)s.set_move_input(0, 0);
    if (!caught) {
        const auto p = s.snapshot();
        std::cerr << "FAIL raised transfer did not catch a real hanging lip at "
                  << p.player_position.x << ',' << p.player_position.y << ','
                  << p.player_position.z << " state=" << static_cast<int>(p.traversal_state) << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "HANG_START " << s.snapshot().player_position.x << ','
              << s.snapshot().player_position.y << ',' << s.snapshot().player_position.z << '\n';
    (void)s.request_release();
    advance(s, 1.0);
    if (!s.snapshot().player_grounded || s.snapshot().support_entity_id != 1901 ||
        std::abs(s.snapshot().player_position.y - 104.4) > 0.12 || s.snapshot().death_count != 1) {
        std::cerr << "FAIL released lip does not recover on the real +103.5 m rest\n";
        return EXIT_FAILURE;
    }
    for (const auto [x, z] : {std::pair<double, double>{15.0, -180.8},
                              {15.2, -180.5}}) {
        if (!walk_to(s, x, z)) {
            std::cerr << "FAIL cannot retry raised transfer after release\n";
            return EXIT_FAILURE;
        }
        advance(s, 0.3);
        (void)s.set_facing(-1, 0);
        (void)s.request_jump();
        (void)s.set_move_input(-0.5, 0);
        caught = false;
        for (int tick = 0; tick < 2 * 90; ++tick) {
            advance(s, Simulation::kFixedStepSeconds);
            if (s.snapshot().traversal_state == scraperx::sim::TraversalState::Hanging) {
                caught = true;
                break;
            }
        }
        (void)s.set_move_input(0, 0);
        if (!caught) {
            std::cerr << "FAIL varied raised transfer cannot catch lip from " << x << ',' << z << '\n';
            return EXIT_FAILURE;
        }
        if (x < 15.1) {
            (void)s.request_release();
            advance(s, 1.0);
            if (!s.snapshot().player_grounded ||
                std::abs(s.snapshot().player_position.y - 104.4) > 0.12) {
                std::cerr << "FAIL first varied catch cannot recover after release\n";
                return EXIT_FAILURE;
            }
        }
    }
    (void)s.request_jump();
    advance(s, 0.5);
    if (s.snapshot().traversal_state != scraperx::sim::TraversalState::Hanging) {
        std::cerr << "FAIL canopy permits premature top-out\n";
        return EXIT_FAILURE;
    }
    (void)s.set_move_input(0, -1);
    bool crossed = false;
    for (int tick = 0; tick < 5 * 90; ++tick) {
        advance(s, Simulation::kFixedStepSeconds);
        if (s.snapshot().player_position.z < -181.85) {
            crossed = true;
            break;
        }
    }
    (void)s.set_move_input(0, 0);
    if (!crossed || s.snapshot().traversal_state != scraperx::sim::TraversalState::Hanging) {
        std::cerr << "FAIL cannot shimmy under canopy to the clear pocket at "
                  << s.snapshot().player_position.x << ',' << s.snapshot().player_position.y << ','
                  << s.snapshot().player_position.z << '\n';
        return EXIT_FAILURE;
    }
    (void)s.request_jump();
    advance(s, 1.0);
    if (!s.snapshot().player_grounded || s.snapshot().support_entity_id != 1901 ||
        std::abs(s.snapshot().player_position.y - 106.9) > 0.12) {
        std::cerr << "FAIL shimmy pocket does not permit top-out at "
                  << s.snapshot().player_position.x << ',' << s.snapshot().player_position.y << ','
                  << s.snapshot().player_position.z << '\n';
        return EXIT_FAILURE;
    }
    if (!walk_to(s, 13.0, -182.75)) {
        std::cerr << "FAIL final ladder approach on +106 m shelf\n";
        return EXIT_FAILURE;
    }
    advance(s, 0.3);
    (void)s.set_facing(0, -1);
    advance(s, 0.3);
    if (!s.snapshot().grip_available || s.snapshot().grip_entity_id != 1901) {
        std::cerr << "FAIL final ladder has no real grip at "
                  << s.snapshot().player_position.x << ',' << s.snapshot().player_position.y
                  << ',' << s.snapshot().player_position.z << '\n';
        return EXIT_FAILURE;
    }
    (void)s.request_traversal();
    advance(s, 0.3);
    if (s.snapshot().traversal_state != scraperx::sim::TraversalState::Climbing) {
        std::cerr << "FAIL final ladder cannot be climbed\n";
        return EXIT_FAILURE;
    }
    (void)s.set_move_input(0, -1);
    bool final_rest = false;
    for (int tick = 0; tick < 7 * 90; ++tick) {
        advance(s, Simulation::kFixedStepSeconds);
        const auto p = s.snapshot();
        if (p.player_grounded && p.support_entity_id == 1901 &&
            std::abs(p.player_position.y - 109.4) < 0.12) {
            final_rest = true;
            break;
        }
    }
    (void)s.set_move_input(0, 0);
    if (!final_rest) {
        const auto p = s.snapshot();
        std::cerr << "FAIL final short climb has no +108.5 m rest at "
                  << p.player_position.x << ',' << p.player_position.y << ','
                  << p.player_position.z << " state=" << static_cast<int>(p.traversal_state) << '\n';
        return EXIT_FAILURE;
    }
    if (!walk_to(s, 15.5, -183.8) || !walk_to(s, 15.5, -177.55)) {
        const auto p = s.snapshot();
        std::cerr << "FAIL high rest does not reach the tower mantle at "
                  << p.player_position.x << ',' << p.player_position.y << ','
                  << p.player_position.z << '\n';
        return EXIT_FAILURE;
    }
    advance(s, 0.4);
    (void)s.set_facing(0, 1);
    advance(s, 0.3);
    if (!s.snapshot().ledge_available) {
        std::cerr << "FAIL +110 m tower receiver has no real mantle affordance\n";
        return EXIT_FAILURE;
    }
    (void)s.request_traversal();
    advance(s, 1.0);
    if (!walk_to(s, 15.5, -174.5)) {
        std::cerr << "FAIL +110 m tower receiver is not walkable\n";
        return EXIT_FAILURE;
    }
    advance(s, 0.4);
    const auto summit = s.snapshot();
    if (!summit.player_grounded || summit.support_entity_id != Simulation::kTowerEntityId ||
        std::abs(summit.player_position.y - 110.9) > 0.1 || summit.death_count != 1) {
        std::cerr << "FAIL supported +110 m ring missing at "
                  << summit.player_position.x << ',' << summit.player_position.y << ','
                  << summit.player_position.z << " support=" << summit.support_entity_id << '\n';
        return EXIT_FAILURE;
    }
    if (!walk_to(s, 24.0, -174.5)) {
        std::cerr << "FAIL +110 m ring cannot reach its exposed east edge\n";
        return EXIT_FAILURE;
    }
    advance(s, 0.5);
    if (s.snapshot().checkpoint_position.y < 110.5) {
        std::cerr << "FAIL +110 m ring did not commit real footing\n";
        return EXIT_FAILURE;
    }
    (void)s.set_facing(1, 0);
    for (int tick = 0; tick < 10 * 90 && s.snapshot().death_count == 1; ++tick) {
        (void)s.set_move_input(s.snapshot().player_position.x < 40.0 ? 1.0 : 0.0, 0.0);
        advance(s, Simulation::kFixedStepSeconds);
    }
    (void)s.set_move_input(0, 0);
    advance(s, 20.0);
    const auto retry = s.snapshot();
    if (retry.death_count != 2 || !retry.player_grounded ||
        retry.support_entity_id != Simulation::kTowerEntityId ||
        std::abs(retry.player_position.y - 110.9) > 0.1) {
        std::cerr << "FAIL +110 m fatal-fall checkpoint not stable at "
                  << retry.player_position.x << ',' << retry.player_position.y << ','
                  << retry.player_position.z << " deaths=" << retry.death_count << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "PASS north frame native +88 to +110 m with real supports, lip retry and two stable receiver checkpoints caller_hz="
              << static_cast<int>(std::round(1.0 / caller_frame_seconds)) << '\n';
    return EXIT_SUCCESS;
}
