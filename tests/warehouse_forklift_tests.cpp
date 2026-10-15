#include "sim/warehouse_forklift.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace JPH;
using namespace scraperx::sim;
namespace {
void require(bool value,const char*why){if(!value){std::cerr<<"FAIL forklift "<<why<<'\n';std::exit(1);}}
struct BP final:BroadPhaseLayerInterface {uint GetNumBroadPhaseLayers()const override{return 2;} BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer l)const override{return BroadPhaseLayer(l);} const char*GetBroadPhaseLayerName(BroadPhaseLayer)const override{return "forklift";}};
struct BF final:ObjectVsBroadPhaseLayerFilter {bool ShouldCollide(ObjectLayer a,BroadPhaseLayer b)const override{return a||b.GetValue();}};
struct LF final:ObjectLayerPairFilter {bool ShouldCollide(ObjectLayer a,ObjectLayer b)const override{return a||b;}};
struct Contacts final:ContactListener {
 bool fork_cargo=false;
 void OnContactAdded(const Body&a,const Body&b,const ContactManifold&,ContactSettings&)override{record(a,b);}
 void OnContactPersisted(const Body&a,const Body&b,const ContactManifold&,ContactSettings&)override{record(a,b);}
 void record(const Body&a,const Body&b){if((a.GetUserData()==2403&&b.GetUserData()==2404)||(a.GetUserData()==2404&&b.GetUserData()==2403))fork_cargo=true;}
};
kit::Part box(Vec3 half,float mass,Vec3 offset=Vec3::sZero()){kit::Part p;p.half=half;p.mass_kg=mass;p.offset=offset;p.convex_radius=.005F;return p;}
struct Fixture {
 BP bp;BF bf;LF lf;PhysicsSystem world;TempAllocatorImpl temp{32*1024*1024};JobSystemSingleThreaded jobs{1024};Contacts contacts;
 std::unique_ptr<kit::Kit> kit;std::unique_ptr<WarehouseForklift> truck;kit::BodyIndex cargo;
 Fixture(bool ground=true,float load=600,bool under_forks=true){
  world.Init(64,0,256,2048,bp,bf,lf);world.SetGravity(Vec3(0,-9.81F,0));world.SetContactListener(&contacts);kit=std::make_unique<kit::Kit>(world,0,1);
  if(ground)kit->add_body(1400,{box(Vec3(30,.1F,30),0)},RVec3(0,-.1,0),Quat::sIdentity(),0,.8F);
  WarehouseForklift::Config c;
  c.chassis=kit->add_body(2400,{box(Vec3(.65F,.25F,.95F),2000),box(Vec3(.60F,.2F,.25F),800,Vec3(0,.45F,-.65F))},RVec3(0,.65,0),Quat::sIdentity(),2800,.8F);
  c.mast=kit->add_body(2401,{box(Vec3(.5F,1.3F,.06F),220)},RVec3(0,1.65,1.02),Quat::sIdentity(),220,.8F);
  c.carriage=kit->add_body(2402,{box(Vec3(.5F,.20F,.05F),55)},RVec3(0,.30,1.15),Quat::sIdentity(),55,.8F);
  c.fork_frame=kit->add_body(2403,{box(Vec3(.52F,.22F,.035F),25,Vec3(0,.20F,-.45F)),box(Vec3(.07F,.035F,.62F),25,Vec3(-.35F,0,.12F)),box(Vec3(.07F,.035F,.62F),25,Vec3(.35F,0,.12F))},RVec3(0,.08,1.66),Quat::sIdentity(),75,.85F);
  c.tilt_pivot=RVec3(0,.40,1.02);c.carriage_mount=Vec3(0,0,.06F);c.tool_mount=Vec3(0,.22F,-.45F);
  truck=std::make_unique<WarehouseForklift>(world,*kit,c);
  if(load>0)cargo=kit->add_body(2404,{box(Vec3(1.5F,.12F,.35F),load)},RVec3(under_forks?0:4,.25,1.95),Quat::sIdentity(),load,.85F);
  world.OptimizeBroadPhase();
 }
 ~Fixture(){truck.reset();kit.reset();world.SetContactListener(nullptr);}
 void tick(WarehouseForklift::Command cmd={},int n=1){while(n-->0){truck->pre_step(cmd);require(world.Update(1.F/90,4,&temp,&jobs)==EPhysicsUpdateError::None,"native update");truck->post_step();}}
};
// Removing the lift joint or replacing fork contact with cargo parenting fails
// physical height/contact/reaction and the side-miss negative together.
void loaded_motion(){Fixture f;f.tick({},180);const double start=f.kit->body_position(f.cargo).GetY();const auto before=f.truck->state();WarehouseForklift::Command c;c.lift=1;f.tick(c,270);c.lift=0;f.tick(c,45);const auto lifted=f.truck->state();
 std::cout<<"LOADED start="<<start<<" cargo="<<f.kit->body_position(f.cargo).GetY()<<" lift="<<lifted.lift_position_m<<" force="<<lifted.lift_force_n<<" work="<<lifted.hydraulic_positive_work_j<<'\n';
 require(f.contacts.fork_cargo&&f.kit->body_position(f.cargo).GetY()>start+.65,"fork contact carries actual cargo");require(lifted.reservation_excess_j==0,"loaded operation stays inside reserved energy");require(lifted.battery_j<before.battery_j&&lifted.hydraulic_positive_work_j>3500,"loaded lift spends finite source");
 c.tilt=.7F;f.tick(c,120);const auto tilted=f.truck->state();require(tilted.tilt_angle_rad>.04F,"finite hinge physically tilts mast");float normal=0;for(const auto&w:f.truck->wheels()){normal+=w.normal_force_n;require(std::isfinite(w.normal_force_n),"finite normal receipt");}require(normal>30000,"ground receives truck and cargo reaction");require(std::abs(tilted.lift_force_n)<=18005&&std::abs(tilted.tilt_torque_nm)<=14005,"actuator force and torque caps");
 Fixture miss(true,600,false);miss.tick({},180);const double y=miss.kit->body_position(miss.cargo).GetY();miss.tick(c,180);require(!miss.contacts.fork_cargo&&std::abs(miss.kit->body_position(miss.cargo).GetY()-y)<.02,"forks beside cargo cannot lift it");
}
void empty_and_neutral(){Fixture f;f.tick({},90);kit::Kit::Checkpoint bodies;f.kit->capture(bodies);WarehouseForklift::Checkpoint cp;f.truck->capture(cp);cp.state.battery_j=0;f.kit->restore(bodies);f.truck->restore(cp);const double y=f.kit->body_position(f.cargo).GetY();WarehouseForklift::Command c;c.drive=1;c.lift=1;c.tilt=1;c.brake=0;f.tick(c,180);const auto s=f.truck->state();require(s.battery_j==0&&s.applied_drive==0&&s.energy_cutoff,"empty shared battery refuses powered commands");require(std::abs(f.kit->body_position(f.cargo).GetY()-y)<.03,"empty lift holds passively");f.tick({},1);require(f.truck->state().applied_drive==0&&f.truck->state().applied_brake==1,"neutral applies finite brake");}
void airborne(){Fixture f(false,0);f.world.SetGravity(Vec3::sZero());WarehouseForklift::Command c;c.drive=1;c.brake=0;f.tick(c,180);float spin=0;for(const auto&w:f.truck->wheels()){require(!w.contact&&w.normal_force_n==0&&w.longitudinal_force_n==0,"airborne wheels have no traction");spin+=std::abs(w.angular_velocity_rad_s);}require(spin>1&&std::abs(f.kit->body_velocity(f.truck->config().chassis).GetZ())<.001,"driven free wheels cannot propel chassis");}
void checkpoint(){Fixture f;f.tick({},90);WarehouseForklift::Command c;c.drive=.4F;c.steer=.5F;c.brake=0;f.tick(c,90);kit::Kit::Checkpoint bodies;f.kit->capture(bodies);WarehouseForklift::Checkpoint cp;f.truck->capture(cp);const auto wheels=f.truck->wheels();const auto count=f.world.GetConstraints().size();for(int i=0;i<3;++i){f.tick(c,20);f.kit->restore(bodies);f.truck->restore(cp);const auto reset=f.truck->wheels();require(f.world.GetConstraints().size()==count,"retry does not duplicate constraints");require(f.truck->state().battery_j==cp.state.battery_j,"retry restores owner battery");for(int j=0;j<3;++j)require(std::abs(reset[j].angular_velocity_rad_s-wheels[j].angular_velocity_rad_s)<1e-5F,"retry restores native wheel spin");}f.tick({},90);require(std::isfinite(f.kit->body_position(f.truck->config().chassis).GetY()),"restored native vehicle remains coherent");}
void ground_drive_brake(){Fixture f(true,0);f.tick({},180);const auto chassis=f.truck->config().chassis;const auto start=f.kit->body_position(chassis);WarehouseForklift::Command c;c.drive=1;c.steer=.5F;c.brake=0;f.tick(c);require(std::abs(f.truck->state().steering_rad)<=.0078F,"steering changes at finite rate");c.steer=0;f.tick(c,270);const auto moving=f.kit->body_position(chassis);require(moving.GetZ()>start.GetZ()+1,"real wheel contacts drive chassis forward");require(f.kit->body_velocity(chassis).GetZ()>.3F,"drive produces real momentum");
 WarehouseForklift::Command coast;coast.brake=0;f.tick(coast,30);
 require(f.truck->state().applied_drive==0&&f.truck->state().applied_brake==0,"released accelerator and released service brake allow real coasting");
 require(f.kit->body_position(chassis).GetZ()>moving.GetZ()+.1,"unbraked neutral preserves actual forward travel");
 f.tick({},180);require(f.kit->body_velocity(chassis).Length()<.05F,"finite native brakes stop truck");c.drive=-1;f.tick(c,180);require(f.kit->body_position(chassis).GetZ()<moving.GetZ()-.3,"manual reverse drives actual vehicle");}
