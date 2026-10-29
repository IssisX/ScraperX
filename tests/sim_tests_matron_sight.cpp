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

// ---- the stalk (matron_stalk.cpp) -------------------------------------------
namespace {
// Her laws, every stalking tick: her feet's force and power within her
// budget, gravity on, no tick moves her more than 0.2 m, and standing on
// the steel under her.
struct LawWatch {
    float peak_force = 0.0F, peak_power = 0.0F, worst_step = 0.0F;
    bool ok = true;
    std::string why;
    RVec3 last = RVec3::sZero();
    bool started = false;
    void check(SightWorld &world, const Matron &matron, const float floor_y, const bool on_floor) {
        BodyInterface &bodies = world.physics.GetBodyInterface();
        const RVec3 at = bodies.GetPosition(matron.body());
        if (started) worst_step = std::max(worst_step, float((at - last).Length()));
        started = true;
        last = at;
        const auto &v = matron.view();
        peak_force = std::max({peak_force, matron.foot_force_n(), v.climb_force_n});
        peak_power = std::max(peak_power, v.foot_power_w);
        const auto fail = [&](const char *what) {
            if (ok) why = what;
            ok = false;
        };
        if (matron.foot_force_n() > law::kForceN + 0.5F || v.climb_force_n > law::kForceN + 0.5F) fail("force over 8,000 N");
        if (v.foot_power_w > kStalkPowerW) fail("power over her budget");
        if (bodies.GetGravityFactor(matron.body()) != 1.0F) fail("gravity off");
        if (worst_step >= 0.2F) fail("a tick moved her 0.2 m or more");
        if (on_floor && std::abs(float(at.GetY()) - (floor_y + Matron::kStandCentre)) > 0.1F) fail("off the steel under her");
        if (!v.alive) fail("dead");
    }
    static constexpr float kStalkPowerW = 22000.0F;
};

float flat_speed(SightWorld &world, const Matron &matron) {
    const Vec3 v = world.physics.GetBodyInterface().GetLinearVelocity(matron.body());
    return std::hypot(v.GetX(), v.GetZ());
}
float flat_gap(const Matron &matron, const RVec3 him) {
    return std::hypot(matron.view().x - float(him.GetX()), matron.view().z - float(him.GetZ()));
}
// His head's facing: to her eyes, or straight away from them.
Vec3 look_to_her(const Matron &matron, const RVec3 him, const bool to_her) {
    const auto &v = matron.view();
    Vec3 d(v.eye_x - float(him.GetX()), 0.0F, v.eye_z - float(him.GetZ()));
    d = d.Normalized();
    return to_her ? d : -d;
}
}  // namespace

