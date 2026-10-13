// Phase 3: the yard's trapeze (see trapeze.hpp).
#include "sim/trapeze.hpp"

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace scraperx::sim {
namespace {
using namespace JPH;

kit::Part box(Vec3 half, Vec3 offset, kit::Material material) {
    kit::Part result;
    result.half = half;
    result.offset = offset;
    result.material = material;
    return result;
}

// A round member from a to b (a capsule along its local y).
kit::Part member(Vec3 a, Vec3 b, float radius, kit::Material material) {
    kit::Part result;
    const Vec3 span = b - a;
    result.shape = kit::Part::Shape::Cylinder;
    result.half = Vec3(radius, span.Length() * 0.5F, radius);
    result.offset = (a + b) * 0.5F;
    result.rotation = Quat::sFromTo(Vec3::sAxisY(), span.Normalized());
    result.material = material;
    return result;
}

constexpr float kPlaneZ = float(Trapeze::kPlaneZ);
constexpr float kStandHalfZ = 1.2F;
constexpr float kPostHalfZ = 1.6F;   // the gantry's legs, either side of the plane
constexpr float kRodHalfZ = 0.5F;    // the bar's rods
// The frame's parts, in world coordinates about this origin.
const RVec3 kFrameOrigin(Trapeze::kPivotX, 0.0, Trapeze::kPlaneZ);

Vec3 local(float x, float y, float z) {
    return Vec3(x - float(kFrameOrigin.GetX()), y - float(kFrameOrigin.GetY()), z - float(kFrameOrigin.GetZ()));
}

// A deck from x0 to x1 at top y on four legs to the yard.
void stand(std::vector<kit::Part> &parts, float x0, float x1, float top) {
    parts.push_back(box(Vec3((x1 - x0) * 0.5F, 0.1F, kStandHalfZ), local((x0 + x1) * 0.5F, top - 0.1F, kPlaneZ),
                        kit::Material::Timber));
    for (const float x : {x0 + 0.15F, x1 - 0.15F})
        for (const float z : {kPlaneZ - kStandHalfZ + 0.15F, kPlaneZ + kStandHalfZ - 0.15F})
            parts.push_back(box(Vec3(0.12F, (top - 0.2F) * 0.5F, 0.12F), local(x, (top - 0.2F) * 0.5F, z),
                                kit::Material::Steel));
}

// A ramp of planks from (x0, y0) to (x1, y1) along x in the swing plane.
kit::Part ramp(float x0, float y0, float x1, float y1) {
    const Vec3 a = local(x0, y0, kPlaneZ), b = local(x1, y1, kPlaneZ);
    const Vec3 run = b - a;
    kit::Part part = box(Vec3(0.5F * run.Length(), 0.12F, 0.8F), (a + b) * 0.5F - Vec3(0.0F, 0.12F, 0.0F),
                         kit::Material::Timber);
    part.rotation = Quat::sRotation(Vec3::sAxisZ(), std::atan2(run.GetY(), run.GetX()));
    return part;
}

std::vector<kit::Part> frame_parts() {
    std::vector<kit::Part> parts;
    const float pivot_y = float(Trapeze::kPivotY);
    const float px = float(Trapeze::kPivotX);
    // The gantry: an A of two legs either side of the plane, splayed along
    // it, and the axle across between them.
    for (const float z : {kPlaneZ - kPostHalfZ, kPlaneZ + kPostHalfZ}) {
        for (const float foot : {px - 3.6F, px + 3.6F})
            parts.push_back(member(local(foot, 0.0F, z), local(px, pivot_y + 0.3F, z), 0.14F, kit::Material::Rust));
        parts.push_back(box(Vec3(0.25F, 0.3F, 0.25F), local(px, pivot_y + 0.15F, z), kit::Material::Steel));
    }
    parts.push_back(member(local(px, pivot_y, kPlaneZ - kPostHalfZ), local(px, pivot_y, kPlaneZ + kPostHalfZ), 0.09F,
                           kit::Material::Steel));
    // The launch stand west, and a ramp up to it from the yard.
    const float launch_top = float(Trapeze::kLaunchTopY);
    const float launch_east = float(Trapeze::kLaunchEastX);
    stand(parts, launch_east - 4.0F, launch_east, launch_top);
    parts.push_back(ramp(launch_east - 12.0F, 0.0F, launch_east - 4.0F, launch_top));
    // The landing stand east, and a ramp down from it to the yard.
    const float landing_top = float(Trapeze::kLandingTopY);
    const float landing_west = float(Trapeze::kLandingWestX);
    stand(parts, landing_west, landing_west + 3.5F, landing_top);
    parts.push_back(ramp(landing_west + 3.5F, landing_top, landing_west + 21.5F, 0.0F));
    return parts;
}

// The bar and its rods about the pivot, hanging plumb.
std::vector<kit::Part> bar_parts() {
    const float r = float(Trapeze::kArmLengthM);
    std::vector<kit::Part> parts;
    for (const float z : {-kRodHalfZ, kRodHalfZ})
        parts.push_back(member(Vec3(0.0F, -0.1F, z), Vec3(0.0F, -r, z), 0.025F, kit::Material::Steel));
    parts.push_back(member(Vec3(0.0F, -r, -kRodHalfZ - 0.05F), Vec3(0.0F, -r, kRodHalfZ + 0.05F), 0.035F,
                           kit::Material::Hazard));
    // The yoke at the axle.
    parts.push_back(box(Vec3(0.08F, 0.1F, kRodHalfZ + 0.05F), Vec3::sZero(), kit::Material::Steel));
    return parts;
}
} // namespace

RVec3 Trapeze::pivot() noexcept { return RVec3(kPivotX, kPivotY, kPlaneZ); }

RVec3 Trapeze::bar_position() const {
    const auto &bodies = system_.GetBodyInterface();
    const BodyID bar = kit_.body_id(bar_);
    return bodies.GetPosition(bar) + bodies.GetRotation(bar) * Vec3(0.0F, -float(kArmLengthM), 0.0F);
}

Trapeze::Trapeze(PhysicsSystem &system, kit::Kit &kit, BodyID player) : system_(system), kit_(kit), player_(player) {
    {
        BodyLockRead lock(system_.GetBodyLockInterface(), player_);
        saved_player_group_ = lock.GetBody().GetCollisionGroup();
    }
    frame_ = kit_.add_body(kFrameEntity, frame_parts(), kFrameOrigin, Quat::sIdentity(), 0.0F, 0.8F);
    bar_ = kit_.add_body(kBarEntity, bar_parts(), pivot(), Quat::sIdentity(), kBarMassKg, 0.8F);
    kit_.set_damping(bar_, 0.0F, 0.0F);
    kit_.disable_collision(bar_, frame_);
    // The rider while hanging (the kit's last sub-group): through the bar.
    kit_.disable_collision(kit::BodyIndex{kit::Kit::kCollisionSubGroups - 1}, bar_);
    {
        HingeConstraintSettings settings;
        settings.mSpace = EConstraintSpace::WorldSpace;
        settings.mPoint1 = settings.mPoint2 = pivot();
        // Positive angles turn the bar east (about +z, -y swings toward +x).
        settings.mHingeAxis1 = settings.mHingeAxis2 = Vec3::sAxisZ();
        settings.mNormalAxis1 = settings.mNormalAxis2 = -Vec3::sAxisY();
        settings.mLimitsMin = -1.45F;
        settings.mLimitsMax = 1.45F;
        settings.mMaxFrictionTorque = kAxleFrictionNm;
        BodyLockWrite lock(system_.GetBodyLockInterface(), kit_.body_id(bar_));
        hinge_ = static_cast<HingeConstraint *>(settings.Create(Body::sFixedToWorld, lock.GetBody()));
    }
    system_.AddConstraint(hinge_);
    refresh_state();
}

Trapeze::~Trapeze() {
    detach();
    if (hinge_ != nullptr) system_.RemoveConstraint(hinge_);
}

double Trapeze::player_radius() const {
    const Vec3 from(system_.GetBodyInterface().GetCenterOfMassPosition(player_) - pivot());
    return Vec3(from.GetX(), from.GetY(), 0.0F).Length();
}

double Trapeze::arm_coordinate() const {
    const auto &bodies = system_.GetBodyInterface();
    const BodyID bar = kit_.body_id(bar_);
    const RMat44 to_local = bodies.GetCenterOfMassTransform(bar).InversedRotationTranslation();
    const Vec3 at(to_local * bodies.GetCenterOfMassPosition(player_));
    return double((at - arm_origin_local_).Dot(arm_axis_local_));
}

double Trapeze::arm_rate() const {
    const auto &bodies = system_.GetBodyInterface();
    const BodyID bar = kit_.body_id(bar_);
    const RVec3 centre = bodies.GetCenterOfMassPosition(player_);
    const Vec3 axis = bodies.GetRotation(bar) * arm_axis_local_;
    return double((bodies.GetLinearVelocity(player_) - bodies.GetPointVelocity(bar, centre)).Dot(axis));
}

bool Trapeze::bar_in_reach() const {
    const RVec3 centre = system_.GetBodyInterface().GetCenterOfMassPosition(player_);
    const Vec3 to_bar(bar_position() - centre);
    return to_bar.GetY() >= kCatchLowM && to_bar.GetY() <= kCatchHighM && std::abs(to_bar.GetX()) <= kCatchAcrossM &&
           std::abs(to_bar.GetZ()) <= kCatchSideM;
}

void Trapeze::attach() {
    if (harness_ != nullptr || !airborne_ || !bar_in_reach()) return;
    BodyLockInterfaceNoLock const &locks = system_.GetBodyLockInterfaceNoLock();
    Body *bar = locks.TryGetBody(kit_.body_id(bar_));
    Body *player = locks.TryGetBody(player_);
    if (bar == nullptr || player == nullptr) return;
    catch_energy_j_ = mechanical_energy(true);
    {
        const Vec3 relative = player->GetLinearVelocity() -
                              bar->GetPointVelocity(player->GetCenterOfMassPosition());
        state_.catch_speed_mps = relative.Length();
    }
    // The constraint's axes: y up the line from the body to the pivot, x
    // along the swing, z out of the plane. The body is held where it is;
    // along y it may fall back to the arms' straight stop or be pulled up.
    const RVec3 centre = player->GetCenterOfMassPosition();
    const Vec3 out(Vec3(centre - pivot()).GetX(), Vec3(centre - pivot()).GetY(), 0.0F);
    const Vec3 down = out.Normalized();
    const Vec3 up_arm = -down;
    const Vec3 along(-down.GetY(), down.GetX(), 0.0F);
    const double radius = out.Length();
    straight_radius_m_ = std::max(kArmLengthM + kHangLengthM, radius);
    const float straight = float(radius - straight_radius_m_);   // <= 0: down the arm
    const float pulled = std::max(0.0F, straight + float(kPullTravelM));
    SixDOFConstraintSettings settings;
    settings.mSpace = EConstraintSpace::WorldSpace;
    settings.mPosition1 = settings.mPosition2 = centre;
    settings.mAxisX1 = settings.mAxisX2 = along;
    settings.mAxisY1 = settings.mAxisY2 = up_arm;
    settings.MakeFixedAxis(SixDOFConstraintSettings::TranslationX);
    settings.MakeFixedAxis(SixDOFConstraintSettings::TranslationZ);
    settings.SetLimitedAxis(SixDOFConstraintSettings::TranslationY, straight, pulled);
    // The straight stop gives: the arms take a catch as a stiff spring.
    settings.mLimitsSpringSettings[SixDOFConstraintSettings::TranslationY] =
        SpringSettings(ESpringMode::FrequencyAndDamping, kArmSpringHz, kArmDamping);
    // The arms only pull the body up the arm, never push it down.
    settings.mMotorSettings[SixDOFConstraintSettings::TranslationY] = MotorSettings(6.0F, 1.0F);
    settings.mMotorSettings[SixDOFConstraintSettings::TranslationY].SetForceLimits(0.0F, kPumpForceN);
    settings.mNumVelocityStepsOverride = 30;
    settings.mNumPositionStepsOverride = 6;
    harness_ = static_cast<SixDOFConstraint *>(settings.Create(*bar, *player));
    harness_->SetMotorState(SixDOFConstraintSettings::TranslationY, EMotorState::Position);
    harness_->SetTargetPositionCS(Vec3(0.0F, straight, 0.0F));
    straight_target_ = straight;
    pulled_target_ = pulled;
    {
        const RMat44 to_local = bar->GetCenterOfMassTransform().InversedRotationTranslation();
        arm_axis_local_ = to_local.Multiply3x3(up_arm);
        arm_origin_local_ = Vec3(to_local * centre);
    }
    system_.AddConstraint(harness_);
    auto group = bar->GetCollisionGroup();
    group.SetSubGroupID(kit::Kit::kCollisionSubGroups - 1);
    system_.GetBodyInterface().SetCollisionGroup(player_, group);
    state_.harnessed = true;
    state_.pulled = false;
    state_.pumps = 0;
    ++state_.catches;
    state_.peak_angle_rad = hinge_->GetCurrentAngle();
    state_.peak_hand_force_n = 0.0;
    state_.arm_work_j = 0.0;
    state_.arm_work_out_j = 0.0;
    state_.catch_loss_j = 0.0;
    // The ledger opens once the catch has settled on the arms.
    ledger_wait_ticks_ = 30;
    over_grip_ticks_ = 0;
    previous_pulled_in_m_ = -double(straight);
}

void Trapeze::detach() {
    if (harness_ != nullptr) {
        system_.RemoveConstraint(harness_);
        harness_ = nullptr;
        system_.GetBodyInterface().SetCollisionGroup(player_, saved_player_group_);
    }
    state_.harnessed = false;
    state_.pulled = false;
}

double Trapeze::mechanical_energy(const bool include_rider) const {
    const auto &locks = system_.GetBodyLockInterface();
    const Vec3 gravity = system_.GetGravity();
    double total = 0.0;
    std::vector<BodyID> ids{kit_.body_id(bar_)};
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
            const Vec3 axis = w.Normalized();
            const double inverse_about = axis.Dot(inverse_inertia.Multiply3x3(axis));
            if (inverse_about > 0.0) rotational = 0.5 * w.LengthSq() / inverse_about;
        }
        total += 0.5 * mass * v.LengthSq() + rotational -
                 mass * double(gravity.GetY()) * double(body.GetCenterOfMassPosition().GetY());
    }
    return total;
}

