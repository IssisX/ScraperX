// The Matron (owner's chase, 2026-10-16): the sealed locker on the 154 m
// deck, her build-up, her hunt, the catch, and the one thing that kills her,
// the rotten catwalk. Every leg is on player inputs from the deck spawn.
#include "sim_tests.hpp"

#include "sim/impacts.hpp"
#include "sim/loose_stock.hpp"
#include "sim/matron.hpp"
#include "sim/zipline.hpp"

namespace {
using scraperx::sim::ImpactLedger;
using scraperx::sim::LooseStock;
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::MatronSnapshot;
using scraperx::sim::Simulation;
using scraperx::sim::Zipline;

constexpr int kStateDormant = static_cast<int>(Matron::State::Dormant);
constexpr int kStateStraining = static_cast<int>(Matron::State::Straining);
constexpr int kStateEmerging = static_cast<int>(Matron::State::Emerging);
constexpr int kStateHunting = static_cast<int>(Matron::State::Hunting);
constexpr int kStateDead = static_cast<int>(Matron::State::Dead);
constexpr int kStateGuarding = static_cast<int>(Matron::State::Guarding);
constexpr int kStatePeering = static_cast<int>(Matron::State::Peering);
constexpr int kStateSearching = static_cast<int>(Matron::State::Searching);

// One tick of a stick held toward (x, z); true once within `tolerance` of it.
bool steer(Simulation &s, const double x, const double z, const bool sprint, const double tolerance = 0.4) {
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

void tick(Simulation &s) {
    (void)s.advance_frame(Simulation::kFixedStepSeconds);
}

// Walks to (x, z) within `seconds`; true when it got there.
bool go(Simulation &s, const double x, const double z, const bool sprint, const double seconds,
        const double tolerance = 0.4) {
    const auto ticks = static_cast<int>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (int i = 0; i < ticks; ++i) {
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

double matron_player_distance(const Simulation &s) {
    const auto m = s.matron_state();
    const auto p = s.snapshot().player_position;
    return std::hypot(m.position.x - p.x, m.position.z - p.z);
}

// A fresh deck spawn, the sprint that wakes her, and the crouched walk to the
// middle of the east band (the catwalk's east end), where the player waits
// quietly for her.
void wake_and_wait_east(Simulation &s) {
    require(s.advance_frame(1.0).accepted, "matron: settle");
    require(go(s, 10.0, -128.5, true, 4.0, 1.0), "matron: a sprint toward her locker");
    (void)s.set_crouch_input(true);
    require(go(s, Matron::kBandX, Matron::kBandNorthZ, false, 40.0) &&
                go(s, Matron::kBandX, Matron::kMidZ, false, 40.0),
            "matron: a crouched walk to the catwalk's east end");
    (void)s.set_move_input(0.0, 0.0);
}

// Waits (quietly, still) until she is hunting within `within` metres.
// Her pace is her own (Matron::Pace: she creeps, stalks, strides, runs, and
// stands listening when the player is quiet), so she is given twice the time
// a single run would have taken.
bool wait_for_her(Simulation &s, const double within, const double seconds) {
    const auto ticks = static_cast<int>(2.0 * seconds * static_cast<double>(Simulation::kTickRateHz));
    for (int i = 0; i < ticks; ++i) {
        tick(s);
        if (s.matron_state().state == kStateHunting && matron_player_distance(s) < within) {
            return true;
        }
    }
    return false;
}

struct CrossResult {
    bool crossed = false;
    MatronSnapshot end;
    std::uint32_t deaths = 0;
    double player_peak_load_n = 0.0;
};

// Sprints west along the catwalk, then keeps going until the far deck; `watch`
// ends the run early (when she is dead, say).
CrossResult sprint_across(Simulation &s, const bool until_dead) {
    CrossResult result;
    (void)s.set_crouch_input(false);
    for (int i = 0; i < 20 * static_cast<int>(Simulation::kTickRateHz); ++i) {
        const bool arrived = steer(s, -Matron::kBandX, Matron::kMidZ, true, 0.6);
        tick(s);
        const auto m = s.matron_state();
        if (arrived && (!until_dead || m.state == kStateDead)) {
            result.crossed = true;
            break;
        }
    }
    result.end = s.matron_state();
    result.deaths = static_cast<std::uint32_t>(s.snapshot().death_count);
    return result;
}

scraperx::sim::Vector3 stock(const Simulation &s, const int piece) {
    return s.kit_body_position(s.kit_body_index(LooseStock::kFirstEntity + static_cast<std::uint64_t>(piece)));
}

// Pushes stock piece `piece` toward (tx, tz) the way a person does: round
// behind it, then walking through it, until `done` says stop or the time runs out.
template <typename Done>
bool push_toward(Simulation &s, const int piece, const double tx, const double tz, const double seconds, Done done) {
    const auto ticks = static_cast<int>(seconds * static_cast<double>(Simulation::kTickRateHz));
    for (int i = 0; i < ticks; ++i) {
        const auto d = stock(s, piece);
        if (done(d)) {
            (void)s.set_move_input(0.0, 0.0);
            return true;
        }
        const auto p = s.snapshot().player_position;
        double gx = tx - d.x;
        double gz = tz - d.z;
        const double gl = std::hypot(gx, gz);
        gx /= gl;
        gz /= gl;
        const double bx = d.x - gx * 1.2;
        const double bz = d.z - gz * 1.2;
        const double px = bx - p.x;
        const double pz = bz - p.z;
        const double pl = std::hypot(px, pz);
        const double ax = d.x - p.x;
        const double az = d.z - p.z;
        const double along = (ax * px + az * pz) / (pl + 1.0e-9);
        const double side = std::abs(ax * pz - az * px) / (pl + 1.0e-9);
        double mx = 0.0;
        double mz = 0.0;
        if (pl > 0.45 && along > 0.0 && along < pl && side < 1.1) {
            double sx = -gz;
            double sz = gx;
            if ((p.x - d.x) * sx + (p.z - d.z) * sz < 0.0) {
                sx = -sx;
                sz = -sz;
            }
            // Round its side, toward its back.
            mx = d.x + sx * 1.7 - gx * 1.0 - p.x;
            mz = d.z + sz * 1.7 - gz * 1.0 - p.z;
        } else if (pl > 0.45) {
            mx = px;
            mz = pz;
        } else {
            mx = gx + 0.8 * (bx - p.x);
            mz = gz + 0.8 * (bz - p.z);
        }
        const double ml = std::hypot(mx, mz);
        (void)s.set_move_input(mx / ml * 0.7, mz / ml * 0.7);
        (void)s.set_facing(mx / ml, mz / ml);
        tick(s);
    }
    (void)s.set_move_input(0.0, 0.0);
    if (std::getenv("SCRAPERX_PUSH_TRACE") != nullptr) {
        const auto d = stock(s, piece);
        const auto p = s.snapshot().player_position;
        std::cout << "    push short: the piece at (" << d.x << ", " << d.y << ", " << d.z << "), the player at (" << p.x
                  << ", " << p.y << ", " << p.z << ")\n";
    }
    return false;
}
// Rides line `line` down from its stand: the jump for its bar, braked to come
// into the end's buffer at 5 m/s (it stops 5.4: it takes the rider to the
// stop, over the far band), and letting go there. True once down on the far
// band, alive.
bool ride_line_down(Simulation &s, const scraperx::sim::Simulation::ZiplineLine &line) {
    const double run = std::hypot(line.bottom.x - line.top.x, line.bottom.z - line.top.z);
    (void)s.set_move_input(0.0, 0.0);
    (void)s.set_sprint_input(false);
    (void)s.advance_frame(0.5);
    (void)s.set_facing((line.bottom.x - line.top.x) / run, (line.bottom.z - line.top.z) / run);
    (void)s.advance_frame(0.2);
    if (!(s.request_jump() &&
          advance_until(s, [](const scraperx::sim::Snapshot &n) { return n.zipline_riding; }, 1.2))) {
        return false;
    }
    const auto deaths = s.snapshot().death_count;
    for (int i = 0; i < 20 * static_cast<int>(Simulation::kTickRateHz); ++i) {
        const auto n = s.snapshot();
        const bool braking = n.zipline_riding && n.zipline_along_m > line.length - 12.0 && n.zipline_speed_mps > 5.0;
        (void)s.set_move_input(0.0, braking ? 1.0 : 0.0);
        tick(s);
        const auto m = s.snapshot();
        if (m.zipline_riding && m.zipline_along_m > line.length - 1.5 && std::abs(m.zipline_speed_mps) < 0.3) {
            (void)s.request_release();
        }
        if (!m.zipline_riding && m.player_grounded && m.player_position.y < line.top.y - 6.0) {
            (void)s.set_move_input(0.0, 0.0);
            return m.death_count == deaths;
        }
    }
    (void)s.set_move_input(0.0, 0.0);
    return false;
}

// Waits (still) until she stands guard at her post on her deck; with
// SCRAPERX_MATRON_TRACE set, her states on the way.
bool until_guarding(Simulation &s, const double seconds) {
    constexpr double kPostX = -10.3;
    constexpr double kPostZ = -129.0;
    int was = -1;
    for (int i = 0; i < static_cast<int>(seconds * static_cast<double>(Simulation::kTickRateHz)); ++i) {
        tick(s);
        const auto m = s.matron_state();
        if (std::getenv("SCRAPERX_MATRON_TRACE") != nullptr && (m.state != was || i % 450 == 0)) {
            const auto p = s.snapshot().player_position;
            std::cout << "    t " << i / 90.0 << " state " << int(m.state) << " at (" << m.position.x << ", "
                      << m.position.y << ", " << m.position.z << ") floor " << m.floor_y << " lead " << m.lead_floor_y
                      << " target (" << m.target_x << ", " << m.target_z << ") heard " << m.noise_heard << " fooled "
                      << m.fooled << " wary " << m.wary << "; player (" << p.x << ", " << p.y << ", " << p.z << ")\n";
            was = m.state;
        }
        if (m.state == kStateGuarding && std::hypot(m.position.x - kPostX, m.position.z - kPostZ) < 0.6) {
            return true;
        }
    }
    return false;
}

// The line whose top stands on the floor at `floor` (another level's below it).
std::uint32_t line_from(const Simulation &s, const double floor) {
    for (std::uint32_t i = 0; i < s.zipline_count(); ++i) {
        const auto line = s.zipline_line(i);
        if (std::abs(line.ramp_foot.y - floor) < 0.3 && line.bottom.y < line.top.y - 6.0) {
            return i;
        }
    }
    return s.zipline_count();
}

} // namespace

// Her body is not a blow (the owner, 2026-10-19: no death colliding with her
// at full speed): a sprint head-on into her as she comes, closing far past a
// lethal hit's speed, is no death; her hands decide, a pest's shove.
void run_matron_collision() {
    Simulation s(InitialSpawn::Deck154);
    s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
    require(s.advance_frame(1.0).accepted, "matron collision: settle");
    require(go(s, 8.0, Matron::kBandNorthZ, true, 4.0, 1.0) && s.matron_state().state == kStateStraining,
            "matron collision: a sprint toward her locker wakes her");
    (void)s.set_crouch_input(true);
    require(go(s, -12.0, Matron::kBandNorthZ, false, 30.0, 0.3), "matron collision: back west along the band, crouched");
    (void)s.set_crouch_input(false);
    stand_still(s, 0.2);
    require(wait_for_her(s, 18.0, 60.0), "matron collision: out, she comes along the band");
    // Her hands held off, so it is her body the sprint meets (a shove would
    // throw the climber back first: her hands reach 0.85 m, her body 0.65 m).
    s.debug_hold_matron_hands(4.0);
    const auto deaths = s.snapshot().death_count;
    constexpr double kLethalHitMps = 10.0;   // kHitLethalSpeed
    const double hz = static_cast<double>(Simulation::kTickRateHz);
    auto last = s.matron_state().position;
    double closing_peak = 0.0;
    double nearest = 1.0e9;
    int after_contact = -1;
    for (int i = 0; i < 6 * static_cast<int>(Simulation::kTickRateHz) && after_contact < static_cast<int>(hz); ++i) {
        const auto her = s.matron_state().position;
        (void)steer(s, her.x, her.z, true, 0.0);
        tick(s);
        const auto m = s.matron_state();
        const auto p = s.snapshot();
        const double dx = m.position.x - p.player_position.x;
        const double dz = m.position.z - p.player_position.z;
        const double d = std::hypot(dx, dz);
        if (after_contact < 0 && d < 1.5) {
            // Closing speed along the line between them, his velocity less hers.
            const double vx = p.player_linear_velocity.x - (m.position.x - last.x) * hz;
            const double vz = p.player_linear_velocity.z - (m.position.z - last.z) * hz;
            closing_peak = std::max(closing_peak, (vx * dx + vz * dz) / std::max(d, 1.0e-3));
        }
        last = m.position;
        nearest = std::min(nearest, d);
        if (after_contact < 0 && (d < 0.7 || p.death_count != deaths)) {
            after_contact = 0;
        } else if (after_contact >= 0) {
            ++after_contact;
        }
    }
    (void)s.set_move_input(0.0, 0.0);
    (void)s.set_sprint_input(false);
    const auto m = s.matron_state();
    std::cout << "  matron collision: closing " << closing_peak << " m/s at contact, nearest " << nearest
              << " m, shoves " << m.shoves << ", deaths " << s.snapshot().death_count - deaths << ", stage "
              << static_cast<int>(m.stage) << "\n";
    require(after_contact >= 0 && closing_peak > kLethalHitMps,
            "matron collision: the sprint met her head-on, closing past a lethal hit's speed");
    require(s.snapshot().death_count == deaths && m.stage == 0 && m.shoves == 0,
            "matron collision: running into her body, a pest, at full speed is no death");
    require(m.knockdowns == 0 && m.staggers == 0, "matron collision: the player's own body is no blow to her balance");
    std::cout << "PASS scraperx_sim matron collision: closing_mps=" << closing_peak << " shoves=" << m.shoves << "\n";
}

// Her balance (the owner, 2026-10-19: a hard enough blow puts her down for a
// few seconds, never dead). With the player out of her way on 143 below, she
// stands guard at her post; stock is sent at her as a throw would send it.
void run_matron_balance() {
    constexpr int kStaggering = static_cast<int>(Matron::State::Staggering);
    constexpr int kDown = static_cast<int>(Matron::State::Down);
    Simulation s(InitialSpawn::Deck154);
    s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
    require(s.advance_frame(1.0).accepted, "matron balance: settle");
    require(go(s, 8.0, Matron::kBandNorthZ, true, 4.0, 1.0) && s.matron_state().state == kStateStraining,
            "matron balance: a sprint toward her locker wakes her");
    const auto line = s.zipline_line(line_from(s, Matron::kDeckTop));
    require(walk_to(s, line.ramp_foot.x, line.ramp_foot.z, 6.0, 0.3) && walk_to(s, line.stand.x, line.stand.z, 6.0, 0.12),
            "matron balance: up the line's ramp onto its stand");
    require(ride_line_down(s, line), "matron balance: down the line to 143 m while she strains");
    (void)s.set_crouch_input(true);
    require(go(s, -Matron::kBandX, Matron::kBandSouthZ, false, 40.0, 1.0) &&
                go(s, -Matron::kBandX, Matron::kBandNorthZ, false, 60.0, 1.0) && go(s, -16.0, -132.6, false, 20.0, 0.3),
            "matron balance: crouched round to 143's north edge of the shaft");
    require(until_guarding(s, 120.0) && s.snapshot().death_count == 0, "matron balance: she stands guard at her post");
    const std::uint32_t first = s.loose_stock_count() - static_cast<std::uint32_t>(LooseStock::kMatronThrowables);
    const std::uint32_t toolbox = s.kit_body_index(LooseStock::kFirstEntity + first);
    const std::uint32_t drum = s.kit_body_index(LooseStock::kFirstEntity + first + 2);
    const auto deaths = s.snapshot().death_count;
    // `piece` sent at her from `ahead_m` north of her (the way she faces at
    // her post), `rise_m` over her feet, at `mps`; the worst it does to her
    // over the contact (Down over Staggering; -1 for neither).
    // Her stage set as each blow comes (her rage cools while she walks back
    // to her post, and would bring her down a stage).
    int stage = 0;
    const auto send = [&](const std::uint32_t piece, const double ahead_m, const double rise_m, const double mps) {
        s.debug_set_matron_stage(static_cast<std::uint8_t>(stage));
        const auto m = s.matron_state();
        const double feet = m.position.y - static_cast<double>(Matron::kStandCentre);
        s.debug_launch_kit_body(piece, {m.position.x, feet + rise_m, m.position.z + ahead_m}, {0.0, 0.0, -mps});
        int knocked = -1;
        for (int i = 0; i < static_cast<int>(0.6 * static_cast<double>(Simulation::kTickRateHz)) && knocked != kDown; ++i) {
            tick(s);
            if (std::getenv("SCRAPERX_BALANCE_TRACE") != nullptr) {
                const auto h = s.matron_state();
                const auto p = s.kit_body_position(piece);
                const auto v = s.kit_body_velocity(piece);
                std::cout << "    tick " << i << " her (" << h.position.x << ", " << h.position.y << ", " << h.position.z
                          << ") speed " << h.speed << " state " << int(h.state) << " blow " << h.blow_mps << "; piece ("
                          << p.x << ", " << p.y << ", " << p.z << ") v (" << v.x << ", " << v.y << ", " << v.z << ")\n";
            }
            const int state = s.matron_state().state;
            if (state == kDown || (state == kStaggering && knocked < 0)) {
                knocked = state;
            }
        }
        return knocked;
    };
    // Up again within her stage's time on the steel (and room to stand), on
    // her own deck, alive.
    const auto rises = [&](const double seconds) {
        for (int i = 0; i < static_cast<int>(seconds * static_cast<double>(Simulation::kTickRateHz)); ++i) {
            tick(s);
            const auto m = s.matron_state();
            if (m.state != kDown && m.state != kStaggering) {
                return m.alive && std::abs(m.floor_y - static_cast<double>(Matron::kDeckTop)) < 0.1 &&
                       std::abs(m.position.y - (static_cast<double>(Matron::kDeckTop) + Matron::kStandCentre)) < 0.2;
            }
        }
        return false;
    };
    // A pest, upright: a toolbox heaved at her chest (5.8 m/s, an arms'
    // throw) puts her down.
    {
        const int knocked = send(toolbox, 2.0, 1.3, 5.8);
        const auto m = s.matron_state();
        std::cout << "  matron balance: a toolbox at a pest's chest, blow " << m.blow_mps << " m/s, state " << knocked
                  << ", knockdowns " << m.knockdowns << ", fell (" << m.fall_x << ", " << m.fall_z << ")"
                  << (m.kneeling ? " kneeling" : " lying") << "\n";
        require(knocked == kDown && m.knockdowns == 1 && m.staggers == 0,
                "matron balance: a toolbox heaved at an upright pest puts her down");
        // Down she has no hands: the rage it costs her, and up again.
        require(m.rage >= Matron::kKnockdownRage - 0.01, "matron balance: a knockdown feeds her rage");
        require(rises(static_cast<double>(Matron::kPestDownSeconds) + 2.0), "matron balance: up again, on her deck, alive");
        std::cout << "PASS scraperx_sim matron balance pest: blow_mps=" << m.blow_mps << "\n";
    }
    // Dangerous: the same toolbox, and a drum at her chest, only stagger her;
    // the drum rolled at her shins puts her down.
    stage = 1;
    require(until_guarding(s, 120.0), "matron balance: dangerous, back at her post");
    {
        const int by_box = send(toolbox, 2.0, 1.3, 5.8);
        const auto boxed = s.matron_state();
        std::cout << "  matron balance: a toolbox at a dangerous chest, blow " << boxed.blow_mps << " m/s, state " << by_box
                  << ", at (" << boxed.position.x << ", " << boxed.position.z << ") yaw " << boxed.yaw << "\n";
        require(by_box == kStaggering && rises(2.0), "matron balance: a toolbox only staggers a dangerous Matron");
        require(until_guarding(s, 120.0), "matron balance: dangerous, at her post again");
        const int by_drum = send(drum, 1.25, 1.35, 3.3);
        const auto m = s.matron_state();
        std::cout << "  matron balance: a drum at a dangerous chest, blow " << m.blow_mps << " m/s, state " << by_drum
                  << "\n";
        require(by_drum == kStaggering && m.knockdowns == 1 && rises(2.0),
                "matron balance: a drum at her chest only staggers a dangerous Matron");
        require(until_guarding(s, 120.0), "matron balance: dangerous, at her post once more");
        // The toolbox lying at her feet since it struck her, put back by the
        // rail (in the drum's way, it would take the drum's blow).
        s.debug_launch_kit_body(toolbox, {-6.5, static_cast<double>(Matron::kDeckTop) + 0.13, -125.0}, {0.0, 0.0, 0.0});
        const int low = send(drum, 0.8, 0.47, 3.3);
        const auto d = s.matron_state();
        std::cout << "  matron balance: a drum at a dangerous Matron's shins, blow " << d.blow_mps << " m/s, state " << low
                  << "\n";
        require(low == kDown && d.knockdowns == 2, "matron balance: a drum rolled at her shins takes her feet");
        require(rises(static_cast<double>(Matron::kDangerousDownSeconds) + 2.0), "matron balance: dangerous, up sooner");
        std::cout << "PASS scraperx_sim matron balance dangerous: staggers=" << d.staggers << " knockdowns=" << d.knockdowns
                  << "\n";
    }
    // Fully changed: a drum she faces only staggers her, however hard.
    stage = 2;
    require(until_guarding(s, 120.0), "matron balance: fully changed, at her post");
    {
        const auto before = s.matron_state();
        const int braced = send(drum, 1.25, 1.35, 7.0);
        const auto m = s.matron_state();
        std::cout << "  matron balance: a drum at 7 m/s into a fully changed Matron's face, blow " << m.blow_mps
                  << " m/s, state " << braced << "\n";
        require(braced == kStaggering && m.knockdowns == before.knockdowns,
                "matron balance: fully changed, a blow she faces only staggers her");
        require(rises(2.0), "matron balance: fully changed, she has her feet back");
    }
    require(s.snapshot().death_count == deaths && s.matron_state().alive && s.matron_state().catches == 0,
            "matron balance: no death, hers or the climber's");
    std::cout << "PASS scraperx_sim matron balance: knockdowns=" << s.matron_state().knockdowns
              << " staggers=" << s.matron_state().staggers << "\n";
    // ---- the goal's own beat: a pest coming for the climber upright, and
    //      the toolbox in their hands thrown at her (the up gesture) puts her
    //      down; up again a few seconds later, nobody dead ----
    {
        Simulation h(InitialSpawn::Deck154);
        h.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        require(h.advance_frame(1.0).accepted, "matron heave: settle");
        require(go(h, 8.0, Matron::kBandNorthZ, true, 4.0, 1.0) && h.matron_state().state == kStateStraining,
                "matron heave: a sprint toward her locker wakes her");
        const std::uint32_t box_piece = h.loose_stock_count() - static_cast<std::uint32_t>(LooseStock::kMatronThrowables) + 1;
        const std::uint64_t box = LooseStock::kFirstEntity + box_piece;
        // Back across the bay on its leaf (bay.hpp), then to the toolbox.
        require(walk_to(h, 0.4, Matron::kBandNorthZ, 20.0, 0.2) && walk_to(h, -4.6, Matron::kBandNorthZ, 20.0, 0.2) &&
                    walk_to(h, -4.4, -125.85, 20.0, 0.08),
                "matron heave: to the toolbox by the north rail");
        (void)h.set_facing(0.0, 1.0);
        (void)h.advance_frame(0.3);
        require(h.snapshot().carry_target_entity_id == box && h.request_pick_up(), "matron heave: the toolbox taken up");
        (void)h.advance_frame(0.6);
        require(h.snapshot().carrying_entity_id == box, "matron heave: the toolbox in the hands");
        // Onto her line west of the bay (bay.hpp): she comes off its leaf
        // straight along the band, and the throw's way to her is clear.
        require(walk_to(h, -6.6, Matron::kBandNorthZ, 10.0, 0.15), "matron heave: west along the band with it");
        // Still, the toolbox held, until she comes for the climber in sight:
        // from 9 m the view kept on her (where she will be a moment on, as a
        // thrower leads a runner), and the throw at 3.4 m.
        bool coming = false;
        auto last = h.matron_state().position;
        for (int i = 0; i < 90 * static_cast<int>(Simulation::kTickRateHz) && !coming; ++i) {
            tick(h);
            const auto m = h.matron_state();
            const auto p = h.snapshot().player_position;
            const double hz = static_cast<double>(Simulation::kTickRateHz);
            const double lead = 0.22;
            const double ax = m.position.x + (m.position.x - last.x) * hz * lead - p.x;
            const double az = m.position.z + (m.position.z - last.z) * hz * lead - p.z;
            last = m.position;
            const double d = std::hypot(m.position.x - p.x, m.position.z - p.z);
            if (m.state == kStateHunting && m.stage == 0 && d < 9.0) {
                (void)h.set_facing(ax / std::hypot(ax, az), az / std::hypot(ax, az));
                coming = d < 3.4;
            }
        }
        require(coming, "matron heave: a pest comes for the climber");
        tick(h);
        require(h.snapshot().up_move == scraperx::sim::UpMove::Throw && h.request_up(),
                "matron heave: up is THROW with the toolbox in the hands");
        const auto deaths_h = h.snapshot().death_count;
        bool downed = false;
        for (int i = 0; i < static_cast<int>(1.2 * static_cast<double>(Simulation::kTickRateHz)) && !downed; ++i) {
            tick(h);
            downed = h.matron_state().state == kDown;
            if (std::getenv("SCRAPERX_BALANCE_TRACE") != nullptr && i < 60) {
                const auto t = h.matron_state();
                const auto b = h.kit_body_position(h.kit_body_index(box));
                const auto pl = h.snapshot().player_position;
                std::cout << "    heave " << i << " her (" << t.position.x << ", " << t.position.y << ", " << t.position.z
                          << ") speed " << t.speed << " state " << int(t.state) << " blow " << t.blow_mps << "; box (" << b.x
                          << ", " << b.y << ", " << b.z << "); player (" << pl.x << ", " << pl.z << ")\n";
            }
        }
        const auto m = h.matron_state();
        std::cout << "  matron heave: thrown at a pest coming upright, blow " << m.blow_mps << " m/s, state " << int(m.state)
                  << ", knockdowns " << m.knockdowns << ", throws " << h.snapshot().throw_count << "\n";
        require(downed && m.knockdowns == 1 && h.snapshot().throw_count == 1,
                "matron heave: the toolbox thrown at a pest puts her down");
        stand_still(h, static_cast<double>(Matron::kPestDownSeconds) + 1.5);
        require(h.matron_state().state != kDown && h.matron_state().alive && h.snapshot().death_count == deaths_h,
                "matron heave: a few seconds down, then up; nobody dead");
        std::cout << "PASS scraperx_sim matron heave: blow_mps=" << m.blow_mps << "\n";
    }
}

// The shaft's corner pipe (the owner, 2026-10-19: she comes down the shaft
// after you; one shaft pipe is hers; you hear her at least 4 s before she
// arrives; a drum dropped on her knocks her off without killing her). The
// climber rides the line down to 143 while she strains and, once she is
// out, sprints along 143's north edge: heard up the shaft. `stage` is set
// as she comes out; `drum_at_feet` > 0 lets a drum go onto her as her feet
// pass that height. Returns what she did.
struct PipeRun {
    bool took = false;        // she set out along the pipe
    bool arrived = false;     // off it onto 143's band
    double warned_s = 0.0;    // from her first hold on it to her feet on 143
    bool head_down = false;   // seen coming down head first
    std::uint32_t holds = 0;
    std::uint32_t flanges = 0;
    std::uint32_t slams = 0;
    double slam_mps = 0.0;
    bool alive = true;
    std::uint64_t deaths = 0;
};
PipeRun pipe_run(const std::uint8_t stage, const double drum_at_feet, const double seconds) {
    PipeRun r;
    Simulation s(InitialSpawn::Deck154);
    s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
    require(s.advance_frame(1.0).accepted, "matron pipe: settle");
    require(go(s, 8.0, Matron::kBandNorthZ, true, 4.0, 1.0) && s.matron_state().state == kStateStraining,
            "matron pipe: a sprint toward her locker wakes her");
    const auto line = s.zipline_line(line_from(s, Matron::kDeckTop));
    require(walk_to(s, line.ramp_foot.x, line.ramp_foot.z, 6.0, 0.3) && walk_to(s, line.stand.x, line.stand.z, 6.0, 0.12),
            "matron pipe: up the line's ramp onto its stand");
    require(ride_line_down(s, line), "matron pipe: down the line to 143 m while she strains");
    (void)s.set_crouch_input(true);
    require(go(s, -Matron::kBandX, Matron::kBandSouthZ, false, 40.0, 1.0) &&
                go(s, -Matron::kBandX, Matron::kBandNorthZ, false, 60.0, 1.0) && go(s, -16.0, -132.6, false, 20.0, 0.3),
            "matron pipe: crouched round to 143's north edge");
    for (int i = 0; i < 40 * static_cast<int>(Simulation::kTickRateHz) && !s.matron_state().released; ++i) {
        tick(s);
    }
    require(s.matron_state().released, "matron pipe: she is out");
    s.debug_set_matron_stage(stage);
    // A sprint along 143's north edge, heard up the shaft; then still.
    (void)s.set_crouch_input(false);
    require(go(s, -2.0, -132.6, true, 8.0, 0.4), "matron pipe: a sprint along 143's north edge");
    (void)s.set_move_input(0.0, 0.0);
    (void)s.set_sprint_input(false);
    const std::uint32_t first = s.loose_stock_count() - static_cast<std::uint32_t>(LooseStock::kMatronThrowables);
    const std::uint32_t drum = s.kit_body_index(LooseStock::kFirstEntity + first + 2);
    const double hz = static_cast<double>(Simulation::kTickRateHz);
    int first_hold = -1;
    bool dropped = false;
    const auto deaths = s.snapshot().death_count;
    for (int i = 0; i < static_cast<int>(seconds * hz); ++i) {
        s.debug_set_matron_stage(stage);   // her rage held at the stage (it cools as she goes)
        tick(s);
        const auto m = s.matron_state();
        r.took = r.took || m.pipe_links > 0;
        r.head_down = r.head_down || m.pipe_head_down;
        if (first_hold < 0 && m.pipe_holds > 0) {
            first_hold = i;
        }
        const double feet = m.position.y - static_cast<double>(Matron::kStandCentre);
        const bool on_line = std::hypot(m.position.x - static_cast<double>(Matron::kPipeLineX),
                                        m.position.z - static_cast<double>(Matron::kPipeLineZ)) < 0.25;
        if (drum_at_feet > 0.0 && !dropped && m.on_pipe && on_line && !m.pipe_sliding && feet < drum_at_feet) {
            // Let go over the corner rail onto her, from a metre and a half up.
            s.debug_launch_kit_body(drum, {m.position.x, m.position.y + 2.6, m.position.z}, {0.0, -2.0, 0.0});
            dropped = true;
        }
        static int dropped_at = -1;
        if (dropped && dropped_at < 0) {
            dropped_at = i;
        }
        if (std::getenv("SCRAPERX_PIPE_DROP_TRACE") != nullptr && dropped && i - dropped_at < 70) {
            const auto dp = s.kit_body_position(drum);
            const auto dv = s.kit_body_velocity(drum);
            std::cout << "    drop " << i - dropped_at << " her y " << m.position.y << " blow " << m.blow_mps << " slams "
                      << m.pipe_slams << " sliding " << m.pipe_sliding << "; drum (" << dp.x << ", " << dp.y << ", "
                      << dp.z << ") v (" << dv.x << ", " << dv.y << ", " << dv.z << ")\n";
        }
        if (std::getenv("SCRAPERX_PIPE_TRACE") != nullptr && i % 45 == 0) {
            std::cout << "    t " << i / hz << " state " << int(m.state) << " at (" << m.position.x << ", "
                      << m.position.y << ", " << m.position.z << ") floor " << m.floor_y << " lead " << m.lead_floor_y
                      << " pipe " << m.on_pipe << " links " << m.pipe_links << " holds " << m.pipe_holds
                      << " slams " << m.pipe_slams << " stage " << int(m.stage) << "\n";
        }
        if (r.took && !m.on_pipe && std::abs(m.floor_y - 143.0) < 0.1) {
            r.arrived = true;
            r.warned_s = static_cast<double>(i - first_hold) / hz;
            break;
        }
        if (!m.alive) {
            break;
        }
    }
    const auto m = s.matron_state();
    r.holds = m.pipe_holds;
    r.flanges = m.pipe_flanges;
    r.slams = m.pipe_slams;
    r.slam_mps = m.pipe_slam_mps;
    r.alive = m.alive;
    r.deaths = s.snapshot().death_count - deaths;
    return r;
}

void run_matron_pipe() {
    // Frail, she cannot take hold of it.
    {
        const PipeRun r = pipe_run(0, 0.0, 40.0);
        std::cout << "  matron pipe: a pest with the climber heard below: took the pipe " << r.took << "\n";
        require(!r.took && r.alive, "matron pipe: frail, she does not take hold of the pipe");
    }
    // Dangerous: down it after the climber, heard all the way.
    {
        const PipeRun r = pipe_run(1, 0.0, 60.0);
        std::cout << "  matron pipe: dangerous, down to 143: arrived " << r.arrived << " after " << r.warned_s
                  << " s of holds (" << r.holds << " holds, " << r.flanges << " flanges)\n";
        require(r.took && r.arrived && r.alive, "matron pipe: dangerous, she comes down the shaft's pipe after the climber");
        require(r.warned_s >= 4.0, "matron pipe: heard on the pipe at least 4 s before she is on the climber's deck");
        require(r.flanges >= 1, "matron pipe: a flange passed on the way (a bong)");
        std::cout << "PASS scraperx_sim matron pipe dangerous: warned_s=" << r.warned_s << " holds=" << r.holds << "\n";
    }
    // Fully changed: head first, and still heard 4 s ahead.
    {
        const PipeRun r = pipe_run(2, 0.0, 60.0);
        std::cout << "  matron pipe: fully changed, down to 143: arrived " << r.arrived << " after " << r.warned_s
                  << " s, head first " << r.head_down << "\n";
        require(r.took && r.arrived && r.alive && r.head_down && r.warned_s >= 4.0,
                "matron pipe: fully changed, she comes down it head first, heard 4 s ahead");
        std::cout << "PASS scraperx_sim matron pipe changed: warned_s=" << r.warned_s << "\n";
    }
    // A drum let go onto her on the pipe: torn off, slammed onto the flange
    // under her, alive, and on she goes.
    {
        const PipeRun r = pipe_run(1, 154.2, 70.0);
        std::cout << "  matron pipe: a drum onto her as she starts down: slams " << r.slams << " at " << r.slam_mps
                  << " m/s, arrived " << r.arrived << ", alive " << r.alive << ", deaths " << r.deaths << "\n";
        require(r.slams == 1 && r.alive && r.deaths == 0, "matron pipe: a drum dropped on her knocks her off, alive");
        require(r.arrived, "matron pipe: torn off, she goes on down to 143 after it");
        std::cout << "PASS scraperx_sim matron pipe drum: slam_mps=" << r.slam_mps << "\n";
    }
}

// A pest's mischief: the player's toolbox taken and thrown, or carried off.
void run_matron_mischief() {
    // ---- a pest's mischief (the owner, 2026-10-19): a toolbox the player has
    //      handled is theirs to her; she goes for it, takes it and throws it at
    //      them; then her hands shove them ----
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        require(s.advance_frame(1.0).accepted, "matron mischief: settle");
        // The toolbox by the north band's outer rail, picked up and set down
        // on the band east of the way out.
        require(walk_to(s, -6.5, -125.95, 6.0, 0.08), "matron mischief: to the toolbox");
        (void)s.set_facing(0.0, 1.0);
        (void)s.advance_frame(0.4);
        const std::uint64_t box = s.snapshot().carry_target_entity_id;
        require(box != 0, "matron mischief: the toolbox is in the hands' reach");
        (void)s.request_pick_up();
        (void)s.advance_frame(0.4);
        require(s.snapshot().carrying_entity_id == box, "matron mischief: picked up");
        require(go(s, -4.6, Matron::kBandNorthZ, false, 12.0, 0.2) && go(s, 0.4, Matron::kBandNorthZ, false, 12.0, 0.2) &&
                    go(s, 2.0, -126.6, false, 12.0, 0.2),
                "matron mischief: carried east along the band, across the bay on its leaf");
        // Stopped, then set down (let go on the move, it slides on off the band).
        stand_still(s, 0.4);
        (void)s.request_set_down();
        (void)s.advance_frame(1.0);
        const std::uint32_t box_body = s.kit_body_index(box);
        const auto set_at = s.kit_body_position(box_body);
        require(s.snapshot().carrying_entity_id == 0, "matron mischief: set down on the band");
        // Woken by a sprint toward her locker; back west to wait, crouched.
        require(go(s, 8.0, Matron::kBandNorthZ, true, 4.0, 1.0) && s.matron_state().state == kStateStraining,
                "matron mischief: a sprint toward her locker wakes her");
        (void)s.set_crouch_input(true);
        require(go(s, -6.0, Matron::kBandNorthZ, false, 30.0, 0.3), "matron mischief: back west, crouched");
        (void)s.set_crouch_input(false);
        stand_still(s, 0.2);
        // She bursts out, comes west along the band, takes the toolbox in
        // passing and throws it at the player standing there.
        const auto before = s.snapshot();
        double nearest = 1.0e9;
        bool thrown = false;
        double took_at = -1.0;
        for (int i = 0; i < 60 * static_cast<int>(Simulation::kTickRateHz); ++i) {
            tick(s);
            const auto m = s.matron_state();
            if (took_at < 0.0 && m.carrying == box) {
                took_at = i / static_cast<double>(Simulation::kTickRateHz);
            }
            if (m.throws > 0) {
                thrown = true;
                const auto b = s.kit_body_position(box_body);
                const auto p = s.snapshot().player_position;
                nearest = std::min(nearest, std::hypot(std::hypot(b.x - p.x, b.z - p.z), b.y - p.y));
            }
            if (thrown && s.snapshot().hit_count > before.hit_count) {
                break;
            }
            if (thrown && i % 9 == 0 && s.kit_body_velocity(box_body).y == 0.0 && nearest < 1.2) {
                break;
            }
        }
        const auto m = s.matron_state();
        std::cout << "  matron mischief: set down at (" << set_at.x << ", " << set_at.z << "), taken " << took_at
                  << " s after the wait began, takes " << m.takes << " throws " << m.throws << ", it came within "
                  << nearest << " m of the player, felt hits " << s.snapshot().hit_count - before.hit_count
                  << ", deaths " << s.snapshot().death_count << ", stage " << int(m.stage) << "\n";
        require(m.takes >= 1 && took_at > 0.0, "matron mischief: a pest takes the toolbox the player handled");
        require(m.throws >= 1 && (nearest < 1.2 || s.snapshot().hit_count > before.hit_count),
                "matron mischief: and throws it at the player");
        require(s.snapshot().death_count == 0 && m.stage == 0, "matron mischief: a pest's throw is no death");
        // Then at the player: her hands shove them.
        bool shoved = false;
        for (int i = 0; i < 20 * static_cast<int>(Simulation::kTickRateHz) && !shoved; ++i) {
            tick(s);
            shoved = s.matron_state().shoves > 0;
        }
        require(shoved && s.snapshot().death_count == 0, "matron mischief: then her hands shove the player, no death");
        std::cout << "PASS scraperx_sim matron mischief: took_s=" << took_at << " throws=" << m.throws
                  << " nearest_m=" << nearest << "\n";
    }
    // ---- a pest's mischief, unseen: never getting the throw, she carries the
    //      player's toolbox off to her post and drops it there ----
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        require(s.advance_frame(1.0).accepted, "matron hoard: settle");
        require(walk_to(s, -6.5, -125.95, 6.0, 0.08), "matron hoard: to the toolbox");
        (void)s.set_facing(0.0, 1.0);
        (void)s.advance_frame(0.4);
        const std::uint64_t box = s.snapshot().carry_target_entity_id;
        (void)s.request_pick_up();
        (void)s.advance_frame(0.4);
        require(box != 0 && s.snapshot().carrying_entity_id == box, "matron hoard: picked up");
        require(go(s, -4.6, Matron::kBandNorthZ, false, 14.0, 0.2) && go(s, 0.4, Matron::kBandNorthZ, false, 14.0, 0.2) &&
                    go(s, 6.0, -126.4, false, 14.0, 0.2),
                "matron hoard: carried east toward her locker, across the bay on its leaf");
        (void)s.request_set_down();
        (void)s.advance_frame(1.0);
        const std::uint32_t box_body = s.kit_body_index(box);
        // A sprint past her locker wakes her; on east and round to the south
        // band, crouched, out of her sight.
        require(go(s, 14.0, Matron::kBandNorthZ, true, 4.0, 1.0) && s.matron_state().state == kStateStraining,
                "matron hoard: a sprint past her locker wakes her");
        (void)s.set_crouch_input(true);
        require(go(s, Matron::kBandX, Matron::kBandNorthZ, false, 30.0) && go(s, Matron::kBandX, Matron::kBandSouthZ, false, 40.0),
                "matron hoard: crouched away round to the south band");
        (void)s.set_move_input(0.0, 0.0);
        bool took = false;
        bool guarding_with_it_down = false;
        for (int i = 0; i < 120 * static_cast<int>(Simulation::kTickRateHz) && !guarding_with_it_down; ++i) {
            tick(s);
            const auto m = s.matron_state();
            took = took || m.carrying == box;
            guarding_with_it_down = took && m.state == kStateGuarding && m.carrying == 0 && m.speed < 0.3;
        }
        const auto b = s.kit_body_position(box_body);
        const double from_post = std::hypot(b.x - Matron::kPostX, b.z - Matron::kPostZ);
        std::cout << "  matron hoard: took " << took << ", the toolbox down " << from_post << " m from her post, throws "
                  << s.matron_state().throws << ", deaths " << s.snapshot().death_count << "\n";
        require(took && guarding_with_it_down && from_post < 2.5 && s.matron_state().throws == 0 &&
                    s.snapshot().death_count == 0,
                "matron hoard: unseen, she carries the player's toolbox off to her post and drops it there");
        std::cout << "PASS scraperx_sim matron hoard: from_post_m=" << from_post << "\n";
    }
}

void run_matron() {
    // ---- asleep: quiet or alone, she stays sealed ------------------------------
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        (void)s.advance_frame(1.0);
        (void)s.advance_frame(20.0);
        auto m = s.matron_state();
        require(m.state == kStateDormant && !m.released && m.strain == 0.0 && m.alive,
                "matron: left alone she stays sealed in her locker");
        (void)s.set_crouch_input(true);
        require(go(s, 10.0, -128.5, false, 30.0, 1.0), "matron: a crouched walk right past her locker");
        stand_still(s, 5.0);
        m = s.matron_state();
        require(m.state == kStateDormant && m.strain == 0.0,
                "matron: a crouched creep past her locker does not wake her");
        (void)s.set_crouch_input(false);
        require(go(s, 20.0, -128.5, false, 8.0, 1.0), "matron: a plain walk beside her locker");
        stand_still(s, 1.0);
        require(s.matron_state().state == kStateStraining, "matron: a plain walk beside her locker wakes her");
    }
    // ---- the build-up: a noise, the gauge, the burst ----------------------------
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        (void)s.advance_frame(1.0);
        require(go(s, 10.0, -128.5, true, 4.0, 1.0), "matron: a sprint toward her locker");
        stand_still(s, 0.1);
        require(s.matron_state().state == kStateStraining, "matron: a sprint beside her locker wakes her to strain");
        double last = s.matron_state().strain;
        double bolts = s.matron_state().bolts_popped;
        double released_at = -1.0;
        for (int i = 0; i < 25 * static_cast<int>(Simulation::kTickRateHz); ++i) {
            tick(s);
            const auto m = s.matron_state();
            require(m.strain + 1.0e-9 >= last, "matron: the gauge only climbs while she strains");
            require(m.bolts_popped + 1.0e-9 >= bolts, "matron: a bolt popped stays popped");
            last = m.strain;
            bolts = m.bolts_popped;
            if (released_at < 0.0 && m.released) {
                released_at = i / static_cast<double>(Simulation::kTickRateHz);
            }
        }
        std::cout << "  matron: released " << released_at << " s after the wake\n";
        require(released_at > 10.0 && released_at < 18.0, "matron: she bursts out about sixteen seconds after the wake");
        require(s.matron_state().state == kStateHunting || s.matron_state().state == kStateEmerging ||
                    s.matron_state().state == static_cast<int>(Matron::State::Searching) ||
                    s.matron_state().state == static_cast<int>(Matron::State::Shuffling),
                "matron: after the burst she is out and about");
    }
    // ---- the hunt: she runs faster than a walk, slower than a sprint; a pest's hands
    //      shove a still player off their feet, and only fully changed is a catch a death ----
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        wake_and_wait_east(s);
        require(wait_for_her(s, 14.0, 40.0), "matron: she comes for a player waiting in the east band");
        const auto met = s.snapshot();
        // Her pace is hers (matron.hpp, Pace): she may creep the last of it.
        // Still until her hands are on the player, then the shove's flight.
        (void)s.set_move_input(0.0, 0.0);
        (void)s.set_sprint_input(false);
        (void)wait_for(s, 8.0, [&](const scraperx::sim::Snapshot &) { return s.matron_state().shoves >= 1; });
        stand_still(s, 1.0);
        const auto shoved = s.snapshot();
        const auto pest = s.matron_state();
        std::cout << "  matron: a pest's hands: shoves=" << pest.shoves << " catches=" << pest.catches << " moved="
                  << std::hypot(shoved.player_position.x - met.player_position.x, shoved.player_position.z - met.player_position.z)
                  << " m, deaths " << met.death_count << " -> " << shoved.death_count << "\n";
        require(pest.stage == 0 && pest.shoves >= 1 && pest.catches == 0 && shoved.death_count == met.death_count,
                "matron: a pest who reaches a player standing still shoves them, and it is no death");
        require(std::hypot(shoved.player_position.x - met.player_position.x,
                           shoved.player_position.z - met.player_position.z) > 0.8,
                "matron: the shove throws the player off their feet and away");
        require(shoved.stumble_count > met.stumble_count && shoved.hit_count > met.hit_count,
                "matron: the shove is a stumble and a hit the body feels");
        s.debug_set_matron_stage(2);
        const auto start = s.snapshot();
        // Fully changed: her hand on them is no longer a death on touch. It
        // is her hold (run_matron_grab): her fingers close on an ankle.
        bool grabbed = false;
        for (int i = 0; i < 12 * static_cast<int>(Simulation::kTickRateHz) && !grabbed; ++i) {
            (void)s.set_move_input(0.0, 0.0);
            tick(s);
            grabbed = s.matron_state().grabs > 0;
            require(s.snapshot().death_count == start.death_count, "matron: her hand on a player is no death on touch");
        }
        std::cout << "  matron: fully changed, catches " << s.matron_state().catches << " grabs "
                  << s.matron_state().grabs << " misses " << s.matron_state().grab_misses << "\n";
        require(s.matron_state().catches > 0 && grabbed,
                "matron: fully changed, a player who just stands there is caught: her hand closes on them");
    }
    // ---- the checkpoint stops being renewed while she hunts ----------------------
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        wake_and_wait_east(s);
        // She is straining or out; the checkpoint was set where she burst out.
        require(wait_for_her(s, 14.0, 40.0), "matron: she comes");
        (void)s.set_crouch_input(false);
        // Run away round the loop: a long way, so a renewed checkpoint would be far from the release.
        require(go(s, Matron::kBandX, Matron::kBandSouthZ, true, 8.0, 1.0), "matron: a run down the east band");
        require(go(s, 0.0, Matron::kBandSouthZ, true, 8.0, 1.0), "matron: a run along the south band");
        const auto far = s.snapshot().player_position;
        const auto deaths_before = s.snapshot().death_count;
        // A death here (her hand is no longer one, run_matron_grab): a fall
        // into the shaft from where the player stands.
        s.debug_launch_player(scraperx::sim::Vector3{0.0, far.y + 0.5, -150.0}, scraperx::sim::Vector3{0.0, -5.0, 0.0});
        stand_still(s, 30.0);
        const auto back = s.snapshot();
        require(back.death_count > deaths_before, "matron: standing still on the loop gets the player caught");
        require(std::hypot(back.player_position.x - far.x, back.player_position.z - far.z) > 12.0,
                "matron: the death returns the player to the checkpoint of her release, not to where they stood");
    }
    // ---- the catwalk holds a climber ----------------------------------------------
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        (void)s.advance_frame(1.0);
        (void)s.set_crouch_input(true);
        require(go(s, Matron::kBandX, Matron::kBandNorthZ, false, 40.0) && go(s, Matron::kBandX, Matron::kMidZ, false, 40.0),
                "matron: a crouched walk to the catwalk's east end");
        const auto before = s.matron_state();
        require(before.state == kStateDormant, "matron: the catwalk's east end is out of earshot of her sleeping");
        (void)s.set_crouch_input(false);
        const auto crossed = sprint_across(s, false);
        require(crossed.crossed, "matron: a sprint across the whole catwalk");
        require(crossed.end.planks_fallen == 0 && crossed.deaths == 0,
                "matron: a climber's sprint across the rotten catwalk brings no plate down");
        require(crossed.end.plank_load_peak_n < 1700.0, "matron: the climber's load on a plate stays under its bolt");
        std::cout << "  matron: a climber's peak plate load " << crossed.end.plank_load_peak_n << " N (bolt 2100)\n";
    }
    // ---- the one kill: she follows across the catwalk and a plate gives way under her ----
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        wake_and_wait_east(s);
        require(wait_for_her(s, 14.0, 40.0), "matron: she comes for the player at the catwalk's east end");
        const auto crossed = sprint_across(s, true);
        std::cout << "  matron: after the crossing state=" << static_cast<int>(crossed.end.state)
                  << " plates fallen=" << crossed.end.planks_fallen << " deaths=" << crossed.deaths
                  << " peak=" << crossed.end.plank_load_peak_n << " N\n";
        require(crossed.end.state == kStateDead && !crossed.end.alive,
                "matron: following the climber over the rotten catwalk is the end of her");
        require(crossed.end.planks_fallen >= 1, "matron: a plate she stood on is down");
        require(crossed.deaths == 0, "matron: the climber crossed alive");
        // She does not come back: a death of the player later leaves her dead.
        stand_still(s, 5.0);
        require(s.matron_state().state == kStateDead, "matron: dead, she stays dead");
    }
    // ---- the Turn: she stops dead, then lunges in jerks, and not again for a while ----
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        wake_and_wait_east(s);
        // Her pace is her own (Pace): the Turn is met on her first approach
        // to the still player, her quiet spell from its cooldown after it.
        (void)s.set_crouch_input(false);
        (void)s.set_move_input(0.0, 0.0);
        const auto deaths_before = s.snapshot().death_count;
        double windup_still_s = 0.0;
        double lunge_peak = 0.0;
        double windup_at = -1.0;
        double lunge_at = -1.0;
        int turns_started = 0;
        std::uint8_t last_turn = 0;
        for (int i = 0; i < 75 * static_cast<int>(Simulation::kTickRateHz); ++i) {  // her pace is her own (Pace): longer than at one speed
            tick(s);
            const auto m = s.matron_state();
            const double now = i / static_cast<double>(Simulation::kTickRateHz);
            if (m.turn == static_cast<std::uint8_t>(Matron::Turn::Windup)) {
                if (last_turn == 0) {
                    ++turns_started;
                    windup_at = now;
                }
                if (m.speed < 1.0) {
                    windup_still_s += 1.0 / static_cast<double>(Simulation::kTickRateHz);
                }
            }
            if (m.turn == static_cast<std::uint8_t>(Matron::Turn::Lunge)) {
                if (last_turn == static_cast<std::uint8_t>(Matron::Turn::Windup)) {
                    lunge_at = now;
                }
                lunge_peak = std::max(lunge_peak, m.speed);
            }
            last_turn = m.turn;
            // Her pace is her own (Pace): a shove off the rim before her Turn
            // is a death and a fresh start, and she comes again; the run ends
            // with the first Turn done.
            if (lunge_at > 0.0 && m.turn == 0) {
                break;
            }
        }
        (void)deaths_before;
        std::cout << "  matron: the Turn: wind-up at " << windup_at << " s (still " << windup_still_s << " s), lunges from "
                  << lunge_at << " s, peak " << lunge_peak << " m/s, turns started " << turns_started << "\n";
        require(turns_started >= 1 && lunge_at > windup_at, "matron: with a clear line close up she snaps, wind-up then lunges");
        require(windup_still_s > 0.5, "matron: the wind-up stops her: the player's one warning");
        require(lunge_peak > 8.2, "matron: the lunges burst faster than a sprint");
        require(turns_started == 1, "matron: one Turn, then a long quiet spell");
    }
    // ---- the deck's loose stock: shoved by walking into it; the ingot dolly breaks a plate ----
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        (void)s.advance_frame(2.0);
        (void)s.set_crouch_input(true);
        require(go(s, Matron::kBandX, Matron::kBandNorthZ, false, 40.0) && go(s, 24.6, -142.8, false, 20.0),
                "matron: a crouched walk to the loose crate north of the catwalk's end");
        (void)s.set_crouch_input(false);
        stand_still(s, 1.0);
        const auto before = stock(s, LooseStock::kDeckCrateNorth);
        for (int i = 0; i < 4 * static_cast<int>(Simulation::kTickRateHz); ++i) {
            (void)s.set_move_input(0.0, -1.0);
            (void)s.set_facing(0.0, -1.0);
            tick(s);
        }
        (void)s.set_move_input(0.0, 0.0);
        const auto after = stock(s, LooseStock::kDeckCrateNorth);
        const double moved = std::hypot(after.x - before.x, after.z - before.z);
        std::cout << "  matron: a crate shoved 4 s by walking into it moved " << moved << " m\n";
        require(moved > 1.0 && moved < 4.0 * 5.5 * 0.6, "matron: a crate walked into moves, and slower than a walk");
        require(s.matron_state().planks_fallen == 0, "matron: nothing has come down");
    }
    // ---- the ingot dolly, from a fresh start: where a shoved crate ends up
    // (and whether it crashes into the next one and wakes her) is not this
    // check's business ----
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        (void)s.advance_frame(2.0);
        // Round behind it and shoved out onto the catwalk.
        (void)s.set_crouch_input(true);
        require(go(s, Matron::kBandX, Matron::kBandNorthZ, false, 40.0) && go(s, 24.6, -142.8, false, 20.0) &&
                    go(s, 21.5, -143.0, false, 20.0),
                "matron: to the dolly's side");
        (void)s.set_crouch_input(false);
        // First south into line with the catwalk, then west along it.
        const bool lined = push_toward(s, LooseStock::kDeckIngotDolly, 19.0, Matron::kMidZ - 0.6, 40.0, [](const scraperx::sim::Vector3 &d) {
            return std::abs(d.z - Matron::kMidZ) < 0.2;
        });
        require(lined, "matron: the ingot dolly pushed into line with the catwalk");
        const bool out = push_toward(s, LooseStock::kDeckIngotDolly, 10.0, Matron::kMidZ, 40.0,
                                     [](const scraperx::sim::Vector3 &d) { return d.x < 17.6; });
        require(out, "matron: the ingot dolly pushed to the catwalk's end");
        // A last hard shove sends it rolling out onto the plates; the
        // pusher stops on the deck.
        for (int i = 0; i < static_cast<int>(0.6 * static_cast<double>(Simulation::kTickRateHz)); ++i) {
            const auto d = stock(s, LooseStock::kDeckIngotDolly);
            const auto p = s.snapshot().player_position;
            (void)s.set_move_input(-1.0, (d.z - p.z) * 2.0);
            (void)s.set_facing(-1.0, 0.0);
            (void)s.set_sprint_input(true);
            tick(s);
            if (p.x < 18.2) {
                break;
            }
        }
        (void)s.set_sprint_input(false);
        // Step back off the catwalk's end and let it do what it does.
        require(go(s, 21.5, Matron::kMidZ, false, 6.0), "matron: back off the catwalk's end");
        stand_still(s, 3.0);
        const auto dolly = stock(s, LooseStock::kDeckIngotDolly);
        const auto m = s.matron_state();
        std::cout << "  matron: the dolly on the catwalk: plates down " << m.planks_fallen << ", the dolly at y " << dolly.y
                  << "\n";
        require(m.planks_fallen >= 1 && dolly.y < Matron::kDeckTop - 5.0,
                "matron: the ingot dolly brings a plate down and falls through");
        require(m.alive && m.state != kStateDead && s.snapshot().death_count == 0,
                "matron: a plate down under the dolly kills no one");
    }
    // ---- her mind (the owner, 2026-10-18: B and C): she guards the way out
    // when she loses the player; a noise that led to nobody makes her wary,
    // the next checked from a vantage; fooled twice, a far noise does not draw
    // her off her post, a close one does; a death forgets it all ------------------
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        const auto post_distance = [&s] {
            const auto m = s.matron_state();
            return std::hypot(m.position.x - Matron::kPostX, m.position.z - Matron::kPostZ);
        };
        const auto run_until = [&s](const double seconds, const auto &done) {
            const auto ticks = static_cast<int>(seconds * static_cast<double>(Simulation::kTickRateHz));
            for (int i = 0; i < ticks; ++i) {
                tick(s);
                if (done()) {
                    return true;
                }
            }
            return false;
        };
        require(s.advance_frame(1.0).accepted, "matron mind: settle");
        // Wake her by her locker, then creep off round the west band to its far corner.
        require(go(s, 10.0, -128.5, true, 4.0, 1.0), "matron mind: a sprint toward her locker");
        (void)s.set_crouch_input(true);
        require(go(s, -Matron::kBandX, Matron::kBandNorthZ, false, 40.0) &&
                    go(s, -Matron::kBandX, Matron::kBandSouthZ, false, 40.0),
                "matron mind: a crouched walk round to the south-west corner");
        stand_still(s, 0.2);
        // She runs to where she last heard the player, finds nobody, searches, and
        // goes to stand in the mouth of Stage A's cage.
        const bool guarding = run_until(60.0, [&] {
            return s.matron_state().state == kStateGuarding && post_distance() < 0.6 && s.matron_state().speed < 0.2;
        });
        const auto at_post = s.matron_state();
        std::cout << "  matron mind: lost the player, at her post " << post_distance() << " m off it, fooled "
                  << at_post.fooled << ", state " << int(at_post.state) << "\n";
        require(guarding && at_post.fooled == 1 && s.snapshot().death_count == 0,
                "matron mind: losing the player, she goes to stand in the way out's mouth");
        stand_still(s, 3.0);
        require(s.matron_state().state == kStateGuarding && post_distance() < 0.6,
                "matron mind: she waits there, still");

        // A second noise: a sprint up the west band, then a creep back to the corner.
        (void)s.set_crouch_input(false);
        require(go(s, -Matron::kBandX, -152.0, true, 6.0, 0.6), "matron mind: a sprint up the west band");
        const double noise_x = s.snapshot().player_position.x;
        const double noise_z = s.snapshot().player_position.z;
        (void)s.set_crouch_input(true);
        double closest = 1.0e9;
        bool peered = false;
        const auto watch = [&] {
            const auto m = s.matron_state();
            closest = std::min(closest, std::hypot(m.position.x - noise_x, m.position.z - noise_z));
            peered = peered || m.state == kStatePeering;
        };
        for (int i = 0; i < 20 * static_cast<int>(Simulation::kTickRateHz); ++i) {
            if (steer(s, -Matron::kBandX, Matron::kBandSouthZ, false, 0.4)) {
                break;
            }
            tick(s);
            watch();
        }
        stand_still(s, 0.2);
        const bool back = run_until(60.0, [&] {
            watch();
            return s.matron_state().state == kStateGuarding && post_distance() < 0.6;
        });
        std::cout << "  matron mind: wary of the second noise: peered " << peered << ", came within " << closest
                  << " m of it, fooled " << s.matron_state().fooled << "\n";
        require(peered && closest > 4.0 && back && s.matron_state().fooled == 2 && s.snapshot().death_count == 0,
                "matron mind: the second noise she checks from a vantage, then goes back to her post");

        // Fooled twice: a sprint far off does not draw her off her post.
        (void)s.set_crouch_input(false);
        double farthest = 0.0;
        for (int i = 0; i < 8 * static_cast<int>(Simulation::kTickRateHz); ++i) {
            if (steer(s, -Matron::kBandX, -158.0, true, 0.6)) {
                (void)s.set_sprint_input(false);
            }
            tick(s);
            farthest = std::max(farthest, post_distance());
        }
        std::cout << "  matron mind: a far sprint: she strayed " << farthest << " m from her post, state "
                  << int(s.matron_state().state) << "\n";
        require(farthest < 1.0 && s.matron_state().state == kStateGuarding,
                "matron mind: fooled twice, a far noise does not draw her off her post");

        // A close one does: up the band toward her, loud, and she comes.
        bool hunted = false;
        for (int i = 0; i < 20 * static_cast<int>(Simulation::kTickRateHz) && !hunted; ++i) {
            (void)steer(s, -Matron::kBandX, Matron::kBandNorthZ, true, 0.6);
            tick(s);
            hunted = s.matron_state().state == kStateHunting;
        }
        require(hunted, "matron mind: a noise close to her post draws her");
        const auto deaths = s.snapshot().death_count;
        s.debug_set_matron_stage(2);   // a catch is a death only fully changed
        stand_still(s, 0.0);
        // Caught: her hand closes on them (her hold, run_matron_grab); then a
        // death (a fall into the shaft) for what a death forgets.
        const bool caught = run_until(20.0, [&] { return s.matron_state().grabs > 0; });
        require(caught, "matron mind: standing still, the player is caught");
        s.debug_launch_player(scraperx::sim::Vector3{0.0, 154.5, -150.0}, scraperx::sim::Vector3{0.0, -5.0, 0.0});
        require(run_until(30.0, [&] { return s.snapshot().death_count > deaths; }), "matron mind: the fall is a death");
        tick(s);
        std::cout << "  matron mind: after the catch, fooled " << s.matron_state().fooled << "\n";
        require(s.matron_state().fooled == 0, "matron mind: a death forgets what she learned");
        std::cout << "PASS scraperx_sim matron mind: guarded at " << at_post.position.x << "," << at_post.position.z
                  << " peered_short_m=" << closest << " fooled_twice_stayed_m=" << farthest << "\n";
    }
    // ---- a hit on her deck draws her (the owner, 2026-10-18: sound is how she
    //      hunts). She searches where she lost the player; out of her earshot
    //      and her sight, the player shoves crate S into crate N; she hears the
    //      hit and goes to it, not wary, finds nobody, and is the warier ----
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        require(s.advance_frame(1.0).accepted, "matron crash: settle");
        require(go(s, 10.0, -128.5, true, 4.0, 1.0), "matron crash: a sprint toward her locker");
        require(s.matron_state().state == kStateStraining, "matron crash: the sprint woke her");
        // Straining, and then emerging, she hears nothing: to the north-east
        // corner to be there as she bursts out, then off down the east band.
        require(go(s, Matron::kBandX, Matron::kBandNorthZ, false, 10.0, 0.3), "matron crash: to the north-east corner");
        bool out = false;
        for (int i = 0; i < 30 * static_cast<int>(Simulation::kTickRateHz) && !out; ++i) {
            tick(s);
            out = s.matron_state().state == kStateEmerging;
        }
        require(out, "matron crash: she bursts out");
        const auto burst = s.snapshot().player_position;
        require(go(s, Matron::kBandX, -143.0, true, 3.0, 0.6) && s.matron_state().state == kStateEmerging,
                "matron crash: a sprint down the east band while she is still emerging");
        (void)s.set_crouch_input(true);
        require(go(s, Matron::kBandX, -157.6, false, 10.0, 0.3) && go(s, 24.5, -156.2, false, 6.0, 0.15),
                "matron crash: a creep round behind crate S");
        bool searching = false;
        bool heard_early = false;
        for (int i = 0; i < 30 * static_cast<int>(Simulation::kTickRateHz) && !searching; ++i) {
            tick(s);
            heard_early = heard_early || s.matron_state().noise_heard > 0.0 || s.matron_state().sees_player;
            searching = s.matron_state().state == kStateSearching;
        }
        stand_still(s, 2.0);
        const auto lost = s.matron_state();
        std::cout << "  matron crash: burst with the player at (" << burst.x << ", " << burst.z << "); she searches at ("
                  << lost.position.x << ", " << lost.position.z << "), fooled " << lost.fooled << "\n";
        require(searching && !heard_early && lost.state == kStateSearching && lost.fooled == 0 &&
                    std::hypot(lost.position.x - burst.x, lost.position.z - burst.z) < 1.5,
                "matron crash: she ran to where the player was at her burst, and searches there");
        // Up, and crate S shoved north into crate N.
        (void)s.set_crouch_input(false);
        std::uint64_t seen = s.last_impact_sequence();
        scraperx::sim::ImpactSnapshot last_tick_loudest{};
        scraperx::sim::ImpactSnapshot crash{};
        bool heard = false;
        double player_from_her = 0.0;
        for (int i = 0; i < 8 * static_cast<int>(Simulation::kTickRateHz) && !heard; ++i) {
            (void)s.set_move_input(0.0, 1.0);
            (void)s.set_facing(0.0, 1.0);
            tick(s);
            const auto m = s.matron_state();
            const auto p = s.snapshot();
            if (m.noise_heard > 0.0) {
                heard = true;
                crash = last_tick_loudest;
                player_from_her = std::hypot(p.player_position.x - m.position.x, p.player_position.z - m.position.z);
                break;
            }
            // This tick's hits: she hears them next tick.
            last_tick_loudest = {};
            for (const auto &hit : s.impacts()) {
                if (hit.sequence > seen) {
                    if (hit.energy_j > last_tick_loudest.energy_j) {
                        last_tick_loudest = hit;
                    }
                    seen = hit.sequence;
                }
            }
        }
        (void)s.set_move_input(0.0, 0.0);
        const auto drawn = s.matron_state();
        const double loud = ImpactLedger::loudness(static_cast<float>(crash.energy_j));
        const double crash_d = std::hypot(crash.at.x - drawn.position.x, crash.at.z - drawn.position.z);
        std::cout << "  matron crash: heard the hit on crate N (" << crash.energy_j << " J, loudness " << loud << ") "
                  << crash_d << " m off; the player " << player_from_her << " m off\n";
        require(heard && crash.entity == LooseStock::kFirstEntity + static_cast<std::uint64_t>(LooseStock::kDeckCrateNorth),
                "matron crash: she hears crate S strike crate N");
        require(drawn.noise_heard == loud, "matron crash: what she heard is the hit, at its loudness");
        require(player_from_her > 0.30 * 55.0 && !drawn.sees_player,
                "matron crash: the player is out of her earshot and her sight");
        require(drawn.state == kStateHunting && !drawn.wary &&
                    std::hypot(drawn.target_x - crash.at.x, drawn.target_z - crash.at.z) < 0.01,
                "matron crash: the hit draws her: she goes for it, not wary");
        // Away south, crouched; she goes to the hit, searches it, finds nobody.
        (void)s.set_crouch_input(true);
        double closest = 1.0e9;
        bool stood_down = false;
        for (int i = 0; i < 30 * static_cast<int>(Simulation::kTickRateHz) && !stood_down; ++i) {
            (void)steer(s, 24.5, Matron::kBandSouthZ, false);
            tick(s);
            const auto m = s.matron_state();
            closest = std::min(closest, std::hypot(m.position.x - crash.at.x, m.position.z - crash.at.z));
            stood_down = m.state == kStateGuarding;
        }
        std::cout << "  matron crash: she came within " << closest << " m of the hit; fooled " << s.matron_state().fooled
                  << ", deaths " << s.snapshot().death_count << "\n";
        require(closest < 1.5 && stood_down && s.matron_state().fooled == 1 && s.snapshot().death_count == 0,
                "matron crash: she went to the hit, found nobody, and is the warier for it");
        std::cout << "PASS scraperx_sim matron crash: heard_m=" << crash_d << " loudness=" << loud << " closest_m=" << closest
                  << "\n";
    }
    // ---- A: in the Turn she vaults what blocks her way; frail, she shoves it -------
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        require(s.advance_frame(1.0).accepted, "matron vault: settle");
        // Quietly round to the east band's south crate and shove it into the band's middle.
        (void)s.set_crouch_input(true);
        require(go(s, Matron::kBandX, Matron::kBandNorthZ, false, 40.0) && go(s, Matron::kBandX, -150.5, false, 40.0),
                "matron vault: a crouched walk to the east band");
        (void)s.set_crouch_input(false);
        // Shoved off the rail into the band's middle, then 5 m down it.
        require(push_toward(s, LooseStock::kDeckCrateSouth, Matron::kBandX, -155.0, 20.0,
                            [](const scraperx::sim::Vector3 &c) { return c.x < Matron::kBandX + 0.3; }) &&
                    push_toward(s, LooseStock::kDeckCrateSouth, Matron::kBandX, -163.0, 20.0,
                                [](const scraperx::sim::Vector3 &c) { return c.z < -160.0; }),
                "matron vault: the crate shoved into the middle of the east band");
        // Wake her with a sprint up the band, then walk off west along the north
        // band and wait there for her.
        require(go(s, Matron::kBandX, -133.0, true, 8.0, 0.8), "matron vault: a sprint up the band, near her locker");
        require(go(s, Matron::kBandX, Matron::kBandNorthZ, false, 10.0, 1.0) &&
                    go(s, -10.0, Matron::kBandNorthZ, false, 15.0),
                "matron vault: a walk off west along the north band");
        // This leg meets the Turn's vault at her one old speed (her paces are
        // met by run_matron_pace): Hunt, no hush.
        (void)s.set_crouch_input(true);
        require(wait_for_her(s, 18.0, 40.0), "matron vault: she comes along the north band");
        (void)s.set_crouch_input(false);
        // Led the long way round, past the crate, she comes up the east band
        // after the player with her first quiet spell long over.
        require(go(s, -Matron::kBandX, Matron::kBandNorthZ, true, 8.0, 1.0) &&
                    go(s, -Matron::kBandX, Matron::kBandSouthZ, true, 8.0, 1.0) &&
                    go(s, Matron::kBandX, Matron::kBandSouthZ, true, 8.0, 1.0) &&
                    go(s, 24.4, -167.0, true, 4.0, 0.8) && go(s, 24.4, -157.5, true, 4.0),
                "matron vault: a run round the west and south bands and up the east band past the crate");
        (void)s.set_move_input(0.0, 0.0);
        bool winding = false;
        for (int i = 0; i < 45 * static_cast<int>(Simulation::kTickRateHz) && !winding; ++i) {
            tick(s);
            winding = s.matron_state().turn == static_cast<std::uint8_t>(Matron::Turn::Windup);
        }
        require(winding, "matron vault: in plain sight up the band she snaps");
        // In her wind-up the player steps in behind the crate, on her line.
        const auto m = s.matron_state().position;
        const auto c = stock(s, LooseStock::kDeckCrateSouth);
        const double run = std::hypot(c.x - m.x, c.z - m.z);
        const double ux = (c.x - m.x) / run;
        const double uz = (c.z - m.z) / run;
        (void)go(s, c.x + ux * 1.6, c.z + uz * 1.6, false, 3.0, 0.3);
        (void)s.set_move_input(0.0, 0.0);
        bool vaulted = false;
        for (int i = 0; i < 8 * static_cast<int>(Simulation::kTickRateHz) && !vaulted; ++i) {
            tick(s);
            vaulted = s.matron_state().vaults > 0;
        }
        require(vaulted, "matron vault: in the Turn she vaults the crate in her way");
        // Over it, not through it: she comes down beyond its far face, the
        // crate where it stood (until the catch puts her back in her locker).
        const auto before = stock(s, LooseStock::kDeckCrateSouth);
        const auto deaths = s.snapshot().death_count;
        double beyond = -1.0e9;
        double peak_y = 0.0;
        for (int i = 0; i < 2 * static_cast<int>(Simulation::kTickRateHz); ++i) {
            tick(s);
            if (s.snapshot().death_count != deaths) {
                break;
            }
            const auto at = s.matron_state().position;
            beyond = std::max(beyond, (at.x - before.x) * ux + (at.z - before.z) * uz);
            peak_y = std::max(peak_y, at.y);
        }
        const auto after = stock(s, LooseStock::kDeckCrateSouth);
        const double shoved = std::hypot(after.x - before.x, after.z - before.z);
        std::cout << "  matron vault: over the crate at (" << before.x << ", " << before.z << "): her feet "
                  << peak_y - Matron::kStandCentre - Matron::kDeckTop << " m up, " << beyond
                  << " m past its middle; the crate moved " << shoved << " m\n";
        require(peak_y - Matron::kStandCentre - Matron::kDeckTop > 0.88 && beyond > 0.8,
                "matron vault: she clears the crate and comes down beyond it");
        require(shoved < 0.3, "matron vault: vaulted, the crate is not shoved");
        require(s.matron_state().alive, "matron vault: she lands on the deck, alive");
        std::cout << "PASS scraperx_sim matron vault: feet_up_m=" << peak_y - Matron::kStandCentre - Matron::kDeckTop
                  << " past_m=" << beyond << " crate_moved_m=" << shoved << "\n";
    }
    // ---- off her deck: into the cage as it leaves she rides it up, clinging; at the top,
    //      frail, she cannot cross the gap to the ring; in the Turn she leaps it ----------
    {
        // The ride alone, for her weight's part in it.
        double alone_s = 0.0;
        {
            Simulation s(InitialSpawn::Deck154);
            s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
            require(s.advance_frame(1.0).accepted && board_well_a(s) && rig_well_a(s) && pull_well_a(s, 0.5, 3.0),
                    "matron ride: Stage A ridden alone");
            for (int i = 0; i < 20 * static_cast<int>(Simulation::kTickRateHz) && s.snapshot().well_a_cage_travel < 21.99;
                 ++i) {
                tick(s);
                alone_s += Simulation::kFixedStepSeconds;
            }
            require(s.matron_state().state == kStateDormant, "matron ride: the ride alone did not wake her");
        }
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        require(s.advance_frame(1.0).accepted, "matron ride: settle");
        require(board_well_a(s) && rig_well_a(s), "matron ride: Stage A's cage rigged");
        require(s.matron_state().state == kStateDormant, "matron ride: rigging the cage did not wake her");
        require(go(s, -10.0, -129.2, false, 6.0) && go(s, 0.0, -128.6, true, 4.0, 0.8),
                "matron ride: out of the cage, a sprint toward her locker");
        require(s.matron_state().state == kStateStraining, "matron ride: the sprint woke her");
        // Round the gantry's east post, back in to the handle, and take it.
        require(go(s, -10.0, -128.4, false, 6.0) && board_well_a(s) && walk_to(s, -11.05, -130.35, 3.0, 0.08),
                "matron ride: back in the cage at the trip handle");
        (void)s.set_facing(0.0, 1.0);
        (void)s.advance_frame(0.5);
        (void)s.request_pick_up();
        (void)s.advance_frame(0.3);
        require(s.snapshot().carrying_entity_id == Simulation::kWellAHandleEntityId, "matron ride: the handle in hand");
        // She comes for the cage; with her 3.8 m from its mouth, step back.
        // (4.5 m before the bay's leaf put her line's last bend at its west
        // end, bay.hpp: she now reaches the mouth a moment later, and clings
        // at its west side.)
        bool near = false;
        for (int i = 0; i < 40 * static_cast<int>(Simulation::kTickRateHz) && !near; ++i) {
            (void)s.set_facing(0.0, 1.0);
            tick(s);
            const auto m = s.matron_state();
            near = m.state == kStateHunting && std::hypot(m.position.x + 10.5, m.position.z + 129.9) < 3.8;
        }
        require(near, "matron ride: she comes for the cage");
        bool opened = false;
        for (int i = 0; i < 3 * static_cast<int>(Simulation::kTickRateHz) && !opened; ++i) {
            (void)s.set_move_input(0.0, -0.35);
            (void)s.set_facing(0.0, 1.0);
            tick(s);
            opened = !s.snapshot().well_a_catch_latched;
        }
        (void)s.set_move_input(0.0, 0.0);
        if (s.snapshot().carrying_entity_id != 0) {
            (void)s.request_set_down();
        }
        require(opened, "matron ride: the catch let go");
        const auto deaths = s.snapshot().death_count;
        // The ride: she steps aboard as it leaves and holds on all the way.
        double with_her_s = 0.0;
        bool clung_all_way = true;
        bool aboard = false;
        for (int i = 0; i < 25 * static_cast<int>(Simulation::kTickRateHz) && s.snapshot().well_a_cage_travel < 21.99;
             ++i) {
            tick(s);
            with_her_s += Simulation::kFixedStepSeconds;
            const auto m = s.matron_state();
            const double travel = s.snapshot().well_a_cage_travel;
            aboard = aboard || m.position.y > 155.05 + 0.5 * travel + 1.0;
            if (travel > 1.0) {
                clung_all_way = clung_all_way && m.clinging && m.position.y > 155.05 + travel - 0.3;
            }
        }
        std::cout << "  matron ride: aboard " << aboard << ", clung all the way " << clung_all_way << ", the ride "
                  << with_her_s << " s with her, " << alone_s << " s alone; deaths " << s.snapshot().death_count - deaths
                  << "\n";
        require(aboard && clung_all_way, "matron ride: she stepped into the cage as it left and held on to the top");
        require(s.snapshot().death_count == deaths, "matron ride: clinging, she took nobody on the way up");
        require(with_her_s > alone_s + 0.5, "matron ride: her weight slows the cage away");
        // At the top: out of the cage's mouth and across the 1.09 m to the ring,
        // then off east along it, out of her reach.
        // The mouth is 3 m wide: the player hops from the side of it she is
        // not clinging at.
        const double hop_x = s.matron_state().position.x < -10.5 ? -9.9 : -11.0;
        require(walk_to(s, hop_x, -130.3, 2.0, 0.1), "matron ride: to the cage's mouth at the top");
        (void)s.set_facing(0.0, 1.0);
        (void)s.set_move_input(0.0, 0.6);
        tick(s);
        (void)s.request_jump();
        bool across = false;
        for (int i = 0; i < 2 * static_cast<int>(Simulation::kTickRateHz) && !across; ++i) {
            tick(s);
            const auto p = s.snapshot();
            if (p.player_position.z > -128.4) {
                (void)s.set_move_input(0.0, 0.0);
            }
            across = p.player_grounded && p.player_position.z > -128.8 && p.player_position.y > 176.5;
        }
        require(across, "matron ride: the player hops the gap onto the 176 ring");
        require(go(s, -11.0, Matron::kRingNorthZ, false, 3.0) && go(s, 6.0, Matron::kRingNorthZ, true, 6.0),
                "matron ride: off east along the ring's north band");
        (void)s.set_move_input(0.0, 0.0);
        // Frail, she stays on the cages: she comes to their edge and no farther.
        double island_s = 0.0;
        bool crossed_without_leap = false;
        for (int i = 0; i < 6 * static_cast<int>(Simulation::kTickRateHz); ++i) {
            tick(s);
            const auto m = s.matron_state();
            if (m.position.z > -129.0 && m.leaps == 0) {
                crossed_without_leap = true;
            }
            if (m.alive && m.position.y > 176.5 && m.position.z < -129.9) {
                island_s += Simulation::kFixedStepSeconds;
            }
        }
        const auto held = s.matron_state();
        std::cout << "  matron ride: on the cages " << island_s << " s of 6 with the player on the ring, at ("
                  << held.position.x << ", " << held.position.y << ", " << held.position.z << "), level " << held.floor_y
                  << ", leaps " << held.leaps << "\n";
        // Her pace is her own (Pace): her first Turn may fall here, and then
        // she has leapt it already. Never across it on her feet.
        require(held.alive && !crossed_without_leap &&
                    ((island_s > 5.9 && held.floor_y > 176.0) || held.leaps > 0),
                "matron ride: frail, she cannot cross the gap to the ring");
        // Come within her reach and the Turn takes her over it.
        bool leapt = false;
        for (int i = 0; i < 25 * static_cast<int>(Simulation::kTickRateHz) && !leapt; ++i) {
            (void)steer(s, -2.0, Matron::kRingNorthZ, false);
            tick(s);
            leapt = s.matron_state().leaps > 0;
        }
        require(leapt, "matron ride: in the Turn she leaps the gap");
        // Down on the ring's band, standing.
        bool landed = false;
        for (int i = 0; i < 3 * static_cast<int>(Simulation::kTickRateHz) && !landed; ++i) {
            tick(s);
            const auto m = s.matron_state();
            landed = m.position.z > -128.9 && std::abs(m.position.y - (Matron::kRingFloor + Matron::kStandCentre)) < 0.06;
        }
        const auto over = s.matron_state();
        std::cout << "  matron ride: leapt to (" << over.position.x << ", " << over.position.y << ", " << over.position.z
                  << "), alive " << over.alive << "\n";
        require(landed && over.alive, "matron ride: she lands on the ring");
        std::cout << "PASS scraperx_sim matron ride: with_her_s=" << with_her_s << " alone_s=" << alone_s
                  << " island_s=" << island_s << " leaps=" << over.leaps << "\n";
    }
    // ---- B: a way out rigged and left, she takes the rope off its eye; hooked on again,
    //      it rides ---------------------------------------------------------------------
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        require(s.advance_frame(1.0).accepted, "matron unrig: settle");
        require(board_well_a(s) && rig_well_a(s), "matron unrig: Stage A's cage rigged");
        require(s.matron_state().state == kStateDormant, "matron unrig: rigging the cage did not wake her");
        require(go(s, -10.0, -129.2, false, 6.0) && go(s, 0.0, -128.6, true, 4.0, 0.8),
                "matron unrig: out of the cage, a sprint toward her locker");
        require(s.matron_state().state == kStateStraining, "matron unrig: the sprint woke her");
        // Away, crouched, round to the south band, where she will not find the player.
        (void)s.set_crouch_input(true);
        require(go(s, Matron::kBandX, Matron::kBandNorthZ, false, 40.0) && go(s, Matron::kBandX, Matron::kBandSouthZ, false, 40.0) &&
                    go(s, 0.0, Matron::kBandSouthZ, false, 40.0),
                "matron unrig: a crouched walk round to the south band");
        (void)s.set_move_input(0.0, 0.0);
        bool unrigged = false;
        for (int i = 0; i < 90 * static_cast<int>(Simulation::kTickRateHz) && !unrigged; ++i) {
            tick(s);
            unrigged = s.matron_state().unrigs > 0;
        }
        const auto after_unrig = s.snapshot();
        std::cout << "  matron unrig: unrigs " << s.matron_state().unrigs << ", the rope's end on "
                  << after_unrig.well_a_rope_end_entity_id << ", she at (" << s.matron_state().position.x << ", "
                  << s.matron_state().position.z << ") state " << int(s.matron_state().state) << ", deaths "
                  << after_unrig.death_count << "\n";
        require(unrigged && after_unrig.well_a_rope_end_entity_id == Simulation::kWellAShackleEntityId &&
                    after_unrig.death_count == 0,
                "matron unrig: guarding the cage, she took the player's rope off its eye");
        stand_still(s, 3.0);
        require(s.matron_state().state == kStateGuarding && s.matron_state().unrigs == 1,
                "matron unrig: then back to her post, once");
        // Draw her: a sprint along the south band and up the west band to the
        // catwalk's west end; then over the catwalk, and she follows to her death.
        (void)s.set_crouch_input(false);
        require(go(s, -Matron::kBandX, Matron::kBandSouthZ, true, 8.0, 1.0) && go(s, -Matron::kBandX, Matron::kMidZ, true, 8.0, 0.6) &&
                    walk_to(s, -Matron::kBandX + 1.5, Matron::kMidZ, 4.0, 0.1),
                "matron unrig: a sprint to the catwalk's west end");
        (void)s.set_move_input(0.0, 0.0);
        // Wary of the noise she comes to look; in sight of the player, she hunts.
        bool hunting = false;
        for (int i = 0; i < 60 * static_cast<int>(Simulation::kTickRateHz) && !hunting; ++i) {
            tick(s);
            const auto m = s.matron_state();
            hunting = m.state == kStateHunting && !m.wary && matron_player_distance(s) < 12.0;
        }
        require(hunting, "matron unrig: she comes for the noise, sees the player and hunts");
        bool dead = false;
        for (int i = 0; i < 20 * static_cast<int>(Simulation::kTickRateHz) && !dead; ++i) {
            (void)steer(s, Matron::kBandX, Matron::kMidZ, true, 0.6);
            tick(s);
            dead = s.matron_state().state == kStateDead;
        }
        require(dead && s.snapshot().death_count == 0, "matron unrig: over the catwalk after the player, she falls");
        // Back to the cage: the shackle hangs at its eye; take it and hook it on.
        require(go(s, Matron::kBandX, Matron::kBandNorthZ, false, 10.0, 1.0) && go(s, -10.0, -128.4, false, 10.0) &&
                    board_well_a(s) && walk_to(s, -11.35, -131.40, 3.0, 0.08),
                "matron unrig: back in the cage at its eye");
        (void)s.set_facing(-1.0, 0.0);
        (void)s.advance_frame(0.5);
        std::cout << "  matron unrig: at the eye: carry target " << s.snapshot().carry_target_entity_id << " kind "
                  << int(s.snapshot().carry_target_kind) << ", rig " << int(s.snapshot().rig_action) << "\n";
        require(s.snapshot().carry_target_entity_id == Simulation::kWellAShackleEntityId && s.snapshot().carry_target_kind == 1,
                "matron unrig: the shackle she took off hangs within reach at the eye");
        (void)s.request_pick_up();
        (void)s.advance_frame(0.3);
        (void)s.set_facing(-1.0, 0.0);
        (void)s.advance_frame(0.5);
        require(s.snapshot().carrying_entity_id == Simulation::kWellAShackleEntityId && s.snapshot().rig_action == 1 &&
                    s.snapshot().rig_target_entity_id == Simulation::kWellACageEntityId,
                "matron unrig: in hand, HOOK onto the cage's eye is offered");
        (void)s.request_rig();
        (void)s.advance_frame(0.3);
        require(s.snapshot().well_a_rope_end_entity_id == Simulation::kWellACageEntityId, "matron unrig: hooked on again");
        require(pull_well_a(s, 0.5, 3.0), "matron unrig: the handle trips the catch");
        bool rode = false;
        for (int i = 0; i < 20 * static_cast<int>(Simulation::kTickRateHz) && !rode; ++i) {
            tick(s);
            rode = s.snapshot().well_a_cage_travel >= 21.99;
        }
        require(rode && s.snapshot().player_position.y > 177.0, "matron unrig: undone, the cage rides the player up");
        std::cout << "PASS scraperx_sim matron unrig: unrigs=1 rehooked=1 rode=1\n";
    }
    // ---- lines: seen riding a line away, in the Turn she goes down its cable after ----
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        // The sprint that wakes her, then crouched west over the bay's leaf
        // and down the west band, to wait for her there: the run round below
        // then comes to the line's ramp from the east, with her behind on
        // this side of the bay. (Run east over it, the player goes unheard
        // under its fan, bay.hpp; or, heard or seen, she does not cross after
        // them: her trap, matron.hpp.)
        require(s.advance_frame(1.0).accepted, "matron line: settle");
        require(go(s, 10.0, -128.5, true, 4.0, 1.0), "matron line: a sprint toward her locker");
        (void)s.set_crouch_input(true);
        require(go(s, -Matron::kBandX, Matron::kBandNorthZ, false, 60.0) &&
                    go(s, -Matron::kBandX, -138.0, false, 40.0),
                "matron line: a crouched walk down the west band");
        (void)s.set_move_input(0.0, 0.0);
        require(wait_for_her(s, 14.0, 40.0), "matron line: she comes");
        (void)s.set_crouch_input(false);
        // A long run round ahead of her (her first quiet spell over), by the
        // south and east bands back along the north band to the 154 m line's
        // ramp.
        require(go(s, -Matron::kBandX, Matron::kBandSouthZ, true, 8.0, 1.0) &&
                    go(s, Matron::kBandX, Matron::kBandSouthZ, true, 10.0, 1.0) &&
                    go(s, Matron::kBandX, Matron::kBandNorthZ, true, 10.0, 1.0) && go(s, 1.6, Matron::kBandNorthZ, true, 6.0, 0.8) &&
                    walk_to(s, 3.0, -132.36, 4.0, 0.3),
                "matron line: round the deck to the line's ramp");
        (void)s.set_move_input(0.0, 0.0);
        // At the ramp's foot, wait for her charge (her Turn); then up onto the
        // stand and the jump for the bar, and away down the line, with her
        // coming up the ramp behind.
        bool charging = false;
        for (int i = 0; i < 30 * static_cast<int>(Simulation::kTickRateHz) && !charging; ++i) {
            tick(s);
            const auto m = s.matron_state();
            charging = m.state == kStateHunting && m.turn != 0 && matron_player_distance(s) < 14.0;
        }
        require(charging && s.snapshot().death_count == 0, "matron line: she comes for the player at the line's ramp");
        require(walk_to(s, 6.0, -132.36, 3.0, 0.12), "matron line: up the ramp onto the stand");
        (void)s.set_move_input(0.0, 0.0);
        (void)s.set_facing(0.0, -1.0);
        // Stood still under the bar (a step's run-on carries a jump off the
        // stand's end).
        for (int i = 0; i < static_cast<int>(Simulation::kTickRateHz); ++i) {
            const double was_x = s.snapshot().player_position.x;
            tick(s);
            if (std::abs(s.snapshot().player_position.x - was_x) < 0.001) {
                break;
            }
        }
        require(s.request_jump() && advance_until(s, [](const scraperx::sim::Snapshot &n) { return n.zipline_riding; }, 1.2),
                "matron line: the player takes the line");
        const int line = s.snapshot().zipline_line;
        const double length = s.zipline_line(static_cast<std::uint32_t>(line)).length;
        bool on_cable = false;
        bool player_off = false;
        for (int i = 0; i < 20 * static_cast<int>(Simulation::kTickRateHz) && !(on_cable && player_off); ++i) {
            const auto n = s.snapshot();
            const bool braking = n.zipline_riding && n.zipline_along_m > length - 12.0 && n.zipline_speed_mps > 4.0;
            (void)s.set_move_input(0.0, braking ? 1.0 : 0.0);
            tick(s);
            on_cable = on_cable || s.matron_state().on_line;
            const auto m = s.snapshot();
            // Stopped in the buffer: let go.
            if (m.zipline_riding && m.zipline_along_m > length - Zipline::kStopMarginM - Zipline::kBufferM &&
                std::abs(m.zipline_speed_mps) < 0.3) {
                (void)s.request_release();
            }
            player_off = player_off || (!m.zipline_riding && m.player_grounded && m.player_position.y < 150.0);
        }
        (void)s.set_move_input(0.0, 0.0);
        std::cout << "  matron line: the player down " << player_off << ", she on the cable " << on_cable << ", deaths "
                  << s.snapshot().death_count << "\n";
        require(player_off && s.snapshot().death_count == 0, "matron line: the player rode the line down to 143 m");
        require(on_cable, "matron line: in the Turn she went onto the cable after the player");
        // Away from the line's end along the south band; she comes down the
        // cable, lets go over the band and hunts there.
        require(go(s, -12.0, Matron::kBandSouthZ, true, 6.0, 1.0), "matron line: away along 143's south band");
        (void)s.set_move_input(0.0, 0.0);
        bool landed = false;
        for (int i = 0; i < 30 * static_cast<int>(Simulation::kTickRateHz) && !landed; ++i) {
            tick(s);
            const auto m = s.matron_state();
            landed = !m.on_line && m.alive && std::abs(m.floor_y - 143.0) < 0.1 && m.position.y < 145.0;
        }
        const auto down = s.matron_state();
        std::cout << "  matron line: she let go at (" << down.position.x << ", " << down.position.y << ", " << down.position.z
                  << "), level " << down.floor_y << ", slides " << down.cable_slides << ", alive " << down.alive << "\n";
        require(landed && down.cable_slides == 1, "matron line: down the cable, she let go over 143's band and hunts there");
        std::cout << "PASS scraperx_sim matron line: slides=" << down.cable_slides << " level=" << down.floor_y << "\n";
    }
    // ---- through the tower (the owner, 2026-10-18: she is not level-bound): a
    //      walk on the deck below hers, under her, is lost in the deck between;
    //      a sprint there carries up the shaft to her, and frail she goes to
    //      the line down to it and stands on it looking down ----
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        require(s.advance_frame(1.0).accepted, "matron below: settle");
        require(go(s, 8.0, Matron::kBandNorthZ, true, 4.0, 1.0) && s.matron_state().state == kStateStraining,
                "matron below: a sprint toward her locker wakes her");
        // Down the 154 m line to 143 while she strains (and hears nothing).
        const auto line = s.zipline_line(line_from(s, Matron::kDeckTop));
        require(walk_to(s, line.ramp_foot.x, line.ramp_foot.z, 6.0, 0.3) && walk_to(s, line.stand.x, line.stand.z, 6.0, 0.12),
                "matron below: up the line's ramp onto its stand");
        require(ride_line_down(s, line), "matron below: down the line to 143 m");
        // Crouched (quiet), round 143's west band to its north band's shaft
        // edge, under her post on the deck above.
        (void)s.set_crouch_input(true);
        require(go(s, -Matron::kBandX, Matron::kBandSouthZ, false, 40.0, 1.0) &&
                    go(s, -Matron::kBandX, Matron::kBandNorthZ, false, 60.0, 1.0) &&
                    go(s, -16.0, -132.6, false, 20.0, 0.3),
                "matron below: crouched round to 143's north edge of the shaft");
        // She bursts out, runs to where the player was then, finds nobody,
        // and goes to stand guard at her post above.
        require(until_guarding(s, 120.0) && s.snapshot().death_count == 0, "matron below: she stands guard at her post");
        // A walk along the shaft's edge on 143, under her post: 12 m from her
        // in a straight line (in a walk's earshot), but through the deck.
        (void)s.set_crouch_input(false);
        bool walk_heard = false;
        double walk_nearest = 1.0e9;
        for (const double x : {-4.0, -16.0, -4.0}) {
            for (int i = 0; i < 6 * static_cast<int>(Simulation::kTickRateHz); ++i) {
                const bool there = steer(s, x, -132.6, false, 0.3);
                tick(s);
                const auto m = s.matron_state();
                const auto p = s.snapshot().player_position;
                walk_nearest = std::min(walk_nearest, std::hypot(std::hypot(p.x - m.position.x, p.z - m.position.z),
                                                                 p.y - m.position.y));
                walk_heard = walk_heard || m.noise_heard > 0.0 || m.state != kStateGuarding;
                if (there) {
                    break;
                }
            }
        }
        std::cout << "  matron below: a walk on 143 under her post, " << walk_nearest << " m from her at nearest: heard "
                  << walk_heard << "\n";
        require(walk_nearest < 0.30 * 55.0 && !walk_heard,
                "matron below: a walk under her on the deck below, in a walk's earshot, is lost in the deck between");
        // A sprint along the same edge: up the shaft to her.
        double heard = 0.0;
        double lead_floor = 0.0;
        double stand_off = 1.0e9;
        bool at_stand = false;
        bool frail = true;
        for (int leg = 0; leg < 6 && !at_stand; ++leg) {
            const double x = leg % 2 == 0 ? -16.0 : -4.0;
            for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) && !at_stand; ++i) {
                const bool there = steer(s, x, -132.6, true, 0.6);
                tick(s);
                const auto m = s.matron_state();
                if (m.noise_heard > 0.0 && heard == 0.0) {
                    heard = m.noise_heard;
                    lead_floor = m.lead_floor_y;
                }
                frail = frail && m.turn == 0 && !m.on_line;
                stand_off = std::hypot(m.position.x - line.stand.x, m.position.z - line.stand.z);
                at_stand = stand_off < 0.6 && m.position.y > Matron::kDeckTop;
                if (there) {
                    break;
                }
            }
        }
        (void)s.set_move_input(0.0, 0.0);
        (void)s.set_sprint_input(false);
        const auto drawn = s.matron_state();
        std::cout << "  matron below: a sprint on 143 heard at loudness " << heard << ", her lead on the level at "
                  << lead_floor << "; she at (" << drawn.position.x << ", " << drawn.position.y << ", "
                  << drawn.position.z << "), " << stand_off << " m off the line's stand, frail " << frail << "\n";
        require(heard > 0.75 && std::abs(lead_floor - 143.0) < 0.1,
                "matron below: a sprint on the deck below carries up the shaft to her, a lead on 143");
        require(at_stand && frail && s.snapshot().death_count == 0,
                "matron below: frail, she goes to the line down to 143 and stands on it");
        std::cout << "PASS scraperx_sim matron below: walk_nearest_m=" << walk_nearest << " sprint_heard=" << heard
                  << " lead_floor=" << lead_floor << "\n";
    }
    // ---- dangerous, her hands aim you at the shaft: a player standing by its
    //      rim is shoved toward it, where a pest would only shove them away ----
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        require(s.advance_frame(1.0).accepted, "matron shove: settle");
        require(go(s, 8.0, Matron::kBandNorthZ, true, 4.0, 1.0) && s.matron_state().state == kStateStraining,
                "matron shove: a sprint toward her locker wakes her");
        // Crouched, to 1.5 m north of the shaft's north rim in the north band,
        // in plain sight of her locker's door.
        (void)s.set_crouch_input(true);
        require(go(s, 0.0, Matron::kMidZ + Matron::kShaftHalfM + 1.5, false, 20.0, 0.3),
                "matron shove: a crouched walk to the shaft's north rim");
        (void)s.set_crouch_input(false);
        s.debug_set_matron_stage(1);
        require(wait_for_her(s, 14.0, 40.0), "matron shove: out and dangerous, she comes for the player at the rim");
        const auto before = s.snapshot();
        double nearest_z = before.player_position.z;
        bool shoved = false;
        for (int i = 0; i < 12 * static_cast<int>(Simulation::kTickRateHz) && !shoved; ++i) {
            tick(s);
            nearest_z = std::min(nearest_z, s.snapshot().player_position.z);
            shoved = s.matron_state().shoves >= 1 && (s.snapshot().player_position.z < before.player_position.z - 0.6 ||
                                                      s.snapshot().death_count > before.death_count);
        }
        const auto after = s.snapshot();
        std::cout << "  matron shove: dangerous, shoves=" << s.matron_state().shoves << " player z " << before.player_position.z
                  << " -> " << after.player_position.z << " (nearest " << nearest_z << "), deaths " << before.death_count
                  << " -> " << after.death_count << "\n";
        require(shoved, "matron shove: dangerous, her shove throws the player toward the shaft's rim");
        require(s.matron_state().catches == 0, "matron shove: dangerous, her hands are not yet a death");
        std::cout << "PASS scraperx_sim matron shove: shoves=" << s.matron_state().shoves << " dz="
                  << after.player_position.z - before.player_position.z << "\n";
    }
    run_matron_mischief();
    run_matron_collision();
    run_matron_balance();
    run_matron_pipe();
    run_matron_room();
    // ---- her rage (the owner, 2026-10-19): fed only by sound at her ear, it
    //      moves her up a stage; quiet moves her back down, and not before her
    //      rage is well under the mark it rose past ----
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        require(s.advance_frame(1.0).accepted, "matron rage: settle");
        require(s.matron_state().rage == 0.0 && s.matron_state().stage == 0, "matron rage: asleep, she has none");
        require(go(s, 8.0, Matron::kBandNorthZ, true, 4.0, 1.0) && s.matron_state().state == kStateStraining,
                "matron rage: a sprint toward her locker wakes her");
        const auto line = s.zipline_line(line_from(s, Matron::kDeckTop));
        require(walk_to(s, line.ramp_foot.x, line.ramp_foot.z, 6.0, 0.3) &&
                    walk_to(s, line.stand.x, line.stand.z, 6.0, 0.12) && ride_line_down(s, line),
                "matron rage: down the line to 143 m while she strains");
        (void)s.set_crouch_input(true);
        require(go(s, -Matron::kBandX, Matron::kBandSouthZ, false, 40.0, 1.0) &&
                    go(s, -Matron::kBandX, Matron::kBandNorthZ, false, 60.0, 1.0) &&
                    go(s, -16.0, -132.6, false, 20.0, 0.3),
                "matron rage: crouched round to 143's north edge of the shaft");
        require(until_guarding(s, 120.0) && s.snapshot().death_count == 0, "matron rage: she stands guard at her post");
        const auto guarding = s.matron_state();
        require(guarding.stage == 0 && guarding.rage < Matron::kRageDangerousUp,
                "matron rage: out and guarding, she is a pest");
        // Sprints back and forth along 143's shaft edge under her post: heard
        // up the shaft pass after pass, where she cannot reach the player.
        (void)s.set_crouch_input(false);
        double sprint_s = 0.0;
        for (int leg = 0; leg < 40 && s.matron_state().stage == 0; ++leg) {
            const double x = leg % 2 == 0 ? -4.0 : -16.0;
            for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz); ++i) {
                const bool there = steer(s, x, -132.6, true, 0.6);
                tick(s);
                sprint_s += 1.0 / static_cast<double>(Simulation::kTickRateHz);
                if (there || s.matron_state().stage != 0) {
                    break;
                }
            }
        }
        (void)s.set_move_input(0.0, 0.0);
        (void)s.set_sprint_input(false);
        const auto dangerous = s.matron_state();
        std::cout << "  matron rage: " << sprint_s << " s of sprinting under her took her rage from " << guarding.rage
                  << " to " << dangerous.rage << ", stage " << static_cast<int>(dangerous.stage) << "\n";
        require(dangerous.stage == 1 && dangerous.rage >= Matron::kRageDangerousUp && s.snapshot().death_count == 0,
                "matron rage: a stretch of sprinting under her makes her dangerous");
        require(sprint_s > 15.0 && sprint_s < 150.0, "matron rage: it takes a stretch of noise, not a footfall and not an age");
        // Quiet: crouched and still. Her rage falls; she stays dangerous while
        // it is above the lower mark, and is a pest again once under it.
        (void)s.set_crouch_input(true);
        bool held = true;
        double pest_after_s = -1.0;
        double rage_at_pest = 0.0;
        for (int i = 0; i < 400 * static_cast<int>(Simulation::kTickRateHz) && pest_after_s < 0.0; ++i) {
            tick(s);
            const auto m = s.matron_state();
            if (m.rage >= Matron::kRageDangerousDown && m.stage != 1) {
                held = false;
            }
            if (m.stage == 0) {
                pest_after_s = i / static_cast<double>(Simulation::kTickRateHz);
                rage_at_pest = m.rage;
            }
        }
        std::cout << "  matron rage: quiet, a pest again after " << pest_after_s << " s, at rage " << rage_at_pest << "\n";
        require(pest_after_s > 30.0 && held && rage_at_pest < Matron::kRageDangerousDown && s.snapshot().death_count == 0,
                "matron rage: a quiet stretch drops her back to a pest, and not before her rage is under the lower mark");
        std::cout << "PASS scraperx_sim matron rage: sprint_s=" << sprint_s << " peak=" << dangerous.rage
                  << " pest_after_s=" << pest_after_s << "\n";
    }
    // ---- two levels down: a sprint on the deck two under hers carries up the
    //      shaft to her, and frail she goes to the way down toward it (the line
    //      to the deck between) and stands on it ----
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        require(s.advance_frame(1.0).accepted, "matron two below: settle");
        require(go(s, 8.0, Matron::kBandNorthZ, true, 4.0, 1.0) && s.matron_state().state == kStateStraining,
                "matron two below: a sprint toward her locker wakes her");
        const auto first = s.zipline_line(line_from(s, Matron::kDeckTop));
        require(walk_to(s, first.ramp_foot.x, first.ramp_foot.z, 6.0, 0.3) &&
                    walk_to(s, first.stand.x, first.stand.z, 6.0, 0.12) && ride_line_down(s, first),
                "matron two below: down the line from her deck to 143");
        // Crouched, round 143's south and west bands to the line down to 132.
        const auto second = s.zipline_line(line_from(s, 143.0));
        (void)s.set_crouch_input(true);
        require(go(s, -Matron::kBandX, Matron::kBandSouthZ, false, 40.0, 1.0) &&
                    go(s, -Matron::kBandX, second.ramp_foot.z, false, 30.0, 0.5) &&
                    walk_to(s, second.ramp_foot.x, second.ramp_foot.z, 10.0, 0.3),
                "matron two below: crouched round to the line from 143 down to 132");
        require(until_guarding(s, 120.0) && s.snapshot().death_count == 0,
                "matron two below: she stands guard at her post");
        (void)s.set_crouch_input(false);
        require(walk_to(s, second.stand.x, second.stand.z, 6.0, 0.12) && ride_line_down(s, second),
                "matron two below: down the line from 143 to 132");
        // To 132's edge of the shaft under her post (round the line's bottom
        // post), walking (out of her earshot two decks down), then a sprint
        // along it.
        const auto landed = s.snapshot().player_position;
        bool round = true;
        for (const auto &[x, z, seconds, tolerance] :
             {std::array<double, 4>{landed.x, landed.z + 3.0, 6.0, 0.6},
              std::array<double, 4>{Matron::kBandX, landed.z + 3.0, 6.0, 0.6},
              std::array<double, 4>{Matron::kBandX, Matron::kBandNorthZ, 20.0, 0.6},
              std::array<double, 4>{-4.0, Matron::kBandNorthZ, 20.0, 0.6}, std::array<double, 4>{-4.0, -132.6, 6.0, 0.3}}) {
            if (round && !go(s, x, z, false, seconds, tolerance)) {
                const auto p = s.snapshot().player_position;
                std::cout << "  matron two below: short of (" << x << ", " << z << "), at (" << p.x << ", " << p.y << ", "
                          << p.z << ")\n";
                round = false;
            }
        }
        require(round, "matron two below: round 132's bands to its north edge of the shaft");
        require(s.matron_state().state == kStateGuarding && s.matron_state().noise_heard == 0.0,
                "matron two below: a walk two decks down is not heard");
        double heard = 0.0;
        double lead_floor = 0.0;
        double stand_off = 1.0e9;
        bool at_stand = false;
        bool frail = true;
        for (int leg = 0; leg < 6 && !at_stand; ++leg) {
            const double x = leg % 2 == 0 ? -16.0 : -4.0;
            for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) && !at_stand; ++i) {
                const bool there = steer(s, x, -132.6, true, 0.6);
                tick(s);
                const auto m = s.matron_state();
                if (m.noise_heard > 0.0 && heard == 0.0) {
                    heard = m.noise_heard;
                    lead_floor = m.lead_floor_y;
                }
                frail = frail && m.turn == 0 && !m.on_line;
                stand_off = std::hypot(m.position.x - first.stand.x, m.position.z - first.stand.z);
                at_stand = stand_off < 0.6 && m.position.y > Matron::kDeckTop;
                if (there) {
                    break;
                }
            }
        }
        (void)s.set_move_input(0.0, 0.0);
        (void)s.set_sprint_input(false);
        const auto drawn = s.matron_state();
        std::cout << "  matron two below: a sprint on 132 heard at loudness " << heard << ", her lead on the level at "
                  << lead_floor << "; she at (" << drawn.position.x << ", " << drawn.position.y << ", "
                  << drawn.position.z << "), " << stand_off << " m off her deck's line's stand, frail " << frail << "\n";
        require(heard > 0.75 && std::abs(lead_floor - 132.0) < 0.1,
                "matron two below: a sprint two decks down carries up the shaft to her, a lead on 132");
        require(at_stand && frail && s.snapshot().death_count == 0,
                "matron two below: frail, she goes to the way down toward it (her deck's line) and stands on it");
        std::cout << "PASS scraperx_sim matron two below: sprint_heard=" << heard << " lead_floor=" << lead_floor
                  << " stand_off_m=" << stand_off << "\n";
    }
    // ---- heard where she has no way to: off the riser's bridge the player drops
    //      to 143's plate out over the shaft; she hears the landing up the shaft,
    //      and with no way there she searches at the place on her own lines over
    //      it, never out over the shaft (where she would stand stuck) nor on the
    //      catwalk, and goes back to guard her post. (A line nobody has ridden
    //      she does not take even in the Turn: she would let go short of its
    //      end, over the shaft.) ----
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        require(s.advance_frame(1.0).accepted, "matron no way: settle");
        require(go(s, 10.0, Matron::kBandNorthZ, true, 4.0, 1.0), "matron no way: a sprint toward her locker");
        (void)s.set_crouch_input(true);
        require(go(s, -6.0, Matron::kBandNorthZ, false, 30.0, 0.4) && go(s, -6.62, -133.0, false, 10.0, 0.15),
                "matron no way: crouched to the riser's bridge and up its steps");
        (void)s.set_move_input(0.0, 0.0);
        require(wait_for_her(s, 10.0, 60.0), "matron no way: she comes");
        (void)s.set_crouch_input(false);
        const auto deaths = s.snapshot().death_count;
        bool landed = false;
        for (int i = 0; i < 10 * static_cast<int>(Simulation::kTickRateHz) && !landed; ++i) {
            const auto p = s.snapshot();
            // Off the bridge's end as a player goes: up to it at a walk, then a
            // gentle nudge until the feet leave the rim, so the body drops onto
            // the plate below rather than out past it (planted feet no longer
            // slide off a rim on their own).
            if (p.player_position.y > 155.0 &&
                (p.player_position.z > -133.55 || p.player_grounded ||
                 p.traversal_state != scraperx::sim::TraversalState::None)) {
                (void)s.set_move_input(0.0, p.player_position.z > -133.55 ? -0.5 : -0.08);
                (void)s.set_facing(0.0, -1.0);
                if (p.traversal_state != scraperx::sim::TraversalState::None) {
                    (void)s.request_release();
                }
            } else {
                (void)s.set_move_input(0.0, 0.0);
            }
            tick(s);
            landed = s.snapshot().player_grounded && s.snapshot().player_position.y < 146.0;
        }
        (void)s.set_move_input(0.0, 0.0);
        require(landed && s.snapshot().death_count == deaths, "matron no way: the player drops to 143's plate, alive");
        bool heard_below = false;
        bool over_shaft = false;
        bool guarding = false;
        for (int i = 0; i < 150 * static_cast<int>(Simulation::kTickRateHz) && !guarding; ++i) {
            tick(s);
            const auto m = s.matron_state();
            heard_below = heard_below || (m.noise_heard > 0.0 && std::abs(m.lead_floor_y - 143.0) < 0.1);
            over_shaft = over_shaft || (m.alive && !m.on_line && std::abs(m.position.y - (Matron::kDeckTop + Matron::kStandCentre)) < 0.2 &&
                                        std::abs(m.position.x) < 16.5 && std::abs(m.position.z - Matron::kMidZ) < 16.5 &&
                                        std::abs(m.position.z - Matron::kMidZ) > 1.4);
            guarding = m.state == kStateGuarding && std::hypot(m.position.x - -10.3, m.position.z - -129.0) < 0.6;
        }
        const auto end = s.matron_state();
        std::cout << "  matron no way: heard the landing on 143 " << heard_below << "; out over the shaft " << over_shaft
                  << "; guarding " << guarding << " at (" << end.position.x << ", " << end.position.z << "), alive "
                  << end.alive << ", plates down " << end.planks_fallen << ", fooled " << end.fooled << "\n";
        require(heard_below, "matron no way: she hears the landing on 143 up the shaft, a lead there");
        require(!over_shaft && end.alive && end.planks_fallen == 0 && s.snapshot().death_count == deaths,
                "matron no way: she never goes out over the shaft or onto the catwalk after it");
        require(guarding, "matron no way: finding nobody, she goes back to guard her post");
        std::cout << "PASS scraperx_sim matron no way: fooled=" << end.fooled << "\n";
    }
    // ---- beside the catwalk on the band: the player stood there as she burst
    //      out; she searches there and then finds her way back to her post
    //      (put on the band's line, not the catwalk's she may not take). Then a
    //      sprint along the east band, past the riser's bridge steps from her
    //      post: the straight line is open air (the steps' flat treads are not
    //      a deck), and she hears it ----
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        require(s.advance_frame(1.0).accepted, "matron beside: settle");
        require(go(s, 10.0, Matron::kBandNorthZ, true, 4.0, 1.0), "matron beside: a sprint toward her locker");
        (void)s.set_crouch_input(true);
        require(go(s, Matron::kBandX, Matron::kBandNorthZ, false, 40.0, 0.8) &&
                    go(s, Matron::kBandX, -148.0, false, 40.0, 0.5) && go(s, 19.5, Matron::kMidZ, false, 10.0, 0.3),
                "matron beside: crouched to the band beside the catwalk's east end");
        (void)s.set_move_input(0.0, 0.0);
        bool out = false;
        for (int i = 0; i < 30 * static_cast<int>(Simulation::kTickRateHz) && !out; ++i) {
            tick(s);
            out = s.matron_state().state == kStateEmerging;
        }
        require(out, "matron beside: she bursts out");
        require(go(s, Matron::kBandX, -152.0, false, 20.0, 0.6) && go(s, Matron::kBandX, Matron::kBandSouthZ, false, 60.0, 0.6) &&
                    go(s, -Matron::kBandX, Matron::kBandSouthZ, false, 60.0, 0.6),
                "matron beside: crouched away round the south band");
        (void)s.set_move_input(0.0, 0.0);
        require(until_guarding(s, 90.0) && s.matron_state().alive, "matron beside: she finds her way back to guard her post");
        // Crouched round to the east band's shaft side, then a sprint along it.
        require(go(s, Matron::kBandX, Matron::kBandSouthZ, false, 60.0, 0.6) && go(s, Matron::kBandX, -137.0, false, 60.0, 0.6) &&
                    go(s, 17.5, -137.0, false, 10.0, 0.3),
                "matron beside: crouched round to the east band");
        require(s.matron_state().noise_heard == 0.0, "matron beside: crouched, unheard");
        (void)s.set_crouch_input(false);
        double heard = 0.0;
        for (int leg = 0; leg < 4 && heard == 0.0; ++leg) {
            const double z = leg % 2 == 0 ? -134.0 : -137.0;
            for (int i = 0; i < 3 * static_cast<int>(Simulation::kTickRateHz) && heard == 0.0; ++i) {
                const bool there = steer(s, 17.5, z, true, 0.3);
                tick(s);
                heard = s.matron_state().noise_heard;
                if (there) {
                    break;
                }
            }
        }
        (void)s.set_move_input(0.0, 0.0);
        (void)s.set_sprint_input(false);
        std::cout << "  matron beside: back at her post; a sprint on the east band by the shaft heard at " << heard << "\n";
        require(heard > 0.75, "matron beside: a sprint 29 m across her deck, past the steps' treads, is heard");
        std::cout << "PASS scraperx_sim matron beside: sprint_heard=" << heard << "\n";
    }
    // ---- climbs: frail, she stands at the ladder's foot looking up; in the Turn she takes
    //      hold and comes up it after the player, and over its head onto the ring --------
    {
        Simulation s(InitialSpawn::Deck154);
        s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
        require(s.advance_frame(1.0).accepted, "matron climb: settle");
        require(go(s, 8.0, Matron::kBandNorthZ, true, 4.0, 1.0), "matron climb: a sprint toward her locker");
        require(s.matron_state().state == kStateStraining, "matron climb: the sprint woke her");
        require(walk_to(s, 3.5, -128.2, 6.0) && walk_to(s, 3.5, -129.8, 4.0) && walk_to(s, 5.0, -129.6, 4.0, 0.08),
                "matron climb: to the ladder's foot");
        // As she bursts out of her locker, 7 m off, up the ladder (any later and
        // she has the player's legs before they are out of her reach).
        bool out = false;
        for (int i = 0; i < 40 * static_cast<int>(Simulation::kTickRateHz) && !out; ++i) {
            (void)s.set_facing(0.0, 1.0);
            tick(s);
            out = s.matron_state().state == kStateEmerging;
        }
        require(out, "matron climb: she bursts out");
        (void)s.set_facing(0.0, 1.0);
        require(s.request_traversal() && s.advance_frame(0.2).accepted && is_climbing(s.snapshot()),
                "matron climb: the ladder taken");
        const auto deaths = s.snapshot().death_count;
        double frail_s = 0.0;
        double held_at_y = 0.0;
        double peak_rate = 0.0;
        double last_y = s.matron_state().position.y;
        bool topped = false;
        for (int i = 0; i < 40 * static_cast<int>(Simulation::kTickRateHz) && !topped; ++i) {
            (void)s.set_move_input(0.0, 1.0);
            (void)s.set_facing(0.0, 1.0);
            tick(s);
            const auto m = s.matron_state();
            const auto p = s.snapshot();
            if (m.climbing) {
                held_at_y = held_at_y > 0.0 ? held_at_y : p.player_position.y;
                peak_rate = std::max(peak_rate, (m.position.y - last_y) * static_cast<double>(Simulation::kTickRateHz));
            } else if (m.climbs == 0 &&
                       std::hypot(m.position.x - Matron::kLadderX, m.position.z - Matron::kLadderFootZ) < 0.35) {
                frail_s += Simulation::kFixedStepSeconds;
            }
            last_y = m.position.y;
            topped = standing_above(p, 176.5);
        }
        (void)s.set_move_input(0.0, 0.0);
        const auto at_top = s.matron_state();
        std::cout << "  matron climb: frail at the foot " << frail_s << " s; took hold with the player at y "
                  << held_at_y << "; climbs " << at_top.climbs << ", up at " << peak_rate
                  << " m/s at most; she at y " << at_top.position.y << " as the player topped out; deaths "
                  << s.snapshot().death_count - deaths << "\n";
        require(topped && s.snapshot().death_count == deaths, "matron climb: the player tops out onto the 176 ring");
        require(frail_s > 1.0, "matron climb: frail, she stands at the ladder's foot and does not take hold");
        require(at_top.climbs == 1 && held_at_y > 0.0, "matron climb: in the Turn she takes hold and comes up after");
        require(peak_rate > 1.5 && peak_rate < Matron::kClimbBurstMps + 0.1,
                "matron climb: in jerks while the Turn has her");
        // Away east along the ring: she comes over the ladder's head and stands on it.
        require(go(s, 15.0, Matron::kRingNorthZ, false, 6.0), "matron climb: away east along the ring");
        (void)s.set_move_input(0.0, 0.0);
        bool over = false;
        for (int i = 0; i < 60 * static_cast<int>(Simulation::kTickRateHz) && !over; ++i) {
            tick(s);
            const auto m = s.matron_state();
            over = !m.climbing && m.alive && std::abs(m.floor_y - Matron::kRingFloor) < 0.1 &&
                   std::abs(m.position.y - (Matron::kRingFloor + Matron::kStandCentre)) < 0.06;
        }
        const auto on_ring = s.matron_state();
        std::cout << "  matron climb: over the head at (" << on_ring.position.x << ", " << on_ring.position.y << ", "
                  << on_ring.position.z << "), level " << on_ring.floor_y << ", state " << int(on_ring.state)
                  << ", deaths " << s.snapshot().death_count - deaths << "\n";
        require(over && s.snapshot().death_count == deaths, "matron climb: over the ladder's head she stands on the ring");
        std::cout << "PASS scraperx_sim matron climb: frail_s=" << frail_s << " climbs=" << on_ring.climbs
                  << " peak_mps=" << peak_rate << "\n";
    }
    // ---- the same start and inputs give the same chase ------------------------------
    {
        const auto run = [] {
            Simulation s(InitialSpawn::Deck154);
            s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
            wake_and_wait_east(s);
            (void)wait_for_her(s, 14.0, 40.0);
            const auto crossed = sprint_across(s, true);
            const auto p = s.snapshot().player_position;
            return std::array<double, 7>{crossed.end.position.x, crossed.end.position.z,
                                         static_cast<double>(crossed.end.planks_fallen), p.x, p.z,
                                         static_cast<double>(s.snapshot().death_count), crossed.end.rage};
        };
        const auto a = run();
        const auto b = run();
        require(std::memcmp(a.data(), b.data(), sizeof(a)) == 0,
                "matron: two identical chases end identically, bit for bit");
    }
}

