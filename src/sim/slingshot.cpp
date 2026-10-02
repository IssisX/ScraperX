#include "sim/slingshot.hpp"

#include <Jolt/Physics/Body/BodyLock.h>

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
    result.convex_radius = .01F;
    return result;
}

slingshot::Vec3 model(RVec3Arg value) {
    return {value.GetX(), value.GetY(), value.GetZ()};
}

RVec3 world(slingshot::Vec3 value) { return RVec3(value.x, value.y, value.z); }
Vec3 force(slingshot::Vec3 value) {
    return Vec3(float(value.x), float(value.y), float(value.z));
}
Vec3 rotation_vector(Quat value) {
    value = value.EnsureWPositive();
    const auto vector = value.GetXYZ();
    const double length = vector.Length();
    return length > 1e-12 ? vector * float(2 * std::atan2(length, double(value.GetW())) / length) : Vec3::sZero();
}
} // namespace

Slingshot::Slingshot(PhysicsSystem &system, kit::Kit &kit, BodyID player)
    : system_(system), kit_(kit), player_(player) {
    {
        BodyLockRead lock(system_.GetBodyLockInterface(), player_);
        saved_player_group_ = lock.GetBody().GetCollisionGroup();
    }
    // CHOSEN: a human-sized open leather pouch. All four load-bearing parts
    // are in the same kit shape that is presented; no duplicate proxy floor.
    const std::vector<kit::Part> pouch_parts{
        box(Vec3(.80F, .06F, .65F), Vec3(0, -.23F, 0), kit::Material::Timber, 6),
        box(Vec3(.04F, .25F, .65F), Vec3(-.84F, .15F, 0), kit::Material::Timber, 3),
        box(Vec3(.04F, .25F, .65F), Vec3(.84F, .15F, 0), kit::Material::Timber, 3),
        box(Vec3(.80F, .08F, .04F), Vec3(0, .34F, .69F), kit::Material::Timber, 3),
    };
    pouch_ = kit_.add_body(kPouchEntity, pouch_parts, neutral_position(),
                          Quat::sIdentity(), kPouchMassKg, .8F);
    // Kit reserves 2048 collision-table entries; this player uses the final
    // unused entry so only its pouch pair is excluded, not other kit bodies.
    kit_.disable_collision(kit::BodyIndex{2047}, pouch_);
    kit_.set_damping(pouch_, 0, 0);
    kit_.set_continuous_collision(pouch_);
    // The harness is a point joint. Keeping the pouch upright is a genuine
    // restricted angular DOF, not a repeated orientation/velocity assignment.
    {
        BodyLockWrite lock(system_.GetBodyLockInterface(), kit_.body_id(pouch_));
        auto *motion = lock.GetBody().GetMotionProperties();
        const auto mass = lock.GetBody().GetShape()->GetMassProperties();
        auto physical_mass = mass;
        physical_mass.ScaleToMass(kPouchMassKg);
        motion->SetMassProperties(EAllowedDOFs::TranslationX |
            EAllowedDOFs::TranslationY | EAllowedDOFs::TranslationZ, physical_mass);
    }

    // Reference-shaped, rooted Y fork. Capsule members are the same native
    // parts Godot draws: stout round timber, curved branches, an open throat.
    // Preserve the proven anchor positions; aim turns the paid guide only.
    const auto initial_rotation = Quat::sRotation(Vec3::sAxisX(), float(State{}.elevation_rad));
    const auto inverse = initial_rotation.Conjugated();
    const Vec3 tip = initial_rotation * Vec3(0, 0, -float(kForkRadiusM));
    std::vector<kit::Part> fork_parts;
    const auto timber = [&](Vec3 a, Vec3 b, float radius) {
        kit::Part part;
        const auto span = b - a;
        part.shape = kit::Part::Shape::Capsule;
        part.half = Vec3(radius, span.Length() * .5F, radius);
        part.offset = inverse * ((a + b) * .5F);
        part.rotation = inverse * Quat::sFromTo(Vec3::sAxisY(), span.Normalized());
        part.material = kit::Material::Timber;
        fork_parts.push_back(part);
    };
    const Vec3 root(0, .6F, -5.0F);
    const Vec3 neck(0, 4.8F, -4.3F);
    timber(root, neck, .90F);
    for (const float side : {-1.0F, 1.0F}) {
        const Vec3 control(side * float(kHalfSpanM), 4.9F, tip.GetZ());
        const Vec3 end(side * float(kHalfSpanM), tip.GetY(), tip.GetZ());
        Vec3 previous = neck;
        for (int step = 1; step <= 14; ++step) {
            const float t = float(step) / 14.0F;
            const auto next = neck * ((1-t)*(1-t)) + control * (2*t*(1-t)) + end * (t*t);
            timber(previous, next, .80F - .10F * t);
            previous = next;
        }
    }
    // Visible foundation bears the rooted handle. Its top stays below the
    // pouch and does not bridge the launch corridor or approach path.
    // Keep the Grade start / backward boarding approach at z=-58 clear of
    // the foundation, including the player's 0.35 m collision radius.
    auto foot = box(Vec3(2.8F, .20F, 1.1F), Vec3::sZero(), kit::Material::Steel);
    foot.offset = inverse * Vec3(0, -.15F, -5.0F);
    foot.rotation = inverse;
    fork_parts.push_back(foot);
    frame_ = kit_.add_body(kFrameEntity, fork_parts, neutral_position(),
                          initial_rotation, 0, .8F);
    kit_.disable_collision(frame_, pouch_);

    // Visible grade rail and tooth rack. The actual slider reacts the
    // vertical band load into this rail; its lower limit is the ratchet face.
    std::vector<kit::Part> rail_parts;
    for (const float x : {-1.20F, 1.20F})
        rail_parts.push_back(box(Vec3(.16F, .12F, 6.35F), Vec3(x, -.23F, 6),
                                 kit::Material::Steel));
    for (const float x : {-3.0F, 3.0F})
        rail_parts.push_back(box(Vec3(1.35F, .35F, 2.1F), Vec3(x, 0, .8F),
                                 kit::Material::Timber));
    for (int tooth = 0; tooth <= 120; ++tooth)
        rail_parts.push_back(box(Vec3(.21F, .08F, .025F),
            Vec3(1.25F, -.03F, .1F * tooth), kit::Material::Yellow));
    rail_parts.push_back(box(Vec3(.07F, .35F, .07F), Vec3(1.5F, 0, -1.8F), kit::Material::Yellow));
    rail_ = kit_.add_body(kRailEntity, rail_parts, neutral_position(),
                          Quat::sIdentity(), 0, .8F);
    kit_.disable_collision(rail_, pouch_);
    const std::vector<kit::Part> launch_parts{
        box(Vec3(.10F, .10F, 6), Vec3(-1.05F, 0, -6), kit::Material::Steel, 28),
        box(Vec3(.10F, .10F, 6), Vec3(1.05F, 0, -6), kit::Material::Steel, 28),
        // Pivot cheeks sit outside the rider aperture and stay above grade
        // throughout the real20–85 degree gimbal sweep.
        box(Vec3(.22F, .12F, .22F), Vec3(-1.05F, 0, 0), kit::Material::Yellow, 2),
        box(Vec3(.22F, .12F, .22F), Vec3(1.05F, 0, 0), kit::Material::Yellow, 2),
    };
    launch_rail_ = kit_.add_body(kLaunchRailEntity, launch_parts, neutral_position(),
        initial_rotation, kLaunchRailMassKg, .4F);
    kit_.set_damping(launch_rail_, 0, 0);
    kit_.disable_collision(launch_rail_, pouch_);
    kit_.disable_collision(launch_rail_, rail_);
    kit_.disable_collision(launch_rail_, frame_);
    system_.GetBodyInterface().SetMotionQuality(player_, EMotionQuality::LinearCast);
    refresh_geometry();
    create_carrier_guide();
    bind_carrier();
    create_guide(0);
    refresh_state();
    system_.AddStepListener(this);
}

