#include "sim/mechanism_kit.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

#include <algorithm>
#include <cmath>

namespace scraperx::sim::kit {

namespace {

// A rope hooked this slack or less carries no load a hand could feel.
constexpr float kSlackTensionNewtons = 60.0F;
// How close a hand must be to a hooked end to take it off.
constexpr float kUnhookReach = 1.3F;
// Hook feasibility needs a small solver margin because a caught multi-tonne
// body can sit millimetres off its nominal seat under constraint load. Five
// centimetres is ~0.1% of AS-006 B's 48.6 m line: enough to absorb numerical
// seat deflection, far below gameplay-scale travel, and it creates no motor
// or stored-energy source.
constexpr float kHookLengthTolerance = 0.05F;
// A rope over its rating for this many consecutive steps parts. One step
// over is a contact spike, not a sustained load.
constexpr std::uint32_t kPartSteps = 2;
// Speeds under this along a guide count as at rest for the governor.
constexpr float kGovernorDeadband = 0.01F;
// The governor's target never falls below this near an end of travel, so a
// governed body always reaches its stop rather than hanging short of it.
constexpr float kGovernorCreep = 0.08F;
// A catch relatches only for a body this slow.
constexpr float kRelatchSpeed = 0.15F;

[[nodiscard]] JPH::Ref<JPH::Shape> make_shape(const std::vector<Part> &parts) {
    const auto box = [](const Part &part) {
        const float smallest =
            std::min({part.half.GetX(), part.half.GetY(), part.half.GetZ()});
        return new JPH::BoxShape(part.half, std::min(JPH::cDefaultConvexRadius, 0.5F * smallest));
    };
    if (parts.size() == 1 && parts.front().offset.IsNearZero() &&
        parts.front().rotation.IsClose(JPH::Quat::sIdentity())) {
        return box(parts.front());
    }
    JPH::StaticCompoundShapeSettings settings;
    for (const Part &part : parts) {
        settings.AddShape(part.offset, part.rotation, box(part));
    }
    const JPH::ShapeSettings::ShapeResult result = settings.Create();
    return result.Get();
}

} // namespace

Kit::Kit(JPH::PhysicsSystem &system, const JPH::ObjectLayer static_layer,
         const JPH::ObjectLayer moving_layer)
    : system_(system), static_layer_(static_layer), moving_layer_(moving_layer) {}

Kit::~Kit() {
    for (Rope &rope : ropes_) {
        disconnect_rope(rope);
    }
    for (Catch &catch_record : catches_) {
        if (catch_record.pin != nullptr) {
            system_.RemoveConstraint(catch_record.pin);
            catch_record.pin = nullptr;
        }
    }
    for (Lever &lever : levers_) {
        if (lever.hinge != nullptr) {
            system_.RemoveConstraint(lever.hinge);
        }
    }
    for (Guide &guide : guides_) {
        if (guide.slider != nullptr) {
            system_.RemoveConstraint(guide.slider);
        }
    }
    for (Line &line : lines_) {
        system_.RemoveConstraint(line.constraint);
    }
    auto &bodies = system_.GetBodyInterface();
    for (auto it = bodies_.rbegin(); it != bodies_.rend(); ++it) {
        if (it->enabled) {
            bodies.RemoveBody(it->id);
        }
        bodies.DestroyBody(it->id);
    }
}

// ---- building ---------------------------------------------------------------

BodyIndex Kit::add_body(const std::uint64_t entity, const std::vector<Part> &parts,
                        const JPH::RVec3 position, const JPH::Quat rotation, const float mass_kg,
                        const float friction) {
    const bool dynamic = mass_kg > 0.0F;
    JPH::BodyCreationSettings settings(make_shape(parts), position, rotation,
                                       dynamic ? JPH::EMotionType::Dynamic
                                               : JPH::EMotionType::Static,
                                       dynamic ? moving_layer_ : static_layer_);
    settings.mFriction = friction;
    settings.mUserData = entity;
    settings.mAllowSleeping = false;
    if (dynamic) {
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = mass_kg;
    }
    auto &bodies = system_.GetBodyInterface();
    JPH::Body *body = bodies.CreateBody(settings);
    bodies.AddBody(body->GetID(),
                   dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    Body record;
    record.id = body->GetID();
    record.entity = entity;
    record.dynamic = dynamic;
    record.mass = mass_kg;
    record.parts = parts;
    record.parked = position;
    bodies_.push_back(record);
    return BodyIndex{static_cast<std::uint32_t>(bodies_.size() - 1U)};
}

void Kit::set_carry(const BodyIndex body, const CarryKind kind, const JPH::Vec3 handle_local) {
    bodies_[body.value].carry = kind;
    bodies_[body.value].handle = handle_local;
}

void Kit::set_damping(const BodyIndex body, const float linear, const float angular) {
    JPH::MotionProperties *motion = jolt_body(body).GetMotionProperties();
    motion->SetLinearDamping(linear);
    motion->SetAngularDamping(angular);
}

AnchorIndex Kit::add_anchor(const BodyIndex body, const JPH::Vec3 local, const float reach) {
    anchors_.push_back({body, local, reach});
    return AnchorIndex{static_cast<std::uint32_t>(anchors_.size() - 1U)};
}

GuideIndex Kit::add_guide(const BodyIndex body, const JPH::Vec3 axis, const float min_travel,
                          const float max_travel, const float governor_speed,
                          const float governor_force, const float level_accel) {
    const JPH::Vec3 unit = axis.Normalized();
    JPH::SliderConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mPoint1 = settings.mPoint2 = jolt_body(body).GetCenterOfMassPosition();
    settings.mSliderAxis1 = settings.mSliderAxis2 = unit;
    settings.mNormalAxis1 = settings.mNormalAxis2 = unit.GetNormalizedPerpendicular();
    settings.mLimitsMin = min_travel;
    settings.mLimitsMax = max_travel;
    JPH::Ref<JPH::SliderConstraint> slider = static_cast<JPH::SliderConstraint *>(
        settings.Create(JPH::Body::sFixedToWorld, jolt_body(body)));
    system_.AddConstraint(slider);
    Guide guide;
    guide.body = body;
    guide.slider = slider;
    guide.axis = unit;
    guide.min_travel = min_travel;
    guide.max_travel = max_travel;
    guide.governor_speed = governor_speed;
    guide.governor_force = governor_force;
    guide.level_accel = level_accel;
    guides_.push_back(guide);
    return GuideIndex{static_cast<std::uint32_t>(guides_.size() - 1U)};
}

RopeIndex Kit::add_rope(const BodyIndex body1, const JPH::Vec3 point1, const JPH::RVec3 fixed1,
                        const BodyIndex end_body, const JPH::Vec3 end_point,
                        const JPH::RVec3 fixed2, const float ratio, const float max_length,
                        const float rating_newtons) {
    Rope rope;
    rope.body1 = body1;
    rope.point1 = point1;
    rope.fixed1 = fixed1;
    rope.fixed2 = fixed2;
    if (bodies_[end_body.value].carry == CarryKind::Shackle) {
        rope.shackle = end_body;
        rope.shackle_point = end_point;
    } else {
        rope.fast_body = end_body;
        rope.fast_point = end_point;
    }
    rope.ratio = ratio;
    rope.length = max_length;
    rope.rating = rating_newtons;
    ropes_.push_back(rope);
    connect_rope(ropes_.back());
    return RopeIndex{static_cast<std::uint32_t>(ropes_.size() - 1U)};
}

LeverIndex Kit::add_lever(const BodyIndex body, const JPH::RVec3 pivot, const JPH::Vec3 axis,
                          const JPH::Vec3 normal, const float min_angle, const float max_angle) {
    JPH::HingeConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mPoint1 = settings.mPoint2 = pivot;
    settings.mHingeAxis1 = settings.mHingeAxis2 = axis.Normalized();
    settings.mNormalAxis1 = settings.mNormalAxis2 = normal.Normalized();
    settings.mLimitsMin = min_angle;
    settings.mLimitsMax = max_angle;
    JPH::Ref<JPH::HingeConstraint> hinge = static_cast<JPH::HingeConstraint *>(
        settings.Create(JPH::Body::sFixedToWorld, jolt_body(body)));
    system_.AddConstraint(hinge);
    levers_.push_back({body, hinge});
    return LeverIndex{static_cast<std::uint32_t>(levers_.size() - 1U)};
}

CatchIndex Kit::add_catch(const BodyIndex body, const LeverIndex lever, const float release_angle,
                          const float seat_tolerance, const bool relatch) {
    Catch record;
    record.body = body;
    record.lever = lever;
    record.release_angle = release_angle;
    record.seat_tolerance = seat_tolerance;
    record.relatch = relatch;
    record.seat = jolt_body(body).GetCenterOfMassPosition();
    catches_.push_back(record);
    latch(catches_.back());
    return CatchIndex{static_cast<std::uint32_t>(catches_.size() - 1U)};
}

LineIndex Kit::add_trip_line(const BodyIndex lever_body, const JPH::Vec3 lever_point,
                             const BodyIndex handle_body, const JPH::Vec3 handle_point,
                             const JPH::RVec3 sheave1, const JPH::RVec3 sheave2) {
    JPH::PulleyConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mBodyPoint1 = world_point(lever_body, lever_point);
    settings.mBodyPoint2 = world_point(handle_body, handle_point);
    settings.mFixedPoint1 = sheave1;
    settings.mFixedPoint2 = sheave2;
    settings.mMinLength = 0.0F;
    settings.mMaxLength = -1.0F;   // as laid: taut, the handle at rest
    Line line;
    line.lever_body = lever_body;
    line.lever_point = lever_point;
    line.handle_body = handle_body;
    line.handle_point = handle_point;
    line.sheave1 = sheave1;
    line.sheave2 = sheave2;
    line.constraint = static_cast<JPH::TwoBodyConstraint *>(
        settings.Create(jolt_body(lever_body), jolt_body(handle_body)));
    system_.AddConstraint(line.constraint);
    lines_.push_back(line);
    return LineIndex{static_cast<std::uint32_t>(lines_.size() - 1U)};
}

// ---- stepping ---------------------------------------------------------------

void Kit::govern(Guide &guide) noexcept {
    if (guide.slider == nullptr) {
        return;
    }
    const JPH::Body &body = jolt_body(guide.body);
    const float speed = body.GetLinearVelocity().Dot(guide.axis);
    guide.peak_speed = std::max(guide.peak_speed, std::abs(speed));
    if (guide.governor_speed <= 0.0F) {
        return;
    }
    const float travel = guide.slider->GetCurrentPosition();
    const auto allowed = [&](const float remaining) {
        return std::min(guide.governor_speed,
                        std::sqrt(2.0F * guide.level_accel * std::max(0.0F, remaining)) +
                            kGovernorCreep);
    };
    JPH::MotorSettings &motor = guide.slider->GetMotorSettings();
    if (speed > kGovernorDeadband) {
        // Rising: brake toward the allowed speed, never push.
        guide.slider->SetTargetVelocity(allowed(guide.max_travel - travel));
        motor.SetForceLimits(-guide.governor_force, 0.0F);
        guide.slider->SetMotorState(JPH::EMotorState::Velocity);
    } else if (speed < -kGovernorDeadband) {
        guide.slider->SetTargetVelocity(-allowed(travel - guide.min_travel));
        motor.SetForceLimits(0.0F, guide.governor_force);
        guide.slider->SetMotorState(JPH::EMotorState::Velocity);
    } else {
        guide.slider->SetMotorState(JPH::EMotorState::Off);
    }
}

void Kit::pre_step(const float) {
    for (Guide &guide : guides_) {
        govern(guide);
    }
    for (Catch &catch_record : catches_) {
        const float angle = lever_angle(catch_record.lever);
        if (catch_record.pin != nullptr) {
            if (catch_record.lever.valid() && angle > catch_record.release_angle) {
                unlatch(catch_record);
            }
            continue;
        }
        if (!catch_record.relatch || angle > 0.1F * catch_record.release_angle) {
            continue;
        }
        const JPH::Body &body = jolt_body(catch_record.body);
        const float off_seat =
            JPH::Vec3(body.GetCenterOfMassPosition() - catch_record.seat).Length();
        if (off_seat <= catch_record.seat_tolerance &&
            body.GetLinearVelocity().Length() < kRelatchSpeed) {
            latch(catch_record);
        }
    }
}

void Kit::post_step(const float delta_seconds) {
    for (Rope &rope : ropes_) {
        if (rope.constraint == nullptr) {
            rope.tension = 0.0F;
            continue;
        }
        rope.tension = std::abs(rope.constraint->GetTotalLambdaPosition()) / delta_seconds;
        if (rope.rating <= 0.0F) {
            continue;
        }
        rope.over_rating_steps = rope.tension > rope.rating ? rope.over_rating_steps + 1U : 0U;
        if (rope.over_rating_steps >= kPartSteps) {
            disconnect_rope(rope);
            rope.parted = true;
            rope.tension = 0.0F;
        }
    }
}

// ---- ropes, catches, bodies -------------------------------------------------

JPH::Body &Kit::jolt_body(const BodyIndex body) const noexcept {
    // Every kit body is created by, and outlives nothing but, this kit; the
    // lock-free lookup is safe outside PhysicsSystem::Update, which is the
    // only place the kit touches bodies.
    return *system_.GetBodyLockInterfaceNoLock().TryGetBody(bodies_[body.value].id);
}

JPH::RVec3 Kit::world_point(const BodyIndex body, const JPH::Vec3 local) const noexcept {
    return jolt_body(body).GetWorldTransform() * local;
}

BodyIndex Kit::rope_end_body(const Rope &rope) const noexcept {
    if (rope.anchor.valid()) {
        return anchors_[rope.anchor.value].body;
    }
    return rope.shackle.valid() ? rope.shackle : rope.fast_body;
}

JPH::Vec3 Kit::rope_end_point(const Rope &rope) const noexcept {
    if (rope.anchor.valid()) {
        return anchors_[rope.anchor.value].local;
    }
    return rope.shackle.valid() ? rope.shackle_point : rope.fast_point;
}

void Kit::connect_rope(Rope &rope) {
    if (rope.parted || rope.constraint != nullptr) {
        return;
    }
    const BodyIndex end = rope_end_body(rope);
    JPH::PulleyConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mBodyPoint1 = world_point(rope.body1, rope.point1);
    settings.mBodyPoint2 = world_point(end, rope_end_point(rope));
    settings.mFixedPoint1 = rope.fixed1;
    settings.mFixedPoint2 = rope.fixed2;
    settings.mRatio = rope.ratio;
    settings.mMinLength = 0.0F;
    settings.mMaxLength = rope.length;
    rope.constraint = static_cast<JPH::PulleyConstraint *>(
        settings.Create(jolt_body(rope.body1), jolt_body(end)));
    system_.AddConstraint(rope.constraint);
    auto &bodies = system_.GetBodyInterface();
    bodies.ActivateBody(bodies_[rope.body1.value].id);
    if (bodies_[end.value].dynamic) {
        bodies.ActivateBody(bodies_[end.value].id);
    }
}

void Kit::disconnect_rope(Rope &rope) {
    if (rope.constraint != nullptr) {
        system_.RemoveConstraint(rope.constraint);
        rope.constraint = nullptr;
    }
}

void Kit::set_enabled(const BodyIndex body, const bool enabled) {
    Body &record = bodies_[body.value];
    if (record.enabled == enabled) {
        return;
    }
    auto &bodies = system_.GetBodyInterface();
    if (enabled) {
        bodies.AddBody(record.id, JPH::EActivation::Activate);
    } else {
        record.parked = bodies.GetPosition(record.id);
        bodies.RemoveBody(record.id);
    }
    record.enabled = enabled;
}

void Kit::latch(Catch &catch_record) {
    if (catch_record.pin != nullptr) {
        return;
    }
    // The pin's taper draws the body onto its seat: the pin holds the seat,
    // not wherever the body is when the pin drops in. A relatch within
    // seat_tolerance lifts the body those last centimetres, which takes the
    // load off a rope that had caught it. Declared: the catch's spring does
    // at most m g seat_tolerance of work.
    JPH::FixedConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mAutoDetectPoint = false;
    settings.mPoint1 = catch_record.seat;
    settings.mPoint2 = jolt_body(catch_record.body).GetCenterOfMassPosition();
    catch_record.pin = static_cast<JPH::FixedConstraint *>(
        settings.Create(JPH::Body::sFixedToWorld, jolt_body(catch_record.body)));
    system_.AddConstraint(catch_record.pin);
}

void Kit::unlatch(Catch &catch_record) {
    if (catch_record.pin == nullptr) {
        return;
    }
    system_.RemoveConstraint(catch_record.pin);
    catch_record.pin = nullptr;
    system_.GetBodyInterface().ActivateBody(bodies_[catch_record.body.value].id);
}

// ---- player verbs -----------------------------------------------------------

void Kit::carry_candidates(std::vector<CarryCandidate> &out) const {
    out.clear();
    for (const Body &body : bodies_) {
        if (body.carry == CarryKind::None || !body.enabled) {
            continue;
        }
        out.push_back({body.id, body.entity, body.handle, body.carry});
    }
}

JPH::Vec3 Kit::carry_handle(const std::uint64_t entity) const noexcept {
    const Body *body = find(bodies_, body_for_entity(entity));
    return body != nullptr ? body->handle : JPH::Vec3::sZero();
}

CarryKind Kit::carry_kind(const std::uint64_t entity) const noexcept {
    const Body *body = find(bodies_, body_for_entity(entity));
    return body != nullptr ? body->carry : CarryKind::None;
}

AnchorIndex Kit::hook_target(const std::uint64_t shackle_entity) const noexcept {
    const BodyIndex shackle = body_for_entity(shackle_entity);
    const Body *shackle_body = find(bodies_, shackle);
    if (shackle_body == nullptr || !shackle_body->enabled) {
        return {};
    }
    const Rope *rope = nullptr;
    for (const Rope &candidate : ropes_) {
        if (candidate.shackle == shackle && !candidate.anchor.valid() && !candidate.parted) {
            rope = &candidate;
        }
    }
    if (rope == nullptr) {
        return {};
    }
    const JPH::RVec3 at = world_point(shackle, rope->shackle_point);
    const JPH::RVec3 first = world_point(rope->body1, rope->point1);
    const float first_leg = JPH::Vec3(first - rope->fixed1).Length();
    AnchorIndex best;
    float best_distance = 0.0F;
    for (std::uint32_t index = 0; index < anchors_.size(); ++index) {
        const Anchor &anchor = anchors_[index];
        if (anchor.body == rope->body1 || !bodies_[anchor.body.value].enabled) {
            continue;
        }
        const JPH::RVec3 point = world_point(anchor.body, anchor.local);
        const float distance = JPH::Vec3(point - at).Length();
        if (distance > anchor.reach || (best.valid() && distance >= best_distance)) {
            continue;
        }
        const float needed =
            first_leg + rope->ratio * JPH::Vec3(point - rope->fixed2).Length();
        if (needed > rope->length + kHookLengthTolerance) {
            continue;
        }
        best = AnchorIndex{index};
        best_distance = distance;
    }
    return best;
}

bool Kit::hook(const std::uint64_t shackle_entity, const AnchorIndex anchor) {
    const BodyIndex shackle = body_for_entity(shackle_entity);
    if (!shackle.valid() || find(anchors_, anchor) == nullptr) {
        return false;
    }
    for (Rope &rope : ropes_) {
        if (rope.shackle != shackle || rope.anchor.valid() || rope.parted) {
            continue;
        }
        disconnect_rope(rope);
        rope.anchor = anchor;
        set_enabled(shackle, false);
        connect_rope(rope);
        return true;
    }
    return false;
}

RopeIndex Kit::unhook_target(const JPH::RVec3 hand) const noexcept {
    RopeIndex best;
    float best_distance = kUnhookReach;
    for (std::uint32_t index = 0; index < ropes_.size(); ++index) {
        const Rope &rope = ropes_[index];
        if (!rope.anchor.valid() || rope.parted || !rope.shackle.valid() ||
            rope.tension > kSlackTensionNewtons) {
            continue;
        }
        const Anchor &anchor = anchors_[rope.anchor.value];
        const float distance =
            JPH::Vec3(world_point(anchor.body, anchor.local) - hand).Length();
        if (distance < best_distance) {
            best = RopeIndex{index};
            best_distance = distance;
        }
    }
    return best;
}

std::uint64_t Kit::unhook(const RopeIndex rope_index) {
    if (find(ropes_, rope_index) == nullptr) {
        return 0;
    }
    Rope &rope = ropes_[rope_index.value];
    if (!rope.anchor.valid() || !rope.shackle.valid()) {
        return 0;
    }
    const Anchor &anchor = anchors_[rope.anchor.value];
    const JPH::RVec3 at = world_point(anchor.body, anchor.local);
    disconnect_rope(rope);
    rope.anchor = AnchorIndex{};
    const JPH::BodyID shackle_id = bodies_[rope.shackle.value].id;
    auto &bodies = system_.GetBodyInterface();
    bodies.SetPositionAndRotation(shackle_id, at - JPH::RVec3(rope.shackle_point),
                                  JPH::Quat::sIdentity(), JPH::EActivation::DontActivate);
    bodies.SetLinearAndAngularVelocity(shackle_id, JPH::Vec3::sZero(), JPH::Vec3::sZero());
    set_enabled(rope.shackle, true);
    connect_rope(rope);
    return bodies_[rope.shackle.value].entity;
}

std::uint64_t Kit::anchor_entity(const AnchorIndex anchor) const noexcept {
    const Anchor *record = find(anchors_, anchor);
    return record != nullptr ? bodies_[record->body.value].entity : 0;
}

std::uint64_t Kit::rope_shackle_entity(const RopeIndex rope) const noexcept {
    const Rope *record = find(ropes_, rope);
    return record != nullptr && record->shackle.valid() ? bodies_[record->shackle.value].entity
                                                        : 0;
}

// ---- checkpoint -------------------------------------------------------------

void Kit::capture(Checkpoint &out) const {
    out.bodies.resize(bodies_.size());
    const auto &bodies = system_.GetBodyInterfaceNoLock();
    for (std::size_t index = 0; index < bodies_.size(); ++index) {
        const Body &record = bodies_[index];
        BodyState &state = out.bodies[index];
        state.enabled = record.enabled;
        if (!record.dynamic) {
            continue;
        }
        state.position = record.enabled ? bodies.GetPosition(record.id) : record.parked;
        state.rotation = bodies.GetRotation(record.id);
        state.linear = bodies.GetLinearVelocity(record.id);
        state.angular = bodies.GetAngularVelocity(record.id);
    }
    out.rope_anchor.resize(ropes_.size());
    out.rope_parted.resize(ropes_.size());
    for (std::size_t index = 0; index < ropes_.size(); ++index) {
        out.rope_anchor[index] = ropes_[index].anchor;
        out.rope_parted[index] = ropes_[index].parted;
    }
    out.catch_latched.resize(catches_.size());
    for (std::size_t index = 0; index < catches_.size(); ++index) {
        out.catch_latched[index] = catches_[index].pin != nullptr;
    }
}

void Kit::restore(const Checkpoint &in) {
    if (in.bodies.size() != bodies_.size()) {
        return;
    }
    for (Rope &rope : ropes_) {
        disconnect_rope(rope);
    }
    for (Catch &catch_record : catches_) {
        unlatch(catch_record);
    }
    auto &bodies = system_.GetBodyInterface();
    for (std::size_t index = 0; index < bodies_.size(); ++index) {
        Body &record = bodies_[index];
        const BodyState &state = in.bodies[index];
        if (!record.dynamic) {
            continue;
        }
        const BodyIndex body{static_cast<std::uint32_t>(index)};
        if (!state.enabled) {
            set_enabled(body, false);
            record.parked = state.position;
            continue;
        }
        bodies.SetPositionAndRotation(record.id, state.position, state.rotation,
                                      JPH::EActivation::DontActivate);
        bodies.SetLinearAndAngularVelocity(record.id, state.linear, state.angular);
        set_enabled(body, true);
        bodies.ActivateBody(record.id);
    }
    for (std::size_t index = 0; index < ropes_.size(); ++index) {
        Rope &rope = ropes_[index];
        rope.parted = in.rope_parted[index];
        rope.anchor = in.rope_anchor[index];
        rope.over_rating_steps = 0;
        connect_rope(rope);
    }
    for (std::size_t index = 0; index < catches_.size(); ++index) {
        if (in.catch_latched[index]) {
            latch(catches_[index]);
        }
    }
}

// ---- read back --------------------------------------------------------------

std::uint64_t Kit::body_entity(const BodyIndex body) const noexcept {
    const Body *record = find(bodies_, body);
    return record != nullptr ? record->entity : 0;
}

bool Kit::body_dynamic(const BodyIndex body) const noexcept {
    const Body *record = find(bodies_, body);
    return record != nullptr && record->dynamic;
}

bool Kit::body_enabled(const BodyIndex body) const noexcept {
    const Body *record = find(bodies_, body);
    return record != nullptr && record->enabled;
}

const std::vector<Part> &Kit::body_parts(const BodyIndex body) const noexcept {
    static const std::vector<Part> kNoParts;
    const Body *record = find(bodies_, body);
    return record != nullptr ? record->parts : kNoParts;
}

JPH::RVec3 Kit::body_position(const BodyIndex body) const noexcept {
    const Body *record = find(bodies_, body);
    if (record == nullptr) {
        return JPH::RVec3::sZero();
    }
    return record->enabled ? system_.GetBodyInterfaceNoLock().GetPosition(record->id)
                           : record->parked;
}

JPH::Quat Kit::body_rotation(const BodyIndex body) const noexcept {
    const Body *record = find(bodies_, body);
    return record != nullptr ? system_.GetBodyInterfaceNoLock().GetRotation(record->id)
                             : JPH::Quat::sIdentity();
}

JPH::Vec3 Kit::body_velocity(const BodyIndex body) const noexcept {
    const Body *record = find(bodies_, body);
    return record != nullptr && record->enabled && record->dynamic
               ? system_.GetBodyInterfaceNoLock().GetLinearVelocity(record->id)
               : JPH::Vec3::sZero();
}

float Kit::body_mass(const BodyIndex body) const noexcept {
    const Body *record = find(bodies_, body);
    return record != nullptr ? record->mass : 0.0F;
}

JPH::BodyID Kit::body_id(const BodyIndex body) const noexcept {
    const Body *record = find(bodies_, body);
    return record != nullptr ? record->id : JPH::BodyID();
}

BodyIndex Kit::body_for_entity(const std::uint64_t entity) const noexcept {
    for (std::uint32_t index = 0; index < bodies_.size(); ++index) {
        if (bodies_[index].entity == entity) {
            return BodyIndex{index};
        }
    }
    return {};
}

void Kit::rope_polyline(const RopeIndex rope_index, std::vector<JPH::RVec3> &out) const {
    out.clear();
    const Rope *rope = find(ropes_, rope_index);
    if (rope == nullptr || rope->parted) {
        return;
    }
    out.push_back(world_point(rope->body1, rope->point1));
    out.push_back(rope->fixed1);
    if (JPH::Vec3(rope->fixed2 - rope->fixed1).LengthSq() > 1.0e-4F) {
        out.push_back(rope->fixed2);
    }
    out.push_back(world_point(rope_end_body(*rope), rope_end_point(*rope)));
}

void Kit::cable_polyline(const std::uint32_t cable, std::vector<JPH::RVec3> &out) const {
    if (cable < ropes_.size()) {
        rope_polyline(RopeIndex{cable}, out);
        return;
    }
    out.clear();
    const std::size_t line_index = cable - ropes_.size();
    if (line_index >= lines_.size()) {
        return;
    }
    const Line &line = lines_[line_index];
    out.push_back(world_point(line.lever_body, line.lever_point));
    out.push_back(line.sheave1);
    out.push_back(line.sheave2);
    out.push_back(world_point(line.handle_body, line.handle_point));
}

bool Kit::rope_parted(const RopeIndex rope) const noexcept {
    const Rope *record = find(ropes_, rope);
    return record != nullptr && record->parted;
}

std::uint64_t Kit::rope_end_entity(const RopeIndex rope) const noexcept {
    const Rope *record = find(ropes_, rope);
    return record != nullptr ? body_entity(rope_end_body(*record)) : 0;
}

float Kit::rope_tension(const RopeIndex rope) const noexcept {
    const Rope *record = find(ropes_, rope);
    return record != nullptr ? record->tension : 0.0F;
}

bool Kit::catch_latched(const CatchIndex catch_index) const noexcept {
    const Catch *record = find(catches_, catch_index);
    return record != nullptr && record->pin != nullptr;
}

float Kit::lever_angle(const LeverIndex lever) const noexcept {
    const Lever *record = find(levers_, lever);
    return record != nullptr && record->hinge != nullptr ? record->hinge->GetCurrentAngle() : 0.0F;
}

float Kit::guide_travel(const GuideIndex guide) const noexcept {
    const Guide *record = find(guides_, guide);
    return record != nullptr && record->slider != nullptr ? record->slider->GetCurrentPosition()
                                                          : 0.0F;
}

float Kit::guide_peak_speed(const GuideIndex guide) const noexcept {
    const Guide *record = find(guides_, guide);
    return record != nullptr ? record->peak_speed : 0.0F;
}

} // namespace scraperx::sim::kit
