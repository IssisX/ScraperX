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
// Envelope audit for option A's minimal cut (remove the tower's 33->44 flight).
// A first entry above 44 m from below must be an upward move of at most
// 3.75 m (measured jump-grab reach) + 0.50 m (standing on the hook block).
// List every (source, target) pair of body tops with source top in
// [39.75, 44), target top in [44, source + 4.25], horizontal gap <= 4.0 m.
// Rotated bodies are included by their AABB (over-approximation).
struct Top { JPH::BodyID id; unsigned long long ent; float y, x0, x1, z0, z1; bool rotated; };
static float gap(const Top &a, const Top &b) {
    const float dx = std::max({0.0F, a.x0 - b.x1, b.x0 - a.x1});
    const float dz = std::max({0.0F, a.z0 - b.z1, b.z0 - a.z1});
    return std::sqrt(dx * dx + dz * dz);
}
int main() {
    Simulation sim(InitialSpawn::ExteriorGrade);
    (void)sim.advance_frame(0.1);
    auto &w = *sim.physics_world_;
    JPH::BodyIDVector ids;
    w.physics_system_.GetBodies(ids);
    auto &bi = w.physics_system_.GetBodyInterface();
    std::vector<Top> tops;
    int removed = 0;
    for (auto id : ids) {
        const JPH::AABox b = bi.GetTransformedShape(id).GetWorldSpaceBounds();
        const auto ent = (unsigned long long)bi.GetUserData(id);
        const JPH::Quat q = bi.GetRotation(id);
        const bool rotated = std::abs(std::abs(q.GetW()) - 1.0F) > 1e-4F;
        // the tower's 33->44 flight: inclined entity-11 slab spanning 33..44 m
        if (ent == Simulation::kTowerEntityId && rotated && b.mMin.GetY() < 33.5F && b.mMin.GetY() > 32.0F &&
            b.mMax.GetY() > 43.5F && b.mMax.GetY() < 44.5F) {
            std::printf("CUT flight x[%.1f,%.1f] y[%.2f,%.2f] z[%.1f,%.1f]\n", b.mMin.GetX(), b.mMax.GetX(),
                        b.mMin.GetY(), b.mMax.GetY(), b.mMin.GetZ(), b.mMax.GetZ());
            ++removed;
            continue;
        }
        const float wx = b.mMax.GetX() - b.mMin.GetX(), wz = b.mMax.GetZ() - b.mMin.GetZ();
        if (wx < 0.3F || wz < 0.3F) continue;
        tops.push_back({id, ent, b.mMax.GetY(), b.mMin.GetX(), b.mMax.GetX(), b.mMin.GetZ(), b.mMax.GetZ(), rotated});
    }
    std::printf("removed=%d tops=%zu\n", removed, tops.size());
    int pairs = 0;
    for (const auto &s : tops) {
        if (s.y < 39.75F || s.y >= 44.0F) continue;
        for (const auto &t : tops) {
            if (t.y < 44.0F || t.y > s.y + 4.25F) continue;
            const float g = gap(s, t);
            if (g > 4.0F) continue;
            ++pairs;
            std::printf("src ent=%2llu%s top=%6.2f x[%7.2f,%7.2f] z[%8.2f,%8.2f] -> tgt ent=%2llu%s top=%6.2f "
                        "x[%7.2f,%7.2f] z[%8.2f,%8.2f] rise=%.2f gap=%.2f\n",
                        s.ent, s.rotated ? "r" : " ", s.y, s.x0, s.x1, s.z0, s.z1, t.ent, t.rotated ? "r" : " ",
                        t.y, t.x0, t.x1, t.z0, t.z1, t.y - s.y, g);
        }
    }
    std::printf("candidate_pairs=%d\n", pairs);
}
