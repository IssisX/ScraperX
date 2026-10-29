// The Matron's journeys over the tower's steel (matron.hpp, travel_to): a
// plan over the scanned map (matron_traversal.hpp), walked by her own legs
// between moves and climbed by her own hands, each move physical. A region is
// hers only when she stands on it; a failed move is a fall, and she plans
// again from where she lands. Nothing here sets her position.
#include "sim/matron.hpp"

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace scraperx::sim {

using namespace JPH;

namespace {
constexpr int kMaxReplans = 6;
constexpr float kLandWaitS = 6.0F;
constexpr float kArrivalStandS = 1.0F;
constexpr float kSupportProbeM = 0.03F;
constexpr float kLandedVerticalMps = 0.30F;
constexpr float kArrivalStillMps = 0.30F;
constexpr float kTwoPi = 6.28318530718F;
constexpr float kTurnRate = 7.0F;   // as matron.cpp

class SupportFilter final : public BodyFilter {
public:
    SupportFilter(const BodyID self, const BodyID player) : self_(self), player_(player) {}
    bool ShouldCollide(const BodyID &id) const override { return id != self_ && id != player_; }
    bool ShouldCollideLocked(const Body &body) const override {
        return !body.IsSensor() && body.GetID() != self_ && body.GetID() != player_;
    }

private:
    BodyID self_, player_;
};

float turn_toward(const float yaw, const float want, const float rate, const float dt) {
    float delta = std::remainder(want - yaw, kTwoPi);
    const float step = rate * dt;
    delta = std::clamp(delta, -step, step);
    return std::remainder(yaw + delta, kTwoPi);
}

float distance_xz(const float ax, const float az, const float bx, const float bz) {
    return std::hypot(ax - bx, az - bz);
}
} // namespace

void Matron::set_routes(const traversal::AffordanceMap *routes) { scanned_ = routes; }

int Matron::region_under() const {
    if (scanned_ == nullptr || !body_in_world_ || lying_ || kneeling_) {
        return -1;
    }
    // A floor within the standing ray's lookahead is not a landing. Query
    // her actual shape at contact tolerance, and accept only upward support
    // after her vertical motion relative to that support has settled.
    AllHitCollisionCollector<CollideShapeCollector> hits;
    RVec3 at, base;
    Vec3 velocity;
    {
        const BodyLockRead lock(physics_.GetBodyLockInterface(), body_);
        if (!lock.Succeeded()) return -1;
        const Body &her = lock.GetBody();
        at = her.GetPosition();
        base = her.GetCenterOfMassPosition();
        velocity = her.GetLinearVelocity();
        const SupportFilter supports(body_, player_);
        physics_.GetNarrowPhaseQuery().CollideShape(
            her.GetShape(), Vec3::sReplicate(1.0F),
            her.GetCenterOfMassTransform().PostTranslated(Vec3(0.0F, -kSupportProbeM, 0.0F)),
            CollideShapeSettings(), base, hits,
            physics_.GetDefaultBroadPhaseLayerFilter(her.GetObjectLayer()),
            physics_.GetDefaultLayerFilter(her.GetObjectLayer()), supports);
    }
    int best = -1;
    float best_gap = std::numeric_limits<float>::max();
    for (const CollideShapeResult &hit : hits.mHits) {
        if (hit.mPenetrationAxis.NormalizedOr(Vec3::sZero()).GetY() > -0.7F) continue;
        const RVec3 contact = base + hit.mContactPointOn2;
        {
            const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mBodyID2);
            if (!lock.Succeeded() || !lock.GetBody().IsInBroadPhase() || lock.GetBody().IsSensor() ||
                std::abs((velocity - lock.GetBody().GetPointVelocity(contact)).GetY()) > kLandedVerticalMps)
                continue;
        }
        const float top = float(contact.GetY());
        int region = scanned_->region_at(Vec3(float(at.GetX()), top, float(at.GetZ())));
        if (region < 0) {
            // On a narrow floor her middle may overhang its outline. The
            // real capsule contact, not a projected middle, names her floor.
            region = scanned_->region_at(Vec3(contact));
        }
        if (region < 0) {
            // A small plinth or foot plate can support her without being a
            // region of its own. Keep the existing nearby-region allowance,
            // but measure it from this real support rather than a ray ahead.
            float best_d = 0.6F;
            const float x = float(contact.GetX()), z = float(contact.GetZ());
            for (const traversal::Region &r : scanned_->regions()) {
                if (x < r.min_x - 0.5F || x > r.max_x + 0.5F || z < r.min_z - 0.5F || z > r.max_z + 0.5F ||
                    top < r.min_y - 0.6F || top > r.max_y + 0.6F) continue;
                const float d = std::abs(r.top - top);
                if (d < best_d || (d == best_d && (region < 0 || r.id < region))) {
                    best_d = d;
                    region = r.id;
                }
            }
        }
        const float gap = std::abs(float(at.GetY()) - kStandCentre - top);
        if (region >= 0 && (gap < best_gap || (gap == best_gap && (best < 0 || region < best)))) {
            best_gap = gap;
            best = region;
        }
    }
    return best;
}

