// Executors through the climbing lab's exam (tests/matron_exam.hpp): seeded
// structures nobody tuned for, each way up and down, judged by the exam's
// judge alone (not by the executor's own success flag). Routecraft
// (src/sim/matron_routecraft.*) climbs bars from a structure's foot, so its
// runs start there: the walk to the foot is the macro layer's. What it does
// not climb (a pole, a ledge stack) it must refuse without a fall. (The exam
// once had to stand on y 154: she died 12 m under her authored deck list;
// her falls are decided by the landing now, Matron::kLethalLandingMps.)

#include "sim_tests.hpp"

#include "matron_exam.hpp"
#include "sim/matron.hpp"
#include "sim/matron_traversal.hpp"
#include "sim/physics_world_support.hpp"

#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>

#include <iostream>
#include <memory>

namespace {
using namespace JPH;
using namespace scraperx::sim;
using namespace scraperx::sim::detail;
namespace exam = scraperx::sim::exam;

constexpr float kDt = exam::Judge::kTickSeconds;
constexpr float kBaseY = 154.0F;

struct World {
    JoltRuntimeLease lease;
    detail::BroadPhaseLayerInterface broad;
    ObjectVsBroadPhaseFilter vs;
    detail::ObjectLayerPairFilter pair;
    TempAllocatorImpl temp{32 * 1024 * 1024};
    JobSystemSingleThreaded jobs{1024};
    PhysicsSystem physics;
    std::unique_ptr<Matron> matron;
    exam::Structure s;

    explicit World(const std::uint64_t seed) {
        physics.Init(1024, 0, 4096, 2048, broad, vs, pair);
        physics.SetGravity(Vec3(0.0F, -9.81F, 0.0F));
        s = exam::build(physics, seed, object_layers::kStatic, kBaseY);
        matron = std::make_unique<Matron>(physics, object_layers::kStatic, object_layers::kMoving,
                                          object_layers::kOnSteel, BodyID());
        Matron::Saved saved;
        saved.state = Matron::State::Hunting;
        matron->restore(saved);
    }
    ~World() {
        matron.reset();
        for (const BodyID id : s.bodies) {
            physics.GetBodyInterface().RemoveBody(id);
            physics.GetBodyInterface().DestroyBody(id);
        }
    }
    // The exam's initial condition only, before the first tick.
    void place(const RVec3 feet) {
        physics.GetBodyInterface().SetPosition(matron->body(), feet + Vec3(0.0F, Matron::kStandCentre, 0.0F),
                                               EActivation::Activate);
    }
    void tick() {
        matron->pre_step(kDt, RVec3(300.0, 0.0, 300.0), 0.0F);
        physics.Update(kDt, 1, &temp, &jobs);
        matron->post_step(kDt);
    }
};

struct Run {
    bool planned = false;
    MatronRoutecraft::Status status = MatronRoutecraft::Status::Idle;
    exam::Verdict verdict;
    float seconds = 0.0F;
};

// From `from` (feet) to a standing goal `to` (feet), judged on `support` and `feet_box`.
Run climb(World &w, const RVec3 from, const RVec3 to, const BodyID support, const AABox &feet_box) {
    Run run;
    w.place(from);
    for (int i = 0; i < 45; ++i) {   // settle on the start's steel
        w.tick();
    }
    exam::Judge judge(support, feet_box, MatronRoutecraft::kForce, Matron::kStandCentre);
    if (std::getenv("SCRAPERX_EXAM_DEBUG") != nullptr) {
        const RVec3 at = w.physics.GetBodyInterface().GetPosition(w.matron->body());
        std::cout << "    debug: state " << int(w.matron->view().state) << " added "
                  << w.physics.GetBodyInterface().IsAdded(w.matron->body()) << " at " << at << '\n';
    }
    run.planned = w.matron->request_climb_goal(to + Vec3(0.0F, Matron::kStandCentre + 0.02F, 0.0F), Vec3::sAxisZ());
    run.status = w.matron->climb_report().status;
    bool released = false;
    int after = 0;   // ticks since her hands came off
    const int limit = run.planned ? 120 * 90 : 90;   // a refusal: a second to see she stays put
    for (int i = 0; i < limit && !judge.done() && after < 6 * 90; ++i) {
        w.tick();
        const MatronRoutecraft::Report &r = w.matron->climb_report();
        if (!released && r.status == MatronRoutecraft::Status::Holding) {
            w.matron->cancel_climb_goal();   // at the goal: her hands off, onto her feet
            released = true;
        } else if (!released && run.planned && r.status != MatronRoutecraft::Status::Moving) {
            judge.note_failure();   // the route ended under her without reaching the goal
            run.status = r.status;
            released = true;
        }
        judge.tick(w.physics, w.matron->body(), r.force, w.matron->climb_report().status ==
                                                             MatronRoutecraft::Status::Moving, kDt);
        run.seconds = float(i + 1) * kDt;
        after += released ? 1 : 0;
    }
    if (run.planned && run.status == MatronRoutecraft::Status::Moving) {
        run.status = w.matron->climb_report().status;
    }
    run.verdict = judge.verdict();
    return run;
}

const char *status_name(const MatronRoutecraft::Status s) {
    switch (s) {
    case MatronRoutecraft::Status::Idle: return "idle";
    case MatronRoutecraft::Status::Moving: return "moving";
    case MatronRoutecraft::Status::Holding: return "holding";
    case MatronRoutecraft::Status::NoRoute: return "no route";
    case MatronRoutecraft::Status::Released: return "released";
    case MatronRoutecraft::Status::BudgetExceeded: return "budget exceeded";
    }
    return "?";
}

void report(const char *way, const Run &r) {
    std::cout << " " << way << ": " << (r.planned ? "planned" : "refused") << " (" << status_name(r.status) << "), "
              << r.verdict.reason << ", worst step " << r.verdict.worst_step_m << " m, peak " << r.verdict.peak_commanded_n
              << " N, landing " << r.verdict.worst_landing_mps << " m/s";
}

// The laws hold whatever the outcome: no snap, no force over the cap, gravity
// kept, no lethal landing, and a refusal is never an arrival.
void require_laws(const Run &r) {
    const std::string &why = r.verdict.reason;
    require(why.find("snapped") != 0 && why != "over the force cap" && why != "gravity changed on her body" &&
                why != "not the native tick",
            "matron exam routecraft: the laws hold on every run");
    require(r.verdict.alive, "matron exam routecraft: no run kills her");
    require(r.planned || !r.verdict.arrived || r.verdict.physical_failures == 0,
            "matron exam routecraft: a refusal is not counted as a climb");
}

} // namespace

