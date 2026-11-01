// A seeded descent through the production Matron: her own scan, planner,
// movement and recovery run every tick. The harness builds steel, supplies
// one bottom destination, removes one loaded rung, and observes physics.

#include "sim_tests.hpp"

#include "matron_exam.hpp"
#include "sim/matron.hpp"
#include "sim/matron_traversal.hpp"
#include "sim/physics_world_support.hpp"

#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace JPH;
using namespace scraperx::sim;
using namespace scraperx::sim::detail;
namespace exam = scraperx::sim::exam;
namespace traversal = scraperx::sim::traversal;

constexpr float kDt = exam::Judge::kTickSeconds;
constexpr int kSeeds = 12;
constexpr int kMaxTicks = 180 * 90;
constexpr float kForceToleranceN = 0.1F;
constexpr float kPoleSpeedToleranceMps = 0.25F;
constexpr float kRequiredPoleDescentM = 2.5F;

struct Course {
    float gap = 0.0F;
    float jump_drop = 0.0F;
    float rung_pitch = 0.0F;
    float rung_x = 0.0F;
    float pole_x = 0.0F;
    float ledge_top = 0.0F;
    float pole_foot_top = 0.0F;
    float recovery_top = 0.0F;
    float jump_top = 0.0F;
    float start_top = 0.0F;
    Vec3 origin = Vec3::sZero();
    int turns = 0;

    [[nodiscard]] float ladder_z() const { return gap + 3.05F; }
    [[nodiscard]] float pole_z() const { return gap + 6.10F; }
    [[nodiscard]] Vec3 bottom() const { return Vec3(0.0F, 0.0F, gap + 12.5F); }
    [[nodiscard]] Vec3 start() const { return Vec3(0.0F, start_top, -1.4F); }

    [[nodiscard]] Vec3 rotate(const Vec3 v) const {
        switch (turns) {
        case 1: return Vec3(-v.GetZ(), v.GetY(), v.GetX());
        case 2: return Vec3(-v.GetX(), v.GetY(), -v.GetZ());
        case 3: return Vec3(v.GetZ(), v.GetY(), -v.GetX());
        default: return v;
        }
    }
    [[nodiscard]] RVec3 world(const Vec3 local) const { return RVec3(origin + rotate(local)); }
    [[nodiscard]] Vec3 local(const RVec3 world_point) const {
        const Vec3 v = Vec3(world_point) - origin;
        switch (turns) {
        case 1: return Vec3(v.GetZ(), v.GetY(), -v.GetX());
        case 2: return Vec3(-v.GetX(), v.GetY(), -v.GetZ());
        case 3: return Vec3(-v.GetZ(), v.GetY(), v.GetX());
        default: return v;
        }
    }
    [[nodiscard]] AABox box(const Vec3 lo, const Vec3 hi) const {
        const Vec3 a(world(lo)), b(world(hi));
        return AABox(Vec3(std::min(a.GetX(), b.GetX()), std::min(a.GetY(), b.GetY()), std::min(a.GetZ(), b.GetZ())),
                     Vec3(std::max(a.GetX(), b.GetX()), std::max(a.GetY(), b.GetY()), std::max(a.GetZ(), b.GetZ())));
    }
};

Course course_for(const std::uint32_t seed) {
    std::uint32_t state = seed * 2654435761U + 0x9E3779B9U;
    const auto uniform = [&](const float lo, const float hi) {
        state = state * 1664525U + 1013904223U;
        const float unit = static_cast<float>(state >> 8U) / 16777216.0F;
        return lo + (hi - lo) * unit;
    };
    Course c;
    c.gap = uniform(0.80F, 1.10F);
    c.jump_drop = uniform(0.55F, 0.70F);
    c.rung_pitch = uniform(0.27F, 0.33F);
    c.rung_x = uniform(-0.35F, 0.35F);
    c.pole_x = uniform(-0.35F, 0.35F);
    c.ledge_top = uniform(0.80F, 0.90F);
    c.pole_foot_top = c.ledge_top + uniform(0.80F, 0.90F);
    // Each tall stage exceeds the ordinary 4.60 m hang-drop envelope. Her
    // scanner cannot replace the ladder or pole with a cheaper lip drop.
    c.recovery_top = c.pole_foot_top + uniform(4.75F, 4.90F);
    c.jump_top = c.recovery_top + uniform(4.75F, 4.90F);
    c.start_top = c.jump_top + c.jump_drop;
    c.origin = Vec3(uniform(30.0F, 50.0F), uniform(0.0F, 2.0F), uniform(30.0F, 50.0F));
    c.turns = static_cast<int>(seed % 4U);
    return c;
}

struct Rung {
    BodyID body;
    Vec3 at = Vec3::sZero();
};

struct World {
    JoltRuntimeLease lease;
    detail::BroadPhaseLayerInterface broad;
    ObjectVsBroadPhaseFilter vs;
    detail::ObjectLayerPairFilter pair;
    TempAllocatorImpl temp{32 * 1024 * 1024};
    JobSystemSingleThreaded jobs{1024};
    PhysicsSystem physics;
    Course course;
    std::vector<BodyID> bodies;
    std::vector<Rung> rungs;
    BodyID start_support, jump_support, recovery_support, pole, pole_foot_support, ledge_support, bottom_support;
    std::unique_ptr<Matron> matron;

