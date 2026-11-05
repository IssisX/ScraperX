// J-041: the forklift on her deck, 154, driven into the Matron (forklift.hpp,
// matron.hpp her balance). Nothing here touches her or the truck but the
// truck's own controls (Action, the stick, the forks' buttons) and the
// player's walk to its wheel. Where she stands when a case starts, and her
// stage, are set (as sim_tests_matron_damage.cpp sets them); what follows is
// the physics: the contact impulse the truck gives her, read by her balance
// (take_blow), her brace at the edge (brace_at_edges) and gravity.
#include "sim/forklift.hpp"
#include "sim/matron.hpp"
#include "sim_tests.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>

using scraperx::sim::Forklift;
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

namespace {

constexpr int kTruck = 4;              // the deck-154 forklift (physics_world.cpp: built last)
constexpr double kDeck = 154.0;
constexpr double kOuterEdgeZ = -124.0; // the north band's open outer edge
constexpr int kHz = static_cast<int>(Simulation::kTickRateHz);

Vector3 truck_at(const Simulation &s) {
    return s.kit_body_position(s.kit_body_index(Forklift::kDeck154ChassisEntity));
}

bool trace() { return std::getenv("SCRAPERX_FM_TRACE") != nullptr; }

// Her balance's thresholds (matron.hpp): under w b she takes a blow.
double takes_upright() {
    return std::sqrt(9.81 / static_cast<double>(Matron::kUprightHeightM)) * static_cast<double>(Matron::kUprightBaseM);
}

// From the deck's arrival east along the north band, past her locker, down
// the east band to the truck's west side and onto its floor at the wheel,
// facing its forward (+z).
bool board(Simulation &s) {
    const double station_z = static_cast<double>(Forklift::kDeck154ParkZ) - 0.15;
    if (!(walk_to(s, 14.0, -129.6, 20.0) && walk_to(s, 19.9, -134.0, 10.0) && walk_to(s, 19.9, station_z, 8.0) &&
          walk_to(s, Forklift::kDeck154ParkX, station_z, 8.0, 0.1))) {
        return false;
    }
    (void)s.set_facing(0.0, 1.0);
    (void)s.advance_frame(0.5);
    return s.forklift_state(kTruck).at_controls;
}

struct Fnv {
    std::uint64_t hash = 14695981039346656037ULL;
    void add(const double value) {
        const auto word = static_cast<std::uint64_t>(std::llround(value * 1000.0));
        for (int byte = 0; byte < 8; ++byte) {
            hash ^= (word >> (8 * byte)) & 0xFFU;
            hash *= 1099511628211ULL;
        }
    }
};

// What a case did, read every tick.
struct Run {
    double hit_mps = 0.0;        // the truck's speed when her judged impulse first rose
    double impulse_ns = 0.0;     // her judged contact impulse (damage_ns) over the case
    double blow_mps = 0.0;       // the largest blow her balance read
    bool low = false;            // a blow at her shins
    bool rode = false;           // riding_ seen
    std::uint32_t staggers = 0;
    std::uint32_t knockdowns = 0;
    bool held_him = false;       // her hand on the player (Grab and after)
    double rise_m = 0.0;         // her middle's greatest rise over where she began
    double fall_mps = 0.0;       // her fastest fall
    double brace_n = 0.0;        // her feet braced back from the edge (brace_at_edges)
    double feet_n = 0.0;         // all her feet's push on a braced tick (her 8 kN budget)
    int braced_ticks = 0;
    double braced_truck_mps = 0.0;   // the truck's speed on the last braced tick
    double off_z = 0.0;          // where she was when she dropped below the deck
    double died_y = 0.0;         // her height when she died
    bool dead = false;
    Fnv fnv;
    // Where it started.
    bool begun = false;
    double y0 = 0.0, last_y = 0.0, damage0 = 0.0, player0 = 0.0;
    std::uint32_t stag0 = 0, knock0 = 0;
};

// A fresh world: the player at the wheel of the deck-154 truck with the
// controls taken; she out of her locker, hunting from `her` (feet), at `stage`.
std::unique_ptr<Simulation> start(const Vector3 her, const int stage) {
    auto s = std::make_unique<Simulation>(InitialSpawn::Deck154);
    s->place_matron_home(static_cast<float>(kDeck));
    for (int i = 0; i < 30; ++i) {
        (void)s->advance_frame(Simulation::kFixedStepSeconds);
    }
    require(board(*s), "forklift_matron: aboard the deck-154 truck, at its wheel");
    require(s->request_forklift_action(), "forklift_matron: the controls taken");
    (void)s->advance_frame(0.3);
    require(s->forklift_state(kTruck).driving, "forklift_matron: driving the deck-154 truck");
    s->launch_matron_for_test({her.x, her.y + static_cast<double>(Matron::kStandCentre), her.z});
    for (int i = 0; i < 5 * kHz && s->matron_state().state != static_cast<int>(Matron::State::Hunting); ++i) {
        (void)s->advance_frame(Simulation::kFixedStepSeconds);
    }
    s->debug_set_matron_stage(static_cast<std::uint8_t>(stage));
    return s;
}

// The stick right that steers the truck at `at` (the stick right steers it
// right, whatever the view).
double steer_toward(const Simulation &s, const Vector3 at) {
    const auto q = s.kit_body_rotation(s.kit_body_index(Forklift::kDeck154ChassisEntity));
    const double yaw = 2.0 * std::atan2(q.y, q.w);
    const auto t = truck_at(s);
    const double dx = at.x - t.x;
    const double dz = at.z - t.z;
    const double ahead = dx * std::sin(yaw) + dz * std::cos(yaw);
    const double right = -dx * std::cos(yaw) + dz * std::sin(yaw);
    return std::clamp(2.0 * std::atan2(right, std::max(ahead, 0.5)), -1.0, 1.0);
}

// The stick at `throttle` (steered at her when `steer`) and the forks' button
// at `lift`, a tick at a time, for at most `seconds` or until `until` holds;
// then both let go. The player faces +z, so the stick's right is world -x.
void drive(Simulation &s, Run &run, const double throttle, const double lift, const double seconds,
           const std::function<bool()> &until = [] { return false; }, const bool steer = false) {
    if (!run.begun) {
        const auto m = s.matron_state();
        run.begun = true;
        run.y0 = run.last_y = m.position.y;
        run.damage0 = m.damage_ns;
        run.player0 = m.player_damage_ns;
        run.stag0 = m.staggers;
        run.knock0 = m.knockdowns;
    }
    const int ticks = static_cast<int>(seconds * kHz);
    for (int i = 0; i < ticks && !until(); ++i) {
        (void)s.set_move_input(steer ? -steer_toward(s, s.matron_state().position) : 0.0, throttle);
        (void)s.set_forklift_lift(lift);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto f = s.forklift_state(kTruck);
        const auto m = s.matron_state();
        const auto t = truck_at(s);
        if (run.impulse_ns == 0.0 && m.damage_ns > run.damage0) {
            run.hit_mps = f.speed_mps;
        }
        run.impulse_ns = m.damage_ns - run.damage0;
        require(m.player_damage_ns == run.player0, "forklift_matron: no blow of the player's own in any case");
        run.blow_mps = std::max(run.blow_mps, m.blow_mps);
        run.low = run.low || (m.blow_mps > 0.0 && m.blow_low);
        run.rode = run.rode || m.riding;
        run.staggers = m.staggers - run.stag0;
        run.knockdowns = m.knockdowns - run.knock0;
        run.held_him = run.held_him || (m.state >= static_cast<int>(Matron::State::Grab) &&
                                        m.state <= static_cast<int>(Matron::State::Spare));
        run.rise_m = std::max(run.rise_m, m.position.y - run.y0);
        run.fall_mps = std::max(run.fall_mps, (run.last_y - m.position.y) * kHz);
        run.last_y = m.position.y;
        if (m.edge_brace_n > 0.0) {
            run.brace_n = std::max(run.brace_n, m.edge_brace_n);
            run.feet_n = std::max(run.feet_n, m.foot_force_n);
            ++run.braced_ticks;
            run.braced_truck_mps = f.speed_mps;
        }
        if (run.off_z == 0.0 && m.position.y < kDeck - 0.5) {
            run.off_z = m.position.z;
        }
        if (!run.dead && !m.alive) {
            run.dead = true;
            run.died_y = m.position.y;
        }
        run.fnv.add(t.x);
        run.fnv.add(t.y);
        run.fnv.add(t.z);
        run.fnv.add(f.lift_m);
        run.fnv.add(m.position.x);
        run.fnv.add(m.position.y);
        run.fnv.add(m.position.z);
        run.fnv.add(m.state);
        if (trace()) {
            std::cout << "    truck (" << t.x << "," << t.y << "," << t.z << ") " << f.speed_mps << " m/s, forks "
                      << f.lift_m << "; her (" << m.position.x << "," << m.position.y << "," << m.position.z
                      << ") state " << int(m.state) << " blow " << m.blow_mps << " low " << m.blow_low << " riding "
                      << m.riding << " brace " << m.edge_brace_n << " feet " << m.foot_force_n << "\n";
        }
    }
    (void)s.set_move_input(0.0, 0.0);
    (void)s.set_forklift_lift(0.0);
}

void print(const char *label, const Run &r) {
    std::cout << "  forklift_matron " << label << ": hit at " << r.hit_mps << " m/s, impulse " << r.impulse_ns
              << " N s, blow " << r.blow_mps << " m/s (low " << r.low << "), staggers " << r.staggers
              << ", knockdowns " << r.knockdowns << ", riding " << r.rode << ", held him " << r.held_him << ", rise "
              << r.rise_m << " m, brace " << r.brace_n << " N (feet " << r.feet_n << " N, " << r.braced_ticks
              << " ticks, truck " << r.braced_truck_mps << " m/s), off at z " << r.off_z << ", fall " << r.fall_mps
              << " m/s, dead " << r.dead << " (at y " << r.died_y << "), hash " << r.fnv.hash << "\n";
}

// Rammed: the truck driven at her from its park until its frame is at z -131,
// then let go (it brakes), 3 s more.
Run ram(const int stage, const double throttle, const double forks_up_s) {
    auto s = start({Forklift::kDeck154ParkX, kDeck, -128.0}, stage);
    Simulation &w = *s;
    Run r;
    if (forks_up_s > 0.0) {
        drive(w, r, 0.0, 1.0, forks_up_s);
    }
    drive(w, r, throttle, 0.0, 14.0, [&] { return truck_at(w).z >= -131.0; });
    drive(w, r, 0.0, 0.0, 3.0);
    return r;
}

// Pushed off: forks up to her middle, she 2.6 m from the open edge; driven
// at her, then on at a walk until the truck's frame is 1.5 m short of the
// edge (its front wheels under a metre short), let go, 10 s more.
Run push_off() {
    auto s = start({Forklift::kDeck154ParkX, kDeck, -126.6}, 2);
    Simulation &w = *s;
    Run r;
    drive(w, r, 0.0, 1.0, 1.5);
    drive(w, r, 0.4, 0.0, 14.0, [&] { return truck_at(w).z >= -127.6; });
    drive(w, r, 0.3, 0.0, 10.0, [&] { return truck_at(w).z >= -125.5; });
    const auto truck = truck_at(w);
    const auto f = w.forklift_state(kTruck);
    std::cout << "  forklift_matron push off: the truck let go at z " << truck.z << ", y " << truck.y
              << ", wheels on the deck " << f.wheel_contact[0] << f.wheel_contact[1] << f.wheel_contact[2]
              << f.wheel_contact[3] << "\n";
    require(truck.z < kOuterEdgeZ - 1.3 && std::abs(truck.y - kDeck) < 0.05 && f.wheel_contact[0] &&
                f.wheel_contact[1] && f.wheel_contact[2] && f.wheel_contact[3],
            "forklift_matron push off: the truck stops on the deck, every wheel on it");
    drive(w, r, 0.0, 0.0, 10.0);
    require(truck_at(w).z < kOuterEdgeZ - 1.2, "forklift_matron push off: braked, the truck stays on the deck");
    return r;
}

// The whole of it, nothing set: from the deck's arrival a sprint by her
// locker wakes her; the walk to the truck; DRIVE; she bursts out and comes
// for him; he drives at her at full speed, steering at her.
Run natural() {
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(static_cast<float>(kDeck));
    for (int i = 0; i < 30; ++i) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    (void)s.set_sprint_input(true);
    const bool by_locker = walk_to(s, 6.0, -128.6, 10.0);
    (void)s.set_sprint_input(false);
    std::cout << "  forklift_matron natural: by her locker " << by_locker << ", her state "
              << int(s.matron_state().state) << "\n";
    require(by_locker, "forklift_matron natural: the sprint toward her locker");
    // Off to the truck's side, quietly; she bursts out of her locker.
    require(walk_to(s, 14.0, -129.6, 20.0) && walk_to(s, 19.9, -134.0, 10.0),
            "forklift_matron natural: to the truck's side");
    int burst = 0;
    while (!s.matron_state().released && burst++ < 40 * kHz) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    std::cout << "  forklift_matron natural: she burst out after " << burst / kHz << " s\n";
    // Heard: a run up the band and back, until her lead is by the truck.
    for (int call = 0; call < 8; ++call) {
        (void)s.set_sprint_input(true);
        (void)walk_to(s, 19.9, -130.0, 4.0);
        (void)s.set_sprint_input(false);
        (void)walk_to(s, 19.9, -134.0, 6.0);
        const auto m = s.matron_state();
        std::cout << "  forklift_matron natural: call " << call << ", her state " << int(m.state) << " at ("
                  << m.position.x << "," << m.position.z << "), lead " << m.lead_valid << " (" << m.lead_x << ","
                  << m.lead_z << ")\n";
        if (m.lead_valid && std::hypot(m.lead_x - 19.9, m.lead_z + 132.0) < 8.0) {
            break;
        }
    }
    require(board(s) && s.request_forklift_action(), "forklift_matron natural: aboard the deck-154 truck, DRIVE");
    Run r;
    int waited = 0;
    drive(s, r, 0.0, 0.0, 60.0, [&] {
        ++waited;
        const auto m = s.matron_state();
        const auto t = truck_at(s);
        return m.state == static_cast<int>(Matron::State::Hunting) &&
               std::hypot(m.position.x - t.x, m.position.z - t.z) < 12.0;
    });
    const auto m0 = s.matron_state();
    std::cout << "  forklift_matron natural: after " << waited / kHz << " s she is at (" << m0.position.x << ","
              << m0.position.z << "), state " << int(m0.state) << ", stage " << int(m0.stage) << "\n";
    drive(s, r, 1.0, 0.0, 8.0, [&] { return r.knockdowns > 0 || truck_at(s).z >= -129.0; }, true);
    drive(s, r, 0.0, 0.0, 2.0);
    return r;
}

} // namespace

