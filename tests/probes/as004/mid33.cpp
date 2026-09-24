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
// Tower's 33 m deck (north edge z = -124) -> AS-002's mid-landing (32.19 m,
// z [-121, -117], entity 44) by a running jump north. Then walk the upper
// flight toward the hall deck to see whether the chain completes.
int main() {
    int landed = 0, forty = 0, n = 0;
    for (float x : {-4.0F, 4.0F, 6.0F}) {
        for (float d : {0.1F, 0.5F}) {
            ++n;
            Simulation sim(InitialSpawn::ExteriorGrade);
            auto &w = *sim.physics_world_;
            auto &bi = w.physics_system_.GetBodyInterface();
            bi.SetPosition(w.player_id_, JPH::RVec3(x, 33.95F, -129.0F), JPH::EActivation::Activate);
            (void)sim.set_facing(0.0, 1.0);
            (void)sim.advance_frame(0.4);
            bool jumped = false, on_mid = false, on_hall = false;
            for (int i = 0; i < 90 * 6 && !on_mid; ++i) {
                (void)sim.set_facing(0.0, 1.0);
                (void)sim.set_move_input(0.0, 1.0);
                auto s = sim.snapshot();
                if (!jumped && s.player_grounded && s.player_position.z >= -124.0 - d) {
                    (void)sim.request_jump();
                    jumped = true;
                }
                (void)sim.advance_frame(1.0 / 90.0);
                s = sim.snapshot();
                if (s.player_grounded && s.support_entity_id == 44 && s.player_position.y < 33.5) on_mid = true;
                if (s.death_count > 0) break;
            }
            if (on_mid) {
                ++landed;
                // the upper flight climbs west (-x) from x = 9.44 at 32.2 to x = -4.6 at 40.3, z [-117.9, -116.1]
                const std::array<std::array<float, 2>, 4> route{{{9.0F, -119.5F}, {9.0F, -117.0F}, {-4.0F, -117.0F}, {-7.5F, -117.0F}}};
                for (const auto &p : route) {
                    for (int i = 0; i < 90 * 8; ++i) {
                        auto s = sim.snapshot();
                        const double dx = p[0] - s.player_position.x, dz = p[1] - s.player_position.z;
                        const double l = std::hypot(dx, dz);
                        if (l < 0.4) break;
                        (void)sim.set_facing(dx / l, dz / l);
                        (void)sim.set_move_input(dx / l, dz / l);
                        (void)sim.advance_frame(1.0 / 90.0);
                        s = sim.snapshot();
                        if (s.player_grounded && s.support_entity_id == 49) on_hall = true;
                    }
                }
                (void)sim.set_move_input(0.0, 0.0);
                (void)sim.advance_frame(1.0);
                {
                    const auto t = sim.snapshot();
                    if (t.player_grounded && t.support_entity_id == 49) on_hall = true;
                }
                forty += on_hall ? 1 : 0;
            }
            const auto s = sim.snapshot();
            std::printf("x=%5.1f d=%.1f jumped=%d on_mid=%d on_hall=%d grounded=%d support=%llu final=(%.2f,%.2f,%.2f) deaths=%d\n", x, d,
                        jumped, on_mid, on_hall, (int)s.player_grounded, (unsigned long long)s.support_entity_id, s.player_position.x, s.player_position.y, s.player_position.z,
                        (int)s.death_count);
        }
    }
    std::printf("attempts=%d landed_mid=%d reached_hall=%d\n", n, landed, forty);
}
