#include "sim/simulation.hpp"

#ifndef SCRAPERX_HAS_JOLT
#error "ScraperX checkpoint requires the pinned Jolt physics substrate"
#endif

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Body/MassProperties.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>


#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <mutex>

namespace {

namespace object_layers {
constexpr JPH::ObjectLayer kStatic = 0;
constexpr JPH::ObjectLayer kMoving = 1;
constexpr JPH::ObjectLayer kCount = 2;
} // namespace object_layers

namespace broadphase_layers {
constexpr JPH::BroadPhaseLayer kStatic(0);
constexpr JPH::BroadPhaseLayer kMoving(1);
constexpr JPH::uint kCount = 2;
} // namespace broadphase_layers

constexpr float kSupportNormalThreshold = 0.55F;
constexpr float kPlayerMaximumRelativeSpeed = 5.5F;
constexpr float kGroundAcceleration = 22.0F;
constexpr float kAirAcceleration = 8.0F;
constexpr float kJumpSpeed = 5.5F;

constexpr double kTranslatingSupportAmplitudeMeters = 2.0;
constexpr double kTranslatingSupportAngularFrequency = 1.0;
constexpr double kRotatingSupportAngularSpeed = 0.8;

constexpr double kHopperGateClosedX = 7.0;
constexpr double kHopperGateOpenX = 11.5;
constexpr double kHopperGateY = 4.8;
constexpr double kHopperGateZ = 33.0;
constexpr double kHopperGateSpeedMetersPerSecond = 2.25;
constexpr double kHopperGateOpenTolerance = 0.12;
constexpr double kHopperLoadInitialX = 7.0;
constexpr double kHopperLoadInitialY = 5.60;
constexpr double kHopperLoadInitialZ = 33.0;
constexpr double kHopperLoadMovedThresholdMeters = 1.0;
constexpr double kHopperControlX = 2.5;
constexpr double kHopperControlY = 0.0;
constexpr double kHopperControlZ = 40.0;
constexpr double kHopperInteractionRadiusMeters = 3.4;

constexpr double kJibMastX = -12.0;
constexpr double kJibMastZ = 48.0;
constexpr double kJibBoomLengthMeters = 8.0;
constexpr double kJibBoomHeightMeters = 9.0;
constexpr double kJibPendantX = -7.0;
constexpr double kJibPendantY = 0.0;
constexpr double kJibPendantZ = 49.0;
constexpr double kJibStationRadiusMeters = 3.6;
constexpr double kJibSlewMinRadians = -1.05;
constexpr double kJibSlewMaxRadians = 1.05;
constexpr double kJibSlewSpeedRadiansPerSecond = 0.35;
constexpr double kJibWinchMinMeters = 2.40;
constexpr double kJibWinchMaxMeters = 8.15;
constexpr double kJibWinchInitialMeters = 8.15;
constexpr double kJibHoistSpeedMetersPerSecond = 1.15;
constexpr double kJibSlingMaxMeters = 0.60;
constexpr double kJibSwlKilograms = 5000.0;
constexpr double kJibRatedCrateKilograms = 800.0;
constexpr double kJibOverweightCrateKilograms = 8000.0;
constexpr double kJibGravity = 9.81;
constexpr float kJibCrateHalfWidth = 0.70F;
constexpr float kJibCrateHalfHeight = 0.16F;
constexpr float kPlayerCapsuleCylinderHalfHeight = 0.55F;
constexpr float kPlayerCapsuleRadius = 0.35F;
constexpr float kPlayerStandingHalfHeight =
    kPlayerCapsuleCylinderHalfHeight + kPlayerCapsuleRadius;
constexpr float kCrateStepHeight = 0.42F;
constexpr float kStepUpHeight = 0.48F;

constexpr double kNeedleSeatX = -5.15;
constexpr double kNeedleSeatY = 5.22;
constexpr double kNeedleSeatZ = 43.90;
constexpr float kNeedleHalfLength = 3.60F;
constexpr float kNeedleHalfHeight = 0.18F;
constexpr float kNeedleHalfWidth = 0.28F;
constexpr double kNeedleMassKilograms = 620.0;
constexpr double kNeedleParkX = -18.0;
constexpr double kNeedleParkZ = 40.0;
constexpr double kNeedleNearLandingX = -10.60;
constexpr double kNeedleNearLandingY = 5.20;
constexpr double kNeedleNearLandingZ = 43.90;
constexpr float kNeedleNearHalfX = 2.20F;
constexpr float kNeedleNearHalfY = 0.20F;
constexpr float kNeedleNearHalfZ = 1.50F;
constexpr double kNeedleFarLandingX = 0.60;
constexpr double kNeedleFarLandingY = 5.20;
constexpr double kNeedleFarLandingZ = 43.90;
constexpr float kNeedleFarHalfX = 2.50F;
constexpr float kNeedleFarHalfY = 0.20F;
constexpr float kNeedleFarHalfZ = 1.50F;
constexpr double kNeedleBayFloorX = 9.50;
constexpr double kNeedleBayFloorY = 8.50;
constexpr double kNeedleBayFloorZ = 40.50;
constexpr float kNeedleBayFloorHalfX = 6.00F;
constexpr float kNeedleBayFloorHalfY = 0.20F;
constexpr float kNeedleBayFloorHalfZ = 5.00F;
constexpr double kNeedleWestPocketX = -8.75;
constexpr double kNeedleEastPocketX = -1.55;
constexpr double kNeedlePocketY = 4.86;
constexpr float kNeedlePocketHalfX = 0.36F;
constexpr float kNeedlePocketHalfY = 0.18F;
constexpr float kNeedlePocketHalfZ = 0.36F;
constexpr double kCrateParkX = -16.0;
constexpr double kCrateParkZ = 53.0;

constexpr double kCageX = 9.50;
constexpr double kCageZ = 34.20;
constexpr float kCageHalfX = 1.55F;
constexpr float kCageHalfY = 0.18F;
constexpr float kCageHalfZ = 1.55F;
constexpr double kCageMinY = 8.52;
constexpr double kCageMaxY = 21.82;
constexpr double kCageSpeedMetersPerSecond = 1.25;
constexpr double kCageLeverRadiusMeters = 3.00;
constexpr double kCageLeverLocalX = 0.70;
constexpr double kCageLeverLocalY = 0.95;
constexpr double kCageLeverLocalZ = -0.35;
constexpr double kCageUpperX = 9.50;
constexpr double kCageUpperY = 21.82;
constexpr double kCageUpperZ = 37.80;
constexpr float kCageUpperHalfX = 3.20F;
constexpr float kCageUpperHalfY = 0.18F;
constexpr float kCageUpperHalfZ = 2.20F;

constexpr double kImpactRockerX = 7.0;
constexpr double kImpactRockerY = 1.65;
constexpr double kImpactRockerZ = 21.55;
constexpr double kImpactRockerAngleThreshold = 0.035;
constexpr double kImpactRockerAngularSpeedThreshold = 0.10;

constexpr double kTraversalLaneX = 2.5;
constexpr double kVaultCenterY = 0.45;
constexpr double kVaultCenterZ = 35.5;
constexpr double kVaultFrontZ = 35.85;
constexpr double kVaultCandidateFarZ = 38.2;
constexpr double kVaultLandingZ = 34.15;
constexpr double kMantleCenterY = 1.15;
constexpr double kMantleCenterZ = 31.5;
constexpr double kMantleFrontZ = 32.9;
constexpr double kMantleCandidateFarZ = 34.25;
constexpr double kMantleTopPlayerY = 3.20;
constexpr double kMantleLandingZ = 30.75;
constexpr double kHangLedgeCenterY = 4.10;
constexpr double kHangLedgeCenterZ = 25.4;
constexpr double kHangLedgeFrontZ = 27.0;
constexpr double kHangCandidateFarZ = 28.35;
constexpr double kHangTargetY = 3.20;
constexpr double kHangTargetZ = 27.48;
constexpr double kHangMantlePlayerY = 5.25;
constexpr double kHangMantleLandingZ = 26.0;
constexpr double kTraversalLaneHalfWidth = 2.65;

constexpr float kVaultDurationSeconds = 0.66F;
constexpr float kVaultLiftSeconds = 0.22F;
constexpr float kMantleDurationSeconds = 0.86F;
constexpr float kMantleLiftSeconds = 0.46F;
constexpr float kTraversalHorizontalSpeed = 5.0F;
constexpr float kVaultLiftVelocity = 4.8F;
constexpr float kMantleVerticalPositionGain = 6.0F;
constexpr float kMantleMaximumVerticalSpeed = 4.5F;
constexpr float kHangPositionGain = 6.0F;
constexpr float kHangMaximumCorrectionSpeed = 3.0F;

class BroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface {
public:
    BroadPhaseLayerInterface() {
        mapping_[object_layers::kStatic] = broadphase_layers::kStatic;
        mapping_[object_layers::kMoving] = broadphase_layers::kMoving;
    }

    [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override {
        return broadphase_layers::kCount;
    }

    [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(
        const JPH::ObjectLayer layer) const override {
        JPH_ASSERT(layer < object_layers::kCount);
        return mapping_[layer];
    }

    [[nodiscard]] const char *GetBroadPhaseLayerName(
        const JPH::BroadPhaseLayer layer) const override {
        if (layer == broadphase_layers::kStatic) {
            return "STATIC";
        }
        if (layer == broadphase_layers::kMoving) {
            return "MOVING";
        }
        JPH_ASSERT(false);
        return "INVALID";
    }

private:
    JPH::BroadPhaseLayer mapping_[object_layers::kCount];
};

class ObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    [[nodiscard]] bool ShouldCollide(const JPH::ObjectLayer object_layer,
                                     const JPH::BroadPhaseLayer broadphase_layer) const override {
        if (object_layer == object_layers::kStatic) {
            return broadphase_layer == broadphase_layers::kMoving;
        }
        if (object_layer == object_layers::kMoving) {
            return true;
        }
        JPH_ASSERT(false);
        return false;
    }
};

class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter {
public:
    [[nodiscard]] bool ShouldCollide(const JPH::ObjectLayer first,
                                     const JPH::ObjectLayer second) const override {
        if (first == object_layers::kStatic) {
            return second == object_layers::kMoving;
        }
        if (first == object_layers::kMoving) {
            return true;
        }
        JPH_ASSERT(false);
        return false;
    }
};

class JoltRuntimeLease final {
public:
    JoltRuntimeLease() {
        const std::scoped_lock lock(mutex_);
        if (lease_count_++ == 0) {
            JPH::RegisterDefaultAllocator();
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
        }
    }

    ~JoltRuntimeLease() {
        const std::scoped_lock lock(mutex_);
        if (--lease_count_ == 0) {
            JPH::UnregisterTypes();
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }
    }

    JoltRuntimeLease(const JoltRuntimeLease &) = delete;
    JoltRuntimeLease &operator=(const JoltRuntimeLease &) = delete;

private:
    static std::mutex mutex_;
    static std::uint32_t lease_count_;
};

std::mutex JoltRuntimeLease::mutex_;
std::uint32_t JoltRuntimeLease::lease_count_ = 0;

struct SupportSample final {
    bool grounded = false;
    std::uint64_t entity_id = 0;
    scraperx::sim::Vector3 contact_point{};
    scraperx::sim::Vector3 point_velocity{};
    float normal_y = 0.0F;
    int rank = 0;
};

class PlayerContactListener final : public JPH::ContactListener {
public:
    void begin_tick() noexcept {
        lock();
        sample_ = {};
        unlock();
    }

    [[nodiscard]] SupportSample sample() const noexcept {
        lock();
        const SupportSample result = sample_;
        unlock();
        return result;
    }

    void OnContactAdded(const JPH::Body &first,
                        const JPH::Body &second,
                        const JPH::ContactManifold &manifold,
                        JPH::ContactSettings &) override {
        observe_support(first, second, manifold);
    }

    void OnContactPersisted(const JPH::Body &first,
                            const JPH::Body &second,
                            const JPH::ContactManifold &manifold,
                            JPH::ContactSettings &) override {
        observe_support(first, second, manifold);
    }

private:
    [[nodiscard]] static int support_rank(const std::uint64_t entity_id,
                                          const bool kinematic) noexcept {
        using scraperx::sim::Simulation;
        if (entity_id == Simulation::kTranslatingSupportEntityId ||
            entity_id == Simulation::kRotatingSupportEntityId ||
            entity_id == Simulation::kHopperGateEntityId ||
            entity_id == Simulation::kJibCrateEntityId ||
            entity_id == Simulation::kCageEntityId) {
            return 2;
        }
        if (entity_id == Simulation::kNeedleEntityId) {
            return kinematic ? 1 : 0;
        }
        if (entity_id == Simulation::kStaticDeckEntityId ||
            entity_id == Simulation::kHighPlatformEntityId ||
            entity_id == Simulation::kHopperChuteEntityId ||
            entity_id == Simulation::kTowerLeftPierEntityId ||
            entity_id == Simulation::kTowerRightPierEntityId ||
            entity_id == Simulation::kHopperLeftWallEntityId ||
            entity_id == Simulation::kHopperRightWallEntityId ||
            entity_id == Simulation::kVaultBlockEntityId ||
            entity_id == Simulation::kMantleBlockEntityId ||
            entity_id == Simulation::kHangLedgeEntityId ||
            entity_id == Simulation::kNeedleWestPocketEntityId ||
            entity_id == Simulation::kNeedleEastPocketEntityId ||
            entity_id == Simulation::kNeedleNearLandingEntityId ||
            entity_id == Simulation::kNeedleFarLandingEntityId ||
            entity_id == Simulation::kNeedleBayFloorEntityId ||
            entity_id == Simulation::kCageUpperLandingEntityId) {
            return 1;
        }
        if (entity_id >= Simulation::kNeedleStairEntityIdBegin &&
            entity_id < Simulation::kNeedleStairEntityIdBegin +
                            Simulation::kNeedleWestStairCount +
                            Simulation::kNeedleEastStairCount) {
            return 1;
        }
        return 0;
    }

