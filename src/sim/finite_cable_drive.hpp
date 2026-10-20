#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
namespace scraperx::sim {
// Finite source and speed/force limits for total cable path travel (SI units).
// Attachment geometry and the single tensile row remain in their cable owner.
struct FiniteCableDriveConfig {
 float min_length_m = .1F, max_length_m = 100;
 float max_speed_mps = .35F, pull_force_n = 50000, brake_force_n = 50000;
 double electrical_power_w = 25000, efficiency = .8, capacity_j = 5e6;
};
struct FiniteCableDriveState {
 float target_speed_mps = 0;
 double energy_j = 0, source_energy_used_j = 0, positive_motor_work_j = 0;
 double absorbed_work_j = 0, heat_j = 0, command_work_bound_j = 0;
 double electrical_power_w = 0, peak_electrical_w = 0, energy_overdraft_j = 0;
 bool energy_cutoff = false, braking = true, at_min = false, at_max = false;
};
namespace finite_cable_drive {
template<class Config> bool valid_config(const Config &c,float length) {
 return c.min_length_m>0&&length>=c.min_length_m&&c.max_length_m>=length&&std::isfinite(c.max_length_m)&&
  c.max_speed_mps>0&&std::isfinite(c.max_speed_mps)&&c.pull_force_n>0&&std::isfinite(c.pull_force_n)&&c.brake_force_n>0&&std::isfinite(c.brake_force_n)&&
  c.electrical_power_w>0&&std::isfinite(c.electrical_power_w)&&c.efficiency>0&&c.efficiency<=1&&c.capacity_j>=0&&std::isfinite(c.capacity_j);
}
template<class Config,class State> bool valid_state(const Config &c,const State &s,float length) {
 return std::isfinite(s.energy_j)&&s.energy_j>=0&&s.energy_j<=c.capacity_j&&std::isfinite(length)&&length>=c.min_length_m&&length<=c.max_length_m&&
  std::isfinite(s.source_energy_used_j)&&s.source_energy_used_j>=0&&std::isfinite(s.positive_motor_work_j)&&s.positive_motor_work_j>=0&&s.positive_motor_work_j<=s.command_work_bound_j+1e-6&&
  std::isfinite(s.absorbed_work_j)&&s.absorbed_work_j>=0&&std::isfinite(s.command_work_bound_j)&&s.command_work_bound_j>=0;
}
// One reservation per collision substep; every signed lambda correction shares
// its positive-event-work budget. Dissipation cannot refill this reservation.
struct Step {
 double step_work_budget = 0, step_positive_work = 0;
 float dt = 0;
 bool motor = false;
 void reset(){dt=0;step_work_budget=step_positive_work=0;motor=false;}
 template<class Config,class State> void begin(const Config &config,State &state,float &length,float h,float effort) {
  reset();dt=h;state.electrical_power_w=0;
  state.target_speed_mps=0;state.braking=true;
  if(!std::isfinite(h)||h<=0) return;
  state.energy_cutoff=state.energy_j<=0;
  const float direction=effort>0?-1.F:effort<0?1.F:0.F;
  double travel=std::abs(effort)*config.max_speed_mps*h;
  if(direction<0) {
   travel=std::min(travel,double(length-config.min_length_m));
   travel=std::min(travel,std::min(config.electrical_power_w*h,state.energy_j)*config.efficiency/config.pull_force_n);
   // Round the target toward the current length: never exceed the admitted
   // source/power reservation through float target representation.
   float next=static_cast<float>(length-travel);
   while(double(length)-next>travel) next=std::nextafter(next,length);
   travel=double(length)-next;
   if(travel>0) {
    const double mechanical=config.pull_force_n*travel;
    const double source=mechanical/config.efficiency;
    state.energy_j-=source;state.source_energy_used_j+=source;
    state.command_work_bound_j+=mechanical;step_work_budget=mechanical;state.electrical_power_w=source/h;
    state.peak_electrical_w=std::max(state.peak_electrical_w,state.electrical_power_w);
    length=next;state.target_speed_mps=-float(travel/h);motor=true;state.braking=false;
   }
  } else if(direction>0&&!state.energy_cutoff) {
   travel=std::min(travel,double(config.max_length_m-length));
   const float old=length;
   float next=static_cast<float>(old+travel);
   while(double(next)-old>travel)next=std::nextafter(next,old);
   length=next;state.target_speed_mps=(next-old)/h;
  }
  state.at_min=length<=config.min_length_m;
  state.at_max=length>=config.max_length_m;
  state.energy_cutoff=state.energy_j<=0;
  state.heat_j=state.source_energy_used_j-state.positive_motor_work_j+state.absorbed_work_j;
 }
 template<class Config,class State> float project(const Config &config,State &state,float lambda,double u,double w,float rating_n=std::numeric_limits<float>::max()) {
  const double force_cap=std::min(motor?config.pull_force_n:config.brake_force_n,rating_n)*dt;
  float maximum=float(force_cap);while(double(maximum)>force_cap)maximum=std::nextafter(maximum,0.F);
  const double wanted=std::clamp(double(lambda)+(u-state.target_speed_mps)/w,0.0,double(maximum));
  float next=std::clamp(float(wanted),0.F,maximum);
  const double remaining=std::max(0.0,step_work_budget-step_positive_work);
  const auto positive_work=[&](float delta) {
   const double along_force=delta>=0?-u:u;
   const double after=along_force+w*std::abs(double(delta));
   return std::max(0.0,(std::pow(std::max(0.0,after),2)-std::pow(std::max(0.0,along_force),2))/(2*w));
  };
  float delta=next-lambda;
  if(positive_work(delta)>remaining) {
   const double along_force=delta>=0?-u:u;
   const double root=std::sqrt(std::max(0.0,along_force)*std::max(0.0,along_force)+2*w*remaining);
   const double allowed=along_force>=0?(remaining>0?2*remaining/(root+along_force):0):(-along_force+root)/w;
   next=std::clamp(float(lambda+std::copysign(allowed,double(delta))),0.F,maximum);
   delta=next-lambda;
   if(positive_work(delta)>remaining) {
    // Subtracting two float impulses can keep the same correction across
    // millions of nextafter(next, lambda) values near zero. Search the ordered
    // nonnegative float representations instead. Current lambda is safe;
    // the candidate is unsafe. Every probe uses the actual float correction
    // applied by both cable owners, preserving the same event-work bound.
    static_assert(sizeof(float)==sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559,
        "finite cable projection requires IEEE 754 binary32 impulses");
    const auto bits=[](float value){std::uint32_t result;std::memcpy(&result,&value,sizeof(result));return result;};
    const auto value=[](std::uint32_t representation){float result;std::memcpy(&result,&representation,sizeof(result));return result;};
    auto safe=bits(lambda),unsafe=bits(next);
    while((safe>unsafe?safe-unsafe:unsafe-safe)>1) {
     const auto low=std::min(safe,unsafe),high=std::max(safe,unsafe);
     const auto midpoint=low+(high-low)/2;
     if(positive_work(value(midpoint)-lambda)<=remaining)safe=midpoint;
     else unsafe=midpoint;
    }
    next=value(safe);delta=next-lambda;
   }
  }
  if(delta==0)return lambda;
  const double positive=positive_work(delta);
  const double kinetic_change=-u*delta+.5*w*delta*delta;
  const double absorbed=std::max(0.0,positive-kinetic_change);
  step_positive_work+=positive;state.positive_motor_work_j+=positive;state.absorbed_work_j+=absorbed;
  state.heat_j=state.source_energy_used_j-state.positive_motor_work_j+state.absorbed_work_j;

  return next;
 }
};
} // namespace finite_cable_drive
} // namespace scraperx::sim
