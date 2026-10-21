// The bay on the 154 m deck: its leaf, district, chain and bracket (bay.hpp).

#include "sim/bay.hpp"

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace scraperx::sim {
namespace {

using kit::Material;
using kit::Part;

Part box(const JPH::Vec3 half, const JPH::Vec3 center, const Material material, const float mass_kg = 0.0F) {
    return {half, center, JPH::Quat::sIdentity(), material, Part::Shape::Box, mass_kg};
}

Part span(const JPH::Vec3 low, const JPH::Vec3 high, const Material material) {
    return box(0.5F * (high - low), 0.5F * (high + low), material);
}

Part pipe(const float radius, const JPH::Vec3 from, const JPH::Vec3 to, const Material material) {
    const JPH::Vec3 along = to - from;
    const float length = along.Length();
    return {JPH::Vec3(radius, 0.5F * length, radius), 0.5F * (from + to),
            JPH::Quat::sFromTo(JPH::Vec3::sAxisY(), along / length), material, Part::Shape::Cylinder, 0.0F};
}

constexpr float kTop = Bay::kDeckTop;
constexpr float kRailH = 1.0F;

// A waist rail along z at x, from z0 to z1, on posts.
void rail_z(std::vector<Part> &parts, const float x, const float z0, const float z1) {
    parts.push_back(span({x - 0.03F, kTop + kRailH - 0.04F, z0}, {x + 0.03F, kTop + kRailH + 0.04F, z1},
                         Material::Yellow));
    const int posts = std::max(2, static_cast<int>(std::ceil((z1 - z0) / 1.5F)) + 1);
    for (int i = 0; i < posts; ++i) {
        const float z = z0 + 0.04F + (z1 - z0 - 0.08F) * static_cast<float>(i) / static_cast<float>(posts - 1);
        parts.push_back(span({x - 0.03F, kTop, z - 0.03F}, {x + 0.03F, kTop + kRailH, z + 0.03F}, Material::Yellow));
    }
}

JPH::HingeConstraint *world_hinge(JPH::PhysicsSystem &system, const JPH::BodyID body, const JPH::RVec3 pivot,
                                  const JPH::Vec3 axis, const JPH::Vec3 normal, const float min_angle,
                                  const float max_angle, const float friction_nm, const int solver_steps = 0) {
    JPH::HingeConstraintSettings settings;
    settings.mNumVelocityStepsOverride = solver_steps;
    settings.mNumPositionStepsOverride = solver_steps;
    settings.mSpace = JPH::EConstraintSpace::WorldSpace;
    settings.mPoint1 = settings.mPoint2 = pivot;
    settings.mHingeAxis1 = settings.mHingeAxis2 = axis;
    settings.mNormalAxis1 = settings.mNormalAxis2 = normal;
    settings.mLimitsMin = min_angle;
    settings.mLimitsMax = max_angle;
    settings.mMaxFrictionTorque = friction_nm;
    JPH::BodyLockWrite lock(system.GetBodyLockInterface(), body);
    auto *hinge = static_cast<JPH::HingeConstraint *>(settings.Create(JPH::Body::sFixedToWorld, lock.GetBody()));
    system.AddConstraint(hinge);
    return hinge;
}

} // namespace

Bay::Bay(JPH::PhysicsSystem &system, kit::Kit &kit) : system_(system), kit_(kit) {
    const float bracket_top = kBracketTop;
    // ---- the frame: linings, bracket, rungs, rails, the pipe bridge ---------
    std::vector<Part> frame;
    // The bay's west wall, plated down past the bracket; its east wall behind
    // the drive's housing and the fan.
    frame.push_back(span({kWestX - 0.05F, bracket_top - 0.35F, -131.4F}, {kWestX, kTop - 0.02F, -127.0F},
                         Material::Rust));
    frame.push_back(span({kEastX, kTop - 1.6F, -130.4F}, {kEastX + 0.05F, kTop - 0.02F, -125.4F}, Material::Rust));
    // The bracket, on two struts back to the wall.
    frame.push_back(span({kWestX, bracket_top - 0.12F, -131.25F}, {kWestX + 1.0F, bracket_top, -129.35F},
                         Material::Galvanised));
    for (const float z : {-131.1F, -129.5F}) {
        frame.push_back(span({kWestX, bracket_top - 0.62F, z - 0.04F}, {kWestX + 0.08F, bracket_top - 0.12F, z + 0.04F},
                             Material::Steel));
        frame.push_back(span({kWestX + 0.08F, bracket_top - 0.2F, z - 0.04F},
                             {kWestX + 0.9F, bracket_top - 0.12F, z + 0.04F}, Material::Steel));
    }
    // Rungs up the west wall from the bracket to the deck, rails past its edge.
    for (const float side : {-0.25F, 0.25F}) {
        frame.push_back(span({kWestX, bracket_top, kRungsZ + side - 0.03F},
                             {kWestX + 0.06F, kTop - 0.02F, kRungsZ + side + 0.03F}, Material::Yellow));
    }
    for (float y = bracket_top + 0.3F; y <= kTop - 0.1F; y += 0.3F) {
        frame.push_back(span({kWestX, y - 0.02F, kRungsZ - 0.25F}, {kWestX + 0.06F, y + 0.02F, kRungsZ + 0.25F},
                             Material::Steel));
    }
    // Waist rails along the cut, open at the leaf's ends and where she drops.
    rail_z(frame, kWestX - 0.06F, kSouthZ + 0.1F, -130.6F);
    rail_z(frame, kWestX - 0.06F, kLeafZ + kLeafHalfWidth + 0.15F, kNorthZ - 0.1F);
    rail_z(frame, kEastX + 0.06F, kSouthZ + 0.1F, kLeafZ - kLeafHalfWidth - 0.15F);
    rail_z(frame, kEastX + 0.06F, kLeafZ + kLeafHalfWidth + 0.15F, kNorthZ - 0.1F);
    // The pipe bridge over the cut, along it, off the leaf's way: a post on
    // the shaft's edge girder and one on the outer face's, a pipe between
    // them, and the chain's hanger under it over the leaf's eye. The chain's
    // shackle is made fast on a cleat on the end post of the east rail, by
    // the leaf's south-east corner.
    const float bridge_x = kEyeX;
    for (const float z : {kSouthZ - 0.25F, kNorthZ + 0.1F}) {
        frame.push_back(span({bridge_x - 0.08F, kTop - 0.15F, z - 0.08F}, {bridge_x + 0.08F, kBridgeY + 0.3F, z + 0.08F},
                             Material::Rust));
    }
    frame.push_back(pipe(0.1F, {bridge_x, kBridgeY + 0.2F, kSouthZ - 0.25F}, {bridge_x, kBridgeY + 0.2F, kNorthZ + 0.1F},
                         Material::Galvanised));
    frame.push_back(span({kEyeX - 0.06F, kBridgeY - 0.05F, kEyeZ - 0.06F}, {kEyeX + 0.06F, kBridgeY + 0.12F, kEyeZ + 0.06F},
                         Material::Steel));
    const float cleat_x = kEastX + 0.06F;
    const float cleat_y = kTop + kRailH - 0.12F;
    const float cleat_z = kLeafZ - kLeafHalfWidth - 0.15F - 0.04F;
    frame.push_back(span({cleat_x - 0.05F, cleat_y - 0.04F, cleat_z - 0.06F},
                         {cleat_x + 0.12F, cleat_y + 0.04F, cleat_z + 0.06F}, Material::Hazard));
    // The leaf's drive in its housing, south of the leaf against the east
    // wall (clear of the leaf hanging), and the fan's cage north of it.
    frame.push_back(span({kEastX - 0.55F, kTop - 1.05F, -130.25F}, {kEastX, kTop - 0.15F, -129.45F}, Material::Hazard));
    frame.push_back(pipe(0.05F, {kEastX - 0.3F, kTop - 0.6F, -129.45F}, {kEastX - 0.3F, kTop - 0.6F, kLeafZ - 0.2F},
                         Material::Steel));
    frame.push_back(span({kFanX - 0.24F, kFanY - 0.58F, kFanZ - 0.58F}, {kFanX - 0.18F, kFanY + 0.58F, kFanZ + 0.58F},
                         Material::Steel));
    frame.push_back(span({kFanX - 0.18F, kFanY - 0.58F, kFanZ - 0.58F}, {kEastX, kFanY - 0.52F, kFanZ + 0.58F},
                         Material::Steel));
    frame.push_back(span({kFanX - 0.18F, kFanY + 0.52F, kFanZ - 0.58F}, {kEastX, kFanY + 0.58F, kFanZ + 0.58F},
                         Material::Steel));
    frame_ = kit_.add_body(kFrameEntity, frame, JPH::RVec3::sZero(), JPH::Quat::sIdentity(), 0.0F, 0.8F);

    // ---- the leaf -----------------------------------------------------------
    const float length = kLeafEastX - kLeafWestX;
    const JPH::RVec3 leaf_at(0.5F * (kLeafWestX + kLeafEastX), kTop - 0.5F * kLeafThick, kLeafZ);
    std::vector<Part> leaf;
    leaf.push_back(box({0.5F * length, 0.5F * kLeafThick, kLeafHalfWidth}, JPH::Vec3::sZero(), Material::Galvanised,
                       101.5F));
    for (const float side : {-1.0F, 1.0F}) {
        const float z = side * (kLeafHalfWidth - 0.04F);
        leaf.push_back(box({0.5F * length, 0.03F, 0.03F}, {0.0F, 0.5F * kLeafThick + kRailH, z}, Material::Yellow,
                           4.0F));
        leaf.push_back(box({0.5F * length, 0.06F, 0.02F}, {0.0F, 0.5F * kLeafThick + 0.06F, z}, Material::Yellow, 2.0F));
        for (const float x : {-0.47F, 0.0F, 0.47F}) {
            leaf.push_back(box({0.025F, 0.5F * kRailH, 0.025F}, {x * length, 0.5F * kLeafThick + 0.5F * kRailH, z},
                               Material::Yellow, 1.0F));
        }
    }
    // The eye the chain hooks onto.
    const JPH::Vec3 eye_local(kEyeX - float(leaf_at.GetX()), 0.5F * kLeafThick + 0.05F, kEyeZ - kLeafZ);
    leaf.push_back(box({0.05F, 0.05F, 0.03F}, eye_local, Material::Hazard, 0.5F));
    float leaf_parts_kg = 0.0F;
    for (const Part &part : leaf) {
        leaf_parts_kg += part.mass_kg;
    }
    leaf_ = kit_.add_body(kLeafEntity, leaf, leaf_at, JPH::Quat::sIdentity(), leaf_parts_kg, 0.8F);
    kit_.disable_collision(leaf_, frame_);
    eye_ = kit_.add_anchor(leaf_, eye_local + JPH::Vec3(0.0F, 0.05F, 0.0F), 1.2F);
    // Hinged on its east edge; +angle lifts the west end; level is its upper stop.
    // Solved harder than the default: a walkway taking a 220 kg runner at a
    // stride must not shudder on its stop (she would take it for a floor
    // going, and cling).
    leaf_hinge_ = world_hinge(system_, kit_.body_id(leaf_), JPH::RVec3(kLeafEastX, float(leaf_at.GetY()), kLeafZ),
                              -JPH::Vec3::sAxisZ(), JPH::Vec3::sAxisX(), kLeafHangAngle, 0.0F, kLeafBrakeNm, 24);

    // ---- the guard on the drive's housing: a flap hung by its top edge -----
    const JPH::RVec3 guard_at(kEastX - 0.57F, kTop - 0.6F, -129.85F);
    guard_ = kit_.add_body(kGuardEntity, {box({0.015F, 0.38F, 0.32F}, JPH::Vec3::sZero(), Material::Hazard)}, guard_at,
                           JPH::Quat::sIdentity(), 4.0F, 0.6F);
    kit_.disable_collision(guard_, frame_);
    guard_hinge_ = world_hinge(system_, kit_.body_id(guard_), guard_at + JPH::Vec3(0.0F, 0.38F, 0.0F),
                               JPH::Vec3::sAxisZ(), JPH::Vec3::sAxisX(), -0.1F, 1.6F, 0.0F);

    // ---- the fan ------------------------------------------------------------
    std::vector<Part> fan;
    fan.push_back(box({0.08F, 0.09F, 0.09F}, JPH::Vec3::sZero(), Material::Steel, 6.0F));
    for (int blade = 0; blade < 4; ++blade) {
        const float angle = 0.5F * JPH::JPH_PI * static_cast<float>(blade);
        const JPH::Quat turn = JPH::Quat::sRotation(JPH::Vec3::sAxisX(), angle);
        fan.push_back({JPH::Vec3(0.015F, 0.22F, 0.07F), turn * JPH::Vec3(0.0F, 0.28F, 0.0F), turn,
                       Material::Galvanised, Part::Shape::Box, 3.5F});
    }
    const JPH::RVec3 fan_at(kFanX, kFanY, kFanZ);
    fan_ = kit_.add_body(kFanEntity, fan, fan_at, JPH::Quat::sIdentity(), 20.0F, 0.4F);
    kit_.disable_collision(fan_, frame_);
    // Limits of -pi and pi: no limits, it turns freely.
    fan_hinge_ = world_hinge(system_, kit_.body_id(fan_), fan_at, JPH::Vec3::sAxisX(), JPH::Vec3::sAxisY(), -JPH::JPH_PI,
                             JPH::JPH_PI, 0.0F);

    // ---- the chain and its shackle ------------------------------------------
    const JPH::RVec3 hanger(kEyeX, kBridgeY - 0.05F, kEyeZ);
    const JPH::RVec3 eye_world = leaf_at + eye_local + JPH::Vec3(0.0F, 0.05F, 0.0F);
    const float chain_length = float(hanger.GetY() - eye_world.GetY()) + 0.02F;
    cleat_ = kit_.add_anchor(frame_, JPH::Vec3(cleat_x + 0.1F, cleat_y, cleat_z), 1.2F);
    shackle_ = kit_.add_body(kShackleEntity,
                             {box({0.09F, 0.11F, 0.05F}, JPH::Vec3::sZero(), Material::Hazard)},
                             JPH::RVec3(cleat_x + 0.1F, cleat_y + 0.11F, cleat_z), JPH::Quat::sIdentity(), 8.0F, 0.6F);
    kit_.set_carry(shackle_, kit::CarryKind::Shackle, JPH::Vec3(0.0F, 0.11F, 0.0F));
    chain_ = kit_.add_direct_rope(frame_, JPH::Vec3(hanger), shackle_, JPH::Vec3(0.0F, -0.11F, 0.0F), chain_length,
                                  0.0F);
    // As found: made fast on the cleat.
    (void)kit_.hook(kShackleEntity, cleat_);

    // ---- the district -------------------------------------------------------
    SharedSurvival::Support support;
    support.structure = kit_.body_id(leaf_);
    support.hand_local = JPH::Vec3(0.0F, kHandUp, kHandForward);
    district_ = std::make_unique<SharedSurvival>(system_, support);
    leaf_channel_ = district_->add_drive({leaf_hinge_.GetPtr(), guard_hinge_.GetPtr(), kLeafRaiseRadS, kLeafDriveNm,
                                          kLeafBrakeNm});
    fan_channel_ = district_->add_drive({fan_hinge_.GetPtr(), guard_hinge_.GetPtr(), kFanRadS, kFanNm, kFanBrakeNm});
    district_->restore_supply();
    (void)district_->start(leaf_channel_);
    (void)district_->start(fan_channel_);
    post_step();
}

Bay::~Bay() {
    district_.reset();
    for (JPH::Ref<JPH::HingeConstraint> *hinge : {&leaf_hinge_, &guard_hinge_, &fan_hinge_}) {
        if (*hinge != nullptr) {
            system_.RemoveConstraint(hinge->GetPtr());
            *hinge = nullptr;
        }
    }
}

bool Bay::leaf_open() const noexcept {
    return view_.leaf_running && view_.leaf_angle > -0.03F;
}

bool Bay::hooked() const noexcept {
    return kit_.rope_end_entity(chain_) == kLeafEntity;
}

float Bay::mask_factor(const JPH::RVec3 at) const noexcept {
    const float d = float(JPH::Vec3(at - JPH::RVec3(kFanX, kFanY, kFanZ)).Length());
    if (d >= kMaskRadiusM) {
        return 1.0F;
    }
    const float near = 1.0F - d / kMaskRadiusM;
    return 1.0F - 0.75F * view_.fan_fraction * std::min(1.0F, near * 2.0F);
}

void Bay::pre_step(const float dt) {
    // The district's own start: never while hands are in it, its guard is
    // open or a drive still turns, and only kRestartSeconds after all three.
    const SharedSurvival::DriveView leaf = district_->drive_view(leaf_channel_);
    const SharedSurvival::DriveView fan = district_->drive_view(fan_channel_);
    const bool stopped = !leaf.running || !fan.running;
    const bool still = std::abs(leaf.speed_rad_s) < 0.05F && (fan.running || std::abs(fan.speed_rad_s) < 0.05F);
    const float guard = guard_hinge_->GetCurrentAngle();
    if (stopped && still && !district_->snapshot().hands_busy && std::abs(guard) < 0.08F) {
        restart_s_ += dt;
        if (restart_s_ >= kRestartSeconds) {
            district_->restore_supply();
            bool started = false;
            if (!leaf.running) {
                started = district_->start(leaf_channel_) == SharedSurvival::Restart::Started || started;
            }
            if (!fan.running) {
                started = district_->start(fan_channel_) == SharedSurvival::Restart::Started || started;
            }
            if (started) {
                ++view_.restarts;
            }
            restart_s_ = 0.0F;
        }
    } else {
        restart_s_ = 0.0F;
    }
}

void Bay::post_step() {
    const SharedSurvival::DriveView leaf = district_->drive_view(leaf_channel_);
    const SharedSurvival::DriveView fan = district_->drive_view(fan_channel_);
    const SharedSurvival::Snapshot &hands = district_->snapshot();
    view_.leaf_angle = leaf_hinge_->GetCurrentAngle();
    view_.leaf_speed = leaf.speed_rad_s;
    view_.leaf_running = leaf.running;
    view_.fan_running = fan.running;
    view_.fan_fraction = fan.motion_fraction;
    view_.hooked = hooked();
    view_.chain_n = kit_.rope_tension(chain_);
    view_.hands = hands.hands_busy;
    view_.support_n = hands.applied_n;
    view_.hand_x = float(hands.hand.GetX());
    view_.hand_y = float(hands.hand.GetY());
    view_.hand_z = float(hands.hand.GetZ());
    if (hands.hands_busy && !was_hands_) {
        ++view_.trips;
    }
    was_hands_ = hands.hands_busy;
}

Bay::State Bay::capture() const noexcept {
    State state;
    state.restart_s = restart_s_;
    state.supplied = district_->supplied() ? 1 : 0;
    state.leaf_running = district_->drive_view(leaf_channel_).running ? 1 : 0;
    state.fan_running = district_->drive_view(fan_channel_).running ? 1 : 0;
    return state;
}

void Bay::restore(const State &state) {
    // Hands never survive a restore (the Matron's restore resets the
    // district too); the drives are started again as they were.
    district_->reset();
    restart_s_ = state.restart_s;
    if (state.supplied != 0) {
        district_->restore_supply();
    }
    district_->restore_running(leaf_channel_, state.leaf_running != 0);
    district_->restore_running(fan_channel_, state.fan_running != 0);
    post_step();
}

} // namespace scraperx::sim
