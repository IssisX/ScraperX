#include "sim_tests.hpp"
#include "sim/twin_tower.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace {
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;
using scraperx::sim::TraversalState;
using scraperx::sim::TwinTower;

bool trace() {
    return std::getenv("SCRAPERX_TWIN_TRACE") != nullptr;
}
void stand(Simulation &s) {
    require(s.set_move_input(0, 0) && s.advance_frame(0.7).accepted, "twin: stand");
}
void go(Simulation &s, double x, double z) {
    const bool reached = walk_to(s, x, z, 35.0, 0.18);
    const auto n = s.snapshot();
    std::cout << "  twin target " << x << ',' << z << " at " << n.player_position.x << ',' << n.player_position.y << ','
              << n.player_position.z << " support=" << n.support_entity_id << '\n';
    require(reached, "twin: walk target reached using movement input");
    require(n.death_count == 0, "twin: route must not recover by death");
}
// The leaf's turn from as built (raised), about +x, radians.
double leaf_turn(const Simulation &s) {
    const auto q = s.kit_body_rotation(s.kit_body_index(TwinTower::kLeafEntity));
    return 2.0 * std::atan2(q.x, q.w) - TwinTower::kLeafLeanRad;
}
bool leaf_down(const Simulation &s) {
    return leaf_turn(s) > 0.5 * 3.14159265358979 - TwinTower::kLeafLeanRad - 0.05;
}
// Aims at a point the way a thumb does: at it, raised by the shot's drop
// over the distance (the hook leaves at 40 m/s and falls under gravity).
void aim_at(Simulation &s, double x, double y, double z) {
    const auto p = s.snapshot().player_position;
    const double dx = x - p.x;
    const double dz = z - p.z;
    const double from_y = p.y + 0.6;
    const double flight = std::hypot(dx, y - from_y, dz) / 40.0;
    const double drop = 0.5 * 9.81 * flight * flight;
    require(s.set_facing(dx, dz) && s.set_hook_aim(dx, y + drop - from_y, dz), "twin: aim the hook");
}
// Fires at a mark and waits for the bite.
bool fire_and_bite(Simulation &s, double x, double y, double z) {
    aim_at(s, x, y, z);
    require(s.request_hook_fire(), "twin: fire the hook");
    return advance_until(s, [](const Snapshot &n) { return n.hook_state == 2 || n.hook_state == 0; }, 2.0) &&
           s.snapshot().hook_state == 2;
}
// The crossing: bitten on the arm's bar over the gap's middle, step off the
// tip toward it with the stick held that way; the line swings the body
// across, to the other tip, where the swing spends itself. Up from a hang
// if the hands took the lip; then let the line go.
// The line's pull is judged as the bite is: what it holds is the jolt over
// its 6 kN rating summed through each run of ticks over it (it tears past
// 500 N s), not one tick's spike as the line snaps taut.
bool swing_over(Simulation &s, double dir_z, double land_z_min, double land_z_max, double &peak_jolt) {
    peak_jolt = 0.0;
    double jolt = 0.0;
    bool let_go = false;
    int ticks = 0;
    bool done = advance_until(s, [&](const Snapshot &n) {
        (void)s.set_move_input(0.0, let_go ? 0.0 : dir_z);
        jolt = n.hook_tension_n > 6000.0 ? jolt + (n.hook_tension_n - 6000.0) / Simulation::kTickRateHz : 0.0;
        peak_jolt = std::max(peak_jolt, jolt);
        if (trace() && ++ticks % 20 == 0) {
            std::cout << "    swing p=(" << n.player_position.x << ',' << n.player_position.y << ',' << n.player_position.z
                      << ") v=(" << n.player_linear_velocity.x << ',' << n.player_linear_velocity.y << ','
                      << n.player_linear_velocity.z << ") line=" << n.hook_line_length << " T=" << n.hook_tension_n
                      << " trav=" << int(n.traversal_state) << " g=" << n.player_grounded << '\n';
        }
        // The top of the swing over the landing deck: let go there.
        const bool over = n.player_position.z >= land_z_min && n.player_position.z <= land_z_max;
        if (over && n.hook_state == 2 && n.player_linear_velocity.y <= 0.0 && n.player_position.y > TwinTower::kDeckTop + 1.2) {
            (void)s.request_hook_release();
            let_go = true;
        }
        return n.death_count > 0 || n.traversal_state == TraversalState::Hanging ||
               (n.player_grounded && over && n.player_position.y > TwinTower::kDeckTop);
    }, 20.0);
    (void)s.set_move_input(0.0, 0.0);
    if (!done || s.snapshot().death_count > 0) {
        return false;
    }
    if (s.snapshot().traversal_state == TraversalState::Hanging) {
        require(s.request_hook_release(), "twin: let the line go from the hang");
        require(s.request_up(), "twin: climb up from the hang");
        if (!advance_until(s, [](const Snapshot &n) { return n.player_grounded && n.traversal_state == TraversalState::None; },
                           4.0)) {
            return false;
        }
    } else if (s.snapshot().hook_state != 0) {
        require(s.request_hook_release(), "twin: let the line go");
    }
    stand(s);
    const auto n = s.snapshot();
    std::cout << "  twin swung to " << n.player_position.x << ',' << n.player_position.y << ',' << n.player_position.z
              << " peak jolt over the hold " << peak_jolt << " N s, deaths " << n.death_count << '\n';
    return n.player_grounded && n.death_count == 0 && n.player_position.z >= land_z_min &&
           n.player_position.z <= land_z_max && n.player_position.y > TwinTower::kDeckTop;
}
}

