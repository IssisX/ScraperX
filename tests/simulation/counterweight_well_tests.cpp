#include "test_suites.hpp"
#include "test_utilities.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

namespace scraperx::tests {

namespace {

// ---- AS-006, the Counterweight Well ----------------------------------------

constexpr double kGravity = 9.81;
constexpr double kRiderMassKg = 85.0;
constexpr double kWellACageMassKg = 350.0;
constexpr double kWellASkipMassKg = 800.0;
constexpr double kWellACageFloorTop = 154.25;
constexpr double kWellATravel = 22.0;
constexpr double kWellBCageMassKg = 350.0;
constexpr double kWellBCounterweightMassKg = 2500.0;
constexpr double kWellBCageFloorTop = 176.25;
constexpr double kWellBTravel = 22.0;
constexpr double kWellBCounterweightHalfHeight = 11.0;
// The energy rule is checked from body states every tick. A standing capsule
// settles by millimetres on whatever floor it is on; 1 cm of that for the
// 85 kg rider is the tolerance, so stance noise is never read as a motor.
constexpr double kStanceJitterJ = kRiderMassKg * kGravity * 0.01;

double kit_y(const scraperx::sim::Simulation &simulation, const std::uint64_t entity) {
    return simulation.kit_body_position(simulation.kit_body_index(entity)).y;
}

// From the stair's top deck into Stage A's cage through its open north side,
// east of the trip handle hanging in front of it.
bool board_well_a(scraperx::sim::Simulation &simulation) {
    return walk_to(simulation, -10.0, -129.2, 5.0) && walk_to(simulation, -10.2, -130.6, 4.0);
}

// Inside the cage: face the bollard south-west and take the rope's end off
// it; face the cage's eye west and hook it on. Every step is the rig verb
// the snapshot offers at that moment, never a forced state.
bool rig_well_a(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, -11.35, -131.95, 4.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(-0.7, -0.7);
    (void)simulation.advance_frame(0.4);
    const auto at_bollard = simulation.snapshot();
    if (at_bollard.rig_action != 2 ||
        at_bollard.rig_target_entity_id != Simulation::kWellAShackleEntityId) {
        return false;
    }
    (void)simulation.request_rig();
    (void)simulation.advance_frame(0.3);
    if (simulation.snapshot().carrying_entity_id != Simulation::kWellAShackleEntityId ||
        !walk_to(simulation, -11.35, -131.40, 3.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(-1.0, 0.0);
    (void)simulation.advance_frame(0.5);
    const auto at_eye = simulation.snapshot();
    if (at_eye.rig_action != 1 || at_eye.rig_target_entity_id != Simulation::kWellACageEntityId) {
        return false;
    }
    (void)simulation.request_rig();
    (void)simulation.advance_frame(0.3);
    const auto hooked = simulation.snapshot();
    return hooked.carrying_entity_id == 0 &&
           hooked.well_a_rope_end_entity_id == Simulation::kWellACageEntityId;
}

// Inside the cage at its open north side: take the trip handle hanging in
// front of it and step back south at `pull` of full stick until the catch
// lets go (or `seconds` pass), then let go of the handle. True once the
// catch has been seen open.
bool pull_well_a(scraperx::sim::Simulation &simulation, const double pull, const double seconds) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, -11.05, -130.35, 3.0, 0.08)) {
        return false;
    }
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(0.5);
    const auto facing = simulation.snapshot();
    if (facing.carry_target_entity_id != Simulation::kWellAHandleEntityId ||
        facing.carry_target_kind != 2) {
        return false;
    }
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.3);
    if (simulation.snapshot().carrying_entity_id != Simulation::kWellAHandleEntityId) {
        return false;
    }
    bool opened = false;
    const auto ticks =
        static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (std::uint32_t tick = 0; tick < ticks && !opened; ++tick) {
        (void)simulation.set_move_input(0.0, -0.7 * pull);
        (void)simulation.set_facing(0.0, 1.0);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        opened = !simulation.snapshot().well_a_catch_latched;
    }
    (void)simulation.set_move_input(0.0, 0.0);
    if (simulation.snapshot().carrying_entity_id != 0) {
        (void)simulation.request_set_down();
    }
    return opened;
}


