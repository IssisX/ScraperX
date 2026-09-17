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
constexpr float kMantleLiftVelocity = 5.5F;
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
    [[nodiscard]] static int support_rank(const std::uint64_t entity_id) noexcept {
        using scraperx::sim::Simulation;
        if (entity_id == Simulation::kTranslatingSupportEntityId ||
            entity_id == Simulation::kRotatingSupportEntityId ||
            entity_id == Simulation::kHopperGateEntityId) {
            return 2;
        }
        if (entity_id == Simulation::kStaticDeckEntityId ||
            entity_id == Simulation::kHopperChuteEntityId ||
            entity_id == Simulation::kTowerLeftPierEntityId ||
            entity_id == Simulation::kTowerRightPierEntityId ||
            entity_id == Simulation::kHopperLeftWallEntityId ||
            entity_id == Simulation::kHopperRightWallEntityId ||
            entity_id == Simulation::kVaultBlockEntityId ||
            entity_id == Simulation::kMantleBlockEntityId ||
            entity_id == Simulation::kHangLedgeEntityId) {
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

        if (support_body == nullptr || support_normal_y < kSupportNormalThreshold ||
            support_rank(support_entity) == 0) {
            return;
        }

        const JPH::Vec3 point_velocity = support_body->GetPointVelocity(support_contact_point);
        const SupportSample candidate{
            true,
            support_entity,
            {support_contact_point.GetX(), support_contact_point.GetY(), support_contact_point.GetZ()},
            {point_velocity.GetX(), point_velocity.GetY(), point_velocity.GetZ()},
            support_normal_y,
        };

        lock();
        const int current_rank = support_rank(sample_.entity_id);
        const int candidate_rank = support_rank(candidate.entity_id);
        if (!sample_.grounded || candidate_rank > current_rank ||
            (candidate_rank == current_rank && candidate.normal_y > sample_.normal_y)) {
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
            new JPH::CapsuleShape(0.55F, 0.35F),
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

        physics_system_.OptimizeBroadPhase();
        read_state();
    }

    ~PhysicsWorld() {
        physics_system_.SetContactListener(nullptr);
        auto &bodies = physics_system_.GetBodyInterface();
        remove_and_destroy(bodies, player_id_);
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
        remove_and_destroy(bodies, deck_id_);
    }

    void step(const double move_input_x,
              const double move_input_z,
              const bool jump_requested,
              const bool traversal_requested,
              const bool drop_from_hang_requested,
              const bool hopper_release_requested,
              const float delta_seconds,
              const double next_time_seconds) noexcept {
        auto &bodies = physics_system_.GetBodyInterface();

        if (hopper_release_requested && !hopper_release_started_) {
            hopper_release_started_ = true;
        }

        update_support_motion(bodies, delta_seconds, next_time_seconds);
        update_hopper_motion(bodies, delta_seconds);

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

        contact_listener_.begin_tick();
        physics_system_.Update(delta_seconds, 1, &temp_allocator_, &job_system_);

        SupportSample support = contact_listener_.sample();
        if (jump_started || traversal_mode_ == TraversalMode::Hang) {
            support = {};
        }
        support_sample_ = support;
        grounded_ = support.grounded;
        support_entity_id_ = support.entity_id;

        advance_traversal_after_physics(delta_seconds);
        read_state();
    }

    [[nodiscard]] const Snapshot &state() const noexcept {
        return state_;
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
            std::min(kTraversalLaneX + 1.65, player_position.GetX()));
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
            std::min(kTraversalLaneX + 1.75, player_position.GetX()));
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
        if (traversal_elapsed_seconds_ < lift_seconds) {
            player_velocity.SetY(traversal_reference_velocity_.GetY() +
                                 (is_vault ? kVaultLiftVelocity : kMantleLiftVelocity));
        }
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

    SupportSample support_sample_{};
    JPH::Vec3 airborne_inherited_velocity_{JPH::Vec3::sZero()};
    bool grounded_ = false;
    std::uint64_t support_entity_id_ = 0;
    double rotating_support_yaw_radians_ = 0.0;

    bool hopper_release_started_ = false;
    bool hopper_gate_open_ = false;
    double hopper_gate_x_ = kHopperGateClosedX;

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

void Simulation::step_fixed() noexcept {
    const double next_time_seconds =
        static_cast<double>(tick_index_ + 1) * kFixedStepSeconds;
    physics_world_->step(move_input_x_,
                         move_input_z_,
                         jump_requested_,
                         traversal_requested_,
                         drop_from_hang_requested_,
                         hopper_release_requested_,
                         static_cast<float>(kFixedStepSeconds),
                         next_time_seconds);
    jump_requested_ = false;
    traversal_requested_ = false;
    drop_from_hang_requested_ = false;
    hopper_release_requested_ = false;
    ++tick_index_;

    snapshot_ = physics_world_->state();
    snapshot_.tick_index = tick_index_;
    snapshot_.simulation_time_seconds =
        static_cast<double>(tick_index_) * kFixedStepSeconds;
    snapshot_.fixed_step_seconds = kFixedStepSeconds;
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
