#include "intake_geometry.hpp"
#include "test_suites.hpp"
#include "test_utilities.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

namespace scraperx::tests {

void run_legal_forty_tests() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;

    // ---- AS-002: Legal Forty, first legal stand at +40 m -----------------
    // Atlas band B00's 24-40 m leftover, chain K0 PLAY. The claim under test
    // is the same shape as AS-001's own: MOD-STAIR-A-SWING opens because
    // real rope tension -- read back from the solver, off a real 4 t pack
    // physically seated in MOD-CW-CRADLE -- beats the flight's own stowed-
    // stop demand, not because a flag flipped. Geometry literals below are
    // read off the kLegalForty*/kIntake* constants in simulation.cpp.
    constexpr double kLegalFortyHandoffSurfaceY = 24.1872;
    constexpr double kLegalFortyMidLandingSurfaceY = 32.1872;
    // Centre of the hall's west strip, clear of the well opening at x=-4.8.
    constexpr double kLegalFortyHallArrivalX = -7.4;
    constexpr double kLegalFortyTravelFull = 0.907572; // stowed 8 deg .. deployed 60 deg

    // 1. wo015_unloaded_flight_is_not_a_route: the stowed flight hangs up
    //    near the hinge, nowhere over the deck it will occupy once deployed
    //    -- walking and mantling at that empty footprint gets nowhere.
    Simulation unrouted(InitialSpawn::IntakeHandoffDeck, scraperx::sim::WorldContent::RegressionFixtures);
    require(unrouted.advance_frame(0.5).accepted, "unrouted settling interval must be accepted");
    require(nearly_equal(unrouted.snapshot().legal_forty_swing_travel_radians, 0.0, 1.0e-3),
            "the swing flight must start at its stowed limit");
    double unrouted_deepest_y = unrouted.snapshot().player_position.y;
    bool unrouted_reached_flight = false;
    for (std::uint32_t tick = 0; tick < 90 * 14; ++tick) {
        const auto state = unrouted.snapshot();
        unrouted_deepest_y = std::max(unrouted_deepest_y, state.player_position.y);
        if (state.support_entity_id == Simulation::kIntakeSwingFlightEntityId) {
            unrouted_reached_flight = true;
        }
        (void)unrouted.set_move_input(-1.0, 0.0);
        (void)unrouted.set_facing(-1.0, 0.0);
        const bool ready = (state.traversal_state == TraversalState::None &&
                            state.player_grounded && state.ledge_available) ||
                           state.traversal_state == TraversalState::Hanging;
        if (ready) {
            (void)unrouted.request_traversal();
        }
        (void)unrouted.advance_frame(Simulation::kFixedStepSeconds);
    }
    const auto unrouted_state = unrouted.snapshot();
    require(unrouted_deepest_y < kLegalFortyHandoffSurfaceY + 1.0,
            "the unloaded flight must not be a route -- the deepest y reached at its deployed "
            "footprint must stay at the deck's own height, not the mid-landing's");
    require(!unrouted_reached_flight,
            "support_entity_id must never be the swing flight while it is stowed and unreachable");
    require(nearly_equal(unrouted_state.legal_forty_swing_travel_radians, 0.0, 1.0e-3),
            "walking and mantling at the stowed flight must not travel its hinge");

    // 2. wo015_flag_is_not_a_stair: every command mashed at once, off
    //    station, for the same interval -- no command surface substitutes
    //    for the physical rope tension the hinge actually needs.
    Simulation mashed(InitialSpawn::IntakeHandoffDeck, scraperx::sim::WorldContent::RegressionFixtures);
    for (std::uint32_t tick = 0; tick < 90 * 14; ++tick) {
        (void)mashed.set_intake_hoist_input(-1.0);
        (void)mashed.set_intake_slew_input(1.0);
        (void)mashed.request_intake_sling_release();
        (void)mashed.request_intake_sling_attach();
        (void)mashed.request_jump();
        (void)mashed.request_traversal();
        (void)mashed.set_move_input(-1.0, 0.0);
        (void)mashed.set_facing(-1.0, 0.0);
        (void)mashed.advance_frame(Simulation::kFixedStepSeconds);
    }
    const auto mashed_state = mashed.snapshot();
    require(!mashed_state.intake_station_active,
            "the B00 pendant must stay inert while the player is at the handoff deck, off "
            "station");
    require(nearly_equal(mashed_state.legal_forty_swing_travel_radians, 0.0, 1.0e-3),
            "no command issued off-station may travel the swing hinge");
    require(mashed_state.legal_forty_pack_slung,
            "no command issued off-station may release the sling");

