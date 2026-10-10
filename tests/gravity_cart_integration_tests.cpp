#include "sim/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {
using scraperx::sim::Simulation;
constexpr std::uint64_t kTower = 11, kLowerReceiver = 1955;
constexpr std::uint64_t kIntermediateReceiver = 1956, kUpperReceiver = 1957;
constexpr std::uint64_t kCart = 2320, kSlab = 2325;
constexpr unsigned kCartMachine = 7;

void report(const char *tag, const Simulation &s) {
    const auto v = s.snapshot();
    std::cout << tag << " tick=" << v.tick_index << " pos="
              << v.player_position.x << ',' << v.player_position.y << ','
              << v.player_position.z << " support=" << v.support_entity_id
              << " grounded=" << v.player_grounded
              << " station=" << int(v.supplied_machine_station)
              << " deck=" << v.supplied_machine_surface_y
              << " energy=" << v.supplied_machine_energy_j
              << " work=" << v.supplied_machine_positive_work_j
              << " power=" << v.supplied_machine_power_w
              << " force=" << v.supplied_machine_force_n
              << " deaths=" << v.death_count << '\n';
}

void require(bool ok, const char *why, const Simulation &s) {
    if (ok) return;
    report(why, s);
    std::cerr << "FAIL GRAVITY_CART " << why << '\n';
    std::exit(1);
}

void tick(Simulation &s, int count = 1) {
    while (count-- > 0) {
        require(s.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "fixed native tick accepted", s);
        const auto v = s.snapshot();
        require(v.death_count == 0, "route completes without a death or rescue", s);
        if (v.supplied_machine_index == kCartMachine) {
            require(std::isfinite(v.supplied_machine_energy_j) &&
                    std::isfinite(v.supplied_machine_power_w) &&
                    std::isfinite(v.supplied_machine_positive_work_j) &&
                    std::isfinite(v.supplied_machine_force_n) &&
                    v.supplied_machine_energy_j >= 0 &&
                    v.supplied_machine_energy_j <= 200000.000001 &&
                    v.supplied_machine_energy_overdraft_j == 0 &&
                    v.supplied_machine_power_w <= 10000.000001 &&
                    std::abs(v.supplied_machine_force_n) <= 9000.000001,
                    "cart uses finite 200kJ bank, 10kW power and 9kN actuator", s);
        }
    }
}

void supported(const Simulation &s, std::uint64_t entity, const char *why) {
    const auto v = s.snapshot();
    require(v.player_grounded && v.support_entity_id == entity, why, s);
}

void station(const Simulation &s, unsigned expected, const char *why) {
    const auto v = s.snapshot();
    require(v.supplied_machine_index == kCartMachine &&
            v.supplied_machine_station == expected, why, s);
}

void steer(Simulation &s, double x, double z) {
    const auto v = s.snapshot();
    const double dx = x - v.player_position.x, dz = z - v.player_position.z;
    (void)s.set_move_input(
        std::clamp(dx * 1.8 - v.player_linear_velocity.x * .28, -1., 1.),
        std::clamp(dz * 1.8 - v.player_linear_velocity.z * .28, -1., 1.));
    if (std::hypot(dx, dz) > .02) (void)s.set_facing(dx, dz);
}

void walk(Simulation &s, double x, double z, int budget = 1800,
          bool tower_grounded_only = false) {
    bool reached = false;
    for (int i = 0; i < budget; ++i) {
        const auto v = s.snapshot();
        if (tower_grounded_only)
            supported(s, kTower, "inland tray detour retains actual Tower footing");
        if (v.player_grounded &&
            std::hypot(x - v.player_position.x, z - v.player_position.z) < .06 &&
            std::hypot(v.player_linear_velocity.x, v.player_linear_velocity.z) < .1) {
            reached = true;
            break;
        }
        steer(s, x, z);
        tick(s);
        if (tower_grounded_only)
            supported(s, kTower, "inland tray detour retains actual Tower footing");
    }
    (void)s.set_move_input(0, 0);
    tick(s, 30);
    if (tower_grounded_only)
        supported(s, kTower, "inland tray detour settles on actual Tower footing");
    require(reached, "ordinary walk reaches supported target", s);
}