    explicit World(const std::uint32_t seed) : course(course_for(seed)) {
        physics.Init(2048, 0, 4096, 2048, broad, vs, pair);
        physics.SetGravity(Vec3(0.0F, -9.81F, 0.0F));
        const Course &c = course;
        start_support = platform(-2.8F, 0.0F, c.start_top);
        jump_support = platform(c.gap, c.gap + 3.0F, c.jump_top);
        recovery_support = platform(c.gap + 3.0F, c.gap + 6.0F, c.recovery_top);
        pole_foot_support = platform(c.gap + 6.0F, c.gap + 9.0F, c.pole_foot_top);
        ledge_support = platform(c.gap + 9.0F, c.gap + 11.0F, c.ledge_top);
        bottom_support = platform(c.gap + 11.0F, c.gap + 14.0F, 0.0F);
        for (float y = c.recovery_top + 0.25F; y <= c.jump_top - 0.10F; y += c.rung_pitch) {
            const Vec3 at(c.rung_x, y, c.ladder_z());
            rungs.push_back(Rung{box(Vec3(0.55F, 0.025F, 0.025F), at), at});
        }
        const float pole_height = c.recovery_top - 0.05F - c.pole_foot_top;
        pole = box(Vec3(0.05F, 0.5F * pole_height, 0.05F),
                   Vec3(c.pole_x, c.pole_foot_top + 0.5F * pole_height, c.pole_z()));
        matron = std::make_unique<Matron>(physics, object_layers::kStatic, object_layers::kMoving,
                                        object_layers::kOnSteel, BodyID());
        matron->place_for_test(c.world(c.start()));
        matron->set_stage_for_test(Matron::Stage::Dangerous);
        // Initial condition: let gravity put her spawned capsule onto its
        // actual support before asking her to travel. No controller, pose or
        // velocity setter runs during these bounded setup steps.
        for (int tick = 0; tick < 18; ++tick) (void)physics.Update(kDt, 1, &temp, &jobs);
        const Vec3 settled_velocity = physics.GetBodyInterface().GetLinearVelocity(matron->body());
        if (!on(start_support) || settled_velocity.Length() >= exam::Judge::kStillMps ||
            std::abs(settled_velocity.GetY()) >= 0.1F ||
            physics.GetBodyInterface().GetGravityFactor(matron->body()) != 1.0F)
            throw std::runtime_error("descent exam: initial actor must stand on the top member under gravity");
    }

    ~World() {
        matron.reset();
        BodyInterface &b = physics.GetBodyInterface();
        for (const BodyID id : bodies) {
            if (b.IsAdded(id)) b.RemoveBody(id);
            b.DestroyBody(id);
        }
    }

    BodyID box(Vec3 half, const Vec3 at) {
        if ((course.turns & 1) != 0) half = Vec3(half.GetZ(), half.GetY(), half.GetX());
        const float convex = std::min(0.05F, 0.5F * half.ReduceMin());
        BodyCreationSettings settings(new BoxShape(half, convex), course.world(at), Quat::sIdentity(),
                                      EMotionType::Static, object_layers::kStatic);
        settings.mFriction = 0.8F;
        const BodyID id = physics.GetBodyInterface().CreateAndAddBody(settings, EActivation::DontActivate);
        if (id.IsInvalid()) throw std::runtime_error("descent exam: creating a real static member failed");
        bodies.push_back(id);
        return id;
    }

    BodyID platform(const float z0, const float z1, const float top) {
        return box(Vec3(1.5F, 0.10F, 0.5F * (z1 - z0)), Vec3(0.0F, top - 0.10F, 0.5F * (z0 + z1)));
    }

    [[nodiscard]] Vec3 feet() const {
        return course.local(physics.GetBodyInterfaceNoLock().GetPosition(matron->body()) -
                            Vec3(0.0F, Matron::kStandCentre, 0.0F));
    }

    // The same physical contact predicate as the lab judge: the actor's
    // actual shape lowered 3 cm meets this particular member from above.
    [[nodiscard]] bool on(const BodyID support) const {
        const BodyLockRead lock(physics.GetBodyLockInterfaceNoLock(), matron->body());
        if (!lock.Succeeded()) return false;
        const Body &her = lock.GetBody();
        AllHitCollisionCollector<CollideShapeCollector> hits;
        const IgnoreSingleBodyFilter not_her(matron->body());
        physics.GetNarrowPhaseQuery().CollideShape(
            her.GetShape(), Vec3::sReplicate(1.0F),
            her.GetCenterOfMassTransform().PostTranslated(Vec3(0.0F, -0.03F, 0.0F)), CollideShapeSettings(),
            her.GetCenterOfMassPosition(), hits, {}, {}, not_her);
        for (const CollideShapeResult &hit : hits.mHits) {
            if (hit.mBodyID2 == support && hit.mPenetrationAxis.NormalizedOr(Vec3::sZero()).GetY() <= -0.7F)
                return true;
        }
        return false;
    }

    [[nodiscard]] BodyID rung_at(const RVec3 world_hand) const {
        const Vec3 hand = course.local(world_hand);
        for (const Rung &r : rungs) {
            if (!physics.GetBodyInterfaceNoLock().IsAdded(r.body)) continue;
            const Vec3 d = hand - r.at;
            if (std::abs(d.GetX()) <= 0.58F && std::abs(d.GetY()) <= 0.055F && std::abs(d.GetZ()) <= 0.055F)
                return r.body;
        }
        return BodyID();
    }

    [[nodiscard]] bool hand_on_pole(const RVec3 world_hand) const {
        const Vec3 hand = course.local(world_hand);
        return std::abs(hand.GetX() - course.pole_x) <= 0.08F &&
               std::abs(hand.GetZ() - course.pole_z()) <= 0.08F &&
               hand.GetY() >= course.pole_foot_top - 0.03F && hand.GetY() <= course.recovery_top - 0.02F &&
               physics.GetBodyInterfaceNoLock().IsAdded(pole);
    }
};

struct DescentResult {
    exam::Verdict verdict;
    bool requested = false, plan_found = false, alive = true, finite = true, force_ok = true, impulse_free = true;
    bool jump_up = false, jump_down = false, gap_crossed = false, jump_landed = false;
    bool rung_removed = false, loss_seen = false, fell_after_loss = false, loss_landed = false, replanned = false;
    bool pole_controlled = true, pole_finished = false, ledge_landed = false;
    float peak_actual_n = 0.0F, peak_declared_n = 0.0F, peak_power_w = 0.0F, worst_step_m = 0.0F;
    float jump_rise_m = 0.0F, ladder_down_m = 0.0F, fall_mps = 0.0F;
    float pole_down_m = 0.0F, pole_max_mps = 0.0F, pole_peak_vertical_n = 0.0F;
    int jump_land_ticks = 0, ticks = 0, landing_tick = -1, replan_tick = -1;
    std::uint32_t replans = 0, failures = 0;
    std::string reason;

