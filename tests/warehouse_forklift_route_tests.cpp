#include "sim/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace scraperx::sim;
namespace {
void require(bool value, const char *why) {
    if (!value) {
        std::cerr << "FAIL forklift cargo route: " << why << std::endl;
        std::exit(1);
    }
}
void tick(Simulation &s, unsigned count = 1) {
    for (unsigned i = 0; i < count; ++i) {
        require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "native tick accepted");
        require(s.snapshot().death_count == 0, "route remains recoverable without death");
    }
}
void observe(Simulation &s, const char *phase, std::uint64_t cargo_entity = 2444) {
    const auto truck = s.kit_body_position(s.kit_body_index(2440));
    const auto crate = s.kit_body_position(s.kit_body_index(cargo_entity));
    const auto forks = s.kit_body_position(s.kit_body_index(2443));
    const auto player = s.snapshot().player_position;
    const auto state = s.forklift_state();
    std::cout << phase << " truck=" << truck.x << ',' << truck.y << ',' << truck.z
              << " crate=" << crate.x << ',' << crate.y << ',' << crate.z
              << " forks=" << forks.x << ',' << forks.y << ',' << forks.z
              << " player=" << player.x << ',' << player.y << ',' << player.z
              << " support=" << s.snapshot().support_entity_id
              << " reachable=" << state.controls_reachable
              << " lift=" << state.lift_position_m << " tilt=" << state.tilt_angle_rad
              << " force=" << state.lift_force_n
              << " traversal=" << unsigned(s.snapshot().traversal_state)
              << " phase=" << unsigned(s.snapshot().traversal_phase)
              << " rejection=" << unsigned(s.snapshot().traversal_rejection_reason)
              << " hand=" << s.snapshot().traversal_left_hand.x << ','
              << s.snapshot().traversal_left_hand.y << ',' << s.snapshot().traversal_left_hand.z
              << " target=" << s.snapshot().traversal_target_point.x << ','
              << s.snapshot().traversal_target_point.y << ',' << s.snapshot().traversal_target_point.z
              << std::endl;
}
#include "forklift_public_driver.hpp"