    void observe_support(const JPH::Body &first,
                         const JPH::Body &second,
                         const JPH::ContactManifold &manifold) noexcept {
        if (manifold.mRelativeContactPointsOn1.size() == 0 ||
            manifold.mRelativeContactPointsOn2.size() == 0) {
            return;
        }

        const auto first_entity = first.GetUserData();
        const auto second_entity = second.GetUserData();
        std::uint64_t support_entity = 0;
        float support_normal_y = 0.0F;
        JPH::RVec3 support_contact_point{};
        const JPH::Body *support_body = nullptr;

        if (first_entity == scraperx::sim::Simulation::kPlayerEntityId) {
            support_entity = second_entity;
            support_normal_y = -manifold.mWorldSpaceNormal.GetY();
            support_contact_point = manifold.GetWorldSpaceContactPointOn2(0);
            support_body = &second;
        } else if (second_entity == scraperx::sim::Simulation::kPlayerEntityId) {
            support_entity = first_entity;
            support_normal_y = manifold.mWorldSpaceNormal.GetY();
            support_contact_point = manifold.GetWorldSpaceContactPointOn1(0);
            support_body = &first;
        }

        if (support_body == nullptr || support_normal_y < kSupportNormalThreshold) {
            return;
        }
        const int candidate_rank = support_rank(support_entity, support_body->IsKinematic());
        if (candidate_rank == 0) {
            return;
        }

        const JPH::Vec3 point_velocity = support_body->GetPointVelocity(support_contact_point);
        const SupportSample candidate{
            true,
            support_entity,
            {support_contact_point.GetX(), support_contact_point.GetY(), support_contact_point.GetZ()},
            {point_velocity.GetX(), point_velocity.GetY(), point_velocity.GetZ()},
            support_normal_y,
            candidate_rank,
        };

        lock();
        if (!sample_.grounded || candidate.rank > sample_.rank ||
            (candidate.rank == sample_.rank && candidate.normal_y > sample_.normal_y)) {
            sample_ = candidate;
        }
        unlock();
    }

    void lock() const noexcept {
        while (lock_.test_and_set(std::memory_order_acquire)) {
        }
    }

    void unlock() const noexcept {
        lock_.clear(std::memory_order_release);
    }

