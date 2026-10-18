#include "sim/simulation.hpp"
#include "sim/physical_foot_push.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace scraperx::sim;

namespace {
constexpr double kReceiverX = -38.5;
constexpr double kReceiverZ = -40.6;
constexpr double kReceiverTop = .65;

void require(bool ok, const char *operation, const Simulation &simulation) {
    if (ok) return;
    const auto s = simulation.snapshot();
    std::cerr << "FAIL WALL_EXIT " << operation << " tick=" << s.tick_index
              << " p=" << s.player_position.x << ',' << s.player_position.y << ',' << s.player_position.z
              << " v=" << s.player_linear_velocity.x << ',' << s.player_linear_velocity.y << ','
              << s.player_linear_velocity.z << " grounded=" << s.player_grounded
              << " support=" << s.support_entity_id << " plants=" << s.wall_stride_count
              << " active=" << s.foot_push_active << " reason=" << unsigned(s.foot_push_stop_reason) << '\n';
    std::exit(EXIT_FAILURE);
}

void report(const char *stage, const Simulation &simulation) {
    const auto s = simulation.snapshot();
    std::cout << "WALL_EXIT " << stage << " tick=" << s.tick_index
              << " p=" << s.player_position.x << ',' << s.player_position.y << ',' << s.player_position.z
              << " v=" << s.player_linear_velocity.x << ',' << s.player_linear_velocity.y << ','
              << s.player_linear_velocity.z << " support=" << s.support_entity_id
              << " plants=" << s.wall_stride_count << '\n';
}

Snapshot tick(Simulation &simulation) {
    require(simulation.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "ordinary fixed tick accepted", simulation);
    const auto s = simulation.snapshot();
    require(s.death_count == 0 && s.player_gravity_factor == 1 &&
            s.traversal_hand_constraint_count == 0 && s.traversal_state == TraversalState::None &&
            s.fixed_step_seconds == Simulation::kFixedStepSeconds,
            "live ordinary gravity and fixed cadence without hand traversal", simulation);
    require(s.foot_push_peak_load_n <= PhysicalFootPush::force_bound_n() + .01 &&
            s.foot_push_command_work_bound_j <= PhysicalFootPush::command_budget_j() + .01 &&
            s.foot_push_stroke_m <= PhysicalFootPush::maximum_stroke_m() + .000001 &&
            s.foot_push_elapsed_seconds <= PhysicalFootPush::duration_seconds() + .000001,
            "unchanged finite per-plant force, work, stroke and time", simulation);
    return s;
}

void steer(Simulation &simulation, double x, double z, double cap = .6) {
    const auto s = simulation.snapshot();
    require(simulation.set_move_input(
                std::clamp(1.3 * (x - s.player_position.x) - .45 * s.player_linear_velocity.x, -cap, cap),
                std::clamp(1.3 * (z - s.player_position.z) - .45 * s.player_linear_velocity.z, -cap, cap)),
            "ordinary world-space movement input accepted", simulation);
}

void arrive_on_grade(Simulation &simulation, double x, double z) {
    require(simulation.set_sprint_input(false), "ordinary walk selected", simulation);
    for (unsigned i = 0; i < 1800; ++i) {
        const auto s = simulation.snapshot();
        if (std::hypot(x - s.player_position.x, z - s.player_position.z) < .18 &&
            s.player_grounded && s.support_entity_id == 1 && std::abs(s.player_position.y - .9) < .05 &&
            std::hypot(s.player_linear_velocity.x, s.player_linear_velocity.z) < .4) {
            require(simulation.set_move_input(0, 0), "arrival releases ordinary movement", simulation);
            for (unsigned settle = 0; settle < 12; ++settle) tick(simulation);
            report("grade-arrival", simulation);
            return;
        }
        steer(simulation, x, z);
        tick(simulation);
    }
    require(false, "bounded ordinary ground approach and recovery", simulation);
}

struct Episode {
    std::uint64_t base_count = 0;
    bool first = false, unloaded = false, second = false;
    std::uint64_t first_contact = 0, first_start = 0, unloaded_tick = 0;
    double first_x = 0, first_work = 0, second_work = 0;

