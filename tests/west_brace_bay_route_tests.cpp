#include "sim/parkour_route.hpp"
#include "sim/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;
using scraperx::sim::TraversalState;
constexpr std::uint64_t kRouteEntity = scraperx::sim::kWestBraceBayEntity;
constexpr std::uint64_t kWorld = Simulation::kWorldSolidEntityId;

void report(const char *why, const Simulation &s) {
    const auto v = s.snapshot();
    std::cerr << "FAIL WEST_BRACE_BAY " << why
              << " tick=" << v.tick_index
              << " pos=" << v.player_position.x << ',' << v.player_position.y << ',' << v.player_position.z
              << " velocity=" << v.player_linear_velocity.x << ',' << v.player_linear_velocity.y << ','
              << v.player_linear_velocity.z
              << " grounded=" << v.player_grounded << " support=" << v.support_entity_id
              << " traversal=" << static_cast<int>(v.traversal_state)
              << " traversal_support=" << v.traversal_support_entity_id
              << " grip=" << v.grip_available << ':' << v.grip_entity_id
              << " hands=" << v.traversal_hand_constraint_count
              << " hand_l=" << v.traversal_left_hand.x << ',' << v.traversal_left_hand.y
              << ',' << v.traversal_left_hand.z
              << " hand_r=" << v.traversal_right_hand.x << ',' << v.traversal_right_hand.y
              << ',' << v.traversal_right_hand.z
              << " gravity=" << v.player_gravity_factor
              << " landings=" << v.landing_count << " deaths=" << v.death_count << '\n';
    std::exit(EXIT_FAILURE);
}

void require(bool condition, const char *why, const Simulation &s) {
    if (!condition) report(why, s);
}

void tick(Simulation &s, int count = 1) {
    while (count-- > 0) {
        const auto result = s.advance_frame(Simulation::kFixedStepSeconds);
        require(result.accepted && result.steps_advanced == 1,
                "one native 90 Hz step accepted", s);
        const auto v = s.snapshot();
        require(std::isfinite(v.player_position.x) && std::isfinite(v.player_position.y) &&
                    std::isfinite(v.player_position.z) &&
                    std::isfinite(v.player_linear_velocity.x) &&
                    std::isfinite(v.player_linear_velocity.y) &&
                    std::isfinite(v.player_linear_velocity.z) &&
                    v.player_gravity_factor == 1.0 && v.death_count == 0,
                "climb keeps finite motion, ordinary gravity, and the same life", s);
    }
}

double soles_y(const Snapshot &v) {
    return v.player_position.y - (v.player_crouched ? .6 : .9);
}

bool on_support_top(const Snapshot &v, double top_y, std::uint64_t support,
                    double tolerance = .10) {
    return v.player_grounded && v.support_entity_id == support &&
           std::abs(soles_y(v) - top_y) <= tolerance &&
           v.traversal_state == TraversalState::None &&
           v.traversal_hand_constraint_count == 0;
}

void require_climbing(const Simulation &s, const char *why,
                      std::uint64_t expected_support = kRouteEntity) {
    const auto v = s.snapshot();
    const bool support_matches = expected_support == kWorld
        ? (v.traversal_support_entity_id == kRouteEntity ||
           v.traversal_support_entity_id == kWorld)
        : v.traversal_support_entity_id == expected_support;
    require(v.traversal_state == TraversalState::Climbing &&
                support_matches &&
                v.traversal_hand_constraint_count == 2 && v.player_gravity_factor == 1.0,
            why, s);
}

// Move only through normal controls until the authored shelf is reached.
// Positive world-X faces/climbs into the brace bay; world-Z traverses across it.
void climb_to_shelf(Simulation &s, double top_y, double side_input,
                    int tick_budget, const char *why,
                    std::uint64_t expected_support = kRouteEntity,
                    double approach_facing_x = 1.0) {
    // A rest interrupts the hand chain. Reacquire the next reachable hold
    // through the ordinary Action command before climbing the next section.
    if (s.snapshot().traversal_state == TraversalState::None) {
        require(s.snapshot().player_grounded && s.snapshot().support_entity_id == kRouteEntity,
                "next climb starts from its real lower rest", s);
        require(s.set_facing(approach_facing_x, 0), "face the next brace line", s);
        tick(s, 30);
        require(s.snapshot().grip_available && s.snapshot().grip_entity_id == kRouteEntity,
                "next diagonal hold is in real reach", s);
        require(s.request_traversal(), "Action requests the next climb", s);
        tick(s);
        require_climbing(s, "next Action engages actual hands", expected_support);
    }
    bool arrived = false;
    for (int i = 0; i < tick_budget; ++i) {
        const auto v = s.snapshot();
        if (on_support_top(v, top_y, expected_support)) {
            arrived = true;
            break;
        }
        require(v.traversal_state == TraversalState::Climbing ||
                    v.traversal_state == TraversalState::Mantling,
                "route remains in a real climb or native mantle", s);
        if (v.traversal_state == TraversalState::Climbing)
            require_climbing(s, "diagonal route remains on two real hand constraints",
                             expected_support);
        require(s.set_move_input(approach_facing_x, side_input * approach_facing_x),
                "ordinary diagonal climb input accepted", s);
        tick(s);
        const auto after = s.snapshot();
        require(after.traversal_state == TraversalState::Climbing ||
                    after.traversal_state == TraversalState::Mantling ||
                    on_support_top(after, top_y, expected_support),
                "climb reaches only the real shelf or remains hand-supported", s);
        if (after.traversal_state == TraversalState::Climbing)
            require_climbing(s, "receiving climb keeps real hand support and gravity",
                             expected_support);
    }
    (void)s.set_move_input(0, 0);
    tick(s, 20);
    require(on_support_top(s.snapshot(), top_y, expected_support), why, s);
}

