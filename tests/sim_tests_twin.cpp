#include "sim_tests.hpp"
#include "sim/twin_tower.hpp"

namespace {
using scraperx::sim::Simulation;
using scraperx::sim::Snapshot;
void stand(Simulation &s) {
    require(s.set_move_input(0, 0) && s.advance_frame(0.7).accepted, "twin: stand");
}
void go(Simulation &s, double x, double z) {
    const bool reached = walk_to(s, x, z, 35.0, 0.18);
    const auto n = s.snapshot();
    std::cout << "  twin target " << x << ',' << z << " at " << n.player_position.x << ','
              << n.player_position.y << ',' << n.player_position.z << " support=" << n.support_entity_id << '\n';
    require(reached, "twin: walk target reached using movement input");
    require(n.death_count == 0, "twin: route must not recover by death");
}
}

void run_twin() {
    Simulation s(scraperx::sim::InitialSpawn::Ring220North);
    stand(s);
    unsigned matching = 0;
    for (std::uint32_t i = 0; i < s.kit_body_count(); ++i)
        matching += s.kit_body_entity(i) == scraperx::sim::TwinTower::kFrameEntity ? 1U : 0U;
    require(matching == 1, "twin: route owns a unique static entity");
    go(s, -21.6, -127.6);
    require(s.request_hook_take() && s.advance_frame(Simulation::kFixedStepSeconds).accepted && s.snapshot().has_hook,
            "twin: take existing hook on A");
    go(s, -21.6, -171.5);
    go(s, -7.0, -172.5);
    go(s, -6.5, -177.0);
    go(s, -7.5, -177.0);
    go(s, -7.0, -218.0);
    stand(s);
    require(s.snapshot().player_grounded && s.snapshot().player_position.y > 221.0, "twin: stand on B receiving deck");
    const auto lower_saved = s.save_game();
    go(s, -17.8, -218.0);
    stand(s);
    require(s.snapshot().player_grounded && s.snapshot().player_position.y > 225.0, "twin: climb the supported incline");
    go(s, -18.4, -218.0);
    require(s.set_facing(-1, 0) && s.request_traversal(), "twin: mantle the service plinth");
    require(advance_until(s, [](const Snapshot &n) { return n.player_grounded && n.player_position.y > 226.4; }, 3.0),
            "twin: ordinary mantle reaches the upper landing");
    stand(s);
    const auto saved = s.save_game();
    require(!saved.empty(), "twin: upper landing saves");
    Simulation restored(scraperx::sim::InitialSpawn::Ring220North);
    require(restored.load_game(saved.data(), saved.size()), "twin: upper landing reloads");
    stand(restored);
    require(restored.snapshot().has_hook && restored.snapshot().player_position.y > 226.4,
            "twin: recovery retains upper checkpoint and acquired hook");
    go(restored, -17.8, -218.0);
    go(restored, -7.0, -218.0);
    go(restored, -7.0, -172.5);
    stand(restored);
    require(restored.snapshot().player_grounded && restored.snapshot().player_position.y < 222.0,
            "twin: recovered player can retreat to A without a hook shot");
    Simulation hooked(scraperx::sim::InitialSpawn::Ring220North);
    require(hooked.load_game(lower_saved.data(), lower_saved.size()), "twin: recover receiving deck for hook cut");
    stand(hooked);
    const auto p = hooked.snapshot().player_position;
    // Aim 0.7 m above the bar: the existing projectile falls under gravity.
    require(hooked.set_hook_aim(-20.5 - p.x, 229.2 - (p.y + 0.6), -218.0 - p.z) && hooked.request_hook_fire(),
            "twin: fire at supported overhead member");
    require(advance_until(hooked, [](const Snapshot &n) {
        if (n.hook_state == 1 && n.hook_position.x < -21.0 && n.hook_position.x > -24.0)
            std::cout << "  twin shot: " << n.hook_position.x << ',' << n.hook_position.y << ',' << n.hook_position.z << '\n';
        return n.hook_state == 2;
    }, 1.2),
            "twin: existing hook bites within 30 m");
    require(hooked.set_hook_reel(true), "twin: reel using existing finite force");
    int reel_ticks = 0;
    require(advance_until(hooked, [&](const Snapshot &n) {
        if (++reel_ticks % 90 == 0)
            std::cout << "  twin reel: " << n.player_position.x << ',' << n.player_position.y << ',' << n.player_position.z
                      << " state=" << int(n.hook_state) << " tension=" << n.hook_tension_n << '\n';
        return n.player_position.x < -20.0 && n.player_position.y > 226.6;
    }, 12.0),
            "twin: reel over upper landing");
    require(hooked.set_hook_reel(false) && hooked.request_hook_release(), "twin: release over landing");
    require(advance_until(hooked, [](const Snapshot &n) { return n.player_grounded; }, 3.0), "twin: hook cut lands");
    require(hooked.snapshot().player_position.y > 226.4 && hooked.snapshot().death_count == 0,
            "twin: hook cut reaches same recoverable landing");
    std::cout << "PASS scraperx_sim twin hook: real shot, reel, release, grounded landing\n";
    std::cout << "PASS scraperx_sim twin: A rack -> B incline -> mantle -> saved recovery -> A, no teleport\n";
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
}
