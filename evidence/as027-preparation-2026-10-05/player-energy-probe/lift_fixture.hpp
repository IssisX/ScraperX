#pragma once
#include "machine.hpp"
#include <algorithm>
#include <cmath>
#include <Jolt/Physics/PhysicsStepListener.h>
namespace scraperx::sim::insertable {
inline float fixture_direction=0;
inline double fixture_initial_energy=1200000;
struct LiftFixture;
inline LiftFixture* fixture_lift=nullptr;
struct LiftFixture {
 struct State {double energy=fixture_initial_energy,work=0;float target=0,direction=0;bool cutoff=false;};
 State capture() const{return {energy,work,target,fixture_direction,cutoff};}
 void restore(State s){energy=s.energy;work=s.work;target=s.target;fixture_direction=s.direction;cutoff=s.cutoff;pending=false;energized=false;m.restore_control(m.capture_control());drive->SetMotorState(EMotorState::Off);drive->SetMaxFrictionForce(220000);last=drive->GetCurrentPosition();}
 Machine m;Ref<SliderConstraint> drive;BodyIndex foot,deck;float target=0;double last=0,work=0,peak_force=0;
 double energy=fixture_initial_energy,peak_electrical=0,overdraft=0;float substep_dt=0;bool pending=false,energized=false,cutoff=false;
 LiftFixture(kit::Kit&kit,PhysicsSystem&world):m({kit,world,{{-31,108.732514f,-153},JPH_PI/2},{1970,2},{2970,9}},"probe.real_player",2,9){
 const float a=DegreesToRadians(15.f),L=18,w=L*cos(a),h=L*sin(a),stroke=w-L*cos(DegreesToRadians(65.f));
 const auto base=m.body("base",{box(Vec3(9.4,.5,2.2),Material::Concrete)},Vec3(9,1,0),0);
 const auto guide=m.body("guide",{box(Vec3(.3,17,.3),Material::Steel)},Vec3(-1,18,2.4),0);
 foot=m.body("foot",{box(Vec3(.5,.25,.7))},Vec3(w,2,0),160);
 const auto mid=m.body("mid",{box(Vec3(9.2,.2,.2),Material::Steel,Vec3(0,0,-1.6)),box(Vec3(9.2,.2,.2),Material::Steel,Vec3(0,0,1.6))},Vec3(9,2+h+.65,0),300);
 const auto center=m.body("center",{box(Vec3(.5,.2,.7))},Vec3(w,2+h,0),160);
 deck=m.body("deck",{box(Vec3(9.2,.3,1.8),Material::Timber)},Vec3(9,2+2*h+.65,0),800);
 const auto top=m.body("top",{box(Vec3(.5,.2,.7))},Vec3(w,2+2*h,0),160);
 drive=m.slider(base,foot,Vec3::sAxisX(),-stroke,.01);
 m.slider(guide,mid,Vec3::sAxisY(),-.02,12);
 m.slider(guide,deck,Vec3::sAxisY(),-.02,24);
 m.slider(mid,center,Vec3::sAxisX(),-stroke,.01);
 m.slider(deck,top,Vec3::sAxisX(),-stroke,.01);
 auto hinge=[&](BodyIndex b1,BodyIndex b2,Vec3 p){m.hinge(b1,b2,p);m.no_collision(b1,b2);};
 for(int stage=0;stage<2;++stage){
   float y=2+stage*h;
   auto up=m.body("up",{box(Vec3(9,.18,.2),Material::Rust,Vec3(0,0,-.65)),box(Vec3(9,.18,.2),Material::Rust,Vec3(0,0,.65))},Vec3(w/2,y+h/2,0),600,a);
   auto down=m.body("down",{box(Vec3(9,.18,.2),Material::Steel,Vec3(0,0,-1.1)),box(Vec3(9,.18,.2),Material::Steel,Vec3(0,0,1.1))},Vec3(w/2,y+h/2,0),600,-a);
   hinge(stage?mid:base,up,Vec3(0,y,0));hinge(stage?center:foot,down,Vec3(w,y,0));
   hinge(up,down,Vec3(w/2,y+h/2,0));hinge(stage?deck:mid,down,Vec3(0,y+h,0));hinge(stage?top:center,up,Vec3(w,y+h,0));
 }

 fixture_lift=this;last=drive->GetCurrentPosition();
 }
 ~LiftFixture(){fixture_lift=nullptr;}
 void pre(float){if(fixture_direction!=0&&!cutoff&&energy>1)m.context.world.GetBodyInterface().ActivateConstraint(drive);}
 void account(){
 if(!pending)return;
 if(energized){
  const double f=drive->GetTotalLambdaMotor()/substep_dt;
  const double positive=std::max(0.,f*(drive->GetCurrentPosition()-last));
  work+=positive;peak_force=std::max(peak_force,std::abs(f));
  const double debit=positive/.8;
  overdraft=std::max(overdraft,debit-energy);energy-=debit;
  peak_electrical=std::max(peak_electrical,debit/substep_dt);
 }
 pending=false;
 }
 void collision_step(const PhysicsStepListenerContext& c) {
 account();const float dt=c.mDeltaTime;
 energized=fixture_direction!=0&&!cutoff&&energy>1;
 const float wanted=energized?fixture_direction*.18f:0;
 target=energized?std::clamp(wanted,target-.11f*dt,target+.11f*dt):0;
 const double velocity=std::abs(m.native(foot).GetLinearVelocity().Dot(m.direction(Vec3::sAxisX())));
 const double cap=std::min(200000.,36000./std::max({velocity,double(std::abs(target)),.01}));
 const double reserve=cap*(std::max(velocity,double(std::abs(target)))+.02)*dt/.8;
 if(energized&&energy<reserve){cutoff=true;energized=false;target=0;}
 drive->GetMotorSettings().SetForceLimit(cap);
 drive->SetMaxFrictionForce(energized?0:220000);
 drive->SetMotorState(energized?EMotorState::Velocity:EMotorState::Off);
 drive->SetTargetVelocity(target);
 last=drive->GetCurrentPosition();substep_dt=dt;pending=true;
 }
 void post(float){account();}
};
}
