// J-042 (#240), "Her ammo, your ammo": she takes a real toolbox
// (LooseStock::Kind::Toolbox, 18 kg) and throws it at the player; he reads
// her windup and dodges the real projectile, races her for it and throws it
// back, through her real take_blow. Every case runs on real inputs (the
// stick, the facing, pick up, the up gesture) against her real tick: no
// staged miss, no staged hit, nothing set going but by a force or a throw.
// Logged for each case: the projectile's path, every contact it makes (the
// impact ledger, with the loudness she hears), the sounds she took and her
// responses, and the damage on both sides.

#include "sim_tests.hpp"

#include "sim/impacts.hpp"
#include "sim/loose_stock.hpp"
#include "sim/matron.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
using scraperx::sim::ImpactLedger;
using scraperx::sim::InitialSpawn;
using scraperx::sim::LooseStock;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

constexpr int kStateStraining = static_cast<int>(Matron::State::Straining);
constexpr int kStateHunting = static_cast<int>(Matron::State::Hunting);
constexpr int kStateDown = static_cast<int>(Matron::State::Down);
constexpr int kStateStaggering = static_cast<int>(Matron::State::Staggering);
constexpr double kHz = static_cast<double>(Simulation::kTickRateHz);

void tick(Simulation &s) {
    (void)s.advance_frame(Simulation::kFixedStepSeconds);
}

bool steer(Simulation &s, const double x, const double z, const bool sprint, const double tolerance) {
    const auto now = s.snapshot();
    const double dx = x - now.player_position.x;
    const double dz = z - now.player_position.z;
    const double length = std::hypot(dx, dz);
    if (length < tolerance) {
        (void)s.set_move_input(0.0, 0.0);
        (void)s.set_sprint_input(false);
        return true;
    }
    (void)s.set_move_input(dx / length, dz / length);
    (void)s.set_facing(dx / length, dz / length);
    (void)s.set_sprint_input(sprint);
    return false;
}

bool go(Simulation &s, const double x, const double z, const bool sprint, const double seconds,
        const double tolerance = 0.4) {
    for (int i = 0; i < static_cast<int>(seconds * kHz); ++i) {
        if (steer(s, x, z, sprint, tolerance)) {
            return true;
        }
        tick(s);
    }
    return false;
}

void stand_still(Simulation &s, const double seconds) {
    (void)s.set_move_input(0.0, 0.0);
    (void)s.set_sprint_input(false);
    (void)s.advance_frame(seconds);
}

double flat(const Vector3 &a, const Vector3 &b) {
    return std::hypot(a.x - b.x, a.z - b.z);
}

double apart(const Vector3 &a, const Vector3 &b) {
    return std::hypot(std::hypot(a.x - b.x, a.z - b.z), a.y - b.y);
}

// What the case logs, tick by tick: the toolbox's path, every hit it takes
// (and how loud she hears it), the player's felt hits, her blows and her
// state, lead and heading.
struct Ledger {
    const char *name = "";
    std::uint64_t box = 0;
    std::uint32_t body = 0;
    std::uint64_t seq = 0;
    int ticks = 0;
    std::uint64_t hits = 0;
    double damage_ns = 0.0;
    int state = -1;
    std::uint64_t his = 0;     // what his hands hold
    std::uint64_t hers = 0;    // what hers hold
    std::uint32_t box_hits = 0;
    std::uint32_t box_hits_silent = 0;   // while her claim on her throw stood
    double loudest = 0.0;

    void start(Simulation &s, const char *case_name, const std::uint64_t entity) {
        name = case_name;
        box = entity;
        body = s.kit_body_index(entity);
        seq = s.last_impact_sequence();
        hits = s.snapshot().hit_count;
        damage_ns = s.matron_state().damage_ns;
        state = s.matron_state().state;
    }