void Trapeze::pre_step(const float, const bool grab, const bool drop, const bool pull, const bool airborne) {
    airborne_ = airborne;
    if (harness_ != nullptr) {
        if (drop) {
            // Let go: the constraint goes, the body keeps its velocity.
            detach();
            return;
        }
        if (pull && !state_.pulled) ++state_.pumps;
        state_.pulled = pull;
        harness_->SetTargetPositionCS(Vec3(0.0F, pull ? pulled_target_ : straight_target_, 0.0F));
    } else {
        if (grab) grab_pending_ticks_ = kGrabBufferTicks;
        if (grab_pending_ticks_ > 0) {
            --grab_pending_ticks_;
            attach();
            if (harness_ != nullptr) grab_pending_ticks_ = 0;
        }
    }
    if (harness_ != nullptr) rate_before_step_ = arm_rate();
}

void Trapeze::post_step(const float delta_seconds) {
    if (harness_ != nullptr && delta_seconds > 0.0F) {
        const double pulled_in = arm_coordinate() - double(straight_target_);
        const double moved = pulled_in - previous_pulled_in_m_;
        previous_pulled_in_m_ = pulled_in;
        state_.pulled_in_m = pulled_in;
        // The hands' load: every part of the constraint, as a force.
        const Vec3 position = harness_->GetTotalLambdaPosition();
        const Vec3 motor = harness_->GetTotalLambdaMotorTranslation();
        state_.hand_force_n = double((position + motor).Length()) / delta_seconds;
        state_.peak_hand_force_n = std::max(state_.peak_hand_force_n, state_.hand_force_n);
        over_grip_ticks_ = state_.hand_force_n > kGripMaxForceN ? over_grip_ticks_ + 1 : 0;
        if (ledger_wait_ticks_ > 0) {
            if (--ledger_wait_ticks_ == 0) {
                state_.initial_mechanical_j = mechanical_energy(true);
                state_.catch_loss_j = catch_energy_j_ - state_.initial_mechanical_j;
            }
        } else {
            // The arms' work: their impulse along the arm (their pull, and
            // their give at the stops) times the body's mean speed up it
            // over the step, which holds for a sharp stop as for a steady
            // pull.
            (void)moved;
            const double work = double(motor.GetY() + position.GetY()) * 0.5 * (rate_before_step_ + arm_rate());
            state_.arm_work_j += work;
            if (work < 0.0) state_.arm_work_out_j += work;
        }
        state_.peak_angle_rad = std::max(state_.peak_angle_rad, double(hinge_->GetCurrentAngle()));
        if (over_grip_ticks_ >= kSlipTicks) {
            // More than two hands can hold: they slip off.
            ++state_.slips;
            detach();
        }
    } else {
        state_.pulled_in_m = 0.0;
        state_.hand_force_n = 0.0;
    }
    refresh_state();
}

void Trapeze::refresh_state() {
    const auto &bodies = system_.GetBodyInterface();
    state_.angle_rad = hinge_->GetCurrentAngle();
    state_.angular_speed = bodies.GetAngularVelocity(kit_.body_id(bar_)).GetZ();
    state_.catch_available = harness_ == nullptr && airborne_ && bar_in_reach();
    {
        const Vec3 to_bar(bar_position() - bodies.GetCenterOfMassPosition(player_));
        state_.catch_near = harness_ == nullptr && airborne_ && to_bar.Length() <= kNearM;
    }
    state_.mechanical_j = mechanical_energy(harness_ != nullptr);
    state_.energy_residual_j = harness_ != nullptr && ledger_wait_ticks_ == 0
                                   ? state_.mechanical_j - state_.initial_mechanical_j - state_.arm_work_j
                                   : 0.0;
}

void Trapeze::restore() {
    detach();
    grab_pending_ticks_ = 0;
    airborne_ = false;
    refresh_state();
}

} // namespace scraperx::sim
