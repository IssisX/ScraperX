#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <mutex>
#include <array>
#include <cmath>
#include <optional>
#include <Jolt/Jolt.h>
#define private public
#include "sim/simulation.cpp"
using namespace scraperx::sim;
// From MOD-HALL-DECK (40.19 m, south edge z = -122.5) run south (-z) and jump
// at the edge toward the tower's 44 m deck. Hold forward through any hang so
// the native mantle can finish. Report the highest grounded y reached.
int main() {
    for (float x : {-8.0F, -5.0F, 5.0F, 8.0F}) {
        Simulation sim(InitialSpawn::ExteriorGrade);
        auto &w = *sim.physics_world_;
        auto &bi = w.physics_system_.GetBodyInterface();
        bi.SetPosition(w.player_id_, JPH::RVec3(x, 41.2F, -118.5F), JPH::EActivation::Activate);
        (void)sim.set_facing(0.0, -1.0);
        (void)sim.advance_frame(0.5);
        bool jumped = false, hung = false;
        double best_grounded_y = 0.0;
        for (int i = 0; i < 90 * 6; ++i) {
            (void)sim.set_facing(0.0, -1.0);
            (void)sim.set_move_input(0.0, -1.0);
            auto s = sim.snapshot();
            if (!jumped && s.player_grounded && s.player_position.z <= -122.35) {
                (void)sim.request_jump();
                jumped = true;
            }
            (void)sim.advance_frame(1.0 / 90.0);
            s = sim.snapshot();
            if (s.traversal_state == TraversalState::Hanging) hung = true;
            if (s.player_grounded) best_grounded_y = std::max(best_grounded_y, s.player_position.y);
            if (s.death_count > 0) break;
        }
        auto s = sim.snapshot();
        std::printf("hall deck -> 44 deck x=%5.1f jumped=%d hung=%d best_grounded_y=%.2f final=(%.2f,%.2f,%.2f) deaths=%d\n",
                    x, jumped, hung, best_grounded_y, s.player_position.x, s.player_position.y,
                    s.player_position.z, (int)s.death_count);
    }
}
