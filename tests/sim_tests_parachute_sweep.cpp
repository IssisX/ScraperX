// J-124 (issue #446, rule 51): the parachute is the way back from every
// height. An exam, not a single case: at each 22 m ring and each machine
// deck, on the faces he can reach, he runs off the edge, opens the canopy
// by the real control 1.0 s after leaving the ground (the normal moment,
// close to the summit arm's vy < -12 rule), and is steered by the stick
// only (full stick outward, away from the tower, the whole descent). Per
// case the exam records the landing place and speed, death or not, every
// steel contact during the descent (max hit_closing_mps, the per-tick
// hardest side-contact closing speed, floor excluded), and the time to
// ground.
//
// A case PASSES when he lands alive (deaths 0, grounded) with no steel
// contact harder than 1.0 m/s closing, no landing inside the second
// tower's footprint (z <= -224, its face), and a landing impact under
// 12 m/s (the ankle-break speed; a chuted landing is ~9). Faces with no
// standable edge (NO-STAND), or an edge he cannot run off within 6 s
// (BLOCKED: a rail or beam he cannot cross), are recorded in the table,
// not counted as failures: they are not jump edges, and whether the
// parachute must reach them is a ruling, not a measurement. The table is
// the deliverable; the test fails only on a runnable case that dies,
// snags, crosses into the second tower, or lands too hard.
//
// Placement is by debug_launch_player (the only way to stand on 41
// levels in one exam); the run-off, the canopy, and the descent are all
// the real controls. The edge is found by probing, not by hardcoded
// extents: the ray finds the last supported radius at the level's top,
// on the face centerline first, then offsets around a cage or machine.
// Compass: north is +z (bay.hpp kNorthZ = -124, away from the second
// tower), south -z (toward it), west -x (the wind side), east +x.
#include "sim_tests.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;
using scraperx::sim::Vector3;

struct Face {
    const char *name;
    double dx;
    double dz;
};
constexpr Face kFaces[] = {
    {"N", 0.0, 1.0},
    {"S", 0.0, -1.0},
    {"E", 1.0, 0.0},
    {"W", -1.0, 0.0},
};
constexpr double kTowerCentreX = 0.0;
constexpr double kTowerCentreZ = -150.0;
// The second tower's face (twin_tower.hpp kFaceZ): a landing at or
// beyond it is a landing in the second tower.
constexpr double kSecondTowerFaceZ = -224.0;
// A steel contact at or above this closing speed is a snag, not a brush.
constexpr double kSnagClosingMps = 1.0;
// A chuted landing at or above this is a broken ankle, not a walk-away.
constexpr double kWalkAwayImpactMps = 12.0;
constexpr double kCanopyDelaySeconds = 1.0;
constexpr double kRunOffSeconds = 6.0;
constexpr double kDescentSeconds = 240.0;

struct CaseResult {
    double level = 0.0;
    const char *face = "";
    const char *status = "NO-STAND";
    double land_x = 0.0;
    double land_y = 0.0;
    double land_z = 0.0;
    double impact_mps = 0.0;
    double max_hit_mps = 0.0;
    double worst_hit_x = 0.0;
    double worst_hit_y = 0.0;
    double worst_hit_z = 0.0;
    int hit_ticks = 0;
    double time_to_ground_s = 0.0;
    double wind_peak_n = 0.0;
    bool opened = false;
    bool held_by_matron = false;
};

// The last supported radius along one line out from the tower center, at
// the level's top: the probe ray starts 2 m above the top and accepts a
// hit within 0.5 m of it, so a machine on the band (hit above the top) or
// a brace below it (hit below) does not count as the deck.
bool probe_edge_line(Simulation &s, const double top, const Face &face, const double lateral,
                     double &edge_r, double &edge_support) {
    bool found = false;
    for (double r = 2.0; r <= 40.0; r += 0.5) {
        const double x = kTowerCentreX + face.dx * r - face.dz * lateral;
        const double z = kTowerCentreZ + face.dz * r + face.dx * lateral;
        const double support = s.debug_support_y(Vector3{x, top + 2.0, z}, 4.0);
        if (std::isnan(support) || std::fabs(support - top) > 0.5) continue;
        edge_r = r;
        edge_support = support;
        found = true;
    }
    return found;
}

