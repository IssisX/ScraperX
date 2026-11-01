// The Matron finds her own way between levels on the tower (the climbing lab,
// src/sim/matron_found_climb.cpp): no table of ladders. She hears the climber
// on another level, finds the bars round her in the world, walks to a column
// that runs toward that level, and climbs it by routecraft's hands (finite
// force, gravity on). Gravity stays 1 on her every tick: the old authored
// climb (gravity off) never runs. Up and down are separate runs.

#include "sim_tests.hpp"
#include "matron_contact_audit.hpp"
#include "sim/matron_traversal.hpp"

#include <cmath>
#include <iostream>

namespace {
using scraperx::sim::InitialSpawn;
using scraperx::sim::Simulation;
using scraperx::sim::Vector3;

struct Leg {
    bool arrived = false;
    bool alive = true;
    bool gravity_kept = true;
    bool her_hands = false;   // routecraft seen moving her (the old climb sets gravity off instead)
    double seconds = 0.0;
    double worst_step = 0.0;
    std::uint32_t climbs = 0;
    bool let_go = false;       // her hands opened on a climb (as a hold giving way opens them)
    double fell_mps = 0.0;     // her fastest fall after
    double let_go_s = 0.0;
    // Her exported contacts (S6): checked every tick; the ticks from the let
    // go to her hand's grip gone; the fall's ticks with no floor and no grip
    // exported; on arrival, her floor exported at the climber's level.
    MatronContactAudit contacts;
    int grip_gone_ticks = -1;
    int fall_free_ticks = 0;
    bool arrived_on_floor = false;
};

// Her at `from` (feet), dangerous; the climber heard at `prey` (feet) every
// 8 s. Until she stands a second on the prey's floor.
Leg leg(const Vector3 from, const Vector3 prey, const double seconds, const double let_go_at = 1.0e9) {
    Simulation s(InitialSpawn::Deck154);
    s.place_matron_home(154.0F);
    require(s.advance_frame(1.0).accepted, "matron found climb: settle");
    s.debug_launch_player(Vector3{prey.x, prey.y + 0.3, prey.z}, Vector3{0.0, 0.0, 0.0});
    s.launch_matron_for_test(Vector3{from.x, from.y + 1.05, from.z});
    // The corner pipe ruled out: the way she takes is one she finds.
    s.debug_matron_rule_out_pipe();
    s.debug_set_matron_stage(1);
    const int hz = static_cast<int>(Simulation::kTickRateHz);
    (void)s.advance_frame(Simulation::kFixedStepSeconds);
    Leg out;
    auto last = s.matron_state().position;
    const auto climbs0 = s.matron_state().climbs;
    int on_goal = 0;
    int let_go_tick = -1;
    for (int i = 0; i < static_cast<int>(seconds) * hz; ++i) {
        if (i % (8 * hz) == 0) {
            s.debug_matron_hear(prey);
        }
        (void)s.set_move_input(0.0, 0.0);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const auto m = s.matron_state();
        out.worst_step = std::max(out.worst_step, std::hypot(m.position.x - last.x, m.position.y - last.y,
                                                             m.position.z - last.z));
        const double vy = (m.position.y - last.y) * hz;
        last = m.position;
        out.contacts.tick(m);
        if (!out.let_go && m.climbing && m.position.y - 1.05 >= let_go_at) {
            s.debug_matron_let_go();
            out.let_go = true;
            out.let_go_s = static_cast<double>(i) / hz;
            let_go_tick = i;
        } else if (out.let_go) {
            out.fell_mps = std::max(out.fell_mps, -vy);
            if (out.grip_gone_ticks < 0 && !m.contact_hand_held) out.grip_gone_ticks = i - let_go_tick;
            if (vy < -3.0 && !m.contact_supported && !m.contact_hand_held) ++out.fall_free_ticks;
        }
        out.gravity_kept = out.gravity_kept && s.debug_matron_gravity_factor() == 1.0;
        out.her_hands = out.her_hands || m.climbing;
        if (!m.alive) {
            out.alive = false;
            break;
        }
        const bool there = std::abs(m.position.y - 1.05 - prey.y) < 0.35 && !m.climbing;
        on_goal = there ? on_goal + 1 : 0;
        if (on_goal >= hz) {
            out.arrived = true;
            out.seconds = static_cast<double>(i) / hz;
            out.arrived_on_floor = m.contact_supported && std::abs(m.contact_feet.y - prey.y) < 0.35;
            break;
        }
    }
    out.climbs = s.matron_state().climbs - climbs0;
    return out;
}

void report(const char *name, const Leg &l) {
    std::cout << "  matron found climb: " << name << ": arrived " << l.arrived << " in " << l.seconds << " s, climbs "
              << l.climbs << ", worst step " << l.worst_step << " m, gravity kept " << l.gravity_kept
              << ", by her hands " << l.her_hands << ", alive " << l.alive << '\n';
    l.contacts.report(name);
}

// S6: what the presentation plants her limbs on is hers, every tick, and on
// arrival she stands on the climber's floor by her own exported contact.
void require_contacts(const Leg &l) {
    require(l.contacts.clean(), "matron found climb: her exported floor and grip are hers every tick (S6)");
    require(!l.arrived || l.arrived_on_floor, "matron found climb: arrived, her exported floor is the climber's level (S6)");
}

} // namespace

