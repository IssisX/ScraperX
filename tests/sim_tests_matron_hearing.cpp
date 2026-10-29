// Focused perception contract tests: real Jolt obstruction and body stepping,
// injected acoustic events through Matron::hear. Physical impact production is
// covered separately by the crate-S/crate-N scenario in sim_tests_matron.cpp.
#include "sim_tests.hpp"
#include "sim/physics_world_support.hpp"

namespace {
using namespace JPH;
using namespace scraperx::sim;

struct HearingWorld {
    detail::JoltRuntimeLease runtime;
    detail::BroadPhaseLayerInterface layers;
    detail::ObjectVsBroadPhaseFilter broad;
    detail::ObjectLayerPairFilter pairs;
    TempAllocatorImpl temp{8 * 1024 * 1024};
    JobSystemThreadPool jobs{cMaxPhysicsJobs, cMaxPhysicsBarriers, 1};
    PhysicsSystem physics;
    std::vector<BodyID> bodies;
    static constexpr float dt = 1.0F / 90.0F;
    HearingWorld() { physics.Init(4096, 0, 2048, 1024, layers, broad, pairs); }
    ~HearingWorld() {
        for (const BodyID id : bodies) {
            physics.GetBodyInterface().RemoveBody(id);
            physics.GetBodyInterface().DestroyBody(id);
        }
    }
    BodyID shape(const Shape *shape, const RVec3 at) {
        BodyCreationSettings settings(shape, at, Quat::sIdentity(),
                                      EMotionType::Static, detail::object_layers::kStatic);
        const BodyID id = physics.GetBodyInterface().CreateAndAddBody(settings, EActivation::DontActivate);
        bodies.push_back(id);
        return id;
    }
    BodyID box(const Vec3 half, const RVec3 at) { return shape(new BoxShape(half), at); }
    void tick(Matron &matron, const RVec3 player = RVec3(80, 144, -150), const float loudness = 0.0F) {
        matron.pre_step(dt, player, loudness);
        physics.Update(dt, 1, &temp, &jobs);
        matron.post_step(dt);
        require(matron.view().acoustic_queries <= Matron::kMaxAcousticQueries,
                "matron hearing: fixed ray-query budget");
    }
    void awake(Matron &matron) {
        Matron::Saved saved;
        saved.state = Matron::State::Hunting;
        matron.restore(saved);
        for (int i = 0; i < 180; ++i) tick(matron);
        require(matron.view().released && matron.view().alive, "matron hearing: awake fixture on real floor");
    }
};

// Sound 20 m from the locker: ordinary volume crosses open air, but a wall
// doubles its effective distance beyond the wake radius; silence never wakes.
Matron::View wake_probe(const float loudness, const int walls) {
    HearingWorld world;
    const RVec3 ear(Matron::kLockerX, Matron::kDeckTop + Matron::kLockerHalfY, Matron::kLockerZ);
    for (int i = 0; i < walls; ++i) {
        world.box(Vec3(0.08F, 2, 2), ear + RVec3(1.0 + i, 0, 0));
    }
    Matron matron(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                  detail::object_layers::kOnSteel, BodyID());
    matron.hear(float(ear.GetX()) + 20, float(ear.GetY()), float(ear.GetZ()), loudness);
    world.tick(matron);
    return matron.view();
}

// All leaves intersect the sound ray; a compound envelope covers both
// transverse axes, but its actual sparse rails/posts leave the aperture open.
Matron::View aperture_probe(const int kind) {
    HearingWorld world;
    const RVec3 ear(Matron::kLockerX, Matron::kDeckTop + Matron::kLockerHalfY, Matron::kLockerZ);
    if (kind == 0) world.box(Vec3(0.04F, 0.04F, 17), ear + RVec3(10, 0, 0));
    if (kind == 1) world.box(Vec3(0.04F, 2, 0.04F), ear + RVec3(10, 0, 0));
    if (kind >= 2) {
        StaticCompoundShapeSettings compound;
        compound.AddShape(Vec3(-1, 0, 0), Quat::sIdentity(), new BoxShape(Vec3(0.04F, 0.04F, 3)));
        compound.AddShape(Vec3(1, 0, 0), Quat::sIdentity(), new BoxShape(Vec3(0.04F, 2, 0.04F)));
        if (kind == 3) compound.AddShape(Vec3(3, 0, 0), Quat::sIdentity(), new BoxShape(Vec3(0.08F, 2, 2)));
        const auto made = compound.Create();
        require(made.IsValid(), "matron hearing: compound acoustic fixture created");
        world.shape(made.Get(), ear + RVec3(10, 0, 0));
    }
    Matron matron(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                  detail::object_layers::kOnSteel, BodyID());
    matron.hear(float(ear.GetX()) + 20, float(ear.GetY()), float(ear.GetZ()), 1);
    world.tick(matron);
    return matron.view();
}

Matron::View shaft_probe(const bool blocked) {
    HearingWorld world;
    // North deck ends at z=-133; its support beam projects 25 cm into the
    // shaft. A sound from 143 must bend into air, not ray-cast along steel.
    world.box(Vec3(26, 0.25F, 4.5F), RVec3(0, Matron::kDeckTop - 0.25F, -128.5));
    world.box(Vec3(17, 0.45F, 0.25F), RVec3(0, 153.45, -133));
    if (blocked) world.box(Vec3(26, 0.1F, 2), RVec3(0, 149, -133.5));
    Matron matron(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                  detail::object_layers::kOnSteel, BodyID());
    world.awake(matron);
    const auto at = matron.view();
    matron.hear(at.x, 143.9F, -132.6F, 0.6F);
    world.tick(matron);
    return matron.view();
}

std::array<float, 6> evidence_replay() {
    HearingWorld world;
    world.box(Vec3(40, 0.2F, 40), RVec3(0, Matron::kDeckTop - 0.2F, -150));
    Matron matron(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                  detail::object_layers::kOnSteel, BodyID());
    world.awake(matron);
    const auto start = matron.view();
    const float sx = start.x - 8.0F, sz = start.z;
    matron.hear(sx, start.y, sz, 1.0F);
    world.tick(matron);
    require(matron.view().lead_valid && !matron.view().lead_seen && !matron.view().sees_player,
            "matron hearing: decoy is acoustic evidence, not a sighting");
    require(matron.view().lead_x == sx && matron.view().lead_z == sz,
            "matron hearing: hidden player cannot replace the selected decoy");
    for (int i = 0; i < 90; ++i) world.tick(matron, RVec3(80 + i, 144, -150));
    const auto retained = matron.view();
    require(retained.lead_x == sx && retained.lead_z == sz && retained.lead_age_s > 0.99F,
            "matron hearing: silent unseen motion leaves the remembered point fixed and ages it");
    for (int i = 0; i < 1800; ++i) world.tick(matron);
    require(!matron.view().lead_valid && matron.view().state != Matron::State::Hunting &&
            matron.view().state != Matron::State::Searching,
            "matron hearing: quiet expires pursuit and even an unreachable search");
    const auto at = matron.view();
    // 2 m in front of her eyes (her sight has a field of view: matron.hpp
    // kSightHalfFovRad); it was 2 m east of her, wherever she faced.
    const RVec3 visible(at.x + 2 * std::sin(at.eye_yaw), at.y, at.z + 2 * std::cos(at.eye_yaw));
    world.tick(matron, visible);
    require(matron.view().sees_player && matron.view().lead_seen && matron.view().lead_valid,
            "matron hearing: real visual reacquisition replaces acoustic uncertainty");
    const auto seen = matron.view();
    // The next distant hidden player emits nothing. Fill and overflow the
    // event queue: all admitted candidates are in range and require geometry.
    for (std::size_t i = 0; i < Matron::kMaxSounds + 8; ++i) {
        matron.hear(seen.x - 5.0F, seen.y, seen.z + float(i) * 0.1F, 1.0F);
    }
    world.tick(matron);
    require(matron.view().acoustic_queries > 0 && matron.view().acoustic_queries <= Matron::kMaxAcousticQueries,
            "matron hearing: overflow batch remains within fixed geometry-query budget");
    require(!matron.view().lead_seen, "matron hearing: following sound does not inherit visual provenance");
    return {retained.x, retained.z, retained.lead_age_s, matron.view().x, matron.view().z,
            float(matron.view().acoustic_queries)};
}
} // namespace

