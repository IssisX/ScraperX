// Observational descent exam on seeded, unseen multi-stage steel. The Matron
// sees only collision geometry through her real scanner and planner. The
// harness supplies a bottom goal and a removable real rung; no route is fed
// to the executor and no body is moved after its initial spawn.

#include "sim_tests.hpp"

#include "matron_exam.hpp"
#include "sim/matron.hpp"
#include "sim/matron_traversal.hpp"
#include "sim/physics_world_support.hpp"

#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
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
constexpr int kMaxTicks = 120 * 90;
const Vec3 kBottom(0.0F, 0.0F, 1.2F);

// Seeded measurements, independent of any runtime scanner/executor choices.
// The discontinuity above B is a jump-to-bar on ascent. A second, lateral
// void between E and its narrow approach must be negotiated on descent.
struct Course {
    float a, b, c, e, vertical_gap, horizontal_gap, board_width, pole_x;
    float e_front;
};

Course course_for(const std::uint32_t seed) {
    std::uint32_t rng = seed * 2654435761U + 0x9E3779B9U;
    const auto uniform = [&](const float lo, const float hi) {
        rng = rng * 1664525U + 1013904223U;
        return lo + (hi - lo) * static_cast<float>(rng >> 8U) / static_cast<float>(1U << 24U);
    };
    Course c{};
    c.a = uniform(2.5F, 3.5F);
    c.b = c.a + uniform(2.7F, 3.5F);
    c.vertical_gap = uniform(2.6F, 3.3F);
    c.c = c.b + c.vertical_gap + uniform(1.7F, 2.3F);
    c.e = c.c + uniform(1.0F, 1.5F);
    // Every void is wider than her 0.60 m diameter: it cannot be straddled.
    c.horizontal_gap = uniform(0.62F, 0.69F);
    c.board_width = uniform(0.25F, 0.40F);
    c.pole_x = uniform(-0.9F, 0.9F);
    c.e_front = -13.0F - c.horizontal_gap;
    return c;
}

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
    std::vector<BodyID> panel_rungs;
    BodyID ground;
    BodyID bridge;
    BodyID catch_deck;
    BodyID her;
    RVec3 top_feet = RVec3::sZero();

    explicit World(const std::uint32_t seed) : course(course_for(seed)) {
        physics.Init(4096, 0, 4096, 2048, broad, vs, pair);
        physics.SetGravity(Vec3(0.0F, -9.81F, 0.0F));
        ground = box(Vec3(20.0F, 0.2F, 20.0F), RVec3(0, -0.2F, 0));
        const Course &c = course;
        // Three separated decks: real rungs from the ground to A, a pole
        // from A to B, and a high, gapped panel from B to C.
        box(Vec3(1.5F, 0.5F * c.a, 1.5F), RVec3(0, 0.5F * c.a, -2.5F));
        for (float y = 0.3F; y <= c.a - 0.1F; y += 0.3F)
            box(Vec3(0.25F, 0.02F, 0.02F), RVec3(0, y, -0.95F));
        box(Vec3(1.5F, 0.5F * c.b, 1.5F), RVec3(0, 0.5F * c.b, -5.5F));
        box(Vec3(0.05F, 0.5F * (c.b - 0.05F - c.a), 0.05F),
            RVec3(c.pole_x, c.a + 0.5F * (c.b - 0.05F - c.a), -3.55F));
        box(Vec3(1.5F, 0.5F * c.c, 1.5F), RVec3(0, 0.5F * c.c, -8.5F));
        for (float y = c.b + c.vertical_gap; y <= c.c - 0.1F; y += 0.3F)
            panel_rungs.push_back(box(Vec3(0.25F, 0.02F, 0.02F), RVec3(0, y, -6.95F)));
        // Narrow bridge, a real catch deck one meter underneath it, and a
        // final ledge with a randomized void in front of its lip. All are
        // support bodies. The void has no invisible floor or authored edge.
        bridge = box(Vec3(0.5F * c.board_width, 0.1F, 1.5F), RVec3(0, c.c - 0.1F, -11.5F));
        catch_deck = box(Vec3(1.1F, 0.5F * (c.c - 1.0F), 1.5F), RVec3(2.0F, 0.5F * (c.c - 1.0F), -11.5F));
        box(Vec3(2.5F, 0.5F * c.e, 1.5F), RVec3(1.0F, 0.5F * c.e, c.e_front - 1.5F));
        for (float y = c.c - 0.7F; y <= c.e - 0.1F; y += 0.3F)
            box(Vec3(0.25F, 0.02F, 0.02F), RVec3(2.0F, y, c.e_front + 0.05F));
        top_feet = RVec3(0, c.e, c.e_front - 1.5F);
    }

    ~World() {
        BodyInterface &b = physics.GetBodyInterface();
        for (const BodyID id : bodies) {
            if (b.IsAdded(id)) b.RemoveBody(id);
            b.DestroyBody(id);
        }
    }

    BodyID box(const Vec3 half, const RVec3 at) {
        const float convex = std::min(0.05F, 0.5F * half.ReduceMin());
        BodyCreationSettings settings(new BoxShape(half, convex), at, Quat::sIdentity(),
                                      EMotionType::Static, object_layers::kStatic);
        settings.mFriction = 0.8F;
        const BodyID id = physics.GetBodyInterface().CreateAndAddBody(settings, EActivation::DontActivate);
        if (id.IsInvalid()) throw std::runtime_error("descent exam: static body creation failed");
        bodies.push_back(id);
        return id;
    }

    void spawn() {
        BodyCreationSettings settings(new CapsuleShape(Matron::kHalfHeight, Matron::kRadius),
                                      top_feet + Vec3(0.0F, Matron::kStandCentre, 0.0F),
                                      Quat::sIdentity(), EMotionType::Dynamic, object_layers::kMoving);
        settings.mAllowedDOFs = EAllowedDOFs::TranslationX | EAllowedDOFs::TranslationY | EAllowedDOFs::TranslationZ;
        settings.mAllowSleeping = false;
        settings.mFriction = 0.0F;
        settings.mLinearDamping = 0.0F;
        settings.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = Matron::kMassKg;
        her = physics.GetBodyInterface().CreateAndAddBody(settings, EActivation::Activate);
        if (her.IsInvalid()) throw std::runtime_error("descent exam: Matron body creation failed");
        bodies.push_back(her);
    }

    Vec3 feet() const {
        return Vec3(physics.GetBodyInterfaceNoLock().GetPosition(her)) - Vec3(0, Matron::kStandCentre, 0);
    }

    bool remove_held_panel_rung(const RVec3 hand) {
        float best = 0.28F;
        BodyID found;
        for (const BodyID id : panel_rungs) {
            if (!physics.GetBodyInterface().IsAdded(id)) continue;
            const float distance = float((hand - physics.GetBodyInterfaceNoLock().GetPosition(id)).Length());
            if (distance < best) { best = distance; found = id; }
        }
        if (found.IsInvalid()) return false;
        // The real member leaves the world. The climber's existing hold
        // validation sees the missing body on its next step.
        physics.GetBodyInterface().RemoveBody(found);
        return true;
    }
};

