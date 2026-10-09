#include "sim/simulation.hpp"

#include <algorithm>
#include <limits>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void require(const bool condition, const char *message) {
    if (!condition) {
        std::cerr << "FAIL parkour flow: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void vault_carries_entry_and_exit_speed() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::TraversalState;
    using scraperx::sim::WorldContent;

    Simulation run(InitialSpawn::VaultApproach, WorldContent::RegressionFixtures);
    require(run.set_facing(1.0, 0.0), "vault facing accepted");
    require(run.set_move_input(0.0, 0.0), "settle input accepted");
    require(run.advance_frame(1.0).accepted && run.snapshot().player_grounded,
            "vault approach settles on the deck");
    require(run.set_move_input(1.0, 0.0), "approach input accepted");

    bool rail_offered = false;
    for (int tick = 0; tick < 180; ++tick) {
        require(run.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "approach tick accepted");
        const auto state = run.snapshot();
        if (state.ledge_available && state.ledge_entity_id == Simulation::kVaultRailEntityId) {
            rail_offered = true;
            break;
        }
    }
    require(rail_offered, "real rail becomes vaultable");
    const double approach_speed = run.snapshot().player_linear_velocity.x;
    require(approach_speed > 3.0, "approach has running speed");
    require(run.request_traversal(), "vault request accepted");
    require(run.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "vault starts on the native clock");
    const auto entry = run.snapshot();
    require(entry.traversal_state == TraversalState::Vaulting, "rail commits a vault");
    std::cout << "INFO vault entry approach=" << approach_speed
              << " first=" << entry.player_linear_velocity.x << '\n';
    require(std::abs(entry.player_linear_velocity.x - approach_speed) < 0.6,
            "vault must enter near running speed instead of snapping to path-average speed");

    double last_vault_speed = entry.player_linear_velocity.x;
    bool completed = false;
    for (int tick = 0; tick < 90; ++tick) {
        require(run.advance_frame(Simulation::kFixedStepSeconds).accepted, "vault tick accepted");
        const auto state = run.snapshot();
        if (state.traversal_state == TraversalState::Vaulting) {
            last_vault_speed = state.player_linear_velocity.x;
        } else if (state.accepted_traversal_count == 1) {
            std::cout << "INFO vault exit before=" << last_vault_speed
                      << " after=" << state.player_linear_velocity.x << '\n';
            require(std::abs(state.player_linear_velocity.x - last_vault_speed) < 0.6,
                    "vault must leave near its carried speed instead of snapping at exit");
            require(state.player_position.x > 5.6, "vault crosses the rail");
            require(state.aborted_traversal_count == 0, "valid vault does not abort");
            completed = true;
            break;
        }
    }
    require(completed, "vault finishes on its real landing");
}

void rendered_pose_fills_between_fixed_ticks() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::WorldContent;

    Simulation run(InitialSpawn::StaticDeck, WorldContent::RegressionFixtures);
    require(run.set_move_input(0.0, 0.0), "settle input accepted");
    require(run.advance_frame(1.0).accepted, "deck settle accepted");
    require(run.set_move_input(1.0, 0.0), "running input accepted");
    const auto before = run.snapshot().player_position;
    require(run.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "first moving fixed tick accepted");
    const auto after = run.snapshot().player_position;
    require(after.x > before.x, "player really advances on a fixed tick");

    const auto at_tick = run.render_player_position();
    require(std::abs(at_tick.x - before.x) < 1.0e-5,
            "display pose begins at previous authoritative tick");
    require(run.advance_frame(Simulation::kFixedStepSeconds * 0.5).accepted,
            "half display tick accepted");
    const auto halfway = run.render_player_position();
    require(std::abs(halfway.x - (before.x + after.x) * 0.5) < 1.0e-5,
            "display pose is halfway between authoritative positions");
    require(std::abs(run.snapshot().player_position.x - after.x) < 1.0e-8,
            "render interpolation does not move native physics");
}