// From a real shelf, face and jump toward the bar. Press Action once the
// upward arc reaches its apex; it must acquire the actual horizontal member.
void jump_and_catch_bar(Simulation &s, const char *why) {
    const auto verify_bar_catch = [&]() {
        require_climbing(s, why);
        const auto v = s.snapshot();
        const double hands_y = .5 * (v.traversal_left_hand.y + v.traversal_right_hand.y);
        require(std::abs(hands_y - 384.6) < .45,
                "caught hand anchors are on the actual384.6m bar", s);
    };
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id == kRouteEntity,
            "bar approach starts on the real route shelf", s);
    require(s.set_facing(-1, 0) && s.set_move_input(-1, 0) && s.request_jump(),
            "ordinary jump and forward intent accepted", s);
    tick(s);
    require(s.set_move_input(0, 0), "wait neutral for the apex timing choice", s);
    bool rose = false;
    bool action_sent = false;
    for (int i = 0; i < 110; ++i) {
        const auto v = s.snapshot();
        if (v.traversal_state == TraversalState::Climbing &&
            v.traversal_support_entity_id == kRouteEntity) {
            require(action_sent, "bar was reached only after the apex Action", s);
            verify_bar_catch();
            return;
        }
        rose = rose || v.player_linear_velocity.y > .2;
        if (rose && !action_sent && v.player_linear_velocity.y <= .05) {
            require(v.grip_available && v.grip_entity_id == kRouteEntity,
                    "actual airborne bar grip is exposed to touch Action", s);
            require(s.set_move_input(-1, 0), "apex reach direction accepted", s);
            require(s.request_traversal(), "apex Action accepted", s);
            action_sent = true;
            tick(s);
            if (s.snapshot().traversal_state == TraversalState::Climbing &&
                s.snapshot().traversal_support_entity_id == kRouteEntity) {
                const auto after = s.snapshot();
                const double displacement = std::hypot(
                    std::hypot(after.player_position.x - v.player_position.x,
                               after.player_position.y - v.player_position.y),
                    after.player_position.z - v.player_position.z);
                require(displacement < .25,
                        "bar catch resolves through hands without a position grant", s);
                verify_bar_catch();
                return;
            }
            require(s.set_move_input(0, 0), "missed apex stops reach input", s);
            continue;
        }
        require(s.set_move_input(0, 0), "ordinary neutral airborne input accepted", s);
        tick(s);
    }
    require(action_sent, "Jump reached an observed apex for Action", s);
    require_climbing(s, why);
}

void traverse_laterally(Simulation &s, double direction, int ticks, const char *why) {
    require_climbing(s, "lateral transfer begins with real route hand contacts");
    const auto before = s.snapshot();
    const double hand_z_before = .5 * (before.traversal_left_hand.z + before.traversal_right_hand.z);
    for (int i = 0; i < ticks; ++i) {
        // Direction is a facing-relative side command; map it to world-space
        // Z because native movement axes are world-space. Here the bar is
        // reached while facing -X, so side -1 means world +Z.
        require(s.set_move_input(0, direction * -1.0), "ordinary lateral traverse input accepted", s);
        tick(s);
        require_climbing(s, "low-clearance traverse retains both real hand contacts");
    }
    (void)s.set_move_input(0, 0);
    tick(s, 20);
    const auto after = s.snapshot();
    const double hand_z_after = .5 * (after.traversal_left_hand.z + after.traversal_right_hand.z);
    require(std::abs(hand_z_after - hand_z_before) >= .12 &&
                std::abs(hand_z_after - hand_z_before) <= 2.2,
            why, s);
}

