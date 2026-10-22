#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <array>
#include <vector>

namespace scraperx::sim {
// Static, horizontal graspable box members; one upright translation-only
// capsule, one load-bearing grip. No moving supports or anatomical claims.
class MatronRoutecraft final {
public:
    enum class Status { Idle, Moving, Holding, NoRoute, Released, BudgetExceeded };
    static constexpr float kReach = 1.65F;
    static constexpr float kForce = 6000.0F;
    static constexpr float kPower = 9000.0F;
    static constexpr int kMaxNodes = 768;
    struct Report {
        Status status = Status::Idle;
        unsigned plans = 0, acquisitions = 0, steps = 0, queries = 0;
        float force = 0, peak_force = 0, peak_power = 0, max_reach = 0;
        JPH::Vec3 reaction = JPH::Vec3::sZero(); // static world's reaction at hand
        JPH::RVec3 hand = JPH::RVec3::sZero();
    };
    bool request(JPH::PhysicsSystem &, JPH::BodyID, JPH::RVec3 goal, JPH::Vec3 facing);
    void step(JPH::PhysicsSystem &, JPH::BodyID, float dt);
    void cancel() noexcept;
    bool owns_motion() const noexcept { return report_.status == Status::Moving || report_.status == Status::Holding; }
    const Report &report() const noexcept { return report_; }
private:
    struct Grip { JPH::BodyID body; JPH::SubShapeID sub; JPH::Vec3 local; JPH::RVec3 point; };
    struct Node { JPH::RVec3 center; Grip grip; };
    bool plan(JPH::PhysicsSystem &, JPH::BodyID);
    bool valid(JPH::PhysicsSystem &, const Grip &) const;
    bool reach(JPH::PhysicsSystem &, JPH::BodyID, JPH::RVec3, const Grip &) const;
    bool transition(JPH::PhysicsSystem &, JPH::BodyID, JPH::RVec3, const Node &) const;
    bool clear(JPH::PhysicsSystem &, JPH::BodyID, JPH::RVec3, JPH::RVec3) const;
    mutable Report report_;
    struct Failed { JPH::RVec3 from, to; };
    std::vector<Failed> failed_;
    bool budget() const { return ++report_.queries <= 200000; }
    bool blocked(JPH::RVec3 from, JPH::RVec3 to) const;
    JPH::RVec3 goal_ = JPH::RVec3::sZero(), last_ = JPH::RVec3::sZero();
    JPH::Vec3 facing_ = JPH::Vec3::sAxisZ();
    std::vector<Node> route_;
    std::size_t cursor_ = 0;
    float stall_ = 0, elapsed_ = 0;
    unsigned retries_ = 0;
};
}
