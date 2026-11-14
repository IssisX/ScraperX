// J-129 (#455): the objective core. A table of mission objectives; each check
// only reads the world, with a done flag that never clears once set. Missions
// 1, 7 and 8 from the audit's B3 table
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
#include "sim/summit.hpp"

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

// Advance `seconds` of world time, updating the objectives on every tick.
void advance_with_objectives(scraperx::sim::Simulation &simulation, scraperx::sim::Objectives &objectives,
                             const double seconds) {
    const auto ticks = static_cast<std::uint32_t>(
        seconds * static_cast<double>(scraperx::sim::Simulation::kTickRateHz) + 0.5);
    for (std::uint32_t tick = 0; tick < ticks; ++tick) {
        tick_objectives(simulation, objectives);
    }
}

// Walk to a target, then one tick of the objectives. The done check is a
// level, not a transient edge, so an update after each leg sees every stage.
bool walk_to_with_objectives(scraperx::sim::Simulation &simulation, scraperx::sim::Objectives &objectives,
                             const double x, const double z, const double budget_seconds,
                             const double tolerance = 0.15) {
    const bool ok = walk_to(simulation, x, z, budget_seconds, tolerance);
    objectives.update(simulation);
    return ok;
}

// Hold the stick until done, then one tick of the objectives.
template <typename Done>
bool hold_stick_with_objectives(scraperx::sim::Simulation &simulation, scraperx::sim::Objectives &objectives,
                                const double x, const double z, const double fx, const double fz,
                                const double seconds, Done done) {
    const bool ok = hold_stick(simulation, x, z, fx, fz, seconds, done);
    objectives.update(simulation);
    return ok;
}

// Up the two blocks and onto the gantry, as a climber would — mirrored from
// tests/sim_tests_summit.cpp, updating the objectives after every leg.
bool climb_gantry_with_objectives(scraperx::sim::Simulation &simulation, scraperx::sim::Objectives &objectives) {
    using scraperx::sim::Summit;
    constexpr double kBlockOneSouthStand = Summit::kStrikerSouthZ + 0.35;
    if (!(walk_to_with_objectives(simulation, objectives, 8.0, -184.6, 6.0, 0.2) &&
          walk_to_with_objectives(simulation, objectives, -3.2, -184.6, 14.0, 0.2) &&
          walk_to_with_objectives(simulation, objectives, -3.2, kBlockOneSouthStand, 4.0, 0.08))) {
        return false;
    }
    if (!c4_mantle(simulation, 0.0, -1.0, Summit::kBlockOneTop + 0.45, "the summit: onto block one")) return false;
    objectives.update(simulation);
    if (!walk_to_with_objectives(simulation, objectives, -2.95, -187.5, 3.0, 0.06)) return false;
    if (!c4_hang_up(simulation, 1.0, 0.0, Summit::kBlockTwoTop + 0.4, "the summit: up onto block two")) return false;
    objectives.update(simulation);
    if (!walk_to_with_objectives(simulation, objectives, -1.25, -187.5, 3.0, 0.06)) return false;
    if (!c4_hang_up(simulation, 1.0, 0.0, Summit::kGantryTop + 0.4, "the summit: up onto the gantry")) return false;
    objectives.update(simulation);
    return true;
}

// Walks east off the gantry's edge onto the plate — mirrored from
// tests/sim_tests_summit.cpp.
bool step_off_gantry_with_objectives(scraperx::sim::Simulation &simulation, scraperx::sim::Objectives &objectives,
                                     const double z, const double speed) {
    using scraperx::sim::Snapshot;
    using scraperx::sim::Summit;
    if (!walk_to_with_objectives(simulation, objectives, 1.9, z, 5.0, 0.08)) return false;
    (void)simulation.set_facing(1.0, 0.0);
    return hold_stick_with_objectives(simulation, objectives, speed, 0.0, 1.0, 0.0, 6.0, [](const Snapshot &state) {
        return state.player_grounded && state.player_position.y < Summit::kGantryTop - 2.0;
    });
}

// From the spawn to the arm's root along the deck's south side — mirrored
// from tests/sim_tests_summit.cpp.
bool walk_to_arm_with_objectives(scraperx::sim::Simulation &simulation, scraperx::sim::Objectives &objectives) {
    using scraperx::sim::Summit;
    return walk_to_with_objectives(simulation, objectives, 8.0, -185.0, 6.0, 0.2) &&
           walk_to_with_objectives(simulation, objectives, -10.5, -185.0, 14.0, 0.2) &&
           walk_to_with_objectives(simulation, objectives, -11.0, Summit::kArmCentreZ, 4.0, 0.15);
}

