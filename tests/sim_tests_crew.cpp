// J-120 (#442): the crew, proven. The yard and deck crew behave as
// src/sim/crew.hpp says they do, and this slice proves it:
//  - every state is reached from the input that should cause it and left
//    again (working <-> walking, yielding, startled, stumbling);
//  - no worker's horizontal acceleration ever passes what its feet can do
//    (kFootAccel, kDodgeAccel while yielding) except while it is shoved;
//  - no worker jumps between ticks by more than its speed allows;
//  - a worker never pushes the player off a deck edge, and never keeps him
//    from passing a narrow passage for more than a few seconds;
//  - a worker knocked off its deck lands and does not drive in the air;
//  - a load leaves the workers where they were (they are not saved).
//
// The workers group in sim_tests_movement.cpp already proves the 30 s
// left-alone walk (floors, pace, no stumbles/startles/yields), the welder
// holding its ground at the edge and the runner shoving the standing
// player; this slice measures the laws the issue names and the exits the
// workers group does not. Gap 13's worker deaths are Cory's call and are
// not here.
//
// The file runs past the 300-line diff cap (one slice, six families, and
// the watch that measures them); J-077 set the precedent for a test-file
// waiver, and this one will need it too.

#include "sim_tests.hpp"

#include "sim/crew.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace {

using scraperx::sim::Crew;
using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;

// Per-tick sampler over the snapshot workers. A worker's feet can only add
// kFootAccel (kDodgeAccel while yielding) per second, so its horizontal
// velocity can change by at most that times one tick; its position can move
// by at most its speed times one tick, and it can rise by at most its step
// lift per tick. Anything past those bounds is a teleport, a velocity
// write, or a shove -- and a shove is exactly the one case the feet are
// not responsible for. A shove is an impulse: it passes in a handful of
// ticks (a worker walking into a stand, a bar swinging into it, a landing
// settling), while a velocity write or a teleport would hold for the whole
// stretch. So the accel check counts runs of ticks past the feet's bound,
// not single ticks: a run of a few is a shove, a long run is a defect.
// The same holds for the rise: a worker knocked off a deck lands hard and
// the solver pushes its capsule back out of the floor over several ticks,
// so the rise is not measured for a few ticks after a landing. A
// stumbling worker is skipped at both ends of the tick (it is being
// shoved), as is a worker in the air (gravity and the kerb step move it,
// not its feet), a tick where the worker is in contact with the player or
// another worker (a contact changes velocity by ~1 m/s in one tick, and
// the contact is the shove, not the feet), and a tick with a vertical
// speed past the kerb step (it is mid-step or falling).
struct CrewWatch {
    bool armed = false;
    bool primed = false;
    const char* where = "";
    std::array<scraperx::sim::Vector3, 14> last{};
    std::array<double, 14> last_speed{};
    std::array<std::uint8_t, 14> last_state{};
    std::array<bool, 14> last_grounded{};
    std::array<int, 14> spike_run{};
    std::array<int, 14> land_cool{};
    int longest_spike = 0;
    double worst_spike_accel = 0.0;
    double worst_step_excess = 0.0;
    double worst_rise = 0.0;
    std::size_t worst_worker = 0;
    const char* worst_where = "";

    void reprime() { primed = false; }

