// PRIVATE DESIGN EXPERIMENT. Not route implementation or gameplay proof.
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include "sim/mechanism_kit.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <algorithm>
using namespace JPH;
using scraperx::sim::kit::Kit;using scraperx::sim::kit::Part;using scraperx::sim::kit::Material;
struct Broad final:BroadPhaseLayerInterface{uint GetNumBroadPhaseLayers() const override{return 2;}BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer x)const override{return BroadPhaseLayer(x);}const char*GetBroadPhaseLayerName(BroadPhaseLayer x)const override{return x.GetValue()?"MOVING":"STATIC";}};
struct BPFilter final:ObjectVsBroadPhaseLayerFilter{bool ShouldCollide(ObjectLayer x,BroadPhaseLayer y)const override{return x==1||y.GetValue()==1;}};
struct Pair final:ObjectLayerPairFilter{bool ShouldCollide(ObjectLayer x,ObjectLayer y)const override{return x==1||y==1;}};
Part box(Vec3 half,Vec3 at,float mass=0){Part p{half,at,Quat::sIdentity(),Material::Steel};p.mass_kg=mass;return p;}
int main(int argc,char**argv){double h=argc>1?std::atof(argv[1]):1./90;float dm=argc>2?std::atof(argv[2]):360;float mu=argc>3?std::atof(argv[3]):.6;float riderx=argc>4?std::atof(argv[4]):-100;float drop=argc>5?std::atof(argv[5]):.1;
RegisterDefaultAllocator();Factory::sInstance=new Factory;RegisterTypes();
{Broad broad;BPFilter bp;Pair pair;TempAllocatorImpl temp(16*1024*1024);JobSystemThreadPool jobs(cMaxPhysicsJobs,cMaxPhysicsBarriers,1);PhysicsSystem sys;sys.Init(256,0,512,512,broad,bp,pair);sys.SetGravity(Vec3(0,-9.81F,0));Kit kit(sys,0,1);auto&bi=sys.GetBodyInterface();
// Local physical pivot(0,10,0). q=clockwise angle; visible pose is Rz(-q).
std::vector<Part> parts{box({3,.15F,.9F},{3,0,0},700),box({1.9F,.15F,.35F},{-1.9F,0,0},160),box({.6F,.4F,.6F},{-3.8F,0,0},800)};
// Broad cradle of two faces. Local valley(3.8,.15), slope35deg. Total100kg.
for(float side:{-1.F,1.F}){Part face=box({.61F,.15F,.7F},{3.8F+side*.5F,.50F,0},45);face.rotation=Quat::sRotation(Vec3::sAxisZ(),side*.610865F);parts.push_back(face);}
for(float z:{-.85F,.85F})parts.push_back(box({1.1F,.45F,.15F},{3.8F,.55F,z},5));
auto rotor=kit.add_body(2500,parts,{0,10,0},Quat::sRotation(Vec3::sAxisZ(),.18F),1760,mu);kit.set_damping(rotor,0,0);kit.set_continuous_collision(rotor);
auto bearing=kit.add_body(1500,{box({.4F,.4F,1.1F},{0,0,0})},{0,10,0},Quat::sIdentity(),0,.8F);kit.add_hinge(bearing,rotor,{0,10,0},Vec3::sAxisZ(),200);
// Physical upper stop only meets outboard deck side edges; no pose writes.
kit.add_body(1501,{box({.4F,.2F,.18F},{5.5F,1.423F,-.70F}),box({.4F,.2F,.18F},{5.5F,1.423F,.70F})},{0,10,0},Quat::sIdentity(),0,.8F);
// Receivers/catch floor are real colliders and remain enabled.
kit.add_body(1502,{box({1.0F,.3F,1.1F},{7.05F,-.50F,0}),box({.4F,.3F,.18F},{5.6F,-.785F,-.7F}),box({.4F,.3F,.18F},{5.6F,-.785F,.7F}),box({5.8F,.25F,1.5F},{1,-2.3F,0})},{0,10,0},Quat::sIdentity(),0,.8F);
// Free cylinder, localY mapped ontoZ. Settled center predictedlocal(3.8,1.075,0).
Part drum{{.6F,.55F,.6F},Vec3::sZero(),Quat::sIdentity(),Material::Steel};drum.shape=Part::Shape::Cylinder;drum.mass_kg=dm;Vec3 drumlocal(3.8F,1.075F+drop,0);auto d=kit.add_body(2501,{drum},(dm>0?RVec3(0,10,0)+RVec3(Quat::sRotation(Vec3::sAxisZ(),.18F)*drumlocal):RVec3(100,10,0)),Quat::sRotation(Vec3::sAxisX(),JPH_PI*.5F),dm,mu);if(dm>0){kit.set_damping(d,0,0);kit.set_continuous_collision(d);}
BodyID rider;bool hasrider=riderx>-90;if(hasrider){auto p=bi.GetWorldTransform(kit.body_id(rotor))*Vec3(riderx,riderx<0?1.3F:1.05F,0);BodyCreationSettings cfg(new CapsuleShape(.55F,.35F),p,Quat::sIdentity(),EMotionType::Dynamic,1);cfg.mAllowedDOFs=EAllowedDOFs::TranslationX|EAllowedDOFs::TranslationY|EAllowedDOFs::TranslationZ;cfg.mFriction=.6F;cfg.mLinearDamping=0;cfg.mAngularDamping=0;cfg.mAllowSleeping=false;cfg.mOverrideMassProperties=EOverrideMassProperties::CalculateInertia;cfg.mMassPropertiesOverride.mMass=85;cfg.mMotionQuality=EMotionQuality::LinearCast;rider=bi.CreateAndAddBody(cfg,EActivation::Activate);}
// One-sided elastoplastic timber contact: force from measured deck/pad gap.
// Aggregate yielding22000Nm / contactlever5.6 =>3928.57N; no collision doublecount.
const double ystart=10+5.6*std::sin(.06)-.15*std::cos(.06);double front=0,plastic=0,damping=0;const double stiffness=40000,yield=22000/5.6,stroke=.68;double maxw=0,maxreaction=0,minq=-.18,maxq=-.18,kepeak=0;double lastq=-.18;double initial=-1,final=0;double maxresidual=0;double frictionwork=0;
std::cout<<std::setprecision(10);for(int i=0;i<int(12/h);++i){auto point=bi.GetWorldTransform(kit.body_id(rotor))*Vec3(5.6F,-.15F,0);double pen=std::max(0.,ystart-point.GetY());if(pen>0){double next=std::min(stroke,std::max(front,pen-yield/stiffness));plastic+=yield*(next-front);front=next;double vel=-bi.GetPointVelocity(kit.body_id(rotor),point).GetY();double elastic=stiffness*std::max(0.,pen-front);double f=std::max(0.,elastic+300*vel);damping+=std::max(0.,(f-elastic)*vel)*h;maxreaction=std::max(maxreaction,f);bi.AddForce(kit.body_id(rotor),Vec3(0,float(f),0),point);}
// Energy includes all free-bodyKE and gravityPE, elastic pad, plastic anddryhingefriction.
double total=0;for(auto idx:{rotor,d}){total+=kit.body_kinetic_energy(idx)+kit.body_mass(idx)*9.81*kit.body_center_of_mass_position(idx).GetY();}if(hasrider){total+=.5*85*bi.GetLinearVelocity(rider).LengthSq()+85*9.81*bi.GetCenterOfMassPosition(rider).GetY();}double elasticpen=std::max(0.,pen-front);total+=.5*stiffness*elasticpen*elasticpen;if(initial<0)initial=total;final=total;
kit.pre_step(float(h));sys.Update(float(h),4,&temp,&jobs);kit.post_step(float(h));Vec3 axis=bi.GetRotation(kit.body_id(rotor))*Vec3::sAxisX();double q=-std::atan2(axis.GetY(),axis.GetX());double w=std::abs(bi.GetAngularVelocity(kit.body_id(rotor)).GetZ());frictionwork+=200*std::abs(q-lastq);lastq=q;maxw=std::max(maxw,w);minq=std::min(minq,q);maxq=std::max(maxq,q);kepeak=std::max(kepeak,kit.body_kinetic_energy(rotor)+kit.body_kinetic_energy(d));double residual=initial-final-plastic-damping-frictionwork;maxresidual=std::max(maxresidual,std::abs(residual));if(i%std::max(1,int(.1/h))==0){auto dl=bi.GetWorldTransform(kit.body_id(rotor)).Inversed()*bi.GetPosition(kit.body_id(d));std::cout<<"sample "<<i*h<<' '<<q<<' '<<w<<' '<<dl.GetX()<<' '<<dl.GetY()<<' '<<front<<' '<<residual<<'\n';}}
auto dl=bi.GetWorldTransform(kit.body_id(rotor)).Inversed()*bi.GetPosition(kit.body_id(d));double angular=bi.GetAngularVelocity(kit.body_id(rotor)).Length();std::cout<<"RESULT h="<<h<<" mass="<<dm<<" mu="<<mu<<" rider_x="<<riderx<<" drop="<<drop<<" end_q="<<lastq<<" max_w="<<maxw<<" q_min="<<minq<<" q_max="<<maxq<<" drum_local_x="<<dl.GetX()<<" drum_local_y="<<dl.GetY()<<" front="<<front<<" plastic_j="<<plastic<<" damping_j="<<damping<<" dry_friction_j="<<frictionwork<<" max_pad_n="<<maxreaction<<" peak_ke="<<kepeak<<" end_w="<<angular<<" energy_delta="<<initial-final<<" max_residual_j="<<maxresidual;if(hasrider)std::cout<<" rider_end_y="<<bi.GetPosition(rider).GetY();std::cout<<'\n';if(hasrider){bi.RemoveBody(rider);bi.DestroyBody(rider);}}
UnregisterTypes();delete Factory::sInstance;Factory::sInstance=nullptr;}