    Snapshot step(Simulation &simulation) {
        const auto s = tick(simulation);
        require(s.wall_stride_count >= base_count && s.wall_stride_count <= base_count + 2,
                "each earned takeoff permits at most two wall plants", simulation);
        if (s.wall_stride_count == base_count + 1) first_work = std::max(first_work, s.foot_push_command_work_bound_j);
        if (s.wall_stride_count == base_count + 2) second_work = std::max(second_work, s.foot_push_command_work_bound_j);
        require(first_work + second_work <= 2 * PhysicalFootPush::command_budget_j() + .02,
                "two original finite plant work ledgers", simulation);
        if (!first && s.wall_stride_count == base_count + 1 && s.foot_push_last_load_n > 0) {
            require(s.wall_stride_active && s.foot_push_support_entity_id == Simulation::kWorldSolidEntityId &&
                    s.wall_stride_normal.z < -.9, "first loaded contact is the real workshop wall", simulation);
            first = true;
            first_contact = s.foot_push_source_contact_tick;
            first_start = s.foot_push_start_count;
            first_x = s.player_position.x;
            report("first-wall-plant", simulation);
        }
        if (first && !second && s.wall_stride_count == base_count + 1 && !s.foot_push_active && !unloaded) {
            require(s.foot_push_stop_reason == unsigned(PhysicalFootPush::StopReason::Cleared),
                    "first wall row retires through its actual handoff", simulation);
            unloaded = true;
            unloaded_tick = s.tick_index;
        }
        if (!second && s.wall_stride_count == base_count + 2 && s.foot_push_last_load_n > 0) {
            require(first && unloaded && s.foot_push_source_contact_tick >= unloaded_tick &&
                    s.foot_push_source_contact_tick > first_contact &&
                    s.foot_push_start_count == first_start + 1 &&
                    s.foot_push_support_entity_id == Simulation::kWorldSolidEntityId &&
                    s.wall_stride_normal.z < -.9 && s.player_position.x > first_x + .15,
                    "second loaded material contact follows a solve without the first row", simulation);
            second = true;
            report("second-wall-plant", simulation);
        }
        return s;
    }
};

Episode run_to_second_plant(Simulation &simulation) {
    const auto base_count = simulation.snapshot().wall_stride_count;
    arrive_on_grade(simulation, -46, -38.45);
    require(simulation.set_facing(1, 0) && simulation.set_move_input(1, 0) && simulation.set_sprint_input(true),
            "ordinary along-wall sprint", simulation);
    bool running = false;
    for (unsigned i = 0; i < 180; ++i) {
        const auto s = tick(simulation);
        require(s.wall_stride_count == base_count, "ground approach cannot supply wall plants", simulation);
        if (s.player_grounded && s.player_linear_velocity.x > 6.7) { running = true; break; }
    }
    require(running && simulation.set_move_input(.9682458, .25) && simulation.request_jump(),
            "earned sprint-jump with deliberate along-wall and inward intent", simulation);
    const auto takeoff = tick(simulation);
    require(!takeoff.player_grounded && takeoff.player_linear_velocity.y > 4,
            "actual ground takeoff precedes wall use", simulation);
    Episode episode;
    episode.base_count = base_count;
    for (unsigned i = 0; i < 180; ++i) {
        episode.step(simulation);
        if (episode.second) return episode;
    }
    require(false, "bounded actual two-plant wall run", simulation);
    return episode;
}

void kick_existing_plant(Simulation &simulation, Episode &episode) {
    const auto before = simulation.snapshot();
    require(simulation.set_move_input(0, 0) && simulation.request_jump(),
            "deliberate Jump spends existing second wall plant", simulation);
    const auto s = episode.step(simulation);
    require(s.wall_stride_count == episode.base_count + 2 && s.wall_stride_active && s.wall_stride_pushing_off &&
            s.foot_push_start_count == before.foot_push_start_count &&
            s.foot_push_source_contact_tick == before.foot_push_source_contact_tick &&
            s.foot_push_initial_distance_m == before.foot_push_initial_distance_m &&
            s.foot_push_elapsed_seconds > before.foot_push_elapsed_seconds &&
            s.foot_push_command_work_bound_j >= before.foot_push_command_work_bound_j,
            "push-off retains actual contact, age and spent work without refill", simulation);
    report("same-plant-kick", simulation);
}

void receive_and_exit(Simulation &simulation, Episode &episode) {
    require(simulation.set_sprint_input(false), "leave sprint after wall push-off", simulation);
    bool contact = false, received = false;
    unsigned firm_ticks = 0;
    for (unsigned i = 0; i < 360; ++i) {
        steer(simulation, kReceiverX, kReceiverZ, .9);
        const auto s = episode.step(simulation);
        require(s.wall_stride_count == episode.base_count + 2, "outward steering cannot buy another wall plant", simulation);
        if (i % 12 == 0 && !received) report("receiving-flight", simulation);
        if (s.player_grounded) {
            require(s.support_entity_id == Simulation::kWorldSolidEntityId,
                    "receiving attempt cannot substitute yard grade for the actual steel", simulation);
            if (!contact) report("steel-edge-contact", simulation);
            contact = true;
            const bool on_top = std::abs(s.player_position.y - (kReceiverTop + .9)) < .06 &&
                s.player_position.x > kReceiverX - 2 && s.player_position.x < kReceiverX + 2 &&
                s.player_position.z > kReceiverZ - 1.4 && s.player_position.z < kReceiverZ + 1.4;
            if (on_top && !received) report("steel-received", simulation);
            received = received || on_top;
            if (on_top && std::hypot(s.player_linear_velocity.x, s.player_linear_velocity.z) < .4 &&
                !s.foot_push_active) ++firm_ticks;
            else firm_ticks = 0;
            if (firm_ticks == 90) break;
        } else firm_ticks = 0;
    }
    require(received && firm_ticks == 90, "ninety consecutive slow supported ticks on receiving steel", simulation);
    report("firm-steel-footing", simulation);
    arrive_on_grade(simulation, kReceiverX, -44.5);
    arrive_on_grade(simulation, -45, -44.5);
}

void miss_and_recover(Simulation &simulation, Episode &episode) {
    // Neutral input retains the earned departure momentum and deliberately
    // misses the outboard deck. No reset or staged state supplies the retry.
    require(simulation.set_move_input(0, 0) && simulation.set_sprint_input(false),
            "deliberate unsteered missed attempt", simulation);
    bool landed = false;
    for (unsigned i = 0; i < 180; ++i) {
        const auto s = episode.step(simulation);
        require(s.wall_stride_count == episode.base_count + 2, "miss grants no additional wall start", simulation);
        if (s.player_grounded) {
            require(s.support_entity_id == 1 && std::abs(s.player_position.y - .9) < .05,
                    "miss recovers on real yard grade", simulation);
            landed = true;
            break;
        }
    }
    require(landed && !simulation.snapshot().foot_push_active, "finite miss ends on actual ground", simulation);
    report("miss-ground-recovery", simulation);
    // The workshop face and receiver leave a real ground corridor. Walk
    // north into it before returning west, clear of the south yard stock.
    arrive_on_grade(simulation, simulation.snapshot().player_position.x, -38.6);
    arrive_on_grade(simulation, -46, -38.6);
}
} // namespace