void run_matron_exam_routecraft() {
    constexpr std::uint64_t kSeeds = 100;   // seeds 1-100, up and down
    int tried[4] = {0, 0, 0, 0};
    int up_clean[4] = {0, 0, 0, 0};
    int down_clean[4] = {0, 0, 0, 0};
    int refused[4] = {0, 0, 0, 0};
    for (std::uint64_t seed = 1; seed <= kSeeds; ++seed) {
        Run up;
        Run down;
        exam::Params p;
        {
            World w(seed);
            p = w.s.params;
            const float h = p.height_m;
            up = climb(w, RVec3(0.0, kBaseY, -0.6), RVec3(0.0, kBaseY + h, p.standoff_m + 0.6F), w.s.goal, w.s.goal_feet);
        }
        {
            World w(seed);
            down = climb(w, RVec3(0.0, kBaseY + p.height_m, p.standoff_m + 0.6F), RVec3(0.0, kBaseY, -0.7), w.s.floor,
                         w.s.start_feet);
        }
        const int k = int(p.kind);
        ++tried[k];
        up_clean[k] += up.verdict.clean ? 1 : 0;
        down_clean[k] += down.verdict.clean ? 1 : 0;
        refused[k] += (!up.planned ? 1 : 0) + (!down.planned ? 1 : 0);
        std::cout << "  exam seed " << seed << " " << exam::kind_name(p.kind) << " " << p.height_m << " m pitch "
                  << p.pitch_m << " first " << p.first_hold_m << (p.missing >= 0 ? " missing a hold" : "") << ":";
        report("up", up);
        report(";  down", down);
        std::cout << '\n';
        require_laws(up);
        require_laws(down);
    }
    std::cout << "PASS scraperx_sim matron exam routecraft: laws held on " << 2 * kSeeds << " runs; clean up/down:";
    for (int k = 0; k < 4; ++k) {
        std::cout << " " << exam::kind_name(exam::Kind(k)) << " " << up_clean[k] << "/" << down_clean[k] << " of "
                  << tried[k] << " (refused " << refused[k] << ")";
    }
    std::cout << '\n';
}

// Grok's move library (src/sim/matron_traversal.*) through the same exam:
// its scanner reads the exam world, its planner chains moves region to
// region, and its Climber does each move (rungs, a pole's friction grip, a
// mantle, a jump-catch, a hang-drop) out of one capped budget. The journey
// driver here is a minimal one (the lab's macro layer is to replace it): it
// walks to each move's start, runs the move, and on a physical failure
// replans from where she is (twice at most). Her body is the bare capsule,
// starting where the exam says, 5 to 9 m off the foot.
namespace {
namespace traversal = scraperx::sim::traversal;

struct Bare {
    JoltRuntimeLease lease;
    detail::BroadPhaseLayerInterface broad;
    ObjectVsBroadPhaseFilter vs;
    detail::ObjectLayerPairFilter pair;
    TempAllocatorImpl temp{32 * 1024 * 1024};
    JobSystemSingleThreaded jobs{1024};
    PhysicsSystem physics;
    exam::Structure s;
    BodyID her;

