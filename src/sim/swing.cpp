// AS-012, the swing: a ram and a seat on pendulums of one make, off the 220
// ring's south face (see swing.hpp).
#include "sim/swing.hpp"

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace scraperx::sim {
namespace {
using namespace JPH;

kit::Part box(Vec3 half, Vec3 offset, kit::Material material, float mass = 0.0F) {
    kit::Part result;
    result.half = half;
    result.offset = offset;
    result.material = material;
    result.mass_kg = mass;
    return result;
}

// A round member from a to b (a capsule along its local y).
kit::Part member(Vec3 a, Vec3 b, float radius, kit::Material material, float mass = 0.0F) {
    kit::Part result;
    const Vec3 span = b - a;
    result.shape = kit::Part::Shape::Cylinder;
    result.half = Vec3(radius, span.Length() * 0.5F, radius);
    result.offset = (a + b) * 0.5F;
    result.rotation = Quat::sFromTo(Vec3::sAxisY(), span.Normalized());
    result.material = material;
    result.mass_kg = mass;
    return result;
}

constexpr float kGangwayX = -13.35F;
constexpr float kGangwayHalfX = 0.6F;
constexpr float kGangwayTop = 220.25F;
constexpr float kFaceZ = -126.73F;   // the 220 ring's outer edge
constexpr float kGangwayEndZ = -97.9F;
constexpr float kWestGirderX = -18.5F;
constexpr float kEastGirderX = -12.7F;
constexpr float kGirderY = 252.0F;
constexpr float kGirderRootZ = -129.0F;
constexpr float kGirderTipZ = -94.5F;
// The 286 ring's outer edge, where the stays are made fast.
constexpr float kStayTopZ = -129.6F;
constexpr float kStayTopY = 286.55F;
constexpr float kDeck242Top = 242.25F;

// The machine's frame, in world coordinates about this origin.
const RVec3 kFrameOrigin(-15.0, 220.0, -112.0);

Vec3 local(float x, float y, float z) {
    return Vec3(x - float(kFrameOrigin.GetX()), y - float(kFrameOrigin.GetY()),
                z - float(kFrameOrigin.GetZ()));
}

std::vector<kit::Part> frame_parts() {
    std::vector<kit::Part> parts;
    // The gangway from the 220 ring out to the seat: a galvanised deck on two
    // stringers, rails either side; the west rail stops short of the seat.
    const float length = kGangwayEndZ - kFaceZ;
    const float mid = (kGangwayEndZ + kFaceZ) * 0.5F;
    parts.push_back(box(Vec3(kGangwayHalfX, 0.06F, length * 0.5F),
                        local(kGangwayX, kGangwayTop - 0.06F, mid), kit::Material::Galvanised));
    for (const float side : {-1.0F, 1.0F})
        parts.push_back(box(Vec3(0.06F, 0.18F, length * 0.5F),
                            local(kGangwayX + side * (kGangwayHalfX - 0.06F), kGangwayTop - 0.30F, mid),
                            kit::Material::Steel));
    const float east_rail_x = kGangwayX + kGangwayHalfX - 0.03F;
    const float west_rail_x = kGangwayX - kGangwayHalfX + 0.03F;
    const float west_end = -100.3F;
    parts.push_back(box(Vec3(0.03F, 0.03F, length * 0.5F), local(east_rail_x, kGangwayTop + 1.05F, mid),
                        kit::Material::Yellow));
    parts.push_back(box(Vec3(0.03F, 0.03F, (west_end - kFaceZ) * 0.5F),
                        local(west_rail_x, kGangwayTop + 1.05F, (west_end + kFaceZ) * 0.5F),
                        kit::Material::Yellow));
    parts.push_back(box(Vec3(kGangwayHalfX, 0.03F, 0.03F), local(kGangwayX, kGangwayTop + 1.05F, kGangwayEndZ + 0.03F),
                        kit::Material::Yellow));
    for (float z = kFaceZ + 0.4F; z > kGangwayEndZ; z -= 3.2F) {
        parts.push_back(box(Vec3(0.03F, 0.52F, 0.03F), local(east_rail_x, kGangwayTop + 0.52F, z),
                            kit::Material::Yellow));
        if (z > west_end)
            parts.push_back(box(Vec3(0.03F, 0.52F, 0.03F), local(west_rail_x, kGangwayTop + 0.52F, z),
                                kit::Material::Yellow));
    }
    for (const float x : {east_rail_x, kGangwayX - kGangwayHalfX + 0.03F})
        parts.push_back(box(Vec3(0.03F, 0.52F, 0.03F), local(x, kGangwayTop + 0.52F, kGangwayEndZ + 0.03F),
                            kit::Material::Yellow));
    // Hangers from the east girder carry the gangway's outer stringer.
    for (const float z : {-121.0F, -113.0F, -105.0F, -98.4F})
        parts.push_back(member(local(kEastGirderX, kGirderY - 0.5F, z),
                               local(kEastGirderX, kGangwayTop - 0.30F, z), 0.05F, kit::Material::Steel));
    // The jib: two girders from posts on the 242 ring out past the ram's axle.
    const float girder_mid = (kGirderRootZ + kGirderTipZ) * 0.5F;
    const float girder_half = (kGirderTipZ - kGirderRootZ) * 0.5F;
    for (const float x : {kWestGirderX, kEastGirderX}) {
        parts.push_back(box(Vec3(0.22F, 0.5F, girder_half), local(x, kGirderY, girder_mid), kit::Material::Rust));
        parts.push_back(box(Vec3(0.25F, (kGirderY - 0.5F - kDeck242Top) * 0.5F, 0.25F),
                            local(x, (kGirderY - 0.5F + kDeck242Top) * 0.5F, kGirderRootZ), kit::Material::Rust));
        // Stays from the jib's tip to the 286 ring.
        parts.push_back(member(local(x, kGirderY + 0.5F, kGirderTipZ + 0.3F), local(x, kStayTopY, kStayTopZ),
                               0.07F, kit::Material::Steel));
        parts.push_back(box(Vec3(0.3F, 0.15F, 0.3F), local(x, kStayTopY + 0.0F, kStayTopZ), kit::Material::Steel));
    }
    // Cross ties between the girders, and the two axles the arms hang on.
    for (const float z : {-127.0F, -118.0F, -109.0F})
        parts.push_back(box(Vec3((kEastGirderX - kWestGirderX) * 0.5F, 0.12F, 0.12F),
                            local((kEastGirderX + kWestGirderX) * 0.5F, kGirderY + 0.38F, z), kit::Material::Rust));
    for (const double z : {Swing::kSeatPivotZ, Swing::kRamPivotZ}) {
        kit::Part axle = member(local(kWestGirderX + 0.2F, float(Swing::kPivotY), float(z)),
                                local(kEastGirderX - 0.2F, float(Swing::kPivotY), float(z)), 0.16F,
                                kit::Material::Steel);
        parts.push_back(axle);
        // A hanger block from each girder down to the axle.
        for (const float x : {kWestGirderX, kEastGirderX})
            parts.push_back(box(Vec3(0.2F, 0.3F, 0.3F), local(x, float(Swing::kPivotY) + 0.15F, float(z)),
                                kit::Material::Steel));
    }
    // The ram's hook, under the girders north of its axle, and the rack the
    // seat's pawl rides at the top of its arc: teeth on a curved rail east of
    // the seat, at its floor's height.
    parts.push_back(box(Vec3(0.30F, 0.45F, 0.22F), local(float(Swing::kPlaneX), float(Swing::kPivotY) + 0.25F, -97.05F),
                        kit::Material::Yellow));
    const double floor_centre_y = Swing::kPivotY - Swing::kPinAboveFloorM;
    for (int tooth = 0; tooth < Swing::kTeeth; ++tooth) {
        const double angle = Swing::kFirstToothRad + tooth * Swing::kToothPitchRad;
        const double y = floor_centre_y - Swing::kArmLengthM * std::cos(angle);
        const double z = Swing::kSeatPivotZ - Swing::kArmLengthM * std::sin(angle);
        kit::Part tooth_part = box(Vec3(0.10F, 0.06F, 0.12F), local(-14.05F, float(y) - 0.05F, float(z)),
                                   kit::Material::Yellow);
        tooth_part.rotation = Quat::sRotation(Vec3::sAxisX(), float(angle));
        parts.push_back(tooth_part);
    }
    {
        const double a0 = Swing::kFirstToothRad - 0.01;
        const double a1 = Swing::kFirstToothRad + (Swing::kTeeth - 1) * Swing::kToothPitchRad + 0.01;
        const Vec3 p0 = local(-13.92F, float(floor_centre_y - Swing::kArmLengthM * std::cos(a0)) - 0.2F,
                              float(Swing::kSeatPivotZ - Swing::kArmLengthM * std::sin(a0)));
        const Vec3 p1 = local(-13.92F, float(floor_centre_y - Swing::kArmLengthM * std::cos(a1)) - 0.2F,
                              float(Swing::kSeatPivotZ - Swing::kArmLengthM * std::sin(a1)));
        parts.push_back(member(p0, p1, 0.09F, kit::Material::Steel));
        // The rack's bracket back to the 242 ring.
        parts.push_back(member(p1, local(-13.92F, kDeck242Top - 0.3F, -128.4F), 0.08F, kit::Material::Steel));
    }
    return parts;
}

// An arm: two round chords 0.5 m apart, laced, from the axle down to the pin;
// a tail past the axle carries the ram's hook (and balances nothing).
std::vector<kit::Part> arm_parts(bool tail) {
    std::vector<kit::Part> parts;
    const float length = float(Swing::kArmLengthM);
    const float chord_mass = Swing::kArmMassKg * (tail ? 0.40F : 0.42F);
    for (const float x : {-0.25F, 0.25F})
        parts.push_back(member(Vec3(x, 0.0F, 0.0F), Vec3(x, -length, 0.0F), 0.06F, kit::Material::Rust, chord_mass));
    const int bays = 12;
    const float lacing_mass = Swing::kArmMassKg * 0.16F / float(bays);
    for (int bay = 0; bay < bays; ++bay) {
        const float y0 = -length * float(bay) / float(bays);
        const float y1 = -length * float(bay + 1) / float(bays);
        const float x0 = (bay % 2 == 0) ? -0.25F : 0.25F;
        parts.push_back(member(Vec3(x0, y0, 0.0F), Vec3(-x0, y1, 0.0F), 0.035F, kit::Material::Rust, lacing_mass));
    }
    if (tail)
        parts.push_back(member(Vec3(0.0F, 0.0F, 0.0F), Vec3(0.0F, 1.5F, 0.0F), 0.08F, kit::Material::Yellow,
                               Swing::kArmMassKg * 0.04F));
    return parts;
}

// The seat, about its pin: a floor, a back plate (the buffer's striker), a
// yoke over the rider up to the pin, a rail on the closed west side. The east
// side is open to the gangway, the north side to the deck it is sent to.
std::vector<kit::Part> seat_parts() {
    const float floor_y = -float(Swing::kPinAboveFloorM);
    const float depth = float(Swing::kSeatHalfDepthM);
    return {
        box(Vec3(0.75F, 0.06F, depth), Vec3(0.0F, floor_y - 0.06F, 0.0F), kit::Material::Hazard, 60.0F),
        box(Vec3(0.75F, 0.75F, 0.06F), Vec3(0.0F, floor_y + 0.85F, depth - 0.06F), kit::Material::Yellow, 40.0F),
        // The yoke over the rider: a bar at the pin, carried back to two
        // posts at the back corners, so the east side is open to step in.
        box(Vec3(0.75F, 0.07F, 0.07F), Vec3(0.0F, 0.0F, 0.0F), kit::Material::Steel, 8.0F),
        box(Vec3(0.04F, 0.04F, 0.36F), Vec3(-0.71F, 0.0F, 0.36F), kit::Material::Steel, 2.0F),
        box(Vec3(0.04F, 0.04F, 0.36F), Vec3(0.71F, 0.0F, 0.36F), kit::Material::Steel, 2.0F),
        box(Vec3(0.04F, 1.2F, 0.04F), Vec3(-0.71F, -1.2F, depth - 0.1F), kit::Material::Steel, 9.0F),
        box(Vec3(0.04F, 1.2F, 0.04F), Vec3(0.71F, -1.2F, depth - 0.1F), kit::Material::Steel, 9.0F),
        box(Vec3(0.03F, 0.03F, depth), Vec3(-0.71F, floor_y + 1.0F, 0.0F), kit::Material::Yellow, 10.0F),
        box(Vec3(0.03F, 0.5F, 0.03F), Vec3(-0.71F, floor_y + 0.5F, -depth + 0.03F), kit::Material::Yellow, 5.0F),
        box(Vec3(0.03F, 0.5F, 0.03F), Vec3(-0.71F, floor_y + 0.5F, depth - 0.03F), kit::Material::Yellow, 5.0F),
    };
}

// The ram, about its pin: an iron-banded timber drum across the swing, its
// face to the north, on a yoke up to its pin.
std::vector<kit::Part> ram_parts() {
    kit::Part drum;
    drum.shape = kit::Part::Shape::Cylinder;
    drum.half = Vec3(float(Swing::kRamRadiusM), 0.75F, float(Swing::kRamRadiusM));
    drum.offset = Vec3(0.0F, -1.7F, 0.0F);
    drum.rotation = Quat::sRotation(Vec3::sAxisZ(), 0.5F * JPH_PI);
    drum.material = kit::Material::Timber;
    drum.mass_kg = 190.0F;
    std::vector<kit::Part> parts{drum};
    for (const float x : {-0.55F, 0.0F, 0.55F}) {
        kit::Part band = drum;
        band.half = Vec3(float(Swing::kRamRadiusM) + 0.02F, 0.05F, float(Swing::kRamRadiusM) + 0.02F);
        band.offset = Vec3(x, -1.7F, 0.0F);
        band.material = kit::Material::Steel;
        band.mass_kg = 6.0F;
        parts.push_back(band);
    }
    parts.push_back(box(Vec3(0.75F, 0.07F, 0.07F), Vec3(0.0F, 0.0F, 0.0F), kit::Material::Steel, 9.0F));
    parts.push_back(box(Vec3(0.04F, 0.8F, 0.04F), Vec3(-0.71F, -0.85F, 0.0F), kit::Material::Steel, 4.0F));
    parts.push_back(box(Vec3(0.04F, 0.8F, 0.04F), Vec3(0.71F, -0.85F, 0.0F), kit::Material::Steel, 4.0F));
    return parts;
}

void level_only(PhysicsSystem &system, BodyID id, float mass) {
    // Kept level by its parallel link: rotation is not a degree of freedom.
    BodyLockWrite lock(system.GetBodyLockInterface(), id);
    auto *motion = lock.GetBody().GetMotionProperties();
    auto properties = lock.GetBody().GetShape()->GetMassProperties();
    properties.ScaleToMass(mass);
    motion->SetMassProperties(EAllowedDOFs::TranslationX | EAllowedDOFs::TranslationY | EAllowedDOFs::TranslationZ,
                              properties);
}

Ref<HingeConstraint> world_hinge(PhysicsSystem &system, BodyID arm, RVec3Arg pivot, float min, float max) {
    HingeConstraintSettings settings;
    settings.mSpace = EConstraintSpace::WorldSpace;
    settings.mPoint1 = settings.mPoint2 = pivot;
    settings.mHingeAxis1 = settings.mHingeAxis2 = Vec3::sAxisX();
    settings.mNormalAxis1 = settings.mNormalAxis2 = -Vec3::sAxisY();
    settings.mLimitsMin = min;
    settings.mLimitsMax = max;
    BodyLockWrite lock(system.GetBodyLockInterface(), arm);
    Ref<HingeConstraint> hinge = static_cast<HingeConstraint *>(settings.Create(Body::sFixedToWorld, lock.GetBody()));
    system.AddConstraint(hinge);
    return hinge;
}

Ref<PointConstraint> pin(PhysicsSystem &system, BodyID arm, BodyID hung, RVec3Arg at) {
    PointConstraintSettings settings;
    settings.mSpace = EConstraintSpace::WorldSpace;
    settings.mPoint1 = settings.mPoint2 = at;
    const BodyID ids[2] = {arm, hung};
    BodyLockMultiWrite lock(system.GetBodyLockInterface(), ids, 2);
    Ref<PointConstraint> constraint =
        static_cast<PointConstraint *>(settings.Create(*lock.GetBody(0), *lock.GetBody(1)));
    system.AddConstraint(constraint);
    return constraint;
}

RVec3 pin_at(RVec3Arg pivot, double angle) {
    return pivot + RVec3(0.0, -Swing::kArmLengthM * std::cos(angle), -Swing::kArmLengthM * std::sin(angle));
}
} // namespace

RVec3 Swing::seat_pivot() noexcept { return RVec3(kPlaneX, kPivotY, kSeatPivotZ); }
RVec3 Swing::ram_pivot() noexcept { return RVec3(kPlaneX, kPivotY, kRamPivotZ); }

RVec3 Swing::seat_pin() const { return system_.GetBodyInterface().GetPosition(kit_.body_id(seat_)); }
RVec3 Swing::ram_pin() const { return system_.GetBodyInterface().GetPosition(kit_.body_id(ram_)); }

Swing::Swing(PhysicsSystem &system, kit::Kit &kit, BodyID player)
    : system_(system), kit_(kit), player_(player) {
    {
        BodyLockRead lock(system_.GetBodyLockInterface(), player_);
        saved_player_group_ = lock.GetBody().GetCollisionGroup();
    }
    frame_ = kit_.add_body(kFrameEntity, frame_parts(), kFrameOrigin, Quat::sIdentity(), 0.0F, 0.8F);

    // As found: the seat hangs plumb at the gangway's end; the ram is hung
    // back on its hook.
    // Both are built plumb, where their hinges read zero, and the ram is then
    // hung back on its hook.
    const RVec3 seat_at = pin_at(seat_pivot(), 0.0);
    const RVec3 ram_at = pin_at(ram_pivot(), 0.0);
    seat_arm_ = kit_.add_body(kSeatArmEntity, arm_parts(false), seat_pivot(), Quat::sIdentity(), kArmMassKg, 0.5F);
    ram_arm_ = kit_.add_body(kRamArmEntity, arm_parts(true), ram_pivot(), Quat::sIdentity(), kArmMassKg, 0.5F);
    seat_ = kit_.add_body(kSeatEntity, seat_parts(), seat_at, Quat::sIdentity(), kSeatMassKg, 0.9F);
    ram_ = kit_.add_body(kRamEntity, ram_parts(), ram_at, Quat::sIdentity(), kRamMassKg, 0.5F);
    // The kick bar across the seat's front, at the rider's shins.
    const std::vector<kit::Part> bar_parts{
        box(Vec3(0.55F, 0.05F, 0.05F), Vec3::sZero(), kit::Material::Hazard, kKickBarMassKg)};
    const RVec3 bar_at = seat_at + RVec3(0.0, -kPinAboveFloorM + 0.38, -kSeatHalfDepthM + 0.12);
    kick_bar_ = kit_.add_body(kKickBarEntity, bar_parts, bar_at, Quat::sIdentity(), kKickBarMassKg, 0.5F);

    for (const auto body : {seat_arm_, ram_arm_, seat_, ram_, kick_bar_}) {
        kit_.set_damping(body, 0.0F, 0.0F);
        kit_.disable_collision(body, frame_);
    }
    const kit::BodyIndex bodies[] = {seat_arm_, ram_arm_, seat_, ram_, kick_bar_};
    for (std::size_t i = 0; i < 5; ++i)
        for (std::size_t j = i + 1; j < 5; ++j) kit_.disable_collision(bodies[i], bodies[j]);
    // The rider while harnessed (the kit's last sub-group): through the seat
    // and its bar, as a rider in a pouch.
    kit_.disable_collision(kit::BodyIndex{kit::Kit::kCollisionSubGroups - 1}, seat_);
    kit_.disable_collision(kit::BodyIndex{kit::Kit::kCollisionSubGroups - 1}, kick_bar_);
    level_only(system_, kit_.body_id(seat_), kSeatMassKg);
    level_only(system_, kit_.body_id(ram_), kRamMassKg);
    level_only(system_, kit_.body_id(kick_bar_), kKickBarMassKg);
    // Under the rider's shin, in the seat: nothing but the leg moves it, so a
    // body walking in cannot trip the ram.
    system_.GetBodyInterface().SetIsSensor(kit_.body_id(kick_bar_), true);
    kit_.set_continuous_collision(seat_);
    kit_.set_continuous_collision(ram_);

    // The seat's arm hangs on a stop at plumb: nothing on it can swing it back
    // toward the ram; it goes only north, up its arc.
    seat_hinge_ = world_hinge(system_, kit_.body_id(seat_arm_), seat_pivot(), 0.0F, float(kSeatTopStopRad));
    ram_hinge_ = world_hinge(system_, kit_.body_id(ram_arm_), ram_pivot(), -1.52F, 1.0F);
    seat_pin_ = pin(system_, kit_.body_id(seat_arm_), kit_.body_id(seat_), seat_at);
    ram_pin_ = pin(system_, kit_.body_id(ram_arm_), kit_.body_id(ram_), ram_at);
    {
        SliderConstraintSettings settings;
        settings.mSpace = EConstraintSpace::WorldSpace;
        settings.mPoint1 = settings.mPoint2 = bar_at;
        settings.SetSliderAxis(-Vec3::sAxisZ());
        settings.mLimitsMin = 0.0F;
        settings.mLimitsMax = float(kKickTravelM);
        const BodyID ids[2] = {kit_.body_id(seat_), kit_.body_id(kick_bar_)};
        BodyLockMultiWrite lock(system_.GetBodyLockInterface(), ids, 2);
        kick_slide_ = static_cast<SliderConstraint *>(settings.Create(*lock.GetBody(0), *lock.GetBody(1)));
        system_.AddConstraint(kick_slide_);
    }
    {
        auto &bodies = system_.GetBodyInterface();
        bodies.SetPositionAndRotation(kit_.body_id(ram_arm_), ram_pivot(),
                                      Quat::sRotation(Vec3::sAxisX(), float(kRamHeldAngleRad)), EActivation::Activate);
        bodies.SetPosition(kit_.body_id(ram_), pin_at(ram_pivot(), kRamHeldAngleRad), EActivation::Activate);
    }
    hold_ram();
    state_.apex_floor_y = seat_at.GetY() - kPinAboveFloorM;
    state_.initial_mechanical_j = machine_energy(false);
    state_.ledger_valid = true;
    refresh_state();
    system_.AddStepListener(this);
}

Swing::~Swing() {
    system_.RemoveStepListener(this);
    detach();
    for (Constraint *constraint : {static_cast<Constraint *>(kick_slide_.GetPtr()),
                                   static_cast<Constraint *>(seat_pin_.GetPtr()),
                                   static_cast<Constraint *>(ram_pin_.GetPtr()),
                                   static_cast<Constraint *>(seat_hinge_.GetPtr()),
                                   static_cast<Constraint *>(ram_hinge_.GetPtr())})
        if (constraint != nullptr) system_.RemoveConstraint(constraint);
}

void Swing::hold_ram() {
    // The hook takes the arm's tail: it cannot come off the hook toward plumb.
    ram_hinge_->SetLimits(-1.52F, float(kRamHeldAngleRad));
    state_.ram_held = true;
}

void Swing::release_ram() {
    ram_hinge_->SetLimits(-1.52F, 1.0F);
    state_.ram_held = false;
    state_.tripped = true;
}

void Swing::begin_ride_ledger() {
    // The ride's ledger opens as the hook lets go: what the machine and its
    // rider hold then, against what they hold, store and lose after.
    state_.initial_mechanical_j = machine_energy(harness_ != nullptr);
    state_.kick_work_j = 0.0;
    state_.buffer_loss_j = 0.0;
    step_buffer_loss_j_.store(0.0, std::memory_order_relaxed);
    state_.peak_buffer_force_n = 0.0;
    state_.peak_seat_accel_mps2 = 0.0;
    state_.ledger_valid = true;
}

void Swing::set_tooth(const int tooth) {
    state_.tooth = tooth;
    const float min = tooth < 0 ? 0.0F : float(kFirstToothRad + tooth * kToothPitchRad);
    seat_hinge_->SetLimits(min, float(kSeatTopStopRad));
}

bool Swing::player_in_seat() const {
    const auto &bodies = system_.GetBodyInterface();
    const RVec3 relative = bodies.GetPosition(player_) - bodies.GetPosition(kit_.body_id(seat_));
    constexpr double player_half_height = 0.90;
    const double soles = relative.GetY() - player_half_height + kPinAboveFloorM;
    return std::abs(relative.GetX()) <= 0.40 && relative.GetZ() >= -0.50 && relative.GetZ() <= 0.40 &&
           soles >= -0.05 && soles <= 0.15;
}

void Swing::attach() {
    if (harness_ != nullptr || !player_in_seat()) return;
    BodyLockInterfaceNoLock const &locks = system_.GetBodyLockInterfaceNoLock();
    Body *seat = locks.TryGetBody(kit_.body_id(seat_));
    Body *player = locks.TryGetBody(player_);
    if (seat == nullptr || player == nullptr) return;
    // Held where the rider stands: fixed across, along and up the seat.
    SixDOFConstraintSettings settings;
    settings.mSpace = EConstraintSpace::WorldSpace;
    settings.mPosition1 = settings.mPosition2 = player->GetCenterOfMassPosition();
    settings.MakeFixedAxis(SixDOFConstraintSettings::TranslationX);
    settings.MakeFixedAxis(SixDOFConstraintSettings::TranslationY);
    settings.MakeFixedAxis(SixDOFConstraintSettings::TranslationZ);
    settings.mNumVelocityStepsOverride = 30;
    settings.mNumPositionStepsOverride = 6;
    harness_ = static_cast<SixDOFConstraint *>(settings.Create(*seat, *player));
    system_.AddConstraint(harness_);
    auto group = seat->GetCollisionGroup();
    group.SetSubGroupID(kit::Kit::kCollisionSubGroups - 1);
    system_.GetBodyInterface().SetCollisionGroup(player_, group);
    state_.seated = true;
    // The rider joins the ledger with the energy they bring.
    state_.initial_mechanical_j += machine_energy(true) - machine_energy(false);
}

void Swing::detach() {
    if (harness_ != nullptr) {
        // ... and leaves it with the energy they take.
        state_.initial_mechanical_j -= machine_energy(true) - machine_energy(false);
        system_.RemoveConstraint(harness_);
        harness_ = nullptr;
        system_.GetBodyInterface().SetCollisionGroup(player_, saved_player_group_);
    }
    state_.seated = false;
    state_.kicking = false;
}

double Swing::machine_energy(const bool include_rider) const {
    const auto &locks = system_.GetBodyLockInterface();
    const Vec3 gravity = system_.GetGravity();
    double total = 0.0;
    std::vector<BodyID> ids{kit_.body_id(seat_arm_), kit_.body_id(ram_arm_), kit_.body_id(seat_),
                            kit_.body_id(ram_), kit_.body_id(kick_bar_)};
    if (include_rider) ids.push_back(player_);
    for (const BodyID id : ids) {
        BodyLockRead lock(locks, id);
        if (!lock.Succeeded()) continue;
        const Body &body = lock.GetBody();
        const MotionProperties *motion = body.GetMotionProperties();
        if (motion == nullptr || motion->GetInverseMass() <= 0.0F) continue;
        const double mass = 1.0 / motion->GetInverseMass();
        const Vec3 v = body.GetLinearVelocity();
        const Vec3 w = body.GetAngularVelocity();
        double rotational = 0.0;
        if (w.LengthSq() > 0.0F) {
            const Mat44 inverse_inertia = body.GetInverseInertia();
            // I = (I^-1)^-1 on the allowed axes; the arms turn about x alone.
            const Vec3 axis = w.Normalized();
            const double inverse_about = axis.Dot(inverse_inertia.Multiply3x3(axis));
            if (inverse_about > 0.0) rotational = 0.5 * w.LengthSq() / inverse_about;
        }
        total += 0.5 * mass * v.LengthSq() + rotational -
                 mass * double(gravity.GetY()) * double(body.GetCenterOfMassPosition().GetY());
    }
    return total;
}

void Swing::pre_step(const float delta_seconds, const bool action, const bool drop) {
    // The harness opens before the kick, or with the seat at rest on the rack:
    // never mid-ride, where a rider let out would stand on a swinging seat.
    if (drop && harness_ != nullptr && state_.may_leave) {
        detach();
    } else if (action) {
        if (harness_ == nullptr) {
            attach();
        } else if (state_.ram_held && !state_.kicking) {
            state_.kicking = true;
            state_.kick_elapsed_s = 0.0;
        } else if (state_.tooth >= 0 && !state_.ram_held && state_.may_leave) {
            // Held at the top: unbuckle and step off.
            detach();
        }
    }
    if (state_.kicking) {
        state_.kick_elapsed_s += delta_seconds;
        if (state_.kick_elapsed_s >= kKickSeconds || harness_ == nullptr) state_.kicking = false;
    }
    kick_requested_ = state_.kicking;
}

void Swing::OnStep(const PhysicsStepListenerContext &context) {
    auto &bodies = system_.GetBodyInterfaceNoLock();
    const BodyID seat = kit_.body_id(seat_);
    const BodyID ram = kit_.body_id(ram_);
    // The buffer: the ram's face is kBufferFreeLength north of its drum; the
    // seat's back is its striker. Both stay level, so the gap is along z.
    const RVec3 seat_at = bodies.GetPosition(seat);
    const RVec3 ram_at = bodies.GetPosition(ram);
    const double face_z = ram_at.GetZ() - kRamRadiusM - kBufferFreeLengthM;
    const double back_z = seat_at.GetZ() + kSeatHalfDepthM;
    const double compression = back_z - face_z;
    const double vertical = std::abs(double(ram_at.GetY() - seat_at.GetY()));
    double force = 0.0;
    double loss_power = 0.0;
    double stored = 0.0;
    if (compression > 0.0 && vertical < 0.8 && compression < kBufferFreeLengthM + 0.3) {
        // The rate the buffer closes: the seat's back coming south into the
        // face, the face going north into the back.
        const double rate = double(bodies.GetLinearVelocity(seat).GetZ() - bodies.GetLinearVelocity(ram).GetZ());
        const double elastic = kBufferStiffnessNPerM * compression;
        force = std::max(0.0, elastic + kBufferDampingNsPerM * rate);
        loss_power = std::max(0.0, (force - elastic) * rate);
        stored = 0.5 * kBufferStiffnessNPerM * compression * compression;
    }
    if (force > 0.0) {
        bodies.AddForce(seat, Vec3(0.0F, 0.0F, -float(force)));
        bodies.AddForce(ram, Vec3(0.0F, 0.0F, float(force)));
    }
    // The rider's leg on the kick bar, the reaction on the rider; the bar's
    // return spring against the seat.
    const BodyID bar = kit_.body_id(kick_bar_);
    const double travel = kick_slide_->GetCurrentPosition();
    const double back = kKickReturnNPerM * travel;
    bodies.AddForce(bar, Vec3(0.0F, 0.0F, float(back)));
    bodies.AddForce(seat, Vec3(0.0F, 0.0F, -float(back)));
    if (kick_requested_ && harness_ != nullptr && travel < kKickTravelM - 0.005) {
        bodies.AddForce(bar, Vec3(0.0F, 0.0F, -float(kKickForceN)));
        bodies.AddForce(player_, Vec3(0.0F, 0.0F, float(kKickForceN)));
        state_.kick_work_j += kKickForceN * std::abs(double(bodies.GetLinearVelocity(bar).GetZ() -
                                                            bodies.GetLinearVelocity(player_).GetZ())) *
                              context.mDeltaTime;
    }
    step_buffer_force_n_.store(force, std::memory_order_relaxed);
    step_buffer_compression_m_.store(std::max(0.0, compression), std::memory_order_relaxed);
    step_buffer_stored_j_.store(stored, std::memory_order_relaxed);
    step_buffer_loss_j_.store(step_buffer_loss_j_.load(std::memory_order_relaxed) + loss_power * context.mDeltaTime,
                              std::memory_order_relaxed);
}

void Swing::post_step(const float delta_seconds) {
    // The trip wire: the bar pushed past its trip travel lets the hook go.
    state_.kick_travel_m = kick_slide_->GetCurrentPosition();
    if (state_.ram_held && state_.kick_travel_m >= kTripTravelM) {
        release_ram();
        begin_ride_ledger();
    }
    // The ratchet: the pawl drops into each tooth the seat's arm passes.
    const double angle = seat_hinge_->GetCurrentAngle();
    for (int tooth = kTeeth - 1; tooth > state_.tooth; --tooth) {
        if (angle >= kFirstToothRad + tooth * kToothPitchRad + 0.002) {
            set_tooth(tooth);
            break;
        }
    }
    const Vec3 seat_velocity = system_.GetBodyInterface().GetLinearVelocity(kit_.body_id(seat_));
    if (delta_seconds > 0.0F)
        state_.peak_seat_accel_mps2 = std::max(state_.peak_seat_accel_mps2,
                                               double((seat_velocity - previous_seat_velocity_).Length()) / delta_seconds);
    previous_seat_velocity_ = seat_velocity;
    state_.buffer_force_n = step_buffer_force_n_.load(std::memory_order_relaxed);
    state_.buffer_compression_m = step_buffer_compression_m_.load(std::memory_order_relaxed);
    state_.buffer_loss_j = step_buffer_loss_j_.load(std::memory_order_relaxed);
    state_.peak_buffer_force_n = std::max(state_.peak_buffer_force_n, state_.buffer_force_n);
    refresh_state();
}

void Swing::refresh_state() {
    const auto &bodies = system_.GetBodyInterface();
    state_.seat_angle_rad = seat_hinge_->GetCurrentAngle();
    state_.ram_angle_rad = ram_hinge_->GetCurrentAngle();
    state_.seat_speed_mps = bodies.GetLinearVelocity(kit_.body_id(seat_)).Length();
    state_.ram_speed_mps = bodies.GetLinearVelocity(kit_.body_id(ram_)).Length();
    state_.seat_floor_y = bodies.GetPosition(kit_.body_id(seat_)).GetY() - kPinAboveFloorM;
    state_.apex_floor_y = std::max(state_.apex_floor_y, state_.seat_floor_y);
    state_.station_available = harness_ == nullptr && state_.ram_held && player_in_seat();
    state_.may_leave = state_.ram_held || (state_.tooth >= 0 && state_.seat_speed_mps < 0.3);
    state_.mechanical_j = machine_energy(harness_ != nullptr);
    state_.buffer_stored_j = step_buffer_stored_j_.load(std::memory_order_relaxed);
    state_.energy_residual_j = state_.mechanical_j + state_.buffer_stored_j + state_.buffer_loss_j -
                               state_.kick_work_j - state_.initial_mechanical_j;
}

void Swing::restore(const State &saved) {
    detach();
    const double loss = saved.buffer_loss_j;
    state_ = saved;
    state_.seated = false;
    state_.kicking = false;
    kick_requested_ = false;
    step_buffer_loss_j_.store(loss, std::memory_order_relaxed);
    step_buffer_force_n_.store(0.0, std::memory_order_relaxed);
    step_buffer_compression_m_.store(0.0, std::memory_order_relaxed);
    step_buffer_stored_j_.store(0.0, std::memory_order_relaxed);
    if (saved.ram_held) hold_ram();
    else release_ram();
    state_.tripped = saved.tripped;
    set_tooth(saved.tooth);
    previous_seat_velocity_ = system_.GetBodyInterface().GetLinearVelocity(kit_.body_id(seat_));
    refresh_state();
}

} // namespace scraperx::sim