void overload_and_tip(){Fixture heavy(true,10000);WarehouseForklift::Command c;c.lift=1;heavy.tick(c,270);const auto stalled=heavy.truck->state();require(stalled.lift_position_m<.08F&&std::abs(stalled.lift_force_n)<=18005,"overload stalls against finite lift force");
 Fixture f(true,0);f.tick({},180);const auto chassis=f.truck->config().chassis;const auto point=f.kit->body_center_of_mass_position(chassis)+Vec3(0,1.5F,0);f.world.GetBodyInterface().AddImpulse(f.kit->body_id(chassis),Vec3(8000,0,0),point);float minimum_up=1;for(int i=0;i<180;++i){f.tick();minimum_up=std::min(minimum_up,(f.kit->body_rotation(chassis)*Vec3::sAxisY()).GetY());}std::cout<<"TIP minimum_up="<<minimum_up<<'\n';require(minimum_up<.6F,"finite lateral impact can tip truck without upright rescue");}
void coupling(){Fixture f(true,0);f.tick({},180);const auto tool=f.truck->config().fork_frame;const auto p=f.kit->body_position(tool);const auto velocity=f.kit->body_velocity(tool);require(f.truck->try_detach(),"supported unloaded tool detaches");require((f.kit->body_position(tool)-p).Length()<1e-6F&&(f.kit->body_velocity(tool)-velocity).Length()<1e-6F,"detach preserves solved pose and momentum");require(f.truck->try_couple(),"aligned supported hardware recaptures");require((f.kit->body_position(tool)-p).Length()<1e-6F,"capture does not snap pose");WarehouseForklift::Command c;c.lift=1;f.tick(c,180);require(!f.truck->try_detach(),"suspended attachment cannot detach");}

}
int main(){std::cout<<std::unitbuf;RegisterDefaultAllocator();Factory::sInstance=new Factory;RegisterTypes();loaded_motion();empty_and_neutral();airborne();checkpoint();ground_drive_brake();overload_and_tip();coupling();UnregisterTypes();delete Factory::sInstance;Factory::sInstance=nullptr;std::cout<<"PASS forklift native contact lift tilt finite source airborne and retry\n";}
