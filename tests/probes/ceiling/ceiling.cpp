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
// The reachable ceiling of the world as built: the highest body top a
// player can reach from grade by any route, under a deliberately generous
// move envelope (up: rise <= 3.75 m measured jump-grab reach + 0.50 m for
// standing on the hook block, horizontal gap <= 4.0 m; level/down: drop
// <= 19.2 m, gap within a running jump's range). An over-approximation, so
// the printed CEILING is an upper bound: nothing above it is reachable.
// Seeds: grade, every AS-001/AS-002 body, and the tower decks its open
// flights reach (11 ... 154 m). Build with tests/probes/as004/build.sh.
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
    for (auto id : ids) {
        const JPH::AABox b = bi.GetTransformedShape(id).GetWorldSpaceBounds();
        const auto ent = (unsigned long long)bi.GetUserData(id);
        const JPH::Quat q = bi.GetRotation(id);
        const bool rotated = std::abs(std::abs(q.GetW()) - 1.0F) > 1e-4F;
        const float wx = b.mMax.GetX() - b.mMin.GetX(), wz = b.mMax.GetZ() - b.mMin.GetZ();
        if (wx < 0.3F || wz < 0.3F) continue;
        tops.push_back({id, ent, b.mMax.GetY(), b.mMin.GetX(), b.mMax.GetX(), b.mMin.GetZ(), b.mMax.GetZ(), rotated});
    }
    std::printf("tops=%zu\n", tops.size());
    // Fixpoint over the envelope from the seeds below.
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
        const bool low_deck = t.ent == Simulation::kTowerEntityId && !t.rotated && t.y <= 154.01F &&
                              std::abs(t.y / 11.0F - std::round(t.y / 11.0F)) < 0.001F;
        if (grade || ascent || low_deck) { parent[i] = -1; queue.push_back(i); }
    }
    std::vector<size_t> entries;
    for (size_t qi = 0; qi < queue.size(); ++qi) {
        const auto &a = tops[queue[qi]];
        for (size_t j = 0; j < n; ++j) {
            if (parent[j] != -2) continue;
            const auto &b = tops[j];
            const float rise = b.y - a.y, g = gap(a, b);
            const bool ok = rise > 0.0F ? (rise <= 4.25F && g <= 4.0F)
                                        : (-rise <= 19.2F && g <= range(-rise));
            if (!ok) continue;
            parent[j] = (int)queue[qi];
            queue.push_back(j);
        }
    }
    size_t best = n;
    int reached = 0;
    for (size_t i = 0; i < n; ++i) {
        if (parent[i] == -2) continue;
        ++reached;
        if (best == n || tops[i].y > tops[best].y) best = i;
    }
    std::printf("reached_tops=%d of %zu\n", reached, n);
    std::printf("CEILING ent=%llu%s top=%.2f x[%.2f,%.2f] z[%.2f,%.2f]\n", tops[best].ent, tops[best].rotated ? "r" : "",
                tops[best].y, tops[best].x0, tops[best].x1, tops[best].z0, tops[best].z1);
    for (int p = parent[best]; p >= 0; p = parent[p])
        std::printf("   <- ent=%llu%s top=%.2f x[%.2f,%.2f] z[%.2f,%.2f]%s\n", tops[p].ent, tops[p].rotated ? "r" : "",
                    tops[p].y, tops[p].x0, tops[p].x1, tops[p].z0, tops[p].z1, parent[p] == -1 ? "  (seed)" : "");
    // the ten highest reached tops that are not tower decks seeded by the stair
    std::vector<size_t> hi;
    for (size_t i = 0; i < n; ++i) if (parent[i] >= 0 && tops[i].y > 150.0F) hi.push_back(i);
    std::sort(hi.begin(), hi.end(), [&](size_t a, size_t b) { return tops[a].y > tops[b].y; });
    for (size_t k = 0; k < hi.size() && k < 10; ++k)
        std::printf("HIGH ent=%llu%s top=%.2f x[%.2f,%.2f] z[%.2f,%.2f]\n", tops[hi[k]].ent, tops[hi[k]].rotated ? "r" : "",
                    tops[hi[k]].y, tops[hi[k]].x0, tops[hi[k]].x1, tops[hi[k]].z0, tops[hi[k]].z1);
}