bool Matron::travel_to(const Vec3 feet) {
    if (!body_in_world_ || dead_ || !std::isfinite(feet.GetX()) || !std::isfinite(feet.GetY()) ||
        !std::isfinite(feet.GetZ())) return false;
    if (wants_routes()) {
        own_scan_ = std::make_unique<traversal::AffordanceMap>();
        own_scan_->scan(physics_, physics_.GetBounds(), {body_, BodyID(), moving_layer()});
        scanned_ = own_scan_.get();
    }
    const int region = scanned_->region_at(feet);
    return region >= 0 && travel_to(region);
}

bool Matron::travel_to(const int region) {
    return travel_to(region, {});
}

bool Matron::travel_to(const int region, const std::vector<int> &avoid) {
    if (scanned_ == nullptr || !body_in_world_ || dead_ || lying_ || kneeling_ ||
        region < 0 || static_cast<std::size_t>(region) >= scanned_->regions().size()) {
        return false;
    }
    const int here = region_under();
    // Off any floor she knows, a way that moves her body keeps it, as her
    // cling, her hold on a load and her bracing do.
    const Mover *way = holder();
    if (here < 0 && ((way != nullptr && way->view().drives) || riding_ || held_ >= 0 || bracing_structure())) {
        return false;
    }
    // A new destination cannot take ownership away from hands or feet
    // already executing a move, including a jump between supports.
    if ((climber_ != nullptr && (climber_->status() == traversal::Climber::Status::Running ||
                                 climber_->view().holding)) ||
        pipe_phase_ != PipePhase::Off) {
        return false;
    }
    // Accept a free body's destination while she is airborne. A plan from
    // her last real support can describe the work ahead, but remains
    // provisional: region stays unknown and step_travel waits under gravity,
    // then replans from her actual landing before executing any move.
    std::vector<int> route;
    const int from = here >= 0 ? here : travel_.region;
    if (from >= 0) route = plan_with_risers(from, region, avoid);
    if (here >= 0 && route.empty() && here != region) {
        return false;
    }
    // Her hands on this body (a respawn gives her a new one). Invalid or
    // unreachable requests above leave her existing controller untouched.
    climber_ = std::make_unique<traversal::Climber>(physics_, body_, traversal::Ignore{body_, player_, static_cast<int>(moving_layer_)});
    travel_avoid_ = avoid;
    travel_edges_ = std::move(route);
    travel_.active = true;   // already there still earns a supported, quiet arrival
    chase_way_held_ = true;  // a travel taken holds the chase window past its own ticks
    travel_.goal = region;
    travel_.region = here;
    travel_.step = 0;
    travel_.steps = static_cast<int>(travel_edges_.size());
    travel_walk_.clear();
    travel_walk_i_ = 0;
    travel_wp_best_ = 1.0e9F;
    travel_wp_stall_s_ = 0.0F;
    travel_rerouted_ = false;
    travel_stuck_ = 0;
    travel_still_at_ = Vec3(physics_.GetBodyInterface().GetPosition(body_)) - Vec3(0.0F, kStandCentre, 0.0F);
    travel_still_s_ = 0.0F;
    travel_yield_s_ = 0.0F;
    travel_wait_s_ = 0.0F;
    travel_replans0_ = travel_.replans;   // her counts run on; this journey's replans from here
    travel_riser_ = false;
    travel_riser_wait_s_ = 0.0F;
    climber_->cancel();
    drop_stock();
    return true;
}