    // 3. wo015_cradle_load_deploys, wo015_deployed_flight_is_support,
    //    wo015_reaches_forty_and_commits: the real causal path, start to
    //    finish -- lift, slew to the cradle bearing, load it, and walk the
    //    route that opens all the way to the hall deck.
    Simulation ascent(InitialSpawn::IntakePendant, scraperx::sim::WorldContent::RegressionFixtures);
    require(ascent.advance_frame(0.5).accepted, "ascent settling interval must be accepted");
    require(ascent.snapshot().intake_station_active,
            "the ascent spawn must be inside CAP-PENDANT's station radius");
    // Matches the verified station sequence exactly: the hoist stays
    // asserted (the winch actively holding the pack up, not a free hang)
    // all the way through the slew and settle, only reversing at Release --
    // found by direct observation that zeroing it straight after lift-clear
    // leaves enough residual swing for the pack to seat off-centre on the
    // cradle, and the mechanism that follows never recovers the margin.
    require(ascent.set_intake_hoist_input(1.0), "ascent hoist-clear command must be accepted");
    require(advance_until(ascent,
                          [](const auto &state) { return state.intake_pack_position.y > 8.0; },
                          20.0),
            "lifting the pack clear must be driven by the real winch, within budget");
    require(ascent.set_intake_slew_input(-1.0), "ascent slew command must be accepted");
    require(advance_until(
                ascent,
                [](const auto &state) { return state.intake_boom_angle_radians <= -0.80; }, 20.0),
            "slewing to the cradle bearing (negative input reaches the +X side the flight's own "
            "hinge shares) must complete within budget");
    require(ascent.set_intake_slew_input(0.0), "ascent slew-stop command must be accepted");
    require(ascent.advance_frame(6.0).accepted, "slew-settle interval must be accepted");
    const double cradle_y_before_load = ascent.snapshot().legal_forty_cradle_position.y;
    require(ascent.set_intake_hoist_input(-1.0), "ascent lower command must be accepted");
    bool ascent_released = false;
    for (std::uint32_t tick = 0; tick < 90 * 45 && !ascent_released; ++tick) {
        (void)ascent.request_intake_sling_release();
        (void)ascent.advance_frame(Simulation::kFixedStepSeconds);
        ascent_released = !ascent.snapshot().legal_forty_pack_slung;
    }
    require(ascent_released,
            "holding Release over the seated pack must free the sling within budget");
    for (int i = 0; i < 40 * 90; ++i) {
        (void)ascent.advance_frame(Simulation::kFixedStepSeconds);
        if (ascent.snapshot().legal_forty_swing_travel_radians >= 0.85) {
            break;
        }
    }
    require(ascent.snapshot().legal_forty_swing_travel_radians >= 0.85,
            "the loaded cradle's real rope tension must swing the hinge past 0.85 rad within "
            "the plan's own 40 s budget");
    const auto deployed = ascent.snapshot();
    require(cradle_y_before_load - deployed.legal_forty_cradle_position.y >= 1.40,
            "the cradle must physically descend at least 1.40 m once loaded");
    require(deployed.legal_forty_swing_travel_radians <= kLegalFortyTravelFull + 1.0e-3,
            "the hinge must never exceed its own deployed limit");
    // The loaded, deployed mechanism is at rest, and stays there: over two
    // full strokes of the B00 belt (2 * 2 pi / 0.40 s) nothing may strike the
    // car, knock the flight off its stop, or walk the seated pack. The plan's
    // 1.20 m-half-height car hung below the belt's top and was rammed once a
    // stroke until the pack fell off.
    {
        require(ascent.advance_frame(3.0).accepted, "deploy-arrival interval must be accepted");
        const auto seated = ascent.snapshot();
        double least_travel = std::numeric_limits<double>::infinity();
        double most_pack_drift = 0.0;
        for (std::uint32_t tick = 0; tick < 90 * 32; ++tick) {
            (void)ascent.advance_frame(Simulation::kFixedStepSeconds);
            const auto held = ascent.snapshot();
            least_travel = std::min(least_travel, held.legal_forty_swing_travel_radians);
            most_pack_drift = std::max(
                most_pack_drift, std::hypot(held.intake_pack_position.x - seated.intake_pack_position.x,
                                            held.intake_pack_position.z - seated.intake_pack_position.z));
        }
        require(least_travel >= 0.90,
                "the loaded flight must hold its deployed stop through two belt strokes");
        require(most_pack_drift < 0.05,
                "the seated pack must not walk on the car while the flight is deployed");
        std::cout << "INFO AS-002 deployed hold: least_travel=" << least_travel
                  << " pack_drift=" << most_pack_drift << '\n';
    }

