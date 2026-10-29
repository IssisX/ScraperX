// Her perception is honest (matron_perception.hpp; 00_START_HERE.md rule 29:
// perception is evidence with an age). Her decisions read what she saw, heard
// and felt, never where he truly is. The proof: the same world, the same start
// and the same evidence (a lead she heard, then nothing of him at all: he is
// silent and inside solid steel), with only his true position different from
// run to run, standing or moving. Her every tick must match the first run's,
// bit for bit. Real Jolt bodies; the player is a point she is handed, as in
// the sight and hearing tests.
#include "sim_tests.hpp"
#include "sim/bay.hpp"
#include "sim/physics_world_support.hpp"

#include <Jolt/Physics/Constraints/FixedConstraint.h>

#include <cmath>
#include <iomanip>
#include <vector>

namespace {
using namespace JPH;
using namespace scraperx::sim;

struct HonestWorld {
    detail::JoltRuntimeLease runtime;
    detail::BroadPhaseLayerInterface layers;
    detail::ObjectVsBroadPhaseFilter broad;
    detail::ObjectLayerPairFilter pairs;
    TempAllocatorImpl temp{8 * 1024 * 1024};
    JobSystemThreadPool jobs{cMaxPhysicsJobs, cMaxPhysicsBarriers, 1};
    PhysicsSystem physics;
    std::vector<BodyID> bodies;
    static constexpr float dt = 1.0F / 90.0F;
    static constexpr float kFloor = Matron::kDeckTop;
    HonestWorld() {
        physics.Init(4096, 0, 2048, 1024, layers, broad, pairs);
        // Her deck: the tower's footprint, where she can perceive him at all.
        box(Vec3(40, 0.2F, 40), RVec3(0, kFloor - 0.2F, -150));
        // Solid steel he hides inside in some runs: two blocks and a long wall,
        // thick enough that no eye sees through them and no breath carries out.
        box(Vec3(2.5F, 1.5F, 2.5F), RVec3(0, kFloor + 1.5F, -134));
        box(Vec3(2.5F, 1.5F, 2.5F), RVec3(14, kFloor + 1.5F, -150));
        box(Vec3(2.5F, 1.5F, 16.0F), RVec3(-14, kFloor + 1.5F, -145));
    }
    ~HonestWorld() {
        for (const BodyID id : bodies) {
            physics.GetBodyInterface().RemoveBody(id);
            physics.GetBodyInterface().DestroyBody(id);
        }
    }
    void box(const Vec3 half, const RVec3 at) {
        BodyCreationSettings settings(new BoxShape(half), at, Quat::sIdentity(), EMotionType::Static,
                                      detail::object_layers::kStatic);
        bodies.push_back(physics.GetBodyInterface().CreateAndAddBody(settings, EActivation::DontActivate));
    }
    void tick(Matron &matron, const RVec3 player) {
        matron.pre_step(dt, Stimulus(player, 0.0F));
        physics.Update(dt, 1, &temp, &jobs);
        matron.post_step(dt);
    }
};

constexpr int kSettle = 45;
constexpr int kTicks = 90 * 40;
constexpr const char *kPlacements[] = {"far off her deck", "in the block by her lead", "in the block to her side",
                                       "walking the length of the wall"};

// Where he truly is: silent always, and never where her eyes or ears reach.
RVec3 truth(const int placement, const int tick) {
    const double chest = HonestWorld::kFloor + 0.9;
    switch (placement) {
    case 0: return RVec3(80, 144, -150);
    case 1: return RVec3(0, chest, -134);
    case 2: return RVec3(14, chest, -150);
    default: {
        // Down the inside of the long wall and back, a walking pace.
        const double t = std::fmod(double(tick) * HonestWorld::dt * 1.4, 56.0);
        return RVec3(-14, chest, -158.0 + (t < 28.0 ? t : 56.0 - t));
    }
    }
}

struct Frame {
    Matron::View view;
};

std::vector<Frame> run(const int placement) {
    HonestWorld world;
    Matron matron(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                  detail::object_layers::kOnSteel, BodyID());
    matron.place_for_test(RVec3(0, HonestWorld::kFloor, -150));
    for (int i = 0; i < kSettle; ++i) {
        matron.face_for_test(0.0F);
        world.tick(matron, truth(placement, i));
    }
    // The one piece of evidence she gets: a sound ahead of her.
    matron.hear_for_test(RVec3(0, HonestWorld::kFloor, -140));
    std::vector<Frame> frames;
    frames.reserve(kTicks);
    for (int i = 0; i < kTicks; ++i) {
        world.tick(matron, truth(placement, kSettle + i));
        const Matron::View v = matron.view();
        if (v.sees_player || v.glimpsed || v.lead_sense == Matron::Sense::Sight ||
            v.lead_sense == Matron::Sense::Peripheral || v.lead_sense == Matron::Sense::Touch) {
            std::cerr << "FAIL matron honest: placement '" << kPlacements[placement] << "' tick " << i
                      << ": she perceived him (the scene is not one of frozen evidence)\n";
            std::exit(EXIT_FAILURE);
        }
        frames.push_back({v});
    }
    return frames;
}

bool same(const Matron::View &a, const Matron::View &b) {
    return a.state == b.state && a.x == b.x && a.y == b.y && a.z == b.z && a.yaw == b.yaw &&
           a.lead_valid == b.lead_valid && a.lead_x == b.lead_x && a.lead_z == b.lead_z &&
           a.lead_age_s == b.lead_age_s && a.wary == b.wary;
}
}  // namespace

