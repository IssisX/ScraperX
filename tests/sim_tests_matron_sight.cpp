// Her sight is her eyes (Matron::eyes): cast from her head's height and
// facing, within her field of view, through the world's real solids. What is
// behind her she can only hear. Real Jolt bodies; the player is a point she
// is handed, as in the hearing tests.
#include "sim_tests.hpp"
#include "sim/physics_world_support.hpp"

#include <cmath>

namespace {
using namespace JPH;
using namespace scraperx::sim;

struct SightWorld {
    detail::JoltRuntimeLease runtime;
    detail::BroadPhaseLayerInterface layers;
    detail::ObjectVsBroadPhaseFilter broad;
    detail::ObjectLayerPairFilter pairs;
    TempAllocatorImpl temp{8 * 1024 * 1024};
    JobSystemThreadPool jobs{cMaxPhysicsJobs, cMaxPhysicsBarriers, 1};
    PhysicsSystem physics;
    std::vector<BodyID> bodies;
    static constexpr float dt = 1.0F / 90.0F;
    // Her floor: the deck's top, the tower's footprint (where she perceives).
    static constexpr float kFloor = Matron::kDeckTop;
    SightWorld() {
        physics.Init(4096, 0, 2048, 1024, layers, broad, pairs);
        box(Vec3(40, 0.2F, 40), RVec3(0, kFloor - 0.2F, -150));
    }
    ~SightWorld() {
        for (const BodyID id : bodies) {
            physics.GetBodyInterface().RemoveBody(id);
            physics.GetBodyInterface().DestroyBody(id);
        }
    }
    BodyID box(const Vec3 half, const RVec3 at) {
        BodyCreationSettings settings(new BoxShape(half), at, Quat::sIdentity(), EMotionType::Static,
                                      detail::object_layers::kStatic);
        const BodyID id = physics.GetBodyInterface().CreateAndAddBody(settings, EActivation::DontActivate);
        bodies.push_back(id);
        return id;
    }
    // Far off the tower's footprint and silent: nothing of him reaches her.
    static RVec3 away() { return RVec3(80, 144, -150); }
    void tick(Matron &matron, const RVec3 player = away(), const float loudness = 0.0F) {
        matron.pre_step(dt, player, loudness);
        physics.Update(dt, 1, &temp, &jobs);
        matron.post_step(dt);
    }
};

// The player's centre (his capsule's middle, 0.9 m over his soles) at a
// bearing off `facing` (where her head was turned) and a level distance from
// her eyes.
RVec3 player_at(const Matron::View &v, const float facing, const float bearing, const float distance) {
    const float yaw = facing + bearing;
    return RVec3(v.eye_x + std::sin(yaw) * distance, SightWorld::kFloor + 0.9F, v.eye_z + std::cos(yaw) * distance);
}

struct Probe {
    Matron::View before;
    Matron::View after;
};

// She stands at the footprint's middle facing `yaw`; one tick with the
// player at (bearing, distance) from her eyes, at `loudness`; a wall
// (3 m tall, 4 m wide) across the line halfway when asked.
Probe probe(const float yaw, const float bearing, const float distance, const float loudness, const bool wall) {
    SightWorld world;
    Matron matron(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                  detail::object_layers::kOnSteel, BodyID());
    matron.place_for_test(RVec3(0, SightWorld::kFloor, -150));
    for (int i = 0; i < 45; ++i) {
        matron.face_for_test(yaw);
        world.tick(matron);
    }
    matron.face_for_test(yaw);
    Probe p;
    p.before = matron.view();
    const RVec3 him = player_at(p.before, yaw, bearing, distance);
    if (wall) {
        const float a = yaw + bearing;
        const RVec3 mid(0.5 * (p.before.eye_x + him.GetX()), SightWorld::kFloor + 1.5, 0.5 * (p.before.eye_z + him.GetZ()));
        BodyCreationSettings settings(new BoxShape(Vec3(2.0F, 1.5F, 0.1F)), mid, Quat::sRotation(Vec3::sAxisY(), a),
                                      EMotionType::Static, detail::object_layers::kStatic);
        world.bodies.push_back(world.physics.GetBodyInterface().CreateAndAddBody(settings, EActivation::DontActivate));
    }
    world.tick(matron, him, loudness);
    p.after = matron.view();
    return p;
}

constexpr float kPi = 3.14159265F;
}  // namespace

