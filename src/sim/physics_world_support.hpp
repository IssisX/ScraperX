#pragma once

// The physics world's plumbing: Jolt's layer filters and runtime lease,
// the player's contact listener, the probe results (ledges, grips, lips),
// small geometric helpers, and the save format's byte writer and reader.
// Private to the native (physics_world*.cpp).

#include "sim/simulation.hpp"

#include <cstring>
#include <type_traits>

#include "sim/bands.hpp"
#include "sim/mechanism_kit.hpp"
#include "sim/slingshot.hpp"
#include "sim/swing.hpp"
#include "sim/pipe_gallery.hpp"
#include "sim/trapeze.hpp"
#include "sim/crew.hpp"
#include "sim/climb_c7.hpp"
#include "sim/summit.hpp"
#include "sim/forklift.hpp"
#include "sim/yard_crane.hpp"
#include "sim/loose_stock.hpp"
#include "sim/impacts.hpp"
#include "sim/zipline.hpp"
#include "sim/corpse.hpp"
#include "sim/matron.hpp"
#include "sim/vertical/vertical_route.hpp"
#include "sim/climb_c6.hpp"

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
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/PulleyConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>
#include <cmath>
#include <limits>
#include <mutex>
#include <utility>

#include "sim/physics_world_tuning.hpp"