double horizontal_speed(const scraperx::sim::Snapshot &s) {
    return std::hypot(s.player_linear_velocity.x, s.player_linear_velocity.z);
}

void ordinary_jump_intent_uses_native_contact_and_clock() {
    using namespace scraperx::sim;

    const auto tick = [](Simulation &run) {
        const auto result = run.advance_frame(Simulation::kFixedStepSeconds);
        require(result.accepted && result.steps_advanced == 1,
                "Jump intent advances on exactly one native tick");
        const auto state = run.snapshot();
        require(state.death_count == 0 && state.traversal_state == TraversalState::None &&
                    state.traversal_hand_constraint_count == 0 && state.jump_vault_count == 0,
                "ordinary Jump intent never uses traversal, a vault or checkpoint rescue");
        return state;
    };
    const auto settle = [&](Simulation &run) {
        require(run.set_move_input(0.0, 0.0) && run.set_facing(1.0, 0.0),
                "ordinary grade neutral Jump inputs accepted");
        for (int i = 0; i < 180; ++i) (void)tick(run);
        const auto state = run.snapshot();
        require(state.player_grounded && state.support_entity_id == Simulation::kStaticDeckEntityId &&
                    std::abs(state.player_linear_velocity.y) < 0.01 && horizontal_speed(state) < 0.01,
                "default normal-world Jump starts from real stable grade contact");
        return state;
    };

    {
        Simulation with_action;
        const auto action_floor = settle(with_action);
        require(with_action.request_jump() && with_action.request_traversal(),
                "same-batch grade Jump and Action requests accepted");
        bool ordinary_takeoff = false;
        for (int i = 0; i < 18; ++i) {
            const auto state = tick(with_action);
            require(state.rejected_traversal_count == action_floor.rejected_traversal_count + 1,
                    "flat-grade Action is genuinely rejected without a traversal affordance");
            ordinary_takeoff = ordinary_takeoff || (!state.player_grounded &&
                state.support_entity_id == 0 && state.player_linear_velocity.y > 0.5);
        }
        require(ordinary_takeoff,
                "a rejected same-batch Action cannot swallow a valid ordinary Jump");
    }

    // An input near the first hop's apex has no foot contact throughout the
    // buffer window. It must neither create an air jump nor survive to landing.
    Simulation expired;
    const auto expiry_floor = settle(expired);
    require(expired.request_jump(), "ordinary expiry preparation Jump accepted");
    bool apex_reached = false;
    for (int i = 0; i < 180; ++i) {
        const auto state = tick(expired);
        if (!state.player_grounded && state.player_position.y > expiry_floor.player_position.y + 0.6 &&
            std::abs(state.player_linear_velocity.y) < 1.0) {
            apex_reached = true;
            break;
        }
    }
    require(apex_reached, "ordinary preparation hop reaches a real airborne apex");
    const auto expiry_press = expired.snapshot();
    require(expired.request_jump(), "airborne expiry Jump intent accepted");
    std::uint64_t expiry_contact_tick = 0;
    for (int i = 0; i < 180; ++i) {
        const auto before = expired.snapshot();
        const auto after = tick(expired);
        if (after.landing_count == expiry_press.landing_count && !after.player_grounded) {
            require(after.player_linear_velocity.y <= before.player_linear_velocity.y + 0.01,
                    "buffered Jump cannot add upward velocity without actual foot contact");
        }
        if (after.player_grounded && expiry_contact_tick == 0) {
            expiry_contact_tick = after.tick_index;
            require(expiry_contact_tick > expiry_press.tick_index + 20,
                    "expiry case remains contact-free well beyond the0.12s buffer window");
        }
        if (expiry_contact_tick != 0) {
            require(after.player_grounded && after.support_entity_id == expiry_floor.support_entity_id &&
                        after.landing_count == expiry_press.landing_count + 1 &&
                        std::abs(after.player_position.y - expiry_floor.player_position.y) < 0.05 &&
                        std::abs(after.player_linear_velocity.y) < 0.1,
                    "expired airborne Jump cannot fire later when real ground finally arrives");
        }
    }
    require(expiry_contact_tick != 0, "expiry preparation hop returns to real grade footing");
    std::cout << "INFO Jump expiry press_tick=" << expiry_press.tick_index
              << " contact_after_ticks=" << expiry_contact_tick - expiry_press.tick_index << '\n';

    // The only second press is made while descending shortly before real
    // touchdown. Compare a nine-tick caller frame with individual90Hz ticks
    // across that exact contact interval, not merely after settling again.
    Simulation ticked;
    Simulation grouped;
    Simulation dropped;
    const auto floor = settle(ticked);
    (void)settle(grouped);
    const auto drop_floor = settle(dropped);
    require(ticked.request_jump() && grouped.request_jump() && dropped.request_jump(),
            "ordinary buffer preparation Jumps accepted");
    bool prelanding_reached = false;
    for (int i = 0; i < 180; ++i) {
        const auto state = tick(ticked);
        (void)tick(grouped);
        (void)tick(dropped);
        const double height = state.player_position.y - floor.player_position.y;
        if (!state.player_grounded && state.support_entity_id == 0 &&
            state.player_linear_velocity.y < -2.0 && height > 0.05 && height < 0.25) {
            prelanding_reached = true;
            break;
        }
    }
    require(prelanding_reached, "ordinary hop reaches an unsupported descending prelanding stance");
    const auto press = ticked.snapshot();
    require(ticked.request_jump() && grouped.request_jump(),
            "one short prelanding Jump intent per caller accepted");
    const auto drop_press = dropped.snapshot();
    require(!drop_press.player_grounded && drop_press.support_entity_id == 0 &&
                drop_press.player_linear_velocity.y < -2.0,
            "same-batch Drop case starts genuinely unsupported before landing");
    require(dropped.request_jump(), "same-batch prelanding Jump before Drop accepted");
    require(!dropped.request_release(),
            "unsupported Drop retains its native refusal while cancelling Jump intent");
    const auto distance = [](const Vector3 a, const Vector3 b) {
        return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) +
                         (a.z - b.z) * (a.z - b.z));
    };
    std::uint64_t contact_tick = 0;
    bool buffered_takeoff = false;
    for (int frame = 0; frame < 20; ++frame) {
        for (int i = 0; i < 9; ++i) {
            const auto before = ticked.snapshot();
            const auto after = tick(ticked);
            const auto drop_state = tick(dropped);
            if (drop_state.landing_count > drop_press.landing_count) {
                require(drop_state.player_grounded &&
                            drop_state.support_entity_id == drop_floor.support_entity_id &&
                            drop_state.landing_count == drop_press.landing_count + 1 &&
                            std::abs(drop_state.player_position.y - drop_floor.player_position.y) < 0.05 &&
                            std::abs(drop_state.player_linear_velocity.y) < 0.1,
                        "refused airborne Drop cancels the same-batch Jump without a later hop");
            }
            if (after.landing_count == press.landing_count && !after.player_grounded) {
                require(after.player_linear_velocity.y <= before.player_linear_velocity.y + 0.01,
                        "short prelanding Jump waits for contact instead of taking off freely in air");
            }
            if (after.landing_count > press.landing_count && contact_tick == 0) {
                contact_tick = after.tick_index;
                require(after.player_grounded && after.support_entity_id == floor.support_entity_id &&
                            contact_tick <= press.tick_index + 10,
                        "buffer case proves a genuine native landing inside the short intent window");
            }
            if (contact_tick != 0 && !after.player_grounded && after.support_entity_id == 0 &&
                after.player_linear_velocity.y > 0.5 &&
                after.player_position.y > floor.player_position.y + 0.05) {
                buffered_takeoff = true;
            }
        }
        const auto result = grouped.advance_frame(9.0 * Simulation::kFixedStepSeconds);
        require(result.accepted && result.steps_advanced == 9,
                "grouped Jump caller advances exactly nine native ticks");
        const auto one = ticked.snapshot();
        const auto many = grouped.snapshot();
        require(one.tick_index == many.tick_index && one.player_grounded == many.player_grounded &&
                    one.support_entity_id == many.support_entity_id &&
                    one.traversal_state == many.traversal_state &&
                    one.landing_count == many.landing_count && one.death_count == many.death_count &&
                    one.jump_vault_count == many.jump_vault_count &&
                    distance(one.player_position, many.player_position) < 0.00001 &&
                    distance(one.player_linear_velocity, many.player_linear_velocity) < 0.00001,
                "prelanding Jump outcome is identical for grouped frames and individual90Hz ticks");
        if (frame == 1) {
            std::cout << "INFO Jump buffer press_tick=" << press.tick_index
                      << " contact_tick=" << contact_tick << " after18_vy="
                      << one.player_linear_velocity.y << " grounded=" << one.player_grounded << '\n';
        }
    }
    require(contact_tick != 0 && buffered_takeoff,
            "one short prelanding Jump must survive until real contact and produce an ordinary takeoff");
    const auto final = ticked.snapshot();
    require(final.player_grounded && final.support_entity_id == floor.support_entity_id &&
                final.landing_count == press.landing_count + 2 &&
                std::abs(final.player_position.y - floor.player_position.y) < 0.05 &&
                std::abs(final.player_linear_velocity.y) < 0.1,
            "one buffered press produces exactly one hop, then returns to real footing without repeats");
    const auto drop_final = dropped.snapshot();
    require(drop_final.player_grounded && drop_final.support_entity_id == drop_floor.support_entity_id &&
                drop_final.landing_count == drop_press.landing_count + 1,
            "cancelled prelanding Jump returns to real footing with only its original landing");
}

