#include "sim/simulation.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace scraperx::sim;
namespace {
void require(bool value, const char *message, const Simulation &sim) {
    if (value) return;
    const auto s = sim.snapshot(); const auto line = sim.traversal_line_state();
    std::cerr << "FAIL PORTABLE_LINE_INPUT " << message << " tick=" << s.tick_index
        << " p=" << s.player_position.x << ',' << s.player_position.y << ',' << s.player_position.z
        << " state=" << unsigned(s.traversal_state) << " hands=" << s.traversal_hand_constraint_count
        << " installed=" << line.installed << " gripping=" << line.gripping
        << " target=" << line.target << " failure=" << line.failure << " tension_n=" << line.tension_n << '\n';
    std::exit(1);
}
void tick(Simulation &sim, unsigned count = 1) {
    while (count--) require(sim.advance_frame(Simulation::kFixedStepSeconds).accepted, "native fixed tick", sim);
}
}
int main() {
    Simulation sim(InitialSpawn::FreightYardArrival);
    tick(sim, 20);
    require(sim.traversal_line_state().available && !sim.traversal_line_state().installed,
        "ordinary world starts with a carried traversal line", sim);
    require(!sim.request_traversal_line(static_cast<TraversalLineCommandKind>(255)), "unknown command rejects", sim);
    require(!sim.set_traversal_line_reel_input(std::numeric_limits<double>::quiet_NaN()) &&
        !sim.set_traversal_line_reel_input(1.01), "invalid reel input rejects", sim);
    for (unsigned i = 0; i < 8; ++i)
        require(sim.request_traversal_line(TraversalLineCommandKind::Equip), "bounded queue admission", sim);
    require(!sim.request_traversal_line(TraversalLineCommandKind::Equip), "full input queue rejects without overwriting", sim);
    require(sim.request_traversal_line(TraversalLineCommandKind::Unequip), "cancellation survives a full queue", sim);
    tick(sim);
    // Explicitly staged owner integration fixture. The separate complete route
    // must prove walking here, onward footing and same-life reuse without this.
    require(sim.debug_restart_at({-50, 2.0, -59}), "clear staged native input position", sim);
    const auto bar = sim.kit_body_index(2480);
    const auto origin = sim.kit_body_position(bar);
    const auto rotation = sim.kit_body_rotation(bar);
    // This ordinary transverse rail is 1.2m below its pivot. Its actual pose,
    // not an invented attachment or authored rigging eye, supplies the target.
    const double target_x = origin.x + 2 * (rotation.x * rotation.y - rotation.z * rotation.w) * -1.2;
    const double target_y = origin.y + (1 - 2 * (rotation.x * rotation.x + rotation.z * rotation.z)) * -1.2;
    const double target_z = origin.z + 2 * (rotation.y * rotation.z + rotation.x * rotation.w) * -1.2;
    const auto player = sim.snapshot().player_position;
    require(sim.set_rigging_aim(target_x-player.x, target_y-player.y-.6, target_z-player.z), "actual beam aim", sim);
    require(sim.request_traversal_line(TraversalLineCommandKind::Equip), "public line equip", sim);
    tick(sim);
    const auto candidate = sim.traversal_line_state();
    require(candidate.target_valid && candidate.target, "real moving rail is an eligible broad clamp surface", sim);
    require(sim.request_traversal_line(TraversalLineCommandKind::Action, candidate.target), "displayed Action queued", sim);
    tick(sim);
    require(sim.traversal_line_state().installed && sim.traversal_line_state().gripping &&
        sim.snapshot().traversal_state == TraversalState::LineSwing &&
        sim.snapshot().traversal_hand_constraint_count == 2, "public Action installs then attaches existing two hands", sim);
    require(sim.traversal_line_state().available_spools == 0, "installed length consumes actual carried stock", sim);
    require(sim.set_move_input(0, 0), "neutral swing intent", sim);
    tick(sim, 30);
    require(sim.traversal_line_state().gripping && sim.snapshot().player_gravity_factor == 1,
        "ordinary solved line swing retains gravity and hand ownership", sim);
    require(sim.request_release(), "ordinary Drop input", sim);
    tick(sim);
    require(!sim.traversal_line_state().gripping && sim.traversal_line_state().installed &&
        sim.snapshot().traversal_hand_constraint_count == 0 &&
        sim.snapshot().traversal_state == TraversalState::None,
        "Drop releases actual hands while preserving installation", sim);
    require(sim.request_traversal_line(TraversalLineCommandKind::Unequip), "stow portable tool", sim);
    tick(sim);
    require(!sim.traversal_line_state().equipped && sim.traversal_line_state().installed,
        "stowed world line remains physical and allocated", sim);
    std::cout << "PASS PORTABLE_LINE_INPUT ordinary_stock=1 queue=1 real_rail_target=1 installed_hands=2 gravity=1"
        " drop_preserves_installation=1 tool_change=1\n";
}
