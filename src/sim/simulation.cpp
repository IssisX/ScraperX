#include "sim/simulation.hpp"

#ifndef SCRAPERX_HAS_JOLT
#error "WO-003 requires the pinned Jolt physics substrate"
#endif

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
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

constexpr double kPi = 3.14159265358979323846;

constexpr float kSupportNormalThreshold = 0.55F;
constexpr float kPlayerMaximumRelativeSpeed = 5.5F;
constexpr float kGroundAcceleration = 22.0F;
constexpr float kAirAcceleration = 8.0F;
constexpr float kJumpSpeed = 5.5F;
constexpr double kTranslatingSupportAmplitudeMeters = 2.0;
constexpr double kTranslatingSupportAngularFrequency = 1.0;
constexpr double kRotatingSupportAngularSpeed = 0.8;
constexpr double kMovingLedgeAmplitudeMeters = 1.5;
constexpr double kMovingLedgeAngularFrequency = 0.9;
constexpr double kMovingLedgeCenterZ = 12.5;

// Player capsule: cylinder half-height 0.55 plus radius 0.35.
constexpr float kPlayerRadius = 0.35F;
constexpr float kPlayerHalfHeight = 0.9F;

// Traversal reach / clearance rules. Every one of these is a bound on what the
// native assist may attempt; none of them fabricates geometry.
constexpr float kTraversalReach = 0.95F;
constexpr float kTopProbeInset = 0.12F;
constexpr float kLandingInset = kPlayerRadius + 0.12F;
constexpr float kTopProbeMargin = 0.35F;
constexpr float kLedgeTopNormalThreshold = 0.7F;
constexpr float kLandingSkin = 0.02F;
constexpr float kLandingSupportProbeUp = 0.12F;
constexpr float kLandingSupportTolerance = 0.10F;

constexpr float kMantleMinimumRise = 0.35F;
constexpr float kMantleMaximumRise = 1.85F;
constexpr float kMantleClearanceLift = 0.12F;
constexpr double kMantleDurationSeconds = 0.42;

constexpr float kVaultMinimumRise = 0.35F;
constexpr float kVaultMaximumRise = 1.15F;
constexpr float kVaultCrossDistance = 1.30F;
constexpr float kVaultApexClearance = 0.10F;
constexpr float kVaultMaximumDrop = 1.40F;
constexpr double kVaultDurationSeconds = 0.38;

// Hang band expressed against the capsule centre: hands reach a ledge between
// chest height and just above the head.
constexpr float kHangMinimumRiseAboveCentre = 0.45F;
constexpr float kHangMaximumRiseAboveCentre = 1.35F;
constexpr float kHangDropBelowLedge = 1.05F;
constexpr float kHangWallGap = 0.06F;
constexpr float kHangMaximumClimbSpeed = 0.2F;
constexpr double kHangIntentDotThreshold = 0.3;

// After a deliberate release the controller stops offering an automatic re-grab
// for a moment, so letting go is a real decision rather than an instant re-hang.
constexpr std::uint32_t kReleaseRegrabLockoutTicks = 27;

constexpr float kTraversalStallTolerance = 0.22F;
constexpr std::uint32_t kTraversalStallAbortTicks = 12;

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

[[nodiscard]] bool entity_is_kinematic_support(const std::uint64_t entity_id) noexcept {
    return entity_id == scraperx::sim::Simulation::kTranslatingSupportEntityId ||
           entity_id == scraperx::sim::Simulation::kRotatingSupportEntityId ||
           entity_id == scraperx::sim::Simulation::kMovingLedgeEntityId;
}

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
        if (entity_id == 0 || entity_id == scraperx::sim::Simulation::kPlayerEntityId) {
            return 0;
        }
        return entity_is_kinematic_support(entity_id) ? 2 : 1;
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
    case scraperx::sim::InitialSpawn::RotatingSupport:
        return {-6.5, 3.0, 0.0};
    case scraperx::sim::InitialSpawn::VaultApproach:
        return {1.2, 1.2, -6.0};
    case scraperx::sim::InitialSpawn::MantleApproach:
        return {7.9, 1.2, -6.0};
    case scraperx::sim::InitialSpawn::HangApproach:
        return {7.6, 4.2, 4.0};
    case scraperx::sim::InitialSpawn::MovingLedgeApproach:
        return {6.1, 4.2, 12.0};
    case scraperx::sim::InitialSpawn::BlockedLedgeApproach:
        return {-3.6, 1.2, -8.0};
    case scraperx::sim::InitialSpawn::TranslatingSupport:
    default:
        return {0.0, 3.0, 8.0};
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

[[nodiscard]] float smoothstep(const float edge0, const float edge1, const float value) noexcept {
    if (edge1 <= edge0) {
        return value < edge0 ? 0.0F : 1.0F;
    }
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

[[nodiscard]] scraperx::sim::Vector3 to_vector3(const JPH::RVec3 value) noexcept {
    return {value.GetX(), value.GetY(), value.GetZ()};
}

// Result of one geometry probe against the authoritative Jolt world. Every
// field is derived from an actual cast or clearance query.
struct LedgeProbe final {
    bool valid = false;
    JPH::BodyID ledge_body;
    std::uint64_t ledge_entity_id = 0;
    JPH::RVec3 wall_point{};
    JPH::RVec3 ledge_point{};
    JPH::RVec3 landing_centre{};
    JPH::BodyID landing_body;
    std::uint64_t landing_entity_id = 0;
    float rise = 0.0F;
};

} // namespace

