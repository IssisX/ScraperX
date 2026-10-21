// Water (physics_world_water.cpp): a body in a pool floats, is slowed, swims,
// and always has its way out. The TP-340 header tank (band_wet_isolation.cpp)
// is the pool a player fell into and could not leave.

#include "sim_tests.hpp"

void run_water() {
    using scraperx::sim::InitialSpawn;
    using scraperx::sim::Simulation;
    using scraperx::sim::Snapshot;
    using scraperx::sim::TraversalState;
    Simulation sim(InitialSpawn::PlateTop);
    (void)sim.advance_frame(1.0);
    require(!sim.snapshot().in_water, "water: dry on the plate");
    const std::uint64_t entries = sim.snapshot().water_entry_count;
    // Dropped in from 6 m over the surface, mid-tank, falling.
    sim.debug_launch_player({6.0, 351.0, -147.5}, {0.0, -2.0, 0.0});
    double entry_speed = 0.0;
    double deepest = 1.0e9;
    require(wait_for(sim, 3.0,
                     [&](const Snapshot &state) {
                         if (state.in_water) {
                             entry_speed = state.water_entry_speed_mps;
                         }
                         deepest = std::min(deepest, state.player_position.y);
                         return state.in_water && state.player_linear_velocity.y > 0.0;
                     }),
            "water: the fall goes in and the water turns it");
    const Snapshot in = sim.snapshot();
    require(in.water_entry_count == entries + 1, "water: one entry counted");
    require(entry_speed > 9.0 && entry_speed < 13.0, "water: entry speed is the fall's");
    require(in.water_surface_y > 344.0 && in.water_surface_y < 345.3, "water: the header's level");
    require(in.death_count == 0, "water: the fall into water is not a death");
    // Afloat: settles with the head out, the body barely moving.
    (void)sim.advance_frame(6.0);
    const Snapshot floating = sim.snapshot();
    require(floating.in_water && floating.swimming, "water: afloat");
    require(!floating.head_underwater, "water: the head is out of the water afloat");
    require(std::abs(floating.player_linear_velocity.y) < 0.15, "water: afloat and still");
    require(floating.water_submersion > 0.75 && floating.water_submersion < 0.97, "water: mostly under");
    // Swimming: the stick drives at no more than the swim's speed.
    double fastest = 0.0;
    (void)hold_stick(sim, 1.0, 0.0, 1.0, 0.0, 2.0, [&](const Snapshot &state) {
        fastest = std::max(fastest, horizontal_speed(state));
        return false;
    });
    std::cout << "  swim fastest " << fastest << "\n";
    require(fastest > 0.8 && fastest < 1.45, "water: a swim, at a swimmer's speed");
    // The way out: swim north to the ladder inside the north wall, catch it,
    // climb, top out onto the landing.
    (void)hold_stick(sim, -1.0, 0.0, -1.0, 0.0, 8.0, [](const Snapshot &state) { return state.player_position.x < 5.1; });
    const bool caught = hold_stick(sim, 0.0, 1.0, 0.0, 1.0, 12.0, [](const Snapshot &state) {
        return state.traversal_state == TraversalState::Climbing;
    });
    {
        const Snapshot d = sim.snapshot();
        std::cout << "  at " << d.player_position.x << "," << d.player_position.y << "," << d.player_position.z
                  << " grip=" << d.grip_available << " trav=" << static_cast<int>(d.traversal_state)
                  << " vy=" << d.player_linear_velocity.y << " grounded=" << d.player_grounded << "\n";
    }
    require(caught, "water: swum to the ladder, the hands take a rung");
    require(hold_stick(sim, 0.0, 1.0, 0.0, 1.0, 30.0,
                       [](const Snapshot &state) { return standing_above(state, 345.0); }),
            "water: up the ladder and out onto the landing");
    const Snapshot out = sim.snapshot();
    require(!out.in_water && out.water_exit_count >= 1, "water: out and dry");
    std::cout << "  out at " << out.player_position.x << "," << out.player_position.y << "," << out.player_position.z << "\n";
    {
        const bool w = walk_to(sim, 5.0, -140.8, 12.0, 0.15);
        (void)sim.advance_frame(1.5);
        const Snapshot d = sim.snapshot();
        std::cout << "  walked=" << w << " at " << d.player_position.x << "," << d.player_position.y << "," << d.player_position.z
                  << " deaths=" << d.death_count << "\n";
    }
    // And down the crates to the plate, on foot.
    require(sim.snapshot().player_position.y < 341.5 &&
                sim.snapshot().death_count == 0,
            "water: off the plank, down to the plate, alive");
    std::cout << "PASS scraperx_sim water: entry_mps=" << entry_speed << " deepest_y=" << deepest
              << " float_submersion=" << floating.water_submersion << " swim_mps=" << fastest
              << " out_y=" << out.player_position.y << "\n";
}