    mutable std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
    SupportSample sample_{};
};

[[nodiscard]] JPH::RVec3 spawn_position(const scraperx::sim::InitialSpawn spawn) noexcept {
    switch (spawn) {
    case scraperx::sim::InitialSpawn::StaticDeck:
        return {0.0, 3.0, -8.0};
    case scraperx::sim::InitialSpawn::TranslatingSupport:
        return {0.0, 3.0, 8.0};
    case scraperx::sim::InitialSpawn::RotatingSupport:
        return {-6.5, 3.0, 0.0};
    case scraperx::sim::InitialSpawn::HopperControl:
        return {kHopperControlX, 3.0, kHopperControlZ};
    case scraperx::sim::InitialSpawn::TraversalCourse:
        return {kTraversalLaneX, 3.0, 38.0};
    case scraperx::sim::InitialSpawn::HangCourse:
        return {kTraversalLaneX, kHangTargetY, 27.75};
    case scraperx::sim::InitialSpawn::HighDeck:
        return {0.0, 18.55, 0.0};
    case scraperx::sim::InitialSpawn::JibStation:
    case scraperx::sim::InitialSpawn::JibOverweight:
    case scraperx::sim::InitialSpawn::NeedleBay:
        return {kJibPendantX, 3.0, kJibPendantZ};
    case scraperx::sim::InitialSpawn::NeedleNearLanding:
    case scraperx::sim::InitialSpawn::NeedleSeated:
        return {kNeedleNearLandingX,
                kNeedleNearLandingY + static_cast<double>(kNeedleNearHalfY) +
                    static_cast<double>(kPlayerStandingHalfHeight) + 0.08,
                kNeedleNearLandingZ};
    case scraperx::sim::InitialSpawn::CageDeck:
    case scraperx::sim::InitialSpawn::CageSeated:
        return {kCageX,
                kCageMinY + static_cast<double>(kCageHalfY) +
                    static_cast<double>(kPlayerStandingHalfHeight) + 0.08,
                kCageZ};
    case scraperx::sim::InitialSpawn::ApproachGrade:
    default:
        return {0.0, 3.0, 60.0};
    }
}

void approach_relative_horizontal_velocity(JPH::Vec3 &world_velocity,
                                           const JPH::Vec3 reference_velocity,
                                           const double move_input_x,
                                           const double move_input_z,
                                           const float acceleration,
                                           const float delta_seconds) noexcept {
    float relative_x = world_velocity.GetX() - reference_velocity.GetX();
    float relative_z = world_velocity.GetZ() - reference_velocity.GetZ();
    const float target_x = static_cast<float>(move_input_x) * kPlayerMaximumRelativeSpeed;
    const float target_z = static_cast<float>(move_input_z) * kPlayerMaximumRelativeSpeed;
    float delta_x = target_x - relative_x;
    float delta_z = target_z - relative_z;
    const float delta_length = std::sqrt(delta_x * delta_x + delta_z * delta_z);
    const float maximum_delta = acceleration * delta_seconds;

    if (delta_length > maximum_delta && delta_length > 0.0F) {
        const float scale = maximum_delta / delta_length;
        delta_x *= scale;
        delta_z *= scale;
    }

    relative_x += delta_x;
    relative_z += delta_z;
    world_velocity.SetX(reference_velocity.GetX() + relative_x);
    world_velocity.SetZ(reference_velocity.GetZ() + relative_z);
}

[[nodiscard]] double distance_3d(const JPH::RVec3 &position,
                                 const double x,
                                 const double y,
                                 const double z) noexcept {
    const double dx = position.GetX() - x;
    const double dy = position.GetY() - y;
    const double dz = position.GetZ() - z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

[[nodiscard]] float clamp_float(const float value, const float low, const float high) noexcept {
    return std::max(low, std::min(high, value));
}

[[nodiscard]] double clamp_double(const double value, const double low, const double high) noexcept {
    return std::max(low, std::min(high, value));
}

[[nodiscard]] JPH::RVec3 jib_rvec(const double x, const double y, const double z) noexcept {
    return JPH::RVec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
}

[[nodiscard]] JPH::RVec3 jib_boom_direction(const double slew_radians) noexcept {
    return jib_rvec(std::cos(slew_radians), 0.0, std::sin(slew_radians));
}

[[nodiscard]] JPH::RVec3 jib_boom_tip(const double slew_radians) noexcept {
    const JPH::RVec3 direction = jib_boom_direction(slew_radians);
    return jib_rvec(kJibMastX + kJibBoomLengthMeters * static_cast<double>(direction.GetX()),
                    kJibBoomHeightMeters,
                    kJibMastZ + kJibBoomLengthMeters * static_cast<double>(direction.GetZ()));
}

[[nodiscard]] JPH::RVec3 jib_boom_center(const double slew_radians) noexcept {
    const JPH::RVec3 direction = jib_boom_direction(slew_radians);
    return jib_rvec(kJibMastX + 0.5 * kJibBoomLengthMeters * static_cast<double>(direction.GetX()),
                    kJibBoomHeightMeters,
                    kJibMastZ + 0.5 * kJibBoomLengthMeters * static_cast<double>(direction.GetZ()));
}

[[nodiscard]] JPH::RVec3 jib_hook_position(const double slew_radians,
                                           const double winch_length_meters) noexcept {
    const JPH::RVec3 tip = jib_boom_tip(slew_radians);
    return jib_rvec(static_cast<double>(tip.GetX()),
                    kJibBoomHeightMeters - winch_length_meters,
                    static_cast<double>(tip.GetZ()));
}

[[nodiscard]] JPH::RVec3 jib_crate_rest_position(const double slew_radians) noexcept {
    const JPH::RVec3 tip = jib_boom_tip(slew_radians);
    return jib_rvec(static_cast<double>(tip.GetX()),
                    static_cast<double>(kJibCrateHalfHeight),
                    static_cast<double>(tip.GetZ()));
}

[[nodiscard]] JPH::RVec3 needle_seat_position() noexcept {
    return jib_rvec(kNeedleSeatX, kNeedleSeatY, kNeedleSeatZ);
}

[[nodiscard]] JPH::RVec3 needle_padeye(const JPH::RVec3 &center) noexcept {
    return center + JPH::RVec3(0.0F, kNeedleHalfHeight, 0.0F);
}

[[nodiscard]] JPH::RVec3 crate_padeye(const JPH::RVec3 &center) noexcept {
    return center + JPH::RVec3(0.0F, kJibCrateHalfHeight, 0.0F);
}

[[nodiscard]] double yaw_from_quat(const JPH::Quat &rotation) noexcept {
    const JPH::Vec3 forward = rotation * JPH::Vec3(1.0F, 0.0F, 0.0F);
    return std::atan2(static_cast<double>(forward.GetZ()), static_cast<double>(forward.GetX()));
}

} // namespace

namespace scraperx::sim {

class Simulation::PhysicsWorld final {
public:
    explicit PhysicsWorld(const InitialSpawn initial_spawn)
        : temp_allocator_(8U * 1024U * 1024U),
          job_system_(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, 1) {
        physics_system_.Init(768,
                             0,
                             1536,
                             768,
                             broadphase_layer_interface_,
                             object_vs_broadphase_filter_,
                             object_layer_pair_filter_);
        physics_system_.SetContactListener(&contact_listener_);

        auto &bodies = physics_system_.GetBodyInterface();

        auto create_static_box =
            [&bodies](const JPH::Vec3 half_extents,
                      const JPH::RVec3 position,
                      const JPH::Quat rotation,
                      const std::uint64_t entity_id,
                      const float friction) {
                JPH::BodyCreationSettings settings(new JPH::BoxShape(half_extents),
                                                   position,
                                                   rotation,
                                                   JPH::EMotionType::Static,
                                                   object_layers::kStatic);
                settings.mFriction = friction;
                settings.mUserData = entity_id;
                return bodies.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
            };

        deck_id_ = create_static_box(JPH::Vec3(70.0F, 0.5F, 100.0F),
                                     JPH::RVec3(0.0, -0.5, 10.0),
                                     JPH::Quat::sIdentity(),
                                     Simulation::kStaticDeckEntityId,
                                     0.72F);

        high_platform_id_ = create_static_box(JPH::Vec3(1.6F, 0.25F, 1.6F),
                                              JPH::RVec3(0.0, 17.25, 0.0),
                                              JPH::Quat::sIdentity(),
                                              Simulation::kHighPlatformEntityId,
                                              0.72F);

        tower_left_pier_id_ =
            create_static_box(JPH::Vec3(10.0F, 15.0F, 3.0F),
                              JPH::RVec3(-17.0, 15.0, 10.0),
                              JPH::Quat::sIdentity(),
                              Simulation::kTowerLeftPierEntityId,
                              0.78F);
        tower_right_pier_id_ =
            create_static_box(JPH::Vec3(10.0F, 15.0F, 3.0F),
                              JPH::RVec3(17.0, 15.0, 10.0),
                              JPH::Quat::sIdentity(),
                              Simulation::kTowerRightPierEntityId,
                              0.78F);

        hopper_chute_id_ =
            create_static_box(JPH::Vec3(2.0F, 0.16F, 6.0F),
                              JPH::RVec3(7.0, 2.65, 28.8),
                              JPH::Quat::sRotation(JPH::Vec3(1.0F, 0.0F, 0.0F), -0.20F),
                              Simulation::kHopperChuteEntityId,
                              0.24F);
        hopper_left_wall_id_ =
            create_static_box(JPH::Vec3(0.14F, 0.8F, 1.8F),
                              JPH::RVec3(5.05, 5.85, 33.0),
                              JPH::Quat::sIdentity(),
                              Simulation::kHopperLeftWallEntityId,
                              0.55F);
        hopper_right_wall_id_ =
            create_static_box(JPH::Vec3(0.14F, 0.8F, 1.8F),
                              JPH::RVec3(8.95, 5.85, 33.0),
                              JPH::Quat::sIdentity(),
                              Simulation::kHopperRightWallEntityId,
                              0.55F);

        vault_block_id_ =
            create_static_box(JPH::Vec3(2.0F, 0.45F, 0.35F),
                              JPH::RVec3(kTraversalLaneX, kVaultCenterY, kVaultCenterZ),
                              JPH::Quat::sIdentity(),
                              Simulation::kVaultBlockEntityId,
                              0.72F);
        mantle_block_id_ =
            create_static_box(JPH::Vec3(2.2F, 1.15F, 1.4F),
                              JPH::RVec3(kTraversalLaneX, kMantleCenterY, kMantleCenterZ),
                              JPH::Quat::sIdentity(),
                              Simulation::kMantleBlockEntityId,
                              0.78F);
        hang_ledge_id_ =
            create_static_box(JPH::Vec3(2.8F, 0.25F, 1.6F),
                              JPH::RVec3(kTraversalLaneX, kHangLedgeCenterY, kHangLedgeCenterZ),
                              JPH::Quat::sIdentity(),
                              Simulation::kHangLedgeEntityId,
                              0.76F);

        JPH::BodyCreationSettings translating_support_settings(
            new JPH::BoxShape(JPH::Vec3(2.75F, 0.25F, 2.75F)),
            JPH::RVec3(0.0, 0.25, 8.0),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Kinematic,
            object_layers::kMoving);
        translating_support_settings.mAllowSleeping = false;
        translating_support_settings.mFriction = 0.8F;
        translating_support_settings.mUserData = Simulation::kTranslatingSupportEntityId;
        translating_support_id_ =
            bodies.CreateAndAddBody(translating_support_settings, JPH::EActivation::Activate);

        JPH::BodyCreationSettings rotating_support_settings(
            new JPH::BoxShape(JPH::Vec3(3.0F, 0.25F, 3.0F)),
            JPH::RVec3(-8.0, 0.25, 0.0),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Kinematic,
            object_layers::kMoving);
        rotating_support_settings.mAllowSleeping = false;
        rotating_support_settings.mFriction = 0.8F;
        rotating_support_settings.mUserData = Simulation::kRotatingSupportEntityId;
        rotating_support_id_ =
            bodies.CreateAndAddBody(rotating_support_settings, JPH::EActivation::Activate);

        JPH::BodyCreationSettings hopper_gate_settings(
            new JPH::BoxShape(JPH::Vec3(1.8F, 0.15F, 1.8F)),
            JPH::RVec3(kHopperGateClosedX, kHopperGateY, kHopperGateZ),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Kinematic,
            object_layers::kMoving);
        hopper_gate_settings.mAllowSleeping = false;
        hopper_gate_settings.mFriction = 0.42F;
        hopper_gate_settings.mUserData = Simulation::kHopperGateEntityId;
        hopper_gate_id_ =
            bodies.CreateAndAddBody(hopper_gate_settings, JPH::EActivation::Activate);

        JPH::BodyCreationSettings hopper_load_settings(
            new JPH::SphereShape(0.65F),
            JPH::RVec3(kHopperLoadInitialX, kHopperLoadInitialY, kHopperLoadInitialZ),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic,
            object_layers::kMoving);
        hopper_load_settings.mAllowSleeping = false;
        hopper_load_settings.mFriction = 0.22F;
        hopper_load_settings.mRestitution = 0.04F;
        hopper_load_settings.mLinearDamping = 0.04F;
        hopper_load_settings.mAngularDamping = 0.04F;
        hopper_load_settings.mUserData = Simulation::kHopperLoadEntityId;
        hopper_load_id_ =
            bodies.CreateAndAddBody(hopper_load_settings, JPH::EActivation::Activate);

        JPH::BodyCreationSettings impact_rocker_settings(
            new JPH::BoxShape(JPH::Vec3(2.0F, 1.60F, 0.22F)),
            JPH::RVec3(kImpactRockerX, kImpactRockerY, kImpactRockerZ),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic,
            object_layers::kMoving);
        impact_rocker_settings.mAllowedDOFs = JPH::EAllowedDOFs::RotationX;
        impact_rocker_settings.mAllowSleeping = false;
        impact_rocker_settings.mFriction = 0.52F;
        impact_rocker_settings.mRestitution = 0.03F;
        impact_rocker_settings.mAngularDamping = 0.08F;
        impact_rocker_settings.mUserData = Simulation::kImpactRockerEntityId;
        impact_rocker_id_ =
            bodies.CreateAndAddBody(impact_rocker_settings, JPH::EActivation::Activate);

        JPH::BodyCreationSettings player_settings(
            new JPH::CapsuleShape(kPlayerCapsuleCylinderHalfHeight, kPlayerCapsuleRadius),
            spawn_position(initial_spawn),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic,
            object_layers::kMoving);
        player_settings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX |
                                       JPH::EAllowedDOFs::TranslationY |
                                       JPH::EAllowedDOFs::TranslationZ;
        player_settings.mAllowSleeping = false;
        player_settings.mFriction = 0.0F;
        player_settings.mLinearDamping = 0.0F;
        player_settings.mUserData = Simulation::kPlayerEntityId;
        player_id_ = bodies.CreateAndAddBody(player_settings, JPH::EActivation::Activate);

        crate_mass_kg_ = initial_spawn == InitialSpawn::JibOverweight
                             ? kJibOverweightCrateKilograms
                             : kJibRatedCrateKilograms;
        slew_radians_ = 0.0;
        winch_length_ = kJibWinchInitialMeters;
        jib_brake_engaged_ = true;

        jib_mast_id_ = create_static_box(JPH::Vec3(0.38F, 4.55F, 0.38F),
                                         JPH::RVec3(kJibMastX, 4.55, kJibMastZ),
                                         JPH::Quat::sIdentity(),
                                         Simulation::kJibMastEntityId,
                                         0.70F);

        const JPH::RVec3 boom_center = jib_boom_center(slew_radians_);
        JPH::BodyCreationSettings boom_settings(
            new JPH::BoxShape(JPH::Vec3(static_cast<float>(kJibBoomLengthMeters * 0.5), 0.18F, 0.18F)),
            boom_center,
            JPH::Quat::sRotation(JPH::Vec3(0.0F, 1.0F, 0.0F), static_cast<float>(slew_radians_)),
            JPH::EMotionType::Kinematic,
            object_layers::kMoving);
        boom_settings.mAllowSleeping = false;
        boom_settings.mFriction = 0.55F;
        boom_settings.mUserData = Simulation::kJibBoomEntityId;
        jib_boom_id_ = bodies.CreateAndAddBody(boom_settings, JPH::EActivation::Activate);

        JPH::BodyCreationSettings hook_settings(
            new JPH::SphereShape(0.12F),
            jib_hook_position(slew_radians_, winch_length_),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Kinematic,
            object_layers::kMoving);
        hook_settings.mIsSensor = true;
        hook_settings.mAllowSleeping = false;
        hook_settings.mUserData = Simulation::kJibHookEntityId;
        jib_hook_id_ = bodies.CreateAndAddBody(hook_settings, JPH::EActivation::Activate);

        const bool needle_fixture =
            initial_spawn == InitialSpawn::NeedleBay ||
            initial_spawn == InitialSpawn::NeedleNearLanding ||
            initial_spawn == InitialSpawn::NeedleSeated ||
            initial_spawn == InitialSpawn::CageDeck ||
            initial_spawn == InitialSpawn::CageSeated;
        const JPH::RVec3 crate_spawn =
            needle_fixture ? jib_rvec(kCrateParkX, static_cast<double>(kJibCrateHalfHeight), kCrateParkZ)
                           : jib_crate_rest_position(slew_radians_);
        JPH::BodyCreationSettings crate_settings(
            new JPH::BoxShape(JPH::Vec3(kJibCrateHalfWidth, kJibCrateHalfHeight, kJibCrateHalfWidth)),
            crate_spawn,
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic,
            object_layers::kMoving);
        crate_settings.mAllowSleeping = false;
        crate_settings.mFriction = 0.82F;
        crate_settings.mRestitution = 0.0F;
        crate_settings.mLinearDamping = 0.08F;
        crate_settings.mAngularDamping = 0.12F;
        crate_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        crate_settings.mMassPropertiesOverride.mMass = static_cast<float>(crate_mass_kg_);
        crate_settings.mUserData = Simulation::kJibCrateEntityId;
        jib_crate_id_ = bodies.CreateAndAddBody(crate_settings, JPH::EActivation::Activate);

        needle_west_pocket_id_ =
            create_static_box(JPH::Vec3(kNeedlePocketHalfX, kNeedlePocketHalfY, kNeedlePocketHalfZ),
                              jib_rvec(kNeedleWestPocketX, kNeedlePocketY, kNeedleSeatZ),
                              JPH::Quat::sIdentity(),
                              Simulation::kNeedleWestPocketEntityId,
                              0.78F);
        needle_east_pocket_id_ =
            create_static_box(JPH::Vec3(kNeedlePocketHalfX, kNeedlePocketHalfY, kNeedlePocketHalfZ),
                              jib_rvec(kNeedleEastPocketX, kNeedlePocketY, kNeedleSeatZ),
                              JPH::Quat::sIdentity(),
                              Simulation::kNeedleEastPocketEntityId,
                              0.78F);
        needle_near_landing_id_ =
            create_static_box(JPH::Vec3(kNeedleNearHalfX, kNeedleNearHalfY, kNeedleNearHalfZ),
                              jib_rvec(kNeedleNearLandingX, kNeedleNearLandingY, kNeedleNearLandingZ),
                              JPH::Quat::sIdentity(),
                              Simulation::kNeedleNearLandingEntityId,
                              0.80F);
        needle_far_landing_id_ =
            create_static_box(JPH::Vec3(kNeedleFarHalfX, kNeedleFarHalfY, kNeedleFarHalfZ),
                              jib_rvec(kNeedleFarLandingX, kNeedleFarLandingY, kNeedleFarLandingZ),
                              JPH::Quat::sIdentity(),
                              Simulation::kNeedleFarLandingEntityId,
                              0.80F);
        needle_bay_floor_id_ =
            create_static_box(JPH::Vec3(kNeedleBayFloorHalfX, kNeedleBayFloorHalfY, kNeedleBayFloorHalfZ),
                              jib_rvec(kNeedleBayFloorX, kNeedleBayFloorY, kNeedleBayFloorZ),
                              JPH::Quat::sIdentity(),
                              Simulation::kNeedleBayFloorEntityId,
                              0.80F);

        for (std::uint32_t i = 0; i < Simulation::kNeedleWestStairCount; ++i) {
            const double y_top = 0.36 * static_cast<double>(i + 1);
            const double z = 53.20 - 0.60 * static_cast<double>(i);
            west_stair_ids_[i] = create_static_box(
                JPH::Vec3(1.15F, 0.10F, 0.32F),
                jib_rvec(kNeedleNearLandingX, y_top - 0.10, z),
                JPH::Quat::sIdentity(),
                Simulation::kNeedleStairEntityIdBegin + i,
                0.80F);
        }
        for (std::uint32_t i = 0; i < Simulation::kNeedleEastStairCount; ++i) {
            const double y_top = 5.40 + 0.367 * static_cast<double>(i + 1);
            const double x = 2.40 + 0.68 * static_cast<double>(i);
            east_stair_ids_[i] = create_static_box(
                JPH::Vec3(0.42F, 0.10F, 1.20F),
                jib_rvec(x, y_top - 0.10, kNeedleFarLandingZ),
                JPH::Quat::sIdentity(),
                Simulation::kNeedleStairEntityIdBegin + Simulation::kNeedleWestStairCount + i,
                0.80F);
        }

        needle_seated_ = initial_spawn == InitialSpawn::NeedleSeated ||
                         initial_spawn == InitialSpawn::CageSeated;
        JPH::RVec3 needle_spawn = jib_rvec(kNeedleParkX, static_cast<double>(kNeedleHalfHeight), kNeedleParkZ);
        if (initial_spawn == InitialSpawn::NeedleBay ||
            initial_spawn == InitialSpawn::NeedleNearLanding) {
            needle_spawn = jib_crate_rest_position(slew_radians_);
            needle_spawn.SetY(kNeedleHalfHeight);
        } else if (needle_seated_) {
            needle_spawn = needle_seat_position();
        }

        JPH::BodyCreationSettings needle_settings(
            new JPH::BoxShape(JPH::Vec3(kNeedleHalfLength, kNeedleHalfHeight, kNeedleHalfWidth)),
            needle_spawn,
            JPH::Quat::sIdentity(),
            needle_seated_ ? JPH::EMotionType::Kinematic : JPH::EMotionType::Dynamic,
            object_layers::kMoving);
        needle_settings.mAllowSleeping = false;
        needle_settings.mFriction = 0.84F;
        needle_settings.mRestitution = 0.0F;
        needle_settings.mLinearDamping = 0.18F;
        needle_settings.mAngularDamping = 0.25F;
        needle_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        needle_settings.mMassPropertiesOverride.mMass = static_cast<float>(kNeedleMassKilograms);
        needle_settings.mUserData = Simulation::kNeedleEntityId;
        needle_id_ = bodies.CreateAndAddBody(needle_settings, JPH::EActivation::Activate);

        cage_y_ = kCageMinY;
        cage_command_ = 0.0;
        cage_brake_engaged_ = true;
        JPH::BodyCreationSettings cage_settings(
            new JPH::BoxShape(JPH::Vec3(kCageHalfX, kCageHalfY, kCageHalfZ)),
            jib_rvec(kCageX, cage_y_, kCageZ),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Kinematic,
            object_layers::kMoving);
        cage_settings.mAllowSleeping = false;
        cage_settings.mFriction = 0.82F;
        cage_settings.mUserData = Simulation::kCageEntityId;
        cage_id_ = bodies.CreateAndAddBody(cage_settings, JPH::EActivation::Activate);

        cage_upper_landing_id_ =
            create_static_box(JPH::Vec3(kCageUpperHalfX, kCageUpperHalfY, kCageUpperHalfZ),
                              jib_rvec(kCageUpperX, kCageUpperY, kCageUpperZ),
                              JPH::Quat::sIdentity(),
                              Simulation::kCageUpperLandingEntityId,
                              0.78F);

        if (initial_spawn == InitialSpawn::NeedleBay) {
            attach_hook(HookLoad::Needle);
        } else if (!needle_fixture) {
            attach_hook(HookLoad::Crate);
        } else {
            hook_attachment_ = HookLoad::None;
        }

        physics_system_.OptimizeBroadPhase();
        read_state();
    }

