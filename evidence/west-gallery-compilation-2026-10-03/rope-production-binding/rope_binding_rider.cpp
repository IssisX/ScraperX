// Private production-settings binding check; frozen elastic assembly, exact Update(h,4).
// One 85 kg central catch/hold/release. Initial insertion velocity only; no wind or extra weight force.
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
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <array>
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
constexpr double HAND_FORCE=1500., AXIS_FORCE=HAND_FORCE/1.7320508075688772;
constexpr double DERIVED_STIFFNESS = 250822.807895133;
constexpr float STIFFNESS = float(DERIVED_STIFFNESS);
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
    std::atomic<unsigned> added{0}, persisted{0}, player_floor{0}, player_other{0};
    BodyID floor_id, player_id;
    void mark(const Body &a,const Body &b) {
        if (a.GetID()==player_id || b.GetID()==player_id) {
            if (a.GetID()==floor_id || b.GetID()==floor_id) ++player_floor; else ++player_other;
        }
    }
    void OnContactAdded(const Body &a, const Body &b, const ContactManifold &, ContactSettings &) override {
        ++added; mark(a,b);
    }
    void OnContactPersisted(const Body &a, const Body &b, const ContactManifold &, ContactSettings &) override {
        ++persisted; mark(a,b);
    }
};
struct Rope {
    Body *a, *b;
    Vec3 la, lb;
    double length, initial_length, expected_tension;
    Ref<DistanceConstraint> joint;
};
struct Hand { int rung=26; Vec3 player_local, rung_local; Ref<SixDOFConstraint> joint; };
RVec3 point(const Body *body, Vec3 local) { return body->GetCenterOfMassTransform() * local; }
double body_mass(const Body *body) {
    return 1. / double(body->GetMotionProperties()->GetInverseMass());
}
double kinetic(const Body *body) {
    const auto *mp = body->GetMotionProperties();
    Vec3 v = body->GetLinearVelocity();
    Vec3 principal_w = mp->GetInertiaRotation().Conjugated()
                       * (body->GetRotation().Conjugated() * body->GetAngularVelocity());
    Vec3 inverse_i = mp->GetInverseInertiaDiagonal();
    double rotation = 0;
    for (int axis=0; axis<3; ++axis) if (inverse_i[axis]>0)
        rotation += .5 * double(principal_w[axis]) * principal_w[axis] / inverse_i[axis];
    return .5 * body_mass(body) * v.LengthSq() + rotation;
}
struct Observer final : PhysicsStepListener {
    const std::vector<Body *> *rungs;
    const std::vector<Rope> *ropes;
    Body *player=nullptr;
    const std::array<Hand,2> *hands=nullptr;
    double inserted_energy=0, max_player_speed=0, max_hand_force=0, max_hand_error=0, catch_peak=0;
    double time=0;
    double initial_energy = 0, max_speed = 0, max_stretch = 0, max_energy_delta = 0;
    double min_signed_energy_delta = 0, max_signed_energy_delta = 0, max_ke = 0;
    unsigned callbacks = 0;
    double gravity = 0, actual_substep = 0, max_pose_change = 0, max_span_change = 0;
    double max_lambda = 0, max_tension = 0, max_static_force_error = 0;
    std::vector<RVec3> initial_positions;
    double spring_energy() const {
        double result = 0;
        for (const auto &rope : *ropes) {
            double l = Vec3(point(rope.b,rope.lb)-point(rope.a,rope.la)).Length();
            double extension = std::max(0.,l-rope.length);
            result += .5 * double(STIFFNESS) * extension * extension;
        }
        return result;
    }
    double energy() const {
        double result = 0;
        for (auto *body : *rungs) result += kinetic(body) + body_mass(body) * gravity * body->GetCenterOfMassPosition().GetY();
        if(player) result += kinetic(player)+body_mass(player)*gravity*player->GetCenterOfMassPosition().GetY();
        return result + spring_energy();
    }
    void observe() {
        double ke = 0;
        int i=0;
        for (auto *body : *rungs) {
            max_pose_change=std::max(max_pose_change,double(Vec3(body->GetCenterOfMassPosition()-initial_positions[i++]).Length()));
            max_speed = std::max(max_speed, double(body->GetLinearVelocity().Length()));
            ke += kinetic(body);
        }
        max_ke = std::max(max_ke, ke);
        for (const auto &rope : *ropes) {
            double length = Vec3(point(rope.b, rope.lb) - point(rope.a, rope.la)).Length();
            max_stretch = std::max(max_stretch, length - rope.length);
            max_span_change = std::max(max_span_change,std::abs(length-rope.initial_length));
            double lambda=rope.joint->GetTotalLambdaPosition();
            max_lambda=std::max(max_lambda,lambda);
            if (actual_substep>0 && callbacks>1) {
                double tension=-lambda/actual_substep;
                max_tension=std::max(max_tension,tension);
                if(!player) max_static_force_error=std::max(max_static_force_error,std::abs(tension-rope.expected_tension));
            }
        }
        if(player) {
            max_player_speed=std::max(max_player_speed,double(player->GetLinearVelocity().Length()));
            for(const auto &hand:*hands) if(hand.joint) {
                if(actual_substep>0) {
                    double force=hand.joint->GetTotalLambdaMotorTranslation().Length()/actual_substep;
                    max_hand_force=std::max(max_hand_force,force);
                    if(time<4.) catch_peak=std::max(catch_peak,force);
                }
                max_hand_error=std::max(max_hand_error,double(Vec3(point(player,hand.player_local)-point((*rungs)[hand.rung],hand.rung_local)).Length()));
            }
        }
        double delta = energy() - initial_energy - inserted_energy;
        max_energy_delta = std::max(max_energy_delta, std::abs(delta));
        min_signed_energy_delta = std::min(min_signed_energy_delta, delta);
        max_signed_energy_delta = std::max(max_signed_energy_delta, delta);
    }
    void OnStep(const PhysicsStepListenerContext &context) override {
        actual_substep=context.mDeltaTime;
        ++callbacks;
        time += actual_substep;
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
        settings.mNumVelocitySteps = 10;
        settings.mNumPositionSteps = 2;
        settings.mPenetrationSlop = .02;
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
        Body *floor=add(floor_config);
        contacts.floor_id=floor->GetID();
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
        double min_length = 1e9, max_length = 0, max_preload_error=0, restsum_left=0;
        for (int i = 0; i < COUNT; ++i) for (double x : {-.44, .44}) {
            Body *a = i == 0 ? anchor : rungs[i - 1];
            Body *b = rungs[i];
            Vec3 la = i == 0 ? Vec3(float(x), -.15, 0) : rung_local(x);
            Vec3 lb = rung_local(x);
            DistanceConstraintSettings c;
            c.mNumVelocityStepsOverride=40;
            c.mNumPositionStepsOverride=8;
            c.mSpace = EConstraintSpace::LocalToBodyCOM;
            c.mPoint1 = RVec3(la);
            c.mPoint2 = RVec3(lb);
            c.mMinDistance = 0;
            const double initial_length=Vec3(point(b, lb) - point(a, la)).Length();
            const double tension=(COUNT-i)*body_mass(b)*(-double(sys.GetGravity().GetY()))*.5;
            c.mMaxDistance=float(initial_length-tension/double(STIFFNESS));
            c.mLimitsSpringSettings=SpringSettings(ESpringMode::StiffnessAndDamping,STIFFNESS,0);
            max_preload_error=std::max(max_preload_error,std::abs(double(STIFFNESS)*(initial_length-c.mMaxDistance)-tension));
            if(x<0) restsum_left+=c.mMaxDistance;
            Ref<DistanceConstraint> joint = static_cast<DistanceConstraint *>(c.Create(*a, *b));
            sys.AddConstraint(joint);
            ropes.push_back({a, b, la, lb, c.mMaxDistance, initial_length, tension, joint});
            min_length = std::min(min_length, double(c.mMaxDistance));
            max_length = std::max(max_length, double(c.mMaxDistance));
        }
        std::array<Hand,2> hands;
        Body *player=nullptr;
        auto release=[&](){ for(auto &hand:hands) if(hand.joint) {sys.RemoveConstraint(hand.joint); hand.joint=nullptr;} };
        auto attach=[&](int which) {
            auto &hand=hands[which];
            hand.rung_local=rung_local(which==0?-.15:.15);
            hand.player_local=Vec3(which==0?-.15F:.15F,.65F,.55F);
            SixDOFConstraintSettings c;
            c.mNumVelocityStepsOverride=40;
            c.mNumPositionStepsOverride=8;
            c.mPosition1=point(rungs[26],hand.rung_local);
            c.mPosition2=point(player,hand.player_local);
            for(int axis=0;axis<6;++axis) c.MakeFreeAxis(static_cast<SixDOFConstraintSettings::EAxis>(axis));
            for(int axis=0;axis<3;++axis) {
                c.mMotorSettings[axis].mSpringSettings=SpringSettings(ESpringMode::StiffnessAndDamping,5000,180);
                c.mMotorSettings[axis].SetForceLimit(AXIS_FORCE);
            }
            hand.joint=static_cast<SixDOFConstraint*>(c.Create(*rungs[26],*player));
            for(int axis=0;axis<3;++axis) hand.joint->SetMotorState(static_cast<SixDOFConstraint::EAxis>(axis),EMotorState::PositionAndVelocity);
            sys.AddConstraint(hand.joint);
        };
        Observer observer;
        observer.hands=&hands;
        observer.rungs = &rungs;
        observer.ropes = &ropes;
        observer.gravity=-double(sys.GetGravity().GetY());
        for(auto *body:rungs) observer.initial_positions.push_back(body->GetCenterOfMassPosition());
        observer.initial_energy = observer.energy();
        double initial_spring=observer.spring_energy();
        observer.observe();
        sys.AddStepListener(&observer);
        std::cout << std::setprecision(12);
        const int ticks = int(std::llround(16. / h));
        unsigned update_errors = 0;
        bool attached=false,released=false;
        double release_jump=0,departure_speed=0,departure_rung_speed=0;
        double loaded_root=0, unloaded_root=0, loaded_extension=0, max_extension=0, loaded_hand_error=0;
        unsigned loaded_samples=0, unloaded_samples=0;
        for (int tick = 0; tick < ticks; ++tick) {
            double t=tick*h;
            if(!attached && t>=3.-h*.1) {
                auto p=point(rungs[26],rung_local(0));
                BodyCreationSettings c(new CapsuleShape(.55,.35),p-RVec3(0,.65,.55),Quat::sIdentity(),EMotionType::Dynamic,1);
                c.mAllowedDOFs=EAllowedDOFs::TranslationX|EAllowedDOFs::TranslationY|EAllowedDOFs::TranslationZ;
                c.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;
                c.mMassPropertiesOverride.mMass=85;
                c.mLinearDamping=0; c.mAngularDamping=0; c.mFriction=0; c.mRestitution=0;
                c.mAllowSleeping=false; c.mMotionQuality=EMotionQuality::LinearCast;
                c.mLinearVelocity=rungs[26]->GetPointVelocity(p)+Vec3(0,-2.5F,0);
                player=add(c); contacts.player_id=player->GetID(); observer.player=player;
                observer.inserted_energy=kinetic(player)+body_mass(player)*observer.gravity*player->GetCenterOfMassPosition().GetY();
                attach(0); attach(1); attached=true;
            }
            if(attached && !released && t>=9.-h*.1) {
                Vec3 before=player->GetLinearVelocity(); release(); Vec3 after=player->GetLinearVelocity();
                release_jump=(after-before).Length(); departure_speed=after.Length();
                departure_rung_speed=rungs[26]->GetPointVelocity(point(player,hands[0].player_local)).Length();
                released=true;
            }
            // Exact shipping call shape. Constraint getters are NOT used as whole-tick impulses.
            update_errors |= unsigned(sys.Update(float(h), 4, &temp, &jobs));
            observer.observe();
            double root=0,extension=0;
            for(int side=0;side<2;++side) {
                const auto &rope=ropes[side];
                Vec3 n=Vec3(point(rope.b,rope.lb)-point(rope.a,rope.la)).NormalizedOr(Vec3(0,-1,0));
                root += rope.joint->GetTotalLambdaPosition()/observer.actual_substep*n.GetY();
            }
            for(int i=0;i<COUNT;++i) { const auto &rope=ropes[2*i]; extension+=std::max(0.,double(Vec3(point(rope.b,rope.lb)-point(rope.a,rope.la)).Length())-rope.length); }
            max_extension=std::max(max_extension,extension);
            if(t>=7 && t<8.5) { loaded_root+=root; loaded_extension+=extension;
                for(const auto &hand:hands) loaded_hand_error+=double(Vec3(point(player,hand.player_local)-point(rungs[26],hand.rung_local)).Length())*.5;
                ++loaded_samples; }
            if(t>=12 && t<15) { unloaded_root+=root; ++unloaded_samples; }
            if (tick % std::max(1, int(std::llround(.1 / h))) == 0) {
                std::cout << "SAMPLE " << (tick + 1) * h << ' '
                          << observer.energy() - observer.initial_energy - observer.inserted_energy << ' '
                          << rungs.back()->GetPosition().GetY() << '\n';
            }
        }
        sys.RemoveStepListener(&observer);
        const double final_energy = observer.energy();
        std::cout << "RESULT {\"h\":" << h << ",\"call\":\"Update(float(h),4)\",\"ticks\":" << ticks
                  << ",\"substep_callbacks\":" << observer.callbacks << ",\"velocity_iterations\":10,\"position_iterations\":2"
                  << ",\"penetration_slop_m\":" << sys.GetPhysicsSettings().mPenetrationSlop
                  << ",\"local_velocity_override\":" << ropes[0].joint->GetNumVelocityStepsOverride()
                  << ",\"local_position_override\":" << ropes[0].joint->GetNumPositionStepsOverride()
                  << ",\"stiffness_n_per_m\":" << STIFFNESS << ",\"material_damping_n_s_per_m\":0"
                  << ",\"actual_gravity\":" << observer.gravity << ",\"actual_rung_mass\":" << body_mass(rungs[0])
                  << ",\"actual_substep_s\":" << observer.actual_substep << ",\"rest_length_sum_left_m\":" << restsum_left
                  << ",\"max_initial_preload_error_n\":" << max_preload_error << ",\"initial_spring_energy_j\":" << initial_spring
                  << ",\"final_spring_energy_j\":" << observer.spring_energy() << ",\"max_pose_change_m\":" << observer.max_pose_change
                  << ",\"max_span_change_m\":" << observer.max_span_change << ",\"max_lambda\":" << observer.max_lambda
                  << ",\"max_tension_last_substep_n\":" << observer.max_tension
                  << ",\"max_static_last_substep_force_error_n\":" << observer.max_static_force_error
                  << ",\"rung_mass\":" << MASS << ",\"min_initial_span\":" << min_length
                  << ",\"max_initial_span\":" << max_length << ",\"max_speed\":" << observer.max_speed
                  << ",\"max_stretch\":" << observer.max_stretch << ",\"max_kinetic_j\":" << observer.max_ke
                  << ",\"initial_energy_j\":" << observer.initial_energy << ",\"final_energy_j\":" << final_energy
                  << ",\"final_energy_delta_j\":" << final_energy - observer.initial_energy
                  << ",\"max_abs_energy_delta_j\":" << observer.max_energy_delta
                  << ",\"min_signed_energy_delta_j\":" << observer.min_signed_energy_delta
                  << ",\"max_signed_energy_delta_j\":" << observer.max_signed_energy_delta
                  << ",\"case\":\"central_catch_hold_release\",\"player_mass\":" << body_mass(player)
                  << ",\"player_gravity_factor\":" << player->GetMotionProperties()->GetGravityFactor()
                  << ",\"player_rotation_locked\":true,\"inserted_energy_j\":" << observer.inserted_energy
                  << ",\"final_unclosed_mechanical_delta_j\":" << final_energy-observer.initial_energy-observer.inserted_energy
                  << ",\"max_player_speed_m_s\":" << observer.max_player_speed
                  << ",\"max_hand_force_last_substep_n\":" << observer.max_hand_force
                  << ",\"catch_peak_hand_force_n\":" << observer.catch_peak
                  << ",\"max_hand_error_m\":" << observer.max_hand_error
                  << ",\"loaded_hand_error_m\":" << loaded_hand_error/loaded_samples
                  << ",\"release_velocity_jump_m_s\":" << release_jump
                  << ",\"departure_capsule_speed_m_s\":" << departure_speed
                  << ",\"departure_rung_point_speed_m_s\":" << departure_rung_speed
                  << ",\"loaded_root_vertical_n\":" << loaded_root/loaded_samples
                  << ",\"unloaded_root_vertical_n\":" << unloaded_root/unloaded_samples
                  << ",\"loaded_left_strand_extension_sum_m\":" << loaded_extension/loaded_samples
                  << ",\"max_left_strand_extension_sum_m\":" << max_extension
                  << ",\"player_floor_contact_events\":" << contacts.player_floor.load()
                  << ",\"player_other_contact_events\":" << contacts.player_other.load()
                  << ",\"contacts_added\":" << contacts.added.load() << ",\"contacts_persisted\":" << contacts.persisted.load()
                  << ",\"update_errors\":" << update_errors << ",\"added_force_calls\":0}\n";
        if (!std::isfinite(final_energy) || update_errors || release_jump!=0 || observer.max_lambda>1e-5 || observer.max_hand_force>HAND_FORCE+1) result = 1;
        release();
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
