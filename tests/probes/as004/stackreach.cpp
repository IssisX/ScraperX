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
// Stand on an as-built stack surface, run toward a planned AS-004 member
// (added as a static probe box, entity 990), jump at the surface's edge.
// Report whether the player ever stands on or hangs from the probe member.
struct Probe { const char *name; float sx, sy, sz; float dirz; float edge_z;
               float cx, cy, cz, hx, hy, hz; };
static void run(const Probe &p, float run_x, bool jump = true) {
    Simulation sim(InitialSpawn::ExteriorGrade);
    auto &w = *sim.physics_world_;
    auto &bi = w.physics_system_.GetBodyInterface();
    w.add_box(bi, JPH::Vec3(p.hx, p.hy, p.hz), JPH::RVec3(p.cx, p.cy, p.cz),
              JPH::EMotionType::Static, object_layers::kStatic, 0.7F, 990);
    bi.SetPosition(w.player_id_, JPH::RVec3(run_x, p.sy + 1.0F, p.sz), JPH::EActivation::Activate);
    (void)sim.set_facing(0.0, p.dirz);
    (void)sim.advance_frame(0.5);
    bool jumped = false, stood = false, hung = false;
    double min_y = 1e9;
    for (int i = 0; i < 90 * 8; ++i) {
        (void)sim.set_facing(0.0, p.dirz);
        (void)sim.set_move_input(0.0, p.dirz);
        auto s = sim.snapshot();
        if (jump && !jumped && s.player_grounded && (p.dirz > 0 ? s.player_position.z >= p.edge_z - 0.15
                                                         : s.player_position.z <= p.edge_z + 0.15)) {
            (void)sim.request_jump();
            jumped = true;
        }
        (void)sim.advance_frame(1.0 / 90.0);
        s = sim.snapshot();
        min_y = std::min(min_y, s.player_position.y);
        if (s.support_entity_id == 990 && s.player_grounded) stood = true;
        if (s.traversal_state == TraversalState::Hanging && s.traversal_support_entity_id == 990) hung = true;
        if (stood || hung || s.death_count > 0) break;
    }
    auto s = sim.snapshot();
    std::printf("%-34s %s x=%6.2f jumped=%d stood=%d hung=%d final=(%.2f,%.2f,%.2f) min_y=%.2f deaths=%d\n",
                p.name, jump ? "jump" : "walk", run_x, jumped, stood, hung, s.player_position.x, s.player_position.y,
                s.player_position.z, min_y, (int)s.death_count);
}
int main() {
    // needle A stowed on its 48 m rack (AS-004 8.3 as corrected: centre 48.61, top 48.86)
    const Probe balcony51{"51.35 balcony -> stowed needle A", 6.5F, 51.35F, -123.0F, 1.0F, -121.4F,
                          0.0F, 48.61F, -112.8F, 9.20F, 0.25F, 0.45F};
    // west pocket A at 96 m: centre (-9, 95.75, -112.8), half (0.5, 0.3, 0.65), top 96.05
    const Probe balc95{"95.35 balcony -> west pocket A", -13.0F, 95.35F, -123.0F, 1.0F, -121.4F,
                       -9.0F, 95.75F, -112.8F, 0.50F, 0.30F, 0.65F};
    // needle A seated at 96 m
    const Probe gant109{"109.5 gantry -> seated needle A", 8.0F, 109.5F, -122.8F, 1.0F, -122.15F,
                        0.0F, 96.0F, -112.8F, 9.20F, 0.25F, 0.45F};
    // from the tower decks themselves, toward the balcony and the gantry
    const Probe deck55{"55 deck -> balcony -> stowed needle A", 6.5F, 55.0F, -127.0F, 1.0F, -121.4F,
                       0.0F, 48.61F, -112.8F, 9.20F, 0.25F, 0.45F};
    const Probe deck110{"110 deck -> gantry -> seated needle A", 6.5F, 110.0F, -127.0F, 1.0F, -122.15F,
                        0.0F, 96.0F, -112.8F, 9.20F, 0.25F, 0.45F};
    for (float x : {5.0F, 6.5F, 8.0F}) run(balcony51, x, true);
    for (float x : {-15.5F, -13.0F}) run(balc95, x, true);
    for (float x : {5.0F, 6.0F, 7.0F, 7.5F, 8.0F, 9.0F, 11.0F}) run(gant109, x, false);
    for (float x : {5.0F, 6.5F}) run(deck55, x, true);
    for (float x : {3.0F, 6.5F, 9.0F}) run(deck110, x, false);
}