    [[nodiscard]] bool passed() const {
        return requested && plan_found && alive && finite && force_ok && impulse_free &&
               worst_step_m < exam::Judge::kMaxStepM && verdict.arrived &&
               jump_up && jump_down && gap_crossed && jump_landed && ladder_down_m >= 0.5F &&
               rung_removed && loss_seen && fell_after_loss && loss_landed && replanned &&
               pole_controlled && pole_finished && pole_down_m >= kRequiredPoleDescentM && ledge_landed;
    }
};

bool finite_vec(const Vec3 v) {
    return std::isfinite(v.GetX()) && std::isfinite(v.GetY()) && std::isfinite(v.GetZ());
}

DescentResult descend(World &w) {
    DescentResult out;
    const traversal::Limits limits;
    const Course &c = w.course;
    exam::Judge judge(w.bottom_support,
                      c.box(Vec3(-1.45F, -0.06F, c.gap + 11.0F), Vec3(1.45F, 0.06F, c.gap + 14.0F)),
                      limits.total_force_n, Matron::kStandCentre);
    out.requested = true;
    out.plan_found = w.matron->travel_to(Vec3(c.world(c.bottom())));
    if (!out.plan_found) {
        out.reason = "her initial scan/planner found no route to the bottom";
        out.verdict = judge.verdict();
        return out;
    }
    RVec3 last_position = w.physics.GetBodyInterface().GetPosition(w.matron->body());
    Vec3 last_feet = w.feet();
    bool flight_started = false;
    float flight_start_y = 0.0F;
    std::uint32_t failures_seen = 0, loss_failures = 0, landing_replans = 0;
    std::uint32_t previous_replans = w.matron->journey().replans;
    int landing_region = -1;

    for (int tick = 0; tick < kMaxTicks && !judge.done(); ++tick) {
        const traversal::Climber *before = w.matron->climber();
        if (out.plan_found && !out.rung_removed && out.jump_landed && before != nullptr &&
            before->view().holding && before->view().kind == traversal::MoveKind::RungDown &&
            before->view().force_n > 0.2F * limits.mass_kg * 9.81F && out.ladder_down_m >= 0.5F &&
            last_feet.GetY() >= c.recovery_top + 1.0F && last_feet.GetY() <= c.recovery_top + 1.8F) {
            const BodyID held = w.rung_at(before->view().hand);
            if (!held.IsInvalid()) {
                w.physics.GetBodyInterface().RemoveBody(held);
                out.rung_removed = true;
                loss_failures = w.matron->journey().moves_failed;
            }
        }

        const Vec3 velocity_before = w.physics.GetBodyInterface().GetLinearVelocity(w.matron->body());
        w.matron->pre_step(kDt, Stimulus(RVec3(1000.0, 1000.0, 1000.0), 0.0F));
        const Vec3 commanded_velocity = w.physics.GetBodyInterface().GetLinearVelocity(w.matron->body());
        Vec3 actual_force = Vec3::sZero();
        {
            const BodyLockRead lock(w.physics.GetBodyLockInterface(), w.matron->body());
            if (!lock.Succeeded()) throw std::runtime_error("descent exam: reading the actor's tick force failed");
            actual_force = lock.GetBody().GetAccumulatedForce();
        }
        const traversal::Climber *climber = w.matron->climber();
        const traversal::ClimbView view = climber != nullptr ? climber->view() : traversal::ClimbView{};
        const float actual_n = actual_force.Length();
        out.peak_actual_n = std::max(out.peak_actual_n, actual_n);
        out.peak_declared_n = std::max(out.peak_declared_n, view.force_n);
        out.peak_power_w = std::max(out.peak_power_w, view.power_w);
        out.finite = out.finite && finite_vec(actual_force) && finite_vec(commanded_velocity) &&
                     std::isfinite(view.force_n) && std::isfinite(view.power_w);
        out.force_ok = out.force_ok && actual_n <= limits.total_force_n + kForceToleranceN &&
                       view.force_n >= -kForceToleranceN && view.force_n <= limits.total_force_n + kForceToleranceN &&
                       view.power_w <= limits.power_w + 0.1F;
        // An impulse or direct velocity write before integration cannot hide
        // behind a zero AddForce total. Initial placement happened before this loop.
        out.impulse_free = out.impulse_free && (commanded_velocity - velocity_before).Length() <= 1.0e-4F;

        const Matron::Journey journey = w.matron->journey();
        if (journey.moves_failed > failures_seen) {
            judge.note_failure();
            failures_seen = journey.moves_failed;
        }
        if (out.rung_removed && journey.moves_failed > loss_failures &&
            journey.last_failure == traversal::Failure::LostHold) out.loss_seen = true;
        const bool replan_event = journey.replans > previous_replans;
        if (out.loss_landed && replan_event && journey.replans > landing_replans &&
            journey.region == landing_region && tick > out.landing_tick && w.on(w.recovery_support)) {
            out.replanned = true;
            if (out.replan_tick < 0) out.replan_tick = tick;
        }
        previous_replans = journey.replans;

        const bool holding_pole = view.holding && view.kind == traversal::MoveKind::PoleDown &&
                                  w.hand_on_pole(view.hand);
        if (holding_pole && last_feet.GetY() < c.recovery_top - 0.20F &&
            last_feet.GetY() > c.pole_foot_top + 0.20F) {
            const float squeeze = std::min(limits.pole_squeeze_n, limits.total_force_n * 0.8F);
            const float mu = view.sliding ? limits.pole_mu_kinetic : limits.pole_mu_static;
            out.pole_peak_vertical_n = std::max(out.pole_peak_vertical_n, std::abs(actual_force.GetY()));
            out.pole_controlled = out.pole_controlled && std::abs(actual_force.GetY()) <= mu * squeeze + kForceToleranceN;
        }

        (void)w.physics.Update(kDt, 1, &w.temp, &w.jobs);
        w.matron->post_step(kDt);
        const RVec3 position = w.physics.GetBodyInterface().GetPosition(w.matron->body());
        const Vec3 velocity = w.physics.GetBodyInterface().GetLinearVelocity(w.matron->body());
        const Vec3 feet = w.feet();
        out.finite = out.finite && finite_vec(Vec3(position)) && finite_vec(velocity);
        out.alive = out.alive && w.matron->view().alive;
        out.worst_step_m = std::max(out.worst_step_m, float((position - last_position).Length()));
        const traversal::Climber *after = w.matron->climber();
        const bool held_after = (after != nullptr && after->view().holding) || w.matron->view().climbing;
        judge.tick(w.physics, w.matron->body(), std::max(actual_n, view.force_n), held_after, kDt);
        out.ticks = tick + 1;
        out.replans = w.matron->journey().replans;
        out.failures = w.matron->journey().moves_failed;

        if (out.plan_found && !out.jump_landed) {
            const bool on_start = w.on(w.start_support);
            const bool on_destination = w.on(w.jump_support);
            const bool unsupported = !on_start && !on_destination && !held_after && !view.leg_driving;
            if (!flight_started && unsupported && velocity.GetY() > 0.20F &&
                feet.GetZ() >= -0.80F && feet.GetZ() < c.gap) {
                flight_started = true;
                flight_start_y = feet.GetY();
                out.jump_up = true;
            }
            if (flight_started && unsupported) {
                out.jump_rise_m = std::max(out.jump_rise_m, feet.GetY() - flight_start_y);
                out.jump_down = out.jump_down || velocity.GetY() < -0.20F;
                out.gap_crossed = out.gap_crossed ||
                                  (feet.GetZ() > 0.15F && feet.GetZ() < c.gap - 0.15F && feet.GetY() > c.jump_top);
            }
            const bool landed = flight_started && out.jump_down && out.gap_crossed && on_destination &&
                                !held_after && std::abs(velocity.GetY()) < 0.25F;
            out.jump_land_ticks = landed ? out.jump_land_ticks + 1 : 0;
            out.jump_landed = out.jump_land_ticks >= 6;
        }

        if (view.holding && view.kind == traversal::MoveKind::RungDown && !w.rung_at(view.hand).IsInvalid())
            out.ladder_down_m += std::max(0.0F, last_feet.GetY() - feet.GetY());

        if (out.rung_removed && !out.loss_landed) {
            out.fall_mps = std::max(out.fall_mps, std::max(0.0F, -velocity.GetY()));
            out.fell_after_loss = out.fell_after_loss || (!held_after && velocity.GetY() < -0.50F);
            if (out.loss_seen && out.fell_after_loss && !held_after && w.on(w.recovery_support) &&
                std::abs(velocity.GetY()) < 0.30F) {
                out.loss_landed = true;
                out.landing_tick = tick;
                landing_replans = w.matron->journey().replans;
                const traversal::AffordanceMap *map = w.matron->scanned_routes();
                if (map != nullptr)
                    landing_region = map->region_at(Vec3(c.world(Vec3(feet.GetX(), c.recovery_top, feet.GetZ()))));
            }
        }

        if (holding_pole) {
            out.pole_down_m += std::max(0.0F, last_feet.GetY() - feet.GetY());
            out.pole_max_mps = std::max(out.pole_max_mps, std::max(0.0F, -velocity.GetY()));
            out.pole_controlled = out.pole_controlled &&
                                  -velocity.GetY() <= limits.pole_down_mps + kPoleSpeedToleranceMps;
        } else if (out.loss_landed && feet.GetY() < c.recovery_top - 0.6F &&
                   feet.GetY() > c.pole_foot_top + 0.3F &&
                   std::hypot(feet.GetX() - c.pole_x, feet.GetZ() - c.pole_z()) < 0.9F &&
                   !w.on(w.recovery_support) && !w.on(w.pole_foot_support)) {
            out.pole_controlled = false;
        }
        if (out.replanned && out.pole_down_m >= kRequiredPoleDescentM && !held_after && w.on(w.pole_foot_support))
            out.pole_finished = true;
        if (out.pole_finished && !held_after && w.on(w.ledge_support)) out.ledge_landed = true;

        last_position = position;
        last_feet = feet;
        if (!out.finite || !out.force_ok || !out.impulse_free || !out.alive ||
            out.worst_step_m >= exam::Judge::kMaxStepM) break;
    }
    out.verdict = judge.verdict();
    if (out.passed()) out.reason = "all required events and supported bottom arrival";
    else if (!out.finite) out.reason = "non-finite physical state or force";
    else if (!out.force_ok) out.reason = "actual/declared actuator cap exceeded";
    else if (!out.impulse_free) out.reason = "velocity changed before physics integration";
    else if (!out.alive || !out.verdict.alive) out.reason = "did not survive";
    else if (out.worst_step_m >= exam::Judge::kMaxStepM) out.reason = "step reached the 0.20 m limit";
    else if (!out.reason.empty()) {}
    else if (!out.jump_landed) out.reason = "no physical descending gap jump and supported destination";
    else if (!out.rung_removed) out.reason = "loaded descent rung never reached for the physical fault";
    else if (!out.loss_seen || !out.fell_after_loss) out.reason = "rung removal did not produce observed hold loss and a fall";
    else if (!out.loss_landed || !out.replanned) out.reason = "no runtime replan after the actual intermediate landing";
    else if (!out.pole_controlled || !out.pole_finished) out.reason = "pole descent lacked controlled friction or supported exit";
    else if (!out.ledge_landed) out.reason = "lower ledge was not reached on real support";
    else out.reason = out.verdict.reason == "not yet" ? "no one-second ungripped standing arrival before timeout" : out.verdict.reason;
    return out;
}