void run_matron_found_climb() {
    {
        const Leg l = leg(Vector3{1.0, 154.0, -129.0}, Vector3{5.0, 176.25, -127.5}, 120.0);
        report("154 up to the 176.25 ring", l);
        require_contacts(l);
        require(l.alive && l.arrived, "matron found climb: up to the climber's level, a way she found herself");
        require(l.gravity_kept && l.her_hands, "matron found climb: by her own hands, gravity on (not the old climb)");
        require(l.worst_step < 0.2, "matron found climb: never carried or snapped");
    }
    {
        const Leg l = leg(Vector3{5.0, 176.25, -127.5}, Vector3{1.0, 154.0, -129.0}, 120.0);
        report("the 176.25 ring down to 154", l);
        require_contacts(l);
        require(l.alive && l.arrived, "matron found climb: down to the climber's level, a way she found herself");
        require(l.gravity_kept && l.her_hands, "matron found climb: by her own hands, gravity on (not the old climb)");
        require(l.worst_step < 0.2, "matron found climb: never carried or snapped");
    }
    // Above her map's last deck: the standpipe (a 0.10 m pole: the traversal
    // Climber's friction grip) and the scaffold panel (bars).
    {
        const Leg l = leg(Vector3{7.05, 176.25, -128.4}, Vector3{8.0, 198.25, -129.3}, 240.0);
        report("176.25 up to 198.25 (the standpipe)", l);
        require_contacts(l);
        require(l.alive && l.arrived, "matron found climb: up the standpipe to the climber's level");
        require(l.gravity_kept && l.her_hands, "matron found climb: by her own hands, gravity on");
        require(l.worst_step < 0.2, "matron found climb: never carried or snapped");
    }
    {
        const Leg l = leg(Vector3{8.0, 198.25, -129.3}, Vector3{7.05, 176.25, -128.4}, 240.0);
        report("198.25 down to 176.25 (the standpipe)", l);
        require_contacts(l);
        require(l.alive && l.arrived, "matron found climb: down the standpipe to the climber's level");
        require(l.gravity_kept && l.her_hands, "matron found climb: by her own hands, gravity on");
        require(l.worst_step < 0.2, "matron found climb: never carried or snapped");
    }
    {
        const Leg l = leg(Vector3{8.0, 198.25, -129.3}, Vector3{11.0, 220.25, -129.0}, 240.0);
        report("198.25 up to 220.25 (the scaffold panel)", l);
        require_contacts(l);
        require(l.alive && l.arrived, "matron found climb: up the panel to the climber's level");
        require(l.gravity_kept && l.her_hands, "matron found climb: by her own hands, gravity on");
        require(l.worst_step < 0.2, "matron found climb: never carried or snapped");
    }
    {
        const Leg l = leg(Vector3{11.0, 220.25, -129.0}, Vector3{8.0, 198.25, -129.3}, 240.0);
        report("220.25 down to 198.25 (the scaffold panel)", l);
        require_contacts(l);
        require(l.alive && l.arrived, "matron found climb: down the panel to the climber's level");
        require(l.gravity_kept && l.her_hands, "matron found climb: by her own hands, gravity on");
        require(l.worst_step < 0.2, "matron found climb: never carried or snapped");
    }
    // The owner's chain, knocked off mid-way: up the ladder (bars, by
    // routecraft) and on up the standpipe (a pole, by the Climber's grip),
    // her hands opened 9 m up the pipe. She falls for real, lands alive,
    // and plans again from where she lies until she reaches the climber.
    {
        const Leg l = leg(Vector3{1.0, 154.0, -129.0}, Vector3{8.0, 198.25, -129.3}, 420.0, 185.25);
        report("154 to 198.25, ladder then standpipe, knocked off the pipe", l);
        require_contacts(l);
        std::cout << "    let go at " << l.let_go_s << " s, fell at " << l.fell_mps << " m/s\n";
        require(l.let_go && l.fell_mps > 5.0 && l.alive, "matron found climb: knocked off mid-chain, she falls for real and lives");
        std::cout << "    her grip gone " << l.grip_gone_ticks << " ticks after the let go; " << l.fall_free_ticks
                  << " ticks of the fall with no floor and no grip exported\n";
        require(l.grip_gone_ticks >= 0 && l.grip_gone_ticks <= 3, "matron found climb: her exported grip goes within 3 ticks of her hands opening (S6)");
        require(l.fall_free_ticks > 0, "matron found climb: her fall exported as a fall, no floor and no grip under her (S6)");
        require(l.arrived, "matron found climb: and plans again from where she landed, and reaches the climber");
        require(l.gravity_kept && l.worst_step < 0.2, "matron found climb: gravity on, never carried or snapped");
    }
    // A failed column closes that column, not the journey: with every
    // column of her cheapest way blocked (re-blocked as the blocks tick
    // down, so the failure stays live), she still takes the next route
    // the planner offers instead of standing out the block. The hop is
    // a 1.39 m lip from open ground onto a platform: too high to walk
    // (the 0.43 m lip tried first let her walk up it, climbs 0 throughout),
    // served in parallel by a mantle and by a mantle-plus-gap-jump route,
    // with no bar column in her 14 m search box spanning it (find_climb
    // needs a 2 m column), so no bar climb can rescue the test either
    // way; only the journey's plan decides.
    {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        require(s.advance_frame(1.0).accepted, "matron found climb: settle");
        const Vector3 from{0.0, 0.0, -60.0};
        const Vector3 prey{-8.3, 1.3, -115.0};
        s.debug_launch_player(Vector3{prey.x, prey.y + 0.3, prey.z}, Vector3{0.0, 0.0, 0.0});
        s.launch_matron_for_test(Vector3{from.x, from.y + 1.05, from.z});
        s.debug_matron_rule_out_pipe();
        s.debug_set_matron_stage(1);
        (void)s.advance_frame(Simulation::kFixedStepSeconds);
        const scraperx::sim::traversal::AffordanceMap &map = s.matron_affordances();
        const int home = map.region_at(JPH::Vec3(float(from.x), float(from.y), float(from.z)));
        const int goal = map.region_at(JPH::Vec3(float(prey.x), float(prey.y), float(prey.z)));
        require(home >= 0 && goal >= 0, "matron found climb: blocked-column staging, both floors on her scan");
        const std::vector<int> cheapest = map.plan(home, goal);
        require(!cheapest.empty(), "matron found climb: blocked-column staging, a cheapest way exists");
        std::vector<std::pair<float, float>> blocked;
        for (const int e : cheapest) {
            const scraperx::sim::traversal::Edge &edge = map.edges()[static_cast<std::size_t>(e)];
            if (edge.kind == scraperx::sim::traversal::MoveKind::Walk) continue;
            const std::pair<float, float> line{edge.spec.line.GetX(), edge.spec.line.GetZ()};
            if (std::find(blocked.begin(), blocked.end(), line) == blocked.end()) blocked.push_back(line);
            std::cout << "  matron found climb: cheapest edge " << e << " kind " << int(edge.kind) << " line ("
                      << line.first << ", " << line.second << ")\n";
        }
        require(!blocked.empty() && blocked.size() <= 8,
                "matron found climb: blocked-column staging, the cheapest way's columns fit her block list");
        // A second route past every blocked column, close enough for her
        // journey's own scan (12 m round her and the heard spot).
        std::vector<int> avoid;
        for (std::size_t i = 0; i < map.edges().size(); ++i) {
            const scraperx::sim::traversal::Edge &edge = map.edges()[i];
            if (edge.kind == scraperx::sim::traversal::MoveKind::Walk) continue;
            for (const auto &line : blocked) {
                if (std::abs(edge.spec.line.GetX() - line.first) <= 1.5F &&
                    std::abs(edge.spec.line.GetZ() - line.second) <= 1.5F) {
                    avoid.push_back(static_cast<int>(i));
                    break;
                }
            }
        }
        const std::vector<int> second = map.plan(home, goal, avoid);
        for (const int e : second) {
            const scraperx::sim::traversal::Edge &edge = map.edges()[static_cast<std::size_t>(e)];
            std::cout << "  matron found climb: second-route edge " << e << " kind " << int(edge.kind) << " line ("
                      << edge.spec.line.GetX() << ", " << edge.spec.line.GetZ() << ")\n";
        }
        require(!second.empty(),
                "matron found climb: blocked-column staging, a second route exists past the blocked columns");
        const float lo_x = std::min(float(from.x), float(prey.x)) - 12.0F;
        const float hi_x = std::max(float(from.x), float(prey.x)) + 12.0F;
        const float lo_z = std::min(float(from.z), float(prey.z)) - 12.0F;
        const float hi_z = std::max(float(from.z), float(prey.z)) + 12.0F;
        for (const int e : second) {
            const scraperx::sim::traversal::Edge &edge = map.edges()[static_cast<std::size_t>(e)];
            if (edge.kind == scraperx::sim::traversal::MoveKind::Walk) continue;
            require(edge.spec.line.GetX() >= lo_x && edge.spec.line.GetX() <= hi_x &&
                        edge.spec.line.GetZ() >= lo_z && edge.spec.line.GetZ() <= hi_z,
                    "matron found climb: blocked-column staging, the second route stays inside her journey's scan");
        }
        const auto block_all = [&] {
            for (const auto &line : blocked) s.debug_matron_block_column(line.first, line.second);
        };
        block_all();
        const int hz = static_cast<int>(Simulation::kTickRateHz);
        bool arrived = false, alive = true, gravity_kept = true;
        int on_goal = 0;
        for (int i = 0; i < 420 * hz && !arrived; ++i) {
            if (i % (8 * hz) == 0) {
                s.debug_matron_hear(prey);
                block_all();
            }
            (void)s.set_move_input(0.0, 0.0);
            (void)s.advance_frame(Simulation::kFixedStepSeconds);
            const auto m = s.matron_state();
            gravity_kept = gravity_kept && s.debug_matron_gravity_factor() == 1.0;
            if (!m.alive) {
                alive = false;
                break;
            }
            if (i % (5 * hz) == 0) {
                std::cout << "  matron found climb: blocked columns at " << (i / hz) << " s, feet ("
                          << m.position.x << "," << (m.position.y - 1.05) << "," << m.position.z << "), climbing "
                          << m.climbing << ", climbs " << m.climbs << '\n';
            }
            const bool there = std::abs(m.position.y - 1.05 - prey.y) < 0.35 && !m.climbing;
            on_goal = there ? on_goal + 1 : 0;
            if (on_goal >= hz) arrived = true;
        }
        {
            const auto m = s.matron_state();
            std::cout << "  matron found climb: blocked columns, arrived " << arrived << ", alive " << alive
                      << ", final feet (" << m.position.x << "," << (m.position.y - 1.05) << "," << m.position.z
                      << "), climbs " << m.climbs << ", climbing " << m.climbing << '\n';
        }
        require(alive && gravity_kept, "matron found climb: blocked columns, she lives with gravity on");
        require(arrived, "matron found climb: a failed column closes that column, not the journey");
    }
    std::cout << "PASS scraperx_sim matron found climb\n";
}
