// Private follow-up: identical hard-rope assembly, exact shipping Update(h, 4).
// No rider, aerodynamic force, generic damping, or pose/velocity writes.
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace JPH;
constexpr int COUNT = 60;
constexpr double G = 9.81, PITCH = .4, WIDTH = 1., RUNG_RADIUS = .03;
constexpr double ROPE_DIAMETER = .05, ROPE_DENSITY = 700., WOOD_DENSITY = 650.;
constexpr double WOOD_MASS = WOOD_DENSITY * JPH_PI * RUNG_RADIUS * RUNG_RADIUS * WIDTH;
constexpr double ROPE_LINEAR_MASS = ROPE_DENSITY * JPH_PI * ROPE_DIAMETER * ROPE_DIAMETER / 4.;
constexpr double ROPE_NODE_MASS = 2. * PITCH * ROPE_LINEAR_MASS;
constexpr double MASS = WOOD_MASS + ROPE_NODE_MASS;
constexpr double IX = .5 * WOOD_MASS * RUNG_RADIUS * RUNG_RADIUS;
constexpr double IYZ = WOOD_MASS * (3 * RUNG_RADIUS * RUNG_RADIUS + WIDTH * WIDTH) / 12.
                       + ROPE_NODE_MASS * .44 * .44;

struct Broad final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer x) const override { return BroadPhaseLayer(x); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer x) const override {
        return x.GetValue() ? "MOVING" : "STATIC";
    }
};
struct BPFilter final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer x, BroadPhaseLayer y) const override {
        return x == 1 || y.GetValue() == 1;
    }
};
struct Pair final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer x, ObjectLayer y) const override { return x == 1 || y == 1; }
};
struct Contacts final : ContactListener {
    std::atomic<unsigned> added{0}, persisted{0};
    void OnContactAdded(const Body &, const Body &, const ContactManifold &, ContactSettings &) override {
        ++added;
    }
    void OnContactPersisted(const Body &, const Body &, const ContactManifold &, ContactSettings &) override {
        ++persisted;
    }
};
struct Rope {
    Body *a, *b;
    Vec3 la, lb;
    double length;
    Ref<DistanceConstraint> joint;
};
RVec3 point(const Body *body, Vec3 local) { return body->GetCenterOfMassTransform() * local; }
double kinetic(const Body *body) {
    Vec3 v = body->GetLinearVelocity();
    Vec3 w = body->GetRotation().Conjugated() * body->GetAngularVelocity();
    return .5 * MASS * v.LengthSq()
           + .5 * (IYZ * w.GetX() * w.GetX() + IX * w.GetY() * w.GetY() + IYZ * w.GetZ() * w.GetZ());
}
struct Observer final : PhysicsStepListener {
    const std::vector<Body *> *rungs;
    const std::vector<Rope> *ropes;
    double initial_energy = 0, max_speed = 0, max_stretch = 0, max_energy_delta = 0;
    double min_signed_energy_delta = 0, max_signed_energy_delta = 0, max_ke = 0;
    unsigned callbacks = 0;
    double energy() const {
        double result = 0;
        for (auto *body : *rungs) result += kinetic(body) + MASS * G * body->GetCenterOfMassPosition().GetY();
        return result;
    }
    void observe() {
        double ke = 0;
        for (auto *body : *rungs) {
            max_speed = std::max(max_speed, double(body->GetLinearVelocity().Length()));
            ke += kinetic(body);
        }
        max_ke = std::max(max_ke, ke);
        for (const auto &rope : *ropes) {
            double length = Vec3(point(rope.b, rope.lb) - point(rope.a, rope.la)).Length();
            max_stretch = std::max(max_stretch, length - rope.length);
        }
        double delta = energy() - initial_energy;
        max_energy_delta = std::max(max_energy_delta, std::abs(delta));
        min_signed_energy_delta = std::min(min_signed_energy_delta, delta);
        max_signed_energy_delta = std::max(max_signed_energy_delta, delta);
    }
    void OnStep(const PhysicsStepListenerContext &) override {
        ++callbacks;
        observe(); // Read-only: the documented callback runs before every collision step.
    }
};