struct GapObstructionResult {
    bool fixture_intersects = false, planned = false, rejected = false, blocked_edge = false;
    bool supported = true, alive = true, lawful = true, impulse_free = true, never_launched = true;
    float peak_actual_n = 0.0F, peak_declared_n = 0.0F, peak_power_w = 0.0F, worst_step_m = 0.0F;
    int ticks = 0;
    std::string reason;

    [[nodiscard]] bool passed(const bool after_plan) const {
        return fixture_intersects && planned == after_plan && rejected && (after_plan || blocked_edge) &&
               supported && alive && lawful && impulse_free && never_launched &&
               worst_step_m < exam::Judge::kMaxStepM;
    }
};

GapObstructionResult obstructed_gap(const bool after_plan) {
    World w(1);
    GapObstructionResult out;
    const Course &c = w.course;
    const traversal::Limits limits;
    BodyInterface &bodies = w.physics.GetBodyInterface();
    // A 3.4 m crossbeam spans the whole 3 m deck width. Its 4 cm section
    // cannot be a standing patch, and its width exceeds the scanner's rung
    // envelope. It only obstructs the lower capsule in the open gap: its
    // top is 18 cm above the starting floor, around the short hop's apex.
    const auto add_beam = [&]() {
        return w.box(Vec3(1.70F, 0.02F, 0.02F), Vec3(0.0F, c.start_top + 0.16F, 0.5F * c.gap));
    };
    BodyID beam;
    if (!after_plan) beam = add_beam();
    const Vec3 initial_velocity = bodies.GetLinearVelocity(w.matron->body());
    const RVec3 initial_position = bodies.GetPosition(w.matron->body());
    out.planned = w.matron->travel_to(Vec3(c.world(c.bottom())));
    out.impulse_free = (bodies.GetLinearVelocity(w.matron->body()) - initial_velocity).Length() <= 1.0e-4F &&
                       (bodies.GetPosition(w.matron->body()) - initial_position).Length() <= 1.0e-6F;
    if (after_plan) beam = add_beam();

    // Prove the fixture intersects her actual shape's lower cap without
    // moving her body: query a hypothetical mid-gap pose at feet +16 cm.
    {
        const BodyLockRead lock(w.physics.GetBodyLockInterface(), w.matron->body());
        if (!lock.Succeeded()) throw std::runtime_error("descent obstruction: reading the actor shape failed");
        const RVec3 at = c.world(Vec3(0.0F, c.start_top + 0.16F + Matron::kStandCentre, 0.5F * c.gap));
        AllHitCollisionCollector<CollideShapeCollector> hits;
        const IgnoreSingleBodyFilter not_her(w.matron->body());
        w.physics.GetNarrowPhaseQuery().CollideShape(
            lock.GetBody().GetShape(), Vec3::sReplicate(1.0F), RMat44::sTranslation(at),
            CollideShapeSettings(), at, hits, {}, {}, not_her);
        for (const CollideShapeResult &hit : hits.mHits)
            out.fixture_intersects = out.fixture_intersects || hit.mBodyID2 == beam;
    }
    const traversal::AffordanceMap *map = w.matron->scanned_routes();
    if (!after_plan && map != nullptr) {
        const int from = map->region_at(Vec3(c.world(c.start())));
        const int to = map->region_at(Vec3(c.world(Vec3(0.0F, c.jump_top, c.gap + 1.5F))));
        for (const traversal::Edge &edge : map->edges())
            out.blocked_edge = out.blocked_edge || (edge.from == from && edge.to == to && !edge.usable);
        for (const traversal::Patch &patch : map->patches())
            if (patch.body == beam) throw std::runtime_error("descent obstruction: beam must not supply a new standing patch");
    }
    out.supported = w.on(w.start_support);
    out.alive = w.matron->view().alive;
    out.lawful = bodies.GetGravityFactor(w.matron->body()) == 1.0F;
    if (!after_plan) out.rejected = !out.planned;
    const std::uint32_t failures_before = w.matron->journey().moves_failed;
    RVec3 last_position = bodies.GetPosition(w.matron->body());
    constexpr int kObstructionTicks = 15 * 90;
    for (int tick = 0; after_plan && out.planned && !out.rejected && tick < kObstructionTicks; ++tick) {
        const Vec3 velocity_before = bodies.GetLinearVelocity(w.matron->body());
        w.matron->pre_step(kDt, Stimulus(RVec3(1000.0, 1000.0, 1000.0), 0.0F));
        const Vec3 commanded_velocity = bodies.GetLinearVelocity(w.matron->body());
        Vec3 force = Vec3::sZero();
        {
            const BodyLockRead lock(w.physics.GetBodyLockInterface(), w.matron->body());
            if (!lock.Succeeded()) throw std::runtime_error("descent obstruction: reading the actor force failed");
            force = lock.GetBody().GetAccumulatedForce();
        }
        const traversal::Climber *climber = w.matron->climber();
        const traversal::ClimbView view = climber != nullptr ? climber->view() : traversal::ClimbView{};
        out.never_launched = out.never_launched && view.launches == 0 && !view.leg_driving && !view.holding;
        out.impulse_free = out.impulse_free && (commanded_velocity - velocity_before).Length() <= 1.0e-4F;
        out.peak_actual_n = std::max(out.peak_actual_n, force.Length());
        out.peak_declared_n = std::max(out.peak_declared_n, view.force_n);
        out.peak_power_w = std::max({out.peak_power_w, view.power_w, std::max(0.0F, force.Dot(commanded_velocity))});
        out.lawful = out.lawful && finite_vec(force) && finite_vec(commanded_velocity) &&
                     std::isfinite(view.force_n) && std::isfinite(view.power_w) &&
                     force.Length() <= limits.total_force_n + kForceToleranceN &&
                     view.force_n >= -kForceToleranceN && view.force_n <= limits.total_force_n + kForceToleranceN &&
                     bodies.GetGravityFactor(w.matron->body()) == 1.0F;
        const Matron::Journey journey = w.matron->journey();
        out.rejected = journey.moves_failed > failures_before && journey.last_failure == traversal::Failure::BlockedSweep;
        out.supported = out.supported && w.on(w.start_support);

        (void)w.physics.Update(kDt, 1, &w.temp, &w.jobs);
        w.matron->post_step(kDt);
        const RVec3 position = bodies.GetPosition(w.matron->body());
        const Vec3 velocity = bodies.GetLinearVelocity(w.matron->body());
        out.peak_power_w = std::max(out.peak_power_w, std::max(0.0F, force.Dot(velocity)));
        out.lawful = out.lawful && finite_vec(Vec3(position)) && finite_vec(velocity) &&
                     out.peak_power_w <= limits.power_w + 0.1F && bodies.GetGravityFactor(w.matron->body()) == 1.0F;
        out.worst_step_m = std::max(out.worst_step_m, float((position - last_position).Length()));
        out.supported = out.supported && w.on(w.start_support);
        out.alive = out.alive && w.matron->view().alive;
        last_position = position;
        out.ticks = tick + 1;
        if (!out.supported || !out.alive || !out.lawful || !out.impulse_free || !out.never_launched ||
            out.worst_step_m >= exam::Judge::kMaxStepM) break;
    }
    if (out.passed(after_plan)) out.reason = after_plan ? "new obstruction refused on real top support before leg drive"
                                                       : "initial own scan refuses the obstructed gap";
    else if (!out.fixture_intersects) out.reason = "beam did not intersect the actual lower capsule fixture pose";
    else if (out.planned != after_plan) out.reason = after_plan ? "clean initial route was not accepted"
                                                             : "initial scan accepted an obstructed gap";
    else if (!after_plan && !out.blocked_edge) out.reason = "own scan did not identify the blocked gap edge";
    else if (!out.never_launched) out.reason = "leg drive or launch began despite the new obstruction";
    else if (!out.impulse_free) out.reason = "velocity or position changed outside physics integration";
    else if (!out.lawful) out.reason = "non-finite state, force/power cap or gravity violation";
    else if (!out.supported) out.reason = "left real top support before refusing the obstruction";
    else if (!out.alive) out.reason = "did not remain alive";
    else if (out.worst_step_m >= exam::Judge::kMaxStepM) out.reason = "step reached the 0.20 m limit";
    else out.reason = "no runtime blocked-sweep refusal within 15 seconds";
    return out;
}

