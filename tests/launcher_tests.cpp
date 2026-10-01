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
struct Shot {
    double apex, draw, energy, landed_y;
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
    require(machine.aim_locked, "charged native aim lock reaches the Simulation snapshot");
    require(machine.ledger_valid, "closed normal-world draw retained a stale pre-BOARD contact witness");
    advance(s, .5);
    require(std::abs(s.slingshot_state().draw_m - draw) < .03, "ratchet holds actual draw");
    require(s.request_slingshot_action(), "release request accepted");
    double apex = s.snapshot().player_position.y;
    double landed_y = 0;
    bool deployed = false;
    bool crossed_300m = false;
    Vector3 ascent_at_300m{};
    for (int n = 0; n < 90 * 18; ++n) {
        const auto previous = s.snapshot().player_position;
        require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "flight tick accepted");
        const auto current = s.snapshot().player_position;
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
    require(s.slingshot_state().launch_count > 0, "release is recorded");
    return {apex, draw, energy, landed_y, s.snapshot().player_position, ascent_at_300m,
            s.snapshot().death_count, crossed_300m};
}
}
int main() {
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