    ~PhysicsWorld() {
        physics_system_.SetContactListener(nullptr);
        auto &bodies = physics_system_.GetBodyInterface();
        if (hook_constraint_.GetPtr() != nullptr) {
            physics_system_.RemoveConstraint(hook_constraint_.GetPtr());
            hook_constraint_ = nullptr;
        }
        remove_and_destroy(bodies, player_id_);
        remove_and_destroy(bodies, needle_id_);
        remove_and_destroy(bodies, cage_id_);
        remove_and_destroy(bodies, cage_upper_landing_id_);
        remove_and_destroy(bodies, jib_crate_id_);
        remove_and_destroy(bodies, jib_hook_id_);
        remove_and_destroy(bodies, jib_boom_id_);
        remove_and_destroy(bodies, jib_mast_id_);
        remove_and_destroy(bodies, impact_rocker_id_);
        remove_and_destroy(bodies, hopper_load_id_);
        remove_and_destroy(bodies, hopper_gate_id_);
        remove_and_destroy(bodies, rotating_support_id_);
        remove_and_destroy(bodies, translating_support_id_);
        remove_and_destroy(bodies, hang_ledge_id_);
        remove_and_destroy(bodies, mantle_block_id_);
        remove_and_destroy(bodies, vault_block_id_);
        remove_and_destroy(bodies, hopper_right_wall_id_);
        remove_and_destroy(bodies, hopper_left_wall_id_);
        remove_and_destroy(bodies, hopper_chute_id_);
        remove_and_destroy(bodies, tower_right_pier_id_);
        remove_and_destroy(bodies, tower_left_pier_id_);
        remove_and_destroy(bodies, high_platform_id_);
        remove_and_destroy(bodies, deck_id_);
        remove_and_destroy(bodies, needle_west_pocket_id_);
        remove_and_destroy(bodies, needle_east_pocket_id_);
        remove_and_destroy(bodies, needle_near_landing_id_);
        remove_and_destroy(bodies, needle_far_landing_id_);
        remove_and_destroy(bodies, needle_bay_floor_id_);
        for (std::uint32_t i = 0; i < Simulation::kNeedleWestStairCount; ++i) {
            remove_and_destroy(bodies, west_stair_ids_[i]);
        }
        for (std::uint32_t i = 0; i < Simulation::kNeedleEastStairCount; ++i) {
            remove_and_destroy(bodies, east_stair_ids_[i]);
        }
    }

    void step(const double move_input_x,
              const double move_input_z,
              const bool jump_requested,
              const bool traversal_requested,
              const bool drop_from_hang_requested,
              const bool hopper_release_requested,
              const bool parachute_requested,
              const bool jib_enter_requested,
              const bool jib_exit_requested,
              const double jib_hoist_input,
              const double jib_slew_input,
              const bool jib_brake_engaged,
              const bool jib_brake_command_valid,
              const bool cage_lever_requested,
              const float delta_seconds,
              const double next_time_seconds) noexcept {
        auto &bodies = physics_system_.GetBodyInterface();

        if (hopper_release_requested && !hopper_release_started_) {
            hopper_release_started_ = true;
        }

        if (jib_brake_command_valid) {
            jib_brake_engaged_ = jib_brake_engaged;
        }
        jib_hoist_input_ = jib_hoist_input;
        jib_slew_input_ = jib_slew_input;
        jib_enter_requested_ = jib_enter_requested;
        jib_exit_requested_ = jib_exit_requested;

        update_support_motion(bodies, delta_seconds, next_time_seconds);
        update_hopper_motion(bodies, delta_seconds);
        update_jib_motion(bodies, delta_seconds);
        update_cage_motion(bodies, cage_lever_requested, delta_seconds);

        if (drop_from_hang_requested && traversal_mode_ == TraversalMode::Hang) {
            traversal_mode_ = TraversalMode::None;
            traversal_elapsed_seconds_ = 0.0F;
            JPH::Vec3 drop_velocity = bodies.GetLinearVelocity(player_id_);
            drop_velocity.SetY(-1.5F);
            bodies.SetLinearVelocity(player_id_, drop_velocity);
        }

        if (traversal_requested && traversal_mode_ == TraversalMode::None) {
            begin_traversal(bodies);
        }

        bool jump_started = false;
        if (traversal_mode_ == TraversalMode::Hang && jump_requested) {
            begin_mantle_from_hang(bodies);
        }

        JPH::Vec3 player_velocity = bodies.GetLinearVelocity(player_id_);
        JPH::Vec3 reference_velocity = airborne_inherited_velocity_;

        if (traversal_mode_ != TraversalMode::None) {
            apply_traversal_controller(bodies, player_velocity, delta_seconds);
        } else {
            if (grounded_ && support_entity_id_ != 0) {
                reference_velocity = current_support_point_velocity(bodies);
                airborne_inherited_velocity_ = reference_velocity;
                approach_relative_horizontal_velocity(player_velocity,
                                                      reference_velocity,
                                                      move_input_x,
                                                      move_input_z,
                                                      kGroundAcceleration,
                                                      delta_seconds);
                player_velocity.SetY(reference_velocity.GetY());
            } else {
                approach_relative_horizontal_velocity(player_velocity,
                                                      reference_velocity,
                                                      move_input_x,
                                                      move_input_z,
                                                      kAirAcceleration,
                                                      delta_seconds);
            }

            jump_started = jump_requested && grounded_;
            if (jump_started) {
                player_velocity.SetY(reference_velocity.GetY() + kJumpSpeed);
            }
        }

        bodies.SetLinearVelocity(player_id_, player_velocity);

        if (parachute_requested) {
            try_deploy_parachute(bodies);
        }

        contact_listener_.begin_tick();
        physics_system_.Update(delta_seconds, 1, &temp_allocator_, &job_system_);

        SupportSample support = contact_listener_.sample();
        if (jump_started || traversal_mode_ == TraversalMode::Hang) {
            support = {};
        }
        support_sample_ = support;
        grounded_ = support.grounded;
        support_entity_id_ = support.entity_id;
        maybe_step_up(bodies);

        apply_parachute_and_fall(bodies, delta_seconds);
        maybe_autocommit();
        maybe_restore_from_death(bodies);

        advance_traversal_after_physics(delta_seconds);
        read_state();
    }

    [[nodiscard]] const Snapshot &state() const noexcept {
        return state_;
    }

    bool commit_checkpoint_public() noexcept {
        if (!grounded_) {
            return false;
        }
        store_checkpoint();
        read_state();
        return true;
    }

private:
    static void remove_and_destroy(JPH::BodyInterface &bodies, const JPH::BodyID body_id) {
        bodies.RemoveBody(body_id);
        bodies.DestroyBody(body_id);
    }

    [[nodiscard]] JPH::BodyID body_id_for_entity(const std::uint64_t entity_id) const noexcept {
        if (entity_id == Simulation::kStaticDeckEntityId) {
            return deck_id_;
        }
        if (entity_id == Simulation::kTranslatingSupportEntityId) {
            return translating_support_id_;
        }
        if (entity_id == Simulation::kRotatingSupportEntityId) {
            return rotating_support_id_;
        }
        if (entity_id == Simulation::kHopperGateEntityId) {
            return hopper_gate_id_;
        }
        if (entity_id == Simulation::kHopperChuteEntityId) {
            return hopper_chute_id_;
        }
        if (entity_id == Simulation::kTowerLeftPierEntityId) {
            return tower_left_pier_id_;
        }
        if (entity_id == Simulation::kTowerRightPierEntityId) {
            return tower_right_pier_id_;
        }
        if (entity_id == Simulation::kHopperLeftWallEntityId) {
            return hopper_left_wall_id_;
        }
        if (entity_id == Simulation::kHopperRightWallEntityId) {
            return hopper_right_wall_id_;
        }
        if (entity_id == Simulation::kVaultBlockEntityId) {
            return vault_block_id_;
        }
        if (entity_id == Simulation::kMantleBlockEntityId) {
            return mantle_block_id_;
        }
        if (entity_id == Simulation::kHangLedgeEntityId) {
            return hang_ledge_id_;
        }
        if (entity_id == Simulation::kHighPlatformEntityId) {
            return high_platform_id_;
        }
        if (entity_id == Simulation::kJibCrateEntityId) {
            return jib_crate_id_;
        }
        if (entity_id == Simulation::kNeedleEntityId) {
            return needle_id_;
        }
        if (entity_id == Simulation::kNeedleNearLandingEntityId) {
            return needle_near_landing_id_;
        }
        if (entity_id == Simulation::kNeedleFarLandingEntityId) {
            return needle_far_landing_id_;
        }
        if (entity_id == Simulation::kNeedleBayFloorEntityId) {
            return needle_bay_floor_id_;
        }
        if (entity_id == Simulation::kCageEntityId) {
            return cage_id_;
        }
        if (entity_id == Simulation::kCageUpperLandingEntityId) {
            return cage_upper_landing_id_;
        }
        if (entity_id == Simulation::kJibBoomEntityId) {
            return jib_boom_id_;
        }
        return {};
    }

    [[nodiscard]] JPH::Vec3 current_support_point_velocity(
        const JPH::BodyInterface &bodies) const noexcept {
        const JPH::BodyID support_id = body_id_for_entity(support_entity_id_);
        if (support_id.IsInvalid()) {
            return JPH::Vec3::sZero();
        }
        return bodies.GetPointVelocity(
            support_id,
            JPH::RVec3(support_sample_.contact_point.x,
                       support_sample_.contact_point.y,
                       support_sample_.contact_point.z));
    }

    void update_support_motion(JPH::BodyInterface &bodies,
                               const float delta_seconds,
                               const double next_time_seconds) noexcept {
        const double translating_x =
            kTranslatingSupportAmplitudeMeters *
            std::sin(kTranslatingSupportAngularFrequency * next_time_seconds);
        bodies.MoveKinematic(translating_support_id_,
                             JPH::RVec3(translating_x, 0.25, 8.0),
                             JPH::Quat::sIdentity(),
                             delta_seconds);

        rotating_support_yaw_radians_ =
            std::fmod(kRotatingSupportAngularSpeed * next_time_seconds,
                      2.0 * 3.14159265358979323846);
        bodies.MoveKinematic(
            rotating_support_id_,
            JPH::RVec3(-8.0, 0.25, 0.0),
            JPH::Quat::sRotation(JPH::Vec3(0.0F, 1.0F, 0.0F),
                                 static_cast<float>(rotating_support_yaw_radians_)),
            delta_seconds);
    }

    void update_hopper_motion(JPH::BodyInterface &bodies,
                              const float delta_seconds) noexcept {
        if (hopper_release_started_) {
            hopper_gate_x_ = std::min(
                kHopperGateOpenX,
                hopper_gate_x_ + kHopperGateSpeedMetersPerSecond * delta_seconds);
        }

        bodies.MoveKinematic(hopper_gate_id_,
                             JPH::RVec3(hopper_gate_x_, kHopperGateY, kHopperGateZ),
                             JPH::Quat::sIdentity(),
                             delta_seconds);
        hopper_gate_open_ =
            hopper_gate_x_ >= kHopperGateOpenX - kHopperGateOpenTolerance;
    }

