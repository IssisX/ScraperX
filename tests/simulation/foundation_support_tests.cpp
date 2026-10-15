#include "test_suites.hpp"
#include "test_utilities.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

namespace scraperx::tests {

SupportReport run_foundation_support_tests() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;

    // The actual shipped default must contain no retired machinery, including
    // static frames or invisible colliders. Do not select a fixture spawn here.
    {
        Simulation foundation;
        for (std::uint64_t entity = 3; entity <= 59; ++entity) {
            if (entity == Simulation::kTowerEntityId || entity == Simulation::kWorldSolidEntityId) {
                continue;
            }
            require(foundation.entity_body_count(entity) == 0,
                    "default world must not construct a retired fixture or campaign body");
        }
        // AS-027 owns nine movers and three static bodies. Later encounters
        // append real bodies/cables without replacing this existing machine.
        for (std::uint64_t entity = 2970; entity <= 2978; ++entity)
            require(foundation.entity_body_count(entity) == 1,
                    "default world must retain every service-lift mover exactly once");
        for (std::uint64_t entity = 1970; entity <= 1972; ++entity)
            require(foundation.entity_body_count(entity) == 1,
                    "default world must retain every service-lift support exactly once");
        require(foundation.moving_body_count() >= 27 + 9,
                "default world must retain existing movers and include the nine service-lift movers");
        require(foundation.kit_body_count() >= 38 + 12 && foundation.kit_cable_count() >= 3,
                "default world must retain existing kit bodies and cables, plus twelve service-lift bodies");
        require(foundation.entity_body_count(Simulation::kTowerEntityId) > 1 &&
                    foundation.entity_body_count(Simulation::kWorldSolidEntityId) > 0,
                "the tower frame and environment must remain real collision geometry");
        (void)foundation.advance_frame(1.0);
        require(foundation.snapshot().player_grounded, "default player must settle at grade");
        const auto start = foundation.snapshot().player_position;
        (void)foundation.set_move_input(1.0, 0.0);
        (void)foundation.advance_frame(0.5);
        (void)foundation.set_move_input(0.0, 0.0);
        require(foundation.snapshot().player_position.x > start.x + 1.0,
                "normal locomotion must work without legacy machinery");
        (void)foundation.set_crouch_input(true);
        (void)foundation.advance_frame(0.2);
        require(foundation.snapshot().player_crouched, "default crouch must remain functional");
        (void)foundation.set_crouch_input(false);
        (void)foundation.advance_frame(0.2);
        (void)foundation.request_jump();
        (void)foundation.advance_frame(0.1);
        require(!foundation.snapshot().player_grounded && foundation.snapshot().player_linear_velocity.y > 2.0,
                "default jump must remain functional");
        (void)foundation.advance_frame(2.0);
        require(foundation.snapshot().player_grounded && foundation.snapshot().death_count == 0,
                "the default jump must land safely on grade");
        std::cout << "PASS scraperx_sim ground foundation: retired_bodies=0 moving_bodies="
                  << foundation.moving_body_count() << " kit_bodies=" << foundation.kit_body_count()
                  << " locomotion=1 crouch=1 jump=1\n";
    }

    {
        Simulation fallback;
        require(fallback.advance_frame(0.5).accepted,
                "rejected-fallback settling interval must be accepted");
        require(fallback.entity_body_count(Simulation::kTowerEntityId) == 385,
                "the default tower must contain continuous deck rings and columns only");
        walk_toward(fallback, -25.0, -118.0, 20.0);
        walk_toward(fallback, -25.0, -128.5, 6.0);
        walk_toward(fallback, 21.0, -128.5, 16.0);
        const auto former_flight = fallback.snapshot();
        require(former_flight.player_grounded && former_flight.death_count == 0,
                "walking the former first-flight lane must remain supported at grade");
        require(former_flight.player_position.x > 15.0,
                "the former first-flight lane must remain traversable after ramp removal");
        require(former_flight.player_position.y < 2.0,
                "the rejected first flight must not raise the player above grade");
        std::cout << "PASS scraperx_sim rejected fallback absent: tower_bodies="
                  << fallback.entity_body_count(Simulation::kTowerEntityId)
                  << " former_flight_y=" << former_flight.player_position.y
                  << " jump_requests=0\n";

        Simulation drop(InitialSpawn::HighDrop);
        const auto original_movers = drop.moving_body_count();
        const auto original_kit_bodies = drop.kit_body_count();
        (void)drop.advance_frame(8.0);
        require(drop.snapshot().death_count == 1 && drop.snapshot().player_grounded,
                "default fatal fall must restore safely without any legacy body queries");
        require(drop.moving_body_count() == original_movers && drop.kit_body_count() == original_kit_bodies,
                "checkpoint restore must retain the complete current machinery inventory");
        for (std::uint64_t entity = 2970; entity <= 2978; ++entity)
            require(drop.entity_body_count(entity) == 1,
                    "checkpoint restore must retain every service-lift mover exactly once");
        for (std::uint64_t entity = 3; entity <= 59; ++entity)
            if (entity != Simulation::kTowerEntityId && entity != Simulation::kWorldSolidEntityId)
                require(drop.entity_body_count(entity) == 0,
                        "checkpoint restore must not recreate retired machinery");
        std::cout << "PASS scraperx_sim default checkpoint: death_restore=1 retired_respawn=0\n";
    }