void run_forklift_matron() {
    // 0. The whole of it, nothing set: wake her, call her, DRIVE, ram her.
    {
        const Run whole = natural();
        print("the whole of it, nothing set", whole);
        require(whole.impulse_ns > 10.0 && whole.low && whole.knockdowns >= 1 && !whole.dead,
                "forklift_matron: woken, called and rammed, she goes down at the forks");
    }
    std::cout << "  forklift_matron: her balance takes an upright blow under " << takes_upright() << " m/s\n";
    // Parked on her deck, it stays put on its wheels.
    {
        Simulation s(InitialSpawn::Deck154);
        const auto parked = truck_at(s);
        (void)s.advance_frame(5.0);
        const auto after = truck_at(s);
        const double crept = std::hypot(after.x - parked.x, after.z - parked.z);
        std::cout << "  forklift_matron: parked on 154 at (" << after.x << "," << after.y << "," << after.z
                  << "), crept " << crept << " m\n";
        require(crept < 0.01 && std::abs(after.y - kDeck) < 0.05,
                "forklift_matron: parked on her deck, the truck stands on its wheels and stays put");
    }

    // 1. Ram. At full speed (3 m/s) she goes down; at a creep (under 1 m/s)
    // the forks' blades slide under her shins and she takes it.
    const Run full = ram(1, 1.0, 0.0);
    print("ram, full speed, dangerous", full);
    require(full.hit_mps > 2.5 && full.impulse_ns > 100.0 && full.knockdowns >= 1 && !full.dead,
            "forklift_matron ram: at full speed the truck's contact impulse knocks her down");
    const Run creep = ram(0, 0.3, 0.0);
    print("ram, creep, pest", creep);
    require(creep.hit_mps > 0.4 && creep.hit_mps < 1.2 && creep.impulse_ns > 10.0 &&
                creep.blow_mps < takes_upright() && creep.staggers == 0 && creep.knockdowns == 0,
            "forklift_matron ram: at a creep the truck meets her, its impulse under what her balance takes");
    // Run twice, the same.
    const Run again = ram(1, 1.0, 0.0);
    std::cout << "  forklift_matron: the full-speed ram again, hash " << again.fnv.hash << "\n";
    require(again.fnv.hash == full.fnv.hash, "forklift_matron: the same inputs give the same ram, tick for tick");

    // 2. The fork hit is low. Fully changed she braces against a blow she
    // faces and only staggers -- unless it comes at her shins, where she has
    // no step to take: forks down, the same truck puts her down; forks up at
    // her middle it only staggers her.
    const Run shins = ram(2, 1.0, 0.0);
    print("ram, forks down, changed", shins);
    const Run middle = ram(2, 1.0, 1.5);
    print("ram, forks at her middle, changed", middle);
    require(full.low && shins.low && shins.knockdowns >= 1,
            "forklift_matron low: a hit at fork height takes the low-blow path and fells even a changed Matron");
    require(!middle.low && middle.blow_mps > shins.blow_mps * 0.5 && middle.staggers >= 1 && middle.knockdowns == 0,
            "forklift_matron low: the forks at her middle, as hard a hit only staggers her, braced");

    // 3. Lift. Parked, forks down: she comes at the driver over the blades;
    // once her feet are up on them, RAISE for 4 s. Contact alone carries her.
    for (const int stage : {1, 2}) {
        auto s = start({Forklift::kDeck154ParkX, kDeck, -131.0}, stage);
        Simulation &w = *s;
        Run wait;
        drive(w, wait, 0.0, 0.0, 10.0,
              [&] { return w.matron_state().position.y > kDeck + static_cast<double>(Matron::kStandCentre) + 0.03; });
        const double forks0 = w.forklift_state(kTruck).lift_m;
        Run up;
        drive(w, up, 0.0, 1.0, 4.0);
        const double forks = w.forklift_state(kTruck).lift_m - forks0;
        std::cout << "  stage " << stage << ": the forks rose " << forks << " m\n";
        print("lift", up);
        require(wait.knockdowns == 0 && forks > 1.4 && up.rise_m > 1.3 && std::abs(up.rise_m - forks) < 0.15 &&
                    up.impulse_ns < 1.0 && up.knockdowns == 0 && w.matron_state().alive,
                "forklift_matron lift: the forks under her and raised, she goes up with them on contact alone");
    }

    // 4. Push off. Her feet brace back from the edge with all her 8 kN; the
    // truck pushes on, she goes over, falls by gravity, and the landing at
    // her fall's speed decides it.
    const Run off = push_off();
    print("push off, changed", off);
    require(off.braced_ticks > 10 && off.feet_n > 7900.0 && off.braced_truck_mps > 0.3,
            "forklift_matron push off: braced at the edge with her whole budget, the truck still drives her back");
    require(off.off_z > kOuterEdgeZ, "forklift_matron push off: she goes over the open edge");
    require(off.dead == (off.fall_mps >= static_cast<double>(Matron::kLethalLandingMps)) && off.dead &&
                off.died_y < 10.0,
            "forklift_matron push off: she falls the 154 m and the landing at that speed kills her, at the ground");

    // 5. Fight back. Too slow (0.6 m/s), the truck neither fells nor stops
    // her: she holds, comes at the driver and takes him off it.
    {
        auto s = start({Forklift::kDeck154ParkX, kDeck, -128.0}, 1);
        Simulation &w = *s;
        Run slow;
        drive(w, slow, 0.15, 0.0, 14.0,
              [&] { return w.matron_state().state == static_cast<int>(Matron::State::Grab); });
        drive(w, slow, 0.0, 0.0, 1.0);
        print("too slow, dangerous", slow);
        require(slow.knockdowns == 0 && slow.staggers == 0 && slow.held_him && !w.forklift_state(kTruck).driving,
                "forklift_matron fight back: too slow to fell her, she holds and takes the driver off the truck");
    }
    std::cout << "PASS scraperx_sim forklift_matron: ram_ns=" << full.impulse_ns << " creep_blow_mps=" << creep.blow_mps
              << " push_feet_n=" << off.feet_n << " fall_mps=" << off.fall_mps << "\n";
}
