// Corpses (sim/corpse.hpp): a lethal fall leaves the body where it hit, a
// ragdoll that comes to rest there, while the climber is back at the
// checkpoint; each death leaves another; at most Corpses::kMaxCorpses lie in
// the world; a loaded game has none.

#include "sim_tests.hpp"

#include "sim/corpse.hpp"

#include <cmath>
#include <iostream>

using scraperx::sim::Corpses;
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;

namespace {

// Off deck 4's south edge at a walk: down until the fall kills.
bool walk_off_deck_4(Simulation &s) {
    const std::uint64_t deaths = s.snapshot().death_count;
    (void)s.set_facing(0.0, 1.0);
    (void)s.set_move_input(0.0, 1.0);
    const bool died = advance_until(s, [deaths](const Snapshot &n) { return n.death_count > deaths; }, 20.0);
    (void)s.set_move_input(0.0, 0.0);
    return died;
}

// From the deck spawn, crouched (her locker not woken), east over the bay's
// leaf and past her locker to the band's east end, in line with the
// locker's east side: a run-up along the band clear of the bay (bay.hpp).
bool to_the_run_up(Simulation &s) {
    (void)s.set_crouch_input(true);
    const bool there = walk_to(s, 0.6, -128.5, 12.0, 0.2) && walk_to(s, 25.0, -129.5, 30.0, 0.3) &&
                       walk_to(s, 25.0, -126.2, 6.0, 0.1);
    (void)s.set_crouch_input(false);
    return there;
}

// J-061: wind is not his own run. A gust's push into a wall is a blow; his
// own drive's speed is not. The run-off helper below drives him west off a
// test platform into a test wall 12 m on and measures the blow.
struct WindOffRun {
    double takeoff_mps = 0.0;    // his westward speed on the last grounded tick
    double closing_mps = 0.0;    // his westward speed going into the wall-hit tick
    double blow_mps = 0.0;       // the listener's hit_closing_mps on the hit tick
    double wind_share_mps = 0.0; // closing less takeoff: what the gust added
    double max_wind_mps = 0.0;
    double max_wind_force_n = 0.0;
    std::uint64_t hits = 0;
    bool stopped = false;
};

// The pace he goes west off the edge: a sprint, a walk, or a drift —
// launched just past the edge with no velocity, the gust alone moving him.
enum class WindPace { Sprint, Walk, Drift };

WindOffRun wind_off_run(Simulation &s, const double platform_top_y, const WindPace pace, const bool wait_for_gust) {
    WindOffRun out;
    const double lane_z = -147.0;
    const double start_x = -32.0;
    // Launch him just above the platform near its east end; he lands and settles.
    (void)s.debug_launch_player({start_x, platform_top_y + 1.5, lane_z}, {0.0, 0.0, 0.0});
    require(advance_until(s, [](const Snapshot &n) { return n.player_grounded; }, 2.0),
            "wind: he lands on the platform");
    (void)s.advance_frame(0.5);
    require(s.snapshot().player_grounded, "wind: he settles on the platform");
    // A strong gust, for the wind contrasts (the gust cycles; a peak comes in seconds).
    if (wait_for_gust) {
        require(advance_until(s, [](const Snapshot &n) { return n.wind_mps >= 13.0; }, 15.0),
                "wind: a strong gust comes");
    }
    // West, to the edge.
    (void)s.set_facing(-1.0, 0.0);
    if (pace == WindPace::Sprint) {
        (void)s.set_move_input(-1.0, 0.0);
        (void)s.set_sprint_input(true);
    } else if (pace == WindPace::Walk) {
        (void)s.set_move_input(-1.0, 0.0);
    } else {
        // Drift: no run. Place him 3 m from the wall, in the air with no
        // velocity, and let the gust do all the work. (A slow creep cannot
        // leave the edge: the capsule balances on the corner and the weak
        // drive never tips it over, so the creep stalls there, grounded, at
        // the edge. And the full 12 m from the edge is too far to drift: the
        // gust only acts while he is in the zone (y 262-300), about 2 s of
        // fall, and after that he coasts — he falls below the wall before
        // he reaches it. Three metres is inside what one gust peak gives.)
        (void)s.debug_launch_player({-61.0, platform_top_y + 1.0, lane_z}, {0.0, 0.0, 0.0});
    }
    // Tick by tick, west, to the wall. Neutral air input once airborne, so
    // only the gust moves him in the air.
    const std::uint64_t hits_before = s.snapshot().hit_count;
    const double budget = pace == WindPace::Drift ? 12.0 : 8.0;
    const std::uint32_t budget_ticks = static_cast<std::uint32_t>(budget * 90.0);
    bool airborne = false;
    bool hit_seen = false;
    // The blow lives only on the tick its contact is NEW (the listener
    // charges it in OnContactAdded, and begin_tick zeroes it): a gentle
    // touch takes a tick or two for the solver to stop him, so by the
    // tick his speed collapses the blow is already gone. Keep the
    // hardest charge seen anywhere in the flight; the wall is the only
    // side contact out there, so it is the wall hit's blow.
    double max_blow_mps = 0.0;
    for (std::uint32_t tick = 0; tick < budget_ticks; ++tick) {
        const Snapshot before = s.snapshot();
        const double west_into_tick = -before.player_linear_velocity.x;
        if (before.player_grounded) out.takeoff_mps = west_into_tick;
        out.max_wind_mps = std::max(out.max_wind_mps, before.wind_mps);
        if (!s.advance_frame(Simulation::kFixedStepSeconds).accepted) break;
        const Snapshot after = s.snapshot();
        out.max_wind_force_n = std::max(out.max_wind_force_n, after.wind_force_n);
        max_blow_mps = std::max(max_blow_mps, after.hit_closing_mps);
        if (!after.player_grounded && !airborne) {
            airborne = true;
            (void)s.set_move_input(0.0, 0.0);
            (void)s.set_sprint_input(false);
        }
        // The wall contact is the tick his westward speed collapses, not the
        // tick the blow signal fires: on his own run the blow is ~0 by design
        // (the fix excuses it), so hit_closing_mps never fires there. The
        // blow is read on the collapse tick, where the listener has just
        // charged it. He is a meter from the wall's east face (x=-64) when
        // it collapses; nothing else out there can stop him in one tick.
        // The collapse is RELATIVE, not absolute: the drift approaches under
        // 1 m/s (one gust peak gives ~0.99 m/s), so an absolute "under 1.0"
        // after-speed is true on every drift tick and the detector fires
        // before he touches anything, reading a 0 blow. Halving in one tick
        // is the impact signature at any speed: on a free tick his speed
        // barely changes, on the wall tick the wall zeroes it.
        if (!hit_seen && airborne && west_into_tick > 0.5 &&
            -after.player_linear_velocity.x < 0.5 * west_into_tick &&
            after.player_position.x <= -63.0) {
            hit_seen = true;
            out.closing_mps = west_into_tick;
            out.blow_mps = max_blow_mps;
            out.wind_share_mps = out.closing_mps - out.takeoff_mps;
        }
        if (hit_seen && horizontal_speed(after) < 1.0) {
            out.stopped = true;
            break;
        }
    }
    (void)s.set_move_input(0.0, 0.0);
    (void)s.set_sprint_input(false);
    out.hits = s.snapshot().hit_count - hits_before;
    require(hit_seen, "wind: he hits the wall");
    require(out.stopped, "wind: he stops at the wall");
    return out;
}

} // namespace