int main(int argc, char **argv) {
    const double h = argc > 1 ? std::atof(argv[1]) : 1. / 90.;
    if (!(h > 0 && h <= .02)) return 2;
    RegisterDefaultAllocator();
    Factory::sInstance = new Factory;
    RegisterTypes();
    int result = 0;
    {
        Broad broad;
        BPFilter bp;
        Pair pair;
        TempAllocatorImpl temp(32 * 1024 * 1024);
        JobSystemThreadPool jobs(cMaxPhysicsJobs, cMaxPhysicsBarriers, 1);
        PhysicsSystem sys;
        sys.Init(256, 0, 1024, 2048, broad, bp, pair);
        sys.SetGravity(Vec3(0, -G, 0));
        auto settings = sys.GetPhysicsSettings();
        settings.mNumVelocitySteps = 64;
        settings.mNumPositionSteps = 8;
        settings.mPenetrationSlop = .002;
        sys.SetPhysicsSettings(settings);
        auto &bi = sys.GetBodyInterface();
        Contacts contacts;
        sys.SetContactListener(&contacts);
        std::vector<Body *> bodies, rungs;
        std::vector<Rope> ropes;
        auto add = [&](BodyCreationSettings c) {
            Body *body = bi.CreateBody(c);
            bi.AddBody(body->GetID(), EActivation::Activate);
            bodies.push_back(body);
            return body;
        };
        BodyCreationSettings anchor_config(new BoxShape(Vec3(.7, .15, .3)), RVec3(0, 30.15, 0),
                                            Quat::sIdentity(), EMotionType::Static, 0);
        Body *anchor = add(anchor_config);
        BodyCreationSettings floor_config(new BoxShape(Vec3(8, .3, 8)), RVec3(0, 2.7, 0),
                                           Quat::sIdentity(), EMotionType::Static, 0);
        add(floor_config);
        Quat tilt = Quat::sRotation(Vec3::sAxisX(), 0);
        for (int i = 0; i < COUNT; ++i) {
            RVec3 p = RVec3(0, 30, 0) + RVec3(tilt * Vec3(0, float(-PITCH * (i + 1)), 0));
            BodyCreationSettings c(new CylinderShape(WIDTH / 2., RUNG_RADIUS), p,
                                   tilt * Quat::sRotation(Vec3::sAxisZ(), JPH_PI * .5F),
                                   EMotionType::Dynamic, 1);
            c.mOverrideMassProperties = EOverrideMassProperties::MassAndInertiaProvided;
            c.mMassPropertiesOverride.mMass = MASS;
            c.mMassPropertiesOverride.mInertia = Mat44::sScale(Vec3(IYZ, IX, IYZ));
            c.mFriction = .6;
            c.mRestitution = 0;
            c.mLinearDamping = 0;
            c.mAngularDamping = 0;
            c.mAllowSleeping = false;
            c.mMotionQuality = EMotionQuality::LinearCast;
            rungs.push_back(add(c));
        }
        auto rung_local = [](double x) { return Vec3(0, float(-x), 0); };
        double min_length = 1e9, max_length = 0;
        for (int i = 0; i < COUNT; ++i) for (double x : {-.44, .44}) {
            Body *a = i == 0 ? anchor : rungs[i - 1];
            Body *b = rungs[i];
            Vec3 la = i == 0 ? Vec3(float(x), -.15, 0) : rung_local(x);
            Vec3 lb = rung_local(x);
            DistanceConstraintSettings c;
            c.mSpace = EConstraintSpace::LocalToBodyCOM;
            c.mPoint1 = RVec3(la);
            c.mPoint2 = RVec3(lb);
            c.mMinDistance = 0;
            c.mMaxDistance = Vec3(point(b, lb) - point(a, la)).Length();
            Ref<DistanceConstraint> joint = static_cast<DistanceConstraint *>(c.Create(*a, *b));
            sys.AddConstraint(joint);
            ropes.push_back({a, b, la, lb, c.mMaxDistance, joint});
            min_length = std::min(min_length, double(c.mMaxDistance));
            max_length = std::max(max_length, double(c.mMaxDistance));
        }
        Observer observer;
        observer.rungs = &rungs;
        observer.ropes = &ropes;
        observer.initial_energy = observer.energy();
        observer.observe();
        sys.AddStepListener(&observer);
        std::cout << std::setprecision(12);
        const int ticks = int(std::llround(16. / h));
        unsigned update_errors = 0;
        for (int tick = 0; tick < ticks; ++tick) {
            // Exact shipping call shape. Constraint getters are NOT used as whole-tick impulses.
            update_errors |= unsigned(sys.Update(float(h), 4, &temp, &jobs));
            observer.observe();
            if (tick % std::max(1, int(std::llround(.1 / h))) == 0) {
                std::cout << "SAMPLE " << (tick + 1) * h << ' '
                          << observer.energy() - observer.initial_energy << ' '
                          << rungs.back()->GetPosition().GetY() << '\n';
            }
        }
        sys.RemoveStepListener(&observer);
        const double final_energy = observer.energy();
        std::cout << "RESULT {\"h\":" << h << ",\"call\":\"Update(float(h),4)\",\"ticks\":" << ticks
                  << ",\"substep_callbacks\":" << observer.callbacks << ",\"velocity_iterations\":64,\"position_iterations\":8"
                  << ",\"rung_mass\":" << MASS << ",\"min_initial_span\":" << min_length
                  << ",\"max_initial_span\":" << max_length << ",\"max_speed\":" << observer.max_speed
                  << ",\"max_stretch\":" << observer.max_stretch << ",\"max_kinetic_j\":" << observer.max_ke
                  << ",\"initial_energy_j\":" << observer.initial_energy << ",\"final_energy_j\":" << final_energy
                  << ",\"final_energy_delta_j\":" << final_energy - observer.initial_energy
                  << ",\"max_abs_energy_delta_j\":" << observer.max_energy_delta
                  << ",\"min_signed_energy_delta_j\":" << observer.min_signed_energy_delta
                  << ",\"max_signed_energy_delta_j\":" << observer.max_signed_energy_delta
                  << ",\"contacts_added\":" << contacts.added.load() << ",\"contacts_persisted\":" << contacts.persisted.load()
                  << ",\"update_errors\":" << update_errors << ",\"added_force_calls\":0}\n";
        if (!std::isfinite(final_energy) || update_errors) result = 1;
        for (auto &rope : ropes) sys.RemoveConstraint(rope.joint);
        ropes.clear();
        for (auto *body : bodies) {
            BodyID id = body->GetID();
            bi.RemoveBody(id);
            bi.DestroyBody(id);
        }
    }
    UnregisterTypes();
    delete Factory::sInstance;
    Factory::sInstance = nullptr;
    return result;
}