void run_twin() {
    Simulation s(scraperx::sim::InitialSpawn::Ring220North);
    stand(s);
    for (const auto entity : {TwinTower::kFrameEntity, TwinTower::kLeafEntity, TwinTower::kFlagEntity}) {
        unsigned matching = 0;
        for (std::uint32_t i = 0; i < s.kit_body_count(); ++i)
            matching += s.kit_body_entity(i) == entity ? 1U : 0U;
        require(matching == 1, "twin: the bridge's frame, leaf and flag each own a unique entity");
    }
    require(leaf_turn(s) < 0.02, "twin: the leaf stands raised as built");
    go(s, -21.6, -127.6);
    require(s.request_hook_take() && s.advance_frame(Simulation::kFixedStepSeconds).accepted && s.snapshot().has_hook,
            "twin: take existing hook on A");
    go(s, -21.6, -171.5);
    go(s, -7.0, -172.5);
    go(s, TwinTower::kSwingX, -180.0);
    go(s, TwinTower::kSwingX, TwinTower::kNearTipZ + 0.45);
    stand(s);
    require(s.snapshot().player_grounded && std::abs(s.snapshot().player_position.y - 221.15) < 0.1,
            "twin: stand at the near tip's east lane");
    const auto near_tip_deck = s.save_game();
    const double near_step_x = TwinTower::kNearStepWestX + 0.5;
    go(s, near_step_x, TwinTower::kNearTipZ + 1.9);
    require(s.set_facing(0, -1) && s.request_traversal(), "twin: mantle onto the near step");
    require(advance_until(s, [](const Snapshot &n) {
        return n.player_grounded && n.player_position.y > TwinTower::kDeckTop + TwinTower::kStepHeight + 0.5;
    }, 3.0), "twin: up on the near step");
    go(s, near_step_x, TwinTower::kNearTipZ + 0.5);
    stand(s);
    const auto near_tip = s.save_game();

    // A walk cannot cross: straight off the tip, the gap has no floor.
    {
        Simulation walker(scraperx::sim::InitialSpawn::Ring220North);
        require(walker.load_game(near_tip_deck.data(), near_tip_deck.size()), "twin: reload at the near tip");
        stand(walker);
        require(advance_until(walker, [&](const Snapshot &n) {
            (void)walker.set_move_input(0.0, -1.0);
            return n.player_position.y < 214.0;
        }, 6.0), "twin: a walk off the near tip falls into the gap");
        std::cout << "PASS scraperx_sim twin gap: a walk off the near tip falls\n";
    }
    // The line's swing is no run of his (J-048): bitten on the arm's bar from
    // the deck at the near tip (1.2 m under the step, so the arc meets the
    // far tip's face, not its edge) and run off it, what the line's swing
    // brings him into the far face with is a blow felt (kHitFeltSpeed, 6 m/s).
    {
        Simulation swinger(scraperx::sim::InitialSpawn::Ring220North);
        require(swinger.load_game(near_tip_deck.data(), near_tip_deck.size()), "twin: reload at the near tip's deck");
        stand(swinger);
        require(fire_and_bite(swinger, near_step_x, TwinTower::kArmY - 0.06, TwinTower::kArmTipZ - 0.06),
                "twin: the hook bites the arm's bar from the near tip's deck");
        const auto hits = swinger.snapshot().hit_count;
        (void)advance_until(swinger, [&](const Snapshot &n) {
            (void)swinger.set_move_input(0.0, -1.0);
            (void)swinger.set_sprint_input(true);
            return n.hit_count != hits || n.death_count > 0;
        }, 6.0);
        (void)swinger.set_move_input(0.0, 0.0);
        (void)swinger.set_sprint_input(false);
        const Snapshot n = swinger.snapshot();
        std::cout << "  twin swing hit: at " << n.player_position.x << ',' << n.player_position.y << ','
                  << n.player_position.z << " hits " << n.hit_count - hits << " at " << n.hit_speed_mps
                  << " m/s, hook " << int(n.hook_state) << ", deaths " << n.death_count << '\n';
        require(n.hit_count == hits + 1 && n.hit_speed_mps >= 6.0 && n.death_count == 0 && n.hook_state == 2 &&
                    n.player_position.z < TwinTower::kFarTipZ + 0.5,
                "twin: the line's swing into the far tip's face is a blow felt, not his own run");
        std::cout << "PASS scraperx_sim twin swing hit: the line's swing into the far face is felt at "
                  << n.hit_speed_mps << " m/s\n";
    }
    // The flag cannot be reached from tower one's side: a shot from the
    // near tip at it meets the raised leaf or the hood's back.
    {
        Simulation shooter(scraperx::sim::InitialSpawn::Ring220North);
        require(shooter.load_game(near_tip.data(), near_tip.size()), "twin: reload at the near tip for the trip");
        stand(shooter);
        aim_at(shooter, TwinTower::kFlagX, TwinTower::kFlagPivotY - TwinTower::kFlagHalfHeight, TwinTower::kFlagZ);
        require(shooter.request_hook_fire(), "twin: fire at the trip from tower one");
        (void)advance_until(shooter, [](const Snapshot &) { return false; }, 4.0);
        require(leaf_turn(shooter) < 0.02, "twin: the trip cannot be thrown from tower one's side");
        std::cout << "PASS scraperx_sim twin hood: the trip cannot be thrown from tower one\n";
    }

    // The hook over: the arm's bar over the gap's middle.
    require(fire_and_bite(s, near_step_x, TwinTower::kArmY - 0.06, TwinTower::kArmTipZ - 0.06),
            "twin: the hook bites the arm's bar from the near step");
    double peak = 0.0;
    require(swing_over(s, -1.0, -215.5, TwinTower::kFarTipZ, peak),
            "twin: swung over and let go onto the far half");
    require(peak < 500.0, "twin: the line's jolt stays under what tears the bite");
    require(leaf_turn(s) < 0.02, "twin: crossing does not bring the leaf down");
    std::cout << "PASS scraperx_sim twin hook over: near step -> far half, no deaths, jolt over the hold " << peak
              << " N s\n";
    const auto far_tip = s.save_game();

    // The hook back, from the far tip's west corner to the near tip's east
    // nosing.
    {
        Simulation back(scraperx::sim::InitialSpawn::Ring220North);
        require(back.load_game(far_tip.data(), far_tip.size()), "twin: reload on the far tip");
        stand(back);
        const double far_step_x = TwinTower::kFarStepWestX + 0.5;
        go(back, far_step_x, TwinTower::kFarTipZ - 1.9);
        require(back.set_facing(0, 1) && back.request_traversal(), "twin: mantle onto the far step");
        require(advance_until(back, [](const Snapshot &n) {
            return n.player_grounded && n.player_position.y > TwinTower::kDeckTop + TwinTower::kStepHeight + 0.5;
        }, 3.0), "twin: up on the far step");
        go(back, far_step_x, TwinTower::kFarTipZ - 0.5);
        stand(back);
        require(fire_and_bite(back, far_step_x, TwinTower::kArmY - 0.06, TwinTower::kArmTipZ - 0.06),
                "twin: the hook bites the arm's bar from the far step");
        double back_peak = 0.0;
        require(swing_over(back, 1.0, TwinTower::kNearTipZ, -173.0, back_peak),
                "twin: swung back and let go onto the near half");
        require(back_peak < 500.0, "twin: the line's jolt back stays under what tears the bite");
        std::cout << "PASS scraperx_sim twin hook back: far step -> near half, no deaths, jolt over the hold "
                  << back_peak << " N s\n";
    }

    // Past the raised leaf to the shaft's side, then the trip.
    go(s, TwinTower::kSwingX, -214.0);
    go(s, TwinTower::kFlagX, -217.5);
    stand(s);
    require(leaf_turn(s) < 0.02, "twin: the leaf still stands");
    aim_at(s, TwinTower::kFlagX, TwinTower::kFlagPivotY - TwinTower::kFlagHalfHeight, TwinTower::kFlagZ);
    require(s.request_hook_fire(), "twin: shot at the trip from the shaft's side");
    double came_down = -1.0;
    int ticks = 0;
    require(advance_until(s, [&](const Snapshot &) {
        ++ticks;
        if (trace() && ticks % 10 == 0 && ticks < 300) {
            const auto n = s.snapshot();
            const auto fq = s.kit_body_rotation(s.kit_body_index(TwinTower::kFlagEntity));
            std::cout << "    leaf turn " << leaf_turn(s) << " flag " << 2.0 * std::atan2(-fq.x, fq.w) << " hook "
                      << int(n.hook_state) << " at (" << n.hook_position.x << ',' << n.hook_position.y << ','
                      << n.hook_position.z << ")\n";
        }
        if (leaf_down(s) && came_down < 0.0)
            came_down = double(ticks) / Simulation::kTickRateHz;
        return came_down > 0.0;
    }, 30.0), "twin: the trip lets the leaf go and it comes down across the gap");
    if (s.snapshot().hook_state != 0)
        (void)s.request_hook_release();
    std::cout << "  twin leaf down in " << came_down << " s\n";
    const auto tip = s.kit_body_position(s.kit_body_index(TwinTower::kLeafEntity));
    (void)tip;

    // Walk over the leaf to tower one and back.
    // Past the trip's post (it stands in the leaf's lane, behind the hinge).
    const double by_post_x = TwinTower::kLeafEastX - 0.3;
    go(s, by_post_x, -214.0);
    go(s, by_post_x, TwinTower::kHingeZ - 1.0);
    go(s, TwinTower::kFlagX, TwinTower::kHingeZ + 1.0);
    go(s, TwinTower::kFlagX, TwinTower::kNearTipZ + 2.0);
    stand(s);
    require(s.snapshot().player_grounded && s.snapshot().player_position.z > TwinTower::kNearTipZ,
            "twin: walked over the leaf onto the near half");
    go(s, TwinTower::kFlagX, TwinTower::kHingeZ + 1.0);
    go(s, by_post_x, TwinTower::kHingeZ - 1.0);
    stand(s);
    require(s.snapshot().player_grounded && s.snapshot().player_position.z < TwinTower::kFarTipZ &&
                s.snapshot().death_count == 0,
            "twin: walked back over the leaf to the shaft's side");
    std::cout << "PASS scraperx_sim twin leaf: hook shot from the shaft's side brings it down in " << came_down
              << " s; walked over both ways\n";

    // For good: saved and loaded, the leaf is still down and still walked.
    const auto saved = s.save_game();
    require(!saved.empty(), "twin: saves with the leaf down");
    Simulation restored(scraperx::sim::InitialSpawn::Ring220North);
    require(restored.load_game(saved.data(), saved.size()), "twin: reloads with the leaf down");
    stand(restored);
    require(leaf_down(restored), "twin: the leaf stays down after a load");
    require(restored.snapshot().has_hook, "twin: the hook is kept");
    go(restored, TwinTower::kFlagX, TwinTower::kNearTipZ + 2.0);
    stand(restored);
    require(restored.snapshot().player_grounded && restored.snapshot().death_count == 0,
            "twin: after a load the leaf still carries the walk to tower one");
    std::cout << "PASS scraperx_sim twin save: the leaf stays down through save and load\n";
}
SCRAPERX_GROUP("twin", 70, run_twin);
SCRAPERX_SELECT("twin", run_twin, run_twin_balconies, run_twin_pitch);

