#include "sim/finite_cable_constraint.hpp"
#include "sim/finite_line_reel.hpp"
#include "sim/rounded_box_line_path.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/StateRecorderImpl.h>
#include <Jolt/RegisterTypes.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace scraperx::sim;
using namespace JPH;
namespace {
constexpr float h=1.0F/360.0F;
float max_tension_difference=0,max_velocity_difference=0;
void check(bool ok,const char *message) { if(!ok) throw std::runtime_error(message); }
void near(double a,double b,double tolerance,const char *message) {
    if(std::abs(a-b)>tolerance) { std::cerr<<a<<" != "<<b<<' '; check(false,message); }
}
struct BP final:BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override { return BroadPhaseLayer(layer); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "finite-cable-check"; }
};
struct BV final:ObjectVsBroadPhaseLayerFilter { bool ShouldCollide(ObjectLayer,BroadPhaseLayer) const override { return false; } };
struct LP final:ObjectLayerPairFilter { bool ShouldCollide(ObjectLayer,ObjectLayer) const override { return false; } };
struct World {
    BP bp; BV bv; LP lp; PhysicsSystem world;
    TempAllocatorImpl allocator{8*1024*1024}; JobSystemSingleThreaded jobs{1024};
    World() { world.Init(32,0,32,32,bp,bv,lp); world.SetGravity(Vec3::sZero()); }
    Body *body(RVec3Arg position,float mass,EMotionType type=EMotionType::Dynamic,Vec3 half=Vec3(.3F,.3F,.3F),float radius=.05F) {
        BodyCreationSettings settings(new BoxShape(half,radius),position,Quat::sIdentity(),type,1);
        settings.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass=mass;
        settings.mLinearDamping=settings.mAngularDamping=0; settings.mAllowSleeping=false;
        const auto id=world.GetBodyInterface().CreateAndAddBody(settings,EActivation::Activate);
        BodyLockWrite lock(world.GetBodyLockInterface(),id); check(lock.Succeeded(),"body exists"); return &lock.GetBody();
    }
    void tick(int substeps=1) { check(world.Update(h*substeps,substeps,&allocator,&jobs)==EPhysicsUpdateError::None,"actual Jolt update"); }
};
struct Straight final:World,PhysicsStepListener {
    Body *a,*b; Vec3 eye{0,.4F,0}; Ref<FiniteCableConstraint> cable; Ref<GrappleRope> rope;
    unsigned steps=0; bool repeated=false;
    Straight(bool finite,EMotionType type=EMotionType::Dynamic,float rest=2,Vec3 local_eye=Vec3(0,.4F,0),bool drive=false):eye(local_eye) {
        a=body(RVec3::sZero(),85); b=body(RVec3(2.2F,0,0),20,type);
        if(finite) {
            cable=new FiniteCableConstraint(rest); cable->set_drive_limited(drive); refresh();
            world.AddConstraint(cable); world.AddStepListener(this);
        } else {
            GrappleRopeSettings settings; settings.mLocalEye=eye; settings.mRestLength=rest;
            rope=static_cast<GrappleRope *>(settings.Create(*a,*b)); rope->set_drive_limited(drive); world.AddConstraint(rope);
        }
    }
    ~Straight() { if(cable) { world.RemoveStepListener(this); world.RemoveConstraint(cable); } if(rope) world.RemoveConstraint(rope); }
    void refresh() {
        const RVec3 point=b->GetCenterOfMassTransform()*eye;
        const Vec3 delta(point-a->GetCenterOfMassPosition()); const Vec3 axis=delta.Normalized();
        const Vec3 angular=Vec3(point-b->GetCenterOfMassPosition()).Cross(axis);
        FiniteCableConstraint::Participant p[]={{a,-axis,Vec3::sZero()},{b,axis,angular},
            {a,-.25F*axis,Vec3::sZero()},{a,.25F*axis,Vec3::sZero()}};
        check(cable->set_geometry(p,repeated?4:2,delta.Length())==FiniteCableConstraint::Admission::Accepted,"straight admission");
    }
    void OnStep(const PhysicsStepListenerContext &context) override { near(context.mDeltaTime,h,1e-9,"actual collision dt"); ++steps; refresh(); }
};
void straight_agreement() {
    for(const auto type:{EMotionType::Dynamic,EMotionType::Static,EMotionType::Kinematic}) {
        Straight finite(true,type),shared(false,type);
        finite.repeated=true;
        if(type==EMotionType::Kinematic) {
            finite.b->SetLinearVelocity(Vec3(.2F,0,0)); shared.b->SetLinearVelocity(Vec3(.2F,0,0));
            finite.b->SetAngularVelocity(Vec3(0,0,.5F)); shared.b->SetAngularVelocity(Vec3(0,0,.5F));
        }
        for(int i=0;i<24;++i) {
            finite.tick(); shared.tick();
            const float td=std::abs(finite.cable->tension()-shared.rope->tension());
            const float vd=(finite.a->GetLinearVelocity()-shared.a->GetLinearVelocity()).Length();
            max_tension_difference=std::max(max_tension_difference,td); max_velocity_difference=std::max(max_velocity_difference,vd);
            near(td,0,.08,"finite tension agrees with shared rope"); near(vd,0,2e-5,"finite velocity agrees with shared rope");
            near((finite.b->GetAngularVelocity()-shared.b->GetAngularVelocity()).Length(),0,2e-5,"offcentre angular response agrees");
            check(!finite.cable->broken() && finite.cable->participant_count()==2,"repeated bodies combine");
        }
    }
}
void slack_break_caps_release() {
    Straight slack(true,EMotionType::Dynamic,3); slack.tick(4);
    check(slack.steps==4 && slack.cable->tension()==0 && slack.a->GetLinearVelocity().LengthSq()==0,"slack and collision callback cannot push");
    Straight stretched(true,EMotionType::Dynamic,1); stretched.tick();
    check(stretched.cable->broken() && stretched.cable->tension()==0 && stretched.a->GetLinearVelocity().LengthSq()==0,"stretch breaks before impulses");
    Straight demanded(true); demanded.a->SetLinearVelocity(Vec3(-30,0,0)); demanded.tick();
    check(demanded.cable->broken() && demanded.cable->tension()==0,"excess damping demand breaks before impulses");
    Straight passive(true,EMotionType::Static,1.8F,Vec3::sZero());
    Straight driven(true,EMotionType::Static,1.8F,Vec3::sZero(),true);
    passive.tick(); driven.tick();
    check(passive.cable->tension()>1500 && passive.cable->tension()<=3000,"passive retains material rating");
    check(driven.cable->tension()>1499 && driven.cable->tension()<=1500,"active drive caps at 1500N");
    for(int i=0;i<12;++i) { passive.tick(); driven.tick(); check(passive.cable->tension()<=3000 && driven.cable->tension()<=1500,"cold stepping cannot overshoot cap"); }
    const Vec3 velocity=driven.a->GetLinearVelocity(),support_velocity=driven.b->GetLinearVelocity();
    driven.world.RemoveStepListener(&driven); driven.world.RemoveConstraint(driven.cable); driven.cable=nullptr;
    check((driven.a->GetLinearVelocity()-velocity).LengthSq()==0,"removal preserves velocity");
    driven.tick(); check((driven.a->GetLinearVelocity()-velocity).LengthSq()==0 &&
        (driven.b->GetLinearVelocity()-support_velocity).LengthSq()==0,"release carries momentum");
}
void prospective_stall() {
    Straight f(true,EMotionType::Static,2.2F,Vec3::sZero()); f.a->SetLinearVelocity(Vec3(-10,0,0)); f.refresh();
    const double demand=f.cable->prospective_tension(2.2F,h);
    const double damping=GrappleRopeSettings::damping_ns_per_m+h*GrappleRopeSettings::stiffness_n_per_m;
    const double expected=damping*10/(1+h*damping/85);
    near(demand,expected,.001,"prospective includes current point damping and effective mass");
    check(demand>1500 && demand<3000,"stall fixture in physical range");
    const auto result=FiniteLineReel::step(2.2F,2.2F,0,h,1,.2F,40,
        [&](float rest,float dt) { return f.cable->prospective_tension(rest,dt); });
    check(result.rest_length_m==2.2F && result.work_j==0,"current prospective demand stalls reel despite prior zero load");
    Straight duplicate(true); duplicate.refresh();
    const double original=duplicate.cable->prospective_tension(2,h); duplicate.repeated=true; duplicate.refresh();
    near(duplicate.cable->prospective_tension(2,h),original,1e-5,"duplicate body terms combined before effective mass");
}
void admission_checkpoint() {
    World f; std::array<Body *,10> bodies{};
    for(int i=0;i<10;++i) bodies[i]=f.body(RVec3(float(i),0,0),20);
    FiniteCableConstraint row(2);
    std::array<FiniteCableConstraint::Participant,10> p{};
    for(int i=0;i<10;++i) p[i]={bodies[i],Vec3::sAxisX(),Vec3::sZero()};
    check(row.set_geometry(p.data(),9,2.2)==FiniteCableConstraint::Admission::Accepted,"capacity nine accepted");
    check(row.set_geometry(p.data(),10,2.2)==FiniteCableConstraint::Admission::CapacityExceeded && !row.geometry_valid() && !row.broken(),"capacity overflow explicit and clears stale geometry");
    p[0].linear=Vec3(std::numeric_limits<float>::quiet_NaN(),0,0);
    check(row.set_geometry(p.data(),1,2.2)==FiniteCableConstraint::Admission::InvalidGeometry,"nonfinite derivatives rejected");
    check(!row.set_rest(-1) && row.rest()==2,"invalid rest rejected without changing state");
    p[0]={f.body(RVec3::sZero(),20,EMotionType::Static),Vec3::sAxisX(),Vec3::sZero()};
    check(row.set_geometry(p.data(),1,2.2)==FiniteCableConstraint::Admission::NoDynamicBody,"no dynamic participant explicit");
    StateRecorderImpl recorder; bool save=false,restore=false;
    try { row.SaveState(recorder); } catch(const std::logic_error &) { save=true; }
    try { row.RestoreState(recorder); } catch(const std::logic_error &) { restore=true; }
    check(save && restore && row.GetConstraintSettings()==nullptr,"checkpoint requires cold owner rebuild");
}
Vec3 linear_momentum(Body *p,float mass) { return mass*p->GetLinearVelocity(); }
Vec3 angular_momentum(Body *p,float mass) {
    const auto inertia=p->GetInverseInertia().Inversed3x3();
    return Vec3(p->GetCenterOfMassPosition()).Cross(linear_momentum(p,mass))+inertia.Multiply3x3(p->GetAngularVelocity());
}
struct Beam final:World,PhysicsStepListener {
    Body *a,*b,*beam; Ref<FiniteCableConstraint> cable; double last_length=0;
    Beam() {
        a=body(RVec3(-3,0,-.7F),85); b=body(RVec3(3,0,.8F),20); beam=body(RVec3::sZero(),40,EMotionType::Dynamic,Vec3(1,1,5),.2F);
        beam->SetLinearVelocity(Vec3(.1F,.02F,0)); beam->SetAngularVelocity(Vec3(0,0,.1F));
        cable=new FiniteCableConstraint(6.3F); refresh(); world.AddConstraint(cable); world.AddStepListener(this);
    }
    ~Beam() { world.RemoveStepListener(this); world.RemoveConstraint(cable); }
    void refresh() {
        RoundedBoxLineQuery q; q.leaf_to_world=beam->GetCenterOfMassTransform();
        const auto *box=static_cast<const BoxShape *>(beam->GetShape()); q.half_extent=box->GetHalfExtent(); q.convex_radius=box->GetConvexRadius();
        q.endpoint_a=a->GetCenterOfMassPosition(); q.endpoint_b=b->GetCenterOfMassPosition(); q.retained={2,1};
        const auto path=rounded_box_line_path(q); check(path.status==RoundedBoxLineStatus::Wrapped,"actual moving rounded beam path");
        const Vec3 obstacle_linear=-path.gradient_a-path.gradient_b;
        const Vec3 obstacle_angular=-Vec3(q.endpoint_a-beam->GetCenterOfMassPosition()).Cross(path.gradient_a)-
            Vec3(q.endpoint_b-beam->GetCenterOfMassPosition()).Cross(path.gradient_b);
        const FiniteCableConstraint::Participant p[]={{a,path.gradient_a,Vec3::sZero()},
            {b,path.gradient_b,Vec3::sZero()},{beam,obstacle_linear,obstacle_angular}};
        check(cable->set_geometry(p,3,path.length_m)==FiniteCableConstraint::Admission::Accepted,"beam participants admitted"); last_length=path.length_m;
    }
    void OnStep(const PhysicsStepListenerContext &) override { refresh(); }
};
void moving_beam_reciprocity() {
    Beam f;
    check(f.last_length>f.cable->rest() && f.last_length-f.cable->rest()<.5,"beam elastic range");
    const Vec3 p0=linear_momentum(f.a,85)+linear_momentum(f.b,20)+linear_momentum(f.beam,40);
    const Vec3 l0=angular_momentum(f.a,85)+angular_momentum(f.b,20)+angular_momentum(f.beam,40);
    const Vec3 old_v=f.beam->GetLinearVelocity(),old_w=f.beam->GetAngularVelocity(); f.tick();
    check(!f.cable->broken() && f.cable->tension()>0,"wrapped row loads");
    const Vec3 p1=linear_momentum(f.a,85)+linear_momentum(f.b,20)+linear_momentum(f.beam,40);
    const Vec3 l1=angular_momentum(f.a,85)+angular_momentum(f.b,20)+angular_momentum(f.beam,40);
    const double pe=(p1-p0).Length(),le=(l1-l0).Length();
    near(pe,0,1e-4,"three dynamic participants conserve linear momentum");
    near(le,0,.003,"three dynamic participants conserve angular momentum within orientation integration error");
    check((f.beam->GetLinearVelocity()-old_v).Length()>1e-3 && (f.beam->GetAngularVelocity()-old_w).Length()>1e-4,"real obstacle receives linear and angular response");
    Vec3 force=Vec3::sZero(),moment=Vec3::sZero();
    for(std::size_t i=0;i<3;++i) { const auto &r=f.cable->reaction(i); force+=r.force_n; moment+=r.torque_nm; }
    near(force.Length(),0,.001,"reciprocal reaction force readback");
    check(moment.Length()>1,"obstacle torque readback is nonzero");
    std::cout<<"moving_beam tension_n="<<f.cable->tension()<<" linear_momentum_error_kg_m_s="<<pe<<" angular_momentum_error_kg_m2_s="<<le<<'\n';
}
}
int main() {
    RegisterDefaultAllocator(); Factory::sInstance=new Factory; RegisterTypes(); int result=0;
    try {
        straight_agreement(); slack_break_caps_release(); prospective_stall(); admission_checkpoint(); moving_beam_reciprocity();
        std::cout<<"max_shared_tension_difference_n="<<max_tension_difference<<" max_shared_velocity_difference_m_s="<<max_velocity_difference<<'\n';
        std::cout<<"PASS finite_cable_constraint shared_agreement slack caps cold_step break reciprocal_linear_angular prospective_stall release admission cold_checkpoint\n";
    } catch(const std::exception &e) { std::cerr<<"FAIL "<<e.what()<<'\n'; result=1; }
    UnregisterTypes(); delete Factory::sInstance; Factory::sInstance=nullptr; return result;
}