void rejected_pending_requests() {
    World w(1);
    BodyInterface &bodies = w.physics.GetBodyInterface();
    require(w.matron->travel_to(Vec3(w.course.world(w.course.bottom()))),
            "descent request: initial own scan finds the destination");
    const int goal = w.matron->journey().goal;
    const int planned_steps = w.matron->journey().steps;
    // Losing the starting support leaves her freely falling. This request
    // must wait for a physical landing without committing a guessed floor.
    bodies.RemoveBody(w.start_support);
    (void)w.physics.Update(kDt, 1, &w.temp, &w.jobs);
    require(w.matron->travel_to(goal), "descent request: a free airborne body accepts a pending destination");
    const Matron::Journey pending = w.matron->journey();
    const traversal::Climber *controller = w.matron->climber();
    const RVec3 position = bodies.GetPosition(w.matron->body());
    const Vec3 velocity = bodies.GetLinearVelocity(w.matron->body());
    require(pending.active && pending.goal == goal && pending.region == -1 && pending.step == 0 &&
                pending.steps == planned_steps && pending.steps > 0 &&
                controller != nullptr && controller->status() == traversal::Climber::Status::Idle &&
                !controller->view().holding && velocity.GetY() < 0.0F,
            "descent request: a provisional plan owns no grip or unsupported region");
    const int past_end = static_cast<int>(w.matron->scanned_routes()->regions().size());
    for (const int invalid : {-1, past_end}) {
        require(!w.matron->travel_to(invalid), "descent request: invalid destination is rejected");
        const Matron::Journey after = w.matron->journey();
        require(w.matron->climber() == controller && after.active == pending.active && after.goal == pending.goal &&
                    after.region == pending.region && after.step == pending.step && after.steps == pending.steps &&
                    after.moves_done == pending.moves_done && after.moves_failed == pending.moves_failed &&
                    after.replans == pending.replans && after.arrivals == pending.arrivals && after.yields == pending.yields &&
                    after.move == pending.move && after.last_failure == pending.last_failure &&
                    (bodies.GetPosition(w.matron->body()) - position).Length() == 0.0 &&
                    (bodies.GetLinearVelocity(w.matron->body()) - velocity).Length() == 0.0F &&
                    bodies.GetGravityFactor(w.matron->body()) == 1.0F,
                "descent request: rejection preserves the pending controller, journey and physical state");
    }
    std::cout << "PASS scraperx_sim matron descent pending request: free fall accepted, invalid destinations preserve state\n";
}