void ordinary_tower_walk_uses_finite_contact_work() {
    using namespace scraperx::sim;

    // One supported stage reproduces the ordinary Tower velocity bypass.
    // Expire impact recovery before measuring actual contact locomotion.
    Simulation run;
    require(run.debug_restart_at({-24.7, 330.9, -158.25}),
            "ordinary Tower force staging accepted");
    require(run.set_move_input(0.0, 0.0) && run.set_facing(1.0, 0.0) &&
                run.set_sprint_input(false) && run.set_crouch_input(false),
            "ordinary Tower neutral walking policy accepted");
    for (int tick = 0; tick < 180; ++tick) {
        require(run.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "ordinary Tower settle tick accepted");
    }
    const auto settled = run.snapshot();
    require(settled.player_grounded && settled.support_entity_id == Simulation::kTowerEntityId &&
                std::abs(settled.player_position.y - 330.9) < 0.1 &&
                !settled.landing_recovering && settled.landing_recovery_seconds == 0.0 &&
                horizontal_speed(settled) < 0.01 && settled.death_count == 0,
            "ordinary Tower starts at real static footing after impact recovery expires");

    const double maximum_delta_v = 0.85 * 9.81 * Simulation::kFixedStepSeconds;
    const auto sample = [&](const double input_x) {
        const auto before = run.snapshot();
        require(run.set_move_input(input_x, 0.0), "ordinary Tower movement input accepted");
        require(run.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "ordinary Tower movement tick accepted");
        const auto after = run.snapshot();
        require(after.player_grounded && after.support_entity_id == Simulation::kTowerEntityId &&
                    after.traversal_state == TraversalState::None &&
                    after.traversal_hand_constraint_count == 0 &&
                    !after.player_crouched && !after.player_sprinting &&
                    !after.landing_recovering && after.landing_recovery_seconds == 0.0 &&
                    after.landing_count == settled.landing_count &&
                    after.step_up_count == settled.step_up_count && after.death_count == 0,
                "ordinary Tower sample keeps actual footing without impact, traversal, step or rescue");
        const double delta_v = std::hypot(
            after.player_linear_velocity.x - before.player_linear_velocity.x,
            after.player_linear_velocity.z - before.player_linear_velocity.z);
        require(std::isfinite(delta_v) && delta_v <= maximum_delta_v + 0.001,
                "ordinary Tower acceleration, braking and reversal obey finite0.85g contact authority");
        const double work = after.landing_recovery_work_j - before.landing_recovery_work_j;
        require(std::isfinite(after.landing_recovery_work_j) && std::isfinite(work) &&
                    work >= -0.000001 && work <= 3000.1 * Simulation::kFixedStepSeconds,
                "ordinary Tower contact-work receipt stays finite, nonnegative and within3kW");
        const double kinetic_gain = 0.5 * 85.0 *
            (horizontal_speed(after) * horizontal_speed(after) -
             horizontal_speed(before) * horizontal_speed(before));
        if (kinetic_gain > 0.02) {
            require(work >= kinetic_gain - 0.02,
                    "ordinary Tower acceleration is earned by measured contact work");
        }
        return after;
    };

    for (int tick = 0; tick < 20; ++tick) (void)sample(1.0);
    const auto east = run.snapshot();
    require(east.player_linear_velocity.x > 0.5 &&
                east.player_position.x > settled.player_position.x + 0.01 &&
                east.landing_recovery_work_j > settled.landing_recovery_work_j + 0.05,
            "ordinary eastward walking accelerates and advances through nonzero contact work");

    for (int tick = 0; tick < 30; ++tick) {
        const auto before = run.snapshot();
        const auto after = sample(0.0);
        require(horizontal_speed(after) <= horizontal_speed(before) + 0.001 &&
                    after.player_linear_velocity.x >= -0.01,
                "ordinary neutral braking dissipates motion without reversing it");
    }
    const auto stopped = run.snapshot();
    require(horizontal_speed(stopped) < 0.1,
            "ordinary neutral input settles through bounded braking");

    for (int tick = 0; tick < 20; ++tick) (void)sample(1.0);
    const auto before_reverse = run.snapshot();
    require(before_reverse.player_linear_velocity.x > 0.5,
            "ordinary reversal starts with actual eastward momentum");
    require(run.set_facing(-1.0, 0.0), "ordinary westward facing accepted");
    const auto first_reverse = sample(-1.0);
    require(first_reverse.player_linear_velocity.x > 0.0,
            "opposite input brakes earned momentum before reversing it");
    for (int tick = 1; tick < 40; ++tick) (void)sample(-1.0);
    const auto reversed = run.snapshot();
    require(reversed.player_linear_velocity.x < -0.5 &&
                reversed.landing_recovery_work_j > before_reverse.landing_recovery_work_j + 0.05,
            "ordinary reversal earns westward motion through nonzero contact work");
    std::cout << "INFO ordinary Tower force east_vx=" << east.player_linear_velocity.x
              << " east_work=" << east.landing_recovery_work_j - settled.landing_recovery_work_j
              << " neutral_speed=" << horizontal_speed(stopped)
              << " reversed_vx=" << reversed.player_linear_velocity.x
              << " total_work=" << reversed.landing_recovery_work_j - settled.landing_recovery_work_j
              << '\n';
}

