// J-077 (issue #392, rule 34): a hard blow from a real object he can swing
// or drop takes her balance or puts her down for a few seconds, alive, and
// gives him a window to escape a hold; a bare punch does not. Runtime
// scenarios per stage, on real throws and a real low blow, never a debug
// launch. The pest's real thrown toolbox is already proven by the
// matron-heave beat in sim_tests_matron.cpp (run_matron_balance): a toolbox
// heaved at a pest coming upright puts her down, up again a few seconds
// later, nobody dead. This file proves the rest: the dangerous and changed
// throws, the low toolbox that downs a changed Matron, the bare punch at every
// stage, the hold-escape window an object blow opens, and the stock along
// the route.
#include "sim_tests.hpp"

#include "sim/loose_stock.hpp"
#include "sim/matron.hpp"

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::LooseStock;
using scraperx::sim::Matron;
using scraperx::sim::Simulation;
using scraperx::sim::UpMove;
using scraperx::sim::Vector3;

constexpr int kStateStraining = static_cast<int>(Matron::State::Straining);
constexpr int kStateHunting = static_cast<int>(Matron::State::Hunting);
constexpr int kStateGuarding = static_cast<int>(Matron::State::Guarding);
constexpr int kStateGrab = static_cast<int>(Matron::State::Grab);
constexpr int kStateStaggering = static_cast<int>(Matron::State::Staggering);
constexpr int kStateDown = static_cast<int>(Matron::State::Down);

void tick(Simulation &s) { (void)s.advance_frame(Simulation::kFixedStepSeconds); }

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

