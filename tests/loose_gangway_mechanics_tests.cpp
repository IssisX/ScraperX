#include "sim/loose_gangway_route.hpp"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace {
using namespace JPH;
using namespace scraperx::sim;
constexpr float dt = 1.0F / 90.0F;
void require(bool value, const char *message) {
    if (value) return;
    std::cerr << "FAIL LOOSE_GANGWAY_MECHANICS " << message << '\n';
    std::exit(EXIT_FAILURE);
}
struct BP final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override { return BroadPhaseLayer(layer); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "loose_route"; }
};
struct BV final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override {
        return a != 0 || b.GetValue() != 0;
    }
};
struct LP final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override { return a != 0 || b != 0; }
};
struct Fixture final {
    BP bp; BV bv; LP lp;
    PhysicsSystem system;
    TempAllocatorImpl allocator{8 * 1024 * 1024};
    JobSystemSingleThreaded jobs{1024};
    BodyID floor, load;
    std::unique_ptr<kit::Kit> parts;
    Fixture() {
        system.Init(64, 0, 128, 128, bp, bv, lp);
        BodyCreationSettings ground(new BoxShape(Vec3(20, .5F, 20)),
            kLooseGangwayBay + Vec3(0, -.5F, 0), Quat::sIdentity(), EMotionType::Static, 0);
        floor = system.GetBodyInterface().CreateAndAddBody(ground, EActivation::DontActivate);
        parts = std::make_unique<kit::Kit>(system, 0, 1);
        build_loose_gangway_route(*parts);
    }
    ~Fixture() {
        auto &bodies = system.GetBodyInterface();
        if (!load.IsInvalid()) { bodies.RemoveBody(load); bodies.DestroyBody(load); }
        parts.reset();
        bodies.RemoveBody(floor); bodies.DestroyBody(floor);
    }
    kit::BodyIndex board() const { return parts->body_for_entity(kLooseGangwayEntity); }
    void step(unsigned count) {
        for (unsigned i = 0; i < count; ++i) {
            parts->pre_step(dt);
            require(system.Update(dt, 4, &allocator, &jobs) == EPhysicsUpdateError::None,
                    "Jolt contact step completes");
            parts->post_step(dt);
        }
    }
    void put_load(Vec3 offset) {
        BodyCreationSettings settings(new BoxShape(Vec3(.2F, .2F, .2F), .005F),
            kLooseGangwayBay + offset, Quat::sIdentity(), EMotionType::Dynamic, 1);
        settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = 85;
        settings.mFriction = .7F;
        load = system.GetBodyInterface().CreateAndAddBody(settings, EActivation::Activate);
    }
};

void balanced_load_falls() {
    Fixture f;
    f.step(180);
    const auto initial = f.parts->body_position(f.board());
    require(std::abs(initial.GetY() - .46) < .025 &&
            std::abs((f.parts->body_rotation(f.board()) * Vec3::sAxisX()).GetY()) < .025,
            "unloaded board rests level on its loose crib");
    require(f.system.GetConstraints().empty(), "neither board nor crib has a hidden attachment");
    // Actual off-center weight, with ordinary collision enabled. Gravity
    // and contact reactions must topple the assembly without a tip script.
    f.put_load({kLooseGangwayStorageX - 1.5F, .80F, 0});
    float peak_tilt = 0;
    float lowest_end = 10;
    for (unsigned i = 0; i < 240; ++i) {
        f.step(1);
        const auto axis = f.parts->body_rotation(f.board()) * Vec3::sAxisX();
        peak_tilt = std::max(peak_tilt, std::abs(axis.GetY()));
        lowest_end = std::min(lowest_end, f.parts->body_position(f.board()).GetY() -
            kLooseGangwayHalfLength * std::abs(axis.GetY()));
    }
    require(peak_tilt > .08F && lowest_end < .15,
            "off-center real load tips an unsupported end down to the yard");
    std::cout << "PASS LOOSE_GANGWAY_MECHANICS balanced-load tilt=" << peak_tilt
              << " low_end_m=" << lowest_end << '\n';
}

void two_supports_carry_load() {
    Fixture f;
    auto &bodies = f.system.GetBodyInterface();
    // Diagnostic setup only. Shipping play uses ordinary pickup/drop; the
    // public-path test separately checks that complete interaction.
    bodies.SetPositionAndRotation(f.parts->body_id(f.board()),
        kLooseGangwayBay + Vec3(0, kLooseGangwayDockTop + .06F, 0),
        Quat::sIdentity(), EActivation::Activate);
    f.put_load({0, .75F, 0});
    f.step(240);
    const auto position = f.parts->body_position(f.board());
    const auto axis = f.parts->body_rotation(f.board()) * Vec3::sAxisX();
    require(std::abs(position.GetY() - kLooseGangwayDockTop - .06F) < .025 &&
            std::abs(axis.GetY()) < .025F &&
            bodies.GetPosition(f.load).GetY() > .65,
            "two actual bearings support the board and an 85kg center load");
    require(f.system.GetConstraints().empty() && f.parts->body_dynamic(f.board()),
            "placed board stays freely dynamic without a puzzle joint");
    std::cout << "PASS LOOSE_GANGWAY_MECHANICS two-supports board_y_m=" << position.GetY()
              << " load_y_m=" << bodies.GetPosition(f.load).GetY() << '\n';
}
} // namespace

int main() {
    RegisterDefaultAllocator();
    Factory::sInstance = new Factory;
    RegisterTypes();
    balanced_load_falls();
    two_supports_carry_load();
    UnregisterTypes();
    delete Factory::sInstance;
    Factory::sInstance = nullptr;
}