void miss_apex_and_recover(Simulation &s) {
    const auto entry = s.snapshot();
    require(entry.player_grounded && entry.support_entity_id == kRouteEntity,
            "miss begins on the lower route shelf", s);
    require(s.set_facing(-1, 0) && s.set_move_input(-1, 0) && s.request_jump(),
            "ordinary miss-attempt Jump accepted", s);
    tick(s);
    require(s.set_move_input(0, 0), "neutralize air input while observing the jump", s);
    bool late_request_sent = false;
    bool recovered = false;
    bool unsupported = false;
    bool descending = false;
    for (int i = 0; i < 5 * 90; ++i) {
        const auto v = s.snapshot();
        // The bar's lowest hand-search boundary is about 383.85m: the
        // player's hand aim is center+0.45m with a 0.30m vertical search.
        // Wait for observed descent below that boundary before deliberately
        // requesting the now-unreachable bar.
        if (!late_request_sent && !v.player_grounded && v.player_linear_velocity.y < -1.0 &&
            v.player_position.y < 383.75) {
            require(s.set_move_input(-1, 0), "late out-of-reach direction accepted", s);
            require(s.request_traversal(), "late out-of-reach Action accepted", s);
            late_request_sent = true;
            tick(s);
            const auto rejected = s.snapshot();
            require(rejected.player_gravity_factor == 1.0 &&
                        rejected.traversal_state == TraversalState::None &&
                        rejected.traversal_hand_constraint_count == 0,
                    "late out-of-reach Action does not create traversal or hand support", s);
            require(s.set_move_input(0, 0), "neutralize after rejected late Action", s);
            continue;
        }
        tick(s);
        const auto after = s.snapshot();
        unsupported = unsupported || (!after.player_grounded && after.support_entity_id == 0);
        descending = descending || (!after.player_grounded && after.player_linear_velocity.y < -1.0);
        if (on_support_top(after, 381.9, kRouteEntity)) {
            recovered = true;
            break;
        }
    }
    require(late_request_sent && unsupported && descending && recovered &&
                s.snapshot().landing_count > entry.landing_count &&
                s.snapshot().death_count == entry.death_count,
            "rejected late catch produces unsupported descent and inboard-shelf recovery", s);
    (void)s.set_move_input(0, 0);
    tick(s, 30);
    require(on_support_top(s.snapshot(), 381.9, kRouteEntity),
            "inboard recovery shelf settles before the retry", s);
}

bool walk_to(Simulation &s, double target_x, double target_z, int budget = 1800) {
    for (int i = 0; i < budget; ++i) {
        const auto v = s.snapshot();
        const double dx = target_x - v.player_position.x;
        const double dz = target_z - v.player_position.z;
        if (std::hypot(dx, dz) < .08 &&
            std::hypot(v.player_linear_velocity.x, v.player_linear_velocity.z) < .16) {
            (void)s.set_move_input(0, 0);
            return true;
        }
        const double ix = dx * 1.8 - v.player_linear_velocity.x * .28;
        const double iz = dz * 1.8 - v.player_linear_velocity.z * .28;
        const double scale = std::max(1.0, std::hypot(ix, iz));
        require(s.set_move_input(ix / scale, iz / scale), "ordinary onward movement accepted", s);
        if (std::hypot(dx, dz) > .02)
            require(s.set_facing(dx, dz), "onward facing input accepted", s);
        tick(s);
        if (!s.snapshot().player_grounded || s.snapshot().support_entity_id != kWorld) return false;
    }
    return false;
}

void walk_on_second_shelf_to_final_grip(Simulation &s) {
    const auto start = s.snapshot();
    require(start.player_grounded && start.support_entity_id == kRouteEntity &&
                std::abs(soles_y(start) - 389.4) < .18 &&
                start.player_position.x > -25.35 && start.player_position.x < -24.60,
            "final-grip approach starts on the real outboard shelf", s);
    require(s.set_facing(0, 1), "face along the second shelf toward world +Z", s);
    bool arrived = false;
    for (int i = 0; i < 180; ++i) {
        const auto v = s.snapshot();
        const double dz = -138.0 - v.player_position.z;
        if (std::abs(dz) < .06 && std::abs(v.player_linear_velocity.z) < .16) {
            arrived = true;
            break;
        }
        const double iz = std::clamp(dz * 2.0 - v.player_linear_velocity.z * .28, -1.0, 1.0);
        require(s.set_move_input(0, iz), "ordinary shelf traverse input accepted", s);
        tick(s);
        const auto after = s.snapshot();
        require(after.player_grounded && after.support_entity_id == kRouteEntity &&
                    std::abs(soles_y(after) - 389.4) < .18 &&
                    after.player_position.x > -25.35 && after.player_position.x < -24.60,
                "player remains on the narrow physical second shelf during traverse", s);
    }
    (void)s.set_move_input(0, 0);
    tick(s, 20);
    const auto settled = s.snapshot();
    require(arrived && settled.player_grounded && settled.support_entity_id == kRouteEntity &&
                std::abs(soles_y(settled) - 389.4) < .18 &&
                std::abs(settled.player_position.z + 138.0) < .12 &&
                settled.player_position.z - start.player_position.z > .1,
            "grounded +Z traverse reaches the final-grip staging point", s);
}

