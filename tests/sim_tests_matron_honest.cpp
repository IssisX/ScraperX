// Her perception is honest (matron_perception.hpp; 00_START_HERE.md rule 29:
// perception is evidence with an age). Her decisions read what she saw, heard
// and felt, never where he truly is. The proof: the same world, the same start
// and the same evidence (a lead she heard, then nothing of him at all: he is
// silent and inside solid steel), with only his true position different from
// run to run, standing or moving. Her every tick must match the first run's,
// bit for bit. Real Jolt bodies; the player is a point she is handed, as in
// the sight and hearing tests.
#include "sim_tests.hpp"
#include "sim/physics_world_support.hpp"

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
    std::cout << "PASS matron honest: with her evidence frozen, moving his true position changes none of her "
                 "decisions\n";
}
