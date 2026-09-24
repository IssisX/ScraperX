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
int main() {
    Simulation sim(InitialSpawn::Hook5Cage);
    (void)sim.advance_frame(0.1);
    auto &w = *sim.physics_world_;
    JPH::BodyIDVector ids;
    w.physics_system_.GetBodies(ids);
    auto &bi = w.physics_system_.GetBodyInterface();
    // standable tops (y extent small relative to area) between 40 and 125 m, near the AS-004 shaft
    for (auto id : ids) {
        JPH::AABox b = bi.GetTransformedShape(id).GetWorldSpaceBounds();
        if (b.mMax.GetY() < 40.0F || b.mMax.GetY() > 125.0F) continue;
        if (b.mMax.GetX() < -20 || b.mMin.GetX() > 20 || b.mMax.GetZ() < -135 || b.mMin.GetZ() > -95) continue;
        const float wx = b.mMax.GetX() - b.mMin.GetX(), wz = b.mMax.GetZ() - b.mMin.GetZ();
        if (wx < 0.6F || wz < 0.6F) continue;   // too narrow to stand on
        std::printf("ent=%3llu top=%6.2f x[%6.2f,%6.2f] z[%8.2f,%8.2f]\n", (unsigned long long)bi.GetUserData(id), b.mMax.GetY(),
                    b.mMin.GetX(), b.mMax.GetX(), b.mMin.GetZ(), b.mMax.GetZ());
    }
}