    // One tick's log; `path` prints the box's place every few ticks while it
    // flies (in nobody's hands, moving).
    void step(Simulation &s, const bool path) {
        const auto m = s.matron_state();
        const auto p = s.snapshot();
        const auto b = s.kit_body_position(body);
        const auto v = s.kit_body_velocity(body);
        const bool flying =
            p.carrying_entity_id != box && m.carrying != box && std::hypot(std::hypot(v.x, v.z), v.y) > 0.05;
        if (path && flying && ticks % 3 == 0) {
            std::cout << "    " << name << " t=" << ticks / kHz << " box (" << b.x << ", " << b.y << ", " << b.z << ") v "
                      << std::hypot(std::hypot(v.x, v.z), v.y) << " player (" << p.player_position.x << ", "
                      << p.player_position.y << ", " << p.player_position.z << ") her (" << m.position.x << ", "
                      << m.position.z << ") windup " << m.throw_windup << " carrying " << (m.carrying != 0) << " sees "
                      << m.sees_player << " stalking " << m.stalking << " state " << int(m.state) << "\n";
        }
        for (const auto &hit : s.impacts()) {
            if (hit.sequence <= seq) {
                continue;
            }
            seq = hit.sequence;
            if (hit.entity != box) {
                continue;
            }
            const double loud = ImpactLedger::loudness(static_cast<float>(hit.energy_j));
            ++box_hits;
            if (m.stock_thrown == box) {
                ++box_hits_silent;
            }
            loudest = std::max(loudest, loud);
            std::cout << "    " << name << " contact t=" << ticks / kHz << " at (" << hit.at.x << ", " << hit.at.y << ", "
                      << hit.at.z << ") dv " << hit.speed_mps << " m/s, " << hit.energy_j << " J, loudness " << loud
                      << (m.stock_thrown == box ? " (her own throw: no sound to her)" : " (a sound to her)") << "\n";
        }
        if (p.hit_count != hits) {
            std::cout << "    " << name << " player hit t=" << ticks / kHz << " closing " << p.hit_speed_mps
                      << " m/s, felt hits " << p.hit_count << ", arm broken " << p.arm_broken << ", deaths "
                      << p.death_count << "\n";
            hits = p.hit_count;
        }
        if (m.damage_ns != damage_ns) {
            std::cout << "    " << name << " her blow t=" << ticks / kHz << " impulse +" << m.damage_ns - damage_ns
                      << " N s (total " << m.damage_ns << "), blow " << m.blow_mps << " m/s, staggers " << m.staggers
                      << ", knockdowns " << m.knockdowns << "\n";
            damage_ns = m.damage_ns;
        }
        if (p.carrying_entity_id != his || m.carrying != hers) {
            std::cout << "    " << name << " hands t=" << ticks / kHz << ": his " << p.carrying_entity_id << ", hers "
                      << m.carrying << " (her " << flat(m.position, p.player_position) << " m off, state "
                      << int(m.state) << ")\n";
            his = p.carrying_entity_id;
            hers = m.carrying;
        }
        if (m.state != state) {
            std::cout << "    " << name << " her state t=" << ticks / kHz << " " << state << " -> " << int(m.state)
                      << ", lead (" << m.lead_x << ", " << m.lead_z << ") sense " << int(m.lead_sense) << ", yaw "
                      << m.yaw << ", heard " << m.noise_heard << "\n";
            state = m.state;
        }
        ++ticks;
    }
};

// The prelude (as her mischief, sim_tests_matron.cpp): the first toolbox
// by the north rail picked up, carried east across the bay and set down on
// the band (his, to her); a sprint toward her locker wakes her, and he goes
// back west to wait on the band. Returns the toolbox's entity.
std::uint64_t handled_toolbox(Simulation &s, const char *name) {
    s.place_matron_home(154.0F);
    s.debug_fix_matron_pace(3);   // her old one speed, as her mischief's own run
    require(s.advance_frame(1.0).accepted, "ammo: settle");
    require(walk_to(s, -6.5, -125.95, 6.0, 0.08), "ammo: to the toolbox");
    (void)s.set_facing(0.0, 1.0);
    (void)s.advance_frame(0.4);
    const std::uint64_t box = s.snapshot().carry_target_entity_id;
    require(box == LooseStock::kFirstEntity + s.loose_stock_throwables_begin(), "ammo: the toolbox is in the hands' reach");
    (void)s.request_pick_up();
    (void)s.advance_frame(0.4);
    require(s.snapshot().carrying_entity_id == box, "ammo: picked up");
    require(go(s, -4.6, Matron::kBandNorthZ, false, 12.0, 0.2) && go(s, 0.4, Matron::kBandNorthZ, false, 12.0, 0.2) &&
                go(s, 2.0, -126.6, false, 12.0, 0.2),
            "ammo: carried east along the band, across the bay on its leaf");
    stand_still(s, 0.4);
    (void)s.request_set_down();
    (void)s.advance_frame(1.0);
    require(s.snapshot().carrying_entity_id == 0, "ammo: set down on the band");
    require(go(s, 8.0, Matron::kBandNorthZ, true, 4.0, 1.0) && s.matron_state().state == kStateStraining,
            "ammo: a sprint toward her locker wakes her");
    (void)s.set_crouch_input(true);
    require(go(s, -6.0, Matron::kBandNorthZ, false, 30.0, 0.3), "ammo: back west, crouched");
    (void)s.set_crouch_input(false);
    stand_still(s, 0.2);
    std::cout << "  " << name << ": the toolbox (entity " << box << ") set down, she is out\n";
    return box;
}

