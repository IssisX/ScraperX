#include "sim/line_material_hand_constraint.hpp"
#include <Jolt/Physics/Body/BodyManager.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/IslandBuilder.h>
#include <Jolt/Physics/LargeIslandSplitter.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace scraperx::sim {
namespace {
template<class V> bool finite(const V &v) {
    return std::isfinite(v.GetX()) && std::isfinite(v.GetY()) && std::isfinite(v.GetZ());
}
JPH::Vec3 axis(unsigned i) { return i==0 ? JPH::Vec3::sAxisX() : i==1 ? JPH::Vec3::sAxisY() : JPH::Vec3::sAxisZ(); }
constexpr float axis_force=866.025390625F;
}
LineMaterialHandConstraint::LineMaterialHandConstraint() : Constraint(JPH::DistanceConstraintSettings()) {
    SetNumVelocityStepsOverride(40);SetNumPositionStepsOverride(8);
}
void LineMaterialHandConstraint::invalidate() { valid_=false;count_=0;parts_={};ResetWarmStart(); }
bool LineMaterialHandConstraint::set_geometry(JPH::Body &player,const LineMaterialSample &sample,JPH::Vec3 offset) {
    invalidate();
    if(!player.IsDynamic() || !player.IsRigidBody() || player.IsSensor() || !player.IsInBroadPhase() ||
       !finite(sample.world_point) || !finite(sample.point_velocity) || !finite(offset)) return false;
    const auto translation=JPH::EAllowedDOFs::TranslationX|JPH::EAllowedDOFs::TranslationY|JPH::EAllowedDOFs::TranslationZ;
    if(player.GetMotionProperties()->GetAllowedDOFs()!=translation) return false;
    std::array<Participant,3> combined{};unsigned used=1;combined[0]={&player,1,JPH::Vec3::sZero()};
    double weights=0;
    for(const auto &endpoint:sample.endpoints) {
        if(!std::isfinite(endpoint.weight) || endpoint.weight<0 || !finite(endpoint.shape_local_point)) return false;
        if(endpoint.weight==0) continue;
        auto *body=endpoint.body;
        if(!body || body==&player || !body->IsRigidBody() || body->IsSensor() || !body->IsInBroadPhase()) return false;
        const auto point=body->GetWorldTransform()*endpoint.shape_local_point;
        const JPH::Vec3 arm(point-body->GetCenterOfMassPosition());
        if(!finite(arm)) return false;
        unsigned i=1;while(i<used && combined[i].body!=body) ++i;
        if(i==used) combined[used++].body=body;
        combined[i].weight-=endpoint.weight;
        combined[i].arm-=endpoint.weight*arm;
        weights+=endpoint.weight;
    }
    if(std::abs(weights-1)>1.0e-5) return false;
    error_=JPH::Vec3(player.GetCenterOfMassPosition()-sample.world_point)-offset;
    if(!finite(error_)) return false;
    parts_=combined;count_=used;valid_=true;return true;
}
bool LineMaterialHandConstraint::IsActive() const {
    if(!GetEnabled() || !valid_) return false;
    for(unsigned i=0;i<count_;++i) if(parts_[i].body->IsActive()) return true;
    return false;
}
void LineMaterialHandConstraint::ResetWarmStart() {
    lambda_={};mass_={};bias_={};gamma_=cap_=0;dt_=0;solving_=false;
}
void LineMaterialHandConstraint::SetupVelocityConstraint(float h) {
    ResetWarmStart();
    if(!GetEnabled() || !valid_ || !std::isfinite(h) || h<=0) return;
    dt_=h;const double d=180.0+5000.0*double(h);gamma_=1/(double(h)*d);
    float cap=axis_force*h;if(double(cap)/h>axis_force)cap=std::nextafter(cap,0.0F);cap_=cap;
    for(unsigned a=0;a<3;++a) {
        const auto n=axis(a);double inverse=0;
        for(unsigned i=0;i<count_;++i) {
            const auto &p=parts_[i];if(!p.body->IsDynamic())continue;
            const auto *motion=p.body->GetMotionPropertiesUnchecked();const auto angular=p.arm.Cross(n);
            inverse+=double(motion->GetInverseMass())*p.weight*p.weight+
                angular.Dot(motion->MultiplyWorldSpaceInverseInertiaByVector(p.body->GetRotation(),angular));
        }
        mass_[a]=1/(inverse+gamma_);bias_[a]=5000.0*error_[a]/d;
    }
    solving_=true;
}
bool LineMaterialHandConstraint::SolveVelocityConstraint(float) {
    if(!solving_)return false;
    bool changed=false;
    for(unsigned a=0;a<3;++a) {
        const auto n=axis(a);double speed=0;
        for(unsigned i=0;i<count_;++i) {
            const auto &p=parts_[i];if(p.body->IsStatic())continue;
            speed+=p.weight*n.Dot(p.body->GetLinearVelocity())+p.arm.Cross(n).Dot(p.body->GetAngularVelocity());
        }
        const double next=std::clamp(lambda_[a]-(speed+bias_[a]+gamma_*lambda_[a])*mass_[a],-cap_,cap_);
        const float impulse=float(next-lambda_[a]);lambda_[a]=next;
        if(impulse==0)continue;
        for(unsigned i=0;i<count_;++i) {
            const auto &p=parts_[i];if(!p.body->IsDynamic())continue;
            p.body->AddImpulse(n*(impulse*p.weight));p.body->AddAngularImpulse(p.arm.Cross(n)*impulse);
        }
        changed=true;
    }
    return changed;
}
JPH::Vec3 LineMaterialHandConstraint::player_force() const {
    return solving_ && GetEnabled() && valid_ && dt_>0 ?
        JPH::Vec3(float(lambda_[0]/dt_),float(lambda_[1]/dt_),float(lambda_[2]/dt_)) : JPH::Vec3::sZero();
}
void LineMaterialHandConstraint::BuildIslands(JPH::uint32 index,JPH::IslandBuilder &builder,JPH::BodyManager &manager) {
    std::array<JPH::BodyID,3> ids{};unsigned n=0;
    for(unsigned i=0;i<count_;++i)if(parts_[i].body->IsDynamic())ids[n++]=parts_[i].body->GetID();
    if(n==0)return;
    manager.ActivateBodies(ids.data(),int(n));const auto first=manager.GetBody(ids[0]).GetIndexInActiveBodiesInternal();
    for(unsigned i=1;i<n;++i)builder.LinkBodies(first,manager.GetBody(ids[i]).GetIndexInActiveBodiesInternal());
    builder.LinkConstraint(index,first);
}
JPH::uint LineMaterialHandConstraint::BuildIslandSplits(JPH::LargeIslandSplitter &splitter) const {
    for(unsigned i=0;i<count_;++i)if(parts_[i].body->IsDynamic())splitter.AssignToNonParallelSplit(parts_[i].body);
    return JPH::LargeIslandSplitter::cNonParallelSplitIdx;
}
void LineMaterialHandConstraint::SaveState(JPH::StateRecorder &) const {
    throw std::logic_error("LineMaterialHandConstraint: remove before snapshot; rebuild cold from material owner state");
}
void LineMaterialHandConstraint::RestoreState(JPH::StateRecorder &) {
    throw std::logic_error("LineMaterialHandConstraint: rebuild cold after body restore");
}
}
