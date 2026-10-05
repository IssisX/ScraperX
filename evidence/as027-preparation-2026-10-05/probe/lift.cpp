#include "machine.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/RegisterTypes.h>
#include <cstdio>
#include <set>
#include <Jolt/Physics/Body/MotionProperties.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <cmath>
#include <algorithm>
using namespace JPH;
using namespace scraperx::sim;
using namespace scraperx::sim::insertable;
struct BP:BroadPhaseLayerInterface { uint GetNumBroadPhaseLayers() const override{return 2;} BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l) const override{return BroadPhaseLayer(l);} const char*GetBroadPhaseLayerName(BroadPhaseLayer)const override{return "probe";} };
struct BF:ObjectVsBroadPhaseLayerFilter {bool ShouldCollide(ObjectLayer a,BroadPhaseLayer b)const override{return a||b.GetValue();}};
struct LF:ObjectLayerPairFilter {bool ShouldCollide(ObjectLayer a,ObjectLayer b)const override{return a||b;}};
struct Contacts:ContactListener {std::set<std::pair<uint64,uint64>> pairs; void OnContactAdded(const Body&a,const Body&b,const ContactManifold&,ContactSettings&)override{const uint64 first=a.GetUserData(),second=b.GetUserData();const std::pair<uint64,uint64> pair{std::min(first,second),std::max(first,second)}; if(pairs.insert(pair).second)std::printf("CONTACT %llu %llu\n",(unsigned long long)pair.first,(unsigned long long)pair.second);}};
struct Joint { BodyIndex a,b; Vec3 pa,pb; };
int main(int argc,char**argv){
 const float hz=argc>1?std::atof(argv[1]):90, rider_mass=argc>2?std::atof(argv[2]):85, force=argc>3?std::atof(argv[3]):200000;
 const float eccentric=argc>5?std::atof(argv[5]):0;
 const int mode=argc>4?std::atoi(argv[4]):0;const bool power_loss=mode==1;
 const float dt=1/hz, a=DegreesToRadians(15.f), end=DegreesToRadians(65.f), L=18,w=L*cos(a),h=L*sin(a), stroke=w-L*cos(end);
 RegisterDefaultAllocator();Factory::sInstance=new Factory;RegisterTypes();
 {
 BP bp;BF bf;LF lf;PhysicsSystem world;world.Init(128,0,256,1024,bp,bf,lf);world.SetGravity(Vec3(0,-9.81,0));
 TempAllocatorImpl temp(32*1024*1024);JobSystemSingleThreaded jobs(1024);
 Contacts contacts;world.SetContactListener(&contacts);
 kit::Kit kit(world,0,1);
 Machine m({kit,world,{}, {1970,2},{2970,10}},"probe.two_stage",2,9);
 const auto base=m.body("base",{box(Vec3(9.4,.5,2.2),Material::Concrete)},Vec3(9,1,0),0);
 const auto guide=m.body("guide",{box(Vec3(.3,17,.3),Material::Steel)},Vec3(-1,18,2.4),0);
 const auto foot=m.body("foot",{box(Vec3(.5,.25,.7))},Vec3(w,2,0),160);
 const auto mid=m.body("mid",{box(Vec3(9.2,.2,.2),Material::Steel,Vec3(0,0,-1.6)),box(Vec3(9.2,.2,.2),Material::Steel,Vec3(0,0,1.6))},Vec3(9,2+h+.65,0),300);
 const auto center=m.body("center",{box(Vec3(.5,.2,.7))},Vec3(w,2+h,0),160);
 const auto deck=m.body("deck",{box(Vec3(9.2,.3,1.8),Material::Timber)},Vec3(9,2+2*h+.65,0),800);
 const auto top=m.body("top",{box(Vec3(.5,.2,.7))},Vec3(w,2+2*h,0),160);
 auto drive=m.slider(base,foot,Vec3::sAxisX(),-stroke,.01);
 m.slider(guide,mid,Vec3::sAxisY(),-.02,12);
 m.slider(guide,deck,Vec3::sAxisY(),-.02,24);
 m.slider(mid,center,Vec3::sAxisX(),-stroke,.01);
 m.slider(deck,top,Vec3::sAxisX(),-stroke,.01);
 std::vector<Joint> joints;
 auto hinge=[&](BodyIndex b1,BodyIndex b2,Vec3 p){m.hinge(b1,b2,p);m.no_collision(b1,b2);joints.push_back({b1,b2,Vec3(m.native(b1).GetWorldTransform().InversedRotationTranslation()*RVec3(p)),Vec3(m.native(b2).GetWorldTransform().InversedRotationTranslation()*RVec3(p))});};
 for(int stage=0;stage<2;++stage){
   float y=2+stage*h;
   auto up=m.body("up",{box(Vec3(9,.18,.2),Material::Rust,Vec3(0,0,-.65)),box(Vec3(9,.18,.2),Material::Rust,Vec3(0,0,.65))},Vec3(w/2,y+h/2,0),600,a);
   auto down=m.body("down",{box(Vec3(9,.18,.2),Material::Steel,Vec3(0,0,-1.1)),box(Vec3(9,.18,.2),Material::Steel,Vec3(0,0,1.1))},Vec3(w/2,y+h/2,0),600,-a);
   hinge(stage?mid:base,up,Vec3(0,y,0));hinge(stage?center:foot,down,Vec3(w,y,0));
   hinge(up,down,Vec3(w/2,y+h/2,0));hinge(stage?deck:mid,down,Vec3(0,y+h,0));hinge(stage?top:center,up,Vec3(w,y+h,0));
 }
 BodyIndex rider;
 if(rider_mass>0)rider=m.body("rider",{box(Vec3(.35,.9,.35))},Vec3(9+eccentric,2+2*h+.95+.9,0),rider_mass);
 world.OptimizeBroadPhase();
 auto potential=[&](){double value=0;for(auto &entry:m.bodies){auto &b=m.native(entry.index);if(b.IsDynamic())value+=9.81*b.GetCenterOfMassPosition().GetY()/b.GetMotionProperties()->GetInverseMass();}return value;};
 const double initial=kit.body_position(deck).GetY();double initialPE=potential(),settledY=initial,settledPE=initialPE,peakPower=0,maxResidual=0;double work=0,peakForce=0,peakJoint=0,peakSpeed=0,offY=0,offDrop=0;int errors=0;double maxRiderOffset=0;
 float priorTarget=0;
 kit::Kit::Checkpoint saved;float savedTarget=0;double savedWork=0;bool restored=false,replayMeasured=false;RVec3 replayPosition;Vec3 replayVelocity;
 const int n=int((mode==3?90:mode==2?68:58)*hz);
 for(int i=0;i<n;++i){float t=i*dt;
   if(!power_loss&&i==int(20*hz)&&!restored){kit.capture(saved);savedTarget=priorTarget;savedWork=work;}
   if(!power_loss&&i==int(25*hz)&&!replayMeasured){
     if(!restored){replayPosition=kit.body_position(deck);replayVelocity=kit.body_velocity(deck);kit.restore(saved);for(auto &constraint:world.GetConstraints())constraint->ResetWarmStart();priorTarget=savedTarget;work=savedWork;restored=true;i=int(20*hz)-1;continue;}
     std::printf("REPLAY position_error_m=%.8f velocity_error_mps=%.8f\n",(kit.body_position(deck)-replayPosition).Length(),(kit.body_velocity(deck)-replayVelocity).Length());replayMeasured=true;
   }
   bool powered=t>=2&&t<(mode==3?85:mode==2?60:50)&&(!power_loss||t<12)&&(mode!=2||t<12||t>=17);
   float remain=stroke+drive->GetCurrentPosition();
   float target=(mode==3&&t>=32)?0:-stroke;
   float speed=powered?std::clamp((target-drive->GetCurrentPosition())*.6f,-.22f,.22f):0;
   if(powered){speed=std::clamp(speed,priorTarget-.11f*dt,priorTarget+.11f*dt);}priorTarget=speed;
   if(i==int(2*hz)){settledY=kit.body_position(deck).GetY();settledPE=potential();}
   drive->GetMotorSettings().SetForceLimit(force);
   drive->SetMaxFrictionForce(powered?1500:220000);
   drive->SetMotorState(powered?EMotorState::Velocity:EMotorState::Off);drive->SetTargetVelocity(speed);
   const double x0=kit.body_position(foot).GetX();
   if(power_loss&&i==int(12*hz))offY=kit.body_position(deck).GetY();
   auto error=world.Update(dt,1,&temp,&jobs);if(error!=EPhysicsUpdateError::None)++errors;
   double f=drive->GetTotalLambdaMotor()/dt;
   if(powered){peakPower=std::max(peakPower,std::abs(f*(kit.body_position(foot).GetX()-x0)/dt));work+=std::max(0.0,f*(kit.body_position(foot).GetX()-x0));peakForce=std::max(peakForce,std::abs(f));}
   for(auto &j:joints){double gap=(m.native(j.a).GetWorldTransform()*j.pa-m.native(j.b).GetWorldTransform()*j.pb).Length();peakJoint=std::max(peakJoint,gap);}
   peakSpeed=std::max(peakSpeed,std::abs(double(kit.body_velocity(deck).GetY())));
   if(rider.valid()){auto relative=kit.body_position(rider)-kit.body_position(deck);maxRiderOffset=std::max(maxRiderOffset,double(std::sqrt((relative.GetX()-eccentric)*(relative.GetX()-eccentric)+relative.GetZ()*relative.GetZ())));}
   if(power_loss&&t>=12)offDrop=std::max(offDrop,offY-kit.body_position(deck).GetY());
   if(i%int(2*hz)==0)std::printf("t=%.2f rise=%.5f foot=%.5f vy=%.5f force=%.1f work=%.1f\n",t,kit.body_position(deck).GetY()-initial,drive->GetCurrentPosition(),kit.body_velocity(deck).GetY(),f,work);
 }
 std::printf("RESULT hz=%.0f rider=%.1f cap=%.0f mode=%d rise=%.6f theory=%.6f work_j=%.3f peak_force_n=%.3f peak_joint_m=%.6f peak_speed=%.5f off_drop_m=%.6f rider_gap=%.6f max_rider_offset=%.6f errors=%d\n",hz,rider_mass,force,mode,kit.body_position(deck).GetY()-initial,2*L*(sin(end)-sin(a)),work,peakForce,peakJoint,peakSpeed,offDrop,rider.valid()?kit.body_position(rider).GetY()-kit.body_position(deck).GetY()-1.2:0,maxRiderOffset,errors);
 std::printf("LEDGER settled_travel_m=%.6f delta_pe_j=%.3f positive_work_j=%.3f peak_power_w=%.3f positive_work_minus_pe_fraction=%.6f eccentric_m=%.2f\n",kit.body_position(deck).GetY()-settledY,potential()-settledPE,work,peakPower,(work-(potential()-settledPE))/std::max(work,1.0),eccentric);
 }
 UnregisterTypes();delete Factory::sInstance;
}
