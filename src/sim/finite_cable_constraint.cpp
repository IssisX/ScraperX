#include "sim/finite_cable_constraint.hpp"
#include <Jolt/Physics/Body/BodyManager.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/IslandBuilder.h>
#include <Jolt/Physics/LargeIslandSplitter.h>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace scraperx::sim {
namespace {
bool finite(JPH::Vec3Arg v) { return std::isfinite(v.GetX()) && std::isfinite(v.GetY()) && std::isfinite(v.GetZ()); }
}
FiniteCableConstraint::FiniteCableConstraint(float rest_length_m)
    : FiniteCableConstraint(rest_length_m, GrappleRopeSettings::stiffness_n_per_m,
                            GrappleRopeSettings::damping_ns_per_m) {}
FiniteCableConstraint::FiniteCableConstraint(float rest_length_m, float stiffness, float damping)
    : Constraint(JPH::DistanceConstraintSettings()),rest_(rest_length_m), stiffness_(stiffness), damping_(damping) {
    if(!std::isfinite(rest_) || rest_<=0) throw std::invalid_argument("FiniteCableConstraint: rest length must be finite and positive");
    if(!std::isfinite(stiffness_) || stiffness_<=0 || !std::isfinite(damping_) || damping_<0)
        throw std::invalid_argument("FiniteCableConstraint: expected positive stiffness N/m and nonnegative damping Ns/m");
}
bool FiniteCableConstraint::set_rest(float value) {
    if(!std::isfinite(value) || value<=0) return false;
    rest_=value; return true;
}
FiniteCableConstraint::Admission FiniteCableConstraint::set_geometry(const Participant *p,std::size_t n,double length) {
    geometry_valid_=false; count_=0; length_=0; ResetWarmStart();
    if(!p || n==0 || !std::isfinite(length) || length<=0) return Admission::InvalidGeometry;
    std::array<Participant,capacity> combined{}; std::size_t used=0; bool dynamic=false;
    for(std::size_t j=0;j<n;++j) {
        if(!p[j].body || !p[j].body->IsRigidBody() || p[j].body->IsSensor() ||
           p[j].body->GetID().IsInvalid() || !finite(p[j].linear) || !finite(p[j].angular)) return Admission::InvalidGeometry;
        std::size_t i=0; while(i<used && combined[i].body!=p[j].body) ++i;
        if(i==used) {
            if(used==capacity) return Admission::CapacityExceeded;
            combined[used++].body=p[j].body;
        }
        combined[i].linear+=p[j].linear; combined[i].angular+=p[j].angular;
        if(!finite(combined[i].linear) || !finite(combined[i].angular)) return Admission::InvalidGeometry;
        dynamic|=p[j].body->IsDynamic();
    }
    if(!dynamic) return Admission::NoDynamicBody;
    parts_=combined; count_=used; length_=length; geometry_valid_=true;
    for(std::size_t i=0;i<count_;++i) reactions_[i]={parts_[i].body->GetID(),JPH::Vec3::sZero(),JPH::Vec3::sZero()};
    return Admission::Accepted;
}
void FiniteCableConstraint::NotifyShapeChanged(const JPH::BodyID &,JPH::Vec3Arg) {
    geometry_valid_=false; ResetWarmStart();
}
bool FiniteCableConstraint::IsActive() const {
    if(!GetEnabled() || !geometry_valid_ || broken_) return false;
    for(std::size_t i=0;i<count_;++i) if(parts_[i].body->IsActive()) return true;
    return false;
}
void FiniteCableConstraint::ResetWarmStart() {
    lambda_=gamma_=bias_=effective_mass_=cap_=0; dt_=0; solving_=false;
    for(auto &r:reactions_) { r.force_n=JPH::Vec3::sZero(); r.torque_nm=JPH::Vec3::sZero(); }
}
double FiniteCableConstraint::inverse_mass() const {
    double w=0;
    for(std::size_t i=0;i<count_;++i) {
        const auto &p=parts_[i]; if(!p.body->IsDynamic()) continue;
        const auto *motion=p.body->GetMotionPropertiesUnchecked();
        w+=double(motion->GetInverseMass())*p.linear.LengthSq()+
            p.angular.Dot(motion->MultiplyWorldSpaceInverseInertiaByVector(p.body->GetRotation(),p.angular));
    }
    return w;
}
double FiniteCableConstraint::speed() const {
    double value=0;
    for(std::size_t i=0;i<count_;++i) {
        const auto &p=parts_[i]; if(p.body->IsStatic()) continue;
        value+=p.linear.Dot(p.body->GetLinearVelocity())+p.angular.Dot(p.body->GetAngularVelocity());
    }
    return value;
}
double FiniteCableConstraint::prospective_tension(float rest,float h) const {
    if(!geometry_valid_ || !std::isfinite(rest) || rest<=0 || !std::isfinite(h) || h<=0)
        return std::numeric_limits<double>::quiet_NaN();
    if(length_<rest) return 0;
    const double d=damping_+double(h)*stiffness_;
    const double demand=(stiffness_*(length_-rest)+d*speed())/(1+double(h)*d*inverse_mass());
    return std::isfinite(demand) ? std::max(0.0,demand):demand;
}
void FiniteCableConstraint::SetupVelocityConstraint(float h) {
    ResetWarmStart(); dt_=h;
    if(!GetEnabled() || !geometry_valid_ || broken_ || !std::isfinite(h) || h<=0 || length_<rest_) return;
    const double extension=length_-rest_;
    const double demand=prospective_tension(rest_,h);
    if(!std::isfinite(demand) || extension>GrappleRopeSettings::strength_n/stiffness_ ||
       demand>GrappleRopeSettings::strength_n) { broken_=true; return; }
    const double d=damping_+double(h)*stiffness_;
    gamma_=1/(double(h)*d); bias_=stiffness_*extension/d;
    effective_mass_=1/(inverse_mass()+gamma_);
    const float limit=drive_limited_ ? GrappleRopeSettings::reel_force_n:GrappleRopeSettings::strength_n;
    float cap=limit*h;
    if(double(cap)/h>limit) cap=std::nextafter(cap,0.0F);
    cap_=cap; solving_=true;
}
void FiniteCableConstraint::update_reactions() {
    const float load=tension();
    for(std::size_t i=0;i<count_;++i) {
        reactions_[i].force_n=-load*parts_[i].linear;
        reactions_[i].torque_nm=-load*parts_[i].angular;
    }
}
float FiniteCableConstraint::tension() const { return solving_ && dt_>0 ? float(lambda_/dt_):0; }
bool FiniteCableConstraint::SolveVelocityConstraint(float) {
    if(!solving_) return false;
    const double next=std::clamp(lambda_+(speed()+bias_-gamma_*lambda_)*effective_mass_,0.0,cap_);
    const float delta=float(next-lambda_);
    lambda_=next;
    if(delta==0) return false;
    for(std::size_t i=0;i<count_;++i) {
        const auto &p=parts_[i]; if(!p.body->IsDynamic()) continue;
        p.body->AddImpulse(-delta*p.linear); p.body->AddAngularImpulse(-delta*p.angular);
    }
    update_reactions(); return true;
}
void FiniteCableConstraint::BuildIslands(JPH::uint32 index,JPH::IslandBuilder &builder,JPH::BodyManager &manager) {
    std::array<JPH::BodyID,capacity> ids{}; unsigned n=0;
    for(std::size_t i=0;i<count_;++i) if(parts_[i].body->IsDynamic()) ids[n++]=parts_[i].body->GetID();
    if(n==0) return;
    manager.ActivateBodies(ids.data(),int(n));
    const auto first=manager.GetBody(ids[0]).GetIndexInActiveBodiesInternal();
    for(unsigned i=1;i<n;++i) builder.LinkBodies(first,manager.GetBody(ids[i]).GetIndexInActiveBodiesInternal());
    builder.LinkConstraint(index,first);
}
JPH::uint FiniteCableConstraint::BuildIslandSplits(JPH::LargeIslandSplitter &splitter) const {
    for(std::size_t i=0;i<count_;++i) if(parts_[i].body->IsDynamic()) splitter.AssignToNonParallelSplit(parts_[i].body);
    return JPH::LargeIslandSplitter::cNonParallelSplitIdx;
}
void FiniteCableConstraint::SaveState(JPH::StateRecorder &) const {
    throw std::logic_error("FiniteCableConstraint: remove before snapshot; rebuild cold from cable owner state");
}
void FiniteCableConstraint::RestoreState(JPH::StateRecorder &) {
    throw std::logic_error("FiniteCableConstraint: rebuild cold after body restore");
}
} // namespace scraperx::sim
