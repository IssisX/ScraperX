// J-134 #460: measure her chase above 220 m, band by band (F5).
// A measurement, not a fix: one whole-climb run with her on as the game
// builds her (locker on deck 1, Dormant at grade — no launch, no debug
// placement), from grade to TP-340 on the canonical route, the way a
// player would climb it, with real inputs and no teleports. For each band
// (each deck or machine stage) the run reports: did she get within 10 m
// of him, her closest distance, and what stopped her.
//
// How it is measured: a test-only per-tick hook (Simulation's
// debug_set_step_hook) samples every fixed tick — her distance to him
// (centre to centre), her state, her height, her journey — including
// inside the ride helpers' own loops and batched advances, where a
// leg-boundary sample would miss her closing at 6.8 m/s. The hook only
// reads state; it changes no gameplay. The bands are the legs the driver
// is on, not height guesses: the driver sets the band before each leg.
// One tick is excepted from the never-carried-or-snapped check and
// recorded on its own: her emergence, where spawn_body() places a body
// that did not exist in the world at the locker's mouth, ~0.95 m in front
// of where the view reported her inside the locker. That placement is
// measured and printed, not hidden; the step check covers her movement
// under her own physics, as the pursuit tests' baseline-after-placement
// does.
//
// What the table can say: "asleep" (Dormant the whole band — he may still
// walk past her locker; that is his approach, not her chase, and "closest"
// counts only Hunting or Searching), "still hunting" (she was hunting
// when he left the band), "lost his scent" (she stopped hunting in the
// band and went back to the patrol), "no way up" (she hunted for 5 s and
// gained no height, or a move failed), "stuck on a body" (Hunting for
// 5 s without moving half a metre), "caught him". The evidence behind
// each name is printed with the row, so the name is auditable. A catch
// stops the climb: the table is printed to the catch band and the run
// passes with that noted, because a catch answers the question (she
// hunts) — it is not a climb failure. A death fails the run: the climb
// is supposed to complete with none, and the table would be incomplete.

#include "sim_tests.hpp"

#include "sim/matron.hpp"

#include <cmath>
#include <cstdint>

