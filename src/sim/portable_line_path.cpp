#include "sim/portable_line_path.hpp"
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/ScaleHelpers.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace scraperx::sim {
namespace {
using namespace JPH;
using Path=PortableLinePath;
template<class V> bool finite(const V &v) { return std::isfinite(v.GetX()) && std::isfinite(v.GetY()) && std::isfinite(v.GetZ()); }
struct Filter final:BodyFilter {
    BodyID player,fitting;
    bool ShouldCollide(const BodyID &id) const override { return id!=player && id!=fitting; }
    bool ShouldCollideLocked(const Body &body) const override {
        // Empty material point masses have no collision surface. Their
        // zero-volume broadphase bounds must not become rope obstructions.
        return body.IsRigidBody() && !body.IsSensor() && body.GetShape()->GetType()!=EShapeType::Empty;
    }
};
struct Hits final:CastRayCollector {
    std::array<RayCastResult,Path::query_capacity> hits{};
    unsigned count=0; bool overflow=false;
    void AddHit(const RayCastResult &hit) override {
        if(hit.mFraction<0 || hit.mFraction>1) return;
        for(unsigned i=0;i<count;++i) if(hits[i].mBodyID==hit.mBodyID && hits[i].mSubShapeID2==hit.mSubShapeID2) return;
        if(count==hits.size()) { overflow=true; ForceEarlyOut(); return; }
        unsigned i=count++;
        while(i>0 && (hits[i-1].mFraction>hit.mFraction ||
              (hits[i-1].mFraction==hit.mFraction && hits[i-1].mBodyID.GetIndexAndSequenceNumber()>hit.mBodyID.GetIndexAndSequenceNumber()))) {
            hits[i]=hits[i-1]; --i;
        }
        hits[i]=hit;
    }
};
Hits cast(PhysicsSystem &world,const Filter &filter,RVec3Arg a,RVec3Arg b) {
    Hits hits;
    if(Vec3(b-a).LengthSq()>0) world.GetNarrowPhaseQueryNoLock().CastRay(RRayCast(a,Vec3(b-a)),RayCastSettings(),hits,{},{},filter);
    return hits;
}
bool same(const Path::Contact &contact,const RayCastResult &hit) { return contact.body==hit.mBodyID && contact.subshape==hit.mSubShapeID2; }
Path::Reason leaf(Body &body,SubShapeID subshape,TransformedShape &out) {
    SubShapeID remainder;
    // Validate the full leaf ID before indexing a compound child.
    if(!body.GetShape()->GetLeafShape(subshape,remainder)) return Path::Reason::InvalidLeaf;
    out=body.GetTransformedShape();
    for(unsigned depth=0;depth<16;++depth) {
        if(!out.mShape) return Path::Reason::InvalidLeaf;
        const auto type=out.mShape->GetSubType();
        if(type==EShapeSubType::Box) {
            const Vec3 scale=out.GetShapeScale();
            return finite(scale) && scale.Abs().ReduceMin()>0 ? Path::Reason::None:Path::Reason::InvalidLeaf;
        }
        if(type!=EShapeSubType::StaticCompound && type!=EShapeSubType::RotatedTranslated &&
           type!=EShapeSubType::OffsetCenterOfMass && type!=EShapeSubType::Scaled) return Path::Reason::UnsupportedShape;
        SubShapeID next;
        auto child=out.GetSubShapeTransformedShape(subshape,next);
        out=child; subshape=next;
    }
    return Path::Reason::InvalidLeaf;
}
RoundedBoxLineResult geometry(const TransformedShape &ts,RVec3Arg a,RVec3Arg b,RoundedBoxLineTopology retained={}) {
    const auto *box=static_cast<const BoxShape *>(ts.mShape.GetPtr());
    RoundedBoxLineQuery query;
    query.leaf_to_world=ts.GetCenterOfMassTransform();
    query.half_extent=box->GetHalfExtent()*ts.GetShapeScale().Abs();
    // This is the pinned Jolt Box support's actual reduced-core radius, which
    // can differ from the stored radius even at unit scale.
    query.convex_radius=ScaleHelpers::ScaleConvexRadius(box->GetConvexRadius(),ts.GetShapeScale());
    query.endpoint_a=a; query.endpoint_b=b; query.retained=retained;
    return rounded_box_line_path(query);
}
void release(Path::State &state) { if(state.has_contact) { state.has_contact=false; state.contact={}; ++state.token; } }
Path::Result failure(Path::Reason reason,const Path::State &state,RoundedBoxLineStatus status=RoundedBoxLineStatus::Invalid) {
    Path::Result result; result.reason=reason; result.has_contact=state.has_contact; result.contact=state.contact;
    result.topology_token=state.token; result.geometry_status=status; return result;
}
Path::Reason leg_clear(PhysicsSystem &world,const Filter &filter,const Path::Contact &own,RVec3Arg a,RVec3Arg b) {
    const auto hits=cast(world,filter,a,b);
    if(hits.overflow) return Path::Reason::CapacityExceeded;
    for(unsigned i=0;i<hits.count;++i) {
        const auto &hit=hits.hits[i]; if(same(own,hit)) continue;
        auto *body=world.GetBodyLockInterfaceNoLock().TryGetBody(hit.mBodyID);
        if(!body || !body->IsInBroadPhase()) return Path::Reason::ContactChanged;
        TransformedShape ts; const auto reason=leaf(*body,hit.mSubShapeID2,ts);
        if(reason!=Path::Reason::None) return reason;
        if(geometry(ts,a,b).status!=RoundedBoxLineStatus::Clear) return Path::Reason::AdditionalContact;
    }
    return Path::Reason::None;
}
// Boundary samples are used only to build a conservative search envelope.
// Between samples the exact path is at most half the true arclength interval
// from an endpoint, so expanding by that distance cannot miss an intervening
// arc/corner. Overlap is explicitly unsupported, not proof of a collision.
struct BoundaryCheck final:TransformedShapeCollector {
    AABox bounds; const Path::Contact &own; unsigned count=0; bool overlap=false,overflow=false;
    BoundaryCheck(const AABox &box,const Path::Contact &contact):bounds(box),own(contact) {}
    void AddHit(const TransformedShape &ts) override {
        if(ts.mBodyID==own.body && ts.mSubShapeIDCreator.GetID()==own.subshape) return;
        if(!bounds.Overlaps(ts.GetWorldSpaceBounds())) return;
        if(++count>Path::query_capacity) { overflow=true; ForceEarlyOut(); return; }
        overlap=true;
    }
};
Path::Reason boundary_clear(PhysicsSystem &world,const Filter &filter,const Path::Contact &own,const RoundedBoxLineResult &path) {
    if(path.surface_point_count==0) return Path::Reason::UnsupportedGeometry;
    AABox box;
    for(std::uint32_t i=0;i<path.surface_point_count;++i) box.Encapsulate(Vec3(path.surface_points[i]));
    const double gap=path.surface_point_count>1 ? path.boundary_length_m*path.length_m/path.cross_section_length_m/
        (2*(path.surface_point_count-1)):0;
    const double coordinate=std::max(Vec3(box.mMin).Abs().ReduceMax(),Vec3(box.mMax).Abs().ReduceMax());
    box.ExpandBy(Vec3::sReplicate(float(gap+8*std::numeric_limits<float>::epsilon()*(1+coordinate))));
    BoundaryCheck collector(box,own);
    world.GetNarrowPhaseQueryNoLock().CollectTransformedShapes(box,collector,{},{},filter);
    return collector.overflow ? Path::Reason::CapacityExceeded:
        collector.overlap ? Path::Reason::NearbyBoundaryGeometry:Path::Reason::None;
}
} // namespace

PortableLinePath::Result PortableLinePath::evaluate_world(JPH::PhysicsSystem &world,
        JPH::Body *fitting,JPH::Body *anchor,JPH::RVec3Arg a,JPH::RVec3Arg b,
        JPH::BodyID player,JPH::BodyID ignored,State &state) {
    using namespace JPH;
    if(!finite(a) || !finite(b) || Vec3(b-a).LengthSq()<1e-12F) return failure(Reason::InvalidInput,state);
    Filter filter; filter.player=player; filter.fitting=ignored;
    RoundedBoxLineResult path;
    Body *obstacle=nullptr;
    Contact selected;
    bool wrapped=false;
    if(state.has_contact) {
        obstacle=world.GetBodyLockInterfaceNoLock().TryGetBody(state.contact.body);
        if(!obstacle || !obstacle->IsInBroadPhase() || obstacle->IsSensor() ||
           obstacle->GetShape()!=state.contact.body_shape_identity.GetPtr()) {
            release(state); return failure(Reason::ContactChanged,state);
        }
        TransformedShape ts; const auto reason=leaf(*obstacle,state.contact.subshape,ts);
        if(reason!=Reason::None || ts.mShape.GetPtr()!=state.contact.leaf_shape_identity.GetPtr()) {
            release(state); return failure(Reason::ContactChanged,state);
        }
        path=geometry(ts,a,b,state.contact.topology);
        if(path.status==RoundedBoxLineStatus::Clear) { release(state); obstacle=nullptr; }
        else if(path.status!=RoundedBoxLineStatus::Wrapped) return failure(Reason::UnsupportedGeometry,state,path.status);
        else { wrapped=true; selected=state.contact; }
    }
    const auto hits=cast(world,filter,a,b);
    if(hits.overflow) return failure(Reason::CapacityExceeded,state);
    for(unsigned i=0;i<hits.count;++i) {
        const auto &hit=hits.hits[i]; if(wrapped && same(selected,hit)) continue;
        auto *body=world.GetBodyLockInterfaceNoLock().TryGetBody(hit.mBodyID);
        if(!body || !body->IsInBroadPhase()) return failure(Reason::ContactChanged,state);
        TransformedShape ts; const auto reason=leaf(*body,hit.mSubShapeID2,ts);
        if(reason!=Reason::None) return failure(reason,state);
        const auto candidate=geometry(ts,a,b);
        if(candidate.status==RoundedBoxLineStatus::Clear) continue; // outer-box ray false positive
        if(candidate.status!=RoundedBoxLineStatus::Wrapped) return failure(Reason::UnsupportedGeometry,state,candidate.status);
        if(wrapped) return failure(Reason::AdditionalContact,state,candidate.status);
        wrapped=true; obstacle=body; path=candidate;
        selected.body=hit.mBodyID; selected.subshape=hit.mSubShapeID2;
        selected.body_shape_identity=body->GetShape(); selected.leaf_shape_identity=ts.mShape;
        selected.topology=candidate.topology;
    }
    if(wrapped) {
        const auto first=path.surface_points[0],last=path.surface_points[path.surface_point_count-1];
        auto reason=leg_clear(world,filter,selected,a,first);
        if(reason==Reason::None) reason=leg_clear(world,filter,selected,last,b);
        if(reason==Reason::None) reason=boundary_clear(world,filter,selected,path);
        if(reason!=Reason::None) return failure(reason,state,path.status);
        if(!state.has_contact) { state.has_contact=true; state.contact=selected; ++state.token; }
    }
    Result result; result.valid=true; result.reason=Reason::None; result.has_contact=wrapped;
    result.contact=state.contact; result.topology_token=state.token;
    result.geometry_status=wrapped ? RoundedBoxLineStatus::Wrapped:RoundedBoxLineStatus::Clear;
    Vec3 ga,gb;
    if(wrapped) { result.length_m=path.length_m; ga=path.gradient_a; gb=path.gradient_b; }
    else {
        const double x=double(b.GetX())-a.GetX(),y=double(b.GetY())-a.GetY(),z=double(b.GetZ())-a.GetZ();
        result.length_m=std::sqrt(x*x+y*y+z*z);
        gb=Vec3(float(x/result.length_m),float(y/result.length_m),float(z/result.length_m)); ga=-gb;
    }
    result.participants[0]={fitting,ga,Vec3(a-fitting->GetCenterOfMassPosition()).Cross(ga)};
    result.participants[1]={anchor,gb,Vec3(b-anchor->GetCenterOfMassPosition()).Cross(gb)};
    result.participant_count=2;
    if(wrapped) {
        const Vec3 angular=-Vec3(a-obstacle->GetCenterOfMassPosition()).Cross(ga)-Vec3(b-obstacle->GetCenterOfMassPosition()).Cross(gb);
        result.participants[result.participant_count++]={obstacle,-ga-gb,angular};
    }
    result.visual_points[result.visual_point_count++]=a;
    if(wrapped) for(std::uint32_t i=0;i<path.surface_point_count;++i) result.visual_points[result.visual_point_count++]=path.surface_points[i];
    result.visual_points[result.visual_point_count++]=b;
    return result;
}
PortableLinePath::Result PortableLinePath::evaluate(JPH::PhysicsSystem &world,const Input &input,State &state) {
    using namespace JPH;
    if(!finite(input.endpoint_a) || !finite(input.anchor_shape_local_point) || input.fitting.IsInvalid() ||
       input.anchor_body.IsInvalid() || input.fitting==input.anchor_body) return failure(Reason::InvalidInput,state);
    auto *fitting=world.GetBodyLockInterfaceNoLock().TryGetBody(input.fitting);
    auto *anchor=world.GetBodyLockInterfaceNoLock().TryGetBody(input.anchor_body);
    if(!fitting || !fitting->IsInBroadPhase() || !fitting->IsRigidBody() || fitting->IsSensor()) return failure(Reason::EndpointMissing,state);
    if(!anchor || !anchor->IsInBroadPhase() || !anchor->IsRigidBody() || anchor->IsSensor() ||
       anchor->GetShape()!=input.anchor_shape_identity.GetPtr()) { release(state); return failure(Reason::AnchorChanged,state); }
    if(Vec3(input.endpoint_a-fitting->GetCenterOfMassPosition()).LengthSq()>1.0e-12F) return failure(Reason::InvalidInput,state);
    return evaluate_world(world,fitting,anchor,input.endpoint_a,
        anchor->GetWorldTransform()*input.anchor_shape_local_point,input.player,input.fitting,state);
}
PortableLinePath::Result PortableLinePath::evaluate_span(JPH::PhysicsSystem &world,const SpanInput &input,State &state) {
    using namespace JPH;
    if(input.a.body.IsInvalid() || input.b.body.IsInvalid() ||
       !finite(input.a.shape_local_point) || !finite(input.b.shape_local_point)) return failure(Reason::InvalidInput,state);
    auto *a=world.GetBodyLockInterfaceNoLock().TryGetBody(input.a.body);
    auto *b=world.GetBodyLockInterfaceNoLock().TryGetBody(input.b.body);
    if(!a || !b || !a->IsInBroadPhase() || !b->IsInBroadPhase() ||
       !a->IsRigidBody() || !b->IsRigidBody() || a->IsSensor() || b->IsSensor()) {
        release(state); return failure(Reason::EndpointMissing,state);
    }
    if(a->GetShape()!=input.a.shape_identity.GetPtr() || b->GetShape()!=input.b.shape_identity.GetPtr()) {
        release(state); return failure(Reason::AnchorChanged,state);
    }
    return evaluate_world(world,a,b,a->GetWorldTransform()*input.a.shape_local_point,
        b->GetWorldTransform()*input.b.shape_local_point,input.player,BodyID(),state);
}
} // namespace scraperx::sim
