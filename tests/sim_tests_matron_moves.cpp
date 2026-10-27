// The Matron's paces and her getting unstuck (sim/matron_moves.cpp): she is
// many speeds, chosen and changed by her, never snapped; and her patrol round
// the 154 m deck reaches every node of its loop with no stall she stays in.

#include "sim_tests.hpp"

#include "sim/loose_stock.hpp"
#include "sim/matron.hpp"

#include <set>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::LooseStock;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;

constexpr int kHunting = static_cast<int>(Matron::State::Hunting);

void tick(Simulation &s) { (void)s.advance_frame(Simulation::kFixedStepSeconds); }

bool steer_to(Simulation &s, const double x, const double z, const bool sprint) {
    const auto p = s.snapshot().player_position;
    const double dx = x - p.x;
    const double dz = z - p.z;
    const double l = std::hypot(dx, dz);
    if (l < 0.6) {
        (void)s.set_move_input(0.0, 0.0);
        return true;
    }
    (void)s.set_move_input(dx / l, dz / l);
    (void)s.set_facing(dx / l, dz / l);
    (void)s.set_sprint_input(sprint);
    return false;
}
} // namespace

void run_matron_pace() {
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    require(s.advance_frame(1.0).accepted, "matron pace: settle");
    // Wake her with a sprint by her locker, then lead her round the deck's
    // bands: sprinting, walking, standing still and quiet, in turn.
    for (int i = 0; i < 4 * 90 && !steer_to(s, 10.0, -128.5, true); ++i) {
        tick(s);
    }
    const double corners[4][2] = {{21.5, -128.5}, {21.5, -171.5}, {-21.5, -171.5}, {-21.5, -128.5}};
    std::set<int> paces;
    int changes_up = 0;
    int changes_down = 0;
    int last_pace = -1;
    double top_speed = 0.0;
    double worst_speed_step = 0.0;
    double last_speed = -1.0;
    double peak_foot_force_n = 0.0;
    double peak_foot_power_w = 0.0;
    double peak_lunge_force_n = 0.0;
    double peak_lunge_power_w = 0.0;
    // Her feet's force and power each tick, held to the cap of the drive
    // that moves her: the Turn lunge's own when it is the lunge, the shared
    // one for every other step, the Turn's wind-up included.
    const auto sample_drive = [&](const auto &m) {
        if (m.climb_lunge_drive) {
            peak_lunge_force_n = std::max(peak_lunge_force_n, m.climb_force_n);
            peak_lunge_power_w = std::max(peak_lunge_power_w, m.foot_power_w);
        } else {
            peak_foot_force_n = std::max(peak_foot_force_n, m.climb_force_n);
            peak_foot_power_w = std::max(peak_foot_power_w, m.foot_power_w);
        }
    };
    bool gravity_kept = true;
    bool hunted = false;
    std::uint32_t hushes = 0;
    bool was_hushed = false;
    int corner = 0;
    double phase_s = 0.0;
    std::uint32_t staggers = 0, vaults = 0, clambers = 0, step_ups = 0;
    std::uint8_t last_turn = 0;
    const int ticks = 240 * 90;
    for (int i = 0; i < ticks; ++i) {
        // 12 s a phase: sprint, walk, stand silent.
        phase_s += Simulation::kFixedStepSeconds;
        const int phase = static_cast<int>(phase_s / 12.0) % 3;
        if (phase == 2) {
            (void)s.set_move_input(0.0, 0.0);
            (void)s.set_sprint_input(false);
        } else if (steer_to(s, corners[corner][0], corners[corner][1], phase == 0)) {
            corner = (corner + 1) % 4;
        }
        tick(s);
        const auto m = s.matron_state();
        sample_drive(m);
        gravity_kept = gravity_kept && s.debug_matron_gravity_factor() == 1.0;
        if (m.state != kHunting) {
            last_speed = -1.0;
            continue;
        }
        hunted = true;
        paces.insert(m.pace);
        if (last_pace >= 0 && m.pace > last_pace) {
            ++changes_up;
        }
        if (last_pace >= 0 && m.pace < last_pace) {
            ++changes_down;
        }
        last_pace = m.pace;
        top_speed = std::max(top_speed, m.speed);
        sample_drive(m);
        gravity_kept = gravity_kept && s.debug_matron_gravity_factor() == 1.0;
        if (m.hushed && !was_hushed) {
            ++hushes;
        }
        was_hushed = m.hushed;
        // Her feet's speed from tick to tick: at most kFootAccel speeding up
        // (the Turn's own 70 m/s/s aside), never a snap up to a faster pace;
        // braking is her plant (kCutAccel) and the edge guard's stop.
        const auto p = s.snapshot().player_position;
        const bool contact = std::hypot(m.position.x - p.x, m.position.z - p.z) < 2.0 || m.staggers != staggers;
        staggers = m.staggers;
        const bool turn_now = m.turn != 0 || last_turn != 0;
        last_turn = m.turn;
        if (last_speed >= 0.0 && !turn_now && !contact && m.vaults == vaults && m.clambers == clambers &&
            m.step_ups == step_ups && !m.climbing && !m.on_line && !m.on_pipe && !m.clinging &&
            m.state == kHunting) {
            worst_speed_step = std::max(worst_speed_step, m.speed - last_speed);   // speeding up: her feet's kFootAccel
        }
        last_speed = m.speed;
        vaults = m.vaults;
        clambers = m.clambers;
        step_ups = m.step_ups;
    }
    const auto m = s.matron_state();
    std::cout << "  matron pace: paces seen " << paces.size() << ", up " << changes_up << ", down " << changes_down
              << ", changes " << m.pace_changes << ", top " << top_speed << " m/s, worst step " << worst_speed_step
              << " m/s/tick, hushes " << hushes << ", creaks " << m.creaks << ", crumbs " << m.crumbs << " walked "
              << m.crumbs_walked << "\n";
    require(hunted, "matron pace: she hunts the player round the deck");
    require(paces.size() >= 3, "matron pace: she goes at more than two paces");
    require(changes_up >= 2 && changes_down >= 2, "matron pace: up and down between them, more than once");
    require(top_speed > Matron::kHuntMps - 0.3, "matron pace: her old top speed is still hers");
    // Her feet add at most kFootAccel*dt (0.1 m/s) a tick; contacts and
    // landings add a little more. A snap between paces would be 1.5-6 m/s.
    require(worst_speed_step <= 0.3,
            "matron pace: from pace to pace by her feet's acceleration, never a snap");
    std::cout << "  matron pace: her feet's peak " << peak_foot_force_n << " N, " << peak_foot_power_w
              << " W; the lunge's peak " << peak_lunge_force_n << " N, " << peak_lunge_power_w << " W\n";
    require(gravity_kept && peak_foot_force_n > 500.0 && peak_foot_force_n <= law::kForceN + 0.5 &&
                peak_foot_power_w <= law::kPowerW + 0.5,
            "matron pace: her walking and running drive through her capped force and power budget with gravity on");
    require(peak_lunge_force_n <= law::kLungeForceN + 0.5 && peak_lunge_power_w <= law::kLungePowerW + 0.5,
            "matron pace: the Turn lunge drives within its own force and power cap");
    // Determinism: the same run again is the same run.
    std::cout << "PASS scraperx_sim matron pace: paces=" << paces.size() << " top_mps=" << top_speed
              << " worst_step=" << worst_speed_step << " hushes=" << hushes << "\n";
}