void Matron::step_travel(const float dt) {
    BodyInterface &bodies = physics_.GetBodyInterface();
    const RVec3 at = bodies.GetPosition(body_);
    speed_ = 0.0F;
    // On a riser of hers (matron_pipe.cpp): her hands and feet on it, nothing
    // else; off it, on along the plan from the deck it took her to, or (her
    // hands gone) a plan again from where she lands.
    if (travel_riser_) {
        if (pipe_phase_ != PipePhase::Off) {
            ride_pipe(dt);
            travel_.move = pipe_to_ < pipe_from_ ? traversal::MoveKind::RiserDown : traversal::MoveKind::RiserUp;
            return;
        }
        travel_riser_ = false;
        travel_wait_s_ = 0.0F;
        if (pipe_failed_ || travel_.step >= travel_.steps) {
            ++travel_.moves_failed;
            travel_.last_failure = traversal::Failure::LostHold;
            travel_.step = travel_.steps;
            return;
        }
        ++travel_.moves_done;
        travel_.region = travel_edge(travel_edges_[static_cast<std::size_t>(travel_.step)]).to;
        ++travel_.step;
        travel_walk_.clear();
        return;
    }
    // The player on a climb she means to take (on it, at its foot, or at
    // its head), as she saw, heard or felt him there just now: the way is
    // theirs. Any climb, from its own line and span.
    const auto player_on = [&](const traversal::MoveSpec &spec) {
        RVec3 p;
        if (!knows_where(kYieldMemoryS, p)) {
            return false;
        }
        const float lo = std::min(spec.bottom_y, spec.start.GetY()) - 2.5F;
        const float hi = std::max(spec.top_y, spec.goal_top) + 0.5F;
        return distance_xz(float(p.GetX()), float(p.GetZ()), spec.line.GetX(), spec.line.GetZ()) < kYieldReachM &&
               float(p.GetY()) > lo && float(p.GetY()) < hi;
    };
    // A move not yet off her floor gives way the same as one not begun.
    if (climber_->status() == traversal::Climber::Status::Running && climber_->phase() == 0 &&
        travel_.step < travel_.steps &&
        player_on(travel_edge(travel_edges_[static_cast<std::size_t>(travel_.step)]).spec)) {
        climber_->cancel();
    }
    // A move under way: her hands and feet on it, nothing else.
    const bool moving = climber_->status() == traversal::Climber::Status::Running;
    if (moving) {
        travel_wait_s_ = 0.0F;
        climber_->step(dt);
        travel_.move = climber_->view().kind;
        const Vec3 v = bodies.GetLinearVelocity(body_);
        if (std::abs(v.GetX()) + std::abs(v.GetZ()) > 0.2F) {
            yaw_ = turn_toward(yaw_, std::atan2(v.GetX(), v.GetZ()), kTurnRate, dt);
        }
        stride_ += v.Length() * dt;
    }
    if (climber_->status() == traversal::Climber::Status::Done) {
        // A move can release its last hold within landing reach. Keep its
        // destination pending until real support, without throwing away a
        // successful route or spending a replan on the final falling tick.
        const int under = region_under();
        if (under < 0) {
            travel_wait_s_ = std::min(travel_wait_s_, 0.0F) - dt;
            if (travel_wait_s_ < -kLandWaitS) {
                travel_.active = false;
                climber_->cancel();
            }
            return;
        }
        ++travel_.moves_done;
        const traversal::Edge e = travel_edge(travel_edges_[static_cast<std::size_t>(travel_.step)]);
        if (under == e.to) {
            travel_.region = under;
            ++travel_.step;
        } else {
            travel_.step = travel_.steps;   // landed elsewhere: replan from that support
        }
        travel_wait_s_ = 0.0F;
        climber_->cancel();
        return;
    }
    if (climber_->status() == traversal::Climber::Status::Failed) {
        ++travel_.moves_failed;
        travel_.last_failure = climber_->failure();
        // Failed before she left her floor (could not reach its start, no
        // hold there): that way is closed to this journey; a physical
        // failure on the way is not (it may go next time).
        if (climber_->phase() == 0 && travel_.step < travel_.steps) {
            travel_avoid_.push_back(travel_edges_[static_cast<std::size_t>(travel_.step)]);
        }
        travel_.step = travel_.steps;
        travel_wait_s_ = 0.0F;
        climber_->cancel();
        return;
    }
    if (moving) return;
    // Arrival is a full second standing on real support without a grip.
    // Keep the journey's capped standing controller in charge while she
    // brakes; ordinary idle movement cannot end the journey early.
    const int under = region_under();
    if (under == travel_.goal) {
        travel_.region = under;
        travel_.step = travel_.steps;   // support lost during arrival must replan before any further move
        const Vec3 velocity = bodies.GetLinearVelocity(body_);
        (void)climber_->stand(dt);
        const bool quiet = !climber_->view().holding && velocity.Length() < kArrivalStillMps &&
                           std::abs(velocity.GetY()) < 0.1F && bodies.GetGravityFactor(body_) == 1.0F;
        travel_wait_s_ = quiet ? std::max(travel_wait_s_, 0.0F) + dt : 0.0F;
        if (travel_wait_s_ >= kArrivalStandS) {
            ++travel_.arrivals;
            travel_.active = false;
        }
        return;
    }
    // Positive time belongs to quiet arrival; negative time to awaiting a
    // landing. Crossing between them cannot carry elapsed time forward.
    travel_wait_s_ = std::min(travel_wait_s_, 0.0F);
    // Off the plan (a fall, a move that ended elsewhere, knocked off her
    // floor partway along a walk): when she is on her feet again, plan from
    // there.
    if (travel_.region < 0 || travel_.step >= travel_.steps || (under >= 0 && under != travel_.region)) {
        if (under < 0) {
            travel_wait_s_ -= dt;
            if (travel_wait_s_ < -kLandWaitS) {
                travel_.active = false;   // nothing under her she knows: she gives it up
            }
            return;
        }
        travel_wait_s_ = 0.0F;
        if (travel_.replans - travel_replans0_ >= kMaxReplans) {
            travel_.active = false;
            return;
        }
        ++travel_.replans;
        travel_edges_ = plan_with_risers(under, travel_.goal, travel_avoid_);
        travel_.region = under;
        travel_.step = 0;
        travel_.steps = static_cast<int>(travel_edges_.size());
        travel_walk_.clear();
        travel_walk_i_ = 0;
        if (travel_edges_.empty()) {
            travel_.active = false;
        }
        return;
    }
    travel_wait_s_ = 0.0F;
    // Walk her region to the next move's start, then begin it.
    const traversal::Edge e = travel_edge(travel_edges_[static_cast<std::size_t>(travel_.step)]);
    const Vec3 feet(float(at.GetX()), float(at.GetY()) - kStandCentre, float(at.GetZ()));
    // Giving way: off the climb's foot (her body is not to stand in the
    // player's way onto it) and still until it is clear. Her yielding is
    // her own feet; the player's climbing is not touched.
    if (player_on(e.spec)) {
        ++travel_.yields;
        const Vec3 line(e.spec.line.GetX(), feet.GetY(), e.spec.line.GetZ());
        Vec3 away(feet.GetX() - line.GetX(), 0.0F, feet.GetZ() - line.GetZ());
        const float off = away.Length();
        away = off > 1.0e-3F ? away / off : e.spec.out;
        if (off < kYieldClearM) {
            climber_->walk_between(dt, line + away * (kYieldClearM + 0.2F), 1.0F);
        } else {
            climber_->walk_between(dt, feet, 0.0F);
        }
        travel_walk_.clear();
        // Kept waiting there long: another way, if her map has one.
        travel_yield_s_ += dt;
        if (travel_yield_s_ > kYieldGiveUpS) {
            travel_yield_s_ = 0.0F;
            travel_avoid_.push_back(travel_edges_[static_cast<std::size_t>(travel_.step)]);
            travel_.step = travel_.steps;
        }
        return;
    }
    travel_yield_s_ = 0.0F;
    const float to_start = distance_xz(feet.GetX(), feet.GetZ(), e.spec.start.GetX(), e.spec.start.GetZ());
    // Over the scanned map's walkable tops only (no authored ways): patch to
    // patch to the move's start, handing over to the move's own feet once
    // the rest of the way is floor all along and close.
    if (travel_walk_.empty()) {
        // Her feet's own search over the floor first; the map's patch links
        // for a long way (or where the search finds nothing).
        if (to_start < 40.0F) {
            travel_walk_ = traversal::grid_walk(physics_, traversal::Ignore{body_, player_, static_cast<int>(moving_layer_)}, climber_->limits(), feet,
                                                 e.spec.start);
        }
        if (travel_walk_.empty() && to_start < 15.0F) {
            // Her feet's search again on a finer grid, close round the way (a
            // narrow top, a board, a gap a coarse cell steps over).
            travel_walk_ = traversal::grid_walk(physics_, traversal::Ignore{body_, player_, static_cast<int>(moving_layer_)},
                                                climber_->limits(), feet, e.spec.start, 3.0F, 0.1F, 60000);
        }
        if (travel_walk_.empty() && to_start < 15.0F) {
            // And wide (the way round may leave the box round the two).
            travel_walk_ = traversal::grid_walk(physics_, traversal::Ignore{body_, player_, static_cast<int>(moving_layer_)},
                                                climber_->limits(), feet, e.spec.start, 16.0F, 0.2F, 150000);
        }
        if (travel_walk_.empty() && to_start < 15.0F && to_start > 0.6F) {
            // Close, and her feet find no floor that takes her there (a wall
            // standing on the deck between, found at both grain sizes): that start is not hers from
            // here. She leaves that move out and plans again.
            travel_avoid_.push_back(travel_edges_[static_cast<std::size_t>(travel_.step)]);
            travel_.step = travel_.steps;
            return;
        }
        if (travel_walk_.empty()) {
            travel_walk_ = scanned_->walk_path(feet, e.spec.start);
        }
        travel_walk_i_ = 0;
    }
    // Off her path (knocked off a board onto the floor below it, the same
    // region by the stairs): the way from here is searched again.
    if (travel_walk_i_ > 0 && climber_->supported()) {
        const Vec3 a = travel_walk_[travel_walk_i_ - 1], b = travel_walk_[travel_walk_i_];
        const float len = distance_xz(a.GetX(), a.GetZ(), b.GetX(), b.GetZ());
        const float u = len > 1.0e-3F ? std::clamp(((feet.GetX() - a.GetX()) * (b.GetX() - a.GetX()) +
                                                    (feet.GetZ() - a.GetZ()) * (b.GetZ() - a.GetZ())) / (len * len), 0.0F, 1.0F)
                                      : 0.0F;
        const float path_y = a.GetY() + u * (b.GetY() - a.GetY());
        if (path_y - feet.GetY() > 1.0F) {
            travel_walk_.clear();
            travel_walk_i_ = 0;
            return;
        }
    }
    // A waypoint is passed when she is at it, or when her feet will take her
    // no nearer (an edge her feet keep her from): on to the next.
    {
        const float to_wp = distance_xz(feet.GetX(), feet.GetZ(), travel_walk_[travel_walk_i_].GetX(),
                                        travel_walk_[travel_walk_i_].GetZ());
        if (to_wp < travel_wp_best_ - 0.05F) {
            travel_wp_best_ = to_wp;
            travel_wp_stall_s_ = 0.0F;
        } else {
            travel_wp_stall_s_ += dt;
        }
        if (travel_wp_stall_s_ > 1.5F && to_wp > 1.0F && !travel_rerouted_) {
            // Something on the floor in her way: her feet's search round it.
            std::vector<Vec3> round = traversal::grid_walk(physics_, traversal::Ignore{body_, player_, static_cast<int>(moving_layer_)},
                                                            climber_->limits(), feet, travel_walk_[travel_walk_i_], 3.0F);
            travel_rerouted_ = true;
            if (!round.empty()) {
                travel_walk_.erase(travel_walk_.begin(), travel_walk_.begin() + static_cast<long>(travel_walk_i_));
                travel_walk_.insert(travel_walk_.begin(), round.begin(), round.end() - 1);
                travel_walk_i_ = 0;
                travel_wp_best_ = 1.0e9F;
                travel_wp_stall_s_ = 0.0F;
            }
        }
        if ((feet - travel_still_at_).Length() > 0.15F) {
            travel_still_at_ = feet;
            travel_still_s_ = 0.0F;
            travel_stuck_ = 0;   // she is getting somewhere
        } else {
            travel_still_s_ += dt;
        }
        if ((travel_wp_stall_s_ > 6.0F && to_wp > 1.0F) || travel_still_s_ > 6.0F) {
            travel_still_s_ = 0.0F;
            // Held fast where her feet's search sent her: a fresh search from
            // where she is; and when that keeps holding her, that move's way
            // is not hers from here: leave it out and plan again.
            travel_wp_stall_s_ = 0.0F;
            travel_wp_best_ = 1.0e9F;
            travel_rerouted_ = false;
            travel_walk_.clear();
            travel_walk_i_ = 0;
            if (++travel_stuck_ >= 3) {
                travel_stuck_ = 0;
                travel_avoid_.push_back(travel_edges_[static_cast<std::size_t>(travel_.step)]);
                travel_.step = travel_.steps;
            }
            return;
        }
        if (travel_walk_i_ + 1 < travel_walk_.size() && (to_wp < 0.35F || (to_wp < 1.5F && travel_wp_stall_s_ > 0.8F))) {
            travel_rerouted_ = false;
            ++travel_walk_i_;
            travel_wp_best_ = 1.0e9F;
            travel_wp_stall_s_ = 0.0F;
        }
    }
    const bool last_leg = travel_walk_i_ + 1 >= travel_walk_.size();
    if (!(last_leg && to_start < 1.6F)) {
        Vec3 w = travel_walk_[travel_walk_i_];
        if (travel_walk_i_ > 0) {
            // Along the leg, not across to its end: her aim a stride ahead
            // of where she is on it (a stair's line taken straight, not cut).
            const Vec3 a = travel_walk_[travel_walk_i_ - 1];
            const float len = distance_xz(a.GetX(), a.GetZ(), w.GetX(), w.GetZ());
            if (len > 0.8F) {
                const float u = std::clamp(((feet.GetX() - a.GetX()) * (w.GetX() - a.GetX()) +
                                            (feet.GetZ() - a.GetZ()) * (w.GetZ() - a.GetZ())) / (len * len) + 0.6F / len,
                                           0.0F, 1.0F);
                w = a + (w - a) * u;
            }
        }
        const float off = distance_xz(feet.GetX(), feet.GetZ(), w.GetX(), w.GetZ());
        // Her own capped feet (the one budget), keeping to floor.
        // Her feet's search found that waypoint down a drop she walks off
        // (under 1 m): she may leave the edge for it.
        const float drop = feet.GetY() - travel_walk_[travel_walk_i_].GetY();   // the waypoint's own floor, not the aim along the leg
        climber_->walk_between(dt, w, std::min(2.4F, 0.6F + 1.5F * off),
                               drop > climber_->limits().step_max_m + 0.05F ? std::min(1.1F, drop + 0.1F) : 0.0F);
        if (off > 0.15F) {
            yaw_ = turn_toward(yaw_, std::atan2(w.GetX() - feet.GetX(), w.GetZ() - feet.GetZ()), kTurnRate, dt);
        }
        return;
    }
    if (e.kind == traversal::MoveKind::RiserUp || e.kind == traversal::MoveKind::RiserDown) {
        // A riser: right to her stand by it, then her hands onto it (the
        // edge's top, or from a floor her line stands on, its face).
        const int way = -travel_edges_[static_cast<std::size_t>(travel_.step)] - 1;
        pipe_route_ = way / 2;
        if (to_start > 0.2F) {
            climber_->walk_between(dt, e.spec.start, std::min(1.0F, 0.3F + 1.5F * to_start));
            yaw_ = turn_toward(yaw_, std::atan2(e.spec.start.GetX() - feet.GetX(), e.spec.start.GetZ() - feet.GetZ()), kTurnRate, dt);
            return;
        }
        (void)climber_->stand(dt);
        climb_retry_s_ = 0.0F;
        if (get_on_pipe(e.kind == traversal::MoveKind::RiserUp ? e.spec.bottom_y : e.spec.top_y,
                        e.kind == traversal::MoveKind::RiserUp ? e.spec.top_y : e.spec.bottom_y)) {
            travel_riser_ = true;
            travel_riser_wait_s_ = 0.0F;
            travel_walk_.clear();
            travel_.move = e.kind;
            return;
        }
        // No hold there for her hands a while: not her way from here.
        travel_riser_wait_s_ += dt;
        if (travel_riser_wait_s_ > 3.0F) {
            travel_riser_wait_s_ = 0.0F;
            ++travel_.moves_failed;
            travel_.last_failure = traversal::Failure::NoHold;
            travel_avoid_.push_back(travel_edges_[static_cast<std::size_t>(travel_.step)]);
            travel_.step = travel_.steps;
        }
        return;
    }
    travel_walk_.clear();
    travel_.move = e.kind;
    climber_->begin(e.spec);
}