void run_corpse() {
    using scraperx::sim::InitialSpawn;
    Simulation s(InitialSpawn::Deck4South);
    require(s.advance_frame(0.5).accepted && s.snapshot().corpse_count == 0, "corpse: none before a death");
    require(walk_off_deck_4(s), "corpse: walking off deck 4 must kill");
    const Snapshot dead = s.snapshot();
    std::cout << "  corpse: died at " << dead.death_speed_mps << " m/s, hit at (" << dead.death_point.x << ", "
              << dead.death_point.y << ", " << dead.death_point.z << "), the climber back at (" << dead.player_position.x
              << ", " << dead.player_position.y << ", " << dead.player_position.z << ")\n";
    require(dead.corpse_count == 1 && dead.corpses_spawned == 1 && dead.death_speed_mps > 15.0,
            "corpse: the death leaves one body");
    require(dead.player_position.y > dead.death_point.y + 20.0, "corpse: the climber is back at the checkpoint, up on the deck");
    // It comes to rest where it hit, lying, not standing.
    require(advance_until(s, [&s](const Snapshot &) { return s.corpse_resting(0); }, 6.0),
            "corpse: the body comes to rest");
    const auto head = s.corpse_part(0, Corpses::kHead);
    const auto pelvis = s.corpse_part(0, Corpses::kPelvis);
    const double spread = std::hypot(head.position.x - dead.death_point.x, head.position.z - dead.death_point.z);
    std::cout << "  corpse: at rest, head at (" << head.position.x << ", " << head.position.y << ", " << head.position.z
              << "), pelvis y " << pelvis.position.y << ", " << spread << " m from the hit\n";
    require(head.position.y < dead.death_point.y + 0.6 && pelvis.position.y < dead.death_point.y + 0.6 && spread < 3.0,
            "corpse: it lies where the body hit, not standing");
    // Another death, another body; past the cap the oldest goes.
    for (int death = 2; death <= Corpses::kMaxCorpses + 1; ++death) {
        require(walk_off_deck_4(s), "corpse: walking off again must kill again");
        const Snapshot n = s.snapshot();
        require(n.corpses_spawned == static_cast<std::uint64_t>(death) &&
                    n.corpse_count == static_cast<std::uint32_t>(std::min(death, Corpses::kMaxCorpses)),
                "corpse: each death leaves a body, up to the cap");
    }
    // Saved and loaded: the world comes back without them.
    const auto bytes = s.save_game();
    require(s.load_game(bytes.data(), bytes.size()) && s.snapshot().corpse_count == 0,
            "corpse: a loaded game has no corpses");
    std::cout << "PASS scraperx_sim corpse: death_mps=" << dead.death_speed_mps << " rest_head_y=" << head.position.y
              << " hit_y=" << dead.death_point.y << " capped=" << Corpses::kMaxCorpses << "\n";
}
SCRAPERX_GROUP("corpse", 720, run_corpse);
SCRAPERX_SELECT("corpse", run_corpse, run_arm_break, run_wind_own_drive);

