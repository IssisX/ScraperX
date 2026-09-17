#include "sim/simulation.hpp"

#ifndef SCRAPERX_HAS_JOLT
#error "WO-002 requires the pinned Jolt physics substrate"
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

constexpr float kSupportNormalThreshold = 0.55F;
constexpr float kPlayerMaximumRelativeSpeed = 5.5F;
constexpr float kGroundAcceleration = 22.0F;
constexpr float kAirAcceleration = 8.0F;
constexpr float kJumpSpeed = 5.5F;
constexpr double kTranslatingSupportAmplitudeMeters = 2.0;
constexpr double kTranslatingSupportAngularFrequency = 1.0;
constexpr double kRotatingSupportAngularSpeed = 0.8;

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
        if (entity_id == scraperx::sim::Simulation::kTranslatingSupportEntityId ||
            entity_id == scraperx::sim::Simulation::kRotatingSupportEntityId) {
            return 2;
        }
        if (entity_id == scraperx::sim::Simulation::kStaticDeckEntityId) {
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
    case scraperx::sim::InitialSpawn::RotatingSupport:
        return {-6.5, 3.0, 0.0};
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

        JPH::BodyCreationSettings deck_settings(
            new JPH::BoxShape(JPH::Vec3(16.0F, 0.5F, 16.0F)),
            JPH::RVec3(0.0, -0.5, 0.0),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Static,
            object_layers::kStatic);
        deck_settings.mFriction = 0.6F;
        deck_settings.mUserData = Simulation::kStaticDeckEntityId;
        deck_id_ = bodies.CreateAndAddBody(deck_settings, JPH::EActivation::DontActivate);

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
        remove_and_destroy(bodies, rotating_support_id_);
        remove_and_destroy(bodies, translating_support_id_);
        remove_and_destroy(bodies, deck_id_);
    }

    void step(const double move_input_x,
              const double move_input_z,
              const bool jump_requested,
              const float delta_seconds,
              const double next_time_seconds) noexcept {
        auto &bodies = physics_system_.GetBodyInterface();
        update_support_motion(bodies, delta_seconds, next_time_seconds);

        JPH::Vec3 player_velocity = bodies.GetLinearVelocity(player_id_);
        JPH::Vec3 reference_velocity = airborne_inherited_velocity_;

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

        const bool jump_started = jump_requested && grounded_;
        if (jump_started) {
            player_velocity.SetY(reference_velocity.GetY() + kJumpSpeed);
        }
        bodies.SetLinearVelocity(player_id_, player_velocity);

        contact_listener_.begin_tick();
        physics_system_.Update(delta_seconds, 1, &temp_allocator_, &job_system_);

        SupportSample support = contact_listener_.sample();
        if (jump_started) {
            support = {};
        }
        support_sample_ = support;
        grounded_ = support.grounded;
        support_entity_id_ = support.entity_id;
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
    JPH::BodyID translating_support_id_;
    JPH::BodyID rotating_support_id_;
    JPH::BodyID player_id_;
    SupportSample support_sample_{};
    JPH::Vec3 airborne_inherited_velocity_{JPH::Vec3::sZero()};
    bool grounded_ = false;
    std::uint64_t support_entity_id_ = 0;
    double rotating_support_yaw_radians_ = 0.0;
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

void Simulation::step_fixed() noexcept {
    const double next_time_seconds =
        static_cast<double>(tick_index_ + 1) * kFixedStepSeconds;
    physics_world_->step(move_input_x_,
                         move_input_z_,
                         jump_requested_,
                         static_cast<float>(kFixedStepSeconds),
                         next_time_seconds);
    jump_requested_ = false;
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
