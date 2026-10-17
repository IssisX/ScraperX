#include "sim/traversal_affordance.hpp"
#include "sim/traversal_receiver.hpp"
#include "sim/traversal_plan.hpp"
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
using namespace JPH;
using namespace scraperx::sim;
unsigned checks = 0;
void check(bool condition, const char *message) {
    ++checks;
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
struct BP final : BroadPhaseLayerInterface {
    uint GetNumBroadPhaseLayers() const override { return 1; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer) const override { return BroadPhaseLayer(0); }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char *GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "traversal-test"; }
#endif
};
struct BV final : ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(ObjectLayer, BroadPhaseLayer) const override { return false; }
};
struct LP final : ObjectLayerPairFilter {
    bool ShouldCollide(ObjectLayer, ObjectLayer) const override { return false; }
};
struct World {
    BP bp; BV bv; LP lp; PhysicsSystem system;
    BodyID support, receiver;
    World() {
        system.Init(16, 0, 16, 16, bp, bv, lp);
        RefConst<Shape> shape = new OffsetCenterOfMassShape(new BoxShape(Vec3(1,1,1)), Vec3(.2F, .3F, .4F));
        BodyCreationSettings settings(shape, RVec3(2,5,8), Quat::sIdentity(), EMotionType::Dynamic, 0);
        settings.mUserData = 41;
        support = system.GetBodyInterface().CreateAndAddBody(settings, EActivation::Activate);
        settings.mUserData = 42;
        settings.mPosition = RVec3(6,5,8);
        receiver = system.GetBodyInterface().CreateAndAddBody(settings, EActivation::Activate);
        check(!support.IsInvalid() && !receiver.IsInvalid(), "actual rigid bodies exist");
    }
    TraversalAffordance sample() {
        TraversalAffordance a;
        a.rigid.body = support;
        a.rigid.entity_id = 41;
        a.rigid.geometry_generation = 1;
        a.rigid.local_point = Vec3(1,0,0);
        a.rigid.local_normal = Vec3::sAxisY();
        a.rigid.local_tangent = Vec3::sAxisX();
        a.rigid.shape_identity = system.GetBodyInterface().GetShape(support).GetPtr();
        a.receiver_body = receiver;
        a.receiver_entity_id = 42;
        a.receiver_shape_identity = system.GetBodyInterface().GetShape(receiver).GetPtr();
        a.grip_capable = a.receiver_footing_capable = true;
        return a;
    }
};
void rigid_motion_and_age() {
    World w; TraversalAffordanceCache cache;
    const auto a = w.sample();
    const auto generation = cache.remember(a, 100);
    check(generation != 0, "validated rigid affordance accepted");
    const auto *first = cache.refresh(generation, w.system, 100);
    check(first && (first->world_point - (w.system.GetBodyInterface().GetCenterOfMassPosition(w.support) + RVec3(1,0,0))).Length() < .00001F,
          "point refreshed in COM frame, including displaced shape COM");
    auto &bodies = w.system.GetBodyInterface();
    bodies.SetPositionAndRotation(w.support, RVec3(4,6,9), Quat::sRotation(Vec3::sAxisZ(), JPH_PI * .5F), EActivation::Activate);
    bodies.SetLinearAndAngularVelocity(w.support, Vec3(3,2,1), Vec3(0,0,2));
    const auto *rotated = cache.refresh(generation, w.system, 103);
    check(rotated != nullptr, "same body remains usable while translating/rotating");
    check((rotated->world_normal - Vec3(-1,0,0)).Length() < .00001F &&
          (rotated->world_tangent - Vec3(0,1,0)).Length() < .00001F, "normal and tangent rotate with actual body");
    check((rotated->point_velocity - Vec3(1,2,1)).Length() < .00001F, "point velocity includes angular lever arm");
    check(rotated->observed_tick == 100, "refresh cannot renew probe observation age");
    check(cache.refresh(generation, w.system, 108) != nullptr, "maximum age inclusive");
    check(cache.refresh(generation, w.system, 109) == nullptr && cache.find(generation) == nullptr, "expired sample invalidated");
    auto g = cache.remember(a, 200);
    check(cache.refresh(g, w.system, 199) == nullptr, "tick before observation rejected");
    g = cache.remember(a, 200);
    check(cache.refresh(g, w.system, 204) != nullptr && cache.refresh(g, w.system, 203) == nullptr, "refresh tick rollback rejected");
}
void identities() {
    World w; TraversalAffordanceCache cache;
    auto &b = w.system.GetBodyInterface();
    auto a = w.sample();
    auto g = cache.remember(a, 10);
    b.SetUserData(w.support, 99);
    check(cache.refresh(g, w.system, 10) == nullptr, "changed support entity rejected");
    b.SetUserData(w.support, 41);
    g = cache.remember(a, 10);
    b.SetUserData(w.receiver, 99);
    check(cache.refresh(g, w.system, 10) == nullptr, "changed receiver entity rejected");
    b.SetUserData(w.receiver, 42);
    g = cache.remember(a, 10);
    { BodyLockWrite lock(w.system.GetBodyLockInterface(), w.receiver); lock.GetBody().SetIsSensor(true); }
    check(cache.refresh(g, w.system, 10) == nullptr, "sensor receiver cannot provide footing");
    { BodyLockWrite lock(w.system.GetBodyLockInterface(), w.receiver); lock.GetBody().SetIsSensor(false); }
    g = cache.remember(a, 10);
    { BodyLockWrite lock(w.system.GetBodyLockInterface(), w.support); lock.GetBody().SetIsSensor(true); }
    check(cache.refresh(g, w.system, 10) == nullptr, "sensor support cannot provide grip");
    { BodyLockWrite lock(w.system.GetBodyLockInterface(), w.support); lock.GetBody().SetIsSensor(false); }
    g = cache.remember(a, 10);
    b.SetShape(w.support, new BoxShape(Vec3(2,1,1)), true, EActivation::Activate);
    check(cache.refresh(g, w.system, 10) == nullptr, "replaced support shape rejected before first refresh");
    a = w.sample();
    g = cache.remember(a, 10);
    b.SetShape(w.receiver, new BoxShape(Vec3(2,2,1)), true, EActivation::Activate);
    check(cache.refresh(g, w.system, 10) == nullptr, "replaced receiver shape rejected before first refresh");
    a = w.sample();
    g = cache.remember(a, 10);
    b.RemoveBody(w.receiver);
    check(cache.refresh(g, w.system, 10) == nullptr, "removed receiver rejected while body still allocated");
    b.AddBody(w.receiver, EActivation::Activate);
    g = cache.remember(a, 10);
    b.RemoveBody(w.support);
    b.DestroyBody(w.support);
    check(cache.refresh(g, w.system, 10) == nullptr, "destroyed support body rejected without dereferencing stale shape");
    g = cache.remember(a, 10);
    BodyCreationSettings replacement(new BoxShape(Vec3(1,1,1)), RVec3::sZero(), Quat::sIdentity(), EMotionType::Dynamic, 0);
    replacement.mUserData = 41;
    const auto reused = b.CreateAndAddBody(replacement, EActivation::Activate);
    check(reused.GetIndex() == w.support.GetIndex() && reused != w.support, "fixture actually reuses body index with new sequence");
    check(cache.refresh(g, w.system, 10) == nullptr, "recycled body index cannot revive old BodyID");
}
void rejection_and_capacity() {
    World w; TraversalAffordanceCache cache;
    auto a = w.sample();
    for (auto kind : {TraversalAnchorKind::CargoNetMaterial, TraversalAnchorKind::DeformableFragment}) {
        a.anchor_kind = kind;
        check(cache.remember(a, 1) == 0, "material anchors cannot manufacture rigid support");
    }
    a = w.sample(); a.grip_capable = false;
    check(cache.remember(a, 1) == 0, "ungrippable rejected");
    a = w.sample(); a.receiver_footing_capable = false;
    check(cache.remember(a, 1) == 0, "unvalidated receiver rejected");
    a = w.sample(); a.rigid.shape_identity = nullptr;
    check(cache.remember(a, 1) == 0, "missing support shape identity rejected");
    a = w.sample(); a.receiver_shape_identity = nullptr;
    check(cache.remember(a, 1) == 0, "missing receiver shape identity rejected");
    a = w.sample(); a.rigid.entity_id = 0;
    check(cache.remember(a, 1) == 0, "missing entity identity rejected");
    a = w.sample(); a.rigid.geometry_generation = 0;
    check(cache.remember(a, 1) == 0, "missing geometry revision rejected");
    a = w.sample(); a.receiver_body = BodyID();
    check(cache.remember(a, 1) == 0, "missing receiver body rejected");
    a = w.sample(); a.rigid.body = BodyID();
    check(cache.remember(a, 1) == 0, "missing support body rejected");
    a = w.sample(); a.rigid.local_normal = Vec3::sZero();
    check(cache.remember(a, 1) == 0, "degenerate surface frame rejected");
    a = w.sample(); a.rigid.local_point = Vec3(std::numeric_limits<float>::quiet_NaN(), 0, 0);
    check(cache.remember(a, 1) == 0, "nonfinite anchor rejected in Release");
    a = w.sample();
    auto first = cache.remember(a, 10);
    for (unsigned i = 0; i < 4; ++i) check(cache.remember(a, 10) > first, "generation increases within fixed capacity");
    check(cache.find(first) == nullptr, "fifth observation evicts oldest of four slots");
    auto last = cache.remember(a, 10);
    cache.clear();
    check(cache.find(last) == nullptr && cache.find(0) == nullptr, "clear invalidates all entries");
    check(cache.remember(a, 10) > last, "clear cannot reuse an old generation");
    auto old_geometry = cache.remember(a, 10);
    ++a.rigid.geometry_generation;
    auto new_geometry = cache.remember(a, 10);
    check(cache.find(old_geometry) == nullptr && cache.refresh(new_geometry, w.system, 10), "explicit geometry revision retires prior observations");
    a.receiver_body = a.rigid.body;
    a.receiver_entity_id = a.rigid.entity_id;
    a.receiver_shape_identity = a.rigid.shape_identity;
    auto same = cache.remember(a, 10);
    check(cache.refresh(same, w.system, 10) != nullptr, "same support and receiver lock safely");
}
// Removing the coplanar collector or accepting the whole body must fail these
// queries and the downstream solved-contact acceptance checks.
void receiver_surface_patch() {
    World w;
    StaticCompoundShapeSettings compound;
    compound.AddShape(Vec3::sZero(), Quat::sIdentity(), new BoxShape(Vec3(1,1,1)));
    compound.AddShape(Vec3(0,.9F,0), Quat::sIdentity(), new BoxShape(Vec3(.8F,.1F,.8F)));
    compound.AddShape(Vec3(0,.2F,0), Quat::sIdentity(), new BoxShape(Vec3(.5F,1,.5F)));
    compound.AddShape(Vec3(2,0,0), Quat::sIdentity(), new BoxShape(Vec3(.5F,1,.5F)));
    compound.AddShape(Vec3(0,1.F-1.F/std::cos(.2F),0), Quat::sRotation(Vec3::sAxisZ(),.2F),
                      new BoxShape(Vec3(.5F,1,.5F)));
    const auto shape = compound.Create().Get();
    w.system.GetBodyInterface().SetShape(w.receiver, shape, true, EActivation::Activate);
    const RRayCast ray(RVec3(6,7,8), Vec3(0,-3,0));
    const auto *root = static_cast<const CompoundShape *>(shape.GetPtr());
    const auto leaf = [root](int i) { return root->GetSubShapeIDFromIndex(i, SubShapeIDCreator()).GetID(); };
    RayCastResult canonical;
    canonical.mBodyID = w.receiver;
    canonical.mSubShapeID2 = leaf(0);
    canonical.mFraction = 1.F/3.F;
    ReceiverSurfacePatch patch;
    check(collect_receiver_surface_patch(w.system, ray, canonical, patch), "real compound ray collects receiver patch");
    check(patch.count == 2 && patch.contains(leaf(0)) && patch.contains(leaf(1)), "structural top and coplanar cover accepted");
    check(!patch.contains(leaf(2)) && !patch.contains(leaf(3)) && !patch.contains(leaf(4)), "raised, neighboring and sloped leaves excluded");
    auto a = w.sample(); a.receiver_subshape = leaf(0); a.receiver_patch = patch;
    TraversalPlanState plan; plan.latch(a, Vec3::sZero(), shape.GetPtr(), 10);
    FootContactSample contact;
    contact.valid = true; contact.body = w.receiver; contact.subshape = leaf(1);
    contact.entity_id = 42; contact.shape_identity = shape.GetPtr(); contact.friction = .7F; contact.solved_tick = 11;
    check(plan.accepts(contact, 11), "actual solved cover contact accepted");
    contact.subshape = leaf(2);
    check(!plan.accepts(contact, 11), "same-body raised face remains rejected");
    contact.subshape = leaf(3);
    check(!plan.accepts(contact, 11), "same-body unlisted face remains rejected");
    contact.subshape = leaf(1); contact.solved_tick = 10;
    check(!plan.accepts(contact, 10), "patch cannot renew stale contact");
    contact.solved_tick = 11; contact.body = w.support;
    check(!plan.accepts(contact, 11), "patch cannot accept another body");
    TraversalAffordanceCache cache;
    check(cache.remember(a, 10) != 0, "validated explicit patch cached");
    a.receiver_patch.overflow = true;
    check(cache.remember(a, 10) == 0, "overflowed patch rejected");
    a.receiver_patch = patch;
    for (unsigned i = 0; i < a.receiver_patch.count; ++i)
        if (a.receiver_patch.subshapes[i] == leaf(0)) a.receiver_patch.subshapes[i] = leaf(3);
    check(cache.remember(a, 10) == 0, "explicit patch missing canonical identity rejected");
    a.receiver_patch = {};
    plan.latch(a, Vec3::sZero(), shape.GetPtr(), 10);
    contact.body = w.receiver; contact.subshape = leaf(0);
    check(plan.accepts(contact, 11), "legacy candidate retains exact singleton identity");
    contact.subshape = leaf(1);
    check(!plan.accepts(contact, 11), "legacy fallback never accepts another leaf");
    StaticCompoundShapeSettings too_many;
    for (unsigned i = 0; i != 9; ++i)
        too_many.AddShape(Vec3(0,.01F*float(i),0), Quat::sIdentity(),
                          new BoxShape(Vec3(1,1.F-.01F*float(i),1)));
    const auto crowded_shape = too_many.Create().Get();
    w.system.GetBodyInterface().SetShape(w.receiver, crowded_shape, true, EActivation::Activate);
    canonical.mSubShapeID2 = static_cast<const CompoundShape *>(crowded_shape.GetPtr())
        ->GetSubShapeIDFromIndex(0, SubShapeIDCreator()).GetID();
    check(!collect_receiver_surface_patch(w.system, ray, canonical, patch) && patch.count == 0,
          "actual query rejects more coincident leaves than patch capacity");
    canonical.mBodyID = BodyID();
    check(!collect_receiver_surface_patch(w.system, ray, canonical, patch), "invalid receiver body rejected");
    ReceiverSurfacePatch capacity;
    for (unsigned i = 0; i != 8; ++i) { SubShapeID id; id.SetValue(i); check(capacity.add(id), "bounded patch accepts unique identity"); }
    SubShapeID extra; extra.SetValue(8);
    check(!capacity.add(extra) && !capacity.contains(extra) && capacity.overflow, "ninth identity invalidates patch");
}
void continuation_cannot_supply_contact() {
    World w;
    auto a=w.sample();
    TraversalPlanState plan;
    plan.latch(a,Vec3::sZero(),a.receiver_shape_identity,10);
    const auto centre=w.system.GetBodyInterface().GetPosition(w.receiver);
    const RRayCast ray(centre+RVec3(0,2,0),Vec3(0,-4,0));
    const auto accepted=[&](const RRayCast &probe) {
        return receiver_supports_continuation(w.system,probe,plan,BodyID(),.7F);
    };
    check(accepted(ray),"clear continuation on exact receiver is prospective geometry");
    FootContactSample absent;
    check(!plan.accepts(absent,11)&&!plan.foot_started&&plan.shared_solved_ticks==0,
          "continuation cannot fabricate contact, a foot row or shared solve");
    check(!accepted(RRayCast(ray.mOrigin+RVec3(3,0,0),ray.mDirection)),
          "unsupported continuation rejected while original receiver remains");
    const auto other=w.system.GetBodyInterface().GetPosition(w.support);
    check(!accepted(RRayCast(other+RVec3(0,2,0),ray.mDirection)),
          "another body cannot extend the accepted receiver");
    const auto patch=plan.receiver_patch;
    plan.receiver_patch={};
    check(!accepted(ray),"unaccepted receiver leaf cannot extend the target");
    plan.receiver_patch=patch;
    auto &b=w.system.GetBodyInterface();
    b.SetUserData(w.receiver,99);
    check(!accepted(ray),"changed entity invalidates prospective continuation");
    b.SetUserData(w.receiver,42);
    { BodyLockWrite lock(w.system.GetBodyLockInterface(),w.receiver); lock.GetBody().SetIsSensor(true); }
    check(!accepted(ray),"sensor cannot extend the receiving surface");
    { BodyLockWrite lock(w.system.GetBodyLockInterface(),w.receiver); lock.GetBody().SetIsSensor(false); }
    check(accepted(ray),"original receiver can be revalidated without renewing contact");
    b.SetShape(w.receiver,new BoxShape(Vec3(1,1,1)),true,EActivation::Activate);
    check(!accepted(ray),"replaced receiver shape invalidates continuation");
}
} // namespace

int main() {
    RegisterDefaultAllocator(); Factory::sInstance = new Factory; RegisterTypes();
    rigid_motion_and_age(); identities(); rejection_and_capacity(); receiver_surface_patch();
    continuation_cannot_supply_contact();
    UnregisterTypes(); delete Factory::sInstance; Factory::sInstance = nullptr;
    std::cout << "traversal affordance: PASS " << checks << " checks (Release-safe)\n";
}