// Her throw at him, read and met, until it has left her hands and come to
// rest. `dodge`: from the tick it leaves her hands (her windup read, the
// release seen), a walked sidestep across her line toward the outer rail
// for kSidestepS, then his face back on her. Without it he stands, looking
// on along the band.
constexpr double kSidestepS = 0.3;

struct Throw {
    bool thrown = false;
    bool hit = false;
    double windup_seen_s = -1.0;
    double released_s = -1.0;
    double nearest_m = 1.0e9;
    double release_gap_m = 0.0;
    double top_speed = 0.0;
    double hit_mps = 0.0;
    double stepped_m = 0.0;
    double hit_s = -1.0;
    double claim_end_s = -1.0;     // her claim on it ended (its hits sounds to her again)
    Vector3 rest{};
};

// His face on her (her stalk freezes under it: matron_stalk.cpp).
void look_at_her(Simulation &s) {
    const auto m = s.matron_state();
    const auto p = s.snapshot().player_position;
    const double l = flat(m.position, p);
    if (l > 0.05) {
        (void)s.set_facing((m.position.x - p.x) / l, (m.position.z - p.z) / l);
    }
}

Throw her_throw(Simulation &s, Ledger &log, const bool dodge) {
    Throw t;
    const auto before = s.snapshot();
    const std::uint32_t throws = s.matron_state().throws;
    double dx = 0.0;
    double dz = 0.0;
    int stepping = -1;
    int quiet = 0;
    Vector3 stood{};
    const int step_ticks = static_cast<int>(kSidestepS * kHz);
    for (int i = 0; i < static_cast<int>(60.0 * kHz); ++i) {
        const auto m = s.matron_state();
        if (m.throw_windup > 0.0 && t.windup_seen_s < 0.0) {
            t.windup_seen_s = log.ticks / kHz;
            std::cout << "    " << log.name << " windup seen t=" << t.windup_seen_s << ", her "
                      << flat(m.position, s.snapshot().player_position) << " m off\n";
        }
        if (stepping >= 0 && stepping < step_ticks) {
            (void)s.set_move_input(dx, dz);
            (void)s.set_sprint_input(false);
            ++stepping;
        } else {
            (void)s.set_move_input(0.0, 0.0);
            if (stepping >= 0) {
                look_at_her(s);
            }
        }
        tick(s);
        log.step(s, t.thrown || s.matron_state().carrying != 0);
        const auto after = s.matron_state();
        const auto p = s.snapshot().player_position;
        if (!t.thrown && after.throws > throws) {
            t.thrown = true;
            t.released_s = log.ticks / kHz;
            t.release_gap_m = flat(after.position, p);
            stood = p;
            std::cout << "    " << log.name << " released t=" << t.released_s << ", " << t.release_gap_m
                      << " m from him\n";
            if (dodge) {
                // Across her line, the rail side (+z): the band is 9 m wide.
                const double lx = p.x - after.position.x;
                const double lz = p.z - after.position.z;
                const double l = std::hypot(lx, lz);
                dx = -lz / l;
                dz = lx / l;
                if (dz < 0.0) {
                    dx = -dx;
                    dz = -dz;
                }
                stepping = 0;
            }
        }
        if (t.thrown) {
            const auto b = s.kit_body_position(log.body);
            const auto v = s.kit_body_velocity(log.body);
            const double speed = std::hypot(std::hypot(v.x, v.z), v.y);
            t.top_speed = std::max(t.top_speed, speed);
            t.nearest_m = std::min(t.nearest_m, apart(b, p));
            t.stepped_m = std::max(t.stepped_m, flat(p, stood));
            if (t.claim_end_s < 0.0 && after.stock_thrown == 0) {
                t.claim_end_s = log.ticks / kHz;
            }
            if (!t.hit && s.snapshot().hit_count > before.hit_count) {
                t.hit = true;
                t.hit_s = log.ticks / kHz;
                t.hit_mps = s.snapshot().hit_speed_mps;
            }
            quiet = speed < 0.05 ? quiet + 1 : 0;
            // Dodged, he goes for it as soon as it is down (slowed to a
            // slide); else to its rest.
            const bool down = dodge && stepping >= step_ticks && speed < 1.5 && b.y < p.y;
            if ((quiet > static_cast<int>(0.5 * kHz) && (stepping < 0 || stepping >= step_ticks)) || down) {
                t.rest = b;
                break;
            }
        }
    }
    (void)s.set_move_input(0.0, 0.0);
    (void)s.set_sprint_input(false);
    return t;
}