    explicit Bare(const std::uint64_t seed) {
        physics.Init(1024, 0, 4096, 2048, broad, vs, pair);
        physics.SetGravity(Vec3(0.0F, -9.81F, 0.0F));
        s = exam::build(physics, seed, object_layers::kStatic, kBaseY);
    }
    ~Bare() {
        BodyInterface &b = physics.GetBodyInterface();
        if (!her.IsInvalid()) {
            b.RemoveBody(her);
            b.DestroyBody(her);
        }
        for (const BodyID id : s.bodies) {
            b.RemoveBody(id);
            b.DestroyBody(id);
        }
    }
    void spawn(const RVec3 feet) {
        BodyCreationSettings b(new CapsuleShape(Matron::kHalfHeight, Matron::kRadius),
                               feet + Vec3(0.0F, Matron::kStandCentre, 0.0F), Quat::sIdentity(), EMotionType::Dynamic,
                               object_layers::kMoving);
        b.mAllowedDOFs = EAllowedDOFs::TranslationX | EAllowedDOFs::TranslationY | EAllowedDOFs::TranslationZ;
        b.mAllowSleeping = false;
        b.mFriction = 0.0F;
        b.mLinearDamping = 0.0F;
        b.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
        b.mMassPropertiesOverride.mMass = Matron::kMassKg;
        her = physics.GetBodyInterface().CreateAndAddBody(b, EActivation::Activate);
    }
    Vec3 feet() const {
        return Vec3(physics.GetBodyInterfaceNoLock().GetPosition(her)) - Vec3(0.0F, Matron::kStandCentre, 0.0F);
    }
};

struct Journey {
    bool planned = false;
    int moves = 0;
    int failures = 0;
    std::string last_failure = "none";
    exam::Verdict verdict;
};

Journey journey(Bare &w, const RVec3 from, const RVec3 to, const BodyID support, const AABox &feet_box) {
    Journey out;
    w.spawn(from);
    for (int i = 0; i < 45; ++i) {
        (void)w.physics.Update(kDt, 1, &w.temp, &w.jobs);
    }
    const traversal::Ignore ignore{w.her, BodyID(), -1};
    traversal::AffordanceMap map;
    map.scan(w.physics, AABox(Vec3(-14.0F, kBaseY - 2.0F, -14.0F), Vec3(14.0F, kBaseY + 16.0F, 8.0F)), ignore);
    traversal::Climber climber(w.physics, w.her, ignore);
    exam::Judge judge(support, feet_box, climber.limits().total_force_n, Matron::kStandCentre);
    const auto tick = [&](const float force, const bool holding) {
        (void)w.physics.Update(kDt, 1, &w.temp, &w.jobs);
        judge.tick(w.physics, w.her, force, holding, kDt);
    };
    // Her feet over floor to `at`: grid_walk's way round what is in the
    // way, else straight.
    const auto walk_to = [&](const Vec3 at) {
        std::vector<Vec3> legs = traversal::grid_walk(w.physics, ignore, climber.limits(), w.feet(), at);
        if (std::getenv("SCRAPERX_EXAM_DEBUG") != nullptr) {
            std::cout << "    walk from " << w.feet() << " to " << at << " legs " << legs.size();
            for (const Vec3 &l : legs) std::cout << " " << l;
            std::cout << '\n';
        }
        legs.push_back(at);
        for (const Vec3 &leg : legs) {
            for (int i = 0; i < 20 * 90 && !judge.done(); ++i) {
                const Vec3 off = leg - w.feet();
                if (Vec3(off.GetX(), 0.0F, off.GetZ()).Length() < 0.12F) {
                    break;
                }
                tick(climber.walk_between(kDt, leg, 1.2F), false);
            }
        }
    };
    // Standing: her feet hold her where she stopped (her body has no
    // friction of its own; its feet are the Climber's, out of the budget).
    const auto stand = [&](const float seconds) {
        const Vec3 here = w.feet();
        for (int i = 0; i < int(seconds * 90.0F) && !judge.done(); ++i) {
            tick(climber.walk_between(kDt, here, 0.0F), false);
        }
    };
    const int goal = map.region_at(Vec3(to));
    for (int attempt = 0; attempt < 3 && !judge.done(); ++attempt) {
        const int here = map.region_at(w.feet());
        const std::vector<int> plan = here >= 0 && goal >= 0 ? map.plan(here, goal) : std::vector<int>{};
        if (plan.empty()) {
            break;   // refused: no way she knows of, from here
        }
        out.planned = true;
        bool failed = false;
        for (const int e : plan) {
            const traversal::Edge &edge = map.edges()[static_cast<std::size_t>(e)];
            // To the move's start over her floor, round what is in the way.
            walk_to(edge.spec.start);
            if (edge.kind == traversal::MoveKind::Walk) {
                continue;
            }
            ++out.moves;
            climber.begin(edge.spec);
            for (int i = 0; i < 120 * 90 && !judge.done(); ++i) {
                const traversal::Climber::Status status = climber.step(kDt);
                tick(climber.view().force_n, climber.view().holding);
                if (status == traversal::Climber::Status::Done) {
                    if (std::getenv("SCRAPERX_EXAM_DEBUG") != nullptr) {
                        const Vec3 f = w.feet();
                        std::cout << "    done " << traversal::move_name(edge.kind) << " feet " << f << " spec goal "
                                  << edge.spec.goal << " holding " << climber.view().holding << " on steel "
                                  << climber.on_steel() << '\n';
                    }
                    break;
                }
                if (status == traversal::Climber::Status::Failed) {
                    failed = true;
                    break;
                }
            }
            if (failed) {
                ++out.failures;
                out.last_failure = traversal::failure_name(climber.failure());
                judge.note_failure();
                climber.cancel();
                break;
            }
        }
        if (!failed) {
            walk_to(Vec3(to));   // off the move's end, onto where she was going
            break;
        }
        stand(3.0F);   // on her feet again before she replans
    }
    stand(6.0F);   // standing where she ended
    out.verdict = judge.verdict();
    return out;
}

void report_journey(const char *way, const Journey &j) {
    std::cout << " " << way << ": " << (j.planned ? "planned" : "refused") << ", moves " << j.moves << ", failures "
              << j.failures << " (" << j.last_failure << "), " << j.verdict.reason << ", worst step "
              << j.verdict.worst_step_m << " m, peak " << j.verdict.peak_commanded_n << " N, landing "
              << j.verdict.worst_landing_mps << " m/s";
}

void require_journey_laws(const Journey &j) {
    const std::string &why = j.verdict.reason;
    require(why.find("snapped") != 0 && why != "over the force cap" && why != "gravity changed on her body" &&
                why != "not the native tick",
            "matron exam climber: the laws hold on every run");
    require(j.verdict.alive, "matron exam climber: no run kills her");
}

} // namespace

