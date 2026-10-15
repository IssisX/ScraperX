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

void run_intake_rise_tests() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;

    // ---- AS-001: B00 intake rise, apron to +24 m -------------------------
    // The first campaign slice. Atlas band B00, chain K0. The claim under test
    // is the causal path itself: MOD-STAIR-A is shut because 4 t of freight is
    // physically standing in MOD-DOG-A's swing, and it opens because the jib
    // moved that freight -- not because a predicate flipped.
    //
    // Geometry literals below are read off the kIntake* constants in
    // simulation.cpp. The bay's throat is at (0, -110.5); MOD-STAIR-A
    // switchbacks in two lanes at z = -116 and z = -120 between x = -7 and
    // x = +7, landings at x = +-8; the +24 m handoff deck is at x = -6,
    // z = -117.5 .. -107.7, walking surface 24.19 m.
    constexpr double kThroatZ = -110.5;
    // The bay's front wall spans z = -110.8 .. -110.2. A 0.35 m capsule has
    // therefore not cleared it until its centre passes -111.15, so anything
    // south of -111.2 is still outside the bay.
    constexpr double kBayInteriorZ = -111.2;
    // Player capsule standing half-height is 0.90 m, so a stable stand on the
    // 24.19 m walking surface puts the capsule centre near 25.09 m.

    // 1. Pinned: the throat is shut, and walking straight at it does not pass.
    Simulation pinned(InitialSpawn::IntakeThroat, scraperx::sim::WorldContent::RegressionFixtures);
    require(pinned.advance_frame(1.0).accepted, "intake settling interval must be accepted");
    const auto pinned_start = pinned.snapshot();
    require(pinned_start.intake_pack_pins_dog,
            "the 4 t pack must start inside MOD-DOG-A's swing envelope");
    require(!pinned_start.intake_throat_clear,
            "MOD-DOG-A must start blocking the MOD-STAIR-A throat");
    const double pinned_deepest = walk_toward(pinned, kThroatX, kThroatZ - 8.0, 14.0);
    const auto pinned_blocked = pinned.snapshot();
    require(pinned_deepest > kBayInteriorZ,
            "while the pack pins the dog, walking at the throat must not get the player past "
            "the bay wall at any point -- MOD-STAIR-A is physically impassable, not gated");
    require(pinned_blocked.player_position.y < 2.0,
            "a blocked player must still be at grade, not somehow up the stair");
    require(pinned.snapshot().intake_pack_pins_dog,
            "walking into the dog must not shift 4 t of freight");

    // 2. No command surface opens it. Every input the player has, off-station,
    //    hammered at once: none of them is a way past a physical body.
    Simulation forced(InitialSpawn::IntakeThroat, scraperx::sim::WorldContent::RegressionFixtures);
    double forced_deepest = 0.0;
    for (std::uint32_t tick = 0; tick < 1260; ++tick) {
        (void)forced.set_intake_hoist_input(1.0);
        (void)forced.set_intake_slew_input(1.0);
        (void)forced.set_jib_hoist_input(1.0);
        (void)forced.set_needle_hoist_input(1.0);
        (void)forced.request_valve_toggle();
        (void)forced.request_jump();
        (void)forced.request_traversal();
        (void)forced.set_move_input(0.0, -1.0);
        (void)forced.set_facing(0.0, -1.0);
        (void)forced.advance_frame(Simulation::kFixedStepSeconds);
        forced_deepest = std::min(forced_deepest, forced.snapshot().player_position.z);
    }
    const auto forced_state = forced.snapshot();
    require(!forced_state.intake_station_active,
            "the B00 pendant must stay inert while the player is at the throat, 10 m off station");
    require(forced_state.intake_pack_pins_dog && !forced_state.intake_throat_clear,
            "no command issued off-station may move the pack or travel the dog");
    require(forced_deepest > kBayInteriorZ,
            "mashing every input must not produce passage a body is blocking");

    // 3. The rating is real: 9 t under the same rated winch force never rises.
    Simulation rated(InitialSpawn::IntakePendant, scraperx::sim::WorldContent::RegressionFixtures);
    require(rated.advance_frame(20.0).accepted, "overweight interval must be accepted");
    const auto rated_state = rated.snapshot();
    require(rated_state.intake_overweight_pack_position.y < 1.2,
            "9 t exceeds the 49050 N rated winch force and must stall on the stand, however long "
            "it is commanded up");

    // 4. Lift the pack, and the dog travels because nothing is in its way.
    Simulation freight(InitialSpawn::IntakePendant, scraperx::sim::WorldContent::RegressionFixtures);
    require(freight.advance_frame(0.5).accepted, "pendant settling interval must be accepted");
    require(freight.snapshot().intake_station_active,
            "the B00 pendant spawn must be inside CAP-PENDANT's station radius");
    require(freight.set_intake_hoist_input(1.0), "intake hoist command must be accepted");
    require(advance_until(freight,
                          [](const auto &state) { return state.intake_throat_clear; },
                          20.0),
            "raising the pack clear of the dog's swing must let the dog travel and open the "
            "throat");
    const auto opened = freight.snapshot();
    require(!opened.intake_pack_pins_dog,
            "the dog must only travel once the pack has physically left its envelope");
    require(opened.intake_pack_position.y > 2.5,
            "the pack must be lifted clear of the dog, not nudged");
    require(freight.set_intake_hoist_input(0.0), "intake hoist stop must be accepted");

    // 5. Now walk the route the freight opened, all the way to +24 m.
    walk_toward(freight, kThroatX, -107.5, 12.0, true);
    walk_toward(freight, kThroatX, -113.0, 8.0, true);
    for (int flight = 0; flight < 6; ++flight) {
        const double side = (flight % 2 == 0) ? 1.0 : -1.0;
        const double lane = -118.0 + side * 2.0;
        walk_toward(freight, -side * kStairLandingX, lane, 8.0, true);
        walk_toward(freight, side * kStairLandingX, lane, 14.0, true);
    }
    walk_toward(freight, -6.0, -112.0, 12.0, true);
    const auto arrived = freight.snapshot();
    require(arrived.support_entity_id == Simulation::kIntakeHandoffEntityId,
            "the player must finish standing on the +24 m handoff deck itself");
    require(arrived.player_grounded &&
                arrived.player_position.y > kHandoffSurfaceY + 0.7,
            "the +24 m handoff must be stable support, not a graze");

    // 6. SKIN is a legal bypass: the same +24 m deck, freight untouched. The
    //    Atlas forbids walling SKIN off to protect the freight sequence.
    Simulation skin(InitialSpawn::IntakeSkinFoot, scraperx::sim::WorldContent::RegressionFixtures);
    require(skin.advance_frame(0.5).accepted, "skin settling interval must be accepted");
    std::uint32_t skin_mantles = 0;
    bool skin_arrived = false;
    for (std::uint32_t tick = 0; tick < 90 * 120; ++tick) {
        const auto state = skin.snapshot();
        if (state.support_entity_id == Simulation::kIntakeHandoffEntityId) {
            skin_arrived = true;
            break;
        }
        (void)skin.set_move_input(0.0, -1.0);
        (void)skin.set_facing(0.0, -1.0);
        const bool ready = (state.traversal_state == TraversalState::None &&
                            state.player_grounded && state.ledge_available) ||
                           state.traversal_state == TraversalState::Hanging;
        if (ready && skin.request_traversal()) {
            ++skin_mantles;
        }
        (void)skin.advance_frame(Simulation::kFixedStepSeconds);
    }
    require(skin_arrived,
            "MOD-SKIN-LADDER-S must be climbable to the same +24 m deck with the freight "
            "sequence untouched");
    const auto skin_state = skin.snapshot();
    require(skin_state.intake_pack_pins_dog && !skin_state.intake_throat_clear,
            "climbing SKIN must not falsely mutate the freight mechanism -- the pack is still "
            "down and the dog is still pinned");
    require(skin_state.intake_pack_position.y < 1.5,
            "the pack must not have moved while the player climbed the facade");

    std::cout << "PASS scraperx_sim B00 intake rise: pinned_impassable="
              << int(pinned_deepest > kBayInteriorZ)
              << " pinned_deepest_z=" << pinned_deepest
              << " forced_impassable=" << int(forced_deepest > kBayInteriorZ)
              << " overweight_y=" << rated_state.intake_overweight_pack_position.y
              << " lifted_y=" << opened.intake_pack_position.y
              << " dog_rad=" << opened.intake_dog_angle_radians
              << " handoff_y=" << arrived.player_position.y
              << " skin_mantles=" << skin_mantles
              << " skin_freight_untouched="
              << int(skin_state.intake_pack_pins_dog) << '\n';
}

} // namespace scraperx::tests