// The toolbox where it lies taken up: walked to (his face on her as he
// goes), looked at, picked up. `far_side`: from its side away from her
// (below); else straight from where he is.
// The way from him to it, flat.
double ux0(const Vector3 &p, const Vector3 &b) {
    return (b.x - p.x) / std::max(flat(b, p), 1.0e-3);
}
double uz0(const Vector3 &p, const Vector3 &b) {
    return (b.z - p.z) / std::max(flat(b, p), 1.0e-3);
}

struct TakeUp {
    bool taken = false;
    double hands_s = -1.0;        // first tick in his hands
    double claim_end_s = -1.0;    // her claim on it ended
};

TakeUp take_up(Simulation &s, Ledger &log, const std::uint64_t box, const bool far_side) {
    TakeUp out;
    const auto note = [&]() {
        if (out.claim_end_s < 0.0 && s.matron_state().stock_thrown != box) {
            out.claim_end_s = log.ticks / kHz;
        }
        if (out.hands_s < 0.0 && s.snapshot().carrying_entity_id == box) {
            out.hands_s = log.ticks / kHz;
        }
    };
    // Taken up from its far side from her, facing her: lifted, it is in
    // front of him and toward her (taken facing away, it drags round his
    // body slowly to the front, and out of his hands as he steps). Round it
    // by its side nearer him, never through it.
    bool rounded = !far_side;
    for (int i = 0; i < static_cast<int>(12.0 * kHz); ++i) {
        const auto b = s.kit_body_position(log.body);
        const auto p = s.snapshot().player_position;
        const auto her = s.matron_state().position;
        const double hb = std::max(flat(her, b), 1.0e-3);
        const double hx = (her.x - b.x) / hb;   // from the box toward her
        const double hz = (her.z - b.z) / hb;
        const double side = (p.x - b.x) * -hz + (p.z - b.z) * hx >= 0.0 ? 1.0 : -1.0;
        const double sx = b.x - hx * 0.9;       // where he lifts it from
        const double sz = b.z - hz * 0.9;
        const double wx = b.x - hz * side * 1.1 - hx * 0.4;   // beside it, on his side
        const double wz = b.z + hx * side * 1.1 - hz * 0.4;
        rounded = rounded || std::hypot(p.x - wx, p.z - wz) < 0.4 ||
                  ((p.x - b.x) * hx + (p.z - b.z) * hz) < -0.3;
        const double tx = !far_side ? b.x - ux0(p, b) * 0.85 : rounded ? sx : wx;
        const double tz = !far_side ? b.z - uz0(p, b) * 0.85 : rounded ? sz : wz;
        const double d = std::hypot(tx - p.x, tz - p.z);
        const double ux = (b.x - p.x) / std::max(flat(b, p), 1.0e-3);
        const double uz = (b.z - p.z) / std::max(flat(b, p), 1.0e-3);
        if (s.snapshot().carry_target_entity_id == box && rounded) {
            (void)s.set_move_input(0.0, 0.0);
            (void)s.request_pick_up();
            for (int k = 0; k < static_cast<int>(0.6 * kHz); ++k) {
                tick(s);
                log.step(s, false);
                note();
            }
            (void)s.set_look_pitch(0.0);
            out.taken = s.snapshot().carrying_entity_id == box;
            return out;
        }
        if (d > 0.15) {
            (void)s.set_move_input((tx - p.x) / d, (tz - p.z) / d);
            look_at_her(s);
        } else {
            (void)s.set_move_input(0.0, 0.0);
            (void)s.set_facing(ux, uz);
            (void)s.set_look_pitch(-0.6);
        }
        tick(s);
        log.step(s, false);
        note();
    }
    return out;
}

