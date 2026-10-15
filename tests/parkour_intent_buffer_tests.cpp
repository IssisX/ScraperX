#include "sim/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace scraperx::sim;
namespace {
void require(bool value, const char *why) {
    if (!value) throw std::runtime_error(why);
}
void tick(Simulation &s, unsigned count = 1) {
    for (unsigned i = 0; i < count; ++i)
        require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "native tick accepted");
}
void stage(Simulation &s) {
    // The sole staging operation. Gravity and real capsule/deck contacts own
    // all subsequent motion; no injected support or velocity is used.
    require(s.debug_restart_at({0, 1.2, -20}), "stage above clear static deck");
}
unsigned validate_floor() {
    Simulation s(InitialSpawn::StaticDeck, WorldContent::RegressionFixtures);
    stage(s);
    unsigned first = 0;
    for (unsigned i = 1; i <= 50; ++i) {
        tick(s);
        if (s.snapshot().player_grounded) {
            require(s.snapshot().support_entity_id == Simulation::kStaticDeckEntityId,
                    "receiving support is actual static deck");
            first = i;
            break;
        }
        require(s.snapshot().landing_jump_work_j == 0, "passive fall has no Jump work");
    }
    require(first > 15 && first < 40, "fixture has airborne approach and bounded real landing");
    tick(s, 10);
    require(s.snapshot().player_grounded && std::abs(s.snapshot().player_linear_velocity.y) < .01,
            "passive fixture stays supported");
    std::cout << "fixture first_support_tick=" << first << " support="
              << s.snapshot().support_entity_id << '\n';
    return first;
}
void early_jump(unsigned landing_tick, bool repeat_caller_frames) {
    Simulation s(InitialSpawn::StaticDeck, WorldContent::RegressionFixtures);
    stage(s);
    tick(s, landing_tick - 5);
    require(!s.snapshot().player_grounded && s.snapshot().player_linear_velocity.y < -1,
            "fresh Jump is requested during actual falling approach");
    require(s.request_jump(), "early Jump accepted");
    if (repeat_caller_frames) {
        // Four render/caller frames advance only one native tick. The held
        // delivery must coalesce into one command, not four queued takeoffs.
        for (unsigned frame = 0; frame < 4; ++frame) {
            require(s.request_jump(), "repeated caller-frame Jump accepted");
            require(s.advance_frame(Simulation::kFixedStepSeconds / 4).accepted,
                    "quarter native-step caller frame accepted");
        }
    } else tick(s);
    bool supported = false, departed = false, landed_again = false;
    double peak = -100, work = 0;
    unsigned work_events = 0;
    for (unsigned i = 0; i < 180; ++i) {
        tick(s);
        const auto &p = s.snapshot();
        if (p.player_grounded) {
            require(p.support_entity_id == Simulation::kStaticDeckEntityId,
                    "Jump waits for real receiving deck support");
            supported = true;
            if (departed) landed_again = true;
        }
        if (!supported) {
            require(p.player_linear_velocity.y <= 0 && p.landing_jump_work_j == 0 && !p.foot_push_active,
                    "buffer cannot manufacture takeoff before actual support");
        }
        if (p.landing_jump_work_j > work + .001) ++work_events;
        work = p.landing_jump_work_j;
        peak = std::max(peak, p.player_linear_velocity.y);
        if (supported && !p.player_grounded && p.player_linear_velocity.y > 1) departed = true;
    }
    std::cout << (repeat_caller_frames ? "caller_frames" : "early") << " peak_vy=" << peak
              << " jump_work_J=" << work << " work_events=" << work_events
              << " returned_to_floor=" << landed_again << '\n';
    require(supported && departed && peak > 1 && work > 0,
            "fresh pre-contact Jump produces useful upward motion after support");
    require(landed_again && s.snapshot().player_grounded && work_events == 1 && work <= 3000.01,
            "one buffered command yields one bounded takeoff and no landing bounce");
}
void expired_jump(unsigned landing_tick) {
    Simulation s(InitialSpawn::StaticDeck, WorldContent::RegressionFixtures);
    stage(s);
    require(s.request_jump(), "over-age early Jump accepted");
    double peak_after_contact = -100;
    bool supported = false;
    for (unsigned i = 0; i < landing_tick + 40; ++i) {
        tick(s);
        supported = supported || s.snapshot().player_grounded;
        if (supported) peak_after_contact = std::max(peak_after_contact, s.snapshot().player_linear_velocity.y);
        require(s.snapshot().landing_jump_work_j == 0 && !s.snapshot().foot_push_active,
                "expired early intent cannot command work on delayed landing");
    }
    std::cout << "expiry peak_after_contact_vy=" << peak_after_contact << '\n';
    require(supported && s.snapshot().player_grounded && peak_after_contact < .1,
            "over-age intent expires before real floor landing");
}
void cancelled_jump(unsigned landing_tick) {
    for (bool crouch : {false, true}) {
        Simulation s(InitialSpawn::StaticDeck, WorldContent::RegressionFixtures);
        stage(s);
        tick(s, landing_tick - 5);
        require(s.request_jump(), "queued Jump before cancellation accepted");
        tick(s);
        require(!s.snapshot().player_grounded, "cancellation happens before actual support");
        if (crouch) require(s.set_crouch_input(true), "crouch cancellation input accepted");
        else {
            // In air Release has no held body to drop, so false is expected;
            // its public implementation still cancels the pending Jump.
            require(!s.request_release(), "airborne Release reports no held attachment");
        }
        tick(s);
        if (crouch) require(s.set_crouch_input(false), "release crouch before landing");
        double peak = -100, work = 0;
        for (unsigned i = 0; i < 45; ++i) {
            tick(s);
            peak = std::max(peak, s.snapshot().player_linear_velocity.y);
            work = std::max(work, s.snapshot().landing_jump_work_j);
        }
        std::cout << "cancel=" << (crouch ? "crouch" : "release") << " peak_vy=" << peak
                  << " jump_work_J=" << work << '\n';
        require(work == 0 && peak < .1 && s.snapshot().player_grounded,
                "explicit cancellation erases queued intent even after input release");
    }
}
void fresh_crouch_jump(unsigned landing_tick) {
    Simulation s(InitialSpawn::StaticDeck, WorldContent::RegressionFixtures);
    stage(s);
    tick(s, landing_tick + 10);
    require(s.snapshot().player_grounded &&
            s.snapshot().support_entity_id == Simulation::kStaticDeckEntityId,
            "fresh crouch Jump begins on actual settled deck support");
    require(s.set_crouch_input(true), "hold crouch input");
    tick(s, 10);
    require(s.snapshot().player_crouched && s.snapshot().player_grounded,
            "real short capsule is supported before fresh Jump");
    const double before_work = s.snapshot().landing_jump_work_j;
    require(s.request_jump(), "fresh Jump accepted while crouch remains held");
    bool stood = false, departed = false;
    double peak = -100, work = 0;
    for (unsigned i = 0; i < 30; ++i) {
        tick(s);
        const auto &p = s.snapshot();
        stood = stood || !p.player_crouched;
        departed = departed || (!p.player_grounded && p.player_linear_velocity.y > 1);
        peak = std::max(peak, p.player_linear_velocity.y);
        work = std::max(work, p.landing_jump_work_j - before_work);
    }
    std::cout << "fresh_crouch stood=" << stood << " departed=" << departed
              << " peak_vy=" << peak << " jump_work_J=" << work << '\n';
    require(stood && departed && peak > 1 && work > 0 && work <= 3000.01,
            "fresh Jump stands in clear space and performs one finite takeoff while crouch held");
}
std::vector<Snapshot> caller_rate_run(unsigned frames_per_checkpoint, const char *name) {
    Simulation s(InitialSpawn::StaticDeck, WorldContent::RegressionFixtures);
    stage(s);
    std::vector<Snapshot> checkpoints;
    unsigned native_steps = 0, work_events = 0;
    double last_work = 0, peak = -100;
    bool departed = false, returned = false;
    for (unsigned checkpoint = 1; checkpoint <= 60; ++checkpoint) {
        // All rates reach .2 s / native tick 18 exactly before the same
        // one-shot input is queued for tick 19. No caller repeats it.
        if (checkpoint == 7) {
            require(native_steps == 18 && !s.snapshot().player_grounded,
                    "rate input is scheduled at .2 seconds before real contact");
            require(s.request_jump(), "common-time Jump accepted");
        }
        const double delta = 3 * Simulation::kFixedStepSeconds / frames_per_checkpoint;
        for (unsigned frame = 0; frame < frames_per_checkpoint; ++frame) {
            const auto result = s.advance_frame(delta);
            require(result.accepted, "caller-rate frame accepted");
            native_steps += result.steps_advanced;
            const auto &p = s.snapshot();
            peak = std::max(peak, p.player_linear_velocity.y);
            if (p.landing_jump_work_j > last_work + .001) ++work_events;
            last_work = p.landing_jump_work_j;
            if (last_work > 0 && !p.player_grounded && p.player_linear_velocity.y > 1) departed = true;
            if (departed && p.player_grounded) returned = true;
        }
        require(native_steps == checkpoint * 3 && s.snapshot().tick_index == native_steps,
                "every common checkpoint has the same exact native step count");
        checkpoints.push_back(s.snapshot());
    }
    std::cout << "caller_rate=" << name << " native_steps=" << native_steps
              << " work_events=" << work_events << " peak_vy=" << peak
              << " returned=" << returned << '\n';
    require(native_steps == 180 && departed && returned && s.snapshot().player_grounded &&
            s.snapshot().support_entity_id == Simulation::kStaticDeckEntityId &&
            work_events == 1 && last_work > 0 && last_work <= 3000.01,
            "each rate performs one finite takeoff and returns to actual floor by two seconds");
    return checkpoints;
}
void caller_rates() {
    const auto baseline = caller_rate_run(3, "90Hz");
    for (const auto frames : {2U, 12U, 1U}) {
        const char *name = frames == 2 ? "60Hz" : frames == 12 ? "360Hz" : "grouped3ticks";
        const auto actual = caller_rate_run(frames, name);
        double max_position_error = 0, max_velocity_error = 0;
        for (unsigned i = 0; i < baseline.size(); ++i) {
            const auto &a = actual[i]; const auto &b = baseline[i];
            require(a.tick_index == b.tick_index && a.player_grounded == b.player_grounded &&
                    a.player_crouched == b.player_crouched && a.support_entity_id == b.support_entity_id &&
                    a.traversal_state == b.traversal_state && a.fall_state == b.fall_state &&
                    a.death_count == b.death_count && a.landing_count == b.landing_count &&
                    a.landing_response == b.landing_response && a.foot_push_active == b.foot_push_active &&
                    a.foot_push_start_count == b.foot_push_start_count &&
                    a.foot_push_stop_reason == b.foot_push_stop_reason,
                    "native movement decisions agree at every common checkpoint");
            const auto max_component_error = [](Vector3 x, Vector3 y) {
                return std::max({std::abs(x.x-y.x), std::abs(x.y-y.y), std::abs(x.z-y.z)});
            };
            max_position_error = std::max(max_position_error, max_component_error(a.player_position, b.player_position));
            max_velocity_error = std::max(max_velocity_error, max_component_error(a.player_linear_velocity, b.player_linear_velocity));
            require(max_position_error <= 1e-5 && max_velocity_error <= 1e-5 &&
                    std::abs(a.landing_jump_work_j - b.landing_jump_work_j) <= 1e-5 &&
                    std::abs(a.foot_push_command_work_bound_j - b.foot_push_command_work_bound_j) <= 1e-5,
                    "body state and commanded work agree throughout the fixed-tick trajectory");
        }
        std::cout << "matched_rate=" << name << " checkpoints=" << actual.size()
                  << " max_position_error_m=" << max_position_error
                  << " max_velocity_error_mps=" << max_velocity_error << '\n';
    }
}
unsigned coyote_event_tick() {
    Simulation s;
    require(s.debug_restart_at({35, 1.77, -48}), "stage actual stock for edge calibration");
    tick(s, 45);
    require(s.snapshot().player_grounded && s.snapshot().support_entity_id == Simulation::kWorldSolidEntityId,
            "edge calibration starts on actual stock support");
    require(s.set_facing(0, 1) && s.set_move_input(0, .4), "calibration walking inputs accepted");
    for (unsigned absolute_tick = 46; absolute_tick <= 240; ++absolute_tick) {
        tick(s);
        if (!s.snapshot().player_grounded) {
            const unsigned event = ((absolute_tick + 2) / 3) * 3;
            std::cout << "coyote calibration loss_tick=" << absolute_tick << " event_tick=" << event << '\n';
            require(absolute_tick >= 147 && absolute_tick <= 207 && event - absolute_tick <= 2,
                    "stock edge loss remains near accepted tick177 and common-boundary delay is at most two ticks");
            require(s.snapshot().foot_push_active, "real departing stock contact retained as live plant");
            return event;
        }
    }
    throw std::runtime_error("calibration never departed real stock edge");
}
std::vector<Snapshot> coyote_rate_run(unsigned frames_per_checkpoint, const char *name, unsigned event_tick) {
    Simulation s;
    require(s.debug_restart_at({35, 1.77, -48}), "stage actual stock once for rate run");
    std::vector<Snapshot> checkpoints;
    unsigned native_steps = 0;
    double peak_after_jump = -100, peak_force = 0, work = 0;
    for (unsigned checkpoint = 1; checkpoint <= 90; ++checkpoint) {
        if (native_steps == 45) {
            require(s.snapshot().player_grounded && s.snapshot().support_entity_id == Simulation::kWorldSolidEntityId,
                    "rate run settles on actual stock before walking");
            require(s.set_facing(0, 1) && s.set_move_input(0, .4), "common-time stock walking input accepted");
        }
        if (native_steps == event_tick) {
            require(!s.snapshot().player_grounded && s.snapshot().foot_push_active,
                    "common-time late Jump has actual contact loss and a live physical plant");
            require(s.request_jump(), "common-time late Jump accepted");
        }
        const double delta = 3 * Simulation::kFixedStepSeconds / frames_per_checkpoint;
        for (unsigned frame = 0; frame < frames_per_checkpoint; ++frame) {
            const auto result = s.advance_frame(delta);
            require(result.accepted, "coyote caller frame accepted");
            native_steps += result.steps_advanced;
            const auto &p = s.snapshot();
            if (native_steps > event_tick) {
                peak_after_jump = std::max(peak_after_jump, p.player_linear_velocity.y);
                peak_force = std::max(peak_force, p.foot_push_player_force.y);
                work = std::max(work, p.foot_push_command_work_bound_j);
            }
        }
        require(native_steps == checkpoint * 3 && s.snapshot().tick_index == native_steps,
                "coyote common checkpoint has exact native step count");
        checkpoints.push_back(s.snapshot());
    }
    std::cout << "coyote_rate=" << name << " native_steps=" << native_steps << " peak_vy=" << peak_after_jump
              << " peak_foot_force_N=" << peak_force << " command_work_J=" << work
              << " push_starts=" << s.snapshot().foot_push_start_count << '\n';
    require(native_steps == 270 && peak_after_jump > 1 && peak_force > 0 && work > 0 &&
            s.snapshot().foot_push_start_count == 1,
            "each caller rate activates one useful finite actual trailing-foot push");
    return checkpoints;
}
void coyote_rates() {
    const unsigned event_tick = coyote_event_tick();
    const auto baseline = coyote_rate_run(3, "90Hz", event_tick);
    for (const auto frames : {2U, 12U, 1U}) {
        const char *name = frames == 2 ? "60Hz" : frames == 12 ? "360Hz" : "grouped3ticks";
        const auto actual = coyote_rate_run(frames, name, event_tick);
        double position_error = 0, velocity_error = 0, force_error = 0;
        for (unsigned i = 0; i < baseline.size(); ++i) {
            const auto &a = actual[i]; const auto &b = baseline[i];
            require(a.tick_index == b.tick_index && a.player_grounded == b.player_grounded &&
                    a.support_entity_id == b.support_entity_id && a.traversal_state == b.traversal_state &&
                    a.fall_state == b.fall_state && a.death_count == b.death_count &&
                    a.landing_count == b.landing_count && a.landing_response == b.landing_response &&
                    a.foot_push_active == b.foot_push_active && a.foot_push_start_count == b.foot_push_start_count &&
                    a.foot_push_stop_reason == b.foot_push_stop_reason &&
                    a.foot_push_support_entity_id == b.foot_push_support_entity_id,
                    "coyote native decisions and actual support agree at every common checkpoint");
            const auto error = [](Vector3 x, Vector3 y) {
                return std::max({std::abs(x.x-y.x), std::abs(x.y-y.y), std::abs(x.z-y.z)});
            };
            position_error = std::max(position_error, error(a.player_position, b.player_position));
            velocity_error = std::max(velocity_error, error(a.player_linear_velocity, b.player_linear_velocity));
            force_error = std::max(force_error, error(a.foot_push_player_force, b.foot_push_player_force));
            require(position_error <= 1e-5 && velocity_error <= 1e-5 && force_error <= 1e-5 &&
                    std::abs(a.foot_push_elapsed_seconds - b.foot_push_elapsed_seconds) <= 1e-8 &&
                    std::abs(a.foot_push_command_work_bound_j - b.foot_push_command_work_bound_j) <= 1e-5 &&
                    std::abs(a.foot_push_stroke_m - b.foot_push_stroke_m) <= 1e-5 &&
                    std::abs(a.landing_jump_work_j - b.landing_jump_work_j) <= 1e-5,
                    "coyote force epoch, elapsed time, work and body trajectory agree across caller batching");
        }
        std::cout << "matched_coyote_rate=" << name << " checkpoints=" << actual.size()
                  << " max_position_error_m=" << position_error << " max_velocity_error_mps=" << velocity_error
                  << " max_force_error_N=" << force_error << '\n';
    }
}
} // namespace
int main(int argc, char **argv) {
    const auto mode = argc > 1 ? std::string_view(argv[1]) : std::string_view("all");
    unsigned failures = 0;
    try {
        const unsigned landing_tick = validate_floor();
        const auto run = [&](std::string_view name, auto test) {
            if (mode != "all" && mode != name) return;
            try { test(); std::cout << "PASS " << name << '\n'; }
            catch (const std::exception &error) { ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
        };
        run("early", [&] { early_jump(landing_tick, false); });
        run("expiry", [&] { expired_jump(landing_tick); });
        run("cancel", [&] { cancelled_jump(landing_tick); });
        run("caller_frames", [&] { early_jump(landing_tick, true); });
        run("fresh_crouch", [&] { fresh_crouch_jump(landing_tick); });
        run("caller_rates", [&] { caller_rates(); });
        run("coyote_rates", [&] { coyote_rates(); });
    } catch (const std::exception &error) {
        std::cerr << "FAIL fixture: " << error.what() << '\n';
        return 1;
    }
    return failures ? 1 : 0;
}