    // Walk the route the deploy just opened: deck -> up the flight's own
    // incline to the hinge end -> across to the mid-landing -> up the
    // (now-inclined) upper flight -> through the well onto the hall deck.
    walk_toward(ascent, kThroatX, -107.5, 12.0, true);
    walk_toward(ascent, kThroatX, -113.0, 8.0, true);
    for (int flight = 0; flight < 6; ++flight) {
        const double side = (flight % 2 == 0) ? 1.0 : -1.0;
        const double lane = -118.0 + side * 2.0;
        walk_toward(ascent, -side * kStairLandingX, lane, 8.0, true);
        walk_toward(ascent, side * kStairLandingX, lane, 14.0, true);
    }
    // Lands west of the flight's own reach (it spans x in [-6, 7.856] once
    // deployed) and clear of its z in [-113.4, -111.6] -- clear of it on
    // both axes, unlike the freight braid's own final waypoint (-6, -112),
    // which the flight now physically occupies. Arrives and stops 1.3 m
    // inside the deck's north edge: a full stick dithering about (-9, -108),
    // 0.3 m inside it, walked the body off the deck -- a lethal 24 m fall
    // this run never checked for (found when AS-003 carried a load here).
    require(walk_to(ascent, -9.0, -109.0, 12.0),
            "the ascent must arrive on the +24 m handoff deck clear of its north edge");
    const auto on_deck = ascent.snapshot();
    require(on_deck.support_entity_id == Simulation::kIntakeHandoffEntityId,
            "the ascent must reach the +24 m handoff deck exactly as the freight braid does");
    // Board the deployed foot on its centreline. SKIN rung 17 overhangs
    // this approach: a standing capsule wedges against its underside at
    // x=-5.73 while the feet remain on the flight. Duck under the rung,
    // then stand only once its east face (-5.8) is behind the capsule.
    walk_toward(ascent, -6.0, -112.5, 25.0, true);
    (void)ascent.set_crouch_input(true);
    bool ascent_crouch_released = false;
    for (int i = 0; i < 20 * 90; ++i) {
        const auto state = ascent.snapshot();
        if (!ascent_crouch_released && state.player_position.x > -5.35) {
            (void)ascent.set_crouch_input(false);
            ascent_crouch_released = true;
        }
        double dx = kLegalFortyHingeX - state.player_position.x;
        double dz = kLegalFortyHingeZ - state.player_position.z;
        const double len = std::hypot(dx, dz);
        if (len > 1.0e-6) {
            dx /= len;
            dz /= len;
        }
        (void)ascent.set_move_input(dx, dz);
        (void)ascent.set_facing(dx, dz);
        (void)ascent.advance_frame(Simulation::kFixedStepSeconds);
    }
    // Crosses the hinge-to-landing gap at the hinge's own z first, staying
    // centred on the flight's own (narrow, 1.80 m) width the whole way --
    // found by direct observation that aiming straight at the landing's own
    // centre from the flight walks off its north edge diagonally, into the
    // gap, before reaching far enough east to be over the landing at all.
    walk_toward(ascent, kLegalFortyMidLandingX - 0.85, kLegalFortyHingeZ, 10.0, true);
    // Full-stick dithering at this narrow arrival can leave the body in a
    // small airborne bounce on the arbitrary final tick. Deliberately brake
    // to physical footing instead of relying on implicit airborne stopping.
    for (int i = 0; i < 10 * 90; ++i) {
        const auto state = ascent.snapshot();
        const double dx = kLegalFortyMidLandingX - state.player_position.x;
        const double dz = kLegalFortyMidLandingZ - state.player_position.z;
        if (state.player_grounded && std::hypot(dx, dz) < 0.06 &&
            horizontal_magnitude(state.player_linear_velocity) < 0.10) break;
        (void)ascent.set_move_input(
            std::clamp(dx * 1.8 - state.player_linear_velocity.x * 0.28, -1.0, 1.0),
            std::clamp(dz * 1.8 - state.player_linear_velocity.z * 0.28, -1.0, 1.0));
        if (std::hypot(dx, dz) > 0.0001) (void)ascent.set_facing(dx, dz);
        (void)ascent.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)ascent.set_move_input(0.0, 0.0);
    (void)ascent.advance_frame(30 * Simulation::kFixedStepSeconds);
    const auto mid_landing_state = ascent.snapshot();
    require(mid_landing_state.player_grounded &&
                mid_landing_state.player_position.y >= kLegalFortyMidLandingSurfaceY + 0.70,
            "the deployed flight must carry the player to the mid-landing at y >= 32.89");
    // Aligns to the upper flight's own (narrow, 1.80 m) z-band while still
    // on the wide mid-landing, for the same reason as the hinge crossing
    // above: the landing's z in [-121.0, -110.6] does not fully overlap
    // the flight's own z in [-117.9, -116.1], and a diagonal from the
    // landing's centre (z = -114.80) reaches the landing's west edge
    // before it reaches the flight's own band, and falls through the gap
    // between them -- found by direct observation.
    walk_toward(ascent, kLegalFortyMidLandingX, kLegalFortyUpperFlightZ, 10.0, true);
    // The rotated foot's rounded corner blocks the standing capsule on
    // the flat landing (contact normal y=0.531, below the support limit).
    // An ordinary Jump clears that lip; the flight then carries the feet.
    (void)ascent.request_jump();
    // First contact can still carry momentum back into the well. Walk to
    // the west strip's interior and brake to grounded, low-speed footing.
    const bool reached_hall_deck =
        walk_to(ascent, kLegalFortyHallArrivalX, kLegalFortyUpperFlightZ, 40.0, 0.15, true) &&
        ascent.snapshot().support_entity_id == Simulation::kIntakeHallDeckEntityId;
    require(reached_hall_deck,
            "the route must reach MOD-HALL-DECK itself, at the top of the upper flight, within "
            "budget");
    require(ascent.set_move_input(0.0, 0.0), "ascent settle-on-forty input must be accepted");
    require(ascent.advance_frame(2.0).accepted, "settle-on-forty interval must be accepted");
    const auto on_forty = ascent.snapshot();
    require(on_forty.support_entity_id == Simulation::kIntakeHallDeckEntityId,
            "the route must finish on MOD-HALL-DECK itself, at the top of the upper flight");
    require(on_forty.player_position.y >= kLegalFortyHallDeckSurfaceY + 0.70,
            "the hall deck must be stable support at y >= 40.89, not a graze");
    require(on_forty.death_count == 0,
            "the route from the apron to the hall deck must be walked without a lethal fall");
    require(on_forty.checkpoint_commit_count > 0 && on_forty.checkpoint_position.y >= 40.0,
            "standing on the hall deck must commit a checkpoint at y >= 40.0 through the "
            "existing automatic commit path, with no new machinery");

