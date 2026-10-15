// Isolated causal impact diagnostic, not shipping capture or route acceptance.
#include "sim/grappling_gun.hpp"
#include "sim/grappling_hook_geometry.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace JPH;
using Gun=scraperx::sim::GrapplingGun;
namespace {
constexpr float dt=1.0F/360.0F;
const RVec3 bar_center(4.04,5.24,0);
void check(bool x,const char *message){if(!x)throw std::runtime_error(message);}
struct BP final:BroadPhaseLayerInterface {
 uint GetNumBroadPhaseLayers()const override{return 2;}
 BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer x)const override{return BroadPhaseLayer(x);}
 const char*GetBroadPhaseLayerName(BroadPhaseLayer)const override{return "capture-probe";}
};
struct BV final:ObjectVsBroadPhaseLayerFilter {bool ShouldCollide(ObjectLayer a,BroadPhaseLayer b)const override{return a!=0||b.GetValue()!=0;}};
struct LP final:ObjectLayerPairFilter {bool ShouldCollide(ObjectLayer a,ObjectLayer b)const override{return a!=0||b!=0;}};
void vector(const char*name,Vec3 v){std::cout<<' '<<name<<'='<<v.GetX()<<','<<v.GetY()<<','<<v.GetZ();}
void point(const char*name,RVec3 v){vector(name,Vec3(v));}
Vec3 spin_momentum(const Body&b){
 auto*motion=b.GetMotionProperties();auto r=Mat44::sRotation(b.GetRotation());
 auto inertia=r*motion->GetLocalSpaceInverseInertia().Inversed3x3()*r.Transposed3x3();
 return inertia*b.GetAngularVelocity();
}
double rotation_energy(const Body&b){
 auto*motion=b.GetMotionProperties(); auto r=Mat44::sRotation(b.GetRotation());
 auto inertia=r*motion->GetLocalSpaceInverseInertia().Inversed3x3()*r.Transposed3x3();
 return .5*double(b.GetAngularVelocity().Dot(inertia*b.GetAngularVelocity()));
}
void sample(const char*label,int tick,const Body&b){
 auto c=b.GetCenterOfMassPosition(); const auto transform=b.GetWorldTransform();
 auto aperture=transform*Vec3(0,.10F,0),eye=transform*Gun::hook_eye_local();
 const float mass=1/b.GetMotionProperties()->GetInverseMass();
 const Vec3 orbital=Vec3(c-bar_center).Cross(mass*b.GetLinearVelocity());
 std::cout<<label<<" tick="<<tick<<" time="<<tick*dt;
 point("COM",c);point("aperture",aperture);point("eye",eye);
 point("bar_in_body",transform.Inversed()*bar_center);
 vector("linear",b.GetLinearVelocity());vector("angular",b.GetAngularVelocity());
 vector("orbital_L_bar",orbital); std::cout<<" rotor_energy_j="<<rotation_energy(b)<<'\n';
}
struct Contacts final:ContactListener{
 BodyID hook,bar;int tick=0,first_tick=-1;unsigned contacts=0,other_hook_contacts=0;Vec3 last_normal=Vec3::sZero();RVec3 last_hook_point=RVec3::sZero();
 void OnContactAdded(const Body&a,const Body&b,const ContactManifold&m,ContactSettings&)override{
  observe(a,b,m);
 }
 void OnContactPersisted(const Body&a,const Body&b,const ContactManifold&m,ContactSettings&)override{
  observe(a,b,m);
 }
 void observe(const Body&a,const Body&b,const ContactManifold&m){
  if((a.GetID()==hook||b.GetID()==hook)&&a.GetID()!=bar&&b.GetID()!=bar)++other_hook_contacts;
  if(!((a.GetID()==hook&&b.GetID()==bar)||(b.GetID()==hook&&a.GetID()==bar)))return;
  ++contacts;const bool first=a.GetID()==hook;
  last_normal=first?m.mWorldSpaceNormal:-m.mWorldSpaceNormal;
  last_hook_point=first?m.GetWorldSpaceContactPointOn1(0):m.GetWorldSpaceContactPointOn2(0);
  if(first_tick>=0) { return; }
  first_tick=tick;
  const Body&h=first?a:b;
  std::cout<<"FIRST_CONTACT tick="<<tick;
  point("hook_point",first?m.GetWorldSpaceContactPointOn1(0):m.GetWorldSpaceContactPointOn2(0));
  point("bar_point",first?m.GetWorldSpaceContactPointOn2(0):m.GetWorldSpaceContactPointOn1(0));
  vector("normal_hook_to_bar",first?m.mWorldSpaceNormal:-m.mWorldSpaceNormal);
  std::cout<<" penetration="<<m.mPenetrationDepth<<'\n';sample("PRE_CONTACT",tick,h);
 }
};
struct Step final:PhysicsStepListener{Gun&gun;explicit Step(Gun&g):gun(g){} void OnStep(const PhysicsStepListenerContext&c)override{gun.pre_step(c.mDeltaTime);}};
}
int main(){try{
 RegisterDefaultAllocator();Factory::sInstance=new Factory;RegisterTypes();
 {
 BP bp;BV bv;LP lp;PhysicsSystem world;world.Init(64,0,256,256,bp,bv,lp);
 world.SetGravity(Vec3(0,-9.81F,0));TempAllocatorImpl allocator(8*1024*1024);JobSystemSingleThreaded jobs(1024);
 auto&bodies=world.GetBodyInterface();
 BodyCreationSettings player_settings(new BoxShape(Vec3(.3F,.3F,.3F)),RVec3::sZero(),Quat::sIdentity(),EMotionType::Dynamic,1);
 player_settings.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;player_settings.mMassPropertiesOverride.mMass=85;
 player_settings.mAllowedDOFs=EAllowedDOFs::TranslationX|EAllowedDOFs::TranslationY|EAllowedDOFs::TranslationZ;
 player_settings.mLinearDamping=player_settings.mAngularDamping=0;player_settings.mAllowSleeping=false;
 auto player=bodies.CreateAndAddBody(player_settings,EActivation::Activate);
 BodyCreationSettings floor_settings(new BoxShape(Vec3(3,.1F,2),0),RVec3(0,-.4,0),Quat::sIdentity(),EMotionType::Static,0);
 auto floor=bodies.CreateAndAddBody(floor_settings,EActivation::DontActivate);
 BodyCreationSettings bar_settings(new CylinderShape(.115F,.020F,0),bar_center,Quat::sRotation(Vec3::sAxisX(),.5F*3.14159265358979323846F),EMotionType::Static,0);
 bar_settings.mFriction=.6F;bar_settings.mRestitution=0;
 auto bar=bodies.CreateAndAddBody(bar_settings,EActivation::DontActivate);
 check(!player.IsInvalid()&&!floor.IsInvalid()&&!bar.IsInvalid(),"fixture bodies created");
 Gun gun(world,player,1);Step step(gun);Contacts contacts;contacts.bar=bar;
 world.AddStepListener(&step);world.SetContactListener(&contacts);
 const double speed=std::sqrt(2*Gun::cartridge_energy_j/(1.0/85+1.0/Gun::hook_mass_kg))/Gun::hook_mass_kg;
 double low=1.1,high=1.49;const double range=bar_center.GetX(),height=bar_center.GetY()-.35;
 for(int i=0;i<48;++i){double angle=(low+high)*.5;double t=(range-.95*std::cos(angle))/(speed*std::cos(angle));double y=.95*std::sin(angle)+speed*std::sin(angle)*t-4.905*t*t;if(y>height)low=angle;else high=angle;}
 const double angle=(low+high)*.5;const Vec3 aim(float(std::cos(angle)),float(std::sin(angle)),0);
 std::cout<<std::setprecision(12)<<"CONFIG parts="<<scraperx::sim::grappling_hook_layout().parts.size()<<" aim_angle="<<angle<<" speed="<<speed<<" dt="<<dt<<" gravity=9.81 bar_diameter=.04 bar_length=.23 floor_top=-.3\n";
 check(gun.fire(aim),"production gun fires");auto initial=gun.readback();contacts.hook=initial.hook;
 std::cout<<"LAUNCH paid_j="<<initial.last_launch_work_j;point("player_endpoint",initial.player_endpoint);std::cout<<'\n';
 int first_impulse_tick=-1;int failure_tick=-1;double peak_spin=0,peak_rotor=0;int peak_tick=0;
 for(int tick=0;tick<=1800;++tick){
  auto rb=gun.readback();BodyLockRead lock(world.GetBodyLockInterface(),rb.hook);check(lock.Succeeded(),"actual hook exists");
  const Body&h=lock.GetBody();double spin=h.GetAngularVelocity().Length();
  if(spin>peak_spin){peak_spin=spin;peak_tick=tick;}peak_rotor=std::max(peak_rotor,rotation_energy(h));
  if(tick%30==0||tick==contacts.first_tick||tick==contacts.first_tick+1)sample("STATE",tick,h);
  if(failure_tick<0&&rb.failure!=Gun::Failure::None){failure_tick=tick;std::cout<<"LINE_FAILURE tick="<<tick<<" failure="<<unsigned(rb.failure)<<" state="<<unsigned(rb.state)<<'\n';sample("FAILURE_STATE",tick,h);}
  if(tick==1800)break;
  const Vec3 before_v=h.GetLinearVelocity(),before_w=h.GetAngularVelocity(),before_L=spin_momentum(h);
  const float hook_mass=1/h.GetMotionProperties()->GetInverseMass();
  const unsigned before_contacts=contacts.contacts,before_other=contacts.other_hook_contacts;
  // Read lock scope ends before the native physics update.
  lock.ReleaseLock();contacts.tick=tick+1;
  check(world.Update(dt,1,&allocator,&jobs)==EPhysicsUpdateError::None,"native update succeeds");gun.post_step();
  if(contacts.first_tick==tick+1||(first_impulse_tick<0&&contacts.contacts>before_contacts)){
   BodyLockRead after_lock(world.GetBodyLockInterface(),initial.hook);check(after_lock.Succeeded(),"post-contact actual hook");
   const Body&after=after_lock.GetBody();auto after_rb=gun.readback();
   const Vec3 net_J=hook_mass*(after.GetLinearVelocity()-before_v-Vec3(0,-9.81F*dt,0));
   const Vec3 delta_L=spin_momentum(after)-before_L;
   const bool actual_impulse=first_impulse_tick<0&&(net_J.Length()>1e-4F||delta_L.Length()>1e-5F);
   if(actual_impulse) first_impulse_tick=tick+1;
   if(contacts.first_tick==tick+1||actual_impulse){
   std::cout<<(actual_impulse?"FIRST_NONZERO_IMPULSE_SUBSTEP":"FIRST_IMPACT_SUBSTEP")<<" tick="<<tick+1<<" bar_manifolds="<<contacts.contacts-before_contacts<<" other_hook_manifolds="<<contacts.other_hook_contacts-before_other<<" tension_before="<<rb.tension_n<<" tension_after="<<after_rb.tension_n;
   vector("pre_linear",before_v);vector("pre_angular",before_w);vector("pre_spin_L",before_L);
   vector("net_J_gravity_removed",hook_mass*(after.GetLinearVelocity()-before_v-Vec3(0,-9.81F*dt,0)));
   vector("delta_spin_L",delta_L);vector("last_manifold_normal",contacts.last_normal);point("last_manifold_hook_point",contacts.last_hook_point);std::cout<<'\n';sample("POST_CONTACT_SUBSTEP",tick+1,after);
   }
  }
 }
 std::cout<<"SUMMARY first_contact_tick="<<contacts.first_tick<<" first_nonzero_impulse_tick="<<first_impulse_tick<<" contact_callbacks="<<contacts.contacts<<" failure_tick="<<failure_tick<<" spin_peak="<<peak_spin<<" spin_peak_tick="<<peak_tick<<" rotor_energy_peak_j="<<peak_rotor<<" final_failure="<<unsigned(gun.readback().failure)<<" diagnosis_only=1\n";
 world.RemoveStepListener(&step);world.SetContactListener(nullptr);
 }
 UnregisterTypes();delete Factory::sInstance;Factory::sInstance=nullptr;return 0;
 }catch(const std::exception&e){std::cerr<<"PROBE_ERROR "<<e.what()<<'\n';return 1;}}