// Stand 3.5 m inside the edge with a run-up, on the face centerline
// first, then around a cage or machine. The settle check proves the spot
// is really standable: grounded, unmoved, at the probed support height.
bool find_stand(Simulation &s, const double top, const Face &face, double &stand_x, double &stand_z,
                double &stand_support) {
    for (const double lateral : {0.0, 4.0, -4.0, 8.0, -8.0}) {
        double edge_r = 0.0;
        double edge_support = 0.0;
        if (!probe_edge_line(s, top, face, lateral, edge_r, edge_support)) continue;
        const double stand_r = edge_r - 3.5;
        if (stand_r < 2.0) continue;
        stand_x = kTowerCentreX + face.dx * stand_r - face.dz * lateral;
        stand_z = kTowerCentreZ + face.dz * stand_r + face.dx * lateral;
        stand_support = edge_support;
        s.debug_launch_player(Vector3{stand_x, stand_support + 0.9, stand_z}, Vector3{0.0, 0.0, 0.0});
        for (int tick = 0; tick < 45; ++tick) {
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
        }
        const auto settled = s.snapshot();
        if (!settled.player_grounded) continue;
        if (std::hypot(settled.player_position.x - stand_x, settled.player_position.z - stand_z) > 0.5) {
            continue;
        }
        if (std::fabs(settled.player_position.y - (stand_support + 0.9)) > 0.3) continue;
        return true;
    }
    return false;
}

CaseResult run_case(const double level, const Face &face) {
    CaseResult result;
    result.level = level;
    result.face = face.name;
    Simulation s(InitialSpawn::ExteriorGrade);
    (void)s.advance_frame(1.0);
    double stand_x = 0.0;
    double stand_z = 0.0;
    double stand_support = 0.0;
    if (!find_stand(s, level, face, stand_x, stand_z, stand_support)) {
        std::cout << "  J124 " << level << " " << face.name << " NO-STAND\n";
        return result;
    }
    // The run-off: sprint, full stick outward, until the ground is gone.
    (void)s.set_sprint_input(true);
    const bool airborne = hold_stick(s, face.dx, face.dz, face.dx, face.dz, kRunOffSeconds,
                                     [](const Snapshot &state) { return !state.player_grounded; });
    if (!airborne) {
        result.status = "BLOCKED";
        const auto stuck = s.snapshot();
        std::cout << "  J124 " << level << " " << face.name << " BLOCKED at=(" << stuck.player_position.x
                  << "," << stuck.player_position.y << "," << stuck.player_position.z << ")\n";
        return result;
    }
    const double airborne_t = s.snapshot().simulation_time_seconds;
    // The descent: the stick only, full outward, the canopy by the real
    // control 1.0 s after leaving the ground. Sample every tick.
    bool opened = false;
    double max_hit = 0.0;
    double worst_x = 0.0;
    double worst_y = 0.0;
    double worst_z = 0.0;
    int hit_ticks = 0;
    double wind_peak = 0.0;
    double land_t = -1.0;
    bool held = false;
    const auto max_ticks = static_cast<std::uint32_t>(kDescentSeconds * Simulation::kTickRateHz);
    for (std::uint32_t tick = 0; tick < max_ticks; ++tick) {
        (void)s.set_move_input(face.dx, face.dz);
        (void)s.set_facing(face.dx, face.dz);
        const auto now = s.snapshot();
        if (now.held_by_matron) held = true;
        if (!opened && now.simulation_time_seconds - airborne_t >= kCanopyDelaySeconds) {
            opened = s.request_parachute();
        }
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto after = s.snapshot();
        if (after.held_by_matron) held = true;
        if (!after.player_grounded && after.death_count == 0) {
            if (after.hit_closing_mps > 0.0) {
                ++hit_ticks;
                if (after.hit_closing_mps > max_hit) {
                    max_hit = after.hit_closing_mps;
                    worst_x = after.player_position.x;
                    worst_y = after.player_position.y;
                    worst_z = after.player_position.z;
                }
            }
            wind_peak = std::max(wind_peak, after.wind_peak_force_n);
        }
        if (after.player_grounded || after.death_count > 0) {
            land_t = after.simulation_time_seconds;
            break;
        }
    }
    const auto end = s.snapshot();
    result.opened = opened;
    result.max_hit_mps = max_hit;
    result.worst_hit_x = worst_x;
    result.worst_hit_y = worst_y;
    result.worst_hit_z = worst_z;
    result.hit_ticks = hit_ticks;
    result.wind_peak_n = wind_peak;
    result.held_by_matron = held;
    result.land_x = end.player_position.x;
    result.land_y = end.player_position.y;
    result.land_z = end.player_position.z;
    result.impact_mps = end.last_impact_speed_mps;
    result.time_to_ground_s = land_t < 0.0 ? -1.0 : land_t - airborne_t;
    const bool in_second_tower = end.player_position.z <= kSecondTowerFaceZ;
    const bool alive = end.death_count == 0 && end.player_grounded;
    const bool snagged = max_hit >= kSnagClosingMps;
    const bool hard_landing = end.last_impact_speed_mps >= kWalkAwayImpactMps;
    const bool timed_out = land_t < 0.0;
    result.status = (alive && !snagged && !in_second_tower && !hard_landing && !timed_out) ? "PASS" : "FAIL";
    const char *landing = end.player_position.y <= 2.0   ? "grade"
                          : in_second_tower               ? "second-tower"
                                                          : "deck/structure";
    std::cout << "  J124 " << level << " " << face.name << " " << result.status << " land=" << landing << " ("
              << end.player_position.x << "," << end.player_position.y << "," << end.player_position.z
              << ") impact=" << end.last_impact_speed_mps << " max_hit=" << max_hit << " at=(" << worst_x << ","
              << worst_y << "," << worst_z << ") hits=" << hit_ticks << " t=" << result.time_to_ground_s
              << "s wind=" << wind_peak << " opened=" << (opened ? 1 : 0) << " deaths=" << end.death_count
              << (held ? " MATRON-HOLD" : "") << (timed_out ? " TIMEOUT" : "") << '\n';
    return result;
}

