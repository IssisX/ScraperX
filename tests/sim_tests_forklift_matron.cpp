// J-041: the forklift on her deck, 154, driven into the Matron (forklift.hpp,
// matron.hpp her balance). Nothing here touches her or the truck but the
// truck's own controls (Action, the stick, the forks' buttons) and the
// player's walk to its wheel. Where she stands when a case starts, and her
// stage, are set (as sim_tests_matron_damage.cpp sets them); what follows is
// the physics: the contact impulse the truck gives her, read by her balance
// (take_blow), her brace at the edge (brace_at_edges) and gravity.
#include "sim/forklift.hpp"
#include "sim/matron.hpp"
#include "sim/matron_traversal.hpp"
#include "sim_tests.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

using scraperx::sim::Forklift;
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

namespace {

constexpr int kTruck = 4;              // the deck-154 forklift (physics_world.cpp: built last)
constexpr double kDeck = 154.0;
constexpr double kOuterEdgeZ = -124.0; // the north band's open outer edge
constexpr double kCallX = 22.5;        // the player's line up the band, west of the truck
constexpr double kMiddle = 21.5;       // the east band's middle line (Matron::kBandX)
constexpr int kHz = static_cast<int>(Simulation::kTickRateHz);
// Its engine as still as it was parked, before she is called: under 60 rpm
// (taken from parked, it turned 46 at her first tick).
constexpr double kRestRpm = 60.0;
constexpr double kRunUpM = 4.0;        // the full-speed ram's run, beyond its park

Vector3 truck_at(const Simulation &s) {
    return s.kit_body_position(s.kit_body_index(Forklift::kDeck154ChassisEntity));
}

bool trace() { return std::getenv("SCRAPERX_FM_TRACE") != nullptr; }

// Her balance's thresholds (matron.hpp): under w b she takes a blow.
double takes_upright() {
    return std::sqrt(9.81 / static_cast<double>(Matron::kUprightHeightM)) * static_cast<double>(Matron::kUprightBaseM);
}

// From beside the truck's west side onto its floor at the wheel, facing
// its forward (+z).
bool step_aboard(Simulation &s) {
    const double station_z = static_cast<double>(Forklift::kDeck154ParkZ) - 0.15;
    if (!(walk_to(s, static_cast<double>(Forklift::kDeck154ParkX) - 1.3, station_z, 8.0) &&
          walk_to(s, Forklift::kDeck154ParkX, station_z, 8.0, 0.1))) {
        return false;
    }
    (void)s.set_facing(0.0, 1.0);
    (void)s.advance_frame(0.5);
    return s.forklift_state(kTruck).at_controls;
}

// From the deck's arrival east along the north band, past her locker, down
// the east band to the truck's west side and aboard.
bool board(Simulation &s) {
    return walk_to(s, 14.0, -129.6, 20.0) && walk_to(s, kCallX, -134.0, 10.0) && step_aboard(s);
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
    double braced_front_n = 1.0e9;   // the driven (front) wheels' least normal load while she braced
    double braced_slip = 0.0;        // and their greatest |spin r| - |speed| (m/s), a tire spinning
    double off_z = 0.0;          // where she was when she dropped below the deck
    double died_y = 0.0;         // her height when she died
    bool dead = false;
    // The decisive trace (#229, ChatGPT 14:51 and 14:57): which of the
    // truck's two bodies her balance judged (take_blow's gates, read back by
    // debug_matron_strikers), the truck's tires while it touches her, her
    // feet against her budget, and how near the open edge she came.
    bool log = false;                 // a line a tick while the truck touches her
    int touch_ticks = 0;              // either truck body touching her
    int missed_ticks = 0;             // touching her, yet not among what her balance read
    int carriage_judged = 0, chassis_judged = 0, carriage_low = 0, chassis_low = 0;
    std::uint64_t first_entity = 0;   // the body of the first judged blow
    double first_centre_y = 0.0, first_feet_y = 0.0, first_closing = 0.0, first_relative = 0.0, first_change = 0.0;
    std::uint32_t box_moving = 0;     // the most moving bodies in her 3 m box
    double front_load_min = 1.0e9;    // the driven (front) wheels' least normal load while touching her
    double front_slip_max = 0.0;      // their greatest |spin r| - |speed| while touching her
    double pitch_max_deg = 0.0;       // the chassis' greatest pitch either way while touching her
    double foot_n_max = 0.0, foot_w_max = 0.0;   // her feet's force and power, any tick
    double edge_min = 1.0e9;          // her middle's least distance short of the open edge
    Fnv fnv;
    // Where it started.
    bool begun = false;
    double y0 = 0.0, last_y = 0.0, damage0 = 0.0, player0 = 0.0;
    std::uint32_t stag0 = 0, knock0 = 0;
};

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

// Out from its park by the band's open edge to the band's middle, by its
// controls alone: forward steered at the band's middle line ahead, then
// straightened along it, then backed straight down it to where it parked
// (or to `back_to`, further down it, for a longer run at her).
// Returns the truck's distance off the middle line and its yaw (rad).
std::pair<double, double> centre(Simulation &s, const double back_to) {
    const auto yaw = [&s] {
        const auto q = s.kit_body_rotation(s.kit_body_index(Forklift::kDeck154ChassisEntity));
        return 2.0 * std::atan2(q.y, q.w);
    };
    const auto stick = [&s](const double right, const double throttle, const double seconds,
                            const std::function<bool()> &until) {
        for (int i = 0; i < static_cast<int>(seconds * kHz) && !until(); ++i) {
            (void)s.set_move_input(right, throttle);
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
        }
        (void)s.set_move_input(0.0, 0.0);
    };
    const double middle = static_cast<double>(Matron::kBandX);
    stick(0.0, -0.5, 12.0, [&s] { return truck_at(s).z <= -150.0; });
    for (int i = 0; i < 20 * kHz && truck_at(s).z < -131.0 &&
                    (std::abs(truck_at(s).x - middle) > 0.15 || std::abs(yaw()) > 0.03);
         ++i) {
        (void)s.set_move_input(-steer_toward(s, {middle, kDeck, truck_at(s).z + 4.0}), 0.3);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        if (trace() && i % 9 == 0) {
            std::cout << "    centre " << truck_at(s).x << "," << truck_at(s).z << " yaw " << yaw() << " steer "
                      << s.forklift_state(kTruck).steer << "\n";
        }
    }
    stick(0.0, -0.5, 10.0, [&s, back_to] { return truck_at(s).z <= back_to; });
    // Stopped there, its engine let run down to the stillness it had parked
    // (it turns only while driven): out of gear it spins down by itself,
    // halving about every 3.5 s.
    stick(0.0, 0.0, 25.0, [&s] { return s.forklift_state(kTruck).engine_rpm < kRestRpm; });
    require(s.forklift_state(kTruck).engine_rpm < kRestRpm, "forklift_matron: the truck's engine run down at the park");
    return {truck_at(s).x - middle, yaw()};
}

// A fresh world: the player at the wheel of the deck-154 truck with the
// controls taken and the truck driven out to the band's middle; she out of
// her locker, hunting from `her` (feet), at `stage`.
std::unique_ptr<Simulation> start(const Vector3 her, const int stage,
                                  const double back_to = static_cast<double>(Forklift::kDeck154ParkZ)) {
    auto s = std::make_unique<Simulation>(InitialSpawn::Deck154);
    s->place_matron_home(static_cast<float>(kDeck));
    for (int i = 0; i < 30; ++i) {
        (void)s->advance_frame(Simulation::kFixedStepSeconds);
    }
    require(board(*s), "forklift_matron: aboard the deck-154 truck, at its wheel");
    require(s->request_forklift_action(), "forklift_matron: the controls taken");
    (void)s->advance_frame(0.3);
    require(s->forklift_state(kTruck).driving, "forklift_matron: driving the deck-154 truck");
    const auto [off, yaw] = centre(*s, back_to);
    require(std::abs(off) < 0.4 && std::abs(yaw) < 0.05,
            "forklift_matron: driven out to the band's middle, square down it");
    s->launch_matron_for_test({her.x, her.y + static_cast<double>(Matron::kStandCentre), her.z});
    for (int i = 0; i < 5 * kHz && s->matron_state().state != static_cast<int>(Matron::State::Hunting); ++i) {
        (void)s->advance_frame(Simulation::kFixedStepSeconds);
    }
    s->debug_set_matron_stage(static_cast<std::uint8_t>(stage));
    return s;
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
            run.feet_n = std::max(run.feet_n, m.feet_push_n);
            ++run.braced_ticks;
            run.braced_truck_mps = f.speed_mps;
            for (int w = 0; w < 2; ++w) {
                run.braced_front_n = std::min(run.braced_front_n, f.wheel_load_n[w]);
                run.braced_slip = std::max(run.braced_slip, std::abs(std::abs(f.wheel_spin_rad_s[w]) * f.wheel_radius[w] -
                                                                     std::abs(f.speed_mps)));
            }
        }
        if (run.off_z == 0.0 && m.position.y < kDeck - 0.5) {
            run.off_z = m.position.z;
        }
        if (!run.dead && !m.alive) {
            run.dead = true;
            run.died_y = m.position.y;
        }
        const auto k = s.debug_matron_strikers();
        run.box_moving = std::max(run.box_moving, k.box_moving);
        bool touch = false;
        const Simulation::MatronStrikerSample::Striker *chassis = nullptr;
        const Simulation::MatronStrikerSample::Striker *carriage = nullptr;
        for (const auto &st : k.kept) {
            const bool is_carriage = st.entity == Forklift::kDeck154CarriageEntity;
            if (!is_carriage && st.entity != Forklift::kDeck154ChassisEntity) {
                continue;
            }
            (is_carriage ? carriage : chassis) = &st;
            touch = touch || st.contact;
            if (st.judged) {
                ++(is_carriage ? run.carriage_judged : run.chassis_judged);
                run.carriage_low += is_carriage && st.low ? 1 : 0;
                run.chassis_low += !is_carriage && st.low ? 1 : 0;
                if (run.first_entity == 0) {
                    run.first_entity = st.entity;
                    run.first_centre_y = st.centre_y;
                    run.first_feet_y = st.feet_y;
                    run.first_closing = st.closing_mps;
                    run.first_relative = st.relative_mps;
                    run.first_change = st.change_mps;
                }
            }
        }
        const bool missed = std::any_of(k.touching_missed.begin(), k.touching_missed.end(), [](const std::uint64_t e) {
            return e == Forklift::kDeck154ChassisEntity || e == Forklift::kDeck154CarriageEntity;
        });
        touch = touch || missed;
        run.missed_ticks += missed ? 1 : 0;
        const double slip0 = std::abs(f.wheel_spin_rad_s[0]) * f.wheel_radius[0] - std::abs(f.speed_mps);
        const double slip1 = std::abs(f.wheel_spin_rad_s[1]) * f.wheel_radius[1] - std::abs(f.speed_mps);
        if (touch) {
            ++run.touch_ticks;
            run.front_load_min = std::min({run.front_load_min, f.wheel_load_n[0], f.wheel_load_n[1]});
            run.front_slip_max = std::max({run.front_slip_max, std::abs(slip0), std::abs(slip1)});
            run.pitch_max_deg = std::max(run.pitch_max_deg, std::abs(f.pitch_rad) * 180.0 / 3.14159265358979);
        }
        run.foot_n_max = std::max(run.foot_n_max, m.feet_push_n);
        run.foot_w_max = std::max(run.foot_w_max, m.foot_power_w);
        run.edge_min = std::min(run.edge_min, kOuterEdgeZ - m.position.z);
        if ((run.log && (touch || m.edge_brace_n > 0.0)) || trace()) {
            const auto gates = [](const char *name, const Simulation::MatronStrikerSample::Striker *st) {
                if (st == nullptr) {
                    std::cout << " | " << name << " -";
                    return;
                }
                std::cout << " | " << name << " c" << st->contact << " close " << st->closing_mps << "/"
                          << st->relative_mps << " dv " << st->change_mps << " y " << st->centre_y << " low " << st->low
                          << " J " << st->judged;
            };
            std::cout << "    t" << i << " thr " << f.throttle << " rpm " << f.engine_rpm << " v " << f.speed_mps
                      << " pitch " << f.pitch_rad * 180.0 / 3.14159265358979 << " load " << f.wheel_load_n[0] << "/"
                      << f.wheel_load_n[1] << "/" << f.wheel_load_n[2] << "/" << f.wheel_load_n[3] << " slip " << slip0
                      << "/" << slip1;
            gates("carriage", carriage);
            gates("chassis", chassis);
            std::cout << " | box " << k.box_moving << " missed " << missed << " | her z " << m.position.z << " edge "
                      << kOuterEdgeZ - m.position.z << " st " << int(m.state) << " feet " << m.feet_push_n << " N "
                      << m.foot_power_w << " W brace " << m.edge_brace_n << " ns " << m.damage_ns - run.damage0 << "\n";
        }
        run.fnv.add(t.x);
        run.fnv.add(t.y);
        run.fnv.add(t.z);
        run.fnv.add(f.lift_m);
        run.fnv.add(m.position.x);
        run.fnv.add(m.position.y);
        run.fnv.add(m.position.z);
        run.fnv.add(m.state);
    }
    (void)s.set_move_input(0.0, 0.0);
    (void)s.set_forklift_lift(0.0);
}