    void update_jib_motion(JPH::BodyInterface &bodies, const float delta_seconds) noexcept {
        const JPH::RVec3 player_position = bodies.GetPosition(player_id_);
        const bool in_range = distance_3d(player_position, kJibPendantX, 0.9, kJibPendantZ) <=
                              kJibStationRadiusMeters;

        if (jib_enter_requested_ && in_range) {
            jib_station_occupied_ = true;
        }
        if (jib_exit_requested_ || (jib_station_occupied_ && !in_range)) {
            jib_station_occupied_ = false;
            jib_hoist_input_ = 0.0;
            jib_slew_input_ = 0.0;
        }
        jib_station_available_ = in_range && !jib_station_occupied_;

        jib_stalled_ = false;
        jib_at_hoist_limit_ = winch_length_ <= kJibWinchMinMeters + 1.0e-4 ||
                              winch_length_ >= kJibWinchMaxMeters - 1.0e-4;
        jib_at_slew_limit_ = slew_radians_ <= kJibSlewMinRadians + 1.0e-4 ||
                             slew_radians_ >= kJibSlewMaxRadians - 1.0e-4;

        const bool commands_live = jib_station_occupied_;
        const double hoist = commands_live ? jib_hoist_input_ : 0.0;
        const double slew_command = commands_live ? jib_slew_input_ : 0.0;
        const double attached_mass = attached_load_mass();
        const double load_newtons = attached_mass * kJibGravity;
        const bool within_swl = attached_mass <= kJibSwlKilograms + 1.0e-6;
        const bool raise_requested = hoist > 0.05;
        const bool lower_requested = hoist < -0.05;
        const bool slew_requested = std::abs(slew_command) > 0.05;

        if (commands_live && jib_brake_engaged_ && (raise_requested || lower_requested || slew_requested)) {
            jib_stalled_ = true;
        } else if (commands_live && !jib_brake_engaged_) {
            if (raise_requested) {
                if (!within_swl) {
                    jib_stalled_ = true;
                } else if (winch_length_ <= kJibWinchMinMeters + 1.0e-4) {
                    jib_at_hoist_limit_ = true;
                } else {
                    winch_length_ = std::max(
                        kJibWinchMinMeters,
                        winch_length_ - kJibHoistSpeedMetersPerSecond * hoist *
                                            static_cast<double>(delta_seconds));
                    jib_at_hoist_limit_ = winch_length_ <= kJibWinchMinMeters + 1.0e-4;
                    bodies.ActivateBody(jib_crate_id_);
                    bodies.ActivateBody(needle_id_);
                }
            } else if (lower_requested) {
                if (winch_length_ >= kJibWinchMaxMeters - 1.0e-4) {
                    jib_at_hoist_limit_ = true;
                } else {
                    winch_length_ = std::min(
                        kJibWinchMaxMeters,
                        winch_length_ - kJibHoistSpeedMetersPerSecond * hoist *
                                            static_cast<double>(delta_seconds));
                    jib_at_hoist_limit_ = winch_length_ >= kJibWinchMaxMeters - 1.0e-4;
                    bodies.ActivateBody(jib_crate_id_);
                    bodies.ActivateBody(needle_id_);
                }
            }

            if (slew_requested) {
                const double next_slew =
                    slew_radians_ + kJibSlewSpeedRadiansPerSecond * slew_command *
                                        static_cast<double>(delta_seconds);
                if (next_slew <= kJibSlewMinRadians || next_slew >= kJibSlewMaxRadians) {
                    jib_at_slew_limit_ = true;
                    slew_radians_ = clamp_double(next_slew, kJibSlewMinRadians, kJibSlewMaxRadians);
                } else {
                    slew_radians_ = next_slew;
                    jib_at_slew_limit_ = false;
                }
                bodies.ActivateBody(jib_crate_id_);
                bodies.ActivateBody(needle_id_);
            }
        }

        const JPH::RVec3 boom_center = jib_boom_center(slew_radians_);
        const JPH::Quat boom_rotation =
            JPH::Quat::sRotation(JPH::Vec3(0.0F, 1.0F, 0.0F), static_cast<float>(slew_radians_));
        bodies.MoveKinematic(jib_boom_id_, boom_center, boom_rotation, delta_seconds);
        bodies.MoveKinematic(jib_hook_id_,
                             jib_hook_position(slew_radians_, winch_length_),
                             JPH::Quat::sIdentity(),
                             delta_seconds);

        maybe_attach_hook(bodies);
        maybe_seat_or_unseat_needle(bodies, raise_requested && commands_live && !jib_brake_engaged_);
        if (needle_seated_) {
            bodies.MoveKinematic(needle_id_,
                                 needle_seat_position(),
                                 JPH::Quat::sIdentity(),
                                 delta_seconds);
        }

        if (hook_constraint_.GetPtr() != nullptr) {
            hook_constraint_->SetDistance(0.0F, static_cast<float>(kJibSlingMaxMeters));
        }

        jib_load_newtons_ = load_newtons;
    }

    void update_cage_motion(JPH::BodyInterface &bodies,
                            const bool cage_lever_requested,
                            const float delta_seconds) noexcept {
        const JPH::RVec3 player_position = bodies.GetPosition(player_id_);
        const double lever_x = kCageX + kCageLeverLocalX;
        const double lever_y = cage_y_ + kCageLeverLocalY;
        const double lever_z = kCageZ + kCageLeverLocalZ;
        cage_lever_available_ =
            distance_3d(player_position, lever_x, lever_y, lever_z) <= kCageLeverRadiusMeters;

        const bool at_min = cage_y_ <= kCageMinY + 1.0e-4;
        const bool at_max = cage_y_ >= kCageMaxY - 1.0e-4;
        cage_at_limit_ = at_min || at_max;

        if (cage_lever_requested && cage_lever_available_) {
            const bool moving = std::abs(cage_command_) > 0.05 && !cage_brake_engaged_;
            if (moving) {
                cage_command_ = 0.0;
                cage_brake_engaged_ = true;
                cage_stalled_ = false;
            } else if (!at_max) {
                if (!needle_seated_) {
                    cage_command_ = 0.0;
                    cage_brake_engaged_ = true;
                    cage_stalled_ = true;
                } else {
                    cage_command_ = 1.0;
                    cage_brake_engaged_ = false;
                    cage_stalled_ = false;
                }
            } else {
                cage_command_ = -1.0;
                cage_brake_engaged_ = false;
                cage_stalled_ = false;
            }
        }

        if (cage_command_ > 0.05 && !needle_seated_) {
            cage_command_ = 0.0;
            cage_brake_engaged_ = true;
            cage_stalled_ = true;
        }

        double travel = 0.0;
        if (!cage_brake_engaged_ && std::abs(cage_command_) > 0.05) {
            travel = cage_command_ * kCageSpeedMetersPerSecond * static_cast<double>(delta_seconds);
        }
        const double previous_y = cage_y_;
        cage_y_ = clamp_double(cage_y_ + travel, kCageMinY, kCageMaxY);
        if ((travel > 0.0 && cage_y_ >= kCageMaxY - 1.0e-4) ||
            (travel < 0.0 && cage_y_ <= kCageMinY + 1.0e-4)) {
            cage_y_ = travel > 0.0 ? kCageMaxY : kCageMinY;
            cage_command_ = 0.0;
            cage_brake_engaged_ = true;
            cage_at_limit_ = true;
        }
        cage_velocity_y_ = (cage_y_ - previous_y) / static_cast<double>(delta_seconds);

        bodies.MoveKinematic(cage_id_,
                             jib_rvec(kCageX, cage_y_, kCageZ),
                             JPH::Quat::sIdentity(),
                             delta_seconds);
    }

    [[nodiscard]] double attached_load_mass() const noexcept {
        if (hook_attachment_ == HookLoad::Crate) {
            return crate_mass_kg_;
        }
        if (hook_attachment_ == HookLoad::Needle) {
            return kNeedleMassKilograms;
        }
        return 0.0;
    }

    void detach_hook() noexcept {
        if (hook_constraint_.GetPtr() != nullptr) {
            physics_system_.RemoveConstraint(hook_constraint_.GetPtr());
            hook_constraint_ = nullptr;
        }
        hook_attachment_ = HookLoad::None;
    }

    void attach_hook(const HookLoad load) noexcept {
        if (load == HookLoad::None) {
            detach_hook();
            return;
        }
        detach_hook();
        const auto &bodies = physics_system_.GetBodyInterface();
        const JPH::BodyID load_id = load == HookLoad::Crate ? jib_crate_id_ : needle_id_;
        const JPH::RVec3 load_position = bodies.GetPosition(load_id);
        const JPH::RVec3 padeye =
            load == HookLoad::Crate ? crate_padeye(load_position) : needle_padeye(load_position);
        JPH::DistanceConstraintSettings settings;
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mPoint1 = jib_hook_position(slew_radians_, winch_length_);
        settings.mPoint2 = padeye;
        settings.mMinDistance = 0.0F;
        settings.mMaxDistance = static_cast<float>(kJibSlingMaxMeters);
        JPH::Body *hook_body = physics_system_.GetBodyLockInterfaceNoLock().TryGetBody(jib_hook_id_);
        JPH::Body *load_body = physics_system_.GetBodyLockInterfaceNoLock().TryGetBody(load_id);
        if (hook_body == nullptr || load_body == nullptr) {
            return;
        }
        hook_constraint_ =
            static_cast<JPH::DistanceConstraint *>(settings.Create(*hook_body, *load_body));
        physics_system_.AddConstraint(hook_constraint_.GetPtr());
        hook_attachment_ = load;
    }

    void maybe_attach_hook(JPH::BodyInterface &bodies) noexcept {
        const JPH::RVec3 hook = bodies.GetPosition(jib_hook_id_);
        auto consider = [&](const HookLoad load, const JPH::BodyID load_id, const JPH::RVec3 &padeye) {
            if (hook_attachment_ == load) {
                return;
            }
            if (distance_3d(hook, padeye.GetX(), padeye.GetY(), padeye.GetZ()) <=
                kJibSlingMaxMeters + 0.12) {
                attach_hook(load);
                bodies.ActivateBody(load_id);
            }
        };
        if (hook_attachment_ == HookLoad::None) {
            const JPH::RVec3 crate = bodies.GetPosition(jib_crate_id_);
            const JPH::RVec3 needle = bodies.GetPosition(needle_id_);
            const double crate_d =
                distance_3d(hook, crate_padeye(crate).GetX(), crate_padeye(crate).GetY(),
                            crate_padeye(crate).GetZ());
            const double needle_d =
                distance_3d(hook, needle_padeye(needle).GetX(), needle_padeye(needle).GetY(),
                            needle_padeye(needle).GetZ());
            if (crate_d <= needle_d) {
                consider(HookLoad::Crate, jib_crate_id_, crate_padeye(crate));
            }
            if (hook_attachment_ == HookLoad::None) {
                consider(HookLoad::Needle, needle_id_, needle_padeye(needle));
            }
            if (hook_attachment_ == HookLoad::None) {
                consider(HookLoad::Crate, jib_crate_id_, crate_padeye(crate));
            }
        }
    }

    [[nodiscard]] bool needle_aligned_for_seat(const JPH::BodyInterface &bodies) const noexcept {
        const JPH::RVec3 position = bodies.GetPosition(needle_id_);
        const JPH::Vec3 velocity = bodies.GetLinearVelocity(needle_id_);
        const JPH::Quat rotation = bodies.GetRotation(needle_id_);
        const JPH::Vec3 up = rotation * JPH::Vec3(0.0F, 1.0F, 0.0F);
        const JPH::Vec3 axis = rotation * JPH::Vec3(1.0F, 0.0F, 0.0F);
        const double dx = static_cast<double>(position.GetX()) - kNeedleSeatX;
        const double dy = static_cast<double>(position.GetY()) - kNeedleSeatY;
        const double dz = static_cast<double>(position.GetZ()) - kNeedleSeatZ;
        const double speed = std::sqrt(static_cast<double>(velocity.LengthSq()));
        const bool upright = up.GetY() > 0.90F;
        const bool along_x = std::abs(axis.GetX()) > 0.90F;
        return std::hypot(dx, dz) <= 0.55 && dy >= -0.20 && dy <= 0.75 && speed <= 1.45 &&
               upright && along_x;
    }

    void seat_needle(JPH::BodyInterface &bodies) noexcept {
        if (hook_attachment_ == HookLoad::Needle) {
            detach_hook();
        }
        bodies.SetMotionType(needle_id_, JPH::EMotionType::Kinematic, JPH::EActivation::Activate);
        bodies.SetPositionAndRotation(needle_id_,
                                      needle_seat_position(),
                                      JPH::Quat::sIdentity(),
                                      JPH::EActivation::Activate);
        bodies.SetLinearVelocity(needle_id_, JPH::Vec3::sZero());
        bodies.SetAngularVelocity(needle_id_, JPH::Vec3::sZero());
        needle_seated_ = true;
    }

    void unseat_needle(JPH::BodyInterface &bodies) noexcept {
        if (!needle_seated_) {
            return;
        }
        bodies.SetMotionType(needle_id_, JPH::EMotionType::Dynamic, JPH::EActivation::Activate);
        bodies.ActivateBody(needle_id_);
        needle_seated_ = false;
    }

    void maybe_seat_or_unseat_needle(JPH::BodyInterface &bodies, const bool raise_live) noexcept {
        if (needle_seated_) {
            const JPH::RVec3 hook = bodies.GetPosition(jib_hook_id_);
            const JPH::RVec3 padeye = needle_padeye(needle_seat_position());
            const double horiz = std::hypot(static_cast<double>(hook.GetX() - padeye.GetX()),
                                            static_cast<double>(hook.GetZ() - padeye.GetZ()));
            const bool over_span = horiz <= 0.90 &&
                                   static_cast<double>(hook.GetY()) >= padeye.GetY() - 0.25;
            if (raise_live && over_span) {
                unseat_needle(bodies);
                attach_hook(HookLoad::Needle);
            }
            return;
        }
        if (raise_live && hook_attachment_ == HookLoad::Needle) {
            return;
        }
        if (needle_aligned_for_seat(bodies)) {
            seat_needle(bodies);
        }
    }

