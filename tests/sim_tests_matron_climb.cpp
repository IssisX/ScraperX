// Minimal real-body falsifiers: an authored route is insufficient without
// steel, sensors are not steel, and removing real held steel releases load.
#include "sim_tests.hpp"
#include "sim/physics_world_support.hpp"

namespace {
using namespace JPH;
using namespace scraperx::sim;
struct ClimbWorld {
    detail::JoltRuntimeLease runtime;
    detail::BroadPhaseLayerInterface layers;
    detail::ObjectVsBroadPhaseFilter broad;
    detail::ObjectLayerPairFilter pairs;
    TempAllocatorImpl temp{8 * 1024 * 1024};
    JobSystemThreadPool jobs{cMaxPhysicsJobs, cMaxPhysicsBarriers, 1};
    PhysicsSystem physics;
    std::vector<BodyID> bodies, rungs;
    static constexpr float dt = 1.0F / 90.0F;
    ClimbWorld() {
        physics.Init(4096, 0, 2048, 1024, layers, broad, pairs);
        box(Vec3(40, 0.2F, 40), RVec3(0, Matron::kDeckTop - 0.2F, -150));
    }
    ~ClimbWorld() {
        for (BodyID id : bodies) {
            if (physics.GetBodyInterface().IsAdded(id)) physics.GetBodyInterface().RemoveBody(id);
            physics.GetBodyInterface().DestroyBody(id);
        }
    }
    BodyID box(Vec3 half, RVec3 at, bool sensor = false) {
        BodyCreationSettings settings(new BoxShape(half), at, Quat::sIdentity(),
                                      EMotionType::Static, detail::object_layers::kStatic);
        settings.mIsSensor = sensor;
        const BodyID id = physics.GetBodyInterface().CreateAndAddBody(settings, EActivation::DontActivate);
        bodies.push_back(id);
        return id;
    }
    void steel(bool sensor, float height = 5.0F) {
        for (float y = Matron::kDeckTop + 0.3F; y < Matron::kDeckTop + height; y += 0.3F) {
            rungs.push_back(box(Vec3(0.28F, 0.02F, 0.02F), RVec3(5, y, -129.05), sensor));
        }
    }
    void tick(Matron &matron) {
        // A visible noisy climber on the known ladder yields a head-of-ladder
        // lead through the normal perception and Turn initiation paths.
        matron.pre_step(dt, RVec3(5, 160, -129.55), 1.0F);
        physics.Update(dt, 1, &temp, &jobs);
        matron.post_step(dt);
    }
    void awake(Matron &matron) {
        Matron::Saved saved;
        saved.state = Matron::State::Hunting;
        matron.restore(saved);
        // Fixture placement only; production climbing never sets a pose.
        physics.GetBodyInterface().SetPosition(matron.body(),
            RVec3(5, Matron::kDeckTop + Matron::kStandCentre, -129.55), EActivation::Activate);
    }
};
}

void test_matron_climb_support() {
    std::array<std::array<float, 6>, 6> recorded{};
    for (int replay = 0; replay < 2; ++replay) {
    for (int scenario = 0; scenario < 6; ++scenario) {
        ClimbWorld world;
        if (scenario > 0) world.steel(scenario == 1, scenario == 5 ? 2.8F : 5.0F);
        if (scenario == 4) world.box(Vec3(0.3F, 0.5F, 0.02F), RVec3(5, 156.2, -129.2));
        if (scenario == 3) world.box(Vec3(1, 0.1F, 1), RVec3(5, 157, -129.55));
        Matron matron(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                      detail::object_layers::kOnSteel, BodyID());
        world.awake(matron);
        bool attached = false, removed = false, released = false, falling = false, landed = false;
        float peak_force = 0.0F;
        RVec3 previous = world.physics.GetBodyInterface().GetPosition(matron.body());
        for (int i = 0; i < 40 * 90; ++i) {
            world.tick(matron);
            const auto &view = matron.view();
            const RVec3 position = world.physics.GetBodyInterface().GetPosition(matron.body());
            require(Vec3(position - previous).Length() < 0.15F,
                    "matron support: attachment and release preserve continuous physical motion");
            previous = position;
            peak_force = std::max(peak_force, view.climb_force_n);
            require(view.climb_force_n <= Matron::kClimbGripMaxN + 0.1F,
                    "matron support: applied grip force is bounded");
            if (view.climbing) attached = true;
            if (scenario == 2 && !removed && view.climbing && view.y > 155.6F) {
                for (const BodyID rung : world.rungs) world.physics.GetBodyInterface().RemoveBody(rung);
                removed = true;
            } else if (removed || ((scenario == 3 || scenario == 5) && attached)) {
                if (!view.climbing && view.climb_force_n == 0.0F && view.climb_releases > 0) released = true;
                if (released && world.physics.GetBodyInterface().GetLinearVelocity(matron.body()).GetY() < -0.3F) falling = true;
                if (falling && std::abs(view.y - (Matron::kDeckTop + Matron::kStandCentre)) < 0.04F) {
                    landed = true;
                    break;
                }
            }
        }
        if (scenario < 2 || scenario == 4) {
            require(!attached && matron.view().climb_releases > 0 && peak_force == 0,
                    "matron support: missing/sensor/obscured rungs refuse real attempted attachment");
            require(std::abs(matron.view().y - (Matron::kDeckTop + Matron::kStandCentre)) < 0.05F,
                    "matron support: authored route does not lift her through empty space");
        } else if (scenario == 2) {
            require(attached && removed && released && falling && landed && peak_force > 2000,
                    "matron support: removed held steel releases force, falls under gravity, recovers floor");
        } else {
            require(attached && released && falling && landed,
                    "matron support: blocked or incomplete ascent stalls then releases to recoverable floor");
        }
        const auto &end = matron.view();
        const std::array<float, 6> result{end.x, end.y, end.z, peak_force,
            float(end.climb_grips), float(end.climb_releases)};
        if (replay == 0) recorded[scenario] = result;
        else require(result == recorded[scenario], "matron support: identical physics inputs replay identically");
    }
    }
    std::cout << "PASS matron climb support: missing steel, sensor exclusion, actual support removal, blocked ascent and floor recovery\n";
}