// Thrown back into her: his face on her, a step in to within his throw's
// reach, the drawn path (throw_prediction) put onto her by the pitch of his
// view as a thumb would, then the up gesture. Returns whether it threw.
bool throw_back(Simulation &s, Ledger &log, const double within) {
    (void)s.set_look_pitch(0.0);
    // First his face on her, standing, until the box has come round in front
    // of him and stopped swinging (turned on the move, it swings out of his
    // hands, as her heave's own run found: sim_tests_matron.cpp).
    {
        auto was = s.kit_body_position(log.body);
        int settled = 0;
        for (int i = 0; i < static_cast<int>(3.0 * kHz) && settled < static_cast<int>(kHz / 4.0); ++i) {
            (void)s.set_move_input(0.0, 0.0);
            look_at_her(s);
            tick(s);
            log.step(s, false);
            const auto m = s.matron_state();
            const auto p = s.snapshot().player_position;
            const auto b = s.kit_body_position(log.body);
            const double d = std::max(flat(m.position, p), 1.0e-3);
            const bool in_front = ((b.x - p.x) * (m.position.x - p.x) + (b.z - p.z) * (m.position.z - p.z)) / d > 0.3;
            settled = in_front && flat(b, was) < 0.002 ? settled + 1 : 0;
            was = b;
        }
    }
    for (int i = 0; i < static_cast<int>(10.0 * kHz); ++i) {
        const auto m = s.matron_state();
        const auto p = s.snapshot().player_position;
        const double d = flat(m.position, p);
        look_at_her(s);
        if (d > within) {
            (void)s.set_move_input((m.position.x - p.x) / d, (m.position.z - p.z) / d);
            tick(s);
            log.step(s, false);
            continue;
        }
        (void)s.set_move_input(0.0, 0.0);
        // The drawn path read at each pitch (the view moves; the world steps
        // on, as under a thumb): the one ending on her body, between her
        // knees and her shoulders, is the aim.
        double best = 1.0e9;
        double pitch = 0.0;
        for (double a = -0.3; a <= 0.9; a += 0.05) {
            (void)s.set_look_pitch(a);
            look_at_her(s);
            tick(s);
            log.step(s, false);
            const auto path = s.throw_prediction();
            if (path.size() < 2) {
                continue;
            }
            const auto end = path.back();
            const auto her = s.matron_state().position;
            const double miss = std::hypot(flat(end, her), std::max(0.0, std::abs(end.y - (her.y + 0.2)) - 0.5));
            if (miss < best) {
                best = miss;
                pitch = a;
            }
        }
        (void)s.set_look_pitch(pitch);
        look_at_her(s);
        tick(s);
        log.step(s, false);
        std::cout << "    " << log.name << " aim t=" << log.ticks / kHz << ": " << flat(s.matron_state().position,
                                                                                         s.snapshot().player_position)
                  << " m from her, view pitch " << pitch << ", the drawn path ends " << best << " m off her (up "
                  << int(s.snapshot().up_move) << ", carrying " << s.snapshot().carrying_entity_id << ", her state "
                  << int(s.matron_state().state) << ", stalking " << s.matron_state().stalking << ")\n";
        if (s.snapshot().up_move != scraperx::sim::UpMove::Throw) {
            return false;
        }
        return s.request_up();
    }
    return false;
}

}   // namespace

// (b) Stood still: the same throw hits him.
void run_ammo_stand() {
    Simulation s(InitialSpawn::Deck154);
    const std::uint64_t box = handled_toolbox(s, "ammo stand");
    Ledger log;
    log.start(s, "ammo stand", box);
    const Throw t = her_throw(s, log, false);
    const auto p = s.snapshot();
    std::cout << "  ammo stand: windup seen " << t.windup_seen_s << " s, released " << t.released_s << " s at "
              << t.release_gap_m << " m, top speed " << t.top_speed << " m/s, nearest " << t.nearest_m
              << " m, hit " << t.hit << " at " << t.hit_mps << " m/s, arm broken " << p.arm_broken << ", deaths "
              << p.death_count << "; its contacts " << log.box_hits << " (" << log.box_hits_silent
              << " while hers)\n";
    require(t.thrown && t.windup_seen_s > 0.0 && t.windup_seen_s < t.released_s,
            "ammo stand: she rears back (seen), then throws the toolbox at him");
    require(t.hit && t.hit_mps >= 6.0, "ammo stand: stood still, it hits him: a hit the body feels");
    require(p.death_count == 0, "ammo stand: a pest's throw is no death");
    std::cout << "  ammo stand: her claim on it (its landing no sound to her) ended " << t.claim_end_s - t.released_s
              << " s after it left her hands, " << t.claim_end_s - t.hit_s << " s after it struck him\n";
    require(t.claim_end_s >= t.hit_s && t.claim_end_s - t.hit_s <= 3.0 / kHz &&
                t.claim_end_s - t.released_s < static_cast<double>(Matron::kThrowQuietSeconds) - 0.5,
            "ammo stand: struck against him, it is hers no longer: its next hits are sounds to her");
    std::cout << "PASS scraperx_sim ammo stand: hit_mps=" << t.hit_mps << " nearest_m=" << t.nearest_m << "\n";
}

