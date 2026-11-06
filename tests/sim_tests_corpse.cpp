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

// The broken arm (kArmBreakHitSpeed): thrown into a wall a little past a
// sprint's speed, the arm that took the blow is broken; her own run into it,
// flat out or in a jump, is no blow at all (J-048); a ride down the 154 m
// deck's line into the end's buffer too fast tears the hands off and wrenches
// the right arm, and the riser ladder back up from 143 m is the player's
// one-armed haul at half the climb's rate until it heals.
void run_arm_break() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::TraversalState;
    using scraperx::sim::Vector3;
    const auto hz = static_cast<int>(Simulation::kTickRateHz);
    // Sprints along (dx, dz) until a wall stops her (6 s at most), jumping
    // `jump_short` metres short of `wall` (where a sprint stopped) when one
    // is given: the fastest she ran, and whether she was stopped in the air.
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
    // she stood), from 1.5 m out at 9.2 m/s, a little past a sprint, and
    // clear of the floor until she meets it (0.5 m up, past the step-down's
    // reach: thrown, not stepping off): until the blow or a death (1 s).
    const auto throw_in = [](Simulation &s, const Vector3 &wall, const double dx, const double dz) {
        s.debug_launch_player(Vector3{wall.x - 1.5 * dx, wall.y + 0.5, wall.z - 1.5 * dz},
                              Vector3{9.2 * dx, 0.5, 9.2 * dz});
        return advance_until(s, [](const Snapshot &n) { return n.hit_count > 0 || n.death_count > 0; }, 1.0);
    };
    {
        // Head-on into the proving ground's wall-run wall from 27 m back:
        // flat out, then in a jump at that speed, her own run is no blow.
        Simulation s(InitialSpawn::WallRunApproach);
        require(s.advance_frame(0.5).accepted, "arm: settle");
        require(walk_to(s, 2.0, 0.0, 12.0, 0.1), "arm: 27 m back from the wall");
        const RunIn ran = run_in(s, 0.0, -1.0);
        const Snapshot met = s.snapshot();
        std::cout << "  arm: ran in at " << ran.fastest << " m/s; felt hits " << met.hit_count << ", breaks "
                  << met.arm_break_count << ", deaths " << met.death_count << "\n";
        require(ran.stopped && ran.fastest >= 8.5, "arm: flat out into the wall (setup)");
        require(met.hit_count == 0 && met.arm_break_count == 0 && met.death_count == 0,
                "arm: her own run flat out into the wall is no blow");
        const Vector3 wall = met.player_position;
        require(walk_to(s, 2.0, 0.0, 12.0, 0.1), "arm: 27 m back again");
        const RunIn leapt = run_in(s, 0.0, -1.0, &wall, 2.2);
        const Snapshot leap = s.snapshot();
        std::cout << "  arm: jumped in at " << leapt.fastest << " m/s, met in the air " << leapt.in_air
                  << "; felt hits " << leap.hit_count << ", breaks " << leap.arm_break_count << ", deaths "
                  << leap.death_count << "\n";
        require(leapt.stopped && leapt.in_air && leapt.fastest >= 8.5, "arm: a jump at that speed into the wall (setup)");
        require(leap.hit_count == 0 && leap.arm_break_count == 0 && leap.death_count == 0,
                "arm: her own jump into the wall is no blow");
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
                "arm: her own run flat out into the locker is no blow");
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
    std::cout << "PASS scraperx_sim arm break: her own run is no blow, flat out or jumped; a throw and a wrench break an"
                 " arm; one arm climbs at half rate, holds half the grip, reaches no bar\n";
}
