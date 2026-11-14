#include "sim_tests.hpp"
#include "sim/matron.hpp"
#include "sim/physics_world_tuning.hpp"

namespace {
using scraperx::sim::Simulation;
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;

void rung(Simulation &s, const double rise_seconds) {
    s.place_matron_home(154.0F);
    s.debug_hold_matron_grab(true);
    require(s.advance_frame(1.0).accepted, "ladder pull: settle");
    require(walk_to(s, 3.5, -128.2, 16.0) && walk_to(s, 3.5, -129.8, 4.0) &&
                walk_to(s, 5.0, -129.6, 4.0, 0.08), "ladder pull: reach the real ladder");
    require(s.set_facing(0.0, 1.0) && s.request_traversal() && s.advance_frame(0.2).accepted &&
                is_climbing(s.snapshot()), "ladder pull: hands take scanned rungs");
    if (rise_seconds > 0.0) {
        require(s.set_move_input(0.0, 1.0) && s.advance_frame(rise_seconds).accepted,
                "ladder pull: climb to another rung");
    }
    const auto before = s.snapshot();
    require(s.set_move_input(0.0, 0.0) && s.advance_frame(0.4).accepted,
            "ladder pull: settle the rung");
    const auto after = s.snapshot();
    std::cout << "ladder setup: rise=" << rise_seconds << " y=" << before.player_position.y
              << "->" << after.player_position.y << " force=" << before.climb_grip_force_n
              << "->" << after.climb_grip_force_n << " slips=" << before.climb_grip_slips
              << "->" << after.climb_grip_slips << " v=" << before.player_linear_velocity.y
              << "->" << after.player_linear_velocity.y << '\n';
    require(is_climbing(s.snapshot()),
            "ladder pull: finite grip holds without her pull");
}

double step(const scraperx::sim::Vector3 a, const scraperx::sim::Vector3 b) {
    return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));
}
}