void test_matron_stalk() {
    const float g = SightWorld::kFloor;
    // ---- in the open: frozen under his gaze, closing when he looks away,
    //      holding her band, never a grab, until her patience is out ----
    {
        SightWorld world;
        Matron matron(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                      detail::object_layers::kOnSteel, BodyID());
        matron.place_for_test(RVec3(0, g, -150));
        for (int i = 0; i < 45; ++i) {
            matron.face_for_test(0.0F);
            world.tick(matron);
        }
        matron.face_for_test(0.0F);
        RVec3 him(0, g + 0.9, -140);   // 10 m in front of her
        LawWatch law_watch;
        bool grabbed = false;
        const auto step = [&](const bool to_her) {
            matron.set_player_look(look_to_her(matron, him, to_her));
            world.tick(matron, him, 0.0F);
            law_watch.check(world, matron, g, true);
            grabbed = grabbed || matron.caught(him);
        };
        // He looks at her: she has seen his face; she freezes.
        float worst_frozen = 0.0F;
        for (int i = 0; i < 180; ++i) {
            step(true);
            if (i >= 45) worst_frozen = std::max(worst_frozen, flat_speed(world, matron));
        }
        require(matron.view().noticed && matron.view().watched && matron.view().stalking,
                "matron stalk: his face turned to her, seen, begins her stalk");
        require(worst_frozen < 0.2F, "matron stalk: under his gaze she is still");
        const float d0 = flat_gap(matron, him);
        // He looks away 1.5 s: she comes on.
        float fastest = 0.0F;
        for (int i = 0; i < 135; ++i) {
            step(false);
            fastest = std::max(fastest, flat_speed(world, matron));
        }
        const float d1 = flat_gap(matron, him);
        // He looks back: under 0.2 m/s within 0.5 s, and still after.
        float worst_after = 0.0F;
        for (int i = 0; i < 135; ++i) {
            step(true);
            if (i >= 45) worst_after = std::max(worst_after, flat_speed(world, matron));
        }
        const float d2 = flat_gap(matron, him);
        // Away again: she closes again, to her band, and holds there.
        float closest = 1.0e9F;
        for (int i = 0; i < 90 * 6; ++i) {
            step(false);
            closest = std::min(closest, flat_gap(matron, him));
        }
        const float d3 = flat_gap(matron, him);
        std::cout << "  matron stalk: 10 m, gazed at " << d0 << " m; looked away " << d1 << " m (fastest " << fastest
                  << " m/s); gazed again " << d2 << " m (worst " << worst_after << " m/s); away 6 s " << d3
                  << " m (closest " << closest << ")\n";
        require(fastest > 0.5F && d1 < d0 - 1.0F, "matron stalk: when he looks away she comes on");
        require(worst_after < 0.2F && std::abs(d2 - d1) < 0.5F,
                "matron stalk: his face turned to her again, she is under 0.2 m/s within 0.5 s and stays so");
        require(d3 < d2 - 1.0F && d3 <= Matron::kStalkNearM + 0.4F && closest >= Matron::kStalkNearM - 0.3F,
                "matron stalk: looked away from again, she closes to her band's near edge and holds it");
        // He walks off, looking ahead (away from her): she shadows him inside her band.
        float band_lo = 1.0e9F, band_hi = 0.0F;
        for (int i = 0; i < 90 * 5; ++i) {
            him = him + RVec3(0.0, 0.0, 0.6 / 90.0);
            step(false);
            if (i >= 90) {
                band_lo = std::min(band_lo, flat_gap(matron, him));
                band_hi = std::max(band_hi, flat_gap(matron, him));
            }
        }
        std::cout << "  matron stalk: shadowing a walk: band " << band_lo << ".." << band_hi << " m, stalked "
                  << matron.view().stalk_s << " s\n";
        require(matron.view().stalking && band_lo >= Matron::kStalkNearM - 0.3F && band_hi <= Matron::kStalkFarM + 0.5F,
                "matron stalk: he walks off; she shadows him within her band");
        require(!grabbed && law_watch.ok, "matron stalk: no grab in the stalk, and every tick within her laws");
        std::cout << "  matron stalk: laws: peak force " << law_watch.peak_force << " N, peak power "
                  << law_watch.peak_power << " W, worst step " << law_watch.worst_step << " m\n";
        // Her patience: the stalk spent, she attacks (the hunt as it was).
        int t = 0;
        for (; t < 90 * 40 && matron.view().stalk_attacks == 0; ++t) {
            step(t % 400 < 200);
        }
        require(matron.view().stalk_attacks == 1 && !grabbed && matron.view().stalk_s >= Matron::kStalkPatienceS - 0.05F,
                "matron stalk: no grab before her patience is out");
        bool caught = false;
        for (int i = 0; i < 90 * 10 && !caught; ++i) {
            matron.set_player_look(look_to_her(matron, him, false));
            world.tick(matron, him, 0.0F);
            caught = matron.caught(him);
        }
        require(caught && !matron.view().stalking, "matron stalk: her patience out, she attacks and her hands reach him");
    }
    // ---- he steps into her strike: she attacks ----
    {
        SightWorld world;
        Matron matron(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                      detail::object_layers::kOnSteel, BodyID());
        matron.place_for_test(RVec3(0, g, -150));
        for (int i = 0; i < 45; ++i) {
            matron.face_for_test(0.0F);
            world.tick(matron);
        }
        matron.face_for_test(0.0F);
        RVec3 him(0, g + 0.9, -144);
        bool early = false;
        for (int i = 0; i < 90; ++i) {
            matron.set_player_look(look_to_her(matron, him, true));
            world.tick(matron, him, 0.0F);
        }
        require(matron.view().stalking, "matron stalk: 6 m off, gazing at her: a stalk");
        bool caught = false;
        for (int i = 0; i < 90 * 8 && !caught; ++i) {
            if (flat_gap(matron, him) > 1.2F) him = him + RVec3(0.0, 0.0, -1.0 / 90.0);
            matron.set_player_look(look_to_her(matron, him, true));
            world.tick(matron, him, 0.0F);
            caught = matron.caught(him);
            if (caught && matron.view().stalk_attacks == 0) early = true;
        }
        require(caught && !early && matron.view().stalk_attacks == 1,
                "matron stalk: he steps within her strike; she attacks, and only then do her hands reach him");
    }
    // ---- a player who never looks at her is not stalked: taken ----
    {
        SightWorld world;
        Matron matron(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                      detail::object_layers::kOnSteel, BodyID());
        matron.place_for_test(RVec3(0, g, -150));
        for (int i = 0; i < 45; ++i) {
            matron.face_for_test(0.0F);
            world.tick(matron);
        }
        matron.face_for_test(0.0F);
        const RVec3 him(0, g + 0.9, -142);
        bool caught = false, stalked = false;
        for (int i = 0; i < 90 * 15 && !caught; ++i) {
            matron.set_player_look(look_to_her(matron, him, false));
            world.tick(matron, him, 0.0F);
            caught = matron.caught(him);
            stalked = stalked || matron.view().stalking || matron.view().noticed;
        }
        require(caught && !stalked, "matron stalk: unaware of her, he is not stalked; she comes and takes him");
    }
    std::cout << "PASS matron stalk: frozen under his gaze, closing when he looks away, her band, no grab in it, "
                 "the attack rule (patience, strike, unaware)\n";
}