struct Run {
    exam::Verdict verdict;
    std::string reason = "not started";
    std::string last_climber_failure = "none";
    bool plan_found = false;
    bool rung_down = false;
    bool pole_down = false;
    bool ledge_down = false;
    bool pole_descending = false;
    bool jump_edge = false;
    bool jump_apex = false;
    bool jump_landed = false;
    bool gap_crossed = false;
    bool gap_supported = false;
    bool gap_upward = false;
    bool gap_reversed = false;
    bool removed_held_rung = false;
    bool observed_lost_hold = false;
    bool landed_after_loss = false;
    bool replanned_from_landing = false;
    int replans = 0;
    int moves = 0;
    int ticks = 0;
    float max_pole_speed_mps = 0.0F;
    float peak_power_w = 0.0F;
    std::string planned_moves;
};

float horizontal_distance(const Vec3 a, const Vec3 b) {
    const Vec3 d = a - b;
    return std::sqrt(d.GetX() * d.GetX() + d.GetZ() * d.GetZ());
}

Run descend(World &w, const bool remove_grip) {
    Run result;
    w.spawn();
    const traversal::Ignore ignore{w.her, BodyID(), -1};
    traversal::Climber climber(w.physics, w.her, ignore);
    exam::Judge judge(w.ground, AABox(Vec3(-3.0F, -0.06F, 0.40F), Vec3(3.0F, 0.06F, 2.6F)),
                      climber.limits().total_force_n, Matron::kStandCentre);
    const auto tick = [&](const float force, const bool held) {
        (void)w.physics.Update(kDt, 1, &w.temp, &w.jobs);
        judge.tick(w.physics, w.her, force, held, kDt);
        ++result.ticks;
        result.peak_power_w = std::max(result.peak_power_w, climber.view().power_w);
    };
    // Even the settling ticks are judged. Spawn is the sole initial pose.
    for (int t = 0; t < 45 && !judge.done(); ++t) tick(climber.stand(kDt), false);

    traversal::AffordanceMap map;
    const AABox bounds(Vec3(-6.0F, -2.0F, -23.0F), Vec3(7.0F, w.course.e + 5.0F, 5.0F));
    std::vector<int> route;
    std::vector<Vec3> walk;
    std::size_t next = 0, waypoint = 0;
    int goal_region = -1;
    enum class Mode { Walk, Move, Landing, Recover, Final, Refused };
    Mode mode = Mode::Refused;
    int current_edge = -1;
    int mode_ticks = 0;
    bool jump_up = false;
    bool jump_down = false;
    bool pending_jump_landing = false;
    bool awaiting_lost_hold_landing = false;

    // Walking never invents a route through a missing grid path. All targets
    // come from the scanned plan, except the real bottom support goal.
    const auto start_walk = [&](const Vec3 destination, const Mode next_mode) {
        const Vec3 here = w.feet();
        walk = traversal::grid_walk(w.physics, ignore, climber.limits(), here, destination);
        if (walk.empty() && horizontal_distance(here, destination) > 0.45F) {
            result.reason = "planner: no supported foot path";
            mode = Mode::Refused;
            return false;
        }
        walk.push_back(destination);
        waypoint = 0;
        mode_ticks = 0;
        mode = next_mode;
        return true;
    };
    const auto next_target = [&]() {
        if (next >= route.size()) return start_walk(kBottom, Mode::Final);
        current_edge = route[next];
        return start_walk(map.edges()[static_cast<std::size_t>(current_edge)].spec.start, Mode::Walk);
    };
    const auto plan_from_support = [&]() {
        map.scan(w.physics, bounds, ignore, climber.limits());
        const int from = map.region_at(w.feet());
        goal_region = map.region_at(kBottom);
        if (from < 0 || goal_region < 0) {
            result.reason = from < 0 ? "planner: no region under her real feet" : "planner: no bottom region";
            mode = Mode::Refused;
            return false;
        }
        route = map.plan(from, goal_region);
        if (route.empty() && from != goal_region) {
            result.reason = "planner: no descent route";
            mode = Mode::Refused;
            return false;
        }
        result.plan_found = true;
        if (result.planned_moves.empty()) {
            for (const int index : route) {
                if (!result.planned_moves.empty()) result.planned_moves += ',';
                result.planned_moves += traversal::move_name(map.edges()[static_cast<std::size_t>(index)].kind);
            }
        }
        next = 0;
        if (awaiting_lost_hold_landing) {
            result.replanned_from_landing = true;
            awaiting_lost_hold_landing = false;
        }
        return next_target();
    };

    if (!judge.done()) (void)plan_from_support();
    while (result.ticks < kMaxTicks && !judge.done() && mode != Mode::Refused) {
        ++mode_ticks;
        const Vec3 at = w.feet();
        const Vec3 velocity = w.physics.GetBodyInterfaceNoLock().GetLinearVelocity(w.her);
        float force = 0.0F;
        bool held = false;
        traversal::Climber::Status status = traversal::Climber::Status::Idle;
        const traversal::MoveKind kind = mode == Mode::Move
                                              ? map.edges()[static_cast<std::size_t>(current_edge)].kind
                                              : traversal::MoveKind::Walk;
        if (mode == Mode::Walk || mode == Mode::Final) {
            while (waypoint + 1 < walk.size() && horizontal_distance(at, walk[waypoint]) < 0.30F) ++waypoint;
            if (horizontal_distance(at, walk.back()) < 0.45F) {
                if (mode == Mode::Walk) {
                    climber.begin(map.edges()[static_cast<std::size_t>(current_edge)].spec);
                    mode = Mode::Move;
                    mode_ticks = 0;
                    ++result.moves;
                }
                force = climber.stand(kDt);
            } else {
                force = climber.walk_between(kDt, walk[waypoint], 1.2F);
            }
            if (mode_ticks > 25 * 90) {
                result.reason = "planner: walk stalled";
                mode = Mode::Refused;
            }
        } else if (mode == Mode::Move) {
            // The fault is physical and happens only when a hand actually
            // bears weight on a middle rung. Never set her pose or speed.
            if (remove_grip && !result.removed_held_rung && kind == traversal::MoveKind::RungDown &&
                climber.view().holding && at.GetY() > w.course.b + 1.0F &&
                at.GetY() < w.course.c - 0.7F) {
                result.removed_held_rung = w.remove_held_panel_rung(climber.view().hand);
            }
            status = climber.step(kDt);
            force = climber.view().force_n;
            held = climber.view().holding;
            if (kind == traversal::MoveKind::PoleDown && held) {
                result.pole_descending = result.pole_descending || velocity.GetY() < -0.1F;
                result.max_pole_speed_mps = std::max(result.max_pole_speed_mps, std::max(0.0F, -velocity.GetY()));
            }
            if (kind == traversal::MoveKind::JumpCatch) {
                result.jump_edge = true;
                jump_up = jump_up || velocity.GetY() > 0.3F;
                jump_down = jump_down || (jump_up && velocity.GetY() < -0.3F);
            }
            if (mode_ticks > 120 * 90 && status == traversal::Climber::Status::Running) {
                result.reason = "planner: move timed out";
                mode = Mode::Refused;
            }
        } else {
            // Landing/recovery are genuine free physics, with only her normal
            // ground-balanced stance when there is a supporting surface.
            force = climber.stand(kDt);
            if (mode_ticks > 10 * 90) {
                result.reason = "planner: no real landing to replan from";
                mode = Mode::Refused;
            }
        }
        tick(force, held || climber.on_steel());
        if (judge.done()) break;

        // Measure the gap from real displacement and support contacts, not
        // from the label of a planner edge. A controlled drop can cross it,
        // but only a rise followed by a fall counts as a downward jump.
        const Vec3 after = w.feet();
        const Vec3 after_velocity = w.physics.GetBodyInterfaceNoLock().GetLinearVelocity(w.her);
        if (!result.gap_supported && after.GetY() > w.course.c - 0.5F) {
            result.gap_upward = result.gap_upward || after_velocity.GetY() > 0.3F;
            result.gap_reversed = result.gap_reversed || (result.gap_upward && after_velocity.GetY() < -0.3F);
        }
        result.gap_crossed = result.gap_crossed || after.GetZ() > -13.0F + 0.02F;
        if (result.gap_crossed && !result.gap_supported) {
            float contact_height = 0.0F;
            BodyID support;
            if (climber.supported(&contact_height, &support) &&
                (support == w.bridge || support == w.catch_deck)) {
                result.gap_supported = true;
            }
        }

        if (mode == Mode::Move && status == traversal::Climber::Status::Done) {
            result.rung_down = result.rung_down || kind == traversal::MoveKind::RungDown;
            result.pole_down = result.pole_down || kind == traversal::MoveKind::PoleDown;
            result.ledge_down = result.ledge_down || kind == traversal::MoveKind::HangDrop ||
                                kind == traversal::MoveKind::StepDown;
            if (kind == traversal::MoveKind::JumpCatch) {
                result.jump_apex = jump_up && jump_down;
                pending_jump_landing = true;
            }
            ++next;
            mode = Mode::Landing;
            mode_ticks = 0;
        } else if (mode == Mode::Move && status == traversal::Climber::Status::Failed) {
            result.last_climber_failure = traversal::failure_name(climber.failure());
            result.reason = "climber: " + result.last_climber_failure;
            if (result.removed_held_rung && climber.failure() == traversal::Failure::LostHold) {
                result.observed_lost_hold = true;
                awaiting_lost_hold_landing = true;
            }
            judge.note_failure();
            climber.cancel();
            mode = Mode::Recover;
            mode_ticks = 0;
        } else if (mode == Mode::Landing || mode == Mode::Recover) {
            float support_top = 0.0F;
            BodyID on;
            const bool supported = climber.supported(&support_top, &on);
            const float speed = w.physics.GetBodyInterfaceNoLock().GetLinearVelocity(w.her).Length();
            if (supported && !on.IsInvalid() && speed < 0.5F) {
                if (pending_jump_landing) {
                    result.jump_landed = result.jump_apex &&
                                         map.region_at(w.feet()) == map.edges()[static_cast<std::size_t>(current_edge)].to;
                    pending_jump_landing = false;
                }
                if (mode == Mode::Recover) {
                    if (awaiting_lost_hold_landing) result.landed_after_loss = true;
                    if (++result.replans > 8) {
                        result.reason = "planner: replan limit";
                        mode = Mode::Refused;
                    } else {
                        (void)plan_from_support();
                    }
                } else {
                    const int landed_region = map.region_at(w.feet());
                    const int expected = map.edges()[static_cast<std::size_t>(current_edge)].to;
                    if (landed_region == expected) {
                        (void)next_target();
                    } else if (++result.replans > 8) {
                        result.reason = "planner: landing off route";
                        mode = Mode::Refused;
                    } else {
                        (void)plan_from_support();
                    }
                }
            }
        }
    }
    result.verdict = judge.verdict();
    if (judge.done()) {
        result.reason = result.verdict.reason;
    } else if (result.ticks >= kMaxTicks && mode != Mode::Refused) {
        result.reason = "planner: descent time limit";
    }
    return result;
}