void neutral_airborne_input_preserves_horizontal_momentum() {
    using namespace scraperx::sim;

    Simulation run(InitialSpawn::ExteriorGrade, WorldContent::GroundFoundation);
    require(run.set_move_input(0.0, 0.0), "neutral-air settle input accepted");
    require(run.advance_frame(1.0).accepted && run.snapshot().player_grounded,
            "ordinary ground settles before the neutral-air run");

    // Reach the same walking speed through finite traction before measuring
    // passive flight; preparation time is not the momentum acceptance gate.
    for (int tick = 0; tick < 90; ++tick) {
        require(run.set_move_input(1.0, 0.0), "eastward run input accepted");
        require(run.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "eastward ground tick accepted");
    }
    const auto approach = run.snapshot();
    const double approach_speed = horizontal_speed(approach);
    require(approach.player_grounded && approach_speed > 5.0 && approach_speed <= 5.55,
            "ordinary ground run reaches the existing 5.5 m/s walking speed");

    require(run.request_jump(), "physical jump request accepted");
    require(run.set_move_input(1.0, 0.0), "direction remains held through takeoff");
    require(run.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "directional physical takeoff tick accepted");
    const auto takeoff = run.snapshot();
    require(!takeoff.player_grounded && takeoff.support_entity_id == 0 &&
                takeoff.traversal_state == TraversalState::None &&
                takeoff.traversal_hand_constraint_count == 0 &&
                takeoff.player_linear_velocity.y > 5.0,
            "ordinary jump is airborne without support or traversal");
    const double takeoff_speed = horizontal_speed(takeoff);

    require(run.set_move_input(0.0, 0.0), "neutral airborne input accepted");
    for (int tick = 0; tick < 18; ++tick) {
        require(run.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "neutral airborne tick accepted");
        const auto state = run.snapshot();
        require(!state.player_grounded && state.support_entity_id == 0 &&
                    state.traversal_state == TraversalState::None &&
                    state.traversal_hand_constraint_count == 0,
                "neutral-air sample remains airborne without contact or traversal");
    }

    const auto after_neutral = run.snapshot();
    const double neutral_speed = horizontal_speed(after_neutral);
    std::cout << "INFO neutral-air momentum approach=" << approach_speed
              << " takeoff=" << takeoff_speed << " after_18_ticks=" << neutral_speed
              << " delta=" << neutral_speed - takeoff_speed << " y="
              << after_neutral.player_position.y << " vy="
              << after_neutral.player_linear_velocity.y << '\n';
    require(neutral_speed >= takeoff_speed - 0.25,
            "neutral airborne input preserves horizontal momentum without contact or forces");
}

