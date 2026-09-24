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
// Stand on the stack's 44 m deck, walk north (+z) off its edge toward
// MOD-HALL-DECK (entity 49, 40.19 m, z edge -122.5). No jump.
int main() {
    for (float x : {-8.0F, -5.0F, 0.0F, 5.0F, 8.0F}) {
        Simulation sim(InitialSpawn::ExteriorGrade);
        auto &w = *sim.physics_world_;
        auto &bi = w.physics_system_.GetBodyInterface();
        bi.SetPosition(w.player_id_, JPH::RVec3(x, 45.0F, -127.0F), JPH::EActivation::Activate);
        (void)sim.advance_frame(0.5);
        bool on_hall = false;
        for (int i = 0; i < 90 * 6 && !on_hall; ++i) {
            (void)sim.set_facing(0.0, 1.0);
            (void)sim.set_move_input(0.0, 1.0);
            (void)sim.advance_frame(1.0 / 90.0);
            auto s = sim.snapshot();
            if (s.player_grounded && s.support_entity_id == 49) on_hall = true;
            if (s.death_count > 0) break;
        }
        auto s = sim.snapshot();
        std::printf("44 deck -> hall deck x=%5.1f on_hall=%d pos=(%.2f,%.2f,%.2f) deaths=%d\n", x, on_hall,
                    s.player_position.x, s.player_position.y, s.player_position.z, (int)s.death_count);
    }
}
