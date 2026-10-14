#pragma once

// Life L2: the yard crew (GDD 19, Inhabitants). Workers are real bodies: an
// 80 kg upright capsule each, in the same Jolt world as the player and every
// machine, held upright (rotation locked) and moved only by a bounded
// horizontal acceleration from their own feet. They can be bumped, pushed
// and blocked like anything else; nothing teleports them.
//
// Each worker walks a route of stations over the yard's real grade. At a
// station it works for a while, facing its work, then walks on. Around the
// player it behaves like a person on a site:
//   - a player walking into its way: it steps aside, watches them pass, and
//     goes back to what it was doing (YIELDING);
//   - a player running at it, landing hard or falling near it: it flinches
//     back and stares (STARTLED);
//   - a shove its feet did not ask for (the body's velocity off what it was
//     driving by more than kShoveMps): it stumbles, its feet stop driving,
//     and it recovers (STUMBLING).
// Only the feet's bounded acceleration and gravity move a worker; a worker
// off the ground does not drive at all.
//
// Workers are not saved: on a load they carry on from where they are.

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <array>
#include <cstdint>
#include <vector>

namespace scraperx::sim {

class Crew final {
public:
    enum class State : std::uint8_t { Walking = 0, Working = 1, Yielding = 2, Startled = 3, Stumbling = 4 };

    static constexpr std::uint64_t kWorkerEntityBase = 3100;
    static constexpr std::size_t kMaxWorkers = 8;
    static constexpr float kMassKg = 80.0F;
    static constexpr float kRadius = 0.28F;
    static constexpr float kHalfHeight = 0.62F;      // cylinder half; standing 1.8 m tall
    static constexpr float kStandCentre = kHalfHeight + kRadius;
    // The feet: a walk of about 1.3 m/s, reached and left at no more than
    // kFootAccel (CHOSEN: an easy human start, ~0.25 g).
    static constexpr float kWalkMps = 1.3F;
    static constexpr float kFootAccel = 2.5F;
    // A walking player within kYieldM and closing: step aside kYieldStepM.
    static constexpr float kYieldM = 2.6F;
    static constexpr float kYieldStepM = 1.1F;
    // ... or sooner, kYieldLeadS before a faster walker would reach it; the
    // sidestep is quick (kDodgeMps, kDodgeAccel ~0.45 g), a person getting
    // out of the way.
    static constexpr float kYieldLeadS = 1.4F;
    static constexpr float kDodgeMps = 2.2F;
    static constexpr float kDodgeAccel = 4.5F;
    // A sprinting player (the walk tops out at 5.5 m/s, a sprint at 8)
    // faster than kRunMps within kRunStartleM, or landing at
    // kThudMps or faster within kThudM: flinch.
    static constexpr float kRunMps = 6.5F;
    static constexpr float kRunStartleM = 4.0F;
    static constexpr float kThudMps = 7.0F;
    static constexpr float kThudM = 12.0F;
    // A shove: the body's horizontal velocity this far from what the feet drove.
    static constexpr float kShoveMps = 1.2F;
    // Steps: an obstacle ahead lower than kStepMaxM is stepped onto by a lift
    // of kStepLiftMps (rising ~0.43 m), CHOSEN for the yard's 0.28 m kerbs.
    static constexpr float kStepMaxM = 0.4F;
    static constexpr float kStepLiftMps = 2.9F;

    struct View {
        float x = 0.0F, y = 0.0F, z = 0.0F;
        float yaw = 0.0F;          // radians; 0 faces +z
        State state = State::Walking;
        float speed = 0.0F;        // horizontal, m/s
        float stride = 0.0F;       // metres walked, for the legs' phase
        bool grounded = true;
    };

    Crew(JPH::PhysicsSystem &physics, std::uint32_t moving_layer);
    ~Crew();
    Crew(const Crew &) = delete;
    Crew &operator=(const Crew &) = delete;

    void pre_step(float dt, JPH::RVec3 player, JPH::Vec3 player_velocity, bool player_grounded);
    void post_step(float dt);

    [[nodiscard]] std::size_t count() const noexcept { return workers_.size(); }
    [[nodiscard]] const View &view(std::size_t index) const noexcept { return workers_[index].view; }
    [[nodiscard]] std::uint32_t yields() const noexcept { return yields_; }
    [[nodiscard]] std::uint32_t startles() const noexcept { return startles_; }
    [[nodiscard]] std::uint32_t stumbles() const noexcept { return stumbles_; }
    [[nodiscard]] std::uint32_t steps() const noexcept { return steps_; }
    [[nodiscard]] bool is_worker(JPH::BodyID body) const noexcept;

private:
    struct Station {
        float x, z;
        float dwell_s;   // 0: walk straight on
        float face_yaw;  // facing while working
    };
    struct Worker {
        JPH::BodyID body;
        std::vector<Station> route;
        std::size_t next = 0;
        float timer = 0.0F;
        float walk_mps = kWalkMps;
        JPH::Vec3 drive = JPH::Vec3::sZero();   // horizontal velocity the feet asked for
        JPH::Vec3 aside = JPH::Vec3::sZero();   // where a yield steps to
        State resume = State::Walking;
        float step_cooldown = 0.0F;
        View view;
    };

    void add(std::vector<Station> route, float walk_mps);
    [[nodiscard]] bool grounded(const Worker &worker, const JPH::BodyInterface &bodies) const;

    JPH::PhysicsSystem &physics_;
    JPH::RefConst<JPH::Shape> shape_;
    std::uint32_t moving_layer_;
    std::vector<Worker> workers_;
    float last_player_vy_ = 0.0F;
    std::uint32_t yields_ = 0;
    std::uint32_t startles_ = 0;
    std::uint32_t stumbles_ = 0;
    std::uint32_t steps_ = 0;
};

}  // namespace scraperx::sim