// Her claim ended by his hands (#240 ruling, 11-05): dodged, the toolbox
// taken up at once from where he stands, inside her quiet.
void run_ammo_hands() {
    Simulation s(InitialSpawn::Deck154);
    const std::uint64_t box = handled_toolbox(s, "ammo hands");
    Ledger log;
    log.start(s, "ammo hands", box);
    const Throw t = her_throw(s, log, true);
    require(t.thrown && !t.hit, "ammo hands: her throw, dodged");
    require(log.box_hits_silent > 0 && s.matron_state().stock_thrown == box,
            "ammo hands: untouched, its landing is no sound to her");
    const TakeUp up = take_up(s, log, box, false);
    std::cout << "  ammo hands: in his hands " << up.hands_s - t.released_s << " s after it left hers; her claim ended "
              << up.claim_end_s - t.released_s << " s after (" << up.claim_end_s - up.hands_s << " s from his hands)\n";
    require(up.taken && up.hands_s - t.released_s < static_cast<double>(Matron::kThrowQuietSeconds),
            "ammo hands: taken up inside her quiet");
    require(up.claim_end_s > 0.0 && std::abs(up.claim_end_s - up.hands_s) <= 2.0 / kHz,
            "ammo hands: in his hands it is hers no longer: its hits are sounds to her");
    std::cout << "PASS scraperx_sim ammo hands: claim_end_s=" << up.claim_end_s - t.released_s
              << " hands_s=" << up.hands_s - t.released_s << "\n";
}

namespace {

// (a)'s chain, shared by the race: her throw read and dodged, the toolbox
// taken up facing her, thrown back into her and read through her blow.
struct Chain {
    Throw her;
    TakeUp up;
    bool thrown_back = false;
    double blow_ns = 0.0;
    double blow_mps = 0.0;
    const char *outcome = "";
    std::uint32_t knockdowns = 0;
};

Chain dodge_and_throw_back(Simulation &s, Ledger &log, const std::uint64_t box) {
    Chain c;
    c.her = her_throw(s, log, true);
    const Throw &t = c.her;
    std::cout << "  " << log.name << ": windup seen " << t.windup_seen_s << " s, released " << t.released_s << " s at "
              << t.release_gap_m << " m, top speed " << t.top_speed << " m/s, stepped " << t.stepped_m
              << " m, nearest " << t.nearest_m << " m, hit " << t.hit << ", at rest (" << t.rest.x << ", " << t.rest.y
              << ", " << t.rest.z << "); its contacts " << log.box_hits << " (" << log.box_hits_silent
              << " while hers, no sound to her)\n";
    require(t.thrown && t.windup_seen_s > 0.0 && t.windup_seen_s < t.released_s,
            "ammo dodge: she rears back (seen), then throws");
    require(!t.hit && t.nearest_m > 0.6, "ammo dodge: a sidestep as it leaves her hands, and it misses him");
    require(log.box_hits_silent > 0 && s.matron_state().stock_thrown == box,
            "ammo dodge: untouched, her own throw's landing is still no sound to her");
    c.up = take_up(s, log, box, true);
    std::cout << "  " << log.name << ": in his hands " << c.up.hands_s - t.released_s
              << " s after it left hers (her claim ended " << c.up.claim_end_s - t.released_s << " s after)\n";
    require(c.up.taken, "ammo dodge: he takes the toolbox up where it lies, facing her");
    const auto m0 = s.matron_state();
    const std::uint64_t throws0 = s.snapshot().throw_count;
    c.thrown_back = throw_back(s, log, 2.8);
    require(c.thrown_back, "ammo dodge: up is THROW with the toolbox in his hands, and he throws");
    for (int i = 0; i < static_cast<int>(1.0 * kHz); ++i) {
        tick(s);
        log.step(s, true);
        const auto m = s.matron_state();
        c.blow_ns = m.damage_ns - m0.damage_ns;
        c.blow_mps = std::max(c.blow_mps, m.blow_mps);
    }
    const auto m = s.matron_state();
    c.knockdowns = m.knockdowns - m0.knockdowns;
    c.outcome = m.knockdowns > m0.knockdowns ? "knocked down" : m.staggers > m0.staggers ? "staggered" : "taken on her feet";
    std::cout << "  " << log.name << ": thrown back (" << s.snapshot().throw_count - throws0 << "), her blow impulse "
              << c.blow_ns << " N s, blow " << c.blow_mps << " m/s, " << c.outcome << ", her state " << int(m.state)
              << ", deaths " << s.snapshot().death_count << "\n";
    require(s.snapshot().throw_count == throws0 + 1 && c.blow_ns > 0.0 && c.blow_mps > 0.0,
            "ammo dodge: thrown back, it hits her through her take_blow");
    return c;
}

}   // namespace

// (a) Read, dodged, retrieved and thrown back into her.
void run_ammo_dodge() {
    Simulation s(InitialSpawn::Deck154);
    const std::uint64_t box = handled_toolbox(s, "ammo dodge");
    Ledger log;
    log.start(s, "ammo dodge", box);
    const Chain c = dodge_and_throw_back(s, log, box);
    require(s.snapshot().death_count == 0, "ammo dodge: nobody dead");
    std::cout << "PASS scraperx_sim ammo dodge: nearest_m=" << c.her.nearest_m << " blow_ns=" << c.blow_ns
              << " blow_mps=" << c.blow_mps << " outcome=" << c.outcome << "\n";
}

