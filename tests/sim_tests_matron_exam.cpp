// The climbing lab's exam (tests/matron_exam.hpp) under test itself, with no
// executor: every seed builds the same world twice and keeps its bounds; the
// holds it builds read as holds to the repo's own predicate; and the judge
// passes a body set down on the goal and fails a snap, a held grip, a force
// over the cap, gravity taken off and a lethal fall.

#include "sim_tests.hpp"

#include "matron_exam.hpp"
#include "sim/matron.hpp"
#include "sim/physics_world_support.hpp"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CompoundShape.h>
#include <Jolt/RegisterTypes.h>

#include <cmath>
#include <iostream>
#include <set>

namespace {
namespace exam = scraperx::sim::exam;
using scraperx::sim::Matron;

constexpr JPH::ObjectLayer kLayer = 0;
constexpr float kDt = 1.0F / 90.0F;

class Layers final : public JPH::BroadPhaseLayerInterface {
public:
    [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override { return 1; }
    [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer) const override {
        return JPH::BroadPhaseLayer(0);
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    [[nodiscard]] const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer) const override { return "ALL"; }
#endif
};

class AllVsBroadPhase final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer, JPH::BroadPhaseLayer) const override { return true; }
};

class AllPairs final : public JPH::ObjectLayerPairFilter {
public:
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer, JPH::ObjectLayer) const override { return true; }
};

struct World {
    Layers layers;
    AllVsBroadPhase broad_phase_filter;
    AllPairs pair_filter;
    JPH::PhysicsSystem physics;
    JPH::TempAllocatorImpl temp{8 << 20};
    JPH::JobSystemSingleThreaded jobs{JPH::cMaxPhysicsJobs};
    World() { physics.Init(256, 0, 1024, 1024, layers, broad_phase_filter, pair_filter); }
    void step() { (void)physics.Update(kDt, 1, &temp, &jobs); }
};

// Her body as the lab's executors get it: 0.30 m by 2.1 m, 220 kg, upright.
JPH::BodyID her_at(World &world, const JPH::RVec3 feet) {
    JPH::BodyCreationSettings settings(new JPH::CapsuleShape(Matron::kHalfHeight, Matron::kRadius),
                                       feet + JPH::Vec3(0.0F, Matron::kStandCentre, 0.0F), JPH::Quat::sIdentity(),
                                       JPH::EMotionType::Dynamic, kLayer);
    settings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY |
                            JPH::EAllowedDOFs::TranslationZ;
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = Matron::kMassKg;
    settings.mFriction = 0.8F;
    return world.physics.GetBodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
}

JPH::RVec3 middle_of(const JPH::AABox &box) { return JPH::RVec3(box.GetCenter().GetX(), box.mMin.GetY() + 0.06F, box.GetCenter().GetZ()); }

// `ticks` ticks of the judge, each after a step; `each` may act on the world first.
template <typename Each>
const exam::Verdict &judge(World &world, exam::Judge &judge, const JPH::BodyID body, const int ticks,
                           const float commanded_n, const bool gripping, Each each) {
    for (int i = 0; i < ticks && !judge.done(); ++i) {
        each(i);
        world.step();
        judge.tick(world.physics, body, commanded_n, gripping, kDt);
    }
    return judge.verdict();
}

// The horizontal holds of a structure, as the repo's own predicate reads them.
int horizontal_holds(World &world, const JPH::BodyID holds) {
    const JPH::BodyLockRead lock(world.physics.GetBodyLockInterfaceNoLock(), holds);
    require(lock.Succeeded(), "matron exam: the structure is in the world");
    const JPH::Body &body = lock.GetBody();
    require(body.GetShape()->GetType() == JPH::EShapeType::Compound, "matron exam: a ladder or panel is a compound");
    const auto *compound = static_cast<const JPH::CompoundShape *>(body.GetShape());
    int count = 0;
    for (JPH::uint i = 0; i < compound->GetNumSubShapes(); ++i) {
        const JPH::SubShapeID id = compound->GetSubShapeIDFromIndex(int(i), JPH::SubShapeIDCreator()).GetID();
        scraperx::sim::detail::LeafBox box;
        std::uint32_t axis = 0;
        if (scraperx::sim::detail::leaf_box(body, id, box) && scraperx::sim::detail::box_is_hold(box, axis) &&
            std::abs(box.axes[axis].GetY()) < 0.05F) {
            ++count;
        }
    }
    return count;
}

} // namespace