void run_band(const double *levels, const std::size_t count, const char *label) {
    std::size_t pass = 0;
    std::size_t fail = 0;
    std::size_t no_stand = 0;
    std::size_t blocked = 0;
    std::cout << "J124 sweep " << label << ": " << count << " levels x 4 faces\n";
    for (std::size_t i = 0; i < count; ++i) {
        for (const Face &face : kFaces) {
            const CaseResult result = run_case(levels[i], face);
            if (std::string(result.status) == "PASS") {
                ++pass;
            } else if (std::string(result.status) == "FAIL") {
                ++fail;
            } else if (std::string(result.status) == "BLOCKED") {
                ++blocked;
            } else {
                ++no_stand;
            }
        }
    }
    std::cout << "J124 sweep " << label << ": " << pass << " PASS, " << fail << " FAIL, " << no_stand
              << " NO-STAND, " << blocked << " BLOCKED\n";
    require(fail == 0, "parachute sweep: every runnable case lands alive with no steel contact");
}

void run_parachute_sweep_low() {
    // The stack decks (11 m pitch) and the 154 deck.
    constexpr double levels[] = {11.0, 22.0, 33.0, 44.0, 55.0, 66.0, 77.0,
                                 88.0, 99.0, 110.0, 121.0, 132.0, 143.0, 154.0};
    run_band(levels, sizeof(levels) / sizeof(levels[0]), "low");
}

void run_parachute_sweep_mid() {
    // The rings, 176.25 to 396.25 (kRingFloor + 22k).
    constexpr double levels[] = {176.25, 198.25, 220.25, 242.25, 264.25, 286.25,
                                 308.25, 330.25, 352.25, 374.25, 396.25};
    run_band(levels, sizeof(levels) / sizeof(levels[0]), "mid");
}

void run_parachute_sweep_high() {
    // The upper rings, 418.25 to 616.25, and the top machine decks.
    constexpr double levels[] = {418.25, 440.25, 462.25, 484.25, 506.25, 528.25, 550.25,
                                 572.25, 594.25, 616.25, 640.25, 662.1,  684.1,  706.1,
                                 728.1,  750.1};
    run_band(levels, sizeof(levels) / sizeof(levels[0]), "high");
}

void run_parachute_sweep() {
    run_parachute_sweep_low();
    run_parachute_sweep_mid();
    run_parachute_sweep_high();
}
} // namespace

SCRAPERX_GROUP("parachute-sweep", 1090, run_parachute_sweep);
SCRAPERX_SLICE("parachute-sweep-low", run_parachute_sweep_low);
SCRAPERX_SLICE("parachute-sweep-mid", run_parachute_sweep_mid);
SCRAPERX_SLICE("parachute-sweep-high", run_parachute_sweep_high);