namespace scraperx::sim {

class Simulation::PhysicsWorld final {
public:
    explicit PhysicsWorld(const InitialSpawn initial_spawn)
        : temp_allocator_(8U * 1024U * 1024U),
          job_system_(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, 1) {
        physics_system_.Init(256,
                             0,
                             512,
                             256,
                             broadphase_layer_interface_,
                             object_vs_broadphase_filter_,
                             object_layer_pair_filter_);
        physics_system_.SetContactListener(&contact_listener_);

        auto &bodies = physics_system_.GetBodyInterface();

        deck_id_ = add_box(bodies,
                           JPH::Vec3(16.0F, 0.5F, 16.0F),
                           JPH::RVec3(0.0, -0.5, 0.0),
                           JPH::EMotionType::Static,
                           object_layers::kStatic,
                           0.6F,
                           Simulation::kStaticDeckEntityId);

        translating_support_id_ = add_box(bodies,
                                          JPH::Vec3(2.75F, 0.25F, 2.75F),
                                          JPH::RVec3(0.0, 0.25, 8.0),
                                          JPH::EMotionType::Kinematic,
                                          object_layers::kMoving,
                                          0.8F,
                                          Simulation::kTranslatingSupportEntityId);

        rotating_support_id_ = add_box(bodies,
                                       JPH::Vec3(3.0F, 0.25F, 3.0F),
                                       JPH::RVec3(-8.0, 0.25, 0.0),
                                       JPH::EMotionType::Kinematic,
                                       object_layers::kMoving,
                                       0.8F,
                                       Simulation::kRotatingSupportEntityId);

        vault_rail_id_ = add_box(bodies,
                                 JPH::Vec3(0.22F, 0.475F, 2.5F),
                                 JPH::RVec3(5.0, 0.475, -6.0),
                                 JPH::EMotionType::Static,
                                 object_layers::kStatic,
                                 0.7F,
                                 Simulation::kVaultRailEntityId);

        mantle_ledge_id_ = add_box(bodies,
                                   JPH::Vec3(2.0F, 0.775F, 2.0F),
                                   JPH::RVec3(11.0, 0.775, -6.0),
                                   JPH::EMotionType::Static,
                                   object_layers::kStatic,
                                   0.7F,
                                   Simulation::kMantleLedgeEntityId);

        hang_ledge_id_ = add_box(bodies,
                                 JPH::Vec3(2.5F, 1.8F, 2.5F),
                                 JPH::RVec3(11.0, 1.8, 4.0),
                                 JPH::EMotionType::Static,
                                 object_layers::kStatic,
                                 0.7F,
                                 Simulation::kHangLedgeEntityId);

        moving_ledge_id_ = add_box(bodies,
                                   JPH::Vec3(2.0F, 1.8F, 2.0F),
                                   JPH::RVec3(9.0, 1.8, kMovingLedgeCenterZ),
                                   JPH::EMotionType::Kinematic,
                                   object_layers::kMoving,
                                   0.8F,
                                   Simulation::kMovingLedgeEntityId);

        blocked_ledge_id_ = add_box(bodies,
                                    JPH::Vec3(1.5F, 0.775F, 1.5F),
                                    JPH::RVec3(-6.0, 0.775, -8.0),
                                    JPH::EMotionType::Static,
                                    object_layers::kStatic,
                                    0.7F,
                                    Simulation::kBlockedLedgeEntityId);

        blocked_ledge_canopy_id_ = add_box(bodies,
                                           JPH::Vec3(2.2F, 0.15F, 2.2F),
                                           JPH::RVec3(-6.0, 2.7, -8.0),
                                           JPH::EMotionType::Static,
                                           object_layers::kStatic,
                                           0.7F,
                                           Simulation::kBlockedLedgeCanopyEntityId);

        player_shape_ = new JPH::CapsuleShape(0.55F, kPlayerRadius);
        JPH::BodyCreationSettings player_settings(player_shape_,
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
        remove_and_destroy(bodies, blocked_ledge_canopy_id_);
        remove_and_destroy(bodies, blocked_ledge_id_);
        remove_and_destroy(bodies, moving_ledge_id_);
        remove_and_destroy(bodies, hang_ledge_id_);
        remove_and_destroy(bodies, mantle_ledge_id_);
        remove_and_destroy(bodies, vault_rail_id_);
        remove_and_destroy(bodies, rotating_support_id_);
        remove_and_destroy(bodies, translating_support_id_);
        remove_and_destroy(bodies, deck_id_);
    }

    struct StepCommands final {
        double move_input_x = 0.0;
        double move_input_z = 0.0;
        double facing_x = 0.0;
        double facing_z = 0.0;
        bool jump_requested = false;
        bool traversal_requested = false;
        bool release_requested = false;
    };

    void step(const StepCommands &commands,
              const float delta_seconds,
              const double next_time_seconds) noexcept {
        auto &bodies = physics_system_.GetBodyInterface();
        update_support_motion(bodies, delta_seconds, next_time_seconds);

        if (regrab_lockout_ticks_ > 0) {
            --regrab_lockout_ticks_;
        }
        facing_ = normalized_horizontal(commands.facing_x, commands.facing_z);

        apply_traversal_commands(bodies, commands);

        bool jump_started = false;
        if (traversal_state_ == TraversalState::None) {
            jump_started = apply_locomotion(bodies, commands, delta_seconds);
            if (!jump_started) {
                try_begin_hang(bodies, commands);
            }
        }

        if (traversal_state_ != TraversalState::None) {
            drive_traversal(bodies, delta_seconds);
        }

        contact_listener_.begin_tick();
        physics_system_.Update(delta_seconds, 1, &temp_allocator_, &job_system_);

        SupportSample support = contact_listener_.sample();
        if (jump_started || traversal_state_ != TraversalState::None) {
            support = {};
        }
        support_sample_ = support;
        grounded_ = support.grounded;
        support_entity_id_ = support.entity_id;

        if (traversal_state_ != TraversalState::None) {
            resolve_traversal_outcome(bodies);
        }

        update_affordance(bodies);
        read_state();
    }

    [[nodiscard]] const Snapshot &state() const noexcept {
        return state_;
    }

    [[nodiscard]] bool traversal_committed() const noexcept {
        return traversal_state_ != TraversalState::None;
    }

private:
    static void remove_and_destroy(JPH::BodyInterface &bodies, const JPH::BodyID body_id) {
        bodies.RemoveBody(body_id);
        bodies.DestroyBody(body_id);
    }

    static JPH::BodyID add_box(JPH::BodyInterface &bodies,
                               const JPH::Vec3 half_extent,
                               const JPH::RVec3 position,
                               const JPH::EMotionType motion_type,
                               const JPH::ObjectLayer layer,
                               const float friction,
                               const std::uint64_t entity_id) {
        JPH::BodyCreationSettings settings(new JPH::BoxShape(half_extent),
                                           position,
                                           JPH::Quat::sIdentity(),
                                           motion_type,
                                           layer);
        settings.mFriction = friction;
        settings.mUserData = entity_id;
        settings.mAllowSleeping = motion_type == JPH::EMotionType::Static;
        const JPH::EActivation activation = motion_type == JPH::EMotionType::Static
                                                ? JPH::EActivation::DontActivate
                                                : JPH::EActivation::Activate;
        return bodies.CreateAndAddBody(settings, activation);
    }

    [[nodiscard]] static JPH::Vec3 normalized_horizontal(const double x, const double z) noexcept {
        const double length = std::hypot(x, z);
        if (!(length > 1.0e-6)) {
            return JPH::Vec3::sZero();
        }
        return JPH::Vec3(static_cast<float>(x / length), 0.0F, static_cast<float>(z / length));
    }

    [[nodiscard]] JPH::BodyID body_id_for_entity(const std::uint64_t entity_id) const noexcept {
        switch (entity_id) {
        case Simulation::kStaticDeckEntityId:
            return deck_id_;
        case Simulation::kTranslatingSupportEntityId:
            return translating_support_id_;
        case Simulation::kRotatingSupportEntityId:
            return rotating_support_id_;
        case Simulation::kVaultRailEntityId:
            return vault_rail_id_;
        case Simulation::kMantleLedgeEntityId:
            return mantle_ledge_id_;
        case Simulation::kHangLedgeEntityId:
            return hang_ledge_id_;
        case Simulation::kMovingLedgeEntityId:
            return moving_ledge_id_;
        case Simulation::kBlockedLedgeEntityId:
            return blocked_ledge_id_;
        case Simulation::kBlockedLedgeCanopyEntityId:
            return blocked_ledge_canopy_id_;
        default:
            return {};
        }
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
            std::fmod(kRotatingSupportAngularSpeed * next_time_seconds, 2.0 * kPi);
        bodies.MoveKinematic(
            rotating_support_id_,
            JPH::RVec3(-8.0, 0.25, 0.0),
            JPH::Quat::sRotation(JPH::Vec3(0.0F, 1.0F, 0.0F),
                                 static_cast<float>(rotating_support_yaw_radians_)),
            delta_seconds);

        const double moving_ledge_z =
            kMovingLedgeCenterZ +
            kMovingLedgeAmplitudeMeters *
                std::sin(kMovingLedgeAngularFrequency * next_time_seconds);
        bodies.MoveKinematic(moving_ledge_id_,
                             JPH::RVec3(9.0, 1.8, moving_ledge_z),
                             JPH::Quat::sIdentity(),
                             delta_seconds);
    }

    // ---- geometry probes -------------------------------------------------

    [[nodiscard]] bool cast_ray(const JPH::RVec3 origin,
                                const JPH::Vec3 direction,
                                JPH::RayCastResult &hit) const noexcept {
        const JPH::RRayCast ray(origin, direction);
        hit.Reset();
        const JPH::IgnoreSingleBodyFilter body_filter(player_id_);
        return physics_system_.GetNarrowPhaseQuery().CastRay(ray, hit, {}, {}, body_filter);
    }

    [[nodiscard]] JPH::Vec3 surface_normal(const JPH::BodyID body_id,
                                           const JPH::SubShapeID &sub_shape_id,
                                           const JPH::RVec3 point) const noexcept {
        const JPH::BodyLockRead lock(physics_system_.GetBodyLockInterfaceNoLock(), body_id);
        if (!lock.Succeeded()) {
            return JPH::Vec3::sZero();
        }
        return lock.GetBody().GetWorldSpaceSurfaceNormal(sub_shape_id, point);
    }

    [[nodiscard]] bool capsule_pose_is_clear(const JPH::RVec3 centre) const noexcept {
        JPH::AnyHitCollisionCollector<JPH::CollideShapeCollector> collector;
        JPH::CollideShapeSettings settings;
        settings.mMaxSeparationDistance = 0.0F;
        const JPH::IgnoreSingleBodyFilter body_filter(player_id_);
        physics_system_.GetNarrowPhaseQuery().CollideShape(player_shape_,
                                                           JPH::Vec3::sReplicate(1.0F),
                                                           JPH::RMat44::sTranslation(centre),
                                                           settings,
                                                           centre,
                                                           collector,
                                                           {},
                                                           {},
                                                           body_filter);
        return !collector.HadHit();
    }

    // Finds a ledge in front of `origin`. Every returned field comes from a real
    // cast against the authoritative world; a failed reach, a missing top
    // surface, a too-steep top, an out-of-band rise, an unsupported landing, or
    // an obstructed landing pose all return an invalid probe.
    [[nodiscard]] LedgeProbe probe_ledge(const JPH::RVec3 origin,
                                         const JPH::Vec3 facing,
                                         const float minimum_rise,
                                         const float maximum_rise,
                                         const bool require_supported_landing) const noexcept {
        LedgeProbe probe;
        if (facing.IsNearZero()) {
            return probe;
        }

        JPH::RayCastResult wall_hit;
        if (!cast_ray(origin, facing * (kTraversalReach + kPlayerRadius), wall_hit)) {
            return probe;
        }
        const JPH::RVec3 wall_point =
            JPH::RRayCast(origin, facing * (kTraversalReach + kPlayerRadius))
                .GetPointOnRay(wall_hit.mFraction);

        const float feet_y = origin.GetY() - kPlayerHalfHeight;
        const JPH::RVec3 top_origin(wall_point.GetX() + facing.GetX() * kTopProbeInset,
                                    feet_y + maximum_rise + kTopProbeMargin,
                                    wall_point.GetZ() + facing.GetZ() * kTopProbeInset);
        const float top_ray_length = maximum_rise + kTopProbeMargin - minimum_rise;
        if (top_ray_length <= 0.0F) {
            return probe;
        }
        const JPH::Vec3 top_direction(0.0F, -top_ray_length, 0.0F);

        JPH::RayCastResult top_hit;
        if (!cast_ray(top_origin, top_direction, top_hit)) {
            return probe;
        }
        if (top_hit.mBodyID != wall_hit.mBodyID) {
            return probe;
        }
        const JPH::RVec3 ledge_point =
            JPH::RRayCast(top_origin, top_direction).GetPointOnRay(top_hit.mFraction);
        if (surface_normal(top_hit.mBodyID, top_hit.mSubShapeID2, ledge_point).GetY() <
            kLedgeTopNormalThreshold) {
            return probe;
        }

        const float rise = ledge_point.GetY() - feet_y;
        if (rise < minimum_rise || rise > maximum_rise) {
            return probe;
        }

        const JPH::RVec3 landing_centre(wall_point.GetX() + facing.GetX() * kLandingInset,
                                        ledge_point.GetY() + kPlayerHalfHeight + kLandingSkin,
                                        wall_point.GetZ() + facing.GetZ() * kLandingInset);

        JPH::BodyID landing_body = top_hit.mBodyID;
        if (require_supported_landing) {
            const JPH::RVec3 support_origin(landing_centre.GetX(),
                                            ledge_point.GetY() + kLandingSupportProbeUp,
                                            landing_centre.GetZ());
            const JPH::Vec3 support_direction(
                0.0F, -(kLandingSupportProbeUp + kLandingSupportTolerance), 0.0F);
            JPH::RayCastResult landing_hit;
            if (!cast_ray(support_origin, support_direction, landing_hit)) {
                return probe;
            }
            if (landing_hit.mBodyID != top_hit.mBodyID) {
                return probe;
            }
            landing_body = landing_hit.mBodyID;
        }

        if (!capsule_pose_is_clear(landing_centre)) {
            return probe;
        }

        const auto &bodies = physics_system_.GetBodyInterface();
        probe.valid = true;
        probe.ledge_body = top_hit.mBodyID;
        probe.ledge_entity_id = bodies.GetUserData(top_hit.mBodyID);
        probe.wall_point = wall_point;
        probe.ledge_point = ledge_point;
        probe.landing_centre = landing_centre;
        probe.landing_body = landing_body;
        probe.landing_entity_id = bodies.GetUserData(landing_body);
        probe.rise = rise;
        return probe;
    }

    // Finds the far-side landing that makes an obstacle vaultable rather than
    // mantleable. Without a real, clear landing beyond the obstacle there is no
    // vault.
    [[nodiscard]] bool probe_vault_landing(const LedgeProbe &obstacle,
                                           const JPH::Vec3 facing,
                                           const float feet_y,
                                           JPH::RVec3 &landing_centre,
                                           JPH::BodyID &landing_body) const noexcept {
        const JPH::RVec3 far_origin(
            obstacle.wall_point.GetX() + facing.GetX() * kVaultCrossDistance,
            obstacle.ledge_point.GetY() + 0.40F,
            obstacle.wall_point.GetZ() + facing.GetZ() * kVaultCrossDistance);
        const float drop_length = obstacle.ledge_point.GetY() + 0.40F - (feet_y - kVaultMaximumDrop);
        if (drop_length <= 0.0F) {
            return false;
        }
        const JPH::Vec3 drop_direction(0.0F, -drop_length, 0.0F);

        JPH::RayCastResult landing_hit;
        if (!cast_ray(far_origin, drop_direction, landing_hit)) {
            return false;
        }
        if (landing_hit.mBodyID == obstacle.ledge_body) {
            return false;
        }
        const JPH::RVec3 landing_point =
            JPH::RRayCast(far_origin, drop_direction).GetPointOnRay(landing_hit.mFraction);
        if (surface_normal(landing_hit.mBodyID, landing_hit.mSubShapeID2, landing_point).GetY() <
            kLedgeTopNormalThreshold) {
            return false;
        }

        const JPH::RVec3 candidate(landing_point.GetX(),
                                   landing_point.GetY() + kPlayerHalfHeight + kLandingSkin,
                                   landing_point.GetZ());
        if (!capsule_pose_is_clear(candidate)) {
            return false;
        }

        landing_centre = candidate;
        landing_body = landing_hit.mBodyID;
        return true;
    }

    // ---- support-frame helpers ------------------------------------------

    [[nodiscard]] JPH::Vec3 to_support_local(const JPH::BodyInterface &bodies,
                                             const JPH::BodyID body_id,
                                             const JPH::RVec3 world_point) const noexcept {
        if (body_id.IsInvalid()) {
            return JPH::Vec3(world_point);
        }
        return JPH::Vec3(bodies.GetCenterOfMassTransform(body_id).Inversed() * world_point);
    }

    [[nodiscard]] JPH::RVec3 from_support_local(const JPH::BodyInterface &bodies,
                                                const JPH::BodyID body_id,
                                                const JPH::Vec3 local_point) const noexcept {
        if (body_id.IsInvalid()) {
            return JPH::RVec3(local_point);
        }
        return bodies.GetCenterOfMassTransform(body_id) * JPH::RVec3(local_point);
    }

    // ---- traversal state machine ----------------------------------------

    void apply_traversal_commands(JPH::BodyInterface &bodies,
                                  const StepCommands &commands) noexcept {
        if (traversal_state_ == TraversalState::Hanging) {
            if (commands.release_requested) {
                release_hang(bodies);
            } else if (commands.jump_requested || commands.traversal_requested) {
                begin_mantle_from_hang(bodies);
            }
            return;
        }

        if (traversal_state_ != TraversalState::None) {
            if (commands.traversal_requested || commands.release_requested) {
                ++rejected_traversal_count_;
            }
            return;
        }

        if (commands.traversal_requested && !try_begin_ground_traversal(bodies)) {
            ++rejected_traversal_count_;
        }
    }

    [[nodiscard]] bool apply_locomotion(JPH::BodyInterface &bodies,
                                        const StepCommands &commands,
                                        const float delta_seconds) noexcept {
        JPH::Vec3 player_velocity = bodies.GetLinearVelocity(player_id_);
        JPH::Vec3 reference_velocity = airborne_inherited_velocity_;

        if (grounded_ && support_entity_id_ != 0) {
            reference_velocity = current_support_point_velocity(bodies);
            airborne_inherited_velocity_ = reference_velocity;
            approach_relative_horizontal_velocity(player_velocity,
                                                  reference_velocity,
                                                  commands.move_input_x,
                                                  commands.move_input_z,
                                                  kGroundAcceleration,
                                                  delta_seconds);
        } else {
            approach_relative_horizontal_velocity(player_velocity,
                                                  reference_velocity,
                                                  commands.move_input_x,
                                                  commands.move_input_z,
                                                  kAirAcceleration,
                                                  delta_seconds);
        }

        const bool jump_started = commands.jump_requested && grounded_;
        if (jump_started) {
            player_velocity.SetY(reference_velocity.GetY() + kJumpSpeed);
        }
        bodies.SetLinearVelocity(player_id_, player_velocity);
        return jump_started;
    }

    void try_begin_hang(JPH::BodyInterface &bodies, const StepCommands &commands) noexcept {
        if (grounded_ || regrab_lockout_ticks_ > 0) {
            return;
        }
        if (bodies.GetLinearVelocity(player_id_).GetY() > kHangMaximumClimbSpeed) {
            return;
        }
        if (facing_.IsNearZero()) {
            return;
        }
        const double intent = commands.move_input_x * static_cast<double>(facing_.GetX()) +
                              commands.move_input_z * static_cast<double>(facing_.GetZ());
        if (intent < kHangIntentDotThreshold) {
            return;
        }

        const JPH::RVec3 origin = bodies.GetPosition(player_id_);
        const LedgeProbe probe = probe_ledge(origin,
                                             facing_,
                                             kPlayerHalfHeight + kHangMinimumRiseAboveCentre,
                                             kPlayerHalfHeight + kHangMaximumRiseAboveCentre,
                                             true);
        if (!probe.valid) {
            return;
        }

        const JPH::RVec3 hold(probe.wall_point.GetX() - facing_.GetX() * (kPlayerRadius + kHangWallGap),
                              probe.ledge_point.GetY() - kHangDropBelowLedge,
                              probe.wall_point.GetZ() - facing_.GetZ() * (kPlayerRadius + kHangWallGap));

        traversal_state_ = TraversalState::Hanging;
        traversal_body_ = probe.ledge_body;
        traversal_entity_id_ = probe.ledge_entity_id;
        traversal_target_body_ = probe.landing_body;
        traversal_local_hold_ = to_support_local(bodies, traversal_body_, hold);
        traversal_local_ledge_ = to_support_local(bodies, traversal_body_, probe.ledge_point);
        traversal_local_target_ =
            to_support_local(bodies, traversal_target_body_, probe.landing_centre);
        traversal_progress_ = 0.0;
        traversal_stall_ticks_ = 0;
        traversal_desired_ = hold;
        bodies.SetGravityFactor(player_id_, 0.0F);
    }

    [[nodiscard]] bool try_begin_ground_traversal(JPH::BodyInterface &bodies) noexcept {
        if (!grounded_ || facing_.IsNearZero()) {
            return false;
        }

        const JPH::RVec3 origin = bodies.GetPosition(player_id_);
        const float feet_y = origin.GetY() - kPlayerHalfHeight;

        const LedgeProbe vault_probe =
            probe_ledge(origin, facing_, kVaultMinimumRise, kVaultMaximumRise, false);
        if (vault_probe.valid) {
            JPH::RVec3 landing_centre;
            JPH::BodyID landing_body;
            if (probe_vault_landing(vault_probe, facing_, feet_y, landing_centre, landing_body)) {
                begin_vault(bodies, vault_probe, landing_centre, landing_body, origin);
                return true;
            }
        }

        const LedgeProbe mantle_probe =
            probe_ledge(origin, facing_, kMantleMinimumRise, kMantleMaximumRise, true);
        if (!mantle_probe.valid) {
            return false;
        }
        begin_mantle(bodies, mantle_probe, origin);
        return true;
    }

    void begin_mantle(JPH::BodyInterface &bodies,
                      const LedgeProbe &probe,
                      const JPH::RVec3 origin) noexcept {
        traversal_state_ = TraversalState::Mantling;
        traversal_body_ = probe.ledge_body;
        traversal_entity_id_ = probe.ledge_entity_id;
        traversal_target_body_ = probe.landing_body;
        traversal_local_start_ = to_support_local(bodies, traversal_body_, origin);
        traversal_local_ledge_ = to_support_local(bodies, traversal_body_, probe.ledge_point);
        traversal_local_target_ =
            to_support_local(bodies, traversal_target_body_, probe.landing_centre);
        traversal_progress_ = 0.0;
        traversal_duration_ = kMantleDurationSeconds;
        traversal_stall_ticks_ = 0;
        traversal_desired_ = origin;
        traversal_exit_relative_velocity_ = JPH::Vec3::sZero();
        bodies.SetGravityFactor(player_id_, 0.0F);
    }

    void begin_mantle_from_hang(JPH::BodyInterface &bodies) noexcept {
        const JPH::RVec3 origin = bodies.GetPosition(player_id_);
        traversal_state_ = TraversalState::Mantling;
        traversal_local_start_ = to_support_local(bodies, traversal_body_, origin);
        traversal_progress_ = 0.0;
        traversal_duration_ = kMantleDurationSeconds;
        traversal_stall_ticks_ = 0;
        traversal_desired_ = origin;
        traversal_exit_relative_velocity_ = JPH::Vec3::sZero();
        bodies.SetGravityFactor(player_id_, 0.0F);
    }

    void begin_vault(JPH::BodyInterface &bodies,
                     const LedgeProbe &probe,
                     const JPH::RVec3 landing_centre,
                     const JPH::BodyID landing_body,
                     const JPH::RVec3 origin) noexcept {
        const JPH::Vec3 player_velocity = bodies.GetLinearVelocity(player_id_);
        const JPH::Vec3 support_velocity =
            grounded_ ? current_support_point_velocity(bodies) : airborne_inherited_velocity_;
        JPH::Vec3 relative(player_velocity.GetX() - support_velocity.GetX(),
                           0.0F,
                           player_velocity.GetZ() - support_velocity.GetZ());
        const float relative_speed = relative.Length();
        if (relative_speed > kPlayerMaximumRelativeSpeed) {
            relative = relative * (kPlayerMaximumRelativeSpeed / relative_speed);
        }

        const JPH::RVec3 apex(origin.GetX(),
                              probe.ledge_point.GetY() + kPlayerHalfHeight + kVaultApexClearance,
                              origin.GetZ());

        traversal_state_ = TraversalState::Vaulting;
        traversal_body_ = probe.ledge_body;
        traversal_entity_id_ = probe.ledge_entity_id;
        traversal_target_body_ = landing_body;
        traversal_local_start_ = to_support_local(bodies, traversal_body_, origin);
        traversal_local_ledge_ = to_support_local(bodies, traversal_body_, probe.ledge_point);
        traversal_local_apex_ = to_support_local(bodies, traversal_body_, apex);
        traversal_local_target_ = to_support_local(bodies, traversal_target_body_, landing_centre);
        traversal_progress_ = 0.0;
        traversal_duration_ = kVaultDurationSeconds;
        traversal_stall_ticks_ = 0;
        traversal_desired_ = origin;
        traversal_exit_relative_velocity_ = relative;
        bodies.SetGravityFactor(player_id_, 0.0F);
    }

    [[nodiscard]] JPH::RVec3 traversal_point(const JPH::BodyInterface &bodies,
                                             const float progress) const noexcept {
        const JPH::RVec3 start = from_support_local(bodies, traversal_body_, traversal_local_start_);
        const JPH::RVec3 target =
            from_support_local(bodies, traversal_target_body_, traversal_local_target_);

        if (traversal_state_ == TraversalState::Vaulting) {
            const JPH::RVec3 apex = from_support_local(bodies, traversal_body_, traversal_local_apex_);
            const float horizontal = progress;
            float height;
            if (progress < 0.5F) {
                height = start.GetY() +
                         (apex.GetY() - start.GetY()) * smoothstep(0.0F, 1.0F, progress * 2.0F);
            } else {
                height = apex.GetY() + (target.GetY() - apex.GetY()) *
                                           smoothstep(0.0F, 1.0F, (progress - 0.5F) * 2.0F);
            }
            return JPH::RVec3(start.GetX() + (target.GetX() - start.GetX()) * horizontal,
                              height,
                              start.GetZ() + (target.GetZ() - start.GetZ()) * horizontal);
        }

        const float vertical = smoothstep(0.0F, 0.55F, progress);
        const float horizontal = smoothstep(0.45F, 1.0F, progress);
        const float lift =
            kMantleClearanceLift * std::sin(static_cast<float>(kPi) * std::clamp(progress, 0.0F, 1.0F));
        return JPH::RVec3(start.GetX() + (target.GetX() - start.GetX()) * horizontal,
                          start.GetY() + (target.GetY() - start.GetY()) * vertical + lift,
                          start.GetZ() + (target.GetZ() - start.GetZ()) * horizontal);
    }

    void drive_traversal(JPH::BodyInterface &bodies, const float delta_seconds) noexcept {
        const JPH::RVec3 current = bodies.GetPosition(player_id_);

        if (traversal_state_ == TraversalState::Hanging) {
            traversal_desired_ = from_support_local(bodies, traversal_body_, traversal_local_hold_);
        } else {
            traversal_progress_ =
                std::min(1.0, traversal_progress_ + static_cast<double>(delta_seconds) /
                                                        traversal_duration_);
            traversal_desired_ = traversal_point(bodies, static_cast<float>(traversal_progress_));
        }

        bodies.SetLinearVelocity(player_id_,
                                 JPH::Vec3(traversal_desired_ - current) / delta_seconds);
    }

    void resolve_traversal_outcome(JPH::BodyInterface &bodies) noexcept {
        const JPH::RVec3 actual = bodies.GetPosition(player_id_);
        const float error = JPH::Vec3(actual - traversal_desired_).Length();
        if (error > kTraversalStallTolerance) {
            ++traversal_stall_ticks_;
        } else {
            traversal_stall_ticks_ = 0;
        }

        if (traversal_stall_ticks_ >= kTraversalStallAbortTicks) {
            abort_traversal(bodies);
            return;
        }

        if (traversal_state_ != TraversalState::Hanging && traversal_progress_ >= 1.0) {
            complete_traversal(bodies);
        }
    }

    [[nodiscard]] JPH::Vec3 traversal_support_point_velocity(
        const JPH::BodyInterface &bodies) const noexcept {
        if (traversal_target_body_.IsInvalid()) {
            return JPH::Vec3::sZero();
        }
        const JPH::RVec3 target =
            from_support_local(bodies, traversal_target_body_, traversal_local_target_);
        return bodies.GetPointVelocity(traversal_target_body_, target);
    }

    void complete_traversal(JPH::BodyInterface &bodies) noexcept {
        const JPH::Vec3 support_velocity = traversal_support_point_velocity(bodies);
        JPH::Vec3 exit_velocity = support_velocity;
        if (traversal_state_ == TraversalState::Vaulting) {
            exit_velocity.SetX(support_velocity.GetX() + traversal_exit_relative_velocity_.GetX());
            exit_velocity.SetZ(support_velocity.GetZ() + traversal_exit_relative_velocity_.GetZ());
        }
        bodies.SetLinearVelocity(player_id_, exit_velocity);
        airborne_inherited_velocity_ = support_velocity;
        bodies.SetGravityFactor(player_id_, 1.0F);
        ++accepted_traversal_count_;
        clear_traversal();
    }

    void abort_traversal(JPH::BodyInterface &bodies) noexcept {
        JPH::Vec3 velocity = bodies.GetLinearVelocity(player_id_);
        const JPH::Vec3 support_velocity = traversal_support_point_velocity(bodies);
        JPH::Vec3 relative(velocity.GetX() - support_velocity.GetX(),
                           0.0F,
                           velocity.GetZ() - support_velocity.GetZ());
        const float relative_speed = relative.Length();
        if (relative_speed > kPlayerMaximumRelativeSpeed) {
            relative = relative * (kPlayerMaximumRelativeSpeed / relative_speed);
        }
        velocity.SetX(support_velocity.GetX() + relative.GetX());
        velocity.SetZ(support_velocity.GetZ() + relative.GetZ());
        bodies.SetLinearVelocity(player_id_, velocity);
        airborne_inherited_velocity_ = support_velocity;
        bodies.SetGravityFactor(player_id_, 1.0F);
        ++aborted_traversal_count_;
        clear_traversal();
    }

    void release_hang(JPH::BodyInterface &bodies) noexcept {
        const JPH::RVec3 ledge = from_support_local(bodies, traversal_body_, traversal_local_ledge_);
        const JPH::Vec3 support_velocity =
            traversal_body_.IsInvalid() ? JPH::Vec3::sZero()
                                        : bodies.GetPointVelocity(traversal_body_, ledge);
        bodies.SetLinearVelocity(player_id_, support_velocity);
        airborne_inherited_velocity_ = support_velocity;
        bodies.SetGravityFactor(player_id_, 1.0F);
        regrab_lockout_ticks_ = kReleaseRegrabLockoutTicks;
        clear_traversal();
    }

    void clear_traversal() noexcept {
        traversal_state_ = TraversalState::None;
        traversal_body_ = {};
        traversal_target_body_ = {};
        traversal_entity_id_ = 0;
        traversal_progress_ = 0.0;
        traversal_stall_ticks_ = 0;
        traversal_exit_relative_velocity_ = JPH::Vec3::sZero();
    }

    void update_affordance(const JPH::BodyInterface &bodies) noexcept {
        affordance_ = {};
        if (traversal_state_ != TraversalState::None || facing_.IsNearZero()) {
            return;
        }

        const JPH::RVec3 origin = bodies.GetPosition(player_id_);
        if (grounded_) {
            const LedgeProbe vault_probe =
                probe_ledge(origin, facing_, kVaultMinimumRise, kVaultMaximumRise, false);
            if (vault_probe.valid) {
                JPH::RVec3 landing_centre;
                JPH::BodyID landing_body;
                if (probe_vault_landing(vault_probe,
                                        facing_,
                                        origin.GetY() - kPlayerHalfHeight,
                                        landing_centre,
                                        landing_body)) {
                    affordance_ = vault_probe;
                    return;
                }
            }
            affordance_ = probe_ledge(origin, facing_, kMantleMinimumRise, kMantleMaximumRise, true);
            return;
        }

        affordance_ = probe_ledge(origin,
                                  facing_,
                                  kPlayerHalfHeight + kHangMinimumRiseAboveCentre,
                                  kPlayerHalfHeight + kHangMaximumRiseAboveCentre,
                                  true);
    }

    void read_state() noexcept {
        const auto &bodies = physics_system_.GetBodyInterface();

        const JPH::RVec3 player_position = bodies.GetPosition(player_id_);
        const JPH::Vec3 player_velocity = bodies.GetLinearVelocity(player_id_);
        state_.player_position = to_vector3(player_position);
        state_.player_linear_velocity =
            {player_velocity.GetX(), player_velocity.GetY(), player_velocity.GetZ()};
        state_.player_grounded = grounded_;
        state_.support_entity_id = support_entity_id_;
        state_.support_contact_point = support_sample_.contact_point;
        state_.support_point_linear_velocity = support_sample_.point_velocity;

        const JPH::RVec3 translating_position = bodies.GetPosition(translating_support_id_);
        const JPH::Vec3 translating_velocity = bodies.GetLinearVelocity(translating_support_id_);
        state_.translating_support_position = to_vector3(translating_position);
        state_.translating_support_linear_velocity =
            {translating_velocity.GetX(), translating_velocity.GetY(), translating_velocity.GetZ()};

        const JPH::RVec3 rotating_position = bodies.GetPosition(rotating_support_id_);
        const JPH::Vec3 rotating_angular_velocity = bodies.GetAngularVelocity(rotating_support_id_);
        state_.rotating_support_position = to_vector3(rotating_position);
        state_.rotating_support_yaw_radians = rotating_support_yaw_radians_;
        state_.rotating_support_angular_velocity =
            {rotating_angular_velocity.GetX(),
             rotating_angular_velocity.GetY(),
             rotating_angular_velocity.GetZ()};

        const JPH::RVec3 moving_ledge_position = bodies.GetPosition(moving_ledge_id_);
        const JPH::Vec3 moving_ledge_velocity = bodies.GetLinearVelocity(moving_ledge_id_);
        state_.moving_ledge_position = to_vector3(moving_ledge_position);
        state_.moving_ledge_linear_velocity =
            {moving_ledge_velocity.GetX(), moving_ledge_velocity.GetY(), moving_ledge_velocity.GetZ()};

        state_.traversal_state = traversal_state_;
        state_.traversal_support_entity_id = traversal_entity_id_;
        state_.traversal_progress = traversal_progress_;
        if (traversal_state_ == TraversalState::None) {
            state_.traversal_ledge_point = {};
            state_.traversal_target_point = {};
        } else {
            state_.traversal_ledge_point =
                to_vector3(from_support_local(bodies, traversal_body_, traversal_local_ledge_));
            state_.traversal_target_point = to_vector3(
                from_support_local(bodies, traversal_target_body_, traversal_local_target_));
        }

        state_.ledge_available = affordance_.valid;
        state_.ledge_entity_id = affordance_.valid ? affordance_.ledge_entity_id : 0;
        state_.ledge_point = affordance_.valid ? to_vector3(affordance_.ledge_point) : Vector3{};
        state_.ledge_rise_meters = affordance_.valid ? affordance_.rise : 0.0;

        state_.accepted_traversal_count = accepted_traversal_count_;
        state_.rejected_traversal_count = rejected_traversal_count_;
        state_.aborted_traversal_count = aborted_traversal_count_;
    }

    JoltRuntimeLease runtime_;
    JPH::TempAllocatorImpl temp_allocator_;
    JPH::JobSystemThreadPool job_system_;
    BroadPhaseLayerInterface broadphase_layer_interface_;
    ObjectVsBroadPhaseFilter object_vs_broadphase_filter_;
    ObjectLayerPairFilter object_layer_pair_filter_;
    JPH::PhysicsSystem physics_system_;
    PlayerContactListener contact_listener_;
    JPH::RefConst<JPH::Shape> player_shape_;
    JPH::BodyID deck_id_;
    JPH::BodyID translating_support_id_;
    JPH::BodyID rotating_support_id_;
    JPH::BodyID vault_rail_id_;
    JPH::BodyID mantle_ledge_id_;
    JPH::BodyID hang_ledge_id_;
    JPH::BodyID moving_ledge_id_;
    JPH::BodyID blocked_ledge_id_;
    JPH::BodyID blocked_ledge_canopy_id_;
    JPH::BodyID player_id_;
    SupportSample support_sample_{};
    JPH::Vec3 airborne_inherited_velocity_{JPH::Vec3::sZero()};
    JPH::Vec3 facing_{JPH::Vec3::sZero()};
    bool grounded_ = false;
    std::uint64_t support_entity_id_ = 0;
    double rotating_support_yaw_radians_ = 0.0;

    TraversalState traversal_state_ = TraversalState::None;
    JPH::BodyID traversal_body_;
    JPH::BodyID traversal_target_body_;
    std::uint64_t traversal_entity_id_ = 0;
    JPH::Vec3 traversal_local_start_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_local_hold_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_local_ledge_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_local_apex_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_local_target_{JPH::Vec3::sZero()};
    JPH::Vec3 traversal_exit_relative_velocity_{JPH::Vec3::sZero()};
    JPH::RVec3 traversal_desired_{JPH::RVec3::sZero()};
    double traversal_progress_ = 0.0;
    double traversal_duration_ = kMantleDurationSeconds;
    std::uint32_t traversal_stall_ticks_ = 0;
    std::uint32_t regrab_lockout_ticks_ = 0;
    std::uint64_t accepted_traversal_count_ = 0;
    std::uint64_t rejected_traversal_count_ = 0;
    std::uint64_t aborted_traversal_count_ = 0;
    LedgeProbe affordance_{};

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

bool Simulation::set_facing(const double world_x, const double world_z) noexcept {
    if (!std::isfinite(world_x) || !std::isfinite(world_z)) {
        return false;
    }
    const double length = std::hypot(world_x, world_z);
    if (!(length > 1.0e-6)) {
        return false;
    }
    facing_x_ = world_x / length;
    facing_z_ = world_z / length;
    return true;
}

bool Simulation::request_jump() noexcept {
    jump_requested_ = true;
    return true;
}

bool Simulation::request_traversal() noexcept {
    if (snapshot_.traversal_state == TraversalState::Mantling ||
        snapshot_.traversal_state == TraversalState::Vaulting) {
        return false;
    }
    traversal_requested_ = true;
    return true;
}

bool Simulation::request_release() noexcept {
    if (snapshot_.traversal_state != TraversalState::Hanging) {
        return false;
    }
    release_requested_ = true;
    return true;
}

void Simulation::step_fixed() noexcept {
    const double next_time_seconds =
        static_cast<double>(tick_index_ + 1) * kFixedStepSeconds;

    PhysicsWorld::StepCommands commands;
    commands.move_input_x = move_input_x_;
    commands.move_input_z = move_input_z_;
    commands.facing_x = facing_x_;
    commands.facing_z = facing_z_;
    commands.jump_requested = jump_requested_;
    commands.traversal_requested = traversal_requested_;
    commands.release_requested = release_requested_;

    physics_world_->step(commands, static_cast<float>(kFixedStepSeconds), next_time_seconds);
    jump_requested_ = false;
    traversal_requested_ = false;
    release_requested_ = false;
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