void run_route() {
    Simulation s;
    require(s.entity_body_count(kRouteEntity) > 0,
            "native static route body is part of the real world", s);
    // The only staged pose is supported on the existing lower receiver.
    require(s.debug_restart_at({-24.70, 375.15, -139.5}),
            "supported lower west deck staging accepted", s);
    tick(s, 45);
    const auto lower = s.snapshot();
    require(lower.player_grounded && lower.support_entity_id == kWorld &&
                std::abs(lower.player_position.y - 375.15) < .12,
            "route starts on the real 374.25 m receiver", s);

    require(s.set_facing(1, 0), "face the first brace grip", s);
    tick(s, 30);
    require(s.snapshot().grip_available && s.snapshot().grip_entity_id == kRouteEntity,
            "first grip is reachable on the authored route body", s);
    require(s.request_traversal(), "ordinary Action requests first climb", s);
    tick(s);
    require_climbing(s, "first Action engages two actual hands");
    const auto entry_transfer = s.snapshot().foot_transfer_count;
    climb_to_shelf(s, 381.9, .55, 18 * 90,
                   "first diagonal climb tops out on the real381.9m recovery shelf");
    require(s.snapshot().foot_transfer_count > entry_transfer &&
                s.snapshot().foot_transfer_support_entity_id == kRouteEntity &&
                s.snapshot().foot_transfer_peak_hand_load_n > 0,
            "first shelf receives a measured hand-to-foot transfer", s);
    require(s.snapshot().player_position.x > -24.18 &&
                s.snapshot().player_position.z > -138.55 && s.snapshot().player_position.z < -136.85,
            "first shelf landing is on the inboard side of its rail", s);

    // Test a real missed Action on the lower shelf, then retry the same bar.
    // The miss remains under the actual jump, gravity, and landing rules.
    miss_apex_and_recover(s);
    jump_and_catch_bar(s, "ordinary retry Action recatches the horizontal bar");
    traverse_laterally(s, -1.0, 90,
                       "bar traverse reaches the reverse rail at world +Z and changes hand anchors");

    // Reverse diagonal ascent from the bar must transfer body weight to the
    // second real shelf.
    const auto second_transfer = s.snapshot().foot_transfer_count;
    climb_to_shelf(s, 389.4, 0.5, 24 * 90,
                   "reverse diagonal ascent reaches the real389.4m shelf",
                   kRouteEntity, -1.0);
    require(s.snapshot().foot_transfer_count > second_transfer &&
                s.snapshot().foot_transfer_support_entity_id == kRouteEntity &&
                s.snapshot().foot_transfer_peak_hand_load_n > 0,
            "second shelf receives a measured hand-to-foot transfer", s);
    require(s.snapshot().player_position.x > -25.35 && s.snapshot().player_position.x < -24.60,
            "second shelf landing is inside its revised outboard footing bounds", s);

    walk_on_second_shelf_to_final_grip(s);
    const auto final_transfer = s.snapshot().foot_transfer_count;
    climb_to_shelf(s, 396.25, -.45, 24 * 90,
                   "final rising hand line mantles onto the existing396.25m receiver", kWorld,
                   1.0);
    const auto upper = s.snapshot();
    require(upper.player_grounded && upper.support_entity_id == kWorld &&
                std::abs(soles_y(upper) - 396.25) < .18 && upper.death_count == lower.death_count,
            "top-out lands on the actual396.25m world floor", s);
    require(upper.foot_transfer_count > final_transfer &&
                upper.foot_transfer_support_entity_id == kWorld &&
                upper.foot_transfer_peak_hand_load_n > 0,
            "receiver top-out follows a measured loaded hand-to-foot transfer", s);
    require(walk_to(s, -22.5, -142.0), "upper receiver supports ordinary onward walking", s);
    const auto onward = s.snapshot();
    require(onward.player_grounded && onward.support_entity_id == kWorld &&
                std::abs(soles_y(onward) - 396.25) < .18 && onward.death_count == lower.death_count,
            "ordinary onward route remains on the real upper receiver", s);
    std::cout << "PASS WEST_BRACE_BAY lower374.25/diagonal-climb381.9/jump-Action-bar/lateral-miss-recovery-retry/reverse-ascent389.4/final-mantle396.25/onward ordinary_inputs_only=1\n";
}
} // namespace

int main() {
    run_route();
    return EXIT_SUCCESS;
}