    void maybe_step_up(JPH::BodyInterface &bodies) noexcept {
        if (traversal_mode_ != TraversalMode::None) {
            return;
        }
        const JPH::RVec3 player = bodies.GetPosition(player_id_);
        const JPH::Vec3 velocity = bodies.GetLinearVelocity(player_id_);
        const double feet = player.GetY() - static_cast<double>(kPlayerStandingHalfHeight);

        struct StepTarget {
            JPH::BodyID id;
            std::uint64_t entity;
            float half_x;
            float half_y;
            float half_z;
            bool snap_center;
            float max_rise;
        };
        StepTarget targets[7] = {
            {jib_crate_id_, Simulation::kJibCrateEntityId, kJibCrateHalfWidth, kJibCrateHalfHeight,
             kJibCrateHalfWidth, true, kCrateStepHeight},
            {needle_id_, Simulation::kNeedleEntityId, kNeedleHalfLength, kNeedleHalfHeight,
             kNeedleHalfWidth, false, kStepUpHeight},
            {needle_near_landing_id_, Simulation::kNeedleNearLandingEntityId, kNeedleNearHalfX,
             kNeedleNearHalfY, kNeedleNearHalfZ, false, kStepUpHeight},
            {needle_far_landing_id_, Simulation::kNeedleFarLandingEntityId, kNeedleFarHalfX,
             kNeedleFarHalfY, kNeedleFarHalfZ, false, kStepUpHeight},
            {needle_bay_floor_id_, Simulation::kNeedleBayFloorEntityId, kNeedleBayFloorHalfX,
             kNeedleBayFloorHalfY, kNeedleBayFloorHalfZ, false, kStepUpHeight},
            {cage_id_, Simulation::kCageEntityId, kCageHalfX, kCageHalfY, kCageHalfZ, false,
             kStepUpHeight},
            {cage_upper_landing_id_, Simulation::kCageUpperLandingEntityId, kCageUpperHalfX,
             kCageUpperHalfY, kCageUpperHalfZ, false, kStepUpHeight},
        };
        if (!needle_seated_) {
            targets[1].id = JPH::BodyID();
        }

        auto try_step = [&](const StepTarget &target) {
            if (target.id.IsInvalid() || support_entity_id_ == target.entity) {
                return false;
            }
            const JPH::RVec3 center = bodies.GetPosition(target.id);
            const double dx = static_cast<double>(player.GetX()) - center.GetX();
            const double dz = static_cast<double>(player.GetZ()) - center.GetZ();
            const double horiz = std::hypot(dx, dz);
            const double radius = static_cast<double>(
                std::max(target.half_x, target.half_z) + kPlayerCapsuleRadius + 0.08F);
            if (horiz > radius) {
                return false;
            }
            const double top = center.GetY() + static_cast<double>(target.half_y);
            const double rise = top - feet;
            if (rise <= 0.04 || rise > static_cast<double>(target.max_rise)) {
                return false;
            }
            const double approach = static_cast<double>(velocity.GetX()) * (center.GetX() - player.GetX()) +
                                    static_cast<double>(velocity.GetZ()) * (center.GetZ() - player.GetZ());
            if (approach < 0.12) {
                return false;
            }
            const double x = target.snap_center ? static_cast<double>(center.GetX())
                                                : static_cast<double>(player.GetX());
            const double z = target.snap_center ? static_cast<double>(center.GetZ())
                                                : static_cast<double>(player.GetZ());
            bodies.SetPosition(player_id_,
                               jib_rvec(x, top + static_cast<double>(kPlayerStandingHalfHeight) + 0.02, z),
                               JPH::EActivation::Activate);
            if (target.snap_center) {
                bodies.SetLinearVelocity(player_id_, JPH::Vec3::sZero());
            }
            return true;
        };

        for (const StepTarget &target : targets) {
            if (try_step(target)) {
                return;
            }
        }
        for (std::uint32_t i = 0; i < Simulation::kNeedleWestStairCount; ++i) {
            StepTarget tread{west_stair_ids_[i],
                             Simulation::kNeedleStairEntityIdBegin + i,
                             1.15F,
                             0.10F,
                             0.32F,
                             false,
                             kStepUpHeight};
            if (try_step(tread)) {
                return;
            }
        }
        for (std::uint32_t i = 0; i < Simulation::kNeedleEastStairCount; ++i) {
            StepTarget tread{east_stair_ids_[i],
                             Simulation::kNeedleStairEntityIdBegin + Simulation::kNeedleWestStairCount + i,
                             0.42F,
                             0.10F,
                             1.20F,
                             false,
                             kStepUpHeight};
            if (try_step(tread)) {
                return;
            }
        }
    }

    [[nodiscard]] TraversalMode traversal_candidate(const JPH::RVec3 &player_position) const noexcept {
        if (traversal_mode_ != TraversalMode::None) {
            return TraversalMode::None;
        }

        const double x = player_position.GetX();
        const double y = player_position.GetY();
        const double z = player_position.GetZ();
        const bool lane_clear = std::abs(x - kTraversalLaneX) <= kTraversalLaneHalfWidth;
        if (!lane_clear) {
            return TraversalMode::None;
        }

        if (grounded_ && y < 1.55 && z >= kVaultFrontZ && z <= kVaultCandidateFarZ) {
            return TraversalMode::Vault;
        }
        if (grounded_ && y < 1.55 && z >= kMantleFrontZ && z <= kMantleCandidateFarZ) {
            return TraversalMode::Mantle;
        }
        if (!grounded_ && y >= 2.55 && y <= 4.30 &&
            z >= kHangLedgeFrontZ && z <= kHangCandidateFarZ) {
            return TraversalMode::Hang;
        }
        return TraversalMode::None;
    }

    void begin_traversal(JPH::BodyInterface &bodies) noexcept {
        const JPH::RVec3 player_position = bodies.GetPosition(player_id_);
        const TraversalMode candidate = traversal_candidate(player_position);
        if (candidate == TraversalMode::None) {
            return;
        }

        traversal_mode_ = candidate;
        traversal_elapsed_seconds_ = 0.0F;
        mantle_from_hang_ = false;
        traversal_reference_velocity_ = grounded_ ? current_support_point_velocity(bodies)
                                                 : airborne_inherited_velocity_;

        const double target_x = std::max(
            kTraversalLaneX - 1.65,
            std::min(kTraversalLaneX + 1.65,
                     static_cast<double>(player_position.GetX())));
        if (candidate == TraversalMode::Vault) {
            traversal_target_ = JPH::RVec3(target_x, 0.90, kVaultLandingZ);
        } else if (candidate == TraversalMode::Mantle) {
            traversal_target_ = JPH::RVec3(target_x, kMantleTopPlayerY, kMantleLandingZ);
        } else {
            traversal_target_ = JPH::RVec3(target_x, kHangTargetY, kHangTargetZ);
        }
    }

    void begin_mantle_from_hang(JPH::BodyInterface &bodies) noexcept {
        const JPH::RVec3 player_position = bodies.GetPosition(player_id_);
        const double target_x = std::max(
            kTraversalLaneX - 1.75,
            std::min(kTraversalLaneX + 1.75,
                     static_cast<double>(player_position.GetX())));
        traversal_mode_ = TraversalMode::Mantle;
        traversal_elapsed_seconds_ = 0.0F;
        mantle_from_hang_ = true;
        traversal_reference_velocity_ = JPH::Vec3::sZero();
        traversal_target_ = JPH::RVec3(target_x, kHangMantlePlayerY, kHangMantleLandingZ);
    }

    void apply_traversal_controller(const JPH::BodyInterface &bodies,
                                    JPH::Vec3 &player_velocity,
                                    const float delta_seconds) noexcept {
        const JPH::RVec3 player_position = bodies.GetPosition(player_id_);

        if (traversal_mode_ == TraversalMode::Hang) {
            const float error_x = static_cast<float>(traversal_target_.GetX() - player_position.GetX());
            const float error_y = static_cast<float>(traversal_target_.GetY() - player_position.GetY());
            const float error_z = static_cast<float>(traversal_target_.GetZ() - player_position.GetZ());
            player_velocity.SetX(clamp_float(error_x * kHangPositionGain,
                                             -kHangMaximumCorrectionSpeed,
                                             kHangMaximumCorrectionSpeed));
            player_velocity.SetY(clamp_float(error_y * kHangPositionGain + 9.81F * delta_seconds,
                                             -kHangMaximumCorrectionSpeed,
                                             kHangMaximumCorrectionSpeed));
            player_velocity.SetZ(clamp_float(error_z * kHangPositionGain,
                                             -kHangMaximumCorrectionSpeed,
                                             kHangMaximumCorrectionSpeed));
            return;
        }

        const float dx = static_cast<float>(traversal_target_.GetX() - player_position.GetX());
        const float dz = static_cast<float>(traversal_target_.GetZ() - player_position.GetZ());
        const float horizontal_length = std::sqrt(dx * dx + dz * dz);
        float direction_x = 0.0F;
        float direction_z = 0.0F;
        if (horizontal_length > 1.0e-5F) {
            direction_x = dx / horizontal_length;
            direction_z = dz / horizontal_length;
        }

        const bool is_vault = traversal_mode_ == TraversalMode::Vault;
        const float lift_seconds = is_vault ? kVaultLiftSeconds : kMantleLiftSeconds;
        const float horizontal_scale = traversal_elapsed_seconds_ < lift_seconds && !is_vault
                                           ? 0.12F
                                           : 1.0F;

        player_velocity.SetX(traversal_reference_velocity_.GetX() +
                             direction_x * kTraversalHorizontalSpeed * horizontal_scale);
        player_velocity.SetZ(traversal_reference_velocity_.GetZ() +
                             direction_z * kTraversalHorizontalSpeed * horizontal_scale);
        if (is_vault) {
            if (traversal_elapsed_seconds_ < lift_seconds) {
                player_velocity.SetY(traversal_reference_velocity_.GetY() +
                                     kVaultLiftVelocity);
            }
        } else {
            const float error_y =
                static_cast<float>(traversal_target_.GetY() - player_position.GetY());
            const float vertical_correction =
                clamp_float(error_y * kMantleVerticalPositionGain + 9.81F * delta_seconds,
                            -kMantleMaximumVerticalSpeed,
                            kMantleMaximumVerticalSpeed);
            player_velocity.SetY(traversal_reference_velocity_.GetY() + vertical_correction);
        }
    }

    void try_deploy_parachute(JPH::BodyInterface &bodies) noexcept {
        const JPH::RVec3 position = bodies.GetPosition(player_id_);
        const bool enough_clearance = position.GetY() > 3.5F &&
                                      traversal_mode_ == TraversalMode::None &&
                                      !grounded_;
        if (!enough_clearance) {
            return;
        }
        parachute_deployed_ = true;
    }

    void apply_parachute_and_fall(JPH::BodyInterface &bodies,
                                  const float delta_seconds) noexcept {
        const JPH::RVec3 position = bodies.GetPosition(player_id_);
        JPH::Vec3 velocity = bodies.GetLinearVelocity(player_id_);

        const bool airborne = !grounded_ && traversal_mode_ != TraversalMode::Hang;
        if (airborne) {
            airborne_seconds_ += static_cast<double>(delta_seconds);
            const double drop = fall_start_y_ - static_cast<double>(position.GetY());
            std::uint8_t severity = 0;
            if (drop > 3.0 || airborne_seconds_ > 0.45) {
                severity = 1;
            }
            if (drop > 8.0 || airborne_seconds_ > 1.1) {
                severity = 2;
            }
            if (drop > 14.0 || airborne_seconds_ > 1.8) {
                severity = 3;
            }
            if (severity > fall_severity_) {
                fall_severity_ = severity;
                ++fear_event_id_;
            }
        } else if (grounded_) {
            parachute_deployed_ = false;
            airborne_seconds_ = 0.0;
            fall_severity_ = 0;
            fall_start_y_ = static_cast<double>(position.GetY());
        }

        if (airborne && airborne_seconds_ <= static_cast<double>(delta_seconds) + 1.0e-9) {
            fall_start_y_ = static_cast<double>(position.GetY()) +
                            std::max(0.0F, velocity.GetY()) * 0.05;
        }

        if (parachute_deployed_ && airborne) {
            if (velocity.GetY() > 0.0F) {
                velocity.SetY(velocity.GetY() * 0.82F);
            }
            const float sink = -6.5F;
            if (velocity.GetY() < sink) {
                velocity.SetY(velocity.GetY() + 28.0F * delta_seconds);
                if (velocity.GetY() > sink) {
                    velocity.SetY(sink);
                }
            }
            velocity.SetX(velocity.GetX() * 0.985F);
            velocity.SetZ(velocity.GetZ() * 0.985F);
            bodies.SetLinearVelocity(player_id_, velocity);
        }

        const bool fell_off_world = position.GetY() < -6.0F;
        const bool fatal_impact = grounded_ && !parachute_deployed_ &&
                                  last_airborne_speed_y_ < -16.0F;
        if (fell_off_world || fatal_impact) {
            pending_death_ = true;
        }
        if (airborne) {
            last_airborne_speed_y_ = velocity.GetY();
        } else {
            last_airborne_speed_y_ = 0.0F;
        }
    }

    void maybe_autocommit() noexcept {
        if (grounded_ && !pending_death_) {
            grounded_dwell_seconds_ += Simulation::kFixedStepSeconds;
            if (grounded_dwell_seconds_ >= 0.35 && !checkpoint_committed_) {
                store_checkpoint();
            }
        } else if (!grounded_) {
            grounded_dwell_seconds_ = 0.0;
        }
    }

    void store_checkpoint() noexcept {
        checkpoint_has_pose_ = true;
        checkpoint_committed_ = true;
        checkpoint_player_position_ = state_.player_position;
        checkpoint_player_velocity_ = state_.player_linear_velocity;
        checkpoint_hopper_release_ = hopper_release_started_;
        checkpoint_hopper_gate_x_ = hopper_gate_x_;
        checkpoint_tick_ = state_.tick_index;
        checkpoint_slew_radians_ = slew_radians_;
        checkpoint_winch_length_ = winch_length_;
        checkpoint_jib_brake_ = jib_brake_engaged_;
        checkpoint_jib_occupied_ = jib_station_occupied_;
        checkpoint_crate_position_ = state_.jib_crate_position;
        checkpoint_crate_velocity_ = state_.jib_crate_linear_velocity;
        checkpoint_needle_seated_ = needle_seated_;
        checkpoint_needle_position_ = state_.needle_position;
        checkpoint_needle_velocity_ = state_.needle_linear_velocity;
        checkpoint_hook_attachment_ = hook_attachment_;
        checkpoint_cage_y_ = cage_y_;
        checkpoint_cage_command_ = cage_command_;
        checkpoint_cage_brake_ = cage_brake_engaged_;
    }