void test_matron_honest() {
    const std::vector<Frame> base = run(0);
    // The scene has to put her decisions to work: she goes after the sound,
    // loses it, and searches.
    float travelled = 0.0F;
    bool searched = false;
    for (std::size_t i = 1; i < base.size(); ++i) {
        travelled += std::hypot(base[i].view.x - base[i - 1].view.x, base[i].view.z - base[i - 1].view.z);
        searched = searched || base[i].view.state == Matron::State::Searching;
    }
    std::cout << "  matron honest: on the lead alone she walked " << travelled << " m, searched " << searched
              << ", ended in state " << int(base.back().view.state) << "\n";
    require(travelled > 5.0F && searched, "matron honest: on a heard lead she hunts, loses it and searches");
    for (int placement = 1; placement < 4; ++placement) {
        const std::vector<Frame> other = run(placement);
        for (std::size_t i = 0; i < base.size(); ++i) {
            if (!same(base[i].view, other[i].view)) {
                const Matron::View &a = base[i].view;
                const Matron::View &b = other[i].view;
                std::cerr << std::setprecision(9) << "  matron honest: placement '" << kPlacements[placement]
                          << "' parted at tick " << i << ": state " << int(a.state) << " vs " << int(b.state)
                          << ", at (" << a.x << ", " << a.z << ") vs (" << b.x << ", " << b.z << "), lead ("
                          << a.lead_x << ", " << a.lead_z << ") vs (" << b.lead_x << ", " << b.lead_z << ")\n";
                require(false, "matron honest: with the same evidence, where he truly is changes nothing she does");
            }
        }
        std::cout << "  matron honest: he " << kPlacements[placement] << ": her " << base.size()
                  << " ticks match bit for bit\n";
    }

    // The chain on the bay's leaf is hers only by looking at it: seen hooked,
    // she sets no trap; the chain taken off, seen gone, the trap comes back.
    {
        HonestWorld world;
        const float leaf_x = 0.5F * (Bay::kLeafWestX + Bay::kLeafEastX);
        BodyCreationSettings leaf_settings(
            new BoxShape(Vec3(0.5F * (Bay::kLeafEastX - Bay::kLeafWestX), Bay::kLeafThick * 0.5F, Bay::kLeafHalfWidth)),
            RVec3(leaf_x, HonestWorld::kFloor + 0.02F, Bay::kLeafZ), Quat::sIdentity(), EMotionType::Static,
            detail::object_layers::kStatic);
        const BodyID leaf = world.physics.GetBodyInterface().CreateAndAddBody(leaf_settings, EActivation::DontActivate);
        world.bodies.push_back(leaf);
        Matron matron(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                      detail::object_layers::kOnSteel, BodyID());
        // West of the bay on its band, facing east over the leaf; he is east
        // of it on her deck, seen and heard.
        matron.place_for_test(RVec3(Bay::kWestX - 2.5F, HonestWorld::kFloor, Bay::kLeafZ));
        const RVec3 him(Bay::kEastX + 3.0F, HonestWorld::kFloor + 0.9F, Bay::kLeafZ);
        const float east = 0.5F * 3.14159265F;
        const auto look = [&](const bool hooked, const int ticks) {
            for (int i = 0; i < ticks; ++i) {
                matron.set_bay(leaf, true, hooked);
                matron.face_for_test(east);
                matron.pre_step(HonestWorld::dt, Stimulus(him, 0.5F));
                world.physics.Update(HonestWorld::dt, 1, &world.temp, &world.jobs);
                matron.post_step(HonestWorld::dt);
            }
        };
        look(true, 45);
        const std::uint32_t hooked_traps = matron.view().traps;
        look(false, 45);
        const std::uint32_t unhooked_traps = matron.view().traps;
        std::cout << "  matron honest: the bay's chain seen hooked, traps " << hooked_traps << "; seen taken off, traps "
                  << unhooked_traps << "\n";
        require(hooked_traps == 0, "matron honest: the chain seen on the leaf, she sets no trap");
        require(unhooked_traps == 1, "matron honest: the chain seen taken off the leaf, her trap comes back");
    }

    // A piece held fast in another's hands (his carry, crouched and still: the
    // load 0.78 m over her floor, at rest) her hands feel held, and do not take.
    {
        HonestWorld world;
        BodyInterface &bodies = world.physics.GetBodyInterface();
        const RVec3 laid(4.0, HonestWorld::kFloor + 0.2, -150.0);
        BodyCreationSettings box_settings(new BoxShape(Vec3(0.25F, 0.15F, 0.15F)), laid, Quat::sIdentity(),
                                          EMotionType::Dynamic, detail::object_layers::kMoving);
        box_settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        box_settings.mMassPropertiesOverride.mMass = 10.0F;
        const BodyID box = bodies.CreateAndAddBody(box_settings, EActivation::Activate);
        world.bodies.push_back(box);
        Matron matron(world.physics, detail::object_layers::kStatic, detail::object_layers::kMoving,
                      detail::object_layers::kOnSteel, BodyID());
        matron.set_throwables({Matron::Throwable{box, 9001, Vec3::sZero(), 0.0F, Vec3::sZero()}});
        // Picked up and held still at a crouch's height 1.6 m ahead of her:
        // his hands, a body of their own, hold it on a fixed joint.
        const RVec3 held(0.0, HonestWorld::kFloor + 0.78, -148.4);
        bodies.SetPositionAndRotation(box, held, Quat::sIdentity(), EActivation::Activate);
        BodyCreationSettings hands_settings(new SphereShape(0.05F), held + RVec3(0.0, 0.0, 0.4), Quat::sIdentity(),
                                            EMotionType::Static, detail::object_layers::kStatic);
        const BodyID hands = bodies.CreateAndAddBody(hands_settings, EActivation::DontActivate);
        world.bodies.push_back(hands);
        FixedConstraintSettings carry;
        carry.mAutoDetectPoint = true;
        Ref<Constraint> carry_joint;
        {
            const BodyID ids[2] = {hands, box};
            BodyLockMultiWrite lock(world.physics.GetBodyLockInterface(), ids, 2);
            carry_joint = carry.Create(*lock.GetBody(0), *lock.GetBody(1));
        }
        world.physics.AddConstraint(carry_joint);
        matron.place_for_test(RVec3(0, HonestWorld::kFloor, -150));
        matron.hear_for_test(RVec3(0, HonestWorld::kFloor, -150));
        std::uint32_t takes = 0;
        bool handled_seen = false;
        for (int i = 0; i < 90 * 4; ++i) {
            matron.face_for_test(0.0F);
            world.tick(matron, truth(0, i));
            takes = std::max(takes, matron.view().takes);
            handled_seen = handled_seen || matron.view().carrying != 0;
        }
        const Vec3 v = bodies.GetLinearVelocity(box);
        std::cout << "  matron honest: a piece held fast at 0.78 m in another's hands: takes " << takes << ", in her hands "
                  << handled_seen << ", its speed " << v.Length() << " m/s\n";
        require(takes == 0 && !handled_seen, "matron honest: a load held fast in his hands is never taken out of them");
        world.physics.RemoveConstraint(carry_joint);
    }
    std::cout << "PASS matron honest: with her evidence frozen, moving his true position changes none of her "
                 "decisions; the bay's chain is hers by looking; a load held in his hands is never taken\n";
}
