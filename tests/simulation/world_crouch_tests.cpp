#include "test_suites.hpp"
#include "test_utilities.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

namespace scraperx::tests {

void run_world_crouch_tests() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;

    // ---- World solids: every visible body is a body -----------------------
    // The dressing drawn by the Godot builders (world_solids.inc) is native
    // collision, not decoration. The buttress footings at the stack's corners
    // are the case a player walked straight through: a 6 x 2.6 x 6 m block
    // under a 2.4 m splayed leg. Walked into from the yard, the capsule must
    // stop at its face -- centre never nearer than half-width (3.0 m) plus
    // most of the capsule radius (0.35 m) on the dominant axis.
    Simulation solid(InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::RegressionFixtures);
    require(solid.advance_frame(0.5).accepted, "world-solids settling interval must be accepted");
    const auto solid_loaded = solid.snapshot();
    require(solid_loaded.world_solid_rejected == 0,
            "every generated hull must be a valid Jolt convex shape");
    require(solid_loaded.world_solid_bodies > 1400,
            "the generated world-solid table must load as native bodies");
    require(solid_loaded.world_solid_mirrors > 0,
            "drawn mirrors of owned bodies must be recognised and not doubled");
    constexpr double kFootX = -39.0;
    constexpr double kFootZ = -111.0;
    double foot_closest = 1.0e9;
    const auto track_foot = [&]() {
        const auto at = solid.snapshot().player_position;
        foot_closest = std::min(foot_closest,
                                std::max(std::abs(at.x - kFootX), std::abs(at.z - kFootZ)));
    };
    for (std::uint32_t tick = 0; tick < 90 * 14; ++tick) {
        walk_toward(solid, -30.0, -100.0, Simulation::kFixedStepSeconds);
        track_foot();
    }
    for (std::uint32_t tick = 0; tick < 90 * 4; ++tick) {
        walk_toward(solid, kFootX, kFootZ, Simulation::kFixedStepSeconds);
        track_foot();
    }
    require(foot_closest < 3.6, "the walk must actually reach the buttress footing");
    require(foot_closest > 3.25,
            "walking into a buttress footing must be stopped at its face by native collision");
    require(solid.snapshot().player_grounded,
            "the player stopped by the footing must still stand on the yard");

    // The rejected fallback is absent from both native ownership and generated
    // visible collision. The former first-flight lane remains grade-level and
    // walkable instead of becoming a hidden or visual-only ramp.
    Simulation former_stair(InitialSpawn::ExteriorGrade,
                            scraperx::sim::WorldContent::RegressionFixtures);
    require(former_stair.advance_frame(0.5).accepted,
            "former-stair settling interval must be accepted");
    require(former_stair.entity_body_count(Simulation::kTowerEntityId) == 169,
            "continuous stack rings and columns must own exactly 169 native bodies");
    walk_toward(former_stair, -25.0, -118.0, 20.0);
    walk_toward(former_stair, -25.0, -128.5, 6.0);
    walk_toward(former_stair, 21.0, -128.5, 16.0);
    const auto former_stair_lane = former_stair.snapshot();
    require(former_stair_lane.player_grounded && former_stair_lane.death_count == 0 &&
                former_stair_lane.player_position.x > 15.0 &&
                former_stair_lane.player_position.y < 2.0,
            "generated solids must not reintroduce the rejected first ramp");