// The broken arm (kArmBreakHitSpeed): thrown into a wall a little past a
// sprint's speed, the arm that took the blow is broken; his own run into it,
// flat out or in a jump, is no blow at all (J-048); a ride down the 154 m
// deck's line into the end's buffer too fast tears the hands off and wrenches
// the right arm, and the riser ladder back up from 143 m is the player's
// one-armed haul at half the climb's rate until it heals.
void run_arm_break() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::TraversalState;
    using scraperx::sim::Vector3;
    const auto hz = static_cast<int>(Simulation::kTickRateHz);
    // Sprints along (dx, dz) until a wall stops him (6 s at most), jumping
    // `jump_short` metres short of `wall` (where a sprint stopped) when one
    // is given: the fastest he ran, and whether he was stopped in the air.
    struct RunIn {
        double fastest = 0.0;
        bool stopped = false;
        bool in_air = false;
    };
    const auto run_in = [hz](Simulation &s, const double dx, const double dz, const Vector3 *wall = nullptr,
                             const double jump_short = 0.0) {
        RunIn out;
        bool jumped = false;
        for (int i = 0; i < 6 * hz && !out.stopped; ++i) {
            const Snapshot n = s.snapshot();
            (void)s.set_facing(dx, dz);
            (void)s.set_move_input(dx, dz);
            (void)s.set_sprint_input(true);
            if (wall != nullptr && !jumped &&
                (wall->x - n.player_position.x) * dx + (wall->z - n.player_position.z) * dz < jump_short) {
                jumped = s.request_jump();
            }
            out.fastest = std::max(out.fastest, horizontal_speed(n));
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
            const Snapshot m = s.snapshot();
            out.stopped = out.fastest > 7.0 && horizontal_speed(m) < 1.0;
            out.in_air = out.stopped && jumped && !m.player_grounded && m.traversal_state == TraversalState::None;
        }
        (void)s.set_move_input(0.0, 0.0);
        (void)s.set_sprint_input(false);
        return out;
    };
    // Thrown along (dx, dz) into the wall a sprint stopped at (`wall`, where
    // he stood), from 1.5 m out at 9.2 m/s, a little past a sprint, and
    // clear of the floor until he meets it (0.5 m up, past the step-down's
    // reach: thrown, not stepping off): until the blow or a death (1 s).
    const auto throw_in = [](Simulation &s, const Vector3 &wall, const double dx, const double dz) {
        s.debug_launch_player(Vector3{wall.x - 1.5 * dx, wall.y + 0.5, wall.z - 1.5 * dz},
                              Vector3{9.2 * dx, 0.5, 9.2 * dz});
        return advance_until(s, [](const Snapshot &n) { return n.hit_count > 0 || n.death_count > 0; }, 1.0);
    };
    {
        // Head-on into the proving ground's wall-run wall from 27 m back:
        // flat out, then in a jump at that speed, his own run is no blow.
        Simulation s(InitialSpawn::WallRunApproach);
        require(s.advance_frame(0.5).accepted, "arm: settle");
        require(walk_to(s, 2.0, 0.0, 12.0, 0.1), "arm: 27 m back from the wall");
        const RunIn ran = run_in(s, 0.0, -1.0);
        const Snapshot met = s.snapshot();
        std::cout << "  arm: ran in at " << ran.fastest << " m/s; felt hits " << met.hit_count << ", breaks "
                  << met.arm_break_count << ", deaths " << met.death_count << "\n";
        require(ran.stopped && ran.fastest >= 8.5, "arm: flat out into the wall (setup)");
        require(met.hit_count == 0 && met.arm_break_count == 0 && met.death_count == 0,
                "arm: his own run flat out into the wall is no blow");
        const Vector3 wall = met.player_position;
        require(walk_to(s, 2.0, 0.0, 12.0, 0.1), "arm: 27 m back again");
        const RunIn leapt = run_in(s, 0.0, -1.0, &wall, 2.2);
        const Snapshot leap = s.snapshot();
        std::cout << "  arm: jumped in at " << leapt.fastest << " m/s, met in the air " << leapt.in_air
                  << "; felt hits " << leap.hit_count << ", breaks " << leap.arm_break_count << ", deaths "
                  << leap.death_count << "\n";
        require(leapt.stopped && leapt.in_air && leapt.fastest >= 8.5, "arm: a jump at that speed into the wall (setup)");
        require(leap.hit_count == 0 && leap.arm_break_count == 0 && leap.death_count == 0,
                "arm: his own jump into the wall is no blow");
        // Thrown into it a little faster: a blow lived, the arm that took it
        // broken.
        require(throw_in(s, wall, 0.0, -1.0), "arm: thrown into the wall");
        const Snapshot hit = s.snapshot();
        std::cout << "  arm: thrown in, hit at " << hit.hit_speed_mps << " m/s; arm broken " << hit.arm_broken
                  << " (side " << hit.broken_arm_side << "), breaks " << hit.arm_break_count << ", deaths "
                  << hit.death_count << "\n";
        require(hit.hit_count == 1 && hit.hit_speed_mps >= 8.5 && hit.hit_speed_mps < 10.0 && hit.death_count == 0,
                "arm: thrown into the wall, a blow lived");
        require(hit.arm_broken && hit.arm_break_count == 1 && hit.arm_broken_seconds_left > 29.0,
                "arm: the blow breaks the arm that took it");
    }
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        require(s.advance_frame(1.0).accepted, "arm: settle on the deck");
        // Crouched, not to wake her, along the north band and up the line's ramp.
        (void)s.set_crouch_input(true);
        require(walk_to(s, 1.0, -128.5, 20.0, 0.3) && walk_to(s, 3.0, -132.36, 8.0, 0.3) &&
                    walk_to(s, 6.0, -132.36, 10.0, 0.12),
                "arm: crouched up the ramp onto the 154 m line's stand");
        (void)s.set_crouch_input(false);
        require(s.advance_frame(0.4).accepted && s.matron_state().state == 0, "arm: she sleeps on");
        (void)s.set_facing(0.0, -1.0);
        require(s.request_jump() && advance_until(s, [](const Snapshot &n) { return n.zipline_riding; }, 1.2),
                "arm: the line taken");
        // Braked only to 10.5 m/s, more than the end's buffer takes (it
        // stops 7.5 here): the stop's jolt tears the hands off the bar.
        const double length = s.zipline_line(static_cast<std::uint32_t>(s.snapshot().zipline_line)).length;
        const double brake_to = 10.5;
        double run_in = 0.0;
        bool torn = false;
        for (int i = 0; i < 12 * hz && !torn; ++i) {
            const Snapshot n = s.snapshot();
            const bool braking = n.zipline_riding && n.zipline_along_m > length - 12.0 && n.zipline_speed_mps > brake_to;
            (void)s.set_move_input(0.0, braking ? 1.0 : 0.0);
            if (n.zipline_riding && n.zipline_along_m > length - 6.0) {
                run_in = std::max(run_in, n.zipline_speed_mps);
            }
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
            torn = s.snapshot().zipline_torn_count > 0;
        }
        (void)s.set_move_input(0.0, 0.0);
        require(advance_until(s, [](const Snapshot &n) { return n.player_grounded; }, 3.0), "arm: down on 143 m");
        const Snapshot down = s.snapshot();
        std::cout << "  arm: into the buffer at " << run_in << " m/s, torn " << down.zipline_torn_count
                  << "; arm broken " << down.arm_broken << " (side " << down.broken_arm_side << "), down at ("
                  << down.player_position.x << ", " << down.player_position.y << ", " << down.player_position.z
                  << "), deaths " << down.death_count << "\n";
        require(torn && down.death_count == 0 && down.player_position.y < 145.0, "arm: torn off at the end, alive on 143 m");
        require(down.arm_broken && down.broken_arm_side == 1, "arm: the stop's jolt wrenches the right arm");
    }
    {
        // Flat out along her deck's north band into her locker's steel side is
        // no blow; thrown into it, an arm broken, then up the ladder to the
        // 176 ring, one-armed until it heals. (The sprint wakes her; she is
        // out as the climb is well under way.)
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        require(s.advance_frame(1.0).accepted && to_the_run_up(s), "arm: back along the band");
        const RunIn ran = run_in(s, -1.0, 0.0);
        const Snapshot met = s.snapshot();
        require(ran.stopped && ran.fastest >= 8.5, "arm: flat out into the locker (setup)");
        require(met.hit_count == 0 && !met.arm_broken && met.death_count == 0,
                "arm: his own run flat out into the locker is no blow");
        const bool struck = throw_in(s, met.player_position, -1.0, 0.0);
        const Snapshot hit = s.snapshot();
        std::cout << "  arm: ran into her locker at " << ran.fastest << " m/s, felt hits " << met.hit_count
                  << "; thrown in, hit at " << hit.hit_speed_mps << " m/s; arm broken " << hit.arm_broken << ", deaths "
                  << hit.death_count << "\n";
        require(struck && hit.arm_broken && hit.death_count == 0, "arm: thrown into the locker, an arm broken");
        require(walk_to(s, 12.2, -128.2, 4.0) && walk_to(s, 3.5, -128.2, 8.0) && walk_to(s, 3.5, -129.8, 4.0) &&
                    walk_to(s, 5.0, -129.6, 4.0, 0.08),
                "arm: to the ladder's foot");
        (void)s.set_facing(0.0, 1.0);
        require(s.advance_frame(0.4).accepted && s.request_traversal() && s.advance_frame(0.2).accepted &&
                    is_climbing(s.snapshot()),
                "arm: the ladder taken one-armed");
        double one_armed = 0.0;
        double healed = 0.0;
        bool topped = false;
        double last_y = s.snapshot().player_position.y;
        for (int i = 0; i < 50 * hz && !topped; ++i) {
            const bool broken = s.snapshot().arm_broken;
            (void)s.set_move_input(0.0, 1.0);
            (void)s.set_facing(0.0, 1.0);
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
            const Snapshot n = s.snapshot();
            if (is_climbing(n)) {
                const double rate = (n.player_position.y - last_y) * static_cast<double>(Simulation::kTickRateHz);
                if (broken && n.arm_broken) {
                    one_armed = std::max(one_armed, rate);
                } else if (!broken) {
                    healed = std::max(healed, rate);
                }
            }
            last_y = n.player_position.y;
            topped = standing_above(n, 176.5);
        }
        (void)s.set_move_input(0.0, 0.0);
        std::cout << "  arm: up the ladder at " << one_armed << " m/s one-armed, " << healed
                  << " m/s healed; on the 176 ring " << topped << ", deaths " << s.snapshot().death_count << "\n";
        require(one_armed > 0.4 && one_armed < 0.5, "arm: one good arm hauls at half the climb's rate");
        require(healed > 0.85 && healed < 0.95, "arm: healed, both arms climb at the full rate");
        require(topped && s.snapshot().death_count == 0 && s.snapshot().arm_break_count == 1,
                "arm: on up to the ring, the healing no new break");
    }
    // One good arm holds a hang at half two hands' grip: the deck's line
    // braked to 7.5 m/s into its end holds two hands and tears one.
    const auto ride_to_the_stop = [](Simulation &s, const double brake_to) {
        (void)s.set_facing(0.0, -1.0);
        const bool jumped = s.request_jump();
        const bool caught = advance_until(s, [](const Snapshot &n) { return n.zipline_riding; }, 1.2);
        if (!caught) {
            const Snapshot d = s.snapshot();
            std::cout << "  arm: jump " << jumped << " missed the bar at (" << d.player_position.x << ", "
                      << d.player_position.y << ", " << d.player_position.z << ") trav " << int(d.traversal_state)
                      << " grounded " << d.player_grounded << " arm " << d.arm_broken << "\n";
        }
        require(jumped && caught, "arm: the line taken");
        const double length = s.zipline_line(static_cast<std::uint32_t>(s.snapshot().zipline_line)).length;
        bool stopped = false;
        for (int i = 0; i < 12 * hz && !stopped && s.snapshot().zipline_torn_count == 0; ++i) {
            const Snapshot n = s.snapshot();
            const bool braking = n.zipline_riding && n.zipline_along_m > length - 12.0 && n.zipline_speed_mps > brake_to;
            (void)s.set_move_input(0.0, braking ? 1.0 : 0.0);
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
            const Snapshot m = s.snapshot();
            stopped = m.zipline_riding && m.zipline_along_m > length - 1.5 && std::abs(m.zipline_speed_mps) < 0.3;
        }
        (void)s.set_move_input(0.0, 0.0);
        return s.snapshot().zipline_torn_count;
    };
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        (void)s.set_crouch_input(true);
        require(s.advance_frame(1.0).accepted && walk_to(s, 1.0, -128.5, 20.0, 0.3) && walk_to(s, 3.0, -132.36, 8.0, 0.3) &&
                    walk_to(s, 6.0, -132.36, 10.0, 0.12),
                "arm: two arms, crouched onto the line's stand");
        (void)s.set_crouch_input(false);
        const auto torn = ride_to_the_stop(s, 7.5);
        std::cout << "  arm: two arms into the stop at 7.5 m/s: torn " << torn << "\n";
        require(torn == 0 && s.snapshot().zipline_riding, "arm: braked to 7.5 m/s, two hands hold at the stop");
    }
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        require(s.advance_frame(1.0).accepted && to_the_run_up(s), "arm: back along the band again");
        const bool stopped = run_in(s, -1.0, 0.0).stopped;
        const bool struck = stopped && throw_in(s, s.snapshot().player_position, -1.0, 0.0) && s.snapshot().arm_broken;
        (void)s.set_crouch_input(true);
        require(struck && walk_to(s, 12.2, -128.5, 4.0, 0.3) && walk_to(s, 3.0, -128.5, 8.0, 0.3) &&
                    walk_to(s, 3.0, -132.36, 8.0, 0.3) &&
                    walk_to(s, 6.0, -132.36, 10.0, 0.12),
                "arm: an arm broken thrown into the locker, crouched onto the line's stand");
        (void)s.set_crouch_input(false);
        const auto torn = ride_to_the_stop(s, 7.5);
        std::cout << "  arm: one arm into the stop at 7.5 m/s: torn " << torn << ", arm broken " << s.snapshot().arm_broken
                  << ", deaths " << s.snapshot().death_count << "\n";
        require(torn == 1 && s.snapshot().death_count == 0, "arm: one hand tears off at the stop two hands held");
    }
    {
        // Hand over hand needs a good hand to let go: an arm broken thrown
        // into the wall-run wall, the monkey bars' first bar is caught and
        // never left.
        Simulation s(InitialSpawn::SwingApproach);
        require(s.advance_frame(0.4).accepted && walk_to(s, 2.0, 0.0, 14.0, 0.1), "arm: 27 m back from the wall");
        const bool stopped = run_in(s, 0.0, -1.0).stopped;
        const bool struck = stopped && throw_in(s, s.snapshot().player_position, 0.0, -1.0) && s.snapshot().arm_broken;
        require(struck && walk_to(s, -1.0, -20.0, 14.0, 0.1), "arm: an arm broken, back to the monkey bars");
        (void)s.set_facing(1.0, 0.0);
        (void)s.set_move_input(1.0, 0.0);
        require(s.request_jump() &&
                    advance_until(s, [](const Snapshot &n) { return n.traversal_state == scraperx::sim::TraversalState::Swinging; }, 1.0),
                "arm: the first bar caught with the good hand");
        (void)s.advance_frame(2.0);
        (void)s.set_move_input(0.0, 0.0);
        const Snapshot hung = s.snapshot();
        std::cout << "  arm: on the bars one-armed: reaches " << hung.swing_reach_count << ", still on "
                  << (hung.traversal_state == scraperx::sim::TraversalState::Swinging) << "\n";
        require(hung.swing_reach_count == 0 && hung.arm_broken, "arm: one-armed, no bar reached hand over hand");
    }
    std::cout << "PASS scraperx_sim arm break: his own run is no blow, flat out or jumped; a throw and a wrench break an"
                 " arm; one arm climbs at half rate, holds half the grip, reaches no bar\n";
}
SCRAPERX_GROUP("arm_break", 730, run_arm_break);

