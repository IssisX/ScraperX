// Private two-case loaded-contact discriminator. No production gameplay writes.
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
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
using namespace JPH;
constexpr double H=1./90., CONTROL_CAP=250., WORK_CAP=80.;
enum Tag:uint64 { RACK=1,DRUM,TONGUE,WALL_LEFT,WALL_RIGHT,GUIDE,DECK,PLAYER,RECEIVER,FLOOR };
struct Broad:BroadPhaseLayerInterface { uint GetNumBroadPhaseLayers()const override{return 2;} BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l)const override{return BroadPhaseLayer(l);} const char*GetBroadPhaseLayerName(BroadPhaseLayer l)const override{return l.GetValue()?"MOVING":"STATIC";} };
struct BP:ObjectVsBroadPhaseLayerFilter {bool ShouldCollide(ObjectLayer a,BroadPhaseLayer b)const override{return a==1||b.GetValue()==1;}};
struct Pair:ObjectLayerPairFilter {bool ShouldCollide(ObjectLayer a,ObjectLayer b)const override{return a==1||b==1;}};
struct Contacts:ContactListener {
 std::array<std::atomic<unsigned>,10> counts{};
 std::atomic<bool> foot{false}; BodyID player_id,deck_id;
 std::atomic<double> max_penetration{0};
 int type(uint64 a,uint64 b){if(a>b)std::swap(a,b);
  if(a==RACK&&b==DRUM)return 0; if(a==DRUM&&b==TONGUE)return 1;
  if(a==DRUM&&b==WALL_LEFT)return 2; if(a==DRUM&&b==WALL_RIGHT)return 3;
  if(a==TONGUE&&b==GUIDE)return 4; if(a==TONGUE&&(b==WALL_LEFT||b==WALL_RIGHT))return 5;
  if(a==DECK&&b==PLAYER)return 6; if(a==DRUM&&b==RECEIVER)return 7;
  if(a==DRUM&&b==FLOOR)return 8;return 9;
 }
 void record(const Body&a,const Body&b,const ContactManifold&m){
  ++counts[type(a.GetUserData(),b.GetUserData())];
  double penetration=m.mPenetrationDepth,old=max_penetration.load();while(old<penetration&&!max_penetration.compare_exchange_weak(old,penetration)){}
  if((a.GetID()==player_id&&b.GetID()==deck_id)||(a.GetID()==deck_id&&b.GetID()==player_id)) {
   Vec3 normal=a.GetID()==player_id?-m.mWorldSpaceNormal:m.mWorldSpaceNormal;
   foot=normal.GetY()>.7F;
  }
 }
 void OnContactAdded(const Body&a,const Body&b,const ContactManifold&m,ContactSettings&)override{record(a,b,m);}
 void OnContactPersisted(const Body&a,const Body&b,const ContactManifold&m,ContactSettings&)override{record(a,b,m);}
 void OnContactRemoved(const SubShapeIDPair&p)override{if((p.GetBody1ID()==player_id&&p.GetBody2ID()==deck_id)||(p.GetBody2ID()==player_id&&p.GetBody1ID()==deck_id))foot=false;}
};
double mass(const Body*b){return 1./b->GetMotionProperties()->GetInverseMass();}
double kinetic(const Body*b){auto*m=b->GetMotionProperties();Vec3 w=m->GetInertiaRotation().Conjugated()*(b->GetRotation().Conjugated()*b->GetAngularVelocity());Vec3 inv=m->GetInverseInertiaDiagonal();double e=.5*mass(b)*b->GetLinearVelocity().LengthSq();for(int i=0;i<3;++i)if(inv[i]>0)e+=.5*double(w[i])*w[i]/inv[i];return e;}
RVec3 point(const Body*b,Vec3 l){return b->GetCenterOfMassTransform()*l;}
struct Observer:PhysicsStepListener {
 Body *drum,*tongue,*player;SliderConstraint*guide;SixDOFConstraint*grip=nullptr;Contacts*contacts;
 Vec3 downhill,normal,grip_local,player_local;RVec3 origin,initial_tongue;
 double dt=H/4,time=0,max_grip=0,max_guide=0,max_guide_torque=0,max_guide_friction=0,max_error=0,max_yaw=0,max_drum_speed=0;
 double max_transverse_drift=0,max_grip_error=0,max_grip_storage=0,max_travel=0,min_travel=0,exit_time=-1,exit_speed=0,exit_yaw=0;
 double current_control=0;unsigned unsupported_force_samples=0,callbacks=0;bool finite=true;
 double drum_s(){return Vec3(drum->GetCenterOfMassPosition()-origin).Dot(downhill);}
 double yaw(){Vec3 axis=drum->GetRotation()*Vec3::sAxisY();return std::atan2(axis.Dot(downhill),axis.GetX());}
 void observe(){
  for(Body*b:{drum,tongue,player}){auto p=b->GetCenterOfMassPosition();auto v=b->GetLinearVelocity();auto w=b->GetAngularVelocity();for(int i=0;i<3;++i)finite&=std::isfinite(p[i])&&std::isfinite(v[i])&&std::isfinite(w[i]);}
  double q=tongue->GetPosition().GetX()-.35;
  max_travel=std::max(max_travel,q);min_travel=std::min(min_travel,q);
  Vec3 error=Vec3(tongue->GetCenterOfMassPosition()-initial_tongue);error.SetX(0);max_error=std::max(max_error,double(error.Length()));
  auto l=guide->GetTotalLambdaPosition();max_guide=std::max(max_guide,std::hypot(l[0],l[1])/dt);
  max_guide_torque=std::max(max_guide_torque,double(guide->GetTotalLambdaRotation().Length())/dt);
  max_guide_friction=std::max(max_guide_friction,std::abs(guide->GetTotalLambdaMotor())/dt);
  if(grip){max_grip=std::max(max_grip,double(grip->GetTotalLambdaMotorTranslation().Length())/dt);double e=Vec3(point(tongue,grip_local)-point(player,player_local)).Length();max_grip_error=std::max(max_grip_error,e);max_grip_storage=std::max(max_grip_storage,.5*5000*e*e);}
  max_yaw=std::max(max_yaw,std::abs(yaw()));max_drum_speed=std::max(max_drum_speed,double(drum->GetLinearVelocity().Length()));
  max_transverse_drift=std::max(max_transverse_drift,std::abs(double(drum->GetCenterOfMassPosition().GetX())));
  if(exit_time<0&&drum_s()>1.2){exit_time=time;exit_speed=drum->GetLinearVelocity().Length();exit_yaw=yaw();}
  if(current_control!=0&&!contacts->foot.load())++unsupported_force_samples;
 }
 void OnStep(const PhysicsStepListenerContext&c)override{dt=c.mDeltaTime;time+=dt;++callbacks;observe();}
};
int main(int argc,char**argv){const bool withdraw=argc>1&&std::string(argv[1])=="withdraw";
 RegisterDefaultAllocator();Factory::sInstance=new Factory;RegisterTypes();int code=0;
 {
 Broad broad;BP bp;Pair pair;TempAllocatorImpl temp(32*1024*1024);JobSystemThreadPool jobs(cMaxPhysicsJobs,cMaxPhysicsBarriers,1);PhysicsSystem sys;sys.Init(128,0,1024,2048,broad,bp,pair);sys.SetGravity(Vec3(0,-9.81F,0));auto ps=sys.GetPhysicsSettings();ps.mNumVelocitySteps=10;ps.mNumPositionSteps=2;ps.mPenetrationSlop=.02;sys.SetPhysicsSettings(ps);auto&bi=sys.GetBodyInterface();
 Contacts contacts;sys.SetContactListener(&contacts);std::vector<Body*>bodies,dynamics;
 double slope=std::asin(.18/2.);Quat rack_q=Quat::sRotation(Vec3::sAxisX(),float(slope));Vec3 down=rack_q*Vec3::sAxisZ(),normal=rack_q*Vec3::sAxisY();RVec3 origin(0,1.5,0);
 auto world=[&](double x,double n,double s){return origin+RVec3(rack_q*Vec3(x,n,s));};
 auto add=[&](RefConst<Shape>shape,RVec3 p,Quat q,double m,uint64 tag,double friction){BodyCreationSettings c(shape,p,q,m?EMotionType::Dynamic:EMotionType::Static,m?1:0);c.mUserData=tag;c.mFriction=friction;c.mRestitution=0;if(m){c.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;c.mMassPropertiesOverride.mMass=m;c.mAllowSleeping=false;c.mLinearDamping=0;c.mAngularDamping=0;c.mMotionQuality=EMotionQuality::LinearCast;}Body*b=bi.CreateBody(c);bi.AddBody(b->GetID(),EActivation::Activate);bodies.push_back(b);if(m)dynamics.push_back(b);return b;};
 Body*rack=add(new BoxShape(Vec3(.75,.08,1.),.008F),world(0,-.08,0),rack_q,0,RACK,.9);
 add(new BoxShape(Vec3(.06,.725,1.),.008F),world(-.81,.725,0),rack_q,0,WALL_LEFT,.9);
 add(new BoxShape(Vec3(.06,.725,1.),.008F),world(.81,.725,0),rack_q,0,WALL_RIGHT,.9);
 for(double z:{-.075,.075})add(new BoxShape(Vec3(1.5,.02,.03),.005F),world(0,1.438,.01+z),rack_q,0,GUIDE,0);
 StaticCompoundShapeSettings tongue_shape;
 for(int sign:{-1,1}){
  std::array<Vec3,8> verts;int i=0;
  for(float y:{-.7F,.7F})for(auto p:std::array<std::array<float,2>,4>{{{.93F,-.10F},{.75F,.08F},{.75F,.14F},{.93F,.14F}}})verts[i++]=Vec3(sign*p[0],y,p[1]);
  ConvexHullShapeSettings hull(verts.data(),int(verts.size()),.008F);auto result=hull.Create();if(result.HasError()){std::cerr<<result.GetError()<<'\n';return 2;}tongue_shape.AddShape(Vec3::sZero(),Quat::sIdentity(),result.Get());
 }
 tongue_shape.AddShape(Vec3(0,.76,.02),Quat::sIdentity(),new BoxShape(Vec3(.93,.06,.06),.008F));
 tongue_shape.AddShape(Vec3(-.87,.82,-.10),Quat::sIdentity(),new BoxShape(Vec3(.03,.10,.035),.005F));
 tongue_shape.AddShape(Vec3(-1.05,.92,-.10),Quat::sIdentity(),new BoxShape(Vec3(.27,.03,.035),.005F));
 auto compound=tongue_shape.Create();if(compound.HasError()){std::cerr<<compound.GetError()<<'\n';return 2;}
 Body*tongue=add(compound.Get(),world(.35,.76,.01),rack_q,60,TONGUE,.9);
 SliderConstraintSettings guide_config;guide_config.mPoint1=guide_config.mPoint2=tongue->GetCenterOfMassPosition();guide_config.mSliderAxis1=guide_config.mSliderAxis2=Vec3::sAxisX();guide_config.mNormalAxis1=guide_config.mNormalAxis2=normal;guide_config.mLimitsMin=-.4;guide_config.mLimitsMax=.4;guide_config.mMaxFrictionForce=35;guide_config.mNumVelocityStepsOverride=40;guide_config.mNumPositionStepsOverride=8;Ref<SliderConstraint>guide=static_cast<SliderConstraint*>(guide_config.Create(*rack,*tongue));sys.AddConstraint(guide);
 BodyCreationSettings drum_config(new CylinderShape(.55,.60,.01F),world(0,.605,-.65),rack_q*Quat::sRotation(Vec3::sAxisZ(),-JPH_PI*.5F),EMotionType::Dynamic,1);drum_config.mUserData=DRUM;drum_config.mOverrideMassProperties=EOverrideMassProperties::MassAndInertiaProvided;drum_config.mMassPropertiesOverride.mMass=360;drum_config.mMassPropertiesOverride.mInertia=Mat44::sScale(Vec3(68.7,64.8,68.7));drum_config.mFriction=.9;drum_config.mRestitution=0;drum_config.mAllowSleeping=false;drum_config.mLinearDamping=0;drum_config.mAngularDamping=0;drum_config.mMotionQuality=EMotionQuality::LinearCast;Body*drum=bi.CreateBody(drum_config);bi.AddBody(drum->GetID(),EActivation::Activate);bodies.push_back(drum);dynamics.push_back(drum);
 RVec3 handle_world=tongue->GetWorldTransform()*Vec3(-1.30,.92,-.10);RVec3 player_pos=handle_world-RVec3(.4,.65,.55);double deck_top=player_pos.GetY()-.90;
 Body*deck=add(new BoxShape(Vec3(.9,.10,1.),.01F),RVec3(-1.8,deck_top-.1,player_pos.GetZ()),Quat::sIdentity(),0,DECK,.6);contacts.deck_id=deck->GetID();
 BodyCreationSettings pc(new CapsuleShape(.55,.35),player_pos,Quat::sIdentity(),EMotionType::Dynamic,1);pc.mUserData=PLAYER;pc.mAllowedDOFs=EAllowedDOFs::TranslationX|EAllowedDOFs::TranslationY|EAllowedDOFs::TranslationZ;pc.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;pc.mMassPropertiesOverride.mMass=85;pc.mFriction=0;pc.mRestitution=0;pc.mAllowSleeping=false;pc.mLinearDamping=0;pc.mAngularDamping=0;pc.mMotionQuality=EMotionQuality::LinearCast;Body*player=bi.CreateBody(pc);bi.AddBody(player->GetID(),EActivation::Activate);bodies.push_back(player);dynamics.push_back(player);contacts.player_id=player->GetID();
 add(new BoxShape(Vec3(1.5,.10,2.5),.01F),RVec3(0,1.22,3.51),Quat::sIdentity(),0,RECEIVER,.9);
 add(new BoxShape(Vec3(8,.1,8)),RVec3(0,.15,3),Quat::sIdentity(),0,FLOOR,.9);
 auto energy=[&](){double e=0;for(auto*b:dynamics)e+=kinetic(b)+mass(b)*(-double(sys.GetGravity().GetY()))*b->GetCenterOfMassPosition().GetY();return e;};double initial_energy=energy();
 Observer observer;observer.drum=drum;observer.tongue=tongue;observer.player=player;observer.guide=guide;observer.contacts=&contacts;observer.origin=origin;observer.downhill=down;observer.normal=normal;observer.initial_tongue=tongue->GetCenterOfMassPosition();observer.grip_local=Vec3(tongue->GetInverseCenterOfMassTransform()*handle_world);observer.player_local=Vec3(.4,.65,.55);observer.observe();sys.AddStepListener(&observer);
 Ref<SixDOFConstraint>grip;bool acquired=false;double acquire_gap=0,positive_work=0,negative_work=0,max_control=0,control_target0=player_pos.GetX(),baseline_offset_at3=.35;unsigned errors=0,traction_ticks=0;
 std::cout<<std::setprecision(12);
 for(int tick=0;tick<1080;++tick){double t=tick*H;
  if(withdraw&&!acquired&&t>=3.-H*.1){acquire_gap=Vec3(point(tongue,observer.grip_local)-point(player,observer.player_local)).Length();baseline_offset_at3=tongue->GetPosition().GetX();
   if(acquire_gap<.15){SixDOFConstraintSettings c;c.mPosition1=point(tongue,observer.grip_local);c.mPosition2=point(player,observer.player_local);c.mNumVelocityStepsOverride=40;c.mNumPositionStepsOverride=8;for(int a=0;a<6;++a)c.MakeFreeAxis(static_cast<SixDOFConstraintSettings::EAxis>(a));for(int a=0;a<3;++a){c.mMotorSettings[a].mSpringSettings=SpringSettings(ESpringMode::StiffnessAndDamping,5000,180);c.mMotorSettings[a].SetForceLimit(a==0?240:40);}grip=static_cast<SixDOFConstraint*>(c.Create(*tongue,*player));for(int a=0;a<3;++a)grip->SetMotorState(static_cast<SixDOFConstraint::EAxis>(a),EMotorState::PositionAndVelocity);sys.AddConstraint(grip);observer.grip=grip;control_target0=player->GetCenterOfMassPosition().GetX();}acquired=true;
  }
  double force=0;
  if(withdraw&&grip&&t>=3&&contacts.foot.load()&&positive_work<WORK_CAP){double target=control_target0-.25*std::clamp((t-3.)/3.,0.,1.);double vx=player->GetLinearVelocity().GetX();force=std::clamp(2000*(target-player->GetCenterOfMassPosition().GetX())-180*vx,-CONTROL_CAP,CONTROL_CAP);double max_dx=H*(std::abs(vx)+H*(CONTROL_CAP+240)/mass(player));double budget_force=(WORK_CAP-positive_work)/std::max(1e-9,max_dx);force=std::clamp(force,-budget_force,budget_force);bi.AddForce(player->GetID(),Vec3(force,0,0));++traction_ticks;max_control=std::max(max_control,std::abs(force));}
  observer.current_control=force;RVec3 before=player->GetCenterOfMassPosition();errors|=unsigned(sys.Update(float(H),4,&temp,&jobs));observer.observe();double work=force*(player->GetCenterOfMassPosition().GetX()-before.GetX());positive_work+=std::max(0.,work);negative_work-=std::min(0.,work);
  if(tick%18==0)std::cout<<"SAMPLE "<<t+H<<' '<<tongue->GetPosition().GetX()<<' '<<observer.drum_s()<<' '<<observer.yaw()<<' '<<force<<' '<<positive_work<<' '<<contacts.foot.load()<<'\n';
 }
 sys.RemoveStepListener(&observer);
 std::cout<<"RESULT {\"case\":\""<<(withdraw?"bounded_withdrawal":"no_input")<<"\",\"global_velocity_steps\":10,\"global_position_steps\":2,\"slop_m\":"<<ps.mPenetrationSlop<<",\"local_velocity_steps\":"<<guide->GetNumVelocityStepsOverride()<<",\"local_position_steps\":"<<guide->GetNumPositionStepsOverride()<<",\"h\":"<<H<<",\"substep\":"<<observer.dt<<",\"callbacks\":"<<observer.callbacks<<",\"face_friction\":0.9,\"guide_friction_n\":35,\"drum_mass\":"<<mass(drum)<<",\"tongue_mass\":"<<mass(tongue)<<",\"player_mass\":"<<mass(player)<<",\"player_gravity_factor\":"<<player->GetMotionProperties()->GetGravityFactor()<<",\"player_friction\":"<<player->GetFriction()<<",\"initial_tongue_x_m\":0.35,\"final_tongue_x_m\":"<<tongue->GetPosition().GetX()<<",\"min_tongue_relative_travel_m\":"<<observer.min_travel<<",\"max_tongue_relative_travel_m\":"<<observer.max_travel<<",\"tongue_at_acquisition_m\":"<<baseline_offset_at3<<",\"acquire_gap_m\":"<<acquire_gap<<",\"grip_acquired\":"<<(bool(grip)?"true":"false")<<",\"max_grip_force_n\":"<<observer.max_grip<<",\"max_grip_error_m\":"<<observer.max_grip_error<<",\"max_grip_storage_proxy_j\":"<<observer.max_grip_storage<<",\"max_control_force_n\":"<<max_control<<",\"positive_control_work_j\":"<<positive_work<<",\"negative_control_work_j\":"<<negative_work<<",\"traction_ticks\":"<<traction_ticks<<",\"unsupported_force_samples\":"<<observer.unsupported_force_samples<<",\"max_guide_transverse_force_n\":"<<observer.max_guide<<",\"max_guide_torque_nm\":"<<observer.max_guide_torque<<",\"max_guide_friction_force_n\":"<<observer.max_guide_friction<<",\"max_guide_drift_m\":"<<observer.max_error<<",\"max_drum_yaw_rad\":"<<observer.max_yaw<<",\"final_drum_yaw_rad\":"<<observer.yaw()<<",\"max_drum_lateral_shift_m\":"<<observer.max_transverse_drift<<",\"final_drum_s_m\":"<<observer.drum_s()<<",\"drum_exit_time_s\":"<<observer.exit_time<<",\"drum_exit_speed_m_s\":"<<observer.exit_speed<<",\"drum_exit_yaw_rad\":"<<observer.exit_yaw<<",\"max_drum_speed_m_s\":"<<observer.max_drum_speed<<",\"max_contact_penetration_m\":"<<contacts.max_penetration.load()<<",\"initial_mechanical_energy_j\":"<<initial_energy<<",\"final_mechanical_energy_j\":"<<energy()<<",\"finite_states\":"<<(observer.finite?"true":"false")<<",\"update_errors\":"<<errors<<",\"contacts\":[";for(int i=0;i<10;++i){if(i)std::cout<<',';std::cout<<contacts.counts[i].load();}std::cout<<"]}\n";
 if(!observer.finite||errors||positive_work>80.01||max_control>250.001||observer.max_grip>246.7||observer.unsupported_force_samples)code=1;
 if(grip)sys.RemoveConstraint(grip);sys.RemoveConstraint(guide);grip=nullptr;guide=nullptr;for(auto*b:bodies){auto id=b->GetID();bi.RemoveBody(id);bi.DestroyBody(id);}
 }
 UnregisterTypes();delete Factory::sInstance;Factory::sInstance=nullptr;return code;
}