void print(const char *label, const Run &r) {
    std::cout << "  forklift_matron " << label << ": hit at " << r.hit_mps << " m/s, impulse " << r.impulse_ns
              << " N s, blow " << r.blow_mps << " m/s (low " << r.low << "), staggers " << r.staggers
              << ", knockdowns " << r.knockdowns << ", riding " << r.rode << ", held him " << r.held_him << ", rise "
              << r.rise_m << " m, brace " << r.brace_n << " N (feet " << r.feet_n << " N, " << r.braced_ticks
              << " ticks, truck " << r.braced_truck_mps << " m/s, its front tyres at least " << (r.braced_ticks > 0 ? r.braced_front_n : 0.0) << " N, slip " << r.braced_slip << " m/s), off at z " << r.off_z << ", fall " << r.fall_mps
              << " m/s, dead " << r.dead << " (at y " << r.died_y << "), hash " << r.fnv.hash << "\n";
    std::cout << "    judged: carriage " << r.carriage_judged << " (low " << r.carriage_low << "), chassis "
              << r.chassis_judged << " (low " << r.chassis_low << "); first by " << r.first_entity << ", its centre at y "
              << r.first_centre_y << ", her soles " << r.first_feet_y << " (low under +" << Matron::kLowBlowM
              << "), closing " << r.first_closing << "/" << r.first_relative << " m/s (gate "
              << Matron::kStrikerClosingMps << "), change " << r.first_change << " m/s a tick (gate 0.1); touching "
              << r.touch_ticks << " ticks, missed " << r.missed_ticks << ", box " << r.box_moving
              << " moving; front wheels' least load " << (r.touch_ticks > 0 ? r.front_load_min : 0.0) << " N, slip "
              << r.front_slip_max << " m/s, pitch " << r.pitch_max_deg << " deg; her feet " << r.foot_n_max << " N / "
              << r.foot_w_max << " W; edge " << r.edge_min << " m\n";
}