bool go(Simulation &s, const double x, const double z, const bool sprint, const double seconds,
        const double tolerance = 0.4) {
    for (int i = 0; i < static_cast<int>(seconds * static_cast<double>(Simulation::kTickRateHz)); ++i) {
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

// Wakes her: the shared start of every throw case below, the
// matron-heave beat's own opening. No wait for her guard: he stands in the
// open on this deck and she hunts him instead of guarding, and the throw's
// wait is written for her hunt, not her post.
void to_her_post(Simulation &s) {
    s.place_matron_home(154.0F);
    s.debug_fix_matron_pace(3);  // her old one speed: these legs meet her mechanics (paces: run_matron_pace)
    require(s.advance_frame(1.0).accepted, "hard blow: settle");
    require(go(s, 8.0, Matron::kBandNorthZ, true, 4.0, 1.0) && s.matron_state().state == kStateStraining,
            "hard blow: a sprint toward her locker wakes her");
}

// Takes up the piece at `entity` where it lies by the north band: the
// matron-heave beat's legs — west along the band on its leaf over the bay's
// cut, then north to the piece. The straight line does not cross the bay:
// the cut runs the whole band, x -3.9..-0.4, and the leaf is only on the
// band's centerline. Then the ammo handled_piece's pick-up: look down on
// it, nudge in until the hands have it.
void take_up_piece(Simulation &s, const std::uint64_t entity, const char *what) {
    const auto at = s.kit_body_position(s.kit_body_index(entity));
    require(walk_to(s, 0.4, Matron::kBandNorthZ, 20.0, 0.2) &&
                walk_to(s, -4.6, Matron::kBandNorthZ, 20.0, 0.2) && walk_to(s, at.x, at.z, 20.0, 0.3),
            what);
    const std::uint32_t body = s.kit_body_index(entity);
    (void)s.set_look_pitch(-0.5);
    for (int i = 0; i < static_cast<int>(3.0 * Simulation::kTickRateHz) &&
                    s.snapshot().carry_target_entity_id != entity;
         ++i) {
        const auto b = s.kit_body_position(body);
        const auto p = s.snapshot().player_position;
        const double d = std::max(std::hypot(b.x - p.x, b.z - p.z), 1.0e-3);
        (void)s.set_facing((b.x - p.x) / d, (b.z - p.z) / d);
        (void)s.set_move_input(d > 0.7 ? 0.3 * (b.x - p.x) / d : 0.0, d > 0.7 ? 0.3 * (b.z - p.z) / d : 0.0);
        tick(s);
    }
    (void)s.set_move_input(0.0, 0.0);
    (void)s.advance_frame(0.4);
    const auto me = s.snapshot().player_position;
    std::cout << "  hard blow take-up: piece " << entity << " at (" << at.x << ", " << at.z << "), he is "
              << std::hypot(at.x - me.x, at.z - me.z) << " m off, carry target "
              << s.snapshot().carry_target_entity_id << "\n";
    require(s.snapshot().carry_target_entity_id == entity, what);
    require(s.request_pick_up(), what);
    (void)s.advance_frame(0.6);
    require(s.snapshot().carrying_entity_id == entity, what);
    (void)s.set_look_pitch(0.0);
}

struct Thrown {
    bool threw = false;
    int worst = -1;  // worst the blow did: Down over Staggering, -1 neither
    double blow_mps = 0.0;
    int turn_at_hit = -1;  // her Turn when the blow first read, -1 none read
    std::uint32_t staggers = 0;
    std::uint32_t knockdowns = 0;
    bool alive = false;
    double floor_y = 0.0;  // her floor before the throw, to rise back onto
    double y = 0.0;        // her height before the throw
    double release_m = 0.0;  // her distance when the throw released
    double release_speed = 0.0;  // her speed when the throw released
    int release_stalking = -1;  // whether she stalked (held) at the release
    double box_mps = 0.0;    // the piece's fastest in flight
    double box_hit_mps = 0.0;  // the piece's speed when the blow first read
    double box_hit_y = 0.0;    // the piece's height when the blow first read
    double box_hit_dx = 0.0;   // the piece's east-west offset from her centre at the hit
    double box_hit_dz = 0.0;   // the piece's north-south offset from her centre at the hit
};
// Throws the piece in his hands at her as she bursts from her locker,
// holding `stage` the whole way; `look_pitch` aims the throw (0 at her
// chest, down low). A Dangerous or Changed matron at her hunt speed is
// crouched, her base 0.45 m and her take bar 1.90 m/s, and no throwable
// carries that (a toolbox reads 0.80 m/s). The throw that lands is the one
// thrown at her burst: she comes out of her locker upright, at the creep,
// her take bar 0.244 m/s, and he is standing still a few metres off with
// the piece settled, so the throw is clean and square. A free throw at
// range never gets that window: her Turn's lunge closes the distance, and
// on contact she shoves him, and a throw released while he is moving
// carries his velocity with it (a toolbox thrown at 2.6 m/s, north of her
// line, reads no blow at all). The throw releases on her first hunting
// tick in range, before her Turn can fire; a wind-up throw puts her down
// (her step is nothing there), any other staggers her. Either is the blow
// taking her balance. A chest throw staggers her; a low one puts her down
// (her step is nothing low).
Thrown throw_at_her(Simulation &s, const int stage, const double look_pitch) {
    Thrown out;
    const std::uint64_t piece = s.snapshot().carrying_entity_id;
    const std::uint32_t body = s.kit_body_index(piece);
    s.debug_fix_matron_pace(0);  // creep: she comes out upright, under the crouch bar
    // A few metres west of her locker, on her line: she comes out of it at
    // (11.0, -127.15) and he is in throw range the moment she hunts. The
    // locker is east, against the north rail; her post (-10.3, -129.0) is
    // where she guards, not where she wakes. The legs cross the bay on its
    // leaf, on the band's centreline: a straight line from the piece would
    // run through the cut. The walk is inside her strain, so she is still
    // in the locker while he gets there.
    require(walk_to(s, -4.6, Matron::kBandNorthZ, 10.0, 0.2) &&
                walk_to(s, 8.0, Matron::kBandNorthZ, 10.0, 0.15),
            "hard blow: to the burst spot with it");
    bool coming = false;
    int settled = 0;
    double release_d = 0.0;
    double release_speed = 0.0;
    int release_stalking = -1;
    auto box_was = s.kit_body_position(body);
    for (int i = 0; i < 30 * static_cast<int>(Simulation::kTickRateHz) && !coming; ++i) {
        s.debug_set_matron_stage(static_cast<std::uint8_t>(stage));
        tick(s);
        const auto m = s.matron_state();
        const auto p = s.snapshot().player_position;
        const double dx = m.position.x - p.x;
        const double dz = m.position.z - p.z;
        const double d = std::hypot(dx, dz);
        // Face her every tick, as the heave beat does: the throw's aim is
        // the facing, and a facing set once goes stale while she closes.
        if (d > 1.0e-3) {
            (void)s.set_facing(dx / d, dz / d);
        }
        const auto b = s.kit_body_position(body);
        const double swing = std::hypot(b.x - box_was.x, b.z - box_was.z);
        box_was = b;
        const bool in_front = d > 1.0e-3 && ((b.x - p.x) * dx + (b.z - p.z) * dz) / d > 0.3;
        settled = in_front && swing < 0.002 ? settled + 1 : 0;
        // She bursts: hunting, in range, the piece settled, and not
        // lunging. The throw releases on the first such tick, before her
        // Turn fires (half a second of far sight) and before she can close
        // on him and shove him. A wind-up throw puts her down; any other
        // staggers her.
        if (m.state == kStateHunting && m.stage == stage && d < 3.6 && m.turn != 2 &&
            settled >= static_cast<int>(Simulation::kTickRateHz) / 4) {
            coming = true;
            release_d = d;
            release_speed = m.speed;
            release_stalking = m.stalking ? 1 : 0;
        }
        if (i % static_cast<int>(Simulation::kTickRateHz) == 0) {
            std::cout << "  hard blow wait: t=" << i / static_cast<int>(Simulation::kTickRateHz) << "s state="
                      << m.state << " stage=" << int(m.stage) << " d=" << d << " turn=" << m.turn
                      << " speed=" << m.speed << " settled=" << settled << "\n";
        }
    }
    require(coming, "hard blow: she bursts, the piece in his hands");
    out.release_m = release_d;
    out.release_speed = release_speed;
    out.release_stalking = release_stalking;
    (void)s.set_look_pitch(look_pitch);
    tick(s);
    // The drawn path's end is the throw's aim: where the ray from his eye
    // lands. Print it against her centre, so a south aim reads as an aim
    // error and a centred aim with a south hit reads as a flight error.
    {
        const auto path = s.throw_prediction();
        const auto her_now = s.matron_state().position;
        const auto me_now = s.snapshot().player_position;
        if (!path.empty()) {
            const auto end = path.back();
            std::cout << "  hard blow aim: path ends (" << end.x << ", " << end.y << ", " << end.z << "), her centre ("
                      << her_now.x << ", " << her_now.y << ", " << her_now.z << "), he is (" << me_now.x << ", "
                      << me_now.y << ", " << me_now.z << ")\n";
        }
    }
    const auto throws0 = s.snapshot().throw_count;
    const auto stags0 = s.matron_state().staggers;
    const auto downs0 = s.matron_state().knockdowns;
    out.floor_y = s.matron_state().floor_y;
    out.y = s.matron_state().position.y;
    require(s.snapshot().up_move == UpMove::Throw && s.request_up(), "hard blow: up is THROW with it in the hands");
    double closest = 1.0e9;
    Vector3 thrown_v{0.0, 0.0, 0.0};
    for (int i = 0; i < static_cast<int>(1.2 * static_cast<double>(Simulation::kTickRateHz)); ++i) {
        s.debug_set_matron_stage(static_cast<std::uint8_t>(stage));
        tick(s);
        const auto m = s.matron_state();
        const auto bp = s.kit_body_position(body);
        closest = std::min(closest, std::hypot(bp.x - m.position.x, bp.z - m.position.z));
        if (i == 1) {
            thrown_v = s.kit_body_velocity(body);
        }
        out.blow_mps = std::max(out.blow_mps, m.blow_mps);
        if (out.turn_at_hit < 0 && m.blow_mps > 0.0) {
            out.turn_at_hit = m.turn;
            const auto bv = s.kit_body_velocity(body);
            out.box_hit_mps = std::hypot(bv.x, bv.y, bv.z);
            out.box_hit_y = bp.y;
            out.box_hit_dx = bp.x - m.position.x;
            out.box_hit_dz = bp.z - m.position.z;
        }
        const auto bv = s.kit_body_velocity(body);
        out.box_mps = std::max(out.box_mps, std::hypot(bv.x, bv.y, bv.z));
        const int state = m.state;
        if (state == kStateDown || (state == kStateStaggering && out.worst < 0)) {
            out.worst = state;
        }
    }
    // The throw's release is a tick after the request: the count is read
    // after the watch, not on the request's own tick.
    out.threw = s.snapshot().throw_count == throws0 + 1;
    const auto m = s.matron_state();
    const auto bend = s.kit_body_position(body);
    std::cout << "  hard blow flight: thrown velocity (" << thrown_v.x << ", " << thrown_v.y << ", " << thrown_v.z
              << "), closest to her centre " << closest << " m, box ends (" << bend.x << ", " << bend.y << ", " << bend.z
              << "), her centre (" << m.position.x << ", " << m.position.y << ", " << m.position.z << ")\n";
    out.staggers = static_cast<std::uint32_t>(m.staggers - stags0);
    out.knockdowns = static_cast<std::uint32_t>(m.knockdowns - downs0);
    out.alive = m.alive;
    return out;
}

// Rises again within her stage's time on the steel, on the floor she was
// knocked down from (her pre-blow floor, not an assumed deck), alive.
bool rises(Simulation &s, const double floor_y, const double y, const double seconds) {
    for (int i = 0; i < static_cast<int>(seconds * static_cast<double>(Simulation::kTickRateHz)); ++i) {
        tick(s);
        const auto m = s.matron_state();
        if (m.state != kStateDown && m.state != kStateStaggering) {
            return m.alive && std::abs(m.floor_y - floor_y) < 0.1 && std::abs(m.position.y - y) < 0.2;
        }
    }
    return false;
}

}  // namespace

// A bare punch (move 0, his escape in her hand: 1500 N over 3 ticks, 50 N-s,
// 0.227 m/s over her 220 kg) on a free Matron, at each stage: she takes it
// on her feet. The punch is forced for one press (debug_force_strike_move):
// the press would otherwise throw the jab, which does stagger a pest.
// Creep pace: the slowest closing, the hardest punch the power cap allows.
void run_hard_blow_punch() {
    constexpr double kTakeMps = 0.2446;  // upright, any stage: sqrt(g / 1.05) * 0.08
    for (const int stage : {0, 1, 2}) {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        for (int i = 0; i < 30; ++i) {
            tick(s);
        }
        const Vector3 p = s.snapshot().player_position;
        (void)s.set_facing(0.0, 1.0);
        s.launch_matron_for_test(Vector3{p.x, p.y - 0.20, p.z + 1.05});
        s.debug_hold_matron_hands(30.0);
        s.debug_fix_matron_pace(0);  // creep
        for (int i = 0; i < 5 * static_cast<int>(Simulation::kTickRateHz) &&
                        s.matron_state().state != kStateHunting;
             ++i) {
            tick(s);
        }
        require(s.matron_state().state == kStateHunting, "hard blow punch: she hunts (setup)");
        s.debug_set_matron_stage(static_cast<std::uint8_t>(stage));
        bool entered = false;
        for (int i = 0; i < 10 * static_cast<int>(Simulation::kTickRateHz) && !entered; ++i) {
            const Vector3 me = s.snapshot().player_position;
            const Vector3 her = s.matron_state().position;
            (void)s.set_facing(her.x - me.x, her.z - me.z);
            tick(s);
            s.debug_set_matron_stage(static_cast<std::uint8_t>(stage));
            const Vector3 now = s.matron_state().position;
            const Vector3 here = s.snapshot().player_position;
            entered = s.matron_state().state == kStateHunting &&
                      std::hypot(now.x - here.x, now.z - here.z) <= 1.15;
        }
        require(entered, "hard blow punch: she hunts into punch range (setup)");
        // The forced move first: the offer is then read for the punch, not
        // the jab the press would otherwise throw. The move itself is read
        // while the strike is under way: at guard it reads the next press.
        s.debug_force_strike_move(0);
        tick(s);
        const bool offered = s.snapshot().strike_offered;
        const std::uint64_t strikes0 = s.snapshot().strike_count;
        const std::uint64_t hits0 = s.snapshot().strike_hit_count;
        const std::uint64_t downs0 = s.matron_state().knockdowns;
        const std::uint64_t stags0 = s.matron_state().staggers;
        (void)s.request_strike();
        double peak = 0.0;
        std::uint8_t move = 255;
        for (int i = 0; i < 60; ++i) {
            tick(s);
            s.debug_set_matron_stage(static_cast<std::uint8_t>(stage));
            peak = std::max(peak, s.matron_state().blow_mps);
            if (s.snapshot().strike_phase != 0) {
                move = s.snapshot().strike_move;
            }
        }
        const auto &snap = s.snapshot();
        const auto &m = s.matron_state();
        std::cout << "  hard blow punch stage " << stage << ": offered=" << offered
                  << " hits=" << (snap.strike_hit_count - hits0) << " move=" << int(move)
                  << " impulse_ns=" << snap.strike_impulse_ns << " peak=" << peak << " state=" << int(m.state)
                  << "\n";
        require(offered, "hard blow punch: Action offers the punch with her body close in front");
        require(snap.strike_count - strikes0 == 1 && snap.strike_hit_count - hits0 == 1,
                "hard blow punch: one press throws one punch, and it lands");
        require(move == 0, "hard blow punch: the forced press threw the punch, move 0");
        require(snap.strike_impulse_ns > 0.0 && snap.strike_impulse_ns <= 50.01,
                "hard blow punch: 1500 N over 3 ticks is at most 50 N-s");
        require(peak > 0.10 && peak < kTakeMps,
                "hard blow punch: the blow reads, under her take bar at every stage");
        require(m.staggers - stags0 == 0 && m.knockdowns - downs0 == 0 && m.state != kStateStaggering &&
                    m.state != kStateDown,
                "hard blow punch: a bare punch takes nothing from her balance");
    }
    std::cout << "PASS scraperx_sim hard blow punch: no stagger, no knockdown, any stage\n";
}

// A real thrown toolbox at a dangerous Matron, and a real thrown toolbox
// at a fully changed one she faces: each takes her balance, a stagger or a
// knockdown, and she has her feet back within a few seconds, alive. (The
// pest's real throw is the matron-heave beat in run_matron_balance: down,
// up again, nobody dead.) The drum is too far west to carry to her locker
// inside her strain; the toolbox is the same blow at either stage.
void run_hard_blow_throw() {
    {
        Simulation s(InitialSpawn::Deck154);
        to_her_post(s);
        const std::uint32_t throwables = s.loose_stock_throwables_begin();
        const std::uint64_t toolbox = LooseStock::kFirstEntity + throwables + 1;
        take_up_piece(s, toolbox, "hard blow throw: to the toolbox by the north rail");
        const Thrown by_box = throw_at_her(s, 1, 0.0);
        const auto m = s.matron_state();
        std::cout << "  hard blow throw dangerous: threw=" << by_box.threw << " blow=" << by_box.blow_mps
                  << " m/s, worst=" << by_box.worst << ", staggers=" << by_box.staggers
                  << ", knockdowns=" << by_box.knockdowns << ", turn at hit=" << by_box.turn_at_hit
                  << ", release " << by_box.release_m << " m, her speed " << by_box.release_speed
                  << " m/s, stalking " << by_box.release_stalking << ", box max " << by_box.box_mps
                  << " m/s, at hit " << by_box.box_hit_mps << " m/s at y " << by_box.box_hit_y << " ("
                  << by_box.box_hit_dx << ", " << by_box.box_hit_dz << ") off her centre\n";
        require(by_box.threw, "hard blow throw: the toolbox thrown at a dangerous Matron");
        require((by_box.worst == kStateStaggering || by_box.worst == kStateDown) &&
                    by_box.staggers + by_box.knockdowns == 1,
                "hard blow throw: a toolbox takes a dangerous Matron's balance");
        require(by_box.alive && rises(s, by_box.floor_y, by_box.y, 3.0),
                "hard blow throw: dangerous, she has her feet back");
        (void)m;
    }
    {
        Simulation s(InitialSpawn::Deck154);
        to_her_post(s);
        const std::uint32_t throwables = s.loose_stock_throwables_begin();
        const std::uint64_t toolbox = LooseStock::kFirstEntity + throwables + 1;
        take_up_piece(s, toolbox, "hard blow throw: to the toolbox by the north rail");
        const Thrown by_box = throw_at_her(s, 2, 0.0);
        const auto m = s.matron_state();
        std::cout << "  hard blow throw changed: threw=" << by_box.threw << " blow=" << by_box.blow_mps
                  << " m/s, worst=" << by_box.worst << ", staggers=" << by_box.staggers
                  << ", knockdowns=" << by_box.knockdowns << ", turn at hit=" << by_box.turn_at_hit
                  << ", release " << by_box.release_m << " m, box max " << by_box.box_mps << " m/s, at hit "
                  << by_box.box_hit_mps << " m/s at y " << by_box.box_hit_y << " ("
                  << by_box.box_hit_dx << ", " << by_box.box_hit_dz << ") off her centre\n";
        require(by_box.threw, "hard blow throw: the toolbox thrown at a fully changed Matron");
        require((by_box.worst == kStateStaggering || by_box.worst == kStateDown) &&
                    by_box.staggers + by_box.knockdowns == 1,
                "hard blow throw: a toolbox takes a fully changed Matron's balance");
        require(by_box.alive && rises(s, by_box.floor_y, by_box.y, 3.0),
                "hard blow throw: fully changed, she has her feet back");
        (void)m;
    }
    std::cout << "PASS scraperx_sim hard blow throw: dangerous and changed lose their balance, alive\n";
}

// A toolbox thrown low at a fully changed Matron: low, her step is nothing,
// and the toolbox puts her down for her stage's seconds, alive. The escape
// window is her time on the steel: while she lies there he gets clear, and
// she takes nobody in it.
void run_hard_blow_low() {
    Simulation s(InitialSpawn::Deck154);
    to_her_post(s);
    const std::uint32_t throwables = s.loose_stock_throwables_begin();
    const std::uint64_t toolbox = LooseStock::kFirstEntity + throwables + 1;
    take_up_piece(s, toolbox, "hard blow low: to the toolbox by the north rail");
    const auto deaths0 = s.snapshot().death_count;
    const Thrown low = throw_at_her(s, 2, -0.2);
    std::cout << "  hard blow low changed: threw=" << low.threw << " blow=" << low.blow_mps
              << " m/s, worst=" << low.worst << ", staggers=" << low.staggers
              << ", knockdowns=" << low.knockdowns << ", turn at hit=" << low.turn_at_hit
              << ", release " << low.release_m << " m, box max " << low.box_mps << " m/s, at hit "
              << low.box_hit_mps << " m/s at y " << low.box_hit_y << " (" << low.box_hit_dx << ", "
              << low.box_hit_dz << ") off her centre\n";
    require(low.threw, "hard blow low: the toolbox thrown low at a fully changed Matron");
    require(low.worst == kStateDown && low.knockdowns == 1 && low.staggers == 0,
            "hard blow low: a toolbox at her shins puts a fully changed Matron down");
    require(low.alive && s.snapshot().death_count == deaths0, "hard blow low: down, alive, nobody dead");
    // While she lies there he gets clear east along the band, and she takes
    // nobody in it: her down is her stage's seconds, his window.
    const auto from = s.snapshot().player_position;
    const std::uint64_t catches0 = s.matron_state().catches;
    double moved = 0.0;
    (void)s.set_sprint_input(true);
    for (int i = 0; i < static_cast<int>(Matron::kChangedDownSeconds * static_cast<double>(Simulation::kTickRateHz));
         ++i) {
        (void)s.set_move_input(1.0, 0.0);
        (void)s.set_facing(1.0, 0.0);
        tick(s);
        const auto p = s.snapshot().player_position;
        moved = std::max(moved, std::hypot(p.x - from.x, p.z - from.z));
    }
    (void)s.set_move_input(0.0, 0.0);
    (void)s.set_sprint_input(false);
    std::cout << "  hard blow low: clear " << moved << " m in her " << Matron::kChangedDownSeconds << " s down\n";
    require(s.matron_state().catches == catches0, "hard blow low: nobody taken while she is down");
    require(moved >= 2.0, "hard blow low: he gets clear while she is down");
    require(rises(s, low.floor_y, low.y, 2.0), "hard blow low: up again, on her deck, alive");
    std::cout << "PASS scraperx_sim hard blow low: down, clear, up again\n";
}

// She reaches for him with the toolbox in his hands: thrown at her as she
// reaches, it staggers her, the grab ends before it holds him, and he walks
// clear with no re-grab inside her cooldown. The object blow opens the
// escape window her hold would otherwise close.
void run_hard_blow_hold() {
    Simulation s(InitialSpawn::Deck154);
    to_her_post(s);
    const std::uint32_t throwables = s.loose_stock_throwables_begin();
    const std::uint64_t toolbox = LooseStock::kFirstEntity + throwables + 1;
    take_up_piece(s, toolbox, "hard blow hold: to the toolbox by the north rail");
    require(walk_to(s, -6.0, -129.0, 10.0, 0.15), "hard blow hold: to the meeting spot with the toolbox");
    s.debug_matron_hold(0, 3.0);
    bool reaching = false;
    for (int i = 0; i < 60 * static_cast<int>(Simulation::kTickRateHz) && !reaching; ++i) {
        tick(s);
        reaching = s.matron_state().state == kStateGrab && !s.snapshot().held_by_matron;
    }
    require(reaching, "hard blow hold: she reaches for him (setup)");
    const auto stags0 = s.matron_state().staggers;
    const auto grabs0 = s.matron_state().grabs;
    const auto throws0 = s.snapshot().throw_count;
    (void)s.set_look_pitch(0.0);
    {
        const auto me = s.snapshot().player_position;
        const auto her = s.matron_state().position;
        (void)s.set_facing(her.x - me.x, her.z - me.z);
    }
    require(s.snapshot().up_move == UpMove::Throw && s.request_up(),
            "hard blow hold: up is THROW with the toolbox in the hands");
    int worst = -1;
    double blow = 0.0;
    bool ever_held = false;
    for (int i = 0; i < static_cast<int>(2.0 * static_cast<double>(Simulation::kTickRateHz)); ++i) {
        tick(s);
        const auto m = s.matron_state();
        blow = std::max(blow, m.blow_mps);
        ever_held = ever_held || s.snapshot().held_by_matron;
        if (m.state == kStateDown || (m.state == kStateStaggering && worst < 0)) {
            worst = m.state;
        }
    }
    // The throw's release is a tick after the request: the count is read
    // after the watch, not on the request's own tick.
    require(s.snapshot().throw_count == throws0 + 1, "hard blow hold: the toolbox thrown as she reaches");
    const auto m = s.matron_state();
    std::cout << "  hard blow hold: blow=" << blow << " m/s, worst=" << worst << ", staggers="
              << (m.staggers - stags0) << ", grabs=" << (m.grabs - grabs0) << ", held=" << ever_held << "\n";
    require(worst == kStateStaggering && m.staggers - stags0 == 1,
            "hard blow hold: the toolbox staggers her as she reaches");
    require(!ever_held && m.grabs - grabs0 == 0, "hard blow hold: the grab never held him");
    // He walks clear while the grab is on cooldown: no second grab inside
    // her 1.5 s, and he is 4 m off before she is back on her feet.
    const auto from = s.snapshot().player_position;
    require(walk_to(s, 2.0, -129.0, 10.0, 0.3), "hard blow hold: clear of her along the band");
    const auto away = s.snapshot().player_position;
    require(std::hypot(away.x - from.x, away.z - from.z) >= 4.0, "hard blow hold: four metres clear");
    const auto grabs1 = s.matron_state().grabs;
    stand_still(s, 2.0);
    require(s.matron_state().grabs == grabs1, "hard blow hold: no re-grab inside her cooldown");
    std::cout << "PASS scraperx_sim hard blow hold: the grab breaks, he walks clear\n";
}

// The stock that can do it is reachable along the route at each stage: on
// every deck the route climbs, a piece the hands carry (at most the carry's
// grip, LooseStock::heavy) lies on the ring round the shaft. The bands are
// the tower's own rings: the yard, deck 8's gangway, the stack rings
// 11..143, her deck 154, the upper rings 176.25..396.25 and the top decks.
// The 706 deck and the 802.64 summit are bare by design (dress_tower keeps
// their places but feels for no floor there), so they are not required.
void run_hard_blow_stock() {
    Simulation s(InitialSpawn::Deck154);
    static constexpr double kRings[] = {0.0, 8.0, 11.0, 22.0, 33.0, 44.0, 55.0, 66.0, 77.0, 88.0, 99.0,
                                        110.0, 121.0, 132.0, 143.0, 154.0, 176.25, 198.25, 220.25, 242.25,
                                        264.25, 286.25, 308.25, 330.25, 352.25, 374.25, 396.25, 640.25,
                                        662.1, 684.1, 728.1, 750.1};
    constexpr std::size_t kBands = sizeof(kRings) / sizeof(kRings[0]);
    std::size_t counts[kBands] = {};
    std::uint32_t carryable = 0;
    for (std::uint32_t i = 0; i < s.loose_stock_count(); ++i) {
        const auto piece = s.loose_stock_piece(i);
        if (LooseStock::heavy(static_cast<LooseStock::Kind>(piece.kind))) {
            continue;
        }
        ++carryable;
        const auto pos = s.kit_body_position(s.kit_body_index(LooseStock::kFirstEntity + i));
        if (std::abs(pos.x) > 35.0 || std::abs(pos.z + 150.0) > 35.0) {
            continue;  // not on the route's rings round the shaft
        }
        for (std::size_t b = 0; b < kBands; ++b) {
            if (pos.y >= kRings[b] - 0.5 && pos.y <= kRings[b] + 1.5) {
                ++counts[b];
                break;
            }
        }
    }
    std::cout << "  hard blow stock: " << carryable << " carryable pieces in the world\n";
    for (std::size_t b = 0; b < kBands; ++b) {
        std::cout << "    ring " << kRings[b] << " m: " << counts[b] << " carryable\n";
        require(counts[b] >= 1, "hard blow stock: a carryable piece on every deck of the route");
    }
    std::cout << "PASS scraperx_sim hard blow stock: a throwable on every deck\n";
}

void run_hard_blow() {
    run_hard_blow_punch();
    run_hard_blow_throw();
    run_hard_blow_low();
    run_hard_blow_hold();
    run_hard_blow_stock();
    std::cout << "PASS scraperx_sim hard blow\n";
}
SCRAPERX_GROUP("hard_blow", 875, run_hard_blow);
SCRAPERX_SLICE("hard-blow", run_hard_blow);