void pickup(Simulation &s, std::uint64_t cargo_entity = 2444) {
    const auto cargo = s.kit_body_index(cargo_entity);
    const auto chassis = s.kit_body_index(2440);
    const auto initial = s.kit_body_position(cargo);
    const double target = initial.z - 3.0;
    bool arrived = false;
    for (unsigned i = 0; i < 1800; ++i) {
        require(s.forklift_state().controls_reachable, "operator stays on moving rear floor");
        const auto truck = s.kit_body_position(chassis);
        const auto velocity = s.kit_body_velocity(chassis);
        const double error = target - truck.z;
        if (std::abs(error) < .045 && std::abs(velocity.z) < .06) {
            arrived = true;
            break;
        }
        const double speed = std::clamp(.7 * error, 0., .65);
        const double drive = std::clamp(.08 + .7 * (speed - velocity.z), 0., .4);
        const double brake = velocity.z > speed + .08 || error < 0 ? 1 : 0;
        require(s.set_forklift_input(brake ? 0 : drive, 0, 0, 0, brake), "finite approach drive");
        tick(s);
        if (i % 300 == 0) observe(s, "PICKUP_APPROACH", cargo_entity);
    }
    observe(s, "FORKS_INSERTED", cargo_entity);
    require(arrived, "truck reaches actual open fork tunnels under cargo");
    require(s.set_forklift_input(0, 0, 1, 0, 1), "lift cargo through actual fork contact");
    tick(s, 225);
    observe(s, "CARGO_RAISED", cargo_entity);
    const auto raised = s.kit_body_position(cargo);
    require(raised.y > initial.y + .45, "independent600kg crate rises on actual forks");
    require(std::abs(raised.x - initial.x) < .25 && std::abs(raised.z - initial.z) < .3,
            "pickup preserves useful alignment without a cargo latch");
    require(s.set_forklift_input(0, 0, 0, .5, 1), "finite backward mast tilt");
    tick(s, 90);
    require(s.set_forklift_input(0, 0, 0, 0, 1), "hold raised load with existing finite brakes");
    tick(s, 90);
    observe(s, "PICKUP_HELD", cargo_entity);
    require(s.kit_body_position(cargo).y > initial.y + .4, "real raised cargo remains on stopped forks");
}
void place_first_crate(Simulation &s) {
    // Reverse with the load in view, then level, lower and withdraw. The cargo
    // has no joint, parenting or placement flag at any phase.
    drive_straight_to(s, -90.0, 1.5, 2444);
    for (unsigned i = 0; i < 180; ++i) {
        const double angle = s.forklift_state().tilt_angle_rad;
        require(s.set_forklift_input(0, 0, 0, std::abs(angle) > .002 ? -.4 : 0, 1),
                "level actual mast before setting cargo down");
        tick(s);
        if (std::abs(angle) < .002) break;
    }
    require(s.set_forklift_input(0, 0, -1, 0, 1), "lower actual cargo onto grade");
    tick(s, 300);
    require(s.set_forklift_input(0, 0, 0, 0, 1), "stop lowering at real support");
    tick(s, 60);
    observe(s, "FIRST_CRATE_PLACED");
    const auto crate = s.kit_body_index(2444);
    const auto placed = s.kit_body_position(crate);
    require(std::abs(placed.x + 100) < .3 && std::abs(placed.z + 87) < .4 && std::abs(placed.y) < .04,
            "600kg cargo is placed in the actual planned upward-route footprint");
    drive_straight_to(s, -93.2, .65);
    tick(s, 90);
    observe(s, "FORKS_WITHDRAWN");
    const auto released = s.kit_body_position(crate);
    const auto forks = s.kit_body_position(s.kit_body_index(2443));
    require(forks.z + 1.8 < released.z - 1.2 - .2, "actual fork solids are clear of cargo");
    require(std::abs(released.y - placed.y) < .025 && std::abs(released.z - placed.z) < .08 &&
            std::abs(s.kit_body_velocity(crate).y) < .03,
            "independent dynamic cargo remains supported after real withdrawal");
}
std::vector<Point> reverse_shift(double x, double z, double side) {
    std::vector<Point> path{{x, z}};
    constexpr double radius = 4;
    for (unsigned i = 1; i <= 12; ++i) {
        const double t = 1.5707963267948966 * i / 12;
        path.push_back({x + side * radius * (1 - std::cos(t)), z - radius * std::sin(t)});
    }
    path.push_back({x + side * 9, z - radius});
    return path;
}
std::vector<Point> forward_shift(double x, double z, double side) {
    std::vector<Point> path{{x + side * 9, z - 4}};
    for (unsigned i = 1; i <= 12; ++i) {
        const double t = 1.5707963267948966 * i / 12;
        path.push_back({x + side * 9 - side * 4 * std::sin(t), z - 4 * std::cos(t)});
    }
    return path;
}
void stack_second_crate(Simulation &s) {
    drive_straight_to(s, -103, .8);
    follow(s, reverse_shift(-100, -103, -1), -1);
    auto to_crate = forward_shift(-100, -103, -1);
    to_crate.push_back({-105, -56});
    follow(s, to_crate, 1);
    pickup(s, 2445);
    drive_straight_to(s, -103, 1.0, 2445);
    follow(s, reverse_shift(-105, -103, 1), -1, 2445);
    auto to_stack = forward_shift(-105, -103, 1);
    to_stack.push_back({-100, -93});
    follow(s, to_stack, 1, 2445);
    require(s.set_forklift_input(0, 0, 1, 0, 1), "raise real second crate above lower crate");
    while (s.kit_body_position(s.kit_body_index(2445)).y < 1.65) {
        tick(s);
        require(s.forklift_state().lift_position_m < 2.3, "rated mast stroke can clear lower crate");
    }
    require(s.set_forklift_input(0, 0, 0, 0, 1), "hold clearance height");
    tick(s, 30);
    for (unsigned i = 0; i < 180; ++i) {
        const double angle = s.forklift_state().tilt_angle_rad;
        if (std::abs(angle) < .002) break;
        require(s.set_forklift_input(0, 0, 0, angle > 0 ? -.4 : .4, 1), "level second load");
        tick(s);
    }
    require(s.set_forklift_input(0, 0, 0, 0, 1), "stop mast rotation");
    drive_straight_to(s, -90.95, .35, 2445, true);
    require(s.set_forklift_input(0, 0, -1, 0, 1), "lower second crate onto actual first crate");
    unsigned unloaded_ticks = 0;
    const double lower_roof = s.kit_body_position(s.kit_body_index(2444)).y + 1.5;
    for (unsigned i = 0; i < 180; ++i) {
        tick(s);
        const auto load = s.kit_body_position(s.kit_body_index(2445));
        const auto forks = s.kit_body_position(s.kit_body_index(2443));
        require(forks.y - .05 > lower_roof + .025,
                "set-down never drives fork steel into the lower crate roof");
        // Height identifies the expected encounter, not transfer completion.
        // Continue lowering until solved hydraulic force no longer carries
        // the600kg load; a near-touch while still on the forks is insufficient.
        const bool unloaded = std::abs(load.y - lower_roof) < .025 &&
            std::abs(s.forklift_state().lift_force_n) < 1800 &&
            std::abs(s.kit_body_velocity(s.kit_body_index(2445)).y) < .03;
        unloaded_ticks = unloaded ? unloaded_ticks + 1 : 0;
        if (unloaded_ticks >= 8) break;
    }
    require(unloaded_ticks >= 8, "actual sustained load transfer precedes fork withdrawal");
    require(s.set_forklift_input(0, 0, 0, 0, 1), "stop on actual stack contact");
    tick(s, 90);
    observe(s, "SECOND_CRATE_STACKED", 2445);
    const auto upper = s.kit_body_position(s.kit_body_index(2445));
    require(std::abs(upper.x + 100) < .2 && std::abs(upper.z + 87.9) < .2 &&
            std::abs(upper.y - 1.5) < .04, "offset stack provides real stepped footing");
    drive_straight_to(s, -94, .4);
    tick(s, 180);
    observe(s, "STACK_FORKS_WITHDRAWN", 2445);
    const auto released = s.kit_body_position(s.kit_body_index(2445));
    const auto forks = s.kit_body_position(s.kit_body_index(2443));
    require(forks.z + 1.8 < released.z - 1.2 - .2 && std::abs(released.y - upper.y) < .025,
            "upper crate remains physically supported after fork withdrawal");
}
} // namespace

#include "forklift_route_walk.hpp"

int main() {
    Simulation s;
    board(s);
    pickup(s);
    place_first_crate(s);
    stack_second_crate(s);
    ascend_stack(s);
    std::cout << "PASS public cargo-built upward route: boarding, two600kg loads, placement, withdrawal, actual player gallery footing, onward walk and grade retry\n";
}
