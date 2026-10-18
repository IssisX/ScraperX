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
constexpr int kStateGuarding = static_cast<int>(Matron::State::Guarding);
constexpr int kStatePeering = static_cast<int>(Matron::State::Peering);

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
    // ---- her mind (the owner, 2026-10-18: B and C): she guards the way out
    // when she loses the player; a noise that led to nobody makes her wary,
    // the next checked from a vantage; fooled twice, a far noise does not draw
    // her off her post, a close one does; a death forgets it all ------------------
    {
        Simulation s(InitialSpawn::Deck154);
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
        stand_still(s, 0.0);
        const bool caught = run_until(20.0, [&] { return s.snapshot().death_count > deaths; });
        require(caught, "matron mind: standing still, the player is caught");
        tick(s);
        std::cout << "  matron mind: after the catch, fooled " << s.matron_state().fooled << "\n";
        require(s.matron_state().fooled == 0, "matron mind: a death forgets what she learned");
        std::cout << "PASS scraperx_sim matron mind: guarded at " << at_post.position.x << "," << at_post.position.z
                  << " peered_short_m=" << closest << " fooled_twice_stayed_m=" << farthest << "\n";
    }
    // ---- A: in the Turn she vaults what blocks her way; frail, she shoves it -------
    {
        Simulation s(InitialSpawn::Deck154);
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
        for (int i = 0; i < 20 * static_cast<int>(Simulation::kTickRateHz) && !winding; ++i) {
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
        // She comes for the cage; with her 4.5 m from its mouth, step back.
        bool near = false;
        for (int i = 0; i < 40 * static_cast<int>(Simulation::kTickRateHz) && !near; ++i) {
            (void)s.set_facing(0.0, 1.0);
            tick(s);
            const auto m = s.matron_state();
            near = m.state == kStateHunting && std::hypot(m.position.x + 10.5, m.position.z + 129.9) < 4.5;
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
        require(walk_to(s, -11.0, -130.3, 2.0, 0.1), "matron ride: to the cage's mouth at the top");
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
        require(held.alive && !crossed_without_leap && island_s > 5.9 && held.floor_y > 176.0,
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
        wake_and_wait_east(s);
        require(wait_for_her(s, 14.0, 40.0), "matron line: she comes");
        (void)s.set_crouch_input(false);
        // A long run round ahead of her (her first quiet spell over), back
        // along the north band to the 154 m line's ramp.
        require(go(s, Matron::kBandX, Matron::kBandSouthZ, true, 8.0, 1.0) &&
                    go(s, -Matron::kBandX, Matron::kBandSouthZ, true, 10.0, 1.0) &&
                    go(s, -Matron::kBandX, Matron::kBandNorthZ, true, 10.0, 1.0) && go(s, 1.0, Matron::kBandNorthZ, true, 6.0, 0.8) &&
                    walk_to(s, 3.0, -132.36, 4.0, 0.3),
                "matron line: round the deck to the line's ramp");
        require(walk_to(s, 6.0, -132.36, 6.0, 0.12), "matron line: up the ramp onto the stand");
        (void)s.set_move_input(0.0, 0.0);
        // With her near, the jump for the bar, and away down the line.
        bool near = false;
        for (int i = 0; i < 20 * static_cast<int>(Simulation::kTickRateHz) && !near; ++i) {
            tick(s);
            near = s.matron_state().state == kStateHunting && matron_player_distance(s) < 11.0;
        }
        require(near && s.snapshot().death_count == 0, "matron line: she comes for the player on the stand");
        (void)s.set_facing(0.0, -1.0);
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
            if (m.zipline_riding && m.zipline_along_m > length - 1.5 && std::abs(m.zipline_speed_mps) < 0.3) {
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