Slingshot::~Slingshot() {
    system_.RemoveStepListener(this);
    detach();
    system_.GetBodyInterface().SetCollisionGroup(player_, saved_player_group_);
    remove_launch_guide();
    remove_aim_gimbal();
    unbind_carrier();
    if (carrier_guide_ != nullptr) system_.RemoveConstraint(carrier_guide_);
    remove_guide();
}

void Slingshot::refresh_geometry(bool no_lock) {
    const auto &bodies = no_lock ? system_.GetBodyInterfaceNoLock() : system_.GetBodyInterface();
    const auto frame_at = bodies.GetPosition(kit_.body_id(frame_));
    const auto frame_rotation = bodies.GetRotation(kit_.body_id(frame_));
    state_.anchor_left = frame_at + RVec3(frame_rotation * Vec3(-float(kHalfSpanM), 0, -float(kForkRadiusM)));
    state_.anchor_right = frame_at + RVec3(frame_rotation * Vec3(float(kHalfSpanM), 0, -float(kForkRadiusM)));
    anchors_.position = {model(state_.anchor_left), model(state_.anchor_right)};
    const auto axis = bodies.GetRotation(kit_.body_id(launch_rail_)) * Vec3(0, 0, -1);
    state_.yaw_rad = std::atan2(axis.GetX(), -axis.GetZ());
    state_.elevation_rad = std::asin(std::clamp(double(axis.GetY()), -1.0, 1.0));
    state_.aim_ready = aim_gimbal_ == nullptr;
}

void Slingshot::create_aim_gimbal() {
    if (aim_gimbal_ != nullptr) return;
    if (!audit_initialized_) {
        double kinetic = 0, gravity = 0;
        initial_mechanical_energy_j_ = body_energy(kit_.body_id(pouch_), kinetic, gravity) +
            body_energy(kit_.body_id(launch_rail_), kinetic, gravity) + bands().elastic_energy_j;
        audit_rider_ = harness_ != nullptr;
        if (audit_rider_) initial_mechanical_energy_j_ += body_energy(player_, kinetic, gravity);
        audit_initialized_ = true;
    }
    unbind_carrier();
    if (carrier_guide_ != nullptr) {
        system_.RemoveConstraint(carrier_guide_);
        carrier_guide_ = nullptr;
    }
    auto *carrier = system_.GetBodyLockInterfaceNoLock().TryGetBody(kit_.body_id(launch_rail_));
    SixDOFConstraintSettings settings;
    settings.mPosition1 = settings.mPosition2 = carrier->GetPosition();
    settings.mAxisX2 = carrier->GetRotation() * Vec3::sAxisX();
    settings.mAxisY2 = carrier->GetRotation() * Vec3::sAxisY();
    settings.MakeFixedAxis(SixDOFConstraintSettings::TranslationX);
    settings.MakeFixedAxis(SixDOFConstraintSettings::TranslationY);
    settings.MakeFixedAxis(SixDOFConstraintSettings::TranslationZ);
    settings.SetLimitedAxis(SixDOFConstraintSettings::RotationX, .33F, 1.50F);
    settings.SetLimitedAxis(SixDOFConstraintSettings::RotationY, -.82F, .82F);
    settings.MakeFixedAxis(SixDOFConstraintSettings::RotationZ);
    settings.mSwingType = ESwingType::Pyramid;
    settings.mNumVelocityStepsOverride = 40;
    settings.mNumPositionStepsOverride = 8;
    aim_gimbal_ = static_cast<SixDOFConstraint *>(settings.Create(Body::sFixedToWorld, *carrier));
    system_.AddConstraint(aim_gimbal_);
    state_.aim_ready = false;
}

void Slingshot::remove_aim_gimbal() {
    if (aim_gimbal_ != nullptr) system_.RemoveConstraint(aim_gimbal_);
    aim_gimbal_ = nullptr;
}

void Slingshot::aim(double yaw, double elevation, float) {
    if (!std::isfinite(yaw) || !std::isfinite(elevation)) return;
    state_.target_yaw_rad = std::clamp(yaw, -.8, .8);
    state_.target_elevation_rad = std::clamp(elevation, .35, 1.48);
    const auto current = kit_.body_rotation(launch_rail_);
    const auto target = Quat::sRotation(Vec3::sAxisY(), -float(state_.target_yaw_rad)) *
                        Quat::sRotation(Vec3::sAxisX(), float(state_.target_elevation_rad));
    const float angle = rotation_vector(target * current.Conjugated()).Length();
    const double angular_speed = system_.GetBodyInterface().GetAngularVelocity(kit_.body_id(launch_rail_)).Length();
    if (aim_gimbal_ != nullptr && angle < .003F && angular_speed < .025) {
        remove_aim_gimbal();
        create_carrier_guide();
        bind_carrier();
    } else if (angle > .006F) create_aim_gimbal();
    refresh_geometry();
}