// Loose stock made on the deck (live fabrication, add_fabricated: real
// bodies, made as the game makes them): 0.3 m timber crates, 16.2 kg, in a
// block of 4 across by `count / 4` along beside where the truck meets her (east of the
// truck's path, all within her 3 m box), so her balance has more moving
// bodies near her than sense_near keeps (its first 32).
int stock(Simulation &s, const int count) {
    constexpr float kHalf = 0.15F;
    constexpr float kTimber = 600.0F;
    const float volume = 8.0F * kHalf * kHalf * kHalf;
    std::vector<std::uint8_t> file;
    const auto put = [&file](const void *value, const std::size_t size) {
        const auto *bytes = static_cast<const std::uint8_t *>(value);
        file.insert(file.end(), bytes, bytes + size);
    };
    const std::uint32_t version = 1, material = 1, hulls = 1, points = 8;
    const float mass = kTimber * volume;
    const std::array<float, 3> half{kHalf, kHalf, kHalf};
    put("SXFABHUL", 8);
    put(&version, 4);
    put(&material, 4);
    put(&hulls, 4);
    put(&mass, 4);
    put(&volume, 4);
    put(half.data(), 12);
    const std::array<std::uint8_t, 64> digest{};
    put(digest.data(), digest.size());
    put(&points, 4);
    for (const float x : {-kHalf, kHalf}) {
        for (const float y : {-kHalf, kHalf}) {
            for (const float z : {-kHalf, kHalf}) {
                const std::array<float, 3> p{x, y, z};
                put(p.data(), 12);
            }
        }
    }
    int made = 0;
    for (int i = 0; i < count; ++i) {
        const Vector3 at{23.2 + 0.4 * (i % 4), kDeck, -135.3 + 0.4 * (i / 4)};
        made += s.add_fabricated(file.data(), file.size(), at, 0.0) != 0 ? 1 : 0;
    }
    return made;
}