namespace {

using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

// Each deck or machine stage on the canonical route to TP-340, in order.
struct Band {
    const char *name;
    const char *heights;
};
constexpr Band kBands[] = {
    {"S1 water-balance hoist", "grade to deck 2 (22 m)"},
    {"C1 facade climb", "deck 2 to deck 4 (44 m)"},
    {"S2 walking-beam hoist", "44 to 66 m"},
    {"C2 east machinery hall", "66 to 88 m"},
    {"S3 counterweight hoist", "88 to 132 m"},
    {"C3 crown truss", "132 to 154 m"},
    {"Well A", "154 to the 176 ring"},
    {"Well B", "176 to the 198 ring"},
    {"Well C", "198 to the 220 ring"},
    {"AS-012 swing", "220 to the 242 ring"},
    {"C6 west band", "242 to the 264 ring"},
    {"cascade mast", "264 to 291.25 m"},
    {"pitman lift", "291.25 to 314.89 m"},
    {"barrel helix", "314.89 to 338.89 m"},
    {"TP-340 mantle", "338.89 to TP-340"},
};
constexpr std::size_t kBandCount = sizeof(kBands) / sizeof(kBands[0]);

// Her state names, by Matron::State (0..18).
const char *state_name(const std::uint8_t state) {
    static const char *kNames[] = {"Dormant",   "Straining", "Emerging", "Hunting",  "Searching",
                                   "Shuffling", "Dead",      "Guarding", "Peering",  "Unrigging",
                                   "Staggering", "Down",     "Grab",     "Carry",    "Dangle",
                                   "ArmPlay",   "Release",   "Spare",    "Claimed"};
    return state < sizeof(kNames) / sizeof(kNames[0]) ? kNames[state] : "unknown";
}

struct BandRow {
    bool measured = false;
    std::uint64_t ticks = 0;
    // Closest she ever was, any state (he can walk past her locker while
    // she sleeps: that is his approach, not her chase).
    double min_dist = 1.0e9;
    std::uint8_t state_at_min = 0;
    // Closest she got while chasing him (Hunting or Searching): the
    // number the "within 10 m" column judges. Unset if she never chased.
    double min_dist_chasing = 1.0e9;
    double min_dist_chasing_t = 0.0;
    std::uint8_t state_at_min_chasing = 0;
    double her_y_at_min_chasing = 0.0;
    double min_dist_t = 0.0;
    double her_y_at_min = 0.0;
    double her_y_min = 1.0e9;
    double her_y_max = -1.0e9;
    std::uint32_t states_seen = 0;
    std::uint64_t hunting_ticks = 0;
    std::uint64_t searching_ticks = 0;
    std::uint64_t shuffling_ticks = 0;
    std::uint8_t last_state = 0;
    bool journey_active_seen = false;
    std::uint32_t moves_failed_first = 0;
    std::uint32_t moves_failed_last = 0;
    std::uint32_t arrivals_first = 0;
    std::uint32_t arrivals_last = 0;
    double hunt_first_x = 0.0;
    double hunt_first_y = 0.0;
    double hunt_first_z = 0.0;
    double hunt_last_x = 0.0;
    double hunt_last_y = 0.0;
    double hunt_last_z = 0.0;
    std::uint64_t catches_first = 0;
    std::uint64_t catches_last = 0;
};

// The per-tick sampler: one watch for the whole climb, fed by the sim's
// debug step hook after every fixed step.
struct ChaseWatch {
    Simulation *sim = nullptr;
    int band = -1;
    int last_band = -2;
    BandRow rows[kBandCount];
    double worst_her_step = 0.0;
    double worst_her_step_t = 0.0;
    Vector3 worst_her_step_at{};
    // Her emergence: the body does not exist in the world until
    // spawn_body() places it at the locker's mouth (kSpawnZ, ~0.95 m in
    // front of where view() reported her inside the locker). That one
    // placement is not a step she took, so the sampler baselines past it
    // and records it here instead of in worst_her_step.
    double spawn_step = 0.0;
    double spawn_step_t = 0.0;
    Vector3 spawn_step_at{};
    bool spawn_seen = false;
    std::uint8_t prev_state = 0;
    Vector3 last_her{};
    bool have_last_her = false;
    std::uint64_t catches0 = 0;
};
ChaseWatch g_chase_watch;

void chase_sample(void *context) noexcept {
    ChaseWatch *w = static_cast<ChaseWatch *>(context);
    if (w->sim == nullptr || w->band < 0 || w->band >= static_cast<int>(kBandCount)) {
        return;
    }
    const auto snap = w->sim->snapshot();
    const auto m = w->sim->matron_state();
    BandRow &row = w->rows[static_cast<std::size_t>(w->band)];
    if (w->band != w->last_band) {
        // First tick of a new band: entry values for the stop rule.
        const auto journey = w->sim->matron_journey();
        row.moves_failed_first = journey.moves_failed;
        row.arrivals_first = journey.arrivals;
        row.catches_first = m.catches;
        w->last_band = w->band;
    }
    row.measured = true;
    ++row.ticks;
    const double dist = std::hypot(m.position.x - snap.player_position.x, m.position.y - snap.player_position.y,
                                   m.position.z - snap.player_position.z);
    if (dist < row.min_dist) {
        row.min_dist = dist;
        row.min_dist_t = snap.simulation_time_seconds;
        row.state_at_min = m.state;
        row.her_y_at_min = m.position.y;
    }
    // Her chase is Hunting or Searching: only those states count toward
    // "she got within 10 m of him". Straining, Emerging and Shuffling are
    // not a chase, and Dormant is asleep.
    if (m.state == static_cast<std::uint8_t>(Matron::State::Hunting) ||
        m.state == static_cast<std::uint8_t>(Matron::State::Searching)) {
        if (dist < row.min_dist_chasing) {
            row.min_dist_chasing = dist;
            row.min_dist_chasing_t = snap.simulation_time_seconds;
            row.state_at_min_chasing = m.state;
            row.her_y_at_min_chasing = m.position.y;
        }
    }
    row.her_y_min = std::min(row.her_y_min, m.position.y);
    row.her_y_max = std::max(row.her_y_max, m.position.y);
    if (m.state < 32) {
        row.states_seen |= (1u << m.state);
    }
    row.last_state = m.state;
    if (m.state == static_cast<std::uint8_t>(Matron::State::Hunting)) {
        ++row.hunting_ticks;
        if (row.hunting_ticks == 1) {
            row.hunt_first_x = m.position.x;
            row.hunt_first_y = m.position.y;
            row.hunt_first_z = m.position.z;
        }
        row.hunt_last_x = m.position.x;
        row.hunt_last_y = m.position.y;
        row.hunt_last_z = m.position.z;
    } else if (m.state == static_cast<std::uint8_t>(Matron::State::Searching)) {
        ++row.searching_ticks;
    } else if (m.state == static_cast<std::uint8_t>(Matron::State::Shuffling)) {
        ++row.shuffling_ticks;
    }
    const auto journey = w->sim->matron_journey();
    row.journey_active_seen = row.journey_active_seen || journey.active;
    row.moves_failed_last = journey.moves_failed;
    row.arrivals_last = journey.arrivals;
    row.catches_last = m.catches;
    if (w->have_last_her) {
        const double step = std::hypot(m.position.x - w->last_her.x, m.position.y - w->last_her.y,
                                       m.position.z - w->last_her.z);
        // Her emergence places the body at the locker's mouth; the step
        // from the locker's interior point is the placement, not a move.
        if (w->prev_state == static_cast<std::uint8_t>(Matron::State::Straining) &&
            m.state == static_cast<std::uint8_t>(Matron::State::Emerging)) {
            w->spawn_step = step;
            w->spawn_step_t = snap.simulation_time_seconds;
            w->spawn_step_at = m.position;
            w->spawn_seen = true;
        } else if (step > w->worst_her_step) {
            w->worst_her_step = step;
            w->worst_her_step_t = snap.simulation_time_seconds;
            w->worst_her_step_at = m.position;
        }
    }
    w->last_her = m.position;
    w->have_last_her = true;
    w->prev_state = m.state;
}

// What stopped her in this band, from the row's measured facts. The
// journey is not the hunt's mechanism — she hunts by direct steering, so
// a journey never going active says nothing about her way up; what says
// it is whether she gained height, whether a move failed, and whether she
// was still hunting when he left the band. "Asleep" is no action at all:
// she never hunted, searched or shuffled in the band, whatever she was
// doing inside the locker (Dormant, Straining).
const char *stop_name(const BandRow &row) {
    if (row.catches_last > row.catches_first) {
        return "caught him";
    }
    if (row.hunting_ticks == 0 && row.searching_ticks == 0 && row.shuffling_ticks == 0) {
        return "asleep";
    }
    if (row.min_dist_chasing <= 10.0) {
        return "chasing him (within 10 m)";
    }
    if (row.hunting_ticks > 0) {
        const double hunt_move = std::hypot(row.hunt_last_x - row.hunt_first_x, row.hunt_last_y - row.hunt_first_y,
                                            row.hunt_last_z - row.hunt_first_z);
        if (row.hunting_ticks >= 5 * static_cast<std::uint64_t>(Simulation::kTickRateHz) && hunt_move < 0.5) {
            return "stuck on a body";
        }
        if (row.moves_failed_last > row.moves_failed_first) {
            return "no way up (a move failed)";
        }
        if (row.hunting_ticks >= 5 * static_cast<std::uint64_t>(Simulation::kTickRateHz) &&
            row.her_y_max - row.her_y_min < 0.5) {
            return "no way up (hunted, gained no height)";
        }
        if (row.last_state == static_cast<std::uint8_t>(Matron::State::Hunting)) {
            return "still hunting (he left the band)";
        }
        return "lost his scent";
    }
    if (row.searching_ticks > 0 || row.shuffling_ticks > 0) {
        return "lost his scent";
    }
    return "unclassified";
}

void print_table(const bool caught, const int caught_band) {
    std::cout << "  chase-bands table (her on, whole climb grade to TP-340; distances centre to centre, "
                 "\"closest\" is while chasing: Hunting or Searching):\n";
    if (g_chase_watch.spawn_seen) {
        std::cout << "  spawn placement " << g_chase_watch.spawn_step << " m at t=" << g_chase_watch.spawn_step_t
                  << " s at (" << g_chase_watch.spawn_step_at.x << ", " << g_chase_watch.spawn_step_at.y << ", "
                  << g_chase_watch.spawn_step_at.z << "): her body placed at the locker's mouth on emergence, "
                  << "not a step she took\n";
    }
    std::cout << "  | band | within 10 m | closest | stopped by |\n";
    for (std::size_t b = 0; b < kBandCount; ++b) {
        const BandRow &row = g_chase_watch.rows[b];
        if (!row.measured) {
            std::cout << "  | " << kBands[b].name << " (" << kBands[b].heights << ") | not reached | - | "
                      << (caught && static_cast<int>(b) > caught_band ? "not reached (he was caught)"
                                                                      : "not reached (climb stopped)")
                      << " |\n";
            continue;
        }
        std::cout << "  | " << kBands[b].name << " (" << kBands[b].heights << ") | "
                  << (row.min_dist_chasing <= 10.0 ? "yes" : "no") << " | ";
        if (row.min_dist_chasing > 1.0e8) {
            std::cout << "never chased";
        } else {
            std::cout << row.min_dist_chasing << " m";
        }
        std::cout << " | " << stop_name(row) << " |\n";
        std::cout << "    evidence: ticks " << row.ticks << ", closest any state " << row.min_dist << " m at t="
                  << row.min_dist_t << " s her state " << state_name(row.state_at_min) << " her y " << row.her_y_at_min
                  << " m; closest chasing";
        if (row.min_dist_chasing > 1.0e8) {
            std::cout << " never";
        } else {
            std::cout << " " << row.min_dist_chasing << " m at t=" << row.min_dist_chasing_t << " s her state "
                      << state_name(row.state_at_min_chasing) << " her y " << row.her_y_at_min_chasing << " m";
        }
        std::cout << "; her y range " << row.her_y_min << ".." << row.her_y_max << " m, hunting " << row.hunting_ticks
                  << " ticks, searching " << row.searching_ticks << ", shuffling " << row.shuffling_ticks
                  << ", states seen";
        bool first_state = true;
        for (std::uint8_t s = 0; s < 19; ++s) {
            if ((row.states_seen & (1u << s)) != 0) {
                std::cout << (first_state ? " " : ", ") << state_name(s);
                first_state = false;
            }
        }
        std::cout << ", journey active " << row.journey_active_seen << ", moves failed " << row.moves_failed_first
                  << "->" << row.moves_failed_last << ", arrivals " << row.arrivals_first << "->" << row.arrivals_last
                  << ", catches " << row.catches_first << "->" << row.catches_last << '\n';
    }
    std::cout << "  worst her step " << g_chase_watch.worst_her_step << " m at t=" << g_chase_watch.worst_her_step_t
              << " s at (" << g_chase_watch.worst_her_step_at.x << ", " << g_chase_watch.worst_her_step_at.y << ", "
              << g_chase_watch.worst_her_step_at.z << ")\n";
}

// A leg that fails prints where it stopped, with its numbers, then fails
// the run: the stage, the height, the clock and the deaths.
bool require_leg(Simulation &s, const bool ok, const char *leg) {
    if (!ok) {
        const auto state = s.snapshot();
        std::cerr << "chase-bands: stopped at " << leg << ", y=" << state.player_position.y
                  << ", t=" << state.simulation_time_seconds << ", deaths=" << state.death_count << '\n';
    }
    return ok;
}

struct ClimbResult {
    bool completed = false;
    bool caught = false;
    int caught_band = -1;
    const char *failed_leg = nullptr;
};

// The whole canonical route, grade to TP-340, with her on. The band is set
// before each leg, so every tick the hook samples belongs to the stage he
// is on. Returns at the first leg that cannot pass, or when she catches
// him, or on TP-340.
ClimbResult climb_to_340(Simulation &s) {
    using scraperx::sim::Snapshot;
    ClimbResult result;
    const auto check = [&](const char *leg) {
        const auto snap = s.snapshot();
        if (s.matron_state().catches > g_chase_watch.catches0) {
            result.caught = true;
            result.caught_band = g_chase_watch.band;
            return false;
        }
        if (snap.death_count > 0) {
            result.failed_leg = leg;
            return false;
        }
        return true;
    };

    // ---- The canonical Stack: S1, C1, S2, C2, S3, C3, grade to deck 14 ----
    g_chase_watch.band = 0;
    if (!require_leg(s, board_s1(s), "S1 boarded") || !check("S1 boarded")) return result;
    if (!require_leg(s, ride_s1(s, 20.0).reached_top, "S1 to deck 2") || !check("S1 to deck 2")) return result;
    if (!require_leg(s, walk_to(s, 10.0, -123.2, 6.0) && walk_to(s, 10.0, -126.0, 6.0), "off S1 onto deck 2") ||
        !check("off S1 onto deck 2"))
        return result;
    g_chase_watch.band = 1;
    if (!require_leg(s, climb_c1(s), "C1 to deck 4") || !check("C1 to deck 4")) return result;
    g_chase_watch.band = 2;
    if (!require_leg(s, shove_s2_bolt(s), "S2 bolt shoved") || !check("S2 bolt shoved")) return result;
    if (!require_leg(s, ride_s2(s, 20.0).reached_top, "S2 to deck 6") || !check("S2 to deck 6")) return result;
    if (!require_leg(s, walk_to(s, 10.0, -133.0, 6.0) && walk_to(s, 10.0, -126.0, 6.0), "off S2 onto deck 6") ||
        !check("off S2 onto deck 6"))
        return result;
    g_chase_watch.band = 3;
    if (!require_leg(s, climb_c2(s), "C2 to deck 8") || !check("C2 to deck 8")) return result;
    g_chase_watch.band = 4;
    if (!require_leg(s, load_s3_pan(s), "S3 pig in its pan") || !check("S3 pig in its pan")) return result;
    if (!require_leg(s, ride_s3(s, 25.0).reached_top, "S3 to deck 12") || !check("S3 to deck 12")) return result;
    if (!require_leg(s, walk_to(s, -8.0, -164.0, 6.0) && walk_to(s, -8.0, -171.2, 6.0), "off S3 onto deck 12") ||
        !check("off S3 onto deck 12"))
        return result;
    g_chase_watch.band = 5;
    if (!require_leg(s, climb_c3(s), "C3 to deck 14") || !check("C3 to deck 14")) return result;

    // ---- The canonical Counterweight Well: A, B, C to the 220 ring --------
    g_chase_watch.band = 6;
    if (!require_leg(s, board_well_a(s) && rig_well_a(s) && pull_well_a(s, 0.5, 3.0), "Well A rigged and tripped") ||
        !check("Well A rigged and tripped"))
        return result;
    if (!require_leg(s, wait_for(s, 16.0, [](const Snapshot &state) {
                         return state.well_a_cage_travel >= kWellATravel - 0.01;
                     }),
                     "Well A to the 176 ring") ||
        !check("Well A to the 176 ring"))
        return result;
    (void)s.advance_frame(1.0);
    g_chase_watch.band = 7;
    if (!require_leg(s, walk_to(s, -9.4, -131.2, 4.0) && walk_to(s, -7.6, -131.2, 4.0) && rig_well_b(s) &&
                         pull_well_b(s),
                     "across into Well B, rigged and tripped") ||
        !check("across into Well B, rigged and tripped"))
        return result;
    if (!require_leg(s, wait_for(s, 16.0, [](const Snapshot &state) {
                         return state.well_b_cage_travel >= kWellATravel - kWellDogPitch - 0.01;
                     }),
                     "Well B to the 198 ring") ||
        !check("Well B to the 198 ring"))
        return result;
    (void)s.advance_frame(3.0);
    g_chase_watch.band = 8;
    if (!require_leg(s, walk_to(s, -6.2, -131.9, 4.0) && walk_to(s, -4.4, -131.9, 4.0) && clear_well_c_chute(s) &&
                         fill_well_c(s, 10.0) && pull_well_c_latch(s, 3.0),
                     "across onto Well C, its chute cleared, filled and let go") ||
        !check("across onto Well C, its chute cleared, filled and let go"))
        return result;
    if (!require_leg(s, wait_for(s, 16.0, [](const Snapshot &state) {
                         return state.well_c_platform_travel >= kWellATravel - kWellDogPitch - 0.01;
                     }),
                     "Well C to the 220 ring") ||
        !check("Well C to the 220 ring"))
        return result;
    (void)s.advance_frame(2.0);
    if (!require_leg(s, walk_to(s, -3.0, -129.0, 4.0), "off Well C onto the 220 ring") ||
        !check("off Well C onto the 220 ring"))
        return result;

    // ---- The canonical swing and west band: AS-012, C6, to the 264 ring --
    g_chase_watch.band = 9;
    const SwingRide swing = swing_ride(s);
    if (!require_leg(s, swing.on_242 && swing.end.death_count == 0 && swing.end.player_grounded &&
                         swing.end.player_position.y > 242.9 && swing.end.player_position.y < 243.4,
                     "AS-012 swing to the 242 ring") ||
        !check("AS-012 swing to the 242 ring"))
        return result;
    g_chase_watch.band = 10;
    C6Notes c6{};
    if (!require_leg(s, climb_c6(s, &c6), "C6 to the 264 ring") || !check("C6 to the 264 ring")) return result;
    if (!require_leg(s, c6.leap_caught && c6.balanced_out && c6.balanced_up,
                     "C6 leap caught, girders balanced out and up") ||
        !check("C6 leap caught, girders balanced out and up"))
        return result;

    // ---- The canonical machines: mast, pitman, helix, to TP-340 ----------
    g_chase_watch.band = 11;
    const MastRide mast = mast_ride(s);
    if (!require_leg(s, mast.on_exit && mast.end.death_count == 0 && mast.end.player_grounded &&
                         mast.end.player_position.y > 292.0 && mast.end.player_position.y < 292.4,
                     "cascade mast to 291.25") ||
        !check("cascade mast to 291.25"))
        return result;
    g_chase_watch.band = 12;
    double pitman_s = 0.0;
    if (!require_leg(s, pitman_ride(s, &pitman_s), "pitman lift to 314.89") || !check("pitman lift to 314.89"))
        return result;
    g_chase_watch.band = 13;
    double helix_s = 0.0;
    if (!require_leg(s, helix_ride(s, &helix_s), "barrel helix to 338.89") || !check("barrel helix to 338.89"))
        return result;
    g_chase_watch.band = 14;
    if (!require_leg(s, walk_to(s, -15.2, -149.0, 8.0) && c4_mantle(s, 1.0, 0.0, 340.6, "TP-340"),
                     "along the walkway east and a mantle onto TP-340") ||
        !check("along the walkway east and a mantle onto TP-340"))
        return result;
    (void)walk_to(s, -12.0, -149.0, 4.0);
    (void)s.advance_frame(1.0);
    const auto at_340 = s.snapshot();
    if (!require_leg(s, at_340.death_count == 0 && at_340.player_grounded && at_340.player_position.y > 341.0 &&
                         at_340.player_position.y < 341.5,
                     "standing on TP-340, alive") ||
        !check("standing on TP-340, alive"))
        return result;
    result.completed = true;
    return result;
}

} // namespace