    // The yard sits in a basin, not on a slab in a void: off the grade's
    // east edge the valley floor carries the player 0.3 m lower, the step
    // back up is walked, and walking out to the rim ends against rock --
    // grounded, low, and never past the ridge line.
    Simulation basin(InitialSpawn::ExteriorGrade, scraperx::sim::WorldContent::RegressionFixtures);
    require(basin.advance_frame(0.5).accepted, "basin settling interval must be accepted");
    walk_toward(basin, 270.0, -60.0, 70.0);
    const auto on_valley = basin.snapshot();
    require(on_valley.player_grounded && on_valley.player_position.x > 250.0 &&
                std::abs(on_valley.player_position.y - (-0.3 + 0.9)) < 0.1,
            "walking off the grade must land on the valley floor, not fall");
    walk_toward(basin, 200.0, -60.0, 20.0);
    const auto back_on_grade = basin.snapshot();
    require(back_on_grade.player_grounded && back_on_grade.player_position.x < 230.0 &&
                back_on_grade.player_position.y > 0.8,
            "the 0.3 m step from the valley floor back onto the grade must be walked");
    walk_toward(basin, 200.0, 1400.0, 320.0);
    const auto at_rim = basin.snapshot();
    const double rim_reach = std::hypot(at_rim.player_position.x - 0.0,
                                        at_rim.player_position.z - (-100.0));
    require(at_rim.player_grounded && at_rim.player_position.y < 5.0 && rim_reach < 1100.0,
            "walking out to the basin rim must end against its rock, low and grounded");

    std::cout << "PASS scraperx_sim world solids: bodies=" << solid_loaded.world_solid_bodies
              << " mirrors=" << solid_loaded.world_solid_mirrors
              << " rejected=" << solid_loaded.world_solid_rejected
              << " buttress_foot_closest=" << foot_closest
              << " tower_bodies=" << former_stair.entity_body_count(Simulation::kTowerEntityId)
              << " former_flight_y=" << former_stair_lane.player_position.y
              << " rim_reach=" << rim_reach << '\n';

    // Crouch (GDD 7.2, Governing Law 4). The crawl beam beside the traversal
    // fixtures has its underside 1.45 m over the deck: under the standing
    // capsule's 1.8 m, over the crouched one's 1.2 m. Standing, the beam
    // stops the body at its face; crouched, it walks under; beneath it,
    // letting go of crouch does not stand it up, and neither Jump nor Action
    // does anything; walked out past it, it stands up by itself.
    constexpr double kCrawlLaneX = -2.0;
    constexpr double kCrawlBeamZ = -13.0;
    constexpr double kCrawlBeamHalfZ = 0.3;
    constexpr double kCrouchSpeedLimit = 5.5 * 0.45;
    Simulation crouch(InitialSpawn::StaticDeck, scraperx::sim::WorldContent::RegressionFixtures);
    require(crouch.advance_frame(1.0).accepted, "crouch settling interval must be accepted");
    require(crouch.snapshot().player_grounded && !crouch.snapshot().player_crouched,
            "the crouch run must start standing on the deck");
    const double standing_deepest = walk_toward(crouch, kCrawlLaneX, kCrawlBeamZ - 2.0, 5.0);
    require(standing_deepest > kCrawlBeamZ + kCrawlBeamHalfZ,
            "a standing capsule must not get under a beam 1.45 m over the deck");
    walk_toward(crouch, kCrawlLaneX, kCrawlBeamZ + 3.0, 2.0);
    require(crouch.set_move_input(0.0, 0.0), "crouch stop input must be accepted");
    require(crouch.advance_frame(0.5).accepted, "crouch standing settle must be accepted");
    const auto crouch_standing = crouch.snapshot();
    require(crouch_standing.player_grounded && !crouch_standing.player_crouched,
            "backing off the beam must leave the body standing");

    require(crouch.set_crouch_input(true), "crouch input must be accepted");
    require(crouch.advance_frame(0.3).accepted, "crouch interval must be accepted");
    const auto crouched = crouch.snapshot();
    const double crouch_drop = crouch_standing.player_position.y - crouched.player_position.y;
    require(crouched.player_crouched && crouched.player_grounded,
            "crouch held on the ground must crouch the body");
    require(std::abs(crouch_drop - 0.3) < 0.03,
            "crouching must lower the centre by 0.3 m and leave the feet where they were");

