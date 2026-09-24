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
// Physics audit of the envelope's candidate first entries above 44 m, with
// the tower's 33->44 flight cut. Each attempt: settle, move/face, jump on a
// rule, then request traversal every tick and climb (jump) from any hang.
// Success = grounded with centre >= 44.85 (standing on a top >= 44 m).
struct Attempt {
    const char *family; float x, y, z, fx, fz;
    std::function<bool(const Snapshot &)> jump_when;
    bool block = false; float bx = 0, bz = 0;
};
static bool cut_flight(Simulation &sim) {
    auto &w = *sim.physics_world_;
    auto &bi = w.physics_system_.GetBodyInterface();
    JPH::BodyIDVector ids;
    w.physics_system_.GetBodies(ids);
    for (auto id : ids) {
        const JPH::AABox b = bi.GetTransformedShape(id).GetWorldSpaceBounds();
        const JPH::Quat q = bi.GetRotation(id);
        if (bi.GetUserData(id) == Simulation::kTowerEntityId && std::abs(std::abs(q.GetW()) - 1.0F) > 1e-4F &&
            b.mMin.GetY() > 32.0F && b.mMin.GetY() < 33.5F && b.mMax.GetY() > 43.5F && b.mMax.GetY() < 44.5F) {
            bi.SetPosition(id, JPH::RVec3(0.0F, -2000.0F, 0.0F), JPH::EActivation::DontActivate);
            return true;
        }
    }
    return false;
}
struct Out { bool entered; double best_y; int hangs; unsigned long long hang_ent; double hang_y; int deaths; };
static Out run(const Attempt &a) {
    Simulation sim(InitialSpawn::ExteriorGrade);
    if (!cut_flight(sim)) { std::printf("cut failed\n"); std::exit(2); }
    auto &w = *sim.physics_world_;
    auto &bi = w.physics_system_.GetBodyInterface();
    if (a.block)
        w.add_box(bi, JPH::Vec3(0.30F, 0.25F, 0.30F), JPH::RVec3(a.bx, 40.1872F + 0.25F, a.bz),
                  JPH::EMotionType::Static, object_layers::kStatic, 0.7F, 991);
    bi.SetPosition(w.player_id_, JPH::RVec3(a.x, a.y, a.z), JPH::EActivation::Activate);
    bi.SetLinearVelocity(w.player_id_, JPH::Vec3::sZero());
    (void)sim.set_facing(a.fx, a.fz);
    (void)sim.advance_frame(0.4);
    Out o{false, 0.0, 0, 0, 0.0, 0};
    bool jumped = false;
    for (int i = 0; i < 90 * 5 && !o.entered; ++i) {
        (void)sim.set_facing(a.fx, a.fz);
        (void)sim.set_move_input(a.fx, a.fz);
        auto s = sim.snapshot();
        if (!jumped && s.player_grounded && a.jump_when(s)) { (void)sim.request_jump(); jumped = true; }
        else if (jumped) {
            if (s.traversal_state == TraversalState::Hanging) (void)sim.request_jump();
            else (void)sim.request_traversal();
        }
        (void)sim.advance_frame(1.0 / 90.0);
        s = sim.snapshot();
        if (s.player_grounded) o.best_y = std::max(o.best_y, s.player_position.y);
        if (s.traversal_state == TraversalState::Hanging) {
            ++o.hangs; o.hang_ent = s.traversal_support_entity_id; o.hang_y = s.player_position.y;
        }
        if (s.player_grounded && s.player_position.y >= 44.85) o.entered = true;
        o.deaths = (int)s.death_count;
        if (o.deaths > 0) break;
    }
    return o;
}
int main() {
    std::vector<Attempt> list;
    // F1: MOD-HALL-DECK (40.19, south edge z = -122.5) toward the tower's 44 m
    // north edge (trim 43.85 at z [-124.3, -123.7]; deck 44.0 from z = -124).
    for (float x = -9.5F; x <= 9.51F; x += 1.0F) {
        const bool in_well_x = x > -4.8F && x < 1.5F;
        const float z0 = in_well_x ? -119.6F : -113.0F;
        for (float d : {0.05F, 0.35F, 0.7F})
            list.push_back({"F1 run", x, 41.2F, z0, 0.0F, -1.0F,
                            [d](const Snapshot &s) { return s.player_position.z <= -122.5 + d; }});
        list.push_back({"F1 stand", x, 41.2F, -122.1F, 0.0F, -1.0F, [](const Snapshot &) { return true; }});
    }
    // F1 + the hook block set down at the edge, stood on
    for (float x : {-8.0F, -6.0F, -3.0F, 0.0F, 3.0F, 6.0F, 8.0F}) {
        Attempt a{"F1 block stand", x, 41.75F, -122.15F, 0.0F, -1.0F, [](const Snapshot &) { return true; }};
        a.block = true; a.bx = x; a.bz = -122.15F;
        list.push_back(a);
        Attempt r{"F1 block run", x, 41.2F, (x > -4.8F && x < 1.5F) ? -119.6F : -115.0F, 0.0F, -1.0F,
                  [](const Snapshot &s) { return s.player_position.y > 41.4 && s.player_position.z <= -122.2; }};
        r.block = true; r.bx = x; r.bz = -122.15F;
        list.push_back(r);
    }
    // F2: the west annex (x [-34, -25], z [-156.5, -143.5], 32-54 m); ledges
    // on its north face at x [-30.6, -28.4], tops 36.35 / 40.75 / 45.15.
    for (float z0 : {-142.9F, -143.1F})
        for (float d : {0.0F, 0.4F})
            list.push_back({"F2 deck33 run west", -20.0F, 33.9F, z0, -1.0F, 0.0F,
                            [d](const Snapshot &s) { return s.player_position.x <= -25.7 + d; }});
    for (float x : {-25.5F, -26.3F})
        list.push_back({"F2 deck33 stand SW", x, 33.9F, -142.8F, -0.7071F, -0.7071F,
                        [](const Snapshot &) { return true; }});
    for (float x : {-33.0F, -31.0F, -29.5F, -28.0F})
        list.push_back({"F2 strip stand S", x, 33.1F, -143.2F, 0.0F, -1.0F, [](const Snapshot &) { return true; }});
    int entered = 0;
    for (const auto &a : list) {
        const Out o = run(a);
        entered += o.entered ? 1 : 0;
        std::printf("%-20s x=%6.2f z=%8.2f f=(%5.2f,%5.2f) entered=%d best_grounded_y=%6.2f hang_ticks=%3d "
                    "hang_ent=%llu hang_y=%6.2f deaths=%d\n",
                    a.family, a.x, a.z, a.fx, a.fz, o.entered, o.best_y, o.hangs, o.hang_ent, o.hang_y, o.deaths);
    }
    std::printf("attempts=%zu entered_above_44=%d\n", list.size(), entered);
}
