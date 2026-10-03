// PRIVATE FEASIBILITY PROTOTYPE. Native Jolt, no shipping world changes.
// Units: m, kg, s, N, J. The executable's input defines the initial state.
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
using namespace JPH;
constexpr int COUNT=60;
constexpr double G=9.81, PITCH=.4, WIDTH=1., RUNG_RADIUS=.03;
constexpr double ROPE_DIAMETER=.05, ROPE_DENSITY=700., WOOD_DENSITY=650.;
constexpr double WOOD_MASS=WOOD_DENSITY*JPH_PI*RUNG_RADIUS*RUNG_RADIUS*WIDTH;
constexpr double ROPE_LINEAR_MASS=ROPE_DENSITY*JPH_PI*ROPE_DIAMETER*ROPE_DIAMETER/4.;
constexpr double ROPE_NODE_MASS=2.*PITCH*ROPE_LINEAR_MASS;
constexpr double MASS=WOOD_MASS+ROPE_NODE_MASS;
constexpr double IX=.5*WOOD_MASS*RUNG_RADIUS*RUNG_RADIUS;
constexpr double IYZ=WOOD_MASS*(3*RUNG_RADIUS*RUNG_RADIUS+WIDTH*WIDTH)/12.+ROPE_NODE_MASS*.44*.44;
constexpr double HAND_FORCE=1500., AXIS_FORCE=HAND_FORCE/1.7320508075688772;
struct Broad:BroadPhaseLayerInterface{uint GetNumBroadPhaseLayers()const override{return 2;}BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer x)const override{return BroadPhaseLayer(x);}const char*GetBroadPhaseLayerName(BroadPhaseLayer x)const override{return x.GetValue()?"MOVING":"STATIC";}};
struct BPFilter:ObjectVsBroadPhaseLayerFilter{bool ShouldCollide(ObjectLayer x,BroadPhaseLayer y)const override{return x==1||y.GetValue()==1;}};
struct Pair:ObjectLayerPairFilter{bool ShouldCollide(ObjectLayer x,ObjectLayer y)const override{return x==1||y==1;}};
struct Contacts:ContactListener{BodyID floor,player;unsigned floor_events=0,rung_events=0;bool floor_this_step=false;void mark(const Body&a,const Body&b){if(a.GetID()==player||b.GetID()==player){if(a.GetID()==floor||b.GetID()==floor){++floor_events;floor_this_step=true;}else ++rung_events;}}void OnContactAdded(const Body&a,const Body&b,const ContactManifold&,ContactSettings&)override{mark(a,b);}void OnContactPersisted(const Body&a,const Body&b,const ContactManifold&,ContactSettings&)override{mark(a,b);}};
struct Rope{Body*a;Body*b;Vec3 la,lb;Ref<DistanceConstraint> joint;Vec3 before_rel,before_normal;double before_gap;};
struct Hand{int rung=-1;Vec3 body_local,hold_local;Ref<SixDOFConstraint> joint;Quat initial_frame;Vec3 before_rel,before_axis[3];};
double body_energy(const Body*b,bool player=false){if(!b||b->IsStatic())return 0;double m=player?85.:MASS;auto v=b->GetLinearVelocity();auto w=b->GetRotation().Conjugated()*b->GetAngularVelocity();return .5*m*v.LengthSq()+(player?0.:.5*(IYZ*w.GetX()*w.GetX()+IX*w.GetY()*w.GetY()+IYZ*w.GetZ()*w.GetZ()))+m*G*b->GetCenterOfMassPosition().GetY();}
RVec3 point(const Body*b,Vec3 local){return b->GetCenterOfMassTransform()*local;}
// Seed42 fixes phases; time and actual relative velocity determine drag.
Vec3 wind(double t,double sign){return Vec3(float(sign*.25*std::sin(.23*t+.7)),0,float(sign*(1.6+.35*std::sin(.41*t+.271828)+.2*std::sin(.83*t+1.17))));}
int main(int argc,char**argv){double h=argc>1?std::atof(argv[1]):1./90.;std::string mode=argc>2?argv[2]:"unloaded";double breeze=argc>3?std::atof(argv[3]):1.;double side=argc>4?std::atof(argv[4]):0.;double catch_speed=argc>5?std::atof(argv[5]):2.5;double angle=argc>6?std::atof(argv[6]):0.;
if(h<=0||h>.02||std::abs(side)>.3){std::cerr<<"invalid inputs\n";return 2;}
RegisterDefaultAllocator();Factory::sInstance=new Factory;RegisterTypes();int outcome=0;
{
Broad broad;BPFilter bp;Pair pair;TempAllocatorImpl temp(32*1024*1024);JobSystemThreadPool jobs(cMaxPhysicsJobs,cMaxPhysicsBarriers,1);PhysicsSystem sys;sys.Init(256,0,1024,2048,broad,bp,pair);sys.SetGravity(Vec3(0,-G,0));auto ps=sys.GetPhysicsSettings();ps.mNumVelocitySteps=argc>7?std::atoi(argv[7]):64;ps.mNumPositionSteps=8;ps.mPenetrationSlop=.002;sys.SetPhysicsSettings(ps);auto&bi=sys.GetBodyInterface();Contacts contacts;sys.SetContactListener(&contacts);std::vector<Body*> bodies,rungs;std::vector<Rope> ropes;std::array<Hand,2> hands;Body*player=nullptr;
auto add=[&](BodyCreationSettings c){Body*b=bi.CreateBody(c);bi.AddBody(b->GetID(),EActivation::Activate);bodies.push_back(b);return b;};
BodyCreationSettings anchorcfg(new BoxShape(Vec3(.7,.15,.3)),RVec3(0,30.15,0),Quat::sIdentity(),EMotionType::Static,0);Body*anchor=add(anchorcfg);
BodyCreationSettings floorcfg(new BoxShape(Vec3(8,.3,8)),RVec3(0,2.7,0),Quat::sIdentity(),EMotionType::Static,0);Body*floor=add(floorcfg);contacts.floor=floor->GetID();
Quat tilt=Quat::sRotation(Vec3::sAxisX(),float(angle));double initial_pitch=mode=="slack"?.395:PITCH;
for(int i=0;i<COUNT;++i){RVec3 p=RVec3(0,30,0)+RVec3(tilt*Vec3(0,float(-initial_pitch*(i+1)),0));BodyCreationSettings c(new CylinderShape(WIDTH/2.,RUNG_RADIUS),p,tilt*Quat::sRotation(Vec3::sAxisZ(),JPH_PI*.5F),EMotionType::Dynamic,1);c.mOverrideMassProperties=EOverrideMassProperties::MassAndInertiaProvided;c.mMassPropertiesOverride.mMass=MASS;
// Cylinder localY is its long axis; rotate the prescribed inertia into that shape frame.
c.mMassPropertiesOverride.mInertia=Mat44::sScale(Vec3(IYZ,IX,IYZ));c.mFriction=.6;c.mRestitution=0;c.mLinearDamping=0;c.mAngularDamping=0;c.mAllowSleeping=false;c.mMotionQuality=EMotionQuality::LinearCast;rungs.push_back(add(c));}
auto rung_local=[&](double x){return Vec3(0,float(-x),0);}; // Shape localY maps to world -X.
for(int i=0;i<COUNT;++i)for(double x:{-.44,.44}){Body*a=i==0?anchor:rungs[i-1];Body*b=rungs[i];Vec3 la=i==0?Vec3(float(x),-.15,0):rung_local(x);Vec3 lb=rung_local(x);DistanceConstraintSettings c;c.mSpace=EConstraintSpace::LocalToBodyCOM;c.mPoint1=RVec3(la);c.mPoint2=RVec3(lb);c.mMinDistance=0;c.mMaxDistance=PITCH;Ref<DistanceConstraint> joint=static_cast<DistanceConstraint*>(c.Create(*a,*b));sys.AddConstraint(joint);ropes.push_back({a,b,la,lb,joint});}
auto release=[&](int which){auto&hand=hands[which];if(hand.joint){sys.RemoveConstraint(hand.joint);hand.joint=nullptr;}hand.rung=-1;};
auto attach=[&](int which,int rung){release(which);auto&hand=hands[which];hand.rung=rung;double hx=side+(which==0?-.15:.15);hand.hold_local=rung_local(hx);hand.body_local=Vec3(float(which==0?-.15:.15),.65,.55);SixDOFConstraintSettings c;c.mPosition1=point(rungs[rung],hand.hold_local);c.mPosition2=point(player,hand.body_local);for(int axis=0;axis<6;++axis)c.MakeFreeAxis(static_cast<SixDOFConstraintSettings::EAxis>(axis));for(int axis=0;axis<3;++axis){c.mMotorSettings[axis].mSpringSettings=SpringSettings(ESpringMode::StiffnessAndDamping,5000,180);c.mMotorSettings[axis].SetForceLimit(AXIS_FORCE);}hand.initial_frame=rungs[rung]->GetRotation();hand.joint=static_cast<SixDOFConstraint*>(c.Create(*rungs[rung],*player));for(int axis=0;axis<3;++axis)hand.joint->SetMotorState(static_cast<SixDOFConstraint::EAxis>(axis),EMotorState::PositionAndVelocity);sys.AddConstraint(hand.joint);};
const bool rider=mode=="rider"||mode=="climb";const double dt=h/4.;const int steps=int(std::llround(16./dt));bool attached=false,released=false,transferred0=false,transferred1=false;double inserted=0,initial=0,wind_source=0,aero_loss=0,hand_work=0,rope_loss=0,contact_loss=0;double max_residual=0,max_stretch=0,min_lambda=0,max_lambda=0,max_tension=0,max_hand_force=0,max_power=0,max_sway=0,max_hand_error=0,min_clearance=1e9;double release_jump=0,depart_speed=0,max_rope_injection=0,max_rung_speed=0,catch_peak=0,loaded_root=0,unloaded_root=0;int loaded_samples=0,unloaded_samples=0,slack_samples=0;double climb_start_y=0,climb_end_y=0;std::cout<<std::setprecision(10);
auto energy=[&](){double e=body_energy(player,true);for(auto*b:rungs)e+=body_energy(b);return e;};initial=energy();
for(int step=0;step<steps;++step){double t=step*dt;
if(rider&&!attached&&t>=3.-dt*.1){auto p=point(rungs[26],rung_local(side));BodyCreationSettings c(new CapsuleShape(.55,.35),p-RVec3(0,.65,.55),Quat::sIdentity(),EMotionType::Dynamic,1);c.mAllowedDOFs=EAllowedDOFs::TranslationX|EAllowedDOFs::TranslationY|EAllowedDOFs::TranslationZ;c.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;c.mMassPropertiesOverride.mMass=85;c.mLinearDamping=0;c.mAngularDamping=0;c.mFriction=0;c.mRestitution=0;c.mAllowSleeping=false;c.mMotionQuality=EMotionQuality::LinearCast;player=add(c);contacts.player=player->GetID();bi.SetLinearVelocity(player->GetID(),rungs[26]->GetPointVelocity(p)+Vec3(0,float(-catch_speed),0));inserted+=body_energy(player,true);attach(0,26);attach(1,26);attached=true;}
if(mode=="climb"&&attached&&!released){if(t>=5&&t<6){double shift=.4*(t-5);for(auto&hand:hands)if(hand.joint){hand.joint->SetTargetPositionCS(Vec3(0,float(shift),0));hand.joint->SetTargetVelocityCS(Vec3(0,.4,0));}if(climb_start_y==0)climb_start_y=player->GetPosition().GetY();}if(t>=6&&!transferred0){attach(0,25);transferred0=true;}if(t>=6.2&&!transferred1){attach(1,25);transferred1=true;climb_end_y=player->GetPosition().GetY();}}
if(attached&&!released&&t>=9.-dt*.1){Vec3 before=player->GetLinearVelocity();release(0);release(1);Vec3 after=player->GetLinearVelocity();release_jump=(after-before).Length();depart_speed=after.Length();released=true;}
Vec3 u=wind(t,breeze);std::vector<Vec3> forces,old_velocity;std::vector<RVec3> old_position;for(auto*b:rungs){Vec3 rel=u-b->GetLinearVelocity();double speed=rel.Length();Vec3 dir=speed>1e-9?rel/float(speed):Vec3::sAxisZ();Vec3 axis=b->GetRotation()*Vec3::sAxisY();double projection=std::abs(axis.Dot(dir));double area=2*RUNG_RADIUS*WIDTH*std::sqrt(std::max(0.,1-projection*projection))+JPH_PI*RUNG_RADIUS*RUNG_RADIUS*projection;
// Lump each pair of rope spans' crossflow area with their assigned rope mass.
double vertical=std::abs(dir.Dot(Vec3::sAxisY()));area+=2*ROPE_DIAMETER*PITCH*std::sqrt(std::max(0.,1-vertical*vertical));Vec3 f=rel*float(.5*1.225*1.2*area*speed);forces.push_back(f);old_velocity.push_back(b->GetLinearVelocity());old_position.push_back(b->GetCenterOfMassPosition());bi.AddForce(b->GetID(),f);}
for(auto&rope:ropes){auto p1=point(rope.a,rope.la),p2=point(rope.b,rope.lb);Vec3 delta=Vec3(p2-p1);rope.before_normal=delta.NormalizedOr(Vec3(0,-1,0));rope.before_gap=PITCH-delta.Length();rope.before_rel=rope.b->GetPointVelocity(p2)-rope.a->GetPointVelocity(p1);}
for(auto&hand:hands)if(hand.joint){auto p2=point(player,hand.body_local);hand.before_rel=player->GetPointVelocity(p2)-rungs[hand.rung]->GetPointVelocity(p2);Quat cs=rungs[hand.rung]->GetRotation()*hand.initial_frame.Conjugated();for(int axis=0;axis<3;++axis)hand.before_axis[axis]=cs*(axis==0?Vec3::sAxisX():(axis==1?Vec3::sAxisY():Vec3::sAxisZ()));}
Vec3 before_player=player?player->GetLinearVelocity():Vec3::sZero();contacts.floor_this_step=false;sys.Update(float(dt),1,&temp,&jobs);
for(int i=0;i<COUNT;++i){wind_source+=forces[i].Dot(u)*dt;double w=forces[i].Dot(Vec3(rungs[i]->GetCenterOfMassPosition()-old_position[i]));aero_loss+=forces[i].Dot(u)*dt-w;max_rung_speed=std::max(max_rung_speed,double(rungs[i]->GetLinearVelocity().Length()));auto p=rungs[i]->GetPosition();max_sway=std::max(max_sway,double(std::sqrt(p.GetX()*p.GetX()+p.GetZ()*p.GetZ())));min_clearance=std::min(min_clearance,double(p.GetY()-.5*std::abs((rungs[i]->GetRotation()*Vec3::sAxisY()).GetY())-RUNG_RADIUS-3.));}
double root=0;for(int i=0;i<int(ropes.size());++i){auto&rope=ropes[i];double lambda=rope.joint->GetTotalLambdaPosition();min_lambda=std::min(min_lambda,lambda);max_lambda=std::max(max_lambda,lambda);max_tension=std::max(max_tension,-lambda/dt);auto p1=point(rope.a,rope.la),p2=point(rope.b,rope.lb);double length=Vec3(p2-p1).Length();max_stretch=std::max(max_stretch,length-PITCH);if(length<PITCH-.001&&std::abs(lambda)<1e-7)++slack_samples;Vec3 after=rope.b->GetPointVelocity(p2)-rope.a->GetPointVelocity(p1);double w=lambda*rope.before_normal.Dot((rope.before_rel+after)*.5F);rope_loss-=w;max_rope_injection=std::max(max_rope_injection,w);if(i<2)root-=lambda/dt*rope.before_normal.GetY();}
double step_hand_work=0;for(auto&hand:hands)if(hand.joint){Vec3 impulse=hand.joint->GetTotalLambdaMotorTranslation();max_hand_force=std::max(max_hand_force,double(impulse.Length()/dt));auto p2=point(player,hand.body_local);Vec3 after=player->GetPointVelocity(p2)-rungs[hand.rung]->GetPointVelocity(p2);for(int axis=0;axis<3;++axis)step_hand_work+=impulse[axis]*hand.before_axis[axis].Dot((hand.before_rel+after)*.5F);double error=Vec3(p2-point(rungs[hand.rung],hand.hold_local)).Length();max_hand_error=std::max(max_hand_error,error);if(t<4)catch_peak=std::max(catch_peak,double(impulse.Length()/dt));}
hand_work+=step_hand_work;max_power=std::max(max_power,step_hand_work/dt);
if(player&&contacts.floor_this_step&&released){Vec3 impulse=(player->GetLinearVelocity()-before_player-Vec3(0,float(-G*dt),0))*85.F;contact_loss-=impulse.Dot((before_player+player->GetLinearVelocity())*.5F);}
if(t>=7&&t<8.5&&player&&!released){loaded_root+=-root;++loaded_samples;}if(t>=12&&t<15){unloaded_root+=-root;++unloaded_samples;}
double residual=energy()-initial-inserted-wind_source+aero_loss-hand_work+rope_loss+contact_loss;max_residual=std::max(max_residual,std::abs(residual));if(step%std::max(1,int(std::llround(.1/dt)))==0){auto p=rungs.back()->GetPosition();std::cout<<"SAMPLE "<<t<<' '<<p.GetX()<<' '<<p.GetY()<<' '<<p.GetZ()<<' '<<(-root)<<' '<<residual<<' '<<(player?player->GetPosition().GetY():0)<<' '<<(player?player->GetLinearVelocity().GetY():0)<<'\n';}}
double final=energy();std::cout<<"RESULT {\"mode\":\""<<mode<<"\",\"velocity_iterations\":"<<ps.mNumVelocitySteps<<",\"h\":"<<h<<",\"substep\":"<<dt<<",\"breeze\":"<<breeze<<",\"side\":"<<side<<",\"catch_speed\":"<<catch_speed<<",\"rung_mass\":"<<MASS<<",\"ladder_mass\":"<<MASS*COUNT<<",\"max_stretch\":"<<max_stretch<<",\"min_lambda\":"<<min_lambda<<",\"max_lambda\":"<<max_lambda<<",\"max_tension\":"<<max_tension<<",\"max_hand_force\":"<<max_hand_force<<",\"catch_peak\":"<<catch_peak<<",\"max_hand_error\":"<<max_hand_error<<",\"max_power\":"<<max_power<<",\"max_sway\":"<<max_sway<<",\"max_rung_speed\":"<<max_rung_speed<<",\"min_floor_clearance\":"<<min_clearance<<",\"release_velocity_jump\":"<<release_jump<<",\"departure_speed\":"<<depart_speed<<",\"loaded_root_reaction\":"<<(loaded_samples?loaded_root/loaded_samples:0)<<",\"unloaded_root_reaction\":"<<(unloaded_samples?unloaded_root/unloaded_samples:0)<<",\"floor_contacts\":"<<contacts.floor_events<<",\"rung_contacts\":"<<contacts.rung_events<<",\"slack_samples\":"<<slack_samples<<",\"climb_rise\":"<<(climb_end_y-climb_start_y)<<",\"initial_energy\":"<<initial<<",\"inserted_energy\":"<<inserted<<",\"final_energy\":"<<final<<",\"wind_source_j\":"<<wind_source<<",\"aero_loss_j\":"<<aero_loss<<",\"hand_work_j\":"<<hand_work<<",\"rope_loss_j\":"<<rope_loss<<",\"contact_loss_j\":"<<contact_loss<<",\"max_rope_injection_step_j\":"<<max_rope_injection<<",\"max_residual_j\":"<<max_residual<<",\"tail_z\":"<<rungs.back()->GetPosition().GetZ()<<",\"tail_y\":"<<rungs.back()->GetPosition().GetY()<<",\"player_y\":"<<(player?player->GetPosition().GetY():0)<<"}\n";
if(max_lambda>1e-5||max_hand_force>HAND_FORCE+1||release_jump!=0||!std::isfinite(final))outcome=1;for(int i=0;i<2;++i)release(i);for(auto&rope:ropes)sys.RemoveConstraint(rope.joint);ropes.clear();for(auto*b:bodies){auto id=b->GetID();bi.RemoveBody(id);bi.DestroyBody(id);}
}
UnregisterTypes();delete Factory::sInstance;Factory::sInstance=nullptr;return outcome;}
