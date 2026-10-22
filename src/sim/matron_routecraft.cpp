#include "sim/matron_routecraft.hpp"
#include "sim/physics_world_support.hpp"
#include <algorithm>
#include <tuple>
#include <map>

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
    search_ = {}; holding_.reset(); route_.clear(); cursor_ = 0;
    report_.phase = PlanPhase::None; report_.slice_expansions = 0;
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
bool MatronRoutecraft::reach(PhysicsSystem &p, BodyID self, RVec3 at, const Grip &g, bool count_budget) const {
    if (count_budget && !budget()) return false;
    if (Vec3(g.point - at).Length() > kReach) return false;
    // Ray to the near surface of the graspable member. Its centerline is
    // inside steel, so permit only the member's own thin terminal section.
    const RVec3 shoulder = at + Vec3(0, 0.45F, 0);
    const Vec3 delta(g.point - shoulder);
    RayCastResult hit;
    ++report_.native_queries;
    if (!p.GetNarrowPhaseQuery().CastRay(RRayCast(shoulder, delta), hit, p.GetDefaultBroadPhaseLayerFilter(p.GetBodyInterface().GetObjectLayer(self)),
        p.GetDefaultLayerFilter(p.GetBodyInterface().GetObjectLayer(self)), SolidFilter(self))) return false;
    return hit.mBodyID == g.body && hit.mSubShapeID2 == g.sub && (1 - hit.mFraction) * delta.Length() <= 0.13F;
}
bool MatronRoutecraft::clear(PhysicsSystem &p, BodyID self, RVec3 from, RVec3 to, bool count_budget) const {
    if (count_budget && !budget()) return false;
    CapsuleShape capsule(Matron::kHalfHeight, Matron::kRadius);
    BoundedHits overlap;
    ++report_.native_queries;
    p.GetNarrowPhaseQuery().CollideShape(&capsule, Vec3::sReplicate(1), RMat44::sTranslation(to),
        CollideShapeSettings(), to, overlap, p.GetDefaultBroadPhaseLayerFilter(p.GetBodyInterface().GetObjectLayer(self)),
        p.GetDefaultLayerFilter(p.GetBodyInterface().GetObjectLayer(self)), SolidFilter(self));
    if (overlap.overflow) { if (count_budget) incomplete_clearance_ = true; return false; }
    for (const auto &hit : overlap.hits) if (hit.mPenetrationDepth > 0.015F) return false;
    if (Vec3(to - from).LengthSq() < 1.0e-8F) return true;
    BoundedSweeps hits;
    ++report_.native_queries;
    p.GetNarrowPhaseQuery().CastShape(RShapeCast(&capsule, Vec3::sReplicate(1), RMat44::sTranslation(from), Vec3(to-from)),
        ShapeCastSettings(), from, hits, p.GetDefaultBroadPhaseLayerFilter(p.GetBodyInterface().GetObjectLayer(self)),
        p.GetDefaultLayerFilter(p.GetBodyInterface().GetObjectLayer(self)), SolidFilter(self));
    if (hits.overflow) { if (count_budget) incomplete_clearance_ = true; return false; }
    for (const auto &hit : hits.hits) {
        // Only initial resting contact while separating may be ignored.
        if (hit.mFraction <= 1.0e-5F && hit.mPenetrationDepth <= 0.015F &&
            Vec3(to-from).Dot(hit.mPenetrationAxis) <= 0) continue;
        return false;
    }
    return true;
}
bool MatronRoutecraft::transition(PhysicsSystem &p, BodyID self, RVec3 from, const Node &to, bool count_budget) const {
    const float length = Vec3(to.center - from).Length();
    if (length > 3.2F || Vec3(to.grip.point - from).Length() > kReach ||
        Vec3(to.grip.point - to.center).Length() > kReach) return false;
    if (!clear(p,self,from,to.center,count_budget)) return false;
    const int samples=std::max(1,int(std::ceil(Vec3(to.center-from).Length()/0.2F)));
    if (samples>16) return false;
    for (int i=0;i<=samples;++i) if (!reach(p,self,from+(to.center-from)*(double(i)/samples),to.grip,count_budget)) return false;
    return true;
}
bool MatronRoutecraft::request(PhysicsSystem &p, BodyID self, RVec3 goal, Vec3 facing) {
    const auto previous_hold = holding_;
    cancel(); report_ = {}; failed_.clear(); retries_ = 0; elapsed_ = 0;
    if (!std::isfinite(goal.GetX()) || !std::isfinite(goal.GetY()) || !std::isfinite(goal.GetZ()) ||
        !std::isfinite(facing.GetX()) || !std::isfinite(facing.GetY()) || !std::isfinite(facing.GetZ())) { report_.status = Status::NoRoute; return false; }
    if (self.IsInvalid() || !p.GetBodyInterface().IsAdded(self) || goal.IsNaN() ||
        facing.IsNaN() || std::abs(facing.GetY()) > 0.01F || facing.LengthSq() < 0.9F) return false;
    goal_ = goal; facing_ = facing.Normalized();
    holding_ = previous_hold;
    return plan(p, self);
}
bool MatronRoutecraft::plan(PhysicsSystem &p, BodyID self) {
    ++report_.plans; search_ = {}; route_.clear(); cursor_ = 0; stall_ = 0; incomplete_clearance_ = false;
    report_.phase = PlanPhase::None;
    const RVec3 start = p.GetBodyInterface().GetPosition(self); last_ = start;
    search_.start = start;
    if (holding_) holding_->center = start;
    const Vec3 span = Vec3(goal_ - start).Abs();
    if (!budget()) { finish_search(Status::BudgetExceeded); return false; }
    if (span.GetX() > 8 || span.GetY() > 28 || span.GetZ() > 8 || p.GetGravity().Length() * Matron::kMassKg > kForce * 0.7F) {
        finish_search(Status::BudgetExceeded); return false;
    }
    const RVec3 middle = (start + goal_) * 0.5;
    BoxShape region(span * 0.5F + Vec3(2, 2, 2));
    BoundedHits hits;
    ++report_.native_queries;
    p.GetNarrowPhaseQuery().CollideShape(&region, Vec3::sReplicate(1), RMat44::sTranslation(middle),
        CollideShapeSettings(), middle, hits, p.GetDefaultBroadPhaseLayerFilter(p.GetBodyInterface().GetObjectLayer(self)),
        p.GetDefaultLayerFilter(p.GetBodyInterface().GetObjectLayer(self)), SolidFilter(self));
    if (hits.overflow) { finish_search(Status::BudgetExceeded); return false; }
    std::vector<Grip> grips;
    // One geometry leaf can produce several query hits. Its samples are
    // generated once, independent of narrow-phase callback ordering.
    const auto leaf_key = [](const CollideShapeResult &hit) {
        return std::make_pair(hit.mBodyID2.GetIndexAndSequenceNumber(), hit.mSubShapeID2.GetValue());
    };
    std::sort(hits.hits.begin(), hits.hits.end(), [&](const auto &a, const auto &b) { return leaf_key(a) < leaf_key(b); });
    hits.hits.erase(std::unique(hits.hits.begin(), hits.hits.end(),
        [&](const auto &a, const auto &b) { return leaf_key(a) == leaf_key(b); }), hits.hits.end());
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
            const Vec3 offset = Vec3(point - middle).Abs();
            const Vec3 extent = span * 0.5F + Vec3(2, 2, 2);
            if (offset.GetX() > extent.GetX() || offset.GetY() > extent.GetY() || offset.GetZ() > extent.GetZ()) continue;
            grips.push_back({hit.mBodyID2, hit.mSubShapeID2, Vec3(lock.GetBody().GetInverseCenterOfMassTransform() * point), point});
        }
    }
    std::sort(grips.begin(), grips.end(), [](const Grip &a, const Grip &b) {
        return std::make_tuple(a.point.GetY(),a.point.GetX(),a.point.GetZ(),a.body.GetIndexAndSequenceNumber(),a.sub.GetValue()) <
               std::make_tuple(b.point.GetY(),b.point.GetX(),b.point.GetZ(),b.body.GetIndexAndSequenceNumber(),b.sub.GetValue());
    });
    // Cheap catalog only: no whole-region clearance/visibility work. Its
    // upper bound follows from 2048 leaf hits, five samples, seven postures.
    // All heights and lateral branches remain available to the search.
    auto &catalog = search_.catalog;
    for (const Grip &grip : grips) {
        for (const float standoff : {0.40F, 0.55F}) {
            for (const float height : {-0.65F, 0.35F, 1.45F}) {
                catalog.push_back({grip.point - facing_ * standoff + Vec3(0, height, 0), grip});
            }
        }
        if (Vec3(goal_ - grip.point).Length() <= kReach) catalog.push_back({goal_, grip});
    }
    const auto key = [](const Node &node) {
        return std::make_tuple(node.center.GetY(), node.center.GetX(), node.center.GetZ(),
            node.grip.point.GetY(), node.grip.point.GetX(), node.grip.point.GetZ(),
            node.grip.body.GetIndexAndSequenceNumber(), node.grip.sub.GetValue());
    };
    std::sort(catalog.begin(), catalog.end(), [&](const Node &a, const Node &b) { return key(a) < key(b); });
    catalog.erase(std::unique(catalog.begin(), catalog.end(),
        [&](const Node &a, const Node &b) { return key(a) == key(b); }), catalog.end());

    if (catalog.empty()) { finish_search(Status::NoRoute); return false; }
    for (std::size_t i = 0; i < catalog.size(); ++i) search_.cells[cell(catalog[i].center)].push_back(int(i));
    search_.admitted.assign(catalog.size(), -1);
    search_.posture_valid.assign(catalog.size(), -1);
    search_.states.reserve(kMaxNodes);
    search_.states.push_back({{start, {BodyID(), SubShapeID(), Vec3::sZero(), RVec3::sZero()}}, -1, 0, false});
    report_.status = Status::Moving;
    report_.phase = PlanPhase::Search;
    return true;
}
MatronRoutecraft::Cell MatronRoutecraft::cell(RVec3 point) const {
    // The grid origin belongs to the search, even while the body moves.
    const Vec3 local(point - search_.start);
    return Cell(int(std::floor(local.GetX() / kCellSize)), int(std::floor(local.GetY() / kCellSize)),
        int(std::floor(local.GetZ() / kCellSize)));
}
void MatronRoutecraft::finish_search(Status status) {
    search_ = {}; route_.clear(); holding_.reset(); cursor_ = 0;
    report_.phase = PlanPhase::None;
    report_.status = status;
}
void MatronRoutecraft::replan(PhysicsSystem &p, BodyID self) {
    if (report_.queries > 200000) { finish_search(Status::BudgetExceeded); return; }
    if (++retries_ > 3) { cancel(); return; }
    (void)plan(p, self);
}
void MatronRoutecraft::advance_plan(PhysicsSystem &p, BodyID self, unsigned expansion_budget, unsigned transition_budget) {
    if (expansion_budget == 0 || transition_budget == 0) return;
    if (report_.phase == PlanPhase::Validate) {
        // Sampled hand reach is sliced too. New validation and live hold
        // queries do not debit the unchanged A* / execution query budget.
        if (search_.validated < route_.size()) {
            for (unsigned checked = 0; checked < expansion_budget && search_.validated < route_.size(); ++checked) {
                const std::size_t i = search_.validated;
                const RVec3 from = i == 0 ? p.GetBodyInterface().GetPosition(self) : route_[i - 1].center;
                if (!valid(p, route_[i].grip) || !transition(p, self, from, route_[i], false)) { replan(p, self); return; }
                ++search_.validated;
            }
            return;
        }
        // Publication gets its own tick. Recheck every grip and body edge
        // together: geometry may have changed after an earlier validation
        // slice. Execution also checks the first transfer from the live body.
        RVec3 from = p.GetBodyInterface().GetPosition(self);
        for (const Node &node : route_) {
            if (!valid(p, node.grip) || !clear(p, self, from, node.center, false)) { replan(p, self); return; }
            from = node.center;
        }
        search_ = {};
        report_.phase = PlanPhase::None;
        ++report_.acquisitions;
        return;
    }
    unsigned transitions = 0, neighbor_checks = 0;
    constexpr unsigned kNeighborChecksPerExpansion = 128;
    const unsigned unlimited = std::numeric_limits<unsigned>::max();
    const unsigned neighbor_budget = expansion_budget > unlimited / kNeighborChecksPerExpansion
        ? unlimited : expansion_budget * kNeighborChecksPerExpansion;
    // Resuming the neighbor list spends an expansion slot too: one slice
    // cannot finish a suspended expansion and then start a full extra batch.
    if (search_.current >= 0) report_.slice_expansions = 1;
    while (true) {
        auto &states = search_.states;
        if (search_.current < 0) {
            if (report_.slice_expansions >= expansion_budget || transitions >= transition_budget) return;
            int current = -1;
            float best = std::numeric_limits<float>::max();
            if (search_.expansions < kMaxNodes) {
                for (std::size_t i = 0; i < states.size(); ++i) {
                    if (states[i].closed) continue;
                    const float score = states[i].cost + Vec3(states[i].node.center - goal_).Length();
                    if (score < best) { best = score; current = int(i); }
                }
            }
            if (current < 0) {
                finish_search(search_.node_budget_exhausted || incomplete_clearance_ || report_.queries > 200000
                    ? Status::BudgetExceeded : Status::NoRoute);
                return;
            }
            ++search_.expansions; ++report_.expansions; ++report_.slice_expansions;
            search_.current = current;
            states[current].closed = true;
            const Node from = states[current].node;
            if (current > 0 && Vec3(from.center - goal_).Length() < 0.05F) {
                for (int i = current; i > 0; i = states[i].parent) route_.push_back(states[i].node);
                std::reverse(route_.begin(), route_.end());
                report_.phase = PlanPhase::Validate;
                return;
            }
            const int radius = current == 0 ? 3 : 1;
            const Cell origin = cell(from.center);
            const int cx = std::get<0>(origin), cy = std::get<1>(origin), cz = std::get<2>(origin);
            search_.neighbors.clear(); search_.neighbor = 0;
            for (int x = cx - radius; x <= cx + radius; ++x) {
                for (int y = cy - radius; y <= cy + radius; ++y) {
                    for (int z = cz - radius; z <= cz + radius; ++z) {
                        const auto found = search_.cells.find(Cell(x, y, z));
                        if (found != search_.cells.end())
                            search_.neighbors.insert(search_.neighbors.end(), found->second.begin(), found->second.end());
                    }
                }
            }
            std::sort(search_.neighbors.begin(), search_.neighbors.end());
        }
        const int current = search_.current;
        const Node from = states[current].node;
        const float edge_limit = current == 0 ? 3.2F : kCellSize;
        while (search_.neighbor < search_.neighbors.size()) {
            if (neighbor_checks == neighbor_budget) return;
            ++neighbor_checks;
            const int candidate = search_.neighbors[search_.neighbor];
            const Node &to = search_.catalog[candidate];
            const float length = Vec3(to.center - from.center).Length();
            int index = search_.admitted[candidate];
            const float cost = states[current].cost + length;
            if (length > edge_limit || Vec3(to.grip.point - from.center).Length() > kReach ||
                Vec3(to.grip.point - to.center).Length() > kReach || blocked(from.center, to.center) ||
                (index >= 0 && (states[index].closed || cost >= states[index].cost))) {
                ++search_.neighbor;
                continue;
            }
            // A cached refusal performs no native query. Preserve its
            // original logical budget charge without spending a query slot.
            if (search_.posture_valid[candidate] == 0) {
                ++search_.neighbor;
                if (!budget()) { finish_search(Status::BudgetExceeded); return; }
                continue;
            }
            // A dense source can make a single expansion expensive. Resume
            // its next edge in the same sorted order, without redoing queries.
            if (transitions == transition_budget) return;
            ++transitions; ++search_.neighbor;
            if (!budget()) { finish_search(Status::BudgetExceeded); return; }
            if (search_.posture_valid[candidate] < 0) {
                search_.posture_valid[candidate] = clear(p, self, to.center, to.center) && reach(p, self, to.center, to.grip);
            }
            if (!search_.posture_valid[candidate] || !transition(p, self, from.center, to)) continue;
            if (index < 0) {
                if (states.size() == kMaxNodes) { search_.node_budget_exhausted = true; continue; }
                index = int(states.size());
                search_.admitted[candidate] = index;
                states.push_back({to, current, cost, false});
            } else {
                states[index].parent = current;
                states[index].cost = cost;
            }
        }
        search_.current = -1;
    }
}
void MatronRoutecraft::hold(PhysicsSystem &p, BodyID self) {
    if (!holding_) {
        // Her normal walking controller is paused during planning. The body
        // has no friction or damping, so retain standing through finite foot
        // traction only when the capsule actually touches a static floor.
        auto &bodies = p.GetBodyInterface();
        const Vec3 velocity = bodies.GetLinearVelocity(self);
        const Vec3 horizontal(velocity.GetX(), 0, velocity.GetZ());
        const RVec3 at = bodies.GetPosition(self);
        const Vec3 offset(search_.start - at);
        Vec3 desired = 3 * Vec3(offset.GetX(), 0, offset.GetZ());
        if (desired.Length() > 0.9F) desired *= 0.9F / desired.Length();
        if ((desired - horizontal).LengthSq() < 1.0e-8F) return;
        CapsuleShape capsule(Matron::kHalfHeight, Matron::kRadius);
        BoundedHits contacts;
        CollideShapeSettings settings;
        // A resting capsule can be micrometres above the analytic surface
        // after float rounding. Require the solver's real contact as well.
        settings.mMaxSeparationDistance = cDefaultCollisionTolerance;
        ++report_.native_queries;
        p.GetNarrowPhaseQuery().CollideShape(&capsule, Vec3::sReplicate(1), RMat44::sTranslation(at),
            settings, at, contacts, p.GetDefaultBroadPhaseLayerFilter(bodies.GetObjectLayer(self)),
            p.GetDefaultLayerFilter(bodies.GetObjectLayer(self)), SolidFilter(self));
        if (contacts.overflow) return;
        bool grounded = false;
        for (const auto &hit : contacts.hits) {
            if (hit.mPenetrationDepth < -cDefaultCollisionTolerance || !p.WereBodiesInContact(self, hit.mBodyID2) ||
                hit.mContactPointOn1.GetY() > -Matron::kHalfHeight ||
                hit.mPenetrationAxis.LengthSq() < 1.0e-8F || -hit.mPenetrationAxis.Normalized().GetY() < 0.7F) continue;
            BodyLockRead lock(p.GetBodyLockInterface(), hit.mBodyID2);
            if (lock.Succeeded() && lock.GetBody().IsInBroadPhase() && lock.GetBody().IsStatic() && !lock.GetBody().IsSensor()) {
                grounded = true;
                break;
            }
        }
        if (!grounded) return;
        Vec3 force = Matron::kMassKg * 7 * (desired - horizontal);
        const float limit = std::min(kForce, Matron::kMassKg * std::min(Matron::kFootAccel, std::max(0.0F, -p.GetGravity().GetY())));
        if (force.Length() > limit) force *= limit / force.Length();
        const float power = std::abs(force.Dot(velocity));
        if (power > kPower) force *= kPower / power;
        bodies.AddForce(self, force);
        report_.force = force.Length(); report_.reaction = -force;
        report_.peak_force = std::max(report_.peak_force, report_.force);
        report_.peak_power = std::max(report_.peak_power, std::abs(force.Dot(velocity)));
        return; // no vertical support, acquired hand, or body pose writes
    }
    const RVec3 at = p.GetBodyInterface().GetPosition(self);
    // These bounded live support checks run every waiting tick. They are
    // real native queries, but must not make search exhaustion depend on
    // how many slices were used to find the same route.
    if (!valid(p, holding_->grip) || !clear(p, self, at, holding_->center, false) ||
        !reach(p, self, at, holding_->grip, false) || !reach(p, self, holding_->center, holding_->grip, false)) {
        holding_.reset();
        return;
    }
    drive(p, self, *holding_);
}
void MatronRoutecraft::drive(PhysicsSystem &p, BodyID self, const Node &target) {
    auto &bodies = p.GetBodyInterface();
    const RVec3 at = bodies.GetPosition(self);
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
}
void MatronRoutecraft::step(PhysicsSystem &p, BodyID self, float dt, unsigned expansion_budget, unsigned transition_budget) {
    if (!planning() && !owns_motion()) return;
    report_.force=0; report_.reaction=Vec3::sZero(); report_.slice_expansions=0;
    if (dt <= 0 || dt > 1.0F/89 || !p.GetBodyInterface().IsAdded(self)) { cancel(); return; }
    if (!planning()) elapsed_ += dt; // retain the movement timeout, independent of slice size
    if (elapsed_ > 120) { cancel(); return; }
    auto &bodies=p.GetBodyInterface();
    const RVec3 at=bodies.GetPosition(self);
    if (planning()) {
        advance_plan(p, self, expansion_budget, transition_budget);
        if (planning()) { hold(p, self); return; }
        if (!owns_motion()) return;
    }
    Node &node=route_[cursor_];
    if (!valid(p,node.grip) || !transition(p,self,at,node) || stall_ > 2.5F) {
        if (valid(p,node.grip)) failed_.push_back({at,node.center});
        replan(p, self);
        if (planning()) hold(p, self);
        return;
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
    holding_ = Node{at, target.grip};
    drive(p, self, target);
    if (Vec3(at-last_).Length()>0.03F) { last_=at; stall_=0; } else if (report_.status!=Status::Holding) stall_+=dt;
}
}