void run_twin_geometry() {
    Simulation s;
    Simulation::LookHit hit;
    require(s.look_target(-5, 1200, -200, 0, 0, -1, 40, hit) && std::abs(hit.point.z + 224.0) < 0.001,
            "twin: native shaft face is z=-224 (48m nominal gap)");
    require(!s.look_target(-30, 1200, -380, 0, 0, 1, 60, hit),
            "twin: no stale exported mass at old position");
    require(s.look_target(-30, 1200, -340, 0, 0, 1, 40, hit) && std::abs(hit.point.z + 304.0) < 0.001,
            "twin: back of relocated solid shaft is z=-304");
    require(s.snapshot().world_solid_rejected == 0, "twin: all exported collision is accepted");
    std::cout << "PASS scraperx_sim twin geometry: face=-224 back=-304 no ghost mass\n";

    // From the ground, nothing reaches the shaft. Every surface a body can
    // stand on (normal y >= 0.55) in the void and round the shaft, from the
    // ground to the bridge, found by stacked rays straight down on a 1 m
    // grid; then joined, from the ground up, wherever a surface is within
    // 6 m across and 3.8 m up (a hang's reach) of one already reached. The
    // highest reached must leave the hook's 30 m short of the bridge.
    struct Spot {
        int ix;
        int iz;
        double y;
    };
    std::vector<Spot> spots;
    const double x0 = -90.0;
    const double z0 = -320.0;
    const int nx = 120;
    const int nz = 146;
    for (int ix = 0; ix <= nx; ++ix) {
        for (int iz = 0; iz <= nz; ++iz) {
            const double x = x0 + ix;
            const double z = z0 + iz;
            double from = 214.0;
            for (int k = 0; k < 64 && from > -1.0; ++k) {
                Simulation::LookHit down;
                if (!s.look_target(x, from, z, 0, -1, 0, from + 1.0, down))
                    break;
                if (down.normal.y >= 0.55)
                    spots.push_back({ix, iz, down.point.y});
                from = down.point.y - 0.05;
            }
        }
    }
    std::map<int, std::vector<std::size_t>> by_cell;
    for (std::size_t i = 0; i < spots.size(); ++i)
        by_cell[spots[i].ix * 1000 + spots[i].iz].push_back(i);
    std::vector<char> reached(spots.size(), 0);
    std::vector<std::size_t> frontier;
    for (std::size_t i = 0; i < spots.size(); ++i) {
        if (spots[i].y < 1.0) {
            reached[i] = 1;
            frontier.push_back(i);
        }
    }
    double highest = 0.0;
    while (!frontier.empty()) {
        const Spot from = spots[frontier.back()];
        frontier.pop_back();
        highest = std::max(highest, from.y);
        for (int dx = -6; dx <= 6; ++dx) {
            for (int dz = -6; dz <= 6; ++dz) {
                if (dx * dx + dz * dz > 36)
                    continue;
                const auto cell = by_cell.find((from.ix + dx) * 1000 + from.iz + dz);
                if (cell == by_cell.end())
                    continue;
                for (const std::size_t j : cell->second) {
                    if (!reached[j] && spots[j].y - from.y <= 3.8) {
                        reached[j] = 1;
                        frontier.push_back(j);
                    }
                }
            }
        }
    }
    std::cout << "  twin ground: " << spots.size() << " standable surfaces, highest reached from the ground "
              << highest << " m\n";
    require(highest + 30.0 < 214.0, "twin: from the ground, the hook's reach stays short of the bridge");
    std::cout << "PASS scraperx_sim twin ground: highest reachable " << highest << " m, the bridge at 220 m out of reach\n";
}
SCRAPERX_GROUP("twin_geometry", 60, run_twin_geometry);
SCRAPERX_SELECT("twin-geometry", run_twin_geometry);