// Her hearing takes the player's real room (03_EXECUTION/UNCANNY/HER_CHART.md; the owner,
// 2026-10-20: "if my dog barks, her head turns, that would be amazing"). The microphone's
// loudness over the room's own quiet comes in held, like the stick, and she hears it as the
// player's own noise, where the player is: a quiet room is not heard, a mutter only close by,
// and a bark carries as far as a hard landing.
void run_matron_room() {
    Simulation s(InitialSpawn::Deck154);
    s.debug_fix_matron_pace(3);   // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
    require(s.advance_frame(1.0).accepted, "matron room: settle");
    require(!s.set_room_loudness(std::nan("")) && s.set_room_loudness(0.0),
            "matron room: a loudness that is not a number is refused");
    require(go(s, 10.0, -128.5, true, 4.0, 1.0), "matron room: a sprint toward her locker");
    require(s.matron_state().state == kStateStraining, "matron room: the sprint woke her");
    // While she strains and bursts out: away down the east band and a creep round behind
    // crate S, out of her sight (the crash leg's way); she searches where the player was.
    require(go(s, Matron::kBandX, Matron::kBandNorthZ, false, 10.0, 0.3), "matron room: to the north-east corner");
    bool out = false;
    for (int i = 0; i < 30 * static_cast<int>(Simulation::kTickRateHz) && !out; ++i) {
        tick(s);
        out = s.matron_state().state == kStateEmerging;
    }
    require(out, "matron room: she bursts out");
    require(go(s, Matron::kBandX, -143.0, true, 3.0, 0.6), "matron room: a sprint down the east band");
    (void)s.set_crouch_input(true);
    require(go(s, Matron::kBandX, -157.6, false, 10.0, 0.3) && go(s, 24.5, -156.2, false, 6.0, 0.15),
            "matron room: a creep round behind crate S");
    bool searching = false;
    for (int i = 0; i < 30 * static_cast<int>(Simulation::kTickRateHz) && !searching; ++i) {
        tick(s);
        searching = s.matron_state().state == kStateSearching;
    }
    require(searching, "matron room: she searches where she lost the player");
    // The room at `loudness` for `seconds`, the player crouched and still: whether she heard it,
    // and whether she ever saw the player meanwhile.
    bool seen = false;
    double loudest = 0.0;
    const auto room_heard = [&](const double loudness, const double seconds) {
        (void)s.set_room_loudness(loudness);
        loudest = 0.0;
        const auto ticks = static_cast<int>(seconds * static_cast<double>(Simulation::kTickRateHz));
        for (int i = 0; i < ticks; ++i) {
            tick(s);
            loudest = std::max(loudest, s.matron_state().noise_heard);
            seen = seen || s.matron_state().sees_player;
        }
        (void)s.set_room_loudness(0.0);
        return loudest > 0.0;
    };
    stand_still(s, 0.0);
    require(!room_heard(0.0, 3.0) && s.matron_state().state == kStateSearching,
            "matron room: a quiet room is not heard");
    const double off_m = matron_player_distance(s);
    require(off_m > 0.1 * 55.0 && !room_heard(0.1, 2.0), "matron room: a mutter farther off than it carries is not heard");
    require(room_heard(1.0, 0.25), "matron room: a bark in the room is heard");
    require(!seen, "matron room: she never saw the player: it was the room she heard");
    const auto drawn = s.matron_state();
    const double at_bark_m = matron_player_distance(s);
    stand_still(s, 4.0);
    const double after_m = matron_player_distance(s);
    std::cout << "  matron room: the player " << off_m << " m off; the bark heard at " << loudest << "; state "
              << static_cast<int>(drawn.state) << ", wary " << drawn.wary << "; she comes from " << at_bark_m
              << " to " << after_m << " m in 4 s\n";
    require(drawn.state == kStateHunting && after_m < at_bark_m - 5.0,
            "matron room: the bark sends her hunting, toward the player");
    std::cout << "PASS scraperx_sim matron room: off_m=" << off_m << " heard=" << loudest << " came_m="
              << at_bark_m - after_m << "\n";
}