    void sample(const Snapshot& n) {
        if (!armed) {
            return;
        }
        const double dt = Simulation::kFixedStepSeconds;
        for (std::size_t w = 0; w < n.worker_count; ++w) {
            const auto& cur = n.workers[w];
            if (!primed) {
                last[w] = cur.position;
                last_speed[w] = cur.speed;
                last_state[w] = cur.state;
                last_grounded[w] = cur.grounded;
                continue;
            }
            const double vx = (cur.position.x - last[w].x) / dt;
            const double vz = (cur.position.z - last[w].z) / dt;
            const double speed = std::hypot(vx, vz);
            const double step = std::hypot(cur.position.x - last[w].x, cur.position.z - last[w].z);
            const double rise = cur.position.y - last[w].y;
            const double accel = std::abs(speed - last_speed[w]) / dt;
            const bool shoved = cur.state == 4 || last_state[w] == 4;
            const bool airborne = !cur.grounded || !last_grounded[w];
            const bool stepping = std::abs((cur.position.y - last[w].y) / dt) > 0.5;
            bool contact = std::hypot(cur.position.x - n.player_position.x,
                                      cur.position.z - n.player_position.z) < 0.9;
            for (std::size_t o = 0; o < n.worker_count && !contact; ++o) {
                if (o != w) {
                    contact = std::hypot(cur.position.x - n.workers[o].position.x,
                                         cur.position.z - n.workers[o].position.z) < 0.9;
                }
            }
            if (cur.grounded && !last_grounded[w]) {
                land_cool[w] = 10;  // a landing settles over several ticks
            }
            if (!shoved && !airborne && !stepping && !contact) {
                const double bound = (cur.state == 2 || last_state[w] == 2) ? Crew::kDodgeAccel : Crew::kFootAccel;
                if (accel > bound + 0.5) {
                    ++spike_run[w];
                    if (spike_run[w] > longest_spike) {
                        longest_spike = spike_run[w];
                        worst_spike_accel = accel;
                        worst_worker = w;
                        worst_where = where;
                    }
                    if (accel > 5.0) {
                        std::cout << "  crew watch spike: worker " << w << " " << where << " accel=" << accel
                                  << " from=" << last[w].x << "," << last[w].y << "," << last[w].z
                                  << " to=" << cur.position.x << "," << cur.position.y << "," << cur.position.z
                                  << " speed " << last_speed[w] << "->" << cur.speed << " state="
                                  << static_cast<int>(last_state[w]) << "->" << static_cast<int>(cur.state)
                                  << " grounded=" << last_grounded[w] << "->" << cur.grounded << "\n";
                    }
                } else {
                    spike_run[w] = 0;
                }
                const double step_bound = std::max(cur.speed, last_speed[w]) * dt + 0.05;
                worst_step_excess = std::max(worst_step_excess, step - step_bound);
            }
            if (cur.grounded && last_grounded[w] && land_cool[w] == 0) {
                worst_rise = std::max(worst_rise, rise);
            }
            if (land_cool[w] > 0) {
                --land_cool[w];
            }
            last[w] = cur.position;
            last_speed[w] = cur.speed;
            last_state[w] = cur.state;
            last_grounded[w] = cur.grounded;
        }
        primed = true;
    }
};

// Round the 374 ring's south band to its east band, where the welder works.
bool walk_ring_east(Simulation& s) {
    return walk_to(s, -14.9, -164.9, 20.0, 0.4) && walk_to(s, 14.9, -164.9, 20.0, 0.4) &&
           walk_to(s, 15.8, -161.0, 10.0, 0.3);
}

double max_moved(const Snapshot& a, const Snapshot& b) {
    double far = 0.0;
    for (std::size_t w = 0; w < 14; ++w) {
        far = std::max(far, std::hypot(a.workers[w].position.x - b.workers[w].position.x,
                                       a.workers[w].position.z - b.workers[w].position.z));
    }
    return far;
}