std::vector<int> Matron::plan_with_risers(const int from, const int goal, const std::vector<int> &avoid) {
    std::vector<int> direct = scanned_->plan(from, goal, avoid);
    if (!direct.empty() || from == goal || pipe_ruled_out_) {
        return direct;
    }
    // Where the steel offers her map no way: her risers beside it, one graph: a route takes
    // as many of them as the way needs (a tall riser climbed deck to deck),
    // each walked to and from over her map.
    std::vector<std::pair<int, traversal::Edge>> risers;
    const auto &routes = riser_routes();
    for (std::size_t i = 0; i < routes.size(); ++i) {
        for (int down = 0; down < 2; ++down) {
            const int way = 2 * static_cast<int>(i) + down;
            if (!pipe_way_open(way)) continue;
            const RiserDeck &a = down != 0 ? routes[i].hi : routes[i].lo;
            const RiserDeck &b = down != 0 ? routes[i].lo : routes[i].hi;
            if (a.region < 0 || b.region < 0 || a.region == b.region) continue;
            risers.emplace_back(-way - 1, travel_edge(-way - 1));
        }
    }
    return scanned_->plan(from, goal, avoid, risers);
}

traversal::Edge Matron::travel_edge(const int k) {
    if (k >= 0) {
        return scanned_->edges()[static_cast<std::size_t>(k)];
    }
    // One of her risers, one way (riser_routes).
    const int way = -k - 1;
    const RiserRoute &r = riser_routes()[static_cast<std::size_t>(way / 2)];
    const bool down = (way % 2) != 0;
    const RiserDeck &a = down ? r.hi : r.lo;
    const RiserDeck &b = down ? r.lo : r.hi;
    traversal::Edge e;
    e.from = a.region;
    e.to = b.region;
    e.kind = down ? traversal::MoveKind::RiserDown : traversal::MoveKind::RiserUp;
    e.spec.kind = e.kind;
    e.spec.line = riser_line(r, 0.2F);
    e.spec.out = r.out;
    e.spec.bottom_y = r.lo.floor_y;
    e.spec.top_y = r.hi.floor_y;
    e.spec.start = a.stand;
    e.spec.goal = b.stand;
    e.spec.goal_top = b.floor_y;
    e.cost = 5.0F + 2.0F * (r.hi.floor_y - r.lo.floor_y);
    return e;
}