namespace {
// ---- J-086: one ladder per kind on the route -------------------------------
// The pull is the same mechanism on every kind (physics_world.cpp: her hand
// takes the ankle whenever Matron::caught(true) and grab_ready()); what
// differs is the staging: where the ladder stands, how he reaches its foot,
// which way he faces, and where she can stand beside him.
struct LadderKind {
    const char *name;
    InitialSpawn spawn;
    bool launch;  // place him over the foot by debug_launch_player, no walk
    double foot_x, foot_z;   // the board he stands on at the foot
    double face_x, face_z;   // the way he faces to take hold
    double foot_y;           // the deck under the foot, for her placement
    double her_dx, her_dz;   // her offset from him while she pulls
    int legs;                // approach waypoints before the foot
    double leg[4][2];
};

// Cage ladder: the 484 ring's ladder, reached from the parked cage I.
// ring_inner(484) = 8.35, so the ladder stands at x = -8.21, z = -148.1
// (kIZ - 2.5); its foot board runs the 462 ring's west band. No spawn stands
// on it, so he is launched onto the board.
// Stack ladder: the plate-shop route ladder 340.25 to 352.25. ring_inner(352)
// = 13.81, so it stands at x = 13.67, z = -158.0 (kRouteZ), its foot board on
// TP-340; the walk goes round F's hole (x 11.6 to 14.4, z -141.35 to -138.45)
// and the header tank (x 2 to 10, z -152 to -143).
// Upper-band ladder: the facade crane ladder 484.25 to 506.25.
// ring_inner(506) = 7.44, so it stands at x = 7.30, z = -150.0 (kRouteZ), its
// foot board on the 484 ring; the walk goes round the east band.
const LadderKind kLadderKinds[] = {
    {"cage", InitialSpawn::Deck154, true, -8.76, -148.1, 1.0, 0.0, 462.25, -0.72, 0.0, 0,
     {{0, 0}}},
    {"stack", InitialSpawn::PlateTop, false, 13.12, -158.0, 1.0, 0.0, 340.25, -0.72, 0.0, 4,
     {{12.0, -136.0}, {1.0, -136.0}, {1.0, -157.0}, {13.12, -157.0}}},
    {"upper-band", InitialSpawn::Ring484North, false, 6.75, -150.0, 1.0, 0.0, 484.25, 0.0, 0.72, 3,
     {{10.0, -139.6}, {10.0, -149.0}, {6.75, -149.0}}},
};

void stage_ladder(Simulation &s, const LadderKind &k) {
    std::cout << "ladder kind: " << k.name << '\n';
    s.place_matron_home(static_cast<float>(k.foot_y));
    s.debug_hold_matron_grab(true);
    require(s.advance_frame(1.0).accepted, "ladder kind: settle");
    if (k.launch) {
        s.debug_launch_player({k.foot_x, k.foot_y + 1.5, k.foot_z}, {0.0, 0.0, 0.0});
        require(s.advance_frame(1.0).accepted, "ladder kind: settle the launch");
    }
    for (int i = 0; i < k.legs; ++i) {
        require(walk_to(s, k.leg[i][0], k.leg[i][1], 16.0), "ladder kind: walk to the foot");
    }
    require(walk_to(s, k.foot_x, k.foot_z, 4.0, 0.08), "ladder kind: stand at the foot");
    require(s.set_facing(k.face_x, k.face_z) && s.request_traversal() &&
                s.advance_frame(0.2).accepted && is_climbing(s.snapshot()),
            "ladder kind: hands take scanned rungs");
    require(s.set_move_input(0.0, 1.0) && s.advance_frame(0.6).accepted,
            "ladder kind: climb to a low rung");
    const auto before = s.snapshot();
    require(s.set_move_input(0.0, 0.0) && s.advance_frame(0.4).accepted,
            "ladder kind: settle the rung");
    const auto after = s.snapshot();
    std::cout << "ladder kind " << k.name << ": y=" << before.player_position.y << "->"
              << after.player_position.y << " force=" << before.climb_grip_force_n << "->"
              << after.climb_grip_force_n << " slips=" << before.climb_grip_slips << "->"
              << after.climb_grip_slips << '\n';
    require(is_climbing(s.snapshot()), "ladder kind: finite grip holds without her pull");
}

// She reaches him mid-climb on this ladder and pulls him off by the ankle.
void pull_off_ladder(Simulation &s, const LadderKind &k, double &peak_n, double &peak_w) {
    const auto start = s.snapshot();
    s.debug_place_matron({start.player_position.x + k.her_dx, k.foot_y,
                          start.player_position.z + k.her_dz});
    s.debug_fix_matron_pace(3);
    s.debug_set_matron_stage(2);
    s.debug_hold_matron_hands(true);
    require(s.advance_frame(0.5).accepted && is_climbing(s.snapshot()) &&
                s.snapshot().climb_grip_slips == start.climb_grip_slips,
            "ladder kind: her nearby body alone cannot open his hands");
    s.debug_hold_matron_grab(false);
    s.debug_hold_matron_hands(false);
    bool both_grips = false, taken_off = false;
    peak_n = 0.0;
    peak_w = 0.0;
    double max_step = 0.0;
    auto previous = s.snapshot();
    auto previous_her = s.matron_state().position;
    for (int i = 0; i < 5 * 90; ++i) {
        require(s.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "ladder kind: real physics tick");
        const auto p = s.snapshot();
        const auto m = s.matron_state();
        both_grips = both_grips || (is_climbing(p) && m.grip_on);
        peak_n = std::max(peak_n, p.ladder_pull_force_n);
        peak_w = std::max(peak_w, p.ladder_pull_power_w);
        max_step = std::max(max_step, step(previous.player_position, p.player_position));
        require(p.ladder_pull_force_n <= 8000.1 && p.ladder_pull_power_w <= 22000.1,
                "ladder kind: every tick inside her one force and power budget");
        require(p.player_gravity_factor == 1.0, "ladder kind: gravity stays on");
        require(step(previous.player_position, p.player_position) <= 0.2,
                "ladder kind: no teleport or step over 0.2 m");
        require(step(previous_her, m.position) <= 0.2,
                "ladder kind: her every step stays within 0.2 m");
        require(p.death_count == start.death_count, "ladder kind: first catch is not death");
        if (p.climb_grip_slips > start.climb_grip_slips && m.grip_on && !is_climbing(p)) {
            taken_off = true;
            break;
        }
        previous = p;
        previous_her = m.position;
    }
    std::cout << "ladder kind " << k.name << ": both=" << both_grips << " off=" << taken_off
              << " force=" << peak_n << " power=" << peak_w << " step=" << max_step << '\n';
    require(both_grips && taken_off && peak_n > 0.0,
            "ladder kind: she must overcome his real grip before taking him off");
    require(s.snapshot().climb_grip_slips == start.climb_grip_slips + 1,
            "ladder kind: exactly one physical grip failure precedes the hold");
    require(s.advance_frame(Simulation::kFixedStepSeconds).accepted,
            "ladder kind: advance into the existing hold");
}

// After she has taken him off a ladder: his blow frees her grip, and for
// 30 s (Matron::kRegrabAfterTearS) she must not take hold again, however
// close he stands. He stands still beside her; a shove is allowed, a grab
// is not. The window is timed from the tick her hand opens.
void back_off_after_blow(Simulation &s, const char *label) {
    std::cout << "back-off " << label << ": his blow frees the hold\n";
    require(s.matron_state().grip_on, "back-off: he is in her hold to begin with");
    const auto deaths_before = s.snapshot().death_count;
    bool freed = false;
    for (int i = 0; i < 10 * 90 && !freed; ++i) {
        s.debug_matron_player_blow();
        require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "back-off: free tick");
        freed = !s.matron_state().grip_on;
    }
    require(freed, "back-off: his blow frees her grip");
    const auto grabs0 = s.matron_state().grabs;
    int in_range = 0;
    for (int i = 0; i < 30 * 90; ++i) {
        require(s.set_move_input(0.0, 0.0) &&
                    s.advance_frame(Simulation::kFixedStepSeconds).accepted,
                "back-off: tick");
        const auto p = s.snapshot();
        const auto m = s.matron_state();
        require(m.grabs == grabs0, "back-off: no new grab inside 30 s");
        require(p.death_count == deaths_before, "back-off: he survives the window");
        const double dx = p.player_position.x - m.position.x;
        const double dz = p.player_position.z - m.position.z;
        const double dy = p.player_position.y - m.position.y;
        if (std::hypot(dx, dz) < 0.85 && std::abs(dy) < 1.4) ++in_range;
    }
    std::cout << "back-off " << label << ": in_range_ticks=" << in_range << '\n';
    require(in_range > 0, "back-off: she was in reach and still did not grab");
}
}