void run_wind_own_drive() {
    using scraperx::sim::InitialSpawn;
    // Two extra-solid pairs (a platform and a wall, test-only): a wind pair
    // high in the west-face gust zone (platform top 280.5; the zone is
    // y 262-300) and a calm pair below every gust zone (platform top 200.5).
    // The wall's east face is x=-64, 12 m west of the platform's west edge.
    const std::vector<Simulation::TestExtraSolid> wind_solids = {
        Simulation::TestExtraSolid{-40.0, 280.0, -147.0, 12.0F, 0.5F, 5.0F, false},
        Simulation::TestExtraSolid{-65.0, 227.5, -147.0, 1.0F, 77.5F, 5.0F, false},
    };
    const std::vector<Simulation::TestExtraSolid> calm_solids = {
        Simulation::TestExtraSolid{-40.0, 200.0, -147.0, 12.0F, 0.5F, 5.0F, false},
        Simulation::TestExtraSolid{-65.0, 147.5, -147.0, 1.0F, 77.5F, 5.0F, false},
    };
    // (5) adds the east wall the steer-back meets: its west face is
    // x=-57.5, 6.5 m east of the west wall, top 185.0 — under the
    // sprint's path at that x (he passes above 189), so he only meets it
    // steering back east.
    const std::vector<Simulation::TestExtraSolid> rebound_solids = {
        Simulation::TestExtraSolid{-40.0, 200.0, -147.0, 12.0F, 0.5F, 5.0F, false},
        Simulation::TestExtraSolid{-65.0, 147.5, -147.0, 1.0F, 77.5F, 5.0F, false},
        Simulation::TestExtraSolid{-57.0, 147.5, -147.0, 1.0F, 37.5F, 5.0F, false},
    };

    // (1) The same run, no wind: nothing charged.
    {
        Simulation s(InitialSpawn::ExteriorGrade, calm_solids);
        const WindOffRun run = wind_off_run(s, 200.5, WindPace::Sprint, false);
        std::cout << "  wind: calm sprint takeoff=" << run.takeoff_mps << " closing=" << run.closing_mps
                  << " blow=" << run.blow_mps << " wind=" << run.max_wind_mps << "\n";
        require(run.takeoff_mps >= 7.0 && run.takeoff_mps <= 9.1, "wind: (1) he sprints off the edge");
        require(run.max_wind_mps == 0.0, "wind: (1) no wind below the gust zone");
        require(run.blow_mps <= 0.4, "wind: (1) his own run into the wall is no blow");
        require(run.hits == 0, "wind: (1) no hit felt");
        require(s.snapshot().death_count == 0, "wind: (1) he lives");
    }

    // (2) Wind only, no run: the full closing charged.
    {
        Simulation s(InitialSpawn::ExteriorGrade, wind_solids);
        const WindOffRun run = wind_off_run(s, 280.5, WindPace::Drift, true);
        std::cout << "  wind: wind-only drift takeoff=" << run.takeoff_mps << " closing=" << run.closing_mps
                  << " blow=" << run.blow_mps << " share=" << run.wind_share_mps << " wind=" << run.max_wind_mps
                  << " force=" << run.max_wind_force_n << "\n";
        require(run.takeoff_mps <= 1.2, "wind: (2) he only drifts off the edge");
        require(run.max_wind_mps >= 13.0, "wind: (2) a strong gust blows");
        require(run.wind_share_mps >= 0.8, "wind: (2) the gust adds a real share");
        require(std::abs(run.blow_mps - run.wind_share_mps) <= 0.4,
                "wind: (2) the wind's share of the closing is charged as a blow");
        require(run.blow_mps >= run.closing_mps - 0.6, "wind: (2) nearly the full closing is charged");
        require(run.hits == 0, "wind: (2) no hit felt (below the felt speed)");
        require(s.snapshot().death_count == 0, "wind: (2) he lives");
    }

    // (3) Run plus wind: only the run excused. A walk, not a sprint, so the
    // closing stays under the 9 m/s allowance: on the old code the whole
    // closing was excused (blow 0) because the frame velocity is the
    // platform's (zero) and the listener took his whole speed off; on the
    // fixed code only his own drive is excused and the wind's share is charged.
    {
        Simulation s(InitialSpawn::ExteriorGrade, wind_solids);
        const WindOffRun run = wind_off_run(s, 280.5, WindPace::Walk, true);
        std::cout << "  wind: run plus wind takeoff=" << run.takeoff_mps << " closing=" << run.closing_mps
                  << " blow=" << run.blow_mps << " share=" << run.wind_share_mps << " wind=" << run.max_wind_mps
                  << " force=" << run.max_wind_force_n << "\n";
        require(run.takeoff_mps >= 4.0 && run.takeoff_mps <= 6.5, "wind: (3) he walks off the edge");
        require(run.max_wind_mps >= 13.0, "wind: (3) a strong gust blows");
        // The bar is 0.4 here, not 0.8 as in (2): the walk is fast, so the
        // gust has only ~2 s on him before he reaches the wall, and the
        // measured share is 0.53. That is still a real wind contribution
        // (the blow matches it exactly, below), and 0.4 keeps the guard
        // against a vacuous share while the |blow - share| check does the
        // discrimination: on the old code the blow is 0 and it fails.
        require(run.wind_share_mps >= 0.4, "wind: (3) the gust adds a real share");
        require(run.closing_mps < 9.0, "wind: (3) the closing stays under the allowance");
        require(std::abs(run.blow_mps - run.wind_share_mps) <= 0.4,
                "wind: (3) only his run is excused; the wind's share is charged");
        require(run.hits == 0, "wind: (3) no hit felt (below the felt speed)");
        require(s.snapshot().death_count == 0, "wind: (3) he lives");
    }

    // (4) Thrown: no allowance.
    {
        Simulation s(InitialSpawn::ExteriorGrade, calm_solids);
        // 1.5 m east of the wall's east face, at the platform's height, thrown west at 7 m/s.
        (void)s.debug_launch_player({-62.5, 201.0, -147.0}, {-7.0, 0.5, 0.0});
        const bool struck = advance_until(s, [](const Snapshot &n) { return n.hit_count > 0; }, 2.0);
        const Snapshot after = s.snapshot();
        std::cout << "  wind: thrown hit_speed=" << after.hit_speed_mps << " hits=" << after.hit_count << "\n";
        require(struck, "wind: (4) the throw hits the wall");
        require(after.hit_count == 1, "wind: (4) the throw is a blow felt");
        require(std::abs(after.hit_speed_mps - 7.0) <= 0.7, "wind: (4) the throw is charged its full closing");
        require(s.snapshot().death_count == 0, "wind: (4) he lives (below lethal)");
    }

    // (5) Rebound (Codex P1 on #337): a sprint into the west wall,
    // stopped, then steered back east in the air into the east wall — the
    // second hit is his own steering, no blow. On the unfixed code the
    // tracked self-drive still holds the first run after the wall stopped
    // it, so the steering cancels it to ~0 and the second hit is charged
    // in full (a blow past the arm-break speed).
    {
        Simulation s(InitialSpawn::ExteriorGrade, rebound_solids);
        const double lane_z = -147.0;
        (void)s.debug_launch_player({-32.0, 200.5 + 1.5, lane_z}, {0.0, 0.0, 0.0});
        require(advance_until(s, [](const Snapshot &n) { return n.player_grounded; }, 2.0),
                "wind: (5) he lands on the platform");
        (void)s.advance_frame(0.5);
        require(s.snapshot().player_grounded, "wind: (5) he settles on the platform");
        (void)s.set_facing(-1.0, 0.0);
        (void)s.set_move_input(-1.0, 0.0);
        (void)s.set_sprint_input(true);
        const std::uint64_t hits_before = s.snapshot().hit_count;
        double takeoff_mps = 0.0;
        double first_closing = 0.0;
        double second_closing = 0.0;
        double second_blow = 0.0;
        bool first_seen = false;
        bool second_seen = false;
        bool steering = false;
        const std::uint32_t budget_ticks = static_cast<std::uint32_t>(12.0 * 90.0);
        for (std::uint32_t tick = 0; tick < budget_ticks; ++tick) {
            const Snapshot before = s.snapshot();
            const double west_into_tick = -before.player_linear_velocity.x;
            const double east_into_tick = before.player_linear_velocity.x;
            if (before.player_grounded) takeoff_mps = west_into_tick;
            if (!s.advance_frame(Simulation::kFixedStepSeconds).accepted) break;
            const Snapshot after = s.snapshot();
            if (!first_seen) {
                // First wall: the westward collapse, as in wind_off_run.
                if (!after.player_grounded && west_into_tick > 0.5 &&
                    -after.player_linear_velocity.x < 0.5 * west_into_tick &&
                    after.player_position.x <= -63.0) {
                    first_seen = true;
                    first_closing = west_into_tick;
                }
            } else {
                if (!steering) {
                    // Stopped at the wall: steer back east, full stick.
                    steering = true;
                    (void)s.set_facing(1.0, 0.0);
                    (void)s.set_move_input(1.0, 0.0);
                    (void)s.set_sprint_input(false);
                }
                second_blow = std::max(second_blow, after.hit_closing_mps);
                if (!second_seen && east_into_tick > 0.5 &&
                    after.player_linear_velocity.x < 0.5 * east_into_tick &&
                    after.player_position.x >= -58.5) {
                    second_seen = true;
                    second_closing = east_into_tick;
                }
            }
            if (second_seen && horizontal_speed(after) < 1.0) break;
        }
        (void)s.set_move_input(0.0, 0.0);
        const Snapshot end = s.snapshot();
        std::cout << "  wind: rebound takeoff=" << takeoff_mps << " first=" << first_closing
                  << " second=" << second_closing << " blow=" << second_blow
                  << " hits=" << (end.hit_count - hits_before) << " breaks=" << end.arm_break_count << "\n";
        require(first_seen, "wind: (5) the run hits the west wall");
        require(second_seen, "wind: (5) the steer back hits the east wall");
        require(takeoff_mps >= 7.0 && takeoff_mps <= 9.1, "wind: (5) he sprints off the edge");
        require(second_closing >= 8.0, "wind: (5) he steers back to a full run's speed");
        require(second_blow <= 0.4, "wind: (5) his own steering back is no blow");
        require(end.hit_count - hits_before == 0, "wind: (5) no hit felt");
        require(end.arm_break_count == 0, "wind: (5) no arm broken");
        require(end.death_count == 0, "wind: (5) he lives");
    }

    std::cout << "PASS scraperx_sim wind own drive: a gust's push into a wall is a blow; his own run is not, "
                 "steered back or no\n";
}