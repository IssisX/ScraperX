#pragma once

// Corpses (the owner, 2026-10-17: deaths must show their physical
// consequences). A lethal impact, or the Matron's catch, leaves the body
// where it died: a ragdoll of the climber's own proportions (kParts, the
// BodyPlan below) spawned the tick of the death in a standing pose at the
// body's place, moving as the body was, before the climber is restored to
// the checkpoint. It falls, folds and lies where the impact put it; the
// knees and elbows bend only the way they bend, so a limb past its stop
// reads as broken, not as rope. The presentation rides the newest corpse's
// head for the moment after the death.
//
// Corpses collide with the static world, the climber and each other, never
// with the machines' moving bodies: a corpse is not a load or a weight and
// cannot jam a cage or sit on a scale (object_layers::kCorpse). They are not
// kit bodies and not saved: a loaded game has none (the declared
// abstraction). At most kMaxCorpses lie in the world; a death past that
// takes the oldest away (DROP_OLDEST).

#include <Jolt/Jolt.h>

#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Ragdoll/Ragdoll.h>

#include <array>
#include <cstdint>
#include <deque>

namespace scraperx::sim {

class Corpses final {
public:
    static constexpr int kMaxCorpses = 8;

    // The climber's parts, root first; each a capsule along its own length.
    enum Part : int {
        kPelvis = 0,
        kChest,
        kHead,
        kUpperArmLeft,
        kForearmLeft,
        kUpperArmRight,
        kForearmRight,
        kThighLeft,
        kShinLeft,
        kThighRight,
        kShinRight,
        kParts
    };
    // The climber's proportions, shared with the presentation's living body:
    // each part's capsule (radius, half the length of its straight middle),
    // its mass, and the joints' heights over the soles and spans across the
    // body. An 84 kg, 1.8 m climber, as the physics body (kPlayerMassKg).
    struct PartPlan {
        float radius;
        float half_length;
        float mass_kg;
    };
    static constexpr std::array<PartPlan, kParts> kPlan{{
        {0.14F, 0.07F, 12.0F},   // pelvis, across the hips
        {0.16F, 0.12F, 30.4F},   // chest, the spine to the shoulders
        {0.11F, 0.03F, 5.0F},    // head
        {0.05F, 0.10F, 2.5F},    // upper arm, left
        {0.045F, 0.10F, 1.8F},   // forearm and hand, left
        {0.05F, 0.10F, 2.5F},    // upper arm, right
        {0.045F, 0.10F, 1.8F},   // forearm and hand, right
        {0.075F, 0.15F, 9.0F},   // thigh, left
        {0.06F, 0.17F, 5.0F},    // shin and boot, left
        {0.075F, 0.15F, 9.0F},   // thigh, right
        {0.06F, 0.17F, 5.0F},    // shin and boot, right
    }};
    static constexpr float kHipY = 0.92F;
    static constexpr float kKneeY = 0.50F;
    static constexpr float kSoleY = 0.0F;
    static constexpr float kSpineY = 1.07F;
    static constexpr float kNeckY = 1.48F;
    static constexpr float kShoulderY = 1.43F;
    static constexpr float kElbowY = 1.14F;
    static constexpr float kWristY = 0.88F;
    static constexpr float kHipHalfSpan = 0.10F;
    static constexpr float kShoulderHalfSpan = 0.21F;

    struct PartPose {
        JPH::RVec3 position = JPH::RVec3::sZero();
        JPH::Quat rotation = JPH::Quat::sIdentity();
    };

    Corpses(JPH::PhysicsSystem &physics, JPH::ObjectLayer layer);
    ~Corpses();
    Corpses(const Corpses &) = delete;
    Corpses &operator=(const Corpses &) = delete;

    // A corpse in a standing pose, its soles at `soles`, facing `facing`
    // (horizontal), every part moving at `velocity`.
    void spawn(JPH::RVec3 soles, JPH::Vec3 facing, JPH::Vec3 velocity);
    // A loaded game: none.
    void clear();

    [[nodiscard]] int count() const noexcept { return static_cast<int>(corpses_.size()); }
    // Oldest first; the newest is count() - 1.
    [[nodiscard]] PartPose part(int corpse, int part) const;
    [[nodiscard]] bool resting(int corpse) const;
    // Every corpse spawned since the world was made.
    [[nodiscard]] std::uint64_t spawned() const noexcept { return spawned_; }

private:
    JPH::PhysicsSystem &physics_;
    JPH::ObjectLayer layer_;
    std::deque<JPH::Ref<JPH::Ragdoll>> corpses_;
    std::uint64_t spawned_ = 0;
};

} // namespace scraperx::sim
