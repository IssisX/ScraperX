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
// Clearance only needs a refusal and the hit cap. Keep visiting hits after a
// refusal so overflow has exactly the same meaning as the catalog collector,
// but avoid allocating and copying a result vector for every search query.
class ClearanceHits final : public CollideShapeCollector {
public:
    bool blocked = false, overflow = false;
    void AddHit(const CollideShapeResult &hit) override {
        if (count_ == 2048) { overflow = true; ForceEarlyOut(); }
        else { ++count_; blocked = blocked || hit.mPenetrationDepth > 0.015F; }
    }
private:
    unsigned count_ = 0;
};
class ClearanceSweeps final : public CastShapeCollector {
public:
    explicit ClearanceSweeps(Vec3 direction) : direction_(direction) {}
    bool blocked = false, overflow = false;
    void AddHit(const ShapeCastResult &hit) override {
        if (count_ == 2048) { overflow = true; ForceEarlyOut(); }
        else {
            ++count_;
            // Only initial resting contact while separating may be ignored.
            blocked = blocked || !(hit.mFraction <= 1.0e-5F && hit.mPenetrationDepth <= 0.015F &&
                direction_.Dot(hit.mPenetrationAxis) <= 0);
        }
    }
private:
    Vec3 direction_;
    unsigned count_ = 0;
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
    report_.slice_queries = 0; report_.slice_neighbor_checks = 0;
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
    if (!p.GetNarrowPhaseQuery().CastRay(RRayCast(shoulder, delta), hit, p.GetDefaultBroadPhaseLayerFilter(query_layer_),
        p.GetDefaultLayerFilter(query_layer_), SolidFilter(self))) return false;
    return hit.mBodyID == g.body && hit.mSubShapeID2 == g.sub && (1 - hit.mFraction) * delta.Length() <= 0.13F;
}
bool MatronRoutecraft::clear(PhysicsSystem &p, BodyID self, RVec3 from, RVec3 to, bool count_budget) const {
    if (count_budget && !budget()) return false;
    CapsuleShape capsule(Matron::kHalfHeight, Matron::kRadius);
    ClearanceHits overlap;
    ++report_.native_queries;
    p.GetNarrowPhaseQuery().CollideShape(&capsule, Vec3::sReplicate(1), RMat44::sTranslation(to),
        CollideShapeSettings(), to, overlap, p.GetDefaultBroadPhaseLayerFilter(query_layer_),
        p.GetDefaultLayerFilter(query_layer_), SolidFilter(self));
    if (overlap.overflow) { if (count_budget) incomplete_clearance_ = true; return false; }
    if (overlap.blocked) return false;
    if (Vec3(to - from).LengthSq() < 1.0e-8F) return true;
    ClearanceSweeps hits{Vec3(to - from)};
    ++report_.native_queries;
    p.GetNarrowPhaseQuery().CastShape(RShapeCast(&capsule, Vec3::sReplicate(1), RMat44::sTranslation(from), Vec3(to-from)),
        ShapeCastSettings(), from, hits, p.GetDefaultBroadPhaseLayerFilter(query_layer_),
        p.GetDefaultLayerFilter(query_layer_), SolidFilter(self));
    if (hits.overflow) { if (count_budget) incomplete_clearance_ = true; return false; }
    return !hits.blocked;
}
bool MatronRoutecraft::transition(PhysicsSystem &p, BodyID self, RVec3 from, const Node &to, bool count_budget) const {
    Transfer transfer;
    unsigned credits = std::numeric_limits<unsigned>::max();
    return resume_transition(p, self, from, to, transfer, credits, count_budget) == ProbeResult::Passed;
}
MatronRoutecraft::ProbeResult MatronRoutecraft::resume_transition(PhysicsSystem &p, BodyID self,
    RVec3 from, const Node &to, Transfer &transfer, unsigned &credits, bool count_budget) const {
    if (!transfer.started) {
        transfer.from = from;
        transfer.started = true;
        const float length = Vec3(to.center - from).Length();
        if (length > 3.2F || Vec3(to.grip.point - from).Length() > kReach ||
            Vec3(to.grip.point - to.center).Length() > kReach) return ProbeResult::Failed;
    }
    from = transfer.from;
    if (!transfer.cleared) {
        if (credits == 0) return ProbeResult::Pending;
        --credits;
        if (!clear(p, self, from, to.center, count_budget)) return ProbeResult::Failed;
        transfer.samples = unsigned(std::max(1, int(std::ceil(Vec3(to.center - from).Length() / 0.2F))));
        if (transfer.samples > 16) return ProbeResult::Failed;
        transfer.cleared = true;
    }
    while (transfer.sample <= transfer.samples) {
        if (credits == 0) return ProbeResult::Pending;
        --credits;
        const RVec3 at = from + (to.center - from) * (double(transfer.sample) / transfer.samples);
        if (!reach(p, self, at, to.grip, count_budget)) return ProbeResult::Failed;
        ++transfer.sample;
    }
    return ProbeResult::Passed;
}
MatronRoutecraft MatronRoutecraft::replacement() const {
    MatronRoutecraft result;
    result.holding_ = holding_;
    return result;
}
bool MatronRoutecraft::request(PhysicsSystem &p, BodyID self, RVec3 goal, Vec3 facing) {
    const auto previous_hold = holding_;
    cancel(); report_ = {}; failed_.clear(); retries_ = 0; elapsed_ = 0;
    if (!std::isfinite(goal.GetX()) || !std::isfinite(goal.GetY()) || !std::isfinite(goal.GetZ()) ||
        !std::isfinite(facing.GetX()) || !std::isfinite(facing.GetY()) || !std::isfinite(facing.GetZ())) { report_.status = Status::NoRoute; return false; }
    if (self.IsInvalid() || !p.GetBodyInterface().IsAdded(self) || goal.IsNaN() ||
        facing.IsNaN() || std::abs(facing.GetY()) > 0.01F || facing.LengthSq() < 0.9F) return false;
    query_layer_ = p.GetBodyInterface().GetObjectLayer(self);
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
        CollideShapeSettings(), middle, hits, p.GetDefaultBroadPhaseLayerFilter(query_layer_),
        p.GetDefaultLayerFilter(query_layer_), SolidFilter(self));
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
    for (std::size_t i = 0; i < catalog.size(); ++i) {
        const auto entry = search_.cells.emplace(cell(catalog[i].center), unsigned(search_.buckets.size()));
        if (entry.second) search_.buckets.emplace_back();
        search_.buckets[entry.first->second].push_back(int(i));
    }
    search_.admitted.assign(catalog.size(), -1);
    search_.posture_valid.assign(catalog.size(), -1);
    search_.states.reserve(kMaxNodes);
    search_.states.push_back({{start, {BodyID(), SubShapeID(), Vec3::sZero(), RVec3::sZero()}}, -1, 0, false});
    update_open(0);
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
bool MatronRoutecraft::earlier(int a, int b) const {
    const auto &states = search_.states;
    const float left = states[a].cost + Vec3(states[a].node.center - goal_).Length();
    const float right = states[b].cost + Vec3(states[b].node.center - goal_).Length();
    // The old ascending-index scan chose the first equal score. Preserve
    // that tie rule exactly, including after a state's cost decreases.
    return left < right || (left == right && a < b);
}
void MatronRoutecraft::update_open(int index) {
    auto &state = search_.states[index];
    unsigned at;
    if (state.open < 0) at = search_.open_count++;
    else at = unsigned(state.open);
    while (at > 0) {
        const unsigned parent = (at - 1) / 2;
        const int previous = search_.open[parent];
        if (!earlier(index, previous)) break;
        search_.open[at] = previous;
        search_.states[previous].open = int(at);
        at = parent;
    }
    search_.open[at] = index;
    state.open = int(at);
}
int MatronRoutecraft::take_open() {
    if (search_.open_count == 0) return -1;
    const int result = search_.open[0];
    search_.states[result].open = -1;
    const int last = search_.open[--search_.open_count];
    if (search_.open_count > 0) {
        unsigned at = 0;
        while (2 * at + 1 < search_.open_count) {
            unsigned child = 2 * at + 1;
            if (child + 1 < search_.open_count && earlier(search_.open[child + 1], search_.open[child])) ++child;
            if (!earlier(search_.open[child], last)) break;
            search_.open[at] = search_.open[child];
            search_.states[search_.open[at]].open = int(at);
            at = child;
        }
        search_.open[at] = last;
        search_.states[last].open = int(at);
    }
    return result;
}
void MatronRoutecraft::start_neighbors(const Node &from, int radius) {
    const Cell origin = cell(from.center);
    const int cx = std::get<0>(origin), cy = std::get<1>(origin), cz = std::get<2>(origin);
    search_.neighbor_count = 0;
    for (int x = cx - radius; x <= cx + radius; ++x) {
        for (int y = cy - radius; y <= cy + radius; ++y) {
            for (int z = cz - radius; z <= cz + radius; ++z) {
                const auto found = search_.cells.find(Cell(x, y, z));
                if (found == search_.cells.end()) continue;
                const unsigned bucket = found->second;
                search_.neighbors[search_.neighbor_count++] = {search_.buckets[bucket][0], bucket, 0};
            }
        }
    }
    const auto later = [](const Neighbor &a, const Neighbor &b) { return a.candidate > b.candidate; };
    std::make_heap(search_.neighbors.begin(), search_.neighbors.begin() + search_.neighbor_count, later);
}
void MatronRoutecraft::consume_neighbor() {
    const auto later = [](const Neighbor &a, const Neighbor &b) { return a.candidate > b.candidate; };
    auto end = search_.neighbors.begin() + search_.neighbor_count;
    std::pop_heap(search_.neighbors.begin(), end, later);
    Neighbor &head = *(end - 1);
    const auto &bucket = search_.buckets[head.bucket];
    if (++head.offset < bucket.size()) {
        head.candidate = bucket[head.offset];
        std::push_heap(search_.neighbors.begin(), end, later);
    } else --search_.neighbor_count;
}
void MatronRoutecraft::advance_plan(PhysicsSystem &p, BodyID self, unsigned expansion_budget,
    unsigned transition_budget, unsigned &credits) {
    if (expansion_budget == 0 || transition_budget == 0 || credits == 0) return;
    // An explicitly unlimited reference search also removes the cheap-work
    // slice cap. Normal and query-limited calls retain the fixed bound.
    constexpr unsigned unlimited = std::numeric_limits<unsigned>::max();
    const unsigned neighbor_budget = expansion_budget == unlimited && transition_budget == unlimited && credits == unlimited
        ? unlimited : kPlanningNeighborChecksPerTick;
    if (report_.phase == PlanPhase::Validate) {
        // Sampled validation can stop between any two logical queries. Keep
        // the live publication barrier separate from this cached work.
        if (search_.validated < route_.size()) {
            constexpr unsigned kValidationEdgesPerTick = 4;
            const unsigned edge_budget = expansion_budget == std::numeric_limits<unsigned>::max()
                ? expansion_budget : std::min(expansion_budget, kValidationEdgesPerTick);
            for (unsigned checked = 0; checked < edge_budget && search_.validated < route_.size(); ++checked) {
                const std::size_t i = search_.validated;
                const RVec3 from = i == 0 ? p.GetBodyInterface().GetPosition(self) : route_[i - 1].center;
                if (!search_.transfer.started && !valid(p, route_[i].grip)) { replan(p, self); return; }
                const auto result = resume_transition(p, self, from, route_[i], search_.transfer, credits, false);
                if (result == ProbeResult::Pending) return;
                if (result == ProbeResult::Failed) { replan(p, self); return; }
                search_.transfer = {};
                ++search_.validated;
            }
            return;
        }
        // Recheck every grip and body edge together: geometry may have
        // changed after an earlier validation slice. At most kMaxNodes-1
        // edges; execution also checks the first transfer from the live body.
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
    unsigned transitions = search_.candidate >= 0 ? 1 : 0;
    // Resuming an expansion spends a slot too. The neighbor heap contains
    // only cell heads, so no slice first gathers/sorts a whole neighborhood.
    if (search_.current >= 0) report_.slice_expansions = 1;
    while (true) {
        auto &states = search_.states;
        if (search_.current < 0) {
            if (report_.slice_expansions >= expansion_budget || transitions >= transition_budget || credits == 0) return;
            const int current = search_.expansions < kMaxNodes ? take_open() : -1;
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
                search_.transfer = {};
                report_.phase = PlanPhase::Validate;
                return;
            }
            start_neighbors(from, current == 0 ? 3 : 1);
        }
        const int current = search_.current;
        const Node from = states[current].node;
        const float edge_limit = current == 0 ? 3.2F : kCellSize;
        while (search_.candidate >= 0 || search_.neighbor_count > 0) {
            if (search_.candidate < 0) {
                if (report_.slice_neighbor_checks == neighbor_budget) return;
                ++report_.slice_neighbor_checks;
                const int candidate = search_.neighbors[0].candidate;
                const Node &to = search_.catalog[candidate];
                const float length = Vec3(to.center - from.center).Length();
                const int index = search_.admitted[candidate];
                const float cost = states[current].cost + length;
                if (length > edge_limit || Vec3(to.grip.point - from.center).Length() > kReach ||
                    Vec3(to.grip.point - to.center).Length() > kReach || blocked(from.center, to.center) ||
                    (index >= 0 && (states[index].closed || cost >= states[index].cost))) {
                    consume_neighbor();
                    continue;
                }
                // Cached refusals keep their original logical charge, even
                // though they need no native query and no candidate slot.
                if (search_.posture_valid[candidate] == 0) {
                    if (credits == 0) return;
                    --credits;
                    consume_neighbor();
                    if (!budget()) { finish_search(Status::BudgetExceeded); return; }
                    continue;
                }
                if (transitions == transition_budget || credits == 0) return;
                ++transitions;
                consume_neighbor();
                search_.candidate = candidate;
                search_.candidate_phase = CandidatePhase::Charge;
            }
            const int candidate = search_.candidate;
            const Node &to = search_.catalog[candidate];
            if (search_.candidate_phase == CandidatePhase::Charge) {
                if (credits == 0) return;
                --credits;
                if (!budget()) { finish_search(Status::BudgetExceeded); return; }
                search_.candidate_phase = search_.posture_valid[candidate] < 0
                    ? CandidatePhase::PostureClear : CandidatePhase::Transfer;
            }
            if (search_.candidate_phase == CandidatePhase::PostureClear) {
                if (credits == 0) return;
                --credits;
                if (!clear(p, self, to.center, to.center)) {
                    if (report_.queries > 200000) { finish_search(Status::BudgetExceeded); return; }
                    search_.posture_valid[candidate] = 0;
                    search_.candidate = -1;
                    continue;
                }
                search_.candidate_phase = CandidatePhase::PostureReach;
            }
            if (search_.candidate_phase == CandidatePhase::PostureReach) {
                if (credits == 0) return;
                --credits;
                search_.posture_valid[candidate] = reach(p, self, to.center, to.grip);
                if (!search_.posture_valid[candidate]) {
                    if (report_.queries > 200000) { finish_search(Status::BudgetExceeded); return; }
                    search_.candidate = -1;
                    continue;
                }
                search_.candidate_phase = CandidatePhase::Transfer;
            }
            const auto result = resume_transition(p, self, from.center, to, search_.transfer, credits, true);
            if (result == ProbeResult::Pending) return;
            if (report_.queries > 200000) { finish_search(Status::BudgetExceeded); return; }
            search_.candidate = -1;
            search_.transfer = {};
            if (result == ProbeResult::Failed) continue;
            const float cost = states[current].cost + Vec3(to.center - from.center).Length();
            int index = search_.admitted[candidate];
            if (index < 0) {
                if (states.size() == kMaxNodes) { search_.node_budget_exhausted = true; continue; }
                index = int(states.size());
                search_.admitted[candidate] = index;
                states.push_back({to, current, cost, false});
            } else {
                states[index].parent = current;
                states[index].cost = cost;
            }
            update_open(index);
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
            settings, at, contacts, p.GetDefaultBroadPhaseLayerFilter(query_layer_),
            p.GetDefaultLayerFilter(query_layer_), SolidFilter(self));
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
void MatronRoutecraft::step(PhysicsSystem &p, BodyID self, float dt, unsigned expansion_budget,
    unsigned transition_budget, unsigned query_budget) {
    if (!planning() && !owns_motion()) return;
    report_.force=0; report_.reaction=Vec3::sZero(); report_.slice_expansions=0;
    report_.slice_queries=0; report_.slice_neighbor_checks=0;
    if (dt <= 0 || dt > 1.0F/89 || !p.GetBodyInterface().IsAdded(self)) { cancel(); return; }
    if (!planning()) elapsed_ += dt; // retain the movement timeout, independent of slice size
    if (elapsed_ > 120) { cancel(); return; }
    auto &bodies=p.GetBodyInterface();
    query_layer_ = bodies.GetObjectLayer(self);
    const RVec3 at=bodies.GetPosition(self);
    if (planning()) {
        ++report_.planning_ticks;
        unsigned credits = query_budget;
        advance_plan(p, self, expansion_budget, transition_budget, credits);
        report_.slice_queries = query_budget - credits;
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
    if (report_.published_after_ticks == 0) report_.published_after_ticks = report_.planning_ticks;
    holding_ = Node{at, target.grip};
    drive(p, self, target);
    if (Vec3(at-last_).Length()>0.03F) { last_=at; stall_=0; } else if (report_.status!=Status::Holding) stall_+=dt;
}
}