// Drive the summit ending to Home, updating the objectives on every tick.
// With light_lamp, the searchlight is lit (gantry strike) between the unrig
// and the jump, in B3 order: seat the beam, light the searchlight, go home.
// The ending's own falsifiers live in run_summit_ending; this drive only
// requires the legs mission 8's check depends on.
void drive_summit_home_with_objectives(scraperx::sim::Simulation &simulation, scraperx::sim::Objectives &objectives,
                                       const bool light_lamp) {
    using scraperx::sim::Snapshot;
    using scraperx::sim::Summit;
    const auto beam = simulation.kit_body_index(Summit::kLastBeamEntity);
    require(beam < simulation.kit_body_count(), "objectives: the last beam exists in the player's world");
    require(!simulation.request_summit_unrig(), "objectives: a hanging load cannot be unrigged");
    require(walk_to_with_objectives(simulation, objectives, 8.0, -185.0, 5.0, 0.15) &&
                walk_to_with_objectives(simulation, objectives, Summit::kWorkX, Summit::kWorkZ, 12.0, 0.12),
            "objectives: walk to the chain block through the existing summit");
    (void)simulation.set_move_input(0, 0);
    require(simulation.summit_state().work_in_reach, "objectives: the hands can reach the chain block");
    (void)simulation.set_summit_lower(true);
    advance_with_objectives(simulation, objectives, 2.0);
    (void)simulation.set_summit_lower(false);
    advance_with_objectives(simulation, objectives, 1.0);
    const double paid = simulation.summit_state().chain_paid_m;
    require(paid > 0.35 && paid < 0.45, "objectives: easing the chain lowers the real load");
    advance_with_objectives(simulation, objectives, 2.0);
    (void)simulation.set_summit_lower(true);
    advance_with_objectives(simulation, objectives, 3.95);
    (void)simulation.set_summit_lower(false);
    advance_with_objectives(simulation, objectives, 1.0);
    (void)simulation.set_summit_lower(true);
    advance_with_objectives(simulation, objectives, 3.0);
    (void)simulation.set_summit_lower(false);
    const auto seated = simulation.summit_state();
    require(seated.beam_seated && seated.left_bearing && seated.right_bearing &&
                seated.ending == static_cast<std::uint8_t>(Summit::Ending::Seated),
            "objectives: both flanges rest on real steel, the chain unloaded");
    require(simulation.request_summit_unrig(), "objectives: free the slack sling with the second player action");
    advance_with_objectives(simulation, objectives, 3.0);
    require(simulation.summit_state().sling_free && simulation.summit_state().beam_seated &&
                simulation.summit_state().ending == static_cast<std::uint8_t>(Summit::Ending::Unrigged),
            "objectives: the beam stays seated after both load-bearing ropes are removed");
    objectives.update(simulation);
    require(!objectives.done(scraperx::sim::Objectives::Id::Mission8TopOut),
            "objectives: seated and unrigged, mission 8 is not done yet");
    if (light_lamp) {
        require(climb_gantry_with_objectives(simulation, objectives),
                "objectives: the climb to the gantry on player inputs");
        require(!simulation.summit_state().lit, "objectives: the lamp is still dark on the gantry");
        require(step_off_gantry_with_objectives(simulation, objectives, -187.5, 0.5),
                "objectives: off the gantry's edge and down onto the plate");
        advance_with_objectives(simulation, objectives, 1.5);
        require(simulation.snapshot().death_count == 0, "objectives: the drop from the gantry is survivable");
        require(simulation.summit_state().lit, "objectives: landing from the gantry must light the lamp");
        objectives.update(simulation);
        require(!objectives.done(scraperx::sim::Objectives::Id::Mission8TopOut),
                "objectives: the lamp is lit but the ending is short of Home, mission 8 is not done");
    }
    require(walk_to_arm_with_objectives(simulation, objectives) &&
                walk_to_with_objectives(simulation, objectives, Summit::kArmEndX + 6.0, Summit::kArmCentreZ, 14.0,
                                        0.15),
            "objectives: walk to the existing parachute runway");
    require(hold_stick_with_objectives(simulation, objectives, -1, 0, -1, 0, 6.0, [](const Snapshot &state) {
                return !state.player_grounded && state.player_position.x < Summit::kArmEndX;
            }),
            "objectives: jump clear of the tower by player input");
    bool canopy_seen = false;
    bool stowed = false;
    for (int tick = 0; tick < 260 * 90; ++tick) {
        const auto now = simulation.snapshot();
        const bool stow_this_tick =
            simulation.summit_state().ending == static_cast<std::uint8_t>(Summit::Ending::Canopy) && !stowed;
        if (stow_this_tick) {
            require(simulation.request_parachute(), "objectives: the real chute control stows the canopy");
            stowed = true;
        } else if (now.player_linear_velocity.y < -12.0 && !now.parachute_deployed) {
            (void)simulation.request_parachute();
        }
        tick_objectives(simulation, objectives);
        canopy_seen = canopy_seen ||
                      simulation.summit_state().ending == static_cast<std::uint8_t>(Summit::Ending::Canopy);
        if (simulation.snapshot().player_grounded || simulation.snapshot().death_count != 0) break;
    }
    const auto home = simulation.snapshot();
    require(canopy_seen && home.player_grounded && home.player_position.y < 5.0 && home.death_count == 0 &&
                simulation.summit_state().ending == static_cast<std::uint8_t>(Summit::Ending::Home),
            "objectives: only the live canopy descent to ground reaches the quiet end card");
    objectives.update(simulation);
}

}  // namespace