Vec3 Slingshot::aim_torque(float dt, bool no_lock) const {
    if (aim_gimbal_ == nullptr) return Vec3::sZero();
    const auto &bodies = no_lock ? system_.GetBodyInterfaceNoLock() : system_.GetBodyInterface();
    const auto id = kit_.body_id(launch_rail_);
    const auto rotation = bodies.GetRotation(id);
    const auto target = Quat::sRotation(Vec3::sAxisY(), -float(state_.target_yaw_rad)) *
                        Quat::sRotation(Vec3::sAxisX(), float(state_.target_elevation_rad));
    const auto error = rotation_vector(target * rotation.Conjugated());
    const auto angular_velocity = bodies.GetAngularVelocity(id);
    const auto desired_velocity = error.NormalizedOr(Vec3::sZero()) * std::min(error.Length() * 4.0F, .70F);
    const auto lever = Vec3(bodies.GetCenterOfMassPosition(id) - bodies.GetPosition(id));
    auto torque = (desired_velocity - angular_velocity) * 10000.0F -
                  lever.Cross(system_.GetGravity() * kLaunchRailMassKg);
    constexpr float maximum_torque = 20000.0F;
    if (torque.Length() > maximum_torque) torque *= maximum_torque / torque.Length();
    const auto *body = system_.GetBodyLockInterfaceNoLock().TryGetBody(id);
    const double predicted_power = torque.Dot(angular_velocity) + .5 * dt *
        torque.Dot(body->GetInverseInertia().Multiply3x3(torque));
    const double power_limit = kPlayerPowerW * kTransmissionEfficiency;
    if (predicted_power > power_limit) torque *= float(power_limit / predicted_power);
    return torque;
}

void Slingshot::create_carrier_guide() {
    if (carrier_guide_ != nullptr) return;
    auto *carrier = system_.GetBodyLockInterfaceNoLock().TryGetBody(kit_.body_id(launch_rail_));
    SliderConstraintSettings settings;
    const auto origin = carrier->GetPosition();
    settings.mPoint1 = RVec3(origin.GetX(), origin.GetY(), state_.carrier_origin.GetZ()) +
                      carrier->GetCenterOfMassPosition() - carrier->GetPosition();
    settings.mPoint2 = carrier->GetCenterOfMassPosition();
    settings.SetSliderAxis(Vec3::sAxisZ());
    settings.mLimitsMin = 0;
    settings.mLimitsMax = float(kMaximumDrawM);
    settings.mNumVelocityStepsOverride = 40;
    settings.mNumPositionStepsOverride = 8;
    carrier_guide_ = static_cast<SliderConstraint *>(settings.Create(Body::sFixedToWorld, *carrier));
    system_.AddConstraint(carrier_guide_);
}

void Slingshot::bind_carrier() {
    if (carrier_bind_ != nullptr) return;
    auto *carrier = system_.GetBodyLockInterfaceNoLock().TryGetBody(kit_.body_id(launch_rail_));
    auto *pouch = system_.GetBodyLockInterfaceNoLock().TryGetBody(kit_.body_id(pouch_));
    FixedConstraintSettings settings;
    settings.mAutoDetectPoint = true;
    settings.mNumVelocityStepsOverride = 40;
    settings.mNumPositionStepsOverride = 8;
    carrier_bind_ = static_cast<FixedConstraint *>(settings.Create(*carrier, *pouch));
    system_.AddConstraint(carrier_bind_);
}

void Slingshot::unbind_carrier() {
    if (carrier_bind_ == nullptr) return;
    system_.RemoveConstraint(carrier_bind_);
    carrier_bind_ = nullptr;
}

void Slingshot::create_launch_guide(bool restoring) {
    if (launch_guide_ != nullptr) return;
    auto *carrier = system_.GetBodyLockInterfaceNoLock().TryGetBody(kit_.body_id(launch_rail_));
    auto *pouch = system_.GetBodyLockInterfaceNoLock().TryGetBody(kit_.body_id(pouch_));
    SliderConstraintSettings settings;
    if (!restoring) state_.launch_track_start = pouch->GetPosition();
    settings.mPoint1 = state_.launch_track_start +
        pouch->GetCenterOfMassPosition() - pouch->GetPosition();
    settings.mPoint2 = pouch->GetCenterOfMassPosition();
    settings.SetSliderAxis(force(slingshot::aim_axis(state_.yaw_rad, state_.elevation_rad)));
    settings.mLimitsMin = 0;
    settings.mLimitsMax = 10000; // The geometric open end must not act as a stop.
    settings.mNumVelocityStepsOverride = 40;
    settings.mNumPositionStepsOverride = 8;
    launch_guide_ = static_cast<SliderConstraint *>(settings.Create(*carrier, *pouch));
    system_.AddConstraint(launch_guide_);
    state_.guided_launch = true;
    state_.track_exit = false;
    state_.launch_track_end = state_.launch_track_start +
        RVec3(force(slingshot::aim_axis(state_.yaw_rad, state_.elevation_rad))) * kLaunchTrackLengthM;
}

void Slingshot::remove_launch_guide() {
    if (launch_guide_ != nullptr) system_.RemoveConstraint(launch_guide_);
    launch_guide_ = nullptr;
    state_.guided_launch = false;
}

void Slingshot::create_guide(double held_draw) {
    if (guide_ != nullptr) return;
    auto *pouch = system_.GetBodyLockInterfaceNoLock().TryGetBody(kit_.body_id(pouch_));
    if (pouch == nullptr) return;
    SliderConstraintSettings settings;
    settings.mSpace = EConstraintSpace::WorldSpace;
    // The reference is the undrawn centre of mass, retaining the shape's
    // true COM offset rather than treating the visible floor as its COM.
    const auto com_offset = pouch->GetCenterOfMassPosition() - pouch->GetPosition();
    settings.mPoint1 = state_.guide_origin + com_offset;
    settings.mPoint2 = pouch->GetCenterOfMassPosition();
    settings.mSliderAxis1 = settings.mSliderAxis2 = Vec3::sAxisZ();
    settings.mNormalAxis1 = settings.mNormalAxis2 = Vec3::sAxisY();
    settings.mLimitsMin = float(std::clamp(held_draw, 0.0, kMaximumDrawM));
    settings.mLimitsMax = held_draw > .03 ? float(kMaximumDrawM) : settings.mLimitsMin;
    settings.mNumVelocityStepsOverride = 40;
    settings.mNumPositionStepsOverride = 8;
    settings.mMotorSettings.SetForceLimits(0, 0);
    guide_ = static_cast<SliderConstraint *>(settings.Create(Body::sFixedToWorld, *pouch));
    system_.AddConstraint(guide_);
    state_.guide_latched = true;
}

void Slingshot::remove_guide() {
    if (guide_ == nullptr) return;
    system_.RemoveConstraint(guide_);
    guide_ = nullptr;
    state_.guide_latched = false;
}

