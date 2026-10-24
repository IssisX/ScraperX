#pragma once

#include "sim/matron_traversal.hpp"
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
    static constexpr float kForce = traversal::Limits{}.total_force_n;
    static constexpr float kPower = traversal::Limits{}.power_w;
    static constexpr int kMaxNodes = 1536;
    static constexpr unsigned kPlanningExpansionsPerTick = 64;
    static constexpr unsigned kPlanningTransitionsPerTick = 128;
    static constexpr unsigned kPlanningQueriesPerTick = 64;
    static constexpr unsigned kPlanningNeighborChecksPerTick = 8192;
    struct Report {
        Status status = Status::Idle;
        PlanPhase phase = PlanPhase::None;
        unsigned plans = 0, acquisitions = 0, steps = 0, queries = 0, native_queries = 0;
        unsigned expansions = 0, slice_expansions = 0;
        // Search/validation credits; live support and the publication sweep
        // are separate safety checks and do not debit the A* query budget.
        unsigned slice_queries = 0, slice_neighbor_checks = 0;
        unsigned planning_ticks = 0, published_after_ticks = 0;
        float force = 0, peak_force = 0, peak_power = 0, max_reach = 0;
        JPH::Vec3 reaction = JPH::Vec3::sZero(); // static support's reaction at hand or feet
        JPH::RVec3 hand = JPH::RVec3::sZero();
    };
    // Acceptance starts planning; a route is usable only after phase is None.
    bool request(JPH::PhysicsSystem &, JPH::BodyID, JPH::RVec3 goal, JPH::Vec3 facing);
    // A replacement request needs the actual supporting hand, not a copy of
    // the pending catalog, open set, and route that request() will discard.
    MatronRoutecraft replacement() const;
    void step(JPH::PhysicsSystem &, JPH::BodyID, float dt,
              unsigned expansion_budget = kPlanningExpansionsPerTick,
              unsigned transition_budget = kPlanningTransitionsPerTick,
              unsigned query_budget = kPlanningQueriesPerTick);
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
    struct SearchState { Node node; int parent; float cost; bool closed; int open = -1; };
    struct Neighbor { int candidate; unsigned bucket; std::size_t offset; };
    struct Transfer {
        JPH::RVec3 from = JPH::RVec3::sZero();
        unsigned sample = 0, samples = 0;
        bool started = false, cleared = false;
    };
    enum class ProbeResult { Pending, Passed, Failed };
    enum class CandidatePhase { Charge, PostureClear, PostureReach, Transfer };
    struct Search {
        JPH::RVec3 start = JPH::RVec3::sZero();
        std::vector<Node> catalog;
        std::map<Cell, unsigned> cells;
        std::vector<std::vector<int>> buckets;
        std::vector<int> admitted;
        std::vector<std::int8_t> posture_valid;
        std::vector<SearchState> states;
        std::array<int, kMaxNodes> open{};
        unsigned open_count = 0;
        // At most 7^3 cells for the source, 3^3 for every other expansion.
        std::array<Neighbor, 343> neighbors{};
        unsigned neighbor_count = 0;
        std::size_t validated = 0;
        unsigned expansions = 0;
        int current = -1, candidate = -1;
        CandidatePhase candidate_phase = CandidatePhase::Charge;
        Transfer transfer;
        bool node_budget_exhausted = false;
    };
    bool plan(JPH::PhysicsSystem &, JPH::BodyID);
    void advance_plan(JPH::PhysicsSystem &, JPH::BodyID, unsigned, unsigned, unsigned &);
    void update_open(int);
    int take_open();
    bool earlier(int, int) const;
    void start_neighbors(const Node &, int);
    void consume_neighbor();
    void finish_search(Status);
    void replan(JPH::PhysicsSystem &, JPH::BodyID);
    Cell cell(JPH::RVec3) const;
    void hold(JPH::PhysicsSystem &, JPH::BodyID);
    void drive(JPH::PhysicsSystem &, JPH::BodyID, const Node &);
    bool valid(JPH::PhysicsSystem &, const Grip &) const;
    bool reach(JPH::PhysicsSystem &, JPH::BodyID, JPH::RVec3, const Grip &, bool count_budget = true) const;
    bool transition(JPH::PhysicsSystem &, JPH::BodyID, JPH::RVec3, const Node &, bool count_budget = true) const;
    ProbeResult resume_transition(JPH::PhysicsSystem &, JPH::BodyID, JPH::RVec3,
                                  const Node &, Transfer &, unsigned &, bool) const;
    bool clear(JPH::PhysicsSystem &, JPH::BodyID, JPH::RVec3, JPH::RVec3, bool count_budget = true) const;
    mutable Report report_;
    mutable bool incomplete_clearance_ = false;
    struct Failed { JPH::RVec3 from, to; };
    std::vector<Failed> failed_;
    bool budget() const { return ++report_.queries <= 200000; }
    bool blocked(JPH::RVec3 from, JPH::RVec3 to) const;
    JPH::RVec3 goal_ = JPH::RVec3::sZero(), last_ = JPH::RVec3::sZero();
    JPH::Vec3 facing_ = JPH::Vec3::sAxisZ();
    // Refreshed once per synchronous request/step, before any query. Avoid
    // two additional body read locks for every narrow-phase filter pair.
    JPH::ObjectLayer query_layer_ = 0;
    Search search_;
    std::optional<Node> holding_;
    std::vector<Node> route_;
    std::size_t cursor_ = 0;
    float stall_ = 0, elapsed_ = 0;
    unsigned retries_ = 0;
};
}