// (c) The race: thrown back, the toolbox lies by her. He backs off from her
// (his face on her), and as she goes for it he runs for it too; she is the
// nearer, and takes it first.
void run_ammo_race() {
    Simulation s(InitialSpawn::Deck154);
    const std::uint64_t box = handled_toolbox(s, "ammo race");
    Ledger log;
    log.start(s, "ammo race", box);
    (void)dodge_and_throw_back(s, log, box);
    // Back off, facing her, to 4 m.
    for (int i = 0; i < static_cast<int>(3.0 * kHz); ++i) {
        const auto m = s.matron_state();
        const auto p = s.snapshot().player_position;
        const double d = flat(m.position, p);
        if (d >= 4.0 || m.stock_seeking == box || m.carrying == box) {
            break;
        }
        (void)s.set_move_input(-(m.position.x - p.x) / d, -(m.position.z - p.z) / d);
        look_at_her(s);
        tick(s);
        log.step(s, false);
    }
    (void)s.set_move_input(0.0, 0.0);
    // Waiting, his face on her, until she goes for it.
    double went_s = -1.0;
    for (int i = 0; i < static_cast<int>(10.0 * kHz) && went_s < 0.0; ++i) {
        look_at_her(s);
        tick(s);
        log.step(s, false);
        const auto m = s.matron_state();
        if (m.stock_seeking == box || m.carrying == box) {
            went_s = log.ticks / kHz;
        }
    }
    require(went_s > 0.0, "ammo race: she goes for the toolbox lying by her");
    const auto b0 = s.kit_body_position(log.body);
    const auto m0 = s.matron_state();
    const auto p0 = s.snapshot().player_position;
    const double hers_m = flat(m0.position, b0);
    const double his_m = flat(p0, b0);
    std::cout << "  ammo race: she goes for it at t=" << went_s << ": her " << hers_m << " m from it, him " << his_m
              << " m; he runs for it\n";
    // His run for it: flat out at it, his hands on it as soon as it is his to take.
    double took_s = -1.0;
    bool his = false;
    for (int i = 0; i < static_cast<int>(4.0 * kHz) && took_s < 0.0 && !his; ++i) {
        const auto b = s.kit_body_position(log.body);
        const auto p = s.snapshot().player_position;
        const double d = std::max(flat(b, p), 1.0e-3);
        if (s.snapshot().carry_target_entity_id == box) {
            (void)s.request_pick_up();
        }
        (void)s.set_move_input((b.x - p.x) / d, (b.z - p.z) / d);
        (void)s.set_facing((b.x - p.x) / d, (b.z - p.z) / d);
        (void)s.set_look_pitch(d < 1.5 ? -0.6 : 0.0);
        (void)s.set_sprint_input(true);
        tick(s);
        log.step(s, true);
        if (s.matron_state().carrying == box) {
            took_s = log.ticks / kHz;
        }
        his = s.snapshot().carrying_entity_id == box;
    }
    (void)s.set_move_input(0.0, 0.0);
    (void)s.set_sprint_input(false);
    const auto p1 = s.snapshot().player_position;
    std::cout << "  ammo race: she has it " << took_s - went_s << " s after she went for it; he got to "
              << flat(p1, s.kit_body_position(log.body)) << " m of it, his hands on it " << his << "\n";
    require(took_s > 0.0 && !his, "ammo race: she gets to it first and takes it");
    require(s.snapshot().death_count == 0, "ammo race: nobody dead");
    std::cout << "PASS scraperx_sim ammo race: her_m=" << hers_m << " his_m=" << his_m << " took_s=" << took_s - went_s
              << "\n";
}