bool Slingshot::player_in_pouch() const {
    const auto &bodies = system_.GetBodyInterface();
    const auto pouch = bodies.GetPosition(kit_.body_id(pouch_));
    const auto player = bodies.GetPosition(player_);
    const auto relative = player - pouch;
    // Aperture dimensions subtract the real 0.35 m capsule radius. The soles
    // must be at the physical base top; walking nearby cannot seat the rider.
    constexpr double player_half_height = .90;
    constexpr double base_top_local = -.17;
    const double soles_local_y = relative.GetY() - player_half_height;
    return std::abs(relative.GetX()) <= .45 &&
           relative.GetZ() >= -.31 && relative.GetZ() <= .32 &&
           soles_local_y >= base_top_local - .05 &&
           soles_local_y <= base_top_local + .12;
}

bool Slingshot::player_at_control() const {
    const auto player = system_.GetBodyInterface().GetPosition(player_);
    const auto offset = player - retrieval_control_position();
    const double ground_y = neutral_position().GetY() - .35;
    const double soles_y = player.GetY() - .9;
    return double(offset.GetX()) * offset.GetX() + double(offset.GetZ()) * offset.GetZ() <= .75 * .75 &&
           soles_y >= ground_y - .08 && soles_y <= ground_y + .55;
}

void Slingshot::attach(bool checkpoint) {
    if (harness_ != nullptr || (!checkpoint && guide_ == nullptr) || !player_in_pouch()) return;
    auto *pouch = system_.GetBodyLockInterfaceNoLock().TryGetBody(kit_.body_id(pouch_));
    auto *player = system_.GetBodyLockInterfaceNoLock().TryGetBody(player_);
    if (pouch == nullptr || player == nullptr) return;
    PointConstraintSettings settings;
    settings.mSpace = EConstraintSpace::WorldSpace;
    settings.mPoint1 = settings.mPoint2 = player->GetCenterOfMassPosition();
    settings.mNumVelocityStepsOverride = 40;
    settings.mNumPositionStepsOverride = 8;
    harness_ = static_cast<PointConstraint *>(settings.Create(*pouch, *player));
    system_.AddConstraint(harness_);
    // This dedicated subgroup disables only the joined pouch contact;
    // every other kit/world collision remains available.
    auto harness_group = pouch->GetCollisionGroup();
    harness_group.SetSubGroupID(2047);
    system_.GetBodyInterface().SetCollisionGroup(player_, harness_group);
    state_.seated = true;
    if (audit_initialized_ && !audit_rider_) {
        double kinetic = 0, gravity = 0;
        initial_mechanical_energy_j_ += body_energy(player_, kinetic, gravity);
    }
    audit_rider_ = true;
    if (!audit_initialized_) {
        double kinetic = 0, gravity = 0;
        initial_mechanical_energy_j_ = body_energy(kit_.body_id(pouch_), kinetic, gravity) +
                                       body_energy(player_, kinetic, gravity) +
                                       body_energy(kit_.body_id(launch_rail_), kinetic, gravity) + bands().elastic_energy_j;
        audit_initialized_ = true;
    }
}

void Slingshot::detach() {
    if (!state_.released && audit_initialized_ && audit_rider_) {
        double kinetic = 0, gravity = 0;
        initial_mechanical_energy_j_ -= body_energy(player_, kinetic, gravity);
        audit_rider_ = false;
    }
    if (harness_ != nullptr) {
        system_.RemoveConstraint(harness_);
        harness_ = nullptr;
        if (state_.released) collision_exclusion_active_ = true;
        else system_.GetBodyInterface().SetCollisionGroup(player_, saved_player_group_);
    }
    state_.seated = false;
    state_.drawing = false;
    if (aim_gimbal_ != nullptr) {
        remove_aim_gimbal();
        create_carrier_guide();
        if (guide_ != nullptr) bind_carrier();
    }
}

void Slingshot::cancel_player_interaction() {
    detach();
    system_.GetBodyInterface().SetCollisionGroup(player_, saved_player_group_);
    collision_exclusion_active_ = false;
    state_.recovering = state_.drawing = false;
    draw_effort_ = 0;
    if (guide_ != nullptr) guide_->SetMotorState(EMotorState::Off);
    if (carrier_guide_ != nullptr) carrier_guide_->SetMotorState(EMotorState::Off);
    state_.source_power_w = state_.retrieval_source_power_w = 0;
    if (audit_initialized_) state_.ledger_valid = false;
    refresh_state();
}

slingshot::BandEvaluation Slingshot::bands(bool no_lock) const {
    const auto &bodies = no_lock ? system_.GetBodyInterfaceNoLock() : system_.GetBodyInterface();
    const auto point = bodies.GetPosition(kit_.body_id(pouch_));
    return slingshot::evaluate_bands(model(point),
        model(RVec3(bodies.GetPointVelocity(kit_.body_id(pouch_), point))),
        anchors_, band_parameters_);
}

double Slingshot::body_energy(BodyID id, double &kinetic, double &gravity) const {
    BodyLockRead lock(system_.GetBodyLockInterface(), id);
    if (!lock.Succeeded()) return 0;
    const auto &body = lock.GetBody();
    const auto *motion = body.GetMotionProperties();
    if (motion == nullptr || motion->GetInverseMass() <= 0) return 0;
    const double mass = 1.0 / motion->GetInverseMass();
    const double translation = .5 * mass * body.GetLinearVelocity().LengthSq();
    const auto spin = (body.GetRotation() * motion->GetInertiaRotation()).Conjugated() *
                      body.GetAngularVelocity();
    const auto inverse = motion->GetInverseInertiaDiagonal();
    double rotation = 0;
    for (int i = 0; i < 3; ++i)
        if (inverse[i] > 0) rotation += .5 * spin[i] * spin[i] / inverse[i];
    const double potential = -mass * body.GetCenterOfMassPosition().GetY() *
                             system_.GetGravity().GetY();
    kinetic += translation + rotation;
    gravity += potential;
    return translation + rotation + potential;
}