// J-053, the Void Crossing balconies (Claude 2026-11-07 06:13 ruling): five
// steel balconies on tower two's face at 640, 662, 684, 706, 728 m, each 4 m
// deep with three rails along each side, an open front edge, and an
// outboard hook bar below the lip; 728 is reached by hook-climb. Measured
// glide, step 1 (2026-11-06): walk face contact 665.7/654.4 m, sprint
// 702.6/691.9 m, flow identical to sprint; no chute dies on the face at
// 71.78 m/s. The departure is a running jump, not a vault.
void run_twin_balconies() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::TwinTower;
    static constexpr double kTops[] = {640.0, 662.0, 684.0, 706.0, 728.0};
    // J-053 (Claude 2026-11-12): the 706 deck's lip is back at its nominal
    // top — the 0.15 m drop did not close the gap, so it was reverted
    // (13:47 ruling), and the +0.20 m bar raise was reverted too (09:25
    // ruling, 2026-11-13); the deck the rays meet is 706.0 again and the
    // bar is back at top - 0.5.
    static constexpr double kDeckTops[] = {640.0, 662.0, 684.0, 706.0, 728.0};
    static constexpr double kHookBarZ = -219.65;
    static constexpr double kRailHeights[] = {0.35, 0.7, 1.1};
    static constexpr std::size_t kTopCount = sizeof(kTops) / sizeof(kTops[0]);
    Simulation s;
    Simulation::LookHit hit;
    // The decks are there, at the five heights, on the face, and nowhere
    // else: no invisible support beside or in front of them.
    for (std::size_t i = 0; i < kTopCount; ++i) {
        const double top = kTops[i];
        const double deck_top = kDeckTops[i];
        require(s.look_target(8.0, top + 5.0, -222.5, 0, -1, 0, 10.0, hit) &&
                    std::abs(hit.point.y - deck_top) < 0.02 && hit.entity == TwinTower::kFrameEntity &&
                    hit.normal.y >= 0.55,
                "twin balconies: the deck top is there at its height");
        require(s.look_target(2.5, top + 5.0, -222.5, 0, -1, 0, 10.0, hit) &&
                    std::abs(hit.point.y - deck_top) < 0.02,
                "twin balconies: the deck reaches its west end");
        require(s.look_target(13.5, top + 5.0, -222.5, 0, -1, 0, 10.0, hit) &&
                    std::abs(hit.point.y - deck_top) < 0.02,
                "twin balconies: the deck reaches its east end");
        for (const double x : {1.0, 15.0}) {
            // Nothing at balcony height beside the deck: open air (no hit
            // in reach) or something far below.
            require(!s.look_target(x, top + 5.0, -222.5, 0, -1, 0, 150.0, hit) || hit.point.y < top - 10.0,
                    "twin balconies: no deck beside the balcony");
        }
        require(!s.look_target(8.0, top + 5.0, -219.0, 0, -1, 0, 150.0, hit) || hit.point.y < top - 10.0,
                "twin balconies: no deck in front of the balcony's edge");
        // Three rails along each side, on posts (the front edge is open:
        // a glide and the hook-climb mantle cross it, so nothing stands
        // across it). A ray level toward the front at each rail height
        // meets open air, not a rail.
        for (const double side : {2.0, 14.0})
            for (const double rail : kRailHeights) {
                // Ray between the posts (posts stand at z -224, -222, -220).
                require(s.look_target(side, top + rail + 0.15, -223.0, 0, -1, 0, 2.0, hit) &&
                            std::abs(hit.point.y - (top + rail + 0.06)) < 0.02 &&
                            hit.entity == TwinTower::kFrameEntity,
                        "twin balconies: each of the three side rails is at its height");
                require(!s.look_target(8.0, top + rail, -219.0, 0, 0, -1, 2.0, hit),
                        "twin balconies: the front edge is open at rail height");
            }
        // The climb's mark: the outboard bar, 0.35 m past the edge and
        // 0.5 m below the deck's top.
        require(s.look_target(8.0, top - 0.2, kHookBarZ, 0, -1, 0, 2.0, hit) &&
                    std::abs(hit.point.y - (top - 0.44)) < 0.02 && hit.entity == TwinTower::kFrameEntity,
                "twin balconies: the hook bar hangs below the deck's lip");
    }
    // The chain: from the lower deck's edge, the next deck's hook bar is
    // inside the hook's 30 m, and what the ray meets is the bar itself:
    // the ray is aimed at the bar's front face from the edge, so a hit at
    // the bar's height means it cleared the deck's slab.
    for (std::size_t i = 0; i + 1 < kTopCount; ++i) {
        const double top = kTops[i];
        const double dx = 0.0;
        const double dy = 22.0 - 0.5 - 1.6;
        const double dz = (kHookBarZ + 0.06) - -220.3;
        const double len = std::sqrt(dx * dx + dy * dy + dz * dz);
        require(s.look_target(8.0, top + 1.6, -220.3, dx / len, dy / len, dz / len, 30.0, hit) &&
                    hit.entity == TwinTower::kFrameEntity && hit.distance < 30.0 &&
                    std::abs(hit.point.y - (top + 21.5)) < 0.15,
                "twin balconies: the next deck's hook bar is inside hook range");
    }
    // The glide: a running jump off the 750 deck's edge, chute open, lands
    // alive on a balcony; walk and sprint, early and late deploy.
    const auto glide = [](const bool sprint, const double deploy_delay_s) {
        Simulation g(InitialSpawn::Deck750);
        require(g.set_facing(0.0, -1.0), "twin balconies: face the void");
        (void)g.set_move_input(0.0, -1.0);
        (void)g.set_sprint_input(sprint);
        require(advance_until(g, [](const Snapshot &n) { return n.player_position.z <= -160.0; }, 10.0),
                "twin balconies: reach the deck's edge");
        require(g.request_jump(), "twin balconies: the running jump off the edge");
        // The delay runs from the edge, not the jump: the jump's apex is
        // at the edge, so a delay from the jump would open the chute half
        // a second early and flatter the worst case.
        require(advance_until(g, [](const Snapshot &n) { return n.player_position.z <= -162.0; }, 3.0),
                "twin balconies: past the deck's edge");
        const double t0 = g.snapshot().simulation_time_seconds;
        require(advance_until(g, [&](const Snapshot &n) { return n.simulation_time_seconds >= t0 + deploy_delay_s; },
                              5.0),
                "twin balconies: wait out the deploy delay");
        require(g.request_parachute(), "twin balconies: open the chute");
        require(advance_until(g, [](const Snapshot &n) { return n.player_grounded || n.death_count > 0; }, 45.0),
                "twin balconies: the glide comes down");
        (void)g.set_move_input(0.0, 0.0);
        (void)g.set_sprint_input(false);
        stand(g);
        const auto n = g.snapshot();
        double deck = -1.0;
        for (const double top : kTops) {
            if (std::abs(n.player_position.y - (top + 0.9)) < 0.6)
                deck = top;
        }
        std::cout << "  twin glide " << (sprint ? "sprint" : "walk") << " delay " << deploy_delay_s << " s: lands "
                  << n.player_position.y << " m on " << deck << " support=" << n.support_entity_id
                  << " deaths " << n.death_count << '\n';
        require(n.death_count == 0 && n.player_grounded && deck > 0.0 &&
                    n.support_entity_id == TwinTower::kFrameEntity,
                "twin balconies: the glide lands alive on a balcony");
        return deck;
    };
    const double landed = glide(true, 0.0);
    require(glide(true, 2.0) > 0.0, "twin balconies: sprint, late deploy, lands");
    require(glide(false, 0.0) > 0.0, "twin balconies: walk, early deploy, lands");
    require(glide(false, 2.0) > 0.0, "twin balconies: walk, late deploy, lands");
    std::cout << "PASS scraperx_sim twin balconies glide: sprint lands on " << landed << " m, all four takeoffs alive\n";
    // The control: the same sprint off, chute never opened, dies on the
    // face. No invisible support carries him across.
    {
        Simulation c(InitialSpawn::Deck750);
        require(c.set_facing(0.0, -1.0), "twin balconies control: face the void");
        (void)c.set_move_input(0.0, -1.0);
        (void)c.set_sprint_input(true);
        require(advance_until(c, [](const Snapshot &n) { return n.player_position.z <= -160.0; }, 10.0),
                "twin balconies control: reach the edge");
        require(c.request_jump(), "twin balconies control: jump");
        // The death restores the body to the deck, so the fall is judged
        // by the lowest height reached on the way down, not by where the
        // body stands after the restore.
        double lowest = 1.0e9;
        bool died = false;
        for (int tick = 0; tick < 90 * 15 && !died; ++tick) {
            require(c.advance_frame(Simulation::kFixedStepSeconds).accepted,
                    "twin balconies control: the fall");
            const auto n = c.snapshot();
            lowest = std::min(lowest, n.player_position.y);
            died = n.death_count > 0;
        }
        require(died, "twin balconies control: no chute dies");
        const auto n = c.snapshot();
        std::cout << "  twin control: no chute dies, lowest " << lowest << " m\n";
        require(lowest < 600.0, "twin balconies control: dies below the balconies");
        (void)n;
        std::cout << "PASS scraperx_sim twin balconies control: no chute, no crossing\n";
    }
    // The checkpoint: saved on the balcony, loaded, still standing on it.
    {
        Simulation g(InitialSpawn::Deck750);
        require(g.set_facing(0.0, -1.0), "twin balconies save: face the void");
        (void)g.set_move_input(0.0, -1.0);
        (void)g.set_sprint_input(true);
        require(advance_until(g, [](const Snapshot &n) { return n.player_position.z <= -160.0; }, 10.0),
                "twin balconies save: reach the edge");
        require(g.request_jump(), "twin balconies save: jump");
        require(advance_until(g, [](const Snapshot &n) { return n.player_position.z <= -162.0; }, 3.0),
                "twin balconies save: past the deck's edge");
        require(g.request_parachute(), "twin balconies save: open the chute");
        require(advance_until(g, [](const Snapshot &n) { return n.player_grounded || n.death_count > 0; }, 45.0),
                "twin balconies save: come down");
        (void)g.set_move_input(0.0, 0.0);
        (void)g.set_sprint_input(false);
        stand(g);
        const auto at = g.snapshot();
        require(at.death_count == 0 && at.player_grounded, "twin balconies save: landed alive");
        const auto saved = g.save_game();
        require(!saved.empty(), "twin balconies save: saves on the balcony");
        Simulation restored(InitialSpawn::Deck750);
        require(restored.load_game(saved.data(), saved.size()), "twin balconies save: reloads");
        stand(restored);
        const auto n = restored.snapshot();
        require(n.player_grounded && n.death_count == 0 &&
                    std::abs(n.player_position.y - at.player_position.y) < 0.6 &&
                    n.support_entity_id == TwinTower::kFrameEntity,
                "twin balconies save: the stand on the balcony survives save and load");
        std::cout << "PASS scraperx_sim twin balconies save: " << at.player_position.y
                  << " m stand survives save and load\n";
    }
}
SCRAPERX_GROUP("twin_balconies", 80, run_twin_balconies);