// The whole climb with the Matron on as the game builds her: she starts
// Dormant in her locker on deck 1 and wakes on his noise, or not. The bar
// is the climb itself; the table is the measurement.
void run_matron_chase_bands() {
    Simulation s(InitialSpawn::ExteriorGrade);
    g_chase_watch = ChaseWatch{};
    g_chase_watch.sim = &s;
    g_chase_watch.catches0 = s.matron_state().catches;
    g_chase_watch.band = 0;
    s.debug_set_step_hook(&chase_sample, &g_chase_watch);
    (void)s.advance_frame(0.5);
    const double start = s.snapshot().simulation_time_seconds;
    const ClimbResult climbed = climb_to_340(s);
    s.debug_set_step_hook(nullptr, nullptr);
    print_table(climbed.caught, climbed.caught_band);
    if (climbed.caught) {
        std::cout << "PASS scraperx_sim matron chase bands: she caught him in band " << climbed.caught_band << " ("
                  << kBands[static_cast<std::size_t>(climbed.caught_band)].name << "); table to the catch band\n";
        return;
    }
    require(climbed.completed, "matron chase bands: the whole climb, grade to TP-340, with her hunting");
    for (std::size_t b = 0; b < kBandCount; ++b) {
        require(g_chase_watch.rows[b].measured, "matron chase bands: every band to 340 m has a measured row");
    }
    require(g_chase_watch.worst_her_step < 0.2, "matron chase bands: never carried or snapped (a step a tick)");
    const auto top = s.snapshot();
    require(top.death_count == 0, "matron chase bands: grade to TP-340 without dying");
    std::cout << "PASS scraperx_sim matron chase bands: seconds=" << top.simulation_time_seconds - start
              << " top_y=" << top.player_position.y << " worst_her_step_m=" << g_chase_watch.worst_her_step << '\n';
}
SCRAPERX_SLICE("matron-chase-bands", run_matron_chase_bands);
