// The Matron (owner's chase, 2026-10-16): the sealed locker on the 154 m
// deck, her build-up, her hunt, the catch, and the one thing that kills her,
// the rotten catwalk. Every leg is on player inputs from the deck spawn.
#include "sim_tests.hpp"

#include "sim/loose_stock.hpp"
#include "sim/matron.hpp"

namespace {
using scraperx::sim::LooseStock;
using scraperx::sim::InitialSpawn;
using scraperx::sim::Matron;
using scraperx::sim::MatronSnapshot;
using scraperx::sim::Simulation;

constexpr int kStateDormant = static_cast<int>(Matron::State::Dormant);
constexpr int kStateStraining = static_cast<int>(Matron::State::Straining);
constexpr int kStateEmerging = static_cast<int>(Matron::State::Emerging);
constexpr int kStateHunting = static_cast<int>(Matron::State::Hunting);
constexpr int kStateDead = static_cast<int>(Matron::State::Dead);

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
bool wait_for_her(Simulation &s, const double within, const double seconds) {
    const auto ticks = static_cast<int>(seconds * static_cast<double>(Simulation::kTickRateHz));
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
    return false;
}
} // namespace

void run_matron() {
    // ---- asleep: quiet or alone, she stays sealed ------------------------------
    {
        Simulation s(InitialSpawn::Deck154);
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
    // ---- the hunt: she runs faster than a walk, slower than a sprint, and catches a still player ----
    {
        Simulation s(InitialSpawn::Deck154);
        wake_and_wait_east(s);
        require(wait_for_her(s, 14.0, 40.0), "matron: she comes for a player waiting in the east band");
        const auto start = s.snapshot();
        stand_still(s, 12.0);
        const auto after = s.snapshot();
        require(s.matron_state().catches > 0 && after.death_count > start.death_count,
                "matron: a player who just stands there is caught, which is a death");
        const auto m = s.matron_state();
        require(m.alive && (m.state == kStateEmerging || m.state == kStateHunting),
                "matron: after a catch she is back at her locker, out again");
        require(std::hypot(after.player_position.x - start.player_position.x,
                           after.player_position.z - start.player_position.z) > 3.0 ||
                    after.death_count > start.death_count,
                "matron: the catch put the player back at a checkpoint");
    }
    // ---- the checkpoint stops being renewed while she hunts ----------------------
    {
        Simulation s(InitialSpawn::Deck154);
        wake_and_wait_east(s);
        // She is straining or out; the checkpoint was set where she burst out.
        require(wait_for_her(s, 14.0, 40.0), "matron: she comes");
        (void)s.set_crouch_input(false);
        // Run away round the loop: a long way, so a renewed checkpoint would be far from the release.
        require(go(s, Matron::kBandX, Matron::kBandSouthZ, true, 8.0, 1.0), "matron: a run down the east band");
        require(go(s, 0.0, Matron::kBandSouthZ, true, 8.0, 1.0), "matron: a run along the south band");
        const auto far = s.snapshot().player_position;
        const auto deaths_before = s.snapshot().death_count;
        // Stand and let her take the player.
        stand_still(s, 30.0);
        const auto back = s.snapshot();
        require(back.death_count > deaths_before, "matron: standing still on the loop gets the player caught");
        require(std::hypot(back.player_position.x - far.x, back.player_position.z - far.z) > 12.0,
                "matron: the death returns the player to the checkpoint of her release, not to where they stood");
    }
    // ---- the catwalk holds a climber ----------------------------------------------
    {
        Simulation s(InitialSpawn::Deck154);
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
        wake_and_wait_east(s);
        require(wait_for_her(s, 14.0, 40.0), "matron: she comes");
        (void)s.set_crouch_input(false);
        // A long run ahead of her, round the east and south bands, so her
        // first quiet spell is over; then stand still and let her come.
        require(go(s, Matron::kBandX, Matron::kBandSouthZ, true, 8.0, 1.0) &&
                    go(s, 0.0, Matron::kBandSouthZ, true, 8.0, 1.0) &&
                    go(s, -Matron::kBandX, Matron::kBandSouthZ, true, 8.0, 1.0),
                "matron: a run round the south band");
        (void)s.set_move_input(0.0, 0.0);
        const auto deaths_before = s.snapshot().death_count;
        double windup_still_s = 0.0;
        double lunge_peak = 0.0;
        double windup_at = -1.0;
        double lunge_at = -1.0;
        int turns_started = 0;
        std::uint8_t last_turn = 0;
        for (int i = 0; i < 40 * static_cast<int>(Simulation::kTickRateHz); ++i) {
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
            if (s.snapshot().death_count > deaths_before) {
                break;
            }
        }
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
    // ---- the same start and inputs give the same chase ------------------------------
    {
        const auto run = [] {
            Simulation s(InitialSpawn::Deck154);
            wake_and_wait_east(s);
            (void)wait_for_her(s, 14.0, 40.0);
            const auto crossed = sprint_across(s, true);
            const auto p = s.snapshot().player_position;
            return std::array<double, 6>{crossed.end.position.x, crossed.end.position.z,
                                         static_cast<double>(crossed.end.planks_fallen), p.x, p.z,
                                         static_cast<double>(s.snapshot().death_count)};
        };
        const auto a = run();
        const auto b = run();
        require(std::memcmp(a.data(), b.data(), sizeof(a)) == 0,
                "matron: two identical chases end identically, bit for bit");
    }
}
