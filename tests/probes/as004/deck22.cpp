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
// Tower's 11 and 22 m decks (north edge z = -124) -> AS-001/AS-002 bodies
// (entities 44-50) by a running jump north, traversal requested in flight.
int main() {
    int n = 0, hit = 0;
    for (float deck : {11.0F, 22.0F}) {
        for (float x : {-8.0F, -6.0F, -4.0F, 4.0F, 6.0F, 8.0F}) {
            for (float d : {0.1F, 0.5F}) {
                ++n;
                Simulation sim(InitialSpawn::ExteriorGrade);
                auto &w = *sim.physics_world_;
                auto &bi = w.physics_system_.GetBodyInterface();
                bi.SetPosition(w.player_id_, JPH::RVec3(x, deck + 0.95F, -129.0F), JPH::EActivation::Activate);
                (void)sim.set_facing(0.0, 1.0);
                (void)sim.advance_frame(0.4);
                bool jumped = false;
                unsigned long long first = 0;
                double first_y = 0;
                for (int i = 0; i < 90 * 5 && first == 0; ++i) {
                    (void)sim.set_facing(0.0, 1.0);
                    (void)sim.set_move_input(0.0, 1.0);
                    auto s = sim.snapshot();
                    if (!jumped && s.player_grounded && s.player_position.z >= -124.0 - d) {
                        (void)sim.request_jump();
                        jumped = true;
                    } else if (jumped) {
                        if (s.traversal_state == TraversalState::Hanging) (void)sim.request_jump();
                        else (void)sim.request_traversal();
                    }
                    (void)sim.advance_frame(1.0 / 90.0);
                    s = sim.snapshot();
                    if (s.player_grounded && s.support_entity_id >= 44 && s.support_entity_id <= 50) {
                        first = s.support_entity_id;
                        first_y = s.player_position.y;
                    }
                    if (s.death_count > 0) break;
                }
                hit += first ? 1 : 0;
                const auto s = sim.snapshot();
                std::printf("deck=%4.0f x=%5.1f d=%.1f jumped=%d landed_ent=%llu at_y=%6.2f final=(%.2f,%.2f,%.2f) deaths=%d\n",
                            deck, x, d, jumped, first, first_y, s.player_position.x, s.player_position.y,
                            s.player_position.z, (int)s.death_count);
            }
        }
    }
    std::printf("attempts=%d landed_on_ascent=%d\n", n, hit);
}