// J-053 the pitch (Claude 2026-11-12 ruling, option (b)): from a glide
// landing on the 684 m balcony, the two hook pitches up the face, 684 ->
// 706 -> 728. Each pitch bites the next deck's hook bar from the deck
// below, winches up on the line, and tops out onto the deck. The first
// pitch's centre must reach 705.20 m, where the hands (centre + 0.9 m)
// are at the 706 deck's lip; the second's 727.20 m. The setup is the
// climb's honest one: the hook is taken at its rack on the 220 ring, the
// save's checkpoint is moved to the 750 deck, and the glide lands on 684.
// The helper is named for the pitch so it does not collide with the
// climb's own helper when that lands.
void pitch_carry_hook_to_deck750(Simulation &h) {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    Simulation a(InitialSpawn::Ring220North);
    stand(a);
    go(a, -21.6, -127.6);
    require(a.request_hook_take() && a.advance_frame(Simulation::kFixedStepSeconds).accepted &&
                a.snapshot().has_hook,
            "twin pitch: take the hook at its rack on the 220 ring");
    stand(a);
    const auto at = a.snapshot().player_position;
    auto saved = a.save_game();
    require(!saved.empty(), "twin pitch: the hook-taken save exists");
    // Find the checkpoint position in the byte stream: the stand position,
    // as three floats or three doubles.
    std::size_t at_pos = std::string::npos;
    bool as_double = false;
    for (std::size_t off = 0; off + 12 <= saved.size() && off <= 64; ++off) {
        float f[3];
        std::memcpy(f, saved.data() + off, sizeof(f));
        if (std::abs(f[0] - at.x) < 0.01 && std::abs(f[1] - at.y) < 0.01 && std::abs(f[2] - at.z) < 0.01) {
            at_pos = off;
            break;
        }
    }
    if (at_pos == std::string::npos) {
        for (std::size_t off = 0; off + 24 <= saved.size() && off <= 64; ++off) {
            double d[3];
            std::memcpy(d, saved.data() + off, sizeof(d));
            if (std::abs(d[0] - at.x) < 0.01 && std::abs(d[1] - at.y) < 0.01 &&
                std::abs(d[2] - at.z) < 0.01) {
                at_pos = off;
                as_double = true;
                break;
            }
        }
    }
    require(at_pos != std::string::npos, "twin pitch: the checkpoint sits in the save");
    const double deck750[3] = {8.0, 751.0, -145.0};
    if (as_double) {
        for (int i = 0; i < 3; ++i) {
            const double v = deck750[i];
            std::memcpy(saved.data() + at_pos + static_cast<std::size_t>(i) * sizeof(double), &v,
                        sizeof(v));
        }
    } else {
        for (int i = 0; i < 3; ++i) {
            const float v = static_cast<float>(deck750[i]);
            std::memcpy(saved.data() + at_pos + static_cast<std::size_t>(i) * sizeof(float), &v,
                        sizeof(v));
        }
    }
    require(h.load_game(saved.data(), saved.size()), "twin pitch: the moved save loads");
    stand(h);
    const auto n = h.snapshot();
    require(n.has_hook, "twin pitch: the hook is still in hand at the 750 deck");
    require(n.player_grounded && n.death_count == 0 &&
                std::abs(n.player_position.x - 8.0) < 0.6 && std::abs(n.player_position.y - 751.0) < 0.6 &&
                std::abs(n.player_position.z + 145.0) < 0.6,
            "twin pitch: the moved save stands on the 750 deck");
    std::cout << "  twin pitch setup: hook in hand at the 750 deck, at " << n.player_position.x << ','
              << n.player_position.y << ',' << n.player_position.z << '\n';
}

// Bite the next deck's bar from the deck at `from_top` and reel up until
// the line runs out. When `steer` is set, face the wall and press toward
// it for the whole reel (the (a) probe: the hands' catch needs the face
// in front and the stick held toward it). Returns the reel's max centre;
// the sim is left hanging on the line at the stall, reel off.
double pitch_reel(Simulation &h, double from_top, double need_y, bool steer) {
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    using scraperx::sim::TwinTower;
    const double next_top = from_top + 22.0;
    // The next deck's bar, at trunk height (the +0.20 m raise was reverted,
    // 09:25 ruling 2026-11-13); the hook is aimed where the bar actually is.
    const double bar_y = next_top - TwinTower::kBalconyHookBarDropM;
    const double bar_z = TwinTower::kBalconyHookBarZ;
    go(h, 8.0, -220.3);
    stand(h);
    require(fire_and_bite(h, 8.0, bar_y, bar_z), "twin pitch: the hook bites the next deck's bar");
    if (steer) {
        require(h.set_facing(0.0, -1.0), "twin pitch: face the wall for the reel");
        (void)h.set_move_input(0.0, -1.0);
    }
    const double start_y = h.snapshot().player_position.y;
    double max_centre = start_y;
    double last_up = h.snapshot().simulation_time_seconds;
    (void)h.set_hook_reel(true);
    for (int tick = 0; tick < 90 * 25; ++tick) {
        require(h.advance_frame(Simulation::kFixedStepSeconds).accepted, "twin pitch: reel up");
        const auto n = h.snapshot();
        if (n.player_position.y > max_centre) {
            max_centre = n.player_position.y;
            last_up = n.simulation_time_seconds;
        }
        if (n.traversal_state == TraversalState::Hanging)
            break;  // the hands took the lip on the way up
        if (n.player_position.y >= need_y)
            break;
        if (n.death_count > 0 || n.hook_state != 2)
            break;
        if (max_centre > start_y + 1.0 && n.simulation_time_seconds - last_up > 2.0)
            break;  // stalled: the capsule is jammed under the bar
        if (tick % 90 == 0)
            std::cout << "  twin pitch reel: centre " << n.player_position.y << " m, line "
                      << n.hook_line_length << " m, trav " << int(n.traversal_state)
                      << ", hook " << int(n.hook_state) << '\n';
    }
    (void)h.set_hook_reel(false);
    return max_centre;
}