void run_matron_exam_climber() {
    constexpr std::uint64_t kSeeds = 100;   // seeds 1-100, up and down
    int tried[4] = {0, 0, 0, 0};
    int up_clean[4] = {0, 0, 0, 0};
    int down_clean[4] = {0, 0, 0, 0};
    int up_any[4] = {0, 0, 0, 0};
    int down_any[4] = {0, 0, 0, 0};
    for (std::uint64_t seed = 1; seed <= kSeeds; ++seed) {
        Journey up;
        Journey down;
        exam::Params p;
        {
            Bare w(seed);
            p = w.s.params;
            up = journey(w, w.s.start_at, RVec3(0.0, kBaseY + p.height_m, p.standoff_m + 1.0F), w.s.goal, w.s.goal_feet);
        }
        {
            Bare w(seed);
            down = journey(w, RVec3(0.0, kBaseY + p.height_m, p.standoff_m + 1.0F), w.s.start_at, w.s.floor,
                           w.s.start_feet);
        }
        const int k = int(p.kind);
        ++tried[k];
        up_clean[k] += up.verdict.clean ? 1 : 0;
        down_clean[k] += down.verdict.clean ? 1 : 0;
        up_any[k] += up.verdict.arrived ? 1 : 0;
        down_any[k] += down.verdict.arrived ? 1 : 0;
        std::cout << "  exam seed " << seed << " " << exam::kind_name(p.kind) << " " << p.height_m << " m"
                  << (p.wall ? " walled" : "") << ":";
        report_journey("up", up);
        report_journey(";  down", down);
        std::cout << '\n';
        require_journey_laws(up);
        require_journey_laws(down);
        require(up.verdict.clean && down.verdict.clean && up.verdict.worst_step_m <= 0.20F && down.verdict.worst_step_m <= 0.20F,
                "matron exam climber: every seed climbed clean, up and down, no tick over 0.20 m");
    }
    std::cout << "PASS scraperx_sim matron exam climber: laws held on " << 2 * kSeeds
              << " runs; clean (any) up/down:";
    for (int k = 0; k < 4; ++k) {
        std::cout << " " << exam::kind_name(exam::Kind(k)) << " " << up_clean[k] << "(" << up_any[k] << ")/"
                  << down_clean[k] << "(" << down_any[k] << ") of " << tried[k];
    }
    std::cout << '\n';
}