    // 4. wo015_unload_retracts: a fresh station-side instance -- the hinge
    //    relaxes back to its stowed limit once the cradle is unloaded again,
    //    reversible rather than a one-way flag.
    Simulation retract(InitialSpawn::IntakePendant, scraperx::sim::WorldContent::RegressionFixtures);
    require(retract.advance_frame(0.5).accepted, "retract settling interval must be accepted");
    require(retract.set_intake_hoist_input(1.0), "retract hoist-clear command must be accepted");
    require(advance_until(retract,
                          [](const auto &state) { return state.intake_pack_position.y > 8.0; },
                          20.0),
            "retract's own lift-clear must complete within budget");
    require(retract.set_intake_slew_input(-1.0), "retract slew command must be accepted");
    require(advance_until(
                retract,
                [](const auto &state) { return state.intake_boom_angle_radians <= -0.80; }, 20.0),
            "retract's own slew to bearing must complete within budget");
    require(retract.set_intake_slew_input(0.0), "retract slew-stop command must be accepted");
    require(retract.advance_frame(6.0).accepted, "retract slew-settle interval must be accepted");
    require(retract.set_intake_hoist_input(-1.0), "retract lower command must be accepted");
    bool retract_released = false;
    for (std::uint32_t tick = 0; tick < 90 * 45 && !retract_released; ++tick) {
        (void)retract.request_intake_sling_release();
        (void)retract.advance_frame(Simulation::kFixedStepSeconds);
        retract_released = !retract.snapshot().legal_forty_pack_slung;
    }
    require(retract_released, "retract's own release must free the sling within budget");
    require(advance_until(retract,
                          [](const auto &state) {
                              return state.legal_forty_swing_travel_radians >= 0.85;
                          },
                          40.0),
            "retract's own deploy must reach the same 0.85 rad within budget before unloading");
    // Lets the deploy actually settle at its own hinge limit before
    // unloading, rather than interrupting it mid-swing -- found by direct
    // observation that unslinging right as travel first crosses 0.85 (while
    // still accelerating toward the limit) leaves enough residual angular
    // velocity that the flight runs on to, and then sticks at, the
    // deployed stop regardless of the now-unloaded cradle.
    require(advance_until(retract,
                          [](const auto &state) {
                              return state.legal_forty_swing_travel_radians >= 0.90;
                          },
                          10.0),
            "retract's own deploy must settle past 0.90 rad before unloading");
    require(retract.advance_frame(20.0).accepted, "post-deploy settle interval must be accepted");
    bool reattached = false;
    for (std::uint32_t tick = 0; tick < 90 * 30 && !reattached; ++tick) {
        (void)retract.request_intake_sling_attach();
        (void)retract.advance_frame(Simulation::kFixedStepSeconds);
        reattached = retract.snapshot().legal_forty_pack_slung;
    }
    require(reattached, "holding Attach over the seated hook must re-sling the pack within "
                        "budget");
    require(retract.set_intake_hoist_input(1.0), "retract lift-clear-again command must be "
                                                  "accepted");
    for (int i = 0; i < 30 * 90; ++i) {
        (void)retract.advance_frame(Simulation::kFixedStepSeconds);
        if (retract.snapshot().intake_pack_position.y > 6.0) {
            break;
        }
    }
    require(retract.snapshot().intake_pack_position.y > 6.0,
            "lifting the pack clear of the cradle again must complete within budget");
    for (int i = 0; i < 30 * 90; ++i) {
        (void)retract.advance_frame(Simulation::kFixedStepSeconds);
        if (retract.snapshot().legal_forty_swing_travel_radians <= 0.20) {
            break;
        }
    }
    require(retract.snapshot().legal_forty_swing_travel_radians <= 0.20,
            "unloading the cradle must let the flight's own weight relax the hinge back to "
            "<= 0.20 rad, reversible rather than a one-way flag");

