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
// Tower (entity 11) bodies above the stack's 154 m top deck, and the lowest
// underside of anything above 156.1 m within the tower's plan.
int main() {
    Simulation sim(InitialSpawn::ExteriorGrade);
    (void)sim.advance_frame(0.1);
    auto &w = *sim.physics_world_;
    JPH::BodyIDVector ids;
    w.physics_system_.GetBodies(ids);
    auto &bi = w.physics_system_.GetBodyInterface();
    std::vector<std::array<float, 7>> rows;
    for (auto id : ids) {
        const JPH::AABox b = bi.GetTransformedShape(id).GetWorldSpaceBounds();
        const auto ent = (unsigned long long)bi.GetUserData(id);
        if (b.mMax.GetY() <= 156.2F) continue;
        if (ent != Simulation::kTowerEntityId && !(b.mMin.GetX() < 60 && b.mMax.GetX() > -90 && b.mMin.GetZ() < -100 && b.mMax.GetZ() > -400)) continue;
        rows.push_back({(float)ent, b.mMin.GetY(), b.mMax.GetY(), b.mMin.GetX(), b.mMax.GetX(), b.mMin.GetZ(), b.mMax.GetZ()});
    }
    std::sort(rows.begin(), rows.end(), [](auto &a, auto &b) { return a[1] < b[1]; });
    std::printf("bodies_above_156_near_tower=%zu\n", rows.size());
    for (size_t i = 0; i < rows.size() && i < 40; ++i)
        std::printf("ent=%2.0f y[%7.1f,%7.1f] x[%6.1f,%6.1f] z[%7.1f,%7.1f]\n", rows[i][0], rows[i][1], rows[i][2], rows[i][3],
                    rows[i][4], rows[i][5], rows[i][6]);
}
