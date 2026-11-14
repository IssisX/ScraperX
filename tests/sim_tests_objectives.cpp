// J-129 (#455): the objective core. A table of mission objectives; each check
// only reads the world, with a done flag that never clears once set. Missions
// 1 and 7 from the audit's B3 table
// (03_EXECUTION/PLANNING/FINISH_LINE_AND_REASONS_TO_PLAY.md).
//
// The core is driven here by calling Objectives::update once per fixed step,
// the way the sim tick will once the hook lands (after J-116 #438). The save
// fields ride F8's bump (J-116), one bump not two, so the save/restore facts
// are not provable yet; the save seam (one done bit per objective) is proven
// by a bits round-trip below. The tick hook and the Saved wiring are the
// follow-up, not this change.

#include "sim_tests.hpp"

#include <cmath>

#include "sim/objectives.hpp"

namespace {

// One fixed step of the world, then one tick of the objectives.
void tick_objectives(scraperx::sim::Simulation &simulation, scraperx::sim::Objectives &objectives) {
    (void)simulation.advance_frame(scraperx::sim::Simulation::kFixedStepSeconds);
    objectives.update(simulation);
}

// E's cab at the top and still, mirroring wait_wet_e_top but updating the
// objectives on every tick (that helper takes no callback).
bool wait_wet_e_top_with_objectives(scraperx::sim::Simulation &simulation,
                                    scraperx::sim::Objectives &objectives,
                                    const double budget_seconds) {
    using scraperx::sim::Simulation;
    const auto ticks = static_cast<std::uint32_t>(budget_seconds * Simulation::kTickRateHz);
    std::uint32_t still = 0;
    for (std::uint32_t tick = 0; tick < ticks; ++tick) {
        tick_objectives(simulation, objectives);
        const auto w = simulation.wet_state();
        still = w.e_cab_travel > 40.0 && std::abs(w.e_cab_speed) < 0.05 ? still + 1 : 0;
        if (still >= 2 * Simulation::kTickRateHz) {
            return true;
        }
    }
    return false;
}

}  // namespace