void run_crew() {
    const double dt = Simulation::kFixedStepSeconds;
    CrewWatch watch;
    watch.armed = true;

    // S0: 30 s left alone, the watch armed. The walk itself (floors, pace,
    // no stumbles/startles/yields) is the workers group's proof; this is
    // the accel and teleport baseline over a long quiet stretch.
    {
        Simulation s(InitialSpawn::ExteriorGrade);
        require(s.advance_frame(0.5).accepted, "crew: settle");
        require(s.snapshot().worker_count == 14,
                "crew: fourteen workers, six on the yard, eight on the decks and rings");
        watch.where = "30 s alone";
        watch.reprime();
        for (int tick = 0; tick < 90 * 30; ++tick) {
            require(s.advance_frame(dt).accepted, "crew tick");
            watch.sample(s.snapshot());
        }
    }

    // S1: the state cycle. Worker 7 works the deck-2 north band: 12 s at one
    // station, 8 s at the other. It starts at work, walks, and works again.
    // Then a hard landing startles someone, and every flinch passes.
    {
        Simulation s(InitialSpawn::ExteriorGrade);
        require(s.advance_frame(0.5).accepted, "crew: settle");
        watch.where = "worker 7 cycle";
        watch.reprime();
        bool saw_working = s.snapshot().workers[7].state == 1;
        bool saw_walking = false;
        bool saw_working_again = false;
        for (int tick = 0; tick < 90 * 30 && !saw_working_again; ++tick) {
            require(s.advance_frame(dt).accepted, "crew tick");
            const Snapshot n = s.snapshot();
            watch.sample(n);
            if (n.workers[7].state == 0) {
                saw_walking = true;
            }
            if (saw_walking && n.workers[7].state == 1) {
                saw_working_again = true;
            }
        }
        require(saw_working && saw_walking && saw_working_again,
                "crew: a worker goes from work to its walk and back to work");
    }
    {
        Simulation s(InitialSpawn::SurvivableDrop);
        require(advance_until(s, [](const Snapshot& n) { return n.player_grounded; }, 4.0),
                "crew: the drop lands");
        require(s.advance_frame(0.1).accepted && s.snapshot().worker_startles >= 1,
                "crew: a hard landing near the crew startles someone");
        require(advance_until(s, [](const Snapshot& n) {
                    for (std::size_t w = 0; w < n.worker_count; ++w) {
                        if (n.workers[w].state == 3) {
                            return false;
                        }
                    }
                    return true;
                }, 5.0),
                "crew: the flinch passes and every worker goes back to its walk or work");
        watch.reprime();
    }

    // S2: yielding. Walk at the foreman; he steps aside, then goes back.
    {
        Simulation s(InitialSpawn::ExteriorGrade);
        require(s.advance_frame(2.5).accepted, "crew: yield settle");
        watch.where = "yield";
        watch.reprime();
        for (int tick = 0; tick < 90 * 10 && s.snapshot().worker_yields == 0; ++tick) {
            const auto p = s.snapshot().player_position;
            const auto w = s.snapshot().workers[0].position;
            const double d = std::hypot(w.x - p.x, w.z - p.z);
            (void)s.set_move_input((w.x - p.x) / d, (w.z - p.z) / d);
            require(s.advance_frame(dt).accepted, "crew: approach tick");
            watch.sample(s.snapshot());
        }
        (void)s.set_move_input(0.0, 0.0);
        require(s.snapshot().worker_yields >= 1, "crew: a walking player coming at a worker makes it step aside");
        require(s.advance_frame(2.0).accepted, "crew: yield after");
        watch.reprime();
        require(s.snapshot().worker_stumbles == 0, "crew: stepping aside in time, nobody is shoved");
        require(advance_until(s, [](const Snapshot& n) { return n.workers[0].state <= 1; }, 8.0),
                "crew: the player gone by, the foreman goes back to his walk");
        watch.reprime();
    }

    // S3: stumbling. Sprint into the rigger; he goes down and recovers.
    {
        Simulation s(InitialSpawn::ExteriorGrade);
        require(s.advance_frame(0.2).accepted, "crew: shove settle");
        watch.where = "stumble";
        watch.reprime();
        const auto rigger = s.snapshot().workers[1].position;
        require(walk_to(s, rigger.x, rigger.z + 14.0, 12.0, 0.4), "crew: line up on the rigger");
        watch.reprime();
        require(s.set_sprint_input(true), "crew: sprint");
        for (int tick = 0; tick < 90 * 4 && s.snapshot().worker_stumbles == 0; ++tick) {
            const auto p = s.snapshot().player_position;
            const auto w = s.snapshot().workers[1].position;
            const double d = std::hypot(w.x - p.x, w.z - p.z);
            (void)s.set_move_input((w.x - p.x) / d, (w.z - p.z) / d);
            require(s.advance_frame(dt).accepted, "crew: sprint tick");
            watch.sample(s.snapshot());
        }
        (void)s.set_sprint_input(false);
        (void)s.set_move_input(0.0, 0.0);
        require(s.snapshot().worker_stumbles >= 1, "crew: run into, a worker must stumble");
        require(advance_until(s, [](const Snapshot& n) { return n.workers[1].state <= 1; }, 6.0),
                "crew: a stumbled worker recovers and goes back to work");
        watch.reprime();
    }

    // S4: the edge. Stand on the 374 welder's route while he works it; he
    // walks into the standing player, and the player must stay on the ring.
    // Then the narrow band past him: the player must get through in seconds.
    {
        Simulation s(InitialSpawn::Ring374West);
        require(s.advance_frame(0.2).accepted, "crew: settle on the 374 ring");
        watch.where = "edge";
        watch.reprime();
        require(walk_ring_east(s), "crew: round the 374 ring to its east band");
        require(walk_to(s, 13.6, -150.5, 10.0, 0.3), "crew: stand on the welder's route");
        watch.reprime();
        const std::uint64_t shoves_before = s.snapshot().shove_count;
        const std::uint64_t deaths_before = s.snapshot().death_count;
        double min_y = s.snapshot().player_position.y;
        int after_shove = -1;
        for (int tick = 0; tick < 90 * 60; ++tick) {
            (void)s.set_move_input(0.0, 0.0);
            require(s.advance_frame(dt).accepted, "crew: edge tick");
            const Snapshot n = s.snapshot();
            watch.sample(n);
            min_y = std::min(min_y, n.player_position.y);
            if (n.shove_count > shoves_before && after_shove < 0) {
                after_shove = 90 * 3;
            }
            if (after_shove > 0 && --after_shove == 0) {
                break;
            }
        }
        const Snapshot n = s.snapshot();
        std::cout << "  crew edge: shoves=" << n.shove_count - shoves_before << " min_y=" << min_y << "\n";
        require(n.shove_count > shoves_before, "crew: the welder must walk into the standing player");
        require(n.death_count == deaths_before, "crew: shoved on a deck, the player must live");
        require(min_y > 374.0, "crew: a worker never pushes the player off a deck edge");
        const double t0 = s.snapshot().simulation_time_seconds;
        require(walk_to(s, 15.8, -138.0, 15.0, 0.3), "crew: through the narrow band past the welder");
        const double took = s.snapshot().simulation_time_seconds - t0;
        std::cout << "  crew narrow pass: took=" << took << " s\n";
        require(took < 10.0, "crew: a worker never keeps the player from passing for more than a few seconds");
        watch.reprime();
    }

    // S5: knocked off the deck. Sprint at the 374 welder from the band's
    // outer side, so the shove carries him west, over the well's edge.
    // He must fall, land, and go back to his walk or work -- and his feet
    // must add nothing to his speed while he is in the air. A bounce off
    // the shaft is a solver impulse, past what the feet can add in a tick;
    // feet driving would push at kFootAccel on every tick of the fall, so
    // the check counts runs of feet-sized ticks: a contact settles in a
    // handful of ticks, feet would push for the whole ~780-tick fall.
    {
        Simulation s(InitialSpawn::Ring374West);
        require(s.advance_frame(0.2).accepted, "crew: settle on the 374 ring");
        watch.where = "knock-off";
        watch.reprime();
        require(walk_ring_east(s), "crew: round the 374 ring to its east band");
        const std::size_t welder = 12;
        require(s.set_sprint_input(true), "crew: sprint");
        for (int tick = 0; tick < 90 * 4 && s.snapshot().worker_stumbles == 0; ++tick) {
            const auto p = s.snapshot().player_position;
            const auto w = s.snapshot().workers[welder].position;
            const double d = std::hypot(w.x - p.x, w.z - p.z);
            (void)s.set_move_input((w.x - p.x) / d, (w.z - p.z) / d);
            require(s.advance_frame(dt).accepted, "crew: knock-off tick");
            watch.sample(s.snapshot());
        }
        (void)s.set_sprint_input(false);
        require(s.snapshot().worker_stumbles >= 1, "crew: the sprint must shove the welder");
        // Get clear east, onto the band's outer side, and stop.
        for (int tick = 0; tick < 90 && s.snapshot().player_position.x < 15.5; ++tick) {
            (void)s.set_move_input(1.0, 0.0);
            require(s.advance_frame(dt).accepted, "crew: clear tick");
        }
        (void)s.set_move_input(0.0, 0.0);
        watch.reprime();
        double entry_speed = -1.0;
        double fall_accel = 0.0;
        double prev_speed = -1.0;
        int push_run = 0;
        int longest_push = 0;
        double player_min_y = s.snapshot().player_position.y;
        bool left_ground = false;
        bool landed = false;
        double land_y = 0.0;
        for (int tick = 0; tick < 90 * 60 && !landed; ++tick) {
            require(s.advance_frame(dt).accepted, "crew: fall tick");
            const Snapshot n = s.snapshot();
            watch.sample(n);
            const auto& wv = n.workers[welder];
            player_min_y = std::min(player_min_y, n.player_position.y);
            if (!left_ground) {
                if (!wv.grounded) {
                    left_ground = true;
                    entry_speed = wv.speed;
                }
                prev_speed = wv.speed;
            } else if (!wv.grounded) {
                const double accel = std::abs(wv.speed - prev_speed) / dt;
                if (accel > Crew::kFootAccel + 0.5) {
                    push_run = 0;  // a bounce: past what the feet can add
                } else {
                    fall_accel = std::max(fall_accel, accel);
                    push_run = accel > 0.5 ? push_run + 1 : 0;
                    longest_push = std::max(longest_push, push_run);
                }
                prev_speed = wv.speed;
            } else {
                const double y0 = wv.position.y;
                bool held = true;
                for (int t2 = 0; t2 < 90; ++t2) {
                    require(s.advance_frame(dt).accepted, "crew: landing tick");
                    const Snapshot m = s.snapshot();
                    watch.sample(m);
                    prev_speed = m.workers[welder].speed;
                    if (!m.workers[welder].grounded ||
                        std::abs(m.workers[welder].position.y - y0) > 0.1) {
                        held = false;
                        break;
                    }
                }
                if (held) {
                    landed = true;
                    land_y = y0;
                }
            }
        }
        const Snapshot n = s.snapshot();
        std::cout << "  crew knock-off: left_ground=" << left_ground << " entry=" << entry_speed
                  << " fall_accel=" << fall_accel << " longest_push=" << longest_push
                  << " landed=" << landed << " land_y=" << land_y
                  << " welder=" << n.workers[welder].position.x << "," << n.workers[welder].position.y << ","
                  << n.workers[welder].position.z << " state=" << static_cast<int>(n.workers[welder].state)
                  << " player_min_y=" << player_min_y << "\n";
        require(left_ground, "crew: the shove must carry the welder off its ring");
        require(landed, "crew: a worker knocked off its deck lands");
        require(longest_push <= 10, "crew: a worker off the ground does not drive");
        require(player_min_y > 374.0, "crew: the player stays on the ring while the welder falls");
        require(advance_until(s, [welder](const Snapshot& m) { return m.workers[welder].state <= 1; }, 15.0),
                "crew: landed, the welder goes back to its walk or work");
        watch.reprime();
    }

    // S6: not saved. Two worlds walk to different places; loading A's save
    // into B must leave B's workers where they were, and loading A's save
    // into A must leave A's where they were.
    {
        Simulation a(InitialSpawn::ExteriorGrade);
        require(a.advance_frame(10.0).accepted, "crew: A settles and walks");
        Simulation b(InitialSpawn::ExteriorGrade);
        require(b.advance_frame(17.0).accepted, "crew: B walks longer");
        const Snapshot before_a = a.snapshot();
        const Snapshot before_b = b.snapshot();
        require(max_moved(before_a, before_b) > 1.0, "crew: A and B walked to different places");
        const std::vector<std::uint8_t> saved = a.save_game();
        require(b.load_game(saved.data(), saved.size()), "crew: B loads A's save");
        const Snapshot after_b = b.snapshot();
        std::cout << "  crew not saved: B workers moved " << max_moved(before_b, after_b)
                  << " m on loading A's save\n";
        require(max_moved(before_b, after_b) < 1e-3, "crew: a load leaves the workers where they were");
        require(a.load_game(saved.data(), saved.size()), "crew: A loads its own save");
        require(max_moved(before_a, a.snapshot()) < 1e-3,
                "crew: a load leaves the workers where they were, in its own world too");
    }

    std::cout << "  crew watch: longest accel run past the feet " << watch.longest_spike << " ticks (worst "
              << watch.worst_spike_accel << " m/s^2, worker " << watch.worst_worker << ", " << watch.worst_where
              << ") worst step excess " << watch.worst_step_excess << " m worst rise " << watch.worst_rise
              << " m/tick\n";
    require(watch.longest_spike <= 5,
            "crew: no worker's horizontal acceleration passes what its feet can do, except while shoved");
    require(watch.worst_step_excess <= 0.001,
            "crew: no worker jumps between ticks by more than its speed allows");
    require(watch.worst_rise <= 3.5 * Simulation::kFixedStepSeconds + 0.02,
            "crew: no worker jumps upward between ticks");
}

}  // namespace

SCRAPERX_SLICE("crew", run_crew);
