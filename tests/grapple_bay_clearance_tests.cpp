#include "sim/grapple_practice_route.hpp"
#include "sim/split_inspection_route.hpp"

#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CompoundShape.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/RegisterTypes.h>
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
#include <utility>

// Fast production-geometry regression, before an expensive public route replay.
// Explicit legal span-pose samples and the real standing capsule dimensions
// from Simulation (0.55m cylinder half-height,0.35m radius). No stepping or
// gameplay pose/velocity/anchor authority is supplied by this layout fixture.
namespace {
using namespace JPH;
using namespace scraperx::sim;
struct BP final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 2; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override { return BroadPhaseLayer(layer); }
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "bay-clearance"; }
};
struct BV final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer a,BroadPhaseLayer b) const override { return a!=0 || b.GetValue()!=0; }
};
struct LP final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer a,ObjectLayer b) const override { return a!=0 || b!=0; }
};

int run(const bool structure) {
    BP bp; BV bv; LP lp;
    PhysicsSystem world;
    world.Init(256,0,1024,4096,bp,bv,lp);
    kit::Kit kit(world,0,1);
    build_split_inspection_route(kit);
    (void)build_grapple_practice_route(kit);
    world.OptimizeBroadPhase();
    const auto span=kit.body_id(kit.body_for_entity(kSplitInspectionGirderEntity));
    const RefConst<Shape> standing=new CapsuleShape(.55F,.35F);
    CollideShapeSettings settings;
    settings.mMaxSeparationDistance=0;
    if(structure) {
        struct FrameFilter final : BodyFilter {
            BodyID self,bearing;
            bool ShouldCollide(const BodyID &id) const override { return id!=self && id!=bearing; }
        } filter;
        filter.self=span;
        filter.bearing=kit.body_id(kit.body_for_entity(kSplitInspectionBearingEntity));
        unsigned overlaps=0;
        for(int sample=0;sample<=20;++sample) {
            const float angle=kSplitInspectionSpanRaisedAngle*(1.F-static_cast<float>(sample)/20.F);
            world.GetBodyInterface().SetPositionAndRotation(span,kSplitInspectionSpanPivot,
                Quat::sRotation(Vec3::sAxisZ(),angle),EActivation::Activate);
            RefConst<Shape> shape; RMat44 transform;
            { BodyLockRead lock(world.GetBodyLockInterface(),span);
              shape=lock.GetBody().GetShape();transform=lock.GetBody().GetCenterOfMassTransform(); }
            AllHitCollisionCollector<CollideShapeCollector> hits;
            world.GetNarrowPhaseQuery().CollideShape(shape,Vec3::sReplicate(1),transform,
                settings,kSplitInspectionSpanPivot,hits,{}, {},filter);
            for(const auto &hit:hits.mHits) {
                if(hit.mPenetrationDepth<=.001F) continue;
                ++overlaps;
                SubShapeID remainder;
                const auto first=static_cast<const CompoundShape *>(shape.GetPtr())->GetSubShapeIndexFromID(hit.mSubShapeID1,remainder);
                const auto entity=world.GetBodyInterface().GetUserData(hit.mBodyID2);
                const auto other_body=kit.body_for_entity(entity);
                BodyLockRead other_lock(world.GetBodyLockInterface(),hit.mBodyID2);
                const auto second=static_cast<const CompoundShape *>(other_lock.GetBody().GetShape())->GetSubShapeIndexFromID(hit.mSubShapeID2,remainder);
                // The25mm lower-seat shim intentionally meets the walking
                // box at the neutral endpoint. All other steel must clear.
                if(entity==kSplitInspectionSpanSeatEntity && first==1 && second==0 &&
                    sample==20 && hit.mPenetrationDepth<.03F) { --overlaps; continue; }
                std::cout << "STRUCTURE_OVERLAP angle=" << angle
                    << " entity=" << entity
                    << " span_part=" << first
                    << " other_part=" << second
                    << " depth=" << hit.mPenetrationDepth
                    << " point=" << hit.mContactPointOn2.GetX() << ','
                    << hit.mContactPointOn2.GetY() << ',' << hit.mContactPointOn2.GetZ() << '\n';
                if(sample==0) for(const auto &p:{kit.body_parts(kit.body_for_entity(kSplitInspectionGirderEntity)).at(first),kit.body_parts(other_body).at(second)})
                    std::cout << "MEMBER center=" << p.offset.GetX() << ',' << p.offset.GetY() << ',' << p.offset.GetZ()
                        << " half=" << p.half.GetX() << ',' << p.half.GetY() << ',' << p.half.GetZ() << '\n';
            }
        }
        std::cout << (overlaps ? "FAIL" : "PASS") << " GRAPPLE_BAY_STRUCTURE overlaps="
            << overlaps << " legal_poses=21 intended_lower_seat=1\n";
        return overlaps ? 1 : 0;
    }
    unsigned poses=0,blocked=0;
    std::set<std::pair<std::uint64_t,std::uint32_t>> reported;
    const auto check_pose=[&](RVec3Arg p,const float angle) {
        ++poses;
        AllHitCollisionCollector<CollideShapeCollector> hits;
        world.GetNarrowPhaseQuery().CollideShape(standing,Vec3::sReplicate(1),
            RMat44::sTranslation(p),settings,p,hits);
        bool clear=true;
        for(const auto &hit:hits.mHits) {
            // Only millimetric floor tangency is ignored. A real limb-height
            // member must not disappear behind debug-restart's20mm allowance.
            if(hit.mPenetrationDepth<=.001F) continue;
            clear=false;
            const auto entity=world.GetBodyInterface().GetUserData(hit.mBodyID2);
            const auto subshape=hit.mSubShapeID2.GetValue();
            if(reported.emplace(entity,subshape).second)
                std::cout << "BAY_OBSTRUCTION entity=" << entity << " subshape=" << subshape
                          << " capsule=" << p.GetX() << ',' << p.GetY() << ',' << p.GetZ()
                          << " span_angle=" << angle << " penetration=" << hit.mPenetrationDepth << '\n';
        }
        if(!clear) ++blocked;
    };
    // Standing approach and a0.7m-wide central ingress at5cm spacing. These
    // samples reject the complete set of known blockers in one cheap query
    // pass; the ordinary-input test remains the continuous runtime proof.
    for(float angle:{kSplitInspectionSpanRaisedAngle,kSplitInspectionSpanRaisedAngle*.5F,0.F}) {
        world.GetBodyInterface().SetPositionAndRotation(span,kSplitInspectionSpanPivot,
            Quat::sRotation(Vec3::sAxisZ(),angle),EActivation::Activate);
        for(int i=0;i<=40;++i) check_pose(RVec3(10+.05*i,485.15,-132.8),angle);
        for(double x:{11.65,12.0,12.35})
            for(int i=0;i<=54;++i) check_pose(RVec3(x,485.15,-132.8-.05*i),angle);
    }
    std::cout << (blocked ? "FAIL" : "PASS") << " GRAPPLE_BAY_CLEARANCE poses=" << poses
              << " blocked=" << blocked << " native_compounds=1 player_capsule=1 gameplay_route=0\n";
    return blocked ? 1 : 0;
}
}
int main(int argc,char **) {
    RegisterDefaultAllocator();
    Factory::sInstance=new Factory;
    RegisterTypes();
    int result=1;
    try { result=run(true); if(argc==1) result=run(false) || result; }
    catch(const std::exception &e) { std::cerr << "FAIL GRAPPLE_BAY_CLEARANCE " << e.what() << '\n'; }
    UnregisterTypes();
    delete Factory::sInstance; Factory::sInstance=nullptr;
    return result;
}
