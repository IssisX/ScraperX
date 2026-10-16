// Actual production forklift/crane assembly regression for the pinned-Jolt
// finite neutral-friction position projection. No body pose/velocity overrides.
#include "sim/yard_crane.hpp"
#include "sim/warehouse_forklift_yard.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
using namespace scraperx::sim;
using namespace JPH;
namespace {
void require(bool ok, const char *why) {
    if (!ok) { std::cerr << "FAIL yard crane: " << why << std::endl; std::exit(1); }
}
struct BP final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer v) const override { return BroadPhaseLayer(v); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "crane"; }
};
struct BF final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override { return a || b.GetValue(); }
};
struct LF final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override { return a || b; }
};
struct Fixture {
    BP bp; BF bf; LF lf;
    PhysicsSystem world;
    TempAllocatorImpl temp{64 * 1024 * 1024};
    JobSystemSingleThreaded jobs{2048};
    std::unique_ptr<kit::Kit> kit;
    std::unique_ptr<WarehouseForklift> truck;
    std::unique_ptr<YardCrane> crane;
    struct WeldSteps final : PhysicsStepListener {
        kit::Kit *kit = nullptr;
        void OnStep(const PhysicsStepListenerContext &c) override { kit->begin_weld_step(c.mDeltaTime); }
    } weld_steps;
    Fixture() {
        world.Init(256, 0, 2048, 16384, bp, bf, lf);
        world.SetGravity(Vec3(0, -9.81F, 0));
        kit = std::make_unique<kit::Kit>(world, 0, 1);
        kit::Part ground; ground.half = Vec3(100, .25F, 100);
        kit->add_body(1400, {ground}, RVec3(-65, -.25, -150), Quat::sIdentity(), 0, .8F);
        // Authored initial assembly for the mechanism fixture. The full world
        // must drive its existing truck here; no operation below writes poses.
        const auto yaw = Quat::sRotation(Vec3::sAxisY(), JPH_PI * .5F);
        const auto yard = build_warehouse_forklift_yard(*kit, RVec3(-94, 0, -150), yaw);
        truck = std::make_unique<WarehouseForklift>(world, *kit, yard.truck);
        crane = std::make_unique<YardCrane>(world, *kit, yard.truck);
        weld_steps.kit = kit.get(); world.AddStepListener(&weld_steps);
        world.OptimizeBroadPhase();
    }
    ~Fixture() {
        world.RemoveStepListener(&weld_steps); crane.reset(); truck.reset(); kit.reset();
    }
    void tick(YardCrane::Command command = {}, unsigned n = 1,
              WarehouseForklift::Command drive = {}) {
        while (n--) {
            kit->pre_step(1.F / 90); truck->pre_step(drive); crane->pre_step(command);
            require(world.Update(1.F / 90, 4, &temp, &jobs) == EPhysicsUpdateError::None, "native update");
            kit->finish_weld_steps(); truck->post_step(); crane->post_step(); kit->post_step(1.F / 90);
        }
    }
    double y() { return kit->body_position(truck->config().chassis).GetY(); }
    void stow_transport() {
        // Raise while the mast is neutral so forward tilt cannot ground
        // the long tips and lift the front tires off their support.
        WarehouseForklift::Command raise; raise.lift = 1;
        for (unsigned i = 0; i < 360 && truck->state().lift_position_m < .35F; ++i) tick({}, 1, raise);
        require(truck->state().lift_position_m >= .35F, "finite hydraulics raise actual carriage for tine clearance");
        tick({}, 30);
        WarehouseForklift::Command stow; stow.tilt = -1;
        const auto ready = [&] {
            const auto s = truck->state();
            return std::abs(s.tilt_angle_rad - truck->config().tilt_min_rad) < .002F;
        };
        for (unsigned i = 0; i < 360 && !ready(); ++i) tick({}, 1, stow);
        require(ready(), "finite truck hydraulics reach actual forward transport stop with raised forks");
        tick({}, 90);
    }
    void align_empty_rig(bool first_leg_only = false) {
        // Guide the actual empty rig through its finite drives. The free ends
        // remain the vertically hanging ends of the authored fixed stock.
        const auto eyes = WarehouseForkliftYard::lifting_points();
        const auto &g = crane->geometry();
        const Vec3 initial_axis = kit->body_rotation(g.boom) * Vec3::sAxisZ();
        const auto initial = crane->state();
        float target_trolley = initial.trolley_position_m;
        const auto truck_frame = world.GetBodyInterface().GetWorldTransform(kit->body_id(truck->config().chassis));
        for (unsigned leg = 0; leg < eyes.size(); ++leg)
            target_trolley += Vec3(truck_frame * eyes[leg] - initial.loose_ends[leg]).Dot(initial_axis) / float(eyes.size());
        bool aligned = false;
        for (unsigned i = 0; i < 1800; ++i) {
            const auto s = crane->state();
            const auto *chassis = world.GetBodyLockInterfaceNoLock().TryGetBody(kit->body_id(truck->config().chassis));
            const auto *spreader = world.GetBodyLockInterfaceNoLock().TryGetBody(kit->body_id(g.spreader));
            const Vec3 axis = kit->body_rotation(g.boom) * Vec3::sAxisZ();
            float excess = -100;
            bool fits = true;
            for (unsigned leg = 0; leg < eyes.size(); ++leg) {
                const auto eye = chassis->GetWorldTransform() * eyes[leg];
                const auto top = spreader->GetWorldTransform() * g.spreader_eyes[leg];
                const Vec3 error(eye - s.loose_ends[leg]);
                const Vec3 speed(chassis->GetPointVelocity(eye) - spreader->GetPointVelocity(s.loose_ends[leg]));
                excess = std::max(excess, float((eye-top).Length()) - g.sling_lengths[leg]);
                fits = fits && error.LengthSq() <= .06F * .06F &&
                       (eye-top).Length() <= g.sling_lengths[leg] + .0001F && speed.Length() <= .15F;
            }
            bool supported = chassis->GetLinearVelocity().Length() < .10F &&
                             chassis->GetAngularVelocity().Length() < .05F;
            for (const auto &wheel : truck->wheels())
                supported = supported && wheel.contact && wheel.contact_normal.GetY() >= .7F && wheel.normal_force_n > 0;
            if (fits && supported) {
                tick({}, 90);
                if (first_leg_only ? crane->try_attach_leg(0, *truck) : crane->try_attach(*truck)) { aligned = true; break; }
                continue;
            }
            YardCrane::Command guide;
            // Move the finite carriage to one measured pickup target. Chasing
            // the long suspended pendulum directly would excite its swing.
            const auto *trolley = world.GetBodyLockInterfaceNoLock().TryGetBody(kit->body_id(g.trolley));
            const float carriage_error = target_trolley - s.trolley_position_m;
            if (std::abs(carriage_error) > .001F)
                guide.trolley = std::clamp(4 * carriage_error - 4 * trolley->GetLinearVelocity().Dot(axis), -.2F, .2F);
            // Pay out only until every actual endpoint is inside fixed stock;
            // the passive neutral brake then holds that attained target.
            if (excess > -.001F) guide.hoist = -.02F;
            tick(guide);
            if (crane->state().failure != YardCrane::Failure::None) break;
        }
        require(aligned, "finite trolley and hoist align supported truck with real fixed-stock rig");
    }
};
}
struct Observe final : PhysicsStepListener {
 SliderConstraint *lift=nullptr; double integrated=0, abs_integrated=0; float max_speed=0,max_force=0; unsigned count=0; float max_budget_ratio=0,max_joint_anchor=0,max_fixed_angle=0,max_hinge_angle=0,max_end_residual=0;
 void joints(PhysicsSystem &world) {
 max_end_residual=std::max(max_end_residual,std::abs(lift->GetCurrentPosition()-lift->GetFrictionPositionTarget()));
 const float budget=lift->GetFrictionPositionBudget(); if(budget>0)max_budget_ratio=std::max(max_budget_ratio,std::abs(lift->GetFrictionPositionLambda())/budget);
 for(const auto &c:world.GetConstraints())if(c->GetType()==EConstraintType::TwoBodyConstraint && (c->GetSubType()==EConstraintSubType::Fixed || c->GetSubType()==EConstraintSubType::Hinge)) {
 const auto*t=static_cast<const TwoBodyConstraint*>(c.GetPtr());
 if(t->GetBody1()!=lift->GetBody1() && t->GetBody2()!=lift->GetBody1() && t->GetBody1()!=lift->GetBody2() && t->GetBody2()!=lift->GetBody2())continue;
 const auto a=t->GetBody1()->GetCenterOfMassTransform()*t->GetConstraintToBody1Matrix(),b=t->GetBody2()->GetCenterOfMassTransform()*t->GetConstraintToBody2Matrix();
 max_joint_anchor=std::max(max_joint_anchor,float((a.GetTranslation()-b.GetTranslation()).Length()));
 if(c->GetSubType()==EConstraintSubType::Fixed)max_fixed_angle=std::max({max_fixed_angle,(a.GetAxisX().Cross(b.GetAxisX())).Length(),(a.GetAxisY().Cross(b.GetAxisY())).Length(),(a.GetAxisZ().Cross(b.GetAxisZ())).Length()});
 else max_hinge_angle=std::max(max_hinge_angle,(a.GetAxisX().Cross(b.GetAxisX())).Length());
 }
 }
 void sample(float dt) {
 const Body &a=*lift->GetBody1(),&b=*lift->GetBody2();
 const auto f1=lift->GetConstraintToBody1Matrix(),f2=lift->GetConstraintToBody2Matrix();
 const Vec3 r1=a.GetRotation()*f1.GetTranslation(),r2=b.GetRotation()*f2.GetTranslation();
 const Vec3 n=a.GetRotation()*f1.GetAxisX();
 const Vec3 u=Vec3(b.GetCenterOfMassPosition()-a.GetCenterOfMassPosition())+r2-r1;
 const float v=(b.GetLinearVelocity()-a.GetLinearVelocity()+b.GetAngularVelocity().Cross(r2)-a.GetAngularVelocity().Cross(r1+u)).Dot(n);
 integrated+=v*dt;abs_integrated+=std::abs(v)*dt;max_speed=std::max(max_speed,std::abs(v));max_force=std::max(max_force,std::abs(lift->GetTotalLambdaMotor()/dt));++count;
 }
 void OnStep(const PhysicsStepListenerContext&c)override {sample(c.mDeltaTime);}
};
int main() {
 std::cout<<std::unitbuf<<std::setprecision(12);RegisterDefaultAllocator(); Factory::sInstance=new Factory;RegisterTypes();
 { Fixture f;f.tick({},180); f.stow_transport();
 f.align_empty_rig();YardCrane::Command hoist;hoist.hoist=1;f.tick(hoist,360);f.tick({},90);require(f.crane->state().failure==YardCrane::Failure::None && f.y()>1.5,"actual crane suspends actual truck");
 Observe obs;for(const auto&c:f.world.GetConstraints())if(c->GetSubType()==EConstraintSubType::Slider){auto*l=static_cast<SliderConstraint*>(c.GetPtr());if(l->GetBody1()->GetID()==f.kit->body_id(f.truck->config().mast))obs.lift=l;}
 require(obs.lift!=nullptr,"actual truck lift found");f.world.AddStepListener(&obs);
 require(obs.lift->GetUseFiniteFrictionPositionCorrection(),"production forklift constructor enables its actual lift finite-friction correction");
 const float q0=obs.lift->GetCurrentPosition();


 const auto before_neutral=f.truck->state();
 for(unsigned i=0;i<9000;++i){f.tick();obs.joints(f.world);}
 const auto after_neutral=f.truck->state();const float dq=obs.lift->GetCurrentPosition()-q0;
 // Freight clearance exceeds80mm. Allow10% (8mm) to lift drift over100s;
 // the established red fixture exports85.137mm outside integrated motion.
 require(std::abs(dq)<.008F && std::abs(dq-obs.integrated)<.00425F,"finite brake removes more than95percent of split-projection drift while preserving integrated motion");
 require(obs.max_joint_anchor<.0005F && obs.max_fixed_angle<.0005F && obs.max_hinge_angle<.0005F,"neutral holds adjacent anchors/axes within halfmillimeter/halfmilliradian");
 require(obs.max_budget_ratio<=1 && obs.max_end_residual<1e-6F,"one signed finite projection budget covers the entire collision substep");
 require(before_neutral.battery_j==after_neutral.battery_j && before_neutral.hydraulic_positive_work_j==after_neutral.hydraulic_positive_work_j,"neutral100s correction draws no battery or hydraulic source work");
 std::cout<<"NEUTRAL100 dq="<<dq<<" integrated="<<obs.integrated<<" joint_anchor="<<obs.max_joint_anchor<<" fixed_angle="<<obs.max_fixed_angle<<std::endl;
 struct Controls final : PhysicsStepListener {
 SliderConstraint *lift; bool released=false; unsigned saturated=0; float max_ratio=0; unsigned zero_budget=0;
 void OnStep(const PhysicsStepListenerContext &c) override {
 const float cap=lift->GetMaxFrictionForce()*c.mDeltaTime;
 if(cap>0 && std::abs(lift->GetTotalLambdaMotor())>=cap*(1-1e-6F)){++saturated;if(lift->GetFrictionPositionBudget()==0)++zero_budget;}
 if(cap>0)max_ratio=std::max(max_ratio,(std::abs(lift->GetTotalLambdaMotor())+std::abs(lift->GetFrictionPositionLambda())/c.mDeltaTime)/cap);
 if(released)lift->SetMaxFrictionForce(0);
 }
 } control;control.lift=obs.lift; f.world.AddStepListener(&control);
 WarehouseForklift::Command lower;lower.lift=-1;
 f.tick({},36,lower);const float lowered=obs.lift->GetCurrentPosition();
 std::cout<<"LOWERING q0="<<q0<<" lowered="<<lowered<<std::endl;
 require(lowered<q0-.02F,"powered lowering retains actual velocity-integrated motion");
 f.tick({},1);const auto neutral=f.truck->state();const float new_q=obs.lift->GetCurrentPosition();
 for(unsigned k=0;k<270;++k){f.tick();obs.joints(f.world);}const auto held=f.truck->state();
 const float held_q=obs.lift->GetCurrentPosition();
 BodyIDVector all_ids;f.world.GetBodies(all_ids);unsigned external_contacts=0;
 const auto &cfg=f.truck->config();
 for(auto part:{cfg.mast,cfg.carriage,cfg.fork_frame})for(auto id:all_ids){
 if(id==f.kit->body_id(cfg.chassis)||id==f.kit->body_id(cfg.mast)||id==f.kit->body_id(cfg.carriage)||id==f.kit->body_id(cfg.fork_frame))continue;
 if(f.world.WereBodiesInContact(f.kit->body_id(part),id))++external_contacts;
 }
 require(external_contacts==0 && obs.max_joint_anchor<.0005F && obs.max_fixed_angle<.0005F && obs.max_hinge_angle<.0005F,"suspended joint corrections stay below submillimeter/milliradian bounds with no external tool/mast contacts");
 require(std::abs(held_q-new_q)<.001F && held_q<q0-.02F,"neutral captures newly lowered current-substep coordinate without returning to old height");
 require(neutral.battery_j==held.battery_j && neutral.hydraulic_positive_work_j==held.hydraulic_positive_work_j,"neutral finite correction supplies no hydraulic work or battery debit");
 std::cout<<"LOWER_NEUTRAL q0="<<q0<<" lowered="<<lowered<<" hold_start="<<new_q<<" held="<<held_q<<" battery_delta="<<held.battery_j-neutral.battery_j<<" positive_work_delta="<<held.hydraulic_positive_work_j-neutral.hydraulic_positive_work_j<<" max_joint_anchor="<<obs.max_joint_anchor<<" fixed_all_axes="<<obs.max_fixed_angle<<" hinge_axis="<<obs.max_hinge_angle<<" external_contacts="<<external_contacts<<std::endl;
 // Actual finite external force at the carriage COM, no pose, velocity, gravity or mass writes.
 const float overload_start=obs.lift->GetCurrentPosition();
 for(unsigned i=0;i<3;++i){const auto*a=obs.lift->GetBody1(); const Vec3 axis=a->GetRotation()*obs.lift->GetConstraintToBody1Matrix().GetAxisX();f.world.GetBodyInterface().AddForce(obs.lift->GetBody2()->GetID(),-50000.F*axis);f.tick();}
 const float overloaded=obs.lift->GetCurrentPosition();
 std::cout<<"OVERLOAD before="<<overload_start<<" after="<<overloaded<<" saturated="<<control.saturated<<" zero_budget="<<control.zero_budget<<" combined_ratio="<<control.max_ratio<<std::endl;
 require(overloaded<overload_start-.001F && control.saturated>0 && control.zero_budget==control.saturated && control.max_ratio<=1.00001F,"actual axial overload reaches finite physical cap, leaves zero position budget and slips");
 f.tick({},30);control.released=true;const float release_start=obs.lift->GetCurrentPosition();f.tick({},9);
 std::cout<<"RELEASE before="<<release_start<<" after="<<obs.lift->GetCurrentPosition()<<" budget="<<obs.lift->GetFrictionPositionBudget()<<" lambda="<<obs.lift->GetFrictionPositionLambda()<<std::endl;
 require(obs.lift->GetCurrentPosition()<release_start-.01F && obs.lift->GetFrictionPositionBudget()==0 && obs.lift->GetFrictionPositionLambda()==0,"zero-friction release genuinely falls with no correction budget");
 f.world.RemoveStepListener(&control);

f.world.RemoveStepListener(&obs);
 }
 UnregisterTypes();delete Factory::sInstance;Factory::sInstance=nullptr;
}