int main() {
    std::cout << std::unitbuf;
    Simulation simulation(InitialSpawn::FreightYardArrival);
    for (unsigned i = 0; i < 90; ++i) tick(simulation);
    require(simulation.snapshot().player_grounded && simulation.snapshot().support_entity_id != 0,
            "shipping FreightYardArrival supplies actual starting support", simulation);
    arrive_on_grade(simulation, -80, -43.5);
    arrive_on_grade(simulation, -48, -43.5);
    auto first = run_to_second_plant(simulation);
    kick_existing_plant(simulation, first);
    receive_and_exit(simulation, first);
    // Return along the ordinary aisle, then deliberately miss and recover.
    arrive_on_grade(simulation, -48, -43.5);
    auto missed = run_to_second_plant(simulation);
    kick_existing_plant(simulation, missed);
    miss_and_recover(simulation, missed);
    auto retry = run_to_second_plant(simulation);
    kick_existing_plant(simulation, retry);
    receive_and_exit(simulation, retry);
    std::cout << "SCRAPERX_WALL_EXIT PASS public_arrival=1 wall_runs=3 same_plant_kicks=3"
                 " steel_receivings=2 firm_ticks=90 onward=1 ground_miss=1 recovery=1 retry=1 resets=0 deaths=0"
              << " tick=" << simulation.snapshot().tick_index << '\n';
}