void jump_to(Simulation &s, double x, double z, std::uint64_t receiver) {
    require(s.snapshot().player_grounded, "jump departs actual support", s);
    steer(s, x, z);
    require(s.request_jump(), "ordinary jump accepted", s);
    tick(s);
    bool landed = false;
    for (int i = 0; i < 900; ++i) {
        steer(s, x, z);
        tick(s);
        const auto v = s.snapshot();
        if (v.player_grounded && v.support_entity_id == receiver &&
            std::hypot(x - v.player_position.x, z - v.player_position.z) < .18) {
            landed = true;
            break;
        }
    }
    (void)s.set_move_input(0, 0);
    tick(s, 60);
    require(landed, "jump reaches the intended native receiver", s);
    supported(s, receiver, "jump settles on actual receiver contact");
}

void move_cart_to(Simulation &s, double input, double threshold,
                  int budget_seconds) {
    require(s.set_supplied_machine_input(input), "ordinary cart command accepted", s);
    bool reached = false;
    for (int i = 0; i < budget_seconds * 90; ++i) {
        tick(s);
        const double floor = s.snapshot().supplied_machine_surface_y;
        if (input > 0 ? floor >= threshold : floor <= threshold) {
            reached = true;
            break;
        }
    }
    (void)s.set_supplied_machine_input(0);
    tick(s, 90);
    require(reached, "cart reaches requested height within finite travel time", s);
}

void upper_cart_support(const Simulation &s) {
    supported(s, kCart, "loaded rider retains real upper cart footing");
    require(std::abs(s.snapshot().supplied_machine_surface_y - 352) < .04,
            "actual upper deck settles at 352m", s);
}
} // namespace

