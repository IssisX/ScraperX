#include "sim/mechanism_kit.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace scraperx::sim::kit {

namespace {

// A rope hooked this slack or less carries no load a hand could feel.
constexpr float kSlackTensionNewtons = 60.0F;
// How close a hand must be to a hooked end to take it off.
constexpr float kUnhookReach = 1.3F;
// Hooking may take up at most this much slack beyond the rope's length:
// more, and the rope would have to stretch.
constexpr float kHookLengthTolerance = 0.03F;
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
// How far a stream of rubble can fall looking for somewhere to land.
constexpr float kStreamReach = 80.0F;
// Spills landing this close to a pile join it; the kit keeps at most
// kMaxPiles piles, and past that a spill joins the nearest.
constexpr float kPileMergeRadius = 1.5F;
constexpr std::size_t kMaxPiles = 16;

// The declared water and air models (AS-007).
constexpr float kWaterDensity = 1000.0F;          // kg/m^3
constexpr float kGravity = 9.81F;
constexpr float kDischarge = 0.6F;                // orifice discharge coefficient
constexpr float kHeadLinear = 0.05F;              // m: flow is linear in head below this
constexpr float kFloatDrag = 2500.0F;             // N s/m per m^2 of float
constexpr float kAtmosphere = 101325.0F;          // Pa
constexpr float kAirDensity = 1.2F;               // kg/m^3 at kAtmosphere
constexpr float kPressureLinear = 50.0F;          // Pa: flow is linear below this
constexpr float kBreakerOpen = 100.0F;            // Pa under kAtmosphere
constexpr float kMinCellVolume = 0.5F;            // m^3, a guard: no cell is ever empty

// Orifice flow, m^3/s, through area for a pressure difference dp in a fluid
// of density rho; linear below `linear` so a nearly-level pair does not
// chatter.
[[nodiscard]] float orifice(const float area, const float dp, const float rho,
                            const float linear) {
    const float magnitude = std::abs(dp);
    if (magnitude < linear) {
        return kDischarge * area * std::sqrt(2.0F * linear / rho) * (magnitude / linear);
    }
    return kDischarge * area * std::sqrt(2.0F * magnitude / rho);
}

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
    guide.dog_floor = min_travel;
    guides_.push_back(guide);
    return GuideIndex{static_cast<std::uint32_t>(guides_.size() - 1U)};
}

void Kit::set_dogs(const GuideIndex guide, const float pitch) {
    guides_[guide.value].dog_pitch = pitch;
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
    levers_.push_back({body, hinge, pivot});
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
    record.seat_rotation = jolt_body(body).GetRotation();
    catches_.push_back(record);
    latch(catches_.back());
    return CatchIndex{static_cast<std::uint32_t>(catches_.size() - 1U)};
}

SlipIndex Kit::add_slip(const RopeIndex rope, const LeverIndex lever, const float release_angle) {
    slips_.push_back({rope, lever, release_angle});
    return SlipIndex{static_cast<std::uint32_t>(slips_.size() - 1U)};
}

