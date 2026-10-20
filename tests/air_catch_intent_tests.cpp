#include "sim/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

using namespace scraperx::sim;

namespace {
unsigned checks = 0;

void require(bool condition, const char *why, const Simulation &s) {
    ++checks;
    if (condition) return;
    const auto v = s.snapshot();
    std::cerr << "FAIL AIR_CATCH " << why << " tick=" << v.tick_index
              << " position=" << v.player_position.x << ',' << v.player_position.y << ','
              << v.player_position.z << " velocity_y=" << v.player_linear_velocity.y
              << " grip=" << v.grip_available << " hands=" << v.traversal_hand_constraint_count
              << " rejected=" << v.rejected_traversal_count << '\n';
    std::exit(EXIT_FAILURE);
}

void tick(Simulation &s, unsigned count = 1, unsigned caller_frames = 1) {
    for (unsigned i = 0; i < count; ++i) {
        const auto before = s.snapshot().tick_index;
        unsigned advanced = 0;
        for (unsigned frame = 0; frame < caller_frames; ++frame) {
            const auto result = s.advance_frame(Simulation::kFixedStepSeconds / caller_frames);
            require(result.accepted, "caller frame accepted", s);
            advanced += result.steps_advanced;
        }
        require(advanced == 1 && s.snapshot().tick_index == before + 1,
                "caller frames advance exactly one native tick", s);
        require(s.snapshot().player_gravity_factor == 1 && s.snapshot().death_count == 0,
                "gravity remains active without death or rescue", s);
    }
}

void stage(Simulation &s) {
    // One initial condition beside the existing shipping gantry. Subsequent
    // approach is an ordinary fall; no velocity, grip or support is injected.
    require(s.debug_restart_at({16.85, 15, -118}) && s.set_facing(1, 0) &&
                s.set_move_input(0, 0) && s.set_crouch_input(false),
            "stage free fall beside shipping gantry", s);
}

void unsupported(const Simulation &s) {
    const auto v = s.snapshot();
    require(!v.player_grounded && !v.foot_push_active &&
                v.traversal_hand_constraint_count == 0 && !v.hand_foot_overlap_active,
            "pending or cancelled input creates no physical support", s);
}

unsigned find_reach(Simulation &s) {
    stage(s);
    for (unsigned frame = 1; frame <= 100; ++frame) {
        tick(s);
        unsupported(s);
        if (s.snapshot().grip_available) {
            require(s.snapshot().grip_entity_id == 2952 && frame > 20 &&
                        s.snapshot().player_linear_velocity.y < -1,
                    "actual airborne approach reaches the existing gantry", s);
            std::cout << "AIR_CATCH first_reach=" << frame << " y="
                      << s.snapshot().player_position.y << '\n';
            return frame;
        }
    }
    require(false, "approach reaches a real grip within its bounded fall", s);
    return 0;
}

void attached(const Simulation &s) {
    const auto v = s.snapshot();
    require(v.traversal_hand_constraint_count > 0 && v.traversal_support_entity_id == 2952 &&
                !v.player_grounded && !v.foot_push_active && !v.hand_foot_overlap_active,
            "actual finite hands catch the gantry without invented footing", s);
    const auto magnitude = [](Vector3 f) { return std::hypot(f.x, f.y, f.z); };
    require(magnitude(v.traversal_left_hand_force) <= 1500.05 &&
                magnitude(v.traversal_right_hand_force) <= 1500.05,
            "catch retains existing solved per-hand force limits", s);
}

void drop(Simulation &s) {
    require(s.request_release(), "Drop releases an actual catch", s);
    tick(s);
    unsupported(s);
    tick(s, 3);
    unsupported(s);
}

std::vector<Snapshot> early(Simulation &s, unsigned reach, unsigned caller_frames) {
    stage(s);
    tick(s, reach - 3, caller_frames);
    require(!s.snapshot().grip_available, "early Action precedes real hand reach", s);
    const auto rejected = s.snapshot().rejected_traversal_count;
    require(s.request_traversal(), "one early Action accepted", s);
    std::vector<Snapshot> trace;
    bool caught = false;
    unsigned wait = 0;
    for (unsigned i = 0; i < 12; ++i) {
        const auto before = s.snapshot();
        tick(s, 1, caller_frames);
        const auto v = s.snapshot();
        trace.push_back(v);
        if (v.traversal_hand_constraint_count > 0) {
            require(!v.air_catch_intent_pending, "physical acquisition consumes pending reach", s);
            if (!caught) {
                require(before.grip_available && i > 0 && i < 11,
                        "buffer catches only after current geometry enters real reach", s);
                wait = i + 1;
            }
            caught = true;
            attached(s);
        } else {
            require(!caught, "accepted catch remains attached during its initial loaded interval", s);
            require(v.air_catch_intent_pending, "waiting intent is visible without physical support", s);
            unsupported(s);
        }
        require(v.rejected_traversal_count == rejected,
                "one successful buffered Action is consumed without later rejection", s);
    }
    require(caught && s.snapshot().actual_hand_load_n > 50,
            "early Action produces a genuinely loaded catch", s);
    std::cout << "AIR_CATCH early frames_per_tick=" << caller_frames << " waited=" << wait
              << " load_N=" << s.snapshot().actual_hand_load_n << '\n';
    drop(s);
    return trace;
}

void expiry(Simulation &s, unsigned reach) {
    stage(s);
    tick(s, reach - 14);
    const auto rejected = s.snapshot().rejected_traversal_count;
    require(!s.snapshot().grip_available && s.request_traversal(), "over-early Action accepted", s);
    for (unsigned i = 0; i < 15; ++i) { tick(s); unsupported(s); }
    require(s.snapshot().grip_available && s.snapshot().rejected_traversal_count == rejected + 1,
            "expired Action rejects once and cannot catch when reach arrives later", s);
    require(!s.snapshot().air_catch_intent_pending, "expiry clears the published pending intent", s);
    require(s.request_traversal(), "fresh Action after expiry accepted", s);
    tick(s);
    attached(s);
    drop(s);
}

void cancel(Simulation &s, unsigned reach, bool crouch) {
    stage(s);
    tick(s, reach - 3);
    const auto rejected = s.snapshot().rejected_traversal_count;
    require(s.request_traversal(), "early Action before cancellation accepted", s);
    tick(s);
    unsupported(s);
    if (crouch) require(s.set_crouch_input(true), "crouch cancellation accepted", s);
    else require(!s.request_release(), "unattached Drop cancels intent while reporting no held body", s);
    tick(s);
    if (crouch) require(s.set_crouch_input(false), "release crouch before reaching grip", s);
    tick(s, 3);
    unsupported(s);
    require(s.snapshot().grip_available && s.snapshot().rejected_traversal_count == rejected,
            "cancellation prevents a later automatic catch or expiry rejection", s);
    require(!s.snapshot().air_catch_intent_pending, "cancellation clears the published pending intent", s);
    require(s.request_traversal(), "fresh Action after cancellation accepted", s);
    tick(s);
    attached(s);
    drop(s);
}

void simultaneous_drop(Simulation &s, unsigned reach, bool drop_first) {
    stage(s);
    tick(s, reach);
    require(s.snapshot().grip_available, "simultaneous input begins at actual reach", s);
    if (drop_first) {
        require(!s.request_release(), "unattached Drop accepted as cancellation", s);
        require(s.request_traversal(), "Action after same-batch Drop queued", s);
    } else {
        require(s.request_traversal(), "Action before same-batch Drop queued", s);
        require(!s.request_release(), "same-batch Drop cancels pending Action", s);
    }
    tick(s, 2);
    unsupported(s);
    require(s.request_traversal(), "later fresh Action after same-batch cancellation accepted", s);
    tick(s);
    attached(s);
    drop(s);
}

void restart(Simulation &s, unsigned reach) {
    stage(s);
    tick(s, reach - 3);
    require(s.request_traversal(), "pending Action before restart accepted", s);
    tick(s);
    // Stage another actual approach through the production restart owner.
    // The old Action must not survive, even when the same grip is reachable.
    stage(s);
    tick(s, reach + 1);
    unsupported(s);
    require(s.snapshot().grip_available, "restart negative still reaches the original legal grip", s);
    require(s.request_traversal(), "fresh Action after restart accepted", s);
    tick(s);
    attached(s);
    drop(s);
}
} // namespace