    void maybe_restore_from_death(JPH::BodyInterface &bodies) noexcept {
        if (!pending_death_) {
            return;
        }
        pending_death_ = false;
        if (!checkpoint_has_pose_) {
            store_checkpoint();
        }
        bodies.SetPosition(
            player_id_,
            JPH::RVec3(checkpoint_player_position_.x,
                       checkpoint_player_position_.y,
                       checkpoint_player_position_.z),
            JPH::EActivation::Activate);
        bodies.SetLinearVelocity(
            player_id_,
            JPH::Vec3(static_cast<float>(checkpoint_player_velocity_.x),
                      static_cast<float>(checkpoint_player_velocity_.y),
                      static_cast<float>(checkpoint_player_velocity_.z)));
        hopper_release_started_ = checkpoint_hopper_release_;
        hopper_gate_x_ = checkpoint_hopper_gate_x_;
        slew_radians_ = checkpoint_slew_radians_;
        winch_length_ = checkpoint_winch_length_;
        jib_brake_engaged_ = checkpoint_jib_brake_;
        jib_station_occupied_ = checkpoint_jib_occupied_;
        bodies.SetPosition(
            jib_crate_id_,
            JPH::RVec3(checkpoint_crate_position_.x,
                       checkpoint_crate_position_.y,
                       checkpoint_crate_position_.z),
            JPH::EActivation::Activate);
        bodies.SetLinearVelocity(
            jib_crate_id_,
            JPH::Vec3(static_cast<float>(checkpoint_crate_velocity_.x),
                      static_cast<float>(checkpoint_crate_velocity_.y),
                      static_cast<float>(checkpoint_crate_velocity_.z)));
        bodies.SetAngularVelocity(jib_crate_id_, JPH::Vec3::sZero());
        bodies.MoveKinematic(
            jib_boom_id_,
            jib_boom_center(slew_radians_),
            JPH::Quat::sRotation(JPH::Vec3(0.0F, 1.0F, 0.0F), static_cast<float>(slew_radians_)),
            static_cast<float>(Simulation::kFixedStepSeconds));
        bodies.MoveKinematic(jib_hook_id_,
                             jib_hook_position(slew_radians_, winch_length_),
                             JPH::Quat::sIdentity(),
                             static_cast<float>(Simulation::kFixedStepSeconds));
        needle_seated_ = checkpoint_needle_seated_;
        bodies.SetMotionType(needle_id_,
                             needle_seated_ ? JPH::EMotionType::Kinematic : JPH::EMotionType::Dynamic,
                             JPH::EActivation::Activate);
        bodies.SetPositionAndRotation(
            needle_id_,
            JPH::RVec3(checkpoint_needle_position_.x,
                       checkpoint_needle_position_.y,
                       checkpoint_needle_position_.z),
            JPH::Quat::sIdentity(),
            JPH::EActivation::Activate);
        bodies.SetLinearVelocity(
            needle_id_,
            JPH::Vec3(static_cast<float>(checkpoint_needle_velocity_.x),
                      static_cast<float>(checkpoint_needle_velocity_.y),
                      static_cast<float>(checkpoint_needle_velocity_.z)));
        bodies.SetAngularVelocity(needle_id_, JPH::Vec3::sZero());
        attach_hook(checkpoint_hook_attachment_);
        cage_y_ = checkpoint_cage_y_;
        cage_command_ = checkpoint_cage_command_;
        cage_brake_engaged_ = checkpoint_cage_brake_;
        cage_stalled_ = false;
        bodies.SetPosition(cage_id_,
                           jib_rvec(kCageX, cage_y_, kCageZ),
                           JPH::EActivation::Activate);
        bodies.SetLinearVelocity(cage_id_, JPH::Vec3::sZero());
        parachute_deployed_ = false;
        airborne_seconds_ = 0.0;
        fall_severity_ = 0;
        traversal_mode_ = TraversalMode::None;
        grounded_ = true;
    }

    void advance_traversal_after_physics(const float delta_seconds) noexcept {
        if (traversal_mode_ == TraversalMode::Vault) {
            traversal_elapsed_seconds_ += delta_seconds;
            if (traversal_elapsed_seconds_ >= kVaultDurationSeconds) {
                traversal_mode_ = TraversalMode::None;
                traversal_elapsed_seconds_ = 0.0F;
            }
            return;
        }
        if (traversal_mode_ == TraversalMode::Mantle) {
            traversal_elapsed_seconds_ += delta_seconds;
            if (traversal_elapsed_seconds_ >= kMantleDurationSeconds) {
                traversal_mode_ = TraversalMode::None;
                traversal_elapsed_seconds_ = 0.0F;
                mantle_from_hang_ = false;
            }
        }
    }

    void read_state() noexcept {
        const auto &bodies = physics_system_.GetBodyInterface();

        const JPH::RVec3 player_position = bodies.GetPosition(player_id_);
        const JPH::Vec3 player_velocity = bodies.GetLinearVelocity(player_id_);
        state_.player_position =
            {player_position.GetX(), player_position.GetY(), player_position.GetZ()};
        state_.player_linear_velocity =
            {player_velocity.GetX(), player_velocity.GetY(), player_velocity.GetZ()};
        state_.player_grounded = grounded_;
        state_.support_entity_id = support_entity_id_;
        state_.support_contact_point = support_sample_.contact_point;
        state_.support_point_linear_velocity = support_sample_.point_velocity;

        state_.traversal_mode = traversal_mode_;
        state_.traversal_candidate_mode = traversal_candidate(player_position);
        state_.traversal_assist_available =
            state_.traversal_candidate_mode != TraversalMode::None;
        state_.traversal_target_position =
            {traversal_target_.GetX(), traversal_target_.GetY(), traversal_target_.GetZ()};

        const JPH::RVec3 translating_position = bodies.GetPosition(translating_support_id_);
        const JPH::Vec3 translating_velocity = bodies.GetLinearVelocity(translating_support_id_);
        state_.translating_support_position =
            {translating_position.GetX(), translating_position.GetY(), translating_position.GetZ()};
        state_.translating_support_linear_velocity =
            {translating_velocity.GetX(), translating_velocity.GetY(), translating_velocity.GetZ()};

        const JPH::RVec3 rotating_position = bodies.GetPosition(rotating_support_id_);
        const JPH::Vec3 rotating_angular_velocity = bodies.GetAngularVelocity(rotating_support_id_);
        state_.rotating_support_position =
            {rotating_position.GetX(), rotating_position.GetY(), rotating_position.GetZ()};
        state_.rotating_support_yaw_radians = rotating_support_yaw_radians_;
        state_.rotating_support_angular_velocity =
            {rotating_angular_velocity.GetX(),
             rotating_angular_velocity.GetY(),
             rotating_angular_velocity.GetZ()};

        const JPH::RVec3 gate_position = bodies.GetPosition(hopper_gate_id_);
        state_.hopper_gate_position =
            {gate_position.GetX(), gate_position.GetY(), gate_position.GetZ()};

        const JPH::RVec3 load_position = bodies.GetPosition(hopper_load_id_);
        const JPH::Vec3 load_velocity = bodies.GetLinearVelocity(hopper_load_id_);
        state_.hopper_load_position =
            {load_position.GetX(), load_position.GetY(), load_position.GetZ()};
        state_.hopper_load_linear_velocity =
            {load_velocity.GetX(), load_velocity.GetY(), load_velocity.GetZ()};

        state_.hopper_control_position =
            {kHopperControlX, kHopperControlY, kHopperControlZ};
        state_.hopper_release_started = hopper_release_started_;
        state_.hopper_gate_open = hopper_gate_open_;
        state_.hopper_load_moved =
            distance_3d(load_position,
                        kHopperLoadInitialX,
                        kHopperLoadInitialY,
                        kHopperLoadInitialZ) >= kHopperLoadMovedThresholdMeters;
        state_.hopper_interaction_available =
            !hopper_release_started_ &&
            distance_3d(player_position,
                        kHopperControlX,
                        0.9,
                        kHopperControlZ) <= kHopperInteractionRadiusMeters;

        state_.player_alive = !pending_death_;
        state_.parachute_deployed = parachute_deployed_;
        state_.parachute_allowed = !grounded_ &&
                                   traversal_mode_ == TraversalMode::None &&
                                   player_position.GetY() > 3.5F;
        state_.fall_severity = fall_severity_;
        state_.airborne_seconds = airborne_seconds_;
        state_.fear_event_id = fear_event_id_;
        state_.checkpoint_tick = checkpoint_tick_;
        state_.checkpoint_committed = checkpoint_committed_;

        const JPH::RVec3 rocker_position = bodies.GetPosition(impact_rocker_id_);
        const JPH::Vec3 rocker_angular_velocity = bodies.GetAngularVelocity(impact_rocker_id_);
        const JPH::Quat rocker_rotation = bodies.GetRotation(impact_rocker_id_);
        const double rocker_angle =
            2.0 * std::atan2(static_cast<double>(rocker_rotation.GetX()),
                             static_cast<double>(rocker_rotation.GetW()));
        state_.impact_rocker_position =
            {rocker_position.GetX(), rocker_position.GetY(), rocker_position.GetZ()};
        state_.impact_rocker_angular_velocity =
            {rocker_angular_velocity.GetX(),
             rocker_angular_velocity.GetY(),
             rocker_angular_velocity.GetZ()};
        state_.impact_rocker_angle_radians = rocker_angle;
        state_.impact_rocker_struck =
            std::abs(rocker_angle) >= kImpactRockerAngleThreshold ||
            std::abs(rocker_angular_velocity.GetX()) >= kImpactRockerAngularSpeedThreshold;

        const JPH::RVec3 hook_position = bodies.GetPosition(jib_hook_id_);
        const JPH::RVec3 crate_position = bodies.GetPosition(jib_crate_id_);
        const JPH::Vec3 crate_velocity = bodies.GetLinearVelocity(jib_crate_id_);
        const JPH::RVec3 boom_tip = jib_boom_tip(slew_radians_);
        const JPH::RVec3 needle_position = bodies.GetPosition(needle_id_);
        const JPH::Vec3 needle_velocity = bodies.GetLinearVelocity(needle_id_);
        const JPH::Quat needle_rotation = bodies.GetRotation(needle_id_);
        JPH::RVec3 attached_padeye = hook_position;
        if (hook_attachment_ == HookLoad::Crate) {
            attached_padeye = crate_padeye(crate_position);
        } else if (hook_attachment_ == HookLoad::Needle) {
            attached_padeye = needle_padeye(needle_position);
        }
        const double hook_load_distance =
            hook_attachment_ == HookLoad::None
                ? 1.0e9
                : distance_3d(hook_position,
                              attached_padeye.GetX(),
                              attached_padeye.GetY(),
                              attached_padeye.GetZ());

        state_.jib_pendant_position = {kJibPendantX, kJibPendantY, kJibPendantZ};
        state_.jib_mast_position = {kJibMastX, 0.0, kJibMastZ};
        state_.jib_boom_tip_position = {boom_tip.GetX(), boom_tip.GetY(), boom_tip.GetZ()};
        state_.jib_hook_position = {hook_position.GetX(), hook_position.GetY(), hook_position.GetZ()};
        state_.jib_crate_position =
            {crate_position.GetX(), crate_position.GetY(), crate_position.GetZ()};
        state_.jib_crate_linear_velocity =
            {crate_velocity.GetX(), crate_velocity.GetY(), crate_velocity.GetZ()};
        state_.jib_slew_radians = slew_radians_;
        state_.jib_winch_length_meters = winch_length_;
        state_.jib_crate_mass_kg = crate_mass_kg_;
        state_.jib_swl_kg = kJibSwlKilograms;
        state_.jib_load_newtons = jib_load_newtons_;
        state_.jib_station_available = jib_station_available_;
        state_.jib_station_occupied = jib_station_occupied_;
        state_.jib_brake_engaged = jib_brake_engaged_;
        state_.jib_stalled = jib_stalled_;
        state_.jib_at_hoist_limit = jib_at_hoist_limit_;
        state_.jib_at_slew_limit = jib_at_slew_limit_;
        state_.jib_hook_attached =
            hook_attachment_ != HookLoad::None && hook_load_distance <= kJibSlingMaxMeters + 0.08;
        state_.jib_hook_load = hook_attachment_;
        state_.needle_position = {needle_position.GetX(), needle_position.GetY(), needle_position.GetZ()};
        state_.needle_linear_velocity =
            {needle_velocity.GetX(), needle_velocity.GetY(), needle_velocity.GetZ()};
        state_.needle_yaw_radians = yaw_from_quat(needle_rotation);
        state_.needle_seated = needle_seated_;
        state_.needle_near_landing_position = {kNeedleNearLandingX, kNeedleNearLandingY,
                                               kNeedleNearLandingZ};
        state_.needle_far_landing_position = {kNeedleFarLandingX, kNeedleFarLandingY,
                                              kNeedleFarLandingZ};
        state_.needle_bay_floor_position = {kNeedleBayFloorX, kNeedleBayFloorY, kNeedleBayFloorZ};

        const JPH::RVec3 cage_position = bodies.GetPosition(cage_id_);
        const double lever_x = kCageX + kCageLeverLocalX;
        const double lever_y = cage_y_ + kCageLeverLocalY;
        const double lever_z = kCageZ + kCageLeverLocalZ;
        cage_lever_available_ =
            distance_3d(player_position, lever_x, lever_y, lever_z) <= kCageLeverRadiusMeters;
        state_.cage_position = {cage_position.GetX(), cage_position.GetY(), cage_position.GetZ()};
        state_.cage_linear_velocity = {0.0, cage_velocity_y_, 0.0};
        state_.cage_lever_position = {lever_x, lever_y, lever_z};
        state_.cage_upper_landing_position = {kCageUpperX, kCageUpperY, kCageUpperZ};
        state_.cage_lever_available = cage_lever_available_;
        state_.cage_brake_engaged = cage_brake_engaged_;
        state_.cage_stalled = cage_stalled_;
        state_.cage_at_limit = cage_at_limit_;
        state_.cage_command = cage_command_;
    }

