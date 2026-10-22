// Her own way between levels (the climbing lab, issue #3): no table of
// ladders. She looks round her in the world itself for bars a hand closes
// round, lying level (physics_world_support.hpp box_is_hold), and stacks them
// into columns. A column that runs from her floor toward her lead's, with
// real floor she fits on at its foot and at its head, is a way; her own hands
// take it (matron_routecraft.cpp: finite force, gravity on). A column her
// hands refuse is left alone a while (a search's refusal, not a fall), and
// she looks again. Only her own collision layer's bodies count.

#include "sim/matron.hpp"
#include "sim/physics_world_support.hpp"

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace scraperx::sim {
using namespace JPH;

namespace {

constexpr float kSeekM = 14.0F;          // how far round her she looks for bars
constexpr float kColumnCellM = 0.75F;    // members this close in plan are one column
constexpr int kMinMembers = 3;           // a column is a way only with three or more holds
constexpr int kMaxColumns = 32;
constexpr float kFootOutM = 0.6F;        // where she stands off a column's line at its foot
constexpr float kHeadOutM[3] = {0.45F, 0.6F, 0.75F};   // where she may stand over its head, nearest first
constexpr float kFirstHoldReachM = 2.4F; // the lowest hold within her standing stretch
constexpr float kAtFootM = 0.3F;

// Overflow rejects the whole query (no order-dependent prefix).
class Bounded final : public CollideShapeCollector {
public:
    static constexpr std::size_t kMax = 2048;
    std::vector<CollideShapeResult> hits;
    bool overflow = false;
    void AddHit(const CollideShapeResult &hit) override {
        if (hits.size() == kMax) {
            overflow = true;
            ForceEarlyOut();
        } else {
            hits.push_back(hit);
        }
    }
};

struct Column {
    float x = 0.0F, z = 0.0F;
    float min_y = 1.0e9F, max_y = -1.0e9F;
    Vec3 across = Vec3::sZero();   // level, square to the members: the two sides she can be on
    int count = 0;
    std::int64_t key = 0;
};

constexpr float kTurnRate = 7.0F;   // as matron.cpp's
constexpr int kFindEveryTicks = 45;  // a look round for bars twice a second, not every tick

float turn_toward(const float yaw, const float want, const float rate, const float dt) {
    float delta = std::remainder(want - yaw, 2.0F * detail::kPi);
    const float step = rate * dt;
    delta = std::clamp(delta, -step, step);
    return std::remainder(yaw + delta, 2.0F * detail::kPi);
}

float distance_xz(const float ax, const float az, const float bx, const float bz) {
    return std::hypot(ax - bx, az - bz);
}

std::int64_t cell_key(const float x, const float z) {
    const auto cx = static_cast<std::int64_t>(std::floor(x / kColumnCellM));
    const auto cz = static_cast<std::int64_t>(std::floor(z / kColumnCellM));
    return cx * 1000003LL + cz;
}

} // namespace

// Real floor under (x, z) between `from_y` and `to_y` (from above), level
// enough to stand on, with her capsule clear standing there.
bool Matron::found_floor(const float x, const float z, const float from_y, const float to_y, float &floor_y) const {
    const NarrowPhaseQuery &query = physics_.GetNarrowPhaseQuery();
    const auto broad = physics_.GetDefaultBroadPhaseLayerFilter(static_cast<ObjectLayer>(moving_layer_));
    const auto layer = physics_.GetDefaultLayerFilter(static_cast<ObjectLayer>(moving_layer_));
    const IgnoreSingleBodyFilter not_her(body_);
    const RRayCast ray(RVec3(x, from_y, z), Vec3(0.0F, to_y - from_y, 0.0F));
    RayCastResult hit;
    if (!query.CastRay(ray, hit, broad, layer, not_her)) {
        return false;
    }
    const RVec3 point = ray.GetPointOnRay(hit.mFraction);
    {
        const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mBodyID);
        if (!lock.Succeeded() || lock.GetBody().IsSensor() ||
            lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, point).GetY() < 0.7F) {
            return false;
        }
    }
    floor_y = float(point.GetY());
    const CapsuleShape capsule(kHalfHeight, kRadius);
    AllHitCollisionCollector<CollideShapeCollector> overlap;
    const RVec3 centre(x, floor_y + kStandCentre + 0.02F, z);
    query.CollideShape(&capsule, Vec3::sReplicate(1.0F), RMat44::sTranslation(centre), CollideShapeSettings(), centre,
                       overlap, broad, layer, not_her);
    for (const CollideShapeResult &h : overlap.mHits) {
        if (h.mPenetrationDepth > 0.02F) {
            return false;
        }
    }
    return true;
}