    double crouched_top_speed = 0.0;
    for (int i = 0; i < 4 * 90; ++i) {
        const auto state = crouch.snapshot();
        double dx = kCrawlLaneX - state.player_position.x;
        double dz = kCrawlBeamZ - state.player_position.z;
        const double len = std::hypot(dx, dz);
        if (len < 0.05) {
            break;
        }
        // Stop under the beam rather than carrying full crouch speed
        // past it during the neutral interval's finite braking stroke.
        const double input_x = dx * 1.8 -
            (state.player_linear_velocity.x - state.support_point_linear_velocity.x) * 0.28;
        const double input_z = dz * 1.8 -
            (state.player_linear_velocity.z - state.support_point_linear_velocity.z) * 0.28;
        const double scale = std::max(1.0, std::hypot(input_x, input_z));
        (void)crouch.set_move_input(input_x / scale, input_z / scale);
        (void)crouch.set_facing(dx / len, dz / len);
        (void)crouch.advance_frame(Simulation::kFixedStepSeconds);
        crouched_top_speed = std::max(crouched_top_speed,
                                      horizontal_magnitude(crouch.snapshot().player_linear_velocity));
    }
    require(crouch.set_move_input(0.0, 0.0), "crouch stop input must be accepted");
    require(crouch.advance_frame(0.3).accepted, "crouch under-beam settle must be accepted");
    const auto under = crouch.snapshot();
    require(std::abs(under.player_position.z - kCrawlBeamZ) < 0.3 && under.player_crouched &&
                under.player_grounded,
            "a crouched capsule must walk in under the beam and stand on the deck there");
    require(crouched_top_speed > 2.0 && crouched_top_speed < kCrouchSpeedLimit + 0.05,
            "crouched walking must run at the crouch speed, not the standing one");

    require(crouch.set_crouch_input(false), "crouch release must be accepted");
    require(crouch.advance_frame(0.5).accepted, "crouch release interval must be accepted");
    require(crouch.snapshot().player_crouched,
            "under the beam, letting go of crouch must not stand the body up into it");
    const double under_y = crouch.snapshot().player_position.y;
    const auto rejected_before = crouch.snapshot().rejected_traversal_count;
    require(crouch.request_jump(), "a jump request under the beam must be queued");
    require(crouch.advance_frame(0.3).accepted, "under-beam jump interval must be accepted");
    const auto after_jump = crouch.snapshot();
    require(after_jump.player_crouched && after_jump.player_grounded &&
                after_jump.player_position.y < under_y + 0.05,
            "with no room to stand there is no jump");
    require(crouch.request_traversal(), "a traversal request under the beam must be queued");
    require(crouch.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "under-beam traversal tick must advance");
    require(crouch.snapshot().rejected_traversal_count == rejected_before + 1 &&
                crouch.snapshot().traversal_state == TraversalState::None,
            "a traversal request with no room to stand must be refused, and counted");

    walk_toward(crouch, kCrawlLaneX, kCrawlBeamZ - 2.0, 3.0);
    require(crouch.set_move_input(0.0, 0.0), "crouch exit stop input must be accepted");
    require(crouch.advance_frame(0.5).accepted, "crouch exit settle must be accepted");
    const auto beyond = crouch.snapshot();
    require(beyond.player_position.z < kCrawlBeamZ - kCrawlBeamHalfZ - 0.35,
            "the released-crouch body must still walk on out from under the beam");
    require(!beyond.player_crouched && beyond.player_grounded &&
                std::abs(beyond.player_position.y - crouch_standing.player_position.y) < 0.03,
            "clear of the beam, a released crouch must stand the body back up");

    std::cout << "PASS scraperx_sim crouch: standing_deepest_z=" << standing_deepest
              << " crouch_drop=" << crouch_drop << " crouched_top_speed=" << crouched_top_speed
              << " under_z=" << under.player_position.z
              << " stood_y=" << beyond.player_position.y << '\n';
}

} // namespace scraperx::tests
