#include "sim/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace scraperx::sim;

namespace {
struct Landing {
    const char *name;
    double x, z, top, half_x, half_z;
};
constexpr Landing perches[] = {
    {"perch1", -69.8, -67.5, 3.0, .7, .9},
    {"perch2", -74.2, -78.5, 3.2, .7, .9},
    {"perch3", -69.8, -89.5, 3.0, .7, .9},
};

void require(bool ok, const char *operation, const char *stage, const Simulation &simulation) {
    if (ok) return;
    const auto s = simulation.snapshot();
    std::cerr << "FAIL UPPER_GALLERY stage=" << stage << " operation=" << operation
              << " tick=" << s.tick_index
              << " p=" << s.player_position.x << ',' << s.player_position.y << ',' << s.player_position.z
              << " v=" << s.player_linear_velocity.x << ',' << s.player_linear_velocity.y << ',' << s.player_linear_velocity.z
              << " grounded=" << s.player_grounded << " support=" << s.support_entity_id
              << " contact=" << s.support_contact_point.x << ',' << s.support_contact_point.y << ',' << s.support_contact_point.z
              << " deaths=" << s.death_count << '\n';
    std::exit(1);
}

void tick(Simulation &simulation, const char *stage, unsigned count = 1) {
    for (unsigned i = 0; i < count; ++i) {
        require(simulation.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "fixed tick accepted", stage, simulation);
        require(simulation.snapshot().death_count == 0, "same player remains alive", stage, simulation);
    }
}

bool on(const Snapshot &s, const Landing &landing) {
    return s.player_grounded && !s.player_crouched &&
        s.support_entity_id == (landing.top == 0 ? Simulation::kStaticDeckEntityId :
                                 Simulation::kWorldSolidEntityId) &&
        std::abs(s.player_position.y - landing.top - .9) < .13 &&
        std::abs(s.support_contact_point.y - landing.top) < .15 &&
        std::abs(s.support_contact_point.x - landing.x) <= landing.half_x + .1 &&
        std::abs(s.support_contact_point.z - landing.z) <= landing.half_z + .1;
}

void steer(Simulation &simulation, const Landing &landing, double cap) {
    const auto s = simulation.snapshot();
    require(simulation.set_move_input(
                std::clamp(1.3 * (landing.x - s.player_position.x) - .45 * s.player_linear_velocity.x, -cap, cap),
                std::clamp(1.3 * (landing.z - s.player_position.z) - .45 * s.player_linear_velocity.z, -cap, cap)),
            "ordinary joystick accepted", landing.name, simulation);
}

void settle(Simulation &simulation, const Landing &landing, bool require_flight = false) {
    bool airborne = false;
    unsigned stable = 0;
    for (unsigned i = 0; i < 1800; ++i) {
        const auto s = simulation.snapshot();
        airborne = airborne || !s.player_grounded;
        const double distance = std::hypot(landing.x - s.player_position.x, landing.z - s.player_position.z);
        const bool arrived = on(s, landing) && distance < .18 &&
            std::hypot(s.player_linear_velocity.x, s.player_linear_velocity.z) < .4;
        stable = arrived ? stable + 1 : 0;
        if (stable >= 8) {
            require(!require_flight || airborne, "receiving contact follows actual flight", landing.name, simulation);
            require(simulation.set_move_input(0, 0), "release joystick", landing.name, simulation);
            require(simulation.set_sprint_input(false), "release sprint", landing.name, simulation);
            std::cout << "PASS UPPER_GALLERY landing=" << landing.name << " top=" << landing.top
                      << " tick=" << s.tick_index << '\n';
            return;
        }
        // A grounded wrong surface after flight is a failed transfer, not a
        // license to walk around below the target for the remainder of a loop.
        if (require_flight && airborne && s.player_grounded)
            require(on(s, landing), "first receiving footing is intended deck", landing.name, simulation);
        steer(simulation, landing, require_flight ? .9 : .6);
        tick(simulation, landing.name);
    }
    require(false, "bounded route reaches stable actual footing", landing.name, simulation);
}

void walk(Simulation &simulation, double x, double z, const char *name) {
    settle(simulation, {name, x, z, 0, .8, .8});
}

// Start from real settled footing. Earn a measured run-up before requesting
// Jump; in particular, a short raised transfer must not launch from rest just
// because its target is already inside the older runner's distance threshold.
void jump(Simulation &simulation, const Landing &from, const Landing &to, double runup) {
    settle(simulation, from);
    const auto initial = simulation.snapshot();
    require(on(initial, from), "takeoff has fresh actual support", to.name, simulation);
    const double dx = to.x - initial.player_position.x;
    const double dz = to.z - initial.player_position.z;
    const double length = std::hypot(dx, dz);
    require(length > runup + .2, "run-up leaves a receiving gap", to.name, simulation);
    const double ux = dx / length, uz = dz / length;
    require(simulation.set_facing(ux, uz), "face takeoff direction", to.name, simulation);
    require(simulation.set_sprint_input(true), "ordinary sprint accepted", to.name, simulation);
    bool launched = false;
    for (unsigned i = 0; i < 180; ++i) {
        const auto s = simulation.snapshot();
        require(on(s, from), "run-up retains intended takeoff footing", to.name, simulation);
        const double travel = (s.player_position.x - initial.player_position.x) * ux +
                              (s.player_position.z - initial.player_position.z) * uz;
        const double forward_speed = s.player_linear_velocity.x * ux + s.player_linear_velocity.z * uz;
        if (travel >= runup && forward_speed >= .7) {
            require(simulation.request_jump(), "ordinary Jump accepted after run-up", to.name, simulation);
            launched = true;
            break;
        }
        require(simulation.set_move_input(.65 * ux, .65 * uz), "run-up joystick accepted", to.name, simulation);
        tick(simulation, to.name);
    }
    require(launched, "bounded run-up earns takeoff speed", to.name, simulation);
    // The Jump command is consumed by the next real tick. No pose, velocity,
    // support or finite-actuator state is supplied by this runner.
    steer(simulation, to, .9);
    tick(simulation, to.name);
    settle(simulation, to, true);
}

void stairs(Simulation &simulation) {
    walk(simulation, -63, -59.5, "north_clear_aisle");
    walk(simulation, -63, -62, "north_stair_approach");
    jump(simulation, {"grade_runup", -63, -62, 0, 1.8, 1},
         {"north_step1", -66, -62, .6, 1, 1.2}, .9);
    // Begin .6m east of each step centre. Its next riser blocks the capsule
    // centre .35m before the face; keep room for run-up plus arrival tolerance.
    jump(simulation, {"step1_runup", -65.4, -62, .6, .7, 1.2},
         {"north_step2", -68, -62, 1.2, 1, 1.2}, .35);
    jump(simulation, {"step2_runup", -67.4, -62, 1.2, .7, 1.2},
         {"north_step3", -70, -62, 1.8, 1, 1.2}, .35);
    jump(simulation, {"step3_runup", -69.4, -62, 1.8, .7, 1.2},
         {"gallery1", -71.8, -62, 2.4, 1.3, 4.5}, .35);
}

void intentional_miss(Simulation &simulation) {
    const Landing departure{"miss_takeoff", -71.3, -64.3, 2.4, 1.3, 2};
    settle(simulation, departure);
    const auto before = simulation.snapshot();
    // Deliberately steer east of P1 rather than granting an artificial fall.
    // This line clears the stair volumes and lands in the existing lower aisle.
    const Landing grade{"miss_grade", -66.7, -67.5, 0, 1, 1};
    require(simulation.set_facing(grade.x - before.player_position.x, grade.z - before.player_position.z),
            "miss facing accepted", grade.name, simulation);
    require(simulation.set_sprint_input(true), "miss sprint accepted", grade.name, simulation);
    require(simulation.request_jump(), "intentional miss uses ordinary Jump", grade.name, simulation);
    bool airborne = false, fell_below_receiver = false, landed = false;
    for (unsigned i = 0; i < 900; ++i) {
        steer(simulation, grade, .9);
        tick(simulation, grade.name);
        const auto s = simulation.snapshot();
        airborne = airborne || !s.player_grounded;
        fell_below_receiver = fell_below_receiver || (airborne && s.player_position.y < perches[0].top);
        require(!on(s, perches[0]), "intentional miss does not count as perch contact", grade.name, simulation);
        if (airborne && s.player_grounded) {
            require(fell_below_receiver && on(s, grade), "miss ends on real grade", grade.name, simulation);
            landed = true;
            break;
        }
    }
    require(landed, "bounded missed jump reaches survivable grade", grade.name, simulation);
    settle(simulation, grade);
    walk(simulation, -63, -67.5, "lower_recovery_aisle");
    stairs(simulation);
}
}

