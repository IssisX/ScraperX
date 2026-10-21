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