void run_matron_ladder_pull() {
    for (int scenario = 0; scenario < 4; ++scenario) {
        const double rise = scenario == 0 ? 0.0 : scenario == 2 ? 6.0 : 0.6;
        Simulation s(InitialSpawn::Deck154);
        if (scenario == 3) s.debug_lose_arm(0);
        rung(s, rise);
        const auto start = s.snapshot();
        s.debug_place_matron({start.player_position.x + 0.72, 154.0, start.player_position.z});
        s.debug_fix_matron_pace(3);
        s.debug_set_matron_stage(2);
        s.debug_hold_matron_hands(true);
        // Mere proximity changes none of his grip rules.
        require(s.advance_frame(0.5).accepted && is_climbing(s.snapshot()) &&
                    s.snapshot().climb_grip_slips == start.climb_grip_slips,
                "ladder pull: her nearby body alone cannot open his hands");
        s.debug_hold_matron_grab(false);
        s.debug_hold_matron_hands(false);
        bool both_grips = false, taken_off = false;
        double peak_n = 0.0, peak_w = 0.0, max_step = 0.0;
        auto previous = s.snapshot();
        auto previous_her = s.matron_state().position;
        const auto grabs = s.matron_state().grabs;
        for (int i = 0; i < 5 * 90; ++i) {
            require(s.advance_frame(Simulation::kFixedStepSeconds).accepted, "ladder pull: real physics tick");
            const auto p = s.snapshot();
            const auto m = s.matron_state();
            both_grips = both_grips || (is_climbing(p) && m.grip_on);
            peak_n = std::max(peak_n, p.ladder_pull_force_n);
            peak_w = std::max(peak_w, p.ladder_pull_power_w);
            max_step = std::max(max_step, step(previous.player_position, p.player_position));
            require(p.ladder_pull_force_n <= 8000.1 && p.ladder_pull_power_w <= 22000.1,
                    "ladder pull: every tick inside her one force and power budget");
            require(p.player_gravity_factor == 1.0, "ladder pull: gravity stays on");
            require(step(previous.player_position, p.player_position) <= 0.2,
                    "ladder pull: no teleport or step over 0.2 m");
            require(step(previous_her, m.position) <= 0.2,
                    "ladder pull: her every step stays within 0.2 m");
            require(p.death_count == start.death_count, "ladder pull: first catch is not death");
            if (p.climb_grip_slips > start.climb_grip_slips && m.grip_on && !is_climbing(p)) {
                taken_off = true;
                break;
            }
            previous = p;
            previous_her = m.position;
        }
        std::cout << "ladder pull: rise_s=" << rise << " both=" << both_grips << " off=" << taken_off
                  << " force=" << peak_n << " power=" << peak_w << " step=" << max_step << '\n';
        if (rise < 1.0) {
            require(both_grips && taken_off && peak_n > 0.0,
                    "ladder pull: she must overcome his real grip before taking him off");
            require(s.snapshot().climb_grip_slips == start.climb_grip_slips + 1,
                    "ladder pull: exactly one physical grip failure precedes the hold");
            require(s.advance_frame(Simulation::kFixedStepSeconds).accepted,
                    "ladder pull: advance into the existing hold");
            if (scenario == 3) {
                require(s.matron_state().fatal_path != 0,
                        "ladder pull: one arm left hands off to the existing fatality");
            }
        } else {
            require(!taken_off && s.matron_state().grabs == grabs && is_climbing(s.snapshot()),
                    "ladder pull: high out-of-reach rung stays safe");
        }
    }
    // J-086: the same pull on every other ladder kind in play, and the 30 s
    // back-off after his blow frees the hold, in the same scenarios.
    for (const LadderKind &kind : kLadderKinds) {
        Simulation s2(kind.spawn);
        stage_ladder(s2, kind);
        double peak_n = 0.0, peak_w = 0.0;
        pull_off_ladder(s2, kind, peak_n, peak_w);
        back_off_after_blow(s2, kind.name);
    }
    std::cout << "PASS scraperx_sim ladder pull: reachable rungs release only under load; high rung safe; "
                 "cage, stack and upper-band ladders release the same way, and the 30 s back-off holds\n";
}
SCRAPERX_SLICE("matron-ladder-pull", run_matron_ladder_pull);