    JoltRuntimeLease runtime_;
    JPH::TempAllocatorImpl temp_allocator_;
    JPH::JobSystemThreadPool job_system_;
    BroadPhaseLayerInterface broadphase_layer_interface_;
    ObjectVsBroadPhaseFilter object_vs_broadphase_filter_;
    ObjectLayerPairFilter object_layer_pair_filter_;
    JPH::PhysicsSystem physics_system_;
    PlayerContactListener contact_listener_;

    JPH::BodyID deck_id_;
    JPH::BodyID high_platform_id_;
    JPH::BodyID tower_left_pier_id_;
    JPH::BodyID tower_right_pier_id_;
    JPH::BodyID hopper_chute_id_;
    JPH::BodyID hopper_left_wall_id_;
    JPH::BodyID hopper_right_wall_id_;
    JPH::BodyID vault_block_id_;
    JPH::BodyID mantle_block_id_;
    JPH::BodyID hang_ledge_id_;
    JPH::BodyID translating_support_id_;
    JPH::BodyID rotating_support_id_;
    JPH::BodyID hopper_gate_id_;
    JPH::BodyID hopper_load_id_;
    JPH::BodyID impact_rocker_id_;
    JPH::BodyID player_id_;
    JPH::BodyID jib_mast_id_;
    JPH::BodyID jib_boom_id_;
    JPH::BodyID jib_hook_id_;
    JPH::BodyID jib_crate_id_;
    JPH::BodyID needle_id_;
    JPH::BodyID needle_west_pocket_id_;
    JPH::BodyID needle_east_pocket_id_;
    JPH::BodyID needle_near_landing_id_;
    JPH::BodyID needle_far_landing_id_;
    JPH::BodyID needle_bay_floor_id_;
    JPH::BodyID cage_id_;
    JPH::BodyID cage_upper_landing_id_;
    JPH::BodyID west_stair_ids_[Simulation::kNeedleWestStairCount]{};
    JPH::BodyID east_stair_ids_[Simulation::kNeedleEastStairCount]{};
    JPH::Ref<JPH::DistanceConstraint> hook_constraint_;

    SupportSample support_sample_{};
    JPH::Vec3 airborne_inherited_velocity_{JPH::Vec3::sZero()};
    bool grounded_ = false;
    std::uint64_t support_entity_id_ = 0;
    double rotating_support_yaw_radians_ = 0.0;

    bool hopper_release_started_ = false;
    bool hopper_gate_open_ = false;
    double hopper_gate_x_ = kHopperGateClosedX;

    bool parachute_deployed_ = false;
    double airborne_seconds_ = 0.0;
    double fall_start_y_ = 3.0;
    double grounded_dwell_seconds_ = 0.0;
    float last_airborne_speed_y_ = 0.0F;
    std::uint8_t fall_severity_ = 0;
    std::uint32_t fear_event_id_ = 0;
    bool pending_death_ = false;
    bool checkpoint_committed_ = false;
    bool checkpoint_has_pose_ = false;
    std::uint64_t checkpoint_tick_ = 0;
    Vector3 checkpoint_player_position_{};
    Vector3 checkpoint_player_velocity_{};
    bool checkpoint_hopper_release_ = false;
    double checkpoint_hopper_gate_x_ = kHopperGateClosedX;
    double checkpoint_slew_radians_ = 0.0;
    double checkpoint_winch_length_ = kJibWinchInitialMeters;
    bool checkpoint_jib_brake_ = true;
    bool checkpoint_jib_occupied_ = false;
    Vector3 checkpoint_crate_position_{};
    Vector3 checkpoint_crate_velocity_{};
    bool checkpoint_needle_seated_ = false;
    Vector3 checkpoint_needle_position_{};
    Vector3 checkpoint_needle_velocity_{};
    HookLoad checkpoint_hook_attachment_ = HookLoad::Crate;
    double checkpoint_cage_y_ = kCageMinY;
    double checkpoint_cage_command_ = 0.0;
    bool checkpoint_cage_brake_ = true;

    double crate_mass_kg_ = kJibRatedCrateKilograms;
    double slew_radians_ = 0.0;
    double winch_length_ = kJibWinchInitialMeters;
    double jib_hoist_input_ = 0.0;
    double jib_slew_input_ = 0.0;
    double jib_load_newtons_ = 0.0;
    bool jib_brake_engaged_ = true;
    bool jib_station_occupied_ = false;
    bool jib_station_available_ = false;
    bool jib_stalled_ = false;
    bool jib_at_hoist_limit_ = false;
    bool jib_at_slew_limit_ = false;
    bool jib_enter_requested_ = false;
    bool jib_exit_requested_ = false;
    HookLoad hook_attachment_ = HookLoad::None;
    bool needle_seated_ = false;
    double cage_y_ = kCageMinY;
    double cage_command_ = 0.0;
    double cage_velocity_y_ = 0.0;
    bool cage_brake_engaged_ = true;
    bool cage_stalled_ = false;
    bool cage_at_limit_ = true;
    bool cage_lever_available_ = false;

    TraversalMode traversal_mode_ = TraversalMode::None;
    JPH::RVec3 traversal_target_{JPH::RVec3::sZero()};
    JPH::Vec3 traversal_reference_velocity_{JPH::Vec3::sZero()};
    float traversal_elapsed_seconds_ = 0.0F;
    bool mantle_from_hang_ = false;

    Snapshot state_{};
};

Simulation::Simulation(const InitialSpawn initial_spawn)
    : physics_world_(std::make_unique<PhysicsWorld>(initial_spawn)) {
    snapshot_ = physics_world_->state();
    snapshot_.fixed_step_seconds = kFixedStepSeconds;
}

Simulation::~Simulation() = default;

bool Simulation::set_move_input(double world_x, double world_z) noexcept {
    if (!std::isfinite(world_x) || !std::isfinite(world_z)) {
        return false;
    }

    const double length = std::hypot(world_x, world_z);
    if (length > 1.0) {
        world_x /= length;
        world_z /= length;
    }
    move_input_x_ = world_x;
    move_input_z_ = world_z;
    return true;
}

bool Simulation::request_jump() noexcept {
    jump_requested_ = true;
    return true;
}

bool Simulation::can_traverse() const noexcept {
    return physics_world_->state().traversal_assist_available;
}

bool Simulation::request_traversal() noexcept {
    if (traversal_requested_ || !physics_world_->state().traversal_assist_available) {
        return false;
    }
    traversal_requested_ = true;
    return true;
}

bool Simulation::request_drop_from_hang() noexcept {
    if (drop_from_hang_requested_ ||
        physics_world_->state().traversal_mode != TraversalMode::Hang) {
        return false;
    }
    drop_from_hang_requested_ = true;
    return true;
}

bool Simulation::can_operate_hopper() const noexcept {
    return physics_world_->state().hopper_interaction_available;
}

bool Simulation::request_hopper_release() noexcept {
    if (hopper_release_requested_ || !physics_world_->state().hopper_interaction_available) {
        return false;
    }
    hopper_release_requested_ = true;
    return true;
}

bool Simulation::request_parachute() noexcept {
    const auto &state = physics_world_->state();
    if (parachute_requested_ || state.player_grounded ||
        state.traversal_mode != TraversalMode::None ||
        state.player_position.y <= 3.5) {
        return false;
    }
    parachute_requested_ = true;
    return true;
}

bool Simulation::commit_checkpoint() noexcept {
    if (!physics_world_->commit_checkpoint_public()) {
        return false;
    }
    const Snapshot &world = physics_world_->state();
    snapshot_.checkpoint_committed = world.checkpoint_committed;
    snapshot_.checkpoint_tick = world.checkpoint_tick;
    return true;
}

bool Simulation::can_enter_jib_station() const noexcept {
    return physics_world_->state().jib_station_available;
}

bool Simulation::request_enter_jib_station() noexcept {
    if (jib_enter_requested_ || !physics_world_->state().jib_station_available) {
        return false;
    }
    jib_enter_requested_ = true;
    return true;
}

bool Simulation::request_exit_jib_station() noexcept {
    if (jib_exit_requested_ || !physics_world_->state().jib_station_occupied) {
        return false;
    }
    jib_exit_requested_ = true;
    jib_hoist_input_ = 0.0;
    jib_slew_input_ = 0.0;
    return true;
}

bool Simulation::set_jib_hoist_input(const double hoist) noexcept {
    if (!std::isfinite(hoist) || !physics_world_->state().jib_station_occupied) {
        return false;
    }
    jib_hoist_input_ = clamp_double(hoist, -1.0, 1.0);
    return true;
}

bool Simulation::set_jib_slew_input(const double slew) noexcept {
    if (!std::isfinite(slew) || !physics_world_->state().jib_station_occupied) {
        return false;
    }
    jib_slew_input_ = clamp_double(slew, -1.0, 1.0);
    return true;
}

bool Simulation::set_jib_brake(const bool engaged) noexcept {
    if (!physics_world_->state().jib_station_occupied) {
        return false;
    }
    jib_brake_engaged_ = engaged;
    jib_brake_command_valid_ = true;
    return true;
}

bool Simulation::can_operate_cage() const noexcept {
    return physics_world_->state().cage_lever_available;
}

bool Simulation::request_cage_lever() noexcept {
    if (cage_lever_requested_ || !physics_world_->state().cage_lever_available) {
        return false;
    }
    cage_lever_requested_ = true;
    return true;
}

void Simulation::step_fixed() noexcept {
    const double next_time_seconds =
        static_cast<double>(tick_index_ + 1) * kFixedStepSeconds;
    physics_world_->step(move_input_x_,
                         move_input_z_,
                         jump_requested_,
                         traversal_requested_,
                         drop_from_hang_requested_,
                         hopper_release_requested_,
                         parachute_requested_,
                         jib_enter_requested_,
                         jib_exit_requested_,
                         jib_hoist_input_,
                         jib_slew_input_,
                         jib_brake_engaged_,
                         jib_brake_command_valid_,
                         cage_lever_requested_,
                         static_cast<float>(kFixedStepSeconds),
                         next_time_seconds);
    jump_requested_ = false;
    traversal_requested_ = false;
    drop_from_hang_requested_ = false;
    hopper_release_requested_ = false;
    parachute_requested_ = false;
    jib_enter_requested_ = false;
    jib_exit_requested_ = false;
    jib_brake_command_valid_ = false;
    cage_lever_requested_ = false;
    ++tick_index_;

    snapshot_ = physics_world_->state();
    snapshot_.tick_index = tick_index_;
    snapshot_.simulation_time_seconds =
        static_cast<double>(tick_index_) * kFixedStepSeconds;
    snapshot_.fixed_step_seconds = kFixedStepSeconds;
    if (!snapshot_.jib_station_occupied) {
        jib_hoist_input_ = 0.0;
        jib_slew_input_ = 0.0;
    }
}

AdvanceResult Simulation::advance_frame(const double frame_delta_seconds) noexcept {
    if (!std::isfinite(frame_delta_seconds) || frame_delta_seconds < 0.0 ||
        frame_delta_seconds > kMaximumAcceptedFrameDeltaSeconds) {
        return {};
    }

    const double accumulated = remainder_seconds_ + frame_delta_seconds;
    const double step_epsilon = kFixedStepSeconds * 1.0e-9;
    const double due_as_double = std::floor((accumulated + step_epsilon) / kFixedStepSeconds);

    if (due_as_double < 0.0 ||
        due_as_double > static_cast<double>(std::numeric_limits<std::uint32_t>::max()) ||
        due_as_double > static_cast<double>(std::numeric_limits<std::uint64_t>::max() - tick_index_)) {
        return {};
    }

    auto due = static_cast<std::uint32_t>(due_as_double);
    remainder_seconds_ = accumulated - static_cast<double>(due) * kFixedStepSeconds;

    if (remainder_seconds_ < 0.0 && remainder_seconds_ > -step_epsilon) {
        remainder_seconds_ = 0.0;
    }
    if (remainder_seconds_ >= kFixedStepSeconds &&
        remainder_seconds_ - kFixedStepSeconds < step_epsilon) {
        remainder_seconds_ = 0.0;
        ++due;
    }

    for (std::uint32_t step = 0; step < due; ++step) {
        step_fixed();
    }
    return {true, due};
}

Snapshot Simulation::snapshot() const noexcept {
    Snapshot result = snapshot_;
    result.interpolation_alpha = remainder_seconds_ / kFixedStepSeconds;
    return result;
}

} // namespace scraperx::sim
