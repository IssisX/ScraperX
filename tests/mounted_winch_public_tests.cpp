#include "sim/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace scraperx::sim;
namespace {
void report(const char *stage, const Simulation &sim) {
    const auto s = sim.snapshot(); const auto r = sim.rigging_state();
    const auto p = sim.kit_body_position(sim.kit_body_index(2499));
    std::cout << "MOUNTED_WINCH " << stage << " tick=" << s.tick_index
        << " player=" << s.player_position.x << ',' << s.player_position.y << ',' << s.player_position.z
        << " beam=" << p.x << ',' << p.y << ',' << p.z << " installed=" << r.installed_count
        << " target=" << r.target << " kind=" << r.target_kind << " failure=" << r.failure
        << " operable=" << r.power_operable << " brake=" << r.braking
        << " speed=" << r.target_speed_mps << " length=" << r.rest_length_m
        << " tension=" << r.tension_n << " energy=" << r.energy_j
        << " used=" << r.source_energy_used_j << '\n';
}
void require(bool value, const char *why, const Simulation &sim) {
    if (value) return;
    report(why, sim); std::cerr << "FAIL MOUNTED_WINCH_PUBLIC " << why << '\n'; std::exit(1);
}
void tick(Simulation &sim, unsigned count = 1) {
    while (count--) {
        require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "fixed tick accepted", sim);
        require(sim.snapshot().death_count == 0 && sim.snapshot().player_gravity_factor == 1,
            "ordinary same-life operation retains gravity", sim);
    }
}
void walk(Simulation &sim, double x, double z) {
    for (unsigned i = 0; i < 2400; ++i) {
        const auto s = sim.snapshot(); const auto p = s.player_position, v = s.player_linear_velocity;
        const double dx = x - p.x, dz = z - p.z;
        if (std::hypot(dx, dz) < .08 && s.player_grounded && std::hypot(v.x, v.z) < .17) {
            require(sim.set_move_input(0, 0), "neutral walking", sim); tick(sim, 8); return;
        }
        require(sim.set_move_input(std::clamp(1.3*dx-.45*v.x, -.55, .55),
            std::clamp(1.3*dz-.45*v.z, -.55, .55)), "ordinary movement input", sim);
        tick(sim);
    }
    require(false, "public walking reaches workstation", sim);
}
void command(Simulation &sim, RiggingCommandKind kind, std::uint64_t target = 0) {
    require(sim.request_rigging(kind, target), "ordered rigging command accepted", sim); tick(sim, 2);
}
void aim(Simulation &sim, Vector3 target) {
    const auto p = sim.snapshot().player_position;
    require(sim.set_rigging_aim(target.x-p.x, target.y-p.y-.62, target.z-p.z), "finite aim accepted", sim);
    tick(sim, 3);
}
Vector3 beam_eye(const Simulation &sim) {
    const auto body = sim.kit_body_index(2499);
    const auto p = sim.kit_body_position(body); const auto q = sim.kit_body_rotation(body);
    // Shape-origin eye, transformed by this ordinary body's actual rotation.
    constexpr double x = -1.8, y = .17;
    return {p.x+(1-2*(q.y*q.y+q.z*q.z))*x+2*(q.x*q.y-q.z*q.w)*y,
        p.y+2*(q.x*q.y+q.z*q.w)*x+(1-2*(q.x*q.x+q.z*q.z))*y,
        p.z+2*(q.x*q.z-q.y*q.w)*x+2*(q.y*q.z+q.x*q.w)*y};
}
}
int main() {
    std::cout << std::unitbuf;
    Simulation sim(InitialSpawn::FreightYardArrival); tick(sim, 90);
    walk(sim, -80, -43.5); walk(sim, -100, -43.5); walk(sim, -108, -40.5);
    walk(sim, -109.1, -38.2); walk(sim, -109.1, -36.85);
    command(sim, RiggingCommandKind::Equip);
    aim(sim, {-109.8, 2.70, -36});
    require(sim.rigging_state().target && sim.rigging_state().target_kind == 2,
        "actual reachable winch eye offered", sim);
    command(sim, RiggingCommandKind::Action, sim.rigging_state().target);
    require(sim.rigging_state().draft, "first clip only previews cable", sim);
    aim(sim, beam_eye(sim));
    require(sim.rigging_state().target && sim.rigging_state().target_kind == 0,
        "actual loose beam eye offered", sim);
    command(sim, RiggingCommandKind::Action, sim.rigging_state().target);
    tick(sim, 45);
    require(sim.rigging_state().installed_count == 1 && sim.rigging_state().power_operable,
        "two clips install one operable powered cable", sim);
    require(!sim.set_rigging_power_input(std::numeric_limits<double>::quiet_NaN()),
        "input boundary rejects non-finite effort", sim);
    const double initial_eye_y = beam_eye(sim).y;
    const double initial_energy = sim.rigging_state().energy_j;
    require(sim.set_rigging_power_input(1), "held lift accepted", sim); tick(sim, 270);
    require(sim.set_rigging_power_input(0), "finger release engages brake", sim); tick(sim, 90);
    const double raised_y = beam_eye(sim).y;
    require(raised_y > initial_eye_y + .4 && sim.rigging_state().tension_n > 300 &&
        sim.rigging_state().energy_j < initial_energy && sim.rigging_state().motor_work_j > 0,
        "real loose beam lifts through paid cable tension", sim);
    require(sim.rigging_state().braking && sim.rigging_state().target_speed_mps == 0,
        "neutral command holds finite brake", sim);
    report("lift-and-brake", sim);
    const auto installed = sim.rigging_state().selected;
    command(sim, RiggingCommandKind::Unclip, installed);
    require(sim.rigging_state().installed_count == 1 && sim.rigging_state().failure == 8,
        "loaded unclip is rejected", sim);
    command(sim, RiggingCommandKind::Unequip); tick(sim, 180);
    require(sim.rigging_state().installed_count == 1 &&
        std::abs(beam_eye(sim).y-raised_y) < .08, "stow keeps installed cable and held load", sim);
    command(sim, RiggingCommandKind::Equip);
    const double used = sim.rigging_state().source_energy_used_j;
    require(sim.set_rigging_power_input(-1), "held lower accepted", sim); tick(sim, 270);
    require(sim.set_rigging_power_input(0), "stop lowering", sim); tick(sim, 90);
    require(beam_eye(sim).y < raised_y - .3 && sim.rigging_state().braking &&
        sim.rigging_state().source_energy_used_j >= used,
        "payout lowers actual beam without source refill", sim);
    report("lowered", sim);
    require(sim.set_rigging_power_input(1), "lift starts before moving away", sim); tick(sim, 30);
    walk(sim, -108, -40.5);
    require(!sim.rigging_state().power_operable && sim.rigging_state().target_speed_mps == 0,
        "leaving source reach stops powered command", sim);
    require(sim.set_rigging_power_input(0), "neutral before return", sim);
    walk(sim, -109.1, -38.2); walk(sim, -109.1, -36.85);
    require(sim.rigging_state().power_operable, "same installation remains usable on return", sim);
    command(sim, RiggingCommandKind::Unequip);
    walk(sim, -108, -40.5); walk(sim, -105, -43.5);
    require(sim.snapshot().player_grounded && sim.rigging_state().installed_count == 1,
        "ordinary onward play preserves winch installation", sim);
    std::cout << "PASS MOUNTED_WINCH_PUBLIC arrival=1 paid_lift=1 brake=1 lower=1 loaded_unclip_rejected=1"
        << " stow_persists=1 range_stop=1 return=1 onward=1 deaths=0 resets=0\n";
}
