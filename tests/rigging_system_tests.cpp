#include "sim/rigging_system.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/RegisterTypes.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace {
using namespace JPH;
using namespace scraperx::sim;
constexpr float dt = 1.F / 360;
void require(bool value, const char *why) {
    if (!value) { std::cerr << "FAIL RIGGING_SYSTEM " << why << '\n'; std::exit(1); }
}
struct BP final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l) const override { return BroadPhaseLayer(l); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "rigging"; }
};
struct BF final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override { return a != 0 || b.GetValue() != 0; }
};
struct LF final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override { return a != 0 || b != 0; }
};
struct Fixture {
    BP bp; BF bf; LF lf; PhysicsSystem world;
    TempAllocatorImpl temp{8 * 1024 * 1024}; JobSystemSingleThreaded jobs{1024};
    std::unique_ptr<kit::Kit> parts;
    std::unique_ptr<RiggingSystem> rig;
    kit::BodyIndex first, second, support;
    explicit Fixture(float second_mass = 200) {
        world.Init(64, 0, 128, 128, bp, bf, lf);
        parts = std::make_unique<kit::Kit>(world, 0, 1);
        first = box(2000, RVec3(-2, 0, 0), 100);
        second = box(2001, RVec3(2, 0, 0), second_mass);
        support = box(1000, RVec3(0, 5, 0), 0);
        rig = std::make_unique<RiggingSystem>(world, *parts);
        rig->add_eye(first, Vec3(0, .3F, 0), 100000);
        rig->add_eye(second, Vec3(0, .3F, 0), 100000);
        rig->add_fixed_guide(support, Vec3(0, 0, 1), Vec3(-2, 0, 0), Vec3(2, 0, 0), 100000);
        rig->equip(true);
    }
    ~Fixture() { rig.reset(); parts.reset(); }
    kit::BodyIndex box(std::uint64_t entity, RVec3 at, float mass) {
        kit::Part p; p.half = Vec3(.2F, .2F, .2F);
        auto b = parts->add_body(entity, {p}, at, Quat::sIdentity(), mass, .5F);
        if (mass > 0) parts->set_damping(b, 0, 0);
        return b;
    }
    RVec3 eye(unsigned id) const {
        if (id == 3) return parts->body_position(support) + Vec3(0, 0, 1);
        return parts->body_position(id == 1 ? first : second) + Vec3(0, .3F, 0);
    }
    std::uint64_t aim(unsigned id) {
        // Owner-fixture staging of the real hand; no player-route assertion.
        rig->aim(eye(id) + Vec3(0, 0, 1), -Vec3::sAxisZ(), BodyID(), true);
        const auto token = rig->readback().target;
        require(token != 0 && unsigned(token & 0xffffffffU) == id, "actual aim identifies authored eye");
        return token;
    }
    void action(unsigned id) { require(rig->action(aim(id)), "real tool action succeeds"); }
    void install() { action(1); action(3); action(2); }
    void tick(unsigned count) {
        while (count--) {
            parts->pre_step(dt);
            require(world.Update(dt, 1, &temp, &jobs) == EPhysicsUpdateError::None, "Jolt update");
            parts->post_step(dt); rig->post_step(dt);
        }
    }
};
void draft_and_loaded_cut() {
    Fixture f;
    f.world.SetGravity(Vec3::sZero());
    const auto bodies = f.parts->body_count(), ropes = f.parts->rope_count();
    f.action(1); f.action(3);
    require(f.rig->readback().draft && f.rig->readback().guide_threaded && f.world.GetConstraints().empty(),
            "draft is topology only, with no constraint");
    f.tick(20);
    require(f.parts->body_velocity(f.first).LengthSq() == 0 && f.parts->body_velocity(f.second).LengthSq() == 0,
            "draft applies no force");
    f.action(2);
    require(f.world.GetConstraints().size() == 1 && f.rig->readback().installed_count == 1,
            "final endpoint creates one real constraint");
    f.world.SetGravity(Vec3(0, -9.81F, 0));
    f.tick(180);
    const float a = f.parts->body_position(f.first).GetY(), b = f.parts->body_position(f.second).GetY();
    const float tension = f.rig->readback().tension_n;
    require(a > .15F && b < -.15F && std::abs(a + b) < .04F,
            "heavier end descends and lifts lighter end through fixed guide");
    require(tension > 60 && std::isfinite(tension), "solved physical tension exceeds unclip limit");
    const auto handle = f.rig->line_handle(0);
    f.aim(1);
    require(!f.rig->unclip(handle) && f.rig->readback().failure == RiggingSystem::Failure::Loaded &&
            f.world.GetConstraints().size() == 1, "ordinary unclip rejects measured loaded line");
    const auto va = f.parts->body_velocity(f.first), vb = f.parts->body_velocity(f.second);
    require(f.rig->cut(handle) && f.world.GetConstraints().empty(), "deliberate cut removes tensile row");
    require((f.parts->body_velocity(f.first) - va).LengthSq() == 0 &&
            (f.parts->body_velocity(f.second) - vb).LengthSq() == 0, "cut preserves solved endpoint velocities");
    f.world.SetGravity(Vec3::sZero()); f.tick(1);
    require((f.parts->body_velocity(f.first) - va).Length() < 1.e-5F &&
            (f.parts->body_velocity(f.second) - vb).Length() < 1.e-5F, "cut leaves no hidden braking row");
    require(f.parts->body_count() == bodies && f.parts->rope_count() == ropes, "rigging does not mutate Kit inventory");
    std::cout << "RIGGING_COUNTERWEIGHT rise_m=" << a << " descent_m=" << b << " tension_n=" << tension << '\n';
}
void obstruction_and_capacity() {
    Fixture f; f.world.SetGravity(Vec3::sZero()); f.install();
    const auto existing = f.rig->capture().lines[0];
    f.box(1001, RVec3(-2, 2, 0), 0);
    f.action(1); f.action(3);
    require(!f.rig->action(f.aim(2)) && f.rig->readback().failure == RiggingSystem::Failure::Obstructed,
            "solid span blocks new installation");
    require(f.world.GetConstraints().size() == 1 && f.rig->capture().lines[0].paid_length_m == existing.paid_length_m,
            "blocked installation preserves existing rig");
    Fixture full; full.world.SetGravity(Vec3::sZero());
    for (std::size_t i = 0; i < RiggingSystem::kCapacity; ++i) full.install();
    full.action(1); full.action(3);
    require(!full.rig->action(full.aim(2)) && full.rig->readback().failure == RiggingSystem::Failure::Capacity &&
            full.world.GetConstraints().size() == RiggingSystem::kCapacity &&
            full.rig->readback().installed_count == RiggingSystem::kCapacity,
            "capacity failure preserves all installed lines");
}
void checkpoint_replacement_and_stale_handles() {
    Fixture f; f.world.SetGravity(Vec3::sZero()); f.install(); f.tick(1);
    kit::Kit::Checkpoint bodies; f.parts->capture(bodies);
    const auto graph = f.rig->capture();
    const auto abandoned = f.rig->line_handle(0);
    const auto old_eye = f.aim(1);
    require(f.rig->unclip(abandoned), "solved unloaded line can unclip");
    f.install();
    const auto replacement = f.rig->line_handle(0);
    require(replacement != abandoned && !f.rig->select(abandoned), "slot replacement rejects generation-stale handle");
    require(f.world.GetConstraints().size() == 1, "replacement has one row");
    const auto count = f.parts->body_count(), ropes = f.parts->rope_count();
    f.rig->detach_for_restore();
    require(f.world.GetConstraints().empty(), "rig rows destroyed before restoring Kit bodies");
    f.parts->restore(bodies);
    f.rig->restore(graph);
    require(f.world.GetConstraints().size() == 1 && f.rig->readback().installed_count == 1,
            "restored graph creates exactly one row after bodies");
    const auto restored = f.rig->capture().lines[0];
    require(restored.first == graph.lines[0].first && restored.second == graph.lines[0].second &&
            restored.guide == graph.lines[0].guide && restored.reversed == graph.lines[0].reversed &&
            restored.paid_length_m == graph.lines[0].paid_length_m,
            "checkpoint preserves authored topology and paid length");
    f.rig->equip(true); f.aim(1);
    require(!f.rig->action(old_eye) && f.rig->readback().failure == RiggingSystem::Failure::StaleTarget,
            "restore epoch rejects stale aimed eye token");
    require(!f.rig->select(replacement) && !f.rig->cut(abandoned), "restore rejects abandoned line handles");
    require(f.rig->select(f.rig->line_handle(0)), "restored current line is selectable");
    f.rig->restore(graph);
    require(f.world.GetConstraints().size() == 1 && f.parts->body_count() == count && f.parts->rope_count() == ropes,
            "repeated restore does not duplicate rows, bodies or Kit rope inventory");
}
void sleeping_cut(bool kit_rope) {
    Fixture f(100);
    kit::RopeIndex rope;
    if (kit_rope) {
        rope = f.parts->add_rope(f.first, Vec3(0,.3F,0), RVec3(-2,5,0),
            f.second, Vec3(0,.3F,0), RVec3(2,5,0), 1, 9.4F, 0);
    } else f.install();
    f.tick(360);
    auto &bodies = f.world.GetBodyInterface();
    const auto first = f.parts->body_id(f.first), second = f.parts->body_id(f.second);
    require(!bodies.IsActive(first) && !bodies.IsActive(second),
            "balanced suspended loads enter ordinary native sleep");
    const auto before = f.parts->body_position(f.first).GetY();
    if (kit_rope) f.parts->set_rope_connected(rope, false);
    else require(f.rig->cut(f.rig->line_handle(0)), "sleeping loaded line can be cut");
    require(bodies.IsActive(first) && bodies.IsActive(second), "cut wakes both sleeping endpoints");
    f.tick(36);
    require(f.parts->body_position(f.first).GetY() < before - .03F &&
            f.parts->body_position(f.second).GetY() < before - .03F,
            "cut sleeping loads fall under ordinary gravity");
    std::cout << "RIGGING_SLEEP kit_rope=" << kit_rope << " native_sleep=1 cut_wake=1 gravity_fall=1\n";
}
}
int main() {
    RegisterDefaultAllocator(); Factory::sInstance = new Factory; RegisterTypes();
    draft_and_loaded_cut(); obstruction_and_capacity(); checkpoint_replacement_and_stale_handles(); sleeping_cut(false); sleeping_cut(true);
    UnregisterTypes(); delete Factory::sInstance; Factory::sInstance = nullptr;
    std::cout << "PASS RIGGING_SYSTEM real_jolt_kit=1 public_player_route=unverified\n";
}