void run_objectives() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Objectives;
    using scraperx::sim::Simulation;

    // ---- Mission 1, "Clock in": S1 run and ridden to 22 m -------------------
    {
        Simulation ride(InitialSpawn::ExteriorGrade);
        Objectives objectives;
        require(ride.advance_frame(0.5).accepted, "objectives: S1 ride settle interval must be accepted");
        for (std::uint32_t tick = 0; tick < 90; ++tick) {
            tick_objectives(ride, objectives);
        }
        require(!objectives.done(Objectives::Id::Mission1ClockIn) &&
                    !objectives.done(Objectives::Id::Mission7BringPlantUp),
                "objectives: fresh, no objective is done");
        require(board_s1(ride), "objectives: the rider must board S1");
        // Standing on the plate, no input, until the cage tops out; the
        // objectives see every tick of the rise.
        bool topped = false;
        for (std::uint32_t tick = 0; tick < 60 * Simulation::kTickRateHz && !topped; ++tick) {
            (void)ride.set_move_input(0.0, 0.0);
            (void)ride.set_facing(-1.0, 0.0);
            tick_objectives(ride, objectives);
            topped = ride.stack_state().s1_cage_travel >= kS1Travel - 0.01;
        }
        require(topped, "objectives: S1 must carry the cage to the top of its travel");
        require(objectives.done(Objectives::Id::Mission1ClockIn),
                "objectives: ridden to 22 m, mission 1 is done");
        require(objectives.done_count() == 1u, "objectives: exactly one objective is done");
        // Sticky: off the cage over the gangway onto deck 2, the flag stays.
        require(walk_to(ride, 10.0, -123.2, 6.0) && walk_to(ride, 10.0, -126.5, 6.0),
                "objectives: the rider must walk off S1's cage onto deck 2");
        for (std::uint32_t tick = 0; tick < 90; ++tick) {
            tick_objectives(ride, objectives);
        }
        require(objectives.done(Objectives::Id::Mission1ClockIn),
                "objectives: mission 1 stays done after walking off");
        std::cout << "PASS scraperx_sim objectives mission 1: cage_travel="
                  << ride.stack_state().s1_cage_travel << " done_count=" << objectives.done_count() << "\n";
    }

    // ---- Mission 1, falsifier: an empty cage is not a ride -------------------
    {
        Simulation brief(InitialSpawn::ExteriorGrade);
        Objectives objectives;
        require(brief.advance_frame(0.5).accepted, "objectives: empty-cage settle interval must be accepted");
        require(board_s1(brief), "objectives: the rider must board S1");
        require(walk_to(brief, 10.0, -116.5, 6.0), "objectives: the rider must step straight back off S1's cage");
        // The empty cage rises by itself; the objectives see every tick.
        const bool sent_up = wait_for(brief, 120.0, [&](const scraperx::sim::Snapshot &) {
            objectives.update(brief);
            return brief.stack_state().s1_cage_travel >= kS1Travel - 0.01;
        });
        require(sent_up, "objectives: the empty cage must go up by itself");
        require(!objectives.done(Objectives::Id::Mission1ClockIn),
                "objectives: an empty cage is not a ride, mission 1 stays false");
        require(objectives.done_count() == 0u, "objectives: no objective is done after the empty cage");
        std::cout << "PASS scraperx_sim objectives mission 1 falsifier: cage_travel="
                  << brief.stack_state().s1_cage_travel << " done_count=" << objectives.done_count() << "\n";
    }

    // ---- Mission 7, "Bring the plant up": ram, chiller, accumulator ---------
    {
        Simulation band(InitialSpawn::Ring220North);
        Objectives objectives;
        require(band.advance_frame(1.0).accepted, "objectives: wet band settle interval must be accepted");
        require(!objectives.done(Objectives::Id::Mission1ClockIn) &&
                    !objectives.done(Objectives::Id::Mission7BringPlantUp),
                "objectives: fresh, no objective is done");
        require(seat_wet_spool(band), "objectives: the spool carried aboard and set in its gap");
        const bool d_done = wait_for(band, 120.0, [&](const scraperx::sim::Snapshot &) {
            objectives.update(band);
            return band.wet_state().d_platform_travel >= 35.95;
        });
        require(d_done, "objectives: D's float must carry the platform to its stop at 256.25");
        require(!objectives.done(Objectives::Id::Mission7BringPlantUp),
                "objectives: after the ram only, mission 7 is not done");
        require(drop_into_wet_e(band), "objectives: from D's platform through E's doors, off the sill");
        require(wait_wet_e_top_with_objectives(band, objectives, 60.0),
                "objectives: E must blow the cab to the top of its shaft");
        require(!objectives.done(Objectives::Id::Mission7BringPlantUp),
                "objectives: after the chiller, mission 7 is not done");
        require(walk_to(band, 13.4, -137.2, 4.0) && walk_to(band, 13.4, -139.9, 6.0),
                "objectives: from E's cab over the rim onto F's platform");
        require(couple_wet_hose(band), "objectives: F's hose coupled");
        require(pull_wet_stop_valve(band), "objectives: F's stop valve thrown");
        const bool f_done = wait_for(band, 90.0, [&](const scraperx::sim::Snapshot &) {
            objectives.update(band);
            return band.wet_state().f_platform_travel >= 41.95;
        });
        require(f_done, "objectives: the accumulator must lift F's platform to TP-340");
        require(objectives.done(Objectives::Id::Mission7BringPlantUp),
                "objectives: ram, chiller, accumulator, in order, mission 7 is done");
        require(!objectives.done(Objectives::Id::Mission1ClockIn),
                "objectives: no S1 ride here, mission 1 stays false");
        require(objectives.done_count() == 1u, "objectives: exactly one objective is done");
        std::cout << "PASS scraperx_sim objectives mission 7: d_travel="
                  << band.wet_state().d_platform_travel
                  << " e_cab_travel=" << band.wet_state().e_cab_travel
                  << " f_travel=" << band.wet_state().f_platform_travel
                  << " done_count=" << objectives.done_count() << "\n";
    }

    // ---- The save seam: one done bit per objective ---------------------------
    {
        Objectives loaded;
        loaded.restore_bits(1u << static_cast<std::uint32_t>(Objectives::Id::Mission1ClockIn));
        require(loaded.done(Objectives::Id::Mission1ClockIn) &&
                    !loaded.done(Objectives::Id::Mission7BringPlantUp),
                "objectives: restored bits set exactly the saved flags");
        require(loaded.save_bits() ==
                    (1u << static_cast<std::uint32_t>(Objectives::Id::Mission1ClockIn)),
                "objectives: save bits round-trip");
        // Restore never clears: loading the second flag keeps the first.
        loaded.restore_bits(1u << static_cast<std::uint32_t>(Objectives::Id::Mission7BringPlantUp));
        require(loaded.done(Objectives::Id::Mission1ClockIn) &&
                    loaded.done(Objectives::Id::Mission7BringPlantUp) && loaded.done_count() == 2u,
                "objectives: restoring more flags never clears one already done");
        // An old save carries no fields: every flag false.
        Objectives old_save;
        old_save.restore_bits(0);
        require(!old_save.done(Objectives::Id::Mission1ClockIn) &&
                    !old_save.done(Objectives::Id::Mission7BringPlantUp) && old_save.done_count() == 0u,
                "objectives: an old save loads with every flag false");
        std::cout << "PASS scraperx_sim objectives save seam: bits round-trip, restore never clears\n";
    }
}

SCRAPERX_GROUP("objectives", 1110, run_objectives);
SCRAPERX_SLICE("objectives", run_objectives);