    Simulation partitioned(InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::RegressionFixtures);
    for (std::uint32_t i = 0; i < Simulation::kTickRateHz; ++i) {
        const auto result = partitioned.advance_frame(Simulation::kFixedStepSeconds);
        require(result.accepted, "fixed-step input must be accepted");
        require(result.steps_advanced == 1, "each exact fixed step must advance once");
    }

    const auto partitioned_snapshot = partitioned.snapshot();
    require(partitioned_snapshot.tick_index == 90, "90 fixed steps must produce tick 90");
    require(nearly_equal(partitioned_snapshot.simulation_time_seconds, 1.0),
            "tick-derived simulation time must equal one second");

    Simulation batched(InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::RegressionFixtures);
    const auto batched_result = batched.advance_frame(1.0);
    const auto batched_snapshot = batched.snapshot();
    require(batched_result.accepted, "one-second frame input must be accepted");
    require(batched_result.steps_advanced == 90, "one second must advance 90 authoritative ticks");
    require(batched_snapshot.tick_index == partitioned_snapshot.tick_index,
            "frame partitioning must not change tick count");
    require(nearly_equal(batched_snapshot.simulation_time_seconds,
                         partitioned_snapshot.simulation_time_seconds),
            "frame partitioning must not change simulation time");
    require(nearly_equal(batched_snapshot.player_position.x,
                         partitioned_snapshot.player_position.x,
                         1.0e-6) &&
                nearly_equal(batched_snapshot.player_position.y,
                             partitioned_snapshot.player_position.y,
                             1.0e-6) &&
                nearly_equal(batched_snapshot.player_position.z,
                             partitioned_snapshot.player_position.z,
                             1.0e-6),
            "frame partitioning must not change native player position");

    Simulation remainder(InitialSpawn::StaticDeck, scraperx::sim::WorldContent::RegressionFixtures);
    require(remainder.advance_frame(Simulation::kFixedStepSeconds * 0.5).steps_advanced == 0,
            "half a fixed step must remain buffered");
    require(remainder.advance_frame(Simulation::kFixedStepSeconds * 0.5).steps_advanced == 1,
            "two half steps must advance exactly once");
    require(nearly_equal(remainder.snapshot().interpolation_alpha, 0.0),
            "exactly consumed time must leave no interpolation remainder");

    const auto before_invalid = remainder.snapshot();
    require(!remainder.advance_frame(-0.001).accepted, "negative delta must be rejected");
    require(!remainder.advance_frame(std::numeric_limits<double>::quiet_NaN()).accepted,
            "NaN delta must be rejected");
    require(remainder.snapshot().tick_index == before_invalid.tick_index,
            "rejected frame input must not mutate authoritative state");

    Simulation supported(InitialSpawn::StaticDeck, scraperx::sim::WorldContent::RegressionFixtures);
    require(supported.advance_frame(2.0).accepted,
            "static-deck settling interval must be accepted");
    const auto supported_snapshot = supported.snapshot();
    require(supported_snapshot.player_grounded,
            "the native player capsule must be grounded after falling onto the static deck");
    require(supported_snapshot.support_entity_id == Simulation::kStaticDeckEntityId,
            "static support must expose the stable deck entity ID");
    require(nearly_equal(supported_snapshot.player_position.y, 0.9, 0.03),
            "the supported capsule center must settle at the static deck contact height");
    require(horizontal_magnitude(supported_snapshot.support_point_linear_velocity) < 1.0e-5,
            "the static deck support-point velocity must be zero");

    require(supported.set_move_input(1.0, 0.0),
            "finite desired movement input must be accepted");
    require(supported.advance_frame(0.5).accepted,
            "static-deck locomotion interval must be accepted");
    const auto moved_snapshot = supported.snapshot();
    require(moved_snapshot.player_position.x > supported_snapshot.player_position.x + 1.0,
            "desired relative velocity input must move the native body across the static deck");
    require(moved_snapshot.player_grounded,
            "horizontal locomotion must preserve static-deck support");
    require(moved_snapshot.support_entity_id == Simulation::kStaticDeckEntityId,
            "static locomotion support identity must remain native and stable");

    require(!supported.set_move_input(std::numeric_limits<double>::quiet_NaN(), 0.0),
            "non-finite desired movement input must be rejected");
    const auto before_rejected_command = supported.snapshot();
    require(supported.advance_frame(0.25).accepted,
            "simulation must remain usable after rejecting a movement command");
    const auto after_rejected_command = supported.snapshot();
    require(horizontal_distance(after_rejected_command.player_position,
                                before_rejected_command.player_position) > 0.5,
            "a rejected movement command must not replace the last accepted command");