enum class Kind : std::size_t { Ladder, Pole, Ledge, JumpGap, Recovery, Count };
constexpr std::array<const char *, static_cast<std::size_t>(Kind::Count)> kKindNames{
    "ladder", "pole", "ledge", "jump gap", "lost-hold recovery"};

std::string fail_reason(const Run &run, const bool met) {
    if (met) return "none";
    if (!run.verdict.alive || (run.verdict.reason != "not yet" && run.verdict.reason != "arrived" &&
                                    run.verdict.reason != "arrived after a physical failure (a recovery)"))
        return "judge: " + run.verdict.reason;
    if (!run.verdict.arrived) return run.reason;
    if (!run.last_climber_failure.empty() && run.last_climber_failure != "none")
        return "climber: " + run.last_climber_failure;
    return "planner: required move not executed";
}

const char *yesno(const bool b) { return b ? "yes" : "no"; }

} // namespace

void run_matron_exam_descent() {
    std::array<int, static_cast<std::size_t>(Kind::Count)> successes{};
    std::array<std::map<std::string, int>, static_cast<std::size_t>(Kind::Count)> failures{};
    int full_clean = 0, arrived = 0, recovered = 0;
    std::cout << "DESCENT_EXAM seeds=" << kSeeds << " native_hz=90 judge=lab limits=real force,gravity,step,alive,1s-supported"
              << " selected_only=yes\n";
    for (int seed = 1; seed <= kSeeds; ++seed) {
        const Course c = course_for(static_cast<std::uint32_t>(seed));
        Run clean, fault;
        {
            World w(static_cast<std::uint32_t>(seed));
            clean = descend(w, false);
        }
        {
            World w(static_cast<std::uint32_t>(seed));
            fault = descend(w, true);
        }
        const bool pole_controlled = clean.pole_down && clean.pole_descending &&
                                     clean.max_pole_speed_mps <= traversal::Limits{}.pole_down_mps + 0.8F;
        const bool jump = clean.gap_crossed && clean.gap_supported && clean.gap_reversed;
        const bool recovery = fault.verdict.arrived && fault.removed_held_rung && fault.observed_lost_hold &&
                              fault.landed_after_loss && fault.replanned_from_landing;
        const std::array<bool, static_cast<std::size_t>(Kind::Count)> met{
            clean.verdict.clean && clean.rung_down,
            clean.verdict.clean && pole_controlled,
            clean.verdict.clean && clean.ledge_down,
            clean.verdict.clean && jump,
            recovery};
        for (std::size_t k = 0; k < met.size(); ++k) {
            std::string reason = fail_reason(k == static_cast<std::size_t>(Kind::Recovery) ? fault : clean, met[k]);
            if (met[k]) ++successes[k];
            else {
                if (reason == "planner: required move not executed") {
                    if (k == static_cast<std::size_t>(Kind::Pole) && clean.pole_down && !pole_controlled)
                        reason = "climber: uncontrolled pole slide";
                    if (k == static_cast<std::size_t>(Kind::JumpGap) && !clean.jump_edge)
                        reason = "planner: no downward jump edge";
                    if (k == static_cast<std::size_t>(Kind::Recovery) && !fault.removed_held_rung)
                        reason = "climber: held middle rung never reached";
                }
                ++failures[k][reason];
            }
            std::cout << "DESCENT_KIND seed=" << seed << " kind=" << kKindNames[k]
                      << " clean_or_recovered=" << yesno(met[k]) << " reason=" << reason << '\n';
        }
        arrived += int(clean.verdict.arrived);
        recovered += int(recovery);
        full_clean += int(clean.verdict.clean && clean.rung_down && pole_controlled && clean.ledge_down && jump);
        std::cout << "DESCENT_SEED seed=" << seed << " heights=" << std::fixed << std::setprecision(2)
                  << c.a << '/' << c.b << '/' << c.c << '/' << c.e << " gap_xz=" << c.horizontal_gap
                  << " gap_rungs=" << c.vertical_gap << " bridge=" << c.board_width
                  << " plan=" << (clean.planned_moves.empty() ? "none" : clean.planned_moves)
                  << " judge=" << clean.verdict.reason << " result=" << clean.reason
                  << " climber_failure=" << clean.last_climber_failure << " replans=" << clean.replans
                  << " rung=" << yesno(clean.rung_down) << " pole=" << yesno(pole_controlled)
                  << " pole_max_down=" << clean.max_pole_speed_mps << " jump=" << yesno(jump)
                  << " jump_edge=" << yesno(clean.jump_edge) << " gap_crossed=" << yesno(clean.gap_crossed)
                  << " gap_landed=" << yesno(clean.gap_supported) << " momentum_reversed=" << yesno(clean.gap_reversed)
                  << " ledge=" << yesno(clean.ledge_down) << " worst_step=" << clean.verdict.worst_step_m
                  << " peak_force=" << clean.verdict.peak_commanded_n << " power=" << clean.peak_power_w
                  << " fault=" << fault.last_climber_failure << " removed=" << yesno(fault.removed_held_rung)
                  << " landed=" << yesno(fault.landed_after_loss)
                  << " replanned=" << yesno(fault.replanned_from_landing)
                  << " recovery=" << yesno(recovery) << " recovery_judge=" << fault.verdict.reason
                  << " recovery_result=" << fault.reason << " alive=" << yesno(clean.verdict.alive)
                  << " worst_landing=" << clean.verdict.worst_landing_mps << '\n';
    }
    for (std::size_t k = 0; k < successes.size(); ++k) {
        std::vector<std::pair<std::string, int>> ranked(failures[k].begin(), failures[k].end());
        std::sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) {
            return a.second != b.second ? a.second > b.second : a.first < b.first;
        });
        std::cout << "DESCENT_CENSUS kind=" << kKindNames[k] << " clean_or_recovered=" << successes[k]
                  << '/' << kSeeds << " top_failure=" << (ranked.empty() ? "none" : ranked[0].first)
                  << " count=" << (ranked.empty() ? 0 : ranked[0].second)
                  << " second_failure=" << (ranked.size() < 2 ? "none" : ranked[1].first)
                  << " second_count=" << (ranked.size() < 2 ? 0 : ranked[1].second) << '\n';
    }
    std::cout << "PASS scraperx_sim matron exam descent: census completed on " << kSeeds
              << " unseen multistage seeds; judged arrivals=" << arrived
              << " full clean with required moves=" << full_clean
              << " recovered after real hold loss=" << recovered << " (observational group, no score gate)\n";
}
