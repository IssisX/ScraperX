#include "sim/simulation.hpp"
#include "sim/vault_rail_route.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace scraperx::sim;
namespace {
void require(bool ok, const char *message, const Simulation &sim) {
    if (ok) return;
    const auto s = sim.snapshot();
    std::cerr << "FAIL PALM_VAULT " << message << " tick=" << s.tick_index
              << " p=" << s.player_position.x << ',' << s.player_position.y << ',' << s.player_position.z
              << " v=" << s.player_linear_velocity.x << ',' << s.player_linear_velocity.y << ',' << s.player_linear_velocity.z
              << " support=" << s.support_entity_id << " compact=" << s.player_crouched
              << " count=" << s.palm_vault_count << '\n';
    std::exit(EXIT_FAILURE);
}
bool active(const Snapshot &s) { return s.hand_props[0].active || s.hand_props[1].active; }
double load(const Snapshot &s) {
    double total = 0;
    for (const auto &p : s.hand_props) total += std::hypot(p.player_force.x, p.player_force.y, p.player_force.z);
    return total;
}
void tick(Simulation &sim) {
    require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "ordinary native tick accepted", sim);
    const auto s = sim.snapshot();
    require(s.death_count == 0 && s.player_gravity_factor == 1 && s.traversal_hand_constraint_count == 0 &&
            s.traversal_plan_kind == 0 && s.traversal_state == TraversalState::None,
            "real gravity and survival without a climbing grip or traversal plan", sim);
    for (const auto &p : s.hand_props) {
        require(std::isfinite(p.last_event_work_j) && p.last_event_work_j <= 1e-8 && p.command_work_j == 0 &&
                p.absorbed_event_work_j >= 0 && p.last_impulse_ns >= 0 && p.last_impulse_ns <= 1500.0 / 360.0 + 1e-5 &&
                std::hypot(p.player_force.x, p.player_force.y, p.player_force.z) <= 1500.01 && p.elapsed_seconds <= .250001,
                "finite passive palm readback never invents work or exceeds physical limits", sim);
        if (p.active) {
            const double reach = std::hypot(p.anchor.x - s.player_position.x,
                p.anchor.y - s.player_position.y - (.45-(.9-.6)), p.anchor.z - s.player_position.z);
            require(p.support_entity_id == kVaultRailEntity && std::abs(p.anchor.y - kVaultRailTopM) < .002 &&
                    p.anchor.x >= -43.5 && p.anchor.x <= -40.5 && p.anchor.z >= -72.091 && p.anchor.z <= -71.909 &&
                    reach <= 1.05001 && p.normal.y > .999 &&
                    std::abs(p.player_force.x) < 1e-6 && std::abs(p.player_force.z) < 1e-6,
                    "palm acts at reachable actual exposed rail material and adds no tangent impulse", sim);
        }
    }
}
void stage(Simulation &sim) {
    require(sim.debug_restart_at({-42, .9, -65}), "initial grade staging only", sim);
    for (unsigned i = 0; i < 60; ++i) tick(sim);
    require(sim.snapshot().player_grounded && sim.snapshot().support_entity_id == 1, "actual approach grade", sim);
}
void arrive(Simulation &sim, double x, double z) {
    require(sim.set_crouch_input(false) && sim.set_sprint_input(false), "clear grounded standing approach", sim);
    for (unsigned i = 0; i < 600; ++i) {
        const auto s = sim.snapshot();
        require(s.player_grounded && s.support_entity_id == 1 && !active(s), "retry walk uses actual grade", sim);
        const double dx = x - s.player_position.x, dz = z - s.player_position.z;
        if (std::hypot(dx, dz) < .18 && std::hypot(s.player_linear_velocity.x, s.player_linear_velocity.z) < .4) {
            require(sim.set_move_input(0, 0), "release waypoint steering", sim);
            return;
        }
        require(sim.set_move_input(std::clamp(1.3 * dx - .45 * s.player_linear_velocity.x, -.6, .6),
                                  std::clamp(1.3 * dz - .45 * s.player_linear_velocity.z, -.6, .6)),
                "ordinary grade waypoint steering", sim);
        tick(sim);
    }
    require(false, "bounded physical retry walk reaches waypoint", sim);
}
void launch(Simulation &sim) {
    require(sim.set_facing(0, -1) && sim.set_move_input(0, -1) && sim.set_sprint_input(false),
            "ordinary committed forward run", sim);
    bool jumped = false;
    for (unsigned i = 0; i < 180; ++i) {
        tick(sim);
        const auto s = sim.snapshot();
        if (s.player_position.z < -67.6) {
            require(s.player_grounded && s.support_entity_id == 1 && s.player_linear_velocity.z < -5.4 &&
                    sim.request_jump(), "earned grade takeoff at actual running speed", sim);
            jumped = true;
            break;
        }
    }
    require(jumped, "bounded run reaches chosen takeoff", sim);
    for (unsigned i = 0; i < 30; ++i) {
        tick(sim);
        if (!sim.snapshot().player_grounded && sim.snapshot().player_position.y > 1.2) {
            require(sim.set_crouch_input(true), "held airborne Crouch tucks without another button", sim);
            return;
        }
    }
    require(false, "actual jump reaches compact free flight", sim);
}
void crossing(Simulation &sim, bool cancel) {
    const auto before = sim.snapshot().palm_vault_count;
    launch(sim);
    bool loaded = false, interrupted = false;
    double peak_load = 0;
    unsigned loaded_ticks = 0;
    for (unsigned i = 0; i < 210; ++i) {
        tick(sim);
        const auto s = sim.snapshot();
        peak_load = std::max(peak_load, load(s));
        if (load(s) > 0) { loaded = true; ++loaded_ticks; }
        if (!cancel && s.player_position.z < -71.56 && s.player_position.z > -72.44)
            require(!s.player_grounded && s.support_entity_id == 0 &&
                    s.player_position.y - .6 > kVaultRailTopM,
                    "actual compact capsule clears the cap without a body-contact bounce", sim);
        // The observed palm force above has no tangent component. Ordinary
        // capsule/rail collision may still redirect the body's momentum.
        if (cancel && !interrupted && loaded && active(s)) {
            require(sim.request_release(), "explicit release during actual loaded palm prop", sim);
            tick(sim);
            require(!active(sim.snapshot()) && load(sim.snapshot()) == 0, "release clears actual palm load", sim);
            interrupted = true;
        } else if (interrupted && !s.player_grounded) {
            require(!active(s) && s.palm_vault_count == before + 1,
                    "cancelled departure never reacquires a delayed palm before real footing", sim);
        }
        if (s.player_grounded && s.support_entity_id == 1 && s.player_position.z < -72.4) {
            require(loaded && (cancel ? interrupted : loaded_ticks >= 2) && s.palm_vault_count == before + 1,
                    "one actual positive-load crossing returns to receiving grade", sim);
            require(sim.set_crouch_input(false) && sim.set_move_input(0, 0), "clear stance on real receiving grade", sim);
            tick(sim);
            require(!sim.snapshot().player_crouched && sim.snapshot().player_grounded &&
                    sim.snapshot().support_entity_id == 1 && !active(sim.snapshot()),
                    "receiving grade permits clear standing without retained palm support", sim);
            std::cout << "PALM_CROSS cancel=" << cancel << " peak_load_n=" << peak_load
                      << " loaded_ticks=" << loaded_ticks << " grade_tick=" << s.tick_index
                      << " z=" << s.player_position.z << '\n';
            return;
        }
    }
    require(false, "bounded jump earns loaded palm crossing and real grade", sim);
}
void flow(Simulation &sim) {
    stage(sim);
    crossing(sim, false);
    arrive(sim, -42, -75);
    // The existing storage trays obstruct the east end. The clear west
    // aisle lies between the rail and duct; no reset or pose write.
    arrive(sim, -45, -75);
    arrive(sim, -45, -65);
    arrive(sim, -42, -65);
    crossing(sim, false);
    require(sim.snapshot().palm_vault_count == 2, "real receiving footing rearms exactly one physical retry", sim);
}
void under(Simulation &sim) {
    stage(sim);
    require(sim.set_facing(0, -1) && sim.set_crouch_input(true), "compact grounded approach", sim);
    for (unsigned i = 0; i < 600; ++i) {
        const auto s = sim.snapshot();
        const double dz = -72 - s.player_position.z;
        if (std::abs(dz) < .06 && std::abs(s.player_linear_velocity.z) < .15) break;
        require(sim.set_move_input(0, std::clamp(1.3 * dz - .45 * s.player_linear_velocity.z, -.8, .8)),
                "ordinary compact under-rail approach", sim);
        tick(sim);
    }
    require(std::abs(sim.snapshot().player_position.z + 72) < .08 && sim.snapshot().player_grounded &&
            sim.snapshot().player_crouched && sim.snapshot().support_entity_id == 1,
            "real compact capsule passes under the steel cap", sim);
    require(sim.set_move_input(0, 0) && sim.set_crouch_input(false), "request full standing below actual underside", sim);
    tick(sim);
    require(sim.snapshot().player_crouched && sim.snapshot().player_grounded && !active(sim.snapshot()) &&
            sim.snapshot().palm_vault_count == 0, "actual overhead rail blocks stand without palm lift", sim);
    require(sim.set_crouch_input(true) && sim.set_move_input(0, -1), "continue compact underpass", sim);
    for (unsigned i = 0; i < 150 && sim.snapshot().player_position.z > -72.7; ++i) tick(sim);
    require(sim.snapshot().player_position.z < -72.6 && sim.set_move_input(0, 0) && sim.set_crouch_input(false),
            "clear the real rail before standing", sim);
    tick(sim);
    require(!sim.snapshot().player_crouched && sim.snapshot().player_grounded &&
            sim.snapshot().support_entity_id == 1 && sim.snapshot().palm_vault_count == 0,
            "grounded underpass exits standing without a vault or phantom support", sim);
}
void slow(Simulation &sim) {
    require(sim.debug_restart_at({-42, .9, -70}), "initial slow approach staging only", sim);
    for (unsigned i = 0; i < 60; ++i) tick(sim);
    require(sim.set_facing(0, -1) && sim.set_move_input(0, -.25), "ordinary slow forward approach", sim);
    for (unsigned i = 0; i < 30; ++i) tick(sim);
    const auto approach = sim.snapshot();
    require(approach.player_grounded && approach.support_entity_id == 1 &&
            approach.player_linear_velocity.z < -.5 && approach.player_linear_velocity.z > -2.475 &&
            sim.request_jump(), "insufficient-speed approach still gets an ordinary earned jump", sim);
    bool airborne = false, tucked = false, landed = false;
    for (unsigned i = 0; i < 180; ++i) {
        tick(sim);
        const auto s = sim.snapshot();
        airborne = airborne || !s.player_grounded;
        if (!tucked && !s.player_grounded && s.player_position.y > 1.2) {
            require(sim.set_crouch_input(true), "ordinary midair tuck on a slow jump", sim);
            tucked = true;
        }
        require(!active(s) && load(s) == 0 && s.palm_vault_count == 0,
                "insufficient committed approach never receives an automatic palm contact", sim);
        landed = landed || (airborne && s.player_grounded);
    }
    require(airborne && tucked && landed, "refused palm attempt retains ordinary flight and real receiving contact", sim);
    std::cout << "PALM_SLOW approach_mps=" << -approach.player_linear_velocity.z << " palm_count=0\n";
}
void flat(Simulation &sim) {
    stage(sim);
    require(sim.set_facing(1, 0) && sim.set_move_input(1, 0), "ordinary run across clear grade", sim);
    for (unsigned i = 0; i < 150; ++i) {
        tick(sim);
        if (sim.snapshot().player_linear_velocity.x > 5.4) break;
    }
    require(sim.snapshot().player_grounded && sim.snapshot().player_linear_velocity.x > 5.4 &&
            sim.request_jump(), "earned running jump without an obstacle", sim);
    bool airborne = false, tucked = false;
    for (unsigned i = 0; i < 180; ++i) {
        tick(sim);
        const auto s = sim.snapshot();
        airborne = airborne || !s.player_grounded;
        if (!tucked && !s.player_grounded && s.player_position.y > 1.2) {
            require(sim.set_crouch_input(true), "ordinary airborne tuck above flat ground", sim);
            tucked = true;
        }
        require(!active(s) && load(s) == 0 && s.palm_vault_count == 0,
                "a flat receiving floor cannot become an automatic vault palm", sim);
        if (airborne && s.player_grounded && s.support_entity_id == 1) {
            require(tucked && s.player_linear_velocity.x > 2.475,
                    "ordinary tuck retains its momentum and actual foot landing", sim);
            std::cout << "PALM_FLAT grade_tick=" << s.tick_index << " palm_count=0\n";
            return;
        }
    }
    require(false, "ordinary flat-ground tuck reaches real receiving feet", sim);
}
} // namespace
int main(int argc, char **argv) {
    std::cout << std::unitbuf;
    const std::string mode = argc > 1 ? argv[1] : "flow";
    Simulation sim(InitialSpawn::ExteriorGrade);
    if (mode == "flow") flow(sim);
    else if (mode == "cancel") { stage(sim); crossing(sim, true); }
    else if (mode == "under") under(sim);
    else if (mode == "slow") slow(sim);
    else if (mode == "flat") flat(sim);
    else require(false, "known mode required", sim);
    std::cout << "PASS PALM_VAULT_FLOW mode=" << mode << '\n';
}
