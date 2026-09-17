#include "sim/simulation.hpp"

#ifndef SCRAPERX_HAS_JOLT
#error "WO-001 requires the pinned Jolt physics substrate"
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
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

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

class PlayerContactListener final : public JPH::ContactListener {
public:
    void begin_tick() noexcept {
        grounded_.store(false, std::memory_order_relaxed);
        support_entity_id_.store(0, std::memory_order_relaxed);
    }

    [[nodiscard]] bool grounded() const noexcept {
        return grounded_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::uint64_t support_entity_id() const noexcept {
        return support_entity_id_.load(std::memory_order_relaxed);
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
    void observe_support(const JPH::Body &first,
                         const JPH::Body &second,
                         const JPH::ContactManifold &manifold) noexcept {
        const auto first_entity = first.GetUserData();
        const auto second_entity = second.GetUserData();
        std::uint64_t support_entity = 0;
        float support_normal_y = 0.0F;

        if (first_entity == scraperx::sim::Simulation::kPlayerEntityId) {
            support_entity = second_entity;
            support_normal_y = -manifold.mWorldSpaceNormal.GetY();
        } else if (second_entity == scraperx::sim::Simulation::kPlayerEntityId) {
            support_entity = first_entity;
            support_normal_y = manifold.mWorldSpaceNormal.GetY();
        }

        if (support_entity != 0 && support_normal_y >= 0.55F) {
            grounded_.store(true, std::memory_order_relaxed);
            support_entity_id_.store(support_entity, std::memory_order_relaxed);
        }
    }

    std::atomic<bool> grounded_{false};
    std::atomic<std::uint64_t> support_entity_id_{0};
};

} // namespace

namespace scraperx::sim {

class Simulation::PhysicsWorld final {
public:
    PhysicsWorld()
        : temp_allocator_(8U * 1024U * 1024U),
          job_system_(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, 1) {
        physics_system_.Init(128,
                             0,
                             256,
                             128,
                             broadphase_layer_interface_,
                             object_vs_broadphase_filter_,
                             object_layer_pair_filter_);
        physics_system_.SetContactListener(&contact_listener_);

        auto &bodies = physics_system_.GetBodyInterface();

        JPH::BodyCreationSettings deck_settings(
            new JPH::BoxShape(JPH::Vec3(16.0F, 0.5F, 16.0F)),
            JPH::RVec3(0.0, -0.5, 0.0),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Static,
            object_layers::kStatic);
        deck_settings.mFriction = 0.6F;
        deck_settings.mUserData = Simulation::kStaticDeckEntityId;
        deck_id_ = bodies.CreateAndAddBody(deck_settings, JPH::EActivation::DontActivate);

        JPH::BodyCreationSettings player_settings(
            new JPH::CapsuleShape(0.55F, 0.35F),
            JPH::RVec3(0.0, 3.0, 8.0),
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
        read_player_state();
    }

    ~PhysicsWorld() {
        physics_system_.SetContactListener(nullptr);
        auto &bodies = physics_system_.GetBodyInterface();
        bodies.RemoveBody(player_id_);
        bodies.DestroyBody(player_id_);
        bodies.RemoveBody(deck_id_);
        bodies.DestroyBody(deck_id_);
    }

    void step(const double move_input_x, const double move_input_z, const float delta_seconds) noexcept {
        constexpr float kMaximumSpeedMetersPerSecond = 5.5F;
        constexpr float kAccelerationMetersPerSecondSquared = 22.0F;

        auto &bodies = physics_system_.GetBodyInterface();
        JPH::Vec3 velocity = bodies.GetLinearVelocity(player_id_);
        const float target_x = static_cast<float>(move_input_x) * kMaximumSpeedMetersPerSecond;
        const float target_z = static_cast<float>(move_input_z) * kMaximumSpeedMetersPerSecond;
        float delta_x = target_x - velocity.GetX();
        float delta_z = target_z - velocity.GetZ();
        const float delta_length = std::sqrt(delta_x * delta_x + delta_z * delta_z);
        const float maximum_delta = kAccelerationMetersPerSecondSquared * delta_seconds;
        if (delta_length > maximum_delta && delta_length > 0.0F) {
            const float scale = maximum_delta / delta_length;
            delta_x *= scale;
            delta_z *= scale;
        }

        velocity.SetX(velocity.GetX() + delta_x);
        velocity.SetZ(velocity.GetZ() + delta_z);
        bodies.SetLinearVelocity(player_id_, velocity);

        contact_listener_.begin_tick();
        physics_system_.Update(delta_seconds, 1, &temp_allocator_, &job_system_);
        read_player_state();
        grounded_ = contact_listener_.grounded();
        support_entity_id_ = contact_listener_.support_entity_id();
    }

    [[nodiscard]] Vector3 position() const noexcept { return position_; }
    [[nodiscard]] Vector3 linear_velocity() const noexcept { return linear_velocity_; }
    [[nodiscard]] bool grounded() const noexcept { return grounded_; }
    [[nodiscard]] std::uint64_t support_entity_id() const noexcept { return support_entity_id_; }

private:
    void read_player_state() noexcept {
        const auto &bodies = physics_system_.GetBodyInterface();
        const JPH::RVec3 position = bodies.GetPosition(player_id_);
        const JPH::Vec3 velocity = bodies.GetLinearVelocity(player_id_);
        position_ = {position.GetX(), position.GetY(), position.GetZ()};
        linear_velocity_ = {velocity.GetX(), velocity.GetY(), velocity.GetZ()};
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
    JPH::BodyID player_id_;
    Vector3 position_{};
    Vector3 linear_velocity_{};
    bool grounded_ = false;
    std::uint64_t support_entity_id_ = 0;
};

Simulation::Simulation() : physics_world_(std::make_unique<PhysicsWorld>()) {
    player_position_ = physics_world_->position();
    player_linear_velocity_ = physics_world_->linear_velocity();
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

void Simulation::step_fixed() noexcept {
    physics_world_->step(move_input_x_, move_input_z_, static_cast<float>(kFixedStepSeconds));
    player_position_ = physics_world_->position();
    player_linear_velocity_ = physics_world_->linear_velocity();
    player_grounded_ = physics_world_->grounded();
    support_entity_id_ = physics_world_->support_entity_id();
    ++tick_index_;
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
    return Snapshot{
        tick_index_,
        static_cast<double>(tick_index_) * kFixedStepSeconds,
        kFixedStepSeconds,
        remainder_seconds_ / kFixedStepSeconds,
        player_position_,
        player_linear_velocity_,
        player_grounded_,
        support_entity_id_,
    };
}

} // namespace scraperx::sim