int main() {
    // This is the only staging operation. Every later transfer uses normal
    // Simulation movement/jump/cart inputs in its complete native world.
    Simulation s;
    require(s.debug_restart_at({-24.7, 330.9, -158.25}),
            "initial supported 330m staging", s);
    tick(s, 90);
    supported(s, kTower, "Tower330 entry has real footing");

    // The wheel exit's raised service tray blocks the old straight approach.
    // Walk around its visible inland end on the existing Tower ring.
    walk(s, -22.4, -158.25, 1800, true);
    walk(s, -22.4, -156.9, 1800, true);
    walk(s, -24.7, -156.9, 1800, true);
    walk(s, -24.7, -140.15);
    walk(s, -25.05, -140.15);
    station(s, 2, "ordinary approach reaches lower control");
    walk(s, -41.8, -140.25);
    supported(s, kLowerReceiver, "loading tongue supplies actual departure footing");
    jump_to(s, -41.8, -141.6, kCart);
    station(s, 1, "ordinary boarding reaches cart control");
    report("boarded330", s);

    const double gravity_bank = s.snapshot().supplied_machine_energy_j;
    const double gravity_work = s.snapshot().supplied_machine_positive_work_j;
    move_cart_to(s, 1, 340.985, 80);
    require(std::abs(s.snapshot().supplied_machine_surface_y - 341) < .10,
            "neutral input brakes at intermediate341", s);
    jump_to(s, -35.45, -140.25, kIntermediateReceiver);
    // Stand wholly on receiver1956, clear of its coincident Tower floor seam.
    walk(s, -26.5, -140.15);
    station(s, 4, "341 receiver has reachable intermediate control");
    walk(s, -35.45, -140.25);
    jump_to(s, -35.45, -141.6, kCart);
    report("intermediate_exit_reboard", s);

    move_cart_to(s, 1, 351.985, 110);
    upper_cart_support(s);
    require(s.snapshot().supplied_machine_energy_j == gravity_bank &&
            s.snapshot().supplied_machine_positive_work_j == gravity_work,
            "gravity ascent spends no actuator source work", s);
    report("upper_seat", s);

    // Walk into the upper gap without a jump. Recovery must come from the
    // real 341m apron, followed by paid recall and ordinary reboarding.
    (void)s.set_move_input(1, 0);
    (void)s.set_facing(1, 0);
    bool left = false, caught = false;
    for (int i = 0; i < 900; ++i) {
        tick(s);
        const auto v = s.snapshot();
        if (!v.player_grounded) left = true;
        if (left) {
            steer(s, -28.0, -141.6);
            if (v.player_grounded && v.support_entity_id == kIntermediateReceiver) {
                caught = true;
                break;
            }
        }
    }
    (void)s.set_move_input(0, 0);
    tick(s, 90);
    require(left && caught, "deliberate walking miss falls onto actual341 apron", s);
    supported(s, kIntermediateReceiver, "miss recovery has real341 footing");
    require(std::abs(s.snapshot().player_position.y - 341.9) < .15,
            "recovery apron is at the actual intermediate level", s);
    report("walking_miss_recovery", s);

    walk(s, -28.0, -140.15);
    walk(s, -26.5, -140.15);
    station(s, 4, "recovered player reaches fixed recall control");
    const double recall_bank = s.snapshot().supplied_machine_energy_j;
    const double recall_work = s.snapshot().supplied_machine_positive_work_j;
    move_cart_to(s, -1, 341.015, 100);
    require(std::abs(s.snapshot().supplied_machine_surface_y - 341) < .10,
            "paid recall brakes cart at341 for reboarding", s);
    require(s.snapshot().supplied_machine_energy_j < recall_bank - 100 &&
            s.snapshot().supplied_machine_positive_work_j > recall_work + 100,
            "intermediate recall performs actual finite paid work", s);
    supported(s, kIntermediateReceiver, "recall preserves ashore footing");
    walk(s, -35.45, -140.25);
    jump_to(s, -35.45, -141.6, kCart);
    const double retry_bank = s.snapshot().supplied_machine_energy_j;
    const double retry_work = s.snapshot().supplied_machine_positive_work_j;
    move_cart_to(s, 1, 351.985, 80);
    upper_cart_support(s);
    require(s.snapshot().supplied_machine_energy_j == retry_bank &&
            s.snapshot().supplied_machine_positive_work_j == retry_work,
            "retry ascent also uses gravity rather than powered UP", s);
    report("paid_recall_reboard_retry352", s);

    walk(s, -28.10, -141.6, 600);
    jump_to(s, -25.35, -141.6, kTower);
    require(std::abs(s.snapshot().player_position.y - 352.9) < .15,
            "ordinary onward jump reaches actual Tower352 footing", s);
    walk(s, -25.05, -140.15);
    station(s, 3, "onward receiver exposes upper recall control");
    supported(s, kUpperReceiver, "upper control stands on attached receiver body");
    report("onward_upper_control", s);

    const double return_bank = s.snapshot().supplied_machine_energy_j;
    const double return_work = s.snapshot().supplied_machine_positive_work_j;
    const double slab_y = s.kit_body_position(s.kit_body_index(kSlab)).y;
    move_cart_to(s, -1, 330.05, 160);
    const auto end = s.snapshot();
    require(std::abs(end.supplied_machine_surface_y - 330) < .08,
            "full paid reset returns actual cart to330", s);
    require(end.supplied_machine_energy_j < return_bank - 100 &&
            end.supplied_machine_positive_work_j > return_work + 100,
            "full reset draws finite source work", s);
    require(s.kit_body_position(s.kit_body_index(kSlab)).y > slab_y + 20,
            "reset actually hoists the counterweight slab", s);
    require(end.player_grounded &&
            (end.support_entity_id == kTower || end.support_entity_id == kUpperReceiver),
            "full reset preserves ordinary upper ashore footing", s);
    report("returned330", s);
    std::cout << "PASS GRAVITY_CART ordinary approach/board/341exit-reboard/"
                 "gravity352/walking-miss341-recovery/paid-recall/retry/"
                 "onward-jump/full-paid-reset initial_staging_only=1\n";
}
