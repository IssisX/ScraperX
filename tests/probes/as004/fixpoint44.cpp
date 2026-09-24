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
    // Fixpoint over conservative moves, seeded with every top below 40.5 m
    // (everything up to the hall deck is legitimately reachable or treated so).
    // up: rise <= 4.25 (grab reach + hook block), xz gap <= 4.0
    // level/down: drop <= 19.2 (non-lethal from a 1.2 m apex), gap <= range
    const auto range = [](float drop) {
        return 5.5F * (0.5F + std::sqrt(2.0F * (1.2F + drop) / 9.81F)) + 0.7F;
    };
    const size_t n = tops.size();
    std::vector<int> parent(n, -2);   // -2 unreached, -1 seed
    std::vector<size_t> queue;
    // Seeds, each actually standable today: grade-level tops; every AS-001/
    // AS-002 body (entities 44-50, the legitimate route to the hall deck);
    // the tower's decks at 11, 22 and 33 m (its open flights).
    for (size_t i = 0; i < n; ++i) {
        const auto &t = tops[i];
        const bool grade = t.y <= 1.5F;
        const bool ascent = t.ent >= 44 && t.ent <= 50;
        const bool low_deck = t.ent == Simulation::kTowerEntityId && !t.rotated &&
                              (std::abs(t.y - 11.0F) < 0.01F || std::abs(t.y - 22.0F) < 0.01F ||
                               std::abs(t.y - 33.0F) < 0.01F);
        if (grade || ascent || low_deck) { parent[i] = -1; queue.push_back(i); }
    }
    std::vector<size_t> entries;
    for (size_t qi = 0; qi < queue.size(); ++qi) {
        const auto &a = tops[queue[qi]];
        if (a.y >= 44.0F) continue;           // first entries only; do not expand above
        for (size_t j = 0; j < n; ++j) {
            if (parent[j] != -2) continue;
            const auto &b = tops[j];
            const float rise = b.y - a.y, g = gap(a, b);
            const bool ok = rise > 0.0F ? (rise <= 4.25F && g <= 4.0F)
                                        : (-rise <= 19.2F && g <= range(-rise));
            if (!ok) continue;
            parent[j] = (int)queue[qi];
            queue.push_back(j);
            if (b.y >= 44.0F) entries.push_back(j);
        }
    }
    int reached_mid = 0;
    for (size_t i = 0; i < n; ++i) if (parent[i] >= 0 && tops[i].y >= 40.5F && tops[i].y < 44.0F) ++reached_mid;
    std::printf("reached_40.5_to_44=%d first_entries_above_44=%zu\n", reached_mid, entries.size());
    for (size_t e : entries) {
        std::printf("ENTRY ent=%llu%s top=%.2f x[%.2f,%.2f] z[%.2f,%.2f]\n", tops[e].ent, tops[e].rotated ? "r" : "",
                    tops[e].y, tops[e].x0, tops[e].x1, tops[e].z0, tops[e].z1);
        for (int p = parent[e]; p >= 0; p = parent[p])
            std::printf("   <- ent=%llu%s top=%.2f x[%.2f,%.2f] z[%.2f,%.2f]%s\n", tops[p].ent,
                        tops[p].rotated ? "r" : "", tops[p].y, tops[p].x0, tops[p].x1, tops[p].z0, tops[p].z1,
                        parent[p] == -1 ? "  (seed)" : "");
    }
}