void run_matron_exam() {
    // No Simulation is alive here: hold the runtime the way its lease does.
    require(JPH::Factory::sInstance == nullptr, "matron exam: the test must own the Jolt runtime");
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    // Every seed: the same world twice, its bounds kept, its holds holds.
    constexpr std::uint64_t kSeeds = 200;
    int kinds[4] = {0, 0, 0, 0};
    std::set<std::uint64_t> fingerprints;
    for (std::uint64_t seed = 1; seed <= kSeeds; ++seed) {
        World a;
        World b;
        const exam::Structure s = exam::build(a.physics, seed, kLayer);
        require(exam::build(b.physics, seed, kLayer).fingerprint == s.fingerprint,
                "matron exam: a seed builds the same world every time");
        fingerprints.insert(s.fingerprint);
        const exam::Params &p = s.params;
        ++kinds[int(p.kind)];
        require(p.opening_m >= exam::kMinOpeningM, "matron exam: the rail's gap lets her through");
        switch (p.kind) {
        case exam::Kind::Ladder:
        case exam::Kind::Panel: {
            require(p.pitch_m >= exam::kMinPitchM && p.pitch_m <= exam::kMaxPitchM, "matron exam: hold spacing in bounds");
            require(p.first_hold_m >= exam::kMinFirstHoldM && p.first_hold_m <= exam::kMaxFirstHoldM,
                    "matron exam: the first hold in reach");
            require((p.missing >= 0 ? 2.0F : 1.0F) * p.pitch_m <= exam::kMaxHoldGapM,
                    "matron exam: no gap between holds past her reach");
            const float span = p.height_m - p.first_hold_m;
            const int built = p.kind == exam::Kind::Ladder ? 1 + int((p.height_m - 0.10F - p.first_hold_m) / p.pitch_m)
                                                           : 1 + int((span - 0.05F) / p.pitch_m);
            const float top_hold = p.first_hold_m + float(built - 1) * p.pitch_m;
            require(p.height_m - top_hold <= 0.10F + exam::kMaxPitchM, "matron exam: the top hold under the goal's lip");
            require(horizontal_holds(a, s.holds) == built - (p.missing >= 0 ? 1 : 0),
                    "matron exam: every rung or bar reads as a hold to the repo's own predicate");
            break;
        }
        case exam::Kind::Pole:
            require(p.section_m >= 0.06F && p.section_m <= 0.14F && p.standoff_m >= 0.30F && p.standoff_m <= 0.50F,
                    "matron exam: a pole in bounds");
            break;
        case exam::Kind::LedgeStack:
            require(p.pitch_m >= exam::kMinRiseM && p.pitch_m <= exam::kMaxRiseM, "matron exam: a ledge's rise in bounds");
            break;
        }
    }
    require(kinds[0] > 0 && kinds[1] > 0 && kinds[2] > 0 && kinds[3] > 0, "matron exam: every kind of structure comes up");
    require(fingerprints.size() >= kSeeds - 2, "matron exam: different seeds, different worlds");

    // The judge, with a body and no executor.
    const auto none = [](int) {};
    int judged = 0;
    {
        // Set down on the goal: a second standing on it, holding nothing.
        World w;
        const exam::Structure s = exam::build(w.physics, 7, kLayer);
        const JPH::BodyID body = her_at(w, middle_of(s.goal_feet) + JPH::Vec3(0.0F, 0.3F, 0.0F));
        exam::Judge j(s.goal, s.goal_feet, 6000.0F, Matron::kStandCentre);
        const exam::Verdict &v = judge(w, j, body, 270, 2000.0F, false, none);
        require(v.arrived && v.alive && v.worst_step_m < exam::Judge::kMaxStepM, "matron exam: a body set down on the goal arrives");
        ++judged;
    }
    {
        // Snapped there from the start: failed, whatever it does after.
        World w;
        const exam::Structure s = exam::build(w.physics, 7, kLayer);
        const JPH::BodyID body = her_at(w, s.start_at);
        exam::Judge j(s.goal, s.goal_feet, 6000.0F, Matron::kStandCentre);
        const exam::Verdict &v = judge(w, j, body, 270, 0.0F, false, [&](const int i) {
            if (i == 30) {
                w.physics.GetBodyInterface().SetPosition(
                    body, middle_of(s.goal_feet) + JPH::Vec3(0.0F, Matron::kStandCentre, 0.0F), JPH::EActivation::Activate);
            }
        });
        require(!v.arrived && v.reason.find("snapped") == 0, "matron exam: a snap onto the goal fails");
        ++judged;
    }
    {
        // On the goal, but with a grip held: never arrived.
        World w;
        const exam::Structure s = exam::build(w.physics, 7, kLayer);
        const JPH::BodyID body = her_at(w, middle_of(s.goal_feet));
        exam::Judge j(s.goal, s.goal_feet, 6000.0F, Matron::kStandCentre);
        const exam::Verdict &v = judge(w, j, body, 270, 0.0F, true, none);
        require(!v.arrived && v.reason == "not yet", "matron exam: standing there holding on is not arriving");
        ++judged;
    }
    {
        // A commanded force over the cap.
        World w;
        const exam::Structure s = exam::build(w.physics, 7, kLayer);
        const JPH::BodyID body = her_at(w, middle_of(s.goal_feet));
        exam::Judge j(s.goal, s.goal_feet, 6000.0F, Matron::kStandCentre);
        const exam::Verdict &v = judge(w, j, body, 270, 6100.0F, false, none);
        require(!v.arrived && v.reason == "over the force cap", "matron exam: over the cap fails");
        ++judged;
    }
    {
        // Gravity taken off her.
        World w;
        const exam::Structure s = exam::build(w.physics, 7, kLayer);
        const JPH::BodyID body = her_at(w, middle_of(s.goal_feet));
        w.physics.GetBodyInterface().SetGravityFactor(body, 0.0F);
        exam::Judge j(s.goal, s.goal_feet, 6000.0F, Matron::kStandCentre);
        const exam::Verdict &v = judge(w, j, body, 270, 0.0F, false, none);
        require(!v.arrived && v.reason == "gravity changed on her body", "matron exam: gravity off fails");
        ++judged;
    }
    {
        // Dropped 14 m onto the floor: a lethal landing (16.6 m/s).
        World w;
        const exam::Structure s = exam::build(w.physics, 7, kLayer);
        const JPH::BodyID body = her_at(w, s.start_at + JPH::Vec3(0.0F, 14.0F, 0.0F));
        exam::Judge j(s.floor, s.start_feet, 6000.0F, Matron::kStandCentre);
        const exam::Verdict &v = judge(w, j, body, 360, 0.0F, false, none);
        require(!v.alive && v.worst_landing_mps >= exam::Judge::kLethalLandingMps,
                "matron exam: a 14 m fall is a lethal landing");
        ++judged;
    }
    {
        // Standing on the floor is not standing on the goal.
        World w;
        const exam::Structure s = exam::build(w.physics, 7, kLayer);
        const JPH::BodyID body = her_at(w, s.start_at);
        exam::Judge j(s.goal, s.goal_feet, 6000.0F, Matron::kStandCentre);
        const exam::Verdict &v = judge(w, j, body, 270, 0.0F, false, none);
        require(!v.arrived && v.alive, "matron exam: the wrong support is not the goal");
        ++judged;
    }

    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
    std::cout << "PASS scraperx_sim matron exam: " << kSeeds << " seeds (ladder " << kinds[0] << ", pole " << kinds[1]
              << ", panel " << kinds[2] << ", ledge stack " << kinds[3] << "), " << fingerprints.size()
              << " distinct worlds; judge " << judged << " cases\n";
}
