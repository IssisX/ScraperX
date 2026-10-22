#include "sim/matron_routecraft.hpp"
#include "sim/physics_world_support.hpp"
#include <algorithm>
#include <tuple>

namespace scraperx::sim {
using namespace JPH;
namespace {
// Overflow rejects the entire query: never select an order-dependent prefix.
class BoundedHits final : public CollideShapeCollector {
public:
    std::vector<CollideShapeResult> hits;
    bool overflow = false;
    void AddHit(const CollideShapeResult &hit) override {
        if (hits.size() == 2048) { overflow = true; ForceEarlyOut(); }
        else hits.push_back(hit);
    }
};
class BoundedSweeps final : public CastShapeCollector {
public:
    std::vector<ShapeCastResult> hits;
    bool overflow = false;
    void AddHit(const ShapeCastResult &hit) override {
        if (hits.size() == 2048) { overflow = true; ForceEarlyOut(); }
        else hits.push_back(hit);
    }
};
class SolidFilter final : public BodyFilter {
public:
    explicit SolidFilter(BodyID self) : self_(self) {}
    bool ShouldCollide(const BodyID &id) const override { return id != self_; }
    bool ShouldCollideLocked(const Body &body) const override { return !body.IsSensor(); }
private: BodyID self_;
};
}
void MatronRoutecraft::cancel() noexcept {
    route_.clear(); cursor_ = 0;
    report_.status = Status::Released; report_.force = 0; report_.reaction = Vec3::sZero();
}
bool MatronRoutecraft::blocked(RVec3 from, RVec3 to) const {
    for (const auto &edge : failed_) if (Vec3(edge.from-from).Length()<0.5F && Vec3(edge.to-to).Length()<0.1F) return true;
    return false;
}
bool MatronRoutecraft::valid(PhysicsSystem &physics, const Grip &g) const {
    BodyLockRead lock(physics.GetBodyLockInterface(), g.body);
    if (!lock.Succeeded() || !lock.GetBody().IsInBroadPhase() || !lock.GetBody().IsStatic() || lock.GetBody().IsSensor()) return false;
    SubShapeID remainder;
    if (!lock.GetBody().GetShape()->GetLeafShape(g.sub, remainder)) return false;
    detail::LeafBox box; uint32 axis;
    if (!detail::leaf_box(lock.GetBody(), g.sub, box) || !detail::box_is_hold(box, axis)) return false;
    const RVec3 point = lock.GetBody().GetCenterOfMassTransform() * g.local;
    if (Vec3(point - g.point).Length() > 0.005F) return false;
    const Vec3 delta(point - box.centre);
    for (uint32 i = 0; i < 3; ++i) if (std::abs(delta.Dot(box.axes[i])) > box.half[i] + 0.005F) return false;
    return std::abs(box.axes[axis].GetY()) < 0.05F;
}
bool MatronRoutecraft::reach(PhysicsSystem &p, BodyID self, RVec3 at, const Grip &g) const {
    if (!budget()) return false;
    if (Vec3(g.point - at).Length() > kReach) return false;
    // Ray to the near surface of the graspable member. Its centerline is
    // inside steel, so permit only the member's own thin terminal section.
    const RVec3 shoulder = at + Vec3(0, 0.45F, 0);
    const Vec3 delta(g.point - shoulder);
    RayCastResult hit;
    ++report_.native_queries;
    if (!p.GetNarrowPhaseQuery().CastRay(RRayCast(shoulder, delta), hit, {}, {}, SolidFilter(self))) return false;
    return hit.mBodyID == g.body && hit.mSubShapeID2 == g.sub && (1 - hit.mFraction) * delta.Length() <= 0.13F;
}
bool MatronRoutecraft::clear(PhysicsSystem &p, BodyID self, RVec3 from, RVec3 to) const {
    if (!budget()) return false;
    CapsuleShape capsule(Matron::kHalfHeight, Matron::kRadius);
    BoundedHits overlap;
    ++report_.native_queries;
    p.GetNarrowPhaseQuery().CollideShape(&capsule, Vec3::sReplicate(1), RMat44::sTranslation(to),
        CollideShapeSettings(), to, overlap, {}, {}, SolidFilter(self));
    if (overlap.overflow) return false;
    for (const auto &hit : overlap.hits) if (hit.mPenetrationDepth > 0.015F) return false;
    if (Vec3(to - from).LengthSq() < 1.0e-8F) return true;
    BoundedSweeps hits;
    ++report_.native_queries;
    p.GetNarrowPhaseQuery().CastShape(RShapeCast(&capsule, Vec3::sReplicate(1), RMat44::sTranslation(from), Vec3(to-from)),
        ShapeCastSettings(), from, hits, {}, {}, SolidFilter(self));
    if (hits.overflow) return false;
    for (const auto &hit : hits.hits) {
        // Only initial resting contact while separating may be ignored.
        if (hit.mFraction <= 1.0e-5F && hit.mPenetrationDepth <= 0.015F &&
            Vec3(to-from).Dot(hit.mPenetrationAxis) <= 0) continue;
        return false;
    }
    return true;
}
bool MatronRoutecraft::transition(PhysicsSystem &p, BodyID self, RVec3 from, const Node &to) const {
    if (!clear(p,self,from,to.center)) return false;
    const int samples=std::max(1,int(std::ceil(Vec3(to.center-from).Length()/0.2F)));
    if (samples>16) return false;
    for (int i=0;i<=samples;++i) if (!reach(p,self,from+(to.center-from)*(double(i)/samples),to.grip)) return false;
    return true;
}
bool MatronRoutecraft::request(PhysicsSystem &p, BodyID self, RVec3 goal, Vec3 facing) {
    cancel(); report_ = {}; failed_.clear(); retries_ = 0; elapsed_ = 0;
    if (!std::isfinite(goal.GetX()) || !std::isfinite(goal.GetY()) || !std::isfinite(goal.GetZ()) ||
        !std::isfinite(facing.GetX()) || !std::isfinite(facing.GetY()) || !std::isfinite(facing.GetZ())) { report_.status = Status::NoRoute; return false; }
    if (self.IsInvalid() || !p.GetBodyInterface().IsAdded(self) || goal.IsNaN() ||
        facing.IsNaN() || std::abs(facing.GetY()) > 0.01F || facing.LengthSq() < 0.9F) return false;
    goal_ = goal; facing_ = facing.Normalized();
    return plan(p, self);
}
bool MatronRoutecraft::plan(PhysicsSystem &p, BodyID self) {
    ++report_.plans; route_.clear(); cursor_ = 0; stall_ = 0;
    const RVec3 start = p.GetBodyInterface().GetPosition(self); last_ = start;
    const Vec3 span = Vec3(goal_ - start).Abs();
    if (!budget()) { report_.status = Status::BudgetExceeded; return false; }
    if (span.GetX() > 8 || span.GetY() > 28 || span.GetZ() > 8 || p.GetGravity().Length() * Matron::kMassKg > kForce * 0.7F) {
        report_.status = Status::BudgetExceeded; return false;
    }
    const RVec3 middle = (start + goal_) * 0.5;
    BoxShape region(span * 0.5F + Vec3(2, 2, 2));
    BoundedHits hits;
    ++report_.native_queries;
    p.GetNarrowPhaseQuery().CollideShape(&region, Vec3::sReplicate(1), RMat44::sTranslation(middle),
        CollideShapeSettings(), middle, hits, {}, {}, SolidFilter(self));
    if (hits.overflow) { report_.status = Status::BudgetExceeded; return false; }
    std::vector<Grip> grips;
    for (const auto &hit : hits.hits) {
        BodyLockRead lock(p.GetBodyLockInterface(), hit.mBodyID2);
        if (!lock.Succeeded() || !lock.GetBody().IsStatic()) continue;
        detail::LeafBox box; uint32 axis;
        if (!detail::leaf_box(lock.GetBody(), hit.mSubShapeID2, box) || !detail::box_is_hold(box, axis) ||
            std::abs(box.axes[axis].GetY()) > 0.05F || std::abs(box.axes[axis].Dot(facing_)) > 0.1F) continue;
        // Long scaffold members supply spaced opportunities, short rungs one.
        const int samples = std::min(5, std::max(1, int(2 * box.half[axis] / 0.5F)));
        for (int s = 0; s < samples; ++s) {
            const float t = samples == 1 ? 0 : (float(s) / float(samples-1) * 2 - 1) * (box.half[axis] - 0.1F);
            const RVec3 point = box.centre + box.axes[axis] * t;
            grips.push_back({hit.mBodyID2, hit.mSubShapeID2, Vec3(lock.GetBody().GetInverseCenterOfMassTransform() * point), point});
        }
    }
    if (grips.size() > 192) { report_.status = Status::BudgetExceeded; return false; }
    std::sort(grips.begin(), grips.end(), [](const Grip &a, const Grip &b) {
        return std::make_tuple(a.point.GetY(),a.point.GetX(),a.point.GetZ(),a.body.GetIndexAndSequenceNumber(),a.sub.GetValue()) <
               std::make_tuple(b.point.GetY(),b.point.GetX(),b.point.GetZ(),b.body.GetIndexAndSequenceNumber(),b.sub.GetValue());
    });
    std::vector<Node> nodes;
    for (const auto &g : grips) {
        for (float y : {-0.65F, 0.35F, 1.45F}) {
            const RVec3 at = g.point - facing_ * 0.55F + Vec3(0,y,0);
            if (clear(p,self,at,at) && reach(p,self,at,g)) nodes.push_back({at,g});
        }
        if (reach(p,self,goal_,g) && clear(p,self,goal_,goal_)) nodes.push_back({goal_,g});
        if (nodes.size() > kMaxNodes) { report_.status = Status::BudgetExceeded; return false; }
    }
    // Breadth first search over contact states, no height monotonicity.
    // New hand acquired at source, old hand then released, body swept while
    // the new support remains within reach for the entire straight segment.
    std::vector<int> parent(nodes.size(), -2), queue;
    for (std::size_t i=0; i<nodes.size(); ++i) {
        if (!blocked(start,nodes[i].center) && transition(p,self,start,nodes[i])) { parent[i]=-1; queue.push_back(int(i)); }
    }
    int end=-1;
    for (std::size_t q=0; q<queue.size(); ++q) {
        const int a=queue[q];
        if (Vec3(nodes[a].center-goal_).Length() < 0.05F) { end=a; break; }
        for (std::size_t b=0; b<nodes.size(); ++b) {
            if (parent[b]!=-2 || Vec3(nodes[b].center-nodes[a].center).Length()>1.4F) continue;
            if (!budget()) { report_.status=Status::BudgetExceeded; return false; }
            if (blocked(nodes[a].center,nodes[b].center) || !transition(p,self,nodes[a].center,nodes[b])) continue;
            parent[b]=a; queue.push_back(int(b));
        }
    }
    if (end<0) {
 report_.status=report_.queries>200000 ? Status::BudgetExceeded : Status::NoRoute; return false; }
    for (int i=end; i>=0; i=parent[i]) route_.push_back(nodes[i]);
    std::reverse(route_.begin(),route_.end());
    report_.status=Status::Moving; ++report_.acquisitions;
    return true;
}
void MatronRoutecraft::step(PhysicsSystem &p, BodyID self, float dt) {
    if (!owns_motion()) return;
    report_.force=0; report_.reaction=Vec3::sZero();
    if (dt <= 0 || dt > 1.0F/89 || !p.GetBodyInterface().IsAdded(self)) { cancel(); return; }
    elapsed_ += dt;
    if (elapsed_ > 120) { cancel(); return; }
    auto &bodies=p.GetBodyInterface();
    const RVec3 at=bodies.GetPosition(self);
    Node &node=route_[cursor_];
    if (!valid(p,node.grip) || !transition(p,self,at,node) || stall_ > 2.5F) {
        if (valid(p,node.grip)) failed_.push_back({at,node.center});
        if (++retries_ > 3) { cancel(); return; }
        if (!plan(p,self)) return;
        return; // no load applied on a failed transition
    }
    const float distance=Vec3(node.center-at).Length();
    if (distance<0.08F && bodies.GetLinearVelocity(self).Length()<0.25F) {
        if (cursor_+1<route_.size()) {
            const Node &next=route_[cursor_+1];
            if (valid(p,next.grip) && transition(p,self,at,next)) { ++cursor_; ++report_.acquisitions; stall_=0; }
            else { stall_+=dt; }
        } else report_.status=Status::Holding;
    }
    const Node &target=route_[cursor_];
    Vec3 desired=Vec3(target.center-at)*3;
    if (desired.Length()>0.9F) desired*=0.9F/desired.Length();
    const Vec3 velocity=bodies.GetLinearVelocity(self);
    Vec3 force=Matron::kMassKg*((desired-velocity)*7-p.GetGravity());
    if (force.Length()>kForce) force*=kForce/force.Length();
    const float power=std::abs(force.Dot(velocity));
    if (power>kPower) force*=kPower/power;
    bodies.AddForce(self,force);
    report_.force=force.Length(); report_.peak_force=std::max(report_.peak_force,report_.force);
    report_.peak_power=std::max(report_.peak_power,std::abs(force.Dot(velocity)));
    report_.hand=target.grip.point; report_.reaction=-force;
    report_.max_reach=std::max(report_.max_reach,Vec3(target.grip.point-at).Length());
    ++report_.steps;
    if (Vec3(at-last_).Length()>0.03F) { last_=at; stall_=0; } else if (report_.status!=Status::Holding) stall_+=dt;
}
}