// One pitch: from a stand on the deck at `from_top`, bite the next deck's
// bar, winch up on the line, and top out onto the next deck. `need_y` is
// the centre height the pitch must reach (hands at the lip).
void pitch_up(Simulation &h, double from_top, double need_y) {
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    using scraperx::sim::TwinTower;
    const double next_top = from_top + 22.0;
    // J-053 (Claude 2026-11-13): the reel is steered — face the wall and
    // hold the stick toward it for the whole reel (the P6 inputs). The
    // unsteered reel holds zero move input, and the hands' catch needs
    // the face in front and the stick held toward it.
    double max_centre = pitch_reel(h, from_top, need_y, true);
    const double reel_max = max_centre;
    std::cout << "  twin pitch reel: max centre " << reel_max << " m (need " << need_y << " m)\n";
    // Top out onto the next deck: if the winch brought the hands to the
    // lip, pull up over it; otherwise launch from the bar the winch ends
    // on. The pitch's height is the max centre over the whole manoeuvre,
    // winch and top-out together: the winch alone caps where the line
    // runs out (1.0 m) and the capsule meets the bar, so the launch's
    // apex is part of the pitch, not a second attempt.
    {
        auto n = h.snapshot();
        std::cout << "  twin pitch top: centre " << n.player_position.y << " m, trav "
                  << int(n.traversal_state) << ", up " << int(n.up_move) << ", hook " << int(n.hook_state)
                  << '\n';
        if (n.traversal_state == TraversalState::Hanging) {
            // The winch brought the hands to the lip: let the line go and
            // pull up over it.
            require(h.request_hook_release(), "twin pitch: let the line go");
            require(h.request_up(), "twin pitch: pull up over the lip");
        } else {
            // J-053 (2026-11-14): the deck's lip cannot be hung from here, so
            // the pitch tops out from the bar. The hang's ledge probe needs a
            // face at the centre's height (probe_ledge), its band puts the
            // centre 0.45-1.35 m under the lip, and the deck is a 0.3 m slab
            // with open air under its front edge; the hang's pose (0.41 m off
            // the edge, 1.05 m under the lip) is where the hook bar is. The
            // winch ends with the hands on the bar and its launch offered:
            // face the wall, hold the stick toward it, let the line go and
            // launch from the bar onto the deck.
            require(n.traversal_state == TraversalState::Swinging && n.up_move == scraperx::sim::UpMove::Launch,
                    "twin pitch: the hands are on the bar at the top, its launch offered");
            require(h.set_facing(0.0, -1.0), "twin pitch: face the wall for the launch");
            (void)h.set_move_input(0.0, -1.0);
            require(h.request_hook_release(), "twin pitch: let the line go for the launch");
            require(h.request_up(), "twin pitch: launch from the bar");
        }
        require(advance_until(h, [&](const Snapshot &m) {
                    if (m.player_position.y > max_centre)
                        max_centre = m.player_position.y;
                    return m.player_grounded && m.traversal_state == TraversalState::None;
                }, 6.0),
                "twin pitch: up on the next deck");
        {
            const auto after = h.snapshot();
            std::cout << "  twin pitch top-out: centre " << after.player_position.y << " m at "
                      << after.player_position.x << ',' << after.player_position.z << ", deaths "
                      << after.death_count << '\n';
        }
        stand(h);
        const auto m = h.snapshot();
        if (m.player_position.y > max_centre)
            max_centre = m.player_position.y;
        require(m.death_count == 0 && m.player_grounded &&
                    std::abs(m.player_position.y - (next_top + 0.9)) < 0.6 &&
                    m.support_entity_id == TwinTower::kFrameEntity,
                "twin pitch: standing on the next deck");
    }
    std::cout << "  twin pitch: max centre " << max_centre << " m (need " << need_y << " m, reel max "
              << reel_max << " m)\n";
    require(max_centre >= need_y, "twin pitch: the pitch reaches its height");
    std::cout << "PASS scraperx_sim twin pitch: up onto " << next_top << " m\n";
}

// The glide: a sprint off, chute at 2 s, lands on the 684 deck (the
// balconies' measured sprint-late landing), hook still in hand.
void glide_to_684(Simulation &h) {
    using scraperx::sim::Snapshot;
    using scraperx::sim::TwinTower;
    require(h.set_facing(0.0, -1.0), "twin pitch: face the void");
    (void)h.set_move_input(0.0, -1.0);
    (void)h.set_sprint_input(true);
    require(advance_until(h, [](const Snapshot &n) { return n.player_position.z <= -160.0; }, 10.0),
            "twin pitch: reach the deck's edge");
    require(h.request_jump(), "twin pitch: the running jump off the edge");
    require(advance_until(h, [](const Snapshot &n) { return n.player_position.z <= -162.0; }, 3.0),
            "twin pitch: past the deck's edge");
    const double t0 = h.snapshot().simulation_time_seconds;
    require(advance_until(h, [&](const Snapshot &n) { return n.simulation_time_seconds >= t0 + 2.0; }, 5.0),
            "twin pitch: wait out the deploy delay");
    require(h.request_parachute(), "twin pitch: open the chute");
    require(advance_until(h, [](const Snapshot &n) { return n.player_grounded || n.death_count > 0; }, 45.0),
            "twin pitch: the glide comes down");
    (void)h.set_move_input(0.0, 0.0);
    (void)h.set_sprint_input(false);
    stand(h);
    const auto landed = h.snapshot();
    std::cout << "  twin pitch glide: lands " << landed.player_position.y << " m at "
              << landed.player_position.x << ',' << landed.player_position.z << " support="
              << landed.support_entity_id << " deaths " << landed.death_count
              << " hook " << landed.has_hook << '\n';
    require(landed.death_count == 0 && landed.player_grounded && landed.has_hook &&
                std::abs(landed.player_position.y - (684.0 + 0.9)) < 0.6 &&
                landed.support_entity_id == TwinTower::kFrameEntity,
            "twin pitch: the sprint glide lands alive on the 684 deck, hook in hand");
    std::cout << "PASS scraperx_sim twin pitch glide: sprint, late deploy, lands on 684, hook in hand\n";
}