bool Matron::find_climb(const RVec3 at, FoundClimb &out) const {
    const float feet = float(at.GetY()) - kStandCentre;
    const bool up = lead_floor_ > feet;
    const float low = std::min(feet, lead_floor_) - 3.0F;
    const float high = std::max(feet, lead_floor_) + 4.0F;
    const RVec3 middle(at.GetX(), 0.5F * (low + high), at.GetZ());
    const BoxShape region(Vec3(kSeekM, 0.5F * (high - low), kSeekM));
    Bounded found;
    const auto broad = physics_.GetDefaultBroadPhaseLayerFilter(static_cast<ObjectLayer>(moving_layer_));
    const auto layer = physics_.GetDefaultLayerFilter(static_cast<ObjectLayer>(moving_layer_));
    const IgnoreSingleBodyFilter not_her(body_);
    physics_.GetNarrowPhaseQuery().CollideShape(&region, Vec3::sReplicate(1.0F), RMat44::sTranslation(middle),
                                                CollideShapeSettings(), middle, found, broad, layer, not_her);
    if (found.overflow) {
        return false;
    }
    // The level bars, by column.
    std::array<Column, kMaxColumns> columns{};
    int used = 0;
    for (const CollideShapeResult &hit : found.hits) {
        const BodyLockRead lock(physics_.GetBodyLockInterface(), hit.mBodyID2);
        if (!lock.Succeeded() || !lock.GetBody().IsStatic() || lock.GetBody().IsSensor()) {
            continue;
        }
        detail::LeafBox box;
        std::uint32_t axis = 0;
        if (!detail::leaf_box(lock.GetBody(), hit.mSubShapeID2, box) || !detail::box_is_hold(box, axis) ||
            std::abs(box.axes[axis].GetY()) > 0.05F) {
            continue;
        }
        const float x = float(box.centre.GetX());
        const float z = float(box.centre.GetZ());
        const float y = float(box.centre.GetY());
        const std::int64_t key = cell_key(x, z);
        int c = 0;
        while (c < used && columns[static_cast<std::size_t>(c)].key != key) {
            ++c;
        }
        if (c == used) {
            if (used == kMaxColumns) {
                continue;
            }
            ++used;
            Column &fresh = columns[static_cast<std::size_t>(c)];
            fresh.key = key;
            fresh.x = x;
            fresh.z = z;
            const Vec3 along(box.axes[axis].GetX(), 0.0F, box.axes[axis].GetZ());
            fresh.across = Vec3(-along.GetZ(), 0.0F, along.GetX()).NormalizedOr(Vec3::sAxisX());
        }
        Column &col = columns[static_cast<std::size_t>(c)];
        col.min_y = std::min(col.min_y, y);
        col.max_y = std::max(col.max_y, y);
        ++col.count;
    }
    // The nearest column that is a way toward her lead: its foot on the lower
    // floor, its head at the upper, real floor at both.
    float best = std::numeric_limits<float>::max();
    for (int i = 0; i < used; ++i) {
        const Column &col = columns[static_cast<std::size_t>(i)];
        if (col.count < kMinMembers || climb_blocked(col.key)) {
            continue;
        }
        const float lower = up ? feet : lead_floor_;
        if (col.min_y > lower + kFirstHoldReachM || col.max_y < std::min(feet, lead_floor_) + 2.0F) {
            continue;
        }
        for (const float side : {1.0F, -1.0F}) {
            const Vec3 front = col.across * side;   // her side at the foot
            float foot_y = 0.0F;
            float head_y = 0.0F;
            const float fx = col.x + front.GetX() * kFootOutM;
            const float fz = col.z + front.GetZ() * kFootOutM;
            const bool foot_ok = found_floor(fx, fz, col.min_y + 0.5F, col.min_y - kFirstHoldReachM - 0.5F, foot_y);
            float hx = col.x;
            float hz = col.z;
            bool head_ok = false;
            for (const float out_m : kHeadOutM) {
                hx = col.x - front.GetX() * out_m;
                hz = col.z - front.GetZ() * out_m;
                if (found_floor(hx, hz, col.max_y + 1.6F, col.max_y - 1.2F, head_y)) {
                    head_ok = true;
                    break;
                }
            }
            if (!foot_ok || !head_ok || head_y - foot_y < 2.0F) {
                continue;
            }
            // Toward her lead: from her floor, ending nearer the lead's.
            const float from_y = up ? foot_y : head_y;
            const float to_y = up ? head_y : foot_y;
            if (std::abs(from_y - feet) > 0.4F || std::abs(to_y - lead_floor_) >= std::abs(feet - lead_floor_) - 1.0F) {
                continue;
            }
            const float sx = up ? fx : hx;
            const float sz = up ? fz : hz;
            const float d = distance_xz(float(at.GetX()), float(at.GetZ()), sx, sz);
            if (d < best) {
                best = d;
                out.active = true;
                out.key = col.key;
                out.start_x = sx;
                out.start_z = sz;
                out.goal = RVec3(up ? hx : fx, to_y + kStandCentre + 0.02F, up ? hz : fz);
                out.facing = -front;   // into the supports from the foot's side, both ways
            }
        }
    }
    return out.active;
}