void Slingshot::refresh_state() {
    refresh_geometry();
    state_.pouch_position = kit_.body_position(pouch_);
    state_.draw_m = std::clamp(double(state_.pouch_position.GetZ() - state_.guide_origin.GetZ()),
                              0.0, kMaximumDrawM);
    state_.energy_j = bands().elastic_energy_j;
    // The fixed fork's anchors do not move when the rail aims. The grade
    // ratchet holds the actual pouch coordinate while a finite-work gimbal
    // turns only the 60 kg rail. Loaded rubber therefore needs no aim lock.
    state_.aim_locked = guide_ == nullptr || state_.released || state_.recovering;
    if (!state_.guided_launch && !state_.released)
        state_.launch_track_start = kit_.body_position(launch_rail_);
    state_.launch_track_end = state_.launch_track_start +
        RVec3(force(slingshot::aim_axis(state_.yaw_rad, state_.elevation_rad))) * kLaunchTrackLengthM;
    state_.seated = harness_ != nullptr;
    state_.pouch_pair_excluded = harness_ != nullptr || collision_exclusion_active_;
    state_.guide_latched = guide_ != nullptr;
    state_.can_retrieve = guide_ == nullptr && harness_ == nullptr && state_.released && player_at_control();
    state_.station_available = (guide_ != nullptr && harness_ == nullptr && player_in_pouch()) ||
                              state_.can_retrieve;
    double loaded_mass = kPouchMassKg;
    {
        BodyLockRead player_lock(system_.GetBodyLockInterface(), player_);
        if (player_lock.Succeeded()) loaded_mass += 1.0 / player_lock.GetBody().GetMotionProperties()->GetInverseMass();
    }
    const auto axis = slingshot::aim_axis(state_.yaw_rad, state_.elevation_rad);
    const auto end_band = slingshot::evaluate_bands(model(state_.pouch_position) + axis * kLaunchTrackLengthM,
                                                   {}, anchors_, band_parameters_);
    state_.release_ready = state_.aim_ready && guide_ != nullptr && state_.energy_j - end_band.elastic_energy_j >
        loaded_mass * -system_.GetGravity().GetY() * axis.y * kLaunchTrackLengthM + 1000.0;
    state_.kinetic_j = 0;
    state_.gravity_j = 0;
    double mechanical = body_energy(kit_.body_id(pouch_), state_.kinetic_j, state_.gravity_j);
    mechanical += body_energy(kit_.body_id(launch_rail_), state_.kinetic_j, state_.gravity_j);
    if (audit_rider_) mechanical += body_energy(player_, state_.kinetic_j, state_.gravity_j);
    if (audit_initialized_)
        state_.energy_residual_j = mechanical + state_.energy_j - initial_mechanical_energy_j_ -
            state_.work_j + state_.transmission_loss_j + state_.dissipated_work_j;
}

void Slingshot::finish_previous_step(bool no_lock) {
    if (!step_pending_) return;
    const auto &bodies = no_lock ? system_.GetBodyInterfaceNoLock() : system_.GetBodyInterface();
    const double current_draw = bodies.GetPosition(kit_.body_id(pouch_)).GetZ() - neutral_position().GetZ();
    double source_work = 0;
    if (guide_ != nullptr && previous_dt_ > 0) {
        const double force_n = guide_->GetTotalLambdaMotor() / previous_dt_;
        const double mechanical_work = std::max(0.0, force_n * (current_draw - previous_draw_));
        source_work = mechanical_work / kTransmissionEfficiency;
        state_.work_j += source_work;
        state_.source_work_j = state_.work_j;
        state_.transmission_loss_j += source_work - mechanical_work;
    }
    state_.source_power_w = previous_dt_ > 0 ? source_work / previous_dt_ : 0;
    const auto turn = rotation_vector(bodies.GetRotation(kit_.body_id(launch_rail_)) * previous_rail_rotation_.Conjugated());
    const double aim_mechanical_work = previous_aim_torque_.Dot(turn);
    const double aim_source_work = std::max(0.0, aim_mechanical_work) / kTransmissionEfficiency;
    state_.aim_control_work_j += aim_mechanical_work;
    state_.aim_source_power_w = previous_dt_ > 0 ? aim_source_work / previous_dt_ : 0;
    state_.work_j += aim_source_work;
    state_.source_work_j = state_.work_j;
    state_.source_power_w += state_.aim_source_power_w;
    state_.transmission_loss_j += aim_source_work - std::max(0.0, aim_mechanical_work);
    state_.dissipated_work_j += std::max(0.0, -aim_mechanical_work);
    const auto position = bodies.GetPosition(kit_.body_id(pouch_));
    const double retrieval_mechanical_work = previous_recovery_force_.Dot(Vec3(position - previous_position_));
    const double retrieval_source_work = std::max(0.0, retrieval_mechanical_work) / kTransmissionEfficiency;
    state_.retrieval_work_j += retrieval_source_work;
    state_.retrieval_source_power_w = previous_dt_ > 0 ? retrieval_source_work / previous_dt_ : 0;
    state_.work_j += retrieval_source_work;
    state_.source_work_j = state_.work_j;
    state_.transmission_loss_j += retrieval_source_work - std::max(0.0, retrieval_mechanical_work);
    const double braking_work = std::max(0.0, -retrieval_mechanical_work);
    state_.retrieval_braking_work_j += braking_work;
    state_.dissipated_work_j += braking_work;
    if (state_.recovering && carrier_guide_ != nullptr && previous_dt_ > 0) {
        const double carrier_z = bodies.GetPosition(kit_.body_id(launch_rail_)).GetZ();
        const double mechanical_work = carrier_guide_->GetTotalLambdaMotor() / previous_dt_ *
                                       (carrier_z - previous_carrier_z_);
        const double source = std::max(0.0, mechanical_work) / kTransmissionEfficiency;
        state_.retrieval_work_j += source;
        state_.retrieval_source_power_w += source / previous_dt_;
        state_.work_j += source;
        state_.source_work_j = state_.work_j;
        state_.transmission_loss_j += source - std::max(0.0, mechanical_work);
        state_.retrieval_braking_work_j += std::max(0.0, -mechanical_work);
        state_.dissipated_work_j += std::max(0.0, -mechanical_work);
    }
    state_.dissipated_work_j += previous_band_loss_w_ * previous_dt_;
    step_pending_ = false;
}