void test_matron_sight() {
    // Her eyes are her head's, not her knees': 1.69 m over her soles.
    {
        const Probe p = probe(0.0F, 0.0F, 15.0F, 0.0F, false);
        const float eye_over_soles = p.before.eye_y - SightWorld::kFloor;
        std::cout << "  matron sight: eyes " << eye_over_soles << " m over her soles, head yaw " << p.before.eye_yaw
                  << "\n";
        require(std::abs(eye_over_soles - 1.69F) < 0.08F, "matron sight: her eyes are at her head's height");
        require(p.after.sees_player && p.after.lead_seen && p.after.lead_sense == Matron::Sense::Sight,
                "matron sight: in front at 15 m in the open she sees him, a lead labeled sight");
    }
    for (const float yaw : {0.0F, 1.9F, -2.6F}) {
        const Probe behind = probe(yaw, kPi, 3.0F, 0.0F, false);
        require(!behind.after.sees_player && !behind.after.glimpsed && behind.after.lead_sense != Matron::Sense::Sight &&
                    behind.after.lead_sense != Matron::Sense::Peripheral,
                "matron sight: behind her at 3 m in the open (outside her field of view) he is not seen");
        const Probe front = probe(yaw, 0.0F, 15.0F, 0.0F, false);
        require(front.after.sees_player, "matron sight: whichever way she faces, in front at 15 m she sees him");
        const Probe walled = probe(yaw, 0.0F, 6.0F, 0.0F, true);
        require(!walled.after.sees_player && !walled.after.glimpsed && walled.after.lead_sense != Matron::Sense::Sight,
                "matron sight: behind a solid wall in front of her he is not seen");
    }
    // Her field of view is a cone, not the world: 50 degrees off her face is
    // sight; 80 degrees close is only the edge of her eye; 80 degrees far,
    // and anything past her eye's range, nothing.
    {
        require(probe(0.4F, 0.87F, 8.0F, 0.0F, false).after.sees_player,
                "matron sight: 50 degrees off where her head faces is inside her field of view");
        const Probe edge = probe(0.4F, 1.4F, 3.0F, 0.0F, false);
        require(!edge.after.sees_player && edge.after.glimpsed && !edge.after.lead_seen &&
                    edge.after.lead_sense == Matron::Sense::Peripheral && edge.after.lead_valid,
                "matron sight: 80 degrees off at 3 m is peripheral: a lead labeled so, not a sighting");
        const Probe far_edge = probe(0.4F, 1.4F, 8.0F, 0.0F, false);
        require(!far_edge.after.sees_player && !far_edge.after.glimpsed,
                "matron sight: 80 degrees off at 8 m is past her peripheral sense");
        require(!probe(0.4F, 0.0F, 18.0F, 0.0F, false).after.sees_player,
                "matron sight: past her eyes' range in front she does not see him");
    }
    // Footsteps behind her: heard, labeled hearing, with an age that grows
    // while nothing new comes; she turns to it and, if he is still there,
    // sees him. A lead nothing renews goes stale.
    {
        SightWorld world;
        Matron matron(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                      detail::object_layers::kOnSteel, BodyID());
        matron.place_for_test(RVec3(0, SightWorld::kFloor, -150));
        for (int i = 0; i < 45; ++i) {
            matron.face_for_test(0.0F);
            world.tick(matron);
        }
        matron.face_for_test(0.0F);
        const auto start = matron.view();
        const RVec3 steps = player_at(start, 0.0F, kPi, 5.0F);
        world.tick(matron, steps, 0.5F);
        const auto heard = matron.view();
        std::cout << "  matron sight: footsteps behind her: sees " << heard.sees_player << " sense "
                  << int(heard.lead_sense) << " lead (" << heard.lead_x << ", " << heard.lead_z << ") age "
                  << heard.lead_age_s << "\n";
        require(!heard.sees_player && heard.lead_valid && !heard.lead_seen &&
                    heard.lead_sense == Matron::Sense::Hearing &&
                    std::hypot(heard.lead_x - float(steps.GetX()), heard.lead_z - float(steps.GetZ())) < 0.01F,
                "matron sight: heard footsteps behind her give a lead labeled hearing, at the sound");
        for (int i = 0; i < 90; ++i) world.tick(matron);
        const auto aged = matron.view();
        require(aged.lead_sense == Matron::Sense::Hearing && aged.lead_age_s > 0.99F &&
                    aged.lead_x == heard.lead_x && aged.lead_z == heard.lead_z,
                "matron sight: the heard lead keeps its place and ages while nothing renews it");
        int ticks = 0;
        for (; ticks < 90 * 120 && matron.view().lead_valid; ++ticks) world.tick(matron);
        const auto stale = matron.view();
        std::cout << "  matron sight: the heard lead went stale at age " << stale.lead_age_s << " s, state "
                  << int(stale.state) << "\n";
        require(!stale.lead_valid && stale.state != Matron::State::Hunting,
                "matron sight: a lead goes stale as it ages and she stops hunting on it");
    }
    // A glance at the edge of her eye turns her head and then her: he is then seen.
    {
        SightWorld world;
        Matron matron(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                      detail::object_layers::kOnSteel, BodyID());
        matron.place_for_test(RVec3(0, SightWorld::kFloor, -150));
        for (int i = 0; i < 45; ++i) {
            matron.face_for_test(0.0F);
            world.tick(matron);
        }
        matron.face_for_test(0.0F);
        const RVec3 him = player_at(matron.view(), 0.0F, 1.4F, 3.2F);
        bool glimpsed = false, seen = false;
        for (int i = 0; i < 90 && !seen; ++i) {
            world.tick(matron, him, 0.0F);
            glimpsed = glimpsed || matron.view().glimpsed;
            seen = matron.view().sees_player;
        }
        require(glimpsed && seen && matron.view().lead_sense == Matron::Sense::Sight,
                "matron sight: a glimpse at the edge of her eye turns her to him, and then she sees him");
    }
    std::cout << "PASS matron sight: eyes at her head, field of view, walls, hearing labeled with an age, stale leads, "
                 "peripheral glimpse\n";
}