void reject_free_drop_through_overhead_steel() {
    World world(4);
    (void)world.box(Vec3(10.0F, 0.10F, 10.0F), Vec3(40.0F, -0.10F, 0.0F));
    const BodyID upper = world.box(Vec3(2.5F, 0.10F, 2.5F), Vec3(40.0F, 10.90F, 0.0F));
    // Four steel beams intercept the actual falling arc but sit ABOVE the
    // straight exit-to-landing diagonal. The scanner must not mistake a
    // collision-free diagonal for a collision-free ballistic flight.
    for (const float sign : {-1.0F, 1.0F}) {
        (void)world.box(Vec3(0.14F, 0.16F, 2.5F), Vec3(40.0F + sign * 4.4F, 9.50F, 0.0F));
        (void)world.box(Vec3(2.5F, 0.16F, 0.14F), Vec3(40.0F, 9.50F, sign * 4.4F));
    }
    world.matron->place_for_test(world.course.world(Vec3(40.0F, 11.0F, 0.0F)));
    for (int tick = 0; tick < 40; ++tick)
        (void)world.physics.Update(kDt, 1, &world.temp, &world.jobs);
    require(world.on(upper), "overhead steel: actor must start on upper support");
    (void)world.matron->travel_to(Vec3(world.course.world(Vec3(40.0F, 0.0F, 0.0F))));
    const traversal::AffordanceMap *scan = world.matron->scanned_routes();
    require(scan != nullptr, "overhead steel: real scanner must run");
    const float upper_y = float(world.course.world(Vec3(40.0F, 11.0F, 0.0F)).GetY());
    const float lower_y = float(world.course.world(Vec3(40.0F, 0.0F, 0.0F)).GetY());
    for (const traversal::Edge &edge : scan->edges()) {
        require(!(edge.kind == traversal::MoveKind::FreeDrop && edge.usable &&
                  std::abs(edge.spec.top_y - upper_y) < 0.10F &&
                  std::abs(edge.spec.goal_top - lower_y) < 0.10F),
                "overhead steel: reject a free drop whose actual flight intersects a steel beam");
    }
    std::cout << "PASS scraperx_sim matron safe drop: overhead steel blocks every flight corridor\n";
}