bool Matron::climb_blocked(const std::int64_t key) const {
    for (const ClimbBlock &b : climb_blocks_) {
        if (b.ticks > 0 && b.key == key) {
            return true;
        }
    }
    return false;
}

void Matron::block_climb(const std::int64_t key) {
    ClimbBlock *slot = &climb_blocks_[0];
    for (ClimbBlock &b : climb_blocks_) {
        if (b.ticks < slot->ticks) {
            slot = &b;
        }
    }
    slot->key = key;
    slot->ticks = kFoundClimbBlockTicks;
}

bool Matron::follow_found_climb(const float dt, const RVec3 at) {
    for (ClimbBlock &b : climb_blocks_) {
        b.ticks = std::max(0, b.ticks - 1);
    }
    find_wait_ticks_ = std::max(0, find_wait_ticks_ - 1);
    journey_wait_ticks_ = std::max(0, journey_wait_ticks_ - 1);
    if (!found_.active) {
        if (find_wait_ticks_ > 0) {
            return false;
        }
        find_wait_ticks_ = kFindEveryTicks;
        if (!find_climb(at, found_)) {
            // No bar column: a way she plans over the steel round her.
            const bool frail = stage_ == Stage::Pest && turn_ == Turn::None;
            const bool still = std::abs(physics_.GetBodyInterface().GetLinearVelocity(body_).GetY()) < 0.3F;
            return !frail && still && start_journey(at);
        }
    }
    const float ax = float(at.GetX());
    const float az = float(at.GetZ());
    const float off = distance_xz(ax, az, found_.start_x, found_.start_z);
    const float accel = turn_ != Turn::None ? kTurnFootAccel : kFootAccel;
    if (off > kAtFootM) {
        // To its foot (or over its head) along her lines, slowing into it.
        float wx = found_.start_x;
        float wz = found_.start_z;
        route_toward(ax, az, found_.start_x, found_.start_z, false, wx, wz);
        walk_toward(dt, wx, wz, std::min(kHuntMps, 1.0F + 3.0F * off), accel);
        return true;
    }
    walk_toward(dt, found_.start_x, found_.start_z, 0.0F, accel);
    yaw_ = turn_toward(yaw_, std::atan2(found_.facing.GetX(), found_.facing.GetZ()), kTurnRate, dt);
    // Frail, a pest takes hold only in her Turn.
    if (stage_ == Stage::Pest && turn_ == Turn::None) {
        return true;
    }
    if (request_climb_goal(found_.goal, found_.facing)) {
        routecraft_mine_ = true;
        climbing_key_ = found_.key;
        ++climbs_done_;
    } else {
        block_climb(found_.key);   // her hands refuse it: a search's refusal, left a while
    }
    found_ = FoundClimb{};
    return true;
}

} // namespace scraperx::sim

