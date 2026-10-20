#include "sim/material_traversal_line.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/RegisterTypes.h>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
using namespace JPH;
using namespace scraperx::sim;
constexpr float h = 1.F / 360;
void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
struct BP final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l) const override { return BroadPhaseLayer(l); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "material-line"; }
};
struct BV final : ObjectVsBroadPhaseLayerFilter { bool ShouldCollide(ObjectLayer, BroadPhaseLayer) const override { return false; } };
struct LP final : ObjectLayerPairFilter { bool ShouldCollide(ObjectLayer, ObjectLayer) const override { return false; } };
struct Fixture {
    BP bp; BV bv; LP lp; PhysicsSystem world;
    TempAllocatorImpl allocator{8 * 1024 * 1024}; JobSystemSingleThreaded jobs{1024};
    Body *a = nullptr, *b = nullptr;
    std::unique_ptr<MaterialTraversalLine> line;
    Fixture() {
        world.Init(16, 0, 16, 16, bp, bv, lp); world.SetGravity(Vec3::sZero());
        a = make(RVec3(-3, 0, 0)); b = make(RVec3(3, 0, 0));
        line = std::make_unique<MaterialTraversalLine>(world, BodyID(), 1);
        require(line->install({a->GetID(), a->GetShape(), Vec3(.3F, .4F, 0)},
                             {b->GetID(), b->GetShape(), Vec3(-.3F, .4F, 0)}, 5.42F), "two real off-centre mounts install");
    }
    Body *make(RVec3Arg at) {
        BodyCreationSettings s(new BoxShape(Vec3(.3F, .3F, .3F)), at, Quat::sIdentity(), EMotionType::Dynamic, 1);
        s.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia; s.mMassPropertiesOverride.mMass = 20;
        s.mAllowSleeping = false; s.mLinearDamping = s.mAngularDamping = 0;
        const auto id = world.GetBodyInterface().CreateAndAddBody(s, EActivation::Activate);
        return world.GetBodyLockInterfaceNoLock().TryGetBody(id);
    }
    void step() {
        line->collision_step();
        require(world.Update(h, 1, &allocator, &jobs) == EPhysicsUpdateError::None, "actual Jolt material-line solve");
    }
    Vec3 momentum() const {
        Vec3 p = 20 * (a->GetLinearVelocity() + b->GetLinearVelocity());
        const auto s = line->capture();
        for (const auto &n : s.nodes) p += MaterialTraversalLine::node_mass_kg * n.velocity;
        return p;
    }
};
void reciprocal_material_and_cold_restore() {
    Fixture f;
    const LineMaterialHandle planted{f.line->generation(), f.line->rest_length_m() * .43F};
    LineMaterialSample sample;
    require(f.line->sample_material(planted, sample), "real material coordinate admitted");
    require(sample.endpoints[0].body != sample.endpoints[1].body &&
        std::abs(sample.endpoints[0].weight + sample.endpoints[1].weight - 1) < 1.e-6F, "distributed load weights partition unity");
    sample.endpoints[0].body->AddImpulse(Vec3(0, 1, .4F));
    const Vec3 initial = f.momentum();
    float peak = 0;
    for (unsigned i = 0; i < 180; ++i) {
        f.step(); peak = std::max(peak, f.line->tension_n());
        require(!f.line->broken() && f.line->valid(), "normal material movement stays admitted and below strength");
        require(f.line->sample_material(planted, sample), "planted coordinate survives physical motion");
    }
    require(peak > 5 && peak <= GrappleRopeSettings::strength_n, "real finite tension");
    require(f.a->GetLinearVelocity().Length() > 1.e-4F && f.b->GetLinearVelocity().Length() > 1.e-4F, "both real supports receive load");
    require(f.a->GetAngularVelocity().Length() > 1.e-4F && f.b->GetAngularVelocity().Length() > 1.e-4F, "off-centre mounts turn both supports");
    require((f.momentum() - initial).Length() < 1.e-3F, "material solve preserves reciprocal linear momentum");
    const auto saved = f.line->capture();
    const auto old_generation = f.line->generation();
    f.line->stop();
    require(!f.line->sample_material(planted, sample), "removed line invalidates planted identity");
    require(f.line->restore(saved), "cold restore rebuilds topology and real node state");
    require(f.line->generation() != old_generation && !f.line->sample_material(planted, sample), "restore cannot resurrect stale hands");
    const auto restored = f.line->capture();
    for (unsigned i = 0; i < MaterialTraversalLine::node_count; ++i) {
        require((saved.nodes[i].position - restored.nodes[i].position).LengthSq() == 0, "cold restore preserves material position");
        require((saved.nodes[i].velocity - restored.nodes[i].velocity).LengthSq() == 0, "cold restore preserves material velocity");
    }
    LineMaterialHandle nearest; RVec3 at;
    require(f.line->nearest_material(sample.world_point, 10, nearest, at), "reachable material can be found after restore");
    require(nearest.generation == f.line->generation(), "new material selection uses actual generation");
    require(!f.line->sample_material({f.line->generation(), -1}, sample) &&
            !f.line->sample_material({f.line->generation(), 19}, sample), "material range cannot manufacture support");
    std::cout << "PASS MATERIAL_TRAVERSAL_LINE two_real_mounts=1 persistent_nodes=2 reciprocal_load=1 cold_restore=1 peak_n=" << peak
              << " momentum_error_kg_mps=" << (f.momentum() - initial).Length() << '\n';
}
}
int main() {
    RegisterDefaultAllocator(); Factory::sInstance = new Factory; RegisterTypes();
    int code = 0;
    try { reciprocal_material_and_cold_restore(); }
    catch (const std::exception &e) { std::cerr << "FAIL MATERIAL_TRAVERSAL_LINE " << e.what() << '\n'; code = 1; }
    UnregisterTypes(); delete Factory::sInstance; Factory::sInstance = nullptr; return code;
}
