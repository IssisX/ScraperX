// The yard's forklift (see forklift.hpp).
#include "sim/forklift.hpp"

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Constraints/MotorSettings.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace scraperx::sim {
namespace {
using namespace JPH;
using kit::Material;
using kit::Part;

Part box(const Vec3 half, const Vec3 offset, const Material material, const float mass_kg) {
    return {half, offset, Quat::sIdentity(), material, Part::Shape::Box, mass_kg};
}

// The truck, in its own frame: +z forward, +y up, its origin on the ground
// under the middle of its frame. Masses CHOSEN for a 3.5 t-class
// counterbalance truck (about 5.1 t empty: the counterweight, the engine and
// transmission in the frame, the drive axle, the mast); the centre of mass
// falls between the axles about 1.05 m behind the front ones, near
// three-quarters of the empty weight on the rear wheels, as an empty one's
// does: the front wheels carry enough to drive it, and it balances about
// 5.5 t m about them, 3.5 t half a metre out in front of the forks' face.
std::vector<Part> chassis_parts() {
    std::vector<Part> parts;
    // The frame, its top the driver's floor (0.32 m: a step up from the
    // ground), the engine and transmission in it under the seat.
    parts.push_back(box(Vec3(0.38F, 0.10F, 0.85F), Vec3(0.0F, 0.22F, -0.15F), Material::Yellow, 1600.0F));
    // The drive axle's housing across between the front wheels.
    parts.push_back(box(Vec3(0.42F, 0.08F, 0.08F), Vec3(0.0F, 0.41F, 0.55F), Material::Steel, 250.0F));
    // The counterweight, behind the rear axle, and on it the propane tank it
    // runs on, lying across the back in its cradle under two straps.
    parts.push_back(box(Vec3(0.56F, 0.40F, 0.18F), Vec3(0.0F, 0.55F, -1.42F), Material::Steel, 2300.0F));
    const Quat across = Quat::sRotation(Vec3::sAxisZ(), 0.5F * JPH_PI);
    parts.push_back({Vec3(0.16F, 0.32F, 0.0F), Vec3(0.0F, 1.13F, -1.40F), across, Material::Galvanised,
                     Part::Shape::Cylinder, 30.0F});
    for (const float x : {-0.22F, 0.22F}) {
        parts.push_back(box(Vec3(0.03F, 0.05F, 0.15F), Vec3(x, 0.99F, -1.40F), Material::Steel, 2.0F));
        parts.push_back(box(Vec3(0.015F, 0.012F, 0.17F), Vec3(x, 1.30F, -1.40F), Material::Rubble, 0.3F));
    }
    // The cowl in front of the driver, between the front wheels, and the
    // wheel on its column.
    parts.push_back(box(Vec3(0.34F, 0.32F, 0.10F), Vec3(0.0F, 0.64F, 0.55F), Material::Yellow, 80.0F));
    parts.push_back(box(Vec3(0.025F, 0.08F, 0.025F), Vec3(0.0F, 1.0F, 0.43F), Material::Steel, 3.0F));
    // The wheel: a rim of eight short bars round a hub, tipped toward the
    // driver.
    const Quat tip = Quat::sRotation(Vec3::sAxisX(), -0.6F);
    const Vec3 wheel_at(0.0F, 1.06F, 0.38F);
    parts.push_back({Vec3(0.035F, 0.02F, 0.0F), wheel_at, tip, Material::Steel, Part::Shape::Cylinder, 1.0F});
    for (int k = 0; k < 8; ++k) {
        const float angle = 0.25F * JPH_PI * static_cast<float>(k);
        const Quat around = tip * Quat::sRotation(Vec3::sAxisY(), angle);
        parts.push_back({Vec3(0.065F, 0.014F, 0.014F), wheel_at + around * Vec3(0.0F, 0.0F, 0.155F), around,
                         Material::Rubble, Part::Shape::Box, 0.5F});
    }
    // The overhead guard: four posts and its roof, 2.33 m up.
    for (const float x : {-0.45F, 0.45F}) {
        parts.push_back(box(Vec3(0.035F, 1.0F, 0.035F), Vec3(x, 1.30F, 0.70F), Material::Steel, 10.0F));
        parts.push_back(box(Vec3(0.035F, 1.0F, 0.035F), Vec3(x, 1.33F, -1.18F), Material::Steel, 10.0F));
    }
    // Its roof: two side rails and slats across, open between.
    for (const float x : {-0.47F, 0.47F}) {
        parts.push_back(box(Vec3(0.03F, 0.04F, 0.97F), Vec3(x, 2.36F, -0.24F), Material::Steel, 12.0F));
    }
    for (int k = 0; k < 7; ++k) {
        parts.push_back(box(Vec3(0.44F, 0.025F, 0.035F), Vec3(0.0F, 2.37F, 0.62F - 0.29F * static_cast<float>(k)),
                            Material::Steel, 5.0F));
    }
    // The mast: two uprights in front of the front wheels and their top tie.
    for (const float x : {-0.42F, 0.42F}) {
        parts.push_back(box(Vec3(0.05F, 1.30F, 0.06F), Vec3(x, 1.40F, 0.97F), Material::Steel, 270.0F));
    }
    parts.push_back(box(Vec3(0.47F, 0.05F, 0.06F), Vec3(0.0F, 2.65F, 0.97F), Material::Steel, 60.0F));
    // The lift ram's barrel, standing between the uprights (its rod, drawn
    // up to the carriage, is the presentation's).
    parts.push_back({Vec3(0.06F, 0.65F, 0.0F), Vec3(0.0F, 0.80F, 0.97F), Quat::sIdentity(), Material::Steel,
                     Part::Shape::Cylinder, 25.0F});
    return parts;
}

// The carriage and its forks, in its own frame: its origin at the blades'
// middle height, at the carriage's back plate.
std::vector<Part> carriage_parts() {
    std::vector<Part> parts;
    // Its frame and load backrest: a grille of bars, close enough that a drum
    // leaning back on it is held, open enough that the driver sees the forks.
    parts.push_back(box(Vec3(0.48F, 0.07F, 0.03F), Vec3(0.0F, 0.12F, 0.0F), Material::Steel, 26.0F));
    parts.push_back(box(Vec3(0.48F, 0.025F, 0.02F), Vec3(0.0F, 0.50F, 0.0F), Material::Steel, 4.0F));
    parts.push_back(box(Vec3(0.48F, 0.04F, 0.025F), Vec3(0.0F, 0.84F, 0.0F), Material::Steel, 12.0F));
    for (const float x : {-0.47F, -0.16F, 0.16F, 0.47F}) {
        parts.push_back(box(Vec3(0.02F, 0.40F, 0.02F), Vec3(x, 0.48F, 0.0F), Material::Steel, 2.0F));
    }
    for (const float x : {-0.30F, 0.30F}) {
        // The fork's shank up the frame, and its blade, 1.2 m long and 5 cm
        // thick: low enough to go under a pallet's deck between its stringers.
        parts.push_back(box(Vec3(0.05F, 0.42F, 0.02F), Vec3(x, 0.42F, 0.04F), Material::Steel, 15.0F));
        parts.push_back(box(Vec3(0.05F, 0.025F, 0.60F), Vec3(x, 0.0F, 0.66F), Material::Steel, 35.0F));
    }
    return parts;
}

float total(const std::vector<Part> &parts) {
    float mass = 0.0F;
    for (const Part &part : parts) {
        mass += part.mass_kg;
    }
    return mass;
}

// The carriage's origin on the truck with the forks lowest: the blades 3.5 cm
// off the ground (their tips a little more: an empty truck sits slightly
// nose up on its loaded rear wheels), the plate just in front of the
// uprights.
const Vec3 kCarriageAt(0.0F, 0.06F, 1.09F);
// The driver's place on the floor, behind the cowl.
const Vec3 kStation(0.0F, 0.32F, -0.15F);

struct WheelSpec {
    Vec3 at;          // the suspension's top, in the truck's frame
    float radius;
    float width;
    bool driven;
};
// Front (driven, left then right; +x is the truck's left) and rear
// (steered). The suspension, as stiff as a truck with none can be in the
// solver, holds the origin at the ground.
const WheelSpec kWheelSpecs[Forklift::kWheels] = {
    {Vec3(0.55F, 0.41F, 0.55F), 0.33F, 0.25F, true},
    {Vec3(-0.55F, 0.41F, 0.55F), 0.33F, 0.25F, true},
    {Vec3(0.48F, 0.34F, -0.90F), 0.26F, 0.20F, false},
    {Vec3(-0.48F, 0.34F, -0.90F), 0.26F, 0.20F, false},
};
constexpr float kSuspensionMin = 0.02F;
constexpr float kSuspensionMax = 0.08F;

// The wheels themselves are Jolt's vehicle wheels, which nothing but the
// ground meets. Inside each, where it stands at rest, the truck carries a
// solid hub a little smaller (6 cm off the ground, 2 cm narrower) so a body
// walked or shoved into a tyre stops at it.
void add_wheel_hubs(std::vector<Part> &parts) {
    const Quat axle = Quat::sRotation(Vec3::sAxisZ(), 0.5F * JPH_PI);
    for (const WheelSpec &spec : kWheelSpecs) {
        const Vec3 centre = spec.at - Vec3(0.0F, kSuspensionMax, 0.0F);
        parts.push_back({Vec3(spec.radius - 0.06F, 0.5F * spec.width - 0.01F, 0.0F), centre, axle, Material::Rubble,
                         Part::Shape::Cylinder, 10.0F});
    }
}

float signed_unit(const float value) {
    return std::isfinite(value) ? std::clamp(value, -1.0F, 1.0F) : 0.0F;
}

WheeledVehicleController &controller_of(VehicleConstraint &vehicle) {
    return *static_cast<WheeledVehicleController *>(vehicle.GetController());
}

// The steer axle's geometry (Ackermann's). Jolt's controller turns every
// steered wheel through the same angle; on a truck's steer axle the inner
// wheel turns further than the outer, so that both roll about one point on
// the line of the fixed front axle. Turned alike, on full lock under the
// counterweight's load, the two scrub against each other and stall the
// truck. The controller's angle is the one a wheel on the centreline would
// take; each wheel is turned to roll about the same point.
class SteerAxleController final : public WheeledVehicleController {
public:
    SteerAxleController(const WheeledVehicleControllerSettings &settings, VehicleConstraint &constraint)
        : WheeledVehicleController(settings, constraint) {}

protected:
    void PreCollide(const float delta_seconds, PhysicsSystem &physics) override {
        WheeledVehicleController::PreCollide(delta_seconds, physics);
        for (Wheel *wheel : mConstraint.GetWheels()) {
            const float centreline = wheel->GetSteerAngle();
            if (centreline == 0.0F) {
                continue;
            }
            // tan(wheel) = base tan(centreline) / (base + x tan(centreline)),
            // base the wheel's distance behind the fixed axle, x its offset.
            const Vec3 at = wheel->GetSettings()->mPosition;
            const float base = kWheelSpecs[0].at.GetZ() - at.GetZ();
            const float t = std::tan(centreline);
            wheel->SetSteerAngle(std::atan2(base * t, base + at.GetX() * t));
        }
    }
};

class SteerAxleControllerSettings final : public WheeledVehicleControllerSettings {
public:
    VehicleController *ConstructController(VehicleConstraint &constraint) const override {
        return new SteerAxleController(*this, constraint);
    }
};

} // namespace

Forklift::Forklift(PhysicsSystem &physics, kit::Kit &kit, const ObjectLayer moving_layer, const BodyID player,
                   const std::uint64_t chassis_entity, const std::uint64_t carriage_entity, const float park_x,
                   const float park_z, const float yaw, const float park_y)
    : physics_(physics), kit_(kit), chassis_entity_(chassis_entity) {
    const Quat facing = Quat::sRotation(Vec3::sAxisY(), yaw);
    const RVec3 park(park_x, park_y, park_z);
    std::vector<Part> chassis = chassis_parts();
    add_wheel_hubs(chassis);
    const std::vector<Part> carriage = carriage_parts();
    chassis_ = kit_.add_body(chassis_entity, chassis, park, facing, total(chassis), 0.6F);
    carriage_ = kit_.add_body(carriage_entity, carriage, park + facing * kCarriageAt, facing, total(carriage), 0.6F);
    // The lifting eye on the guard's roof: a crane's sling hooks on here, and
    // the truck hangs level under it.
    (void)kit_.add_anchor(chassis_, Vec3(0.0F, kEyeY, kEyeZ), 1.3F);
    // The carriage rides inside the uprights: the two pass through each other.
    kit_.disable_collision(chassis_, carriage_);

    BodyLockInterfaceNoLock const &locks = physics_.GetBodyLockInterfaceNoLock();
    Body &chassis_body = *locks.TryGetBody(kit_.body_id(chassis_));
    Body &carriage_body = *locks.TryGetBody(kit_.body_id(carriage_));

    VehicleConstraintSettings settings;
    settings.mMaxPitchRollAngle = JPH_PI;   // nothing keeps it upright but its wheels
    for (const WheelSpec &spec : kWheelSpecs) {
        auto *wheel = new WheelSettingsWV;
        wheel->mPosition = spec.at;
        wheel->mRadius = spec.radius;
        wheel->mWidth = spec.width;
        wheel->mSuspensionMinLength = kSuspensionMin;
        wheel->mSuspensionMaxLength = kSuspensionMax;
        wheel->mSuspensionSpring = SpringSettings(ESpringMode::FrequencyAndDamping, 3.0F, 0.8F);
        wheel->mMaxSteerAngle = spec.driven ? 0.0F : kSteerMaxRad;
        // Grip from no slip at all: Jolt's default curves start at none,
        // which let a parked truck wander sideways a few millimetres a second.
        wheel->mLongitudinalFriction.Clear();
        wheel->mLongitudinalFriction.AddPoint(0.0F, 1.0F);
        wheel->mLongitudinalFriction.AddPoint(0.06F, 1.2F);
        wheel->mLongitudinalFriction.AddPoint(0.2F, 1.0F);
        wheel->mLateralFriction.Clear();
        wheel->mLateralFriction.AddPoint(0.0F, 1.0F);
        wheel->mLateralFriction.AddPoint(3.0F, 1.2F);
        wheel->mLateralFriction.AddPoint(20.0F, 1.0F);
        wheel->mMaxBrakeTorque = 2000.0F;
        wheel->mMaxHandBrakeTorque = 0.0F;
        // A solid tyre on its hub: 30 kg (front) or 20 kg (rear).
        const float wheel_kg = spec.driven ? 30.0F : 20.0F;
        wheel->mInertia = 0.5F * wheel_kg * spec.radius * spec.radius;
        settings.mWheels.push_back(wheel);
    }
    auto *drive = new SteerAxleControllerSettings;
    // The propane engine through its torque converter, as one drive: full
    // torque from a standstill to 70 % of its speed, none at its top
    // (CHOSEN: 220 N m through 30 : 1 to the front axle, a top speed of about
    // 3.4 m/s, 12 km/h).
    drive->mEngine.mMaxTorque = 220.0F;
    drive->mEngine.mMinRPM = 0.0F;
    drive->mEngine.mMaxRPM = 3000.0F;
    drive->mEngine.mNormalizedTorque.Clear();
    drive->mEngine.mNormalizedTorque.AddPoint(0.0F, 1.0F);
    drive->mEngine.mNormalizedTorque.AddPoint(0.7F, 1.0F);
    drive->mEngine.mNormalizedTorque.AddPoint(1.0F, 0.0F);
    drive->mTransmission.mMode = ETransmissionMode::Manual;
    drive->mTransmission.mGearRatios = {1.0F};
    drive->mTransmission.mReverseGearRatios = {-1.0F};
    drive->mTransmission.mClutchStrength = 30.0F;
    VehicleDifferentialSettings axle;
    axle.mLeftWheel = 0;
    axle.mRightWheel = 1;
    axle.mDifferentialRatio = 30.0F;
    drive->mDifferentials.push_back(axle);
    settings.mController = drive;
    vehicle_ = new VehicleConstraint(chassis_body, settings);
    // The wheels roll on everything but the truck itself and the player.
    wheel_filter_.IgnoreBody(kit_.body_id(chassis_));
    wheel_filter_.IgnoreBody(kit_.body_id(carriage_));
    wheel_filter_.IgnoreBody(player);
    Ref<VehicleCollisionTesterCastCylinder> tester = new VehicleCollisionTesterCastCylinder(moving_layer);
    tester->SetBodyFilter(&wheel_filter_);
    vehicle_->SetVehicleCollisionTester(tester);
    physics_.AddConstraint(vehicle_);
    physics_.AddStepListener(vehicle_);

    // The forks' slide up the mast, a position motor of finite force.
    SliderConstraintSettings slide;
    slide.mAutoDetectPoint = true;
    slide.SetSliderAxis(Vec3::sAxisY());
    slide.mLimitsMin = 0.0F;
    slide.mLimitsMax = kLiftStrokeM;
    // Held, the ram is stiff: a 2 t load sags it about a centimetre.
    slide.mMotorSettings = MotorSettings(20.0F, 1.0F, kHoldForceN, 0.0F);
    slide.mMotorSettings.SetForceLimits(0.0F, kHoldForceN);
    lift_ = static_cast<SliderConstraint *>(slide.Create(chassis_body, carriage_body));
    physics_.AddConstraint(lift_);
    lift_->SetMotorState(EMotorState::Position);
    lift_->SetTargetPosition(0.0F);
    controller_of(*vehicle_).SetDriverInput(0.0F, 0.0F, 1.0F, 0.0F);
}

Forklift::~Forklift() {
    physics_.RemoveConstraint(lift_);
    physics_.RemoveStepListener(vehicle_);
    physics_.RemoveConstraint(vehicle_);
}

JPH::RVec3 Forklift::station() const {
    const BodyInterface &bodies = physics_.GetBodyInterfaceNoLock();
    const BodyID chassis = kit_.body_id(chassis_);
    return bodies.GetPosition(chassis) + bodies.GetRotation(chassis) * kStation;
}

JPH::RMat44 Forklift::wheel_transform(const int wheel) const {
    // A cylinder along its own y: y the axle, x up.
    return vehicle_->GetWheelWorldTransform(static_cast<uint>(wheel), Vec3::sAxisY(), Vec3::sAxisX());
}

float Forklift::wheel_radius(const int wheel) const noexcept {
    return kWheelSpecs[wheel].radius;
}

float Forklift::wheel_width(const int wheel) const noexcept {
    return kWheelSpecs[wheel].width;
}

void Forklift::pre_step(const float delta_seconds, const bool take, const bool at_controls, const float drive,
                        const float steer, const float lift) {
    if (take) {
        state_.driving = !state_.driving && at_controls;
    } else if (state_.driving && !at_controls) {
        // Off the floor (stepped, thrown or fallen): the controls are let go.
        state_.driving = false;
    }
    const float want_drive = state_.driving ? signed_unit(drive) : 0.0F;
    const float want_steer = state_.driving ? signed_unit(steer) : 0.0F;
    const float want_lift = state_.driving ? signed_unit(lift) : 0.0F;

    // The forks, as a single-acting hydraulic ram: it only ever pushes up.
    // RAISE held, the pump drives it at the lift's speed with all the force
    // its relief valve allows (no more: past it, it stalls). LOWER held, the
    // load's own weight lets the oil out through a valve that meters it to
    // the lift's speed, whatever the weight. Let go, the check valve shuts and
    // the ram holds where it stands, whatever the mast carries.
    const float at = lift_->GetCurrentPosition();
    if (want_lift > 0.05F) {
        lift_->GetMotorSettings().SetForceLimits(0.0F, kLiftForceN);
        lift_->SetMotorState(EMotorState::Velocity);
        lift_->SetTargetVelocity(want_lift * kLiftSpeedMps);
        lift_target_ = at;
    } else if (want_lift < -0.05F) {
        lift_->GetMotorSettings().SetForceLimits(0.0F, kHoldForceN);
        lift_->SetMotorState(EMotorState::Velocity);
        lift_->SetTargetVelocity(want_lift * kLiftSpeedMps);
        lift_target_ = at;
    } else {
        lift_->GetMotorSettings().SetForceLimits(0.0F, kHoldForceN);
        lift_->SetMotorState(EMotorState::Position);
        lift_->SetTargetPosition(lift_target_);
    }

    // The wheels. The steered wheels turn at their rate; they are the rear
    // ones, so turning right turns them left.
    const float steer_step = kSteerRateRadS / kSteerMaxRad * delta_seconds;
    state_.steer = std::clamp(want_steer, state_.steer - steer_step, state_.steer + steer_step);
    const BodyInterface &bodies = physics_.GetBodyInterfaceNoLock();
    const BodyID chassis = kit_.body_id(chassis_);
    const Vec3 forward = bodies.GetRotation(chassis) * Vec3::sAxisZ();
    const float speed = bodies.GetLinearVelocity(chassis).Dot(forward);
    int gear = 0;
    float throttle = 0.0F;
    float brake = 0.0F;
    if (!state_.driving) {
        brake = 1.0F;   // parked
    } else if (want_drive * speed < 0.0F && std::abs(speed) > 0.25F) {
        brake = std::abs(want_drive);   // the other way while moving: brake first
    } else if (std::abs(want_drive) > 0.05F) {
        // The pedal asks for a speed, as a hydrostatic drive's does: the
        // engine pushes toward it with all the torque it has, and short of
        // it under a load it is slower to get there; past it (downhill) the
        // drive holds back.
        gear = want_drive > 0.0F ? 1 : -1;
        const float short_of = std::abs(want_drive) * kTopSpeedMps - std::abs(speed);
        throttle = std::clamp(2.0F * short_of, 0.0F, 1.0F);
        brake = std::clamp(-0.5F * (short_of + 0.2F), 0.0F, 0.35F);
    } else {
        brake = 0.35F;   // stick let go: it slows to a stop
    }
    WheeledVehicleController &controller = controller_of(*vehicle_);
    controller.GetTransmission().Set(gear, gear != 0 ? 1.0F : 0.0F);
    controller.SetDriverInput(throttle, -state_.steer, brake, 0.0F);
    state_.drive = static_cast<float>(gear) * throttle;
    state_.speed_mps = speed;
    state_.lift_m = at;
    state_.lift = want_lift;
}

void Forklift::post_step(const float delta_seconds) {
    if (delta_seconds <= 0.0F) {
        return;
    }
    state_.lift_n = std::abs(lift_->GetTotalLambdaMotor()) / delta_seconds;
    for (int index = 0; index < kWheels; ++index) {
        const Wheel *wheel = vehicle_->GetWheel(static_cast<uint>(index));
        const auto i = static_cast<std::size_t>(index);
        state_.wheel_contact[i] = wheel->HasContact();
        state_.wheel_load_n[i] = wheel->HasContact() ? std::max(0.0F, wheel->GetSuspensionLambda()) / delta_seconds : 0.0F;
    }
    const BodyInterface &bodies = physics_.GetBodyInterfaceNoLock();
    const Vec3 forward = bodies.GetRotation(kit_.body_id(chassis_)) * Vec3::sAxisZ();
    state_.pitch_rad = -std::asin(std::clamp(forward.GetY(), -1.0F, 1.0F));
}

void Forklift::restore() {
    state_ = {};
    WheeledVehicleController &controller = controller_of(*vehicle_);
    for (Wheel *wheel : vehicle_->GetWheels()) {
        wheel->SetAngularVelocity(0.0F);
    }
    controller.GetEngine().SetCurrentRPM(0.0F);
    controller.GetTransmission().Set(0, 0.0F);
    controller.SetDriverInput(0.0F, 0.0F, 1.0F, 0.0F);
    lift_target_ = lift_->GetCurrentPosition();
    lift_->GetMotorSettings().SetForceLimits(0.0F, kHoldForceN);
    lift_->SetMotorState(EMotorState::Position);
    lift_->SetTargetPosition(lift_target_);
    state_.lift_m = lift_target_;
}

} // namespace scraperx::sim
