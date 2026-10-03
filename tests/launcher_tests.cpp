#include "sim/simulation.hpp"
#include "sim/slingshot.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace scraperx::sim;
namespace {
void require(bool condition, const char *message) {
    if (!condition) { std::cerr << "FAIL " << message << '\n'; std::exit(EXIT_FAILURE); }
}
void advance(Simulation &s, double seconds) {
    for (int n = 0; n < int(std::round(seconds * 90)); ++n)
        require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "fixed tick accepted");
}
void rider_observables() {
    Simulation s;
    advance(s, 1);
    const auto standing = s.slingshot_state();
    require(std::abs(standing.rider_specific_acceleration.y - 9.81) < .02,
            "standing support supplies one g of specific acceleration");
    require(s.debug_restart_at({70, 100, -25}), "rider observable stages a clear gravity fall");
    const auto reset = s.slingshot_state();
    require(reset.rider_specific_acceleration.y == 0 &&
            s.render_slingshot_state().rider_specific_acceleration.y == 0,
            "explicit restart clears the measured acceleration without a false impulse");
    advance(s, .2);
    const auto falling = s.slingshot_state();
    require(std::abs(falling.rider_specific_acceleration.y) < .02 &&
            s.snapshot().player_linear_velocity.y < -1,
            "ballistic drift has gravity acceleration and zero specific load");
    const auto tick = falling.tick_index;
    const auto before_velocity = s.snapshot().player_linear_velocity;
    require(s.advance_frame(0).steps_advanced == 0 && s.slingshot_state().tick_index == tick &&
            s.slingshot_state().rider_specific_acceleration.y == falling.rider_specific_acceleration.y,
            "zero-advance/read-only observation cannot evolve rider dynamics");
    require(s.snapshot().player_linear_velocity.y == before_velocity.y,
            "reading inertial evidence supplies no impulse");
    const auto neutral = Slingshot::neutral_position();
    require(s.debug_restart_at({neutral.GetX(), neutral.GetY() + .85, neutral.GetZ()}),
            "rider seat proof stages at the actual basin centre");
    advance(s, .5);
    require(s.request_slingshot_action(), "rider seat proof boards");
    advance(s, 1);
    const auto seated = s.slingshot_state();
    require(seated.seated && seated.harness_rest_local.y > .6 &&
            std::abs(seated.seat_surface_position.y - seated.pouch_position.y + .17 -
                     seated.leather_deflection_m) < .0001,
            "seated pelvis target lies on the loaded leather basin rather than the capsule midpoint");
    require(seated.fixed_step_seconds == Simulation::kFixedStepSeconds &&
            seated.tick_index == s.snapshot().tick_index &&
            seated.simulation_time_seconds == s.snapshot().simulation_time_seconds,
            "rider samples identify their authoritative native clock");
    require(s.set_slingshot_input(1, 0, Slingshot::State{}.elevation_rad),
            "seat render proof draws through real native muscle work");
    advance(s, .2);
    const auto previous = s.slingshot_state();
    advance(s, Simulation::kFixedStepSeconds);
    const auto current = s.slingshot_state();
    require(s.advance_frame(Simulation::kFixedStepSeconds * .5).steps_advanced == 0,
            "seat render half tick leaves authoritative dynamics unchanged");
    const auto rendered = s.render_slingshot_state();
    require(std::abs(rendered.seat_surface_position.z -
                     (previous.seat_surface_position.z + current.seat_surface_position.z) * .5) < .0001 &&
            std::abs(rendered.simulation_time_seconds -
                     (previous.simulation_time_seconds + current.simulation_time_seconds) * .5) < 1e-9,
            "rider seat and inertial clock share the rendered native interpolation");
    std::cout << "RIDER specific_g=" << standing.rider_specific_acceleration.y
              << " freefall_specific=" << falling.rider_specific_acceleration.y
              << " seat_local_y=" << seated.seat_surface_position.y - seated.pouch_position.y << '\n';
}
struct Shot {
    double apex, draw, energy, landed_y, peak_specific_acceleration;
    Vector3 end, ascent_at_300m;
    std::uint64_t deaths;
    bool crossed_300m;
};
Shot shot(double seconds, double yaw = 0, double elevation = Slingshot::State{}.elevation_rad,
          bool use_parachute = false) {
    Simulation s;
    const auto at = Slingshot::neutral_position();
    require(s.debug_restart_at({at.GetX(), at.GetY() + .85, at.GetZ()}), "explicit playtest move to clear pouch");
    advance(s, .5);
    auto machine = s.slingshot_state();
    std::cout << "seat available=" << machine.station_available << " p="
              << s.snapshot().player_position.y << " pouch=" << machine.pouch_position.y << '\n';
    require(machine.station_available, "real pouch offers enter");
    require(s.set_slingshot_input(0, yaw, elevation), "aim accepted");
    require(s.request_slingshot_action(), "enter request accepted");
    advance(s, .1);
    require(s.slingshot_state().seated, "physical harness attached");
    require(!s.slingshot_state().aim_locked, "BOARD retains slack native aim state");
    require(s.set_slingshot_input(1, yaw, elevation), "manual backward draw accepted");
    advance(s, seconds);
    require(s.set_slingshot_input(0, yaw, elevation), "manual draw released");
    advance(s, .1);
    machine = s.slingshot_state();
    const double draw = machine.draw_m, energy = machine.energy_j;
    require(draw > .1 && energy > 0, "manual work stores elastic energy");
    require(!machine.aim_locked, "charged native aiming remains available through the Simulation snapshot");
    require(machine.ledger_valid, "closed normal-world draw retained a stale pre-BOARD contact witness");
    advance(s, .5);
    require(std::abs(s.slingshot_state().draw_m - draw) < .03, "ratchet holds actual draw");
    require(s.request_slingshot_action(), "release request accepted");
    double apex = s.snapshot().player_position.y;
    double landed_y = 0;
    bool deployed = false;
    bool crossed_300m = false;
    double peak_specific_acceleration = 0;
    Vector3 ascent_at_300m{};
    for (int n = 0; n < 90 * 18; ++n) {
        const auto previous = s.snapshot().player_position;
        require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "flight tick accepted");
        const auto current = s.snapshot().player_position;
        if (n < 90) {
            const auto load = s.slingshot_state().rider_specific_acceleration;
            peak_specific_acceleration = std::max(peak_specific_acceleration,
                std::sqrt(load.x*load.x + load.y*load.y + load.z*load.z));
        }
        apex = std::max(apex, current.y);
        if (!crossed_300m && s.snapshot().death_count == 0 && previous.y < 300 && current.y >= 300) {
            const double fraction = (300 - previous.y) / (current.y - previous.y);
            ascent_at_300m = {previous.x + (current.x - previous.x) * fraction, 300,
                             previous.z + (current.z - previous.z) * fraction};
            crossed_300m = true;
        }
        if (use_parachute && !deployed &&
            ((s.snapshot().player_position.y > 354) ||
             s.snapshot().player_linear_velocity.y < -1)) {
            require(s.request_parachute(), "ordered parachute input accepted");
            deployed = true;
        }
        if (s.snapshot().player_grounded && s.snapshot().support_entity_id == Simulation::kTowerEntityId &&
            s.snapshot().player_position.y > 100 && std::abs(s.snapshot().player_linear_velocity.y) < 1) {
            landed_y = s.snapshot().player_position.y - .9;
            break;
        }
    }
    std::cout << "shot draw=" << draw << " energy=" << energy << " apex=" << apex
              << " landed_y=" << landed_y << " deaths=" << s.snapshot().death_count
              << " end=" << s.snapshot().player_position.x << ',' << s.snapshot().player_position.y << ','
              << s.snapshot().player_position.z << " ascent_300m_x=" << ascent_at_300m.x
              << " launches=" << s.slingshot_state().launch_count << '\n';
    std::cout << "RIDER launch_peak_specific_mps2=" << peak_specific_acceleration << '\n';
    require(s.slingshot_state().launch_count > 0, "release is recorded");
    return {apex, draw, energy, landed_y, peak_specific_acceleration, s.snapshot().player_position, ascent_at_300m,
            s.snapshot().death_count, crossed_300m};
}
}
int main() {
    rider_observables();
    {
        Simulation fast;
        // A clear, explicitly staged fall proves the former one-metre render
        // cutoff with margin. Gravity supplies all speed; the elastic shot's
        // exit speed now lies close to that cutoff and is not a stable fixture.
        require(fast.debug_restart_at({70, 900, -25}), "fast render proof stages an unobstructed drop");
        bool witnessed = false;
        double maximum_distance = 0;
        for (int n = 0; n < 1200; ++n) {
            const auto before = fast.snapshot().player_position;
            advance(fast, Simulation::kFixedStepSeconds);
            const auto after = fast.snapshot().player_position;
            const double distance = std::sqrt(std::pow(after.x - before.x, 2) +
                                             std::pow(after.y - before.y, 2) + std::pow(after.z - before.z, 2));
            maximum_distance = std::max(maximum_distance, distance);
            if (distance <= 1.1 || fast.snapshot().death_count != 0) continue;
            require(fast.advance_frame(Simulation::kFixedStepSeconds * .5).steps_advanced == 0, "fast render half tick leaves authority fixed");
            const auto render = fast.render_player_position();
            require(std::abs(render.y - (before.y + after.y) * .5) < 1e-5 &&
                    std::abs(render.z - (before.z + after.z) * .5) < 1e-5,
                    "legitimate motion above 90 m/s stays interpolated during slow motion");
            witnessed = true;
            break;
        }
        std::cout << "gravity render fixture maximum_tick_m=" << maximum_distance << '\n';
        require(witnessed, "natural native fall exceeds the former render cutoff with margin");
    }
    {
        Simulation aiming;
        const auto neutral = aiming.slingshot_state().neutral_position;
        require(aiming.debug_restart_at({neutral.x, neutral.y + .85, neutral.z}), "aim render proof stages at pouch");
        advance(aiming, .5);
        require(aiming.request_slingshot_action(), "aim render proof boards");
        advance(aiming, .1);
        require(aiming.set_slingshot_input(0, .3, 1.10), "aim render proof requests both axes");
        advance(aiming, .1);
        const auto previous = aiming.slingshot_state();
        advance(aiming, Simulation::kFixedStepSeconds);
        const auto current = aiming.slingshot_state();
        const auto tick = aiming.snapshot().tick_index;
        require(std::abs(current.yaw_rad - previous.yaw_rad) > .0001,
                "finite native torque supplies distinct rotating rail endpoints");
        require(aiming.advance_frame(Simulation::kFixedStepSeconds * .5).steps_advanced == 0,
                "half-tick aim rendering does not step authority");
        const auto q = aiming.render_kit_body_rotation(aiming.kit_body_index(Slingshot::kLaunchRailEntity));
        const double axis_x = -2 * (q.x * q.z + q.w * q.y);
        const double axis_y = 2 * (q.w * q.x - q.y * q.z);
        const double axis_z = -(1 - 2 * (q.x * q.x + q.y * q.y));
        const auto rendered = aiming.render_slingshot_state();
        require(std::abs(rendered.yaw_rad - std::atan2(axis_x, -axis_z)) < 1e-5 &&
                std::abs(rendered.elevation_rad - std::asin(axis_y)) < 1e-5 &&
                std::abs(rendered.yaw_rad - current.yaw_rad) > .00001,
                "aim camera angles share the rail mesh SLERP instead of raw tick angles");
        require(aiming.snapshot().tick_index == tick && aiming.slingshot_state().yaw_rad == current.yaw_rad,
                "aim rendering leaves measured native angles untouched");
    }
    {
        Simulation interpolated;
        const auto neutral = interpolated.slingshot_state().neutral_position;
        require(interpolated.debug_restart_at({neutral.x, neutral.y + .85, neutral.z}), "render proof stages at actual pouch");
        advance(interpolated, .5);
        require(interpolated.request_slingshot_action(), "render proof boards");
        advance(interpolated, .1);
        require(interpolated.set_slingshot_input(1, 0, Slingshot::State{}.elevation_rad), "render proof manually draws");
        advance(interpolated, .2);
        const auto previous = interpolated.slingshot_state().pouch_position;
        const auto pouch = interpolated.kit_body_index(Slingshot::kPouchEntity);
        const auto previous_body = interpolated.kit_body_position(pouch);
        advance(interpolated, Simulation::kFixedStepSeconds);
        const auto current = interpolated.slingshot_state().pouch_position;
        const auto current_body = interpolated.kit_body_position(pouch);
        const auto authority = interpolated.snapshot();
        require(std::abs(current.z - previous.z) > .001, "moving native pouch supplies interpolation endpoints");
        require(interpolated.advance_frame(Simulation::kFixedStepSeconds * .5).steps_advanced == 0,
                "render-only half tick performs no physics step");
        const auto rendered = interpolated.render_slingshot_state().pouch_position;
        const auto rendered_body = interpolated.render_kit_body_position(pouch);
        require(std::abs(rendered.z - (previous.z + current.z) * .5) < 1e-5 &&
                std::abs(rendered_body.z - (previous_body.z + current_body.z) * .5) < 1e-5,
                "pouch leather and native kit share the interpolated half-tick pose");
        require(interpolated.snapshot().tick_index == authority.tick_index &&
                interpolated.slingshot_state().pouch_position.z == current.z,
                "render interpolation leaves native physics authority untouched");
        require(interpolated.debug_restart_at({20, 30, -120}), "render restart accepts clear point");
        require(interpolated.render_player_position().y == 30 &&
                interpolated.render_slingshot_state().pouch_position.z == interpolated.slingshot_state().pouch_position.z,
                "restart seeds presentation history without blending across relocation");
    }
    Simulation s;
    require(s.slingshot_state().available, "shipped default constructs slingshot");
    require(s.entity_body_count(2500) == 0 && s.entity_body_count(2502) == 0, "old grade machine absent");
    require(s.entity_body_count(1950) == 1 && s.entity_body_count(2900) == 1, "wooden frame and physical pouch present");
    advance(s, 1);
    require(s.snapshot().player_grounded, "normal player settles at grade");
    const auto settled = s.snapshot().player_position;
    require(s.debug_restart_at(settled), "current standing XYZ round-trips with solver contact slop");
    require(s.slingshot_state().energy_j < .001 && s.slingshot_state().work_j == 0, "no input no charge");
    const auto before = s.snapshot();
    require(!s.debug_restart_at({0, std::numeric_limits<double>::quiet_NaN(), 0}), "nonfinite restart rejected");
    require(!s.debug_restart_at({-30, 800, -330}), "restart inside solid tower rejected");
    require(s.snapshot().player_position.x == before.player_position.x &&
            s.snapshot().tick_index == before.tick_index, "rejection leaves state unchanged");
    require(s.debug_restart_at({20, 700, -150}), "arbitrary high clear point accepted");
    require(s.snapshot().player_linear_velocity.x == 0 && !s.snapshot().player_grounded, "restart clears momentum/contact");
    require(s.render_player_position().y == 700, "render pose immediately follows restart");
    const auto deaths = s.snapshot().death_count;
    require(s.restart_checkpoint() && s.snapshot().death_count == deaths, "menu restart does not count as death");
    const auto low = shot(1);
    const auto high = shot(6);
    require(high.draw > low.draw && high.energy > low.energy, "more input creates more stretch and energy");
    require(high.apex > low.apex + 5, "greater draw produces a substantially higher native flight");
    require(high.peak_specific_acceleration > 100 &&
            high.peak_specific_acceleration > low.peak_specific_acceleration,
            "real recoil load grows with the physical draw instead of a cinematic velocity/timer");
    require(high.apex > 250, "full draw reaches hundreds of metres in the actual shipping world");
    const auto replay = shot(6);
    require(std::abs(replay.apex - high.apex) < .001, "same initial state and fixed inputs reproduce flight");
    const auto landing = shot(6, 0, Slingshot::State{}.elevation_rad, true);
    require(landing.landed_y > 350 && landing.deaths == 0,
            "ordered launch and parachute inputs reach a real supported +352 m tower deck");
    const auto aimed = shot(6, .20, 1.43, true);
    require(aimed.crossed_300m && landing.crossed_300m &&
            std::abs(aimed.ascent_at_300m.x - landing.ascent_at_300m.x) > 5,
            "horizontal aim changes the actual flight direction at the same airborne height before any respawn");
    std::cout << "PASS launcher, real draw/release, deterministic native flight and restart boundary\n";
    return EXIT_SUCCESS;
}