void run_twin_pitch() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TwinTower;
    Simulation h(InitialSpawn::Deck750);
    pitch_carry_hook_to_deck750(h);
    glide_to_684(h);
    // The two pitches up the face.
    pitch_up(h, 684.0, 705.20);
    pitch_up(h, 706.0, 727.20);
    std::cout << "PASS scraperx_sim twin pitch: 684 -> 706 -> 728, standing on the 728 balcony\n";
}
SCRAPERX_GROUP("twin_pitch", 86, run_twin_pitch);
SCRAPERX_SELECT("twin-pitch", run_twin_pitch);

// J-053 (a) probe (Claude 2026-11-12): from the measured jam (reel max
// 704.541 m, hands 0.56 m under the 706 lip), try every existing move
// that could put the hands on the lip, and MEASURE what each one does.
// Nothing here is required to succeed: the probe reports the numbers,
// and a catch that never comes is the (c) answer. Candidates, in order:
// P6 reel with the face in front and the stick held toward it (the reel
// pumps the pendulum, the hands take the bar); P5' launch off the bar-
// swing at its top toward the wall, timed so the body is back in the
// lip's catch window when the 27-tick regrab lockout clears; P4 let the
// line go and jump for the lip with the face and stick toward the wall.
// If any candidate hangs him on the lip, the probe pulls up over it and
// says so. (The hold and the pump from the unsteered stall are the second
// probe group below, twin_pitch_probe_pump.)
void run_twin_pitch_probe() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    using scraperx::sim::TwinTower;
    Simulation h(InitialSpawn::Deck750);
    pitch_carry_hook_to_deck750(h);
    glide_to_684(h);
    // P6: reel with the face in front, stick toward the wall.
    const double reel_max = pitch_reel(h, 684.0, 705.20, true);
    auto n = h.snapshot();
    std::cout << "  probe P6 reel steered: max centre " << reel_max << " m, at stall centre "
              << n.player_position.y << " m z " << n.player_position.z << ", vy "
              << n.player_linear_velocity.y << ", trav " << int(n.traversal_state) << ", up "
              << int(n.up_move) << ", hook " << int(n.hook_state) << '\n';
    // If any candidate hung him on the lip, pull up over it.
    if (h.snapshot().traversal_state == TraversalState::Hanging) {
        (void)h.request_hook_release();
        (void)h.request_up();
        const bool up = advance_until(h, [](const Snapshot &m) {
                        return m.player_grounded && m.traversal_state == TraversalState::None;
                    }, 6.0);
        n = h.snapshot();
        std::cout << "  probe HANG pull-up: up=" << (up ? "yes" : "no") << " centre "
                  << n.player_position.y << " m at " << n.player_position.x << ',' << n.player_position.z
                  << ", grounded " << n.player_grounded << ", deaths " << n.death_count << '\n';
        std::cout << "PASS scraperx_sim twin pitch probe: the hands took the lip\n";
        return;
    }
    // P5': the lip catch off the bar-swing. The hands took the bar, not
    // the lip; try_begin_hang cannot run while Swinging, and a clean
    // release falls 0.44 m through the 27-tick regrab lockout, below the
    // lip's catch window (centre 705.15-705.45 m). A launch instead buys
    // hang time: thrown near the swing's top toward the wall, the body is
    // back in the window, falling, when the lockout clears 0.3 s later.
    // Fire the launch while the swing still moves toward the wall faster
    // than the launch's 0.5 m/s gate, the centre in the window, and the
    // rise slow; then watch for the hands to take the lip.
    if (h.snapshot().traversal_state == TraversalState::Swinging) {
        bool launched = false;
        double launch_centre = 0.0;
        for (int tick = 0; tick < 90 * 20 && !launched; ++tick) {
            require(h.advance_frame(Simulation::kFixedStepSeconds).accepted,
                    "twin pitch probe: ride the bar-swing");
            n = h.snapshot();
            const double vy = n.player_linear_velocity.y;
            const double vz = n.player_linear_velocity.z;
            if (n.player_position.y >= 705.15 && n.player_position.y <= 705.45 &&
                vz < -0.5 && std::abs(vy) < 1.5) {
                (void)h.request_up();
                launched = true;
                launch_centre = n.player_position.y;
                std::cout << "  probe P5 launch: centre " << launch_centre << " m z "
                          << n.player_position.z << ", vy " << vy << ", vz " << vz << '\n';
            }
            if (tick % 90 == 0)
                std::cout << "  probe P5 ride: centre " << n.player_position.y << " m z "
                          << n.player_position.z << ", vy " << vy << ", vz " << vz << ", trav "
                          << int(n.traversal_state) << '\n';
        }
        std::cout << "  probe P5 ride: launched=" << (launched ? "yes" : "no") << " from centre "
                  << launch_centre << " m, trav " << int(h.snapshot().traversal_state) << '\n';
        if (launched) {
            bool hung = false;
            for (int tick = 0; tick < 90 * 2; ++tick) {
                require(h.advance_frame(Simulation::kFixedStepSeconds).accepted,
                        "twin pitch probe: lip-catch flight");
                n = h.snapshot();
                hung = n.traversal_state == TraversalState::Hanging;
                if (hung || n.player_grounded || n.death_count > 0)
                    break;
                if (tick % 9 == 0)
                    std::cout << "  probe P5 flight: centre " << n.player_position.y << " m z "
                              << n.player_position.z << ", vy " << n.player_linear_velocity.y
                              << ", trav " << int(n.traversal_state) << '\n';
            }
            std::cout << "  probe P5 flight: hung=" << (hung ? "yes" : "no") << " centre "
                      << n.player_position.y << " m z " << n.player_position.z << ", grounded "
                      << n.player_grounded << ", deaths " << n.death_count << '\n';
            if (hung) {
                (void)h.request_hook_release();
                (void)h.request_up();
                const bool up = advance_until(h, [](const Snapshot &m) {
                                return m.player_grounded &&
                                       m.traversal_state == TraversalState::None;
                            }, 6.0);
                n = h.snapshot();
                std::cout << "  probe P5 pull-up: up=" << (up ? "yes" : "no") << " centre "
                          << n.player_position.y << " m at " << n.player_position.x << ','
                          << n.player_position.z << ", grounded " << n.player_grounded << ", deaths "
                          << n.death_count << '\n';
                std::cout << "PASS scraperx_sim twin pitch probe: the hands took the lip off the swing\n";
                return;
            }
        }
    }
    // P4: let the line go and jump for the lip, face and stick toward the
    // wall (the old dyno, with the two missing inputs supplied).
    require(h.set_facing(0.0, -1.0), "twin pitch probe: face the wall for the drop");
    (void)h.set_move_input(0.0, -1.0);
    require(h.request_hook_release(), "twin pitch probe: let the line go");
    require(h.request_up(), "twin pitch probe: jump for the lip");
    bool hung = false;
    for (int tick = 0; tick < 90 * 5; ++tick) {
        require(h.advance_frame(Simulation::kFixedStepSeconds).accepted, "twin pitch probe: drop");
        n = h.snapshot();
        hung = n.traversal_state == TraversalState::Hanging;
        if (hung || n.player_grounded || n.death_count > 0)
            break;
        if (tick % 45 == 0)
            std::cout << "  probe P4 drop: centre " << n.player_position.y << " m z "
                      << n.player_position.z << ", trav " << int(n.traversal_state) << '\n';
    }
    std::cout << "  probe P4 drop: hung=" << (hung ? "yes" : "no") << " centre " << n.player_position.y
              << " m z " << n.player_position.z << ", grounded " << n.player_grounded << ", deaths "
              << n.death_count << '\n';
    std::cout << "PASS scraperx_sim twin pitch probe: P6/P5'/P4 measured (see above)\n";
}
SCRAPERX_GROUP("twin_pitch_probe", 87, run_twin_pitch_probe);
SCRAPERX_SELECT("twin-pitch-probe", run_twin_pitch_probe);

