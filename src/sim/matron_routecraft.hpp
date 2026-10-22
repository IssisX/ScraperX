#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <array>
#include <map>
#include <optional>
#include <tuple>
#include <vector>

namespace scraperx::sim {
// Static, horizontal graspable box members; one upright translation-only
// capsule, one load-bearing grip. No moving supports or anatomical claims.
class MatronRoutecraft final {
public:
    // Moving includes an accepted request still being planned. Keep the
    // existing status values for callers; phase describes pending work.
    enum class Status { Idle, Moving, Holding, NoRoute, Released, BudgetExceeded };
    enum class PlanPhase { None, Search, Validate };
    static constexpr float kReach = 1.65F;
    static constexpr float kForce = 6000.0F;
    static constexpr float kPower = 9000.0F;
    static constexpr int kMaxNodes = 1536;
    static constexpr unsigned kPlanningExpansionsPerTick = 4;
    static constexpr unsigned kPlanningTransitionsPerTick = 4;
    struct Report {
        Status status = Status::Idle;
        PlanPhase phase = PlanPhase::None;
        unsigned plans = 0, acquisitions = 0, steps = 0, queries = 0, native_queries = 0;
        unsigned expansions = 0, slice_expansions = 0;
        float force = 0, peak_force = 0, peak_power = 0, max_reach = 0;
        JPH::Vec3 reaction = JPH::Vec3::sZero(); // static support's reaction at hand or feet
        JPH::RVec3 hand = JPH::RVec3::sZero();
    };
    // Acceptance starts planning; a route is usable only after phase is None.
    bool request(JPH::PhysicsSystem &, JPH::BodyID, JPH::RVec3 goal, JPH::Vec3 facing);
    void step(JPH::PhysicsSystem &, JPH::BodyID, float dt,
              unsigned expansion_budget = kPlanningExpansionsPerTick,
              unsigned transition_budget = kPlanningTransitionsPerTick);
    void cancel() noexcept;
    bool planning() const noexcept { return report_.phase != PlanPhase::None; }
    bool owns_motion() const noexcept {
        return (report_.status == Status::Moving || report_.status == Status::Holding) &&
               (!planning() || holding_.has_value());
    }
    const Report &report() const noexcept { return report_; }
private:
    friend struct MatronRoutecraftTestAccess;
    struct Grip { JPH::BodyID body; JPH::SubShapeID sub; JPH::Vec3 local; JPH::RVec3 point; };
    struct Node { JPH::RVec3 center; Grip grip; };
    using Cell = std::tuple<int, int, int>;
    static constexpr float kCellSize = 1.4F;
    struct SearchState { Node node; int parent; float cost; bool closed; };
    struct Search {
        JPH::RVec3 start = JPH::RVec3::sZero();
        std::vector<Node> catalog;
        std::map<Cell, std::vector<int>> cells;
        std::vector<int> admitted;
        std::vector<std::int8_t> posture_valid;
        std::vector<SearchState> states;
        std::vector<int> neighbors;
        std::size_t neighbor = 0, validated = 0;
        unsigned expansions = 0;
        int current = -1;
        bool node_budget_exhausted = false;
    };
    bool plan(JPH::PhysicsSystem &, JPH::BodyID);
    void advance_plan(JPH::PhysicsSystem &, JPH::BodyID, unsigned, unsigned);
    void finish_search(Status);
    void replan(JPH::PhysicsSystem &, JPH::BodyID);
    Cell cell(JPH::RVec3) const;
    void hold(JPH::PhysicsSystem &, JPH::BodyID);
    void drive(JPH::PhysicsSystem &, JPH::BodyID, const Node &);
    bool valid(JPH::PhysicsSystem &, const Grip &) const;
    bool reach(JPH::PhysicsSystem &, JPH::BodyID, JPH::RVec3, const Grip &, bool count_budget = true) const;
    bool transition(JPH::PhysicsSystem &, JPH::BodyID, JPH::RVec3, const Node &, bool count_budget = true) const;
    bool clear(JPH::PhysicsSystem &, JPH::BodyID, JPH::RVec3, JPH::RVec3, bool count_budget = true) const;
    mutable Report report_;
    mutable bool incomplete_clearance_ = false;
    struct Failed { JPH::RVec3 from, to; };
    std::vector<Failed> failed_;
    bool budget() const { return ++report_.queries <= 200000; }
    bool blocked(JPH::RVec3 from, JPH::RVec3 to) const;
    JPH::RVec3 goal_ = JPH::RVec3::sZero(), last_ = JPH::RVec3::sZero();
    JPH::Vec3 facing_ = JPH::Vec3::sAxisZ();
    Search search_;
    std::optional<Node> holding_;
    std::vector<Node> route_;
    std::size_t cursor_ = 0;
    float stall_ = 0, elapsed_ = 0;
    unsigned retries_ = 0;
};
}
