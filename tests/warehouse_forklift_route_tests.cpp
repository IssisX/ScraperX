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
void board(Simulation &s) {
    // Only the initial grade approach is staged. Boarding/operation below use
    // public intent, ordinary foot contact and actual vehicle motion.
    require(s.debug_restart_at({-102, .9, -57.65}), "initial grade approach");
    tick(s, 45);
    require(s.snapshot().player_grounded && !s.forklift_state().controls_reachable,
            "approach begins outside truck on actual grade");
    require(s.set_facing(1, 0), "face actual rear platform from its open side");
    require(s.request_jump(), "ordinary foot-powered boarding jump");
    for (unsigned i = 0; i < 270; ++i) {
        const auto target = s.kit_body_position(s.kit_body_index(2440));
        const auto player = s.snapshot().player_position;
        const auto velocity = s.snapshot().player_linear_velocity;
        const double x = std::clamp(1.2 * (target.x - player.x) - .45 * velocity.x, -.42, .42);
        const double z = std::clamp(1.2 * (target.z - 1.65 - player.z) - .45 * velocity.z, -.3, .3);
        require(s.set_move_input(x, z), "boarding direction accepted");
        tick(s);
        if (s.forklift_state().controls_reachable) {
            require(s.set_move_input(0, 0), "stand on platform");
            tick(s, 30);
            observe(s, "BOARDED");
            require(s.forklift_state().controls_reachable, "real platform footing retained");
            return;
        }
    }
    observe(s, "BOARDING_FAILED");
    require(false, "actual jump and landing reaches rear operator platform");
}
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
void drive_straight_to(Simulation &s, double z, double speed_limit = 1.5,
                       std::uint64_t carried_entity = 0, bool align_over_receiver = false) {
    const auto chassis = s.kit_body_index(2440);
    for (unsigned i = 0; i < 6000; ++i) {
        require(s.forklift_state().controls_reachable, "operator keeps actual platform contact during transport");
        if (carried_entity) {
            const auto load = s.kit_body_position(s.kit_body_index(carried_entity));
            const auto forks = s.kit_body_position(s.kit_body_index(2443));
            require(load.y > .4 && load.z > forks.z && load.z < forks.z + 1.8,
                    "transport retains actual load inside the fork support polygon");
        }
        const auto p = s.kit_body_position(chassis);
        const auto v = s.kit_body_velocity(chassis);
        const double error = z - p.z;
        if (std::abs(error) < .045 && std::abs(v.z) < .06) {
            require(s.set_forklift_input(0, 0, 0, 0, 1), "finite brake at placement");
            tick(s, 30);
            return;
        }
        const double direction = error >= 0 ? 1 : -1;
        const double speed = std::min(.7 * std::abs(error), speed_limit);
        const double relative_speed = v.z * direction;
        const double drive = direction * std::clamp(.08 + .7 * (speed - relative_speed), 0., .55);
        const double brake = relative_speed > speed + .08 || std::abs(error) < .025 ? 1 : 0;
        double steer = 0;
        if (align_over_receiver) {
            require(carried_entity && direction > 0, "placement alignment uses a forward carried load");
            const auto load = s.kit_body_position(s.kit_body_index(carried_entity));
            const auto receiver = s.kit_body_position(s.kit_body_index(2444));
            const auto q = s.kit_body_rotation(chassis);
            const double fx = 2 * (q.x * q.z + q.w * q.y);
            const double fz = 1 - 2 * (q.x * q.x + q.y * q.y);
            require(fz > .9 && load.z - p.z > 2, "placement approaches with the actual load ahead");
            // Aim the carried crate, not just chassis Z: a small residual
            // heading after the turn otherwise drifts the load sideways.
            // Rear-steer kinematics at this forward point give
            // dx_load/dz ~= heading - reach * tan(wheel_angle) / wheelbase.
            // This driver still sends only finite analog steering commands.
            steer = std::clamp(std::atan(1.65 * (fx / fz + .8 * (load.x - receiver.x)) /
                                         (load.z - p.z)) / .9, -.2, .2);
        }
        require(s.set_forklift_input(brake ? 0 : drive, steer, 0, 0, brake), "finite transport command");
        tick(s);
        if (i % 900 == 0) observe(s, "TRANSPORT");
    }
    observe(s, "TRANSPORT_FAILED");
    require(false, "real drive/brakes reaches placement lane");
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
struct Point { double x, z; };
Point forward(Simulation &s) {
    const auto q = s.kit_body_rotation(s.kit_body_index(2440));
    const double x = 2 * (q.x * q.z + q.w * q.y);
    const double z = 1 - 2 * (q.x * q.x + q.y * q.y);
    const double length = std::hypot(x, z);
    require(length > .8, "truck stays physically upright during route operation");
    return {x / length, z / length};
}
void follow(Simulation &s, const std::vector<Point> &path, double direction,
            std::uint64_t carried_entity = 0) {
    // A test driver chooses only the same analog controls as the player.
    // Pure-pursuit geometry never writes a transform, velocity or cargo state.
    require(path.size() >= 2, "maneuver has real path extent");
    const auto chassis = s.kit_body_index(2440);
    unsigned segment = 0;
    double closest_progress = 0;
    for (unsigned i = 0; i < 9000; ++i) {
        require(s.forklift_state().controls_reachable, "operator retains actual contact in steering maneuver");
        const auto p = s.kit_body_position(chassis);
        const auto v = s.kit_body_velocity(chassis);
        const auto f = forward(s);
        const double end_distance = std::hypot(path.back().x - p.x, path.back().z - p.z);
        if (segment + 2 == path.size() && end_distance < .4) {
            require(s.set_forklift_input(0, 0, 0, 0, 1), "stop after actual maneuver");
            tick(s, 90);
            observe(s, "MANEUVER_FINISHED", carried_entity ? carried_entity : 2444);
            return;
        }
        const auto a = path[segment], b = path[segment + 1];
        const double dx = b.x - a.x, dz = b.z - a.z;
        const double length = std::hypot(dx, dz);
        const double projection = std::clamp(((p.x - a.x) * dx + (p.z - a.z) * dz) /
                                             (length * length), 0., 1.);
        closest_progress = std::max(closest_progress, projection);
        if ((projection > .96 || std::hypot(b.x - p.x, b.z - p.z) < .5) && segment + 2 < path.size()) {
            ++segment;
            closest_progress = 0;
            continue;
        }
        Point target{a.x + closest_progress * dx, a.z + closest_progress * dz};
        double lookahead = 1.8;
        unsigned look_segment = segment;
        while (lookahead > 0 && look_segment + 1 < path.size()) {
            const auto end = path[look_segment + 1];
            const double distance = std::hypot(end.x - target.x, end.z - target.z);
            if (distance >= lookahead) {
                target = {target.x + (end.x - target.x) * lookahead / distance,
                          target.z + (end.z - target.z) * lookahead / distance};
                break;
            }
            lookahead -= distance;
            target = end;
            ++look_segment;
        }
        const double tx = target.x - p.x, tz = target.z - p.z;
        const double lateral = f.z * tx - f.x * tz;
        // Rear steer: positive demand rotates the forward axis toward -X.
        const double steer = std::clamp(-std::atan2(2 * 1.65 * lateral, tx * tx + tz * tz) / .9, -1., 1.);
        // Crawl through turns; use the already accepted1.5m/s transport speed
        // on long aligned straights instead of spending most of the replay
        // slowly traversing empty aisle. These remain finite driver inputs.
        const double speed_limit = length > 10 && std::abs(steer) < .08 && end_distance > 5 ? 1.5 : .8;
        const double speed = std::min(speed_limit, .7 * end_distance);
        const double current_speed = direction * (v.x * f.x + v.z * f.z);
        const double brake = current_speed > speed + .08 ? 1 : 0;
        const double drive = direction * std::clamp(.08 + .7 * (speed - current_speed), 0., .4);
        require(s.set_forklift_input(brake ? 0 : drive, steer, 0, 0, brake), "finite rear-steer maneuver input");
        tick(s);
        if (carried_entity) {
            const auto load = s.kit_body_position(s.kit_body_index(carried_entity));
            const auto forks = s.kit_body_position(s.kit_body_index(2443));
            const double lx = load.x - forks.x, lz = load.z - forks.z;
            const double along = lx * f.x + lz * f.z;
            const double across = lx * f.z - lz * f.x;
            require(load.y > .4 && along > .2 && along < 1.8 && std::abs(across) < .25,
                    "steering carries independent load inside real fork polygon");
        }
        if (i % 900 == 0) {
            observe(s, "MANEUVER", carried_entity ? carried_entity : 2444);
            std::cout << "PATH segment=" << segment << " forward=" << f.x << ',' << f.z
                      << " steer=" << steer << " target=" << target.x << ',' << target.z << std::endl;
        }
    }
    observe(s, "MANEUVER_FAILED", carried_entity ? carried_entity : 2444);
    require(false, "real steering reaches cargo aisle within finite drive path");
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