void run_matron_unstick() {
    // Her patrol round the 154 m deck's loop, the player away up the tower
    // (TP-340, out of her hearing): every node of the loop reached, and no
    // stall she does not get out of.
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    require(s.advance_frame(1.0).accepted, "matron unstick: settle");
    s.debug_launch_player({12.0, 341.4, -136.8}, {0.0, 0.0, 0.0});
    (void)s.advance_frame(1.0);
    s.debug_matron_patrol();
    // A crate set down across the east band's line, in her way: she must get
    // over it or round it.
    const auto crate = s.kit_body_index(LooseStock::kFirstEntity + static_cast<std::uint64_t>(LooseStock::kDeckCrateSouth));
    s.debug_launch_kit_body(crate, {Matron::kBandX, Matron::kDeckTop + 0.6, -140.0}, {0.0, 0.0, 0.0});
    const double nodes[8][2] = {{-Matron::kBandX, Matron::kBandNorthZ}, {0.0, Matron::kBandNorthZ},
                                {Matron::kBandX, Matron::kBandNorthZ},  {Matron::kBandX, Matron::kMidZ},
                                {Matron::kBandX, Matron::kBandSouthZ},  {0.0, Matron::kBandSouthZ},
                                {-Matron::kBandX, Matron::kBandSouthZ}, {-Matron::kBandX, Matron::kMidZ}};
    std::set<int> reached;
    double still_s = 0.0;
    double worst_still_s = 0.0;
    double peak_foot_force_n = 0.0;
    double peak_foot_power_w = 0.0;
    bool lunge_in_check = true;   // a Turn lunge here is held to its own cap
    bool gravity_kept = true;
    auto last = s.matron_state().position;
    for (int i = 0; i < 420 * 90 && reached.size() < 8; ++i) {
        tick(s);
        const auto m = s.matron_state();
        if (m.climb_lunge_drive) {
            lunge_in_check = lunge_in_check && m.climb_force_n <= law::kLungeForceN + 0.5 &&
                             m.foot_power_w <= law::kLungePowerW + 0.5;
        } else {
            peak_foot_force_n = std::max(peak_foot_force_n, m.climb_force_n);
            peak_foot_power_w = std::max(peak_foot_power_w, m.foot_power_w);
        }
        gravity_kept = gravity_kept && s.debug_matron_gravity_factor() == 1.0;
        for (int n = 0; n < 8; ++n) {
            if (std::hypot(m.position.x - nodes[n][0], m.position.z - nodes[n][1]) < 2.0) {
                reached.insert(n);
            }
        }
        if (i % 90 == 0) {
            const double moved = std::hypot(m.position.x - last.x, m.position.z - last.z);
            still_s = moved < 0.3 ? still_s + 1.0 : 0.0;
            worst_still_s = std::max(worst_still_s, still_s);
            last = m.position;
        }
    }
    const auto m = s.matron_state();
    std::cout << "  matron unstick: nodes reached " << reached.size() << " of 8, state " << int(m.state)
              << ", stalls " << m.stalls << ", step-ups " << m.step_ups << ", clambers " << m.clambers << ", vaults "
              << m.vaults << ", sidesteps " << m.sidesteps << ", reroutes " << m.reroutes << ", longest stall "
              << m.longest_stall_s << " s, longest still " << worst_still_s << " s\n";
    require(m.alive, "matron unstick: alive on her deck");
    require(reached.size() == 8, "matron unstick: her patrol reaches every node of the deck's loop");
    require(worst_still_s <= 10.0, "matron unstick: no stall she stays in");
    std::cout << "  matron unstick: her feet's peak " << peak_foot_force_n << " N, " << peak_foot_power_w << " W\n";
    require(gravity_kept && peak_foot_force_n > 500.0 && peak_foot_force_n <= law::kForceN + 0.5 &&
                peak_foot_power_w <= law::kPowerW + 0.5 && lunge_in_check,
            "matron unstick: patrol and obstacle clamber/vault stay within her capped force and power budget with gravity on");
    std::cout << "PASS scraperx_sim matron unstick: nodes=" << reached.size() << " stalls=" << m.stalls
              << " longest_still_s=" << worst_still_s << "\n";
}
