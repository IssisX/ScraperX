#include "sim/simulation.hpp"
#include "sim/workshop_roof_route.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace scraperx::sim;

namespace {
void require(bool ok, const char *message, const Simulation &sim) {
    if (ok) return;
    const auto s = sim.snapshot();
    std::cerr << "FAIL HANG_INTENT " << message << " tick=" << s.tick_index
              << " p=" << s.player_position.x << ',' << s.player_position.y << ',' << s.player_position.z
              << " state=" << int(s.traversal_state) << " hands=" << s.traversal_hand_constraint_count
              << " plan=" << int(s.traversal_plan_kind) << " accepted=" << s.accepted_traversal_count
              << " support=" << s.support_entity_id << '\n';
    std::exit(EXIT_FAILURE);
}

void advance(Simulation &sim, unsigned ticks = 1) {
    const auto before = sim.snapshot().tick_index;
    require(sim.advance_frame(Simulation::kFixedStepSeconds * ticks).accepted,
            "caller frame accepted", sim);
    const auto s = sim.snapshot();
    require(s.tick_index == before + ticks, "caller frame delivers every native tick", sim);
    require(s.death_count == 0 && s.player_gravity_factor == 1,
            "real gravity and survival throughout fixture", sim);
}

double shoulder_reach(const Snapshot &s) {
    return std::hypot(s.traversal_ledge_point.x - s.player_position.x,
                      s.traversal_ledge_point.y - s.player_position.y - .45,
                      s.traversal_ledge_point.z - s.player_position.z);
}

Snapshot catch_roof(Simulation &sim) {
    // Only initial staging uses a debug pose. Every subsequent approach,
    // tread, takeoff and catch runs through ordinary production input.
    require(sim.debug_restart_at({-36, .9, -48.5}), "stage actual grade before service stairs", sim);
    for (unsigned i = 0; i < 45; ++i) advance(sim);
    require(sim.snapshot().player_grounded && sim.snapshot().support_entity_id == 1,
            "staging settles onto actual grade", sim);
    require(sim.set_facing(0, 1) && sim.set_move_input(0, .75), "walk actual service stairs", sim);
    bool plinth = false, foot = false;
    for (unsigned i = 0; i < 450; ++i) {
        advance(sim);
        const auto s = sim.snapshot();
        foot = foot || s.foot_push_active;
        if (s.player_position.z > -43.9 && s.player_grounded &&
                s.support_entity_id == kWorkshopRoofAccessEntity &&
                std::abs(s.player_position.y - kWorkshopLaunchTopM - .9) < .1) {
            plinth = true;
            break;
        }
    }
    require(plinth && foot, "real finite feet reach actual launch plinth", sim);
    require(sim.set_move_input(0, 1) && sim.set_sprint_input(true), "run toward actual launch edge", sim);
    bool jumped = false;
    for (unsigned i = 0; i < 90; ++i) {
        advance(sim);
        const auto s = sim.snapshot();
        if (s.player_grounded && s.support_entity_id == kWorkshopRoofAccessEntity &&
                s.player_position.z > -41.8) {
            require(s.player_linear_velocity.z > 3 && sim.request_jump(), "earned running Jump", sim);
            jumped = true;
            break;
        }
    }
    require(jumped, "takeoff occurs on real footing", sim);
    bool wall_requested = false;
    for (unsigned i = 0; i < 180; ++i) {
        advance(sim);
        const auto s = sim.snapshot();
        if (s.traversal_hand_constraint_count == 2 && s.traversal_state == TraversalState::Hanging) {
            require(s.traversal_plan_kind == 0 && shoulder_reach(s) > 1.05,
                    "first actual two-hand catch is outside existing top-out reach", sim);
            require(s.traversal_left_hand_generation > 0 && s.traversal_right_hand_generation > 0,
                    "catch has two actual grip identities", sim);
            std::cout << "HANG_CATCH tick=" << s.tick_index << " reach_m=" << shoulder_reach(s)
                      << " grips=" << s.traversal_left_hand_generation << ','
                      << s.traversal_right_hand_generation << '\n';
            return s;
        }
        if (!wall_requested && s.traversal_hand_constraint_count == 0 && i > 20 &&
                !s.player_grounded && s.player_linear_velocity.y <= .05) {
            require(sim.request_jump(), "ordinary airborne wall/catch input", sim);
            wall_requested = true;
        }
    }
    require(false, "bounded public approach catches actual workshop roof", sim);
    return {};
}

void success(Simulation &sim, bool action) {
    const auto caught = catch_roof(sim);
    require(action ? sim.request_traversal() : sim.request_jump(), "one early top-out request", sim);
    advance(sim);
    require(sim.snapshot().traversal_plan_kind == 0,
            "outside-reach request does not create an immediate plan", sim);
    bool started = false;
    for (unsigned elapsed = 1; elapsed <= 11; ++elapsed) {
        const auto s = sim.snapshot();
        if (s.traversal_plan_kind != 0) {
            require(s.traversal_plan_kind == 1 && s.traversal_plan_generation > caught.traversal_plan_generation,
                    "deferred request starts one real physical top-out", sim);
            started = true;
            std::cout << "HANG_START elapsed_ticks=" << s.tick_index - caught.tick_index
                      << " grouped=" << action << '\n';
            break;
        }
        require(s.traversal_left_hand_generation == caught.traversal_left_hand_generation &&
                s.traversal_right_hand_generation == caught.traversal_right_hand_generation &&
                s.traversal_hand_constraint_count == 2,
                "pending input remains on its exact original two grips", sim);
        if (elapsed == 11) break;
        advance(sim, action && elapsed < 10 ? 2 : 1);
        // The native tick identity, rather than the number of caller frames,
        // bounds the transaction when an Action frame contains two ticks.
        if (sim.snapshot().tick_index - caught.tick_index > 11) break;
    }
    require(started && sim.snapshot().tick_index - caught.tick_index <= 11,
            "single early input is consumed within eleven native ticks", sim);
    bool received = false;
    for (unsigned i = 0; i < 240; ++i) {
        advance(sim, action ? 2 : 1);
        const auto s = sim.snapshot();
        require(s.accepted_traversal_count <= caught.accepted_traversal_count + 1,
                "buffer never replays a second completion", sim);
        if (s.traversal_hand_constraint_count == 0 && s.player_grounded &&
                s.support_entity_id == Simulation::kWorldSolidEntityId &&
                std::abs(s.player_position.y - kWorkshopRoofTopM - .9) < .15) {
            require(s.accepted_traversal_count == caught.accepted_traversal_count + 1,
                    "exactly one physical transfer earns actual roof support", sim);
            require(sim.set_move_input(0, 0) && sim.set_sprint_input(false), "stop on receiving roof", sim);
            received = true;
            break;
        }
    }
    require(received, "accepted top-out reaches real workshop roof", sim);
    for (unsigned i = 0; i < 90; ++i) advance(sim);
    require(sim.snapshot().accepted_traversal_count == caught.accepted_traversal_count + 1 &&
            sim.snapshot().traversal_plan_kind == 0 && sim.snapshot().player_grounded &&
            sim.snapshot().support_entity_id == Simulation::kWorldSolidEntityId,
            "completed transaction stays consumed on actual roof footing", sim);
}

std::vector<Snapshot> cancellation_trace(bool early_action) {
    Simulation sim(InitialSpawn::ExteriorGrade);
    const auto caught = catch_roof(sim);
    if (early_action)
        require(sim.request_traversal(), "one early Action before posture cancellation", sim);
    advance(sim);
    std::vector<Snapshot> trace{sim.snapshot()};
    require(sim.set_crouch_input(true), "paired held posture cancellation", sim);
    advance(sim);
    trace.push_back(sim.snapshot());
    require(sim.set_crouch_input(false) && sim.set_move_input(0, 0) && sim.set_sprint_input(false),
            "paired neutral input without a fresh Action", sim);
    for (unsigned i = 0; i < 90; ++i) {
        advance(sim);
        trace.push_back(sim.snapshot());
    }
    for (const auto &s : trace)
        require(s.traversal_plan_kind == 0 && s.accepted_traversal_count == caught.accepted_traversal_count,
                "cancelled input never starts or replays a physical top-out", sim);
    return trace;
}

void cancellation(Simulation &diagnostic) {
    // This actual Crouch/neutral sequence eventually loses the baseline
    // radial hang too. Cancellation must not change that physical outcome.
    const auto baseline = cancellation_trace(false);
    const auto buffered = cancellation_trace(true);
    require(baseline.size() == buffered.size(), "paired traces cover the same native ticks", diagnostic);
    double peak_position_delta = 0, peak_velocity_delta = 0, peak_hand_load_delta = 0;
    for (unsigned i = 0; i < baseline.size(); ++i) {
        const auto &a = baseline[i], &b = buffered[i];
        const double position_delta = std::hypot(a.player_position.x - b.player_position.x,
            a.player_position.y - b.player_position.y, a.player_position.z - b.player_position.z);
        const double velocity_delta = std::hypot(a.player_linear_velocity.x - b.player_linear_velocity.x,
            a.player_linear_velocity.y - b.player_linear_velocity.y,
            a.player_linear_velocity.z - b.player_linear_velocity.z);
        peak_position_delta = std::max(peak_position_delta, position_delta);
        peak_velocity_delta = std::max(peak_velocity_delta, velocity_delta);
        peak_hand_load_delta = std::max(peak_hand_load_delta, std::abs(a.actual_hand_load_n - b.actual_hand_load_n));
        require(position_delta < 1e-9 && velocity_delta < 1e-9 &&
                std::abs(a.actual_hand_load_n - b.actual_hand_load_n) < 1e-9 &&
                std::abs(a.actual_foot_load_n - b.actual_foot_load_n) < 1e-9 &&
                a.tick_index == b.tick_index && a.player_grounded == b.player_grounded &&
                a.support_entity_id == b.support_entity_id && a.player_crouched == b.player_crouched &&
                a.foot_push_active == b.foot_push_active && a.traversal_state == b.traversal_state &&
                a.traversal_hand_constraint_count == b.traversal_hand_constraint_count &&
                a.traversal_left_hand_generation == b.traversal_left_hand_generation &&
                a.traversal_right_hand_generation == b.traversal_right_hand_generation,
                "cancelling pending Action preserves baseline motion, loads and contact on every tick", diagnostic);
    }
    std::cout << "HANG_CANCEL_PAIR samples=" << baseline.size()
              << " peak_position_delta_m=" << peak_position_delta
              << " peak_velocity_delta_mps=" << peak_velocity_delta
              << " peak_hand_load_delta_n=" << peak_hand_load_delta << '\n';
}

void negative(Simulation &sim, const std::string &mode) {
    const auto caught = catch_roof(sim);
    require(sim.request_traversal(), "one early Action before interruption", sim);
    advance(sim);
    require(sim.snapshot().traversal_plan_kind == 0, "interruption precedes physical top-out", sim);
    if (mode == "release") {
        require(sim.request_release(), "explicitly release actual caught hands", sim);
        advance(sim);
        require(sim.snapshot().traversal_hand_constraint_count == 0, "release removes actual hand support", sim);
    } else {
        require(sim.debug_restart_at({-36, .9, -48.5}), "explicit restart clears old grip transaction", sim);
    }
    require(sim.set_move_input(0, 0) && sim.set_sprint_input(false), "neutral input after interruption", sim);
    const auto baseline = sim.snapshot().accepted_traversal_count;
    for (unsigned i = 0; i < 90; ++i) {
        advance(sim);
        require(sim.snapshot().traversal_plan_kind == 0 &&
                sim.snapshot().accepted_traversal_count == baseline,
                "interrupted input never creates or completes a top-out", sim);
    }
    if (mode == "restart")
        require(sim.snapshot().player_grounded && sim.snapshot().support_entity_id == 1,
                "restart returns to actual grade without stale traversal", sim);
    (void)caught;
}
} // namespace

int main(int argc, char **argv) {
    std::cout << std::unitbuf;
    const std::string mode = argc > 1 ? argv[1] : "jump";
    Simulation sim(InitialSpawn::ExteriorGrade);
    if (mode == "jump" || mode == "action") success(sim, mode == "action");
    else if (mode == "cancel") cancellation(sim);
    else if (mode == "release" || mode == "restart") negative(sim, mode);
    else require(false, "known test mode required", sim);
    std::cout << "PASS PHYSICAL_HANG_INTENT mode=" << mode << '\n';
}
