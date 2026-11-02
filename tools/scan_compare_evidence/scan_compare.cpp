// Throwaway (never committed): compares the Matron's AffordanceMap::scan
// between builds. Linked with -Wl,--wrap=<scan> so every scan() call in the
// process (the world's own and this file's) is timed on the monotonic clock,
// its input steel fingerprinted, and its whole output dumped bit for bit.
#include "sim/simulation.hpp"
#include "sim/matron.hpp"
#include "sim/matron_traversal.hpp"
#include "sim/physics_world_support.hpp"

#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/StreamOut.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseQuery.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Geometry/RayAABox.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace JPH;
using namespace scraperx::sim;
using namespace scraperx::sim::detail;
namespace traversal = scraperx::sim::traversal;

extern "C" void __real__ZN8scraperx3sim9traversal13AffordanceMap4scanERKN3JPH13PhysicsSystemERKNS3_5AABoxERKNS1_6IgnoreERKNS1_6LimitsE(
    traversal::AffordanceMap *, const PhysicsSystem &, const AABox &, const traversal::Ignore &, const traversal::Limits &);

namespace {
std::string g_out_dir = ".";
std::string g_label = "scan";
int g_index = 0;
struct LastCall {
    const PhysicsSystem *physics = nullptr;
    AABox bounds;
    traversal::Ignore ignore;
    traversal::Limits limits;
} g_last;

std::uint64_t fnv(const std::string &s) {
    std::uint64_t h = 1469598103934665603ULL;
    for (const unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}
std::string hx(const float f) {
    std::uint32_t u;
    std::memcpy(&u, &f, 4);
    char b[16];
    std::snprintf(b, sizeof b, "%08x", u);
    return b;
}
std::string hv(const Vec3 v) { return hx(v.GetX()) + "," + hx(v.GetY()) + "," + hx(v.GetZ()); }

class StrOut final : public StreamOut {
public:
    explicit StrOut(std::string &s) : s_(s) {}
    void WriteBytes(const void *d, size_t n) override { s_.append(static_cast<const char *>(d), n); }
    bool IsFailed() const override { return false; }

private:
    std::string &s_;
};

// What the scan can see, written out independently of either build's key:
// every non-dynamic, non-sensor body in the broadphase, ID order, with its
// place and whole shape; and the call's own bounds, ignore and limits.
std::string fingerprint(const PhysicsSystem &physics, const AABox &bounds, const traversal::Ignore &ignore,
                        const traversal::Limits &limits, int &fixed_bodies) {
    std::string s;
    StrOut out(s);
    out.Write(bounds.mMin);
    out.Write(bounds.mMax);
    out.Write(ignore.self.GetIndexAndSequenceNumber());
    out.Write(ignore.player.GetIndexAndSequenceNumber());
    out.Write(ignore.layer);
    out.WriteBytes(&limits, sizeof limits);
    BodyIDVector ids;
    physics.GetBodies(ids);
    std::sort(ids.begin(), ids.end());
    Shape::ShapeToIDMap shapes;
    Shape::MaterialToIDMap materials;
    fixed_bodies = 0;
    for (const BodyID id : ids) {
        BodyLockRead lock(physics.GetBodyLockInterface(), id);
        if (!lock.Succeeded()) continue;
        const Body &b = lock.GetBody();
        if (b.IsDynamic() || b.IsSensor() || !b.IsInBroadPhase()) continue;
        ++fixed_bodies;
        out.Write(id.GetIndexAndSequenceNumber());
        out.Write(static_cast<std::uint8_t>(b.GetMotionType()));
        out.Write(b.GetObjectLayer());
        out.Write(b.GetUserData());
        out.Write(b.GetCenterOfMassPosition());
        out.Write(b.GetRotation());
        b.GetShape()->SaveWithChildren(out, shapes, materials);
    }
    return s;
}

std::string dump(const traversal::AffordanceMap &m) {
    std::ostringstream o;
    const traversal::ScanStats &st = m.stats();
    o << "stats bodies=" << st.bodies << " leaves=" << st.leaves << " patches=" << st.patches
      << " usable_patches=" << st.usable_patches << " bars=" << st.bars << " ladders=" << st.ladders
      << " poles=" << st.poles << " regions=" << st.regions << " edges=" << st.edges
      << " usable_edges=" << st.usable_edges << " capsule_queries=" << st.capsule_queries
      << " split_patches=" << st.split_patches << "\n";
    for (const traversal::Patch &p : m.patches()) {
        o << "patch " << p.body.GetIndexAndSequenceNumber() << " " << p.sub.GetValue() << " " << hx(p.top) << " "
          << hx(p.min_x) << " " << hx(p.max_x) << " " << hx(p.min_z) << " " << hx(p.max_z) << " r" << p.region
          << " n" << p.narrow << " s" << p.split << " so" << p.split_of << "\n";
    }
    for (const traversal::Region &r : m.regions()) {
        o << "region " << r.id << " " << hx(r.top) << " " << hx(r.min_x) << " " << hx(r.max_x) << " " << hx(r.min_y)
          << " " << hx(r.max_y) << " " << hx(r.min_z) << " " << hx(r.max_z) << " " << hx(r.area) << " [";
        for (const int p : r.patches) o << p << ",";
        o << "]\n";
    }
    for (const traversal::Edge &e : m.edges()) {
        const traversal::MoveSpec &s = e.spec;
        o << "edge " << e.from << ">" << e.to << " " << traversal::move_name(e.kind) << " u" << e.usable << " c"
          << hx(e.cost) << " spec " << traversal::move_name(s.kind) << " L" << hv(s.line) << " O" << hv(s.out)
          << " b" << hx(s.bottom_y) << " t" << hx(s.top_y) << " S" << hv(s.start) << " G" << hv(s.goal) << " gt"
          << hx(s.goal_top) << " bh" << hx(s.bar_half) << " | " << e.reason << "\n";
    }
    for (const traversal::Ladder &l : m.ladders()) {
        o << "ladder " << hx(l.x) << " " << hx(l.z) << " " << l.along_x << " " << hx(l.half) << " " << hx(l.bottom)
          << " " << hx(l.top) << " " << hx(l.pitch) << " " << l.rungs << " " << l.entity << "\n";
    }
    for (const traversal::Pole &p : m.poles()) {
        o << "pole " << hx(p.x) << " " << hx(p.z) << " " << hx(p.section) << " " << hx(p.bottom) << " " << hx(p.top)
          << "\n";
    }
    // The planner and the walk over what was built (patch links included).
    std::vector<int> big;
    for (const traversal::Region &r : m.regions()) big.push_back(r.id);
    std::sort(big.begin(), big.end(), [&](const int a, const int b) {
        const float aa = m.regions()[std::size_t(a)].area, ab = m.regions()[std::size_t(b)].area;
        return aa != ab ? aa > ab : a < b;
    });
    if (big.size() > 16) big.resize(16);
    for (const int a : big)
        for (const int b : big) {
            if (a == b) continue;
            o << "plan " << a << ">" << b << " [";
            for (const int k : m.plan(a, b)) o << k << ",";
            o << "]\n";
        }
    for (const traversal::Region &r : m.regions()) {
        if (r.patches.size() < 2) continue;
        const traversal::Patch &p0 = m.patches()[std::size_t(r.patches.front())];
        const traversal::Patch &p1 = m.patches()[std::size_t(r.patches.back())];
        const Vec3 from(0.5F * (p0.min_x + p0.max_x), p0.top, 0.5F * (p0.min_z + p0.max_z));
        const Vec3 to(0.5F * (p1.min_x + p1.max_x), p1.top, 0.5F * (p1.min_z + p1.max_z));
        o << "walk " << r.id << " [";
        for (const Vec3 w : m.walk_path(from, to)) o << hv(w) << ";";
        o << "]\n";
    }
    return o.str();
}
} // namespace

extern "C" void __wrap__ZN8scraperx3sim9traversal13AffordanceMap4scanERKN3JPH13PhysicsSystemERKNS3_5AABoxERKNS1_6IgnoreERKNS1_6LimitsE(
    traversal::AffordanceMap *self, const PhysicsSystem &physics, const AABox &bounds, const traversal::Ignore &ignore,
    const traversal::Limits &limits) {
    int fixed = 0;
    const std::uint64_t world = fnv(fingerprint(physics, bounds, ignore, limits, fixed));
    const auto t0 = std::chrono::steady_clock::now();
    __real__ZN8scraperx3sim9traversal13AffordanceMap4scanERKN3JPH13PhysicsSystemERKNS3_5AABoxERKNS1_6IgnoreERKNS1_6LimitsE(
        self, physics, bounds, ignore, limits);
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    g_last = {&physics, bounds, ignore, limits};
    const std::string text = dump(*self);
    int freedrop = 0, freedrop_usable = 0, ledge = 0, ledge_usable = 0;
    for (const traversal::Edge &e : self->edges()) {
        if (e.kind == traversal::MoveKind::FreeDrop) {
            ++freedrop;
            freedrop_usable += e.usable ? 1 : 0;
        }
        if (e.kind == traversal::MoveKind::LedgeJump) {
            ++ledge;
            ledge_usable += e.usable ? 1 : 0;
        }
    }
    const int index = g_index++;
    char name[64];
    std::snprintf(name, sizeof name, "%02d", index);
    const std::string path = g_out_dir + "/" + name + "_" + g_label + ".txt";
    std::ofstream(path) << text;
    std::printf("SCAN %02d %-22s wall_s=%.4f stats_s=%.4f world=%016llx fixed_bodies=%d patches=%zu regions=%zu "
                "edges=%zu usable=%u ladders=%zu poles=%zu free_drop=%d/%d ledge_jump=%d/%d graph=%016llx\n",
                index, g_label.c_str(), secs, self->stats().seconds, static_cast<unsigned long long>(world), fixed,
                self->patches().size(), self->regions().size(), self->edges().size(), self->stats().usable_edges,
                self->ladders().size(), self->poles().size(), freedrop_usable, freedrop, ledge_usable, ledge,
                static_cast<unsigned long long>(fnv(text)));
    std::fflush(stdout);
}

namespace {
// The descent exam's course (tests/sim_tests_matron_exam_descent.cpp),
// copied: its static steel only, no Matron.
struct Course {
    float gap = 0, jump_drop = 0, rung_pitch = 0, rung_x = 0, pole_x = 0, ledge_top = 0, pole_foot_top = 0,
          recovery_top = 0, jump_top = 0, start_top = 0;
    Vec3 origin = Vec3::sZero();
    int turns = 0;
    [[nodiscard]] float ladder_z() const { return gap + 3.05F; }
    [[nodiscard]] float pole_z() const { return gap + 6.10F; }
    [[nodiscard]] Vec3 rotate(const Vec3 v) const {
        switch (turns) {
        case 1: return Vec3(-v.GetZ(), v.GetY(), v.GetX());
        case 2: return Vec3(-v.GetX(), v.GetY(), -v.GetZ());
        case 3: return Vec3(v.GetZ(), v.GetY(), -v.GetX());
        default: return v;
        }
    }
    [[nodiscard]] RVec3 world(const Vec3 local) const { return RVec3(origin + rotate(local)); }
};
Course course_for(const std::uint32_t seed) {
    std::uint32_t state = seed * 2654435761U + 0x9E3779B9U;
    const auto uniform = [&](const float lo, const float hi) {
        state = state * 1664525U + 1013904223U;
        const float unit = static_cast<float>(state >> 8U) / 16777216.0F;
        return lo + (hi - lo) * unit;
    };
    Course c;
    c.gap = uniform(0.80F, 1.10F);
    c.jump_drop = uniform(0.55F, 0.70F);
    c.rung_pitch = uniform(0.27F, 0.33F);
    c.rung_x = uniform(-0.35F, 0.35F);
    c.pole_x = uniform(-0.35F, 0.35F);
    c.ledge_top = uniform(0.80F, 0.90F);
    c.pole_foot_top = c.ledge_top + uniform(0.80F, 0.90F);
    c.recovery_top = c.pole_foot_top + uniform(4.75F, 4.90F);
    c.jump_top = c.recovery_top + uniform(4.75F, 4.90F);
    c.start_top = c.jump_top + c.jump_drop;
    c.origin = Vec3(uniform(30.0F, 50.0F), uniform(0.0F, 2.0F), uniform(30.0F, 50.0F));
    c.turns = static_cast<int>(seed % 4U);
    return c;
}
struct Steel {
    detail::JoltRuntimeLease lease;
    detail::BroadPhaseLayerInterface broad;
    detail::ObjectVsBroadPhaseFilter vs;
    detail::ObjectLayerPairFilter pair;
    PhysicsSystem physics;
    Course course;
    std::vector<BodyID> bodies;
    explicit Steel(const std::uint32_t seed) : course(course_for(seed)) {
        physics.Init(2048, 0, 4096, 2048, broad, vs, pair);
        physics.SetGravity(Vec3(0.0F, -9.81F, 0.0F));
        const Course &c = course;
        platform(-2.8F, 0.0F, c.start_top);
        platform(c.gap, c.gap + 3.0F, c.jump_top);
        platform(c.gap + 3.0F, c.gap + 6.0F, c.recovery_top);
        platform(c.gap + 6.0F, c.gap + 9.0F, c.pole_foot_top);
        platform(c.gap + 9.0F, c.gap + 11.0F, c.ledge_top);
        platform(c.gap + 11.0F, c.gap + 14.0F, 0.0F);
        for (float y = c.recovery_top + 0.25F; y <= c.jump_top - 0.10F; y += c.rung_pitch)
            box(Vec3(0.55F, 0.025F, 0.025F), Vec3(c.rung_x, y, c.ladder_z()));
        const float pole_height = c.recovery_top - 0.05F - c.pole_foot_top;
        box(Vec3(0.05F, 0.5F * pole_height, 0.05F), Vec3(c.pole_x, c.pole_foot_top + 0.5F * pole_height, c.pole_z()));
    }
    ~Steel() {
        BodyInterface &b = physics.GetBodyInterface();
        for (const BodyID id : bodies) {
            if (b.IsAdded(id)) b.RemoveBody(id);
            b.DestroyBody(id);
        }
    }
    BodyID box(Vec3 half, const Vec3 at) {
        if ((course.turns & 1) != 0) half = Vec3(half.GetZ(), half.GetY(), half.GetX());
        const float convex = std::min(0.05F, 0.5F * half.ReduceMin());
        BodyCreationSettings settings(new BoxShape(half, convex), course.world(at), Quat::sIdentity(),
                                      EMotionType::Static, object_layers::kStatic);
        settings.mFriction = 0.8F;
        const BodyID id = physics.GetBodyInterface().CreateAndAddBody(settings, EActivation::DontActivate);
        bodies.push_back(id);
        return id;
    }
    BodyID platform(const float z0, const float z1, const float top) {
        return box(Vec3(1.5F, 0.10F, 0.5F * (z1 - z0)), Vec3(0.0F, top - 0.10F, 0.5F * (z0 + z1)));
    }
    void scan(const std::string &label) {
        g_label = label;
        traversal::AffordanceMap map;
        map.scan(physics, physics.GetBounds(), traversal::Ignore{BodyID(), BodyID(), int(object_layers::kMoving)});
    }
};

void tower(const int worlds) {
    for (int w = 0; w < worlds; ++w) {
        Simulation s(InitialSpawn::Deck154);
        s.place_matron_home(154.0F);
        g_label = "tower_world" + std::to_string(w);
        (void)s.advance_frame(0.1);
        const traversal::AffordanceMap &map = s.matron_affordances();
        (void)map;
        if (w == 0 && g_last.physics != nullptr) {
            // The same steel read again in the same world.
            g_label = "tower_again";
            traversal::AffordanceMap again;
            again.scan(*g_last.physics, g_last.bounds, g_last.ignore, g_last.limits);
        }
    }
}

void worlds() {
    for (std::uint32_t seed = 1; seed <= 12; ++seed) {
        Steel w(seed);
        w.scan("exam_seed" + std::to_string(seed));
    }
    // The free-drop regressions' steel (seed 4 course plus their members).
    for (const bool landing : {false, true}) {
        Steel w(4);
        w.box(Vec3(landing ? 10.0F : 2.9F, 0.10F, landing ? 10.0F : 2.9F), Vec3(40.0F, -0.10F, 0.0F));
        const BodyID upper = w.box(Vec3(2.5F, 0.10F, 2.5F), Vec3(40.0F, 10.90F, 0.0F));
        w.scan(landing ? "free_drop_landing" : "free_drop_narrow");
        if (landing) {
            // The upper deck taken out (RemoveBody only, not destroyed), then
            // the same steel read again: no deck, no drop off it.
            w.physics.GetBodyInterface().RemoveBody(upper);
            w.scan("free_drop_deck_removed");
            w.physics.GetBodyInterface().AddBody(upper, EActivation::DontActivate);
            w.scan("free_drop_deck_back");
        }
    }
    {
        Steel w(4);
        w.box(Vec3(10.0F, 0.10F, 10.0F), Vec3(40.0F, -0.10F, 0.0F));
        w.box(Vec3(2.5F, 0.10F, 2.5F), Vec3(40.0F, 10.90F, 0.0F));
        for (const float sign : {-1.0F, 1.0F}) {
            w.box(Vec3(0.14F, 0.16F, 2.5F), Vec3(40.0F + sign * 4.4F, 9.50F, 0.0F));
            w.box(Vec3(2.5F, 0.16F, 0.14F), Vec3(40.0F, 9.50F, sign * 4.4F));
        }
        w.scan("free_drop_overhead");
    }
}
} // namespace

#include "diag.inc"

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s tower|worlds <out_dir> [tower_worlds]\n", argv[0]);
        return 2;
    }
    g_out_dir = argv[2];
    const std::string mode = argv[1];
    if (mode == "tower") tower(argc > 3 ? std::atoi(argv[3]) : 1);
    if (mode == "worlds") worlds();
    if (mode == "diag") diag();
    return 0;
}