void Slingshot::pre_step(float dt, double draw_input, double yaw,
                        double elevation, bool action, bool release) {
    finish_previous_step();
    refresh_state();
    if (!(dt > 0) || !std::isfinite(dt)) return;
    if (collision_exclusion_active_) {
        BodyLockRead player_lock(system_.GetBodyLockInterface(), player_);
        BodyLockRead pouch_lock(system_.GetBodyLockInterface(), kit_.body_id(pouch_));
        const auto &a = player_lock.GetBody().GetWorldSpaceBounds();
        const auto &b = pouch_lock.GetBody().GetWorldSpaceBounds();
        bool separated = false;
        for (int axis = 0; axis < 3; ++axis)
            separated = separated || a.mMin[axis] - b.mMax[axis] > .10F ||
                                     b.mMin[axis] - a.mMax[axis] > .10F;
        if (separated) {
            // Unlock before changing the body's collision group.
            player_lock.ReleaseLock();
            pouch_lock.ReleaseLock();
            system_.GetBodyInterface().SetCollisionGroup(player_, saved_player_group_);
            collision_exclusion_active_ = false;
        }
    }
    if (release) {
        detach();
        state_.recovering = false;
    }
    if (state_.track_exit) {
        remove_launch_guide();
        detach();
        state_.track_exit = false;
    }
    // Aim cannot borrow work from charged bands or move an attached rider.
    // Unbinding the rail leaves the grade restraint and harness in place;
    // the actual finite torque/work owner operates even with a held draw.
    if (!state_.aim_locked && (harness_ != nullptr || player_in_pouch()))
        aim(yaw, elevation, dt);

    if (action && !release) {
        if (state_.can_retrieve && !state_.recovering) begin_recovery();
        else if (harness_ == nullptr && !state_.recovering) attach();
        else if (guide_ != nullptr && state_.release_ready && state_.aim_ready) {
            unbind_carrier();
            const float held = carrier_guide_->GetCurrentPosition();
            carrier_guide_->SetMotorState(EMotorState::Off);
            carrier_guide_->SetLimits(held, held);
            remove_guide();
            create_launch_guide();
            state_.released = true;
            state_.drawing = false;
            ++state_.launch_count;
        }
    }
    const auto evaluation = bands();
    if (state_.released && !state_.guided_launch && harness_ != nullptr &&
        evaluation.bands[0].extension_m <= .005 &&
        evaluation.bands[1].extension_m <= .005)
        detach(); // Actual slack geometry; the rider retains Jolt momentum.

    draw_effort_ = (harness_ != nullptr || (state_.recovering && player_at_control())) && std::isfinite(draw_input)
        ? std::clamp(draw_input, 0.0, 1.0) : 0.0;
    if (state_.recovering && draw_effort_ > 0) {
        const auto position = kit_.body_position(pouch_);
        if ((position - neutral_position()).Length() <= .03F &&
            kit_.body_velocity(pouch_).Length() < .1F && evaluation.elastic_energy_j < 1.0 &&
            (kit_.body_position(launch_rail_) - neutral_position()).Length() <= .03F &&
            kit_.body_velocity(launch_rail_).Length() < .1F)
            finish_recovery();
    }
    state_.drawing = guide_ != nullptr && state_.aim_ready && draw_effort_ > 0 &&
                     state_.draw_m < kMaximumDrawM - .0001;
    refresh_state();
}

void Slingshot::drive_ratchet(const slingshot::BandEvaluation &evaluation, double actual_speed) {
    state_.drawing = false;
    if (guide_ != nullptr) {
        // This ideal unilateral catch can resist forward recoil, but cannot
        // push the pouch backwards. Every new held face is its actual pose.
        state_.ratchet_draw_m = std::max(state_.ratchet_draw_m, state_.draw_m);
        const double effort = harness_ != nullptr && state_.aim_ready ? draw_effort_ : 0.0;
        // BOARD fastens the rider but does not open the seat restraint.
        // Only deliberate draw effort opens its travel; once charged, the
        // existing one-way catch retains the physically reached coordinate.
        const float upper_limit = effort > 0 || state_.ratchet_draw_m > .03
            ? float(kMaximumDrawM) : float(state_.ratchet_draw_m);
        guide_->SetLimits(float(state_.ratchet_draw_m), upper_limit);
        if (effort > 0 && state_.draw_m < kMaximumDrawM - .0001) {
            const double available_power = kPlayerPowerW * kTransmissionEfficiency * effort;
            const double resistance = std::max(0.0, -evaluation.force_n.z);
            const double target_speed = std::min(kMaximumDrawSpeedMps * effort,
                available_power / std::max(1000.0, resistance + 1000.0));
            const double force_limit = std::min(kMaximumPouchForceN * effort,
                available_power / std::max({target_speed, std::max(0.0, actual_speed), .10}));
            guide_->GetMotorSettings().SetForceLimits(0, float(force_limit));
            guide_->SetMotorState(EMotorState::Velocity);
            guide_->SetTargetVelocity(float(target_speed));
            state_.drawing = true;
        } else {
            guide_->SetMotorState(EMotorState::Off);
            guide_->GetMotorSettings().SetForceLimits(0, 0);
        }
    }
}

void Slingshot::OnStep(const PhysicsStepListenerContext &context) {
    // Jolt invokes this before each collision substep while body mutexes are
    // held. Its no-lock interface is the documented listener seam. Refreshing
    // the real force here improves spring convergence without a second body
    // integrator or any launch pose/velocity assignment.
    finish_previous_step(true);
    auto &bodies = system_.GetBodyInterfaceNoLock();
    refresh_geometry(true);
    const auto point = bodies.GetPosition(kit_.body_id(pouch_));
    if (launch_guide_ != nullptr && !state_.track_exit &&
        launch_guide_->GetCurrentPosition() >= kLaunchTrackLengthM) {
        launch_guide_->SetEnabled(false);
        if (harness_ != nullptr) harness_->SetEnabled(false);
        collision_exclusion_active_ = true;
        state_.track_exit = true;
    }
    state_.draw_m = std::clamp(double(point.GetZ() - state_.guide_origin.GetZ()),
                              0.0, kMaximumDrawM);
    const auto evaluation = bands(true);
    drive_ratchet(evaluation, bodies.GetLinearVelocity(kit_.body_id(pouch_)).GetZ());
    if (state_.recovering && carrier_guide_ != nullptr) {
        const double q = carrier_guide_->GetCurrentPosition();
        carrier_guide_->SetLimits(0, float(kMaximumDrawM));
        const double speed = bodies.GetLinearVelocity(kit_.body_id(launch_rail_)).GetZ();
        if (draw_effort_ > 0 && q > .0001) {
            const double target = -std::min(3.0, q * 4.0) * draw_effort_;
            const double cap = std::min(kRetrievalForceN * .5 * kTransmissionEfficiency * draw_effort_,
                kRetrievalPowerW * .5 * kTransmissionEfficiency * draw_effort_ /
                std::max({std::abs(target), std::abs(speed), .1}));
            carrier_guide_->GetMotorSettings().SetForceLimits(-float(cap), float(cap));
            carrier_guide_->SetTargetVelocity(float(target));
            carrier_guide_->SetMotorState(EMotorState::Velocity);
        } else carrier_guide_->SetMotorState(EMotorState::Off);
    }
    if (context.mIsFirstStep) {
        applied_band_force_ = Vec3::sZero();
        applied_recovery_force_ = Vec3::sZero();
        applied_aim_torque_ = Vec3::sZero();
    }
    const auto current_force = force(evaluation.force_n);
    // Applied forces persist between Jolt collision substeps and reset only
    // at the end of Update. Replace our contribution by adding its delta.
    bodies.AddForce(kit_.body_id(pouch_), current_force - applied_band_force_);
    applied_band_force_ = current_force;
    const auto retrieve_force = recovery_force(evaluation, point,
        bodies.GetLinearVelocity(kit_.body_id(pouch_)), context.mDeltaTime);
    bodies.AddForce(kit_.body_id(pouch_), retrieve_force - applied_recovery_force_);
    applied_recovery_force_ = retrieve_force;
    const auto torque = aim_torque(context.mDeltaTime, true);
    bodies.AddTorque(kit_.body_id(launch_rail_), torque - applied_aim_torque_);
    applied_aim_torque_ = torque;
    previous_aim_torque_ = torque;
    previous_rail_rotation_ = bodies.GetRotation(kit_.body_id(launch_rail_));
    previous_recovery_force_ = retrieve_force;
    previous_position_ = point;
    previous_carrier_z_ = bodies.GetPosition(kit_.body_id(launch_rail_)).GetZ();
    previous_draw_ = point.GetZ() - neutral_position().GetZ();
    previous_band_loss_w_ = evaluation.dissipated_power_w;
    previous_dt_ = context.mDeltaTime;
    step_pending_ = true;
}