void survivable_free_drop() {
    // A deck directly above a larger grade slab has no ladder. This is
    // deliberately an 11 m drop; the ordinary hang/drop limit is 4.60 m.
    // The same course with a narrow grade slab must reject the move.
    for (const bool landing_available : {false, true}) {
        World world(4);
        const BodyID grade = world.box(
            Vec3(landing_available ? 10.0F : 2.9F, 0.10F, landing_available ? 10.0F : 2.9F),
            Vec3(40.0F, -0.10F, 0.0F));
        const BodyID upper = world.box(Vec3(2.5F, 0.10F, 2.5F), Vec3(40.0F, 10.90F, 0.0F));
        world.matron->place_for_test(world.course.world(Vec3(40.0F, 11.0F, 0.0F)));
        for (int tick = 0; tick < 40; ++tick) (void)world.physics.Update(kDt, 1, &world.temp, &world.jobs);
        require(world.on(upper), "safe drop: actor must start on real upper support");
        require(world.physics.GetBodyInterface().GetGravityFactor(world.matron->body()) == 1.0F,
                "safe drop: gravity is active at departure");
        const bool planned = world.matron->travel_to(
            Vec3(world.course.world(Vec3(40.0F, 0.0F, 0.0F))));
        const traversal::AffordanceMap *scan = world.matron->scanned_routes();
        const float expected_upper_y = float(world.course.world(Vec3(40.0F, 11.0F, 0.0F)).GetY());
        const float expected_lower_y = float(world.course.world(Vec3(40.0F, 0.0F, 0.0F)).GetY());
        bool offered_drop = false;
        for (const traversal::Edge &edge : scan->edges()) {
            if (edge.kind == traversal::MoveKind::FreeDrop && edge.usable &&
                std::abs(edge.spec.top_y - expected_upper_y) < 0.10F &&
                std::abs(edge.spec.goal_top - expected_lower_y) < 0.10F) {
                offered_drop = true;
            }
        }
        if (!landing_available) {
            require(!planned && !offered_drop,
                    "safe drop: no route when the landing is too narrow for the ballistic runout");
            continue;
        }
        if (!planned || !offered_drop) {
            const Vec3 start_feet(world.course.world(Vec3(40.0F, 11.0F, 0.0F)));
            const Vec3 landing_feet(world.course.world(Vec3(40.0F, 0.0F, 0.0F)));
            std::cerr << "SAFE_DROP_DIAG planned=" << planned << " edge=" << offered_drop
                      << " upper_region=" << scan->region_at(start_feet)
                      << " lower_region=" << scan->region_at(landing_feet)
                      << " patches=" << scan->stats().patches
                      << " regions=" << scan->stats().regions
                      << " edges=" << scan->stats().edges
                      << " usable_edges=" << scan->stats().usable_edges << '\n';
            for (const traversal::Edge &edge : scan->edges()) {
                if (edge.kind != traversal::MoveKind::FreeDrop) continue;
                std::cerr << "SAFE_DROP_EDGE from=" << edge.from << " to=" << edge.to
                          << " top=" << edge.spec.top_y << " goal=" << edge.spec.goal_top
                          << " usable=" << edge.usable << " reason=" << edge.reason << '\n';
            }
        }
        require(planned && offered_drop,
                "safe drop: a clear static landing 11 m below gives a directed physical route");
        const traversal::Limits limits;
        bool used_drop = false, airborne = false, landed = false, alive = true;
        float worst_step = 0.0F, greatest_fall = 0.0F;
        RVec3 last = world.physics.GetBodyInterface().GetPosition(world.matron->body());
        std::uint32_t max_arrivals = 0;
        for (int tick = 0; tick < 90 * 45; ++tick) {
            const Vec3 before = world.physics.GetBodyInterface().GetLinearVelocity(world.matron->body());
            world.matron->pre_step(kDt, Stimulus(RVec3(1000.0, 1000.0, 1000.0), 0.0F));
            const Vec3 after = world.physics.GetBodyInterface().GetLinearVelocity(world.matron->body());
            require((after - before).Length() <= 1.0e-4F,
                    "safe drop: no controller velocity write");
            const traversal::Climber *climber = world.matron->climber();
            if (climber != nullptr && climber->view().kind == traversal::MoveKind::FreeDrop &&
                climber->view().active) used_drop = true;
            {
                const BodyLockRead lock(world.physics.GetBodyLockInterface(), world.matron->body());
                require(lock.Succeeded(), "safe drop: active body must remain readable");
                require(lock.GetBody().GetAccumulatedForce().Length() <= limits.total_force_n + 0.1F,
                        "safe drop: all movement forces stay in one budget");
            }
            (void)world.physics.Update(kDt, 1, &world.temp, &world.jobs);
            world.matron->post_step(kDt);
            alive = world.matron->view().alive;
            if (!alive) break;
            const RVec3 now = world.physics.GetBodyInterface().GetPosition(world.matron->body());
            const Vec3 velocity = world.physics.GetBodyInterface().GetLinearVelocity(world.matron->body());
            worst_step = std::max(worst_step, float((now - last).Length()));
            greatest_fall = std::max(greatest_fall, -velocity.GetY());
            last = now;
            require(world.physics.GetBodyInterface().GetGravityFactor(world.matron->body()) == 1.0F,
                    "safe drop: gravity stays on during the descent");
            airborne = airborne || (!world.on(upper) && !world.on(grade) && velocity.GetY() < -1.0F);
            max_arrivals = std::max(max_arrivals, world.matron->journey().arrivals);
            if (max_arrivals > 0 && world.on(grade)) {
                landed = true;
                break;
            }
        }
        require(alive && used_drop && airborne && landed && max_arrivals > 0,
                "safe drop: actual airborne free fall survives and arrives on real lower support");
        require(greatest_fall < Matron::kLethalLandingMps && greatest_fall > Matron::kHardLandingMps,
                "safe drop: impact speed is hard but survivable under the existing landing law");
        require(worst_step < 0.20F, "safe drop: no position snap over a 90 Hz physics tick");
        std::cout << "PASS scraperx_sim matron safe drop: 11 m, airborne, grounded, alive; worst_step="
                  << worst_step << " peak_descent=" << greatest_fall << '\n';
    }
}


} // namespace