    // 5. wo015_skin_skips_the_freight: MOD-SKIN-LADDER-S is still the
    //    always-legal bypass -- now reaching the hall deck via the
    //    mid-landing walkway and the (always-present) static upper flight,
    //    with the freight sequence and the swing hinge both untouched.
    Simulation skin_forty(InitialSpawn::IntakeSkinFoot, scraperx::sim::WorldContent::RegressionFixtures);
    require(skin_forty.advance_frame(0.5).accepted, "skin-forty settling interval must be "
                                                     "accepted");
    // IntakeSkinFoot spawns at the ladder's own base, so this climbs the
    // WHOLE ladder -- all 20 rungs, not just this continuation's 5 (16-20)
    // -- one mantle apiece, exactly like B00's own skin_mantles=15 climb of
    // rungs 1-15 alone above. Found by direct observation: counting only 5
    // stopped this loop at rung 5 (y ~= 7.3, near the ladder's own lower-
    // middle section), and the walk_toward calls meant for the TOP of the
    // ladder then steered the player sideways off it into open air there.
    //
    // Unlike the B00 climb, whose target (the +24 m handoff deck) sits at
    // the rungs' own x = kIntakeSkinCenterX = kIntakeHandoffCenterX, the
    // walkway this continuation lands on is offset east of the rung column
    // (x in [-5.0, 8.30] against the rungs' own x in [-7.0, -5.0]) -- so
    // climbing stops the instant all 20 rungs are mantled, by count, rather
    // than waiting on a support match that continued (0, -1) steering can
    // never produce.
    constexpr std::uint32_t kSkinFortyRungCount = 20;
    std::uint32_t skin_forty_mantles = 0;
    for (std::uint32_t tick = 0; tick < 90 * 150 && skin_forty_mantles < kSkinFortyRungCount;
         ++tick) {
        const auto state = skin_forty.snapshot();
        (void)skin_forty.set_move_input(0.0, -1.0);
        (void)skin_forty.set_facing(0.0, -1.0);
        const bool ready = (state.traversal_state == TraversalState::None &&
                            state.player_grounded && state.ledge_available) ||
                           state.traversal_state == TraversalState::Hanging;
        if (ready && skin_forty.request_traversal()) {
            ++skin_forty_mantles;
        }
        (void)skin_forty.advance_frame(Simulation::kFixedStepSeconds);
    }
    require(skin_forty_mantles == kSkinFortyRungCount,
            "MOD-SKIN-LADDER-S's continuation (rungs 16-20) must climb to the mid-landing's "
            "own height with the same rung law as rungs 1-15");
    // The 20th mantle's own rise is still in progress the instant the loop
    // above exits (it only just counted, on this same tick) -- a fixed 0.5 s
    // settle was not long enough to wait it out: found by direct
    // observation, the very next diagnostic checkpoint already showing the
    // player mid-fall (grounded=0) well short of the walkway. Wait for the
    // traversal to actually finish instead.
    bool skin_forty_settled = false;
    for (int i = 0; i < 5 * 90 && !skin_forty_settled; ++i) {
        (void)skin_forty.set_move_input(0.0, 0.0);
        (void)skin_forty.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = skin_forty.snapshot();
        skin_forty_settled =
            state.traversal_state == TraversalState::None && state.player_grounded;
    }
    require(skin_forty_settled,
            "the 20th mantle must finish and settle before the walkway crossing begins");
    // The walkway's lane with standing headroom is z in [-120.65, -118.25]
    // for the capsule's centre: the walkway's own south edge (-121.0) and
    // the upper flight's own south edge (-117.9), each less the 0.35 m
    // radius. North of it the flight's underside falls toward its foot at
    // x = 9.350 and leaves under 2.1 m of headroom past x = 5.7. Rung 20's
    // band alone, the walkway's old footprint, left [-118.65, -118.25]: a
    // 0.4 m lane, reported by a player as an opening too small to fit
    // through. Zigzag from one side of the lane to the other through the
    // stretch where the flight is lowest, on foot: every waypoint reached,
    // never below the walkway's own top, never by a mantle or a vault.
    // Against the old footprint the first southern waypoint is past the
    // walkway's edge. Ends at x = 9.8, east of the flight's own foot
    // (9.350) -- past that x the flight has no footprint left to overhang,
    // at any z -- so the final approach below can safely turn north.
    const double walkway_lane[][2] = {
        {0.0, -120.3}, {4.5, -118.6}, {6.5, -120.3}, {8.0, -118.6}, {9.8, -120.3}};
    double walkway_lowest_y = skin_forty.snapshot().player_position.y;
    bool walkway_traversed = false;
    for (const auto &waypoint : walkway_lane) {
        bool reached = false;
        for (int i = 0; i < 12 * 90 && !reached; ++i) {
            const auto state = skin_forty.snapshot();
            double dx = waypoint[0] - state.player_position.x;
            double dz = waypoint[1] - state.player_position.z;
            const double len = std::hypot(dx, dz);
            // Brake before changing direction: full southward speed at
            // the third waypoint runs off the -121 m edge while finite
            // traction is still slowing the body for the northward turn.
            const double vx = state.player_linear_velocity.x -
                              state.support_point_linear_velocity.x;
            const double vz = state.player_linear_velocity.z -
                              state.support_point_linear_velocity.z;
            reached = len < 0.3 && state.player_grounded && std::hypot(vx, vz) < 0.10;
            const double input_x = dx * 1.8 - vx * 0.28;
            const double input_z = dz * 1.8 - vz * 0.28;
            const double scale = std::max(1.0, std::hypot(input_x, input_z));
            if (len > 1.0e-6) {
                dx /= len;
                dz /= len;
            }
            (void)skin_forty.set_move_input(input_x / scale, input_z / scale);
            (void)skin_forty.set_facing(dx, dz);
            (void)skin_forty.advance_frame(Simulation::kFixedStepSeconds);
            const auto after = skin_forty.snapshot();
            walkway_lowest_y = std::min(walkway_lowest_y, after.player_position.y);
            walkway_traversed =
                walkway_traversed || after.traversal_state != TraversalState::None;
        }
        require(reached, "every point of the walkway's lane under the upper flight must be "
                         "reachable on foot");
    }
    std::cout << "INFO AS-002 walkway lane: lowest_y=" << walkway_lowest_y << '\n';
    require(walkway_lowest_y > kLegalFortyMidLandingSurfaceY + 0.70,
            "crossing the walkway's lane must never drop below the walkway's own top");
    require(!walkway_traversed, "the walkway's lane is walked, not mantled or vaulted");
    // From here to the landing's own centre, x only decreases from 9.8 to
    // 9.350 -- never west of the flight's own foot -- while z climbs clear
    // of its band, so this straight line never passes under it.
    walk_toward(skin_forty, kLegalFortyMidLandingX, kLegalFortyMidLandingZ, 15.0, true);
    // Same two-step crossing the ascent route above needed at this identical
    // boundary: the landing's z in [-121.0, -110.6] does not fully overlap
    // the upper flight's own, narrower z in [-117.9, -116.1], so align to
    // the flight's own z-band first, while still on the wide landing, then
    // cross west along that band -- a direct diagonal from the landing's
    // own centre falls through the gap between them instead.
    walk_toward(skin_forty, kLegalFortyMidLandingX, kLegalFortyUpperFlightZ, 10.0, true);
    // Same rotated foot and real landing as the deployed-flight route.
    (void)skin_forty.request_jump();
    const bool skin_forty_reached_hall_deck =
        walk_to(skin_forty, kLegalFortyHallArrivalX, kLegalFortyUpperFlightZ,
                40.0, 0.15, true) &&
        skin_forty.snapshot().support_entity_id == Simulation::kIntakeHallDeckEntityId;
    require(skin_forty_reached_hall_deck,
            "SKIN's own bypass must reach MOD-HALL-DECK itself within budget");
    require(skin_forty.set_move_input(0.0, 0.0), "skin-forty settle input must be accepted");
    require(skin_forty.advance_frame(2.0).accepted, "skin-forty settle interval must be accepted");
    const auto skin_forty_state = skin_forty.snapshot();
    require(skin_forty_state.support_entity_id == Simulation::kIntakeHallDeckEntityId,
            "SKIN's own bypass must still finish on MOD-HALL-DECK itself");
    require(skin_forty_state.intake_pack_pins_dog && skin_forty_state.legal_forty_pack_slung,
            "climbing SKIN to +40 m must not falsely mutate either mechanism -- the pack is "
            "still down at the dog and still slung at the hook");
    require(skin_forty_state.legal_forty_swing_travel_radians <= 0.20,
            "climbing SKIN must never travel the swing hinge -- the walkway and the upper "
            "flight are the route, not the bascule");