// Her travel as a way of hers (matron_mover.hpp): to the lead's place (begin),
// then its walks and moves after her clocks and her floor, nothing of the
// hunt meanwhile.
bool Matron::TravelWay::begin(const MoverGoal &goal) {
    goal_ = goal;
    return m_.travel_to(Vec3(goal.x, goal.floor_y, goal.z));
}

MoverStatus Matron::TravelWay::step(const float dt, traversal::Budget & /*budget*/) {
    if (!m_.travel_.active) {
        return MoverStatus::Idle;
    }
    m_.step_travel(dt);
    return MoverStatus::Running;
}

void Matron::TravelWay::cancel() {
    m_.travel_.active = false;
    m_.travel_riser_ = false;   // her hands on a riser keep it (her pipe way has her)
    if (m_.climber_ != nullptr) {
        m_.climber_->cancel();
    }
}

MoverStatus Matron::TravelWay::status() const {
    return m_.travel_.active ? MoverStatus::Running : MoverStatus::Idle;
}

MoverView Matron::TravelWay::view() const {
    MoverView v;
    v.tick = MoverTick::Travel;
    if (!m_.travel_.active) {
        return v;
    }
    v.holds_chase = true;
    v.blow_proof = true;
    if (m_.travel_riser_ && m_.pipe_phase_ != PipePhase::Off) {
        // On a riser her hands hold her against her weight, as in her hunt
        // (PipeWay::view): her floor is in her hands, a blow is read.
        v.drives = true;
        v.held = true;
        v.lands = true;
        v.blow_proof = m_.pipe_phase_ == PipePhase::Slide || m_.pipe_phase_ == PipePhase::Stunned;
        v.floor_in_hands = true;
        v.drive_dv = m_.pipe_drive_dv_;
        v.force_shown = true;
        v.force_n = m_.pipe_force_n_;
    }
    if (m_.travel_.goal >= 0 && m_.scanned_ != nullptr &&
        m_.travel_.goal < static_cast<int>(m_.scanned_->regions().size())) {
        v.has_goal = true;
        v.goal_floor_y = m_.scanned_->regions()[static_cast<std::size_t>(m_.travel_.goal)].top;
    }
    // The move her travel is making, shown as her journey's is (her drawn
    // body poses the move her climber is in; a hand only once it holds).
    if (m_.climber_ != nullptr && m_.climber_->status() == traversal::Climber::Status::Running &&
        m_.climber_->view().active) {
        const traversal::ClimbView &climber = m_.climber_->view();
        v.move_active = true;
        v.move = climber.kind;
        v.phase = climber.phase;
        v.airborne = climber.airborne;
        v.sliding = climber.sliding;
        v.leg_driving = climber.leg_driving;
        if (climber.holding) {
            v.hands = true;
            v.hand_shown = true;
            v.hand = climber.hand;
            v.force_shown = true;
            v.force_n = climber.force_n;
            v.hand_held = true;
        }
    }
    return v;
}

} // namespace scraperx::sim