    Simulation translating(InitialSpawn::TranslatingSupport, scraperx::sim::WorldContent::RegressionFixtures);
    require(translating.set_move_input(0.0, 0.0),
            "zero relative movement must be accepted on a translating support");
    require(translating.advance_frame(1.0).accepted,
            "translating-support settling interval must be accepted");
    const auto translating_grounded = translating.snapshot();
    require(translating_grounded.player_grounded,
            "the player must settle on the native translating support");
    require(translating_grounded.support_entity_id == Simulation::kTranslatingSupportEntityId,
            "translating support must expose its stable native entity ID");
    require(std::abs(translating_grounded.support_point_linear_velocity.x) > 0.5,
            "translating support must expose non-zero contact-point velocity");
    require(std::abs(translating_grounded.player_linear_velocity.x -
                     translating_grounded.support_point_linear_velocity.x) < 0.75,
            "zero-input grounded locomotion must remain relative to translating support velocity");

    const auto translating_support_velocity =
        translating_grounded.support_point_linear_velocity;
    require(translating.request_jump(), "grounded jump request must be accepted");
    require(translating.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "jump tick must advance");
    const auto translating_jump = translating.snapshot();
    require(!translating_jump.player_grounded,
            "jump must detach the player from translating support");
    require(translating_jump.player_linear_velocity.y > 4.0,
            "jump must create upward velocity without teleporting");
    require(translating_jump.player_linear_velocity.x * translating_support_velocity.x > 0.0,
            "jump must preserve the translating support velocity direction");
    require(std::abs(translating_jump.player_linear_velocity.x) >
                std::abs(translating_support_velocity.x) * 0.45,
            "jump must preserve a material fraction of inherited translating support velocity");

    require(translating.advance_frame(0.20).accepted,
            "airborne inherited-momentum interval must advance");
    const auto translating_airborne = translating.snapshot();
    require(!translating_airborne.player_grounded,
            "short post-jump interval must remain airborne");
    require(translating_airborne.player_linear_velocity.x * translating_support_velocity.x > 0.0,
            "air control must not immediately cancel inherited translating support momentum");

    Simulation rotating(InitialSpawn::RotatingSupport, scraperx::sim::WorldContent::RegressionFixtures);
    require(rotating.set_move_input(0.0, 0.0),
            "zero relative movement must be accepted on a rotating support");
    require(rotating.advance_frame(1.2).accepted,
            "rotating-support settling interval must be accepted");
    const auto rotating_grounded = rotating.snapshot();
    require(rotating_grounded.player_grounded,
            "the player must settle on the native rotating support");
    require(rotating_grounded.support_entity_id == Simulation::kRotatingSupportEntityId,
            "rotating support must expose its stable native entity ID");
    require(std::abs(rotating_grounded.rotating_support_angular_velocity.y) > 0.5,
            "rotating support must expose material native angular velocity");
    require(horizontal_magnitude(rotating_grounded.support_point_linear_velocity) > 0.5,
            "rotating support must produce non-zero support-point linear velocity away from its axis");

    const double radius_x =
        rotating_grounded.support_contact_point.x - rotating_grounded.rotating_support_position.x;
    const double radius_z =
        rotating_grounded.support_contact_point.z - rotating_grounded.rotating_support_position.z;
    const double omega_y = rotating_grounded.rotating_support_angular_velocity.y;
    const double expected_point_x = omega_y * radius_z;
    const double expected_point_z = -omega_y * radius_x;
    require(nearly_equal(rotating_grounded.support_point_linear_velocity.x,
                         expected_point_x,
                         0.12) &&
                nearly_equal(rotating_grounded.support_point_linear_velocity.z,
                             expected_point_z,
                             0.12),
            "rotating support point velocity must obey omega cross r at the actual contact point");
    require(horizontal_dot(rotating_grounded.player_linear_velocity,
                           rotating_grounded.support_point_linear_velocity) > 0.15,
            "rotating support motion must be imparted to the grounded player");

    return {
        translating_grounded.support_entity_id,
        translating_support_velocity.x,
        translating_jump.player_linear_velocity.x,
        rotating_grounded.support_entity_id,
        rotating_grounded.support_point_linear_velocity.x,
        rotating_grounded.support_point_linear_velocity.z,
        omega_y
    };
}

void print_support_report(const SupportReport &report) {
    using scraperx::sim::Simulation;
    std::cout << "PASS scraperx_sim moving-support truth: translating_support="
              << report.translating_support
              << " translating_vx=" << report.translating_vx
              << " jump_vx=" << report.jump_vx
              << " rotating_support=" << report.rotating_support
              << " rotating_point_v=("
              << report.rotating_point_vx << ','
              << report.rotating_point_vz << ')'
              << " omega_y=" << report.omega_y
              << " hz=" << Simulation::kTickRateHz << '\n';
}

} // namespace scraperx::tests