void sprint_keeps_jump_momentum() {
    using scraperx::sim::Simulation;
    using scraperx::sim::InitialSpawn;
    Simulation sprint(InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::GroundFoundation);
    require(sprint.advance_frame(1.0).accepted, "the sprint settle interval must be accepted");
    // Runs along the deck, east (+1) or west (-1).
    const auto run_east = [&](const double seconds, double &peak, bool &flag,
                              const double way = 1.0) {
        const auto ticks = static_cast<std::uint32_t>(seconds * Simulation::kTickRateHz);
        for (std::uint32_t tick = 0; tick < ticks; ++tick) {
            (void)sprint.set_move_input(way, 0.0);
            (void)sprint.set_facing(way, 0.0);
            (void)sprint.advance_frame(Simulation::kFixedStepSeconds);
            peak = std::max(peak, horizontal_speed(sprint.snapshot()));
            flag = flag || sprint.snapshot().player_sprinting;
        }
    };
    // A jump east at whatever speed the body has, stick held: its distance
    // and its slowest speed in the air.
    const auto jump_east = [&](double &distance, double &air_slowest, const double way = 1.0) {
        const double x0 = sprint.snapshot().player_position.x;
        (void)sprint.request_jump();
        air_slowest = std::numeric_limits<double>::infinity();
        for (std::uint32_t tick = 0; tick < 2U * Simulation::kTickRateHz; ++tick) {
            (void)sprint.set_move_input(way, 0.0);
            (void)sprint.set_facing(way, 0.0);
            (void)sprint.advance_frame(Simulation::kFixedStepSeconds);
            const auto state = sprint.snapshot();
            if (!state.player_grounded) {
                air_slowest = std::min(air_slowest, horizontal_speed(state));
            } else if (tick > 10U) {
                break;
            }
        }
        distance = (sprint.snapshot().player_position.x - x0) * way;
    };
    (void)sprint.set_sprint_input(true);
    double sprint_peak = 0.0;
    bool sprint_flag = false;
    run_east(2.0, sprint_peak, sprint_flag);
    double sprint_jump = 0.0;
    double sprint_air_slowest = 0.0;
    jump_east(sprint_jump, sprint_air_slowest);
    (void)sprint.set_sprint_input(false);
    // Walking back west: settled to a walk, then measured.
    double settle_peak = 0.0;
    bool settle_flag = false;
    run_east(0.6, settle_peak, settle_flag, -1.0);
    double walk_peak = 0.0;
    bool walk_flag = false;
    run_east(1.0, walk_peak, walk_flag, -1.0);
    double walk_jump = 0.0;
    double walk_air_slowest = 0.0;
    jump_east(walk_jump, walk_air_slowest, -1.0);
    require(sprint_flag && sprint_peak >= 7.9 && sprint_peak <= 8.05,
            "sprinting, the body must run at 8 m/s");
    require(sprint_air_slowest >= 7.9, "a sprinting jump must keep its speed in the air");
    require(sprint_jump >= walk_jump + 2.0,
            "a sprinting jump must carry at least 2 m past a walking one");
    require(!walk_flag && walk_peak <= 5.55, "without sprint held the body walks at 5.5 m/s");
    (void)sprint.set_sprint_input(true);
    (void)sprint.set_crouch_input(true);
    (void)sprint.advance_frame(0.3);
    double crouched_peak = 0.0;
    bool crouched_flag = false;
    for (std::uint32_t tick = 0; tick < 135U; ++tick) {
        (void)sprint.set_move_input(-1.0, 0.0);
        (void)sprint.set_facing(-1.0, 0.0);
        (void)sprint.advance_frame(Simulation::kFixedStepSeconds);
        if (tick > 45U) {
            crouched_peak = std::max(crouched_peak, horizontal_speed(sprint.snapshot()));
        }
        crouched_flag = crouched_flag || sprint.snapshot().player_sprinting;
    }
    (void)sprint.set_crouch_input(false);
    (void)sprint.set_move_input(0.0, 0.0);
    (void)sprint.advance_frame(0.5);
    double across_peak = 0.0;
    bool across_flag = false;
    for (std::uint32_t tick = 0; tick < 135U; ++tick) {
        (void)sprint.set_move_input(-1.0, 0.0);
        (void)sprint.set_facing(0.0, 1.0);
        (void)sprint.advance_frame(Simulation::kFixedStepSeconds);
        across_peak = std::max(across_peak, horizontal_speed(sprint.snapshot()));
        across_flag = across_flag || sprint.snapshot().player_sprinting;
    }
    (void)sprint.set_move_input(0.0, 0.0);
    require(!crouched_flag && crouched_peak <= 2.6, "crouched, sprint held must not sprint");
    require(!across_flag && across_peak <= 5.55,
            "with the stick across the facing, sprint held must not sprint");
    std::cout << "PASS scraperx_sim step2 sprint: peak=" << sprint_peak
              << " air_slowest=" << sprint_air_slowest << " sprint_jump_m=" << sprint_jump
              << " walk_jump_m=" << walk_jump << " crouched_peak=" << crouched_peak
              << " across_peak=" << across_peak << '\n';

}

} // namespace

int main() {
    ordinary_jump_intent_uses_native_contact_and_clock();
    ordinary_tower_walk_uses_finite_contact_work();
    vault_carries_entry_and_exit_speed();
    neutral_airborne_input_preserves_horizontal_momentum();
    sprint_keeps_jump_momentum();
    rendered_pose_fills_between_fixed_ticks();
    std::cout << "PASS scraperx_sim parkour flow\n";
    return EXIT_SUCCESS;
}
