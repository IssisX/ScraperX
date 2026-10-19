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
        std::cerr << "FAIL beam ramp route: " << why << std::endl;
        std::exit(1);
    }
}
void tick(Simulation &s, unsigned count = 1) {
    for (unsigned i = 0; i < count; ++i) {
        require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "native tick accepted");
        require(s.snapshot().death_count == 0, "route remains recoverable without death");
    }
}
void observe(Simulation &s, const char *phase, std::uint64_t cargo_entity = 2446) {
    cargo_entity = 2446; // This route observes the broad beam during every shared driver phase.
    const auto truck = s.kit_body_position(s.kit_body_index(2440));
    const auto crate = s.kit_body_position(s.kit_body_index(cargo_entity));
    const auto forks = s.kit_body_position(s.kit_body_index(2443));
    const auto player = s.snapshot().player_position;
    const auto state = s.forklift_state();
    const auto rotation = s.kit_body_rotation(s.kit_body_index(cargo_entity));
    std::cout << phase << " truck=" << truck.x << ',' << truck.y << ',' << truck.z
              << " crate=" << crate.x << ',' << crate.y << ',' << crate.z
              << " forks=" << forks.x << ',' << forks.y << ',' << forks.z
              << " player=" << player.x << ',' << player.y << ',' << player.z
              << " cargo_q=" << rotation.x << ',' << rotation.y << ',' << rotation.z << ',' << rotation.w
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
[[maybe_unused]] const auto staged_boarding_diagnostic = &board; // Shared helper is not executed by this route.


std::vector<Point> shift_reverse(double x, double z, double shift) {
    std::vector<Point> path{{x, z}};
    for (unsigned i = 1; i <= 18; ++i) {
        const double t = 1.5707963267948966 * i / 18;
        path.push_back({x + 4 * (1 - std::cos(t)), z - 4 * std::sin(t)});
    }
    path.push_back({x + 4 + shift, z - 4});
    return path;
}
std::vector<Point> shift_forward(double x, double z, double shift) {
    std::vector<Point> path{{x + 4 + shift, z - 4}};
    for (unsigned i = 1; i <= 18; ++i) {
        const double t = 1.5707963267948966 * i / 18;
        path.push_back({x + 4 + shift - 4 * std::sin(t), z - 4 * std::cos(t)});
    }
    path.push_back({x + shift, z + 2});
    return path;
}
void pickup_beam(Simulation &s) {
    follow(s, shift_reverse(-100, -56, 5), -1);
    auto approach = shift_forward(-100, -56, 5);
    approach.push_back({-95, -46.65});
    follow(s, approach, 1);
    drive_straight_to(s, -46.65, .3);
    observe(s, "BEAM_FORKS_INSERTED");
    require(s.set_forklift_input(0, 0, 1, 0, 1), "raise existing independent beam through ordinary contacts");
    for (unsigned i = 0; i < 600 && s.forklift_state().lift_position_m < .9; ++i) tick(s);
    require(s.forklift_state().lift_position_m >= .9, "finite low transport lift completes");
    require(s.set_forklift_input(0, 0, 0, 0, 1), "hold finite full lift");
    tick(s, 90);
    observe(s, "BEAM_RAISED");
    require(s.kit_body_dynamic(s.kit_body_index(2446)) &&
            std::abs(s.kit_body_mass(s.kit_body_index(2446)) - 600) < .01,
            "existing beam retains independent600kg body");
    require(s.kit_body_position(s.kit_body_index(2446)).y > 1.2,
            "ordinary finite lift clears beam from real dunnage");
}
void place_beam(Simulation &s) {
    const auto truck = s.kit_body_position(s.kit_body_index(2440));
    constexpr double radius = 4;
    const double shift = -93.2 - s.kit_body_position(s.kit_body_index(2446)).x;
    const double angle = std::acos(1 - shift / (2 * radius));
    std::vector<Point> path{{truck.x, truck.z}};
    for (unsigned i = 1; i <= 16; ++i) {
        const double t = angle * i / 16;
        path.push_back({truck.x + radius * (1 - std::cos(t)), truck.z - radius * std::sin(t)});
    }
    for (unsigned i = 1; i <= 16; ++i) {
        const double t = angle * (1 - i / 16.);
        path.push_back({truck.x + radius * (1 - 2 * std::cos(angle) + std::cos(t)),
                        truck.z - radius * (2 * std::sin(angle) - std::sin(t))});
    }
    path.push_back({truck.x + shift, truck.z - 2 * radius * std::sin(angle) - 4});
    follow(s, path, -1);
    const auto backed = s.kit_body_position(s.kit_body_index(2440));
    follow(s, {{backed.x, backed.z}, {truck.x + shift, -49}}, 1);
    require(s.set_forklift_input(0, 0, 1, 0, 1), "raise only after low transport and straight alignment");
    tick(s, 900);
    require(s.set_forklift_input(0, 0, 0, 0, 1), "hold full finite stroke for deposit");
    tick(s, 60);
    require(s.kit_body_position(s.kit_body_index(2446)).y > 2.7,
            "final aligned lift clears actual deck steel");
    drive_straight_to(s, -46.65, .35);
    observe(s, "BEAM_OVER_RECEIVER");
    require(s.set_forklift_input(0, 0, -1, 0, 1), "lower beam onto trestle and grade");
    for (unsigned i = 0; i < 900 && s.forklift_state().lift_position_m > .65; ++i) tick(s);
    require(s.forklift_state().lift_position_m <= .65, "lowering reaches open clearance below ramp");
    require(s.set_forklift_input(0, 0, 0, 0, 1), "hold lowered mast");
    tick(s, 180);
    observe(s, "BEAM_LOWERED");
    drive_straight_to(s, s.kit_body_position(s.kit_body_index(2440)).z - 4, .5);
    tick(s, 180);
    observe(s, "BEAM_FORKS_WITHDRAWN");
    const auto released = s.kit_body_position(s.kit_body_index(2446));
    const auto forks = s.kit_body_position(s.kit_body_index(2443));
    require(forks.z + 1.8 < released.z - .8, "actual fork blades withdraw clear of the broadside beam");
    const auto velocity = s.kit_body_velocity(s.kit_body_index(2446));
    require(std::hypot(velocity.x, velocity.z) < .03 && std::abs(velocity.y) < .03,
            "released independent beam settles without fork support");
}
void walk_to(Simulation &s, double x, double z, double speed = .3) {
    for (unsigned i = 0; i < 1600; ++i) {
        const auto p = s.snapshot().player_position;
        const auto v = s.snapshot().player_linear_velocity;
        if (s.snapshot().player_grounded && std::hypot(x - p.x, z - p.z) < .08 &&
            std::hypot(v.x, v.z) < .15) {
            require(s.set_move_input(0, 0), "stop ordinary route walking");
            tick(s, 30);
            return;
        }
        require(s.set_move_input(std::clamp(x - p.x - .45 * v.x, -speed, speed),
                                 std::clamp(z - p.z - .45 * v.z, -speed, speed)),
                "ordinary route walking input");
        tick(s);
        if (i % 400 == 0) observe(s, "PLAYER_WALK");
    }
    observe(s, "PLAYER_WALK_FAILED");
    require(false, "walking reaches physical route waypoint");
}
void cross_beam(Simulation &s) {
    const auto beam = s.kit_body_index(2446);
    const auto p = s.kit_body_position(beam);
    const auto q = s.kit_body_rotation(beam);
    const double ux = 1 - 2 * (q.y * q.y + q.z * q.z);
    const double uy = 2 * (q.x * q.y + q.w * q.z);
    const double uz = 2 * (q.x * q.z - q.w * q.y);
    require(ux > .8 && uy > .2, "withdrawn beam is a physical rising ramp toward receiving trestle");
    const double low_x = p.x - 3 * ux, low_z = p.z - 3 * uz;
    walk_to(s, s.snapshot().player_position.x - 2, s.snapshot().player_position.z);
    require(!s.forklift_state().controls_reachable, "ordinary departure leaves operator platform");
    walk_to(s, low_x - 1.4, s.snapshot().player_position.z);
    walk_to(s, low_x - 1.4, low_z - 1.25);
    walk_to(s, low_x + .65 * ux, low_z - 1.25);
    require(s.set_facing(0, 1), "face low flange from south, clear of original timber dunnage");
    require(s.request_jump(), "jump onto actual loose beam low end");
    bool beam_footing = false;
    for (unsigned i = 0; i < 300; ++i) {
        const auto a = s.snapshot().player_position;
        const auto v = s.snapshot().player_linear_velocity;
        require(s.set_move_input(std::clamp(low_x + .65 * ux - a.x - .45 * v.x, -.3, .3),
                                 std::clamp(low_z + .65 * uz - a.z - .45 * v.z, -.3, .3)),
                "land by ordinary movement on free beam");
        tick(s);
        if (s.snapshot().player_grounded && s.snapshot().support_entity_id == 2446) {
            beam_footing = true;
            break;
        }
    }
    observe(s, "PLAYER_BEAM_FOOTING");
    require(beam_footing, "actual beam contact supports player after public jump");
    walk_to(s, p.x + 1.8 * ux, p.z + 1.8 * uz, .22);
    require(s.snapshot().support_entity_id == 2446, "ordinary uphill walking retains actual loose beam support");
    walk_to(s, -89.5, -44, .22);
    for (unsigned i = 0; i < 90; ++i) {
        tick(s);
        require(s.snapshot().player_grounded && s.snapshot().support_entity_id == 1491,
                "real receiving trestle supports player after beam transfer");
    }
    observe(s, "PLAYER_RECEIVING_FOOTING");
    walk_to(s, -88.7, -44, .25);
    require(s.snapshot().support_entity_id == 1491, "receiving deck permits onward ordinary walking");
    walk_to(s, -88.7, -46, .25);
    require(s.snapshot().player_grounded && s.snapshot().player_position.y < 1,
            "ordinary trestle exit returns safely to actual grade");
    walk_to(s, low_x + .65 * ux, -46, .3);
    walk_to(s, low_x + .65 * ux, low_z - 1.25, .3);
    require(s.snapshot().player_grounded && s.snapshot().player_position.y < 1,
            "same life ground recovery returns to the ramp approach without reset");
    observe(s, "GRADE_RECOVERY");
}

void board_from_arrival(Simulation &s) {
    tick(s, 45);
    require(s.snapshot().player_grounded && !s.forklift_state().controls_reachable,
            "normal arrival starts outside the real cab");
    const auto poses = s.snapshot().step_up_pose_count;
    walk_to(s, -98, -57.65, .55);
    require(s.set_facing(-1, 0), "face real east cab entrance");
    for (unsigned i = 0; i < 540; ++i) {
        const auto target = s.kit_body_position(s.kit_body_index(2440));
        const auto p = s.snapshot().player_position;
        const auto v = s.snapshot().player_linear_velocity;
        require(s.set_move_input(std::clamp(1.2 * (target.x - p.x) - .45 * v.x, -.3, .3),
                                 std::clamp(1.2 * (target.z - 1.65 - p.z) - .45 * v.z, -.15, .15)),
                "ordinary walking boards real forklift from arrival");
        tick(s);
        if (s.forklift_state().controls_reachable) {
            require(s.set_move_input(0, 0), "stand on actual operator platform");
            tick(s, 90);
            require(s.snapshot().support_entity_id == 2440 && s.forklift_state().controls_reachable &&
                    s.snapshot().step_up_pose_count == poses,
                    "arrival boarding retains real finite step footing");
            observe(s, "NORMAL_ARRIVAL_BOARDED");
            return;
        }
    }
    require(false, "normal arrival reaches actual forklift controls");
}
} // namespace
int main() {
    Simulation s(InitialSpawn::FreightYardArrival);
    board_from_arrival(s);
    pickup_beam(s);
    place_beam(s);
    cross_beam(s);
    std::cout << "SCRAPERX_BEAM_RAMP PASS normal arrival, ordinary600kg pickup, low transport, settled ramp, fork withdrawal, actual beam/trestle footing, onward walk and grade recovery\n";
}