void run_objectives() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Objectives;
    using scraperx::sim::Simulation;
    using scraperx::sim::Summit;

    // ---- Mission 1, "Clock in": S1 run and ridden to 22 m -------------------
    {
        Simulation ride(InitialSpawn::ExteriorGrade);
        Objectives objectives;
        require(ride.advance_frame(0.5).accepted, "objectives: S1 ride settle interval must be accepted");
        for (std::uint32_t tick = 0; tick < 90; ++tick) {
            tick_objectives(ride, objectives);
        }
        require(!objectives.done(Objectives::Id::Mission1ClockIn) &&
                    !objectives.done(Objectives::Id::Mission7BringPlantUp) &&
                    !objectives.done(Objectives::Id::Mission8TopOut),
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
                    !objectives.done(Objectives::Id::Mission7BringPlantUp) &&
                    !objectives.done(Objectives::Id::Mission8TopOut),
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

    // ---- Mission 8, "Top out": summit ending Home and the searchlight lit ---
    {
        Simulation summit(InitialSpawn::Summit);
        Objectives objectives;
        require(summit.advance_frame(1.0).accepted, "objectives: summit settle interval must be accepted");
        require(!objectives.done(Objectives::Id::Mission1ClockIn) &&
                    !objectives.done(Objectives::Id::Mission7BringPlantUp) &&
                    !objectives.done(Objectives::Id::Mission8TopOut),
                "objectives: fresh, no objective is done");
        drive_summit_home_with_objectives(summit, objectives, true);
        require(objectives.done(Objectives::Id::Mission8TopOut),
                "objectives: Home with the lamp lit, mission 8 is done");
        require(objectives.done_count() == 1u, "objectives: exactly one objective is done");
        std::cout << "PASS scraperx_sim objectives mission 8: ending="
                  << static_cast<int>(summit.summit_state().ending)
                  << " lit=" << summit.summit_state().lit << " done_count=" << objectives.done_count() << "\n";
    }

    // ---- Mission 8, falsifier: Home with the lamp unlit is not done ---------
    {
        Simulation summit(InitialSpawn::Summit);
        Objectives objectives;
        require(summit.advance_frame(1.0).accepted, "objectives: summit settle interval must be accepted");
        drive_summit_home_with_objectives(summit, objectives, false);
        require(!objectives.done(Objectives::Id::Mission8TopOut),
                "objectives: Home with the lamp unlit, mission 8 is not done");
        require(objectives.done_count() == 0u, "objectives: no objective is done after Home unlit");
        std::cout << "PASS scraperx_sim objectives mission 8 falsifier: ending="
                  << static_cast<int>(summit.summit_state().ending)
                  << " lit=" << summit.summit_state().lit << " done_count=" << objectives.done_count() << "\n";
    }

    // ---- Mission 8, falsifier: lamp lit, ending short of Home ---------------
    {
        Simulation summit(InitialSpawn::Summit);
        Objectives objectives;
        require(summit.advance_frame(1.0).accepted, "objectives: summit settle interval must be accepted");
        require(climb_gantry_with_objectives(summit, objectives),
                "objectives: the climb to the gantry on player inputs");
        require(step_off_gantry_with_objectives(summit, objectives, -187.5, 0.5),
                "objectives: off the gantry's edge and down onto the plate");
        advance_with_objectives(summit, objectives, 1.5);
        require(summit.summit_state().lit, "objectives: landing from the gantry must light the lamp");
        require(summit.summit_state().ending == static_cast<std::uint8_t>(Summit::Ending::Unfinished),
                "objectives: the lamp strike leaves the ending unfinished");
        objectives.update(summit);
        require(!objectives.done(Objectives::Id::Mission8TopOut),
                "objectives: the lamp is lit but the ending is short of Home, mission 8 is not done");
        require(objectives.done_count() == 0u, "objectives: no objective is done after the lamp strike alone");
        std::cout << "PASS scraperx_sim objectives mission 8 falsifier 2: ending="
                  << static_cast<int>(summit.summit_state().ending)
                  << " lit=" << summit.summit_state().lit << " done_count=" << objectives.done_count() << "\n";
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
        // The third bit round-trips with the rest.
        loaded.restore_bits(1u << static_cast<std::uint32_t>(Objectives::Id::Mission8TopOut));
        require(loaded.done(Objectives::Id::Mission8TopOut) && loaded.done_count() == 3u,
                "objectives: the third bit round-trips with the rest");
        // An old save carries no fields: every flag false.
        Objectives old_save;
        old_save.restore_bits(0);
        require(!old_save.done(Objectives::Id::Mission1ClockIn) &&
                    !old_save.done(Objectives::Id::Mission7BringPlantUp) &&
                    !old_save.done(Objectives::Id::Mission8TopOut) && old_save.done_count() == 0u,
                "objectives: an old save loads with every flag false");
        std::cout << "PASS scraperx_sim objectives save seam: bits round-trip, restore never clears\n";
    }
}

SCRAPERX_GROUP("objectives", 1110, run_objectives);
SCRAPERX_SLICE("objectives", run_objectives);