// (d) Thrown to distract her: her back to him, he throws the second toolbox
// off to his side; its landing is heard (its loudness) and she turns to it.
void run_ammo_distract() {
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    s.debug_fix_matron_pace(3);
    require(s.advance_frame(1.0).accepted, "ammo distract: settle");
    // The second toolbox, by the north rail east of the way out.
    require(walk_to(s, -4.4, -126.05, 6.0, 0.08), "ammo distract: to the toolbox");
    (void)s.set_facing(0.0, 1.0);
    (void)s.advance_frame(0.4);
    const std::uint64_t box = s.snapshot().carry_target_entity_id;
    require(box == LooseStock::kFirstEntity + s.loose_stock_throwables_begin() + 1, "ammo distract: the second toolbox in reach");
    (void)s.request_pick_up();
    (void)s.advance_frame(0.6);
    require(s.snapshot().carrying_entity_id == box, "ammo distract: picked up");
    require(go(s, -4.6, Matron::kBandNorthZ, false, 12.0, 0.2) && go(s, 0.4, Matron::kBandNorthZ, false, 12.0, 0.2) &&
                go(s, 3.0, Matron::kBandNorthZ, false, 12.0, 0.2),
            "ammo distract: east along the band with it, across the bay on its leaf");
    stand_still(s, 0.4);
    // Her on the band 6 m west of him, her back to him (her way to her post
    // is on west, away from him).
    s.debug_place_matron({-3.0, 154.0, Matron::kBandNorthZ});
    s.debug_face_matron(-0.5 * 3.14159265);
    // Something heard on west along the band (a lead with an age, as a
    // sound gives her): she goes off after it.
    s.debug_matron_hear({-14.0, 154.0, Matron::kBandNorthZ});
    // His view down the band toward the outer rail side, the box come round
    // in front and still.
    Ledger log;
    log.start(s, "ammo distract", box);
    auto was = s.kit_body_position(log.body);
    for (int i = 0, still = 0; i < static_cast<int>(3.0 * kHz) && still < static_cast<int>(kHz / 4.0); ++i) {
        (void)s.set_facing(0.6, 0.8);
        tick(s);
        log.step(s, false);
        const auto b = s.kit_body_position(log.body);
        still = flat(b, was) < 0.002 ? still + 1 : 0;
        was = b;
    }
    const auto m0 = s.matron_state();
    const double yaw0 = m0.yaw;
    require(!m0.sees_player, "ammo distract: her back to him, she does not see him");
    require(s.snapshot().up_move == scraperx::sim::UpMove::Throw && s.request_up(), "ammo distract: thrown");
    bool landed = false;
    Vector3 at{};
    double loud = 0.0;
    double turned = 1.0e9;      // closest she came to facing it, within 1 s of hearing it
    double off_heard = 0.0;     // off it as she heard it
    double off_unseen = 0.0;    // off it the last tick before she saw him
    double heard_s = -1.0;
    double saw_s = -1.0;
    for (int i = 0; i < static_cast<int>(4.0 * kHz); ++i) {
        (void)s.set_move_input(0.0, 0.0);
        tick(s);
        const std::uint32_t hits = log.box_hits;
        log.step(s, true);
        const auto m = s.matron_state();
        if (!landed && log.box_hits > hits && log.loudest > 0.0) {
            landed = true;
            at = s.kit_body_position(log.body);
            loud = log.loudest;
        }
        if (landed && heard_s < 0.0 && m.lead_sense == 3 && std::hypot(m.lead_x - at.x, m.lead_z - at.z) < 1.0) {
            heard_s = log.ticks / kHz;
        }
        if (saw_s < 0.0 && m.sees_player) {
            saw_s = log.ticks / kHz;
        }
        if (heard_s > 0.0 && log.ticks / kHz <= heard_s + 1.0) {
            const double want = std::atan2(at.x - m.position.x, at.z - m.position.z);
            const double off = std::abs(std::remainder(m.yaw - want, 2.0 * 3.14159265));
            if (log.ticks / kHz == heard_s) {
                off_heard = off;
            }
            if (saw_s < 0.0 || log.ticks / kHz < saw_s) {
                off_unseen = off;
            }
            turned = std::min(turned, off);
        }
    }
    const auto m = s.matron_state();
    std::cout << "  ammo distract: landed at (" << at.x << ", " << at.z << "), loudness " << loud << ", " << flat(at, m0.position)
              << " m from her; heard at t=" << heard_s << " (lead (" << m.lead_x << ", " << m.lead_z << ") sense "
              << int(m.lead_sense) << "); her yaw " << yaw0 << " -> " << m.yaw << ", off the landing " << turned
              << " rad at the closest (" << off_heard << " as she heard it, " << off_unseen
              << " before she saw him); she saw him at t=" << saw_s << "\n";
    require(landed && loud > 0.0, "ammo distract: its landing makes a sound");
    require(heard_s > 0.0 && (saw_s < 0.0 || heard_s <= saw_s), "ammo distract: she hears it land, before she sees him");
    require(off_unseen < off_heard - 0.5 && turned < 0.5, "ammo distract: and turns to it");
    std::cout << "PASS scraperx_sim ammo distract: loudness=" << loud << " heard_s=" << heard_s << " off_rad=" << turned
              << "\n";
}

void run_ammo() {
    run_ammo_stand();
    run_ammo_hands();
    run_ammo_dodge();
    run_ammo_race();
    run_ammo_distract();
}
