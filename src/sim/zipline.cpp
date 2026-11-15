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
#include <Jolt/Geometry/RayAABox.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>

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

Zipline::Zipline(PhysicsSystem &physics, kit::Kit &kit, const BodyID player, std::vector<AABox> reserved)
    : physics_(physics), kit_(kit), player_(player), machine_travel_(kit.travel_envelopes()),
      worked_(kit.work_bounds()), reserved_(std::move(reserved)) {}

Zipline::~Zipline() {
    unclip();
    unclip_other();
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

bool Zipline::clear_of_machines(const RVec3 top, const RVec3 bottom, const std::vector<AABox> &footings) const {
    const Vec3 span(bottom - top);
    const RayInvDirection along(span);
    for (const AABox &travel : machine_travel_) {
        // The cable crosses no envelope grown by the rider hanging under it
        // (the room's half width each side, kRoomBelowM above its top).
        const AABox rider_room(travel.mMin - Vec3(0.5F, 0.1F, 0.5F), travel.mMax + Vec3(0.5F, kRoomBelowM + 0.1F, 0.5F));
        if (RayAABox(Vec3(top), along, rider_room.mMin, rider_room.mMax) <= 1.0F) {
            return false;
        }
    }
    for (const AABox &ground : reserved_) {
        for (const AABox &footing : footings) {
            if (footing.Overlaps(ground)) {
                return false;
            }
        }
    }
    const Vec3 working_room(kWorkingRoomM, 0.0F, kWorkingRoomM);
    for (const std::vector<AABox> *worked : {&machine_travel_, &worked_}) {
        for (const AABox &spot : *worked) {
            const AABox room(spot.mMin - working_room, spot.mMax + working_room);
            for (const AABox &footing : footings) {
                if (footing.Overlaps(room)) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool Zipline::add_line(const RVec3 top_floor, const RVec3 bottom_floor, const bool keeper_pin) {
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
    {
        const Vec3 base(top_floor);
        const Vec3 post_foot = base + across * 0.95F;
        AABox stand = AABox::sFromTwoPoints(base + stand_at - across * 0.6F - run * 0.6F,
                                            base + stand_at + across * 0.6F + run * 0.6F + Vec3(0.0F, kStandM, 0.0F));
        stand.Encapsulate(base + ramp_foot - run * 0.5F);
        stand.Encapsulate(base + ramp_foot + run * 0.5F);
        const std::vector<AABox> footings{
            AABox::sFromTwoPoints(post_foot - Vec3(0.1F, 0.0F, 0.1F), post_foot + Vec3(0.1F, kTopPostM, 0.1F)),
            stand,
            AABox::sFromTwoPoints(Vec3(bottom_floor) - Vec3(0.1F, 0.0F, 0.1F),
                                  Vec3(bottom_floor) + Vec3(0.1F, kBottomPostM, 0.1F)),
        };
        if (!clear_of_machines(top, bottom, footings)) {
            return false;
        }
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
    // A keeper-pin line carries no cable member in the static body: the
    // cable is a chain of segments, held by the pin (J-095). The pin's
    // slot is in the top post, on the stand's side, low enough for a hand
    // on the deck or the stand: a shelf under the pin and cheeks either
    // side of it, as the crane's pin sits in its frame.
    if (keeper_pin) {
        // The bracket the pin lies on, reaching south from the post to
        // under the pin's far end: a shelf under the pin and cheeks
        // either side of it, as the crane's pin sits in its frame.
        const Vec3 bracket_at = post_at + run * (kPinRunM - 0.1F) + Vec3(0.0F, kPinSeatHeightM, 0.0F);
        parts.push_back(box(Vec3(0.06F, 0.02F, 0.4F), bracket_at - Vec3(0.0F, 0.05F, 0.0F), Material::Steel));
        for (const float side : {-1.0F, 1.0F}) {
            parts.push_back(box(Vec3(0.01F, 0.04F, 0.4F), bracket_at + across * (side * 0.045F),
                                Material::Steel));
        }
    } else {
        parts.push_back(member(Vec3(0.0F, kTopPostM, 0.0F), to_bottom + Vec3(0.0F, kBottomPostM, 0.0F), kCableRadius,
                               Material::Steel));
    }
    Line line;
    line.top = top;
    line.bottom = bottom;
    line.top_floor = top_floor;
    line.bottom_floor = bottom_floor;
    line.stand = top_floor + stand_at + Vec3(0.0F, kStandM, 0.0F);
    line.ramp_foot = top_floor + ramp_foot;
    line.direction = direction;
    line.length = length;
    line.brake_m = travel;
    line.brake_n = brake;
    line.trolley_m = kParkM;
    line.block_m = length - kStopMarginM - travel;   // its rest: kBufferM short of the stop
    line.parked = true;
    line.keeper_pin = keeper_pin;
    line.body = kit_.add_body(kFirstLineEntity + lines_.size(), parts, top_floor, Quat::sIdentity(), 0.0F, 0.6F);
    if (keeper_pin) {
        build_chain(line, static_cast<int>(lines_.size()));
    }
    lines_.push_back(line);
    return true;
}

// J-095: the chain for a keeper-pin line. The segments lie end to end on
// the old cable line, each a dynamic kit cylinder of 30 mm wire rope
// (3.5 kg/m), joined to the next by a direct rope of length 0 — the two
// joint points coincident, a ball joint that cannot stretch, so the
// pinned chain, exactly the anchor distance long and held at both ends,
// lies straight on the line and cannot sag. The bottom segment hinges at
// the bottom anchor; the top segment is held by the pin catch. The pin
// is a small steel bar in its slot on the top post, a handle the hands
// take. Adjacent segments do not collide: their ends meet at the joint,
// and a contact there would fight the rope.
void Zipline::build_chain(Line &line, const int index) {
    const float length = line.length;
    const int count = static_cast<int>(std::ceil(length / kCableSegmentM));
    const float pitch = length / static_cast<float>(count);
    const Vec3 direction = line.direction;
    const Quat rotation = Quat::sFromTo(Vec3::sAxisY(), direction);
    const std::uint64_t base = static_cast<std::uint64_t>(index) * 64U;
    line.cable_count = count;
    line.cable_pitch = pitch;
    line.cable_segments.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const RVec3 centre = line.top + direction * (static_cast<float>(i) * pitch + 0.5F * pitch);
        const Part segment{Vec3(kCableRadius, 0.5F * pitch, kCableRadius), Vec3::sZero(), Quat::sIdentity(),
                           Material::Steel, Part::Shape::Cylinder, 0.0F};
        const kit::BodyIndex seg =
            kit_.add_body(kFirstCableEntity + base + static_cast<std::uint64_t>(i), {segment}, centre, rotation,
                          kCableMassPerM * pitch, 0.6F);
        line.cable_segments.push_back(seg);
        if (i > 0) {
            kit_.disable_collision(line.cable_segments[static_cast<std::size_t>(i - 1)], seg);
            line.cable_ropes.push_back(kit_.add_direct_rope(line.cable_segments[static_cast<std::size_t>(i - 1)],
                                                            Vec3(0.0F, 0.5F * pitch, 0.0F), seg,
                                                            Vec3(0.0F, -0.5F * pitch, 0.0F), 0.0F, 0.0F));
        }
    }
    // The pin, a steel bar along the line's run in its slot on the post.
    const Vec3 run = Vec3(direction.GetX(), 0.0F, direction.GetZ()).NormalizedOr(Vec3::sAxisZ());
    const Vec3 across(-run.GetZ(), 0.0F, run.GetX());
    const Vec3 post_at = across * 0.95F;
    const RVec3 pin_at = line.top_floor + post_at + run * kPinRunM + Vec3(0.0F, kPinSeatHeightM, 0.0F);
    const Part pin{Vec3(0.03F, 0.03F, 0.25F), Vec3::sZero(), Quat::sIdentity(), Material::Steel, Part::Shape::Box,
                   0.0F};
    line.pin_body = kit_.add_body(kPinEntityId + base, {pin}, pin_at, Quat::sFromTo(Vec3::sAxisZ(), run), 0.5F, 0.6F);
    kit_.set_carry(line.pin_body, kit::CarryKind::Handle, Vec3::sZero());
    line.pin_catch = kit_.add_pin_catch(line.cable_segments.front(), line.pin_body, kPinToleranceM, 0.05F);
    // The hinge: the bottom segment's far end on the bottom anchor.
    {
        JPH::PointConstraintSettings hinge;
        hinge.mSpace = JPH::EConstraintSpace::WorldSpace;
        hinge.mPoint1 = line.bottom;
        hinge.mPoint2 = line.bottom;
        const JPH::BodyID ids[2] = {kit_.body_id(line.body), kit_.body_id(line.cable_segments.back())};
        JPH::BodyLockMultiWrite lock(physics_.GetBodyLockInterface(), ids, 2);
        if (lock.GetBody(0) != nullptr && lock.GetBody(1) != nullptr) {
            line.bottom_hinge =
                static_cast<JPH::PointConstraint *>(hinge.Create(*lock.GetBody(0), *lock.GetBody(1)));
            physics_.AddConstraint(line.bottom_hinge);
        }
    } // the hinge's lock is out before the welds take theirs
    weld_chain(line);
}

// J-095: the pin welds. The inter-segment ropes alone stretch under the
// chain's weight — a 5.5 m catenary at mid-span while pinned, the joints
// opening a few centimetres each under the load, the iterative solver
// never quite pulling them back. So while the pin is in, each segment is
// welded to the static body at its seat: the pinned chain lies straight
// on the line, exactly as the static cable did. The welds come off on
// the release, and the chain falls on its ropes.
void Zipline::weld_chain(Line &line) {
    const Vec3 direction = line.direction;
    const Quat rotation = Quat::sFromTo(Vec3::sAxisY(), direction);
    for (int i = 0; i < line.cable_count; ++i) {
        const RVec3 centre =
            line.top + direction * (static_cast<float>(i) * line.cable_pitch + 0.5F * line.cable_pitch);
        JPH::FixedConstraintSettings weld;
        weld.mSpace = JPH::EConstraintSpace::WorldSpace;
        weld.mAutoDetectPoint = false;
        weld.mPoint1 = centre;
        weld.mPoint2 = centre;
        weld.mAxisX1 = rotation * JPH::Vec3::sAxisX();
        weld.mAxisY1 = rotation * JPH::Vec3::sAxisY();
        weld.mAxisX2 = rotation * JPH::Vec3::sAxisX();
        weld.mAxisY2 = rotation * JPH::Vec3::sAxisY();
        const JPH::BodyID ids[2] = {kit_.body_id(line.body),
                                    kit_.body_id(line.cable_segments[static_cast<std::size_t>(i)])};
        JPH::BodyLockMultiWrite lock(physics_.GetBodyLockInterface(), ids, 2);
        if (lock.GetBody(0) == nullptr || lock.GetBody(1) == nullptr) {
            continue;
        }
        JPH::Ref<JPH::FixedConstraint> fixed =
            static_cast<JPH::FixedConstraint *>(weld.Create(*lock.GetBody(0), *lock.GetBody(1)));
        physics_.AddConstraint(fixed);
        line.cable_welds.push_back(fixed);
    }
}

void Zipline::unweld_chain(Line &line) {
    for (JPH::Ref<JPH::FixedConstraint> &weld : line.cable_welds) {
        if (weld != nullptr) {
            physics_.RemoveConstraint(weld);
            weld = nullptr;
        }
    }
    line.cable_welds.clear();
}

JPH::RVec3 Zipline::segment_centre(const Line &line, const int segment) const {
    return kit_.body_position(line.cable_segments[static_cast<std::size_t>(segment)]);
}

JPH::RVec3 Zipline::segment_point(const Line &line, const int segment, const JPH::RVec3 at) const {
    const JPH::RVec3 centre = segment_centre(line, segment);
    const JPH::Vec3 axis = kit_.body_rotation(line.cable_segments[static_cast<std::size_t>(segment)]) *
                           JPH::Vec3::sAxisY();
    const float along = std::clamp(JPH::Vec3(at - centre).Dot(axis), -0.5F * line.cable_pitch, 0.5F * line.cable_pitch);
    return centre + axis * along;
}

// J-095: pulling the pin lifts it clear of its slot, out of its hole.
// The kit's pre_step sees it past its tolerance and unlatches the catch
// on the next tick; pin_pulled is set here, at once, so a pull is a pull
// the moment it is made. A line with no keeper pin has no pin to pull.
void Zipline::pull_pin(const int line) {
    if (line < 0 || line >= static_cast<int>(lines_.size())) {
        return;
    }
    Line &held = lines_[static_cast<std::size_t>(line)];
    if (!held.keeper_pin || held.pin_pulled) {
        return;
    }
    held.pin_pulled = true;
    const JPH::RVec3 at = kit_.body_position(held.pin_body);
    kit_.place_body(held.pin_body, at + JPH::Vec3(0.0F, 0.5F, 0.0F), kit_.body_rotation(held.pin_body));
}

bool Zipline::pin_pulled(const int line) const noexcept {
    if (line < 0 || line >= static_cast<int>(lines_.size())) {
        return false;
    }
    return lines_[static_cast<std::size_t>(line)].pin_pulled;
}

RVec3 Zipline::cable_top(const int line) const {
    const Line &held = lines_[static_cast<std::size_t>(line)];
    if (!held.keeper_pin) {
        return held.top;
    }
    // The cable's near end is the top segment's centre, pinned or not:
    // while pinned it sits 0.4 m below the anchor, and reading the anchor
    // instead hides a chain that fell early and pollutes the first
    // velocity sample with the offset.
    return segment_centre(held, 0);
}

RVec3 Zipline::cable_nearest(const int line, const RVec3 at) const {
    const Line &held = lines_[static_cast<std::size_t>(line)];
    if (!held.keeper_pin) {
        return held.top;
    }
    // The nearest point on the nearest segment, pinned or released: the
    // hands hold the cable itself, not a segment's centre a metre away,
    // and reading the anchor while pinned hides a chain that fell early.
    float best_distance = std::numeric_limits<float>::max();
    JPH::RVec3 best_point = held.top;
    for (int i = 0; i < held.cable_count; ++i) {
        const JPH::RVec3 point = segment_point(held, i, at);
        const float distance = JPH::Vec3(at - point).Length();
        if (distance < best_distance) {
            best_distance = distance;
            best_point = point;
        }
    }
    return best_point;
}

RVec3 Zipline::pin_position(const int line) const {
    const Line &held = lines_[static_cast<std::size_t>(line)];
    if (!held.keeper_pin) {
        return held.top;
    }
    return kit_.body_position(held.pin_body);
}

int Zipline::at_hand(const RVec3 hands, const float reach, RVec3 &bar) const {
    int best = -1;
    float best_distance = reach;
    for (std::size_t i = 0; i < lines_.size(); ++i) {
        const Line &line = lines_[i];
        if (!line.parked) {
            continue;
        }
        const RVec3 hanging = parked_bar(static_cast<int>(i));
        const float distance = Vec3(hanging - hands).Length();
        if (distance <= best_distance) {
            best = static_cast<int>(i);
            best_distance = distance;
            bar = hanging;
        }
    }
    return best;
}

BodyID Zipline::clip(const int index, const Vec3 hands_offset) {
    unclip();
    hands_offset_ = hands_offset;
    attach(index, player_, hands_offset, false, slider_, state_);
    return kit_.body_id(lines_[static_cast<std::size_t>(index)].body);
}

void Zipline::clip_other(const int index, const BodyID body, const Vec3 hands_offset, const bool on_cable) {
    unclip_other();
    other_ = body;
    other_on_cable_ = on_cable;
    other_hands_offset_ = hands_offset;
    attach(index, body, hands_offset, on_cable, other_slider_, other_state_);
}

RVec3 Zipline::parked_bar(const int index) const {
    const Line &line = lines_[static_cast<std::size_t>(index)];
    return line.top + line.direction * line.trolley_m - Vec3(0.0F, kBarDropM, 0.0F);
}

void Zipline::attach(const int index, const BodyID body, const Vec3 hands_offset, const bool on_cable,
                     Ref<SliderConstraint> &slider, State &state) {
    Line &line = lines_[static_cast<std::size_t>(index)];
    // On the bare cable the hold starts at the park, whatever the trolley.
    const float start_m = on_cable ? kParkM : line.trolley_m;
    // The hands' hold: the rider slides along the cable with the trolley,
    // held under it at the arms' length (the hands pulled onto the bar over
    // the first ticks, from wherever the catch reached from). The slide runs
    // from the park (the stop there: the trolley cannot ride up past its
    // hook) to the stop short of the bottom post.
    const BodyInterface &bodies = physics_.GetBodyInterfaceNoLock();
    SliderConstraintSettings slide;
    slide.mSpace = EConstraintSpace::WorldSpace;
    slide.mAutoDetectPoint = false;
    slide.mPoint1 = line.top + line.direction * start_m - Vec3(0.0F, on_cable ? 0.0F : kBarDropM, 0.0F);
    slide.mPoint2 = bodies.GetPosition(body) + hands_offset;
    slide.SetSliderAxis(line.direction);
    slide.mLimitsMin = 0.0F;
    slide.mLimitsMax = line.length - start_m - kStopMarginM;
    slide.mLimitsSpringSettings = SpringSettings(ESpringMode::FrequencyAndDamping, 4.0F, 1.0F);
    slide.mMaxFrictionForce = kTrolleyDragN;
    const BodyID ids[2] = {kit_.body_id(line.body), body};
    BodyLockMultiWrite lock(physics_.GetBodyLockInterface(), ids, 2);
    slider = static_cast<SliderConstraint *>(slide.Create(*lock.GetBody(0), *lock.GetBody(1)));
    physics_.AddConstraint(slider);
    if (!on_cable) {
        line.parked = false;
    }
    state.riding = true;
    state.line = index;
    state.along_m = start_m;
    state.speed_mps = 0.0F;
    state.braking = false;
    ++state.rides;
}

RVec3 Zipline::bar() const {
    if (slider_ == nullptr) {
        return RVec3::sZero();
    }
    return physics_.GetBodyInterfaceNoLock().GetPosition(player_) + hands_offset_;
}

void Zipline::unclip() {
    remove_rope(rope_);
    detach(slider_, state_);
}

void Zipline::unclip_other() {
    remove_rope(other_rope_);
    detach(other_slider_, other_state_);
    other_hands_offset_ = JPH::Vec3::sZero();
}

void Zipline::remove_rope(Ref<kit::RopeConstraint> &rope) {
    if (rope != nullptr) {
        physics_.RemoveConstraint(rope);
        rope = nullptr;
    }
}

void Zipline::detach(Ref<SliderConstraint> &slider, State &state) {
    if (slider != nullptr) {
        physics_.RemoveConstraint(slider);
        slider = nullptr;
    }
    state.riding = false;
    state.line = -1;
    state.along_m = 0.0F;
    state.speed_mps = 0.0F;
    state.braking = false;
    state.hand_brake = 0.0F;
    state.hands_load_n = 0.0F;
    state.rope_riding = false;
    state.rope_segment = -1;
}

void Zipline::set_hand_brake(const float squeeze) noexcept {
    state_.hand_brake = slider_ == nullptr ? 0.0F : std::clamp(squeeze, 0.0F, 1.0F);
}

void Zipline::set_other_brake(const float squeeze) noexcept {
    other_state_.hand_brake = other_slider_ == nullptr ? 0.0F : std::clamp(squeeze, 0.0F, 1.0F);
}

void Zipline::post_step(const float delta_seconds) {
    // A pulled pin releases the chain: the kit's pre_step unlatches the
    // catch once the pin is past its tolerance, and the riders move from
    // the static slider onto the falling chain in this step. The pin may
    // have been pulled by the debug pull (pin_pulled already set) or taken
    // in hand (the catch alone says so); either way the catch is the
    // release, and the transfer runs once.
    for (int i = 0; i < static_cast<int>(lines_.size()); ++i) {
        Line &line = lines_[static_cast<std::size_t>(i)];
        if (!line.keeper_pin || line.cable_transferred || kit_.catch_latched(line.pin_catch)) {
            continue;
        }
        line.pin_pulled = true;
        unweld_chain(line);
        transfer_riders(line, i);
        line.cable_transferred = true;
    }
    track(slider_, state_, player_, false, delta_seconds, rope_);
    track(other_slider_, other_state_, other_, other_on_cable_, delta_seconds, other_rope_);
}

// J-095: the riders on a released line move onto the falling chain. Each
// rider's slider comes off the static body and a transfer rope ties the
// rider's hands to the nearest point on the nearest segment — her forearms
// over the cable, his hands on the bar. The rope, not a velocity write,
// carries the rider with the cable: the rider's acceleration matches the
// cable's through it.
void Zipline::transfer_riders(Line &line, const int index) {
    (void)line;
    // DIAG J-095: remove once the transfer is proven.
    std::printf("  zipline pin: DIAG transfer line %d player slider %d line %d, other slider %d line %d\n", index,
                slider_ != nullptr, state_.line, other_slider_ != nullptr, other_state_.line);
    if (slider_ != nullptr && state_.line == index) {
        transfer(index, player_, hands_offset_, kTransferRopeHisM, slider_, state_, rope_);
    }
    if (other_slider_ != nullptr && other_state_.line == index) {
        transfer(index, other_, other_hands_offset_, kTransferRopeHerM, other_slider_, other_state_, other_rope_);
    }
}

void Zipline::transfer(const int index, const JPH::BodyID body, const JPH::Vec3 hands_offset,
                       const float max_length, Ref<SliderConstraint> &slider, State &state,
                       Ref<kit::RopeConstraint> &rope) {
    Line &line = lines_[static_cast<std::size_t>(index)];
    const BodyInterface &bodies = physics_.GetBodyInterfaceNoLock();
    const RVec3 hands = bodies.GetPosition(body) + hands_offset;
    // The nearest segment, and the nearest point on it to the hands.
    int best = 0;
    float best_distance = std::numeric_limits<float>::max();
    RVec3 best_point = line.top;
    for (int i = 0; i < line.cable_count; ++i) {
        const RVec3 point = segment_point(line, i, hands);
        const float distance = Vec3(hands - point).Length();
        if (distance < best_distance) {
            best = i;
            best_distance = distance;
            best_point = point;
        }
    }
    // The slider comes off; the state stays a ride, now on the rope.
    if (slider != nullptr) {
        physics_.RemoveConstraint(slider);
        slider = nullptr;
    }
    remove_rope(rope);
    JPH::PulleyConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mBodyPoint1 = hands;
    settings.mBodyPoint2 = best_point;
    settings.mFixedPoint1 = best_point;
    settings.mFixedPoint2 = best_point;
    settings.mMinLength = 0.0F;
    settings.mMaxLength = max_length;
    const JPH::BodyID ids[2] = {body, kit_.body_id(line.cable_segments[static_cast<std::size_t>(best)])};
    JPH::BodyLockMultiWrite lock(physics_.GetBodyLockInterface(), ids, 2);
    if (lock.GetBody(0) == nullptr || lock.GetBody(1) == nullptr) {
        return;
    }
    rope = new kit::RopeConstraint(*lock.GetBody(0), *lock.GetBody(1), settings, true);
    physics_.AddConstraint(rope);
    // DIAG J-095: remove once the transfer is proven.
    const JPH::RVec3 seg_centre = segment_centre(line, best);
    std::printf("  zipline pin: DIAG rope to segment %d, hands (%.2f, %.2f, %.2f) point (%.2f, %.2f, %.2f) "
                "gap %.3f max %.2f centre (%.2f, %.2f, %.2f)\n",
                best, float(hands.GetX()), float(hands.GetY()), float(hands.GetZ()), float(best_point.GetX()),
                float(best_point.GetY()), float(best_point.GetZ()), best_distance, max_length,
                float(seg_centre.GetX()), float(seg_centre.GetY()), float(seg_centre.GetZ()));
    state.rope_riding = true;
    state.rope_segment = best;
}

void Zipline::track(const Ref<SliderConstraint> &slider, State &state, const BodyID body, const bool on_cable,
                    const float delta_seconds, const Ref<kit::RopeConstraint> &rope) {
    if (slider == nullptr) {
        // On the falling chain: the place is the segment's arc length, the
        // speed the rider's velocity along the segment's axis, and the load
        // the transfer rope's pull this step.
        if (!state.rope_riding || state.line < 0 || state.line >= static_cast<int>(lines_.size())) {
            return;
        }
        const Line &line = lines_[static_cast<std::size_t>(state.line)];
        if (state.rope_segment < 0 || state.rope_segment >= line.cable_count) {
            return;
        }
        const JPH::Vec3 axis = kit_.body_rotation(line.cable_segments[static_cast<std::size_t>(state.rope_segment)]) *
                               JPH::Vec3::sAxisY();
        state.along_m = static_cast<float>(state.rope_segment) * line.cable_pitch + 0.5F * line.cable_pitch;
        state.speed_mps = physics_.GetBodyInterfaceNoLock().GetLinearVelocity(body).Dot(axis);
        state.hands_load_n =
            rope != nullptr ? std::abs(rope->GetTotalLambdaPosition()) / std::max(delta_seconds, 1.0e-4F) : 0.0F;
        state.braking = false;
        return;
    }
    Line &line = lines_[static_cast<std::size_t>(state.line)];
    // What the bar put on the hands this step: the stop and the drags along
    // the cable, and the body's weight and swing across it.
    const auto across = slider->GetTotalLambdaPosition();
    const float along = std::abs(slider->GetTotalLambdaPositionLimits()) + std::abs(slider->GetTotalLambdaMotor());
    state.hands_load_n = std::sqrt(along * along + across[0] * across[0] + across[1] * across[1]) / delta_seconds;
    state.along_m = kParkM + slider->GetCurrentPosition();
    state.speed_mps = physics_.GetBodyInterfaceNoLock().GetLinearVelocity(body).Dot(line.direction);
    // The buffer, run into, goes down the cable ahead of the trolley (a
    // body on the bare cable moves neither).
    if (!on_cable) {
        line.trolley_m = state.along_m;
        line.block_m = std::max(line.block_m, state.along_m);
    }
    // A trolley meets the buffer at its rest; a body on the bare cable meets
    // the block where a trolley left it (at the stop, after a ride).
    const bool buffering = on_cable ? state.along_m > line.block_m - 0.3F
                                    : state.along_m > line.length - kStopMarginM - line.brake_m;
    state.braking = buffering || state.hand_brake > 0.05F;
    slider->SetMaxFrictionForce(kTrolleyDragN + state.hand_brake * kHandBrakeMaxN + (buffering ? line.brake_n : 0.0F));
}

void Zipline::restore() {
    unclip();
    unclip_other();
    for (Line &line : lines_) {
        line.trolley_m = kParkM;
        line.block_m = line.length - kStopMarginM - line.brake_m;
        line.parked = true;
        if (!line.keeper_pin) {
            continue;
        }
        // A load resets to the pinned state: the chain back on the line,
        // the pin back in its hole, at rest, so the kit's pre_step
        // relatches the catch on the next tick.
        line.pin_pulled = false;
        line.cable_transferred = false;
        unweld_chain(line);
        const Vec3 direction = line.direction;
        const Quat rotation = Quat::sFromTo(Vec3::sAxisY(), direction);
        BodyInterface &bodies = physics_.GetBodyInterface();
        for (int i = 0; i < line.cable_count; ++i) {
            const kit::BodyIndex seg = line.cable_segments[static_cast<std::size_t>(i)];
            const RVec3 centre =
                line.top + direction * (static_cast<float>(i) * line.cable_pitch + 0.5F * line.cable_pitch);
            kit_.place_body(seg, centre, rotation);
            bodies.SetLinearAndAngularVelocity(kit_.body_id(seg), Vec3::sZero(), Vec3::sZero());
        }
        const Vec3 run = Vec3(direction.GetX(), 0.0F, direction.GetZ()).NormalizedOr(Vec3::sAxisZ());
        const Vec3 across(-run.GetZ(), 0.0F, run.GetX());
        const RVec3 pin_at =
            line.top_floor + across * 0.95F + run * kPinRunM + Vec3(0.0F, kPinSeatHeightM, 0.0F);
        kit_.place_body(line.pin_body, pin_at, Quat::sFromTo(Vec3::sAxisZ(), run));
        bodies.SetLinearAndAngularVelocity(kit_.body_id(line.pin_body), Vec3::sZero(), Vec3::sZero());
        weld_chain(line);
    }
}

} // namespace scraperx::sim