void Slingshot::begin_recovery() {
    remove_launch_guide();
    state_.track_exit = false;
    state_.recovering = true;
    state_.work_j = state_.source_work_j = state_.transmission_loss_j = 0;
    state_.dissipated_work_j = state_.retrieval_work_j = state_.retrieval_braking_work_j = 0;
    state_.ledger_valid = true;
    contact_observed_.store(false, std::memory_order_relaxed);
    audit_rider_ = false; // The departing rider is outside this recovery ledger.
    double kinetic = 0, gravity = 0;
    initial_mechanical_energy_j_ = body_energy(kit_.body_id(pouch_), kinetic, gravity) + bands().elastic_energy_j;
    initial_mechanical_energy_j_ += body_energy(kit_.body_id(launch_rail_), kinetic, gravity);
    audit_initialized_ = true;
}

Vec3 Slingshot::recovery_force(const slingshot::BandEvaluation &evaluation,
                              RVec3 position, Vec3 velocity, float dt) const {
    if (!state_.recovering || draw_effort_ <= 0 || guide_ != nullptr) return Vec3::sZero();
    const auto displacement = Vec3(neutral_position() - position);
    const double distance = displacement.Length();
    if (distance < .00001) return Vec3::sZero();
    const auto direction = displacement / float(distance);
    const double toward_speed = velocity.Dot(direction);
    const double desired_speed = std::min(3.0, distance * 4.0) * draw_effort_;
    const auto environment = force(evaluation.force_n) + system_.GetGravity() * kPouchMassKg;
    const double tangential_speed_sq = std::max(0.0, double(velocity.LengthSq()) - toward_speed * toward_speed);
    const double demand = kPouchMassKg * (6.0 * (desired_speed - toward_speed) +
        tangential_speed_sq / std::max(distance, .03)) - environment.Dot(direction);
    const double mechanical_power = kRetrievalPowerW * .5 * kTransmissionEfficiency * draw_effort_;
    const double force_cap = kRetrievalForceN * .5 * kTransmissionEfficiency * draw_effort_;
    // Source power is bounded even under the largest acceleration this
    // substep could admit. Negative F·v is payout braking, not paid work.
    const double speed_bound = std::max(0.0, toward_speed) +
        (force_cap + environment.Length()) / kPouchMassKg * dt;
    const double powered_cap = toward_speed >= 0 ?
        std::min(force_cap, mechanical_power / std::max({desired_speed, speed_bound, .1})) : force_cap;
    const double tension = std::clamp(demand, 0.0, powered_cap);
    return direction * float(tension);
}

void Slingshot::finish_recovery() {
    // A captured rail seat has a finite 30 mm clearance. Build the restraint
    // at the measured pose rather than snapping it to a scripted exact point.
    state_.guide_origin = kit_.body_position(pouch_);
    state_.carrier_origin = kit_.body_position(launch_rail_);
    state_.ratchet_draw_m = 0;
    create_guide(0);
    carrier_guide_->SetMotorState(EMotorState::Off);
    system_.RemoveConstraint(carrier_guide_);
    carrier_guide_ = nullptr;
    create_carrier_guide();
    bind_carrier();
    state_.recovering = false;
    state_.released = false;
    state_.work_j = state_.source_work_j = state_.transmission_loss_j = state_.dissipated_work_j = 0;
    state_.ledger_valid = true;
    contact_observed_.store(false, std::memory_order_relaxed);
    external_influence_observed_.store(false, std::memory_order_relaxed);
    state_.source_power_w = state_.retrieval_source_power_w = 0;
    state_.energy_residual_j = 0;
    audit_rider_ = false;
    audit_initialized_ = false;
    initial_mechanical_energy_j_ = 0;
    draw_effort_ = 0;
    refresh_state();
}

void Slingshot::post_step(float) {
    finish_previous_step();
    const bool machine_contact = contact_observed_.exchange(false, std::memory_order_relaxed);
    const bool external_influence = external_influence_observed_.exchange(false, std::memory_order_relaxed);
    if (audit_initialized_ && (machine_contact || (audit_rider_ && external_influence)))
        state_.ledger_valid = false;
    // Query the completed step only. A pre-BOARD cached pouch/player pair
    // belongs to the newly captured baseline, not to its subsequent audit.
    // Contact invalidates the closed-motion claim without erasing residuals.
    if (audit_initialized_ && state_.ledger_valid) {
        BodyIDVector bodies;
        system_.GetBodies(bodies);
        const auto pouch_id = kit_.body_id(pouch_);
        const auto carrier_id = kit_.body_id(launch_rail_);
        if (std::any_of(bodies.begin(), bodies.end(), [&](BodyID other) {
                return (other != pouch_id && system_.WereBodiesInContact(pouch_id, other)) ||
                       (other != carrier_id && system_.WereBodiesInContact(carrier_id, other));
            }))
            state_.ledger_valid = false;
    }
    refresh_state();
}