int main(int argc, char **argv) {
    Simulation s;
    tick(s, 180);
    const unsigned reach = find_reach(s);
    if (argc > 1 && std::string_view(argv[1]) == "baseline") {
        require(s.request_traversal(), "baseline fresh Action at actual reach accepted", s);
        tick(s);
        attached(s);
        std::cout << "AIR_CATCH BASELINE PASS checks=" << checks << '\n';
        return EXIT_SUCCESS;
    }
    const auto normal = early(s, reach, 1);
    // A caught gantry retains real motion after Drop. Compare caller rates
    // from identical world histories, rather than reusing that loaded support.
    const auto fast = [&] {
        Simulation same_start;
        tick(same_start, 180);
        const auto same_reach = find_reach(same_start);
        require(same_reach == reach,
                "equivalent replay starts at the same real grip reach", same_start);
        return early(same_start, same_reach, 4);
    }();
    require(normal.size() == fast.size(), "caller-rate traces cover the same native ticks", s);
    for (unsigned i = 0; i < normal.size(); ++i) {
        const auto a = normal[i], b = fast[i];
        require(a.traversal_hand_constraint_count == b.traversal_hand_constraint_count &&
                    a.traversal_state == b.traversal_state &&
                    std::hypot(a.player_position.x - b.player_position.x,
                               a.player_position.y - b.player_position.y,
                               a.player_position.z - b.player_position.z) < .0001 &&
                    std::abs(a.actual_hand_load_n - b.actual_hand_load_n) < .1,
                "90 and 360 Hz callers make equivalent catches within numerical tolerance", s);
    }
    expiry(s, reach);
    cancel(s, reach, false);
    cancel(s, reach, true);
    simultaneous_drop(s, reach, false);
    simultaneous_drop(s, reach, true);
    restart(s, reach);
    std::cout << "SCRAPERX_AIR_CATCH PASS checks=" << checks << '\n';
}
