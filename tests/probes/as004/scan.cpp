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
// Every body whose top is above 12 m, anywhere in the world, grouped by entity,
// with the union of their bounds -- to find what, besides the tower's stair,
// stands high.
int main() {
    Simulation sim(InitialSpawn::ExteriorGrade);
    (void)sim.advance_frame(0.1);
    auto &w = *sim.physics_world_;
    JPH::BodyIDVector ids;
    w.physics_system_.GetBodies(ids);
    auto &bi = w.physics_system_.GetBodyInterface();
    struct Agg { int n = 0; float x0 = 1e9, x1 = -1e9, y0 = 1e9, y1 = -1e9, z0 = 1e9, z1 = -1e9; };
    std::map<unsigned long long, Agg> by;
    for (auto id : ids) {
        JPH::AABox b = bi.GetTransformedShape(id).GetWorldSpaceBounds();
        if (b.mMax.GetY() < 12.0F) continue;
        auto &a = by[(unsigned long long)bi.GetUserData(id)];
        ++a.n;
        a.x0 = std::min(a.x0, b.mMin.GetX()); a.x1 = std::max(a.x1, b.mMax.GetX());
        a.y0 = std::min(a.y0, b.mMin.GetY()); a.y1 = std::max(a.y1, b.mMax.GetY());
        a.z0 = std::min(a.z0, b.mMin.GetZ()); a.z1 = std::max(a.z1, b.mMax.GetZ());
    }
    for (auto &[e, a] : by)
        std::printf("ent=%3llu bodies=%4d x[%7.1f,%7.1f] y[%6.1f,%6.1f] z[%7.1f,%7.1f]\n", e, a.n, a.x0, a.x1, a.y0,
                    a.y1, a.z0, a.z1);
}