bool board_well_b(scraperx::sim::Simulation &simulation) {
    return walk_to(simulation, -8.65, -131.40, 3.0, 0.08) &&
           walk_to(simulation, -7.55, -131.40, 3.0, 0.08);
}

bool rig_well_b(scraperx::sim::Simulation &simulation) {
    using scraperx::sim::Simulation;
    const auto fail = [&](const char *step) {
        const auto s = simulation.snapshot();
        std::cout << "INFO AS-006 B rig fail: step=" << step
                  << " pos=(" << s.player_position.x << ',' << s.player_position.y << ','
                  << s.player_position.z << ") grounded=" << int(s.player_grounded)
                  << " support=" << s.support_entity_id
                  << " rig_action=" << int(s.rig_action)
                  << " rig_target=" << s.rig_target_entity_id
                  << " carrying=" << s.carrying_entity_id
                  << " rope_end=" << s.well_b_rope_end_entity_id
                  << " rope_tension=" << s.well_b_rope_tension_n
                  << " cw_travel=" << s.well_b_counterweight_travel << '\n';
        return false;
    };
    // The interaction, not centimeter-perfect foot placement, is the
    // contract. The cage/post collision resolves this stance about 0.11 m
    // from the nominal point while already offering the correct UNHOOK verb.
    if (!walk_to(simulation, -8.45, -131.95, 3.0, 0.15)) return fail("walk_bollard");
    (void)simulation.set_facing(-0.65, -0.75);
    (void)simulation.advance_frame(0.4);
    const auto at_bollard = simulation.snapshot();
    if (at_bollard.rig_action != 2 ||
        at_bollard.rig_target_entity_id != Simulation::kWellBShackleEntityId)
        return fail("offer_unhook");
    (void)simulation.request_rig();
    (void)simulation.advance_frame(0.3);
    if (simulation.snapshot().carrying_entity_id != Simulation::kWellBShackleEntityId)
        return fail("take_shackle");
    if (!walk_to(simulation, -6.55, -131.40, 3.0, 0.08)) return fail("walk_eye");
    (void)simulation.set_facing(1.0, 0.0);
    (void)simulation.advance_frame(0.5);
    const auto at_eye = simulation.snapshot();
    if (at_eye.rig_action != 1 ||
        at_eye.rig_target_entity_id != Simulation::kWellBCageEntityId)
        return fail("offer_hook");
    (void)simulation.request_rig();
    (void)simulation.advance_frame(0.3);
    const auto hooked = simulation.snapshot();
    if (hooked.carrying_entity_id != 0 ||
        hooked.well_b_rope_end_entity_id != Simulation::kWellBCageEntityId)
        return fail("hook_commit");
    return true;
}

bool pull_well_b(scraperx::sim::Simulation &simulation, const double pull, const double seconds) {
    using scraperx::sim::Simulation;
    if (!walk_to(simulation, -7.35, -130.35, 3.0, 0.08)) return false;
    (void)simulation.set_facing(0.0, 1.0);
    (void)simulation.advance_frame(0.5);
    const auto facing = simulation.snapshot();
    if (facing.carry_target_entity_id != Simulation::kWellBHandleEntityId ||
        facing.carry_target_kind != 2) return false;
    (void)simulation.request_pick_up();
    (void)simulation.advance_frame(0.3);
    if (simulation.snapshot().carrying_entity_id != Simulation::kWellBHandleEntityId) return false;
    bool opened = false;
    const auto ticks =
        static_cast<std::uint32_t>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (std::uint32_t tick = 0; tick < ticks && !opened; ++tick) {
        (void)simulation.set_move_input(0.0, -0.7 * pull);
        (void)simulation.set_facing(0.0, 1.0);
        (void)simulation.advance_frame(Simulation::kFixedStepSeconds);
        opened = !simulation.snapshot().well_b_catch_latched;
    }
    (void)simulation.set_move_input(0.0, 0.0);
    if (simulation.snapshot().carrying_entity_id != 0) (void)simulation.request_set_down();
    return opened;
}

} // namespace