    // 6. wo015_jib_cannot_reach_forty: the rated winch's own known ceiling
    //    is unmoved by anything AS-002 added -- this is Design Values
    //    8.4.5's arithmetic made executable, so the gap cannot be quietly
    //    closed later by a boom change.
    Simulation capped(InitialSpawn::IntakePendant, scraperx::sim::WorldContent::RegressionFixtures);
    require(capped.advance_frame(0.5).accepted, "capped settling interval must be accepted");
    double capped_deepest_hook_y = capped.snapshot().intake_hook_position.y;
    for (std::uint32_t tick = 0; tick < 90 * 60; ++tick) {
        (void)capped.set_intake_hoist_input(1.0);
        (void)capped.advance_frame(Simulation::kFixedStepSeconds);
        capped_deepest_hook_y = std::max(capped_deepest_hook_y, capped.snapshot().intake_hook_position.y);
    }
    require(capped_deepest_hook_y < 12.0,
            "holding the intake hoist at its limit for 60 s must never put the hook within "
            "reach of +40 m -- every jib-driven body stays below y = 12.0");

    std::cout << "PASS scraperx_sim AS-002 Legal Forty: unrouted_deepest_y=" << unrouted_deepest_y
              << " flag_hinge=" << mashed_state.legal_forty_swing_travel_radians
              << " cradle_drop=" << (cradle_y_before_load - deployed.legal_forty_cradle_position.y)
              << " deployed_travel=" << deployed.legal_forty_swing_travel_radians
              << " mid_landing_y=" << mid_landing_state.player_position.y
              << " forty_y=" << on_forty.player_position.y
              << " checkpoint_commits=" << on_forty.checkpoint_commit_count
              << " checkpoint_y=" << on_forty.checkpoint_position.y
              << " retracted_travel=" << retract.snapshot().legal_forty_swing_travel_radians
              << " skin_forty_mantles=" << skin_forty_mantles
              << " skin_forty_y=" << skin_forty_state.player_position.y
              << " jib_capped_hook_y=" << capped_deepest_hook_y << '\n';
}

} // namespace scraperx::tests
