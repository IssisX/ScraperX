// Ziplines (see zipline.hpp).
#include "sim/zipline.hpp"

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>

#include <algorithm>
#include <cmath>

namespace scraperx::sim {
namespace {
using namespace JPH;
using kit::Material;
using kit::Part;

Part box(const Vec3 half, const Vec3 offset, const Material material) {
    return {half, offset, Quat::sIdentity(), material, Part::Shape::Box, 0.0F};
}

// A round member from a to b (a cylinder along its local y).
Part member(const Vec3 a, const Vec3 b, const float radius, const Material material) {
    const Vec3 span = b - a;
    return {Vec3(radius, 0.5F * span.Length(), radius), 0.5F * (a + b), Quat::sFromTo(Vec3::sAxisY(), span.Normalized()),
            material, Part::Shape::Cylinder, 0.0F};
}

// The sweep that proves a line's room ignores the player and the crew (they
// walk through; a line is never built through what stands still).
class NotPeopleFilter final : public BodyFilter {
public:
    explicit NotPeopleFilter(const BodyID player) : player_(player) {}
    [[nodiscard]] bool ShouldCollide(const BodyID &body) const override { return body != player_; }
    [[nodiscard]] bool ShouldCollideLocked(const Body &body) const override {
        const std::uint64_t entity = body.GetUserData();
        return !(entity >= 3100 && entity < 3200);   // the crew's workers (Crew::kWorkerEntityBase)
    }

private:
    BodyID player_;
};

} // namespace

Zipline::Zipline(PhysicsSystem &physics, kit::Kit &kit, const BodyID player)
    : physics_(physics), kit_(kit), player_(player) {}

Zipline::~Zipline() {
    unclip();
}

bool Zipline::floor_under(const RVec3 at) const {
    const RRayCast ray(at + Vec3(0.0F, 0.3F, 0.0F), Vec3(0.0F, -0.6F, 0.0F));
    RayCastResult hit;
    return physics_.GetNarrowPhaseQuery().CastRay(ray, hit) &&
           physics_.GetBodyInterfaceNoLock().GetMotionType(hit.mBodyID) == EMotionType::Static;
}

bool Zipline::room_along(const RVec3 top, const RVec3 bottom) const {
    // The hanging body's room, a box under the cable, swept from the top
    // anchor to the bottom one.
    const RefConst<Shape> room = new BoxShape(Vec3(0.5F, 0.5F * (kRoomBelowM + 0.1F), 0.5F));
    const RVec3 start = top - Vec3(0.0F, 0.5F * (kRoomBelowM - 0.1F), 0.0F);
    const RShapeCast sweep(room, Vec3::sReplicate(1.0F), RMat44::sTranslation(start), Vec3(bottom - top));
    const NotPeopleFilter people(player_);
    AnyHitCollisionCollector<CastShapeCollector> hit;
    physics_.GetNarrowPhaseQuery().CastShape(sweep, ShapeCastSettings(), start, hit, {}, {}, people);
    return !hit.HadHit();
}

bool Zipline::add_line(const RVec3 top_floor, const RVec3 bottom_floor) {
    if (static_cast<int>(lines_.size()) >= kMaxLines || !floor_under(top_floor) || !floor_under(bottom_floor)) {
        return false;
    }
    const RVec3 top = top_floor + Vec3(0.0F, kTopPostM, 0.0F);
    const RVec3 bottom = bottom_floor + Vec3(0.0F, kBottomPostM, 0.0F);
    const Vec3 span(bottom - top);
    const float length = span.Length();
    if (length < 10.0F || span.GetY() > -1.0F || !room_along(top, bottom)) {
        return false;
    }
    const Vec3 direction = span / length;
    // The buffer: kBufferM of travel whose clamp holds back the slope's drive
    // on a rider (less the sheaves' drag) and kBufferDecelMps2 more.
    const float gravity = -physics_.GetGravity().GetY();
    const float drive = gravity * -direction.GetY() - kTrolleyDragN / kRiderMassKg;
    const float travel = std::min(kBufferM, 0.5F * length);
    const float brake = kRiderMassKg * (kBufferDecelMps2 + drive);
    // Along the line's run on the ground, and across it.
    const Vec3 run = Vec3(direction.GetX(), 0.0F, direction.GetZ()).NormalizedOr(Vec3::sAxisZ());
    const Vec3 across(-run.GetZ(), 0.0F, run.GetX());
    // The stand stands under the parked trolley, so a climber on it jumps
    // straight up into the bar (a standing jump's reach: kTopPostM); its ramp
    // comes up along the band from the side away from the post. The post,
    // the stand and the ramp keep to a strip by the line's start, and the
    // rest of the band stays open to walk. All of it on floor, or no line.
    const Vec3 stand_at = run * (kParkM * Vec3(direction.GetX(), 0.0F, direction.GetZ()).Length());
    const Vec3 ramp_foot = stand_at - across * (0.6F + kRampRunM);
    for (const float sa : {-0.55F, 0.55F}) {
        for (const float sr : {-0.55F, 0.55F}) {
            if (!floor_under(top_floor + stand_at + across * sa + run * sr)) {
                return false;
            }
        }
    }
    if (!floor_under(top_floor + ramp_foot)) {
        return false;
    }
    // The body's origin under the top anchor, on the floor. The post stands
    // beside the line's start, its bracket arm reaching across to the
    // anchor; the stand under the parked trolley, its ramp up its far side;
    // the bottom post; and the cable.
    std::vector<Part> parts;
    const Vec3 post_at = across * 0.95F;
    parts.push_back(box(Vec3(0.08F, 0.5F * kTopPostM, 0.08F), post_at + Vec3(0.0F, 0.5F * kTopPostM, 0.0F),
                        Material::Steel));
    parts.push_back(box(Vec3(0.06F, 0.06F, 0.06F), Vec3(0.0F, kTopPostM, 0.0F), Material::Steel));
    parts.push_back(member(post_at + Vec3(0.0F, kTopPostM, 0.0F), Vec3(0.0F, kTopPostM, 0.0F), 0.05F, Material::Steel));
    parts.push_back(box(Vec3(0.6F, 0.05F, 0.6F), stand_at + Vec3(0.0F, kStandM - 0.05F, 0.0F), Material::Steel));
    for (const float sx : {-0.5F, 0.5F}) {
        for (const float sz : {-0.5F, 0.5F}) {
            parts.push_back(box(Vec3(0.05F, 0.5F * (kStandM - 0.1F), 0.05F),
                                stand_at + across * sx + run * sz + Vec3(0.0F, 0.5F * (kStandM - 0.1F), 0.0F),
                                Material::Steel));
        }
    }
    // A ramp up its far side, kRampRunM long (a slope the legs walk).
    {
        const Vec3 head = stand_at - across * 0.6F + Vec3(0.0F, kStandM, 0.0F);
        const Vec3 along = (head - ramp_foot).Normalized();
        const Vec3 normal = run.Cross(along).GetY() > 0.0F ? run.Cross(along) : along.Cross(run);
        // A right-handed frame (x = y cross z): a mirrored one is no
        // rotation, and its quaternion tilts the slab wrong.
        const Mat44 frame(Vec4(normal.Cross(along), 0.0F), Vec4(normal, 0.0F), Vec4(along, 0.0F),
                          Vec4(0.0F, 0.0F, 0.0F, 1.0F));
        parts.push_back({Vec3(0.5F, 0.04F, 0.5F * (head - ramp_foot).Length()), 0.5F * (ramp_foot + head) - normal * 0.04F,
                         frame.GetQuaternion(), Material::Hazard, Part::Shape::Box, 0.0F});
    }
    const Vec3 to_bottom(bottom_floor - top_floor);
    parts.push_back(box(Vec3(0.08F, 0.5F * kBottomPostM, 0.08F), to_bottom + Vec3(0.0F, 0.5F * kBottomPostM, 0.0F),
                        Material::Steel));
    parts.push_back(member(Vec3(0.0F, kTopPostM, 0.0F), to_bottom + Vec3(0.0F, kBottomPostM, 0.0F), kCableRadius,
                           Material::Steel));
    Line line;
    line.top = top;
    line.bottom = bottom;
    line.direction = direction;
    line.length = length;
    line.brake_m = travel;
    line.brake_n = brake;
    line.trolley_m = kParkM;
    line.block_m = length - kStopMarginM - travel;   // its rest: kBufferM short of the stop
    line.parked = true;
    line.body = kit_.add_body(kFirstLineEntity + lines_.size(), parts, top_floor, Quat::sIdentity(), 0.0F, 0.6F);
    lines_.push_back(line);
    return true;
}

int Zipline::at_hand(const RVec3 hands, const float reach, RVec3 &bar) const {
    int best = -1;
    float best_distance = reach;
    for (std::size_t i = 0; i < lines_.size(); ++i) {
        const Line &line = lines_[i];
        if (!line.parked) {
            continue;
        }
        const RVec3 parked_bar = line.top + line.direction * line.trolley_m - Vec3(0.0F, kBarDropM, 0.0F);
        const float distance = Vec3(parked_bar - hands).Length();
        if (distance <= best_distance) {
            best = static_cast<int>(i);
            best_distance = distance;
            bar = parked_bar;
        }
    }
    return best;
}

BodyID Zipline::clip(const int index, const Vec3 hands_offset) {
    unclip();
    Line &line = lines_[static_cast<std::size_t>(index)];
    hands_offset_ = hands_offset;
    // The hands' hold: the rider slides along the cable with the trolley,
    // held under it at the arms' length (the hands pulled onto the bar over
    // the first ticks, from wherever the catch reached from). The slide runs
    // from the park (the stop there: the trolley cannot ride up past its
    // hook) to the stop short of the bottom post.
    const RVec3 parked_bar = line.top + line.direction * line.trolley_m - Vec3(0.0F, kBarDropM, 0.0F);
    const BodyInterface &bodies = physics_.GetBodyInterfaceNoLock();
    SliderConstraintSettings slide;
    slide.mSpace = EConstraintSpace::WorldSpace;
    slide.mAutoDetectPoint = false;
    slide.mPoint1 = parked_bar;
    slide.mPoint2 = bodies.GetPosition(player_) + hands_offset;
    slide.SetSliderAxis(line.direction);
    slide.mLimitsMin = 0.0F;
    slide.mLimitsMax = line.length - line.trolley_m - kStopMarginM;
    slide.mLimitsSpringSettings = SpringSettings(ESpringMode::FrequencyAndDamping, 4.0F, 1.0F);
    slide.mMaxFrictionForce = kTrolleyDragN;
    const BodyID ids[2] = {kit_.body_id(line.body), player_};
    BodyLockMultiWrite lock(physics_.GetBodyLockInterface(), ids, 2);
    slider_ = static_cast<SliderConstraint *>(slide.Create(*lock.GetBody(0), *lock.GetBody(1)));
    physics_.AddConstraint(slider_);
    line.parked = false;
    state_.riding = true;
    state_.line = index;
    state_.along_m = line.trolley_m;
    state_.speed_mps = 0.0F;
    state_.braking = false;
    ++state_.rides;
    return kit_.body_id(line.body);
}

RVec3 Zipline::bar() const {
    if (slider_ == nullptr) {
        return RVec3::sZero();
    }
    return physics_.GetBodyInterfaceNoLock().GetPosition(player_) + hands_offset_;
}

void Zipline::unclip() {
    if (slider_ != nullptr) {
        physics_.RemoveConstraint(slider_);
        slider_ = nullptr;
    }
    state_.riding = false;
    state_.line = -1;
    state_.along_m = 0.0F;
    state_.speed_mps = 0.0F;
    state_.braking = false;
    state_.hand_brake = 0.0F;
    state_.hands_load_n = 0.0F;
}

void Zipline::set_hand_brake(const float squeeze) noexcept {
    state_.hand_brake = slider_ == nullptr ? 0.0F : std::clamp(squeeze, 0.0F, 1.0F);
}

void Zipline::post_step(const float delta_seconds) {
    if (slider_ == nullptr) {
        return;
    }
    Line &line = lines_[static_cast<std::size_t>(state_.line)];
    // What the bar put on the hands this step: the stop and the drags along
    // the cable, and the body's weight and swing across it.
    const auto across = slider_->GetTotalLambdaPosition();
    const float along = std::abs(slider_->GetTotalLambdaPositionLimits()) + std::abs(slider_->GetTotalLambdaMotor());
    state_.hands_load_n = std::sqrt(along * along + across[0] * across[0] + across[1] * across[1]) / delta_seconds;
    state_.along_m = kParkM + slider_->GetCurrentPosition();
    state_.speed_mps = physics_.GetBodyInterfaceNoLock().GetLinearVelocity(player_).Dot(line.direction);
    // The buffer, run into, goes down the cable ahead of the trolley.
    line.trolley_m = state_.along_m;
    line.block_m = std::max(line.block_m, state_.along_m);
    const bool buffering = state_.along_m > line.length - kStopMarginM - line.brake_m;
    state_.braking = buffering || state_.hand_brake > 0.05F;
    slider_->SetMaxFrictionForce(kTrolleyDragN + state_.hand_brake * kHandBrakeMaxN + (buffering ? line.brake_n : 0.0F));
}

void Zipline::restore() {
    unclip();
    for (Line &line : lines_) {
        line.trolley_m = kParkM;
        line.block_m = line.length - kStopMarginM - line.brake_m;
        line.parked = true;
    }
}

} // namespace scraperx::sim
