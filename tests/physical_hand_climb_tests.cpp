#include <Jolt/Jolt.h>
#include "sim/physical_hand_climb.hpp"
#include "sim/cargo_net.hpp"
#include "sim/cargo_net_route.hpp"
#include "sim/facade_route.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// Native coupling fixtures, not ordinary-input acquisition, traversal or device proof.
// Removing coupling fails sag/load/recoil; reversing targets fails ascent;
// unbounded motors/commands fail catches/budget; copied release velocity fails momentum.
namespace {
using namespace JPH;
using scraperx::sim::PhysicalHandClimb;
constexpr float h = 1.0F / 90.0F;
constexpr double weight = 85.0 * double(9.81F);
void check(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
template<class V> bool finite(const V &v) {
    return std::isfinite(v.GetX()) && std::isfinite(v.GetY()) && std::isfinite(v.GetZ());
}
struct BP final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l) const override { return BroadPhaseLayer(l); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "hand-test"; }
};
struct BV final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override { return a != 0 || b.GetValue() != 0; }
};
struct LP final : ObjectLayerPairFilter {
    bool contact_free = false;
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override { return !contact_free && (a != 0 || b != 0); }
};
struct Contacts final : ContactListener {
    unsigned count = 0;
    void OnContactAdded(const Body &, const Body &, const ContactManifold &, ContactSettings &) override { ++count; }
    void OnContactPersisted(const Body &, const Body &, const ContactManifold &, ContactSettings &) override { ++count; }
};
struct ForceObserver final : PhysicsStepListener {
    PhysicalHandClimb &hands;
    BodyID player,support;
    bool support_has_world_weld=false,radial_force_valid=true,receipt_valid=true,pending=false;
    float last_dt=0,player_mass=0,support_mass=0;
    Vec3 radii[2],player_velocity,support_velocity,player_gravity,support_gravity;
    bool free_dynamic_support=false;
    double max_player_impulse_error=0,max_support_impulse_error=0;
    ForceObserver(PhysicalHandClimb &c,BodyID p,BodyID s,bool weld):
        hands(c),player(p),support(s),support_has_world_weld(weld) {}
    void finish(PhysicsSystem &system,bool bodies_locked=false) {
        if(!pending)return;
        hands.post_step(last_dt);
        const BodyLockInterface &access=bodies_locked?
            static_cast<const BodyLockInterface &>(system.GetBodyLockInterfaceNoLock()):
            static_cast<const BodyLockInterface &>(system.GetBodyLockInterface());
        BodyLockRead p(access,player),s(access,support);
        if(!p.Succeeded()||!s.Succeeded()){receipt_valid=false;pending=false;return;}
        Vec3 total=Vec3::sZero();
        for(unsigned i=0;i<2;++i) {
            const auto force=hands.hand_force(i);total+=force;
            // Force belongs to the completed solve: compare with its cached
            // start radius, not the body's subsequently integrated pose.
            radial_force_valid&=finite(force)&&force.Length()<=1500.05F&&
                force.Cross(radii[i].Normalized()).Length()<=.0001F*force.Length()+.001F;
        }
        const double player_error=(player_mass*(p.GetBody().GetLinearVelocity()-player_velocity)-
            (total+player_mass*player_gravity)*last_dt).Length();
        max_player_impulse_error=std::max(max_player_impulse_error,player_error);
        receipt_valid&=std::isfinite(player_error)&&player_error<=.01;
        if(free_dynamic_support) {
            const double support_error=(support_mass*(s.GetBody().GetLinearVelocity()-support_velocity)-
                (-total+support_mass*support_gravity)*last_dt).Length();
            max_support_impulse_error=std::max(max_support_impulse_error,support_error);
            receipt_valid&=std::isfinite(support_error)&&support_error<=.01;
        }
        pending=false;
    }
    void OnStep(const PhysicsStepListenerContext &c) override {
        finish(*c.mPhysicsSystem,true);
        hands.pre_step(c.mDeltaTime,true);
        if(!hands.radial_active()){hands.post_step(c.mDeltaTime);return;}
        const auto &access=c.mPhysicsSystem->GetBodyLockInterfaceNoLock();
        const auto *p=access.TryGetBody(player),*s=access.TryGetBody(support);
        if(!p||!s||!p->IsDynamic()){receipt_valid=false;return;}
        player_mass=1/p->GetMotionProperties()->GetInverseMass();
        player_velocity=p->GetLinearVelocity();
        player_gravity=c.mPhysicsSystem->GetGravity()*p->GetMotionProperties()->GetGravityFactor();
        free_dynamic_support=s->IsDynamic()&&!support_has_world_weld;
        if(free_dynamic_support) {
            support_mass=1/s->GetMotionProperties()->GetInverseMass();
            support_velocity=s->GetLinearVelocity();
            support_gravity=c.mPhysicsSystem->GetGravity()*s->GetMotionProperties()->GetGravityFactor();
        }
        for(unsigned i=0;i<2;++i) {
            const auto grip=s->GetWorldTransform()*Vec3(i?.2F:-.2F,-.1F,-.1F);
            radii[i]=Vec3(grip-p->GetCenterOfMassPosition());
        }
        last_dt=c.mDeltaTime;pending=true;
    }
};
struct World {
    BP bp; BV bv; LP lp;
    PhysicsSystem system;
    TempAllocatorImpl temporary { 16 * 1024 * 1024 };
    JobSystemSingleThreaded jobs { 1024 };
    Contacts contacts;
    std::vector<BodyID> ids;
    BodyID player, support;
    bool support_has_world_weld=false;
    double max_player_impulse_error=0,max_support_impulse_error=0;
    explicit World(bool dynamic_support = false, Vec3 velocity = Vec3::sZero(), bool kinematic_support = false, float support_mass = 25.0F, bool gyroscopic_support = false, RVec3 player_at = RVec3(0, 4.3, -0.2), bool contact_free = false) {
        lp.contact_free = contact_free;
        system.Init(64, 0, 128, 256, bp, bv, lp);
        system.SetGravity(Vec3(0, -9.81F, 0));
        system.SetContactListener(&contacts);
        check(system.GetPhysicsSettings().mNumVelocitySteps == 10 &&
              system.GetPhysicsSettings().mNumPositionSteps == 2, "unexpected shipping global solver defaults");
        BodyCreationSettings p(new CapsuleShape(0.55F, 0.35F), player_at,
                               Quat::sIdentity(), EMotionType::Dynamic, 1);
        p.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        p.mMassPropertiesOverride.mMass = 85.0F;
        p.mAllowedDOFs = EAllowedDOFs::TranslationX | EAllowedDOFs::TranslationY | EAllowedDOFs::TranslationZ;
        p.mLinearDamping = p.mAngularDamping = 0;
        p.mAllowSleeping = false; p.mFriction = p.mRestitution = 0;
        p.mMotionQuality = EMotionQuality::LinearCast;
        p.mLinearVelocity = velocity;
        player = add(p);
        BodyCreationSettings s(new BoxShape(Vec3(0.5F, 0.1F, 0.1F)), RVec3(0, 5.1, 0.65),
                               Quat::sIdentity(), kinematic_support ? EMotionType::Kinematic :
                               (dynamic_support ? EMotionType::Dynamic : EMotionType::Static),
                               (dynamic_support || kinematic_support) ? 1 : 0);
        s.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        s.mMassPropertiesOverride.mMass = support_mass;
        s.mApplyGyroscopicForce = gyroscopic_support;
        s.mLinearDamping = s.mAngularDamping = 0; s.mAllowSleeping = false;
        support = add(s);
        // Grip lies on the visible support front/bottom edge, 0.4m clear of
        // the capsule. There is no platform/contact capable of faking a hang.
    }
    ~World() {
        system.SetContactListener(nullptr);
        check(system.GetConstraints().empty(), "constraint leaked past hand-owner cleanup");
        for (auto id : ids) { system.GetBodyInterface().RemoveBody(id); system.GetBodyInterface().DestroyBody(id); }
    }
    BodyID add(const BodyCreationSettings &settings) {
        auto id = system.GetBodyInterface().CreateAndAddBody(settings, EActivation::Activate);
        check(!id.IsInvalid(), "native body allocation failed"); ids.push_back(id); return id;
    }
    Vec3 velocity(BodyID id) { return system.GetBodyInterface().GetLinearVelocity(id); }
    RVec3 position(BodyID id) { return system.GetBodyInterface().GetPosition(id); }
    void tick(PhysicalHandClimb &hands, unsigned frames) {
        ForceObserver observer(hands,player,support,support_has_world_weld);
        system.AddStepListener(&observer);
        for (unsigned i = 0; i < frames; ++i) {
            check(system.Update(h, 4, &temporary, &jobs) == EPhysicsUpdateError::None, "native update error");
            observer.finish(system);
            hands.post_step(h / 4);
            check(finite(position(player)) && finite(velocity(player)) && finite(position(support)) &&
                  finite(velocity(support)), "nonfinite constrained body state");
        }
        observer.finish(system);
        system.RemoveStepListener(&observer);
        check(observer.radial_force_valid,"actual substep radial force has a tangential component or exceeds cap");
        check(observer.receipt_valid,"actual radial force receipt fails substep momentum within0.01N*s");
        max_player_impulse_error=std::max(max_player_impulse_error,observer.max_player_impulse_error);
        max_support_impulse_error=std::max(max_support_impulse_error,observer.max_support_impulse_error);
    }
    void acquire(PhysicalHandClimb &hands) {
        check(hands.attach(0, support, RVec3(-0.2, 5.0, 0.55)), "first physical hand was not acquired");
        check(hands.attach(1, support, RVec3(0.2, 5.0, 0.55)), "second physical hand was not acquired");
    }
};
// Radial owner fixtures use the same per-collision-step callback as production.
// No body state is written after acquisition; neutral commands cannot supply work.
void radial_rows_only(World &w,unsigned fixture_welds=0) {
    unsigned rows=0,welds=0;
    for(const auto &constraint:w.system.GetConstraints()) {
        check(constraint->GetSubType()!=EConstraintSubType::SixDOF,"radial mode retains competing Cartesian hand constraint");
        if(constraint->GetSubType()==EConstraintSubType::User2) {
            check(constraint->GetType()==EConstraintType::TwoBodyConstraint,"radial row lacks actual two-body coupling");
            const auto *pair=static_cast<const TwoBodyConstraint *>(constraint.GetPtr());
            check(pair->GetBody1()->GetID()==w.player&&pair->GetBody2()->GetID()==w.support,
                  "radial row does not join the actual player and support");++rows;
        }else if(constraint->GetSubType()==EConstraintSubType::Fixed)++welds;
        else check(false,"unexpected competing constraint in radial fixture");
    }
    check(rows==2&&welds==fixture_welds,"radial fixture needs exactly two scalar rows and its declared welds");
}
void radial_force_bounds(World &w, PhysicalHandClimb &hands) {
    radial_rows_only(w);
    for (unsigned i=0;i<2;++i) {
        const Vec3 force=hands.hand_force(i);
        check(finite(force)&&force.Length()<=1500.05F,"radial hand force exceeds1500N or is nonfinite");
    }
    check(w.contacts.count==0,"radial fixture has an external contact");
}
void passive_radial_arc() {
    World w(false,Vec3(0,1.096F,-1.023F));PhysicalHandClimb hands(w.system,w.player);
    w.acquire(hands);
    const auto velocity=w.velocity(w.player),grip=hands.grip_midpoint();
    check(hands.set_radial_profile(true)&&hands.radial_active(),"compatible fixed hands did not enter radial mode");
    check((w.velocity(w.player)-velocity).LengthSq()==0&&Vec3(hands.grip_midpoint()-grip).LengthSq()==0,
          "radial handoff rewrote velocity or anchors");
    radial_rows_only(w);
    const float span=hands.radial_commanded_span_m();
    check(std::abs(span-hands.radial_actual_span_m())<.00001F,"radial entry retained old spring preload");
    const double paid=hands.actuator_positive_work_j();
    double low=1e9,high=-1e9;
    for(unsigned i=0;i<360;++i) {
        check(hands.advance_radial_targets(0,h)==0,"neutral command changed radial length");
        w.tick(hands,1);radial_force_bounds(w,hands);
        check(std::abs(hands.radial_commanded_span_m()-span)<.000001F,"neutral rest length drifted");
        check(hands.actuator_positive_work_j()==paid,"neutral arc generated positive target work");
        if(i>=270){low=std::min(low,double(w.position(w.player).GetZ()));high=std::max(high,double(w.position(w.player).GetZ()));}
    }
    std::cout<<"FREE ARC late270_360_z_span_m="<<high-low<<" actual_span_m="<<hands.radial_actual_span_m()
             <<" commanded_span_m="<<span<<" speed="<<w.velocity(w.player).Length()<<" neutral_positive_work=0\n";
    // CHOSEN before baseline:0.05m rejects the measured old pin's0.00125115m.
    check(high-low>.05,"fixed neutral hands suppress sustained late passive arc");
}
Vec3 total_linear_momentum(World &w,float mass) {
    return 85.0F*w.velocity(w.player)+mass*w.velocity(w.support);
}
Vec3 relative_angular_momentum(World &w,float mass) {
    const auto p=w.position(w.player),q=w.position(w.support);
    const RVec3 center=(85.0*p+double(mass)*q)/(85.0+mass);
    const Vec3 mean=total_linear_momentum(w,mass)/(85.0F+mass);
    Vec3 result=Vec3(p-center).Cross(85.0F*(w.velocity(w.player)-mean))+
        Vec3(q-center).Cross(mass*(w.velocity(w.support)-mean));
    BodyLockRead lock(w.system.GetBodyLockInterface(),w.support);
    const auto &body=lock.GetBody();
    // The freely rotating support contributes spin as well as orbital momentum.
    result+=body.GetInverseInertia().Inversed3x3()*body.GetAngularVelocity();
    return result;
}
void radial_dynamic_reciprocity() {
    for(float mass:{25.0F,1.0F}) {
        World w(true,Vec3(1.5F,0,-1),false,mass,true);PhysicalHandClimb hands(w.system,w.player);
        w.acquire(hands);check(hands.set_radial_profile(true),"dynamic hands rejected radial mode");
        const auto momentum=total_linear_momentum(w,mass),angular=relative_angular_momentum(w,mass);
        double max_linear_error=0,max_angular_error=0;
        for(unsigned i=0;i<180;++i) {
            w.tick(hands,1);radial_force_bounds(w,hands);
            // Uniform gravity changes total linearY only. About the system COM
            // its torque cancels; off-centre radial reaction must spin the support.
            const Vec3 expected=momentum+Vec3(0,-(85+mass)*9.81F*h*float(i+1),0);
            max_linear_error=std::max(max_linear_error,double((total_linear_momentum(w,mass)-expected).Length()));
            max_angular_error=std::max(max_angular_error,double((relative_angular_momentum(w,mass)-angular).Length()));
        }
        std::cout<<"RADIAL RECOIL mass_kg="<<mass<<" momentum_error="<<max_linear_error
                 <<" angular_error="<<max_angular_error<<" support_spin="
                 <<w.system.GetBodyInterface().GetAngularVelocity(w.support).Length()<<'\n';
        const auto final_angular=relative_angular_momentum(w,mass);
        const auto spin=w.system.GetBodyInterface().GetAngularVelocity(w.support);
        std::cout<<"RADIAL ANGULAR before="<<angular.GetX()<<','<<angular.GetY()<<','<<angular.GetZ()
                 <<" after="<<final_angular.GetX()<<','<<final_angular.GetY()<<','<<final_angular.GetZ()
                 <<" support_omega="<<spin.GetX()<<','<<spin.GetY()<<','<<spin.GetZ()
                 <<" max_player_impulse_receipt_error="<<w.max_player_impulse_error
                 <<" max_support_impulse_receipt_error="<<w.max_support_impulse_error<<'\n';
        { BodyLockRead lock(w.system.GetBodyLockInterface(),w.support);
          const auto inverse=lock.GetBody().GetInverseInertia();
          std::cout<<"RADIAL INERTIA inverse_world_columns=";
          for(unsigned col=0;col<3;++col){const auto v=inverse.GetColumn3(col);std::cout<<v.GetX()<<','<<v.GetY()<<','<<v.GetZ()<<';';}
          std::cout<<" actual_mass="<<1/lock.GetBody().GetMotionProperties()->GetInverseMass()<<'\n'; }
        check(max_linear_error<.1,"radial forces violate reciprocal linear momentum");
        check(max_angular_error<.2,"off-centre radial pair violates angular momentum");
        check(w.system.GetBodyInterface().GetAngularVelocity(w.support).Length()>.001F,
              "off-centre reciprocal force failed to rotate dynamic support");
    }
}
double radial_potential(double error) {
    const double e=std::max(error,0.0);
    return e<=.3?.5*5000*e*e:1500*e-225;
}
void radial_work_and_handoffs() {
    World w;PhysicalHandClimb hands(w.system,w.player);w.acquire(hands);w.tick(hands,180);
    unsigned completed=0;bool reach_exhausted=false;
    for(unsigned cycle=0;cycle<12;++cycle) {
        const auto before_velocity=w.velocity(w.player),before_grip=hands.grip_midpoint();
        const double ledger=hands.actuator_positive_work_j();
        const auto body=w.position(w.player);
        const auto transform=w.system.GetBodyInterface().GetWorldTransform(w.support);
        const float left_span=Vec3(transform*Vec3(-.2F,-.1F,-.1F)-body).Length();
        const float right_span=Vec3(transform*Vec3(.2F,-.1F,-.1F)-body).Length();
        std::cout<<"RADIAL HANDOFF ENTRY cycle="<<cycle<<" com="<<body.GetX()<<','<<body.GetY()<<','<<body.GetZ()
                 <<" midpoint="<<before_grip.GetX()<<','<<before_grip.GetY()<<','<<before_grip.GetZ()
                 <<" speed="<<before_velocity.Length()<<" hand_attached="<<hands.attached(0)<<','<<hands.attached(1)
                 <<" compatible="<<hands.compatible_hands()<<" actual_spans="<<left_span<<','<<right_span
                 <<" paid_work="<<ledger<<'\n';
        if(left_span>1.75F||right_span>1.75F) {
            const auto support_position=w.position(w.support),support_velocity=w.velocity(w.support);
            check(completed>=3,"fixture loses eligible reach before meaningful repeated handoffs");
            check(!hands.set_radial_profile(true)&&!hands.radial_active(),"out-of-reach radial entry accepted");
            check(Vec3(w.position(w.player)-body).LengthSq()==0&&
                  Vec3(w.position(w.support)-support_position).LengthSq()==0&&
                  (w.velocity(w.player)-before_velocity).LengthSq()==0&&
                  (w.velocity(w.support)-support_velocity).LengthSq()==0&&
                  Vec3(hands.grip_midpoint()-before_grip).LengthSq()==0&&
                  hands.actuator_positive_work_j()==ledger&&w.system.GetConstraints().size()==2,
                  "rejected reach handoff mutated actual bodies/anchors/ledger/Cartesian ownership");
            reach_exhausted=true;break;
        }
        check(hands.set_radial_profile(true),"within-reach repeated radial entry rejected");
        check((w.velocity(w.player)-before_velocity).LengthSq()==0&&Vec3(hands.grip_midpoint()-before_grip).LengthSq()==0,
              "radial entry changed instantaneous velocity or anchors");
        check(std::abs(hands.radial_actual_span_m()-hands.radial_commanded_span_m())<.00001F,
              "radial entry did not discard old preload at zero error");
        check(hands.actuator_positive_work_j()==ledger,"handoff charged or refunded positive work");
        const float entry_span=hands.radial_commanded_span_m();
        double shortened=0;
        for(unsigned i=0;i<45;++i) {
            const float old_span=hands.radial_commanded_span_m(),actual=hands.radial_actual_span_m();
            const double paid=hands.actuator_positive_work_j();
            const float accepted=hands.advance_radial_targets(float(std::max(0.0,.25-shortened)),h);
            const float new_span=hands.radial_commanded_span_m();
            const double supplied=hands.actuator_positive_work_j()-paid;
            check(accepted>=0&&accepted<=.6F*h+.000001F&&std::abs(old_span-new_span-accepted)<.00001F,
                  "radial shortening violates combined stroke or actual return receipt");
            const double expected=2*std::max(0.0,radial_potential(actual-new_span)-radial_potential(actual-old_span));
            check(std::abs(supplied-expected)<.005,"radial work differs from capped-force spring potential");
            check(supplied<=500*h+.00001&&supplied>=0,"radial shortening violates500W or refunds paid work");
            shortened+=accepted;w.tick(hands,1);radial_force_bounds(w,hands);
        }
        check(shortened>.10&&std::abs(entry_span-hands.radial_commanded_span_m()-shortened)<.00001,
              "radial actuator failed meaningful independently measured shortening");
        const float held=hands.radial_commanded_span_m();const double paid=hands.actuator_positive_work_j();
        for(unsigned i=0;i<45;++i){(void)hands.advance_radial_targets(0,h);w.tick(hands,1);radial_force_bounds(w,hands);}
        check(hands.radial_commanded_span_m()==held&&hands.actuator_positive_work_j()==paid,
              "held radial length ratchets or invents paid tracking work");
        for(unsigned i=0;i<90;++i) {
            const float old_span=hands.radial_commanded_span_m(),actual=hands.radial_actual_span_m();
            const double before_paid=hands.actuator_positive_work_j();
            const float accepted=hands.advance_radial_targets(old_span-entry_span,h);
            const float new_span=hands.radial_commanded_span_m();
            const double supplied=hands.actuator_positive_work_j()-before_paid;
            const double expected=2*std::max(0.0,radial_potential(actual-new_span)-radial_potential(actual-old_span));
            check(accepted<=.000001F&&std::abs(accepted)<=.6F*h+.000001F&&std::abs(old_span-new_span-accepted)<.00001F,
                  "radial extension violates stroke or actual return receipt");
            check(supplied>=0&&supplied<=500*h+.00001&&std::abs(supplied-expected)<.005,
                  "radial extension violates potential work or refunds paid work");
            w.tick(hands,1);radial_force_bounds(w,hands);
        }
        const double exit_paid=hands.actuator_positive_work_j();
        const auto exit_velocity=w.velocity(w.player),exit_grip=hands.grip_midpoint();
        check(hands.set_radial_profile(false)&&!hands.radial_active(),"radial exit rejected");
        check((w.velocity(w.player)-exit_velocity).LengthSq()==0&&Vec3(hands.grip_midpoint()-exit_grip).LengthSq()==0,
              "radial exit changed velocity or anchors");
        check(Vec3(hands.commanded_position()-w.position(w.player)).Length()<.00001F,
              "Cartesian handoff retained radial preload");
        check(w.system.GetConstraints().size()==2&&hands.actuator_positive_work_j()==exit_paid,
              "handoff leaked exclusive solver ownership or refunded work");
        w.tick(hands,1);++completed;
    }
    check(completed>=3,"insufficient real repeated handoffs");
    std::cout<<"RADIAL HANDOFF completed_cycles="<<completed<<" max_cycles=12 reach_exhausted="<<reach_exhausted
             <<" positive_work_j="<<hands.actuator_positive_work_j()<<'\n';
}
// A real world weld must carry the same radial catch as a static support.
// This contact-free fixture isolates joint-coupled inertia; it does not add
// ballast or replace the free support's actual0.722kg mass after creation.
struct CatchResult { double max_span, max_radius, support_motion; };
CatchResult radial_falling_support(bool dynamic,bool fixed,const char *label) {
    World w(dynamic,Vec3(0,-3.87F,0),false,.722F,false,RVec3(0,4.55,-.015),true);
    struct WorldWeld {
        PhysicsSystem &system; Ref<Constraint> constraint;
        ~WorldWeld(){if(constraint)system.RemoveConstraint(constraint);}
    } weld{w.system,{}};
    if(fixed) {
        FixedConstraintSettings settings;settings.mAutoDetectPoint=true;
        auto *body=w.system.GetBodyLockInterfaceNoLock().TryGetBody(w.support);
        check(body!=nullptr,"fixed light support missing");
        weld.constraint=settings.Create(Body::sFixedToWorld,*body);
        w.system.AddConstraint(weld.constraint);w.support_has_world_weld=true;
    }
    PhysicalHandClimb hands(w.system,w.player);w.acquire(hands);
    const auto initial_support=w.position(w.support);const auto initial_momentum=total_linear_momentum(w,.722F);
    const Vec3 incoming=w.velocity(w.player);
    check(hands.set_radial_profile(true)&&hands.radial_active(),"falling pair enters radial mode");
    check((w.velocity(w.player)-incoming).LengthSq()==0,"radial entry changes incoming catch velocity");
    const double rest=hands.radial_commanded_span_m(),paid=hands.actuator_positive_work_j();
    check(rest>.74&&rest<.76,"controlled catch starts within unchanged reach");
    double max_span=rest,max_radius=0,max_motion=0,peak_force=0;
    for(unsigned tick=0;tick<270;++tick) {
        check(hands.advance_radial_targets(0,h)==0,"neutral catch changes rest length");w.tick(hands,1);
        max_span=std::max(max_span,double(hands.radial_actual_span_m()));
        max_motion=std::max(max_motion,double(Vec3(w.position(w.support)-initial_support).Length()));
        for(unsigned hand=0;hand<2;++hand) {
            const auto grip=w.system.GetBodyInterface().GetWorldTransform(w.support)*Vec3(hand?.2F:-.2F,-.1F,-.1F);
            max_radius=std::max(max_radius,double(Vec3(grip-w.system.GetBodyInterface().GetCenterOfMassPosition(w.player)).Length()));
            const auto force=hands.hand_force(hand);peak_force=std::max(peak_force,double(force.Length()));
            check(finite(force)&&force.Length()<=1500.05F,"radial catch exceeds actual1500N per-hand force");
        }
        check(w.contacts.count==0,"controlled radial catch develops a contact");
        // The only extra constraint here is the fixture's support/world weld.
        radial_rows_only(w,fixed?1U:0U);
        check(std::abs(hands.radial_commanded_span_m()-rest)<.000001&&hands.actuator_positive_work_j()==paid,
              "neutral catch changes rest length or positive paid ledger");
    }
    std::cout<<"FIXED LIGHT CATCH kind="<<label<<" rest_span="<<rest<<" max_span="<<max_span
      <<" max_hand_radius="<<max_radius<<" max_support_motion="<<max_motion<<" player_vy="<<w.velocity(w.player).GetY()
      <<" support_vy="<<w.velocity(w.support).GetY()<<" peak_hand_force="<<peak_force
      <<" neutral_positive_work="<<hands.actuator_positive_work_j()-paid<<'\n';
    if(dynamic&&!fixed) {
        const Vec3 expected=initial_momentum+Vec3(0,-(85.0F+.722F)*9.81F*h*270,0);
        const double error=(total_linear_momentum(w,.722F)-expected).Length();
        std::cout<<"FIXED LIGHT FREE CONTROL momentum_error="<<error<<'\n';
        check(max_motion>.1&&error<.05,"free light support has actual recoil and reciprocal linear momentum");
    }
    return {max_span,max_radius,max_motion};
}
void radial_fixed_light_catch() {
    const auto a=radial_falling_support(false,false,"static");
    const auto b=radial_falling_support(true,true,"world_fixed_0.722kg");
    (void)radial_falling_support(true,false,"free_0.722kg");
    std::cout<<"FIXED LIGHT EQUIVALENCE absolute_max_span_difference="<<std::abs(a.max_span-b.max_span)<<'\n';
    // Chosen before candidate runtime:2.5cm rejects the measured7.14m
    // divergence while allowing finite substep solver differences.
    check(a.max_span<1.75&&b.max_span<1.75&&a.max_radius<1.75&&b.max_radius<1.75,
          "static and world-fixed light support must arrest the same within-reach catch");
    check(b.support_motion<.01&&std::abs(a.max_span-b.max_span)<.025,
          "actual fixed-joint load path must match static catch within2.5cm");
}
void static_load_and_release() {
    World w;
    PhysicalHandClimb hands(w.system, w.player);
    w.acquire(hands);
    check(hands.active() && hands.attached(0) && hands.attached(1), "hand state disagrees with acquisition");
    const double initial_y = w.position(w.player).GetY();
    w.tick(hands, 360);
    const double sag = initial_y - w.position(w.player).GetY();
    const double reaction = hands.hand_force(0).GetY() + hands.hand_force(1).GetY();
    check(std::abs(sag - weight / 10000.0) < 0.002, "two-hand gravity-on compliance differs from mg/(2k)");
    check(std::abs(reaction - weight) < 2.0, "hand reaction does not support actual player weight");
    check(w.velocity(w.player).Length() < 0.002F, "hanging equilibrium still moving");
    {
        BodyLockRead lock(w.system.GetBodyLockInterface(), w.player);
        check(lock.GetBody().GetMotionProperties()->GetGravityFactor() == 1.0F, "hand acquisition disabled gravity");
        check(std::abs(1.0 / lock.GetBody().GetMotionProperties()->GetInverseMass() - 85) < 0.001, "hand acquisition changed mass");
    }
    const auto before = w.velocity(w.player);
    hands.detach(0);
    check((w.velocity(w.player) - before).LengthSq() == 0, "one-hand detach changed momentum");
    check(!hands.attached(0) && hands.attached(1), "detach removed wrong hand");
    w.tick(hands, 360);
    std::cout << "one-hand at4s sag=" << initial_y - w.position(w.player).GetY() << " reaction=" << hands.hand_force(1).GetY()
              << " vy=" << w.velocity(w.player).GetY() << '\n';
    // The specified per-axis cap is 866N: only 32N above mg. Detachment's
    // downward transient is still force-saturated at4s; wait for equilibrium
    // without changing the chosen actuator or relaxing static balance checks.
    w.tick(hands, 1080);
    const double one_sag = initial_y - w.position(w.player).GetY();
    check(std::abs(one_sag - weight / 5000.0) < 0.002, "remaining hand fails real weight transfer");
    check(std::abs(hands.hand_force(1).GetY() - weight) < 2.0, "one-hand reaction is wrong");
    const auto departure = w.velocity(w.player);
    hands.clear();
    check((w.velocity(w.player) - departure).LengthSq() == 0, "release copied support velocity or added boost");
    check(!hands.active() && w.system.GetConstraints().empty(), "clear left a hand constraint");
    w.tick(hands, 9);
    check(std::abs(w.velocity(w.player).GetY() - departure.GetY() + 9.81F * 9 * h) < 0.002, "released body is not freely falling");
    check(w.contacts.count == 0, "fixture contact could have faked hand support");
    std::cout << "static sag=" << sag << " reaction=" << reaction << " one_sag=" << one_sag << " peak_force=" << hands.peak_hand_force_n() << '\n';
}
void command_sign_and_budget() {
    World w;
    PhysicalHandClimb hands(w.system, w.player);
    w.acquire(hands); w.tick(hands, 360);
    const double y = w.position(w.player).GetY();
    (void)hands.advance_targets(Vec3(0, 0.2F, 0), 0.2F);
    check(hands.last_command_work_bound_j() > 599.9 && hands.last_command_work_bound_j() <= 600.001, "two-hand command debit is not shared Fmax-distance budget");
    w.tick(hands, 270);
    check(w.position(w.player).GetY() - y > 0.195 && w.position(w.player).GetY() - y < 0.205, "positive climb command moved player in wrong direction");
    const double previous = hands.command_work_bound_j();
    (void)hands.advance_targets(Vec3(100, 30, -70), h);
    check(hands.last_command_work_bound_j() > 33.32 && hands.last_command_work_bound_j() <= 3000.0 * h + 0.0001, "command exceeded 3000W aggregate bound");
    check(std::abs(hands.command_work_bound_j() - previous - hands.last_command_work_bound_j()) < 0.0001, "cumulative command debit missing");
    const double ledger = hands.command_work_bound_j();
    (void)hands.advance_targets(Vec3(std::numeric_limits<float>::quiet_NaN(), 0, 0), h);
    check(hands.last_command_work_bound_j() == 0 && hands.command_work_bound_j() == ledger, "invalid command entered ledger");
    hands.detach(0); (void)hands.advance_targets(Vec3(100, 30, -70), h);
    check(hands.last_command_work_bound_j() > 33.32 && hands.last_command_work_bound_j() <= 3000.0 * h + 0.0001, "one-hand budget unexpectedly doubled or halved");
    hands.clear(); (void)hands.advance_targets(Vec3(1, 1, 1), h);
    check(hands.last_command_work_bound_j() == 0, "unattached command consumed work budget");
    std::cout << "command ascent=" << w.position(w.player).GetY() - y << " cumulative_bound=" << hands.command_work_bound_j() << '\n';
}
void measured_swing_actuation() {
    World w;
    PhysicalHandClimb hands(w.system,w.player);
    w.acquire(hands);w.tick(hands,360);hands.set_swing_profile();
    const double sag=4.3-w.position(w.player).GetY();
    const auto target=hands.commanded_position();
    (void)hands.advance_targets(Vec3(0,.15F,0),h);
    const double stroke=hands.commanded_position().GetY()-target.GetY();
    const double expected=5000.0*(2*sag*stroke+stroke*stroke);
    check(stroke>0 && stroke<=.6*h+.000001,"swing lean exceeded finite body stroke");
    check(std::abs(hands.last_actuator_positive_work_j()-expected)<.002,
          "spring actuation receipt differs from actual loaded extension/work");
    check(hands.last_actuator_positive_work_j()<=500*h+.00001,
          "swing actual positive spring work exceeded500W budget");
    const double supplied=hands.actuator_positive_work_j();
    (void)hands.advance_targets(Vec3::sZero(),h);
    check(hands.actuator_positive_work_j()==supplied && hands.last_actuator_positive_work_j()==0,
          "neutral swing command generated actuator work");
    (void)hands.advance_targets(Vec3(0,-float(stroke),0),h);
    check(hands.actuator_absorbed_work_j()>0 && hands.actuator_positive_work_j()==supplied,
          "negative rest-target work was rewarded as positive pumping");
    std::cout<<"swing spring_work="<<supplied<<" stroke="<<stroke
             <<" absorbed="<<hands.actuator_absorbed_work_j()<<" neutral_work=0\n";
}
void dynamic_recoil() {
    World w(true, Vec3(2, 0, 0));
    PhysicalHandClimb hands(w.system, w.player);
    const auto pv = w.velocity(w.player), sv = w.velocity(w.support);
    w.acquire(hands);
    check((w.velocity(w.player) - pv).LengthSq() == 0 && (w.velocity(w.support) - sv).LengthSq() == 0, "catch changed actual initial velocities");
    w.tick(hands, 45);
    const auto player_v = w.velocity(w.player), support_v = w.velocity(w.support);
    check(support_v.GetX() > 0.2F && player_v.GetX() < 1.9F, "free support did not receive reciprocal recoil");
    check(std::abs(85.0 * player_v.GetX() + 25.0 * support_v.GetX() - 170.0) < 0.05, "hand forces violated horizontal momentum balance");
    check(w.contacts.count == 0, "dynamic recoil fixture had external contact");
    hands.clear();
    check((w.velocity(w.player) - player_v).LengthSq() == 0 && (w.velocity(w.support) - support_v).LengthSq() == 0, "dynamic departure replaced body velocities");
    std::cout << "recoil player_vx=" << player_v.GetX() << " support_vx=" << support_v.GetX() << " peak_force=" << hands.peak_hand_force_n() << '\n';
}
void finite_landing_catch() {
    // An oblique landing saturates all three axes and can reject a per-axis
    // 1500N error that a purely vertical catch would not distinguish.
    World w(false, Vec3(-6, -6, -6));
    PhysicalHandClimb hands(w.system, w.player);
    w.acquire(hands);
    check((w.velocity(w.player) - Vec3(-6, -6, -6)).LengthSq() == 0, "catch froze incoming landing velocity");
    double minimum_y = w.position(w.player).GetY();
    for (unsigned i = 0; i < 540; ++i) { w.tick(hands, 1); minimum_y = std::min(minimum_y, double(w.position(w.player).GetY())); }
    check(minimum_y < 4.0 && minimum_y > 1.0, "catch was rigid snap or failed to arrest falling player");
    check(w.velocity(w.player).Length() < 0.005F, "finite catch did not settle");
    check(hands.peak_hand_force_n() > 1490 && hands.peak_hand_force_n() <= PhysicalHandClimb::force_bound_n() + 0.05F, "catch force was absent or exceeded vector bound");
    check(w.contacts.count == 0, "catch fixture had external collision");
    std::cout << "catch minimum_y=" << minimum_y << " peak_force=" << hands.peak_hand_force_n() << '\n';
}
void kinematic_tracking_and_regrip() {
    World w(false, Vec3::sZero(), true);
    auto &bodies = w.system.GetBodyInterface();
    bodies.SetLinearVelocity(w.support, Vec3(0.6F, 0, 0));
    PhysicalHandClimb hands(w.system, w.player);
    w.acquire(hands);
    w.tick(hands, 360);
    check(std::abs(w.velocity(w.player).GetX() - 0.6F) < 0.005,
          "finite hands did not acquire prescribed support motion");
    const auto velocity = w.velocity(w.player);
    const auto rest = hands.commanded_position();
    // The new real grip is 0.2 m along the same moving visible edge. A
    // replacement must preserve the old elastic extension and neutral root.
    const auto grip = w.position(w.support) + Vec3(0.0F, -0.1F, -0.1F);
    check(hands.regrip(0, w.support, grip), "moving-lip regrip rejected");
    check(Vec3(hands.commanded_position() - rest).Length() < 0.00002F,
          "regrip reset the acquired offset or spring extension");
    check((w.velocity(w.player) - velocity).LengthSq() == 0,
          "regrip overwrote actual departure momentum");
    w.tick(hands, 90);
    check(std::abs(hands.hand_force(0).GetY() + hands.hand_force(1).GetY() - weight) < 2,
          "regrip lost gravity load or double-counted player weight");
    check(hands.peak_hand_force_n() <= PhysicalHandClimb::force_bound_n() + 0.05F,
          "kinematic grip bypassed finite force bound");
    const auto departure = w.velocity(w.player);
    hands.clear();
    check((w.velocity(w.player) - departure).LengthSq() == 0,
          "kinematic release copied prescribed velocity");
    w.tick(hands, 9);
    check(std::abs(w.velocity(w.player).GetY() - departure.GetY() + 9.81F * 9 * h) < 0.002,
          "kinematic release did not restore free fall");
    check(w.contacts.count == 0, "kinematic fixture contact faked hand support");
    std::cout << "kinematic regrip tracking_vx=" << departure.GetX() << '\n';
}
void soft_material_reaction() {
    World w;
    scraperx::sim::CargoNet net(w.system,1,w.support);
    auto &bodies=w.system.GetBodyInterface();
    const Vec3 material(4,10,0);
    const auto grip=net.world_point(material);
    bodies.SetPosition(w.player,grip+Vec3(0,-0.5F,0.8F),EActivation::Activate);
    bodies.SetLinearVelocity(w.player,Vec3(2,0,0));
    PhysicalHandClimb hands(w.system,w.player,&net);
    const auto incoming=w.velocity(w.player);
    check(hands.attach(0,net.body(),grip) && hands.attach(1,net.body(),grip),"real soft grips rejected");
    check((w.velocity(w.player)-incoming).LengthSq()==0,"soft catch overwrote incoming momentum");
    const auto before=net.material_velocity(material);
    hands.pre_step(h/4);
    const auto rider_delta=w.velocity(w.player)-incoming;
    const auto rope_delta=net.material_velocity(material)-before;
    // Exact knot: four vertices of0.06kg, each with sampler weight1/4.
    // No gravity/integration/contact occurs between the velocity samples.
    check((rider_delta*85.0F+rope_delta*0.24F).Length()<0.00003F,"soft grip failed reciprocal momentum balance");
    check(rope_delta.GetX()>0.01F && rider_delta.GetX()<0,"soft catch did not recoil actual rope material");
    check(hands.hand_force(0).Length()>0 && hands.hand_force(0).Length()<=1500.01F,"soft grip force absent or unbounded");
    const auto neutral=hands.commanded_position();
    const auto velocity=w.velocity(w.player);
    check(hands.regrip(0,net.body(),net.world_point(Vec3(5,10,0))),"soft regrip rejected");
    check(Vec3(hands.commanded_position()-neutral).Length()<0.00003F,"soft regrip reset spring extension");
    check((w.velocity(w.player)-velocity).LengthSq()==0,"soft regrip changed momentum");
    (void)hands.advance_targets(Vec3(0,100,0),h);
    check(hands.last_command_work_bound_j()>33.3 && hands.last_command_work_bound_j()<=3000.0*h+0.0001,"soft command bypassed shared work bound");
    hands.clear();
    check((w.velocity(w.player)-velocity).LengthSq()==0 && !hands.active(),"soft release changed momentum or left attachments");
    std::cout<<"soft reciprocal impulse residual="<<(rider_delta*85.0F+rope_delta*0.24F).Length()<<'\n';
}
void net_frame_reaction() {
    World w;
    scraperx::sim::kit::Kit kit(w.system,0,1);
    const auto frame=scraperx::sim::build_cargo_net_route(kit);
    scraperx::sim::CargoNet net(w.system,1,kit.body_id(frame));
    struct Attachments final : PhysicsStepListener {
        scraperx::sim::CargoNet &net;
        explicit Attachments(scraperx::sim::CargoNet &n):net(n) {}
        void OnStep(const PhysicsStepListenerContext &c) override { net.pre_step(c.mDeltaTime); }
    } attachments(net);
    w.system.AddStepListener(&attachments);
    const auto tick=[&](int count) {
        while(count-->0) {
            check(w.system.Update(h,4,&w.temporary,&w.jobs)==EPhysicsUpdateError::None,"net-frame native update failed");
            net.refresh();
        }
    };
    tick(120);
    const auto before=net.world_point(Vec3(4,22,0));
    auto &bodies=w.system.GetBodyInterface();
    bodies.AddImpulse(kit.body_id(frame),Vec3(8000,0,0));
    const auto frame_v=bodies.GetLinearVelocity(kit.body_id(frame));
    Vec3 rope_v[18]; unsigned node=0;
    for(int row:{0,22}) for(int column=0;column<9;++column)
        rope_v[node++]=net.material_velocity(Vec3(float(column),float(row),0));
    net.pre_step(h/4);
    auto residual=(bodies.GetLinearVelocity(kit.body_id(frame))-frame_v)*kit.body_mass(frame);
    node=0;
    for(int row:{0,22}) for(int column=0;column<9;++column)
        residual+=(net.material_velocity(Vec3(float(column),float(row),0))-rope_v[node++])*0.24F;
    check(residual.Length()<0.02F,"attachment forces failed net/frame momentum balance");
    tick(6);
    const auto after=net.world_point(Vec3(4,22,0));
    check(std::abs(after.GetX()-before.GetX())>0.001,"net ends stayed pinned in world as frame moved");
    check(finite(after) && kit.body_kinetic_energy(frame)>0,"finite structure failed actual impulse response");
    w.system.RemoveStepListener(&attachments);
    std::cout<<"net-frame mass="<<kit.body_mass(frame)<<" anchor_dx="<<after.GetX()-before.GetX()
             <<" momentum_residual="<<residual.Length()<<'\n';
}
void facade_structural_response() {
    World w;
    scraperx::sim::kit::Kit kit(w.system,0,1);
    scraperx::sim::build_facade_route(kit);
    check(kit.body_count()==8,"facade structural ownership is incomplete");
    for(unsigned i=0;i<kit.body_count();++i) {
        check(kit.body_mass({i})>0,"facade member remains massless/static");
        check(w.system.GetBodyInterface().GetMotionType(kit.body_id({i}))==EMotionType::Dynamic,
              "facade member does not participate in native dynamics");
    }
    auto &bodies=w.system.GetBodyInterface();
    const auto ladder=kit.body_for_entity(2566);
    const auto carriage=kit.body_for_entity(2564);
    const auto point=[&](scraperx::sim::kit::BodyIndex b,RVec3 authored) {
        return bodies.GetWorldTransform(kit.body_id(b))*Vec3(authored);
    };
    for(unsigned i=0;i<90;++i) w.system.Update(h,4,&w.temporary,&w.jobs);
    const auto rung=point(ladder,{12.5,35.6,-120.15});
    const auto neutral_carriage=bodies.GetPosition(kit.body_id(carriage));
    bodies.AddImpulse(kit.body_id(ladder),Vec3(100,0,0),rung);
    bodies.AddImpulse(kit.body_id(carriage),Vec3(0,0,250));
    float deflection=0;
    for(unsigned i=0;i<30;++i) {
        w.system.Update(h,4,&w.temporary,&w.jobs);
        deflection=std::max(deflection,float((point(ladder,{12.5,35.6,-120.15})-rung).Length()));
    }
    const auto carriage_travel=bodies.GetPosition(kit.body_id(carriage))-neutral_carriage;
    check(deflection>0.0001F && deflection<0.20F,"mounted ladder has no finite bounded load response");
    check(carriage_travel.GetZ()>0.01 && carriage_travel.GetZ()<1.10,
          "trolley failed passive actual-impulse rail travel");
    check(finite(carriage_travel),"facade coupling produced nonfinite state");
    std::cout<<"facade ladder_deflection_m="<<deflection
        <<" carriage_travel_m="<<carriage_travel.GetZ()<<'\n';
}
void invalid_holds_and_cleanup() {
    World w;
    {
        PhysicalHandClimb hands(w.system, w.player);
        check(!hands.attach(2, w.support, RVec3::sZero()), "invalid hand index accepted");
        check(!hands.attach(0, BodyID(), RVec3::sZero()), "invalid body accepted");
        check(!hands.attach(0, w.player, RVec3::sZero()), "self grip accepted");
        w.acquire(hands);
        Ref<SoftBodySharedSettings> mesh = new SoftBodySharedSettings;
        for (auto p : { Vec3(0, 0, 0), Vec3(1, 0, 0), Vec3(0, 1, 0) }) {
            SoftBodySharedSettings::Vertex v; p.StoreFloat3(&v.mPosition); mesh->mVertices.push_back(v);
        }
        SoftBodySharedSettings::Face f; f.mVertex[0] = 0; f.mVertex[1] = 1; f.mVertex[2] = 2; mesh->AddFace(f);
        const SoftBodySharedSettings::VertexAttributes material(1e-5F, 1e-5F, 1e-5F);
        mesh->CreateConstraints(&material, 1); mesh->Optimize();
        SoftBodyCreationSettings settings(mesh, RVec3(10, 10, 0), Quat::sIdentity(), 1);
        const auto soft = w.system.GetBodyInterface().CreateAndAddSoftBody(settings, EActivation::Activate);
        check(!soft.IsInvalid(), "soft-body rejection fixture failed allocation"); w.ids.push_back(soft);
        check(!hands.attach(0, soft, RVec3(10, 10, 0)), "rigid-hand adapter accepted soft body");
        check(hands.attached(0) && w.system.GetConstraints().size() == 2, "invalid replacement destroyed good grip");
        check(!hands.attach(0, w.support, RVec3(std::numeric_limits<float>::infinity(), 0, 0)), "nonfinite grip accepted");
    }
    check(w.system.GetConstraints().empty(), "destructor leaked native constraints");
}
}
int main(int argc,char **argv) {
    JPH::RegisterDefaultAllocator(); JPH::Factory::sInstance = new JPH::Factory; JPH::RegisterTypes();
    unsigned failures = 0;
    const bool fixed_only=argc>1&&std::string(argv[1])=="free_arc_fixed_light";
    const bool radial_only=argc>1&&std::string(argv[1])=="free_arc";
    const bool handoff_only=argc>1&&std::string(argv[1])=="free_arc_handoff";
    const bool angular_only=argc>1&&std::string(argv[1])=="free_arc_angular";
    const std::vector<std::pair<const char *, void (*)()>> tests=fixed_only?
        std::vector<std::pair<const char *,void (*)()>>{{"radial_fixed_light_catch",radial_fixed_light_catch}}:angular_only?
        std::vector<std::pair<const char *,void (*)()>>{{"radial_dynamic_reciprocity",radial_dynamic_reciprocity}}:handoff_only?
        std::vector<std::pair<const char *,void (*)()>>{{"radial_work_and_handoffs",radial_work_and_handoffs}}:radial_only?
        std::vector<std::pair<const char *, void (*)()>> {{"passive_radial_arc",passive_radial_arc},
          {"radial_dynamic_reciprocity",radial_dynamic_reciprocity},{"radial_work_and_handoffs",radial_work_and_handoffs}}:
        std::vector<std::pair<const char *, void (*)()>> {
        { "static_load_and_release", static_load_and_release }, { "command_sign_and_budget", command_sign_and_budget },
        { "measured_swing_actuation", measured_swing_actuation },
        { "dynamic_recoil", dynamic_recoil }, { "finite_landing_catch", finite_landing_catch },
        { "kinematic_tracking_and_regrip", kinematic_tracking_and_regrip },
        { "soft_material_reaction", soft_material_reaction },
        { "net_frame_reaction", net_frame_reaction },
        { "facade_structural_response", facade_structural_response },
        { "invalid_holds_and_cleanup", invalid_holds_and_cleanup },
        {"passive_radial_arc",passive_radial_arc},{"radial_dynamic_reciprocity",radial_dynamic_reciprocity},
        {"radial_work_and_handoffs",radial_work_and_handoffs},{"radial_fixed_light_catch",radial_fixed_light_catch} };
    for (const auto &test : tests) {
        try { test.second(); std::cout << "PASS " << test.first << '\n'; }
        catch (const std::exception &e) { ++failures; std::cerr << "FAIL " << test.first << ": " << e.what() << '\n'; }
    }
    JPH::UnregisterTypes(); delete JPH::Factory::sInstance; JPH::Factory::sInstance = nullptr;
    std::cout << "physical hand checks: " << tests.size() - failures << "/"<<tests.size()<<"\n";
    return failures ? 1 : 0;
}