void run_matron_exam_descent() {
    rejected_pending_requests();
    reject_free_drop_through_overhead_steel();
    survivable_free_drop();
    int passed = 0;
    for (std::uint32_t seed = 1; seed <= kSeeds; ++seed) {
        const Course c = course_for(seed);
        DescentResult result;
        try {
            World world(seed);
            result = descend(world);
        } catch (const std::exception &error) {
            result.reason = std::string("fixture/runtime exception: ") + error.what();
        }
        const bool success = result.passed();
        passed += success ? 1 : 0;
        std::cout << (success ? "PASS" : "FAIL") << " scraperx_sim matron exam descent seed=" << seed
                  << std::fixed << std::setprecision(4) << " height=" << c.start_top << " gap=" << c.gap
                  << " jump_drop=" << c.jump_drop << " quarter_turns=" << c.turns
                  << " planned=" << result.plan_found << " jump=" << result.jump_landed
                  << " airborne_up_down=" << (result.jump_up && result.jump_down)
                  << " gap_crossed=" << result.gap_crossed << " jump_rise=" << result.jump_rise_m
                  << " ladder_down=" << result.ladder_down_m << " removed=" << result.rung_removed
                  << " lost=" << result.loss_seen << " fall_mps=" << result.fall_mps
                  << " loss_landed=" << result.loss_landed << " replanned=" << result.replanned
                  << " landing_tick=" << result.landing_tick << " replan_tick=" << result.replan_tick
                  << " pole_down=" << result.pole_down_m << " pole_max_mps=" << result.pole_max_mps
                  << " pole_vertical_n=" << result.pole_peak_vertical_n << " pole_controlled=" << result.pole_controlled
                  << " ledge=" << result.ledge_landed << " actual_n=" << result.peak_actual_n
                  << " declared_n=" << result.peak_declared_n << " positive_w=" << result.peak_power_w
                  << " worst_step=" << result.worst_step_m << " supported_s=" << result.verdict.supported_s
                  << " alive=" << (result.alive && result.verdict.alive) << " replans=" << result.replans
                  << " failures=" << result.failures << " seconds=" << result.ticks * kDt
                  << " judge=" << result.verdict.reason << " reason=" << result.reason << '\n';
    }
    int obstructions_passed = 0;
    for (const bool after_plan : {false, true}) {
        GapObstructionResult result;
        try {
            result = obstructed_gap(after_plan);
        } catch (const std::exception &error) {
            result.reason = std::string("fixture/runtime exception: ") + error.what();
        }
        const bool success = result.passed(after_plan);
        obstructions_passed += success ? 1 : 0;
        std::cout << (success ? "PASS" : "FAIL") << " scraperx_sim matron exam descent obstruction="
                  << (after_plan ? "inserted_after_plan" : "initial_scan")
                  << " seed=1 beam_width=3.4000 beam_section=0.0400 beam_top_above_start=0.1800"
                  << " lower_capsule_hit=" << result.fixture_intersects << " planned=" << result.planned
                  << " rejected=" << result.rejected << " blocked_edge=" << result.blocked_edge
                  << " supported=" << result.supported << " never_launched=" << result.never_launched
                  << " impulse_free=" << result.impulse_free << " actual_n=" << result.peak_actual_n
                  << " declared_n=" << result.peak_declared_n << " positive_w=" << result.peak_power_w
                  << " worst_step=" << result.worst_step_m << " alive=" << result.alive
                  << " seconds=" << result.ticks * kDt << " reason=" << result.reason << '\n';
    }
    require(passed == kSeeds, "matron exam descent: every seed must jump down, recover from a lost rung, control the pole and stand at the bottom");
    require(obstructions_passed == 2, "matron exam descent: initial and newly obstructed gap corridors must be refused before launch on real support");
    std::cout << "PASS scraperx_sim matron exam descent: " << passed << '/' << kSeeds
              << " unseen courses, all required events in each faulted production journey\n";
}