BinIndex Kit::add_bin(const BodyIndex body, const float contents_kg, const float capacity_kg,
                       const JPH::Vec3 mouth_local, const LeverIndex gate,
                       const float gate_open_angle, const float gate_reach,
                       const float flow_rate) {
    Bin bin;
    bin.body = body;
    bin.contents = contents_kg;
    bin.capacity = capacity_kg;
    bin.mouth = mouth_local;
    bin.gate = gate;
    bin.gate_open_angle = gate_open_angle;
    bin.gate_reach = gate_reach;
    bin.flow_rate = flow_rate;
    bins_.push_back(bin);
    apply_bin_mass(bins_.back());
    return BinIndex{static_cast<std::uint32_t>(bins_.size() - 1U)};
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

void Kit::open_catch(const CatchIndex catch_index) {
    unlatch(catches_[catch_index.value]);
}

void Kit::place_body(const BodyIndex body, const JPH::RVec3 position, const JPH::Quat rotation) {
    system_.GetBodyInterface().SetPositionAndRotation(bodies_[body.value].id, position, rotation,
                                                      JPH::EActivation::Activate);
}

void Kit::set_bin_water(const BinIndex bin) {
    bins_[bin.value].water = true;
}

void Kit::set_strut(const RopeIndex rope) {
    Rope &record = ropes_[rope.value];
    disconnect_rope(record);
    record.strut = true;
    connect_rope(record);
}

PoolIndex Kit::add_pool(const JPH::Vec3 min_corner, const JPH::Vec3 max_corner,
                        const float water_kg) {
    Pool pool;
    pool.min_corner = min_corner;
    pool.max_corner = max_corner;
    pool.water = water_kg;
    pools_.push_back(pool);
    settle_level(pools_.back());
    return PoolIndex{static_cast<std::uint32_t>(pools_.size() - 1U)};
}

void Kit::add_float(const PoolIndex pool, const BodyIndex body, const float half_x,
                    const float half_z, const float bottom_local, const float height) {
    pools_[pool.value].floats.push_back({body, half_x, half_z, bottom_local, height});
    settle_level(pools_[pool.value]);
}

PipeIndex Kit::add_pipe(const PoolIndex from, const float from_y, const PoolIndex to,
                        const float to_y, const JPH::RVec3 spout, const float area,
                        const LeverIndex valve, const float shut_angle, const float open_angle) {
    Pipe pipe;
    pipe.from = from;
    pipe.from_y = from_y;
    pipe.to = to;
    pipe.to_y = to_y;
    pipe.spout = spout;
    pipe.area = area;
    pipe.valve = valve;
    pipe.shut_angle = shut_angle;
    pipe.open_angle = open_angle;
    pipes_.push_back(pipe);
    return PipeIndex{static_cast<std::uint32_t>(pipes_.size() - 1U)};
}

void Kit::set_pipe_spool(const PipeIndex pipe, const BodyIndex spool, const JPH::RVec3 seat,
                         const JPH::Vec3 axis, const float tolerance,
                         const float angle_tolerance) {
    Pipe &record = pipes_[pipe.value];
    record.spool = spool;
    record.seat = seat;
    record.seat_axis = axis.Normalized();
    record.seat_tolerance = tolerance;
    record.seat_angle = angle_tolerance;
}

void Kit::add_charge(const PoolIndex pool, const float from_y, const BodyIndex body,
                     const float area, const float base_y, const LeverIndex valve,
                     const float open_angle) {
    Charge charge;
    charge.pool = pool;
    charge.from_y = from_y;
    charge.body = body;
    charge.area = area;
    charge.base_y = base_y;
    charge.valve = valve;
    charge.open_angle = open_angle;
    charge.last_y = static_cast<float>(jolt_body(body).GetCenterOfMassPosition().GetY());
    charges_.push_back(charge);
}

CellIndex Kit::add_cell(const float volume_m3) {
    Cell cell;
    cell.volume0 = volume_m3;
    cell.air = kAtmosphere * volume_m3;
    cells_.push_back(cell);
    return CellIndex{static_cast<std::uint32_t>(cells_.size() - 1U)};
}

void Kit::add_piston(const CellIndex cell, const BodyIndex body, const float area) {
    const float rest = static_cast<float>(jolt_body(body).GetCenterOfMassPosition().GetY());
    cells_[cell.value].pistons.push_back({body, area, rest});
}

void Kit::add_throttle(const CellIndex a, const CellIndex b, const float area) {
    throttles_.push_back({a, b, area});
}

void Kit::add_door(const CellIndex cell, const LeverIndex door, const float area,
                   const float full_angle, const JPH::Vec3 normal) {
    cells_[cell.value].doors.push_back({door, area, full_angle, normal.Normalized()});
}

void Kit::add_breaker(const CellIndex cell, const float area) {
    cells_[cell.value].breaker = area;
}

void Kit::add_bleed(const CellIndex cell, const float area) {
    cells_[cell.value].bleed = area;
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

void Kit::pre_step(const float delta_seconds) {
    press(delta_seconds);
    for (Guide &guide : guides_) {
        govern(guide);
        engage_dogs(guide);
    }
    for (const Slip &slip : slips_) {
        Rope &rope = ropes_[slip.rope.value];
        if (!rope.parted && lever_angle(slip.lever) > slip.release_angle) {
            disconnect_rope(rope);
            rope.parted = true;
            rope.tension = 0.0F;
        }
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

void Kit::apply_bin_mass(const Bin &bin) {
    const Body &record = bodies_[bin.body.value];
    if (!record.dynamic) {
        return;
    }
    jolt_body(bin.body).GetMotionProperties()->ScaleToMass(record.mass + bin.contents);
}

bool Kit::land_stream(const JPH::RVec3 from, const JPH::BodyID ignore, Bin *&receiver,
                      JPH::RVec3 &lands) {
    // Straight down: the first bin, or the first static surface; moving
    // bodies that are not bins are passed through.
    const JPH::NarrowPhaseQuery &query = system_.GetNarrowPhaseQueryNoLock();
    const JPH::RRayCast ray{from, JPH::Vec3(0.0F, -kStreamReach, 0.0F)};
    JPH::AllHitCollisionCollector<JPH::CastRayCollector> hits;
    const JPH::IgnoreSingleBodyFilter not_self(ignore);
    query.CastRay(ray, JPH::RayCastSettings(), hits, {}, {}, not_self);
    hits.Sort();
    receiver = nullptr;
    for (const JPH::RayCastResult &hit : hits.mHits) {
        for (Bin &candidate : bins_) {
            if (bodies_[candidate.body.value].id == hit.mBodyID) {
                receiver = &candidate;
            }
        }
        const JPH::BodyLockRead lock(system_.GetBodyLockInterfaceNoLock(), hit.mBodyID);
        const bool is_static = lock.Succeeded() && lock.GetBody().IsStatic();
        if (receiver != nullptr || is_static) {
            lands = ray.GetPointOnRay(hit.mFraction);
            if (receiver != nullptr) {
                // Into a bin, the stream runs on past its rim or bail to its floor.
                const JPH::RVec3 floor = world_point(receiver->body, floor_top(*receiver));
                if (floor.GetY() < lands.GetY()) {
                    lands.SetY(floor.GetY());
                }
            }
            return true;
        }
    }
    return false;
}

void Kit::flow_bins(const float delta_seconds) {
    for (Bin &bin : bins_) {
        bin.flowing = false;
        const Lever *gate = find(levers_, bin.gate);
        if (gate == nullptr || bin.contents <= 0.0F || !bodies_[bin.body.value].enabled ||
            lever_angle(bin.gate) <= bin.gate_open_angle) {
            continue;
        }
        const JPH::RVec3 mouth = world_point(bin.body, bin.mouth);
        if (JPH::Vec3(gate->pivot - mouth).Length() > bin.gate_reach) {
            continue;
        }
        Bin *receiver = nullptr;
        JPH::RVec3 lands = mouth;
        if (!land_stream(mouth, bodies_[bin.body.value].id, receiver, lands)) {
            continue;
        }
        float moved = std::min(bin.contents, bin.flow_rate * delta_seconds);
        if (receiver != nullptr) {
            moved = std::min(moved, std::max(0.0F, receiver->capacity - receiver->contents));
            receiver->contents += moved;
            apply_bin_mass(*receiver);
        } else if (bin.water) {
            drained_ += moved;
        } else {
            spill(lands, moved);
        }
        if (moved <= 0.0F) {
            continue;
        }
        bin.contents -= moved;
        apply_bin_mass(bin);
        bin.flowing = true;
        bin.stream_from = mouth;
        bin.stream_to = lands;
    }
}

JPH::Vec3 Kit::floor_top(const Bin &bin) const {
    const std::vector<Part> &parts = bodies_[bin.body.value].parts;
    if (parts.empty()) {
        return bin.mouth;
    }
    const Part &floor = parts.front();
    return floor.offset + floor.rotation * JPH::Vec3(0.0F, floor.half.GetY(), 0.0F);
}

void Kit::spill(const JPH::RVec3 at, const float kg) {
    if (kg <= 0.0F) {
        return;
    }
    Pile *nearest = nullptr;
    float nearest_distance = std::numeric_limits<float>::max();
    for (Pile &pile : piles_) {
        const float distance = JPH::Vec3(pile.at - at).Length();
        if (distance < nearest_distance) {
            nearest = &pile;
            nearest_distance = distance;
        }
    }
    if (nearest != nullptr &&
        (nearest_distance <= kPileMergeRadius || piles_.size() >= kMaxPiles)) {
        nearest->kg += kg;
        return;
    }
    piles_.push_back({at, kg});
}

float Kit::spilled() const noexcept {
    float kg = 0.0F;
    for (const Pile &pile : piles_) {
        kg += pile.kg;
    }
    return kg;
}

void Kit::post_step(const float delta_seconds) {
    flow_bins(delta_seconds);
    flow_water(delta_seconds);
    flow_air(delta_seconds);
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

// ---- water and air (declared models, AS-007) -------------------------------

float Kit::pool_volume_at(const Pool &pool, const float level) const {
    const JPH::Vec3 size = pool.max_corner - pool.min_corner;
    float volume = size.GetX() * size.GetZ() * std::max(0.0F, level - pool.min_corner.GetY());
    for (const Float &record : pool.floats) {
        const float bottom = static_cast<float>(
            (jolt_body(record.body).GetWorldTransform() * JPH::Vec3(0.0F, record.bottom, 0.0F))
                .GetY());
        volume -= 4.0F * record.half_x * record.half_z *
                  std::clamp(level - bottom, 0.0F, record.height);
    }
    return volume;
}

void Kit::settle_level(Pool &pool) {
    // The volume below a level only grows with it (a float is narrower than
    // its pool), so bisect for the level that holds this much water.
    const float volume = std::max(0.0F, pool.water) / kWaterDensity;
    const JPH::Vec3 size = pool.max_corner - pool.min_corner;
    const float area = std::max(1.0e-3F, size.GetX() * size.GetZ());
    float low = pool.min_corner.GetY();
    float high = pool.max_corner.GetY() + volume / area + 1.0F;
    for (int step = 0; step < 32; ++step) {
        const float middle = 0.5F * (low + high);
        (pool_volume_at(pool, middle) < volume ? low : high) = middle;
    }
    pool.level = 0.5F * (low + high);
    if (pool.level > pool.max_corner.GetY()) {
        const float kept = pool_volume_at(pool, pool.max_corner.GetY()) * kWaterDensity;
        drained_ += pool.water - kept;
        pool.water = kept;
        pool.level = pool.max_corner.GetY();
    }
}

float Kit::pipe_opening(const Pipe &pipe) const {
    float opening = 1.0F;
    if (pipe.valve.valid()) {
        const float span = pipe.open_angle - pipe.shut_angle;
        opening = std::abs(span) < 1.0e-4F
                      ? 1.0F
                      : std::clamp((lever_angle(pipe.valve) - pipe.shut_angle) / span, 0.0F, 1.0F);
    }
    if (pipe.spool.valid()) {
        const Body &spool = bodies_[pipe.spool.value];
        if (!spool.enabled) {
            return 0.0F;
        }
        const JPH::Body &body = jolt_body(pipe.spool);
        const float off_seat = JPH::Vec3(body.GetCenterOfMassPosition() - pipe.seat).Length();
        const JPH::Vec3 axis = body.GetRotation() * JPH::Vec3::sAxisX();
        const float cosine = std::min(1.0F, std::abs(axis.Dot(pipe.seat_axis)));
        if (off_seat > pipe.seat_tolerance || std::acos(cosine) > pipe.seat_angle) {
            return 0.0F;
        }
    }
    return opening;
}

float Kit::cell_volume(const Cell &cell) const {
    float volume = cell.volume0;
    for (const Piston &piston : cell.pistons) {
        const float y =
            static_cast<float>(jolt_body(piston.body).GetCenterOfMassPosition().GetY());
        volume += piston.area * (y - piston.rest_y);
    }
    return std::max(kMinCellVolume, volume);
}

void Kit::press(const float) {
    JPH::BodyInterface &bodies = system_.GetBodyInterfaceNoLock();
    for (Pool &pool : pools_) {
        settle_level(pool);
        for (const Float &record : pool.floats) {
            const JPH::Body &body = jolt_body(record.body);
            const float bottom = static_cast<float>(
                (body.GetWorldTransform() * JPH::Vec3(0.0F, record.bottom, 0.0F)).GetY());
            const float depth = std::clamp(pool.level - bottom, 0.0F, record.height);
            if (depth <= 0.0F) {
                continue;
            }
            const float area = 4.0F * record.half_x * record.half_z;
            const float lift = kWaterDensity * kGravity * area * depth -
                               kFloatDrag * area * body.GetLinearVelocity().GetY();
            bodies.AddForce(body.GetID(), JPH::Vec3(0.0F, lift, 0.0F));
        }
    }
    for (const Cell &cell : cells_) {
        const float gauge = cell.air / cell_volume(cell) - kAtmosphere;
        for (const Piston &piston : cell.pistons) {
            bodies.AddForce(bodies_[piston.body.value].id,
                            JPH::Vec3(0.0F, gauge * piston.area, 0.0F));
        }
        for (const Door &door : cell.doors) {
            const Lever &lever = levers_[door.lever.value];
            const float shut =
                1.0F - std::clamp(std::abs(lever_angle(door.lever)) / door.full_angle, 0.0F, 1.0F);
            bodies.AddForce(bodies_[lever.body.value].id, door.normal * (gauge * door.area * shut));
        }
    }
    for (const Charge &charge : charges_) {
        const Pool &pool = pools_[charge.pool.value];
        if (lever_angle(charge.valve) <= charge.open_angle || pool.level <= charge.from_y) {
            continue;
        }
        const float push = kWaterDensity * kGravity * (pool.level - charge.base_y) * charge.area;
        bodies.AddForce(bodies_[charge.body.value].id, JPH::Vec3(0.0F, std::max(0.0F, push), 0.0F));
    }
}

void Kit::flow_water(const float delta_seconds) {
    for (Pipe &pipe : pipes_) {
        pipe.flow = 0.0F;
        pipe.pouring = false;
        const float opening = pipe_opening(pipe);
        Pool &from = pools_[pipe.from.value];
        if (opening <= 0.0F) {
            continue;
        }
        const JPH::Vec3 from_size = from.max_corner - from.min_corner;
        const float from_area = from_size.GetX() * from_size.GetZ();
        if (pipe.to.valid()) {
            Pool &to = pools_[pipe.to.value];
            const JPH::Vec3 to_size = to.max_corner - to.min_corner;
            const float to_area = to_size.GetX() * to_size.GetZ();
            const float head_from = std::max(from.level, pipe.from_y);
            const float head_to = std::max(to.level, pipe.to_y);
            const float head = head_from - head_to;
            Pool &source = head > 0.0F ? from : to;
            Pool &sink = head > 0.0F ? to : from;
            const float outlet = head > 0.0F ? pipe.from_y : pipe.to_y;
            if (source.level <= outlet) {
                continue;
            }
            float volume = orifice(pipe.area * opening, kWaterDensity * kGravity * head,
                                   kWaterDensity, kWaterDensity * kGravity * kHeadLinear) *
                           delta_seconds;
            // Never past level: at most what would bring the heads together,
            // and no more than a sealed pool has room for.
            volume = std::min(volume, std::abs(head) / (1.0F / from_area + 1.0F / to_area));
            volume = std::min(volume, pool_volume_at(source, source.level) -
                                          pool_volume_at(source, outlet));
            volume = std::min(volume, pool_volume_at(sink, sink.max_corner.GetY()) -
                                          pool_volume_at(sink, sink.level));
            const float kg = std::max(0.0F, volume) * kWaterDensity;
            source.water -= kg;
            sink.water += kg;
            settle_level(source);
            settle_level(sink);
            pipe.flow = (head > 0.0F ? kg : -kg) / delta_seconds;
            continue;
        }
        const float spout_y = static_cast<float>(pipe.spout.GetY());
        if (from.level <= std::max(pipe.from_y, spout_y)) {
            continue;
        }
        Bin *receiver = nullptr;
        JPH::RVec3 lands = pipe.spout;
        if (!land_stream(pipe.spout, JPH::BodyID(), receiver, lands)) {
            continue;
        }
        float volume = orifice(pipe.area * opening,
                               kWaterDensity * kGravity * (from.level - spout_y), kWaterDensity,
                               kWaterDensity * kGravity * kHeadLinear) *
                       delta_seconds;
        volume = std::min(volume, pool_volume_at(from, from.level) -
                                      pool_volume_at(from, std::max(pipe.from_y, spout_y)));
        float kg = std::max(0.0F, volume) * kWaterDensity;
        if (receiver != nullptr) {
            kg = std::min(kg, std::max(0.0F, receiver->capacity - receiver->contents));
            receiver->contents += kg;
            apply_bin_mass(*receiver);
        } else {
            drained_ += kg;
        }
        if (kg <= 0.0F) {
            continue;
        }
        from.water -= kg;
        settle_level(from);
        pipe.flow = kg / delta_seconds;
        pipe.pouring = true;
        pipe.stream_to = lands;
    }
    for (Charge &charge : charges_) {
        const float y = static_cast<float>(jolt_body(charge.body).GetCenterOfMassPosition().GetY());
        Pool &pool = pools_[charge.pool.value];
        if (lever_angle(charge.valve) > charge.open_angle && pool.level > charge.from_y) {
            // The ram's rise is water out of the pool; a fall pushes it back.
            pool.water = std::max(0.0F, pool.water - kWaterDensity * charge.area * (y - charge.last_y));
            settle_level(pool);
        }
        charge.last_y = y;
    }
}

void Kit::flow_air(const float delta_seconds) {
    for (const Throttle &throttle : throttles_) {
        Cell &a = cells_[throttle.a.value];
        Cell &b = cells_[throttle.b.value];
        const float va = cell_volume(a);
        const float vb = cell_volume(b);
        const float pa = a.air / va;
        const float pb = b.air / vb;
        const float upstream = std::max(pa, pb);
        const float rho = kAirDensity * upstream / kAtmosphere;
        float moved = upstream * orifice(throttle.area, pa - pb, rho, kPressureLinear) * delta_seconds;
        // Never past equal pressure.
        const float equal = (a.air + b.air) / (va + vb);
        moved = std::min(moved, std::abs(a.air - equal * va));
        a.air += pa > pb ? -moved : moved;
        b.air += pa > pb ? moved : -moved;
    }
    for (Cell &cell : cells_) {
        const float volume = cell_volume(cell);
        const float pressure = cell.air / volume;
        const float gauge = pressure - kAtmosphere;
        float leak = cell.bleed;
        for (const Door &door : cell.doors) {
            leak += door.area *
                    std::clamp(std::abs(lever_angle(door.lever)) / door.full_angle, 0.0F, 1.0F);
        }
        if (gauge < -kBreakerOpen) {
            leak += cell.breaker;
        }
        if (leak <= 0.0F) {
            continue;
        }
        const float upstream = std::max(pressure, kAtmosphere);
        const float rho = kAirDensity * upstream / kAtmosphere;
        float moved = upstream * orifice(leak, gauge, rho, kPressureLinear) * delta_seconds;
        moved = std::min(moved, std::abs(cell.air - kAtmosphere * volume));
        cell.air += gauge > 0.0F ? -moved : moved;
    }
}

void Kit::engage_dogs(Guide &guide) {
    if (guide.dog_pitch <= 0.0F || guide.slider == nullptr) {
        return;
    }
    // The highest tooth at or below the body's travel. A pawl that has
    // dropped into a tooth never lifts out, so the floor only ever rises.
    const float travel = guide.slider->GetCurrentPosition();
    const float teeth_below_top =
        std::ceil((guide.max_travel - travel) / guide.dog_pitch - 1.0e-3F);
    const float tooth = guide.max_travel - teeth_below_top * guide.dog_pitch;
    if (tooth > guide.dog_floor + 1.0e-4F) {
        guide.dog_floor = tooth;
        guide.slider->SetLimits(guide.dog_floor, guide.max_travel);
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
    // An uncoupled hose holds nothing: its fluid runs out of the open end.
    if (rope.strut && !rope.anchor.valid()) {
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
    if (rope.strut) {
        // Coupled, the line holds whatever fluid it holds now, and pushes.
        rope.length = JPH::Vec3(settings.mBodyPoint1 - rope.fixed1).Length() +
                      rope.ratio * JPH::Vec3(settings.mBodyPoint2 - rope.fixed2).Length();
        settings.mMinLength = rope.length;
        settings.mMaxLength = rope.length + 1.0e4F;
    }
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
    // not wherever the body is when the pin drops in -- its place and its
    // turn (a door latched a few degrees open is drawn shut). A relatch
    // within seat_tolerance lifts the body those last centimetres, which
    // takes the load off a rope that had caught it. Declared: the catch's
    // spring does at most m g seat_tolerance of work.
    const JPH::Body &body = jolt_body(catch_record.body);
    JPH::FixedConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mAutoDetectPoint = false;
    settings.mPoint1 = catch_record.seat;
    settings.mPoint2 = body.GetCenterOfMassPosition();
    settings.mAxisX1 = catch_record.seat_rotation * JPH::Vec3::sAxisX();
    settings.mAxisY1 = catch_record.seat_rotation * JPH::Vec3::sAxisY();
    settings.mAxisX2 = body.GetRotation() * JPH::Vec3::sAxisX();
    settings.mAxisY2 = body.GetRotation() * JPH::Vec3::sAxisY();
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
        if (!rope->strut && needed > rope->length + kHookLengthTolerance) {
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
    out.dog_floor.resize(guides_.size());
    for (std::size_t index = 0; index < guides_.size(); ++index) {
        out.dog_floor[index] = guides_[index].dog_floor;
    }
    out.bin_contents.resize(bins_.size());
    for (std::size_t index = 0; index < bins_.size(); ++index) {
        out.bin_contents[index] = bins_[index].contents;
    }
    out.catch_latched.resize(catches_.size());
    for (std::size_t index = 0; index < catches_.size(); ++index) {
        out.catch_latched[index] = catches_[index].pin != nullptr;
    }
    out.piles = piles_;
    out.pool_water.resize(pools_.size());
    for (std::size_t index = 0; index < pools_.size(); ++index) {
        out.pool_water[index] = pools_[index].water;
    }
    out.cell_air.resize(cells_.size());
    for (std::size_t index = 0; index < cells_.size(); ++index) {
        out.cell_air[index] = cells_[index].air;
    }
    out.drained = drained_;
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
    for (std::size_t index = 0; index < bins_.size(); ++index) {
        bins_[index].contents = in.bin_contents[index];
        bins_[index].flowing = false;
        apply_bin_mass(bins_[index]);
    }
    piles_ = in.piles;
    for (std::size_t index = 0; index < pools_.size() && index < in.pool_water.size(); ++index) {
        pools_[index].water = in.pool_water[index];
        settle_level(pools_[index]);
    }
    for (std::size_t index = 0; index < cells_.size() && index < in.cell_air.size(); ++index) {
        cells_[index].air = in.cell_air[index];
    }
    drained_ = in.drained;
    for (Charge &charge : charges_) {
        charge.last_y =
            static_cast<float>(jolt_body(charge.body).GetCenterOfMassPosition().GetY());
    }
    for (std::size_t index = 0; index < guides_.size(); ++index) {
        Guide &guide = guides_[index];
        guide.dog_floor = in.dog_floor[index];
        if (guide.slider != nullptr) {
            guide.slider->SetLimits(guide.dog_floor, guide.max_travel);
        }
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

JPH::RVec3 Kit::body_center_of_mass(const BodyIndex body) const noexcept {
    const Body *record = find(bodies_, body);
    if (record == nullptr) {
        return JPH::RVec3::sZero();
    }
    const JPH::BodyInterface &bodies = system_.GetBodyInterfaceNoLock();
    if (record->enabled) {
        return bodies.GetCenterOfMassPosition(record->id);
    }
    return record->parked +
           bodies.GetRotation(record->id) * bodies.GetShape(record->id)->GetCenterOfMass();
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
    if (record == nullptr) {
        return 0.0F;
    }
    float mass = record->mass;
    for (const Bin &bin : bins_) {
        if (bin.body == body && record->dynamic) {
            mass += bin.contents;
        }
    }
    return mass;
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
    if (rope == nullptr) {
        return;
    }
    if (rope->parted) {
        out.push_back(rope->fixed2);
        out.push_back(world_point(rope_end_body(*rope), rope_end_point(*rope)));
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

float Kit::bin_contents(const BinIndex bin) const noexcept {
    const Bin *record = find(bins_, bin);
    return record != nullptr ? record->contents : 0.0F;
}

float Kit::bin_capacity(const BinIndex bin) const noexcept {
    const Bin *record = find(bins_, bin);
    return record != nullptr ? record->capacity : 0.0F;
}

bool Kit::bin_water(const BinIndex bin) const noexcept {
    const Bin *record = find(bins_, bin);
    return record != nullptr && record->water;
}

BodyIndex Kit::bin_body(const BinIndex bin) const noexcept {
    const Bin *record = find(bins_, bin);
    return record != nullptr ? record->body : BodyIndex{};
}

bool Kit::bin_stream(const BinIndex bin, JPH::RVec3 &from, JPH::RVec3 &to) const noexcept {
    const Bin *record = find(bins_, bin);
    if (record == nullptr || !record->flowing) {
        return false;
    }
    from = record->stream_from;
    to = record->stream_to;
    return true;
}

float Kit::pool_level(const PoolIndex pool) const noexcept {
    const Pool *record = find(pools_, pool);
    return record != nullptr ? record->level : 0.0F;
}

float Kit::pool_water(const PoolIndex pool) const noexcept {
    const Pool *record = find(pools_, pool);
    return record != nullptr ? record->water : 0.0F;
}

float Kit::pool_floor(const PoolIndex pool) const noexcept {
    const Pool *record = find(pools_, pool);
    return record != nullptr ? record->min_corner.GetY() : 0.0F;
}

void Kit::pool_box(const PoolIndex pool, JPH::Vec3 &min_corner,
                   JPH::Vec3 &max_corner) const noexcept {
    const Pool *record = find(pools_, pool);
    min_corner = record != nullptr ? record->min_corner : JPH::Vec3::sZero();
    max_corner = record != nullptr ? record->max_corner : JPH::Vec3::sZero();
}

bool Kit::pipe_stream(const PipeIndex pipe, JPH::RVec3 &from, JPH::RVec3 &to) const noexcept {
    const Pipe *record = find(pipes_, pipe);
    if (record == nullptr || !record->pouring) {
        return false;
    }
    from = record->spout;
    to = record->stream_to;
    return true;
}

float Kit::pipe_flow(const PipeIndex pipe) const noexcept {
    const Pipe *record = find(pipes_, pipe);
    return record != nullptr ? record->flow : 0.0F;
}

bool Kit::pipe_whole(const PipeIndex pipe) const noexcept {
    const Pipe *record = find(pipes_, pipe);
    if (record == nullptr) {
        return false;
    }
    Pipe spool_only = *record;
    spool_only.valve = LeverIndex{};
    return pipe_opening(spool_only) > 0.0F;
}

float Kit::cell_pressure(const CellIndex cell) const noexcept {
    const Cell *record = find(cells_, cell);
    return record != nullptr ? record->air / cell_volume(*record) : kAtmosphere;
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