int main() {
    Simulation simulation(InitialSpawn::FreightYardArrival, WorldContent::Slingshot);
    tick(simulation, "opening", 90);
    require(simulation.snapshot().player_grounded, "normal arrival has real ground", "opening", simulation);
    walk(simulation, -80, -43.5, "arrival_stock_clearance");
    walk(simulation, -48, -43.5, "arrival_crossing");
    walk(simulation, -48, -49, "arrival_aisle");
    walk(simulation, -48, -55, "utility_entrance");
    // Entity1480's rear bearing posts occupy x=-49/-47.9 at
    // z=-57.55..-57.25; its entry plinth spans x=-52.2..-49.65.
    // Pass north of the utility plinths (north edges z=-53.8), then
    // use their 1.6m gap at x=-62 before turning into the south aisle.
    walk(simulation, -48, -52.9, "swing_frame_north_bypass");
    walk(simulation, -62, -52.9, "utility_north_clearance");
    walk(simulation, -62, -59.5, "utility_between_plinths");
    stairs(simulation);
    intentional_miss(simulation);
    unsigned perch_count = 0;
    jump(simulation, {"gallery1_runup", -72, -63.8, 2.4, 1.5, 2.7}, perches[0], 1.5);
    ++perch_count;
    jump(simulation, perches[0], {"gallery2", -71.6, -70.5, 2.4, 1.1, 2}, .2);
    jump(simulation, {"gallery2_runup", -72, -74.8, 2.4, 1.5, 2.7}, perches[1], 1.5);
    ++perch_count;
    jump(simulation, perches[1], {"gallery3", -72.3, -81.5, 2.4, 1.1, 2}, .2);
    jump(simulation, {"gallery3_runup", -72, -85.8, 2.4, 1.5, 2.7}, perches[2], 1.5);
    ++perch_count;
    jump(simulation, perches[2], {"gallery4", -71.6, -92.5, 2.4, 1.1, 2}, .2);
    settle(simulation, {"south_gallery_exit", -71.8, -98, 2.4, 1.3, 1.5});
    settle(simulation, {"south_step3", -70, -98, 1.8, 1, 1.2});
    settle(simulation, {"south_step2", -68, -98, 1.2, 1, 1.2});
    settle(simulation, {"south_step1", -66, -98, .6, 1, 1.2});
    walk(simulation, -63, -98, "supported_ground_exit");
    walk(simulation, -63, -101, "ordinary_onward_walk");
    require(perch_count == 3, "all actual perches visited", "finish", simulation);
    std::cout << "SCRAPERX_UPPER_GALLERY PASS perches=" << perch_count
              << " miss_recovery=1 retry=1 deaths=" << simulation.snapshot().death_count
              << " public_input=1 no_reset=1 tick=" << simulation.snapshot().tick_index << '\n';
}
