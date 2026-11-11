// J-084 #402: one scenario plays the whole climb, from the ground to the
// summit, on the canonical route (rules 56, 27). Today no scenario does:
// run_stack ends at TP-340 through AS-007, run_ascent runs 154 to TP-640
// through AS-007, and the legs above 340 m are only ever played from their
// own spawns. This chains all twenty-one stages in one Simulation, the way
// a player would climb it, with real inputs and no teleports: the canonical
// Stack, Well, swing, C6, mast, pitman and helix to TP-340, then the
// non-canonical Plate Shop, Facade Crane, service lifts and climbs (AS-008
// to AS-010, the only way from 340 to 750 m today), and the owner's crown
// gondola and luffing derrick to the summit. Each stage it reaches is a
// checkpoint in the log; a stage it cannot pass prints where it stopped,
// with its numbers, and that list becomes the next jobs. Do not route
// around a broken stage: J-082 replaces the upper ladders band by band.

#include "sim_tests.hpp"

#include <string>

namespace {

// A leg that fails prints where it stopped, with the numbers, then fails
// the run: the stage, whether rule 56 calls it canonical, the height, the
// clock and the deaths.
bool require_leg(scraperx::sim::Simulation &s, const bool ok, const char *leg, const bool canonical) {
    if (!ok) {
        const auto state = s.snapshot();
        std::cerr << "canonical-ascent: stopped at " << leg << " ("
                  << (canonical ? "canonical" : "NON-CANONICAL") << "), y=" << state.player_position.y
                  << ", t=" << state.simulation_time_seconds << ", deaths=" << state.death_count << '\n';
    }
    require(ok, leg);
    return ok;
}

// The largest one-tick sideways step of the body over the whole climb, and
// the stage it came in: each checkpoint reports its own stage's worst and
// starts the next stage's from zero.
double g_worst_step_m = 0.0;
std::string g_worst_step_stage;

void checkpoint(scraperx::sim::Simulation &s, const double start, const char *stage, const bool canonical) {
    const auto state = s.snapshot();
    const auto her = s.matron_state();
    std::cout << "CHECKPOINT canonical-ascent " << stage << " y=" << state.player_position.y
              << " t=" << state.simulation_time_seconds - start << " deaths=" << state.death_count
              << " worst_step_m=" << g_path_watch.worst << " at (" << g_path_watch.worst_at.x << ", "
              << g_path_watch.worst_at.y << ", " << g_path_watch.worst_at.z
              << ") traversal=" << g_path_watch.worst_traversal << " her_state=" << int(her.state)
              << " her_y=" << her.position.y << " her_catches=" << her.catches
              << (canonical ? " canonical" : " NON-CANONICAL") << '\n';
    if (g_path_watch.worst > g_worst_step_m) {
        g_worst_step_m = g_path_watch.worst;
        g_worst_step_stage = stage;
    }
    g_path_watch.worst = 0.0;
}

// The whole route in one Simulation, from ExteriorGrade to the summit.
// Returns false at the first stage that cannot pass; the leg that fails
// already printed where it stopped, with its numbers.
bool climb_whole_route(scraperx::sim::Simulation &s, const double start) {
    using scraperx::sim::Snapshot;

    // ---- The canonical Stack: S1, C1, S2, C2, S3, C3, grade to deck 14 ----
    if (!require_leg(s, board_s1(s), "S1 boarded", true)) {
        return false;
    }
    if (!require_leg(s, ride_s1(s, 20.0).reached_top, "S1 to deck 2", true)) {
        return false;
    }
    if (!require_leg(s, walk_to(s, 10.0, -123.2, 6.0) && walk_to(s, 10.0, -126.0, 6.0), "off S1 onto deck 2",
                     true)) {
        return false;
    }
    checkpoint(s, start, "S1 deck 2", true);
    if (!require_leg(s, climb_c1(s), "C1 to deck 4", true)) {
        return false;
    }
    checkpoint(s, start, "C1 deck 4", true);
    if (!require_leg(s, shove_s2_bolt(s), "S2 bolt shoved", true)) {
        return false;
    }
    if (!require_leg(s, ride_s2(s, 20.0).reached_top, "S2 to deck 6", true)) {
        return false;
    }
    if (!require_leg(s, walk_to(s, 10.0, -133.0, 6.0) && walk_to(s, 10.0, -126.0, 6.0), "off S2 onto deck 6",
                     true)) {
        return false;
    }
    checkpoint(s, start, "S2 deck 6", true);
    if (!require_leg(s, climb_c2(s), "C2 to deck 8", true)) {
        return false;
    }
    checkpoint(s, start, "C2 deck 8", true);
    if (!require_leg(s, load_s3_pan(s), "S3 pig in its pan", true)) {
        return false;
    }
    if (!require_leg(s, ride_s3(s, 25.0).reached_top, "S3 to deck 12", true)) {
        return false;
    }
    if (!require_leg(s, walk_to(s, -8.0, -164.0, 6.0) && walk_to(s, -8.0, -171.2, 6.0),
                     "off S3 onto deck 12", true)) {
        return false;
    }
    checkpoint(s, start, "S3 deck 12", true);
    if (!require_leg(s, climb_c3(s), "C3 to deck 14", true)) {
        return false;
    }
    checkpoint(s, start, "C3 deck 14", true);

    // ---- The canonical Counterweight Well: A, B, C to the 220 ring --------
    if (!require_leg(s, board_well_a(s) && rig_well_a(s) && pull_well_a(s, 0.5, 3.0), "Well A rigged and tripped",
                     true)) {
        return false;
    }
    if (!require_leg(s, wait_for(s, 16.0, [](const Snapshot &state) {
                         return state.well_a_cage_travel >= kWellATravel - 0.01;
                     }),
                     "Well A to the 176 ring", true)) {
        return false;
    }
    (void)s.advance_frame(1.0);
    checkpoint(s, start, "Well A 176 ring", true);
    if (!require_leg(s, walk_to(s, -9.4, -131.2, 4.0) && walk_to(s, -7.6, -131.2, 4.0) && rig_well_b(s) &&
                         pull_well_b(s),
                     "across into Well B, rigged and tripped", true)) {
        return false;
    }
    if (!require_leg(s, wait_for(s, 16.0, [](const Snapshot &state) {
                         return state.well_b_cage_travel >= kWellATravel - kWellDogPitch - 0.01;
                     }),
                     "Well B to the 198 ring", true)) {
        return false;
    }
    (void)s.advance_frame(3.0);
    checkpoint(s, start, "Well B 198 ring", true);
    if (!require_leg(s, walk_to(s, -6.2, -131.9, 4.0) && walk_to(s, -4.4, -131.9, 4.0) &&
                         clear_well_c_chute(s) && fill_well_c(s, 10.0) && pull_well_c_latch(s, 3.0),
                     "across onto Well C, its chute cleared, filled and let go", true)) {
        return false;
    }
    if (!require_leg(s, wait_for(s, 16.0, [](const Snapshot &state) {
                         return state.well_c_platform_travel >= kWellATravel - kWellDogPitch - 0.01;
                     }),
                     "Well C to the 220 ring", true)) {
        return false;
    }
    (void)s.advance_frame(2.0);
    if (!require_leg(s, walk_to(s, -3.0, -129.0, 4.0), "off Well C onto the 220 ring", true)) {
        return false;
    }
    checkpoint(s, start, "Well C 220 ring", true);

    // ---- The canonical swing and west band: AS-012, C6, to the 264 ring --
    // The sideways watch is off for the swing (its gangway walk included, as
    // swing_ride walks it): strapped in, the seat carries the rider up its
    // 28.8 m arc to the 242 ring at the seat's own speed, about 21 m/s at the
    // bottom (0.24 m a tick, measured), a body carried, not a snap.
    g_path_watch.armed = false;
    const SwingRide swing = swing_ride(s);
    g_path_watch.armed = true;
    g_path_watch.primed = false;
    if (!require_leg(s, swing.on_242 && swing.end.death_count == 0 && swing.end.player_grounded &&
                         swing.end.player_position.y > 242.9 && swing.end.player_position.y < 243.4,
                     "AS-012 swing to the 242 ring", true)) {
        return false;
    }
    checkpoint(s, start, "AS-012 swing 242 ring", true);
    C6Notes c6{};
    if (!require_leg(s, climb_c6(s, &c6), "C6 to the 264 ring", true)) {
        return false;
    }
    if (!require_leg(s, c6.leap_caught && c6.balanced_out && c6.balanced_up,
                     "C6 leap caught, girders balanced out and up", true)) {
        return false;
    }
    checkpoint(s, start, "C6 264 ring", true);

    // ---- The canonical machines: mast, pitman, helix, to TP-340 ----------
    const MastRide mast = mast_ride(s);
    if (!require_leg(s, mast.on_exit && mast.end.death_count == 0 && mast.end.player_grounded &&
                         mast.end.player_position.y > 292.0 && mast.end.player_position.y < 292.4,
                     "cascade mast to 291.25", true)) {
        return false;
    }
    checkpoint(s, start, "cascade mast 291 ring", true);
    double pitman_s = 0.0;
    if (!require_leg(s, pitman_ride(s, &pitman_s), "pitman lift to 314.89", true)) {
        return false;
    }
    checkpoint(s, start, "pitman lift 315 ring", true);
    double helix_s = 0.0;
    if (!require_leg(s, helix_ride(s, &helix_s), "barrel helix to 338.89", true)) {
        return false;
    }
    checkpoint(s, start, "barrel helix 339 receiver", true);
    if (!require_leg(s, walk_to(s, -15.2, -149.0, 8.0) && c4_mantle(s, 1.0, 0.0, 340.6, "TP-340"),
                     "along the walkway east and a mantle onto TP-340", true)) {
        return false;
    }
    (void)walk_to(s, -12.0, -149.0, 4.0);
    (void)s.advance_frame(1.0);
    const auto at_340 = s.snapshot();
    if (!require_leg(s, at_340.death_count == 0 && at_340.player_grounded && at_340.player_position.y > 341.0 &&
                         at_340.player_position.y < 341.5,
                     "standing on TP-340, alive", true)) {
        return false;
    }
    checkpoint(s, start, "TP-340 (canonical route ends here)", true);

    // ---- Not canonical: the Plate Shop, G, H, I, and the ladder to 484 ---
    if (!require_leg(s, walk_to_shop_cleat(s) && rig_shop_g(s) && pull_shop_g(s), "Shop G rigged and its pin pulled",
                     false)) {
        return false;
    }
    if (!require_leg(s, wait_for(s, 60.0, [&](const Snapshot &) { return s.shop_state().g_platform_travel >= 33.7; }),
                     "Shop G to the 374 ring", false)) {
        return false;
    }
    (void)s.advance_frame(1.0);
    checkpoint(s, start, "Shop G 374 ring", false);
    if (!require_leg(s, walk_to(s, -14.5, -149.3, 8.0) && pull_shop_girder_pin(s) && board_shop_h(s) &&
                         pull_shop_chock(s),
                     "the girder's tail pin out, over the gangway onto Shop H, its chock pulled", false)) {
        return false;
    }
    if (!require_leg(s, wait_for(s, 90.0, [&](const Snapshot &) { return s.shop_state().h_platform_travel >= 43.9; }),
                     "Shop H to the 418 ring", false)) {
        return false;
    }
    (void)s.advance_frame(1.0);
    checkpoint(s, start, "Shop H 418 ring", false);
    if (!require_leg(s, rig_shop_i(s) && pull_shop_domino(s), "onto Shop I's cage, hooked on, the domino's pin pulled",
                     false)) {
        return false;
    }
    if (!require_leg(s, wait_for(s, 90.0, [&](const Snapshot &) { return s.shop_state().i_cage_travel >= 43.9; }),
                     "Shop I to 462.25", false)) {
        return false;
    }
    (void)s.advance_frame(1.0);
    if (!require_leg(s, walk_to(s, -8.1, -145.6, 8.0) && walk_to(s, -8.1, -147.3, 8.0) &&
                         walk_to(s, -7.7, -147.85, 8.0) &&
                         climb_wet_hold(s, -7.6, -148.1, -1.0, 0.0, false, 484.5),
                     "up the ladder onto the 484 ring", false)) {
        return false;
    }
    checkpoint(s, start, "Plate Shop 484 ring", false);

    // ---- Not canonical: the Facade Crane Stack, J, K, L, to TP-640 --------
    if (!require_leg(s, climb_crane_band(s), "Facade Crane J, K and L to standing on TP-640", false)) {
        return false;
    }
    checkpoint(s, start, "Facade Crane TP-640", false);

    // ---- Not canonical: the service lifts and climbs, TP-640 to 750 -------
    if (!require_leg(s, rig_service_m(s), "M's shackle hooked on the cage's eye", false)) {
        return false;
    }
    if (!require_leg(s, pull_service_chock(s), "M's reel chock pulled, hooked on", false)) {
        return false;
    }
    const ServiceRide m_ride = ride_service_m(s);
    if (!require_leg(s, m_ride.settled, "M's cage at rest on its dogs", false)) {
        return false;
    }
    if (!require_leg(s, walk_to(s, -5.8, -143.5, 6.0), "off M's cage east onto the 662 deck", false)) {
        return false;
    }
    (void)s.advance_frame(1.0);
    checkpoint(s, start, "Service M 662 deck", false);
    C4Notes c4{};
    if (!require_leg(s, climb_c4(s, &c4), "C4 to the 684 deck", false)) {
        return false;
    }
    if (!require_leg(s, c4.balanced && !c4.house_offered && !c4.house_caught && c4.shimmied >= 3.0,
                     "C4 beam balanced, winch house offered and caught nothing, hang shimmied past it", false)) {
        return false;
    }
    checkpoint(s, start, "C4 684 deck", false);
    if (!require_leg(s, open_n_flap(s), "N's flap drawn past its dead point, fallen open", false)) {
        return false;
    }
    const NRide n_ride = ride_service_n(s, true);
    if (!require_leg(s, n_ride.settled, "N's cage at rest on its dogs", false)) {
        return false;
    }
    if (!require_leg(s, step_off_n(s), "off N's cage onto the 706 deck", false)) {
        return false;
    }
    checkpoint(s, start, "Service N 706 deck", false);
    C5Notes c5{};
    if (!require_leg(s, climb_c5(s, &c5), "C5 to the 728 deck", false)) {
        return false;
    }
    if (!require_leg(s, c5.vaulted && c5.crawled, "C5 manifold vaulted, ducts crawled under", false)) {
        return false;
    }
    checkpoint(s, start, "C5 728 deck", false);
    double o_freed_at = -1.0;
    if (!require_leg(s, pull_o_handle(s, &o_freed_at), "O's chock drawn clear of the ram", false)) {
        return false;
    }
    const ORide o_ride = ride_service_o(s, true, o_freed_at);
    if (!require_leg(s, o_ride.settled, "O's cab at rest on its dogs", false)) {
        return false;
    }
    if (!require_leg(s, step_off_o(s), "off O's cab onto the 750 deck", false)) {
        return false;
    }
    checkpoint(s, start, "Service O 750 deck", false);

    // ---- Canonical again: the owner's crown gondola and luffing derrick ---
    const HighRide high = high_ride(s);
    const auto &e = high.end;
    if (!require_leg(s, high.vaulted, "vault over the 750 deck's east parapet onto the crown's receiver", true)) {
        return false;
    }
    if (!require_leg(s, high.crown_up && high.on_crown_exit, "crown gondola's cabin to its upper receiver at 778.09",
                     true)) {
        return false;
    }
    if (!require_leg(s, high.derrick_up, "derrick's cradle to its upper receiver at 802.64", true)) {
        return false;
    }
    if (!require_leg(s, e.death_count == 0 && e.player_grounded &&
                         e.support_entity_id == scraperx::sim::vertical::Route::kDerrickStaticFirst + 2 &&
                         e.player_position.y > 803.3 && e.player_position.y < 803.8,
                     "standing on the derrick's upper receiver, about 800 m up, alive", true)) {
        return false;
    }
    if (!require_leg(s, e.summit_reached && e.summit_deaths == 0 && e.summit_seconds > 0.0,
                     "the derrick's upper receiver reads as the route's top", true)) {
        return false;
    }
    if (!require_leg(s, e.support_stumble_count >= 1 && e.support_jolt_peak_m > 0.15,
                     "the gondola's swinging cabin jolted its rider into a stumble", true)) {
        return false;
    }
    checkpoint(s, start, "luffing derrick summit receiver", true);
    return true;
}

// One whole climb from the game's start: every stage, the body's worst
// one-tick sideways step, no death. `name` heads its PASS and FAIL lines.
void climb_from_grade(scraperx::sim::Simulation &s, const char *name) {
    (void)s.advance_frame(0.5);
    const double start = s.snapshot().simulation_time_seconds;
    g_path_watch = PathWatch{};
    g_path_watch.armed = true;
    g_worst_step_m = 0.0;
    g_worst_step_stage.clear();
    const bool climbed = climb_whole_route(s, start);
    g_path_watch.armed = false;
    require(climbed, "canonical-ascent: the whole climb, ground to summit");
    const auto top = s.snapshot();
    const auto her = s.matron_state();
    std::cout << "  " << name << ": worst_step_m=" << g_worst_step_m << " in " << g_worst_step_stage
              << " her_state=" << int(her.state) << " her_alive=" << her.alive << " her_y=" << her.position.y
              << " her_catches=" << her.catches << '\n';
    require(g_worst_step_m <= 0.15, "canonical-ascent: the body must never move more than 0.15 m sideways in one tick");
    require(top.death_count == 0, "canonical-ascent: ground to summit without dying");
    std::cout << "PASS scraperx_sim " << name << ": seconds=" << top.simulation_time_seconds - start
              << " top_y=" << top.player_position.y << " worst_step_m=" << g_worst_step_m << '\n';
}

}  // namespace

// The whole climb with the Matron off: switched off before the first tick,
// so the rider climbs alone.
void run_canonical_ascent() {
    scraperx::sim::Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
    s.debug_matron_off();
    climb_from_grade(s, "canonical-ascent");
    require(!s.matron_state().alive && s.matron_state().catches == 0,
            "canonical-ascent: switched off, she must stay off the whole climb");
}

// The whole climb with the Matron on, as the game builds her: in her locker
// on deck 1, woken by what she hears. The bar is the same climb; where she
// stops it, the leg prints the stage and its numbers.
void run_canonical_ascent_matron() {
    scraperx::sim::Simulation s(scraperx::sim::InitialSpawn::ExteriorGrade);
    climb_from_grade(s, "canonical-ascent-matron");
}

SCRAPERX_SLICE("canonical-ascent", run_canonical_ascent);
SCRAPERX_SLICE("canonical-ascent-matron", run_canonical_ascent_matron);