// ---- the ladder he climbs: she goes round and waits at its head ----
void test_matron_ladder_head() {
    const float g = SightWorld::kFloor;
    SightWorld world;
    // A block 4 m up (x 5..11, z -142..-138); his ladder up its south face;
    // another up its north face, the only other way up.
    world.box(Vec3(3.0F, 2.0F, 2.0F), RVec3(8, g + 2.0F, -140));
    for (int k = 1; k <= 13; ++k) {
        world.box(Vec3(0.28F, 0.02F, 0.02F), RVec3(8, g + 0.3F * float(k), -142.35));
        world.box(Vec3(0.28F, 0.02F, 0.02F), RVec3(8, g + 0.3F * float(k), -137.65));
    }
    Matron matron(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                  detail::object_layers::kOnSteel, BodyID());
    matron.place_for_test(RVec3(8, g, -150));
    for (int i = 0; i < 90 && matron.scanned_routes() == nullptr; ++i) {
        matron.face_for_test(0.0F);
        world.tick(matron);
    }
    require(matron.scanned_routes() != nullptr, "matron ladder head: she reads the world");
    // His ladder as her scan has it (the test's answer key, for placing him).
    const auto *map = matron.scanned_routes();
    int ladder = -1;
    for (std::size_t i = 0; i < map->edges().size(); ++i) {
        const auto &e = map->edges()[i];
        if (e.usable && e.kind == traversal::MoveKind::RungUp && std::abs(e.spec.line.GetX() - 8.0F) < 0.5F &&
            std::abs(e.spec.line.GetZ() + 142.35F) < 0.5F) {
            ladder = static_cast<int>(i);
        }
    }
    if (ladder < 0) {
        for (const auto &e : map->edges()) {
            if (e.kind != traversal::MoveKind::Walk)
                std::cout << "    edge " << traversal::move_name(e.kind) << " usable " << e.usable << " line "
                          << e.spec.line.GetX() << "," << e.spec.line.GetZ() << " " << e.reason << "\n";
        }
        std::cout << "    ladders " << map->ladders().size() << "\n";
    }
    require(ladder >= 0, "matron ladder head: her scan reads his ladder");
    const auto spec = map->edges()[std::size_t(ladder)].spec;
    const Vec3 face = -spec.out;
    const RVec3 hang(spec.line.GetX() - face.GetX() * 0.35F, 0.0, spec.line.GetZ() - face.GetZ() * 0.35F);
    const RVec3 head(spec.goal.GetX(), spec.goal.GetY() + 0.9F, spec.goal.GetZ());
    RVec3 him(hang.GetX(), g + 0.9, hang.GetZ() - 0.4);   // at its foot
    matron.face_for_test(0.0F);
    LawWatch law_watch;
    const auto step = [&](const Vec3 look, const bool on_floor) {
        matron.set_player_look(look);
        world.tick(matron, him, 0.0F);
        law_watch.check(world, matron, g, on_floor);
    };
    // At its foot he looks at her: a stalk.
    for (int i = 0; i < 90; ++i) step(look_to_her(matron, him, true), true);
    require(matron.view().stalking, "matron ladder head: he has seen her; she stalks him");
    // He turns to the rungs and climbs 1.5 m, then hangs there, still.
    him = RVec3(hang.GetX(), him.GetY(), hang.GetZ());
    for (int i = 0; i < 135; ++i) {
        him = him + RVec3(0.0, 1.5 / 135.0, 0.0);
        step(Vec3(face.GetX(), 0.0F, face.GetZ()), false);
    }
    bool went = false, at_head = false;
    int t = 0;
    float waited = 0.0F;
    for (; t < 90 * 90 && waited < 5.0F; ++t) {
        step(Vec3(face.GetX(), 0.0F, face.GetZ()), false);
        went = went || matron.view().ambush;
        const auto &v = matron.view();
        const float feet = v.y - Matron::kStandCentre;
        at_head = v.ambush_waiting && std::abs(feet - spec.goal.GetY()) < 0.1F &&
                  std::hypot(v.x - spec.goal.GetX(), v.z - spec.goal.GetZ()) < 0.8F;
        if (at_head) waited += SightWorld::dt;
    }
    const auto waiting = matron.view();
    std::cout << "  matron ladder head: round by her own way " << went << ", at its head after " << float(t) / 90.0F
              << " s at (" << waiting.x << ", " << waiting.y << ", " << waiting.z << "), waited " << waited
              << " s; laws: force " << law_watch.peak_force << " N, power " << law_watch.peak_power << " W, step "
              << law_watch.worst_step << " m" << (law_watch.ok ? "" : " FAILED: ") << law_watch.why << "\n";
    require(went && waiting.ambushes == 1, "matron ladder head: seen up the ladder, she goes round by her own way");
    require(at_head && waited >= 5.0F && flat_speed(world, matron) < 0.2F,
            "matron ladder head: she stands at its head before him, on its floor, and waits there");
    require(law_watch.ok, "matron ladder head: every tick on her way and at the head within her laws");
    // He climbs on and tops out into her hands.
    bool caught = false;
    for (int i = 0; i < 90 * 10 && !caught; ++i) {
        if (him.GetY() < head.GetY() - 0.9) him = him + RVec3(0.0, 0.6 / 90.0, 0.0);
        else him = head;
        matron.set_player_look(Vec3(face.GetX(), 0.0F, face.GetZ()));
        world.tick(matron, him, 0.0F);
        caught = matron.caught(him);
    }
    require(caught, "matron ladder head: he tops out where she waits, and her hands have him");
    std::cout << "PASS matron ladder head: seen up a ladder, round to its head by her own travel, waiting there, "
                 "within her laws\n";
}
