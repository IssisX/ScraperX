#include "sim/portable_traversal_line.hpp"
#include "sim/rigging_system.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/RegisterTypes.h>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
using namespace JPH;
using namespace scraperx::sim;
constexpr float dt = 1.F / 360;
void require(bool value, const char *why) {
    if (!value) throw std::runtime_error(why);
}
struct Layers final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l) const override { return BroadPhaseLayer(l); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "portable-line"; }
};
struct BroadFilter final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override { return a || b.GetValue(); }
};
struct PairFilter final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override { return a || b; }
};
struct Fixture {
    Layers layers; BroadFilter broad; PairFilter pair;
    PhysicsSystem world;
    TempAllocatorImpl allocator{8 * 1024 * 1024}; JobSystemSingleThreaded jobs{1024};
    BodyID player, beam;
    std::unique_ptr<PhysicalHandClimb> hands;
    std::unique_ptr<PortableTraversalLine> line;
    Fixture() {
        world.Init(32, 0, 64, 64, layers, broad, pair);
        auto &bodies = world.GetBodyInterface();
        BodyCreationSettings p(new CapsuleShape(.55F, .35F), RVec3(0, 1.5F, 0),
            Quat::sIdentity(), EMotionType::Dynamic, 1);
        p.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        p.mMassPropertiesOverride.mMass = 85;
        p.mAllowedDOFs = EAllowedDOFs::TranslationX | EAllowedDOFs::TranslationY | EAllowedDOFs::TranslationZ;
        p.mAllowSleeping = false;
        player = bodies.CreateAndAddBody(p, EActivation::Activate);
        beam = bodies.CreateAndAddBody(BodyCreationSettings(new BoxShape(Vec3(3, .1F, .1F)),
            RVec3(0, 3.7F, 0), Quat::sIdentity(), EMotionType::Static, 0), EActivation::DontActivate);
        bodies.CreateAndAddBody(BodyCreationSettings(new BoxShape(Vec3(10, .1F, 10)),
            RVec3(0, -.1F, 0), Quat::sIdentity(), EMotionType::Static, 0), EActivation::DontActivate);
        hands = std::make_unique<PhysicalHandClimb>(world, player);
        line = std::make_unique<PortableTraversalLine>(world, player, 1, *hands);
        line->equip(true); aim();
    }
    ~Fixture() { line.reset(); hands.reset(); }
    void aim() {
        line->aim(world.GetBodyInterface().GetPosition(player) + Vec3(0, .6F, 0), Vec3::sAxisY(), true);
    }
    void install() {
        aim(); const auto before = line->readback();
        require(before.target_valid, "ordinary beam surface is eligible without authored eye");
        require(line->install(before.target), "actual shown installation succeeds");
    }
    void step(unsigned count) {
        for (unsigned i = 0; i < count; ++i) {
            line->collision_step(dt); hands->pre_step(dt);
            require(world.Update(dt, 1, &allocator, &jobs) == EPhysicsUpdateError::None, "actual native solve");
            hands->post_step(dt); line->post_step();
        }
    }
};
void lifecycle() {
    Fixture f;
    const auto body_count = f.world.GetNumBodies();
    const auto target = f.line->readback().target;
    require(f.world.GetConstraints().empty() && f.line->readback().available_spools == 1,
        "preview has neither support nor consumed equipment");
    require(!f.line->install(target + 1), "stale displayed target rejects");
    f.install();
    require(f.world.GetConstraints().size() == 1 && f.line->readback().available_spools == 0,
        "one allocated spool creates one real line");
    require(!f.line->install(target), "installed stock cannot duplicate");
    require(f.line->grab(), "existing physical hands grip real fitting");
    require(!f.line->recover() && f.line->readback().failure == PortableTraversalLine::Failure::Loaded,
        "held equipment cannot be silently recovered");
    f.step(180);
    require(f.line->readback().gripping && f.line->readback().tension_n > 400,
        "actual player load reaches installed anchor");
    auto &bodies = f.world.GetBodyInterface();
    const Vec3 departure = bodies.GetLinearVelocity(f.player);
    const auto checkpoint = f.line->capture();
    const auto fitting = f.line->readback().fitting;
    f.line->equip(false);
    require(!f.hands->active() && f.line->readback().installed && f.world.GetConstraints().size() == 1,
        "tool change releases hands while preserving world installation");
    require((bodies.GetLinearVelocity(f.player) - departure).LengthSq() == 0,
        "release keeps actual departure momentum");
    require(f.line->restore(checkpoint), "installed equipment checkpoint rebuilds cold");
    require(f.line->readback().fitting == fitting && f.world.GetNumBodies() == body_count,
        "restore reuses bounded hardware identity and does not append Kit bodies");
    require(!f.hands->active() && f.line->readback().available_spools == 0 && f.world.GetConstraints().size() == 1,
        "restore does not duplicate stock, old rows or player grip");
    for (unsigned i = 0; i < 4; ++i) require(f.line->restore(checkpoint), "repeat checkpoint restore");
    require(f.world.GetConstraints().size() == 1 && f.world.GetNumBodies() == body_count,
        "repeated restore has one installation and fixed body count");
    f.step(1080);
    f.line->equip(true); f.aim();
    require(f.line->grab(), "real hands catch reachable unloaded hardware before packing");
    unsigned settled_ticks = 0;
    while (!f.line->readback().can_recover && settled_ticks < 720) {
        f.step(1); ++settled_ticks;
    }
    if (!f.line->recover()) {
        const auto state = f.line->readback();
        const auto p = bodies.GetPosition(f.player);
        const auto at = bodies.GetPosition(fitting);
        const auto velocity = bodies.GetLinearVelocity(fitting);
        std::cerr << "RECOVERY failure=" << unsigned(state.failure)
            << " tension_n=" << state.tension_n
            << " player=" << p.GetX() << ',' << p.GetY() << ',' << p.GetZ()
            << " fitting=" << at.GetX() << ',' << at.GetY() << ',' << at.GetZ()
            << " fitting_velocity=" << velocity.GetX() << ',' << velocity.GetY() << ',' << velocity.GetZ() << '\n';
        require(false, "reachable unloaded equipment is reusable");
    }
    require(f.world.GetConstraints().empty() && f.line->readback().available_spools == 1,
        "recovery returns one spool and removes line and settled hand rows");
    require(f.line->restore(PortableTraversalLine::Checkpoint{}), "empty checkpoint restores carried spool");
    // A second fixture-stage location exercises broad surface reuse; the
    // shipping public route must separately prove walking between structures.
    bodies.SetPositionAndRotation(f.player, RVec3(0, 1.5F, 0), Quat::sIdentity(), EActivation::Activate);
    bodies.SetLinearVelocity(f.player, Vec3::sZero());
    bodies.SetPositionAndRotation(f.beam, RVec3(0, 3.7F, 0), Quat::sRotation(Vec3::sAxisY(), .7F), EActivation::DontActivate);
    f.line->equip(true); f.install();
    require(f.line->readback().available_spools == 0 && f.world.GetNumBodies() == body_count,
        "reused spool installs on rotated ordinary beam");
    const auto broken_reference = f.line->capture();
    bodies.RemoveBody(f.beam);
    f.line->post_step();
    require(f.line->readback().broken && !f.hands->active() && f.world.GetConstraints().empty(),
        "missing support drops load without stale constraint");
    const auto failure_event = f.line->readback().event_serial;
    f.line->post_step();
    require(f.line->readback().event_serial == failure_event, "one support failure emits one event");
    require(!f.line->restore(broken_reference), "stale supporting body cannot recreate an anchor");
    require(f.world.GetNumBodies() == body_count, "failure still has bounded hardware count");
    std::cout << "PASS PORTABLE_LINE_LIFECYCLE preview=1 real_hands=1 loaded_recovery_rejected=1 release_velocity_preserved=1"
        " installed_tool_change=1 checkpoint_stock=1 pooled_body_identity=1 reuse_rotated_beam=1 stale_support_rejected=1\n";
}
void existing_eye_mount() {
    Fixture f;
    f.line.reset();
    kit::Kit kit(f.world, 0, 1);
    const auto block = kit.add_body(999, {{Vec3(.3F,.3F,.3F), Vec3::sZero(),
        Quat::sIdentity(), kit::Material::Steel}}, RVec3(1.5F,2,0), Quat::sIdentity(), 0, .7F);
    RiggingSystem registry(f.world, kit);
    const Vec3 eye(-.35F,.15F,0);
    registry.add_eye(block, eye, 1000);
    f.line = std::make_unique<PortableTraversalLine>(f.world, f.player, 1, *f.hands, &registry);
    f.line->equip(true);
    const auto hand = f.world.GetBodyInterface().GetPosition(f.player) + Vec3(0,.6F,0);
    const auto target = kit.body_position(block) + eye;
    f.line->aim(hand, Vec3(target-hand), true);
    require(!f.line->readback().target_valid, "unrated block eye cannot impersonate broad beam clamp");
    registry.add_eye(block, eye, 8000);
    f.line->aim(hand, Vec3(target-hand), true);
    const auto shown = f.line->readback();
    require(shown.target_valid && f.line->install(shown.target), "rated existing eye mounts without a duplicate eye registry");
    require(f.line->readback().support == kit.body_id(block) && f.line->readback().path_valid,
        "existing eye loads its actual owner with clear cable geometry");
    require(f.line->recover(), "unloaded existing-eye installation returns one carried spool");
    f.line.reset();
    std::cout << "PASS PORTABLE_LINE_MOUNTS existing_rated_eye=1 weak_eye_rejected=1 actual_support=1 local_recovery=1\n";
}
}
int main() {
    RegisterDefaultAllocator(); Factory::sInstance = new Factory; RegisterTypes();
    int status = 0;
    try { lifecycle(); existing_eye_mount(); }
    catch (const std::exception &error) { std::cerr << "FAIL PORTABLE_LINE_LIFECYCLE " << error.what() << '\n'; status = 1; }
    UnregisterTypes(); delete Factory::sInstance; Factory::sInstance = nullptr;
    return status;
}