// J-053 (a) probe, second group: from the UNSTEERED stall (reel max
// 704.541 m, trav None), the two candidates that cannot run after the
// steered reel takes the bar. P1: hold at the stall with the face and
// stick toward the wall. P2: pump the line's swing RESONANTLY — push
// outboard while moving outboard, coast back, and reach for the wall
// only at an outboard extreme already inside the lip's catch window
// (centre >= 705.16 m, z >= -218.80 m, so the wall is within 1.2 m).
// The input is never toward the wall anywhere else: at the swing's
// bottom the rise is ~0, so a toward-wall stick there would take the
// BAR (its catch runs when the ledge probe fails), and at the wall-side
// extreme the same. The outboard side has no slab over it, so the swing
// can in principle carry the centre to the window there (theta ~70 deg,
// wall 1.29 m away, just in reach) — the wall side is slab-capped at
// centre ~704.8 m, which the first probe group measured. If P2 hangs
// him on the lip, pull up over it and say so.
void run_twin_pitch_probe_pump() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    using scraperx::sim::TwinTower;
    // P1: hold at the unsteered stall, face and stick toward the wall.
    {
        Simulation h(InitialSpawn::Deck750);
        pitch_carry_hook_to_deck750(h);
        glide_to_684(h);
        const double reel_max = pitch_reel(h, 684.0, 705.20, false);
        auto n = h.snapshot();
        std::cout << "  probe P1 reel unsteered: max centre " << reel_max << " m, at stall centre "
                  << n.player_position.y << " m z " << n.player_position.z << ", trav "
                  << int(n.traversal_state) << ", hook " << int(n.hook_state) << '\n';
        require(h.set_facing(0.0, -1.0), "twin pitch probe pump: face the wall for the hold");
        (void)h.set_move_input(0.0, -1.0);
        bool hung = false;
        for (int tick = 0; tick < 90 * 5; ++tick) {
            require(h.advance_frame(Simulation::kFixedStepSeconds).accepted,
                    "twin pitch probe pump: hold at the stall");
            n = h.snapshot();
            hung = n.traversal_state == TraversalState::Hanging;
            if (hung || n.player_grounded || n.death_count > 0)
                break;
            if (tick % 90 == 0)
                std::cout << "  probe P1 hold: centre " << n.player_position.y << " m z "
                          << n.player_position.z << ", trav " << int(n.traversal_state) << '\n';
        }
        std::cout << "  probe P1 hold: hung=" << (hung ? "yes" : "no") << " centre "
                  << n.player_position.y << " m z " << n.player_position.z << ", trav "
                  << int(n.traversal_state) << ", grounded " << n.player_grounded << ", deaths "
                  << n.death_count << '\n';
        if (hung) {
            (void)h.request_hook_release();
            (void)h.request_up();
            const bool up = advance_until(h, [](const Snapshot &m) {
                            return m.player_grounded &&
                                   m.traversal_state == TraversalState::None;
                        }, 6.0);
            n = h.snapshot();
            std::cout << "  probe P1 pull-up: up=" << (up ? "yes" : "no") << " centre "
                      << n.player_position.y << " m at " << n.player_position.x << ','
                      << n.player_position.z << ", grounded " << n.player_grounded << ", deaths "
                      << n.death_count << '\n';
            std::cout << "PASS scraperx_sim twin pitch probe pump: the hold took the lip\n";
            return;
        }
    }
    // P2: resonant pump from the unsteered stall.
    {
        Simulation h(InitialSpawn::Deck750);
        pitch_carry_hook_to_deck750(h);
        glide_to_684(h);
        const double reel_max = pitch_reel(h, 684.0, 705.20, false);
        auto n = h.snapshot();
        std::cout << "  probe P2 reel unsteered: max centre " << reel_max << " m, at stall centre "
                  << n.player_position.y << " m z " << n.player_position.z << ", trav "
                  << int(n.traversal_state) << '\n';
        require(h.set_facing(0.0, -1.0), "twin pitch probe pump: face the wall for the pump");
        double pump_max = n.player_position.y;
        double pump_max_z = n.player_position.z;
        bool hung = false;
        for (int tick = 0; tick < 90 * 30; ++tick) {
            n = h.snapshot();
            const double vz = n.player_linear_velocity.z;
            const double y = n.player_position.y;
            const double z = n.player_position.z;
            // Kick the swing outboard, then pump in phase; reach for the
            // wall only at an outboard extreme already in the window.
            double in_z = 0.0;
            if (tick < 45)
                in_z = 1.0;
            else if (std::abs(vz) > 0.5)
                in_z = (vz > 0.0) ? 1.0 : 0.0;
            else if (z > -219.5 && y >= 705.16 && z >= -218.80)
                in_z = -1.0;
            (void)h.set_move_input(0.0, in_z);
            require(h.advance_frame(Simulation::kFixedStepSeconds).accepted,
                    "twin pitch probe pump: pump the swing");
            n = h.snapshot();
            if (n.player_position.y > pump_max) {
                pump_max = n.player_position.y;
                pump_max_z = n.player_position.z;
            }
            hung = n.traversal_state == TraversalState::Hanging;
            if (hung || n.player_grounded || n.death_count > 0)
                break;
            if (tick % 90 == 0)
                std::cout << "  probe P2 pump: centre " << n.player_position.y << " m z "
                          << n.player_position.z << ", vy " << n.player_linear_velocity.y
                          << ", vz " << n.player_linear_velocity.z << ", trav "
                          << int(n.traversal_state) << ", pump max " << pump_max << " m at z "
                          << pump_max_z << '\n';
        }
        std::cout << "  probe P2 pump: hung=" << (hung ? "yes" : "no") << " pump max centre "
                  << pump_max << " m at z " << pump_max_z << ", final centre "
                  << n.player_position.y << " m z " << n.player_position.z << ", trav "
                  << int(n.traversal_state) << ", grounded " << n.player_grounded << ", deaths "
                  << n.death_count << '\n';
        if (hung) {
            (void)h.request_hook_release();
            (void)h.request_up();
            const bool up = advance_until(h, [](const Snapshot &m) {
                            return m.player_grounded &&
                                   m.traversal_state == TraversalState::None;
                        }, 6.0);
            n = h.snapshot();
            std::cout << "  probe P2 pull-up: up=" << (up ? "yes" : "no") << " centre "
                      << n.player_position.y << " m at " << n.player_position.x << ','
                      << n.player_position.z << ", grounded " << n.player_grounded << ", deaths "
                      << n.death_count << '\n';
            std::cout << "PASS scraperx_sim twin pitch probe pump: the pump took the lip\n";
            return;
        }
    }
    std::cout << "PASS scraperx_sim twin pitch probe pump: P1/P2 measured (see above)\n";
}
SCRAPERX_GROUP("twin_pitch_probe_pump", 88, run_twin_pitch_probe_pump);
SCRAPERX_SELECT("twin-pitch-probe-pump", run_twin_pitch_probe_pump);