void test_matron_hearing() {
    require(wake_probe(0.0F, 0).state == Matron::State::Dormant, "matron hearing: zero event is silent");
    require(wake_probe(0.1F, 0).state == Matron::State::Dormant, "matron hearing: quiet source out of range");
    const auto open = wake_probe(1.0F, 0);
    require(open.state == Matron::State::Straining && open.lead_valid && !open.lead_seen,
            "matron hearing: loud open source wakes with an acoustic lead");
    require(wake_probe(1.0F, 1).state == Matron::State::Dormant,
            "matron hearing: intervening static wall muffles the same source");
    require(aperture_probe(0).state == Matron::State::Straining,
            "matron hearing: 8 cm horizontal rail is not a solid acoustic wall");
    require(aperture_probe(1).state == Matron::State::Straining,
            "matron hearing: narrow upright post leaves the acoustic aperture open");
    require(aperture_probe(2).state == Matron::State::Straining,
            "matron hearing: compound rail/post envelope does not mask open space");
    require(aperture_probe(3).state == Matron::State::Dormant,
            "matron hearing: skipped compound rail cannot suppress its solid wall");
    require(wake_probe(1.0F, 16).state == Matron::State::Dormant,
            "matron hearing: saturated obstruction collector cannot report a clear path");
    {
        HearingWorld world;
        world.box(Vec3(40, 0.2F, 40), RVec3(0, Matron::kDeckTop - 0.2F, -150));
        Matron matron(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                      detail::object_layers::kOnSteel, BodyID());
        const float sx = Matron::kLockerX - 5;
        matron.hear(sx, Matron::kDeckTop + 1, Matron::kLockerZ, 1);
        world.tick(matron);
        for (int i = 0; i < 1450; ++i) world.tick(matron, RVec3(100 + i, 144, -160));
        require(matron.view().state == Matron::State::Emerging && matron.view().lead_x == sx &&
                matron.view().lead_z == Matron::kLockerZ && matron.view().lead_age_s > 16,
                "matron hearing: emergence preserves waking evidence instead of hidden current coordinates");
        for (int i = 0; i < 220; ++i) world.tick(matron);
        require(matron.view().state == Matron::State::Hunting && matron.view().lead_valid &&
                matron.view().lead_x == sx && matron.view().lead_z == Matron::kLockerZ &&
                matron.view().lead_age_s > 18.0F,
                "matron hearing: old waking evidence gets an active investigation without refreshing its age or point");
        for (int i = 0; i < 900; ++i) world.tick(matron);
        require(matron.view().state != Matron::State::Hunting && matron.view().lead_age_s > 27.0F,
                "matron hearing: waking investigation still expires after the bounded active pursuit");
    }
    const auto open_shaft = shaft_probe(false);
    require(open_shaft.noise_heard > 0 && open_shaft.lead_floor_y == 143.0F,
            "matron hearing: open shaft carries around deck and supporting beam");
    require(shaft_probe(true).noise_heard == 0,
            "matron hearing: actual shaft obstruction still muffles the cleared path");
    const auto first = evidence_replay();
    const auto repeat = evidence_replay();
    require(first == repeat, "matron hearing: identical fixed-step inputs replay identically");
    std::cout << "PASS matron hearing: quiet/loud, obstruction/saturation, wake provenance, hidden motion, expiry, reacquisition, budget, replay\n";
}
