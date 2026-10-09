#include "sim/simulation.hpp"

#include <algorithm>
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
        if (distance < 0.12 &&
            std::hypot(p.player_linear_velocity.x, p.player_linear_velocity.z) < .15) {
            (void)s.set_move_input(0, 0);
            return true;
        }
        // Stop through ordinary input before accepting a narrow grip stance.
        // A distance-only arrival at4.76m/s overshoots into the real rungs
        // under finite braking; it is not a successfully reached approach.
        const double input_x = dx * 1.8 - p.player_linear_velocity.x * .28;
        const double input_z = dz * 1.8 - p.player_linear_velocity.z * .28;
        const double scale = std::max(1.0, std::hypot(input_x, input_z));
        (void)s.set_move_input(input_x / scale, input_z / scale);
        if (distance > .0001) (void)s.set_facing(dx, dz);
        advance(s, Simulation::kFixedStepSeconds);
        if (s.snapshot().death_count != initial_deaths) return false;
    }
    (void)s.set_move_input(0, 0);
    return false;
}

bool powered_jump_recovers_and_retries() {
    using scraperx::sim::TraversalState;
    Simulation s;
    const auto fail = [](const char *message) {
        std::cerr << "FAIL north powered Jump " << message << '\n';
        return false;
    };
    // This normal-world case owns its 90Hz ticks, independent of the full
    // route's 60/360Hz caller remainder. Only the initial supported pose is set.
    const auto ticks = [&](int count) {
        for (int i = 0; i < count; ++i)
            if (!s.advance_frame(Simulation::kFixedStepSeconds).accepted) return false;
        return true;
    };
    if (!s.debug_restart_at({16, 88.9, -179.55}) || !s.set_facing(0, -1) || !ticks(45))
        return fail("supported normal-world setup rejected");
    if (!s.snapshot().player_grounded || s.snapshot().support_entity_id != 1901 ||
        !s.snapshot().grip_available || s.snapshot().grip_entity_id != 1901)
        return fail("initial pose lacks real frame footing and grip");
    if (!s.request_traversal() || !ticks(1) || !s.set_move_input(0, -1))
        return fail("ordinary climb input rejected");
    for (int i = 0; i < 500 && s.snapshot().player_position.y < 90; ++i)
        if (!ticks(1)) return fail("climb tick rejected");
    if (!s.set_move_input(0, 0) || !ticks(30)) return fail("neutral hand stance rejected");
    const auto start = s.snapshot();
    if (start.traversal_state != TraversalState::Climbing ||
        start.traversal_support_entity_id != 1901 || start.traversal_hand_constraint_count != 2 ||
        start.player_grounded || start.support_entity_id != 0 || start.player_gravity_factor != 1 ||
        start.player_position.y < 90 || start.player_position.y > 91 || start.death_count != 0)
        return fail("Jump must start unsupported with two actual gravity-on hands");
    if (!s.request_jump()) return fail("ordinary Jump request rejected");
    int stroke_ticks = 0;
    bool released = false;
    for (; stroke_ticks < 30; ++stroke_ticks) {
        const auto before = s.snapshot();
        if (!ticks(1)) return fail("push tick rejected");
        const auto after = s.snapshot();
        const double debit = after.traversal_command_work_bound_j - before.traversal_command_work_bound_j;
        const double supplied = after.traversal_actuator_positive_work_j - before.traversal_actuator_positive_work_j;
        const auto length = [](scraperx::sim::Vector3 v) { return std::hypot(v.x, v.y, v.z); };
        const double left_force = length(after.traversal_left_hand_force);
        const double right_force = length(after.traversal_right_hand_force);
        // Conservative command debit and actual positive spring-target work
        // are separate receipts; neither may borrow dissipated work.
        if (!std::isfinite(debit) || !std::isfinite(supplied) || debit < -1e-6 || supplied < -1e-6 ||
            debit > 3000 * Simulation::kFixedStepSeconds + .01 ||
            supplied > 3000 * Simulation::kFixedStepSeconds + .01 ||
            !std::isfinite(left_force) || !std::isfinite(right_force) ||
            left_force > 1501 || right_force > 1501 ||
            after.player_gravity_factor != 1 || after.death_count != 0)
            return fail("push bypassed finite hand force/work or gravity");
        const auto dv = scraperx::sim::Vector3{
            after.player_linear_velocity.x - before.player_linear_velocity.x,
            after.player_linear_velocity.y - before.player_linear_velocity.y,
            after.player_linear_velocity.z - before.player_linear_velocity.z};
        if (!std::isfinite(length(dv))) return fail("push produced non-finite velocity");
        if (stroke_ticks == 0 && (after.traversal_hand_constraint_count != 2 ||
            after.player_grounded || length(dv) > (2 * 1501 / 85.0 + 9.81) * Simulation::kFixedStepSeconds + .01))
            return fail("fresh Jump discarded hands or granted departure velocity");
        if (after.traversal_hand_constraint_count == 0 && after.traversal_state == TraversalState::None) {
            if (stroke_ticks < 1 || before.traversal_hand_constraint_count != 2 ||
                before.player_grounded || after.player_grounded ||
                std::abs(dv.x) > .01 || std::abs(dv.z) > .01 ||
                std::abs(dv.y + 9.81 * Simulation::kFixedStepSeconds) > .02)
                return fail("free-air release replaced earned velocity");
            released = true;
            break;
        }
    }
    const auto departure = s.snapshot();
    if (!released || departure.traversal_command_work_bound_j <= start.traversal_command_work_bound_j + .01 ||
        departure.traversal_actuator_positive_work_j <= start.traversal_actuator_positive_work_j + .01)
        return fail("bounded stroke did not pay for its passive departure");
    for (int i = 0; i < 270 && !s.snapshot().player_grounded; ++i)
        if (!ticks(1) || s.snapshot().death_count != 0) return fail("release restored a fall");
    if (!s.snapshot().player_grounded || s.snapshot().support_entity_id != 1901 ||
        std::abs(s.snapshot().player_position.y - 88.9) > .12 || s.snapshot().death_count != 0)
        return fail("released player missed the actual recovery platform");
    bool arrived = false;
    for (int i = 0; i < 8 * 90; ++i) {
        const auto p = s.snapshot();
        const double dx = 16 - p.player_position.x, dz = -179.55 - p.player_position.z;
        if (std::hypot(dx, dz) < .12 &&
            std::hypot(p.player_linear_velocity.x, p.player_linear_velocity.z) < .15) {
            arrived = true;
            break;
        }
        const double x = dx * 1.8 - p.player_linear_velocity.x * .28;
        const double z = dz * 1.8 - p.player_linear_velocity.z * .28;
        const double scale = std::max(1.0, std::hypot(x, z));
        if (!s.set_move_input(x / scale, z / scale) || !ticks(1) || s.snapshot().death_count != 0)
            return fail("ordinary recovery approach rejected");
    }
    if (!arrived || !s.set_move_input(0, 0) || !s.set_facing(0, -1) || !ticks(36) ||
        !s.snapshot().player_grounded || s.snapshot().support_entity_id != 1901 ||
        !s.snapshot().grip_available || s.snapshot().grip_entity_id != 1901 ||
        !s.request_traversal() || !ticks(1) ||
        s.snapshot().traversal_state != TraversalState::Climbing ||
        s.snapshot().traversal_support_entity_id != 1901 ||
        s.snapshot().traversal_hand_constraint_count != 2 || s.snapshot().player_gravity_factor != 1 ||
        s.snapshot().death_count != 0)
        return fail("ordinary retry did not catch two real frame hands");
    std::cout << "PASS north frame paid Jump/recovery/retry stroke_ticks=" << stroke_ticks + 1
              << " command_j=" << departure.traversal_command_work_bound_j - start.traversal_command_work_bound_j
              << " spring_positive_j=" << departure.traversal_actuator_positive_work_j - start.traversal_actuator_positive_work_j
              << " ordinary_inputs_only=1\n";
    return true;
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
    if (!powered_jump_recovers_and_retries()) return EXIT_FAILURE;
    Simulation s(InitialSpawn::NorthFrameEntry, scraperx::sim::WorldContent::PipeBridge);
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
    std::uint64_t first_transfer_tick = 0;
    double first_transfer_seconds = 0;
    for (int tick = 0; tick < 12 * 90; ++tick) {
        advance(s, Simulation::kFixedStepSeconds);
        const auto p = s.snapshot();
        if (p.player_gravity_factor != 1.0 || p.death_count != entry.death_count) {
            std::cerr << "FAIL north-frame hand-to-foot transfer fabricated gravity or restored a fall\n";
            return EXIT_FAILURE;
        }
        if (!first_transfer_tick && p.traversal_target_point.y > 94.0)
            first_transfer_tick = p.tick_index;
        if (p.player_grounded && p.support_entity_id == 1901 &&
            std::abs(p.player_position.y - 94.9) < 0.12 &&
            p.traversal_hand_constraint_count == 0 &&
            p.traversal_state == scraperx::sim::TraversalState::None) {
            first_rest = true;
            first_transfer_seconds =
                (p.tick_index - first_transfer_tick + 1) * Simulation::kFixedStepSeconds;
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
    if (s.snapshot().foot_transfer_count != entry.foot_transfer_count + 1 ||
        s.snapshot().foot_transfer_support_entity_id != 1901 ||
        s.snapshot().foot_transfer_peak_hand_load_n <= 0) {
        std::cerr << "FAIL north-frame first rest lacks physical hand-to-foot receipt\n";
        return EXIT_FAILURE;
    }
    if (!first_transfer_tick || first_transfer_seconds > 1.7 + 1e-9) {
        std::cerr << "FAIL physical first top-out exceeded responsive1.7s bound: "
                  << first_transfer_seconds << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "FIRST_REST " << s.snapshot().player_position.x << ','
              << s.snapshot().player_position.y << ',' << s.snapshot().player_position.z
              << " physical_topout_s=" << first_transfer_seconds << '\n';
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
    const auto trace = [](const char *label, const scraperx::sim::Snapshot &p) {
        std::cout << label << " pos=" << p.player_position.x << ',' << p.player_position.y
                  << ',' << p.player_position.z << " vel=" << p.player_linear_velocity.x
                  << ',' << p.player_linear_velocity.y << ',' << p.player_linear_velocity.z
                  << " grounded=" << p.player_grounded << " support=" << p.support_entity_id
                  << " deaths=" << p.death_count << " impact=" << p.last_impact_speed_mps << '\n';
    };
    bool caught = false;
    for (int tick = 0; tick < 2 * 90; ++tick) {
        const auto before = s.snapshot();
        advance(s, Simulation::kFixedStepSeconds);
        if (s.snapshot().traversal_state == scraperx::sim::TraversalState::Hanging) {
            trace("PRE_CATCH", before);
            const auto capture = s.snapshot();
            trace("FIRST_HANG", capture);
            // Two 1500 N hands, 85 kg, gravity and bounded air steering
            // permit less than 0.7 m/s per 90 Hz tick, without other contact.
            // A 60 Hz caller may observe two native ticks in one snapshot.
            const double delta_v = std::hypot(
                capture.player_linear_velocity.x - before.player_linear_velocity.x,
                capture.player_linear_velocity.y - before.player_linear_velocity.y,
                capture.player_linear_velocity.z - before.player_linear_velocity.z);
            const double native_ticks = double(capture.tick_index - before.tick_index);
            const double displacement = std::hypot(
                capture.player_position.x - before.player_position.x,
                capture.player_position.y - before.player_position.y,
                capture.player_position.z - before.player_position.z);
            if (capture.player_gravity_factor != 1.0 ||
                capture.traversal_hand_constraint_count != 2 ||
                delta_v > 0.7 * native_ticks || displacement > 0.1 * native_ticks) {
                std::cerr << "FAIL lip catch bypasses finite gravity-on hands delta_v="
                          << delta_v << " displacement=" << displacement << '\n';
                return EXIT_FAILURE;
            }
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
    trace("RELEASE_RESULT", s.snapshot());
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
    const auto hang_transfer_start = s.snapshot();
    (void)s.request_jump();
    bool hang_transfer_finished = false;
    for (int tick = 0; tick < 153; ++tick) {
        advance(s, Simulation::kFixedStepSeconds);
        const auto p = s.snapshot();
        if (p.player_gravity_factor != 1 || p.death_count != hang_transfer_start.death_count ||
            p.traversal_state == scraperx::sim::TraversalState::Mantling) {
            std::cerr << "FAIL hanging top-out bypasses real gravity-on hands\n";
            return EXIT_FAILURE;
        }
        if (p.player_grounded && p.traversal_state == scraperx::sim::TraversalState::None &&
            p.traversal_hand_constraint_count == 0 && p.support_entity_id == 1901 &&
            std::abs(p.player_position.y - 106.9) < .12) {
            hang_transfer_finished = true;
            break;
        }
    }
    const auto hang_transfer_end = s.snapshot();
    if (!hang_transfer_finished ||
        hang_transfer_end.foot_transfer_count != hang_transfer_start.foot_transfer_count + 1 ||
        hang_transfer_end.foot_transfer_support_entity_id != 1901 ||
        hang_transfer_end.foot_transfer_peak_hand_load_n <= 0 ||
        hang_transfer_end.traversal_actuator_positive_work_j <= hang_transfer_start.traversal_actuator_positive_work_j) {
        std::cerr << "FAIL physical hanging transfer lacks responsive real footing/work; elapsed="
                  << hang_transfer_end.simulation_time_seconds - hang_transfer_start.simulation_time_seconds
                  << " P=" << hang_transfer_end.player_position.x << ',' << hang_transfer_end.player_position.y << ',' << hang_transfer_end.player_position.z
                  << " V=" << hang_transfer_end.player_linear_velocity.x << ',' << hang_transfer_end.player_linear_velocity.y << ',' << hang_transfer_end.player_linear_velocity.z
                  << " target=" << hang_transfer_end.traversal_target_point.x << ',' << hang_transfer_end.traversal_target_point.y << ',' << hang_transfer_end.traversal_target_point.z
                  << " state=" << int(hang_transfer_end.traversal_state) << " hands=" << hang_transfer_end.traversal_hand_constraint_count
                  << " foot=" << hang_transfer_end.foot_transfer_count << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "NORTH_HANG_TRANSFER seconds="
              << hang_transfer_end.simulation_time_seconds - hang_transfer_start.simulation_time_seconds
              << " peak_hand_n=" << hang_transfer_end.foot_transfer_peak_hand_load_n << '\n';
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
