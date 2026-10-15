#include "sim/warehouse_forklift.hpp"
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
#include <Jolt/Physics/StateRecorderImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace scraperx::sim {
namespace {
using namespace JPH;
float demand(float x){return std::isfinite(x)?std::clamp(x,-1.F,1.F):0;}
float approach(float value,float target,float change){return std::clamp(target,value-change,value+change);}
void positive(double value,const char*name){if(!std::isfinite(value)||value<=0)throw std::invalid_argument(name);}
void unit(Vec3 value,const char*name){if(value.IsNaN()||!value.IsNormalized())throw std::invalid_argument(name);}
}
struct WarehouseForklift::Impl {
    void collision_step(float dt);
    struct StepListener final:PhysicsStepListener {
        Impl &owner;explicit StepListener(Impl&o):owner(o){}
        void OnStep(const PhysicsStepListenerContext&context)override{
            owner.collision_step(context.mDeltaTime);
            // Dispatch through the public base interface; VehicleConstraint
            // keeps its override private in the pinned Jolt API.
            static_cast<PhysicsStepListener*>(owner.vehicle.GetPtr())->OnStep(context);
        }
    } listener;
    PhysicsSystem &world;kit::Kit &kit;Config config;State state;Command command;
    Ref<VehicleConstraint> vehicle;Ref<HingeConstraint> tilt;Ref<SliderConstraint> lift;
    Ref<FixedConstraint> coupling;IgnoreMultipleBodiesFilter wheel_filter;
    FixedConstraintSettings coupling_settings;
    bool listener_registered=false,pending=false,lift_powered=false,tilt_powered=false;
    float step_dt=0,wheel_dt=0,last_lift=0,last_tilt=0,last_engine_omega=0;
    double hydraulic_reserve=0;
    Impl(PhysicsSystem&w,kit::Kit&k,const Config&c):listener(*this),world(w),kit(k),config(c){}
    ~Impl(){if(listener_registered)world.RemoveStepListener(&listener);if(coupling)world.RemoveConstraint(coupling);if(lift)world.RemoveConstraint(lift);if(tilt)world.RemoveConstraint(tilt);if(vehicle)world.RemoveConstraint(vehicle);}
    Body &body(kit::BodyIndex index)const{return *world.GetBodyLockInterfaceNoLock().TryGetBody(kit.body_id(index));}
    WheeledVehicleController &controller()const{return *static_cast<WheeledVehicleController*>(vehicle->GetController());}
    Vec3 lift_world_axis()const{return body(config.mast).GetRotation()*lift_axis_local;}
    Vec3 tilt_world_axis()const{return body(config.chassis).GetRotation()*tilt_axis_local;}
    Vec3 lift_axis_local=Vec3::sAxisY(),tilt_axis_local=Vec3::sAxisX();
    void account(){
        if(!pending)return;
        const float position=lift->GetCurrentPosition(),angle=tilt->GetCurrentAngle();
        state.lift_force_n=lift->GetTotalLambdaMotor()/step_dt;
        state.tilt_torque_nm=tilt->GetTotalLambdaMotor()/step_dt;
        const double linear_work=double(state.lift_force_n)*(position-last_lift);
        const double angular_work=double(state.tilt_torque_nm)*(angle-last_tilt);
        const double positive_work=(lift_powered?std::max(0.,linear_work):0)+(tilt_powered?std::max(0.,angular_work):0);
        state.hydraulic_positive_work_j+=positive_work;
        state.hydraulic_dissipation_j+=std::max(0.,-linear_work)+std::max(0.,-angular_work);
        // Normally the pre-solve power reservation covers this receipt. An
        // external impact can accelerate a joint inside a solve; expose and
        // debit any measured excess, never disguise it as free actuator work.
        const double excess=std::max(0.,positive_work/config.efficiency-hydraulic_reserve);
        state.reservation_excess_j+=excess;
        state.battery_j=std::max(0.,state.battery_j-excess);
        state.reserved_debit_j+=excess;
        if(excess>0&&state.battery_j==0)state.energy_cutoff=true;
        state.lift_position_m=position;state.tilt_angle_rad=angle;pending=false;
    }
    bool aligned()const{
        const Body&a=body(config.carriage),&b=body(config.fork_frame);
        const RVec3 p=a.GetWorldTransform()*config.carriage_mount,q=b.GetWorldTransform()*config.tool_mount;
        const Quat ra=a.GetRotation()*config.carriage_mount_rotation,rb=b.GetRotation()*config.tool_mount_rotation;
        const float cosine=std::min(1.F,std::abs(ra.Dot(rb)));
        return (p-q).Length()<=config.capture_distance_m&&2*std::acos(cosine)<=config.capture_angle_rad
            &&(a.GetPointVelocity(p)-b.GetPointVelocity(q)).Length()<=config.capture_speed_mps
            &&(a.GetAngularVelocity()-b.GetAngularVelocity()).Length()<=config.capture_angular_speed_rad_s;
    }
    bool supported_tool()const{
        const Body&tool=body(config.fork_frame);
        if(tool.GetLinearVelocity().Length()>config.capture_speed_mps||tool.GetAngularVelocity().Length()>config.capture_angular_speed_rad_s)return false;
        // A broad geometric candidate is insufficient. Require an actual
        // persisted contact against a static body, plus that body's material
        // immediately below the authored lower fork solids.
        BodyIDVector ids;world.GetBodies(ids);
        for(const auto&id:ids){const Body*b=world.GetBodyLockInterfaceNoLock().TryGetBody(id);
            if(!b||!b->IsStatic()||!world.WereBodiesInContact(tool.GetID(),id))continue;
            for(const auto&part:kit.body_parts(config.fork_frame)){
                const RVec3 bottom=tool.GetWorldTransform()*(part.offset+part.rotation*Vec3(0,-part.half.GetY(),0));
                class OnlyBody final:public BodyFilter{public:BodyID id;explicit OnlyBody(BodyID i):id(i){}bool ShouldCollide(const BodyID&i)const override{return i==id;}}filter(id);
                RayCastResult hit;
                if(world.GetNarrowPhaseQuery().CastRay(RRayCast(bottom+Vec3(0,.015F,0),Vec3(0,-.04F,0)),hit,{},{},filter)){
                    const RVec3 p=bottom+Vec3(0,.015F-.04F*hit.mFraction,0);
                    if(.04F*hit.mFraction-.015F<=.003F&&b->GetWorldSpaceSurfaceNormal(hit.mSubShapeID2,p).GetY()>.7F)return true;
                }
            }
        }
        return false;
    }
    void refresh_wheel_filter(){
        wheel_filter.Clear();for(auto b:{config.chassis,config.mast,config.carriage})wheel_filter.IgnoreBody(kit.body_id(b));
        if(state.coupled)wheel_filter.IgnoreBody(kit.body_id(config.fork_frame));
    }
    bool mating_clear()const{
        const RVec3 a=body(config.carriage).GetWorldTransform()*config.carriage_mount;
        const RVec3 b=body(config.fork_frame).GetWorldTransform()*config.tool_mount;
        class HardwareFilter final:public BodyFilter {
        public:std::array<BodyID,4> ids;bool ShouldCollide(const BodyID&id)const override{for(auto own:ids)if(id==own)return false;return true;}
        }filter;
        filter.ids={kit.body_id(config.chassis),kit.body_id(config.mast),kit.body_id(config.carriage),kit.body_id(config.fork_frame)};
        RayCastResult hit;return !world.GetNarrowPhaseQuery().CastRay(RRayCast(a,Vec3(b-a)),hit,{},{},filter);
    }
    void create_coupling(bool current){
        if(current){
            const Body&a=body(config.carriage),&b=body(config.fork_frame);
            coupling_settings.mSpace=EConstraintSpace::LocalToBodyCOM;
            coupling_settings.mAutoDetectPoint=false;
            const RVec3 point=a.GetWorldTransform()*config.carriage_mount;
            coupling_settings.mPoint1=a.GetInverseCenterOfMassTransform()*point;
            coupling_settings.mPoint2=b.GetInverseCenterOfMassTransform()*point;
            coupling_settings.mAxisX1=Vec3::sAxisX();coupling_settings.mAxisY1=Vec3::sAxisY();
            const Quat q=b.GetRotation().Conjugated()*a.GetRotation();
            coupling_settings.mAxisX2=q*Vec3::sAxisX();coupling_settings.mAxisY2=q*Vec3::sAxisY();
        }
        coupling_settings.mNumVelocityStepsOverride=40;coupling_settings.mNumPositionStepsOverride=8;
        coupling=static_cast<FixedConstraint*>(coupling_settings.Create(body(config.carriage),body(config.fork_frame)));
        kit.set_pair_collision(config.carriage,config.fork_frame,false);world.AddConstraint(coupling);
        state.coupled=true;refresh_wheel_filter();
    }
    void remove_coupling(){if(coupling){world.RemoveConstraint(coupling);coupling=nullptr;}kit.set_pair_collision(config.carriage,config.fork_frame,true);state.coupled=false;refresh_wheel_filter();}
};
WarehouseForklift::WarehouseForklift(PhysicsSystem&w,kit::Kit&k,const Config&c):impl_(std::make_unique<Impl>(w,k,c)){
    auto&m=*impl_;
    const std::array<kit::BodyIndex,4> bodies{c.chassis,c.mast,c.carriage,c.fork_frame};
    for(std::size_t i=0;i<bodies.size();++i){if(!k.body_dynamic(bodies[i])||!k.body_enabled(bodies[i]))throw std::invalid_argument("forklift requires enabled dynamic Kit bodies");for(std::size_t j=0;j<i;++j)if(bodies[i]==bodies[j])throw std::invalid_argument("forklift bodies must be distinct");m.wheel_filter.IgnoreBody(k.body_id(bodies[i]));}
    for(float value:{c.wheel_radius_m,c.wheel_width_m,c.suspension_max_m,c.suspension_frequency_hz,c.suspension_damping,c.drive_torque_nm,c.engine_max_rpm,c.reduction,c.wheel_brake_torque_nm,c.steering_angle_rad,c.steering_rate_rad_s,c.lift_stroke_m,c.lift_speed_mps,c.lift_acceleration_mps2,c.lift_force_n,c.lift_brake_force_n,c.tilt_speed_rad_s,c.tilt_acceleration_rad_s2,c.tilt_torque_nm,c.tilt_brake_torque_nm,c.capture_distance_m,c.capture_angle_rad,c.capture_speed_mps,c.capture_angular_speed_rad_s})positive(value,"forklift invalid rating");
    positive(c.battery_capacity_j,"forklift battery");positive(c.hydraulic_power_w,"forklift hydraulic power");positive(c.efficiency,"forklift efficiency");
    if(c.efficiency>1||!std::isfinite(c.suspension_min_m)||c.suspension_min_m<0||c.suspension_min_m>=c.suspension_max_m||!std::isfinite(c.tilt_min_rad)||!std::isfinite(c.tilt_max_rad)||c.tilt_min_rad>0||c.tilt_max_rad<0||c.tilt_min_rad>=c.tilt_max_rad)throw std::invalid_argument("forklift invalid limits");
    unit(c.tilt_axis,"forklift tilt axis");unit(c.tilt_normal,"forklift tilt normal");unit(c.lift_axis,"forklift lift axis");
    if(std::abs(c.tilt_axis.Dot(c.tilt_normal))>1e-4F)throw std::invalid_argument("forklift hinge frame not orthogonal");
    if(c.tilt_pivot.IsNaN()||c.carriage_mount.IsNaN()||c.tool_mount.IsNaN()||!c.carriage_mount_rotation.IsNormalized()||!c.tool_mount_rotation.IsNormalized())throw std::invalid_argument("forklift invalid authored frame");
    for(auto p:c.wheel_positions)if(p.IsNaN()||!std::isfinite(p.LengthSq()))throw std::invalid_argument("forklift invalid wheel position");
    if(c.initially_coupled&&!m.aligned())throw std::invalid_argument("forklift initial tool hardware not aligned");
    m.tilt_axis_local=m.body(c.chassis).GetRotation().Conjugated()*c.tilt_axis;
    m.lift_axis_local=m.body(c.mast).GetRotation().Conjugated()*c.lift_axis;
    VehicleConstraintSettings vs;vs.mMaxPitchRollAngle=JPH_PI;vs.mNumVelocityStepsOverride=40;vs.mNumPositionStepsOverride=8;
    for(std::size_t i=0;i<3;++i){auto*wheel=new WheelSettingsWV;
        wheel->mPosition=c.wheel_positions[i];wheel->mRadius=c.wheel_radius_m;wheel->mWidth=c.wheel_width_m;
        wheel->mSuspensionMinLength=c.suspension_min_m;wheel->mSuspensionMaxLength=c.suspension_max_m;
        wheel->mSuspensionSpring=SpringSettings(ESpringMode::FrequencyAndDamping,c.suspension_frequency_hz,c.suspension_damping);
        wheel->mMaxSteerAngle=i==2?c.steering_angle_rad:0;wheel->mMaxBrakeTorque=c.wheel_brake_torque_nm;wheel->mMaxHandBrakeTorque=0;
        wheel->mInertia=.5F*35*c.wheel_radius_m*c.wheel_radius_m;wheel->mEnableSuspensionForcePoint=false;
        vs.mWheels.push_back(wheel);
    }
    auto*controller=new WheeledVehicleControllerSettings;
    controller->mEngine.mMinRPM=0;controller->mEngine.mMaxRPM=c.engine_max_rpm;controller->mEngine.mMaxTorque=c.drive_torque_nm;
    controller->mEngine.mNormalizedTorque.Clear();controller->mEngine.mNormalizedTorque.AddPoint(0,1);controller->mEngine.mNormalizedTorque.AddPoint(1,0);
    controller->mTransmission.mMode=ETransmissionMode::Manual;controller->mTransmission.mGearRatios={1};controller->mTransmission.mReverseGearRatios={-1};controller->mTransmission.mClutchStrength=30;
    VehicleDifferentialSettings differential;differential.mLeftWheel=0;differential.mRightWheel=1;differential.mDifferentialRatio=c.reduction;controller->mDifferentials.push_back(differential);vs.mController=controller;
    m.vehicle=new VehicleConstraint(m.body(c.chassis),vs);
    Ref<VehicleCollisionTesterCastCylinder> tester=new VehicleCollisionTesterCastCylinder(c.wheel_layer);tester->SetBodyFilter(&m.wheel_filter);m.vehicle->SetVehicleCollisionTester(tester);
    m.vehicle->SetPostStepCallback([owner=&m](VehicleConstraint&,const PhysicsStepListenerContext&context){
        owner->wheel_dt=context.mDeltaTime;
        const auto&e=owner->controller().GetEngine();const float omega=e.GetAngularVelocity();
        // Estimate from the engine's actual bounded torque curve and sampled
        // rotor speed; separate from conservative electrical reservation.
        owner->state.drivetrain_work_estimate_j+=std::max(0.F,e.GetTorque(std::abs(owner->state.applied_drive))*.5F*(owner->last_engine_omega+omega))*context.mDeltaTime;
        owner->last_engine_omega=omega;
    });
    w.AddConstraint(m.vehicle);w.AddStepListener(&m.listener);m.listener_registered=true;
    HingeConstraintSettings hs;hs.mPoint1=hs.mPoint2=c.tilt_pivot;hs.mHingeAxis1=hs.mHingeAxis2=c.tilt_axis;hs.mNormalAxis1=hs.mNormalAxis2=c.tilt_normal;hs.mLimitsMin=c.tilt_min_rad;hs.mLimitsMax=c.tilt_max_rad;hs.mMaxFrictionTorque=c.tilt_brake_torque_nm;hs.mNumVelocityStepsOverride=40;hs.mNumPositionStepsOverride=8;
    m.tilt=static_cast<HingeConstraint*>(hs.Create(m.body(c.chassis),m.body(c.mast)));w.AddConstraint(m.tilt);k.disable_collision(c.chassis,c.mast);
    SliderConstraintSettings ss;ss.mAutoDetectPoint=true;ss.SetSliderAxis(c.lift_axis);ss.mLimitsMin=0;ss.mLimitsMax=c.lift_stroke_m;ss.mMaxFrictionForce=c.lift_brake_force_n;ss.mNumVelocityStepsOverride=40;ss.mNumPositionStepsOverride=8;
    m.lift=static_cast<SliderConstraint*>(ss.Create(m.body(c.mast),m.body(c.carriage)));w.AddConstraint(m.lift);k.disable_collision(c.mast,c.carriage);
    if(c.initially_coupled){m.create_coupling(true);m.state.coupling_generation=1;}
    m.state.battery_j=c.battery_capacity_j;m.refresh_wheel_filter();
}
WarehouseForklift::~WarehouseForklift()=default;
void WarehouseForklift::pre_step(const Command&c){auto&m=*impl_;m.command={demand(c.drive),demand(c.steer),demand(c.lift),demand(c.tilt),std::isfinite(c.brake)?std::clamp(c.brake,0.F,1.F):1};
    if(m.command.drive!=0||m.command.lift!=0||m.command.tilt!=0||m.command.steer!=0){for(auto b:{m.config.chassis,m.config.mast,m.config.carriage,m.config.fork_frame})m.world.GetBodyInterface().ActivateBody(m.kit.body_id(b));}
}
void WarehouseForklift::Impl::collision_step(float dt){
    auto&m=*this;if(!std::isfinite(dt)||dt<=0)throw std::invalid_argument("forklift collision dt");m.account();const auto&c=m.config;
    bool enabled=m.state.battery_j>0&&!m.state.energy_cutoff;
    const bool wants_lift=m.command.lift!=0,wants_tilt=m.command.tilt!=0,wants_drive=m.command.drive!=0;
    const double drive_reserve=wants_drive?c.drive_torque_nm*(c.engine_max_rpm*2*JPH_PI/60)*dt/c.efficiency:0;
    const double hydraulic_reserve=(wants_lift||wants_tilt)?c.hydraulic_power_w*dt:0;
    const double steering_reserve=std::abs(m.command.steer*c.steering_angle_rad-m.state.steering_rad)>1e-5F?80*dt:0;
    const double reserve=drive_reserve+hydraulic_reserve+steering_reserve;
    if(enabled&&reserve>m.state.battery_j){enabled=false;m.state.energy_cutoff=true;}
    if(m.state.battery_j<=0)m.state.energy_cutoff=true;
    if(enabled){m.state.battery_j-=reserve;m.state.reserved_debit_j+=reserve;}
    m.hydraulic_reserve=enabled?hydraulic_reserve:0;
    m.state.steering_rad=enabled?approach(m.state.steering_rad,m.command.steer*c.steering_angle_rad,c.steering_rate_rad_s*dt):m.state.steering_rad;
    // Accelerator release selects neutral torque, not a service-brake pedal.
    // The host's default unauthorized command still requests full braking.
    m.state.applied_drive=enabled?m.command.drive:0;m.state.applied_brake=std::max(m.command.brake,!enabled?1.F:0.F);
    auto&driver=m.controller();driver.GetTransmission().Set(m.state.applied_drive==0?0:(m.state.applied_drive>0?1:-1),m.state.applied_drive==0?0:1);
    // Rear steering reverses the wheel's ordinary front-steer sign.
    driver.SetDriverInput(std::abs(m.state.applied_drive),-m.state.steering_rad/c.steering_angle_rad,m.state.applied_brake,0);
    m.lift_powered=enabled&&wants_lift;m.tilt_powered=enabled&&wants_tilt;
    m.state.lift_target_mps=m.lift_powered?approach(m.state.lift_target_mps,m.command.lift*c.lift_speed_mps,c.lift_acceleration_mps2*dt):0;
    m.state.tilt_target_rad_s=m.tilt_powered?approach(m.state.tilt_target_rad_s,m.command.tilt*c.tilt_speed_rad_s,c.tilt_acceleration_rad_s2*dt):0;
    const float lift_speed=std::abs((m.body(c.carriage).GetLinearVelocity()-m.body(c.mast).GetPointVelocity(m.body(c.carriage).GetCenterOfMassPosition())).Dot(m.lift_world_axis()));
    const float tilt_speed=std::abs((m.body(c.mast).GetAngularVelocity()-m.body(c.chassis).GetAngularVelocity()).Dot(m.tilt_world_axis()));
    const double share=c.hydraulic_power_w*c.efficiency/std::max(1,int(m.lift_powered)+int(m.tilt_powered));
    const float force=std::min(double(c.lift_force_n),share/std::max({double(lift_speed+c.lift_acceleration_mps2*dt),double(std::abs(m.state.lift_target_mps)),.01}));
    const float torque=std::min(double(c.tilt_torque_nm),share/std::max({double(tilt_speed+c.tilt_acceleration_rad_s2*dt),double(std::abs(m.state.tilt_target_rad_s)),.01}));
    m.lift->GetMotorSettings().SetForceLimit(force);m.lift->SetMaxFrictionForce(m.lift_powered?0:c.lift_brake_force_n);m.lift->SetMotorState(m.lift_powered?EMotorState::Velocity:EMotorState::Off);m.lift->SetTargetVelocity(m.state.lift_target_mps);
    m.tilt->GetMotorSettings().SetTorqueLimit(torque);m.tilt->SetMaxFrictionTorque(m.tilt_powered?0:c.tilt_brake_torque_nm);m.tilt->SetMotorState(m.tilt_powered?EMotorState::Velocity:EMotorState::Off);m.tilt->SetTargetAngularVelocity(m.state.tilt_target_rad_s);
    m.last_lift=m.lift->GetCurrentPosition();m.last_tilt=m.tilt->GetCurrentAngle();m.step_dt=dt;m.pending=true;
}
void WarehouseForklift::post_step(){impl_->account();}
WarehouseForklift::State WarehouseForklift::state()const{return impl_->state;}
const WarehouseForklift::Config&WarehouseForklift::config()const{return impl_->config;}
std::array<WarehouseForklift::WheelReceipt,3> WarehouseForklift::wheels()const{
    const auto&m=*impl_;std::array<WheelReceipt,3> result{};
    for(std::size_t i=0;i<3;++i){const auto*w=m.vehicle->GetWheel(i);auto&r=result[i];const auto pose=m.vehicle->GetWheelWorldTransform(i,Vec3::sAxisY(),Vec3::sAxisZ());r.position=pose.GetTranslation();r.rotation=pose.GetRotation().GetQuaternion();r.radius_m=m.config.wheel_radius_m;r.width_m=m.config.wheel_width_m;r.angular_velocity_rad_s=w->GetAngularVelocity();r.steer_rad=w->GetSteerAngle();r.suspension_length_m=w->GetSuspensionLength();r.contact=w->HasContact();r.contact_body=w->GetContactBodyID();if(r.contact){r.contact_position=w->GetContactPosition();r.contact_normal=w->GetContactNormal();if(m.wheel_dt>0){r.normal_force_n=w->GetSuspensionLambda()/m.wheel_dt;r.longitudinal_force_n=w->GetLongitudinalLambda()/m.wheel_dt;r.lateral_force_n=w->GetLateralLambda()/m.wheel_dt;}}}
    return result;
}
void WarehouseForklift::capture(Checkpoint&out)const{
    const auto&m=*impl_;if(m.pending)throw std::logic_error("forklift capture requires completed solve");out.state=m.state;
    StateRecorderImpl stream;m.vehicle->SaveState(stream);m.lift->SaveState(stream);m.tilt->SaveState(stream);if(m.coupling)m.coupling->SaveState(stream);
    // Preserve the exact latch frame, including the carriage-side point.
    stream.Write(m.coupling_settings.mPoint1);stream.Write(m.coupling_settings.mPoint2);stream.Write(m.coupling_settings.mAxisX2);stream.Write(m.coupling_settings.mAxisY2);stream.Write(m.wheel_dt);stream.Write(m.last_engine_omega);
    out.native_state=stream.GetData();
}
void WarehouseForklift::restore(const Checkpoint&cp){
    auto&m=*impl_;if(cp.native_state.empty()||!std::isfinite(cp.state.battery_j))throw std::invalid_argument("forklift invalid checkpoint");
    // Configure the joint topology before consuming its native replay state.
    if(cp.state.coupled&&!m.coupling)m.create_coupling(true);else if(!cp.state.coupled&&m.coupling)m.remove_coupling();
    StateRecorderImpl stream;stream.WriteBytes(cp.native_state.data(),cp.native_state.size());stream.Rewind();m.vehicle->RestoreState(stream);m.lift->RestoreState(stream);m.tilt->RestoreState(stream);
    std::string coupling_state;
    if(m.coupling){StateRecorderImpl temp;m.coupling->SaveState(temp);coupling_state.resize(temp.GetDataSize());stream.ReadBytes(coupling_state.data(),coupling_state.size());}
    stream.Read(m.coupling_settings.mPoint1);stream.Read(m.coupling_settings.mPoint2);stream.Read(m.coupling_settings.mAxisX2);stream.Read(m.coupling_settings.mAxisY2);stream.Read(m.wheel_dt);stream.Read(m.last_engine_omega);
    if(stream.IsFailed())throw std::invalid_argument("forklift truncated native checkpoint");
    if(m.coupling){m.remove_coupling();m.create_coupling(false);StateRecorderImpl temp;temp.WriteBytes(coupling_state.data(),coupling_state.size());temp.Rewind();m.coupling->RestoreState(temp);}
    m.state=cp.state;m.state.battery_j=std::clamp(cp.state.battery_j,0.,m.config.battery_capacity_j);m.command={};m.pending=false;m.lift_powered=m.tilt_powered=false;
    // Demand is external input. Native spin/engine/transmission and warm starts
    // remain restored; the next collision_step applies neutral finite brakes.
    m.state.applied_drive=0;m.state.applied_brake=1;
    m.vehicle->GetWheel(0)->SetSteerAngle(0);m.vehicle->GetWheel(1)->SetSteerAngle(0);m.vehicle->GetWheel(2)->SetSteerAngle(m.state.steering_rad);
}
bool WarehouseForklift::try_couple(){auto&m=*impl_;if(m.pending||m.coupling||!m.aligned()||!m.supported_tool()||!m.mating_clear())return false;m.create_coupling(true);++m.state.coupling_generation;return true;}
bool WarehouseForklift::try_detach(){auto&m=*impl_;if(m.pending||!m.coupling||!m.supported_tool())return false;const double own_weight=m.kit.body_mass(m.config.fork_frame)*m.world.GetGravity().Length();if(m.wheel_dt>0&&m.coupling->GetTotalLambdaPosition().Length()/m.wheel_dt>own_weight*1.15+100)return false;m.remove_coupling();++m.state.coupling_generation;return true;}
} // namespace scraperx::sim