void Slingshot::restore(const State &saved) {
    const auto launches = state_.launch_count;
    detach();
    system_.GetBodyInterface().SetCollisionGroup(player_, saved_player_group_);
    collision_exclusion_active_ = false;
    remove_guide();
    remove_launch_guide();
    remove_aim_gimbal();
    unbind_carrier();
    if (carrier_guide_ != nullptr) system_.RemoveConstraint(carrier_guide_);
    carrier_guide_ = nullptr;
    state_ = saved;
    state_.launch_count = std::max(launches, saved.launch_count);
    refresh_geometry();
    if (!saved.aim_ready && saved.guide_latched) create_aim_gimbal();
    else create_carrier_guide();
    if (saved.guide_latched) {
        create_guide(saved.ratchet_draw_m);
        if (carrier_guide_ != nullptr) {
            carrier_guide_->SetLimits(0, float(kMaximumDrawM));
            bind_carrier();
        }
    } else if (saved.guided_launch) {
        const float held = carrier_guide_->GetCurrentPosition();
        carrier_guide_->SetMotorState(EMotorState::Off);
        carrier_guide_->SetLimits(held, held);
        state_.launch_track_start = saved.launch_track_start;
        create_launch_guide(true);
    }
    else if (!saved.recovering) {
        const float held = carrier_guide_->GetCurrentPosition();
        carrier_guide_->SetLimits(held, held);
    }
    // The checkpoint's true body pose, rather than a saved interaction flag,
    // must still fit the pouch before rebuilding the harness.
    if (saved.seated) attach(true);
    audit_rider_ = harness_ != nullptr || (saved.released && !saved.recovering);
    audit_initialized_ = saved.seated || saved.work_j > 0 || saved.recovering;
    initial_mechanical_energy_j_ = 0;
    if (audit_initialized_) {
        double kinetic = 0, gravity = 0;
        double mechanical = body_energy(kit_.body_id(pouch_), kinetic, gravity);
        mechanical += body_energy(kit_.body_id(launch_rail_), kinetic, gravity);
        if (audit_rider_) mechanical += body_energy(player_, kinetic, gravity);
        initial_mechanical_energy_j_ = mechanical + saved.energy_j - saved.work_j +
            saved.transmission_loss_j + saved.dissipated_work_j - saved.energy_residual_j;
        audit_initialized_ = true;
    }
    state_.energy_residual_j = audit_initialized_ ? saved.energy_residual_j : 0;
    contact_observed_.store(false, std::memory_order_relaxed);
    external_influence_observed_.store(false, std::memory_order_relaxed);
    state_.seated = harness_ != nullptr;
    state_.source_power_w = 0;
    step_pending_ = false;
    refresh_state();
}

void Slingshot::reset() {
    detach();
    system_.GetBodyInterface().SetCollisionGroup(player_, saved_player_group_);
    collision_exclusion_active_ = false;
    remove_guide();
    remove_launch_guide();
    remove_aim_gimbal();
    unbind_carrier();
    if (carrier_guide_ != nullptr) system_.RemoveConstraint(carrier_guide_);
    carrier_guide_ = nullptr;
    const double yaw = state_.yaw_rad, elevation = state_.elevation_rad;
    const auto launches = state_.launch_count;
    state_ = State{};
    state_.launch_count = launches;
    auto &bodies = system_.GetBodyInterface();
    bodies.SetPositionAndRotation(kit_.body_id(pouch_), neutral_position(),
                                 Quat::sIdentity(), EActivation::Activate);
    bodies.SetLinearAndAngularVelocity(kit_.body_id(pouch_), Vec3::sZero(), Vec3::sZero());
    bodies.SetPositionAndRotation(kit_.body_id(launch_rail_), neutral_position(),
        Quat::sRotation(Vec3::sAxisY(), -float(yaw)) * Quat::sRotation(Vec3::sAxisX(), float(elevation)),
        EActivation::Activate);
    bodies.SetLinearAndAngularVelocity(kit_.body_id(launch_rail_), Vec3::sZero(), Vec3::sZero());
    state_.target_yaw_rad = yaw;
    state_.target_elevation_rad = elevation;
    refresh_geometry();
    create_carrier_guide();
    create_guide(0);
    bind_carrier();
    step_pending_ = false;
    audit_rider_ = false;
    audit_initialized_ = false;
    initial_mechanical_energy_j_ = 0;
    previous_recovery_force_ = Vec3::sZero();
    contact_observed_.store(false, std::memory_order_relaxed);
    external_influence_observed_.store(false, std::memory_order_relaxed);
    refresh_state();
}

std::vector<RVec3> Slingshot::prediction(double duration_seconds, double sample_seconds) const {
    std::vector<RVec3> points;
    if (!std::isfinite(duration_seconds) || !std::isfinite(sample_seconds) ||
        duration_seconds <= 0 || sample_seconds <= 0) return points;
    duration_seconds = std::min(duration_seconds, 20.0);
    sample_seconds = std::clamp(sample_seconds, .025, .5);
    auto &bodies = system_.GetBodyInterface();
    const auto pouch_at = bodies.GetPosition(kit_.body_id(pouch_));
    auto position = model(pouch_at);
    auto velocity = model(RVec3(bodies.GetPointVelocity(kit_.body_id(pouch_), pouch_at)));
    const auto rider_offset = model(bodies.GetPosition(player_) - pouch_at);
    double loaded_mass = kPouchMassKg;
    {
        BodyLockRead lock(system_.GetBodyLockInterface(), player_);
        if (lock.Succeeded()) loaded_mass += 1.0 / lock.GetBody().GetMotionProperties()->GetInverseMass();
    }
    const auto gravity = model(RVec3(system_.GetGravity()));
    constexpr double h = 1.0 / 480.0;
    bool attached = true;
    double next_sample = 0;
    const auto axis = slingshot::aim_axis(state_.yaw_rad, state_.elevation_rad);
    const auto start = position;
    velocity = axis * slingshot::dot(velocity, axis);
    const auto acceleration = [&](slingshot::Vec3 p, slingshot::Vec3 v) {
        if (!attached) return gravity;
        const auto free_acceleration = slingshot::evaluate_bands(p, v, anchors_, band_parameters_).force_n /
                                       loaded_mass + gravity;
        return axis * slingshot::dot(free_acceleration, axis);
    };
    for (double t = 0; t <= duration_seconds; t += h) {
        if (t + h * .5 >= next_sample) {
            points.push_back(world(position + rider_offset));
            next_sample += sample_seconds;
        }
        if (attached && slingshot::dot(position - start, axis) >= kLaunchTrackLengthM) attached = false;
        // RK4 is a predictor only; production launch motion remains Jolt's.
        const auto k1p = velocity;
        const auto k1v = acceleration(position, velocity);
        const auto k2p = velocity + k1v * (h * .5);
        const auto k2v = acceleration(position + k1p * (h * .5), k2p);
        const auto k3p = velocity + k2v * (h * .5);
        const auto k3v = acceleration(position + k2p * (h * .5), k3p);
        const auto k4p = velocity + k3v * h;
        const auto k4v = acceleration(position + k3p * h, k4p);
        position = position + (k1p + k2p * 2 + k3p * 2 + k4p) * (h / 6);
        velocity = velocity + (k1v + k2v * 2 + k3v * 2 + k4v) * (h / 6);
        if (attached && slingshot::dot(position - start, axis) < 0) {
            position = start;
            velocity = {};
        }
    }
    return points;
}

} // namespace scraperx::sim