namespace scraperx::sim {

bool Matron::start_journey(const RVec3 at) {
    if (journey_wait_ticks_ > 0) {
        return false;
    }
    journey_wait_ticks_ = 2 * 90;
    const float feet = float(at.GetY()) - kStandCentre;
    const traversal::Ignore ignore{body_, player_, static_cast<int>(moving_layer_)};
    const float lo_x = std::min(float(at.GetX()), heard_x_) - 12.0F;
    const float hi_x = std::max(float(at.GetX()), heard_x_) + 12.0F;
    const float lo_z = std::min(float(at.GetZ()), heard_z_) - 12.0F;
    const float hi_z = std::max(float(at.GetZ()), heard_z_) + 12.0F;
    traversal::AffordanceMap map;
    map.scan(physics_,
             AABox(Vec3(lo_x, std::min(feet, lead_floor_) - 3.0F, lo_z), Vec3(hi_x, std::max(feet, lead_floor_) + 4.0F, hi_z)),
             ignore);
    const int from = map.region_at(Vec3(float(at.GetX()), feet, float(at.GetZ())));
    const int to = map.region_near_top(lead_floor_, 0.3F);
    if (from < 0 || to < 0 || from == to) {
        return false;
    }
    const std::vector<int> plan = map.plan(from, to);
    if (plan.empty()) {
        return false;
    }
    journey_.clear();
    for (const int e : plan) {
        journey_.push_back(map.edges()[static_cast<std::size_t>(e)]);
    }
    if (climber_ == nullptr) {
        climber_ = std::make_unique<traversal::Climber>(physics_, body_, ignore);
    }
    journey_at_ = 0;
    journey_legs_.clear();
    journey_leg_ = 0;
    journey_leg_ticks_ = 0;
    journey_moving_ = false;
    journey_active_ = true;
    ++climbs_done_;
    return true;
}

void Matron::end_journey(const bool failed) {
    if (climber_ != nullptr && journey_moving_) {
        climber_->cancel();   // her hands off whatever they held: a fall if on steel
    }
    journey_active_ = false;
    journey_moving_ = false;
    journey_.clear();
    journey_legs_.clear();
    if (failed) {
        journey_wait_ticks_ = kFoundClimbBlockTicks;
    }
}

void Matron::step_journey(const float dt) {
    if (journey_at_ >= journey_.size()) {
        end_journey(false);
        return;
    }
    const traversal::Edge &edge = journey_[journey_at_];
    if (journey_moving_) {
        const traversal::Climber::Status status = climber_->step(dt);
        if (status == traversal::Climber::Status::Done) {
            journey_moving_ = false;
            ++journey_at_;
            journey_legs_.clear();
            journey_leg_ = 0;
        } else if (status == traversal::Climber::Status::Failed) {
            end_journey(true);   // a physical failure: she falls or stands where it left her
        }
        return;
    }
    // To the move's start over floor, round what is in the way, her feet
    // out of the Climber's budget.
    const RVec3 at = physics_.GetBodyInterface().GetPosition(body_);
    const Vec3 feet(float(at.GetX()), float(at.GetY()) - kStandCentre, float(at.GetZ()));
    const Vec3 to = edge.kind == traversal::MoveKind::Walk ? edge.spec.goal : edge.spec.start;
    if (journey_legs_.empty()) {
        const traversal::Ignore ignore{body_, player_, static_cast<int>(moving_layer_)};
        journey_legs_ = traversal::grid_walk(physics_, ignore, climber_->limits(), feet, to);
        journey_legs_.push_back(to);
        journey_leg_ = 0;
        journey_leg_ticks_ = 0;
    }
    // A pole within arm's reach of where she stands, on her floor: she grips
    // it from here (its scanned foot spot can lie past it on a board her
    // body cannot pass it on).
    const bool pole = edge.kind == traversal::MoveKind::PoleUp || edge.kind == traversal::MoveKind::PoleDown;
    const Vec3 to_line(edge.spec.line.GetX() - feet.GetX(), 0.0F, edge.spec.line.GetZ() - feet.GetZ());
    if (pole && to_line.Length() <= climber_->limits().reach_out && std::abs(feet.GetY() - edge.spec.start.GetY()) < 0.3F &&
        climber_->supported()) {
        climber_->begin(edge.spec);   // its move grips from where she stands when she is in reach
        journey_moving_ = true;
        return;
    }
    if (journey_leg_ < journey_legs_.size()) {
        const Vec3 leg = journey_legs_[journey_leg_];
        const Vec3 off = leg - feet;
        if (Vec3(off.GetX(), 0.0F, off.GetZ()).Length() < 0.12F) {
            ++journey_leg_;
            journey_leg_ticks_ = 0;
        } else if (++journey_leg_ticks_ > 20 * 90) {
            end_journey(true);   // no headway: a way that does not serve
            return;
        } else {
            (void)climber_->walk_between(dt, leg, 1.2F);
            yaw_ = std::atan2(off.GetX(), off.GetZ());
        }
        return;
    }
    if (edge.kind == traversal::MoveKind::Walk) {
        ++journey_at_;
        journey_legs_.clear();
        return;
    }
    climber_->begin(edge.spec);
    journey_moving_ = true;
}

} // namespace scraperx::sim