namespace scraperx::sim::detail {

class BroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface {
public:
    BroadPhaseLayerInterface() {
        mapping_[object_layers::kStatic] = broadphase_layers::kStatic;
        mapping_[object_layers::kMoving] = broadphase_layers::kMoving;
        mapping_[object_layers::kPlayer] = broadphase_layers::kMoving;
        mapping_[object_layers::kCorpse] = broadphase_layers::kMoving;
        mapping_[object_layers::kOnSteel] = broadphase_layers::kMoving;
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
        if (object_layer == object_layers::kMoving || object_layer == object_layers::kPlayer ||
            object_layer == object_layers::kCorpse) {
            return true;
        }
        if (object_layer == object_layers::kOnSteel) {
            return broadphase_layer == broadphase_layers::kMoving;
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
            return second != object_layers::kStatic && second != object_layers::kOnSteel;
        }
        if (first == object_layers::kMoving) {
            return second != object_layers::kCorpse;
        }
        if (first == object_layers::kPlayer) {
            return true;
        }
        if (first == object_layers::kCorpse) {
            return second != object_layers::kMoving && second != object_layers::kOnSteel;
        }
        if (first == object_layers::kOnSteel) {
            return second == object_layers::kMoving || second == object_layers::kPlayer ||
                   second == object_layers::kOnSteel;
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


struct SupportSample final {
    bool grounded = false;
    std::uint64_t entity_id = 0;
    scraperx::sim::Vector3 contact_point{};
    scraperx::sim::Vector3 point_velocity{};
    float normal_y = 0.0F;
};

// Supports that actually move. These outrank static ground when the player is
// in contact with both, so support-relative locomotion picks the machine.
[[nodiscard]] inline bool entity_is_moving_support(const std::uint64_t entity_id) noexcept {
    using Sim = scraperx::sim::Simulation;
    // Mechanism-kit bodies that move are machines by construction.
    return scraperx::sim::kit::is_dynamic_entity(entity_id) ||
           entity_id == Sim::kTranslatingSupportEntityId ||
           entity_id == Sim::kRotatingSupportEntityId ||
           entity_id == Sim::kMovingLedgeEntityId;
}

class PlayerContactListener final : public JPH::ContactListener {
public:
    void begin_tick() noexcept {
        lock();
        sample_ = {};
        struck_closing_ = 0.0F;
        hit_closing_ = 0.0F;
        hit_toward_ = JPH::Vec3::sZero();
        unlock();
    }

    // The hardest the body ran into anything, or was run into, this tick: of
    // the contacts new this tick that are not the floor under it (the normal
    // to the other within 60 degrees of horizontal, or above), how fast the
    // two closed along the normal, the body's velocity less the other's at
    // the contact, as the step began (before the solver took it). m/s, 0 for
    // none. A body's own corpses are not counted.
    [[nodiscard]] float hit_closing() const noexcept {
        lock();
        const float result = hit_closing_;
        unlock();
        return result;
    }
    // That hardest hit's normal, from the body toward what it met.
    [[nodiscard]] JPH::Vec3 hit_toward() const noexcept {
        lock();
        const JPH::Vec3 result = hit_toward_;
        unlock();
        return result;
    }

    // Phase 4: the fastest a moving body came at the player this tick across a
    // side contact (normal within 60 degrees of horizontal), its own velocity
    // toward the player, m/s; 0 for none. Walking into a wall, or into
    // anything standing still, is never being struck.
    [[nodiscard]] float struck_closing() const noexcept {
        lock();
        const float result = struck_closing_;
        unlock();
        return result;
    }

    [[nodiscard]] SupportSample sample() const noexcept {
        lock();
        const SupportSample result = sample_;
        unlock();
        return result;
    }

    // AS-003: the entity on the player's carry point, 0 for none. The player
    // and what it holds share the carry point; contact between them is never
    // meaningful, only the constraint relates them.
    void set_carried_entity(const std::uint64_t entity) noexcept {
        carried_entity_.store(entity, std::memory_order_relaxed);
    }

    // The soles' grip, as a friction coefficient, while the feet hold the body
    // at rest on its support (PhysicsWorld::apply_locomotion); 0 otherwise.
    // The body's contacts are frictionless, so the feet act only through the
    // velocity locomotion sets before the solve. Held there, the support's own
    // reaction still moved it in the solve: along a contact normal that tilts
    // (the capsule's rim on an edge, a slope) it pushes out and down as well
    // as up, and nothing opposed that. Given to every contact the body stands
    // on this tick, the grip holds it as soles do, up to the grip times the
    // load on them.
    void set_planted_grip(const float grip) noexcept {
        planted_grip_.store(grip, std::memory_order_relaxed);
    }

    // The player and what it carries share the carry point: contact between
    // them is never meaningful, only the constraint relates them.
    [[nodiscard]] JPH::ValidateResult OnContactValidate(
        const JPH::Body &first, const JPH::Body &second, JPH::RVec3Arg,
        const JPH::CollideShapeResult &) override {
        const auto first_entity = first.GetUserData();
        const auto second_entity = second.GetUserData();
        using Sim = scraperx::sim::Simulation;
        const std::uint64_t carried = carried_entity_.load(std::memory_order_relaxed);
        const bool is_player_vs_carried =
            carried != 0 &&
            ((first_entity == Sim::kPlayerEntityId && second_entity == carried) ||
             (first_entity == carried && second_entity == Sim::kPlayerEntityId));
        return is_player_vs_carried ? JPH::ValidateResult::RejectAllContactsForThisBodyPair
                                    : JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    void OnContactAdded(const JPH::Body &first,
                        const JPH::Body &second,
                        const JPH::ContactManifold &manifold,
                        JPH::ContactSettings &settings) override {
        observe_support(first, second, manifold, settings);
        observe_hit(first, second, manifold);
    }

    void OnContactPersisted(const JPH::Body &first,
                            const JPH::Body &second,
                            const JPH::ContactManifold &manifold,
                            JPH::ContactSettings &settings) override {
        observe_support(first, second, manifold, settings);
    }

private:
    void observe_hit(const JPH::Body &first, const JPH::Body &second, const JPH::ContactManifold &manifold) noexcept {
        if (manifold.mRelativeContactPointsOn1.size() == 0) {
            return;
        }
        using Sim = scraperx::sim::Simulation;
        const bool player_first = first.GetUserData() == Sim::kPlayerEntityId;
        if (!player_first && second.GetUserData() != Sim::kPlayerEntityId) {
            return;
        }
        const JPH::Body &player = player_first ? first : second;
        const JPH::Body &other = player_first ? second : first;
        const std::uint64_t entity = other.GetUserData();
        // Her body is not a blow: what her hands do decides (matron.hpp, the
        // stages), a shove before she is fully changed.
        if (other.IsSensor() || (entity >= kCorpseEntityBase && entity < kCorpseEntityBase + 0x10000) ||
            entity == Matron::kEntity) {
            return;
        }
        // The normal points from body 1 to body 2: here from the player to
        // the other. The floor under the body is a landing's.
        const JPH::Vec3 toward = player_first ? manifold.mWorldSpaceNormal : -manifold.mWorldSpaceNormal;
        if (toward.GetY() < -0.5F) {
            return;
        }
        const JPH::RVec3 at = manifold.GetWorldSpaceContactPointOn1(0);
        const float closing = (player.GetLinearVelocity() - other.GetPointVelocity(at)).Dot(toward);
        if (closing > 0.0F) {
            lock();
            if (closing > hit_closing_) {
                hit_closing_ = closing;
                hit_toward_ = toward;
            }
            unlock();
        }
    }

    [[nodiscard]] static int support_rank(const std::uint64_t entity_id) noexcept {
        if (entity_id == 0 || entity_id == scraperx::sim::Simulation::kPlayerEntityId) {
            return 0;
        }
        return entity_is_moving_support(entity_id) ? 2 : 1;
    }

    void observe_support(const JPH::Body &first,
                         const JPH::Body &second,
                         const JPH::ContactManifold &manifold,
                         JPH::ContactSettings &settings) noexcept {
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

        // A sensor body still produces a full contact manifold -- Jolt's own
        // doc comment is explicit that sensors "will receive collision
        // callbacks, but will not cause any collision responses" -- so without
        // this check a sensor would read as real support from geometry alone,
        // with nothing holding the player up. A sensor is never a support.
        if (support_body != nullptr && !support_body->IsSensor() && std::abs(support_normal_y) < 0.5F &&
            support_body->GetMotionType() != JPH::EMotionType::Static) {
            // Toward the player: the manifold normal points from body 1 to body 2.
            // The body's own motion toward the player, not the closing of the
            // two: walking into something that stands still is not being struck.
            const JPH::Vec3 to_player =
                support_body == &second ? -manifold.mWorldSpaceNormal : manifold.mWorldSpaceNormal;
            const float closing = support_body->GetPointVelocity(support_contact_point).Dot(to_player);
            if (closing > 0.0F) {
                lock();
                struck_closing_ = std::max(struck_closing_, closing);
                unlock();
            }
        }
        if (support_body == nullptr || support_body->IsSensor() ||
            support_normal_y < kSupportNormalThreshold || support_rank(support_entity) == 0) {
            return;
        }
        // Stood on: planted feet grip it (set_planted_grip).
        settings.mCombinedFriction = planted_grip_.load(std::memory_order_relaxed);

        const JPH::Vec3 point_velocity = support_body->GetPointVelocity(support_contact_point);
        const SupportSample candidate{
            true,
            support_entity,
            {support_contact_point.GetX(), support_contact_point.GetY(), support_contact_point.GetZ()},
            {point_velocity.GetX(), point_velocity.GetY(), point_velocity.GetZ()},
            support_normal_y,
        };

        lock();
        if (!sample_.grounded || supports_better(candidate, sample_)) {
            sample_ = candidate;
        }
        unlock();
    }

    // Jolt calls the contact callbacks from its worker threads in no fixed
    // order, so the pick among a tick's candidates must be a total order: with
    // "first seen wins" on a tie, the rider's support -- and everything it
    // carries -- differed from run to run.
    [[nodiscard]] static bool supports_better(const SupportSample &candidate,
                                              const SupportSample &current) noexcept {
        const int candidate_rank = support_rank(candidate.entity_id);
        const int current_rank = support_rank(current.entity_id);
        if (candidate_rank != current_rank) {
            return candidate_rank > current_rank;
        }
        if (candidate.normal_y != current.normal_y) {
            return candidate.normal_y > current.normal_y;
        }
        if (candidate.contact_point.y != current.contact_point.y) {
            return candidate.contact_point.y > current.contact_point.y;
        }
        if (candidate.entity_id != current.entity_id) {
            return candidate.entity_id < current.entity_id;
        }
        if (candidate.contact_point.x != current.contact_point.x) {
            return candidate.contact_point.x < current.contact_point.x;
        }
        return candidate.contact_point.z < current.contact_point.z;
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
    float struck_closing_ = 0.0F;
    float hit_closing_ = 0.0F;
    JPH::Vec3 hit_toward_ = JPH::Vec3::sZero();
    // Corpses' bodies (Corpses: kUserDataBase on).
    static constexpr std::uint64_t kCorpseEntityBase = 3300;
    std::atomic<std::uint64_t> carried_entity_{0};
    std::atomic<float> planted_grip_{0.0F};
};

[[nodiscard]] inline JPH::RVec3 spawn_position(const scraperx::sim::InitialSpawn spawn) noexcept {
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
    case scraperx::sim::InitialSpawn::WallRunApproach:
        return {-19.0, 1.0, -26.3};
    case scraperx::sim::InitialSpawn::PipeGallery:
        // On the gallery's start stand, 2.5 m short of its take-off.
        return {23.5, 2.5, -34.0};
    case scraperx::sim::InitialSpawn::SwingApproach:
        return {-1.0, 1.0, -20.0};
    case scraperx::sim::InitialSpawn::HangHighDrop:
        return {7.6, 10.2, 4.0};
    case scraperx::sim::InitialSpawn::MovingLedgeApproach:
        return {6.1, 4.2, 12.0};
    case scraperx::sim::InitialSpawn::BlockedLedgeApproach:
        return {-3.6, 1.2, -8.0};
    case scraperx::sim::InitialSpawn::HighDrop:
        // ~61 m above the static deck: unmitigated free fall reaches
        // sqrt(2*g*61) ~= 34.6 m/s, well past kLethalImpactSpeedMps.
        return {0.0, 62.0, -8.0};
    case scraperx::sim::InitialSpawn::SurvivableDrop:
        // ~12 m above the static deck: unmitigated free fall reaches
        // sqrt(2*g*12) ~= 15.3 m/s, under kLethalImpactSpeedMps with margin.
        return {0.0, 13.0, -8.0};
    case scraperx::sim::InitialSpawn::Deck154:
        // The 154 m deck's north band, north of Stage A's cage.
        return {-10.5, 155.0, -128.2};
    case scraperx::sim::InitialSpawn::WellBCage:
        // Inside B's cage, west of its middle.
        return {-7.6, 177.2, -131.2};
    case scraperx::sim::InitialSpawn::WellCPlatform:
        // On C's platform, west of its middle.
        return {-4.3, 199.2, -132.2};
    case scraperx::sim::InitialSpawn::Ring176East:
        // On the 176 ring, 1 m in from its inner edge.
        return {9.5, 177.2, -127.9};
    case scraperx::sim::InitialSpawn::Ring198East:
        return {11.0, 199.2, -127.5};
    case scraperx::sim::InitialSpawn::Ring220North:
        // On the 220 ring west of the spool, as a rider off C's platform.
        return {4.0, 221.2, -129.2};
    case scraperx::sim::InitialSpawn::WetECab:
        // In E's cab on its bottom stop, south of its door.
        return {13.0, 257.2, -137.2};
    case scraperx::sim::InitialSpawn::WetFPlatform:
        // On F's platform, east of the hose.
        return {13.4, 299.2, -139.9};
    case scraperx::sim::InitialSpawn::PlateTop:
        // On TP-340 north of F's hole, as a rider off F's platform.
        return {12.0, 341.2, -136.8};
    case scraperx::sim::InitialSpawn::PlateTop640:
        // On TP-640 north of L's hole, as a rider off L's cab.
        return {-3.0, 641.2, -156.5};
    case scraperx::sim::InitialSpawn::Deck662:
        // On the 662 deck north of C4's cabinet, clear of M's shafts.
        return {6.2, 663.05, -146.0};
    case scraperx::sim::InitialSpawn::Deck684:
        // On the 684 deck by its east girder, east of stage N's cage.
        return {-3.2, 685.05, -155.0};
    case scraperx::sim::InitialSpawn::Deck706:
        // On the 706 deck by where C5's plant floor opens off its east edge.
        return {-3.5, 707.05, -147.5};
    case scraperx::sim::InitialSpawn::Ring242South:
        // On the 242 ring's south side, where the swing sets its rider down.
        return {-15.0, 243.15, -129.3};
    case scraperx::sim::InitialSpawn::Ring264West:
        // On the 264 ring's west band, where C6's climber tops out.
        return {-19.5, 265.15, -164.5};
    case scraperx::sim::InitialSpawn::Deck750:
        // On the 750 deck, east of stage O's cab.
        return {8.0, 751.0, -145.0};
    case scraperx::sim::InitialSpawn::Summit:
        // On the summit deck, a few steps west of the derrick's upper receiver.
        return {9.0, 803.65, -187.5};
    case scraperx::sim::InitialSpawn::Deck728:
        // On the 728 deck by C5's last mantle, north-east of stage O's cab.
        return {9.0, 729.05, -144.0};
    case scraperx::sim::InitialSpawn::Ring374West:
        // On the 374 ring's west band by H's gangway.
        return {-14.5, 375.2, -145.6};
    case scraperx::sim::InitialSpawn::ShopICage:
        // On I's cage, west of its shackle.
        return {-7.3, 419.2, -145.6};
    case scraperx::sim::InitialSpawn::Ring484North:
        // On the 484 ring's north band west of J's traveler.
        return {-3.0, 485.2, -139.6};
    case scraperx::sim::InitialSpawn::CraneKCage:
        // On K's cage, west of its shackle.
        return {-13.2, 529.2, -155.0};
    case scraperx::sim::InitialSpawn::CraneLCab:
        // On L's cab, west of its middle.
        return {-3.6, 573.2, -160.1};
    case scraperx::sim::InitialSpawn::Deck2South:
        // On deck 2's south band, north of S1's gangway.
        return {10.0, 23.2, -125.5};
    case scraperx::sim::InitialSpawn::Deck4South:
        // On deck 4's south band, west of C1's davit.
        return {11.0, 45.2, -125.5};
    case scraperx::sim::InitialSpawn::Deck6South:
        // On deck 6's south band, by S2's upper gangway.
        return {10.0, 67.2, -125.5};
    case scraperx::sim::InitialSpawn::Deck8North:
        // On deck 8's north band, where C2 lands.
        return {10.0, 89.2, -170.0};
    case scraperx::sim::InitialSpawn::Deck12North:
        // On deck 12's north band, where S3 lands.
        return {-8.0, 133.2, -170.0};
    case scraperx::sim::InitialSpawn::ExteriorGrade:
        // At grade, outdoors, 120 m short of the tower face: far enough that the
        // mass reads as something you approach, close enough that its lower
        // third already fills the frame.
        return {6.0, 1.2, -25.0};
    case scraperx::sim::InitialSpawn::TranslatingSupport:
        return {0.0, 3.0, 8.0};
    default:
        return {6.0, 1.2, -25.0};
    }
}

inline void approach_relative_horizontal_velocity(JPH::Vec3 &world_velocity,
                                           const JPH::Vec3 reference_velocity,
                                           const double move_input_x,
                                           const double move_input_z,
                                           const float acceleration,
                                           const float delta_seconds,
                                           const float full_speed = kPlayerMaximumRelativeSpeed) noexcept {
    float relative_x = world_velocity.GetX() - reference_velocity.GetX();
    float relative_z = world_velocity.GetZ() - reference_velocity.GetZ();
    const float target_x = static_cast<float>(move_input_x) * full_speed;
    const float target_z = static_cast<float>(move_input_z) * full_speed;
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

[[nodiscard]] inline float smoothstep(const float edge0, const float edge1, const float value) noexcept {
    if (edge1 <= edge0) {
        return value < edge0 ? 0.0F : 1.0F;
    }
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

// Cubic ease over [0, 1] that leaves at `entry_slope` (in units of the mean
// rate) and arrives at rest: a Hermite segment, monotonic for slopes in
// [0, 3], so a body entering it at speed carries that speed in.
[[nodiscard]] inline float ease_to_rest(const float value, const float entry_slope) noexcept {
    const float t = std::clamp(value, 0.0F, 1.0F);
    const float slope = std::clamp(entry_slope, 0.0F, 3.0F);
    return slope * t * (1.0F - t) * (1.0F - t) + t * t * (3.0F - 2.0F * t);
}

// A vault should meet the run on both ends. Equal Hermite end slopes preserve
// the incoming velocity along the landing path, while the middle absorbs the
// remaining distance. Slopes in [0, 3] keep the path monotone.
[[nodiscard]] inline float ease_with_carried_speed(const float value, const float slope) noexcept {
    const float t = std::clamp(value, 0.0F, 1.0F);
    const float s = std::clamp(slope, 0.0F, 3.0F);
    return t * t * (3.0F - 2.0F * t) + s * t * (1.0F - t) * (1.0F - 2.0F * t);
}

[[nodiscard]] inline scraperx::sim::Vector3 to_vector3(const JPH::RVec3 value) noexcept {
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

// Step 2 movement. A hold a hand has closed round: the point on the member's
// axis, on the body the member belongs to.
struct Grip final {
    bool valid = false;
    JPH::BodyID body;
    std::uint64_t entity_id = 0;
    JPH::RVec3 point{JPH::RVec3::sZero()};
};

// A ledge's lip, faced along a hang's normal: the top just inside the edge,
// where a hanging body holds below it, and where a climb up it lands.
struct Lip final {
    bool valid = false;
    JPH::BodyID body;
    std::uint64_t entity_id = 0;
    JPH::RVec3 ledge{JPH::RVec3::sZero()};
    JPH::RVec3 hold{JPH::RVec3::sZero()};
    JPH::RVec3 landing{JPH::RVec3::sZero()};
};

// One box of a body's shape, in world space.
struct LeafBox final {
    JPH::RVec3 centre{JPH::RVec3::sZero()};
    JPH::Vec3 axes[3]{};
    JPH::Vec3 half{JPH::Vec3::sZero()};
};

// A member a hand can close round (see kGripMaxSection): its long axis in
// `long_axis`.
[[nodiscard]] inline bool box_is_hold(const LeafBox &box, std::uint32_t &long_axis) noexcept {
    long_axis = 0;
    for (std::uint32_t axis = 1; axis < 3; ++axis) {
        if (box.half[axis] > box.half[long_axis]) {
            long_axis = axis;
        }
    }
    if (2.0F * box.half[long_axis] < kGripMinLength) {
        return false;
    }
    for (std::uint32_t axis = 0; axis < 3; ++axis) {
        if (axis != long_axis && 2.0F * box.half[axis] > kGripMaxSection) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline bool leaf_box(const JPH::Body &body, const JPH::SubShapeID &sub_shape,
                            LeafBox &out) noexcept {
    JPH::SubShapeID remainder;
    const JPH::TransformedShape leaf = body.GetShape()->GetSubShapeTransformedShape(
        sub_shape, body.GetCenterOfMassPosition(), body.GetRotation(), JPH::Vec3::sReplicate(1.0F),
        remainder);
    if (leaf.mShape == nullptr || leaf.mShape->GetSubType() != JPH::EShapeSubType::Box) {
        return false;
    }
    out.half = static_cast<const JPH::BoxShape *>(leaf.mShape.GetPtr())->GetHalfExtent();
    const JPH::RMat44 frame = leaf.GetCenterOfMassTransform();
    out.centre = frame.GetTranslation();
    for (std::uint32_t axis = 0; axis < 3; ++axis) {
        out.axes[axis] = frame.GetColumn3(axis).Normalized();
    }
    return true;
}

// A saved climb (release plan B5): the committed checkpoint the death restore
// uses, written as the bytes of this build's state types. A save carries its
// format key; a save from a build whose layout differs is refused, not misread.
class SaveWriter final {
public:
    template <typename T>
    void pod(const T &value) {
        static_assert(std::is_trivially_copyable_v<T>, "saved as bytes");
        const auto *bytes = reinterpret_cast<const std::uint8_t *>(&value);
        out.insert(out.end(), bytes, bytes + sizeof(T));
    }
    template <typename T>
    void list(const std::vector<T> &values) {
        pod(static_cast<std::uint32_t>(values.size()));
        for (const T &value : values) {
            pod(value);
        }
    }
    void bits(const std::vector<bool> &values) {
        pod(static_cast<std::uint32_t>(values.size()));
        for (const bool value : values) {
            pod(static_cast<std::uint8_t>(value ? 1 : 0));
        }
    }
    std::vector<std::uint8_t> out;
};

class SaveReader final {
public:
    SaveReader(const std::uint8_t *data, std::size_t size) : data_(data), size_(size) {}
    template <typename T>
    bool pod(T &value) {
        static_assert(std::is_trivially_copyable_v<T>, "saved as bytes");
        if (size_ - at_ < sizeof(T)) {
            return false;
        }
        std::memcpy(&value, data_ + at_, sizeof(T));
        at_ += sizeof(T);
        return true;
    }
    // A list must be exactly as long as this build's: a save from another
    // world (more bodies, fewer bins) is refused.
    template <typename T>
    bool list(std::vector<T> &values, std::size_t expected) {
        std::uint32_t count = 0;
        if (!pod(count) || count != expected) {
            return false;
        }
        values.resize(count);
        for (T &value : values) {
            if (!pod(value)) {
                return false;
            }
        }
        return true;
    }
    // A list that grows in play (the piles), up to `most` long.
    template <typename T>
    bool list_up_to(std::vector<T> &values, std::size_t most) {
        std::uint32_t count = 0;
        if (!pod(count) || count > most) {
            return false;
        }
        values.resize(count);
        for (T &value : values) {
            if (!pod(value)) {
                return false;
            }
        }
        return true;
    }
    bool bits(std::vector<bool> &values, std::size_t expected) {
        std::uint32_t count = 0;
        if (!pod(count) || count != expected) {
            return false;
        }
        values.assign(count, false);
        for (std::uint32_t i = 0; i < count; ++i) {
            std::uint8_t value = 0;
            if (!pod(value) || value > 1) {
                return false;
            }
            values[i] = value == 1;
        }
        return true;
    }
    [[nodiscard]] bool done() const noexcept { return at_ == size_; }

private:
    const std::uint8_t *data_;
    std::size_t size_;
    std::size_t at_ = 0;
};

} // namespace scraperx::sim::detail