// Rammed: the truck driven at her from its park (or from `back_to`) until
// its frame is at z -131, then let go (it brakes), 3 s more. Logged, a line
// a tick while it touches her. With `crates` >= 0, that many crates of loose stock are made where
// the truck meets her, and 1 s let pass, before it starts (0: the same
// second, no crates, the crowded case's control).
Run ram(const int stage, const double throttle, const double forks_up_s, const bool log = false,
        const int crates = -1, const double back_to = static_cast<double>(Forklift::kDeck154ParkZ)) {
    auto s = start({kMiddle, kDeck, -128.0}, stage, back_to);
    Simulation &w = *s;
    Run r;
    r.log = log;
    if (crates >= 0) {
        const int made = stock(w, crates);
        (void)w.advance_frame(1.0);
        std::cout << "  forklift_matron: " << made << " crates made where the truck meets her\n";
        require(made == crates, "forklift_matron crowded: the crates made");
    }
    if (forks_up_s > 0.0) {
        drive(w, r, 0.0, 1.0, forks_up_s);
    }
    drive(w, r, throttle, 0.0, 14.0, [&] { return truck_at(w).z >= -131.0; });
    drive(w, r, 0.0, 0.0, 3.0);
    return r;
}

// Pushed off: forks up to her middle, she 2.6 m from the open edge; driven
// at her, then on at a walk until the truck's frame is 2 m short of the
// edge (its front wheels under a meter short), let go, 10 s more.
Run push_off() {
    auto s = start({kMiddle, kDeck, -126.6}, 2);
    Simulation &w = *s;
    Run r;
    r.log = true;
    drive(w, r, 0.0, 1.0, 1.5);
    drive(w, r, 0.4, 0.0, 14.0, [&] { return truck_at(w).z >= -127.6; });
    drive(w, r, 0.3, 0.0, 10.0, [&] { return truck_at(w).z >= -126.0; });
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
// locker wakes her; the walk to the truck's side; she bursts out, and a
// run down beside the truck and back calls her; aboard, DRIVE, back off 4 m;
// she comes for him; he drives at her at full speed, steering at her.
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
    require(walk_to(s, 14.0, -129.6, 20.0) && walk_to(s, kCallX, -134.0, 10.0),
            "forklift_matron natural: to the truck's side");
    int burst = 0;
    while (!s.matron_state().released && burst++ < 40 * kHz) {
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
    }
    std::cout << "  forklift_matron natural: she burst out after " << burst / kHz << " s\n";
    // Heard: a run down beside the truck and back, until her lead is by it.
    for (int call = 0; call < 8; ++call) {
        (void)s.set_sprint_input(true);
        (void)walk_to(s, kCallX, -141.0, 4.0);
        (void)s.set_sprint_input(false);
        (void)walk_to(s, kCallX, -137.0, 6.0);
        const auto m = s.matron_state();
        std::cout << "  forklift_matron natural: call " << call << ", her state " << int(m.state) << " at ("
                  << m.position.x << "," << m.position.z << "), lead " << m.lead_valid << " (" << m.lead_x << ","
                  << m.lead_z << ")\n";
        if (m.lead_valid && std::hypot(m.lead_x - kCallX, m.lead_z + 139.0) < 8.0) {
            break;
        }
    }
    require(step_aboard(s) && s.request_forklift_action(), "forklift_matron natural: aboard the deck-154 truck, DRIVE");
    (void)s.advance_frame(0.3);
    Run r;
    // She comes on down the band, ahead of its forks.
    int waited = 0;
    drive(s, r, 0.0, 0.0, 60.0, [&] {
        ++waited;
        const auto m = s.matron_state();
        const auto t = truck_at(s);
        return m.state == static_cast<int>(Matron::State::Hunting) && m.position.z > t.z + 2.0 &&
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
    // Full speed wants its run: backed kRunUpM further down the band's
    // middle than it parks, it is at speed by the time it meets her.
    const double run_from = static_cast<double>(Forklift::kDeck154ParkZ) - kRunUpM;
    const Run full = ram(1, 1.0, 0.0, true, -1, run_from);
    print("ram, full speed, dangerous", full);
    require(full.hit_mps > 2.5 && full.impulse_ns > 100.0 && full.knockdowns >= 1 && !full.dead,
            "forklift_matron ram: at full speed the truck's contact impulse knocks her down");
    const Run creep = ram(0, 0.3, 0.0);
    print("ram, creep, pest", creep);
    require(creep.hit_mps > 0.4 && creep.hit_mps < 1.2 && creep.impulse_ns > 10.0 &&
                creep.blow_mps < takes_upright() && creep.staggers == 0 && creep.knockdowns == 0,
            "forklift_matron ram: at a creep the truck meets her, its impulse under what her balance takes");
    // Run twice, the same.
    const Run again = ram(1, 1.0, 0.0, false, -1, run_from);
    std::cout << "  forklift_matron: the full-speed ram again, hash " << again.fnv.hash << "\n";
    require(again.fnv.hash == full.fnv.hash, "forklift_matron: the same inputs give the same ram, tick for tick");
    // Which body her balance judged, past which gates (take_blow): at full
    // speed and at a creep both closed on her past 0.5 m/s, changed past
    // 0.1 m/s a tick and touched her; none touched her unread.
    require(full.chassis_judged + full.carriage_judged > 0 && full.first_closing >= Matron::kStrikerClosingMps &&
                full.first_relative >= Matron::kStrikerClosingMps && full.first_change >= 0.1 &&
                full.missed_ticks == 0,
            "forklift_matron gates: the full-speed ram is judged on a truck body past all three of take_blow's gates");
    require(creep.first_entity == Forklift::kDeck154CarriageEntity && creep.carriage_low > 0 &&
                creep.first_closing >= Matron::kStrikerClosingMps && creep.missed_ticks == 0,
            "forklift_matron gates: at a creep the fork carriage, its centre under her knees, is the judged low blow");

    // Crowded: 32 crates of loose stock (live fabrication) where the truck
    // meets her, so 34 moving bodies in her box, more than sense_near's 32;
    // against the same second with none. Required now that sense_near keeps
    // the 32 nearest (#254): the truck is read on every tick it touches her
    // and the ram still fells her, crates or not.
    const Run control = ram(1, 1.0, 0.0, false, 0);
    print("ram, full speed, dangerous, the crowded case's control", control);
    const Run crowded = ram(1, 1.0, 0.0, std::getenv("SCRAPERX_FM_CROWD") != nullptr, 32);
    print("ram, full speed, dangerous, 32 crates where it meets her", crowded);
    require(control.knockdowns >= 1 && control.missed_ticks == 0,
            "forklift_matron crowded: with no crates the same ram is read and fells her");
    require(crowded.box_moving > 32, "forklift_matron crowded: more moving bodies in her box than sense_near keeps");
    require(crowded.missed_ticks == 0 && crowded.knockdowns >= 1,
            "forklift_matron crowded: with 32 crates the truck is still read on every tick it touches her and fells her");
    std::cout << "  forklift_matron crowded: the truck touched her " << crowded.touch_ticks << " ticks, "
              << crowded.missed_ticks << " of them unread by her balance; judged " << crowded.carriage_judged
              << " carriage + " << crowded.chassis_judged << " chassis, knockdowns " << crowded.knockdowns
              << " (control " << control.knockdowns << "), held him " << crowded.held_him << "\n";

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
    // Low or not is the judged body's center of mass against her soles + 0.5
    // m: the fork carriage's, forks down (about 0.27 m up) or at her middle
    // (about 0.87 m).
    require(shins.carriage_low > 0 && middle.carriage_judged > 0 && middle.carriage_low == 0 && middle.chassis_low == 0,
            "forklift_matron low: the fork carriage is the judged body, low forks down and not low at her middle");

    // 3. Lift. Parked, forks down: she comes at the driver over the blades;
    // once her feet are up on them, RAISE for 4 s. Contact alone carries her.
    for (const int stage : {1, 2}) {
        auto s = start({kMiddle, kDeck, -131.0}, stage);
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

    // 4. Push off. Driven back to the open edge she goes over, falls by
    // gravity, and the landing at her fall's speed decides it. Where her feet
    // brace back from the edge (brace_at_edges: not while a blow staggers
    // her), they do it with all her 8 kN, and the truck's driven tires still
    // grip and drive her back.
    const Run off = push_off();
    print("push off, changed", off);
    require(off.braced_ticks > 10 && off.feet_n > 7900.0 && off.braced_truck_mps > 0.3,
            "forklift_matron push off: braced at the edge with her whole budget, the truck still drives her back");
    require(off.braced_front_n > 3000.0 && off.braced_slip < 0.1,
            "forklift_matron push off: against her brace the driven front tyres stay loaded and grip, no spin");
    require(off.off_z > kOuterEdgeZ, "forklift_matron push off: she goes over the open edge");
    require(off.dead == (off.fall_mps >= static_cast<double>(Matron::kLethalLandingMps)) && off.dead &&
                off.died_y < 10.0,
            "forklift_matron push off: she falls from 154 m and the landing at that speed kills her");

    // 5. Fight back. Too slow (0.6 m/s), the truck neither fells nor stops
    // her: she holds, comes at the driver and takes him off it.
    {
        auto s = start({kMiddle, kDeck, -128.0}, 1);
        Simulation &w = *s;
        Run slow;
        drive(w, slow, 0.15, 0.0, 14.0,
              [&] { return w.matron_state().state == static_cast<int>(Matron::State::Grab); });
        drive(w, slow, 0.0, 0.0, 1.0);
        print("too slow, dangerous", slow);
        require(slow.knockdowns == 0 && slow.staggers == 0 && slow.held_him && !w.forklift_state(kTruck).driving,
                "forklift_matron fight back: too slow to fell her, she holds and takes the driver off the truck");
    }
    // Her feet never past their budget in any case: 8 kN / 44 kW, or a
    // lunge's 9.8 kN / 65 kW; at the edge, 8 kN.
    for (const Run *r : {&full, &creep, &shins, &middle, &off, &control, &crowded}) {
        require(r->foot_n_max <= static_cast<double>(Matron::kTurnLungeForceN) + 1.0 &&
                    r->foot_w_max <= static_cast<double>(Matron::kTurnLungePowerW) + 1.0,
                "forklift_matron: her feet within their budget in every case");
    }
    require(off.feet_n <= static_cast<double>(scraperx::sim::traversal::Limits{}.total_force_n) + 0.5,
            "forklift_matron push off: her brace and catch together within her 8 kN");
    std::cout << "PASS scraperx_sim forklift_matron: ram_ns=" << full.impulse_ns << " creep_blow_mps=" << creep.blow_mps
              << " push_feet_n=" << off.feet_n << " fall_mps=" << off.fall_mps << "\n";
}