void run_counterweight_well_tests() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;

    // ---- AS-006 Stage A, the skip lift (03_EXECUTION/ASCENT/AS-006_CW_PIN.md)
    //
    // No link, no lift: as found, the rope's end is made fast on the bollard.
    // Take the trip handle and keep pulling past the lever's stop: the catch
    // lets go, the bollard holds the skip within its 0.03 m of slack, and the
    // cage never moves. Let go: the lever falls back, the catch seats the
    // skip again and takes the load off the rope, and the rope's end comes
    // off the bollard onto the cage as before -- the stage is not stranded.
    Simulation unlinked(InitialSpawn::StairTop, scraperx::sim::WorldContent::RegressionFixtures);
    require(unlinked.advance_frame(1.0).accepted, "the stair-top settle interval must be accepted");
    const auto well_stair_top = unlinked.snapshot();
    require(well_stair_top.player_grounded && well_stair_top.player_position.y > 154.8 &&
                well_stair_top.player_position.y < 155.0,
            "StairTop must stand the player on the stair's 154 m top deck");
    require(well_stair_top.well_a_catch_latched &&
                well_stair_top.well_a_rope_end_entity_id == Simulation::kWellAFrameEntityId,
            "as found, the skip must be in its catch and the rope made fast on the bollard");
    const double unlinked_skip_y0 = kit_y(unlinked, Simulation::kWellASkipEntityId);
    const double unlinked_cage_y0 = kit_y(unlinked, Simulation::kWellACageEntityId);
    require(board_well_a(unlinked), "the player must walk into Stage A's cage");
    require(walk_to(unlinked, -11.05, -130.35, 3.0, 0.08),
            "the player must reach the trip handle from inside the cage");
    (void)unlinked.set_facing(0.0, 1.0);
    (void)unlinked.advance_frame(0.5);
    require(unlinked.snapshot().carry_target_entity_id == Simulation::kWellAHandleEntityId &&
                unlinked.snapshot().carry_target_kind == 2,
            "the trip handle must be offered to a player facing it from the cage");
    (void)unlinked.request_pick_up();
    (void)unlinked.advance_frame(0.3);
    require(unlinked.snapshot().carrying_entity_id == Simulation::kWellAHandleEntityId,
            "the player must take the trip handle");
    double unlinked_cage_worst = 0.0;
    double unlinked_skip_drop = 0.0;
    double bollard_tension = 0.0;
    bool unlinked_opened = false;
    const auto note_unlinked = [&]() {
        const auto state = unlinked.snapshot();
        unlinked_opened = unlinked_opened || !state.well_a_catch_latched;
        unlinked_cage_worst =
            std::max(unlinked_cage_worst,
                     std::abs(kit_y(unlinked, Simulation::kWellACageEntityId) - unlinked_cage_y0));
        unlinked_skip_drop = std::max(
            unlinked_skip_drop, unlinked_skip_y0 - kit_y(unlinked, Simulation::kWellASkipEntityId));
        if (!state.well_a_catch_latched) {
            bollard_tension = std::max(bollard_tension, state.well_a_rope_tension_n);
        }
    };
    for (std::uint32_t tick = 0; tick < 90 * 3; ++tick) {
        (void)unlinked.set_move_input(0.0, -0.35);
        (void)unlinked.set_facing(0.0, 1.0);
        (void)unlinked.advance_frame(Simulation::kFixedStepSeconds);
        note_unlinked();
    }
    (void)unlinked.set_move_input(0.0, 0.0);
    if (unlinked.snapshot().carrying_entity_id != 0) {
        (void)unlinked.request_set_down();
    }
    for (std::uint32_t tick = 0; tick < 90 * 4; ++tick) {
        (void)unlinked.advance_frame(Simulation::kFixedStepSeconds);
        note_unlinked();
    }
    const auto after_unlinked = unlinked.snapshot();
    require(unlinked_opened, "pulling the trip handle must open the catch, linked or not");
    require(unlinked_cage_worst <= 0.05,
            "no link, no lift: with the rope on the bollard the cage must not move");
    require(unlinked_skip_drop <= 0.05,
            "the bollard must hold the skip within its slack when the catch opens");
    require(bollard_tension > 0.9 * kWellASkipMassKg * kGravity,
            "with the catch open, the bollard must carry the skip's weight through the rope");
    require(after_unlinked.well_a_catch_latched && after_unlinked.well_a_rope_tension_n < 60.0,
            "let go, the catch must seat the skip again and take the load off the rope");
    require(rig_well_a(unlinked),
            "after the wasted pull the rope's end must still come off the bollard onto the cage");
    std::cout << "PASS scraperx_sim AS-006 A no link: cage_worst=" << unlinked_cage_worst
              << " skip_drop=" << unlinked_skip_drop << " bollard_tension=" << bollard_tension
              << " relatched=" << int(after_unlinked.well_a_catch_latched) << " rerigged=1\n";

    // The ride: take the rope's end off the bollard, hook it on the cage's
    // eye, take the handle and step back. The skip falls 22 m and the cage
    // carries the rider 22 m under a governor that can only brake. The rider
    // stands on the cage the whole way; the floor stops flush with the 176
    // ring; at no tick has the payload gained more energy than the skip
    // released.
    Simulation well(InitialSpawn::StairTop, scraperx::sim::WorldContent::RegressionFixtures);
    require(well.advance_frame(1.0).accepted, "the ride's settle interval must be accepted");
    require(board_well_a(well) && rig_well_a(well),
            "the player must rig Stage A from inside its cage");
    const double skip_y0 = kit_y(well, Simulation::kWellASkipEntityId);
    const double cage_y0 = kit_y(well, Simulation::kWellACageEntityId);
    const double rider_y0 = well.snapshot().player_position.y;
    require(pull_well_a(well, 0.5, 3.0), "stepping back with the handle must trip the catch");
    const double well_ride_start = well.snapshot().simulation_time_seconds;
    bool rode_on_cage = true;
    double well_ride_seconds = 0.0;
    double worst_energy_margin = std::numeric_limits<double>::infinity();
    for (std::uint32_t tick = 0; tick < 90 * 20; ++tick) {
        (void)well.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = well.snapshot();
        const double cage_rise = kit_y(well, Simulation::kWellACageEntityId) - cage_y0;
        if (cage_rise > 0.05 && cage_rise < kWellATravel - 0.05) {
            rode_on_cage = rode_on_cage && state.player_grounded &&
                           state.support_entity_id == Simulation::kWellACageEntityId;
        }
        const double released =
            kWellASkipMassKg * kGravity * (skip_y0 - kit_y(well, Simulation::kWellASkipEntityId));
        const double gained = kWellACageMassKg * kGravity * cage_rise +
                              kRiderMassKg * kGravity * (state.player_position.y - rider_y0);
        worst_energy_margin = std::min(worst_energy_margin, released - gained);
        if (well_ride_seconds == 0.0 && state.well_a_cage_travel >= kWellATravel - 0.01) {
            well_ride_seconds = state.simulation_time_seconds - well_ride_start;
        }
    }
    const auto at_top = well.snapshot();
    const double floor_y = kit_y(well, Simulation::kWellACageEntityId) + 0.10;
    const double skip_released =
        kWellASkipMassKg * kGravity * (skip_y0 - kit_y(well, Simulation::kWellASkipEntityId));
    const double payload_gained =
        kWellACageMassKg * kGravity * (kit_y(well, Simulation::kWellACageEntityId) - cage_y0) +
        kRiderMassKg * kGravity * (at_top.player_position.y - rider_y0);
    require(rode_on_cage, "the rider must stand on the cage for the whole ride");
    require(std::abs(floor_y - (kWellACageFloorTop + kWellATravel)) <= 0.05,
            "the cage's floor must stop flush with the 176 ring at 176.25 m");
    require(at_top.well_a_cage_peak_speed <= 2.6, "the governor must hold the cage to 2.5 m/s");
    require(at_top.player_grounded && at_top.support_entity_id == Simulation::kWellACageEntityId &&
                at_top.player_position.y > 177.0,
            "the rider must arrive standing in the cage at the top");
    require(worst_energy_margin >= -kStanceJitterJ,
            "at no tick may the payload have gained more energy than the skip released");

    // Dying restores the last commit, machines included. Stage B now occupies
    // the east handoff beside A, so walking east can legitimately advance the
    // player's automatic checkpoint onto B before the eventual fall. The
    // invariant here is therefore the machine commit, not which adjacent
    // support owns the latest player checkpoint: A must still restore parked
    // at the top with its rope on the cage eye.
    const auto well_deaths = at_top.death_count;
    for (std::uint32_t tick = 0; tick < 90 * 8 && well.snapshot().death_count == well_deaths;
         ++tick) {
        (void)well.set_move_input(well.snapshot().player_grounded ? 0.6 : 0.0, 0.0);
        (void)well.set_facing(1.0, 0.0);
        (void)well.advance_frame(Simulation::kFixedStepSeconds);
    }
    require(well.snapshot().death_count == well_deaths + 1,
            "walking off the parked cage must be a lethal fall");
    (void)well.set_move_input(0.0, 0.0);
    require(well.advance_frame(1.0).accepted, "the restore settle interval must be accepted");
    const auto well_restored = well.snapshot();
    require(well_restored.player_position.y > 177.0 &&
                std::abs(well_restored.well_a_cage_travel - kWellATravel) <= 0.05 &&
                well_restored.well_a_rope_end_entity_id == Simulation::kWellACageEntityId,
            "death restore must preserve Stage A's committed machine state");
    std::cout << "PASS scraperx_sim AS-006 A ride: ride_s=" << well_ride_seconds
              << " floor_y=" << floor_y << " peak_speed=" << at_top.well_a_cage_peak_speed
              << " rode_on_cage=" << int(rode_on_cage) << " skip_released_J=" << skip_released
              << " payload_gained_J=" << payload_gained
              << " energy_margin_J=" << worst_energy_margin
              << " restored_y=" << well_restored.player_position.y
              << " restored_support=" << well_restored.support_entity_id << '\n';


    // ---- AS-006 Stage B, guided lattice counterweight ----------------------
    require(board_well_b(well), "the rider must step directly from A's parked cage into B's cage");
    require(well.snapshot().well_b_catch_latched &&
                well.snapshot().well_b_rope_end_entity_id == Simulation::kWellBFrameEntityId,
            "Stage B must be found caught with its rope made fast on the bollard");

    const double b_unlinked_weight_y0 = kit_y(well, Simulation::kWellBCounterweightEntityId);
    const double b_unlinked_cage_y0 = kit_y(well, Simulation::kWellBCageEntityId);
    require(pull_well_b(well, 0.55, 3.0),
            "Stage B's trip handle must withdraw the counterweight catch");
    double b_unlinked_cage_worst = 0.0;
    double b_unlinked_weight_drop = 0.0;
    double b_bollard_tension = 0.0;
    for (std::uint32_t tick = 0; tick < 90 * 4; ++tick) {
        (void)well.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = well.snapshot();
        b_unlinked_cage_worst =
            std::max(b_unlinked_cage_worst,
                     std::abs(kit_y(well, Simulation::kWellBCageEntityId) - b_unlinked_cage_y0));
        b_unlinked_weight_drop =
            std::max(b_unlinked_weight_drop,
                     b_unlinked_weight_y0 - kit_y(well, Simulation::kWellBCounterweightEntityId));
        if (!state.well_b_catch_latched)
            b_bollard_tension = std::max(b_bollard_tension, state.well_b_rope_tension_n);
    }
    const auto b_relatched = well.snapshot();
    require(b_unlinked_cage_worst <= 0.05,
            "Stage B no link, no lift: the cage must remain parked");
    require(b_unlinked_weight_drop <= 0.05,
            "Stage B's bollard must arrest the lattice within its declared slack");
    require(b_bollard_tension > 0.9 * kWellBCounterweightMassKg * kGravity,
            "Stage B's bollard must carry the released lattice through the rope");
    require(b_relatched.well_b_catch_latched && b_relatched.well_b_rope_tension_n < 60.0,
            "letting go must re-seat B's lattice and unload its rope");
    std::cout << "PASS scraperx_sim AS-006 B no link: cage_worst="
              << b_unlinked_cage_worst << " lattice_drop=" << b_unlinked_weight_drop
              << " bollard_tension=" << b_bollard_tension << " relatched=1\n";

    require(rig_well_b(well),
            "after the wasted pull, B's rope must come off the bollard and hook to its cage");
    const double b_weight_y0 = kit_y(well, Simulation::kWellBCounterweightEntityId);
    const double b_cage_y0 = kit_y(well, Simulation::kWellBCageEntityId);
    const double b_rider_y0 = well.snapshot().player_position.y;
    require(pull_well_b(well, 0.55, 3.0),
            "with the rope on the cage, B's trip line must release the lattice");
    const double b_ride_start = well.snapshot().simulation_time_seconds;
    bool b_rode_on_cage = true;
    double b_ride_seconds = 0.0;
    double b_ride_peak_speed = 0.0;
    double b_worst_energy_margin = std::numeric_limits<double>::infinity();
    const auto b_cage_index = well.kit_body_index(Simulation::kWellBCageEntityId);
    require(b_cage_index != Simulation::kKitNone,
            "Stage B's cage must remain addressable through the mechanism kit");
    for (std::uint32_t tick = 0; tick < 90 * 20; ++tick) {
        (void)well.advance_frame(Simulation::kFixedStepSeconds);
        const auto state = well.snapshot();
        const auto cage_velocity = well.kit_body_velocity(b_cage_index);
        b_ride_peak_speed = std::max(b_ride_peak_speed, std::abs(cage_velocity.y));
        const double cage_rise = kit_y(well, Simulation::kWellBCageEntityId) - b_cage_y0;
        if (cage_rise > 0.05 && cage_rise < kWellBTravel - 0.05) {
            b_rode_on_cage = b_rode_on_cage && state.player_grounded &&
                             state.support_entity_id == Simulation::kWellBCageEntityId;
        }
        const double released =
            kWellBCounterweightMassKg * kGravity *
            (b_weight_y0 - kit_y(well, Simulation::kWellBCounterweightEntityId));
        const double gained = kWellBCageMassKg * kGravity * cage_rise +
                              kRiderMassKg * kGravity * (state.player_position.y - b_rider_y0);
        b_worst_energy_margin = std::min(b_worst_energy_margin, released - gained);
        if (b_ride_seconds == 0.0 && state.well_b_cage_travel >= kWellBTravel - 0.01)
            b_ride_seconds = state.simulation_time_seconds - b_ride_start;
    }
    const auto b_top = well.snapshot();
    const double b_floor_y = kit_y(well, Simulation::kWellBCageEntityId) + 0.10;
    const double b_lattice_top =
        kit_y(well, Simulation::kWellBCounterweightEntityId) + kWellBCounterweightHalfHeight;
    require(b_rode_on_cage, "Stage B's rider must inherit the cage motion for the whole lift");
    require(std::abs(b_floor_y - (kWellBCageFloorTop + kWellBTravel)) <= 0.05,
            "Stage B's cage floor must stop flush with the 198.25 m ring");
    // The guide's snapshot peak is lifetime-wide and can include the player's
    // earlier Stage A handoff/death contact with this already-existing cage.
    // Governor proof is ride-scoped: measure the cage's real body velocity
    // only after B's catch has released and the powered lift has begun.
    std::cout << "INFO AS-006 B governor: ride_peak=" << b_ride_peak_speed
              << " lifetime_peak=" << b_top.well_b_cage_peak_speed
              << " ride_s=" << b_ride_seconds
              << " floor_y=" << b_floor_y << '\n';
    require(b_ride_peak_speed <= 3.1,
            "Stage B's brake-only governor must hold the powered ride to 3.0 m/s");
    require(b_top.player_grounded && b_top.support_entity_id == Simulation::kWellBCageEntityId &&
                b_top.player_position.y > 199.0,
            "Stage B must arrive with the rider standing in its cage at the 198 ring");
    require(b_worst_energy_margin >= -kStanceJitterJ,
            "Stage B may not give the payload more potential energy than the lattice released");
    require(std::abs(b_lattice_top - 198.25) <= 0.05,
            "the spent 22 m lattice must finish spanning 176.25..198.25 as real structure");
    std::cout << "PASS scraperx_sim AS-006 B ride: ride_s=" << b_ride_seconds
              << " floor_y=" << b_floor_y
              << " peak_speed=" << b_ride_peak_speed
              << " lifetime_peak_speed=" << b_top.well_b_cage_peak_speed
              << " rode_on_cage=" << int(b_rode_on_cage)
              << " energy_margin_J=" << b_worst_energy_margin
              << " lattice_top_y=" << b_lattice_top << '\n';
}

} // namespace scraperx::tests
